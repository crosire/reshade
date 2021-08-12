/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::opengl
{
	struct pipeline_impl
	{
		~pipeline_impl()
		{
			glDeleteProgram(program);
			glDeleteVertexArrays(1, &vao);
		}

		GLuint program;
		GLuint vao;

		GLboolean sample_alpha_to_coverage;
		GLboolean blend_enable;
		GLboolean logic_op_enable;
		GLenum blend_src;
		GLenum blend_dst;
		GLenum blend_src_alpha;
		GLenum blend_dst_alpha;
		GLenum blend_eq;
		GLenum blend_eq_alpha;
		GLenum logic_op;
		GLfloat blend_constant[4];
		GLboolean color_write_mask[4];

		GLenum polygon_mode;
		GLenum cull_mode;
		GLenum front_face;
		GLboolean depth_clamp;
		GLboolean scissor_test;
		GLboolean multisample_enable;
		GLboolean line_smooth_enable;

		GLboolean depth_test;
		GLboolean depth_mask;
		GLenum depth_func;
		GLboolean stencil_test;
		GLuint stencil_read_mask;
		GLuint stencil_write_mask;
		GLint  stencil_reference_value;
		GLenum front_stencil_op_fail;
		GLenum front_stencil_op_depth_fail;
		GLenum front_stencil_op_pass;
		GLenum front_stencil_func;
		GLenum back_stencil_op_fail;
		GLenum back_stencil_op_depth_fail;
		GLenum back_stencil_op_pass;
		GLenum back_stencil_func;

		GLbitfield sample_mask;
		GLenum prim_mode;
		GLuint patch_vertices;

		void apply_compute() const;
		void apply_graphics() const;
	};

	struct pipeline_layout_impl
	{
		std::vector<GLuint> bindings;
		std::vector<api::pipeline_layout_param> params;
	};

	struct descriptor_set_layout_impl
	{
		api::descriptor_range range;
	};

	struct query_pool_impl
	{
		~query_pool_impl()
		{
			glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
		}

		std::vector<GLuint> queries;
	};

	struct descriptor_set_impl
	{
		api::descriptor_type type;
		GLuint count;
		std::vector<uint64_t> descriptors;
	};

	auto convert_format(api::format format) -> GLenum;
	auto convert_format(GLenum internal_format) -> api::format;
	auto convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum;
	auto convert_upload_format(GLenum internal_format, GLenum &type) -> GLenum;

	bool is_depth_stencil_format(GLenum internal_format, GLenum usage = GL_DEPTH_STENCIL);

	void convert_memory_heap_to_usage(const api::resource_desc &desc, GLenum &usage);
	void convert_memory_heap_to_flags(const api::resource_desc &desc, GLbitfield &flags);
	void convert_memory_heap_from_usage(api::resource_desc &desc, GLenum usage);
	void convert_memory_heap_from_flags(api::resource_desc &desc, GLbitfield flags);

	bool check_resource_desc(GLenum target, const api::resource_desc &desc, GLenum &internal_format);
	api::resource_type convert_resource_type(GLenum target);
	api::resource_desc convert_resource_desc(GLenum target, GLsizeiptr buffer_size);
	api::resource_desc convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	bool check_resource_view_desc(GLenum target, const api::resource_view_desc &desc, GLenum &internal_format);
	api::resource_view_type convert_resource_view_type(GLenum target);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLintptr offset, GLsizeiptr size);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);

	api::subresource_data convert_mapped_subresource(GLenum format, GLenum type, const GLvoid *pixels, GLsizei width, GLsizei height = 1, GLsizei depth = 1);

	GLuint get_index_type_size(GLenum index_type);

	GLenum get_binding_for_target(GLenum target);

	auto   convert_logic_op(GLenum value) -> api::logic_op;
	GLenum convert_logic_op(api::logic_op value);
	auto   convert_blend_op(GLenum value) -> api::blend_op;
	GLenum convert_blend_op(api::blend_op value);
	auto   convert_blend_factor(GLenum value) -> api::blend_factor;
	GLenum convert_blend_factor(api::blend_factor value);
	auto   convert_fill_mode(GLenum value) -> api::fill_mode;
	GLenum convert_fill_mode(api::fill_mode value);
	auto   convert_cull_mode(GLenum value) -> api::cull_mode;
	GLenum convert_cull_mode(api::cull_mode value);
	auto   convert_compare_op(GLenum value) -> api::compare_op;
	GLenum convert_compare_op(api::compare_op value);
	auto   convert_stencil_op(GLenum value) -> api::stencil_op;
	GLenum convert_stencil_op(api::stencil_op value);
	auto   convert_primitive_topology(GLenum value) -> api::primitive_topology;
	GLenum convert_primitive_topology(api::primitive_topology value);
	GLenum convert_query_type(api::query_type type);
	GLenum convert_shader_type(api::shader_stage type);

	auto   convert_buffer_type_to_aspect(GLenum type) -> api::attachment_type;
	auto   convert_buffer_bits_to_aspect(GLbitfield mask) -> api::attachment_type;
	auto   convert_aspect_to_buffer_bits(api::attachment_type mask) -> GLbitfield;
}
