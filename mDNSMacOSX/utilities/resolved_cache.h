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

#ifndef __RESOLVED_CACHE_H__
#define __RESOLVED_CACHE_H__

#include "ClientRequests.h"

OS_ASSUME_NONNULL_BEGIN

//======================================================================================================================
// MARK: - query_resolved_cache functions

void
resolved_cache_append_name(const uintptr_t cache_id, const domainname * const name);

void
resolved_cache_append_address(const uintptr_t cache_id, DNS_TypeValues type, const void * const data);

void
resolved_cache_idle(void);

void
resolved_cache_delete(const uintptr_t cache_id);

OS_ASSUME_NONNULL_END

#endif	// __RESOLVED_CACHE_H__
