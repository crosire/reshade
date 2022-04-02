/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_type_convert.hpp"
#include <algorithm>

#define glEnableOrDisable(cap, enable) \
	if (enable) { \
		glEnable(cap); \
	} \
	else { \
		glDisable(cap); \
	}

void reshade::opengl::pipeline_impl::apply(api::pipeline_stage stages) const
{
	if ((stages & api::pipeline_stage::all_shader_stages) != 0)
	{
		glUseProgram(program);
	}

	if ((stages & api::pipeline_stage::output_merger) != 0)
	{
		glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, sample_alpha_to_coverage);

		for (GLuint i = 0; i < 8; ++i)
		{
			if (blend_enable[i])
			{
				glEnablei(GL_BLEND, i);
			}
			else
			{
				glDisablei(GL_BLEND, i);
			}

			glBlendFuncSeparatei(i, blend_src[i], blend_dst[i], blend_src_alpha[i], blend_dst_alpha[i]);
			glBlendEquationSeparatei(i, blend_eq[i], blend_eq_alpha[i]);
		}

		glBlendColor(blend_constant[0], blend_constant[1], blend_constant[2], blend_constant[3]);

		if (logic_op_enable)
		{
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(logic_op);
		}
		else
		{
			glDisable(GL_COLOR_LOGIC_OP);
		}

		for (GLuint i = 0; i < 8; ++i)
		{
			glColorMaski(i, color_write_mask[i][0], color_write_mask[i][1], color_write_mask[i][2], color_write_mask[i][3]);
		}
	}

	if ((stages & api::pipeline_stage::input_assembler) != 0)
	{
		glBindVertexArray(vao);

		// Rebuild vertex array object every time
		// This fixes weird artifacts in melonDS and the first Call of Duty
		for (const api::input_element &element : input_elements)
		{
			glEnableVertexAttribArray(element.location);

			GLint attrib_size = 0;
			GLboolean normalized = GL_FALSE;
			const GLenum attrib_format = convert_attrib_format(element.format, attrib_size, normalized);
#if 1
			glVertexAttribFormat(element.location, attrib_size, attrib_format, normalized, element.offset);
			glVertexAttribBinding(element.location, element.buffer_binding);
#else
			glVertexAttribPointer(element.location, attrib_size, attrib_format, normalized, element.stride, reinterpret_cast<const void *>(static_cast<uintptr_t>(element.offset)));
#endif
			glVertexBindingDivisor(element.buffer_binding, element.instance_step_rate);
		}

		glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

		if (prim_mode == GL_PATCHES)
		{
			glPatchParameteri(GL_PATCH_VERTICES, patch_vertices);
		}
	}

	if ((stages & api::pipeline_stage::rasterizer) != 0)
	{
		if (cull_mode != GL_NONE)
		{
			glEnable(GL_CULL_FACE);
			glCullFace(cull_mode);
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}

		glFrontFace(front_face);

		glEnableOrDisable(GL_DEPTH_CLAMP, depth_clamp);
		glEnableOrDisable(GL_SCISSOR_TEST, scissor_test);
		glEnableOrDisable(GL_MULTISAMPLE, multisample_enable);
		glEnableOrDisable(GL_LINE_SMOOTH, line_smooth_enable);
	}

	if ((stages & api::pipeline_stage::depth_stencil) != 0)
	{
		if (depth_test)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthMask(depth_mask);
			glDepthFunc(depth_func);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		if (stencil_test)
		{
			glEnable(GL_STENCIL_TEST);
			glStencilMask(stencil_write_mask);
			glStencilOpSeparate(GL_BACK, back_stencil_op_fail, back_stencil_op_depth_fail, back_stencil_op_pass);
			glStencilOpSeparate(GL_FRONT, front_stencil_op_fail, front_stencil_op_depth_fail, front_stencil_op_pass);
			glStencilFuncSeparate(GL_BACK, back_stencil_func, stencil_reference_value, stencil_read_mask);
			glStencilFuncSeparate(GL_FRONT, front_stencil_func, stencil_reference_value, stencil_read_mask);
		}
		else
		{
			glDisable(GL_STENCIL_TEST);
		}

		glSampleMaski(0, sample_mask);
	}
}

void reshade::opengl::device_impl::barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
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

	glMemoryBarrier(barriers);
}

void reshade::opengl::device_impl::begin_render_pass(uint32_t count, const api::render_pass_render_target_desc *rts, const api::render_pass_depth_stencil_desc *ds)
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
			glClearBufferfv(GL_COLOR, i, rts[i].clear_color);
		}
	}

	if (ds != nullptr)
	{
		if (ds->depth_load_op == api::render_pass_load_op::clear)
		{
			const auto clear_value = ds->clear_depth;
			glClearBufferfv(GL_DEPTH, 0, &clear_value);
		}
		if (ds->stencil_load_op == api::render_pass_load_op::clear)
		{
			const auto clear_value = static_cast<GLint>(ds->clear_stencil);
			glClearBufferiv(GL_STENCIL, 0, &clear_value);
		}
	}
}
void reshade::opengl::device_impl::end_render_pass()
{
}
void reshade::opengl::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	bind_framebuffer_with_resource_views(GL_FRAMEBUFFER, count, rtvs, dsv);

	bool has_srgb_attachment = false;

	if (count == 0)
	{
		glDrawBuffer(GL_NONE);
	}
	else if ((count == 1) && (rtvs[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT)
	{
		glDrawBuffer(rtvs[0].handle & 0xFFFFFFFF);
	}
	else
	{
		temp_mem<GLenum, 8> draw_buffers(count);
		for (uint32_t i = 0; i < count; ++i)
		{
			draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;

			if ((rtvs[i].handle >> 32) & 0x2)
				has_srgb_attachment = true;
		}

		glDrawBuffers(count, draw_buffers.p);
	}

	glEnableOrDisable(GL_FRAMEBUFFER_SRGB, has_srgb_attachment);
}

void reshade::opengl::device_impl::bind_framebuffer_with_resource(GLenum target, GLenum attachment, api::resource dst, uint32_t dst_subresource, const api::resource_desc &dst_desc)
{
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	if (dst_target == GL_FRAMEBUFFER_DEFAULT)
	{
		glBindFramebuffer(target, 0);
		return;
	}

	size_t hash = 0;
	hash_combine(hash, dst.handle);
	hash_combine(hash, dst_subresource);

	if (const auto it = _fbo_lookup.find(hash);
		it != _fbo_lookup.end())
	{
		glBindFramebuffer(target, it->second);
		return;
	}

	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	_fbo_lookup.emplace(hash, fbo);

	glBindFramebuffer(target, fbo);

	switch (dst_target)
	{
	case GL_TEXTURE_BUFFER:
	case GL_TEXTURE_1D:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_2D_MULTISAMPLE:
	case GL_TEXTURE_RECTANGLE:
		glFramebufferTexture(target, attachment, dst_object, dst_subresource);
		break;
	case GL_TEXTURE_1D_ARRAY:
	case GL_TEXTURE_2D_ARRAY:
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
	case GL_TEXTURE_3D:
		glFramebufferTextureLayer(target, attachment, dst_object, dst_subresource % dst_desc.texture.levels, dst_subresource / dst_desc.texture.levels);
		break;
	case GL_RENDERBUFFER:
		glFramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, dst_object);
		break;
	default:
		assert(false);
		return;
	}

	assert(glCheckFramebufferStatus(target));
}
void reshade::opengl::device_impl::bind_framebuffer_with_resource_views(GLenum target, uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if ((count == 0 || ((count == 1) && (rtvs[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT)) && (dsv.handle == 0 || (dsv.handle >> 40) == GL_FRAMEBUFFER_DEFAULT))
	{
		glBindFramebuffer(target, 0);
		update_current_window_height(0);
		return;
	}

	size_t hash = 0;
	for (uint32_t i = 0; i < count; ++i)
		hash_combine(hash, rtvs[i].handle);
	hash_combine(hash, dsv.handle);

	if (const auto it = _fbo_lookup.find(hash);
		it != _fbo_lookup.end())
	{
		glBindFramebuffer(target, it->second);
		update_current_window_height(it->second);
		return;
	}

	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(target, fbo);

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
			glFramebufferTexture(target, GL_COLOR_ATTACHMENT0 + i, rtvs[i].handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(target, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rtvs[i].handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			glBindFramebuffer(target, 0);
			glDeleteFramebuffers(1, &fbo);
			return;
		}
	}

	if (dsv.handle != 0)
	{
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
			glFramebufferTexture(target, GL_DEPTH_STENCIL_ATTACHMENT, dsv.handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(target, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsv.handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			glBindFramebuffer(target, 0);
			glDeleteFramebuffers(1, &fbo);
			return;
		}
	}

	assert(glCheckFramebufferStatus(target));

	_fbo_lookup.emplace(hash, fbo);

	update_current_window_height(fbo);
}

void reshade::opengl::device_impl::bind_pipeline(api::pipeline_stage stages, api::pipeline pipeline)
{
	// Special case for application pipeline handles
	if ((pipeline.handle >> 40) == GL_PROGRAM)
	{
		assert((stages & ~api::pipeline_stage::all_shader_stages) == 0);
		glUseProgram(pipeline.handle & 0xFFFFFFFF);
		return;
	}

	assert(pipeline.handle != 0);

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

	// Always disable alpha test in case the application set that (fixes broken GUI rendering in Quake)
	if (_compatibility_context)
		glDisable(GL_ALPHA_TEST);

	reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply(stages);

	if ((stages & api::pipeline_stage::input_assembler) != 0)
		_current_prim_mode = reinterpret_cast<pipeline_impl *>(pipeline.handle)->prim_mode;
}
void reshade::opengl::device_impl::bind_pipeline_states(uint32_t count, const api::dynamic_state *states, const uint32_t *values)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::dynamic_state::alpha_test_enable:
			glEnableOrDisable(GL_ALPHA_TEST, values[i]);
			break;
		case api::dynamic_state::srgb_write_enable:
			glEnableOrDisable(GL_FRAMEBUFFER_SRGB, values[i]);
			break;
		case api::dynamic_state::primitive_topology:
			_current_prim_mode = values[i];
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
		case api::dynamic_state::logic_op:
			glLogicOp(convert_logic_op(static_cast<api::logic_op>(values[i])));
			break;
		case api::dynamic_state::render_target_write_mask:
			glColorMask(values[i] & 0x1, (values[i] >> 1) & 0x1, (values[i] >> 2) & 0x1, (values[i] >> 3) & 0x1);
			break;
		case api::dynamic_state::fill_mode:
			glPolygonMode(GL_FRONT_AND_BACK, convert_fill_mode(static_cast<api::fill_mode>(values[i])));
			break;
		case api::dynamic_state::cull_mode:
			glEnableOrDisable(GL_CULL_FACE, convert_cull_mode(static_cast<api::cull_mode>(values[i])));
			break;
		case api::dynamic_state::front_counter_clockwise:
			glFrontFace(values[i] ? GL_CCW : GL_CW);
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
			glDepthMask(values[i] ? GL_TRUE : GL_FALSE);
			break;
		case api::dynamic_state::stencil_enable:
			glEnableOrDisable(GL_STENCIL_TEST, values[i]);
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::opengl::device_impl::bind_viewports(uint32_t first, uint32_t count, const api::viewport *viewports)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		glViewportIndexedf(first + i, viewports[i].x, viewports[i].y, viewports[i].width, viewports[i].height);
		glDepthRangeIndexed(first + i, static_cast<GLdouble>(viewports[i].min_depth), static_cast<GLdouble>(viewports[i].max_depth));
	}
}
void reshade::opengl::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const api::rect *rects)
{
	GLint clip_origin = GL_LOWER_LEFT;
	if (gl3wProcs.gl.ClipControl != nullptr)
		glGetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (clip_origin == GL_UPPER_LEFT)
		{
			glScissorIndexed(first + i, rects[i].left, rects[i].top, rects[i].width(), rects[i].height());
		}
		else
		{
			assert(_current_window_height != 0);
			glScissorIndexed(first + i, rects[i].left, _current_window_height - rects[i].bottom, rects[i].width(), rects[i].height());
		}
	}
}

void reshade::opengl::device_impl::push_constants(api::shader_stage, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	const GLuint push_constants_binding = (layout.handle != 0 && layout != global_pipeline_layout) ?
		reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding : 0;

	// Binds the push constant buffer to the requested indexed binding point as well as the generic binding point
	glBindBufferBase(GL_UNIFORM_BUFFER, push_constants_binding, _push_constants);

	// Recreate the buffer data store in case it is no longer large enough
	if (count > _push_constants_size)
	{
		glBufferData(GL_UNIFORM_BUFFER, count * sizeof(uint32_t), first == 0 ? values : nullptr, GL_DYNAMIC_DRAW);
		if (first != 0)
			glBufferSubData(GL_UNIFORM_BUFFER, first * sizeof(uint32_t), count * sizeof(uint32_t), values);

		set_resource_name(make_resource_handle(GL_BUFFER, _push_constants), "Push constants");

		_push_constants_size = count;
	}
	// Otherwise discard the previous range (so driver can return a new memory region to avoid stalls) and update it with the new constants
	else if (void *const data = glMapBufferRange(GL_UNIFORM_BUFFER, first * sizeof(uint32_t), count * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
		data != nullptr)
	{
		std::memcpy(data, values, count * sizeof(uint32_t));
		glUnmapBuffer(GL_UNIFORM_BUFFER);
	}
}
void reshade::opengl::device_impl::push_descriptors(api::shader_stage, api::pipeline_layout layout, uint32_t layout_param, const api::descriptor_set_update &update)
{
	assert(update.set.handle == 0 && update.array_offset == 0);

	uint32_t first = update.binding;
	if (layout.handle != 0 && layout != global_pipeline_layout)
		first = reinterpret_cast<pipeline_layout_impl *>(layout.handle)->ranges[layout_param].binding;

	switch (update.type)
	{
	case api::descriptor_type::sampler:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(update.descriptors)[i];
			glBindSampler(first + i, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(update.descriptors)[i];
			if (descriptor.view.handle == 0)
				continue;
			glActiveTexture(GL_TEXTURE0 + first + i);
			glBindTexture(descriptor.view.handle >> 40, descriptor.view.handle & 0xFFFFFFFF);
			glBindSampler(first + i, descriptor.sampler.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::shader_resource_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[i];
			if (descriptor.handle == 0)
				continue;
			glActiveTexture(GL_TEXTURE0 + first + i);
			glBindTexture(descriptor.handle >> 40, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::unordered_access_view:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(update.descriptors)[i];
			if (descriptor.handle == 0)
				continue;

			const GLenum target = descriptor.handle >> 40;
			const GLuint object = descriptor.handle & 0xFFFFFFFF;

			GLint internal_format = 0;
			if (_supports_dsa)
			{
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
			}
			else
			{
				GLint prev_binding = 0;
				glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
				glBindTexture(target, object);

				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

				glBindTexture(target, prev_binding);
			}

			glBindImageTexture(first + i, descriptor.handle & 0xFFFFFFFF, 0, GL_FALSE, 0, GL_READ_WRITE, internal_format);
		}
		break;
	case api::descriptor_type::constant_buffer:
		for (uint32_t i = 0; i < update.count; ++i)
		{
			const auto &descriptor = static_cast<const api::buffer_range *>(update.descriptors)[i];
			if (descriptor.size == UINT64_MAX)
			{
				assert(descriptor.offset == 0);
				glBindBufferBase(GL_UNIFORM_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF);
			}
			else
			{
				assert(descriptor.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && descriptor.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				glBindBufferRange(GL_UNIFORM_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF, static_cast<GLintptr>(descriptor.offset), static_cast<GLsizeiptr>(descriptor.size));
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
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF);
			}
			else
			{
				assert(descriptor.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && descriptor.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				glBindBufferRange(GL_SHADER_STORAGE_BUFFER, first + i, descriptor.buffer.handle & 0xFFFFFFFF, static_cast<GLintptr>(descriptor.offset), static_cast<GLsizeiptr>(descriptor.size));
			}
		}
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::opengl::device_impl::bind_descriptor_sets(api::shader_stage stages, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<const descriptor_set_impl *>(sets[i].handle);

		push_descriptors(
			stages,
			layout,
			first + i,
			api::descriptor_set_update { {}, 0, 0, set_impl->count, set_impl->type, set_impl->descriptors.data() });
	}
}

void reshade::opengl::device_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	assert(offset == 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.handle & 0xFFFFFFFF);

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
		assert(false);
		break;
	}
}
void reshade::opengl::device_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(offsets[i] <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		glBindVertexBuffer(first + i, buffers[i].handle & 0xFFFFFFFF, static_cast<GLintptr>(offsets[i]), strides[i]);
	}
}
void reshade::opengl::device_impl::bind_stream_output_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint64_t *max_sizes)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (offsets == nullptr || max_sizes == nullptr || max_sizes[i] == UINT64_MAX)
		{
			assert(offsets == nullptr || offsets[i] == 0);

			glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, first + i, buffers[i].handle & 0xFFFFFFFF);
		}
		else
		{
			assert(offsets[i] <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			assert(max_sizes[i] <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

			glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, first + i, buffers[i].handle & 0xFFFFFFFF, static_cast<GLintptr>(offsets[i]), static_cast<GLsizeiptr>(max_sizes[i]));
		}
	}
}

void reshade::opengl::device_impl::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	glDrawArraysInstancedBaseInstance(_current_prim_mode, first_vertex, vertex_count, instance_count, first_instance);
}
void reshade::opengl::device_impl::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	glDrawElementsInstancedBaseVertexBaseInstance(_current_prim_mode, index_count, _current_index_type, reinterpret_cast<void *>(static_cast<uintptr_t>(first_index) * get_index_type_size(_current_index_type)), instance_count, vertex_offset, first_instance);
}
void reshade::opengl::device_impl::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
	glDispatchCompute(group_count_x, group_count_y, group_count_z);
}
void reshade::opengl::device_impl::draw_or_dispatch_indirect(api::indirect_command type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	switch (type)
	{
	case api::indirect_command::draw:
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		glMultiDrawArraysIndirect(_current_prim_mode, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case api::indirect_command::draw_indexed:
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		glMultiDrawElementsIndirect(_current_prim_mode, _current_index_type, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case api::indirect_command::dispatch:
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		for (GLuint i = 0; i < draw_count; ++i)
		{
			assert(offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

			glDispatchComputeIndirect(static_cast<GLintptr>(offset + static_cast<uint64_t>(i) * stride));
		}
		break;
	}
}

void reshade::opengl::device_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = get_resource_desc(src);

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
void reshade::opengl::device_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);

	const GLuint src_object = src.handle & 0xFFFFFFFF;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	assert(src_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) &&
		   dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && (size == UINT64_MAX || size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max())));

	if (_supports_dsa)
	{
		if (size == UINT64_MAX)
		{
			GLint max_size = 0;
			glGetNamedBufferParameteriv(src_object, GL_BUFFER_SIZE, &max_size);
			size  = max_size;
		}

		glCopyNamedBufferSubData(src_object, dst_object, static_cast<GLintptr>(src_offset), static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));
	}
	else
	{
		GLint prev_read_buf = 0;
		GLint prev_write_buf = 0;
		glGetIntegerv(GL_COPY_READ_BUFFER, &prev_read_buf);
		glGetIntegerv(GL_COPY_WRITE_BUFFER, &prev_write_buf);

		glBindBuffer(GL_COPY_READ_BUFFER, src_object);
		glBindBuffer(GL_COPY_WRITE_BUFFER, dst_object);

		if (size == UINT64_MAX)
		{
			GLint max_size = 0;
			glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &max_size);
			size  = max_size;
		}

		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(src_offset), static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));

		glBindBuffer(GL_COPY_READ_BUFFER, prev_read_buf);
		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_write_buf);
	}
}
void reshade::opengl::device_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box)
{
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_unpack_binding = 0;
	GLint prev_unpack_lsb = GL_FALSE;
	GLint prev_unpack_swap = GL_FALSE;
	GLint prev_unpack_alignment = 0;
	GLint prev_unpack_row_length = 0;
	GLint prev_unpack_image_height = 0;
	GLint prev_unpack_skip_rows = 0;
	GLint prev_unpack_skip_pixels = 0;
	GLint prev_unpack_skip_images = 0;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prev_unpack_binding);
	glGetIntegerv(GL_UNPACK_LSB_FIRST, &prev_unpack_lsb);
	glGetIntegerv(GL_UNPACK_SWAP_BYTES, &prev_unpack_swap);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_unpack_row_length);
	glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &prev_unpack_image_height);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &prev_unpack_skip_rows);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &prev_unpack_skip_pixels);
	glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &prev_unpack_skip_images);

	// Bind source buffer
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, src.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, slice_height);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

	GLuint xoffset, yoffset, zoffset, width, height, depth = 1;
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
		width   = 0;
		height  = 0;
	}

	if (dst_target == GL_FRAMEBUFFER_DEFAULT)
	{
		assert(false);
	}
	else if (dst_target == GL_RENDERBUFFER)
	{
		assert(false);
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(get_binding_for_target(dst_target), &prev_binding);
		glBindTexture(dst_target, dst_object);

		const api::resource_desc desc = get_resource_desc(dst);

		const GLuint level = dst_subresource % desc.texture.levels;
		      GLuint layer = dst_subresource / desc.texture.levels;

		GLenum level_target = dst_target;
		if (dst_target == GL_TEXTURE_CUBE_MAP || dst_target == GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		if (dst_box == nullptr)
		{
			width  = desc.texture.width;
			height = desc.texture.height;
			depth  = desc.texture.depth_or_layers;
		}

		const auto row_pitch = api::format_row_pitch(desc.texture.format, row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(desc.texture.format, row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * static_cast<size_t>(slice_pitch);

		assert(total_image_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		GLenum type, format = convert_upload_format(convert_format(desc.texture.format), type);

		switch (level_target)
		{
		case GL_TEXTURE_1D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage1D(level_target, level, xoffset, width, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage1D(level_target, level, xoffset, width, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		case GL_TEXTURE_1D_ARRAY:
			yoffset += layer;
			[[fallthrough]];
		case GL_TEXTURE_2D:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		case GL_TEXTURE_2D_ARRAY:
			zoffset += layer;
			[[fallthrough]];
		case GL_TEXTURE_3D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		}

		glBindTexture(dst_target, prev_binding);
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, prev_unpack_binding);
	glPixelStorei(GL_UNPACK_LSB_FIRST, prev_unpack_lsb);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, prev_unpack_swap);
	glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, prev_unpack_row_length);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, prev_unpack_image_height);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, prev_unpack_skip_rows);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, prev_unpack_skip_pixels);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, prev_unpack_skip_images);
}
void reshade::opengl::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, const api::subresource_box *dst_box, api::filter_mode filter)
{
	assert(src.handle != 0 && dst.handle != 0);

	const api::resource_desc src_desc = get_resource_desc(src);
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	const api::resource_desc dst_desc = get_resource_desc(dst);
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

	if (src_target != GL_FRAMEBUFFER_DEFAULT &&
		dst_target != GL_FRAMEBUFFER_DEFAULT &&
		src_desc.texture.samples == dst_desc.texture.samples &&
		(src_region[3] - src_region[0]) == (dst_region[3] - dst_region[0]) &&
		(src_region[4] - src_region[1]) == (dst_region[4] - dst_region[1]) &&
		(src_region[5] - src_region[0]) == (dst_region[5] - dst_region[2]))
	{
		if (src_box != nullptr)
		{
			src_region[3] -= src_box->left;
			src_region[4] -= src_box->top;
			src_region[5] -= src_box->front;
		}

		glCopyImageSubData(
			src_object, src_target, src_subresource % src_desc.texture.levels, src_region[0], src_region[1], src_region[2] + (src_subresource / src_desc.texture.levels),
			dst_object, dst_target, dst_subresource % dst_desc.texture.levels, dst_region[0], dst_region[1], dst_region[2] + (dst_subresource / src_desc.texture.levels),
			src_region[3], src_region[4], src_region[5]);
	}
	else
	{
		GLint prev_read_fbo = 0;
		GLint prev_draw_fbo = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

		const bool copy_depth = is_depth_stencil_format(convert_format(src_desc.texture.format), GL_DEPTH);
		assert(is_depth_stencil_format(convert_format(dst_desc.texture.format), GL_DEPTH) == copy_depth);

		bind_framebuffer_with_resource(GL_READ_FRAMEBUFFER, copy_depth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, src, src_subresource, src_desc);
		bind_framebuffer_with_resource(GL_DRAW_FRAMEBUFFER, copy_depth ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, dst, dst_subresource, dst_desc);

		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);

		assert(src_region[2] == 0 && dst_region[2] == 0 && src_region[5] == 1 && dst_region[5] == 1);
		glBlitFramebuffer(
			src_region[0], src_region[1], src_region[3], src_region[4],
			dst_region[0], dst_region[4], dst_region[3], dst_region[1],
			copy_depth ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT,
			// Must be nearest filtering for depth or stencil attachments
			(filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point) && !copy_depth ? GL_LINEAR : GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
	}
}
void reshade::opengl::device_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_pack_binding = 0;
	GLint prev_pack_lsb = GL_FALSE;
	GLint prev_pack_swap = GL_FALSE;
	GLint prev_pack_alignment = 0;
	GLint prev_pack_row_length = 0;
	GLint prev_pack_image_height = 0;
	GLint prev_pack_skip_rows = 0;
	GLint prev_pack_skip_pixels = 0;
	GLint prev_pack_skip_images = 0;
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &prev_pack_binding);
	glGetIntegerv(GL_PACK_LSB_FIRST, &prev_pack_lsb);
	glGetIntegerv(GL_PACK_SWAP_BYTES, &prev_pack_swap);
	glGetIntegerv(GL_PACK_ALIGNMENT, &prev_pack_alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &prev_pack_row_length);
	glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &prev_pack_image_height);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &prev_pack_skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &prev_pack_skip_pixels);
	glGetIntegerv(GL_PACK_SKIP_IMAGES, &prev_pack_skip_images);

	// Bind destination buffer
	glBindBuffer(GL_PIXEL_PACK_BUFFER, dst.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, slice_height);

	GLuint xoffset, yoffset, zoffset, width, height, depth = 1;
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
		width   = 0;
		height  = 0;
	}

	if (src_target == GL_FRAMEBUFFER_DEFAULT)
	{
		assert(src_subresource == 0);
		assert(zoffset == 0 && depth == 1);

		GLint prev_binding = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_binding);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		glReadBuffer(src_object);

		if (src_box == nullptr)
		{
			width = _default_fbo_width;
			height = _default_fbo_height;
		}

		GLenum format = (src_object == GL_BACK || src_object == GL_BACK_LEFT || src_object == GL_BACK_RIGHT) ? _default_color_format : _default_depth_format, type;
		format = convert_upload_format(format, type);

		glReadPixels(xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));

		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_binding);
	}
	else if (src_target == GL_RENDERBUFFER)
	{
		assert(src_subresource == 0);

		GLint prev_rbo_binding = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_rbo_binding);
		GLint prev_fbo_binding = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_fbo_binding);

		glBindRenderbuffer(GL_RENDERBUFFER, src_object);

		bind_framebuffer_with_resource(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, src, src_subresource, {});

		glReadBuffer(GL_COLOR_ATTACHMENT0);

		GLenum format = GL_NONE, type;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		if (src_box == nullptr)
		{
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, reinterpret_cast<GLint *>(&width));
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, reinterpret_cast<GLint *>(&height));
		}

		format = convert_upload_format(format, type);

		glReadPixels(xoffset, yoffset, width, height, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));

		glBindRenderbuffer(GL_RENDERBUFFER, prev_rbo_binding);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_fbo_binding);
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(get_binding_for_target(src_target), &prev_binding);
		glBindTexture(src_target, src_object);

		const api::resource_desc desc = get_resource_desc(src);

		const GLuint level = src_subresource % desc.texture.levels;
		      GLuint layer = src_subresource / desc.texture.levels;

		GLenum level_target = src_target;
		if (src_target == GL_TEXTURE_CUBE_MAP || src_target == GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		if (src_box == nullptr)
		{
			width  = desc.texture.width;
			height = desc.texture.height;
			depth  = desc.texture.depth_or_layers;
		}

		const auto row_pitch = api::format_row_pitch(desc.texture.format, row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(desc.texture.format, row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * static_cast<size_t>(slice_pitch);

		assert(total_image_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		GLenum type, format = convert_upload_format(convert_format(desc.texture.format), type);

		if (src_box == nullptr)
		{
			assert(layer == 0);

			glGetTexImage(src_target == GL_TEXTURE_CUBE_MAP || src_target == GL_TEXTURE_CUBE_MAP_ARRAY ? level_target : src_target, level, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}
		else if (_supports_dsa)
		{
			switch (src_target)
			{
			case GL_TEXTURE_1D_ARRAY:
				yoffset += layer;
				break;
			case GL_TEXTURE_CUBE_MAP:
			case GL_TEXTURE_CUBE_MAP_ARRAY:
			case GL_TEXTURE_2D_ARRAY:
				zoffset += layer;
				break;
			}

			glGetTextureSubImage(src_object, level, xoffset, yoffset, zoffset, width, height, depth, format, type, static_cast<GLsizei>(total_image_size), reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}

		glBindTexture(src_target, prev_binding);
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_PACK_BUFFER, prev_pack_binding);
	glPixelStorei(GL_PACK_LSB_FIRST, prev_pack_lsb);
	glPixelStorei(GL_PACK_SWAP_BYTES, prev_pack_swap);
	glPixelStorei(GL_PACK_ALIGNMENT, prev_pack_alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, prev_pack_row_length);
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, prev_pack_image_height);
	glPixelStorei(GL_PACK_SKIP_ROWS, prev_pack_skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, prev_pack_skip_pixels);
	glPixelStorei(GL_PACK_SKIP_IMAGES, prev_pack_skip_images);
}
void reshade::opengl::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const api::subresource_box *src_box, api::resource dst, uint32_t dst_subresource, int32_t dst_x, int32_t dst_y, int32_t dst_z, api::format)
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
		const api::resource_desc desc = get_resource_desc(dst);

		dst_box.right  = dst_x + std::max(1u, desc.texture.width >> (dst_subresource % desc.texture.levels));
		dst_box.bottom = dst_y + std::max(1u, desc.texture.height >> (dst_subresource % desc.texture.levels));
		dst_box.back   = dst_z + 1;
	}

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, &dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::opengl::device_impl::clear_depth_stencil_view(api::resource_view dsv, const float *depth, const uint8_t *stencil, uint32_t rect_count, const api::rect *)
{
	assert(dsv.handle != 0 && rect_count == 0); // Clearing rectangles is not supported

	GLint prev_draw_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

	bind_framebuffer_with_resource_views(GL_DRAW_FRAMEBUFFER, 0, nullptr, dsv);

	if (depth != nullptr && stencil != nullptr)
	{
		glClearBufferfi(GL_DEPTH_STENCIL, 0, *depth, *stencil);
	}
	else if (depth != nullptr)
	{
		const auto clear_value = *depth;
		glClearBufferfv(GL_DEPTH, 0, &clear_value);
	}
	else if (stencil != nullptr)
	{
		const auto clear_value = static_cast<GLint>(*stencil);
		glClearBufferiv(GL_STENCIL, 0, &clear_value);
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
}
void reshade::opengl::device_impl::clear_render_target_view(api::resource_view rtv, const float color[4], uint32_t rect_count, const api::rect *)
{
	assert(rtv.handle != 0 && rect_count == 0); // Clearing rectangles is not supported

	GLint prev_draw_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

	bind_framebuffer_with_resource_views(GL_DRAW_FRAMEBUFFER, 1, &rtv, { 0 });

	glClearBufferfv(GL_COLOR, 0, color);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
}
void reshade::opengl::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const api::rect *)
{
	assert(false);
}
void reshade::opengl::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const api::rect *)
{
	assert(false);
}

void reshade::opengl::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	const GLenum target = srv.handle >> 40;
	const GLuint object = srv.handle & 0xFFFFFFFF;

	glBindSampler(0, _mipmap_sampler);
	glActiveTexture(GL_TEXTURE0); // src
	glBindTexture(target, object);

#if 0
	glGenerateMipmap(target);
#else
	// Use custom mipmap generation implementation because 'glGenerateMipmap' generates shifted results
	glUseProgram(_mipmap_program);

	GLuint levels = 0;
	GLuint base_width = 0;
	GLuint base_height = 0;
	GLenum internal_format = GL_NONE;
	glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, reinterpret_cast<GLint *>(&levels));
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, reinterpret_cast<GLint *>(&base_width));
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&base_height));
	glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&internal_format));

	for (GLuint level = 1; level < levels; ++level)
	{
		const GLuint width = std::max(1u, base_width >> level);
		const GLuint height = std::max(1u, base_height >> level);

		glUniform3f(0 /* info */, 1.0f / width, 1.0f / height, static_cast<float>(level - 1));
		glBindImageTexture(1 /* dest */, object, level, GL_FALSE, 0, GL_WRITE_ONLY, internal_format);

		glDispatchCompute(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Ensure next iteration reads the data written by this iteration
		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
	}
#endif
}

void reshade::opengl::device_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	switch (type)
	{
	default:
		glBeginQuery(convert_query_type(type), reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		glBeginQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index], static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::opengl::device_impl::end_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	switch (type)
	{
	default:
		glEndQuery(convert_query_type(type));
		break;
	case api::query_type::timestamp:
		glQueryCounter(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index], GL_TIMESTAMP);
		break;
	case api::query_type::stream_output_statistics_0:
	case api::query_type::stream_output_statistics_1:
	case api::query_type::stream_output_statistics_2:
	case api::query_type::stream_output_statistics_3:
		glEndQueryIndexed(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, static_cast<uint32_t>(type) - static_cast<uint32_t>(api::query_type::stream_output_statistics_0));
		break;
	}
}
void reshade::opengl::device_impl::copy_query_pool_results(api::query_pool pool, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	assert(pool.handle != 0);

	for (size_t i = 0; i < count; ++i)
	{
		assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		glGetQueryBufferObjectui64v(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[first + i], dst.handle & 0xFFFFFFFF, GL_QUERY_RESULT_NO_WAIT, static_cast<GLintptr>(dst_offset + i * stride));
	}
}

void reshade::opengl::device_impl::begin_debug_event(const char *label, const float[4])
{
	glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, label);
}
void reshade::opengl::device_impl::end_debug_event()
{
	glPopDebugGroup();
}
void reshade::opengl::device_impl::insert_debug_marker(const char *label, const float[4])
{
	glDebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
}
