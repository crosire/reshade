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
extern "C" LRESULT WINAPI HookCloseDriver(_In_ HDRVR hDriver,_In_ LPARAM lParam1,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'CloseDriver' to original library...";
	static const auto trampoline = reshade::hooks::call(CloseDriver);
	return trampoline(hDriver,lParam1,lParam2);
}

extern "C" LRESULT WINAPI HookDefDriverProc(_In_ DWORD_PTR dwDriverIdentifier,_In_ HDRVR hdrvr,_In_ UINT uMsg,_In_ LPARAM lParam1,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'DefDriverProc' to original library...";
	static const auto trampoline = reshade::hooks::call(DefDriverProc);
	return trampoline(dwDriverIdentifier,hdrvr,uMsg,lParam1,lParam2);
}

extern "C" BOOL WINAPI HookDriverCallback(DWORD_PTR dwCallback,DWORD dwFlags,HDRVR hDevice,DWORD dwMsg,DWORD_PTR dwUser,DWORD_PTR dwParam1,DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'DriverCallback' to original library...";
	static const auto trampoline = reshade::hooks::call(DriverCallback);
	return trampoline(dwCallback,dwFlags,hDevice,dwMsg,dwUser,dwParam1,dwParam2);
}

extern "C" HMODULE WINAPI HookDrvGetModuleHandle(_In_ HDRVR hDriver )
{
	//LOG(DEBUG) << "Redirecting 'DrvGetModuleHandle' to original library...";
	static const auto trampoline = reshade::hooks::call(DrvGetModuleHandle);
	return trampoline(hDriver);
}

extern "C" HMODULE WINAPI HookGetDriverModuleHandle(_In_ HDRVR hDriver)
{
	//LOG(DEBUG) << "Redirecting 'GetDriverModuleHandle' to original library...";
	static const auto trampoline = reshade::hooks::call(GetDriverModuleHandle);
	return trampoline(hDriver);
}

extern "C" HDRVR WINAPI HookOpenDriver(_In_ LPCWSTR szDriverName,_In_ LPCWSTR szSectionName,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'OpenDriver' to original library...";
	static const auto trampoline = reshade::hooks::call(OpenDriver);
	return trampoline(szDriverName,szSectionName,lParam2);
}

/* just a define to select PlaySoundA orPlaySoundAW
extern "C" void WINAPI PlaySound()
{
	//LOG(DEBUG) << "Redirecting 'PlaySound' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySound);
	trampoline();
}
*/

extern "C" BOOL WINAPI HookPlaySoundA(_In_opt_ LPCSTR pszSound,_In_opt_ HMODULE hmod,_In_ DWORD fdwSound)
{
	//LOG(DEBUG) << "Redirecting 'PlaySoundA' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySoundA);
	return trampoline(pszSound,hmod,fdwSound);
}

extern "C" BOOL WINAPI HookPlaySoundW( _In_opt_ LPCWSTR pszSound,_In_opt_ HMODULE hmod,_In_ DWORD fdwSound)
{
	//LOG(DEBUG) << "Redirecting 'PlaySoundW' to original library...";
	static const auto trampoline = reshade::hooks::call(PlaySoundW);
	return trampoline(pszSound,hmod,fdwSound);
}

extern "C" LRESULT WINAPI HookSendDriverMessage(  _In_ HDRVR hDriver,_In_ UINT message,_In_ LPARAM lParam1,_In_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'SendDriverMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(SendDriverMessage);
	return trampoline(hDriver,message,lParam1,lParam2);
}

/* @Deprecated
extern "C" void WINAPI HookWOWAppExit()
{
	//LOG(DEBUG) << "Redirecting 'WOWAppExit' to original library...";
	static const auto trampoline = reshade::hooks::call(WOWAppExit);
	return trampoline();
}
*/

extern "C" MMRESULT WINAPI HookauxGetDevCapsA( _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbac) LPAUXCAPSA pac,_In_ UINT cbac)
{
	//LOG(DEBUG) << "Redirecting 'auxGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetDevCapsA);
	return trampoline(uDeviceID,pac,cbac);
}

extern "C" MMRESULT WINAPI HookauxGetDevCapsW( _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbac) LPAUXCAPSW pac,_In_ UINT cbac)
{
	//LOG(DEBUG) << "Redirecting 'auxGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetDevCapsW);
	return trampoline(uDeviceID,pac,cbac);
}

extern "C" UINT WINAPI HookauxGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'auxGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookauxGetVolume(_In_ UINT uDeviceID,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'auxGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(auxGetVolume);
	return trampoline(uDeviceID,pdwVolume);
}

extern "C" MMRESULT WINAPI HookauxOutMessage( _In_ UINT uDeviceID,_In_ UINT uMsg,_In_opt_ DWORD_PTR dw1,_In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'auxOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(auxOutMessage);
	return trampoline(uDeviceID,uMsg,dw1,dw2);
}

extern "C" MMRESULT WINAPI HookauxSetVolume(  _In_ UINT uDeviceID,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'auxSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(auxSetVolume);
	return trampoline(uDeviceID,dwVolume);
}

extern "C" MMRESULT WINAPI HookjoyConfigChanged(  _In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'joyConfigChanged' to original library...";
	static const auto trampoline = reshade::hooks::call(joyConfigChanged);
	return trampoline(dwFlags);
}

extern "C" MMRESULT WINAPI HookjoyGetDevCapsA(  _In_ UINT_PTR uJoyID, _Out_writes_bytes_(cbjc) LPJOYCAPSA pjc,_In_ UINT cbjc)
{
	//LOG(DEBUG) << "Redirecting 'joyGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetDevCapsA);
	return trampoline(uJoyID,pjc,cbjc);
}

extern "C" MMRESULT WINAPI HookjoyGetDevCapsW(  _In_ UINT_PTR uJoyID, _Out_writes_bytes_(cbjc) LPJOYCAPSW pjc, _In_ UINT cbjc)
{
	//LOG(DEBUG) << "Redirecting 'joyGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetDevCapsW);
	return trampoline(uJoyID,pjc,cbjc);
}

extern "C" UINT WINAPI HookjoyGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'joyGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookjoyGetPos(  _In_ UINT uJoyID, _Out_ LPJOYINFO pji )
{
	//LOG(DEBUG) << "Redirecting 'joyGetPos' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetPos);
	return trampoline(uJoyID,pji);
}

extern "C" MMRESULT WINAPI HookjoyGetPosEx( _In_ UINT uJoyID,_Out_ LPJOYINFOEX pji)
{
	//LOG(DEBUG) << "Redirecting 'joyGetPosEx' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetPosEx);
	return trampoline(uJoyID,pji);
}

extern "C" MMRESULT WINAPI HookjoyGetThreshold(_In_ UINT uJoyID,_Out_ LPUINT puThreshold)
{
	//LOG(DEBUG) << "Redirecting 'joyGetThreshold' to original library...";
	static const auto trampoline = reshade::hooks::call(joyGetThreshold);
	return trampoline(uJoyID,puThreshold);
}

extern "C" MMRESULT WINAPI HookjoyReleaseCapture(_In_ UINT uJoyID)
{
	//LOG(DEBUG) << "Redirecting 'joyReleaseCapture' to original library...";
	static const auto trampoline = reshade::hooks::call(joyReleaseCapture);
	return trampoline(uJoyID);
}

extern "C" MMRESULT WINAPI HookjoySetCapture( _In_ HWND hwnd,_In_ UINT uJoyID,_In_ UINT uPeriod,_In_ BOOL fChanged)
{
	//LOG(DEBUG) << "Redirecting 'joySetCapture' to original library...";
	static const auto trampoline = reshade::hooks::call(joySetCapture);
	return trampoline(hwnd,uJoyID,uPeriod,fChanged);
}

extern "C" MMRESULT WINAPI HookjoySetThreshold(_In_ UINT uJoyID,_In_ UINT uThreshold)
{
	//LOG(DEBUG) << "Redirecting 'joySetThreshold' to original library...";
	static const auto trampoline = reshade::hooks::call(joySetThreshold);
	return trampoline(uJoyID,uThreshold);
}

extern "C" BOOL WINAPI HookmciDriverNotify( HANDLE hwndCallback, MCIDEVICEID wDeviceID,UINT uStatus)
{
	//LOG(DEBUG) << "Redirecting 'mciDriverNotify' to original library...";
	static const auto trampoline = reshade::hooks::call(mciDriverNotify);
	return trampoline(hwndCallback,wDeviceID,uStatus);
}

extern "C" UINT WINAPI HookmciDriverYield( MCIDEVICEID wDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'mciDriverYield' to original library...";
	static const auto trampoline = reshade::hooks::call(mciDriverYield);
	return trampoline(wDeviceID);
}

/* @Deprecated
extern "C" void WINAPI HookmciExecute()
{
	//LOG(DEBUG) << "Redirecting 'mciExecute' to original library...";
	static const auto trampoline = reshade::hooks::call(mciExecute);
	return trampoline();
}
*/

extern "C" BOOL WINAPI HookmciFreeCommandResource( UINT wTable)
{
	//LOG(DEBUG) << "Redirecting 'mciFreeCommandResource' to original library...";
	static const auto trampoline = reshade::hooks::call(mciFreeCommandResource);
	return trampoline(wTable);
}

extern "C" HTASK WINAPI HookmciGetCreatorTask( _In_ MCIDEVICEID mciId)
{
	//LOG(DEBUG) << "Redirecting 'mciGetCreatorTask' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetCreatorTask);
	return trampoline(mciId);
}

extern "C" MCIDEVICEID WINAPI HookmciGetDeviceIDA( _In_ LPCSTR pszDevice)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDA);
	return trampoline(pszDevice);
}

extern "C" MCIDEVICEID WINAPI HookmciGetDeviceIDFromElementIDA(  _In_ DWORD dwElementID, _In_ LPCSTR lpstrType)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDFromElementIDA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDFromElementIDA);
	return trampoline(dwElementID, lpstrType);
}

extern "C" MCIDEVICEID WINAPI HookmciGetDeviceIDFromElementIDW(_In_ DWORD dwElementID,_In_ LPCWSTR lpstrType)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDFromElementIDW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDFromElementIDW);
	return trampoline(dwElementID,lpstrType);
}

extern "C" MCIDEVICEID WINAPI HookmciGetDeviceIDW(_In_ LPCWSTR pszDevice)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDeviceIDW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDeviceIDW);
	return trampoline(pszDevice);
}

extern "C" DWORD_PTR WINAPI HookmciGetDriverData(MCIDEVICEID wDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'mciGetDriverData' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetDriverData);
	return trampoline(wDeviceID);
}

extern "C" BOOL WINAPI HookmciGetErrorStringA( _In_ MCIERROR mcierr,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'mciGetErrorStringA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetErrorStringA);
	return trampoline(mcierr,pszText,cchText);
}

extern "C" BOOL WINAPI HookmciGetErrorStringW(_In_ MCIERROR mcierr,_Out_writes_(cchText) LPWSTR pszText, _In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'mciGetErrorStringW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetErrorStringW);
	return trampoline(mcierr,pszText,cchText);
}

extern "C" YIELDPROC WINAPI HookmciGetYieldProc( _In_ MCIDEVICEID mciId,_In_ LPDWORD pdwYieldData)
{
	//LOG(DEBUG) << "Redirecting 'mciGetYieldProc' to original library...";
	static const auto trampoline = reshade::hooks::call(mciGetYieldProc);
	return trampoline(mciId,pdwYieldData);
}

extern "C" UINT WINAPI HookmciLoadCommandResource(  HANDLE hInstance, LPCWSTR lpResName, UINT wType)
{
	//LOG(DEBUG) << "Redirecting 'mciLoadCommandResource' to original library...";
	static const auto trampoline = reshade::hooks::call(mciLoadCommandResource);
	return trampoline(hInstance,lpResName,wType);
}

extern "C" MCIERROR WINAPI HookmciSendCommandA( _In_ MCIDEVICEID mciId,_In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1,_In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mciSendCommandA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendCommandA);
	return trampoline(mciId,uMsg,dwParam1,dwParam2);
}

extern "C" MCIERROR WINAPI HookmciSendCommandW(  _In_ MCIDEVICEID mciId, _In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1,_In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mciSendCommandW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendCommandW);
	return trampoline(mciId,uMsg,dwParam1,dwParam2);
}

extern "C" MCIERROR WINAPI HookmciSendStringA( _In_ LPCSTR lpstrCommand, _Out_writes_opt_(uReturnLength) LPSTR lpstrReturnString,_In_ UINT uReturnLength,_In_opt_ HWND hwndCallback)
{
	//LOG(DEBUG) << "Redirecting 'mciSendStringA' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendStringA);
	return trampoline(lpstrCommand,lpstrReturnString,uReturnLength,hwndCallback);
}

extern "C" MCIERROR WINAPI HookmciSendStringW(_In_ LPCWSTR lpstrCommand,_Out_writes_opt_(uReturnLength) LPWSTR lpstrReturnString,_In_ UINT uReturnLength,_In_opt_ HWND hwndCallback)
{
	//LOG(DEBUG) << "Redirecting 'mciSendStringW' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSendStringW);
	return trampoline(lpstrCommand,lpstrReturnString,uReturnLength,hwndCallback);
}

extern "C" BOOL WINAPI HookmciSetDriverData( MCIDEVICEID wDeviceID, DWORD_PTR dwData)
{
	//LOG(DEBUG) << "Redirecting 'mciSetDriverData' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSetDriverData);
	return trampoline(wDeviceID,dwData);
}

extern "C" BOOL WINAPI HookmciSetYieldProc(  _In_ MCIDEVICEID mciId, _In_opt_ YIELDPROC fpYieldProc,_In_ DWORD dwYieldData)
{
	//LOG(DEBUG) << "Redirecting 'mciSetYieldProc' to original library...";
	static const auto trampoline = reshade::hooks::call(mciSetYieldProc);
	return trampoline(mciId,fpYieldProc,dwYieldData);
}

extern "C" MMRESULT WINAPI HookmidiConnect( _In_ HMIDI hmi, _In_ HMIDIOUT hmo, _In_opt_ LPVOID pReserved)
{
	//LOG(DEBUG) << "Redirecting 'midiConnect' to original library...";
	static const auto trampoline = reshade::hooks::call(midiConnect);
	return trampoline(hmi,hmo,pReserved);
}

extern "C" MMRESULT WINAPI HookmidiDisconnect( _In_ HMIDI hmi,_In_ HMIDIOUT hmo, _In_opt_ LPVOID pReserved)
{
	//LOG(DEBUG) << "Redirecting 'midiDisconnect' to original library...";
	static const auto trampoline = reshade::hooks::call(midiDisconnect);
	return trampoline(hmi,hmo,pReserved);
}

extern "C" MMRESULT WINAPI HookmidiInAddBuffer(  _In_ HMIDIIN hmi, _Out_writes_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInAddBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInAddBuffer);
	return trampoline(hmi,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiInClose(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInClose);
	return trampoline(hmi);
}

extern "C" MMRESULT WINAPI HookmidiInGetDevCapsA(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbmic) LPMIDIINCAPSA pmic,_In_ UINT cbmic)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetDevCapsA);
	return trampoline(uDeviceID,pmic,cbmic);
}

extern "C" MMRESULT WINAPI HookmidiInGetDevCapsW(_In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbmic) LPMIDIINCAPSW pmic,_In_ UINT cbmic)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetDevCapsW);
	return trampoline(uDeviceID,pmic,cbmic);
}

extern "C" MMRESULT WINAPI HookmidiInGetErrorTextA(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookmidiInGetErrorTextW( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookmidiInGetID(_In_ HMIDIIN hmi, _Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'midiInGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetID);
	return trampoline(hmi,puDeviceID);
}

extern "C" UINT WINAPI HookmidiInGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'midiInGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookmidiInMessage(_In_opt_ HMIDIIN hmi, _In_ UINT uMsg, _In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'midiInMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInMessage);
	return trampoline(hmi,uMsg,dw1,dw2);
}

extern "C" MMRESULT WINAPI HookmidiInOpen(_Out_ LPHMIDIIN phmi,_In_ UINT uDeviceID,_In_opt_ DWORD_PTR dwCallback,_In_opt_ DWORD_PTR dwInstance,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiInOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInOpen);
	return trampoline(phmi,uDeviceID,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookmidiInPrepareHeader(_In_ HMIDIIN hmi,_Inout_updates_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInPrepareHeader);
	return trampoline(hmi,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiInReset(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInReset' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInReset);
	return trampoline(hmi);
}

extern "C" MMRESULT WINAPI HookmidiInStart(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInStart' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInStart);
	return trampoline(hmi);
}

extern "C" MMRESULT WINAPI HookmidiInStop(_In_ HMIDIIN hmi)
{
	//LOG(DEBUG) << "Redirecting 'midiInStop' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInStop);
	return trampoline(hmi);
}

extern "C" MMRESULT WINAPI HookmidiInUnprepareHeader(_In_ HMIDIIN hmi, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiInUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiInUnprepareHeader);
	return trampoline(hmi,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiOutCacheDrumPatches(  _In_ HMIDIOUT hmo,_In_ UINT uPatch,_In_reads_(MIDIPATCHSIZE) LPWORD pwkya, _In_ UINT fuCache)
{
	//LOG(DEBUG) << "Redirecting 'midiOutCacheDrumPatches' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutCacheDrumPatches);
	return trampoline(hmo,uPatch,pwkya,fuCache);
}


extern "C" MMRESULT WINAPI HookmidiOutCachePatches(_In_ HMIDIOUT hmo, _In_ UINT uBank,_In_reads_(MIDIPATCHSIZE) LPWORD pwpa, _In_ UINT fuCache)
{
	//LOG(DEBUG) << "Redirecting 'midiOutCachePatches' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutCachePatches);
	return trampoline(hmo,uBank,pwpa,fuCache);
}

extern "C" MMRESULT WINAPI HookmidiOutClose(_In_ HMIDIOUT hmo)
{
	//LOG(DEBUG) << "Redirecting 'midiOutClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutClose);
	return trampoline(hmo);
}

extern "C" MMRESULT WINAPI HookmidiOutGetDevCapsA(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbmoc) LPMIDIOUTCAPSA pmoc, _In_ UINT cbmoc)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetDevCapsA);
	return trampoline(uDeviceID, pmoc,cbmoc);
}

extern "C" MMRESULT WINAPI HookmidiOutGetDevCapsW(  _In_ UINT_PTR uDeviceID,_Out_writes_bytes_(cbmoc) LPMIDIOUTCAPSW pmoc,_In_ UINT cbmoc)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetDevCapsW);
	return trampoline(uDeviceID, pmoc,cbmoc);
}

extern "C" MMRESULT WINAPI HookmidiOutGetErrorTextA( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookmidiOutGetErrorTextW( _In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookmidiOutGetID( _In_ HMIDIOUT hmo,_Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetID);
	return trampoline(hmo,puDeviceID);
}

extern "C" UINT WINAPI HookmidiOutGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookmidiOutGetVolume(  _In_opt_ HMIDIOUT hmo,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'midiOutGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutGetVolume);
	return trampoline(hmo,pdwVolume);
}

extern "C" MMRESULT WINAPI HookmidiOutLongMsg(  _In_ HMIDIOUT hmo,  _In_reads_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutLongMsg' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutLongMsg);
	return trampoline(hmo,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiOutMessage( _In_opt_ HMIDIOUT hmo,_In_ UINT uMsg,_In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'midiOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutMessage);
	return trampoline(hmo,uMsg, dw1,dw2);
}

extern "C" MMRESULT WINAPI HookmidiOutOpen(   _Out_ LPHMIDIOUT phmo, _In_ UINT uDeviceID, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiOutOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutOpen);
	return trampoline(phmo, uDeviceID,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookmidiOutPrepareHeader( _In_ HMIDIOUT hmo, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh,_In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutPrepareHeader);
	return trampoline(hmo,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiOutReset(_In_ HMIDIOUT hmo)
{
	//LOG(DEBUG) << "Redirecting 'midiOutReset' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutReset);
	return trampoline(hmo);
}

extern "C" MMRESULT WINAPI HookmidiOutSetVolume( _In_opt_ HMIDIOUT hmo,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'midiOutSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutSetVolume);
	return trampoline(hmo,dwVolume);
}

extern "C" MMRESULT WINAPI HookmidiOutShortMsg( _In_ HMIDIOUT hmo, _In_ DWORD dwMsg)
{
	//LOG(DEBUG) << "Redirecting 'midiOutShortMsg' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutShortMsg);
	return trampoline(hmo,dwMsg);
}

extern "C" MMRESULT WINAPI HookmidiOutUnprepareHeader(    _In_ HMIDIOUT hmo, _Inout_updates_bytes_(cbmh) LPMIDIHDR pmh,_In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiOutUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(midiOutUnprepareHeader);
	return trampoline(hmo,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiStreamClose( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamClose' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamClose);
	return trampoline(hms);
}

extern "C" MMRESULT WINAPI HookmidiStreamOpen( _Out_ LPHMIDISTRM phms, _Inout_updates_(cMidi) LPUINT puDeviceID, _In_ DWORD cMidi, _In_opt_ DWORD_PTR dwCallback,_In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamOpen);
	return trampoline(phms,puDeviceID,cMidi,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookmidiStreamOut(   _In_ HMIDISTRM hms, _Out_writes_bytes_(cbmh) LPMIDIHDR pmh, _In_ UINT cbmh)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamOut' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamOut);
	return trampoline(hms,pmh,cbmh);
}

extern "C" MMRESULT WINAPI HookmidiStreamPause(_In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamPause' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamPause);
	return trampoline(hms);
}

extern "C" MMRESULT WINAPI HookmidiStreamPosition( _In_ HMIDISTRM hms,_Out_writes_bytes_(cbmmt) LPMMTIME lpmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamPosition);
	return trampoline(hms,lpmmt,cbmmt);
}

extern "C" MMRESULT WINAPI HookmidiStreamProperty( _In_ HMIDISTRM hms,  _Inout_updates_bytes_(sizeof(DWORD) + sizeof(DWORD)) LPBYTE lppropdata,_In_ DWORD dwProperty)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamProperty' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamProperty);
	return trampoline(hms,lppropdata,dwProperty);
}

extern "C" MMRESULT WINAPI HookmidiStreamRestart( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamRestart' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamRestart);
	return trampoline(hms);
}

extern "C" MMRESULT WINAPI HookmidiStreamStop( _In_ HMIDISTRM hms)
{
	//LOG(DEBUG) << "Redirecting 'midiStreamStop' to original library...";
	static const auto trampoline = reshade::hooks::call(midiStreamStop);
	return trampoline(hms);
}

extern "C" MMRESULT WINAPI HookmixerClose(_In_ HMIXER hmx)
{
	//LOG(DEBUG) << "Redirecting 'mixerClose' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerClose);
	return trampoline(hmx);
}

extern "C" MMRESULT WINAPI HookmixerGetControlDetailsA( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERCONTROLDETAILS pmxcd,_In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetControlDetailsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetControlDetailsA);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

extern "C" MMRESULT WINAPI HookmixerGetControlDetailsW( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERCONTROLDETAILS pmxcd, _In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetControlDetailsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetControlDetailsW);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

extern "C" MMRESULT WINAPI HookmixerGetDevCapsA( _In_ UINT_PTR uMxId,_Out_writes_bytes_(cbmxcaps) LPMIXERCAPSA pmxcaps,_In_ UINT cbmxcaps)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetDevCapsA);
	return trampoline(uMxId,pmxcaps,cbmxcaps);
}

extern "C" MMRESULT WINAPI HookmixerGetDevCapsW(   _In_ UINT_PTR uMxId, _Out_writes_bytes_(cbmxcaps) LPMIXERCAPSW pmxcaps,_In_ UINT cbmxcaps)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetDevCapsW);
		return trampoline(uMxId,pmxcaps,cbmxcaps);
}

extern "C" MMRESULT WINAPI HookmixerGetID( _In_opt_ HMIXEROBJ hmxobj, _Out_ UINT  FAR * puMxId, _In_ DWORD fdwId)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetID);
	return trampoline(hmxobj,puMxId,fdwId);
}

extern "C" MMRESULT WINAPI HookmixerGetLineControlsA( _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINECONTROLSA pmxlc,_In_ DWORD fdwControls)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineControlsA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineControlsA);
	return trampoline(hmxobj,pmxlc,fdwControls);
}

extern "C" MMRESULT WINAPI HookmixerGetLineControlsW(  _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINECONTROLSW pmxlc, _In_ DWORD fdwControls)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineControlsW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineControlsW);
	return trampoline(hmxobj,pmxlc,fdwControls);
}

extern "C" MMRESULT WINAPI HookmixerGetLineInfoA( _In_opt_ HMIXEROBJ hmxobj,_Inout_ LPMIXERLINEA pmxl, _In_ DWORD fdwInfo)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineInfoA' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineInfoA);
	return trampoline(hmxobj,pmxl,fdwInfo);
}

extern "C" MMRESULT WINAPI HookmixerGetLineInfoW(  _In_opt_ HMIXEROBJ hmxobj, _Inout_ LPMIXERLINEW pmxl, _In_ DWORD fdwInfo)
{
	//LOG(DEBUG) << "Redirecting 'mixerGetLineInfoW' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetLineInfoW);
	return trampoline(hmxobj,pmxl,fdwInfo);
}

extern "C" UINT WINAPI HookmixerGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'mixerGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerGetNumDevs);
	return trampoline();
}

extern "C" DWORD WINAPI HookmixerMessage(_In_opt_ HMIXER hmx, _In_ UINT uMsg,_In_opt_ DWORD_PTR dwParam1, _In_opt_ DWORD_PTR dwParam2)
{
	//LOG(DEBUG) << "Redirecting 'mixerMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerMessage);
	return trampoline(hmx,uMsg,dwParam1,dwParam2);
}

extern "C" MMRESULT WINAPI HookmixerOpen(    _Out_opt_ LPHMIXER phmx,  _In_ UINT uMxId, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mixerOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerOpen);
	return trampoline(phmx,uMxId,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookmixerSetControlDetails( _In_opt_ HMIXEROBJ hmxobj, _In_ LPMIXERCONTROLDETAILS pmxcd, _In_ DWORD fdwDetails)
{
	//LOG(DEBUG) << "Redirecting 'mixerSetControlDetails' to original library...";
	static const auto trampoline = reshade::hooks::call(mixerSetControlDetails);
	return trampoline(hmxobj,pmxcd,fdwDetails);
}

extern "C" UINT WINAPI HookmmDrvInstall(  HDRVR hDriver, LPCWSTR wszDrvEntry, DRIVERMSGPROC drvMessage, UINT wFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmDrvInstall' to original library...";
	static const auto trampoline = reshade::hooks::call(mmDrvInstall);
	return trampoline(hDriver,wszDrvEntry,drvMessage,wFlags);
}

/* @Deprecated
extern "C" void WINAPI HookmmGetCurrentTask()
{
	//LOG(DEBUG) << "Redirecting 'mmGetCurrentTask' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmGetCurrentTask);
	//return trampoline();
}

extern "C" void WINAPI HookmmTaskBlock()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskBlock' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskBlock);
	//return trampoline();
}

extern "C" void WINAPI HookmmTaskCreate()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskCreate' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskCreate);
	//return trampoline();
}

extern "C" void WINAPI HookmmTaskSignal()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskSignal' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskSignal);
	//return trampoline();
}

extern "C" void WINAPI HookmmTaskYield()
{
	//LOG(DEBUG) << "Redirecting 'mmTaskYield' to original library...";
	//static const auto trampoline = reshade::hooks::call(mmTaskYield);
	//return trampoline();
}
*/

extern "C" MMRESULT WINAPI HookmmioAdvance(_In_ HMMIO hmmio,_In_opt_ LPMMIOINFO pmmioinfo,_In_ UINT fuAdvance)
{
	//LOG(DEBUG) << "Redirecting 'mmioAdvance' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioAdvance);
	return trampoline(hmmio,pmmioinfo,fuAdvance);
}

extern "C" MMRESULT WINAPI HookmmioAscend( _In_ HMMIO hmmio, _In_ LPMMCKINFO pmmcki,_In_ UINT fuAscend)
{
	//LOG(DEBUG) << "Redirecting 'mmioAscend' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioAscend);
	return trampoline(hmmio,pmmcki,fuAscend);
}

extern "C" MMRESULT WINAPI HookmmioClose( _In_ HMMIO hmmio,_In_ UINT fuClose)
{
	//LOG(DEBUG) << "Redirecting 'mmioClose' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioClose);
	return trampoline(hmmio,fuClose);
}

extern "C" MMRESULT WINAPI HookmmioCreateChunk(_In_ HMMIO hmmio,_In_ LPMMCKINFO pmmcki,_In_ UINT fuCreate)
{
	//LOG(DEBUG) << "Redirecting 'mmioCreateChunk' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioCreateChunk);
	return trampoline(hmmio,pmmcki,fuCreate);
}

extern "C" MMRESULT WINAPI HookmmioDescend(  _In_ HMMIO hmmio, _Inout_ LPMMCKINFO pmmcki, _In_opt_ const MMCKINFO  FAR * pmmckiParent, _In_ UINT fuDescend)
{
	//LOG(DEBUG) << "Redirecting 'mmioDescend' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioDescend);
	return trampoline(hmmio,pmmcki,pmmckiParent,fuDescend);
}

extern "C" MMRESULT WINAPI HookmmioFlush(  _In_ HMMIO hmmio,_In_ UINT fuFlush)
{
	//LOG(DEBUG) << "Redirecting 'mmioFlush' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioFlush);
	return trampoline(hmmio,fuFlush);
}

extern "C" MMRESULT WINAPI HookmmioGetInfo( _In_ HMMIO hmmio, _Out_ LPMMIOINFO pmmioinfo, _In_ UINT fuInfo)
{
	//LOG(DEBUG) << "Redirecting 'mmioGetInfo' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioGetInfo);
	return trampoline(hmmio,pmmioinfo,fuInfo);
}

extern "C" LPMMIOPROC WINAPI HookmmioInstallIOProcA( _In_ FOURCC fccIOProc,_In_opt_ LPMMIOPROC pIOProc,_In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioInstallIOProcA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioInstallIOProcA);
	return trampoline(fccIOProc,pIOProc,dwFlags);
}

extern "C" LPMMIOPROC WINAPI HookmmioInstallIOProcW(   _In_ FOURCC fccIOProc, _In_opt_ LPMMIOPROC pIOProc, _In_ DWORD dwFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioInstallIOProcW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioInstallIOProcW);
	return trampoline(fccIOProc,pIOProc,dwFlags);
}

extern "C" HMMIO WINAPI HookmmioOpenA( _Inout_updates_bytes_opt_(128) LPSTR pszFileName,_Inout_opt_ LPMMIOINFO pmmioinfo,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mmioOpenA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioOpenA);
	return trampoline(pszFileName,pmmioinfo,fdwOpen);
}

extern "C" HMMIO WINAPI HookmmioOpenW( _Inout_updates_bytes_opt_(128) LPWSTR pszFileName,_Inout_opt_ LPMMIOINFO pmmioinfo,_In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'mmioOpenW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioOpenW);
	return trampoline(pszFileName,pmmioinfo,fdwOpen);
}

extern "C" LONG WINAPI HookmmioRead(  _In_ HMMIO hmmio, _Out_writes_bytes_(cch) HPSTR pch,_In_ LONG cch)
{
	//LOG(DEBUG) << "Redirecting 'mmioRead' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRead);
	return trampoline(hmmio,pch,cch);
}

extern "C" MMRESULT WINAPI HookmmioRenameA(_In_ LPCSTR pszFileName, _In_ LPCSTR pszNewFileName, _In_opt_ LPCMMIOINFO pmmioinfo, _In_ DWORD fdwRename)
{
	//LOG(DEBUG) << "Redirecting 'mmioRenameA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRenameA);
	return trampoline(pszFileName,pszNewFileName,pmmioinfo,fdwRename);
}

extern "C" MMRESULT WINAPI HookmmioRenameW( _In_ LPCWSTR pszFileName, _In_ LPCWSTR pszNewFileName, _In_opt_ LPCMMIOINFO pmmioinfo, _In_ DWORD fdwRename)
{
	//LOG(DEBUG) << "Redirecting 'mmioRenameW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioRenameW);
	return trampoline(pszFileName,pszNewFileName,pmmioinfo,fdwRename);
}

extern "C" LONG WINAPI HookmmioSeek(_In_ HMMIO hmmio, _In_ LONG lOffset,_In_ int iOrigin)
{
	//LOG(DEBUG) << "Redirecting 'mmioSeek' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSeek);
	return trampoline(hmmio,lOffset,iOrigin);
}

extern "C" LRESULT WINAPI HookmmioSendMessage(  _In_ HMMIO hmmio,_In_ UINT uMsg, _In_opt_ LPARAM lParam1,_In_opt_ LPARAM lParam2)
{
	//LOG(DEBUG) << "Redirecting 'mmioSendMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSendMessage);
	return trampoline(hmmio,uMsg,lParam1,lParam2);
}

extern "C" MMRESULT WINAPI HookmmioSetBuffer(  _In_ HMMIO hmmio, _Out_writes_opt_(cchBuffer) LPSTR pchBuffer, _In_ LONG cchBuffer, _In_ UINT fuBuffer)
{
	//LOG(DEBUG) << "Redirecting 'mmioSetBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSetBuffer);
	return trampoline(hmmio,pchBuffer,cchBuffer,fuBuffer);
}

extern "C" MMRESULT WINAPI HookmmioSetInfo(_In_ HMMIO hmmio, _In_ LPCMMIOINFO pmmioinfo, _In_ UINT fuInfo)
{
	//LOG(DEBUG) << "Redirecting 'mmioSetInfo' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioSetInfo);
	return trampoline(hmmio,pmmioinfo,fuInfo);
}

extern "C" FOURCC WINAPI HookmmioStringToFOURCCA(  LPCSTR sz, _In_ UINT uFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioStringToFOURCCA' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioStringToFOURCCA);
	return trampoline(sz,uFlags);
}

extern "C" FOURCC WINAPI HookmmioStringToFOURCCW(LPCWSTR sz,_In_ UINT uFlags)
{
	//LOG(DEBUG) << "Redirecting 'mmioStringToFOURCCW' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioStringToFOURCCW);
	return trampoline(sz,uFlags);
}

extern "C" LONG WINAPI HookmmioWrite( _In_ HMMIO hmmio,_In_reads_bytes_(cch) const char  _huge * pch, _In_ LONG cch)
{
	//LOG(DEBUG) << "Redirecting 'mmioWrite' to original library...";
	static const auto trampoline = reshade::hooks::call(mmioWrite);
	return trampoline(hmmio,pch,cch);
}

/* @Deprecated
extern "C" UINT WINAPI HookmmsystemGetVersion()
{
	//LOG(DEBUG) << "Redirecting 'mmsystemGetVersion' to original library...";
	static const auto trampoline = reshade::hooks::call(mmsystemGetVersion);
	return trampoline();
}
*/

extern "C" BOOL WINAPI HooksndPlaySoundA(_In_opt_ LPCSTR pszSound,_In_ UINT fuSound)
{
	//LOG(DEBUG) << "Redirecting 'sndPlaySoundA' to original library...";
	static const auto trampoline = reshade::hooks::call(sndPlaySoundA);
	return trampoline(pszSound,fuSound);
}

extern "C" BOOL WINAPI HooksndPlaySoundW(  _In_opt_ LPCWSTR pszSound,_In_ UINT fuSound)
{
	//LOG(DEBUG) << "Redirecting 'sndPlaySoundW' to original library...";
	static const auto trampoline = reshade::hooks::call(sndPlaySoundW);
	return trampoline(pszSound,fuSound);
}

extern "C" MMRESULT WINAPI HooktimeBeginPeriod(  _In_ UINT uPeriod)
{
	//LOG(DEBUG) << "Redirecting 'timeBeginPeriod' to original library...";
	static const auto trampoline = reshade::hooks::call(timeBeginPeriod);
	return trampoline(uPeriod);
}

extern "C" MMRESULT WINAPI HooktimeEndPeriod( _In_ UINT uPeriod)
{
	//LOG(DEBUG) << "Redirecting 'timeEndPeriod' to original library...";
	static const auto trampoline = reshade::hooks::call(timeEndPeriod);
	return trampoline(uPeriod);
}

extern "C" MMRESULT WINAPI HooktimeGetDevCaps(_Out_writes_bytes_(cbtc) LPTIMECAPS ptc,_In_ UINT cbtc)
{
	//LOG(DEBUG) << "Redirecting 'timeGetDevCaps' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetDevCaps);
	return trampoline(ptc,cbtc);
}

extern "C" MMRESULT WINAPI HooktimeGetSystemTime(  _Out_writes_bytes_(cbmmt) LPMMTIME pmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'timeGetSystemTime' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetSystemTime);
	return trampoline(pmmt,cbmmt);
}

extern "C" DWORD WINAPI HooktimeGetTime()
{
	//LOG(DEBUG) << "Redirecting 'timeGetTime' to original library...";
	static const auto trampoline = reshade::hooks::call(timeGetTime);
	return trampoline();
}

extern "C" MMRESULT WINAPI HooktimeKillEvent( _In_ UINT uTimerID)
{
	//LOG(DEBUG) << "Redirecting 'timeKillEvent' to original library...";
	static const auto trampoline = reshade::hooks::call(timeKillEvent);
	return trampoline(uTimerID);
}

extern "C" MMRESULT WINAPI HooktimeSetEvent( _In_ UINT uDelay, _In_ UINT uResolution,_In_ LPTIMECALLBACK fptc, _In_ DWORD_PTR dwUser, _In_ UINT fuEvent)
{
	//LOG(DEBUG) << "Redirecting 'timeSetEvent' to original library...";
	static const auto trampoline = reshade::hooks::call(timeSetEvent);
	return trampoline(uDelay,uResolution,fptc,dwUser,fuEvent);
}

extern "C" MMRESULT WINAPI HookwaveInAddBuffer( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInAddBuffer' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInAddBuffer);
	return trampoline(hwi,pwh,cbwh);
}

extern "C" MMRESULT WINAPI HookwaveInClose(_In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInClose' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInClose);
	return trampoline(hwi);
}

extern "C" MMRESULT WINAPI HookwaveInGetDevCapsA( _In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbwic) LPWAVEINCAPSA pwic,_In_ UINT cbwic)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetDevCapsA);
	return trampoline(uDeviceID,pwic,cbwic);
}

extern "C" MMRESULT WINAPI HookwaveInGetDevCapsW(_In_ UINT_PTR uDeviceID, _Out_writes_bytes_(cbwic) LPWAVEINCAPSW pwic,_In_ UINT cbwic)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetDevCapsW);
	return trampoline(uDeviceID,pwic,cbwic);
}

extern "C" MMRESULT WINAPI HookwaveInGetErrorTextA(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookwaveInGetErrorTextW(_In_ MMRESULT mmrError, _Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookwaveInGetID(_In_ HWAVEIN hwi,_In_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetID);
	return trampoline(hwi,puDeviceID);
}

extern "C" UINT WINAPI HookwaveInGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'waveInGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookwaveInGetPosition(_In_ HWAVEIN hwi,_Inout_updates_bytes_(cbmmt) LPMMTIME pmmt,_In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'waveInGetPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInGetPosition);
	return trampoline(hwi,pmmt,cbmmt);
}

extern "C" MMRESULT WINAPI HookwaveInMessage( _In_opt_ HWAVEIN hwi, _In_ UINT uMsg, _In_opt_ DWORD_PTR dw1, _In_opt_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'waveInMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInMessage);
	return trampoline(hwi,uMsg,dw1,dw2);
}

extern "C" MMRESULT WINAPI HookwaveInOpen(  _Out_opt_ LPHWAVEIN phwi, _In_ UINT uDeviceID, _In_ LPCWAVEFORMATEX pwfx, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'waveInOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInOpen);
	return trampoline(phwi,uDeviceID,pwfx,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookwaveInPrepareHeader( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInPrepareHeader);
	return trampoline(hwi,pwh,cbwh);
}

extern "C" MMRESULT WINAPI HookwaveInReset( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInReset' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInReset);
	return trampoline(hwi);
}

extern "C" MMRESULT WINAPI HookwaveInStart( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInStart' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInStart);
	return trampoline(hwi);
}

extern "C" MMRESULT WINAPI HookwaveInStop( _In_ HWAVEIN hwi)
{
	//LOG(DEBUG) << "Redirecting 'waveInStop' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInStop);
	return trampoline(hwi);
}

extern "C" MMRESULT WINAPI HookwaveInUnprepareHeader( _In_ HWAVEIN hwi, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveInUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveInUnprepareHeader);
	return trampoline(hwi,pwh,cbwh);
}

extern "C" MMRESULT WINAPI HookwaveOutBreakLoop( _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutBreakLoop' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutBreakLoop);
	return trampoline(hwo);
}

extern "C" MMRESULT WINAPI HookwaveOutClose(_In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutClose' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutClose);
	return trampoline(hwo);
}

extern "C" MMRESULT WINAPI HookwaveOutGetDevCapsA( _In_ UINT_PTR uDeviceID, _Out_ LPWAVEOUTCAPSA pwoc,_In_ UINT cbwoc)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetDevCapsA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetDevCapsA);
	return trampoline(uDeviceID,pwoc,cbwoc);
}

extern "C" MMRESULT WINAPI HookwaveOutGetDevCapsW( _In_ UINT_PTR uDeviceID, _Out_ LPWAVEOUTCAPSW pwoc, _In_ UINT cbwoc)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetDevCapsW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetDevCapsW);
	return trampoline(uDeviceID,pwoc,cbwoc);
}

extern "C" MMRESULT WINAPI HookwaveOutGetErrorTextA( _In_ MMRESULT mmrError,_Out_writes_(cchText) LPSTR pszText, _In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetErrorTextA' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetErrorTextA);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookwaveOutGetErrorTextW(_In_ MMRESULT mmrError,_Out_writes_(cchText) LPWSTR pszText,_In_ UINT cchText)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetErrorTextW' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetErrorTextW);
	return trampoline(mmrError,pszText,cchText);
}

extern "C" MMRESULT WINAPI HookwaveOutGetID(  _In_ HWAVEOUT hwo, _Out_ LPUINT puDeviceID)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetID' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetID);
	return trampoline(hwo,puDeviceID);
}

extern "C" UINT WINAPI HookwaveOutGetNumDevs()
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetNumDevs' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetNumDevs);
	return trampoline();
}

extern "C" MMRESULT WINAPI HookwaveOutGetPitch(_In_ HWAVEOUT hwo, _Out_ LPDWORD pdwPitch)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPitch' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPitch);
	return trampoline(hwo,pdwPitch);
}

extern "C" MMRESULT WINAPI HookwaveOutGetPlaybackRate(_In_ HWAVEOUT hwo, _Out_ LPDWORD pdwRate)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPlaybackRate' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPlaybackRate);
	return trampoline(hwo,pdwRate);
}

extern "C" MMRESULT WINAPI HookwaveOutGetPosition( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbmmt) LPMMTIME pmmt, _In_ UINT cbmmt)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetPosition' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetPosition);
	return trampoline(hwo,pmmt,cbmmt);
}

extern "C" MMRESULT WINAPI HookwaveOutGetVolume(_In_opt_ HWAVEOUT hwo,_Out_ LPDWORD pdwVolume)
{
	//LOG(DEBUG) << "Redirecting 'waveOutGetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutGetVolume);
	return trampoline(hwo,pdwVolume);
}

extern "C" MMRESULT WINAPI HookwaveOutMessage(_In_opt_ HWAVEOUT hwo, _In_ UINT uMsg, _In_ DWORD_PTR dw1,_In_ DWORD_PTR dw2)
{
	//LOG(DEBUG) << "Redirecting 'waveOutMessage' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutMessage);
	return trampoline(hwo,uMsg,dw1,dw2);
}

extern "C" MMRESULT WINAPI HookwaveOutOpen( _Out_opt_ LPHWAVEOUT phwo, _In_ UINT uDeviceID, _In_ LPCWAVEFORMATEX pwfx, _In_opt_ DWORD_PTR dwCallback, _In_opt_ DWORD_PTR dwInstance, _In_ DWORD fdwOpen)
{
	//LOG(DEBUG) << "Redirecting 'waveOutOpen' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutOpen);
	return trampoline(phwo,uDeviceID,pwfx,dwCallback,dwInstance,fdwOpen);
}

extern "C" MMRESULT WINAPI HookwaveOutPause(_In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutPause' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutPause);
	return trampoline(hwo);
}

extern "C" MMRESULT WINAPI HookwaveOutPrepareHeader( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutPrepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutPrepareHeader);
	return trampoline(hwo,pwh,cbwh);
}

extern "C" MMRESULT WINAPI HookwaveOutReset(  _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutReset' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutReset);
	return trampoline(hwo);
}

extern "C" MMRESULT WINAPI HookwaveOutRestart(  _In_ HWAVEOUT hwo)
{
	//LOG(DEBUG) << "Redirecting 'waveOutRestart' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutRestart);
	return trampoline(hwo);
}

extern "C" MMRESULT WINAPI HookwaveOutSetPitch(_In_ HWAVEOUT hwo,_In_ DWORD dwPitch)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetPitch' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetPitch);
	return trampoline(hwo,dwPitch);
}

extern "C" MMRESULT WINAPI HookwaveOutSetPlaybackRate( _In_ HWAVEOUT hwo, _In_ DWORD dwRate)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetPlaybackRate' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetPlaybackRate);
	return trampoline(hwo,dwRate);
}

extern "C" MMRESULT WINAPI HookwaveOutSetVolume( _In_opt_ HWAVEOUT hwo,_In_ DWORD dwVolume)
{
	//LOG(DEBUG) << "Redirecting 'waveOutSetVolume' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutSetVolume);
	return trampoline(hwo,dwVolume);
}

extern "C" MMRESULT WINAPI HookwaveOutUnprepareHeader( _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh, _In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutUnprepareHeader' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutUnprepareHeader);
	return trampoline(hwo,pwh,cbwh);
}

extern "C" MMRESULT WINAPI HookwaveOutWrite(  _In_ HWAVEOUT hwo, _Inout_updates_bytes_(cbwh) LPWAVEHDR pwh,_In_ UINT cbwh)
{
	//LOG(DEBUG) << "Redirecting 'waveOutWrite' to original library...";
	static const auto trampoline = reshade::hooks::call(waveOutWrite);
	return trampoline(hwo,pwh,cbwh);
}
