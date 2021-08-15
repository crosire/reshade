/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_type_convert.hpp"

auto reshade::opengl::convert_format(api::format format) -> GLenum
{
	switch (format)
	{
	default:
	case api::format::unknown:
		break;
	case api::format::r1_unorm:
		break; // Unsupported
	case api::format::r8_uint:
		return GL_R8UI;
	case api::format::r8_sint:
		return GL_R8I;
	case api::format::r8_typeless:
	case api::format::r8_unorm:
	case api::format::a8_unorm:
		return GL_R8;
	case api::format::r8_snorm:
		return GL_R8_SNORM;
	case api::format::r8g8_uint:
		return GL_RG8UI;
	case api::format::r8g8_sint:
		return GL_RG8I;
	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
		return GL_RG8;
	case api::format::r8g8_snorm:
		return GL_RG8_SNORM;
	case api::format::r8g8b8a8_uint:
		return GL_RGBA8UI;
	case api::format::r8g8b8a8_sint:
		return GL_RGBA8I;
	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
		return GL_RGBA8;
	case api::format::r8g8b8a8_unorm_srgb:
		return GL_SRGB8_ALPHA8;
	case api::format::r8g8b8a8_snorm:
		return GL_RGBA8_SNORM;
	case api::format::r8g8b8x8_typeless:
	case api::format::r8g8b8x8_unorm:
		return GL_RGB8;
	case api::format::r8g8b8x8_unorm_srgb:
		return GL_SRGB8;
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
	case api::format::b8g8r8a8_unorm_srgb:
	case api::format::b8g8r8x8_typeless:
	case api::format::b8g8r8x8_unorm:
	case api::format::b8g8r8x8_unorm_srgb:
		break; // Unsupported
	case api::format::r10g10b10a2_uint:
		return GL_RGB10_A2UI;
	case api::format::r10g10b10a2_typeless:
	case api::format::r10g10b10a2_unorm:
		return GL_RGB10_A2;
	case api::format::r10g10b10a2_xr_bias:
	case api::format::b10g10r10a2_typeless:
	case api::format::b10g10r10a2_uint:
	case api::format::b10g10r10a2_unorm:
		break; // Unsupported
	case api::format::r16_uint:
		return GL_R16UI;
	case api::format::r16_sint:
		return GL_R16I;
	case api::format::r16_typeless:
	case api::format::r16_float:
		return GL_R16F;
	case api::format::r16_unorm:
		return GL_R16;
	case api::format::r16_snorm:
		return GL_R16_SNORM;
	case api::format::r16g16_uint:
		return GL_RG16UI;
	case api::format::r16g16_sint:
		return GL_RG16I;
	case api::format::r16g16_typeless:
	case api::format::r16g16_float:
		return GL_RG16F;
	case api::format::r16g16_unorm:
		return GL_RG16;
	case api::format::r16g16_snorm:
		return GL_RG16_SNORM;
	case api::format::r16g16b16a16_uint:
		return GL_RGBA16UI;
	case api::format::r16g16b16a16_sint:
		return GL_RGBA16I;
	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_float:
		return GL_RGBA16F;
	case api::format::r16g16b16a16_unorm:
		return GL_RGBA16;
	case api::format::r16g16b16a16_snorm:
		return GL_RGBA16_SNORM;
	case api::format::r32_uint:
		return GL_R32UI;
	case api::format::r32_sint:
		return GL_R32I;
	case api::format::r32_typeless:
	case api::format::r32_float:
		return GL_R32F;
	case api::format::r32g32_uint:
		return GL_RG32UI;
	case api::format::r32g32_sint:
		return GL_RG32I;
	case api::format::r32g32_typeless:
	case api::format::r32g32_float:
		return GL_RG32F;
	case api::format::r32g32b32_uint:
		return GL_RGB32UI;
	case api::format::r32g32b32_sint:
		return GL_RGB32I;
	case api::format::r32g32b32_typeless:
	case api::format::r32g32b32_float:
		return GL_RGB32F;
	case api::format::r32g32b32a32_uint:
		return GL_RGBA32UI;
	case api::format::r32g32b32a32_sint:
		return GL_RGBA32I;
	case api::format::r32g32b32a32_typeless:
	case api::format::r32g32b32a32_float:
		return GL_RGBA32F;
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
		return GL_RGBA4;
	case api::format::s8_uint:
		return GL_STENCIL_INDEX8;
	case api::format::d16_unorm:
		return GL_DEPTH_COMPONENT16;
	case api::format::d16_unorm_s8_uint:
		break; // Unsupported
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
auto reshade::opengl::convert_format(GLenum internal_format) -> api::format
{
	switch (internal_format)
	{
	default:
		return api::format::unknown;
	case GL_R8UI:
		return api::format::r8_uint;
	case GL_R8I:
		return api::format::r8_sint;
	case GL_R8:
		return api::format::r8_unorm;
	case GL_R8_SNORM:
		return api::format::r8_snorm;
	case GL_RG8UI:
		return api::format::r8g8_uint;
	case GL_RG8I:
		return api::format::r8g8_sint;
	case GL_RG8:
		return api::format::r8g8_unorm;
	case GL_RG8_SNORM:
		return api::format::r8g8_snorm;
	case GL_RGBA8UI:
		return api::format::r8g8b8a8_uint;
	case GL_RGBA8I:
		return api::format::r8g8b8a8_sint;
	case GL_RGBA8:
		return api::format::r8g8b8a8_unorm;
	case GL_SRGB8_ALPHA8:
		return api::format::r8g8b8a8_unorm_srgb;
	case GL_RGBA8_SNORM:
		return api::format::r8g8b8a8_snorm;
	case GL_RGB8:
		return api::format::r8g8b8x8_unorm;
	case GL_SRGB8:
		return api::format::r8g8b8x8_unorm_srgb;
	case GL_RGB10_A2UI:
		return api::format::r10g10b10a2_uint;
	case GL_RGB10_A2:
		return api::format::r10g10b10a2_unorm;
	case GL_R16UI:
		return api::format::r16_uint;
	case GL_R16I:
		return api::format::r16_sint;
	case GL_R16F:
		return api::format::r16_float;
	case GL_R16:
		return api::format::r16_unorm;
	case GL_R16_SNORM:
		return api::format::r16_snorm;
	case GL_RG16UI:
		return api::format::r16g16_uint;
	case GL_RG16I:
		return api::format::r16g16_sint;
	case GL_RG16F:
		return api::format::r16g16_float;
	case GL_RG16:
		return api::format::r16g16_unorm;
	case GL_RG16_SNORM:
		return api::format::r16g16_snorm;
	case GL_RGBA16UI:
		return api::format::r16g16b16a16_uint;
	case GL_RGBA16I:
		return api::format::r16g16b16a16_sint;
	case GL_RGBA16F:
		return api::format::r16g16b16a16_float;
	case GL_RGBA16:
		return api::format::r16g16b16a16_unorm;
	case GL_RGBA16_SNORM:
		return api::format::r16g16b16a16_snorm;
	case GL_R32UI:
		return api::format::r32_uint;
	case GL_R32I:
		return api::format::r32_sint;
	case GL_R32F:
		return api::format::r32_float;
	case GL_RG32UI:
		return api::format::r32g32_uint;
	case GL_RG32I:
		return api::format::r32g32_sint;
	case GL_RG32F:
		return api::format::r32g32_float;
	case GL_RGB32UI:
		return api::format::r32g32b32_uint;
	case GL_RGB32I:
		return api::format::r32g32b32_sint;
	case GL_RGB32F:
		return api::format::r32g32b32_float;
	case GL_RGBA32UI:
		return api::format::r32g32b32a32_uint;
	case GL_RGBA32I:
		return api::format::r32g32b32a32_sint;
	case GL_RGBA32F:
		return api::format::r32g32b32a32_float;
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
	case GL_STENCIL_INDEX:
	case GL_STENCIL_INDEX8:
		return api::format::s8_uint;
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
		return api::format::d16_unorm;
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_STENCIL:
	case GL_DEPTH24_STENCIL8:
		return api::format::d24_unorm_s8_uint;
	case GL_DEPTH_COMPONENT32:
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
	}
}
auto reshade::opengl::convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum
{
	size = 0;
	normalized = GL_FALSE;

	switch (format)
	{
	case api::format::r8g8b8a8_unorm:
		normalized = GL_TRUE;
		[[fallthrough]];
	case api::format::r8g8b8a8_uint:
		size = 4;
		return GL_UNSIGNED_BYTE;
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
	case api::format::r16_float:
		size = 1;
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
	case api::format::r16g16_float:
		size = 2;
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
	case api::format::r16g16b16a16_float:
		size = 4;
		return GL_HALF_FLOAT;
	case api::format::r32_uint:
		size = 1;
		return GL_UNSIGNED_INT;
	case api::format::r32_sint:
		size = 1;
		return GL_INT;
	case api::format::r32_float:
		size = 1;
		return GL_FLOAT;
	case api::format::r32g32_uint:
		size = 2;
		return GL_UNSIGNED_INT;
	case api::format::r32g32_sint:
		size = 2;
		return GL_INT;
	case api::format::r32g32_float:
		size = 2;
		return GL_FLOAT;
	case api::format::r32g32b32_uint:
		size = 3;
		return GL_UNSIGNED_INT;
	case api::format::r32g32b32_sint:
		size = 3;
		return GL_INT;
	case api::format::r32g32b32_float:
		size = 3;
		return GL_FLOAT;
	case api::format::r32g32b32a32_uint:
		size = 4;
		return GL_UNSIGNED_INT;
	case api::format::r32g32b32a32_sint:
		size = 4;
		return GL_INT;
	case api::format::r32g32b32a32_float:
		size = 4;
		return GL_FLOAT;
	}

	return GL_NONE;
}
auto reshade::opengl::convert_upload_format(GLenum internal_format, GLenum &type) -> GLenum
{
	switch (internal_format)
	{
	case GL_R8UI:
	case GL_R8:
		type = GL_UNSIGNED_BYTE;
		return GL_RED;
	case GL_R8I:
	case GL_R8_SNORM:
		type = GL_BYTE;
		return GL_RED;
	case GL_RG8UI:
	case GL_RG8:
		type = GL_UNSIGNED_BYTE;
		return GL_RG;
	case GL_RG8I:
	case GL_RG8_SNORM:
		type = GL_BYTE;
		return GL_RG;
	case GL_RGBA8UI:
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
		type = GL_UNSIGNED_BYTE;
		return GL_RGBA;
	case GL_RGBA8I:
	case GL_RGBA8_SNORM:
		type = GL_BYTE;
		return GL_RGBA;
	case GL_RGB10_A2UI:
	case GL_RGB10_A2:
		type = GL_UNSIGNED_INT_10_10_10_2;
		return GL_RGBA;
	case GL_R16UI:
	case GL_R16F:
	case GL_R16:
		type = GL_UNSIGNED_SHORT;
		return GL_RED;
	case GL_R16I:
	case GL_R16_SNORM:
		type = GL_SHORT;
		return GL_RED;
	case GL_RG16UI:
	case GL_RG16F:
	case GL_RG16:
		type = GL_UNSIGNED_SHORT;
		return GL_RG;
	case GL_RG16I:
	case GL_RG16_SNORM:
		type = GL_SHORT;
		return GL_RG;
	case GL_RGBA16UI:
	case GL_RGBA16F:
	case GL_RGBA16:
		type = GL_UNSIGNED_SHORT;
		return GL_RGBA;
	case GL_RGBA16I:
	case GL_RGBA16_SNORM:
		type = GL_SHORT;
		return GL_RGBA;
	case GL_R32UI:
		type = GL_UNSIGNED_INT;
		return GL_RED;
	case GL_R32I:
		type = GL_INT;
		return GL_RED;
	case GL_R32F:
		type = GL_FLOAT;
		return GL_RED;
	case GL_RG32UI:
		type = GL_UNSIGNED_INT;
		return GL_RG;
	case GL_RG32I:
		type = GL_INT;
		return GL_RG;
	case GL_RG32F:
		type = GL_FLOAT;
		return GL_RG;
	case GL_RGB32UI:
		type = GL_UNSIGNED_INT;
		return GL_RGB;
	case GL_RGB32I:
		type = GL_INT;
		return GL_RGB;
	case GL_RGB32F:
		type = GL_FLOAT;
		return GL_RGB;
	case GL_RGBA32UI:
		type = GL_UNSIGNED_INT;
		return GL_RGBA;
	case GL_RGBA32I:
		type = GL_INT;
		return GL_RGBA;
	case GL_RGBA32F:
		type = GL_FLOAT;
		return GL_RGBA;
	case GL_RGB9_E5:
		type = GL_UNSIGNED_INT_5_9_9_9_REV;
		return GL_RGBA;
	case GL_R11F_G11F_B10F:
		type = GL_UNSIGNED_INT_10F_11F_11F_REV;
		return GL_RGB;
	case GL_RGB565:
		type = GL_UNSIGNED_SHORT_5_6_5;
		return GL_RGB;
	case GL_RGB5_A1:
		type = GL_UNSIGNED_SHORT_5_5_5_1;
		return GL_RGBA;
	case GL_RGBA4:
		type = GL_UNSIGNED_SHORT_4_4_4_4;
		return GL_RGBA;
	case GL_STENCIL_INDEX:
	case GL_STENCIL_INDEX8:
		type = GL_UNSIGNED_BYTE;
		return GL_STENCIL_INDEX;
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
		type = GL_UNSIGNED_SHORT;
		return GL_DEPTH_COMPONENT;
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_STENCIL:
	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH_COMPONENT32:
		type = GL_UNSIGNED_INT;
		return GL_DEPTH_COMPONENT;
	case GL_DEPTH_COMPONENT32F:
	case GL_DEPTH_COMPONENT32F_NV:
		type = GL_FLOAT;
		return GL_DEPTH_COMPONENT;
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
	case 0x8C4D /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT */:
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
	case 0x8C4E /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT */:
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
	case 0x8C4F /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT */:
	case GL_COMPRESSED_RED_RGTC1:
	case GL_COMPRESSED_SIGNED_RED_RGTC1:
	case GL_COMPRESSED_RG_RGTC2:
	case GL_COMPRESSED_SIGNED_RG_RGTC2:
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
	case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
		type = GL_COMPRESSED_TEXTURE_FORMATS;
		return internal_format;
	default:
		assert(false);
		break;
	}

	return type = GL_NONE;
}

bool reshade::opengl::is_depth_stencil_format(GLenum internal_format, GLenum usage)
{
	switch (internal_format)
	{
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
	case GL_DEPTH_COMPONENT32F:
		return usage == GL_DEPTH_STENCIL || usage == GL_DEPTH;
	case GL_DEPTH_STENCIL:
	case GL_DEPTH24_STENCIL8:
	case GL_DEPTH32F_STENCIL8:
		return usage == GL_DEPTH_STENCIL || usage == GL_DEPTH || usage == GL_STENCIL;
	case GL_STENCIL:
	case GL_STENCIL_INDEX1:
	case GL_STENCIL_INDEX4:
	case GL_STENCIL_INDEX8:
	case GL_STENCIL_INDEX16:
		return usage == GL_DEPTH_STENCIL || usage == GL_STENCIL;
	default:
		return false;
	}
}

void reshade::opengl::convert_memory_heap_to_usage(const api::resource_desc &desc, GLenum &usage)
{
	switch (desc.heap)
	{
	case api::memory_heap::gpu_only:
		usage = GL_STATIC_DRAW;
		break;
	case api::memory_heap::cpu_to_gpu:
		usage = (desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic ? GL_DYNAMIC_DRAW : GL_STREAM_DRAW;
		break;
	case api::memory_heap::gpu_to_cpu:
		usage = (desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic ? GL_DYNAMIC_READ : GL_STREAM_READ;
		break;
	}
}
void reshade::opengl::convert_memory_heap_to_flags(const api::resource_desc &desc, GLbitfield &flags)
{
	switch (desc.heap)
	{
	case api::memory_heap::cpu_to_gpu:
		flags |= GL_MAP_WRITE_BIT;
		break;
	case api::memory_heap::gpu_to_cpu:
		flags |= GL_MAP_READ_BIT;
		break;
	case api::memory_heap::cpu_only:
		flags |= GL_CLIENT_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
		break;
	}

	if ((desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic)
		flags |= GL_DYNAMIC_STORAGE_BIT;
}
void reshade::opengl::convert_memory_heap_from_usage(api::resource_desc &desc, GLenum usage)
{
	switch (usage)
	{
	case GL_STATIC_DRAW:
		desc.heap = api::memory_heap::gpu_only;
		break;
	case GL_STREAM_DRAW:
		desc.heap = api::memory_heap::cpu_to_gpu;
		break;
	case GL_DYNAMIC_DRAW:
		desc.heap = api::memory_heap::cpu_to_gpu;
		desc.flags |= api::resource_flags::dynamic;
		break;
	case GL_STREAM_READ:
	case GL_STATIC_READ:
		desc.heap = api::memory_heap::gpu_to_cpu;
		break;
	case GL_DYNAMIC_READ:
		desc.heap = api::memory_heap::gpu_to_cpu;
		desc.flags |= api::resource_flags::dynamic;
		break;
	}
}
void reshade::opengl::convert_memory_heap_from_flags(api::resource_desc &desc, GLbitfield flags)
{
	if ((flags & GL_MAP_WRITE_BIT) != 0)
		desc.heap = api::memory_heap::cpu_to_gpu;
	else if ((flags & GL_MAP_READ_BIT) != 0)
		desc.heap = api::memory_heap::gpu_to_cpu;
	else if ((flags & GL_CLIENT_STORAGE_BIT) != 0)
		desc.heap = api::memory_heap::cpu_only;

	if ((flags & GL_DYNAMIC_STORAGE_BIT) != 0)
		desc.flags |= api::resource_flags::dynamic;
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
reshade::api::resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizeiptr buffer_size)
{
	api::resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.buffer.size = buffer_size;
	desc.heap = api::memory_heap::gpu_only;
	desc.usage = api::resource_usage::shader_resource | api::resource_usage::copy_dest | api::resource_usage::copy_source;
	return desc;
}
reshade::api::resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height, GLsizei depth)
{
	api::resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.texture.width = width;
	desc.texture.height = height;
	assert(depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	desc.texture.format = convert_format(internal_format);
	desc.texture.samples = static_cast<uint16_t>(samples);
	desc.heap = api::memory_heap::gpu_only;

	desc.usage = api::resource_usage::copy_dest | api::resource_usage::copy_source | api::resource_usage::resolve_dest;
	if (desc.texture.samples >= 2)
		desc.usage = api::resource_usage::resolve_source;

	if (is_depth_stencil_format(internal_format))
		desc.usage |= api::resource_usage::depth_stencil;
	if (desc.type == api::resource_type::texture_1d || desc.type == api::resource_type::texture_2d || desc.type == api::resource_type::surface)
		desc.usage |= api::resource_usage::render_target;

	if (desc.type != api::resource_type::surface)
		desc.usage |= api::resource_usage::shader_resource;

	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
		target == GL_PROXY_TEXTURE_CUBE_MAP || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY || (
		target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
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
	assert(convert_resource_view_type(target) == api::resource_view_type::buffer);
	return api::resource_view_desc(convert_format(internal_format), offset, size);
}
reshade::api::resource_view_desc reshade::opengl::convert_resource_view_desc(GLenum target, GLenum internal_format, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
	return api::resource_view_desc(convert_resource_view_type(target), convert_format(internal_format), minlevel, numlevels, minlayer, numlayers);
}

GLuint reshade::opengl::get_index_type_size(GLenum index_type)
{
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
		return GL_TEXTURE_BINDING_BUFFER;
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
		return api::blend_op::rev_subtract;
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
	case api::blend_op::rev_subtract:
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
		return api::blend_factor::src_color;
	case GL_ONE_MINUS_SRC_COLOR:
		return api::blend_factor::inv_src_color;
	case GL_DST_COLOR:
		return api::blend_factor::dst_color;
	case GL_ONE_MINUS_DST_COLOR:
		return api::blend_factor::inv_dst_color;
	case GL_SRC_ALPHA:
		return api::blend_factor::src_alpha;
	case GL_ONE_MINUS_SRC_ALPHA:
		return api::blend_factor::inv_src_alpha;
	case GL_DST_ALPHA:
		return api::blend_factor::dst_alpha;
	case GL_ONE_MINUS_DST_ALPHA:
		return api::blend_factor::inv_dst_alpha;
	case GL_CONSTANT_COLOR:
		return api::blend_factor::constant_color;
	case GL_ONE_MINUS_CONSTANT_COLOR:
		return api::blend_factor::inv_constant_color;
	case GL_CONSTANT_ALPHA:
		return api::blend_factor::constant_alpha;
	case GL_ONE_MINUS_CONSTANT_ALPHA:
		return api::blend_factor::inv_constant_alpha;
	case GL_SRC_ALPHA_SATURATE:
		return api::blend_factor::src_alpha_sat;
	case GL_SRC1_COLOR:
		return api::blend_factor::src1_color;
	case GL_ONE_MINUS_SRC1_COLOR:
		return api::blend_factor::inv_src1_color;
	case GL_SRC1_ALPHA:
		return api::blend_factor::src1_alpha;
	case GL_ONE_MINUS_SRC1_ALPHA:
		return api::blend_factor::inv_src1_alpha;
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
	case api::blend_factor::src_color:
		return GL_SRC_COLOR;
	case api::blend_factor::inv_src_color:
		return GL_ONE_MINUS_SRC_COLOR;
	case api::blend_factor::dst_color:
		return GL_DST_COLOR;
	case api::blend_factor::inv_dst_color:
		return GL_ONE_MINUS_DST_COLOR;
	case api::blend_factor::src_alpha:
		return GL_SRC_ALPHA;
	case api::blend_factor::inv_src_alpha:
		return GL_ONE_MINUS_SRC_ALPHA;
	case api::blend_factor::dst_alpha:
		return GL_DST_ALPHA;
	case api::blend_factor::inv_dst_alpha:
		return GL_ONE_MINUS_DST_ALPHA;
	case api::blend_factor::constant_color:
		return GL_CONSTANT_COLOR;
	case api::blend_factor::inv_constant_color:
		return GL_ONE_MINUS_CONSTANT_COLOR;
	case api::blend_factor::constant_alpha:
		return GL_CONSTANT_ALPHA;
	case api::blend_factor::inv_constant_alpha:
		return GL_ONE_MINUS_CONSTANT_ALPHA;
	case api::blend_factor::src_alpha_sat:
		return GL_SRC_ALPHA_SATURATE;
	case api::blend_factor::src1_color:
		return GL_SRC1_COLOR;
	case api::blend_factor::inv_src1_color:
		return GL_ONE_MINUS_SRC1_COLOR;
	case api::blend_factor::src1_alpha:
		return GL_SRC1_ALPHA;
	case api::blend_factor::inv_src1_alpha:
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
		return api::stencil_op::incr_sat;
	case GL_DECR:
		return api::stencil_op::decr_sat;
	case GL_INVERT:
		return api::stencil_op::invert;
	case GL_INCR_WRAP:
		return api::stencil_op::incr;
	case GL_DECR_WRAP:
		return api::stencil_op::decr;
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
	case api::stencil_op::incr_sat:
		return GL_INCR;
	case api::stencil_op::decr_sat:
		return GL_DECR;
	case api::stencil_op::invert:
		return GL_INVERT;
	case api::stencil_op::incr:
		return GL_INCR_WRAP;
	case api::stencil_op::decr:
		return GL_DECR_WRAP;
	}
}
auto   reshade::opengl::convert_primitive_topology(GLenum value) -> api::primitive_topology
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case GL_POINTS:
		return api::primitive_topology::point_list;
	case GL_LINES:
		return api::primitive_topology::line_list;
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
	case GL_PATCHES:
		GLint cps = 1;
		glGetIntegerv(GL_PATCH_VERTICES, &cps);
		return static_cast<api::primitive_topology>(static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) + cps - 1);
	}
}
GLenum reshade::opengl::convert_primitive_topology(api::primitive_topology value)
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
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
		return GL_PATCHES;
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

auto   reshade::opengl::convert_buffer_type_to_aspect(GLenum type) -> api::attachment_type
{
	switch (type)
	{
	default:
	case GL_COLOR:
		return api::attachment_type::color;
	case GL_DEPTH:
		return api::attachment_type::depth;
	case GL_STENCIL:
		return api::attachment_type::stencil;
	case GL_DEPTH_STENCIL:
		return api::attachment_type::depth | api::attachment_type::stencil;
	}
}
auto   reshade::opengl::convert_buffer_bits_to_aspect(GLbitfield mask) -> api::attachment_type
{
	api::attachment_type result = {};
	if (mask & GL_COLOR_BUFFER_BIT)
		result |= api::attachment_type::color;
	if (mask & GL_DEPTH_BUFFER_BIT)
		result |= api::attachment_type::depth;
	if (mask & GL_STENCIL_BUFFER_BIT)
		result |= api::attachment_type::stencil;
	return result;
}
auto   reshade::opengl::convert_aspect_to_buffer_bits(api::attachment_type mask) -> GLbitfield
{
	GLbitfield result = 0;
	if ((mask & api::attachment_type::color) == api::attachment_type::color)
		result |= GL_COLOR_BUFFER_BIT;
	if ((mask & api::attachment_type::depth) == api::attachment_type::depth)
		result |= GL_DEPTH_BUFFER_BIT;
	if ((mask & api::attachment_type::stencil) == api::attachment_type::stencil)
		result |= GL_STENCIL_BUFFER_BIT;
	return result;
}
