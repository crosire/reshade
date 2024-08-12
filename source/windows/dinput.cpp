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

#define IDirectInputDevice_SetCooperativeLevel_Impl(vtable_index, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel(IDirectInputDevice##device_interface_version##encoding *pDevice, HWND hwnd, DWORD dwFlags) \
	{ \
		reshade::log::message(reshade::log::level::info, "Redirecting IDirectInputDevice::SetCooperativeLevel(this = %p, hwnd = %p, dwFlags = %#x) ...", pDevice, hwnd, dwFlags); \
		\
		if (dwFlags & DISCL_EXCLUSIVE) \
		{ \
			DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
			pDevice->GetDeviceInfo(&info); \
			if (LOBYTE(info.dwDevType) == DIDEVTYPE_MOUSE || \
				LOBYTE(info.dwDevType) == DIDEVTYPE_KEYBOARD) \
			{ \
				/* Need to remove exclusive flag, otherwise DirectInput will block input window messages and input.cpp will not receive input anymore */ \
				dwFlags = (dwFlags & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE; \
				\
				reshade::log::message(reshade::log::level::info, "> Replacing flags with %#x.", dwFlags); \
			} \
		} \
		\
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, hwnd, dwFlags); \
	}

IDirectInputDevice_SetCooperativeLevel_Impl(13,  , A)
IDirectInputDevice_SetCooperativeLevel_Impl(13,  , W)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 2, A)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 2, W)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 7, A)
IDirectInputDevice_SetCooperativeLevel_Impl(13, 7, W)

#define IDirectInputDevice_GetDeviceState_Impl(vtable_index, device_interface_version, encoding) \
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
		\
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_GetDeviceState, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, cbData, lpvData); \
	}

IDirectInputDevice_GetDeviceState_Impl(9,  , A)
IDirectInputDevice_GetDeviceState_Impl(9,  , W)
IDirectInputDevice_GetDeviceState_Impl(9, 2, A)
IDirectInputDevice_GetDeviceState_Impl(9, 2, W)
IDirectInputDevice_GetDeviceState_Impl(9, 7, A)
IDirectInputDevice_GetDeviceState_Impl(9, 7, W)

#define IDirectInputDevice_GetDeviceData_Impl(vtable_index, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInputDevice##device_interface_version##encoding##_GetDeviceData(IDirectInputDevice##device_interface_version##encoding *pDevice, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) \
	{ \
		if ((dwFlags & DIGDD_PEEK) == 0) \
		{ \
			DIDEVICEINSTANCE##encoding info = { sizeof(info) }; \
			pDevice->GetDeviceInfo(&info); \
			switch (LOBYTE(info.dwDevType)) \
			{ \
			case DIDEVTYPE_MOUSE: \
				if (is_blocking_mouse_input()) \
				{ \
					*pdwInOut = 0; \
					return DI_OK; \
				} \
				break; \
			case DIDEVTYPE_KEYBOARD: \
				if (is_blocking_keyboard_input()) \
				{ \
					*pdwInOut = 0; \
					return DI_OK; \
				} \
				break; \
			} \
		} \
		\
		return reshade::hooks::call(IDirectInputDevice##device_interface_version##encoding##_GetDeviceData, reshade::hooks::vtable_from_instance(pDevice) + vtable_index)(pDevice, cbObjectData, rgdod, pdwInOut, dwFlags); \
	}

IDirectInputDevice_GetDeviceData_Impl(10,  , A)
IDirectInputDevice_GetDeviceData_Impl(10,  , W)
IDirectInputDevice_GetDeviceData_Impl(10, 2, A)
IDirectInputDevice_GetDeviceData_Impl(10, 2, W)
IDirectInputDevice_GetDeviceData_Impl(10, 7, A)
IDirectInputDevice_GetDeviceData_Impl(10, 7, W)

#define IDirectInput_CreateDevice_Impl(vtable_index, factory_interface_version, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInput##factory_interface_version##encoding##_CreateDevice(IDirectInput##factory_interface_version##encoding *pDI, REFGUID rguid, LPDIRECTINPUTDEVICE##device_interface_version##encoding *lplpDirectInputDevice, LPUNKNOWN pUnkOuter) \
	{ \
		reshade::log::message( \
			reshade::log::level::info, \
			"Redirecting IDirectInput" #factory_interface_version #encoding "::CreateDevice(this = %p, rguid = %s, lplpDirectInputDevice = %p, pUnkOuter = %p) ...", \
			pDI, reshade::log::iid_to_string(rguid).c_str(), lplpDirectInputDevice, pUnkOuter); \
		\
		const HRESULT hr = reshade::hooks::call(IDirectInput##factory_interface_version##encoding##_CreateDevice, reshade::hooks::vtable_from_instance(pDI) + vtable_index)(pDI, rguid, lplpDirectInputDevice, pUnkOuter); \
		if (SUCCEEDED(hr)) \
		{ \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::GetDeviceState", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 9, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_GetDeviceState)); \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::GetDeviceData", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 10, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_GetDeviceData)); \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::SetCooperativeLevel", reshade::hooks::vtable_from_instance(*lplpDirectInputDevice), 13, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel)); \
		} \
		else \
		{ \
			reshade::log::message(reshade::log::level::warning, "IDirectInput" #factory_interface_version #encoding "::CreateDevice failed with error code %s.", reshade::log::hr_to_string(hr).c_str()); \
		} \
		return hr; \
	}
#define IDirectInput_CreateDeviceEx_Impl(vtable_index, factory_interface_version, device_interface_version, encoding) \
	HRESULT STDMETHODCALLTYPE IDirectInput##factory_interface_version##encoding##_CreateDeviceEx(IDirectInput##factory_interface_version##encoding *pDI, REFGUID rguid, REFGUID riid, LPVOID *ppvOut, LPUNKNOWN pUnkOuter) \
	{ \
		reshade::log::message( \
			reshade::log::level::info, \
			"Redirecting IDirectInput" #factory_interface_version #encoding "::CreateDeviceEx(this = %p, rguid = %s, riid = %s, ppvOut = %p, pUnkOuter = %p) ...", \
			pDI, reshade::log::iid_to_string(rguid).c_str(), reshade::log::iid_to_string(riid).c_str(), ppvOut, pUnkOuter); \
		\
		const HRESULT hr = reshade::hooks::call(IDirectInput##factory_interface_version##encoding##_CreateDeviceEx, reshade::hooks::vtable_from_instance(pDI) + vtable_index)(pDI, rguid, riid, ppvOut, pUnkOuter); \
		if (SUCCEEDED(hr)) \
		{ \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::GetDeviceState", reshade::hooks::vtable_from_instance(static_cast<LPDIRECTINPUTDEVICE##device_interface_version##encoding>(*ppvOut)), 9, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_GetDeviceState)); \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::GetDeviceData", reshade::hooks::vtable_from_instance(static_cast<LPDIRECTINPUTDEVICE##device_interface_version##encoding>(*ppvOut)), 10, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_GetDeviceData)); \
			reshade::hooks::install("IDirectInputDevice" #device_interface_version #encoding "::SetCooperativeLevel", reshade::hooks::vtable_from_instance(static_cast<LPDIRECTINPUTDEVICE##device_interface_version##encoding>(*ppvOut)), 13, reinterpret_cast<reshade::hook::address>(&IDirectInputDevice##device_interface_version##encoding##_SetCooperativeLevel)); \
		} \
		else \
		{ \
			reshade::log::message(reshade::log::level::warning, "IDirectInput" #factory_interface_version #encoding "::CreateDeviceEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str()); \
		} \
		return hr; \
	}

IDirectInput_CreateDevice_Impl(3,  ,  , A)
IDirectInput_CreateDevice_Impl(3,  ,  , W)
IDirectInput_CreateDevice_Impl(3, 2, 2, A)
IDirectInput_CreateDevice_Impl(3, 2, 2, W)
IDirectInput_CreateDevice_Impl(3, 7, 7, A)
IDirectInput_CreateDevice_Impl(3, 7, 7, W)
IDirectInput_CreateDeviceEx_Impl(9, 7, 7, A)
IDirectInput_CreateDeviceEx_Impl(9, 7, 7, W)

extern "C" HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA *ppDI, LPUNKNOWN punkOuter)
{
	reshade::log::message(reshade::log::level::info, "Redirecting DirectInputCreateA(hinst = %p, dwVersion = %x, ppDI = %p, punkOuter = %p) ...", hinst, dwVersion, ppDI, punkOuter);

	const HRESULT hr = reshade::hooks::call(DirectInputCreateA)(hinst, dwVersion, ppDI, punkOuter);
	if (SUCCEEDED(hr))
	{
		reshade::hooks::install("IDirectInputA::CreateDevice", reshade::hooks::vtable_from_instance(*ppDI), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputA_CreateDevice));
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "DirectInputCreateA failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}

extern "C" HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTW *ppDI, LPUNKNOWN punkOuter)
{
	reshade::log::message(reshade::log::level::info, "Redirecting DirectInputCreateW(hinst = %p, dwVersion = %x, ppDI = %p, punkOuter = %p) ...", hinst, dwVersion, ppDI, punkOuter);

	const HRESULT hr = reshade::hooks::call(DirectInputCreateW)(hinst, dwVersion, ppDI, punkOuter);
	if (SUCCEEDED(hr))
	{
		reshade::hooks::install("IDirectInputW::CreateDevice", reshade::hooks::vtable_from_instance(*ppDI), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputW_CreateDevice));
	}
	else
	{
		reshade::log::message(reshade::log::level::warning, "DirectInputCreateW failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}

extern "C" HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting DirectInputCreateEx(hinst = %p, dwVersion = %x, riidltf = %s, ppvOut = %p, punkOuter = %p) ...",
		hinst, dwVersion, reshade::log::iid_to_string(riidltf).c_str(), ppvOut, punkOuter);

	const HRESULT hr = reshade::hooks::call(DirectInputCreateEx)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	if (FAILED(hr))
	{
		reshade::log::message(reshade::log::level::warning, "DirectInputCreateEx failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
		return hr;
	}

	IUnknown *const factory = static_cast<IUnknown *>(*ppvOut);

	if (riidltf == IID_IDirectInputA)
		reshade::hooks::install("IDirectInputA::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInputA *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputA_CreateDevice));
	if (riidltf == IID_IDirectInputW)
		reshade::hooks::install("IDirectInputW::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInputW *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInputW_CreateDevice));
	if (riidltf == IID_IDirectInput2A)
		reshade::hooks::install("IDirectInput2A::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput2A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput2A_CreateDevice));
	if (riidltf == IID_IDirectInput2W)
		reshade::hooks::install("IDirectInput2W::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput2W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput2W_CreateDevice));

	if (riidltf == IID_IDirectInput7A)
	{
		reshade::hooks::install("IDirectInput7A::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput7A *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput7A_CreateDevice));
		reshade::hooks::install("IDirectInput7A::CreateDeviceEx", reshade::hooks::vtable_from_instance(static_cast<IDirectInput7A *>(factory)), 9, reinterpret_cast<reshade::hook::address>(&IDirectInput7A_CreateDeviceEx));
	}
	if (riidltf == IID_IDirectInput7W)
	{
		reshade::hooks::install("IDirectInput7W::CreateDevice", reshade::hooks::vtable_from_instance(static_cast<IDirectInput7W *>(factory)), 3, reinterpret_cast<reshade::hook::address>(&IDirectInput7W_CreateDevice));
		reshade::hooks::install("IDirectInput7W::CreateDeviceEx", reshade::hooks::vtable_from_instance(static_cast<IDirectInput7W *>(factory)), 9, reinterpret_cast<reshade::hook::address>(&IDirectInput7W_CreateDeviceEx));
	}

	return hr;
}
