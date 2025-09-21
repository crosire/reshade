/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_device.hpp"
#include "d3d11_resource.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

// Chromium ANGLE checks the device pointer queried from textures, which fails if that is not pointing to the proxy device, causing content to not show up
// See https://chromium.googlesource.com/angle/angle/+/refs/heads/main/src/libANGLE/renderer/d3d/d3d11/Renderer11.cpp#1567
void STDMETHODCALLTYPE ID3D11Resource_GetDevice(ID3D11Resource *pResource, ID3D11Device **ppDevice)
{
	reshade::hooks::call(ID3D11Resource_GetDevice, reshade::hooks::vtable_from_instance(pResource) + 3)(pResource, ppDevice);

	const auto device = *ppDevice;
	assert(device != nullptr);

	// Do not return proxy device when video support is enabled due to checks performed by the Microsoft Media Foundation library (see also comment in 'D3D11Device::QueryInterface')
	if (device->GetCreationFlags() & D3D11_CREATE_DEVICE_VIDEO_SUPPORT)
		return;

	const auto device_proxy = get_private_pointer_d3dx<D3D11Device>(device);
	if (device_proxy != nullptr && device_proxy->_orig == device)
	{
		InterlockedIncrement(&device_proxy->_ref);
		*ppDevice = device_proxy;
	}
}
