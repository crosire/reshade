/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d10_device.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"
#include "addon_manager.hpp"

extern thread_local bool g_in_dxgi_runtime;

extern "C" HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D10CreateDevice)(pAdapter, DriverType, Software, Flags, SDKVersion, ppDevice);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D10CreateDevice(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, SDKVersion = %u, ppDevice = %p) ...",
		pAdapter, DriverType, Software, Flags, SDKVersion, ppDevice);
	reshade::log::message(reshade::log::level::info, "> Passing on to D3D10CreateDeviceAndSwapChain1:");

	// Only 'd3d10.dll' is guaranteed to be loaded at this point, but the 'D3D10CreateDeviceAndSwapChain1' entry point is in 'd3d10_1.dll', so load that now to make sure hooks can be resolved
	LoadLibraryW(L"d3d10_1.dll");

	// Upgrade to feature level 10.1, since 10.0 did not allow copying between depth-stencil resources
	// See https://docs.microsoft.com/windows/win32/api/d3d10/nf-d3d10-id3d10device-copyresource
	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, nullptr, nullptr, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

extern "C" HRESULT WINAPI D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D10CreateDevice1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, ppDevice);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D10CreateDevice1(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, HardwareLevel = %x, SDKVersion = %u, ppDevice = %p) ...",
		pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, ppDevice);
	reshade::log::message(reshade::log::level::info, "> Passing on to D3D10CreateDeviceAndSwapChain1:");

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
}

extern "C" HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D10CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D10CreateDeviceAndSwapChain(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, SDKVersion = %u, pSwapChainDesc = %p, ppSwapChain = %p, ppDevice = %p) ...",
		pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
	reshade::log::message(reshade::log::level::info, "> Passing on to D3D10CreateDeviceAndSwapChain1:");

	LoadLibraryW(L"d3d10_1.dll");

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, pSwapChainDesc, ppSwapChain, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

extern "C" HRESULT WINAPI D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	const auto trampoline = reshade::hooks::call(D3D10CreateDeviceAndSwapChain1);

	// Pass on unmodified in case this called from within 'CDXGISwapChain::EnsureChildDeviceInternal', which indicates that the DXGI runtime is trying to create an internal device, which should not be hooked
	if (g_in_dxgi_runtime)
		return trampoline(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting D3D10CreateDeviceAndSwapChain1(pAdapter = %p, DriverType = %d, Software = %p, Flags = %#x, HardwareLevel = %x, SDKVersion = %u, pSwapChainDesc = %p, ppSwapChain = %p, ppDevice = %p) ...",
		pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

#ifndef NDEBUG
	// Remove flag that prevents turning on the debug layer
	Flags &= ~D3D10_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#endif

#if RESHADE_ADDON >= 2
	reshade::load_addons();

	uint32_t api_version = static_cast<uint32_t>(HardwareLevel);
	if (reshade::invoke_addon_event<reshade::addon_event::create_device>(reshade::api::device_api::d3d10, api_version))
	{
		HardwareLevel = static_cast<D3D10_FEATURE_LEVEL1>(api_version);
	}
#endif

	// This may call 'D3D11CreateDeviceAndSwapChain' internally, so to avoid duplicated hooks, set the flag that forces it to return early
	g_in_dxgi_runtime = true;
	HRESULT hr = trampoline(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
#if RESHADE_ADDON >= 2
		reshade::unload_addons();
#endif
		reshade::log::message(reshade::log::level::warning, "D3D10CreateDeviceAndSwapChain1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
	{
		assert(ppSwapChain == nullptr);

#if RESHADE_ADDON >= 2
		reshade::unload_addons();
#endif
		return hr;
	}

	auto device = *ppDevice;
	// Query for the DXGI device since we need to reference it in the proxy device
	com_ptr<IDXGIDevice1> dxgi_device;
	hr = device->QueryInterface(&dxgi_device);
	assert(SUCCEEDED(hr));

	// Create device proxy unless this is a software device
	D3D10Device *device_proxy = nullptr;
	if (DriverType == D3D10_DRIVER_TYPE_WARP || DriverType == D3D10_DRIVER_TYPE_REFERENCE)
	{
		reshade::log::message(reshade::log::level::warning, "Skipping device because the driver type is 'D3D_DRIVER_TYPE_WARP' or 'D3D_DRIVER_TYPE_REFERENCE'.");
	}
	else
	{
		// Change device to proxy for swap chain creation below
		device = device_proxy = new D3D10Device(dxgi_device.get(), device);
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
			assert(SUCCEEDED(hr));
		}

		// Time to find a factory associated with the target adapter and create a swap chain with it
		com_ptr<IDXGIFactory> factory;
		hr = adapter->GetParent(IID_PPV_ARGS(&factory));
		assert(SUCCEEDED(hr));

		reshade::log::message(reshade::log::level::info, "Calling IDXGIFactory::CreateSwapChain:");

		hr = factory->CreateSwapChain(device, pSwapChainDesc, ppSwapChain);
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
				"Returning ID3D10Device1 object %p (%p) and IDXGIDevice1 object %p (%p).",
				static_cast<ID3D10Device *>(device_proxy), device_proxy->_orig,
				static_cast<IDXGIDevice1 *>(device_proxy), static_cast<DXGIDevice *>(device_proxy)->_orig);
#endif
			*ppDevice = device_proxy;
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
