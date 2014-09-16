#include "Log.hpp"
#include "Manager.hpp"

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

			TCHAR pathModule[MAX_PATH], pathExecutable[MAX_PATH], pathSystem[MAX_PATH];
			::GetModuleFileName(hModule, pathModule, MAX_PATH);
			::GetModuleFileName(nullptr, pathExecutable, MAX_PATH);
			::GetSystemDirectory(pathSystem, MAX_PATH);
			
			ReShade::Manager::Initialize(pathExecutable, pathModule, pathSystem);
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			ReShade::Manager::Exit();
			break;
		}
	}

	return TRUE;
}