/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <cassert>
#include <cinttypes>
#include <string>
#include <filesystem>

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
	/// Opens a log file for writing.
	/// </summary>
	/// <param name="path">Path to the log file.</param>
	bool open_log_file(const std::filesystem::path &path, std::error_code &ec);

	/// <summary>
	/// Constructs a single log message including current time and level and writes it to the open log file.
	/// </summary>
	void message(level level, const char *format, ...);

#if defined(_HRESULT_DEFINED)
	inline std::string hr_to_string(HRESULT hr)
	{
		switch (hr)
		{
		case E_NOTIMPL:
			return "E_NOTIMPL";
		case E_OUTOFMEMORY:
			return "E_OUTOFMEMORY";
		case E_INVALIDARG:
			return "E_INVALIDARG";
		case E_NOINTERFACE:
			return "E_NOINTERFACE";
		case E_FAIL:
			return "E_FAIL";
		case 0x8876017C:
			return "D3DERR_OUTOFVIDEOMEMORY";
		case 0x88760868:
			return "D3DERR_DEVICELOST";
		case 0x8876086A:
			return "D3DERR_NOTAVAILABLE";
		case 0x8876086C:
			return "D3DERR_INVALIDCALL";
		case 0x88760870:
			return "D3DERR_DEVICEREMOVED";
		case 0x88760874:
			return "D3DERR_DEVICEHUNG";
		case DXGI_ERROR_INVALID_CALL:
			return "DXGI_ERROR_INVALID_CALL";
		case DXGI_ERROR_UNSUPPORTED:
			return "DXGI_ERROR_UNSUPPORTED";
		case DXGI_ERROR_DEVICE_REMOVED:
			return "DXGI_ERROR_DEVICE_REMOVED";
		case DXGI_ERROR_DEVICE_HUNG:
			return "DXGI_ERROR_DEVICE_HUNG";
		case DXGI_ERROR_DEVICE_RESET:
			return "DXGI_ERROR_DEVICE_RESET";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return "DXGI_ERROR_DRIVER_INTERNAL_ERROR";
		default:
			char temp_string[11];
			return std::string(temp_string, std::snprintf(temp_string, std::size(temp_string), "%#lx", static_cast<unsigned long>(hr)));
		}
	}
#endif

#if defined(_REFIID_DEFINED) && defined(_COMBASEAPI_H_)
	inline std::string iid_to_string(REFIID riid)
	{
		OLECHAR riid_string[40];
		const int len = StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));
		if (len != 0)
		{
			char temp_string[40];
			for (int i = 0; i < len; ++i)
				temp_string[i] = riid_string[i] & 0xFF;
			return std::string(temp_string, len);
		}
		return "{...}";
	}
#endif
}
