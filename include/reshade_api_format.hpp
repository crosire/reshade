/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <cfloat>
#include <cstdint>

namespace reshade::api
{
	/// <summary>
	/// The available data and texture formats.
	/// This is mostly compatible with 'DXGI_FORMAT'.
	/// </summary>
	enum class format : uint32_t
	{
		unknown = 0,

		// Color formats

		r1_unorm = 66,
		l8_unorm = 0x3030384C,
		a8_unorm = 65,
		r8_typeless = 60,
		r8_uint = 62,
		r8_sint = 64,
		r8_unorm = 61,
		r8_snorm = 63,
		l8a8_unorm = 0x3038414C,
		r8g8_typeless = 48,
		r8g8_uint = 50,
		r8g8_sint = 52,
		r8g8_unorm = 49,
		r8g8_snorm = 51,
		r8g8b8a8_typeless = 27,
		r8g8b8a8_uint = 30,
		r8g8b8a8_sint = 32,
		r8g8b8a8_unorm = 28,
		r8g8b8a8_unorm_srgb = 29,
		r8g8b8a8_snorm = 31,
		r8g8b8x8_typeless = 0x424757B8,
		r8g8b8x8_unorm = 0x424757B9,
		r8g8b8x8_unorm_srgb = 0x424757BA,
		b8g8r8a8_typeless = 90,
		b8g8r8a8_unorm = 87,
		b8g8r8a8_unorm_srgb = 91,
		b8g8r8x8_typeless = 92,
		b8g8r8x8_unorm = 88,
		b8g8r8x8_unorm_srgb = 93,
		r10g10b10a2_typeless = 23,
		r10g10b10a2_uint = 25,
		r10g10b10a2_unorm = 24,
		r10g10b10a2_xr_bias = 89,
		b10g10r10a2_typeless = 0x42475330,
		b10g10r10a2_uint = 0x42475332,
		b10g10r10a2_unorm = 0x42475331,
		l16_unorm = 0x3036314C,
		r16_typeless = 53,
		r16_uint = 57,
		r16_sint = 59,
		r16_float = 54,
		r16_unorm = 56,
		r16_snorm = 58,
		l16a16_unorm = 0x3631414C,
		r16g16_typeless = 33,
		r16g16_uint = 36,
		r16g16_sint = 38,
		r16g16_float = 34,
		r16g16_unorm = 35,
		r16g16_snorm = 37,
		r16g16b16a16_typeless = 9,
		r16g16b16a16_uint = 12,
		r16g16b16a16_sint = 14,
		r16g16b16a16_float = 10,
		r16g16b16a16_unorm = 11,
		r16g16b16a16_snorm = 13,
		r32_typeless = 39,
		r32_uint = 42,
		r32_sint = 43,
		r32_float = 41,
		r32g32_typeless = 15,
		r32g32_uint = 17,
		r32g32_sint = 18,
		r32g32_float = 16,
		r32g32b32_typeless = 5,
		r32g32b32_uint = 7,
		r32g32b32_sint = 8,
		r32g32b32_float = 6,
		r32g32b32a32_typeless = 1,
		r32g32b32a32_uint = 3,
		r32g32b32a32_sint = 4,
		r32g32b32a32_float = 2,
		r9g9b9e5 = 67,
		r11g11b10_float = 26,
		b5g6r5_unorm = 85,
		b5g5r5a1_unorm = 86,
		b5g5r5x1_unorm = 0x424757B5,
		b4g4r4a4_unorm = 115,

		// Depth-stencil formats

		s8_uint = 0x30303853,
		d16_unorm = 55,
		d16_unorm_s8_uint = 0x38363144,
		d24_unorm_x8_uint = 0x38343244,
		d24_unorm_s8_uint = 45,
		d32_float = 40,
		d32_float_s8_uint = 20,

		r32_g8_typeless = 19,
		r32_float_x8_uint = 21,
		x32_float_g8_uint = 22,
		r24_g8_typeless = 44,
		r24_unorm_x8_uint = 46,
		x24_unorm_g8_uint = 47,

		// Compressed data formats

		bc1_typeless = 70,
		bc1_unorm = 71,
		bc1_unorm_srgb = 72,
		bc2_typeless = 73,
		bc2_unorm = 74,
		bc2_unorm_srgb = 75,
		bc3_typeless = 76,
		bc3_unorm = 77,
		bc3_unorm_srgb = 78,
		bc4_typeless = 79,
		bc4_unorm = 80,
		bc4_snorm = 81,
		bc5_typeless = 82,
		bc5_unorm = 83,
		bc5_snorm = 84,
		bc6h_typeless = 94,
		bc6h_ufloat = 95,
		bc6h_sfloat = 96,
		bc7_typeless = 97,
		bc7_unorm = 98,
		bc7_unorm_srgb = 99,

		// Video formats

		r8g8_b8g8_unorm = 68,
		g8r8_g8b8_unorm = 69,

		// Special purpose formats

		intz = 0x5A544E49,
	};

	/// <summary>
	/// The available color space types for presentation.
	/// </summary>
	enum class color_space : uint32_t
	{
		unknown = 0,

		srgb_nonlinear,
		extended_srgb_linear,
		hdr10_st2084,
		hdr10_hlg,
	};

	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent typeless variant.
	/// </summary>
	/// <param name="value">The format to convert.</param>
	inline format format_to_typeless(format value)
	{
		switch (value)
		{
		case format::l8_unorm:
		case format::r8_typeless:
		case format::r8_uint:
		case format::r8_sint:
		case format::r8_unorm:
		case format::r8_snorm:
			return format::r8_typeless;
		case format::l8a8_unorm:
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
		case format::b10g10r10a2_typeless:
		case format::b10g10r10a2_uint:
		case format::b10g10r10a2_unorm:
			return format::b10g10r10a2_typeless;
		case format::l16_unorm:
		case format::d16_unorm:
		case format::r16_typeless:
		case format::r16_uint:
		case format::r16_sint:
		case format::r16_float:
		case format::r16_unorm:
		case format::r16_snorm:
			return format::r16_typeless;
		case format::l16a16_unorm:
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
		case format::d24_unorm_s8_uint: // Do not also convert 'd24_unorm_x8_uint' here, to keep it distinguishable from 'd24_unorm_s8_uint'
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
	/// Converts the specified format <paramref name="value"/> to its equivalent typed variant ("unorm" or "float").
	/// </summary>
	/// <param name="value">The format to convert.</param>
	/// <param name="srgb_variant">Set to 1 to get sRGB variant, 0 to get linear variant and -1 to preserve existing one.</param>
	inline format format_to_default_typed(format value, int srgb_variant = -1)
	{
		switch (value)
		{
		case format::r8_typeless:
			return format::r8_unorm;
		case format::r8g8_typeless:
			return format::r8g8_unorm;
		case format::r8g8b8a8_typeless:
		case format::r8g8b8a8_unorm:
			return srgb_variant == 1 ? format::r8g8b8a8_unorm_srgb : format::r8g8b8a8_unorm;
		case format::r8g8b8a8_unorm_srgb:
			return srgb_variant != 0 ? format::r8g8b8a8_unorm_srgb : format::r8g8b8a8_unorm;
		case format::r8g8b8x8_typeless:
		case format::r8g8b8x8_unorm:
			return srgb_variant == 1 ? format::r8g8b8x8_unorm_srgb : format::r8g8b8x8_unorm;
		case format::r8g8b8x8_unorm_srgb:
			return srgb_variant != 0 ? format::r8g8b8x8_unorm_srgb : format::r8g8b8x8_unorm;
		case format::b8g8r8a8_typeless:
		case format::b8g8r8a8_unorm:
			return srgb_variant == 1 ? format::b8g8r8a8_unorm_srgb : format::b8g8r8a8_unorm;
		case format::b8g8r8a8_unorm_srgb:
			return srgb_variant != 0 ? format::b8g8r8a8_unorm_srgb : format::b8g8r8a8_unorm;
		case format::b8g8r8x8_typeless:
		case format::b8g8r8x8_unorm:
			return srgb_variant == 1 ? format::b8g8r8x8_unorm_srgb : format::b8g8r8x8_unorm;
		case format::b8g8r8x8_unorm_srgb:
			return srgb_variant != 0 ? format::b8g8r8x8_unorm_srgb : format::b8g8r8x8_unorm;
		case format::r10g10b10a2_typeless:
			return format::r10g10b10a2_unorm;
		case format::b10g10r10a2_typeless:
			return format::b10g10r10a2_unorm;
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
		case format::d24_unorm_s8_uint: // Do not also convert 'd24_unorm_x8_uint' here, to keep it distinguishable from 'd24_unorm_s8_uint'
		case format::r24_g8_typeless:
			return format::r24_unorm_x8_uint;
		case format::bc1_typeless:
		case format::bc1_unorm:
			return srgb_variant == 1 ? format::bc1_unorm_srgb : format::bc1_unorm;
		case format::bc1_unorm_srgb:
			return srgb_variant != 0 ? format::bc1_unorm_srgb : format::bc1_unorm;
		case format::bc2_typeless:
		case format::bc2_unorm:
			return srgb_variant == 1 ? format::bc2_unorm_srgb : format::bc2_unorm;
		case format::bc2_unorm_srgb:
			return srgb_variant != 0 ? format::bc2_unorm_srgb : format::bc2_unorm;
		case format::bc3_typeless:
		case format::bc3_unorm:
			return srgb_variant == 1 ? format::bc3_unorm_srgb : format::bc3_unorm;
		case format::bc3_unorm_srgb:
			return srgb_variant != 0 ? format::bc3_unorm_srgb : format::bc3_unorm;
		case format::bc4_typeless:
			return format::bc4_unorm;
		case format::bc5_typeless:
			return format::bc5_unorm;
		case format::bc6h_typeless:
			return format::bc6h_ufloat;
		case format::bc7_typeless:
		case format::bc7_unorm:
			return srgb_variant == 1 ? format::bc7_unorm_srgb : format::bc7_unorm;
		case format::bc7_unorm_srgb:
			return srgb_variant != 0 ? format::bc7_unorm_srgb : format::bc7_unorm;
		default:
			return value;
		}
	}

	/// <summary>
	/// Converts the specified format <paramref name="value"/> to its equivalent depth-stencil variant.
	/// </summary>
	/// <param name="value">The format to convert.</param>
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
	/// Gets the number of bytes a texture row of the specified format <paramref name="value"/> occupies.
	/// </summary>
	inline const uint32_t format_row_pitch(format value, uint32_t width)
	{
		if (value == format::unknown)
			return 0;

		if (value <= format::r32g32b32a32_sint)
			return 16 * width;
		if (value <= format::r32g32b32_sint)
			return 12 * width;
		if (value <= format::x32_float_g8_uint)
			return  8 * width;
		if (value <= format::x24_unorm_g8_uint || value == format::l16a16_unorm)
			return  4 * width;
		if (value <= format::r16_sint || value == format::b5g6r5_unorm || value == format::b5g5r5a1_unorm || value == format::b5g5r5x1_unorm || value == format::l8a8_unorm || value == format::l16_unorm)
			return  2 * width;
		if (value <= format::a8_unorm || value == format::l8_unorm)
			return  1 * width;
		if (value <= format::g8r8_g8b8_unorm || (value >= format::b8g8r8a8_unorm && value <= format::b8g8r8x8_unorm_srgb) || (value >= format::r8g8b8x8_typeless && value <= format::r8g8b8x8_unorm_srgb))
			return  4 * width;

		// Block compressed formats are bytes per block, rather than per pixel
		if ((value >= format::bc1_typeless && value <= format::bc1_unorm_srgb) || (value >= format::bc4_typeless && value <= format::bc4_snorm))
			return  8 * ((width + 3) / 4);
		if ((value >= format::bc2_typeless && value <= format::bc2_unorm_srgb) || (value >= format::bc3_typeless && value <= format::bc3_unorm_srgb) || (value >= format::bc5_typeless && value <= format::bc7_unorm_srgb))
			return 16 * ((width + 3) / 4);

		return 0;
	}
	/// <summary>
	/// Gets the number of bytes a texture slice of the specified format <paramref name="value"/> occupies.
	/// </summary>
	inline const uint32_t format_slice_pitch(format value, uint32_t row_pitch, uint32_t height)
	{
		if ((value >= format::bc1_typeless && value <= format::bc5_snorm) || (value >= format::bc6h_typeless && value <= format::bc7_unorm_srgb))
			return row_pitch * ((height + 3) / 4);

		return row_pitch * height;
	}
}
