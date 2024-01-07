/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dll_resources.hpp"
#include <cassert>
#include <Windows.h>
#include <utf8/unchecked.h>

extern HMODULE g_module_handle;

reshade::resources::data_resource reshade::resources::load_data_resource(unsigned short id)
{
	const HRSRC info = FindResource(g_module_handle, MAKEINTRESOURCE(id), RT_RCDATA);
	assert(info != nullptr);
	const HGLOBAL handle = LoadResource(g_module_handle, info);

	data_resource result;
	result.data = LockResource(handle);
	result.data_size = SizeofResource(g_module_handle, info);

	return result;
}

std::string reshade::resources::load_string(unsigned short id)
{
	LPCWSTR s = nullptr;
	const int length = LoadStringW(g_module_handle, id, reinterpret_cast<LPWSTR>(&s), 0);
	assert(length > 0);

	std::string utf8_string;
	utf8_string.reserve(length);
	utf8::unchecked::utf16to8(s, s + length, std::back_inserter(utf8_string));

	return utf8_string;
}

#if RESHADE_LOCALIZATION
std::string reshade::resources::get_current_language()
{
	std::vector<std::string> languages;
	if (ULONG num = 0, size = 0;
		GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, nullptr, &size))
	{
		std::wstring wbuf; wbuf.resize(size);
		if (GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, wbuf.data(), &size))
		{
			std::string cbuf; cbuf.reserve(wbuf.size());
			utf8::unchecked::utf16to8(wbuf.begin(), wbuf.end(), std::back_inserter(cbuf));

			for (size_t i = 0; i < cbuf.size();)
				i += languages.emplace_back(&cbuf[i]).size() + 1;

			if (!languages.empty() && languages.back().empty())
				languages.pop_back();
		}
	}

	static const std::vector<std::string> availables = get_languages();

	if (const auto it = std::find_if(languages.cbegin(), languages.cend(),
			[](const std::string &lang) { return std::find(availables.cbegin(), availables.cend(), lang) != availables.cend(); });
		it != languages.cend())
		return *it;
	else
		return std::string();
}
std::string reshade::resources::set_current_language(const std::string &language)
{
	std::string previous;
	if (ULONG num = 0, size = 0;
		GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, nullptr, &size))
	{
		std::wstring languages; languages.resize(size);
		if (previous.reserve(size);
		GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, languages.data(), &size))
			utf8::unchecked::utf16to8(languages.cbegin(), languages.cend(), std::back_inserter(previous));
	}

	std::wstring current;
	if (language.empty() || language.front() == '\0')
	{
		if (ULONG num = 0, size = 0;
			SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, L"\0", nullptr) &&
			GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, nullptr, &size))
		{
			if (current.resize(size);
				GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, current.data(), &size))
				current.resize(std::distance(current.cbegin(), std::find(current.cbegin(), current.cend(), L'\0')));
			else
				current.clear();
		}
	}
	else
	{
		utf8::unchecked::utf8to16(language.cbegin(), language.cend(), std::back_inserter(current));
	}

	if (!current.empty())
	{
		current.append(L"\0en-US\0", 7);
		SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, current.c_str(), nullptr);
	}

	return previous;
}
void reshade::resources::unset_current_language(const std::string &previous)
{
	std::wstring languages; languages.reserve(previous.size());
	utf8::unchecked::utf8to16(previous.cbegin(), previous.cend(), std::back_inserter(languages));
	if (languages.empty() || languages.back() != L'\0')
		languages.append(1, L'\0');

	SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, languages.c_str(), nullptr);
}

std::vector<std::string> reshade::resources::get_languages()
{
	// Find a valid string table resource to use as reference to query languages for
	LPCWSTR first_string_table_block = nullptr;
	EnumResourceNamesW(g_module_handle, RT_STRING,
		[](HMODULE, LPCWSTR, LPWSTR lpName, LONG_PTR lParam) -> BOOL {
			*reinterpret_cast<LPCWSTR *>(lParam) = lpName;
			return FALSE;
		}, reinterpret_cast<LONG_PTR>(&first_string_table_block));

	std::vector<std::string> languages;
	EnumResourceLanguages(g_module_handle, RT_STRING, first_string_table_block,
		[](HMODULE, LPCTSTR, LPCTSTR, LANGID wLanguage, LONG_PTR lParam) -> BOOL {
			WCHAR locale_name[16];
			const int length = LCIDToLocaleName(MAKELCID(wLanguage, SORT_DEFAULT), locale_name, ARRAYSIZE(locale_name), 0);
			if (length != 0)
			{
				std::string utf8_locale_name;
				utf8_locale_name.reserve(length);
				// Length includes null-terminator
				utf8::unchecked::utf16to8(locale_name, locale_name + length - 1, std::back_inserter(utf8_locale_name));

				reinterpret_cast<std::vector<std::string> *>(lParam)->push_back(std::move(utf8_locale_name));
			}
			return TRUE;
		}, reinterpret_cast<LONG_PTR>(&languages));

	return languages;
}
#endif
