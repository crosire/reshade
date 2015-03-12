#include "Runtime.hpp"

#include <windows.h>

// -----------------------------------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);

			TCHAR modulePath[MAX_PATH], executablePath[MAX_PATH];
			GetModuleFileName(hModule, modulePath, MAX_PATH);
			GetModuleFileName(nullptr, executablePath, MAX_PATH);
			
			ReShade::Runtime::Startup(executablePath, modulePath);
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			ReShade::Runtime::Shutdown();
			break;
		}
	}

	return TRUE;
}