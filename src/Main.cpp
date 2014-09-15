#include "Log.hpp"

#include <windows.h>

_INITIALIZE_EASYLOGGINGPP;

// -----------------------------------------------------------------------------------------------------

BOOL APIENTRY													DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			::DisableThreadLibraryCalls(hModule);
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			break;
		}
	}

	return TRUE;
}