/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <GL/gl3w.h>

namespace reshade::opengl
{
	class state_block
	{
	public:
		state_block();

		void capture(bool compatibility);
		void apply(bool compatibility) const;

	private:
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
		GLboolean _blend_enable = GL_FALSE;
		GLboolean _logic_op_enable = GL_FALSE;
		GLint _blend_src = GL_NONE;
		GLint _blend_dst = GL_NONE;
		GLint _blend_src_alpha = GL_NONE;
		GLint _blend_dst_alpha = GL_NONE;
		GLint _blend_eq = GL_NONE;
		GLint _blend_eq_alpha = GL_NONE;
		GLint _logic_op = GL_NONE;
		GLfloat _blend_constant[4] = {};
		GLboolean _color_write_mask[4] = {};

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
		GLboolean _depth_mask = GL_FALSE;
		GLint _depth_func = GL_NONE;
		GLboolean _stencil_test = GL_FALSE;
		GLint _stencil_func = GL_NONE;
		GLint _stencil_read_mask = GL_NONE;
		GLint _stencil_write_mask = GL_NONE;
		GLint _stencil_reference_value = GL_NONE;
		GLint _stencil_op_fail = GL_NONE;
		GLint _stencil_op_depth_fail = GL_NONE;
		GLint _stencil_op_depth_pass = GL_NONE;

		GLint _clip_origin = GL_NONE;
		GLint _clip_depthmode = GL_NONE;
	};
}
