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

#ifndef MDNS_TICKS_H
#define MDNS_TICKS_H

#include <mdns/base.h>

#include <stdint.h>

MDNS_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

/*!
 *	@brief
 *		Returns the number of Mach time API ticks equal to one second.
 */
MDNS_SPI_AVAILABLE_FALL_2021
uint64_t
mdns_mach_ticks_per_second(void);

__END_DECLS

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_TICKS_H
