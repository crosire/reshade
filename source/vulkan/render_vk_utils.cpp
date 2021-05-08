/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_vk.hpp"
#include "render_vk_utils.hpp"

using namespace reshade::api;

const pipeline_state reshade::vulkan::pipeline_states_graphics[] = {
	// VkGraphicsPipelineCreateInfo::pInputAssemblyState::topology
	pipeline_state::primitive_topology,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthClampEnable
	pipeline_state::depth_clip,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::polygonMode
	pipeline_state::fill_mode,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::cullMode
	pipeline_state::cull_mode,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::frontFace
	pipeline_state::front_face_ccw,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasConstantFactor
	pipeline_state::depth_bias,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasClamp
	pipeline_state::depth_bias_clamp,
	// VkGraphicsPipelineCreateInfo::pRasterizationState::depthBiasSlopeFactor
	pipeline_state::depth_bias_slope_scaled,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::sampleShadingEnable
	pipeline_state::multisample,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::pSampleMask[0]
	pipeline_state::sample_mask,
	// VkGraphicsPipelineCreateInfo::pMultisampleState::alphaToCoverageEnable
	pipeline_state::alpha_to_coverage,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthTestEnable
	pipeline_state::depth_test,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthWriteEnable
	pipeline_state::depth_write_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::depthCompareOp
	pipeline_state::depth_func,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::stencilTestEnable
	pipeline_state::stencil_test,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::failOp
	pipeline_state::front_stencil_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::passOp
	pipeline_state::front_stencil_pass,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::depthFailOp
	pipeline_state::front_stencil_depth_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::compareOp
	pipeline_state::front_stencil_func,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::compareMask
	pipeline_state::stencil_read_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::writeMask
	pipeline_state::stencil_write_mask,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::front::reference
	pipeline_state::stencil_reference_value,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::failOp
	pipeline_state::back_stencil_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::passOp
	pipeline_state::back_stencil_pass,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::depthFailOp
	pipeline_state::back_stencil_depth_fail,
	// VkGraphicsPipelineCreateInfo::pDepthStencilState::back::compareOp
	pipeline_state::back_stencil_func,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::blendEnable
	pipeline_state::blend_enable,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::srcColorBlendFactor
	pipeline_state::src_color_blend_factor,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::dstColorBlendFactor
	pipeline_state::dst_color_blend_factor,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::colorBlendOp
	pipeline_state::color_blend_op,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::srcAlphaBlendFactor
	pipeline_state::src_alpha_blend_factor,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::dstAlphaBlendFactor
	pipeline_state::dst_alpha_blend_factor,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::alphaBlendOp
	pipeline_state::alpha_blend_op,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::pAttachments[0]::colorWriteMask
	pipeline_state::render_target_write_mask,
	// VkGraphicsPipelineCreateInfo::pColorBlendState::blendConstants
	pipeline_state::blend_constant,
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

void reshade::vulkan::convert_format_to_vk_format(format format, VkFormat &vk_format)
{
	switch (format)
	{
	default:
	case format::unknown:
		break;
	case format::r1_unorm:
		// Unsupported
		break;
	case format::r8_uint:
		vk_format = VK_FORMAT_R8_UINT;
		break;
	case format::r8_sint:
		vk_format = VK_FORMAT_R8_SINT;
		break;
	case format::r8_typeless:
	case format::r8_unorm:
	case format::a8_unorm: // { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R }
		vk_format = VK_FORMAT_R8_UNORM;
		break;
	case format::r8_snorm:
		vk_format = VK_FORMAT_R8_SNORM;
		break;
	case format::r8g8_uint:
		vk_format = VK_FORMAT_R8G8_UINT;
		break;
	case format::r8g8_sint:
		vk_format = VK_FORMAT_R8G8_SINT;
		break;
	case format::r8g8_typeless:
	case format::r8g8_unorm:
		vk_format = VK_FORMAT_R8G8_UNORM;
		break;
	case format::r8g8_snorm:
		vk_format = VK_FORMAT_R8G8_SNORM;
		break;
	case format::r8g8b8a8_uint:
		vk_format = VK_FORMAT_R8G8B8A8_UINT;
		break;
	case format::r8g8b8a8_sint:
		vk_format = VK_FORMAT_R8G8B8A8_SINT;
		break;
	case format::r8g8b8a8_typeless:
	case format::r8g8b8a8_unorm:
		vk_format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	case format::r8g8b8a8_unorm_srgb:
		vk_format = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	case format::r8g8b8a8_snorm:
		vk_format = VK_FORMAT_R8G8B8A8_SNORM;
		break;
	case format::b8g8r8a8_typeless:
	case format::b8g8r8a8_unorm:
	case format::b8g8r8x8_typeless: // { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }
	case format::b8g8r8x8_unorm:
		vk_format = VK_FORMAT_B8G8R8A8_UNORM;
		break;
	case format::b8g8r8a8_unorm_srgb:
	case format::b8g8r8x8_unorm_srgb:
		vk_format = VK_FORMAT_B8G8R8A8_SRGB;
		break;
	case format::r10g10b10a2_uint:
		vk_format = VK_FORMAT_A2B10G10R10_UINT_PACK32;
		break;
	case format::r10g10b10a2_typeless:
	case format::r10g10b10a2_unorm:
		vk_format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		break;
	case format::r10g10b10a2_xr_bias:
		// Unsupported
		break;
	case format::r16_sint:
		vk_format = VK_FORMAT_R16_SINT;
		break;
	case format::r16_uint:
		vk_format = VK_FORMAT_R16_UINT;
		break;
	case format::r16_typeless:
	case format::r16_float:
		vk_format = VK_FORMAT_R16_SFLOAT;
		break;
	case format::r16_unorm:
		vk_format = VK_FORMAT_R16_UNORM;
		break;
	case format::r16_snorm:
		vk_format = VK_FORMAT_R16_SNORM;
		break;
	case format::r16g16_uint:
		vk_format = VK_FORMAT_R16G16_UINT;
		break;
	case format::r16g16_sint:
		vk_format = VK_FORMAT_R16G16_SINT;
		break;
	case format::r16g16_typeless:
	case format::r16g16_float:
		vk_format = VK_FORMAT_R16G16_SFLOAT;
		break;
	case format::r16g16_unorm:
		vk_format = VK_FORMAT_R16G16_UNORM;
		break;
	case format::r16g16_snorm:
		vk_format = VK_FORMAT_R16G16_SNORM;
		break;
	case format::r16g16b16a16_uint:
		vk_format = VK_FORMAT_R16G16B16A16_UINT;
		break;
	case format::r16g16b16a16_sint:
		vk_format = VK_FORMAT_R16G16B16A16_SINT;
		break;
	case format::r16g16b16a16_typeless:
	case format::r16g16b16a16_float:
		vk_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		break;
	case format::r16g16b16a16_unorm:
		vk_format = VK_FORMAT_R16G16B16A16_UNORM;
		break;
	case format::r16g16b16a16_snorm:
		vk_format = VK_FORMAT_R16G16B16A16_SNORM;
		break;
	case format::r32_uint:
		vk_format = VK_FORMAT_R32_UINT;
		break;
	case format::r32_sint:
		vk_format = VK_FORMAT_R32_SINT;
		break;
	case format::r32_typeless:
	case format::r32_float:
		vk_format = VK_FORMAT_R32_SFLOAT;
		break;
	case format::r32g32_uint:
		vk_format = VK_FORMAT_R32G32_UINT;
		break;
	case format::r32g32_sint:
		vk_format = VK_FORMAT_R32G32_SINT;
		break;
	case format::r32g32_typeless:
	case format::r32g32_float:
		vk_format = VK_FORMAT_R32G32_SFLOAT;
		break;
	case format::r32g32b32_uint:
		vk_format = VK_FORMAT_R32G32B32_UINT;
		break;
	case format::r32g32b32_sint:
		vk_format = VK_FORMAT_R32G32B32_SINT;
		break;
	case format::r32g32b32_typeless:
	case format::r32g32b32_float:
		vk_format = VK_FORMAT_R32G32B32_SFLOAT;
		break;
	case format::r32g32b32a32_uint:
		vk_format = VK_FORMAT_R32G32B32A32_UINT;
		break;
	case format::r32g32b32a32_sint:
		vk_format = VK_FORMAT_R32G32B32A32_SINT;
		break;
	case format::r32g32b32a32_typeless:
	case format::r32g32b32a32_float:
		vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
		break;
	case format::r9g9b9e5:
		vk_format = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
		break;
	case format::r11g11b10_float:
		vk_format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		break;
	case format::b5g6r5_unorm:
		vk_format = VK_FORMAT_R5G6B5_UNORM_PACK16;
		break;
	case format::b5g5r5a1_unorm:
		vk_format = VK_FORMAT_A1R5G5B5_UNORM_PACK16;
		break;
#if 0
	case format::b4g4r4a4_unorm:
		vk_format = VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
		break;
#endif
	case format::s8_uint:
		vk_format = VK_FORMAT_S8_UINT;
		break;
	case format::d16_unorm:
		vk_format = VK_FORMAT_D16_UNORM;
		break;
	case format::d16_unorm_s8_uint:
		vk_format = VK_FORMAT_D16_UNORM_S8_UINT;
		break;
	case format::r24_g8_typeless:
	case format::r24_unorm_x8_uint:
	case format::x24_unorm_g8_uint:
	case format::d24_unorm_s8_uint:
		vk_format = VK_FORMAT_D24_UNORM_S8_UINT;
		break;
	case format::d32_float:
		vk_format = VK_FORMAT_D32_SFLOAT;
		break;
	case format::r32_g8_typeless:
	case format::r32_float_x8_uint:
	case format::x32_float_g8_uint:
	case format::d32_float_s8_uint:
		vk_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
		break;
	case format::bc1_typeless:
	case format::bc1_unorm:
		vk_format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		break;
	case format::bc1_unorm_srgb:
		vk_format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		break;
	case format::bc2_typeless:
	case format::bc2_unorm:
		vk_format = VK_FORMAT_BC2_UNORM_BLOCK;
		break;
	case format::bc2_unorm_srgb:
		vk_format = VK_FORMAT_BC2_SRGB_BLOCK;
		break;
	case format::bc3_typeless:
	case format::bc3_unorm:
		vk_format = VK_FORMAT_BC3_UNORM_BLOCK;
		break;
	case format::bc3_unorm_srgb:
		vk_format = VK_FORMAT_BC3_SRGB_BLOCK;
		break;
	case format::bc4_typeless:
	case format::bc4_unorm:
		vk_format = VK_FORMAT_BC4_UNORM_BLOCK;
		break;
	case format::bc4_snorm:
		vk_format = VK_FORMAT_BC4_SNORM_BLOCK;
		break;
	case format::bc5_typeless:
	case format::bc5_unorm:
		vk_format = VK_FORMAT_BC5_UNORM_BLOCK;
		break;
	case format::bc5_snorm:
		vk_format = VK_FORMAT_BC5_SNORM_BLOCK;
		break;
	case format::bc6h_typeless:
	case format::bc6h_ufloat:
		vk_format = VK_FORMAT_BC6H_UFLOAT_BLOCK;
		break;
	case format::bc6h_sfloat:
		vk_format = VK_FORMAT_BC6H_SFLOAT_BLOCK;
		break;
	case format::bc7_typeless:
	case format::bc7_unorm:
		vk_format = VK_FORMAT_BC7_UNORM_BLOCK;
		break;
	case format::bc7_unorm_srgb:
		vk_format = VK_FORMAT_BC7_SRGB_BLOCK;
		break;
	case format::r8g8_b8g8_unorm:
		vk_format = VK_FORMAT_B8G8R8G8_422_UNORM;
		break;
	case format::g8r8_g8b8_unorm:
		vk_format = VK_FORMAT_G8B8G8R8_422_UNORM;
		break;
	}
}
void reshade::vulkan::convert_vk_format_to_format(VkFormat vk_format, format &format)
{
	switch (vk_format)
	{
	default:
	case VK_FORMAT_UNDEFINED:
		format = format::unknown;
		break;
#if 0
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:
		format = format::b4g4r4a4_unorm;
		break;
#endif
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
		format = format::b5g6r5_unorm;
		break;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		format = format::b5g5r5a1_unorm;
		break;
	case VK_FORMAT_R8_UNORM:
		format = format::r8_unorm;
		break;
	case VK_FORMAT_R8_SNORM:
		format = format::r8_snorm;
		break;
	case VK_FORMAT_R8_UINT:
		format = format::r8_uint;
		break;
	case VK_FORMAT_R8_SINT:
		format = format::r8_sint;
		break;
	case VK_FORMAT_R8G8_UNORM:
		format = format::r8g8_unorm;
		break;
	case VK_FORMAT_R8G8_SNORM:
		format = format::r8g8_snorm;
		break;
	case VK_FORMAT_R8G8_UINT:
		format = format::r8g8_uint;
		break;
	case VK_FORMAT_R8G8_SINT:
		format = format::r8g8_sint;
		break;
	case VK_FORMAT_R8G8B8A8_UNORM:
		format = format::r8g8b8a8_unorm;
		break;
	case VK_FORMAT_R8G8B8A8_SNORM:
		format = format::r8g8b8a8_snorm;
		break;
	case VK_FORMAT_R8G8B8A8_UINT:
		format = format::r8g8b8a8_uint;
		break;
	case VK_FORMAT_R8G8B8A8_SINT:
		format = format::r8g8b8a8_sint;
		break;
	case VK_FORMAT_R8G8B8A8_SRGB:
		format = format::r8g8b8a8_unorm_srgb;
		break;
	case VK_FORMAT_B8G8R8A8_UNORM:
		format = format::b8g8r8a8_unorm;
		break;
	case VK_FORMAT_B8G8R8A8_SRGB:
		format = format::b8g8r8a8_unorm_srgb;
		break;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		format = format::r10g10b10a2_unorm;
		break;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		format = format::r10g10b10a2_uint;
		break;
	case VK_FORMAT_R16_UNORM:
		format = format::r16_unorm;
		break;
	case VK_FORMAT_R16_SNORM:
		format = format::r16_snorm;
		break;
	case VK_FORMAT_R16_UINT:
		format = format::r16_uint;
		break;
	case VK_FORMAT_R16_SINT:
		format = format::r16_sint;
		break;
	case VK_FORMAT_R16_SFLOAT:
		format = format::r16_float;
		break;
	case VK_FORMAT_R16G16_UNORM:
		format = format::r16g16_unorm;
		break;
	case VK_FORMAT_R16G16_SNORM:
		format = format::r16g16_snorm;
		break;
	case VK_FORMAT_R16G16_UINT:
		format = format::r16g16_uint;
		break;
	case VK_FORMAT_R16G16_SINT:
		format = format::r16g16_sint;
		break;
	case VK_FORMAT_R16G16_SFLOAT:
		format = format::r16g16_float;
		break;
	case VK_FORMAT_R16G16B16A16_UNORM:
		format = format::r16g16b16a16_unorm;
		break;
	case VK_FORMAT_R16G16B16A16_SNORM:
		format = format::r16g16b16a16_snorm;
		break;
	case VK_FORMAT_R16G16B16A16_UINT:
		format = format::r16g16b16a16_uint;
		break;
	case VK_FORMAT_R16G16B16A16_SINT:
		format = format::r16g16b16a16_sint;
		break;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		format = format::r16g16b16a16_float;
		break;
	case VK_FORMAT_R32_UINT:
		format = format::r32_uint;
		break;
	case VK_FORMAT_R32_SINT:
		format = format::r32_sint;
		break;
	case VK_FORMAT_R32_SFLOAT:
		format = format::r32_float;
		break;
	case VK_FORMAT_R32G32_UINT:
		format = format::r32g32_uint;
		break;
	case VK_FORMAT_R32G32_SINT:
		format = format::r32g32_sint;
		break;
	case VK_FORMAT_R32G32_SFLOAT:
		format = format::r32g32_float;
		break;
	case VK_FORMAT_R32G32B32_UINT:
		format = format::r32g32b32_uint;
		break;
	case VK_FORMAT_R32G32B32_SINT:
		format = format::r32g32b32_sint;
		break;
	case VK_FORMAT_R32G32B32_SFLOAT:
		format = format::r32g32b32_float;
		break;
	case VK_FORMAT_R32G32B32A32_UINT:
		format = format::r32g32b32a32_uint;
		break;
	case VK_FORMAT_R32G32B32A32_SINT:
		format = format::r32g32b32a32_sint;
		break;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		format = format::r32g32b32a32_float;
		break;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		format = format::r11g11b10_float;
		break;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		format = format::r9g9b9e5;
		break;
	case VK_FORMAT_D16_UNORM:
		format = format::d16_unorm;
		break;
	case VK_FORMAT_D32_SFLOAT:
		format = format::d32_float;
		break;
	case VK_FORMAT_S8_UINT:
		format = format::s8_uint;
		break;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		format = format::d24_unorm_s8_uint;
		break;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		format = format::d32_float_s8_uint;
		break;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		format = format::bc1_unorm;
		break;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		format = format::bc1_unorm_srgb;
		break;
	case VK_FORMAT_BC2_UNORM_BLOCK:
		format = format::bc2_unorm;
		break;
	case VK_FORMAT_BC2_SRGB_BLOCK:
		format = format::bc2_unorm_srgb;
		break;
	case VK_FORMAT_BC3_UNORM_BLOCK:
		format = format::bc3_unorm;
		break;
	case VK_FORMAT_BC3_SRGB_BLOCK:
		format = format::bc3_unorm_srgb;
		break;
	case VK_FORMAT_BC4_UNORM_BLOCK:
		format = format::bc4_unorm;
		break;
	case VK_FORMAT_BC4_SNORM_BLOCK:
		format = format::bc4_snorm;
		break;
	case VK_FORMAT_BC5_UNORM_BLOCK:
		format = format::bc5_unorm;
		break;
	case VK_FORMAT_BC5_SNORM_BLOCK:
		format = format::bc5_snorm;
		break;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		format = format::bc6h_ufloat;
		break;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		format = format::bc6h_sfloat;
		break;
	case VK_FORMAT_BC7_UNORM_BLOCK:
		format = format::bc7_unorm;
		break;
	case VK_FORMAT_BC7_SRGB_BLOCK:
		format = format::bc7_unorm_srgb;
		break;
	case VK_FORMAT_G8B8G8R8_422_UNORM:
		format = format::g8r8_g8b8_unorm;
		break;
	case VK_FORMAT_B8G8R8G8_422_UNORM:
		format = format::r8g8_b8g8_unorm;
		break;
	}
}

auto reshade::vulkan::convert_usage_to_access(resource_usage state) -> VkAccessFlags
{
	if (state == resource_usage::host)
		return VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

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
	if (state == resource_usage::host)
		return VK_PIPELINE_STAGE_HOST_BIT;

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
		create_info.extent = { desc.texture.width, 1u, 1u };
		create_info.arrayLayers = desc.texture.depth_or_layers;
		break;
	case resource_type::texture_2d:
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent = { desc.texture.width, desc.texture.height, 1u };
		create_info.arrayLayers = desc.texture.depth_or_layers;
		break;
	case resource_type::texture_3d:
		create_info.imageType = VK_IMAGE_TYPE_3D;
		create_info.extent = { desc.texture.width, desc.texture.height, desc.texture.depth_or_layers };
		create_info.arrayLayers = 1u;
		break;
	}

	convert_format_to_vk_format(desc.texture.format, create_info.format);
	create_info.mipLevels = desc.texture.levels;
	create_info.samples = static_cast<VkSampleCountFlagBits>(desc.texture.samples);
	convert_usage_to_image_usage_flags(desc.usage, create_info.usage);
}
void reshade::vulkan::convert_resource_desc(const resource_desc &desc, VkBufferCreateInfo &create_info)
{
	assert(desc.type == resource_type::buffer);
	create_info.size = desc.buffer.size;
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
		desc.texture.width = create_info.extent.width;
		assert(create_info.extent.height == 1 && create_info.extent.depth == 1);
		desc.texture.height = 1;
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_2D:
		desc.type = resource_type::texture_2d;
		desc.texture.width = create_info.extent.width;
		desc.texture.height = create_info.extent.height;
		assert(create_info.extent.depth == 1);
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_3D:
		desc.type = resource_type::texture_3d;
		desc.texture.width = create_info.extent.width;
		desc.texture.height = create_info.extent.height;
		assert(create_info.extent.depth <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.extent.depth);
		assert(create_info.arrayLayers == 1);
		break;
	}

	assert(create_info.mipLevels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(create_info.mipLevels);
	convert_vk_format_to_format(create_info.format, desc.texture.format);
	desc.texture.samples = static_cast<uint16_t>(create_info.samples);

	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	if (desc.type == resource_type::texture_2d && (
		create_info.usage & (desc.texture.samples > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
		desc.usage |= desc.texture.samples > 1 ? resource_usage::resolve_source : resource_usage::resolve_dest;

	return desc;
}
resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	resource_desc desc = {};
	desc.type = resource_type::buffer;
	desc.buffer.size = create_info.size;
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

	convert_format_to_vk_format(desc.format, create_info.format);
	create_info.subresourceRange.baseMipLevel = desc.texture.first_level;
	create_info.subresourceRange.levelCount = desc.texture.levels;
	create_info.subresourceRange.baseArrayLayer = desc.texture.first_layer;
	create_info.subresourceRange.layerCount = desc.texture.layers;
}
void reshade::vulkan::convert_resource_view_desc(const resource_view_desc &desc, VkBufferViewCreateInfo &create_info)
{
	assert(desc.type == resource_view_type::buffer);

	convert_format_to_vk_format(desc.format, create_info.format);
	create_info.offset = desc.buffer.offset;
	create_info.range = desc.buffer.size;
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

	convert_vk_format_to_format(create_info.format, desc.format);
	desc.texture.first_level = create_info.subresourceRange.baseMipLevel;
	desc.texture.levels = create_info.subresourceRange.levelCount;
	desc.texture.first_layer = create_info.subresourceRange.baseArrayLayer;
	desc.texture.layers = create_info.subresourceRange.layerCount;

	return desc;
}
resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkBufferViewCreateInfo &create_info)
{
	resource_view_desc desc = {};
	desc.type = resource_view_type::buffer;
	convert_vk_format_to_format(create_info.format, desc.format);
	desc.buffer.offset = create_info.offset;
	desc.buffer.size = create_info.sType;
	return desc;
}
