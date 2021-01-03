/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_config.hpp"
#include "runtime_gl.hpp"
#include "runtime_objects.hpp"
#include <imgui.h>

namespace reshade::opengl
{
	struct tex_data
	{
		GLuint id[2] = {};
		GLuint levels = 0;
		GLenum internal_format = GL_NONE;
	};

	struct sampler_data
	{
		GLuint id;
		tex_data *texture;
		bool is_srgb_format;
	};

	struct pass_data
	{
		GLuint fbo = 0;
		GLuint program = 0;
		GLenum blend_eq_color = GL_NONE;
		GLenum blend_eq_alpha = GL_NONE;
		GLenum blend_src = GL_NONE;
		GLenum blend_src_alpha = GL_NONE;
		GLenum blend_dest = GL_NONE;
		GLenum blend_dest_alpha = GL_NONE;
		GLenum stencil_func = GL_NONE;
		GLenum stencil_op_fail = GL_NONE;
		GLenum stencil_op_z_fail = GL_NONE;
		GLenum stencil_op_z_pass = GL_NONE;
		GLenum draw_targets[8] = {};
		std::vector<tex_data *> storages;
		std::vector<sampler_data> samplers;
	};

	struct technique_data
	{
		GLuint query = 0;
		bool query_in_flight = false;
		std::vector<pass_data> passes;
	};
}

reshade::opengl::runtime_gl::runtime_gl()
{
	GLint major = 0, minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	_renderer_id = 0x10000 | (major << 12) | (minor << 8);

	const GLubyte *const name = glGetString(GL_RENDERER);
	const GLubyte *const version = glGetString(GL_VERSION);
	LOG(INFO) << "Running on " << name << " using OpenGL " << version;

	// Query vendor and device ID from Windows assuming we are running on the primary display device
	// This is done because the information reported by OpenGL is not always reflecting the actual rendering device (e.g. on NVIDIA Optimus laptops)
	DISPLAY_DEVICEA dd = { sizeof(dd) };
	for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0) != FALSE; ++i)
	{
		if ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0)
		{
			std::sscanf(dd.DeviceID, "PCI\\VEN_%x&DEV_%x", &_vendor_id, &_device_id);
			break;
		}
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

#if RESHADE_GUI
	subscribe_to_ui("OpenGL", [this]() {
		// Add some information about the device and driver to the UI
		ImGui::Text("OpenGL %s", glGetString(GL_VERSION));
		ImGui::TextUnformatted(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));

#if RESHADE_DEPTH
		draw_depth_debug_menu();
#endif
	});
#endif
	subscribe_to_load_config([this](const ini_file &config) {
		// Reserve a fixed amount of texture names by default to work around issues in old OpenGL games (which will use a compatibility context)
		auto num_reserve_texture_names = _compatibility_context ? 512u : 0u;
		config.get("APP", "ReserveTextureNames", num_reserve_texture_names);
		_reserved_texture_names.resize(num_reserve_texture_names);

#if RESHADE_DEPTH
		auto force_default_depth_override = false;
		config.get("DEPTH", "ForceMainDepthBuffer", force_default_depth_override);
		config.get("DEPTH", "DepthCopyBeforeClears", _state_tracking.preserve_depth_buffers);
		config.get("DEPTH", "DepthCopyAtClearIndex", _state_tracking.depthstencil_clear_index.second);
		config.get("DEPTH", "UseAspectRatioHeuristics", _state_tracking.use_aspect_ratio_heuristics);

		if (force_default_depth_override)
			_depth_source_override = 0; // Zero has a special meaning and corresponds to the default depth buffer
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("DEPTH", "ForceMainDepthBuffer", _depth_source_override == 0);
		config.set("DEPTH", "DepthCopyBeforeClears", _state_tracking.preserve_depth_buffers);
		config.set("DEPTH", "DepthCopyAtClearIndex", _state_tracking.depthstencil_clear_index.second);
		config.set("DEPTH", "UseAspectRatioHeuristics", _state_tracking.use_aspect_ratio_heuristics);
#endif
	});

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	glDebugMessageCallback([](unsigned int /*source*/, unsigned int type, unsigned int /*id*/, unsigned int /*severity*/, int /*length*/, const char *message, const void */*userParam*/) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message);
		}, nullptr);
#endif
}

bool reshade::opengl::runtime_gl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);

	const HDC hdc = GetDC(hwnd);
	PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
	DescribePixelFormat(hdc, GetPixelFormat(hdc), sizeof(pfd), &pfd);

	_width = width;
	_height = height;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_color_bit_depth = std::min(pfd.cRedBits, std::min(pfd.cGreenBits, pfd.cBlueBits));

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

	// Initialize default frame buffer information
	_state_tracking.reset(_width, _height, _default_depth_format);

	// Capture and later restore so that the resource creation code below does not affect the application state
	_app_state.capture(_compatibility_context);

	// Some games (like Hot Wheels Velocity X) use fixed texture names, which can clash with the ones ReShade generates below, since most implementations will return values linearly
	// Reserve a configurable range of names for those games to work around this
	if (!_reserved_texture_names.empty())
		glGenTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());

	glGenBuffers(NUM_BUF, _buf);
	glGenTextures(NUM_TEX, _tex);
	glGenVertexArrays(NUM_VAO, _vao);
	glGenFramebuffers(NUM_FBO, _fbo);
	glGenRenderbuffers(NUM_RBO, _rbo);

	glBindTexture(GL_TEXTURE_2D, _tex[TEX_BACK]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _width, _height);
	glTextureView(_tex[TEX_BACK_SRGB], GL_TEXTURE_2D, _tex[TEX_BACK], GL_SRGB8_ALPHA8, 0, 1, 0, 1);

	glBindRenderbuffer(GL_RENDERBUFFER, _rbo[RBO_COLOR]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, _width, _height);
	glBindRenderbuffer(GL_RENDERBUFFER, _rbo[RBO_STENCIL]);
	// As of OpenGL 4.3 support for GL_STENCIL_INDEX8 is a requirement for render buffers
	glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, _width, _height);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_BACK]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo[RBO_COLOR]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo[RBO_STENCIL]);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_BLIT]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _tex[TEX_BACK_SRGB], 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

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

#if RESHADE_DEPTH
	// Initialize depth texture and FBO by assuming they refer to the default frame buffer
	_has_depth_texture = _default_depth_format != GL_NONE;
	if (_has_depth_texture)
	{
		_copy_depth_source = true;
		_depth_source_width = _width;
		_depth_source_height = _height;
		_depth_source_format = _default_depth_format;
		glBindTexture(GL_TEXTURE_2D, _tex[TEX_DEPTH]);
		glTexStorage2D(GL_TEXTURE_2D, 1, _depth_source_format, _depth_source_width, _depth_source_height);

		glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_DEPTH_DEST]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _tex[TEX_DEPTH], 0);
		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	}
#endif

#if RESHADE_GUI
	init_imgui_resources();
#endif

	_app_state.apply(_compatibility_context);

	return runtime::on_init(hwnd);
}
void reshade::opengl::runtime_gl::on_reset()
{
	runtime::on_reset();

	_state_tracking.release();

	glDeleteBuffers(NUM_BUF, _buf);
	glDeleteTextures(NUM_TEX, _tex);
	glDeleteTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());
	glDeleteVertexArrays(NUM_VAO, _vao);
	glDeleteFramebuffers(NUM_FBO, _fbo);
	glDeleteRenderbuffers(NUM_RBO, _rbo);

	std::memset(_buf, 0, sizeof(_vao));
	std::memset(_tex, 0, sizeof(_tex));
	std::memset(_vao, 0, sizeof(_vao));
	std::memset(_fbo, 0, sizeof(_fbo));
	std::memset(_rbo, 0, sizeof(_rbo));

	glDeleteProgram(_mipmap_program);
	_mipmap_program = 0;

#if RESHADE_DEPTH
	_depth_source = 0;
	_depth_source_width = 0;
	_depth_source_height = 0;
	_depth_source_format = 0;
	_depth_source_override = std::numeric_limits<GLuint>::max();
	_has_depth_texture = false;
	_copy_depth_source = false;
#endif

#if RESHADE_GUI
	glDeleteProgram(_imgui.program);
	_imgui.program = 0;
#endif
}

void reshade::opengl::runtime_gl::on_present()
{
	if (!_is_initialized)
		return;

	_vertices = _state_tracking.total_vertices();
	_drawcalls = _state_tracking.total_drawcalls();

	_app_state.capture(_compatibility_context);

#if RESHADE_DEPTH
	update_depth_texture_bindings(_has_high_network_activity ? state_tracking::depthstencil_info { 0 } :
		_state_tracking.find_best_depth_texture(_width, _height, _depth_source_override));
#endif

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// Copy back buffer to RBO (and flip it vertically)
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_BACK]);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	_current_fbo = _fbo[FBO_BACK];

#if RESHADE_DEPTH
	// Copy depth from FBO to depth texture (and flip it vertically)
	if (_copy_depth_source)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _depth_source == 0 ? 0 : _fbo[FBO_DEPTH_SRC]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_DEPTH_DEST]);
		glBlitFramebuffer(0, 0, _depth_source_width, _depth_source_height, 0, _depth_source_height, _depth_source_width, 0, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}
#endif

	update_and_render_effects();

	// Copy results from RBO to back buffer (and flip it back vertically)
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[FBO_BACK]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	_current_fbo = 0;

	runtime::on_present();

	_state_tracking.reset(_width, _height, _default_depth_format);

	// Apply previous state from application
	_app_state.apply(_compatibility_context);
}

bool reshade::opengl::runtime_gl::capture_screenshot(uint8_t *buffer) const
{
	assert(_app_state.has_state);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, _current_fbo);
	glReadBuffer(_current_fbo == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, GLsizei(_width), GLsizei(_height), GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	// Flip image vertically (unless it came from the RBO, which is already upside down)
	if (_current_fbo == 0)
	{
		for (unsigned int y = 0, pitch = _width * 4; y * 2 < _height; ++y)
		{
			const auto i1 = y * pitch;
			const auto i2 = (_height - 1 - y) * pitch;

			for (unsigned int x = 0; x < pitch; x += 4)
			{
				std::swap(buffer[i1 + x + 0], buffer[i2 + x + 0]);
				std::swap(buffer[i1 + x + 1], buffer[i2 + x + 1]);
				std::swap(buffer[i1 + x + 2], buffer[i2 + x + 2]);
				std::swap(buffer[i1 + x + 3], buffer[i2 + x + 3]);
			}
		}
	}

	return true;
}

bool reshade::opengl::runtime_gl::init_effect(size_t index)
{
	assert(_app_state.has_state); // Make sure all binds below are reset later when application state is restored

	effect &effect = _effects[index];

	// Add specialization constant defines to source code
	std::vector<GLuint> spec_data;
	std::vector<GLuint> spec_constants;
	if (!effect.module.spirv.empty())
	{
		for (const reshadefx::uniform_info &constant : effect.module.spec_constants)
		{
			const GLuint id = static_cast<GLuint>(spec_constants.size());
			spec_data.push_back(constant.initializer_value.as_uint[0]);
			spec_constants.push_back(id);
		}
	}

	// Compile all entry points
	std::unordered_map<std::string, GLuint> entry_points;
	for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
	{
		GLuint shader_type = GL_NONE;
		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			shader_type = GL_VERTEX_SHADER;
			break;
		case reshadefx::shader_type::ps:
			shader_type = GL_FRAGMENT_SHADER;
			break;
		case reshadefx::shader_type::cs:
			shader_type = GL_COMPUTE_SHADER;
			break;
		}

		GLuint shader_object = glCreateShader(shader_type);
		entry_points[entry_point.name] = shader_object;

		if (!effect.module.spirv.empty())
		{
			assert(_renderer_id >= 0x14600); // Core since OpenGL 4.6 (see https://www.khronos.org/opengl/wiki/SPIR-V)
			assert(gl3wProcs.gl.ShaderBinary != nullptr && gl3wProcs.gl.SpecializeShader != nullptr);

			glShaderBinary(1, &shader_object, GL_SHADER_BINARY_FORMAT_SPIR_V, effect.module.spirv.data(), static_cast<GLsizei>(effect.module.spirv.size() * sizeof(uint32_t)));
			glSpecializeShader(shader_object, entry_point.name.c_str(), GLuint(spec_constants.size()), spec_constants.data(), spec_data.data());
		}
		else
		{
			std::string defines = "#version 430\n";
			defines += "#define ENTRY_POINT_" + entry_point.name + " 1\n";

			if (entry_point.type == reshadefx::shader_type::vs)
			{
				// OpenGL does not allow using 'discard' in the vertex shader profile
				defines += "#define discard\n";
				// 'dFdx', 'dFdx' and 'fwidth' too are only available in fragment shaders
				defines += "#define dFdx(x) x\n";
				defines += "#define dFdy(y) y\n";
				defines += "#define fwidth(p) p\n";
			}
			if (entry_point.type != reshadefx::shader_type::cs)
			{
				// OpenGL does not allow using 'shared' in vertex/fragment shader profile
				defines += "#define shared\n";
				defines += "#define atomicAdd(a, b) a\n";
				defines += "#define atomicAnd(a, b) a\n";
				defines += "#define atomicOr(a, b) a\n";
				defines += "#define atomicXor(a, b) a\n";
				defines += "#define atomicMin(a, b) a\n";
				defines += "#define atomicMax(a, b) a\n";
				defines += "#define atomicExchange(a, b) a\n";
				defines += "#define atomicCompSwap(a, b, c) a\n";
				// Barrier intrinsics are only available in compute shaders
				defines += "#define barrier()\n";
				defines += "#define memoryBarrier()\n";
				defines += "#define groupMemoryBarrier()\n";
			}

			defines += "#line 1 0\n"; // Reset line number, so it matches what is shown when viewing the generated code
			defines += effect.preamble;

			GLsizei lengths[] = { static_cast<GLsizei>(defines.size()), static_cast<GLsizei>(effect.module.hlsl.size()) };
			const GLchar *sources[] = { defines.c_str(), effect.module.hlsl.c_str() };
			glShaderSource(shader_object, 2, sources, lengths);
			glCompileShader(shader_object);
		}

		GLint status = GL_FALSE;
		glGetShaderiv(shader_object, GL_COMPILE_STATUS, &status);
		if (GL_FALSE == status)
		{
			GLint log_size = 0;
			glGetShaderiv(shader_object, GL_INFO_LOG_LENGTH, &log_size);
			std::vector<char> log(log_size);
			glGetShaderInfoLog(shader_object, log_size, nullptr, log.data());

			effect.errors += log.data();

			for (auto &it : entry_points)
				glDeleteShader(it.second);

			// No need to setup resources if any of the shaders failed to compile
			return false;
		}
	}

	if (index >= _effect_ubos.size())
		_effect_ubos.resize(index + 1);

	if (!effect.uniform_data_storage.empty())
	{
		GLuint &ubo = _effect_ubos[index];
		glGenBuffers(1, &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		glBufferData(GL_UNIFORM_BUFFER, effect.uniform_data_storage.size(), effect.uniform_data_storage.data(), GL_DYNAMIC_DRAW);
	}

	bool success = true;
	assert(effect.module.num_texture_bindings == 0); // Use combined texture samplers

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		auto impl = new technique_data();
		technique.impl = impl;

		glGenQueries(1, &impl->query);

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			pass_data &pass_data = impl->passes[pass_index];
			reshadefx::pass_info &pass_info = technique.passes[pass_index];

			pass_data.program = glCreateProgram();

			if (!pass_info.cs_entry_point.empty())
			{
				const GLuint cs_shader_id = entry_points.at(pass_info.cs_entry_point);
				glAttachShader(pass_data.program, cs_shader_id);
				glLinkProgram(pass_data.program);
				glDetachShader(pass_data.program, cs_shader_id);

				pass_data.storages.resize(effect.module.num_storage_bindings);
				for (const reshadefx::storage_info &info : pass_info.storages)
				{
					const texture &texture = look_up_texture_by_name(info.texture_name);

					pass_data.storages[info.binding] = static_cast<tex_data *>(texture.impl);
				}
			}
			else
			{
				// Link program from input shaders
				const GLuint vs_shader_id = entry_points.at(pass_info.vs_entry_point);
				const GLuint fs_shader_id = entry_points.at(pass_info.ps_entry_point);
				glAttachShader(pass_data.program, vs_shader_id);
				glAttachShader(pass_data.program, fs_shader_id);
				glLinkProgram(pass_data.program);
				glDetachShader(pass_data.program, vs_shader_id);
				glDetachShader(pass_data.program, fs_shader_id);

				const auto convert_blend_op = [](reshadefx::pass_blend_op value) -> GLenum {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return GL_FUNC_ADD;
					case reshadefx::pass_blend_op::subtract: return GL_FUNC_SUBTRACT;
					case reshadefx::pass_blend_op::rev_subtract: return GL_FUNC_REVERSE_SUBTRACT;
					case reshadefx::pass_blend_op::min: return GL_MIN;
					case reshadefx::pass_blend_op::max: return GL_MAX;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) -> GLenum {
					switch (value)
					{
					case reshadefx::pass_blend_func::zero: return GL_ZERO;
					default:
					case reshadefx::pass_blend_func::one: return GL_ONE;
					case reshadefx::pass_blend_func::src_color: return GL_SRC_COLOR;
					case reshadefx::pass_blend_func::src_alpha: return GL_SRC_ALPHA;
					case reshadefx::pass_blend_func::inv_src_color: return GL_ONE_MINUS_SRC_COLOR;
					case reshadefx::pass_blend_func::inv_src_alpha: return GL_ONE_MINUS_SRC_ALPHA;
					case reshadefx::pass_blend_func::dst_color: return GL_DST_COLOR;
					case reshadefx::pass_blend_func::dst_alpha: return GL_DST_ALPHA;
					case reshadefx::pass_blend_func::inv_dst_color: return GL_ONE_MINUS_DST_COLOR;
					case reshadefx::pass_blend_func::inv_dst_alpha: return GL_ONE_MINUS_DST_ALPHA;
					}
				};
				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) -> GLenum {
					switch (value)
					{
					case reshadefx::pass_stencil_op::zero: return GL_ZERO;
					default:
					case reshadefx::pass_stencil_op::keep: return GL_KEEP;
					case reshadefx::pass_stencil_op::invert: return GL_INVERT;
					case reshadefx::pass_stencil_op::replace: return GL_REPLACE;
					case reshadefx::pass_stencil_op::incr: return GL_INCR_WRAP;
					case reshadefx::pass_stencil_op::incr_sat: return GL_INCR;
					case reshadefx::pass_stencil_op::decr: return GL_DECR_WRAP;
					case reshadefx::pass_stencil_op::decr_sat: return GL_DECR;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) -> GLenum {
					switch (value)
					{
					case reshadefx::pass_stencil_func::never: return GL_NEVER;
					case reshadefx::pass_stencil_func::equal: return GL_EQUAL;
					case reshadefx::pass_stencil_func::not_equal: return GL_NOTEQUAL;
					case reshadefx::pass_stencil_func::less: return GL_LESS;
					case reshadefx::pass_stencil_func::less_equal: return GL_LEQUAL;
					case reshadefx::pass_stencil_func::greater: return GL_GREATER;
					case reshadefx::pass_stencil_func::greater_equal: return GL_GEQUAL;
					default:
					case reshadefx::pass_stencil_func::always: return GL_ALWAYS;
					}
				};

				pass_data.blend_eq_color = convert_blend_op(pass_info.blend_op);
				pass_data.blend_eq_alpha = convert_blend_op(pass_info.blend_op_alpha);
				pass_data.blend_src = convert_blend_func(pass_info.src_blend);
				pass_data.blend_dest = convert_blend_func(pass_info.dest_blend);
				pass_data.blend_src_alpha = convert_blend_func(pass_info.src_blend_alpha);
				pass_data.blend_dest_alpha = convert_blend_func(pass_info.dest_blend_alpha);
				pass_data.stencil_func = convert_stencil_func(pass_info.stencil_comparison_func);
				pass_data.stencil_op_z_pass = convert_stencil_op(pass_info.stencil_op_pass);
				pass_data.stencil_op_fail = convert_stencil_op(pass_info.stencil_op_fail);
				pass_data.stencil_op_z_fail = convert_stencil_op(pass_info.stencil_op_depth_fail);

				glGenFramebuffers(1, &pass_data.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, pass_data.fbo);

				if (pass_info.render_target_names[0].empty())
				{
					pass_info.viewport_width = _width;
					pass_info.viewport_height = _height;

					glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo[RBO_COLOR]);

					pass_data.draw_targets[0] = GL_COLOR_ATTACHMENT0;
				}
				else
				{
					for (uint32_t k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
					{
						tex_data *const tex_impl = static_cast<tex_data *>(
							look_up_texture_by_name(pass_info.render_target_names[k]).impl);

						pass_data.draw_targets[k] = GL_COLOR_ATTACHMENT0 + k;

						glFramebufferTexture(GL_FRAMEBUFFER, pass_data.draw_targets[k], tex_impl->id[pass_info.srgb_write_enable], 0);

						// Add texture to list of modified resources so mipmaps are generated at end of pass
						pass_data.storages.push_back(tex_impl);
					}

					assert(pass_info.viewport_width != 0 && pass_info.viewport_height != 0);
				}

				if (pass_info.stencil_enable && // Only need to attach stencil if stencil is actually used in this pass
					pass_info.viewport_width == _width &&
					pass_info.viewport_height == _height)
				{
					// Only attach stencil when viewport matches back buffer or else the frame buffer will always be resized to those dimensions
					glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo[RBO_STENCIL]);
				}

				assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
			}

			GLint status = GL_FALSE;
			glGetProgramiv(pass_data.program, GL_LINK_STATUS, &status);
			if (GL_FALSE == status)
			{
				GLint log_size = 0;
				glGetProgramiv(pass_data.program, GL_INFO_LOG_LENGTH, &log_size);
				std::vector<char> log(log_size);
				glGetProgramInfoLog(pass_data.program, log_size, nullptr, log.data());

				effect.errors += log.data();

				LOG(ERROR) << "Failed to link program for pass " << pass_index << " in technique '" << technique.name << "'!";
				success = false;
				break;
			}

			pass_data.samplers.resize(effect.module.num_sampler_bindings);
			for (const reshadefx::sampler_info &info : pass_info.samplers)
			{
				const texture &texture = look_up_texture_by_name(info.texture_name);

				// Hash sampler state to avoid duplicated sampler objects
				size_t hash = 2166136261;
				hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_u);
				hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_v);
				hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_w);
				hash = (hash * 16777619) ^ static_cast<uint32_t>(info.filter);
				hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.lod_bias);
				hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.min_lod);
				hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.max_lod);

				std::unordered_map<size_t, GLuint>::iterator it = _effect_sampler_states.find(hash);
				if (it == _effect_sampler_states.end())
				{
					GLenum min_filter = GL_NONE, mag_filter = GL_NONE;
					switch (info.filter)
					{
					case reshadefx::texture_filter::min_mag_mip_point:
						min_filter = GL_NEAREST_MIPMAP_NEAREST;
						mag_filter = GL_NEAREST;
						break;
					case reshadefx::texture_filter::min_mag_point_mip_linear:
						min_filter = GL_NEAREST_MIPMAP_LINEAR;
						mag_filter = GL_NEAREST;
						break;
					case reshadefx::texture_filter::min_point_mag_linear_mip_point:
						min_filter = GL_NEAREST_MIPMAP_NEAREST;
						mag_filter = GL_LINEAR;
						break;
					case reshadefx::texture_filter::min_point_mag_mip_linear:
						min_filter = GL_NEAREST_MIPMAP_LINEAR;
						mag_filter = GL_LINEAR;
						break;
					case reshadefx::texture_filter::min_linear_mag_mip_point:
						min_filter = GL_LINEAR_MIPMAP_NEAREST;
						mag_filter = GL_NEAREST;
						break;
					case reshadefx::texture_filter::min_linear_mag_point_mip_linear:
						min_filter = GL_LINEAR_MIPMAP_LINEAR;
						mag_filter = GL_NEAREST;
						break;
					case reshadefx::texture_filter::min_mag_linear_mip_point:
						min_filter = GL_LINEAR_MIPMAP_NEAREST;
						mag_filter = GL_LINEAR;
						break;
					case reshadefx::texture_filter::min_mag_mip_linear:
						min_filter = GL_LINEAR_MIPMAP_LINEAR;
						mag_filter = GL_LINEAR;
						break;
					}

					const auto convert_address_mode = [](reshadefx::texture_address_mode value) {
						switch (value)
						{
						case reshadefx::texture_address_mode::wrap:
							return GL_REPEAT;
						case reshadefx::texture_address_mode::mirror:
							return GL_MIRRORED_REPEAT;
						case reshadefx::texture_address_mode::clamp:
							return GL_CLAMP_TO_EDGE;
						case reshadefx::texture_address_mode::border:
							return GL_CLAMP_TO_BORDER;
						default:
							return GL_NONE;
						}
					};

					GLuint sampler_id = 0;
					glGenSamplers(1, &sampler_id);
					glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, convert_address_mode(info.address_u));
					glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, convert_address_mode(info.address_v));
					glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_R, convert_address_mode(info.address_w));
					glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, mag_filter);
					glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, min_filter);
					glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, info.lod_bias);
					glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, info.min_lod);
					glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, info.max_lod);

					it = _effect_sampler_states.emplace(hash, sampler_id).first;
				}

				sampler_data &sampler_data = pass_data.samplers[info.binding];
				sampler_data.id = it->second;
				sampler_data.texture = static_cast<tex_data *>(texture.impl);
				sampler_data.is_srgb_format = info.srgb;
			}
		}
	}

	for (auto &it : entry_points)
		glDeleteShader(it.second);

	return success;
}
void reshade::opengl::runtime_gl::unload_effect(size_t index)
{
	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		const auto impl = static_cast<technique_data *>(tech.impl);
		if (impl == nullptr)
			continue;

		glDeleteQueries(1, &impl->query);

		for (pass_data &pass_data : impl->passes)
		{
			if (pass_data.program)
				glDeleteProgram(pass_data.program);
			glDeleteFramebuffers(1, &pass_data.fbo);
		}

		delete impl;
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);

	if (index < _effect_ubos.size())
	{
		glDeleteBuffers(1, &_effect_ubos[index]);
		_effect_ubos[index] = 0;
	}
}
void reshade::opengl::runtime_gl::unload_effects()
{
	for (technique &tech : _techniques)
	{
		const auto impl = static_cast<technique_data *>(tech.impl);
		if (impl == nullptr)
			continue;

		glDeleteQueries(1, &impl->query);

		for (pass_data &pass_data : impl->passes)
		{
			if (pass_data.program)
				glDeleteProgram(pass_data.program);
			glDeleteFramebuffers(1, &pass_data.fbo);
		}

		delete impl;
		tech.impl = nullptr;
	}

	runtime::unload_effects();

	glDeleteBuffers(static_cast<GLsizei>(_effect_ubos.size()), _effect_ubos.data());
	_effect_ubos.clear();

	for (const auto &info : _effect_sampler_states)
		glDeleteSamplers(1, &info.second);
	_effect_sampler_states.clear();
}

bool reshade::opengl::runtime_gl::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	if (texture.semantic == "COLOR")
	{
		impl->id[0] = _tex[TEX_BACK];
		impl->id[1] = _tex[TEX_BACK_SRGB];
		return true;
	}
	if (texture.semantic == "DEPTH")
	{
#if RESHADE_DEPTH
		impl->id[0] = impl->id[1] =
			_copy_depth_source ? _tex[TEX_DEPTH] : _depth_source;
#endif
		return true;
	}

	GLenum internal_format = GL_RGBA8;
	GLenum internal_format_srgb = GL_NONE;
	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		internal_format = GL_R8;
		break;
	case reshadefx::texture_format::r16f:
		internal_format = GL_R16F;
		break;
	case reshadefx::texture_format::r32f:
		internal_format = GL_R32F;
		break;
	case reshadefx::texture_format::rg8:
		internal_format = GL_RG8;
		break;
	case reshadefx::texture_format::rg16:
		internal_format = GL_RG16;
		break;
	case reshadefx::texture_format::rg16f:
		internal_format = GL_RG16F;
		break;
	case reshadefx::texture_format::rg32f:
		internal_format = GL_RG32F;
		break;
	case reshadefx::texture_format::rgba8:
		internal_format = GL_RGBA8;
		internal_format_srgb = GL_SRGB8_ALPHA8;
		break;
	case reshadefx::texture_format::rgba16:
		internal_format = GL_RGBA16;
		break;
	case reshadefx::texture_format::rgba16f:
		internal_format = GL_RGBA16F;
		break;
	case reshadefx::texture_format::rgba32f:
		internal_format = GL_RGBA32F;
		break;
	case reshadefx::texture_format::rgb10a2:
		internal_format = GL_RGB10_A2;
		break;
	}

	impl->levels = texture.levels;
	impl->internal_format = internal_format;

	// Get current state
	GLint previous_tex = 0;
	GLint previous_draw_buffer = 0;
	GLint previous_frame_buffer = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_tex);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_frame_buffer);
	glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);

	// Allocate texture storage
	glGenTextures(2, impl->id);
	glBindTexture(GL_TEXTURE_2D, impl->id[0]);
	glTexStorage2D(GL_TEXTURE_2D, texture.levels, internal_format, texture.width, texture.height);

	// Only create SRGB texture view if necessary
	if (internal_format_srgb != GL_NONE) {
		glTextureView(impl->id[1], GL_TEXTURE_2D, impl->id[0], internal_format_srgb, 0, texture.levels, 0, 1);
	}
	else {
		impl->id[1] = impl->id[0];
	}

	// Set default minification filter to linear (used during mipmap generation)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	// Clear texture to zero since by default its contents are undefined
	// Use a separate FBO here to make sure there is no mismatch with the dimensions of others
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_CLEAR]);
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, impl->id[0], 0);
	assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	const GLuint clear_color[4] = { 0, 0, 0, 0 };
	glClearBufferuiv(GL_COLOR, 0, clear_color);
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);

	// Restore previous state from application
	glBindTexture(GL_TEXTURE_2D, previous_tex);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previous_frame_buffer);
	glDrawBuffer(previous_draw_buffer);

	return true;
}
void reshade::opengl::runtime_gl::upload_texture(const texture &texture, const uint8_t *pixels)
{
	auto impl = static_cast<tex_data *>(texture.impl);
	assert(impl != nullptr && texture.semantic.empty() && pixels != nullptr);

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
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_tex);
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
	glBindTexture(GL_TEXTURE_2D, impl->id[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	generate_mipmaps(impl);

	// Restore previous state from application
	glBindTexture(GL_TEXTURE_2D, previous_tex);
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
void reshade::opengl::runtime_gl::destroy_texture(texture &texture)
{
	if (texture.impl == nullptr)
		return;
	auto impl = static_cast<tex_data *>(texture.impl);

	if (texture.semantic != "COLOR" && texture.semantic != "DEPTH") {
		glDeleteTextures(impl->id[0] != impl->id[1] ? 2 : 1, impl->id);
	}

	delete impl;
	texture.impl = nullptr;
}
void reshade::opengl::runtime_gl::generate_mipmaps(const tex_data *impl)
{
	if (impl == nullptr || impl->levels <= 1)
		return;

	glBindSampler(0, 0);
	glActiveTexture(GL_TEXTURE0); // src
	glBindTexture(GL_TEXTURE_2D, impl->id[0]);
#if 0
	glGenerateMipmap(GL_TEXTURE_2D);
#else
	// Use custom mipmap generation implementation because 'glGenerateMipmap' generates shifted results
	glUseProgram(_mipmap_program);

	GLuint base_width = 0;
	GLuint base_height = 0;
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, reinterpret_cast<GLint *>(&base_width));
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, reinterpret_cast<GLint *>(&base_height));

	for (GLuint level = 1; level < impl->levels; ++level)
	{
		const GLuint width = std::max(1u, base_width >> level);
		const GLuint height = std::max(1u, base_height >> level);

		glUniform3f(0 /* info */, 1.0f / width, 1.0f / height, static_cast<float>(level - 1));
		glBindImageTexture(1 /* dest */, impl->id[0], level, GL_FALSE, 0, GL_WRITE_ONLY, impl->internal_format);

		glDispatchCompute(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);
	}
#endif
}

void reshade::opengl::runtime_gl::render_technique(technique &technique)
{
	assert(_app_state.has_state);

	const auto impl = static_cast<technique_data *>(technique.impl);

	if (GLuint available = 0; impl->query_in_flight)
	{
		glGetQueryObjectuiv(impl->query, GL_QUERY_RESULT_AVAILABLE, &available);
		if (GLuint64 elapsed_time = 0; available != GL_FALSE)
		{
			glGetQueryObjectui64v(impl->query, GL_QUERY_RESULT, &elapsed_time);
			technique.average_gpu_duration.append(elapsed_time);
			impl->query_in_flight = false; // Reset query status
		}
	}

	if (!impl->query_in_flight) {
		glBeginQuery(GL_TIME_ELAPSED, impl->query);
	}

	// Set up global state
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	if (_compatibility_context)
		glDisable(GL_ALPHA_TEST);
	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask(GL_FALSE); // No need to write to the depth buffer

	// Bind an empty vertex array object
	glBindVertexArray(_vao[VAO_FX]);

	// Set up shader constants
	if (_effect_ubos[technique.effect_index] != 0)
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.effect_index]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, _effects[technique.effect_index].uniform_data_storage.size(), _effects[technique.effect_index].uniform_data_storage.data());
	}

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			// Copy back buffer of previous pass to texture
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[FBO_BACK]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_BLIT]);
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0);
			glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}

		const pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

		glUseProgram(pass_data.program);

		// Set up shader resources
		for (GLuint binding = 0; binding < pass_data.storages.size(); ++binding)
		{
			tex_data *const tex_impl = pass_data.storages[binding];
			if (tex_impl != nullptr)
			{
				glBindImageTexture(binding, tex_impl->id[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, tex_impl->internal_format);
			}
			else
			{
				glBindImageTexture(binding, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
			}
		}
		for (GLuint binding = 0; binding < pass_data.samplers.size(); ++binding)
		{
			const sampler_data &sampler_data = pass_data.samplers[binding];

			glBindSampler(binding, sampler_data.id);
			glActiveTexture(GL_TEXTURE0 + binding);
			glBindTexture(GL_TEXTURE_2D, sampler_data.texture != nullptr ? sampler_data.texture->id[sampler_data.is_srgb_format] : 0);
		}

		if (!pass_info.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_backbuffer_copy = false;

			glDispatchCompute(pass_info.viewport_width, pass_info.viewport_height, pass_info.viewport_dispatch_z);
		}
		else
		{
			// Set up pass specific state
			glViewport(0, 0, static_cast<GLsizei>(pass_info.viewport_width), static_cast<GLsizei>(pass_info.viewport_height));
			glBindFramebuffer(GL_FRAMEBUFFER, pass_data.fbo);
			glDrawBuffers(8, pass_data.draw_targets);

			if (pass_info.blend_enable) {
				glEnable(GL_BLEND);
				glBlendFuncSeparate(pass_data.blend_src, pass_data.blend_dest, pass_data.blend_src_alpha, pass_data.blend_dest_alpha);
				glBlendEquationSeparate(pass_data.blend_eq_color, pass_data.blend_eq_alpha);
			}
			else {
				glDisable(GL_BLEND);
			}

			if (pass_info.stencil_enable) {
				glEnable(GL_STENCIL_TEST);
				glStencilOp(pass_data.stencil_op_fail, pass_data.stencil_op_z_fail, pass_data.stencil_op_z_pass);
				glStencilMask(pass_info.stencil_write_mask);
				glStencilFunc(pass_data.stencil_func, pass_info.stencil_reference_value, pass_info.stencil_read_mask);
			}
			else {
				glDisable(GL_STENCIL_TEST);
			}

			if (pass_info.stencil_enable && !is_effect_stencil_cleared)
			{
				is_effect_stencil_cleared = true;

				GLint clear_value = 0;
				glClearBufferiv(GL_STENCIL, 0, &clear_value);
			}

			if (pass_info.srgb_write_enable) {
				glEnable(GL_FRAMEBUFFER_SRGB);
			}
			else {
				glDisable(GL_FRAMEBUFFER_SRGB);
			}

			glColorMask(
				(pass_info.color_write_mask & (1 << 0)) != 0,
				(pass_info.color_write_mask & (1 << 1)) != 0,
				(pass_info.color_write_mask & (1 << 2)) != 0,
				(pass_info.color_write_mask & (1 << 3)) != 0);

			if (pass_info.clear_render_targets)
			{
				for (GLuint k = 0; k < 8; k++)
				{
					if (pass_data.draw_targets[k] == GL_NONE)
						break; // Ignore unbound render targets
					const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					glClearBufferfv(GL_COLOR, k, color);
				}
			}

			// Draw primitives
			GLenum topology;
			switch (pass_info.topology)
			{
			case reshadefx::primitive_topology::point_list:
				topology = GL_POINTS;
				break;
			case reshadefx::primitive_topology::line_list:
				topology = GL_LINES;
				break;
			case reshadefx::primitive_topology::line_strip:
				topology = GL_LINE_STRIP;
				break;
			default:
			case reshadefx::primitive_topology::triangle_list:
				topology = GL_TRIANGLES;
				break;
			case reshadefx::primitive_topology::triangle_strip:
				topology = GL_TRIANGLE_STRIP;
				break;
			}
			glDrawArrays(topology, 0, pass_info.num_vertices);

			_vertices += pass_info.num_vertices;
			_drawcalls += 1;

			needs_implicit_backbuffer_copy = pass_info.render_target_names[0].empty();
		}

		// Generate mipmaps for modified resources (graphics passes add their render targets to the 'storages' list)
		for (tex_data *const tex_impl : pass_data.storages)
			generate_mipmaps(tex_impl);
	}

	if (!impl->query_in_flight) {
		glEndQuery(GL_TIME_ELAPSED);
	}

	impl->query_in_flight = true;
}

#if RESHADE_GUI
void reshade::opengl::runtime_gl::init_imgui_resources()
{
	assert(_app_state.has_state);

	const GLchar *vertex_shader[] = {
		"#version 430\n"
		"layout(location = 1) uniform mat4 proj;\n"
		"layout(location = 0) in vec2 pos;\n"
		"layout(location = 1) in vec2 tex;\n"
		"layout(location = 2) in vec4 col;\n"
		"out vec4 frag_col;\n"
		"out vec2 frag_tex;\n"
		"void main()\n"
		"{\n"
		"	frag_col = col;\n"
		"	frag_tex = tex;\n"
		"	gl_Position = proj * vec4(pos.xy, 0, 1);\n"
		"}\n"
	};
	const GLchar *fragment_shader[] = {
		"#version 430\n"
		"layout(binding = 0) uniform sampler2D s0;\n"
		"in vec4 frag_col;\n"
		"in vec2 frag_tex;\n"
		"out vec4 col;\n"
		"void main()\n"
		"{\n"
		"	col = frag_col * texture(s0, frag_tex.st);\n"
		"}\n"
	};

	const GLuint imgui_vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(imgui_vs, 1, vertex_shader, 0);
	glCompileShader(imgui_vs);
	const GLuint imgui_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(imgui_fs, 1, fragment_shader, 0);
	glCompileShader(imgui_fs);

	_imgui.program = glCreateProgram();
	glAttachShader(_imgui.program, imgui_vs);
	glAttachShader(_imgui.program, imgui_fs);
	 glLinkProgram(_imgui.program);
	glDeleteShader(imgui_vs);
	glDeleteShader(imgui_fs);
}

void reshade::opengl::runtime_gl::render_imgui_draw_data(ImDrawData *draw_data)
{
	assert(_app_state.has_state);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	if (_compatibility_context)
		glDisable(GL_ALPHA_TEST);
	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);

	glViewport(0, 0, GLsizei(draw_data->DisplaySize.x), GLsizei(draw_data->DisplaySize.y));

	glUseProgram(_imgui.program);
	glBindSampler(0, 0); // Do not use separate sampler object, since state is already set in texture
	glActiveTexture(GL_TEXTURE0); // s0

	const float ortho_projection[16] = {
		2.0f / draw_data->DisplaySize.x, 0.0f,   0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y,  0.0f, 0.0f,
		0.0f,                            0.0f,  -1.0f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x) / draw_data->DisplaySize.x,
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y) / draw_data->DisplaySize.y, 0.0f, 1.0f,
	};
	glUniformMatrix4fv(1 /* proj */, 1, GL_FALSE, ortho_projection);

	glBindVertexArray(_vao[VAO_IMGUI]);
	// Need to rebuild vertex array object every frame
	// Doing so fixes weird interaction with 'glEnableClientState' and 'glVertexPointer' (e.g. in the first Call of Duty)
	glBindVertexBuffer(0, _buf[VBO_IMGUI], 0, sizeof(ImDrawVert));
	glEnableVertexAttribArray(0 /* pos */);
	glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, pos));
	glVertexAttribBinding(0, 0);
	glEnableVertexAttribArray(1 /* tex */);
	glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, uv ));
	glVertexAttribBinding(1, 0);
	glEnableVertexAttribArray(2 /* col */);
	glVertexAttribFormat(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, col));
	glVertexAttribBinding(2, 0);

	glBindBuffer(GL_ARRAY_BUFFER, _buf[VBO_IMGUI]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buf[IBO_IMGUI]);

	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		ImDrawList *const draw_list = draw_data->CmdLists[n];

		glBufferData(GL_ARRAY_BUFFER, draw_list->VtxBuffer.Size * sizeof(ImDrawVert), draw_list->VtxBuffer.Data, GL_STREAM_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx), draw_list->IdxBuffer.Data, GL_STREAM_DRAW);

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const ImVec4 scissor_rect(
				cmd.ClipRect.x - draw_data->DisplayPos.x,
				cmd.ClipRect.y - draw_data->DisplayPos.y,
				cmd.ClipRect.z - draw_data->DisplayPos.x,
				cmd.ClipRect.w - draw_data->DisplayPos.y);
			glScissor(
				static_cast<GLint>(scissor_rect.x),
				static_cast<GLint>(_height - scissor_rect.w),
				static_cast<GLint>(scissor_rect.z - scissor_rect.x),
				static_cast<GLint>(scissor_rect.w - scissor_rect.y));

			glBindTexture(GL_TEXTURE_2D,
				static_cast<const tex_data *>(cmd.TextureId)->id[0]);

			glDrawElementsBaseVertex(GL_TRIANGLES, cmd.ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
				reinterpret_cast<const void *>(static_cast<uintptr_t>(cmd.IdxOffset * sizeof(ImDrawIdx))), cmd.VtxOffset);
		}
	}
}
#endif

#if RESHADE_DEPTH
void reshade::opengl::runtime_gl::draw_depth_debug_menu()
{
	if (!ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (_has_high_network_activity)
	{
		ImGui::TextColored(ImColor(204, 204, 0), "High network activity discovered.\nAccess to depth buffers is disabled to prevent exploitation.");
		return;
	}

	bool modified = false;
	modified |= ImGui::Checkbox("Use aspect ratio heuristics", &_state_tracking.use_aspect_ratio_heuristics);
	modified |= ImGui::Checkbox("Copy depth buffer before clear operations", &_state_tracking.preserve_depth_buffers);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Sort object list so that added/removed items do not change the UI much
	std::vector<std::pair<GLuint, state_tracking::depthstencil_info>> sorted_buffers;
	sorted_buffers.reserve(_state_tracking.depth_buffer_counters().size());
	for (const auto &[depth_source, snapshot] : _state_tracking.depth_buffer_counters())
		sorted_buffers.push_back({ depth_source, snapshot });
	std::sort(sorted_buffers.begin(), sorted_buffers.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	for (const auto &[depth_source, snapshot] : sorted_buffers)
	{
		if (snapshot.format == GL_NONE)
			continue; // Skip invalid entries

		char label[512] = "";
		sprintf_s(label, "%s0x%08x", (depth_source == _state_tracking.depthstencil_clear_index.first && !_has_high_network_activity ? "> " : "  "), depth_source);

		if (bool value = _depth_source_override == depth_source;
			ImGui::Checkbox(label, &value))
			_depth_source_override = value ? depth_source : std::numeric_limits<GLuint>::max();

		ImGui::SameLine();
		ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
			snapshot.width, snapshot.height, snapshot.total_stats.drawcalls, snapshot.total_stats.vertices,
			(depth_source & 0x80000000) != 0 ? " RBO" : depth_source != 0 ? " FBO" : "");

		if (_state_tracking.preserve_depth_buffers && depth_source == _state_tracking.depthstencil_clear_index.first)
		{
			for (UINT clear_index = 1; clear_index <= snapshot.clears.size(); ++clear_index)
			{
				sprintf_s(label, "%s  CLEAR %2u", (clear_index == _state_tracking.depthstencil_clear_index.second ? "> " : "  "), clear_index);

				if (bool value = (_state_tracking.depthstencil_clear_index.second == clear_index);
					ImGui::Checkbox(label, &value))
				{
					_state_tracking.depthstencil_clear_index.second = value ? clear_index : 0;
					modified = true;
				}

				ImGui::SameLine();
				ImGui::Text("|           | %5u draw calls ==> %8u vertices |",
					snapshot.clears[clear_index - 1].drawcalls, snapshot.clears[clear_index - 1].vertices);
			}
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (modified)
		runtime::save_config();
}

void reshade::opengl::runtime_gl::update_depth_texture_bindings(state_tracking::depthstencil_info info)
{
	if (_has_high_network_activity)
	{
		_depth_source = 0;
		_has_depth_texture = false;
		_copy_depth_source = false;

		if (_tex[TEX_DEPTH])
		{
			glDeleteTextures(1, &_tex[TEX_DEPTH]);
			_tex[TEX_DEPTH] = 0;

			for (const texture &tex : _textures)
			{
				if (tex.impl == nullptr || tex.semantic != "DEPTH")
					continue;
				const auto tex_impl = static_cast<tex_data *>(tex.impl);

				tex_impl->id[0] = tex_impl->id[1] = 0;
			}
		}
		return;
	}

	const GLuint source = info.obj | (info.target == GL_RENDERBUFFER ? 0x80000000 : 0);
	if (_tex[TEX_DEPTH] &&
		source == _depth_source)
		return;
	_depth_source = source;

	// Convert depth formats to internal texture formats
	switch (info.format)
	{
	case GL_NONE:
		_has_depth_texture = false;
		return; // Skip invalid entries (e.g. default frame buffer if pixel format has no depth)
	case GL_DEPTH_STENCIL:
		info.format = GL_DEPTH24_STENCIL8;
		break;
	case GL_DEPTH_COMPONENT:
		info.format = GL_DEPTH_COMPONENT24;
		break;
	}

	_has_depth_texture = true;

	// Can just use source directly if it is a simple depth texture already
	if (info.target != GL_TEXTURE_2D || info.level != 0 || _depth_source == 0)
	{
		assert(_app_state.has_state);

		if (_tex[TEX_DEPTH] == 0 ||
			// Resize depth texture if dimensions have changed
			_depth_source_width != info.width || _depth_source_height != info.height || _depth_source_format != info.format)
		{
			// Recreate depth texture (since the storage is immutable after the first call to glTexStorage)
			glDeleteTextures(1, &_tex[TEX_DEPTH]); glGenTextures(1, &_tex[TEX_DEPTH]);

			glBindTexture(GL_TEXTURE_2D, _tex[TEX_DEPTH]);
			glTexStorage2D(GL_TEXTURE_2D, 1, info.format, info.width, info.height);

			if (GLenum err = glGetError(); err != GL_NO_ERROR)
			{
				glDeleteTextures(1, &_tex[TEX_DEPTH]);
				_tex[TEX_DEPTH] = 0;

				_has_depth_texture = false;
				_depth_source_width = _depth_source_height = 0;
				_depth_source_format = GL_NONE;

				LOG(ERROR) << "Failed to create depth texture of format " << std::hex << info.format << " with error code " << err << std::dec << '!';
				return;
			}

			_depth_source_width = info.width;
			_depth_source_height = info.height;
			_depth_source_format = info.format;

			glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_DEPTH_DEST]);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _tex[TEX_DEPTH], 0);
			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		}

		if (_depth_source != 0)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_DEPTH_SRC]);

			if (info.target != GL_RENDERBUFFER) {
				glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, info.obj, info.level);
			}
			else {
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, info.target, info.obj);
			}

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		}

		info.obj = _tex[TEX_DEPTH];
		_copy_depth_source = true;
	}
	else
	{
		_copy_depth_source = false;
	}

	// Update all references to the new texture
	for (const texture &tex : _textures)
	{
		if (tex.impl == nullptr || tex.semantic != "DEPTH")
			continue;
		const auto tex_impl = static_cast<tex_data *>(tex.impl);

		tex_impl->id[0] = tex_impl->id[1] = info.obj;
	}
}
#endif
