#pragma once

#include <sstream>
#include <iomanip>
#include <codecvt>
#include <boost\filesystem\path.hpp>

#define LOG(LEVEL) LOG_##LEVEL()
#define LOG_FATAL() ReShade::Log::Message(ReShade::Log::Level::Fatal)
#define LOG_ERROR() ReShade::Log::Message(ReShade::Log::Level::Error)
#define LOG_WARNING() ReShade::Log::Message(ReShade::Log::Level::Warning)
#define LOG_INFO() ReShade::Log::Message(ReShade::Log::Level::Info)
#define LOG_TRACE() ReShade::Log::Message(ReShade::Log::Level::Trace)

namespace ReShade
{
	class Log abstract
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
		struct Message
		{
			Message(Level level);
			~Message();

			template <typename T>
			inline Message &operator<<(const T &value)
			{
				this->mStream << value;

				return *this;
			}
			inline Message &operator<<(const char *message)
			{
				return operator<<<std::string>(message);
			}
			inline Message &operator<<(const wchar_t *message)
			{
				return operator<<<std::wstring>(message);
			}
			template <>
			inline Message &operator<<(const std::string &message)
			{
				this->mStream << message;

				return *this;
			}
			template <>
			inline Message &operator<<(const std::wstring &message)
			{
				this->mStream << std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(message);

				return *this;
			}

		private:
			Level mLevel;
			std::stringstream mStream;
		};

		static bool Open(const boost::filesystem::path &path, Level maxlevel);
		static void Close();

	private:
		static void Dispatch(Level level, const std::string &message);
	};
}