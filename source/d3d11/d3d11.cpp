/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "dxgi/dxgi_factory.hpp"
#include "dxgi/dxgi_adapter.hpp"
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

	com_ptr<DXGIAdapter> adapter_proxy;
	if (pAdapter && SUCCEEDED(pAdapter->QueryInterface(&adapter_proxy)))
		pAdapter = adapter_proxy->_orig;

#ifndef NDEBUG
	// Remove flag that prevents turning on the debug layer
	Flags &= ~D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#endif

#if RESHADE_ADDON >= 2
	uint32_t original_api_version = (pFeatureLevels != nullptr && FeatureLevels > 0) ? pFeatureLevels[0] : D3D_FEATURE_LEVEL_11_0;
	uint32_t upgraded_api_version = original_api_version;
	bool api_version_increased = false;
	if (ppDevice != nullptr)
	{
		reshade::load_addons();

		if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d11, upgraded_api_version))
		{
			FeatureLevels = 1;
			pFeatureLevels = reinterpret_cast<const D3D_FEATURE_LEVEL *>(&upgraded_api_version);
			api_version_increased = upgraded_api_version > original_api_version;
		}
	}
#endif

#ifdef RESHADE_TEST_APPLICATION
	// Perform dummy call to 'CreateDXGIFactory1' to ensure virtual function table hooks are set up correctly
	// This is done here in case a third party is hooking the factory too, to ensure the call chain of the factory methods is consistent:
	//    App -> ReShade -> X (some third party that installed hooks) -> driver
	// Otherwise it may happen that it will be called like this:
	//    App -> D3D11CreateDeviceAndSwapChain -> X -> driver -> CreateDXGIFactory1
	// And therefore the virtual function table hooks would be installed to the driver object, rather than the object created by the third party.
	g_in_dxgi_runtime = true;
	com_ptr<IDXGIFactory1> dummy_factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dummy_factory));
	dummy_factory.reset();
#endif

	// Use local feature level variable in case the application did not pass one in
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	g_in_dxgi_runtime = true;
	HRESULT hr = trampoline(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, &FeatureLevel, nullptr);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
#if RESHADE_ADDON >= 2
		if (ppDevice != nullptr)
			reshade::unload_addons();
#endif
		reshade::log::message(reshade::log::level::warning, "D3D11CreateDeviceAndSwapChain failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	if (pFeatureLevel != nullptr) // Copy feature level value to application variable if the argument exists
	{
#if RESHADE_ADDON >= 2
		// Don't tell the game the feature level was upgraded, it might have issue if the game came out before the upgraded feature level existed
		if (api_version_increased && FeatureLevel >= static_cast<D3D_FEATURE_LEVEL>(original_api_version))
			*pFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(original_api_version);
		else
#endif
			*pFeatureLevel = FeatureLevel;
	}

	reshade::log::message(reshade::log::level::info, "Using feature level %x.", FeatureLevel);

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
	{
		assert(ppSwapChain == nullptr && ppImmediateContext == nullptr);
		return hr;
	}

	auto device = *ppDevice;
	// Query for the DXGI device and immediate device context since we need to reference them in the proxy device
	com_ptr<IDXGIDevice1> dxgi_device;
	hr = device->QueryInterface(&dxgi_device);
	assert(SUCCEEDED(hr));

	com_ptr<IDXGIFactory> factory;
	com_ptr<IDXGIAdapter> adapter;
	if (adapter_proxy == nullptr)
	{
		hr = dxgi_device->GetAdapter(&adapter);
		assert(SUCCEEDED(hr)); // Lets just assume this works =)
		hr = adapter->GetParent(IID_PPV_ARGS(&factory));
		assert(SUCCEEDED(hr));

		// Only create proxy factory when not using vtable hooking for 'IDXGIFactory::CreateSwapChain'
		if (!reshade::hooks::is_hooked(reshade::hooks::vtable_from_instance(factory.get()) + 10))
		{
			factory = com_ptr<IDXGIFactory>(new DXGIFactory(factory.release()), true);
			adapter = com_ptr<IDXGIAdapter>(new DXGIAdapter(factory.get(), adapter.release()), true);
		}
	}
	else
	{
		hr = adapter_proxy->GetParent(IID_PPV_ARGS(&factory));
		assert(SUCCEEDED(hr));

		adapter = std::move(reinterpret_cast<com_ptr<IDXGIAdapter> &>(adapter_proxy));
	}

	// Create device proxy unless this is a software device (used by D2D for example)
	D3D11Device *device_proxy = nullptr;
	if (DriverType == D3D_DRIVER_TYPE_WARP || DriverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because the driver type is 'D3D_DRIVER_TYPE_WARP' or 'D3D_DRIVER_TYPE_REFERENCE'.");
	}
	else if (DXGI_ADAPTER_DESC adapter_desc;
		SUCCEEDED(adapter->GetDesc(&adapter_desc)) &&
		adapter_desc.VendorId == 0x1414 /* Microsoft */ && adapter_desc.DeviceId == 0x8C /* Microsoft Basic Render Driver */)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because it uses the Microsoft Basic Render Driver.");
	}
	else
	{
		ID3D11DeviceContext *device_context = nullptr;
		device->GetImmediateContext(&device_context);

		// Change device to proxy for swap chain creation below
		device = device_proxy = new D3D11Device(adapter.get(), dxgi_device.get(), device);
		device_proxy->_immediate_context = new D3D11DeviceContext(device_proxy, device_context);
#if RESHADE_ADDON >= 2
		if (api_version_increased && FeatureLevel >= static_cast<D3D_FEATURE_LEVEL>(original_api_version))
		{
			device_proxy->_custom_feature_level = true; // Only set this in case it was upgraded, not downgraded
			device_proxy->_orig_feature_level = static_cast<D3D_FEATURE_LEVEL>(original_api_version);
		}
		else
		{
			device_proxy->_orig_feature_level = FeatureLevel;
		}
#endif
	}

	// Swap chain creation is piped through the 'IDXGIFactory::CreateSwapChain' function hook
	if (pSwapChainDesc != nullptr)
	{
		assert(ppSwapChain != nullptr);

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
