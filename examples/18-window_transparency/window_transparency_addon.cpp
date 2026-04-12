/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <reshade.hpp>
#include <dwmapi.h>

static void on_init_effect_runtime(reshade::api::effect_runtime *runtime)
{
	const auto hwnd = static_cast<HWND>(runtime->get_hwnd());

	// GTK 3 enables transparency for windows, which messes with effects that do not return an alpha value, so disable that again
	if (hwnd != nullptr)
	{
		DWM_BLURBEHIND blur_behind = {};
		blur_behind.dwFlags = DWM_BB_ENABLE;
		blur_behind.fEnable = FALSE;
		DwmEnableBlurBehindWindow(GetAncestor(hwnd, GA_ROOT), &blur_behind);
	}
}

extern "C" __declspec(dllexport) const char *NAME = "Fix Window Transparency";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Disables transparency for windows, since it messes with effects that do not return an alpha value.";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
			return FALSE;
		reshade::register_event<reshade::addon_event::init_effect_runtime>(on_init_effect_runtime);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
