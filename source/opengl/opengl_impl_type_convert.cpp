/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_type_convert.hpp"
#include <cassert>

auto reshade::opengl::convert_format(api::format format, GLint swizzle_mask[4]) -> GLenum
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

	case api::format::r8_typeless:
	case api::format::r8_unorm:
		return GL_R8;
	case api::format::r8_uint:
		return GL_R8UI;
	case api::format::r8_snorm:
		return GL_R8_SNORM;
	case api::format::r8_sint:
		return GL_R8I;
	case api::format::l8_unorm:
		if (swizzle_mask != nullptr)
		{
			swizzle_mask[0] = GL_RED;
			swizzle_mask[1] = GL_RED;
			swizzle_mask[2] = GL_RED;
			swizzle_mask[3] = GL_ONE;
			return GL_R8;
		}
		return GL_LUMINANCE8_EXT;
	case api::format::a8_unorm:
		if (swizzle_mask != nullptr)
		{
			swizzle_mask[0] = GL_ZERO;
			swizzle_mask[1] = GL_ZERO;
			swizzle_mask[2] = GL_ZERO;
			swizzle_mask[3] = GL_RED;
			return GL_R8;
		}
		return GL_ALPHA8_EXT;

	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
		return GL_RG8;
	case api::format::r8g8_uint:
		return GL_RG8UI;
	case api::format::r8g8_snorm:
		return GL_RG8_SNORM;
	case api::format::r8g8_sint:
		return GL_RG8I;
	case api::format::l8a8_unorm:
		if (swizzle_mask != nullptr)
		{
			swizzle_mask[0] = GL_RED;
			swizzle_mask[1] = GL_RED;
			swizzle_mask[2] = GL_RED;
			swizzle_mask[3] = GL_GREEN;
			return GL_RG8;
		}
		return GL_LUMINANCE8_ALPHA8_EXT;

	case api::format::r8g8b8_typeless:
	case api::format::r8g8b8_unorm:
	case api::format::b8g8r8_typeless:
	case api::format::b8g8r8_unorm:
		return GL_RGB8;
	case api::format::r8g8b8_unorm_srgb:
	case api::format::b8g8r8_unorm_srgb:
		return GL_SRGB8;
	case api::format::r8g8b8_uint:
		return GL_RGB8UI;
	case api::format::r8g8b8_snorm:
		return GL_RGB8_SNORM;
	case api::format::r8g8b8_sint:
		return GL_RGB8I;

	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
		return GL_RGBA8;
	case api::format::r8g8b8a8_unorm_srgb:
	case api::format::b8g8r8a8_unorm_srgb:
		return GL_SRGB8_ALPHA8;
	case api::format::r8g8b8a8_uint:
		return GL_RGBA8UI;
	case api::format::r8g8b8a8_snorm:
		return GL_RGBA8_SNORM;
	case api::format::r8g8b8a8_sint:
		return GL_RGBA8I;
	case api::format::r8g8b8x8_unorm:
	case api::format::b8g8r8x8_typeless:
	case api::format::b8g8r8x8_unorm:
		return GL_RGB8;
	case api::format::r8g8b8x8_unorm_srgb:
	case api::format::b8g8r8x8_unorm_srgb:
		return GL_SRGB8;

	case api::format::r10g10b10a2_typeless:
	case api::format::r10g10b10a2_unorm:
	case api::format::b10g10r10a2_typeless:
	case api::format::b10g10r10a2_unorm:
		return GL_RGB10_A2;
	case api::format::r10g10b10a2_uint:
	case api::format::b10g10r10a2_uint:
		return GL_RGB10_A2UI;
	case api::format::r10g10b10a2_xr_bias:
		break; // Unsupported

	case api::format::r16_typeless:
	case api::format::r16_float:
		return GL_R16F;
	case api::format::r16_unorm:
		return GL_R16;
	case api::format::r16_uint:
		return GL_R16UI;
	case api::format::r16_snorm:
		return GL_R16_SNORM;
	case api::format::r16_sint:
		return GL_R16I;
#if 0
	case api::format::l16_float:
		return GL_LUMINANCE16F_EXT;
	case api::format::a16_float:
		return GL_ALPHA16F_EXT;
#endif
	case api::format::l16_unorm:
		if (swizzle_mask != nullptr)
		{
			swizzle_mask[0] = GL_RED;
			swizzle_mask[1] = GL_RED;
			swizzle_mask[2] = GL_RED;
			swizzle_mask[3] = GL_ONE;
			return GL_R16;
		}
		return 0x8042 /* GL_LUMINANCE16 */;

	case api::format::r16g16_typeless:
	case api::format::r16g16_float:
		return GL_RG16F;
	case api::format::r16g16_unorm:
		return GL_RG16;
	case api::format::r16g16_uint:
		return GL_RG16UI;
	case api::format::r16g16_snorm:
		return GL_RG16_SNORM;
	case api::format::r16g16_sint:
		return GL_RG16I;
#if 0
	case api::format::l16a16_float:
		return GL_LUMINANCE_ALPHA16F_EXT;
#endif
	case api::format::l16a16_unorm:
		if (swizzle_mask != nullptr)
		{
			swizzle_mask[0] = GL_RED;
			swizzle_mask[1] = GL_RED;
			swizzle_mask[2] = GL_RED;
			swizzle_mask[3] = GL_GREEN;
			return GL_RG16;
		}
		return 0x8048 /* GL_LUMINANCE16_ALPHA16 */;

	case api::format::r16g16b16_typeless:
	case api::format::r16g16b16_float:
		return GL_RGB16F;
	case api::format::r16g16b16_unorm:
		return GL_RGB16;
	case api::format::r16g16b16_uint:
		return GL_RGB16UI;
	case api::format::r16g16b16_snorm:
		return GL_RGB16_SNORM;
	case api::format::r16g16b16_sint:
		return GL_RGB16I;

	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_float:
		return GL_RGBA16F;
	case api::format::r16g16b16a16_unorm:
		return GL_RGBA16;
	case api::format::r16g16b16a16_uint:
		return GL_RGBA16UI;
	case api::format::r16g16b16a16_snorm:
		return GL_RGBA16_SNORM;
	case api::format::r16g16b16a16_sint:
		return GL_RGBA16I;

	case api::format::r32_typeless:
	case api::format::r32_float:
		return GL_R32F;
	case api::format::r32_uint:
		return GL_R32UI;
	case api::format::r32_sint:
		return GL_R32I;
#if 0
	case api::format::l32_float:
		return GL_LUMINANCE32F_EXT;
	case api::format::a32_float:
		return GL_ALPHA32F_EXT;
#endif

	case api::format::r32g32_typeless:
	case api::format::r32g32_float:
		return GL_RG32F;
	case api::format::r32g32_uint:
		return GL_RG32UI;
	case api::format::r32g32_sint:
		return GL_RG32I;
#if 0
	case api::format::l32a32_float:
		return GL_LUMINANCE_ALPHA32F_EXT;
#endif

	case api::format::r32g32b32_typeless:
	case api::format::r32g32b32_float:
		return GL_RGB32F;
	case api::format::r32g32b32_uint:
		return GL_RGB32UI;
	case api::format::r32g32b32_sint:
		return GL_RGB32I;

	case api::format::r32g32b32a32_typeless:
	case api::format::r32g32b32a32_float:
		return GL_RGBA32F;
	case api::format::r32g32b32a32_uint:
		return GL_RGBA32UI;
	case api::format::r32g32b32a32_sint:
		return GL_RGBA32I;

	case api::format::r9g9b9e5:
		return GL_RGB9_E5;
	case api::format::r11g11b10_float:
		return GL_R11F_G11F_B10F;
	case api::format::b5g6r5_unorm:
		return GL_RGB565;
	case api::format::b5g5r5a1_unorm:
		return GL_RGB5_A1;
	case api::format::b5g5r5x1_unorm:
		return GL_RGB5;
	case api::format::b4g4r4a4_unorm:
	case api::format::a4b4g4r4_unorm:
		return GL_RGBA4;

	case api::format::s8_uint:
		return GL_STENCIL_INDEX8;
	case api::format::d16_unorm:
		return GL_DEPTH_COMPONENT16;
	case api::format::d16_unorm_s8_uint:
		break; // Unsupported
	case api::format::d24_unorm_x8_uint:
		return GL_DEPTH_COMPONENT24;
	case api::format::r24_g8_typeless:
	case api::format::r24_unorm_x8_uint:
	case api::format::x24_unorm_g8_uint:
	case api::format::d24_unorm_s8_uint:
		return GL_DEPTH24_STENCIL8;
	case api::format::d32_float:
		return GL_DEPTH_COMPONENT32F;
	case api::format::r32_g8_typeless:
	case api::format::r32_float_x8_uint:
	case api::format::x32_float_g8_uint:
	case api::format::d32_float_s8_uint:
		return GL_DEPTH32F_STENCIL8;

	case api::format::bc1_typeless:
	case api::format::bc1_unorm:
		return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
	case api::format::bc1_unorm_srgb:
		return 0x8C4D /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT */;
	case api::format::bc2_typeless:
	case api::format::bc2_unorm:
		return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
	case api::format::bc2_unorm_srgb:
		return 0x8C4E /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT */;
	case api::format::bc3_typeless:
	case api::format::bc3_unorm:
		return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
	case api::format::bc3_unorm_srgb:
		return 0x8C4F /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT */;
	case api::format::bc4_typeless:
	case api::format::bc4_unorm:
		return GL_COMPRESSED_RED_RGTC1;
	case api::format::bc4_snorm:
		return GL_COMPRESSED_SIGNED_RED_RGTC1;
	case api::format::bc5_typeless:
	case api::format::bc5_unorm:
		return GL_COMPRESSED_RG_RGTC2;
	case api::format::bc5_snorm:
		return GL_COMPRESSED_SIGNED_RG_RGTC2;
	case api::format::bc6h_typeless:
	case api::format::bc6h_ufloat:
		return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
	case api::format::bc6h_sfloat:
		return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB;
	case api::format::bc7_typeless:
	case api::format::bc7_unorm:
		return GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
	case api::format::bc7_unorm_srgb:
		return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;

	case api::format::r8g8_b8g8_unorm:
	case api::format::g8r8_g8b8_unorm:
		break; // Unsupported
	}

	return GL_NONE;
}
auto reshade::opengl::convert_format(GLenum internal_format, const GLint swizzle_mask[4]) -> api::format
{
	// These should already have been converted by 'convert_sized_internal_format' before
	assert(
		internal_format != 2 &&
		internal_format != 3 &&
		internal_format != 4 &&
		internal_format != GL_RED &&
		internal_format != GL_ALPHA &&
		internal_format != 0x1909 /* GL_LUMINANCE */ &&
		internal_format != 0x8049 /* GL_INTENSITY */ &&
		internal_format != GL_RG &&
		internal_format != 0x190A /* GL_LUMINANCE_ALPHA */ &&
		internal_format != GL_RGB &&
		internal_format != GL_RGBA &&
		internal_format != GL_STENCIL_INDEX &&
		internal_format != GL_DEPTH_COMPONENT &&
		internal_format != GL_DEPTH_STENCIL);

	switch (internal_format)
	{
	case GL_R8: // { R, 0, 0, 1 }
		if (swizzle_mask != nullptr &&
			swizzle_mask[0] == GL_RED &&
			swizzle_mask[1] == GL_RED &&
			swizzle_mask[2] == GL_RED)
			return api::format::l8_unorm;
		if (swizzle_mask != nullptr &&
			swizzle_mask[0] == GL_ZERO &&
			swizzle_mask[1] == GL_ZERO &&
			swizzle_mask[2] == GL_ZERO &&
			swizzle_mask[3] == GL_RED)
			return api::format::a8_unorm;
		return api::format::r8_unorm;
	case GL_R8UI:
		return api::format::r8_uint;
	case GL_R8_SNORM:
		return api::format::r8_snorm;
	case GL_R8I:
		return api::format::r8_sint;
	case GL_LUMINANCE8_EXT: // { R, R, R, 1 }
	case 0x804B /* GL_INTENSITY8 */: // { R, R, R, R }
		return api::format::l8_unorm;
	case GL_ALPHA8_EXT:
		return api::format::a8_unorm;

	case GL_RG8: // { R, G, 0, 1 }
		if (swizzle_mask != nullptr &&
			swizzle_mask[0] == GL_RED &&
			swizzle_mask[1] == GL_RED &&
			swizzle_mask[2] == GL_RED &&
			swizzle_mask[3] == GL_GREEN)
			return api::format::l8a8_unorm;
		return api::format::r8g8_unorm;
	case GL_RG8UI:
		return api::format::r8g8_uint;
	case GL_RG8_SNORM:
		return api::format::r8g8_snorm;
	case GL_RG8I:
		return api::format::r8g8_sint;
	case GL_LUMINANCE8_ALPHA8_EXT: // { R, R, R, G }
		return api::format::l8a8_unorm;

	case GL_RGB8:
		return api::format::r8g8b8_unorm;
	case GL_SRGB8:
		return api::format::r8g8b8_unorm_srgb;
	case GL_RGB8UI:
		return api::format::r8g8b8_uint;
	case GL_RGB8_SNORM:
		return api::format::r8g8b8_snorm;
	case GL_RGB8I:
		return api::format::r8g8b8_sint;

	case GL_RGBA8:
		return api::format::r8g8b8a8_unorm;
	case GL_SRGB8_ALPHA8:
		return api::format::r8g8b8a8_unorm_srgb;
	case GL_RGBA8UI:
		return api::format::r8g8b8a8_uint;
	case GL_RGBA8_SNORM:
		return api::format::r8g8b8a8_snorm;
	case GL_RGBA8I:
		return api::format::r8g8b8a8_sint;
	case GL_BGRA8_EXT:
		return api::format::b8g8r8a8_unorm;

	case GL_RGB10_A2:
		return api::format::r10g10b10a2_unorm;
	case GL_RGB10_A2UI:
		return api::format::r10g10b10a2_uint;

	case GL_R16F:
		return api::format::r16_float;
	case GL_R16: // { R, 0, 0, 1 }
		if (swizzle_mask != nullptr &&
			swizzle_mask[0] == GL_RED &&
			swizzle_mask[1] == GL_RED &&
			swizzle_mask[2] == GL_RED)
			return api::format::l16_unorm;
		return api::format::r16_unorm;
	case GL_R16UI:
		return api::format::r16_uint;
	case GL_R16_SNORM:
		return api::format::r16_snorm;
	case GL_R16I:
		return api::format::r16_sint;
#if 0
	case GL_LUMINANCE16F_EXT:
		return api::format::l16_float;
	case GL_ALPHA16F_EXT:
		return api::format::a16_float;
#endif
	case 0x8042 /* GL_LUMINANCE16 */: // { R, R, R, 1 }
	case 0x804D /* GL_INTENSITY16 */: // { R, R, R, R }
		return api::format::l16_unorm;

	case GL_RG16F:
		return api::format::r16g16_float;
	case GL_RG16: // { R, G, 0, 1 }
		if (swizzle_mask != nullptr &&
			swizzle_mask[0] == GL_RED &&
			swizzle_mask[1] == GL_RED &&
			swizzle_mask[2] == GL_RED &&
			swizzle_mask[3] == GL_GREEN)
			return api::format::l16a16_unorm;
		return api::format::r16g16_unorm;
	case GL_RG16UI:
		return api::format::r16g16_uint;
	case GL_RG16_SNORM:
		return api::format::r16g16_snorm;
	case GL_RG16I:
		return api::format::r16g16_sint;
#if 0
	case GL_LUMINANCE_ALPHA16F_EXT:
		return api::format::l16a16_float;
#endif
	case 0x8048 /* GL_LUMINANCE16_ALPHA16 */: // { R, R, R, G }
		return api::format::l16a16_unorm;

	case GL_RGB16F:
		return api::format::r16g16b16_float;
	case GL_RGB16:
		return api::format::r16g16b16_unorm;
	case GL_RGB16UI:
		return api::format::r16g16b16_uint;
	case GL_RGB16_SNORM:
		return api::format::r16g16b16_snorm;
	case GL_RGB16I:
		return api::format::r16g16b16_sint;

	case GL_RGBA16F:
		return api::format::r16g16b16a16_float;
	case GL_RGBA16:
		return api::format::r16g16b16a16_unorm;
	case GL_RGBA16UI:
		return api::format::r16g16b16a16_uint;
	case GL_RGBA16_SNORM:
		return api::format::r16g16b16a16_snorm;
	case GL_RGBA16I:
		return api::format::r16g16b16a16_sint;

	case GL_R32F:
		return api::format::r32_float;
	case GL_R32UI:
		return api::format::r32_uint;
	case GL_R32I:
		return api::format::r32_sint;
#if 0
	case GL_LUMINANCE32F_EXT:
		return api::format::l32_float;
	case GL_ALPHA32F_EXT:
		return api::format::a32_float;
#endif

	case GL_RG32F:
		return api::format::r32g32_float;
	case GL_RG32UI:
		return api::format::r32g32_uint;
	case GL_RG32I:
		return api::format::r32g32_sint;
#if 0
	case GL_LUMINANCE_ALPHA32F_EXT:
		return api::format::l32a32_float;
#endif

	case GL_RGB32F:
		return api::format::r32g32b32_float;
	case GL_RGB32UI:
		return api::format::r32g32b32_uint;
	case GL_RGB32I:
		return api::format::r32g32b32_sint;

	case GL_RGBA32F:
		return api::format::r32g32b32a32_float;
	case GL_RGBA32UI:
		return api::format::r32g32b32a32_uint;
	case GL_RGBA32I:
		return api::format::r32g32b32a32_sint;

	case GL_RGB9_E5:
		return api::format::r9g9b9e5;
	case GL_R11F_G11F_B10F:
		return api::format::r11g11b10_float;
	case GL_RGB565:
		return api::format::b5g6r5_unorm;
	case GL_RGB5_A1:
		return api::format::b5g5r5a1_unorm;
	case GL_RGB5:
		return api::format::b5g5r5x1_unorm;
	case GL_RGBA4:
		return api::format::b4g4r4a4_unorm;

	case GL_STENCIL_INDEX8:
		return api::format::s8_uint;
	case GL_DEPTH_COMPONENT16:
		return api::format::d16_unorm;
	case GL_DEPTH_COMPONENT24:
		return api::format::d24_unorm_x8_uint;
	case GL_DEPTH24_STENCIL8:
		return api::format::d24_unorm_s8_uint;
	case GL_DEPTH_COMPONENT32F:
	case GL_DEPTH_COMPONENT32F_NV:
		return api::format::d32_float;
	case GL_DEPTH32F_STENCIL8:
	case GL_DEPTH32F_STENCIL8_NV:
		return api::format::d32_float_s8_uint;

	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		return api::format::bc1_unorm;
	case 0x8C4C /* GL_COMPRESSED_SRGB_S3TC_DXT1_EXT */:
	case 0x8C4D /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT */:
		return api::format::bc1_unorm_srgb;
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		return api::format::bc2_unorm;
	case 0x8C4E /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT */:
		return api::format::bc2_unorm_srgb;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		return api::format::bc3_unorm;
	case 0x8C4F /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT */:
		return api::format::bc3_unorm_srgb;
	case GL_COMPRESSED_RED_RGTC1:
		return api::format::bc4_unorm;
	case GL_COMPRESSED_SIGNED_RED_RGTC1:
		return api::format::bc4_snorm;
	case GL_COMPRESSED_RG_RGTC2:
		return api::format::bc5_unorm;
	case GL_COMPRESSED_SIGNED_RG_RGTC2:
		return api::format::bc5_snorm;
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
		return api::format::bc6h_ufloat;
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
		return api::format::bc6h_sfloat;
	case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
		return api::format::bc7_unorm;
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
		return api::format::bc7_unorm_srgb;

	default:
		return api::format::unknown;
	}
}

void reshade::opengl::convert_pixel_format(api::format format, PIXELFORMATDESCRIPTOR &pfd)
{
	switch (format)
	{
	default:
		assert(false);
		break;
	case api::format::r8g8b8a8_unorm:
	case api::format::r8g8b8a8_unorm_srgb:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 8;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 8;
		pfd.cGreenShift = 8;
		pfd.cBlueBits = 8;
		pfd.cBlueShift = 16;
		pfd.cAlphaBits = 8;
		pfd.cAlphaShift = 24;
		break;
	case api::format::r8g8b8_unorm:
	case api::format::r8g8b8_unorm_srgb:
	case api::format::r8g8b8x8_unorm:
	case api::format::r8g8b8x8_unorm_srgb:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 8;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 8;
		pfd.cGreenShift = 8;
		pfd.cBlueBits = 8;
		pfd.cBlueShift = 16;
		pfd.cAlphaBits = 0;
		pfd.cAlphaShift = 0;
		break;
	case api::format::b8g8r8a8_unorm:
	case api::format::b8g8r8a8_unorm_srgb:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 8;
		pfd.cRedShift = 16;
		pfd.cGreenBits = 8;
		pfd.cGreenShift = 8;
		pfd.cBlueBits = 8;
		pfd.cBlueShift = 0;
		pfd.cAlphaBits = 8;
		pfd.cAlphaShift = 24;
		break;
	case api::format::b8g8r8x8_unorm:
	case api::format::b8g8r8x8_unorm_srgb:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 8;
		pfd.cRedShift = 16;
		pfd.cGreenBits = 8;
		pfd.cGreenShift = 8;
		pfd.cBlueBits = 8;
		pfd.cBlueShift = 0;
		pfd.cAlphaBits = 0;
		pfd.cAlphaShift = 0;
		break;
	case api::format::r10g10b10a2_unorm:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 10;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 10;
		pfd.cGreenBits = 10;
		pfd.cBlueBits = 10;
		pfd.cBlueShift = 20;
		pfd.cAlphaBits = 2;
		pfd.cAlphaShift = 30;
		break;
	case api::format::r16g16b16a16_float:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 64;
		pfd.cRedBits = 16;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 16;
		pfd.cGreenShift = 16;
		pfd.cBlueBits = 16;
		pfd.cBlueShift = 32;
		pfd.cAlphaBits = 16;
		pfd.cAlphaShift = 48;
		break;
	case api::format::r32g32b32a32_float:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 128;
		pfd.cRedBits = 32;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 32;
		pfd.cGreenShift = 32;
		pfd.cBlueBits = 32;
		pfd.cBlueShift = 64;
		pfd.cAlphaBits = 32;
		pfd.cAlphaShift = 96;
		break;
	case api::format::r11g11b10_float:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cRedBits = 11;
		pfd.cRedShift = 0;
		pfd.cGreenBits = 11;
		pfd.cGreenShift = 11;
		pfd.cBlueBits = 10;
		pfd.cBlueShift = 22;
		pfd.cAlphaBits = 0;
		pfd.cAlphaShift = 0;
		break;
	case api::format::b5g6r5_unorm:
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 16;
		pfd.cRedBits = 5;
		pfd.cRedShift = 11;
		pfd.cGreenBits = 6;
		pfd.cGreenShift = 5;
		pfd.cBlueBits = 5;
		pfd.cBlueShift = 0;
		pfd.cAlphaBits = 0;
		pfd.cAlphaShift = 0;
		break;
	}
}
auto reshade::opengl::convert_pixel_format(const PIXELFORMATDESCRIPTOR &pfd) -> api::format
{
	assert(pfd.iPixelType == PFD_TYPE_RGBA);

	switch (pfd.cColorBits)
	{
	default:
		assert(false);
		return api::format::unknown;
	case 16:
		return api::format::b5g6r5_unorm;
	case 24:
	case 32:
		if (pfd.cRedBits == 11 && pfd.cGreenBits == 11 && pfd.cBlueBits == 10)
			return api::format::r11g11b10_float;
		if (pfd.cAlphaBits != 0)
			return pfd.cRedShift == 0 ? api::format::r8g8b8a8_unorm : api::format::b8g8r8a8_unorm;
		else
			return pfd.cRedShift == 0 ? api::format::r8g8b8x8_unorm : api::format::b8g8r8x8_unorm;
	case 30:
		return api::format::r10g10b10a2_unorm;
	case 64:
		return api::format::r16g16b16a16_float;
	case 128:
		return api::format::r32g32b32a32_float;
	}
}

auto reshade::opengl::convert_upload_format(api::format format, GLenum &type) -> GLenum
{
	switch (format)
	{
	case api::format::r8_typeless:
	case api::format::r8_unorm:
	case api::format::l8_unorm:
		type = GL_UNSIGNED_BYTE;
		return GL_RED;
	case api::format::r8_uint:
		type = GL_UNSIGNED_BYTE;
		return GL_RED_INTEGER;
	case api::format::r8_snorm:
		type = GL_BYTE;
		return GL_RED;
	case api::format::r8_sint:
		type = GL_BYTE;
		return GL_RED_INTEGER;
	case api::format::a8_unorm:
		type = GL_UNSIGNED_BYTE;
		return GL_ALPHA;

	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
	case api::format::l8a8_unorm:
		type = GL_UNSIGNED_BYTE;
		return GL_RG;
	case api::format::r8g8_uint:
		type = GL_UNSIGNED_BYTE;
		return GL_RG_INTEGER;
	case api::format::r8g8_snorm:
		type = GL_BYTE;
		return GL_RG;
	case api::format::r8g8_sint:
		type = GL_BYTE;
		return GL_RG_INTEGER;

	case api::format::r8g8b8_typeless:
	case api::format::r8g8b8_unorm:
	case api::format::r8g8b8_unorm_srgb:
		type = GL_UNSIGNED_BYTE;
		return GL_RGB;
	case api::format::r8g8b8_uint:
		type = GL_UNSIGNED_BYTE;
		return GL_RGB_INTEGER;
	case api::format::r8g8b8_snorm:
		type = GL_BYTE;
		return GL_RGB;
	case api::format::r8g8b8_sint:
		type = GL_BYTE;
		return GL_RGB_INTEGER;

	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
	case api::format::r8g8b8a8_unorm_srgb:
	case api::format::r8g8b8x8_unorm:
	case api::format::r8g8b8x8_unorm_srgb:
		type = GL_UNSIGNED_BYTE;
		return GL_RGBA;
	case api::format::r8g8b8a8_uint:
		type = GL_UNSIGNED_BYTE;
		return GL_RGBA_INTEGER;
	case api::format::r8g8b8a8_snorm:
		type = GL_BYTE;
		return GL_RGBA;
	case api::format::r8g8b8a8_sint:
		type = GL_BYTE;
		return GL_RGBA_INTEGER;
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
	case api::format::b8g8r8a8_unorm_srgb:
	case api::format::b8g8r8x8_typeless:
	case api::format::b8g8r8x8_unorm:
	case api::format::b8g8r8x8_unorm_srgb:
		type = GL_UNSIGNED_BYTE;
		return GL_BGRA;

	case api::format::r10g10b10a2_typeless:
	case api::format::r10g10b10a2_unorm:
		type = GL_UNSIGNED_INT_2_10_10_10_REV;
		return GL_RGBA;
	case api::format::r10g10b10a2_uint:
	case api::format::r10g10b10a2_xr_bias:
		type = GL_UNSIGNED_INT_2_10_10_10_REV;
		return GL_RGBA_INTEGER;
	case api::format::b10g10r10a2_typeless:
	case api::format::b10g10r10a2_unorm:
		type = GL_UNSIGNED_INT_2_10_10_10_REV;
		return GL_BGRA;
	case api::format::b10g10r10a2_uint:
		type = GL_UNSIGNED_INT_2_10_10_10_REV;
		return GL_BGRA_INTEGER;

	case api::format::r16_float:
		type = GL_HALF_FLOAT;
		return GL_RED;
	case api::format::r16_typeless:
	case api::format::r16_unorm:
	case api::format::l16_unorm:
		type = GL_UNSIGNED_SHORT;
		return GL_RED;
	case api::format::r16_uint:
		type = GL_UNSIGNED_SHORT;
		return GL_RED_INTEGER;
	case api::format::r16_snorm:
		type = GL_SHORT;
		return GL_RED;
	case api::format::r16_sint:
		type = GL_SHORT;
		return GL_RED_INTEGER;

	case api::format::r16g16_float:
		type = GL_HALF_FLOAT;
		return GL_RG;
	case api::format::r16g16_typeless:
	case api::format::r16g16_unorm:
	case api::format::l16a16_unorm:
		type = GL_UNSIGNED_SHORT;
		return GL_RG;
	case api::format::r16g16_uint:
		type = GL_UNSIGNED_SHORT;
		return GL_RG_INTEGER;
	case api::format::r16g16_snorm:
		type = GL_SHORT;
		return GL_RG;
	case api::format::r16g16_sint:
		type = GL_SHORT;
		return GL_RG_INTEGER;

	case api::format::r16g16b16_float:
		type = GL_HALF_FLOAT;
		return GL_RGB;
	case api::format::r16g16b16_typeless:
	case api::format::r16g16b16_unorm:
		type = GL_UNSIGNED_SHORT;
		return GL_RGB;
	case api::format::r16g16b16_uint:
		type = GL_UNSIGNED_SHORT;
		return GL_RGB_INTEGER;
	case api::format::r16g16b16_snorm:
		type = GL_SHORT;
		return GL_RGB;
	case api::format::r16g16b16_sint:
		type = GL_SHORT;
		return GL_RGB_INTEGER;

	case api::format::r16g16b16a16_float:
		type = GL_HALF_FLOAT;
		return GL_RGBA;
	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_unorm:
		type = GL_UNSIGNED_SHORT;
		return GL_RGBA;
	case api::format::r16g16b16a16_uint:
		type = GL_UNSIGNED_SHORT;
		return GL_RGBA_INTEGER;
	case api::format::r16g16b16a16_snorm:
		type = GL_SHORT;
		return GL_RGBA;
	case api::format::r16g16b16a16_sint:
		type = GL_SHORT;
		return GL_RGBA_INTEGER;

	case api::format::r32_typeless:
	case api::format::r32_float:
		type = GL_FLOAT;
		return GL_RED;
	case api::format::r32_uint:
		type = GL_UNSIGNED_INT;
		return GL_RED_INTEGER;
	case api::format::r32_sint:
		type = GL_INT;
		return GL_RED_INTEGER;

	case api::format::r32g32_typeless:
	case api::format::r32g32_float:
		type = GL_FLOAT;
		return GL_RG;
	case api::format::r32g32_uint:
		type = GL_UNSIGNED_INT;
		return GL_RG_INTEGER;
	case api::format::r32g32_sint:
		type = GL_INT;
		return GL_RG_INTEGER;

	case api::format::r32g32b32_typeless:
	case api::format::r32g32b32_float:
		type = GL_FLOAT;
		return GL_RGB;
	case api::format::r32g32b32_uint:
		type = GL_UNSIGNED_INT;
		return GL_RGB_INTEGER;
	case api::format::r32g32b32_sint:
		type = GL_INT;
		return GL_RGB_INTEGER;

	case api::format::r32g32b32a32_typeless:
	case api::format::r32g32b32a32_float:
		type = GL_FLOAT;
		return GL_RGBA;
	case api::format::r32g32b32a32_uint:
		type = GL_UNSIGNED_INT;
		return GL_RGBA_INTEGER;
	case api::format::r32g32b32a32_sint:
		type = GL_INT;
		return GL_RGBA_INTEGER;

	case api::format::r9g9b9e5:
		type = GL_UNSIGNED_INT_5_9_9_9_REV;
		return GL_RGBA;
	case api::format::r11g11b10_float:
		type = GL_UNSIGNED_INT_10F_11F_11F_REV;
		return GL_RGB;
	case api::format::b5g6r5_unorm:
		type = GL_UNSIGNED_SHORT_5_6_5_REV;
		return GL_BGR;
	case api::format::b5g5r5a1_unorm:
		type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		return GL_BGRA;
	case api::format::b5g5r5x1_unorm:
		type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		return GL_BGRA;
	case api::format::b4g4r4a4_unorm:
		type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
		return GL_BGRA;
	case api::format::a4b4g4r4_unorm:
		type = GL_UNSIGNED_SHORT_4_4_4_4;
		return GL_RGBA;

	case api::format::s8_uint:
		type = GL_UNSIGNED_BYTE;
		return GL_STENCIL_INDEX;
	case api::format::d16_unorm:
		type = GL_UNSIGNED_SHORT;
		return GL_DEPTH_COMPONENT;
	case api::format::d24_unorm_x8_uint:
		type = GL_UNSIGNED_INT;
		return GL_DEPTH_COMPONENT;
	case api::format::r24_g8_typeless:
	case api::format::r24_unorm_x8_uint:
	case api::format::x24_unorm_g8_uint:
	case api::format::d24_unorm_s8_uint:
		type = GL_UNSIGNED_INT_24_8;
		return GL_DEPTH_STENCIL;
	case api::format::d32_float:
		type = GL_FLOAT;
		return GL_DEPTH_COMPONENT;
	case api::format::r32_g8_typeless:
	case api::format::r32_float_x8_uint:
	case api::format::x32_float_g8_uint:
	case api::format::d32_float_s8_uint:
		type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
		return GL_DEPTH_STENCIL;

	case api::format::bc1_typeless:
	case api::format::bc1_unorm:
	case api::format::bc1_unorm_srgb:
	case api::format::bc2_typeless:
	case api::format::bc2_unorm:
	case api::format::bc2_unorm_srgb:
	case api::format::bc3_typeless:
	case api::format::bc3_unorm:
	case api::format::bc3_unorm_srgb:
	case api::format::bc4_typeless:
	case api::format::bc4_unorm:
	case api::format::bc4_snorm:
	case api::format::bc5_typeless:
	case api::format::bc5_unorm:
	case api::format::bc5_snorm:
	case api::format::bc6h_typeless:
	case api::format::bc6h_ufloat:
	case api::format::bc6h_sfloat:
	case api::format::bc7_typeless:
	case api::format::bc7_unorm:
	case api::format::bc7_unorm_srgb:
		type = GL_COMPRESSED_TEXTURE_FORMATS;
		return convert_format(format);

	default:
		assert(false);
		break;
	}

	return type = GL_NONE;
}
auto reshade::opengl::convert_upload_format(GLenum format, GLenum type) -> api::format
{
	switch (format)
	{
	case GL_RED:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8_snorm;
		case GL_UNSIGNED_BYTE:
			return api::format::r8_unorm;
		case GL_SHORT:
			return api::format::r16_snorm;
		case GL_UNSIGNED_SHORT:
			return api::format::r16_unorm;
		case GL_HALF_FLOAT:
			return api::format::r16_float;
		case GL_INT:
			return api::format::r32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32_uint;
		case GL_FLOAT:
			return api::format::r32_float;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RED_INTEGER:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8_sint;
		case GL_UNSIGNED_BYTE:
			return api::format::r8_uint;
		case GL_SHORT:
			return api::format::r16_sint;
		case GL_UNSIGNED_SHORT:
			return api::format::r16_uint;
		case GL_INT:
			return api::format::r32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_ALPHA:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::a8_unorm;
		case GL_UNSIGNED_SHORT: // Used by Amnesia: A Machine for Pigs (to upload to a GL_ALPHA8 texture)
			return api::format::unknown;
		default:
			assert(false);
			return api::format::unknown;
		}
	case 0x1909 /* GL_LUMINANCE */:
	case 0x8049 /* GL_INTENSITY */:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::l8_unorm;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RG:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8_snorm;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8_unorm;
		case GL_SHORT:
			return api::format::r16g16_snorm;
		case GL_UNSIGNED_SHORT:
			return api::format::r16g16_unorm;
		case GL_HALF_FLOAT:
			return api::format::r16g16_float;
		case GL_INT:
			return api::format::r32g32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32_uint;
		case GL_FLOAT:
			return api::format::r32g32_float;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RG_INTEGER:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8_sint;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8_uint;
		case GL_SHORT:
			return api::format::r16g16_sint;
		case GL_UNSIGNED_SHORT:
			return api::format::r16g16_uint;
		case GL_INT:
			return api::format::r32g32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case 0x190A /* GL_LUMINANCE_ALPHA */:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::l8a8_unorm;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RGB:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8b8_snorm;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8b8_unorm;
		case GL_SHORT:
			return api::format::r16g16b16_snorm;
		case GL_UNSIGNED_SHORT: // Used by Amnesia: A Machine for Pigs (to upload to a GL_RGB8 texture)
			return api::format::r16g16b16_unorm;
		case GL_HALF_FLOAT:
			return api::format::r16g16b16_float;
		case GL_INT:
			return api::format::r32g32b32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32b32_uint;
		case GL_UNSIGNED_INT_10F_11F_11F_REV:
			return api::format::r11g11b10_float;
		case GL_FLOAT:
			return api::format::r32g32b32_float;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RGB_INTEGER:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8b8_sint;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8b8_uint;
		case GL_SHORT:
			return api::format::r16g16b16_sint;
		case GL_UNSIGNED_SHORT:
			return api::format::r16g16b16_uint;
		case GL_INT:
			return api::format::r32g32b32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32b32_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_BGR:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::b8g8r8_unorm;
		case GL_UNSIGNED_SHORT: // Used by Amnesia: A Machine for Pigs (to upload to a GL_RGB8 texture)
			return api::format::unknown;
		case GL_UNSIGNED_SHORT_5_6_5_REV:
			return api::format::b5g6r5_unorm;
		case GL_UNSIGNED_SHORT_1_5_5_5_REV:
			return api::format::b5g5r5x1_unorm;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_BGR_INTEGER:
		{
			assert(false);
			return api::format::unknown;
		}
	case GL_RGBA:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8b8a8_snorm;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8b8a8_unorm;
		case GL_SHORT:
			return api::format::r16g16b16a16_snorm;
		case GL_UNSIGNED_SHORT:
			return api::format::r16g16b16a16_unorm;
		case GL_UNSIGNED_SHORT_4_4_4_4:
			return api::format::a4b4g4r4_unorm;
		case GL_HALF_FLOAT:
			return api::format::r16g16b16a16_float;
		case GL_INT:
			return api::format::r32g32b32a32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32b32a32_uint;
		case GL_UNSIGNED_INT_8_8_8_8: // Not technically correct here, since it is ABGR8, but used by RPCS3
		case GL_UNSIGNED_INT_8_8_8_8_REV: // On a little endian machine the least-significant byte is stored first
			return api::format::r8g8b8a8_unorm;
		case GL_UNSIGNED_INT_2_10_10_10_REV:
			return api::format::r10g10b10a2_unorm;
		case GL_UNSIGNED_INT_5_9_9_9_REV:
			return api::format::r9g9b9e5;
		case GL_FLOAT:
			return api::format::r32g32b32a32_float;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_RGBA_INTEGER:
		switch (type)
		{
		case GL_BYTE:
			return api::format::r8g8b8a8_sint;
		case GL_UNSIGNED_BYTE:
			return api::format::r8g8b8a8_uint;
		case GL_SHORT:
			return api::format::r16g16b16a16_sint;
		case GL_UNSIGNED_SHORT:
			return api::format::r16g16b16a16_uint;
		case GL_INT:
			return api::format::r32g32b32a32_sint;
		case GL_UNSIGNED_INT:
			return api::format::r32g32b32a32_uint;
		case GL_UNSIGNED_INT_2_10_10_10_REV:
			return api::format::r10g10b10a2_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_BGRA:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::b8g8r8a8_unorm;
		case GL_UNSIGNED_SHORT: // Used by Amnesia: Rebirth
			return api::format::unknown;
		case GL_UNSIGNED_SHORT_4_4_4_4_REV:
			return api::format::b4g4r4a4_unorm;
		case GL_UNSIGNED_SHORT_1_5_5_5_REV:
			return api::format::b5g5r5a1_unorm;
		case GL_UNSIGNED_INT_8_8_8_8:
		case GL_UNSIGNED_INT_8_8_8_8_REV:
			return api::format::b8g8r8a8_unorm;
		case GL_UNSIGNED_INT_2_10_10_10_REV:
			return api::format::b10g10r10a2_unorm;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_BGRA_INTEGER:
		switch (type)
		{
		case GL_UNSIGNED_INT_2_10_10_10_REV:
			return api::format::b10g10r10a2_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_STENCIL_INDEX:
		switch (type)
		{
		case GL_UNSIGNED_BYTE:
			return api::format::s8_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_DEPTH_COMPONENT:
		switch (type)
		{
		case GL_UNSIGNED_SHORT:
			return api::format::d16_unorm;
		case GL_UNSIGNED_INT:
			return api::format::d24_unorm_x8_uint;
		case GL_FLOAT:
			return api::format::d32_float;
		default:
			assert(false);
			return api::format::unknown;
		}
	case GL_DEPTH_STENCIL:
		switch (type)
		{
		case GL_UNSIGNED_INT_24_8:
			return api::format::d24_unorm_s8_uint;
		case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
			return api::format::d32_float_s8_uint;
		default:
			assert(false);
			return api::format::unknown;
		}
	default:
		return convert_format(format);
	}
}

auto reshade::opengl::convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum
{
	size = 0;
	normalized = GL_FALSE;

	switch (format)
	{
	case api::format::r8g8b8_unorm:
	case api::format::r8g8b8x8_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r8g8b8_uint:
		size = 3;
		return GL_UNSIGNED_BYTE;
	case api::format::r8g8b8_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r8g8b8_sint:
		size = 3;
		return GL_BYTE;

	case api::format::b8g8r8_unorm:
	case api::format::b8g8r8x8_unorm:
		normalized = GL_TRUE;
		size = GL_BGR;
		return GL_UNSIGNED_BYTE;

	case api::format::r8g8b8a8_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r8g8b8a8_uint:
		size = 4;
		return GL_UNSIGNED_BYTE;
	case api::format::r8g8b8a8_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r8g8b8a8_sint:
		size = 4;
		return GL_BYTE;

	case api::format::b8g8r8a8_unorm:
		normalized = GL_TRUE;
		size = GL_BGRA;
		return GL_UNSIGNED_BYTE;

	case api::format::r10g10b10a2_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r10g10b10a2_uint:
		size = 4;
		return GL_UNSIGNED_INT_2_10_10_10_REV;

	case api::format::b10g10r10a2_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::b10g10r10a2_uint:
		size = GL_BGRA;
		return GL_UNSIGNED_INT_2_10_10_10_REV;

	case api::format::r16_float:
		size = 1;
		return GL_HALF_FLOAT;
	case api::format::r16_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16_uint:
		size = 1;
		return GL_UNSIGNED_SHORT;
	case api::format::r16_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16_sint:
		size = 1;
		return GL_SHORT;

	case api::format::r16g16_float:
		size = 2;
		return GL_HALF_FLOAT;
	case api::format::r16g16_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16_uint:
		size = 2;
		return GL_UNSIGNED_SHORT;
	case api::format::r16g16_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16_sint:
		size = 2;
		return GL_SHORT;

	case api::format::r16g16b16_float:
		size = 3;
		return GL_HALF_FLOAT;
	case api::format::r16g16b16_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16b16_uint:
		size = 3;
		return GL_UNSIGNED_SHORT;
	case api::format::r16g16b16_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16b16_sint:
		size = 3;
		return GL_SHORT;

	case api::format::r16g16b16a16_float:
		size = 4;
		return GL_HALF_FLOAT;
	case api::format::r16g16b16a16_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16b16a16_uint:
		size = 4;
		return GL_UNSIGNED_SHORT;
	case api::format::r16g16b16a16_snorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r16g16b16a16_sint:
		size = 4;
		return GL_SHORT;

	case api::format::r32_float:
		size = 1;
		return GL_FLOAT;
	case api::format::r32_uint:
		size = 1;
		return GL_UNSIGNED_INT;
	case api::format::r32_sint:
		size = 1;
		return GL_INT;

	case api::format::r32g32_float:
		size = 2;
		return GL_FLOAT;
	case api::format::r32g32_uint:
		size = 2;
		return GL_UNSIGNED_INT;
	case api::format::r32g32_sint:
		size = 2;
		return GL_INT;

	case api::format::r32g32b32_float:
		size = 3;
		return GL_FLOAT;
	case api::format::r32g32b32_uint:
		size = 3;
		return GL_UNSIGNED_INT;
	case api::format::r32g32b32_sint:
		size = 3;
		return GL_INT;

	case api::format::r32g32b32a32_float:
		size = 4;
		return GL_FLOAT;
	case api::format::r32g32b32a32_uint:
		size = 4;
		return GL_UNSIGNED_INT;
	case api::format::r32g32b32a32_sint:
		size = 4;
		return GL_INT;
	}

	assert(false);
	return GL_NONE;
}
auto reshade::opengl::convert_attrib_format(GLint size, GLenum type, GLboolean normalized) -> api::format
{
	switch (size)
	{
	case 1:
		return convert_upload_format(normalized || type == GL_FLOAT || type == GL_HALF_FLOAT ? GL_RED : GL_RED_INTEGER, type);
	case 2:
		return convert_upload_format(normalized || type == GL_FLOAT || type == GL_HALF_FLOAT ? GL_RG : GL_RG_INTEGER, type);
	case 3:
		return convert_upload_format(normalized || type == GL_FLOAT || type == GL_HALF_FLOAT ? GL_RGB : GL_RGB_INTEGER, type);
	case 4:
		return convert_upload_format(normalized || type == GL_FLOAT || type == GL_HALF_FLOAT ? GL_RGBA : GL_RGBA_INTEGER, type);
	case GL_BGRA:
		assert(normalized);
		return convert_upload_format(GL_BGRA, type);
	default:
		assert(false);
		return api::format::unknown;
	}
}

auto reshade::opengl::convert_sized_internal_format(GLenum internal_format, GLenum format) -> GLenum
{
	// Convert base internal formats to sized internal formats
	switch (internal_format)
	{
	case 1:
		if (format == GL_ALPHA)
			return 0x803C /* GL_ALPHA8 */;
		if (format == 0x1909 /* GL_LUMINANCE */)
			return 0x8040 /* GL_LUMINANCE8 */;
		if (format == 0x8049 /* GL_INTENSITY */)
			return 0x804B /* GL_INTENSITY8 */;
		[[fallthrough]];
	case GL_RED:
		return GL_R8;
	case GL_ALPHA:
		return 0x803C /* GL_ALPHA8 */;
	case 0x1909 /* GL_LUMINANCE */:
		return 0x8040 /* GL_LUMINANCE8 */;
	case 0x8049 /* GL_INTENSITY */:
		return 0x804B /* GL_INTENSITY8 */;
	case 2:
		if (format == 0x190A /* GL_LUMINANCE_ALPHA */) // Used by Penumbra: Overture
			return 0x8045 /* GL_LUMINANCE8_ALPHA8 */;
		[[fallthrough]];
	case GL_RG:
		return GL_RG8;
	case 0x190A /* GL_LUMINANCE_ALPHA */:
		return 0x8045 /* GL_LUMINANCE8_ALPHA8 */;
	case 3:
	case GL_RGB:
		return GL_RGB8;
	case 4:
	case GL_RGBA:
		return GL_RGBA8;
	case GL_STENCIL_INDEX:
		return GL_STENCIL_INDEX8;
	case GL_DEPTH_COMPONENT:
		return GL_DEPTH_COMPONENT24;
	case GL_DEPTH_COMPONENT32:
	// Replace formats from 'GL_NV_depth_buffer_float' extension with their core variants
	case GL_DEPTH_COMPONENT32F_NV:
		return GL_DEPTH_COMPONENT32F;
	case GL_DEPTH_STENCIL:
		return GL_DEPTH24_STENCIL8;
	case GL_DEPTH32F_STENCIL8_NV:
		return GL_DEPTH32F_STENCIL8;
	default:
		return internal_format;
	}
}

auto reshade::opengl::is_depth_stencil_format(api::format format) -> GLenum
{
	switch (format)
	{
	default:
		return GL_NONE;
	case api::format::s8_uint:
		return GL_STENCIL_ATTACHMENT;
	case api::format::d16_unorm:
	case api::format::d24_unorm_x8_uint:
	case api::format::d32_float:
		return GL_DEPTH_ATTACHMENT;
	case api::format::d24_unorm_s8_uint:
	case api::format::d32_float_s8_uint:
		return GL_DEPTH_STENCIL_ATTACHMENT;
	}
}

auto reshade::opengl::convert_access_flags(reshade::api::map_access flags) -> GLbitfield
{
	switch (flags)
	{
	case api::map_access::read_only:
		return GL_MAP_READ_BIT;
	case api::map_access::write_only:
		return GL_MAP_WRITE_BIT;
	case api::map_access::read_write:
		return GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
	case api::map_access::write_discard:
		return GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
	default:
		return 0;
	}
}
reshade::api::map_access reshade::opengl::convert_access_flags(GLbitfield flags)
{
	if ((flags & (GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)) != 0)
		return reshade::api::map_access::write_discard;

	switch (flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))
	{
	case GL_MAP_READ_BIT:
		return reshade::api::map_access::read_only;
	case GL_MAP_WRITE_BIT:
		return reshade::api::map_access::write_only;
	default:
		return reshade::api::map_access::read_write;
	}
}

void reshade::opengl::convert_resource_desc(const api::resource_desc &desc, GLsizeiptr &buffer_size, GLbitfield &storage_flags)
{
	assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
	buffer_size = static_cast<GLsizeiptr>(desc.buffer.size);

	switch (desc.heap)
	{
	default:
	case api::memory_heap::unknown:
		storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
		break;
	case api::memory_heap::gpu_only:
		storage_flags = 0;
		break;
	case api::memory_heap::cpu_to_gpu:
		storage_flags = GL_MAP_WRITE_BIT;
		break;
	case api::memory_heap::gpu_to_cpu:
		storage_flags = GL_MAP_READ_BIT;
		break;
	case api::memory_heap::cpu_only:
		storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_CLIENT_STORAGE_BIT;
		break;
	}

	if ((desc.flags & api::resource_flags::dynamic) != 0)
		storage_flags |= GL_DYNAMIC_STORAGE_BIT;
}
reshade::api::resource_type reshade::opengl::convert_resource_type(GLenum target)
{
	switch (target)
	{
	case GL_BUFFER:
	case GL_ARRAY_BUFFER:
	case GL_ELEMENT_ARRAY_BUFFER:
	case GL_PIXEL_PACK_BUFFER:
	case GL_PIXEL_UNPACK_BUFFER:
	case GL_UNIFORM_BUFFER:
	case GL_TEXTURE_BUFFER:
	case GL_TRANSFORM_FEEDBACK_BUFFER:
	case GL_COPY_READ_BUFFER:
	case GL_COPY_WRITE_BUFFER:
	case GL_DRAW_INDIRECT_BUFFER:
	case GL_SHADER_STORAGE_BUFFER:
	case GL_DISPATCH_INDIRECT_BUFFER:
	case GL_QUERY_BUFFER:
	case GL_ATOMIC_COUNTER_BUFFER:
		return api::resource_type::buffer;
	case GL_TEXTURE_1D:
	case GL_TEXTURE_1D_ARRAY:
	case GL_PROXY_TEXTURE_1D:
	case GL_PROXY_TEXTURE_1D_ARRAY:
		return api::resource_type::texture_1d;
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_RECTANGLE: // This is not technically compatible with 2D textures
	case GL_PROXY_TEXTURE_2D:
	case GL_PROXY_TEXTURE_2D_ARRAY:
	case GL_PROXY_TEXTURE_RECTANGLE:
		return api::resource_type::texture_2d;
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
		return api::resource_type::texture_2d;
	case GL_TEXTURE_3D:
	case GL_PROXY_TEXTURE_3D:
		return api::resource_type::texture_3d;
	case GL_TEXTURE_CUBE_MAP:
	case GL_TEXTURE_CUBE_MAP_ARRAY:
	case GL_PROXY_TEXTURE_CUBE_MAP:
	case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		return api::resource_type::texture_2d;
	case GL_RENDERBUFFER:
	case GL_FRAMEBUFFER_DEFAULT:
		return api::resource_type::surface;
	default:
		assert(false);
		return api::resource_type::unknown;
	}
}
reshade::api::resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizeiptr buffer_size, GLbitfield storage_flags)
{
	api::resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.buffer.size = buffer_size;
	desc.buffer.stride = 0;

	switch (storage_flags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))
	{
	case GL_MAP_READ_BIT | GL_MAP_WRITE_BIT:
		desc.heap = api::memory_heap::unknown;
		break;
	case 0:
		desc.heap = api::memory_heap::gpu_only;
		break;
	case GL_MAP_WRITE_BIT:
		desc.heap = api::memory_heap::cpu_to_gpu;
		break;
	case GL_MAP_READ_BIT:
		desc.heap = api::memory_heap::gpu_to_cpu;
		break;
	}

	desc.usage = api::resource_usage::copy_dest | api::resource_usage::copy_source;
	if (target == GL_ELEMENT_ARRAY_BUFFER)
		desc.usage |= api::resource_usage::index_buffer;
	else if (target == GL_ARRAY_BUFFER)
		desc.usage |= api::resource_usage::vertex_buffer;
	else if (target == GL_UNIFORM_BUFFER)
		desc.usage |= api::resource_usage::constant_buffer;
	else if (target == GL_TRANSFORM_FEEDBACK_BUFFER)
		desc.usage |= api::resource_usage::stream_output;
	else if (target == GL_DRAW_INDIRECT_BUFFER || target == GL_DISPATCH_INDIRECT_BUFFER)
		desc.usage |= api::resource_usage::indirect_argument;
	else
		desc.usage |= api::resource_usage::shader_resource;

	if ((storage_flags & GL_DYNAMIC_STORAGE_BIT) != 0)
		desc.flags |= api::resource_flags::dynamic;

	return desc;
}
reshade::api::resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height, GLsizei depth, const GLint swizzle_mask[4])
{
	api::resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.texture.width = width;
	desc.texture.height = height;
	assert(depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	desc.texture.format = convert_format(internal_format, swizzle_mask);
	desc.texture.samples = static_cast<uint16_t>(samples);
	desc.heap = api::memory_heap::gpu_only;

	desc.usage = api::resource_usage::copy_dest | api::resource_usage::copy_source | api::resource_usage::resolve_dest;
	if (desc.texture.samples >= 2)
		desc.usage = api::resource_usage::resolve_source;

	if (is_depth_stencil_format(desc.texture.format))
		desc.usage |= api::resource_usage::depth_stencil;
	if (desc.type == api::resource_type::texture_1d || desc.type == api::resource_type::texture_2d || desc.type == api::resource_type::surface)
		desc.usage |= api::resource_usage::render_target;

	if (desc.type != api::resource_type::surface)
		desc.usage |= api::resource_usage::shader_resource;

	assert(!(target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
		target == GL_PROXY_TEXTURE_CUBE_MAP || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY)
	{
		desc.texture.depth_or_layers *= 6;
		desc.flags |= api::resource_flags::cube_compatible;
	}

	// Mipmap generation is supported for all textures
	if (levels != 1 && desc.type == api::resource_type::texture_2d)
		desc.flags |= api::resource_flags::generate_mipmaps;

	return desc;
}

reshade::api::resource_view_type reshade::opengl::convert_resource_view_type(GLenum target)
{
	switch (target)
	{
	case GL_TEXTURE_BUFFER:
		return api::resource_view_type::buffer;
	case GL_TEXTURE_1D:
	case GL_PROXY_TEXTURE_1D:
		return api::resource_view_type::texture_1d;
	case GL_TEXTURE_1D_ARRAY:
	case GL_PROXY_TEXTURE_1D_ARRAY:
		return api::resource_view_type::texture_1d_array;
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
	case GL_PROXY_TEXTURE_2D:
	case GL_PROXY_TEXTURE_RECTANGLE:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X: // Single cube face is a 2D view
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		return api::resource_view_type::texture_2d;
	case GL_TEXTURE_2D_ARRAY:
	case GL_PROXY_TEXTURE_2D_ARRAY:
		return api::resource_view_type::texture_2d_array;
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
		return api::resource_view_type::texture_2d_multisample;
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
		return api::resource_view_type::texture_2d_multisample_array;
	case GL_TEXTURE_3D:
	case GL_PROXY_TEXTURE_3D:
		return api::resource_view_type::texture_3d;
	case GL_TEXTURE_CUBE_MAP:
	case GL_PROXY_TEXTURE_CUBE_MAP:
		return api::resource_view_type::texture_cube;
	case GL_TEXTURE_CUBE_MAP_ARRAY:
	case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
		return api::resource_view_type::texture_cube_array;
	case GL_RENDERBUFFER:
		return api::resource_view_type::texture_2d; // There is no explicit surface view type
	default:
		assert(false);
		return api::resource_view_type::unknown;
	}
}
reshade::api::resource_view_desc reshade::opengl::convert_resource_view_desc(GLenum target, GLenum internal_format, GLintptr offset, GLsizeiptr size)
{
	assert(convert_resource_view_type(target) == api::resource_view_type::buffer && size != 0);
	return api::resource_view_desc(convert_format(internal_format), offset, size);
}
reshade::api::resource_view_desc reshade::opengl::convert_resource_view_desc(GLenum target, GLenum internal_format, GLuint min_level, GLuint num_levels, GLuint min_layer, GLuint num_layers)
{
	return api::resource_view_desc(convert_resource_view_type(target), convert_format(internal_format), min_level, num_levels, min_layer, num_layers);
}

GLuint reshade::opengl::get_index_type_size(GLenum index_type)
{
#if 1
	assert(index_type == GL_UNSIGNED_BYTE || index_type == GL_UNSIGNED_SHORT || index_type == GL_UNSIGNED_INT);
	static_assert(((GL_UNSIGNED_SHORT - GL_UNSIGNED_BYTE) == 2) && ((GL_UNSIGNED_INT - GL_UNSIGNED_BYTE) == 4));
	return 1 << ((index_type - GL_UNSIGNED_BYTE) / 2);
#else
	switch (index_type)
	{
	default:
		assert(false);
		return 0;
	case GL_UNSIGNED_BYTE:
		return 1;
	case GL_UNSIGNED_SHORT:
		return 2;
	case GL_UNSIGNED_INT:
		return 4;
	}
#endif
}

GLenum reshade::opengl::get_binding_for_target(GLenum target)
{
	switch (target)
	{
	case GL_ARRAY_BUFFER:
		return GL_ARRAY_BUFFER_BINDING;
	case GL_ELEMENT_ARRAY_BUFFER:
		return GL_ELEMENT_ARRAY_BUFFER_BINDING;
	case GL_PIXEL_PACK_BUFFER:
		return GL_PIXEL_PACK_BUFFER_BINDING;
	case GL_PIXEL_UNPACK_BUFFER:
		return GL_PIXEL_UNPACK_BUFFER_BINDING;
	case GL_UNIFORM_BUFFER:
		return GL_UNIFORM_BUFFER_BINDING;
	case GL_TEXTURE_BUFFER:
		return GL_TEXTURE_BUFFER_BINDING; // GL_TEXTURE_BINDING_BUFFER does not seem to work
	case GL_TRANSFORM_FEEDBACK_BUFFER:
		return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
	case GL_COPY_READ_BUFFER:
		return GL_COPY_READ_BUFFER_BINDING;
	case GL_COPY_WRITE_BUFFER:
		return GL_COPY_WRITE_BUFFER_BINDING;
	case GL_DRAW_INDIRECT_BUFFER:
		return GL_DRAW_INDIRECT_BUFFER_BINDING;
	case GL_SHADER_STORAGE_BUFFER:
		return GL_SHADER_STORAGE_BUFFER_BINDING;
	case GL_DISPATCH_INDIRECT_BUFFER:
		return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
	case GL_QUERY_BUFFER:
		return GL_QUERY_BUFFER_BINDING;
	case GL_ATOMIC_COUNTER_BUFFER:
		return GL_ATOMIC_COUNTER_BUFFER_BINDING;
	case GL_TEXTURE_1D:
		return GL_TEXTURE_BINDING_1D;
	case GL_TEXTURE_1D_ARRAY:
		return GL_TEXTURE_BINDING_1D_ARRAY;
	case GL_TEXTURE_2D:
		return GL_TEXTURE_BINDING_2D;
	case GL_TEXTURE_2D_ARRAY:
		return GL_TEXTURE_BINDING_2D_ARRAY;
	case GL_TEXTURE_2D_MULTISAMPLE:
		return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
	case GL_TEXTURE_3D:
		return GL_TEXTURE_BINDING_3D;
	case GL_TEXTURE_CUBE_MAP:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		return GL_TEXTURE_BINDING_CUBE_MAP;
	case GL_TEXTURE_CUBE_MAP_ARRAY:
		return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
	case GL_TEXTURE_RECTANGLE:
		return GL_TEXTURE_BINDING_RECTANGLE;
	case GL_RENDERBUFFER:
		return GL_RENDERBUFFER_BINDING;
	case GL_FRAMEBUFFER:
		return GL_FRAMEBUFFER_BINDING;
	case GL_READ_FRAMEBUFFER:
		return GL_READ_FRAMEBUFFER_BINDING;
	case GL_DRAW_FRAMEBUFFER:
		return GL_DRAW_FRAMEBUFFER_BINDING;
	default:
		assert(false);
		return GL_NONE;
	}
}

auto   reshade::opengl::convert_logic_op(GLenum value) -> api::logic_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_CLEAR:
		return api::logic_op::clear;
	case GL_AND:
		return api::logic_op::bitwise_and;
	case GL_AND_REVERSE:
		return api::logic_op::bitwise_and_reverse;
	case GL_COPY:
		return api::logic_op::copy;
	case GL_AND_INVERTED:
		return api::logic_op::bitwise_and_inverted;
	case GL_NOOP:
		return api::logic_op::noop;
	case GL_XOR:
		return api::logic_op::bitwise_xor;
	case GL_OR:
		return api::logic_op::bitwise_or;
	case GL_NOR:
		return api::logic_op::bitwise_nor;
	case GL_EQUIV:
		return api::logic_op::equivalent;
	case GL_INVERT:
		return api::logic_op::invert;
	case GL_OR_REVERSE:
		return api::logic_op::bitwise_or_reverse;
	case GL_COPY_INVERTED:
		return api::logic_op::copy_inverted;
	case GL_OR_INVERTED:
		return api::logic_op::bitwise_or_inverted;
	case GL_NAND:
		return api::logic_op::bitwise_nand;
	case GL_SET:
		return api::logic_op::set;
	}
}
GLenum reshade::opengl::convert_logic_op(api::logic_op value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::logic_op::clear:
		return GL_CLEAR;
	case api::logic_op::bitwise_and:
		return GL_AND;
	case api::logic_op::bitwise_and_reverse:
		return GL_AND_REVERSE;
	case api::logic_op::copy:
		return GL_COPY;
	case api::logic_op::bitwise_and_inverted:
		return GL_AND_INVERTED;
	case api::logic_op::noop:
		return GL_NOOP;
	case api::logic_op::bitwise_xor:
		return GL_XOR;
	case api::logic_op::bitwise_or:
		return GL_OR;
	case api::logic_op::bitwise_nor:
		return GL_NOR;
	case api::logic_op::equivalent:
		return GL_EQUIV;
	case api::logic_op::invert:
		return GL_INVERT;
	case api::logic_op::bitwise_or_reverse:
		return GL_OR_REVERSE;
	case api::logic_op::copy_inverted:
		return GL_COPY_INVERTED;
	case api::logic_op::bitwise_or_inverted:
		return GL_OR_INVERTED;
	case api::logic_op::bitwise_nand:
		return GL_NAND;
	case api::logic_op::set:
		return GL_SET;
	}
}
auto   reshade::opengl::convert_blend_op(GLenum value) -> api::blend_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_FUNC_ADD:
		return api::blend_op::add;
	case GL_FUNC_SUBTRACT:
		return api::blend_op::subtract;
	case GL_FUNC_REVERSE_SUBTRACT:
		return api::blend_op::reverse_subtract;
	case GL_MIN:
		return api::blend_op::min;
	case GL_MAX:
		return api::blend_op::max;
	}
}
GLenum reshade::opengl::convert_blend_op(api::blend_op value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_op::add:
		return GL_FUNC_ADD;
	case api::blend_op::subtract:
		return GL_FUNC_SUBTRACT;
	case api::blend_op::reverse_subtract:
		return GL_FUNC_REVERSE_SUBTRACT;
	case api::blend_op::min:
		return GL_MIN;
	case api::blend_op::max:
		return GL_MAX;
	}
}
auto   reshade::opengl::convert_blend_factor(GLenum value) -> api::blend_factor
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_ZERO:
		return api::blend_factor::zero;
	case GL_ONE:
		return api::blend_factor::one;
	case GL_SRC_COLOR:
		return api::blend_factor::source_color;
	case GL_ONE_MINUS_SRC_COLOR:
		return api::blend_factor::one_minus_source_color;
	case GL_DST_COLOR:
		return api::blend_factor::dest_color;
	case GL_ONE_MINUS_DST_COLOR:
		return api::blend_factor::one_minus_dest_color;
	case GL_SRC_ALPHA:
		return api::blend_factor::source_alpha;
	case GL_ONE_MINUS_SRC_ALPHA:
		return api::blend_factor::one_minus_source_alpha;
	case GL_DST_ALPHA:
		return api::blend_factor::dest_alpha;
	case GL_ONE_MINUS_DST_ALPHA:
		return api::blend_factor::one_minus_dest_alpha;
	case GL_CONSTANT_COLOR:
		return api::blend_factor::constant_color;
	case GL_ONE_MINUS_CONSTANT_COLOR:
		return api::blend_factor::one_minus_constant_color;
	case GL_CONSTANT_ALPHA:
		return api::blend_factor::constant_alpha;
	case GL_ONE_MINUS_CONSTANT_ALPHA:
		return api::blend_factor::one_minus_constant_alpha;
	case GL_SRC_ALPHA_SATURATE:
		return api::blend_factor::source_alpha_saturate;
	case GL_SRC1_COLOR:
		return api::blend_factor::source1_color;
	case GL_ONE_MINUS_SRC1_COLOR:
		return api::blend_factor::one_minus_source1_color;
	case GL_SRC1_ALPHA:
		return api::blend_factor::source1_alpha;
	case GL_ONE_MINUS_SRC1_ALPHA:
		return api::blend_factor::one_minus_source1_alpha;
	}
}
GLenum reshade::opengl::convert_blend_factor(api::blend_factor value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::zero:
		return GL_ZERO;
	case api::blend_factor::one:
		return GL_ONE;
	case api::blend_factor::source_color:
		return GL_SRC_COLOR;
	case api::blend_factor::one_minus_source_color:
		return GL_ONE_MINUS_SRC_COLOR;
	case api::blend_factor::dest_color:
		return GL_DST_COLOR;
	case api::blend_factor::one_minus_dest_color:
		return GL_ONE_MINUS_DST_COLOR;
	case api::blend_factor::source_alpha:
		return GL_SRC_ALPHA;
	case api::blend_factor::one_minus_source_alpha:
		return GL_ONE_MINUS_SRC_ALPHA;
	case api::blend_factor::dest_alpha:
		return GL_DST_ALPHA;
	case api::blend_factor::one_minus_dest_alpha:
		return GL_ONE_MINUS_DST_ALPHA;
	case api::blend_factor::constant_color:
		return GL_CONSTANT_COLOR;
	case api::blend_factor::one_minus_constant_color:
		return GL_ONE_MINUS_CONSTANT_COLOR;
	case api::blend_factor::constant_alpha:
		return GL_CONSTANT_ALPHA;
	case api::blend_factor::one_minus_constant_alpha:
		return GL_ONE_MINUS_CONSTANT_ALPHA;
	case api::blend_factor::source_alpha_saturate:
		return GL_SRC_ALPHA_SATURATE;
	case api::blend_factor::source1_color:
		return GL_SRC1_COLOR;
	case api::blend_factor::one_minus_source1_color:
		return GL_ONE_MINUS_SRC1_COLOR;
	case api::blend_factor::source1_alpha:
		return GL_SRC1_ALPHA;
	case api::blend_factor::one_minus_source1_alpha:
		return GL_ONE_MINUS_SRC1_ALPHA;
	}
}
auto   reshade::opengl::convert_fill_mode(GLenum value) -> api::fill_mode
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_FILL:
		return api::fill_mode::solid;
	case GL_LINE:
		return api::fill_mode::wireframe;
	case GL_POINT:
		return api::fill_mode::point;
	}
}
GLenum reshade::opengl::convert_fill_mode(api::fill_mode value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::fill_mode::solid:
		return GL_FILL;
	case api::fill_mode::wireframe:
		return GL_LINE;
	case api::fill_mode::point:
		return GL_POINT;
	}
}
auto   reshade::opengl::convert_cull_mode(GLenum value) -> api::cull_mode
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_NONE:
		return api::cull_mode::none;
	case GL_FRONT:
		return api::cull_mode::front;
	case GL_BACK:
		return api::cull_mode::back;
	case GL_FRONT_AND_BACK:
		return api::cull_mode::front_and_back;
	}
}
GLenum reshade::opengl::convert_cull_mode(api::cull_mode value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::cull_mode::none:
		return GL_NONE;
	case api::cull_mode::front:
		return GL_FRONT;
	case api::cull_mode::back:
		return GL_BACK;
	case api::cull_mode::front_and_back:
		return GL_FRONT_AND_BACK;
	}
}
auto   reshade::opengl::convert_compare_op(GLenum value) -> api::compare_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_NEVER:
		return api::compare_op::never;
	case GL_LESS:
		return api::compare_op::less;
	case GL_EQUAL:
		return api::compare_op::equal;
	case GL_LEQUAL:
		return api::compare_op::less_equal;
	case GL_GREATER:
		return api::compare_op::greater;
	case GL_NOTEQUAL:
		return api::compare_op::not_equal;
	case GL_GEQUAL:
		return api::compare_op::greater_equal;
	case GL_ALWAYS:
		return api::compare_op::always;
	}
}
GLenum reshade::opengl::convert_compare_op(api::compare_op value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::compare_op::never:
		return GL_NEVER;
	case api::compare_op::less:
		return GL_LESS;
	case api::compare_op::equal:
		return GL_EQUAL;
	case api::compare_op::less_equal:
		return GL_LEQUAL;
	case api::compare_op::greater:
		return GL_GREATER;
	case api::compare_op::not_equal:
		return GL_NOTEQUAL;
	case api::compare_op::greater_equal:
		return GL_GEQUAL;
	case api::compare_op::always:
		return GL_ALWAYS;
	}
}
auto   reshade::opengl::convert_stencil_op(GLenum value) -> api::stencil_op
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_KEEP:
		return api::stencil_op::keep;
	case GL_ZERO:
		return api::stencil_op::zero;
	case GL_REPLACE:
		return api::stencil_op::replace;
	case GL_INCR:
		return api::stencil_op::increment_saturate;
	case GL_DECR:
		return api::stencil_op::decrement_saturate;
	case GL_INVERT:
		return api::stencil_op::invert;
	case GL_INCR_WRAP:
		return api::stencil_op::increment;
	case GL_DECR_WRAP:
		return api::stencil_op::decrement;
	}
}
GLenum reshade::opengl::convert_stencil_op(api::stencil_op value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::stencil_op::keep:
		return GL_KEEP;
	case api::stencil_op::zero:
		return GL_ZERO;
	case api::stencil_op::replace:
		return GL_REPLACE;
	case api::stencil_op::increment_saturate:
		return GL_INCR;
	case api::stencil_op::decrement_saturate:
		return GL_DECR;
	case api::stencil_op::invert:
		return GL_INVERT;
	case api::stencil_op::increment:
		return GL_INCR_WRAP;
	case api::stencil_op::decrement:
		return GL_DECR_WRAP;
	}
}
auto   reshade::opengl::convert_primitive_topology(GLenum value) -> api::primitive_topology
{
	switch (value)
	{
	default:
		assert(false);
		return api::primitive_topology::undefined;
	case GL_POINTS:
		return api::primitive_topology::point_list;
	case GL_LINES:
		return api::primitive_topology::line_list;
	case GL_LINE_LOOP:
	case GL_LINE_STRIP:
		return api::primitive_topology::line_strip;
	case GL_TRIANGLES:
		return api::primitive_topology::triangle_list;
	case GL_TRIANGLE_STRIP:
		return api::primitive_topology::triangle_strip;
	case GL_TRIANGLE_FAN:
		return api::primitive_topology::triangle_fan;
	case GL_LINES_ADJACENCY:
		return api::primitive_topology::line_list_adj;
	case GL_LINE_STRIP_ADJACENCY:
		return api::primitive_topology::line_strip_adj;
	case GL_TRIANGLES_ADJACENCY:
		return api::primitive_topology::triangle_list_adj;
	case GL_TRIANGLE_STRIP_ADJACENCY:
		return api::primitive_topology::triangle_strip_adj;
	case GL_QUADS:
		return api::primitive_topology::quad_list;
	case 0x0008 /* GL_QUAD_STRIP */:
		return api::primitive_topology::quad_strip;
	case GL_PATCHES:
		// This needs to be adjusted externally based on 'GL_PATCH_VERTICES'
		return api::primitive_topology::patch_list_01_cp;
	}
}
GLenum reshade::opengl::convert_primitive_topology(api::primitive_topology value)
{
	switch (value)
	{
	case api::primitive_topology::point_list:
		return GL_POINTS;
	case api::primitive_topology::line_list:
		return GL_LINES;
	case api::primitive_topology::line_strip:
		return GL_LINE_STRIP;
	case api::primitive_topology::triangle_list:
		return GL_TRIANGLES;
	case api::primitive_topology::triangle_strip:
		return GL_TRIANGLE_STRIP;
	case api::primitive_topology::triangle_fan:
		return GL_TRIANGLE_FAN;
	case api::primitive_topology::quad_list:
		return GL_QUADS;
	case api::primitive_topology::quad_strip:
		return 0x0008 /* GL_QUAD_STRIP */;
	case api::primitive_topology::line_list_adj:
		return GL_LINES_ADJACENCY;
	case api::primitive_topology::line_strip_adj:
		return GL_LINE_STRIP_ADJACENCY;
	case api::primitive_topology::triangle_list_adj:
		return GL_TRIANGLES_ADJACENCY;
	case api::primitive_topology::triangle_strip_adj:
		return GL_TRIANGLE_STRIP_ADJACENCY;
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
		// Also need to adjust 'GL_PATCH_VERTICES' externally
		return GL_PATCHES;
	default:
		assert(false);
		return GL_NONE;
	}
}
GLenum reshade::opengl::convert_query_type(api::query_type value)
{
	switch (value)
	{
	case api::query_type::occlusion:
	case api::query_type::binary_occlusion:
		return GL_SAMPLES_PASSED;
	case api::query_type::timestamp:
		return GL_TIMESTAMP;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		return GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
	default:
		assert(false);
		return GL_NONE;
	}
}
GLenum reshade::opengl::convert_shader_type(api::shader_stage type)
{
	switch (type)
	{
	case api::shader_stage::vertex:
		return GL_VERTEX_SHADER;
	case api::shader_stage::hull:
		return GL_TESS_CONTROL_SHADER;
	case api::shader_stage::domain:
		return GL_TESS_EVALUATION_SHADER;
	case api::shader_stage::geometry:
		return GL_GEOMETRY_SHADER;
	case api::shader_stage::pixel:
		return GL_FRAGMENT_SHADER;
	case api::shader_stage::compute:
		return GL_COMPUTE_SHADER;
	default:
		assert(false);
		return GL_NONE;
	}
}
