#include <windows.h>
#include <easylogging++.h>

_INITIALIZE_EASYLOGGINGPP;

// -----------------------------------------------------------------------------------------------------

BOOL APIENTRY													DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			break;
		}
	}

	return TRUE;
}