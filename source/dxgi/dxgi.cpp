/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "dxgi_device.hpp"
#include "dxgi_swapchain.hpp"
#include "d3d10/runtime_d3d10.hpp"
#include "d3d11/runtime_d3d11.hpp"

void dump_swapchain_desc(const DXGI_SWAP_CHAIN_DESC &desc)
{
	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Width                                   | " << std::setw(39) << desc.BufferDesc.Width << " |";
	LOG(INFO) << "  | Height                                  | " << std::setw(39) << desc.BufferDesc.Height << " |";
	LOG(INFO) << "  | RefreshRate                             | " << std::setw(19) << desc.BufferDesc.RefreshRate.Numerator << ' ' << std::setw(19) << desc.BufferDesc.RefreshRate.Denominator << " |";
	LOG(INFO) << "  | Format                                  | " << std::setw(39) << desc.BufferDesc.Format << " |";
	LOG(INFO) << "  | ScanlineOrdering                        | " << std::setw(39) << desc.BufferDesc.ScanlineOrdering << " |";
	LOG(INFO) << "  | Scaling                                 | " << std::setw(39) << desc.BufferDesc.Scaling << " |";
	LOG(INFO) << "  | SampleCount                             | " << std::setw(39) << desc.SampleDesc.Count << " |";
	LOG(INFO) << "  | SampleQuality                           | " << std::setw(39) << desc.SampleDesc.Quality << " |";
	LOG(INFO) << "  | BufferUsage                             | " << std::setw(39) << desc.BufferUsage << " |";
	LOG(INFO) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount << " |";
	LOG(INFO) << "  | OutputWindow                            | " << std::setw(39) << desc.OutputWindow << " |";
	LOG(INFO) << "  | Windowed                                | " << std::setw(39) << (desc.Windowed != FALSE ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << desc.Flags << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	if (desc.SampleDesc.Count > 1)
	{
		LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
	}
}
void dump_swapchain_desc(const DXGI_SWAP_CHAIN_DESC1 &desc)
{
	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Width                                   | " << std::setw(39) << desc.Width << " |";
	LOG(INFO) << "  | Height                                  | " << std::setw(39) << desc.Height << " |";
	LOG(INFO) << "  | Format                                  | " << std::setw(39) << desc.Format << " |";
	LOG(INFO) << "  | Stereo                                  | " << std::setw(39) << (desc.Stereo != FALSE ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | SampleCount                             | " << std::setw(39) << desc.SampleDesc.Count << " |";
	LOG(INFO) << "  | SampleQuality                           | " << std::setw(39) << desc.SampleDesc.Quality << " |";
	LOG(INFO) << "  | BufferUsage                             | " << std::setw(39) << desc.BufferUsage << " |";
	LOG(INFO) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount << " |";
	LOG(INFO) << "  | Scaling                                 | " << std::setw(39) << desc.Scaling << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect << " |";
	LOG(INFO) << "  | AlphaMode                               | " << std::setw(39) << desc.AlphaMode << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << desc.Flags << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	if (desc.SampleDesc.Count > 1)
	{
		LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
	}
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	IUnknown *device_orig = pDevice;
	D3D10Device *device_d3d10 = nullptr;
	D3D11Device *device_d3d11 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d10)))
	{
		device_orig = device_d3d10->_orig;

		device_d3d10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d11)))
	{
		device_orig = device_d3d11->_orig;

		device_d3d11->Release();
	}

	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(&IDXGIFactory_CreateSwapChain)(pFactory, device_orig, pDesc, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory::CreateSwapChain' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGISwapChain *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (device_d3d10 != nullptr)
	{
		device_d3d10->AddRef();

		const auto runtime = std::make_shared<reshade::d3d10::runtime_d3d10>(device_d3d10->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d10->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d10, swapchain, runtime);
	}
	else if (device_d3d11 != nullptr)
	{
		device_d3d11->AddRef();

		const auto runtime = std::make_shared<reshade::d3d11::runtime_d3d11>(device_d3d11->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d11->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d11, swapchain, runtime);
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGISwapChain' object " << *ppSwapChain;
#endif

	return S_OK;
}

// IDXGIFactory2
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *device_orig = pDevice;
	D3D10Device *device_d3d10 = nullptr;
	D3D11Device *device_d3d11 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d10)))
	{
		device_orig = device_d3d10->_orig;

		device_d3d10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d11)))
	{
		device_orig = device_d3d11->_orig;

		device_d3d11->Release();
	}

	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, device_orig, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForHwnd' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (device_d3d10 != nullptr)
	{
		device_d3d10->AddRef();

		const auto runtime = std::make_shared<reshade::d3d10::runtime_d3d10>(device_d3d10->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d10->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d10, swapchain, runtime);
	}
	else if (device_d3d11 != nullptr)
	{
		device_d3d11->AddRef();

		const auto runtime = std::make_shared<reshade::d3d11::runtime_d3d11>(device_d3d11->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d11->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d11, swapchain, runtime);
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGISwapChain1' object " << *ppSwapChain;
#endif

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *device_orig = pDevice;
	D3D10Device *device_d3d10 = nullptr;
	D3D11Device *device_d3d11 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d10)))
	{
		device_orig = device_d3d10->_orig;

		device_d3d10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d11)))
	{
		device_orig = device_d3d11->_orig;

		device_d3d11->Release();
	}

	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, device_orig, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForCoreWindow' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (device_d3d10 != nullptr)
	{
		device_d3d10->AddRef();

		const auto runtime = std::make_shared<reshade::d3d10::runtime_d3d10>(device_d3d10->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d10->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d10, swapchain, runtime);
	}
	else if (device_d3d11 != nullptr)
	{
		device_d3d11->AddRef();

		const auto runtime = std::make_shared<reshade::d3d11::runtime_d3d11>(device_d3d11->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d11->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d11, swapchain, runtime);
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGISwapChain1' object " << *ppSwapChain;
#endif

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *device_orig = pDevice;
	D3D10Device *device_d3d10 = nullptr;
	D3D11Device *device_d3d11 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d10)))
	{
		device_orig = device_d3d10->_orig;

		device_d3d10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&device_d3d11)))
	{
		device_orig = device_d3d11->_orig;

		device_d3d11->Release();
	}

	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, device_orig, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForComposition' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (device_d3d10 != nullptr)
	{
		device_d3d10->AddRef();

		const auto runtime = std::make_shared<reshade::d3d10::runtime_d3d10>(device_d3d10->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d10->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d10, swapchain, runtime);
	}
	else if (device_d3d11 != nullptr)
	{
		device_d3d11->AddRef();

		const auto runtime = std::make_shared<reshade::d3d11::runtime_d3d11>(device_d3d11->_orig, swapchain);

		if (!runtime->on_init(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << runtime.get() << ".";
		}

		device_d3d11->_runtimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(device_d3d11, swapchain, runtime);
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGISwapChain1' object " << *ppSwapChain;
#endif

	return S_OK;
}

// DXGI
HOOK_EXPORT HRESULT WINAPI DXGIDumpJournal()
{
	assert(false);

	return E_NOTIMPL;
}
HOOK_EXPORT HRESULT WINAPI DXGIReportAdapterConfiguration()
{
	assert(false);

	return E_NOTIMPL;
}

HOOK_EXPORT HRESULT WINAPI DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, void **ppDevice)
{
	return reshade::hooks::call(&DXGID3D10CreateDevice)(hModule, pFactory, pAdapter, Flags, pUnknown, ppDevice);
}
HOOK_EXPORT HRESULT WINAPI DXGID3D10CreateLayeredDevice(void *pUnknown1, void *pUnknown2, void *pUnknown3, void *pUnknown4, void *pUnknown5)
{
	return reshade::hooks::call(&DXGID3D10CreateLayeredDevice)(pUnknown1, pUnknown2, pUnknown3, pUnknown4, pUnknown5);
}
HOOK_EXPORT SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	return reshade::hooks::call(&DXGID3D10GetLayeredDeviceSize)(pLayers, NumLayers);
}
HOOK_EXPORT HRESULT WINAPI DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	return reshade::hooks::call(&DXGID3D10RegisterLayers)(pLayers, NumLayers);
}

HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR riid_string[40];
	StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));

	LOG(INFO) << "Redirecting 'CreateDXGIFactory(" << riid_string << ", " << ppFactory << ")' ...";
	LOG(INFO) << "> Passing on to 'CreateDXGIFactory1':";

	return CreateDXGIFactory1(riid, ppFactory);
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR riid_string[40];
	StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));

	LOG(INFO) << "Redirecting 'CreateDXGIFactory1(" << riid_string << ", " << ppFactory << ")' ...";

	const HRESULT hr = reshade::hooks::call(&CreateDXGIFactory1)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory2), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory2), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory2), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGIFactory1' object " << *ppFactory;
#endif

	return S_OK;
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR riid_string[40];
	StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));

	LOG(INFO) << "Redirecting 'CreateDXGIFactory2(" << flags << ", " << riid_string << ", " << ppFactory << ")' ...";

	ULONGLONG verinfo_condition_mask = 0;
	VER_SET_CONDITION(verinfo_condition_mask, VER_MAJORVERSION, VER_EQUAL);
	VER_SET_CONDITION(verinfo_condition_mask, VER_MINORVERSION, VER_LESS_EQUAL);

	OSVERSIONINFOEX verinfo_windows8 = { sizeof(OSVERSIONINFOEX), 6, 2 };

	if (VerifyVersionInfo(&verinfo_windows8, VER_MAJORVERSION | VER_MINORVERSION, verinfo_condition_mask) != FALSE)
	{
		LOG(INFO) << "> Passing on to 'CreateDXGIFactory1':";

		return CreateDXGIFactory1(riid, ppFactory);
	}

#ifdef _DEBUG
	flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	const HRESULT hr = reshade::hooks::call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory2), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory2), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory2), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDXGIFactory2' object " << *ppFactory;
#endif

	return S_OK;
}
