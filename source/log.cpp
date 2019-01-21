/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include <mutex>
#include <assert.h>
#include <Windows.h>

namespace reshade::log
{
	std::ofstream stream;
	std::ostringstream linestream;
	std::vector<std::string> lines;
	static std::mutex s_mutex;

	message::message(level level)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);

		const char level_names[][6] = { "ERROR", "WARN ", "INFO ", "DEBUG" };
		assert(static_cast<unsigned int>(level) - 1 < _countof(level_names));

		// Lock the stream until the message is complete
		s_mutex.lock();

		// Start a new line
		linestream.str("");
		linestream.clear();

		stream << std::right << std::setfill('0')
#if RESHADE_VERBOSE_LOG
			<< std::setw(4) << time.wYear << '-'
			<< std::setw(2) << time.wMonth << '-'
			<< std::setw(2) << time.wDay << 'T'
#endif
			<< std::setw(2) << time.wHour << ':'
			<< std::setw(2) << time.wMinute << ':'
			<< std::setw(2) << time.wSecond << ':'
			<< std::setw(3) << time.wMilliseconds << ' '
			<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
			<< " | "
			<< level_names[static_cast<unsigned int>(level) - 1] << " | " << std::left;
		linestream
			<< level_names[static_cast<unsigned int>(level) - 1] << " | ";
	}
	message::~message()
	{
		// Finish the line
		stream << std::endl;
		linestream << std::endl;

		lines.push_back(linestream.str());

		// The message is finished, we can unlock the stream
		s_mutex.unlock();
	}

	bool open(const std::filesystem::path &path)
	{
		stream.open(path, std::ios::out | std::ios::trunc);

		if (!stream.is_open())
		{
			return false;
		}

		stream.setf(std::ios_base::showbase);

		stream.flush();

		return true;
	}
}
