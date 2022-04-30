/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#if RESHADE_ADDON

#include "d3d9_device.hpp"
#include "d3d9_resource.hpp"
#include "d3d9_impl_type_convert.hpp"

using reshade::d3d9::to_handle;

extern const reshade::api::subresource_box *convert_rect_to_box(const RECT *rect, reshade::api::subresource_box &box);

Direct3DSurface9::Direct3DSurface9(Direct3DDevice9 *device, IDirect3DSurface9 *original, IDirect3DResource9 *parent) :
	resource_impl(device, original, __uuidof(this)),
	_parent(parent)
{
	_orig->GetDesc(&_orig_desc);
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
	if (_parent != nullptr)
		return _parent->AddRef();

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DSurface9::Release()
{
	if (_parent != nullptr)
		return _parent->Release();

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
	if (_parent != nullptr)
		return _parent->QueryInterface(riid, ppContainer);

	return _orig->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
		return D3DERR_INVALIDCALL;

	// Return original surface description in case it was modified by an add-on (see 'device_impl::create_surface_replacement')
	// This is necessary to e.g. fix missing GUI elements in The Elder Scrolls IV: Oblivion and Fallout: New Vegas, since those game verify the format of created surfaces, and returning the modified one can fail that
	*pDesc = _orig_desc;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	const HRESULT hr = _orig->LockRect(pLockedRect, pRect, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		uint32_t subresource;
		reshade::api::subresource_box box;
		reshade::api::subresource_data data;
		data.data = pLockedRect->pBits;
		data.row_pitch = pLockedRect->Pitch;
		data.slice_pitch = 0;

		reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
			_device,
			_device->get_resource_from_view(to_handle(_orig), &subresource),
			subresource,
			convert_rect_to_box(pRect, box),
			reshade::d3d9::convert_access_flags(Flags),
			&data);

		pLockedRect->pBits = data.data;
		pLockedRect->Pitch = data.row_pitch;
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::UnlockRect()
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	uint32_t subresource;

	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(
		_device,
		_device->get_resource_from_view(to_handle(_orig), &subresource),
		subresource);
#endif

	return _orig->UnlockRect();
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::GetDC(HDC *phdc)
{
	return _orig->GetDC(phdc);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface9::ReleaseDC(HDC hdc)
{
	return _orig->ReleaseDC(hdc);
}

Direct3DVolume9::Direct3DVolume9(Direct3DDevice9 *device, IDirect3DVolume9 *original, IDirect3DResource9 *parent) :
	resource_impl(device, original, __uuidof(this)),
	_parent(parent)
{
}

HRESULT STDMETHODCALLTYPE Direct3DVolume9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DVolume9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DVolume9::AddRef()
{
	if (_parent != nullptr)
		return _parent->AddRef();

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DVolume9::Release()
{
	if (_parent != nullptr)
		return _parent->Release();

	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
		delete this;
	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DVolume9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::GetContainer(REFIID riid, void **ppContainer)
{
	if (_parent != nullptr)
		return _parent->QueryInterface(riid, ppContainer);

	return _orig->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::GetDesc(D3DVOLUME_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::LockBox(D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags)
{
	const HRESULT hr = _orig->LockBox(pLockedVolume, pBox, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(pLockedVolume != nullptr);

		uint32_t subresource;
		reshade::api::subresource_data data;
		data.data = pLockedVolume->pBits;
		data.row_pitch = pLockedVolume->RowPitch;
		data.slice_pitch = pLockedVolume->SlicePitch;

		reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
			_device,
			_device->get_resource_from_view(to_handle(_orig), &subresource),
			subresource,
			reinterpret_cast<const reshade::api::subresource_box *>(pBox),
			reshade::d3d9::convert_access_flags(Flags),
			&data);

		pLockedVolume->pBits = data.data;
		pLockedVolume->RowPitch = data.row_pitch;
		pLockedVolume->SlicePitch = data.slice_pitch;
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume9::UnlockBox()
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	uint32_t subresource;

	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(
		_device,
		_device->get_resource_from_view(to_handle(_orig), &subresource),
		subresource);
#endif

	return _orig->UnlockBox();
}

Direct3DTexture9::Direct3DTexture9(Direct3DDevice9 *device, IDirect3DTexture9 *original) :
	resource_impl(device, original, __uuidof(this))
{
	for (UINT level = 0, levels = _orig->GetLevelCount(); level < levels; ++level)
	{
		com_ptr<IDirect3DSurface9> surface;
		if (SUCCEEDED(_orig->GetSurfaceLevel(level, &surface)))
		{
			_surfaces.push_back(new Direct3DSurface9(device, surface.get(), this));
		}
	}
}

HRESULT STDMETHODCALLTYPE Direct3DTexture9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DBaseTexture9) ||
		riid == __uuidof(IDirect3DTexture9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DTexture9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DTexture9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
	{
		for (const auto surface_proxy : _surfaces)
			delete surface_proxy;

		delete this;
	}

	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DTexture9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DTexture9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DTexture9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DTexture9::GetType()
{
	return _orig->GetType();
}

DWORD   STDMETHODCALLTYPE Direct3DTexture9::SetLOD(DWORD LODNew)
{
	return _orig->SetLOD(LODNew);
}
DWORD   STDMETHODCALLTYPE Direct3DTexture9::GetLOD()
{
	return _orig->GetLOD();
}
DWORD   STDMETHODCALLTYPE Direct3DTexture9::GetLevelCount()
{
	return _orig->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType)
{
	return _orig->SetAutoGenFilterType(FilterType);
}
D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE Direct3DTexture9::GetAutoGenFilterType()
{
	return _orig->GetAutoGenFilterType();
}
void    STDMETHODCALLTYPE Direct3DTexture9::GenerateMipSubLevels()
{
	_orig->GenerateMipSubLevels();
}

HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc)
{
	return _orig->GetLevelDesc(Level, pDesc);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel)
{
	if (Level >= _surfaces.size())
		return D3DERR_INVALIDCALL;

	const auto surface_proxy = _surfaces[Level];
	surface_proxy->AddRef();
	*ppSurfaceLevel = surface_proxy;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = _orig->LockRect(Level, pLockedRect, pRect, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		reshade::api::subresource_box box;
		reshade::api::subresource_data data;
		data.data = pLockedRect->pBits;
		data.row_pitch = pLockedRect->Pitch;
		data.slice_pitch = 0;

		reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
			_device,
			to_handle(_orig),
			Level,
			convert_rect_to_box(pRect, box),
			reshade::d3d9::convert_access_flags(Flags),
			&data);

		pLockedRect->pBits = data.data;
		pLockedRect->Pitch = data.row_pitch;
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::UnlockRect(UINT Level)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(_device, to_handle(_orig), Level);
#endif

	return _orig->UnlockRect(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture9::AddDirtyRect(CONST RECT *pDirtyRect)
{
	return _orig->AddDirtyRect(pDirtyRect);
}

Direct3DVolumeTexture9::Direct3DVolumeTexture9(Direct3DDevice9 *device, IDirect3DVolumeTexture9 *original) :
	resource_impl(device, original, __uuidof(this))
{
	for (UINT level = 0, levels = _orig->GetLevelCount(); level < levels; ++level)
	{
		com_ptr<IDirect3DVolume9> volume;
		if (SUCCEEDED(_orig->GetVolumeLevel(level, &volume)))
		{
			_volumes.push_back(new Direct3DVolume9(device, volume.get(), this));
		}
	}
}

HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DBaseTexture9) ||
		riid == __uuidof(IDirect3DVolumeTexture9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DVolumeTexture9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DVolumeTexture9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
	{
		for (const auto volume_proxy : _volumes)
			delete volume_proxy;

		delete this;
	}

	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DVolumeTexture9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DVolumeTexture9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DVolumeTexture9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVolumeTexture9::GetType()
{
	return _orig->GetType();
}

DWORD   STDMETHODCALLTYPE Direct3DVolumeTexture9::SetLOD(DWORD LODNew)
{
	return _orig->SetLOD(LODNew);
}
DWORD   STDMETHODCALLTYPE Direct3DVolumeTexture9::GetLOD()
{
	return _orig->GetLOD();
}
DWORD   STDMETHODCALLTYPE Direct3DVolumeTexture9::GetLevelCount()
{
	return _orig->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType)
{
	return _orig->SetAutoGenFilterType(FilterType);
}
D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE Direct3DVolumeTexture9::GetAutoGenFilterType()
{
	return _orig->GetAutoGenFilterType();
}
void    STDMETHODCALLTYPE Direct3DVolumeTexture9::GenerateMipSubLevels()
{
	_orig->GenerateMipSubLevels();
}

HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc)
{
	return _orig->GetLevelDesc(Level, pDesc);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::GetVolumeLevel(UINT Level, IDirect3DVolume9 **ppVolumeLevel)
{
	if (Level >= _volumes.size())
		return D3DERR_INVALIDCALL;

	const auto volume_proxy = _volumes[Level];
	volume_proxy->AddRef();
	*ppVolumeLevel = volume_proxy;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
	const HRESULT hr = _orig->LockBox(Level, pLockedVolume, pBox, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(pLockedVolume != nullptr);

		reshade::api::subresource_data data;
		data.data = pLockedVolume->pBits;
		data.row_pitch = pLockedVolume->RowPitch;
		data.slice_pitch = pLockedVolume->SlicePitch;

		reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
			_device,
			to_handle(_orig),
			Level,
			reinterpret_cast<const reshade::api::subresource_box *>(pBox),
			reshade::d3d9::convert_access_flags(Flags),
			&data);

		pLockedVolume->pBits = data.data;
		pLockedVolume->RowPitch = data.row_pitch;
		pLockedVolume->SlicePitch = data.slice_pitch;
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::UnlockBox(UINT Level)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(_device, to_handle(_orig), Level);
#endif

	return _orig->UnlockBox(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture9::AddDirtyBox(CONST D3DBOX *pDirtyBox)
{
	return _orig->AddDirtyBox(pDirtyBox);
}

Direct3DCubeTexture9::Direct3DCubeTexture9(Direct3DDevice9 *device, IDirect3DCubeTexture9 *original) :
	resource_impl(device, original, __uuidof(this))
{
	for (UINT level = 0, levels = _orig->GetLevelCount(); level < levels; ++level)
	{
		for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
		{
			com_ptr<IDirect3DSurface9> surface;
			if (SUCCEEDED(_orig->GetCubeMapSurface(face, level, &surface)))
			{
				_surfaces[face].push_back(new Direct3DSurface9(device, surface.get(), this));
			}
		}
	}
}

HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DBaseTexture9) ||
		riid == __uuidof(IDirect3DCubeTexture9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DCubeTexture9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DCubeTexture9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
	{
		for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
			for (const auto surface_proxy : _surfaces[face])
				delete surface_proxy;

		delete this;
	}

	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DCubeTexture9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DCubeTexture9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DCubeTexture9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DCubeTexture9::GetType()
{
	return _orig->GetType();
}

DWORD   STDMETHODCALLTYPE Direct3DCubeTexture9::SetLOD(DWORD LODNew)
{
	return _orig->SetLOD(LODNew);
}
DWORD   STDMETHODCALLTYPE Direct3DCubeTexture9::GetLOD()
{
	return _orig->GetLOD();
}
DWORD   STDMETHODCALLTYPE Direct3DCubeTexture9::GetLevelCount()
{
	return _orig->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType)
{
	return _orig->SetAutoGenFilterType(FilterType);
}
D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE Direct3DCubeTexture9::GetAutoGenFilterType()
{
	return _orig->GetAutoGenFilterType();
}
void    STDMETHODCALLTYPE Direct3DCubeTexture9::GenerateMipSubLevels()
{
	_orig->GenerateMipSubLevels();
}

HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc)
{
	return _orig->GetLevelDesc(Level, pDesc);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9 **ppCubeMapSurface)
{
	if (FaceType >= 6 || Level >= _surfaces[FaceType].size())
		return D3DERR_INVALIDCALL;

	const auto surface_proxy = _surfaces[FaceType][Level];
	surface_proxy->AddRef();
	*ppCubeMapSurface = surface_proxy;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
{
	const HRESULT hr = _orig->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(pLockedRect != nullptr);

		const uint32_t subresource = Level + static_cast<uint32_t>(FaceType) * _orig->GetLevelCount();
		reshade::api::subresource_box box;
		reshade::api::subresource_data data;
		data.data = pLockedRect->pBits;
		data.row_pitch = pLockedRect->Pitch;
		data.slice_pitch = 0;

		reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
			_device,
			to_handle(_orig),
			subresource,
			convert_rect_to_box(pRect, box),
			reshade::d3d9::convert_access_flags(Flags),
			&data);

		pLockedRect->pBits = data.data;
		pLockedRect->Pitch = data.row_pitch;
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const uint32_t subresource = Level + static_cast<uint32_t>(FaceType) * _orig->GetLevelCount();

	reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(_device, to_handle(_orig), subresource);
#endif

	return _orig->UnlockRect(FaceType, Level);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture9::AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect)
{
	return _orig->AddDirtyRect(FaceType, pDirtyRect);
}

Direct3DVertexBuffer9::Direct3DVertexBuffer9(Direct3DDevice9 *device, IDirect3DVertexBuffer9 *original) :
	resource_impl(device, original, __uuidof(this))
{
}

HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DVertexBuffer9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DVertexBuffer9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DVertexBuffer9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
		delete this;
	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DVertexBuffer9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DVertexBuffer9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DVertexBuffer9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVertexBuffer9::GetType()
{
	return _orig->GetType();
}

HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = _orig->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(ppbData != nullptr);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			_device,
			to_handle(_orig),
			OffsetToLock,
			SizeToLock != 0 ? SizeToLock : UINT64_MAX,
			reshade::d3d9::convert_access_flags(Flags),
			ppbData);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::Unlock()
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(_device, to_handle(_orig));
#endif

	return _orig->Unlock();;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::GetDesc(D3DVERTEXBUFFER_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}

Direct3DIndexBuffer9::Direct3DIndexBuffer9(Direct3DDevice9 *device, IDirect3DIndexBuffer9 *original) :
	resource_impl(device, original, __uuidof(this))
{
}

HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DResource9) ||
		riid == __uuidof(IDirect3DIndexBuffer9))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DIndexBuffer9::AddRef()
{
	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DIndexBuffer9::Release()
{
	const ULONG ref_orig = _orig->Release();
	if (ref_orig == 0)
		delete this;
	return ref_orig;
}

HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	return _device->QueryInterface(IID_PPV_ARGS(ppDevice));
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _orig->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _orig->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::FreePrivateData(REFGUID refguid)
{
	return _orig->FreePrivateData(refguid);
}
DWORD   STDMETHODCALLTYPE Direct3DIndexBuffer9::SetPriority(DWORD PriorityNew)
{
	return _orig->SetPriority(PriorityNew);
}
DWORD   STDMETHODCALLTYPE Direct3DIndexBuffer9::GetPriority()
{
	return _orig->GetPriority();
}
void    STDMETHODCALLTYPE Direct3DIndexBuffer9::PreLoad()
{
	_orig->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DIndexBuffer9::GetType()
{
	return _orig->GetType();
}

HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
	const HRESULT hr = _orig->Lock(OffsetToLock, SizeToLock, ppbData, Flags);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		assert(ppbData != nullptr);

		reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
			_device,
			to_handle(_orig),
			OffsetToLock,
			SizeToLock != 0 ? SizeToLock : UINT64_MAX,
			reshade::d3d9::convert_access_flags(Flags),
			ppbData);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::Unlock()
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(_device, to_handle(_orig));
#endif

	return _orig->Unlock();
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}

#endif
