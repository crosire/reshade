/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <cassert>
#include <xr_generated_dispatch_table_core.h>

template <typename T>
static const T *find_in_structure_chain(const void *structure_chain, XrStructureType type)
{
	const T *next = reinterpret_cast<const T *>(structure_chain);
	while (next != nullptr && next->type != type)
		next = reinterpret_cast<const T *>(next->next);
	return next;
}

struct openxr_instance
{
	XrInstance handle;
	XrGeneratedDispatchTable dispatch_table;
};

#define RESHADE_OPENXR_GET_DISPATCH_PTR(name) \
	PFN_xr##name trampoline = g_openxr_instances.at(instance).dispatch_table.name; \
	assert(trampoline != nullptr)
#define RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(name, data) \
	assert((data) != nullptr); \
	PFN_xr##name trampoline = (data)->name; \
	assert(trampoline != nullptr)
