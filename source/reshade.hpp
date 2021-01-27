/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"

#ifdef _WINDOWS_
#ifdef RESHADE_ADDON_MAIN
HMODULE g_module_handle = nullptr;
#else
extern HMODULE g_module_handle;
#endif
extern "C" BOOL __stdcall K32EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

namespace reshade
{
	inline bool init_addon()
	{
		DWORD num = 0;
		HMODULE modules[1024];

		if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &num))
		{
			num /= sizeof(HMODULE);
			if (num > 1024)
				num = 1024;

			for (DWORD i = 0; i < num; ++i)
			{
				if (GetProcAddress(modules[i], "ReShadeVersion") != nullptr)
				{
					g_module_handle = modules[i];
					return true;
				}
			}
		}

		return false;
	}

	inline void register_event(reshade::addon_event ev, void *callback)
	{
		static const auto func = reinterpret_cast<decltype(&register_event)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterEvent"));
		func(ev, callback);
	}
	inline void unregister_event(reshade::addon_event ev, void *callback)
	{
		static const auto func = reinterpret_cast<decltype(&unregister_event)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterEvent"));
		func(ev, callback);
	}

	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
	{
		static const auto func = reinterpret_cast<decltype(&register_overlay)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterOverlay"));
		func(title, callback);
	}
	inline void unregister_overlay(const char *title)
	{
		static const auto func = reinterpret_cast<decltype(&unregister_overlay)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterOverlay"));
		func(title);
	}
}
#endif
