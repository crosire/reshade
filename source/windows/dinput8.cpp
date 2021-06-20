/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

// set direct input version to directx8 to remove the compiler warning
#define DIRECTINPUT_VERSION 0x0800

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <dinput.h>
//#include <combaseapi.h> // for dll load and unload hooks
//#include <OleCtl.h> // server register

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


// Removed this methods below for now as they might not directly related to only dinput8 (interface from combaseapi.h & OleCtl.h)
/*
HOOK_EXPORT HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
	static const auto trampoline = reshade::hooks::call(DllCanUnloadNow);
	return trampoline();
}

HOOK_EXPORT HRESULT STDAPICALLTYPE  DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
{
	static const auto trampoline = reshade::hooks::call(DllGetClassObject);
	return trampoline(rclsid,riid,ppv);
}


HOOK_EXPORT HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
	static const auto trampoline = reshade::hooks::call(DllRegisterServer);
	return trampoline();
}


HOOK_EXPORT HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
	static const auto trampoline = reshade::hooks::call(DllUnregisterServer);
	return trampoline();
}
*/


/*
 dinput8.dll exports
2 (0x0002), 1 (0x00000001), DllCanUnloadNow, 0x00015220, None
1 (0x0001), N/A, DirectInput8Create, 0x00002230, None
3 (0x0003), 2 (0x00000002), DllGetClassObject, 0x00015330, None
4 (0x0004), 3 (0x00000003), DllRegisterServer, 0x00024740, None
5 (0x0005), 4 (0x00000004), DllUnregisterServer, 0x000249c0, None
6 (0x0006), 5 (0x00000005), GetdfDIJoystick, 0x0000a1c0, None
*/
