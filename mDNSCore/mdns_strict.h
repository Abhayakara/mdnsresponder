/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2020 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MDNS_STRICT_H__
#define __MDNS_STRICT_H__

#ifndef MDNS_NO_STRICT
#define MDNS_NO_STRICT				0
#endif

#define APPLE_OSX_mDNSResponder		0

#ifndef DEBUG
#define DEBUG 0
#endif


#if !MDNS_NO_STRICT && (TARGET_OS_OSX || TARGET_OS_IPHONE || TARGET_OS_SIMULATOR)

#include <CoreFoundation/CoreFoundation.h>
#include <os/log.h>

#ifndef MDNS_ALLOW_GOTO
#define MDNS_ALLOW_GOTO 1
#endif

#ifdef BlockForget
#undef BlockForget
#if( COMPILER_ARC )
	#define	BlockForget( X )			do { *(X) = nil; } while( 0 )
#else
	#define	BlockForget( X )			ForgetCustom( X, _Block_release )	// Bypass poisoned Block_release
#endif
#endif

#include "../mDNSMacOSX/strict.h"

#pragma mark -- Alloc --

#define mdns_malloc strict_malloc
#define MDNS_MALLOC_TYPE STRICT_MALLOC_TYPE
#define mdns_calloc strict_calloc
#define MDNS_CALLOC_TYPE STRICT_CALLOC_TYPE
#define mdns_reallocf strict_reallocf
#define MDNS_REALLOCF_TYPE STRICT_REALLOCF_TYPE
#define mdns_memalign strict_memalign
#define MDNS_ALLOC_ALIGN_TYPE STRICT_ALLOC_ALIGN_TYPE
#define mdns_strdup strict_strdup

#pragma mark -- Dispose --

#define MDNS_DISPOSE_XPC STRICT_DISPOSE_XPC
#define MDNS_DISPOSE_XPC_PROPERTY(obj, prop) MDNS_DISPOSE_XPC(obj->prop)

#define MDNS_DISPOSE_ALLOCATED STRICT_DISPOSE_ALLOCATED
#define MDNS_DISPOSE_ALLOCATED_PROPERTY(obj, prop) MDNS_DISPOSE_ALLOCATED(obj->prop)
#define mdns_free(ptr) MDNS_DISPOSE_ALLOCATED(ptr)

#define MDNS_DISPOSE_DISPATCH STRICT_DISPOSE_DISPATCH
#define MDNS_DISPOSE_DISPATCH_PROPERTY(obj, prop) MDNS_DISPOSE_DISPATCH(obj->prop)

#define MDNS_RESET_BLOCK STRICT_RESET_BLOCK
#define MDNS_RESET_BLOCK_PROPERTY(obj, prop, new_block) MDNS_RESET_BLOCK(obj->prop, new_block)

#define MDNS_DISPOSE_BLOCK STRICT_DISPOSE_BLOCK
#define MDNS_DISPOSE_BLOCK_PROPERTY(obj, prop) MDNS_DISPOSE_BLOCK(obj->prop)

#define MDNS_DISPOSE_CF_OBJECT STRICT_DISPOSE_CF_OBJECT
#define MDNS_DISPOSE_CF_PROPERTY(obj, prop) MDNS_DISPOSE_CF_OBJECT(obj->prop)

#define MDNS_DISPOSE_ADDRINFO STRICT_DISPOSE_ADDRINFO

#pragma mark -- Poison --

#if !defined(BUILD_TEXT_BASED_API) || BUILD_TEXT_BASED_API == 0

#if !MDNS_ALLOW_GOTO

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#define goto _Pragma("GCC error \"goto is forbidden - https://xkcd.com/292 \"")
#pragma clang diagnostic pop

#endif // !MDNS_ALLOW_GOTO

#endif // defined(BUILD_TEXT_BASED_API) && BUILD_TEXT_BASED_API

#else  // !MDNS_NO_STRICT && (TARGET_OS_OSX || TARGET_OS_IPHONE || TARGET_OS_SIMULATOR)

#define _MDNS_STRICT_DISPOSE_TEMPLATE(ptr, function) \
	do {                                        \
		if ((ptr) != NULL) {                    \
			function(ptr);                      \
			(ptr) = NULL;                       \
		}                                       \
	} while(0)

#define mdns_malloc 			malloc
#define mdns_calloc 			calloc
#define mdns_strdup 			strdup
#define mdns_free(obj) \
	_MDNS_STRICT_DISPOSE_TEMPLATE(obj, free)


#endif // !MDNS_NO_STRICT &&(TARGET_OS_OSX || TARGET_OS_IPHONE || TARGET_OS_SIMULATOR)

#endif // __MDNS_STRICT_H__

