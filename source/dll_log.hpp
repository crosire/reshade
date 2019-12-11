/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <iomanip>
#include <sstream>
#include <filesystem>
#include <utf8/unchecked.h>
#include <combaseapi.h> // Included for REFIID and HRESULT

#define LOG(LEVEL) LOG_##LEVEL()
#define LOG_INFO() reshade::log::message(reshade::log::level::info)
#define LOG_ERROR() reshade::log::message(reshade::log::level::error)
#define LOG_WARN() reshade::log::message(reshade::log::level::warning)
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

	/// <summary>
	/// The current log line stream.
	/// </summary>
	extern std::ostringstream line;
	/// <summary>
	/// The history of all log messages.
	/// </summary>
	extern std::vector<std::string> lines;

	/// <summary>
	/// Constructs a single log message including current time and level and writes it to the open log file.
	/// </summary>
	struct message
	{
		explicit message(level level);
		~message();

		template <typename T>
		message &operator<<(const T &value)
		{
			line << value;
			return *this;
		}

		template <>
		message &operator<<(REFIID riid)
		{
			OLECHAR riid_string[40];
			StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));
			return *this << riid_string;
		}

		template <>
		message &operator<<(const HRESULT &hresult) // Note: HRESULT is just an alias for long, so this falsely catches all long values too
		{
			switch (hresult)
			{
			case E_INVALIDARG:
				return *this << "E_INVALIDARG";
			case DXGI_ERROR_INVALID_CALL:
				return *this << "DXGI_ERROR_INVALID_CALL";
			case DXGI_ERROR_DEVICE_REMOVED:
				return *this << "DXGI_ERROR_DEVICE_REMOVED";
			case DXGI_ERROR_DEVICE_HUNG:
				return *this << "DXGI_ERROR_DEVICE_HUNG";
			case DXGI_ERROR_DEVICE_RESET:
				return *this << "DXGI_ERROR_DEVICE_RESET";
			default:
				*this << std::hex << static_cast<unsigned long>(hresult) << std::dec;
				break;
			}

			if (LPCTSTR message = nullptr;
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
					nullptr, hresult, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&message), 0, nullptr)
				&& message != nullptr)
			{
				*this << " (" << message << ')';
				LocalFree((HLOCAL)message);
			}

			return *this;
		}

		template <>
		message &operator<<(const std::wstring &message)
		{
			static_assert(sizeof(std::wstring::value_type) == sizeof(uint16_t), "expected 'std::wstring' to use UTF-16 encoding");
			std::string utf8_message;
			utf8_message.reserve(message.size());
			utf8::unchecked::utf16to8(message.begin(), message.end(), std::back_inserter(utf8_message));
			return operator<<(utf8_message);
		}

		template <>
		message &operator<<(const std::filesystem::path &path)
		{
			return operator<<('"' + path.u8string() + '"');
		}

		inline message &operator<<(const char *message)
		{
			line << message;
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
