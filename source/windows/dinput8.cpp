/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Set version to DirectInput 8
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include "com_ptr.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

#undef IDirectInputDevice_SetCooperativeLevel

extern bool is_blocking_mouse_input();
extern bool is_blocking_keyboard_input();

extern HRESULT STDMETHODCALLTYPE IDirectInputDevice_SetCooperativeLevel(IUnknown *pDevice, HWND hwnd, DWORD dwFlags);

#define IDirectInputDevice_GetDeviceState_Impl(vtable_offset, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice##device_interface_version##encoding##_GetDeviceState(IDirectInputDevice##device_interface_version##encoding *pDevice, DWORD cbData, LPVOID lpvData) \
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
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_GetDeviceState, vtable_from_instance(pDevice) + vtable_offset)(pDevice, cbData, lpvData); \
	}

IDirectInputDevice_GetDeviceState_Impl(9, 8, A)
IDirectInputDevice_GetDeviceState_Impl(9, 8, W)

#define IDirectInput_CreateDevice_Impl(vtable_offset, factory_interface_version, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInput##factory_interface_version##encoding##_CreateDevice(IDirectInput##factory_interface_version##encoding *pDI, REFGUID rguid, LPDIRECTINPUTDEVICE##device_interface_version##encoding *lplpDirectInputDevice, LPUNKNOWN pUnkOuter) \
	{ \
		LOG(INFO) << "Redirecting " << "IDirectInput" #factory_interface_version #encoding "::CreateDevice" << '(' \
			<<   "this = " << pDI \
			<< ", rguid = " << rguid \
			<< ", lplpDirectInputDevice = " << lplpDirectInputDevice \
			<< ", pUnkOuter = " << pUnkOuter \
			<< ')' << " ..."; \
		const HRESULT hr = reshade::hooks::call(IDirectInput##factory_interface_version##encoding##_CreateDevice, vtable_from_instance(pDI) + vtable_offset)(pDI, rguid, lplpDirectInputDevice, pUnkOuter); \
		if (SUCCEEDED(hr)) \
		{ \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::GetDeviceState", vtable_from_instance(*lplpDirectInputDevice), 9, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_GetDeviceState)); \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::SetCooperativeLevel", vtable_from_instance(*lplpDirectInputDevice), 13, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice_SetCooperativeLevel)); \
		} \
		else \
		{ \
			LOG(WARN) << "IDirectInput" #factory_interface_version #encoding "::CreateDevice" << " failed with error code " << hr << '.'; \
		} \
		return hr; \
	}

IDirectInput_CreateDevice_Impl(3, 8, 8, A)
IDirectInput_CreateDevice_Impl(3, 8, 8, W)

HOOK_EXPORT HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN pUnkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInput8Create" << '('
		<<   "hinst = " << hinst
		<< ", dwVersion = " << std::hex << dwVersion << std::dec
		<< ", riidltf = " << riidltf
		<< ", ppvOut = " << ppvOut
		<< ", pUnkOuter = " << pUnkOuter
		<< ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, pUnkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInput8Create" << " failed with error code " << hr << '.';
		return hr;
	}

	IUnknown *const factory = static_cast<IUnknown *>(*ppvOut);

	if (riidltf == GUID { 0xBF798030, 0x483A, 0x4DA2, 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 })
		reshade::hooks::install("IDirectInput8A::CreateDevice", vtable_from_instance(static_cast<IDirectInput8A *>(factory)), 3, &IDirectInput8A_CreateDevice);
	if (riidltf == GUID { 0xBF798031, 0x483A, 0x4DA2, 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 })
		reshade::hooks::install("IDirectInput8W::CreateDevice", vtable_from_instance(static_cast<IDirectInput8W *>(factory)), 3, &IDirectInput8W_CreateDevice);

	return hr;
}

HOOK_EXPORT LPCDIDATAFORMAT WINAPI GetdfDIJoystick()
{
	static const auto trampoline = reshade::hooks::call(GetdfDIJoystick);
	return trampoline();
}
