/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "dxgi/dxgi_adapter.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

std::shared_mutex g_adapter_mutex;

extern thread_local bool g_in_dxgi_runtime;

extern "C" HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	const auto trampoline = reshade::hooks::call(D3D12CreateDevice);

	// Pass on unmodified in case this called from within 'Direct3DCreate9', which indicates that the D3D9 runtime is trying to create an internal device for D3D9on12, which should not be hooked
	if (g_in_dxgi_runtime)
		return trampoline(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	// Need to lock during device creation to ensure an existing device proxy cannot be destroyed in while it is queried below
	const std::unique_lock<std::shared_mutex> lock(g_adapter_mutex);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D12CreateDevice(pAdapter = %p, MinimumFeatureLevel = %x, riid = %s, ppDevice = %p) ...",
		pAdapter, MinimumFeatureLevel, reshade::log::iid_to_string(riid).c_str(), ppDevice);

	com_ptr<DXGIAdapter> adapter_proxy;
	if (pAdapter && SUCCEEDED(pAdapter->QueryInterface(&adapter_proxy)))
		pAdapter = adapter_proxy->_orig;

#if RESHADE_ADDON >= 2
	if (ppDevice != nullptr)
	{
		reshade::load_addons();

		uint32_t api_version = static_cast<uint32_t>(MinimumFeatureLevel);
		if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d12, api_version))
		{
			MinimumFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(api_version);
		}
	}
#endif

	// NVIDIA Ansel creates a D3D11 device internally, so to avoid hooking that, set the flag that forces 'D3D11CreateDevice' to return early
	g_in_dxgi_runtime = true;
	const HRESULT hr = trampoline(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
#if RESHADE_ADDON >= 2
		if (ppDevice != nullptr)
			reshade::unload_addons();
#endif
		reshade::log::message(reshade::log::level::warning, "D3D12CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	if (ppDevice == nullptr)
		return hr;

	// The returned device should alway implement the 'ID3D12Device' base interface
	const auto device = static_cast<ID3D12Device *>(*ppDevice);

	// Direct3D 12 devices are singletons per adapter, so first check if one was already created previously
	D3D12Device *device_proxy = nullptr;
	D3D12Device *const device_proxy_existing = get_private_pointer_d3dx<D3D12Device>(device);
	if (device_proxy_existing != nullptr && device_proxy_existing->_orig == device)
	{
		InterlockedIncrement(&device_proxy_existing->_ref);
		device_proxy = device_proxy_existing;
	}
	else
	{
		device_proxy = new D3D12Device(device);
	}

#if RESHADE_ADDON >= 2
	reshade::unload_addons();
#endif

	// Upgrade to the actual interface version requested here
	if (device_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		reshade::log::message(
			reshade::log::level::debug,
			"Returning ID3D12Device%hu object %p (%p).",
			device_proxy->_interface_version, device_proxy, device_proxy->_orig);
#endif
		*ppDevice = device_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		reshade::log::message(reshade::log::level::warning, "Unknown interface %s in D3D12CreateDevice.", reshade::log::iid_to_string(riid).c_str());

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
