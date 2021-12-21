/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "addon.hpp"
#include "runtime.hpp"
#include "dll_log.hpp"
#include "ini_file.hpp"

typedef struct HINSTANCE__ *HMODULE;

extern reshade::addon::info *find_addon(HMODULE module);

extern "C" __declspec(dllexport) void ReShadeLogMessage(HMODULE module, int level, const char *message)
{
	std::string prefix;
	if (module != nullptr)
	{
		reshade::addon::info *const info = find_addon(module);
		if (info != nullptr)
			prefix = "[" + info->name + "] ";
	}

	reshade::log::message(static_cast<reshade::log::level>(level)) << prefix << message;
}

extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, char *value, size_t *length)
{
	if (key == nullptr || length == nullptr)
		return false;

	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	std::string value_string;
	if (!config.get(section, key, value_string))
		return false;

	if (value != nullptr && *length != 0)
		value[value_string.copy(value, *length - 1)] = '\0';

	*length = value_string.size();
	return true;
}
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(HMODULE, reshade::api::effect_runtime *runtime, const char *section, const char *key, const char *value)
{
	ini_file &config = (runtime != nullptr) ? ini_file::load_cache(static_cast<reshade::runtime *>(runtime)->get_config_path()) : reshade::global_config();

	config.set(section, key, std::string(value));

	config.save();
}

#if RESHADE_GUI

#include "reshade.hpp"
#include "imgui_function_table.hpp"

extern imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == IMGUI_VERSION_NUM)
		return &g_imgui_function_table;
	return nullptr;
}

#endif

#endif
