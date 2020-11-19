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

#ifndef MDNS_SET_IMP_H
#define MDNS_SET_IMP_H

#include "set.h"

/*!
 *	@brief
 *		A pointer to an mdns_set_imp object, which implements the core functionality of an mdns_set object.
 */
typedef struct mdns_set_imp_s *	mdns_set_imp_t;

MDNS_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

/*!
 *	@brief
 *		Creates a set, i.e., a container for a collection of unordered unique objects.
 *
 *	@param initial_capacity
 *		The initial capacity of the set.
 *
 *	@result
 *		A new set object or NULL if there was a lack of resources.
 *
 *	@discussion
 *		This object is meant for implementing the core functionality of an mdns_set object.
 */
CF_RETURNS_RETAINED MDNS_WARN_RESULT
mdns_set_imp_t _Nullable
mdns_set_imp_create(uint32_t initial_capacity);

/*!
 *	@brief
 *		Releases a reference to a set.
 *
 *	@param set
 *		The set.
 */
void
mdns_set_imp_release(mdns_set_imp_t CF_RELEASES_ARGUMENT set);

/*!
 *	@brief
 *		Adds an object to a set if it isn't already a member of the set.
 *
 *	@param set
 *		The set.
 *
 *	@param object
 *		The object.
 */
void
mdns_set_imp_add(mdns_set_imp_t set, mdns_object_t object);

/*!
 *	@brief
 *		Removes an object from a set if it's currently a member of the set.
 *
 *	@param set
 *		The set.
 *
 *	@param object
 *		The object.
 */
void
mdns_set_imp_remove(mdns_set_imp_t set, mdns_object_t object);

/*!
 *	@brief
 *		Returns the number of member objects contained in a set.
 *
 *	@param set
 *		The set.
 */
size_t
mdns_set_imp_get_count(mdns_set_imp_t set);

/*!
 *	@brief
 *		Iterates over each member object of a set.
 *
 *	@param set
 *		The set.
 *
 *	@param applier
 *		Block to invoke for each member object.
 */
void
mdns_set_imp_iterate(mdns_set_imp_t set, mdns_set_applier_t applier);

__END_DECLS

MDNS_ASSUME_NONNULL_END

#endif	// MDNS_SET_IMP_H
