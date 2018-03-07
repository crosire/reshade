/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_context.hpp"
#include "../dxgi/dxgi_device.hpp"

// D3D11
HOOK_EXPORT HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting '" << "D3D11CreateDevice" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ")' ...";
	LOG(INFO) << "> Passing on to 'D3D11CreateDeviceAndSwapChain':";

	return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, ppImmediateContext);
}
HOOK_EXPORT HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext)
{
	LOG(INFO) << "Redirecting '" << "D3D11CreateDeviceAndSwapChain" << "(" << pAdapter << ", " << DriverType << ", " << Software << ", " << std::hex << Flags << std::dec << ", " << pFeatureLevels << ", " << FeatureLevels << ", " << SDKVersion << ", " << pSwapChainDesc << ", " << ppSwapChain << ", " << ppDevice << ", " << pFeatureLevel << ", " << ppImmediateContext << ")' ...";

#ifdef _DEBUG
	Flags |= D3D11_CREATE_DEVICE_DEBUG;
	Flags &= ~D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#endif

	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0;

	HRESULT hr = reshade::hooks::call(&D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, &FeatureLevel, nullptr);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'D3D11CreateDeviceAndSwapChain' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	LOG(INFO) << "> Using feature level " << std::hex << FeatureLevel << std::dec << ".";

	if (ppDevice != nullptr)
	{
		IDXGIDevice1 *dxgidevice = nullptr;
		ID3D11Device *const device = *ppDevice;
		ID3D11DeviceContext *devicecontext = nullptr;

		assert(device != nullptr);

		device->QueryInterface(&dxgidevice);
		device->GetImmediateContext(&devicecontext);

		assert(dxgidevice != nullptr);
		assert(devicecontext != nullptr);

		const auto device_proxy = new D3D11Device(device);
		const auto devicecontext_proxy = new D3D11DeviceContext(device_proxy, devicecontext);
		device_proxy->_dxgi_device = new DXGIDevice(dxgidevice, device_proxy);
		device_proxy->_immediate_context = devicecontext_proxy;

		if (pSwapChainDesc != nullptr)
		{
			assert(ppSwapChain != nullptr);

			if (pAdapter != nullptr)
			{
				pAdapter->AddRef();
			}
			else
			{
				hr = device_proxy->_dxgi_device->GetAdapter(&pAdapter);

				assert(SUCCEEDED(hr));
			}

			IDXGIFactory *factory = nullptr;

			hr = pAdapter->GetParent(IID_PPV_ARGS(&factory));

			assert(SUCCEEDED(hr));

			LOG(INFO) << "> Calling 'IDXGIFactory::CreateSwapChain':";

			hr = factory->CreateSwapChain(device_proxy, const_cast<DXGI_SWAP_CHAIN_DESC *>(pSwapChainDesc), ppSwapChain);

			factory->Release();
			pAdapter->Release();
		}

		if (SUCCEEDED(hr))
		{
			LOG(INFO) << "Returning 'IDXGIDevice1' object " << device_proxy->_dxgi_device;
			LOG(INFO) << "Returning 'ID3D11Device' object " << device_proxy;

			*ppDevice = device_proxy;

			if (ppImmediateContext != nullptr)
			{
				LOG(INFO) << "Returning 'ID3D11DeviceContext' object " << devicecontext_proxy;

				devicecontext_proxy->AddRef();
				*ppImmediateContext = devicecontext_proxy;
			}
		}
		else
		{
			device_proxy->Release();
		}
	}

	if (pFeatureLevel != nullptr)
	{
		*pFeatureLevel = FeatureLevel;
	}

	return hr;
}
