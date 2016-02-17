#include "log.hpp"

#include <Windows.h>

namespace reshade
{
	namespace log
	{
		bool debug = false;
		std::ofstream stream;

		message::message(level level) : _dispatch(level <= (debug ? level::debug : level::info) && stream.is_open())
		{
			if (!_dispatch)
			{
				return;
			}

			SYSTEMTIME time;
			GetLocalTime(&time);

			const char level_names[][6] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };

			stream << std::right << std::setfill('0')
				<< std::setw(2) << time.wDay << '/'
				<< std::setw(2) << time.wMonth << '/'
				<< std::setw(4) << time.wYear << ' '
				<< std::setw(2) << time.wHour << ':'
				<< std::setw(2) << time.wMinute << ':'
				<< std::setw(2) << time.wSecond << ':'
				<< std::setw(3) << time.wMilliseconds << ' '
				<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
				<< " | " << level_names[static_cast<unsigned int>(level)] << " | " << std::left;
		}

		bool open(const boost::filesystem::path &path)
		{
			stream.open(path.native(), std::ios::out | std::ios::trunc);

			if (!stream.is_open())
			{
				return false;
			}

			stream.setf(std::ios_base::showbase);

			stream.flush();

			return true;
		}
	}
}
