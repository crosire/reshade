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

memory_usage  reshade::opengl::convert_memory_usage(GLenum usage)
{
	switch (usage)
	{
	default:
	case GL_STATIC_DRAW:
		return memory_usage::gpu_only;
	case GL_STREAM_DRAW:
	case GL_DYNAMIC_DRAW:
		return memory_usage::cpu_to_gpu;
	case GL_STREAM_READ:
	case GL_STATIC_READ:
	case GL_DYNAMIC_READ:
		return memory_usage::gpu_to_cpu;
	}
}
memory_usage  reshade::opengl::convert_memory_usage_from_flags(GLbitfield flags)
{
	if ((flags & GL_DYNAMIC_STORAGE_BIT) != 0)
		return memory_usage::cpu_to_gpu;
	if ((flags & GL_MAP_READ_BIT) != 0)
		return memory_usage::gpu_to_cpu;
	return memory_usage::gpu_only;
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

resource_desc reshade::opengl::convert_resource_desc(GLsizeiptr buffer_size)
{
	resource_desc desc = {};
	desc.size = buffer_size;
	desc.usage = resource_usage::shader_resource; // TODO: Only texture copy currently implemented in 'device_impl::copy_resource', so cannot add copy usage flags here
	return desc;
}
resource_desc reshade::opengl::convert_resource_desc(resource_type type, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	resource_desc desc = {};
	desc.width = width;
	desc.height = height;
	assert(depth <= std::numeric_limits<uint16_t>::max());
	desc.depth_or_layers = static_cast<uint16_t>(depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.levels = static_cast<uint16_t>(levels);
	desc.format = internalformat;
	desc.samples = 1;

	desc.usage = resource_usage::copy_dest | resource_usage::copy_source;
	if (is_depth_stencil_format(internalformat))
		desc.usage |= resource_usage::depth_stencil;
	if (type == resource_type::texture_1d || type == resource_type::texture_2d || type == resource_type::surface)
		desc.usage |= resource_usage::render_target;
	if (type != resource_type::surface)
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
