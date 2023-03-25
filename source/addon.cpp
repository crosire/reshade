/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON

#include "runtime.hpp"
#include "addon_manager.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"

extern "C" __declspec(dllexport) void ReShadeLogMessage(void *module, int level, const char *message)
{
	std::string prefix;
	if (module != nullptr)
	{
		reshade::addon_info *const info = reshade::find_addon(module);
		if (info != nullptr)
			prefix = "[" + info->name + "] ";
	}

	reshade::log::message(static_cast<reshade::log::level>(level)) << prefix << message;
}

extern "C" __declspec(dllexport) void ReShadeGetBasePath(void *, char *path, size_t *length)
{
	if (length == nullptr)
		return;

	const std::string path_string = g_reshade_base_path.u8string();

	if (path != nullptr)
		*length = path_string.copy(path, *length);
	else
		*length = path_string.size();
}

extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void *, reshade::api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *length)
{
	if (length == nullptr)
		return false;

	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();
	std::string value_string;

	if (!config.get(section_string, key_string, value_string))
	{
		*length = 0;
		return false;
	}

	if (value != nullptr)
		*length = value_string.copy(value, *length);
	else
		*length = value_string.size();

	return true;
}
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void *, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value)
{
	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	const std::string section_string = section != nullptr ? section : std::string();
	const std::string key_string = key != nullptr ? key : std::string();
	const std::string value_string = value != nullptr ? value : std::string();

	config.set(section_string, key_string, value_string);
}

#if RESHADE_GUI

#include <imgui.h>
#include "reshade_overlay.hpp"

extern imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == IMGUI_VERSION_NUM)
		return &g_imgui_function_table;

	LOG(ERROR) << "Failed to retrieve ImGui function table, because the requested ImGui version (" << version << ") is not supported (expected " << IMGUI_VERSION_NUM << ").";
	return nullptr;
}

#endif

#endif
