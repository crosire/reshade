/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

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
