/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dll_log.hpp"
#include <Windows.h>

struct scoped_file_handle
{
	~scoped_file_handle()
	{
		if (handle != INVALID_HANDLE_VALUE)
			CloseHandle(handle);
	}

	operator HANDLE() const { return handle; }

	void operator=(HANDLE new_handle)
	{
		handle = new_handle;
	}

private:
	HANDLE handle = INVALID_HANDLE_VALUE;
};

static scoped_file_handle s_file_handle;

reshade::log::message::message(level level)
{
	static constexpr char level_names[][6] = { "ERROR", "WARN ", "INFO ", "DEBUG" };

	if (static_cast<size_t>(level) == 0)
		level = level::error;
	if (static_cast<size_t>(level) > std::size(level_names))
		level = level::debug;

	SYSTEMTIME time;
	GetLocalTime(&time);

	// Set default line stream settings
	_line_stream.setf(std::ios::left);
	_line_stream.setf(std::ios::showbase);

	// Start a new line
	_line_stream << std::right << std::setfill('0')
#if RESHADE_VERBOSE_LOG
		<< std::setw(4) << time.wYear << '-'
		<< std::setw(2) << time.wMonth << '-'
		<< std::setw(2) << time.wDay << 'T'
#endif
		<< std::setw(2) << time.wHour << ':'
		<< std::setw(2) << time.wMinute << ':'
		<< std::setw(2) << time.wSecond << ':'
		<< std::setw(3) << time.wMilliseconds << ' '
		<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ') << " | "
		<< level_names[static_cast<size_t>(level) - 1] << " | " << std::left;
}
reshade::log::message::~message()
{
	std::string line_string = _line_stream.str();
	line_string += '\n'; // Terminate line with line feed

	// Replace all LF with CRLF
	for (size_t offset = 0; (offset = line_string.find('\n', offset)) != std::string::npos; offset += 2)
		line_string.replace(offset, 1, "\r\n", 2);

	// Write line to the log file
	if (s_file_handle != INVALID_HANDLE_VALUE)
	{
		DWORD written = 0;
		WriteFile(s_file_handle, line_string.data(), static_cast<DWORD>(line_string.size()), &written, nullptr);
		assert(written == line_string.size());
	}

#ifndef NDEBUG
	// Write line to the debug output
	OutputDebugStringA(line_string.c_str());
#endif
}

bool reshade::log::open_log_file(const std::filesystem::path &path, std::error_code &ec)
{
	// Close the previous file first
	// Do this here, instead of in 'scoped_file_handle::operator=', so that the old handle is closed before the new handle is created
	if (s_file_handle != INVALID_HANDLE_VALUE)
		CloseHandle(s_file_handle);

	// Open the log file for writing (and flush on each write) and clear previous contents
	s_file_handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);

	if (s_file_handle != INVALID_HANDLE_VALUE)
	{
		// Last error may be ERROR_ALREADY_EXISTS if an existing file was overwritten, which can be ignored
		ec.clear();
		return true;
	}
	else
	{
		ec.assign(GetLastError(), std::system_category());
		return false;
	}
}
