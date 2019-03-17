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

	// The returned device should alway implement at least the 'ID3D12Device' interface
	const auto device_proxy = new D3D12Device(static_cast<ID3D12Device *>(*ppDevice));

	*ppDevice = device_proxy;

	LOG(INFO) << "Returning ID3D12Device object " << device_proxy;

	return hr;
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeRootSignature)(pRootSignature, Version, ppBlob, ppErrorBlob);
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeVersionedRootSignature)(pRootSignature, ppBlob, ppErrorBlob);
}
