/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <glad/gl.h>

namespace reshade::opengl
{
	class device_impl;

	class state_block
	{
	public:
		explicit state_block(device_impl *device);

		void capture();
		void apply() const;

	private:
		device_impl *const _device_impl;

		GLint _copy_read = 0;
		GLint _copy_write = 0;

		GLint _vao = 0;
		GLint _vbo = 0;
		GLint _ibo = 0;
		GLint _ubo[4] = {};
		GLintptr _ubo_offsets[4] = {};
		GLsizeiptr _ubo_sizes[4] = {};
		GLint _active_ubo = 0;
		GLint _program = 0;
		GLint _textures2d[32] = {}, _samplers[32] = {};
		GLint _active_texture = 0;

		GLint _read_fbo = 0;
		GLint _draw_fbo = 0;
		GLint _viewport[4] = {};
		GLint _read_buffer = 0;
		GLint _draw_buffers[8] = {};

		GLboolean _srgb_enable = GL_FALSE;
		GLboolean _alpha_test = GL_FALSE;

		GLboolean _sample_alpha_to_coverage = GL_FALSE;
		GLboolean _blend_enable[8] = {};
		GLboolean _logic_op_enable = GL_FALSE;
		GLint _blend_src[8] = {};
		GLint _blend_dst[8] = {};
		GLint _blend_src_alpha[8] = {};
		GLint _blend_dst_alpha[8] = {};
		GLint _blend_eq[8] = {};
		GLint _blend_eq_alpha[8] = {};
		GLfloat _blend_constant[4] = {};
		GLint _logic_op = GL_NONE;
		GLboolean _color_write_mask[8][4] = {};
		GLint _sample_mask = 0;

		GLint _polygon_mode = GL_NONE;
		GLboolean _cull_enable = GL_FALSE;
		GLint _cull_mode = GL_NONE;
		GLint _front_face = GL_NONE;
		GLboolean _depth_clamp = GL_FALSE;
		GLboolean _scissor_test = GL_FALSE;
		GLint _scissor_rect[4] = {};
		GLboolean _multisample_enable = GL_FALSE;
		GLboolean _line_smooth_enable = GL_FALSE;

		GLboolean _depth_test = GL_FALSE;
		GLboolean _depth_mask = GL_TRUE;
		GLint _depth_func = GL_LESS;
		GLboolean _stencil_test = GL_FALSE;
		GLint _front_stencil_read_mask = 0xFF;
		GLint _front_stencil_write_mask = 0xFF;
		GLint _front_stencil_reference_value = 0;
		GLint _front_stencil_func = GL_ALWAYS;
		GLint _front_stencil_pass_op = GL_KEEP;
		GLint _front_stencil_fail_op = GL_KEEP;
		GLint _front_stencil_depth_fail_op = GL_KEEP;
		GLint _back_stencil_read_mask = 0xFF;
		GLint _back_stencil_write_mask = 0xFF;
		GLint _back_stencil_reference_value = 0;
		GLint _back_stencil_func = GL_ALWAYS;
		GLint _back_stencil_pass_op = GL_KEEP;
		GLint _back_stencil_fail_op = GL_KEEP;
		GLint _back_stencil_depth_fail_op = GL_KEEP;

		GLint _clip_origin = GL_NONE;
		GLint _clip_depthmode = GL_NONE;
	};
}
