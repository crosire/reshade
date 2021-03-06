/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::opengl
{
	bool is_depth_stencil_format(GLenum format, GLenum usage = GL_DEPTH_STENCIL);

	api::memory_usage  convert_memory_usage(GLenum usage);
	api::memory_usage  convert_memory_usage_from_flags(GLbitfield flags);

	api::resource_type convert_resource_type(GLenum target);

	api::resource_desc convert_resource_desc(GLsizeiptr buffer_size);
	api::resource_desc convert_resource_desc(api::resource_type type, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	api::resource_view_type convert_resource_view_type(GLenum target);
}
