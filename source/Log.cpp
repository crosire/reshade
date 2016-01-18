#include "Log.hpp"

#include <Windows.h>

namespace reshade
{
	log::level log::s_max_level = log::level::info;
	std::ofstream log::s_filestream;

	log::message::message(level level) : _dispatch(level <= s_max_level && s_filestream.is_open())
	{
		if (!_dispatch)
		{
			return;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);

		const char levelNames[][6] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };

		s_filestream << std::right << std::setfill('0')
			<< std::setw(2) << time.wDay << '/'
			<< std::setw(2) << time.wMonth << '/'
			<< std::setw(4) << time.wYear << ' '
			<< std::setw(2) << time.wHour << ':'
			<< std::setw(2) << time.wMinute << ':'
			<< std::setw(2) << time.wSecond << ':'
			<< std::setw(3) << time.wMilliseconds << ' '
			<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
			<< " | " << levelNames[static_cast<unsigned int>(level)] << " | " << std::left;
	}
	log::message::~message()
	{
		if (!_dispatch)
		{
			return;
		}

		s_filestream << std::endl;
	}

	bool log::open(const boost::filesystem::path &path, level maxlevel)
	{
		s_filestream.open(path.native(), std::ios::out | std::ios::trunc);

		if (!s_filestream.is_open())
		{
			return false;
		}

		s_filestream.flush();

		s_max_level = maxlevel;

		return true;
	}
}
