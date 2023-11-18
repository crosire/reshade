/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_swapchain.hpp"
#include "d3d10_impl_type_convert.hpp"

reshade::d3d10::swapchain_impl::swapchain_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device)
{
	on_init();
}
reshade::d3d10::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::device *reshade::d3d10::swapchain_impl::get_device()
{
	return _device_impl;
}

void *reshade::d3d10::swapchain_impl::get_hwnd() const
{
	DXGI_SWAP_CHAIN_DESC swap_desc = {};
	_orig->GetDesc(&swap_desc);

	return swap_desc.OutputWindow;
}

reshade::api::resource reshade::d3d10::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(_back_buffer.get());
}

void reshade::d3d10::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	if (is_initialized())
		return;

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_back_buffer))))
		return;
	assert(_back_buffer != nullptr);

#ifndef NDEBUG
	DXGI_SWAP_CHAIN_DESC swap_desc = {};
	_orig->GetDesc(&swap_desc);
	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);
#endif
}
void reshade::d3d10::swapchain_impl::on_reset()
{
	_back_buffer.reset();
}
