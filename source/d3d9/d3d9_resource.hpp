/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON

struct Direct3DDevice9;

struct DECLSPEC_UUID("0F433AEB-B389-4589-81A7-9DB59F34CB55") Direct3DDepthStencilSurface9 final : IDirect3DSurface9
{
	Direct3DDepthStencilSurface9(Direct3DDevice9 *device, IDirect3DSurface9 *original, const D3DSURFACE_DESC &desc);

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DResource9
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) override;
	HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
	DWORD   STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
	DWORD   STDMETHODCALLTYPE GetPriority() override;
	void    STDMETHODCALLTYPE PreLoad() override;
	D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
	#pragma endregion
	#pragma region IDirect3DSurface9
	HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer) override;
	HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE UnlockRect() override;
	HRESULT STDMETHODCALLTYPE GetDC(HDC *phdc) override;
	HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hdc) override;
	#pragma endregion

	IDirect3DSurface9 *_orig;
	ULONG _ref = 1;

	Direct3DDevice9 *const _device;
	const  D3DSURFACE_DESC _orig_desc;
};

#endif

#if RESHADE_ADDON >= 2

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
