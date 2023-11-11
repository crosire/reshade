/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

HRESULT STDMETHODCALLTYPE ID3D12Resource_GetDevice(ID3D12Resource *pResource, REFIID riid, void **ppvDevice);

#if RESHADE_ADDON >= 2

HRESULT STDMETHODCALLTYPE ID3D12Resource_Map(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData);
HRESULT STDMETHODCALLTYPE ID3D12Resource_Unmap(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pWrittenRange);

#endif
