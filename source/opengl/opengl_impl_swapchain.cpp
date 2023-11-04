/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"
#include "dll_log.hpp"
#include "addon_manager.hpp"

#define gl gl3wProcs.gl

reshade::opengl::swapchain_impl::swapchain_impl(HDC hdc, HGLRC initial_hglrc, bool compatibility_context) :
	device_impl(hdc, initial_hglrc, compatibility_context), render_context_impl(this, initial_hglrc), swapchain_base(this)
{
	_hdcs.insert(hdc);

	create_effect_runtime(this, this);

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

	destroy_effect_runtime(this);
}

reshade::api::resource reshade::opengl::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
}

void reshade::opengl::swapchain_impl::on_init(HWND hwnd, unsigned int width, unsigned int height)
{
	assert(width != 0 && height != 0);

	_hwnd = hwnd;
	_width = width;
	_height = height;
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

	init_effect_runtime(this);
}
void reshade::opengl::swapchain_impl::on_reset()
{
	if (_width == 0 && _height == 0)
		return;

	reset_effect_runtime(this);

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

	_hwnd = nullptr;
}

bool reshade::opengl::swapchain_impl::on_present(HDC hdc)
{
	// The window handle can be invalid if the window was already destroyed
	const HWND hwnd = WindowFromDC(hdc);
	if (hwnd == nullptr)
		return false;

	assert(hwnd == _hwnd);

	RECT rect = { 0, 0, 0, 0 };
	GetClientRect(hwnd, &rect);

	const auto width = static_cast<unsigned int>(rect.right);
	const auto height = static_cast<unsigned int>(rect.bottom);

	if (width != _width || height != _height)
	{
		LOG(INFO) << "Resizing device context " << GetDC(hwnd) << " to " << width << "x" << height << " ...";

		on_reset();

		if (width != 0 && height != 0)
			on_init(hwnd, width, height);
	}

#ifndef NDEBUG
	GLenum type = GL_NONE; char message[512] = "";
	while (gl.GetDebugMessageLog(1, 512, nullptr, &type, nullptr, nullptr, nullptr, message))
		if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
			OutputDebugStringA(message), OutputDebugStringA("\n");
#endif

	return true;
}

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
