/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_gl.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include <imgui.h>

namespace reshade::opengl
{
	struct opengl_tex_data : base_object
	{
		~opengl_tex_data()
		{
			if (should_delete)
				glDeleteTextures(id[0] != id[1] ? 2 : 1, id);
		}

		bool should_delete = false;
		GLuint id[2] = {};
	};

	struct opengl_sampler_data
	{
		GLuint id;
		opengl_tex_data *texture;
		bool is_srgb;
		bool has_mipmaps;
	};

	struct opengl_pass_data : base_object
	{
		~opengl_pass_data()
		{
			if (program)
				glDeleteProgram(program);
			glDeleteFramebuffers(1, &fbo);
		}

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
		GLuint draw_textures[8] = {};
		GLsizei viewport_width = 0;
		GLsizei viewport_height = 0;
	};

	struct opengl_technique_data : base_object
	{
		~opengl_technique_data()
		{
			glDeleteQueries(1, &query);
		}

		GLuint query = 0;
		bool query_in_flight = false;
		std::vector<opengl_sampler_data> samplers;
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
			// Format: PCI\VEN_XXXX&DEV_XXXX...
			const std::string id = dd.DeviceID;

			if (id.length() > 20)
			{
				_vendor_id = std::stoi(id.substr(8, 4), nullptr, 16);
				_device_id = std::stoi(id.substr(17, 4), nullptr, 16);
			}
			break;
		}
	}

#if RESHADE_GUI && RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
	subscribe_to_ui("OpenGL", [this]() { draw_depth_debug_menu(); });
#endif
	subscribe_to_load_config([this](const ini_file &config) {
		// Reserve a fixed amount of texture names by default to work around issues in old OpenGL games
		// This hopefully should not affect performance much in other games
		auto num_reserve_texture_names = 512u;
		config.get("OPENGL", "ReserveTextureNames", num_reserve_texture_names);
		_reserved_texture_names.resize(num_reserve_texture_names);

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
		auto force_default_depth_override = false;
		config.get("OPENGL", "ForceMainDepthBuffer", force_default_depth_override);
		config.get("OPENGL", "UseAspectRatioHeuristics", _use_aspect_ratio_heuristics);

		if (force_default_depth_override)
			_depth_source_override = 0; // Zero has a special meaning and corresponds to the default depth buffer
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("OPENGL", "ForceMainDepthBuffer", _depth_source_override == 0);
		config.set("OPENGL", "UseAspectRatioHeuristics", _use_aspect_ratio_heuristics);
#endif
	});

#ifdef _DEBUG
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
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_color_bit_depth = pfd.cRedBits;

	switch (pfd.cDepthBits)
	{
	case 16: _default_depth_format = GL_DEPTH_COMPONENT16;
		break;
	case 24: _default_depth_format = pfd.cStencilBits ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
		break;
	case 32: _default_depth_format = pfd.cStencilBits ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32;
		break;
	}

	// Initialize default depth buffer information
	_buffer_detection.reset(_width, _height, _default_depth_format);

	// Capture and later restore so that the resource creation code below does not affect the application state
	_app_state.capture();

	// Some games (like Hot Wheels Velocity X) use fixed texture names, which can clash with the ones ReShade generates below, since most implementations will return values linearly
	// Reserve a configurable range of names for those games to work around this
	glGenTextures(GLsizei(_reserved_texture_names.size()), _reserved_texture_names.data());

	glGenBuffers(NUM_BUF, _buf);
	glGenTextures(NUM_TEX, _tex);
	glGenVertexArrays(NUM_VAO, _vao);
	glGenFramebuffers(NUM_FBO, _fbo);
	glGenRenderbuffers(NUM_RBO, _rbo);

	glBindTexture(GL_TEXTURE_2D, _tex[TEX_BACK]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, _width, _height);
	glTextureView(_tex[TEX_BACK_SRGB], GL_TEXTURE_2D, _tex[TEX_BACK], GL_SRGB8_ALPHA8, 0, 1, 0, 1);

	// Initialize depth texture and FBO by assuming they refer to the default depth source
	_depth_source_width = _width;
	_depth_source_height = _height;
	_depth_source_format = _default_depth_format;
	glBindTexture(GL_TEXTURE_2D, _tex[TEX_DEPTH]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, _depth_source_width, _depth_source_height);

	glBindRenderbuffer(GL_RENDERBUFFER, _rbo[RBO_COLOR]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, _width, _height);
	glBindRenderbuffer(GL_RENDERBUFFER, _rbo[RBO_DEPTH]);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_BACK]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo[RBO_COLOR]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo[RBO_DEPTH]);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_BLIT]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _tex[TEX_BACK_SRGB], 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_DEPTH_DEST]);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _tex[TEX_DEPTH], 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

#if RESHADE_GUI
	init_imgui_resources();
#endif

	_app_state.apply();

	return runtime::on_init(hwnd);
}
void reshade::opengl::runtime_gl::on_reset()
{
	runtime::on_reset();

	glDeleteBuffers(NUM_BUF, _buf);
	glDeleteTextures(NUM_TEX, _tex);
	glDeleteTextures(GLsizei(_reserved_texture_names.size()), _reserved_texture_names.data());
	glDeleteVertexArrays(NUM_VAO, _vao);
	glDeleteFramebuffers(NUM_FBO, _fbo);
	glDeleteRenderbuffers(NUM_RBO, _rbo);

	std::memset(_buf, 0, sizeof(_vao));
	std::memset(_tex, 0, sizeof(_tex));
	std::memset(_vao, 0, sizeof(_vao));
	std::memset(_fbo, 0, sizeof(_fbo));
	std::memset(_rbo, 0, sizeof(_rbo));

#if RESHADE_GUI
	glDeleteProgram(_imgui_program);
	_imgui_program = 0;
#endif

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
	_depth_source = 0;
	_depth_source_width = 0;
	_depth_source_height = 0;
	_depth_source_format = 0;
	_depth_source_override = std::numeric_limits<GLuint>::max();
	_default_depth_format = GL_NONE;
#endif
}

void reshade::opengl::runtime_gl::on_present()
{
	if (!_is_initialized)
		return;

	_vertices = _buffer_detection.total_vertices();
	_drawcalls = _buffer_detection.total_drawcalls();

	_app_state.capture();

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
	update_depthstencil_texture(
		_buffer_detection.find_best_depth_texture(_use_aspect_ratio_heuristics ? _width : 0, _height, _depth_source_override));
#endif

	// Copy back buffer to RBO
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_BACK]);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	if (_depth_source == 0 || (_depth_source & 0x80000000) != 0)
	{
		// Copy depth from FBO to depth texture
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _depth_source == 0 ? 0 : _fbo[FBO_DEPTH_SRC]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_DEPTH_DEST]);
		glBlitFramebuffer(0, 0, _depth_source_width, _depth_source_height, 0, 0, _depth_source_width, _depth_source_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	update_and_render_effects();

	// Copy results from RBO to back buffer
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[FBO_BACK]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	runtime::on_present();

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
	_buffer_detection.reset(_width, _height, _default_depth_format);
#endif

	// Apply previous state from application
	_app_state.apply();
}

bool reshade::opengl::runtime_gl::capture_screenshot(uint8_t *buffer) const
{
	assert(_app_state.has_state); // Can only call this while rendering to FBO_BACK

	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[FBO_BACK]);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, GLsizei(_width), GLsizei(_height), GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	// Flip image horizontally
	for (unsigned int y = 0, pitch = _width * 4; y * 2 < _height; ++y)
	{
		const auto i1 = y * pitch;
		const auto i2 = (_height - 1 - y) * pitch;

		for (unsigned int x = 0; x < pitch; x += 4)
		{
			buffer[i1 + x + 3] = 0xFF; // Clear alpha channel
			buffer[i2 + x + 3] = 0xFF;

			std::swap(buffer[i1 + x + 0], buffer[i2 + x + 0]);
			std::swap(buffer[i1 + x + 1], buffer[i2 + x + 1]);
			std::swap(buffer[i1 + x + 2], buffer[i2 + x + 2]);
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
	else
	{
		effect.preamble = "#version 430\n" + effect.preamble;
	}

	std::unordered_map<std::string, GLuint> entry_points;

	// Compile all entry points
	for (const auto &entry_point : effect.module.entry_points)
	{
		GLuint shader_id = glCreateShader(entry_point.is_pixel_shader ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
		entry_points[entry_point.name] = shader_id;

		if (!effect.module.spirv.empty())
		{
			assert(_renderer_id >= 0x14600); // Core since OpenGL 4.6 (see https://www.khronos.org/opengl/wiki/SPIR-V)
			assert(gl3wProcs.gl.ShaderBinary && gl3wProcs.gl.SpecializeShader);

			glShaderBinary(1, &shader_id, GL_SHADER_BINARY_FORMAT_SPIR_V, effect.module.spirv.data(), static_cast<GLsizei>(effect.module.spirv.size() * sizeof(uint32_t)));
			glSpecializeShader(shader_id, entry_point.name.c_str(), GLuint(spec_constants.size()), spec_constants.data(), spec_data.data());
		}
		else
		{
			std::string defines = effect.preamble;
			defines += "#define ENTRY_POINT_" + entry_point.name + " 1\n";
			if (!entry_point.is_pixel_shader) // OpenGL does not allow using 'discard' in the vertex shader profile
				defines += "#define discard\n"
				"#define dFdx(x) x\n" // 'dFdx', 'dFdx' and 'fwidth' too are only available in fragment shaders
				"#define dFdy(y) y\n"
				"#define fwidth(p) p\n";

			GLsizei lengths[] = { static_cast<GLsizei>(defines.size()), static_cast<GLsizei>(effect.module.hlsl.size()) };
			const GLchar *sources[] = { defines.c_str(), effect.module.hlsl.c_str() };
			glShaderSource(shader_id, 2, sources, lengths);
			glCompileShader(shader_id);
		}

		GLint status = GL_FALSE;
		glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
		if (GL_FALSE == status)
		{
			GLint log_size = 0;
			glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_size);
			std::string log(log_size, '\0');
			glGetShaderInfoLog(shader_id, log_size, nullptr, log.data());

			effect.errors += log;

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

	opengl_technique_data technique_init;

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		if (existing_texture == _textures.end())
		{
			success = false;
			continue;
		}

		// Hash sampler state to avoid duplicated sampler objects
		size_t hash = 2166136261;
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_u);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_v);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.address_w);
		hash = (hash * 16777619) ^ static_cast<uint32_t>(info.filter);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.lod_bias);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.min_lod);
		hash = (hash * 16777619) ^ reinterpret_cast<const uint32_t &>(info.max_lod);

		auto it = _effect_sampler_states.find(hash);

		if (it == _effect_sampler_states.end())
		{
			GLenum minfilter = GL_NONE, magfilter = GL_NONE;

			switch (info.filter)
			{
			case reshadefx::texture_filter::min_mag_mip_point:
				minfilter = GL_NEAREST_MIPMAP_NEAREST;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_mag_point_mip_linear:
				minfilter = GL_NEAREST_MIPMAP_LINEAR;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_point_mag_linear_mip_point:
				minfilter = GL_NEAREST_MIPMAP_NEAREST;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_point_mag_mip_linear:
				minfilter = GL_NEAREST_MIPMAP_LINEAR;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_linear_mag_mip_point:
				minfilter = GL_LINEAR_MIPMAP_NEAREST;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_linear_mag_point_mip_linear:
				minfilter = GL_LINEAR_MIPMAP_LINEAR;
				magfilter = GL_NEAREST;
				break;
			case reshadefx::texture_filter::min_mag_linear_mip_point:
				minfilter = GL_LINEAR_MIPMAP_NEAREST;
				magfilter = GL_LINEAR;
				break;
			case reshadefx::texture_filter::min_mag_mip_linear:
				minfilter = GL_LINEAR_MIPMAP_LINEAR;
				magfilter = GL_LINEAR;
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
			glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, magfilter);
			glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, minfilter);
			glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, info.lod_bias);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, info.min_lod);
			glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, info.max_lod);

			it = _effect_sampler_states.emplace(hash, sampler_id).first;
		}

		opengl_sampler_data sampler;
		sampler.id = it->second;
		sampler.texture = existing_texture->impl->as<opengl_tex_data>();
		sampler.is_srgb = info.srgb;
		sampler.has_mipmaps = existing_texture->levels > 1;

		technique_init.samplers.resize(std::max(technique_init.samplers.size(), size_t(info.binding + 1)));

		technique_init.samplers[info.binding] = std::move(sampler);
	}

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		technique.impl = std::make_unique<opengl_technique_data>(technique_init);
		const auto technique_data = technique.impl->as<opengl_technique_data>();

		glGenQueries(1, &technique_data->query);

		for (size_t i = 0; i < technique.passes.size(); ++i)
		{
			technique.passes_data.push_back(std::make_unique<opengl_pass_data>());

			auto &pass = *technique.passes_data.back()->as<opengl_pass_data>();
			const auto &pass_info = technique.passes[i];

			const auto literal_to_blend_eq = [](unsigned int value) -> GLenum {
				switch (value)
				{
				default:
				case 1: return GL_FUNC_ADD;
				case 2: return GL_FUNC_SUBTRACT;
				case 3: return GL_FUNC_REVERSE_SUBTRACT;
				case 4: return GL_MIN;
				case 5: return GL_MAX;
				}
			};
			const auto literal_to_blend_func = [](unsigned int value) -> GLenum {
				switch (value)
				{
				default:
				case 0: return GL_ZERO;
				case 1: return GL_ONE;
				case 2: return GL_SRC_COLOR;
				case 3: return GL_SRC_ALPHA;
				case 4: return GL_ONE_MINUS_SRC_COLOR;
				case 5: return GL_ONE_MINUS_SRC_ALPHA;
				case 8: return GL_DST_COLOR;
				case 6: return GL_DST_ALPHA;
				case 9: return GL_ONE_MINUS_DST_COLOR;
				case 7: return GL_ONE_MINUS_DST_ALPHA;
				}
			};
			const auto literal_to_comp_func = [](unsigned int value) -> GLenum {
				switch (value)
				{
				default:
				case 8: return GL_ALWAYS;
				case 1: return GL_NEVER;
				case 3: return GL_EQUAL;
				case 6: return GL_NOTEQUAL;
				case 2: return GL_LESS;
				case 4: return GL_LEQUAL;
				case 5: return GL_GREATER;
				case 7: return GL_GEQUAL;
				}
			};
			const auto literal_to_stencil_op = [](unsigned int value) -> GLenum {
				switch (value)
				{
				default:
				case 1: return GL_KEEP;
				case 0: return GL_ZERO;
				case 3: return GL_REPLACE;
				case 7: return GL_INCR_WRAP;
				case 4: return GL_INCR;
				case 8: return GL_DECR_WRAP;
				case 5: return GL_DECR;
				case 6: return GL_INVERT;
				}
			};

			pass.blend_eq_color = literal_to_blend_eq(pass_info.blend_op);
			pass.blend_eq_alpha = literal_to_blend_eq(pass_info.blend_op_alpha);
			pass.blend_src = literal_to_blend_func(pass_info.src_blend);
			pass.blend_dest = literal_to_blend_func(pass_info.dest_blend);
			pass.blend_src_alpha = literal_to_blend_func(pass_info.src_blend_alpha);
			pass.blend_dest_alpha = literal_to_blend_func(pass_info.dest_blend_alpha);
			pass.stencil_func = literal_to_comp_func(pass_info.stencil_comparison_func);
			pass.stencil_op_z_pass = literal_to_stencil_op(pass_info.stencil_op_pass);
			pass.stencil_op_fail = literal_to_stencil_op(pass_info.stencil_op_fail);
			pass.stencil_op_z_fail = literal_to_stencil_op(pass_info.stencil_op_depth_fail);

			glGenFramebuffers(1, &pass.fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo);

			bool backbuffer_fbo = true;

			for (unsigned int k = 0; k < 8; ++k)
			{
				if (pass_info.render_target_names[k].empty())
					continue; // Skip unbound render targets

				const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				});
				assert(render_target_texture != _textures.end());

				backbuffer_fbo = false;

				const auto texture_impl = render_target_texture->impl->as<opengl_tex_data>();
				assert(texture_impl != nullptr);

				glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + k, texture_impl->id[pass_info.srgb_write_enable], 0);

				pass.draw_targets[k] = GL_COLOR_ATTACHMENT0 + k;
				pass.draw_textures[k] = texture_impl->id[pass_info.srgb_write_enable];
			}

			if (backbuffer_fbo)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo[RBO_COLOR]);

				pass.draw_targets[0] = GL_COLOR_ATTACHMENT0;
				pass.draw_textures[0] = _tex[TEX_BACK_SRGB];

				pass.viewport_width = static_cast<GLsizei>(_width);
				pass.viewport_height = static_cast<GLsizei>(_height);
			}
			else
			{
				// Effect compiler sets the viewport to the render target dimensions
				pass.viewport_width = pass_info.viewport_width;
				pass.viewport_height = pass_info.viewport_height;
			}

			assert(pass.viewport_width != 0);
			assert(pass.viewport_height != 0);

			if (pass.viewport_width == GLsizei(_width) && pass.viewport_height == GLsizei(_height))
			{
				// Only attach depth-stencil when viewport matches back buffer or else the frame buffer will always be resized to those dimensions
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _rbo[RBO_DEPTH]);
			}

			assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

			// Link program from input shaders
			pass.program = glCreateProgram();
			const GLuint vs_shader_id = entry_points.at(pass_info.vs_entry_point);
			const GLuint fs_shader_id = entry_points.at(pass_info.ps_entry_point);
			glAttachShader(pass.program, vs_shader_id);
			glAttachShader(pass.program, fs_shader_id);
			glLinkProgram(pass.program);
			glDetachShader(pass.program, vs_shader_id);
			glDetachShader(pass.program, fs_shader_id);

			GLint status = GL_FALSE;
			glGetProgramiv(pass.program, GL_LINK_STATUS, &status);
			if (GL_FALSE == status)
			{
				GLint log_size = 0;
				glGetProgramiv(pass.program, GL_INFO_LOG_LENGTH, &log_size);
				std::string log(log_size, '\0');
				glGetProgramInfoLog(pass.program, log_size, nullptr, log.data());

				effect.errors += log;

				LOG(ERROR) << "Failed to link program for pass " << i << " in technique '" << technique.name << "'.";
				success = false;
				break;
			}
		}
	}

	for (auto &it : entry_points)
		glDeleteShader(it.second);

	return success;
}
void reshade::opengl::runtime_gl::unload_effect(size_t index)
{
	runtime::unload_effect(index);

	if (index < _effect_ubos.size())
	{
		glDeleteBuffers(1, &_effect_ubos[index]);
		_effect_ubos[index] = 0;
	}
}
void reshade::opengl::runtime_gl::unload_effects()
{
	runtime::unload_effects();

	glDeleteBuffers(static_cast<GLsizei>(_effect_ubos.size()), _effect_ubos.data());
	_effect_ubos.clear();

	for (const auto &info : _effect_sampler_states)
		glDeleteSamplers(1, &info.second);
	_effect_sampler_states.clear();
}

bool reshade::opengl::runtime_gl::init_texture(texture &texture)
{
	texture.impl = std::make_unique<opengl_tex_data>();
	const auto impl = texture.impl->as<opengl_tex_data>();

	switch (texture.impl_reference)
	{
	case texture_reference::back_buffer:
		impl->id[0] = _tex[TEX_BACK];
		impl->id[1] = _tex[TEX_BACK_SRGB];
		return true;
	case texture_reference::depth_buffer:
		impl->id[0] = impl->id[1] =
			_depth_source == 0 || (_depth_source & 0x80000000) != 0 ? _tex[TEX_DEPTH] : _depth_source;
		return true;
	}

	GLenum internalformat = GL_RGBA8;
	GLenum internalformat_srgb = GL_NONE;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		internalformat = GL_R8;
		break;
	case reshadefx::texture_format::r16f:
		internalformat = GL_R16F;
		break;
	case reshadefx::texture_format::r32f:
		internalformat = GL_R32F;
		break;
	case reshadefx::texture_format::rg8:
		internalformat = GL_RG8;
		break;
	case reshadefx::texture_format::rg16:
		internalformat = GL_RG16;
		break;
	case reshadefx::texture_format::rg16f:
		internalformat = GL_RG16F;
		break;
	case reshadefx::texture_format::rg32f:
		internalformat = GL_RG32F;
		break;
	case reshadefx::texture_format::rgba8:
		internalformat = GL_RGBA8;
		internalformat_srgb = GL_SRGB8_ALPHA8;
		break;
	case reshadefx::texture_format::rgba16:
		internalformat = GL_RGBA16;
		break;
	case reshadefx::texture_format::rgba16f:
		internalformat = GL_RGBA16F;
		break;
	case reshadefx::texture_format::rgba32f:
		internalformat = GL_RGBA32F;
		break;
	case reshadefx::texture_format::rgb10a2:
		internalformat = GL_RGB10_A2;
		break;
	}

	impl->should_delete = true;

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
	glTexStorage2D(GL_TEXTURE_2D, texture.levels, internalformat, texture.width, texture.height);

	// Only create SRGB texture view if necessary
	if (internalformat_srgb != GL_NONE) {
		glTextureView(impl->id[1], GL_TEXTURE_2D, impl->id[0], internalformat_srgb, 0, texture.levels, 0, 1);
	}
	else {
		impl->id[1] = impl->id[0];
	}

	// Clear texture to black since by default its contents are undefined
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
void reshade::opengl::runtime_gl::upload_texture(texture &texture, const uint8_t *pixels)
{
	const auto impl = texture.impl->as<opengl_tex_data>();
	assert(impl != nullptr && pixels != nullptr && texture.impl_reference == texture_reference::none);

	unsigned int upload_pitch = texture.width * 4;
	std::vector<uint8_t> upload_data(pixels, pixels + upload_pitch * texture.height);

	// Flip image data horizontally
	const auto temp = static_cast<uint8_t *>(alloca(upload_pitch));
	for (uint32_t y = 0; 2 * y < texture.height; y++)
	{
		const auto line1 = upload_data.data() + upload_pitch * (y);
		const auto line2 = upload_data.data() + upload_pitch * (texture.height - 1 - y);

		std::memcpy(temp,  line1, upload_pitch);
		std::memcpy(line1, line2, upload_pitch);
		std::memcpy(line2, temp,  upload_pitch);
	}

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
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture.width, texture.height, GL_RGBA, GL_UNSIGNED_BYTE, upload_data.data());

	if (texture.levels > 1)
		glGenerateMipmap(GL_TEXTURE_2D);

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

void reshade::opengl::runtime_gl::render_technique(technique &technique)
{
	assert(_app_state.has_state);

	opengl_technique_data &technique_data = *technique.impl->as<opengl_technique_data>();

	if (technique_data.query_in_flight)
	{
		GLuint64 elapsed_time = 0;
		glGetQueryObjectui64v(technique_data.query, GL_QUERY_RESULT, &elapsed_time);
		technique.average_gpu_duration.append(elapsed_time);
		technique_data.query_in_flight = false; // Reset query status
	}

	if (!technique_data.query_in_flight)
		glBeginQuery(GL_TIME_ELAPSED, technique_data.query);

	// Clear depth stencil
	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[FBO_BACK]);
	glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

	// Set up global states
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask(GL_FALSE); // No need to write to the depth buffer
	glBindVertexArray(_vao[VAO_FX]); // This is an empty vertex array object

	// Set up shader constants
	if (_effect_ubos[technique.effect_index] != 0)
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubos[technique.effect_index]);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, _effects[technique.effect_index].uniform_data_storage.size(), _effects[technique.effect_index].uniform_data_storage.data());
	}

	// Set up shader resources
	for (size_t i = 0; i < technique_data.samplers.size(); i++)
	{
		glActiveTexture(GL_TEXTURE0 + GLenum(i));
		glBindTexture(GL_TEXTURE_2D, technique_data.samplers[i].texture->id[technique_data.samplers[i].is_srgb]);
		glBindSampler(GLuint(i), technique_data.samplers[i].id);
	}

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<opengl_pass_data>();

		// Copy back buffer of previous pass to texture
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[FBO_BACK]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[FBO_BLIT]);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Set up pass specific state
		glViewport(0, 0, pass_data.viewport_width, pass_data.viewport_height);
		glUseProgram(pass_data.program);
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

		if (pass_info.srgb_write_enable) {
			glEnable(GL_FRAMEBUFFER_SRGB);
		} else {
			glDisable(GL_FRAMEBUFFER_SRGB);
		}

		glColorMask(
			(pass_info.color_write_mask & (1 << 0)) != 0,
			(pass_info.color_write_mask & (1 << 1)) != 0,
			(pass_info.color_write_mask & (1 << 2)) != 0,
			(pass_info.color_write_mask & (1 << 3)) != 0);

		if (pass_info.clear_render_targets)
			for (GLuint k = 0; k < 8; k++)
			{
				if (pass_data.draw_targets[k] == GL_NONE)
					continue; // Ignore unbound render targets
				const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				glClearBufferfv(GL_COLOR, k, color);
			}

		glDrawArrays(GL_TRIANGLES, 0, pass_info.num_vertices);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		// Regenerate mipmaps of any textures bound as render target
		for (GLuint texture_id : pass_data.draw_textures)
		{
			for (size_t k = 0; k < technique_data.samplers.size(); ++k)
			{
				const auto texture = technique_data.samplers[k].texture;
				if (technique_data.samplers[k].has_mipmaps &&
					(texture->id[0] == texture_id || texture->id[1] == texture_id))
				{
					glActiveTexture(GL_TEXTURE0 + GLenum(k));
					glGenerateMipmap(GL_TEXTURE_2D);
				}
			}
		}
	}

	if (!technique_data.query_in_flight)
		glEndQuery(GL_TIME_ELAPSED);
	technique_data.query_in_flight = true;
}

#if RESHADE_GUI
void reshade::opengl::runtime_gl::init_imgui_resources()
{
	assert(_app_state.has_state);

	const GLchar *vertex_shader[] = {
		"#version 330\n"
		"uniform mat4 ProjMtx;\n"
		"in vec2 Position, UV;\n"
		"in vec4 Color;\n"
		"out vec2 Frag_UV;\n"
		"out vec4 Frag_Color;\n"
		"void main()\n"
		"{\n"
		"	Frag_UV = UV * vec2(1.0, -1.0) + vec2(0.0, 1.0);\n" // Texture coordinates were flipped in 'update_texture'
		"	Frag_Color = Color;\n"
		"	gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		"}\n"
	};
	const GLchar *fragment_shader[] = {
		"#version 330\n"
		"uniform sampler2D Texture;\n"
		"in vec2 Frag_UV;\n"
		"in vec4 Frag_Color;\n"
		"out vec4 Out_Color;\n"
		"void main()\n"
		"{\n"
		"	Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
		"}\n"
	};

	const GLuint imgui_vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(imgui_vs, 1, vertex_shader, 0);
	glCompileShader(imgui_vs);
	const GLuint imgui_fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(imgui_fs, 1, fragment_shader, 0);
	glCompileShader(imgui_fs);

	_imgui_program = glCreateProgram();
	glAttachShader(_imgui_program, imgui_vs);
	glAttachShader(_imgui_program, imgui_fs);
	glLinkProgram(_imgui_program);
	glDeleteShader(imgui_vs);
	glDeleteShader(imgui_fs);

	_imgui_uniform_tex  = glGetUniformLocation(_imgui_program, "Texture");
	_imgui_uniform_proj = glGetUniformLocation(_imgui_program, "ProjMtx");

	const int attrib_pos = glGetAttribLocation(_imgui_program, "Position");
	const int attrib_uv  = glGetAttribLocation(_imgui_program, "UV");
	const int attrib_col = glGetAttribLocation(_imgui_program, "Color");

	glBindBuffer(GL_ARRAY_BUFFER, _buf[VBO_IMGUI]);
	glBindVertexArray(_vao[VAO_IMGUI]);
	glEnableVertexAttribArray(attrib_pos);
	glEnableVertexAttribArray(attrib_uv );
	glEnableVertexAttribArray(attrib_col);
	glVertexAttribPointer(attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, pos)));
	glVertexAttribPointer(attrib_uv , 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, uv )));
	glVertexAttribPointer(attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), reinterpret_cast<GLvoid *>(offsetof(ImDrawVert, col)));
}

void reshade::opengl::runtime_gl::render_imgui_draw_data(ImDrawData *draw_data)
{
	assert(_app_state.has_state);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glFrontFace(GL_CCW);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);
	glEnable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);
	glActiveTexture(GL_TEXTURE0); // Bind texture at location zero below
	glUseProgram(_imgui_program);
	glBindSampler(0, 0); // Do not use separate sampler object, since state is already set in texture
	glBindVertexArray(_vao[VAO_IMGUI]);

	glViewport(0, 0, GLsizei(draw_data->DisplaySize.x), GLsizei(draw_data->DisplaySize.y));

	const float ortho_projection[16] = {
		2.0f / draw_data->DisplaySize.x, 0.0f,   0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y,  0.0f, 0.0f,
		0.0f,                            0.0f,  -1.0f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x) / draw_data->DisplaySize.x,
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y) / draw_data->DisplaySize.y, 0.0f, 1.0f,
	};

	glUniform1i(_imgui_uniform_tex, 0); // Set to GL_TEXTURE0
	glUniformMatrix4fv(_imgui_uniform_proj, 1, GL_FALSE, ortho_projection);

	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		ImDrawList *const draw_list = draw_data->CmdLists[n];

		glBindBuffer(GL_ARRAY_BUFFER, _buf[VBO_IMGUI]);
		glBufferData(GL_ARRAY_BUFFER, draw_list->VtxBuffer.Size * sizeof(ImDrawVert), draw_list->VtxBuffer.Data, GL_STREAM_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _buf[IBO_IMGUI]);
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
				static_cast<const opengl_tex_data *>(cmd.TextureId)->id[0]);

			glDrawElementsBaseVertex(GL_TRIANGLES, cmd.ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
				reinterpret_cast<const void *>(static_cast<uintptr_t>(cmd.IdxOffset * sizeof(ImDrawIdx))), cmd.VtxOffset);
		}
	}
}
#endif

#if RESHADE_OPENGL_CAPTURE_DEPTH_BUFFERS
void reshade::opengl::runtime_gl::draw_depth_debug_menu()
{
	if (ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Use aspect ratio heuristics", &_use_aspect_ratio_heuristics))
			runtime::save_config();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		for (const auto &[depth_source, snapshot] : _buffer_detection.depth_buffer_counters())
		{
			char label[512] = "";
			sprintf_s(label, "%s0x%08x", (depth_source == _depth_source && !_has_high_network_activity ? "> " : "  "), depth_source);

			if (bool value = _depth_source_override == depth_source;
				ImGui::Checkbox(label, &value))
				_depth_source_override = value ? depth_source : std::numeric_limits<GLuint>::max();

			ImGui::SameLine();
			ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
				snapshot.width, snapshot.height, snapshot.stats.drawcalls, snapshot.stats.vertices,
				(depth_source & 0x80000000) != 0 ? " RBO" : depth_source != 0 ? " FBO" : "");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}
}

void reshade::opengl::runtime_gl::update_depthstencil_texture(buffer_detection::depthstencil_info info)
{
	if (_has_high_network_activity)
	{
		_depth_source = 0;

		if (_tex[TEX_DEPTH])
		{
			glDeleteTextures(1, &_tex[TEX_DEPTH]);
			_tex[TEX_DEPTH] = 0;

			for (auto &tex : _textures)
			{
				if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
				{
					const auto texture_impl = tex.impl->as<opengl_tex_data>();
					assert(texture_impl != nullptr && !texture_impl->should_delete);
					texture_impl->id[0] = 0;
					texture_impl->id[1] = 0;
				}
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
	case GL_DEPTH_STENCIL:
		info.format = GL_DEPTH24_STENCIL8;
		break;
	case GL_DEPTH_COMPONENT:
		info.format = GL_DEPTH_COMPONENT24;
		break;
	}

	// Can just use source directly if it is a simple texture already, so only create extra one for RBOs
	if (info.target != GL_TEXTURE_2D || info.level != 0)
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

				LOG(ERROR) << "Failed to create depth texture of format " << std::hex << info.format << " with error code " << err << std::dec << '.';
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
	}

	// Update all references to the new texture
	for (auto &tex : _textures)
	{
		if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
		{
			const auto texture_impl = tex.impl->as<opengl_tex_data>();
			assert(texture_impl != nullptr && !texture_impl->should_delete);
			texture_impl->id[0] = info.obj;
			texture_impl->id[1] = info.obj;
		}
	}
}
#endif
