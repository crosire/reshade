/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cassert>
#include <vulkan/vulkan.h>

// Windows SDK headers define these, which breaks the dispatch table
#undef CreateEvent
#undef CreateSemaphore

#include <vk_layer_dispatch_table.h>

template <VkObjectType type>
struct vk_handle_traits;
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_IMAGE>
{
	using T = VkImage;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.DestroyImage(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_IMAGE_VIEW>
{
	using T = VkImageView;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.DestroyImageView(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_BUFFER>
{
	using T = VkBuffer;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.DestroyBuffer(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_BUFFER_VIEW>
{
	using T = VkBufferView;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.DestroyBufferView(device, obj, nullptr); }
};

template <>
struct vk_handle_traits<VK_OBJECT_TYPE_SHADER_MODULE>
{
	using T = VkShaderModule;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.DestroyShaderModule(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_DEVICE_MEMORY>
{
	using T = VkDeviceMemory;
	static inline void destroy(VkDevice device, const VkLayerDispatchTable &vk, T obj) { vk.FreeMemory(device, obj, nullptr); }
};

template <VkObjectType type>
struct vk_handle
{
	using T = typename vk_handle_traits<type>::T;

	vk_handle(VkDevice device, const VkLayerDispatchTable &table)
		: _object(VK_NULL_HANDLE), _device(device), _dtable(&table) {}
	vk_handle(VkDevice device, const VkLayerDispatchTable &table, T object)
		: _object(object), _device(device), _dtable(&table) {}
	vk_handle(const vk_handle &other) = delete;
	vk_handle(vk_handle &&other) { operator=(std::move(other)); }
	~vk_handle() {
		vk_handle_traits<type>::destroy(_device, *_dtable, _object);
	}

	operator T() const { return _object; }

	/// <summary>
	/// Returns the stored object and releases ownership without freeing it.
	/// </summary>
	T release()
	{
		const T object = _object;
		_object = VK_NULL_HANDLE;
		return object;
	}

	// This should only be called on uninitialized objects, e.g. when passed into creation functions.
	T *operator&() { assert(_object == VK_NULL_HANDLE); return &_object; }

	vk_handle &operator=(T object)
	{
		_object = object;
		return *this;
	}
	vk_handle &operator=(const vk_handle &) = delete;
	vk_handle &operator=(vk_handle &&move)
	{
		_object = move._object;
		_device = move._device;
		_dtable = move._dtable;
		move._object = VK_NULL_HANDLE;
		return *this;
	}

	bool operator==(const vk_handle &rhs) const { return _object == rhs._object; }
	bool operator!=(const vk_handle &rhs) const { return _object != rhs._object; }

	// Default operator used for sorting
	friend bool operator< (const vk_handle &lhs, const vk_handle &rhs) { return lhs._object < rhs._object; }

private:
	T _object;
	VkDevice _device;
	const VkLayerDispatchTable *_dtable;
};
