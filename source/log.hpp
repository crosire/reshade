#pragma once

#include <codecvt>
#include <fstream>
#include <iomanip>
#include <boost\filesystem\path.hpp>

#define LOG(LEVEL) LOG_##LEVEL()
#define LOG_FATAL() reshade::log::message(reshade::log::level::fatal)
#define LOG_ERROR() reshade::log::message(reshade::log::level::error)
#define LOG_WARNING() reshade::log::message(reshade::log::level::warning)
#define LOG_INFO() reshade::log::message(reshade::log::level::info)
#define LOG_TRACE() reshade::log::message(reshade::log::level::debug)

namespace reshade
{
	namespace log
	{
		extern bool debug;
		extern std::ofstream stream;

		enum class level
		{
			fatal,
			error,
			warning,
			info,
			debug,
		};
		struct message
		{
			message(level level);
			inline ~message()
			{
				if (_dispatch)
				{
					stream << std::endl;
				}
			}

			template <typename T>
			inline message &operator<<(const T &value)
			{
				if (_dispatch)
				{
					stream << value;
				}

				return *this;
			}
			inline message &operator<<(const char *message)
			{
				return operator<<<std::string>(message);
			}
			inline message &operator<<(const wchar_t *message)
			{
				return operator<<<std::wstring>(message);
			}
			template <>
			inline message &operator<<(const std::wstring &message)
			{
				return operator<<<std::string>(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(message));
			}

		private:
			bool _dispatch;
		};

		/// <summary>
		/// Open a log file for writing.
		/// </summary>
		/// <param name="path">The path to the log file.</param>
		bool open(const boost::filesystem::path &path);
	};
}
