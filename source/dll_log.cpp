/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include <mutex>
#include <Windows.h>

struct scoped_file_handle
{
	~scoped_file_handle()
	{
		if (handle != INVALID_HANDLE_VALUE)
			CloseHandle(handle);
	}

	inline operator HANDLE() const { return handle; }
	inline void operator=(HANDLE new_handle) { handle = new_handle; }

private:
	HANDLE handle = INVALID_HANDLE_VALUE;
};

static std::mutex s_message_mutex;
static scoped_file_handle s_file_handle;
std::ostringstream reshade::log::line_stream;

reshade::log::message::message(level level)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	const char level_names[][6] = { "ERROR", "WARN ", "INFO ", "DEBUG" };
	assert((static_cast<size_t>(level) - 1) < ARRAYSIZE(level_names));

	// Lock the stream until the message is complete
	s_message_mutex.lock();

	// Start a new line
	line_stream.str(std::string());
	line_stream.clear();

	line_stream << std::right << std::setfill('0')
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
		<< level_names[static_cast<unsigned int>(level) - 1] << " | " << std::left;
}
reshade::log::message::~message()
{
	std::string line_string = line_stream.str();
	line_string += "\r\n"; // Terminate line with line feed

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

	// The message is finished, we can unlock the stream
	s_message_mutex.unlock();
}

void reshade::log::open_log_file(const std::filesystem::path &path)
{
	// Close the previous file first
	if (s_file_handle != INVALID_HANDLE_VALUE)
		CloseHandle(s_file_handle);

	// Set default line stream settings
	line_stream.setf(std::ios::left);
	line_stream.setf(std::ios::showbase);

	// Open the log file for writing (and flush on each write) and clear previous contents
	s_file_handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
}
