/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_config.hpp"
#include "runtime_gl.hpp"
#include "runtime_objects.hpp"

reshade::opengl::runtime_impl::runtime_impl(HDC hdc, HGLRC hglrc) : device_impl(hdc, hglrc)
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

	switch (_default_color_format)
	{
	case GL_RGBA8:
		_color_bit_depth = 8;
		break;
	case GL_RGB10_A2:
		_color_bit_depth = 10;
		break;
	case GL_RGBA16F:
		_color_bit_depth = 16;
		break;
	}

	subscribe_to_load_config([this](const ini_file &config) {
		// Reserve a fixed amount of texture names by default to work around issues in old OpenGL games (which will use a compatibility context)
		auto num_reserve_texture_names = _compatibility_context ? 512u : 0u;
		config.get("APP", "ReserveTextureNames", num_reserve_texture_names);
		_reserved_texture_names.resize(num_reserve_texture_names);
	});
}
reshade::opengl::runtime_impl::~runtime_impl()
{
	on_reset();
}

bool reshade::opengl::runtime_impl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	_width = _window_width = width;
	_height = _window_height = height;

	if (hwnd != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(hwnd, &window_rect);

		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
		_default_fbo_width = width;
		_default_fbo_height = height;
	}

	// Capture and later restore so that the resource creation code below does not affect the application state
	_app_state.capture(_compatibility_context);

	// Some games (like Hot Wheels Velocity X) use fixed texture names, which can clash with the ones ReShade generates below, since most implementations will return values linearly
	// Reserve a configurable range of names for those games to work around this
	if (!_reserved_texture_names.empty())
		glGenTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());

	glGenFramebuffers(2, _fbo);
	glGenRenderbuffers(1, &_rbo);

	glBindRenderbuffer(GL_RENDERBUFFER, _rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, _width, _height);

	glBindFramebuffer(GL_FRAMEBUFFER, _fbo[0]);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	_app_state.apply(_compatibility_context);

	return runtime::on_init(hwnd);
}
void reshade::opengl::runtime_impl::on_reset()
{
	runtime::on_reset();

	for (const auto &it : _framebuffer_list_internal)
		glDeleteFramebuffers(1, &it.second);
	_framebuffer_list_internal.clear();

	glDeleteTextures(static_cast<GLsizei>(_reserved_texture_names.size()), _reserved_texture_names.data());
	glDeleteFramebuffers(2, _fbo);
	glDeleteRenderbuffers(1, &_rbo);

	_rbo = 0;
	std::memset(_fbo, 0, sizeof(_fbo));
}

void reshade::opengl::runtime_impl::on_present(bool default_fbo)
{
	if (!_is_initialized)
		return;

	_app_state.capture(_compatibility_context);

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	if (default_fbo)
	{
		// Copy back buffer to RBO (and flip it vertically)
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _current_fbo = _fbo[0]);
		glReadBuffer(GL_BACK);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _current_fbo = _fbo[0]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}

	update_and_render_effects();

	runtime::on_present();

	if (default_fbo)
	{
		// Copy results from RBO to back buffer (and flip it back vertically)
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _current_fbo = 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_BACK);
		glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _current_fbo = _fbo[0]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}

	// Apply previous state from application
	_app_state.apply(_compatibility_context);
}
bool reshade::opengl::runtime_impl::on_layer_submit(uint32_t eye, GLuint source_object, bool is_rbo, bool is_array, const float bounds[4], GLuint *target_rbo)
{
	assert(eye < 2 && source_object != 0);

	reshade::api::resource_desc object_desc = get_resource_desc(
		reshade::opengl::make_resource_handle(is_rbo ? GL_RENDERBUFFER : GL_TEXTURE, source_object));

	GLint source_region[4] = { 0, 0, static_cast<GLint>(object_desc.texture.width), static_cast<GLint>(object_desc.texture.height) };
	if (bounds != nullptr)
	{
		source_region[0] = static_cast<GLint>(object_desc.texture.width * std::min(bounds[0], bounds[2]));
		source_region[1] = static_cast<GLint>(object_desc.texture.height * std::min(bounds[1], bounds[3]));
		source_region[2] = static_cast<GLint>(object_desc.texture.width * std::max(bounds[0], bounds[2]));
		source_region[3] = static_cast<GLint>(object_desc.texture.height * std::max(bounds[1], bounds[3]));
	}

	const GLint region_width = source_region[2] - source_region[0];
	object_desc.texture.width = region_width * 2;

	if (object_desc.texture.width != _width || object_desc.texture.height != _height)
	{
		on_reset();

		if (!on_init(nullptr, object_desc.texture.width, object_desc.texture.height))
		{
			LOG(ERROR) << "Failed to initialize OpenGL runtime environment on runtime " << this << '!';
			return false;
		}
	}

	// Copy source region to RBO
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[1]); // Use clear FBO here, since it is reset on every use anyway
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _current_fbo = _fbo[0]);

	if (is_rbo) {
		// TODO: This or the second blit below will fail if RBO is multisampled
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, source_object);
	}
	else if (is_array) {
		glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_object, 0, 0);
	}
	else {
		glFramebufferTexture(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_object, 0);
	}

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(source_region[0], source_region[1], source_region[2], source_region[3], eye * region_width, 0, (eye + 1) * region_width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	*target_rbo = _rbo;

	return true;
}

bool reshade::opengl::runtime_impl::capture_screenshot(uint8_t *buffer) const
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

bool reshade::opengl::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, api::shader_module &out)
{
	if (!effect.module.spirv.empty())
	{
		assert(_renderer_id >= 0x14600); // Core since OpenGL 4.6 (see https://www.khronos.org/opengl/wiki/SPIR-V)
		assert(gl3wProcs.gl.ShaderBinary != nullptr && gl3wProcs.gl.SpecializeShader != nullptr);

		return create_shader_module(type, api::shader_format::spirv, effect.module.spirv.data(), effect.module.spirv.size() * sizeof(uint32_t), entry_point.c_str(), &out);
	}
	else
	{
		std::string source = "#version 430\n";
		source += "#define ENTRY_POINT_" + entry_point + " 1\n";

		if (type == api::shader_stage::vertex)
		{
			// OpenGL does not allow using 'discard' in the vertex shader profile
			source += "#define discard\n";
			// 'dFdx', 'dFdx' and 'fwidth' too are only available in fragment shaders
			source += "#define dFdx(x) x\n";
			source += "#define dFdy(y) y\n";
			source += "#define fwidth(p) p\n";
		}
		if (type != api::shader_stage::compute)
		{
			// OpenGL does not allow using 'shared' in vertex/fragment shader profile
			source += "#define shared\n";
			source += "#define atomicAdd(a, b) a\n";
			source += "#define atomicAnd(a, b) a\n";
			source += "#define atomicOr(a, b) a\n";
			source += "#define atomicXor(a, b) a\n";
			source += "#define atomicMin(a, b) a\n";
			source += "#define atomicMax(a, b) a\n";
			source += "#define atomicExchange(a, b) a\n";
			source += "#define atomicCompSwap(a, b, c) a\n";
			// Barrier intrinsics are only available in compute shaders
			source += "#define barrier()\n";
			source += "#define memoryBarrier()\n";
			source += "#define groupMemoryBarrier()\n";
		}

		source += "#line 1 0\n"; // Reset line number, so it matches what is shown when viewing the generated code
		source += effect.preamble;
		source += effect.module.hlsl;

		return create_shader_module(type, api::shader_format::glsl, source.data(), source.size(), "main", &out);
	}
}
