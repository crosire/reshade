/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d12_device.hpp"

extern reshade::log::message &operator<<(reshade::log::message &m, REFIID riid);

HOOK_EXPORT HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	LOG(INFO) << "Redirecting D3D12CreateDevice" << '(' << pAdapter << ", " << std::hex << MinimumFeatureLevel << std::dec << ", " << riid << ", " << ppDevice << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	if (FAILED(hr))
	{
		LOG(WARN) << "> D3D12CreateDevice failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	if (ppDevice == nullptr)
		return hr;

	ID3D12Device *device = nullptr;

	if (SUCCEEDED(static_cast<IUnknown *>(*ppDevice)->QueryInterface(&device)))
	{
		const auto device_proxy = new D3D12Device(device);

		device->Release();

		*ppDevice = device_proxy;

		LOG(INFO) << "Returning 'ID3D12Device' object " << device_proxy;
	}
	else
	{
		LOG(WARN) << "> Skipping device because it is missing support for the ID3D12Device interface.";
	}

	return hr;
}
