/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hook_manager.hpp"

#if RESHADE_ADDON >= 2

#undef IDirect3DSurface9_LockRect
#define IDirect3DSurface9_LockRect reshade::hooks::call_vtable<13, HRESULT, IDirect3DSurface9, D3DLOCKED_RECT *, const RECT *, DWORD>
#undef IDirect3DSurface9_UnlockRect
#define IDirect3DSurface9_UnlockRect reshade::hooks::call_vtable<14, HRESULT, IDirect3DSurface9>

#undef IDirect3DVolume9_LockBox
#define IDirect3DVolume9_LockBox reshade::hooks::call_vtable<9, HRESULT, IDirect3DVolume9, D3DLOCKED_BOX *, const D3DBOX *, DWORD>
#undef IDirect3DVolume9_UnlockBox
#define IDirect3DVolume9_UnlockBox reshade::hooks::call_vtable<10, HRESULT, IDirect3DVolume9>

#undef IDirect3DTexture9_LockRect
#define IDirect3DTexture9_LockRect reshade::hooks::call_vtable<19, HRESULT, IDirect3DTexture9, UINT, D3DLOCKED_RECT *, const RECT *, DWORD>
#undef IDirect3DTexture9_UnlockRect
#define IDirect3DTexture9_UnlockRect reshade::hooks::call_vtable<20, HRESULT, IDirect3DTexture9, UINT>

#undef IDirect3DVolumeTexture9_LockBox
#define IDirect3DVolumeTexture9_LockBox reshade::hooks::call_vtable<19, HRESULT, IDirect3DVolumeTexture9, UINT, D3DLOCKED_BOX *, const D3DBOX *, DWORD>
#undef IDirect3DVolumeTexture9_UnlockBox
#define IDirect3DVolumeTexture9_UnlockBox reshade::hooks::call_vtable<20, HRESULT, IDirect3DVolumeTexture9, UINT>

#undef IDirect3DCubeTexture9_LockRect
#define IDirect3DCubeTexture9_LockRect reshade::hooks::call_vtable<19, HRESULT, IDirect3DCubeTexture9, D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT *, const RECT *, DWORD>
#undef IDirect3DCubeTexture9_UnlockRect
#define IDirect3DCubeTexture9_UnlockRect reshade::hooks::call_vtable<20, HRESULT, IDirect3DCubeTexture9, D3DCUBEMAP_FACES, UINT>

#undef IDirect3DVertexBuffer9_Lock
#define IDirect3DVertexBuffer9_Lock reshade::hooks::call_vtable<11, HRESULT, IDirect3DVertexBuffer9, UINT, UINT, void **, DWORD>
#undef IDirect3DVertexBuffer9_Unlock
#define IDirect3DVertexBuffer9_Unlock reshade::hooks::call_vtable<12, HRESULT, IDirect3DVertexBuffer9>

#undef IDirect3DIndexBuffer9_Lock
#define IDirect3DIndexBuffer9_Lock reshade::hooks::call_vtable<11, HRESULT, IDirect3DIndexBuffer9, UINT, UINT, void **, DWORD>
#undef IDirect3DIndexBuffer9_Unlock
#define IDirect3DIndexBuffer9_Unlock reshade::hooks::call_vtable<12, HRESULT, IDirect3DIndexBuffer9>

#endif
