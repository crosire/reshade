/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "unicode.hpp"

namespace reshade::unicode
{
	static inline bool IsSurrogate(const char16_t chr) { return 0b110110'0000000000 == (chr & 0b111110'0000000000); }
	static inline bool IsHighSurrogate(const char16_t chr) { return 0b110110'0000000000 == (chr & 0b111111'0000000000); }
	static inline bool IsLowSurrogate(const char16_t chr) { return 0b110111'0000000000 == (chr & 0b111111'0000000000); }

	bool convert(const char16_t a, const char16_t b, char32_t &chr)
	{
		if (IsSurrogate(a))
		{
			if (IsHighSurrogate(a) && IsLowSurrogate(b))
			{
				chr &= 0;
				chr |= (0b0'0000000000'1111111111 & b);
				chr |= (0b0'0000000000'1111111111 & a) << 10;
				chr |= (0b1'0000000000'0000000000);
				return true;
			}
			return false;
		}
		chr = a;
		return true;
	}

	bool convert(const char32_t chr, std::string& res)
	{
		/**/ if (chr < 0b00000'00000'0001'0000000) // 1 byte (7bit)
		{
			res += 0b0'0000000 | ((chr >> 00) & 0b0'1111111);
		}
		else if (chr < 0b00000'00001'0000'0000000) // 2 byte (11bit)
		{
			res += 0b110'00000 | ((chr >> 06) & 0b000'11111);
			res += 0b10'000000 | ((chr >> 00) & 0b00'111111);
		}
		else if (chr < 0b00001'00000'0000'0000000) // 3 byte (16bit)
		{
			res += 0b1110'0000 | ((chr >> 12) & 0b0000'1111);
			res += 0b10'000000 | ((chr >> 06) & 0b00'111111);
			res += 0b10'000000 | ((chr >> 00) & 0b00'111111);
		}
		else if (chr < 0b10001'00000'0000'0000000) // 4 byte (21bit but end with 0x10ffff)
		{
			res += 0b11110'000 | ((chr >> 18) & 0b00000'111);
			res += 0b10'000000 | ((chr >> 12) & 0b00'111111);
			res += 0b10'000000 | ((chr >> 06) & 0b00'111111);
			res += 0b10'000000 | ((chr >> 00) & 0b00'111111);
		}
		else
		{
			return false;
		}
		return true;
	}

	bool convert(const char32_t chr, std::wstring& res)
	{
		/**/ if (chr < 0b0'0001000000'0000000000)
		{
			res += static_cast<wchar_t>(chr);
		}
		else if (chr < 0b1'0001000000'0000000000)
		{
			res += static_cast<wchar_t>(0b0'0000110110'0000000000 | ((chr & 0b0'1111111111'0000000000) >> 10));
			res += static_cast<wchar_t>(0b0'0000110111'0000000000 | ((chr & 0b0'0000000000'1111111111) >> 00));
		}
		else
		{
			return false;
		}
		return true;
	}

	bool convert(const std::string& str, std::u32string& res)
	{
		for (auto it = str.begin(); it != str.end(); it++)
		{
			char32_t chr = 0;
			if ((*it & 0b1'0000000) == 0) // 1 byte (7bit)
			{
				chr = *it;
			}
			else if ((*it & 0b111'00000) == 0b110'00000) // 2 byte (11bit)
			{
				chr |= (*it & 0b000'11111) << 06;
				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 00;

				if (chr < 0b00000'00000'0001'0000000) // block to 1 byte char
					return false;
			}
			else if ((*it & 0b1111'0000) == 0b1110'0000) // 3 byte (16bit)
			{
				chr |= (*it & 0b0000'1111) << 12;

				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 06;

				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 00;

				if (chr < 0b00000'00001'0000'0000000) // block to 1 and 2 bytes char
					return false;
			}
			else if ((*it & 0b11111'000) == 0b11110'000) // 4 byte (21bit but end with 0x10ffff)
			{
				chr |= (*it & 0b00000'111) << 18;

				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 12;

				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 06;

				if (++it == str.end() || (*it & 0b10'000000) != 0b10'000000)
					return false;
				chr |= (*it & 0b00'111111) << 00;

				if (chr < 0b0'0001000000'0000000000) // block to 1, 2 and 3 bytes char
					return false;

				if (chr > 0b1'0000111111'1111111111) // end with 0x10ffff
					return false;
			}
			else
			{
				return false;
			}
			res += chr;
		}
		return true;
	}

	bool convert(const std::wstring& str, std::string& res)
	{
		std::u32string u32string;
		return convert(str, u32string) && convert(u32string, res);
	}

	bool convert(const std::wstring& str, std::u32string& res)
	{
		for (auto it = str.begin(); it != str.end(); it++)
		{
			char16_t a = *it, b = 0;
			if (IsHighSurrogate(a))
			{
				if (++it == str.end() || !IsLowSurrogate(*it))
					return false;
				b = *it;
			}
			char32_t chr;
			if (!convert(a, b, chr))
				return false;
			res.push_back(chr);
		}
		return true;
	}

	bool convert(const std::u32string& str, std::string& res)
	{
		for (auto it = str.begin(); it != str.end(); it++)
		{
			if (!convert(*it, res))
				return false;
		}
		return true;
	}

	bool convert(const std::u32string& str, std::wstring& res)
	{
		for (auto it = str.begin(); it != str.end(); it++)
		{
			if (!convert(*it, res))
				return false;
		}
		return true;
	}

	std::string convert_utf8(const std::wstring& str)
	{
		std::string res;
		std::u32string u32string;
		convert(str, u32string) && convert(u32string, res);
		return res;
	}

	std::wstring convert_utf16(const std::string& str)
	{
		std::wstring res;
		std::u32string u32string;
		convert(str, u32string) && convert(u32string, res);
		return res;
	}
}
