/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d11_extensions.hpp"

NvAPI_D3D11_BeginUAVOverlap_t NvAPI_D3D11_BeginUAVOverlap = nullptr;
NvAPI_D3D11_EndUAVOverlap_t NvAPI_D3D11_EndUAVOverlap = nullptr;

void reshade::d3d11::load_driver_extensions()
{
	if (const HMODULE nvapi_module = GetModuleHandleW(
#ifndef _WIN64
			L"nvapi.dll"
#else
			L"nvapi64.dll"
#endif
			))
	{
		const auto nvapi_QueryInterface = reinterpret_cast<void *(*)(unsigned long)>(GetProcAddress(nvapi_module, "nvapi_QueryInterface"));
		if (nvapi_QueryInterface != nullptr)
		{
			NvAPI_D3D11_BeginUAVOverlap = reinterpret_cast<NvAPI_D3D11_BeginUAVOverlap_t>(nvapi_QueryInterface(0x65B93CA8));
			NvAPI_D3D11_EndUAVOverlap = reinterpret_cast<NvAPI_D3D11_EndUAVOverlap_t>(nvapi_QueryInterface(0x2216A357));
		}
	}
}
