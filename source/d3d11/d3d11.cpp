/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"

HOOK_EXPORT HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting D3D11CreateDevice" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ')' << " ...";
	LOG(INFO) << "> Passing on to D3D11CreateDeviceAndSwapChain:";

	return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, ppImmediateContext);
}

HOOK_EXPORT HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting D3D11CreateDeviceAndSwapChain" << '(' << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ')' << " ...";

	// Use local feature level variable in case the application did not pass one in
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = reshade::hooks::call(D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, &FeatureLevel, nullptr);
	if (FAILED(hr))
	{
		LOG(WARN) << "> D3D11CreateDeviceAndSwapChain failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	if (pFeatureLevel != nullptr) // Copy feature level value to application variable if the argument exists
		*pFeatureLevel = FeatureLevel;

	LOG(INFO) << "> Using feature level " << std::hex << FeatureLevel << std::dec << '.';

	// It is valid for the device out parameter to be NULL if the application wants to check feature level support, so just return early in that case
	if (ppDevice == nullptr)
		return hr;

	const auto device = *ppDevice;

	// Query for the DXGI device and immediate device context since we need to reference them in the hooked device
	IDXGIDevice1 *dxgi_device = nullptr;
	device->QueryInterface(&dxgi_device);
	ID3D11DeviceContext *device_context = nullptr;
	device->GetImmediateContext(&device_context);

	const auto device_proxy = new D3D11Device(dxgi_device, device, device_context);

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

		LOG(INFO) << "> Calling IDXGIFactory::CreateSwapChain:";

		hr = factory->CreateSwapChain(device_proxy, const_cast<DXGI_SWAP_CHAIN_DESC *>(pSwapChainDesc), ppSwapChain);
	}

	if (SUCCEEDED(hr))
	{
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Returning IDXGIDevice1 object " << device_proxy->_dxgi_device << " and ID3D11Device object " << device_proxy << '.';
#endif
		*ppDevice = device_proxy;

		if (ppImmediateContext != nullptr)
		{
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Returning ID3D11DeviceContext object " << device_proxy->_immediate_context << '.';
#endif
			device_proxy->_immediate_context->AddRef(); // D3D11CreateDeviceAndSwapChain increases the reference count on the returned object
			*ppImmediateContext = device_proxy->_immediate_context;
		}
	}
	else
	{
		// Swap chain creation failed, so do clean up
		device_proxy->Release();
	}

	return hr;
}
