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

#ifndef MDNS_POWERLOG_H
#define MDNS_POWERLOG_H

#include <mdns/base.h>

#include <stdint.h>
#include <sys/types.h>

MDNS_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

/*!
 *	@brief
 *		Logs the start of an mDNS browse operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being queried for.
 *
 *	@param record_type
 *		The type of the resource record being queried for. This is usually the PTR resource record type.
 *
 *	@param client_pid
 *		The PID of the client that requested the browse operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_browse_start(const uint8_t *record_name, int record_type, pid_t client_pid);

/*!
 *	@brief
 *		Logs the end of an mDNS browse operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being queried for.
 *
 *	@param record_type
 *		The type of the resource record being queried for. This is usually the PTR resource record type.
 *
 *	@param client_pid
 *		The PID of the client that requested the browse operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_browse_stop(const uint8_t *record_name, int record_type, pid_t client_pid);

/*!
 *	@brief
 *		Logs the start of an mDNS advertise operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being advertised.
 *
 *	@param record_type
 *		The type of the resource record being advertised.
 *
 *	@param client_pid
 *		The PID of the client that requested the advertise operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_advertise_start(const uint8_t *record_name, int record_type, pid_t client_pid);

/*!
 *	@brief
 *		Logs the end of an mDNS advertise operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being advertised.
 *
 *	@param record_type
 *		The type of the resource record being advertised.
 *
 *	@param client_pid
 *		The PID of the client that requested the advertise operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_advertise_stop(const uint8_t *record_name, int record_type, pid_t client_pid);

/*!
 *	@brief
 *		Logs the start of an mDNS resolve operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being queried for.
 *
 *	@param record_type
 *		The type of the resource record being queried for.
 *
 *	@param client_pid
 *		The PID of the client that requested the resolve operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_resolve_start(const uint8_t *record_name, int record_type, pid_t client_pid);

/*!
 *	@brief
 *		Logs the end of an mDNS resolve operation via AWDL.
 *
 *	@param record_name
 *		The name of the resource record being queried for.
 *
 *	@param record_type
 *		The type of the resource record being queried for.
 *
 *	@param client_pid
 *		The PID of the client that requested the resolve operation.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_powerlog_awdl_resolve_stop(const uint8_t *record_name, int record_type, pid_t client_pid);

__END_DECLS

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_POWERLOG_H
