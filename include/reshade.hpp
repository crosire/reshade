/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <charconv>
#include <Windows.h>

#define RESHADE_API_VERSION 4

 // Use the kernel32 variant of module enumeration functions so it can be safely called from 'DllMain'
extern "C" BOOL WINAPI K32EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

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
	/// Gets a value from one of ReShade's config files.
	/// </summary>
	/// <param name="runtime">Optional effect runtime to use the config file from, or <see langword="nullptr"/> to use the global config file.</param>
	/// <param name="section">Name of the config section.</param>
	/// <param name="key">Name of the config value.</param>
	/// <param name="value">Pointer to a string buffer that is filled with the config value.</param>
	/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
	/// <returns><see langword="true"/> if the specified config value exists, <see cref="false"/> otherwise.</returns>
	inline bool config_get_value(api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *length)
	{
		static const auto func = reinterpret_cast<bool(*)(HMODULE, api::effect_runtime *, const char *, const char *, char *, size_t *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeGetConfigValue"));
		return func(internal::get_current_module_handle(), runtime, section, key, value, length);
	}
	template <typename T>
	inline bool config_get_value(api::effect_runtime *runtime, const char *section, const char *key, T &value)
	{
		char value_string[32] = ""; size_t value_length = sizeof(value_string) - 1;
		if (!config_get_value(runtime, section, key, value_string, &value_length))
			return false;
		return std::from_chars(value_string, value_string + value_length, value).ec == std::errc {};
	}
	template <>
	inline bool config_get_value<bool>(api::effect_runtime *runtime, const char *section, const char *key, bool &value)
	{
		int value_int = 0;
		if (!config_get_value<int>(runtime, section, key, value_int))
			return false;
		value = value_int != 0;
		return true;
	}

	/// <summary>
	/// Sets and saves a value in one of ReShade's config files.
	/// </summary>
	/// <param name="runtime">Optional effect runtime to use the config file from, or <see langword="nullptr"/> to use the global config file.</param>
	/// <param name="section">Name of the config section.</param>
	/// <param name="key">Name of the config value.</param>
	/// <param name="value">Config value to set.</param>
	inline void config_set_value(api::effect_runtime *runtime, const char *section, const char *key, const char *value)
	{
		static const auto func = reinterpret_cast<void(*)(HMODULE, api::effect_runtime *, const char *, const char *, const char *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeSetConfigValue"));
		func(internal::get_current_module_handle(), runtime, section, key, value);
	}
	template <typename T>
	inline void config_set_value(api::effect_runtime *runtime, const char *section, const char *key, const T &value)
	{
		char value_string[32] = "";
		std::to_chars(value_string, value_string + sizeof(value_string) - 1, value);
		config_set_value(runtime, section, key, static_cast<const char *>(value_string));
	}
	template <>
	inline void config_set_value<bool>(api::effect_runtime *runtime, const char *section, const char *key, const bool &value)
	{
		config_set_value<int>(runtime, section, key, value ? 1 : 0);
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
		const auto imgui_func = reinterpret_cast<const imgui_function_table *(*)(uint32_t)>(
			GetProcAddress(reshade_module, "ReShadeGetImGuiFunctionTable"));
		if (imgui_func == nullptr || !(imgui_function_table_instance() = imgui_func(IMGUI_VERSION_NUM)))
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
	/// <para>The callback function is then called when the overlay is visible and allows adding Dear ImGui widgets for user interaction.</para>
	/// </summary>
	/// <param name="title">Null-terminated title string, or <see langword="nullptr"/> to register a settings overlay for this add-on.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeRegisterOverlay"));
		if (func != nullptr)
			func(title, callback);
	}
	/// <summary>
	/// Unregisters an overlay that was previously registered via <see cref="register_overlay"/>.
	/// </summary>
	/// <param name="title">Null-terminated title string.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void unregister_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeUnregisterOverlay"));
		if (func != nullptr)
			func(title, callback);
	}
}
