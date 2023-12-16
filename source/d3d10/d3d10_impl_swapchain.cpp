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
#ifndef NDEBUG
	DXGI_SWAP_CHAIN_DESC swap_desc = {};
	_orig->GetDesc(&swap_desc);

	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);
#endif
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

	com_ptr<ID3D10Texture2D> back_buffer;
	_orig->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	assert(back_buffer != nullptr);

	return to_handle(back_buffer.get());
}
