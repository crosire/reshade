/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <codecvt>
#include <algorithm>
#include <nowide/convert.hpp>

inline std::string utf16_to_utf8(const std::wstring &s)
{
	return nowide::narrow(s);
}
inline std::string utf16_to_utf8(const wchar_t *s, size_t len)
{
	return nowide::narrow(std::wstring(s, len));
}

template <size_t OUTSIZE>
inline void utf16_to_utf8(const std::wstring &s, char(&target)[OUTSIZE])
{
	const auto o = utf16_to_utf8(s);
	std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
}

inline std::wstring utf8_to_utf16(const std::string &s)
{
	return nowide::widen(s);
}
inline std::wstring utf8_to_utf16(const char *s, size_t len)
{
	return nowide::widen(std::string(s, len));
}

template <size_t OUTSIZE>
inline void utf8_to_utf16(const std::string &s, wchar_t(&target)[OUTSIZE])
{
	const auto o = utf8_to_utf16(s);
	std::copy_n(o.begin(), std::min(o.size(), OUTSIZE), target);
}
