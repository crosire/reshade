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
	ULONG num = 0, size = 0;
	if (!GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, nullptr, &size))
		return std::string();
	std::vector<WCHAR> languages(size);
	if (!GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, languages.data(), &size) || num == 0)
		return std::string();

	std::string language;
	// Extract first language from the double null-terminated multi-string buffer
	utf8::unchecked::utf16to8(languages.begin(), std::find(languages.begin(), languages.end(), L'\0'), std::back_inserter(language));
	return language;
}
std::string reshade::resources::set_current_language(const std::string &language)
{
	std::string prev_language;
	if (ULONG num = 0, size = 0;
		GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, nullptr, &size))
	{
		std::wstring previous; previous.resize(size);
		if (GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, previous.data(), &size))
			utf8::unchecked::utf16to8(previous.cbegin(), previous.cend(), std::back_inserter(prev_language));
	}

	std::wstring languages;
	if (language.empty() || language.front() == '\0')
	{
		if (ULONG num = 0, size = 0;
			SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, L"\0", nullptr) &&
			GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, nullptr, &size))
		{
			if (languages.resize(size);
				GetThreadPreferredUILanguages(MUI_LANGUAGE_NAME | MUI_UI_FALLBACK, &num, languages.data(), &size))
				languages.resize(std::distance(languages.cbegin(), std::find(languages.cbegin(), languages.cend(), L'\0')));
		}
	}
	else
	{
		utf8::unchecked::utf8to16(language.cbegin(), language.cend(), std::back_inserter(languages));
	}

	languages.append(L"\0en-US\0", 7);
	SetThreadPreferredUILanguages(MUI_LANGUAGE_NAME, languages.c_str(), nullptr);

	return prev_language;
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
