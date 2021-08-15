/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "opengl_impl_state_block.hpp"

#define glEnableOrDisable(cap, enable) \
	if (enable) { \
		glEnable(cap); \
	} \
	else { \
		glDisable(cap); \
	}

reshade::opengl::state_block::state_block()
{
	memset(this, 0, sizeof(*this));
}

void reshade::opengl::state_block::capture(bool compatibility)
{
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vao);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &_vbo);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &_ibo);

	glGetIntegerv(GL_CURRENT_PROGRAM, &_program);
	glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &_ubo);

	// Technically should capture image bindings here as well ...
	glGetIntegerv(GL_ACTIVE_TEXTURE, &_active_texture);
	for (GLuint i = 0; i < 32; i++)
	{
		glGetIntegeri_v(GL_SAMPLER_BINDING, i, &_samplers[i]);
		glGetIntegeri_v(GL_TEXTURE_BINDING_2D, i, &_textures2d[i]);
	}

	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_draw_fbo);

	glGetIntegerv(GL_VIEWPORT, _viewport);

	glGetIntegerv(GL_READ_BUFFER, &_read_buffer);
	for (GLuint i = 0; i < 8; i++)
	{
		glGetIntegerv(GL_DRAW_BUFFER0 + i, &_draw_buffers[i]);
	}

	_srgb_enable = glIsEnabled(GL_FRAMEBUFFER_SRGB);

	if (compatibility)
		_alpha_test = glIsEnabled(GL_ALPHA_TEST);

	_sample_alpha_to_coverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);
	_blend_enable = glIsEnabled(GL_BLEND);
	_logic_op_enable = glIsEnabled(GL_COLOR_LOGIC_OP);
	glGetIntegerv(GL_BLEND_SRC_RGB, &_blend_src);
	glGetIntegerv(GL_BLEND_DST_RGB, &_blend_dst);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &_blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &_blend_dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &_blend_eq);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &_blend_eq_alpha);
	glGetIntegerv(GL_LOGIC_OP_MODE, &_logic_op);
	glGetFloatv(GL_BLEND_COLOR, _blend_constant);
	glGetBooleanv(GL_COLOR_WRITEMASK, _color_write_mask);

	glGetIntegerv(GL_POLYGON_MODE, &_polygon_mode);
	_cull_enable = glIsEnabled(GL_CULL_FACE);
	glGetIntegerv(GL_CULL_FACE_MODE, &_cull_mode);
	glGetIntegerv(GL_FRONT_FACE, &_front_face);

	_depth_clamp = glIsEnabled(GL_DEPTH_CLAMP);
	_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, _scissor_rect);
	_multisample_enable = glIsEnabled(GL_MULTISAMPLE);
	_line_smooth_enable = glIsEnabled(GL_LINE_SMOOTH);

	_depth_test = glIsEnabled(GL_DEPTH_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);
	glGetIntegerv(GL_DEPTH_FUNC, &_depth_func);
	_stencil_test = glIsEnabled(GL_STENCIL_TEST);
	glGetIntegerv(GL_STENCIL_FUNC, &_stencil_func);
	glGetIntegerv(GL_STENCIL_VALUE_MASK, &_stencil_read_mask);
	glGetIntegerv(GL_STENCIL_WRITEMASK, &_stencil_write_mask);
	glGetIntegerv(GL_STENCIL_REF, &_stencil_reference_value);
	glGetIntegerv(GL_STENCIL_FAIL, &_stencil_op_fail);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &_stencil_op_depth_fail);
	glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &_stencil_op_depth_pass);

	if (gl3wProcs.gl.ClipControl != nullptr)
	{
		glGetIntegerv(GL_CLIP_ORIGIN, &_clip_origin);
		glGetIntegerv(GL_CLIP_DEPTH_MODE, &_clip_depthmode);
	}
}
void reshade::opengl::state_block::apply(bool compatibility) const
{
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

	glBindFramebuffer(GL_READ_FRAMEBUFFER, _read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _draw_fbo);

	glViewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);

	glReadBuffer(_read_buffer);

	// Number of buffers has to be one if any draw buffer is GL_BACK
	// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawBuffers.xhtml
	int num_draw_buffers = 8;
	if (_draw_buffers[0] == GL_BACK ||
		_draw_buffers[0] == GL_FRONT ||
		_draw_buffers[0] == GL_FRONT_AND_BACK)
		num_draw_buffers = 1;
	glDrawBuffers(num_draw_buffers, reinterpret_cast<const GLenum *>(_draw_buffers));

	glEnableOrDisable(GL_FRAMEBUFFER_SRGB, _srgb_enable);

	if (compatibility)
		glEnableOrDisable(GL_ALPHA_TEST, _alpha_test);

	glEnableOrDisable(GL_BLEND, _blend_enable);
	glEnableOrDisable(GL_COLOR_LOGIC_OP, _logic_op_enable);
	glBlendFuncSeparate(_blend_src, _blend_dst, _blend_src_alpha, _blend_dst_alpha);
	glBlendEquationSeparate(_blend_eq, _blend_eq_alpha);
	glLogicOp(_logic_op);
	glBlendColor(_blend_constant[0], _blend_constant[1], _blend_constant[2], _blend_constant[3]);
	glColorMask(_color_write_mask[0], _color_write_mask[1], _color_write_mask[2], _color_write_mask[3]);

	glPolygonMode(GL_FRONT_AND_BACK, _polygon_mode);
	glEnableOrDisable(GL_CULL_FACE, _cull_enable);
	glCullFace(_cull_mode);
	glFrontFace(_front_face);

	glEnableOrDisable(GL_DEPTH_CLAMP, _depth_clamp);
	glEnableOrDisable(GL_SCISSOR_TEST, _scissor_test);
	glScissor(_scissor_rect[0], _scissor_rect[1], _scissor_rect[2], _scissor_rect[3]);
	glEnableOrDisable(GL_MULTISAMPLE, _multisample_enable);
	glEnableOrDisable(GL_LINE_SMOOTH, _line_smooth_enable);

	glEnableOrDisable(GL_DEPTH_TEST, _depth_test);
	glDepthMask(_depth_mask);
	glDepthFunc(_depth_func);
	glEnableOrDisable(GL_STENCIL_TEST, _stencil_test);
	glStencilMask(_stencil_write_mask);
	glStencilOp(_stencil_op_fail, _stencil_op_depth_fail, _stencil_op_depth_pass);
	glStencilFunc(_stencil_func, _stencil_reference_value, _stencil_read_mask);

	if (gl3wProcs.gl.ClipControl != nullptr)
	{
		glClipControl(_clip_origin, _clip_depthmode);
	}
}
