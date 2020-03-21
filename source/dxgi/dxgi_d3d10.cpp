/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook_manager.hpp"
#include <dxgi.h>
#include <cassert>

// These are filtered out during hook installation (see hook_manager.cpp)
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

// These are actually called internally by the Direct3D driver on some versions of Windows, so just pass them through
HOOK_EXPORT HRESULT WINAPI DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, void **ppDevice)
{
	return reshade::hooks::call(DXGID3D10CreateDevice)(hModule, pFactory, pAdapter, Flags, pUnknown, ppDevice);
}
HOOK_EXPORT HRESULT WINAPI DXGID3D10CreateLayeredDevice(void *pUnknown1, void *pUnknown2, void *pUnknown3, void *pUnknown4, void *pUnknown5)
{
	return reshade::hooks::call(DXGID3D10CreateLayeredDevice)(pUnknown1, pUnknown2, pUnknown3, pUnknown4, pUnknown5);
}
HOOK_EXPORT  SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	return reshade::hooks::call(DXGID3D10GetLayeredDeviceSize)(pLayers, NumLayers);
}
HOOK_EXPORT HRESULT WINAPI DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	return reshade::hooks::call(DXGID3D10RegisterLayers)(pLayers, NumLayers);
}
