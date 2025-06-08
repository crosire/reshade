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

#define IDirectInputDevice8_GetDeviceState_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice8##encoding##_GetDeviceState(IDirectInputDevice8##encoding *pDevice, DWORD cbData, LPVOID lpvData) \
	{ \
		static const auto trampoline = reshade::hooks::call(IDirectInputDevice8##encoding##_GetDeviceState, reshade::hooks::vtable_from_instance(pDevice) + vtable_index); \
		\
		const HRESULT hr = trampoline(pDevice, cbData, lpvData); \
		if (SUCCEEDED(hr)) \
		{ \
			DIDEVCAPS caps = { sizeof(caps) }; \
			pDevice->GetCapabilities(&caps); \
			\
			switch (LOBYTE(caps.dwDevType)) \
			{ \
			case DI8DEVTYPE_MOUSE: \
				if (reshade::input::is_blocking_any_mouse_input() && (cbData == sizeof(DIMOUSESTATE) || cbData == sizeof(DIMOUSESTATE2))) \
					/* Only clear button state, to prevent camera resetting when overlay is opened in RoadCraft */ \
					std::memset(static_cast<LPBYTE>(lpvData) + offsetof(DIMOUSESTATE, rgbButtons), 0, cbData - offsetof(DIMOUSESTATE, rgbButtons)); \
				break; \
			case DI8DEVTYPE_KEYBOARD: \
				if (reshade::input::is_blocking_any_keyboard_input()) \
					std::memset(lpvData, 0, cbData); \
				break; \
			} \
		} \
		return hr; \
	}

IDirectInputDevice8_GetDeviceState_Impl(9, A)
IDirectInputDevice8_GetDeviceState_Impl(9, W)

// This may be called a lot (e.g. in Dungeons & Dragons Online), so should have as low overhead as possible
#define IDirectInputDevice8_GetDeviceData_Impl(vtable_index, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice8##encoding##_GetDeviceData(IDirectInputDevice8##encoding *pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) \
	{ \
		static const auto trampoline = reshade::hooks::call(IDirectInputDevice8##encoding##_GetDeviceData, reshade::hooks::vtable_from_instance(pDevice) + vtable_index); \
		\
		HRESULT hr = trampoline(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags); \
		if (SUCCEEDED(hr) && \
			(dwFlags & DIGDD_PEEK) == 0 && \
			(rgdod != nullptr && *pdwInOut != 0)) \
		{ \
			DIDEVCAPS caps = { sizeof(caps) }; \
			pDevice->GetCapabilities(&caps); \
			\
			switch (LOBYTE(caps.dwDevType)) \
			{ \
			case DI8DEVTYPE_MOUSE: \
				if (reshade::input::is_blocking_any_mouse_input()) \
				{ \
					*pdwInOut = 0; \
					hr = DI_OK; /* Overwrite potential 'DI_BUFFEROVERFLOW' */ \
				} \
				break; \
			case DI8DEVTYPE_KEYBOARD: \
				if (reshade::input::is_blocking_any_keyboard_input()) \
				{ \
					*pdwInOut = 0; \
					hr = DI_OK; \
				} \
				break; \
			} \
		} \
		return hr; \
	}

IDirectInputDevice8_GetDeviceData_Impl(10, A)
IDirectInputDevice8_GetDeviceData_Impl(10, W)

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
		} \
		else \
		{ \
			reshade::log::message(reshade::log::level::warning, "IDirectInput8" #encoding "::CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str()); \
		} \
		return hr; \
	}

IDirectInput8_CreateDevice_Impl(3, A)
IDirectInput8_CreateDevice_Impl(3, W)

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN pUnkOuter)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting DirectInput8Create(hinst = %p, dwVersion = %x, riidltf = %s, ppvOut = %p, pUnkOuter = %p) ...",
		hinst, dwVersion, reshade::log::iid_to_string(riidltf).c_str(), ppvOut, pUnkOuter);

	const HRESULT hr = reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "DirectInput8Create failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	IUnknown *const factory = static_cast<IUnknown *>(*ppvOut);

	if (riidltf == IID_IDirectInput8W)
		reshade::hooks::install("IDirectInput8W::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput8W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput8W_CreateDevice));
	if (riidltf == IID_IDirectInput8A)
		reshade::hooks::install("IDirectInput8A::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput8A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput8A_CreateDevice));

	return hr;
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
