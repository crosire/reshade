/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>

namespace reshade::unicode
{
	bool convert(const char16_t h, const char16_t l, char32_t &chr);
	bool convert(const char32_t chr, std::string& res);
	bool convert(const char32_t chr, std::wstring& res);
	bool convert(const std::string& str, std::u32string& res);
	bool convert(const std::wstring& str, std::string& res);
	bool convert(const std::wstring& str, std::u32string& res);
	bool convert(const std::u32string& str, std::string& res);
	bool convert(const std::u32string& str, std::wstring& res);
	std::string convert_utf8(const std::wstring& str);
	std::wstring convert_utf16(const std::string& str);
}
