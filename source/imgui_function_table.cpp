/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON && RESHADE_GUI

#include "dll_log.hpp"
#include "imgui_function_table_19222.hpp"
#include "imgui_function_table_19191.hpp"
#include "imgui_function_table_19180.hpp"
#include "imgui_function_table_19040.hpp"
#include "imgui_function_table_19000.hpp"
#include "imgui_function_table_18971.hpp"
#include "imgui_function_table_18600.hpp"

extern const imgui_function_table_19222 init_imgui_function_table_19222();
extern const imgui_function_table_19191 init_imgui_function_table_19191();
extern const imgui_function_table_19180 init_imgui_function_table_19180();
extern const imgui_function_table_19040 init_imgui_function_table_19040();
extern const imgui_function_table_19000 init_imgui_function_table_19000();
extern const imgui_function_table_18971 init_imgui_function_table_18971();
extern const imgui_function_table_18600 init_imgui_function_table_18600();

// Force initialization order (has to happen in a single translation unit to be well defined by declaration order) from newest to oldest, so that older function tables can reference newer ones
const imgui_function_table_19222 g_imgui_function_table_19222 = init_imgui_function_table_19222();
const imgui_function_table_19191 g_imgui_function_table_19191 = init_imgui_function_table_19191();
const imgui_function_table_19180 g_imgui_function_table_19180 = init_imgui_function_table_19180();
const imgui_function_table_19040 g_imgui_function_table_19040 = init_imgui_function_table_19040();
const imgui_function_table_19000 g_imgui_function_table_19000 = init_imgui_function_table_19000();
const imgui_function_table_18971 g_imgui_function_table_18971 = init_imgui_function_table_18971();
const imgui_function_table_18600 g_imgui_function_table_18600 = init_imgui_function_table_18600();

extern "C" __declspec(dllexport) const void *ReShadeGetImGuiFunctionTable(uint32_t version)
{
	if (version == 19220 || version == 19222)
		return &g_imgui_function_table_19222;
	if (version == 19190 || version == 19191)
		return &g_imgui_function_table_19191;
	if (version == 19180)
		return &g_imgui_function_table_19180;
	if (version == 19040)
		return &g_imgui_function_table_19040;
	if (version >= 19000 && version < 19040)
		return &g_imgui_function_table_19000;
	if (version == 18971)
		return &g_imgui_function_table_18971;
	if (version == 18600)
		return &g_imgui_function_table_18600;

	reshade::log::message(reshade::log::level::error, "Failed to retrieve ImGui function table, because the requested ImGui version (%u) is not supported.", version);
	return nullptr;
}

#endif
