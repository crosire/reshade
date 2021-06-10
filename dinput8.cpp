/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <dinput.h>


/**
 * Added hook when a direct input device is created to able to load the original library
 */
HOOK_EXPORT HRESULT WINAPI DirectInput8Create(HINSTANCE hinst,DWORD dwVersion,REFIID riidltf,LPVOID * ppvOut,LPUNKNOWN punkOuter)
{

	LOG(INFO) << "> Passing on to " << "DirectInput8Create" << ':';

	const HRESULT hr = reshade::hooks::call(DirectInput8Create)(hinst, dwVersion, riidltf,ppvOut,punkOuter);
	if (FAILED(hr))
	{
		LOG(WARN) << "DirectInput8Create" << " failed with error code " << hr << '.';
	}

	return hr;
}
