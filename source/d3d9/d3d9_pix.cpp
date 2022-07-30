/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "hook_manager.hpp"
#include <Windows.h>
#include <d3d9types.h>

HOOK_EXPORT int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName)
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_BeginEvent);
	return trampoline(col, wszName);
#else
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);

	return 0;
#endif
}
HOOK_EXPORT int WINAPI D3DPERF_EndEvent()
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_EndEvent);
	return trampoline();
#else
	return 0;
#endif
}

HOOK_EXPORT void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName)
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_SetMarker);
	trampoline(col, wszName);
#else
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
#endif
}
HOOK_EXPORT void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
HOOK_EXPORT void WINAPI D3DPERF_SetOptions(DWORD dwOptions)
{
	UNREFERENCED_PARAMETER(dwOptions);

#ifndef NDEBUG // Enable PIX in debug builds (calling 'D3DPERF_SetOptions(1)' disables profiling/analysis tools, so revert that)
	reshade::hooks::call(D3DPERF_SetOptions)(0);
#endif
}

HOOK_EXPORT BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	return FALSE;
}

HOOK_EXPORT DWORD WINAPI D3DPERF_GetStatus()
{
#ifndef NDEBUG
	return reshade::hooks::call(D3DPERF_GetStatus)();
#else
	return 0;
#endif
}
