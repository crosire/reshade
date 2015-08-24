#include "Log.hpp"

#include <Windows.h>

namespace ReShade
{
	Log::Level Log::sMaxLevel = Level::Info;
	std::ofstream Log::sFileStream;

	Log::Message::Message(Level level) : mDispatch(level <= sMaxLevel && sFileStream.is_open())
	{
		if (this->mDispatch)
		{
			SYSTEMTIME time;
			GetLocalTime(&time);

			const char levelNames[][6] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };

			sFileStream << std::right << std::setfill('0')
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
	}
	Log::Message::~Message()
	{
		if (this->mDispatch)
		{
			sFileStream << std::endl;
		}
	}

	bool Log::Open(const boost::filesystem::path &path, Level maxlevel)
	{
		sFileStream.open(path.c_str(), std::ios::out | std::ios::trunc);

		if (!sFileStream.is_open())
		{
			return false;
		}

		sFileStream.flush();

		sMaxLevel = maxlevel;

		return true;
	}
}
