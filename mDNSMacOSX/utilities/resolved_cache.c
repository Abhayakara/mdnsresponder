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

#include "resolved_cache.h"
#include "symptoms.h"

#include <CoreUtils/DebugServices.h>
#include <bsm/libbsm.h>
#include <sys/queue.h>

#include "mdns_strict.h"

//======================================================================================================================
// MARK: - resolved_cache Kind Definition

struct _cache_item_s {
	SLIST_ENTRY(_cache_item_s)		entries;				// Cache entries
	uintptr_t						id;						// Id (key) for this cache
	CFMutableArrayRef				names;					// Name cache
	CFMutableArrayRef				addrs;					// Addr cache
} ;

static SLIST_HEAD(, _cache_item_s)	s_head = SLIST_HEAD_INITIALIZER(s_head);

//======================================================================================================================
// MARK: - resolved_cache cache_item Private Functions

static void
_cache_item_add(struct _cache_item_s *item)
{
	SLIST_INSERT_HEAD(&s_head, item, entries);
}

//======================================================================================================================

static void
_cache_item_remove(struct _cache_item_s *item)
{
	if (item == SLIST_FIRST(&s_head)) {
		SLIST_REMOVE_HEAD(&s_head, entries);
	} else {
		SLIST_REMOVE(&s_head, item, _cache_item_s, entries);
	}
}

//======================================================================================================================

static void
_cache_item_remove_and_free(struct _cache_item_s *item)
{
	_cache_item_remove(item);
	ForgetCF(&item->names);
	ForgetCF(&item->addrs);
	ForgetMem(&item);
}

//======================================================================================================================

static void
_cache_item_report(struct _cache_item_s *item)
{
	size_t count = CFArrayGetCount(item->addrs);
	require_quiet(count, exit);

	DNSQuestion *q = (DNSQuestion *)item->id;
	require_quiet(q, exit);

	if (audit_token_to_pid(q->delegateAuditToken) != 0) {
		mdns_symptoms_report_resolved_delegated_audit_token(item->names, item->addrs, q->peerAuditToken,
			q->inAppBrowserRequest, q->request_id, &q->delegateAuditToken);
	} else if (q->pid) {
		pid_t delegate_pid = (audit_token_to_pid(q->peerAuditToken) == q->pid) ? 0 : q->pid;
		if (delegate_pid) {
			mdns_symptoms_report_resolved_delegated_pid(item->names, item->addrs, q->peerAuditToken,
				q->inAppBrowserRequest, q->request_id, delegate_pid);
		} else {
			mdns_symptoms_report_resolved(item->names, item->addrs, q->peerAuditToken, q->inAppBrowserRequest,
				q->request_id);
		}
	} else {
		mdns_symptoms_report_resolved_delegated_uuid(item->names, item->addrs, q->peerAuditToken,
			q->inAppBrowserRequest, q->request_id, q->uuid);
	}

	// Remove addrs after reported
	CFArrayRemoveAllValues(item->addrs);

exit:
	return;
}

//======================================================================================================================
// MARK: - resolved_cache cache_id Private Functions

static struct _cache_item_s *
_cache_item_create(const uintptr_t cache_id)
{
	struct _cache_item_s *cache = NULL;
	struct _cache_item_s *obj = (struct _cache_item_s *)mdns_calloc(1, sizeof(*obj));
	require_quiet(obj, exit);

	obj->names = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	require_quiet(obj->names, exit);

	obj->addrs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	require_quiet(obj->addrs, exit);

	obj->id = cache_id;
	cache = obj;
	obj = NULL;

exit:
	if (obj) {
		ForgetCF(&obj->names);
		ForgetCF(&obj->addrs);
		ForgetMem(&obj);
	}
	return cache;
}

//======================================================================================================================

static struct _cache_item_s *
_cache_item_find(uintptr_t item_id, bool create)
{
	struct _cache_item_s *item = NULL;
	struct _cache_item_s *next;
	SLIST_FOREACH(next, &s_head, entries) {
		if (next->id == item_id) {
			item = next;
			break;
		}
	}
	if (!item && create) {
		struct _cache_item_s *new_item = _cache_item_create(item_id);
		require_quiet(new_item, exit);
		item = new_item;
		_cache_item_add(item);
	}
exit:
	return item;
}

//======================================================================================================================
// MARK: - resolved_cache Public Functions

void
resolved_cache_append_name(uintptr_t item_id, const domainname * const name)
{
	CFStringRef str = NULL;
	struct _cache_item_s *item = _cache_item_find(item_id, true);
	require_quiet(item, exit);

	char name_str[MAX_ESCAPED_DOMAIN_NAME];
	ConvertDomainNameToCString(name, name_str);
	str = CFStringCreateWithCString(kCFAllocatorDefault, name_str, kCFStringEncodingUTF8);
	require_quiet(str, exit);

	CFIndex index = CFArrayGetFirstIndexOfValue(item->names, CFRangeMake(0, CFArrayGetCount(item->names)), str);
	require_quiet(index == kCFNotFound, exit);
	CFArrayAppendValue(item->names, str);

exit:
	ForgetCF(&str);
	return;
}

//======================================================================================================================

void
resolved_cache_append_address(uintptr_t cache_id, DNS_TypeValues type, const void * const data)
{
	struct _cache_item_s *item = _cache_item_find(cache_id, false);
	require_quiet(item, exit);

	size_t	size = (type == kDNSType_A) ? 4 : 16;
	CFDataRef addr = CFDataCreate(kCFAllocatorDefault, data, size);
	require_quiet(addr, exit);

	CFArrayAppendValue(item->addrs, addr);
	ForgetCF(&addr);

exit:
	return;
}

//======================================================================================================================

void
resolved_cache_idle(void)
{
	struct _cache_item_s *next;
	SLIST_FOREACH(next, &s_head, entries) {
		_cache_item_report(next);
	}
}

//======================================================================================================================

void
resolved_cache_delete(uintptr_t cache_id)
{
	struct _cache_item_s *item = _cache_item_find(cache_id, false);
	require_quiet(item, exit);

	_cache_item_report(item);
	_cache_item_remove_and_free(item);

exit:
	return;
}
