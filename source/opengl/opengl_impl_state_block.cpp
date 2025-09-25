/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_state_block.hpp"

#define gl _device_impl->_dispatch_table

#define glEnableOrDisable(cap, enable) \
	if (enable) { \
		gl.Enable(cap); \
	} \
	else { \
		gl.Disable(cap); \
	}
#define glEnableOrDisablei(cap, index, enable) \
	if (enable) { \
		gl.Enablei(cap, index); \
	} \
	else { \
		gl.Disablei(cap, index); \
	}

reshade::opengl::state_block::state_block(device_impl *device) :
	_device_impl(device)
{
}

void reshade::opengl::state_block::capture()
{
	gl.GetIntegerv(GL_COPY_READ_BUFFER_BINDING, &_copy_read);
	gl.GetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &_copy_write);

	gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &_vao);
	gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING, &_vbo);
	gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &_ibo);

	gl.GetIntegerv(GL_CURRENT_PROGRAM, &_program);

	// Save and restore individual UBO bindings (for compatibility with Yamagi Quake II)
	gl.GetIntegerv(GL_UNIFORM_BUFFER_BINDING, &_active_ubo);
	for (GLuint i = 0; i < 4; i++)
	{
		gl.GetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, i, &_ubo[i]);
#ifndef _WIN64
		gl.GetIntegeri_v(GL_UNIFORM_BUFFER_SIZE, i, reinterpret_cast<GLint *>(&_ubo_sizes[i]));
		gl.GetIntegeri_v(GL_UNIFORM_BUFFER_START, i, reinterpret_cast<GLint *>(&_ubo_offsets[i]));
#else
		gl.GetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, i, &_ubo_sizes[i]);
		gl.GetInteger64i_v(GL_UNIFORM_BUFFER_START, i, &_ubo_offsets[i]);
#endif
	}

	// Technically should capture image bindings here as well ...
	gl.GetIntegerv(GL_ACTIVE_TEXTURE, &_active_texture);
	for (GLuint i = 0; i < 32; i++)
	{
		gl.GetIntegeri_v(GL_SAMPLER_BINDING, i, &_samplers[i]);
		gl.GetIntegeri_v(GL_TEXTURE_BINDING_2D, i, &_textures2d[i]);
	}

	gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_read_fbo);
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_draw_fbo);

	gl.GetIntegerv(GL_VIEWPORT, _viewport);

	gl.GetIntegerv(GL_READ_BUFFER, &_read_buffer);
	for (GLuint i = 0; i < 8; i++)
	{
		gl.GetIntegerv(GL_DRAW_BUFFER0 + i, &_draw_buffers[i]);
	}

	_srgb_enable = gl.IsEnabled(GL_FRAMEBUFFER_SRGB);

	if (_device_impl->get_compatibility_context())
		_alpha_test = gl.IsEnabled(GL_ALPHA_TEST);

	_sample_alpha_to_coverage = gl.IsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);

	for (GLuint i = 0; i < 8; ++i)
	{
		_blend_enable[i] = gl.IsEnabledi(GL_BLEND, i);

		gl.GetIntegeri_v(GL_BLEND_SRC_RGB, i, &_blend_src[i]);
		gl.GetIntegeri_v(GL_BLEND_DST_RGB, i, &_blend_dst[i]);
		gl.GetIntegeri_v(GL_BLEND_SRC_ALPHA, i, &_blend_src_alpha[i]);
		gl.GetIntegeri_v(GL_BLEND_DST_ALPHA, i, &_blend_dst_alpha[i]);
		gl.GetIntegeri_v(GL_BLEND_EQUATION_RGB, i, &_blend_eq[i]);
		gl.GetIntegeri_v(GL_BLEND_EQUATION_ALPHA, i, &_blend_eq_alpha[i]);
	}

	gl.GetFloatv(GL_BLEND_COLOR, _blend_constant);

	_logic_op_enable = gl.IsEnabled(GL_COLOR_LOGIC_OP);
	gl.GetIntegerv(GL_LOGIC_OP_MODE, &_logic_op);

	for (GLuint i = 0; i < 8; ++i)
	{
		gl.GetBooleani_v(GL_COLOR_WRITEMASK, i, _color_write_mask[i]);
	}

	gl.GetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &_sample_mask);

	gl.GetIntegerv(GL_POLYGON_MODE, &_polygon_mode);
	_cull_enable = gl.IsEnabled(GL_CULL_FACE);
	gl.GetIntegerv(GL_CULL_FACE_MODE, &_cull_mode);
	gl.GetIntegerv(GL_FRONT_FACE, &_front_face);

	_depth_clamp = gl.IsEnabled(GL_DEPTH_CLAMP);
	_scissor_test = gl.IsEnabled(GL_SCISSOR_TEST);
	gl.GetIntegerv(GL_SCISSOR_BOX, _scissor_rect);
	_multisample_enable = gl.IsEnabled(GL_MULTISAMPLE);
	_line_smooth_enable = gl.IsEnabled(GL_LINE_SMOOTH);

	_depth_test = gl.IsEnabled(GL_DEPTH_TEST);
	gl.GetBooleanv(GL_DEPTH_WRITEMASK, &_depth_mask);
	gl.GetIntegerv(GL_DEPTH_FUNC, &_depth_func);
	_stencil_test = gl.IsEnabled(GL_STENCIL_TEST);
	gl.GetIntegerv(GL_STENCIL_VALUE_MASK, &_front_stencil_read_mask);
	gl.GetIntegerv(GL_STENCIL_WRITEMASK, &_front_stencil_write_mask);
	gl.GetIntegerv(GL_STENCIL_REF, &_front_stencil_reference_value);
	gl.GetIntegerv(GL_STENCIL_FUNC, &_front_stencil_func);
	gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &_front_stencil_pass_op);
	gl.GetIntegerv(GL_STENCIL_FAIL, &_front_stencil_fail_op);
	gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &_front_stencil_depth_fail_op);
	gl.GetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &_back_stencil_read_mask);
	gl.GetIntegerv(GL_STENCIL_BACK_WRITEMASK, &_back_stencil_write_mask);
	gl.GetIntegerv(GL_STENCIL_BACK_REF, &_back_stencil_reference_value);
	gl.GetIntegerv(GL_STENCIL_BACK_FUNC, &_back_stencil_func);
	gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &_back_stencil_pass_op);
	gl.GetIntegerv(GL_STENCIL_BACK_FAIL, &_back_stencil_fail_op);
	gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &_back_stencil_depth_fail_op);

	if (gl.VERSION_4_5)
	{
		gl.GetIntegerv(GL_CLIP_ORIGIN, &_clip_origin);
		gl.GetIntegerv(GL_CLIP_DEPTH_MODE, &_clip_depthmode);
	}
}
void reshade::opengl::state_block::apply() const
{
	gl.BindBuffer(GL_COPY_READ_BUFFER, _copy_read);
	gl.BindBuffer(GL_COPY_WRITE_BUFFER, _copy_write);

	gl.BindVertexArray(_vao);
	gl.BindBuffer(GL_ARRAY_BUFFER, _vbo);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);

	gl.UseProgram(_program);

	for (GLuint i = 0; i < 4; i++)
	{
		if (_ubo_offsets[i] == 0 && _ubo_sizes[i] == 0)
		{
			gl.BindBufferBase(GL_UNIFORM_BUFFER, i, _ubo[i]);
		}
		else
		{
			gl.BindBufferRange(GL_UNIFORM_BUFFER, i, _ubo[i], _ubo_offsets[i], _ubo_sizes[i]);
		}
	}
	// 'glBindBufferBase' and 'glBindBufferRange' also update the general binding point, so set it after these were called
	gl.BindBuffer(GL_UNIFORM_BUFFER, _active_ubo);

	for (GLuint i = 0; i < 32; i++)
	{
		gl.ActiveTexture(GL_TEXTURE0 + i);
		gl.BindTexture(GL_TEXTURE_2D, _textures2d[i]);
		gl.BindSampler(i, _samplers[i]);
	}
	gl.ActiveTexture(_active_texture);

	gl.BindFramebuffer(GL_READ_FRAMEBUFFER, _read_fbo);
	gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, _draw_fbo);

	gl.Viewport(_viewport[0], _viewport[1], _viewport[2], _viewport[3]);

	gl.ReadBuffer(_read_buffer);

	// Number of buffers has to be one if any draw buffer is GL_BACK
	// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawBuffers.xhtml
	int num_draw_buffers = 8;
	if (_draw_buffers[0] == GL_BACK ||
		_draw_buffers[0] == GL_FRONT ||
		_draw_buffers[0] == GL_FRONT_AND_BACK)
		num_draw_buffers = 1;
	gl.DrawBuffers(num_draw_buffers, reinterpret_cast<const GLenum *>(_draw_buffers));

	glEnableOrDisable(GL_FRAMEBUFFER_SRGB, _srgb_enable);

	if (_device_impl->get_compatibility_context())
		glEnableOrDisable(GL_ALPHA_TEST, _alpha_test);

	glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, _sample_alpha_to_coverage);

	for (GLuint i = 0; i < 8; ++i)
	{
		glEnableOrDisablei(GL_BLEND, i, _blend_enable[i]);
		gl.BlendFuncSeparatei(i, _blend_src[i], _blend_dst[i], _blend_src_alpha[i], _blend_dst_alpha[i]);
		gl.BlendEquationSeparatei(i, _blend_eq[i], _blend_eq_alpha[i]);
	}

	gl.BlendColor(_blend_constant[0], _blend_constant[1], _blend_constant[2], _blend_constant[3]);

	glEnableOrDisable(GL_COLOR_LOGIC_OP, _logic_op_enable);
	gl.LogicOp(_logic_op);

	for (GLuint i = 0; i < 8; ++i)
	{
		gl.ColorMaski(i, _color_write_mask[i][0], _color_write_mask[i][1], _color_write_mask[i][2], _color_write_mask[i][3]);
	}

	gl.SampleMaski(0, _sample_mask);

	gl.PolygonMode(GL_FRONT_AND_BACK, _polygon_mode);
	glEnableOrDisable(GL_CULL_FACE, _cull_enable);
	gl.CullFace(_cull_mode);
	gl.FrontFace(_front_face);

	glEnableOrDisable(GL_DEPTH_CLAMP, _depth_clamp);
	glEnableOrDisable(GL_SCISSOR_TEST, _scissor_test);
	gl.Scissor(_scissor_rect[0], _scissor_rect[1], _scissor_rect[2], _scissor_rect[3]);
	glEnableOrDisable(GL_MULTISAMPLE, _multisample_enable);
	glEnableOrDisable(GL_LINE_SMOOTH, _line_smooth_enable);

	glEnableOrDisable(GL_DEPTH_TEST, _depth_test);
	gl.DepthMask(_depth_mask);
	gl.DepthFunc(_depth_func);

	glEnableOrDisable(GL_STENCIL_TEST, _stencil_test);
	gl.StencilMaskSeparate(GL_FRONT, _front_stencil_write_mask);
	gl.StencilFuncSeparate(GL_FRONT, _front_stencil_func, _front_stencil_reference_value, _front_stencil_read_mask);
	gl.StencilOpSeparate(GL_FRONT, _front_stencil_fail_op, _front_stencil_depth_fail_op, _front_stencil_pass_op);
	gl.StencilMaskSeparate(GL_BACK, _back_stencil_write_mask);
	gl.StencilFuncSeparate(GL_BACK, _back_stencil_func, _back_stencil_reference_value, _back_stencil_read_mask);
	gl.StencilOpSeparate(GL_BACK, _back_stencil_fail_op, _back_stencil_depth_fail_op, _back_stencil_pass_op);

	if (gl.VERSION_4_5)
	{
		gl.ClipControl(_clip_origin, _clip_depthmode);
	}
}
