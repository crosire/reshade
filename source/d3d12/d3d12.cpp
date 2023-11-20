/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"
#include "hook_manager.hpp"

std::shared_mutex g_adapter_mutex;

extern thread_local bool g_in_dxgi_runtime;

extern "C" HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	// Pass on unmodified in case this called from within 'Direct3DCreate9', which indicates that the D3D9 runtime is trying to create an internal device for D3D9on12, which should not be hooked
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	// Need to lock during device creation to ensure an existing device proxy cannot be destroyed in while it is queried below
	const std::unique_lock<std::shared_mutex> lock(g_adapter_mutex);

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
	const auto device = static_cast<ID3D12Device *>(*ppDevice);

	// Direct3D 12 devices are singletons per adapter, so first check if one was already created previously
	const auto device_proxy_existing = get_private_pointer_d3dx<D3D12Device>(device);
	const auto device_proxy = (device_proxy_existing != nullptr) ? device_proxy_existing : new D3D12Device(device);

	if (device_proxy_existing != nullptr)
		device_proxy_existing->_ref++;

	// Upgrade to the actual interface version requested here
	if (device_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Returning " << "ID3D12Device" << device_proxy->_interface_version << " object " << device_proxy << " (" << device_proxy->_orig << ").";
#endif
		*ppDevice = device_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		LOG(WARN) << "Unknown interface " << riid << " in " << "D3D12CreateDevice" << '.';

		if (device_proxy != device_proxy_existing)
			delete device_proxy; // Delete instead of release to keep reference count untouched
	}

	return hr;
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void **ppvDebug)
{
	return reshade::hooks::call(D3D12GetDebugInterface)(riid, ppvDebug);
}

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void **ppRootSignatureDeserializer)
{
	return reshade::hooks::call(D3D12CreateVersionedRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

extern "C" HRESULT WINAPI D3D12EnableExperimentalFeatures(UINT NumFeatures, const IID *pIIDs, void *pConfigurationStructs, UINT *pConfigurationStructSizes)
{
	return reshade::hooks::call(D3D12EnableExperimentalFeatures)(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
}

extern "C" HRESULT WINAPI D3D12GetInterface(REFCLSID rclsid, REFIID riid, void **ppvDebug)
{
	return reshade::hooks::call(D3D12GetInterface)(rclsid, riid, ppvDebug);
}

extern "C" HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeRootSignature)(pRootSignature, Version, ppBlob, ppErrorBlob);
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
	return reshade::hooks::call(D3D12SerializeVersionedRootSignature)(pRootSignature, ppBlob, ppErrorBlob);
}
