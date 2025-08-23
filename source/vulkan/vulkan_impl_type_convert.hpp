/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <glad/vulkan.h>
#include "reshade_api_pipeline.hpp"
#include <vector>
#include <unordered_map>
#include <limits>

#ifndef AMD_VULKAN_MEMORY_ALLOCATOR_H
using VmaAllocation = void *;
using VmaPool = void *;
#endif

namespace reshade::vulkan
{
	static_assert(sizeof(VkBuffer) == sizeof(api::resource));
	static_assert(sizeof(VkBufferView) == sizeof(api::resource_view));
	static_assert(sizeof(VkViewport) == sizeof(api::viewport));
	static_assert(sizeof(VkDescriptorSet) == sizeof(api::descriptor_table));
	static_assert(sizeof(VkDescriptorBufferInfo) == sizeof(api::buffer_range));
	static_assert(sizeof(VkAccelerationStructureKHR) == sizeof(api::resource_view));
	static_assert(sizeof(VkAccelerationStructureInstanceKHR) == sizeof(api::acceleration_structure_instance));

	template <VkObjectType type>
	struct object_data;

	template <>
	struct object_data<VK_OBJECT_TYPE_IMAGE>
	{
		using Handle = VkImage;

		VmaAllocation allocation = nullptr;
		VmaPool pool = nullptr;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t memory_offset = 0;
		VkImageCreateInfo create_info;
		VkImageView default_view = VK_NULL_HANDLE;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_BUFFER>
	{
		using Handle = VkBuffer;

		VmaAllocation allocation = nullptr;
		VmaPool pool = nullptr;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t memory_offset = 0;
		VkBufferCreateInfo create_info;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_IMAGE_VIEW>
	{
		using Handle = VkImageView;

		VkImageViewCreateInfo create_info;
		// Keep track of image extent to avoid common extra lookup of view
		VkExtent3D image_extent;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_BUFFER_VIEW>
	{
		using Handle = VkBufferView;

		VkBufferViewCreateInfo create_info;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_SAMPLER>
	{
		using Handle = VkSampler;

		VkSamplerCreateInfo create_info;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_SHADER_MODULE>
	{
		using Handle = VkShaderModule;

		std::vector<uint8_t> spirv;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_RENDER_PASS>
	{
		using Handle = VkRenderPass;

		struct subpass
		{
			uint32_t color_attachments[8];
			uint32_t num_color_attachments;
			uint32_t depth_stencil_attachment;
		};

		std::vector<subpass> subpasses;
		std::vector<VkAttachmentDescription> attachments;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_FRAMEBUFFER>
	{
		using Handle = VkFramebuffer;

		std::vector<VkImageView> attachments;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT>
	{
		using Handle = VkPipelineLayout;

		std::vector<VkDescriptorSetLayout> set_layouts;
		std::vector<VkSampler> embedded_samplers;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
	{
		using Handle = VkDescriptorSetLayout;

		uint32_t num_descriptors;
		std::vector<api::descriptor_range> ranges;
		std::vector<api::descriptor_range_with_static_samplers> ranges_with_static_samplers;
		std::vector<std::vector<api::sampler_desc>> static_samplers;
		std::vector<uint32_t> binding_to_offset;
		bool push_descriptors;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET>
	{
		using Handle = VkDescriptorSet;

		VkDescriptorPool pool;
		uint32_t offset;
		VkDescriptorSetLayout layout;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_DESCRIPTOR_POOL>
	{
		using Handle = VkDescriptorPool;

		uint32_t max_sets;
		uint32_t max_descriptors;
		uint32_t next_set;
		uint32_t next_offset;
		std::vector<object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET>> sets;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>
	{
		using Handle = VkDescriptorUpdateTemplate;

		VkPipelineBindPoint bind_point;
		std::vector<VkDescriptorUpdateTemplateEntry> entries;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_QUERY_POOL>
	{
		using Handle = VkQueryPool;

		VkQueryType type;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR>
	{
		using Handle = VkAccelerationStructureKHR;

		VkAccelerationStructureCreateInfoKHR create_info;
	};

	auto convert_format(api::format format, VkComponentMapping *components = nullptr) -> VkFormat;
	auto convert_format(VkFormat vk_format, const VkComponentMapping *components = nullptr) -> api::format;

	auto convert_color_space(api::color_space color_space) -> VkColorSpaceKHR;
	auto convert_color_space(VkColorSpaceKHR color_space) -> api::color_space;

	inline VkImageAspectFlags aspect_flags_from_format(VkFormat format)
	{
		if (format >= VK_FORMAT_D16_UNORM && format <= VK_FORMAT_D32_SFLOAT)
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		if (format == VK_FORMAT_S8_UINT)
			return VK_IMAGE_ASPECT_STENCIL_BIT;
		if (format >= VK_FORMAT_D16_UNORM_S8_UINT && format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}

	auto convert_access_to_usage(VkAccessFlags2 flags) -> api::resource_usage;
	auto convert_image_layout_to_usage(VkImageLayout layout) -> api::resource_usage;
	void convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, api::resource_usage &usage);
	void convert_buffer_usage_flags_to_usage(const VkBufferUsageFlags2 buffer_flags, api::resource_usage &usage);

	auto convert_usage_to_access(api::resource_usage state) -> VkAccessFlags;
	auto convert_usage_to_image_layout(api::resource_usage state) -> VkImageLayout;
	auto convert_usage_to_pipeline_stage(api::resource_usage state, bool src_stage, const VkPhysicalDeviceFeatures &enabled_features, bool enabled_ray_tracing) -> VkPipelineStageFlags;

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
	void convert_resource_view_desc(const api::resource_view_desc &desc, VkAccelerationStructureCreateInfoKHR &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkImageViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkBufferViewCreateInfo &create_info);
	api::resource_view_desc convert_resource_view_desc(const VkAccelerationStructureCreateInfoKHR &create_info);

	void convert_dynamic_states(uint32_t count, const api::dynamic_state *states, std::vector<VkDynamicState> &internal_states);
	std::vector<api::dynamic_state> convert_dynamic_states(const VkPipelineDynamicStateCreateInfo *create_info);

	void convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<VkVertexInputBindingDescription> &vertex_bindings, std::vector<VkVertexInputAttributeDescription> &vertex_attributes);
	std::vector<api::input_element> convert_input_layout_desc(const VkPipelineVertexInputStateCreateInfo *create_info);

	void convert_stream_output_desc(const api::stream_output_desc &desc, VkPipelineRasterizationStateCreateInfo &create_info);
	api::stream_output_desc convert_stream_output_desc(const VkPipelineRasterizationStateCreateInfo *create_info);
	void convert_blend_desc(const api::blend_desc &desc, VkPipelineColorBlendStateCreateInfo &create_info, VkPipelineMultisampleStateCreateInfo &multisample_create_info);
	api::blend_desc convert_blend_desc(const VkPipelineColorBlendStateCreateInfo *create_info, const VkPipelineMultisampleStateCreateInfo *multisample_create_info);
	void convert_rasterizer_desc(const api::rasterizer_desc &desc, VkPipelineRasterizationStateCreateInfo &create_info);
	api::rasterizer_desc convert_rasterizer_desc(const VkPipelineRasterizationStateCreateInfo *create_info, const VkPipelineMultisampleStateCreateInfo *multisample_create_info = nullptr);
	void convert_depth_stencil_desc(const api::depth_stencil_desc &desc, VkPipelineDepthStencilStateCreateInfo &create_info);
	api::depth_stencil_desc convert_depth_stencil_desc(const VkPipelineDepthStencilStateCreateInfo *create_info);

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
	auto convert_query_type(VkQueryType value, uint32_t index = 0) -> api::query_type;

	auto convert_descriptor_type(api::descriptor_type value) -> VkDescriptorType;
	auto convert_descriptor_type(VkDescriptorType value) -> api::descriptor_type;

	auto convert_render_pass_load_op(api::render_pass_load_op value) -> VkAttachmentLoadOp;
	auto convert_render_pass_load_op(VkAttachmentLoadOp value) -> api::render_pass_load_op;
	auto convert_render_pass_store_op(api::render_pass_store_op value) -> VkAttachmentStoreOp;
	auto convert_render_pass_store_op(VkAttachmentStoreOp value) -> api::render_pass_store_op;

	auto convert_pipeline_flags(api::pipeline_flags value) -> VkPipelineCreateFlags;
	auto convert_pipeline_flags(VkPipelineCreateFlags2 value) -> api::pipeline_flags;
	auto convert_shader_group_type(api::shader_group_type value) -> VkRayTracingShaderGroupTypeKHR;
	auto convert_shader_group_type(VkRayTracingShaderGroupTypeKHR value) -> api::shader_group_type;
	auto convert_acceleration_structure_type(api::acceleration_structure_type value) -> VkAccelerationStructureTypeKHR;
	auto convert_acceleration_structure_type(VkAccelerationStructureTypeKHR value) -> api::acceleration_structure_type;
	auto convert_acceleration_structure_copy_mode(api::acceleration_structure_copy_mode value) -> VkCopyAccelerationStructureModeKHR;
	auto convert_acceleration_structure_copy_mode(VkCopyAccelerationStructureModeKHR value) -> api::acceleration_structure_copy_mode;
	auto convert_acceleration_structure_build_flags(api::acceleration_structure_build_flags value) -> VkBuildAccelerationStructureFlagsKHR;
	auto convert_acceleration_structure_build_flags(VkBuildAccelerationStructureFlagsKHR value) -> api::acceleration_structure_build_flags;

	void convert_acceleration_structure_build_input(const api::acceleration_structure_build_input &build_input, VkAccelerationStructureGeometryKHR &geometry, VkAccelerationStructureBuildRangeInfoKHR &range_info);
	api::acceleration_structure_build_input convert_acceleration_structure_build_input(const VkAccelerationStructureGeometryKHR &geometry, const VkAccelerationStructureBuildRangeInfoKHR &range_info);

	auto convert_shader_stages(VkPipelineBindPoint value) -> api::shader_stage;
	auto convert_pipeline_stages(api::pipeline_stage value) -> VkPipelineBindPoint;
	auto convert_pipeline_stages(VkPipelineBindPoint value) -> api::pipeline_stage;
}
