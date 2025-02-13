/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if defined(RESHADE_API_LIBRARY_EXPORT) && RESHADE_ADDON

#include "imgui_function_table_19180.hpp"
#include "imgui_function_table_19040.hpp"
#include "imgui_function_table_19000.hpp"
#include "imgui_function_table_18971.hpp"
#include "imgui_function_table_18600.hpp"

extern const imgui_function_table_19180 init_imgui_function_table_19180();
extern const imgui_function_table_19040 init_imgui_function_table_19040();
extern const imgui_function_table_19000 init_imgui_function_table_19000();
extern const imgui_function_table_18971 init_imgui_function_table_18971();
extern const imgui_function_table_18600 init_imgui_function_table_18600();

// Force initialization order (has to happen in a single translation unit to be well defined by declaration order) from newest to oldest, so that older function tables can reference newer ones
const imgui_function_table_19180 g_imgui_function_table_19180 = init_imgui_function_table_19180();
const imgui_function_table_19040 g_imgui_function_table_19040 = init_imgui_function_table_19040();
const imgui_function_table_19000 g_imgui_function_table_19000 = init_imgui_function_table_19000();
const imgui_function_table_18971 g_imgui_function_table_18971 = init_imgui_function_table_18971();
const imgui_function_table_18600 g_imgui_function_table_18600 = init_imgui_function_table_18600();

#endif
