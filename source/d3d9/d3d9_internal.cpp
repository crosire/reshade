/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook_manager.hpp"
#include <Windows.h>

// The following hooks can only be called when installed as d3d9.dll, since they have no names in the export table, just ordinals
// Therefore can assume that the export module handle points toward the system d3d9.dll
// Note that the hooks below are sometimes called before the CRT is initialized, so keep logic as simple as possible!
extern HMODULE g_export_module_handle;

HOOK_EXPORT void WINAPI Direct3D9ForceHybridEnumeration(UINT Mode)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(16));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9ForceHybridEnumeration)>(proc)(Mode);
	}
}

// This is called when the 'DXMaximizedWindowedMode' compatibility fix is active (by AcLayers.dll shim), via export ordinal 17
HOOK_EXPORT void WINAPI Direct3D9SetMaximizedWindowedModeShim(int Unknown, UINT Mode)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(17));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9SetMaximizedWindowedModeShim)>(proc)(Unknown, Mode);
	}
}

HOOK_EXPORT void WINAPI Direct3D9SetSwapEffectUpgradeShim(int Unknown)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(18));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9SetSwapEffectUpgradeShim)>(proc)(Unknown);
	}
}

// This is called when the 'D3D9On12Enabler' compatibility fix is active (by AcGenral.dll shim), via export ordinal 19
HOOK_EXPORT void WINAPI Direct3D9Force9on12(int Unknown)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(19));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9Force9on12)>(proc)(Unknown);
	}
}

// This is called when the 'DXMaximizedWindowedHwndOverride' compatibility fix is active (by AcLayers.dll shim), via export ordinal 22
HOOK_EXPORT void WINAPI Direct3D9SetMaximizedWindowHwndOverride(int Unknown)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(22));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9SetMaximizedWindowHwndOverride)>(proc)(Unknown);
	}
}

// This is called when the 'D3D9On12VendorIDLie' compatibility fix is active (by AcGenral.dll shim), via export ordinal 23
HOOK_EXPORT void WINAPI Direct3D9SetVendorIDLieFor9on12(int Unknown)
{
	if (g_export_module_handle != nullptr)
	{
		const FARPROC proc = GetProcAddress(g_export_module_handle, reinterpret_cast<LPCSTR>(23));
		if (proc != nullptr)
			reinterpret_cast<decltype(&Direct3D9SetVendorIDLieFor9on12)>(proc)(Unknown);
	}
}

HOOK_EXPORT void WINAPI Direct3D9EnableMaximizedWindowedModeShim(int Unknown)
{
	Direct3D9SetMaximizedWindowedModeShim(Unknown, 1);
}
