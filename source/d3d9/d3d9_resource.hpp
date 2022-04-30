/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#if RESHADE_ADDON

#include <d3d9.h>

struct Direct3DDevice9;

namespace reshade::d3d9
{
	template <typename T>
	class resource_impl : public T
	{
	protected:
		resource_impl(Direct3DDevice9 *device, T *resource, REFIID proxy_iid) :
			_orig(resource), _device(device)
		{
			assert(_orig != nullptr && _device != nullptr);

			const auto resource_proxy = this;
			_orig->SetPrivateData(proxy_iid, &resource_proxy, sizeof(resource_proxy), 0);

			// Do not increase device reference count for surfaces, since those may refer to the back buffer or auto depth-stencil surface
			if constexpr (!std::is_same_v<T, IDirect3DSurface9> && !std::is_same_v<T, IDirect3DVolume9>)
				_device->AddRef();
		}
		~resource_impl()
		{
			if constexpr (!std::is_same_v<T, IDirect3DSurface9> && !std::is_same_v<T, IDirect3DVolume9>)
				_device->Release();
		}

	public:
		T *const _orig;
		Direct3DDevice9 *const _device;
	};

	template <typename T>
	inline auto to_orig(T *ptr)
	{
		return ptr != nullptr ? static_cast<resource_impl<T> *>(ptr)->_orig : nullptr;
	}

	template <typename ProxyT, typename T>
	inline auto replace_with_resource_proxy(Direct3DDevice9 *device, T **out_resource) -> ProxyT *
	{
		static_assert(std::is_base_of_v<resource_impl<T>, ProxyT>);

		const auto resource = *out_resource;
		if (resource == nullptr)
			return nullptr;

		ProxyT *resource_proxy = nullptr;
		DWORD size = sizeof(resource_proxy);
		if (FAILED(resource->GetPrivateData(__uuidof(ProxyT), &resource_proxy, &size)))
		{
			// This should only happen for resources that were just created and therefore have a reference count of 1
			assert((std::is_same_v<T, IDirect3DSurface9>) || (resource->AddRef(), resource->Release()) == 1);

			resource_proxy = new ProxyT(device, resource);
			assert(SUCCEEDED(resource->GetPrivateData(__uuidof(ProxyT), &resource_proxy, &size)));
		}

		*out_resource = resource_proxy;
		return resource_proxy;
	}
}

struct DECLSPEC_UUID("0F433AEB-B389-4589-81A7-9DB59F34CB55") Direct3DSurface9 final : reshade::d3d9::resource_impl<IDirect3DSurface9>
{
	Direct3DSurface9(Direct3DDevice9 *device, IDirect3DSurface9 *original, IDirect3DResource9 *parent = nullptr);

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

	IDirect3DResource9 *const _parent;
	D3DSURFACE_DESC _orig_desc = {};
};

struct DECLSPEC_UUID("264D345D-81E7-4B0D-AB6D-B7A947766844") Direct3DVolume9 final : reshade::d3d9::resource_impl<IDirect3DVolume9>
{
	Direct3DVolume9(Direct3DDevice9 *device, IDirect3DVolume9 *original, IDirect3DResource9 *parent = nullptr);

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DVolume9
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) override;
	HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
	HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer) override;
	HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE UnlockBox() override;
	#pragma endregion

	IDirect3DResource9 *const _parent;
};

struct DECLSPEC_UUID("3A260500-2381-4F07-BA32-A01FE863E799") Direct3DTexture9 final : reshade::d3d9::resource_impl<IDirect3DTexture9>
{
	Direct3DTexture9(Direct3DDevice9 *device, IDirect3DTexture9 *original);

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
	#pragma region IDirect3DBaseTexture9
	DWORD   STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
	DWORD   STDMETHODCALLTYPE GetLOD() override;
	DWORD   STDMETHODCALLTYPE GetLevelCount() override;
	HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
	D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override;
	void    STDMETHODCALLTYPE GenerateMipSubLevels() override;
	#pragma endregion
	#pragma region IDirect3DTexture9
	HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9 **ppSurfaceLevel) override;
	HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override;
	HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT *pDirtyRect) override;
	#pragma endregion

	std::vector<Direct3DSurface9 *> _surfaces;
};

struct DECLSPEC_UUID("981C21FC-8A3A-4836-9779-B5F1DBA1613C") Direct3DVolumeTexture9 final : reshade::d3d9::resource_impl<IDirect3DVolumeTexture9>
{
	Direct3DVolumeTexture9(Direct3DDevice9 *device, IDirect3DVolumeTexture9 *original);

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
	#pragma region IDirect3DBaseTexture9
	DWORD   STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
	DWORD   STDMETHODCALLTYPE GetLOD() override;
	DWORD   STDMETHODCALLTYPE GetLevelCount() override;
	HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
	D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override;
	void    STDMETHODCALLTYPE GenerateMipSubLevels() override;
	#pragma endregion
	#pragma region IDirect3DVolumeTexture9
	HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, IDirect3DVolume9 **ppVolumeLevel) override;
	HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level) override;
	HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX *pDirtyBox) override;
	#pragma endregion

	std::vector<Direct3DVolume9 *> _volumes;
};

struct DECLSPEC_UUID("25061384-0807-4695-BC88-486A356855CD") Direct3DCubeTexture9 final : reshade::d3d9::resource_impl<IDirect3DCubeTexture9>
{
	Direct3DCubeTexture9(Direct3DDevice9 *device, IDirect3DCubeTexture9 *original);

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
	#pragma region IDirect3DBaseTexture9
	DWORD   STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
	DWORD   STDMETHODCALLTYPE GetLOD() override;
	DWORD   STDMETHODCALLTYPE GetLevelCount() override;
	HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
	D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override;
	void    STDMETHODCALLTYPE GenerateMipSubLevels() override;
	#pragma endregion
	#pragma region IDirect3DCubeTexture
	HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override;
	HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9 **ppCubeMapSurface) override;
	HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT *pRect, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) override;
	HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect) override;
	#pragma endregion

	std::vector<Direct3DSurface9 *> _surfaces[6];
};

struct DECLSPEC_UUID("64F60FD1-AA1D-4497-BDC7-8B9A2E35B7D1") Direct3DVertexBuffer9 final : reshade::d3d9::resource_impl<IDirect3DVertexBuffer9>
{
	Direct3DVertexBuffer9(Direct3DDevice9 *device, IDirect3DVertexBuffer9 *original);

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
	#pragma region IDirect3DVertexBuffer9
	HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE Unlock() override;
	HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override;
	#pragma endregion
};

struct DECLSPEC_UUID("AEEDCCA1-9489-4C7E-AE78-12147B437001") Direct3DIndexBuffer9 final : reshade::d3d9::resource_impl<IDirect3DIndexBuffer9>
{
	Direct3DIndexBuffer9(Direct3DDevice9 *device, IDirect3DIndexBuffer9 *original);

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
	#pragma region IDirect3DIndexBuffer9
	HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags) override;
	HRESULT STDMETHODCALLTYPE Unlock() override;
	HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *pDesc) override;
	#pragma endregion
};

#endif
