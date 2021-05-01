/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::opengl
{
	bool is_depth_stencil_format(GLenum format, GLenum usage = GL_DEPTH_STENCIL);

	GLuint get_index_type_size(GLenum type);

	GLenum get_binding_for_target(GLenum target);

	void convert_format_to_internal_format(api::format format, GLenum &internalformat);
	void convert_internal_format_to_format(GLenum internalformat, api::format &format);

	api::memory_heap convert_memory_heap_from_usage(GLenum usage);
	api::memory_heap convert_memory_heap_from_flags(GLbitfield flags);
	void convert_memory_heap_to_usage(api::memory_heap heap, GLenum &usage);
	void convert_memory_heap_to_flags(api::memory_heap heap, GLbitfield &flags);

	api::resource_type convert_resource_type(GLenum target);

	api::resource_desc convert_resource_desc(GLenum target, GLsizeiptr buffer_size, api::memory_heap heap);
	api::resource_desc convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	api::resource_view_type convert_resource_view_type(GLenum target);

	api::subresource_data convert_mapped_subresource(GLenum format, GLenum type, const GLvoid *pixels, GLsizei width, GLsizei height = 1, GLsizei depth = 1);
}
