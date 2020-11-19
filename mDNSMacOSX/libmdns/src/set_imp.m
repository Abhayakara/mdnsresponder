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

#import "set_imp.h"

#import <CoreUtils/CoreUtils.h>
#import <Foundation/Foundation.h>
#import "mdns_strict.h"

//======================================================================================================================
// MARK: - Local Prototypes

static NSMutableSet *
_mdns_set_imp_to_nsmutableset(mdns_set_imp_t set);

//======================================================================================================================
// MARK: - Set Imp Public Methods

mdns_set_imp_t
mdns_set_imp_create(const uint32_t initial_capacity)
{
	@autoreleasepool {
		NSMutableSet *nsset;
		if (initial_capacity > 0) {
			nsset = [[NSMutableSet alloc] initWithCapacity:initial_capacity];
			require_return_value(nsset, NULL);
		} else {
			nsset = [[NSMutableSet alloc] init];
			require_return_value(nsset, NULL);
		}
		const mdns_set_imp_t set = (mdns_set_imp_t)CFBridgingRetain(nsset);
		ForgetObjectiveCObject(&nsset);
		return set;
	}
}

//======================================================================================================================

void
mdns_set_imp_release(const mdns_set_imp_t me)
{
	@autoreleasepool {
		NSMutableSet *nsset = (NSMutableSet *)CFBridgingTransfer(me);
		ForgetObjectiveCObject(&nsset);
	}
}

//======================================================================================================================

void
mdns_set_imp_add(const mdns_set_imp_t me, const mdns_object_t object)
{
	@autoreleasepool {
		[_mdns_set_imp_to_nsmutableset(me) addObject:object];
	}
}

//======================================================================================================================

void
mdns_set_imp_remove(const mdns_set_imp_t me, const mdns_object_t object)
{
	@autoreleasepool {
		[_mdns_set_imp_to_nsmutableset(me) removeObject:object];
	}
}

//======================================================================================================================

size_t
mdns_set_imp_get_count(const mdns_set_imp_t me)
{
	@autoreleasepool {
		return [_mdns_set_imp_to_nsmutableset(me) count];
	}
}

//======================================================================================================================

void
mdns_set_imp_iterate(const mdns_set_imp_t me, const mdns_set_applier_t applier)
{
	@autoreleasepool {
		NSMutableSet * const set = _mdns_set_imp_to_nsmutableset(me);
		for (mdns_object_t object in set) {
			const bool stop = applier(object);
			if (stop) {
				break;
			}
		}
	}
}

//======================================================================================================================
// MARK: - Set Imp Private Methods

static NSMutableSet *
_mdns_set_imp_to_nsmutableset(const mdns_set_imp_t me)
{
	return (__bridge NSMutableSet *)me;
}
