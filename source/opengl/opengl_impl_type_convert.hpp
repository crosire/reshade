/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <GL/glcorearb.h>
#include "reshade_api_pipeline.hpp"
#include <vector>
#include <limits>

namespace reshade::opengl
{
	struct pipeline_impl
	{
		void apply(api::pipeline_stage stages) const;

		GLuint program;

		std::vector<api::input_element> input_elements;
		api::primitive_topology topology;

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
		GLbitfield sample_mask;

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
		GLuint front_stencil_read_mask;
		GLuint front_stencil_write_mask;
		GLint  front_stencil_reference_value;
		GLenum front_stencil_func;
		GLenum front_stencil_op_fail;
		GLenum front_stencil_op_depth_fail;
		GLenum front_stencil_op_pass;
		GLuint back_stencil_read_mask;
		GLuint back_stencil_write_mask;
		GLint  back_stencil_reference_value;
		GLenum back_stencil_func;
		GLenum back_stencil_op_fail;
		GLenum back_stencil_op_depth_fail;
		GLenum back_stencil_op_pass;
	};

	struct descriptor_table_impl
	{
		api::descriptor_type type;
		uint32_t count;
		uint32_t base_binding;
		std::vector<uint64_t> descriptors;
	};

	struct pipeline_layout_impl
	{
		std::vector<api::descriptor_range> ranges;
	};

	struct query_heap_impl
	{
		std::vector<GLuint> queries;
	};

	struct fence_impl
	{
		uint64_t current_value;
		GLsync sync_objects[8];
	};

	constexpr auto make_resource_handle(GLenum target, GLuint object) -> api::resource
	{
		if (!object)
			return { 0 };
		return { (static_cast<uint64_t>(target) << 40) | object };
	}
	constexpr auto make_resource_view_handle(GLenum target, GLuint object, bool standalone_object = false) -> api::resource_view
	{
		return { (static_cast<uint64_t>(target) << 40) | (static_cast<uint64_t>(standalone_object ? 0x1 : 0) << 32) | object };
	}

	auto convert_format(api::format format, GLint swizzle_mask[4] = nullptr) -> GLenum;
	auto convert_format(GLenum internal_format, const GLint swizzle_mask[4] = nullptr) -> api::format;
	void convert_pixel_format(api::format format, PIXELFORMATDESCRIPTOR &pfd);
	auto convert_pixel_format(const PIXELFORMATDESCRIPTOR &pfd) -> api::format;
	auto convert_upload_format(api::format format, GLenum &type) -> GLenum;
	auto convert_upload_format(GLenum format, GLenum type) -> api::format;
	auto convert_attrib_format(api::format format, GLint &size, GLboolean &normalized) -> GLenum;
	auto convert_attrib_format(GLint size, GLenum type, GLboolean normalized) -> api::format;
	auto convert_sized_internal_format(GLenum internal_format, GLenum format) -> GLenum;

	auto is_depth_stencil_format(api::format format) -> GLenum;

	auto convert_access_flags(api::map_access flags) -> GLbitfield;
	api::map_access convert_access_flags(GLbitfield flags);

	void convert_resource_desc(const api::resource_desc &desc, GLsizeiptr &buffer_size, GLbitfield &storage_flags);
	api::resource_type convert_resource_type(GLenum target);
	api::resource_desc convert_resource_desc(GLenum target, GLsizeiptr buffer_size, GLbitfield storage_flags);
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

template <typename T>
inline void hash_combine(size_t &seed, const T &v)
{
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
