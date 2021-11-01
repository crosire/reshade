/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook_manager.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"
#include "opengl_hooks.hpp" // Fix name clashes with gl3w
#include <malloc.h>

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

// Initialize thread local variable in this translation unit, to avoid the compiler generating calls to '__dyn_tls_on_demand_init' on every use in the frequently called functions below
thread_local reshade::opengl::swapchain_impl *g_current_context = nullptr;

#if RESHADE_ADDON
#define gl3wGetFloatv gl3wProcs.gl.GetFloatv
#define gl3wGetIntegerv gl3wProcs.gl.GetIntegerv
#define gl3wTexParameteriv gl3wProcs.gl.TexParameteriv
#define gl3wGetTexParameteriv gl3wProcs.gl.GetTexParameteriv
#define gl3wGetTextureParameteriv gl3wProcs.gl.GetTextureParameteriv
#define gl3wNamedFramebufferTexture gl3wProcs.gl.NamedFramebufferTexture
#define gl3wNamedFramebufferRenderbuffer gl3wProcs.gl.NamedFramebufferRenderbuffer

static void init_resource(GLenum target, GLuint object, const reshade::api::resource_desc &desc, const reshade::api::subresource_data *initial_data = nullptr, bool update_texture = false)
{
	if (!g_current_context || (!reshade::has_addon_event<reshade::addon_event::init_resource>() && !reshade::has_addon_event<reshade::addon_event::init_resource_view>() && !update_texture))
		return;

	// Get actual texture target from the texture object
	if (target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
		gl3wGetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));

	GLenum base_target = target;
	switch (target)
	{
	case GL_ARRAY_BUFFER:
	case GL_ELEMENT_ARRAY_BUFFER:
	case GL_PIXEL_PACK_BUFFER:
	case GL_PIXEL_UNPACK_BUFFER:
	case GL_UNIFORM_BUFFER:
	case GL_TRANSFORM_FEEDBACK_BUFFER:
	case GL_COPY_READ_BUFFER:
	case GL_COPY_WRITE_BUFFER:
	case GL_DRAW_INDIRECT_BUFFER:
	case GL_SHADER_STORAGE_BUFFER:
	case GL_DISPATCH_INDIRECT_BUFFER:
	case GL_QUERY_BUFFER:
	case GL_ATOMIC_COUNTER_BUFFER:
		base_target = GL_BUFFER;
		break;
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		base_target = GL_TEXTURE_CUBE_MAP;
		break;
	}

	const reshade::api::resource resource = reshade::opengl::make_resource_handle(base_target, object);

	if (update_texture && initial_data != nullptr)
	{
		assert(base_target != GL_BUFFER);
		g_current_context->update_texture_region(*initial_data, resource, 0, nullptr);
	}

	reshade::invoke_addon_event<reshade::addon_event::init_resource>(
		g_current_context, desc, initial_data, reshade::api::resource_usage::general, resource);

	if (base_target == GL_BUFFER)
		return;

	const reshade::api::resource_usage usage_type = (target == GL_RENDERBUFFER) ? reshade::api::resource_usage::render_target : reshade::api::resource_usage::undefined;

	// Register all possible views of this texture too
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		g_current_context, resource, usage_type, reshade::api::resource_view_desc(reshade::opengl::convert_resource_view_type(base_target), desc.texture.format, 0, 0xFFFFFFFF, 0, 0xFFFFFFFF), reshade::opengl::make_resource_view_handle(base_target, object));

	if (base_target == GL_TEXTURE_CUBE_MAP)
	{
		for (GLuint face = 0; face < 6; ++face)
		{
			const GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				g_current_context, resource, usage_type, reshade::api::resource_view_desc(reshade::opengl::convert_resource_view_type(face_target), desc.texture.format, 0, 0xFFFFFFFF, face, 1), reshade::opengl::make_resource_view_handle(face_target, object));
		}
	}
}
static void init_resource_view(GLenum target, GLuint object, const reshade::api::resource_view_desc &desc, GLenum orig_target, GLuint orig)
{
	if (!g_current_context || !reshade::has_addon_event<reshade::addon_event::init_resource_view>())
		return;

	// Get actual texture target from the texture object
	if (orig != 0 && orig_target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
		gl3wGetTextureParameteriv(orig, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&orig_target));

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));

	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
		g_current_context, reshade::opengl::make_resource_handle(orig_target, orig), reshade::api::resource_usage::undefined, desc, reshade::opengl::make_resource_view_handle(target, object));
}
static void destroy_resource_or_view(GLenum target, GLuint object)
{
	if (!g_current_context || (!reshade::has_addon_event<reshade::addon_event::destroy_resource>() && !reshade::has_addon_event<reshade::addon_event::destroy_resource_view>()))
		return;

	if (target != GL_BUFFER)
	{
		// Get actual texture target from the texture object
		if (target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
			gl3wGetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

		reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(g_current_context, reshade::opengl::make_resource_view_handle(target, object));

		if (target == GL_TEXTURE_CUBE_MAP)
		{
			for (GLuint face = 0; face < 6; ++face)
			{
				const GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(g_current_context, reshade::opengl::make_resource_view_handle(face_target, object));
			}
		}
	}

	if (target != GL_TEXTURE_BUFFER)
	{
		reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(g_current_context, reshade::opengl::make_resource_handle(target, object));
	}
}

static reshade::api::subresource_data convert_mapped_subresource(GLenum format, GLenum type, const void *pixels, std::vector<uint8_t> *temp_data, GLsizei width, GLsizei height = 1, GLsizei depth = 1)
{
	GLint unpack = 0;
	gl3wGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);

	if (0 != unpack || pixels == nullptr)
		return {};

	GLint row_length = 0;
	gl3wGetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
	GLint slice_height = 0;
	gl3wGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &slice_height);

	if (0 != row_length)
		width = row_length;
	if (0 != slice_height)
		height = slice_height;

	reshade::api::subresource_data result;
	result.data = const_cast<void *>(pixels);

	// Convert RGB to RGBA format (GL_RGB -> GL_RGBA, GL_BGR -> GL_BRGA, etc.)
	const bool convert_rgb_to_rgba = (format == GL_RGB || format == GL_RGB_INTEGER || format == GL_BGR || format == GL_BGR_INTEGER) && type == GL_UNSIGNED_BYTE;
	if (convert_rgb_to_rgba && temp_data != nullptr)
	{
		format += 1;

		temp_data->resize(width * height * depth * 4);
		for (GLint z = 0; z < depth; ++z)
		{
			for (GLint y = 0; y < height; ++y)
			{
				for (GLint x = 0; x < width; ++x)
				{
					const GLsizei in_index = (z * width * height + y * width + x) * 3;
					const GLsizei out_index = (z * width * height + y * width + x) * 4;

					for (GLint c = 0; c < 3; ++c)
						(*temp_data)[out_index + c] = static_cast<const uint8_t *>(pixels)[in_index + c];
					(*temp_data)[out_index + 3] = 0xFF;
				}
			}
		}

		result.data = temp_data->data();
	}

	const auto pixels_format = reshade::opengl::convert_format(format, type);

	result.row_pitch = reshade::api::format_row_pitch(pixels_format, width);
	result.slice_pitch = reshade::api::format_slice_pitch(pixels_format, result.row_pitch, height);

	GLint skip_rows = 0;
	gl3wGetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
	GLint skip_slices = 0;
	gl3wGetIntegerv(GL_UNPACK_SKIP_IMAGES, &skip_slices);
	GLint skip_pixels = 0;
	gl3wGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);

	result.data = static_cast<uint8_t *>(result.data) +
		skip_rows * result.row_pitch +
		skip_slices * result.slice_pitch +
		skip_pixels * (result.row_pitch / width);

	return result;
}

static bool copy_buffer_region(GLenum src_target, GLuint src_object, GLintptr src_offset, GLenum dst_target, GLuint dst_object, GLintptr dst_offset, GLsizeiptr size)
{
	if (!g_current_context || !reshade::has_addon_event<reshade::addon_event::copy_buffer_region>())
		return false;

	if (src_object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(src_target), reinterpret_cast<GLint *>(&src_object));
	if (dst_object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(dst_target), reinterpret_cast<GLint *>(&dst_object));

	reshade::api::resource src = reshade::opengl::make_resource_handle(GL_BUFFER, src_object);
	reshade::api::resource dst = reshade::opengl::make_resource_handle(GL_BUFFER, dst_object);

	return reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(g_current_context, src, src_offset, dst, dst_offset, size);

}
static bool copy_texture_region(GLenum src_target, GLuint src_object, GLint src_level, GLint x, GLint y, GLint z, GLenum dst_target, GLuint dst_object, GLint dst_level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum filter = GL_NONE)
{
	if (!g_current_context || !reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
		return false;

	if (src_object == 0)
	{
		if (src_target == GL_FRAMEBUFFER_DEFAULT)
			gl3wGetIntegerv(GL_READ_BUFFER, reinterpret_cast<GLint *>(&src_object));
		else
			gl3wGetIntegerv(reshade::opengl::get_binding_for_target(src_target), reinterpret_cast<GLint *>(&src_object));
	}
	else
	{
		if (src_target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
			gl3wGetTextureParameteriv(src_object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&src_target));
	}

	if (dst_object == 0)
	{
		if (src_target == GL_FRAMEBUFFER_DEFAULT)
			gl3wGetIntegerv(GL_DRAW_BUFFER, reinterpret_cast<GLint *>(&dst_object));
		else
			gl3wGetIntegerv(reshade::opengl::get_binding_for_target(dst_target), reinterpret_cast<GLint *>(&dst_object));
	}
	else
	{
		if (dst_target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
			gl3wGetTextureParameteriv(dst_object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&dst_target));
	}

	reshade::api::resource src = reshade::opengl::make_resource_handle(src_target, src_object);
	// Check if this is actually referencing the default framebuffer, or really an attachment of a framebuffer object
	if (src_target == GL_FRAMEBUFFER_DEFAULT && (src_object >= GL_COLOR_ATTACHMENT0 && src_object <= GL_COLOR_ATTACHMENT31))
	{
		GLint src_fbo = 0;
		gl3wGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &src_fbo);
		assert(src_fbo != 0);

		src = g_current_context->get_resource_from_view(g_current_context->get_framebuffer_attachment(
			reshade::opengl::make_framebuffer_handle(src_fbo), reshade::api::attachment_type::color, src_object - GL_COLOR_ATTACHMENT0));
	}

	reshade::api::resource dst = reshade::opengl::make_resource_handle(dst_target, dst_object);
	if (dst_target == GL_FRAMEBUFFER_DEFAULT && (dst_object >= GL_COLOR_ATTACHMENT0 && dst_object <= GL_COLOR_ATTACHMENT31))
	{
		GLint dst_fbo = 0;
		gl3wGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);
		assert(dst_fbo != 0);

		dst = g_current_context->get_resource_from_view(g_current_context->get_framebuffer_attachment(
			reshade::opengl::make_framebuffer_handle(dst_fbo), reshade::api::attachment_type::color, dst_object - GL_COLOR_ATTACHMENT0));
	}

	const int32_t src_box[6] = { x, y, z, x + width, y + height, z + depth };
	const int32_t dst_box[6] = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + depth };

	const reshade::api::filter_mode filter_mode = (filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear;

	return reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context, src, src_level, src_box, dst, dst_level, dst_box, filter_mode);
}
static bool update_buffer_region(GLenum target, GLuint object, GLintptr offset, GLsizeiptr size, const void *data)
{
	if (!g_current_context || !reshade::has_addon_event<reshade::addon_event::update_buffer_region>())
		return false;

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));

	reshade::api::resource dst = reshade::opengl::make_resource_handle(GL_BUFFER, object);

	return reshade::invoke_addon_event<reshade::addon_event::update_buffer_region>(g_current_context, data, dst, static_cast<uint64_t>(offset), static_cast<uint64_t>(size));
}
static bool update_texture_region(GLenum target, GLuint object, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	if (!g_current_context || !reshade::has_addon_event<reshade::addon_event::update_texture_region>())
		return false;

	// Get actual texture target from the texture object
	if (target == GL_TEXTURE && gl3wGetTextureParameteriv != nullptr)
		gl3wGetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));

	GLenum base_target = target;
	switch (target)
	{
	case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
	case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
	case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		base_target = GL_TEXTURE_CUBE_MAP;
		break;
	}

	reshade::api::resource dst = reshade::opengl::make_resource_handle(base_target, object);

	GLuint subresource = level;
	if (base_target == GL_TEXTURE_CUBE_MAP)
	{
		subresource += (target - GL_TEXTURE_CUBE_MAP_POSITIVE_X) * g_current_context->get_resource_desc(dst).texture.levels;
	}

	const int32_t dst_box[6] = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + depth };

	std::vector<uint8_t> temp_data;
	return reshade::invoke_addon_event<reshade::addon_event::update_texture_region>(g_current_context, convert_mapped_subresource(format, type, pixels, &temp_data, width, height, depth), dst, subresource, dst_box);
}

static void update_framebuffer_object(GLenum target, GLuint fbo)
{
	if (!g_current_context)
		return;

	// Get object from current binding in case it was not specified
	if (fbo == 0)
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&fbo));

	const auto fbo_handle = reshade::opengl::make_framebuffer_handle(fbo);

	reshade::invoke_addon_event<reshade::addon_event::destroy_framebuffer>(g_current_context, fbo_handle);

	std::vector<reshade::api::resource_view> old_attachments(8 + 1);
	for (uint32_t i = 0; i < 8; ++i)
		old_attachments[i] = g_current_context->get_framebuffer_attachment(fbo_handle, reshade::api::attachment_type::color, i);
	old_attachments[8] = g_current_context->get_framebuffer_attachment(fbo_handle, reshade::api::attachment_type::depth, 0);
	std::vector<reshade::api::resource_view> new_attachments = old_attachments;

	if (reshade::invoke_addon_event<reshade::addon_event::create_framebuffer>(g_current_context, reshade::api::render_pass { 0 }, static_cast<uint32_t>(new_attachments.size()), new_attachments.data()))
	{
		// Update depth-stencil attachment
		{
			if (new_attachments[8] != old_attachments[8])
			{
				if ((new_attachments[8].handle >> 40) == GL_RENDERBUFFER)
					gl3wNamedFramebufferRenderbuffer(fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, new_attachments[8].handle & 0xFFFFFFFF);
				else
					gl3wNamedFramebufferTexture(fbo, GL_DEPTH_STENCIL_ATTACHMENT, new_attachments[8].handle & 0xFFFFFFFF, 0);
			}
		}

		// Update render target attachments
		for (uint32_t i = 0; i < 8; ++i)
		{
			if (new_attachments[i] != new_attachments[i])
			{
				if ((new_attachments[i].handle >> 40) == GL_RENDERBUFFER)
					gl3wNamedFramebufferRenderbuffer(fbo, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, new_attachments[i].handle & 0xFFFFFFFF);
				else
					gl3wNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, new_attachments[i].handle & 0xFFFFFFFF, 0);
			}
		}
	}

	reshade::invoke_addon_event<reshade::addon_event::init_framebuffer>(g_current_context, reshade::api::render_pass { 0 }, static_cast<uint32_t>(new_attachments.size()), new_attachments.data(), fbo_handle);
}

static void update_current_primitive_topology(GLenum mode, GLenum type)
{
	assert(g_current_context != nullptr);
	g_current_context->_current_index_type = type;

	if (mode != g_current_context->_current_prim_mode)
	{
		g_current_context->_current_prim_mode = mode;

		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::opengl::convert_primitive_topology(mode));

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, &state, &value);
	}
}

static __forceinline GLuint get_index_buffer_offset(const GLvoid *indices)
{
	return g_current_context->_current_ibo != 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(g_current_context->_current_index_type)) : 0;
}
#endif

#ifdef GL_VERSION_1_0
HOOK_EXPORT void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_ALPHA:
		internalformat = GL_ALPHA8;
		break;
	case GL_LUMINANCE:
		internalformat = GL_LUMINANCE8;
		break;
	case GL_LUMINANCE_ALPHA:
		internalformat = GL_LUMINANCE8_ALPHA8;
		break;
	case GL_INTENSITY:
		internalformat = GL_INTENSITY8;
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

	static const auto trampoline = reshade::hooks::call(glTexImage1D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_1D);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		gl3wGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		std::vector<uint8_t> temp_data;

		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width, 1, 1, swizzle_mask);
		auto initial_data = convert_mapped_subresource(format, type, pixels, &temp_data, width);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format, swizzle_mask);
			width = desc.texture.width;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			pixels = nullptr;

			gl3wTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
		}

		trampoline(target, level, internalformat, width, border, format, type, pixels);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr, initial_data.data != pixels);
	}
	else
#endif
		trampoline(target, level, internalformat, width, border, format, type, pixels);
}
HOOK_EXPORT void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_ALPHA:
		internalformat = GL_ALPHA8;
		break;
	case GL_LUMINANCE:
		internalformat = GL_LUMINANCE8;
		break;
	case GL_LUMINANCE_ALPHA:
		internalformat = GL_LUMINANCE8_ALPHA8;
		break;
	case GL_INTENSITY:
		internalformat = GL_INTENSITY8;
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

	static const auto trampoline = reshade::hooks::call(glTexImage2D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D_ARRAY || target == GL_PROXY_TEXTURE_RECTANGLE || target == GL_PROXY_TEXTURE_CUBE_MAP);
	// Ignore all cube map faces except for the first
	const bool cube_map_face = (target > GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object && !cube_map_face)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		gl3wGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		std::vector<uint8_t> temp_data;

		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width, height, 1, swizzle_mask);
		auto initial_data = convert_mapped_subresource(format, type, pixels, &temp_data, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data && target != GL_TEXTURE_CUBE_MAP_POSITIVE_X ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format, swizzle_mask);
			width = desc.texture.width;
			height = desc.texture.height;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			pixels = nullptr;

			gl3wTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
		}

		trampoline(target, level, internalformat, width, height, border, format, type, pixels);

		init_resource(target, 0, desc, initial_data.data && target != GL_TEXTURE_CUBE_MAP_POSITIVE_X ? &initial_data : nullptr, initial_data.data != pixels);
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, border, format, type, pixels);
}

HOOK_EXPORT void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_FRAMEBUFFER_DEFAULT, 0, 0, 0, 0, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyPixels);
	trampoline(x, y, width, height, type);
}

HOOK_EXPORT void APIENTRY glClear(GLbitfield mask)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::clear_attachments>())
	{
		GLfloat color_value[4] = {};
		gl3wGetFloatv(GL_COLOR_CLEAR_VALUE, color_value);
		GLfloat depth_value = 0.0f;
		gl3wGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_value);
		GLint   stencil_value = 0;
		gl3wGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil_value);

		if (reshade::invoke_addon_event<reshade::addon_event::clear_attachments>(g_current_context,
			reshade::opengl::convert_buffer_bits_to_aspect(mask), color_value, depth_value, static_cast<uint8_t>(stencil_value & 0xFF), 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClear);
	trampoline(mask);
}

HOOK_EXPORT void APIENTRY glEnable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glEnable);
	trampoline(cap);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		uint32_t value = GL_TRUE;
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
			value = GL_FALSE;
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
HOOK_EXPORT void APIENTRY glDisable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glDisable);
	trampoline(cap);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		uint32_t value = GL_FALSE;
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
			value = GL_TRUE;
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

HOOK_EXPORT void APIENTRY glCullFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glCullFace);
	trampoline(mode);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::cull_mode };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_cull_mode(mode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glFrontFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glFrontFace);
	trampoline(mode);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::front_counter_clockwise };
		const uint32_t values[1] = { mode == GL_CCW };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glHint);
	trampoline(target, mode);
}
HOOK_EXPORT void APIENTRY glLineWidth(GLfloat width)
{
	static const auto trampoline = reshade::hooks::call(glLineWidth);
	trampoline(width);
}
HOOK_EXPORT void APIENTRY glPointSize(GLfloat size)
{
	static const auto trampoline = reshade::hooks::call(glPointSize);
	trampoline(size);
}
HOOK_EXPORT void APIENTRY glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glPolygonMode);
	trampoline(face, mode);

#if RESHADE_ADDON
	if (g_current_context && face == GL_FRONT_AND_BACK &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::fill_mode };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_fill_mode(mode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void APIENTRY glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = reshade::hooks::call(glAlphaFunc);
	trampoline(func, ref);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::alpha_func, reshade::api::dynamic_state::alpha_reference_value };
		const uint32_t values[2] = { static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)), *reinterpret_cast<const uint32_t *>(&ref) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = reshade::hooks::call(glBlendFunc);
	trampoline(sfactor, dfactor);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::source_color_blend_factor, reshade::api::dynamic_state::dest_color_blend_factor };
		const uint32_t values[2] = { static_cast<uint32_t>(reshade::opengl::convert_blend_factor(sfactor)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(dfactor)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glLogicOp(GLenum opcode)
{
	static const auto trampoline = reshade::hooks::call(glLogicOp);
	trampoline(opcode);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::logic_op };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_logic_op(opcode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = reshade::hooks::call(glColorMask);
	trampoline(red, green, blue, alpha);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::render_target_write_mask };
		const uint32_t values[1] = { static_cast<uint32_t>((red) | (green << 1) | (blue << 2) | (alpha << 3)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void APIENTRY glDepthFunc(GLenum func)
{
	static const auto trampoline = reshade::hooks::call(glDepthFunc);
	trampoline(func);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_func };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glDepthMask(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(glDepthMask);
	trampoline(flag);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_write_mask };
		const uint32_t values[1] = { flag };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}

HOOK_EXPORT void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilFunc);
	trampoline(func, ref, mask);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[4] = { reshade::api::dynamic_state::front_stencil_func, reshade::api::dynamic_state::back_stencil_func, reshade::api::dynamic_state::stencil_reference_value, reshade::api::dynamic_state::stencil_read_mask };
		const uint32_t values[4] = { static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)), static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)), static_cast<uint32_t>(ref), mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 4, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glStencilMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilMask);
	trampoline(mask);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::stencil_write_mask };
		const uint32_t values[1] = { mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 1, states, values);
	}
#endif
}
HOOK_EXPORT void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = reshade::hooks::call(glStencilOp);
	trampoline(fail, zfail, zpass);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_fail_op, reshade::api::dynamic_state::back_stencil_fail_op, reshade::api::dynamic_state::front_stencil_depth_fail_op, reshade::api::dynamic_state::back_stencil_depth_fail_op, reshade::api::dynamic_state::front_stencil_pass_op, reshade::api::dynamic_state::back_stencil_pass_op };
		const uint32_t values[6] = { static_cast<uint32_t>(reshade::opengl::convert_stencil_op(fail)), static_cast<uint32_t>(reshade::opengl::convert_stencil_op(fail)), static_cast<uint32_t>(reshade::opengl::convert_stencil_op(zfail)), static_cast<uint32_t>(reshade::opengl::convert_stencil_op(zfail)), static_cast<uint32_t>(reshade::opengl::convert_stencil_op(zpass)), static_cast<uint32_t>(reshade::opengl::convert_stencil_op(zpass)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 6, states, values);
	}
#endif
}

HOOK_EXPORT void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissor);
	trampoline(x, y, width, height);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		const int32_t rect_data[4] = { x, y, x + width, y + height };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, 0, 1, rect_data);
	}
#endif
}

HOOK_EXPORT void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glViewport);
	trampoline(x, y, width, height);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
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
HOOK_EXPORT void APIENTRY glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = reshade::hooks::call(glDepthRange);
	trampoline(zNear, zFar);
}
#endif

#ifdef GL_VERSION_1_1
HOOK_EXPORT void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < n; ++i)
		if (textures[i] != 0)
			destroy_resource_or_view(GL_TEXTURE, textures[i]);
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteTextures);
	trampoline(n, textures);
}

HOOK_EXPORT void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, 0, 0, width, 1, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage1D);
	trampoline(target, level, xoffset, width, format, type, pixels);
}
HOOK_EXPORT void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, yoffset, 0, width, height, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

HOOK_EXPORT void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
	static const auto trampoline = reshade::hooks::call(glCopyTexImage1D);
	trampoline(target, level, internalformat, x, y, width, border);
}
HOOK_EXPORT void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	static const auto trampoline = reshade::hooks::call(glCopyTexImage2D);
	trampoline(target, level, internalFormat, x, y, width, height, border);
}

HOOK_EXPORT void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, 0, 0, width, 1, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage1D);
	trampoline(target, level, xoffset, x, y, width);
}
HOOK_EXPORT void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, yoffset, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}

HOOK_EXPORT void APIENTRY glBindTexture(GLenum target, GLuint texture)
{
#if RESHADE_ADDON
	// Only interested in existing textures that are were bound to the render pipeline
	const bool exists = glIsTexture(texture);
#endif

	static const auto trampoline = reshade::hooks::call(glBindTexture);
	trampoline(target, texture);

#if RESHADE_ADDON
	if (g_current_context && exists &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		GLint texunit = GL_TEXTURE0;
		gl3wGetIntegerv(GL_ACTIVE_TEXTURE, &texunit);
		texunit -= GL_TEXTURE0;

		// Could technically get current sampler via "GL_SAMPLER_BINDING", but that only works if sampler objects are used and bound before the texture, which is not necessarily the case
		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 0,
			reshade::api::descriptor_set_update(static_cast<GLuint>(texunit), 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data));
	}
#endif
}

HOOK_EXPORT void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = reshade::hooks::call(glPolygonOffset);
	trampoline(factor, units);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::depth_bias_slope_scaled, reshade::api::dynamic_state::depth_bias };
		const uint32_t values[2] = { *reinterpret_cast<const uint32_t *>(&factor), *reinterpret_cast<const uint32_t *>(&units) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_current_context, 2, states, values);
	}
#endif
}

HOOK_EXPORT void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, count, 1, first, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArrays);
	trampoline(mode, first, count);
}
HOOK_EXPORT void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, get_index_buffer_offset(indices), 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElements);
	trampoline(mode, count, type, indices);
}
#endif

#ifdef GL_VERSION_1_2
void APIENTRY glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	// Convert base internal formats to sized internal formats
	switch (internalformat)
	{
	case GL_RED:
		internalformat = GL_R8;
		break;
	case GL_ALPHA:
		internalformat = GL_ALPHA8;
		break;
	case GL_LUMINANCE:
		internalformat = GL_LUMINANCE8;
		break;
	case GL_LUMINANCE_ALPHA:
		internalformat = GL_LUMINANCE8_ALPHA8;
		break;
	case GL_INTENSITY:
		internalformat = GL_INTENSITY8;
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

	static const auto trampoline = reshade::hooks::call(glTexImage3D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		gl3wGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		std::vector<uint8_t> temp_data;

		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width, height, depth, swizzle_mask);
		auto initial_data = convert_mapped_subresource(format, type, pixels, &temp_data, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format, swizzle_mask);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			pixels = nullptr;

			gl3wTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
		}

		trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr, initial_data.data != pixels);
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void APIENTRY glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void APIENTRY glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, yoffset, zoffset, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, get_index_buffer_offset(indices), 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawRangeElements);
	trampoline(mode, start, end, count, type, indices);
}
#endif

#ifdef GL_VERSION_1_3
void APIENTRY glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data)
{
	static const auto trampoline = reshade::hooks::call(glCompressedTexImage1D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_1D);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width);
		auto initial_data = convert_mapped_subresource(internalformat, GL_UNSIGNED_BYTE, data, nullptr, width);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			data = nullptr;
		}

		trampoline(target, level, internalformat, width, border, imageSize, data);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr, initial_data.data != data);
	}
	else
#endif
		trampoline(target, level, internalformat, width, border, imageSize, data);
}
void APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
	static const auto trampoline = reshade::hooks::call(glCompressedTexImage2D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP);
	// Ignore all cube map faces except for the first
	const bool cube_map_face = (target > GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object && !cube_map_face)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width, height);
		auto initial_data = convert_mapped_subresource(internalformat, GL_UNSIGNED_BYTE, data, nullptr, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			data = nullptr;
		}

		trampoline(target, level, internalformat, width, height, border, imageSize, data);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr, initial_data.data != data);
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, border, imageSize, data);
}
void APIENTRY glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)
{
	static const auto trampoline = reshade::hooks::call(glCompressedTexImage3D);

#if RESHADE_ADDON
	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY);

	if (g_current_context &&
		// TODO: Add support for other mipmap levels
		level == 0 && !proxy_object)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 0, 1, static_cast<GLenum>(internalformat), width, height, depth);
		auto initial_data = convert_mapped_subresource(internalformat, GL_UNSIGNED_BYTE, data, nullptr, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;

			// Skip initial upload, data is uploaded after creation in 'init_resource' below
			data = nullptr;
		}

		trampoline(target, level, internalformat, width, height, depth, border, imageSize, data);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr, initial_data.data != data);
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void APIENTRY glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, 0, 0, width, 1, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage1D);
	trampoline(target, level, xoffset, width, format, imageSize, data);
}
void APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, yoffset, 0, width, height, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void APIENTRY glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(target, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}
#endif

#ifdef GL_VERSION_1_4
void APIENTRY glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawArrays(mode, first[i], count[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawArrays);
	trampoline(mode, first, count, drawcount);
#endif
}
void APIENTRY glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawElements(mode, count[i], type, indices[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawElements);
	trampoline(mode, count, type, indices, drawcount);
#endif
}
#endif

#ifdef GL_VERSION_1_5
void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < n; ++i)
		if (buffers[i] != 0)
			destroy_resource_or_view(GL_BUFFER, buffers[i]);
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteBuffers);
	trampoline(n, buffers);
}

void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glBufferData);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, size);
		reshade::opengl::convert_memory_heap_from_usage(desc, usage);
		auto initial_data = reshade::api::subresource_data { const_cast<void *>(data) }; // Row and depth pitch are unused for buffer data

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
			reshade::opengl::convert_memory_heap_to_usage(desc, usage);
			data = initial_data.data;
		}

		trampoline(target, size, data, usage);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr);
	}
	else
#endif
		trampoline(target, size, data, usage);
}

void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
#if RESHADE_ADDON
	if (update_buffer_region(target, 0, offset, size, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glBufferSubData);
	trampoline(target, offset, size, data);
}

auto APIENTRY glMapBuffer(GLenum target, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapBuffer);
	void *result = trampoline(target, access);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		GLint object = 0;
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, object), 0, std::numeric_limits<uint64_t>::max(), reshade::opengl::convert_access_flags(access), &result);
	}
#endif

	return result;
}
void APIENTRY glUnmapBuffer(GLenum target)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
	{
		GLint object = 0;
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, object));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glUnmapBuffer);
	trampoline(target);
}

void APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
#if RESHADE_ADDON
	// Only interested in existing buffers that are were bound to the render pipeline
	const bool exists = glIsBuffer(buffer);
#endif

	static const auto trampoline = reshade::hooks::call(glBindBuffer);
	trampoline(target, buffer);

#if RESHADE_ADDON
	if (g_current_context && exists && (
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>() ||
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>()))
	{
		const reshade::api::resource resource = reshade::opengl::make_resource_handle(GL_BUFFER, buffer);
		const uint64_t offset_64 = 0;
		const uint32_t stride_32 = 0;

		switch (target)
		{
		case GL_ELEMENT_ARRAY_BUFFER:
			g_current_context->_current_ibo = buffer;
			// The index format is provided to 'glDrawElements' and is unknown at this point, so call with index size set to zero
			reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(g_current_context, resource, offset_64, 0);
			return;
		case GL_ARRAY_BUFFER:
			reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_current_context, 0, 1, &resource, &offset_64, &stride_32);
			return;
		}
	}
#endif
}
#endif

#ifdef GL_VERSION_2_0
void APIENTRY glDeleteProgram(GLuint program)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint status = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);

		// Only invoke 'destroy_pipeline' event for programs that had a corresponding 'init_pipeline' event invoked in 'glLinkProgram'
		if (GL_FALSE != status)
		{
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(g_current_context, reshade::api::pipeline { program });
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteProgram);
	trampoline(program);
}

void APIENTRY glLinkProgram(GLuint program)
{
	static const auto trampoline = reshade::hooks::call(glLinkProgram);
	trampoline(program);

#if RESHADE_ADDON
	if (g_current_context)
	{
		// Only invoke 'init_pipeline' event for programs that were successfully compiled and linked
		GLint status = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);

		if (GL_FALSE != status)
		{
			std::vector<std::string> sources;

			GLuint shaders[16] = {};
			glGetAttachedShaders(program, 16, nullptr, shaders);
			for (int i = 0; i < 16 && shaders[i] != GL_NONE; ++i)
			{
				GLint size = 0;
				glGetShaderiv(shaders[i], GL_SHADER_SOURCE_LENGTH, &size);

				std::string &source = sources.emplace_back(size, '\0');
				glGetShaderSource(shaders[i], size, nullptr, source.data());
			}

			reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::all_shader_stages };
			for (int i = 0; i < 16 && shaders[i] != GL_NONE; ++i)
			{
				GLint type = GL_NONE;
				glGetShaderiv(shaders[i], GL_SHADER_TYPE, &type);

				reshade::api::shader_desc *shader_desc = nullptr;
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

				if (shader_desc != nullptr)
				{
					shader_desc->code = sources[i].c_str();
					shader_desc->code_size = sources[i].size();
					shader_desc->entry_point = "main";
				}
			}

			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(g_current_context, desc, 0, nullptr, reshade::api::pipeline { program });
		}
	}
#endif
}

void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
#if RESHADE_ADDON
	std::string combined_source;
	reshade::api::pipeline_desc desc = {};
	GLint combined_source_length = -1;

	if (g_current_context)
	{
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

		if (shader_desc != nullptr)
		{
			shader_desc->code = combined_source.c_str();
			shader_desc->code_size = combined_source.size();
			shader_desc->entry_point = "main";

			if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(g_current_context, desc, 0, nullptr))
			{
				assert(shader_desc->code_size <= static_cast<size_t>(std::numeric_limits<GLint>::max()));
				combined_source_length = static_cast<GLint>(shader_desc->code_size);

				count = 1;
				string = reinterpret_cast<const GLchar *const *>(&shader_desc->code);
				length = &combined_source_length;
			}
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glShaderSource);
	trampoline(shader, count, string, length);
}

void APIENTRY glUseProgram(GLuint program)
{
	static const auto trampoline = reshade::hooks::call(glUseProgram);
	trampoline(program);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(g_current_context, reshade::api::pipeline_stage::all_shader_stages, reshade::api::pipeline { program });
	}
#endif
}

void APIENTRY glUniform1f(GLint location, GLfloat v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1f);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2f);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3f);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4f);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLfloat v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform1i(GLint location, GLint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1i);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2i(GLint location, GLint v0, GLint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2i);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3i);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4i);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}

void APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 1 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 2 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 3 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4fv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 4, location, 4 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 1 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 2 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 3 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4iv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 4 * count, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
#endif

#ifdef GL_VERSION_3_0
auto APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapBufferRange);
	void *result = trampoline(target, offset, length, access);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		GLint object = 0;
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, object), offset, length, reshade::opengl::convert_access_flags(access), &result);
	}
#endif

	return result;
}

void APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		for (GLsizei i = 0; i < n; ++i)
			if (framebuffers[i] != 0)
				reshade::invoke_addon_event<reshade::addon_event::destroy_framebuffer>(g_current_context, reshade::opengl::make_framebuffer_handle(framebuffers[i]));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteFramebuffers);
	trampoline(n, framebuffers);
}

void APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		for (GLsizei i = 0; i < n; ++i)
			if (renderbuffers[i] != 0)
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(g_current_context, reshade::opengl::make_resource_handle(GL_RENDERBUFFER, renderbuffers[i]));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteRenderbuffers);
	trampoline(n, renderbuffers);
}

void APIENTRY glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture1D);
	trampoline(target, attachment, textarget, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}
void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture2D);
	trampoline(target, attachment, textarget, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}
void APIENTRY glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture3D);
	trampoline(target, attachment, textarget, texture, level, layer);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}
void APIENTRY glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTextureLayer);
	trampoline(target, attachment, texture, level, layer);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}
void APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferRenderbuffer);
	trampoline(target, attachment, renderbuffertarget, renderbuffer);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}

void APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 1, 1, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(target, internalformat, width, height);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, internalformat, width, height);
}
void APIENTRY glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorageMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height);

		if (g_current_context &&
			reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(target, samples, internalformat, width, height);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height);
}

void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_current_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || drawbuffer == 0);

		GLint fbo = 0;
		gl3wGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const auto type = reshade::opengl::convert_buffer_type_to_aspect(buffer);

		const reshade::api::resource_view view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(fbo), type, drawbuffer);

		if (view.handle != 0)
		{
			if (buffer != GL_COLOR ?
				reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, view, type, *value, static_cast<uint8_t>(0), 0, nullptr) :
				reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_current_context, view, value, 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferfv);
	trampoline(buffer, drawbuffer, value);
}
void APIENTRY glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		assert(buffer == GL_DEPTH_STENCIL && drawbuffer == 0);

		GLint fbo = 0;
		gl3wGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const reshade::api::resource_view depth_view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(fbo), reshade::api::attachment_type::depth, drawbuffer);
		const reshade::api::resource_view stencil_view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(fbo), reshade::api::attachment_type::stencil, drawbuffer);

		if (depth_view.handle != 0 && depth_view.handle == stencil_view.handle)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, depth_view, reshade::api::attachment_type::depth | reshade::api::attachment_type::stencil, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
		}
		else
		{
			if (depth_view.handle != 0 && reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, depth_view, reshade::api::attachment_type::depth, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
			if (stencil_view.handle != 0 &&	reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, stencil_view, reshade::api::attachment_type::stencil, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferfi);
	trampoline(buffer, drawbuffer, depth, stencil);
}

void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON
	if (g_current_context && (
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>()))
	{
		GLint src_fbo = 0;
		gl3wGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &src_fbo);
		GLint dst_fbo = 0;
		gl3wGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);

		constexpr GLbitfield flags[3] = { GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT };
		for (GLbitfield flag : flags)
		{
			if ((mask & flag) != flag)
				continue;

			const reshade::api::attachment_type type = reshade::opengl::convert_buffer_bits_to_aspect(flag);

			reshade::api::resource src = g_current_context->get_resource_from_view(
				g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(src_fbo), type, 0));
			reshade::api::resource dst = g_current_context->get_resource_from_view(
				g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(dst_fbo), type, 0));

			const int32_t src_box[6] = { srcX0, srcY0, 0, srcX1, srcY1, 1 };
			const int32_t dst_box[6] = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

			if (g_current_context->get_resource_desc(src).texture.samples <= 1)
			{
				const reshade::api::filter_mode filter_type = (filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear;

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, filter_type))
					mask ^= flag;
			}
			else if ((srcX1 - srcX0) == (dstX1 - dstX0) && (srcY1 - srcY0) == (dstY1 - dstY0))
			{
				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, reshade::api::format::unknown))
					mask ^= flag;
			}
		}

		if (mask == 0)
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glBlitFramebuffer);
	trampoline(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY glGenerateMipmap(GLenum target)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint object = 0;
		gl3wGetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_current_context, reshade::opengl::make_resource_view_handle(target, object)))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glGenerateMipmap);
	trampoline(target);
}

void APIENTRY glBindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glBindBufferBase);
	trampoline(target, index, buffer);

#if RESHADE_ADDON
	if (g_current_context && (
		target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const reshade::api::buffer_range descriptor_data = {
			reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
			0,
			std::numeric_limits<uint64_t>::max()
		};

		const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
		const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 3;

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, layout_param,
			reshade::api::descriptor_set_update(index, 1, type, &descriptor_data));
	}
#endif
}
void APIENTRY glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glBindBufferRange);
	trampoline(target, index, buffer, offset, size);

#if RESHADE_ADDON
	if (g_current_context && (
		target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const reshade::api::buffer_range descriptor_data = {
			reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
			static_cast<uint64_t>(offset),
			static_cast<uint64_t>(size)
		};

		const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
		const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 3;

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, layout_param,
			reshade::api::descriptor_set_update(index, 1, type, &descriptor_data));
	}
#endif
}

void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	static const auto trampoline = reshade::hooks::call(glBindFramebuffer);
	trampoline(target, framebuffer);

#if RESHADE_ADDON
	if (g_current_context && (
		target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) && (
		reshade::has_addon_event<reshade::addon_event::begin_render_pass>() || reshade::has_addon_event<reshade::addon_event::end_render_pass>()) &&
		// Only interested in existing framebuffers that are were bound to the render pipeline
		glCheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE)
	{
		if (framebuffer != g_current_context->_current_fbo)
		{
			reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(g_current_context);

			g_current_context->_current_fbo = framebuffer;

			reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(g_current_context, reshade::api::render_pass { 0 }, reshade::opengl::make_framebuffer_handle(framebuffer), 0, nullptr);
		}
	}
#endif
}

void APIENTRY glBindVertexArray(GLuint array)
{
#if RESHADE_ADDON
	// Only interested in existing vertex arrays that are were bound to the render pipeline
	const bool exists = glIsVertexArray(array);
#endif

	static const auto trampoline = reshade::hooks::call(glBindVertexArray);
	trampoline(array);

#if RESHADE_ADDON
	if (g_current_context && exists && (
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>() ||
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>()))
	{
		GLint count = 0, vbo = 0, ibo = 0;
		gl3wGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &count);

		const auto buffer_handles = static_cast<reshade::api::resource *>(_malloca(count * sizeof(reshade::api::resource)));
		const auto offsets_64 = static_cast<uint64_t *>(_malloca(count * sizeof(uint64_t)));
		const auto strides_32 = static_cast<uint32_t *>(_malloca(count * sizeof(uint32_t)));

		for (GLsizei i = 0; i < count; ++i)
		{
			offsets_64[i] = 0;
			strides_32[i] = 0;

			glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vbo);

			if (vbo != 0)
			{
				buffer_handles[i] = reshade::opengl::make_resource_handle(GL_BUFFER, vbo);

				glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, reinterpret_cast<GLint *>(&strides_32[i]));
				glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, reinterpret_cast<GLvoid **>(&offsets_64[i]));
			}
			else
			{
				buffer_handles[i] = { 0 };
			}
		}

		gl3wGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ibo);

		g_current_context->_current_ibo = ibo;
		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(
			g_current_context, ibo != 0 ? reshade::opengl::make_resource_handle(GL_BUFFER, ibo) : reshade::api::resource { 0 }, 0, 0);

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(
			g_current_context, 0, count, buffer_handles, offsets_64, strides_32);

		_freea(strides_32);
		_freea(offsets_64);
		_freea(buffer_handles);
	}
#endif
}

void APIENTRY glUniform1ui(GLint location, GLuint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1ui);
	trampoline(location, v0);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 1, v);
	}
#endif
}
void APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2ui);
	trampoline(location, v0, v1);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 2, v);
	}
#endif
}
void APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3ui);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 3, v);
	}
#endif
}
void APIENTRY glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4ui);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON
	if (g_current_context)
	{
		const GLuint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 4, v);
	}
#endif
}

void APIENTRY glUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform1uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 1 * count, v);
	}
#endif
}
void APIENTRY glUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform2uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 2 * count, v);
	}
#endif
}
void APIENTRY glUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform3uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 3 * count, v);
	}
#endif
}
void APIENTRY glUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
	static const auto trampoline = reshade::hooks::call(glUniform4uiv);
	trampoline(location, count, v);

#if RESHADE_ADDON
	if (g_current_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_current_context, reshade::api::shader_stage::all, reshade::opengl::global_pipeline_layout, 5, location, 4 * count, v);
	}
#endif
}
#endif

#ifdef GL_VERSION_3_1
void APIENTRY glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
#if RESHADE_ADDON
	GLint size = 0;
	glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);

	glTexBufferRange(target, internalformat, buffer, 0, size);
#else
	static const auto trampoline = reshade::hooks::call(glTexBuffer);
	trampoline(target, internalformat, buffer);
#endif
}

void APIENTRY glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON
	if (copy_buffer_region(readTarget, 0, readOffset, writeTarget, 0, writeOffset, size))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyBufferSubData);
	trampoline(readTarget, writeTarget, readOffset, writeOffset, size);
}

void APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, primcount, count, first, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstanced);
	trampoline(mode, first, count, primcount);
}
void APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, get_index_buffer_offset(indices), 0, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstanced);
	trampoline(mode, count, type, indices, primcount);
}
#endif

#ifdef GL_VERSION_3_2
void APIENTRY glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture);
	trampoline(target, attachment, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(target, 0);
#endif
}

void APIENTRY glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		gl3wGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		auto desc =	reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height, 1, swizzle_mask);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format, swizzle_mask);
			width = desc.texture.width;
			height = desc.texture.height;

			gl3wTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
		}

		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		gl3wGetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		auto desc =	reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height, depth, swizzle_mask);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format, swizzle_mask);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;

			gl3wTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
		}

		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, get_index_buffer_offset(indices), basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, basevertex);
}
void APIENTRY glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, count, 1, get_index_buffer_offset(indices), basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawRangeElementsBaseVertex);
	trampoline(mode, start, end, count, type, indices, basevertex);
}
void APIENTRY glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, get_index_buffer_offset(indices), basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseVertex);
	trampoline(mode, count, type, indices, primcount, basevertex);
}
void APIENTRY glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < drawcount; ++i)
		glDrawElementsBaseVertex(mode, count[i], type, indices[i], basevertex[i]);
#else
	static const auto trampoline = reshade::hooks::call(glMultiDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, drawcount, basevertex);
#endif
}
#endif

#ifdef GL_VERSION_4_0
void APIENTRY glDrawArraysIndirect(GLenum mode, const GLvoid *indirect)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint indirect_buffer_binding = 0;
		gl3wGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

		if (0 != indirect_buffer_binding)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, reshade::api::indirect_command::draw, reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), 1, 0))
				return;
		}
		else
		{
			// Redirect to non-indirect draw call so proper event is invoked
			const auto cmd = static_cast<const DrawArraysIndirectCommand *>(indirect);
			glDrawArraysInstancedBaseInstance(mode, cmd->first, cmd->count, cmd->primcount, cmd->baseinstance);
			return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysIndirect);
	trampoline(mode, indirect);
}
void APIENTRY glDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint indirect_buffer_binding = 0;
		gl3wGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode, type);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
				g_current_context, reshade::api::indirect_command::draw_indexed, reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), 1, 0))
				return;
		}
		else
		{
			// Redirect to non-indirect draw call so proper event is invoked
			const auto cmd = static_cast<const DrawElementsIndirectCommand *>(indirect);
			glDrawElementsInstancedBaseVertexBaseInstance(mode, cmd->count, type, reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(cmd->firstindex * reshade::opengl::get_index_type_size(type))), cmd->primcount, cmd->basevertex, cmd->baseinstance);
			return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsIndirect);
	trampoline(mode, type, indirect);
}
#endif

#ifdef GL_VERSION_4_1
void APIENTRY glScissorArrayv(GLuint first, GLsizei count, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glScissorArrayv);
	trampoline(first, count, v);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		const auto rect_data = static_cast<int32_t *>(_malloca(count * 4 * sizeof(int32_t)));

		for (GLsizei i = 0, k = 0; i < count; ++i, k += 4)
		{
			rect_data[k + 0] = v[k + 0];
			rect_data[k + 1] = v[k + 2];
			rect_data[k + 2] = v[k + 0] + v[k + 3];
			rect_data[k + 3] = v[k + 2] + v[k + 4];
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, first, count, rect_data);

		_freea(rect_data);
	}
#endif
}
void APIENTRY glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexed);
	trampoline(index, left, bottom, width, height);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		const int32_t rect_data[4] = { left, bottom, left + width, bottom + height };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, index, 1, rect_data);
	}
#endif
}
void APIENTRY glScissorIndexedv(GLuint index, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexedv);
	trampoline(index, v);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		const int32_t rect_data[4] = { v[0], v[1], v[0] + v[2], v[1] + v[3] };

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_current_context, index, 1, rect_data);
	}
#endif
}

void APIENTRY glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glViewportArrayv);
	trampoline(first, count, v);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		const auto viewport_data = static_cast<float *>(_malloca(count * 6 * sizeof(float)));

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

		_freea(viewport_data);
	}
#endif
}
void APIENTRY glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
	static const auto trampoline = reshade::hooks::call(glViewportIndexedf);
	trampoline(index, x, y, w, h);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		const float viewport_data[4] = { x, y, w, h };

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_current_context, 0, 1, viewport_data);
	}
#endif
}
void APIENTRY glViewportIndexedfv(GLuint index, const GLfloat *v)
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
#endif

#ifdef GL_VERSION_4_2
void APIENTRY glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage1D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc =	reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			levels = desc.texture.levels;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
		}

		trampoline(target, levels, internalformat, width);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, levels, internalformat, width);
}
void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			levels = desc.texture.levels;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(target, levels, internalformat, width, height);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, levels, internalformat, width, height);
}
void APIENTRY glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, levels, 1, internalformat, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			levels = desc.texture.levels;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;
		}

		trampoline(target, levels, internalformat, width, height, depth);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, levels, internalformat, width, height, depth);
}

void APIENTRY glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTexture);
	trampoline(unit, texture, level, layered, layer, access, format);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		GLint target = GL_TEXTURE;
		if (gl3wGetTextureParameteriv != nullptr)
			gl3wGetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		const reshade::api::resource_view descriptor_data = reshade::opengl::make_resource_view_handle(target, texture);

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 1,
			reshade::api::descriptor_set_update(unit, 1, reshade::api::descriptor_type::unordered_access_view, &descriptor_data));
	}
#endif
}

void APIENTRY glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{

#if RESHADE_ADDON
	if (g_current_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_current_context, primcount, count, first, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstancedBaseInstance);
	trampoline(mode, first, count, primcount, baseinstance);
}
void APIENTRY glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, get_index_buffer_offset(indices), 0, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseInstance);
	trampoline(mode, count, type, indices, primcount, baseinstance);
}
void APIENTRY glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		update_current_primitive_topology(mode, type);

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_current_context, primcount, count, get_index_buffer_offset(indices), basevertex, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseVertexBaseInstance);
	trampoline(mode, count, type, indices, primcount, basevertex, baseinstance);
}
#endif

#ifdef GL_VERSION_4_3
void APIENTRY glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
	static const auto trampoline = reshade::hooks::call(glTextureView);

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLenum orig_target = GL_TEXTURE;
		// 'glTextureView' is available since OpenGL 4.3, so no guarantee that 'glGetTextureParameteriv' exists, since it was introduced in OpenGL 4.5
		if (gl3wGetTextureParameteriv != nullptr)
			gl3wGetTextureParameteriv(origtexture, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&orig_target));

		auto desc = reshade::opengl::convert_resource_view_desc(target, internalformat, minlevel, numlevels, minlayer, numlayers);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(g_current_context, reshade::opengl::make_resource_handle(orig_target, origtexture), reshade::api::resource_usage::undefined, desc))
		{
			internalformat = reshade::opengl::convert_format(desc.format);
			minlevel = desc.texture.first_level;
			numlevels = desc.texture.level_count;
			minlayer = desc.texture.first_layer;
			numlayers = desc.texture.layer_count;

			if (desc.texture.level_count == 0xFFFFFFFF)
				numlevels = g_current_context->get_resource_desc(reshade::opengl::make_resource_handle(orig_target, origtexture)).texture.levels;
			if (desc.texture.layer_count == 0xFFFFFFFF)
				numlayers = g_current_context->get_resource_desc(reshade::opengl::make_resource_handle(orig_target, origtexture)).texture.depth_or_layers;
		}

		trampoline(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);

		init_resource_view(target, texture, desc, GL_TEXTURE, origtexture);
	}
	else
#endif
		trampoline(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void APIENTRY glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTexBufferRange);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_view_desc(target, internalformat, offset, size);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, desc))
		{
			internalformat = reshade::opengl::convert_format(desc.format);
			assert(desc.buffer.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			offset = static_cast<GLintptr>(desc.buffer.offset);
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
		}

		trampoline(target, internalformat, buffer, offset, size);

		init_resource_view(target, 0, desc, GL_BUFFER, buffer);
	}
	else
#endif
		trampoline(target, internalformat, buffer, offset, size);
}

void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, 1, samples, internalformat, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;
		}

		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);

		init_resource(target, 0, desc);
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
#if RESHADE_ADDON
	if (copy_texture_region(srcTarget, srcName, srcLevel, srcX, srcY, srcZ, dstTarget, dstName, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyImageSubData);
	trampoline(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
}

void APIENTRY glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexBuffer);
	trampoline(bindingindex, buffer, offset, stride);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		const reshade::api::resource buffer_handle = reshade::opengl::make_resource_handle(GL_BUFFER, buffer);
		uint64_t offset_64 = offset;
		uint32_t stride_32 = stride;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_current_context, bindingindex, 1, &buffer_handle, &offset_64, &stride_32);
	}
#endif
}

void APIENTRY glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::dispatch>(g_current_context, num_groups_x, num_groups_y, num_groups_z))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDispatchCompute);
	trampoline(num_groups_x, num_groups_y, num_groups_z);
}
void APIENTRY glDispatchComputeIndirect(GLintptr indirect)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint indirect_buffer_binding = 0;
		gl3wGetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

		if (0 != indirect_buffer_binding)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(g_current_context, reshade::api::indirect_command::dispatch, reshade::opengl::make_resource_handle(GL_DISPATCH_INDIRECT_BUFFER, indirect_buffer_binding), indirect, 1, 0))
				return;
		}
		else
		{
			assert(false);
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDispatchComputeIndirect);
	trampoline(indirect);
}

void APIENTRY glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{

#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint indirect_buffer_binding = 0;
		gl3wGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

		if (0 != indirect_buffer_binding)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
				g_current_context, reshade::api::indirect_command::draw, reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), drawcount, stride))
				return;
		}
		else
		{
			// Redirect to non-indirect draw calls so proper events are invoked
			for (GLsizei i = 0; i < drawcount; ++i)
			{
				const auto cmd = reinterpret_cast<const DrawArraysIndirectCommand *>(static_cast<const uint8_t *>(indirect) + i * (stride != 0 ? stride : sizeof(DrawArraysIndirectCommand)));
				glDrawArraysInstancedBaseInstance(mode, cmd->first, cmd->count, cmd->primcount, cmd->baseinstance);
			}
			return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawArraysIndirect);
	trampoline(mode, indirect, drawcount, stride);
}
void APIENTRY glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint indirect_buffer_binding = 0;
		gl3wGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);

		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode, type);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
				g_current_context, reshade::api::indirect_command::draw_indexed, reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding), reinterpret_cast<uintptr_t>(indirect), drawcount, stride))
				return;
		}
		else
		{
			// Redirect to non-indirect draw calls so proper events are invoked
			for (GLsizei i = 0; i < drawcount; ++i)
			{
				const auto cmd = reinterpret_cast<const DrawElementsIndirectCommand *>(static_cast<const uint8_t *>(indirect) + i * (stride != 0 ? stride : sizeof(DrawElementsIndirectCommand)));
				glDrawElementsInstancedBaseVertexBaseInstance(mode, cmd->count, type, reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(cmd->firstindex * reshade::opengl::get_index_type_size(type))), cmd->primcount, cmd->basevertex, cmd->baseinstance);
			}
			return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawElementsIndirect);
	trampoline(mode, type, indirect, drawcount, stride);
}
#endif

#ifdef GL_VERSION_4_4
void APIENTRY glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags)
{
	static const auto trampoline = reshade::hooks::call(glBufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(target, size);
		reshade::opengl::convert_memory_heap_from_flags(desc, flags);
		auto initial_data = reshade::api::subresource_data { const_cast<void *>(data) };

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
			reshade::opengl::convert_memory_heap_to_flags(desc, flags);
			data = initial_data.data;
		}

		trampoline(target, size, data, flags);

		init_resource(target, 0, desc, initial_data.data ? &initial_data : nullptr);
	}
	else
#endif
		trampoline(target, size, data, flags);
}

void APIENTRY glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersBase);
	trampoline(target, first, count, buffers);

#if RESHADE_ADDON
	if (g_current_context && (
		target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const auto descriptor_data = static_cast<reshade::api::buffer_range *>(_malloca(count * sizeof(reshade::api::buffer_range)));

		if (buffers != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
			{
				descriptor_data[i].buffer = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
				descriptor_data[i].offset = 0;
				descriptor_data[i].size = std::numeric_limits<uint64_t>::max();
			}
		}
		else
		{
			std::memset(descriptor_data, 0, count * sizeof(reshade::api::buffer_range));
		}

		const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
		const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 3;

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, layout_param,
			reshade::api::descriptor_set_update(first, static_cast<uint32_t>(count), type, descriptor_data));

		_freea(descriptor_data);
	}
#endif
}
void APIENTRY glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLintptr *sizes)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersRange);
	trampoline(target, first, count, buffers, offsets, sizes);

#if RESHADE_ADDON
	if (g_current_context && (
		target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const auto descriptor_data = static_cast<reshade::api::buffer_range *>(_malloca(count * sizeof(reshade::api::buffer_range)));

		if (buffers != nullptr)
		{
			assert(offsets != nullptr && sizes != nullptr);

			for (GLsizei i = 0; i < count; ++i)
			{
				descriptor_data[i].buffer = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
				descriptor_data[i].offset = static_cast<uint64_t>(offsets[i]);
				descriptor_data[i].size = static_cast<uint64_t>(sizes[i]);
			}
		}
		else
		{
			std::memset(descriptor_data, 0, count * sizeof(reshade::api::buffer_range));
		}

		const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
		const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 3;

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, layout_param,
			reshade::api::descriptor_set_update(first, static_cast<uint32_t>(count), type, descriptor_data));

		_freea(descriptor_data);
	}
#endif
}

void APIENTRY glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const auto descriptor_data = static_cast<reshade::api::sampler_with_resource_view *>(_malloca(count * sizeof(reshade::api::sampler_with_resource_view)));

		if (textures != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
			{
				GLint target = GL_TEXTURE;
				if (gl3wGetTextureParameteriv != nullptr)
					gl3wGetTextureParameteriv(textures[i], GL_TEXTURE_TARGET, &target);

				descriptor_data[i].view = reshade::opengl::make_resource_view_handle(target, textures[i]);
				descriptor_data[i].sampler = { 0 };
			}
		}
		else
		{
			std::memset(descriptor_data, 0, count * sizeof(reshade::api::sampler_with_resource_view));
		}

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 0,
			reshade::api::descriptor_set_update(first, static_cast<uint32_t>(count), reshade::api::descriptor_type::sampler_with_resource_view, descriptor_data));

		_freea(descriptor_data);
	}
#endif
}

void APIENTRY glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		const auto descriptor_data = static_cast<reshade::api::resource_view *>(_malloca(count * sizeof(reshade::api::resource_view)));

		if (textures != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
			{
				GLint target = GL_TEXTURE;
				if (gl3wGetTextureParameteriv != nullptr)
					gl3wGetTextureParameteriv(textures[i], GL_TEXTURE_TARGET, &target);

				descriptor_data[i] = reshade::opengl::make_resource_view_handle(target, textures[i]);
			}
		}
		else
		{
			std::memset(descriptor_data, 0, count * sizeof(reshade::api::resource_view));
		}

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 1,
			reshade::api::descriptor_set_update(first, static_cast<uint32_t>(count), reshade::api::descriptor_type::unordered_access_view, descriptor_data));

		_freea(descriptor_data);
	}
#endif
}

void APIENTRY glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexBuffers);
	trampoline(first, count, buffers, offsets, strides);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		const auto buffer_handles = static_cast<reshade::api::resource *>(_malloca(count * sizeof(reshade::api::resource)));
		const auto offsets_64 = static_cast<uint64_t *>(_malloca(count * sizeof(uint64_t)));

		if (buffers != nullptr)
		{
			assert(offsets != nullptr && strides != nullptr);

			for (GLsizei i = 0; i < count; ++i)
			{
				buffer_handles[i] = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
				offsets_64[i] = offsets[i];
			}
		}
		else
		{
			std::memset(buffer_handles, 0, count * sizeof(reshade::api::resource));
			std::memset(offsets_64, 0, count * sizeof(uint64_t));
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(
			g_current_context, first, count, buffer_handles, offsets_64, reinterpret_cast<const uint32_t *>(strides));

		_freea(offsets_64);
		_freea(buffer_handles);
	}
#endif
}
#endif

#ifdef GL_VERSION_4_5
void APIENTRY glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
#if RESHADE_ADDON
	GLint size = 0;
	glGetNamedBufferParameteriv(buffer, GL_BUFFER_SIZE, &size);

	glTextureBufferRange(texture, internalformat, buffer, 0, size);
#else
	static const auto trampoline = reshade::hooks::call(glTextureBuffer);
	trampoline(texture, internalformat, buffer);
#endif
}
void APIENTRY glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTextureBufferRange);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc =	reshade::opengl::convert_resource_view_desc(GL_TEXTURE_BUFFER, internalformat, offset, size);

		if (g_current_context &&
			reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), reshade::api::resource_usage::undefined, desc))
		{
			internalformat = reshade::opengl::convert_format(desc.format);
			assert(desc.buffer.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			offset = static_cast<GLintptr>(desc.buffer.offset);
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
		}

		trampoline(texture, internalformat, buffer, offset, size);

		init_resource_view(GL_TEXTURE_BUFFER, texture, desc, GL_BUFFER, buffer);
	}
	else
#endif
		trampoline(texture, internalformat, buffer, offset, size);
}

void APIENTRY glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferData);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_BUFFER, size);
		reshade::opengl::convert_memory_heap_from_usage(desc, usage);
		auto initial_data = reshade::api::subresource_data { const_cast<void *>(data) };

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
			reshade::opengl::convert_memory_heap_to_usage(desc, usage);
			data = initial_data.data;
		}

		trampoline(buffer, size, data, usage);

		init_resource(GL_BUFFER, buffer, desc, initial_data.data ? &initial_data : nullptr);
	}
	else
#endif
		trampoline(buffer, size, data, usage);
}

void APIENTRY glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_BUFFER, size);
		reshade::opengl::convert_memory_heap_from_flags(desc, flags);
		auto initial_data = reshade::api::subresource_data { const_cast<void *>(data) };

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, initial_data.data ? &initial_data : nullptr, reshade::api::resource_usage::general))
		{
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			size = static_cast<GLsizeiptr>(desc.buffer.size);
			reshade::opengl::convert_memory_heap_to_flags(desc, flags);
			data = initial_data.data;
		}

		trampoline(buffer, size, data, flags);

		init_resource(GL_BUFFER, buffer, desc, initial_data.data ? &initial_data : nullptr);
	}
	else
#endif
		trampoline(buffer, size, data, flags);
}

void APIENTRY glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage1D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_TEXTURE_1D, levels, 1, internalformat, width);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
		}

		trampoline(texture, levels, internalformat, width);

		init_resource(GL_TEXTURE, texture, desc);
	}
	else
#endif
		trampoline(texture, levels, internalformat, width);
}
void APIENTRY glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_TEXTURE_2D, levels, 1, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(texture, levels, internalformat, width, height);

		init_resource(GL_TEXTURE, texture, desc);
	}
	else
#endif
		trampoline(texture, levels, internalformat, width, height);
}
void APIENTRY glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_TEXTURE_2D, 1, samples, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(texture, samples, internalformat, width, height, fixedsamplelocations);

		init_resource(GL_TEXTURE, texture, desc);
	}
	else
#endif
		trampoline(texture, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3D);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_TEXTURE_3D, levels, 1, internalformat, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;
		}

		trampoline(texture, levels, internalformat, width, height, depth);

		init_resource(GL_TEXTURE, texture, desc);
	}
	else
#endif
		trampoline(texture, levels, internalformat, width, height, depth);
}
void APIENTRY glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3DMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_TEXTURE_3D, 1, samples, internalformat, width, height, depth);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
			depth = desc.texture.depth_or_layers;
		}

		trampoline(texture, samples, internalformat, width, height, depth, fixedsamplelocations);

		init_resource(GL_TEXTURE, texture, desc);
	}
	else
#endif
		trampoline(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
#if RESHADE_ADDON
	if (update_buffer_region(GL_BUFFER, buffer, offset, size, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glNamedBufferSubData);
	trampoline(buffer, offset, size, data);
}

void APIENTRY glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid* pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage1D);
	trampoline(texture, level, xoffset, width, format, type, pixels);
}
void APIENTRY glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}
void APIENTRY glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
void APIENTRY glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage1D);
	trampoline(texture, level, xoffset, width, format, imageSize, data);
}
void APIENTRY glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void APIENTRY glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void APIENTRY glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage1D);
	trampoline(texture, level, xoffset, x, y, width);
}
void APIENTRY glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, x, y, width, height);
}
void APIENTRY glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_TEXTURE, texture, level, xoffset, yoffset, zoffset, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

auto APIENTRY glMapNamedBuffer(GLuint buffer, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapNamedBuffer);
	void *result = trampoline(buffer, access);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), 0, std::numeric_limits<uint64_t>::max(), reshade::opengl::convert_access_flags(access), &result);
	}
#endif

	return result;
}
auto APIENTRY glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapNamedBufferRange);
	void *result = trampoline(buffer, offset, length, access);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer), offset, length, reshade::opengl::convert_access_flags(access), &result);
	}
#endif

	return result;
}
void APIENTRY glUnmapNamedBuffer(GLuint buffer)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(g_current_context, reshade::opengl::make_resource_handle(GL_BUFFER, buffer));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glUnmapNamedBuffer);
	trampoline(buffer);
}

void APIENTRY glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON
	if (copy_buffer_region(GL_BUFFER, readBuffer, readOffset, GL_BUFFER, writeBuffer, writeOffset, size))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyNamedBufferSubData);
	trampoline(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glNamedFramebufferTexture);
	trampoline(framebuffer, attachment, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(GL_FRAMEBUFFER, framebuffer);
#endif
}
void APIENTRY glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = reshade::hooks::call(glNamedFramebufferRenderbuffer);
	trampoline(framebuffer, attachment, renderbuffertarget, renderbuffer);

#if RESHADE_ADDON
	update_framebuffer_object(GL_FRAMEBUFFER, framebuffer);
#endif
}

void APIENTRY glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorage);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_RENDERBUFFER, 1, 1, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(renderbuffer, internalformat, width, height);

		init_resource(GL_RENDERBUFFER, renderbuffer, desc);
	}
	else
#endif
		trampoline(renderbuffer, internalformat, width, height);
}
void APIENTRY glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorageMultisample);

#if RESHADE_ADDON
	if (g_current_context)
	{
		auto desc = reshade::opengl::convert_resource_desc(GL_RENDERBUFFER, 1, samples, internalformat, width, height);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(g_current_context, desc, nullptr, reshade::api::resource_usage::general))
		{
			samples = desc.texture.samples;
			internalformat = reshade::opengl::convert_format(desc.texture.format);
			width = desc.texture.width;
			height = desc.texture.height;
		}

		trampoline(renderbuffer, samples, internalformat, width, height);

		init_resource(GL_RENDERBUFFER, renderbuffer, desc);
	}
	else
#endif
		trampoline(renderbuffer, samples, internalformat, width, height);
}

void APIENTRY glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_current_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || drawbuffer == 0);

		const auto type = reshade::opengl::convert_buffer_type_to_aspect(buffer);

		const reshade::api::resource_view view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(framebuffer), type, drawbuffer);

		if (view.handle != 0)
		{
			if (buffer != GL_COLOR ?
				reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, view, type, *value, static_cast<uint8_t>(0), 0, nullptr) :
				reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_current_context, view, value, 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferfv);
	trampoline(framebuffer, buffer, drawbuffer, value);
}
void APIENTRY glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		assert(buffer == GL_DEPTH_STENCIL && drawbuffer == 0);

		const reshade::api::resource_view depth_view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(framebuffer), reshade::api::attachment_type::depth, drawbuffer);
		const reshade::api::resource_view stencil_view = g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(framebuffer), reshade::api::attachment_type::stencil, drawbuffer);

		if (depth_view.handle != 0 && depth_view.handle == stencil_view.handle)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, depth_view, reshade::api::attachment_type::depth | reshade::api::attachment_type::stencil, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
		}
		else
		{
			if (depth_view.handle != 0 && reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, depth_view, reshade::api::attachment_type::depth, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
			if (stencil_view.handle != 0 && reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_current_context, stencil_view, reshade::api::attachment_type::stencil, depth, static_cast<uint8_t>(stencil & 0xFF), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferfi);
	trampoline(framebuffer, buffer, drawbuffer, depth, stencil);
}

void APIENTRY glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON
	if (g_current_context && (
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>()))
	{
		constexpr GLbitfield flags[3] = { GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT };
		for (GLbitfield flag : flags)
		{
			if ((mask & flag) != flag)
				continue;

			const reshade::api::attachment_type type = reshade::opengl::convert_buffer_bits_to_aspect(flag);

			reshade::api::resource src = g_current_context->get_resource_from_view(
				g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(readFramebuffer), type, 0));
			reshade::api::resource dst = g_current_context->get_resource_from_view(
				g_current_context->get_framebuffer_attachment(reshade::opengl::make_framebuffer_handle(drawFramebuffer), type, 0));

			const int32_t src_box[6] = { srcX0, srcY0, 0, srcX1, srcY1, 1 };
			const int32_t dst_box[6] = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

			if (g_current_context->get_resource_desc(src).texture.samples <= 1)
			{
				const reshade::api::filter_mode filter_type = (filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear;

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, filter_type))
					mask ^= flag;
			}
			else if ((srcX1 - srcX0) == (dstX1 - dstX0) && (srcY1 - srcY0) == (dstY1 - dstY0))
			{
				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_current_context, src, 0, src_box, dst, 0, dst_box, reshade::api::format::unknown))
					mask ^= flag;
			}
		}

		if (mask == 0)
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glBlitNamedFramebuffer);
	trampoline(readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void APIENTRY glGenerateTextureMipmap(GLuint texture)
{
#if RESHADE_ADDON
	if (g_current_context)
	{
		GLint target = GL_TEXTURE;
		gl3wGetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_current_context, reshade::opengl::make_resource_view_handle(target, texture)))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glGenerateTextureMipmap);
	trampoline(texture);
}

void APIENTRY glBindTextureUnit(GLuint unit, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindTextureUnit);
	trampoline(unit, texture);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		GLint target = GL_TEXTURE;
		gl3wGetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 0,
			reshade::api::descriptor_set_update(unit, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data));
	}
#endif
}
#endif

void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
	static const auto trampoline = reshade::hooks::call(glBindFramebufferEXT);
	trampoline(target, framebuffer);

#if RESHADE_ADDON
	if (g_current_context && (
		target == 0x8D40 /* GL_FRAMEBUFFER_EXT */ || target == 0x8CA9 /* GL_DRAW_FRAMEBUFFER_EXT */) && (
		reshade::has_addon_event<reshade::addon_event::begin_render_pass>() || reshade::has_addon_event<reshade::addon_event::end_render_pass>()) &&
		// Only interested in existing framebuffers that are were bound to the render pipeline
		glCheckFramebufferStatus(target) == 0x8CD5 /* GL_FRAMEBUFFER_COMPLETE_EXT */)
	{
		if (framebuffer != g_current_context->_current_fbo)
		{
			reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(g_current_context);

			g_current_context->_current_fbo = framebuffer;

			reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(g_current_context, reshade::api::render_pass { 0 }, reshade::opengl::make_framebuffer_handle(framebuffer), 0, nullptr);
		}
	}
#endif
}

void APIENTRY glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindMultiTextureEXT);
	trampoline(texunit, target, texture);

#if RESHADE_ADDON
	if (g_current_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		assert(texunit >= GL_TEXTURE0);
		texunit -= GL_TEXTURE0;

		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_current_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'device_impl::get_pipeline_layout_params'
			reshade::opengl::global_pipeline_layout, 0,
			reshade::api::descriptor_set_update(texunit, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data));
	}
#endif
}
