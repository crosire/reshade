/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vulkan_hooks.hpp"
#include "vulkan_impl_type_convert.hpp"
#include <algorithm> // std::copy_n, std::fill_n, std::find_if

auto reshade::vulkan::convert_format(api::format format, VkComponentMapping *components) -> VkFormat
{
	switch (format)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::format::unknown:
		break;

	case api::format::r1_unorm:
		break; // Unsupported

	case api::format::l8_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE };
		return VK_FORMAT_R8_UNORM;
	case api::format::a8_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R };
		return VK_FORMAT_R8_UNORM;
	case api::format::r8_uint:
		return VK_FORMAT_R8_UINT;
	case api::format::r8_sint:
		return VK_FORMAT_R8_SINT;
	case api::format::r8_typeless:
	case api::format::r8_unorm:
		return VK_FORMAT_R8_UNORM;
	case api::format::r8_snorm:
		return VK_FORMAT_R8_SNORM;

	case api::format::l8a8_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G };
		return VK_FORMAT_R8G8_UNORM;
	case api::format::r8g8_uint:
		return VK_FORMAT_R8G8_UINT;
	case api::format::r8g8_sint:
		return VK_FORMAT_R8G8_SINT;
	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
		return VK_FORMAT_R8G8_UNORM;
	case api::format::r8g8_snorm:
		return VK_FORMAT_R8G8_SNORM;

	case api::format::r8g8b8_uint:
		return VK_FORMAT_R8G8B8_UINT;
	case api::format::r8g8b8_sint:
		return VK_FORMAT_R8G8B8_SINT;
	case api::format::r8g8b8_typeless:
	case api::format::r8g8b8_unorm:
		return VK_FORMAT_R8G8B8_UNORM;
	case api::format::r8g8b8_unorm_srgb:
		return VK_FORMAT_R8G8B8_SRGB;
	case api::format::r8g8b8_snorm:
		return VK_FORMAT_R8G8B8_SNORM;

	case api::format::b8g8r8_typeless:
	case api::format::b8g8r8_unorm:
		return VK_FORMAT_B8G8R8_UNORM;
	case api::format::b8g8r8_unorm_srgb:
		return VK_FORMAT_B8G8R8_SRGB;

	case api::format::r8g8b8a8_uint:
		return VK_FORMAT_R8G8B8A8_UINT;
	case api::format::r8g8b8a8_sint:
		return VK_FORMAT_R8G8B8A8_SINT;
	case api::format::r8g8b8x8_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		[[fallthrough]];
	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case api::format::r8g8b8x8_unorm_srgb:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		[[fallthrough]];
	case api::format::r8g8b8a8_unorm_srgb:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case api::format::r8g8b8a8_snorm:
		return VK_FORMAT_R8G8B8A8_SNORM;

	case api::format::b8g8r8x8_typeless:
	case api::format::b8g8r8x8_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		[[fallthrough]];
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case api::format::b8g8r8x8_unorm_srgb:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		[[fallthrough]];
	case api::format::b8g8r8a8_unorm_srgb:
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

	case api::format::l16_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE };
		return VK_FORMAT_R16_UNORM;
	case api::format::r16_uint:
		return VK_FORMAT_R16_UINT;
	case api::format::r16_sint:
		return VK_FORMAT_R16_SINT;
	case api::format::r16_unorm:
		return VK_FORMAT_R16_UNORM;
	case api::format::r16_snorm:
		return VK_FORMAT_R16_SNORM;
	case api::format::r16_typeless:
	case api::format::r16_float:
		return VK_FORMAT_R16_SFLOAT;

	case api::format::l16a16_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G };
		return VK_FORMAT_R16G16_UNORM;
	case api::format::r16g16_uint:
		return VK_FORMAT_R16G16_UINT;
	case api::format::r16g16_sint:
		return VK_FORMAT_R16G16_SINT;
	case api::format::r16g16_unorm:
		return VK_FORMAT_R16G16_UNORM;
	case api::format::r16g16_snorm:
		return VK_FORMAT_R16G16_SNORM;
	case api::format::r16g16_typeless:
	case api::format::r16g16_float:
		return VK_FORMAT_R16G16_SFLOAT;

	case api::format::r16g16b16_uint:
		return VK_FORMAT_R16G16B16_UINT;
	case api::format::r16g16b16_sint:
		return VK_FORMAT_R16G16B16_SINT;
	case api::format::r16g16b16_unorm:
		return VK_FORMAT_R16G16B16_UNORM;
	case api::format::r16g16b16_snorm:
		return VK_FORMAT_R16G16B16_SNORM;
	case api::format::r16g16b16_typeless:
	case api::format::r16g16b16_float:
		return VK_FORMAT_R16G16B16_SFLOAT;

	case api::format::r16g16b16a16_uint:
		return VK_FORMAT_R16G16B16A16_UINT;
	case api::format::r16g16b16a16_sint:
		return VK_FORMAT_R16G16B16A16_SINT;
	case api::format::r16g16b16a16_unorm:
		return VK_FORMAT_R16G16B16A16_UNORM;
	case api::format::r16g16b16a16_snorm:
		return VK_FORMAT_R16G16B16A16_SNORM;
	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_float:
		return VK_FORMAT_R16G16B16A16_SFLOAT;

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
	case api::format::b5g5r5x1_unorm:
		if (components != nullptr)
			*components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		[[fallthrough]];
	case api::format::b5g5r5a1_unorm:
		return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
	case api::format::b4g4r4a4_unorm:
		return VK_FORMAT_A4R4G4B4_UNORM_PACK16;
	case api::format::a4b4g4r4_unorm:
		return VK_FORMAT_A4B4G4R4_UNORM_PACK16;

	case api::format::s8_uint:
		return VK_FORMAT_S8_UINT;
	case api::format::d16_unorm:
		return VK_FORMAT_D16_UNORM;
	case api::format::d16_unorm_s8_uint:
		return VK_FORMAT_D16_UNORM_S8_UINT;
	case api::format::d24_unorm_x8_uint:
		return VK_FORMAT_X8_D24_UNORM_PACK32;
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
auto reshade::vulkan::convert_format(VkFormat vk_format, const VkComponentMapping *components) -> api::format
{
	switch (vk_format)
	{
	default:
	case VK_FORMAT_UNDEFINED:
		return api::format::unknown;

	case VK_FORMAT_R8_UINT:
		return api::format::r8_uint;
	case VK_FORMAT_R8_SINT:
		return api::format::r8_sint;
	case VK_FORMAT_R8_UNORM:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_R &&
			components->b == VK_COMPONENT_SWIZZLE_R &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::l8_unorm;
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_ZERO &&
			components->g == VK_COMPONENT_SWIZZLE_ZERO &&
			components->b == VK_COMPONENT_SWIZZLE_ZERO &&
			components->a == VK_COMPONENT_SWIZZLE_R)
			return api::format::a8_unorm;
		return api::format::r8_unorm;
	case VK_FORMAT_R8_SNORM:
		return api::format::r8_snorm;

	case VK_FORMAT_R8G8_UINT:
		return api::format::r8g8_uint;
	case VK_FORMAT_R8G8_SINT:
		return api::format::r8g8_sint;
	case VK_FORMAT_R8G8_UNORM:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_R &&
			components->b == VK_COMPONENT_SWIZZLE_R &&
			components->a == VK_COMPONENT_SWIZZLE_G)
			return api::format::l8a8_unorm;
		return api::format::r8g8_unorm;
	case VK_FORMAT_R8G8_SNORM:
		return api::format::r8g8_snorm;

	case VK_FORMAT_R8G8B8_UINT:
		return api::format::r8g8b8_uint;
	case VK_FORMAT_R8G8B8_SINT:
		return api::format::r8g8b8_sint;
	case VK_FORMAT_R8G8B8_UNORM:
		return api::format::r8g8b8_unorm;
	case VK_FORMAT_R8G8B8_SRGB:
		return api::format::r8g8b8_unorm_srgb;
	case VK_FORMAT_R8G8B8_SNORM:
		return api::format::r8g8b8_snorm;

	case VK_FORMAT_B8G8R8_UNORM:
		return api::format::b8g8r8_unorm;
	case VK_FORMAT_B8G8R8_SRGB:
		return api::format::b8g8r8_unorm_srgb;

	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_A8B8G8R8_UINT_PACK32:
		return api::format::r8g8b8a8_uint;
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_A8B8G8R8_SINT_PACK32:
		return api::format::r8g8b8a8_sint;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_G &&
			components->b == VK_COMPONENT_SWIZZLE_B &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::r8g8b8x8_unorm;
		return api::format::r8g8b8a8_unorm;
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_G &&
			components->b == VK_COMPONENT_SWIZZLE_B &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::r8g8b8x8_unorm_srgb;
		return api::format::r8g8b8a8_unorm_srgb;
	case VK_FORMAT_R8G8B8A8_SNORM:
	case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
		return api::format::r8g8b8a8_snorm;

	case VK_FORMAT_B8G8R8A8_UNORM:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_G &&
			components->b == VK_COMPONENT_SWIZZLE_B &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::b8g8r8x8_unorm;
		return api::format::b8g8r8a8_unorm;
	case VK_FORMAT_B8G8R8A8_SRGB:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_G &&
			components->b == VK_COMPONENT_SWIZZLE_B &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::b8g8r8x8_unorm_srgb;
		return api::format::b8g8r8a8_unorm_srgb;

	case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		return api::format::r10g10b10a2_uint;
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		return api::format::r10g10b10a2_unorm;

	case VK_FORMAT_A2R10G10B10_UINT_PACK32:
		return api::format::b10g10r10a2_uint;
	case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		return api::format::b10g10r10a2_unorm;

	case VK_FORMAT_R16_UINT:
		return api::format::r16_uint;
	case VK_FORMAT_R16_SINT:
		return api::format::r16_sint;
	case VK_FORMAT_R16_UNORM:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_R &&
			components->b == VK_COMPONENT_SWIZZLE_R &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::l16_unorm;
		return api::format::r16_unorm;
	case VK_FORMAT_R16_SNORM:
		return api::format::r16_snorm;
	case VK_FORMAT_R16_SFLOAT:
		return api::format::r16_float;

	case VK_FORMAT_R16G16_UINT:
		return api::format::r16g16_uint;
	case VK_FORMAT_R16G16_SINT:
		return api::format::r16g16_sint;
	case VK_FORMAT_R16G16_UNORM:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_R &&
			components->b == VK_COMPONENT_SWIZZLE_R &&
			components->a == VK_COMPONENT_SWIZZLE_G)
			return api::format::l16a16_unorm;
		return api::format::r16g16_unorm;
	case VK_FORMAT_R16G16_SNORM:
		return api::format::r16g16_snorm;
	case VK_FORMAT_R16G16_SFLOAT:
		return api::format::r16g16_float;

	case VK_FORMAT_R16G16B16_UINT:
		return api::format::r16g16b16_uint;
	case VK_FORMAT_R16G16B16_SINT:
		return api::format::r16g16b16_sint;
	case VK_FORMAT_R16G16B16_UNORM:
		return api::format::r16g16b16_unorm;
	case VK_FORMAT_R16G16B16_SNORM:
		return api::format::r16g16b16_snorm;
	case VK_FORMAT_R16G16B16_SFLOAT:
		return api::format::r16g16b16_float;

	case VK_FORMAT_R16G16B16A16_UINT:
		return api::format::r16g16b16a16_uint;
	case VK_FORMAT_R16G16B16A16_SINT:
		return api::format::r16g16b16a16_sint;
	case VK_FORMAT_R16G16B16A16_UNORM:
		return api::format::r16g16b16a16_unorm;
	case VK_FORMAT_R16G16B16A16_SNORM:
		return api::format::r16g16b16a16_snorm;
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

	case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
		return api::format::r9g9b9e5;
	case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
		return api::format::r11g11b10_float;
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
		return api::format::b5g6r5_unorm;
	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
		if (components != nullptr &&
			components->r == VK_COMPONENT_SWIZZLE_R &&
			components->g == VK_COMPONENT_SWIZZLE_G &&
			components->b == VK_COMPONENT_SWIZZLE_B &&
			components->a == VK_COMPONENT_SWIZZLE_ONE)
			return api::format::b5g5r5x1_unorm;
		return api::format::b5g5r5a1_unorm;
	case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
		return api::format::b4g4r4a4_unorm;
	case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
		return api::format::a4b4g4r4_unorm;

	case VK_FORMAT_D16_UNORM:
		return api::format::d16_unorm;
	case VK_FORMAT_X8_D24_UNORM_PACK32:
		return api::format::d24_unorm_x8_uint;
	case VK_FORMAT_S8_UINT:
		return api::format::s8_uint;
	case VK_FORMAT_D16_UNORM_S8_UINT:
		return api::format::d16_unorm_s8_uint;
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return api::format::d24_unorm_s8_uint;
	case VK_FORMAT_D32_SFLOAT:
		return api::format::d32_float;
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

auto reshade::vulkan::convert_color_space(api::color_space color_space) -> VkColorSpaceKHR
{
	switch (color_space)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::color_space::srgb_nonlinear:
		return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	case api::color_space::extended_srgb_linear:
		return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
	case api::color_space::hdr10_st2084:
		return VK_COLOR_SPACE_HDR10_ST2084_EXT;
	case api::color_space::hdr10_hlg:
		return VK_COLOR_SPACE_HDR10_HLG_EXT;
	}
}
auto reshade::vulkan::convert_color_space(VkColorSpaceKHR color_space) -> api::color_space
{
	switch (color_space)
	{
	default:
		assert(false);
		return api::color_space::unknown;
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
		return api::color_space::srgb_nonlinear;
	case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
		return api::color_space::extended_srgb_linear;
	case VK_COLOR_SPACE_HDR10_ST2084_EXT:
		return api::color_space::hdr10_st2084;
	case VK_COLOR_SPACE_HDR10_HLG_EXT:
		return api::color_space::hdr10_hlg;
	}
}

auto reshade::vulkan::convert_access_to_usage(VkAccessFlags2 flags) -> api::resource_usage
{
	static_assert(
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT == VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT &&
		VK_ACCESS_INDEX_READ_BIT == VK_ACCESS_2_INDEX_READ_BIT &&
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT == VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT &&
		VK_ACCESS_UNIFORM_READ_BIT == VK_ACCESS_2_UNIFORM_READ_BIT &&
		VK_ACCESS_SHADER_READ_BIT == VK_ACCESS_2_SHADER_READ_BIT &&
		VK_ACCESS_SHADER_WRITE_BIT == VK_ACCESS_2_SHADER_WRITE_BIT &&
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT == VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT &&
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT == VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT &&
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT == VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT &&
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT == VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT &&
		VK_ACCESS_TRANSFER_READ_BIT == VK_ACCESS_2_TRANSFER_READ_BIT &&
		VK_ACCESS_TRANSFER_WRITE_BIT == VK_ACCESS_2_TRANSFER_WRITE_BIT &&
		VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT == VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT &&
		VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR == VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR &&
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR == VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR);

	api::resource_usage result = api::resource_usage::undefined;
	if ((flags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
		result |= api::resource_usage::indirect_argument;
	if ((flags & VK_ACCESS_INDEX_READ_BIT) != 0)
		result |= api::resource_usage::index_buffer;
	if ((flags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) != 0)
		result |= api::resource_usage::vertex_buffer;
	if ((flags & VK_ACCESS_UNIFORM_READ_BIT) != 0)
		result |= api::resource_usage::constant_buffer;
	if ((flags & VK_ACCESS_SHADER_READ_BIT) != 0)
		result |= api::resource_usage::shader_resource;
	if ((flags & VK_ACCESS_SHADER_WRITE_BIT) != 0)
		result |= api::resource_usage::unordered_access;
	if ((flags & (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
		result |= api::resource_usage::render_target;
	if ((flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) != 0)
		result |= api::resource_usage::depth_stencil_read;
	if ((flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0)
		result |= api::resource_usage::depth_stencil_write;
	if ((flags & VK_ACCESS_TRANSFER_READ_BIT) != 0)
		result |= api::resource_usage::copy_source;
	if ((flags & VK_ACCESS_TRANSFER_WRITE_BIT) != 0)
		result |= api::resource_usage::copy_dest;
	if ((flags & VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT) != 0)
		result |= api::resource_usage::stream_output;
	if ((flags & (VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)) != 0)
		result |= api::resource_usage::acceleration_structure;

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
	// case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
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
	if ((state & api::resource_usage::index_buffer) != 0)
		result |= VK_ACCESS_INDEX_READ_BIT;
	if ((state & api::resource_usage::vertex_buffer) != 0)
		result |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	if ((state & api::resource_usage::constant_buffer) != 0)
		result |= VK_ACCESS_UNIFORM_READ_BIT;
	if ((state & api::resource_usage::stream_output) != 0)
		result |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
	if ((state & api::resource_usage::indirect_argument) != 0)
		result |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	if ((state & api::resource_usage::depth_stencil_read) != 0)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if ((state & api::resource_usage::depth_stencil_write) != 0)
		result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if ((state & api::resource_usage::render_target) != 0)
		result |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if ((state & api::resource_usage::shader_resource) != 0)
		result |= VK_ACCESS_SHADER_READ_BIT;
	if ((state & api::resource_usage::unordered_access) != 0)
		result |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	if ((state & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != 0)
		result |= VK_ACCESS_TRANSFER_WRITE_BIT;
	if ((state & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != 0)
		result |= VK_ACCESS_TRANSFER_READ_BIT;
	if ((state & api::resource_usage::acceleration_structure) != 0)
		result |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

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
auto reshade::vulkan::convert_usage_to_pipeline_stage(api::resource_usage state, bool src_stage, const VkPhysicalDeviceFeatures &enabled_features, bool enabled_ray_tracing) -> VkPipelineStageFlags
{
	if (state == api::resource_usage::general)
		return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	if (state == api::resource_usage::present || state == api::resource_usage::undefined)
		// Do not introduce a execution dependency
		return src_stage ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	if (state == api::resource_usage::cpu_access)
		return VK_PIPELINE_STAGE_HOST_BIT;

	VkPipelineStageFlags result = 0;
	if ((state & (api::resource_usage::index_buffer | api::resource_usage::vertex_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	if ((state & api::resource_usage::stream_output) != 0)
		result |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
	if ((state & api::resource_usage::indirect_argument) != 0)
		result |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	if ((state & api::resource_usage::depth_stencil_read) != 0)
		result |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	if ((state & api::resource_usage::depth_stencil_write) != 0)
		result |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	if ((state & api::resource_usage::render_target) != 0)
		result |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if ((state & (api::resource_usage::shader_resource_pixel | api::resource_usage::constant_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if ((state & (api::resource_usage::shader_resource_non_pixel | api::resource_usage::constant_buffer)) != 0)
		result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | (enabled_features.tessellationShader ? VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT : 0) | (enabled_features.geometryShader ? VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT : 0) | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | (enabled_ray_tracing ? VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : 0);
	if ((state & api::resource_usage::unordered_access) != 0)
		result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if ((state & (api::resource_usage::copy_dest | api::resource_usage::copy_source | api::resource_usage::resolve_dest | api::resource_usage::resolve_source)) != 0)
		result |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	if ((state & api::resource_usage::acceleration_structure) != 0)
		result |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

	return result;
}

void reshade::vulkan::convert_usage_to_image_usage_flags(api::resource_usage usage, VkImageUsageFlags &image_flags)
{
	if ((usage & api::resource_usage::depth_stencil) != 0)
		// Add transfer destination usage as well to support clearing via 'vkCmdClearDepthStencilImage'
		image_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if ((usage & api::resource_usage::render_target) != 0)
		// Add transfer destination usage as well to support clearing via 'vkCmdClearColorImage'
		image_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if ((usage & api::resource_usage::shader_resource) != 0)
		image_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_SAMPLED_BIT;

	if ((usage & api::resource_usage::unordered_access) != 0)
		image_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_STORAGE_BIT;

	if ((usage & (api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != 0)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if ((usage & (api::resource_usage::copy_source | api::resource_usage::resolve_source)) != 0)
		image_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	else
		image_flags &= ~VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
}
void reshade::vulkan::convert_image_usage_flags_to_usage(const VkImageUsageFlags image_flags, api::resource_usage &usage)
{
	using namespace reshade;

	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= api::resource_usage::copy_source;
	if ((image_flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= api::resource_usage::copy_dest;
	if ((image_flags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
		usage |= api::resource_usage::shader_resource;
	if ((image_flags & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
		usage |= api::resource_usage::unordered_access;
	if ((image_flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
		usage |= api::resource_usage::render_target;
	if ((image_flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		usage |= api::resource_usage::depth_stencil;
}
void reshade::vulkan::convert_usage_to_buffer_usage_flags(api::resource_usage usage, VkBufferUsageFlags &buffer_flags)
{
	if ((usage & api::resource_usage::index_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if ((usage & api::resource_usage::vertex_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if ((usage & api::resource_usage::constant_buffer) != 0)
		buffer_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	if ((usage & api::resource_usage::stream_output) != 0)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
	else
		buffer_flags &= ~(VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);

	if ((usage & api::resource_usage::indirect_argument) != 0)
		buffer_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	if ((usage & api::resource_usage::shader_resource) != 0)
		buffer_flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

	if ((usage & api::resource_usage::shader_resource) == api::resource_usage::shader_resource_non_pixel)
		buffer_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	if ((usage & api::resource_usage::unordered_access) != 0)
		buffer_flags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	else
		buffer_flags &= ~(VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	if ((usage & api::resource_usage::copy_dest) != 0)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if ((usage & api::resource_usage::copy_source) != 0)
		buffer_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if ((usage & api::resource_usage::acceleration_structure) != 0)
		buffer_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	else
		buffer_flags &= ~VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
}
void reshade::vulkan::convert_buffer_usage_flags_to_usage(const VkBufferUsageFlags2 buffer_flags, api::resource_usage &usage)
{
	using namespace reshade;

	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0)
		usage |= api::resource_usage::copy_source;
	if ((buffer_flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0)
		usage |= api::resource_usage::copy_dest;
	if ((buffer_flags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) != 0)
		usage |= api::resource_usage::shader_resource;
	if ((buffer_flags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) != 0)
		usage |= api::resource_usage::unordered_access;
	if ((buffer_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0)
		usage |= api::resource_usage::constant_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
		usage |= api::resource_usage::unordered_access;
	if ((buffer_flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0)
		usage |= api::resource_usage::index_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0)
		usage |= api::resource_usage::vertex_buffer;
	if ((buffer_flags & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0)
		usage |= api::resource_usage::indirect_argument;
	if ((buffer_flags & (VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT)) != 0)
		usage |= api::resource_usage::stream_output;
	if ((buffer_flags & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR) != 0)
		usage |= api::resource_usage::acceleration_structure;
	if ((buffer_flags & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR) != 0)
		usage |= api::resource_usage::shader_resource_non_pixel | api::resource_usage::unordered_access;
	if ((buffer_flags & VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR) != 0)
		usage |= api::resource_usage::shader_resource_non_pixel;
}

void reshade::vulkan::convert_sampler_desc(const api::sampler_desc &desc, VkSamplerCreateInfo &create_info)
{
	create_info.compareEnable = VK_FALSE;

	switch (desc.filter)
	{
	case api::filter_mode::compare_min_mag_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_mag_mip_point:
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_mag_point_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_point_mag_linear_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_point_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_point_mag_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_point_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_NEAREST;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_linear_mag_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_linear_mag_mip_point:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_linear_mag_point_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_linear_mag_point_mip_linear:
		create_info.magFilter = VK_FILTER_NEAREST;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_mag_linear_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_mag_linear_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_mag_mip_linear:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_mag_mip_linear:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.anisotropyEnable = VK_FALSE;
		break;
	case api::filter_mode::compare_min_mag_anisotropic_mip_point:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::min_mag_anisotropic_mip_point:
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		create_info.anisotropyEnable = VK_TRUE;
		break;
	case api::filter_mode::compare_anisotropic:
		create_info.compareEnable = VK_TRUE;
		[[fallthrough]];
	case api::filter_mode::anisotropic:
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

	const auto border_color_info = const_cast<VkSamplerCustomBorderColorCreateInfoEXT *>(
		find_in_structure_chain<VkSamplerCustomBorderColorCreateInfoEXT>(create_info.pNext, VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT));

	const bool is_float_border_color =
		create_info.borderColor != VK_BORDER_COLOR_INT_TRANSPARENT_BLACK &&
		create_info.borderColor != VK_BORDER_COLOR_INT_OPAQUE_BLACK &&
		create_info.borderColor != VK_BORDER_COLOR_INT_OPAQUE_WHITE &&
		create_info.borderColor != VK_BORDER_COLOR_INT_CUSTOM_EXT;

	if (border_color_info == nullptr)
	{
		if (desc.border_color[3] == 0.0f)
			create_info.borderColor = is_float_border_color ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		else if (desc.border_color[0] == 0.0f && desc.border_color[1] == 0.0f && desc.border_color[2] == 0.0f)
			create_info.borderColor = is_float_border_color ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK : VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		else
			create_info.borderColor = is_float_border_color ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE : VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	}
	else
	{
		if (is_float_border_color)
		{
			create_info.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
			std::copy_n(desc.border_color, 4, border_color_info->customBorderColor.float32);
		}
		else
		{
			create_info.borderColor = VK_BORDER_COLOR_INT_CUSTOM_EXT;
			for (int i = 0; i < 4; ++i)
				border_color_info->customBorderColor.int32[i] = static_cast<int>(desc.border_color[i]);
		}
	}
}
reshade::api::sampler_desc reshade::vulkan::convert_sampler_desc(const VkSamplerCreateInfo &create_info)
{
	api::sampler_desc desc = {};
	if (create_info.anisotropyEnable)
	{
		switch (create_info.mipmapMode)
		{
		case VK_SAMPLER_MIPMAP_MODE_NEAREST:
			desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_mag_anisotropic_mip_point : api::filter_mode::min_mag_anisotropic_mip_point;
			break;
		case VK_SAMPLER_MIPMAP_MODE_LINEAR:
			desc.filter = create_info.compareEnable ? api::filter_mode::compare_anisotropic : api::filter_mode::anisotropic;
			break;
		}
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
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_mag_mip_point : api::filter_mode::min_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_mag_point_mip_linear : api::filter_mode::min_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_point_mag_linear_mip_point : api::filter_mode::min_point_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_point_mag_mip_linear : api::filter_mode::min_point_mag_mip_linear;
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
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_linear_mag_mip_point : api::filter_mode::min_linear_mag_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_linear_mag_point_mip_linear : api::filter_mode::min_linear_mag_point_mip_linear;
					break;
				}
				break;
			case VK_FILTER_LINEAR:
				switch (create_info.mipmapMode)
				{
				case VK_SAMPLER_MIPMAP_MODE_NEAREST:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_mag_linear_mip_point : api::filter_mode::min_mag_linear_mip_point;
					break;
				case VK_SAMPLER_MIPMAP_MODE_LINEAR:
					desc.filter = create_info.compareEnable ? api::filter_mode::compare_min_mag_mip_linear : api::filter_mode::min_mag_mip_linear;
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

	const auto border_color_info =
		find_in_structure_chain<VkSamplerCustomBorderColorCreateInfoEXT>(create_info.pNext, VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT);

	switch (create_info.borderColor)
	{
	case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
	case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
		break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
	case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
		desc.border_color[3] = 1.0f;
		break;
	case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
	case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
		std::fill_n(desc.border_color, 4, 1.0f);
		break;
	case VK_BORDER_COLOR_FLOAT_CUSTOM_EXT:
		assert(border_color_info != nullptr);
		std::copy_n(border_color_info->customBorderColor.float32, 4, desc.border_color);
		break;
	case VK_BORDER_COLOR_INT_CUSTOM_EXT:
		assert(border_color_info != nullptr);
		for (int i = 0; i < 4; ++i)
			desc.border_color[i] = static_cast<float>(border_color_info->customBorderColor.int32[i]);
		break;
	}

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

	if ((desc.flags & api::resource_flags::sparse_binding) != 0)
		create_info.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	else
		create_info.flags &= ~(VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

	if ((desc.flags & api::resource_flags::cube_compatible) != 0)
		create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	else
		create_info.flags &= ~VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	// Mipmap generation is using 'vkCmdBlitImage' and therefore needs transfer usage flags (see 'command_list_impl::generate_mipmaps')
	if ((desc.flags & api::resource_flags::generate_mipmaps) != 0)
		create_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// Dynamic resources do not exist in Vulkan
	assert((desc.flags & api::resource_flags::dynamic) == 0);
}
void reshade::vulkan::convert_resource_desc(const api::resource_desc &desc, VkBufferCreateInfo &create_info)
{
	assert(desc.type == api::resource_type::buffer);

	create_info.size = desc.buffer.size;
	convert_usage_to_buffer_usage_flags(desc.usage, create_info.usage);

	// Dynamic resources do not exist in Vulkan
	assert((desc.flags & api::resource_flags::dynamic) == 0);
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
	if (desc.type == api::resource_type::texture_2d)
	{
		if (desc.texture.samples > 1)
		{
			if ((create_info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
				desc.usage |= api::resource_usage::resolve_source;
		}
		else
		{
			// Images can only be used as resolve destination when also having color attachment or depth-stencil attachment usage
			if ((create_info.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 && (create_info.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
				desc.usage |= api::resource_usage::resolve_dest;
		}
	}

	if ((create_info.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) != 0)
		desc.flags |= api::resource_flags::sparse_binding;

	if ((create_info.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) != 0 &&
		// Do not convert depth-stencil formats to typeless variant, as that breaks later conversion back to 'VkFormat'
		(create_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
		desc.texture.format = api::format_to_typeless(desc.texture.format);

	if ((create_info.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)
		desc.flags |= api::resource_flags::cube_compatible;

	// Images that have both transfer usage flags are usable with the 'generate_mipmaps' function
	if (create_info.mipLevels > 1 && (create_info.usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) == (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		desc.flags |= api::resource_flags::generate_mipmaps;

	if (const auto external_memory_info = find_in_structure_chain<VkExternalMemoryImageCreateInfo>(
			create_info.pNext, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO))
	{
		if (external_memory_info->handleTypes != 0)
		{
			desc.flags |= api::resource_flags::shared;

			if ((external_memory_info->handleTypes & (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT)) != 0)
				desc.flags |= api::resource_flags::shared_nt_handle;
		}
	}

	return desc;
}
reshade::api::resource_desc reshade::vulkan::convert_resource_desc(const VkBufferCreateInfo &create_info)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = create_info.size;
	desc.buffer.stride = 0;

	VkBufferUsageFlags2 usage = create_info.usage;
	if (const auto usage_flags_info = find_in_structure_chain<VkBufferUsageFlags2CreateInfo>(
			create_info.pNext, VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO))
		usage = usage_flags_info->usage;
	convert_buffer_usage_flags_to_usage(usage, desc.usage);

	if (const auto external_memory_info = find_in_structure_chain<VkExternalMemoryBufferCreateInfo>(
			create_info.pNext, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO))
	{
		if (external_memory_info->handleTypes != 0)
		{
			desc.flags |= api::resource_flags::shared;

			if ((external_memory_info->handleTypes & (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT | VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT)) != 0)
				desc.flags |= api::resource_flags::shared_nt_handle;
		}
	}

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

	if (const VkFormat format = convert_format(desc.format, &create_info.components);
		format != VK_FORMAT_UNDEFINED)
		create_info.format = format;

	create_info.subresourceRange.baseMipLevel = desc.texture.first_level;
	create_info.subresourceRange.levelCount = desc.texture.level_count;
	create_info.subresourceRange.baseArrayLayer = desc.texture.first_layer;
	create_info.subresourceRange.layerCount = desc.texture.layer_count;
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
void reshade::vulkan::convert_resource_view_desc(const api::resource_view_desc &desc, VkAccelerationStructureCreateInfoKHR &create_info)
{
	assert(desc.type == api::resource_view_type::acceleration_structure);

	create_info.offset = desc.buffer.offset;
	create_info.size = desc.buffer.size;
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

	desc.format = convert_format(create_info.format, &create_info.components);
	desc.texture.first_level = create_info.subresourceRange.baseMipLevel;
	desc.texture.level_count = create_info.subresourceRange.levelCount;
	desc.texture.first_layer = create_info.subresourceRange.baseArrayLayer;
	desc.texture.layer_count = create_info.subresourceRange.layerCount;

	return desc;
}
reshade::api::resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkBufferViewCreateInfo &create_info)
{
	api::resource_view_desc desc = {};
	desc.type = api::resource_view_type::buffer;
	desc.format = convert_format(create_info.format);
	desc.buffer.offset = create_info.offset;
	desc.buffer.size = create_info.range;

	return desc;
}
reshade::api::resource_view_desc reshade::vulkan::convert_resource_view_desc(const VkAccelerationStructureCreateInfoKHR &create_info)
{
	api::resource_view_desc desc = {};
	desc.type = api::resource_view_type::acceleration_structure;
	desc.buffer.offset = create_info.offset;
	desc.buffer.size = create_info.size;

	return desc;
}

void reshade::vulkan::convert_dynamic_states(uint32_t count, const api::dynamic_state *states, std::vector<VkDynamicState> &internal_states)
{
	internal_states.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::depth_bias:
		case api::dynamic_state::depth_bias_clamp:
		case api::dynamic_state::depth_bias_slope_scaled:
			internal_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
			break;
		case api::dynamic_state::blend_constant:
			internal_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
			break;
		case api::dynamic_state::front_stencil_read_mask:
		case api::dynamic_state::back_stencil_read_mask:
			internal_states.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
			break;
		case api::dynamic_state::front_stencil_write_mask:
		case api::dynamic_state::back_stencil_write_mask:
			internal_states.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
			break;
		case api::dynamic_state::front_stencil_reference_value:
		case api::dynamic_state::back_stencil_reference_value:
			internal_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
			break;
		case api::dynamic_state::cull_mode:
			internal_states.push_back(VK_DYNAMIC_STATE_CULL_MODE);
			break;
		case api::dynamic_state::front_counter_clockwise:
			internal_states.push_back(VK_DYNAMIC_STATE_FRONT_FACE);
			break;
		case api::dynamic_state::primitive_topology:
			internal_states.push_back(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
			break;
		case api::dynamic_state::depth_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
			break;
		case api::dynamic_state::depth_write_mask:
			internal_states.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
			break;
		case api::dynamic_state::depth_func:
			internal_states.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
			break;
		case api::dynamic_state::stencil_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
			break;
		case api::dynamic_state::front_stencil_func:
		case api::dynamic_state::back_stencil_func:
			internal_states.push_back(VK_DYNAMIC_STATE_STENCIL_OP);
			break;
		case api::dynamic_state::logic_op:
			internal_states.push_back(VK_DYNAMIC_STATE_LOGIC_OP_EXT);
			break;
		case api::dynamic_state::fill_mode:
			internal_states.push_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
			break;
		case api::dynamic_state::multisample_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
			break;
		case api::dynamic_state::sample_mask:
			internal_states.push_back(VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);
			break;
		case api::dynamic_state::alpha_to_coverage_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
			break;
		case api::dynamic_state::logic_op_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT);
			break;
		case api::dynamic_state::blend_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
			break;
		case api::dynamic_state::source_color_blend_factor:
		case api::dynamic_state::dest_color_blend_factor:
		case api::dynamic_state::color_blend_op:
		case api::dynamic_state::source_alpha_blend_factor:
		case api::dynamic_state::dest_alpha_blend_factor:
		case api::dynamic_state::alpha_blend_op:
			internal_states.push_back(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
			break;
		case api::dynamic_state::render_target_write_mask:
			internal_states.push_back(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
			break;
		case api::dynamic_state::depth_clip_enable:
			internal_states.push_back(VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);
			break;
		case api::dynamic_state::ray_tracing_pipeline_stack_size:
			internal_states.push_back(VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR);
			break;
		default:
			assert(false);
			break;
		}
	}
}
std::vector<reshade::api::dynamic_state> reshade::vulkan::convert_dynamic_states(const VkPipelineDynamicStateCreateInfo *create_info)
{
	if (create_info == nullptr)
		return {};

	std::vector<api::dynamic_state> states;
	states.reserve(create_info->dynamicStateCount);

	for (uint32_t i = 0; i < create_info->dynamicStateCount; ++i)
	{
		switch (create_info->pDynamicStates[i])
		{
		case VK_DYNAMIC_STATE_DEPTH_BIAS:
			states.push_back(api::dynamic_state::depth_bias);
			states.push_back(api::dynamic_state::depth_bias_clamp);
			states.push_back(api::dynamic_state::depth_bias_slope_scaled);
			break;
		case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
			states.push_back(api::dynamic_state::blend_constant);
			break;
		case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
			states.push_back(api::dynamic_state::front_stencil_read_mask);
			states.push_back(api::dynamic_state::back_stencil_read_mask);
			break;
		case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
			states.push_back(api::dynamic_state::front_stencil_write_mask);
			states.push_back(api::dynamic_state::back_stencil_write_mask);
			break;
		case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
			states.push_back(api::dynamic_state::front_stencil_reference_value);
			states.push_back(api::dynamic_state::back_stencil_reference_value);
			break;
		case VK_DYNAMIC_STATE_CULL_MODE:
			states.push_back(api::dynamic_state::cull_mode);
			break;
		case VK_DYNAMIC_STATE_FRONT_FACE:
			states.push_back(api::dynamic_state::front_counter_clockwise);
			break;
		case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
			states.push_back(api::dynamic_state::primitive_topology);
			break;
		case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:
			states.push_back(api::dynamic_state::depth_enable);
			break;
		case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:
			states.push_back(api::dynamic_state::depth_write_mask);
			break;
		case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:
			states.push_back(api::dynamic_state::depth_func);
			break;
		case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:
			states.push_back(api::dynamic_state::stencil_enable);
			break;
		case VK_DYNAMIC_STATE_STENCIL_OP:
			states.push_back(api::dynamic_state::front_stencil_func);
			states.push_back(api::dynamic_state::back_stencil_func);
			break;
		case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:
			states.push_back(api::dynamic_state::depth_bias);
			states.push_back(api::dynamic_state::depth_bias_clamp);
			states.push_back(api::dynamic_state::depth_bias_slope_scaled);
			break;
		case VK_DYNAMIC_STATE_LOGIC_OP_EXT:
			states.push_back(api::dynamic_state::logic_op);
			break;
		case VK_DYNAMIC_STATE_POLYGON_MODE_EXT:
			states.push_back(api::dynamic_state::fill_mode);
			break;
		case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT:
			states.push_back(api::dynamic_state::multisample_enable);
			break;
		case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:
			states.push_back(api::dynamic_state::sample_mask);
			break;
		case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT:
			states.push_back(api::dynamic_state::alpha_to_coverage_enable);
			break;
		case VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT:
			states.push_back(api::dynamic_state::logic_op_enable);
			break;
		case VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT:
			states.push_back(api::dynamic_state::blend_enable);
			break;
		case VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT:
			states.push_back(api::dynamic_state::source_color_blend_factor);
			states.push_back(api::dynamic_state::dest_color_blend_factor);
			states.push_back(api::dynamic_state::color_blend_op);
			states.push_back(api::dynamic_state::source_alpha_blend_factor);
			states.push_back(api::dynamic_state::dest_alpha_blend_factor);
			states.push_back(api::dynamic_state::alpha_blend_op);
			break;
		case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT:
			states.push_back(api::dynamic_state::render_target_write_mask);
			break;
		case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT:
			states.push_back(api::dynamic_state::depth_clip_enable);
			break;
		case VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR:
			states.push_back(api::dynamic_state::ray_tracing_pipeline_stack_size);
			break;
		}
	}

	return states;
}

void reshade::vulkan::convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<VkVertexInputBindingDescription> &vertex_bindings, std::vector<VkVertexInputAttributeDescription> &vertex_attributes)
{
	vertex_attributes.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::input_element &element = elements[i];

		VkVertexInputAttributeDescription &attribute = vertex_attributes.emplace_back();
		attribute.location = element.location;
		attribute.binding = element.buffer_binding;
		attribute.format = convert_format(element.format);
		attribute.offset = element.offset;

		assert(element.instance_step_rate <= 1);
		const VkVertexInputRate input_rate = element.instance_step_rate > 0 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

		if (const auto it = std::find_if(vertex_bindings.cbegin(), vertex_bindings.cend(),
				[&element](const VkVertexInputBindingDescription &input_binding) { return input_binding.binding == element.buffer_binding; });
			it != vertex_bindings.cend())
		{
			assert(it->inputRate == input_rate && it->stride == element.stride);
		}
		else
		{
			VkVertexInputBindingDescription &binding = vertex_bindings.emplace_back();
			binding.binding = element.buffer_binding;
			binding.stride = element.stride;
			binding.inputRate = input_rate;
		}
	}
}
std::vector<reshade::api::input_element> reshade::vulkan::convert_input_layout_desc(const VkPipelineVertexInputStateCreateInfo *create_info)
{
	if (create_info == nullptr)
		return {};

	std::vector<api::input_element> elements;
	elements.resize(create_info->vertexAttributeDescriptionCount);

	for (uint32_t a = 0; a < create_info->vertexAttributeDescriptionCount; ++a)
	{
		const VkVertexInputAttributeDescription &attribute = create_info->pVertexAttributeDescriptions[a];

		elements[a].location = attribute.location;
		elements[a].format = convert_format(attribute.format);
		elements[a].buffer_binding = attribute.binding;
		elements[a].offset = attribute.offset;

		for (uint32_t b = 0; b < create_info->vertexBindingDescriptionCount; ++b)
		{
			const VkVertexInputBindingDescription &binding = create_info->pVertexBindingDescriptions[b];

			if (binding.binding == attribute.binding)
			{
				elements[a].stride = binding.stride;
				elements[a].instance_step_rate = binding.inputRate != VK_VERTEX_INPUT_RATE_VERTEX ? 1 : 0;
				break;
			}
		}
	}

	return elements;
}

void reshade::vulkan::convert_stream_output_desc(const api::stream_output_desc &desc, VkPipelineRasterizationStateCreateInfo &create_info)
{
	if (const auto stream_info = const_cast<VkPipelineRasterizationStateStreamCreateInfoEXT *>(
			find_in_structure_chain<VkPipelineRasterizationStateStreamCreateInfoEXT>(create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT)))
	{
		stream_info->rasterizationStream = desc.rasterized_stream;
	}
}
reshade::api::stream_output_desc reshade::vulkan::convert_stream_output_desc(const VkPipelineRasterizationStateCreateInfo *create_info)
{
	api::stream_output_desc desc = {};

	if (create_info != nullptr)
	{
		if (const auto stream_info = find_in_structure_chain<VkPipelineRasterizationStateStreamCreateInfoEXT>(
				create_info->pNext, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT))
		{
			desc.rasterized_stream = stream_info->rasterizationStream;
		}
	}

	return desc;
}
void reshade::vulkan::convert_blend_desc(const api::blend_desc &desc, VkPipelineColorBlendStateCreateInfo &create_info, VkPipelineMultisampleStateCreateInfo &multisample_create_info)
{
	create_info.logicOpEnable = desc.logic_op_enable[0];
	create_info.logicOp = convert_logic_op(desc.logic_op[0]);
	std::copy_n(desc.blend_constant, 4, create_info.blendConstants);

	for (uint32_t a = 0; a < create_info.attachmentCount && a < 8; ++a)
	{
		VkPipelineColorBlendAttachmentState &attachment = const_cast<VkPipelineColorBlendAttachmentState *>(create_info.pAttachments)[a];

		attachment.blendEnable = desc.blend_enable[a];
		attachment.srcColorBlendFactor = convert_blend_factor(desc.source_color_blend_factor[a]);
		attachment.dstColorBlendFactor = convert_blend_factor(desc.dest_color_blend_factor[a]);
		attachment.colorBlendOp = convert_blend_op(desc.color_blend_op[a]);
		attachment.srcAlphaBlendFactor = convert_blend_factor(desc.source_alpha_blend_factor[a]);
		attachment.dstAlphaBlendFactor = convert_blend_factor(desc.dest_alpha_blend_factor[a]);
		attachment.alphaBlendOp = convert_blend_op(desc.alpha_blend_op[a]);
		attachment.colorWriteMask = desc.render_target_write_mask[a];
	}

	multisample_create_info.alphaToCoverageEnable = desc.alpha_to_coverage_enable;
}
reshade::api::blend_desc reshade::vulkan::convert_blend_desc(const VkPipelineColorBlendStateCreateInfo *create_info, const VkPipelineMultisampleStateCreateInfo *multisample_create_info)
{
	api::blend_desc desc = {};

	if (create_info != nullptr)
	{
		std::copy_n(create_info->blendConstants, 4, desc.blend_constant);

		for (uint32_t a = 0; a < create_info->attachmentCount && a < 8; ++a)
		{
			const VkPipelineColorBlendAttachmentState &attachment = create_info->pAttachments[a];

			desc.blend_enable[a] = attachment.blendEnable;
			desc.logic_op_enable[a] = create_info->logicOpEnable;
			desc.color_blend_op[a] = convert_blend_op(attachment.colorBlendOp);
			desc.source_color_blend_factor[a] = convert_blend_factor(attachment.srcColorBlendFactor);
			desc.dest_color_blend_factor[a] = convert_blend_factor(attachment.dstColorBlendFactor);
			desc.alpha_blend_op[a] = convert_blend_op(attachment.alphaBlendOp);
			desc.source_alpha_blend_factor[a] = convert_blend_factor(attachment.srcAlphaBlendFactor);
			desc.dest_alpha_blend_factor[a] = convert_blend_factor(attachment.dstAlphaBlendFactor);
			desc.logic_op[a] = convert_logic_op(create_info->logicOp);
			desc.render_target_write_mask[a] = static_cast<uint8_t>(attachment.colorWriteMask);
		}
	}

	if (multisample_create_info != nullptr)
	{
		desc.alpha_to_coverage_enable = multisample_create_info->alphaToCoverageEnable;
	}

	return desc;
}
void reshade::vulkan::convert_rasterizer_desc(const api::rasterizer_desc &desc, VkPipelineRasterizationStateCreateInfo &create_info)
{
	create_info.depthClampEnable = !desc.depth_clip_enable;
	create_info.polygonMode = convert_fill_mode(desc.fill_mode);
	create_info.cullMode = convert_cull_mode(desc.cull_mode);
	create_info.frontFace = desc.front_counter_clockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
	create_info.depthBiasEnable = desc.depth_bias != 0 || desc.depth_bias_clamp != 0 || desc.slope_scaled_depth_bias != 0;
	create_info.depthBiasConstantFactor = desc.depth_bias;
	create_info.depthBiasClamp = desc.depth_bias_clamp;
	create_info.depthBiasSlopeFactor = desc.slope_scaled_depth_bias;

	if (const auto conservative_rasterization_info = const_cast<VkPipelineRasterizationConservativeStateCreateInfoEXT *>(
			find_in_structure_chain<VkPipelineRasterizationConservativeStateCreateInfoEXT>(create_info.pNext, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT)))
	{
		conservative_rasterization_info->conservativeRasterizationMode = static_cast<VkConservativeRasterizationModeEXT>(desc.conservative_rasterization);
	}
}
reshade::api::rasterizer_desc reshade::vulkan::convert_rasterizer_desc(const VkPipelineRasterizationStateCreateInfo *create_info, const VkPipelineMultisampleStateCreateInfo *multisample_create_info)
{
	api::rasterizer_desc desc = {};

	if (create_info != nullptr)
	{
		desc.fill_mode = convert_fill_mode(create_info->polygonMode);
		desc.cull_mode = convert_cull_mode(create_info->cullMode);
		desc.front_counter_clockwise = create_info->frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;
		desc.depth_bias = create_info->depthBiasConstantFactor;
		desc.depth_bias_clamp = create_info->depthBiasClamp;
		desc.slope_scaled_depth_bias = create_info->depthBiasSlopeFactor;
		desc.depth_clip_enable = !create_info->depthClampEnable;
		desc.scissor_enable = true;

		if (const auto conservative_rasterization_info = find_in_structure_chain<VkPipelineRasterizationConservativeStateCreateInfoEXT>(
				create_info->pNext, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT))
		{
			desc.conservative_rasterization = static_cast<uint32_t>(conservative_rasterization_info->conservativeRasterizationMode);
		}
	}

	if (multisample_create_info != nullptr)
	{
		desc.multisample_enable = multisample_create_info->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT;
	}

	return desc;
}
void reshade::vulkan::convert_depth_stencil_desc(const api::depth_stencil_desc &desc, VkPipelineDepthStencilStateCreateInfo &create_info)
{
	create_info.depthTestEnable = desc.depth_enable;
	create_info.depthWriteEnable = desc.depth_write_mask;
	create_info.depthCompareOp = convert_compare_op(desc.depth_func);
	create_info.stencilTestEnable = desc.stencil_enable;
	create_info.front.failOp = convert_stencil_op(desc.front_stencil_fail_op);
	create_info.front.passOp = convert_stencil_op(desc.front_stencil_pass_op);
	create_info.front.depthFailOp = convert_stencil_op(desc.front_stencil_depth_fail_op);
	create_info.front.compareOp = convert_compare_op(desc.front_stencil_func);
	create_info.front.compareMask = desc.front_stencil_read_mask;
	create_info.front.writeMask = desc.front_stencil_write_mask;
	create_info.front.reference = desc.front_stencil_reference_value;
	create_info.back.failOp = convert_stencil_op(desc.back_stencil_fail_op);
	create_info.back.passOp = convert_stencil_op(desc.back_stencil_pass_op);
	create_info.back.depthFailOp = convert_stencil_op(desc.back_stencil_depth_fail_op);
	create_info.back.compareOp = convert_compare_op(desc.back_stencil_func);
	create_info.back.compareMask = desc.back_stencil_read_mask;
	create_info.back.writeMask = desc.back_stencil_write_mask;
	create_info.back.reference = desc.back_stencil_reference_value;
}
reshade::api::depth_stencil_desc reshade::vulkan::convert_depth_stencil_desc(const VkPipelineDepthStencilStateCreateInfo *create_info)
{
	api::depth_stencil_desc desc = {};

	if (create_info != nullptr)
	{
		desc.depth_enable = create_info->depthTestEnable;
		desc.depth_write_mask = create_info->depthWriteEnable;
		desc.depth_func = convert_compare_op(create_info->depthCompareOp);
		desc.stencil_enable = create_info->stencilTestEnable;
		desc.front_stencil_read_mask = create_info->front.compareMask & 0xFF;
		desc.front_stencil_write_mask = create_info->front.writeMask & 0xFF;
		desc.front_stencil_reference_value = create_info->front.reference & 0xFF;
		desc.front_stencil_func = convert_compare_op(create_info->front.compareOp);
		desc.front_stencil_fail_op = convert_stencil_op(create_info->front.failOp);
		desc.front_stencil_pass_op = convert_stencil_op(create_info->front.passOp);
		desc.front_stencil_depth_fail_op = convert_stencil_op(create_info->front.depthFailOp);
		desc.back_stencil_read_mask = create_info->back.compareMask & 0xFF;
		desc.back_stencil_write_mask = create_info->back.writeMask & 0xFF;
		desc.back_stencil_reference_value = create_info->back.reference & 0xFF;
		desc.back_stencil_func = convert_compare_op(create_info->back.compareOp);
		desc.back_stencil_fail_op = convert_stencil_op(create_info->back.failOp);
		desc.back_stencil_pass_op = convert_stencil_op(create_info->back.passOp);
		desc.back_stencil_depth_fail_op = convert_stencil_op(create_info->back.depthFailOp);
	}
	else
	{
		desc.depth_enable = false;
		desc.depth_write_mask = false;
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
		// Also need to adjust 'patchControlPoints' externally
		return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	default:
	case api::primitive_topology::undefined:
		assert(false);
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}
}
auto reshade::vulkan::convert_primitive_topology(VkPrimitiveTopology value) -> api::primitive_topology
{
	switch (value)
	{
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
		// This needs to be adjusted externally based on 'patchControlPoints'
		return api::primitive_topology::patch_list_01_cp;
	default:
	case VK_PRIMITIVE_TOPOLOGY_MAX_ENUM:
		assert(false);
		return api::primitive_topology::undefined;
	}
}

auto reshade::vulkan::convert_query_type(api::query_type type) -> VkQueryType
{
	switch (type)
	{
	case api::query_type::occlusion:
	case api::query_type::binary_occlusion:
		return VK_QUERY_TYPE_OCCLUSION;
	case api::query_type::timestamp:
		return VK_QUERY_TYPE_TIMESTAMP;
	case api::query_type::pipeline_statistics:
		return VK_QUERY_TYPE_PIPELINE_STATISTICS;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		return VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT;
	case api::query_type::acceleration_structure_size:
		return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR;
	case api::query_type::acceleration_structure_compacted_size:
		return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
	case api::query_type::acceleration_structure_serialization_size:
		return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR;
	case api::query_type::acceleration_structure_bottom_level_acceleration_structure_pointers:
		return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR;
	default:
		assert(false);
		return VK_QUERY_TYPE_MAX_ENUM;
	}
}
auto reshade::vulkan::convert_query_type(VkQueryType type, uint32_t index) -> api::query_type
{
	switch (type)
	{
	case VK_QUERY_TYPE_OCCLUSION:
		assert(index == 0);
		return api::query_type::occlusion;
	case VK_QUERY_TYPE_TIMESTAMP:
		assert(index == 0);
		return api::query_type::timestamp;
	case VK_QUERY_TYPE_PIPELINE_STATISTICS:
		assert(index == 0);
		return api::query_type::pipeline_statistics;
	case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT:
		assert(index <= 3);
		return static_cast<api::query_type>(static_cast<uint32_t>(api::query_type::stream_output_statistics_0) + index);
	case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR:
		assert(index == 0);
		return api::query_type::acceleration_structure_compacted_size;
	case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR:
		assert(index == 0);
		return api::query_type::acceleration_structure_serialization_size;
	case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR:
		assert(index == 0);
		return api::query_type::acceleration_structure_bottom_level_acceleration_structure_pointers;
	case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR:
		assert(index == 0);
		return api::query_type::acceleration_structure_size;
	default:
		assert(false);
		return static_cast<api::query_type>(UINT32_MAX);
	}
}

auto reshade::vulkan::convert_descriptor_type(api::descriptor_type value) -> VkDescriptorType
{
	switch (value)
	{
	case api::descriptor_type::sampler:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	case api::descriptor_type::sampler_with_resource_view:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case api::descriptor_type::texture_shader_resource_view:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case api::descriptor_type::texture_unordered_access_view:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case api::descriptor_type::buffer_shader_resource_view:
		return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	case api::descriptor_type::buffer_unordered_access_view:
		return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	case api::descriptor_type::constant_buffer:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case api::descriptor_type::shader_storage_buffer:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case api::descriptor_type::acceleration_structure:
		return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	default:
		assert(false);
		return static_cast<VkDescriptorType>(value);
	}
}
auto reshade::vulkan::convert_descriptor_type(VkDescriptorType value) -> api::descriptor_type
{
	switch (value)
	{
	case VK_DESCRIPTOR_TYPE_SAMPLER:
		return api::descriptor_type::sampler;
	case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		return api::descriptor_type::sampler_with_resource_view;
	case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
	case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: // This cannot be restored by 'convert_descriptor_type' in the other direction
		return api::descriptor_type::texture_shader_resource_view;
	case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return api::descriptor_type::texture_unordered_access_view;
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		return api::descriptor_type::buffer_shader_resource_view;
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return api::descriptor_type::buffer_unordered_access_view;
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		return api::descriptor_type::constant_buffer;
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		return api::descriptor_type::shader_storage_buffer;
	case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		return api::descriptor_type::acceleration_structure;
	default:
		assert(false);
		return static_cast<api::descriptor_type>(value);
	}
}

auto reshade::vulkan::convert_render_pass_load_op(api::render_pass_load_op value) -> VkAttachmentLoadOp
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::render_pass_load_op::load:
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	case api::render_pass_load_op::clear:
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case api::render_pass_load_op::discard:
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	case api::render_pass_load_op::no_access:
		return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
	}
}
auto reshade::vulkan::convert_render_pass_load_op(VkAttachmentLoadOp value) -> api::render_pass_load_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case VK_ATTACHMENT_LOAD_OP_LOAD:
		return api::render_pass_load_op::load;
	case VK_ATTACHMENT_LOAD_OP_CLEAR:
		return api::render_pass_load_op::clear;
	case VK_ATTACHMENT_LOAD_OP_DONT_CARE:
		return api::render_pass_load_op::discard;
	case VK_ATTACHMENT_LOAD_OP_NONE_EXT:
		return api::render_pass_load_op::no_access;
	}
}
auto reshade::vulkan::convert_render_pass_store_op(api::render_pass_store_op value) -> VkAttachmentStoreOp
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::render_pass_store_op::store:
		return VK_ATTACHMENT_STORE_OP_STORE;
	case api::render_pass_store_op::discard:
		return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	case api::render_pass_store_op::no_access:
		return VK_ATTACHMENT_STORE_OP_NONE_EXT;
	}
}
auto reshade::vulkan::convert_render_pass_store_op(VkAttachmentStoreOp value) -> api::render_pass_store_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case VK_ATTACHMENT_STORE_OP_STORE:
		return api::render_pass_store_op::store;
	case VK_ATTACHMENT_STORE_OP_DONT_CARE:
		return api::render_pass_store_op::discard;
	case VK_ATTACHMENT_STORE_OP_NONE_EXT:
		return api::render_pass_store_op::no_access;
	}
}

auto reshade::vulkan::convert_pipeline_flags(api::pipeline_flags value) -> VkPipelineCreateFlags
{
	VkPipelineCreateFlags result = 0;
	if ((value & api::pipeline_flags::library) != 0)
		result |= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
	if ((value & api::pipeline_flags::skip_triangles) != 0)
		result |= VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR;
	if ((value & api::pipeline_flags::skip_aabbs) != 0)
		result |= VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR;

	return result;
}
auto reshade::vulkan::convert_pipeline_flags(VkPipelineCreateFlags2 value) -> api::pipeline_flags
{
	api::pipeline_flags result = api::pipeline_flags::none;
	if ((value & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) != 0)
		result |= api::pipeline_flags::library;
	if ((value & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0)
		result |= api::pipeline_flags::skip_triangles;
	if ((value & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0)
		result |= api::pipeline_flags::skip_aabbs;

	return result;
}
auto reshade::vulkan::convert_shader_group_type(api::shader_group_type value) -> VkRayTracingShaderGroupTypeKHR
{
	switch (value)
	{
	case api::shader_group_type::raygen:
	case api::shader_group_type::miss:
	case api::shader_group_type::callable:
		return VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	case api::shader_group_type::hit_group_triangles:
		return VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	case api::shader_group_type::hit_group_aabbs:
		return VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	default:
		assert(false);
		return VK_RAY_TRACING_SHADER_GROUP_TYPE_MAX_ENUM_KHR;
	}
}
auto reshade::vulkan::convert_shader_group_type(VkRayTracingShaderGroupTypeKHR value) -> api::shader_group_type
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
		return api::shader_group_type::raygen;
	case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
		return api::shader_group_type::hit_group_triangles;
	case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
		return api::shader_group_type::hit_group_aabbs;
	}
}
auto reshade::vulkan::convert_acceleration_structure_type(api::acceleration_structure_type value) -> VkAccelerationStructureTypeKHR
{
	return static_cast<VkAccelerationStructureTypeKHR>(value);
}
auto reshade::vulkan::convert_acceleration_structure_type(VkAccelerationStructureTypeKHR value) -> api::acceleration_structure_type
{
	return static_cast<api::acceleration_structure_type>(value);
}
auto reshade::vulkan::convert_acceleration_structure_copy_mode(api::acceleration_structure_copy_mode value) -> VkCopyAccelerationStructureModeKHR
{
	return static_cast<VkCopyAccelerationStructureModeKHR>(value);
}
auto reshade::vulkan::convert_acceleration_structure_copy_mode(VkCopyAccelerationStructureModeKHR value) -> api::acceleration_structure_copy_mode
{
	return static_cast<api::acceleration_structure_copy_mode>(value);
}
auto reshade::vulkan::convert_acceleration_structure_build_flags(api::acceleration_structure_build_flags value) -> VkBuildAccelerationStructureFlagsKHR
{
	VkBuildAccelerationStructureFlagsKHR result = 0;
	if ((value & api::acceleration_structure_build_flags::allow_update) != 0)
		result |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	if ((value & api::acceleration_structure_build_flags::allow_compaction) != 0)
		result |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
	if ((value & api::acceleration_structure_build_flags::prefer_fast_trace) != 0)
		result |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	if ((value & api::acceleration_structure_build_flags::prefer_fast_build) != 0)
		result |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
	if ((value & api::acceleration_structure_build_flags::minimize_memory_usage) != 0)
		result |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;

	return result;
}
auto reshade::vulkan::convert_acceleration_structure_build_flags(VkBuildAccelerationStructureFlagsKHR value) -> api::acceleration_structure_build_flags
{
	api::acceleration_structure_build_flags result = api::acceleration_structure_build_flags::none;
	if ((value & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR) != 0)
		result |= api::acceleration_structure_build_flags::allow_update;
	if ((value & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != 0)
		result |= api::acceleration_structure_build_flags::allow_compaction;
	if ((value & VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) != 0)
		result |= api::acceleration_structure_build_flags::prefer_fast_trace;
	if ((value & VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR) != 0)
		result |= api::acceleration_structure_build_flags::prefer_fast_build;
	if ((value & VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR) != 0)
		result |= api::acceleration_structure_build_flags::minimize_memory_usage;

	return result;
}

void reshade::vulkan::convert_acceleration_structure_build_input(const api::acceleration_structure_build_input &build_input, VkAccelerationStructureGeometryKHR &geometry, VkAccelerationStructureBuildRangeInfoKHR &range_info)
{
	geometry.geometryType = static_cast<VkGeometryTypeKHR>(build_input.type);

	switch (geometry.geometryType)
	{
	case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
		geometry.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		geometry.geometry.triangles.vertexFormat = convert_format(build_input.triangles.vertex_format);
		geometry.geometry.triangles.vertexData.deviceAddress = build_input.triangles.vertex_offset;
		geometry.geometry.triangles.vertexStride = build_input.triangles.vertex_stride;
		geometry.geometry.triangles.maxVertex = build_input.triangles.vertex_count;
		geometry.geometry.triangles.indexType = build_input.triangles.index_format == api::format::r8_uint ? VK_INDEX_TYPE_UINT8_EXT : build_input.triangles.index_format == api::format::r16_uint ? VK_INDEX_TYPE_UINT16 : build_input.triangles.index_format == api::format::r32_uint ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR;
		geometry.geometry.triangles.indexData.deviceAddress = build_input.triangles.index_offset;
		geometry.geometry.triangles.transformData.deviceAddress = build_input.triangles.transform_offset;
		range_info.primitiveCount = geometry.geometry.triangles.indexData.deviceAddress != 0 ? build_input.triangles.index_count / 3 : build_input.triangles.vertex_count / 3;
		break;
	case VK_GEOMETRY_TYPE_AABBS_KHR:
		geometry.geometry.aabbs = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR };
		geometry.geometry.aabbs.data.deviceAddress = build_input.aabbs.offset;
		geometry.geometry.aabbs.stride = build_input.aabbs.stride;
		range_info.primitiveCount = build_input.aabbs.count;
		break;
	case VK_GEOMETRY_TYPE_INSTANCES_KHR:
		geometry.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		geometry.geometry.instances.data.deviceAddress = build_input.instances.offset;
		geometry.geometry.instances.arrayOfPointers = build_input.instances.array_of_pointers ? VK_TRUE : VK_FALSE;
		range_info.primitiveCount = build_input.instances.count;
		break;
	}

	geometry.flags = static_cast<VkGeometryFlagsKHR>(build_input.flags);

	range_info.primitiveOffset = 0;
	range_info.firstVertex = 0;
	range_info.transformOffset = 0;
}
reshade::api::acceleration_structure_build_input reshade::vulkan::convert_acceleration_structure_build_input(const VkAccelerationStructureGeometryKHR &geometry, const VkAccelerationStructureBuildRangeInfoKHR &range_info)
{
	api::acceleration_structure_build_input build_input = {};
	build_input.type = static_cast<api::acceleration_structure_build_input_type>(geometry.geometryType);

	switch (geometry.geometryType)
	{
	case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
		build_input.triangles.vertex_offset = geometry.geometry.triangles.vertexData.deviceAddress + range_info.primitiveOffset + range_info.firstVertex * geometry.geometry.triangles.vertexStride;
		build_input.triangles.vertex_count = geometry.geometry.triangles.indexData.deviceAddress == 0 ? range_info.primitiveCount * 3 : geometry.geometry.triangles.maxVertex;
		build_input.triangles.vertex_stride = geometry.geometry.triangles.vertexStride;
		build_input.triangles.vertex_format = convert_format(geometry.geometry.triangles.vertexFormat);
		build_input.triangles.index_offset = geometry.geometry.triangles.indexData.deviceAddress;
		build_input.triangles.index_count = geometry.geometry.triangles.indexData.deviceAddress != 0 ? range_info.primitiveCount * 3 : 0;
		build_input.triangles.index_format = geometry.geometry.triangles.indexType == VK_INDEX_TYPE_UINT8_EXT ? api::format::r8_uint : geometry.geometry.triangles.indexType == VK_INDEX_TYPE_UINT16 ? api::format::r16_uint : geometry.geometry.triangles.indexType == VK_INDEX_TYPE_UINT32 ? api::format::r32_uint : api::format::unknown;
		build_input.triangles.transform_offset = geometry.geometry.triangles.transformData.deviceAddress + range_info.transformOffset;
		break;
	case VK_GEOMETRY_TYPE_AABBS_KHR:
		build_input.aabbs.offset = geometry.geometry.aabbs.data.deviceAddress;
		build_input.aabbs.count = range_info.primitiveCount;
		build_input.aabbs.stride = geometry.geometry.aabbs.stride;
		break;
	case VK_GEOMETRY_TYPE_INSTANCES_KHR:
		build_input.instances.offset = geometry.geometry.instances.data.deviceAddress;
		build_input.instances.count = range_info.primitiveCount;
		build_input.instances.array_of_pointers = geometry.geometry.instances.arrayOfPointers != VK_FALSE;
		break;
	}

	build_input.flags = static_cast<api::acceleration_structure_build_input_flags>(geometry.flags);

	return build_input;
}

auto reshade::vulkan::convert_shader_stages(VkPipelineBindPoint value) -> api::shader_stage
{
	switch (value)
	{
	case VK_PIPELINE_BIND_POINT_GRAPHICS:
		return api::shader_stage::all_graphics;
	case VK_PIPELINE_BIND_POINT_COMPUTE:
		return api::shader_stage::all_compute;
	case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
		return api::shader_stage::all_ray_tracing;
	default:
		// Unknown pipeline bind point
		assert(false);
		return static_cast<api::shader_stage>(0);
	}
}
auto reshade::vulkan::convert_pipeline_stages(api::pipeline_stage value) -> VkPipelineBindPoint
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::pipeline_stage::all_graphics:
		return VK_PIPELINE_BIND_POINT_GRAPHICS;
	case api::pipeline_stage::all_compute:
		return VK_PIPELINE_BIND_POINT_COMPUTE;
	case api::pipeline_stage::all_ray_tracing:
		return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
	}
}
auto reshade::vulkan::convert_pipeline_stages(VkPipelineBindPoint value) -> api::pipeline_stage
{
	switch (value)
	{
	case VK_PIPELINE_BIND_POINT_GRAPHICS:
		return api::pipeline_stage::all_graphics;
	case VK_PIPELINE_BIND_POINT_COMPUTE:
		return api::pipeline_stage::all_compute;
	case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
		return api::pipeline_stage::all_ray_tracing;
	default:
		// Unknown pipeline bind point
		assert(false);
		return static_cast<api::pipeline_stage>(0);
	}
}
