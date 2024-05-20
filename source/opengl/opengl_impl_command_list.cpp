/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_type_convert.hpp"
#include <algorithm>

#define gl gl3wProcs.gl

#define glEnableOrDisable(cap, enable) \
	if (enable) { \
		gl.Enable(cap); \
	} \
	else { \
		gl.Disable(cap); \
	}

extern "C" void APIENTRY glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);

void reshade::opengl::pipeline_impl::apply(api::pipeline_stage stages) const
{
	if ((stages & api::pipeline_stage::all_shader_stages) != 0)
	{
		gl.UseProgram(program);
	}

	if ((stages & api::pipeline_stage::output_merger) != 0)
	{
		// Set clip space to something consistent
		if (gl.ClipControl != nullptr)
			gl.ClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

		glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, sample_alpha_to_coverage);

		for (GLuint i = 0; i < 8; ++i)
		{
			if (blend_enable[i])
			{
				gl.Enablei(GL_BLEND, i);
			}
			else
			{
				gl.Disablei(GL_BLEND, i);
			}

			gl.BlendFuncSeparatei(i, blend_src[i], blend_dst[i], blend_src_alpha[i], blend_dst_alpha[i]);
			gl.BlendEquationSeparatei(i, blend_eq[i], blend_eq_alpha[i]);
		}

		gl.BlendColor(blend_constant[0], blend_constant[1], blend_constant[2], blend_constant[3]);

		if (logic_op_enable)
		{
			gl.Enable(GL_COLOR_LOGIC_OP);
			gl.LogicOp(logic_op);
		}
		else
		{
			gl.Disable(GL_COLOR_LOGIC_OP);
		}

		for (GLuint i = 0; i < 8; ++i)
		{
			gl.ColorMaski(i, color_write_mask[i][0], color_write_mask[i][1], color_write_mask[i][2], color_write_mask[i][3]);
		}

		gl.SampleMaski(0, sample_mask);
	}

	if ((stages & api::pipeline_stage::rasterizer) != 0)
	{
		gl.PolygonMode(GL_FRONT_AND_BACK, polygon_mode);

		if (cull_mode != GL_NONE)
		{
			gl.Enable(GL_CULL_FACE);
			gl.CullFace(cull_mode);
		}
		else
		{
			gl.Disable(GL_CULL_FACE);
		}

		gl.FrontFace(front_face);

		glEnableOrDisable(GL_DEPTH_CLAMP, depth_clamp);
		glEnableOrDisable(GL_SCISSOR_TEST, scissor_test);
		glEnableOrDisable(GL_MULTISAMPLE, multisample_enable);
		glEnableOrDisable(GL_LINE_SMOOTH, line_smooth_enable);
	}

	if ((stages & api::pipeline_stage::depth_stencil) != 0)
	{
		if (depth_test)
		{
			gl.Enable(GL_DEPTH_TEST);
			gl.DepthMask(depth_mask);
			gl.DepthFunc(depth_func);
		}
		else
		{
			gl.Disable(GL_DEPTH_TEST);
		}

		if (stencil_test)
		{
			gl.Enable(GL_STENCIL_TEST);
			gl.StencilMaskSeparate(GL_FRONT, front_stencil_write_mask);
			gl.StencilMaskSeparate(GL_BACK, back_stencil_write_mask);
			gl.StencilOpSeparate(GL_FRONT, front_stencil_op_fail, front_stencil_op_depth_fail, front_stencil_op_pass);
			gl.StencilOpSeparate(GL_BACK, back_stencil_op_fail, back_stencil_op_depth_fail, back_stencil_op_pass);
			gl.StencilFuncSeparate(GL_FRONT, front_stencil_func, front_stencil_reference_value, front_stencil_read_mask);
			gl.StencilFuncSeparate(GL_BACK, back_stencil_func, back_stencil_reference_value, back_stencil_read_mask);
		}
		else
		{
			gl.Disable(GL_STENCIL_TEST);
		}
	}
}

void reshade::opengl::device_context_impl::barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	GLbitfield barriers = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		if ((old_states[i] & api::resource_usage::unordered_access) != 0)
		{
			if (new_states[i] == api::resource_usage::cpu_access)
			{
				barriers |= (resources[i].handle >> 40) == GL_BUFFER ? GL_BUFFER_UPDATE_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT : GL_TEXTURE_UPDATE_BARRIER_BIT;
			}
			else
			{
				if ((new_states[i] & api::resource_usage::index_buffer) != 0)
					barriers |= GL_ELEMENT_ARRAY_BARRIER_BIT;
				if ((new_states[i] & api::resource_usage::vertex_buffer) != 0)
					barriers |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
				if ((new_states[i] & api::resource_usage::constant_buffer) != 0)
					barriers |= GL_UNIFORM_BARRIER_BIT;
				if ((new_states[i] & api::resource_usage::indirect_argument) != 0)
					barriers |= GL_COMMAND_BARRIER_BIT;

				if ((new_states[i] & api::resource_usage::render_target) != 0)
					barriers |= GL_FRAMEBUFFER_BARRIER_BIT;
				if ((new_states[i] & api::resource_usage::shader_resource) != 0)
					barriers |= GL_TEXTURE_FETCH_BARRIER_BIT;
				if ((new_states[i] & api::resource_usage::unordered_access) != 0)
					barriers |= (resources[i].handle >> 40) == GL_BUFFER ? GL_SHADER_STORAGE_BARRIER_BIT : GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
			}
		}
	}

	if (barriers == 0)
		return;

	gl.MemoryBarrier(barriers);
}

void reshade::opengl::device_context_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
{
	temp_mem<api::resource_view, 8> rtv_handles(count);
	for (uint32_t i = 0; i < count; ++i)
		rtv_handles[i] = rts[i].view;

	api::resource_view depth_stencil_handle = {};
	if (ds != nullptr)
		depth_stencil_handle = ds->view;

	bind_render_targets_and_depth_stencil(count, rtv_handles.p, depth_stencil_handle);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (rts[i].load_op == api::render_pass_load_op::clear)
		{
			gl.ClearBufferfv(GL_COLOR, i, rts[i].clear_color);
		}
	}

	if (ds != nullptr)
	{
		if (ds->depth_load_op == api::render_pass_load_op::clear)
		{
			const auto clear_value = ds->clear_depth;
			gl.ClearBufferfv(GL_DEPTH, 0, &clear_value);
		}
		if (ds->stencil_load_op == api::render_pass_load_op::clear)
		{
			const auto clear_value = static_cast<GLint>(ds->clear_stencil);
			gl.ClearBufferiv(GL_STENCIL, 0, &clear_value);
		}
	}
}
void reshade::opengl::device_context_impl::end_render_pass()
{
}
void reshade::opengl::device_context_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	bind_framebuffer_with_resource_views(GL_FRAMEBUFFER, count, rtvs, dsv);

	bool has_srgb_attachment = false;

	if (count == 0)
	{
		gl.DrawBuffer(GL_NONE);
	}
	else if ((count == 1) && (rtvs[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT)
	{
		gl.DrawBuffer(rtvs[0].handle & 0xFFFFFFFF);
	}
	else
	{
		temp_mem<GLenum, 8> draw_buffers(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;

			const api::format format = _device_impl->get_resource_format(rtvs[i].handle >> 40, rtvs[i].handle & 0xFFFFFFFF);
			if (format != api::format_to_default_typed(format, 0) &&
				format == api::format_to_default_typed(format, 1))
				has_srgb_attachment = true;
		}

		gl.DrawBuffers(count, draw_buffers.p);
	}

	glEnableOrDisable(GL_FRAMEBUFFER_SRGB, has_srgb_attachment);
}

void reshade::opengl::device_context_impl::bind_framebuffer_with_resource(GLenum target, GLenum attachment, api::resource dst, uint32_t dst_subresource, const api::resource_desc &dst_desc)
{
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	if (dst_target == GL_FRAMEBUFFER_DEFAULT)
	{
		gl.BindFramebuffer(target, 0);
		return;
	}

	size_t hash = 0;
	hash_combine(hash, dst.handle);
	hash_combine(hash, dst_subresource);
	// Include texture description, in case a texture handle is reused and framebuffer cache was not invalidated
	hash_combine(hash, dst_desc.texture.width);
	hash_combine(hash, dst_desc.texture.height);
	hash_combine(hash, static_cast<uint32_t>(dst_desc.texture.format));

	if (const uint64_t current_lookup_version = _device_impl->_fbo_lookup_version;
		current_lookup_version != _last_fbo_lookup_version)
	{
		for (const auto &fbo_data : _fbo_lookup)
			gl.DeleteFramebuffers(1, &fbo_data.second);
		_fbo_lookup.clear();
		_last_fbo_lookup_version = current_lookup_version;
	}
	else if (const auto it = _fbo_lookup.find(hash);
		it != _fbo_lookup.end())
	{
		gl.BindFramebuffer(target, it->second);
		return;
	}

	GLuint fbo = 0;
	gl.GenFramebuffers(1, &fbo);
	_fbo_lookup.emplace(hash, fbo);

	gl.BindFramebuffer(target, fbo);

	switch (dst_target)
	{
	case GL_TEXTURE_BUFFER:
	case GL_TEXTURE_1D:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_TEXTURE_RECTANGLE:
		gl.FramebufferTexture(target, attachment, dst_object, dst_subresource);
		break;
	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_TEXTURE_3D:
		gl.FramebufferTextureLayer(target, attachment, dst_object, dst_subresource % dst_desc.texture.levels, dst_subresource / dst_desc.texture.levels);
		break;
	case GL_RENDERBUFFER:
		gl.FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, dst_object);
		break;
	default:
		assert(false);
		return;
	}

	assert(gl.CheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE);
}
void reshade::opengl::device_context_impl::bind_framebuffer_with_resource_views(GLenum target, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if ((count == 1 && (rtvs[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT) || (count == 0 && (dsv.handle == 0 || (dsv.handle >> 40) == GL_FRAMEBUFFER_DEFAULT)))
	{
		assert(dsv.handle == 0 || (dsv.handle >> 40) == GL_FRAMEBUFFER_DEFAULT);

		gl.BindFramebuffer(target, 0);
		_current_window_height = _default_fbo_height;
		return;
	}

	size_t hash = 0;
	for (uint32_t i = 0; i < count; ++i)
		hash_combine(hash, rtvs[i].handle);
	hash_combine(hash, dsv.handle);

	if (const uint64_t current_lookup_version = _device_impl->_fbo_lookup_version;
		current_lookup_version != _last_fbo_lookup_version)
	{
		for (const auto &fbo_data : _fbo_lookup)
			gl.DeleteFramebuffers(1, &fbo_data.second);
		_fbo_lookup.clear();
		_last_fbo_lookup_version = current_lookup_version;
	}
	else if (const auto it = _fbo_lookup.find(hash);
		it != _fbo_lookup.end())
	{
		gl.BindFramebuffer(target, it->second);
		update_current_window_height(count != 0 ? rtvs[0] : dsv);
		return;
	}

	GLuint fbo = 0;
	gl.GenFramebuffers(1, &fbo);
	gl.BindFramebuffer(target, fbo);

	for (uint32_t i = 0; i < count; ++i)
	{
		switch (rtvs[i].handle >> 40)
		{
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			gl.FramebufferTexture(target, GL_COLOR_ATTACHMENT0 + i, rtvs[i].handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			gl.FramebufferRenderbuffer(target, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rtvs[i].handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			gl.BindFramebuffer(target, 0);
			gl.DeleteFramebuffers(1, &fbo);
			return;
		}
	}

	if (dsv.handle != 0)
	{
		const GLenum attachment = is_depth_stencil_format(_device_impl->get_resource_format(dsv.handle >> 40, dsv.handle & 0xFFFFFFFF));

		switch (dsv.handle >> 40)
		{
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			gl.FramebufferTexture(target, attachment, dsv.handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			gl.FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, dsv.handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			gl.BindFramebuffer(target, 0);
			gl.DeleteFramebuffers(1, &fbo);
			return;
		}
	}

	assert(gl.CheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE);

	_fbo_lookup.emplace(hash, fbo);

	update_current_window_height(count != 0 ? rtvs[0] : dsv);
}

void reshade::opengl::device_context_impl::update_default_framebuffer(unsigned int width, unsigned int height)
{
	_default_fbo_width = width;
	_default_fbo_height = _current_window_height = height;
}
void reshade::opengl::device_context_impl::update_current_window_height(api::resource_view default_attachment)
{
	if (default_attachment.handle == 0)
		return;

	const GLenum target = default_attachment.handle >> 40;
	const GLuint object = default_attachment.handle & 0xFFFFFFFF;

	GLint height = 0;

	switch (target)
	{
	case GL_TEXTURE_BUFFER:
	case GL_TEXTURE_1D:
	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_TEXTURE_3D:
	case GL_TEXTURE_CUBE_MAP:
	case GL_TEXTURE_CUBE_MAP_ARRAY:
	case GL_TEXTURE_RECTANGLE:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		if (_device_impl->_supports_dsa)
		{
			gl.GetTextureLevelParameteriv(object, 0, GL_TEXTURE_HEIGHT, &height);
		}
		else
		{
			GLuint prev_binding = 0;
			gl.GetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_binding));
			if (object != prev_binding)
				gl.BindTexture(target, object);

			GLenum level_target = target;
			if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
				level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

			gl.GetTexLevelParameteriv(level_target, 0, GL_TEXTURE_HEIGHT, &height);

			if (object != prev_binding)
				gl.BindTexture(target, prev_binding);
		}
		break;
	case GL_RENDERBUFFER:
		if (_device_impl->_supports_dsa)
		{
			gl.GetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_HEIGHT, &height);
		}
		else
		{
			GLuint prev_binding = 0;
			gl.GetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_binding));
			if (object != prev_binding)
				gl.BindRenderbuffer(GL_RENDERBUFFER, object);

			gl.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);

			if (object != prev_binding)
				gl.BindRenderbuffer(GL_RENDERBUFFER, prev_binding);
		}
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		height = _default_fbo_height;
		break;
	default:
		assert(false);
		return;
	}

	_current_window_height = height;
}

void reshade::opengl::device_context_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	if (pipeline.handle == 0)
		return;

	// Special case for application handles
	if ((pipeline.handle >> 40) == GL_PROGRAM)
	{
		assert((stages & ~api::pipeline_stage::all_shader_stages) == 0);
		gl.UseProgram(pipeline.handle & 0xFFFFFFFF);
		return;
	}
	if ((pipeline.handle >> 40) == GL_VERTEX_ARRAY)
	{
		assert((stages & ~api::pipeline_stage::input_assembler) == 0);
		gl.BindVertexArray(pipeline.handle & 0xFFFFFFFF);
		_current_vao_dirty = false;
		return;
	}

	// Always disable alpha test in case the application set that (fixes broken GUI rendering in Quake)
	if (_device_impl->_compatibility_context)
		gl.Disable(0x0BC0 /* GL_ALPHA_TEST */);

	const auto pipeline_object = reinterpret_cast<pipeline_impl *>(pipeline.handle);
	pipeline_object->apply(stages);

	if ((stages & api::pipeline_stage::input_assembler) != 0)
	{
		if (pipeline_object->topology != api::primitive_topology::undefined)
			bind_pipeline_state(api::dynamic_state::primitive_topology, static_cast<uint32_t>(pipeline_object->topology));

		_current_vao_dirty = false;

		if (const uint64_t current_lookup_version = _device_impl->_vao_lookup_version;
			current_lookup_version != _last_vao_lookup_version)
		{
			for (const auto &vao_data : _vao_lookup)
				gl.DeleteVertexArrays(1, &vao_data.second);
			_vao_lookup.clear();
			_last_vao_lookup_version = current_lookup_version;
		}
		else if (const auto it = _vao_lookup.find(static_cast<size_t>(pipeline.handle));
			it != _vao_lookup.end())
		{
			gl.BindVertexArray(it->second);
			return;
		}

		GLuint vao = 0;
		gl.GenVertexArrays(1, &vao);
		_vao_lookup.emplace(static_cast<size_t>(pipeline.handle), vao);

		gl.BindVertexArray(vao);

		for (const api::input_element &element : pipeline_object->input_elements)
		{
			gl.EnableVertexAttribArray(element.location);

			GLint attrib_size = 0;
			GLboolean normalized = GL_FALSE;
			const GLenum attrib_type = convert_attrib_format(element.format, attrib_size, normalized);
			if (normalized || attrib_type == GL_FLOAT || attrib_type == GL_HALF_FLOAT)
				gl.VertexAttribFormat(element.location, attrib_size, attrib_type, normalized, element.offset);
			else
				gl.VertexAttribIFormat(element.location, attrib_size, attrib_type, element.offset);
			gl.VertexAttribBinding(element.location, element.buffer_binding);
			gl.VertexBindingDivisor(element.buffer_binding, element.instance_step_rate);
		}
	}
}
void reshade::opengl::device_context_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::alpha_test_enable:
			glEnableOrDisable(0x0BC0 /* GL_ALPHA_TEST */, values[i]);
			break;
		case api::dynamic_state::srgb_write_enable:
			glEnableOrDisable(GL_FRAMEBUFFER_SRGB, values[i]);
			break;
		case api::dynamic_state::primitive_topology:
			_current_prim_mode = static_cast<GLenum>(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			if (_current_prim_mode == GL_PATCHES)
				gl.PatchParameteri(GL_PATCH_VERTICES, values[i] - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp));
			break;
		case api::dynamic_state::alpha_to_coverage_enable:
			glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, values[i]);
			break;
		case api::dynamic_state::blend_enable:
			glEnableOrDisable(GL_BLEND, values[i]);
			break;
		case api::dynamic_state::logic_op_enable:
			glEnableOrDisable(GL_COLOR_LOGIC_OP, values[i]);
			break;
		case api::dynamic_state::source_color_blend_factor:
			if (i + 3 < count &&
				states[i + 1] == api::dynamic_state::dest_color_blend_factor &&
				states[i + 2] == api::dynamic_state::source_alpha_blend_factor &&
				states[i + 3] == api::dynamic_state::dest_alpha_blend_factor)
			{
				gl.BlendFuncSeparate(
					convert_blend_factor(static_cast<api::blend_factor>(values[i + 0])),
					convert_blend_factor(static_cast<api::blend_factor>(values[i + 1])),
					convert_blend_factor(static_cast<api::blend_factor>(values[i + 2])),
					convert_blend_factor(static_cast<api::blend_factor>(values[i + 3])));
				i += 3;
			}
			else
			{
				GLint prev_dest_color_factor = GL_NONE;
				GLint prev_source_alpha_factor = GL_NONE;
				GLint prev_dest_alpha_factor = GL_NONE;
				gl.GetIntegerv(GL_BLEND_DST_RGB, &prev_dest_color_factor);
				gl.GetIntegerv(GL_BLEND_SRC_ALPHA, &prev_source_alpha_factor);
				gl.GetIntegerv(GL_BLEND_DST_ALPHA, &prev_dest_alpha_factor);

				gl.BlendFuncSeparate(
					convert_blend_factor(static_cast<api::blend_factor>(values[i])),
					prev_dest_color_factor,
					prev_source_alpha_factor,
					prev_dest_alpha_factor);
			}
			break;
		case api::dynamic_state::dest_color_blend_factor:
			{
				GLint prev_source_color_blend_factor = GL_NONE;
				GLint prev_source_alpha_factor = GL_NONE;
				GLint prev_dest_alpha_factor = GL_NONE;
				gl.GetIntegerv(GL_BLEND_SRC_RGB, &prev_source_color_blend_factor);
				gl.GetIntegerv(GL_BLEND_SRC_ALPHA, &prev_source_alpha_factor);
				gl.GetIntegerv(GL_BLEND_DST_ALPHA, &prev_dest_alpha_factor);

				gl.BlendFuncSeparate(
					prev_source_color_blend_factor,
					convert_blend_factor(static_cast<api::blend_factor>(values[i])),
					prev_source_alpha_factor,
					prev_dest_alpha_factor);
			}
			break;
		case api::dynamic_state::color_blend_op:
			if (i + 1 < count &&
				states[i + 1] == api::dynamic_state::alpha_blend_op)
			{
				gl.BlendEquationSeparate(
					convert_blend_op(static_cast<api::blend_op>(values[i + 0])),
					convert_blend_op(static_cast<api::blend_op>(values[i + 1])));
				i += 1;
			}
			else
			{
				GLint prev_blend_op = GL_NONE;
				gl.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &prev_blend_op);

				gl.BlendEquationSeparate(convert_blend_op(static_cast<api::blend_op>(values[i])), prev_blend_op);
			}
			break;
		case api::dynamic_state::source_alpha_blend_factor:
			{
				GLint prev_source_color_blend_factor = GL_NONE;
				GLint prev_dest_color_blend_factor = GL_NONE;
				GLint prev_dest_alpha_factor = GL_NONE;
				gl.GetIntegerv(GL_BLEND_SRC_RGB, &prev_source_color_blend_factor);
				gl.GetIntegerv(GL_BLEND_DST_RGB, &prev_dest_color_blend_factor);
				gl.GetIntegerv(GL_BLEND_DST_ALPHA, &prev_dest_alpha_factor);

				gl.BlendFuncSeparate(
					prev_source_color_blend_factor,
					prev_dest_color_blend_factor,
					convert_blend_factor(static_cast<api::blend_factor>(values[i])),
					prev_dest_alpha_factor);
			}
			break;
		case api::dynamic_state::dest_alpha_blend_factor:
			{
				GLint prev_source_color_blend_factor = GL_NONE;
				GLint prev_dest_color_blend_factor = GL_NONE;
				GLint prev_source_alpha_factor = GL_NONE;
				gl.GetIntegerv(GL_BLEND_SRC_RGB, &prev_source_color_blend_factor);
				gl.GetIntegerv(GL_BLEND_DST_RGB, &prev_dest_color_blend_factor);
				gl.GetIntegerv(GL_BLEND_SRC_ALPHA, &prev_source_alpha_factor);

				gl.BlendFuncSeparate(
					prev_source_color_blend_factor,
					prev_dest_color_blend_factor,
					prev_source_alpha_factor,
					convert_blend_factor(static_cast<api::blend_factor>(values[i])));
			}
			break;
		case api::dynamic_state::alpha_blend_op:
			{
				GLint prev_blend_op = GL_NONE;
				gl.GetIntegerv(GL_BLEND_EQUATION_RGB, &prev_blend_op);

				gl.BlendEquationSeparate(prev_blend_op, convert_blend_op(static_cast<api::blend_op>(values[i])));
			}
			break;
		case api::dynamic_state::logic_op:
			gl.LogicOp(convert_logic_op(static_cast<api::logic_op>(values[i])));
			break;
		case api::dynamic_state::blend_constant:
			gl.BlendColor(((values[i]) & 0xFF) / 255.0f, ((values[i] >> 4) & 0xFF) / 255.0f, ((values[i] >> 8) & 0xFF) / 255.0f, ((values[i] >> 12) & 0xFF) / 255.0f);
			break;
		case api::dynamic_state::render_target_write_mask:
			gl.ColorMask(values[i] & 0x1, (values[i] >> 1) & 0x1, (values[i] >> 2) & 0x1, (values[i] >> 3) & 0x1);
			break;
		case api::dynamic_state::fill_mode:
			gl.PolygonMode(GL_FRONT_AND_BACK, convert_fill_mode(static_cast<api::fill_mode>(values[i])));
			break;
		case api::dynamic_state::cull_mode:
			glEnableOrDisable(GL_CULL_FACE, convert_cull_mode(static_cast<api::cull_mode>(values[i])));
			break;
		case api::dynamic_state::front_counter_clockwise:
			gl.FrontFace(values[i] ? GL_CCW : GL_CW);
			break;
		case api::dynamic_state::depth_bias:
			{
				GLfloat prev_depth_bias_slope_scaled = 0.0f;
				gl.GetFloatv(GL_POLYGON_OFFSET_FACTOR, &prev_depth_bias_slope_scaled);

				gl.PolygonOffset(prev_depth_bias_slope_scaled, *reinterpret_cast<const GLfloat *>(&values[i]));
			}
			break;
		case api::dynamic_state::depth_bias_slope_scaled:
			if (i + 1 < count &&
				states[i + 1] == api::dynamic_state::depth_bias)
			{
				gl.PolygonOffset(
					*reinterpret_cast<const GLfloat *>(&values[i + 0]),
					*reinterpret_cast<const GLfloat *>(&values[i + 1]));
				i += 1;
			}
			else
			{
				GLfloat prev_depth_bias = 0.0f;
				gl.GetFloatv(GL_POLYGON_OFFSET_UNITS, &prev_depth_bias);

				gl.PolygonOffset(*reinterpret_cast<const GLfloat *>(&values[i]), prev_depth_bias);
			}
			break;
		case api::dynamic_state::depth_clip_enable:
			glEnableOrDisable(GL_DEPTH_CLAMP, !values[i]);
			break;
		case api::dynamic_state::scissor_enable:
			glEnableOrDisable(GL_SCISSOR_TEST, values[i]);
			break;
		case api::dynamic_state::multisample_enable:
			glEnableOrDisable(GL_MULTISAMPLE, values[i]);
			break;
		case api::dynamic_state::antialiased_line_enable:
			glEnableOrDisable(GL_LINE_SMOOTH, values[i]);
			break;
		case api::dynamic_state::depth_enable:
			glEnableOrDisable(GL_DEPTH_TEST, values[i]);
			break;
		case api::dynamic_state::depth_write_mask:
			gl.DepthMask(values[i] ? GL_TRUE : GL_FALSE);
			break;
		case api::dynamic_state::depth_func:
			gl.DepthFunc(convert_compare_op(static_cast<api::compare_op>(values[i])));
			break;
		case api::dynamic_state::stencil_enable:
			glEnableOrDisable(GL_STENCIL_TEST, values[i]);
			break;
		case api::dynamic_state::front_stencil_read_mask:
			{
				GLint prev_stencil_func = GL_NONE;
				GLint prev_stencil_reference_value = 0;
				gl.GetIntegerv(GL_STENCIL_FUNC, &prev_stencil_func);
				gl.GetIntegerv(GL_STENCIL_REF, &prev_stencil_reference_value);

				gl.StencilFuncSeparate(GL_FRONT, prev_stencil_func, prev_stencil_reference_value, values[i]);
			}
			break;
		case api::dynamic_state::front_stencil_write_mask:
			gl.StencilMaskSeparate(GL_FRONT, values[i]);
			break;
		case api::dynamic_state::front_stencil_reference_value:
			{
				GLint prev_stencil_func = GL_NONE;
				GLint prev_stencil_read_mask = 0;
				gl.GetIntegerv(GL_STENCIL_FUNC, &prev_stencil_func);
				gl.GetIntegerv(GL_STENCIL_VALUE_MASK, &prev_stencil_read_mask);

				gl.StencilFuncSeparate(GL_FRONT, prev_stencil_func, values[i], prev_stencil_read_mask);
			}
			break;
		case api::dynamic_state::front_stencil_func:
			if (i + 2 < count &&
				states[i + 1] == api::dynamic_state::front_stencil_reference_value &&
				states[i + 2] == api::dynamic_state::front_stencil_read_mask)
			{
				gl.StencilFuncSeparate(
					GL_FRONT,
					convert_compare_op(static_cast<api::compare_op>(values[i])),
					values[i + 1],
					values[i + 2]);
				i += 2;
			}
			else
			{
				GLint prev_stencil_reference_value = GL_NONE;
				GLint prev_stencil_read_mask = 0;
				gl.GetIntegerv(GL_STENCIL_REF, &prev_stencil_reference_value);
				gl.GetIntegerv(GL_STENCIL_VALUE_MASK, &prev_stencil_read_mask);

				gl.StencilFuncSeparate(GL_FRONT, convert_compare_op(static_cast<api::compare_op>(values[i])), prev_stencil_reference_value, prev_stencil_read_mask);
			}
			break;
		case api::dynamic_state::front_stencil_pass_op:
			{
				GLint prev_stencil_fail_op = GL_NONE;
				GLint prev_stencil_depth_fail_op = 0;
				gl.GetIntegerv(GL_STENCIL_FAIL, &prev_stencil_fail_op);
				gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &prev_stencil_depth_fail_op);

				gl.StencilOpSeparate(GL_FRONT, prev_stencil_fail_op, prev_stencil_depth_fail_op, convert_stencil_op(static_cast<api::stencil_op>(values[i])));
			}
			break;
		case api::dynamic_state::front_stencil_fail_op:
			if (i + 2 < count &&
				states[i + 1] == api::dynamic_state::front_stencil_depth_fail_op &&
				states[i + 2] == api::dynamic_state::front_stencil_pass_op)
			{
				gl.StencilOpSeparate(
					GL_FRONT,
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 0])),
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 1])),
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 2])));
				i += 2;
			}
			else
			{
				GLint prev_stencil_depth_fail_op = 0;
				GLint prev_stencil_pass_op = GL_NONE;
				gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &prev_stencil_depth_fail_op);
				gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &prev_stencil_pass_op);

				gl.StencilOpSeparate(GL_FRONT, convert_stencil_op(static_cast<api::stencil_op>(values[i])), prev_stencil_depth_fail_op, prev_stencil_pass_op);
			}
			break;
		case api::dynamic_state::front_stencil_depth_fail_op:
			{
				GLint prev_stencil_fail_op = GL_NONE;
				GLint prev_stencil_pass_op = GL_NONE;
				gl.GetIntegerv(GL_STENCIL_FAIL, &prev_stencil_fail_op);
				gl.GetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &prev_stencil_pass_op);

				gl.StencilOpSeparate(GL_FRONT, prev_stencil_fail_op, convert_stencil_op(static_cast<api::stencil_op>(values[i])), prev_stencil_pass_op);
			}
			break;
		case api::dynamic_state::back_stencil_read_mask:
			{
				GLint prev_stencil_func = GL_NONE;
				GLint prev_stencil_reference_value = 0;
				gl.GetIntegerv(GL_STENCIL_BACK_FUNC, &prev_stencil_func);
				gl.GetIntegerv(GL_STENCIL_BACK_REF, &prev_stencil_reference_value);

				gl.StencilFuncSeparate(GL_BACK, prev_stencil_func, prev_stencil_reference_value, values[i]);
			}
			break;
		case api::dynamic_state::back_stencil_write_mask:
			gl.StencilMaskSeparate(GL_BACK, values[i]);
			break;
		case api::dynamic_state::back_stencil_reference_value:
			{
				GLint prev_stencil_func = GL_NONE;
				GLint prev_stencil_read_mask = 0;
				gl.GetIntegerv(GL_STENCIL_BACK_FUNC, &prev_stencil_func);
				gl.GetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &prev_stencil_read_mask);

				gl.StencilFuncSeparate(GL_BACK, prev_stencil_func, values[i], prev_stencil_read_mask);
			}
			break;
		case api::dynamic_state::back_stencil_func:
			if (i + 2 < count &&
				states[i + 1] == api::dynamic_state::back_stencil_reference_value &&
				states[i + 2] == api::dynamic_state::back_stencil_read_mask)
			{
				gl.StencilFuncSeparate(
					GL_BACK,
					convert_compare_op(static_cast<api::compare_op>(values[i])),
					values[i + 1],
					values[i + 2]);
				i += 2;
			}
			else
			{
				GLint prev_stencil_reference_value = GL_NONE;
				GLint prev_stencil_read_mask = 0;
				gl.GetIntegerv(GL_STENCIL_BACK_REF, &prev_stencil_reference_value);
				gl.GetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &prev_stencil_read_mask);

				gl.StencilFuncSeparate(GL_BACK, convert_compare_op(static_cast<api::compare_op>(values[i])), prev_stencil_reference_value, prev_stencil_read_mask);
			}
			break;
		case api::dynamic_state::back_stencil_pass_op:
			{
				GLint prev_stencil_fail_op = GL_NONE;
				GLint prev_stencil_depth_fail_op = 0;
				gl.GetIntegerv(GL_STENCIL_BACK_FAIL, &prev_stencil_fail_op);
				gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &prev_stencil_depth_fail_op);

				gl.StencilOpSeparate(GL_BACK, prev_stencil_fail_op, prev_stencil_depth_fail_op, convert_stencil_op(static_cast<api::stencil_op>(values[i])));
			}
			break;
		case api::dynamic_state::back_stencil_fail_op:
			if (i + 2 < count &&
				states[i + 1] == api::dynamic_state::back_stencil_depth_fail_op &&
				states[i + 2] == api::dynamic_state::back_stencil_pass_op)
			{
				gl.StencilOpSeparate(
					GL_BACK,
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 0])),
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 1])),
					convert_stencil_op(static_cast<api::stencil_op>(values[i + 2])));
				i += 2;
			}
			else
			{
				GLint prev_stencil_depth_fail_op = 0;
				GLint prev_stencil_pass_op = GL_NONE;
				gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &prev_stencil_depth_fail_op);
				gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &prev_stencil_pass_op);

				gl.StencilOpSeparate(GL_BACK, convert_stencil_op(static_cast<api::stencil_op>(values[i])), prev_stencil_depth_fail_op, prev_stencil_pass_op);
			}
			break;
		case api::dynamic_state::back_stencil_depth_fail_op:
			{
				GLint prev_stencil_fail_op = GL_NONE;
				GLint prev_stencil_pass_op = GL_NONE;
				gl.GetIntegerv(GL_STENCIL_BACK_FAIL, &prev_stencil_fail_op);
				gl.GetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &prev_stencil_pass_op);

				gl.StencilOpSeparate(GL_BACK, prev_stencil_fail_op, convert_stencil_op(static_cast<api::stencil_op>(values[i])), prev_stencil_pass_op);
			}
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::opengl::device_context_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		gl.ViewportIndexedf(first + i, viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height);
		gl.DepthRangeIndexed(first + i, static_cast<GLdouble>(viewports[i].min_depth), static_cast<GLdouble>(viewports[i].max_depth));
	}
}
void reshade::opengl::device_context_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	GLint clip_origin = GL_LOWER_LEFT;
	if (gl.ClipControl != nullptr)
		gl.GetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (clip_origin == GL_UPPER_LEFT)
		{
			gl.ScissorIndexed(first + i, rects[i].left, rects[i].top, rects[i].width(), rects[i].height());
		}
		else
		{
			assert(_current_window_height != 0);
			gl.ScissorIndexed(first + i, rects[i].left, _current_window_height - rects[i].bottom, rects[i].width(), rects[i].height());
		}
	}
}

void reshade::opengl::device_context_impl::push_constants(api::shader_stage, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	if (layout.handle == 0 || layout == global_pipeline_layout || reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding == UINT32_MAX)
	{
		first /= 4;

		switch (layout_param)
		{
		case 4:
			if ((count % (4 * 4)) == 0)
			{
				gl.UniformMatrix4fv(first, count / (4 * 4), GL_FALSE, static_cast<const GLfloat *>(values));
				break;
			}
			if ((count % (3 * 3)) == 0)
			{
				gl.UniformMatrix3fv(first, count / (3 * 3), GL_FALSE, static_cast<const GLfloat *>(values));
				break;
			}
			if ((count % 4) == 0)
			{
				gl.Uniform4fv(first, count / 4, static_cast<const GLfloat *>(values));
				break;
			}
			if ((count % 3) == 0)
			{
				gl.Uniform3fv(first, count / 3, static_cast<const GLfloat *>(values));
				break;
			}
			if ((count % 2) == 0)
			{
				gl.Uniform2fv(first, count / 2, static_cast<const GLfloat *>(values));
				break;
			}
			{
				gl.Uniform1fv(first, count / 1, static_cast<const GLfloat *>(values));
				break;
			}
		case 5:
			if ((count % 4) == 0)
			{
				gl.Uniform4iv(first, count / 4, static_cast<const GLint *>(values));
				break;
			}
			if ((count % 3) == 0)
			{
				gl.Uniform3iv(first, count / 3, static_cast<const GLint *>(values));
				break;
			}
			if ((count % 2) == 0)
			{
				gl.Uniform2iv(first, count / 2, static_cast<const GLint *>(values));
				break;
			}
			{
				gl.Uniform1iv(first, count / 1, static_cast<const GLint *>(values));
				break;
			}
		case 6:
			if ((count % 4) == 0)
			{
				gl.Uniform4uiv(first, count / 4, static_cast<const GLuint *>(values));
				break;
			}
			if ((count % 3) == 0)
			{
				gl.Uniform3uiv(first, count / 3, static_cast<const GLuint *>(values));
				break;
			}
			if ((count % 2) == 0)
			{
				gl.Uniform2uiv(first, count / 2, static_cast<const GLuint *>(values));
				break;
			}
			{
				gl.Uniform1uiv(first, count / 1, static_cast<const GLuint *>(values));
				break;
			}
		default:
			assert(false);
			break;
		}
		return;
	}

	assert(first == 0);

	const GLuint push_constants_size = (first + count) * sizeof(uint32_t);

	// Binds the push constant buffer to the requested indexed binding point as well as the generic binding point
	gl.BindBufferBase(GL_UNIFORM_BUFFER, reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding, _push_constants);

	// Recreate the buffer data store in case it is no longer large enough
	if (push_constants_size > _push_constants_size)
	{
		gl.BufferData(GL_UNIFORM_BUFFER, push_constants_size, first == 0 ? values : nullptr, GL_DYNAMIC_DRAW);
		if (first != 0)
			gl.BufferSubData(GL_UNIFORM_BUFFER, first * sizeof(uint32_t), count * sizeof(uint32_t), values);

		_device_impl->set_resource_name(make_resource_handle(GL_BUFFER, _push_constants), "Push constants");

		_push_constants_size = push_constants_size;
	}
	// Otherwise discard the previous range (so driver can return a new memory region to avoid stalls) and update it with the new constants
	else if (void *const data = gl.MapBufferRange(GL_UNIFORM_BUFFER, first * sizeof(uint32_t), count * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT))
	{
		std::memcpy(data, values, count * sizeof(uint32_t));
		gl.UnmapBuffer(GL_UNIFORM_BUFFER);
	}
}
void reshade::opengl::device_context_impl::push_descriptors(api::shader_stage, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_table_update &update)
{
	assert(update.table.handle == 0 && update.array_offset == 0);

	const uint32_t first = update.binding;
	assert(layout.handle == 0 || layout == global_pipeline_layout || update.binding >= reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding);

	switch (update.type)
	{
	case api::descriptor_type::sampler:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[i];
			gl.BindSampler(first + i, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[i];
			if (descriptor.view.handle == 0)
				continue;

			if (_device_impl->_supports_dsa)
			{
				gl.BindTextureUnit(first + i, descriptor.view.handle & 0xFFFFFFFF);
			}
			else
			{
				gl.ActiveTexture(GL_TEXTURE0 + first + i);
				gl.BindTexture(descriptor.view.handle >> 40, descriptor.view.handle & 0xFFFFFFFF);
			}

			gl.BindSampler(first + i, descriptor.sampler.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::buffer_shader_resource_view:
	case api::descriptor_type::texture_shader_resource_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[i];
			if (descriptor.handle == 0)
				continue;

			if (_device_impl->_supports_dsa)
			{
				gl.BindTextureUnit(first + i, descriptor.handle & 0xFFFFFFFF);
			}
			else
			{
				gl.ActiveTexture(GL_TEXTURE0 + first + i);
				gl.BindTexture(descriptor.handle >> 40, descriptor.handle & 0xFFFFFFFF);
			}
		}
		break;
	case api::descriptor_type::buffer_unordered_access_view:
	case api::descriptor_type::texture_unordered_access_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[i];
			if (descriptor.handle == 0)
				continue;

			const GLenum target = descriptor.handle >> 40;
			const GLuint object = descriptor.handle & 0xFFFFFFFF;

			gl.BindImageTexture(first + i, descriptor.handle & 0xFFFFFFFF, 0, GL_FALSE, 0, GL_READ_WRITE, convert_format(_device_impl->get_resource_format(target, object)));
		}
		break;
	case api::descriptor_type::constant_buffer:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::buffer_range *>(update.descriptors)[i];
			if (descriptor.size == UINT64_MAX)
			{
				assert(descriptor.offset == 0);

				gl.BindBufferBase(GL_UNIFORM_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF);
			}
			else
			{
				assert(descriptor.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && descriptor.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

				gl.BindBufferRange(GL_UNIFORM_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF, static_cast<GLintptr>(descriptor.offset), static_cast<GLsizeiptr>(descriptor.size));
			}
		}
		break;
	case api::descriptor_type::shader_storage_buffer:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::buffer_range *>(update.descriptors)[i];
			if (descriptor.size == UINT64_MAX)
			{
				assert(descriptor.offset == 0);

				gl.BindBufferBase(GL_SHADER_STORAGE_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF);
			}
			else
			{
				assert(descriptor.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && descriptor.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

				gl.BindBufferRange(GL_SHADER_STORAGE_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF, static_cast<GLintptr>(descriptor.offset), static_cast<GLsizeiptr>(descriptor.size));
			}
		}
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::opengl::device_context_impl::bind_descriptor_tables(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto table_impl = reinterpret_cast<const descriptor_table_impl *>(tables[i].handle);

		push_descriptors(
			stages,
			layout,
			first + i,
			api::descriptor_table_update { {}, table_impl->base_binding, 0, table_impl->count, table_impl->type, table_impl->descriptors.data() });
	}
}

void reshade::opengl::device_context_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset == 0);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.handle & 0xFFFFFFFF);

	switch (index_size)
	{
	case 1:
		_current_index_type = GL_UNSIGNED_BYTE;
		break;
	case 2:
		_current_index_type = GL_UNSIGNED_SHORT;
		break;
	case 4:
		_current_index_type = GL_UNSIGNED_INT;
		break;
	default:
		assert(buffer.handle == 0);
		break;
	}

	_current_ibo_dirty = false;
}
void reshade::opengl::device_context_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (buffers[i].handle == 0)
			continue;

		assert(offsets[i] <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		gl.BindVertexBuffer(first + i, buffers[i].handle & 0xFFFFFFFF, static_cast<GLintptr>(offsets[i]), strides[i]);
	}

	_current_vbo_dirty = false;
}
void reshade::opengl::device_context_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes, const api::resource *, const uint64_t *)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (offsets == nullptr || max_sizes == nullptr || max_sizes[i] == UINT64_MAX)
		{
			assert(offsets == nullptr || offsets[i] == 0);

			gl.BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, first + i, buffers[i].handle & 0xFFFFFFFF);
		}
		else
		{
			assert(offsets[i] <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			assert(max_sizes[i] <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

			gl.BindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, first + i, buffers[i].handle & 0xFFFFFFFF, static_cast<GLintptr>(offsets[i]), static_cast<GLsizeiptr>(max_sizes[i]));
		}
	}
}

void reshade::opengl::device_context_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	gl.DrawArraysInstancedBaseInstance(_current_prim_mode, first_vertex, vertex_count, instance_count, first_instance);
}
void reshade::opengl::device_context_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	gl.DrawElementsInstancedBaseVertexBaseInstance(_current_prim_mode, index_count, _current_index_type, reinterpret_cast<void *>(static_cast<uintptr_t>(first_index) * get_index_type_size(_current_index_type)), instance_count, vertex_offset, first_instance);
}
void reshade::opengl::device_context_impl::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	gl.DispatchCompute(group_count_x, group_count_y, group_count_z);
}
void reshade::opengl::device_context_impl::dispatch_mesh(uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::opengl::device_context_impl::dispatch_rays(api::resource, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, api::resource, uint64_t, uint64_t, uint64_t, uint32_t, uint32_t, uint32_t)
{
	assert(false);
}
void reshade::opengl::device_context_impl::draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	switch (type)
	{
	case api::indirect_command::draw:
		gl.BindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		gl.MultiDrawArraysIndirect(_current_prim_mode, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case api::indirect_command::draw_indexed:
		gl.BindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		gl.MultiDrawElementsIndirect(_current_prim_mode, _current_index_type, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case api::indirect_command::dispatch:
		gl.BindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		for (GLuint i = 0; i < draw_count; ++i)
		{
			assert(offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

			gl.DispatchComputeIndirect(static_cast<GLintptr>(offset + static_cast<uint64_t>(i) * stride));
		}
		break;
	default:
		assert(false);
		break;
	}
}

void reshade::opengl::device_context_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = _device_impl->get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, UINT64_MAX);
	}
	else
	{
		for (uint32_t layer = 0; layer < desc.texture.depth_or_layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::filter_mode::min_mag_mip_point);
			}
		}
	}
}
void reshade::opengl::device_context_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);

	const GLuint src_object = src.handle & 0xFFFFFFFF;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	assert(src_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) &&
		   dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && (size == UINT64_MAX || size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max())));

	if (_device_impl->_supports_dsa)
	{
		if (UINT64_MAX == size)
		{
#ifndef _WIN64
			GLint max_size = 0;
			gl.GetNamedBufferParameteriv(src_object, GL_BUFFER_SIZE, &max_size);
#else
			GLint64 max_size = 0;
			gl.GetNamedBufferParameteri64v(src_object, GL_BUFFER_SIZE, &max_size);
#endif
			size = max_size;
		}

		gl.CopyNamedBufferSubData(src_object, dst_object, static_cast<GLintptr>(src_offset), static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));
	}
	else
	{
		gl.BindBuffer(GL_COPY_READ_BUFFER, src_object);
		gl.BindBuffer(GL_COPY_WRITE_BUFFER, dst_object);

		if (UINT64_MAX == size)
		{
#ifndef _WIN64
			GLint max_size = 0;
			gl.GetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &max_size);
#else
			GLint64 max_size = 0;
			gl.GetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &max_size);
#endif
			size = max_size;
		}

		gl.CopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(src_offset), static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));
	}
}
void reshade::opengl::device_context_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box)
{
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_unpack_lsb = GL_FALSE;
	GLint prev_unpack_swap = GL_FALSE;
	GLint prev_unpack_alignment = 0;
	GLint prev_unpack_row_length = 0;
	GLint prev_unpack_image_height = 0;
	GLint prev_unpack_skip_rows = 0;
	GLint prev_unpack_skip_pixels = 0;
	GLint prev_unpack_skip_images = 0;
	gl.GetIntegerv(GL_UNPACK_LSB_FIRST, &prev_unpack_lsb);
	gl.GetIntegerv(GL_UNPACK_SWAP_BYTES, &prev_unpack_swap);
	gl.GetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
	gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_unpack_row_length);
	gl.GetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &prev_unpack_image_height);
	gl.GetIntegerv(GL_UNPACK_SKIP_ROWS, &prev_unpack_skip_rows);
	gl.GetIntegerv(GL_UNPACK_SKIP_PIXELS, &prev_unpack_skip_pixels);
	gl.GetIntegerv(GL_UNPACK_SKIP_IMAGES, &prev_unpack_skip_images);

	GLuint prev_unpack_binding = 0;
	gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, reinterpret_cast<GLint *>(&prev_unpack_binding));

	// Bind source buffer
	gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, src.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	gl.PixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	gl.PixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
	gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, slice_height);
	gl.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	gl.PixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

	const api::resource_desc dst_desc = _device_impl->get_resource_desc(dst);

	GLuint xoffset, yoffset, zoffset, width, height, depth;
	if (dst_box != nullptr)
	{
		xoffset = dst_box->left;
		yoffset = dst_box->top;
		zoffset = dst_box->front;
		width   = dst_box->width();
		height  = dst_box->height();
		depth   = dst_box->depth();
	}
	else
	{
		xoffset = yoffset = zoffset = 0;
		width   = dst_desc.texture.width;
		height  = dst_desc.texture.height;
		depth   = dst_desc.texture.depth_or_layers;
	}

	GLenum type, format = convert_upload_format(dst_desc.texture.format, type);

	if (dst_target == GL_FRAMEBUFFER_DEFAULT)
	{
		assert(dst_subresource == 0);
		assert(xoffset == 0 && yoffset == 0 && zoffset == 0 && depth == 1);

		GLuint prev_binding = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_binding));
		if (0 != prev_binding)
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		gl.DrawBuffer(dst_object);

		// This is deprecated and not available in core contexts!
		assert(_device_impl->_compatibility_context);
		glDrawPixels(width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));

		if (0 != prev_binding)
			gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_binding);
	}
	else if (dst_target == GL_RENDERBUFFER)
	{
		assert(dst_subresource == 0);
		assert(xoffset == 0 && yoffset == 0 && zoffset == 0 && depth == 1);

		GLuint prev_rbo_binding = 0;
		gl.GetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_rbo_binding));
		GLuint prev_fbo_binding = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_fbo_binding));

		if (dst_object != prev_rbo_binding)
			gl.BindRenderbuffer(GL_RENDERBUFFER, dst_object);

		bind_framebuffer_with_resource(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dst, dst_subresource, {});

		gl.DrawBuffer(GL_COLOR_ATTACHMENT0);

		// This is deprecated and not available in core contexts!
		assert(_device_impl->_compatibility_context);
		glDrawPixels(width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));

		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_fbo_binding);
		if (dst_object != prev_rbo_binding)
			gl.BindRenderbuffer(GL_RENDERBUFFER, prev_rbo_binding);
	}
	else
	{
		GLuint prev_binding = 0;
		gl.GetIntegerv(get_binding_for_target(dst_target), reinterpret_cast<GLint *>(&prev_binding));
		if (dst_object != prev_binding)
			gl.BindTexture(dst_target, dst_object);

		const GLuint level = dst_subresource % dst_desc.texture.levels;
		      GLuint layer = dst_subresource / dst_desc.texture.levels;

		if (dst_box == nullptr)
		{
			width  = std::max(1u, width >> level);
			height = std::max(1u, height >> level);
			depth  = (dst_desc.type == api::resource_type::texture_3d ? std::max(1u, depth >> level) : 1u);
		}

		GLenum level_target = dst_target;
		if (depth == 1 && (dst_target == GL_TEXTURE_CUBE_MAP || dst_target == GL_TEXTURE_CUBE_MAP_ARRAY))
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		const auto row_pitch = api::format_row_pitch(dst_desc.texture.format, row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(dst_desc.texture.format, row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * static_cast<size_t>(slice_pitch);

		assert(total_image_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		switch (level_target)
		{
		case GL_TEXTURE_1D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
				gl.TexSubImage1D(level_target, level, xoffset, width, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			else
				gl.CompressedTexSubImage1D(level_target, level, xoffset, width, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			break;
		case GL_TEXTURE_1D_ARRAY:
			yoffset += layer;
			[[fallthrough]];
		case GL_TEXTURE_2D:
		case GL_TEXTURE_RECTANGLE:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
				gl.TexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			else
				gl.CompressedTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			break;
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_CUBE_MAP:
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			zoffset += layer;
			[[fallthrough]];
		case GL_TEXTURE_3D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
				gl.TexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			else
				gl.CompressedTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			break;
		}

		if (dst_object != prev_binding)
			gl.BindTexture(dst_target, prev_binding);
	}

	// Restore previous state from application
	gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, prev_unpack_binding);

	gl.PixelStorei(GL_UNPACK_LSB_FIRST, prev_unpack_lsb);
	gl.PixelStorei(GL_UNPACK_SWAP_BYTES, prev_unpack_swap);
	gl.PixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
	gl.PixelStorei(GL_UNPACK_ROW_LENGTH, prev_unpack_row_length);
	gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, prev_unpack_image_height);
	gl.PixelStorei(GL_UNPACK_SKIP_ROWS, prev_unpack_skip_rows);
	gl.PixelStorei(GL_UNPACK_SKIP_PIXELS, prev_unpack_skip_pixels);
	gl.PixelStorei(GL_UNPACK_SKIP_IMAGES, prev_unpack_skip_images);
}
void reshade::opengl::device_context_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode filter)
{
	assert(src.handle != 0 && dst.handle != 0);

	const api::resource_desc src_desc = _device_impl->get_resource_desc(src);
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	const api::resource_desc dst_desc = _device_impl->get_resource_desc(dst);
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	GLint src_region[6] = {};
	if (src_box != nullptr)
	{
		std::copy_n(&src_box->left, 6, src_region);
	}
	else
	{
		src_region[3] = static_cast<GLint>(std::max(1u, src_desc.texture.width >> (src_subresource % src_desc.texture.levels)));
		src_region[4] = static_cast<GLint>(std::max(1u, src_desc.texture.height >> (src_subresource % src_desc.texture.levels)));
		src_region[5] = static_cast<GLint>(src_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(src_desc.texture.depth_or_layers) >> (src_subresource % src_desc.texture.levels)) : 1u);
	}

	GLint dst_region[6] = {};
	if (dst_box != nullptr)
	{
		std::copy_n(&dst_box->left, 6, dst_region);
	}
	else
	{
		dst_region[3] = static_cast<GLint>(std::max(1u, dst_desc.texture.width >> (dst_subresource % dst_desc.texture.levels)));
		dst_region[4] = static_cast<GLint>(std::max(1u, dst_desc.texture.height >> (dst_subresource % dst_desc.texture.levels)));
		dst_region[5] = static_cast<GLint>(dst_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(dst_desc.texture.depth_or_layers) >> (dst_subresource % dst_desc.texture.levels)) : 1u);
	}

	const bool flip_y = (src_target == GL_FRAMEBUFFER_DEFAULT) || (dst_target == GL_FRAMEBUFFER_DEFAULT);

	if (!flip_y &&
		src_desc.texture.samples == dst_desc.texture.samples &&
		(src_region[3] - src_region[0]) == (dst_region[3] - dst_region[0]) &&
		(src_region[4] - src_region[1]) == (dst_region[4] - dst_region[1]) &&
		(src_region[5] - src_region[2]) == (dst_region[5] - dst_region[2]))
	{
		assert(filter == api::filter_mode::min_mag_mip_point);

		if (src_box != nullptr)
		{
			src_region[3] -= src_box->left;
			src_region[4] -= src_box->top;
			src_region[5] -= src_box->front;
		}

		gl.CopyImageSubData(
			src_object, src_target, src_subresource % src_desc.texture.levels, src_region[0], src_region[1], src_region[2] + (src_subresource / src_desc.texture.levels),
			dst_object, dst_target, dst_subresource % dst_desc.texture.levels, dst_region[0], dst_region[1], dst_region[2] + (dst_subresource / src_desc.texture.levels),
			src_region[3], src_region[4], src_region[5]);
	}
	else
	{
		GLuint prev_read_binding = 0;
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_read_binding));
		GLuint prev_draw_binding = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_draw_binding));

		GLboolean prev_scissor_test = gl.IsEnabled(GL_SCISSOR_TEST);
		GLboolean prev_framebuffer_srgb = gl.IsEnabled(GL_FRAMEBUFFER_SRGB);

		const GLenum copy_depth = is_depth_stencil_format(src_desc.texture.format);
		assert(copy_depth != GL_STENCIL_ATTACHMENT && is_depth_stencil_format(dst_desc.texture.format) == copy_depth);

		bind_framebuffer_with_resource(GL_READ_FRAMEBUFFER, copy_depth != GL_NONE ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, src, src_subresource, src_desc);
		bind_framebuffer_with_resource(GL_DRAW_FRAMEBUFFER, copy_depth != GL_NONE ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, dst, dst_subresource, dst_desc);

		if (prev_scissor_test)
			gl.Disable(GL_SCISSOR_TEST);
		if (prev_framebuffer_srgb)
			gl.Disable(GL_FRAMEBUFFER_SRGB);

		assert(src_region[2] == 0 && dst_region[2] == 0 && src_region[5] == 1 && dst_region[5] == 1);
		gl.BlitFramebuffer(
			src_region[0], src_region[1], src_region[3], src_region[4],
			dst_region[0], dst_region[flip_y ? 4 : 1], dst_region[3], dst_region[flip_y ? 1 : 4],
			copy_depth != GL_NONE ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT,
			// Must be nearest filtering for depth or stencil attachments
			(filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point) && copy_depth == GL_NONE ? GL_LINEAR : GL_NEAREST);

		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_binding);
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_binding);

		if (prev_scissor_test)
			gl.Enable(GL_SCISSOR_TEST);
		if (prev_framebuffer_srgb)
			gl.Enable(GL_FRAMEBUFFER_SRGB);
	}
}
void reshade::opengl::device_context_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_pack_lsb = GL_FALSE;
	GLint prev_pack_swap = GL_FALSE;
	GLint prev_pack_alignment = 0;
	GLint prev_pack_row_length = 0;
	GLint prev_pack_image_height = 0;
	GLint prev_pack_skip_rows = 0;
	GLint prev_pack_skip_pixels = 0;
	GLint prev_pack_skip_images = 0;
	gl.GetIntegerv(GL_PACK_LSB_FIRST, &prev_pack_lsb);
	gl.GetIntegerv(GL_PACK_SWAP_BYTES, &prev_pack_swap);
	gl.GetIntegerv(GL_PACK_ALIGNMENT, &prev_pack_alignment);
	gl.GetIntegerv(GL_PACK_ROW_LENGTH, &prev_pack_row_length);
	gl.GetIntegerv(GL_PACK_IMAGE_HEIGHT, &prev_pack_image_height);
	gl.GetIntegerv(GL_PACK_SKIP_ROWS, &prev_pack_skip_rows);
	gl.GetIntegerv(GL_PACK_SKIP_PIXELS, &prev_pack_skip_pixels);
	gl.GetIntegerv(GL_PACK_SKIP_IMAGES, &prev_pack_skip_images);

	GLuint prev_pack_binding = 0;
	gl.GetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, reinterpret_cast<GLint *>(&prev_pack_binding));

	// Bind destination buffer
	gl.BindBuffer(GL_PIXEL_PACK_BUFFER, dst.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	gl.PixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	gl.PixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
	gl.PixelStorei(GL_PACK_ROW_LENGTH, row_length);
	gl.PixelStorei(GL_PACK_SKIP_ROWS, 0);
	gl.PixelStorei(GL_PACK_SKIP_PIXELS, 0);
	gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
	gl.PixelStorei(GL_PACK_SKIP_IMAGES, 0);
	gl.PixelStorei(GL_PACK_IMAGE_HEIGHT, slice_height);

	const api::resource_desc src_desc = _device_impl->get_resource_desc(src);

	GLuint xoffset, yoffset, zoffset, width, height, depth;
	if (src_box != nullptr)
	{
		xoffset = src_box->left;
		yoffset = src_box->top;
		zoffset = src_box->front;
		width   = src_box->width();
		height  = src_box->height();
		depth   = src_box->depth();
	}
	else
	{
		xoffset = yoffset = zoffset = 0;
		width   = src_desc.texture.width;
		height  = src_desc.texture.height;
		depth   = src_desc.texture.depth_or_layers;
	}

	GLenum type, format = convert_upload_format(src_desc.texture.format, type);

	if (src_target == GL_FRAMEBUFFER_DEFAULT)
	{
		assert(src_subresource == 0);
		assert(zoffset == 0 && depth == 1);

		GLuint prev_binding = 0;
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_binding));
		if (0 != prev_binding)
			gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		gl.ReadBuffer(src_object);

		gl.ReadPixels(xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));

		if (0 != prev_binding)
			gl.BindFramebuffer(GL_READ_FRAMEBUFFER, prev_binding);
	}
	else if (src_target == GL_RENDERBUFFER)
	{
		assert(src_subresource == 0);
		assert(zoffset == 0 && depth == 1);

		GLuint prev_rbo_binding = 0;
		gl.GetIntegerv(GL_RENDERBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_rbo_binding));
		GLuint prev_fbo_binding = 0;
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_fbo_binding));

		if (src_object != prev_rbo_binding)
			gl.BindRenderbuffer(GL_RENDERBUFFER, src_object);

		bind_framebuffer_with_resource(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, src, src_subresource, {});

		gl.ReadBuffer(GL_COLOR_ATTACHMENT0);

		gl.ReadPixels(xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));

		gl.BindFramebuffer(GL_READ_FRAMEBUFFER, prev_fbo_binding);
		if (src_object != prev_rbo_binding)
			gl.BindRenderbuffer(GL_RENDERBUFFER, prev_rbo_binding);
	}
	else
	{
		GLuint prev_binding = 0;
		gl.GetIntegerv(get_binding_for_target(src_target), reinterpret_cast<GLint *>(&prev_binding));
		if (src_object != prev_binding)
			gl.BindTexture(src_target, src_object);

		const GLuint level = src_subresource % src_desc.texture.levels;
		      GLuint layer = src_subresource / src_desc.texture.levels;

		if (src_box == nullptr)
		{
			width  = std::max(1u, width >> level);
			height = std::max(1u, height >> level);
			depth  = (src_desc.type == api::resource_type::texture_3d ? std::max(1u, depth >> level) : 1u);
		}

		GLenum level_target = src_target;
		if (depth == 1 && (src_target == GL_TEXTURE_CUBE_MAP || src_target == GL_TEXTURE_CUBE_MAP_ARRAY))
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		const auto row_pitch = api::format_row_pitch(src_desc.texture.format, row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(src_desc.texture.format, row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * static_cast<size_t>(slice_pitch);

		assert(total_image_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		if (src_box == nullptr)
		{
			assert(layer == 0);

			gl.GetTexImage(level_target, level, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}
		else if (_device_impl->_supports_dsa)
		{
			switch (src_target)
			{
			case GL_TEXTURE_1D_ARRAY:
				yoffset += layer;
				break;
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_CUBE_MAP:
			case GL_TEXTURE_CUBE_MAP_ARRAY:
				zoffset += layer;
				break;
			}

			gl.GetTextureSubImage(src_object, level, xoffset, yoffset, zoffset, width, height, depth, format, type, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}

		if (src_object != prev_binding)
			gl.BindTexture(src_target, prev_binding);
	}

	// Restore previous state from application
	gl.BindBuffer(GL_PIXEL_PACK_BUFFER, prev_pack_binding);

	gl.PixelStorei(GL_PACK_LSB_FIRST, prev_pack_lsb);
	gl.PixelStorei(GL_PACK_SWAP_BYTES, prev_pack_swap);
	gl.PixelStorei(GL_PACK_ALIGNMENT, prev_pack_alignment);
	gl.PixelStorei(GL_PACK_ROW_LENGTH, prev_pack_row_length);
	gl.PixelStorei(GL_PACK_IMAGE_HEIGHT, prev_pack_image_height);
	gl.PixelStorei(GL_PACK_SKIP_ROWS, prev_pack_skip_rows);
	gl.PixelStorei(GL_PACK_SKIP_PIXELS, prev_pack_skip_pixels);
	gl.PixelStorei(GL_PACK_SKIP_IMAGES, prev_pack_skip_images);
}
void reshade::opengl::device_context_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, api::format)
{
	api::subresource_box dst_box;
	dst_box.left  = dst_x;
	dst_box.top   = dst_y;
	dst_box.front = dst_z;

	if (src_box != nullptr)
	{
		dst_box.right  = dst_x + src_box->width();
		dst_box.bottom = dst_y + src_box->height();
		dst_box.back   = dst_z + src_box->depth();
	}
	else
	{
		const api::resource_desc desc = _device_impl->get_resource_desc(dst);

		dst_box.right  = dst_x + std::max(1u, desc.texture.width >> (dst_subresource % desc.texture.levels));
		dst_box.bottom = dst_y + std::max(1u, desc.texture.height >> (dst_subresource % desc.texture.levels));
		dst_box.back   = dst_z + 1;
	}

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, &dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::opengl::device_context_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	assert(dsv.handle != 0 && rect_count == 0); // Clearing rectangles is not supported

	GLuint prev_draw_binding = 0;
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_draw_binding));

	const bool binding_has_this_dsv = _device_impl->_supports_dsa && (dsv == _device_impl->get_framebuffer_attachment(prev_draw_binding, GL_DEPTH, 0));
	if (!binding_has_this_dsv)
		bind_framebuffer_with_resource_views(GL_DRAW_FRAMEBUFFER, 0, nullptr, dsv);

	if (depth != nullptr && stencil != nullptr)
	{
		gl.ClearBufferfi(GL_DEPTH_STENCIL, 0, *depth, *stencil);
	}
	else if (depth != nullptr)
	{
		const auto clear_value = *depth;
		gl.ClearBufferfv(GL_DEPTH, 0, &clear_value);
	}
	else if (stencil != nullptr)
	{
		const auto clear_value = static_cast<GLint>(*stencil);
		gl.ClearBufferiv(GL_STENCIL, 0, &clear_value);
	}

	if (!binding_has_this_dsv)
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_binding);
}
void reshade::opengl::device_context_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *)
{
	assert(rtv.handle != 0 && rect_count == 0); // Clearing rectangles is not supported

	GLuint prev_draw_binding = 0;
	gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_draw_binding));

	const bool binding_has_this_rtv = _device_impl->_supports_dsa && (rtv == _device_impl->get_framebuffer_attachment(prev_draw_binding, GL_COLOR, 0));
	if (!binding_has_this_rtv)
		bind_framebuffer_with_resource_views(GL_DRAW_FRAMEBUFFER, 1, &rtv, { 0 });

	gl.ClearBufferfv(GL_COLOR, 0, color);

	if (!binding_has_this_rtv)
		gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_binding);
}
void reshade::opengl::device_context_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const api::rect *)
{
	assert(false);
}
void reshade::opengl::device_context_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const api::rect *)
{
	assert(false);
}

void reshade::opengl::device_context_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	const GLenum target = srv.handle >> 40;
	const GLuint object = srv.handle & 0xFFFFFFFF;

	gl.BindSampler(0, _device_impl->_mipmap_sampler);
	gl.ActiveTexture(GL_TEXTURE0); // src
	gl.BindTexture(target, object);

#if 0
	gl.GenerateMipmap(target);
#else
	// Use custom mipmap generation implementation because 'glGenerateMipmap' generates shifted results
	gl.UseProgram(_device_impl->_mipmap_program);

	GLuint levels = 0;
	gl.GetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, reinterpret_cast<GLint *>(&levels));
	GLuint base_width = 0;
	gl.GetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, reinterpret_cast<GLint *>(&base_width));
	GLuint base_height = 0;
	gl.GetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&base_height));
	GLenum internal_format = GL_NONE;
	gl.GetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_format));

	assert(internal_format != GL_SRGB8 && internal_format != GL_SRGB8_ALPHA8);

	for (GLuint level = 1; level < levels; ++level)
	{
		const GLuint width = std::max(1u, base_width >> level);
		const GLuint height = std::max(1u, base_height >> level);

		gl.Uniform3f(0 /* info */, 1.0f / width, 1.0f / height, static_cast<float>(level - 1));
		gl.BindImageTexture(1 /* dest */, object, level, GL_FALSE, 0, GL_WRITE_ONLY, internal_format);

		gl.DispatchCompute(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Ensure next iteration reads the data written by this iteration
		gl.MemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	}
#endif
}

void reshade::opengl::device_context_impl::begin_query(api::query_heap heap, api::query_type type, uint32_t index)
{
	assert(heap.handle != 0);

	switch (type)
	{
	default:
		gl.BeginQuery(convert_query_type(type), reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index]);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		gl.BeginQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index], static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::opengl::device_context_impl::end_query(api::query_heap heap, api::query_type type, uint32_t index)
{
	assert(heap.handle != 0);

	switch (type)
	{
	default:
		gl.EndQuery(convert_query_type(type));
		break;
	case api::query_type::timestamp:
		gl.QueryCounter(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index], GL_TIMESTAMP);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		gl.EndQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::opengl::device_context_impl::copy_query_heap_results(api::query_heap heap, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	assert(heap.handle != 0);

	for (size_t i = 0; i < count; ++i)
	{
		assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		gl.GetQueryBufferObjectui64v(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[first + i], dst.handle & 0xFFFFFFFF, GL_QUERY_RESULT_NO_WAIT, static_cast<GLintptr>(dst_offset + i * stride));
	}
}

void reshade::opengl::device_context_impl::copy_acceleration_structure(api::resource_view, api::resource_view, api::acceleration_structure_copy_mode)
{
	assert(false);
}
void reshade::opengl::device_context_impl::build_acceleration_structure(api::acceleration_structure_type, api::acceleration_structure_build_flags, uint32_t, const api::acceleration_structure_build_input *, api::resource, uint64_t, api::resource_view, api::resource_view, api::acceleration_structure_build_mode)
{
	assert(false);
}

void reshade::opengl::device_context_impl::begin_debug_event(const char *label, const float[4])
{
	assert(label != nullptr);

	gl.PushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, label);
}
void reshade::opengl::device_context_impl::end_debug_event()
{
	gl.PopDebugGroup();
}
void reshade::opengl::device_context_impl::insert_debug_marker(const char *label, const float[4])
{
	assert(label != nullptr);

	gl.DebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
}
