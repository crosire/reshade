/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"

reshade::opengl::swapchain_impl::swapchain_impl(HDC hdc, HGLRC hglrc) :
	device_impl(hdc, hglrc), runtime(this, this)
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

	_backbuffer_format = convert_format(_default_color_format);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif
}
reshade::opengl::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

void reshade::opengl::swapchain_impl::get_back_buffer(uint32_t index, api::resource *out)
{
	assert(index == 0);

	*out = make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
}
void reshade::opengl::swapchain_impl::get_back_buffer_resolved(uint32_t index, api::resource *out)
{
	assert(index == 0);

	*out = make_resource_handle(GL_RENDERBUFFER, _rbo);
}

bool reshade::opengl::swapchain_impl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	if (hwnd != nullptr)
	{
		_default_fbo_width = width;
		_default_fbo_height = height;
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	_width = width;
	_height = height;

	// Capture and later restore so that the resource creation code below does not affect the application state
	_app_state.capture(_compatibility_context);

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
void reshade::opengl::swapchain_impl::on_reset()
{
	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	glDeleteFramebuffers(2, _fbo);
	glDeleteRenderbuffers(1, &_rbo);

	_rbo = 0;
	_fbo[0] = 0;
	_fbo[1] = 0;
	std::memset(_fbo, 0, sizeof(_fbo));
}

void reshade::opengl::swapchain_impl::on_present(bool default_fbo)
{
	if (!is_initialized())
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
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[0]);
		glReadBuffer(GL_BACK);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[0]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}

	runtime::on_present();

	if (default_fbo)
	{
		// Copy results from RBO to back buffer (and flip it back vertically)
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[0]);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_BACK);
		glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	else
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[0]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}

	// Apply previous state from application
	_app_state.apply(_compatibility_context);
}
bool reshade::opengl::swapchain_impl::on_layer_submit(uint32_t eye, GLuint source_object, bool is_rbo, bool is_array, const float bounds[4], GLuint *target_rbo)
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

		_is_vr = true;

		if (!on_init(nullptr, object_desc.texture.width, object_desc.texture.height))
			return false;
	}

	// Copy source region to RBO
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo[1]); // Use clear FBO here, since it is reset on every use anyway
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo[0]);

	if (is_rbo) {
		// TODO: This or the second blit below will fail if RBO is multisampled
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, source_object);
	}
	else if (is_array) {
		glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_object, 0, eye);
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
