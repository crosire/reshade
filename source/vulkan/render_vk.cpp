/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"
#include "format_utils.hpp"

#define vk _dispatch_table

reshade::vulkan::device_impl::device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table) :
	api_object_impl(device), _physical_device(physical_device), _dispatch_table(device_table), _instance_dispatch_table(instance_table)
{
	{	VmaVulkanFunctions functions;
		functions.vkGetPhysicalDeviceProperties = instance_table.GetPhysicalDeviceProperties;
		functions.vkGetPhysicalDeviceMemoryProperties = instance_table.GetPhysicalDeviceMemoryProperties;
		functions.vkAllocateMemory = device_table.AllocateMemory;
		functions.vkFreeMemory = device_table.FreeMemory;
		functions.vkMapMemory = device_table.MapMemory;
		functions.vkUnmapMemory = device_table.UnmapMemory;
		functions.vkFlushMappedMemoryRanges = device_table.FlushMappedMemoryRanges;
		functions.vkInvalidateMappedMemoryRanges = device_table.InvalidateMappedMemoryRanges;
		functions.vkBindBufferMemory = device_table.BindBufferMemory;
		functions.vkBindImageMemory = device_table.BindImageMemory;
		functions.vkGetBufferMemoryRequirements = device_table.GetBufferMemoryRequirements;
		functions.vkGetImageMemoryRequirements = device_table.GetImageMemoryRequirements;
		functions.vkCreateBuffer = device_table.CreateBuffer;
		functions.vkDestroyBuffer = device_table.DestroyBuffer;
		functions.vkCreateImage = device_table.CreateImage;
		functions.vkDestroyImage = device_table.DestroyImage;
		functions.vkCmdCopyBuffer = device_table.CmdCopyBuffer;
		functions.vkGetBufferMemoryRequirements2KHR = device_table.GetBufferMemoryRequirements2;
		functions.vkGetImageMemoryRequirements2KHR = device_table.GetImageMemoryRequirements2;
		functions.vkBindBufferMemory2KHR = device_table.BindBufferMemory2;
		functions.vkBindImageMemory2KHR = device_table.BindImageMemory2;
		functions.vkGetPhysicalDeviceMemoryProperties2KHR = instance_table.GetPhysicalDeviceMemoryProperties2;

		VmaAllocatorCreateInfo create_info = {};
		// The runtime runs in a single thread, so no synchronization necessary
		create_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
		create_info.physicalDevice = physical_device;
		create_info.device = device;
		create_info.preferredLargeHeapBlockSize = 1920 * 1080 * 4 * 16; // Allocate blocks of memory that can comfortably contain 16 Full HD images
		create_info.pVulkanFunctions = &functions;
		create_info.vulkanApiVersion = VK_API_VERSION_1_1; // Vulkan 1.1 is guaranteed by code in vulkan_hooks_instance.cpp

		vmaCreateAllocator(&create_info, &_alloc);
	}

#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<reshade::addon_event::init_device>(this);
}
reshade::vulkan::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed at this point

	invoke_addon_event<reshade::addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif

	vmaDestroyAllocator(_alloc);
}

bool reshade::vulkan::device_impl::check_format_support(uint32_t format, api::resource_usage usage) const
{
	VkImageUsageFlags image_flags = 0;
	convert_usage_to_image_usage_flags(usage, image_flags);

	VkImageFormatProperties props;
	return _instance_dispatch_table.GetPhysicalDeviceImageFormatProperties(_physical_device, static_cast<VkFormat>(format), VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, image_flags, 0, &props) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::check_resource_handle_valid(api::resource_handle resource) const
{
	if (resource.handle == 0)
		return false;
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
		return data.image == (VkImage)resource.handle;
	else
		return data.buffer == (VkBuffer)resource.handle;
}
bool reshade::vulkan::device_impl::check_resource_view_handle_valid(api::resource_view_handle view) const
{
	if (view.handle == 0)
		return false;
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		return data.image_view == (VkImageView)view.handle;
	else
		return data.buffer_view == (VkBufferView)view.handle;
}

bool reshade::vulkan::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource_handle *out)
{
	if (initial_data != nullptr)
		return false;

	assert((desc.usage & initial_state) == initial_state);

	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	switch (desc.heap)
	{
	default:
	case api::memory_heap::gpu_only:
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		break;
	case api::memory_heap::cpu_to_gpu:
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		// Make sure host visible allocations are coherent, since no explicit flushing is performed
		alloc_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case api::memory_heap::gpu_to_cpu:
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
		break;
	case api::memory_heap::cpu_only:
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		break;
	}

	switch (desc.type)
	{
		case api::resource_type::buffer:
		{
			VkBufferCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			convert_resource_desc(desc, create_info);

			VkBuffer buffer = VK_NULL_HANDLE;
			if (vmaCreateBuffer(_alloc, &create_info, &alloc_info, &buffer, &allocation, nullptr) == VK_SUCCESS)
			{
				register_buffer(buffer, create_info, allocation);
				*out = { (uint64_t)buffer };
				return true;
			}
			break;
		}
		case api::resource_type::texture_1d:
		case api::resource_type::texture_2d:
		case api::resource_type::texture_3d:
		{
			VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			convert_resource_desc(desc, create_info);
			if ((desc.usage & (api::resource_usage::render_target | api::resource_usage::shader_resource)) != 0)
				create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

			VkImage image = VK_NULL_HANDLE;
			if (vmaCreateImage(_alloc, &create_info, &alloc_info, &image, &allocation, nullptr) == VK_SUCCESS)
			{
				register_image(image, create_info, allocation);
				*out = { (uint64_t)image };

				if (initial_state != api::resource_usage::undefined)
				{
					// Transition resource into the initial state using the first available immediate command list
					for (command_queue_impl *const queue : _queues)
					{
						const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
						if (immediate_command_list != nullptr)
						{
							immediate_command_list->transition_state(*out, api::resource_usage::undefined, initial_state);
							immediate_command_list->flush_and_wait((VkQueue)queue->get_native_object());
							break;
						}
					}
				}
				return true;
			}
			break;
		}
	}

	*out = { 0 };
	return false;
}
bool reshade::vulkan::device_impl::create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
	{
		VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.image = data.image;
		create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

		// Shader resource views can never access stencil data, so remove that aspect flag for views created with a format that supports stencil
		if ((usage_type & api::resource_usage::shader_resource) != 0)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

		VkImageView image_view = VK_NULL_HANDLE;
		if (vk.CreateImageView(_orig, &create_info, nullptr, &image_view) == VK_SUCCESS)
		{
			register_image_view(image_view, create_info);
			*out = { (uint64_t)image_view };
			return true;
		}
	}
	else
	{
		VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.buffer = data.buffer;

		VkBufferView buffer_view = VK_NULL_HANDLE;
		if (vk.CreateBufferView(_orig, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			register_buffer_view(buffer_view, create_info);
			*out = { (uint64_t)buffer_view };
			return true;
		}
	}

	*out = { 0 };
	return false;
}

void reshade::vulkan::device_impl::destroy_resource(api::resource_handle resource)
{
	if (resource.handle == 0)
		return;
	const resource_data &data = _resources.at(resource.handle);

	// Can only destroy resources that were allocated via 'create_resource' previously
	assert(data.allocation != nullptr);

	if (data.is_image())
		vmaDestroyImage(_alloc, data.image, data.allocation);
	else
		vmaDestroyBuffer(_alloc, data.buffer, data.allocation);

	_resources.erase(resource.handle);
}
void reshade::vulkan::device_impl::destroy_resource_view(api::resource_view_handle view)
{
	if (view.handle == 0)
		return;
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		vk.DestroyImageView(_orig, data.image_view, nullptr);
	else
		vk.DestroyBufferView(_orig, data.buffer_view, nullptr);

	_views.erase(view.handle);
}

void reshade::vulkan::device_impl::get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const
{
	assert(view.handle != 0);
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		*out_resource = { (uint64_t)data.image_create_info.image };
	else
		*out_resource = { (uint64_t)data.buffer_create_info.buffer };
}

reshade::api::resource_desc reshade::vulkan::device_impl::get_resource_desc(api::resource_handle resource) const
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
		return convert_resource_desc(data.image_create_info);
	else
		return convert_resource_desc(data.buffer_create_info);
}

void reshade::vulkan::device_impl::wait_idle() const
{
	vk.DeviceWaitIdle(_orig);
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != VK_NULL_HANDLE)
{
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
	if (_has_commands)
		invoke_addon_event<addon_event::destroy_command_list>(this);
}

void reshade::vulkan::command_list_impl::copy_resource(api::resource_handle source, api::resource_handle destination)
{
	_has_commands = true;

	assert(source.handle != 0 && destination.handle != 0);
	const resource_data &source_data = _device_impl->_resources.at(source.handle);
	const resource_data &destination_data = _device_impl->_resources.at(destination.handle);

	switch ((source_data.is_image() ? 1 : 0) | (destination_data.is_image() ? 2 : 0))
	{
		case 0x0:
		{
			const VkBufferCopy region = {
				0, 0, destination_data.buffer_create_info.size
			};

			_device_impl->vk.CmdCopyBuffer(_orig, source_data.buffer, destination_data.buffer, 1, &region);
			break;
		}
		case 0x1:
		{
			const uint32_t bpp = 4; // TODO: bpp
			assert(source_data.image_create_info.format == VK_FORMAT_R8G8B8A8_UNORM);

			const VkBufferImageCopy region = {
				0, source_data.image_create_info.extent.width * bpp, source_data.image_create_info.extent.width * source_data.image_create_info.extent.height * bpp,
				{ aspect_flags_from_format(source_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 }, source_data.image_create_info.extent
			};

			_device_impl->vk.CmdCopyImageToBuffer(_orig, source_data.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination_data.buffer, 1, &region);
			break;
		}
		case 0x2:
		{
			const uint32_t bpp = 4; // TODO: bpp
			assert(destination_data.image_create_info.format == VK_FORMAT_R8G8B8A8_UNORM);

			const VkBufferImageCopy region = {
				0, destination_data.image_create_info.extent.width * bpp, destination_data.image_create_info.extent.width * destination_data.image_create_info.extent.height * bpp,
				{ aspect_flags_from_format(destination_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 }, destination_data.image_create_info.extent
			};

			_device_impl->vk.CmdCopyBufferToImage(_orig, source_data.buffer, destination_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			break;
		}
		case 0x3:
		{
			const VkImageCopy region = {
				{ aspect_flags_from_format(source_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 },
				{ aspect_flags_from_format(destination_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 }, destination_data.image_create_info.extent
			};

			_device_impl->vk.CmdCopyImage(_orig, source_data.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			break;
		}
	}
}

void reshade::vulkan::command_list_impl::transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state)
{
	_has_commands = true;

	assert(resource.handle != 0);
	const resource_data &data = _device_impl->_resources.at(resource.handle);

	if (data.is_image())
	{
		VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		transition.srcAccessMask = convert_usage_to_access(old_state);
		transition.dstAccessMask = convert_usage_to_access(new_state);
		transition.oldLayout = convert_usage_to_image_layout(old_state);
		transition.newLayout = convert_usage_to_image_layout(new_state);
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.image = data.image;
		transition.subresourceRange = { aspect_flags_from_format(data.image_create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

		_device_impl->vk.CmdPipelineBarrier(_orig, convert_usage_to_pipeline_stage(old_state), convert_usage_to_pipeline_stage(new_state), 0, 0, nullptr, 0, nullptr, 1, &transition);
	}
	else
	{
		VkBufferMemoryBarrier transition { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		transition.srcAccessMask = convert_usage_to_access(old_state);
		transition.dstAccessMask = convert_usage_to_access(new_state);
		transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		transition.buffer = data.buffer;
		transition.offset = 0;
		transition.size = VK_WHOLE_SIZE;

		_device_impl->vk.CmdPipelineBarrier(_orig, convert_usage_to_pipeline_stage(old_state), convert_usage_to_pipeline_stage(new_state), 0, 0, nullptr, 1, &transition, 0, nullptr);
	}
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	const resource_view_data &dsv_data = _device_impl->_views.at(dsv.handle);
	assert(dsv_data.is_image_view()); // Has to be an image

	VkImageAspectFlags aspect_flags = 0;
	if ((clear_flags & 0x1) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if ((clear_flags & 0x2) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::depth_stencil_write' at this point)
	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = dsv_data.image_create_info.image;
	transition.subresourceRange = { aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

	const VkClearDepthStencilValue clear_value = { depth, stencil };
	_device_impl->vk.CmdClearDepthStencilImage(_orig, dsv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

	std::swap(transition.oldLayout, transition.newLayout);
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
}
void reshade::vulkan::command_list_impl::clear_render_target_view(api::resource_view_handle rtv, const float color[4])
{
	_has_commands = true;

	const resource_view_data &rtv_data = _device_impl->_views.at(rtv.handle);
	assert(rtv_data.is_image_view()); // Has to be an image

	// Transition state to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL (since it will be in 'resource_usage::render_target' at this point)
	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	transition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = rtv_data.image_create_info.image;
	transition.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, color, 4 * sizeof(float));

	_device_impl->vk.CmdClearColorImage(_orig, rtv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &transition.subresourceRange);

	std::swap(transition.oldLayout, transition.newLayout);
	_device_impl->vk.CmdPipelineBarrier(_orig, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);
}

reshade::vulkan::command_list_immediate_impl::command_list_immediate_impl(device_impl *device, uint32_t queue_family_index) :
	command_list_impl(device, VK_NULL_HANDLE)
{
	{	VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = queue_family_index;

		if (_device_impl->vk.CreateCommandPool(_device_impl->_orig, &create_info, nullptr, &_cmd_pool) != VK_SUCCESS)
			return;
	}

	{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = _cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = NUM_COMMAND_FRAMES;

		if (_device_impl->vk.AllocateCommandBuffers(_device_impl->_orig, &alloc_info, _cmd_buffers) != VK_SUCCESS)
			return;
	}

	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		// The validation layers expect the loader to have set the dispatch pointer, but this does not happen when calling down the layer chain from here, so fix it
		*reinterpret_cast<void **>(_cmd_buffers[i]) = *reinterpret_cast<void **>(device->_orig);

		VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so waiting on it when no commands where submitted succeeds

		if (_device_impl->vk.CreateFence(_device_impl->_orig, &create_info, nullptr, &_cmd_fences[i]) != VK_SUCCESS)
			return;

		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (_device_impl->vk.CreateSemaphore(_device_impl->_orig, &sem_create_info, nullptr, &_cmd_semaphores[i]) != VK_SUCCESS)
			return;
	}

	// Command buffer is in an invalid state and ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
}
reshade::vulkan::command_list_immediate_impl::~command_list_immediate_impl()
{
	for (VkFence fence : _cmd_fences)
		_device_impl->vk.DestroyFence(_device_impl->_orig, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		_device_impl->vk.DestroySemaphore(_device_impl->_orig, semaphore, nullptr);

	_device_impl->vk.FreeCommandBuffers(_device_impl->_orig, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	_device_impl->vk.DestroyCommandPool(_device_impl->_orig, _cmd_pool, nullptr);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::vulkan::command_list_immediate_impl::flush(VkQueue queue, std::vector<VkSemaphore> &wait_semaphores)
{
	if (!_has_commands)
		return true;

	// Submit all asynchronous commands in one batch to the current queue
	if (_device_impl->vk.EndCommandBuffer(_cmd_buffers[_cmd_index]) != VK_SUCCESS)
		return false;

	VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &_cmd_buffers[_cmd_index];

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
	_device_impl->vk.ResetFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index]);

	if (_device_impl->vk.QueueSubmit(queue, 1, &submit_info, _cmd_fences[_cmd_index]) != VK_SUCCESS)
		return false;

	// This queue submit now waits on the requested wait semaphores
	// The next queue submit should therefore wait on the semaphore that was signaled by this submit
	wait_semaphores.clear();
	wait_semaphores.push_back(_cmd_semaphores[_cmd_index]);

	// Continue with next command buffer now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (_device_impl->vk.GetFenceStatus(_device_impl->_orig, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		_device_impl->vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Command buffer is now ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return false;

	// Command buffer is now in the recording state
	_orig = _cmd_buffers[_cmd_index];
	return true;
}
bool reshade::vulkan::command_list_immediate_impl::flush_and_wait(VkQueue queue)
{
	// Index is updated during flush below, so keep track of the current one to wait on
	const uint32_t cmd_index_to_wait_on = _cmd_index;

	std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
	if (!flush(queue, wait_semaphores))
		return false;

	// Wait for the submitted work to finish and reset fence again for next use
	return _device_impl->vk.WaitForFences(_device_impl->_orig, 1, &_cmd_fences[cmd_index_to_wait_on], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
	api_object_impl(queue), _device_impl(device)
{
	// Register queue to device
	_device_impl->_queues.push_back(this);

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue_family_index);
	}

	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);

	delete _immediate_cmd_list;

	// Unregister queue from device
	_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), this));
}

void reshade::vulkan::command_queue_impl::flush_immediate_command_list() const
{
	std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig, wait_semaphores);
}
