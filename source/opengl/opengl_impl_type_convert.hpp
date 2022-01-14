/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"

namespace reshade::opengl
{
	struct pipeline_impl
	{
		void apply_compute() const;
		void apply_graphics() const;

		GLuint program;
		GLuint vao;

		// Blend state

		GLboolean sample_alpha_to_coverage;
		GLboolean blend_enable[8];
		GLboolean logic_op_enable;
		GLenum blend_src[8];
		GLenum blend_dst[8];
		GLenum blend_src_alpha[8];
		GLenum blend_dst_alpha[8];
		GLenum blend_eq[8];
		GLenum blend_eq_alpha[8];
		GLenum logic_op;
		GLfloat blend_constant[4];
		GLboolean color_write_mask[8][4];

		// Rasterizer state

		GLenum polygon_mode;
		GLenum cull_mode;
		GLenum front_face;
		GLboolean depth_clamp;
		GLboolean scissor_test;
		GLboolean multisample_enable;
		GLboolean line_smooth_enable;

		// Depth-stencil state

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
	};

	struct descriptor_set_impl
	{
		api::descriptor_type type;
		GLuint count;
		std::vector<uint64_t> descriptors;
	};

	struct pipeline_layout_impl
	{
		std::vector<api::descriptor_range> ranges;
	};

	struct query_pool_impl
	{
		std::vector<GLuint> queries;
	};

	constexpr api::pipeline_layout global_pipeline_layout = { 0xFFFFFFFFFFFFFFFF };

	template <typename T>
	inline void hash_combine(size_t &seed, const T &v)
	{
		seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	auto convert_format(api::format format, GLint swizzle_mask[4] = nullptr) -> GLenum;
	auto convert_format(GLenum internal_format, const GLint swizzle_mask[4] = nullptr) -> api::format;
	auto convert_format(GLenum format, GLenum type) -> api::format;
	auto convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum;
	auto convert_upload_format(GLenum internal_format, GLenum &type) -> GLenum;

	bool is_depth_stencil_format(GLenum internal_format, GLenum usage = GL_DEPTH_STENCIL);

	void convert_memory_heap_to_usage(const api::resource_desc &desc, GLenum &usage);
	void convert_memory_heap_to_flags(const api::resource_desc &desc, GLbitfield &flags);
	void convert_memory_heap_from_usage(api::resource_desc &desc, GLenum usage);
	void convert_memory_heap_from_flags(api::resource_desc &desc, GLbitfield flags);

	GLbitfield convert_access_flags(api::map_access flags);
	api::map_access convert_access_flags(GLbitfield flags);

	api::resource_type convert_resource_type(GLenum target);
	api::resource_desc convert_resource_desc(GLenum target, GLsizeiptr buffer_size);
	api::resource_desc convert_resource_desc(GLenum target, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height = 1, GLsizei depth = 1, const GLint swizzle_mask[4] = nullptr);

	api::resource_view_type convert_resource_view_type(GLenum target);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLintptr offset, GLsizeiptr size);
	api::resource_view_desc convert_resource_view_desc(GLenum target, GLenum internal_format, GLuint min_level, GLuint num_levels, GLuint min_layer, GLuint num_layers);

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
}
