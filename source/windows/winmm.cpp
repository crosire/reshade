/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <mmsystem.h>

/**
 * Multimedia API hook
 */

// 1. TODO add hook methods
// 2. define them in exports.def

/**
* Note: it looks like that all methods of the dll are validated firt before a call is made.
* The dll seems to be prevented to be hooked by a dll as windows always uses the original one
* Still reshade start is triggered by it which is good.
*/



/*
 *------------------------------------------------------------
 * Timer hooks (timerapi.h)
 *------------------------------------------------------------
 */
HOOK_EXPORT __declspec(dllexport) MMRESULT WINAPI timeBeginPeriod(_In_ UINT uPeriod)
{

	LOG(INFO) << "Redirecting " << "timeBeginPeriod" << '(' << "uPeriod = " << uPeriod <<  ')' << " ...";

	static const auto trampoline = reshade::hooks::call(timeBeginPeriod);
	return trampoline(uPeriod);
}

HOOK_EXPORT __declspec(dllexport) MMRESULT WINAPI timeEndPeriod(_In_ UINT uPeriod)
{

	LOG(INFO) << "Redirecting " << "timeEndPeriod" << '(' << "uPeriod = " << uPeriod <<  ')' << " ...";

	static const auto trampoline = reshade::hooks::call(timeEndPeriod);
	return trampoline(uPeriod);
}


HOOK_EXPORT __declspec(dllexport) MMRESULT WINAPI timeGetSystemTime(_Out_writes_bytes_(cbmmt) LPMMTIME pmmt,_In_ UINT cbmmt)
{
	static const auto trampoline = reshade::hooks::call(timeGetSystemTime);
	return trampoline(pmmt,cbmmt);
}


HOOK_EXPORT __declspec(dllexport) DWORD WINAPI timeGetTime(void)
{
	static const auto trampoline = reshade::hooks::call(timeGetTime);
	return trampoline();

}

HOOK_EXPORT __declspec(dllexport) MMRESULT WINAPI timeGetDevCaps(_Out_writes_bytes_(cbtc) LPTIMECAPS ptc, _In_ UINT cbtc)
{
	static const auto trampoline = reshade::hooks::call(timeGetDevCaps);
	return trampoline(ptc,cbtc);
}

/*
 *------------------------------------------------------------
 * Sound support (playsoundapi.h & mmeapi.h)
 *------------------------------------------------------------
 */

HOOK_EXPORT __declspec(dllexport) MMRESULT WINAPI waveOutGetDevCapsW(_In_ UINT_PTR uDeviceID,_Out_ LPWAVEOUTCAPSW pwoc,_In_ UINT cbwoc)
{
	static const auto trampoline = reshade::hooks::call(waveOutGetDevCapsW);
	return trampoline(uDeviceID,pwoc,cbwoc);
}

