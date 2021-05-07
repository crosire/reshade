/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"
#include "format_utils.hpp"
#include <algorithm>

#define vk _dispatch_table

static inline VkImageSubresourceLayers convert_subresource(uint32_t subresource, const VkImageCreateInfo &create_info)
{
	VkImageSubresourceLayers result;
	result.aspectMask = aspect_flags_from_format(create_info.format);
	result.mipLevel = subresource % create_info.mipLevels;
	result.baseArrayLayer = subresource / create_info.mipLevels;
	result.layerCount = 1;
	return result;
}

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

bool reshade::vulkan::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	VkFormat image_format = VK_FORMAT_UNDEFINED;
	convert_format_to_vk_format(format, image_format);

	VkImageUsageFlags image_flags = 0;
	convert_usage_to_image_usage_flags(usage, image_flags);

	VkImageFormatProperties props;
	return _instance_dispatch_table.GetPhysicalDeviceImageFormatProperties(_physical_device, image_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, image_flags, 0, &props) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::check_resource_handle_valid(api::resource resource) const
{
	if (resource.handle == 0)
		return false;
	const resource_data &data = _resources.at(resource.handle);

	if (data.is_image())
		return data.image == (VkImage)resource.handle;
	else
		return data.buffer == (VkBuffer)resource.handle;
}
bool reshade::vulkan::device_impl::check_resource_view_handle_valid(api::resource_view view) const
{
	if (view.handle == 0)
		return false;
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		return data.image_view == (VkImageView)view.handle;
	else
		return data.buffer_view == (VkBufferView)view.handle;
}

bool reshade::vulkan::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	VkSamplerCreateInfo create_info { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	convert_sampler_desc(desc, create_info);

	if (VkSampler object = VK_NULL_HANDLE;
		vk.CreateSampler(_orig, &create_info, nullptr, &object) == VK_SUCCESS)
	{
		*out = { (uint64_t)object };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out)
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

			if (VkBuffer object = VK_NULL_HANDLE;
				vmaCreateBuffer(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr) == VK_SUCCESS)
			{
				register_buffer(object, create_info, allocation);
				*out = { (uint64_t)object };
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

			if (VkImage object = VK_NULL_HANDLE;
				vmaCreateImage(_alloc, &create_info, &alloc_info, &object, &allocation, nullptr) == VK_SUCCESS)
			{
				register_image(object, create_info, allocation);
				*out = { (uint64_t)object };

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
bool reshade::vulkan::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
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

void reshade::vulkan::device_impl::destroy_sampler(api::sampler handle)
{
	vk.DestroySampler(_orig, (VkSampler)handle.handle, nullptr);
}
void reshade::vulkan::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle == 0)
		return;
	const resource_data &data = _resources.at(handle.handle);

	// Can only destroy resources that were allocated via 'create_resource' previously
	assert(data.allocation != nullptr);

	if (data.is_image())
		vmaDestroyImage(_alloc, data.image, data.allocation);
	else
		vmaDestroyBuffer(_alloc, data.buffer, data.allocation);

	_resources.erase(handle.handle);
}
void reshade::vulkan::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle == 0)
		return;
	const resource_view_data &data = _views.at(handle.handle);

	if (data.is_image_view())
		vk.DestroyImageView(_orig, data.image_view, nullptr);
	else
		vk.DestroyBufferView(_orig, data.buffer_view, nullptr);

	_views.erase(handle.handle);
}

void reshade::vulkan::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);
	const resource_view_data &data = _views.at(view.handle);

	if (data.is_image_view())
		*out_resource = { (uint64_t)data.image_create_info.image };
	else
		*out_resource = { (uint64_t)data.buffer_create_info.buffer };
}

reshade::api::resource_desc reshade::vulkan::device_impl::get_resource_desc(api::resource resource) const
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

void reshade::vulkan::device_impl::set_debug_name(api::resource resource, const char *name)
{
	if (vk.DebugMarkerSetObjectNameEXT != nullptr)
	{
		const resource_data &data = _resources.at(resource.handle);

		VkDebugMarkerObjectNameInfoEXT name_info { VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT };
		name_info.object = resource.handle;
		name_info.objectType = data.is_image() ? VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT : VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
		name_info.pObjectName = name;

		vk.DebugMarkerSetObjectNameEXT(_orig, &name_info);
	}
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

void reshade::vulkan::command_list_impl::blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageBlit region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_box != nullptr)
	{
		std::copy_n(src_box, 6, &region.srcOffsets[0].x);
	}
	else
	{
		region.srcOffsets[0] = { 0, 0, 0 };
		region.srcOffsets[1] = {
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel)) };
	}

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_box != nullptr)
	{
		std::copy_n(dst_box, 6, &region.dstOffsets[0].x);
	}
	else
	{
		region.dstOffsets[0] = { 0, 0, 0 };
		region.dstOffsets[1] = {
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.width >> region.dstSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.height >> region.dstSubresource.mipLevel)),
			static_cast<int32_t>(std::max(1u, dst_data.image_create_info.extent.depth >> region.dstSubresource.mipLevel)) };
	}

	_device_impl->vk.CmdBlitImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region,
		filter == api::texture_filter::min_mag_mip_linear || filter == api::texture_filter::min_mag_linear_mip_point ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
}
void reshade::vulkan::command_list_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageResolve region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, &region.srcOffset.x);
	else
		region.srcOffset = { 0, 0, 0 };

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, &region.dstOffset.x);
	else
		region.dstOffset = { 0, 0, 0 };

	if (size != nullptr)
	{
		std::copy_n(size, 3, &region.extent.width);
	}
	else
	{
		region.extent.width = std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel);
	}

	_device_impl->vk.CmdResolveImage(_orig,
		(VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		(VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region);
}
void reshade::vulkan::command_list_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = _device_impl->get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t layer = 0, layers = (desc.type != api::resource_type::texture_3d) ? desc.texture.depth_or_layers : 1; layer < layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, nullptr);
			}
		}
	}
}
void reshade::vulkan::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	VkBufferCopy region;
	region.srcOffset = src_offset;
	region.dstOffset = dst_offset;
	region.size = size;

	_device_impl->vk.CmdCopyBuffer(_orig, (VkBuffer)src.handle, (VkBuffer)dst.handle, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(!src_data.is_image() && dst_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = src_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	region.imageSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_box != nullptr)
	{
		std::copy_n(dst_box, 3, &region.imageOffset.x);
		region.imageExtent.width = dst_box[3] - dst_box[0];
		region.imageExtent.height = dst_box[4] - dst_box[1];
		region.imageExtent.depth = dst_box[5] - dst_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width = std::max(1u, dst_data.image_create_info.extent.width >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, dst_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth = std::max(1u, dst_data.image_create_info.extent.depth >> region.imageSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyBufferToImage(_orig, (VkBuffer)src.handle, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && dst_data.is_image());

	VkImageCopy region;

	region.srcSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, &region.srcOffset.x);
	else
		region.srcOffset = { 0, 0, 0 };

	region.dstSubresource = convert_subresource(dst_subresource, dst_data.image_create_info);
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, &region.dstOffset.x);
	else
		region.dstOffset = { 0, 0, 0 };

	if (size != nullptr)
	{
		std::copy_n(size, 3, &region.extent.width);
	}
	else
	{
		region.extent.width = std::max(1u, src_data.image_create_info.extent.width >> region.srcSubresource.mipLevel);
		region.extent.height = std::max(1u, src_data.image_create_info.extent.height >> region.srcSubresource.mipLevel);
		region.extent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.srcSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyImage(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkImage)dst.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}
void reshade::vulkan::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	const resource_data &src_data = _device_impl->_resources.at(src.handle);
	const resource_data &dst_data = _device_impl->_resources.at(dst.handle);

	assert(src_data.is_image() && !dst_data.is_image());

	VkBufferImageCopy region;
	region.bufferOffset = dst_offset;
	region.bufferRowLength = row_length;
	region.bufferImageHeight = slice_height;

	region.imageSubresource = convert_subresource(src_subresource, src_data.image_create_info);
	if (src_box != nullptr)
	{
		std::copy_n(src_box, 3, &region.imageOffset.x);
		region.imageExtent.width = src_box[3] - src_box[0];
		region.imageExtent.height = src_box[4] - src_box[1];
		region.imageExtent.depth = src_box[5] - src_box[2];
	}
	else
	{
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent.width = std::max(1u, src_data.image_create_info.extent.width >> region.imageSubresource.mipLevel);
		region.imageExtent.height = std::max(1u, src_data.image_create_info.extent.height >> region.imageSubresource.mipLevel);
		region.imageExtent.depth = std::max(1u, src_data.image_create_info.extent.depth >> region.imageSubresource.mipLevel);
	}

	_device_impl->vk.CmdCopyImageToBuffer(_orig, (VkImage)src.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (VkBuffer)dst.handle, 1, &region);
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
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
void reshade::vulkan::command_list_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	_has_commands = true;

	for (uint32_t i = 0; i < count; ++i)
	{
		const resource_view_data &rtv_data = _device_impl->_views.at(rtvs[i].handle);
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
}

void reshade::vulkan::command_list_impl::transition_state(api::resource resource, api::resource_usage old_state, api::resource_usage new_state)
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
