/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::opengl
{
	auto convert_format(api::format format) -> GLenum;
	auto convert_format(GLenum internal_format) -> api::format;
	auto convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum;

	bool is_depth_stencil_format(GLenum internal_format, GLenum usage = GL_DEPTH_STENCIL);

	void convert_memory_heap_to_usage(api::memory_heap heap, GLenum &usage);
	void convert_memory_heap_to_flags(api::memory_heap heap, GLbitfield &flags);
	api::memory_heap convert_memory_heap_from_usage(GLenum usage);
	api::memory_heap convert_memory_heap_from_flags(GLbitfield flags);

	bool check_resource_desc(GLenum target, const api::resource_desc &desc, GLenum &internal_format);
	api::resource_type convert_resource_type(GLenum target);
	api::resource_desc convert_resource_desc(GLenum target, GLsizeiptr buffer_size, api::memory_heap heap);
	api::resource_desc convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	bool check_resource_view_desc(GLenum target, const api::resource_view_desc &desc, GLenum &internal_format);
	api::resource_view_type convert_resource_view_type(GLenum target);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLintptr offset, GLsizeiptr size);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);

	api::subresource_data convert_mapped_subresource(GLenum format, GLenum type, const GLvoid *pixels, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	GLuint get_index_type_size(GLenum index_type);

	GLenum get_binding_for_target(GLenum target);

	GLenum convert_blend_op(api::blend_op value);
	GLenum convert_blend_factor(api::blend_factor value);
	GLenum convert_fill_mode(api::fill_mode value);
	GLenum convert_cull_mode(api::cull_mode value);
	GLenum convert_compare_op(api::compare_op value);
	GLenum convert_stencil_op(api::stencil_op value);
	GLenum convert_primitive_topology(api::primitive_topology value);
	GLenum convert_shader_type(api::shader_stage type);
}
