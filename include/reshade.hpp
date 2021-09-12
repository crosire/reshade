/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <Windows.h>
#include <Psapi.h>

extern HMODULE g_module_handle;

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
		// Use the kernel32 variant of module enumeration functions so it can be safely called from 'DllMain'
		HMODULE modules[1024]; DWORD num = 0;
		if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &num))
		{
			if (num > sizeof(modules))
				num = sizeof(modules);

			for (DWORD i = 0; i < num / sizeof(HMODULE); ++i)
			{
				if (GetProcAddress(modules[i], "ReShadeRegisterAddon") &&
					GetProcAddress(modules[i], "ReShadeUnregisterAddon"))
				{
					g_module_handle = modules[i];
					break;
				}
			}
		}

		if (g_module_handle == nullptr)
			return false;

		// Check that the ReShade module supports the used API
		const auto func = reinterpret_cast<bool(*)(HMODULE, uint32_t)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterAddon"));
		if (!func(module, RESHADE_API_VERSION))
			return false;

#ifdef IMGUI_VERSION
		// Check that the ReShade module was built with imgui support
		const auto imgui_func = reinterpret_cast<const imgui_function_table *(*)(unsigned int)>(
			GetProcAddress(g_module_handle, "ReShadeGetImGuiFunctionTable"));
		if (imgui_func == nullptr)
			return false;

		// Check that the ReShade module supports the used imgui version
		const imgui_function_table *const imgui_table = imgui_func(IMGUI_VERSION_NUM);
		if (imgui_table == nullptr)
			return false;
		g_imgui_function_table = *imgui_table;
#endif

		return true;
	}
	/// <summary>
	/// Unregisters this module. Call this in 'DllMain' during process detach, after any of the other API functions.
	/// </summary>
	inline void unregister_addon(HMODULE module)
	{
		if (g_module_handle == nullptr)
			return;

		const auto func = reinterpret_cast<bool(*)(HMODULE)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterAddon"));
		func(module);
	}

	/// <summary>
	/// Registers a callback for the specified event (via template) with ReShade.
	/// <para>The callback function is then called whenever the application performs a task associated with this event (see also the <see cref="addon_event"/> enumeration).</para>
	/// </summary>
	template <reshade::addon_event ev>
	inline void register_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterEvent"));
		func(ev, static_cast<void *>(callback));
	}
	/// <summary>
	/// Unregisters a callback for the specified event (via template) that was previously registered via <see cref="register_event"/>.
	/// </summary>
	template <reshade::addon_event ev>
	inline void unregister_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(g_module_handle, "ReShadeUnregisterEvent"));
		func(ev, static_cast<void *>(callback));
	}

	/// <summary>
	/// Registers an overlay with ReShade.
	/// <para>The callback function is then called whenever the ReShade overlay is visible and allows adding imgui widgets for user interaction.</para>
	/// </summary>
	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime, void *imgui_context))
	{
		static const auto func = reinterpret_cast<decltype(&register_overlay)>(
			GetProcAddress(g_module_handle, "ReShadeRegisterOverlay"));
		func(title, callback);
	}
	/// <summary>
	/// Unregisters an overlay that was previously registered via <see cref="register_overlay"/>.
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
