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

#ifndef MDNS_TLV_H
#define MDNS_TLV_H

#include <mdns/base.h>

#include <MacTypes.h>
#include <stdint.h>

#if defined(MDNS_TLV_EXCLUDE_AVAILABILITY) && MDNS_TLV_EXCLUDE_AVAILABILITY
	#undef MDNS_SPI_AVAILABLE
	#define MDNS_SPI_AVAILABLE(X)
#endif

MDNS_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

/*!
 *	@brief
 *		Gets the value of the first TLV of a desired type in a series of TLVs.
 *
 *	@param start
 *		The start of the series of TLVs.
 *
 *	@param end
 *		The end of the series of TLVs.
 *
 *	@param type
 *		The desired type.
 *
 *	@param out_length
 *		Address of a variable to set to the TLV's length if such a TLV is found.
 *
 *	@param out_value
 *		Address of a variable to set to the TLV's value if such a TLV is found.
 *
 *	@param out_ptr
 *		Address of a variable to set to the end of the TLV if such a TLV is found.
 *
 *	@result
 *		kNoErr if such a TLV is found, kNotFoundErr if such a TLV is not found, or some other error code.
 */
MDNS_SPI_AVAILABLE(FALL_2021)
OSStatus
mdns_tlv16_get_value(const uint8_t *start, const uint8_t *end, uint16_t type, size_t * _Nullable out_length,
	const uint8_t * _Null_unspecified * _Nullable out_value, const uint8_t * _Null_unspecified * _Nullable out_ptr);

/*!
 *	@brief
 *		Writes a TLV with a specified type, length, and value.
 *
 *	@param dst
 *		The write destination of the TLV.
 *
 *	@param limit
 *		The limit of the destination.
 *
 *	@param type
 *		The type.
 *
 *	@param length
 *		The length.
 *
 *	@param value
 *		Address of the value.
 *
 *	@param out_ptr
 *		Address of a variable to set to the end of the TLV if the TLV was successfully written.
 *
 *	@result
 *		kNoErr if the TLV was successfully written, kNoSpaceErr if there wasn't enough space, or some other
 *		error code.
 */
MDNS_SPI_AVAILABLE(FALL_2021)
OSStatus
mdns_tlv16_set(uint8_t *dst, const uint8_t * _Nullable limit, uint16_t type, uint16_t length, const uint8_t *value,
	uint8_t * _Nonnull * _Nullable out_ptr);

/*!
 *	@brief
 *		Calculates the number of bytes required for a TLV with a given value length.
 *
 *	@param value_length
 *		The value length.
 */
MDNS_SPI_AVAILABLE(FALL_2021)
size_t
mdns_tlv16_get_required_length(const uint16_t value_length);

__END_DECLS

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_TLV_H
