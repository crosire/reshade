/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hook_manager.hpp"

#define ID3D11Resource_GetDevice reshade::hooks::call_vtable<3, HRESULT, ID3D11Resource, ID3D11Device **>
