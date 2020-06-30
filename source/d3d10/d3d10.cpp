/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include "d3d10_device.hpp"

extern thread_local bool g_in_dxgi_runtime;

HOOK_EXPORT HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDevice" << '('
		<<   "pAdapter = " << pAdapter
		<< ", DriverType = " << DriverType
		<< ", Software = " << Software
		<< ", Flags = " << std::hex << Flags << std::dec
		<< ", SDKVersion = " << SDKVersion
		<< ", ppDevice = " << ppDevice
		<< ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	// Only 'd3d10.dll' is guaranteed to be loaded at this point, but the 'D3D10CreateDeviceAndSwapChain1' entry point is in 'd3d10_1.dll', so load that now to make sure hooks can be resolved
	LoadLibraryW(L"d3d10_1.dll");

	// Upgrade to feature level 10.1, since 10.0 did not allow copying between depth-stencil resources
	// See https://docs.microsoft.com/windows/win32/api/d3d10/nf-d3d10-id3d10device-copyresource
	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, nullptr, nullptr, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDevice1" << '('
		<<   "pAdapter = " << pAdapter
		<< ", DriverType = " << DriverType
		<< ", Software = " << Software
		<< ", Flags = " << std::hex << Flags
		<< ", HardwareLevel = " << HardwareLevel << std::dec
		<< ", SDKVersion = " << SDKVersion
		<< ", ppDevice = " << ppDevice
		<< ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDeviceAndSwapChain" << '('
		<<   "pAdapter = " << pAdapter
		<< ", DriverType = " << DriverType
		<< ", Software = " << Software
		<< ", Flags = " << std::hex << Flags << std::dec
		<< ", SDKVersion = " << SDKVersion
		<< ", pSwapChainDesc = " << pSwapChainDesc
		<< ", ppSwapChain = " << ppSwapChain
		<< ", ppDevice = " << ppDevice
		<< ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	LoadLibraryW(L"d3d10_1.dll");

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, pSwapChainDesc, ppSwapChain, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	// Pass on unmodified in case this called from within 'IDXGISwapChain::Present' or 'IDXGIFactory::CreateSwapChain', which indicates that the DXGI runtime is trying to create an internal device, which should not be hooked
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(D3D10CreateDeviceAndSwapChain1)(
			pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);

	LOG(INFO) << "Redirecting D3D10CreateDeviceAndSwapChain1" << '('
		<<   "pAdapter = " << pAdapter
		<< ", DriverType = " << DriverType
		<< ", Software = " << Software
		<< ", Flags = " << std::hex << Flags
		<< ", HardwareLevel = " << HardwareLevel << std::dec
		<< ", SDKVersion = " << SDKVersion
		<< ", pSwapChainDesc = " << pSwapChainDesc
		<< ", ppSwapChain = " << ppSwapChain
		<< ", ppDevice = " << ppDevice
		<< ')' << " ...";

#ifndef NDEBUG
	// Remove flag that prevents turning on the debug layer
	Flags &= ~D3D10_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#endif

	// This may call 'D3D11CreateDeviceAndSwapChain' internally, so to avoid duplicated hooks, set the flag that forces it to return early
	g_in_dxgi_runtime = true;
	HRESULT hr = reshade::hooks::call(D3D10CreateDeviceAndSwapChain1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "D3D10CreateDeviceAndSwapChain1 failed with error code " << hr << '!';
		return hr;
	}

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
	{
		assert(ppSwapChain == nullptr);
		return hr;
	}

	auto device = *ppDevice;
	// Query for the DXGI device since we need to reference it in the hooked device
	IDXGIDevice1 *dxgi_device = nullptr;
	hr = device->QueryInterface(&dxgi_device);
	assert(SUCCEEDED(hr));

	// Create device proxy unless this is a software device
	D3D10Device *device_proxy = nullptr;
	if (DriverType == D3D10_DRIVER_TYPE_WARP || DriverType == D3D10_DRIVER_TYPE_REFERENCE)
	{
		LOG(WARN) << "Skipping device because the driver type is 'D3D_DRIVER_TYPE_WARP' or 'D3D_DRIVER_TYPE_REFERENCE'.";
	}
	else
	{
		// Change device to proxy for swap chain creation below
		device = device_proxy = new D3D10Device(dxgi_device, device);
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

		LOG(INFO) << "> Calling IDXGIFactory::CreateSwapChain:";

		hr = factory->CreateSwapChain(device, pSwapChainDesc, ppSwapChain);
	}

	if (SUCCEEDED(hr))
	{
		if (device_proxy != nullptr)
		{
#if RESHADE_VERBOSE_LOG
			LOG(INFO) << "Returning ID3D10Device1 object " << device_proxy << " and IDXGIDevice1 object " << device_proxy->_dxgi_device << '.';
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
