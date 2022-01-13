/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_device.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

extern thread_local bool g_in_dxgi_runtime;

HOOK_EXPORT HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	LOG(INFO) << "Redirecting " << "D3D12CreateDevice" << '(' << "pAdapter = " << pAdapter << ", MinimumFeatureLevel = " << std::hex << MinimumFeatureLevel << std::dec << ", riid = " << riid << ", ppDevice = " << ppDevice << ')' << " ...";

	// NVIDIA Ansel creates a D3D11 device internally, so to avoid hooking that, set the flag that forces 'D3D11CreateDevice' to return early
	g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "D3D12CreateDevice" << " failed with error code " << hr << '.';
		return hr;
	}

	if (ppDevice == nullptr)
		return hr;

	// The returned device should alway implement the 'ID3D12Device' base interface
	const auto device_proxy = new D3D12Device(static_cast<ID3D12Device *>(*ppDevice));

	// Upgrade to the actual interface version requested here
	if (device_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "Returning ID3D12Device" << device_proxy->_interface_version << " object " << device_proxy << " (" << device_proxy->_orig << ").";
#endif
		*ppDevice = device_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		LOG(WARN) << "Unknown interface " << riid << " in " << "D3D12CreateDevice" << '.';

		delete device_proxy; // Delete instead of release to keep reference count untouched
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void **ppvDebug)
{
	return reshade::hooks::call(D3D12GetDebugInterface)(riid, ppvDebug);
}

HOOK_EXPORT HRESULT WINAPI D3D12CreateRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

HOOK_EXPORT HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateVersionedRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

HOOK_EXPORT HRESULT WINAPI D3D12EnableExperimentalFeatures(UINT NumFeatures, const IID *pIIDs, void *pConfigurationStructs, UINT *pConfigurationStructSizes)
{
	return reshade::hooks::call(D3D12EnableExperimentalFeatures)(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	static const auto trampoline = reshade::hooks::call(D3D12SerializeRootSignature);
	return trampoline(pRootSignature, Version, ppBlob, ppErrorBlob);
}

HOOK_EXPORT HRESULT WINAPI D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	static const auto trampoline = reshade::hooks::call(D3D12SerializeVersionedRootSignature);
	return trampoline(pRootSignature, ppBlob, ppErrorBlob);
}
