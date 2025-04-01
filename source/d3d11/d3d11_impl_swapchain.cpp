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

	com_ptr<ID3D11Texture2D> back_buffer;
	_orig->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	assert(back_buffer != nullptr);
	// Reference is immediately released to make Unreal Engine 4 happy (https://github.com/EpicGames/UnrealEngine/blob/4.7/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp#L195)

	return to_handle(back_buffer.get());
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

inline constexpr GUID SKID_SwapChainColorSpace = { 0x18b57e4, 0x1493, 0x4953, { 0xad, 0xf2, 0xde, 0x6d, 0x99, 0xcc, 0x5, 0xe5 } };

reshade::api::color_space reshade::d3d11::swapchain_impl::get_color_space() const
{
	DXGI_COLOR_SPACE_TYPE color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT color_space_size = sizeof(color_space);
	if (FAILED(_orig->GetPrivateData(SKID_SwapChainColorSpace, &color_space_size, &color_space)))
	{
		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		_orig->GetDesc(&swap_desc);

		if (swap_desc.BufferDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
			color_space = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
	}

	if (color_space == DXGI_COLOR_SPACE_CUSTOM)
		return api::color_space::unknown;

	return convert_color_space(color_space);
}
