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

#include "symptoms.h"
#include "helpers.h"

#include <CoreUtils/CoreUtils.h>
#include <SymptomReporter/SymptomReporter.h>
#include <bsm/libbsm.h>

#include "mdns_strict.h"

#define MAX_DOMAIN_NAME 256

#define SYMPTOM_REPORTER_mDNSResponder_NUMERIC_ID	101
#define SYMPTOM_REPORTER_mDNSResponder_TEXT_ID		"com.apple.mDNSResponder"

#define SYMPTOM_DNS_NO_REPLIES			 			0x00065001
#define SYMPTOM_DNS_RESUMED_RESPONDING	 			0x00065002
#define SYMPTOM_DNS_RESOLVING            			0x00065003
#define SYMPTOM_DNS_ENCRYPTED_CONNECTION_FAILURE 	0x00065004

//======================================================================================================================
// MARK: - Soft Linking

SOFT_LINK_FRAMEWORK_EX(PrivateFrameworks, SymptomReporter);
#if COMPILER_CLANG
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wbad-function-cast"
#endif
SOFT_LINK_FUNCTION_EX(SymptomReporter, symptom_framework_init,
	symptom_framework_t,
	(symptom_ident_t id, const char *originator_string),
	(id, originator_string));
SOFT_LINK_FUNCTION_EX(SymptomReporter, symptom_new,
	symptom_t,
	(symptom_framework_t framework, symptom_ident_t id),
	(framework, id));
SOFT_LINK_FUNCTION_EX(SymptomReporter, symptom_set_additional_qualifier,
	int,
	(symptom_t symptom, uint32_t qualifier_type, size_t qualifier_len, const void *qualifier_data),
	(symptom, qualifier_type, qualifier_len, qualifier_data));
SOFT_LINK_FUNCTION_EX(SymptomReporter, symptom_set_qualifier,
	int,
	(symptom_t symptom, uint64_t qualifier, uint32_t qualifier_index),
	(symptom, qualifier, qualifier_index));
SOFT_LINK_FUNCTION_EX(SymptomReporter, symptom_send,
	int,
	(symptom_t symptom),
	(symptom));
#if COMPILER_CLANG
	#pragma clang diagnostic pop
#endif

MDNS_LOG_CATEGORY_DEFINE(symptoms, "symptoms");

//======================================================================================================================
// MARK: - Local Helper Prototypes

static symptom_framework_t
_mdns_symptoms_get_reporter(void);

static void
_mdns_symptoms_report_dns_server_symptom(symptom_ident_t id, const struct sockaddr *address);

static void
_mdns_symptoms_report_resolved(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid, const uuid_t _Nullable delegated_uuid,
	const audit_token_t * _Nullable delegated_token);

static void
_mdns_symptoms_report_dns_host_symptom(symptom_ident_t id, const char *host);

//======================================================================================================================
// MARK - External Functions

void
mdns_symptoms_report_unresponsive_server(const struct sockaddr *address)
{
	_mdns_symptoms_report_dns_server_symptom(SYMPTOM_DNS_NO_REPLIES, address);
}

//======================================================================================================================

void
mdns_symptoms_report_encrypted_dns_connection_failure(const char *host)
{
	_mdns_symptoms_report_dns_host_symptom(SYMPTOM_DNS_ENCRYPTED_CONNECTION_FAILURE, host);
}

//======================================================================================================================

void
mdns_symptoms_report_responsive_server(const struct sockaddr *address)
{
	_mdns_symptoms_report_dns_server_symptom(SYMPTOM_DNS_RESUMED_RESPONDING, address);
}

//======================================================================================================================

void
mdns_symptoms_report_resolved(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id)
{
	_mdns_symptoms_report_resolved(names, addrs, token, in_app_browser_request, request_id, 0, NULL,
		NULL);
}

//======================================================================================================================

void
mdns_symptoms_report_resolved_delegated_pid(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid)
{
	_mdns_symptoms_report_resolved(names, addrs, token, in_app_browser_request, request_id,
		delegated_pid, NULL, NULL);
}

//======================================================================================================================

void
mdns_symptoms_report_resolved_delegated_uuid(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, const uuid_t delegated_uuid)
{
	_mdns_symptoms_report_resolved(names, addrs, token, in_app_browser_request, request_id, 0,
		delegated_uuid, NULL);
}

//======================================================================================================================

void
mdns_symptoms_report_resolved_delegated_audit_token(const CFArrayRef names, const CFArrayRef addrs,
	audit_token_t token, bool in_app_browser_request, uint32_t request_id, const audit_token_t *delegate_token)
{
	_mdns_symptoms_report_resolved(names, addrs, token, in_app_browser_request, request_id, 0, NULL,
		delegate_token);
}

//======================================================================================================================
// MARK - Local Helpers

static symptom_framework_t
_mdns_symptoms_get_reporter(void)
{
	static dispatch_once_t		s_once		= 0;
	static symptom_framework_t	s_reporter	= NULL;

	dispatch_once(&s_once,
	^{
		if (SOFT_LINK_HAS_FUNCTION(SymptomReporter, symptom_framework_init)) {
			s_reporter = soft_symptom_framework_init(SYMPTOM_REPORTER_mDNSResponder_NUMERIC_ID,
				SYMPTOM_REPORTER_mDNSResponder_TEXT_ID);
		}
	});
	return s_reporter;
}

//======================================================================================================================

static void
_mdns_symptoms_report_dns_server_symptom(symptom_ident_t id, const struct sockaddr *address)
{
	const symptom_framework_t reporter = _mdns_symptoms_get_reporter();
	require_quiet(reporter, exit);

	size_t address_len;
	if (address->sa_family == AF_INET) {
		address_len = sizeof(struct sockaddr_in);
	} else if (address->sa_family == AF_INET6) {
		address_len = sizeof(struct sockaddr_in6);
	} else {
		goto exit;
	}
	const symptom_t symptom = soft_symptom_new(reporter, id);
	soft_symptom_set_additional_qualifier(symptom, 1, address_len, address);
	soft_symptom_send(symptom);

exit:
	return;
}

//======================================================================================================================

static void
_mdns_symptoms_report_dns_host_symptom(symptom_ident_t id, const char *host)
{
	const symptom_framework_t reporter = _mdns_symptoms_get_reporter();
	require_quiet(reporter, exit);

	size_t hostname_len = strnlen(host, MAX_DOMAIN_NAME);
	const symptom_t symptom = soft_symptom_new(reporter, id);
	soft_symptom_set_additional_qualifier(symptom, 2, hostname_len, host);
	soft_symptom_send(symptom);

exit:
	return;
}

//======================================================================================================================

static CFDataRef
_mdns_symptoms_create_deep_copy_data(const CFArrayRef array)
{
	CFDataRef data = NULL;
	CFPropertyListRef plist;

	plist = CFPropertyListCreateDeepCopy(kCFAllocatorDefault, array, kCFPropertyListImmutable);
	require_quiet(plist, exit);

	data = CFPropertyListCreateData(kCFAllocatorDefault, plist, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	ForgetCF(&plist);

exit:
	return data;
}

//======================================================================================================================

static void
_mdns_symptoms_report_resolved(const CFArrayRef names, const CFArrayRef addrs, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid, const uuid_t _Nullable delegated_uuid,
	const audit_token_t * _Nullable delegated_token)
{
#define DELEGATE_STR_MAX	64
	CFDataRef name_data = NULL;
	CFDataRef addr_data = NULL;

	const symptom_framework_t reporter = _mdns_symptoms_get_reporter();
	require_quiet(reporter, exit);

	const CFIndex name_count = CFArrayGetCount(names);
	require_quiet(name_count >= 1, exit);

	const CFIndex addr_count = CFArrayGetCount(addrs);
	require_quiet(addr_count >= 1, exit);

	name_data = _mdns_symptoms_create_deep_copy_data(names);
	require_quiet(name_data, exit);

	addr_data = _mdns_symptoms_create_deep_copy_data(addrs);
	require_quiet(addr_data, exit);

	const symptom_t symptom = soft_symptom_new(reporter, SYMPTOM_DNS_RESOLVING);
	soft_symptom_set_qualifier(symptom, (uint64_t)request_id, 4);
	soft_symptom_set_additional_qualifier(symptom, 2, sizeof(audit_token_t), &token);
	if (in_app_browser_request) {
		soft_symptom_set_qualifier(symptom, (uint64_t)1, 2);
	}
	char log_delegate[DELEGATE_STR_MAX] = "";
	if (delegated_token != NULL) {
		soft_symptom_set_additional_qualifier(symptom, 3, sizeof(audit_token_t), delegated_token);
		snprintf(log_delegate, DELEGATE_STR_MAX, " delegated token: %lld",
			(long long)audit_token_to_pid(*delegated_token));
	} else if (delegated_uuid != NULL) {
		soft_symptom_set_additional_qualifier(symptom, 4, sizeof(uuid_t), delegated_uuid);
		uuid_string_t uuid_str;
		uuid_unparse_lower(delegated_uuid, uuid_str);
		snprintf(log_delegate, DELEGATE_STR_MAX, " delegated uuid: %s", uuid_str);
	} else if (delegated_pid != 0) {
		soft_symptom_set_qualifier(symptom, (uint64_t)delegated_pid, 1);
		snprintf(log_delegate, DELEGATE_STR_MAX, " delegated pid: %lld", (long long)delegated_pid);
	}
	soft_symptom_set_additional_qualifier(symptom, 5, (size_t)CFDataGetLength(addr_data), CFDataGetBytePtr(addr_data));
	soft_symptom_set_additional_qualifier(symptom, 6, (size_t)CFDataGetLength(name_data), CFDataGetBytePtr(name_data));
	soft_symptom_send(symptom);
	os_log_debug(_mdns_symptoms_log(), "[R%u] Report pid: %lld %@%{public}s (cnames %ld) (addrs %ld)%{public}s", request_id,
		(long long)audit_token_to_pid(token), CFArrayGetValueAtIndex(names, 0),
		in_app_browser_request ? " (browse)" : "", (name_count - 1), addr_count, log_delegate);

exit:
	ForgetCF(&name_data);
	ForgetCF(&addr_data);
	return;
}
