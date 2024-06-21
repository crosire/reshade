/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <cassert>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

// Windows SDK headers define these, which breaks the dispatch table
#undef CreateEvent
#undef CreateSemaphore

#include <vk_layer_dispatch_table.h>

struct instance_dispatch_table : public VkLayerInstanceDispatchTable
{
	VkInstance instance;
	uint32_t api_version;
};

template <typename T>
inline T *find_layer_info(const void *structure_chain, VkStructureType type, VkLayerFunction function)
{
	T *next = reinterpret_cast<T *>(const_cast<void *>(structure_chain));
	while (next != nullptr && !(next->sType == type && next->function == function))
		next = reinterpret_cast<T *>(const_cast<void *>(next->pNext));
	return next;
}
template <typename T>
inline const T *find_in_structure_chain(const void *structure_chain, VkStructureType type)
{
	const T *next = reinterpret_cast<const T *>(structure_chain);
	while (next != nullptr && next->sType != type)
		next = reinterpret_cast<const T *>(next->pNext);
	return next;
}

inline void *const dispatch_key_from_handle(const void *dispatch_handle)
{
	assert(dispatch_handle != nullptr);

	// The Vulkan loader writes the dispatch table pointer right to the start of the object, so use that as a key for lookup
	// This ensures that all objects of a specific level (device or instance) will use the same dispatch table
	return *(void **)dispatch_handle;
}
