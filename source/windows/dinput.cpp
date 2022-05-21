/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Set version to DirectInput 7
#define DIRECTINPUT_VERSION 0x0700

#include <dinput.h>
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

HOOK_EXPORT HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateA" << '(' << "hinst = " << hinst << ", dwVersion = " << std::hex << dwVersion << std::dec << ", ppDI = " << ppDI << ", punkOuter = " << punkOuter << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateA)(hinst, dwVersion, ppDI, punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInputCreateA" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW *ppDI, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateW" << '(' << "hinst = " << hinst << ", dwVersion = " << std::hex << dwVersion << std::dec << ", ppDI = " << ppDI << ", punkOuter = " << punkOuter << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateW)(hinst, dwVersion, ppDI, punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInputCreateW" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateEx" << '(' << "hinst = " << hinst << ", dwVersion = " << std::hex << dwVersion << std::dec << ", riidltf = " << riidltf << ", ppvOut = " << ppvOut << ", punkOuter = " << punkOuter << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateEx)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInputCreateEx" << " failed with error code " << hr << '.';
	}

	return hr;
}
