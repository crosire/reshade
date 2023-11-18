/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hook_manager.hpp"

#define ID3D12Resource_GetDevice reshade::hooks::call_vtable<7, HRESULT, ID3D12Resource, REFIID, void **>

#if RESHADE_ADDON >= 2

#define ID3D12Resource_Map reshade::hooks::call_vtable<8, HRESULT, ID3D12Resource, UINT, const D3D12_RANGE *, void **>
#define ID3D12Resource_Unmap reshade::hooks::call_vtable<9, void, ID3D12Resource, UINT, const D3D12_RANGE *>

#else

#define ID3D12Resource_Map(p, a, b, c) (p)->Map(a, b, c)
#define ID3D12Resource_Unmap(p, a, b) (p)->Unmap(a, b)

#endif
