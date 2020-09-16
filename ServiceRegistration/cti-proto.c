/* cti-proto.c
 *
 * Copyright (c) 2020 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * CTI protocol communication primitives
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdint.h>

#include "cti-common.h"
#include "cti-proto.h"

void
cti_connection_finalize(cti_connection_t connection)
{
	if (connection->input.buffer) {
		free(connection->input.buffer);
	}
	if (connection->output.buffer) {
		free(connection->output.buffer);
	}
	free(connection);
}

void
dump_to_hex(uint8_t *data, size_t length, char *buffer, int len)
{
	char *s = buffer;
	unsigned int i;
	for (i = 0; i < length; i++) {
		size_t avail = len - (s - buffer);
		if (i == 0) {
			snprintf(s, avail, "%02x", data[i]);
		} else {
			snprintf(s, avail, ":%02x", data[i]);
		}
		s += strlen(s);
	}
}

bool
cti_make_space(cti_buffer_t *buf, size_t space)
{
	if (buf->buffer == NULL) {
		buf->current = 0;
		buf->size = space;
		buf->buffer = malloc(space);
		if (buf->buffer == NULL) {
			syslog(LOG_ERR, "cti_make_space: no memory for %zd bytes.", space);
			return false;
		}
	}
	if (buf->current + space > buf->size) {
		size_t next_increment = buf->size * 2;
		if (buf->current + space > buf->size) {
			next_increment += space;
		}
		void *new_buf = malloc(next_increment);
		if (new_buf == NULL) {
			syslog(LOG_ERR, "cti_make_space: no memory for %zd bytes.", next_increment);
			return false;
		}
		memcpy(new_buf, buf->buffer, buf->current);
		free(buf->buffer);
		buf->buffer = new_buf;
	}
	return true;
}

static bool
cti_put(cti_connection_t connection, const void *data, size_t length)
{
	if (!cti_make_space(&connection->output, length)) {
		cti_connection_close(connection);
		return false;
	}

	memcpy(connection->output.buffer + connection->output.current, data, length);
	connection->output.current += length;
	return true;
}

bool
cti_connection_u32_put(cti_connection_t connection, uint32_t val)
{
	uint32_t rval = htonl(val);
	return cti_put(connection, &rval, sizeof(rval));
}

bool
cti_connection_i32_put(cti_connection_t connection, int32_t val)
{
	uint32_t uval = htonl((uint32_t)val);
	return cti_connection_u32_put(connection, uval);
}

bool
cti_connection_u16_put(cti_connection_t connection, uint16_t val)
{
	uint16_t rval = htons(val);
	return cti_put(connection, &rval, sizeof(rval));
}

bool
cti_connection_u8_put(cti_connection_t connection, uint8_t val)
{
	uint8_t rval = val;
	return cti_put(connection, &rval, sizeof(rval));
}

bool
cti_connection_bool_put(cti_connection_t connection, bool val)
{
	uint8_t rval = val ? 1 : 0;
	return cti_put(connection, &rval, sizeof(rval));
}

bool
cti_connection_data_put(cti_connection_t connection, const void *data, uint16_t length)
{
	return (cti_connection_u16_put(connection, length) && cti_put(connection, data, length));
}

bool
cti_connection_string_put(cti_connection_t connection, const char *string)
{
	uint16_t len;
    if (string == NULL) {
        return cti_connection_u16_put(connection, 0xffff);
    }
    len = strlen(string);
    return (cti_connection_u16_put(connection, len) && cti_put(connection, string, len));
}

bool
cti_parse(cti_connection_t connection, void *buffer, uint16_t length)
{
	if (length > connection->message_length - connection->input.current) {
		syslog(LOG_ERR, "cti_parse: bogus data element length %d exceeds available space %zd",
			   length, connection->message_length - connection->input.current);
		cti_connection_close(connection);
		return false;
	}

	memcpy(buffer, connection->input.buffer + connection->input.current, length);
	connection->input.current += length;
	return true;
}


bool
cti_connection_u32_parse(cti_connection_t connection, uint32_t *val)
{
	uint32_t rval;
	if (!cti_parse(connection, &rval, sizeof(rval))) {
		return false;
	}
	*val = ntohl(rval);
	return true;
}

bool
cti_connection_i32_parse(cti_connection_t connection, int32_t *val)
{
	uint32_t uval;
	if (!cti_parse(connection, &uval, sizeof(uval))) {
		return false;
	}
	*val = (int32_t)ntohl(uval);
	return true;
}

bool
cti_connection_u16_parse(cti_connection_t connection, uint16_t *val)
{
	uint16_t rval;
	if (!cti_parse(connection, &rval, sizeof(rval))) {
		return false;
	}
	*val = ntohs(rval);
	return true;
}

bool
cti_connection_u8_parse(cti_connection_t connection, uint8_t *val)
{
    uint8_t rval;
	if (cti_parse(connection, &rval, sizeof(rval))) {
        *val = rval;
        return true;
    }
    return false;
}

bool
cti_connection_bool_parse(cti_connection_t connection, bool *val)
{
	uint8_t rval;
	if (!cti_parse(connection, &rval, sizeof(rval))) {
		return false;
	}
	*val = rval == 0 ? false : true;
	return true;
}

bool
cti_connection_data_parse(cti_connection_t connection, void **data, uint16_t *length)
{
	uint16_t len;
	void *ret;
	if (!cti_connection_u16_parse(connection, &len)) {
		return false;
	}
	if (len > connection->message_length - connection->input.current) {
		syslog(LOG_ERR, "cti_connection_data_parse: bogus data element length %d exceeds available space %zd",
			   len, connection->message_length - connection->input.current);
		cti_connection_close(connection);
		return false;
	}
	ret = malloc(len);
	if (ret == NULL) {
		syslog(LOG_ERR, "cti_connection_data_parse: out of memory for %d byte data element", len);
		cti_connection_close(connection);
		return false;
	}
	if (!cti_parse(connection, ret, len)) {
		free(ret);
		return false;
	}
    *length = len;
	*data = ret;
	return true;
}

bool
cti_connection_string_parse(cti_connection_t connection, char **string)
{
	uint16_t len;
	char *ret;
	if (!cti_connection_u16_parse(connection, &len)) {
		return false;
	}
    if (len == 0xffff) {
        *string = NULL;
        return true;
    }
	if (len > connection->message_length - connection->input.current) {
		syslog(LOG_ERR, "cti_connection_string_parse: bogus data element length %d exceeds available space %zd",
			   len, connection->message_length - connection->input.current);
		cti_connection_close(connection);
		return false;
	}
	ret = malloc(len + 1);
	if (ret == NULL) {
		syslog(LOG_ERR, "cti_connection_string_parse: out of memory for %d byte string", len);
		cti_connection_close(connection);
		return false;
	}
	if (!cti_parse(connection, ret, len)) {
		free(ret);
		return false;
	}
    ret[len] = 0;
	*string = ret;
	return true;
}

void
cti_connection_parse_start(cti_connection_t connection)
{
	connection->input.current = 0;
}

bool
cti_connection_parse_done(cti_connection_t connection)
{
	if (connection->input.current != connection->message_length) {
		syslog(LOG_ERR, "cti_connection_parse_done: %zd bytes of junk at end of message",
			   connection->message_length - connection->input.current);
		return false;
	}
	return true;
}

bool
cti_connection_message_create(cti_connection_t connection, int message_type, uint16_t space)
{
	connection->output.current = 0;
	if (connection->output.buffer != NULL) {
		if (connection->output.size < space) {
			free(connection->output.buffer);
			connection->output.buffer = NULL;
		}
	}
    // +4 for the length and the message type, which the caller isn't expected to track.
	if (!cti_make_space(&connection->output, space + 4)) {
		cti_connection_close(connection);
		return false;
	}
	// Space for length, which is stored last.
	connection->output.current = 2;
	return cti_connection_u16_put(connection, message_type);
}

bool
cti_connection_message_send(cti_connection_t connection)
{
	size_t offset = connection->output.current;
    uint16_t len;
	ssize_t result;
	connection->output.current = 0;
	if (offset > UINT16_MAX) {
		syslog(LOG_ERR, "cti_connection_send: too big (%zd)", offset);
	out:
		cti_connection_close(connection);
		return false;
	}
    len = (uint16_t)offset - 2;
	if (!cti_connection_u16_put(connection, len)) {
		return false;
	}
	result = write(connection->fd, connection->output.buffer, offset);
	if (result < 0) {
		syslog(LOG_ERR, "cti_connection_send: write: %s", strerror(errno));
		goto out;
	}
	if ((size_t)result != offset) {
		syslog(LOG_ERR, "cti_connection_send: short write: %zd instead of %zd", result, offset);
		goto out;
	}
	return true;
}

bool
cti_send_response(cti_connection_t connection, int status)
{

	if (cti_connection_message_create(connection, kCTIMessageType_Response, 10) &&
		cti_connection_u16_put(connection, connection->message_type) &&
		cti_connection_u32_put(connection, status))
	{
		return cti_connection_message_send(connection);
	}
	return false;
}

void
cti_read(cti_connection_t connection, cti_datagram_callback_t datagram_callback)
{
	size_t needed = connection->input.expected - connection->input.current;
	if (needed > 0) {
		if (!cti_make_space(&connection->input, needed)) {
			cti_connection_close(connection);
			return;
		}
		ssize_t result = read(connection->fd, connection->input.buffer + connection->input.current, needed);
		if (result < 0) {
			syslog(LOG_INFO, "cti_read_callback: read: %s", strerror(errno));
			cti_connection_close(connection);
			return;
		}
        if (result == 0) {
            syslog(LOG_INFO, "cti_read_callback: remote close");
            cti_connection_close(connection);
            return;
        }
		connection->input.current += result;
		if ((size_t)result < needed) {
			return;
		}
	}
	// We have finished reading the length of the next message.
	if (connection->message_length == 0) {
		if (connection->input.expected != 2) {
			syslog(LOG_ERR, "cti_read_callback: invalid expected length: %zd", connection->input.expected);
			cti_connection_close(connection);
			return;
		}
		connection->message_length = ((size_t)connection->input.buffer[0] << 8) + connection->input.buffer[1];
		connection->input.current = 0;
		connection->input.expected = connection->message_length;
		return;
	}
	// We have finished reading a message.
	datagram_callback(connection);

	// Read the next one.
	connection->input.expected = 2;
	connection->message_length = 0;
	connection->input.current = 0;
}

cti_connection_t
cti_connection_allocate(uint16_t expected_size)
{
	cti_connection_t connection = calloc(1, sizeof(*connection));
	if (connection == NULL) {
		syslog(LOG_ERR, "cti_accept: no memory for connection structure.");
		return NULL;
	}
	if (!cti_make_space(&connection->input, expected_size)) {
		cti_connection_finalize(connection);
		return NULL;
	}
	connection->input.expected = 2;
	return connection;
}

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
