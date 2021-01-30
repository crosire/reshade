/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "format_utils.hpp"

using namespace reshade::api;

static auto convert_usage_to_access(resource_usage state) -> VkAccessFlags
{
	VkAccessFlags result = 0;
	if ((state & resource_usage::depth_stencil_read) != 0)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if ((state & resource_usage::depth_stencil_write) != 0)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if ((state & resource_usage::render_target) != 0)
		result |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if ((state & resource_usage::shader_resource) != 0)
		result |= VK_ACCESS_SHADER_READ_BIT;
	if ((state & resource_usage::unordered_access) != 0)
		result |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	if ((state & (resource_usage::copy_dest | resource_usage::resolve_dest)) != 0)
		result |= VK_ACCESS_TRANSFER_WRITE_BIT;
	if ((state & (resource_usage::copy_source | resource_usage::resolve_source)) != 0)
		result |= VK_ACCESS_TRANSFER_READ_BIT;
	return result;
}
static auto convert_usage_to_image_layout(resource_usage state) -> VkImageLayout
{
	switch (state)
	{
	default:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case resource_usage::depth_stencil:
	case resource_usage::depth_stencil_read:
	case resource_usage::depth_stencil_write:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case resource_usage::render_target:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case resource_usage::shader_resource:
	case resource_usage::shader_resource_pixel:
	case resource_usage::shader_resource_non_pixel:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case resource_usage::unordered_access:
		return VK_IMAGE_LAYOUT_GENERAL;
	case resource_usage::copy_dest:
	case resource_usage::resolve_dest:
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case resource_usage::copy_source:
	case resource_usage::resolve_source:
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
}
static auto convert_usage_to_pipeline_stage(resource_usage state) -> VkPipelineStageFlags
{
	if (state == resource_usage::undefined)
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Do not wait on any previous stage

	VkPipelineStageFlags result = 0;
	if ((state & resource_usage::depth_stencil) != 0)
		result |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if ((state & resource_usage::render_target) != 0)
		result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if ((state & resource_usage::shader_resource_pixel) != 0)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if ((state & resource_usage::shader_resource_non_pixel) != 0)
		result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & resource_usage::unordered_access) != 0)
		result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & (resource_usage::copy_dest | resource_usage::copy_source | resource_usage::resolve_dest | resource_usage::resolve_source)) != 0)
		result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	return result;
}

static void convert_usage_to_image_usage_flags(const resource_usage usage, VkImageUsageFlags &image_flags)
{
	if ((usage & resource_usage::render_target) != 0)
		image_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if ((usage & resource_usage::depth_stencil) != 0)
		image_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if ((usage & resource_usage::shader_resource) != 0)
		image_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_SAMPLED_BIT;

	if ((usage & resource_usage::unordered_access) != 0)
		image_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_STORAGE_BIT;

	if ((usage & (resource_usage::copy_dest | resource_usage::resolve_dest)) != 0)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if ((usage & (resource_usage::copy_source | resource_usage::resolve_source)) != 0)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
}
static void convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, resource_usage &usage)
{
	if ((image_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
		usage |= resource_usage::render_target;
	if ((image_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		usage |= resource_usage::depth_stencil;
	if ((image_flags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
		usage |= resource_usage::shader_resource;
	if ((image_flags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
		usage |= resource_usage::unordered_access;
	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= resource_usage::copy_dest;
	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= resource_usage::copy_source;
}

void reshade::vulkan::convert_resource_desc(const resource_desc &desc, VkBufferCreateInfo &create_info)
{
	create_info.size = desc.width;
	assert(desc.height <= 1 && desc.depth_or_layers <= 1 && desc.levels <= 1 && desc.samples <= 1);
	convert_usage_to_image_usage_flags(desc.usage, create_info.usage);
}
resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	resource_desc desc = {};
	assert(create_info.size <= std::numeric_limits<uint32_t>::max());
	desc.width = static_cast<uint32_t>(create_info.size);
	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	return desc;
}
void reshade::vulkan::convert_resource_desc(resource_type type, const resource_desc &desc, VkImageCreateInfo &create_info)
{
	switch (type)
	{
	default:
		assert(false);
		break;
	case resource_type::texture_1d:
		create_info.imageType = VK_IMAGE_TYPE_1D;
		create_info.extent = { desc.width, 1u, 1u };
		create_info.arrayLayers = desc.depth_or_layers;
		break;
	case resource_type::texture_2d:
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent = { desc.width, desc.height, 1u };
		create_info.arrayLayers = desc.depth_or_layers;
		break;
	case resource_type::texture_3d:
		create_info.imageType = VK_IMAGE_TYPE_3D;
		create_info.extent = { desc.width, desc.height, desc.depth_or_layers };
		create_info.arrayLayers = 1u;
		break;
	}

	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.mipLevels = desc.levels;
	create_info.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
	convert_usage_to_image_usage_flags(desc.usage, create_info.usage);
}
std::pair<resource_type, resource_desc> reshade::vulkan::convert_resource_desc(const VkImageCreateInfo &create_info)
{
	resource_type type = resource_type::unknown;
	resource_desc desc = {};
	switch (create_info.imageType)
	{
	default:
		assert(false);
		break;
	case VK_IMAGE_TYPE_1D:
		type = resource_type::texture_1d;
		desc.width = create_info.extent.width;
		desc.height = 1;
		desc.depth_or_layers = 1;
		break;
	case VK_IMAGE_TYPE_2D:
		type = resource_type::texture_2d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.height;
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_3D:
		type = resource_type::texture_3d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.depth;
		assert(create_info.extent.depth <= std::numeric_limits<uint16_t>::max());
		desc.depth_or_layers = static_cast<uint16_t>(create_info.extent.depth);
		assert(create_info.arrayLayers <= 1);
		break;
	}

	desc.levels = static_cast<uint16_t>(create_info.mipLevels);
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.samples = static_cast<uint16_t>(create_info.samples);
	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);

	return { type, desc };
}

void reshade::vulkan::convert_resource_view_desc(const resource_view_desc &desc, VkImageViewCreateInfo &create_info)
{
	switch (desc.dimension)
	{
	case resource_view_dimension::texture_1d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case resource_view_dimension::texture_1d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case resource_view_dimension::texture_2d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case resource_view_dimension::texture_2d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case resource_view_dimension::texture_3d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case resource_view_dimension::texture_cube:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case resource_view_dimension::texture_cube_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	}

	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.subresourceRange.baseMipLevel = desc.first_level;
	create_info.subresourceRange.levelCount = desc.levels;
	create_info.subresourceRange.baseArrayLayer = desc.first_layer;
	create_info.subresourceRange.layerCount = desc.layers;
}
resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkImageViewCreateInfo &create_info)
{
	resource_view_desc desc = {};
	switch (create_info.viewType)
	{
	case VK_IMAGE_VIEW_TYPE_1D:
		desc.dimension = resource_view_dimension::texture_1d;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		desc.dimension = resource_view_dimension::texture_1d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
		desc.dimension = resource_view_dimension::texture_2d;
		break;
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		desc.dimension = resource_view_dimension::texture_2d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		desc.dimension = resource_view_dimension::texture_3d;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		desc.dimension = resource_view_dimension::texture_cube;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		desc.dimension = resource_view_dimension::texture_cube_array;
		break;
	}

	desc.format = static_cast<uint32_t>(create_info.format);
	desc.first_level = create_info.subresourceRange.baseMipLevel;
	desc.levels = create_info.subresourceRange.levelCount;
	desc.first_layer = create_info.subresourceRange.baseArrayLayer;
	desc.layers = create_info.subresourceRange.layerCount;

	return desc;
}
void reshade::vulkan::convert_resource_view_desc(const resource_view_desc &desc, VkBufferViewCreateInfo &create_info)
{
	assert(desc.dimension == resource_view_dimension::buffer);
	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.offset = desc.buffer_offset;
	create_info.range = desc.buffer_size;
}
resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkBufferViewCreateInfo &create_info)
{
	resource_view_desc desc = { resource_view_dimension::buffer };
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.buffer_offset = create_info.offset;
	desc.buffer_size = create_info.range;
	return desc;
}

reshade::vulkan::device_impl::device_impl(VkDevice device, VkPhysicalDevice physical_device, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table) :
	vk(device_table), _device(device), _physical_device(physical_device), _instance_dispatch_table(instance_table)
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
	// Load and initialize add-ons
	reshade::addon::load_addons();
#endif

	RESHADE_ADDON_EVENT(init_device, this);
}
reshade::vulkan::device_impl::~device_impl()
{
	RESHADE_ADDON_EVENT(destroy_device, this);

#if RESHADE_ADDON
	reshade::addon::unload_addons();
#endif

	vmaDestroyAllocator(_alloc);
}

bool reshade::vulkan::device_impl::check_format_support(uint32_t format, api::resource_usage usage)
{
	VkImageUsageFlags image_flags = 0;
	convert_usage_to_image_usage_flags(usage, image_flags);

	VkImageFormatProperties props;
	return _instance_dispatch_table.GetPhysicalDeviceImageFormatProperties(_physical_device, static_cast<VkFormat>(format), VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, image_flags, 0, &props) == VK_SUCCESS;
}

bool reshade::vulkan::device_impl::is_resource_valid(resource_handle resource)
{
	const resource_data &data = _resources.at(resource.handle);
	return data.type ? (data.image == (VkImage)resource.handle) : (data.buffer == (VkBuffer)resource.handle);
}
bool reshade::vulkan::device_impl::is_resource_view_valid(resource_view_handle view)
{
	const resource_view_data &data = _views.at(view.handle);
	return data.type ? (data.image_view == (VkImageView)view.handle) : (data.buffer_view == (VkBufferView)view.handle);
}

bool reshade::vulkan::device_impl::create_resource(resource_type type, const resource_desc &desc, resource_handle *out_resource)
{
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (type == resource_type::texture_1d || type == resource_type::texture_2d || type == resource_type::texture_3d)
	{
		VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		convert_resource_desc(type, desc, create_info);
		create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

		if (VkImage image = VK_NULL_HANDLE;
			vmaCreateImage(_alloc, &create_info, &alloc_info, &image, &allocation, nullptr) == VK_SUCCESS)
		{
			register_image(image, create_info, allocation);
			*out_resource = { (uint64_t)image };
			return true;
		}
	}

	*out_resource = { 0 };
	return false;
}
bool reshade::vulkan::device_impl::create_resource_view(resource_handle resource, resource_view_type type, const resource_view_desc &desc, resource_view_handle *out_view)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.type)
	{
		VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.image = data.image;
		create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

		// Shader resource views can never access stencil data, so remove that aspect flag for views created with a format that supports stencil
		if (type == resource_view_type::shader_resource)
			create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

		if (VkImageView image_view = VK_NULL_HANDLE;
			vk.CreateImageView(_device, &create_info, nullptr, &image_view) == VK_SUCCESS)
		{
			register_image_view(image_view, create_info);
			*out_view = { (uint64_t)image_view };
			return true;
		}
	}
	else
	{
		VkBufferViewCreateInfo create_info { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		convert_resource_view_desc(desc, create_info);
		create_info.buffer = data.buffer;

		if (VkBufferView buffer_view = VK_NULL_HANDLE;
			vk.CreateBufferView(_device, &create_info, nullptr, &buffer_view) == VK_SUCCESS)
		{
			register_buffer_view(buffer_view, create_info);
			*out_view = { (uint64_t)buffer_view };
			return true;
		}
	}

	*out_view = { 0 };
	return false;
}

void reshade::vulkan::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);
	assert(data.allocation != nullptr);

	if (data.type)
		vmaDestroyImage(_alloc, data.image, data.allocation);
	else
		vmaDestroyBuffer(_alloc, data.buffer, data.allocation);

	_resources.erase(resource.handle);
}
void reshade::vulkan::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);
	const resource_view_data &data = _views.at(view.handle);

	if (data.type)
		vk.DestroyImageView(_device, data.image_view, nullptr);
	else
		vk.DestroyBufferView(_device, data.buffer_view, nullptr);

	_views.erase(view.handle);
}

void reshade::vulkan::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource)
{
	assert(view.handle != 0);
	const resource_view_data &data = _views.at(view.handle);

	*out_resource = { data.type ? (uint64_t)data.image_create_info.image : (uint64_t)data.buffer_create_info.buffer };
}

resource_desc reshade::vulkan::device_impl::get_resource_desc(resource_handle resource)
{
	assert(resource.handle != 0);
	const resource_data &data = _resources.at(resource.handle);

	if (data.type)
		return convert_resource_desc(data.image_create_info).second;
	else
		return convert_resource_desc(data.buffer_create_info);
}

void reshade::vulkan::device_impl::wait_idle()
{
	vk.DeviceWaitIdle(_device);
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	_device_impl(device), _cmd_list(cmd_list), _has_commands(cmd_list != VK_NULL_HANDLE)
{
	if (_has_commands)
	{
		RESHADE_ADDON_EVENT(init_command_list, this);
	}
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
	if (_has_commands)
	{
		RESHADE_ADDON_EVENT(destroy_command_list, this);
	}
}

void reshade::vulkan::command_list_impl::transition_state(resource_handle resource, resource_usage old_state, resource_usage new_state)
{
	_has_commands = true;

	assert(resource.handle != 0);
	const resource_data &data = _device_impl->_resources.at(resource.handle);
	if (data.type)
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

		_device_impl->vk.CmdPipelineBarrier(_cmd_list, convert_usage_to_pipeline_stage(old_state), convert_usage_to_pipeline_stage(new_state), 0, 0, nullptr, 0, nullptr, 1, &transition);
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

		_device_impl->vk.CmdPipelineBarrier(_cmd_list, convert_usage_to_pipeline_stage(old_state), convert_usage_to_pipeline_stage(new_state), 0, 0, nullptr, 1, &transition, 0, nullptr);
	}
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	const resource_view_data &dsv_data = _device_impl->_views.at(dsv.handle);
	assert(dsv_data.type); // Has to be an image

	const VkClearDepthStencilValue clear_value = { depth, stencil };

	VkImageAspectFlags aspect_flags = 0;
	if ((clear_flags & 0x1) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if ((clear_flags & 0x2) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	const VkImageSubresourceRange range = {
		aspect_flags,
		0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	_device_impl->vk.CmdClearDepthStencilImage(_cmd_list, dsv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}
void reshade::vulkan::command_list_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	_has_commands = true;

	const resource_view_data &rtv_data = _device_impl->_views.at(rtv.handle);
	assert(rtv_data.type); // Has to be an image

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, color, 4 * sizeof(float));

	const VkImageSubresourceRange range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	_device_impl->vk.CmdClearColorImage(_cmd_list, rtv_data.image_create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}

void reshade::vulkan::command_list_impl::copy_resource(resource_handle source, resource_handle dest)
{
	_has_commands = true;

	assert(source.handle != 0 && dest.handle != 0);
	const resource_data &dest_data = _device_impl->_resources.at(dest.handle);
	const resource_data &source_data = _device_impl->_resources.at(source.handle);

	switch ((source_data.type ? 1 : 0) | (dest_data.type ? 2 : 0))
	{
		case 0x0:
		{
			const VkBufferCopy region = {
				0, 0, dest_data.buffer_create_info.size
			};

			_device_impl->vk.CmdCopyBuffer(_cmd_list, source_data.buffer, dest_data.buffer, 1, &region);
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

			_device_impl->vk.CmdCopyImageToBuffer(_cmd_list, source_data.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dest_data.buffer, 1, &region);
			break;
		}
		case 0x2:
		{
			const uint32_t bpp = 4; // TODO: bpp
			assert(dest_data.image_create_info.format == VK_FORMAT_R8G8B8A8_UNORM);

			const VkBufferImageCopy region = {
				0, dest_data.image_create_info.extent.width * bpp, dest_data.image_create_info.extent.width * dest_data.image_create_info.extent.height * bpp,
				{ aspect_flags_from_format(dest_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 }, dest_data.image_create_info.extent
			};

			_device_impl->vk.CmdCopyBufferToImage(_cmd_list, source_data.buffer, dest_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			break;
		}
		case 0x3:
		{
			const VkImageCopy region = {
				{ aspect_flags_from_format(source_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 },
				{ aspect_flags_from_format(  dest_data.image_create_info.format), 0, 0, 1 }, { 0, 0, 0 }, dest_data.image_create_info.extent
			};

			_device_impl->vk.CmdCopyImage(_cmd_list, source_data.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dest_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			break;
		}
	}
}

reshade::vulkan::command_list_immediate_impl::command_list_immediate_impl(device_impl *device, uint32_t queue_family_index) :
	command_list_impl(device, VK_NULL_HANDLE)
{
	{	VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = queue_family_index;

		if (_device_impl->vk.CreateCommandPool(*_device_impl, &create_info, nullptr, &_cmd_pool) != VK_SUCCESS)
			return;
	}

	{   VkCommandBufferAllocateInfo alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool = _cmd_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = NUM_COMMAND_FRAMES;

		if (_device_impl->vk.AllocateCommandBuffers(*_device_impl, &alloc_info, _cmd_buffers) != VK_SUCCESS)
			return;
	}

	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		// The validation layers expect the loader to have set the dispatch pointer, but this does not happen when calling down the layer chain from here, so fix it
		*reinterpret_cast<void **>(_cmd_buffers[i]) = *reinterpret_cast<void **>(static_cast<VkDevice>(*device));

		VkFenceCreateInfo create_info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled so waiting on it when no commands where submitted succeeds

		if (_device_impl->vk.CreateFence(*_device_impl, &create_info, nullptr, &_cmd_fences[i]) != VK_SUCCESS)
			return;

		VkSemaphoreCreateInfo sem_create_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		if (_device_impl->vk.CreateSemaphore(*_device_impl, &sem_create_info, nullptr, &_cmd_semaphores[i]) != VK_SUCCESS)
			return;
	}

	// Command buffer is in an invalid state and ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return;

	// Command buffer is now in the recording state
	_cmd_list = _cmd_buffers[_cmd_index];
}
reshade::vulkan::command_list_immediate_impl::~command_list_immediate_impl()
{
	for (VkFence fence : _cmd_fences)
		_device_impl->vk.DestroyFence(*_device_impl, fence, nullptr);
	for (VkSemaphore semaphore : _cmd_semaphores)
		_device_impl->vk.DestroySemaphore(*_device_impl, semaphore, nullptr);

	_device_impl->vk.FreeCommandBuffers(*_device_impl, _cmd_pool, NUM_COMMAND_FRAMES, _cmd_buffers);
	_device_impl->vk.DestroyCommandPool(*_device_impl, _cmd_pool, nullptr);

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
	_device_impl->vk.ResetFences(*_device_impl, 1, &_cmd_fences[_cmd_index]);

	if (_device_impl->vk.QueueSubmit(queue, 1, &submit_info, _cmd_fences[_cmd_index]) != VK_SUCCESS)
		return false;

	// This queue submit now waits on the requested wait semaphores
	// The next queue submit should therefore wait on the semaphore that was signaled by this submit
	if (!wait_semaphores.empty())
	{
		wait_semaphores.clear();
		wait_semaphores.push_back(_cmd_semaphores[_cmd_index]);
	}

	// Continue with next command buffer now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure the next command buffer has finished executing before reusing it this frame
	if (_device_impl->vk.GetFenceStatus(*_device_impl, _cmd_fences[_cmd_index]) == VK_NOT_READY)
	{
		_device_impl->vk.WaitForFences(*_device_impl, 1, &_cmd_fences[_cmd_index], VK_TRUE, UINT64_MAX);
	}

	// Command buffer is now ready for a reset
	VkCommandBufferBeginInfo begin_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (_device_impl->vk.BeginCommandBuffer(_cmd_buffers[_cmd_index], &begin_info) != VK_SUCCESS)
		return false;

	// Command buffer is now in the recording state
	_cmd_list = _cmd_buffers[_cmd_index];
	return true;
}
bool reshade::vulkan::command_list_immediate_impl::flush_and_wait(VkQueue queue)
{
	// Index is updated during flush below, so keep track of the current one to wait on
	const uint32_t cmd_index_to_wait_on = _cmd_index;

	if (std::vector<VkSemaphore> wait_semaphores; // No semaphores to wait on
		!flush(queue, wait_semaphores))
		return false;

	// Wait for the submitted work to finish and reset fence again for next use
	return _device_impl->vk.WaitForFences(*_device_impl, 1, &_cmd_fences[cmd_index_to_wait_on], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
}

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, uint32_t queue_family_index, const VkQueueFamilyProperties &queue_family, VkQueue queue) :
	_device_impl(device), _queue(queue)
{
	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue_family_index);
	}

	RESHADE_ADDON_EVENT(init_command_queue, this);
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_queue, this);

	delete _immediate_cmd_list;
}
