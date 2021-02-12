/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#ifdef RESHADE_ADDON_GUI
#include "reshade_api_imgui.hpp"
#endif
#include <Windows.h>

#ifdef RESHADE_ADDON_MAIN
HMODULE g_module_handle = nullptr;
#ifdef RESHADE_ADDON_GUI
imgui_function_table g_imgui_function_table = {};
#endif
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
#ifdef RESHADE_ADDON_GUI
					g_imgui_function_table = *reinterpret_cast<imgui_function_table *(*)()>(GetProcAddress(modules[i], "ReShadeGetImGuiFunctionTable"))();
#endif
					return true;
				}
			}
		}

		return false;
	}

	template <reshade::addon_event ev>
	inline void register_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterEvent"));
		func(ev, static_cast<void *>(callback));
	}
	template <reshade::addon_event ev>
	inline void unregister_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterEvent"));
		func(ev, static_cast<void *>(callback));
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
