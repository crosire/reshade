/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "reshade_api_swapchain.hpp"
#include "reshade_api_type_convert.hpp"
#include "opengl_hooks.hpp" // Fix name clashes with gl3w

struct DrawArraysIndirectCommand
{
	GLuint count;
	GLuint primcount;
	GLuint first;
	GLuint baseinstance;
};
struct DrawElementsIndirectCommand
{
	GLuint count;
	GLuint primcount;
	GLuint firstindex;
	GLuint basevertex;
	GLuint baseinstance;
};

extern thread_local reshade::opengl::swapchain_impl *g_current_context;

HOOK_EXPORT void WINAPI glAccum(GLenum op, GLfloat value)
{
	static const auto trampoline = reshade::hooks::call(glAccum);
	trampoline(op, value);
}

HOOK_EXPORT void WINAPI glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = reshade::hooks::call(glAlphaFunc);
	trampoline(func, ref);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::alpha_func, reshade::api::dynamic_state::alpha_reference_value };
		const uint32_t values[2] = { func, *reinterpret_cast<const uint32_t *>(&ref) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}

HOOK_EXPORT auto WINAPI glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glAreTexturesResident);
	return trampoline(n, textures, residences);
}

HOOK_EXPORT void WINAPI glArrayElement(GLint i)
{
	static const auto trampoline = reshade::hooks::call(glArrayElement);
	trampoline(i);
}

HOOK_EXPORT void WINAPI glBegin(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glBegin);
	trampoline(mode);

	assert(g_current_context == nullptr || g_current_context->_current_vertex_count == 0);
}

			void WINAPI glBindBuffer(GLenum target, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffer);
	trampoline(target, buffer);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::resource resource = reshade::opengl::make_resource_handle(target, buffer);
		const uint64_t offset = 0;
		const uint32_t stride = 0;

		switch (target)
		{
		case GL_ARRAY_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_current_context, 0, 1, &resource, &offset, &stride);
			return;
		case GL_ELEMENT_ARRAY_BUFFER:
			// The index format is provided to 'glDrawElements' and is unknown at this point, so call with index size set to zero
			reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(g_current_context, resource, offset, 0);
			return;
		}
	}
#endif
}
			void WINAPI glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glBindBufferBase);
	trampoline(target, index, buffer);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::resource resource = reshade::opengl::make_resource_handle(target, buffer);

		switch (target)
		{
		case GL_UNIFORM_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, index, 1, &resource);
			break;
		}
	}
#endif
}
			void WINAPI glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glBindBufferRange);
	trampoline(target, index, buffer, offset, size);

#if RESHADE_ADDON
	if (g_current_context)
	{
		// TODO: Offset
		const reshade::api::resource resource = reshade::opengl::make_resource_handle(target, buffer);

		switch (target)
		{
		case GL_UNIFORM_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, index, 1, &resource);
			break;
		}
	}
#endif
}
			void WINAPI glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersBase);
	trampoline(target, first, count, buffers);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto buffer_handles = static_cast<reshade::api::resource *>(alloca(count * sizeof(reshade::api::resource)));
		for (GLsizei i = 0; i < count; ++i)
			buffer_handles[i] = buffers != nullptr ? reshade::opengl::make_resource_handle(target, buffers[i]) : reshade::api::resource { 0 };

		switch (target)
		{
		case GL_UNIFORM_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, first, count, buffer_handles);
			break;
		}
	}
#endif
}
			void WINAPI glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLintptr *sizes)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersRange);
	trampoline(target, first, count, buffers, offsets, sizes);

#if RESHADE_ADDON
	if (g_current_context)
	{
		// TODO: Offsets
		const auto buffer_handles = static_cast<reshade::api::resource *>(alloca(count * sizeof(reshade::api::resource)));
		for (GLsizei i = 0; i < count; ++i)
			buffer_handles[i] = buffers != nullptr ? reshade::opengl::make_resource_handle(target, buffers[i]) : reshade::api::resource { 0 };

		switch (target)
		{
		case GL_UNIFORM_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::constant_buffer, first, count, buffer_handles);
			break;
		}
	}
#endif
}

			void WINAPI glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	static const auto trampoline = reshade::hooks::call(glBindFramebuffer);
	trampoline(target, framebuffer);

#if RESHADE_ADDON
	if (g_current_context && (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) &&
		glCheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE) // Skip incomplete frame buffer bindings (e.g. during set up)
	{
		reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(g_current_context);
		reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(g_current_context, reshade::opengl::make_render_pass_handle(framebuffer));
	}
#endif
}

			void WINAPI glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTexture);
	trampoline(unit, texture, level, layered, layer, access, format);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::resource_view view = reshade::opengl::make_resource_view_handle(GL_TEXTURE, texture);

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::unordered_access_view, unit, 1, &view);
	}
#endif
}
			void WINAPI glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto view_handles = static_cast<reshade::api::resource_view *>(alloca(count * sizeof(reshade::api::resource_view)));
		for (GLsizei i = 0; i < count; ++i)
			view_handles[i] = textures != nullptr ? reshade::opengl::make_resource_view_handle(GL_TEXTURE, textures[i]) : reshade::api::resource_view { 0 };

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::unordered_access_view, first, count, view_handles);
	}
#endif
}

			void WINAPI glBindSampler(GLuint unit, GLuint sampler)
{
	static const auto trampoline = reshade::hooks::call(glBindSampler);
	trampoline(unit, sampler);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::sampler sampler_handle = { sampler };

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::sampler, unit, 1, &sampler_handle);
	}
#endif
}
			void WINAPI glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers)
{
	static const auto trampoline = reshade::hooks::call(glBindSamplers);
	trampoline(first, count, samplers);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto sampler_handles = static_cast<reshade::api::sampler *>(alloca(count * sizeof(reshade::api::sampler)));
		for (GLsizei i = 0; i < count; ++i)
			sampler_handles[i] = { samplers != nullptr ? samplers[i] : 0 };

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::sampler, first, count, sampler_handles);
	}
#endif
}

HOOK_EXPORT void WINAPI glBindTexture(GLenum target, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindTexture);
	trampoline(target, texture);

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint unit = GL_TEXTURE0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &unit);
		unit -= GL_TEXTURE0;

		const reshade::api::resource_view view = reshade::opengl::make_resource_view_handle(target, texture);

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, unit, 1, &view);
	}
#endif
}
			void WINAPI glBindTextureUnit(GLuint unit, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindTextureUnit);
	trampoline(unit, texture);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::resource_view view = reshade::opengl::make_resource_view_handle(GL_TEXTURE, texture);

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, unit, 1, &view);
	}
#endif
}
			void WINAPI glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto view_handles = static_cast<reshade::api::resource_view *>(alloca(count * sizeof(reshade::api::resource_view)));
		for (GLsizei i = 0; i < count; ++i)
			view_handles[i] = textures != nullptr ? reshade::opengl::make_resource_view_handle(GL_TEXTURE, textures[i]) : reshade::api::resource_view { 0 };

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, reshade::api::descriptor_type::shader_resource_view, first, count, view_handles);
	}
#endif
}

			void WINAPI glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexBuffer);
	trampoline(bindingindex, buffer, offset, stride);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::resource resource = reshade::opengl::make_resource_handle(GL_ARRAY_BUFFER, buffer);
		const uint64_t offset_64 = offset;
		const uint32_t stride_32 = stride;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_current_context, bindingindex, 1, &resource, &offset_64, &stride_32);
	}

	// TODO: glVertexArrayVertexBuffer
#endif
}
			void WINAPI glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexBuffers);
	trampoline(first, count, buffers, offsets, strides);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto buffer_handles = static_cast<reshade::api::resource *>(alloca(count * sizeof(reshade::api::resource)));
		const auto offsets_64 = static_cast<uint64_t *>(alloca(count * sizeof(uint64_t)));
		for (GLsizei i = 0; i < count; ++i)
		{
			buffer_handles[i] = buffers != nullptr ? reshade::opengl::make_resource_handle(GL_ARRAY_BUFFER, buffers[i]) : reshade::api::resource { 0 };
			offsets_64[i] = offsets[i];
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_current_context, first, count, buffer_handles, offsets_64, reinterpret_cast<const uint32_t *>(strides));
	}

	// TODO: glVertexArrayVertexBuffers
#endif
}

HOOK_EXPORT void WINAPI glBitmap(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
	static const auto trampoline = reshade::hooks::call(glBitmap);
	trampoline(width, height, xorig, yorig, xmove, ymove, bitmap);
}

HOOK_EXPORT void WINAPI glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = reshade::hooks::call(glBlendFunc);
	trampoline(sfactor, dfactor);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::src_color_blend_factor, reshade::api::dynamic_state::dst_color_blend_factor };
		const uint32_t values[2] = { sfactor, dfactor };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}

			void WINAPI glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint src_fbo = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &src_fbo);
		GLint dst_fbo = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);

		const reshade::api::attachment_type type = reshade::opengl::convert_buffer_bits_to_aspect(mask);

		reshade::api::resource_view srv_view = { 0 };
		reshade::api::resource src = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(src_fbo), type, 0, &srv_view);
		g_current_context->get_resource_from_view(srv_view, &src);
		reshade::api::resource_view dst_view = { 0 };
		reshade::api::resource dst = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(dst_fbo), type, 0, &dst_view);
		g_current_context->get_resource_from_view(dst_view, &dst);

		const int32_t src_box[6] = { srcX0, srcY0, 0, srcX1, srcY1, 1 };
		const int32_t dst_box[6] = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

		if (g_current_context->get_resource_desc(src).texture.samples <= 1)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box,
				filter == GL_NONE || filter == GL_NEAREST ? reshade::api::filter_type::min_mag_mip_point : reshade::api::filter_type::min_mag_mip_linear))
				return;
		}
		else
		{
			// TODO: Ensure same extent
			if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, reshade::api::format::unknown))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glBlitFramebuffer);
	trampoline(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
			void WINAPI glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::attachment_type type = reshade::opengl::convert_buffer_bits_to_aspect(mask);

		reshade::api::resource_view srv_view = { 0 };
		reshade::api::resource src = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(readFramebuffer), type, 0, &srv_view);
		g_current_context->get_resource_from_view(srv_view, &src);
		reshade::api::resource_view dst_view = { 0 };
		reshade::api::resource dst = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(drawFramebuffer), type, 0, &dst_view);
		g_current_context->get_resource_from_view(dst_view, &dst);

		const int32_t src_box[6] = { srcX0, srcY0, 0, srcX1, srcY1, 1 };
		const int32_t dst_box[6] = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

		if (g_current_context->get_resource_desc(src).texture.samples <= 1)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box,
				filter == GL_NONE || filter == GL_NEAREST ? reshade::api::filter_type::min_mag_mip_point : reshade::api::filter_type::min_mag_mip_linear))
				return;
		}
		else
		{
			// TODO: Ensure same extent
			if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, reshade::api::format::unknown))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glBlitNamedFramebuffer);
	trampoline(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

			void WINAPI glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glBufferData);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::subresource_data initial_data = { data }; // Row and depth pitch are unused for buffer data

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &usage](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (desc.type != reshade::api::resource_type::buffer)
					return false;
				assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				reshade::opengl::convert_memory_heap_to_usage(desc.heap, usage);
				trampoline(target, static_cast<GLsizeiptr>(desc.buffer.size), (initial_data != nullptr) ? initial_data->data : nullptr, usage);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, size, reshade::opengl::convert_memory_heap_from_usage(usage)), (data != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, size, data, usage);
}

			void WINAPI glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
	static const auto trampoline = reshade::hooks::call(glBufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::subresource_data initial_data = { data };

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &flags](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (desc.type != reshade::api::resource_type::buffer)
					return false;
				assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				reshade::opengl::convert_memory_heap_to_flags(desc.heap, flags);
				trampoline(target, static_cast<GLsizeiptr>(desc.buffer.size), (initial_data != nullptr) ? initial_data->data : nullptr, flags);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, size, reshade::opengl::convert_memory_heap_from_flags(flags)), (data != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, size, data, flags);
}

HOOK_EXPORT void WINAPI glCallList(GLuint list)
{
	static const auto trampoline = reshade::hooks::call(glCallList);
	trampoline(list);
}
HOOK_EXPORT void WINAPI glCallLists(GLsizei n, GLenum type, const GLvoid *lists)
{
	static const auto trampoline = reshade::hooks::call(glCallLists);
	trampoline(n, type, lists);
}

HOOK_EXPORT void WINAPI glClear(GLbitfield mask)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLfloat color_value[4] = {};
		glGetFloatv(GL_COLOR_CLEAR_VALUE, color_value);
		GLfloat depth_value = 0.0f;
		glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_value);
		GLint   stencil_value = 0;
		glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil_value);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_attachments>(g_current_context, reshade::opengl::convert_buffer_bits_to_aspect(mask), color_value, depth_value, static_cast<uint8_t>(stencil_value), 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClear);
	trampoline(mask);
}
			void WINAPI glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_current_context && buffer == GL_COLOR)
	{
		GLint fbo = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		reshade::api::resource_view view = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(fbo), reshade::api::attachment_type::color, drawbuffer, &view);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_current_context, view, value, 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferfv);
	trampoline(buffer, drawbuffer, value);
}
			void WINAPI glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
#if RESHADE_ADDON
	if (g_current_context && buffer != GL_COLOR)
	{
		assert(drawbuffer == 0);

		const auto type = reshade::opengl::convert_buffer_type_to_aspect(buffer);

		GLint fbo = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		reshade::api::resource_view view = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(fbo), type, drawbuffer, &view);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, view, type, depth, static_cast<uint8_t>(stencil), 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferfi);
	trampoline(buffer, drawbuffer, depth, stencil);
}
			void WINAPI glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_current_context && buffer == GL_COLOR)
	{
		reshade::api::resource_view view = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(framebuffer), reshade::api::attachment_type::color, drawbuffer, &view);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_current_context, view, value, 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferfv);
	trampoline(framebuffer, buffer, drawbuffer, value);
}
			void WINAPI glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferfi);

#if RESHADE_ADDON
	if (g_current_context && buffer != GL_COLOR)
	{
		assert(drawbuffer == 0);

		const auto type = reshade::opengl::convert_buffer_type_to_aspect(buffer);

		reshade::api::resource_view view = { 0 };
		g_current_context->get_attachment(reshade::opengl::make_render_pass_handle(framebuffer), type, drawbuffer, &view);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, view, type, depth, static_cast<uint8_t>(stencil), 0, nullptr))
			return;
	}
#endif

	trampoline(framebuffer, buffer, drawbuffer, depth, stencil);
}

HOOK_EXPORT void WINAPI glClearAccum(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(glClearAccum);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	static const auto trampoline = reshade::hooks::call(glClearColor);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glClearDepth(GLclampd depth)
{
	static const auto trampoline = reshade::hooks::call(glClearDepth);
	trampoline(depth);
}
HOOK_EXPORT void WINAPI glClearIndex(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(glClearIndex);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glClearStencil(GLint s)
{
	static const auto trampoline = reshade::hooks::call(glClearStencil);
	trampoline(s);
}

HOOK_EXPORT void WINAPI glClipPlane(GLenum plane, const GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(glClipPlane);
	trampoline(plane, equation);
}

HOOK_EXPORT void WINAPI glColor3b(GLbyte red, GLbyte green, GLbyte blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3b);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3bv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3d(GLdouble red, GLdouble green, GLdouble blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3d);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3f);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3i(GLint red, GLint green, GLint blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3i);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3s(GLshort red, GLshort green, GLshort blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3s);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3ub(GLubyte red, GLubyte green, GLubyte blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3ub);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3ubv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3ui(GLuint red, GLuint green, GLuint blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3ui);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3uiv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor3us(GLushort red, GLushort green, GLushort blue)
{
	static const auto trampoline = reshade::hooks::call(glColor3us);
	trampoline(red, green, blue);
}
HOOK_EXPORT void WINAPI glColor3usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor3usv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4b(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4b);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4bv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4d(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4d);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4f);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4i(GLint red, GLint green, GLint blue, GLint alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4i);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4s(GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4s);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4ub);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4ubv(const GLubyte *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4ubv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4ui(GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4ui);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4uiv(const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4uiv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glColor4us(GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	static const auto trampoline = reshade::hooks::call(glColor4us);
	trampoline(red, green, blue, alpha);
}
HOOK_EXPORT void WINAPI glColor4usv(const GLushort *v)
{
	static const auto trampoline = reshade::hooks::call(glColor4usv);
	trampoline(v);
}

HOOK_EXPORT void WINAPI glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = reshade::hooks::call(glColorMask);
	trampoline(red, green, blue, alpha);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::render_target_write_mask };
		const uint32_t values[1] = { static_cast<uint32_t>((red) | (green << 1) | (blue << 2) | (alpha << 3)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void WINAPI glColorMaterial(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glColorMaterial);
	trampoline(face, mode);
}

HOOK_EXPORT void WINAPI glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glColorPointer);
	trampoline(size, type, stride, pointer);
}

			void WINAPI glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_buffer_region)].empty())
	{
		GLint src_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(readTarget), &src_object);
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(writeTarget), &dst_object);

		if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(g_current_context,
			reshade::opengl::make_resource_handle(readTarget, src_object), readOffset,
			reshade::opengl::make_resource_handle(writeTarget, dst_object), writeOffset, size))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyBufferSubData);
	trampoline(readTarget, writeTarget, readOffset, writeOffset, size);
}

			void WINAPI glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		const int32_t src_box[6] = { srcX, srcY, srcZ, srcX + srcWidth, srcY + srcHeight, srcZ + srcDepth };
		const int32_t dst_box[6] = { dstX, dstY, dstZ, dstX + srcWidth, dstY + srcHeight, dstZ + srcDepth };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(srcTarget, srcName), srcLevel, src_box,
			reshade::opengl::make_resource_handle(dstTarget, dstName), dstLevel, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyImageSubData);
	trampoline(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

			void WINAPI glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_buffer_region)].empty())
	{
		if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_COPY_READ_BUFFER, readBuffer), readOffset,
			reshade::opengl::make_resource_handle(GL_COPY_WRITE_BUFFER, writeBuffer), writeOffset, size))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyNamedBufferSubData);
	trampoline(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

HOOK_EXPORT void WINAPI glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	static const auto trampoline = reshade::hooks::call(glCopyPixels);
	trampoline(x, y, width, height, type);
}

HOOK_EXPORT void WINAPI glCopyTexImage1D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		// TODO: Call "create_resource" event here too
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &dst_object);

		const int32_t src_box[6] = { x, y, 0, x + width, y + 1, 1 };
		const int32_t dst_box[6] = { 0, 0, 0, width, 1, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(target, dst_object), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexImage1D);
	trampoline(target, level, internalFormat, x, y, width, border);
}
HOOK_EXPORT void WINAPI glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		// TODO: Call "create_resource" event here too
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &dst_object);

		const int32_t src_box[6] = { x, y, 0, x + width, y + height, 1 };
		const int32_t dst_box[6] = { 0, 0, 0, width, height, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(target, dst_object), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexImage2D);
	trampoline(target, level, internalFormat, x, y, width, height, border);
}
HOOK_EXPORT void WINAPI glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &dst_object);

		const int32_t src_box[6] = { x, y, 0, x + width, y + 1, 1 };
		const int32_t dst_box[6] = { xoffset, 0, 0, xoffset + width, 1, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(target, dst_object), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage1D);
	trampoline(target, level, xoffset, x, y, width);
}
HOOK_EXPORT void WINAPI glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &dst_object);

		const int32_t src_box[6] = { x, y, 0, x + width, y + height, 1 };
		const int32_t dst_box[6] = { xoffset, yoffset, 0, xoffset + width, yoffset + height, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(target, dst_object), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}
			void WINAPI glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		GLint dst_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &dst_object);

		const int32_t src_box[6] = { x, y, 0, x + width, y + height, 1 };
		const int32_t dst_box[6] = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(target, dst_object), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}
			void WINAPI glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		const int32_t src_box[6] = { x, y, 0, x + width, y + 1, 1 };
		const int32_t dst_box[6] = { xoffset, 0, 0, xoffset + width, 1, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(GL_TEXTURE, texture), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage1D);
	trampoline(texture, level, xoffset, x, y, width);
}
			void WINAPI glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		const int32_t src_box[6] = { x, y, 0, x + width, y + height, 1 };
		const int32_t dst_box[6] = { xoffset, yoffset, 0, xoffset + width, yoffset + height, 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(GL_TEXTURE, texture), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, x, y, width, height);
}
			void WINAPI glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (g_current_context && !reshade::addon::event_list[static_cast<size_t>(reshade::addon_event::copy_texture_region)].empty())
	{
		// TODO: Get actual source object from current FBO and "GL_READ_BUFFER"
		const int32_t src_box[6] = { x, y, 0, x + width, y + height, 1 };
		const int32_t dst_box[6] = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + 1 };

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context,
			reshade::opengl::make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK), 0, src_box,
			reshade::opengl::make_resource_handle(GL_TEXTURE, texture), level, dst_box,
			reshade::api::filter_type::min_mag_mip_point))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

HOOK_EXPORT void WINAPI glCullFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glCullFace);
	trampoline(mode);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::cull_mode };
		const uint32_t values[1] = { mode };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void WINAPI glDeleteLists(GLuint list, GLsizei range)
{
	static const auto trampoline = reshade::hooks::call(glDeleteLists);
	trampoline(list, range);
}

HOOK_EXPORT void WINAPI glDeleteTextures(GLsizei n, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glDeleteTextures);
	trampoline(n, textures);
}

HOOK_EXPORT void WINAPI glDepthFunc(GLenum func)
{
	static const auto trampoline = reshade::hooks::call(glDepthFunc);
	trampoline(func);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_func };
		const uint32_t values[1] = { func };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glDepthMask(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(glDepthMask);
	trampoline(flag);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_write_mask };
		const uint32_t values[1] = { flag };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = reshade::hooks::call(glDepthRange);
	trampoline(zNear, zFar);
}

HOOK_EXPORT void WINAPI glDisable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glDisable);
	trampoline(cap);

#if RESHADE_ADDON
	if (g_current_context)
	{
		uint32_t value = 0;
		reshade::api::dynamic_state state = reshade::api::dynamic_state::unknown;
		switch (cap)
		{
		case GL_ALPHA_TEST:
			state = reshade::api::dynamic_state::alpha_test_enable;
			break;
		case GL_BLEND:
			state = reshade::api::dynamic_state::blend_enable;
			break;
		case GL_CULL_FACE:
			state = reshade::api::dynamic_state::cull_mode;
			break;
		case GL_DEPTH_CLAMP:
			value = 1;
			state = reshade::api::dynamic_state::depth_clip_enable;
			break;
		case GL_DEPTH_TEST:
			state = reshade::api::dynamic_state::depth_enable;
			break;
		case GL_FRAMEBUFFER_SRGB:
			state = reshade::api::dynamic_state::srgb_write_enable;
			break;
		case GL_LINE_SMOOTH:
			state = reshade::api::dynamic_state::antialiased_line_enable;
			break;
		case GL_MULTISAMPLE:
			state = reshade::api::dynamic_state::multisample_enable;
			break;
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			state = reshade::api::dynamic_state::alpha_to_coverage_enable;
			break;
		case GL_SCISSOR_TEST:
			state = reshade::api::dynamic_state::scissor_enable;
			break;
		case GL_STENCIL_TEST:
			state = reshade::api::dynamic_state::stencil_enable;
			break;
		case GL_COLOR_LOGIC_OP:
			state = reshade::api::dynamic_state::logic_op_enable;
			break;
		}

		if (reshade::api::dynamic_state::unknown != state)
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &value);
	}
#endif
}
HOOK_EXPORT void WINAPI glDisableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(glDisableClientState);
	trampoline(array);
}

			void WINAPI glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::invoke_addon_event<reshade::addon_event::dispatch>(g_current_context, num_groups_x, num_groups_y, num_groups_z))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glDispatchCompute);
	trampoline(num_groups_x, num_groups_y, num_groups_z);
}
			void WINAPI glDispatchComputeIndirect(GLintptr indirect)
{
#if RESHADE_ADDON
	GLint indirect_buffer_binding = 0;
	glGetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

	if (0 != indirect_buffer_binding)
	{
		if (g_current_context &&
			reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, 3, reshade::opengl::make_resource_handle(GL_DISPATCH_INDIRECT_BUFFER, indirect_buffer_binding), indirect, 1, 0))
			return;
	}
	else
	{
		assert(false);
		return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDispatchComputeIndirect);
	trampoline(indirect);
}

HOOK_EXPORT void WINAPI glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, count, 1, first, 0))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArrays);
	trampoline(mode, first, count);
}
			void WINAPI glDrawArraysIndirect(GLenum mode, const GLvoid *indirect)
{
#if RESHADE_ADDON
	GLint indirect_buffer_binding = 0;
	glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

	if (0 != indirect_buffer_binding)
	{
		if (g_current_context &&
			reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, 1, reshade::opengl::make_resource_handle(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), 1, 0))
			return;
	}
	else
	{
		const auto cmd = static_cast<const DrawArraysIndirectCommand *>(indirect);
		glDrawArraysInstancedBaseInstance(mode, cmd->first, cmd->count, cmd->primcount, cmd->baseinstance);
		return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysIndirect);
	trampoline(mode, indirect);
}
			void WINAPI glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, primcount, count, first, 0))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstanced);
	trampoline(mode, first, count, primcount);
}
			void WINAPI glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, primcount, count, first, baseinstance))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstancedBaseInstance);
	trampoline(mode, first, count, primcount, baseinstance);
}

HOOK_EXPORT void WINAPI glDrawBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glDrawBuffer);
	trampoline(mode);
}

HOOK_EXPORT void WINAPI glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElements);
	trampoline(mode, count, type, indices);
}
			void WINAPI glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, basevertex);
}
			void WINAPI glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect)
{
#if RESHADE_ADDON
	GLint indirect_buffer_binding = 0;
	glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

	if (0 != indirect_buffer_binding)
	{
		if (g_current_context)
		{
			if (mode != g_current_context->_current_prim_mode)
			{
				const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
				reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
			}

			g_current_context->_current_prim_mode = mode;
			g_current_context->_current_index_type = type;

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, 2, reshade::opengl::make_resource_handle(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), 1, 0))
				return;
		}
	}
	else
	{
		const auto cmd = static_cast<const DrawElementsIndirectCommand *>(indirect);
		glDrawElementsInstancedBaseVertexBaseInstance(mode, cmd->count, type, reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(cmd->firstindex * reshade::opengl::get_index_type_size(type))), cmd->primcount, cmd->basevertex, cmd->baseinstance);
		return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsIndirect);
	trampoline(mode, type, indirect);
}
			void WINAPI glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstanced);
	trampoline(mode, count, type, indices, primcount);
}
			void WINAPI glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseVertex);
	trampoline(mode, count, type, indices, primcount, basevertex);
}
			void WINAPI glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, 0, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseInstance);
	trampoline(mode, count, type, indices, primcount, baseinstance);
}
			void WINAPI glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, basevertex, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseVertexBaseInstance);
	trampoline(mode, count, type, indices, primcount, basevertex, baseinstance);
}

HOOK_EXPORT void WINAPI glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glDrawPixels);
	trampoline(width, height, format, type, pixels);
}

			void WINAPI glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawRangeElements);
	trampoline(mode, start, end, count, type, indices);
}
			void WINAPI glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (mode != g_current_context->_current_prim_mode)
		{
			const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
		}

		g_current_context->_current_prim_mode = mode;
		g_current_context->_current_index_type = type;

		GLint index_buffer_binding = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, index_buffer_binding != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawRangeElementsBaseVertex);
	trampoline(mode, start, end, count, type, indices, basevertex);
}

HOOK_EXPORT void WINAPI glEdgeFlag(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlag);
	trampoline(flag);
}
HOOK_EXPORT void WINAPI glEdgeFlagPointer(GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlagPointer);
	trampoline(stride, pointer);
}
HOOK_EXPORT void WINAPI glEdgeFlagv(const GLboolean *flag)
{
	static const auto trampoline = reshade::hooks::call(glEdgeFlagv);
	trampoline(flag);
}

HOOK_EXPORT void WINAPI glEnable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glEnable);
	trampoline(cap);

#if RESHADE_ADDON
	if (g_current_context)
	{
		uint32_t value = 1;
		reshade::api::dynamic_state state = { reshade::api::dynamic_state::unknown };
		switch (cap)
		{
		case GL_ALPHA_TEST:
			state = reshade::api::dynamic_state::alpha_test_enable;
			break;
		case GL_BLEND:
			state = reshade::api::dynamic_state::blend_enable;
			break;
		case GL_CULL_FACE:
			state = reshade::api::dynamic_state::cull_mode;
			break;
		case GL_DEPTH_CLAMP:
			value = 0;
			state = reshade::api::dynamic_state::depth_clip_enable;
			break;
		case GL_DEPTH_TEST:
			state = reshade::api::dynamic_state::depth_enable;
			break;
		case GL_FRAMEBUFFER_SRGB:
			state = reshade::api::dynamic_state::srgb_write_enable;
			break;
		case GL_LINE_SMOOTH:
			state = reshade::api::dynamic_state::antialiased_line_enable;
			break;
		case GL_MULTISAMPLE:
			state = reshade::api::dynamic_state::multisample_enable;
			break;
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
			state = reshade::api::dynamic_state::alpha_to_coverage_enable;
			break;
		case GL_SCISSOR_TEST:
			state = reshade::api::dynamic_state::scissor_enable;
			break;
		case GL_STENCIL_TEST:
			state = reshade::api::dynamic_state::stencil_enable;
			break;
		case GL_COLOR_LOGIC_OP:
			state = reshade::api::dynamic_state::logic_op_enable;
			break;
		}

		if (reshade::api::dynamic_state::unknown != state)
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &value);
	}
#endif
}
HOOK_EXPORT void WINAPI glEnableClientState(GLenum array)
{
	static const auto trampoline = reshade::hooks::call(glEnableClientState);
	trampoline(array);
}

HOOK_EXPORT void WINAPI glEnd()
{
	static const auto trampoline = reshade::hooks::call(glEnd);
	trampoline();

	if (g_current_context)
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, g_current_context->_current_vertex_count, 1, 0, 0); // Cannot be skipped
#endif

		g_current_context->_current_vertex_count = 0;
	}
}

HOOK_EXPORT void WINAPI glEndList()
{
	static const auto trampoline = reshade::hooks::call(glEndList);
	trampoline();
}

HOOK_EXPORT void WINAPI glEvalCoord1d(GLdouble u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1d);
	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1dv);
	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1f(GLfloat u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1f);
	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord1fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord1fv);
	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord2d(GLdouble u, GLdouble v)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2d);
	trampoline(u, v);
}
HOOK_EXPORT void WINAPI glEvalCoord2dv(const GLdouble *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2dv);
	trampoline(u);
}
HOOK_EXPORT void WINAPI glEvalCoord2f(GLfloat u, GLfloat v)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2f);
	trampoline(u, v);
}
HOOK_EXPORT void WINAPI glEvalCoord2fv(const GLfloat *u)
{
	static const auto trampoline = reshade::hooks::call(glEvalCoord2fv);
	trampoline(u);
}

HOOK_EXPORT void WINAPI glEvalMesh1(GLenum mode, GLint i1, GLint i2)
{
	static const auto trampoline = reshade::hooks::call(glEvalMesh1);
	trampoline(mode, i1, i2);
}
HOOK_EXPORT void WINAPI glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	static const auto trampoline = reshade::hooks::call(glEvalMesh2);
	trampoline(mode, i1, i2, j1, j2);
}

HOOK_EXPORT void WINAPI glEvalPoint1(GLint i)
{
	static const auto trampoline = reshade::hooks::call(glEvalPoint1);
	trampoline(i);
}
HOOK_EXPORT void WINAPI glEvalPoint2(GLint i, GLint j)
{
	static const auto trampoline = reshade::hooks::call(glEvalPoint2);
	trampoline(i, j);
}

HOOK_EXPORT void WINAPI glFeedbackBuffer(GLsizei size, GLenum type, GLfloat *buffer)
{
	static const auto trampoline = reshade::hooks::call(glFeedbackBuffer);
	trampoline(size, type, buffer);
}

HOOK_EXPORT void WINAPI glFinish()
{
	static const auto trampoline = reshade::hooks::call(glFinish);
	trampoline();
}
HOOK_EXPORT void WINAPI glFlush()
{
	static const auto trampoline = reshade::hooks::call(glFlush);
	trampoline();
}

HOOK_EXPORT void WINAPI glFogf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glFogf);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glFogfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glFogfv);
	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glFogi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glFogi);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glFogiv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glFogiv);
	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glFrontFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glFrontFace);
	trampoline(mode);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::front_counter_clockwise };
		const uint32_t values[1] = { mode == GL_CCW };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void WINAPI glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(glFrustum);
	trampoline(left, right, bottom, top, zNear, zFar);
}

			void WINAPI glGenerateMipmap(GLenum target)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_current_context, reshade::opengl::make_resource_view_handle(target, object)))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glGenerateMipmap);
	trampoline(target);
}
			void WINAPI glGenerateTextureMipmap(GLuint texture)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_current_context, reshade::opengl::make_resource_view_handle(GL_TEXTURE, texture)))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glGenerateTextureMipmap);
	trampoline(texture);
}

HOOK_EXPORT auto WINAPI glGenLists(GLsizei range) -> GLuint
{
	static const auto trampoline = reshade::hooks::call(glGenLists);
	return trampoline(range);
}

HOOK_EXPORT void WINAPI glGenTextures(GLsizei n, GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glGenTextures);
	trampoline(n, textures);
}

HOOK_EXPORT void WINAPI glGetBooleanv(GLenum pname, GLboolean *params)
{
	static const auto trampoline = reshade::hooks::call(glGetBooleanv);
	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetDoublev(GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glGetDoublev);
	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetFloatv(GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetFloatv);
	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glGetIntegerv(GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetIntegerv);
	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glGetClipPlane(GLenum plane, GLdouble *equation)
{
	static const auto trampoline = reshade::hooks::call(glGetClipPlane);
	trampoline(plane, equation);
}

HOOK_EXPORT auto WINAPI glGetError() -> GLenum
{
	static const auto trampoline = reshade::hooks::call(glGetError);
	return trampoline();
}

HOOK_EXPORT void WINAPI glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetLightfv);
	trampoline(light, pname, params);
}
HOOK_EXPORT void WINAPI glGetLightiv(GLenum light, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetLightiv);
	trampoline(light, pname, params);
}

HOOK_EXPORT void WINAPI glGetMapdv(GLenum target, GLenum query, GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapdv);
	trampoline(target, query, v);
}
HOOK_EXPORT void WINAPI glGetMapfv(GLenum target, GLenum query, GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapfv);
	trampoline(target, query, v);
}
HOOK_EXPORT void WINAPI glGetMapiv(GLenum target, GLenum query, GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glGetMapiv);
	trampoline(target, query, v);
}

HOOK_EXPORT void WINAPI glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetMaterialfv);
	trampoline(face, pname, params);
}
HOOK_EXPORT void WINAPI glGetMaterialiv(GLenum face, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetMaterialiv);
	trampoline(face, pname, params);
}

HOOK_EXPORT void WINAPI glGetPixelMapfv(GLenum map, GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapfv);
	trampoline(map, values);
}
HOOK_EXPORT void WINAPI glGetPixelMapuiv(GLenum map, GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapuiv);
	trampoline(map, values);
}
HOOK_EXPORT void WINAPI glGetPixelMapusv(GLenum map, GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(glGetPixelMapusv);
	trampoline(map, values);
}

HOOK_EXPORT void WINAPI glGetPointerv(GLenum pname, GLvoid **params)
{
	static const auto trampoline = reshade::hooks::call(glGetPointerv);
	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glGetPolygonStipple(GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(glGetPolygonStipple);
	trampoline(mask);
}

HOOK_EXPORT auto WINAPI glGetString(GLenum name) -> const GLubyte *
{
	static const auto trampoline = reshade::hooks::call(glGetString);
	return trampoline(name);
}

HOOK_EXPORT void WINAPI glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexEnvfv);
	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexEnviv);
	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glGetTexGendv(GLenum coord, GLenum pname, GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGendv);
	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexGenfv(GLenum coord, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGenfv);
	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexGeniv(GLenum coord, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexGeniv);
	trampoline(coord, pname, params);
}

HOOK_EXPORT void WINAPI glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glGetTexImage);
	trampoline(target, level, format, type, pixels);
}

HOOK_EXPORT void WINAPI glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexLevelParameterfv);
	trampoline(target, level, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexLevelParameteriv);
	trampoline(target, level, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexParameterfv);
	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glGetTexParameteriv);
	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glHint);
	trampoline(target, mode);
}

HOOK_EXPORT void WINAPI glIndexMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glIndexMask);
	trampoline(mask);
}

HOOK_EXPORT void WINAPI glIndexPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glIndexPointer);
	trampoline(type, stride, pointer);
}

HOOK_EXPORT void WINAPI glIndexd(GLdouble c)
{
	static const auto trampoline = reshade::hooks::call(glIndexd);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexdv(const GLdouble *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexdv);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexf(GLfloat c)
{
	static const auto trampoline = reshade::hooks::call(glIndexf);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexfv(const GLfloat *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexfv);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexi(GLint c)
{
	static const auto trampoline = reshade::hooks::call(glIndexi);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexiv(const GLint *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexiv);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexs(GLshort c)
{
	static const auto trampoline = reshade::hooks::call(glIndexs);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexsv(const GLshort *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexsv);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexub(GLubyte c)
{
	static const auto trampoline = reshade::hooks::call(glIndexub);
	trampoline(c);
}
HOOK_EXPORT void WINAPI glIndexubv(const GLubyte *c)
{
	static const auto trampoline = reshade::hooks::call(glIndexubv);
	trampoline(c);
}

HOOK_EXPORT void WINAPI glInitNames()
{
	static const auto trampoline = reshade::hooks::call(glInitNames);
	trampoline();
}

HOOK_EXPORT void WINAPI glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glInterleavedArrays);
	trampoline(format, stride, pointer);
}

HOOK_EXPORT auto WINAPI glIsEnabled(GLenum cap) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsEnabled);
	return trampoline(cap);
}

HOOK_EXPORT auto WINAPI glIsList(GLuint list) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsList);
	return trampoline(list);
}

HOOK_EXPORT auto WINAPI glIsTexture(GLuint texture) -> GLboolean
{
	static const auto trampoline = reshade::hooks::call(glIsTexture);
	return trampoline(texture);
}

HOOK_EXPORT void WINAPI glLightModelf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glLightModelf);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glLightModelfv(GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glLightModelfv);
	trampoline(pname, params);
}
HOOK_EXPORT void WINAPI glLightModeli(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glLightModeli);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glLightModeliv(GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glLightModeliv);
	trampoline(pname, params);
}

HOOK_EXPORT void WINAPI glLightf(GLenum light, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glLightf);
	trampoline(light, pname, param);
}
HOOK_EXPORT void WINAPI glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glLightfv);
	trampoline(light, pname, params);
}
HOOK_EXPORT void WINAPI glLighti(GLenum light, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glLighti);
	trampoline(light, pname, param);
}
HOOK_EXPORT void WINAPI glLightiv(GLenum light, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glLightiv);
	trampoline(light, pname, params);
}

HOOK_EXPORT void WINAPI glLineStipple(GLint factor, GLushort pattern)
{
	static const auto trampoline = reshade::hooks::call(glLineStipple);
	trampoline(factor, pattern);
}
HOOK_EXPORT void WINAPI glLineWidth(GLfloat width)
{
	static const auto trampoline = reshade::hooks::call(glLineWidth);
	trampoline(width);
}

HOOK_EXPORT void WINAPI glListBase(GLuint base)
{
	static const auto trampoline = reshade::hooks::call(glListBase);
	trampoline(base);
}

HOOK_EXPORT void WINAPI glLoadIdentity()
{
	static const auto trampoline = reshade::hooks::call(glLoadIdentity);
	trampoline();
}
HOOK_EXPORT void WINAPI glLoadMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(glLoadMatrixd);
	trampoline(m);
}
HOOK_EXPORT void WINAPI glLoadMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(glLoadMatrixf);
	trampoline(m);
}

HOOK_EXPORT void WINAPI glLoadName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(glLoadName);
	trampoline(name);
}

HOOK_EXPORT void WINAPI glLogicOp(GLenum opcode)
{
	static const auto trampoline = reshade::hooks::call(glLogicOp);
	trampoline(opcode);
}

HOOK_EXPORT void WINAPI glMap1d(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(glMap1d);
	trampoline(target, u1, u2, stride, order, points);
}
HOOK_EXPORT void WINAPI glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(glMap1f);
	trampoline(target, u1, u2, stride, order, points);
}
HOOK_EXPORT void WINAPI glMap2d(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
	static const auto trampoline = reshade::hooks::call(glMap2d);
	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}
HOOK_EXPORT void WINAPI glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
	static const auto trampoline = reshade::hooks::call(glMap2f);
	trampoline(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

HOOK_EXPORT void WINAPI glMapGrid1d(GLint un, GLdouble u1, GLdouble u2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid1d);
	trampoline(un, u1, u2);
}
HOOK_EXPORT void WINAPI glMapGrid1f(GLint un, GLfloat u1, GLfloat u2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid1f);
	trampoline(un, u1, u2);
}
HOOK_EXPORT void WINAPI glMapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid2d);
	trampoline(un, u1, u2, vn, v1, v2);
}
HOOK_EXPORT void WINAPI glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(glMapGrid2f);
	trampoline(un, u1, u2, vn, v1, v2);
}

HOOK_EXPORT void WINAPI glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glMaterialf);
	trampoline(face, pname, param);
}
HOOK_EXPORT void WINAPI glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glMaterialfv);
	trampoline(face, pname, params);
}
HOOK_EXPORT void WINAPI glMateriali(GLenum face, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glMateriali);
	trampoline(face, pname, param);
}
HOOK_EXPORT void WINAPI glMaterialiv(GLenum face, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glMaterialiv);
	trampoline(face, pname, params);
}

HOOK_EXPORT void WINAPI glMatrixMode(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glMatrixMode);
	trampoline(mode);
}

HOOK_EXPORT void WINAPI glMultMatrixd(const GLdouble *m)
{
	static const auto trampoline = reshade::hooks::call(glMultMatrixd);
	trampoline(m);
}
HOOK_EXPORT void WINAPI glMultMatrixf(const GLfloat *m)
{
	static const auto trampoline = reshade::hooks::call(glMultMatrixf);
	trampoline(m);
}

			void WINAPI glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawArrays(mode, first[i], count[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawArrays);
	trampoline(mode, first, count, drawcount);
#endif
}
			void WINAPI glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{

#if RESHADE_ADDON
	GLint indirect_buffer_binding = 0;
	glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

	if (0 != indirect_buffer_binding)
	{
		if (g_current_context &&
			reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, 1, reshade::opengl::make_resource_handle(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), drawcount, stride))
			return;
	}
	else
	{
		for (GLsizei i = 0; i < drawcount; ++i)
		{
			const auto cmd = reinterpret_cast<const DrawArraysIndirectCommand *>(static_cast<const uint8_t *>(indirect) + i * (stride != 0 ? stride : sizeof(DrawArraysIndirectCommand)));
			glDrawArraysInstancedBaseInstance(mode, cmd->first, cmd->count, cmd->primcount, cmd->baseinstance);
		}
		return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawArraysIndirect);
	trampoline(mode, indirect, drawcount, stride);
}
			void WINAPI glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawElements(mode, count[i], type, indices[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawElements);
	trampoline(mode, count, type, indices, drawcount);
#endif
}
			void WINAPI glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawElementsBaseVertex(mode, count[i], type, indices[i], basevertex[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, drawcount, basevertex);
#endif
}
			void WINAPI glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
#if RESHADE_ADDON
	GLint indirect_buffer_binding = 0;
	glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

	if (0 != indirect_buffer_binding)
	{
		if (g_current_context)
		{
			if (mode != g_current_context->_current_prim_mode)
			{
				const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
				reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &mode);
			}

			g_current_context->_current_prim_mode = mode;
			g_current_context->_current_index_type = type;

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, 2, reshade::opengl::make_resource_handle(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), drawcount, stride))
				return;
		}
	}
	else
	{
		for (GLsizei i = 0; i < drawcount; ++i)
		{
			const auto cmd = reinterpret_cast<const DrawElementsIndirectCommand *>(static_cast<const uint8_t *>(indirect) + i * (stride != 0 ? stride : sizeof(DrawElementsIndirectCommand)));
			glDrawElementsInstancedBaseVertexBaseInstance(mode, cmd->count, type, reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(cmd->firstindex * reshade::opengl::get_index_type_size(type))), cmd->primcount, cmd->basevertex, cmd->baseinstance);
		}
		return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawElementsIndirect);
	trampoline(mode, type, indirect, drawcount, stride);
}

			void WINAPI glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferData);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::subresource_data initial_data = { data };

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[buffer, &usage](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (desc.type != reshade::api::resource_type::buffer)
					return false;
				assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				reshade::opengl::convert_memory_heap_to_usage(desc.heap, usage);
				trampoline(buffer, static_cast<GLsizeiptr>(desc.buffer.size), (initial_data != nullptr) ? initial_data->data : nullptr, usage);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_BUFFER, size, reshade::opengl::convert_memory_heap_from_usage(usage)), (data != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(buffer, size, data, usage);
}

			void WINAPI glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::subresource_data initial_data = { data };

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[buffer, &flags](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (desc.type != reshade::api::resource_type::buffer)
					return false;
				assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				reshade::opengl::convert_memory_heap_to_flags(desc.heap, flags);
				trampoline(buffer, static_cast<GLsizeiptr>(desc.buffer.size), (initial_data != nullptr) ? initial_data->data : nullptr, flags);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_BUFFER, size, reshade::opengl::convert_memory_heap_from_flags(flags)), (data != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(buffer, size, data, flags);
}

			void WINAPI glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[renderbuffer, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_RENDERBUFFER, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(renderbuffer, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_RENDERBUFFER, 1, 1, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(renderbuffer, internalformat, width, height);
}
			void WINAPI glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorageMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[renderbuffer, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_RENDERBUFFER, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(renderbuffer, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_RENDERBUFFER, 1, samples, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(renderbuffer, samples, internalformat, width, height);
}

HOOK_EXPORT void WINAPI glNewList(GLuint list, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glNewList);
	trampoline(list, mode);
}

HOOK_EXPORT void WINAPI glNormal3b(GLbyte nx, GLbyte ny, GLbyte nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3b);
	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3bv(const GLbyte *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3bv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3d(GLdouble nx, GLdouble ny, GLdouble nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3d);
	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3f);
	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3i(GLint nx, GLint ny, GLint nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3i);
	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glNormal3s(GLshort nx, GLshort ny, GLshort nz)
{
	static const auto trampoline = reshade::hooks::call(glNormal3s);
	trampoline(nx, ny, nz);
}
HOOK_EXPORT void WINAPI glNormal3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glNormal3sv);
	trampoline(v);
}

HOOK_EXPORT void WINAPI glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glNormalPointer);
	trampoline(type, stride, pointer);
}

HOOK_EXPORT void WINAPI glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	static const auto trampoline = reshade::hooks::call(glOrtho);
	trampoline(left, right, bottom, top, zNear, zFar);
}

HOOK_EXPORT void WINAPI glPassThrough(GLfloat token)
{
	static const auto trampoline = reshade::hooks::call(glPassThrough);
	trampoline(token);
}

HOOK_EXPORT void WINAPI glPixelMapfv(GLenum map, GLsizei mapsize, const GLfloat *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapfv);
	trampoline(map, mapsize, values);
}
HOOK_EXPORT void WINAPI glPixelMapuiv(GLenum map, GLsizei mapsize, const GLuint *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapuiv);
	trampoline(map, mapsize, values);
}
HOOK_EXPORT void WINAPI glPixelMapusv(GLenum map, GLsizei mapsize, const GLushort *values)
{
	static const auto trampoline = reshade::hooks::call(glPixelMapusv);
	trampoline(map, mapsize, values);
}

HOOK_EXPORT void WINAPI glPixelStoref(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glPixelStoref);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glPixelStorei(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glPixelStorei);
	trampoline(pname, param);
}

HOOK_EXPORT void WINAPI glPixelTransferf(GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glPixelTransferf);
	trampoline(pname, param);
}
HOOK_EXPORT void WINAPI glPixelTransferi(GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glPixelTransferi);
	trampoline(pname, param);
}

HOOK_EXPORT void WINAPI glPixelZoom(GLfloat xfactor, GLfloat yfactor)
{
	static const auto trampoline = reshade::hooks::call(glPixelZoom);
	trampoline(xfactor, yfactor);
}

HOOK_EXPORT void WINAPI glPointSize(GLfloat size)
{
	static const auto trampoline = reshade::hooks::call(glPointSize);
	trampoline(size);
}

HOOK_EXPORT void WINAPI glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glPolygonMode);
	trampoline(face, mode);

#if RESHADE_ADDON
	if (g_current_context && face == GL_FRONT_AND_BACK)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::fill_mode };
		const uint32_t values[1] = { mode };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = reshade::hooks::call(glPolygonOffset);
	trampoline(factor, units);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::depth_bias_slope_scaled, reshade::api::dynamic_state::depth_bias };
		const uint32_t values[2] = { *reinterpret_cast<const uint32_t *>(&factor), *reinterpret_cast<const uint32_t *>(&units) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glPolygonStipple(const GLubyte *mask)
{
	static const auto trampoline = reshade::hooks::call(glPolygonStipple);
	trampoline(mask);
}

HOOK_EXPORT void WINAPI glPopAttrib()
{
	static const auto trampoline = reshade::hooks::call(glPopAttrib);
	trampoline();
}
HOOK_EXPORT void WINAPI glPopClientAttrib()
{
	static const auto trampoline = reshade::hooks::call(glPopClientAttrib);
	trampoline();
}

HOOK_EXPORT void WINAPI glPopMatrix()
{
	static const auto trampoline = reshade::hooks::call(glPopMatrix);
	trampoline();
}

HOOK_EXPORT void WINAPI glPopName()
{
	static const auto trampoline = reshade::hooks::call(glPopName);
	trampoline();
}

HOOK_EXPORT void WINAPI glPrioritizeTextures(GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
	static const auto trampoline = reshade::hooks::call(glPrioritizeTextures);
	trampoline(n, textures, priorities);
}

HOOK_EXPORT void WINAPI glPushAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(glPushAttrib);
	trampoline(mask);
}
HOOK_EXPORT void WINAPI glPushClientAttrib(GLbitfield mask)
{
	static const auto trampoline = reshade::hooks::call(glPushClientAttrib);
	trampoline(mask);
}

HOOK_EXPORT void WINAPI glPushMatrix()
{
	static const auto trampoline = reshade::hooks::call(glPushMatrix);
	trampoline();
}

HOOK_EXPORT void WINAPI glPushName(GLuint name)
{
	static const auto trampoline = reshade::hooks::call(glPushName);
	trampoline(name);
}

HOOK_EXPORT void WINAPI glRasterPos2d(GLdouble x, GLdouble y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2d);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2f(GLfloat x, GLfloat y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2f);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2i(GLint x, GLint y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2i);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos2s(GLshort x, GLshort y)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2s);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glRasterPos2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos2sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3d(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3d);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3f(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3f);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3i(GLint x, GLint y, GLint z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3i);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos3s(GLshort x, GLshort y, GLshort z)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3s);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glRasterPos3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos3sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4d);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4f);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4i(GLint x, GLint y, GLint z, GLint w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4i);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glRasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4s);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glRasterPos4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glRasterPos4sv);
	trampoline(v);
}

HOOK_EXPORT void WINAPI glReadBuffer(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glReadBuffer);
	trampoline(mode);
}

HOOK_EXPORT void WINAPI glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glReadPixels);
	trampoline(x, y, width, height, format, type, pixels);
}

HOOK_EXPORT void WINAPI glRectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	static const auto trampoline = reshade::hooks::call(glRectd);
	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectdv(const GLdouble *v1, const GLdouble *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectdv);
	trampoline(v1, v2);
}
HOOK_EXPORT void WINAPI glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	static const auto trampoline = reshade::hooks::call(glRectf);
	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectfv(const GLfloat *v1, const GLfloat *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectfv);
	trampoline(v1, v2);
}
HOOK_EXPORT void WINAPI glRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	static const auto trampoline = reshade::hooks::call(glRecti);
	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectiv(const GLint *v1, const GLint *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectiv);
	trampoline(v1, v2);
}

HOOK_EXPORT void WINAPI glRects(GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	static const auto trampoline = reshade::hooks::call(glRects);
	trampoline(x1, y1, x2, y2);
}
HOOK_EXPORT void WINAPI glRectsv(const GLshort *v1, const GLshort *v2)
{
	static const auto trampoline = reshade::hooks::call(glRectsv);
	trampoline(v1, v2);
}

			void WINAPI glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, 1, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, internalformat, width, height);
}
			void WINAPI glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorageMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, samples, internalformat, width, height);
}

HOOK_EXPORT auto WINAPI glRenderMode(GLenum mode) -> GLint
{
	static const auto trampoline = reshade::hooks::call(glRenderMode);
	return trampoline(mode);
}

HOOK_EXPORT void WINAPI glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glRotated);
	trampoline(angle, x, y, z);
}
HOOK_EXPORT void WINAPI glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glRotatef);
	trampoline(angle, x, y, z);
}

HOOK_EXPORT void WINAPI glScaled(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glScaled);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glScalef);
	trampoline(x, y, z);
}

HOOK_EXPORT void WINAPI glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissor);
	trampoline(x, y, width, height);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const int32_t rect_data[4] = { x, y, x + width, y + height };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, 0, 1, rect_data);
	}
#endif

}
			void WINAPI glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glScissorArrayv);
	trampoline(first, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const auto rect_data = static_cast<int32_t *>(alloca(count * sizeof(int32_t) * 4));
		for (GLsizei i = 0, k = 0; i < count; ++i, k += 4)
		{
			rect_data[k + 0] = v[k + 0];
			rect_data[k + 1] = v[k + 2];
			rect_data[k + 2] = v[k + 0] + v[k + 3];
			rect_data[k + 3] = v[k + 2] + v[k + 4];
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, first, count, rect_data);
	}
#endif
}
			void WINAPI glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexed);
	trampoline(index, left, bottom, width, height);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const int32_t rect_data[4] = { left, bottom, left + width, bottom + height };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, index, 1, rect_data);
	}
#endif
}
			void WINAPI glScissorIndexedv(GLuint index, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexedv);
	trampoline(index, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const int32_t rect_data[4] = { v[0], v[1], v[0] + v[2], v[1] + v[3] };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, index, 1, rect_data);
	}
#endif
}

HOOK_EXPORT void WINAPI glSelectBuffer(GLsizei size, GLuint *buffer)
{
	static const auto trampoline = reshade::hooks::call(glSelectBuffer);
	trampoline(size, buffer);
}

HOOK_EXPORT void WINAPI glShadeModel(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glShadeModel);
	trampoline(mode);
}

			void WINAPI glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
	static const auto trampoline = reshade::hooks::call(glShaderSource);

#if RESHADE_ADDON
	if (g_current_context)
	{
		std::string combined_source;
		if (length != nullptr)
		{
			combined_source.reserve(length[0]);
			for (GLsizei i = 0; i < count; ++i)
				combined_source.append(string[i], length[i]);
		}
		else
		{
			for (GLsizei i = 0; i < count; ++i)
				combined_source.append(string[i]);
		}

		GLint type = GL_NONE;
		glGetShaderiv(shader, GL_SHADER_TYPE, &type);

		reshade::api::shader_desc *shader_desc = nullptr;
		reshade::api::pipeline_desc desc = {};
		switch (type)
		{
		case GL_VERTEX_SHADER:
			desc.type = reshade::api::pipeline_stage::vertex_shader;
			shader_desc = &desc.graphics.vertex_shader;
			break;
		case GL_TESS_CONTROL_SHADER:
			desc.type = reshade::api::pipeline_stage::hull_shader;
			shader_desc = &desc.graphics.hull_shader;
			break;
		case GL_TESS_EVALUATION_SHADER:
			desc.type = reshade::api::pipeline_stage::domain_shader;
			shader_desc = &desc.graphics.domain_shader;
			break;
		case GL_GEOMETRY_SHADER:
			desc.type = reshade::api::pipeline_stage::geometry_shader;
			shader_desc = &desc.graphics.geometry_shader;
			break;
		case GL_FRAGMENT_SHADER:
			desc.type = reshade::api::pipeline_stage::pixel_shader;
			shader_desc = &desc.graphics.pixel_shader;
			break;
		case GL_COMPUTE_SHADER:
			desc.type = reshade::api::pipeline_stage::compute_shader;
			shader_desc = &desc.compute.shader;
			break;
		default:
			assert(false);
			break;
		}

		shader_desc->code = combined_source.data();
		shader_desc->code_size = combined_source.size();
		shader_desc->format = reshade::api::shader_format::glsl;
		shader_desc->entry_point = "main";

		reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(
			[shader, count, string, length, &combined_source, type](reshade::api::device *, const reshade::api::pipeline_desc &desc) {
				const reshade::api::shader_desc *shader_desc = nullptr;
				switch (type)
				{
				case GL_VERTEX_SHADER:
					shader_desc = &desc.graphics.vertex_shader;
					break;
				case GL_TESS_CONTROL_SHADER:
					shader_desc = &desc.graphics.hull_shader;
					break;
				case GL_TESS_EVALUATION_SHADER:
					shader_desc = &desc.graphics.domain_shader;
					break;
				case GL_GEOMETRY_SHADER:
					shader_desc = &desc.graphics.geometry_shader;
					break;
				case GL_FRAGMENT_SHADER:
					shader_desc = &desc.graphics.pixel_shader;
					break;
				case GL_COMPUTE_SHADER:
					shader_desc = &desc.compute.shader;
					break;
				}

				if (shader_desc->code == combined_source.c_str() &&
					shader_desc->code_size == combined_source.size())
				{
					const auto combined_source_p = combined_source.c_str();
					const auto combined_source_length = static_cast<GLint>(combined_source.size());
					trampoline(shader, count, &combined_source_p, &combined_source_length);
				}
				else
				{
					trampoline(shader, count, string, length);
				}
				return true;
			}, g_current_context, desc);
		return;
	}
#endif

	trampoline(shader, count, string, length);
}

HOOK_EXPORT void WINAPI glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilFunc);
	trampoline(func, ref, mask);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[4] = { reshade::api::dynamic_state::front_stencil_func, reshade::api::dynamic_state::back_stencil_func, reshade::api::dynamic_state::stencil_reference_value, reshade::api::dynamic_state::stencil_read_mask };
		const uint32_t values[4] = { func, func, static_cast<uint32_t>(ref), mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 4, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glStencilMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilMask);
	trampoline(mask);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::stencil_write_mask };
		const uint32_t values[1] = { mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void WINAPI glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = reshade::hooks::call(glStencilOp);
	trampoline(fail, zfail, zpass);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_fail_op, reshade::api::dynamic_state::back_stencil_fail_op, reshade::api::dynamic_state::front_stencil_depth_fail_op, reshade::api::dynamic_state::back_stencil_depth_fail_op, reshade::api::dynamic_state::front_stencil_pass_op, reshade::api::dynamic_state::back_stencil_pass_op };
		const uint32_t values[6] = { fail, fail, zfail, zfail, zpass, zpass };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 6, states, values);
	}
#endif
}

			void WINAPI glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glTexBuffer);

#if RESHADE_ADDON
	if (g_current_context)
	{
		// The target of the original buffer is unknown at this point, so use "GL_BUFFER" as a placeholder instead for the resource handle
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[target, &internalformat](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage, const reshade::api::resource_view_desc &desc) {
				if (!reshade::opengl::check_resource_view_desc(target, desc, internalformat))
					return false;
				if (desc.buffer.offset != 0 || desc.buffer.size != 0)
				{
					static const auto trampoline_range = reshade::hooks::call(glTexBufferRange);
					trampoline_range(target, internalformat, resource.handle & 0xFFFFFFFF, static_cast<GLsizeiptr>(desc.buffer.offset), static_cast<GLintptr>(desc.buffer.size));
				}
				else
				{
					trampoline(target, internalformat, resource.handle & 0xFFFFFFFF);
				}
				return true;
			}, g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, reshade::opengl::convert_resource_view_desc(target, internalformat, 0, 0));
		return;
	}
#endif

	trampoline(target, internalformat, buffer);
}
			void WINAPI glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glTextureBuffer);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[texture, &internalformat](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage, const reshade::api::resource_view_desc &desc) {
				if (!reshade::opengl::check_resource_view_desc(GL_TEXTURE_BUFFER, desc, internalformat))
					return false;
				if (desc.buffer.offset != 0 || desc.buffer.size != 0)
				{
					static const auto trampoline_range = reshade::hooks::call(glTextureBufferRange);
					trampoline_range(texture, internalformat, resource.handle & 0xFFFFFFFF, static_cast<GLsizeiptr>(desc.buffer.offset), static_cast<GLintptr>(desc.buffer.size));
				}
				else
				{
					trampoline(texture, internalformat, resource.handle & 0xFFFFFFFF);
				}
				return true;
			}, g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, reshade::opengl::convert_resource_view_desc(GL_TEXTURE_BUFFER, internalformat, 0, 0));
		return;
	}
#endif

	trampoline(texture, internalformat, buffer);
}

			void WINAPI glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTexBufferRange);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[target, &internalformat](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage, const reshade::api::resource_view_desc &desc) {
				if (!reshade::opengl::check_resource_view_desc(target, desc, internalformat))
					return false;
				trampoline(target, internalformat, resource.handle & 0xFFFFFFFF, static_cast<GLsizeiptr>(desc.buffer.offset), static_cast<GLintptr>(desc.buffer.size));
				return true;
			}, g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, reshade::opengl::convert_resource_view_desc(target, internalformat, offset, size));
		return;
	}
#endif

	trampoline(target, internalformat, buffer, offset, size);
}
			void WINAPI glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTextureBufferRange);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[texture, &internalformat](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage, const reshade::api::resource_view_desc &desc) {
				if (!reshade::opengl::check_resource_view_desc(GL_TEXTURE_BUFFER, desc, internalformat))
					return false;
				trampoline(texture, internalformat, resource.handle & 0xFFFFFFFF, static_cast<GLsizeiptr>(desc.buffer.offset), static_cast<GLintptr>(desc.buffer.size));
				return true;
			}, g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, reshade::opengl::convert_resource_view_desc(GL_TEXTURE_BUFFER, internalformat, offset, size));
		return;
	}
#endif

	trampoline(texture, internalformat, buffer, offset, size);
}

HOOK_EXPORT void WINAPI glTexCoord1d(GLdouble s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1d);
	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1f(GLfloat s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1f);
	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1i(GLint s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1i);
	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord1s(GLshort s)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1s);
	trampoline(s);
}
HOOK_EXPORT void WINAPI glTexCoord1sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord1sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2d(GLdouble s, GLdouble t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2d);
	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2f(GLfloat s, GLfloat t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2f);
	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2i(GLint s, GLint t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2i);
	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord2s(GLshort s, GLshort t)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2s);
	trampoline(s, t);
}
HOOK_EXPORT void WINAPI glTexCoord2sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord2sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3d(GLdouble s, GLdouble t, GLdouble r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3d);
	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3f);
	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3i(GLint s, GLint t, GLint r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3i);
	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord3s(GLshort s, GLshort t, GLshort r)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3s);
	trampoline(s, t, r);
}
HOOK_EXPORT void WINAPI glTexCoord3sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord3sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4d(GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4d);
	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4dv(const GLdouble *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4f);
	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4fv(const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4i(GLint s, GLint t, GLint r, GLint q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4i);
	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4iv(const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glTexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4s);
	trampoline(s, t, r, q);
}
HOOK_EXPORT void WINAPI glTexCoord4sv(const GLshort *v)
{
	static const auto trampoline = reshade::hooks::call(glTexCoord4sv);
	trampoline(v);
}

HOOK_EXPORT void WINAPI glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glTexCoordPointer);
	trampoline(size, type, stride, pointer);
}

HOOK_EXPORT void WINAPI glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvf);
	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvfv);
	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexEnvi);
	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexEnviv);
	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glTexGend(GLenum coord, GLenum pname, GLdouble param)
{
	static const auto trampoline = reshade::hooks::call(glTexGend);
	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGendv(GLenum coord, GLenum pname, const GLdouble *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGendv);
	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glTexGenf(GLenum coord, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexGenf);
	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGenfv);
	trampoline(coord, pname, params);
}
HOOK_EXPORT void WINAPI glTexGeni(GLenum coord, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexGeni);
	trampoline(coord, pname, param);
}
HOOK_EXPORT void WINAPI glTexGeniv(GLenum coord, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexGeniv);
	trampoline(coord, pname, params);
}

HOOK_EXPORT void WINAPI glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexImage1D);

	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_RG:
		internalformat = GL_RG8;
		break;
	case GL_RGB:
		internalformat = GL_RGB8;
		break;
	case GL_RGBA:
		internalformat = GL_RGBA8;
		break;
	case GL_DEPTH_STENCIL:
		internalformat = GL_DEPTH24_STENCIL8;
		break;
	case GL_DEPTH_COMPONENT:
		internalformat = GL_DEPTH_COMPONENT16;
		break;
	}

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint unpack = 0;
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);

		const reshade::api::subresource_data initial_data = reshade::opengl::convert_mapped_subresource(format, type, pixels, width);

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, level, &internalformat, border, format, type, pixels, unpack](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, reinterpret_cast<GLenum &>(internalformat)) || (unpack != 0 && initial_data != nullptr))
					return false;
				// Assumes that the row and depth pitch are equivalent to the 'format' and 'type' options
				trampoline(target, level, internalformat, desc.texture.width, border, format, type, (unpack != 0) ? pixels : (initial_data != nullptr) ? initial_data->data : nullptr);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, 1, static_cast<GLenum>(internalformat), width), (unpack == 0 && pixels != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, level, internalformat, width, border, format, type, pixels);
}
HOOK_EXPORT void WINAPI glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexImage2D);

	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_RG:
		internalformat = GL_RG8;
		break;
	case GL_RGB:
		internalformat = GL_RGB8;
		break;
	case GL_RGBA:
		internalformat = GL_RGBA8;
		break;
	case GL_DEPTH_STENCIL:
		internalformat = GL_DEPTH24_STENCIL8;
		break;
	case GL_DEPTH_COMPONENT:
		internalformat = GL_DEPTH_COMPONENT16;
		break;
	}

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint unpack = 0;
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);

		const reshade::api::subresource_data initial_data = reshade::opengl::convert_mapped_subresource(format, type, pixels, width, height);

		// TODO: This handles every mipmap of a texture initialized via multiple calls to 'glTexImage2D' as a separate resource, which is not technically correct
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, level, &internalformat, border, format, type, pixels, unpack](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, reinterpret_cast<GLenum &>(internalformat)) || (unpack != 0 && initial_data != nullptr))
					return false;
				// Assumes that the row and depth pitch are equivalent to the 'format' and 'type' options
				trampoline(target, level, internalformat, desc.texture.width, desc.texture.height, border, format, type, (unpack != 0) ? pixels : (initial_data != nullptr) ? initial_data->data : nullptr);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, 1, static_cast<GLenum>(internalformat), width, height), (unpack == 0 && pixels != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, level, internalformat, width, height, border, format, type, pixels);
}
			void WINAPI glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height, fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
			void WINAPI glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexImage3D);

	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_RG:
		internalformat = GL_RG8;
		break;
	case GL_RGB:
		internalformat = GL_RGB8;
		break;
	case GL_RGBA:
		internalformat = GL_RGBA8;
		break;
	case GL_DEPTH_STENCIL:
		internalformat = GL_DEPTH24_STENCIL8;
		break;
	case GL_DEPTH_COMPONENT:
		internalformat = GL_DEPTH_COMPONENT16;
		break;
	}

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint unpack = 0;
		glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);

		const reshade::api::subresource_data initial_data = reshade::opengl::convert_mapped_subresource(format, type, pixels, width, height, depth);

		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, level, &internalformat, border, format, type, pixels, unpack](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, reinterpret_cast<GLenum &>(internalformat)) || (unpack != 0 && initial_data != nullptr))
					return false;
				// Assumes that the row and depth pitch are equivalent to the 'format' and 'type' options
				trampoline(
					target, level, internalformat,
					desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6,
					border, format, type,
					(unpack != 0) ? pixels : (initial_data != nullptr) ? initial_data->data : nullptr);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, 1, static_cast<GLenum>(internalformat), width, height, depth), (unpack == 0 && pixels != nullptr) ? &initial_data : nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);
}
			void WINAPI glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(
					target, desc.texture.samples, internalformat,
					desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6,
					fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height, depth), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

HOOK_EXPORT void WINAPI glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	static const auto trampoline = reshade::hooks::call(glTexParameterf);
	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	static const auto trampoline = reshade::hooks::call(glTexParameterfv);
	trampoline(target, pname, params);
}
HOOK_EXPORT void WINAPI glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	static const auto trampoline = reshade::hooks::call(glTexParameteri);
	trampoline(target, pname, param);
}
HOOK_EXPORT void WINAPI glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	static const auto trampoline = reshade::hooks::call(glTexParameteriv);
	trampoline(target, pname, params);
}

HOOK_EXPORT void WINAPI glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexSubImage1D);
	trampoline(target, level, xoffset, width, format, type, pixels);
}
HOOK_EXPORT void WINAPI glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

			void WINAPI glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage1D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.levels, internalformat, desc.texture.width);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, levels, internalformat, width);
}
			void WINAPI glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.levels, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, levels, internalformat, width, height);
}
			void WINAPI glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height, fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
			void WINAPI glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(target, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.levels, internalformat, desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width, height, depth), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, levels, internalformat, width, height, depth);
}
			void WINAPI glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[target, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_1D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(target, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6,
					fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height, depth), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}
			void WINAPI glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage1D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[texture, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_1D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(texture, desc.texture.levels, internalformat, desc.texture.width);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_TEXTURE_1D, levels, 1, internalformat, width), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(texture, levels, internalformat, width);
}
			void WINAPI glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[texture, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_2D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(texture, desc.texture.levels, internalformat, desc.texture.width, desc.texture.height);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_TEXTURE_2D, levels, 1, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(texture, levels, internalformat, width, height);
}
			void WINAPI glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[texture, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_2D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(texture, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height, fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_TEXTURE_2D, 1, samples, internalformat, width, height), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(texture, samples, internalformat, width, height, fixedsamplelocations);
}
			void WINAPI glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[texture, &internalformat](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_3D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(texture, desc.texture.levels, internalformat, desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_TEXTURE_3D, levels, 1, internalformat, width, height, depth), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(texture, levels, internalformat, width, height, depth);
}
			void WINAPI glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::create_resource>(
			[texture, &internalformat, fixedsamplelocations](reshade::api::device *, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data, reshade::api::resource_usage) {
				if (!reshade::opengl::check_resource_desc(GL_TEXTURE_3D, desc, internalformat) || initial_data != nullptr)
					return false;
				trampoline(texture, desc.texture.samples, internalformat, desc.texture.width, desc.texture.height,
					(desc.flags & reshade::api::resource_flags::cube_compatible) != reshade::api::resource_flags::cube_compatible ? desc.texture.depth_or_layers : desc.texture.depth_or_layers / 6,
					fixedsamplelocations);
				return true;
			}, g_current_context, reshade::opengl::convert_resource_desc(GL_TEXTURE_3D, 1, samples, internalformat, width, height, depth), nullptr, reshade::api::resource_usage::undefined);
		return;
	}
#endif

	trampoline(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

			void WINAPI glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
	static const auto trampoline = reshade::hooks::call(glTextureView);

#if RESHADE_ADDON
	if (g_current_context)
	{
		// The target of the original texture is unknown at this point, so use "GL_TEXTURE" as a placeholder instead for the resource handle
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[texture, target, &internalformat](reshade::api::device *, reshade::api::resource resource, reshade::api::resource_usage, const reshade::api::resource_view_desc &desc) {
				if (!reshade::opengl::check_resource_view_desc(target, desc, internalformat))
					return false;
				trampoline(texture, target, resource.handle & 0xFFFFFFFF, internalformat, desc.texture.first_level, desc.texture.levels, desc.texture.first_layer, desc.texture.layers);
				return true;
			}, g_current_context, reshade::opengl::make_resource_handle(GL_TEXTURE, origtexture), reshade::api::resource_usage::undefined, reshade::opengl::convert_resource_view_desc(target, internalformat, minlevel, numlevels, minlayer, numlayers));
		return;
	}
#endif

	trampoline(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

HOOK_EXPORT void WINAPI glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	static const auto trampoline = reshade::hooks::call(glTranslated);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	static const auto trampoline = reshade::hooks::call(glTranslatef);
	trampoline(x, y, z);
}

			void WINAPI glUniform1f(GLint location, GLfloat v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1f);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2f);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3f);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4f);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform1i(GLint location, GLint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1i);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform2i(GLint location, GLint v0, GLint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2i);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3i);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4i);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform1ui(GLint location, GLuint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1ui);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 1, v);
	}
#endif
}
			void WINAPI glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2ui);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 2, v);
	}
#endif
}
			void WINAPI glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3ui);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 3, v);
	}
#endif
}
			void WINAPI glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4ui);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 4, v);
	}
#endif
}
			void WINAPI glUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 1 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 2 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 3 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 0, location, 4 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform1iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 1 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform2iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 2 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform3iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 3 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform4iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 4 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
			void WINAPI glUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 1 * count, v);
	}
#endif
}
			void WINAPI glUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 2 * count, v);
	}
#endif
}
			void WINAPI glUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 3 * count, v);
	}
#endif
}
			void WINAPI glUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(g_current_context, reshade::api::shader_stage::all, reshade::api::pipeline_layout { 0 }, 1, location, 4 * count, v);
	}
#endif
}

			void WINAPI glUseProgram(GLuint program)
{
	static const auto trampoline = reshade::hooks::call(glUseProgram);
	trampoline(program);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(g_current_context,
			reshade::api::pipeline_stage::vertex_shader | reshade::api::pipeline_stage::hull_shader | reshade::api::pipeline_stage::domain_shader | reshade::api::pipeline_stage::geometry_shader | reshade::api::pipeline_stage::pixel_shader | reshade::api::pipeline_stage::compute_shader,
			reshade::api::pipeline { program });
	}
#endif
}

HOOK_EXPORT void WINAPI glVertex2d(GLdouble x, GLdouble y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2d);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2f(GLfloat x, GLfloat y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2f);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2i(GLint x, GLint y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2i);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex2s(GLshort x, GLshort y)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2s);
	trampoline(x, y);
}
HOOK_EXPORT void WINAPI glVertex2sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 2;

	static const auto trampoline = reshade::hooks::call(glVertex2sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3d);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3f);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3i(GLint x, GLint y, GLint z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3i);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex3s(GLshort x, GLshort y, GLshort z)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3s);
	trampoline(x, y, z);
}
HOOK_EXPORT void WINAPI glVertex3sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 3;

	static const auto trampoline = reshade::hooks::call(glVertex3sv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4d);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4dv(const GLdouble *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4dv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4f);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4fv(const GLfloat *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4fv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4i(GLint x, GLint y, GLint z, GLint w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4i);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4iv(const GLint *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4iv);
	trampoline(v);
}
HOOK_EXPORT void WINAPI glVertex4s(GLshort x, GLshort y, GLshort z, GLshort w)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4s);
	trampoline(x, y, z, w);
}
HOOK_EXPORT void WINAPI glVertex4sv(const GLshort *v)
{
	if (g_current_context)
		g_current_context->_current_vertex_count += 4;

	static const auto trampoline = reshade::hooks::call(glVertex4sv);
	trampoline(v);
}

HOOK_EXPORT void WINAPI glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	static const auto trampoline = reshade::hooks::call(glVertexPointer);
	trampoline(size, type, stride, pointer);
}

HOOK_EXPORT void WINAPI glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glViewport);
	trampoline(x, y, width, height);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const float viewport_data[4] = {
			static_cast<float>(x),
			static_cast<float>(y),
			static_cast<float>(width),
			static_cast<float>(height)
		};

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_current_context, 0, 1, viewport_data);
	}
#endif
}
			void WINAPI glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glViewportArrayv);
	trampoline(first, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto viewport_data = static_cast<float *>(alloca(count * sizeof(float) * 6));
		for (GLsizei i = 0, k = 0; i < count; ++i, k += 6, v += 4)
		{
			viewport_data[k + 0] = v[0];
			viewport_data[k + 1] = v[1];
			viewport_data[k + 2] = v[2];
			viewport_data[k + 3] = v[3];
			viewport_data[k + 4] = 0.0f; // This is set via 'glDepthRange', so just assume defaults here for now
			viewport_data[k + 5] = 1.0f;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_current_context, first, count, viewport_data);
	}
#endif
}
			void WINAPI glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
	static const auto trampoline = reshade::hooks::call(glViewportIndexedf);
	trampoline(index, x, y, w, h);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const float viewport_data[4] = { x, y, w, h };

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_current_context, 0, 1, viewport_data);
	}
#endif
}
			void WINAPI glViewportIndexedfv(GLuint index, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glViewportIndexedfv);
	trampoline(index, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_current_context, 0, 1, v);
	}
#endif
}
