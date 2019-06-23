/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <assert.h>
#include <vulkan/vulkan.h>

template <typename T>
using vk_free_func_t = void(VKAPI_CALL *)(VkDevice, T, VkAllocationCallbacks const*);

template <typename T>
struct vk_handle_traits
{
	static const vk_free_func_t<T> free_func;
};

const vk_free_func_t<VkImage> vk_handle_traits<VkImage>::free_func = vkDestroyImage;
const vk_free_func_t<VkBuffer> vk_handle_traits<VkBuffer>::free_func = vkDestroyBuffer;
const vk_free_func_t<VkImageView> vk_handle_traits<VkImageView>::free_func = vkDestroyImageView;
const vk_free_func_t<VkShaderModule> vk_handle_traits<VkShaderModule>::free_func = vkDestroyShaderModule;
const vk_free_func_t<VkDeviceMemory> vk_handle_traits<VkDeviceMemory>::free_func = vkFreeMemory;

template <typename T>
struct vk_handle
{
	explicit vk_handle(VkDevice device)
		: _object(VK_NULL_HANDLE), _device(device) {}
	vk_handle(VkDevice device, T object)
		: _object(object), _device(device) {}
	vk_handle(const vk_handle &other) = delete;
	vk_handle(vk_handle &&other) { operator=(other); }
	~vk_handle()
	{
		vk_handle_traits<T>::free_func(_device, _object, nullptr);
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

	vk_handle<T> &operator=(const vk_handle<T> &) = delete;
	vk_handle<T> &operator=(vk_handle<T> &&move)
	{
		_object = move._object;
		_device = move._device;
		move._object = VK_NULL_HANDLE;
		return *this;
	}

	bool operator==(T rhs) const { return _object == rhs; }
	bool operator==(const vk_handle<T> &rhs) const { return _object == rhs._object; }
	friend bool operator==(T lhs, const vk_handle<T> &rhs) { return rhs.operator==(lhs); }
	bool operator!=(T rhs) const { return _object != rhs; }
	bool operator!=(const vk_handle<T> &rhs) const { return _object != rhs._object; }
	friend bool operator!=(T lhs, const vk_handle<T> &rhs) { return rhs.operator!=(lhs); }

	// Default operator used for sorting
	friend bool operator< (const vk_handle<T> &lhs, const vk_handle<T> &rhs) { return lhs._object < rhs._object; }

private:
	T _object;
	VkDevice _device;
};
