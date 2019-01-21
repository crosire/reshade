/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <dxgi1_5.h>

#include "d3d10/d3d10_device.hpp"
#include "d3d11/d3d11_device.hpp"

namespace reshade { class runtime; }

struct __declspec(uuid("CB285C3B-3677-4332-98C7-D6339B9782B1")) DXGIDevice;
struct __declspec(uuid("1F445F9F-9887-4C4C-9055-4E3BADAFCCA8")) DXGISwapChain;
