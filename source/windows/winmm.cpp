/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "hook_manager.hpp"
#include <windows.h>
#include <Mmsystem.h>

/**
 * Multimedia API hook
 */

/*
 * Partly Generated with modified proxiFy
 */
HOOK_EXPORT LRESULT WINAPI HookCloseDriver(_In_ HDRVR hDriver,_In_ LPARAM lParam1,_In_ LPARAM lParam2)   
{
	//LOG(DEBUG) << "Redirecting 'CloseDriver' to original library...";
	static const auto trampoline = reshade::hooks::call(CloseDriver);
	return trampoline(hDriver,lParam1,lParam2);
}

HOOK_EXPORT LRESULT WINAPI HookDefDriverProc(_In_ DWORD_PTR dwDriverIdentifier,_In_ HDRVR hdrvr,_In_ UINT uMsg,_In_ LPARAM lParam1,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'DefDriverProc' to original library...";
	static const auto trampoline = reshade::hooks::call(DefDriverProc);
	return trampoline(dwDriverIdentifier,hdrvr,uMsg,lParam1,lParam2);
}

HOOK_EXPORT BOOL WINAPI HookDriverCallback(DWORD_PTR dwCallback,DWORD dwFlags,HDRVR hDevice,DWORD dwMsg,DWORD_PTR dwUser,DWORD_PTR dwParam1,DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'DriverCallback' to original library...";
	static const auto trampoline = reshade::hooks::call(DriverCallback);
	return trampoline(dwCallback,dwFlags,hDevice,dwMsg,dwUser,dwParam1,dwParam2);
}

HOOK_EXPORT HMODULE WINAPI HookDrvGetModuleHandle(_In_ HDRVR hDriver )
{
	//LOG(DEBUG) << "Redirecting 'DrvGetModuleHandle' to original library...";
	static const auto trampoline = reshade::hooks::call(DrvGetModuleHandle);
	return trampoline(hDriver);
}

HOOK_EXPORT HMODULE WINAPI HookGetDriverModuleHandle(_In_ HDRVR hDriver)
{
	//LOG(DEBUG) << "Redirecting 'GetDriverModuleHandle' to original library...";
	static const auto trampoline = reshade::hooks::call(GetDriverModuleHandle);
	return trampoline(hDriver);
}

HOOK_EXPORT HDRVR WINAPI HookOpenDriver(_In_ LPCWSTR szDriverName,_In_ LPCWSTR szSectionName,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'OpenDriver' to original library...";
	static const auto trampoline = reshade::hooks::call(OpenDriver);
	return trampoline(szDriverName,szSectionName,lParam2);
}

/* just a define to select PlaySoundA orPlaySoundAW
HOOK_EXPORT void WINAPI PlaySound()
{
	//LOG(DEBUG) << "Redirecting 'PlaySound' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySound);
	trampoline();
}
*/

HOOK_EXPORT BOOL WINAPI HookPlaySoundA(_In_opt_ LPCSTR pszSound,_In_opt_ HMODULE hmod,_In_ DWORD fdwSound)
{
	//LOG(DEBUG) << "Redirecting 'PlaySoundA' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySoundA);
	return trampoline(pszSound,hmod,fdwSound);
}

HOOK_EXPORT BOOL WINAPI HookPlaySoundW( _In_opt_ LPCWSTR pszSound,_In_opt_ HMODULE hmod,_In_ DWORD fdwSound)
{
	//LOG(DEBUG) << "Redirecting 'PlaySoundW' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySoundW);
	return trampoline(pszSound,hmod,fdwSound);
}

HOOK_EXPORT LRESULT WINAPI HookSendDriverMessage(  _In_ HDRVR hDriver,_In_ UINT message,_In_ LPARAM lParam1,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'SendDriverMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(SendDriverMessage);
	return trampoline(hDriver,message,lParam1,lParam2);
}

/* @Deprecated
HOOK_EXPORT void WINAPI HookWOWAppExit()
{
	//LOG(DEBUG) << "Redirecting 'WOWAppExit' to original library...";
	static const auto trampoline = reshade::hooks::call(WOWAppExit);
	return trampoline();
}
*/

HOOK_EXPORT MMRESULT WINAPI HookauxGetDevCapsA( _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbac) LPAUXCAPSA pac,_In_ UINT cbac)
{
	//LOG(DEBUG) << "Redirecting 'auxGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetDevCapsA);
	return trampoline(uDeviceID,pac,cbac);
}

HOOK_EXPORT MMRESULT WINAPI HookauxGetDevCapsW( _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbac) LPAUXCAPSW pac,_In_ UINT cbac)
{
	//LOG(DEBUG) << "Redirecting 'auxGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetDevCapsW);
	return trampoline(uDeviceID,pac,cbac);
}

HOOK_EXPORT UINT WINAPI HookauxGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'auxGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookauxGetVolume(_In_ UINT uDeviceID,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'auxGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetVolume);
	return trampoline(uDeviceID,pdwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookauxOutMessage( _In_ UINT uDeviceID,_In_ UINT uMsg,_In_opt_ DWORD_PTR dw1,_In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'auxOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(auxOutMessage);
	return trampoline(uDeviceID,uMsg,dw1,dw2);
}

HOOK_EXPORT MMRESULT WINAPI HookauxSetVolume(  _In_ UINT uDeviceID,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'auxSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(auxSetVolume);
	return trampoline(uDeviceID,dwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyConfigChanged(  _In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'joyConfigChanged' to original library...";
	static const auto trampoline = reshade::hooks::call(joyConfigChanged);
	return trampoline(dwFlags);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyGetDevCapsA(  _In_ UINT_PTR uJoyID, _Out_writes_bytes_(cbjc) LPJOYCAPSA pjc,_In_ UINT cbjc)
{
	//LOG(DEBUG) << "Redirecting 'joyGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetDevCapsA);
	return trampoline(uJoyID,pjc,cbjc);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyGetDevCapsW(  _In_ UINT_PTR uJoyID, _Out_writes_bytes_(cbjc) LPJOYCAPSW pjc, _In_ UINT cbjc)
{
	//LOG(DEBUG) << "Redirecting 'joyGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetDevCapsW);
	return trampoline(uJoyID,pjc,cbjc);
}

HOOK_EXPORT UINT WINAPI HookjoyGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'joyGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookjoyGetPos(  _In_ UINT uJoyID, _Out_ LPJOYINFO pji )
{
	//LOG(DEBUG) << "Redirecting 'joyGetPos' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetPos);
	return trampoline(uJoyID,pji);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyGetPosEx( _In_ UINT uJoyID,_Out_ LPJOYINFOEX pji)
{
	//LOG(DEBUG) << "Redirecting 'joyGetPosEx' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetPosEx);
	return trampoline(uJoyID,pji);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyGetThreshold(_In_ UINT uJoyID,_Out_ LPUINT puThreshold)
{
	//LOG(DEBUG) << "Redirecting 'joyGetThreshold' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetThreshold);
	return trampoline(uJoyID,puThreshold);
}

HOOK_EXPORT MMRESULT WINAPI HookjoyReleaseCapture(_In_ UINT uJoyID)
{
	//LOG(DEBUG) << "Redirecting 'joyReleaseCapture' to original library...";
	static const auto trampoline = reshade::hooks::call(joyReleaseCapture);
	return trampoline(uJoyID);
}

HOOK_EXPORT MMRESULT WINAPI HookjoySetCapture( _In_ HWND hwnd,_In_ UINT uJoyID,_In_ UINT uPeriod,_In_ BOOL fChanged)
{
	//LOG(DEBUG) << "Redirecting 'joySetCapture' to original library...";
	static const auto trampoline = reshade::hooks::call(joySetCapture);
	return trampoline(hwnd,uJoyID,uPeriod,fChanged);
}

HOOK_EXPORT MMRESULT WINAPI HookjoySetThreshold(_In_ UINT uJoyID,_In_ UINT uThreshold)
{
	//LOG(DEBUG) << "Redirecting 'joySetThreshold' to original library...";
	static const auto trampoline = reshade::hooks::call(joySetThreshold);
	return trampoline(uJoyID,uThreshold);
}

HOOK_EXPORT BOOL WINAPI HookmciDriverNotify( HANDLE hwndCallback, MCIDEVICEID wDeviceID,UINT uStatus)
{
	//LOG(DEBUG) << "Redirecting 'mciDriverNotify' to original library...";
	static const auto trampoline = reshade::hooks::call(mciDriverNotify);
	return trampoline(hwndCallback,wDeviceID,uStatus);
}

HOOK_EXPORT UINT WINAPI HookmciDriverYield( MCIDEVICEID wDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'mciDriverYield' to original library...";
	static const auto trampoline = reshade::hooks::call(mciDriverYield);
	return trampoline(wDeviceID);
}

/* @Deprecated
HOOK_EXPORT void WINAPI HookmciExecute()
{
	//LOG(DEBUG) << "Redirecting 'mciExecute' to original library...";
	static const auto trampoline = reshade::hooks::call(mciExecute);
	return trampoline();
}
*/

HOOK_EXPORT BOOL WINAPI HookmciFreeCommandResource( UINT wTable)
{
	//LOG(DEBUG) << "Redirecting 'mciFreeCommandResource' to original library...";
	static const auto trampoline = reshade::hooks::call(mciFreeCommandResource);
	return trampoline(wTable);
}

HOOK_EXPORT HTASK WINAPI HookmciGetCreatorTask( _In_ MCIDEVICEID mciId)
{
	//LOG(DEBUG) << "Redirecting 'mciGetCreatorTask' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetCreatorTask);
	return trampoline(mciId);
}

HOOK_EXPORT MCIDEVICEID WINAPI HookmciGetDeviceIDA( _In_ LPCSTR pszDevice)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDA);
	return trampoline(pszDevice);
}

HOOK_EXPORT MCIDEVICEID WINAPI HookmciGetDeviceIDFromElementIDA(  _In_ DWORD dwElementID, _In_ LPCSTR lpstrType)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDFromElementIDA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDFromElementIDA);
	return trampoline(dwElementID, lpstrType);
}

HOOK_EXPORT MCIDEVICEID WINAPI HookmciGetDeviceIDFromElementIDW(_In_ DWORD dwElementID,_In_ LPCWSTR lpstrType)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDFromElementIDW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDFromElementIDW);
	return trampoline(dwElementID,lpstrType);
}

HOOK_EXPORT MCIDEVICEID WINAPI HookmciGetDeviceIDW(_In_ LPCWSTR pszDevice)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDW);
	return trampoline(pszDevice);
}

HOOK_EXPORT DWORD_PTR WINAPI HookmciGetDriverData(MCIDEVICEID wDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDriverData' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDriverData);
	return trampoline(wDeviceID);
}

HOOK_EXPORT BOOL WINAPI HookmciGetErrorStringA( _In_ MCIERROR mcierr,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'mciGetErrorStringA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetErrorStringA);
	return trampoline(mcierr,pszText,cchText);
}

HOOK_EXPORT BOOL WINAPI HookmciGetErrorStringW(_In_ MCIERROR mcierr,_Out_writes_(cchText) LPWSTR pszText, _In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'mciGetErrorStringW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetErrorStringW);
	return trampoline(mcierr,pszText,cchText);
}

HOOK_EXPORT YIELDPROC WINAPI HookmciGetYieldProc( _In_ MCIDEVICEID mciId,_In_ LPDWORD pdwYieldData)
{
	//LOG(DEBUG) << "Redirecting 'mciGetYieldProc' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetYieldProc);
	return trampoline(mciId,pdwYieldData);
}

HOOK_EXPORT UINT WINAPI HookmciLoadCommandResource(  HANDLE hInstance, LPCWSTR lpResName, UINT wType)
{
	//LOG(DEBUG) << "Redirecting 'mciLoadCommandResource' to original library...";
	static const auto trampoline = reshade::hooks::call(mciLoadCommandResource);
	return trampoline(hInstance,lpResName,wType);
}

HOOK_EXPORT MCIERROR WINAPI HookmciSendCommandA( _In_ MCIDEVICEID mciId,_In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1,_In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mciSendCommandA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendCommandA);
	return trampoline(mciId,uMsg,dwParam1,dwParam2);
}

HOOK_EXPORT MCIERROR WINAPI HookmciSendCommandW(  _In_ MCIDEVICEID mciId, _In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1,_In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mciSendCommandW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendCommandW);
	return trampoline(mciId,uMsg,dwParam1,dwParam2);
}

HOOK_EXPORT MCIERROR WINAPI HookmciSendStringA( _In_ LPCSTR lpstrCommand, _Out_writes_opt_(uReturnLength) LPSTR lpstrReturnString,_In_ UINT uReturnLength,_In_opt_ HWND hwndCallback)
{
	//LOG(DEBUG) << "Redirecting 'mciSendStringA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendStringA);
	return trampoline(lpstrCommand,lpstrReturnString,uReturnLength,hwndCallback);
}

HOOK_EXPORT MCIERROR WINAPI HookmciSendStringW(_In_ LPCWSTR lpstrCommand,_Out_writes_opt_(uReturnLength) LPWSTR lpstrReturnString,_In_ UINT uReturnLength,_In_opt_ HWND hwndCallback)
{
	//LOG(DEBUG) << "Redirecting 'mciSendStringW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendStringW);
	return trampoline(lpstrCommand,lpstrReturnString,uReturnLength,hwndCallback);
}

HOOK_EXPORT BOOL WINAPI HookmciSetDriverData( MCIDEVICEID wDeviceID, DWORD_PTR dwData)
{
	//LOG(DEBUG) << "Redirecting 'mciSetDriverData' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSetDriverData);
	return trampoline(wDeviceID,dwData);
}

HOOK_EXPORT BOOL WINAPI HookmciSetYieldProc(  _In_ MCIDEVICEID mciId, _In_opt_ YIELDPROC fpYieldProc,_In_ DWORD dwYieldData)
{
	//LOG(DEBUG) << "Redirecting 'mciSetYieldProc' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSetYieldProc);
	return trampoline(mciId,fpYieldProc,dwYieldData);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiConnect( _In_ HMIDI hmi, _In_ HMIDIOUT hmo, _In_opt_ LPVOID pReserved)
{
	//LOG(DEBUG) << "Redirecting 'midiConnect' to original library...";
	static const auto trampoline = reshade::hooks::call(midiConnect);
	return trampoline(hmi,hmo,pReserved);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiDisconnect( _In_ HMIDI hmi,_In_ HMIDIOUT hmo, _In_opt_ LPVOID pReserved)
{
	//LOG(DEBUG) << "Redirecting 'midiDisconnect' to original library...";
	static const auto trampoline = reshade::hooks::call(midiDisconnect);
	return trampoline(hmi,hmo,pReserved);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInAddBuffer(  _In_ HMIDIIN hmi, _Out_writes_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInAddBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInAddBuffer);
	return trampoline(hmi,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInClose(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInClose);
	return trampoline(hmi);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInGetDevCapsA(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbmic) LPMIDIINCAPSA pmic,_In_ UINT cbmic)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetDevCapsA);
	return trampoline(uDeviceID,pmic,cbmic);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInGetDevCapsW(_In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbmic) LPMIDIINCAPSW pmic,_In_ UINT cbmic)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetDevCapsW);
	return trampoline(uDeviceID,pmic,cbmic);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInGetErrorTextA(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInGetErrorTextW( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInGetID(_In_ HMIDIIN hmi, _Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetID);
	return trampoline(hmi,puDeviceID);
}

HOOK_EXPORT UINT WINAPI HookmidiInGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'midiInGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInMessage(_In_opt_ HMIDIIN hmi, _In_ UINT uMsg, _In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'midiInMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInMessage);
	return trampoline(hmi,uMsg,dw1,dw2);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInOpen(_Out_ LPHMIDIIN phmi,_In_ UINT uDeviceID,_In_opt_ DWORD_PTR dwCallback,_In_opt_ DWORD_PTR dwInstance,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiInOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInOpen);
	return trampoline(phmi,uDeviceID,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInPrepareHeader(_In_ HMIDIIN hmi,_Inout_updates_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInPrepareHeader);
	return trampoline(hmi,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInReset(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInReset' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInReset);
	return trampoline(hmi);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInStart(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInStart' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInStart);
	return trampoline(hmi);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInStop(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInStop' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInStop);
	return trampoline(hmi);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiInUnprepareHeader(_In_ HMIDIIN hmi, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInUnprepareHeader);
	return trampoline(hmi,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutCacheDrumPatches(  _In_ HMIDIOUT hmo,_In_ UINT uPatch,_In_reads_(MIDIPATCHSIZE) LPWORD pwkya, _In_ UINT fuCache)
{
	//LOG(DEBUG) << "Redirecting 'midiOutCacheDrumPatches' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutCacheDrumPatches);
	return trampoline(hmo,uPatch,pwkya,fuCache);
}


HOOK_EXPORT MMRESULT WINAPI HookmidiOutCachePatches(_In_ HMIDIOUT hmo, _In_ UINT uBank,_In_reads_(MIDIPATCHSIZE) LPWORD pwpa, _In_ UINT fuCache)
{
	//LOG(DEBUG) << "Redirecting 'midiOutCachePatches' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutCachePatches);
	return trampoline(hmo,uBank,pwpa,fuCache);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutClose(_In_ HMIDIOUT hmo)
{
	//LOG(DEBUG) << "Redirecting 'midiOutClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutClose);
	return trampoline(hmo);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetDevCapsA(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbmoc) LPMIDIOUTCAPSA pmoc, _In_ UINT cbmoc)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetDevCapsA);
	return trampoline(uDeviceID, pmoc,cbmoc);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetDevCapsW(  _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbmoc) LPMIDIOUTCAPSW pmoc,_In_ UINT cbmoc)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetDevCapsW);
	return trampoline(uDeviceID, pmoc,cbmoc);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetErrorTextA( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetErrorTextW( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetID( _In_ HMIDIOUT hmo,_Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetID);
	return trampoline(hmo,puDeviceID);
}

HOOK_EXPORT UINT WINAPI HookmidiOutGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutGetVolume(  _In_opt_ HMIDIOUT hmo,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetVolume);
	return trampoline(hmo,pdwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutLongMsg(  _In_ HMIDIOUT hmo,  _In_reads_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutLongMsg' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutLongMsg);
	return trampoline(hmo,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutMessage( _In_opt_ HMIDIOUT hmo,_In_ UINT uMsg,_In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'midiOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutMessage);
	return trampoline(hmo,uMsg, dw1,dw2);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutOpen(   _Out_ LPHMIDIOUT phmo, _In_ UINT uDeviceID, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiOutOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutOpen);
	return trampoline(phmo, uDeviceID,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutPrepareHeader( _In_ HMIDIOUT hmo, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh,_In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutPrepareHeader);
	return trampoline(hmo,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutReset(_In_ HMIDIOUT hmo)
{
	//LOG(DEBUG) << "Redirecting 'midiOutReset' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutReset);
	return trampoline(hmo);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutSetVolume( _In_opt_ HMIDIOUT hmo,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'midiOutSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutSetVolume);
	return trampoline(hmo,dwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutShortMsg( _In_ HMIDIOUT hmo, _In_ DWORD dwMsg)
{
	//LOG(DEBUG) << "Redirecting 'midiOutShortMsg' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutShortMsg);
	return trampoline(hmo,dwMsg);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiOutUnprepareHeader(    _In_ HMIDIOUT hmo, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh,_In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutUnprepareHeader);
	return trampoline(hmo,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamClose( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamClose);
	return trampoline(hms);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamOpen( _Out_ LPHMIDISTRM phms, _Inout_updates_(cMidi) LPUINT puDeviceID, _In_ DWORD cMidi, _In_opt_ DWORD_PTR dwCallback,_In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamOpen);
	return trampoline(phms,puDeviceID,cMidi,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamOut(   _In_ HMIDISTRM hms, _Out_writes_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamOut' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamOut);
	return trampoline(hms,pmh,cbmh);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamPause(_In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamPause' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamPause);
	return trampoline(hms);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamPosition( _In_ HMIDISTRM hms,_Out_writes_bytes_(cbmmt) LPMMTIME lpmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamPosition);
	return trampoline(hms,lpmmt,cbmmt);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamProperty( _In_ HMIDISTRM hms,  _Inout_updates_bytes_(sizeof(DWORD) + sizeof(DWORD)) LPBYTE lppropdata,_In_ DWORD dwProperty)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamProperty' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamProperty);
	return trampoline(hms,lppropdata,dwProperty);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamRestart( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamRestart' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamRestart);
	return trampoline(hms);
}

HOOK_EXPORT MMRESULT WINAPI HookmidiStreamStop( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamStop' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamStop);
	return trampoline(hms);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerClose(_In_ HMIXER hmx)
{
	//LOG(DEBUG) << "Redirecting 'mixerClose' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerClose);
	return trampoline(hmx);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetControlDetailsA( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERCONTROLDETAILS pmxcd,_In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetControlDetailsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetControlDetailsA);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetControlDetailsW( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERCONTROLDETAILS pmxcd, _In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetControlDetailsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetControlDetailsW);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetDevCapsA( _In_ UINT_PTR uMxId,_Out_writes_bytes_(cbmxcaps) LPMIXERCAPSA pmxcaps,_In_ UINT cbmxcaps)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetDevCapsA);
	return trampoline(uMxId,pmxcaps,cbmxcaps);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetDevCapsW(   _In_ UINT_PTR uMxId, _Out_writes_bytes_(cbmxcaps) LPMIXERCAPSW pmxcaps,_In_ UINT cbmxcaps)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetDevCapsW);
		return trampoline(uMxId,pmxcaps,cbmxcaps);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetID( _In_opt_ HMIXEROBJ hmxobj, _Out_ UINT  FAR * puMxId, _In_ DWORD fdwId)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetID);
	return trampoline(hmxobj,puMxId,fdwId);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetLineControlsA( _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINECONTROLSA pmxlc,_In_ DWORD fdwControls)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineControlsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineControlsA);
	return trampoline(hmxobj,pmxlc,fdwControls);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetLineControlsW(  _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINECONTROLSW pmxlc, _In_ DWORD fdwControls)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineControlsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineControlsW);
	return trampoline(hmxobj,pmxlc,fdwControls);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetLineInfoA( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERLINEA pmxl, _In_ DWORD fdwInfo)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineInfoA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineInfoA);
	return trampoline(hmxobj,pmxl,fdwInfo);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerGetLineInfoW(  _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINEW pmxl, _In_ DWORD fdwInfo)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineInfoW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineInfoW);
	return trampoline(hmxobj,pmxl,fdwInfo);
}

HOOK_EXPORT UINT WINAPI HookmixerGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'mixerGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetNumDevs);
	return trampoline();
}

HOOK_EXPORT DWORD WINAPI HookmixerMessage(_In_opt_ HMIXER hmx, _In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1, _In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mixerMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerMessage);
	return trampoline(hmx,uMsg,dwParam1,dwParam2);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerOpen(    _Out_opt_ LPHMIXER phmx,  _In_ UINT uMxId, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mixerOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerOpen);
	return trampoline(phmx,uMxId,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookmixerSetControlDetails( _In_opt_ HMIXEROBJ hmxobj, _In_ LPMIXERCONTROLDETAILS pmxcd, _In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerSetControlDetails' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerSetControlDetails);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

HOOK_EXPORT UINT WINAPI HookmmDrvInstall(  HDRVR hDriver, LPCWSTR wszDrvEntry, DRIVERMSGPROC drvMessage, UINT wFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmDrvInstall' to original library...";
	static const auto trampoline = reshade::hooks::call(mmDrvInstall);
	return trampoline(hDriver,wszDrvEntry,drvMessage,wFlags);
}

/* @Deprecated
HOOK_EXPORT void WINAPI HookmmGetCurrentTask()
{
	//LOG(DEBUG) << "Redirecting 'mmGetCurrentTask' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmGetCurrentTask);
	//return trampoline();
}

HOOK_EXPORT void WINAPI HookmmTaskBlock()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskBlock' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskBlock);
	//return trampoline();
}

HOOK_EXPORT void WINAPI HookmmTaskCreate()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskCreate' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskCreate);
	//return trampoline();
}

HOOK_EXPORT void WINAPI HookmmTaskSignal()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskSignal' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskSignal);
	//return trampoline();
}

HOOK_EXPORT void WINAPI HookmmTaskYield()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskYield' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskYield);
	//return trampoline();
}
*/

HOOK_EXPORT MMRESULT WINAPI HookmmioAdvance(_In_ HMMIO hmmio,_In_opt_ LPMMIOINFO pmmioinfo,_In_ UINT fuAdvance)
{
	//LOG(DEBUG) << "Redirecting 'mmioAdvance' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioAdvance);
	return trampoline(hmmio,pmmioinfo,fuAdvance);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioAscend( _In_ HMMIO hmmio, _In_ LPMMCKINFO pmmcki,_In_ UINT fuAscend)
{
	//LOG(DEBUG) << "Redirecting 'mmioAscend' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioAscend);
	return trampoline(hmmio,pmmcki,fuAscend);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioClose( _In_ HMMIO hmmio,_In_ UINT fuClose)
{
	//LOG(DEBUG) << "Redirecting 'mmioClose' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioClose);
	return trampoline(hmmio,fuClose);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioCreateChunk(_In_ HMMIO hmmio,_In_ LPMMCKINFO pmmcki,_In_ UINT fuCreate)
{
	//LOG(DEBUG) << "Redirecting 'mmioCreateChunk' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioCreateChunk);
	return trampoline(hmmio,pmmcki,fuCreate);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioDescend(  _In_ HMMIO hmmio, _Inout_ LPMMCKINFO pmmcki, _In_opt_ const MMCKINFO  FAR * pmmckiParent, _In_ UINT fuDescend)
{
	//LOG(DEBUG) << "Redirecting 'mmioDescend' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioDescend);
	return trampoline(hmmio,pmmcki,pmmckiParent,fuDescend);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioFlush(  _In_ HMMIO hmmio,_In_ UINT fuFlush)
{
	//LOG(DEBUG) << "Redirecting 'mmioFlush' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioFlush);
	return trampoline(hmmio,fuFlush);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioGetInfo( _In_ HMMIO hmmio, _Out_ LPMMIOINFO pmmioinfo, _In_ UINT fuInfo)
{
	//LOG(DEBUG) << "Redirecting 'mmioGetInfo' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioGetInfo);
	return trampoline(hmmio,pmmioinfo,fuInfo);
}

HOOK_EXPORT LPMMIOPROC WINAPI HookmmioInstallIOProcA( _In_ FOURCC fccIOProc,_In_opt_ LPMMIOPROC pIOProc,_In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioInstallIOProcA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioInstallIOProcA);
	return trampoline(fccIOProc,pIOProc,dwFlags);
}

HOOK_EXPORT LPMMIOPROC WINAPI HookmmioInstallIOProcW(   _In_ FOURCC fccIOProc, _In_opt_ LPMMIOPROC pIOProc, _In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioInstallIOProcW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioInstallIOProcW);
	return trampoline(fccIOProc,pIOProc,dwFlags);
}

HOOK_EXPORT HMMIO WINAPI HookmmioOpenA( _Inout_updates_bytes_opt_(128) LPSTR pszFileName,_Inout_opt_ LPMMIOINFO pmmioinfo,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mmioOpenA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioOpenA);
	return trampoline(pszFileName,pmmioinfo,fdwOpen);
}

HOOK_EXPORT HMMIO WINAPI HookmmioOpenW( _Inout_updates_bytes_opt_(128) LPWSTR pszFileName,_Inout_opt_ LPMMIOINFO pmmioinfo,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mmioOpenW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioOpenW);
	return trampoline(pszFileName,pmmioinfo,fdwOpen);
}

HOOK_EXPORT LONG WINAPI HookmmioRead(  _In_ HMMIO hmmio, _Out_writes_bytes_(cch) HPSTR pch,_In_ LONG cch)
{
	//LOG(DEBUG) << "Redirecting 'mmioRead' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRead);
	return trampoline(hmmio,pch,cch);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioRenameA(_In_ LPCSTR pszFileName, _In_ LPCSTR pszNewFileName, _In_opt_ LPCMMIOINFO pmmioinfo, _In_ DWORD fdwRename)
{
	//LOG(DEBUG) << "Redirecting 'mmioRenameA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRenameA);
	return trampoline(pszFileName,pszNewFileName,pmmioinfo,fdwRename);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioRenameW( _In_ LPCWSTR pszFileName, _In_ LPCWSTR pszNewFileName, _In_opt_ LPCMMIOINFO pmmioinfo, _In_ DWORD fdwRename)
{
	//LOG(DEBUG) << "Redirecting 'mmioRenameW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRenameW);
	return trampoline(pszFileName,pszNewFileName,pmmioinfo,fdwRename);
}

HOOK_EXPORT LONG WINAPI HookmmioSeek(_In_ HMMIO hmmio, _In_ LONG lOffset,_In_ int iOrigin)
{
	//LOG(DEBUG) << "Redirecting 'mmioSeek' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSeek);
	return trampoline(hmmio,lOffset,iOrigin);
}

HOOK_EXPORT LRESULT WINAPI HookmmioSendMessage(  _In_ HMMIO hmmio,_In_ UINT uMsg, _In_opt_ LPARAM lParam1,_In_opt_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'mmioSendMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSendMessage);
	return trampoline(hmmio,uMsg,lParam1,lParam2);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioSetBuffer(  _In_ HMMIO hmmio, _Out_writes_opt_(cchBuffer) LPSTR pchBuffer, _In_ LONG cchBuffer, _In_ UINT fuBuffer)
{
	//LOG(DEBUG) << "Redirecting 'mmioSetBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSetBuffer);
	return trampoline(hmmio,pchBuffer,cchBuffer,fuBuffer);
}

HOOK_EXPORT MMRESULT WINAPI HookmmioSetInfo(_In_ HMMIO hmmio, _In_ LPCMMIOINFO pmmioinfo, _In_ UINT fuInfo)
{
	//LOG(DEBUG) << "Redirecting 'mmioSetInfo' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSetInfo);
	return trampoline(hmmio,pmmioinfo,fuInfo);
}

HOOK_EXPORT FOURCC WINAPI HookmmioStringToFOURCCA(  LPCSTR sz, _In_ UINT uFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioStringToFOURCCA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioStringToFOURCCA);
	return trampoline(sz,uFlags);
}

HOOK_EXPORT FOURCC WINAPI HookmmioStringToFOURCCW(LPCWSTR sz,_In_ UINT uFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioStringToFOURCCW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioStringToFOURCCW);
	return trampoline(sz,uFlags);
}

HOOK_EXPORT LONG WINAPI HookmmioWrite( _In_ HMMIO hmmio,_In_reads_bytes_(cch) const char  _huge * pch, _In_ LONG cch)
{
	//LOG(DEBUG) << "Redirecting 'mmioWrite' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioWrite);
	return trampoline(hmmio,pch,cch);
}

/* @Deprecated
HOOK_EXPORT UINT WINAPI HookmmsystemGetVersion()
{
	//LOG(DEBUG) << "Redirecting 'mmsystemGetVersion' to original library...";
	static const auto trampoline = reshade::hooks::call(mmsystemGetVersion);
	return trampoline();
}
*/

HOOK_EXPORT BOOL WINAPI HooksndPlaySoundA(_In_opt_ LPCSTR pszSound,_In_ UINT fuSound)
{
	//LOG(DEBUG) << "Redirecting 'sndPlaySoundA' to original library...";
	static const auto trampoline = reshade::hooks::call(sndPlaySoundA);
	return trampoline(pszSound,fuSound);
}

HOOK_EXPORT BOOL WINAPI HooksndPlaySoundW(  _In_opt_ LPCWSTR pszSound,_In_ UINT fuSound)
{
	//LOG(DEBUG) << "Redirecting 'sndPlaySoundW' to original library...";
	static const auto trampoline = reshade::hooks::call(sndPlaySoundW);
	return trampoline(pszSound,fuSound);
}

HOOK_EXPORT MMRESULT WINAPI HooktimeBeginPeriod(  _In_ UINT uPeriod)
{
	//LOG(DEBUG) << "Redirecting 'timeBeginPeriod' to original library...";
	static const auto trampoline = reshade::hooks::call(timeBeginPeriod);
	return trampoline(uPeriod);
}

HOOK_EXPORT MMRESULT WINAPI HooktimeEndPeriod( _In_ UINT uPeriod)
{
	//LOG(DEBUG) << "Redirecting 'timeEndPeriod' to original library...";
	static const auto trampoline = reshade::hooks::call(timeEndPeriod);
	return trampoline(uPeriod);
}

HOOK_EXPORT MMRESULT WINAPI HooktimeGetDevCaps(_Out_writes_bytes_(cbtc) LPTIMECAPS ptc,_In_ UINT cbtc)
{
	//LOG(DEBUG) << "Redirecting 'timeGetDevCaps' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetDevCaps);
	return trampoline(ptc,cbtc);
}

HOOK_EXPORT MMRESULT WINAPI HooktimeGetSystemTime(  _Out_writes_bytes_(cbmmt) LPMMTIME pmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'timeGetSystemTime' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetSystemTime);
	return trampoline(pmmt,cbmmt);
}

HOOK_EXPORT DWORD WINAPI HooktimeGetTime()
{
	//LOG(DEBUG) << "Redirecting 'timeGetTime' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetTime);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HooktimeKillEvent( _In_ UINT uTimerID)
{
	//LOG(DEBUG) << "Redirecting 'timeKillEvent' to original library...";
	static const auto trampoline = reshade::hooks::call(timeKillEvent);
	return trampoline(uTimerID);
}

HOOK_EXPORT MMRESULT WINAPI HooktimeSetEvent( _In_ UINT uDelay, _In_ UINT uResolution,_In_ LPTIMECALLBACK fptc, _In_ DWORD_PTR dwUser, _In_ UINT fuEvent)
{
	//LOG(DEBUG) << "Redirecting 'timeSetEvent' to original library...";
	static const auto trampoline = reshade::hooks::call(timeSetEvent);
	return trampoline(uDelay,uResolution,fptc,dwUser,fuEvent);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInAddBuffer( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInAddBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInAddBuffer);
	return trampoline(hwi,pwh,cbwh);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInClose(_In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInClose' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInClose);
	return trampoline(hwi);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetDevCapsA( _In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbwic) LPWAVEINCAPSA pwic,_In_ UINT cbwic)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetDevCapsA);
	return trampoline(uDeviceID,pwic,cbwic);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetDevCapsW(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbwic) LPWAVEINCAPSW pwic,_In_ UINT cbwic)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetDevCapsW);
	return trampoline(uDeviceID,pwic,cbwic);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetErrorTextA(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetErrorTextW(_In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetID(_In_ HWAVEIN hwi,_In_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetID);
	return trampoline(hwi,puDeviceID);
}

HOOK_EXPORT UINT WINAPI HookwaveInGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'waveInGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInGetPosition(_In_ HWAVEIN hwi,_Inout_updates_bytes_(cbmmt) LPMMTIME pmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetPosition);
	return trampoline(hwi,pmmt,cbmmt);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInMessage( _In_opt_ HWAVEIN hwi, _In_ UINT uMsg, _In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'waveInMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInMessage);
	return trampoline(hwi,uMsg,dw1,dw2);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInOpen(  _Out_opt_ LPHWAVEIN phwi, _In_ UINT uDeviceID, _In_ LPCWAVEFORMATEX pwfx, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'waveInOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInOpen);
	return trampoline(phwi,uDeviceID,pwfx,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInPrepareHeader( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInPrepareHeader);
	return trampoline(hwi,pwh,cbwh);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInReset( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInReset' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInReset);
	return trampoline(hwi);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInStart( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInStart' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInStart);
	return trampoline(hwi);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInStop( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInStop' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInStop);
	return trampoline(hwi);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveInUnprepareHeader( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInUnprepareHeader);
	return trampoline(hwi,pwh,cbwh);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutBreakLoop( _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutBreakLoop' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutBreakLoop);
	return trampoline(hwo);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutClose(_In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutClose' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutClose);
	return trampoline(hwo);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetDevCapsA( _In_ UINT_PTR uDeviceID, _Out_ LPWAVEOUTCAPSA pwoc,_In_ UINT cbwoc)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetDevCapsA);
	return trampoline(uDeviceID,pwoc,cbwoc);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetDevCapsW( _In_ UINT_PTR uDeviceID, _Out_ LPWAVEOUTCAPSW pwoc, _In_ UINT cbwoc)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetDevCapsW);
	return trampoline(uDeviceID,pwoc,cbwoc);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetErrorTextA( _In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText, _In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetErrorTextW(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetID(  _In_ HWAVEOUT hwo, _Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetID);
	return trampoline(hwo,puDeviceID);
}

HOOK_EXPORT UINT WINAPI HookwaveOutGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetNumDevs);
	return trampoline();
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetPitch(_In_ HWAVEOUT hwo, _Out_ LPDWORD pdwPitch)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPitch' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPitch);
	return trampoline(hwo,pdwPitch);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetPlaybackRate(_In_ HWAVEOUT hwo, _Out_ LPDWORD pdwRate)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPlaybackRate' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPlaybackRate);
	return trampoline(hwo,pdwRate);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetPosition( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbmmt) LPMMTIME pmmt, _In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPosition);
	return trampoline(hwo,pmmt,cbmmt);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutGetVolume(_In_opt_ HWAVEOUT hwo,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetVolume);
	return trampoline(hwo,pdwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutMessage(_In_opt_ HWAVEOUT hwo, _In_ UINT uMsg, _In_ DWORD_PTR dw1,_In_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'waveOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutMessage);
	return trampoline(hwo,uMsg,dw1,dw2);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutOpen( _Out_opt_ LPHWAVEOUT phwo, _In_ UINT uDeviceID, _In_ LPCWAVEFORMATEX pwfx, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'waveOutOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutOpen);
	return trampoline(phwo,uDeviceID,pwfx,dwCallback,dwInstance,fdwOpen);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutPause(_In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutPause' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutPause);
	return trampoline(hwo);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutPrepareHeader( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutPrepareHeader);
	return trampoline(hwo,pwh,cbwh);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutReset(  _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutReset' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutReset);
	return trampoline(hwo);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutRestart(  _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutRestart' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutRestart);
	return trampoline(hwo);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutSetPitch(_In_ HWAVEOUT hwo,_In_ DWORD dwPitch)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetPitch' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetPitch);
	return trampoline(hwo,dwPitch);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutSetPlaybackRate( _In_ HWAVEOUT hwo, _In_ DWORD dwRate)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetPlaybackRate' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetPlaybackRate);
	return trampoline(hwo,dwRate);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutSetVolume( _In_opt_ HWAVEOUT hwo,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetVolume);
	return trampoline(hwo,dwVolume);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutUnprepareHeader( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutUnprepareHeader);
	return trampoline(hwo,pwh,cbwh);
}

HOOK_EXPORT MMRESULT WINAPI HookwaveOutWrite(  _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutWrite' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutWrite);
	return trampoline(hwo,pwh,cbwh);
}
