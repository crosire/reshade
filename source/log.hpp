/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <utf8/unchecked.h>

#define LOG(LEVEL) LOG_##LEVEL()
#define LOG_INFO() reshade::log::message(reshade::log::level::info)
#define LOG_ERROR() reshade::log::message(reshade::log::level::error)
#define LOG_WARNING() reshade::log::message(reshade::log::level::warning)
#define LOG_DEBUG() reshade::log::message(reshade::log::level::debug)

namespace reshade::log
{
	enum class level
	{
		info = 3,
		error = 1,
		warning = 2,
		debug = 4,
	};

	extern std::ofstream stream;
	extern std::ostringstream linestream;
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

		template <>
		inline message &operator<<(const std::wstring &message)
		{
			static_assert(sizeof(std::wstring::value_type) == sizeof(uint16_t), "expected 'std::wstring' to use UTF-16 encoding");
			std::string utf8_message;
			utf8_message.reserve(message.size());
			utf8::unchecked::utf16to8(message.begin(), message.end(), std::back_inserter(utf8_message));
			return operator<<(utf8_message);
		}

		inline message &operator<<(const char *message)
		{
			stream << message;
			linestream << message;
			return *this;
		}
		inline message &operator<<(const wchar_t *message)
		{
			static_assert(sizeof(wchar_t) == sizeof(uint16_t), "expected 'wchar_t' to use UTF-16 encoding");
			std::string utf8_message;
			utf8::unchecked::utf16to8(message, message + wcslen(message), std::back_inserter(utf8_message));
			return operator<<(utf8_message);
		}
	};

	/// <summary>
	/// Open a log file for writing.
	/// </summary>
	/// <param name="path">The path to the log file.</param>
	bool open(const std::filesystem::path &path);
}
