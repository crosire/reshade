/*
 * Copyright (C) 2022 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <filesystem>

namespace reshade
{
	/// <summary>
	/// Opens a Windows explorer window with the specified file selected.
	/// </summary>
	bool open_explorer(const std::filesystem::path &path);

	/// <summary>
	/// Executes the specified command as a new process, which basic (not elevated) user privileges. 
	/// </summary>
	bool execute_command(const std::string &command_line, const std::filesystem::path &working_directory = L".", bool no_window = false);
}
