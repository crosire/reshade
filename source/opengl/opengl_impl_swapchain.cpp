/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"

reshade::opengl::swapchain_impl::swapchain_impl(HDC hdc, HGLRC hglrc, bool compatibility_context) :
	device_impl(hdc, hglrc, compatibility_context), runtime(this, this)
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

	_back_buffer_format = convert_format(_default_color_format);

	GLint scissor_box[4] = {};
	glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
	assert(scissor_box[0] == 0 && scissor_box[1] == 0);

	on_init(WindowFromDC(hdc), scissor_box[2], scissor_box[3]);
}
reshade::opengl::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::opengl::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
}
reshade::api::resource reshade::opengl::swapchain_impl::get_back_buffer_resolved(uint32_t index)
{
	assert(index == 0);

	return make_resource_handle(GL_RENDERBUFFER, _rbo);
}

bool reshade::opengl::swapchain_impl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	assert(width != 0 && height != 0);

	_default_fbo_width = width;
	_default_fbo_height = height;
	_current_window_height = height;

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);

	api::resource_view default_rtv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
	api::resource_view default_dsv = make_resource_view_handle(0, 0);
	if (_default_depth_format != GL_NONE)
	{
		default_dsv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
		invoke_addon_event<addon_event::init_resource>(this, get_resource_desc(get_resource_from_view(default_dsv)), nullptr, api::resource_usage::depth_stencil, get_resource_from_view(default_dsv));
		invoke_addon_event<addon_event::init_resource_view>(this, get_resource_from_view(default_dsv), api::resource_usage::depth_stencil, api::resource_view_desc(convert_format(_default_depth_format)), default_dsv);
	}

	// Communicate default state to add-ons
	invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 1, &default_rtv, default_dsv);
#endif

	_width = width;
	_height = height;

	// Capture and later restore so that the resource creation code below does not affect the application state
	_app_state.capture(_compatibility_context);

	glGenRenderbuffers(1, &_rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, _rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, _width, _height);

	glGenFramebuffers(1, &_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _rbo);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	_app_state.apply(_compatibility_context);

	return runtime::on_init(hwnd);
}
void reshade::opengl::swapchain_impl::on_reset()
{
	if (_width == 0 && _height == 0)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	api::resource_view default_dsv = make_resource_view_handle(0, 0);
	if (_default_depth_format != GL_NONE)
	{
		default_dsv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
		invoke_addon_event<addon_event::destroy_resource_view>(this, default_dsv);
		invoke_addon_event<addon_event::destroy_resource>(this, get_resource_from_view(default_dsv));
	}

	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	glDeleteFramebuffers(1, &_fbo);
	_fbo = 0;

	glDeleteRenderbuffers(1, &_rbo);
	_rbo = 0;
}

void reshade::opengl::swapchain_impl::on_present()
{
	if (!is_initialized())
		return;

	_app_state.capture(_compatibility_context);

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

	// Copy back buffer to RBO (and flip it vertically)
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _fbo);
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	runtime::on_present();

	// Copy results from RBO to back buffer (and flip it back vertically)
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_FRAMEBUFFER_SRGB);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, _fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_present>(this);
#endif

	// Apply previous state from application
	_app_state.apply(_compatibility_context);
}

#if RESHADE_FX
void reshade::opengl::swapchain_impl::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	_app_state.capture(_compatibility_context);

	// Set clip space to something consistent
	if (gl3wProcs.gl.ClipControl != nullptr)
		glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);

	runtime::render_effects(cmd_list, rtv, rtv_srgb);

	_app_state.apply(_compatibility_context);
}
#endif
