#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost\noncopyable.hpp>
#include <boost\filesystem\path.hpp>

#pragma region Undefine Log Levels
#undef FATAL
#undef ERROR
#undef WARNING
#undef INFO
#undef TRACE
#pragma endregion

#define LOG(LEVEL) _LOG_##LEVEL(ReShade::Log::Global)
#define _LOG_FATAL(logger) ReShade::Log::Message(logger, ReShade::Log::Level::Fatal)
#define _LOG_ERROR(logger) ReShade::Log::Message(logger, ReShade::Log::Level::Error)
#define _LOG_WARNING(logger) ReShade::Log::Message(logger, ReShade::Log::Level::Warning)
#define _LOG_INFO(logger) ReShade::Log::Message(logger, ReShade::Log::Level::Info)
#define _LOG_TRACE(logger) ReShade::Log::Message(logger, ReShade::Log::Level::Trace)

namespace ReShade
{
	class Log : boost::noncopyable
	{
	public:
		enum class Level
		{
			Fatal,
			Error,
			Warning,
			Info,
			Trace
		};
		struct Message : boost::noncopyable
		{
			Message(Log &log, Level level);
			~Message();

			template <typename T>
			inline Message &operator <<(const T &value)
			{
				this->mStream << value;

				return *this;
			}

		private:
			Log &mLog;
			Level mLevel;
			std::stringstream mStream;
		};

		static Log Global;

	public:
		bool Open(const boost::filesystem::path &path, Level level);
		void Close();

	protected:
		void Dispatch(Level level, const std::string &message);

	private:
		Level mMaxLevel;
		std::ofstream mFileStream;
	};
}