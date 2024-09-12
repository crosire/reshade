/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Set version to DirectInput 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN pUnkOuter)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting DirectInput8Create(hinst = %p, dwVersion = %x, riidltf = %s, ppvOut = %p, pUnkOuter = %p) ...",
		hinst, dwVersion, reshade::log::iid_to_string(riidltf).c_str(), ppvOut, pUnkOuter);

	// Pass through without doing anything, this export only exists to allow wrapper installation as dinput8.dll
	return reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
