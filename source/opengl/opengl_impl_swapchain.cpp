/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_device.hpp"
#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"

reshade::opengl::swapchain_impl::swapchain_impl(device_impl *device, HDC hdc) :
	api_object_impl(hdc),
	_device_impl(device)
{
}

reshade::api::device *reshade::opengl::swapchain_impl::get_device()
{
	return _device_impl;
}

void *reshade::opengl::swapchain_impl::get_hwnd() const
{
	return WindowFromDC(_orig);
}

reshade::api::resource reshade::opengl::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
}
