/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_command_list_immediate.hpp"
#include "vulkan_impl_type_convert.hpp"
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

#if VK_EXT_debug_utils
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
#endif

		{	VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
			create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so waiting on it when no commands where submitted succeeds

			if (vk.CreateFence(_device_impl->_orig, &create_info, nullptr, &_cmd_fences[i]) != VK_SUCCESS)
				return;
		}

		{	VkSemaphoreCreateInfo create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

			if (vk.CreateSemaphore(_device_impl->_orig, &create_info, nullptr, &_cmd_semaphores[i]) != VK_SUCCESS)
				return;
		}

#if VK_KHR_push_descriptor
		if (!vk.KHR_push_descriptor)
#endif
		{
			constexpr VkDescriptorPoolSize pool_sizes[] = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 128 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 512 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128 }
			};

			VkDescriptorPoolCreateInfo create_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			create_info.maxSets = 32;
			create_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
			create_info.pPoolSizes = pool_sizes;

			if (vk.CreateDescriptorPool(_device_impl->_orig, &create_info, nullptr, &_transient_descriptor_pool[i]) != VK_SUCCESS)
			{
				log::message(log::level::error, "Failed to create transient descriptor pool!");
			}
		}
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

	for (VkDescriptorPool pool : _transient_descriptor_pool)
		vk.DestroyDescriptorPool(_device_impl->_orig, pool, nullptr);

	for (VkFence fence : _cmd_fences)
		vk.DestroyFence(_device_impl->_orig, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		vk.DestroySemaphore(_device_impl->_orig, semaphore, nullptr);

	vk.FreeCommandBuffers(_device_impl->_orig, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	vk.DestroyCommandPool(_device_impl->_orig, _cmd_pool, nullptr);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_orig = VK_NULL_HANDLE;
}

void reshade::vulkan::command_list_immediate_impl::push_descriptors(api::shader_stage stages, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update)
{
#if VK_KHR_push_descriptor
	if (vk.KHR_push_descriptor)
	{
		command_list_impl::push_descriptors(stages, layout, layout_param, update);
		return;
	}
#endif

	if (update.count == 0)
		return;

	assert(update.table == 0);

	VkWriteDescriptorSet write { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstBinding = update.binding;
	write.dstArrayElement = update.array_offset;
	write.descriptorCount = update.count;
	write.descriptorType = convert_descriptor_type(update.type);

	temp_mem<VkDescriptorImageInfo> image_info(update.count);
	switch (update.type)
	{
	case api::descriptor_type::sampler:
		write.pImageInfo = image_info.p;
		for (uint32_t k = 0; k < update.count; ++k)
		{
			const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[k];
			image_info[k].sampler = (VkSampler)descriptor.handle;
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		write.pImageInfo = image_info.p;
		for (uint32_t k = 0; k < update.count; ++k)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[k];
			image_info[k].sampler = (VkSampler)descriptor.sampler.handle;
			image_info[k].imageView = (VkImageView)descriptor.view.handle;
			image_info[k].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		break;
	case api::descriptor_type::texture_shader_resource_view:
	case api::descriptor_type::texture_unordered_access_view:
		write.pImageInfo = image_info.p;
		for (uint32_t k = 0; k < update.count; ++k)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[k];
			image_info[k].imageView = (VkImageView)descriptor.handle;
			image_info[k].imageLayout = (update.type == api::descriptor_type::texture_unordered_access_view) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		break;
	case api::descriptor_type::buffer_shader_resource_view:
	case api::descriptor_type::buffer_unordered_access_view:
		write.pTexelBufferView = reinterpret_cast<const VkBufferView *>(update.descriptors);
		break;
	case api::descriptor_type::constant_buffer:
	case api::descriptor_type::shader_storage_buffer:
		write.pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(update.descriptors);
		break;
	default:
		assert(false);
		break;
	}

	assert(update.binding == 0 && update.array_offset == 0);

	VkDescriptorSetLayout set_layout = (VkDescriptorSetLayout)_device_impl->get_private_data_for_object<VK_OBJECT_TYPE_PIPELINE_LAYOUT>((VkPipelineLayout)layout.handle)->set_layouts[layout_param];

	VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	alloc_info.descriptorPool = _transient_descriptor_pool[_cmd_index];
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &set_layout;

	if (vk.AllocateDescriptorSets(_device_impl->_orig, &alloc_info, &write.dstSet) != VK_SUCCESS)
	{
		log::message(log::level::error, "Failed to allocate %u transient descriptor handle(s) of type %u!", static_cast<unsigned int>(update.type), update.count);
		return;
	}

	vk.UpdateDescriptorSets(_device_impl->_orig, 1, &write, 0, nullptr);

	bind_descriptor_tables(stages, layout, layout_param, 1, reinterpret_cast<const api::descriptor_table *>(&write.dstSet));
}

void reshade::vulkan::command_list_immediate_impl::update_buffer_region(const void *data, api::resource dest, uint64_t dest_offset, uint64_t size)
{
	s_last_immediate_command_list = this;

	command_list_impl::update_buffer_region(data, dest, dest_offset, size);
}
void reshade::vulkan::command_list_immediate_impl::update_texture_region(const api::subresource_data &data, api::resource dest, uint32_t dest_subresource, const api::subresource_box *dest_box)
{
	s_last_immediate_command_list = this;

	_device_impl->update_texture_region(data, dest, dest_subresource, dest_box);
}

bool reshade::vulkan::command_list_immediate_impl::flush(VkSubmitInfo *wait_semaphore_info)
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
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_orig;

	if (wait_semaphore_info != nullptr)
	{
		submit_info.waitSemaphoreCount = wait_semaphore_info->waitSemaphoreCount;
		submit_info.pWaitSemaphores = wait_semaphore_info->pWaitSemaphores;
		submit_info.pWaitDstStageMask = wait_semaphore_info->pWaitDstStageMask;

		if (wait_semaphore_info->waitSemaphoreCount != 0 ||
			wait_semaphore_info->signalSemaphoreCount != 0) // Handle case where this is called from 'command_queue_impl::signal'
		{
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &_cmd_semaphores[_cmd_index];
		}
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

	if (wait_semaphore_info != nullptr)
	{
		// This queue submit now waits on the requested wait semaphores
		// The next queue submit should therefore wait on the semaphore that was signaled by this submit
		wait_semaphore_info->waitSemaphoreCount = submit_info.signalSemaphoreCount;
		wait_semaphore_info->pWaitSemaphores = submit_info.pSignalSemaphores;
		assert(wait_semaphore_info->pWaitDstStageMask != nullptr || wait_semaphore_info->waitSemaphoreCount == 0);

		// Continue with next command buffer now that the current one was submitted
		_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;
	}

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (vk.GetFenceStatus(_device_impl->_orig, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Advance transient descriptor pool
#if VK_KHR_push_descriptor
	if (!vk.KHR_push_descriptor)
#endif
	{
		const VkDescriptorPool next_pool = _transient_descriptor_pool[_cmd_index];
		vk.ResetDescriptorPool(_device_impl->_orig, next_pool, 0);
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
