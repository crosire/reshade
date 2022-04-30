/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dll_log.hpp"
#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"

#define vk _device_impl->_dispatch_table

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
	api_object_impl(queue), _device_impl(device), _queue_flags(queue_family.queueFlags)
{
	// Register queue to device (no need to lock, since all command queues are created single threaded in 'vkCreateDevice')
	_device_impl->_queues.push_back(this);

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue_family_index);
		// Ensure the immediate command list was initialized successfully, otherwise disable it
		if (_immediate_cmd_list->_orig == VK_NULL_HANDLE)
		{
			LOG(ERROR) << "Failed to create immediate command list for queue " << _orig << '!';

			delete _immediate_cmd_list;
			_immediate_cmd_list = nullptr;
		}
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_command_queue>(this);
#endif
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_command_queue>(this);
#endif

	delete _immediate_cmd_list;

	// Unregister queue from device
	_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), this));
}

reshade::api::device *reshade::vulkan::command_queue_impl::get_device()
{
	return _device_impl;
}

reshade::api::command_queue_type reshade::vulkan::command_queue_impl::get_type() const
{
	static_assert(
		api::command_queue_type::graphics == VK_QUEUE_GRAPHICS_BIT &&
		api::command_queue_type::compute == VK_QUEUE_COMPUTE_BIT &&
		api::command_queue_type::copy == VK_QUEUE_TRANSFER_BIT);

	return static_cast<api::command_queue_type>(_queue_flags);
}

void reshade::vulkan::command_queue_impl::wait_idle() const
{
	flush_immediate_command_list();

	vk.QueueWaitIdle(_orig);

#ifndef NDEBUG
	_device_impl->_wait_for_idle_happened = true;
#endif
}

void reshade::vulkan::command_queue_impl::flush_immediate_command_list() const
{
	uint32_t num_wait_semaphores = 0; // No semaphores to wait on
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig, nullptr, num_wait_semaphores);
}
void reshade::vulkan::command_queue_impl::flush_immediate_command_list(VkSemaphore *wait_semaphores, uint32_t &num_wait_semaphores) const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig, wait_semaphores, num_wait_semaphores);
}

void reshade::vulkan::command_queue_impl::begin_debug_event(const char *label, const float color[4])
{
	assert(label != nullptr);

	if (vk.QueueBeginDebugUtilsLabelEXT == nullptr)
		return;

	VkDebugUtilsLabelEXT label_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	label_info.pLabelName = label;

	// The optional color value is ignored if all elements are set to zero
	if (color != nullptr)
	{
		label_info.color[0] = color[0];
		label_info.color[1] = color[1];
		label_info.color[2] = color[2];
		label_info.color[3] = color[3];
	}

	vk.QueueBeginDebugUtilsLabelEXT(_orig, &label_info);
}
void reshade::vulkan::command_queue_impl::end_debug_event()
{
	if (vk.QueueEndDebugUtilsLabelEXT == nullptr)
		return;

	vk.QueueEndDebugUtilsLabelEXT(_orig);
}
void reshade::vulkan::command_queue_impl::insert_debug_marker(const char *label, const float color[4])
{
	assert(label != nullptr);

	if (vk.QueueInsertDebugUtilsLabelEXT == nullptr)
		return;

	VkDebugUtilsLabelEXT label_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
	label_info.pLabelName = label;

	if (color != nullptr)
	{
		label_info.color[0] = color[0];
		label_info.color[1] = color[1];
		label_info.color[2] = color[2];
		label_info.color[3] = color[3];
	}

	vk.QueueInsertDebugUtilsLabelEXT(_orig, &label_info);
}
