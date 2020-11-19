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

#ifndef MDNS_TCPINFO_H
#define MDNS_TCPINFO_H

#include <mdns/base.h>

#include <MacTypes.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>

MDNS_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

/*!
 *	@brief
 *		Gets the current TCP info for an IPv4 TCP connection.
 *
 *	@param local_addr
 *		The TCP connection's local IPv4 address in host byte order.
 *
 *	@param local_port
 *		The TCP connecton's local port number in host byte order.
 *
 *	@param remote_addr
 *		The TCP connection's remote IPv4 address in host byte order.
 *
 *	@param remote_port
 *		The TCP connecton's remote port number in host byte order.
 *
 *	@param out_info
 *		Address of a TCP info data structure to fill with the TCP connection's TCP info.
 *
 *	@result
 *		kNoErr if the TCP info was successfully gotten. Otherwise, an error code.
 */
MDNS_SPI_AVAILABLE_FALL_2021
OSStatus
mdns_tcpinfo_get_ipv4(uint32_t local_addr, uint16_t local_port, uint32_t remote_addr, uint16_t remote_port,
	struct tcp_info * _Nullable out_info);

/*!
 *	@brief
 *		Gets the current TCP info for an IPv6 TCP connection.
 *
 *	@param local_addr
 *		The TCP connection's local IPv6 address.
 *
 *	@param local_port
 *		The TCP connecton's local port number in host byte order.
 *
 *	@param remote_addr
 *		The TCP connection's remote IPv6 address.
 *
 *	@param remote_port
 *		The TCP connecton's remote port number in host byte order.
 *
 *	@param out_info
 *		Address of a TCP info data structure to fill with the TCP connection's TCP info.
 *
 *	@result
 *		kNoErr if the TCP info was successfully gotten. Otherwise, an error code.
 */
MDNS_SPI_AVAILABLE_FALL_2021
OSStatus
mdns_tcpinfo_get_ipv6(const uint8_t local_addr[static 16], uint16_t local_port, const uint8_t remote_addr[static 16],
	uint16_t remote_port, struct tcp_info * _Nullable out_info);

__END_DECLS

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_TCPINFO_H
