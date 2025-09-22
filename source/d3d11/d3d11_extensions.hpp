/*
 * Copyright (C) 2025 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d11.h>

using NvAPI_D3D11_BeginUAVOverlap_t = enum NvAPI_Status(*)(IUnknown *pDeviceOrContext);
using NvAPI_D3D11_EndUAVOverlap_t = enum NvAPI_Status(*)(IUnknown *pDeviceOrContext);

extern NvAPI_D3D11_BeginUAVOverlap_t NvAPI_D3D11_BeginUAVOverlap;
extern NvAPI_D3D11_EndUAVOverlap_t NvAPI_D3D11_EndUAVOverlap;

namespace reshade::d3d11
{
	void load_driver_extensions();
	void unload_driver_extensions();
}
