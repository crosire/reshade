/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_config.hpp"
#include "runtime_gl.hpp"
#include "runtime_gl_objects.hpp"

reshade::opengl::runtime_gl::runtime_gl(HDC hdc) : device_impl(hdc)
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

	subscribe_to_load_config([this](const ini_file &config) {
		// Reserve a fixed amount of texture names by default to work around issues in old OpenGL games (which will use a compatibility context)
		auto num_reserve_texture_names = _compatibility_context ? 512u : 0u;
		config.get("APP", "ReserveTextureNames", num_reserve_texture_names);
		_reserved_texture_names.resize(num_reserve_texture_names);
	});

#ifndef NDEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](unsigned int /*source*/, unsigned int type, unsigned int /*id*/, unsigned int /*severity*/, int /*length*/, const char *message, const void */*userParam*/) {
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
		}, nullptr);
#endif
}
reshade::opengl::runtime_gl::~runtime_gl()
{
	on_reset();

#if RESHADE_GUI
	glDeleteProgram(_imgui.program);
	_imgui.program = 0;
#endif

	glDeleteProgram(_mipmap_program);
	_mipmap_program = 0;
}

bool reshade::opengl::runtime_gl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);

	const HDC hdc = GetDC(hwnd);
	PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
	DescribePixelFormat(hdc, GetPixelFormat(hdc), sizeof(pfd), &pfd);

	_width = _default_fbo_width = width;
	_height = _default_fbo_height = height;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_color_bit_depth = std::min(pfd.cRedBits, std::min(pfd.cGreenBits, pfd.cBlueBits));

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

	if (_mipmap_program == 0)
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

#if RESHADE_GUI
	init_imgui_resources();
#endif

	_app_state.apply(_compatibility_context);

	return runtime::on_init(hwnd);
}
void reshade::opengl::runtime_gl::on_reset()
{
	runtime::on_reset();

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
}

void reshade::opengl::runtime_gl::on_present()
{
	if (!_is_initialized)
		return;

	_app_state.capture(_compatibility_context);

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
	else if (!texture.semantic.empty())
	{
		if (const auto it = _texture_semantic_bindings.find(texture.semantic);
			it != _texture_semantic_bindings.end())
			impl->id[0] = impl->id[1] = it->second;
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

	RESHADE_ADDON_EVENT(reshade_before_effects, this, this);

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

			needs_implicit_backbuffer_copy = pass_info.render_target_names[0].empty();
		}

		// Generate mipmaps for modified resources (graphics passes add their render targets to the 'storages' list)
		for (tex_data *const tex_impl : pass_data.storages)
			generate_mipmaps(tex_impl);
	}

	RESHADE_ADDON_EVENT(reshade_after_effects, this, this);

	if (!impl->query_in_flight) {
		glEndQuery(GL_TIME_ELAPSED);
	}

	impl->query_in_flight = true;
}

void reshade::opengl::runtime_gl::update_texture_bindings(const char *semantic, api::resource_view_handle srv)
{
	const GLuint object = srv.handle & 0xFFFFFFFF;

	_texture_semantic_bindings[semantic] = object;

	// Update all references to the new texture
	for (const texture &tex : _textures)
	{
		if (tex.impl == nullptr || tex.semantic != semantic)
			continue;
		const auto tex_impl = static_cast<tex_data *>(tex.impl);

		tex_impl->id[0] = tex_impl->id[1] = object;
	}
}
