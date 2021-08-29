/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "vulkan_impl_device.hpp"
#include "vulkan_impl_type_convert.hpp"

auto reshade::vulkan::convert_format(api::format format) -> VkFormat
{
	switch (format)
	{
	default:
	case api::format::unknown:
		break;
	case api::format::r1_unorm:
		break; // Unsupported
	case api::format::r8_uint:
		return VK_FORMAT_R8_UINT;
	case api::format::r8_sint:
		return VK_FORMAT_R8_SINT;
	case api::format::r8_typeless:
	case api::format::r8_unorm:
	case api::format::a8_unorm: // { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R }
		return VK_FORMAT_R8_UNORM;
	case api::format::r8_snorm:
		return VK_FORMAT_R8_SNORM;
	case api::format::r8g8_uint:
		return VK_FORMAT_R8G8_UINT;
	case api::format::r8g8_sint:
		return VK_FORMAT_R8G8_SINT;
	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
		return VK_FORMAT_R8G8_UNORM;
	case api::format::r8g8_snorm:
		return VK_FORMAT_R8G8_SNORM;
	case api::format::r8g8b8a8_uint:
		return VK_FORMAT_R8G8B8A8_UINT;
	case api::format::r8g8b8a8_sint:
		return VK_FORMAT_R8G8B8A8_SINT;
	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
	case api::format::r8g8b8x8_typeless:
	case api::format::r8g8b8x8_unorm:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case api::format::r8g8b8a8_unorm_srgb:
	case api::format::r8g8b8x8_unorm_srgb:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case api::format::r8g8b8a8_snorm:
		return VK_FORMAT_R8G8B8A8_SNORM;
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
	case api::format::b8g8r8x8_typeless: // { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE }
	case api::format::b8g8r8x8_unorm:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case api::format::b8g8r8a8_unorm_srgb:
	case api::format::b8g8r8x8_unorm_srgb:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case api::format::r10g10b10a2_uint:
		return VK_FORMAT_A2B10G10R10_UINT_PACK32;
	case api::format::r10g10b10a2_typeless:
	case api::format::r10g10b10a2_unorm:
		return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
	case api::format::r10g10b10a2_xr_bias:
		break; // Unsupported
	case api::format::b10g10r10a2_uint:
		return VK_FORMAT_A2R10G10B10_UINT_PACK32;
	case api::format::b10g10r10a2_typeless:
	case api::format::b10g10r10a2_unorm:
		return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	case api::format::r16_sint:
		return VK_FORMAT_R16_SINT;
	case api::format::r16_uint:
		return VK_FORMAT_R16_UINT;
	case api::format::r16_typeless:
	case api::format::r16_float:
		return VK_FORMAT_R16_SFLOAT;
	case api::format::r16_unorm:
		return VK_FORMAT_R16_UNORM;
	case api::format::r16_snorm:
		return VK_FORMAT_R16_SNORM;
	case api::format::r16g16_uint:
		return VK_FORMAT_R16G16_UINT;
	case api::format::r16g16_sint:
		return VK_FORMAT_R16G16_SINT;
	case api::format::r16g16_typeless:
	case api::format::r16g16_float:
		return VK_FORMAT_R16G16_SFLOAT;
	case api::format::r16g16_unorm:
		return VK_FORMAT_R16G16_UNORM;
	case api::format::r16g16_snorm:
		return VK_FORMAT_R16G16_SNORM;
	case api::format::r16g16b16a16_uint:
		return VK_FORMAT_R16G16B16A16_UINT;
	case api::format::r16g16b16a16_sint:
		return VK_FORMAT_R16G16B16A16_SINT;
	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_float:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case api::format::r16g16b16a16_unorm:
		return VK_FORMAT_R16G16B16A16_UNORM;
	case api::format::r16g16b16a16_snorm:
		return VK_FORMAT_R16G16B16A16_SNORM;
	case api::format::r32_uint:
		return VK_FORMAT_R32_UINT;
	case api::format::r32_sint:
		return VK_FORMAT_R32_SINT;
	case api::format::r32_typeless:
	case api::format::r32_float:
		return VK_FORMAT_R32_SFLOAT;
	case api::format::r32g32_uint:
		return VK_FORMAT_R32G32_UINT;
	case api::format::r32g32_sint:
		return VK_FORMAT_R32G32_SINT;
	case api::format::r32g32_typeless:
	case api::format::r32g32_float:
		return VK_FORMAT_R32G32_SFLOAT;
	case api::format::r32g32b32_uint:
		return VK_FORMAT_R32G32B32_UINT;
	case api::format::r32g32b32_sint:
		return VK_FORMAT_R32G32B32_SINT;
	case api::format::r32g32b32_typeless:
	case api::format::r32g32b32_float:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case api::format::r32g32b32a32_uint:
		return VK_FORMAT_R32G32B32A32_UINT;
	case api::format::r32g32b32a32_sint:
		return VK_FORMAT_R32G32B32A32_SINT;
	case api::format::r32g32b32a32_typeless:
	case api::format::r32g32b32a32_float:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case api::format::r9g9b9e5:
		return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
	case api::format::r11g11b10_float:
		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case api::format::b5g6r5_unorm:
		return VK_FORMAT_R5G6B5_UNORM_PACK16;
	case api::format::b5g5r5a1_unorm:
	case api::format::b5g5r5x1_unorm:
		return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
#if 0
	case api::format::b4g4r4a4_unorm:
		return VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
#endif
	case api::format::s8_uint:
		return VK_FORMAT_S8_UINT;
	case api::format::d16_unorm:
		return VK_FORMAT_D16_UNORM;
	case api::format::d16_unorm_s8_uint:
		return VK_FORMAT_D16_UNORM_S8_UINT;
	case api::format::r24_g8_typeless:
	case api::format::r24_unorm_x8_uint:
	case api::format::x24_unorm_g8_uint:
	case api::format::d24_unorm_s8_uint:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case api::format::d32_float:
		return VK_FORMAT_D32_SFLOAT;
	case api::format::r32_g8_typeless:
	case api::format::r32_float_x8_uint:
	case api::format::x32_float_g8_uint:
	case api::format::d32_float_s8_uint:
		return VK_FORMAT_D32_SFLOAT_S8_UINT;
	case api::format::bc1_typeless:
	case api::format::bc1_unorm:
		return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
	case api::format::bc1_unorm_srgb:
		return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
	case api::format::bc2_typeless:
	case api::format::bc2_unorm:
		return VK_FORMAT_BC2_UNORM_BLOCK;
	case api::format::bc2_unorm_srgb:
		return VK_FORMAT_BC2_SRGB_BLOCK;
	case api::format::bc3_typeless:
	case api::format::bc3_unorm:
		return VK_FORMAT_BC3_UNORM_BLOCK;
	case api::format::bc3_unorm_srgb:
		return VK_FORMAT_BC3_SRGB_BLOCK;
	case api::format::bc4_typeless:
	case api::format::bc4_unorm:
		return VK_FORMAT_BC4_UNORM_BLOCK;
	case api::format::bc4_snorm:
		return VK_FORMAT_BC4_SNORM_BLOCK;
	case api::format::bc5_typeless:
	case api::format::bc5_unorm:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	case api::format::bc5_snorm:
		return VK_FORMAT_BC5_SNORM_BLOCK;
	case api::format::bc6h_typeless:
	case api::format::bc6h_ufloat:
		return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case api::format::bc6h_sfloat:
		return VK_FORMAT_BC6H_SFLOAT_BLOCK;
	case api::format::bc7_typeless:
	case api::format::bc7_unorm:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	case api::format::bc7_unorm_srgb:
		return VK_FORMAT_BC7_SRGB_BLOCK;
	case api::format::r8g8_b8g8_unorm:
		return VK_FORMAT_B8G8R8G8_422_UNORM;
	case api::format::g8r8_g8b8_unorm:
		return VK_FORMAT_G8B8G8R8_422_UNORM;
	}

	return VK_FORMAT_UNDEFINED;
}
auto reshade::vulkan::convert_format(VkFormat vk_format) -> api::format
{
	switch (vk_format)
	{
	default:
	case VK_FORMAT_UNDEFINED:
		return api::format::unknown;
#if 0
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:
		return api::format::b4g4r4a4_unorm;
#endif
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
		return api::format::b5g6r5_unorm;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		return api::format::b5g5r5a1_unorm;
	case VK_FORMAT_R8_UNORM:
		return api::format::r8_unorm;
	case VK_FORMAT_R8_SNORM:
		return api::format::r8_snorm;
	case VK_FORMAT_R8_UINT:
		return api::format::r8_uint;
	case VK_FORMAT_R8_SINT:
		return api::format::r8_sint;
	case VK_FORMAT_R8G8_UNORM:
		return api::format::r8g8_unorm;
	case VK_FORMAT_R8G8_SNORM:
		return api::format::r8g8_snorm;
	case VK_FORMAT_R8G8_UINT:
		return api::format::r8g8_uint;
	case VK_FORMAT_R8G8_SINT:
		return api::format::r8g8_sint;
	case VK_FORMAT_R8G8B8A8_UNORM:
		return api::format::r8g8b8a8_unorm;
	case VK_FORMAT_R8G8B8A8_SNORM:
		return api::format::r8g8b8a8_snorm;
	case VK_FORMAT_R8G8B8A8_UINT:
		return api::format::r8g8b8a8_uint;
	case VK_FORMAT_R8G8B8A8_SINT:
		return api::format::r8g8b8a8_sint;
	case VK_FORMAT_R8G8B8A8_SRGB:
		return api::format::r8g8b8a8_unorm_srgb;
	case VK_FORMAT_B8G8R8A8_UNORM:
		return api::format::b8g8r8a8_unorm;
	case VK_FORMAT_B8G8R8A8_SRGB:
		return api::format::b8g8r8a8_unorm_srgb;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		return api::format::r10g10b10a2_unorm;
	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		return api::format::r10g10b10a2_uint;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		return api::format::b10g10r10a2_unorm;
	case VK_FORMAT_A2R10G10B10_UINT_PACK32:
		return api::format::b10g10r10a2_uint;
	case VK_FORMAT_R16_UNORM:
		return api::format::r16_unorm;
	case VK_FORMAT_R16_SNORM:
		return api::format::r16_snorm;
	case VK_FORMAT_R16_UINT:
		return api::format::r16_uint;
	case VK_FORMAT_R16_SINT:
		return api::format::r16_sint;
	case VK_FORMAT_R16_SFLOAT:
		return api::format::r16_float;
	case VK_FORMAT_R16G16_UNORM:
		return api::format::r16g16_unorm;
	case VK_FORMAT_R16G16_SNORM:
		return api::format::r16g16_snorm;
	case VK_FORMAT_R16G16_UINT:
		return api::format::r16g16_uint;
	case VK_FORMAT_R16G16_SINT:
		return api::format::r16g16_sint;
	case VK_FORMAT_R16G16_SFLOAT:
		return api::format::r16g16_float;
	case VK_FORMAT_R16G16B16A16_UNORM:
		return api::format::r16g16b16a16_unorm;
	case VK_FORMAT_R16G16B16A16_SNORM:
		return api::format::r16g16b16a16_snorm;
	case VK_FORMAT_R16G16B16A16_UINT:
		return api::format::r16g16b16a16_uint;
	case VK_FORMAT_R16G16B16A16_SINT:
		return api::format::r16g16b16a16_sint;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
		return api::format::r16g16b16a16_float;
	case VK_FORMAT_R32_UINT:
		return api::format::r32_uint;
	case VK_FORMAT_R32_SINT:
		return api::format::r32_sint;
	case VK_FORMAT_R32_SFLOAT:
		return api::format::r32_float;
	case VK_FORMAT_R32G32_UINT:
		return api::format::r32g32_uint;
	case VK_FORMAT_R32G32_SINT:
		return api::format::r32g32_sint;
	case VK_FORMAT_R32G32_SFLOAT:
		return api::format::r32g32_float;
	case VK_FORMAT_R32G32B32_UINT:
		return api::format::r32g32b32_uint;
	case VK_FORMAT_R32G32B32_SINT:
		return api::format::r32g32b32_sint;
	case VK_FORMAT_R32G32B32_SFLOAT:
		return api::format::r32g32b32_float;
	case VK_FORMAT_R32G32B32A32_UINT:
		return api::format::r32g32b32a32_uint;
	case VK_FORMAT_R32G32B32A32_SINT:
		return api::format::r32g32b32a32_sint;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return api::format::r32g32b32a32_float;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		return api::format::r11g11b10_float;
	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		return api::format::r9g9b9e5;
	case VK_FORMAT_D16_UNORM:
		return api::format::d16_unorm;
	case VK_FORMAT_D32_SFLOAT:
		return api::format::d32_float;
	case VK_FORMAT_S8_UINT:
		return api::format::s8_uint;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return api::format::d24_unorm_s8_uint;
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return api::format::d32_float_s8_uint;
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		return api::format::bc1_unorm;
	case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
		return api::format::bc1_unorm_srgb;
	case VK_FORMAT_BC2_UNORM_BLOCK:
		return api::format::bc2_unorm;
	case VK_FORMAT_BC2_SRGB_BLOCK:
		return api::format::bc2_unorm_srgb;
	case VK_FORMAT_BC3_UNORM_BLOCK:
		return api::format::bc3_unorm;
	case VK_FORMAT_BC3_SRGB_BLOCK:
		return api::format::bc3_unorm_srgb;
	case VK_FORMAT_BC4_UNORM_BLOCK:
		return api::format::bc4_unorm;
	case VK_FORMAT_BC4_SNORM_BLOCK:
		return api::format::bc4_snorm;
	case VK_FORMAT_BC5_UNORM_BLOCK:
		return api::format::bc5_unorm;
	case VK_FORMAT_BC5_SNORM_BLOCK:
		return api::format::bc5_snorm;
	case VK_FORMAT_BC6H_UFLOAT_BLOCK:
		return api::format::bc6h_ufloat;
	case VK_FORMAT_BC6H_SFLOAT_BLOCK:
		return api::format::bc6h_sfloat;
	case VK_FORMAT_BC7_UNORM_BLOCK:
		return api::format::bc7_unorm;
	case VK_FORMAT_BC7_SRGB_BLOCK:
		return api::format::bc7_unorm_srgb;
	case VK_FORMAT_G8B8G8R8_422_UNORM:
		return api::format::g8r8_g8b8_unorm;
	case VK_FORMAT_B8G8R8G8_422_UNORM:
		return api::format::r8g8_b8g8_unorm;
	}
}

auto reshade::vulkan::convert_access_to_usage(VkAccessFlags flags) -> api::resource_usage
{
	api::resource_usage result = api::resource_usage::undefined;
	if ((flags & VK_ACCESS_SHADER_READ_BIT) != 0)
		result |= api::resource_usage::shader_resource;
	if ((flags & VK_ACCESS_SHADER_WRITE_BIT) != 0)
		result |= api::resource_usage::unordered_access;
	if ((flags & VK_ACCESS_TRANSFER_WRITE_BIT) != 0)
		result |= api::resource_usage::copy_dest;
	if ((flags & VK_ACCESS_TRANSFER_READ_BIT) != 0)
		result |= api::resource_usage::copy_source;
	if ((flags & VK_ACCESS_INDEX_READ_BIT) != 0)
		result |= api::resource_usage::index_buffer;
	if ((flags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) != 0)
		result |= api::resource_usage::vertex_buffer;
	if ((flags & VK_ACCESS_UNIFORM_READ_BIT) != 0)
		result |= api::resource_usage::constant_buffer;
	return result;
}
auto reshade::vulkan::convert_image_layout_to_usage(VkImageLayout layout) -> api::resource_usage
{
	switch (layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		return api::resource_usage::undefined;
	default:
	case VK_IMAGE_LAYOUT_GENERAL:
		return api::resource_usage::general;
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		return api::resource_usage::render_target;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
		return api::resource_usage::depth_stencil;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
	case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
		return api::resource_usage::depth_stencil_read;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		return api::resource_usage::shader_resource;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		return api::resource_usage::copy_source;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		return api::resource_usage::copy_dest;
	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		return api::resource_usage::cpu_access;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
	case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
		return api::resource_usage::present;
	}
}

auto reshade::vulkan::convert_usage_to_access(api::resource_usage state) -> VkAccessFlags
{
	if (state == api::resource_usage::present)
		return 0;
	if (state == api::resource_usage::cpu_access)
		return VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

	VkAccessFlags result = 0;
	if ((state & api::resource_usage::depth_stencil_read) != api::resource_usage::undefined)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if ((state & api::resource_usage::depth_stencil_write) != api::resource_usage::undefined)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if ((state & api::resource_usage::render_target) != api::resource_usage::undefined)
		result |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if ((state & api::resource_usage::shader_resource) != api::resource_usage::undefined)
		result |= VK_ACCESS_SHADER_READ_BIT;
	if ((state & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		result |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	if ((state & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != api::resource_usage::undefined)
		result |= VK_ACCESS_TRANSFER_WRITE_BIT;
	if ((state & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != api::resource_usage::undefined)
		result |= VK_ACCESS_TRANSFER_READ_BIT;
	if ((state & api::resource_usage::index_buffer) != api::resource_usage::undefined)
		result |= VK_ACCESS_INDEX_READ_BIT;
	if ((state & api::resource_usage::vertex_buffer) != api::resource_usage::undefined)
		result |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	if ((state & api::resource_usage::constant_buffer) != api::resource_usage::undefined)
		result |= VK_ACCESS_UNIFORM_READ_BIT;
	return result;
}
auto reshade::vulkan::convert_usage_to_image_layout(api::resource_usage state) -> VkImageLayout
{
	switch (state)
	{
	case api::resource_usage::undefined:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case api::resource_usage::depth_stencil:
	case api::resource_usage::depth_stencil_write:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case api::resource_usage::depth_stencil_read:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case api::resource_usage::render_target:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case api::resource_usage::shader_resource:
	case api::resource_usage::shader_resource_pixel:
	case api::resource_usage::shader_resource_non_pixel:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	default: // Default to general layout if multiple usage flags are specified
	case api::resource_usage::general:
	case api::resource_usage::unordered_access:
		return VK_IMAGE_LAYOUT_GENERAL;
	case api::resource_usage::copy_dest:
	case api::resource_usage::resolve_dest:
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	case api::resource_usage::copy_source:
	case api::resource_usage::resolve_source:
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case api::resource_usage::present:
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
}
auto reshade::vulkan::convert_usage_to_pipeline_stage(api::resource_usage state, bool src_stage, const VkPhysicalDeviceFeatures &enabled_features) -> VkPipelineStageFlags
{
	if (state == api::resource_usage::general)
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	if (state == api::resource_usage::present || state == api::resource_usage::undefined)
		// Do not introduce a execution dependency
		return src_stage ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	if (state == api::resource_usage::cpu_access)
		return VK_PIPELINE_STAGE_HOST_BIT;

	VkPipelineStageFlags result = 0;
	if ((state & api::resource_usage::depth_stencil_read) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	if ((state & api::resource_usage::depth_stencil_write) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if ((state & api::resource_usage::render_target) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if ((state & (api::resource_usage::shader_resource_pixel | api::resource_usage::constant_buffer)) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if ((state & (api::resource_usage::shader_resource_non_pixel | api::resource_usage::constant_buffer)) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | (enabled_features.tessellationShader ? VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT : 0) | (enabled_features.geometryShader ? VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT : 0) | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & (api::resource_usage::copy_dest | api::resource_usage::copy_source | api::resource_usage::resolve_dest | api::resource_usage::resolve_source)) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if ((state & (api::resource_usage::index_buffer | api::resource_usage::vertex_buffer)) != api::resource_usage::undefined)
		result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	return result;
}

void reshade::vulkan::convert_usage_to_image_usage_flags(api::resource_usage usage, VkImageUsageFlags &image_flags)
{
	if ((usage & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != api::resource_usage::undefined)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if ((usage & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != api::resource_usage::undefined)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined)
		// Add transfer destination usage as well to support clearing via 'vkCmdClearDepthStencilImage'
		image_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined)
		// Add transfer destination usage as well to support clearing via 'vkCmdClearColorImage'
		image_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined)
		image_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_SAMPLED_BIT;

	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		image_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_STORAGE_BIT;
}
void reshade::vulkan::convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, reshade::api::resource_usage &usage)
{
	using namespace reshade;

	if ((image_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		usage |= api::resource_usage::depth_stencil;
	if ((image_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
		usage |= api::resource_usage::render_target;
	if ((image_flags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
		usage |= api::resource_usage::shader_resource;
	if ((image_flags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
		usage |= api::resource_usage::unordered_access;
	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= api::resource_usage::copy_dest;
	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= api::resource_usage::copy_source;
}
void reshade::vulkan::convert_usage_to_buffer_usage_flags(api::resource_usage usage, VkBufferUsageFlags &buffer_flags)
{
	if ((usage & api::resource_usage::index_buffer) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if ((usage & api::resource_usage::vertex_buffer) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if ((usage & api::resource_usage::constant_buffer) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if ((usage & api::resource_usage::copy_dest) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if ((usage & api::resource_usage::copy_source) != api::resource_usage::undefined)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
}
void reshade::vulkan::convert_buffer_usage_flags_to_usage(const VkBufferUsageFlags buffer_flags, reshade::api::resource_usage &usage)
{
	using namespace reshade;

	if ((buffer_flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
		usage |= api::resource_usage::index_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
		usage |= api::resource_usage::vertex_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
		usage |= api::resource_usage::constant_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
		usage |= api::resource_usage::unordered_access;
	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= api::resource_usage::copy_dest;
	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= api::resource_usage::copy_source;
}

void reshade::vulkan::convert_sampler_desc(const api::sampler_desc &desc, VkSamplerCreateInfo &create_info)
{
	create_info.compareEnable = VK_FALSE;

	switch (desc.filter)
	{
	case api::filter_type::compare_min_mag_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_mag_mip_point:
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_mag_point_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_point_mag_linear_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_point_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_point_mag_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_point_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_linear_mag_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_linear_mag_mip_point:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_linear_mag_point_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_linear_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_mag_linear_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_min_mag_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::min_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_type::compare_anisotropic:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_type::anisotropic:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_TRUE;
		break;
	}

	const auto convert_address_mode = [](api::texture_address_mode mode) {
		switch (mode)
		{
		default:
			assert(false);
			[[fallthrough]];
		case api::texture_address_mode::wrap:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case api::texture_address_mode::mirror:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case api::texture_address_mode::clamp:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case api::texture_address_mode::border:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case api::texture_address_mode::mirror_once:
			return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
		}
	};

	create_info.addressModeU = convert_address_mode(desc.address_u);
	create_info.addressModeV = convert_address_mode(desc.address_v);
	create_info.addressModeW = convert_address_mode(desc.address_w);
	create_info.mipLodBias = desc.mip_lod_bias;
	create_info.maxAnisotropy = desc.max_anisotropy;
	create_info.compareOp = convert_compare_op(desc.compare_op);
	create_info.minLod = desc.min_lod;
	create_info.maxLod = desc.max_lod;
}
reshade::api::sampler_desc reshade::vulkan::convert_sampler_desc(const VkSamplerCreateInfo &create_info)
{
	api::sampler_desc desc = {};
	if (create_info.anisotropyEnable)
	{
		desc.filter = api::filter_type::anisotropic;
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
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_mag_mip_point : api::filter_type::min_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_mag_point_mip_linear : api::filter_type::min_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_point_mag_linear_mip_point : api::filter_type::min_point_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_point_mag_mip_linear : api::filter_type::min_point_mag_mip_linear;
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
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_linear_mag_mip_point : api::filter_type::min_linear_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_linear_mag_point_mip_linear : api::filter_type::min_linear_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_mag_linear_mip_point : api::filter_type::min_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_type::compare_min_mag_mip_linear : api::filter_type::min_mag_mip_linear;
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
			assert(false);
			[[fallthrough]];
		case VK_SAMPLER_ADDRESS_MODE_REPEAT:
			return api::texture_address_mode::wrap;
		case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return api::texture_address_mode::mirror;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return api::texture_address_mode::clamp;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			return api::texture_address_mode::border;
		case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
			return api::texture_address_mode::mirror_once;
		}
	};

	desc.address_u = convert_address_mode(create_info.addressModeU);
	desc.address_v = convert_address_mode(create_info.addressModeV);
	desc.address_w = convert_address_mode(create_info.addressModeW);
	desc.mip_lod_bias = create_info.mipLodBias;
	desc.max_anisotropy = create_info.maxAnisotropy;
	desc.compare_op = convert_compare_op(create_info.compareOp);
	desc.min_lod = create_info.minLod;
	desc.max_lod = create_info.maxLod;

	return desc;
}

void reshade::vulkan::convert_resource_desc(const api::resource_desc &desc, VkImageCreateInfo &create_info)
{
	switch (desc.type)
	{
	default:
		assert(false);
		break;
	case api::resource_type::texture_1d:
		create_info.imageType = VK_IMAGE_TYPE_1D;
		create_info.extent = { desc.texture.width, 1u, 1u };
		create_info.arrayLayers = desc.texture.depth_or_layers;
		break;
	case api::resource_type::texture_2d:
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent = { desc.texture.width, desc.texture.height, 1u };
		create_info.arrayLayers = desc.texture.depth_or_layers;
		break;
	case api::resource_type::texture_3d:
		create_info.imageType = VK_IMAGE_TYPE_3D;
		create_info.extent = { desc.texture.width, desc.texture.height, desc.texture.depth_or_layers };
		create_info.arrayLayers = 1u;
		break;
	}

	if (const VkFormat format = convert_format(desc.texture.format);
		format != VK_FORMAT_UNDEFINED)
		create_info.format = format;

	create_info.mipLevels = desc.texture.levels;
	create_info.samples = static_cast<VkSampleCountFlagBits>(desc.texture.samples);
	convert_usage_to_image_usage_flags(desc.usage, create_info.usage);

	// A typeless format indicates that views with different typed formats can be created, so set mutable flag
	if (desc.texture.format == api::format_to_typeless(desc.texture.format) &&
		desc.texture.format != api::format_to_default_typed(desc.texture.format))
		create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

	if ((desc.flags & api::resource_flags::sparse_binding) == api::resource_flags::sparse_binding)
		create_info.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	else
		create_info.flags &= ~(VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

	if ((desc.flags & api::resource_flags::cube_compatible) == api::resource_flags::cube_compatible)
		create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	else
		create_info.flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	// Mipmap generation is using 'vkCmdBlitImage' and therefore needs transfer usage flags (see 'command_list_impl::generate_mipmaps')
	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
		create_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// Dynamic resources do not exist in Vulkan
	assert((desc.flags & api::resource_flags::dynamic) != api::resource_flags::dynamic);
}
void reshade::vulkan::convert_resource_desc(const api::resource_desc &desc, VkBufferCreateInfo &create_info)
{
	assert(desc.type == api::resource_type::buffer);

	create_info.size = desc.buffer.size;
	convert_usage_to_buffer_usage_flags(desc.usage, create_info.usage);

	// Dynamic resources do not exist in Vulkan
	assert((desc.flags & api::resource_flags::dynamic) != api::resource_flags::dynamic);
}
reshade::api::resource_desc reshade::vulkan::convert_resource_desc(const VkImageCreateInfo &create_info)
{
	api::resource_desc desc = {};
	switch (create_info.imageType)
	{
	default:
		assert(false);
		desc.type = api::resource_type::unknown;
		break;
	case VK_IMAGE_TYPE_1D:
		desc.type = api::resource_type::texture_1d;
		desc.texture.width = create_info.extent.width;
		assert(create_info.extent.height == 1 && create_info.extent.depth == 1);
		desc.texture.height = 1;
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_2D:
		desc.type = api::resource_type::texture_2d;
		desc.texture.width = create_info.extent.width;
		desc.texture.height = create_info.extent.height;
		assert(create_info.extent.depth == 1);
		assert(create_info.arrayLayers <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.arrayLayers);
		break;
	case VK_IMAGE_TYPE_3D:
		desc.type = api::resource_type::texture_3d;
		desc.texture.width = create_info.extent.width;
		desc.texture.height = create_info.extent.height;
		assert(create_info.extent.depth <= std::numeric_limits<uint16_t>::max());
		desc.texture.depth_or_layers = static_cast<uint16_t>(create_info.extent.depth);
		assert(create_info.arrayLayers == 1);
		break;
	}

	assert(create_info.mipLevels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(create_info.mipLevels);
	desc.texture.format = convert_format(create_info.format);
	desc.texture.samples = static_cast<uint16_t>(create_info.samples);

	convert_image_usage_flags_to_usage(create_info.usage, desc.usage);
	if (desc.type == api::resource_type::texture_2d && (
		create_info.usage & (desc.texture.samples > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0)
		desc.usage |= desc.texture.samples > 1 ? api::resource_usage::resolve_source : api::resource_usage::resolve_dest;

	if ((create_info.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0)
		desc.flags |= api::resource_flags::sparse_binding;

	if ((create_info.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0)
		desc.texture.format = api::format_to_typeless(desc.texture.format);

	if ((create_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)
		desc.flags |= api::resource_flags::cube_compatible;

	// Images that have both transfer usage flags are usable with the 'generate_mipmaps' function
	if (create_info.mipLevels > 1 && (create_info.usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) == (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		desc.flags |= api::resource_flags::generate_mipmaps;

	return desc;
}
reshade::api::resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = create_info.size;
	convert_buffer_usage_flags_to_usage(create_info.usage, desc.usage);
	return desc;
}

void reshade::vulkan::convert_resource_view_desc(const api::resource_view_desc &desc, VkImageViewCreateInfo &create_info)
{
	switch (desc.type)
	{
	default:
		assert(false);
		break;
	case api::resource_view_type::texture_1d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case api::resource_view_type::texture_1d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case api::resource_view_type::texture_2d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case api::resource_view_type::texture_2d_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	case api::resource_view_type::texture_3d:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case api::resource_view_type::texture_cube:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		break;
	case api::resource_view_type::texture_cube_array:
		create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		break;
	}

	if (const VkFormat format = convert_format(desc.format);
		format != VK_FORMAT_UNDEFINED)
		create_info.format = format;

	create_info.subresourceRange.baseMipLevel = desc.texture.first_level;
	create_info.subresourceRange.levelCount = desc.texture.levels;
	create_info.subresourceRange.baseArrayLayer = desc.texture.first_layer;
	create_info.subresourceRange.layerCount = desc.texture.layers;
}
void reshade::vulkan::convert_resource_view_desc(const api::resource_view_desc &desc, VkBufferViewCreateInfo &create_info)
{
	assert(desc.type == api::resource_view_type::buffer);

	if (const VkFormat format = convert_format(desc.format);
		format != VK_FORMAT_UNDEFINED)
		create_info.format = format;

	create_info.offset = desc.buffer.offset;
	create_info.range = desc.buffer.size;
}
reshade::api::resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkImageViewCreateInfo &create_info)
{
	api::resource_view_desc desc = {};
	switch (create_info.viewType)
	{
	default:
		assert(false);
		desc.type = api::resource_view_type::unknown;
		break;
	case VK_IMAGE_VIEW_TYPE_1D:
		desc.type = api::resource_view_type::texture_1d;
		break;
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		desc.type = api::resource_view_type::texture_1d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_2D:
		desc.type = api::resource_view_type::texture_2d;
		break;
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		desc.type = api::resource_view_type::texture_2d_array;
		break;
	case VK_IMAGE_VIEW_TYPE_3D:
		desc.type = api::resource_view_type::texture_3d;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE:
		desc.type = api::resource_view_type::texture_cube;
		break;
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		desc.type = api::resource_view_type::texture_cube_array;
		break;
	}

	desc.format = convert_format(create_info.format);
	desc.texture.first_level = create_info.subresourceRange.baseMipLevel;
	desc.texture.levels = create_info.subresourceRange.levelCount;
	desc.texture.first_layer = create_info.subresourceRange.baseArrayLayer;
	desc.texture.layers = create_info.subresourceRange.layerCount;

	return desc;
}
reshade::api::resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkBufferViewCreateInfo &create_info)
{
	api::resource_view_desc desc = {};
	desc.type = api::resource_view_type::buffer;
	desc.format = convert_format(create_info.format);
	desc.buffer.offset = create_info.offset;
	desc.buffer.size = create_info.sType;
	return desc;
}

reshade::api::pipeline_desc reshade::vulkan::device_impl::convert_pipeline_desc(const VkComputePipelineCreateInfo &create_info) const
{
	api::pipeline_desc desc = { api::pipeline_stage::all_compute };
	desc.layout = { (uint64_t)create_info.layout };

	assert(create_info.stage.stage == VK_SHADER_STAGE_COMPUTE_BIT);
	desc.compute.shader.format = api::shader_format::spirv;
	desc.compute.shader.entry_point = create_info.stage.pName;

	shader_module_data module_data = get_native_object_data<shader_module_data>((uint64_t)create_info.stage.module);

	desc.compute.shader.code = module_data.spirv;
	desc.compute.shader.code_size = module_data.spirv_size;

	return desc;
}
reshade::api::pipeline_desc reshade::vulkan::device_impl::convert_pipeline_desc(const VkGraphicsPipelineCreateInfo &create_info) const
{
	bool has_tessellation_shader_stage = false;

	api::pipeline_desc desc = { api::pipeline_stage::all_graphics };
	desc.layout = { (uint64_t)create_info.layout };

	for (uint32_t i = 0; i < create_info.stageCount; ++i)
	{
		const VkPipelineShaderStageCreateInfo &stage = create_info.pStages[i];

		shader_module_data module_data = get_native_object_data<shader_module_data>((uint64_t)stage.module);

		switch (stage.stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			desc.graphics.vertex_shader.code = module_data.spirv;
			desc.graphics.vertex_shader.code_size = module_data.spirv_size;
			desc.graphics.vertex_shader.format = api::shader_format::spirv;
			desc.graphics.vertex_shader.entry_point = stage.pName;
			break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			has_tessellation_shader_stage = true;
			desc.graphics.hull_shader.code = module_data.spirv;
			desc.graphics.hull_shader.code_size = module_data.spirv_size;
			desc.graphics.hull_shader.format = api::shader_format::spirv;
			desc.graphics.hull_shader.entry_point = stage.pName;
			break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			has_tessellation_shader_stage = true;
			desc.graphics.domain_shader.code = module_data.spirv;
			desc.graphics.domain_shader.code_size = module_data.spirv_size;
			desc.graphics.domain_shader.format = api::shader_format::spirv;
			desc.graphics.domain_shader.entry_point = stage.pName;
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			desc.graphics.geometry_shader.code = module_data.spirv;
			desc.graphics.geometry_shader.code_size = module_data.spirv_size;
			desc.graphics.geometry_shader.format = api::shader_format::spirv;
			desc.graphics.geometry_shader.entry_point = stage.pName;
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			desc.graphics.pixel_shader.code = module_data.spirv;
			desc.graphics.pixel_shader.code_size = module_data.spirv_size;
			desc.graphics.pixel_shader.format = api::shader_format::spirv;
			desc.graphics.pixel_shader.entry_point = stage.pName;
			break;
		}
	}

	if (create_info.pVertexInputState != nullptr)
	{
		const VkPipelineVertexInputStateCreateInfo &vertex_input_state_info = *create_info.pVertexInputState;

		for (uint32_t a = 0; a < vertex_input_state_info.vertexAttributeDescriptionCount; ++a)
		{
			const VkVertexInputAttributeDescription &attribute = vertex_input_state_info.pVertexAttributeDescriptions[a];

			desc.graphics.input_layout[a].location = attribute.location;
			desc.graphics.input_layout[a].format = convert_format(attribute.format);
			desc.graphics.input_layout[a].buffer_binding = attribute.binding;
			desc.graphics.input_layout[a].offset = attribute.offset;

			for (uint32_t b = 0; b < vertex_input_state_info.vertexBindingDescriptionCount; ++b)
			{
				const VkVertexInputBindingDescription &binding = vertex_input_state_info.pVertexBindingDescriptions[b];

				if (binding.binding == attribute.binding)
				{
					desc.graphics.input_layout[a].stride = binding.stride;
					desc.graphics.input_layout[a].instance_step_rate = binding.inputRate != VK_VERTEX_INPUT_RATE_VERTEX ? 1 : 0;
					break;
				}
			}
		}
	}

	if (create_info.pInputAssemblyState != nullptr)
	{
		const VkPipelineInputAssemblyStateCreateInfo &input_assembly_state_info = *create_info.pInputAssemblyState;

		desc.graphics.topology = convert_primitive_topology(input_assembly_state_info.topology);
	}

	if (has_tessellation_shader_stage && create_info.pTessellationState != nullptr)
	{
		const VkPipelineTessellationStateCreateInfo &tessellation_state_info = *create_info.pTessellationState;

		assert(desc.graphics.topology == api::primitive_topology::patch_list_01_cp);
		desc.graphics.topology = static_cast<api::primitive_topology>(static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) + tessellation_state_info.patchControlPoints - 1);
	}

	if (create_info.pViewportState != nullptr)
	{
		const VkPipelineViewportStateCreateInfo &viewport_state_info = *create_info.pViewportState;

		desc.graphics.viewport_count = viewport_state_info.viewportCount;
	}

	if (create_info.pRasterizationState != nullptr)
	{
		const VkPipelineRasterizationStateCreateInfo &rasterization_state_info = *create_info.pRasterizationState;

		desc.graphics.rasterizer_state.fill_mode = convert_fill_mode(rasterization_state_info.polygonMode);
		desc.graphics.rasterizer_state.cull_mode = convert_cull_mode(rasterization_state_info.cullMode);
		desc.graphics.rasterizer_state.front_counter_clockwise = rasterization_state_info.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;
		desc.graphics.rasterizer_state.depth_bias = rasterization_state_info.depthBiasConstantFactor;
		desc.graphics.rasterizer_state.depth_bias_clamp = rasterization_state_info.depthBiasClamp;
		desc.graphics.rasterizer_state.slope_scaled_depth_bias = rasterization_state_info.depthBiasSlopeFactor;
		desc.graphics.rasterizer_state.depth_clip_enable = !rasterization_state_info.depthClampEnable;
		desc.graphics.rasterizer_state.scissor_enable = true;
	}

	if (create_info.pMultisampleState != nullptr)
	{
		const VkPipelineMultisampleStateCreateInfo &multisample_state_info = *create_info.pMultisampleState;

		desc.graphics.sample_count = static_cast<uint32_t>(multisample_state_info.rasterizationSamples);
		desc.graphics.blend_state.alpha_to_coverage_enable = multisample_state_info.alphaToCoverageEnable;
		desc.graphics.rasterizer_state.multisample_enable = multisample_state_info.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT;

		if (multisample_state_info.pSampleMask != nullptr)
			desc.graphics.sample_mask = *multisample_state_info.pSampleMask;
		else
			desc.graphics.sample_mask = std::numeric_limits<uint32_t>::max();
	}

	if (create_info.pDepthStencilState != nullptr)
	{
		const VkPipelineDepthStencilStateCreateInfo &depth_stencil_state_info = *create_info.pDepthStencilState;

		desc.graphics.depth_stencil_state.depth_enable = depth_stencil_state_info.depthTestEnable;
		desc.graphics.depth_stencil_state.depth_write_mask = depth_stencil_state_info.depthWriteEnable;
		desc.graphics.depth_stencil_state.depth_func = convert_compare_op(depth_stencil_state_info.depthCompareOp);
		desc.graphics.depth_stencil_state.stencil_enable = depth_stencil_state_info.stencilTestEnable;
		desc.graphics.depth_stencil_state.stencil_read_mask = depth_stencil_state_info.back.compareMask & 0xFF;
		desc.graphics.depth_stencil_state.stencil_write_mask = depth_stencil_state_info.back.writeMask & 0xFF;
		desc.graphics.depth_stencil_state.stencil_reference_value = depth_stencil_state_info.back.reference & 0xFF;
		desc.graphics.depth_stencil_state.back_stencil_fail_op = convert_stencil_op(depth_stencil_state_info.back.failOp);
		desc.graphics.depth_stencil_state.back_stencil_pass_op = convert_stencil_op(depth_stencil_state_info.back.passOp);
		desc.graphics.depth_stencil_state.back_stencil_depth_fail_op = convert_stencil_op(depth_stencil_state_info.back.depthFailOp);
		desc.graphics.depth_stencil_state.back_stencil_func = convert_compare_op(depth_stencil_state_info.back.compareOp);
		desc.graphics.depth_stencil_state.front_stencil_fail_op = convert_stencil_op(depth_stencil_state_info.front.failOp);
		desc.graphics.depth_stencil_state.front_stencil_pass_op = convert_stencil_op(depth_stencil_state_info.front.passOp);
		desc.graphics.depth_stencil_state.front_stencil_depth_fail_op = convert_stencil_op(depth_stencil_state_info.front.depthFailOp);
		desc.graphics.depth_stencil_state.front_stencil_func = convert_compare_op(depth_stencil_state_info.front.compareOp);
	}

	if (create_info.pColorBlendState != nullptr)
	{
		const VkPipelineColorBlendStateCreateInfo &color_blend_state_info = *create_info.pColorBlendState;

		desc.graphics.blend_state.blend_constant =
			(static_cast<uint32_t>(color_blend_state_info.blendConstants[0] * 255)) |
			(static_cast<uint32_t>(color_blend_state_info.blendConstants[1] * 255) << 4) |
			(static_cast<uint32_t>(color_blend_state_info.blendConstants[2] * 255) << 8) |
			(static_cast<uint32_t>(color_blend_state_info.blendConstants[3] * 255) << 12);

		for (uint32_t a = 0; a < color_blend_state_info.attachmentCount; ++a)
		{
			const VkPipelineColorBlendAttachmentState &attachment = color_blend_state_info.pAttachments[a];

			desc.graphics.blend_state.blend_enable[a] = attachment.blendEnable;
			desc.graphics.blend_state.logic_op_enable[a] = color_blend_state_info.logicOpEnable;
			desc.graphics.blend_state.color_blend_op[a] = convert_blend_op(attachment.colorBlendOp);
			desc.graphics.blend_state.src_color_blend_factor[a] = convert_blend_factor(attachment.srcColorBlendFactor);
			desc.graphics.blend_state.dst_color_blend_factor[a] = convert_blend_factor(attachment.dstColorBlendFactor);
			desc.graphics.blend_state.alpha_blend_op[a] = convert_blend_op(attachment.alphaBlendOp);
			desc.graphics.blend_state.src_alpha_blend_factor[a] = convert_blend_factor(attachment.srcAlphaBlendFactor);
			desc.graphics.blend_state.dst_alpha_blend_factor[a] = convert_blend_factor(attachment.dstAlphaBlendFactor);
			desc.graphics.blend_state.logic_op[a] = convert_logic_op(color_blend_state_info.logicOp);
			desc.graphics.blend_state.render_target_write_mask[a] = static_cast<uint8_t>(attachment.colorWriteMask);
		}
	}

	if (create_info.pDynamicState != nullptr)
	{
		const VkPipelineDynamicStateCreateInfo &dynamic_state_info = *create_info.pDynamicState;

		for (uint32_t i = 0, k = 0; i < dynamic_state_info.dynamicStateCount && k < 32; ++i)
		{
			switch (dynamic_state_info.pDynamicStates[i])
			{
			case VK_DYNAMIC_STATE_DEPTH_BIAS:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_bias;
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_bias_clamp;
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_bias_slope_scaled;
				break;
			case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::blend_constant;
				break;
			case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::stencil_read_mask;
				break;
			case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::stencil_write_mask;
				break;
			case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::stencil_reference_value;
				break;
			case VK_DYNAMIC_STATE_CULL_MODE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::cull_mode;
				break;
			case VK_DYNAMIC_STATE_FRONT_FACE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::front_counter_clockwise;
				break;
			case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::primitive_topology;
				break;
			case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_enable;
				break;
			case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_write_mask;
				break;
			case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_func;
				break;
			case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::depth_clip_enable;
				break;
			case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::stencil_enable;
				break;
			case VK_DYNAMIC_STATE_STENCIL_OP_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::back_stencil_func;
				desc.graphics.dynamic_states[k++] = api::dynamic_state::front_stencil_func;
				break;
			case VK_DYNAMIC_STATE_LOGIC_OP_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::logic_op;
				break;
			case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
				desc.graphics.dynamic_states[k++] = api::dynamic_state::render_target_write_mask;
				break;
			}
		}
	}

	return desc;
}

auto reshade::vulkan::convert_logic_op(api::logic_op value) -> VkLogicOp
{
	return static_cast<VkLogicOp>(value);
}
auto reshade::vulkan::convert_logic_op(VkLogicOp value) -> api::logic_op
{
	return static_cast<api::logic_op>(value);
}
auto reshade::vulkan::convert_blend_op(api::blend_op value) -> VkBlendOp
{
	return static_cast<VkBlendOp>(value);
}
auto reshade::vulkan::convert_blend_op(VkBlendOp value) -> api::blend_op
{
	return static_cast<api::blend_op>(value);
}
auto reshade::vulkan::convert_blend_factor(api::blend_factor value) -> VkBlendFactor
{
	return static_cast<VkBlendFactor>(value);
}
auto reshade::vulkan::convert_blend_factor(VkBlendFactor value) -> api::blend_factor
{
	return static_cast<api::blend_factor>(value);
}
auto reshade::vulkan::convert_fill_mode(api::fill_mode value) -> VkPolygonMode
{
	switch (value)
	{
	case api::fill_mode::point:
		return VK_POLYGON_MODE_POINT;
	case api::fill_mode::wireframe:
		return VK_POLYGON_MODE_LINE;
	default:
		assert(false);
		[[fallthrough]];
	case api::fill_mode::solid:
		return VK_POLYGON_MODE_FILL;
	}
}
auto reshade::vulkan::convert_fill_mode(VkPolygonMode value) -> api::fill_mode
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case VK_POLYGON_MODE_FILL:
		return api::fill_mode::solid;
	case VK_POLYGON_MODE_LINE:
		return api::fill_mode::wireframe;
	case VK_POLYGON_MODE_POINT:
		return api::fill_mode::point;
	}
}
auto reshade::vulkan::convert_cull_mode(api::cull_mode value) -> VkCullModeFlags
{
	return static_cast<VkCullModeFlags>(value);
}
auto reshade::vulkan::convert_cull_mode(VkCullModeFlags value) -> api::cull_mode
{
	return static_cast<api::cull_mode>(value);
}
auto reshade::vulkan::convert_compare_op(api::compare_op value) -> VkCompareOp
{
	return static_cast<VkCompareOp>(value);
}
auto reshade::vulkan::convert_compare_op(VkCompareOp value) -> api::compare_op
{
	return static_cast<api::compare_op>(value);
}
auto reshade::vulkan::convert_stencil_op(api::stencil_op value) -> VkStencilOp
{
	return static_cast<VkStencilOp>(value);
}
auto reshade::vulkan::convert_stencil_op(VkStencilOp value) -> api::stencil_op
{
	return static_cast<api::stencil_op>(value);
}
auto reshade::vulkan::convert_primitive_topology(api::primitive_topology value) -> VkPrimitiveTopology
{
	switch (value)
	{
	default:
	case api::primitive_topology::undefined:
		assert(false);
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	case api::primitive_topology::point_list:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case api::primitive_topology::line_list:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case api::primitive_topology::line_strip:
		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case api::primitive_topology::triangle_list:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case api::primitive_topology::triangle_strip:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case api::primitive_topology::triangle_fan:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	case api::primitive_topology::line_list_adj:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
	case api::primitive_topology::line_strip_adj:
		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
	case api::primitive_topology::triangle_list_adj:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
	case api::primitive_topology::triangle_strip_adj:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
	case api::primitive_topology::patch_list_01_cp:
	case api::primitive_topology::patch_list_02_cp:
	case api::primitive_topology::patch_list_03_cp:
	case api::primitive_topology::patch_list_04_cp:
	case api::primitive_topology::patch_list_05_cp:
	case api::primitive_topology::patch_list_06_cp:
	case api::primitive_topology::patch_list_07_cp:
	case api::primitive_topology::patch_list_08_cp:
	case api::primitive_topology::patch_list_09_cp:
	case api::primitive_topology::patch_list_10_cp:
	case api::primitive_topology::patch_list_11_cp:
	case api::primitive_topology::patch_list_12_cp:
	case api::primitive_topology::patch_list_13_cp:
	case api::primitive_topology::patch_list_14_cp:
	case api::primitive_topology::patch_list_15_cp:
	case api::primitive_topology::patch_list_16_cp:
	case api::primitive_topology::patch_list_17_cp:
	case api::primitive_topology::patch_list_18_cp:
	case api::primitive_topology::patch_list_19_cp:
	case api::primitive_topology::patch_list_20_cp:
	case api::primitive_topology::patch_list_21_cp:
	case api::primitive_topology::patch_list_22_cp:
	case api::primitive_topology::patch_list_23_cp:
	case api::primitive_topology::patch_list_24_cp:
	case api::primitive_topology::patch_list_25_cp:
	case api::primitive_topology::patch_list_26_cp:
	case api::primitive_topology::patch_list_27_cp:
	case api::primitive_topology::patch_list_28_cp:
	case api::primitive_topology::patch_list_29_cp:
	case api::primitive_topology::patch_list_30_cp:
	case api::primitive_topology::patch_list_31_cp:
	case api::primitive_topology::patch_list_32_cp:
		return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	}
}
auto reshade::vulkan::convert_primitive_topology(VkPrimitiveTopology value) -> api::primitive_topology
{
	switch (value)
	{
	default:
	case VK_PRIMITIVE_TOPOLOGY_MAX_ENUM:
		assert(false);
		return api::primitive_topology::undefined;
	case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		return api::primitive_topology::point_list;
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		return api::primitive_topology::line_list;
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		return api::primitive_topology::line_strip;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		return api::primitive_topology::triangle_list;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		return api::primitive_topology::triangle_strip;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		return api::primitive_topology::triangle_fan;
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		return api::primitive_topology::line_list_adj;
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		return api::primitive_topology::line_strip_adj;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		return api::primitive_topology::triangle_list_adj;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		return api::primitive_topology::triangle_strip_adj;
	case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		return api::primitive_topology::patch_list_01_cp;
	}
}
auto reshade::vulkan::convert_query_type(api::query_type type) -> VkQueryType
{
	switch (type)
	{
	case reshade::api::query_type::occlusion:
	case reshade::api::query_type::binary_occlusion:
		return VK_QUERY_TYPE_OCCLUSION;
	case reshade::api::query_type::timestamp:
		return VK_QUERY_TYPE_TIMESTAMP;
	case reshade::api::query_type::pipeline_statistics:
		return VK_QUERY_TYPE_PIPELINE_STATISTICS;
	default:
		assert(false);
		return VK_QUERY_TYPE_MAX_ENUM;
	}
}
