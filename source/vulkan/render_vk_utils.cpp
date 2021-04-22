/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"

using namespace reshade::api;

const reshade::api::pipeline_state reshade::vulkan::pipeline_states_graphics[] = {
	// VkGraphicsPipelineCreateInfo::pInputAssemblyState::topology
	reshade::api::pipeline_state::primitive_topology,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthClampEnable
	reshade::api::pipeline_state::depth_clip,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::polygonMode
	reshade::api::pipeline_state::fill_mode,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::cullMode
	reshade::api::pipeline_state::cull_mode,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::frontFace
	reshade::api::pipeline_state::front_face_ccw,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasConstantFactor
	reshade::api::pipeline_state::depth_bias,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasClamp
	reshade::api::pipeline_state::depth_bias_clamp,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasSlopeFactor
	reshade::api::pipeline_state::depth_bias_slope_scaled,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::sampleShadingEnable
	reshade::api::pipeline_state::multisample,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::pSampleMask[0]
	reshade::api::pipeline_state::sample_mask,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::alphaToCoverageEnable
	reshade::api::pipeline_state::sample_alpha_to_coverage,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthTestEnable
	reshade::api::pipeline_state::depth_test,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthWriteEnable
	reshade::api::pipeline_state::depth_write_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthCompareOp
	reshade::api::pipeline_state::depth_func,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::stencilTestEnable
	reshade::api::pipeline_state::stencil_test,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::failOp
	reshade::api::pipeline_state::stencil_front_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::passOp
	reshade::api::pipeline_state::stencil_front_pass,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::depthFailOp
	reshade::api::pipeline_state::stencil_front_depth_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::compareOp
	reshade::api::pipeline_state::stencil_front_func,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::compareMask
	reshade::api::pipeline_state::stencil_read_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::writeMask
	reshade::api::pipeline_state::stencil_write_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::reference
	reshade::api::pipeline_state::stencil_ref,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::failOp
	reshade::api::pipeline_state::stencil_back_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::passOp
	reshade::api::pipeline_state::stencil_back_pass,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::depthFailOp
	reshade::api::pipeline_state::stencil_back_depth_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::compareOp
	reshade::api::pipeline_state::stencil_back_func,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::blendEnable
	reshade::api::pipeline_state::blend,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::srcColorBlendFactor
	reshade::api::pipeline_state::blend_color_src,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::dstColorBlendFactor
	reshade::api::pipeline_state::blend_color_dest,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::colorBlendOp
	reshade::api::pipeline_state::blend_color_op,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::srcAlphaBlendFactor
	reshade::api::pipeline_state::blend_alpha_src,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::dstAlphaBlendFactor
	reshade::api::pipeline_state::blend_alpha_dest,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::alphaBlendOp
	reshade::api::pipeline_state::blend_alpha_op,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::colorWriteMask
	reshade::api::pipeline_state::render_target_write_mask,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::blendConstants
	reshade::api::pipeline_state::blend_factor,
};

void reshade::vulkan::fill_pipeline_state_values(const VkGraphicsPipelineCreateInfo &create_info, uint32_t (&values)[35])
{
	if (create_info.pInputAssemblyState != nullptr)
	{
		const VkPipelineInputAssemblyStateCreateInfo &info = *create_info.pInputAssemblyState;
		values[0] = info.topology;
	}
	else
	{
		values[0] = 0;
	}

	if (create_info.pRasterizationState != nullptr)
	{
		const VkPipelineRasterizationStateCreateInfo &info = *create_info.pRasterizationState;
		values[1] = info.depthClampEnable;
		values[2] = info.polygonMode;
		values[3] = info.cullMode;
		values[4] = info.frontFace;
		values[5] = static_cast<uint32_t>(static_cast<int32_t>(info.depthBiasConstantFactor));
		values[6] = *reinterpret_cast<const uint32_t *>(&info.depthBiasClamp);
		values[7] = *reinterpret_cast<const uint32_t *>(&info.depthBiasSlopeFactor);
	}
	else
	{
		assert(false);
	}

	if (create_info.pMultisampleState != nullptr)
	{
		const VkPipelineMultisampleStateCreateInfo &info = *create_info.pMultisampleState;
		values[8] = info.sampleShadingEnable;
		values[9] = info.pSampleMask[0];
		values[10] = info.alphaToCoverageEnable;
	}
	else
	{
		values[8] = values[9] = values[10] = 0;
	}

	if (create_info.pDepthStencilState != nullptr)
	{
		const VkPipelineDepthStencilStateCreateInfo &info = *create_info.pDepthStencilState;
		values[11] = info.depthTestEnable;
		values[12] = info.depthWriteEnable;
		values[13] = info.depthCompareOp;
		values[14] = info.stencilTestEnable;
		values[15] = info.front.failOp;
		values[16] = info.front.passOp;
		values[17] = info.front.depthFailOp;
		values[18] = info.front.compareOp;
		values[19] = info.front.compareMask;
		values[20] = info.front.writeMask;
		values[21] = info.front.reference;
		values[22] = info.back.failOp;
		values[23] = info.back.passOp;
		values[24] = info.back.depthFailOp;
		values[25] = info.back.compareOp;
	}
	else
	{
		values[11] = VK_FALSE;
		values[12] = 0;
		values[13] = 0;
		values[14] = VK_FALSE;
		values[15] = values[16] = values[17] = VK_STENCIL_OP_KEEP;
		values[18] = VK_COMPARE_OP_ALWAYS;
		values[19] = 0xFFFFFFFF;
		values[20] = 0xFFFFFFFF;
		values[21] = 0xFFFFFFFF;
		values[22] = values[23] = values[24] = VK_STENCIL_OP_KEEP;
		values[25] = VK_COMPARE_OP_ALWAYS;
	}

	if (create_info.pColorBlendState != nullptr &&
		create_info.pColorBlendState->pAttachments != nullptr)
	{
		const VkPipelineColorBlendStateCreateInfo &info = *create_info.pColorBlendState;
		values[26] = info.pAttachments[0].blendEnable;
		values[27] = info.pAttachments[0].srcColorBlendFactor;
		values[28] = info.pAttachments[0].dstColorBlendFactor;
		values[29] = info.pAttachments[0].colorBlendOp;
		values[30] = info.pAttachments[0].srcAlphaBlendFactor;
		values[31] = info.pAttachments[0].dstAlphaBlendFactor;
		values[32] = info.pAttachments[0].alphaBlendOp;
		values[33] = info.pAttachments[0].colorWriteMask;
		values[34] =
			((static_cast<uint32_t>(info.blendConstants[0] * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(info.blendConstants[1] * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(info.blendConstants[2] * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(info.blendConstants[3] * 255.f) & 0xFF) << 24);
	}
	else
	{
		values[26] = VK_FALSE;
		values[27] = VK_BLEND_FACTOR_ONE;
		values[28] = VK_BLEND_FACTOR_ZERO;
		values[29] = VK_BLEND_OP_ADD;
		values[30] = VK_BLEND_FACTOR_ONE;
		values[31] = VK_BLEND_FACTOR_ZERO;
		values[32] = VK_BLEND_OP_ADD;
		values[33] = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		values[34] = 0xFFFFFFFF;
	}
}

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

void reshade::vulkan::convert_sampler_desc(const sampler_desc &desc, VkSamplerCreateInfo &create_info)
{
	switch (desc.filter)
	{
	case texture_filter::min_mag_mip_point:
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_point_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_point_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_linear_mag_mip_point:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_linear_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::min_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case texture_filter::anisotropic:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_TRUE;
		break;
	}

	const auto convert_address_mode = [](texture_address_mode mode) {
		switch (mode)
		{
		default:
		case texture_address_mode::wrap:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case texture_address_mode::mirror:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case texture_address_mode::clamp:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case texture_address_mode::border:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case texture_address_mode::mirror_once:
			return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		}
	};

	create_info.addressModeU = convert_address_mode(desc.address_u);
	create_info.addressModeV = convert_address_mode(desc.address_v);
	create_info.addressModeW = convert_address_mode(desc.address_w);
	create_info.mipLodBias = desc.mip_lod_bias;
	create_info.maxAnisotropy = desc.max_anisotropy;
	create_info.compareEnable = VK_FALSE;
	create_info.compareOp = VK_COMPARE_OP_ALWAYS;
	create_info.minLod = desc.min_lod;
	create_info.maxLod = desc.max_lod;
}
sampler_desc reshade::vulkan::convert_sampler_desc(const VkSamplerCreateInfo &create_info)
{
	sampler_desc desc = {};
	if (create_info.anisotropyEnable)
	{
		desc.filter = texture_filter::anisotropic;
	}
	else
	{
		switch (create_info.minFilter)
		{
		case VK_FILTER_NEAREST:
			switch (create_info.magFilter)
			{
			case VK_FILTER_NEAREST:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = texture_filter::min_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = texture_filter::min_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = texture_filter::min_point_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = texture_filter::min_point_mag_mip_linear;
					break;
				}
				break;
			}
			break;
		case VK_FILTER_LINEAR:
			switch (create_info.magFilter)
			{
			case VK_FILTER_NEAREST:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = texture_filter::min_linear_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = texture_filter::min_linear_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = texture_filter::min_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = texture_filter::min_mag_mip_linear;
					break;
				}
				break;
			}
			break;
		}
	}

	const auto convert_address_mode = [](VkSamplerAddressMode mode) {
		switch (mode)
		{
		default:
		case VK_SAMPLER_ADDRESS_MODE_REPEAT:
			return texture_address_mode::wrap;
		case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return texture_address_mode::mirror;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return texture_address_mode::clamp;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			return texture_address_mode::border;
		case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
			return texture_address_mode::mirror_once;
		}
	};

	desc.address_u = convert_address_mode(create_info.addressModeU);
	desc.address_v = convert_address_mode(create_info.addressModeV);
	desc.address_w = convert_address_mode(create_info.addressModeW);
	desc.mip_lod_bias = create_info.mipLodBias;
	desc.max_anisotropy = create_info.maxAnisotropy;
	desc.min_lod = create_info.minLod;
	desc.max_lod = create_info.maxLod;
	return desc;
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
void reshade::vulkan::convert_resource_desc(const resource_desc &desc, VkBufferCreateInfo &create_info)
{
	assert(desc.type == resource_type::buffer);
	create_info.size = desc.size;
	convert_usage_to_buffer_usage_flags(desc.usage, create_info.usage);
}
resource_desc reshade::vulkan::convert_resource_desc(const VkImageCreateInfo &create_info)
{
	resource_desc desc = {};
	switch (create_info.imageType)
	{
	default:
		assert(false);
		desc.type = resource_type::unknown;
		break;
	case VK_IMAGE_TYPE_1D:
		desc.type = resource_type::texture_1d;
		desc.width = create_info.extent.width;
		assert(create_info.extent.height == 1 && create_info.extent.depth == 1);
		desc.height = 1;
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_2D:
		desc.type = resource_type::texture_2d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.height;
		assert(create_info.extent.depth == 1);
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_3D:
		desc.type = resource_type::texture_3d;
		desc.width = create_info.extent.width;
		desc.height = create_info.extent.height;
		assert(create_info.extent.depth <= std::numeric_limits<uint16_t>::max());
		desc.depth_or_layers = static_cast<uint16_t>(create_info.extent.depth);
		assert(create_info.arrayLayers == 1);
		break;
	}

	assert(create_info.mipLevels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(create_info.mipLevels);
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.samples = static_cast<uint16_t>(create_info.samples);

	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	if (desc.type == resource_type::texture_2d && (
		create_info.usage & (desc.samples > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
		desc.usage |= desc.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;

	return desc;
}
resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.size = create_info.size;
	convert_buffer_usage_flags_to_usage(create_info.usage, desc.usage);
	return desc;
}

void reshade::vulkan::convert_resource_view_desc(const resource_view_desc &desc, VkImageViewCreateInfo &create_info)
{
	switch (desc.type)
	{
	default:
		assert(false);
		break;
	case resource_view_type::texture_1d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case resource_view_type::texture_1d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case resource_view_type::texture_2d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case resource_view_type::texture_2d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case resource_view_type::texture_3d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case resource_view_type::texture_cube:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case resource_view_type::texture_cube_array:
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
	assert(desc.type == resource_view_type::buffer);

	create_info.format = static_cast<VkFormat>(desc.format);
	create_info.offset = desc.offset;
	create_info.range = desc.size;
}
resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkImageViewCreateInfo &create_info)
{
	resource_view_desc desc = {};
	switch (create_info.viewType)
	{
	default:
		assert(false);
		desc.type = resource_view_type::unknown;
		break;
	case VK_IMAGE_VIEW_TYPE_1D:
		desc.type = resource_view_type::texture_1d;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		desc.type = resource_view_type::texture_1d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
		desc.type = resource_view_type::texture_2d;
		break;
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		desc.type = resource_view_type::texture_2d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		desc.type = resource_view_type::texture_3d;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		desc.type = resource_view_type::texture_cube;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		desc.type = resource_view_type::texture_cube_array;
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
	resource_view_desc desc = {};
	desc.type = resource_view_type::buffer;
	desc.format = static_cast<uint32_t>(create_info.format);
	desc.offset = create_info.offset;
	desc.size = create_info.sType;

	return desc;
}
