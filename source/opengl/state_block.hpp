/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "opengl.hpp"

namespace reshade::opengl
{
	class state_block
	{
	public:
		state_block();

		void capture(bool compatibility);
		void apply(bool compatibility) const;

#ifndef NDEBUG
		mutable bool has_state = false;
#endif

	private:
		GLint _vao;
		GLint _vbo;
		GLint _ibo;
		GLint _ubo;
		GLint _program;
		GLint _textures2d[32], _samplers[32];
		GLint _active_texture;
		GLint _viewport[4];
		GLint _scissor_rect[4];
		GLint _scissor_test;
		GLint _blend;
		GLint _blend_src, _blend_dest;
		GLint _blend_eq_color, _blend_eq_alpha;
		GLint _alpha_test;
		GLint _depth_test;
		GLboolean _depth_mask;
		GLint _depth_func;
		GLint _stencil_test;
		GLint _stencil_ref;
		GLint _stencil_func;
		GLint _stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass;
		GLint _stencil_read_mask, _stencil_mask;
		GLint _polygon_mode, _frontface;
		GLint _cullface, _cullface_mode;
		GLint _read_fbo;
		GLint _draw_fbo;
		GLint _srgb;
		GLint clip_origin;
		GLint clip_depthmode;
		GLboolean _color_mask[4];
		GLint _read_buffer;
		GLint _draw_buffers[8];
	};
}
