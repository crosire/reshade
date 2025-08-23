/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_queue.hpp"
#include "dll_log.hpp"
#include <algorithm> // std::find

#define vk _device_impl->_dispatch_table

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
	api_object_impl(queue),
	_device_impl(device),
	_queue_family_props(queue_family)
{
	// Register queue to device (no need to lock, since all command queues are created single threaded in 'vkCreateDevice')
	_device_impl->_queues.push_back(this);

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue_family_index, queue);
		// Ensure the immediate command list was initialized successfully, otherwise disable it
		if (_immediate_cmd_list->_orig == VK_NULL_HANDLE)
		{
			log::message(log::level::error, "Failed to create immediate command list for queue %p!", _orig);

			delete _immediate_cmd_list;
			_immediate_cmd_list = nullptr;
		}
	}
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
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

	return static_cast<api::command_queue_type>(_queue_family_props.queueFlags);
}

void reshade::vulkan::command_queue_impl::wait_idle() const
{
	flush_immediate_command_list();

	vk.QueueWaitIdle(_orig);
}

void reshade::vulkan::command_queue_impl::flush_immediate_command_list() const
{
	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	flush_immediate_command_list(submit_info);
}
void reshade::vulkan::command_queue_impl::flush_immediate_command_list(VkSubmitInfo &semaphore_info) const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(semaphore_info);
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

bool reshade::vulkan::command_queue_impl::wait(api::fence fence, uint64_t value)
{
	const VkSemaphore wait_semaphore = (VkSemaphore)fence.handle;

	VkTimelineSemaphoreSubmitInfo wait_semaphore_info { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	wait_semaphore_info.waitSemaphoreValueCount = 1;
	wait_semaphore_info.pWaitSemaphoreValues = &value;

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO, &wait_semaphore_info };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &wait_semaphore;
	static const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	submit_info.pWaitDstStageMask = &wait_stage;

	return vk.QueueSubmit(_orig, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS;
}
bool reshade::vulkan::command_queue_impl::signal(api::fence fence, uint64_t value)
{
	const VkSemaphore signal_semaphore = (VkSemaphore)fence.handle;

	VkTimelineSemaphoreSubmitInfo signal_semaphore_info { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	signal_semaphore_info.signalSemaphoreValueCount = 1;
	signal_semaphore_info.pSignalSemaphoreValues = &value;

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO, &signal_semaphore_info };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &signal_semaphore;

	flush_immediate_command_list(submit_info);

	return vk.QueueSubmit(_orig, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS;
}

uint64_t reshade::vulkan::command_queue_impl::get_timestamp_frequency() const
{
	if (_queue_family_props.timestampValidBits == 0)
		return 0;

	VkPhysicalDeviceProperties device_props = {};
	vk.GetPhysicalDeviceProperties(_device_impl->_physical_device, &device_props);

	return static_cast<uint64_t>(1000000000ull / device_props.limits.timestampPeriod);
}
