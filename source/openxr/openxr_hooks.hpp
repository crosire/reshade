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
