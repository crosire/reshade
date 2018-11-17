/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <filesystem>

bool imgui_font_select(const char *name, std::filesystem::path &path, int &size);

bool imgui_directory_dialog(const char *name, std::filesystem::path &path);
