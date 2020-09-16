/* cti-server.c
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
 * This code adds border router support to 3rd party HomeKit Routers as part of Apple’s commitment to the CHIP project.
 *
 * Concise Thread Interface Server
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>

#include "cti-server.h"
#include "cti-proto.h"

#include <syslog.h>

int cti_listener_fd;
cti_connection_t connections;

void
cti_connection_close(cti_connection_t connection)
{
	if (connection->fd != -1) {
		close(connection->fd);
		connection->fd = -1;
	}
}

static void
cti_service_add_parse(cti_connection_t connection)
{
    uint32_t enterprise_id;
    uint16_t service_data_length;
    uint16_t server_data_length;
    int status = kCTIStatus_NoError;
    void *service_data;
    void *server_data;
    char service_data_buf[13], server_data_buf[55];

    // And statement will fail as soon as anything fails to parse.
    if (cti_connection_u32_parse(connection, &enterprise_id) &&
        cti_connection_data_parse(connection, &service_data, &service_data_length) &&
        cti_connection_data_parse(connection, &server_data, &server_data_length) &&
        cti_connection_parse_done(connection))
    {
        dump_to_hex(service_data, service_data_length, service_data_buf, sizeof(service_data_buf));
        dump_to_hex(server_data, server_data_length, server_data_buf, sizeof(server_data_buf));
        syslog(LOG_INFO, "cti_service_add_parse: %" PRIu32 " %zd[ %s ] %zd[ %s ]",
               enterprise_id, service_data_length, service_data_buf, server_data_length, server_data_buf);

#ifndef POSIX_BUILD
        status = ctiAddService(enterprise_id,
                               service_data,
                               service_data_length,
                               server_data,
                               server_data_length);

#else
        status = kCTIStatus_NoError;
#endif
        cti_send_response(connection, status);
    }
}

static void
cti_service_remove_parse(cti_connection_t connection)
{
    uint32_t enterprise_id;
    uint16_t service_data_length;
    int status = kCTIStatus_NoError;
    void *service_data;
    char service_data_buf[13];

    // And statement will fail as soon as anything fails to parse.
    if (cti_connection_u32_parse(connection, &enterprise_id) &&
        cti_connection_data_parse(connection, &service_data, &service_data_length) &&
        cti_connection_parse_done(connection))
    {
        dump_to_hex(service_data, service_data_length, service_data_buf, sizeof(service_data_buf));
        syslog(LOG_INFO, "cti_service_add_parse: %" PRIu32 " %zd[ %s ]", enterprise_id, service_data_length, service_data_buf);

#ifndef POSIX_BUILD
        status = ctiRemoveService(enterprise_id,
                                  service_data,
                                  service_data_length);
#else
        status = kCTIStatus_NoError;
#endif
        cti_send_response(connection, status);
    }
}

static void
cti_prefix_add_parse(cti_connection_t connection)
{
    uint32_t preferred, valid;
    uint8_t prefix_length;
    struct in6_addr prefix;
    void *prefix_data = NULL;
    uint16_t prefix_data_length;
    bool slaac, on_mesh, stable;
    if (cti_connection_u32_parse(connection, &preferred) &&
        cti_connection_u32_parse(connection, &valid) &&
        cti_connection_data_parse(connection, &prefix_data, &prefix_data_length) &&
        cti_connection_u8_parse(connection, &prefix_length) &&
        cti_connection_bool_parse(connection, &slaac) &&
        cti_connection_bool_parse(connection, &on_mesh) &&
        cti_connection_bool_parse(connection, &stable) &&
        cti_connection_parse_done(connection))
    {
        int status = kCTIStatus_NoError;
        if (prefix_data_length != 8) {
            status = kCTIStatus_Invalid;
        } else {
            memset(((char *)&prefix) + 8, 0, sizeof(prefix) - 8);
            memcpy(&prefix, prefix_data, prefix_data_length);
#ifndef POSIX_BUILD
            status = ctiAddMeshPrefix(&prefix, prefix_length, on_mesh, true, slaac, stable);
#endif
        }
        cti_send_response(connection, status);
    }
    if (prefix_data != NULL) {
        free(prefix_data);
    }
}

static void
cti_prefix_remove_parse(cti_connection_t connection)
{
    uint8_t prefix_length;
    struct in6_addr prefix;
    void *prefix_data = NULL;
    uint16_t prefix_data_length;
    if (cti_connection_data_parse(connection, &prefix_data, &prefix_data_length) &&
        cti_connection_u8_parse(connection, &prefix_length) &&
        cti_connection_parse_done(connection))
    {
        int status = kCTIStatus_NoError;
        if (prefix_data_length != 8) {
            status = kCTIStatus_Invalid;
        } else {
            memset(((char *)&prefix) + 8, 0, sizeof(prefix) - 8);
            memcpy(&prefix, prefix_data, prefix_data_length);
#ifndef POSIX_BUILD
            status = ctiRemoveMeshPrefix(&prefix, prefix_length);
#endif
            cti_send_response(connection, status);
        }
    }
    if (prefix_data != NULL) {
        free(prefix_data);
    }
}

static void
cti_get_tunnel_name_parse(cti_connection_t connection)
{
    if (cti_connection_parse_done(connection)) {
#ifndef POSIX_BUILD
        ctiRetrieveTunnel(connection);
#endif
    }
}

static void
cti_message_parse(cti_connection_t connection)
{
    cti_connection_parse_start(connection);
    if (!cti_connection_u16_parse(connection, &connection->message_type)) {
        return;
    }
    switch(connection->message_type) {
    case kCTIMessageType_AddService:
        cti_service_add_parse(connection);
        break;
    case kCTIMessageType_RemoveService:
        cti_service_remove_parse(connection);
        break;
    case kCTIMessageType_AddPrefix:
        cti_prefix_add_parse(connection);
        break;
    case kCTIMessageType_RemovePrefix:
        cti_prefix_remove_parse(connection);
        break;
    case kCTIMessageType_GetTunnelName:
        cti_get_tunnel_name_parse(connection);
        break;
    case kCTIMessageType_RequestStateEvents:
        if (cti_connection_parse_done(connection)) {
            connection->registered_event_flags |= CTI_EVENT_STATE;
            cti_send_response(connection, kCTIStatus_NoError);
#ifndef POSIX_BUILD
            ctiRetrieveNodeType(connection, CTI_EVENT_STATE);

#endif
        }
        break;
    case kCTIMessageType_RequestPartitionEvents:
        if (cti_connection_parse_done(connection)) {
            connection->registered_event_flags |= CTI_EVENT_PARTITION_ID;
            cti_send_response(connection, kCTIStatus_NoError);
#ifndef POSIX_BUILD
            ctiRetrievePartitionId(connection, CTI_EVENT_PARTITION_ID);
#endif
        }
        break;
    case kCTIMessageType_RequestRoleEvents:
        if (cti_connection_parse_done(connection)) {
            connection->registered_event_flags |= CTI_EVENT_ROLE;
            cti_send_response(connection, kCTIStatus_NoError);
#ifndef POSIX_BUILD
            ctiRetrieveNodeType(connection, CTI_EVENT_ROLE);
#endif
        }
        break;
    case kCTIMessageType_RequestServiceEvents:
        if (cti_connection_parse_done(connection)) {
            connection->registered_event_flags |= CTI_EVENT_SERVICE;
            cti_send_response(connection, kCTIStatus_NoError);
#ifndef POSIX_BUILD
            ctiRetrieveServiceList(connection, CTI_EVENT_SERVICE);
#endif
        }
        break;
    case kCTIMessageType_RequestPrefixEvents:
        if (cti_connection_parse_done(connection)) {
            connection->registered_event_flags |= CTI_EVENT_PREFIX;
            cti_send_response(connection, kCTIStatus_NoError);
#ifndef POSIX_BUILD
            ctiRetrievePrefixList(connection, CTI_EVENT_PREFIX);
#endif
        }
        break;
    default:
        cti_send_response(connection, kCTIStatus_Invalid);

    }
}

static void
cti_accept(void)
{
    cti_connection_t connection;
    struct sockaddr_un addr;
    socklen_t socksize = sizeof(addr);
    int fd = accept(cti_listener_fd, (struct sockaddr *)&addr, &socksize);
    size_t len;
    struct ucred ucred;

    if (fd < 0) {
        syslog(LOG_ERR, "cti_accept: accept: %s", strerror(errno));
        return;
    }

    len = sizeof(struct ucred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) < 0) {
        syslog(LOG_ERR, "cti_accept: unable to get peer credentials for incoming connection on "
               SERVER_SOCKET_NAME ": %s", strerror(errno));
    out:
        close(fd);
        return;
    }

    if (ucred.uid != 0) {
        char **s;
        struct group *group = getgrnam("cti-clients");
        if (group == NULL) {
            syslog(LOG_ERR, "cti_accept: connecting user %d is not root and there is no cti-clients group.", ucred.uid);
            goto out;
        } else if (group->gr_gid == ucred.gid) {
        } else {
            struct passwd *passwd = getpwuid(ucred.uid);
            if (passwd == NULL || passwd->pw_name == NULL) {
                syslog(LOG_ERR, "cti_accept: connecting user %d is not root and has no username.", ucred.uid);
                goto out;
            } else if (group->gr_mem == NULL || *group->gr_mem == NULL) {
                syslog(LOG_ERR, "cti_accept: connecting user %s is not a member of cti-clients group.", passwd->pw_name);
                goto out;
            } else {
                for (s = group->gr_mem; s != NULL && *s != NULL; s++) {
                    if (!strcmp(*s, passwd->pw_name)) {
                        break;
                    }
                }
                if (*s == NULL) {
                    syslog(LOG_ERR, "cti_accept: connecting user %s is not a member of cti-clients group.", passwd->pw_name);
                    goto out;
                }
            }
        }
    }

#ifdef SO_NOSIGPIPE
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one) < 0) {
        syslog(LOG_ERR, "SO_NOSIGPIPE failed: %s", strerror(errno));

#endif
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        syslog(LOG_ERR, "cti_accept: can't set O_NONBLOCK: %s", strerror(errno));
        goto out;
    }

    // User is authenticated.
    connection = cti_connection_allocate(100);
    if (connection == NULL) {
        goto out;
    }
    connection->fd = fd;
    connection->next = connections;
    connections = connection;
    syslog(LOG_INFO, "cti_accept: connection from user %d accepted", ucred.uid);
}

int
cti_init(void)
{
    struct sockaddr_un addr;

    if (unlink(SERVER_SOCKET_NAME) < 0 && errno != ENOENT) {
        syslog(LOG_ERR, "cti_init: unlink(" SERVER_SOCKET_NAME ": %s", strerror(errno));
        return -1;
    }

    addr.sun_family = AF_LOCAL;
    if (sizeof(SERVER_SOCKET_NAME) > sizeof(addr.sun_path)) {
        syslog(LOG_ERR, "cti_init: no space for unix socket named " SERVER_SOCKET_NAME ".");
        return -1;
    }
    strncpy(addr.sun_path, SERVER_SOCKET_NAME, sizeof(addr.sun_path));
#ifndef NOT_HAVE_SA_LEN
    addr.sun_len = strlen(addr.sun_path) + 1 + sizeof(addr.sun_len) + sizeof(addr.sun_family);
#endif

    cti_listener_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (cti_listener_fd < 0) {
        syslog(LOG_ERR, "cti_init: socket: %s", strerror(errno));
        return -1;
    }

    if (bind(cti_listener_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "cti_init: %s", strerror(errno));
        close(cti_listener_fd);
        return -1;
    }

    if (listen(cti_listener_fd, 1) < 0) {
        syslog(LOG_ERR, "cti_init: listen: %s", strerror(errno));
        close(cti_listener_fd);
        return -1;
    }
    return 0;
}

void
cti_fd_init(int *p_nfds, fd_set *r)
{
    int nfds = *p_nfds;
    cti_connection_t connection, *p_connection;

    if (cti_listener_fd >= nfds) {
        nfds = cti_listener_fd + 1;
    }
    FD_SET(cti_listener_fd, r);

    // GC any closed connections.
    for (p_connection = &connections; *p_connection; ) {
        connection = *p_connection;
        if (connection->fd == -1) {
            *p_connection = connection->next;
            cti_connection_finalize(connection);
        } else {
            p_connection = &connection->next;
        }
    }

    // Now process input on any connections that are still around.
    for (connection = connections; connection; connection = connection->next) {
        if (connection->fd >= nfds) {
            nfds = connection->fd + 1;
        }
        FD_SET(connection->fd, r);
    }
    *p_nfds = nfds;
}

void
cti_fd_process(fd_set *r)
{
    cti_connection_t connection;

    if (FD_ISSET(cti_listener_fd, r)) {
        cti_accept();
    }

    for (connection = connections; connection; connection = connection->next) {
        if (connection->fd != -1 && FD_ISSET(connection->fd, r)) {
            cti_read(connection, cti_message_parse);
        }
    }
}

void
cti_notify_event(unsigned int evt, send_event_t evt_handler)
{
    // Walk through connections and see if have registered for this particular event.
    cti_connection_t connection = connections;
    while (connection) {
        if (evt & connection->registered_event_flags) {
            evt_handler(connection, evt);
        }
        connection = connection->next;
    }
}

#ifdef POSIX_BUILD
int
main(int argc, char **argv)
{
    fd_set fd_r, fd_w, fd_x;
    int nfds = 0;

    openlog("cti-server", LOG_PERROR, LOG_DAEMON);
    signal(SIGPIPE, SIG_IGN); // because why ever?
    cti_init();

    do {
        FD_ZERO(&fd_r);
        FD_ZERO(&fd_w);
        FD_ZERO(&fd_x);

        cti_fd_init(&nfds, &fd_r);
        syslog(LOG_INFO, "selecting: %d descriptors.", nfds);
        if (select(nfds, &fd_r, &fd_w, &fd_x, NULL) < 0) {
            syslog(LOG_ERR, "select: %s", strerror(errno));
            exit(1);
        }

        cti_fd_process(&fd_r);
    } while (1);
}
#endif

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
