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
_mdns_symptoms_report_resolving_symptom(const char *domain, uint32_t cname_count, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid, const uuid_t _Nullable delegated_uuid,
	const audit_token_t * _Nullable delegated_token);

static void
_mdns_symptoms_report_dns_host_symptom(symptom_ident_t id, const char *host);

static bool
_mdns_symptoms_resolving_cache_should_post(uint32_t request_id, uint32_t cname_count, const char *domain, pid_t pid);

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
mdns_symptoms_report_resolving_symptom(const char *domain, uint32_t cname_count, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id)
{
	bool should_post = _mdns_symptoms_resolving_cache_should_post(request_id, cname_count, domain,
		audit_token_to_pid(token));
	require_quiet(should_post, exit);
	_mdns_symptoms_report_resolving_symptom(domain, cname_count, token, in_app_browser_request, request_id, 0, NULL,
		NULL);
exit:
	return;
}

//======================================================================================================================

void
mdns_symptoms_report_resolving_delegated_pid_symptom(const char *domain, uint32_t cname_count, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid)
{
	bool should_post = _mdns_symptoms_resolving_cache_should_post(request_id, cname_count, domain,
		audit_token_to_pid(token));
	require_quiet(should_post, exit);
	_mdns_symptoms_report_resolving_symptom(domain, cname_count, token, in_app_browser_request, request_id,
		delegated_pid, NULL, NULL);
exit:
	return;
}

//======================================================================================================================

void
mdns_symptoms_report_resolving_delegated_uuid_symptom(const char *domain, uint32_t cname_count, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, const uuid_t delegated_uuid)
{
	bool should_post = _mdns_symptoms_resolving_cache_should_post(request_id, cname_count, domain,
		audit_token_to_pid(token));
	require_quiet(should_post, exit);
	_mdns_symptoms_report_resolving_symptom(domain, cname_count, token, in_app_browser_request, request_id, 0,
		delegated_uuid, NULL);
exit:
	return;
}

//======================================================================================================================

void
mdns_symptoms_report_resolving_delegated_audit_token_symptom(const char *domain, uint32_t cname_count,
	audit_token_t token, bool in_app_browser_request, uint32_t request_id, const audit_token_t *delegate_token)
{
	bool should_post = _mdns_symptoms_resolving_cache_should_post(request_id, cname_count, domain,
		audit_token_to_pid(token));
	require_quiet(should_post, exit);
	_mdns_symptoms_report_resolving_symptom(domain, cname_count, token, in_app_browser_request, request_id, 0, NULL,
		delegate_token);
exit:
	return;
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

static void
_mdns_symptoms_report_resolving_symptom(const char *domain, uint32_t cname_count, audit_token_t token,
	bool in_app_browser_request, uint32_t request_id, pid_t delegated_pid, const uuid_t _Nullable delegated_uuid,
	const audit_token_t * _Nullable delegated_token)
{
#define DELEGATE_STR_MAX	64
	const symptom_framework_t reporter = _mdns_symptoms_get_reporter();
	require_quiet(reporter, exit);

	size_t len = strlen(domain);
	require_quiet(len, exit);

	const symptom_t symptom = soft_symptom_new(reporter, SYMPTOM_DNS_RESOLVING);
	soft_symptom_set_qualifier(symptom, (uint64_t)(cname_count > 0), 0);
	soft_symptom_set_qualifier(symptom, (uint64_t)request_id, 4);
	soft_symptom_set_additional_qualifier(symptom, 1, len, domain);
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
	soft_symptom_send(symptom);
	os_log_debug(_mdns_symptoms_log(), "[R%u] Report pid: %lld %s%{public}s (cnames %u)%{public}s", request_id,
		(long long)audit_token_to_pid(token), domain, in_app_browser_request ? " (browse)" : "", cname_count,
		log_delegate);

exit:
	return;
}

//======================================================================================================================
// MARK: - Resolving Cache

#define MDNS_SYMPTOMS_RESOLVING_CACHE_COUNT	20
#define _isupper_ascii(X)		(((X) >= 'A') && ((X) <= 'Z'))

//======================================================================================================================
// MARK: - resolving cache _r_cache Struct Definition

struct _r_cache_item_s {
	TAILQ_ENTRY(_r_cache_item_s)	entries;	// Cache entries
	uint32_t						id;			// Client request id
	uint32_t						count;		// Client cname referral count
	uint32_t						hash;		// Domain name hash
};
static TAILQ_HEAD(_r_cache_head_s, _r_cache_item_s) g_head = TAILQ_HEAD_INITIALIZER(g_head);

//======================================================================================================================
// MARK: - resolving cache Private Functions

static uint32_t
_mdns_symptoms_resolving_cache_domain_hash(const char * const _Nonnull domain)
{
	uint32_t sum = 0;
	const uint8_t *c;

	for (c = (const uint8_t*)domain; c[0] != 0 && c[1] != 0; c += 2) {
		sum += (uint32_t)((_isupper_ascii(c[0]) ? c[0] + 'a' - 'A' : c[0]) << 8) |
						  (_isupper_ascii(c[1]) ? c[1] + 'a' - 'A' : c[1]);
		sum = (sum << 3) | (sum >> 29);
	}
	if (c[0]) {
		sum += (uint32_t)((_isupper_ascii(c[0]) ? c[0] + 'a' - 'A' : c[0]) << 8);
	}
	return sum;
}

//======================================================================================================================

static struct _r_cache_item_s *
_mdns_symptoms_resolving_cache_item_find(uint32_t id, uint32_t count, uint32_t hash)
{
	struct _r_cache_item_s *item = NULL;
	struct _r_cache_item_s *next;
	TAILQ_FOREACH(next, &g_head, entries) {
		if (next->id 	== id 		&&
			next->count == count	&&
			next->hash 	== hash		) {
			item = next;
			break;
		}
	}
	return item;
}

//======================================================================================================================

static void
_mdns_symptoms_resolving_cache_prepare(void)
{
	static dispatch_once_t 			s_once = 0;
	static struct _r_cache_item_s	s_items[MDNS_SYMPTOMS_RESOLVING_CACHE_COUNT] = {0};
	dispatch_once(&s_once,
	^{
		for (size_t i = 0; i < countof(s_items); ++i) {
			struct _r_cache_item_s * const item = &s_items[i];
			TAILQ_INSERT_TAIL(&g_head, item, entries);
		}
	});
}

//======================================================================================================================
// MARK: - resolving cache Internal Functions

static bool
_mdns_symptoms_resolving_cache_should_post(uint32_t request_id, uint32_t cname_count, const char * const _Nonnull domain, pid_t pid)
{
	bool result = false;
	require_quiet(domain[0] != '\0', exit);

	_mdns_symptoms_resolving_cache_prepare();

	uint32_t hash = _mdns_symptoms_resolving_cache_domain_hash(domain);
	struct _r_cache_item_s *item = _mdns_symptoms_resolving_cache_item_find(request_id, cname_count, hash);
	if (item) {
		os_log_debug(_mdns_symptoms_log(), "[R%u] Ignore pid: %lld %s (cnames %u)", request_id, (long long)pid, domain,
			cname_count);
	}
	require_quiet(!item, exit);

	item = TAILQ_LAST(&g_head, _r_cache_head_s);
	item->id 	= request_id;
	item->count = cname_count;
	item->hash 	= hash;
	TAILQ_REMOVE(&g_head, item, entries);
	TAILQ_INSERT_HEAD(&g_head, item, entries);
	result = true;

exit:
	return result;
}
