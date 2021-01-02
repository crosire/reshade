/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <cassert>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <utf8/unchecked.h>
#include <combaseapi.h> // REFIID, HRESULT

#undef INFO
#undef ERROR // This is defined in the Windows SDK headers
#undef WARN
#undef DEBUG

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
	/// Open a log file for writing.
	/// </summary>
	/// <param name="path">The path to the log file.</param>
	void open_log_file(const std::filesystem::path &path);

	/// <summary>
	/// The current log line stream.
	/// </summary>
	extern std::ostringstream line_stream;

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
			line_stream << value;
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
			case E_NOTIMPL:
				return *this << "E_NOTIMPL";
			case E_OUTOFMEMORY:
				return *this << "E_OUTOFMEMORY";
			case E_INVALIDARG:
				return *this << "E_INVALIDARG";
			case E_NOINTERFACE:
				return *this << "E_NOINTERFACE";
			case E_FAIL:
				return *this << "E_FAIL";
			case 0x8876017C:
				return *this << "D3DERR_OUTOFVIDEOMEMORY";
			case 0x88760868:
				return *this << "D3DERR_DEVICELOST";
			case 0x8876086A:
				return *this << "D3DERR_NOTAVAILABLE";
			case 0x8876086C:
				return *this << "D3DERR_INVALIDCALL";
			case 0x88760870:
				return *this << "D3DERR_DEVICEREMOVED";
			case DXGI_ERROR_INVALID_CALL:
				return *this << "DXGI_ERROR_INVALID_CALL";
			case DXGI_ERROR_UNSUPPORTED:
				return *this << "DXGI_ERROR_UNSUPPORTED";
			case DXGI_ERROR_DEVICE_REMOVED:
				return *this << "DXGI_ERROR_DEVICE_REMOVED";
			case DXGI_ERROR_DEVICE_HUNG:
				return *this << "DXGI_ERROR_DEVICE_HUNG";
			case DXGI_ERROR_DEVICE_RESET:
				return *this << "DXGI_ERROR_DEVICE_RESET";
			default:
				return *this << std::hex << static_cast<unsigned long>(hresult) << std::dec;
			}
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
			line_stream << message;
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
}
