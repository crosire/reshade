/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <Windows.h>

#define RESHADE_API_VERSION 1

extern HMODULE g_module_handle;

// Use the kernel32 variant of module enumeration functions so it can be safely called from DllMain
extern "C" BOOL WINAPI K32EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

namespace reshade
{
	/// <summary>
	/// Finds the ReShade module in the current process and remembers it to resolve subsequent calls to 'register_event' or 'register_overlay'.
	/// </summary>
	inline bool init_addon()
	{
		DWORD num = 0;
		HMODULE modules[1024];

		if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &num))
		{
			if (num > sizeof(modules))
				num = sizeof(modules);

			for (DWORD i = 0; i < num / sizeof(HMODULE); ++i)
			{
				const auto api_version = reinterpret_cast<const uint32_t *>(GetProcAddress(modules[i], "ReShadeAPI"));
				if (api_version != nullptr)
				{
#ifdef IMGUI_VERSION
					g_imgui_function_table = *reinterpret_cast<const imgui_function_table *(*)()>(GetProcAddress(modules[i], "ReShadeGetImGuiFunctionTable"))();
#endif
					g_module_handle = modules[i];

					// Check that the ReShade module supports the requested API version
					return *api_version <= RESHADE_API_VERSION;
				}
			}
		}

		return false;
	}

	/// <summary>
	/// Registers a callback for the specified event (via template) with ReShade.
	/// This callback is then called whenever the application performs a task associated with this event (see also the <see cref="addon_event"/> enumeration).
	/// </summary>
	template <reshade::addon_event ev>
	inline void register_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterEvent"));
		func(ev, static_cast<void *>(callback));
	}
	/// <summary>
	/// Unregisters a callback for the specified event (via template) that was previously registered via 'register_event'.
	/// </summary>
	template <reshade::addon_event ev>
	inline void unregister_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterEvent"));
		func(ev, static_cast<void *>(callback));
	}

	/// <summary>
	/// Registers an overlay with ReShade. The callback is called whenever the ReShade overlay is visible and allows adding ImGui widgets for user interaction.
	/// </summary>
	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
	{
		static const auto func = reinterpret_cast<decltype(&register_overlay)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterOverlay"));
		func(title, callback);
	}
	/// <summary>
	/// Unregisters an overlay that was previously registered via 'register_overlay'.
	/// </summary>
	inline void unregister_overlay(const char *title)
	{
		static const auto func = reinterpret_cast<decltype(&unregister_overlay)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterOverlay"));
		func(title);
	}
}

// Define 'RESHADE_ADDON_IMPL' before including this header in exactly one source file to create the implementation
#ifdef RESHADE_ADDON_IMPL
HMODULE g_module_handle = nullptr;
#ifdef IMGUI_VERSION
imgui_function_table g_imgui_function_table = {};
#endif
#endif
