/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>
#include <unordered_map>

namespace reshade::vulkan
{
	template <VkObjectType type>
	struct object_data;

	template <>
	struct object_data<VK_OBJECT_TYPE_IMAGE>
	{
		using Handle = VkImage;

		VmaAllocation allocation;
#ifndef WIN64
		uint32_t padding;
#endif
		VkImageCreateInfo create_info;
		VkImageView default_view = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t memory_offset = 0;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_BUFFER>
	{
		using Handle = VkBuffer;

		VmaAllocation allocation;
#ifndef WIN64
		uint32_t padding;
#endif
		VkBufferCreateInfo create_info;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		uint64_t memory_offset = 0;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_IMAGE_VIEW>
	{
		using Handle = VkImageView;

		VkImageViewCreateInfo create_info;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_BUFFER_VIEW>
	{
		using Handle = VkBufferView;

		VkBufferViewCreateInfo create_info;
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

		struct attachment
		{
			VkImageLayout initial_layout;
			VkImageAspectFlags clear_flags;
			VkImageAspectFlags format_flags;
		};

		std::vector<attachment> attachments;
		VkSampleCountFlagBits samples;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_FRAMEBUFFER>
	{
		using Handle = VkFramebuffer;

		VkExtent2D area;
		std::vector<VkImageView> attachments;
		std::vector<VkImageAspectFlags> attachment_types;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_PIPELINE_LAYOUT>
	{
		using Handle = VkPipelineLayout;

		std::vector<api::pipeline_layout_param> params;
		uint32_t num_sets;
	};

	template <>
	struct object_data<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
	{
		using Handle = VkDescriptorSetLayout;

		uint32_t num_descriptors;
		std::vector<api::descriptor_range> ranges;
		std::unordered_map<uint32_t, uint32_t> binding_to_offset;
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

	void convert_dynamic_states(const VkPipelineDynamicStateCreateInfo &create_info, std::vector<api::dynamic_state> &states);

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
	auto convert_descriptor_type(api::descriptor_type value, bool is_image) -> VkDescriptorType;
	auto convert_descriptor_type(VkDescriptorType value) -> api::descriptor_type;
	auto convert_attachment_type(api::attachment_type value) -> VkImageAspectFlags;
	auto convert_attachment_load_op(api::attachment_load_op value) -> VkAttachmentLoadOp;
	auto convert_attachment_load_op(VkAttachmentLoadOp value) -> api::attachment_load_op;
	auto convert_attachment_store_op(api::attachment_store_op value) -> VkAttachmentStoreOp;
	auto convert_attachment_store_op(VkAttachmentStoreOp value) -> api::attachment_store_op;
}
