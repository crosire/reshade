/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include <Windows.h>

namespace reshade::log
{
	std::ofstream stream;

	message::message(level level)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);

		const char level_names[][6] = { "INFO ", "ERROR", "WARN " };

		stream << std::right << std::setfill('0')
			<< std::setw(4) << time.wYear << '-'
			<< std::setw(2) << time.wMonth << '-'
			<< std::setw(2) << time.wDay << 'T'
			<< std::setw(2) << time.wHour << ':'
			<< std::setw(2) << time.wMinute << ':'
			<< std::setw(2) << time.wSecond << ':'
			<< std::setw(3) << time.wMilliseconds << ' '
			<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
			<< " | " << level_names[static_cast<unsigned int>(level)] << " | " << std::left;
	}
	message::~message()
	{
		stream << std::endl;
	}

	bool open(const filesystem::path &path)
	{
		stream.open(path.wstring(), std::ios::out | std::ios::trunc);

		if (!stream.is_open())
		{
			return false;
		}

		stream.setf(std::ios_base::showbase);

		stream.flush();

		return true;
	}
}
