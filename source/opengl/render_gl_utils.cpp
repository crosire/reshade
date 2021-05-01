/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_gl.hpp"
#include "render_gl_utils.hpp"
#include <cassert>

using namespace reshade::api;

bool reshade::opengl::is_depth_stencil_format(GLenum format, GLenum usage)
{
	switch (format)
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

GLuint reshade::opengl::get_index_type_size(GLenum type)
{
	switch (type)
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
	default:
		return GL_NONE;
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
		return GL_TEXTURE_BINDING_CUBE_MAP;
	case GL_TEXTURE_CUBE_MAP_ARRAY:
		return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
	case GL_TEXTURE_RECTANGLE:
		return GL_TEXTURE_BINDING_RECTANGLE;
	}
}

void reshade::opengl::convert_format_to_internal_format(format format, GLenum &internalformat)
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
		internalformat = GL_R8UI;
		break;
	case format::r8_sint:
		internalformat = GL_R8I;
		break;
	case format::r8_typeless:
	case format::r8_unorm:
	case format::a8_unorm:
		internalformat = GL_R8;
		break;
	case format::r8_snorm:
		internalformat = GL_R8_SNORM;
		break;
	case format::r8g8_uint:
		internalformat = GL_RG8UI;
		break;
	case format::r8g8_sint:
		internalformat = GL_RG8I;
		break;
	case format::r8g8_typeless:
	case format::r8g8_unorm:
		internalformat = GL_RG8;
		break;
	case format::r8g8_snorm:
		internalformat = GL_RG8_SNORM;
		break;
	case format::r8g8b8a8_uint:
		internalformat = GL_RGBA8UI;
		break;
	case format::r8g8b8a8_sint:
		internalformat = GL_RGBA8I;
		break;
	case format::r8g8b8a8_typeless:
	case format::r8g8b8a8_unorm:
		internalformat = GL_RGBA8;
		break;
	case format::r8g8b8a8_unorm_srgb:
		internalformat = GL_SRGB8_ALPHA8;
		break;
	case format::r8g8b8a8_snorm:
		internalformat = GL_RGBA8_SNORM;
		break;
	case format::b8g8r8a8_typeless:
	case format::b8g8r8a8_unorm:
	case format::b8g8r8a8_unorm_srgb:
	case format::b8g8r8x8_typeless:
	case format::b8g8r8x8_unorm:
	case format::b8g8r8x8_unorm_srgb:
		// Unsupported
		break;
	case format::r10g10b10a2_uint:
		internalformat = GL_RGB10_A2UI;
		break;
	case format::r10g10b10a2_typeless:
	case format::r10g10b10a2_unorm:
		internalformat = GL_RGB10_A2;
		break;
	case format::r10g10b10a2_xr_bias:
		// Unsupported
		break;
	case format::r16_uint:
		internalformat = GL_R16UI;
		break;
	case format::r16_sint:
		internalformat = GL_R16I;
		break;
	case format::r16_typeless:
	case format::r16_float:
		internalformat = GL_R16F;
		break;
	case format::r16_unorm:
		internalformat = GL_R16;
		break;
	case format::r16_snorm:
		internalformat = GL_R16_SNORM;
		break;
	case format::r16g16_uint:
		internalformat = GL_RG16UI;
		break;
	case format::r16g16_sint:
		internalformat = GL_RG16I;
		break;
	case format::r16g16_typeless:
	case format::r16g16_float:
		internalformat = GL_RG16F;
		break;
	case format::r16g16_unorm:
		internalformat = GL_RG16;
		break;
	case format::r16g16_snorm:
		internalformat = GL_RG16_SNORM;
		break;
	case format::r16g16b16a16_uint:
		internalformat = GL_RGBA16UI;
		break;
	case format::r16g16b16a16_sint:
		internalformat = GL_RGBA16I;
		break;
	case format::r16g16b16a16_typeless:
	case format::r16g16b16a16_float:
		internalformat = GL_RGBA16F;
		break;
	case format::r16g16b16a16_unorm:
		internalformat = GL_RGBA16;
		break;
	case format::r16g16b16a16_snorm:
		internalformat = GL_RGBA16_SNORM;
		break;
	case format::r32_uint:
		internalformat = GL_R32UI;
		break;
	case format::r32_sint:
		internalformat = GL_R32I;
		break;
	case format::r32_typeless:
	case format::r32_float:
		internalformat = GL_R32F;
		break;
	case format::r32g32_uint:
		internalformat = GL_RG32UI;
		break;
	case format::r32g32_sint:
		internalformat = GL_RG32I;
		break;
	case format::r32g32_typeless:
	case format::r32g32_float:
		internalformat = GL_RG32F;
		break;
	case format::r32g32b32_uint:
		internalformat = GL_RGB32UI;
		break;
	case format::r32g32b32_sint:
		internalformat = GL_RGB32I;
		break;
	case format::r32g32b32_typeless:
	case format::r32g32b32_float:
		internalformat = GL_RGB32F;
		break;
	case format::r32g32b32a32_uint:
		internalformat = GL_RGBA32UI;
		break;
	case format::r32g32b32a32_sint:
		internalformat = GL_RGBA32I;
		break;
	case format::r32g32b32a32_typeless:
	case format::r32g32b32a32_float:
		internalformat = GL_RGBA32F;
		break;
	case format::r9g9b9e5:
		internalformat = GL_RGB9_E5;
		break;
	case format::r11g11b10_float:
		internalformat = GL_R11F_G11F_B10F;
		break;
	case format::b5g6r5_unorm:
		internalformat = GL_RGB565;
		break;
	case format::b5g5r5a1_unorm:
		internalformat = GL_RGB5_A1;
		break;
	case format::b4g4r4a4_unorm:
		internalformat = GL_RGBA4;
		break;
	case format::s8_uint:
		internalformat = GL_STENCIL_INDEX8;
		break;
	case format::d16_unorm:
		internalformat = GL_DEPTH_COMPONENT16;
		break;
	case format::d16_unorm_s8_uint:
		// Unsupported
		break;
	case format::r24_g8_typeless:
	case format::r24_unorm_x8_uint:
	case format::x24_unorm_g8_uint:
	case format::d24_unorm_s8_uint:
		internalformat = GL_DEPTH24_STENCIL8;
		break;
	case format::d32_float:
		internalformat = GL_DEPTH_COMPONENT32F;
		break;
	case format::r32_g8_typeless:
	case format::r32_float_x8_uint:
	case format::x32_float_g8_uint:
	case format::d32_float_s8_uint:
		internalformat = GL_DEPTH32F_STENCIL8;
		break;
	case format::bc1_typeless:
	case format::bc1_unorm:
		internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case format::bc1_unorm_srgb:
		internalformat = 0x8C4D /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT */;
		break;
	case format::bc2_typeless:
	case format::bc2_unorm:
		internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case format::bc2_unorm_srgb:
		internalformat = 0x8C4E /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT */;
		break;
	case format::bc3_typeless:
	case format::bc3_unorm:
		internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	case format::bc3_unorm_srgb:
		internalformat = 0x8C4F /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT */;
		break;
	case format::bc4_typeless:
	case format::bc4_unorm:
		internalformat = GL_COMPRESSED_RED_RGTC1;
		break;
	case format::bc4_snorm:
		internalformat = GL_COMPRESSED_SIGNED_RED_RGTC1;
		break;
	case format::bc5_typeless:
	case format::bc5_unorm:
		internalformat = GL_COMPRESSED_RG_RGTC2;
		break;
	case format::bc5_snorm:
		internalformat = GL_COMPRESSED_SIGNED_RG_RGTC2;
		break;
	case format::bc6h_typeless:
	case format::bc6h_ufloat:
		internalformat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
		break;
	case format::bc6h_sfloat:
		internalformat = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB;
		break;
	case format::bc7_typeless:
	case format::bc7_unorm:
		internalformat = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
		break;
	case format::bc7_unorm_srgb:
		internalformat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB;
		break;
	case format::r8g8_b8g8_unorm:
	case format::g8r8_g8b8_unorm:
		// Unsupported
		break;
	}
}
void reshade::opengl::convert_internal_format_to_format(GLenum internalformat, format &format)
{
	switch (internalformat)
	{
	default:
		format = format::unknown;
		break;
	case GL_R8UI:
		format = format::r8_uint;
		break;
	case GL_R8I:
		format = format::r8_sint;
		break;
	case GL_R8:
		format = format::r8_unorm;
		break;
	case GL_R8_SNORM:
		format = format::r8_snorm;
		break;
	case GL_RG8UI:
		format = format::r8g8_uint;
		break;
	case GL_RG8I:
		format = format::r8g8_sint;
		break;
	case GL_RG8:
		format = format::r8g8_unorm;
		break;
	case GL_RG8_SNORM:
		format = format::r8g8_snorm;
		break;
	case GL_RGBA8UI:
		format = format::r8g8b8a8_uint;
		break;
	case GL_RGBA8I:
		format = format::r8g8b8a8_sint;
		break;
	case GL_RGBA8:
		format = format::r8g8b8a8_unorm;
		break;
	case GL_SRGB8_ALPHA8:
		format = format::r8g8b8a8_unorm_srgb;
		break;
	case GL_RGBA8_SNORM:
		format = format::r8g8b8a8_snorm;
		break;
	case GL_RGB10_A2UI:
		format = format::r10g10b10a2_uint;
		break;
	case GL_RGB10_A2:
		format = format::r10g10b10a2_unorm;
		break;
	case GL_R16UI:
		format = format::r16_uint;
		break;
	case GL_R16I:
		format = format::r16_sint;
		break;
	case GL_R16F:
		format = format::r16_float;
		break;
	case GL_R16:
		format = format::r16_unorm;
		break;
	case GL_R16_SNORM:
		format = format::r16_snorm;
		break;
	case GL_RG16UI:
		format = format::r16g16_uint;
		break;
	case GL_RG16I:
		format = format::r16g16_sint;
		break;
	case GL_RG16F:
		format = format::r16g16_float;
		break;
	case GL_RG16:
		format = format::r16g16_unorm;
		break;
	case GL_RG16_SNORM:
		format = format::r16g16_snorm;
		break;
	case GL_RGBA16UI:
		format = format::r16g16b16a16_uint;
		break;
	case GL_RGBA16I:
		format = format::r16g16b16a16_sint;
		break;
	case GL_RGBA16F:
		format = format::r16g16b16a16_float;
		break;
	case GL_RGBA16:
		format = format::r16g16b16a16_unorm;
		break;
	case GL_RGBA16_SNORM:
		format = format::r16g16b16a16_snorm;
		break;
	case GL_R32UI:
		format = format::r32_uint;
		break;
	case GL_R32I:
		format = format::r32_sint;
		break;
	case GL_R32F:
		format = format::r32_float;
		break;
	case GL_RG32UI:
		format = format::r32g32_uint;
		break;
	case GL_RG32I:
		format = format::r32g32_sint;
		break;
	case GL_RG32F:
		format = format::r32g32_float;
		break;
	case GL_RGB32UI:
		format = format::r32g32b32_uint;
		break;
	case GL_RGB32I:
		format = format::r32g32b32_sint;
		break;
	case GL_RGB32F:
		format = format::r32g32b32_float;
		break;
	case GL_RGBA32UI:
		format = format::r32g32b32a32_uint;
		break;
	case GL_RGBA32I:
		format = format::r32g32b32a32_sint;
		break;
	case GL_RGBA32F:
		format = format::r32g32b32a32_float;
		break;
	case GL_RGB9_E5:
		format = format::r9g9b9e5;
		break;
	case GL_R11F_G11F_B10F:
		format = format::r11g11b10_float;
		break;
	case GL_RGB565:
		format = format::b5g6r5_unorm;
		break;
	case GL_RGB5_A1:
		format = format::b5g5r5a1_unorm;
		break;
	case GL_RGBA4:
		format = format::b4g4r4a4_unorm;
		break;
	case GL_STENCIL_INDEX:
	case GL_STENCIL_INDEX8:
		format = format::s8_uint;
		break;
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
		format = format::d16_unorm;
		break;
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_STENCIL:
	case GL_DEPTH24_STENCIL8:
		format = format::d24_unorm_s8_uint;
		break;
	case GL_DEPTH_COMPONENT32:
	case GL_DEPTH_COMPONENT32F:
	case GL_DEPTH_COMPONENT32F_NV:
		format = format::d32_float;
		break;
	case GL_DEPTH32F_STENCIL8:
	case GL_DEPTH32F_STENCIL8_NV:
		format = format::d32_float_s8_uint;
		break;
	case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		format = format::bc1_unorm;
		break;
	case 0x8C4D /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT */:
		format = format::bc1_unorm_srgb;
		break;
	case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		format = format::bc2_unorm;
		break;
	case 0x8C4E /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT */:
		format = format::bc2_unorm_srgb;
		break;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		format = format::bc3_unorm;
		break;
	case 0x8C4F /* GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT */:
		format = format::bc3_unorm_srgb;
		break;
	case GL_COMPRESSED_RED_RGTC1:
		format = format::bc4_unorm;
		break;
	case GL_COMPRESSED_SIGNED_RED_RGTC1:
		format = format::bc4_snorm;
		break;
	case GL_COMPRESSED_RG_RGTC2:
		format = format::bc5_unorm;
		break;
	case GL_COMPRESSED_SIGNED_RG_RGTC2:
		format = format::bc5_snorm;
		break;
	case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
		format = format::bc6h_ufloat;
		break;
	case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
		format = format::bc6h_sfloat;
		break;
	case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
		format = format::bc7_unorm;
		break;
	case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
		format = format::bc7_unorm_srgb;
		break;
	}
}

memory_heap reshade::opengl::convert_memory_heap_from_usage(GLenum usage)
{
	switch (usage)
	{
	case GL_STATIC_DRAW:
		return memory_heap::gpu_only;
	case GL_STREAM_DRAW:
	case GL_DYNAMIC_DRAW:
		return memory_heap::cpu_to_gpu;
	case GL_STREAM_READ:
	case GL_STATIC_READ:
	case GL_DYNAMIC_READ:
		return memory_heap::gpu_to_cpu;
	}
	return memory_heap::unknown;
}
memory_heap reshade::opengl::convert_memory_heap_from_flags(GLbitfield flags)
{
	if ((flags & (GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT)) != 0)
		return memory_heap::cpu_to_gpu;
	if ((flags & (GL_MAP_READ_BIT)) != 0)
		return memory_heap::gpu_to_cpu;
	if ((flags & (GL_CLIENT_STORAGE_BIT)) != 0)
		return memory_heap::gpu_only;
	return memory_heap::unknown;
}
void reshade::opengl::convert_memory_heap_to_usage(api::memory_heap heap, GLenum &usage)
{
	switch (heap)
	{
	case memory_heap::gpu_only:
		usage = GL_STATIC_DRAW;
		break;
	case memory_heap::cpu_to_gpu:
		usage = GL_DYNAMIC_DRAW;
		break;
	case memory_heap::gpu_to_cpu:
		usage = GL_DYNAMIC_READ;
		break;
	}
}
void reshade::opengl::convert_memory_heap_to_flags(api::memory_heap heap, GLbitfield &flags)
{
	switch (heap)
	{
	case memory_heap::gpu_only:
		flags |= GL_CLIENT_STORAGE_BIT;
		break;
	case memory_heap::cpu_to_gpu:
		flags |= GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
		break;
	case memory_heap::gpu_to_cpu:
		flags |= GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT;
		break;
	}
}

resource_type reshade::opengl::convert_resource_type(GLenum target)
{
	switch (target)
	{
	default:
		return resource_type::unknown;
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
		return resource_type::buffer;
	case GL_TEXTURE_1D:
	case GL_TEXTURE_1D_ARRAY:
	case GL_PROXY_TEXTURE_1D:
	case GL_PROXY_TEXTURE_1D_ARRAY:
		return resource_type::texture_1d;
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_RECTANGLE: // This is not technically compatible with 2D textures
	case GL_PROXY_TEXTURE_2D:
	case GL_PROXY_TEXTURE_2D_ARRAY:
	case GL_PROXY_TEXTURE_RECTANGLE:
		return resource_type::texture_2d;
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
	case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
		return resource_type::texture_2d;
	case GL_TEXTURE_3D:
	case GL_PROXY_TEXTURE_3D:
		return resource_type::texture_3d;
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
		return resource_type::texture_2d;
	case GL_RENDERBUFFER:
	case GL_FRAMEBUFFER_DEFAULT:
		return resource_type::surface;
	}
}

resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizeiptr buffer_size, memory_heap heap)
{
	resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.buffer.size = buffer_size;
	desc.heap = heap;
	desc.usage = resource_usage::shader_resource; // TODO: Only texture copy currently implemented in 'device_impl::copy_resource', so cannot add copy usage flags here
	return desc;
}
resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.texture.width = width;
	desc.texture.height = height;
	assert(depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	convert_internal_format_to_format(internalformat, desc.texture.format);
	desc.texture.samples = static_cast<uint16_t>(samples);

	desc.heap = memory_heap::gpu_only;

	desc.usage = resource_usage::copy_dest | resource_usage::copy_source;
	if (is_depth_stencil_format(internalformat))
		desc.usage |= resource_usage::depth_stencil;
	if (desc.type == resource_type::texture_1d || desc.type == resource_type::texture_2d || desc.type == resource_type::surface)
		desc.usage |= resource_usage::render_target;
	if (desc.type != resource_type::surface)
		desc.usage |= resource_usage::shader_resource;

	return desc;
}

resource_view_type reshade::opengl::convert_resource_view_type(GLenum target)
{
	switch (target)
	{
	default:
		return resource_view_type::unknown;
	case GL_TEXTURE_BUFFER:
		return resource_view_type::buffer;
	case GL_TEXTURE_1D:
		return resource_view_type::texture_1d;
	case GL_TEXTURE_1D_ARRAY:
		return resource_view_type::texture_1d_array;
	case GL_TEXTURE_2D:
	case GL_TEXTURE_RECTANGLE:
		return resource_view_type::texture_2d;
	case GL_TEXTURE_2D_ARRAY:
		return resource_view_type::texture_2d_array;
	case GL_TEXTURE_2D_MULTISAMPLE:
		return resource_view_type::texture_2d_multisample;
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		return resource_view_type::texture_2d_multisample_array;
	case GL_TEXTURE_3D:
		return resource_view_type::texture_3d;
	case GL_TEXTURE_CUBE_MAP:
		return resource_view_type::texture_cube;
	case GL_TEXTURE_CUBE_MAP_ARRAY:
		return resource_view_type::texture_cube_array;
	}
}

subresource_data reshade::opengl::convert_mapped_subresource(GLenum format, GLenum type, const GLvoid *pixels, GLsizei width, GLsizei height, GLsizei)
{
	subresource_data result;
	result.data = pixels;

	uint32_t bpp = 1;
	switch (type)
	{
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
	case GL_UNSIGNED_BYTE_3_3_2:
	case GL_UNSIGNED_BYTE_2_3_3_REV:
		bpp = 1;
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
	case GL_UNSIGNED_SHORT_5_6_5:
	case GL_UNSIGNED_SHORT_5_6_5_REV:
	case GL_UNSIGNED_SHORT_4_4_4_4:
	case GL_UNSIGNED_SHORT_4_4_4_4_REV:
	case GL_UNSIGNED_SHORT_5_5_5_1:
	case GL_UNSIGNED_SHORT_1_5_5_5_REV:
	case GL_HALF_FLOAT:
		bpp = 2;
		break;
	case GL_INT:
	case GL_UNSIGNED_INT:
	case GL_UNSIGNED_INT_8_8_8_8:
	case GL_UNSIGNED_INT_8_8_8_8_REV:
	case GL_UNSIGNED_INT_10_10_10_2:
	case GL_UNSIGNED_INT_2_10_10_10_REV:
	case GL_FLOAT:
		bpp = 4;
		break;
	}

	result.row_pitch = bpp * width;
	result.depth_pitch = bpp * width * height;

	switch (format)
	{
	case GL_RED:
	case GL_RED_INTEGER:
	case GL_STENCIL_INDEX:
	case GL_DEPTH_COMPONENT:
		break;
	case GL_RG:
	case GL_RG_INTEGER:
	case GL_DEPTH_STENCIL:
		result.row_pitch *= 2;
		result.depth_pitch *= 2;
		break;
	case GL_RGB:
	case GL_RGB_INTEGER:
	case GL_BGR:
	case GL_BGR_INTEGER:
		result.row_pitch *= 3;
		result.depth_pitch *= 3;
		break;
	case GL_RGBA:
	case GL_RGBA_INTEGER:
	case GL_BGRA:
	case GL_BGRA_INTEGER:
		result.row_pitch *= 4;
		result.depth_pitch *= 4;
		break;
	}

	return result;
}
