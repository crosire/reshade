/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <ddraw.h>
#include "hook_manager.hpp"

extern "C" HRESULT WINAPI DirectDrawCreate(GUID *lpGUID, LPDIRECTDRAW *lplpDD, IUnknown *pUnkOuter)
{
	return reshade::hooks::call(DirectDrawCreate)(lpGUID, lplpDD, pUnkOuter);
}

extern "C" HRESULT WINAPI DirectDrawCreateEx(GUID *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown *pUnkOuter)
{
	return reshade::hooks::call(DirectDrawCreateEx)(lpGUID, lplpDD, iid, pUnkOuter);
}
