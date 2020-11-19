/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef MDNS_SYMPTOMS_H
#define MDNS_SYMPTOMS_H

#include <mdns/base.h>

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>	// audit_token_t
#include <sys/socket.h>

// Activity domains and labels for metrics collection.
// These are defined in "activity_registry.h" from libnetcore.
#define kDNSActivityDomain 33
#define kDNSActivityLabelUnicastAQuery 1
#define kDNSActivityLabelUnicastAAAAQuery 2
#define kDNSActivityLabelProvisioningRequest 3

MDNS_ASSUME_NONNULL_BEGIN

/*!
 *	@brief
 *		Reports an unresponsive DNS server.
 *
 *	@param address
 *		The address of the DNS server.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_unresponsive_server(const struct sockaddr *address);

/*!
 *	@brief
 *		Reports a responsive DNS server.
 *
 *	@param address
 *		The address of the DNS server.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_responsive_server(const struct sockaddr *address);

/*!
 *	@brief
 *		Reports that a domain name was resolved.
 *
 *	@param names
 *		The cname chain.
 *
 *	@param addrs
 *		The resolved addresses.
 *
 *	@param token
 *		The peer's audit token.
 *
 *	@param in_app_browser_request
 *		Specified whether or not the request is for an InAppBrowser.
 *
 *	@param request_id
 *		The request ID number.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_resolved(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id);

/*!
 *	@brief
 *		Reports that a domain name was resolved on behalf of a delegator (by PID).
 *
 *	@param names
 *		The cname chain.
 *
 *	@param addrs
 *		The resolved addresses.
 *
 *	@param token
 *		The client peer's audit token.
 *
 *	@param in_app_browser_request
 *		Specified whether or not the request is for an InAppBrowser.
 *
 *	@param request_id
 *		The request ID number.
 *
 *	@param delegate_pid
 *		The delegator's PID.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_resolved_delegated_pid(const CFArrayRef names, const CFArrayRef addrs,
	audit_token_t token, bool in_app_browser_request, uint32_t request_id, pid_t delegate_pid);

/*!
 *	@brief
 *		Reports that a domain name was resolved on behalf of a delegator (by UUID).
 *
 *	@param names
 *		The cname chain.
 *
 *	@param addrs
 *		The resolved addresses.
 *
 *	@param token
 *		The client peer's audit token.
 *
 *	@param in_app_browser_request
 *		Specified whether or not the request is for an InAppBrowser.
 *
 *	@param request_id
 *		The request ID number.
 *
 *	@param delegate_uuid
 *		The delegator's UUID.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_resolved_delegated_uuid(const CFArrayRef names, const CFArrayRef addrs,
	audit_token_t token, bool in_app_browser_request, uint32_t request_id, const uuid_t _Nonnull delegate_uuid);

/*!
 *	@brief
 *		Reports that a domain name was resolved on behalf of a delegator (by audit token).
 *
 *	@param names
 *		The cname chain.
 *
 *	@param addrs
 *		The resolved addresses.
 *
 *	@param token
 *		The client peer's audit token.
 *
 *	@param in_app_browser_request
 *		Specified whether or not the request is for an InAppBrowser.
 *
 *	@param request_id
 *		The request ID number.
 *
 *	@param delegate_token
 *		The delegator's audit token.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_resolved_delegated_audit_token(const CFArrayRef names, const CFArrayRef addrs,
	audit_token_t token, bool in_app_browser_request, uint32_t request_id, const audit_token_t *delegate_token);

/*!
 *	@brief
 *		Reports an encrypted DNS connection failure.
 *
 *	@param host
 *		The DNS service's hostname.
 */
MDNS_SPI_AVAILABLE_FALL_2021
void
mdns_symptoms_report_encrypted_dns_connection_failure(const char *host);

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_SYMPTOMS_H
