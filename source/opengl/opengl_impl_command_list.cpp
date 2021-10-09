/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
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

void reshade::opengl::pipeline_impl::apply_compute() const
{
	glUseProgram(program);
}
void reshade::opengl::pipeline_impl::apply_graphics() const
{
	glUseProgram(program);
	glBindVertexArray(vao);

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

	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

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

	if (prim_mode == GL_PATCHES)
	{
		glPatchParameteri(GL_PATCH_VERTICES, patch_vertices);
	}
}

void reshade::opengl::device_impl::begin_render_pass(api::render_pass, api::framebuffer fbo)
{
	const GLuint fbo_object = fbo.handle & 0xFFFFFFFF;
	const GLuint num_color_attachments = static_cast<uint32_t>(fbo.handle >> 40);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo_object);

	if (num_color_attachments == 0)
	{
		glDrawBuffer(GL_NONE);
	}
	else if (fbo_object == 0)
	{
		glDrawBuffer(GL_BACK);
	}
	else
	{
		const GLenum draw_buffers[8] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
		glDrawBuffers(num_color_attachments, draw_buffers);
	}

	glEnableOrDisable(GL_FRAMEBUFFER_SRGB, (fbo.handle & 0x200000000) != 0);
}
void reshade::opengl::device_impl::finish_render_pass()
{
}
void reshade::opengl::device_impl::bind_render_targets_and_depth_stencil(uint32_t, const api::resource_view *, api::resource_view)
{
	assert(false);
}

void reshade::opengl::device_impl::bind_pipeline(api::pipeline_stage type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_stage::all_compute:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply_compute();
		break;
	case api::pipeline_stage::all_graphics:
		// Always disable alpha test in case the application set that (fixes broken GUI rendering in Quake)
		if (_compatibility_context)
			glDisable(GL_ALPHA_TEST);
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply_graphics();
		_current_prim_mode = reinterpret_cast<pipeline_impl *>(pipeline.handle)->prim_mode;
		break;
	default:
		assert(false);
		break;
	}
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
void reshade::opengl::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 6)
	{
		glViewportIndexedf(first + i, viewports[k], viewports[k + 1], viewports[k + 2], viewports[k + 3]);
	}
}
void reshade::opengl::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	for (uint32_t i = 0, k = 0; i < count; ++i, k += 4)
	{
		glScissorIndexed(first + i,
			rects[k + 0],
			rects[k + 1],
			rects[k + 2] - rects[k + 0],
			rects[k + 3] - rects[k + 1]);
	}
}

void reshade::opengl::device_impl::push_constants(api::shader_stage, api::pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void *values)
{
	const GLuint push_constants_binding = layout.handle != 0 ?
		reinterpret_cast<pipeline_layout_impl *>(layout.handle)->bindings[layout_param] : 0;

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
	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->bindings[layout_param];

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
			if (descriptor.size == std::numeric_limits<uint64_t>::max())
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
			if (descriptor.size == std::numeric_limits<uint64_t>::max())
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
			api::descriptor_set_update(0, set_impl->count, set_impl->type, set_impl->descriptors.data()));
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
		copy_buffer_region(src, 0, dst, 0, std::numeric_limits<uint64_t>::max());
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
		   dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && (size == std::numeric_limits<uint64_t>::max() || size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max())));

	if (_supports_dsa)
	{
		if (size == std::numeric_limits<uint64_t>::max())
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

		if (size == std::numeric_limits<uint64_t>::max())
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
void reshade::opengl::device_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	const GLenum dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_unpack_binding = 0;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prev_unpack_binding);

	GLint prev_unpack_swap = GL_FALSE;
	GLint prev_unpack_lsb_first = GL_FALSE;
	GLint prev_unpack_row_length = 0;
	GLint prev_unpack_skip_rows = 0;
	GLint prev_unpack_skip_pixels = 0;
	GLint prev_unpack_alignment = 0;
	GLint prev_unpack_skip_slices = 0;
	GLint prev_unpack_slice_height = 0;
	glGetIntegerv(GL_UNPACK_SWAP_BYTES, &prev_unpack_swap);
	glGetIntegerv(GL_UNPACK_LSB_FIRST, &prev_unpack_lsb_first);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_unpack_row_length);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &prev_unpack_skip_rows);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &prev_unpack_skip_pixels);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
	glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &prev_unpack_skip_slices);
	glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &prev_unpack_slice_height);

	// Bind source buffer
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, src.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, slice_height);

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

		GLint levels = 0;
		glGetTexParameteriv(dst_target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
		if (0 == levels)
			levels = 1;

		const GLuint level = dst_subresource % levels;
		      GLuint layer = dst_subresource / levels;

		GLenum level_target = dst_target;
		if (dst_target == GL_TEXTURE_CUBE_MAP || dst_target == GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		GLenum format = GL_NONE, type;
		glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		GLuint xoffset, yoffset, zoffset, width, height, depth = 1;
		if (dst_box != nullptr)
		{
			xoffset = dst_box[0];
			yoffset = dst_box[1];
			zoffset = dst_box[2];
			width   = dst_box[3] - dst_box[0];
			height  = dst_box[4] - dst_box[1];
			depth   = dst_box[5] - dst_box[2];
		}
		else
		{
			xoffset = yoffset = zoffset = 0;
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_WIDTH,  reinterpret_cast<GLint *>(&width));
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&height));
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_DEPTH,  reinterpret_cast<GLint *>(&depth));
		}

		const auto row_pitch = api::format_row_pitch(convert_format(format), row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(convert_format(format), row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * slice_pitch;

		format = convert_upload_format(format, type);

		switch (level_target)
		{
		case GL_TEXTURE_1D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage1D(level_target, level, xoffset, width, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage1D(level_target, level, xoffset, width, format, total_image_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
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
				glCompressedTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, total_image_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
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
				glCompressedTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, total_image_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		}

		glBindTexture(dst_target, prev_binding);
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, prev_unpack_binding);

	glPixelStorei(GL_UNPACK_SWAP_BYTES, prev_unpack_swap);
	glPixelStorei(GL_UNPACK_LSB_FIRST, prev_unpack_lsb_first);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, prev_unpack_row_length);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, prev_unpack_skip_rows);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, prev_unpack_skip_pixels);
	glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, prev_unpack_skip_slices);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, prev_unpack_slice_height);
}
void reshade::opengl::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::filter_mode filter)
{
	assert(src.handle != 0 && dst.handle != 0);

	const api::resource_desc src_desc = get_resource_desc(src);
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	const api::resource_desc dst_desc = get_resource_desc(dst);
	const GLuint dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	if (src_target != GL_FRAMEBUFFER_DEFAULT && dst_target != GL_FRAMEBUFFER_DEFAULT &&
		(src_box == nullptr && dst_box == nullptr) || (src_box != nullptr && dst_box != nullptr &&
			(src_box[3] - src_box[0]) == (dst_box[3] - dst_box[0]) &&
			(src_box[4] - src_box[1]) == (dst_box[4] - dst_box[1]) &&
			(src_box[5] - src_box[2]) == (dst_box[5] - dst_box[2])))
	{
		int32_t size[3];
		if (src_box != nullptr)
		{
			size[0] = src_box[3] - src_box[0];
			size[1] = src_box[4] - src_box[1];
			size[2] = src_box[5] - src_box[2];
		}
		else
		{
			size[0] = std::max(1u, src_desc.texture.width >> (src_subresource % src_desc.texture.levels));
			size[1] = std::max(1u, src_desc.texture.height >> (src_subresource % src_desc.texture.levels));
			size[2] = (src_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(src_desc.texture.depth_or_layers) >> (src_subresource % src_desc.texture.levels)) : 1u);
		}

		glCopyImageSubData(
			src_object, src_target, src_subresource % src_desc.texture.levels, src_box != nullptr ? src_box[0] : 0, src_box != nullptr ? src_box[1] : 0, (src_box != nullptr ? src_box[2] : 0) + (src_subresource / src_desc.texture.levels),
			dst_object, dst_target, dst_subresource % dst_desc.texture.levels, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, (dst_box != nullptr ? dst_box[2] : 0) + (dst_subresource / src_desc.texture.levels),
			size[0], size[1], size[2]);
	}
	else
	{
		GLint src_region[6] = {};
		if (src_box != nullptr)
		{
			std::copy_n(src_box, 6, src_region);
		}
		else
		{
			src_region[3] = static_cast<GLint>(std::max(1u, src_desc.texture.width >> (src_subresource % src_desc.texture.levels)));
			src_region[4] = static_cast<GLint>(std::max(1u, src_desc.texture.height >> (src_subresource % src_desc.texture.levels)));
			src_region[5] = static_cast<GLint>((src_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(src_desc.texture.depth_or_layers) >> (src_subresource % src_desc.texture.levels)) : 1u));
		}

		GLint dst_region[6] = {};
		if (dst_box != nullptr)
		{
			std::copy_n(dst_box, 6, dst_region);
		}
		else
		{
			dst_region[3] = static_cast<GLint>(std::max(1u, dst_desc.texture.width >> (dst_subresource % dst_desc.texture.levels)));
			dst_region[4] = static_cast<GLint>(std::max(1u, dst_desc.texture.height >> (dst_subresource % dst_desc.texture.levels)));
			dst_region[5] = static_cast<GLint>((dst_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(dst_desc.texture.depth_or_layers) >> (dst_subresource % dst_desc.texture.levels)) : 1u));
		}

		GLint prev_read_fbo = 0;
		GLint prev_draw_fbo = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

		const GLenum src_attachment = is_depth_stencil_format(convert_format(src_desc.texture.format), GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
		switch (src_target)
		{
		case GL_TEXTURE:
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _copy_fbo[0]);
			if (src_desc.texture.depth_or_layers > 1)
			{
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, src_attachment, src_object, src_subresource % src_desc.texture.levels, src_subresource / src_desc.texture.levels);
			}
			else
			{
				assert((src_subresource % src_desc.texture.levels) == 0);
				glFramebufferTexture(GL_READ_FRAMEBUFFER, src_attachment, src_object, src_subresource);
			}
			assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			break;
		case GL_RENDERBUFFER:
			assert(src_subresource == 0);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _copy_fbo[0]);
			glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, src_attachment, GL_RENDERBUFFER, src_object);
			assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			break;
		case GL_FRAMEBUFFER_DEFAULT:
			assert(src_subresource == 0);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			break;
		default:
			assert(false);
			return;
		}

		const GLenum dst_attachment = is_depth_stencil_format(convert_format(dst_desc.texture.format), GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
		switch (dst_target)
		{
		case GL_TEXTURE:
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copy_fbo[1]);
			if (dst_desc.texture.depth_or_layers > 1)
			{
				glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, dst_attachment, dst_object, dst_subresource % dst_desc.texture.levels, dst_subresource / dst_desc.texture.levels);
			}
			else
			{
				assert((dst_subresource % dst_desc.texture.levels) == 0);
				glFramebufferTexture(GL_DRAW_FRAMEBUFFER, dst_attachment, dst_object, dst_subresource);
			}
			assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			break;
		case GL_RENDERBUFFER:
			assert(dst_subresource == 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copy_fbo[1]);
			glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, dst_attachment, GL_RENDERBUFFER, dst_object);
			assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			break;
		case GL_FRAMEBUFFER_DEFAULT:
			assert(dst_subresource == 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			break;
		default:
			assert(false);
			return;
		}

		assert(src_region[2] == 0 && dst_region[2] == 0 && src_region[5] == 1 && dst_region[5] == 1);
		assert(src_attachment == dst_attachment);
		glBlitFramebuffer(
			src_region[0], src_region[1], src_region[3], src_region[4],
			dst_region[0], dst_region[4], dst_region[3], dst_region[1],
			src_attachment == GL_DEPTH_ATTACHMENT ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT,
			// Must be nearest filtering for depth or stencil attachments
			src_attachment != GL_DEPTH_ATTACHMENT && (filter == api::filter_mode::min_mag_mip_linear || filter == api::filter_mode::min_mag_linear_mip_point) ? GL_LINEAR : GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
	}
}
void reshade::opengl::device_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_pack_binding = 0;
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &prev_pack_binding);

	GLint prev_pack_swap = GL_FALSE;
	GLint prev_pack_lsb_first = GL_FALSE;
	GLint prev_pack_alignment = 0;
	GLint prev_pack_row_length = 0;
	GLint prev_pack_slice_height = 0;
	GLint prev_pack_skip_rows = 0;
	GLint prev_pack_skip_pixels = 0;
	GLint prev_pack_skip_slices = 0;
	glGetIntegerv(GL_PACK_SWAP_BYTES, &prev_pack_swap);
	glGetIntegerv(GL_PACK_LSB_FIRST, &prev_pack_lsb_first);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &prev_pack_row_length);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &prev_pack_skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &prev_pack_skip_pixels);
	glGetIntegerv(GL_PACK_ALIGNMENT, &prev_pack_alignment);
	glGetIntegerv(GL_PACK_SKIP_IMAGES, &prev_pack_skip_slices);
	glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &prev_pack_slice_height);

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
		xoffset = src_box[0];
		yoffset = src_box[1];
		zoffset = src_box[2];
		width   = src_box[3] - src_box[0];
		height  = src_box[4] - src_box[1];
		depth   = src_box[5] - src_box[2];
	}
	else
	{
		xoffset = yoffset = zoffset = 0;
		width = height = 0;
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

		GLenum format = src_object == GL_BACK ? _default_color_format : _default_depth_format, type;
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

		glBindFramebuffer(GL_READ_FRAMEBUFFER, _copy_fbo[0]);
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, src_object);
		assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
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

		GLint levels = 0;
		glGetTexParameteriv(src_target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
		if (0 == levels)
			levels = 1;

		const GLuint level = src_subresource % levels;
		      GLuint layer = src_subresource / levels;

		GLenum level_target = src_target;
		if (src_target == GL_TEXTURE_CUBE_MAP || src_target == GL_TEXTURE_CUBE_MAP_ARRAY)
		{
			const GLuint face = layer % 6;
			layer /= 6;
			level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
		}

		GLenum format = GL_NONE, type;
		glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		if (src_box == nullptr)
		{
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_WIDTH,  reinterpret_cast<GLint *>(&width));
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&height));
			glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_DEPTH,  reinterpret_cast<GLint *>(&depth));
		}

		const auto row_pitch = api::format_row_pitch(convert_format(format), row_length != 0 ? row_length : width);
		const auto slice_pitch = api::format_slice_pitch(convert_format(format), row_pitch, slice_height != 0 ? slice_height : height);
		const auto total_image_size = depth * slice_pitch;

		format = convert_upload_format(format, type);

		if (src_box == nullptr)
		{
			glGetTexImage(src_target == GL_TEXTURE_CUBE_MAP ? level_target : src_target, level, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
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

			glGetTextureSubImage(src_object, level, xoffset, yoffset, zoffset, width, height, depth, format, type, total_image_size, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}

		glBindTexture(src_target, prev_binding);
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_PACK_BUFFER, prev_pack_binding);

	glPixelStorei(GL_PACK_SWAP_BYTES, prev_pack_swap);
	glPixelStorei(GL_PACK_LSB_FIRST, prev_pack_lsb_first);
	glPixelStorei(GL_PACK_ROW_LENGTH, prev_pack_row_length);
	glPixelStorei(GL_PACK_SKIP_ROWS, prev_pack_skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, prev_pack_skip_pixels);
	glPixelStorei(GL_PACK_ALIGNMENT, prev_pack_alignment);
	glPixelStorei(GL_PACK_SKIP_IMAGES, prev_pack_skip_slices);
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, prev_pack_slice_height);
}
void reshade::opengl::device_impl::resolve_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], api::format)
{
	int32_t dst_box[6] = {};
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, dst_box);

	if (src_box != nullptr)
	{
		dst_box[3] = dst_box[0] + src_box[3] - src_box[0];
		dst_box[4] = dst_box[1] + src_box[4] - src_box[1];
		dst_box[5] = dst_box[2] + src_box[5] - src_box[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);
		dst_box[3] = dst_box[0] + std::max(1u, desc.texture.width >> (dst_subresource % desc.texture.levels));
		dst_box[4] = dst_box[1] + std::max(1u, desc.texture.height >> (dst_subresource % desc.texture.levels));
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> (dst_subresource % desc.texture.levels)) : 1u);
	}

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::filter_mode::min_mag_mip_point);
}

void reshade::opengl::device_impl::clear_attachments(api::attachment_type clear_flags, const float color[4], float depth, uint8_t stencil, uint32_t rect_count, const int32_t *)
{
	assert(rect_count == 0);

	// Get current state
	GLfloat prev_col[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE, prev_col);
	GLfloat prev_depth;
	glGetFloatv(GL_DEPTH_CLEAR_VALUE, &prev_depth);
	GLint   prev_stencil;
	glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &prev_stencil);

	// Set up clear values before clear
	if (color != nullptr)
		glClearColor(color[0], color[1], color[2], color[3]);
	glClearDepth(depth);
	glClearStencil(stencil);

	glClear(convert_aspect_to_buffer_bits(clear_flags));

	// Restore previous state from application
	if (color != nullptr)
		glClearColor(prev_col[0], prev_col[1], prev_col[2], prev_col[3]);
	glClearDepth(prev_depth);
	glClearStencil(prev_stencil);
}
void reshade::opengl::device_impl::clear_depth_stencil_view(api::resource_view, api::attachment_type, float, uint8_t, uint32_t, const int32_t *)
{
	assert(false);
}
void reshade::opengl::device_impl::clear_render_target_view(api::resource_view, const float[4], uint32_t, const int32_t *)
{
	assert(false);
}
void reshade::opengl::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4], uint32_t, const int32_t *)
{
	assert(false);
}
void reshade::opengl::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4], uint32_t, const int32_t *)
{
	assert(false);
}

void reshade::opengl::device_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);

	const GLenum target = srv.handle >> 40;
	const GLuint object = srv.handle & 0xFFFFFFFF;

	glBindSampler(0, 0);
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
	}
#endif
}

void reshade::opengl::device_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	glBeginQuery(convert_query_type(type), reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index]);
}
void reshade::opengl::device_impl::finish_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	if (type == api::query_type::timestamp)
	{
		glQueryCounter(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[index], GL_TIMESTAMP);
	}
	else
	{
		glEndQuery(convert_query_type(type));
	}
}
void reshade::opengl::device_impl::copy_query_pool_results(api::query_pool pool, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	assert(pool.handle != 0);

	for (uint32_t i = 0; i < count; ++i)
	{
		assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		glGetQueryBufferObjectui64v(reinterpret_cast<query_pool_impl *>(pool.handle)->queries[first + i], dst.handle & 0xFFFFFFFF, GL_QUERY_RESULT_NO_WAIT, static_cast<GLintptr>(dst_offset + static_cast<uint64_t>(i) * stride));
	}
}

void reshade::opengl::device_impl::begin_debug_event(const char *label, const float[4])
{
	glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, label);
}
void reshade::opengl::device_impl::finish_debug_event()
{
	glPopDebugGroup();
}
void reshade::opengl::device_impl::insert_debug_marker(const char *label, const float[4])
{
	glDebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
}
