/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "dll_log.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"

#define gl gl3wProcs.gl

reshade::opengl::swapchain_impl::swapchain_impl(HDC hdc, HGLRC initial_hglrc, bool compatibility_context) :
	device_impl(hdc, initial_hglrc, compatibility_context), render_context_impl(this, initial_hglrc), swapchain_base(this, this)
{
	_hdcs.insert(hdc);

	GLint major = 0, minor = 0;
	gl.GetIntegerv(GL_MAJOR_VERSION, &major);
	gl.GetIntegerv(GL_MINOR_VERSION, &minor);
	_renderer_id = 0x10000 | (major << 12) | (minor << 8);

	const GLubyte *const name = gl.GetString(GL_RENDERER);
	const GLubyte *const version = gl.GetString(GL_VERSION);
	LOG(INFO) << "Running on " << name << " using OpenGL " << version << '.';

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

	GLint scissor_box[4] = {};
	gl.GetIntegerv(GL_SCISSOR_BOX, scissor_box);
	assert(scissor_box[0] == 0 && scissor_box[1] == 0);

	// Wolfenstein: The Old Blood creates a window with a height of zero that is later resized
	if (scissor_box[2] != 0 && scissor_box[3] != 0)
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

bool reshade::opengl::swapchain_impl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	assert(width != 0 && height != 0);

	_default_fbo_desc.texture.width = width;
	_default_fbo_desc.texture.height = _current_window_height = height;

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);

	api::resource_view default_rtv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
	api::resource_view default_dsv = { 0 };
	if (_default_depth_format != api::format::unknown)
	{
		default_dsv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
		invoke_addon_event<addon_event::init_resource>(this, get_resource_desc(get_resource_from_view(default_dsv)), nullptr, api::resource_usage::depth_stencil, get_resource_from_view(default_dsv));
		invoke_addon_event<addon_event::init_resource_view>(this, get_resource_from_view(default_dsv), api::resource_usage::depth_stencil, api::resource_view_desc(_default_depth_format), default_dsv);
	}

	// Communicate default state to add-ons
	invoke_addon_event<addon_event::bind_render_targets_and_depth_stencil>(this, 1, &default_rtv, default_dsv);
#endif

	return runtime::on_init(hwnd);
}
void reshade::opengl::swapchain_impl::on_reset()
{
	if (_width == 0 && _height == 0)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	api::resource_view default_dsv = { 0 };
	if (_default_depth_format != api::format::unknown)
	{
		default_dsv = make_resource_view_handle(GL_FRAMEBUFFER_DEFAULT, GL_DEPTH_STENCIL_ATTACHMENT);
		invoke_addon_event<addon_event::destroy_resource_view>(this, default_dsv);
		invoke_addon_event<addon_event::destroy_resource>(this, get_resource_from_view(default_dsv));
	}

	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif
}

void reshade::opengl::swapchain_impl::on_present()
{
	if (!is_initialized())
		return;

	_app_state.capture(_compatibility_context);

	runtime::on_present();

	// Apply previous state from application
	_app_state.apply(_compatibility_context);

#ifndef NDEBUG
	GLenum type = GL_NONE; char message[512] = "";
	while (gl.GetDebugMessageLog(1, 512, nullptr, &type, nullptr, nullptr, nullptr, message))
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
#endif
}

#if RESHADE_ADDON && RESHADE_FX
void reshade::opengl::swapchain_impl::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	if (!_is_in_present_call)
		_app_state.capture(_compatibility_context);

	runtime::render_effects(cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
		_app_state.apply(_compatibility_context);
}
void reshade::opengl::swapchain_impl::render_technique(api::effect_technique handle, api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	if (!_is_in_present_call)
		_app_state.capture(_compatibility_context);

	runtime::render_technique(handle, cmd_list, rtv, rtv_srgb);

	if (!_is_in_present_call)
		_app_state.apply(_compatibility_context);
}
#endif

void reshade::opengl::swapchain_impl::destroy_resource_view(api::resource_view handle)
{
	device_impl::destroy_resource_view(handle);

	// Destroy all framebuffers, to ensure they are recreated even if a resource view handle is re-used
	for (const auto &fbo_data : _fbo_lookup)
	{
		gl.DeleteFramebuffers(1, &fbo_data.second);
	}

	_fbo_lookup.clear();
}
