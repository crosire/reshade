/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_format.hpp"

namespace reshade { namespace api
{
	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent typeless variant.
	/// </summary>
	inline format format_to_typeless(format value)
	{
		switch (value)
		{
		case format::r8_typeless:
		case format::r8_uint:
		case format::r8_sint:
		case format::r8_unorm:
		case format::r8_snorm:
			return format::r8_typeless;
		case format::r8g8_typeless:
		case format::r8g8_uint:
		case format::r8g8_sint:
		case format::r8g8_unorm:
		case format::r8g8_snorm:
			return format::r8g8_typeless;
		case format::r8g8b8a8_typeless:
		case format::r8g8b8a8_uint:
		case format::r8g8b8a8_sint:
		case format::r8g8b8a8_unorm:
		case format::r8g8b8a8_unorm_srgb:
		case format::r8g8b8a8_snorm:
			return format::r8g8b8a8_typeless;
		case format::b8g8r8a8_typeless:
		case format::b8g8r8a8_unorm:
		case format::b8g8r8a8_unorm_srgb:
			return format::b8g8r8a8_typeless;
		case format::b8g8r8x8_typeless:
		case format::b8g8r8x8_unorm:
		case format::b8g8r8x8_unorm_srgb:
			return format::b8g8r8x8_typeless;
		case format::r10g10b10a2_typeless:
		case format::r10g10b10a2_uint:
		case format::r10g10b10a2_unorm:
		case format::r10g10b10a2_xr_bias:
			return format::r10g10b10a2_typeless;
		case format::d16_unorm:
		case format::r16_typeless:
		case format::r16_uint:
		case format::r16_sint:
		case format::r16_float:
		case format::r16_unorm:
		case format::r16_snorm:
			return format::r16_typeless;
		case format::r16g16_typeless:
		case format::r16g16_uint:
		case format::r16g16_sint:
		case format::r16g16_float:
		case format::r16g16_unorm:
		case format::r16g16_snorm:
			return format::r16g16_typeless;
		case format::r16g16b16a16_typeless:
		case format::r16g16b16a16_uint:
		case format::r16g16b16a16_sint:
		case format::r16g16b16a16_float:
		case format::r16g16b16a16_unorm:
		case format::r16g16b16a16_snorm:
			return format::r16g16b16a16_typeless;
		case format::d32_float:
		case format::r32_typeless:
		case format::r32_uint:
		case format::r32_sint:
		case format::r32_float:
			return format::r32_typeless;
		case format::r32g32_typeless:
		case format::r32g32_uint:
		case format::r32g32_sint:
		case format::r32g32_float:
			return format::r32g32_typeless;
		case format::r32g32b32_typeless:
		case format::r32g32b32_uint:
		case format::r32g32b32_sint:
		case format::r32g32b32_float:
			return format::r32g32b32_typeless;
		case format::r32g32b32a32_typeless:
		case format::r32g32b32a32_uint:
		case format::r32g32b32a32_sint:
		case format::r32g32b32a32_float:
			return format::r32g32b32a32_typeless;
		case format::d32_float_s8_uint:
		case format::r32_g8_typeless:
		case format::r32_float_x8_uint:
		case format::x32_float_g8_uint:
			return format::r32_g8_typeless;
		case format::d24_unorm_s8_uint:
		case format::r24_g8_typeless:
		case format::r24_unorm_x8_uint:
		case format::x24_unorm_g8_uint:
			return format::r24_g8_typeless;
		case format::bc1_typeless:
		case format::bc1_unorm:
		case format::bc1_unorm_srgb:
			return format::bc1_typeless;
		case format::bc2_typeless:
		case format::bc2_unorm:
		case format::bc2_unorm_srgb:
			return format::bc2_typeless;
		case format::bc3_typeless:
		case format::bc3_unorm:
		case format::bc3_unorm_srgb:
			return format::bc2_typeless;
		case format::bc4_typeless:
		case format::bc4_unorm:
		case format::bc4_snorm:
			return format::bc4_typeless;
		case format::bc5_typeless:
		case format::bc5_unorm:
		case format::bc5_snorm:
			return format::bc5_typeless;
		case format::bc6h_typeless:
		case format::bc6h_ufloat:
		case format::bc6h_sfloat:
			return format::bc6h_typeless;
		case format::bc7_typeless:
		case format::bc7_unorm:
		case format::bc7_unorm_srgb:
			return format::bc7_typeless;
		default:
			return value;
		}
	}

	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent typed variant.
	/// </summary>
	inline format format_to_default_typed(format value)
	{
		switch (value)
		{
		case format::r8_typeless:
			return format::r8_unorm;
		case format::r8g8_typeless:
			return format::r8g8_unorm;
		case format::r8g8b8a8_typeless:
		case format::r8g8b8a8_unorm_srgb:
			return format::r8g8b8a8_unorm;
		case format::b8g8r8a8_typeless:
		case format::b8g8r8a8_unorm_srgb:
			return format::b8g8r8a8_unorm;
		case format::b8g8r8x8_typeless:
		case format::b8g8r8x8_unorm_srgb:
			return format::b8g8r8x8_unorm;
		case format::r10g10b10a2_typeless:
			return format::r10g10b10a2_unorm;
		case format::d16_unorm:
			return format::r16_unorm;
		case format::r16g16_typeless:
			return format::r16g16_unorm;
		case format::r16g16b16a16_typeless:
			return format::r16g16b16a16_unorm;
		case format::d32_float:
		case format::r32_typeless:
			return format::r32_float;
		case format::r32g32_typeless:
			return format::r32g32_float;
		case format::r32g32b32_typeless:
			return format::r32g32b32_float;
		case format::r32g32b32a32_typeless:
			return format::r32g32b32a32_float;
		case format::d32_float_s8_uint:
		case format::r32_g8_typeless:
			return format::r32_float_x8_uint;
		case format::d24_unorm_s8_uint:
		case format::r24_g8_typeless:
			return format::r24_unorm_x8_uint;
		case format::bc1_typeless:
		case format::bc1_unorm_srgb:
			return format::bc1_unorm;
		case format::bc2_typeless:
		case format::bc2_unorm_srgb:
			return format::bc2_unorm;
		case format::bc3_typeless:
		case format::bc3_unorm_srgb:
			return format::bc2_unorm;
		case format::bc4_typeless:
			return format::bc4_unorm;
		case format::bc5_typeless:
			return format::bc5_unorm;
		case format::bc6h_typeless:
			return format::bc6h_ufloat;
		case format::bc7_typeless:
		case format::bc7_unorm_srgb:
			return format::bc7_unorm;
		default:
			return value;
		}
	}

	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent typed sRGB variant.
	/// </summary>
	inline format format_to_default_typed_srgb(format value)
	{
		switch (value)
		{
		case format::r8_typeless:
			return format::r8_unorm;
		case format::r8g8_typeless:
			return format::r8g8_unorm;
		case format::r8g8b8a8_typeless:
		case format::r8g8b8a8_unorm:
			return format::r8g8b8a8_unorm_srgb;
		case format::b8g8r8a8_typeless:
		case format::b8g8r8a8_unorm:
			return format::b8g8r8a8_unorm_srgb;
		case format::b8g8r8x8_typeless:
		case format::b8g8r8x8_unorm:
			return format::b8g8r8x8_unorm_srgb;
		case format::r10g10b10a2_typeless:
			return format::r10g10b10a2_unorm;
		case format::d16_unorm:
		case format::r16_typeless:
			return format::r16_unorm;
		case format::r16g16_typeless:
			return format::r16g16_unorm;
		case format::r16g16b16a16_typeless:
			return format::r16g16b16a16_unorm;
		case format::d32_float:
		case format::r32_typeless:
			return format::r32_float;
		case format::r32g32_typeless:
			return format::r32g32_float;
		case format::r32g32b32_typeless:
			return format::r32g32b32_float;
		case format::r32g32b32a32_typeless:
			return format::r32g32b32a32_float;
		case format::d32_float_s8_uint:
		case format::r32_g8_typeless:
			return format::r32_float_x8_uint;
		case format::d24_unorm_s8_uint:
		case format::r24_g8_typeless:
			return format::r24_unorm_x8_uint;
		case format::bc1_typeless:
		case format::bc1_unorm:
			return format::bc1_unorm_srgb;
		case format::bc2_typeless:
		case format::bc2_unorm:
			return format::bc2_unorm_srgb;
		case format::bc3_typeless:
		case format::bc3_unorm:
			return format::bc2_unorm_srgb;
		case format::bc4_typeless:
			return format::bc4_unorm;
		case format::bc5_typeless:
			return format::bc5_unorm;
		case format::bc6h_typeless:
			return format::bc6h_ufloat;
		case format::bc7_typeless:
		case format::bc7_unorm:
			return format::bc7_unorm_srgb;
		default:
			return value;
		}
	}

	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent depth-stencil variant.
	/// </summary>
	inline format format_to_depth_stencil_typed(format value)
	{
		switch (value)
		{
		case format::r16_typeless:
		case format::r16_unorm:
			return format::d16_unorm;
		case format::r32_typeless:
		case format::r32_float:
			return format::d32_float;
		case format::r32_g8_typeless:
		case format::r32_float_x8_uint:
		case format::x32_float_g8_uint:
			return format::d32_float_s8_uint;
		case format::r24_g8_typeless:
		case format::r24_unorm_x8_uint:
		case format::x24_unorm_g8_uint:
			return format::d24_unorm_s8_uint;
		default:
			return value;
		}
	}

	/// <summary>
	/// Gets the number of bytes per pixel the specified format <paramref name="value"/> occupies.
	/// </summary>
	inline unsigned int format_bpp(format value)
	{
		if (value == format::unknown)
			return 0;
		if (value <= format::r32g32b32a32_sint)
			return 32;
		if (value <= format::r32g32b32_sint)
			return 24;
		if (value <= format::x32_float_g8_uint)
			return 16;
		if (value <= format::x24_unorm_g8_uint || value == format::b5g6r5_unorm || value == format::b5g5r5a1_unorm)
			return 4;
		if (value <= format::r16_sint)
			return 2;
		if (value <= format::a8_unorm)
			return 1;
		if (value <= format::g8r8_g8b8_unorm || (value >= format::b8g8r8a8_unorm && value <= format::b8g8r8x8_unorm_srgb))
			return 8;
		// TODO: Block compressed formats
		return 0;
	}
} }
