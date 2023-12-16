/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"

reshade::d3d9::swapchain_impl::swapchain_impl(device_impl *device, IDirect3DSwapChain9 *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device)
{
}

reshade::api::device *reshade::d3d9::swapchain_impl::get_device()
{
	return _device_impl;
}

void *reshade::d3d9::swapchain_impl::get_hwnd() const
{
	// Destination window may be temporarily overriden by 'hDestWindowOverride' parameter during present call
	if (_hwnd != nullptr)
		return _hwnd;

	// Always retrieve present parameters from device, since the application may have set 'hDeviceWindow' to zero and instead used 'hFocusWindow' (e.g. Resident Evil 4)
	// Retrieving them from the device ensures that 'hDeviceWindow' is always set (with either the original value from 'hDeviceWindow' or 'hFocusWindow')
	D3DPRESENT_PARAMETERS pp = {};
	_orig->GetPresentParameters(&pp);
	assert(pp.hDeviceWindow != nullptr);

	return pp.hDeviceWindow;
}

reshade::api::resource reshade::d3d9::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	if (_back_buffer == nullptr)
		_orig->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_back_buffer);
	assert(_back_buffer != nullptr);

	return to_handle(static_cast<IDirect3DResource9 *>(_back_buffer.get()));
}
