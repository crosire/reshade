/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON

#include "d3d9_device.hpp"
#include "d3d9_resource.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

Direct3DSurface9::Direct3DSurface9(Direct3DDevice9 *device, IDirect3DSurface9 *original, const D3DSURFACE_DESC &desc) :
	_orig(original),
	_device(device),
	_orig_desc(desc)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const auto device_proxy = device;
	_orig->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);
#endif
	const auto surface_proxy = this;
	_orig->SetPrivateData(__uuidof(surface_proxy), &surface_proxy, sizeof(surface_proxy), 0);
}

HRESULT STDMETHODCALLTYPE Direct3DSurface9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DSurface9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DSurface9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DSurface9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
		delete this;
	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DSurface9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DSurface9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DSurface9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DSurface9::GetType()
{
	return _orig->GetType();
}

HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetContainer(REFIID riid, void **ppContainer)
{
	return _orig->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
		return D3DERR_INVALIDCALL;

	// Return original surface description in case it was modified by an add-on
	// This is necessary to e.g. fix missing GUI elements in The Elder Scrolls IV: Oblivion and Fallout: New Vegas, since those game verify the format of created surfaces, and returning the modified one can fail that
	*pDesc = _orig_desc;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	return IDirect3DSurface9_LockRect(_orig, pLockedRect, pRect, Flags);
#else
	return _orig->LockRect(pLockedRect, pRect, Flags);
#endif
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::UnlockRect()
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	return IDirect3DSurface9_UnlockRect(_orig);
#else
	return _orig->UnlockRect();
#endif
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDC(HDC *phdc)
{
	return _orig->GetDC(phdc);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::ReleaseDC(HDC hdc)
{
	return _orig->ReleaseDC(hdc);
}

#endif

#if RESHADE_ADDON && !RESHADE_ADDON_LITE

using reshade::d3d9::to_handle;

extern const reshade::api::subresource_box *convert_rect_to_box(const RECT *rect, reshade::api::subresource_box &box);

HRESULT STDMETHODCALLTYPE IDirect3DSurface9_LockRect(IDirect3DSurface9 *pSurface, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DSurface9_LockRect, vtable_from_instance(pSurface) + 13)(pSurface, pLockedRect, pRect, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pSurface))
		{
			uint32_t subresource;
			reshade::api::subresource_box box;
			reshade::api::subresource_data data;
			data.data = pLockedRect->pBits;
			data.row_pitch = pLockedRect->Pitch;
			data.slice_pitch = 0;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				device_proxy->get_resource_from_view(to_handle(pSurface), &subresource),
				subresource,
				convert_rect_to_box(pRect, box),
				reshade::d3d9::convert_access_flags(Flags),
				&data);

			pLockedRect->pBits = data.data;
			pLockedRect->Pitch = data.row_pitch;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DSurface9_UnlockRect(IDirect3DSurface9 *pSurface)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pSurface))
	{
		uint32_t subresource;

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(
			device_proxy,
			device_proxy->get_resource_from_view(to_handle(pSurface), &subresource),
			subresource);
	}

	return reshade::hooks::call(IDirect3DSurface9_UnlockRect, vtable_from_instance(pSurface) + 14)(pSurface);
}

HRESULT STDMETHODCALLTYPE IDirect3DVolume9_LockBox(IDirect3DVolume9 *pVolume, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DVolume9_LockBox, vtable_from_instance(pVolume) + 9)(pVolume, pLockedVolume, pBox, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedVolume != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVolume))
		{
			uint32_t subresource;
			reshade::api::subresource_data data;
			data.data = pLockedVolume->pBits;
			data.row_pitch = pLockedVolume->RowPitch;
			data.slice_pitch = pLockedVolume->SlicePitch;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				device_proxy->get_resource_from_view(to_handle(pVolume), &subresource),
				subresource,
				reinterpret_cast<const reshade::api::subresource_box *>(pBox),
				reshade::d3d9::convert_access_flags(Flags),
				&data);

			pLockedVolume->pBits = data.data;
			pLockedVolume->RowPitch = data.row_pitch;
			pLockedVolume->SlicePitch = data.slice_pitch;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DVolume9_UnlockBox(IDirect3DVolume9 *pVolume)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVolume))
	{
		uint32_t subresource;

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(
			device_proxy,
			device_proxy->get_resource_from_view(to_handle(pVolume), &subresource),
			subresource);
	}

	return reshade::hooks::call(IDirect3DVolume9_UnlockBox, vtable_from_instance(pVolume) + 10)(pVolume);
}

HRESULT STDMETHODCALLTYPE IDirect3DTexture9_LockRect(IDirect3DTexture9 *pTexture, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DTexture9_LockRect, vtable_from_instance(pTexture) + 19)(pTexture, Level, pLockedRect, pRect, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
		{
			reshade::api::subresource_box box;
			reshade::api::subresource_data data;
			data.data = pLockedRect->pBits;
			data.row_pitch = pLockedRect->Pitch;
			data.slice_pitch = 0;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				to_handle(pTexture),
				Level,
				convert_rect_to_box(pRect, box),
				reshade::d3d9::convert_access_flags(Flags),
				&data);

			pLockedRect->pBits = data.data;
			pLockedRect->Pitch = data.row_pitch;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DTexture9_UnlockRect(IDirect3DTexture9 *pTexture, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), Level);
	}

	return reshade::hooks::call(IDirect3DTexture9_UnlockRect, vtable_from_instance(pTexture) + 20)(pTexture, Level);
}

HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_LockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DVolumeTexture9_LockBox, vtable_from_instance(pTexture) + 19)(pTexture, Level, pLockedVolume, pBox, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedVolume != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
		{
			reshade::api::subresource_data data;
			data.data = pLockedVolume->pBits;
			data.row_pitch = pLockedVolume->RowPitch;
			data.slice_pitch = pLockedVolume->SlicePitch;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				to_handle(pTexture),
				Level,
				reinterpret_cast<const reshade::api::subresource_box *>(pBox),
				reshade::d3d9::convert_access_flags(Flags),
				&data);

			pLockedVolume->pBits = data.data;
			pLockedVolume->RowPitch = data.row_pitch;
			pLockedVolume->SlicePitch = data.slice_pitch;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_UnlockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), Level);
	}

	return reshade::hooks::call(IDirect3DVolumeTexture9_UnlockBox, vtable_from_instance(pTexture) + 20)(pTexture, Level);
}

HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_LockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DCubeTexture9_LockRect, vtable_from_instance(pTexture) + 19)(pTexture, FaceType, Level, pLockedRect, pRect, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
		{
			const uint32_t subresource = Level + static_cast<uint32_t>(FaceType) * pTexture->GetLevelCount();
			reshade::api::subresource_box box;
			reshade::api::subresource_data data;
			data.data = pLockedRect->pBits;
			data.row_pitch = pLockedRect->Pitch;
			data.slice_pitch = 0;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				to_handle(pTexture),
				subresource,
				convert_rect_to_box(pRect, box),
				reshade::d3d9::convert_access_flags(Flags),
				&data);

			pLockedRect->pBits = data.data;
			pLockedRect->Pitch = data.row_pitch;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_UnlockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		const uint32_t subresource = Level + static_cast<uint32_t>(FaceType) * pTexture->GetLevelCount();

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), subresource);
	}

	return reshade::hooks::call(IDirect3DCubeTexture9_UnlockRect, vtable_from_instance(pTexture) + 20)(pTexture, FaceType, Level);
}

HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Lock(IDirect3DVertexBuffer9 *pVertexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DVertexBuffer9_Lock, vtable_from_instance(pVertexBuffer) + 11)(pVertexBuffer, OffsetToLock, SizeToLock, ppbData, Flags);
	if (SUCCEEDED(hr))
	{
		assert(ppbData != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVertexBuffer))
		{
			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				device_proxy,
				to_handle(pVertexBuffer),
				OffsetToLock,
				SizeToLock != 0 ? SizeToLock : UINT64_MAX,
				reshade::d3d9::convert_access_flags(Flags),
				ppbData);
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Unlock(IDirect3DVertexBuffer9 *pVertexBuffer)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVertexBuffer))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pVertexBuffer));
	}

	return reshade::hooks::call(IDirect3DVertexBuffer9_Unlock, vtable_from_instance(pVertexBuffer) + 12)(pVertexBuffer);
}

HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Lock(IDirect3DIndexBuffer9 *pIndexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DIndexBuffer9_Lock, vtable_from_instance(pIndexBuffer) + 11)(pIndexBuffer, OffsetToLock, SizeToLock, ppbData, Flags);
	if (SUCCEEDED(hr))
	{
		assert(ppbData != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pIndexBuffer))
		{
			reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
				device_proxy,
				to_handle(pIndexBuffer),
				OffsetToLock,
				SizeToLock != 0 ? SizeToLock : UINT64_MAX,
				reshade::d3d9::convert_access_flags(Flags),
				ppbData);
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Unlock(IDirect3DIndexBuffer9 *pIndexBuffer)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pIndexBuffer))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pIndexBuffer));
	}

	return reshade::hooks::call(IDirect3DIndexBuffer9_Unlock, vtable_from_instance(pIndexBuffer) + 12)(pIndexBuffer);
}

#endif
