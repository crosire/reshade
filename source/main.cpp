#include "runtime.hpp"
#include "filesystem.hpp"

#include <Windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls(hModule);
			reshade::runtime::startup(reshade::filesystem::get_module_path(nullptr), reshade::filesystem::get_module_path(hModule));
			break;
		case DLL_PROCESS_DETACH:
			reshade::runtime::shutdown();
			break;
	}

	return TRUE;
}
