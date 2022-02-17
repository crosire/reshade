/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: 0BSD
 */

#include <reshade.hpp>

extern "C" __declspec(dllexport) const char *NAME = "Basic Add-on Template";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Simple project template to get started with.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
