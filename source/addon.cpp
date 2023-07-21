/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if defined(RESHADE_API_LIBRARY_EXPORT)

#include "reshade.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"

void ReShadeLogMessage(HMODULE module, int level, const char *message)
{
	std::string prefix;
#if RESHADE_ADDON
	if (module != nullptr)
	{
		reshade::addon_info *const info = reshade::find_addon(module);
		if (info != nullptr)
			prefix = "[" + info->name + "] ";
	}
#else
	UNREFERENCED_PARAMETER(module);
#endif

	reshade::log::message(static_cast<reshade::log::level>(level)) << prefix << message;
}

void ReShadeGetBasePath(char *path, size_t *size)
{
	if (size == nullptr)
		return;

	const std::string path_string = g_reshade_base_path.u8string();

	if (path == nullptr)
	{
		*size = path_string.size() + 1;
	}
	else if (*size != 0)
	{
		*size = path_string.copy(path, *size - 1);
		path[*size++] = '\0';
	}
}

bool ReShadeGetConfigValue(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *size)
{
	if (size == nullptr)
		return false;

	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();
	std::string value_string;

	if (!config.get(section_string, key_string, value_string))
	{
		*size = 0;
		return false;
	}

	if (value == nullptr)
	{
		*size = value_string.size() + 1;
	}
	else if (*size != 0)
	{
		*size = value_string.copy(value, *size - 1);
		value[*size++] = '\0';
	}

	return true;
}
void ReShadeSetConfigValue(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value)
{
	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();
	const std::string value_string = value != nullptr ? value : std::string();

	config.set(section_string, key_string, value_string);
}

#if RESHADE_ADDON && RESHADE_GUI

#include <imgui.h>
#include "imgui_function_table_18971.hpp"
#include "imgui_function_table_18600.hpp"

extern imgui_function_table_18971 g_imgui_function_table_18971;
extern imgui_function_table_18600 g_imgui_function_table_18600;

extern "C" __declspec(dllexport) const void *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == 18971)
		return &g_imgui_function_table_18971;
	if (version == 18600)
		return &g_imgui_function_table_18600;

	LOG(ERROR) << "Failed to retrieve ImGui function table, because the requested ImGui version (" << version << ") is not supported.";
	return nullptr;
}

#endif

#endif
