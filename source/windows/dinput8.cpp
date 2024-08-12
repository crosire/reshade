/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Set version to DirectInput 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN pUnkOuter)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting DirectInput8Create(hinst = %p, dwVersion = %x, riidltf = %s, ppvOut = %p, pUnkOuter = %p) ...",
		hinst, dwVersion, reshade::log::iid_to_string(riidltf).c_str(), ppvOut, pUnkOuter);

	return reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
