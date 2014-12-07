#include "Log.hpp"
#include "Runtime.hpp"

#include <windows.h>

_INITIALIZE_EASYLOGGINGPP;

// -----------------------------------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(lpvReserved);

	#pragma region Anti Debugging
#ifndef _DEBUG
	OutputDebugString(TEXT("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"));

	char ollydbg[] = "effsnhm"; for (int i = 0; i < 7; ++i) ollydbg[i] ^= 42;
	char windbg[] = "}CDnHMlXKGOiFKYY"; for (int i = 0; i < 16; ++i) windbg[i] ^= 42;

	if (FindWindowA(ollydbg, nullptr) != nullptr || FindWindowA(windbg, nullptr) != nullptr)
	{
		ExitProcess(0);
	}
#endif
	#pragma endregion

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