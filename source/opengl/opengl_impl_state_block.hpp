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
		GLint _copy_read;
		GLint _copy_write;

		GLint _vao;
		GLint _vbo;
		GLint _ibo;
		GLint _ubo[4];
		GLintptr _ubo_offsets[4];
		GLsizeiptr _ubo_sizes[4];
		GLint _active_ubo;
		GLint _program;
		GLint _textures2d[32], _samplers[32];
		GLint _active_texture;

		GLint _read_fbo;
		GLint _draw_fbo;
		GLint _viewport[4];
		GLint _read_buffer;
		GLint _draw_buffers[8];

		GLboolean _srgb_enable;
		GLboolean _alpha_test;

		GLboolean _sample_alpha_to_coverage;
		GLboolean _blend_enable;
		GLboolean _logic_op_enable;
		GLint _blend_src;
		GLint _blend_dst;
		GLint _blend_src_alpha;
		GLint _blend_dst_alpha;
		GLint _blend_eq;
		GLint _blend_eq_alpha;
		GLint _logic_op;
		GLfloat _blend_constant[4];
		GLboolean _color_write_mask[4];

		GLint _polygon_mode;
		GLboolean _cull_enable;
		GLint _cull_mode;
		GLint _front_face;
		GLboolean _depth_clamp;
		GLboolean _scissor_test;
		GLint _scissor_rect[4];
		GLboolean _multisample_enable;
		GLboolean _line_smooth_enable;

		GLboolean _depth_test;
		GLboolean _depth_mask;
		GLint _depth_func;
		GLboolean _stencil_test;
		GLint _stencil_func;
		GLint _stencil_read_mask;
		GLint _stencil_write_mask;
		GLint _stencil_reference_value;
		GLint _stencil_op_fail;
		GLint _stencil_op_depth_fail;
		GLint _stencil_op_depth_pass;

		GLint _clip_origin;
		GLint _clip_depthmode;
	};
}
