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

#include "ticks.h"

#include "helpers.h"

#include <CoreUtils/CoreUtils.h>
#include "mdns_strict.h"

//======================================================================================================================
// MARK: - Internals

MDNS_LOG_CATEGORY_DEFINE(ticks, "ticks");

//======================================================================================================================

uint64_t
mdns_mach_ticks_per_second(void)
{
	static dispatch_once_t	s_once = 0;
	static uint64_t			s_ticks_per_second = 0;
	dispatch_once(&s_once,
	^{
		mach_timebase_info_data_t info;
		const kern_return_t err = mach_timebase_info(&info);
		if (!err && (info.numer != 0) && (info.denom != 0)) {
			s_ticks_per_second = (info.denom * UINT64_C_safe(kNanosecondsPerSecond)) / info.numer;
		} else {
			os_log_error(_mdns_ticks_log(),
				"Unexpected results from mach_timebase_info: err %d numer %u denom %u", err, info.numer, info.denom);
			s_ticks_per_second = kNanosecondsPerSecond;
		}
	});
	return s_ticks_per_second;
}
