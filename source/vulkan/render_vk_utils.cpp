/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"

using namespace reshade::api;

auto reshade::vulkan::convert_usage_to_access(resource_usage state) -> VkAccessFlags
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
	if ((state & resource_usage::index_buffer) != 0)
		result |= VK_ACCESS_INDEX_READ_BIT;
	if ((state & resource_usage::vertex_buffer) != 0)
		result |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	if ((state & resource_usage::constant_buffer) != 0)
		result |= VK_ACCESS_UNIFORM_READ_BIT;
	return result;
}
auto reshade::vulkan::convert_usage_to_image_layout(resource_usage state) -> VkImageLayout
{
	switch (state)
	{
	case resource_usage::undefined:
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
	default: // Default to general layout if multiple usage flags are specified
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
auto reshade::vulkan::convert_usage_to_pipeline_stage(resource_usage state) -> VkPipelineStageFlags
{
	if (state == resource_usage::undefined)
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Do not wait on any previous stage

	VkPipelineStageFlags result = 0;
	if ((state & resource_usage::depth_stencil_read) != 0)
		result |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	if ((state & resource_usage::depth_stencil_write) != 0)
		result |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if ((state & resource_usage::render_target) != 0)
		result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if ((state & (resource_usage::shader_resource_pixel | resource_usage::constant_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if ((state & (resource_usage::shader_resource_non_pixel | resource_usage::constant_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & resource_usage::unordered_access) != 0)
		result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & (resource_usage::copy_dest | resource_usage::copy_source | resource_usage::resolve_dest | resource_usage::resolve_source)) != 0)
		result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if ((state & (resource_usage::index_buffer | resource_usage::vertex_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	return result;
}

void reshade::vulkan::convert_usage_to_image_usage_flags(resource_usage usage, VkImageUsageFlags &image_flags)
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
static inline void convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, resource_usage &usage)
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
void reshade::vulkan::convert_usage_to_buffer_usage_flags(resource_usage usage, VkBufferUsageFlags &buffer_flags)
{
	if ((usage & resource_usage::index_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if ((usage & resource_usage::vertex_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if ((usage & resource_usage::constant_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	if ((usage & resource_usage::unordered_access) != 0)
		buffer_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if ((usage & resource_usage::copy_dest) != 0)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if ((usage & resource_usage::copy_source) != 0)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
}
static inline void convert_buffer_usage_flags_to_usage(const VkBufferUsageFlags buffer_flags, resource_usage &usage)
{
	if ((buffer_flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
		usage |= resource_usage::index_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
		usage |= resource_usage::vertex_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
		usage |= resource_usage::constant_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
		usage |= resource_usage::unordered_access;
	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= resource_usage::copy_dest;
	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= resource_usage::copy_source;
}

void reshade::vulkan::convert_resource_desc(const resource_desc &desc, VkBufferCreateInfo &create_info)
{
	create_info.size = desc.width | (static_cast<uint64_t>(desc.height) << 32);
	convert_usage_to_buffer_usage_flags(desc.usage, create_info.usage);
}
resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	resource_desc desc = {};
	desc.width = create_info.size & 0xFFFFFFFF;
	desc.height = (create_info.size >> 32) & 0xFFFFFFFF;
	convert_buffer_usage_flags_to_usage(create_info.usage, desc.usage);
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
		assert(create_info.arrayLayers == 1);
		break;
	}

	desc.levels = static_cast<uint16_t>(create_info.mipLevels);
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.samples = static_cast<uint16_t>(create_info.samples);

	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	if (type == resource_type::texture_2d && (
		create_info.usage & (desc.samples > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
		desc.usage |= desc.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;

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
void reshade::vulkan::convert_resource_view_desc(const resource_view_desc &desc, VkBufferViewCreateInfo &create_info)
{
	assert(desc.dimension == resource_view_dimension::buffer);
	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.offset = desc.first_level | (static_cast<uint64_t>(desc.first_layer) << 32);
	create_info.range = desc.levels | (static_cast<uint64_t>(desc.layers) << 32);
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
resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkBufferViewCreateInfo &create_info)
{
	resource_view_desc desc = { resource_view_dimension::buffer };
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.first_level = create_info.offset & 0xFFFFFFFF;
	desc.first_layer = (create_info.offset >> 32) & 0xFFFFFFFF;
	desc.levels = create_info.range & 0xFFFFFFFF;
	desc.layers = (create_info.range >> 32) & 0xFFFFFFFF;
	return desc;
}
