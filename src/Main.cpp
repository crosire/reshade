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

			TCHAR modulePath[MAX_PATH], executablePath[MAX_PATH], systemPath[MAX_PATH];
			::GetModuleFileName(hModule, modulePath, MAX_PATH);
			::GetModuleFileName(nullptr, executablePath, MAX_PATH);
			::GetSystemDirectory(systemPath, MAX_PATH);
			
			ReShade::Manager::Initialize(executablePath, modulePath, systemPath);
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