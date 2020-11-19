/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tcpinfo.h"

#include "helpers.h"

#include <CoreUtils/CoreUtils.h>
#include <os/log.h>
#include <sys/sysctl.h>
#include "mdns_strict.h"

//======================================================================================================================
// MARK: - Local Protoypes

static OSStatus
_mdns_tcpinfo_get_info(struct info_tuple *itpl, struct tcp_info *out_info);

//======================================================================================================================
// MARK: - Logging

MDNS_LOG_CATEGORY_DEFINE(tcpinfo, "tcpinfo");

//======================================================================================================================
// MARK: - External Functions

OSStatus
mdns_tcpinfo_get_ipv4(const uint32_t local_addr, const uint16_t local_port, const uint32_t remote_addr,
	const uint16_t remote_port, struct tcp_info * const out_info)
{
	struct info_tuple itpl;
	memset(&itpl, 0, sizeof(itpl));

	struct sockaddr_in * const local = &itpl.itpl_local_sin;
	SIN_LEN_SET(local);
	local->sin_family		= AF_INET;
	local->sin_port			= htons(local_port);
	local->sin_addr.s_addr	= htonl(local_addr);

	struct sockaddr_in * const remote = &itpl.itpl_remote_sin;
	SIN_LEN_SET(remote);
	remote->sin_family		= AF_INET;
	remote->sin_port		= htons(remote_port);
	remote->sin_addr.s_addr	= htonl(remote_addr);

	const OSStatus err = _mdns_tcpinfo_get_info(&itpl, out_info);
	os_log_with_type(_mdns_tcpinfo_log(), err ? OS_LOG_TYPE_ERROR : OS_LOG_TYPE_INFO,
		"TCP info get -- local: %{network:in_addr}d:%d, remote: %{network:in_addr}d:%d, error: %{mdns:err}ld",
		local->sin_addr.s_addr, ntohs(local->sin_port), remote->sin_addr.s_addr, ntohs(remote->sin_port), (long)err);
	return err;
}

//======================================================================================================================

OSStatus
mdns_tcpinfo_get_ipv6(const uint8_t local_addr[static 16], const uint16_t local_port,
	const uint8_t remote_addr[static 16], const uint16_t remote_port, struct tcp_info * const out_info)
{
	struct info_tuple itpl;
	memset(&itpl, 0, sizeof(itpl));

	struct sockaddr_in6 * const local = &itpl.itpl_local_sin6;
	SIN6_LEN_SET(local);
	local->sin6_family	= AF_INET6;
	local->sin6_port	= htons(local_port);
	memcpy(local->sin6_addr.s6_addr, local_addr, 16);

	struct sockaddr_in6 * const remote = &itpl.itpl_remote_sin6;
	SIN6_LEN_SET(remote);
	remote->sin6_family	= AF_INET6;
	remote->sin6_port	= htons(remote_port);
	memcpy(remote->sin6_addr.s6_addr, remote_addr, 16);

	const OSStatus err = _mdns_tcpinfo_get_info(&itpl, out_info);
	os_log_with_type(_mdns_tcpinfo_log(), err ? OS_LOG_TYPE_ERROR : OS_LOG_TYPE_INFO,
		"TCP info get -- local: %{network:in6_addr}.16P.%d, remote: %{network:in6_addr}.16P.%d, error: %{mdns:err}ld",
		local->sin6_addr.s6_addr, ntohs(local->sin6_port), remote->sin6_addr.s6_addr, ntohs(remote->sin6_port),
		(long)err);
	return err;
}

//======================================================================================================================
// MARK: - Internal Functions

static OSStatus
_mdns_tcpinfo_get_info(struct info_tuple * const itpl, struct tcp_info * const out_info)
{
	itpl->itpl_proto = IPPROTO_TCP;
	struct tcp_info ti;
	memset(&ti, 0, sizeof(ti));
	size_t ti_len = sizeof(ti);
	OSStatus err = sysctlbyname("net.inet.tcp.info", &ti, &ti_len, itpl, sizeof(*itpl));
	err = map_global_value_errno(err != -1, err);
	require_return_value(!err, err);

	if (out_info) {
		memcpy(out_info, &ti, sizeof(ti));
	}
	return err;
}
