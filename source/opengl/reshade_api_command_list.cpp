/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade_api_device.hpp"
#include "reshade_api_type_convert.hpp"
#include <cassert>
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
	if (prim_mode == GL_PATCHES)
	{
		glPatchParameteri(GL_PATCH_VERTICES, patch_vertices);
	}

	glUseProgram(program);
	glBindVertexArray(vao);

	glFrontFace(front_face);

	if (cull_mode != GL_NONE)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(cull_mode);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	glPolygonMode(GL_FRONT_AND_BACK, polygon_mode);

	if (blend_enable)
	{
		glEnable(GL_BLEND);
		glBlendFuncSeparate(blend_src, blend_dst, blend_src_alpha, blend_dst_alpha);
		glBlendEquationSeparate(blend_eq, blend_eq_alpha);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	if (logic_op_enable)
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(logic_op);
	}
	else
	{
		glDisable(GL_COLOR_LOGIC_OP);
	}

	glColorMask(
		(color_write_mask & (1 << 0)) != 0,
		(color_write_mask & (1 << 1)) != 0,
		(color_write_mask & (1 << 2)) != 0,
		(color_write_mask & (1 << 3)) != 0);

	glEnableOrDisable(GL_DEPTH_TEST, depth_test);
	glDepthMask(depth_write_mask);

	if (stencil_test)
	{
		glEnable(GL_STENCIL_TEST);
		glStencilOpSeparate(GL_BACK, back_stencil_op_fail, back_stencil_op_depth_fail, back_stencil_op_pass);
		glStencilOpSeparate(GL_FRONT, front_stencil_op_fail, front_stencil_op_depth_fail, front_stencil_op_pass);
		glStencilMask(stencil_write_mask);
		glStencilFuncSeparate(GL_BACK, back_stencil_func, stencil_reference_value, stencil_read_mask);
		glStencilFuncSeparate(GL_FRONT, front_stencil_func, stencil_reference_value, stencil_read_mask);
	}
	else
	{
		glDisable(GL_STENCIL_TEST);
	}

	glEnableOrDisable(GL_SCISSOR_TEST, scissor_test);
	glEnableOrDisable(GL_MULTISAMPLE, multisample);
	glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, sample_alpha_to_coverage);
	glSampleMaski(0, sample_mask);
}

void reshade::opengl::device_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::compute:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply_compute();
		break;
	case api::pipeline_type::graphics:
		reinterpret_cast<pipeline_impl *>(pipeline.handle)->apply_graphics();
		_current_prim_mode = reinterpret_cast<pipeline_impl *>(pipeline.handle)->prim_mode;
		break;
	default:
		assert(false);
		break;
	}
}
void reshade::opengl::device_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (GLuint i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::primitive_topology:
			_current_prim_mode = values[i];
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::opengl::device_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	for (GLuint i = 0, k = 0; i < count; ++i, k += 6)
	{
		glViewportIndexedf(i + first, viewports[k], viewports[k + 1], viewports[k + 2], viewports[k + 3]);
	}
}
void reshade::opengl::device_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	for (GLuint i = 0, k = 0; i < count; ++i, k += 4)
	{
		glScissorIndexed(i + first,
			rects[k + 0],
			rects[k + 1],
			rects[k + 2] - rects[k + 0],
			rects[k + 3] - rects[k + 1]);
	}
}

void reshade::opengl::device_impl::push_constants(api::shader_stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values)
{
	const GLuint push_constants_binding = layout.handle != 0 ?
		reinterpret_cast<pipeline_layout_impl *>(layout.handle)->bindings[layout_index] : 0;

	if (_push_constants == 0)
	{
		glGenBuffers(1, &_push_constants);
	}

	// Binds the push constant buffer to the requested indexed binding point as well as the generic binding point
	glBindBufferBase(GL_UNIFORM_BUFFER, push_constants_binding, _push_constants);

	// Recreate the buffer data store in case it is no longer large enough
	if (count > _push_constants_size)
	{
		glBufferData(GL_UNIFORM_BUFFER, count * sizeof(uint32_t), first == 0 ? values : nullptr, GL_DYNAMIC_DRAW);
		if (first != 0)
			glBufferSubData(GL_UNIFORM_BUFFER, first * sizeof(uint32_t), count * sizeof(uint32_t), values);

		set_debug_name(make_resource_handle(GL_BUFFER, _push_constants), "Push constants");

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
void reshade::opengl::device_impl::push_descriptors(api::shader_stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	if (layout.handle != 0)
		first += reinterpret_cast<pipeline_layout_impl *>(layout.handle)->bindings[layout_index];

	switch (type)
	{
	case api::descriptor_type::sampler:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler *>(descriptors)[i];
			glBindSampler(i + first, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::sampler_with_resource_view:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::sampler_with_resource_view *>(descriptors)[i];
			if (descriptor.view.handle == 0)
				continue;
			glBindSampler(i + first, descriptor.sampler.handle & 0xFFFFFFFF);
			glActiveTexture(GL_TEXTURE0 + i + first);
			glBindTexture(descriptor.view.handle >> 40, descriptor.view.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::shader_resource_view:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			if (descriptor.handle == 0)
				continue;
			glActiveTexture(GL_TEXTURE0 + i + first);
			glBindTexture(descriptor.handle >> 40, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::unordered_access_view:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			if (descriptor.handle == 0)
				continue;
			glBindImageTexture(i + first, descriptor.handle & 0xFFFFFFFF, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8); // TODO: Format
		}
		break;
	case api::descriptor_type::constant_buffer:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource *>(descriptors)[i];
			glBindBufferBase(GL_UNIFORM_BUFFER, i + first, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	}
}
void reshade::opengl::device_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(sets[i].handle);

		if (set_impl->type != api::descriptor_type::sampler_with_resource_view)
		{
			push_descriptors(
				type == api::pipeline_type::compute ? api::shader_stage::compute : api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->descriptors.size()),
				set_impl->descriptors.data());
		}
		else
		{
			push_descriptors(
				api::shader_stage::all_graphics,
				layout,
				i + first,
				set_impl->type,
				0,
				static_cast<uint32_t>(set_impl->sampler_with_resource_views.size()),
				set_impl->sampler_with_resource_views.data());
		}
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
	for (GLuint i = 0; i < count; ++i)
	{
		assert(offsets[i] <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));

		glBindVertexBuffer(i + first, buffers[i].handle & 0xFFFFFFFF, static_cast<GLintptr>(offsets[i]), strides[i]);
	}
}

void reshade::opengl::device_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	glDrawArraysInstancedBaseInstance(_current_prim_mode, first_vertex, vertices, instances, first_instance);
}
void reshade::opengl::device_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	glDrawElementsInstancedBaseVertexBaseInstance(_current_prim_mode, indices, _current_index_type, reinterpret_cast<void *>(static_cast<uintptr_t>(first_index * get_index_type_size(_current_index_type))), instances, vertex_offset, first_instance);
}
void reshade::opengl::device_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}
void reshade::opengl::device_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	switch (type)
	{
	case 1:
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		glMultiDrawArraysIndirect(_current_prim_mode, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case 2:
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		glMultiDrawElementsIndirect(_current_prim_mode, _current_index_type, reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)), static_cast<GLsizei>(draw_count), static_cast<GLsizei>(stride));
		break;
	case 3:
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer.handle & 0xFFFFFFFF);
		for (GLuint i = 0; i < draw_count; ++i)
		{
			assert(offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			glDispatchComputeIndirect(static_cast<GLintptr>(offset + i * stride));
		}
		break;
	}
}

void reshade::opengl::device_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	GLuint fbo = 0;
	request_framebuffer(count, rtvs, dsv, fbo);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	if (count == 0)
	{
		glDrawBuffer(GL_NONE);
	}
	else if (fbo == 0)
	{
		glDrawBuffer(GL_BACK);

		if (rtvs[0].handle & 0x200000000)
		{
			glEnable(GL_FRAMEBUFFER_SRGB);
		}
		else
		{
			glDisable(GL_FRAMEBUFFER_SRGB);
		}
	}
	else
	{
		const GLenum draw_buffers[8] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
		glDrawBuffers(count, draw_buffers);

		if (rtvs[0].handle & 0x200000000)
		{
			glEnable(GL_FRAMEBUFFER_SRGB);
		}
		else
		{
			glDisable(GL_FRAMEBUFFER_SRGB);
		}
	}
}
void reshade::opengl::device_impl::finish_render_pass()
{
}

void reshade::opengl::device_impl::copy_resource(api::resource src, api::resource dst)
{
	const api::resource_desc desc = get_resource_desc(src);

	if (desc.type == api::resource_type::buffer)
	{
		copy_buffer_region(src, 0, dst, 0, ~0llu);
	}
	else
	{
		for (uint32_t layer = 0; layer < desc.texture.depth_or_layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, api::texture_filter::min_mag_mip_point);
			}
		}
	}
}
void reshade::opengl::device_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

	if (gl3wProcs.gl.CopyNamedBufferSubData != nullptr)
	{
		glCopyNamedBufferSubData(src.handle & 0xFFFFFFFF, dst.handle & 0xFFFFFFFF, static_cast<GLintptr>(src_offset), static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));
	}
	else
	{
		GLint prev_read_buf = 0;
		GLint prev_write_buf = 0;
		glGetIntegerv(GL_COPY_READ_BUFFER, &prev_read_buf);
		glGetIntegerv(GL_COPY_WRITE_BUFFER, &prev_write_buf);

		glBindBuffer(GL_COPY_READ_BUFFER, src.handle & 0xFFFFFFFF);
		glBindBuffer(GL_COPY_WRITE_BUFFER, dst.handle & 0xFFFFFFFF);
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
	GLint previous_unpack = 0;
	GLint previous_unpack_lsb = GL_FALSE;
	GLint previous_unpack_swap = GL_FALSE;
	GLint previous_unpack_alignment = 0;
	GLint previous_unpack_row_length = 0;
	GLint previous_unpack_image_height = 0;
	GLint previous_unpack_skip_rows = 0;
	GLint previous_unpack_skip_pixels = 0;
	GLint previous_unpack_skip_images = 0;
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previous_unpack);
	glGetIntegerv(GL_UNPACK_LSB_FIRST, &previous_unpack_lsb);
	glGetIntegerv(GL_UNPACK_SWAP_BYTES, &previous_unpack_swap);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previous_unpack_row_length);
	glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &previous_unpack_image_height);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &previous_unpack_skip_rows);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &previous_unpack_skip_pixels);
	glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &previous_unpack_skip_images);

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

	GLint x = 0;
	GLint y = 0;
	GLint z = 0;
	GLint w = 0;
	GLint h = 0;
	GLint d = 1;
	if (dst_box != nullptr)
	{
		x = dst_box[0];
		y = dst_box[1];
		z = dst_box[2];
		w = dst_box[3] - dst_box[0];
		h = dst_box[4] - dst_box[1];
		d = dst_box[5] - dst_box[2];
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
		glBindTexture(dst_target, dst_object);

		GLint levels = 1;
		glGetTexParameteriv(dst_target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
		const GLuint level = dst_subresource % levels;
		const GLuint layer = dst_subresource / levels;

		GLenum format = GL_NONE, type;
		glGetTexLevelParameteriv(dst_target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		if (dst_box == nullptr)
		{
			glGetTexLevelParameteriv(dst_target, level, GL_TEXTURE_WIDTH,  &w);
			glGetTexLevelParameteriv(dst_target, level, GL_TEXTURE_HEIGHT, &h);
			glGetTexLevelParameteriv(dst_target, level, GL_TEXTURE_DEPTH,  &d);
		}

		const auto row_size_packed = (row_length != 0 ? row_length : w) * api::format_bpp(convert_format(format));
		const auto slice_size_packed = (slice_height != 0 ? slice_height : h) * row_size_packed;
		const auto total_size = d * slice_size_packed;

		format = convert_upload_format(format, type);

		switch (dst_target)
		{
		case GL_TEXTURE_1D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage1D(dst_target, level, x, w, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage1D(dst_target, level, x, w, format, total_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		case GL_TEXTURE_1D_ARRAY:
			y += layer;
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
				glTexSubImage2D(dst_target, level, x, y, w, h, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage2D(dst_target, level, x, y, w, h, format, total_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		case GL_TEXTURE_2D_ARRAY:
			z += layer;
			[[fallthrough]];
		case GL_TEXTURE_3D:
			if (type != GL_COMPRESSED_TEXTURE_FORMATS)
			{
				glTexSubImage3D(dst_target, level, x, y, z, w, h, d, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			else
			{
				glCompressedTexSubImage3D(dst_target, level, x, y, z, w, h, d, format, total_size, reinterpret_cast<void *>(static_cast<uintptr_t>(src_offset)));
			}
			break;
		}
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, previous_unpack);
	glPixelStorei(GL_UNPACK_LSB_FIRST, previous_unpack_lsb);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, previous_unpack_swap);
	glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, previous_unpack_row_length);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, previous_unpack_image_height);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, previous_unpack_skip_rows);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, previous_unpack_skip_pixels);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, previous_unpack_skip_images);
}
void reshade::opengl::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
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
		return;
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
			src_attachment != GL_DEPTH_ATTACHMENT && (filter == api::texture_filter::min_mag_mip_linear || filter == api::texture_filter::min_mag_linear_mip_point) ? GL_LINEAR : GL_NEAREST);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
	}
}
void reshade::opengl::device_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	// Get current state
	GLint previous_pack = 0;
	GLint previous_pack_lsb = GL_FALSE;
	GLint previous_pack_swap = GL_FALSE;
	GLint previous_pack_alignment = 0;
	GLint previous_pack_row_length = 0;
	GLint previous_pack_image_height = 0;
	GLint previous_pack_skip_rows = 0;
	GLint previous_pack_skip_pixels = 0;
	GLint previous_pack_skip_images = 0;
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previous_pack);
	glGetIntegerv(GL_PACK_LSB_FIRST, &previous_pack_lsb);
	glGetIntegerv(GL_PACK_SWAP_BYTES, &previous_pack_swap);
	glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack_alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &previous_pack_row_length);
	glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &previous_pack_image_height);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &previous_pack_skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &previous_pack_skip_pixels);
	glGetIntegerv(GL_PACK_SKIP_IMAGES, &previous_pack_skip_images);

	// Bind destination buffer
	glBindBuffer(GL_PIXEL_PACK_BUFFER, dst.handle & 0xFFFFFFFF);

	// Set up pixel storage configuration
	glPixelStorei(GL_PACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, slice_height);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_PACK_SKIP_IMAGES, 0);

	GLint x = 0;
	GLint y = 0;
	GLint z = 0;
	GLint w = 0;
	GLint h = 0;
	GLint d = 1;
	if (src_box != nullptr)
	{
		x = src_box[0];
		y = src_box[1];
		z = src_box[2];
		w = src_box[3] - src_box[0];
		h = src_box[4] - src_box[1];
		d = src_box[5] - src_box[2];
	}

	if (src_target == GL_FRAMEBUFFER_DEFAULT)
	{
		assert(src_subresource == 0);
		assert(z == 0 && d == 1);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glReadBuffer(src_object);

		if (src_box == nullptr)
		{
			w = _default_fbo_width;
			h = _default_fbo_height;
		}

		GLenum format = src_object == GL_BACK ? _default_color_format : _default_depth_format, type;
		format = convert_upload_format(format, type);

		glReadPixels(x, y, w, h, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
	}
	else if (src_target == GL_RENDERBUFFER)
	{
		assert(src_subresource == 0);

		glBindRenderbuffer(GL_RENDERBUFFER, src_object);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, _copy_fbo[0]);
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, src_object);
		assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		glReadBuffer(GL_COLOR_ATTACHMENT0);

		GLenum format = GL_NONE, type;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		if (src_box == nullptr)
		{
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
			glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &h);
		}

		format = convert_upload_format(format, type);

		glReadPixels(x, y, w, h, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
	}
	else
	{
		glBindTexture(src_target, src_object);

		GLint levels = 1;
		glGetTexParameteriv(src_target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
		const GLuint level = src_subresource % levels;
		const GLuint layer = src_subresource / levels;

		GLenum format = GL_NONE, type;
		glGetTexLevelParameteriv(src_target, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

		const auto row_size_packed = (row_length != 0 ? row_length : w) * api::format_bpp(convert_format(format));
		const auto slice_size_packed = (slice_height != 0 ? slice_height : h) * row_size_packed;
		const auto total_size = d * slice_size_packed;

		format = convert_upload_format(format, type);

		if (src_box == nullptr)
		{
			glGetTexImage(src_target, level, format, type, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}
		else
		{
			switch (src_target)
			{
			case GL_TEXTURE_1D_ARRAY:
				y += layer;
				break;
			case GL_TEXTURE_2D_ARRAY:
				z += layer;
				break;
			}

			assert(gl3wProcs.gl.GetTextureSubImage != nullptr);

			glGetTextureSubImage(src_object, level, x, y, z, w, h, d, format, type, total_size, reinterpret_cast<void *>(static_cast<uintptr_t>(dst_offset)));
		}
	}

	// Restore previous state from application
	glBindBuffer(GL_PIXEL_PACK_BUFFER, previous_pack);
	glPixelStorei(GL_PACK_LSB_FIRST, previous_pack_lsb);
	glPixelStorei(GL_PACK_SWAP_BYTES, previous_pack_swap);
	glPixelStorei(GL_PACK_ALIGNMENT, previous_pack_alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, previous_pack_row_length);
	glPixelStorei(GL_PACK_IMAGE_HEIGHT, previous_pack_image_height);
	glPixelStorei(GL_PACK_SKIP_ROWS, previous_pack_skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, previous_pack_skip_pixels);
	glPixelStorei(GL_PACK_SKIP_IMAGES, previous_pack_skip_images);
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

	copy_texture_region(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
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

void reshade::opengl::device_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert(dsv.handle != 0);

	GLint prev_binding = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding);

	begin_render_pass(0, nullptr, dsv);

	const GLint stencil_value = stencil;

	switch (clear_flags)
	{
	case 0x1:
		glClearBufferfv(GL_DEPTH, 0, &depth);
		break;
	case 0x2:
		glClearBufferiv(GL_STENCIL, 0, &stencil_value);
		break;
	case 0x3:
		glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil_value);
		break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, prev_binding);
}
void reshade::opengl::device_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	GLint prev_binding = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding);

	begin_render_pass(count, rtvs, { 0 });

	for (GLuint i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		glClearBufferfv(GL_COLOR, i, color);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, prev_binding);
}
void reshade::opengl::device_impl::clear_unordered_access_view_uint(api::resource_view, const uint32_t[4])
{
	assert(false);
}
void reshade::opengl::device_impl::clear_unordered_access_view_float(api::resource_view, const float[4])
{
	assert(false);
}

void reshade::opengl::device_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	glBeginQuery(convert_query_type(type), reinterpret_cast<query_heap_impl *>(pool.handle)->queries[index]);
}
void reshade::opengl::device_impl::finish_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	assert(pool.handle != 0);

	if (type == api::query_type::timestamp)
	{
		glQueryCounter(reinterpret_cast<query_heap_impl *>(pool.handle)->queries[index], GL_TIMESTAMP);
	}
	else
	{
		glEndQuery(convert_query_type(type));
	}
}
void reshade::opengl::device_impl::copy_query_results(api::query_pool pool, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	assert(pool.handle != 0);

	for (uint32_t i = 0; i < count; ++i)
	{
		assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
		glGetQueryBufferObjectui64v(reinterpret_cast<query_heap_impl *>(pool.handle)->queries[i + first], dst.handle & 0xFFFFFFFF, GL_QUERY_RESULT_NO_WAIT, static_cast<GLintptr>(dst_offset + i * stride));
	}
}

void reshade::opengl::device_impl::add_debug_marker(const char *label, const float[4])
{
	glDebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
}
void reshade::opengl::device_impl::begin_debug_marker(const char *label, const float[4])
{
	glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, label);
}
void reshade::opengl::device_impl::finish_debug_marker()
{
	glPopDebugGroup();
}
