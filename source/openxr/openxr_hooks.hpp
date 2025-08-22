/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <xr_generated_dispatch_table_core.h>

struct openxr_dispatch_table : public XrGeneratedDispatchTable
{
	XrInstance instance;
};

template <typename T>
static const T *find_in_structure_chain(const void *structure_chain, XrStructureType type)
{
	const T *next = reinterpret_cast<const T *>(structure_chain);
	while (next != nullptr && next->type != type)
		next = reinterpret_cast<const T *>(next->next);
	return next;
}

#define RESHADE_OPENXR_GET_DISPATCH_PTR(name) \
	PFN_xr##name trampoline = g_openxr_instances.at(instance).name; \
	assert(trampoline != nullptr)
#define RESHADE_OPENXR_GET_DISPATCH_PTR_FROM(name, data) \
	assert((data) != nullptr); \
	PFN_xr##name trampoline = (data)->name; \
	assert(trampoline != nullptr)

#define RESHADE_OPENXR_INIT_DISPATCH_PTR(name) \
	reinterpret_cast<PFN_xr##name>(get_instance_proc(instance, "xr" #name, reinterpret_cast<PFN_xrVoidFunction *>(&dispatch_table.name)))
