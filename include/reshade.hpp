/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <Windows.h>
#include <Psapi.h>

#define RESHADE_API_VERSION 1

namespace reshade
{
	namespace internal
	{
		/// <summary>
		/// Gets the handle to the ReShade module.
		/// </summary>
		inline HMODULE &get_reshade_module_handle()
		{
			static HMODULE handle = nullptr;
			if (handle == nullptr)
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
							handle = modules[i];
							break;
						}
					}
				}
			}
			return handle;
		}

		/// <summary>
		/// Gets the handle to the current add-on module.
		/// </summary>
		inline HMODULE &get_current_module_handle()
		{
			static HMODULE handle = nullptr;
			return handle;
		}

#if defined(IMGUI_VERSION_NUM)
		/// <summary>
		/// Gets a pointer to the Dear ImGui function table.
		/// </summary>
		inline const imgui_function_table *get_imgui_function_table()
		{
			static const auto func = reinterpret_cast<const imgui_function_table *(*)(uint32_t)>(
				GetProcAddress(get_reshade_module_handle(), "ReShadeGetImGuiFunctionTable"));
			if (func != nullptr)
				return func(IMGUI_VERSION_NUM);
			return nullptr;
		}
#endif
	}

	/// <summary>
	/// Writes a message to ReShade's log.
	/// </summary>
	/// <param name="level">Severity level (1 = error, 2 = warning, 3 = info, 4 = debug).</param>
	/// <param name="message">A null-terminated message string.</param>
	inline void log_message(int level, const char *message)
	{
		static const auto func = reinterpret_cast<void(*)(HMODULE, int, const char *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeLogMessage"));
		func(internal::get_current_module_handle(), level, message);
	}

	/// <summary>
	/// Registers this module as an add-on with ReShade.
	/// Call this in 'DllMain' during process attach, before any of the other API functions!
	/// </summary>
	/// <param name="module">Handle of the current add-on module.</param>
	inline bool register_addon(HMODULE module)
	{
		internal::get_current_module_handle() = module;

		const HMODULE reshade_module = internal::get_reshade_module_handle();
		if (reshade_module == nullptr)
			return false;

		// Check that the ReShade module supports the used API
		const auto func = reinterpret_cast<bool(*)(HMODULE, uint32_t)>(
			GetProcAddress(reshade_module, "ReShadeRegisterAddon"));
		if (!func(module, RESHADE_API_VERSION))
			return false;

#if defined(IMGUI_VERSION_NUM)
		// Check that the ReShade module was built with Dear ImGui support and supports the used version
		if (!internal::get_imgui_function_table())
			return false;
#endif

		return true;
	}
	/// <summary>
	/// Unregisters this module. Call this in 'DllMain' during process detach, after any of the other API functions.
	/// </summary>
	/// <param name="module">Handle of the current add-on module.</param>
	inline void unregister_addon(HMODULE module)
	{
		const HMODULE reshade_module = internal::get_reshade_module_handle();
		if (reshade_module == nullptr)
			return;

		const auto func = reinterpret_cast<bool(*)(HMODULE)>(
			GetProcAddress(reshade_module, "ReShadeUnregisterAddon"));
		func(module);
	}

	/// <summary>
	/// Registers a callback for the specified event (via template) with ReShade.
	/// <para>The callback function is then called whenever the application performs a task associated with this event (see also the <see cref="addon_event"/> enumeration).</para>
	/// </summary>
	/// <param name="callback">Pointer to the callback function.</param>
	template <reshade::addon_event ev>
	inline void register_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeRegisterEvent"));
		func(ev, static_cast<void *>(callback));
	}
	/// <summary>
	/// Unregisters a callback for the specified event (via template) that was previously registered via <see cref="register_event"/>.
	/// </summary>
	/// <param name="callback">Pointer to the callback function.</param>
	template <reshade::addon_event ev>
	inline void unregister_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeUnregisterEvent"));
		func(ev, static_cast<void *>(callback));
	}

	/// <summary>
	/// Registers an overlay with ReShade.
	/// <para>The callback function is then called whenever the ReShade overlay is visible and allows adding Dear ImGui widgets for user interaction.</para>
	/// </summary>
	/// <param name="title">A null-terminated title string, or <see langword="nullptr"/> to register a settings overlay for this add-on.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeRegisterOverlay"));
		func(title, callback);
	}
	/// <summary>
	/// Unregisters an overlay that was previously registered via <see cref="register_overlay"/>.
	/// </summary>
	/// <param name="title">A null-terminated title string.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void unregister_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeUnregisterOverlay"));
		func(title, callback);
	}
}
