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
	if (state == resource_usage::common)
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

void reshade::vulkan::convert_resource_desc(const resource_desc &desc, VkImageCreateInfo &create_info)
{
	switch (desc.type)
	{
	default:
		assert(false);
		break;
	case resource_type::texture_1d:
		create_info.imageType = VK_IMAGE_TYPE_1D;
		create_info.extent = { desc.width, 1u, 1u };
		create_info.arrayLayers = desc.layers;
		break;
	case resource_type::texture_2d:
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent = { desc.width, desc.height, 1u };
		create_info.arrayLayers = desc.layers;
		break;
	case resource_type::texture_3d:
		create_info.imageType = VK_IMAGE_TYPE_3D;
		create_info.extent = { desc.width, desc.height, desc.layers };
		break;
	}

	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.mipLevels = desc.levels;
	create_info.samples = static_cast<VkSampleCountFlagBits>(desc.samples);
	convert_usage_to_image_usage_flags(desc.usage, create_info.usage);
}
resource_desc reshade::vulkan::convert_resource_desc(const VkImageCreateInfo &create_info)
{
	resource_desc desc = {};
	switch (create_info.imageType)
	{
	default:
		assert(false);
		break;
	case VK_IMAGE_TYPE_1D:
		desc.type = resource_type::texture_1d;
		desc.width = create_info.extent.width;
		desc.height = 1;
		desc.layers = 1;
		break;
	case VK_IMAGE_TYPE_2D:
		desc.type = resource_type::texture_2d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.height;
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_3D:
		desc.type = resource_type::texture_3d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.depth;
		assert(create_info.extent.depth <= std::numeric_limits<uint16_t>::max());
		desc.layers = static_cast<uint16_t>(create_info.extent.depth);
		break;
	}

	desc.levels = static_cast<uint16_t>(create_info.mipLevels);
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.samples = static_cast<uint16_t>(create_info.samples);
	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	return desc;
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
	resource_view_desc desc = { resource_view_type::unknown };
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

resource_usage reshade::vulkan::convert_image_layout(VkImageLayout layout)
{
	switch (layout)
	{
	default:
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return static_cast<resource_usage>(0);
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
		return resource_usage::depth_stencil;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return resource_usage::render_target;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return resource_usage::shader_resource;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return resource_usage::copy_dest;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return resource_usage::copy_source;
	}
}

reshade::vulkan::device_impl::device_impl(VkDevice device, VkPhysicalDevice physical_device, uint32_t graphics_queue_family_index, const VkLayerInstanceDispatchTable &instance_table, const VkLayerDispatchTable &device_table) :
	vk(device_table), graphics_queue_family_index(graphics_queue_family_index), _device(device), _physical_device(physical_device), _instance_dispatch_table(instance_table)
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

	if (graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
	{
		VkCommandPoolCreateInfo create_info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		create_info.queueFamilyIndex = graphics_queue_family_index;

		vk.CreateCommandPool(_device, &create_info, nullptr, &_cmd_pool);
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

	vk.DestroyCommandPool(_device, _cmd_pool, nullptr);

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
	const VkImage image = (VkImage)resource.handle;
	return _resources.at(image).image == image;
}
bool reshade::vulkan::device_impl::is_resource_view_valid(resource_view_handle view)
{
	const VkImageView image_view = (VkImageView)view.handle;
	return _views.at(image_view).view == image_view;
}

bool reshade::vulkan::device_impl::create_resource(const resource_desc &desc, resource_usage initial_state, resource_handle *out_resource)
{
	assert((desc.usage & initial_state) == initial_state);

	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkImageCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	convert_resource_desc(desc, create_info);

	if (VkImage image = VK_NULL_HANDLE;
		vmaCreateImage(_alloc, &create_info, &alloc_info, &image, &allocation, nullptr) == VK_SUCCESS)
	{
		if (initial_state != resource_usage::common && graphics_queue_family_index != std::numeric_limits<uint32_t>::max())
		{
			VkCommandBufferAllocateInfo cmd_alloc_info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmd_alloc_info.commandBufferCount = 1;
			cmd_alloc_info.commandPool = _cmd_pool;

			VkCommandBuffer cmd_list = VK_NULL_HANDLE;
			vk.AllocateCommandBuffers(_device, &cmd_alloc_info, &cmd_list);

			VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			transition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transition.newLayout = convert_usage_to_image_layout(initial_state);
			transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition.image = image;
			transition.subresourceRange = { aspect_flags_from_format(create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

			vk.CmdPipelineBarrier(cmd_list, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition);

			VkSubmitInfo submit_info { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &cmd_list;

			VkQueue queue = VK_NULL_HANDLE;
			vk.GetDeviceQueue(_device, graphics_queue_family_index, 0, &queue);
			vk.QueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
			vk.QueueWaitIdle(queue);

			vk.FreeCommandBuffers(_device, _cmd_pool, 1, &cmd_list);
		}

		register_resource(image, create_info, allocation);
		*out_resource = { (uint64_t)image };
		return true;
	}
	else
	{
		*out_resource = { 0 };
		return false;
	}
}
bool reshade::vulkan::device_impl::create_resource_view(resource_handle resource, const resource_view_desc &desc, resource_view_handle *out_view)
{
	assert(resource.handle != 0);

	VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	convert_resource_view_desc(desc, create_info);
	create_info.image = (VkImage)resource.handle;
	create_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	create_info.subresourceRange.aspectMask = aspect_flags_from_format(create_info.format);

	// Shader resource views can never access stencil data, so remove that aspect flag for views created with a format that supports stencil
	if (desc.type == resource_view_type::shader_resource)
		create_info.subresourceRange.aspectMask &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

	if (VkImageView image_view = VK_NULL_HANDLE;
		vk.CreateImageView(_device, &create_info, nullptr, &image_view) == VK_SUCCESS)
	{
		register_resource_view(image_view, create_info);
		*out_view = { (uint64_t)image_view };
		return true;
	}
	else
	{
		*out_view = { 0 };
		return false;
	}
}

void reshade::vulkan::device_impl::destroy_resource(resource_handle resource)
{
	assert(resource.handle != 0);

	const resource_data &data = _resources.at((VkImage)resource.handle);
	assert(data.allocation != nullptr);
	vmaDestroyImage(_alloc, data.image, data.allocation);

	unregister_resource((VkImage)resource.handle);
}
void reshade::vulkan::device_impl::destroy_resource_view(resource_view_handle view)
{
	assert(view.handle != 0);

	vk.DestroyImageView(_device, (VkImageView)view.handle, nullptr);

	unregister_resource_view((VkImageView)view.handle);
}

void reshade::vulkan::device_impl::get_resource_from_view(resource_view_handle view, resource_handle *out_resource)
{
	assert(view.handle != 0);

	const resource_view_data &data = _views.at((VkImageView)view.handle);
	*out_resource = { (uint64_t)data.create_info.image };
}

resource_desc reshade::vulkan::device_impl::get_resource_desc(resource_handle resource)
{
	assert(resource.handle != 0);

	const resource_data &data = _resources.at((VkImage)resource.handle);
	return convert_resource_desc(data.create_info);
}

void reshade::vulkan::device_impl::wait_idle()
{
	vk.DeviceWaitIdle(_device);
}

resource_view_handle reshade::vulkan::device_impl::get_default_view(VkImage image)
{
	VkImageViewCreateInfo create_info { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	create_info.image = image;

	register_resource_view((VkImageView)image, create_info);
	return { (uint64_t)image };
}

reshade::vulkan::command_list_impl::command_list_impl(device_impl *device, VkCommandBuffer cmd_list) :
	_device_impl(device), _cmd_list(cmd_list)
{
	RESHADE_ADDON_EVENT(init_command_list, this);
}
reshade::vulkan::command_list_impl::~command_list_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_list, this);
}

void reshade::vulkan::command_list_impl::transition_state(resource_handle resource, resource_usage old_state, resource_usage new_state)
{
	assert(resource.handle != 0);
	const resource_data &data = _device_impl->_resources.at((VkImage)resource.handle);

	VkImageMemoryBarrier transition { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	transition.srcAccessMask = convert_usage_to_access(old_state);
	transition.dstAccessMask = convert_usage_to_access(new_state);
	transition.oldLayout = convert_usage_to_image_layout(old_state);
	transition.newLayout = convert_usage_to_image_layout(new_state);
	transition.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	transition.image = data.image;
	transition.subresourceRange = { aspect_flags_from_format(data.create_info.format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	_device_impl->vk.CmdPipelineBarrier(_cmd_list, convert_usage_to_pipeline_stage(old_state), convert_usage_to_pipeline_stage(new_state), 0, 0, nullptr, 0, nullptr, 1, &transition);
}

void reshade::vulkan::command_list_impl::clear_depth_stencil_view(resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	const resource_view_data &dsv_data = _device_impl->_views.at((VkImageView)dsv.handle);

	const VkClearDepthStencilValue clear_value = { depth, stencil };

	VkImageAspectFlags aspect_flags = 0;
	if ((clear_flags & 0x1) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
	if ((clear_flags & 0x2) != 0)
		aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	const VkImageSubresourceRange range = {
		aspect_flags,
		0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	_device_impl->vk.CmdClearDepthStencilImage(_cmd_list, dsv_data.create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}
void reshade::vulkan::command_list_impl::clear_render_target_view(resource_view_handle rtv, const float color[4])
{
	const resource_view_data &rtv_data = _device_impl->_views.at((VkImageView)rtv.handle);

	VkClearColorValue clear_value;
	std::memcpy(clear_value.float32, color, 4 * sizeof(float));

	const VkImageSubresourceRange range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

	_device_impl->vk.CmdClearColorImage(_cmd_list, rtv_data.create_info.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}

void reshade::vulkan::command_list_impl::copy_resource(resource_handle source, resource_handle dest)
{
	assert(source.handle != 0 && dest.handle != 0);
	const resource_data &dest_data = _device_impl->_resources.at((VkImage)dest.handle);
	const resource_data &source_data = _device_impl->_resources.at((VkImage)source.handle);

	const VkImageCopy region = {
		{ aspect_flags_from_format(source_data.create_info.format), 0, 0, 1 }, { 0, 0, 0 },
		{ aspect_flags_from_format(  dest_data.create_info.format), 0, 0, 1 }, { 0, 0, 0 }, dest_data.create_info.extent
	};

	_device_impl->vk.CmdCopyImage(_cmd_list, source_data.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dest_data.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

reshade::vulkan::command_queue_impl::command_queue_impl(device_impl *device, VkQueue queue) :
	_device_impl(device), _queue(queue)
{
	RESHADE_ADDON_EVENT(init_command_queue, this);
}
reshade::vulkan::command_queue_impl::~command_queue_impl()
{
	RESHADE_ADDON_EVENT(destroy_command_queue, this);
}
