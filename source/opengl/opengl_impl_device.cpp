/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "ini_file.hpp"
#include "opengl_impl_device.hpp"
#include "opengl_impl_type_convert.hpp"

reshade::opengl::device_impl::device_impl(HDC initial_hdc, HGLRC hglrc, bool compatibility_context) :
	api_object_impl(hglrc), _compatibility_context(compatibility_context)
{
	_hdcs.insert(initial_hdc);

	RECT window_rect = {};
	GetClientRect(WindowFromDC(initial_hdc), &window_rect);

	_default_fbo_width = window_rect.right - window_rect.left;
	_default_fbo_height = window_rect.bottom - window_rect.top;

	// The pixel format has to be the same for all device contexts used with this rendering context, so can cache information about it here
	// See https://docs.microsoft.com/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent
	PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
	DescribePixelFormat(initial_hdc, GetPixelFormat(initial_hdc), sizeof(pfd), &pfd);

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

	// Check whether this context supports Direct State Access
	_supports_dsa = gl3wIsSupported(4, 5);

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
	const auto debug_message_callback = [](unsigned int /*source*/, unsigned int type, unsigned int /*id*/, unsigned int /*severity*/, int /*length*/, const char *message, const void * /*userParam*/) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
	};

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debug_message_callback, nullptr);
#endif

	// Some games (like Hot Wheels Velocity X) use fixed texture names, which can clash with the ones ReShade generates below, since most implementations will return values linearly
	// Reserve a configurable range of texture names in old OpenGL games (which will use a compatibility context) to work around this
	auto num_reserve_texture_names = _compatibility_context ? 512u : 0u;
	reshade::global_config().get("APP", "ReserveTextureNames", num_reserve_texture_names);
	_reserved_texture_names.resize(num_reserve_texture_names);
	if (!_reserved_texture_names.empty())
		glGenTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());

	// Generate push constants buffer name
	glGenBuffers(1, &_push_constants);

	// Generate copy framebuffers
	glGenFramebuffers(3, _copy_fbo);

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
	load_addons();

	invoke_addon_event<addon_event::init_device>(this);

	{	api::pipeline_layout_param global_pipeline_layout_params[6];
		global_pipeline_layout_params[0].push_descriptors.type = api::descriptor_type::sampler_with_resource_view;
		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, reinterpret_cast<GLint *>(&global_pipeline_layout_params[0].push_descriptors.count));
		global_pipeline_layout_params[1].push_descriptors.type = api::descriptor_type::shader_storage_buffer;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, reinterpret_cast<GLint *>(&global_pipeline_layout_params[1].push_descriptors.count));
		global_pipeline_layout_params[2].push_descriptors.type = api::descriptor_type::constant_buffer;
		glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, reinterpret_cast<GLint *>(&global_pipeline_layout_params[2].push_descriptors.count));
		global_pipeline_layout_params[3].push_descriptors.type = api::descriptor_type::unordered_access_view;
		glGetIntegerv(GL_MAX_IMAGE_UNITS, reinterpret_cast<GLint *>(&global_pipeline_layout_params[3].push_descriptors.count));
		global_pipeline_layout_params[4].type = api::pipeline_layout_param_type::push_constants;
		global_pipeline_layout_params[4].push_constants.count = std::numeric_limits<uint32_t>::max();
		global_pipeline_layout_params[4].push_constants.visibility = api::shader_stage::all;
		global_pipeline_layout_params[5].type = api::pipeline_layout_param_type::push_constants;
		global_pipeline_layout_params[5].push_constants.count = std::numeric_limits<uint32_t>::max();
		global_pipeline_layout_params[5].push_constants.visibility = api::shader_stage::all;

		invoke_addon_event<addon_event::init_pipeline_layout>(this, 6, global_pipeline_layout_params, global_pipeline_layout);
	}

	invoke_addon_event<addon_event::init_command_list>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

	// Communicate default state to add-ons
	const api::resource_view default_rtv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
	const api::resource_view default_dsv = _default_depth_format != GL_NONE ? make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT) : make_resource_view_handle(0, 0);
	invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 1, &default_rtv, default_dsv);
#endif
}
reshade::opengl::device_impl::~device_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_command_list>(this);

	invoke_addon_event<addon_event::destroy_pipeline_layout>(this, global_pipeline_layout);

	invoke_addon_event<addon_event::destroy_device>(this);

	unload_addons();
#endif

	// Destroy mipmap generation program
	glDeleteProgram(_mipmap_program);

	// Destroy framebuffers used in 'copy_resource' implementation
	glDeleteFramebuffers(3, _copy_fbo);

	// Destroy push constants buffer
	glDeleteBuffers(1, &_push_constants);

	// Free range of reserved texture names
	glDeleteTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());
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
	case api::device_caps::hull_and_domain_shader:
		return true; // OpenGL 4.0
	case api::device_caps::logic_op:
		return true; // OpenGL 1.1
	case api::device_caps::dual_source_blend:
		return true; // OpenGL 3.3
	case api::device_caps::independent_blend:
		return true; // OpenGL 4.0
	case api::device_caps::fill_mode_non_solid:
		return true;
	case api::device_caps::conservative_rasterization:
		return false;
	case api::device_caps::bind_render_targets_and_depth_stencil:
	case api::device_caps::multi_viewport:
		return true;
	case api::device_caps::partial_push_constant_updates:
		return false;
	case api::device_caps::partial_push_descriptor_updates:
		return true;
	case api::device_caps::draw_instanced:
		return true; // OpenGL 3.1
	case api::device_caps::draw_or_dispatch_indirect:
		return true; // OpenGL 4.0
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return true;
	case api::device_caps::copy_query_pool_results:
		return gl3wProcs.gl.GetQueryBufferObjectui64v != nullptr; // OpenGL 4.5
	case api::device_caps::sampler_compare:
		return true;
	case api::device_caps::sampler_anisotropic:
		glGetIntegerv(GL_TEXTURE_MAX_ANISOTROPY, &value); // Core in OpenGL 4.6
		return value > 1;
	case api::device_caps::sampler_with_resource_view:
		return true;
	case api::device_caps::shared_resource:
	case api::device_caps::shared_resource_nt_handle:
		// TODO: Implement using 'GL_EXT_memory_object' and 'GL_EXT_memory_object_win32' extensions
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
	if ((usage & api::resource_usage::depth_stencil) != 0)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_DEPTH_RENDERABLE, 1, &supported_depth);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_STENCIL_RENDERABLE, 1, &supported_stencil);
	}

	GLint supported_color_render = GL_TRUE;
	GLint supported_render_target = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::render_target) != 0)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_COLOR_RENDERABLE, 1, &supported_color_render);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_FRAMEBUFFER_RENDERABLE, 1, &supported_render_target);
	}

	GLint supported_unordered_access_load = GL_CAVEAT_SUPPORT;
	GLint supported_unordered_access_store = GL_CAVEAT_SUPPORT;
	if ((usage & api::resource_usage::unordered_access) != 0)
	{
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_SHADER_IMAGE_LOAD, 1, &supported_unordered_access_load);
		glGetInternalformativ(GL_TEXTURE_2D, internal_format, GL_SHADER_IMAGE_STORE, 1, &supported_unordered_access_store);
	}

	return supported && (supported_depth || supported_stencil) && (supported_color_render && supported_render_target) && (supported_unordered_access_load && supported_unordered_access_store);
}

bool reshade::opengl::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out_handle)
{
	GLuint object = 0;
	glGenSamplers(1, &object);

	GLenum min_filter = GL_NONE;
	GLenum mag_filter = GL_NONE;
	switch (desc.filter)
	{
	case api::filter_mode::min_mag_mip_point:
	case api::filter_mode::compare_min_mag_mip_point:
		min_filter = GL_NEAREST_MIPMAP_NEAREST;
		mag_filter = GL_NEAREST;
		break;
	case api::filter_mode::min_mag_point_mip_linear:
	case api::filter_mode::compare_min_mag_point_mip_linear:
		min_filter = GL_NEAREST_MIPMAP_LINEAR;
		mag_filter = GL_NEAREST;
		break;
	case api::filter_mode::min_point_mag_linear_mip_point:
	case api::filter_mode::compare_min_point_mag_linear_mip_point:
		min_filter = GL_NEAREST_MIPMAP_NEAREST;
		mag_filter = GL_LINEAR;
		break;
	case api::filter_mode::min_point_mag_mip_linear:
	case api::filter_mode::compare_min_point_mag_mip_linear:
		min_filter = GL_NEAREST_MIPMAP_LINEAR;
		mag_filter = GL_LINEAR;
		break;
	case api::filter_mode::min_linear_mag_mip_point:
	case api::filter_mode::compare_min_linear_mag_mip_point:
		min_filter = GL_LINEAR_MIPMAP_NEAREST;
		mag_filter = GL_NEAREST;
		break;
	case api::filter_mode::min_linear_mag_point_mip_linear:
	case api::filter_mode::compare_min_linear_mag_point_mip_linear:
		min_filter = GL_LINEAR_MIPMAP_LINEAR;
		mag_filter = GL_NEAREST;
		break;
	case api::filter_mode::min_mag_linear_mip_point:
	case api::filter_mode::compare_min_mag_linear_mip_point:
		min_filter = GL_LINEAR_MIPMAP_NEAREST;
		mag_filter = GL_LINEAR;
		break;
	case api::filter_mode::anisotropic:
	case api::filter_mode::compare_anisotropic:
		glSamplerParameterf(object, GL_TEXTURE_MAX_ANISOTROPY, desc.max_anisotropy);
		[[fallthrough]];
	case api::filter_mode::min_mag_mip_linear:
	case api::filter_mode::compare_min_mag_mip_linear:
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
	glSamplerParameteri(object, GL_TEXTURE_COMPARE_MODE, (static_cast<uint32_t>(desc.filter) & 0x80) != 0 ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
	glSamplerParameteri(object, GL_TEXTURE_COMPARE_FUNC, convert_compare_op(desc.compare_op));
	glSamplerParameterf(object, GL_TEXTURE_MIN_LOD, desc.min_lod);
	glSamplerParameterf(object, GL_TEXTURE_MAX_LOD, desc.max_lod);

	glSamplerParameterfv(object, GL_TEXTURE_BORDER_COLOR, desc.border_color);

	*out_handle = { static_cast<uint64_t>(object) };
	return true;
}
void reshade::opengl::device_impl::destroy_sampler(api::sampler handle)
{
	const GLuint object = handle.handle & 0xFFFFFFFF;
	glDeleteSamplers(1, &object);
}

bool reshade::opengl::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out_handle, HANDLE *shared_handle)
{
	*out_handle = { 0 };

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
			target = GL_COPY_WRITE_BUFFER;
			if (desc.heap == api::memory_heap::gpu_to_cpu)
				target = GL_PIXEL_PACK_BUFFER;
			else if (desc.heap == api::memory_heap::cpu_to_gpu)
				target = GL_PIXEL_UNPACK_BUFFER;
			break;
		}
		break;
	case api::resource_type::texture_1d:
		target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_1D_ARRAY : GL_TEXTURE_1D;
		break;
	case api::resource_type::texture_2d:
		if ((desc.flags & api::resource_flags::cube_compatible) == 0)
			target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		else
			target = desc.texture.depth_or_layers > 6 ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
		break;
	case api::resource_type::texture_3d:
		target = GL_TEXTURE_3D;
		break;
	default:
		return false;
	}

#if 0
	GLenum shared_handle_type = GL_NONE;
	if ((desc.flags & api::resource_flags::shared) != 0)
	{
		// Only import is supported
		if (shared_handle == nullptr || *shared_handle == nullptr)
			return false;

		assert(initial_data == nullptr);

		if ((desc.flags & api::resource_flags::shared_nt_handle) != 0)
			shared_handle_type = GL_HANDLE_TYPE_OPAQUE_WIN32_EXT;
		else
			shared_handle_type = GL_HANDLE_TYPE_OPAQUE_WIN32_KMT_EXT;
	}
#else
	UNREFERENCED_PARAMETER(shared_handle);

	if ((desc.flags & api::resource_flags::shared) != 0)
		return false;
#endif

	GLuint object = 0;
	GLuint prev_binding = 0;
	glGetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_binding));

	// Clear any errors that may still be on the stack
	while (glGetError() != GL_NO_ERROR)
		continue;
	GLenum status = GL_NO_ERROR;

	if (desc.type == api::resource_type::buffer)
	{
		if (desc.buffer.size == 0)
			return false;

		glGenBuffers(1, &object);
		glBindBuffer(target, object);

		GLbitfield usage_flags = GL_NONE;
		convert_memory_heap_to_flags(desc, usage_flags);

		// Upload of initial data is using 'glBufferSubData', which requires the dynamic storage flag
		if (initial_data != nullptr)
			usage_flags |= GL_DYNAMIC_STORAGE_BIT;

		assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

#if 0
		if (shared_handle_type != GL_NONE)
		{
			GLuint mem = 0;
			glCreateMemoryObjectsEXT(1, &mem);
			glImportMemoryWin32HandleEXT(mem, desc.buffer.size, shared_handle_type, *shared_handle);

			glBufferStorageMemEXT(target, static_cast<GLsizeiptr>(desc.buffer.size), mem, 0);

			glDeleteMemoryObjectsEXT(1, &mem);
		}
		else
#endif
		{
			glBufferStorage(target, static_cast<GLsizeiptr>(desc.buffer.size), nullptr, usage_flags);
		}

		status = glGetError();

		if (initial_data != nullptr && status == GL_NO_ERROR)
		{
			update_buffer_region(initial_data->data, make_resource_handle(GL_BUFFER, object), 0, desc.buffer.size);
		}

		glBindBuffer(target, prev_binding);

		// Handles to buffer resources always have the target set to 'GL_BUFFER'
		target = GL_BUFFER;

		if (status != GL_NO_ERROR)
		{
			glDeleteBuffers(1, &object);
			return false;
		}
	}
	else
	{
		GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

		const GLenum internal_format = convert_format(desc.texture.format, swizzle_mask);
		if (desc.texture.width == 0 || internal_format == GL_NONE)
			return false;

		glGenTextures(1, &object);
		glBindTexture(target, object);

		GLuint depth_or_layers = desc.texture.depth_or_layers;

#if 0
		if (shared_handle_type != GL_NONE)
		{
			GLuint mem = 0;
			glCreateMemoryObjectsEXT(1, &mem);

			GLuint64 import_size = // TODO: Mipmap levels
				api::format_slice_pitch(desc.texture.format,
					api::format_row_pitch(desc.texture.format, desc.texture.width), desc.texture.height) * desc.texture.depth_or_layers;

			glImportMemoryWin32HandleEXT(mem, import_size, shared_handle_type, *shared_handle);

			switch (target)
			{
			case GL_TEXTURE_CUBE_MAP:
				assert(depth_or_layers == 6);
				[[fallthrough]];
			case GL_TEXTURE_1D:
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D:
				glTexStorageMem2DEXT(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height, mem, 0);
				break;
			case GL_TEXTURE_CUBE_MAP_ARRAY:
				assert((depth_or_layers % 6) == 0);
				depth_or_layers /= 6;
				[[fallthrough]];
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glTexStorageMem3DEXT(target, desc.texture.levels, internal_format, desc.texture.width, desc.texture.height, depth_or_layers, mem, 0);
				break;
			}

			glDeleteMemoryObjectsEXT(1, &mem);
		}
		else
#endif
		{
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
		}

		status = glGetError();

		glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

		if (initial_data != nullptr && status == GL_NO_ERROR)
		{
			for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
				update_texture_region(initial_data[subresource], make_resource_handle(target, object), subresource, nullptr);
		}

		glBindTexture(target, prev_binding);

		if (status != GL_NO_ERROR)
		{
			glDeleteTextures(1, &object);
			return false;
		}
	}

	*out_handle = make_resource_handle(target, object);
	return true;
}
void reshade::opengl::device_impl::destroy_resource(api::resource handle)
{
	const GLuint object = handle.handle & 0xFFFFFFFF;
	switch (handle.handle >> 40)
	{
	case GL_BUFFER:
		glDeleteBuffers(1, &object);
		break;
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

reshade::api::resource_desc reshade::opengl::device_impl::get_resource_desc(api::resource resource) const
{
	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
	{
		case GL_BUFFER:
		{
			GLsizeiptr size = 0;

			if (_supports_dsa)
			{
#ifndef WIN64
				glGetNamedBufferParameteriv(object, GL_BUFFER_SIZE, reinterpret_cast<GLint *>(&size));
#else
				glGetNamedBufferParameteri64v(object, GL_BUFFER_SIZE, &size);
#endif
			}
			else
			{
				GLint prev_binding = 0;
				glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &prev_binding);
				glBindBuffer(GL_COPY_READ_BUFFER, object);
#ifndef WIN64
				glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, reinterpret_cast<GLint *>(&size));
#else
				glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);
#endif
				glBindBuffer(GL_COPY_READ_BUFFER, prev_binding);
			}

			return convert_resource_desc(target, size);
		}
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
		{
			GLint width = 0, height = 1, depth = 1, levels = 1, samples = 1, internal_format = GL_NONE;
			GLint swizzle_mask[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

			if (_supports_dsa)
			{
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_WIDTH, &width);
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_HEIGHT, &height);
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_DEPTH, &depth);
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_SAMPLES, &samples);

				glGetTextureParameteriv(object, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
				if (levels == 0)
				{
					// If number of mipmap levels is not immutable, need to walk through the mipmap chain and check how many actually exist
					glGetTextureParameteriv(object, GL_TEXTURE_MAX_LEVEL, &levels);
					for (GLsizei level = 1, level_w = 0; level < levels; ++level)
					{
						// Check if this mipmap level does exist
						glGetTextureLevelParameteriv(object, level, GL_TEXTURE_WIDTH, &level_w);
						if (0 == level_w)
						{
							levels = level;
							break;
						}
					}
				}

				glGetTextureParameteriv(object, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);
			}
			else
			{
				GLint prev_binding = 0;
				glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
				glBindTexture(target, object);

				GLenum level_target = target;
				if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
					level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

				glGetTexLevelParameteriv(level_target, 0, GL_TEXTURE_WIDTH, &width);
				glGetTexLevelParameteriv(level_target, 0, GL_TEXTURE_HEIGHT, &height);
				glGetTexLevelParameteriv(level_target, 0, GL_TEXTURE_DEPTH, &depth);
				glGetTexLevelParameteriv(level_target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
				glGetTexLevelParameteriv(level_target, 0, GL_TEXTURE_SAMPLES, &samples);

				glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
				if (levels == 0)
				{
					glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, &levels);
					for (GLsizei level = 1, level_w = 0; level < levels; ++level)
					{
						glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_WIDTH, &level_w);
						if (0 == level_w)
						{
							levels = level;
							break;
						}
					}
				}

				glGetTextureParameteriv(object, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask);

				glBindTexture(target, prev_binding);
			}

			if (0 == levels)
				levels = 1;
			if (0 == samples)
				samples = 1;

			return convert_resource_desc(target, levels, samples, internal_format, width, height, depth, swizzle_mask);
		}
		case GL_RENDERBUFFER:
		{
			GLint width = 0, height = 1, samples = 1, internal_format = GL_NONE;

			if (_supports_dsa)
			{
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_WIDTH, &width);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_HEIGHT, &height);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_INTERNAL_FORMAT, &internal_format);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_SAMPLES, &samples);
			}
			else
			{
				GLint prev_binding = 0;
				glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_binding);
				glBindRenderbuffer(GL_RENDERBUFFER, object);

				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internal_format);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);

				glBindRenderbuffer(GL_RENDERBUFFER, prev_binding);
			}

			if (0 == samples)
				samples = 1;

			return convert_resource_desc(target, 1, samples, internal_format, width, height);
		}
		case GL_FRAMEBUFFER_DEFAULT:
		{
			const GLenum internal_format = (object == GL_DEPTH_STENCIL_ATTACHMENT || object == GL_DEPTH_ATTACHMENT || object == GL_STENCIL_ATTACHMENT) ? _default_depth_format : _default_color_format;

			return convert_resource_desc(target, 1, 1, internal_format, _default_fbo_width, _default_fbo_height);
		}
	}

	assert(false); // Not implemented
	return api::resource_desc {};
}

bool reshade::opengl::device_impl::create_resource_view(api::resource resource, api::resource_usage, const api::resource_view_desc &desc, api::resource_view *out_handle)
{
	*out_handle = { 0 };

	if (resource.handle == 0)
		return false;

	const bool is_srgb_format =
		desc.format != api::format_to_default_typed(desc.format, 0) &&
		desc.format == api::format_to_default_typed(desc.format, 1);

	const GLenum resource_target = resource.handle >> 40;
	if (resource_target == GL_RENDERBUFFER || resource_target == GL_FRAMEBUFFER_DEFAULT)
	{
		*out_handle = make_resource_view_handle(resource_target, resource.handle & 0xFFFFFFFF, is_srgb_format ? 0x2 : 0);
		return true;
	}

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
		return false;
	}

	GLenum resource_format = GL_NONE;
	if (_supports_dsa)
	{
		glGetTextureLevelParameteriv(resource.handle & 0xFFFFFFFF, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&resource_format));
	}
	else
	{
		GLuint prev_binding = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(resource_target), reinterpret_cast<GLint *>(&prev_binding));
		glBindTexture(resource_target, resource.handle & 0xFFFFFFFF);

		glGetTexLevelParameteriv(resource_target, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&resource_format));

		glBindTexture(resource_target, prev_binding);
	}

	GLint texture_swizzle[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };

	const GLenum internal_format = convert_format(desc.format, texture_swizzle);
	if (internal_format == GL_NONE)
		return false;

	if (target == resource_target &&
		desc.texture.first_level == 0 && desc.texture.first_layer == 0 && internal_format == resource_format)
	{
		assert(target != GL_TEXTURE_BUFFER);

		// No need to create a view, so use resource directly, but set a bit so to not destroy it twice via 'destroy_resource_view'
		*out_handle = make_resource_view_handle(target, resource.handle & 0xFFFFFFFF, is_srgb_format ? 0x2 : 0);
		return true;
	}
	else if (resource_target == GL_TEXTURE_CUBE_MAP && target == GL_TEXTURE_2D && desc.texture.layer_count == 1)
	{
		// Cube map face is handled via special target
		*out_handle = make_resource_view_handle(GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.texture.first_layer, resource.handle & 0xFFFFFFFF, is_srgb_format ? 0x2 : 0);
		return true;
	}

	GLuint object = 0;
	GLuint prev_binding = 0;
	glGetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_binding));

	// Clear any errors that may still be on the stack
	while (glGetError() != GL_NO_ERROR)
		continue;
	GLenum status = GL_NO_ERROR;

	glGenTextures(1, &object);

	if (target != GL_TEXTURE_BUFFER)
	{
		// Number of levels and layers are clamped to those of the original texture
		glTextureView(object, target, resource.handle & 0xFFFFFFFF, internal_format, desc.texture.first_level, desc.texture.level_count, desc.texture.first_layer, desc.texture.layer_count);

		glBindTexture(target, object);
		glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, texture_swizzle);
	}
	else
	{
		glBindTexture(target, object);

		if (desc.buffer.offset == 0 && desc.buffer.size == UINT64_MAX)
		{
			glTexBuffer(target, internal_format, resource.handle & 0xFFFFFFFF);
		}
		else
		{
			assert(desc.buffer.offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()));
			assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
			glTexBufferRange(target, internal_format, resource.handle & 0xFFFFFFFF, static_cast<GLintptr>(desc.buffer.offset), static_cast<GLsizeiptr>(desc.buffer.size));
		}
	}

	status = glGetError();

	glBindTexture(target, prev_binding);

	if (status != GL_NO_ERROR)
	{
		glDeleteTextures(1, &object);
		return false;
	}

	*out_handle = make_resource_view_handle(target, object, 0x1 | (is_srgb_format ? 0x2 : 0));
	return true;
}
void reshade::opengl::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (((handle.handle >> 32) & 0x1) != 0)
		destroy_resource({ handle.handle });
}

reshade::api::resource reshade::opengl::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);

	// Remove extra bits from view
	return { view.handle & 0xFFFFFF00FFFFFFFF };
}
reshade::api::resource_view_desc reshade::opengl::device_impl::get_resource_view_desc(api::resource_view view) const
{
	assert(view.handle != 0);

	const GLenum target = view.handle >> 40;
	const GLuint object = view.handle & 0xFFFFFFFF;

	if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		return api::resource_view_desc(get_resource_desc(get_resource_from_view(view)).texture.format, 0, UINT32_MAX, target - GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1);
	if (((view.handle >> 32) & 0x1) == 0)
		return api::resource_view_desc(get_resource_desc(get_resource_from_view(view)).texture.format, 0, UINT32_MAX, 0, UINT32_MAX);

	if (target != GL_TEXTURE_BUFFER)
	{
		GLint min_level = 0, min_layer = 0, num_levels = 0, num_layers = 0, internal_format = GL_NONE;

		if (_supports_dsa)
		{
			glGetTextureParameteriv(object, GL_TEXTURE_VIEW_MIN_LEVEL, &min_level);
			glGetTextureParameteriv(object, GL_TEXTURE_VIEW_MIN_LAYER, &min_layer);
			glGetTextureParameteriv(object, GL_TEXTURE_VIEW_NUM_LEVELS, &num_levels);
			glGetTextureParameteriv(object, GL_TEXTURE_VIEW_NUM_LAYERS, &num_layers);
			glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
		}
		else
		{
			GLint prev_binding = 0;
			glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
			glBindTexture(target, object);

			glGetTexParameteriv(target, GL_TEXTURE_VIEW_MIN_LEVEL, &min_level);
			glGetTexParameteriv(target, GL_TEXTURE_VIEW_MIN_LAYER, &min_layer);
			glGetTexParameteriv(target, GL_TEXTURE_VIEW_NUM_LEVELS, &num_levels);
			glGetTexParameteriv(target, GL_TEXTURE_VIEW_NUM_LAYERS, &num_layers);
			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

			glBindTexture(target, prev_binding);
		}

		return convert_resource_view_desc(target, internal_format, min_level, num_levels, min_layer, num_layers);
	}
	else
	{
		GLint offset = 0, size = 0, internal_format = GL_NONE;

		if (_supports_dsa)
		{
			glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_BUFFER_OFFSET, &offset);
			glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_BUFFER_SIZE, &size);
			glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
		}
		else
		{
			GLint prev_binding = 0;
			glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_binding);
			glBindTexture(target, object);

			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_BUFFER_OFFSET, &offset);
			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_BUFFER_SIZE, &size);
			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

			glBindTexture(target, prev_binding);
		}

		return convert_resource_view_desc(target, internal_format, offset, size);
	}
}

bool reshade::opengl::device_impl::map_buffer_region(api::resource resource, uint64_t offset, uint64_t size, api::map_access access, void **out_data)
{
	if (out_data == nullptr)
		return false;

	assert(resource.handle != 0 && (resource.handle >> 40) == GL_BUFFER);
	assert(offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && (size == UINT64_MAX || size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max())));

	const GLuint object = resource.handle & 0xFFFFFFFF;

	if (_supports_dsa)
	{
		if (size == UINT64_MAX)
		{
#ifndef WIN64
			GLint max_size = 0;
			glGetNamedBufferParameteriv(object, GL_BUFFER_SIZE, &max_size);
#else
			GLint64 max_size = 0;
			glGetNamedBufferParameteri64v(object, GL_BUFFER_SIZE, &max_size);
#endif
			size  = max_size;
		}

		*out_data = glMapNamedBufferRange(object, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), convert_access_flags(access));
	}
	else
	{
		GLint prev_object = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &prev_object);

		glBindBuffer(GL_COPY_WRITE_BUFFER, object);

		if (size == UINT64_MAX)
		{
#ifndef WIN64
			GLint max_size = 0;
			glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &max_size);
#else
			GLint64 max_size = 0;
			glGetBufferParameteri64v(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &max_size);
#endif
			size  = max_size;
		}

		*out_data = glMapBufferRange(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), convert_access_flags(access));

		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_object);
	}

	return *out_data != nullptr;
}
void reshade::opengl::device_impl::unmap_buffer_region(api::resource resource)
{
	assert(resource.handle != 0 && (resource.handle >> 40) == GL_BUFFER);

	const GLuint object = resource.handle & 0xFFFFFFFF;

	if (_supports_dsa)
	{
		glUnmapNamedBuffer(object);
	}
	else
	{
		GLint prev_object = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &prev_object);

		glBindBuffer(GL_COPY_WRITE_BUFFER, object);
		glUnmapBuffer(GL_COPY_WRITE_BUFFER);
		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_object);
	}
}
bool reshade::opengl::device_impl::map_texture_region(api::resource, uint32_t, const api::subresource_box *, api::map_access, api::subresource_data *out_data)
{
	if (out_data != nullptr)
	{
		out_data->data = nullptr;
		out_data->row_pitch = 0;
		out_data->slice_pitch = 0;
	}

	return false;
}
void reshade::opengl::device_impl::unmap_texture_region(api::resource, uint32_t)
{
}

void reshade::opengl::device_impl::update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size)
{
	assert(resource.handle != 0);
	assert(offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	if (_supports_dsa)
	{
		glNamedBufferSubData(object, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
	}
	else
	{
		GLint prev_binding = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &prev_binding);
		glBindBuffer(GL_COPY_WRITE_BUFFER, object);

		glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);

		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_binding);
	}
}
void reshade::opengl::device_impl::update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)
{
	assert(resource.handle != 0);

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	// Get current state
	GLint prev_binding = 0;
	glGetIntegerv(get_binding_for_target(target), &prev_binding);

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

	// Unset any existing unpack buffer so pointer is not interpreted as an offset
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	// Clear pixel storage modes to defaults (texture uploads can break otherwise)
	glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

	// Bind and upload texture data
	glBindTexture(target, object);

	GLint levels = 0;
	glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
	if (0 == levels)
		levels = 1;

	const GLuint level = subresource % levels;
	      GLuint layer = subresource / levels;

	GLenum level_target = target;
	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		const GLuint face = layer % 6;
		layer /= 6;
		level_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
	}

	GLenum format = GL_NONE, type;
	glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));

	GLuint xoffset, yoffset, zoffset, width, height, depth;
	if (box != nullptr)
	{
		xoffset = box->left;
		yoffset = box->top;
		zoffset = box->front;
		width   = box->right - box->left;
		height  = box->bottom - box->top;
		depth   = box->back - box->front;
	}
	else
	{
		xoffset = yoffset = zoffset = 0;
		glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_WIDTH,  reinterpret_cast<GLint *>(&width));
		glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&height));
		glGetTexLevelParameteriv(level_target, level, GL_TEXTURE_DEPTH,  reinterpret_cast<GLint *>(&depth));
	}

	const auto row_pitch = api::format_row_pitch(convert_format(format), width);
	const auto slice_pitch = api::format_slice_pitch(convert_format(format), row_pitch, height);
	const auto total_image_size = depth * static_cast<size_t>(slice_pitch);

	assert(total_image_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

	format = convert_upload_format(format, type);

	std::vector<uint8_t> temp_pixels;
	const uint8_t *pixels = static_cast<const uint8_t *>(data.data);

	if ((row_pitch != data.row_pitch && height == 1) ||
		(slice_pitch != data.slice_pitch && depth == 1))
	{
		temp_pixels.resize(total_image_size);
		uint8_t *dst_pixels = temp_pixels.data();

		for (size_t z = 0; z < depth; ++z)
			for (size_t y = 0; y < height; ++y, dst_pixels += row_pitch)
				std::memcpy(dst_pixels, pixels + z * data.slice_pitch + y * data.row_pitch, row_pitch);

		pixels = temp_pixels.data();
	}

	switch (level_target)
	{
	case GL_TEXTURE_1D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS)
		{
			glTexSubImage1D(level_target, level, xoffset, width, format, type, pixels);
		}
		else
		{
			glCompressedTexSubImage1D(level_target, level, xoffset, width, format, static_cast<GLsizei>(total_image_size), pixels);
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
			glTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, type, pixels);
		}
		else
		{
			glCompressedTexSubImage2D(level_target, level, xoffset, yoffset, width, height, format, static_cast<GLsizei>(total_image_size), pixels);
		}
		break;
	case GL_TEXTURE_2D_ARRAY:
		zoffset += layer;
		[[fallthrough]];
	case GL_TEXTURE_3D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS)
		{
			glTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
		}
		else
		{
			glCompressedTexSubImage3D(level_target, level, xoffset, yoffset, zoffset, width, height, depth, format, static_cast<GLsizei>(total_image_size), pixels);
		}
		break;
	}

	// Restore previous state from application
	glBindTexture(target, prev_binding);

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

reshade::api::resource_view reshade::opengl::device_impl::get_framebuffer_attachment(GLuint fbo_object, GLenum type, uint32_t index) const
{
	// Zero is valid too, in which case the default frame buffer is referenced, instead of a FBO
	if (fbo_object == 0)
	{
		if (type == GL_COLOR || type == GL_COLOR_BUFFER_BIT)
		{
			return make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
		}
		if (_default_depth_format != GL_NONE)
		{
			return make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
		}
		else
		{
			return make_resource_view_handle(0, 0); // No default depth buffer exists
		}
	}

	GLenum attachment;
	switch (type)
	{
	case GL_COLOR:
	case GL_COLOR_BUFFER_BIT:
		attachment = GL_COLOR_ATTACHMENT0 + index;
		break;
	case GL_DEPTH:
	case GL_DEPTH_BUFFER_BIT:
		attachment = GL_DEPTH_ATTACHMENT;
		break;
	case GL_STENCIL:
	case GL_STENCIL_BUFFER_BIT:
		attachment = GL_STENCIL_ATTACHMENT;
		break;
	case GL_DEPTH_STENCIL:
	case GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT:
		attachment = GL_DEPTH_STENCIL_ATTACHMENT;
		break;
	default:
		return make_resource_view_handle(0, 0);
	}

	GLenum target = GL_NONE, object = 0, format = GL_NONE;
	if (_supports_dsa)
	{
		glGetNamedFramebufferAttachmentParameteriv(fbo_object, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, reinterpret_cast<GLint *>(&target));

		// Check if FBO does have this attachment
		if (target != GL_NONE)
		{
			glGetNamedFramebufferAttachmentParameteriv(fbo_object, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, reinterpret_cast<GLint *>(&object));

			if (target == GL_TEXTURE)
			{
				// Get actual texture target from the texture object
				glGetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

				glGetTextureLevelParameteriv(object, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));
			}
			else if (target == GL_RENDERBUFFER)
			{
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&format));
			}
		}
	}
	else
	{
		GLint prev_object = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_object);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo_object);

		glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, reinterpret_cast<GLint *>(&target));

		if (target != GL_NONE)
		{
			glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, reinterpret_cast<GLint *>(&object));
		}

		glBindFramebuffer(GL_FRAMEBUFFER, prev_object);
	}

	const bool has_srgb_attachment = convert_format(format) != api::format_to_default_typed(convert_format(format), 0);

	// TODO: Create view based on 'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL', 'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE' and 'GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER'
	return make_resource_view_handle(target, object, (type == GL_COLOR || type == GL_COLOR_BUFFER_BIT) && has_srgb_attachment ? 0x2 : 0);
}

static bool create_shader_module(GLenum type, const reshade::api::shader_desc &desc, GLuint &shader_object)
{
	shader_object = 0;

	if (desc.code_size == 0)
		return false;

	shader_object = glCreateShader(type);

	if (!(desc.code_size > 4 && *static_cast<const uint32_t *>(desc.code) == 0x07230203)) // Check for SPIR-V magic number
	{
		assert(desc.entry_point == nullptr || strcmp(desc.entry_point, "main") == 0);
		assert(desc.spec_constants == 0);

		const auto source = static_cast<const GLchar *>(desc.code);
		const auto source_len = static_cast<GLint>(desc.code_size);
		glShaderSource(shader_object, 1, &source, &source_len);
		glCompileShader(shader_object);
	}
	else
	{
		assert(desc.code_size <= static_cast<size_t>(std::numeric_limits<GLsizei>::max()));

		glShaderBinary(1, &shader_object, GL_SPIR_V_BINARY, desc.code, static_cast<GLsizei>(desc.code_size));
		glSpecializeShader(shader_object, desc.entry_point, desc.spec_constants, desc.spec_constant_ids, desc.spec_constant_values);
	}

	GLint status = GL_FALSE;
	glGetShaderiv(shader_object, GL_COMPILE_STATUS, &status);
	if (GL_FALSE != status)
	{
		return true;
	}
	else
	{
		GLint log_size = 0;
		glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_size);

		if (0 != log_size)
		{
			std::vector<char> log(log_size);
			glGetShaderInfoLog(shader_object, log_size, nullptr, log.data());

			LOG(ERROR) << "Failed to compile GLSL shader:\n" << log.data();
		}

		glDeleteShader(shader_object);

		shader_object = 0;
		return false;
	}
}

bool reshade::opengl::device_impl::create_pipeline(const api::pipeline_desc &desc, uint32_t, const api::dynamic_state *, api::pipeline *out_handle)
{
	switch (desc.type)
	{
	case api::pipeline_stage::all_graphics:
		return create_graphics_pipeline(desc, out_handle);
	case api::pipeline_stage::vertex_shader:
		return create_shader_module(GL_VERTEX_SHADER, desc.graphics.vertex_shader, *reinterpret_cast<GLuint *>(out_handle));
	case api::pipeline_stage::hull_shader:
		return create_shader_module(GL_TESS_CONTROL_SHADER, desc.graphics.hull_shader, *reinterpret_cast<GLuint *>(out_handle));
	case api::pipeline_stage::domain_shader:
		return create_shader_module(GL_TESS_EVALUATION_SHADER, desc.graphics.domain_shader, *reinterpret_cast<GLuint *>(out_handle));
	case api::pipeline_stage::geometry_shader:
		return create_shader_module(GL_GEOMETRY_SHADER, desc.graphics.geometry_shader, *reinterpret_cast<GLuint *>(out_handle));
	case api::pipeline_stage::pixel_shader:
		return create_shader_module(GL_FRAGMENT_SHADER, desc.graphics.pixel_shader, *reinterpret_cast<GLuint *>(out_handle));
	case api::pipeline_stage::compute_shader:
		return create_compute_pipeline(desc, out_handle);
	default:
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::opengl::device_impl::create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
	*out_handle = { 0 };

	GLuint cs;
	const GLuint program = glCreateProgram();

	if (create_shader_module(GL_COMPUTE_SHADER, desc.compute.shader, cs))
		glAttachShader(program, cs);

	glLinkProgram(program);

	if (cs != 0)
		glDetachShader(program, cs);

	glDeleteShader(cs);

	GLint status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (GL_FALSE == status ||
		(desc.compute.shader.code_size != 0 && cs == 0))
	{
		GLint log_size = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);

		if (0 != log_size)
		{
			std::vector<char> log(log_size);
			glGetProgramInfoLog(program, log_size, nullptr, log.data());

			LOG(ERROR) << "Failed to link GLSL program:\n" << log.data();
		}

		glDeleteProgram(program);
		return false;
	}

	const auto state = new pipeline_impl();
	state->program = program;

	*out_handle = { reinterpret_cast<uintptr_t>(state) };
	return true;
}
bool reshade::opengl::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
	*out_handle = { 0 };

	if (desc.graphics.rasterizer_state.conservative_rasterization)
		return false;

	GLuint vs, hs, ds, gs, ps;
	const GLuint program = glCreateProgram();

	if (create_shader_module(GL_VERTEX_SHADER, desc.graphics.vertex_shader, vs))
		glAttachShader(program, vs);
	if (create_shader_module(GL_TESS_CONTROL_SHADER, desc.graphics.hull_shader, hs))
		glAttachShader(program, hs);
	if (create_shader_module(GL_TESS_EVALUATION_SHADER, desc.graphics.domain_shader, ds))
		glAttachShader(program, ds);
	if (create_shader_module(GL_GEOMETRY_SHADER, desc.graphics.geometry_shader, gs))
		glAttachShader(program, gs);
	if (create_shader_module(GL_FRAGMENT_SHADER, desc.graphics.pixel_shader, ps))
		glAttachShader(program, ps);

	glLinkProgram(program);

	if (vs != 0)
		glDetachShader(program, vs);
	if (hs != 0)
		glDetachShader(program, hs);
	if (ds != 0)
		glDetachShader(program, ds);
	if (gs != 0)
		glDetachShader(program, gs);
	if (ps != 0)
		glDetachShader(program, ps);

	glDeleteShader(vs);
	glDeleteShader(hs);
	glDeleteShader(ds);
	glDeleteShader(gs);
	glDeleteShader(ps);

	GLint status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (GL_FALSE == status ||
		(desc.graphics.vertex_shader.code_size != 0 && vs == 0) ||
		(desc.graphics.hull_shader.code_size != 0 && hs == 0) ||
		(desc.graphics.domain_shader.code_size != 0 && ds == 0) ||
		(desc.graphics.geometry_shader.code_size != 0 && gs == 0) ||
		(desc.graphics.pixel_shader.code_size != 0 && ps == 0))
	{
		GLint log_size = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);
		std::vector<char> log(log_size);
		glGetProgramInfoLog(program, log_size, nullptr, log.data());

		LOG(ERROR) << "Failed to link GLSL program: " << log.data();

		glDeleteProgram(program);
		return false;
	}

	const auto impl = new pipeline_impl();
	impl->program = program;

	{
		GLint prev_vao = 0;
		glGenVertexArrays(1, &impl->vao);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);

		glBindVertexArray(impl->vao);

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

	impl->sample_alpha_to_coverage = desc.graphics.blend_state.alpha_to_coverage_enable;
	impl->logic_op_enable = desc.graphics.blend_state.logic_op_enable[0]; // Logic operation applies to all attachments
	impl->logic_op = convert_logic_op(desc.graphics.blend_state.logic_op[0]);
	std::copy_n(desc.graphics.blend_state.blend_constant, 4, impl->blend_constant);
	for (int i = 0; i < 8; ++i)
	{
		impl->blend_enable[i] = desc.graphics.blend_state.blend_enable[i];
		impl->blend_src[i] = convert_blend_factor(desc.graphics.blend_state.source_color_blend_factor[i]);
		impl->blend_dst[i] = convert_blend_factor(desc.graphics.blend_state.dest_color_blend_factor[i]);
		impl->blend_src_alpha[i] = convert_blend_factor(desc.graphics.blend_state.source_alpha_blend_factor[i]);
		impl->blend_dst_alpha[i] = convert_blend_factor(desc.graphics.blend_state.dest_alpha_blend_factor[i]);
		impl->blend_eq[i] = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		impl->blend_eq_alpha[i] = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		impl->color_write_mask[i][0] = (desc.graphics.blend_state.render_target_write_mask[i] & (1 << 0)) != 0;
		impl->color_write_mask[i][1] = (desc.graphics.blend_state.render_target_write_mask[i] & (1 << 1)) != 0;
		impl->color_write_mask[i][2] = (desc.graphics.blend_state.render_target_write_mask[i] & (1 << 2)) != 0;
		impl->color_write_mask[i][3] = (desc.graphics.blend_state.render_target_write_mask[i] & (1 << 3)) != 0;
	}

	impl->polygon_mode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	impl->cull_mode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	impl->front_face = desc.graphics.rasterizer_state.front_counter_clockwise ? GL_CCW : GL_CW;
	impl->depth_clamp = !desc.graphics.rasterizer_state.depth_clip_enable;
	impl->scissor_test = desc.graphics.rasterizer_state.scissor_enable;
	impl->multisample_enable = desc.graphics.rasterizer_state.multisample_enable;
	impl->line_smooth_enable = desc.graphics.rasterizer_state.antialiased_line_enable;

	// Polygon offset is not currently implemented
	assert(desc.graphics.rasterizer_state.depth_bias == 0 && desc.graphics.rasterizer_state.depth_bias_clamp == 0 && desc.graphics.rasterizer_state.slope_scaled_depth_bias == 0);

	impl->depth_test = desc.graphics.depth_stencil_state.depth_enable;
	impl->depth_mask = desc.graphics.depth_stencil_state.depth_write_mask;
	impl->depth_func = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	impl->stencil_test = desc.graphics.depth_stencil_state.stencil_enable;
	impl->stencil_read_mask = desc.graphics.depth_stencil_state.stencil_read_mask;
	impl->stencil_write_mask = desc.graphics.depth_stencil_state.stencil_write_mask;
	impl->stencil_reference_value = static_cast<GLint>(desc.graphics.depth_stencil_state.stencil_reference_value);
	impl->front_stencil_op_fail = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	impl->front_stencil_op_depth_fail = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	impl->front_stencil_op_pass = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	impl->front_stencil_func = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);
	impl->back_stencil_op_fail = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	impl->back_stencil_op_depth_fail = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	impl->back_stencil_op_pass = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	impl->back_stencil_func = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);

	impl->sample_mask = desc.graphics.sample_mask;
	impl->prim_mode = convert_primitive_topology(desc.graphics.topology);
	impl->patch_vertices = impl->prim_mode == GL_PATCHES ? static_cast<uint32_t>(desc.graphics.topology) - static_cast<uint32_t>(api::primitive_topology::patch_list_01_cp) : 0;

	*out_handle = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::opengl::device_impl::destroy_pipeline(api::pipeline handle)
{
	if (handle.handle == 0)
		return;

	const auto impl = reinterpret_cast<pipeline_impl *>(handle.handle);

	glDeleteProgram(impl->program);
	glDeleteVertexArrays(1, &impl->vao);

	delete impl;
}

bool reshade::opengl::device_impl::create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle)
{
	*out_handle = { 0 };

	std::vector<api::descriptor_range> ranges(param_count);

	for (uint32_t i = 0; i < param_count; ++i)
	{
		api::descriptor_range &merged_range = ranges[i];

		switch (params[i].type)
		{
		case api::pipeline_layout_param_type::descriptor_set:
			if (params[i].descriptor_set.count == 0)
				return false;

			merged_range = params[i].descriptor_set.ranges[0];
			if (merged_range.array_size > 1)
				return false;

			for (uint32_t k = 1; k < params[i].descriptor_set.count; ++i)
			{
				const api::descriptor_range &range = params[i].descriptor_set.ranges[k];

				if (range.type != merged_range.type || range.array_size > 1 || range.dx_register_space != merged_range.dx_register_space)
					return false;

				if (range.binding >= merged_range.binding)
				{
					const uint32_t distance = range.binding - merged_range.binding;

					merged_range.count += distance;
					merged_range.visibility |= range.visibility;
				}
				else
				{
					const uint32_t distance = merged_range.binding - range.binding;

					merged_range.binding = range.binding;
					merged_range.dx_register_index = range.dx_register_index;
					merged_range.count += distance;
					merged_range.visibility |= range.visibility;
				}
			}
			break;
		case api::pipeline_layout_param_type::push_descriptors:
			merged_range = params[i].push_descriptors;
			break;
		case api::pipeline_layout_param_type::push_constants:
			merged_range.binding = params[i].push_constants.binding;
			break;
		}
	}

	const auto impl = new pipeline_layout_impl();
	impl->ranges = std::move(ranges);

	*out_handle = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::opengl::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	assert(handle != global_pipeline_layout);

	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}

bool reshade::opengl::device_impl::allocate_descriptor_sets(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_set *out_sets)
{
	const auto layout_impl = reinterpret_cast<const pipeline_layout_impl *>(layout.handle);

	if (layout_impl != nullptr)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			const auto impl = new descriptor_set_impl();
			impl->type = layout_impl->ranges[layout_param].type;
			impl->count = layout_impl->ranges[layout_param].count;

			switch (impl->type)
			{
			case api::descriptor_type::sampler:
			case api::descriptor_type::shader_resource_view:
			case api::descriptor_type::unordered_access_view:
				impl->descriptors.resize(impl->count * 1);
				break;
			case api::descriptor_type::sampler_with_resource_view:
				impl->descriptors.resize(impl->count * 2);
				break;
			case api::descriptor_type::constant_buffer:
			case api::descriptor_type::shader_storage_buffer:
				impl->descriptors.resize(impl->count * 3);
				break;
			default:
				assert(false);
				break;
			}

			out_sets[i] = { reinterpret_cast<uintptr_t>(impl) };
		}

		return true;
	}
	else
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			out_sets[i] = { 0 };
		}

		return false;
	}
}
void reshade::opengl::device_impl::free_descriptor_sets(uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}

void reshade::opengl::device_impl::get_descriptor_pool_offset(api::descriptor_set set, uint32_t binding, uint32_t array_offset, api::descriptor_pool *pool, uint32_t *offset) const
{
	assert(set.handle != 0 && array_offset == 0);

	*pool = { 0 }; // Not implemented
	*offset = binding;
}

void reshade::opengl::device_impl::copy_descriptor_sets(uint32_t count, const api::descriptor_set_copy *copies)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_set_copy &copy = copies[i];

		assert(copy.dest_array_offset == 0 && copy.source_array_offset == 0);

		const auto src_set_impl = reinterpret_cast<descriptor_set_impl *>(copy.source_set.handle);
		const auto dst_set_impl = reinterpret_cast<descriptor_set_impl *>(copy.dest_set.handle);

		assert(src_set_impl != nullptr && dst_set_impl != nullptr && src_set_impl->type == dst_set_impl->type);

		switch (src_set_impl->type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
		case api::descriptor_type::unordered_access_view:
			std::memcpy(&dst_set_impl->descriptors[copy.dest_binding * 1], &src_set_impl->descriptors[copy.source_binding * 1], copy.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::sampler_with_resource_view:
			std::memcpy(&dst_set_impl->descriptors[copy.dest_binding * 2], &src_set_impl->descriptors[copy.source_binding * 2], copy.count * sizeof(uint64_t) * 2);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&dst_set_impl->descriptors[copy.dest_binding * 3], &src_set_impl->descriptors[copy.source_binding * 3], copy.count * sizeof(uint64_t) * 3);
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::opengl::device_impl::update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_set_update &update = updates[i];

		const auto set_impl = reinterpret_cast<descriptor_set_impl *>(update.set.handle);

		assert(set_impl != nullptr && set_impl->type == update.type);

		// In GLSL targeting OpenGL, if the binding qualifier is used with an array, the first element of the array takes the specified block binding and each subsequent element takes the next consecutive binding point
		// See https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.html#layout-qualifiers (chapter 4.4.6)
		// Therefore it is not valid to specify an array offset for a binding
		assert(update.array_offset == 0);

		switch (update.type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
		case api::descriptor_type::unordered_access_view:
			std::memcpy(&set_impl->descriptors[update.binding * 1], update.descriptors, update.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::sampler_with_resource_view:
			std::memcpy(&set_impl->descriptors[update.binding * 2], update.descriptors, update.count * sizeof(uint64_t) * 2);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&set_impl->descriptors[update.binding * 3], update.descriptors, update.count * sizeof(uint64_t) * 3);
			break;
		default:
			assert(false);
			break;
		}
	}
}

bool reshade::opengl::device_impl::create_query_pool(api::query_type type, uint32_t size, api::query_pool *out_handle)
{
	*out_handle = { 0 };

	if (type == api::query_type::pipeline_statistics)
		return false;

	const auto impl = new query_pool_impl();
	impl->queries.resize(size);

	glGenQueries(static_cast<GLsizei>(size), impl->queries.data());

	const GLenum target = convert_query_type(type);

	// Actually create and associate query objects with the names generated by 'glGenQueries' above
	for (uint32_t i = 0; i < size; ++i)
	{
		if (type == api::query_type::timestamp)
		{
			glQueryCounter(impl->queries[i], GL_TIMESTAMP);
		}
		else
		{
			glBeginQuery(target, impl->queries[i]);
			glEndQuery(target);
		}
	}

	*out_handle = { reinterpret_cast<uintptr_t>(impl) };
	return true;
}
void reshade::opengl::device_impl::destroy_query_pool(api::query_pool handle)
{
	if (handle.handle == 0)
		return;

	const auto impl = reinterpret_cast<query_pool_impl *>(handle.handle);

	glDeleteQueries(static_cast<GLsizei>(impl->queries.size()), impl->queries.data());

	delete impl;
}

bool reshade::opengl::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	assert(stride >= sizeof(uint64_t));

	const auto impl = reinterpret_cast<query_pool_impl *>(pool.handle);

	for (size_t i = 0; i < count; ++i)
	{
		const GLuint query_object = impl->queries[first + i];

		GLuint available = GL_FALSE;
		glGetQueryObjectuiv(query_object, GL_QUERY_RESULT_AVAILABLE, &available);
		if (!available)
			return false;

		glGetQueryObjectui64v(query_object, GL_QUERY_RESULT, reinterpret_cast<GLuint64 *>(static_cast<uint8_t *>(results) + i * stride));
	}

	return true;
}

void reshade::opengl::device_impl::set_resource_name(api::resource handle, const char *name)
{
	glObjectLabel((handle.handle >> 40) == GL_BUFFER ? GL_BUFFER : GL_TEXTURE, handle.handle & 0xFFFFFFFF, -1, name);
}
void reshade::opengl::device_impl::set_resource_view_name(api::resource_view handle, const char *name)
{
	if (((handle.handle >> 32) & 0x1) == 0)
		return;

	glObjectLabel(GL_TEXTURE, handle.handle & 0xFFFFFFFF, -1, name);
}
