/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <d3d9.h>

#if RESHADE_ADDON
#undef IDirect3DSurface9_GetDesc

HRESULT STDMETHODCALLTYPE IDirect3DSurface9_GetDesc(IDirect3DSurface9 *pSurface, D3DSURFACE_DESC *pDesc);
#endif

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
#undef IDirect3DSurface9_LockRect
#undef IDirect3DSurface9_UnlockRect

HRESULT STDMETHODCALLTYPE IDirect3DSurface9_LockRect(IDirect3DSurface9 *pSurface, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DSurface9_UnlockRect(IDirect3DSurface9 *pSurface);

#undef IDirect3DVolume9_LockBox
#undef IDirect3DVolume9_UnlockBox

HRESULT STDMETHODCALLTYPE IDirect3DVolume9_LockBox(IDirect3DVolume9 *pVolume, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DVolume9_UnlockBox(IDirect3DVolume9 *pVolume);

#undef IDirect3DTexture9_LockRect
#undef IDirect3DTexture9_UnlockRect

HRESULT STDMETHODCALLTYPE IDirect3DTexture9_LockRect(IDirect3DTexture9 *pTexture, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DTexture9_UnlockRect(IDirect3DTexture9 *pTexture, UINT Level);

#undef IDirect3DVolumeTexture9_LockBox
#undef IDirect3DVolumeTexture9_UnlockBox

HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_LockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_UnlockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level);

#undef IDirect3DCubeTexture9_LockRect
#undef IDirect3DCubeTexture9_UnlockRect

HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_LockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_UnlockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level);

#undef IDirect3DVertexBuffer9_Lock
#undef IDirect3DVertexBuffer9_Unlock

HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Lock(IDirect3DVertexBuffer9 *pVertexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Unlock(IDirect3DVertexBuffer9 *pVertexBuffer);

#undef IDirect3DIndexBuffer9_Lock
#undef IDirect3DIndexBuffer9_Unlock

HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Lock(IDirect3DIndexBuffer9 *pIndexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags);
HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Unlock(IDirect3DIndexBuffer9 *pIndexBuffer);

#endif
