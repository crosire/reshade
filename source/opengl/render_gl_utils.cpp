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
	desc.size = buffer_size;
	desc.heap = heap;
	desc.usage = resource_usage::shader_resource; // TODO: Only texture copy currently implemented in 'device_impl::copy_resource', so cannot add copy usage flags here
	return desc;
}
resource_desc reshade::opengl::convert_resource_desc(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	resource_desc desc = {};
	desc.type = convert_resource_type(target);
	desc.width = width;
	desc.height = height;
	assert(depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(levels);
	desc.format = internalformat;
	desc.samples = 1;
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

mapped_subresource reshade::opengl::convert_mapped_subresource(GLenum format, GLenum type, const GLvoid *pixels, GLsizei width, GLsizei height, GLsizei)
{
	mapped_subresource result;
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
