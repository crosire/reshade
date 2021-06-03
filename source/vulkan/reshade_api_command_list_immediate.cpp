/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_command_list_immediate.hpp"

#define vk _device_impl->_dispatch_table

reshade::vulkan::command_list_immediate_impl::command_list_immediate_impl(device_impl *device, uint32_t queue_family_index) :
	command_list_impl(device, VK_NULL_HANDLE)
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
}
reshade::vulkan::command_list_immediate_impl::~command_list_immediate_impl()
{
	for (VkFence fence : _cmd_fences)
		vk.DestroyFence(_device_impl->_orig, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		vk.DestroySemaphore(_device_impl->_orig, semaphore, nullptr);

	vk.FreeCommandBuffers(_device_impl->_orig, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	vk.DestroyCommandPool(_device_impl->_orig, _cmd_pool, nullptr);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::vulkan::command_list_immediate_impl::flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores)
{
	if (!_has_commands)
		return true;
	_has_commands = false;

	assert(_orig != VK_NULL_HANDLE);

	// Submit all asynchronous commands in one batch to the current queue
	if (vk.EndCommandBuffer(_orig) != VK_SUCCESS)
	{
		LOG(ERROR) << "Failed to close immediate command list!";

		// Have to reset the command buffer when closing it was unsuccessfull
		VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], 0);
		return false;
	}

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_orig;

	std::vector<VkPipelineStageFlags> wait_stages(wait_semaphores.size(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	if (!wait_semaphores.empty())
	{
		submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
		submit_info.pWaitSemaphores = wait_semaphores.data();
		submit_info.pWaitDstStageMask = wait_stages.data();
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &_cmd_semaphores[_cmd_index];
	}

	// Only reset fence before an actual submit which can signal it again
	vk.ResetFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index]);

	if (vk.QueueSubmit(queue, 1, &submit_info, _cmd_fences[_cmd_index]) != VK_SUCCESS)
		return false;

	// Only signal and wait on a semaphore if the submit this flush is executed in originally did
	if (!wait_semaphores.empty())
	{
		// This queue submit now waits on the requested wait semaphores
		// The next queue submit should therefore wait on the semaphore that was signaled by this submit
		wait_semaphores.clear();
		wait_semaphores.push_back(_cmd_semaphores[_cmd_index]);
	}

	// Continue with next command buffer now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (vk.GetFenceStatus(_device_impl->_orig, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Command buffer is now ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return false;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
	return true;
}
bool reshade::vulkan::command_list_immediate_impl::flush_and_wait(VkQueue queue)
{
	if (!_has_commands)
		return true;

	// Index is updated during flush below, so keep track of the current one to wait on
	const uint32_t cmd_index_to_wait_on = _cmd_index;

	std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
	if (!flush(queue, wait_semaphores))
		return false;

	// Wait for the submitted work to finish and reset fence again for next use
	return vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[cmd_index_to_wait_on], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}
