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
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](unsigned int /*source*/, unsigned int type, unsigned int /*id*/, unsigned int /*severity*/, int /*length*/, const char *message, const void */*userParam*/) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
	}, nullptr);
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
	glGenFramebuffers(2, _copy_fbo);

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
	invoke_addon_event<addon_event::init_command_list>(this);
	invoke_addon_event<addon_event::init_command_queue>(this);

	GLint max_image_units = 0;
	glGetIntegerv(GL_MAX_IMAGE_UNITS, &max_image_units);
	GLint max_texture_units = 0;
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units);
	GLint max_uniform_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_buffer_bindings);
	GLint max_shader_storage_buffer_bindings = 0;
	glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &max_shader_storage_buffer_bindings);

	// Create global pipeline layout that is used for all application descriptor events
	api::descriptor_range push_descriptors = {};
	push_descriptors.array_size = 1;
	api::pipeline_layout_param layout_params[6] = {};

	layout_params[4].type = api::pipeline_layout_param_type::push_constants;
	layout_params[4].push_constants.count = std::numeric_limits<uint32_t>::max();
	layout_params[4].push_constants.visibility = api::shader_stage::all;
	layout_params[5].type = api::pipeline_layout_param_type::push_constants;
	layout_params[5].push_constants.count = std::numeric_limits<uint32_t>::max();
	layout_params[5].push_constants.visibility = api::shader_stage::all;

	push_descriptors.type = api::descriptor_type::sampler_with_resource_view;
	push_descriptors.count = max_texture_units;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[0].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[0].descriptor_layout);
	push_descriptors.type = api::descriptor_type::unordered_access_view;
	push_descriptors.count = max_image_units;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[1].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[1].descriptor_layout);
	push_descriptors.type = api::descriptor_type::constant_buffer;
	push_descriptors.count = max_uniform_buffer_bindings;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[2].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[2].descriptor_layout);
	push_descriptors.type = api::descriptor_type::shader_storage_buffer;
	push_descriptors.count = max_shader_storage_buffer_bindings;
	push_descriptors.visibility = api::shader_stage::all;
	layout_params[3].type = api::pipeline_layout_param_type::push_descriptors;
	create_descriptor_set_layout(1, &push_descriptors, true, &layout_params[3].descriptor_layout);

	create_pipeline_layout(6, layout_params, &_global_pipeline_layout);

	// Communicate default state to add-ons
	invoke_addon_event<addon_event::begin_render_pass>(this, api::render_pass { 0 }, make_framebuffer_handle(0));
#endif
}
reshade::opengl::device_impl::~device_impl()
{
#if RESHADE_ADDON
	invoke_addon_event<addon_event::finish_render_pass>(this);

	const std::vector<api::pipeline_layout_param> &layout_params = reinterpret_cast<pipeline_layout_impl *>(_global_pipeline_layout.handle)->params;

	destroy_descriptor_set_layout(layout_params[0].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[1].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[2].descriptor_layout);
	destroy_descriptor_set_layout(layout_params[3].descriptor_layout);

	destroy_pipeline_layout(_global_pipeline_layout);

	invoke_addon_event<addon_event::destroy_command_queue>(this);
	invoke_addon_event<addon_event::destroy_command_list>(this);
	invoke_addon_event<addon_event::destroy_device>(this);

	unload_addons();
#endif

	// Destroy mipmap generation program
	glDeleteProgram(_mipmap_program);

	// Destroy framebuffers used in 'copy_resource' implementation
	glDeleteFramebuffers(2, _copy_fbo);

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
	case api::device_caps::dual_src_blend:
		return true; // OpenGL 3.3
	case api::device_caps::independent_blend:
		return true; // OpenGL 4.0
	case api::device_caps::fill_mode_non_solid:
		return true;
	case api::device_caps::bind_render_targets_and_depth_stencil:
		return false;
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

bool reshade::opengl::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage, api::resource *out_handle)
{
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
		if ((desc.flags & api::resource_flags::cube_compatible) != api::resource_flags::cube_compatible)
			target = desc.texture.depth_or_layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
		else
			target = desc.texture.depth_or_layers > 6 ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
		break;
	case api::resource_type::texture_3d:
		target = GL_TEXTURE_3D;
		break;
	default:
		*out_handle = { 0 };
		return false;
	}

	GLuint object = 0;
	GLuint prev_object = 0;
	glGetIntegerv(get_binding_for_target(target), reinterpret_cast<GLint *>(&prev_object));

	if (desc.type == api::resource_type::buffer)
	{
		if (desc.buffer.size == 0)
		{
			*out_handle = { 0 };
			return false;
		}

		glGenBuffers(1, &object);
		glBindBuffer(target, object);

		GLbitfield usage_flags = GL_NONE;
		convert_memory_heap_to_flags(desc, usage_flags);

		// Upload of initial data is using 'glBufferSubData', which requires the dynamic storage flag
		if (initial_data != nullptr)
			usage_flags |= GL_DYNAMIC_STORAGE_BIT;

		assert(desc.buffer.size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));
		glBufferStorage(target, static_cast<GLsizeiptr>(desc.buffer.size), nullptr, usage_flags);

		if (initial_data != nullptr)
		{
			update_buffer_region(initial_data->data, make_resource_handle(GL_BUFFER, object), 0, desc.buffer.size);
		}

		glBindBuffer(target, prev_object);

		// Handles to buffer resources always have the target set to 'GL_BUFFER'
		target = GL_BUFFER;
	}
	else
	{
		const GLenum internal_format = convert_format(desc.texture.format);
		if (desc.texture.width == 0 || internal_format == GL_NONE)
		{
			*out_handle = { 0 };
			return false;
		}

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

		if (initial_data != nullptr)
		{
			for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
				update_texture_region(initial_data[subresource], make_resource_handle(target, object), subresource, nullptr);
		}

		glBindTexture(target, prev_object);
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

bool reshade::opengl::device_impl::create_resource_view(api::resource resource, api::resource_usage, const api::resource_view_desc &desc, api::resource_view *out_handle)
{
	if (resource.handle == 0)
	{
		*out_handle = { 0 };
		return false;
	}

	const bool is_srgb_format =
		desc.format != api::format_to_default_typed(desc.format, 0) &&
		desc.format == api::format_to_default_typed(desc.format, 1);

	const GLenum resource_target = resource.handle >> 40;
	if (resource_target == GL_RENDERBUFFER || resource_target == GL_FRAMEBUFFER_DEFAULT)
	{
		*out_handle = make_resource_view_handle(resource_target, resource.handle & 0xFFFFFFFF, 0x1 | (is_srgb_format ? 0x2 : 0));
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
		*out_handle = { 0 };
		return false;
	}

	GLenum resource_format = GL_NONE;
	if (_supports_dsa)
	{
		glGetTextureLevelParameteriv(resource.handle & 0xFFFFFFFF, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&resource_format));
	}
	else
	{
		GLuint prev_object = 0;
		glGetIntegerv(reshade::opengl::get_binding_for_target(resource_target), reinterpret_cast<GLint *>(&prev_object));
		glBindTexture(resource_target, resource.handle & 0xFFFFFFFF);
		glGetTexLevelParameteriv(resource_target, 0, GL_TEXTURE_INTERNAL_FORMAT, reinterpret_cast<GLint *>(&resource_format));
		glBindTexture(resource_target, prev_object);
	}

	const GLenum internal_format = convert_format(desc.format);
	if (internal_format == GL_NONE)
	{
		*out_handle = { 0 };
		return false;
	}

	if (target == resource_target &&
		desc.texture.first_level == 0 && desc.texture.first_layer == 0 && internal_format == resource_format)
	{
		assert(target != GL_TEXTURE_BUFFER);

		// No need to create a view, so use resource directly, but set a bit so to not destroy it twice via 'destroy_resource_view'
		*out_handle = make_resource_view_handle(target, resource.handle & 0xFFFFFFFF, 0x1 | (is_srgb_format ? 0x2 : 0));
		return true;
	}
	else if (resource_target == GL_TEXTURE_CUBE_MAP && target == GL_TEXTURE_2D && desc.texture.layer_count == 1)
	{
		// Cube map face is handled via special target
		*out_handle = make_resource_view_handle(GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.texture.first_layer, resource.handle & 0xFFFFFFFF, 0x1);
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
			glTextureView(object, target, resource.handle & 0xFFFFFFFF, internal_format, desc.texture.first_level, desc.texture.level_count, desc.texture.first_layer, desc.texture.layer_count);
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

		*out_handle = make_resource_view_handle(target, object, is_srgb_format ? 0x2 : 0);
		return true;
	}
}
void reshade::opengl::device_impl::destroy_resource_view(api::resource_view handle)
{
	if ((handle.handle & 0x100000000) == 0)
		destroy_resource({ handle.handle });
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

bool reshade::opengl::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
	*out_handle = { 0 };

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
		return false;
	}
}
bool reshade::opengl::device_impl::create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
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

		*out_handle = { 0 };
		return false;
	}

	const auto state = new pipeline_impl();
	state->program = program;

	*out_handle = { reinterpret_cast<uintptr_t>(state) };
	return true;
}
bool reshade::opengl::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
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

		*out_handle = { 0 };
		return false;
	}

	const auto impl = new pipeline_impl();
	impl->program = program;

	{
		GLuint prev_vao = 0;
		glGenVertexArrays(1, &impl->vao);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, reinterpret_cast<GLint *>(&prev_vao));

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
	impl->blend_constant[0] = ((desc.graphics.blend_state.blend_constant      ) & 0xFF) / 255.0f;
	impl->blend_constant[1] = ((desc.graphics.blend_state.blend_constant >>  4) & 0xFF) / 255.0f;
	impl->blend_constant[2] = ((desc.graphics.blend_state.blend_constant >>  8) & 0xFF) / 255.0f;
	impl->blend_constant[3] = ((desc.graphics.blend_state.blend_constant >> 12) & 0xFF) / 255.0f;
	for (int i = 0; i < 8; ++i)
	{
		impl->blend_enable[i] = desc.graphics.blend_state.blend_enable[i];
		impl->blend_src[i] = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		impl->blend_dst[i] = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		impl->blend_src_alpha[i] = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		impl->blend_dst_alpha[i] = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
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
void reshade::opengl::device_impl::destroy_pipeline(api::pipeline_stage, api::pipeline handle)
{
	if (handle.handle == 0)
		return;

	const auto impl = reinterpret_cast<pipeline_impl *>(handle.handle);

	glDeleteProgram(impl->program);
	glDeleteVertexArrays(1, &impl->vao);

	delete impl;
}

bool reshade::opengl::device_impl::create_render_pass(const api::render_pass_desc &, api::render_pass *out_handle)
{
	*out_handle = { 0 };
	return true;
}
void reshade::opengl::device_impl::destroy_render_pass(api::render_pass handle)
{
	assert(handle.handle == 0);
}

bool reshade::opengl::device_impl::create_framebuffer(const api::framebuffer_desc &desc, api::framebuffer *out_handle)
{
	if ((desc.render_targets[0].handle >> 40) == GL_FRAMEBUFFER_DEFAULT && (
		// Can only use both the color and depth-stencil attachments of the default framebuffer together, not bind them individually
		(desc.depth_stencil.handle == 0 || (desc.depth_stencil.handle >> 40) == GL_FRAMEBUFFER_DEFAULT)))
	{
		*out_handle = make_framebuffer_handle(0);
		return true;
	}

	GLuint prev_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&prev_fbo));

	GLuint fbo_object = 0;
	glGenFramebuffers(1, &fbo_object);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_object);

	bool has_srgb_attachment = false;
	uint32_t num_color_attachments = 0;

	for (GLuint i = 0; i < 8 && desc.render_targets[i].handle != 0; ++i, ++num_color_attachments)
	{
		switch (desc.render_targets[i].handle >> 40)
		{
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, desc.render_targets[i].handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, desc.render_targets[i].handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
			glDeleteFramebuffers(1, &fbo_object);
			*out_handle = { 0 };
			return false;
		}

		if (desc.render_targets[i].handle & 0x200000000)
			has_srgb_attachment = true;
	}

	if (desc.depth_stencil.handle != 0)
	{
		switch (desc.depth_stencil.handle >> 40)
		{
		case GL_TEXTURE_BUFFER:
		case GL_TEXTURE_1D:
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
		case GL_TEXTURE_RECTANGLE:
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, desc.depth_stencil.handle & 0xFFFFFFFF, 0);
			break;
		case GL_RENDERBUFFER:
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, desc.depth_stencil.handle & 0xFFFFFFFF);
			break;
		default:
			assert(false);
			glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
			glDeleteFramebuffers(1, &fbo_object);
			*out_handle = { 0 };
			return false;
		}
	}

	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);

	if (status == GL_FRAMEBUFFER_COMPLETE)
	{
		*out_handle = make_framebuffer_handle(fbo_object, num_color_attachments, has_srgb_attachment ? 0x2 : 0);
		return true;
	}
	else
	{
		glDeleteFramebuffers(1, &fbo_object);

		*out_handle = { 0 };
		return false;
	}
}
void reshade::opengl::device_impl::destroy_framebuffer(api::framebuffer handle)
{
	const GLuint object = handle.handle & 0xFFFFFFFF;
	glDeleteFramebuffers(1, &object);
}

bool reshade::opengl::device_impl::create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle)
{
	bool success = true;

	const auto impl = new pipeline_layout_impl();
	impl->params.assign(params, params + param_count);
	impl->bindings.resize(param_count);

	for (uint32_t i = 0; i < param_count && success; ++i)
	{
		if (params[i].type != api::pipeline_layout_param_type::push_constants)
		{
			const auto set_layout_impl = reinterpret_cast<const descriptor_set_layout_impl *>(params[i].descriptor_layout.handle);

			impl->bindings[i] = set_layout_impl->range.binding;
		}
		else
		{
			if (params[i].push_constants.offset != 0)
				success = false;

			impl->bindings[i] = params[i].push_constants.binding;
		}
	}

	if (success)
	{
		*out_handle = { reinterpret_cast<uintptr_t>(impl) };
		return true;
	}
	else
	{
		delete impl;

		*out_handle = { 0 };
		return false;
	}
}
void reshade::opengl::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	delete reinterpret_cast<pipeline_layout_impl *>(handle.handle);
}

bool reshade::opengl::device_impl::create_descriptor_set_layout(uint32_t range_count, const api::descriptor_range *ranges, bool, api::descriptor_set_layout *out_handle)
{
	bool success = true;
	api::descriptor_range merged_range = range_count ? ranges[0] : api::descriptor_range {};

	for (uint32_t i = 1; i < range_count && success; ++i)
	{
		if (ranges[i].type != merged_range.type || ranges[i].dx_register_space != 0 || ranges[i].array_size > 1)
			success = false;

		if (ranges[i].offset >= merged_range.offset)
		{
			const uint32_t distance = ranges[i].offset - merged_range.offset;

			if ((ranges[i].binding - merged_range.binding) != distance)
				success = false;

			merged_range.count += distance;
			merged_range.visibility |= ranges[i].visibility;
		}
		else
		{
			const uint32_t distance = merged_range.offset - ranges[i].offset;

			if ((merged_range.binding - ranges[i].binding) != distance)
				success = false;

			merged_range.offset = ranges[i].offset;
			merged_range.binding = ranges[i].binding;
			merged_range.dx_register_index = ranges[i].dx_register_index;
			merged_range.count += distance;
			merged_range.visibility |= ranges[i].visibility;
		}
	}

	if (success)
	{
		const auto impl = new descriptor_set_layout_impl();
		impl->range = merged_range;

		*out_handle = { reinterpret_cast<uintptr_t>(impl) };
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::opengl::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}

bool reshade::opengl::device_impl::create_query_pool(api::query_type type, uint32_t size, api::query_pool *out_handle)
{
	if (type == api::query_type::pipeline_statistics)
	{
		*out_handle = { 0 };
		return false;
	}

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

bool reshade::opengl::device_impl::create_descriptor_sets(uint32_t count, const api::descriptor_set_layout *layouts, api::descriptor_set *out_sets)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto set_layout_impl = reinterpret_cast<const descriptor_set_layout_impl *>(layouts[i].handle);
		assert(set_layout_impl != nullptr);

		const auto impl = new descriptor_set_impl();
		impl->type = set_layout_impl->range.type;
		impl->count = set_layout_impl->range.count;

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
void reshade::opengl::device_impl::destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
		delete reinterpret_cast<descriptor_set_impl *>(sets[i].handle);
}

bool reshade::opengl::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, api::subresource_data *out_data)
{
	GLenum map_access = 0;
	switch (access)
	{
	case api::map_access::read_only:
		map_access = GL_MAP_READ_BIT;
		break;
	case api::map_access::write_only:
		map_access = GL_MAP_WRITE_BIT;
		break;
	case api::map_access::read_write:
		map_access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
		break;
	case api::map_access::write_discard:
		map_access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
		break;
	}

	assert(out_data != nullptr);
	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	assert(resource.handle != 0);
	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	// Can only map buffer resources
	if (target != GL_BUFFER)
		return false;

	assert(subresource == 0);

	if (_supports_dsa)
	{
		GLint max_size = 0;
		glGetNamedBufferParameteriv(object, GL_BUFFER_SIZE, &max_size);

		out_data->data = glMapNamedBufferRange(object, 0, max_size, map_access);
		out_data->row_pitch = max_size;
		out_data->slice_pitch = max_size;
	}
	else
	{
		GLint max_size = 0;
		GLint prev_object = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &prev_object);

		glBindBuffer(GL_COPY_WRITE_BUFFER, object);
		glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &max_size);

		out_data->data = glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, max_size, map_access);
		out_data->row_pitch = max_size;
		out_data->slice_pitch = max_size;

		glBindBuffer(GL_COPY_WRITE_BUFFER, prev_object);
	}

	return out_data->data != nullptr;
}
void reshade::opengl::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	// Can only map buffer resources
	if (target != GL_BUFFER)
		return;

	assert(subresource == 0);

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

void reshade::opengl::device_impl::update_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	assert(dst_offset <= static_cast<uint64_t>(std::numeric_limits<GLintptr>::max()) && size <= static_cast<uint64_t>(std::numeric_limits<GLsizeiptr>::max()));

	const GLenum target = dst.handle >> 40;
	const GLuint object = dst.handle & 0xFFFFFFFF;

	if (_supports_dsa)
	{
		glNamedBufferSubData(object, static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size), data);
	}
	else
	{
		GLint previous_buf = 0;
		glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previous_buf);

		glBindBuffer(GL_COPY_WRITE_BUFFER, object);
		glBufferSubData(GL_COPY_WRITE_BUFFER, static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size), data);

		glBindBuffer(GL_COPY_WRITE_BUFFER, previous_buf);
	}

}
void reshade::opengl::device_impl::update_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
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

	const GLuint level = dst_subresource % levels;
	      GLuint layer = dst_subresource / levels;

	GLint format = GL_NONE; GLenum type;
	glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, &format);

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
		glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH,  &width);
		glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
		glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH,  &depth);
	}

	const auto row_size_packed = width * api::format_bytes_per_pixel(convert_format(format));
	const auto slice_size_packed = height * row_size_packed;
	const auto total_size = depth * slice_size_packed;

	format = convert_upload_format(format, type);

	std::vector<uint8_t> temp_pixels;
	const uint8_t *pixels = static_cast<const uint8_t *>(data.data);

	if ((row_size_packed != data.row_pitch && height == 1) ||
		(slice_size_packed != data.slice_pitch && depth == 1))
	{
		temp_pixels.resize(total_size);
		uint8_t *dst_pixels = temp_pixels.data();

		for (GLint z = 0; z < depth; ++z)
			for (GLint y = 0; y < height; ++y, dst_pixels += row_size_packed)
				std::memcpy(dst_pixels, pixels + z * data.slice_pitch + y * data.row_pitch, row_size_packed);

		pixels = temp_pixels.data();
	}

	GLenum upload_target = target;
	if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY)
	{
		const GLuint face = layer % 6;
		layer /= 6;
		upload_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
	}

	switch (upload_target)
	{
	case GL_TEXTURE_1D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS) {
			glTexSubImage1D(upload_target, level, xoffset, width, format, type, pixels);
		} else {
			glCompressedTexSubImage1D(upload_target, level, xoffset, width, format, total_size, pixels);
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
		if (type != GL_COMPRESSED_TEXTURE_FORMATS) {
			glTexSubImage2D(upload_target, level, xoffset, yoffset, width, height, format, type, pixels);
		} else {
			glCompressedTexSubImage2D(upload_target, level, xoffset, yoffset, width, height, format, total_size, pixels);
		}
		break;
	case GL_TEXTURE_2D_ARRAY:
		zoffset += layer;
		[[fallthrough]];
	case GL_TEXTURE_3D:
		if (type != GL_COMPRESSED_TEXTURE_FORMATS) {
			glTexSubImage3D(upload_target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
		} else {
			glCompressedTexSubImage3D(upload_target, level, xoffset, yoffset, zoffset, width, height, depth, format, total_size, pixels);
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

void reshade::opengl::device_impl::update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto impl = reinterpret_cast<descriptor_set_impl *>(updates[i].set.handle);

		const api::descriptor_set_update &update = updates[i];

		// In GLSL targeting OpenGL, if the binding qualifier is used with an array, the first element of the array takes the specified block binding and each subsequent element takes the next consecutive binding point
		// See https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.60.html#layout-qualifiers (chapter 4.4.6)
		// Therefore it is not valid to specify an array offset for a binding
		assert(update.array_offset == 0);

		switch (update.type)
		{
		case api::descriptor_type::sampler:
		case api::descriptor_type::shader_resource_view:
		case api::descriptor_type::unordered_access_view:
			std::memcpy(&impl->descriptors[update.binding * 1], update.descriptors, update.count * sizeof(uint64_t) * 1);
			break;
		case api::descriptor_type::sampler_with_resource_view:
			std::memcpy(&impl->descriptors[update.binding * 2], update.descriptors, update.count * sizeof(uint64_t) * 2);
			break;
		case api::descriptor_type::constant_buffer:
			std::memcpy(&impl->descriptors[update.binding * 3], update.descriptors, update.count * sizeof(uint64_t) * 3);
			break;
		default:
			assert(false);
			break;
		}
	}
}

bool reshade::opengl::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	assert(stride >= sizeof(uint64_t));

	const auto impl = reinterpret_cast<query_pool_impl *>(pool.handle);

	for (uint32_t i = 0; i < count; ++i)
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

void reshade::opengl::device_impl::wait_idle() const
{
	glFinish();
}

void reshade::opengl::device_impl::set_resource_name(api::resource resource, const char *name)
{
	glObjectLabel((resource.handle >> 40) == GL_BUFFER ? GL_BUFFER : GL_TEXTURE, resource.handle & 0xFFFFFFFF, -1, name);
}

void reshade::opengl::device_impl::get_pipeline_layout_desc(api::pipeline_layout layout, uint32_t *count, api::pipeline_layout_param *params) const
{
	assert(layout.handle != 0 && count != nullptr);
	const auto layout_impl = reinterpret_cast<const pipeline_layout_impl *>(layout.handle);

	if (params != nullptr)
	{
		*count = std::min(*count, static_cast<uint32_t>(layout_impl->params.size()));
		std::memcpy(params, layout_impl->params.data(), *count * sizeof(api::pipeline_layout_param));
	}
	else
	{
		*count = static_cast<uint32_t>(layout_impl->params.size());
	}
}

void reshade::opengl::device_impl::get_descriptor_pool_offset(api::descriptor_set, api::descriptor_pool *pool, uint32_t *offset) const
{
	*pool = { 0 };
	*offset = 0; // Not implemented
}

void reshade::opengl::device_impl::get_descriptor_set_layout_desc(api::descriptor_set_layout layout, uint32_t *count, api::descriptor_range *ranges) const
{
	assert(layout.handle != 0 && count != nullptr);
	const auto layout_impl = reinterpret_cast<descriptor_set_layout_impl *>(layout.handle);

	if (ranges != nullptr)
	{
		if (*count != 0)
		{
			*count = 1;
			*ranges = layout_impl->range;
		}
	}
	else
	{
		*count = 1;
	}
}

reshade::api::resource_desc reshade::opengl::device_impl::get_resource_desc(api::resource resource) const
{
	GLint width = 0, height = 1, depth = 1, levels = 1, samples = 1, internal_format = GL_NONE, prev_object = 0;

	const GLenum target = resource.handle >> 40;
	const GLuint object = resource.handle & 0xFFFFFFFF;

	switch (target)
	{
		case GL_BUFFER:
		{
			if (_supports_dsa)
			{
				glGetNamedBufferParameteriv(object, GL_BUFFER_SIZE, &width);
			}
			else
			{
				glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &prev_object);
				glBindBuffer(GL_COPY_READ_BUFFER, object);

				glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &width);

				glBindBuffer(GL_COPY_READ_BUFFER, prev_object);
			}

			return convert_resource_desc(target, width);
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
			}
			else
			{
				glGetIntegerv(reshade::opengl::get_binding_for_target(target), &prev_object);
				glBindTexture(target, object);

				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &width);
				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &height);
				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &depth);
				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
				glGetTexLevelParameteriv(target, 0, GL_TEXTURE_SAMPLES, &samples);

				glGetTexParameteriv(target, GL_TEXTURE_IMMUTABLE_LEVELS, &levels);
				if (levels == 0)
				{
					glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, &levels);
					for (GLsizei level = 1, level_w = 0; level < levels; ++level)
					{
						glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &level_w);
						if (0 == level_w)
						{
							levels = level;
							break;
						}
					}
				}

				glBindTexture(target, prev_object);
			}

			return convert_resource_desc(target, levels, samples, internal_format, width, height, depth);
		}
		case GL_RENDERBUFFER:
		{
			if (_supports_dsa)
			{
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_WIDTH, &width);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_HEIGHT, &height);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_INTERNAL_FORMAT, &internal_format);
				glGetNamedRenderbufferParameteriv(object, GL_RENDERBUFFER_SAMPLES, &samples);
			}
			else
			{
				glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_object);
				glBindRenderbuffer(GL_RENDERBUFFER, object);

				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &internal_format);
				glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);

				glBindRenderbuffer(GL_RENDERBUFFER, prev_object);
			}

			return convert_resource_desc(target, 1, samples, internal_format, width, height);
		}
		case GL_FRAMEBUFFER_DEFAULT:
		{
			width = _default_fbo_width;
			height = _default_fbo_height;
			internal_format = (object == GL_DEPTH_STENCIL_ATTACHMENT || object == GL_DEPTH_ATTACHMENT || object == GL_STENCIL_ATTACHMENT) ? _default_depth_format : _default_color_format;

			return convert_resource_desc(target, levels, samples, internal_format, width, height);
		}
	}

	assert(false); // Not implemented
	return api::resource_desc {};
}

reshade::api::resource reshade::opengl::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);

	// Remove extra bits from view
	return { view.handle & 0xFFFFFF00FFFFFFFF };
}

reshade::api::resource_view reshade::opengl::device_impl::get_framebuffer_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index) const
{
	assert(fbo.handle != 0);
	const GLuint fbo_object = fbo.handle & 0xFFFFFFFF;

	// Zero is valid too, in which case the default frame buffer is referenced, instead of a FBO
	if (fbo_object == 0)
	{
		if (type == api::attachment_type::color)
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

	GLenum attachment = GL_NONE;
	switch (type)
	{
	case api::attachment_type::color:
		attachment = GL_COLOR_ATTACHMENT0 + index;
		if (index >= (fbo.handle >> 40))
		{
			return make_resource_view_handle(0, 0);
		}
		break;
	case api::attachment_type::depth:
		attachment = GL_DEPTH_ATTACHMENT;
		break;
	case api::attachment_type::stencil:
		attachment = GL_STENCIL_ATTACHMENT;
		break;
	default:
		if (type == (api::attachment_type::depth | api::attachment_type::stencil))
		{
			attachment = GL_DEPTH_STENCIL_ATTACHMENT;
			break;
		}
		else
		{
			return make_resource_view_handle(0, 0);
		}
	}

	GLenum target = GL_NONE, object = 0;
	if (_supports_dsa)
	{
		glGetNamedFramebufferAttachmentParameteriv(fbo_object, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, reinterpret_cast<GLint *>(&target));

		// Check if FBO does have this attachment
		if (target != GL_NONE)
		{
			glGetNamedFramebufferAttachmentParameteriv(fbo_object, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, reinterpret_cast<GLint *>(&object));

			// Get actual texture target from the texture object
			if (target == GL_TEXTURE)
				glGetTextureParameteriv(object, GL_TEXTURE_TARGET, reinterpret_cast<GLint *>(&target));

			return make_resource_view_handle(target, object);
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

	return make_resource_view_handle(target, object);
}
