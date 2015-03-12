#include "Log.hpp"

#include <windows.h>

namespace ReShade
{
	namespace
	{
		const char *sLogLevelNames[] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };
	}

	Log Log::Global;

	Log::Message::Message(Log &log, Level level) : mLog(log), mLevel(level)
	{
		this->mStream.setf(std::ios::left, std::ios::adjustfield);
	}
	Log::Message::~Message()
	{
		this->mLog.Dispatch(this->mLevel, this->mStream.str());
	}

	bool Log::Open(const boost::filesystem::path &path, Level level)
	{
		this->mFileStream.open(path.c_str(), std::ios::out | std::ios::app);

		if (!this->mFileStream.is_open())
		{
			return false;
		}

		this->mMaxLevel = level;
		this->mFileStream.flush();

		return true;
	}
	void Log::Close()
	{
		this->mFileStream.close();
	}

	void Log::Dispatch(Level level, const std::string &message)
	{
		if (level > this->mMaxLevel)
		{
			return;
		}
		if (!this->mFileStream.is_open())
		{
			return;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);		

		this->mFileStream << std::setfill('0') << std::setw(2) << time.wDay << '/' << std::setw(2) << time.wMonth << '/' << std::setw(4) << time.wYear << ' ' << std::setw(2) << time.wHour << ':' << std::setw(2) << time.wMinute << ':' << std::setw(2) << time.wSecond << ':' << std::setw(3) << time.wMilliseconds << ' ' << '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ') << " | " << sLogLevelNames[static_cast<unsigned int>(level)] << " | " << message;
		this->mFileStream << std::endl;
	}
}