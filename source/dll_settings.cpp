/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_settings.hpp"
#include "runtime_config.hpp"

static bool test_path(std::filesystem::path &target, const bool is_directory = true, const std::filesystem::path base = g_reshade_dll_path.parent_path()) noexcept
{
	std::filesystem::path path = target;

	if (path.is_relative())
		path = base / path;

	WCHAR buf[4096];
	DWORD size = 0;

	if (size = GetLongPathNameW(path.c_str(), buf, ARRAYSIZE(buf)); size == 0)
		return false;

	path = std::wstring_view(buf, size);

	if (path = path.lexically_normal(); !path.has_stem())
		path = path.parent_path();

	if (std::error_code ec; !(is_directory ? std::filesystem::is_directory(path, ec) : std::filesystem::exists(path, ec)))
		return false;

	target = std::move(path);
	return true;
}

static bool resolve_env_path(const std::wstring_view &env, std::filesystem::path &resolved, const bool is_directory = true, const std::filesystem::path base = g_reshade_dll_path.parent_path()) noexcept
{
	WCHAR buf[4096];
	DWORD size = 0;

	if (size = ExpandEnvironmentStringsW(env.data(), buf, ARRAYSIZE(buf)); size == 0)
		return false;

	std::filesystem::path path = std::wstring_view(buf, size);
	if (!test_path(path, is_directory, base))
		return false;

	resolved = std::move(path);
	return true;
}

void load_installation_settings() noexcept
{
	if (std::filesystem::path configuration_path = g_reshade_dll_path.stem().native() + L".ini"; test_path(configuration_path, false))
	{
		const reshade::ini_file &config = reshade::ini_file::load_cache(configuration_path);

		if (std::string path_utf8; config.get("INSTALL", "ReShadeContainerPath", path_utf8) && !path_utf8.empty())
		{
			std::wstring path_utf16; path_utf8.reserve(path_utf8.size());
			utf8::unchecked::utf8to16(path_utf8.begin(), path_utf8.end(), std::back_inserter(path_utf16));

			if (std::filesystem::path resolved; !resolve_env_path(path_utf16, resolved, true))
				g_reshade_container_path = g_reshade_dll_path.parent_path();
			else
				g_reshade_container_path = resolved;
		}
	}

	if (g_reshade_container_path.empty() || !test_path(g_reshade_container_path, true))
		g_reshade_container_path = g_reshade_dll_path.parent_path();

	g_reshade_config_path = g_reshade_container_path / L"ReShade.ini";
}

std::filesystem::path get_system_path() noexcept
{
	static std::filesystem::path system_path;

	if (!system_path.empty())
		return system_path; // Return the cached system path

	// First try environment variable, use system directory if it does not exist
	WCHAR buf[4096], sys[4096];
	DWORD buf_size = 0, sys_size = GetSystemDirectoryW(sys, ARRAYSIZE(sys));

	if (std::filesystem::path configuration_path = g_reshade_dll_path.stem().native() + L".ini"; test_path(configuration_path, false))
	{
		const reshade::ini_file &config = reshade::ini_file::load_cache(configuration_path);
		if (std::string path_utf8; config.get("INSTALL", "SystemModulesPath", path_utf8) && !path_utf8.empty())
		{
			std::wstring path_utf16; path_utf8.reserve(path_utf8.size());
			utf8::unchecked::utf8to16(path_utf8.begin(), path_utf8.end(), std::back_inserter(path_utf16));

			if (std::filesystem::path resolved; !resolve_env_path(path_utf16, resolved, true))
				return system_path = std::wstring_view(sys, sys_size);
			else
				return system_path = resolved;
		}
	}

	if (buf_size = GetEnvironmentVariableW(L"RESHADE_MODULE_PATH_OVERRIDE", buf, ARRAYSIZE(buf)); buf_size == 0) // Failure or empty
		return system_path = std::wstring_view(sys, sys_size);
	else if (std::filesystem::path path; !resolve_env_path(std::wstring_view(buf, buf_size), path, true, g_target_executable_path.parent_path()))
		return system_path = std::wstring_view(sys, sys_size);
	else
		return system_path = path;
}

std::filesystem::path get_module_path(HMODULE module) noexcept
{
	WCHAR buf[4096];

	if (DWORD size = GetModuleFileNameW(module, buf, ARRAYSIZE(buf)); size == 0)
		return {};
	else if (std::filesystem::path path = std::wstring_view(buf, size); !test_path(path, false))
		return {};
	else
		return path;
}
