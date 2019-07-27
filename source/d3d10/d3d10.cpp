/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d10_device.hpp"

HOOK_EXPORT HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDevice" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << SDKVersion << ", " << ppDevice << ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	// Only 'd3d10.dll' is guaranteed to be loaded at this point, but the 'D3D10CreateDeviceAndSwapChain1' entry point is in 'd3d10_1.dll', so load that now to make sure hooks can be resolved
	LoadLibraryW(L"d3d10_1.dll");

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, nullptr, nullptr, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDevice1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDevice1" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << HardwareLevel << ", " << SDKVersion << ", " << ppDevice << ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDeviceAndSwapChain" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ')' << " ...";
	LOG(INFO) << "> Passing on to D3D10CreateDeviceAndSwapChain1:";

	LoadLibraryW(L"d3d10_1.dll");

	return D3D10CreateDeviceAndSwapChain1(pAdapter, DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, pSwapChainDesc, ppSwapChain, reinterpret_cast<ID3D10Device1 **>(ppDevice));
}

HOOK_EXPORT HRESULT WINAPI D3D10CreateDeviceAndSwapChain1(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, D3D10_FEATURE_LEVEL1 HardwareLevel, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device1 **ppDevice)
{
	LOG(INFO) << "Redirecting D3D10CreateDeviceAndSwapChain1" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << ", " << HardwareLevel << std::dec << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ')' << " ...";

	HRESULT hr = reshade::hooks::call(D3D10CreateDeviceAndSwapChain1)(pAdapter, DriverType, Software, Flags, HardwareLevel, SDKVersion, nullptr, nullptr, ppDevice);
	if (FAILED(hr))
	{
		LOG(WARN) << "> D3D10CreateDeviceAndSwapChain1 failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
		return hr;

	const auto device = *ppDevice;

	// Query for the DXGI device since we need to reference it in the hooked device
	IDXGIDevice1 *dxgi_device = nullptr;
	device->QueryInterface(&dxgi_device);

	const auto device_proxy = new D3D10Device(dxgi_device, device);

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

		hr = factory->CreateSwapChain(device_proxy, pSwapChainDesc, ppSwapChain);
	}

	if (SUCCEEDED(hr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Returning IDXGIDevice1 object " << device_proxy->_dxgi_device << " and ID3D10Device1 object " << device_proxy << '.';
#endif
		*ppDevice = device_proxy;
	}
	else
	{
		// Swap chain creation failed, so do clean up
		device_proxy->Release();
	}

	return hr;
}
