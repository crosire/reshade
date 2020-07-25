/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_block.hpp"

#define glEnableb(cap, value) \
	if (value) { glEnable(cap); } \
	else { glDisable(cap); }

reshade::opengl::state_block::state_block()
{
	memset(this, 0, sizeof(*this));
}

void reshade::opengl::state_block::capture()
{
#ifndef NDEBUG
	has_state = true;
#endif

	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_vbo);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &_ibo);

	glGetIntegerv(GL_CURRENT_PROGRAM, &_program);
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &_ubo);

	// Technically should capture image bindings here as well ...
	glGetIntegerv(GL_ACTIVE_TEXTURE, &_active_texture);
	for (GLuint i = 0; i < 32; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glGetIntegerv(GL_SAMPLER_BINDING, &_samplers[i]);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &_textures2d[i]);
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
	glGetBooleanv(GL_COLOR_WRITEMASK, _color_mask);

	_srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_draw_fbo);

	glGetIntegerv(GL_READ_BUFFER, &_read_buffer);

	for (GLuint i = 0; i < 8; i++)
	{
		glGetIntegerv(GL_DRAW_BUFFER0 + i, &_draw_buffers[i]);
	}

	if (gl3wProcs.gl.ClipControl != nullptr)
	{
		glGetIntegerv(GL_CLIP_ORIGIN, &clip_origin);
		glGetIntegerv(GL_CLIP_DEPTH_MODE, &clip_depthmode);
	}
}
void reshade::opengl::state_block::apply() const
{
#ifndef NDEBUG
	has_state = false;
#endif

	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);

	glUseProgram(_program);
	glBindBuffer(GL_UNIFORM_BUFFER, _ubo);

	for (GLuint i = 0; i < 32; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, _textures2d[i]);
		glBindSampler(i, _samplers[i]);
	}
	glActiveTexture(_active_texture);

	glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);
	glScissor(_scissor_rect[0], _scissor_rect[1], _scissor_rect[2], _scissor_rect[3]);
	glEnableb(GL_SCISSOR_TEST, _scissor_test);
	glEnableb(GL_BLEND, _blend);
	glBlendFunc(_blend_src, _blend_dest);
	glBlendEquationSeparate(_blend_eq_color, _blend_eq_alpha);
	glEnableb(GL_DEPTH_TEST, _depth_test);
	glDepthMask(_depth_mask);
	glDepthFunc(_depth_func);
	glEnableb(GL_STENCIL_TEST, _stencil_test);
	glStencilFunc(_stencil_func, _stencil_ref, _stencil_read_mask);
	glStencilOp(_stencil_op_fail, _stencil_op_zfail, _stencil_op_zpass);
	glStencilMask(_stencil_mask);
	glPolygonMode(GL_FRONT_AND_BACK, _polygon_mode);
	glFrontFace(_frontface);
	glEnableb(GL_CULL_FACE, _cullface);
	glCullFace(_cullface_mode);
	glColorMask(_color_mask[0], _color_mask[1], _color_mask[2], _color_mask[3]);

	glEnableb(GL_FRAMEBUFFER_SRGB, _srgb);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _draw_fbo);

	glReadBuffer(_read_buffer);

	// Number of buffers has to be one if any draw buffer is GL_BACK
	// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawBuffers.xhtml
	int num_draw_buffers = 8;
	if (_draw_buffers[0] == GL_BACK ||
		_draw_buffers[0] == GL_FRONT ||
		_draw_buffers[0] == GL_FRONT_AND_BACK)
		num_draw_buffers = 1;
	glDrawBuffers(num_draw_buffers, reinterpret_cast<const GLenum *>(_draw_buffers));

	if (gl3wProcs.gl.ClipControl != nullptr)
	{
		glClipControl(clip_origin, clip_depthmode);
	}
}
