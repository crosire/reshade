/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "platform_utils.hpp"
#include <utf8/unchecked.h>
#include <Windows.h>
#include <Shellapi.h>
#include <dwmapi.h>
#include <mmsystem.h>

bool reshade::utils::open_explorer(const std::filesystem::path &path)
{
	// Use absolute path to explorer to avoid potential security issues when executable is replaced
	WCHAR explorer_path[MAX_PATH];
	if (GetWindowsDirectoryW(explorer_path, ARRAYSIZE(explorer_path)) == 0)
		return false;
	wcscat_s(explorer_path, L"\\explorer.exe");

	return ShellExecuteW(nullptr, L"open", explorer_path, (L"/select,\"" + path.wstring() + L"\"").c_str(), nullptr, SW_SHOWDEFAULT) != nullptr;
}

bool reshade::utils::execute_command(const std::string &command_line, const std::filesystem::path &working_directory, bool hide_window)
{
	std::wstring command_line_wide;
	command_line_wide.reserve(command_line.size());
	utf8::unchecked::utf8to16(command_line.cbegin(), command_line.cend(), std::back_inserter(command_line_wide));

	// Check if this is actually a website URL that should be opened in the default web browser via the shell
	if (command_line.find("http") == 0 && command_line.find_first_of(" \t") == std::string::npos)
		return ShellExecuteW(nullptr, L"open", command_line_wide.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != nullptr;

	DWORD state_buffer_size;
	DWORD process_creation_flags = NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP;
	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	if (hide_window)
	{
		process_creation_flags |= CREATE_NO_WINDOW;

		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}

	HANDLE process_token_handle = nullptr;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &process_token_handle))
		return false;
	GetTokenInformation(process_token_handle, TOKEN_INFORMATION_CLASS::TokenPrivileges, nullptr, 0, &state_buffer_size);

	const auto new_state = static_cast<PTOKEN_PRIVILEGES>(operator new(state_buffer_size));
	new_state->PrivilegeCount = 1;
	new_state->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	const auto previous_state = static_cast<PTOKEN_PRIVILEGES>(operator new(state_buffer_size));

	bool success = false;

	if (LookupPrivilegeValue(nullptr, SE_INCREASE_QUOTA_NAME, &new_state->Privileges[0].Luid) != FALSE &&
		AdjustTokenPrivileges(process_token_handle, FALSE, new_state, state_buffer_size, previous_state, &state_buffer_size) != FALSE &&
		GetLastError() != ERROR_NOT_ALL_ASSIGNED)
	{
		// Current process is elevated

		const HWND shell_window_handle = GetShellWindow();
		if (shell_window_handle == nullptr)
			goto exit_failure;

		DWORD shell_process_id = 0;
		GetWindowThreadProcessId(shell_window_handle, &shell_process_id);

		HANDLE shell_process_handle = nullptr;
		if (shell_process_id == 0 || (shell_process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, shell_process_id)) == nullptr)
			goto exit_failure;

		HANDLE desktop_token_handle = nullptr;
		if (!OpenProcessToken(shell_process_handle, TOKEN_DUPLICATE, &desktop_token_handle))
			goto exit_failure;

		HANDLE duplicated_token_handle = nullptr;
		if (!DuplicateTokenEx(desktop_token_handle, TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, nullptr, SECURITY_IMPERSONATION_LEVEL::SecurityImpersonation, TOKEN_TYPE::TokenPrimary, &duplicated_token_handle))
			goto exit_failure;

		if (!CreateProcessWithTokenW(duplicated_token_handle, 0, nullptr, command_line_wide.data(), process_creation_flags, nullptr, working_directory.c_str(), &si, &pi))
			goto exit_failure;

		if (duplicated_token_handle)
			CloseHandle(duplicated_token_handle);
		if (desktop_token_handle)
			CloseHandle(desktop_token_handle);
		if (shell_process_handle)
			CloseHandle(shell_process_handle);
	}
	else
	{
		// Current process is not elevated

		if (!CreateProcessW(nullptr, command_line_wide.data(), nullptr, nullptr, FALSE, process_creation_flags, nullptr, working_directory.c_str(), &si, &pi))
			goto exit_failure;
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	success = true;

exit_failure:
	if (previous_state->PrivilegeCount > 0)
		AdjustTokenPrivileges(process_token_handle, FALSE, previous_state, 0, nullptr, nullptr);

	if (process_token_handle)
		CloseHandle(process_token_handle);

	operator delete(previous_state);
	operator delete(new_state);

	return success;
}

void reshade::utils::play_sound_async(const std::filesystem::path &audio_file)
{
	const std::filesystem::path normalized_audio_file_path = audio_file.lexically_normal();
	if (normalized_audio_file_path.empty() || normalized_audio_file_path.native().length() >= 256)
		return;
	PlaySoundW(normalized_audio_file_path.c_str(), nullptr, SND_ASYNC | SND_NOSTOP | SND_FILENAME);
}

void reshade::utils::set_window_transparency(void *window, bool enabled)
{
	DWM_BLURBEHIND blur_behind = {};
	blur_behind.dwFlags = DWM_BB_ENABLE;
	blur_behind.fEnable = enabled;
	DwmEnableBlurBehindWindow(GetAncestor(static_cast<HWND>(window), GA_ROOT), &blur_behind);
}
