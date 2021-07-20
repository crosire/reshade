/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::vulkan
{
	auto convert_format(api::format format) -> VkFormat;
	auto convert_format(VkFormat vk_format) -> api::format;

	auto convert_access_to_usage(VkAccessFlags flags) -> api::resource_usage;
	auto convert_image_layout_to_usage(VkImageLayout layout) -> api::resource_usage;
	void convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, api::resource_usage &usage);
	void convert_buffer_usage_flags_to_usage(const VkBufferUsageFlags buffer_flags, api::resource_usage &usage);

	auto convert_usage_to_access(api::resource_usage state) -> VkAccessFlags;
	auto convert_usage_to_image_layout(api::resource_usage state) -> VkImageLayout;
	auto convert_usage_to_pipeline_stage(api::resource_usage state, bool src_stage, const VkPhysicalDeviceFeatures &enabled_features) -> VkPipelineStageFlags;

	void convert_usage_to_image_usage_flags(api::resource_usage usage, VkImageUsageFlags &image_flags);
	void convert_usage_to_buffer_usage_flags(api::resource_usage usage, VkBufferUsageFlags &buffer_flags);

	void convert_sampler_desc(const api::sampler_desc &desc, VkSamplerCreateInfo &create_info);
	api::sampler_desc convert_sampler_desc(const VkSamplerCreateInfo &create_info);

	void convert_resource_desc(const api::resource_desc &desc, VkImageCreateInfo &create_info);
	void convert_resource_desc(const api::resource_desc &desc, VkBufferCreateInfo &create_info);
	api::resource_desc convert_resource_desc(const VkImageCreateInfo &create_info);
	api::resource_desc convert_resource_desc(const VkBufferCreateInfo &create_info);

	void convert_resource_view_desc(const api::resource_view_desc &desc, VkImageViewCreateInfo &create_info);
	void convert_resource_view_desc(const api::resource_view_desc &desc, VkBufferViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkImageViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkBufferViewCreateInfo &create_info);

	auto convert_logic_op(api::logic_op value) -> VkLogicOp;
	auto convert_logic_op(VkLogicOp value) -> api::logic_op;
	auto convert_blend_op(api::blend_op value) -> VkBlendOp;
	auto convert_blend_op(VkBlendOp value) -> api::blend_op;
	auto convert_blend_factor(api::blend_factor value) -> VkBlendFactor;
	auto convert_blend_factor(VkBlendFactor value) -> api::blend_factor;
	auto convert_fill_mode(api::fill_mode value) -> VkPolygonMode;
	auto convert_fill_mode(VkPolygonMode value) -> api::fill_mode;
	auto convert_cull_mode(api::cull_mode value) -> VkCullModeFlags;
	auto convert_cull_mode(VkCullModeFlags value) -> api::cull_mode;
	auto convert_compare_op(api::compare_op value) -> VkCompareOp;
	auto convert_compare_op(VkCompareOp value) -> api::compare_op;
	auto convert_stencil_op(api::stencil_op value) -> VkStencilOp;
	auto convert_stencil_op(VkStencilOp value) -> api::stencil_op;
	auto convert_primitive_topology(api::primitive_topology value) -> VkPrimitiveTopology;
	auto convert_primitive_topology(VkPrimitiveTopology value) -> api::primitive_topology;
	auto convert_query_type(api::query_type value) -> VkQueryType;
}
