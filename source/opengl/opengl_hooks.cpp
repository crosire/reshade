/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_device_context.hpp"
#include "opengl_impl_type_convert.hpp"
#include "opengl_hooks.hpp" // Fix name clashes with gl3w
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include <cstring> // std::memset, std::strlen

#define gl gl3wProcs.gl

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
thread_local reshade::opengl::device_context_impl *g_opengl_context = nullptr;

#if RESHADE_ADDON
reshade::api::pipeline_layout get_opengl_pipeline_layout();

// Helper class invoking the 'create/init_resource' add-on events for OpenGL resources
class init_resource
{
public:
	init_resource(GLenum target, GLuint object, GLsizeiptr buffer_size, GLbitfield storage_flags) :
		_target(GL_BUFFER), _object(object)
	{
		// Get object from current binding in case it was not specified
		if (object == 0)
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&_object));

		_desc = reshade::opengl::convert_resource_desc(target, buffer_size, storage_flags);
	}
	init_resource(GLenum target, GLuint object, GLsizei levels, GLsizei samples, GLenum internal_format, GLsizei width, GLsizei height, GLsizei depth) :
		_target(target), _object(object)
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
		if (target != GL_RENDERBUFFER)
		{
			if (object != 0)
			{
				// Get actual texture target from the texture object
				gl.GetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&_target));

				gl.GetTextureParameteriv(object, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
			}
			else
			{
				assert(!(target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));

				gl.GetTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
			}
		}

		if (object == 0)
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&_object));

		_desc = reshade::opengl::convert_resource_desc(target, levels, samples, internal_format, width, height, depth, swizzle_mask);
	}
	explicit init_resource(const reshade::api::resource_desc &desc) :
		_target(GL_NONE), _object(0), _desc(desc)
	{
	}

	void invoke_create_event(GLsizeiptr *buffer_size, GLbitfield *storage_flags, const void *&data)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		_initial_data.data = const_cast<void *>(data); // Row and depth pitch are unused for buffer data

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device, _desc, data != nullptr ? &_initial_data : nullptr, reshade::api::resource_usage::general))
		{
			reshade::opengl::convert_resource_desc(_desc, *buffer_size, *storage_flags);
			data = _initial_data.data;
		}
	}
	void invoke_create_event(GLsizei *levels, GLsizei *samples, GLenum *internal_format, GLsizei *width, GLsizei *height, GLsizei *depth)
	{
		auto pixels = static_cast<const void *>(nullptr);
		invoke_create_event(levels, samples, internal_format, width, height, depth, GL_NONE, GL_NONE, pixels);
	}
	void invoke_create_event(GLsizei *levels, GLsizei *samples, GLenum *internal_format, GLsizei *width, GLsizei *height, GLsizei *depth, GLenum format, GLenum type, const void *&pixels)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::api::subresource_data *const data = convert_mapped_subresource(format, type, pixels, _desc.texture.width, _desc.texture.height, _desc.texture.depth_or_layers);

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(device, _desc, data, reshade::api::resource_usage::general))
		{
			if (levels != nullptr)
				*levels = _desc.texture.levels;
			else
				assert(_desc.texture.levels <= 1);
			if (samples != nullptr)
				*samples = _desc.texture.samples;
			else
				assert(_desc.texture.samples <= 1);

			*internal_format = reshade::opengl::convert_format(_desc.texture.format);

			if (width != nullptr)
				*width = _desc.texture.width;
			else
				assert(_desc.texture.width == 1);
			if (height != nullptr)
				*height = _desc.texture.height;
			else
				assert(_desc.texture.height == 1);
			if (depth != nullptr)
				*depth = _desc.texture.depth_or_layers;
			else
				assert(_desc.texture.depth_or_layers == 1);

			// Skip initial upload, data is uploaded after creation in 'invoke_initialize_event' below
			pixels = nullptr;
			_update_texture = (_initial_data.data != nullptr);
		}
	}

	void invoke_initialize_event()
	{
		assert(_object != 0);

		if (!reshade::has_addon_event<reshade::addon_event::init_resource>() && !reshade::has_addon_event<reshade::addon_event::init_resource_view>() && !_update_texture)
			return;

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		const reshade::api::resource resource = reshade::opengl::make_resource_handle(_target, _object);

		if (_update_texture)
		{
			assert(_target != GL_BUFFER);
			device->update_texture_region(_initial_data, resource, 0, nullptr);
		}

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			device, _desc, _initial_data.data != nullptr ? &_initial_data : nullptr, reshade::api::resource_usage::general, resource);

		if (_target == GL_BUFFER)
			return;

		const reshade::api::resource_usage usage_type = (_target == GL_RENDERBUFFER) ? reshade::api::resource_usage::render_target : reshade::api::resource_usage::undefined;

		// Register all possible views of this texture too
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			device, resource, usage_type, reshade::api::resource_view_desc(
				reshade::opengl::convert_resource_view_type(_target), _desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX), reshade::opengl::make_resource_view_handle(_target, _object));

		if (_target == GL_TEXTURE_CUBE_MAP)
		{
			for (GLuint face = 0; face < 6; ++face)
			{
				const GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

				reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
					device, resource, usage_type, reshade::api::resource_view_desc(
						reshade::opengl::convert_resource_view_type(face_target), _desc.texture.format, 0, UINT32_MAX, face, 1), reshade::opengl::make_resource_view_handle(face_target, _object));
			}
		}
	}

	reshade::api::subresource_data *convert_mapped_subresource(GLenum format, GLenum type, const void *pixels, GLuint width, GLuint height = 1, GLuint depth = 1)
	{
		if (pixels == nullptr)
			return nullptr; // Likely a 'GL_PIXEL_UNPACK_BUFFER' currently bound ...
		if (_target != GL_NONE && ((_desc.type != reshade::api::resource_type::texture_3d && _desc.texture.depth_or_layers != 1) || _desc.texture.levels > 1))
			return nullptr; // Currently only a single subresource data element is passed to 'create_resource' and 'init_resource' (see '_initial_data'), so cannot handle textures with multiple subresources

		GLint row_length = 0;
		gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
		GLint slice_height = 0;
		gl.GetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &slice_height);

		if (0 != row_length)
			width = row_length;
		if (0 != slice_height)
			height = slice_height;

		// Convert RGB to RGBA format (GL_RGB -> GL_RGBA, GL_BGR -> GL_BRGA, etc.)
		const bool convert_rgb_to_rgba = (format == GL_RGB || format == GL_RGB_INTEGER || format == GL_BGR || format == GL_BGR_INTEGER) && type == GL_UNSIGNED_BYTE;
		if (convert_rgb_to_rgba)
		{
			format += 1;

			_temp_data.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4);
			for (size_t z = 0; z < static_cast<size_t>(depth); ++z)
			{
				for (size_t y = 0; y < static_cast<size_t>(height); ++y)
				{
					for (size_t x = 0; x < static_cast<size_t>(width); ++x)
					{
						const size_t in_index = (z * width * height + y * width + x) * 3;
						const size_t out_index = (z * width * height + y * width + x) * 4;

						for (size_t c = 0; c < 3; ++c)
							_temp_data[out_index + c] = static_cast<const uint8_t *>(pixels)[in_index + c];
						_temp_data[out_index + 3] = 0xFF;
					}
				}
			}

			pixels = _temp_data.data();
		}

		// Convert BGRA to RGBA format (GL_BGRA -> GLRGBA)
		const bool convert_bgra_to_rgba = (format == GL_BGRA || format == GL_BGRA_INTEGER) && type == GL_UNSIGNED_BYTE;
		if (convert_bgra_to_rgba)
		{
			switch (format)
			{
			case GL_BGRA:
				format = GL_RGBA;
				break;
			case GL_BGRA_INTEGER:
				format = GL_RGBA_INTEGER;
				break;
			}

			_temp_data.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4);
			for (size_t z = 0; z < static_cast<size_t>(depth); ++z)
			{
				for (size_t y = 0; y < static_cast<size_t>(height); ++y)
				{
					for (size_t x = 0; x < static_cast<size_t>(width); ++x)
					{
						const size_t in_index = (z * width * height + y * width + x) * 4;
						const size_t out_index = (z * width * height + y * width + x) * 4;

						uint8_t b = static_cast<const uint8_t *>(pixels)[in_index + 0];
						uint8_t r = static_cast<const uint8_t *>(pixels)[in_index + 2];
						_temp_data[out_index + 0] = r;
						_temp_data[out_index + 1] = static_cast<const uint8_t *>(pixels)[in_index + 1];
						_temp_data[out_index + 2] = b;
						_temp_data[out_index + 3] = static_cast<const uint8_t *>(pixels)[in_index + 3];;
					}
				}
			}

			pixels = _temp_data.data();
		}

		const auto pixels_format = reshade::opengl::convert_upload_format(format, type);

		if (reshade::api::format_to_typeless(pixels_format) != reshade::api::format_to_typeless(_desc.texture.format) && !convert_rgb_to_rgba)
			return nullptr;

		_initial_data.row_pitch = reshade::api::format_row_pitch(pixels_format, width);
		_initial_data.slice_pitch = reshade::api::format_slice_pitch(pixels_format, _initial_data.row_pitch, height);

		GLint skip_rows = 0;
		gl.GetIntegerv(GL_UNPACK_SKIP_ROWS, &skip_rows);
		GLint skip_slices = 0;
		gl.GetIntegerv(GL_UNPACK_SKIP_IMAGES, &skip_slices);
		GLint skip_pixels = 0;
		gl.GetIntegerv(GL_UNPACK_SKIP_PIXELS, &skip_pixels);

		_initial_data.data =
			static_cast<uint8_t *>(const_cast<void *>(pixels)) +
			skip_rows   * static_cast<size_t>(_initial_data.row_pitch) +
			skip_slices * static_cast<size_t>(_initial_data.slice_pitch) +
			skip_pixels * static_cast<size_t>(_initial_data.row_pitch / width);

		return &_initial_data;
	}

	static bool is_texture_uninitialized(GLenum target)
	{
		GLint exists = 0;
		gl.GetTexLevelParameteriv(target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : target, 0, GL_TEXTURE_WIDTH, &exists);
		return 0 == exists;
	}

private:
	GLenum _target;
	GLuint _object;
	reshade::api::resource_desc _desc;
	std::vector<uint8_t> _temp_data;
	reshade::api::subresource_data _initial_data = {};
	bool _update_texture = false;
};

// Helper class invoking the 'create/init_resource_view' add-on events for OpenGL resource views
class init_resource_view
{
public:
	init_resource_view(GLenum target, GLuint object, GLuint orig_buffer, GLenum internal_format, GLintptr offset, GLsizeiptr size) :
		_target(target), _object(object)
	{
		// Get object from current binding in case it was not specified
		if (object == 0)
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&_object));

		_resource = reshade::opengl::make_resource_handle(GL_BUFFER, orig_buffer);
		_desc = reshade::opengl::convert_resource_view_desc(target, internal_format, offset, size);
	}
	init_resource_view(GLenum target, GLuint object, GLuint orig_texture, GLenum internal_format, GLuint min_level, GLuint num_levels, GLuint min_layer, GLuint num_layers) :
		_target(target), _object(object)
	{
		// Default parent target to the same target as the new texture view
		GLenum orig_texture_target = target;
		// 'glTextureView' is available since OpenGL 4.3, so no guarantee that 'glGetTextureParameteriv' exists, since it was introduced in OpenGL 4.5
		if (gl.GetTextureParameteriv != nullptr)
			gl.GetTextureParameteriv(orig_texture, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&orig_texture_target));

		if (object == 0)
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&_object));

		_resource = reshade::opengl::make_resource_handle(orig_texture_target, orig_texture);
		_desc = reshade::opengl::convert_resource_view_desc(target, internal_format, min_level, num_levels, min_layer, num_layers);
	}

	void invoke_create_event(GLenum *internal_format, GLintptr *offset, GLintptr *size)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device, _resource, reshade::api::resource_usage::undefined, _desc))
		{
			*internal_format = reshade::opengl::convert_format(_desc.format);
			assert(_desc.buffer.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			*offset = static_cast<GLintptr>(_desc.buffer.offset);
			assert(_desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			*size = static_cast<GLsizeiptr>(_desc.buffer.size);
		}
	}
	void invoke_create_event(GLenum *internal_format, GLuint *min_level, GLuint *num_levels, GLuint *min_layer, GLuint *num_layers)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(device, _resource, reshade::api::resource_usage::undefined, _desc))
		{
			*internal_format = reshade::opengl::convert_format(_desc.format);
			*min_level = _desc.texture.first_level;
			*min_layer = _desc.texture.first_layer;

			if (_desc.texture.level_count == UINT32_MAX)
				*num_levels = device->get_resource_desc(_resource).texture.levels;
			else
				*num_levels = _desc.texture.level_count;

			if (_desc.texture.layer_count == UINT32_MAX)
				*num_layers = device->get_resource_desc(_resource).texture.depth_or_layers;
			else
				*num_layers = _desc.texture.layer_count;
		}
	}

	void invoke_initialize_event()
	{
		assert(_object != 0);

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			device, _resource, reshade::api::resource_usage::undefined, _desc, reshade::opengl::make_resource_view_handle(_target, _object));
	}

private:
	GLenum _target;
	GLuint _object;
	reshade::api::resource _resource;
	reshade::api::resource_view_desc _desc;
};

static void destroy_resource_or_view(GLenum target, GLuint object)
{
	if (!g_opengl_context || (!reshade::has_addon_event<reshade::addon_event::destroy_resource>() && !reshade::has_addon_event<reshade::addon_event::destroy_resource_view>()))
		return;

	const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

	if (target != GL_BUFFER)
	{
		// Get actual texture target from the texture object
		if (target == GL_TEXTURE && gl.GetTextureParameteriv != nullptr)
			gl.GetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

		reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device, reshade::opengl::make_resource_view_handle(target, object));

		if (target == GL_TEXTURE_CUBE_MAP)
		{
			for (GLuint face = 0; face < 6; ++face)
			{
				const GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;

				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(device, reshade::opengl::make_resource_view_handle(face_target, object));
			}
		}
	}

	if (target != GL_TEXTURE_BUFFER)
	{
		reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(device, reshade::opengl::make_resource_handle(target, object));
	}
}

static void update_framebuffer_object(GLenum target, GLuint framebuffer = 0)
{
	if (!g_opengl_context || !(target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER))
		return;

	// Only interested in existing framebuffers that are being bound to the render pipeline
	if (gl.CheckFramebufferStatus(target) != GL_FRAMEBUFFER_COMPLETE)
		return;

	const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

	// Get object from current binding in case it was not specified (though it may still be zero)
	if (framebuffer == 0)
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&framebuffer));

	const reshade::api::resource_view default_attachment = device->get_framebuffer_attachment(framebuffer, GL_COLOR, 0);
	g_opengl_context->update_current_window_height(default_attachment);

	if (!reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
		return;

	uint32_t count = 0;
	reshade::api::resource_view rtvs[8], dsv;
	if (default_attachment != 0)
	{
		rtvs[0] = default_attachment;
		for (count = 1; count < 8; ++count)
		{
			rtvs[count] = device->get_framebuffer_attachment(framebuffer, GL_COLOR, count);
			if (rtvs[count] == 0)
				break;
		}
	}

	dsv = device->get_framebuffer_attachment(framebuffer, GL_DEPTH_STENCIL, 0);

	reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(g_opengl_context, count, rtvs, dsv);
}
#endif

#if RESHADE_ADDON >= 2
static bool copy_buffer_region(GLenum src_target, GLuint src_object, GLintptr src_offset, GLenum dst_target, GLuint dst_object, GLintptr dst_offset, GLsizeiptr size)
{
	if (!g_opengl_context || !reshade::has_addon_event<reshade::addon_event::copy_buffer_region>())
		return false;

	if (src_object == 0)
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(src_target), reinterpret_cast<GLint *>(&src_object));
	assert(src_object != 0);
	if (dst_object == 0)
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(dst_target), reinterpret_cast<GLint *>(&dst_object));
	assert(dst_object != 0);

	reshade::api::resource src = reshade::opengl::make_resource_handle(GL_BUFFER, src_object);
	reshade::api::resource dst = reshade::opengl::make_resource_handle(GL_BUFFER, dst_object);

	return reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(g_opengl_context, src, src_offset, dst, dst_offset, size);

}
static bool copy_texture_region(GLenum src_target, GLuint src_object, GLint src_level, GLint x, GLint y, GLint z, GLenum dst_target, GLuint dst_object, GLint dst_level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum filter = GL_NONE)
{
	if (!g_opengl_context || !reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
		return false;

	const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

	if (src_object == 0)
	{
		if (src_target == GL_FRAMEBUFFER_DEFAULT)
			gl.GetIntegerv(GL_READ_BUFFER, reinterpret_cast<GLint *>(&src_object));
		else
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(src_target), reinterpret_cast<GLint *>(&src_object));
		assert(src_object != 0);
	}
	else
	{
		if (src_target == GL_TEXTURE && gl.GetTextureParameteriv != nullptr)
			gl.GetTextureParameteriv(src_object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&src_target));
	}

	if (dst_object == 0)
	{
		if (src_target == GL_FRAMEBUFFER_DEFAULT)
			gl.GetIntegerv(GL_DRAW_BUFFER, reinterpret_cast<GLint *>(&dst_object));
		else
			gl.GetIntegerv(reshade::opengl::get_binding_for_target(dst_target), reinterpret_cast<GLint *>(&dst_object));
		assert(dst_object != 0);
	}
	else
	{
		if (dst_target == GL_TEXTURE && gl.GetTextureParameteriv != nullptr)
			gl.GetTextureParameteriv(dst_object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&dst_target));
	}

	reshade::api::resource src = reshade::opengl::make_resource_handle(src_target, src_object);
	// Check if this is actually referencing the default framebuffer, or really an attachment of a framebuffer object
	if (src_target == GL_FRAMEBUFFER_DEFAULT && (src_object >= GL_COLOR_ATTACHMENT0 && src_object <= GL_COLOR_ATTACHMENT31))
	{
		GLint src_fbo = 0;
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &src_fbo);

		src = device->get_resource_from_view(device->get_framebuffer_attachment(src_fbo, GL_COLOR, src_object - GL_COLOR_ATTACHMENT0));
	}

	reshade::api::resource dst = reshade::opengl::make_resource_handle(dst_target, dst_object);
	if (dst_target == GL_FRAMEBUFFER_DEFAULT && (dst_object >= GL_COLOR_ATTACHMENT0 && dst_object <= GL_COLOR_ATTACHMENT31))
	{
		GLint dst_fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);

		dst = device->get_resource_from_view(device->get_framebuffer_attachment(dst_fbo, GL_COLOR, dst_object - GL_COLOR_ATTACHMENT0));
	}

	const reshade::api::subresource_box src_box = { x, y, z, x + width, y + height, z + depth };
	const reshade::api::subresource_box dst_box = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + depth };

	return reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_opengl_context, src, src_level, &src_box, dst, dst_level, &dst_box,
		(filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear);
}
static bool update_buffer_region(GLenum target, GLuint object, GLintptr offset, GLsizeiptr size, const void *data)
{
	assert(data != nullptr);

	if (!g_opengl_context || !reshade::has_addon_event<reshade::addon_event::update_buffer_region>())
		return false;

	const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));
	assert(object != 0);

	reshade::api::resource dst = reshade::opengl::make_resource_handle(GL_BUFFER, object);

	return reshade::invoke_addon_event<reshade::addon_event::update_buffer_region>(device, data, dst, static_cast<uint64_t>(offset), static_cast<uint64_t>(size));
}
static bool update_texture_region(GLenum target, GLuint object, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
	if (!g_opengl_context || (!reshade::has_addon_event<reshade::addon_event::update_texture_region>() && !reshade::has_addon_event<reshade::addon_event::copy_buffer_to_texture>()))
		return false;

	const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

	// Get actual texture target from the texture object
	if (target == GL_TEXTURE && gl.GetTextureParameteriv != nullptr)
		gl.GetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

	// Get object from current binding in case it was not specified
	if (object == 0)
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), reinterpret_cast<GLint *>(&object));
	assert(object != 0);

	reshade::api::resource dst = reshade::opengl::make_resource_handle(target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z ? GL_TEXTURE_CUBE_MAP : target, object);

	const reshade::api::resource_desc desc = device->get_resource_desc(dst);

	GLuint subresource = level;
	if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		subresource += (target - GL_TEXTURE_CUBE_MAP_POSITIVE_X) * desc.texture.levels;

	const reshade::api::subresource_box dst_box = { xoffset, yoffset, zoffset, xoffset + width, yoffset + height, zoffset + depth };

	GLint unpack = 0;
	gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
	if (0 == unpack)
	{
		assert(pixels != nullptr);

		init_resource resource(desc);
		reshade::api::subresource_data *const data = resource.convert_mapped_subresource(format, type, pixels, width, height, depth);
		if (!data)
			return false;

		return reshade::invoke_addon_event<reshade::addon_event::update_texture_region>(device, *data, dst, subresource, &dst_box);
	}
	else
	{
		reshade::api::resource src = reshade::opengl::make_resource_handle(GL_BUFFER, unpack);

		GLint row_length = 0;
		gl.GetIntegerv(GL_UNPACK_ROW_LENGTH, &row_length);
		GLint slice_height = 0;
		gl.GetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &slice_height);

		return reshade::invoke_addon_event<reshade::addon_event::copy_buffer_to_texture>(g_opengl_context, src, reinterpret_cast<uintptr_t>(pixels), row_length, slice_height, dst, subresource, &dst_box);
	}
}

static void update_current_input_layout()
{
	if (g_opengl_context->_current_vao_dirty &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline>())
	{
		// Changing the vertex array binding also changes the vertex and index buffer bindings, so force an update of those next
		g_opengl_context->_current_vbo_dirty = true;
		g_opengl_context->_current_ibo_dirty = true;
		g_opengl_context->_current_vao_dirty = false;

		GLint count = 0;
		gl.GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count);

		std::vector<reshade::api::input_element> elements;
		elements.reserve(count);

		for (GLsizei i = 0; i < count; ++i)
		{
			GLint enabled = GL_FALSE;
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
			if (GL_FALSE == enabled)
				continue;

			reshade::api::input_element &element = elements.emplace_back();
			element.location = i;

			GLint attrib_size = 0;
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib_size);
			GLint attrib_type = 0;
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib_type);
			GLint attrib_normalized = 0;
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attrib_normalized);
			element.format = reshade::opengl::convert_attrib_format(attrib_size, static_cast<GLenum>(attrib_type), static_cast<GLboolean>(attrib_normalized));

			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, reinterpret_cast<GLint *>(&element.stride));
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, reinterpret_cast<GLint *>(&element.offset));
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_BINDING, reinterpret_cast<GLint *>(&element.buffer_binding));
			gl.GetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, reinterpret_cast<GLint *>(&element.instance_step_rate));
		}

		GLint vertex_array_binding = 0;
		gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array_binding);

		reshade::api::pipeline_subobject subobject;
		subobject.type = reshade::api::pipeline_subobject_type::input_layout;
		subobject.count = static_cast<uint32_t>(elements.size());
		subobject.data = elements.data();

		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(
			g_opengl_context->get_device(),
			get_opengl_pipeline_layout(),
			1,
			&subobject,
			reshade::api::pipeline { (static_cast<uint64_t>(GL_VERTEX_ARRAY) << 40) | vertex_array_binding });

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(
			g_opengl_context,
			reshade::api::pipeline_stage::input_assembler,
			reshade::api::pipeline { (static_cast<uint64_t>(GL_VERTEX_ARRAY) << 40) | vertex_array_binding });
	}
}
#endif
#if RESHADE_ADDON
static void update_current_primitive_topology(GLenum mode)
{
#if RESHADE_ADDON >= 2
	update_current_input_layout();

	if (g_opengl_context->_current_vbo_dirty &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		g_opengl_context->_current_vbo_dirty = false;

		GLint count = 0;
		gl.GetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &count);

		temp_mem<reshade::api::resource> buffer_handles(count);
		temp_mem<uint64_t> offsets_64(count);
		temp_mem<uint32_t> strides_32(count);
		for (GLsizei i = 0; i < count; ++i)
		{
			offsets_64[i] = 0;
			strides_32[i] = 0;

			GLint vertex_buffer_binding = 0;
			gl.GetIntegeri_v(GL_VERTEX_BINDING_BUFFER, i, &vertex_buffer_binding);
			if (0 != vertex_buffer_binding)
			{
				buffer_handles[i] = reshade::opengl::make_resource_handle(GL_BUFFER, vertex_buffer_binding);

				gl.GetIntegeri_v(GL_VERTEX_BINDING_STRIDE, i, reinterpret_cast<GLint *>(&strides_32[i]));
				assert(strides_32[i] != 0);
				gl.GetInteger64i_v(GL_VERTEX_BINDING_OFFSET, i, reinterpret_cast<GLint64 *>(&offsets_64[i]));
			}
			else
			{
				buffer_handles[i] = { 0 };
			}
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(
			g_opengl_context,
			0,
			count,
			buffer_handles.p,
			offsets_64.p,
			strides_32.p);
	}
#endif

	if (mode != g_opengl_context->_current_prim_mode)
	{
		g_opengl_context->_current_prim_mode = mode;

#if RESHADE_ADDON >= 2
		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		uint32_t value = static_cast<uint32_t>(reshade::opengl::convert_primitive_topology(mode));

		if (mode == GL_PATCHES)
		{
			GLint cps = 1;
			gl.GetIntegerv(GL_PATCH_VERTICES, &cps);

			assert(value == static_cast<uint32_t>(reshade::api::primitive_topology::patch_list_01_cp));
			value += cps - 1;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, 1, &state, &value);
#endif
	}
}
static void update_current_primitive_topology(GLenum mode, GLenum index_type)
{
	update_current_primitive_topology(mode);

	g_opengl_context->_current_index_type = index_type;

#if RESHADE_ADDON >= 2
	if (g_opengl_context->_current_ibo_dirty &&
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
	{
		g_opengl_context->_current_ibo_dirty = false;

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(
			g_opengl_context,
			reshade::opengl::make_resource_handle(GL_BUFFER, index_buffer_binding),
			0,
			reshade::opengl::get_index_type_size(index_type));
	}
#endif
}
#endif

#ifdef GL_VERSION_1_0
extern "C" void APIENTRY glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexImage1D);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_1D);

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		init_resource resource(target, 0, 0, 1, static_cast<GLenum>(internalformat), width, 1, 1);
		resource.invoke_create_event(nullptr, nullptr, reinterpret_cast<GLenum *>(&internalformat), &width, nullptr, nullptr, format, type, pixels);
		trampoline(target, level, internalformat, width, border, format, type, pixels);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, level, internalformat, width, border, format, type, pixels);
}
extern "C" void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	static const auto trampoline = reshade::hooks::call(glTexImage2D);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_2D || target == GL_PROXY_TEXTURE_1D_ARRAY || target == GL_PROXY_TEXTURE_RECTANGLE || target == GL_PROXY_TEXTURE_CUBE_MAP);

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		{
			if (init_resource::is_texture_uninitialized(GL_TEXTURE_CUBE_MAP_POSITIVE_X))
			{
				// Initialize resource, so that 'glGetTex(ture)LevelParameter' returns valid information
				for (GLint face = 0; face < 6; ++face)
					trampoline(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, internalformat, width, height, border, format, type, nullptr);

				// Assume only 1 mipmap level, so that the subresource index calculation in 'update_texture_region' below matches
				init_resource resource(GL_TEXTURE_CUBE_MAP, 0, 1, 1, static_cast<GLenum>(internalformat), width, height, 1);
				resource.invoke_initialize_event();
			}
#endif
#if RESHADE_ADDON >= 2
			if (!update_texture_region(target, 0, level, 0, 0, 0, width, height, 1, format, type, pixels))
#endif
#if RESHADE_ADDON
				trampoline(target, level, internalformat, width, height, border, format, type, pixels);
			return;
		}

		init_resource resource(target, 0, 0, 1, static_cast<GLenum>(internalformat), width, height, 1);
		resource.invoke_create_event(nullptr, nullptr, reinterpret_cast<GLenum *>(&internalformat), &width, &height, nullptr, format, type, pixels);
		trampoline(target, level, internalformat, width, height, border, format, type, pixels);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, border, format, type, pixels);
}

extern "C" void APIENTRY glCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_FRAMEBUFFER_DEFAULT, 0, 0, 0, 0, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyPixels);
	trampoline(x, y, width, height, type);
}

extern "C" void APIENTRY glClear(GLbitfield mask)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint dst_fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);

		if (const GLbitfield current_mask = mask & (GL_COLOR_BUFFER_BIT))
		{
			GLfloat color_value[4] = {};
			gl.GetFloatv(GL_COLOR_CLEAR_VALUE, color_value);

			const reshade::api::resource_view view = device->get_framebuffer_attachment(dst_fbo, current_mask, 0);

			if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, color_value, 0, nullptr))
				mask ^= current_mask;
		}
		if (const GLbitfield current_mask = mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
		{
			GLfloat depth_value = 0.0f;
			gl.GetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_value);
			GLint   stencil_value = 0;
			gl.GetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil_value);

			const reshade::api::resource_view view = device->get_framebuffer_attachment(dst_fbo, current_mask, 0);

			if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, mask & GL_DEPTH_BUFFER_BIT ? &depth_value : nullptr, mask & GL_STENCIL_BUFFER_BIT ? reinterpret_cast<const uint8_t *>(&stencil_value) : nullptr, 0, nullptr))
				mask ^= current_mask;
		}

		if (mask == 0)
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClear);
	trampoline(mask);
}

extern "C" void APIENTRY glEnable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glEnable);
	trampoline(cap);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		uint32_t value = GL_TRUE;
		reshade::api::dynamic_state state = { reshade::api::dynamic_state::unknown };
		switch (cap)
		{
		case 0x0BC0 /* GL_ALPHA_TEST */:
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
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, 1, &state, &value);
	}
#endif
}
extern "C" void APIENTRY glDisable(GLenum cap)
{
	static const auto trampoline = reshade::hooks::call(glDisable);
	trampoline(cap);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		uint32_t value = GL_FALSE;
		reshade::api::dynamic_state state = reshade::api::dynamic_state::unknown;
		switch (cap)
		{
		case 0x0BC0 /* GL_ALPHA_TEST */:
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
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, 1, &state, &value);
	}
#endif
}

extern "C" void APIENTRY glCullFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glCullFace);
	trampoline(mode);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::cull_mode };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_cull_mode(mode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glFrontFace(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glFrontFace);
	trampoline(mode);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::front_counter_clockwise };
		const uint32_t values[1] = { mode == GL_CCW };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glHint(GLenum target, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glHint);
	trampoline(target, mode);
}
extern "C" void APIENTRY glLineWidth(GLfloat width)
{
	static const auto trampoline = reshade::hooks::call(glLineWidth);
	trampoline(width);
}
extern "C" void APIENTRY glPointSize(GLfloat size)
{
	static const auto trampoline = reshade::hooks::call(glPointSize);
	trampoline(size);
}
extern "C" void APIENTRY glPolygonMode(GLenum face, GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glPolygonMode);
	trampoline(face, mode);

#if RESHADE_ADDON >= 2
	if (g_opengl_context && face == GL_FRONT_AND_BACK &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::fill_mode };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_fill_mode(mode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

extern "C" void APIENTRY glAlphaFunc(GLenum func, GLclampf ref)
{
	static const auto trampoline = reshade::hooks::call(glAlphaFunc);
	trampoline(func, ref);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::alpha_func, reshade::api::dynamic_state::alpha_reference_value };
		const uint32_t values[2] = { static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)), *reinterpret_cast<const uint32_t *>(&ref) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	static const auto trampoline = reshade::hooks::call(glBlendFunc);
	trampoline(sfactor, dfactor);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[4] = { reshade::api::dynamic_state::source_color_blend_factor, reshade::api::dynamic_state::dest_color_blend_factor, reshade::api::dynamic_state::source_alpha_blend_factor, reshade::api::dynamic_state::dest_alpha_blend_factor };
		const uint32_t values[4] = { static_cast<uint32_t>(reshade::opengl::convert_blend_factor(sfactor)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(dfactor)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(sfactor)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(dfactor)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glLogicOp(GLenum opcode)
{
	static const auto trampoline = reshade::hooks::call(glLogicOp);
	trampoline(opcode);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::logic_op };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_logic_op(opcode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	static const auto trampoline = reshade::hooks::call(glColorMask);
	trampoline(red, green, blue, alpha);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::render_target_write_mask };
		const uint32_t values[1] = { static_cast<uint32_t>((red) | (green << 1) | (blue << 2) | (alpha << 3)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

extern "C" void APIENTRY glDepthFunc(GLenum func)
{
	static const auto trampoline = reshade::hooks::call(glDepthFunc);
	trampoline(func);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_func };
		const uint32_t values[1] = { static_cast<uint32_t>(reshade::opengl::convert_compare_op(func)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glDepthMask(GLboolean flag)
{
	static const auto trampoline = reshade::hooks::call(glDepthMask);
	trampoline(flag);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::depth_write_mask };
		const uint32_t values[1] = { flag };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

extern "C" void APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilFunc);
	trampoline(func, ref, mask);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const auto stencil_func = reshade::opengl::convert_compare_op(func);

		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_func, reshade::api::dynamic_state::front_stencil_reference_value, reshade::api::dynamic_state::front_stencil_read_mask, reshade::api::dynamic_state::back_stencil_func, reshade::api::dynamic_state::back_stencil_reference_value, reshade::api::dynamic_state::back_stencil_read_mask };
		const uint32_t values[6] = { static_cast<uint32_t>(stencil_func), static_cast<uint32_t>(ref), mask, static_cast<uint32_t>(stencil_func), static_cast<uint32_t>(ref), mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = reshade::hooks::call(glStencilOp);
	trampoline(fail, zfail, zpass);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const auto stencil_fail_op = reshade::opengl::convert_stencil_op(fail);
		const auto stencil_depth_fail_op = reshade::opengl::convert_stencil_op(zfail);
		const auto stencil_pass_op = reshade::opengl::convert_stencil_op(zpass);

		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_fail_op, reshade::api::dynamic_state::front_stencil_depth_fail_op, reshade::api::dynamic_state::front_stencil_pass_op, reshade::api::dynamic_state::back_stencil_fail_op, reshade::api::dynamic_state::back_stencil_depth_fail_op, reshade::api::dynamic_state::back_stencil_pass_op };
		const uint32_t values[6] = { static_cast<uint32_t>(stencil_fail_op), static_cast<uint32_t>(stencil_depth_fail_op), static_cast<uint32_t>(stencil_pass_op), static_cast<uint32_t>(stencil_fail_op), static_cast<uint32_t>(stencil_depth_fail_op), static_cast<uint32_t>(stencil_pass_op) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
extern "C" void APIENTRY glStencilMask(GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilMask);
	trampoline(mask);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::front_stencil_write_mask, reshade::api::dynamic_state::back_stencil_write_mask };
		const uint32_t values[2] = { mask, mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

extern "C" void APIENTRY glScissor(GLint left, GLint bottom, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissor);
	trampoline(left, bottom, width, height);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		GLint clip_origin = GL_LOWER_LEFT;
		if (gl.ClipControl != nullptr)
			gl.GetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

		reshade::api::rect rect_data;
		rect_data.left = left;
		rect_data.right = left + width;

		if (clip_origin == GL_UPPER_LEFT)
		{
			rect_data.top = bottom;
			rect_data.bottom = bottom + height;
		}
		else
		{
			assert(g_opengl_context->_current_window_height != 0);
			rect_data.top = g_opengl_context->_current_window_height - (bottom + height);
			rect_data.bottom = g_opengl_context->_current_window_height - bottom;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_opengl_context, 0, 1, &rect_data);
	}
#endif
}

extern "C" void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glViewport);
	trampoline(x, y, width, height);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		const reshade::api::viewport viewport_data = {
			static_cast<float>(x),
			static_cast<float>(y),
			static_cast<float>(width),
			static_cast<float>(height)
		};

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_opengl_context, 0, 1, &viewport_data);
	}
#endif
}
extern "C" void APIENTRY glDepthRange(GLclampd zNear, GLclampd zFar)
{
	static const auto trampoline = reshade::hooks::call(glDepthRange);
	trampoline(zNear, zFar);
}
#endif

#ifdef GL_VERSION_1_1
extern "C" void APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < n; ++i)
		if (gl.IsTexture(textures[i]))
			destroy_resource_or_view(GL_TEXTURE, textures[i]);
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteTextures);
	trampoline(n, textures);
}

extern "C" void APIENTRY glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, 0, 0, width, 1, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage1D);
	trampoline(target, level, xoffset, width, format, type, pixels);
}
extern "C" void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, yoffset, 0, width, height, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

extern "C" void APIENTRY glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
	static const auto trampoline = reshade::hooks::call(glCopyTexImage1D);
	trampoline(target, level, internalformat, x, y, width, border);
}
extern "C" void APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	static const auto trampoline = reshade::hooks::call(glCopyTexImage2D);
	trampoline(target, level, internalFormat, x, y, width, height, border);
}

extern "C" void APIENTRY glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, 0, 0, width, 1, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage1D);
	trampoline(target, level, xoffset, x, y, width);
}
extern "C" void APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, yoffset, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, x, y, width, height);
}

extern "C" void APIENTRY glBindTexture(GLenum target, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindTexture);
	trampoline(target, texture);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		// Only interested in existing textures that are being bound to the render pipeline
		if (init_resource::is_texture_uninitialized(target))
			return;

		GLint texunit = GL_TEXTURE0;
		gl.GetIntegerv(GL_ACTIVE_TEXTURE, &texunit);
		texunit -= GL_TEXTURE0;

		// Could technically get current sampler via "GL_SAMPLER_BINDING", but that only works if sampler objects are used and bound before the texture, which is not necessarily the case
		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 0,
			reshade::api::descriptor_table_update { {}, static_cast<GLuint>(texunit), 0, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data });
	}
#endif
}

extern "C" void APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
	static const auto trampoline = reshade::hooks::call(glPolygonOffset);
	trampoline(factor, units);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::depth_bias_slope_scaled, reshade::api::dynamic_state::depth_bias };
		const uint32_t values[2] = { *reinterpret_cast<const uint32_t *>(&factor), *reinterpret_cast<const uint32_t *>(&units) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

extern "C" void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode);

		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_opengl_context, count, 1, first, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArrays);
	trampoline(mode, first, count);
}
extern "C" void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count, 1, offset, 0, 0))
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
	static const auto trampoline = reshade::hooks::call(glTexImage3D);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_3D || target == GL_PROXY_TEXTURE_2D_ARRAY || target == GL_PROXY_TEXTURE_CUBE_MAP_ARRAY);

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		init_resource resource(target, 0, 0, 1, static_cast<GLenum>(internalformat), width, height, depth);
		resource.invoke_create_event(nullptr, nullptr, reinterpret_cast<GLenum *>(&internalformat), &width, &height, &depth, format, type, pixels);
		trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void APIENTRY glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void APIENTRY glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, target, 0, level, xoffset, yoffset, zoffset, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void APIENTRY glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count, 1, offset, 0, 0))
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

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		init_resource resource(target, 0, 0, 1, internalformat, width, 1, 1);
		resource.invoke_create_event(nullptr, nullptr, &internalformat, &width, nullptr, nullptr, internalformat, GL_UNSIGNED_BYTE, data);
		trampoline(target, level, internalformat, width, border, imageSize, data);
		resource.invoke_initialize_event();
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

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		{
			if (init_resource::is_texture_uninitialized(GL_TEXTURE_CUBE_MAP_POSITIVE_X))
			{
				// Initialize resource, so that 'glGetTex(ture)LevelParameter' returns valid information
				for (GLint face = 0; face < 6; ++face)
					trampoline(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, internalformat, width, height, border, imageSize, nullptr);

				// Assume only 1 mipmap level, so that the subresource index calculation in 'update_texture_region' below matches
				init_resource resource(GL_TEXTURE_CUBE_MAP, 0, 1, 1, internalformat, width, height, 1);
				resource.invoke_initialize_event();
			}
#endif
#if RESHADE_ADDON >= 2
			if (!update_texture_region(target, 0, level, 0, 0, 0, width, height, 1, internalformat, GL_UNSIGNED_BYTE, data))
#endif
#if RESHADE_ADDON
				trampoline(target, level, internalformat, width, height, border, imageSize, data);
			return;
		}

		init_resource resource(target, 0, 0, 1, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, nullptr, &internalformat, &width, &height, nullptr, internalformat, GL_UNSIGNED_BYTE, data);
		trampoline(target, level, internalformat, width, height, border, imageSize, data);
		resource.invoke_initialize_event();
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

	if (g_opengl_context && level == 0 && !proxy_object)
	{
		init_resource resource(target, 0, 0, 1, internalformat, width, height, depth);
		resource.invoke_create_event(nullptr, nullptr, &internalformat, &width, &height, &depth, internalformat, GL_UNSIGNED_BYTE, data);
		trampoline(target, level, internalformat, width, height, depth, border, imageSize, data);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, level, internalformat, width, height, depth, border, imageSize, data);
}

void APIENTRY glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, 0, 0, width, 1, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage1D);
	trampoline(target, level, xoffset, width, format, imageSize, data);
}
void APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, yoffset, 0, width, height, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage2D);
	trampoline(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void APIENTRY glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(target, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTexSubImage3D);
	trampoline(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}
#endif

#ifdef GL_VERSION_1_4
void APIENTRY glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
	static const auto trampoline = reshade::hooks::call(glBlendFuncSeparate);
	trampoline(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[4] = { reshade::api::dynamic_state::source_color_blend_factor, reshade::api::dynamic_state::dest_color_blend_factor, reshade::api::dynamic_state::source_alpha_blend_factor, reshade::api::dynamic_state::dest_alpha_blend_factor };
		const uint32_t values[4] = { static_cast<uint32_t>(reshade::opengl::convert_blend_factor(sfactorRGB)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(dfactorRGB)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(sfactorAlpha)), static_cast<uint32_t>(reshade::opengl::convert_blend_factor(dfactorAlpha)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
void APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	static const auto trampoline = reshade::hooks::call(glBlendColor);
	trampoline(red, green, blue, alpha);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[1] = { reshade::api::dynamic_state::blend_constant };
		const uint32_t values[1] = {
			((static_cast<uint32_t>(red   * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(green * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(blue  * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(alpha * 255.f) & 0xFF) << 24) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}
void APIENTRY glBlendEquation(GLenum mode)
{
	static const auto trampoline = reshade::hooks::call(glBlendEquation);
	trampoline(mode);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::color_blend_op, reshade::api::dynamic_state::alpha_blend_op };
		const uint32_t values[2] = { static_cast<uint32_t>(reshade::opengl::convert_blend_op(mode)), static_cast<uint32_t>(reshade::opengl::convert_blend_op(mode)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

void APIENTRY glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode);

		for (GLsizei i = 0; i < drawcount; ++i)
			if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_opengl_context, count[i], 1, first[i], 0))
				return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawArrays);
	trampoline(mode, first, count, drawcount);
}
void APIENTRY glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		for (GLsizei i = 0; i < drawcount; ++i)
		{
			const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices[i]) / reshade::opengl::get_index_type_size(type)) : 0;

			if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count[i], 1, offset, 0, 0))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawElements);
	trampoline(mode, count, type, indices, drawcount);
}
#endif

#ifdef GL_VERSION_1_5
void APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < n; ++i)
		if (gl.IsBuffer(buffers[i]))
			destroy_resource_or_view(GL_BUFFER, buffers[i]);
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteBuffers);
	trampoline(n, buffers);
}

void APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glBufferData);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		GLbitfield storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT;

		init_resource resource(target, 0, size, storage_flags);
		resource.invoke_create_event(&size, &storage_flags, data);
		trampoline(target, size, data, usage);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, size, data, usage);
}

void APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
#if RESHADE_ADDON >= 2
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint object = 0;
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, object),
			0,
			UINT64_MAX,
			reshade::opengl::convert_access_flags(access),
			&result);
	}
#endif

	return result;
}
void APIENTRY glUnmapBuffer(GLenum target)
{
#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint object = 0;
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, object));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glUnmapBuffer);
	trampoline(target);
}

void APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffer);
	trampoline(target, buffer);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		switch (target)
		{
		case GL_ARRAY_BUFFER:
			g_opengl_context->_current_vbo_dirty = true;
			break;
		case GL_ELEMENT_ARRAY_BUFFER:
			g_opengl_context->_current_ibo_dirty = true;
			break;
		}
	}
#endif
}
#endif

#ifdef GL_VERSION_2_0
void APIENTRY glDeleteProgram(GLuint program)
{
#if RESHADE_ADDON
	if (g_opengl_context && program != 0)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint status = GL_FALSE;
		gl.GetProgramiv(program, GL_LINK_STATUS, &status);

		// Only invoke 'destroy_pipeline' event for programs that had a corresponding 'init_pipeline' event invoked in 'glLinkProgram'
		if (GL_FALSE != status)
		{
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device, reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | program });
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
	if (g_opengl_context && program != 0)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		// Only invoke 'init_pipeline' event for programs that were successfully compiled and linked
		GLint status = GL_FALSE;
		gl.GetProgramiv(program, GL_LINK_STATUS, &status);

		if (GL_FALSE != status)
		{
			std::vector<std::pair<reshade::api::shader_desc, std::vector<char>>> sources;
			sources.reserve(16);
			std::vector<reshade::api::pipeline_subobject> subobjects;
			subobjects.reserve(16);

			GLuint shaders[16] = {};
			gl.GetAttachedShaders(program, 16, nullptr, shaders);

			for (const GLuint shader : shaders)
			{
				if (shader == 0)
					break;

				GLint type = GL_NONE;
				gl.GetShaderiv(shader, GL_SHADER_TYPE, &type);

				reshade::api::pipeline_subobject_type subobject_type = reshade::api::pipeline_subobject_type::unknown;
				switch (type)
				{
				case GL_VERTEX_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::vertex_shader;
					break;
				case GL_TESS_CONTROL_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::hull_shader;
					break;
				case GL_TESS_EVALUATION_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::domain_shader;
					break;
				case GL_GEOMETRY_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::geometry_shader;
					break;
				case GL_FRAGMENT_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::pixel_shader;
					break;
				case GL_COMPUTE_SHADER:
					subobject_type = reshade::api::pipeline_subobject_type::compute_shader;
					break;
				default:
					assert(false);
					continue;
				}

				GLint size = 0;
				gl.GetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &size);

				std::pair<reshade::api::shader_desc, std::vector<char>> &source = sources.emplace_back();
				source.second.resize(size);

				gl.GetShaderSource(shader, size, nullptr, source.second.data());

				reshade::api::shader_desc &desc = source.first;
				desc.code = source.second.data();
				desc.code_size = size;
				desc.entry_point = "main";

				subobjects.push_back({ subobject_type, 1, &desc });
			}

			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device, get_opengl_pipeline_layout(), static_cast<uint32_t>(subobjects.size()), subobjects.data(), reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | program });
		}
	}
#endif
}

void APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
#if RESHADE_ADDON
	std::string combined_source;
	const GLchar *combined_source_ptr = nullptr;
	GLint combined_source_length = -1;

	if (g_opengl_context)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		if (count == 1)
		{
			combined_source_ptr = *string;
			combined_source_length = (length != nullptr) ? *length : static_cast<GLint>(std::strlen(combined_source_ptr));
		}
		else if (length != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
				combined_source.append(string[i], length[i]);

			combined_source_ptr = combined_source.data();
			combined_source_length = static_cast<GLint>(combined_source.size());
		}
		else
		{
			for (GLsizei i = 0; i < count; ++i)
				combined_source.append(string[i]);

			combined_source_ptr = combined_source.data();
			combined_source_length = static_cast<GLint>(combined_source.size());
		}

		GLint type = GL_NONE;
		gl.GetShaderiv(shader, GL_SHADER_TYPE, &type);

		reshade::api::pipeline_subobject_type subobject_type;
		switch (type)
		{
		case GL_VERTEX_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::vertex_shader;
			break;
		case GL_TESS_CONTROL_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::hull_shader;
			break;
		case GL_TESS_EVALUATION_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::domain_shader;
			break;
		case GL_GEOMETRY_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::geometry_shader;
			break;
		case GL_FRAGMENT_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::pixel_shader;
			break;
		case GL_COMPUTE_SHADER:
			subobject_type = reshade::api::pipeline_subobject_type::compute_shader;
			break;
		default:
			assert(false);
			subobject_type = reshade::api::pipeline_subobject_type::unknown;
			break;
		}

		reshade::api::shader_desc desc = {};
		desc.code = combined_source_ptr;
		desc.code_size = combined_source_length;
		desc.entry_point = "main";

		const reshade::api::pipeline_subobject subobject = { subobject_type, 1, &desc };

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device, get_opengl_pipeline_layout(), 1, &subobject))
		{
			assert(desc.code_size <= static_cast<size_t>(std::numeric_limits<GLint>::max()));
			combined_source_ptr = static_cast<const GLchar *>(desc.code);
			combined_source_length = static_cast<GLint>(desc.code_size);

			count = 1;
			string = &combined_source_ptr;
			length = &combined_source_length;
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		const auto pipeline = (program != 0) ? reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | program } : reshade::api::pipeline {};

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(g_opengl_context, reshade::api::pipeline_stage::all_shader_stages, pipeline);
	}
#endif
}

void APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
	static const auto trampoline = reshade::hooks::call(glBlendEquationSeparate);
	trampoline(modeRGB, modeAlpha);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::color_blend_op, reshade::api::dynamic_state::alpha_blend_op };
		const uint32_t values[2] = { static_cast<uint32_t>(reshade::opengl::convert_blend_op(modeRGB)), static_cast<uint32_t>(reshade::opengl::convert_blend_op(modeAlpha)) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, static_cast<uint32_t>(std::size(states)), states, values);
	}
#endif
}

void APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilFuncSeparate);
	trampoline(face, func, ref, mask);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const auto stencil_func = reshade::opengl::convert_compare_op(func);

		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_func, reshade::api::dynamic_state::front_stencil_reference_value, reshade::api::dynamic_state::front_stencil_read_mask, reshade::api::dynamic_state::back_stencil_func, reshade::api::dynamic_state::back_stencil_reference_value, reshade::api::dynamic_state::back_stencil_read_mask };
		const uint32_t values[6] = { static_cast<uint32_t>(stencil_func), static_cast<uint32_t>(ref), mask, static_cast<uint32_t>(stencil_func), static_cast<uint32_t>(ref), mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, face == GL_FRONT_AND_BACK ? 6 : 3, &states[face == GL_BACK ? 3 : 0], values);
	}
#endif
}
void APIENTRY glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
	static const auto trampoline = reshade::hooks::call(glStencilOpSeparate);
	trampoline(face, fail, zfail, zpass);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const auto stencil_fail_op = reshade::opengl::convert_stencil_op(fail);
		const auto stencil_depth_fail_op = reshade::opengl::convert_stencil_op(zfail);
		const auto stencil_pass_op = reshade::opengl::convert_stencil_op(zpass);

		const reshade::api::dynamic_state states[6] = { reshade::api::dynamic_state::front_stencil_fail_op, reshade::api::dynamic_state::front_stencil_depth_fail_op, reshade::api::dynamic_state::front_stencil_pass_op, reshade::api::dynamic_state::back_stencil_fail_op, reshade::api::dynamic_state::back_stencil_depth_fail_op, reshade::api::dynamic_state::back_stencil_pass_op };
		const uint32_t values[6] = { static_cast<uint32_t>(stencil_fail_op), static_cast<uint32_t>(stencil_depth_fail_op), static_cast<uint32_t>(stencil_pass_op), static_cast<uint32_t>(stencil_fail_op), static_cast<uint32_t>(stencil_depth_fail_op), static_cast<uint32_t>(stencil_pass_op) };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, face == GL_FRONT_AND_BACK ? 6 : 3, &states[face == GL_BACK ? 3 : 0], values);
	}
#endif
}
void APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask)
{
	static const auto trampoline = reshade::hooks::call(glStencilMaskSeparate);
	trampoline(face, mask);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::front_stencil_write_mask, reshade::api::dynamic_state::back_stencil_write_mask };
		const uint32_t values[2] = { mask, mask };

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(g_opengl_context, face == GL_FRONT_AND_BACK ? 2 : 1, &states[face == GL_BACK ? 1 : 0], values);
	}
#endif
}

void APIENTRY glUniform1f(GLint location, GLfloat v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1f);
	trampoline(location, v0);

#if RESHADE_ADDON >= 2
	// If location is equal to -1, the data passed in will be silently ignored
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLfloat v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2f);
	trampoline(location, v0, v1);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLfloat v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3f);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLfloat v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4f);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLfloat v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform1i(GLint location, GLint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1i);
	trampoline(location, v0);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, 1, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform2i(GLint location, GLint v0, GLint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2i);
	trampoline(location, v0, v1);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, 2, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3i);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, 3, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}
void APIENTRY glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4i);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, 4, reinterpret_cast<const uint32_t *>(v));
	}
#endif
}

void APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform1fv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 1, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform2fv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 2, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform3fv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 3, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform4fv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 4, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform1iv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, count * 1, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform2iv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, count * 2, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform3iv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, count * 3, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform4iv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 5, location * 4, count * 4, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix2fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 2 * 2, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix3fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 3 * 3, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix4fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 4 * 4, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}

void APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
	static const auto trampoline = reshade::hooks::call(glVertexAttribPointer);
	trampoline(index, size, type, normalized, stride, pointer);

#if RESHADE_ADDON >= 2
	if (g_opengl_context != nullptr)
		g_opengl_context->_current_vao_dirty = true;
#endif
}
#endif

#ifdef GL_VERSION_2_1
void APIENTRY glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix2x3fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 2 * 3, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix3x2fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 3 * 2, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix2x4fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 2 * 4, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix4x2fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 4 * 2, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix3x4fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 3 * 4, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
void APIENTRY glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	static const auto trampoline = reshade::hooks::call(glUniformMatrix3x4fv);
	trampoline(location, count, transpose, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 4, location * 4, count * 4 * 3, reinterpret_cast<const uint32_t *>(value));
	}
#endif
}
#endif

#ifdef GL_VERSION_3_0
void APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
#if RESHADE_ADDON
	for (GLsizei i = 0; i < n; ++i)
		if (gl.IsRenderbuffer(renderbuffers[i]))
			destroy_resource_or_view(GL_RENDERBUFFER, renderbuffers[i]);
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteRenderbuffers);
	trampoline(n, renderbuffers);
}

void APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
#if RESHADE_ADDON
	if (g_opengl_context != nullptr)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		for (GLsizei i = 0; i < n; ++i)
			if (gl.IsVertexArray(arrays[i]))
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device, reshade::api::pipeline { (static_cast<uint64_t>(GL_VERTEX_ARRAY) << 40) | arrays[i] });
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteVertexArrays);
	trampoline(n, arrays);
}

void APIENTRY glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture1D);
	trampoline(target, attachment, textarget, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(target);
#endif
}
void APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture2D);
	trampoline(target, attachment, textarget, texture, level);

#if RESHADE_ADDON
	update_framebuffer_object(target);
#endif
}
void APIENTRY glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTexture3D);
	trampoline(target, attachment, textarget, texture, level, layer);

#if RESHADE_ADDON
	update_framebuffer_object(target);
#endif
}
void APIENTRY glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferTextureLayer);
	trampoline(target, attachment, texture, level, layer);

#if RESHADE_ADDON
	update_framebuffer_object(target);
#endif
}
void APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	static const auto trampoline = reshade::hooks::call(glFramebufferRenderbuffer);
	trampoline(target, attachment, renderbuffertarget, renderbuffer);

#if RESHADE_ADDON
	update_framebuffer_object(target);
#endif
}

void APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorage);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	if (g_opengl_context)
	{
		init_resource resource(target, 0, 1, 1, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, nullptr, &internalformat, &width, &height, nullptr);
		trampoline(target, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, internalformat, width, height);
}
void APIENTRY glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glRenderbufferStorageMultisample);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	if (g_opengl_context)
	{
		init_resource resource(target, 0, 1, samples, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, nullptr);
		trampoline(target, samples, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height);
}

auto APIENTRY glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapBufferRange);
	void *result = trampoline(target, offset, length, access);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint object = 0;
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, object),
			offset,
			length,
			reshade::opengl::convert_access_flags(access),
			&result);
	}
#endif

	return result;
}

void APIENTRY glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_STENCIL && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const reshade::api::resource_view view = device->get_framebuffer_attachment(fbo, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, nullptr, reinterpret_cast<const uint8_t *>(value), 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, reinterpret_cast<const float *>(value), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferiv);
	trampoline(buffer, drawbuffer, value);
}
void APIENTRY glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_STENCIL && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const reshade::api::resource_view view = device->get_framebuffer_attachment(fbo, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, nullptr, reinterpret_cast<const uint8_t *>(value), 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, reinterpret_cast<const float *>(value), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferuiv);
	trampoline(buffer, drawbuffer, value);
}
void APIENTRY glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_DEPTH && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const reshade::api::resource_view view = device->get_framebuffer_attachment(fbo, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, value, nullptr, 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, value, 0, nullptr))
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
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		assert(buffer == GL_DEPTH_STENCIL && drawbuffer == 0);

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

		const reshade::api::resource_view dsv = device->get_framebuffer_attachment(fbo, buffer, drawbuffer);
		if (dsv != 0 &&
			reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, dsv, &depth, reinterpret_cast<const uint8_t *>(&stencil), 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearBufferfi);
	trampoline(buffer, drawbuffer, depth, stencil);
}

void APIENTRY glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON >= 2
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>()))
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		GLint src_fbo = 0;
		gl.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &src_fbo);
		GLint dst_fbo = 0;
		gl.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dst_fbo);

		constexpr GLbitfield flags[3] = { GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT };
		for (GLbitfield flag : flags)
		{
			if ((mask & flag) != flag)
				continue;

			reshade::api::resource src = device->get_resource_from_view(device->get_framebuffer_attachment(src_fbo, flag, 0));
			reshade::api::resource dst = device->get_resource_from_view(device->get_framebuffer_attachment(dst_fbo, flag, 0));

			if (device->get_resource_desc(src).texture.samples <= 1)
			{
				const reshade::api::subresource_box src_box = { srcX0, srcY0, 0, srcX1, srcY1, 1 }, dst_box = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_opengl_context, src, 0, &src_box, dst, 0, &dst_box, (filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
					mask ^= flag;
			}
			else
			{
				assert((srcX1 - srcX0) == (dstX1 - dstX0) && (srcY1 - srcY0) == (dstY1 - dstY0));

				const reshade::api::subresource_box src_box = { srcX0, srcY0, 0, srcX1, srcY1, 1 };

				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_opengl_context, src, 0, &src_box, dst, 0, dstX0, dstY0, 0, reshade::api::format::unknown))
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
#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		GLint object = 0;
		gl.GetIntegerv(reshade::opengl::get_binding_for_target(target), &object);

		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_opengl_context, reshade::opengl::make_resource_view_handle(target, object)))
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		if ((target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) && reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		{
			const reshade::api::buffer_range descriptor_data = {
				reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
				0,
				UINT64_MAX
			};

			const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
			const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 1;

			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
				g_opengl_context,
				reshade::api::shader_stage::all,
				// See global pipeline layout specified in 'wgl_device::wgl_device'
				get_opengl_pipeline_layout(), layout_param,
				reshade::api::descriptor_table_update { {}, index, 0, 1, type, &descriptor_data });
		}
		else if ((target == GL_TRANSFORM_FEEDBACK_BUFFER) && reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		{
			const reshade::api::resource buffer_handle = reshade::opengl::make_resource_handle(GL_BUFFER, buffer);
			uint64_t offset_64 = 0;
			uint64_t max_size_64 = UINT64_MAX;

			reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(g_opengl_context, index, 1, &buffer_handle, &offset_64, &max_size_64, nullptr, nullptr);
		}
	}
#endif
}
void APIENTRY glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glBindBufferRange);
	trampoline(target, index, buffer, offset, size);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		if ((target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) && reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		{
			const reshade::api::buffer_range descriptor_data = {
				reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
				static_cast<uint64_t>(offset),
				static_cast<uint64_t>(size)
			};

			const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
			const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 1;

			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
				g_opengl_context,
				reshade::api::shader_stage::all,
				// See global pipeline layout specified in 'wgl_device::wgl_device'
				get_opengl_pipeline_layout(), layout_param,
				reshade::api::descriptor_table_update { {}, index, 0, 1, type, &descriptor_data });
		}
		else if ((target == GL_TRANSFORM_FEEDBACK_BUFFER) && reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		{
			const reshade::api::resource buffer_handle = reshade::opengl::make_resource_handle(GL_BUFFER, buffer);
			uint64_t offset_64 = offset;
			uint64_t max_size_64 = size;

			reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(g_opengl_context, index, 1, &buffer_handle, &offset_64, &max_size_64, nullptr, nullptr);
		}
	}
#endif
}

void APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	static const auto trampoline = reshade::hooks::call(glBindFramebuffer);
	trampoline(target, framebuffer);

#if RESHADE_ADDON
	update_framebuffer_object(target, framebuffer);
#endif
}

void APIENTRY glBindVertexArray(GLuint array)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexArray);
	trampoline(array);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		g_opengl_context->_current_vao_dirty = true;

		update_current_input_layout();
	}
#endif
}

void APIENTRY glUniform1ui(GLint location, GLuint v0)
{
	static const auto trampoline = reshade::hooks::call(glUniform1ui);
	trampoline(location, v0);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLuint v[1] = { v0 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, 1, v);
	}
#endif
}
void APIENTRY glUniform2ui(GLint location, GLuint v0, GLuint v1)
{
	static const auto trampoline = reshade::hooks::call(glUniform2ui);
	trampoline(location, v0, v1);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLuint v[2] = { v0, v1 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, 2, v);
	}
#endif
}
void APIENTRY glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
	static const auto trampoline = reshade::hooks::call(glUniform3ui);
	trampoline(location, v0, v1, v2);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLuint v[3] = { v0, v1, v2 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, 3, v);
	}
#endif
}
void APIENTRY glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
	static const auto trampoline = reshade::hooks::call(glUniform4ui);
	trampoline(location, v0, v1, v2, v3);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		const GLuint v[4] = { v0, v1, v2, v3 };
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, 4, v);
	}
#endif
}

void APIENTRY glUniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform1uiv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, count * 1, value);
	}
#endif
}
void APIENTRY glUniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform2uiv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, count * 2, value);
	}
#endif
}
void APIENTRY glUniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform3uiv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, count * 3, value);
	}
#endif
}
void APIENTRY glUniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
	static const auto trampoline = reshade::hooks::call(glUniform4uiv);
	trampoline(location, count, value);

#if RESHADE_ADDON >= 2
	if (location < 0)
		return;

	if (g_opengl_context)
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			g_opengl_context, reshade::api::shader_stage::all, get_opengl_pipeline_layout(), 6, location * 4, count * 4, value);
	}
#endif
}

void APIENTRY glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	static const auto trampoline = reshade::hooks::call(glVertexAttribIPointer);
	trampoline(index, size, type, stride, pointer);

#if RESHADE_ADDON >= 2
	if (g_opengl_context != nullptr)
		g_opengl_context->_current_vao_dirty = true;
#endif
}
#endif

#ifdef GL_VERSION_3_1
void APIENTRY glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
#if RESHADE_ADDON
	glTexBufferRange(target, internalformat, buffer, 0, -1);
#else
	static const auto trampoline = reshade::hooks::call(glTexBuffer);
	trampoline(target, internalformat, buffer);
#endif
}

void APIENTRY glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON >= 2
	if (copy_buffer_region(readTarget, 0, readOffset, writeTarget, 0, writeOffset, size))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyBufferSubData);
	trampoline(readTarget, writeTarget, readOffset, writeOffset, size);
}

void APIENTRY glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode);

		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_opengl_context, primcount, count, first, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstanced);
	trampoline(mode, first, count, primcount);
}
void APIENTRY glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, primcount, count, offset, 0, 0))
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
	// Need to update render target and depth-stencil bindings after the current framebuffer was updated again
	update_framebuffer_object(target);
#endif
}

void APIENTRY glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage2DMultisample);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE);

	if (g_opengl_context && !proxy_object)
	{
		init_resource resource(target, 0, 1, samples, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, nullptr);
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexImage3DMultisample);

#if RESHADE_ADDON
	internalformat = reshade::opengl::convert_sized_internal_format(internalformat);

	// Ignore proxy texture objects
	const bool proxy_object = (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY);

	if (g_opengl_context && !proxy_object)
	{
		init_resource resource(target, 0, 1, samples, internalformat, width, height, depth);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, &depth);
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count, 1, offset, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, basevertex);
}
void APIENTRY glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count, 1, offset, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawRangeElementsBaseVertex);
	trampoline(mode, start, end, count, type, indices, basevertex);
}
void APIENTRY glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, primcount, count, offset, basevertex, 0))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseVertex);
	trampoline(mode, count, type, indices, primcount, basevertex);
}
void APIENTRY glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei drawcount, const GLint *basevertex)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);

		for (GLsizei i = 0; i < drawcount; ++i)
		{
			const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices[i]) / reshade::opengl::get_index_type_size(type)) : 0;

			if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, count[i], 1, offset, basevertex[i], 0))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glMultiDrawElementsBaseVertex);
	trampoline(mode, count, type, indices, drawcount, basevertex);
}
#endif

#ifdef GL_VERSION_4_0
void APIENTRY glDrawArraysIndirect(GLenum mode, const GLvoid *indirect)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		GLint indirect_buffer_binding = 0;
		gl.GetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);
		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
					g_opengl_context,
					reshade::api::indirect_command::draw,
					reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding),
					reinterpret_cast<uintptr_t>(indirect),
					1,
					0))
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
	if (g_opengl_context)
	{
		GLint indirect_buffer_binding = 0;
		gl.GetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);
		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode, type);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
					g_opengl_context,
					reshade::api::indirect_command::draw_indexed,
					reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding),
					reinterpret_cast<uintptr_t>(indirect),
					1,
					0))
				return;
		}
		else
		{
			// Redirect to non-indirect draw call so proper event is invoked
			const auto cmd = static_cast<const DrawElementsIndirectCommand *>(indirect);
			glDrawElementsInstancedBaseVertexBaseInstance(
				mode,
				cmd->count,
				type,
				reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(static_cast<size_t>(cmd->firstindex) * reshade::opengl::get_index_type_size(type))),
				cmd->primcount,
				cmd->basevertex,
				cmd->baseinstance);
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
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		GLint clip_origin = GL_LOWER_LEFT;
		if (gl.ClipControl != nullptr)
			gl.GetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

		temp_mem<reshade::api::rect> rect_data(count);
		for (GLsizei i = 0; i < count; ++i, v += 4)
		{
			rect_data[i].left = v[0];
			rect_data[i].right = v[0] + v[2];

			if (clip_origin == GL_UPPER_LEFT)
			{
				rect_data[i].top = v[1];
				rect_data[i].bottom = v[1] + v[3];
			}
			else
			{
				assert(g_opengl_context->_current_window_height != 0);
				rect_data[i].top = g_opengl_context->_current_window_height - (v[1] + v[3]);
				rect_data[i].bottom = g_opengl_context->_current_window_height - v[1];
			}
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_opengl_context, first, count, rect_data.p);
	}
#endif
}
void APIENTRY glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexed);
	trampoline(index, left, bottom, width, height);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		GLint clip_origin = GL_LOWER_LEFT;
		if (gl.ClipControl != nullptr)
			gl.GetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

		reshade::api::rect rect_data;
		rect_data.left = left;
		rect_data.right = left + width;

		if (clip_origin == GL_UPPER_LEFT)
		{
			rect_data.top = bottom;
			rect_data.bottom = bottom + height;
		}
		else
		{
			assert(g_opengl_context->_current_window_height != 0);
			rect_data.top = g_opengl_context->_current_window_height - (bottom + height);
			rect_data.bottom = g_opengl_context->_current_window_height - bottom;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_opengl_context, index, 1, &rect_data);
	}
#endif
}
void APIENTRY glScissorIndexedv(GLuint index, const GLint *v)
{
	static const auto trampoline = reshade::hooks::call(glScissorIndexedv);
	trampoline(index, v);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_scissor_rects>())
	{
		GLint clip_origin = GL_LOWER_LEFT;
		if (gl.ClipControl != nullptr)
			gl.GetIntegerv(GL_CLIP_ORIGIN, &clip_origin);

		reshade::api::rect rect_data;
		rect_data.left = v[0];
		rect_data.right = v[0] + v[2];

		if (clip_origin == GL_UPPER_LEFT)
		{
			rect_data.top = v[1];
			rect_data.bottom = v[1] + v[3];
		}
		else
		{
			assert(g_opengl_context->_current_window_height != 0);
			rect_data.top = g_opengl_context->_current_window_height - (v[1] + v[3]);
			rect_data.bottom = g_opengl_context->_current_window_height - v[1];
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(g_opengl_context, index, 1, &rect_data);
	}
#endif
}

void APIENTRY glViewportArrayv(GLuint first, GLsizei count, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glViewportArrayv);
	trampoline(first, count, v);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		temp_mem<reshade::api::viewport> viewport_data(count);
		for (GLsizei i = 0; i < count; ++i, v += 4)
		{
			viewport_data[i].x = v[0];
			viewport_data[i].y = v[1];
			viewport_data[i].width = v[2];
			viewport_data[i].height = v[3];
			viewport_data[i].min_depth = 0.0f; // This is set via 'glDepthRange', so just assume defaults here for now
			viewport_data[i].max_depth = 1.0f;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_opengl_context, first, count, viewport_data.p);
	}
#endif
}
void APIENTRY glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
	static const auto trampoline = reshade::hooks::call(glViewportIndexedf);
	trampoline(index, x, y, w, h);

#if RESHADE_ADDON
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		const reshade::api::viewport viewport_data = { x, y, w, h, 0.0f, 1.0f };

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_opengl_context, index, 1, &viewport_data);
	}
#endif
}
void APIENTRY glViewportIndexedfv(GLuint index, const GLfloat *v)
{
	static const auto trampoline = reshade::hooks::call(glViewportIndexedfv);
	trampoline(index, v);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		const reshade::api::viewport viewport_data = { v[0], v[1], v[2], v[3], 0.0f, 1.0f };

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(g_opengl_context, index, 1, &viewport_data);
	}
#endif
}

void APIENTRY glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
	static const auto trampoline = reshade::hooks::call(glVertexAttribLPointer);
	trampoline(index, size, type, stride, pointer);

#if RESHADE_ADDON >= 2
	if (g_opengl_context != nullptr)
		g_opengl_context->_current_vao_dirty = true;
#endif
}
#endif

#ifdef GL_VERSION_4_2
void APIENTRY glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage1D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(target, 0, levels, 1, internalformat, width, 1, 1);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, nullptr, nullptr);
		trampoline(target, levels, internalformat, width);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, levels, internalformat, width);
}
void APIENTRY glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(target, 0, levels, 1, internalformat, width, height, 1);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, &height, nullptr);
		trampoline(target, levels, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, levels, internalformat, width, height);
}
void APIENTRY glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(target, 0, levels, 1, internalformat, width, height, depth);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, &height, &depth);
		trampoline(target, levels, internalformat, width, height, depth);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, levels, internalformat, width, height, depth);
}

void APIENTRY glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTexture);
	trampoline(unit, texture, level, layered, layer, access, format);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		GLint target = GL_TEXTURE;
		if (gl.GetTextureParameteriv != nullptr)
			gl.GetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		const reshade::api::resource_view descriptor_data = reshade::opengl::make_resource_view_handle(target, texture);

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 3,
			reshade::api::descriptor_table_update { {}, unit, 0, 1, reshade::api::descriptor_type::unordered_access_view, &descriptor_data });
	}
#endif
}

void APIENTRY glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei primcount, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode);

		if (reshade::invoke_addon_event<reshade::addon_event::draw>(g_opengl_context, primcount, count, first, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawArraysInstancedBaseInstance);
	trampoline(mode, first, count, primcount, baseinstance);
}
void APIENTRY glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, primcount, count, offset, 0, baseinstance))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDrawElementsInstancedBaseInstance);
	trampoline(mode, count, type, indices, primcount, baseinstance);
}
void APIENTRY glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount, GLint basevertex, GLuint baseinstance)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		update_current_primitive_topology(mode, type);

		GLint index_buffer_binding = 0;
		gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index_buffer_binding);
		const uint32_t offset = (index_buffer_binding != 0) ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(indices) / reshade::opengl::get_index_type_size(type)) : 0;

		if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(g_opengl_context, primcount, count, offset, basevertex, baseinstance))
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
	if (g_opengl_context)
	{
		init_resource_view resource_view(target, texture, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
		resource_view.invoke_create_event(&internalformat, &minlevel, &numlevels, &minlayer, &numlayers);
		trampoline(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
		resource_view.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
}

void APIENTRY glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTexBufferRange);

#if RESHADE_ADDON
	if (size == -1)
	{
		if (gl.GetNamedBufferParameteri64v != nullptr)
		{
#ifndef _WIN64
			gl.GetNamedBufferParameteriv(buffer, GL_BUFFER_SIZE, reinterpret_cast<GLint *>(&size));
#else
			gl.GetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
#endif
		}
		else
		{
			GLint prev_binding = 0;
			gl.GetIntegerv(GL_COPY_READ_BUFFER_BINDING, &prev_binding);
			gl.BindBuffer(GL_COPY_READ_BUFFER, buffer);
#ifndef _WIN64
			gl.GetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, reinterpret_cast<GLint *>(&size));
#else
			gl.GetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);
#endif
			gl.BindBuffer(GL_COPY_READ_BUFFER, prev_binding);
		}
	}

	if (g_opengl_context)
	{
		init_resource_view resource_view(target, 0, buffer, internalformat, offset, size);
		resource_view.invoke_create_event(&internalformat, &offset, &size);
		trampoline(target, internalformat, buffer, offset, size);
		resource_view.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, internalformat, buffer, offset, size);
}

void APIENTRY glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage2DMultisample);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(target, 0, 1, samples, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, nullptr);
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTexStorage3DMultisample);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(target, 0, 1, samples, internalformat, width, height, depth);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, &depth);
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth)
{
#if RESHADE_ADDON >= 2
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		g_opengl_context->_current_vbo_dirty = false;

		const reshade::api::resource buffer_handle = reshade::opengl::make_resource_handle(GL_BUFFER, buffer);
		uint64_t offset_64 = offset;
		uint32_t stride_32 = stride;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(g_opengl_context, bindingindex, 1, &buffer_handle, &offset_64, &stride_32);
	}
#endif
}

void APIENTRY glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::dispatch>(g_opengl_context, num_groups_x, num_groups_y, num_groups_z))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDispatchCompute);
	trampoline(num_groups_x, num_groups_y, num_groups_z);
}
void APIENTRY glDispatchComputeIndirect(GLintptr indirect)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		GLint indirect_buffer_binding = 0;
		gl.GetIntegerv(GL_DISPATCH_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);
		if (0 != indirect_buffer_binding)
		{
			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
					g_opengl_context,
					reshade::api::indirect_command::dispatch,
					reshade::opengl::make_resource_handle(GL_DISPATCH_INDIRECT_BUFFER, indirect_buffer_binding),
					indirect,
					1,
					0))
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
	if (g_opengl_context)
	{
		GLint indirect_buffer_binding = 0;
		gl.GetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);
		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
					g_opengl_context,
					reshade::api::indirect_command::draw,
					reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding),
					reinterpret_cast<uintptr_t>(indirect),
					drawcount,
					stride))
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
	if (g_opengl_context)
	{
		GLint indirect_buffer_binding = 0;
		gl.GetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &indirect_buffer_binding);
		if (0 != indirect_buffer_binding)
		{
			update_current_primitive_topology(mode, type);

			if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(
					g_opengl_context,
					reshade::api::indirect_command::draw_indexed,
					reshade::opengl::make_resource_handle(GL_BUFFER, indirect_buffer_binding),
					reinterpret_cast<uintptr_t>(indirect),
					drawcount,
					stride))
				return;
		}
		else
		{
			// Redirect to non-indirect draw calls so proper events are invoked
			for (GLsizei i = 0; i < drawcount; ++i)
			{
				const auto cmd = reinterpret_cast<const DrawElementsIndirectCommand *>(static_cast<const uint8_t *>(indirect) + i * (stride != 0 ? stride : sizeof(DrawElementsIndirectCommand)));
				glDrawElementsInstancedBaseVertexBaseInstance(
					mode,
					cmd->count,
					type,
					reinterpret_cast<const GLvoid *>(static_cast<uintptr_t>(static_cast<size_t>(cmd->firstindex) * reshade::opengl::get_index_type_size(type))),
					cmd->primcount,
					cmd->basevertex,
					cmd->baseinstance);
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
	if (g_opengl_context)
	{
		init_resource resource(target, 0, size, flags);
		resource.invoke_create_event(&size, &flags, data);
		trampoline(target, size, data, flags);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(target, size, data, flags);
}

void APIENTRY glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersBase);
	trampoline(target, first, count, buffers);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		if ((target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) && reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		{
			temp_mem<reshade::api::buffer_range> descriptor_data(count);
			if (buffers != nullptr)
			{
				for (GLsizei i = 0; i < count; ++i)
				{
					descriptor_data[i].buffer = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
					descriptor_data[i].offset = 0;
					descriptor_data[i].size = UINT64_MAX;
				}
			}
			else
			{
				std::memset(descriptor_data.p, 0, count * sizeof(reshade::api::buffer_range));
			}

			const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
			const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 1;

			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
				g_opengl_context,
				reshade::api::shader_stage::all,
				// See global pipeline layout specified in 'wgl_device::wgl_device'
				get_opengl_pipeline_layout(), layout_param,
				reshade::api::descriptor_table_update { {}, first, 0, static_cast<uint32_t>(count), type, descriptor_data.p });
		}
		else if ((target == GL_TRANSFORM_FEEDBACK_BUFFER) && reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		{
			temp_mem<reshade::api::resource> buffer_handles(count);
			temp_mem<uint64_t> offsets_64(count);
			temp_mem<uint64_t> max_sizes_64(count);
			if (buffers != nullptr)
			{
				for (GLsizei i = 0; i < count; ++i)
				{
					buffer_handles[i] = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
					offsets_64[i] = 0;
					max_sizes_64[i] = UINT64_MAX;
				}
			}
			else
			{
				std::memset(buffer_handles.p, 0, count * sizeof(reshade::api::resource));
				std::memset(offsets_64.p, 0, count * sizeof(reshade::api::resource));
				std::memset(max_sizes_64.p, 0, count * sizeof(reshade::api::resource));
			}

			reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(g_opengl_context, first, count, buffer_handles.p, offsets_64.p, max_sizes_64.p, nullptr, nullptr);
		}
	}
#endif
}
void APIENTRY glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLintptr *sizes)
{
	static const auto trampoline = reshade::hooks::call(glBindBuffersRange);
	trampoline(target, first, count, buffers, offsets, sizes);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		if ((target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) && reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		{
			temp_mem<reshade::api::buffer_range> descriptor_data(count);
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
				std::memset(descriptor_data.p, 0, count * sizeof(reshade::api::buffer_range));
			}

			const auto type = (target == GL_UNIFORM_BUFFER) ? reshade::api::descriptor_type::constant_buffer : reshade::api::descriptor_type::shader_storage_buffer;
			const auto layout_param = (target == GL_UNIFORM_BUFFER) ? 2 : 1;

			reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
				g_opengl_context,
				reshade::api::shader_stage::all,
				// See global pipeline layout specified in 'wgl_device::wgl_device'
				get_opengl_pipeline_layout(), layout_param,
				reshade::api::descriptor_table_update { {}, first, 0, static_cast<uint32_t>(count), type, descriptor_data.p });
		}
		else if ((target == GL_TRANSFORM_FEEDBACK_BUFFER) && reshade::has_addon_event<reshade::addon_event::bind_stream_output_buffers>())
		{
			temp_mem<reshade::api::resource> buffer_handles(count);
			temp_mem<uint64_t> offsets_64(count);
			temp_mem<uint64_t> max_sizes_64(count);
			if (buffers != nullptr)
			{
				for (GLsizei i = 0; i < count; ++i)
				{
					buffer_handles[i] = reshade::opengl::make_resource_handle(GL_BUFFER, buffers[i]);
					offsets_64[i] = static_cast<uint64_t>(offsets[i]);
					max_sizes_64[i] = static_cast<uint64_t>(sizes[i]);
				}
			}
			else
			{
				std::memset(buffer_handles.p, 0, count * sizeof(reshade::api::resource));
				std::memset(offsets_64.p, 0, count * sizeof(reshade::api::resource));
				std::memset(max_sizes_64.p, 0, count * sizeof(reshade::api::resource));
			}

			reshade::invoke_addon_event<reshade::addon_event::bind_stream_output_buffers>(g_opengl_context, first, count, buffer_handles.p, offsets_64.p, max_sizes_64.p, nullptr, nullptr);
		}
	}
#endif
}

void APIENTRY glBindTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		temp_mem<reshade::api::sampler_with_resource_view> descriptor_data(count);
		if (textures != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
			{
				GLint target = GL_TEXTURE;
				if (gl.GetTextureParameteriv != nullptr)
					gl.GetTextureParameteriv(textures[i], GL_TEXTURE_TARGET, &target);

				descriptor_data[i].view = reshade::opengl::make_resource_view_handle(target, textures[i]);
				descriptor_data[i].sampler = { 0 };
			}
		}
		else
		{
			std::memset(descriptor_data.p, 0, count * sizeof(reshade::api::sampler_with_resource_view));
		}

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 0,
			reshade::api::descriptor_table_update { {}, first, 0, static_cast<uint32_t>(count), reshade::api::descriptor_type::sampler_with_resource_view, descriptor_data.p });
	}
#endif
}

void APIENTRY glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures)
{
	static const auto trampoline = reshade::hooks::call(glBindImageTextures);
	trampoline(first, count, textures);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		temp_mem<reshade::api::resource_view> descriptor_data(count);
		if (textures != nullptr)
		{
			for (GLsizei i = 0; i < count; ++i)
			{
				GLint target = GL_TEXTURE;
				if (gl.GetTextureParameteriv != nullptr)
					gl.GetTextureParameteriv(textures[i], GL_TEXTURE_TARGET, &target);

				descriptor_data[i] = reshade::opengl::make_resource_view_handle(target, textures[i]);
			}
		}
		else
		{
			std::memset(descriptor_data.p, 0, count * sizeof(reshade::api::resource_view));
		}

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 3,
			reshade::api::descriptor_table_update { {}, first, 0, static_cast<uint32_t>(count), reshade::api::descriptor_type::unordered_access_view, descriptor_data.p });
	}
#endif
}

void APIENTRY glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides)
{
	static const auto trampoline = reshade::hooks::call(glBindVertexBuffers);
	trampoline(first, count, buffers, offsets, strides);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		g_opengl_context->_current_vbo_dirty = false;

		temp_mem<reshade::api::resource> buffer_handles(count);
		temp_mem<uint64_t> offsets_64(count);
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
			std::memset(buffer_handles.p, 0, count * sizeof(reshade::api::resource));
			std::memset(offsets_64.p, 0, count * sizeof(uint64_t));
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(
			g_opengl_context, first, count, buffer_handles.p, offsets_64.p, reinterpret_cast<const uint32_t *>(strides));
	}
#endif
}
#endif

#ifdef GL_VERSION_4_5
void APIENTRY glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer)
{
#if RESHADE_ADDON
	glTextureBufferRange(texture, internalformat, buffer, 0, -1);
#else
	static const auto trampoline = reshade::hooks::call(glTextureBuffer);
	trampoline(texture, internalformat, buffer);
#endif
}
void APIENTRY glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
	static const auto trampoline = reshade::hooks::call(glTextureBufferRange);

#if RESHADE_ADDON
	if (size == -1)
	{
#ifndef _WIN64
		assert(gl.GetNamedBufferParameteriv != nullptr);
		gl.GetNamedBufferParameteriv(buffer, GL_BUFFER_SIZE, reinterpret_cast<GLint *>(&size));
#else
		assert(gl.GetNamedBufferParameteri64v != nullptr);
		gl.GetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
#endif
	}

	if (g_opengl_context)
	{
		init_resource_view resource_view(GL_TEXTURE_BUFFER, texture, buffer, internalformat, offset, size);
		resource_view.invoke_create_event(&internalformat, &offset, &size);
		trampoline(texture, internalformat, buffer, offset, size);
		resource_view.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, internalformat, buffer, offset, size);
}

void APIENTRY glNamedBufferData(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferData);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		GLbitfield storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT;

		init_resource resource(GL_BUFFER, buffer, size, storage_flags);
		resource.invoke_create_event(&size, &storage_flags, data);
		trampoline(buffer, size, data, usage);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(buffer, size, data, usage);
}

void APIENTRY glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags)
{
	static const auto trampoline = reshade::hooks::call(glNamedBufferStorage);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_BUFFER, buffer, size, flags);
		resource.invoke_create_event(&size, &flags, data);
		trampoline(buffer, size, data, flags);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(buffer, size, data, flags);
}

void APIENTRY glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage1D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_TEXTURE_1D, texture, levels, 1, internalformat, width, 1, 1);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, nullptr, nullptr);
		trampoline(texture, levels, internalformat, width);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, levels, internalformat, width);
}
void APIENTRY glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_TEXTURE_2D, texture, levels, 1, internalformat, width, height, 1);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, &height, nullptr);
		trampoline(texture, levels, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, levels, internalformat, width, height);
}
void APIENTRY glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage2DMultisample);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_TEXTURE_2D, texture, 1, samples, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, nullptr);
		trampoline(texture, samples, internalformat, width, height, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, samples, internalformat, width, height, fixedsamplelocations);
}
void APIENTRY glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3D);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_TEXTURE_3D, texture, levels, 1, internalformat, width, height, depth);
		resource.invoke_create_event(&levels, nullptr, &internalformat, &width, &height, &depth);
		trampoline(texture, levels, internalformat, width, height, depth);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, levels, internalformat, width, height, depth);
}
void APIENTRY glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations)
{
	static const auto trampoline = reshade::hooks::call(glTextureStorage3DMultisample);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_TEXTURE_3D, texture, 1, samples, internalformat, width, height, depth);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, &depth);
		trampoline(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(texture, samples, internalformat, width, height, depth, fixedsamplelocations);
}

void APIENTRY glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_buffer_region(GL_BUFFER, buffer, offset, size, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glNamedBufferSubData);
	trampoline(buffer, offset, size, data);
}

void APIENTRY glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid* pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage1D);
	trampoline(texture, level, xoffset, width, format, type, pixels);
}
void APIENTRY glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}
void APIENTRY glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
void APIENTRY glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage1D);
	trampoline(texture, level, xoffset, width, format, imageSize, data);
}
void APIENTRY glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void APIENTRY glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
#if RESHADE_ADDON >= 2
	if (update_texture_region(GL_TEXTURE, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, GL_UNSIGNED_BYTE, data))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCompressedTextureSubImage3D);
	trampoline(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void APIENTRY glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_TEXTURE, texture, level, xoffset, 0, 0, width, 1, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage1D);
	trampoline(texture, level, xoffset, x, y, width);
}
void APIENTRY glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON >= 2
	if (copy_texture_region(GL_FRAMEBUFFER_DEFAULT, 0, 0, x, y, 0, GL_TEXTURE, texture, level, xoffset, yoffset, 0, width, height, 1))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyTextureSubImage2D);
	trampoline(texture, level, xoffset, yoffset, x, y, width, height);
}
void APIENTRY glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
#if RESHADE_ADDON >= 2
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
			0,
			UINT64_MAX,
			reshade::opengl::convert_access_flags(access),
			&result);
	}
#endif

	return result;
}
auto APIENTRY glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLenum access) -> void *
{
	static const auto trampoline = reshade::hooks::call(glMapNamedBufferRange);
	void *result = trampoline(buffer, offset, length, access);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, buffer),
			offset,
			length,
			reshade::opengl::convert_access_flags(access),
			&result);
	}
#endif

	return result;
}
void APIENTRY glUnmapNamedBuffer(GLuint buffer)
{
#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(
			device,
			reshade::opengl::make_resource_handle(GL_BUFFER, buffer));
	}
#endif

	static const auto trampoline = reshade::hooks::call(glUnmapNamedBuffer);
	trampoline(buffer);
}

void APIENTRY glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
#if RESHADE_ADDON >= 2
	if (copy_buffer_region(GL_BUFFER, readBuffer, readOffset, GL_BUFFER, writeBuffer, writeOffset, size))
		return;
#endif

	static const auto trampoline = reshade::hooks::call(glCopyNamedBufferSubData);
	trampoline(readBuffer, writeBuffer, readOffset, writeOffset, size);
}

void APIENTRY glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorage);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_RENDERBUFFER, renderbuffer, 1, 1, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, nullptr, &internalformat, &width, &height, nullptr);
		trampoline(renderbuffer, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(renderbuffer, internalformat, width, height);
}
void APIENTRY glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	static const auto trampoline = reshade::hooks::call(glNamedRenderbufferStorageMultisample);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		init_resource resource(GL_RENDERBUFFER, renderbuffer, 1, samples, internalformat, width, height, 1);
		resource.invoke_create_event(nullptr, &samples, &internalformat, &width, &height, nullptr);
		trampoline(renderbuffer, samples, internalformat, width, height);
		resource.invoke_initialize_event();
	}
	else
#endif
		trampoline(renderbuffer, samples, internalformat, width, height);
}

void APIENTRY glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_STENCIL && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		const reshade::api::resource_view view = device->get_framebuffer_attachment(framebuffer, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, nullptr, reinterpret_cast<const uint8_t *>(value), 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, reinterpret_cast<const float *>(value), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferiv);
	trampoline(framebuffer, buffer, drawbuffer, value);
}
void APIENTRY glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_STENCIL && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		const reshade::api::resource_view view = device->get_framebuffer_attachment(framebuffer, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, nullptr, reinterpret_cast<const uint8_t *>(value), 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, reinterpret_cast<const float *>(value), 0, nullptr))
				return;
		}
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferuiv);
	trampoline(framebuffer, buffer, drawbuffer, value);
}
void APIENTRY glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
#if RESHADE_ADDON
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>() ||
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>()))
	{
		assert(buffer == GL_COLOR || (buffer == GL_DEPTH && drawbuffer == 0));

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		const reshade::api::resource_view view = device->get_framebuffer_attachment(framebuffer, buffer, drawbuffer);
		if (view != 0)
		{
			if (buffer != GL_COLOR ?
					reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, view, value, nullptr, 0, nullptr) :
					reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(g_opengl_context, view, value, 0, nullptr))
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
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		assert(buffer == GL_DEPTH_STENCIL && drawbuffer == 0);

		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		const reshade::api::resource_view dsv = device->get_framebuffer_attachment(framebuffer, buffer, drawbuffer);
		if (dsv != 0 &&
			reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(g_opengl_context, dsv, &depth, reinterpret_cast<const uint8_t *>(&stencil), 0, nullptr))
			return;
	}
#endif

	static const auto trampoline = reshade::hooks::call(glClearNamedFramebufferfi);
	trampoline(framebuffer, buffer, drawbuffer, depth, stencil);
}

void APIENTRY glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
#if RESHADE_ADDON >= 2
	if (g_opengl_context && (
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>()))
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		constexpr GLbitfield flags[3] = { GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT };
		for (GLbitfield flag : flags)
		{
			if ((mask & flag) != flag)
				continue;

			reshade::api::resource src = device->get_resource_from_view(device->get_framebuffer_attachment(readFramebuffer, flag, 0));
			reshade::api::resource dst = device->get_resource_from_view(device->get_framebuffer_attachment(drawFramebuffer, flag, 0));

			if (device->get_resource_desc(src).texture.samples <= 1)
			{
				const reshade::api::subresource_box src_box = { srcX0, srcY0, 0, srcX1, srcY1, 1 }, dst_box = { dstX0, dstY0, 0, dstX1, dstY1, 1 };

				if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(g_opengl_context, src, 0, &src_box, dst, 0, &dst_box, (filter == GL_NONE || filter == GL_NEAREST) ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
					mask ^= flag;
			}
			else
			{
				assert((srcX1 - srcX0) == (dstX1 - dstX0) && (srcY1 - srcY0) == (dstY1 - dstY0));

				const reshade::api::subresource_box src_box = { srcX0, srcY0, 0, srcX1, srcY1, 1 };

				if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(g_opengl_context, src, 0, &src_box, dst, 0, dstX0, dstY0, 0, reshade::api::format::unknown))
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
#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		GLint target = GL_TEXTURE;
		gl.GetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(g_opengl_context, reshade::opengl::make_resource_view_handle(target, texture)))
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

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		GLint target = GL_TEXTURE;
		gl.GetTextureParameteriv(texture, GL_TEXTURE_TARGET, &target);

		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 0,
			reshade::api::descriptor_table_update { {}, unit, 0, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data });
	}
#endif
}
#endif

void APIENTRY glBindProgramARB(GLenum target, GLuint program)
{
	static const auto trampoline = reshade::hooks::call(glBindProgramARB);
	trampoline(target, program);

#if RESHADE_ADDON >= 2
	if (g_opengl_context)
	{
		reshade::api::pipeline_stage stage;
		switch (target)
		{
		case 0x8620 /* GL_VERTEX_PROGRAM_ARB */:
			stage = reshade::api::pipeline_stage::vertex_shader;
			break;
		case 0x8804 /* GL_FRAGMENT_PROGRAM_ARB */:
			stage = reshade::api::pipeline_stage::pixel_shader;
			break;
		default:
			assert(false);
			return;
		}

		const auto pipeline = (program != 0) ? reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | program } : reshade::api::pipeline {};

		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(g_opengl_context, stage, pipeline);
	}
#endif
}
void APIENTRY glProgramStringARB(GLenum target, GLenum format, GLsizei length, const GLvoid *string)
{
	static const auto trampoline = reshade::hooks::call(glProgramStringARB);

#if RESHADE_ADDON
	if (g_opengl_context)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		reshade::api::pipeline_subobject_type subobject_type;
		switch (target)
		{
		case 0x8620 /* GL_VERTEX_PROGRAM_ARB */:
			subobject_type = reshade::api::pipeline_subobject_type::vertex_shader;
			break;
		case 0x8804 /* GL_FRAGMENT_PROGRAM_ARB */:
			subobject_type = reshade::api::pipeline_subobject_type::pixel_shader;
			break;
		default:
			assert(false);
			subobject_type = reshade::api::pipeline_subobject_type::unknown;
			break;
		}

		reshade::api::shader_desc desc = {};
		desc.code = string;
		desc.code_size = length;
		desc.entry_point = nullptr;

		const reshade::api::pipeline_subobject subobject = { subobject_type, 1, &desc };

		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(device, get_opengl_pipeline_layout(), 1, &subobject))
		{
			assert(desc.code_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));
			string = desc.code;
			length = static_cast<GLsizei>(desc.code_size);
		}

		trampoline(target, format, length, string);

		GLuint program = 0;
		const auto glGetProgramivARB = reinterpret_cast<void(APIENTRY *)(GLenum target, GLenum pname, GLint *params)>(wglGetProcAddress("glGetProgramivARB"));
		assert(glGetProgramivARB != nullptr);
		glGetProgramivARB(target, 0x8677 /* GL_PROGRAM_BINDING_ARB */, reinterpret_cast<GLint *>(&program));

		// Only invoke 'init_pipeline' event for programs that were successfully loaded
		GLint status = -1;
		gl.GetIntegerv(0x864B /* GL_PROGRAM_ERROR_POSITION_ARB */, &status);

		if (program != 0 && status < 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(device, get_opengl_pipeline_layout(), 1, &subobject, reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | program });
		}
	}
	else
#endif
		trampoline(target, format, length, string);
}
void APIENTRY glDeleteProgramsARB(GLsizei n, const GLuint *programs)
{
#if RESHADE_ADDON
	if (g_opengl_context)
	{
		const auto device = static_cast<reshade::opengl::device_impl *>(g_opengl_context->get_device());

		for (GLsizei i = 0; i < n; ++i)
			if (programs[i] != 0)
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(device, reshade::api::pipeline { (static_cast<uint64_t>(GL_PROGRAM) << 40) | programs[i] });
	}
#endif

	static const auto trampoline = reshade::hooks::call(glDeleteProgramsARB);
	trampoline(n, programs);
}

void APIENTRY glBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
	static const auto trampoline = reshade::hooks::call(glBindFramebufferEXT);
	trampoline(target, framebuffer);

#if RESHADE_ADDON
	update_framebuffer_object(target, framebuffer);
#endif
}

void APIENTRY glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
	static const auto trampoline = reshade::hooks::call(glBindMultiTextureEXT);
	trampoline(texunit, target, texture);

#if RESHADE_ADDON >= 2
	if (g_opengl_context &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		assert(texunit >= GL_TEXTURE0);
		texunit -= GL_TEXTURE0;

		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::opengl::make_resource_view_handle(target, texture)
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			g_opengl_context,
			reshade::api::shader_stage::all,
			// See global pipeline layout specified in 'wgl_device::wgl_device'
			get_opengl_pipeline_layout(), 0,
			reshade::api::descriptor_table_update { {}, texunit, 0, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data });
	}
#endif
}
