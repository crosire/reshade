/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "hook_manager.hpp"
#include <cassert>
#include <Windows.h>

// The following hooks can only be called when installed as d3d12.dll, since they are skipped otherwise (see hook_manager.cpp)
// Therefore can assume that the export module handle points toward the system d3d12.dll
extern HMODULE g_export_module_handle;

extern "C" UINT64 WINAPI D3D12PIXEventsReplaceBlock(UINT64 *pUnknown1, UINT64 Unknown2)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "D3D12PIXEventsReplaceBlock");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&D3D12PIXEventsReplaceBlock)>(proc)(pUnknown1, Unknown2);
	else
		return 0;
}

extern "C" LPVOID WINAPI D3D12PIXGetThreadInfo(UINT64 Unknown1, UINT64 Unknown2)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "D3D12PIXGetThreadInfo");
	if (proc != nullptr)
		return reinterpret_cast<decltype(&D3D12PIXGetThreadInfo)>(proc)(Unknown1, Unknown2);
	else
		return nullptr;
}

extern "C" void    WINAPI D3D12PIXNotifyWakeFromFenceSignal(UINT64 Unknown1, UINT64 Unknown2)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "D3D12PIXNotifyWakeFromFenceSignal");
	if (proc != nullptr)
		reinterpret_cast<decltype(&D3D12PIXNotifyWakeFromFenceSignal)>(proc)(Unknown1, Unknown2);
}

extern "C" void   WINAPI D3D12PIXReportCounter(UINT64 Unknown1, UINT64 Unknown2, LPCWSTR Unknown3, UINT64 Unknown4, UINT64 Unknown5)
{
	reshade::hooks::ensure_export_module_loaded();
	assert(g_export_module_handle != nullptr);

	const FARPROC proc = GetProcAddress(g_export_module_handle, "D3D12PIXReportCounter");
	if (proc != nullptr)
		reinterpret_cast<decltype(&D3D12PIXReportCounter)>(proc)(Unknown1, Unknown2, Unknown3, Unknown4, Unknown5);
}
