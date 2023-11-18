/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_impl_device.hpp"
#include "d3d11_impl_swapchain.hpp"
#include "d3d11_impl_type_convert.hpp"

reshade::d3d11::swapchain_impl::swapchain_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device)
{
	on_init();
}
reshade::d3d11::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::device *reshade::d3d11::swapchain_impl::get_device()
{
	return _device_impl;
}

void *reshade::d3d11::swapchain_impl::get_hwnd() const
{
	DXGI_SWAP_CHAIN_DESC swap_desc = {};
	_orig->GetDesc(&swap_desc);

	return swap_desc.OutputWindow;
}

reshade::api::resource reshade::d3d11::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(_back_buffer.get());
}

bool reshade::d3d11::swapchain_impl::check_color_space_support(api::color_space color_space) const
{
	if (color_space == api::color_space::unknown)
		return false;

	com_ptr<IDXGISwapChain3> swapchain3;
	if (FAILED(_orig->QueryInterface(&swapchain3)))
		return color_space == api::color_space::srgb_nonlinear;

	UINT support;
	return SUCCEEDED(swapchain3->CheckColorSpaceSupport(convert_color_space(color_space), &support)) && (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0;
}

void reshade::d3d11::swapchain_impl::set_color_space(DXGI_COLOR_SPACE_TYPE type)
{
	_back_buffer_color_space = convert_color_space(type);
}

void reshade::d3d11::swapchain_impl::on_init()
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

	// Clear reference to make Unreal Engine 4 happy (https://github.com/EpicGames/UnrealEngine/blob/4.7/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp#L195)
	_back_buffer->Release();
	assert(_back_buffer.ref_count() == 0);
}
void reshade::d3d11::swapchain_impl::on_reset()
{
	if (!is_initialized())
		return;

	// Resident Evil 3 releases all references to the back buffer before calling 'IDXGISwapChain::ResizeBuffers', even ones it does not own
	// Releasing any references ReShade owns would then make the count negative, which consequently breaks DXGI validation
	// But the only reference was already released above because of Unreal Engine 4 anyway, so can simply clear out the pointer here without touching the reference count
	assert(_back_buffer.ref_count() == 0);
	_back_buffer.release();
}
