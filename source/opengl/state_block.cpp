/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_block.hpp"

namespace reshade::opengl
{
	state_block::state_block()
	{
		memset(this, 0, sizeof(*this));
	}

	void state_block::capture()
	{
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_vbo);
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &_ubo);
		glGetIntegerv(GL_CURRENT_PROGRAM, &_program);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &_active_texture);

		for (GLuint i = 0; i < 32; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &_textures2d[i]);
			glGetIntegerv(GL_SAMPLER_BINDING, &_samplers[i]);
		}

		glGetIntegerv(GL_VIEWPORT, _viewport);
		glGetIntegerv(GL_SCISSOR_BOX, _scissor_rect);
		_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
		_blend = glIsEnabled(GL_BLEND);
		glGetIntegerv(GL_BLEND_SRC, &_blend_src);
		glGetIntegerv(GL_BLEND_DST, &_blend_dest);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &_blend_eq_color);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_blend_eq_alpha);
		_depth_test = glIsEnabled(GL_DEPTH_TEST);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);
		glGetIntegerv(GL_DEPTH_FUNC, &_depth_func);
		_stencil_test = glIsEnabled(GL_STENCIL_TEST);
		glGetIntegerv(GL_STENCIL_REF, &_stencil_ref);
		glGetIntegerv(GL_STENCIL_FUNC, &_stencil_func);
		glGetIntegerv(GL_STENCIL_FAIL, &_stencil_op_fail);
		glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &_stencil_op_zfail);
		glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &_stencil_op_zpass);
		glGetIntegerv(GL_STENCIL_VALUE_MASK, &_stencil_read_mask);
		glGetIntegerv(GL_STENCIL_WRITEMASK, &_stencil_mask);
		glGetIntegerv(GL_POLYGON_MODE, &_polygon_mode);
		glGetIntegerv(GL_FRONT_FACE, &_frontface);
		_cullface = glIsEnabled(GL_CULL_FACE);
		glGetIntegerv(GL_CULL_FACE_MODE, &_cullface_mode);
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_fbo);
		_srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
		glGetBooleanv(GL_COLOR_WRITEMASK, _color_mask);

		for (GLuint i = 0; i < 8; i++)
		{
			GLint drawbuffer = GL_NONE;
			glGetIntegerv(GL_DRAW_BUFFER0 + i, &drawbuffer);
			_drawbuffers[i] = static_cast<GLenum>(drawbuffer);
		}

		if (gl3wProcs.gl.ClipControl != nullptr)
		{
			glGetIntegerv(GL_CLIP_ORIGIN, &clip_origin);
			glGetIntegerv(GL_CLIP_DEPTH_MODE, &clip_depthmode);
		}
	}
	void state_block::apply() const
	{
		glBindVertexArray(_vao);
		glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		glBindBuffer(GL_UNIFORM_BUFFER, _ubo);
		glUseProgram(_program);

		for (GLuint i = 0; i < 32; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, _textures2d[i]);
			glBindSampler(i, _samplers[i]);
		}

		glActiveTexture(_active_texture);
		glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
		glScissor(_scissor_rect[0], _scissor_rect[1], _scissor_rect[2], _scissor_rect[3]);
		if (_scissor_test) { glEnable(GL_SCISSOR_TEST); }
		else { glDisable(GL_SCISSOR_TEST); }
		if (_blend) { glEnable(GL_BLEND); }
		else { glDisable(GL_BLEND); }
		glBlendFunc(_blend_src, _blend_dest);
		glBlendEquationSeparate(_blend_eq_color, _blend_eq_alpha);
		if (_depth_test) { glEnable(GL_DEPTH_TEST); }
		else { glDisable(GL_DEPTH_TEST); }
		glDepthMask(_depth_mask);
		glDepthFunc(_depth_func);
		if (_stencil_test) { glEnable(GL_STENCIL_TEST); }
		else { glDisable(GL_STENCIL_TEST); }
		glStencilFunc(_stencil_func, _stencil_ref, _stencil_read_mask);
		glStencilOp(_stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass);
		glStencilMask(_stencil_mask);
		glPolygonMode(GL_FRONT_AND_BACK, _polygon_mode);
		glFrontFace(_frontface);
		if (_cullface) { glEnable(GL_CULL_FACE); }
		else { glDisable(GL_CULL_FACE); }
		glCullFace(_cullface_mode);
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
		if (_srgb) { glEnable(GL_FRAMEBUFFER_SRGB); }
		else { glDisable(GL_FRAMEBUFFER_SRGB); }
		glColorMask(_color_mask[0], _color_mask[1], _color_mask[2], _color_mask[3]);

		if (_drawbuffers[1] == GL_NONE &&
			_drawbuffers[2] == GL_NONE &&
			_drawbuffers[3] == GL_NONE &&
			_drawbuffers[4] == GL_NONE &&
			_drawbuffers[5] == GL_NONE &&
			_drawbuffers[6] == GL_NONE &&
			_drawbuffers[7] == GL_NONE)
		{
			glDrawBuffer(_drawbuffers[0]);
		}
		else
		{
			glDrawBuffers(8, _drawbuffers);
		}

		if (gl3wProcs.gl.ClipControl != nullptr)
		{
			glClipControl(clip_origin, clip_depthmode);
		}
	}
}
