/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "hook_manager.hpp"
#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "runtime_d3d9.hpp"

#pragma region Undefine Function Names
#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx
#pragma endregion

void dump_present_parameters(const D3DPRESENT_PARAMETERS &pp)
{
	LOG(INFO) << "> Dumping presentation parameters:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | BackBufferWidth                         | " << std::setw(39) << pp.BackBufferWidth << " |";
	LOG(INFO) << "  | BackBufferHeight                        | " << std::setw(39) << pp.BackBufferHeight << " |";
	LOG(INFO) << "  | BackBufferFormat                        | " << std::setw(39) << pp.BackBufferFormat << " |";
	LOG(INFO) << "  | BackBufferCount                         | " << std::setw(39) << pp.BackBufferCount << " |";
	LOG(INFO) << "  | MultiSampleType                         | " << std::setw(39) << pp.MultiSampleType << " |";
	LOG(INFO) << "  | MultiSampleQuality                      | " << std::setw(39) << pp.MultiSampleQuality << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << pp.SwapEffect << " |";
	LOG(INFO) << "  | DeviceWindow                            | " << std::setw(39) << pp.hDeviceWindow << " |";
	LOG(INFO) << "  | Windowed                                | " << std::setw(39) << (pp.Windowed != FALSE ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | EnableAutoDepthStencil                  | " << std::setw(39) << (pp.EnableAutoDepthStencil ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | AutoDepthStencilFormat                  | " << std::setw(39) << pp.AutoDepthStencilFormat << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << pp.Flags << std::dec << " |";
	LOG(INFO) << "  | FullScreen_RefreshRateInHz              | " << std::setw(39) << pp.FullScreen_RefreshRateInHz << " |";
	LOG(INFO) << "  | PresentationInterval                    | " << std::setw(39) << std::hex << pp.PresentationInterval << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	if (pp.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
	}
}

// IDirect3D9
HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9::CreateDevice" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << std::hex << BehaviorFlags << std::dec << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		LOG(WARNING) << "Adapter group devices are unsupported.";

		return D3DERR_NOTAVAILABLE;
	}

	dump_present_parameters(*pPresentationParameters);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;

	if (use_software_rendering)
	{
		LOG(WARNING) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	const HRESULT hr = reshade::hooks::call(&IDirect3D9_CreateDevice)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3D9::CreateDevice' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDirect3DDevice9 *const device = *ppReturnedDeviceInterface;

	if (use_software_rendering)
	{
		device->SetSoftwareVertexProcessing(TRUE);
	}

	if (pPresentationParameters->Flags & D3DPRESENTFLAG_VIDEO)
	{
		LOG(WARNING) << "> Skipping device due to video swapchain.";
	}
	else if (DeviceType == D3DDEVTYPE_NULLREF)
	{
		LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
	}
	else
	{
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);

		D3DPRESENT_PARAMETERS pp;
		swapchain->GetPresentParameters(&pp);

		const auto runtime = std::make_shared<reshade::d3d9::runtime_d3d9>(device, swapchain);

		if (!runtime->on_init(pp))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
		}

		const auto device_proxy = new Direct3DDevice9(device);
		const auto swapchain_proxy = new Direct3DSwapChain9(device_proxy, swapchain, runtime);

		device_proxy->_implicit_swapchain = swapchain_proxy;
		device_proxy->_use_software_rendering = use_software_rendering;
		*ppReturnedDeviceInterface = device_proxy;

		if (pp.EnableAutoDepthStencil)
		{
			device->GetDepthStencilSurface(&device_proxy->_auto_depthstencil);
			device_proxy->SetDepthStencilSurface(device_proxy->_auto_depthstencil.get());
		}

		// Upgrade to extended interface if available
		com_ptr<IDirect3DDevice9Ex> deviceex;
		device_proxy->QueryInterface(IID_PPV_ARGS(&deviceex));
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDirect3DDevice9' object " << *ppReturnedDeviceInterface;
#endif

	return D3D_OK;
}

// IDirect3D9Ex
HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9Ex::CreateDeviceEx" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << std::hex << BehaviorFlags << std::dec << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if ((BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != 0)
	{
		LOG(WARNING) << "Adapter group devices are unsupported.";

		return D3DERR_NOTAVAILABLE;
	}

	dump_present_parameters(*pPresentationParameters);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;

	if (use_software_rendering)
	{
		LOG(WARNING) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	const HRESULT hr = reshade::hooks::call(&IDirect3D9Ex_CreateDeviceEx)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3D9Ex::CreateDeviceEx' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	IDirect3DDevice9Ex *const device = *ppReturnedDeviceInterface;

	if (use_software_rendering)
	{
		device->SetSoftwareVertexProcessing(TRUE);
	}

	if (pPresentationParameters->Flags & D3DPRESENTFLAG_VIDEO)
	{
		LOG(WARNING) << "> Skipping device due to video swapchain.";
	}
	else if (DeviceType == D3DDEVTYPE_NULLREF)
	{
		LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
	}
	else
	{
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);

		D3DPRESENT_PARAMETERS pp;
		swapchain->GetPresentParameters(&pp);

		const auto runtime = std::make_shared<reshade::d3d9::runtime_d3d9>(device, swapchain);

		if (!runtime->on_init(pp))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
		}

		const auto device_proxy = new Direct3DDevice9(device);
		const auto swapchain_proxy = new Direct3DSwapChain9(device_proxy, swapchain, runtime);

		device_proxy->_implicit_swapchain = swapchain_proxy;
		device_proxy->_use_software_rendering = use_software_rendering;
		*ppReturnedDeviceInterface = device_proxy;

		if (pp.EnableAutoDepthStencil)
		{
			device->GetDepthStencilSurface(&device_proxy->_auto_depthstencil);
			device_proxy->SetDepthStencilSurface(device_proxy->_auto_depthstencil.get());
		}
	}

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDirect3DDevice9Ex' object " << *ppReturnedDeviceInterface;
#endif

	return D3D_OK;
}

// PIX
HOOK_EXPORT int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);

	return 0;
}
HOOK_EXPORT int WINAPI D3DPERF_EndEvent()
{
	return 0;
}
HOOK_EXPORT void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
HOOK_EXPORT void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
HOOK_EXPORT BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	return FALSE;
}
HOOK_EXPORT void WINAPI D3DPERF_SetOptions(DWORD dwOptions)
{
	UNREFERENCED_PARAMETER(dwOptions);

#ifdef _DEBUG
	reshade::hooks::call(&D3DPERF_SetOptions)(0);
#endif
}
HOOK_EXPORT DWORD WINAPI D3DPERF_GetStatus()
{
	return 0;
}

// D3D9
HOOK_EXPORT IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9" << "(" << SDKVersion << ")' ...";

	IDirect3D9 *const res = reshade::hooks::call(&Direct3DCreate9)(SDKVersion);

	if (res == nullptr)
	{
		LOG(WARNING) << "> 'Direct3DCreate9' failed!";

		return nullptr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", vtable_from_instance(res), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDirect3D9' object " << res;
#endif

	return res;
}
HOOK_EXPORT HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9Ex" << "(" << SDKVersion << ", " << ppD3D << ")' ...";

	const HRESULT hr = reshade::hooks::call(&Direct3DCreate9Ex)(SDKVersion, ppD3D);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'Direct3DCreate9Ex' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", vtable_from_instance(*ppD3D), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));
	reshade::hooks::install("IDirect3D9Ex::CreateDeviceEx", vtable_from_instance(*ppD3D), 20, reinterpret_cast<reshade::hook::address>(&IDirect3D9Ex_CreateDeviceEx));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning 'IDirect3D9Ex' object " << *ppD3D;
#endif

	return D3D_OK;
}
