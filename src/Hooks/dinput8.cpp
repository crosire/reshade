#include "Log.hpp"
#include "HookManager.hpp"

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>

// -----------------------------------------------------------------------------------------------------

EXPORT HRESULT WINAPI											DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	return ReHook::Call(&DirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}