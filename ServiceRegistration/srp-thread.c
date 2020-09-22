/* srp-thread.c
 *
 * Copyright (c) 2019-2020 Apple Computer, Inc. All rights reserved.
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
 * srp host API implementation for Thread accessories using OpenThread.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <openthread/ip6.h>
#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/joiner.h>
#include <openthread/message.h>
#include <openthread/udp.h>
#include <openthread/platform/time.h>
#include <openthread/platform/settings.h>


#include "srp.h"
#include "srp-thread.h"
#include "srp-api.h"
#include "dns_sd.h"
#include "dns-msg.h"
#include "dns_sd.h"
#include "srp-crypto.h"

#define TIME_FREQUENCY 1000000ULL

const char *key_filename = "srp.key";

#define SRP_IO_CONTEXT_MAGIC 0xFEEDFACEFADEBEEFULL  // BEES!   Everybody gets BEES!
typedef struct io_context io_context_t;

struct io_context {
    uint64_t magic_cookie1;
    io_context_t *next;
    uint64_t wakeup_time;
    void *NONNULL srp_context;
    otSockAddr sockaddr;
    otUdpSocket sock;
    srp_wakeup_callback_t wakeup_callback;
    srp_datagram_callback_t datagram_callback;
    bool sock_active;
    uint64_t magic_cookie2;
} *io_contexts;

static otInstance *otThreadInstance;

static int
validate_io_context(io_context_t **dest, void *src)
{
    io_context_t *context = src;
    if (context->magic_cookie1 == SRP_IO_CONTEXT_MAGIC &&
        context->magic_cookie2 == SRP_IO_CONTEXT_MAGIC)
    {
        *dest = context;
        return kDNSServiceErr_NoError;
    }
    return kDNSServiceErr_BadState;
}

void
datagram_callback(void *context, otMessage *message, const otMessageInfo *messageInfo)
{

    OT_UNUSED_VARIABLE(context);
    OT_UNUSED_VARIABLE(message);
    OT_UNUSED_VARIABLE(messageInfo);

    static uint8_t *buf;
    const int buf_len = 1500;
    int length;
    io_context_t *io_context;
    if (validate_io_context(&io_context, context) == kDNSServiceErr_NoError) {
        if (buf == NULL) {
            buf = malloc(buf_len);
            if (buf == NULL) {
                INFO("No memory for read buffer");
                return;
            }
        }

        DEBUG("%d bytes received", otMessageGetLength(message) - otMessageGetOffset(message));
        length = otMessageRead(message, otMessageGetOffset(message), buf, buf_len - 1);
        io_context->datagram_callback(io_context->srp_context, buf, length);
    }
}

static void
note_wakeup(const char *what, void *at, uint64_t when)
{
    OT_UNUSED_VARIABLE(what);
    OT_UNUSED_VARIABLE(at);
    OT_UNUSED_VARIABLE(when);

#ifdef VERBOSE_DEBUG_MESSAGES
    int microseconds = (int)(when % TIME_FREQUENCY);
    int seconds = when / TIME_FREQUENCY;
    int minute = (int)((seconds / 60) % 60);
    int hour = (int)((seconds / 3600) % (7 * 24));
    int second = (int)(seconds % 60);

    DEBUG(PUB_S_SRP " %p at %llu %d:%d:%d.%d", what, at, when, hour, minute, second, microseconds);
#endif
}

// TODO:  SRP should be running a timer.  The timer is scheduled in srp_set_wakeup and this is
//        its callback.  OT does not expose its timer api, so for now this demo asks the client to 
//        periodically call this function where we check to see if the wakeup time has been met.
int srp_process_time()
{
    io_context_t *io_context;
    uint64_t now = otPlatTimeGet(), next = 0;
    bool more;

    static int locCount = 0;

    do {
        more = false;
        for (io_context = io_contexts; io_context; io_context = io_context->next) {
            if (io_context->wakeup_time != 0 && io_context->wakeup_time <= now) {
                more = true;
                note_wakeup("io wakeup", io_context, io_context->wakeup_time);
                io_context->wakeup_time = 0;
                io_context->wakeup_callback(io_context->srp_context);
                break;
            }
            if (next == 0 || (io_context->wakeup_time != 0 && io_context->wakeup_time < next))
            {
                next = io_context->wakeup_time;
            }
        }
    } while (more);

    locCount++;

    return kDNSServiceErr_NoError;
}

int
srp_deactivate_udp_context(void *host_context, void *in_context)
{
    io_context_t *io_context, **p_io_contexts;
    int err;

    OT_UNUSED_VARIABLE(host_context);

    err = validate_io_context(&io_context, in_context);
    if (err == kDNSServiceErr_NoError) {
        for (p_io_contexts = &io_contexts; *p_io_contexts; p_io_contexts = &(*p_io_contexts)->next) {
            if (*p_io_contexts == io_context) {
                break;
            }
        }
        // If we don't find it on the list, something is wrong.
        if (*p_io_contexts == NULL) {
            return kDNSServiceErr_Invalid;
        }
        *p_io_contexts = io_context->next;
        io_context->wakeup_time = 0;
        if (io_context->sock_active) {
            otUdpClose(otThreadInstance, &io_context->sock);
        }
        free(io_context);
    }
    return err;
}

int
srp_connect_udp(void *context, const uint8_t *port, uint16_t address_type, const uint8_t *address, uint16_t addrlen)
{
    io_context_t *io_context;
    int err, oterr;
    err = validate_io_context(&io_context, context);
    if (err == kDNSServiceErr_NoError) {
        if (address_type != dns_rrtype_aaaa || addrlen != 16) {
            ERROR("srp_make_udp_context: invalid address");
            return kDNSServiceErr_Invalid;
        }
        memcpy(&io_context->sockaddr.mAddress, address, 16);
        memcpy(&io_context->sockaddr.mPort, port, 2);
#ifdef OT_NETIF_INTERFACE_ID_THREAD
        io_context->sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;
#endif
        oterr = otUdpOpen(otThreadInstance, &io_context->sock, datagram_callback, io_context);
        if (oterr != OT_ERROR_NONE) {
            ERROR("srp_make_udp_context: otUdpOpen returned %d", oterr);
            return kDNSServiceErr_Unknown;
        }
        oterr = otUdpConnect(otThreadInstance, &io_context->sock, &io_context->sockaddr);
        if (oterr != OT_ERROR_NONE) {
            otUdpClose(otThreadInstance, &io_context->sock);
            ERROR("srp_make_udp_context: otUdpConnect returned %d", oterr);
            return kDNSServiceErr_Unknown;
        }
        io_context->sock_active = true;
        err = kDNSServiceErr_NoError;
    }
    return err;
}

int
srp_disconnect_udp(void *context)
{
    io_context_t *io_context;
    int err;

    err = validate_io_context(&io_context, context);
    if (err == kDNSServiceErr_NoError && io_context->sock_active) {
        otUdpClose(otThreadInstance, &io_context->sock);
        io_context->sock_active = false;
    }
    return err;
}

int
srp_make_udp_context(void *host_context, void **p_context, srp_datagram_callback_t callback, void *context)
{
    io_context_t *io_context = calloc(1, sizeof *io_context);

    OT_UNUSED_VARIABLE(host_context);

    if (io_context == NULL) {
        ERROR("srp_make_udp_context: no memory");
        return kDNSServiceErr_NoMemory;
    }
    io_context->magic_cookie1 = io_context->magic_cookie2 = SRP_IO_CONTEXT_MAGIC;
    io_context->datagram_callback = callback;
    io_context->srp_context = context;

    *p_context = io_context;
    io_context->next = io_contexts;
    io_contexts = io_context;
    return kDNSServiceErr_NoError;
}

int
srp_set_wakeup(void *host_context, void *context, int milliseconds, srp_wakeup_callback_t callback)
{
    int err;
    io_context_t *io_context;
    uint64_t now;

    OT_UNUSED_VARIABLE(host_context);

    err = validate_io_context(&io_context, context);
    if (err == kDNSServiceErr_NoError) {
        now = otPlatTimeGet();
        io_context->wakeup_time = now + milliseconds * (TIME_FREQUENCY / 1000);
        io_context->wakeup_callback = callback;
        INFO("srp_set_wakeup: %llu (%llu + %dms)", io_context->wakeup_time, now, milliseconds);

        // TODO:  This is where we should be setting our timer that would trigger srp_process_time.
        //        Unfortunately OT does not expose a timer API so instead we ask that the client
        //        periodically call srp_process_time to fake it.
    }
    return err;
}

int
srp_cancel_wakeup(void *host_context, void *context)
{
    int err;

    OT_UNUSED_VARIABLE(host_context);
    OT_UNUSED_VARIABLE(context);

    io_context_t *io_context;

    err = validate_io_context(&io_context, context);
    if (err == kDNSServiceErr_NoError) {
        io_context->wakeup_time = 0;
    }
    return err;
}

int
srp_send_datagram(void *host_context, void *context, void *payload, size_t message_length)
{
    int err;
    io_context_t *io_context;
    otError       error;
    otMessageInfo messageInfo;
    otMessage *   message = NULL;
    uint8_t *ap;

    OT_UNUSED_VARIABLE(host_context);


#ifdef VERBOSE_DEBUG_MESSAGES
    int i, j;
    char buf[80], *bufp;
    char *hexdigits = "01234567689abcdef";
    uint8_t *msg = payload;
#endif // VERBOSE_DEBUG_MESSAGES

    err = validate_io_context(&io_context, context);
    if (err == kDNSServiceErr_NoError) {
        memset(&messageInfo, 0, sizeof(messageInfo));
#ifdef OT_NETIF_INTERFACE_ID_THREAD
        messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
#endif
        messageInfo.mPeerPort    = io_context->sockaddr.mPort;
        messageInfo.mPeerAddr    = io_context->sockaddr.mAddress;
        ap = (uint8_t *)&io_context->sockaddr.mAddress;        
        INFO("Sending to %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x port %d",
             ap[0],
             ap[1],
             ap[2],
             ap[3],
             ap[4],
             ap[5],
             ap[6],
             ap[7],
             ap[8],
             ap[9],
             ap[10],
             ap[11],
             ap[12],
             ap[13],
             ap[14],
             ap[15],
             io_context->sockaddr.mPort);
#ifdef VERBOSE_DEBUG_MESSAGES
        for (i = 0; i < message_length; i += 32) {
            bufp = buf;
            for (j = 0; bufp < buf + sizeof buf && i + j < message_length; j++) {
                *bufp++ = hexdigits[msg[i + j] >> 4];
                if (bufp < buf + sizeof buf) {
                    *bufp++ = hexdigits[msg[i + j] % 15];
                }
                if (bufp < buf + sizeof buf && (j & 1) == 1) {
                    *bufp++ = ' ';
                }
            }
            *bufp = 0;
            DEBUG(PUB_S_SRP, buf);
        }
#endif

        message = otUdpNewMessage(otThreadInstance, NULL);
        if (message == NULL) {
            ERROR("srp_send_datagram: otUdpNewMessage returned NULL");
            return kDNSServiceErr_NoMemory;
        }

        error = otMessageAppend(message, payload, message_length);
        if (error != OT_ERROR_NONE) {
            ERROR("srp_send_datagram: otMessageAppend returned %d", error);
            return kDNSServiceErr_NoMemory;
        }

        error = otUdpSend(otThreadInstance, &io_context->sock, message, &messageInfo);
        if (error != OT_ERROR_NONE) {
            ERROR("srp_send_datagram: otUdpSend returned %d", error);
            return kDNSServiceErr_Unknown;
        }
    }
    return err;
}

#define KEY_ID 1000
#define SERVER_ID 2000
int
srp_load_key_data(void *host_context, const char *key_name,
                  uint8_t *buffer, uint16_t *length, uint16_t buffer_size)
{

    OT_UNUSED_VARIABLE(host_context);
    OT_UNUSED_VARIABLE(key_name);
    OT_UNUSED_VARIABLE(buffer);
    OT_UNUSED_VARIABLE(length);
    OT_UNUSED_VARIABLE(buffer_size);

#if OT_PLAT_SETTINGS_ENABLED
    otError err;
    uint16_t rlength = buffer_size;
    // Note that at present we ignore the key name: we are only going to have one host key on an
    // accessory.
    err = otPlatSettingsGet(otThreadInstance, KEY_ID, 0, buffer, &rlength);
    if (err != OT_ERROR_NONE) {
        *length = 0;
        return kDNSServiceErr_NoSuchKey;
    }
    *length = rlength;
    return kDNSServiceErr_NoError;
#else
        return kDNSServiceErr_NoSuchKey;
#endif

}

int
srp_store_key_data(void *host_context, const char *name, uint8_t *buffer, uint16_t length)
{

    OT_UNUSED_VARIABLE(host_context);
    OT_UNUSED_VARIABLE(name);
    OT_UNUSED_VARIABLE(buffer);
    OT_UNUSED_VARIABLE(length);

#if OT_PLAT_SETTINGS_ENABLED

    otError err;
    err = otPlatSettingsAdd(otThreadInstance, KEY_ID, buffer, length);
    if (err != OT_ERROR_NONE) {
        ERROR("Unable to store key (length %d): %d", length, err);
        return kDNSServiceErr_Unknown;
    }
#endif
    return kDNSServiceErr_NoError;
}

int
srp_reset_key(const char *name, void *host_context)
{
    OT_UNUSED_VARIABLE(name);
    OT_UNUSED_VARIABLE(host_context);

#if OT_PLAT_SETTINGS_ENABLED
    otPlatSettingsDelete(otThreadInstance, KEY_ID, -1);
#endif
    return kDNSServiceErr_NoError;
}

typedef struct{
    uint16_t rrtype;
    uint8_t address[16];
    uint16_t port;
} SrpServerData;

bool
srp_get_last_server(uint16_t *NONNULL rrtype, uint8_t *NONNULL rdata, uint16_t rdlim,
                    uint8_t *NONNULL port, void *NULLABLE host_context)
{
    OT_UNUSED_VARIABLE(rrtype);
    OT_UNUSED_VARIABLE(rdata);
    OT_UNUSED_VARIABLE(rdlim);
    OT_UNUSED_VARIABLE(port);
    OT_UNUSED_VARIABLE(host_context);


#if OT_PLAT_SETTINGS_ENABLED

    SrpServerData data;
    uint16_t sizeFound = sizeof(data);

    otError err = otPlatSettingsGet(otThreadInstance, 
                                    SERVER_ID, 
                                    0, 
                                    (uint8_t*)(&data), 
                                    &sizeFound);
    if (err != OT_ERROR_NONE ) {        
        return false;
    }

    size_t addrlen = sizeof(data.address);
    if ( rdlim < addrlen)
    {
        addrlen = rdlim;
    }

    *rrtype = data.rrtype;
    memcpy(rdata, data.address, addrlen);
    *port = data.port;
    return true;
#else
    return false;
#endif
}


bool
srp_save_last_server(uint16_t rrtype, uint8_t *NONNULL rdata, uint16_t rdlength,
                     uint8_t *NONNULL port, void *NULLABLE host_context)
{
    OT_UNUSED_VARIABLE(rrtype);
    OT_UNUSED_VARIABLE(rdata);
    OT_UNUSED_VARIABLE(rdlength);
    OT_UNUSED_VARIABLE(port);
    OT_UNUSED_VARIABLE(host_context);

#if OT_PLAT_SETTINGS_ENABLED

    SrpServerData data;
    data.rrtype = rrtype;

    if ( rdlength > sizeof(data.address) )
    {
        return false;
    }
    memcpy( data.address, rdata, rdlength);
    data.port = *port;

    otError err = otPlatSettingsSet(
        otThreadInstance,
        SERVER_ID,
        (uint8_t*)(&data),
        sizeof(data));

    if (err != OT_ERROR_NONE) {
        ERROR("Unable to store server %d", err);
        return false;
    }
#endif
    return true;
}



void
register_callback(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
                  const char *name, const char *regtype, const char *domain, void *context)
{
    OT_UNUSED_VARIABLE(sdRef);
    OT_UNUSED_VARIABLE(flags);
    OT_UNUSED_VARIABLE(errorCode);
    OT_UNUSED_VARIABLE(name);
    OT_UNUSED_VARIABLE(regtype);
    OT_UNUSED_VARIABLE(domain);
    OT_UNUSED_VARIABLE(context);

    INFO("Register Reply: %ld " PRI_S_SRP " " PRI_S_SRP " " PRI_S_SRP "\n", errorCode, name == NULL ? "<NULL>" : name,
         regtype == NULL ? "<NULL>" : regtype, domain == NULL ? "<NULL>" : domain);
}

void
conflict_callback(const char *hostname)
{
    OT_UNUSED_VARIABLE(hostname);
    ERROR("Host name conflict: %s", hostname);
}

int
srp_thread_init(otInstance *instance)
{
//    uint32_t app_err;
    DEBUG("In srp_thread_init().");
    otThreadInstance = instance;
    srp_host_init(otThreadInstance);

    // TODO: Should be starting the timer here.

//    app_err = app_timer_create(&m_srp_timer, APP_TIMER_MODE_SINGLE_SHOT, wakeup_callback);
//    if (app_err != 0) {
//        ERROR("app_timer_create returned %lu", app_err);
//        return kDNSServiceErr_Unknown;
//    }
    return kDNSServiceErr_NoError;
}

int
srp_thread_shutdown(otInstance *instance)
{
    INFO("In srp_thread_shutdown().");

    OT_UNUSED_VARIABLE(instance);

    // TODO:  Should be stopping the timer here.

//    uint32_t app_err;
//    app_err = app_timer_stop(m_srp_timer);
//    if (app_err != 0) {
//        ERROR("app_timer_stop returned %lu", app_err);
//        return kDNSServiceErr_Unknown;
//    }
    return kDNSServiceErr_NoError;
}

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
