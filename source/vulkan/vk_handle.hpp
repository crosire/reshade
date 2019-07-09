/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "vulkan.hpp"
#include <assert.h>

template <VkObjectType type>
struct vk_handle_traits;
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_IMAGE>
{
	using T = VkImage;
	static inline void destroy(VkDevice device, const vk_device_table &table, T obj) { table.vkDestroyImage(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_IMAGE_VIEW>
{
	using T = VkImageView;
	static inline void destroy(VkDevice device, const vk_device_table &table, T obj) { table.vkDestroyImageView(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_BUFFER>
{
	using T = VkBuffer;
	static inline void destroy(VkDevice device, const vk_device_table &table, T obj) { table.vkDestroyBuffer(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_SHADER_MODULE>
{
	using T = VkShaderModule;
	static inline void destroy(VkDevice device, const vk_device_table &table, T obj) { table.vkDestroyShaderModule(device, obj, nullptr); }
};
template <>
struct vk_handle_traits<VK_OBJECT_TYPE_DEVICE_MEMORY>
{
	using T = VkDeviceMemory;
	static inline void destroy(VkDevice device, const vk_device_table &table, T obj) { table.vkFreeMemory(device, obj, nullptr); }
};

template <VkObjectType type>
struct vk_handle
{
	using T = typename vk_handle_traits<type>::T;

	vk_handle(VkDevice device, const vk_device_table &table)
		: _object(VK_NULL_HANDLE), _device(device), _dtable(&table) {}
	vk_handle(VkDevice device, const vk_device_table &table, T object)
		: _object(object), _device(device), _dtable(&table) {}
	vk_handle(const vk_handle &other) = delete;
	vk_handle(vk_handle &&other) { operator=(other); }
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

	bool operator==(T rhs) const { return _object == rhs; }
	bool operator==(const vk_handle &rhs) const { return _object == rhs._object; }
	friend bool operator==(T lhs, const vk_handle &rhs) { return rhs.operator==(lhs); }
	bool operator!=(T rhs) const { return _object != rhs; }
	bool operator!=(const vk_handle &rhs) const { return _object != rhs._object; }
	friend bool operator!=(T lhs, const vk_handle &rhs) { return rhs.operator!=(lhs); }

	// Default operator used for sorting
	friend bool operator< (const vk_handle &lhs, const vk_handle &rhs) { return lhs._object < rhs._object; }

private:
	T _object;
	VkDevice _device;
	const vk_device_table *_dtable;
};
