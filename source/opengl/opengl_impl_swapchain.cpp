/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "opengl_impl_swapchain.hpp"
#include "opengl_impl_type_convert.hpp"

#define gl gl3wProcs.gl

reshade::opengl::swapchain_impl::swapchain_impl(HDC hdc, HGLRC initial_hglrc, bool compatibility_context) :
	device_impl(hdc, initial_hglrc, compatibility_context), render_context_impl(this, initial_hglrc), swapchain_base(this)
{
	_hdcs.insert(hdc);
}

reshade::api::resource reshade::opengl::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return make_resource_handle(GL_FRAMEBUFFER_DEFAULT, GL_BACK);
}

void reshade::opengl::swapchain_impl::destroy_resource_view(api::resource_view handle)
{
	device_impl::destroy_resource_view(handle);

	// Destroy all framebuffers, to ensure they are recreated even if a resource view handle is reused
	for (const auto &fbo_data : _fbo_lookup)
	{
		gl.DeleteFramebuffers(1, &fbo_data.second);
	}

	_fbo_lookup.clear();
}
