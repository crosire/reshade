/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"
#include "addon_manager.hpp"

extern thread_local bool g_in_dxgi_runtime;

extern "C" HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D11CreateDevice)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D11CreateDevice(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, pFeatureLevels = %p, FeatureLevels = %u, SDKVersion = %u, ppDevice = %p, pFeatureLevel = %p, ppImmediateContext = %p) ...",
		pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
	reshade::log::message(reshade::log::level::info, "> Passing on to D3D11CreateDeviceAndSwapChain:");

	// The D3D runtime does this internally anyway, so to avoid duplicate hooks, always pass on to D3D11CreateDeviceAndSwapChain
	return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, ppImmediateContext);
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	const auto trampoline = reshade::hooks::call(D3D11CreateDeviceAndSwapChain);

	// Pass on unmodified in case this called from within 'CDXGISwapChain::EnsureChildDeviceInternal' or 'D3D10CreateDeviceAndSwapChain1', which indicates that the DXGI runtime is trying to create an internal device, which should not be hooked
	if (g_in_dxgi_runtime)
		return trampoline(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D11CreateDeviceAndSwapChain(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, pFeatureLevels = %p, FeatureLevels = %u, SDKVersion = %u, pSwapChainDesc = %p, ppSwapChain = %p, ppDevice = %p, pFeatureLevel = %p, ppImmediateContext = %p) ...",
		pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

#ifndef NDEBUG
	// Remove flag that prevents turning on the debug layer
	Flags &= ~D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#endif

#ifdef RESHADE_TEST_APPLICATION
	// Perform dummy call to 'CreateDXGIFactory1' to ensure virtual function table hooks are set up correctly
	// This is done here in case a third party is hooking the factory too, to ensure the call chain of the factory methods is consistent:
	//    App -> ReShade -> X (some third party that installed hooks) -> driver
	// Otherwise it may happen that it will be called like this:
	//    App -> D3D11CreateDeviceAndSwapChain -> X -> driver -> CreateDXGIFactory1
	// And therefore the virtual function table hooks would be installed to the driver object, rather than the object created by the third party.
	com_ptr<IDXGIFactory1> dummy_factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dummy_factory));
	dummy_factory.reset();
#endif

#if RESHADE_ADDON >= 2
	reshade::load_addons();

	uint32_t api_version = (pFeatureLevels != nullptr && FeatureLevels > 0) ? pFeatureLevels[0] : D3D_FEATURE_LEVEL_11_0;
	if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d11, api_version))
	{
		FeatureLevels = 1;
		pFeatureLevels = reinterpret_cast<const D3D_FEATURE_LEVEL *>(&api_version);
	}
#endif

	// Use local feature level variable in case the application did not pass one in
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	g_in_dxgi_runtime = true;
	HRESULT hr = trampoline(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, &FeatureLevel, nullptr);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
#if RESHADE_ADDON >= 2
		reshade::unload_addons();
#endif
		reshade::log::message(reshade::log::level::warning, "D3D11CreateDeviceAndSwapChain failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	if (pFeatureLevel != nullptr) // Copy feature level value to application variable if the argument exists
		*pFeatureLevel = FeatureLevel;

	reshade::log::message(reshade::log::level::info, "Using feature level %x.", FeatureLevel);

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
	{
		assert(ppSwapChain == nullptr && ppImmediateContext == nullptr);

#if RESHADE_ADDON >= 2
		reshade::unload_addons();
#endif
		return hr;
	}

	auto device = *ppDevice;
	// Query for the DXGI device and immediate device context since we need to reference them in the proxy device
	com_ptr<IDXGIDevice1> dxgi_device;
	hr = device->QueryInterface(&dxgi_device);
	assert(SUCCEEDED(hr));

	// Create device proxy unless this is a software device (used by D2D for example)
	D3D11Device *device_proxy = nullptr;
	if (DriverType == D3D_DRIVER_TYPE_WARP || DriverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because the driver type is 'D3D_DRIVER_TYPE_WARP' or 'D3D_DRIVER_TYPE_REFERENCE'.");
	}
	else if (DXGI_ADAPTER_DESC adapter_desc;
		pAdapter != nullptr && SUCCEEDED(pAdapter->GetDesc(&adapter_desc)) &&
		adapter_desc.VendorId == 0x1414 /* Microsoft */ && adapter_desc.DeviceId == 0x8C /* Microsoft Basic Render Driver */)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because it uses the Microsoft Basic Render Driver.");
	}
	else
	{
		ID3D11DeviceContext *device_context = nullptr;
		device->GetImmediateContext(&device_context);

		// Change device to proxy for swap chain creation below
		device = device_proxy = new D3D11Device(dxgi_device.get(), device);
		device_proxy->_immediate_context = new D3D11DeviceContext(device_proxy, device_context);
	}

	// Swap chain creation is piped through the 'IDXGIFactory::CreateSwapChain' function hook
	if (pSwapChainDesc != nullptr)
	{
		assert(ppSwapChain != nullptr);

		com_ptr<IDXGIAdapter> adapter(pAdapter, false);
		// Fall back to the same adapter as the device if it was not explicitly specified in the argument list
		if (adapter == nullptr)
		{
			hr = dxgi_device->GetAdapter(&adapter);
			assert(SUCCEEDED(hr)); // Lets just assume this works =)
		}

		// Time to find a factory associated with the target adapter and create a swap chain with it
		com_ptr<IDXGIFactory> factory;
		hr = adapter->GetParent(IID_PPV_ARGS(&factory));
		assert(SUCCEEDED(hr));

		reshade::log::message(reshade::log::level::info, "Calling IDXGIFactory::CreateSwapChain:");

		hr = factory->CreateSwapChain(device, const_cast<DXGI_SWAP_CHAIN_DESC *>(pSwapChainDesc), ppSwapChain);
	}

#if RESHADE_ADDON >= 2
	reshade::unload_addons();
#endif

	if (SUCCEEDED(hr))
	{
		if (device_proxy != nullptr)
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(
				reshade::log::level::debug,
				"Returning ID3D11Device0 object %p (%p) and IDXGIDevice1 object %p (%p).",
				static_cast<ID3D11Device *>(device_proxy), device_proxy->_orig,
				static_cast<IDXGIDevice1 *>(device_proxy), static_cast<DXGIDevice *>(device_proxy)->_orig);
#endif
			*ppDevice = device_proxy;
		}

		if (ppImmediateContext != nullptr)
		{
			// D3D11CreateDeviceAndSwapChain increases the reference count on the returned object, so can just call this:
			device->GetImmediateContext(ppImmediateContext);
		}
	}
	else
	{
		*ppDevice = nullptr;
		// Swap chain creation failed, so do clean up
		device->Release();
	}

	return hr;
}
