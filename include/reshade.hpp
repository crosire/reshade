/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <Windows.h>

extern HMODULE g_module_handle;

// Use the kernel32 variant of module enumeration functions so it can be safely called from DllMain
extern "C" BOOL WINAPI K32EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

namespace reshade
{
	/// <summary>
	/// Writes a message to the ReShade log.
	/// </summary>
	/// <param name="level">Severity level (1 = error, 2 = warning, 3 = info, 4 = debug).</param>
	/// <param name="message">A null-terminated message string.</param>
	inline void log_message(int level, const char *message)
	{
		static const auto func = reinterpret_cast<void(*)(int, const char *)>(
			GetProcAddress(g_module_handle, "ReShadeLogMessage"));
		func(level, message);
	}

	/// <summary>
	/// Registers this module as an add-on with ReShade. Call this in 'DllMain' during process attach, before any of the other API functions!
	/// </summary>
	inline bool register_addon(HMODULE module)
	{
		DWORD num = 0;
		HMODULE modules[1024];

		if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &num))
		{
			if (num > sizeof(modules))
				num = sizeof(modules);

			for (DWORD i = 0; i < num / sizeof(HMODULE); ++i)
			{
				const auto register_func = reinterpret_cast<bool(*)(HMODULE, uint32_t)>(GetProcAddress(modules[i], "ReShadeRegister"));
				if (register_func != nullptr)
				{
					g_module_handle = modules[i];

#ifdef IMGUI_VERSION
					// Check that the ReShade module supports the used imgui version
					const imgui_function_table *const table = reinterpret_cast<const imgui_function_table *(*)(unsigned int)>(GetProcAddress(modules[i], "ReShadeGetImGuiFunctionTable"))(IMGUI_VERSION_NUM);
					if (table == nullptr)
						return false;
					g_imgui_function_table = *table;
#endif

					return register_func(module, RESHADE_API_VERSION);
				}
			}
		}

		return false;
	}
	/// <summary>
	/// Unregisters this module. Call this in 'DllMain' during process detach, after any of the other API functions.
	/// </summary>
	inline void unregister_addon(HMODULE module)
	{
		if (g_module_handle == nullptr)
			return;

		const auto unregister_func = reinterpret_cast<bool(*)(HMODULE)>(GetProcAddress(g_module_handle, "ReShadeUnregister"));
		if (unregister_func == nullptr)
			return;

		unregister_func(module);
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
