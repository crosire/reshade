#pragma once

#include <fstream>
#include <iomanip>
#include "unicode.hpp"
#include "filesystem.hpp"

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
			~message();

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
				if (_dispatch)
				{
					stream << message;
				}

				return *this;
			}
			template <>
			inline message &operator<<(const std::wstring &message)
			{
				return operator<<(utf16_to_utf8(message));
			}
			inline message &operator<<(const wchar_t *message)
			{
				return operator<<(utf16_to_utf8(message));
			}

		private:
			bool _dispatch;
		};

		/// <summary>
		/// Open a log file for writing.
		/// </summary>
		/// <param name="path">The path to the log file.</param>
		bool open(const filesystem::path &path);
	};
}
