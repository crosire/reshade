#pragma once

#include <vector>
#include <string>
#include <codecvt>
#include <algorithm>

namespace stdext
{
	template <class InputIt, class ForwardIt, class BinOp>
	inline void for_each_token(InputIt first, InputIt last, ForwardIt d_first, ForwardIt d_last, BinOp op)
	{
		while (first != last)
		{
			const auto pos = std::find_first_of(first, last, d_first, d_last);

			op(first, pos);

			if (pos == last)
			{
				break;
			}

			first = std::next(pos);
		}
	}

	inline std::vector<std::string> split(const std::string &str, char delim)
	{
		const char delims[1] = { delim };
		std::vector<std::string> result;
		for_each_token(str.cbegin(), str.cend(), std::cbegin(delims), std::cend(delims),
			[&result](auto first, auto second) {
			if (first != second)
				result.emplace_back(first, second);
		});
		return result;
	}

	inline void trim(std::string &str, const char *chars = " \t")
	{
		str.erase(0, str.find_first_not_of(chars));
		str.erase(str.find_last_not_of(chars) + 1);
	}
	inline std::string trim(const std::string &str, const char *chars = " \t")
	{
		std::string res(str);
		trim(res, chars);
		return res;
	}

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
}
