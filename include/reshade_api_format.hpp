/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cstdint>

namespace reshade { namespace api
{
	/// <summary>
	/// Available data and texture formats. This is mostly compatible with DXGI_FORMAT.
	/// </summary>
	enum class format : uint32_t
	{
		unknown = 0,

		// Color formats

		r1_unorm = 66,
		a8_unorm = 65,
		r8_typeless = 60,
		r8_uint = 62,
		r8_sint = 64,
		r8_unorm = 61,
		r8_snorm = 63,
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
		r16_typeless = 53,
		r16_uint = 57,
		r16_sint = 59,
		r16_float = 54,
		r16_unorm = 56,
		r16_snorm = 58,
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
		b4g4r4a4_unorm = 115,

		// Depth-stencil formats

		s8_uint = 0x30303853,
		d16_unorm = 55,
		d16_unorm_s8_uint = 0x38363144,
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
} }
