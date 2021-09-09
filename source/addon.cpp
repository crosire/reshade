/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_ADDON

#include "addon.hpp"
#include "dll_log.hpp"

extern "C" __declspec(dllexport) void ReShadeLogMessage(int level, const char *message)
{
	reshade::log::message(static_cast<reshade::log::level>(level)) << "Add-on | " << message;
}

#if RESHADE_GUI
struct imgui_function_table;
extern imgui_function_table g_imgui_function_table;

extern "C" __declspec(dllexport) const imgui_function_table *ReShadeGetImGuiFunctionTable()
{
	return &g_imgui_function_table;
}
#endif

#endif
