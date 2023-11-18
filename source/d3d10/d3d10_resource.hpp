/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

void STDMETHODCALLTYPE ID3D10Resource_GetDevice(ID3D10Resource *pResource, ID3D10Device **ppDevice);

#if RESHADE_ADDON >= 2

HRESULT STDMETHODCALLTYPE ID3D10Buffer_Map(ID3D10Buffer *pResource, D3D10_MAP MapType, UINT MapFlags, void **ppData);
HRESULT STDMETHODCALLTYPE ID3D10Buffer_Unmap(ID3D10Buffer *pResource);

HRESULT STDMETHODCALLTYPE ID3D10Texture1D_Map(ID3D10Texture1D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, void **ppData);
HRESULT STDMETHODCALLTYPE ID3D10Texture1D_Unmap(ID3D10Texture1D *pResource, UINT Subresource);

HRESULT STDMETHODCALLTYPE ID3D10Texture2D_Map(ID3D10Texture2D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, D3D10_MAPPED_TEXTURE2D *pMappedTex2D);
HRESULT STDMETHODCALLTYPE ID3D10Texture2D_Unmap(ID3D10Texture2D *pResource, UINT Subresource);

HRESULT STDMETHODCALLTYPE ID3D10Texture3D_Map(ID3D10Texture3D *pResource, UINT Subresource, D3D10_MAP MapType, UINT MapFlags, D3D10_MAPPED_TEXTURE3D *pMappedTex3D);
HRESULT STDMETHODCALLTYPE ID3D10Texture3D_Unmap(ID3D10Texture3D *pResource, UINT Subresource);

#endif
