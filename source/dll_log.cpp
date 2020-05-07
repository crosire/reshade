/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include <mutex>
#include <cassert>
#include <fstream>
#include <Windows.h>

std::ostringstream reshade::log::line;
static std::mutex s_message_mutex;
static std::ofstream s_file_stream;

reshade::log::message::message(level level)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	const char level_names[][6] = { "ERROR", "WARN ", "INFO ", "DEBUG" };
	assert(static_cast<unsigned int>(level) - 1 < ARRAYSIZE(level_names));

	// Lock the stream until the message is complete
	s_message_mutex.lock();

	// Start a new line
	line.str("");
	line.clear();

	line << std::right << std::setfill('0')
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
	std::string line_string = line.str();

	// Write line to the log file and flush it
	s_file_stream << line_string << std::endl;

#ifndef NDEBUG
	// Write line to the debug output
	line_string += '\n';
	OutputDebugStringA(line_string.c_str());
#endif

	// The message is finished, we can unlock the stream
	s_message_mutex.unlock();
}

bool reshade::log::open(const std::filesystem::path &path)
{
	if (s_file_stream.is_open())
		// Close the previous stream first
		s_file_stream.close();

	line.setf(std::ios::left);
	line.setf(std::ios::showbase);

	s_file_stream.open(path, std::ios::out | std::ios::trunc);

	s_file_stream.setf(std::ios::left);
	s_file_stream.setf(std::ios::showbase);
	s_file_stream.flush();

	return s_file_stream.is_open();
}
