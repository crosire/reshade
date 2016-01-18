#include "Runtime.hpp"

#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);

			TCHAR executable_path[MAX_PATH], module_path[MAX_PATH];
			GetModuleFileName(hModule, module_path, MAX_PATH);
			GetModuleFileName(nullptr, executable_path, MAX_PATH);

			reshade::runtime::startup(executable_path, module_path);
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			reshade::runtime::shutdown();
			break;
		}
	}

	return TRUE;
}
