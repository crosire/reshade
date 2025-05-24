/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_list_immediate.hpp"
#include "dll_log.hpp"

#define vk _device_impl->_dispatch_table

thread_local reshade::vulkan::command_list_immediate_impl *reshade::vulkan::command_list_immediate_impl::s_last_immediate_command_list = nullptr;

reshade::vulkan::command_list_immediate_impl::command_list_immediate_impl(device_impl *device, uint32_t queue_family_index, VkQueue queue) :
	command_list_impl(device, VK_NULL_HANDLE),
	_parent_queue(queue)
{
	{	VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = queue_family_index;

		if (vk.CreateCommandPool(_device_impl->_orig, &create_info, nullptr, &_cmd_pool) != VK_SUCCESS)
			return;
	}

	{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = _cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = NUM_COMMAND_FRAMES;

		if (vk.AllocateCommandBuffers(_device_impl->_orig, &alloc_info, _cmd_buffers) != VK_SUCCESS)
			return;
	}

	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		// The validation layers expect the loader to have set the dispatch pointer, but this does not happen when calling down the layer chain from here, so fix it
		*reinterpret_cast<void **>(_cmd_buffers[i]) = *reinterpret_cast<void **>(device->_orig);

		if (vk.SetDebugUtilsObjectNameEXT != nullptr)
		{
			std::string debug_name = "ReShade immediate command list";
			debug_name += " (" + std::to_string(i) + ')';

			VkDebugUtilsObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
			name_info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
			name_info.objectHandle = (uint64_t)_cmd_buffers[i];
			name_info.pObjectName = debug_name.c_str();

			vk.SetDebugUtilsObjectNameEXT(_device_impl->_orig, &name_info);
		}

		VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so waiting on it when no commands where submitted succeeds

		if (vk.CreateFence(_device_impl->_orig, &create_info, nullptr, &_cmd_fences[i]) != VK_SUCCESS)
			return;

		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (vk.CreateSemaphore(_device_impl->_orig, &sem_create_info, nullptr, &_cmd_semaphores[i]) != VK_SUCCESS)
			return;
	}

	// Command buffer is in an invalid state after creation, so reset it to begin
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];

	s_last_immediate_command_list = this;
}
reshade::vulkan::command_list_immediate_impl::~command_list_immediate_impl()
{
	if (this == s_last_immediate_command_list)
		s_last_immediate_command_list = nullptr;

	for (VkFence fence : _cmd_fences)
		vk.DestroyFence(_device_impl->_orig, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		vk.DestroySemaphore(_device_impl->_orig, semaphore, nullptr);

	vk.FreeCommandBuffers(_device_impl->_orig, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	vk.DestroyCommandPool(_device_impl->_orig, _cmd_pool, nullptr);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_orig = VK_NULL_HANDLE;
}

bool reshade::vulkan::command_list_immediate_impl::flush(VkSubmitInfo &semaphore_info)
{
	s_last_immediate_command_list = this;

	if (!_has_commands)
		return true;
	_has_commands = false;

	assert(_orig != VK_NULL_HANDLE);

	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	// Submit all asynchronous commands in one batch to the current queue
	if (vk.EndCommandBuffer(_orig) != VK_SUCCESS)
	{
		log::message(log::level::error, "Failed to close immediate command list!");

		// Have to reset the command buffer when closing it was unsuccessful
		vk.BeginCommandBuffer(_orig, &begin_info);
		return false;
	}

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.waitSemaphoreCount = semaphore_info.waitSemaphoreCount;
	submit_info.pWaitSemaphores = semaphore_info.pWaitSemaphores;
	submit_info.pWaitDstStageMask = semaphore_info.pWaitDstStageMask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_orig;

	if (semaphore_info.waitSemaphoreCount != 0 ||
		semaphore_info.signalSemaphoreCount != 0)
	{
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &_cmd_semaphores[_cmd_index];
	}

	// Only reset fence before an actual submit which can signal it again
	vk.ResetFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index]);

	if (vk.QueueSubmit(_parent_queue, 1, &submit_info, _cmd_fences[_cmd_index]) != VK_SUCCESS)
	{
		log::message(log::level::error, "Failed to submit immediate command list!");

		// Have to reset the command buffer when submitting it was unsuccessful
		vk.BeginCommandBuffer(_orig, &begin_info);
		return false;
	}

	// This queue submit now waits on the requested wait semaphores
	// The next queue submit should therefore wait on the semaphore that was signaled by this submit
	semaphore_info.waitSemaphoreCount = submit_info.signalSemaphoreCount;
	semaphore_info.pWaitSemaphores = submit_info.pSignalSemaphores;
	static const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	semaphore_info.pWaitDstStageMask = &wait_stage;

	// Continue with next command buffer now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (vk.GetFenceStatus(_device_impl->_orig, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Command buffer is now ready for a reset
	if (vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
	{
		log::message(log::level::error, "Failed to reset immediate command list!");
		return false;
	}

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
	return true;
}
bool reshade::vulkan::command_list_immediate_impl::flush_and_wait()
{
	if (!_has_commands)
		return true;

	// Index is updated during flush below, so keep track of the current one to wait on
	const uint32_t cmd_index_to_wait_on = _cmd_index;

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	if (!flush(submit_info))
		return false;

	// Wait for the submitted work to finish and reset fence again for next use
	return vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[cmd_index_to_wait_on], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}
