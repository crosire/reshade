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
	struct query_heap_impl
	{
		~query_heap_impl()
		{
			glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
		}

		std::vector<GLuint> queries;
	};

	struct pipeline_layout_impl
	{
		std::vector<GLuint> bindings;
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

	switch (pfd.cRedBits)
	{
	default:
	case  0: _default_color_format = GL_NONE;
		break;
	case  8: _default_color_format = GL_RGBA8;
		break;
	case 10: _default_color_format = GL_RGB10_A2;
		break;
	case 16: _default_color_format = GL_RGBA16F;
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

	// Generate push constants buffer name
	glGenBuffers(1, &_push_constants);

	// Create mipmap generation program used in the 'generate_mipmaps' function
	{
		const GLchar *mipmap_shader[] = {
			"#version 430\n"
			"layout(binding = 0) uniform sampler2D src;\n"
			"layout(binding = 1) uniform writeonly image2D dest;\n"
			"layout(location = 0) uniform vec3 info;\n"
			"layout(local_size_x = 8, local_size_y = 8) in;\n"
			"void main()\n"
			"{\n"
			"	vec2 uv = info.xy * (vec2(gl_GlobalInvocationID.xy) + vec2(0.5));\n"
			"	imageStore(dest, ivec2(gl_GlobalInvocationID.xy), textureLod(src, uv, int(info.z)));\n"
			"}\n"
		};

		const GLuint mipmap_cs = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(mipmap_cs, 1, mipmap_shader, 0);
		glCompileShader(mipmap_cs);

		_mipmap_program = glCreateProgram();
		glAttachShader(_mipmap_program, mipmap_cs);
		glLinkProgram(_mipmap_program);
		glDeleteShader(mipmap_cs);
	}

#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<addon_event::init_device>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

#if RESHADE_ADDON
	// Communicate default state to add-ons
	const api::resource_view default_depth_stencil = get_depth_stencil_from_fbo(0);
	const api::resource_view default_render_target = get_render_target_from_fbo(0, 0);
	invoke_addon_event<addon_event::begin_render_pass>(this, 1, &default_render_target, default_depth_stencil);
#endif
}
reshade::opengl::device_impl::~device_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

#if RESHADE_ADDON
	addon::unload_addons();
#endif

	for (const auto &it : _framebuffer_list_internal)
		glDeleteFramebuffers(1, &it.second);

	// Destroy framebuffers used in 'copy_resource' implementation
	glDeleteFramebuffers(2, _copy_fbo);

	// Destroy mipmap generation program
	glDeleteProgram(_mipmap_program);

	// Destroy push constants buffer
	glDeleteBuffers(1, &_push_constants);
}

bool reshade::opengl::device_impl::check_capability(api::device_caps capability) const
{
	GLint value;

	switch (capability)
	{
	case api::device_caps::compute_shader:
		return true; // OpenGL 4.3
	case api::device_caps::geometry_shader:
		return true; // OpenGL 3.2
	case api::device_caps::tessellation_shaders:
		return true; // OpenGL 4.0
	case api::device_caps::dual_src_blend:
		return true; // OpenGL 3.3
	case api::device_caps::independent_blend:
		// TODO
		return false;
	case api::device_caps::logic_op:
		return true; // OpenGL 1.1
	case api::device_caps::draw_instanced:
		return true; // OpenGL 3.1
	case api::device_caps::draw_or_dispatch_indirect:
		return true; // OpenGL 4.0
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::multi_viewport:
		return true;
	case api::device_caps::sampler_anisotropy:
		glGetIntegerv(GL_TEXTURE_MAX_ANISOTROPY, &value);
		return value > 1;
	case api::device_caps::partial_push_constant_updates:
		return false;
	case api::device_caps::partial_push_descriptor_updates:
		return true;
	case api::device_caps::descriptor_sets:
		return false;
	case api::device_caps::sampler_with_resource_view:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
	case api::device_caps::copy_buffer_region:
		return true;
	case api::device_caps::copy_buffer_to_texture:
		return false;
	case api::device_caps::copy_query_results:
		return gl3wProcs.gl.GetQueryBufferObjectui64v != nullptr; // OpenGL 4.5
	default:
		return false;
	}
}
bool reshade::opengl::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	const GLenum internal_format = convert_format(format);
	if (internal_format == GL_NONE)
		return false;

	GLint supported = GL_FALSE;
	glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);

	GLint supported_depth = GL_TRUE;
	GLint supported_stencil = GL_TRUE;
	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_DEPTH_RENDERABLE, 1, &supported_depth);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_STENCIL_RENDERABLE, 1, &supported_stencil);
	}

	GLint supported_color_render = GL_TRUE;
	GLint supported_render_target = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_COLOR_RENDERABLE, 1, &supported_color_render);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_FRAMEBUFFER_RENDERABLE, 1, &supported_render_target);
	}

	GLint supported_unordered_access_load = GL_CAVEAT_SUPPORT;
	GLint supported_unordered_access_store = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_SHADER_IMAGE_LOAD, 1, &supported_unordered_access_load);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_SHADER_IMAGE_STORE, 1, &supported_unordered_access_store);
	}

	return supported && (supported_depth || supported_stencil) && (supported_color_render && supported_render_target) && (supported_unordered_access_load && supported_unordered_access_store);
}

bool reshade::opengl::device_impl::check_resource_handle_valid(api::resource handle) const
{
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
		return glIsBuffer(handle.handle & 0xFFFFFFFF) != GL_FALSE;
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
		return glIsTexture(handle.handle & 0xFFFFFFFF) != GL_FALSE;
	case GL_RENDERBUFFER:
		return glIsRenderbuffer(handle.handle & 0xFFFFFFFF) != GL_FALSE;
	case GL_FRAMEBUFFER_DEFAULT:
		return (handle.handle & 0xFFFFFFFF) != GL_DEPTH_ATTACHMENT || _default_depth_format != GL_NONE;
	default:
		return false;
	}
}
bool reshade::opengl::device_impl::check_resource_view_handle_valid(api::resource_view handle) const
{
	return check_resource_handle_valid({ handle.handle });
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
		[[fallthrough]];
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
	case api::resource_type::buffer:
		switch (desc.usage & (api::resource_usage::index_buffer | api::resource_usage::vertex_buffer | api::resource_usage::constant_buffer))
		{
		case api::resource_usage::index_buffer:
			target = GL_ELEMENT_ARRAY_BUFFER;
			break;
		case api::resource_usage::vertex_buffer:
			target = GL_ARRAY_BUFFER;
			break;
		case api::resource_usage::constant_buffer:
			target = GL_UNIFORM_BUFFER;
			break;
		default:
			assert(false);
			return false;
		}
		break;
	case api::resource_type::texture_1d:
		target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_1D_ARRAY : GL_TEXTURE_1D;
		break;
	case api::resource_type::texture_2d:
		if ((desc.flags & api::resource_flags::cube_compatible) != api::resource_flags::cube_compatible)
			target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		else
			target = desc.texture.depth_or_layers > 6 ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
		break;
	case api::resource_type::texture_3d:
		target = GL_TEXTURE_3D;
		break;
	default:
		assert(false);
		return false;
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
		const GLenum internal_format = convert_format(desc.texture.format);
		if (internal_format == GL_NONE)
			return false;

		glGenTextures(1, &object);
		glBindTexture(target, object);

		GLuint depth_or_layers = desc.texture.depth_or_layers;
		switch (target)
		{
		case GL_TEXTURE_1D:
			glTexStorage1D(target, desc.texture.levels, internal_format, desc.texture.width);
			break;
		case GL_TEXTURE_1D_ARRAY:
			glTexStorage2D(target, desc.texture.levels, internal_format, desc.texture.width, depth_or_layers);
			break;
		case GL_TEXTURE_CUBE_MAP:
			assert(depth_or_layers == 6);
			[[fallthrough]];
		case GL_TEXTURE_2D:
			glTexStorage2D(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height);
			break;
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			assert((depth_or_layers % 6) == 0);
			depth_or_layers /= 6;
			[[fallthrough]];
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_3D:
			glTexStorage3D(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height, depth_or_layers);
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
	default:
		assert(false);
		return false;
	}

	const GLenum internal_format = convert_format(desc.format);
	if (internal_format == GL_NONE)
		return false;

	if (target == (resource.handle >> 40) &&
		desc.texture.first_level == 0 && desc.texture.first_layer == 0 &&
		static_cast<GLenum>(get_tex_level_param(target, resource.handle & 0xFFFFFFFF, 0, GL_TEXTURE_INTERNAL_FORMAT)) == internal_format)
	{
		assert(target != GL_TEXTURE_BUFFER);

		// No need to create a view, so use resource directly, but set a bit so to not destroy it twice via 'destroy_resource_view'
		*out = make_resource_view_handle(target, resource.handle & 0xFFFFFFFF, 0x1);
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
	case api::pipeline_type::graphics:
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

bool reshade::opengl::device_impl::create_shader_module(api::shader_stage type, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out)
{
	GLuint shader_object = glCreateShader(convert_shader_type(type));

	if (format == api::shader_format::glsl)
	{
		assert(entry_point == nullptr || strcmp(entry_point, "main") == 0);

		const auto source = static_cast<const GLchar *>(code);
		const auto source_len = static_cast<GLint>(code_size);
		glShaderSource(shader_object, 1, &source, &source_len);
		glCompileShader(shader_object);
	}
	else if (format == api::shader_format::spirv)
	{
		assert(code_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		glShaderBinary(1, &shader_object, GL_SPIR_V_BINARY, code, static_cast<GLsizei>(code_size));
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
bool reshade::opengl::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_set_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	if (num_constant_ranges > 1)
	{
		*out = { 0 };
		return false;
	}

	const auto layout = new pipeline_layout_impl();
	layout->bindings.resize(num_table_layouts + num_constant_ranges);

	if (num_constant_ranges == 1)
	{
		assert(constant_ranges[0].offset == 0);
		layout->bindings[num_table_layouts] = constant_ranges[0].dx_shader_register;
	}

	*out = { reinterpret_cast<uintptr_t>(layout) };
	return true;
}
bool reshade::opengl::device_impl::create_descriptor_sets(api::descriptor_set_layout, uint32_t, api::descriptor_set *out)
{
	assert(false);

	*out = { 0 };
	return false;
}
bool reshade::opengl::device_impl::create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out)
{
	*out = { 0 };
	return push_descriptors;
}

bool reshade::opengl::device_impl::create_query_pool(api::query_type type, uint32_t count, api::query_pool *out)
{
	if (type == api::query_type::pipeline_statistics)
	{
		*out = { 0 };
		return false;
	}

	const auto result = new query_heap_impl();
	result->queries.resize(count);

	glGenQueries(static_cast<GLsizei>(count), result->queries.data());

	// Actually create and associate query objects with the names generated by 'glGenQueries' above
	for (GLuint i = 0; i < count; ++i)
	{
		if (type == api::query_type::timestamp)
		{
			glQueryCounter(result->queries[i], GL_TIMESTAMP);
		}
		else
		{
			const GLenum target = convert_query_type(type);
			glBeginQuery(target, result->queries[i]);
			glEndQuery(target);
		}
	}

	*out = { reinterpret_cast<uintptr_t>(result) };
	return true;
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
	case api::pipeline_type::graphics:
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
void reshade::opengl::device_impl::destroy_descriptor_sets(api::descriptor_set_layout, uint32_t count, const api::descriptor_set *sets)
{
	assert(count == 0 || sets[0].handle == 0);
}
void reshade::opengl::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout)
{
}

void reshade::opengl::device_impl::destroy_query_pool(api::query_pool handle)
{
	delete reinterpret_cast<query_heap_impl *>(handle.handle);
}

void reshade::opengl::device_impl::update_descriptor_sets(uint32_t, const api::descriptor_update *)
{
	assert(false);
}

bool reshade::opengl::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
{
	GLenum map_access = 0;
	switch (access)
	{
	case api::map_access::read_only:
		map_access = GL_MAP_READ_BIT;
		break;
	case api::map_access::read_write:
		map_access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
		break;
	case api::map_access::write_only:
		map_access = GL_MAP_WRITE_BIT;
		break;
	case api::map_access::write_discard:
		map_access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
		break;
	}

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
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
		assert(subresource == 0);
		if (gl3wProcs.gl.MapNamedBuffer != nullptr)
		{
			const GLuint length = get_buf_param(target, object, GL_BUFFER_SIZE);

			*mapped_ptr = glMapNamedBufferRange(object, 0, length, map_access);
		}
		else
		{
			const GLuint length = get_buf_param(target, object, GL_BUFFER_SIZE);

			GLint prev_object = 0;
			glGetIntegerv(get_binding_for_target(target), &prev_object);

			glBindBuffer(target, object);
			*mapped_ptr = glMapBufferRange(target, 0, length, map_access);
			glBindBuffer(target, prev_object);
		}
		break;
	default:
		assert(false);
		*mapped_ptr = nullptr;
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
	default:
		assert(false);
		break;
	}
}

void reshade::opengl::device_impl::upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

	const GLenum target = dst.handle >> 40;
	const GLuint object = dst.handle & 0xFFFFFFFF;

	// Get current state
	GLint previous_buf = 0;
	glGetIntegerv(get_binding_for_target(target), &previous_buf);

	// Bind and upload buffer data
	glBindBuffer(target, object);
	glBufferSubData(target, static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size), data);

	// Restore previous state from application
	glBindBuffer(target, previous_buf);

}
void reshade::opengl::device_impl::upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(dst.handle != 0);
	const GLenum target = dst.handle >> 40;
	const GLuint object = dst.handle & 0xFFFFFFFF;

	// Get current state
	GLint previous_tex = 0;
	GLint previous_unpack = 0;
	GLint previous_unpack_lsb = GL_FALSE;
	GLint previous_unpack_swap = GL_FALSE;
	GLint previous_unpack_alignment = 0;
	GLint previous_unpack_row_length = 0;
	GLint previous_unpack_image_height = 0;
	GLint previous_unpack_skip_rows = 0;
	GLint previous_unpack_skip_pixels = 0;
	GLint previous_unpack_skip_images = 0;
	glGetIntegerv(get_binding_for_target(target), &previous_tex);
	glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previous_unpack);
	glGetIntegerv(GL_UNPACK_LSB_FIRST, &previous_unpack_lsb);
	glGetIntegerv(GL_UNPACK_SWAP_BYTES, &previous_unpack_swap);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previous_unpack_row_length);
	glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &previous_unpack_image_height);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &previous_unpack_skip_rows);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &previous_unpack_skip_pixels);
	glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &previous_unpack_skip_images);

	// Unset any existing unpack buffer so pointer is not interpreted as an offset
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	// Clear pixel storage modes to defaults (texture uploads can break otherwise)
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // RGBA data is 4-byte aligned
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

	// Bind and upload texture data
	glBindTexture(target, object);

	GLint levels = 1;
	glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
	const GLuint level = dst_subresource % levels;
	const GLuint layer = dst_subresource / levels;

	GLint format = GL_NONE; GLenum type;
	glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, &format);
	format = convert_upload_format(format, type);

	GLint xoffset, yoffset, zoffset, width, height, depth;
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
		xoffset = 0;
		yoffset = 0;
		zoffset = 0;
		width   = get_tex_level_param(target, object, level, GL_TEXTURE_WIDTH);
		height  = get_tex_level_param(target, object, level, GL_TEXTURE_HEIGHT);
		depth   = get_tex_level_param(target, object, level, GL_TEXTURE_DEPTH);
	}

	switch (target)
	{
	case GL_TEXTURE_1D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS)
		{
			glTexSubImage1D(target, level, xoffset, width, format, type, data.data);
		}
		else
		{
			glCompressedTexSubImage1D(target, level, xoffset, width, format, data.slice_pitch, data.data);
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
			glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, data.data);
		}
		else
		{
			glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, data.slice_pitch * height, data.data);
		}
		break;
	case GL_TEXTURE_2D_ARRAY:
		zoffset += layer;
		[[fallthrough]];
	case GL_TEXTURE_3D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS)
		{
			glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data.data);
		}
		else
		{
			glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, data.slice_pitch * depth, data.data);
		}
		break;
	}

	// Restore previous state from application
	glBindTexture(target, previous_tex);
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

reshade::api::resource_view reshade::opengl::device_impl::get_depth_stencil_from_fbo(GLuint fbo) const
{
	// Zero is valid too, in which case the default frame buffer is referenced, instead of a FBO
	if (fbo == 0)
	{
		if (_default_depth_format == GL_NONE)
			return make_resource_view_handle(0, 0); // No default depth buffer exists
		else
			return make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
	}

	const GLenum attachments[3] = { GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };

	for (unsigned int attempt = 0; attempt < 3; ++attempt)
	{
		GLenum target = get_fbo_attachment_param(fbo, attachments[attempt], GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
		if (target == GL_NONE)
			continue;

		const GLenum object = get_fbo_attachment_param(fbo, attachments[attempt], GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
		if (target == GL_TEXTURE)
			target  = get_tex_param(target, object, GL_TEXTURE_TARGET);

		return make_resource_view_handle(target, object);
	}

	// FBO does not have this attachment
	return make_resource_view_handle(0, 0);
}
reshade::api::resource_view reshade::opengl::device_impl::get_render_target_from_fbo(GLuint fbo, GLuint drawbuffer) const
{
	if (fbo == 0)
		return make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_COLOR_ATTACHMENT0);

	GLenum target = get_fbo_attachment_param(fbo, GL_COLOR_ATTACHMENT0 + drawbuffer, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
	if (target == GL_NONE)
		return make_resource_view_handle(0, 0); // FBO does not have this attachment

	const GLenum object = get_fbo_attachment_param(fbo, GL_COLOR_ATTACHMENT0 + drawbuffer, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
	if (target == GL_TEXTURE)
		target  = get_tex_param(target, object, GL_TEXTURE_TARGET);

	return make_resource_view_handle(target, object);
}

void reshade::opengl::device_impl::request_framebuffer(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv, GLuint &fbo)
{
	if (count == 1 && (rtvs[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT)
	{
		// Can only use both the color and depth-stencil attachments of the default framebuffer together, not bind them individually
		assert(dsv.handle == 0 || (dsv.handle >> 40) == GL_FRAMEBUFFER_DEFAULT);
		fbo = 0;
		return;
	}

	size_t hash = 0xFFFFFFFF;
	for (uint32_t i = 0; i < count; ++i)
		hash ^= std::hash<uint64_t>()(rtvs[i].handle);
	if (dsv.handle != 0)
		hash ^= std::hash<uint64_t>()(dsv.handle);

	if (const auto it = _framebuffer_list_internal.find(hash);
		it != _framebuffer_list_internal.end())
	{
		fbo = it->second;
	}
	else 
	{
		glGenFramebuffers(1, &fbo);

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		for (GLuint i = 0; i < count; ++i)
		{
			switch (rtvs[i].handle >> 40)
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
				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, rtvs[i].handle & 0xFFFFFFFF, 0);
				break;
			case GL_RENDERBUFFER:
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rtvs[i].handle & 0xFFFFFFFF);
				break;
			default:
				assert(false);
				return;
			}
		}

		if (dsv.handle != 0)
		{
			switch (dsv.handle >> 40)
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
				glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, dsv.handle & 0xFFFFFFFF, 0);
				break;
			case GL_RENDERBUFFER:
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsv.handle & 0xFFFFFFFF);
				break;
			default:
				assert(false);
				return;
			}
		}

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		_framebuffer_list_internal.emplace(hash, fbo);
	}
}

void reshade::opengl::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);

	*out_resource = { view.handle };
}

reshade::api::resource_desc reshade::opengl::device_impl::get_resource_desc(api::resource resource) const
{
	GLsizei width = 0, height = 1, depth = 1, levels = 1, samples = 1, buffer_size = 0; GLenum internal_format = GL_NONE;

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
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
	default:
		assert(false);
		break;
	}

	if (buffer_size != 0)
		return convert_resource_desc(target, buffer_size, api::memory_heap::unknown);
	else
		return convert_resource_desc(target, levels, samples, internal_format, width, height, depth);
}

bool reshade::opengl::device_impl::get_query_results(api::query_pool heap, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(stride >= sizeof(uint64_t));

	const auto impl = reinterpret_cast<query_heap_impl *>(heap.handle);

	for (uint32_t i = 0; i < count; ++i)
	{
		GLuint available = GL_FALSE;
		glGetQueryObjectuiv(impl->queries[i + first], GL_QUERY_RESULT_AVAILABLE, &available);
		if (!available)
			return false;

		glGetQueryObjectui64v(impl->queries[i + first], GL_QUERY_RESULT, reinterpret_cast<GLuint64 *>(static_cast<uint8_t *>(results) + i * stride));
	}

	return true;
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
	case api::pipeline_type::graphics: {
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
			rects[k + 3] > rects[k + 1] ? rects[k + 3] - rects[k + 1] : rects[k + 1] - rects[k + 3]);
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
void reshade::opengl::device_impl::bind_descriptor_sets(api::pipeline_type, api::pipeline_layout, uint32_t, uint32_t, const api::descriptor_set *)
{
	assert(false);
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
void reshade::opengl::device_impl::end_render_pass()
{
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

	const GLenum source_attachment = is_depth_stencil_format(convert_format(src_desc.texture.format), GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
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
	default:
		assert(false);
		return;
	}

	const GLenum destination_attachment = is_depth_stencil_format(convert_format(dst_desc.texture.format), GL_DEPTH) ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
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
			glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, destination_attachment, dst_object, dst_subresource % dst_desc.texture.levels, dst_subresource / dst_desc.texture.levels);
		}
		else
		{
			assert((dst_subresource % dst_desc.texture.levels) == 0);
			glFramebufferTexture(GL_DRAW_FRAMEBUFFER, destination_attachment, dst_object, dst_subresource);
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
	default:
		assert(false);
		return;
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
void reshade::opengl::device_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format)
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

void reshade::opengl::device_impl::begin_query(api::query_pool heap, api::query_type type, uint32_t index)
{
	glBeginQuery(convert_query_type(type), reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index]);
}
void reshade::opengl::device_impl::end_query(api::query_pool heap, api::query_type type, uint32_t index)
{
	if (type == api::query_type::timestamp)
	{
		glQueryCounter(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[index], GL_TIMESTAMP);
	}
	else
	{
		glEndQuery(convert_query_type(type));
	}
}
void reshade::opengl::device_impl::copy_query_results(api::query_pool heap, api::query_type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
		glGetQueryBufferObjectui64v(reinterpret_cast<query_heap_impl *>(heap.handle)->queries[i + first], dst.handle & 0xFFFFFFFF, GL_QUERY_RESULT_NO_WAIT, static_cast<GLintptr>(dst_offset + i * stride));
	}
}

void reshade::opengl::device_impl::begin_debug_marker(const char *label, const float[4])
{
	glPushDebugGroup(GL_DEBUG_SOURCE_THIRD_PARTY, 0, -1, label);
}
void reshade::opengl::device_impl::end_debug_marker()
{
	glPopDebugGroup();
}
void reshade::opengl::device_impl::insert_debug_marker(const char *label, const float[4])
{
	glDebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
}
