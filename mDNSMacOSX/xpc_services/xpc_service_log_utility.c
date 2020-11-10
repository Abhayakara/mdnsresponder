/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <xpc/xpc.h>
#include <dirent.h>						// opendir
#include <sys/stat.h>					// stat
#include <archive.h>
#include <archive_entry.h>
#include <AssertMacros.h>				// require, require_action

#include "mDNSMacOSX.h"
#include "helper.h"
#include "xpc_services.h"
#include "xpc_service_log_utility.h"
#include "xpc_clients.h"
#include "system_utilities.h"			// IsAppleInternalBuild
#include "mdns_strict.h"

#define STATE_DUMP_PLAIN_SUFFIX "txt"
#define STATE_DUMP_COMPRESSED_SUFFIX "tar.bz2"

// global variables
extern mDNS				mDNSStorage;
static dispatch_queue_t log_utility_server_queue = NULL;

// function declaration
extern void
dump_state_to_fd(int fd);

mDNSlocal void
accept_client(xpc_connection_t conn);

mDNSlocal mDNSs8
handle_requests(xpc_object_t req);

mDNSlocal mDNSs8
check_permission(xpc_connection_t connection);

mDNSlocal mDNSs8
handle_state_dump(mDNSu32 dump_option, char *full_file_name, mDNSu32 name_buffer_size, int client_fd,
	mDNSs32 *time_ms_used);

mDNSlocal mDNSs32
find_oldest_state_dump(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 buffer_size,
	char *oldest_file_name, mDNSu32 oldest_file_name_buffer_size);

mDNSlocal mDNSs8
remove_state_dump_if_too_many(const char *dump_dir, const char *oldest_file_name, mDNSs32 dump_file_count,
	mDNSs32 max_allowed);

mDNSlocal int
create_new_state_dump_file(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 buffer_size);

mDNSlocal mDNSs8
handle_state_dump_to_fd(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 buffer_size,
	mDNSBool if_compress);

mDNSlocal mDNSs8
compress_state_dump_and_delete(char *input_file, mDNSu32 buffer_size);

mDNSlocal mDNSs32
timediff_ms(struct timeval *t1, struct timeval *t2);

// function definition
mDNSexport void
init_log_utility_service(void)
{
	xpc_connection_t log_utility_listener = xpc_connection_create_mach_service(kDNSLogUtilityService, NULL, XPC_CONNECTION_MACH_SERVICE_LISTENER);
	if (!log_utility_listener || xpc_get_type(log_utility_listener) != XPC_TYPE_CONNECTION) {
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "Error Creating XPC Listener for Log Utility Server!");
		return;
	}

	log_utility_server_queue = dispatch_queue_create("com.apple.mDNSResponder.log_utility_server_queue", NULL);

	xpc_connection_set_event_handler(log_utility_listener, ^(xpc_object_t eventmsg) {
		xpc_type_t type = xpc_get_type(eventmsg);

		if (type == XPC_TYPE_CONNECTION) {
			LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_INFO, "C%p {action='receives connection'}", eventmsg);
			accept_client(eventmsg);
		} else if (type == XPC_TYPE_ERROR) {
			LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "C%p {xpc_error=\n" PUB_S "\n}", eventmsg,
				xpc_dictionary_get_string(eventmsg, XPC_ERROR_KEY_DESCRIPTION));
		} else {
			LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "C%p {error='receives unknown xpc request'}", eventmsg);
		}
	});

	xpc_connection_resume(log_utility_listener);
}

mDNSlocal void
accept_client(xpc_connection_t conn)
{
	xpc_retain(conn);
	xpc_connection_set_target_queue(conn, log_utility_server_queue);
	xpc_connection_set_event_handler(conn, ^(xpc_object_t req_msg) {
		xpc_type_t type = xpc_get_type(req_msg);

		if (type == XPC_TYPE_DICTIONARY) {
			handle_requests(req_msg);
		} else { // We hit this case ONLY if Client Terminated Connection OR Crashed
			LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEFAULT, "C%p {status='client closed the connection'}", conn);
			xpc_connection_t tmp = conn;
			MDNS_DISPOSE_XPC(tmp);
		}
	});

	xpc_connection_resume(conn);
}

mDNSlocal mDNSs8
handle_requests(xpc_object_t req)
{
	mDNSs8				ret = 0;
	xpc_connection_t	remote_conn = xpc_dictionary_get_remote_connection(req);

	LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_INFO, "C%p {action='handling log utility request'}", remote_conn);

	// create the dictionary for response purpose
	xpc_object_t response = xpc_dictionary_create_reply(req);
	if (response == mDNSNULL) {
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "C%p {error='cannot create reply response dictionary'}", remote_conn);
		return -1;
	}

	mDNSu32 reply_value;
	ret = check_permission(remote_conn);
	if (ret < 0) {
		// permission error
		reply_value = kDNSMsg_Error;
		if (ret == -1) {
			xpc_dictionary_set_string(response, kDNSErrorDescription, "Client must be running as root");
		} else if (ret == -2) {
			xpc_dictionary_set_string(response, kDNSErrorDescription, "Client is missing the entitlement");
		}
	} else if (xpc_dictionary_get_uint64(req, kDNSStateDump)) {
		mDNSu32		dump_option = (mDNSs32)xpc_dictionary_get_uint64(req, kDNSStateDump);
		char		full_file_name[PATH_MAX];
		mDNSs32		time_used;

		// We do not dump state in the customer build due to privacy consideration.
		if (IsAppleInternalBuild()) {
			int client_fd = xpc_dictionary_dup_fd(req, kDNSStateDumpFD);
			ret = handle_state_dump(dump_option, full_file_name, sizeof(full_file_name), client_fd, &time_used);
			if (ret == 0) {
				reply_value = kDNSMsg_NoError;
				xpc_dictionary_set_int64(response, kDNSStateDumpTimeUsed, time_used);

				if (dump_option != full_state_to_stdout) {
					xpc_dictionary_set_string(response, kDNSDumpFilePath, full_file_name);
				}
			} else {
				reply_value = kDNSMsg_Error;
				xpc_dictionary_set_string(response, kDNSErrorDescription, "State dump fails");
			}
			close(client_fd);
		} else {
			reply_value = kDNSMsg_Error;
			xpc_dictionary_set_string(response, kDNSErrorDescription, "State dump is only enabled in internal builds");
		}
	} else {
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR,
					"C%p {error='unknown log utility request from client'}", remote_conn);
		reply_value = kDNSMsg_UnknownRequest;
		xpc_dictionary_set_string(response, kDNSErrorDescription, "unknown log utility request from client");
	}

	xpc_dictionary_set_uint64(response, kDNSDaemonReply, reply_value);
	xpc_connection_send_message(remote_conn, response);
	MDNS_DISPOSE_XPC(response);

	return 0;
}

mDNSlocal mDNSs8
check_permission(xpc_connection_t connection)
{
	uid_t	client_euid = xpc_connection_get_euid(connection);
	int		client_pid = xpc_connection_get_pid(connection);
	mDNSs8	ret = 0;

	if (client_euid != 0) {
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEFAULT,
			"C%p {client_pid=%d,error='not running as root'}", connection, client_pid);
		ret = -1;
	}

	if (!IsEntitled(connection, kDNSLogUtilityService)){
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEFAULT,
			"C%p {client_pid=%d,error='Client is missing entitlement'}", connection, client_pid);
		ret = -2;
	}

	return ret;
}

/*
 * pointers of full_file_name and time_used are passed into, when function returns, full_file_name will be filled
 * with the full path to the dumped file, and time_used is filled with time duration(ms) when mDNSResponder is
 * blocked. if_get_lock indicates if we should lock the kqueue before dumping the state.
 */
mDNSlocal mDNSs8
handle_state_dump(mDNSu32 dump_option, char *full_file_name, mDNSu32 name_buffer_size, int client_fd,
	mDNSs32 *time_ms_used)
{
	mDNSs8 ret;

	// record the start time, and lock the kqueue
	struct timeval time_start;
	gettimeofday(&time_start, mDNSNULL);
	KQueueLock();

	if (dump_option == full_state_to_stdout) {
		dump_state_to_fd(client_fd);
		ret = 0;
	} else {
		// dump_option == full_state || dump_option == full_state_with_compression
		ret = handle_state_dump_to_fd(MDSNRESPONDER_STATE_DUMP_DIR, MDSNRESPONDER_STATE_DUMP_FILE_NAME,
			full_file_name, name_buffer_size, dump_option == full_state_with_compression ? mDNStrue : mDNSfalse);
	}

	// unlock the kqueue, record the end time and calculate the duration.
	KQueueUnlock("State Dump");
	struct timeval time_end;
	gettimeofday(&time_end, mDNSNULL);
	*time_ms_used = timediff_ms(&time_end, &time_start);

	return ret;
}

#define MAX_NUM_DUMP_FILES 5 // controls how many files we are allowed to created for the state dump
mDNSlocal mDNSs8
handle_state_dump_to_fd(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 buffer_size,
	mDNSBool if_compress){

	char			oldest_file_name[PATH_MAX];
	mDNSs32			dump_file_count = 0;
	int				ret;

	dump_file_count = find_oldest_state_dump(dump_dir, file_name, full_file_name, buffer_size, oldest_file_name,
		sizeof(oldest_file_name));
	require_action(dump_file_count >= 0, error,
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "find_oldest_state_dump fails"));

	ret = remove_state_dump_if_too_many(dump_dir, oldest_file_name, dump_file_count, MAX_NUM_DUMP_FILES);
	require_action(ret == 0, error,
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "remove_state_dump_if_too_many fails"));

	int fd = create_new_state_dump_file(dump_dir, file_name, full_file_name, buffer_size);
	require_action(fd >= 0, error,
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "create_new_state_dump_file fails"));

	dump_state_to_fd(fd);
	close(fd); // create_new_state_dump_file open the file, we have to close it here

	if (if_compress) {
		ret = compress_state_dump_and_delete(full_file_name, buffer_size);
		require_action(ret == 0, error,
			LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEFAULT, "State Dump: Error happens when trying to compress the state dump, reason: %s", strerror(errno)));
	}

	return 0;

error:
	return -1;
}

/*
 * Scan the directory, find all the files that start with <mDNSResponder state dump file name>. Return the number of
 * state dump files and the name of the oldest file created.
 */
mDNSlocal mDNSs32
find_oldest_state_dump(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 full_file_name_buffer_size,
	char *oldest_file_name, mDNSu32 oldest_file_name_buffer_size)
{
	int ret;
	int bytes_will_be_written;
	DIR *dir_p = NULL;
	mDNSu8 dump_file_count = 0;

	bytes_will_be_written = snprintf(full_file_name, full_file_name_buffer_size, "%s/%s", dump_dir, file_name);
	require_action_quiet(bytes_will_be_written > 0 && (mDNSu32) bytes_will_be_written < full_file_name_buffer_size, exit,
		ret = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "snprintf truncates the final string - "
			"full name buffer length: %u, directory name length: %zu, file name length: %zu",
			full_file_name_buffer_size, strlen(dump_dir), strlen(file_name))
	);

	dir_p = opendir(dump_dir);
	require_action_quiet(dir_p != NULL, exit, ret = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "State Dump: directory " PUB_S " cannot be opened, reason: "
			PUB_S, dump_dir, strerror(errno))
	);

	// scan every entry under directory, if starts with <mDNSResponder state dump file name>, check its create time.
	struct dirent	*dir_entry_p = mDNSNULL;
	size_t			file_name_len = strnlen(file_name, MAXPATHLEN);
	struct timespec oldest_time = {LONG_MAX, LONG_MAX};

	while ((dir_entry_p = readdir(dir_p)) != mDNSNULL) {
		if (dir_entry_p->d_namlen <= file_name_len)
			continue;

		if (strncmp(dir_entry_p->d_name, file_name, file_name_len) == 0) {
			struct stat file_state;
			bytes_will_be_written = snprintf(full_file_name, full_file_name_buffer_size, "%s/%s", dump_dir,
				dir_entry_p->d_name);
			require_action_quiet(bytes_will_be_written > 0 && (mDNSu32) bytes_will_be_written < full_file_name_buffer_size, exit,
				ret = -1;
				LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "snprintf truncates the final string - "
					"full name buffer length: %u, directory name length: %zu, file name length: %zu",
					full_file_name_buffer_size, strlen(dump_dir), strlen(dir_entry_p->d_name))
			);

			// use stat to get creation time
			ret = stat(full_file_name, &file_state);
			require_action_quiet(ret == 0, exit, ret = -1;
				LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "State Dump: error when reading file properties - "
					"reason: " PUB_S, strerror(errno)));
			// if the file is older than the current record
			if (oldest_time.tv_sec > file_state.st_birthtimespec.tv_sec
				|| (oldest_time.tv_sec == file_state.st_birthtimespec.tv_sec
					&& oldest_time.tv_sec > file_state.st_birthtimespec.tv_sec)) {
				oldest_time = file_state.st_birthtimespec;
				if (dir_entry_p->d_namlen >= oldest_file_name_buffer_size) {
					LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR,
						"State Dump: filename is too long to be put into the buffer, ignoring the current file"
						" - file name to be copied: " PRI_S ", length: %u, buffer length: %u",
						dir_entry_p->d_name, dir_entry_p->d_namlen, oldest_file_name_buffer_size);
					continue;
				}
				strlcpy(oldest_file_name, dir_entry_p->d_name, oldest_file_name_buffer_size);
			}

			dump_file_count++;
		}
	}

exit:
	if (dir_p != NULL) {
		closedir(dir_p);
	}
	return dump_file_count;
}

mDNSlocal mDNSs8
remove_state_dump_if_too_many(const char *dump_dir, const char *oldest_file_name, mDNSs32 dump_file_count,
	mDNSs32 max_allowed)
{
	int ret;
	char path_file_to_remove[PATH_MAX];
	path_file_to_remove[PATH_MAX - 1] = '\0';

	if (dump_file_count < max_allowed) {
		ret = 0;
		goto exit;
	}

	int bytes_will_be_written = snprintf(path_file_to_remove, sizeof(path_file_to_remove), "%s/%s", dump_dir,
		oldest_file_name);
	require_action_quiet(bytes_will_be_written > 0 && (size_t)bytes_will_be_written < sizeof(path_file_to_remove), exit,
		ret = -1;
		 LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "State Dump: snprintf truncates the final string - "
			"buffer length: %zu, directory name length: %zu, old file name length: %zu",
			sizeof(path_file_to_remove), strlen(dump_dir), strlen(oldest_file_name))
	);

	ret = remove(path_file_to_remove);
	require_action_quiet(ret == 0, exit, ret = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "State Dump: file " PUB_S " cannot be deleted, reason: "
			PUB_S, path_file_to_remove, strerror(errno)));

	ret = 0;
exit:
	return ret;
}

/*
 * Generate the file name of state dump with current time stamp, and return the FILE pointer, anyone who calls this
 * function must call fclose() to release the FILE pointer.
 */
mDNSlocal int
create_new_state_dump_file(const char *dump_dir, const char *file_name, char *full_file_name, mDNSu32 buffer_size)
{
	struct timeval		now;
	struct tm			local_time;
	char				date_time_str[32];
	char				time_zone_str[32];
	int 				fd;

	gettimeofday(&now, NULL);
	localtime_r(&now.tv_sec, &local_time);

	// 2008-08-08_20-00-00
	strftime(date_time_str, sizeof(date_time_str), "%F_%H-%M-%S", &local_time);
	// +0800
	strftime(time_zone_str, sizeof(time_zone_str), "%z", &local_time);
	// /private/var/log/mDNSResponder/mDNSResponder_state_dump_2008-08-08_20-00-00-000000+0800.txt
	int bytes_will_be_written = snprintf(full_file_name, buffer_size, "%s/%s_%s-%06lu%s." STATE_DUMP_PLAIN_SUFFIX,
		dump_dir, file_name, date_time_str, (unsigned long)now.tv_usec, time_zone_str);
	require_action_quiet(bytes_will_be_written > 0 && (mDNSu32)bytes_will_be_written < buffer_size, exit, fd = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_ERROR, "State Dump: snprintf truncates the final string - "
			"minimal buffer size: %d, actual buffer size: %u", bytes_will_be_written, buffer_size));

	fd = open(full_file_name, O_WRONLY | O_CREAT, 0644); // 0644 means * (owning) User: read & write * Group: read * Other: read
	require_action_quiet(fd >= 0, exit, fd = -1;
		LogRedact(MDNS_LOG_CATEGORY_DEFAULT, MDNS_LOG_ERROR, "State Dump: file " PUB_S " cannot be opened, reason: "
			PUB_S, full_file_name, strerror(errno))
	);

exit:
	return fd;
}

/*
 * Compress the state dump from pliantext to tar.bz2, remove the original one, and change the content of input_file to
 * newly created compressed file if compression succeeds.
 */
mDNSlocal mDNSs8
compress_state_dump_and_delete(char *input_file, mDNSu32 buffer_size)
{
	struct archive			*a				= mDNSNULL;
	mDNSBool				archive_opened	= mDNSfalse;
	struct archive_entry	*entry			= mDNSNULL;
	struct stat				st;
	int						fd = -1;
	char					output_file[PATH_MAX];
	void					*mapped_pointer = MAP_FAILED;
	int						ret;
	int						error;
	mDNSu32					file_size		= 0;

	output_file[PATH_MAX - 1] = '\0';

	a = archive_write_new();
	require_action(a != mDNSNULL, exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "archive_write_new fails: " PUB_S, archive_error_string(a)));
	archive_write_add_filter_bzip2(a);
	archive_write_set_format_ustar(a);

	// remove the .txt suffix, and append .tar.bz2 suffix
	size_t plain_file_name_len = strlen(input_file); // input_file is guaranteed to be '\0'-terminated
	// xxx.txt -> xxx.tar.bz2
	require_action_quiet(sizeof(output_file) > plain_file_name_len - strlen(STATE_DUMP_PLAIN_SUFFIX) + strlen(STATE_DUMP_COMPRESSED_SUFFIX),
		exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "buffer is not large enough to hold the file name "
			"- buffer length: %zu, required length: %zu", sizeof(output_file),
			plain_file_name_len - strlen(STATE_DUMP_PLAIN_SUFFIX) + strlen(STATE_DUMP_COMPRESSED_SUFFIX))
	);
	// "xxx." + "tar.bz2"
	snprintf(output_file, sizeof(output_file), "%.*s%s", (int)(strlen(input_file) - strlen(STATE_DUMP_PLAIN_SUFFIX)),
		input_file, STATE_DUMP_COMPRESSED_SUFFIX);

	// open/create the archive for the given path name
	ret = archive_write_open_filename(a, output_file);
	archive_opened = (ret == ARCHIVE_OK);
	require_action(archive_opened, exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "archive_write_open_filename fails: " PUB_S, archive_error_string(a)));

	// get the state of file to be compressed
	stat(input_file, &st);

	// entry is required to create an archive
	entry = archive_entry_new();
	require_action(entry != mDNSNULL, exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "archive_entry_new fails: " PUB_S, strerror(errno)));

	// set the name of file in the compressed file
	const char *file_name_with_timestamp = strstr(input_file, MDSNRESPONDER_STATE_DUMP_FILE_NAME);
	if (file_name_with_timestamp == mDNSNULL) {
		file_name_with_timestamp = MDSNRESPONDER_STATE_DUMP_FILE_NAME "." STATE_DUMP_PLAIN_SUFFIX;
	}

	// copy the original file state to entry
	archive_entry_copy_stat(entry, &st);
	archive_entry_set_pathname(entry, file_name_with_timestamp);

	// write entry into archive
	do {
		ret = archive_write_header(a, entry);
	} while (ret == ARCHIVE_RETRY);
	require_action(ret == ARCHIVE_OK, exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "archive_write_header fails: " PUB_S, archive_error_string(a)));

	// if the original file has something to compress, use mmap to read its content
	require_action_quiet(st.st_size > 0, exit, error = -1);
	require_action_quiet(st.st_size <= UINT32_MAX, exit, error = -1);

	file_size = (mDNSu32)st.st_size;
	fd = open(input_file, O_RDONLY);
	require_action_quiet(fd != -1, exit, error = -1);

	mapped_pointer = mmap(NULL, file_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	require_action(mapped_pointer != MAP_FAILED, exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "mmap fails: " PUB_S, strerror(errno)));

	const ssize_t amount_written = archive_write_data(a, mapped_pointer, file_size);
	require_action((amount_written >= 0) && (((size_t)amount_written) == file_size), exit, error = -1;
		LogRedact(MDNS_LOG_CATEGORY_XPC, MDNS_LOG_DEBUG, "archive_write_data fails: amount_written(%ld) != (%u)",
			(long)amount_written, file_size));

	error = 0;

exit:
	if (mapped_pointer != MAP_FAILED) {
		munmap(mapped_pointer, file_size); // undo mmap
	}
	if (fd != -1) {
		close(fd);							// undo open
	}
	if (entry != mDNSNULL) {
		archive_entry_free(entry);			// undo archive_entry_new
	}
	if (archive_opened) {
		archive_write_close(a);				// undo archive_write_open_filename
	}
	if (a != mDNSNULL) {
		archive_write_free(a);				// undo archive_write_new
	}
	remove(input_file);
	if (error == 0 && strlen(output_file) < buffer_size) {
		strlcpy(input_file, output_file, buffer_size);
	} else {
		error = -1;
		input_file[0] = '\0';
	}
	return error;
}

/*
 * Return the time difference(ms) between two struct timeval.
 */
#define US_PER_S 1000000
#define MS_PER_S 1000
mDNSlocal mDNSs32
timediff_ms(struct timeval *t1, struct timeval *t2)
{
	int usec, ms, sec;

	if (t1->tv_sec < t2->tv_sec || (t1->tv_sec == t2->tv_sec && t1->tv_usec < t2->tv_usec)) {
		return -timediff_ms(t2, t1);
	}

	sec = (int)(t1->tv_sec - t2->tv_sec);
	if (t1->tv_usec >= t2->tv_usec) {
		usec = t1->tv_usec - t2->tv_usec;
	} else {
		usec = t1->tv_usec + US_PER_S - t2->tv_usec;
		sec -= 1;
	}
	ms = sec * MS_PER_S;
	ms += usec / MS_PER_S;
	return ms;
}
