/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON

#include "d3d9_device.hpp"
#include "d3d9_resource.hpp"

Direct3DDepthStencilSurface9::Direct3DDepthStencilSurface9(Direct3DDevice9 *device, IDirect3DSurface9 *original, const D3DSURFACE_DESC &desc) :
	_orig(original),
	_device(device),
	_orig_desc(desc)
{
#if RESHADE_ADDON >= 2
	const auto device_proxy = device;
	_orig->SetPrivateData(__uuidof(device_proxy), &device_proxy, sizeof(device_proxy), 0);
#endif
}

HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::QueryInterface(REFIID riid, void **ppvObj)
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
ULONG   STDMETHODCALLTYPE Direct3DDepthStencilSurface9::AddRef()
{
	if (_ref != 0)
		_orig->AddRef();
	else
		assert(this == _device->_auto_depth_stencil);

	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE Direct3DDepthStencilSurface9::Release()
{
	// Star Wars: The Force Unleashed incorrectly releases the auto depth-stencil it retrieved via 'IDirect3DDevice9::GetDepthStencilSurface' twice, so detect this
	if (_ref == 0)
		return 0;

	const ULONG ref = InterlockedDecrement(&_ref);

	if (this == _device->_auto_depth_stencil)
	{
		if (ref != 0)
			_orig->Release();
	}
	else
	{
		if (_orig->Release() == 0)
		{
			// Make sure there won't be an attempt to release this surface again because it's still current despite a reference count of zero (happens in Star Wars: The Force Unleashed 2)
			if (this == _device->_current_depth_stencil)
				_device->_current_depth_stencil.release();

			delete this;
		}
	}

	return ref;
}

HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return IDirect3DSurface9_SetPrivateData(_orig, refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return IDirect3DSurface9_GetPrivateData(_orig, refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::FreePrivateData(REFGUID refguid)
{
	return IDirect3DSurface9_FreePrivateData(_orig, refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DDepthStencilSurface9::SetPriority(DWORD PriorityNew)
{
	return IDirect3DSurface9_SetPriority(_orig, PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetPriority()
{
	return IDirect3DSurface9_GetPriority(_orig);
}
void    STDMETHODCALLTYPE Direct3DDepthStencilSurface9::PreLoad()
{
	IDirect3DSurface9_PreLoad(_orig);
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetType()
{
	return IDirect3DSurface9_GetType(_orig);
}

HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetContainer(REFIID riid, void **ppContainer)
{
	return IDirect3DSurface9_GetContainer(_orig, riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
		return D3DERR_INVALIDCALL;

	// Return original surface description in case it was modified by an add-on
	// This is necessary to e.g. fix missing GUI elements in The Elder Scrolls IV: Oblivion and Fallout: New Vegas, since those game verify the format of created surfaces, and returning the modified one can fail that
	*pDesc = _orig_desc;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return IDirect3DSurface9_LockRect(_orig, pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::UnlockRect()
{
	return IDirect3DSurface9_UnlockRect(_orig);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::GetDC(HDC *phdc)
{
	return IDirect3DSurface9_GetDC(_orig, phdc);
}
HRESULT STDMETHODCALLTYPE Direct3DDepthStencilSurface9::ReleaseDC(HDC hdc)
{
	return IDirect3DSurface9_ReleaseDC(_orig, hdc);
}

#endif

#if RESHADE_ADDON >= 2

#include "d3d9_impl_type_convert.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"

using reshade::d3d9::to_handle;

extern const reshade::api::subresource_box *convert_rect_to_box(const RECT *rect, reshade::api::subresource_box &box);

HRESULT STDMETHODCALLTYPE IDirect3DSurface9_LockRect(IDirect3DSurface9 *pSurface, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DSurface9_LockRect, reshade::hooks::vtable_from_instance(pSurface) + 13)(pSurface, pLockedRect, pRect, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pSurface))
		{
			uint32_t subresource;
			const reshade::api::resource resource = device_proxy->get_resource_from_view(to_handle(pSurface), &subresource);

			reshade::api::subresource_box box;
			reshade::api::subresource_data data;
			data.data = pLockedRect->pBits;
			data.row_pitch = pLockedRect->Pitch;
			data.slice_pitch = 0;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				resource,
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
		const reshade::api::resource resource = device_proxy->get_resource_from_view(to_handle(pSurface), &subresource);

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, resource, subresource);
	}

	return reshade::hooks::call(IDirect3DSurface9_UnlockRect, reshade::hooks::vtable_from_instance(pSurface) + 14)(pSurface);
}

HRESULT STDMETHODCALLTYPE IDirect3DVolume9_LockBox(IDirect3DVolume9 *pVolume, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DVolume9_LockBox, reshade::hooks::vtable_from_instance(pVolume) + 9)(pVolume, pLockedVolume, pBox, Flags);
	if (SUCCEEDED(hr))
	{
		assert(pLockedVolume != nullptr);

		if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVolume))
		{
			uint32_t subresource;
			const reshade::api::resource resource = device_proxy->get_resource_from_view(to_handle(pVolume), &subresource);

			reshade::api::subresource_data data;
			data.data = pLockedVolume->pBits;
			data.row_pitch = pLockedVolume->RowPitch;
			data.slice_pitch = pLockedVolume->SlicePitch;

			reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
				device_proxy,
				resource,
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
		const reshade::api::resource resource = device_proxy->get_resource_from_view(to_handle(pVolume), &subresource);

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, resource, subresource);
	}

	return reshade::hooks::call(IDirect3DVolume9_UnlockBox, reshade::hooks::vtable_from_instance(pVolume) + 10)(pVolume);
}

HRESULT STDMETHODCALLTYPE IDirect3DTexture9_LockRect(IDirect3DTexture9 *pTexture, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	com_ptr<IDirect3DSurface9> surface;
	if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(pTexture, Level, &surface)))
		return IDirect3DSurface9_LockRect(surface.get(), pLockedRect, pRect, Flags);

	assert(false);
	return D3DERR_INVALIDCALL;
}
HRESULT STDMETHODCALLTYPE IDirect3DTexture9_UnlockRect(IDirect3DTexture9 *pTexture, UINT Level)
{
	com_ptr<IDirect3DSurface9> surface;
	if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(pTexture, Level, &surface)))
		return IDirect3DSurface9_UnlockRect(surface.get());

	assert(false);
	return D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_LockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
	com_ptr<IDirect3DVolume9> volume;
	if (SUCCEEDED(IDirect3DVolumeTexture9_GetVolumeLevel(pTexture, Level, &volume)))
		return IDirect3DVolume9_LockBox(volume.get(), pLockedVolume, pBox, Flags);

	assert(false);
	return D3DERR_INVALIDCALL;
}
HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_UnlockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level)
{
	com_ptr<IDirect3DVolume9> volume;
	if (SUCCEEDED(IDirect3DVolumeTexture9_GetVolumeLevel(pTexture, Level, &volume)))
		return IDirect3DVolume9_UnlockBox(volume.get());

	assert(false);
	return D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_LockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	com_ptr<IDirect3DSurface9> surface;
	if (SUCCEEDED(IDirect3DCubeTexture9_GetCubeMapSurface(pTexture, FaceType, Level, &surface)))
		return IDirect3DSurface9_LockRect(surface.get(), pLockedRect, pRect, Flags);

	assert(false);
	return D3DERR_INVALIDCALL;
}
HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_UnlockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level)
{
	com_ptr<IDirect3DSurface9> surface;
	if (SUCCEEDED(IDirect3DCubeTexture9_GetCubeMapSurface(pTexture, FaceType, Level, &surface)))
		return IDirect3DSurface9_UnlockRect(surface.get());

	assert(false);
	return D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Lock(IDirect3DVertexBuffer9 *pVertexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DVertexBuffer9_Lock, reshade::hooks::vtable_from_instance(pVertexBuffer) + 11)(pVertexBuffer, OffsetToLock, SizeToLock, ppbData, Flags);
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

	return reshade::hooks::call(IDirect3DVertexBuffer9_Unlock, reshade::hooks::vtable_from_instance(pVertexBuffer) + 12)(pVertexBuffer);
}

HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Lock(IDirect3DIndexBuffer9 *pIndexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = reshade::hooks::call(IDirect3DIndexBuffer9_Lock, reshade::hooks::vtable_from_instance(pIndexBuffer) + 11)(pIndexBuffer, OffsetToLock, SizeToLock, ppbData, Flags);
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

	return reshade::hooks::call(IDirect3DIndexBuffer9_Unlock, reshade::hooks::vtable_from_instance(pIndexBuffer) + 12)(pIndexBuffer);
}

#endif
