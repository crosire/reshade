/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_resources.hpp"
#include <Windows.h>

extern HMODULE g_module_handle;

reshade::resources::data_resource reshade::resources::load_data_resource(unsigned int id)
{
	const HRSRC info = FindResource(g_module_handle, MAKEINTRESOURCE(id), RT_RCDATA);
	const HGLOBAL handle = LoadResource(g_module_handle, info);

	data_resource result;
	result.data = LockResource(handle);
	result.data_size = SizeofResource(g_module_handle, info);

	return result;
}

reshade::resources::image_resource reshade::resources::load_image_resource(unsigned int id)
{
	DIBSECTION dib = {};
	const HANDLE handle = LoadImage(g_module_handle, MAKEINTRESOURCE(id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	GetObject(handle, sizeof(dib), &dib);

	image_resource result;
	result.width = dib.dsBm.bmWidth;
	result.height = dib.dsBm.bmHeight;
	result.bits_per_pixel = dib.dsBm.bmBitsPixel;
	result.data = dib.dsBm.bmBits;
	result.data_size = dib.dsBmih.biSizeImage;

	return result;
}
