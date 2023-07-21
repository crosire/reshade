/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_events.hpp"
#include "reshade_overlay.hpp"
#include <charconv>
#include <Windows.h>

// Current version of the ReShade API
#define RESHADE_API_VERSION 8

// Optionally import ReShade API functions when 'RESHADE_API_LIBRARY' is defined instead of using header-only mode
#if defined(RESHADE_API_LIBRARY) || defined(RESHADE_API_LIBRARY_EXPORT)

#ifdef RESHADE_API_LIBRARY_EXPORT
	#define RESHADE_API_LIBRARY_DECLSPEC extern "C" __declspec(dllexport)
#else
	#define RESHADE_API_LIBRARY_DECLSPEC extern "C" __declspec(dllimport)
#endif

RESHADE_API_LIBRARY_DECLSPEC void ReShadeLogMessage(HMODULE module, int level, const char *message);

RESHADE_API_LIBRARY_DECLSPEC void ReShadeGetBasePath(char *path, size_t *path_size);

RESHADE_API_LIBRARY_DECLSPEC bool ReShadeGetConfigValue(HMODULE module, reshade::api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *value_size);
RESHADE_API_LIBRARY_DECLSPEC void ReShadeSetConfigValue(HMODULE module, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value);

RESHADE_API_LIBRARY_DECLSPEC bool ReShadeRegisterAddon(HMODULE module, uint32_t api_version);
RESHADE_API_LIBRARY_DECLSPEC void ReShadeUnregisterAddon(HMODULE module);

RESHADE_API_LIBRARY_DECLSPEC void ReShadeRegisterEvent(reshade::addon_event ev, void *callback);
RESHADE_API_LIBRARY_DECLSPEC void ReShadeUnregisterEvent(reshade::addon_event ev, void *callback);

RESHADE_API_LIBRARY_DECLSPEC void ReShadeRegisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));
RESHADE_API_LIBRARY_DECLSPEC void ReShadeUnregisterOverlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime));

#else

// Use the kernel32 variant of module enumeration functions so it can be safely called from 'DllMain'
extern "C" BOOL WINAPI K32EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

namespace reshade { namespace internal
{
	/// <summary>
	/// Gets the handle to the ReShade module.
	/// </summary>
	inline HMODULE get_reshade_module_handle(HMODULE reshade_module = nullptr)
	{
		static HMODULE handle = reshade_module;
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
	inline HMODULE get_current_module_handle(HMODULE addon_module = nullptr)
	{
		static HMODULE handle = addon_module;
		return handle;
	}
} }

#endif

namespace reshade
{
	/// <summary>
	/// Available log severity levels.
	/// </summary>
	enum class log_level
	{
		error = 1,
		warning = 2,
		info = 3,
		debug = 4
	};

	/// <summary>
	/// Writes a message to ReShade's log.
	/// </summary>
	/// <param name="level">Severity level.</param>
	/// <param name="message">A null-terminated message string.</param>
	inline void log_message(log_level level, const char *message)
	{
#if defined(RESHADE_API_LIBRARY) || defined(RESHADE_API_LIBRARY_EXPORT)
		ReShadeLogMessage(nullptr, static_cast<int>(level), message);
#else
		static const auto func = reinterpret_cast<void(*)(HMODULE, int, const char *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeLogMessage"));
		func(internal::get_current_module_handle(), static_cast<int>(level), message);
#endif
	}

	/// <summary>
	/// Gets the file path ReShade uses to resolve relative paths.
	/// </summary>
	/// <param name="path">Pointer to a string buffer that is filled with the file path to the preset, or <see langword="nullptr"/> to query the necessary size.</param>
	/// <param name="path_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
	inline void get_reshade_base_path(char *path, size_t *path_size)
	{
#if defined(RESHADE_API_LIBRARY) || defined(RESHADE_API_LIBRARY_EXPORT)
		ReShadeGetBasePath(path, path_size);
#else
		static const auto func = reinterpret_cast<bool(*)(char *, size_t *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeGetBasePath"));
		func(path, path_size);
#endif
	}

	/// <summary>
	/// Gets a value from one of ReShade's config files.
	/// </summary>
	/// <param name="runtime">Optional effect runtime to use the config file from, or <see langword="nullptr"/> to use the global config file.</param>
	/// <param name="section">Name of the config section.</param>
	/// <param name="key">Name of the config value.</param>
	/// <param name="value">Pointer to a string buffer that is filled with the config value, or <see langword="nullptr"/> to query the necessary size.</param>
	/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
	/// <returns><see langword="true"/> if the specified config value exists, <see cref="false"/> otherwise.</returns>
	inline bool get_config_value(api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *value_size)
	{
#if defined(RESHADE_API_LIBRARY) || defined(RESHADE_API_LIBRARY_EXPORT)
		return ReShadeGetConfigValue(nullptr, runtime, section, key, value, value_size);
#else
		static const auto func = reinterpret_cast<bool(*)(HMODULE, api::effect_runtime *, const char *, const char *, char *, size_t *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeGetConfigValue"));
		return func(internal::get_current_module_handle(), runtime, section, key, value, value_size);
#endif
	}
#if _HAS_CXX17
	template <typename T>
	inline bool get_config_value(api::effect_runtime *runtime, const char *section, const char *key, T &value)
	{
		char value_string[32]; size_t value_length = sizeof(value_string) - 1;
		if (!get_config_value(runtime, section, key, value_string, &value_length))
			return false;
		return std::from_chars(value_string, value_string + value_length, value).ec == std::errc {};
	}
	template <>
	inline bool get_config_value<bool>(api::effect_runtime *runtime, const char *section, const char *key, bool &value)
	{
		int value_int = 0;
		if (!get_config_value<int>(runtime, section, key, value_int))
			return false;
		value = (value_int != 0);
		return true;
	}
#endif

	/// <summary>
	/// Sets and saves a value in one of ReShade's config files.
	/// </summary>
	/// <param name="runtime">Optional effect runtime to use the config file from, or <see langword="nullptr"/> to use the global config file.</param>
	/// <param name="section">Name of the config section.</param>
	/// <param name="key">Name of the config value.</param>
	/// <param name="value">Config value to set.</param>
	inline void set_config_value(api::effect_runtime *runtime, const char *section, const char *key, const char *value)
	{
#if defined(RESHADE_API_LIBRARY) || defined(RESHADE_API_LIBRARY_EXPORT)
		ReShadeSetConfigValue(nullptr, runtime, section, key, value);
#else
		static const auto func = reinterpret_cast<void(*)(HMODULE, api::effect_runtime *, const char *, const char *, const char *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeSetConfigValue"));
		func(internal::get_current_module_handle(), runtime, section, key, value);
#endif
	}
#if _HAS_CXX17
	template <typename T>
	inline void set_config_value(api::effect_runtime *runtime, const char *section, const char *key, const T &value)
	{
		char value_string[32] = "";
		std::to_chars(value_string, value_string + sizeof(value_string) - 1, value);
		set_config_value(runtime, section, key, static_cast<const char *>(value_string));
	}
	template <>
	inline void set_config_value<bool>(api::effect_runtime *runtime, const char *section, const char *key, const bool &value)
	{
		set_config_value<int>(runtime, section, key, value ? 1 : 0);
	}
#endif

	/// <summary>
	/// Registers this module as an add-on with ReShade.
	/// Call this in 'AddonInit' or 'DllMain' during process attach, before any of the other API functions!
	/// </summary>
	/// <param name="addon_module">Handle of the current module.</param>
	/// <param name="reshade_module">Handle of the ReShade module in the process, or <see langword="nullptr"/> to find it automatically.</param>
	inline bool register_addon(HMODULE addon_module, [[maybe_unused]] HMODULE reshade_module = nullptr)
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON)
		return ReShadeRegisterAddon(addon_module, RESHADE_API_VERSION);
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		addon_module = internal::get_current_module_handle(addon_module);
		reshade_module = internal::get_reshade_module_handle(reshade_module);

		if (reshade_module == nullptr)
			return false;

		const auto func = reinterpret_cast<bool(*)(HMODULE, uint32_t)>(
			GetProcAddress(reshade_module, "ReShadeRegisterAddon"));
		// Check that the ReShade module supports the used API
		if (func == nullptr || !func(addon_module, RESHADE_API_VERSION))
			return false;

#if defined(IMGUI_VERSION_NUM)
		const auto imgui_func = reinterpret_cast<const imgui_function_table *(*)(uint32_t)>(
			GetProcAddress(reshade_module, "ReShadeGetImGuiFunctionTable"));
		// Check that the ReShade module was built with Dear ImGui support and supports the used version
		if (imgui_func == nullptr || !(imgui_function_table_instance() = imgui_func(IMGUI_VERSION_NUM)))
			return false;
#endif

		return true;
#else
		UNREFERENCED_PARAMETER(addon_module);
		UNREFERENCED_PARAMETER(reshade_module);
		return false;
#endif
	}
	/// <summary>
	/// Unregisters this module.
	/// Call this in 'AddonUninit' or 'DllMain' during process detach, after any of the other API functions.
	/// </summary>
	/// <param name="addon_module">Handle of the current module.</param>
	/// <param name="reshade_module">Handle of the ReShade module in the process, or <see langword="nullptr"/> to find it automatically.</param>
	inline void unregister_addon(HMODULE addon_module, [[maybe_unused]] HMODULE reshade_module = nullptr)
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON)
		ReShadeUnregisterAddon(addon_module);
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		addon_module = internal::get_current_module_handle(addon_module);
		reshade_module = internal::get_reshade_module_handle(reshade_module);

		if (reshade_module == nullptr)
			return;

		const auto func = reinterpret_cast<bool(*)(HMODULE)>(
			GetProcAddress(reshade_module, "ReShadeUnregisterAddon"));
		if (func != nullptr)
			func(addon_module);
#else
		UNREFERENCED_PARAMETER(addon_module);
		UNREFERENCED_PARAMETER(reshade_module);
#endif
	}

	/// <summary>
	/// Registers a callback for the specified event (via template) with ReShade.
	/// <para>The callback function is then called whenever the application performs a task associated with this event (see also the <see cref="addon_event"/> enumeration).</para>
	/// </summary>
	/// <param name="callback">Pointer to the callback function.</param>
	template <reshade::addon_event ev>
	inline void register_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON)
		ReShadeRegisterEvent(ev, static_cast<void *>(callback));
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeRegisterEvent"));
		if (func != nullptr)
			func(ev, static_cast<void *>(callback));
#else
		UNREFERENCED_PARAMETER(callback);
#endif
	}
	/// <summary>
	/// Unregisters a callback for the specified event (via template) that was previously registered via <see cref="register_event"/>.
	/// </summary>
	/// <param name="callback">Pointer to the callback function.</param>
	template <reshade::addon_event ev>
	inline void unregister_event(typename reshade::addon_event_traits<ev>::decl callback)
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON)
		ReShadeUnregisterEvent(ev, static_cast<void *>(callback));
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		static const auto func = reinterpret_cast<void(*)(reshade::addon_event, void *)>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeUnregisterEvent"));
		if (func != nullptr)
			func(ev, static_cast<void *>(callback));
#else
		UNREFERENCED_PARAMETER(callback);
#endif
	}

	/// <summary>
	/// Registers an overlay with ReShade.
	/// <para>The callback function is then called when the overlay is visible and allows adding Dear ImGui widgets for user interaction.</para>
	/// </summary>
	/// <param name="title">Null-terminated title string, or <see langword="nullptr"/> to register a settings overlay for this add-on.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void register_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI)
		ReShadeRegisterOverlay(title, callback);
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeRegisterOverlay"));
		if (func != nullptr)
			func(title, callback);
#else
		UNREFERENCED_PARAMETER(title);
		UNREFERENCED_PARAMETER(callback);
#endif
	}
	/// <summary>
	/// Unregisters an overlay that was previously registered via <see cref="register_overlay"/>.
	/// </summary>
	/// <param name="title">Null-terminated title string.</param>
	/// <param name="callback">Pointer to the callback function.</param>
	inline void unregister_overlay(const char *title, void(*callback)(reshade::api::effect_runtime *runtime))
	{
#if defined(RESHADE_API_LIBRARY) || (defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI)
		ReShadeUnregisterOverlay(title, callback);
#elif !defined(RESHADE_API_LIBRARY_EXPORT)
		static const auto func = reinterpret_cast<void(*)(const char *, void(*)(reshade::api::effect_runtime *))>(
			GetProcAddress(internal::get_reshade_module_handle(), "ReShadeUnregisterOverlay"));
		if (func != nullptr)
			func(title, callback);
#else
		UNREFERENCED_PARAMETER(title);
		UNREFERENCED_PARAMETER(callback);
#endif
	}
}
