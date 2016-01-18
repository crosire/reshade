#include "runtime.hpp"

#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);

			WCHAR exe_path[MAX_PATH], module_path[MAX_PATH];
			GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
			GetModuleFileNameW(hModule, module_path, MAX_PATH);

			reshade::runtime::startup(exe_path, module_path);
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
