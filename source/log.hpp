/**
* Copyright (C) 2014 Patrick Mours. All rights reserved.
* License: https://github.com/crosire/reshade#license
*/

#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>
#include <utf8/unchecked.h>
#include "filesystem.hpp"
#include <mutex>

#define LOG(LEVEL) LOG_##LEVEL()
#define LOG_INFO() reshade::log::message(reshade::log::level::info)
#define LOG_ERROR() reshade::log::message(reshade::log::level::error)
#define LOG_WARNING() reshade::log::message(reshade::log::level::warning)
#define LOG_DEBUG() reshade::log::message(reshade::log::level::debug)


namespace reshade::log
{
	enum class level
	{
		info = 4,
		error = 1,
		warning = 2,
		debug = 3,
	};

	extern std::ofstream stream;
	extern level last_level;
	extern std::ostringstream linestream;
	extern int log_level;
	extern std::mutex _mutex;
	extern std::vector<std::string> lines;

	struct message
	{
		message(level level);
		~message();

		template <typename T>
		inline message &operator<<(const T &value)
		{
			stream << value;
			linestream << value;
			
			return *this;
		}
		inline message &operator<<(const char *message)
		{
			stream << message;
			linestream << message;

			return *this;
		}
		template <>
		inline message &operator<<(const std::wstring &message)
		{
			std::string utf8message;
			utf8::unchecked::utf16to8(message.begin(), message.end(), std::back_inserter(utf8message));
			return operator<<(utf8message);


		}
		inline message &operator<<(const wchar_t *message)
		{
			std::wstring wmessage = std::wstring(message);
			std::string utf8message;
			utf8::unchecked::utf16to8(wmessage.begin(), wmessage.end(), std::back_inserter(utf8message));
			return operator<<(utf8message);
		}
	};

	/// <summary>
	/// Open a log file for writing.
	/// </summary>
	/// <param name="path">The path to the log file.</param>
	bool open(const filesystem::path &path);
}
