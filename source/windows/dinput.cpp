/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define INITGUID
// Set version to DirectInput 7
#define DIRECTINPUT_VERSION 0x0700

#include <dinput.h>
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "hook_manager.hpp"

extern bool is_blocking_mouse_input();
extern bool is_blocking_keyboard_input();

#define IDirectInputDevice_SetCooperativeLevel_Impl(vtable_offset, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel(IDirectInputDevice##device_interface_version##encoding *pDevice, HWND hwnd, DWORD dwFlags) \
	{ \
		LOG(INFO) << "Redirecting " << "IDirectInputDevice::SetCooperativeLevel" << '(' \
			<<   "this = " << pDevice \
			<< ", hwnd = " << hwnd \
			<< ", dwFlags = " << std::hex << dwFlags << std::dec \
			<< ')' << " ..."; \
		if (dwFlags & DISCL_EXCLUSIVE) \
		{ \
			/* Need to remove exclusive flag, otherwise DirectInput will block input window messages and input.cpp will not receive input anymore */ \
			dwFlags = (dwFlags & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE; \
			\
			LOG(INFO) << "> Replacing flags with " << std::hex << dwFlags << std::dec << " ..."; \
		} \
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel, vtable_from_instance(pDevice) + vtable_offset)(pDevice, hwnd, dwFlags); \
	}

IDirectInputDevice_SetCooperativeLevel_Impl(13,  , A)
IDirectInputDevice_SetCooperativeLevel_Impl(13,  , W)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 2, A)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 2, W)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 7, A)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 7, W)

#define IDirectInputDevice_GetDeviceState_Impl(vtable_offset, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice##device_interface_version##encoding##_GetDeviceState(IDirectInputDevice##device_interface_version##encoding *pDevice, DWORD cbData, LPVOID lpvData) \
	{ \
		DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
		pDevice->GetDeviceInfo(&info); \
		switch (LOBYTE(info.dwDevType)) \
		{ \
		case DIDEVTYPE_MOUSE: \
			if (is_blocking_mouse_input()) \
			{ \
				std::memset(lpvData, 0, cbData); \
				return DI_OK; \
			} \
			break; \
		case DIDEVTYPE_KEYBOARD: \
			if (is_blocking_keyboard_input()) \
			{ \
				std::memset(lpvData, 0, cbData); \
				return DI_OK; \
			} \
			break; \
		} \
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_GetDeviceState, vtable_from_instance(pDevice) + vtable_offset)(pDevice, cbData, lpvData); \
	}

IDirectInputDevice_GetDeviceState_Impl(9,  , A)
IDirectInputDevice_GetDeviceState_Impl(9,  , W)
IDirectInputDevice_GetDeviceState_Impl(9, 2, A)
IDirectInputDevice_GetDeviceState_Impl(9, 2, W)
IDirectInputDevice_GetDeviceState_Impl(9, 7, A)
IDirectInputDevice_GetDeviceState_Impl(9, 7, W)

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
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::SetCooperativeLevel", vtable_from_instance(*lplpDirectInputDevice), 13, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel)); \
		} \
		else \
		{ \
			LOG(WARN) << "IDirectInput" #factory_interface_version #encoding "::CreateDevice" << " failed with error code " << hr << '.'; \
		} \
		return hr; \
	}

IDirectInput_CreateDevice_Impl(3,  ,  , A)
IDirectInput_CreateDevice_Impl(3,  ,  , W)
IDirectInput_CreateDevice_Impl(3, 2, 2, A)
IDirectInput_CreateDevice_Impl(3, 2, 2, W)
IDirectInput_CreateDevice_Impl(3, 7, 7, A)
IDirectInput_CreateDevice_Impl(3, 7, 7, W)

HOOK_EXPORT HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateA" << '('
		<<   "hinst = " << hinst
		<< ", dwVersion = " << std::hex << dwVersion << std::dec
		<< ", ppDI = " << ppDI
		<< ", punkOuter = " << punkOuter
		<< ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateA)(hinst, dwVersion, ppDI, punkOuter);
	if (SUCCEEDED(hr))
	{
		reshade::hooks::install("IDirectInputA::CreateDevice", vtable_from_instance(*ppDI), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputA_CreateDevice));
	}
	else
	{
		LOG(WARN) << "DirectInputCreateA" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW *ppDI, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateW" << '('
		<<   "hinst = " << hinst
		<< ", dwVersion = " << std::hex << dwVersion << std::dec
		<< ", ppDI = " << ppDI
		<< ", punkOuter = " << punkOuter
		<< ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateW)(hinst, dwVersion, ppDI, punkOuter);
	if (SUCCEEDED(hr))
	{
		reshade::hooks::install("IDirectInputW::CreateDevice", vtable_from_instance(*ppDI), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputW_CreateDevice));
	}
	else
	{
		LOG(WARN) << "DirectInputCreateW" << " failed with error code " << hr << '.';
	}

	return hr;
}

HOOK_EXPORT HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	LOG(INFO) << "Redirecting " << "DirectInputCreateEx" << '('
		<<   "hinst = " << hinst
		<< ", dwVersion = " << std::hex << dwVersion << std::dec
		<< ", riidltf = " << riidltf
		<< ", ppvOut = " << ppvOut
		<< ", punkOuter = " << punkOuter
		<< ')' << " ...";

	const HRESULT hr = reshade::hooks::call(DirectInputCreateEx)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInputCreateEx" << " failed with error code " << hr << '.';
		return hr;
	}

	IUnknown *const factory = static_cast<IUnknown *>(*ppvOut);

	if (riidltf == IID_IDirectInputA)
		reshade::hooks::install("IDirectInputA::CreateDevice",  vtable_from_instance(static_cast<IDirectInputA  *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputA_CreateDevice));
	if (riidltf == IID_IDirectInputW)
		reshade::hooks::install("IDirectInputW::CreateDevice",  vtable_from_instance(static_cast<IDirectInputW  *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputW_CreateDevice));
	if (riidltf == IID_IDirectInput2A)
		reshade::hooks::install("IDirectInput2A::CreateDevice", vtable_from_instance(static_cast<IDirectInput2A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput2A_CreateDevice));
	if (riidltf == IID_IDirectInput2W)
		reshade::hooks::install("IDirectInput2W::CreateDevice", vtable_from_instance(static_cast<IDirectInput2W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput2W_CreateDevice));
	if (riidltf == IID_IDirectInput7A)
		reshade::hooks::install("IDirectInput7A::CreateDevice", vtable_from_instance(static_cast<IDirectInput2A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput7A_CreateDevice));
	if (riidltf == IID_IDirectInput7W)
		reshade::hooks::install("IDirectInput7W::CreateDevice", vtable_from_instance(static_cast<IDirectInput2W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput7W_CreateDevice));

	return hr;
}
