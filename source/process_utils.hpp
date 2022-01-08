/*
 * Copyright (C) 2022 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <filesystem>

namespace reshade
{
	bool open_explorer(const std::filesystem::path &path);

	bool execute_command(const std::string &command_line, const std::filesystem::path &working_directory = L".", bool no_window = false);
}
