/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

// Set version to DirectInput 8 to avoid warning
#define DIRECTINPUT_VERSION 0x0800

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <dinput.h>

HOOK_EXPORT HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInput8Create" << '(' << "hinst = " << hinst << ", dwVersion = " << dwVersion << ", riidltf = " << riidltf << ", ppvOut = " << ppvOut << ", punkOuter = " << punkOuter << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInput8Create" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
