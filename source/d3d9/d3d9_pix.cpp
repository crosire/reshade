/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "hook_manager.hpp"
#include <Windows.h>
#include <d3d9types.h>

extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName)
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_BeginEvent);
	if (trampoline == nullptr)
		return 0;
	return trampoline(col, wszName);
#else
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);

	return 0;
#endif
}
extern "C" int WINAPI D3DPERF_EndEvent()
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_EndEvent);
	if (trampoline == nullptr)
		return 0;
	return trampoline();
#else
	return 0;
#endif
}

extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName)
{
#ifndef NDEBUG
	static const auto trampoline = reshade::hooks::call(D3DPERF_SetMarker);
	if (trampoline == nullptr)
		return;
	trampoline(col, wszName);
#else
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
#endif
}
extern "C" void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
extern "C" void WINAPI D3DPERF_SetOptions(DWORD dwOptions)
{
	UNREFERENCED_PARAMETER(dwOptions);

#ifndef NDEBUG // Enable PIX in debug builds (calling 'D3DPERF_SetOptions(1)' disables profiling/analysis tools, so revert that)
	reshade::hooks::call(D3DPERF_SetOptions)(0);
#endif
}

extern "C" BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	return FALSE;
}

extern "C" DWORD WINAPI D3DPERF_GetStatus()
{
#ifndef NDEBUG
	return reshade::hooks::call(D3DPERF_GetStatus)();
#else
	return 0;
#endif
}
