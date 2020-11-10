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

#include "set.h"

#include "helpers.h"
#include "objects.h"
#include "set_imp.h"

#include <CoreUtils/CoreUtils.h>
#include "mdns_strict.h"

//======================================================================================================================
// MARK: - Set Kind Definition

struct mdns_set_s {
	struct mdns_object_s	base;	// Object base.
	mdns_set_imp_t			imp;	// Underlying set implementation object.
};

MDNS_OBJECT_SUBKIND_DEFINE(set);

//======================================================================================================================
// MARK: - Set Public Methods

mdns_set_t
mdns_set_create(const uint32_t initial_capacity)
{
	mdns_set_t set = NULL;
	mdns_set_t obj = _mdns_set_alloc();
	require_quiet(obj, exit);

	obj->imp = mdns_set_imp_create(initial_capacity);
	require_quiet(obj->imp, exit);

	set = obj;
	obj = NULL;

exit:
	mdns_forget(&obj);
	return set;
}

//======================================================================================================================

void
mdns_set_add(const mdns_set_t me, const mdns_object_t object)
{
	mdns_set_imp_add(me->imp, object);
}

//======================================================================================================================

void
mdns_set_remove(const mdns_set_t me, const mdns_object_t object)
{
	mdns_set_imp_remove(me->imp, object);
}

//======================================================================================================================

size_t
mdns_set_get_count(const mdns_set_t me)
{
	return mdns_set_imp_get_count(me->imp);
}

//======================================================================================================================

void
mdns_set_iterate(const mdns_set_t me, const mdns_set_applier_t applier)
{
	mdns_set_imp_iterate(me->imp, applier);
}

//======================================================================================================================
// MARK: - Set Private Methods

typedef struct _str_node_s	_str_node_t;
struct _str_node_s {
	_str_node_t *	next;	// Next node in list.
	char *			str;	// Allocated C string.
};

static _str_node_t *
_mdns_set_create_description_list(mdns_set_t set, bool privacy);

static void
_mdns_set_free_description_list(_str_node_t *list);
#define _mdns_set_forget_description_list(X)	ForgetCustom(X, _mdns_set_free_description_list)

static char *
_mdns_set_copy_description(const mdns_set_t me, const bool debug, const bool privacy)
{
	_str_node_t *list = _mdns_set_create_description_list(me, privacy);
	char *buffer = NULL;
	const char *lim = NULL;
	char *description = NULL;
	while (!description) {
		size_t len = 0;
		char *dst = buffer;
	#define _do_appendf(...)											\
		do {															\
			const int n = mdns_snprintf_add(&dst, lim, __VA_ARGS__);	\
			require_quiet(n >= 0, exit);								\
			len += (size_t)n;											\
		} while(0)
		if (debug) {
			_do_appendf("<%s: %p>: ", me->base.kind->name, me);
		}
		_do_appendf("{");
		for (const _str_node_t *node = list; node; node = node->next) {
			_do_appendf("\n\t%s", node->str ? node->str : "<NO DESC.>");
		}
		_do_appendf("\n}");
	#undef _do_appendf

		if (!buffer) {
			++len;
			buffer = (char *)mdns_malloc(len);
			require_quiet(buffer, exit);
			lim = &buffer[len];
			*buffer = '\0';
		} else {
			description = buffer;
			buffer = NULL;
		}
	}

exit:
	ForgetMem(&buffer);
	_mdns_set_forget_description_list(&list);
	return description;
}

static _str_node_t *
_mdns_set_create_description_list(const mdns_set_t me, const bool privacy)
{
	_str_node_t *list = NULL;
	__block _str_node_t **ptr = &list;
	mdns_set_imp_iterate(me->imp,
	^ bool (mdns_object_t _Nonnull object)
	{
		_str_node_t * const node = (_str_node_t *)mdns_calloc(1, sizeof(*node));
		require_return_value(node, true);
		node->str = mdns_object_copy_description(object, false, privacy);
		*ptr = node;
		ptr = &node->next;
		return false;
	});
	return list;
}

static void
_mdns_set_free_description_list(_str_node_t *list)
{
	for (_str_node_t *node = list; node; node = list) {
		list = node->next;
		ForgetMem(&node->str);
		ForgetMem(&node);
	}
}

//======================================================================================================================

#define _mdns_set_imp_forget(X)	ForgetCustom(X, mdns_set_imp_release)

static void
_mdns_set_finalize(const mdns_set_t me)
{
	_mdns_set_imp_forget(&me->imp);
}
