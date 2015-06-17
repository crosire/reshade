#include "Log.hpp"

#include <fstream>
#include <Windows.h>

namespace ReShade
{
	namespace
	{
		Log::Level sMaxLevel = Log::Level::Info;
		const char sLogLevelNames[][6] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };
		std::ofstream sFileStream;
	}

	Log::Message::Message(Level level) : mLevel(level)
	{
		this->mStream.setf(std::ios::left, std::ios::adjustfield);
	}
	Log::Message::~Message()
	{
		Dispatch(this->mLevel, this->mStream.str());
	}

	bool Log::Open(const boost::filesystem::path &path, Level maxlevel)
	{
		sFileStream.open(path.c_str(), std::ios::out | std::ios::app);

		if (!sFileStream.is_open())
		{
			return false;
		}

		sMaxLevel = maxlevel;
		sFileStream.flush();

		return true;
	}
	void Log::Close()
	{
		sFileStream.close();
	}

	void Log::Dispatch(Level level, const std::string &message)
	{
		if (level > sMaxLevel || !sFileStream.is_open())
		{
			return;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);		

		sFileStream << std::setfill('0')
			<< std::setw(2) << time.wDay << '/'
			<< std::setw(2) << time.wMonth << '/'
			<< std::setw(4) << time.wYear << ' '
			<< std::setw(2) << time.wHour << ':'
			<< std::setw(2) << time.wMinute << ':'
			<< std::setw(2) << time.wSecond << ':'
			<< std::setw(3) << time.wMilliseconds << ' '
			<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
			<< " | " << sLogLevelNames[static_cast<unsigned int>(level)] << " | "
			<< message << std::endl;
	}
}