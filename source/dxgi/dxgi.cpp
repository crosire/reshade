/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "dxgi_device.hpp"
#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d10/runtime_d3d10.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "d3d12/runtime_d3d12.hpp"

static void query_device(IUnknown *&device, com_ptr<D3D10Device> &device_d3d10, com_ptr<D3D11Device> &device_d3d11, com_ptr<D3D12CommandQueue> &command_queue_d3d12)
{
	if (SUCCEEDED(device->QueryInterface(&device_d3d10)))
		device = device_d3d10->_orig;
	else if (SUCCEEDED(device->QueryInterface(&device_d3d11)))
		device = device_d3d11->_orig; // Set device pointer back to original object so that the swapchain creation functions work as expected
	else if (SUCCEEDED(device->QueryInterface(&command_queue_d3d12)))
		device = command_queue_d3d12->_orig;
}

static void dump_swapchain_desc(const DXGI_SWAP_CHAIN_DESC &desc)
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
		LOG(WARN) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
}
static void dump_swapchain_desc(const DXGI_SWAP_CHAIN_DESC1 &desc)
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
		LOG(WARN) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
}

template <typename T>
static void init_reshade_runtime_d3d(T *&swapchain, com_ptr<D3D10Device> &device_d3d10, com_ptr<D3D11Device> &device_d3d11, com_ptr<D3D12CommandQueue> &command_queue_d3d12)
{
	DXGI_SWAP_CHAIN_DESC desc;
	if (FAILED(swapchain->GetDesc(&desc)))
		return; // This doesn't happen, but let's be safe

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARN) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (device_d3d10 != nullptr)
	{
		const auto runtime = std::make_shared<reshade::d3d10::runtime_d3d10>(device_d3d10->_orig, swapchain);

		if (!runtime->on_init(desc))
			LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << runtime.get() << '.';

		device_d3d10->_runtimes.push_back(runtime);

		swapchain = new DXGISwapChain(device_d3d10.get(), swapchain, runtime);
	}
	else if (device_d3d11 != nullptr)
	{
		const auto runtime = std::make_shared<reshade::d3d11::runtime_d3d11>(device_d3d11->_orig, swapchain);

		if (!runtime->on_init(desc))
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << runtime.get() << '.';

		device_d3d11->_runtimes.push_back(runtime);

		swapchain = new DXGISwapChain(device_d3d11.get(), swapchain, runtime); // Overwrite returned swapchain pointer with hooked object
	}
	else if (command_queue_d3d12 != nullptr)
	{
		if (com_ptr<IDXGISwapChain3> swapchain3; SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			const auto runtime = std::make_shared<reshade::d3d12::runtime_d3d12>(command_queue_d3d12->_device->_orig, command_queue_d3d12->_orig, swapchain3.get());

			if (!runtime->on_init(desc))
				LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << runtime.get() << '.';

			swapchain = new DXGISwapChain(command_queue_d3d12->_device, swapchain3.get(), runtime);
		}
		else
		{
			LOG(WARN) << "> Skipping swap chain because it is missing support for the IDXGISwapChain3 interface.";
		}
	}
	else
	{
		LOG(WARN) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning IDXGISwapChain object " << swapchain << '.';
#endif
}

reshade::log::message &operator<<(reshade::log::message &m, REFIID riid)
{
	OLECHAR riid_string[40];
	StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));
	return m << riid_string;
}

HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting IDXGIFactory::CreateSwapChain" << '(' << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<D3D10Device> device_d3d10;
	com_ptr<D3D11Device> device_d3d11;
	com_ptr<D3D12CommandQueue> command_queue_d3d12;
	query_device(pDevice,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);
	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(IDXGIFactory_CreateSwapChain)(pFactory, pDevice, pDesc, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARN) << "> IDXGIFactory::CreateSwapChain failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	init_reshade_runtime_d3d(*ppSwapChain,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);

	return hr;
}

HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting IDXGIFactory2::CreateSwapChainForHwnd" << '(' << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<D3D10Device> device_d3d10;
	com_ptr<D3D11Device> device_d3d11;
	com_ptr<D3D12CommandQueue> command_queue_d3d12;
	query_device(pDevice,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);
	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARN) << "> IDXGIFactory2::CreateSwapChainForHwnd failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	init_reshade_runtime_d3d(*ppSwapChain,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting IDXGIFactory2::CreateSwapChainForCoreWindow" << '(' << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<D3D10Device> device_d3d10;
	com_ptr<D3D11Device> device_d3d11;
	com_ptr<D3D12CommandQueue> command_queue_d3d12;
	query_device(pDevice,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);
	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARN) << "> IDXGIFactory2::CreateSwapChainForCoreWindow failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	init_reshade_runtime_d3d(*ppSwapChain,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting IDXGIFactory2::CreateSwapChainForComposition" << '(' << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	com_ptr<D3D10Device> device_d3d10;
	com_ptr<D3D11Device> device_d3d11;
	com_ptr<D3D12CommandQueue> command_queue_d3d12;
	query_device(pDevice,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);
	dump_swapchain_desc(*pDesc);

	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForComposition)(pFactory, pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARN) << "> IDXGIFactory2::CreateSwapChainForComposition failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	init_reshade_runtime_d3d(*ppSwapChain,
		device_d3d10,
		device_d3d11,
		command_queue_d3d12);

	return hr;
}

HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	LOG(INFO) << "Redirecting CreateDXGIFactory" << '(' << riid << ", " << ppFactory << ')' << " ...";
	LOG(INFO) << "> Passing on to CreateDXGIFactory1:";

	// DXGI1.1 should always be available, so to simplify code just 'CreateDXGIFactory' which is otherwise identical
	return CreateDXGIFactory1(riid, ppFactory);
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	LOG(INFO) << "Redirecting CreateDXGIFactory1" << '(' << riid << ", " << ppFactory << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARN) << "> CreateDXGIFactory1 failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));

	// Check for DXGI1.2 support and install IDXGIFactory2 hooks if it exists
	if (com_ptr<IDXGIFactory2> factory2; SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory2.get()), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory2.get()), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory2.get()), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning IDXGIFactory1 object " << *ppFactory << '.';
#endif
	return hr;
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	LOG(INFO) << "Redirecting CreateDXGIFactory2" << '(' << flags << ", " << riid << ", " << ppFactory << ')' << " ...";

	static const auto trampoline = reshade::hooks::call(CreateDXGIFactory2);

	// CreateDXGIFactory2 is not available on Windows 7, so fall back to CreateDXGIFactory1 if the application calls it
	// This needs to happen because some applications only check if CreateDXGIFactory2 exists, which is always the case if they load ReShade, to decide whether to call it or CreateDXGIFactory1
	if (trampoline == nullptr)
	{
		LOG(INFO) << "> Passing on to CreateDXGIFactory1:";

		return CreateDXGIFactory1(riid, ppFactory);
	}

	const HRESULT hr = trampoline(flags, riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARN) << "> CreateDXGIFactory2 failed with error code " << std::hex << hr << std::dec << '!';
		return hr;
	}

	// We can pretty much assume support for DXGI1.2 at this point
	IDXGIFactory2 *const factory = static_cast<IDXGIFactory2 *>(*ppFactory);
	
	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(&IDXGIFactory_CreateSwapChain));
	reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory), 15, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForHwnd));
	reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory), 16, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
	reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory), 24, reinterpret_cast<reshade::hook::address>(&IDXGIFactory2_CreateSwapChainForComposition));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning IDXGIFactory2 object " << *ppFactory << '.';
#endif
	return hr;
}
