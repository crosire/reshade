/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>

namespace std
{
	template<class... Args>
	std::string format(string_view fmt, const Args &... args) noexcept {
		std::string s(static_cast<size_t>(::_scprintf(fmt.data(), args...)), '\0');
		return ::snprintf(s.data(), s.size() + 1, fmt.data(), args...), s;
	}
	template<class... Args>
	std::wstring format(wstring_view fmt, const Args &... args) noexcept {
		std::wstring s(static_cast<size_t>(::_scwprintf(fmt.data(), args...)), L'\0');
		return ::_snwprintf(s.data(), s.size(), fmt.data(), args...), s;
	}
}
