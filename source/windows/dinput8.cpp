/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Set version to DirectInput 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "hook_manager.hpp"
#include "input.hpp"

// It is technically possible to associate these hooks back to a device (cooperative level), but it may not be the same window as ReShade renders on
extern bool is_blocking_mouse_input(reshade::input::window_handle window = reshade::input::any_window);
extern bool is_blocking_keyboard_input(reshade::input::window_handle window = reshade::input::any_window);

#define IDirectInputDevice8_SetCooperativeLevel_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice8##encoding##_SetCooperativeLevel(IDirectInputDevice8##encoding *pDevice, HWND hwnd, DWORD dwFlags) \
	{ \
		reshade::log::message(reshade::log::level::info, "Redirecting IDirectInputDevice8::SetCooperativeLevel(this = %p, hwnd = %p, dwFlags = %#x) ...", pDevice, hwnd, dwFlags); \
		\
		if (dwFlags & DISCL_EXCLUSIVE) \
		{ \
			DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
			pDevice->GetDeviceInfo(&info); \
			if (LOBYTE(info.dwDevType) == DI8DEVTYPE_MOUSE || \
				LOBYTE(info.dwDevType) == DI8DEVTYPE_KEYBOARD) \
			{ \
				/* Need to remove exclusive flag, otherwise DirectInput will block input window messages and input.cpp will not receive input anymore */ \
				dwFlags = (dwFlags & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE; \
				\
				reshade::log::message(reshade::log::level::info, "> Replacing flags with %#x.", dwFlags); \
			} \
		} \
		\
		return reshade::hooks::call(IDirectInputDevice8##encoding##_SetCooperativeLevel, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, hwnd, dwFlags); \
	}

IDirectInputDevice8_SetCooperativeLevel_Impl(13,A)
IDirectInputDevice8_SetCooperativeLevel_Impl(13,W)

#define IDirectInputDevice8_GetDeviceState_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice8##encoding##_GetDeviceState(IDirectInputDevice8##encoding *pDevice, DWORD cbData, LPVOID lpvData) \
	{ \
		DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
		pDevice->GetDeviceInfo(&info); \
		switch (LOBYTE(info.dwDevType)) \
		{ \
		case DI8DEVTYPE_MOUSE: \
			if (is_blocking_mouse_input()) \
			{ \
				std::memset(lpvData, 0, cbData); \
				return DI_OK; \
			} \
			break; \
		case DI8DEVTYPE_KEYBOARD: \
			if (is_blocking_keyboard_input()) \
			{ \
				std::memset(lpvData, 0, cbData); \
				return DI_OK; \
			} \
			break; \
		} \
		\
		return reshade::hooks::call(IDirectInputDevice8##encoding##_GetDeviceState, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, cbData, lpvData); \
	}

IDirectInputDevice8_GetDeviceState_Impl(9,A)
IDirectInputDevice8_GetDeviceState_Impl(9,W)

#define IDirectInputDevice8_GetDeviceData_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice8##encoding##_GetDeviceData(IDirectInputDevice8##encoding *pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) \
	{ \
		if ((dwFlags & DIGDD_PEEK) == 0) \
		{ \
			DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
			pDevice->GetDeviceInfo(&info); \
			switch (LOBYTE(info.dwDevType)) \
			{ \
			case DI8DEVTYPE_MOUSE: \
				if (is_blocking_mouse_input()) \
				{ \
					*pdwInOut = 0; \
					return DI_OK; \
				} \
				break; \
			case DI8DEVTYPE_KEYBOARD: \
				if (is_blocking_keyboard_input()) \
				{ \
					*pdwInOut = 0; \
					return DI_OK; \
				} \
				break; \
			} \
		} \
		\
		return reshade::hooks::call(IDirectInputDevice8##encoding##_GetDeviceData, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags); \
	}

IDirectInputDevice8_GetDeviceData_Impl(10,A)
IDirectInputDevice8_GetDeviceData_Impl(10,W)

#define IDirectInput8_CreateDevice_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInput8##encoding##_CreateDevice(IDirectInput8##encoding *pDI, REFGUID rguid, LPDIRECTINPUTDEVICE8##encoding *lplpDirectInputDevice, LPUNKNOWN pUnkOuter) \
	{ \
		reshade::log::message( \
			reshade::log::level::info, \
			"Redirecting IDirectInput8" #encoding "::CreateDevice(this = %p, rguid = %s, lplpDirectInputDevice = %p, pUnkOuter = %p) ...", \
			pDI, reshade::log::iid_to_string(rguid).c_str(), lplpDirectInputDevice, pUnkOuter); \
		\
		const HRESULT hr = reshade::hooks::call(IDirectInput8##encoding##_CreateDevice, reshade::hooks::vtable_from_instance(pDI) + vtable_index)(pDI, rguid, lplpDirectInputDevice, pUnkOuter); \
		if (SUCCEEDED(hr)) \
		{ \
			reshade::hooks::install("IDirectInputDevice8" #encoding "::GetDeviceState", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 9, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice8##encoding##_GetDeviceState)); \
			reshade::hooks::install("IDirectInputDevice8" #encoding "::GetDeviceData", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 10, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice8##encoding##_GetDeviceData)); \
			reshade::hooks::install("IDirectInputDevice8" #encoding "::SetCooperativeLevel", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 13, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice8##encoding##_SetCooperativeLevel)); \
		} \
		else \
		{ \
			reshade::log::message(reshade::log::level::warning, "IDirectInput8" #encoding "::CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str()); \
		} \
		return hr; \
	}

IDirectInput8_CreateDevice_Impl(3,A)
IDirectInput8_CreateDevice_Impl(3,W)

extern "C" HRESULT WINAPI HookDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN pUnkOuter)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting DirectInput8Create(hinst = %p, dwVersion = %x, riidltf = %s, ppvOut = %p, pUnkOuter = %p) ...",
		hinst, dwVersion, reshade::log::iid_to_string(riidltf).c_str(), ppvOut, pUnkOuter);

	const HRESULT hr = reshade::hooks::call(HookDirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
	if (SUCCEEDED(hr))
	{
		IUnknown *const factory = static_cast<IUnknown *>(*ppvOut);

		if (riidltf == IID_IDirectInput8W)
			reshade::hooks::install("IDirectInput8W::CreateDevice", reshade::hooks::vtable_from_instance(static_cast <IDirectInput8W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput8W_CreateDevice));
		if (riidltf == IID_IDirectInput8A)
			reshade::hooks::install("IDirectInput8A::CreateDevice", reshade::hooks::vtable_from_instance(static_cast <IDirectInput8A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput8A_CreateDevice));
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "DirectInput8Create failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;

	// Pass through without doing anything, this export only exists to allow wrapper installation as dinput8.dll
	return reshade::hooks::call(HookDirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
