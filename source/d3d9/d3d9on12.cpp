/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9on12_device.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"
#include "hook_manager.hpp"

extern thread_local bool g_in_d3d9_runtime;
extern thread_local bool g_in_dxgi_runtime;

#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx

HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface);
HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface);

void init_device_proxy_for_d3d9on12(Direct3DDevice9 *device_proxy)
{
	IDirect3DDevice9 *device = device_proxy->_orig;
	IDirect3DDevice9On12 *d3d9on12_device = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&d3d9on12_device))))
	{
		com_ptr<ID3D12Device> d3d12_device;
		if (SUCCEEDED(d3d9on12_device->GetD3D12Device(IID_PPV_ARGS(&d3d12_device))))
		{
			if (const auto device_proxy_12 = get_private_pointer_d3dx<D3D12Device>(d3d12_device.get()))
			{
				device_proxy->_d3d9on12_device = new Direct3DDevice9On12(device_proxy, device_proxy_12, d3d9on12_device);
			}
		}
	}
}

extern "C" IDirect3D9 *WINAPI Direct3DCreate9On12(UINT SDKVersion, D3D9ON12_ARGS *pOverrideList, UINT NumOverrideEntries) // Export ordinal 20
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9On12)(SDKVersion, pOverrideList, NumOverrideEntries);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9On12" << '('
		<<   "SDKVersion = " << SDKVersion
		<< ", pOverrideList = " << pOverrideList
		<< ", NumOverrideEntries = " << NumOverrideEntries
		<< ')' << " ...";

	temp_mem<D3D9ON12_ARGS> override_list(NumOverrideEntries);
	for (UINT i = 0; i < NumOverrideEntries; ++i)
	{
		override_list[i] = pOverrideList[i];

		if (com_ptr<D3D12Device> device_proxy_12;
			override_list[i].pD3D12Device != nullptr &&
			SUCCEEDED(override_list[i].pD3D12Device->QueryInterface(&device_proxy_12)))
			override_list[i].pD3D12Device = device_proxy_12->_orig;

		for (UINT k = 0; k < override_list[i].NumQueues; ++k)
		{
			if (com_ptr<D3D12CommandQueue> command_queue_proxy;
				override_list[i].ppD3D12Queues[k] != nullptr &&
				SUCCEEDED(override_list[i].ppD3D12Queues[k]->QueryInterface(&command_queue_proxy)))
				override_list[i].ppD3D12Queues[k] = command_queue_proxy->_orig;
		}
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	IDirect3D9 *const res = reshade::hooks::call(Direct3DCreate9On12)(SDKVersion, override_list.p, NumOverrideEntries);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (res == nullptr)
	{
		LOG(WARN) << "Direct3DCreate9On12" << " failed.";
		return nullptr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", reshade::hooks::vtable_from_instance(res), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning " << "IDirect3D9" << " object " << res << '.';
#endif
	return res;
}

extern "C"     HRESULT WINAPI Direct3DCreate9On12Ex(UINT SDKVersion, D3D9ON12_ARGS *pOverrideList, UINT NumOverrideEntries, IDirect3D9Ex **ppOutputInterface) // Export ordinal 21
{
	if (g_in_d3d9_runtime)
		return reshade::hooks::call(Direct3DCreate9On12Ex)(SDKVersion, pOverrideList, NumOverrideEntries, ppOutputInterface);

	LOG(INFO) << "Redirecting " << "Direct3DCreate9On12Ex" << '('
		<<   "SDKVersion = " << SDKVersion
		<< ", pOverrideList = " << pOverrideList
		<< ", NumOverrideEntries = " << NumOverrideEntries
		<< ", ppOutputInterface = " << ppOutputInterface
		<< ')' << " ...";

	temp_mem<D3D9ON12_ARGS> override_list(NumOverrideEntries);
	for (UINT i = 0; i < NumOverrideEntries; ++i)
	{
		override_list[i] = pOverrideList[i];

		if (com_ptr<D3D12Device> device_proxy_12;
			override_list[i].pD3D12Device != nullptr &&
			SUCCEEDED(override_list[i].pD3D12Device->QueryInterface(&device_proxy_12)))
			override_list[i].pD3D12Device = device_proxy_12->_orig;

		for (UINT k = 0; k < override_list[i].NumQueues; ++k)
		{
			if (com_ptr<D3D12CommandQueue> command_queue_proxy;
				override_list[i].ppD3D12Queues[k] != nullptr &&
				SUCCEEDED(override_list[i].ppD3D12Queues[k]->QueryInterface(&command_queue_proxy)))
				override_list[i].ppD3D12Queues[k] = command_queue_proxy->_orig;
		}
	}

	assert(!g_in_dxgi_runtime);
	g_in_d3d9_runtime = g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(Direct3DCreate9On12Ex)(SDKVersion, override_list.p, NumOverrideEntries, ppOutputInterface);
	g_in_d3d9_runtime = g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "Direct3DCreate9On12Ex" << " failed with error code " << hr << '.';
		return hr;
	}

	reshade::hooks::install("IDirect3D9::CreateDevice", reshade::hooks::vtable_from_instance(*ppOutputInterface), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));
	reshade::hooks::install("IDirect3D9Ex::CreateDeviceEx", reshade::hooks::vtable_from_instance(*ppOutputInterface), 20, reinterpret_cast<reshade::hook::address>(&IDirect3D9Ex_CreateDeviceEx));

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning " << "IDirect3D9Ex" << " object " << *ppOutputInterface << '.';
#endif
	return hr;
}
