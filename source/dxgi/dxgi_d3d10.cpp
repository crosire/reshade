/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "hook_manager.hpp"
#include <dxgi.h>
#include <cassert>

// The following hooks can only be called when installed as dxgi.dll, since they are skipped otherwise (see hook_manager.cpp)
// Therefore can assume that the export module handle points toward the system dxgi.dll
extern HMODULE g_export_module_handle;

extern "C" BOOL    WINAPI CompatValue(LPCSTR szName, UINT64 *pValue)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "CompatValue");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&CompatValue)>(proc)(szName, pValue);
	else
		return FALSE;
}
extern "C" BOOL    WINAPI CompatString(LPCSTR szName, ULONG *pSize, LPSTR lpData, bool Flag)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "CompatString");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&CompatString)>(proc)(szName, pSize, lpData, Flag);
	else
		return FALSE;
}

extern "C" HRESULT WINAPI DXGIDumpJournal(void *pfnCallback)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGIDumpJournal");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGIDumpJournal)>(proc)(pfnCallback);
	else
		return E_NOTIMPL;
}

extern "C" HRESULT WINAPI DXGIReportAdapterConfiguration(void *pAdapterInfo)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGIReportAdapterConfiguration");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGIReportAdapterConfiguration)>(proc)(pAdapterInfo);
	else
		return E_NOTIMPL;
}

// These are actually called internally by the Direct3D driver on some versions of Windows, so just pass them through
extern "C" HRESULT WINAPI DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, const void *pFeatureLevels, UINT FeatureLevels, void **ppDevice)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGID3D10CreateDevice");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGID3D10CreateDevice)>(proc)(hModule, pFactory, pAdapter, Flags, pFeatureLevels, FeatureLevels, ppDevice);
	else
		return E_NOTIMPL; // Starting with Windows 8 these are no longer implemented and always return 'E_NOTIMPL' either way
}

extern "C" HRESULT WINAPI DXGID3D10CreateLayeredDevice(IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, REFIID riid, void **ppDevice)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGID3D10CreateLayeredDevice");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGID3D10CreateLayeredDevice)>(proc)(pAdapter, Flags, pUnknown, riid, ppDevice);
	else
		return E_NOTIMPL;
}

extern "C" void    WINAPI DXGID3D10ETWRundown()
{
}

extern "C" HRESULT WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGID3D10GetLayeredDeviceSize");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGID3D10GetLayeredDeviceSize)>(proc)(pLayers, NumLayers);
	else
		return E_NOTIMPL;
}

extern "C" HRESULT WINAPI DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "DXGID3D10RegisterLayers");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&DXGID3D10RegisterLayers)>(proc)(pLayers, NumLayers);
	else
		return E_NOTIMPL;
}
