#include "Log.hpp"

#include <fstream>
#include <Windows.h>

namespace ReShade
{
	namespace Log
	{
		namespace
		{
			Level sMaxLevel = Level::Info;
			std::ofstream sFileStream;
		}

		Message::Message(Level level) : mDispatch(level <= sMaxLevel && sFileStream.is_open()), mStream(sFileStream)
		{
			if (this->mDispatch)
			{
				SYSTEMTIME time;
				GetLocalTime(&time);

				const char levelNames[][6] = { "FATAL", "ERROR", "WARN ", "INFO ", "TRACE" };

				this->mStream << std::setfill('0')
					<< std::setw(2) << time.wDay << '/'
					<< std::setw(2) << time.wMonth << '/'
					<< std::setw(4) << time.wYear << ' '
					<< std::setw(2) << time.wHour << ':'
					<< std::setw(2) << time.wMinute << ':'
					<< std::setw(2) << time.wSecond << ':'
					<< std::setw(3) << time.wMilliseconds << ' '
					<< '[' << std::setw(5) << GetCurrentThreadId() << ']' << std::setfill(' ')
					<< " | " << levelNames[static_cast<unsigned int>(level)] << " | ";
			}
		}
		Message::~Message()
		{
			if (this->mDispatch)
			{
				this->mStream << std::endl;
			}
		}

		bool Open(const boost::filesystem::path &path, Level maxlevel)
		{
			sFileStream.open(path.c_str(), std::ios::out | std::ios::app);

			if (!sFileStream.is_open())
			{
				return false;
			}

			sFileStream.setf(std::ios::left, std::ios::adjustfield);
			sFileStream.flush();

			sMaxLevel = maxlevel;

			return true;
		}
		void Close()
		{
			sFileStream.close();
		}
	}
}
