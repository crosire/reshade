/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <utf8/unchecked.h>
#include <Windows.h>
#include <filesystem>

extern std::filesystem::path g_reshade_dll_path;
extern std::filesystem::path g_reshade_container_path;
extern std::filesystem::path g_reshade_config_path;
extern std::filesystem::path g_target_executable_path;

void load_installation_settings() noexcept;

std::filesystem::path get_system_path() noexcept;

std::filesystem::path get_module_path(HMODULE module) noexcept;
