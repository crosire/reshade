/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <codecvt>

inline std::string utf16_to_utf8(const std::wstring &s)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s);
}
inline std::string utf16_to_utf8(const wchar_t *s, size_t len)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s, s + len);
}
inline std::wstring utf8_to_utf16(const std::string &s)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s);
}
inline std::wstring utf8_to_utf16(const char *s, size_t len)
{
	return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s, s + len);
}
template <size_t OUTSIZE>
inline void utf16_to_utf8(const std::wstring &s, char(&target)[OUTSIZE])
{
	const auto o = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(s);
	std::copy(o.begin(), o.end(), target);
}
template <size_t OUTSIZE>
inline void utf8_to_utf16(const std::string &s, wchar_t(&target)[OUTSIZE])
{
	const auto o = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(s);
	std::copy(o.begin(), o.end(), target);
}
