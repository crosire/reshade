/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "render_gl.hpp"
#include "render_gl_utils.hpp"
#include <cassert>
#include <algorithm>

namespace
{
	struct pipeline_layout_impl
	{
		~pipeline_layout_impl()
		{
			glDeleteBuffers(1, &push_constants);
		}

		GLuint push_constants;
		GLuint push_constants_binding;
	};

	struct pipeline_compute_impl
	{
		~pipeline_compute_impl()
		{
			glDeleteProgram(program);
		}

		GLuint program;
	};
	struct pipeline_graphics_impl
	{
		~pipeline_graphics_impl()
		{
			glDeleteProgram(program);
			glDeleteVertexArrays(1, &vao);
		}

		GLuint program;
		GLuint vao;

		GLenum prim_mode;
		GLuint patch_vertices;
		GLenum front_face;
		GLenum cull_mode;
		GLenum polygon_mode;

		GLenum blend_eq;
		GLenum blend_eq_alpha;
		GLenum blend_src;
		GLenum blend_dst;
		GLenum blend_src_alpha;
		GLenum blend_dst_alpha;

		GLenum back_stencil_op_fail;
		GLenum back_stencil_op_depth_fail;
		GLenum back_stencil_op_pass;
		GLenum back_stencil_func;
		GLenum front_stencil_op_fail;
		GLenum front_stencil_op_depth_fail;
		GLenum front_stencil_op_pass;
		GLenum front_stencil_func;
		GLuint stencil_read_mask;
		GLuint stencil_write_mask;

		GLboolean blend_enable;
		GLboolean depth_test;
		GLboolean depth_write_mask;
		GLboolean stencil_test;
		GLboolean scissor_test;
		GLboolean multisample;
		GLboolean sample_alpha_to_coverage;
		GLbitfield sample_mask;

		GLuint color_write_mask;
		GLint stencil_reference_value;
	};
}

static GLint get_rbo_param(GLuint id, GLenum param)
{
	GLint value = 0;
	if (gl3wProcs.gl.GetNamedRenderbufferParameteriv != nullptr)
	{
		glGetNamedRenderbufferParameteriv(id, param, &value);
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_binding);
		glBindRenderbuffer(GL_RENDERBUFFER, id);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &value);
		glBindRenderbuffer(GL_RENDERBUFFER, prev_binding);
	}
	return value;
}
static GLint get_buf_param(GLenum target, GLuint id, GLenum param)
{
	GLint value = 0;
	if (gl3wProcs.gl.GetNamedBufferParameteriv != nullptr)
	{
		glGetNamedBufferParameteriv(id, param, &value);
	}
	else
	{
		if (GL_TEXTURE == target)
			target = GL_TEXTURE_2D;

		GLint prev_binding = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
		glBindBuffer(target, id);
		glGetBufferParameteriv(target, param, &value);
		glBindBuffer(target, prev_binding);
	}
	return value;
}
static GLint get_tex_param(GLenum target, GLuint id, GLenum param)
{
	GLint value = 0;
	if (gl3wProcs.gl.GetTextureParameteriv != nullptr)
	{
		glGetTextureParameteriv(id, param, &value);
	}
	else
	{
		if (GL_TEXTURE == target)
			target = GL_TEXTURE_2D;

		GLint prev_binding = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
		glBindTexture(target, id);
		glGetTexParameteriv(target, param, &value);
		glBindTexture(target, prev_binding);
	}
	return value;
}
static GLint get_tex_level_param(GLenum target, GLuint id, GLuint level, GLenum param)
{
	GLint value = 0;
	if (gl3wProcs.gl.GetTextureLevelParameteriv != nullptr)
	{
		glGetTextureLevelParameteriv(id, level, param, &value);
	}
	else
	{
		if (GL_TEXTURE == target)
			target = GL_TEXTURE_2D;

		GLint prev_binding = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
		glBindTexture(target, id);
		glGetTexLevelParameteriv(target, level, param, &value);
		glBindTexture(target, prev_binding);
	}
	return value;
}
static GLint get_fbo_attachment_param(GLuint id, GLenum attachment, GLenum param)
{
	GLint value = 0;
	if (gl3wProcs.gl.GetNamedFramebufferAttachmentParameteriv != nullptr)
	{
		glGetNamedFramebufferAttachmentParameteriv(id, attachment, param, &value);
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding);
		glBindFramebuffer(GL_FRAMEBUFFER, id);
		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, param, &value);
		glBindFramebuffer(GL_FRAMEBUFFER, prev_binding);
	}
	return value;
}

reshade::opengl::device_impl::device_impl(HDC hdc, HGLRC hglrc) :
	api_object_impl(hglrc)
{
	RECT window_rect = {};
	GetClientRect(WindowFromDC(hdc), &window_rect);

	_default_fbo_width = window_rect.right - window_rect.left;
	_default_fbo_height = window_rect.bottom - window_rect.top;

	// The pixel format has to be the same for all device contexts used with this rendering context, so can cache information about it here
	// See https://docs.microsoft.com/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent
	PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
	DescribePixelFormat(hdc, GetPixelFormat(hdc), sizeof(pfd), &pfd);

	switch (pfd.cColorBits) // Excluding alpha
	{
	default:
	case  0: _default_color_format = GL_NONE;
		break;
	case 24: _default_color_format = GL_RGBA8;
		break;
	case 30: _default_color_format = GL_RGB10_A2;
		break;
	case 48: _default_color_format = GL_RGBA16F;
		break;
	}

	switch (pfd.cDepthBits)
	{
	default:
	case  0: _default_depth_format = GL_NONE; // No depth in this pixel format
		break;
	case 16: _default_depth_format = GL_DEPTH_COMPONENT16;
		break;
	case 24: _default_depth_format = pfd.cStencilBits ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
		break;
	case 32: _default_depth_format = pfd.cStencilBits ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32;
		break;
	}

	// Check for special extension to detect whether this is a compatibility context (https://www.khronos.org/opengl/wiki/OpenGL_Context#OpenGL_3.1_and_ARB_compatibility)
	GLint num_extensions = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
	for (GLint i = 0; i < num_extensions; ++i)
	{
		const GLubyte *const extension = glGetStringi(GL_EXTENSIONS, i);
		if (std::strcmp(reinterpret_cast<const char *>(extension), "GL_ARB_compatibility") == 0)
		{
			_compatibility_context = true;
			break;
		}
	}

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](unsigned int /*source*/, unsigned int type, unsigned int /*id*/, unsigned int /*severity*/, int /*length*/, const char *message, const void */*userParam*/) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
	}, nullptr);
#endif

#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

#if RESHADE_ADDON
	// Communicate default state to add-ons
	const api::resource_view default_depth_stencil = get_depth_stencil_from_fbo(0);
	const api::resource_view default_render_target = get_render_target_from_fbo(0, 0);
	invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 1, &default_render_target, default_depth_stencil);
#endif
}
reshade::opengl::device_impl::~device_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif

	// Destroy framebuffers used in 'copy_resource' implementation
	glDeleteFramebuffers(2, _copy_fbo);
}

bool reshade::opengl::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	GLenum internal_format = GL_NONE;
	convert_format_to_internal_format(format, internal_format);

	GLint supported = GL_FALSE;
	glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);

	GLint supported_renderable = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::render_target) != 0)
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_FRAMEBUFFER_RENDERABLE, 1, &supported_renderable);

	GLint supported_image_load = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::unordered_access) != 0)
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_SHADER_IMAGE_LOAD, 1, &supported_image_load);

	return supported != GL_FALSE && supported_renderable != GL_NONE && supported_image_load != GL_NONE;
}

bool reshade::opengl::device_impl::check_resource_handle_valid(api::resource resource) const
{
	switch (resource.handle >> 40)
	{
	default:
		return false;
	case GL_BUFFER:
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
		return glIsBuffer(resource.handle & 0xFFFFFFFF) != GL_FALSE;
	case GL_TEXTURE:
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
		return glIsTexture(resource.handle & 0xFFFFFFFF) != GL_FALSE;
	case GL_RENDERBUFFER:
		return glIsRenderbuffer(resource.handle & 0xFFFFFFFF) != GL_FALSE;
	case GL_FRAMEBUFFER_DEFAULT:
		return (resource.handle & 0xFFFFFFFF) != GL_DEPTH_ATTACHMENT || _default_depth_format != GL_NONE;
	}
}
bool reshade::opengl::device_impl::check_resource_view_handle_valid(api::resource_view view) const
{
	const GLenum attachment = view.handle >> 40;
	if ((attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT31) || attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT)
	{
		const GLuint fbo = view.handle & 0xFFFFFFFF;
		return fbo == 0 || glIsFramebuffer(fbo) != GL_FALSE;
	}
	else
	{
		return check_resource_handle_valid({ view.handle });
	}
}

bool reshade::opengl::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	GLuint object = 0;
	glGenSamplers(1, &object);

	GLenum min_filter = GL_NONE;
	GLenum mag_filter = GL_NONE;
	switch (desc.filter)
	{
	case api::texture_filter::min_mag_mip_point:
		min_filter = GL_NEAREST_MIPMAP_NEAREST;
		mag_filter = GL_NEAREST;
		break;
	case api::texture_filter::min_mag_point_mip_linear:
		min_filter = GL_NEAREST_MIPMAP_LINEAR;
		mag_filter = GL_NEAREST;
		break;
	case api::texture_filter::min_point_mag_linear_mip_point:
		min_filter = GL_NEAREST_MIPMAP_NEAREST;
		mag_filter = GL_LINEAR;
		break;
	case api::texture_filter::min_point_mag_mip_linear:
		min_filter = GL_NEAREST_MIPMAP_LINEAR;
		mag_filter = GL_LINEAR;
		break;
	case api::texture_filter::min_linear_mag_mip_point:
		min_filter = GL_LINEAR_MIPMAP_NEAREST;
		mag_filter = GL_NEAREST;
		break;
	case api::texture_filter::min_linear_mag_point_mip_linear:
		min_filter = GL_LINEAR_MIPMAP_LINEAR;
		mag_filter = GL_NEAREST;
		break;
	case api::texture_filter::min_mag_linear_mip_point:
		min_filter = GL_LINEAR_MIPMAP_NEAREST;
		mag_filter = GL_LINEAR;
		break;
	case api::texture_filter::anisotropic:
		glSamplerParameterf(object, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */, desc.max_anisotropy);
		// fall through
	case api::texture_filter::min_mag_mip_linear:
		min_filter = GL_LINEAR_MIPMAP_LINEAR;
		mag_filter = GL_LINEAR;
		break;
	}

	const auto convert_address_mode = [](api::texture_address_mode value) {
		switch (value)
		{
		default:
			return GL_NONE;
		case api::texture_address_mode::wrap:
			return GL_REPEAT;
		case api::texture_address_mode::mirror:
			return GL_MIRRORED_REPEAT;
		case api::texture_address_mode::clamp:
			return GL_CLAMP_TO_EDGE;
		case api::texture_address_mode::border:
			return GL_CLAMP_TO_BORDER;
		}
	};

	glSamplerParameteri(object, GL_TEXTURE_MIN_FILTER, min_filter);
	glSamplerParameteri(object, GL_TEXTURE_MAG_FILTER, mag_filter);
	glSamplerParameteri(object, GL_TEXTURE_WRAP_S, convert_address_mode(desc.address_u));
	glSamplerParameteri(object, GL_TEXTURE_WRAP_T, convert_address_mode(desc.address_v));
	glSamplerParameteri(object, GL_TEXTURE_WRAP_R, convert_address_mode(desc.address_w));
	glSamplerParameterf(object, GL_TEXTURE_LOD_BIAS, desc.mip_lod_bias);
	glSamplerParameterf(object, GL_TEXTURE_MIN_LOD, desc.min_lod);
	glSamplerParameterf(object, GL_TEXTURE_MAX_LOD, desc.max_lod);

	*out = { (static_cast<uint64_t>(GL_SAMPLER) << 40) | object };
	return true;
}
bool reshade::opengl::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out)
{
	if (initial_data != nullptr)
		return false;

	GLenum target = GL_NONE;
	switch (desc.type)
	{
	default:
		return false;
	case api::resource_type::buffer:
		     if ((desc.usage & api::resource_usage::index_buffer) != 0)
			target = GL_ELEMENT_ARRAY_BUFFER;
		else if ((desc.usage & api::resource_usage::vertex_buffer) != 0)
			target = GL_ARRAY_BUFFER;
		else if ((desc.usage & api::resource_usage::constant_buffer) != 0)
			target = GL_UNIFORM_BUFFER;
		break;
	case api::resource_type::texture_1d:
		target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_1D_ARRAY : GL_TEXTURE_1D;
		break;
	case api::resource_type::texture_2d:
		target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		break;
	case api::resource_type::texture_3d:
		target = GL_TEXTURE_3D;
		break;
	}

	GLuint object = 0;
	GLuint prev_object = 0;
	glGetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_object));

	if (desc.type == api::resource_type::buffer)
	{
		glGenBuffers(1, &object);
		glBindBuffer(target, object);

		GLbitfield usage_flags = GL_NONE;
		convert_memory_heap_to_flags(desc.heap, usage_flags);

		assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
		glBufferStorage(target, static_cast<GLsizeiptr>(desc.buffer.size), nullptr, usage_flags);

		glBindBuffer(target, prev_object);
	}
	else
	{
		GLenum internal_format = GL_NONE;
		convert_format_to_internal_format(desc.texture.format, internal_format);
		if (internal_format == GL_NONE)
			return false;

		glGenTextures(1, &object);
		glBindTexture(target, object);

		switch (target)
		{
		case GL_TEXTURE_1D:
			glTexStorage1D(target, desc.texture.levels, internal_format, desc.texture.width);
			break;
		case GL_TEXTURE_1D_ARRAY:
			glTexStorage2D(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.depth_or_layers);
			break;
		case GL_TEXTURE_2D:
			glTexStorage2D(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height);
			break;
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_3D:
			glTexStorage3D(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height, desc.texture.depth_or_layers);
			break;
		}

		glBindTexture(target, prev_object);
	}

	*out = make_resource_handle(target, object);
	return true;
}
bool reshade::opengl::device_impl::create_resource_view(api::resource resource, api::resource_usage, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);

	GLenum target = GL_NONE;
	switch (desc.type)
	{
	default:
		return false;
	case api::resource_view_type::buffer:
		target = GL_TEXTURE_BUFFER;
		break;
	case api::resource_view_type::texture_1d:
		target = GL_TEXTURE_1D;
		break;
	case api::resource_view_type::texture_1d_array:
		target = GL_TEXTURE_1D_ARRAY;
		break;
	case api::resource_view_type::texture_2d:
		target = GL_TEXTURE_2D;
		break;
	case api::resource_view_type::texture_2d_array:
		target = GL_TEXTURE_2D_ARRAY;
		break;
	case api::resource_view_type::texture_2d_multisample:
		target = GL_TEXTURE_2D_MULTISAMPLE;
		break;
	case api::resource_view_type::texture_2d_multisample_array:
		target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
		break;
	case api::resource_view_type::texture_3d:
		target = GL_TEXTURE_3D;
		break;
	case api::resource_view_type::texture_cube:
		target = GL_TEXTURE_CUBE_MAP;
		break;
	case api::resource_view_type::texture_cube_array:
		target = GL_TEXTURE_CUBE_MAP_ARRAY;
		break;
	}

	GLenum internal_format = GL_NONE;
	convert_format_to_internal_format(desc.format, internal_format);
	if (internal_format == GL_NONE)
		return false;

	if (target == (resource.handle >> 40) &&
		desc.texture.first_level == 0 && desc.texture.first_layer == 0 &&
		static_cast<GLenum>(get_tex_level_param(target, resource.handle & 0xFFFFFFFF, 0, GL_TEXTURE_INTERNAL_FORMAT)) == internal_format)
	{
		assert(target != GL_TEXTURE_BUFFER);

		// No need to create a view, so use resource directly, but set a bit so to not destroy it twice via 'destroy_resource_view'
		*out = { resource.handle | 0x100000000 };
		return true;
	}
	else
	{
		GLuint object = 0;
		GLuint prev_object = 0;
		glGenTextures(1, &object);

		if (target != GL_TEXTURE_BUFFER)
		{
			// Number of levels and layers are clamped to those of the original texture
			glTextureView(object, target, resource.handle & 0xFFFFFFFF, internal_format, desc.texture.first_level, desc.texture.levels, desc.texture.first_layer, desc.texture.layers);
		}
		else
		{
			glGetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_object));

			glBindTexture(target, object);

			if (desc.buffer.offset == 0 && desc.buffer.size == static_cast<uint64_t>(-1))
			{
				glTexBuffer(target, internal_format, resource.handle & 0xFFFFFFFF);
			}
			else
			{
				assert(desc.buffer.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
				assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
				glTexBufferRange(target, internal_format, resource.handle & 0xFFFFFFFF, static_cast<GLintptr>(desc.buffer.offset), static_cast<GLsizeiptr>(desc.buffer.size));
			}

			glBindTexture(target, prev_object);
		}

		*out = make_resource_view_handle(target, object);
		return true;
	}
}

bool reshade::opengl::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_type::compute:
		return create_pipeline_compute(desc, out);
	case api::pipeline_type::graphics_all:
		return create_pipeline_graphics(desc, out);
	}
}
bool reshade::opengl::device_impl::create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out)
{
	const GLuint program = glCreateProgram();

	if (desc.compute.shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.compute.shader.handle));

	glLinkProgram(program);

	if (desc.compute.shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.compute.shader.handle));

	GLint status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (GL_FALSE == status)
	{
		glDeleteProgram(program);

		*out = { 0 };
		return false;
	}

	const auto state = new pipeline_compute_impl();
	state->program = program;

	*out = { reinterpret_cast<uintptr_t>(state) };
	return true;
}
bool reshade::opengl::device_impl::create_pipeline_graphics(const api::pipeline_desc &desc, api::pipeline *out)
{
	const GLuint program = glCreateProgram();

	if (desc.graphics.vertex_shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.graphics.vertex_shader.handle));
	if (desc.graphics.hull_shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.graphics.hull_shader.handle));
	if (desc.graphics.domain_shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.graphics.domain_shader.handle));
	if (desc.graphics.geometry_shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.graphics.geometry_shader.handle));
	if (desc.graphics.pixel_shader.handle != 0)
		glAttachShader(program, static_cast<GLuint>(desc.graphics.pixel_shader.handle));

	glLinkProgram(program);

	if (desc.graphics.vertex_shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.graphics.vertex_shader.handle));
	if (desc.graphics.hull_shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.graphics.hull_shader.handle));
	if (desc.graphics.domain_shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.graphics.domain_shader.handle));
	if (desc.graphics.geometry_shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.graphics.geometry_shader.handle));
	if (desc.graphics.pixel_shader.handle != 0)
		glDetachShader(program, static_cast<GLuint>(desc.graphics.pixel_shader.handle));

	GLint status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (GL_FALSE == status)
	{
		glDeleteProgram(program);

		*out = { 0 };
		return false;
	}

	const auto state = new pipeline_graphics_impl();
	state->program = program;

	{
		GLuint prev_vao = 0;
		glGenVertexArrays(1, &state->vao);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint *>(&prev_vao));

		glBindVertexArray(state->vao);

		for (uint32_t i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
		{
			const auto &element = desc.graphics.input_layout[i];

			glEnableVertexAttribArray(element.location);

			GLint attrib_size = 0;
			GLenum attrib_format = GL_NONE;
			GLboolean normalized = GL_FALSE;
			switch (element.format)
			{
			default:
				assert(false);
				glBindVertexArray(prev_vao);
				glDeleteProgram(program);
				glDeleteVertexArrays(1, &state->vao);
				delete state;
				*out = { 0 };
				return false;
			case api::format::r8g8b8a8_unorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r8g8b8a8_uint:
				attrib_size = 4;
				attrib_format = GL_UNSIGNED_BYTE;
				break;
			case api::format::b8g8r8a8_unorm:
				normalized = GL_TRUE;
				attrib_size = GL_BGRA;
				attrib_format = GL_UNSIGNED_BYTE;
				break;
			case api::format::r10g10b10a2_unorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r10g10b10a2_uint:
				attrib_size = 4;
				attrib_format = GL_UNSIGNED_INT_2_10_10_10_REV;
				break;
			case api::format::r16_unorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16_uint:
				attrib_size = 1;
				attrib_format = GL_UNSIGNED_SHORT;
				break;
			case api::format::r16_snorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16_sint:
				attrib_size = 1;
				attrib_format = GL_SHORT;
				break;
			case api::format::r16_float:
				attrib_size = 1;
				attrib_format = GL_HALF_FLOAT;
				break;
			case api::format::r16g16_unorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16g16_uint:
				attrib_size = 2;
				attrib_format = GL_UNSIGNED_SHORT;
				break;
			case api::format::r16g16_snorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16g16_sint:
				attrib_size = 2;
				attrib_format = GL_SHORT;
				break;
			case api::format::r16g16_float:
				attrib_size = 2;
				attrib_format = GL_HALF_FLOAT;
				break;
			case api::format::r16g16b16a16_unorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16g16b16a16_uint:
				attrib_size = 4;
				attrib_format = GL_UNSIGNED_SHORT;
				break;
			case api::format::r16g16b16a16_snorm:
				normalized = GL_TRUE;
				// fall through
			case api::format::r16g16b16a16_sint:
				attrib_size = 4;
				attrib_format = GL_SHORT;
				break;
			case api::format::r16g16b16a16_float:
				attrib_size = 4;
				attrib_format = GL_HALF_FLOAT;
				break;
			case api::format::r32_uint:
				attrib_size = 1;
				attrib_format = GL_UNSIGNED_INT;
				break;
			case api::format::r32_sint:
				attrib_size = 1;
				attrib_format = GL_INT;
				break;
			case api::format::r32_float:
				attrib_size = 1;
				attrib_format = GL_FLOAT;
				break;
			case api::format::r32g32_uint:
				attrib_size = 2;
				attrib_format = GL_UNSIGNED_INT;
				break;
			case api::format::r32g32_sint:
				attrib_size = 2;
				attrib_format = GL_INT;
				break;
			case api::format::r32g32_float:
				attrib_size = 2;
				attrib_format = GL_FLOAT;
				break;
			case api::format::r32g32b32_uint:
				attrib_size = 3;
				attrib_format = GL_UNSIGNED_INT;
				break;
			case api::format::r32g32b32_sint:
				attrib_size = 3;
				attrib_format = GL_INT;
				break;
			case api::format::r32g32b32_float:
				attrib_size = 3;
				attrib_format = GL_FLOAT;
				break;
			case api::format::r32g32b32a32_uint:
				attrib_size = 4;
				attrib_format = GL_UNSIGNED_INT;
				break;
			case api::format::r32g32b32a32_sint:
				attrib_size = 4;
				attrib_format = GL_INT;
				break;
			case api::format::r32g32b32a32_float:
				attrib_size = 4;
				attrib_format = GL_FLOAT;
				break;
			}

#if 1
			glVertexAttribFormat(element.location, attrib_size, attrib_format, normalized, element.offset);
			glVertexAttribBinding(element.location, element.buffer_binding);
#else
			glVertexAttribPointer(element.location, attrib_size, attrib_format, normalized, element.stride, reinterpret_cast<const void *>(static_cast<uintptr_t>(element.offset)));
#endif
			glVertexBindingDivisor(element.buffer_binding, element.instance_step_rate);
		}

		glBindVertexArray(prev_vao);
	}

	state->prim_mode = convert_primitive_topology(desc.graphics.rasterizer_state.topology);
	state->patch_vertices = state->prim_mode == GL_PATCHES ? static_cast<uint32_t>(desc.graphics.rasterizer_state.topology) - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) : 0; 
	state->front_face = desc.graphics.rasterizer_state.front_counter_clockwise ? GL_CCW : GL_CW;
	state->cull_mode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	state->polygon_mode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);

	state->blend_eq = convert_blend_op(desc.graphics.blend_state.color_blend_op[0]);
	state->blend_eq_alpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[0]);
	state->blend_src = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[0]);
	state->blend_dst = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[0]);
	state->blend_src_alpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[0]);
	state->blend_dst_alpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[0]);

	state->back_stencil_op_fail = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	state->back_stencil_op_depth_fail = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	state->back_stencil_op_pass = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	state->back_stencil_func = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	state->front_stencil_op_fail = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	state->front_stencil_op_depth_fail = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	state->front_stencil_op_pass = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	state->front_stencil_func = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);
	state->stencil_read_mask = desc.graphics.depth_stencil_state.stencil_read_mask;
	state->stencil_write_mask = desc.graphics.depth_stencil_state.stencil_write_mask;
	state->stencil_reference_value = static_cast<GLint>(desc.graphics.depth_stencil_state.stencil_reference_value);

	state->color_write_mask = desc.graphics.blend_state.render_target_write_mask[0];

	state->blend_enable = desc.graphics.blend_state.blend_enable[0];
	state->depth_test = desc.graphics.depth_stencil_state.depth_test;
	state->depth_write_mask = desc.graphics.depth_stencil_state.depth_write_mask;
	state->stencil_test = desc.graphics.depth_stencil_state.stencil_test;
	state->scissor_test = desc.graphics.rasterizer_state.scissor_test;
	state->multisample = desc.graphics.multisample_state.multisample;
	state->sample_alpha_to_coverage = desc.graphics.multisample_state.alpha_to_coverage;
	state->sample_mask = desc.graphics.multisample_state.sample_mask;

	*out = { reinterpret_cast<uintptr_t>(state) };
	return true;
}

bool reshade::opengl::device_impl::create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *data, size_t size, api::shader_module *out)
{
	GLuint shader_object = glCreateShader(convert_shader_type(type));

	if (format == api::shader_format::glsl)
	{
		assert(entry_point == nullptr || strcmp(entry_point, "main") == 0);

		const auto source = static_cast<const GLchar *>(data);
		const auto source_len = static_cast<GLint>(size);
		glShaderSource(shader_object, 1, &source, &source_len);
		glCompileShader(shader_object);
	}
	else if (format == api::shader_format::spirv)
	{
		assert(size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		glShaderBinary(1, &shader_object, GL_SPIR_V_BINARY, data, static_cast<GLsizei>(size));
		glSpecializeShader(shader_object, entry_point, 0, nullptr, nullptr);
	}

	GLint status = GL_FALSE;
	glGetShaderiv(shader_object, GL_COMPILE_STATUS, &status);
	if (GL_FALSE != status)
	{
		*out = { shader_object };
		return true;
	}
	else
	{
		glDeleteShader(shader_object);

		*out = { 0 };
		return false;
	}
}
bool reshade::opengl::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	if (num_table_layouts > 1 || num_constant_ranges > 1)
	{
		*out = { 0 };
		return false;
	}

	const auto layout = new pipeline_layout_impl();

	if (num_constant_ranges == 1)
	{
		assert(constant_ranges[0].offset == 0);

		GLuint prev_object = 0;
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, reinterpret_cast<GLint *>(&prev_object));

		GLuint push_constants = 0;
		glGenBuffers(1, &push_constants);

		glBindBuffer(GL_UNIFORM_BUFFER, push_constants);
		glBufferStorage(GL_UNIFORM_BUFFER, constant_ranges[0].count * 4, nullptr, GL_DYNAMIC_STORAGE_BIT);
		glBindBuffer(GL_UNIFORM_BUFFER, prev_object);

		layout->push_constants = push_constants;
		layout->push_constants_binding = constant_ranges[0].dx_shader_register;
	}
	else
	{
		layout->push_constants = 0;
		layout->push_constants_binding = std::numeric_limits<GLuint>::max();
	}

	*out = { reinterpret_cast<uintptr_t>(layout) };
	return true;
}
bool reshade::opengl::device_impl::create_descriptor_heap(uint32_t, uint32_t, const api::descriptor_heap_size *, api::descriptor_heap *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::opengl::device_impl::create_descriptor_table(api::descriptor_heap, api::descriptor_table_layout, api::descriptor_table *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::opengl::device_impl::create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out)
{
	*out = { 0 };
	return push_descriptors;
}

void reshade::opengl::device_impl::destroy_sampler(api::sampler handle)
{
	assert((handle.handle >> 40) == GL_SAMPLER);

	const GLuint object = handle.handle & 0xFFFFFFFF;
	glDeleteSamplers(1, &object);
}
void reshade::opengl::device_impl::destroy_resource(api::resource handle)
{
	const GLuint object = handle.handle & 0xFFFFFFFF;
	switch (handle.handle >> 40)
	{
	case GL_BUFFER:
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
		glDeleteBuffers(1, &object);
		break;
	case GL_TEXTURE:
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
		glDeleteTextures(1, &object);
		break;
	case GL_RENDERBUFFER:
		glDeleteRenderbuffers(1, &object);
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		assert(false); // It is not allowed to destroy the default frame buffer
		break;
	default:
		assert(object == 0);
		break;
	}
}
void reshade::opengl::device_impl::destroy_resource_view(api::resource_view handle)
{
	if ((handle.handle & 0x100000000) == 0)
		destroy_resource({ handle.handle });
}

void reshade::opengl::device_impl::destroy_pipeline(api::pipeline_type type, api::pipeline handle)
{
	switch (type)
	{
	case api::pipeline_type::compute:
		delete reinterpret_cast<pipeline_compute_impl *>(handle.handle);
		break;
	case api::pipeline_type::graphics_all:
		delete reinterpret_cast<pipeline_graphics_impl *>(handle.handle);
		break;
	}
}
void reshade::opengl::device_impl::destroy_shader_module(api::shader_module handle)
{
	glDeleteShader(static_cast<GLuint>(handle.handle));
}
void reshade::opengl::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}
void reshade::opengl::device_impl::destroy_descriptor_heap(api::descriptor_heap)
{
	assert(false);
}
void reshade::opengl::device_impl::destroy_descriptor_table_layout(api::descriptor_table_layout)
{
	assert(false);
}

void reshade::opengl::device_impl::update_descriptor_tables(uint32_t, const api::descriptor_update *)
{
	assert(false);
}

bool reshade::opengl::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
{
	GLenum map_access = 0;
	switch (access)
	{
	case api::map_access::read_only:
		map_access = GL_READ_ONLY;
		break;
	case api::map_access::write_only:
	case api::map_access::write_discard:
		map_access = GL_WRITE_ONLY;
		break;
	case api::map_access::read_write:
		map_access = GL_READ_WRITE;
		break;
	}

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
	{
	default:
		assert(false);
		*mapped_ptr = nullptr;
		break;
	case GL_BUFFER:
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
		assert(subresource == 0);
		if (gl3wProcs.gl.MapNamedBuffer != nullptr)
		{
			*mapped_ptr = glMapNamedBuffer(object, map_access);
		}
		else
		{
			GLint prev_object = 0;
			glGetIntegerv(get_binding_for_target(target), &prev_object);

			glBindBuffer(target, object);
			*mapped_ptr = glMapBuffer(target, map_access);
			glBindBuffer(target, prev_object);
		}
		break;
	}

	return *mapped_ptr != nullptr;
}
void reshade::opengl::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
	{
	default:
		assert(false);
		break;
	case GL_BUFFER:
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
		assert(subresource == 0);
		if (gl3wProcs.gl.UnmapNamedBuffer != nullptr)
		{
			glUnmapNamedBuffer(object);
		}
		else
		{
			GLint prev_object = 0;
			glGetIntegerv(get_binding_for_target(target), &prev_object);

			glBindBuffer(target, object);
			glUnmapBuffer(target);
			glBindBuffer(target, prev_object);
		}
		break;
	}
}

void reshade::opengl::device_impl::upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size)
{
	assert(false); // TODO
}
void reshade::opengl::device_impl::upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t depth_pitch)
{
	assert(false); // TODO
}

reshade::api::resource_view reshade::opengl::device_impl::get_depth_stencil_from_fbo(GLuint fbo) const
{
	if (fbo == 0 && _default_depth_format == GL_NONE)
		return { 0 }; // No default depth buffer exists

	// TODO: Try again with stencil attachment if there is no depth attachment
	const GLenum attachment = GL_DEPTH_ATTACHMENT;
	if (fbo != 0 && get_fbo_attachment_param(fbo, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) == GL_NONE)
		return { 0 }; // FBO does not have this attachment

	return make_resource_view_handle(attachment, fbo);
}
reshade::api::resource_view reshade::opengl::device_impl::get_render_target_from_fbo(GLuint fbo, GLuint drawbuffer) const
{
	const GLenum attachment = GL_COLOR_ATTACHMENT0 + drawbuffer;
	if (fbo != 0 && get_fbo_attachment_param(fbo, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) == GL_NONE)
		return { 0 }; // FBO does not have this attachment

	return make_resource_view_handle(attachment, fbo);
}

void reshade::opengl::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);

	const GLenum attachment = view.handle >> 40;
	if ((attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT31) || attachment == GL_DEPTH_ATTACHMENT || attachment == GL_STENCIL_ATTACHMENT)
	{
		GLenum target = GL_FRAMEBUFFER_DEFAULT;
		GLuint object = attachment;

		// Zero is valid too, in which case the default frame buffer is referenced, instead of a FBO
		const GLuint fbo = view.handle & 0xFFFFFFFF;
		if (fbo != 0)
		{
			target = get_fbo_attachment_param(fbo, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
			assert(target != GL_NONE);
			object = get_fbo_attachment_param(fbo, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
			if (target == GL_TEXTURE)
				target  = get_tex_param(target, object, GL_TEXTURE_TARGET);
		}

		*out_resource = make_resource_handle(target, object);
	}
	else
	{
		*out_resource = { view.handle };
	}
}

reshade::api::resource_desc reshade::opengl::device_impl::get_resource_desc(api::resource resource) const
{
	GLsizei width = 0, height = 1, depth = 1, levels = 1, samples = 1, buffer_size = 0; GLenum internal_format = GL_NONE;

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
	{
	default:
		assert(false);
		break;
	case GL_BUFFER:
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
		buffer_size = get_buf_param(target, object, GL_BUFFER_SIZE);
		break;
	case GL_TEXTURE:
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
		width = get_tex_level_param(target, object, 0, GL_TEXTURE_WIDTH);
		height = get_tex_level_param(target, object, 0, GL_TEXTURE_HEIGHT);
		depth = get_tex_level_param(target, object, 0, GL_TEXTURE_DEPTH);
		internal_format = get_tex_level_param(target, object, 0, GL_TEXTURE_INTERNAL_FORMAT);
		if (get_tex_param(target, object, GL_TEXTURE_IMMUTABLE_FORMAT))
			levels = get_tex_param(target, object, GL_TEXTURE_IMMUTABLE_LEVELS);
		samples = get_tex_level_param(target, object, 0, GL_TEXTURE_SAMPLES);
		break;
	case GL_RENDERBUFFER:
		width = get_rbo_param(object, GL_RENDERBUFFER_WIDTH);
		height = get_rbo_param(object, GL_RENDERBUFFER_HEIGHT);
		internal_format = get_rbo_param(object, GL_RENDERBUFFER_INTERNAL_FORMAT);
		samples = get_rbo_param(object, GL_RENDERBUFFER_SAMPLES);
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		width = _default_fbo_width;
		height = _default_fbo_height;
		internal_format = (object == GL_DEPTH_ATTACHMENT) ? _default_depth_format : _default_color_format;
		break;
	}

	if (buffer_size != 0)
		return convert_resource_desc(target, buffer_size, api::memory_heap::unknown);
	else
		return convert_resource_desc(target, levels, samples, internal_format, width, height, depth);
}

void reshade::opengl::device_impl::wait_idle() const
{
	glFinish();
}

void reshade::opengl::device_impl::set_debug_name(api::resource resource, const char *name)
{
	GLenum id = resource.handle >> 40;
	switch (id)
	{
	case GL_BUFFER:
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
		id = GL_BUFFER;
		break;
	case GL_TEXTURE:
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
		id = GL_TEXTURE;
		break;
	}

	glObjectLabel(id, resource.handle & 0xFFFFFFFF, -1, name);
}

void reshade::opengl::device_impl::flush_immediate_command_list() const
{
	glFlush();
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

void reshade::opengl::device_impl::bind_pipeline(api::pipeline_type type, api::pipeline pipeline)
{
#define glEnableOrDisable(cap, enable) \
	if (enable) { \
		glEnable(cap); \
	} \
	else { \
		glDisable(cap); \
	}

	assert(pipeline.handle != 0);

	switch (type)
	{
	case api::pipeline_type::compute: {
		const auto state = reinterpret_cast<pipeline_compute_impl *>(pipeline.handle);

		glUseProgram(state->program);
		break;
	}
	case api::pipeline_type::graphics_all: {
		const auto state = reinterpret_cast<pipeline_graphics_impl *>(pipeline.handle);
		_current_prim_mode = state->prim_mode;

		if (state->prim_mode == GL_PATCHES)
		{
			glPatchParameteri(GL_PATCH_VERTICES, state->patch_vertices);
		}

		glUseProgram(state->program);
		glBindVertexArray(state->vao);

		glFrontFace(state->front_face);

		if (state->cull_mode != GL_NONE) {
			glEnable(GL_CULL_FACE);
			glCullFace(state->cull_mode);
		}
		else {
			glDisable(GL_CULL_FACE);
		}

		glPolygonMode(GL_FRONT_AND_BACK, state->polygon_mode);

		if (state->blend_enable) {
			glEnable(GL_BLEND);
			glBlendFuncSeparate(state->blend_src, state->blend_dst, state->blend_src_alpha, state->blend_dst_alpha);
			glBlendEquationSeparate(state->blend_eq, state->blend_eq_alpha);
		}
		else {
			glDisable(GL_BLEND);
		}

		glColorMask(
			(state->color_write_mask & (1 << 0)) != 0,
			(state->color_write_mask & (1 << 1)) != 0,
			(state->color_write_mask & (1 << 2)) != 0,
			(state->color_write_mask & (1 << 3)) != 0);

		if (state->depth_test) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}

		glDepthMask(state->depth_write_mask);

		if (state->stencil_test) {
			glEnable(GL_STENCIL_TEST);
			glStencilOpSeparate(GL_BACK, state->back_stencil_op_fail, state->back_stencil_op_depth_fail, state->back_stencil_op_pass);
			glStencilOpSeparate(GL_FRONT, state->front_stencil_op_fail, state->front_stencil_op_depth_fail, state->front_stencil_op_pass);
			glStencilMask(state->stencil_write_mask);
			glStencilFuncSeparate(GL_BACK, state->back_stencil_func, state->stencil_reference_value, state->stencil_read_mask);
			glStencilFuncSeparate(GL_FRONT, state->front_stencil_func, state->stencil_reference_value, state->stencil_read_mask);
		}
		else {
			glDisable(GL_STENCIL_TEST);
		}

		glEnableOrDisable(GL_SCISSOR_TEST, state->scissor_test);
		glEnableOrDisable(GL_MULTISAMPLE, state->multisample);
		glEnableOrDisable(GL_SAMPLE_ALPHA_TO_COVERAGE, state->sample_alpha_to_coverage);

		glSampleMaski(0, state->sample_mask);
		break;
	}
	}
}
void reshade::opengl::device_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (GLuint i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		default:
			assert(false);
			break;
		case api::pipeline_state::primitive_topology:
			_current_prim_mode = values[i];
			break;
		}
	}
}

void reshade::opengl::device_impl::push_constants(api::shader_stage, api::pipeline_layout layout, uint32_t, uint32_t first, uint32_t count, const uint32_t *values)
{
	const auto layout_impl = reinterpret_cast<pipeline_layout_impl *>(layout.handle);

	if (gl3wProcs.gl.NamedBufferSubData != nullptr)
	{
		glNamedBufferSubData(layout_impl->push_constants, first * 4, count * 4, values);
	}
	else
	{
		GLint prev_object = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &prev_object);

		glBindBuffer(GL_COPY_WRITE_BUFFER, layout_impl->push_constants);
		glBufferSubData(GL_COPY_WRITE_BUFFER, first * 4, count * 4, values);
		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_object);
	}

	glBindBufferBase(GL_UNIFORM_BUFFER, layout_impl->push_constants_binding, layout_impl->push_constants);
}
void reshade::opengl::device_impl::push_descriptors(api::shader_stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	assert(layout_index == 0); // There can only be a single descriptor set in OpenGL

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
			glBindSampler(i + first, descriptor.sampler.handle & 0xFFFFFFFF);
			glActiveTexture(GL_TEXTURE0 + i + first);
			glBindTexture(descriptor.view.handle >> 40, descriptor.view.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::shader_resource_view:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
			glActiveTexture(GL_TEXTURE0 + i + first);
			glBindTexture(descriptor.handle >> 40, descriptor.handle & 0xFFFFFFFF);
		}
		break;
	case api::descriptor_type::unordered_access_view:
		for (GLuint i = 0; i < count; ++i)
		{
			const auto &descriptor = static_cast<const api::resource_view *>(descriptors)[i];
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
void reshade::opengl::device_impl::bind_descriptor_heaps(uint32_t, const api::descriptor_heap *)
{
	assert(false);
}
void reshade::opengl::device_impl::bind_descriptor_tables(api::shader_stage, api::pipeline_layout, uint32_t, uint32_t, const api::descriptor_table *)
{
	assert(false);
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
			rects[k + 3] > rects[k + 1] ? rects[k + 3] - rects[k + 1] : rects[k + 1] - rects[k + 3]);
	}
}
void reshade::opengl::device_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (_main_fbo == 0)
	{
		glGenFramebuffers(1, &_main_fbo);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, _main_fbo);

	GLenum draw_buffers[8];

	for (GLuint i = 0; i < count; ++i)
	{
		draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;

		switch (rtvs[i].handle >> 40)
		{
		default:
			assert(false);
			return;
		case GL_TEXTURE:
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, rtvs[i].handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rtvs[i].handle & 0xFFFFFFFF);
			break;
		case GL_FRAMEBUFFER_DEFAULT:
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDrawBuffer(GL_BACK);
			return;
		}
	}

	glDrawBuffers(count, draw_buffers);

	if (dsv.handle != 0)
	{
		switch (dsv.handle >> 40)
		{
		default:
			assert(false);
			return;
		case GL_TEXTURE:
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, dsv.handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsv.handle & 0xFFFFFFFF);
			break;
		case GL_FRAMEBUFFER_DEFAULT:
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}
	}

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
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

void reshade::opengl::device_impl::blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter)
{
	assert(src.handle != 0 && dst.handle != 0);

	const api::resource_desc src_desc = get_resource_desc(src);
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	const api::resource_desc dst_desc = get_resource_desc(dst);
	const GLuint dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

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

	if (_copy_fbo[0] == 0 || _copy_fbo[1] == 0)
	{
		glGenFramebuffers(2, _copy_fbo);
	}

	GLint prev_read_fbo = 0;
	GLint prev_draw_fbo = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

	GLenum src_internal_format = GL_NONE;
	convert_format_to_internal_format(src_desc.texture.format, src_internal_format);
	const GLenum source_attachment = is_depth_stencil_format(src_internal_format, GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
	switch (src_target)
	{
	default:
		assert(false);
		return;
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
			glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, source_attachment, src_object, src_subresource % src_desc.texture.levels, src_subresource / src_desc.texture.levels);
		}
		else
		{
			assert((src_subresource % src_desc.texture.levels) == 0);
			glFramebufferTexture(GL_READ_FRAMEBUFFER, source_attachment, src_object, src_subresource);
		}
		assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		break;
	case GL_RENDERBUFFER:
		assert(src_subresource == 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _copy_fbo[0]);
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, source_attachment, GL_RENDERBUFFER, src_object);
		assert(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		assert(src_subresource == 0);
		assert(src_object == source_attachment);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		break;
	}

	GLenum dst_internal_format = GL_NONE;
	convert_format_to_internal_format(dst_desc.texture.format, dst_internal_format);
	const GLenum destination_attachment = is_depth_stencil_format(dst_internal_format, GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
	switch (dst_target)
	{
	default:
		assert(false);
		return;
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
			glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, destination_attachment, dst_object, dst_subresource % dst_desc.texture.levels, dst_subresource / dst_desc.texture.levels);
		}
		else
		{
			assert((dst_subresource % dst_desc.texture.levels) == 0);
			glFramebufferTexture(GL_READ_FRAMEBUFFER, destination_attachment, dst_object, dst_subresource);
		}
		assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		break;
	case GL_RENDERBUFFER:
		assert(dst_subresource == 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _copy_fbo[1]);
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, destination_attachment, GL_RENDERBUFFER, dst_object);
		assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		break;
	case GL_FRAMEBUFFER_DEFAULT:
		assert(dst_subresource == 0);
		assert(dst_object == destination_attachment);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		break;
	}

	GLenum stretch_filter = GL_NONE;
	switch (filter)
	{
	case api::texture_filter::min_mag_mip_point:
	case api::texture_filter::min_mag_point_mip_linear:
		stretch_filter = GL_NEAREST;
		break;
	case api::texture_filter::min_mag_mip_linear:
	case api::texture_filter::min_mag_linear_mip_point:
		stretch_filter = GL_LINEAR;
		break;
	}

	assert(src_region[2] == 0 && dst_region[2] == 0 && src_region[5] == 1 && dst_region[5] == 1);
	assert(source_attachment == destination_attachment);
	glBlitFramebuffer(
		src_region[0], src_region[1], src_region[3], src_region[4],
		dst_region[0], dst_region[4], dst_region[3], dst_region[1],
		source_attachment == GL_DEPTH_ATTACHMENT ? GL_DEPTH_BUFFER_BIT : GL_COLOR_BUFFER_BIT, stretch_filter);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
}
void reshade::opengl::device_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t)
{
	int32_t src_box[6] = {};
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, src_box);

	if (size != nullptr)
	{
		src_box[3] = src_box[0] + size[0];
		src_box[4] = src_box[1] + size[1];
		src_box[5] = src_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(src);
		src_box[3] = src_box[0] + std::max(1u, desc.texture.width >> (src_subresource % desc.texture.levels));
		src_box[4] = src_box[1] + std::max(1u, desc.texture.height >> (src_subresource % desc.texture.levels));
		src_box[5] = src_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> (src_subresource % desc.texture.levels)) : 1u);
	}

	int32_t dst_box[6] = {};
	if (dst_offset != nullptr)
		std::copy_n(dst_offset, 3, dst_box);

	if (size != nullptr)
	{
		dst_box[3] = dst_box[0] + size[0];
		dst_box[4] = dst_box[1] + size[1];
		dst_box[5] = dst_box[2] + size[2];
	}
	else
	{
		const api::resource_desc desc = get_resource_desc(dst);
		dst_box[3] = dst_box[0] + std::max(1u, desc.texture.width >> (dst_subresource % desc.texture.levels));
		dst_box[4] = dst_box[1] + std::max(1u, desc.texture.height >> (dst_subresource % desc.texture.levels));
		dst_box[5] = dst_box[2] + (desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(desc.texture.depth_or_layers) >> (dst_subresource % desc.texture.levels)) : 1u);
	}

	blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
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
		for (uint32_t layer = 0, layers = (desc.type != api::resource_type::texture_3d) ? desc.texture.depth_or_layers : 1; layer < layers; ++layer)
		{
			for (uint32_t level = 0; level < desc.texture.levels; ++level)
			{
				const uint32_t subresource = level + layer * desc.texture.levels;

				copy_texture_region(src, subresource, nullptr, dst, subresource, nullptr, nullptr);
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
void reshade::opengl::device_impl::copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6])
{
	assert(false);
}
void reshade::opengl::device_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	assert(src.handle != 0 && dst.handle != 0);

	const api::resource_desc src_desc = get_resource_desc(src);
	const GLenum src_target = src.handle >> 40;
	const GLuint src_object = src.handle & 0xFFFFFFFF;

	const api::resource_desc dst_desc = get_resource_desc(dst);
	const GLuint dst_target = dst.handle >> 40;
	const GLuint dst_object = dst.handle & 0xFFFFFFFF;

	if (src_target != GL_FRAMEBUFFER_DEFAULT && dst_target != GL_FRAMEBUFFER_DEFAULT)
	{
		int32_t cp_size[3];
		if (size != nullptr)
		{
			std::copy_n(size, 3, cp_size);
		}
		else
		{
			cp_size[0] = std::max(1u, src_desc.texture.width >> (src_subresource % src_desc.texture.levels));
			cp_size[1] = std::max(1u, src_desc.texture.height >> (src_subresource % src_desc.texture.levels));
			cp_size[2] = (src_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(src_desc.texture.depth_or_layers) >> (src_subresource % src_desc.texture.levels)) : 1u);
		}

		glCopyImageSubData(
			src_object, src_target, src_subresource % src_desc.texture.levels, src_offset != nullptr ? src_offset[0] : 0, src_offset != nullptr ? src_offset[1] : 0, (src_offset != nullptr ? src_offset[2] : 0) + (src_subresource / src_desc.texture.levels),
			dst_object, dst_target, dst_subresource % dst_desc.texture.levels, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0, (dst_offset != nullptr ? dst_offset[2] : 0) + (dst_subresource / src_desc.texture.levels),
			cp_size[0], cp_size[1], cp_size[2]);
		return;
	}
	else
	{
		int32_t src_box[6] = {};
		if (src_offset != nullptr)
			std::copy_n(src_offset, 3, src_box);

		if (size != nullptr)
		{
			src_box[3] = src_box[0] + size[0];
			src_box[4] = src_box[1] + size[1];
			src_box[5] = src_box[2] + size[2];
		}
		else
		{
			src_box[3] = src_box[0] + std::max(1u, src_desc.texture.width >> (src_subresource % src_desc.texture.levels));
			src_box[4] = src_box[1] + std::max(1u, src_desc.texture.height >> (src_subresource % src_desc.texture.levels));
			src_box[5] = src_box[2] + (src_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(src_desc.texture.depth_or_layers) >> (src_subresource % src_desc.texture.levels)) : 1u);
		}

		int32_t dst_box[6] = {};
		if (dst_offset != nullptr)
			std::copy_n(dst_offset, 3, dst_box);

		if (size != nullptr)
		{
			dst_box[3] = dst_box[0] + size[0];
			dst_box[4] = dst_box[1] + size[1];
			dst_box[5] = dst_box[2] + size[2];
		}
		else
		{
			dst_box[3] = dst_box[0] + std::max(1u, dst_desc.texture.width >> (dst_subresource % dst_desc.texture.levels));
			dst_box[4] = dst_box[1] + std::max(1u, dst_desc.texture.height >> (dst_subresource % dst_desc.texture.levels));
			dst_box[5] = dst_box[2] + (dst_desc.type == api::resource_type::texture_3d ? std::max(1u, static_cast<uint32_t>(dst_desc.texture.depth_or_layers) >> (dst_subresource % dst_desc.texture.levels)) : 1u);
		}

		blit(src, src_subresource, src_box, dst, dst_subresource, dst_box, api::texture_filter::min_mag_mip_point);
	}
}
void reshade::opengl::device_impl::copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::opengl::device_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	assert((dsv.handle >> 40) == GL_DEPTH_ATTACHMENT);
	const GLuint fbo = dsv.handle & 0xFFFFFFFF;
	GLint stencil_value = stencil;

	if (gl3wProcs.gl.ClearNamedFramebufferfv != nullptr)
	{
		switch (clear_flags)
		{
		case 0x1:
			glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &depth);
			break;
		case 0x2:
			glClearNamedFramebufferiv(fbo, GL_STENCIL, 0, &stencil_value);
			break;
		case 0x3:
			glClearNamedFramebufferfi(fbo, GL_DEPTH_STENCIL, 0, depth, stencil_value);
			break;
		}
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

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
}
void reshade::opengl::device_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	for (GLuint i = 0; i < count; ++i)
	{
		assert((rtvs[i].handle >> 40) >= GL_COLOR_ATTACHMENT0 && (rtvs[i].handle >> 40) <= GL_COLOR_ATTACHMENT31);
		const GLuint fbo = rtvs[i].handle & 0xFFFFFFFF;
		const GLuint drawbuffer = (rtvs[i].handle >> 40) - GL_COLOR_ATTACHMENT0;

		if (gl3wProcs.gl.ClearNamedFramebufferfv != nullptr)
		{
			glClearNamedFramebufferfv(fbo, GL_COLOR, drawbuffer, color);
		}
		else
		{
			GLint prev_binding = 0;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_binding);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glClearBufferfv(GL_COLOR, drawbuffer, color);
			glBindFramebuffer(GL_FRAMEBUFFER, prev_binding);
		}
	}
}
