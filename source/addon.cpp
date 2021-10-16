/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "addon.hpp"
#include "dll_log.hpp"

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

#if RESHADE_GUI
#include "reshade.hpp"
#include "imgui_function_table.hpp"

extern imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == RESHADE_API_VERSION_IMGUI)
		return &g_imgui_function_table;
	return nullptr;
}
#endif

#endif
