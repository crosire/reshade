/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dll_resources.hpp"
#include <Windows.h>

extern HMODULE g_module_handle;

reshade::resources::data_resource reshade::resources::load_data_resource(unsigned short id)
{
	const HRSRC info = FindResource(g_module_handle, MAKEINTRESOURCE(id), RT_RCDATA);
	const HGLOBAL handle = LoadResource(g_module_handle, info);

	data_resource result;
	result.data = LockResource(handle);
	result.data_size = SizeofResource(g_module_handle, info);

	return result;
}
