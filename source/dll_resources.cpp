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
