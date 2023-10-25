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
	LOG(INFO) << "Redirecting " << "DirectInput8Create" << '('
		<<   "hinst = " << hinst
		<< ", dwVersion = " << std::hex << dwVersion << std::dec
		<< ", riidltf = " << riidltf
		<< ", ppvOut = " << ppvOut
		<< ", pUnkOuter = " << pUnkOuter
		<< ')' << " ...";

	return reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
