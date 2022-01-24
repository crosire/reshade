/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"

using reshade::d3d9::to_handle;

extern void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, IDirect3D9 *d3d, UINT adapter_index);
extern void dump_and_modify_present_parameters(D3DPRESENT_PARAMETERS &pp, D3DDISPLAYMODEEX &fullscreen_desc, IDirect3D9 *d3d, UINT adapter_index);

static inline const reshade::api::subresource_box *convert_rect_to_box(const RECT *rect, reshade::api::subresource_box &box)
{
	if (rect == nullptr)
		return nullptr;

	box.left = rect->left;
	box.top = rect->top;
	box.front = 0;
	box.right = rect->right;
	box.bottom = rect->bottom;
	box.back = 1;

	return &box;
}
static inline const reshade::api::subresource_box *convert_rect_to_box(const POINT *point, LONG width, LONG height, reshade::api::subresource_box &box)
{
	if (point == nullptr)
		return nullptr;

	box.left = point->x;
	box.top = point->y;
	box.front = 0;
	box.right = point->x + width;
	box.bottom = point->y + height;
	box.back = 1;

	return &box;
}

Direct3DDevice9::Direct3DDevice9(IDirect3DDevice9   *original, bool use_software_rendering) :
	device_impl(original),
	_extended_interface(0),
	_use_software_rendering(use_software_rendering)
{
	assert(_orig != nullptr);
}
Direct3DDevice9::Direct3DDevice9(IDirect3DDevice9Ex *original, bool use_software_rendering) :
	device_impl(original),
	_extended_interface(1),
	_use_software_rendering(use_software_rendering)
{
	assert(_orig != nullptr);
}

bool Direct3DDevice9::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DDevice9))
		return true;
	if (riid != __uuidof(IDirect3DDevice9Ex))
		return false;

	if (!_extended_interface)
	{
		IDirect3DDevice9Ex *new_interface = nullptr;
		if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&new_interface))))
			return false;
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Upgrading IDirect3DDevice9 object " << this << " to IDirect3DDevice9Ex.";
#endif
		_orig->Release();
		_orig = new_interface;
		_extended_interface = true;
	}

	return true;
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	// Release remaining references to this device
	_implicit_swapchain->Release();

	assert(_additional_swapchains.empty());

	const auto orig = _orig;
	const bool extended_interface = _extended_interface;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "IDirect3DDevice9" << (extended_interface ? "Ex" : "") << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "IDirect3DDevice9" << (extended_interface ? "Ex" : "") << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
#include "hook_manager.hpp"

#undef IDirect3DSurface9_LockRect
#undef IDirect3DSurface9_UnlockRect

static HRESULT STDMETHODCALLTYPE IDirect3DSurface9_LockRect(IDirect3DSurface9 *pSurface, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DSurface9_UnlockRect(IDirect3DSurface9 *pSurface)
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

#undef IDirect3DVolume9_LockBox
#undef IDirect3DVolume9_UnlockBox

static HRESULT STDMETHODCALLTYPE IDirect3DVolume9_LockBox(IDirect3DVolume9 *pVolume, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DVolume9_UnlockBox(IDirect3DVolume9 *pVolume)
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

#undef IDirect3DTexture9_LockRect
#undef IDirect3DTexture9_UnlockRect

static HRESULT STDMETHODCALLTYPE IDirect3DTexture9_LockRect(IDirect3DTexture9 *pTexture, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DTexture9_UnlockRect(IDirect3DTexture9 *pTexture, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), Level);
	}

	return reshade::hooks::call(IDirect3DTexture9_UnlockRect, vtable_from_instance(pTexture) + 20)(pTexture, Level);
}

#undef IDirect3DVolumeTexture9_LockBox
#undef IDirect3DVolumeTexture9_UnlockBox

static HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_LockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DVolumeTexture9_UnlockBox(IDirect3DVolumeTexture9 *pTexture, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), Level);
	}

	return reshade::hooks::call(IDirect3DVolumeTexture9_UnlockBox, vtable_from_instance(pTexture) + 20)(pTexture, Level);
}

#undef IDirect3DCubeTexture9_LockRect
#undef IDirect3DCubeTexture9_UnlockRect

static HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_LockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DCubeTexture9_UnlockRect(IDirect3DCubeTexture9 *pTexture, D3DCUBEMAP_FACES FaceType, UINT Level)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pTexture))
	{
		const uint32_t subresource = Level + static_cast<uint32_t>(FaceType) * pTexture->GetLevelCount();

		reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pTexture), subresource);
	}

	return reshade::hooks::call(IDirect3DCubeTexture9_UnlockRect, vtable_from_instance(pTexture) + 20)(pTexture, FaceType, Level);
}

#undef IDirect3DVertexBuffer9_Lock
#undef IDirect3DVertexBuffer9_Unlock

static HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Lock(IDirect3DVertexBuffer9 *pVertexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DVertexBuffer9_Unlock(IDirect3DVertexBuffer9 *pVertexBuffer)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pVertexBuffer))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pVertexBuffer));
	}

	return reshade::hooks::call(IDirect3DVertexBuffer9_Unlock, vtable_from_instance(pVertexBuffer) + 12)(pVertexBuffer);
}

#undef IDirect3DIndexBuffer9_Lock
#undef IDirect3DIndexBuffer9_Unlock

static HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Lock(IDirect3DIndexBuffer9 *pIndexBuffer, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
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
static HRESULT STDMETHODCALLTYPE IDirect3DIndexBuffer9_Unlock(IDirect3DIndexBuffer9 *pIndexBuffer)
{
	if (const auto device_proxy = get_private_pointer_d3d9<Direct3DDevice9>(pIndexBuffer))
	{
		reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pIndexBuffer));
	}

	return reshade::hooks::call(IDirect3DIndexBuffer9_Unlock, vtable_from_instance(pIndexBuffer) + 12)(pIndexBuffer);
}
#endif

HRESULT STDMETHODCALLTYPE Direct3DDevice9::TestCooperativeLevel()
{
	return _orig->TestCooperativeLevel();
}
UINT    STDMETHODCALLTYPE Direct3DDevice9::GetAvailableTextureMem()
{
	return _orig->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EvictManagedResources()
{
	return _orig->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
	return _orig->GetDirect3D(ppD3D9);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
	return _orig->GetDeviceCaps(pCaps);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return _orig->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap)
{
	return _orig->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}
void    STDMETHODCALLTYPE Direct3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags)
{
	return _orig->SetCursorPosition(X, Y, Flags);
}
BOOL    STDMETHODCALLTYPE Direct3DDevice9::ShowCursor(BOOL bShow)
{
	return _orig->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain)
{
	LOG(INFO) << "Redirecting " << "IDirect3DDevice9::CreateAdditionalSwapChain" << '(' << "this = " << this << ", pPresentationParameters = " << pPresentationParameters << ", ppSwapChain = " << ppSwapChain << ')' << " ...";

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, _d3d.get(), _cp.AdapterOrdinal);

	const HRESULT hr = _orig->CreateAdditionalSwapChain(&pp, ppSwapChain);

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createadditionalswapchain)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (FAILED(hr))
	{
		LOG(WARN) << "IDirect3DDevice9::CreateAdditionalSwapChain" << " failed with error code " << hr << '.';
		return hr;
	}

	AddRef(); // Add reference which is released when the swap chain is destroyed (see 'Direct3DSwapChain9::Release')

	const auto swapchain_proxy = new Direct3DSwapChain9(this, *ppSwapChain);
	_additional_swapchains.push_back(swapchain_proxy);
	*ppSwapChain = swapchain_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDirect3DSwapChain9 object " << swapchain_proxy << " (" << swapchain_proxy->_orig << ").";
#endif
	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **ppSwapChain)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	if (ppSwapChain == nullptr)
		return D3DERR_INVALIDCALL;

	_implicit_swapchain->AddRef();
	*ppSwapChain = _implicit_swapchain;

	return D3D_OK;
}
UINT    STDMETHODCALLTYPE Direct3DDevice9::GetNumberOfSwapChains()
{
	return 1; // Multi-head swap chains are not supported
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	LOG(INFO) << "Redirecting " << "IDirect3DDevice9::Reset" << '(' << "this = " << this << ", pPresentationParameters = " << pPresentationParameters << ')' << " ...";

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, _d3d.get(), _cp.AdapterOrdinal);

	// Release all resources before performing reset
	_implicit_swapchain->on_reset();
	device_impl::on_reset();

	const HRESULT hr = _orig->Reset(&pp);

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (FAILED(hr))
	{
		LOG(ERROR) << "IDirect3DDevice9::Reset" << " failed with error code " << hr << '!';
		return hr;
	}

	device_impl::on_init(pp);
	_implicit_swapchain->on_init(pp);

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::present>(
		this,
		_implicit_swapchain,
		reinterpret_cast<const reshade::api::rect *>(pSourceRect),
		reinterpret_cast<const reshade::api::rect *>(pDestRect),
		pDirtyRegion != nullptr ? pDirtyRegion->rdh.nCount : 0,
		pDirtyRegion != nullptr ? reinterpret_cast<const reshade::api::rect *>(pDirtyRegion->Buffer) : nullptr);
#endif

	// Only call into the effect runtime if the entire surface is presented, to avoid partial updates messing up effects and the GUI
	if (Direct3DSwapChain9::is_presenting_entire_surface(pSourceRect, hDestWindowOverride))
		_implicit_swapchain->on_present();

	return _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return _orig->SetDialogBoxMode(bEnableDialogs);
}
void    STDMETHODCALLTYPE Direct3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return;
	}

	return _orig->SetGammaRamp(0, Flags, pRamp);
}
void    STDMETHODCALLTYPE Direct3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return;
	}

	return _orig->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_TEXTURE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, _caps);

	const HRESULT hr = _orig->CreateTexture(internal_desc.Width, internal_desc.Height, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DTexture9 *const resource = *ppTexture;

		Levels = resource->GetLevelCount();

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DTexture9::LockRect", vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(IDirect3DTexture9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DTexture9::UnlockRect", vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(IDirect3DTexture9_UnlockRect));

		// Hook surfaces explicitly here, since some applications lock textures via its individual surfaces and they use a different vtable than standalone surfaces
		for (UINT level = 0; level < Levels; ++level)
		{
			com_ptr<IDirect3DSurface9> surface;
			if (SUCCEEDED(resource->GetSurfaceLevel(level, &surface)))
			{
				surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

				if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
					reshade::hooks::install("IDirect3DSurface9::LockRect", vtable_from_instance(surface.get()), 13, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_LockRect));
				if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
					reshade::hooks::install("IDirect3DSurface9::UnlockRect", vtable_from_instance(surface.get()), 14, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_UnlockRect));
			}
		}
#endif

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});

		// Register all surfaces of this texture too
		if (const reshade::api::resource_usage view_usage = desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::depth_stencil); view_usage != 0)
		{
			for (UINT level = 0; level < Levels; ++level)
			{
				com_ptr<IDirect3DSurface9> surface;
				if (SUCCEEDED(resource->GetSurfaceLevel(level, &surface)))
				{
					reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
						this,
						to_handle(resource),
						view_usage,
						reshade::api::resource_view_desc(desc.texture.format, level, 1, 0, 1),
						to_handle(surface.get()));

					register_destruction_callback_d3d9(surface.get(), [this, resource_view = surface.get()]() {
						reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
					});
				}
			}
		}
		if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			register_destruction_callback_d3d9(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
			}, 1);
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateTexture" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DVOLUME_DESC internal_desc { Format, D3DRTYPE_VOLUMETEXTURE, Usage, Pool, Width, Height, Depth };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, _caps);

	const HRESULT hr = _orig->CreateVolumeTexture(internal_desc.Width, internal_desc.Height, internal_desc.Depth, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppVolumeTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppVolumeTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DVolumeTexture9 *const resource = *ppVolumeTexture;

		Levels = resource->GetLevelCount();

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DVolumeTexture9::LockBox", vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(IDirect3DVolumeTexture9_LockBox));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DVolumeTexture9::UnlockBox", vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(IDirect3DVolumeTexture9_UnlockBox));

		for (UINT level = 0; level < Levels; ++level)
		{
			com_ptr<IDirect3DVolume9> volume;
			if (SUCCEEDED(resource->GetVolumeLevel(level, &volume)))
			{
				volume->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

				if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
					reshade::hooks::install("IDirect3DVolume9::LockBox", vtable_from_instance(volume.get()), 9, reinterpret_cast<reshade::hook::address>(IDirect3DVolume9_LockBox));
				if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
					reshade::hooks::install("IDirect3DVolume9::UnlockBox", vtable_from_instance(volume.get()), 10, reinterpret_cast<reshade::hook::address>(IDirect3DVolume9_UnlockBox));
			}
		}
#endif

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});

		if ((desc.usage & reshade::api::resource_usage::shader_resource) != 0)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			register_destruction_callback_d3d9(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
			}, 1);
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateVolumeTexture" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc { Format, D3DRTYPE_CUBETEXTURE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, EdgeLength, EdgeLength };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, Levels, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, &Levels, _caps);

	const HRESULT hr = _orig->CreateCubeTexture(internal_desc.Width, Levels, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppCubeTexture, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppCubeTexture != nullptr);

#if RESHADE_ADDON
		IDirect3DCubeTexture9 *const resource = *ppCubeTexture;

		Levels = resource->GetLevelCount();

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DCubeTexture9::LockRect", vtable_from_instance(resource), 19, reinterpret_cast<reshade::hook::address>(IDirect3DCubeTexture9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DCubeTexture9::UnlockRect", vtable_from_instance(resource), 20, reinterpret_cast<reshade::hook::address>(IDirect3DCubeTexture9_UnlockRect));

		// Hook surfaces explicitly here, since some applications lock textures via its individual surfaces and they use a different vtable than standalone surfaces
		for (UINT level = 0; level < Levels; ++level)
		{
			for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
			{
				com_ptr<IDirect3DSurface9> surface;
				if (SUCCEEDED(resource->GetCubeMapSurface(face, level, &surface)))
				{
					surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

					if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
						reshade::hooks::install("IDirect3DSurface9::LockRect", vtable_from_instance(surface.get()), 13, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_LockRect));
					if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
						reshade::hooks::install("IDirect3DSurface9::UnlockRect", vtable_from_instance(surface.get()), 14, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_UnlockRect));
				}
			}
		}
#endif

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});

		// Register all surfaces of this texture too
		if (const reshade::api::resource_usage view_usage = desc.usage & (reshade::api::resource_usage::render_target | reshade::api::resource_usage::depth_stencil); view_usage != 0)
		{
			for (UINT level = 0; level < Levels; ++level)
			{
				for (D3DCUBEMAP_FACES face = D3DCUBEMAP_FACE_POSITIVE_X; face <= D3DCUBEMAP_FACE_NEGATIVE_Z; face = static_cast<D3DCUBEMAP_FACES>(face + 1))
				{
					com_ptr<IDirect3DSurface9> surface;
					if (SUCCEEDED(resource->GetCubeMapSurface(face, level, &surface)))
					{
						reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
							this,
							to_handle(resource),
							view_usage,
							reshade::api::resource_view_desc(desc.texture.format, level, 1, face, 1),
							to_handle(surface.get()));

						register_destruction_callback_d3d9(surface.get(), [this, resource_view = surface.get()]() {
							reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(resource_view));
						});
					}
				}
			}
		}
		if ((desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined)
		{
			reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
				this,
				to_handle(resource),
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_view_desc(desc.texture.format, 0, UINT32_MAX, 0, UINT32_MAX),
				reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });

			register_destruction_callback_d3d9(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource) });
			}, 1);
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateCubeTexture" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle)
{
	// Need to allow buffer for use in software vertex processing, since application uses software and not hardware processing, but device was created with both
	if (_use_software_rendering)
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;

#if RESHADE_ADDON
	D3DVERTEXBUFFER_DESC internal_desc = { D3DFMT_UNKNOWN, D3DRTYPE_VERTEXBUFFER, Usage, Pool, Length, FVF };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc);

	const HRESULT hr = _orig->CreateVertexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.FVF, internal_desc.Pool, ppVertexBuffer, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppVertexBuffer != nullptr);

#if RESHADE_ADDON
		IDirect3DVertexBuffer9 *const resource = *ppVertexBuffer;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
			reshade::hooks::install("IDirect3DVertexBuffer9::Lock", vtable_from_instance(resource), 11, reinterpret_cast<reshade::hook::address>(IDirect3DVertexBuffer9_Lock));
		if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
			reshade::hooks::install("IDirect3DVertexBuffer9::Unlock", vtable_from_instance(resource), 12, reinterpret_cast<reshade::hook::address>(IDirect3DVertexBuffer9_Unlock));
#endif

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(*ppVertexBuffer));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateVertexBuffer" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle)
{
	if (_use_software_rendering)
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;

#if RESHADE_ADDON
	D3DINDEXBUFFER_DESC internal_desc = { Format, D3DRTYPE_INDEXBUFFER, Usage, Pool, Length };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc);

	const HRESULT hr = _orig->CreateIndexBuffer(internal_desc.Size, internal_desc.Usage, internal_desc.Format, internal_desc.Pool, ppIndexBuffer, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppIndexBuffer != nullptr);

#if RESHADE_ADDON
		IDirect3DIndexBuffer9 *const resource = *ppIndexBuffer;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		resource->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>())
			reshade::hooks::install("IDirect3DIndexBuffer9::Lock", vtable_from_instance(resource), 11, reinterpret_cast<reshade::hook::address>(IDirect3DIndexBuffer9_Lock));
		if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>())
			reshade::hooks::install("IDirect3DIndexBuffer9::Unlock", vtable_from_instance(resource), 12, reinterpret_cast<reshade::hook::address>(IDirect3DIndexBuffer9_Unlock));
#endif

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

		register_destruction_callback_d3d9(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateIndexBuffer" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::render_target))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) && !Lockable ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		_orig->CreateRenderTarget(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Lockable, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);
#endif

		// In case surface was replaced with a texture resource
		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateRenderTarget" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::depth_stencil))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		_orig->CreateDepthStencilSurface(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Discard, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);
#endif

		// In case surface was replaced with a texture resource
		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::depth_stencil,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::depth_stencil,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateDepthStencilSurface" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateSurface(IDirect3DSurface9 *pSrcSurface, const RECT *pSrcRect, IDirect3DSurface9 *pDstSurface, const POINT *pDstPoint)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		uint32_t src_subresource;
		reshade::api::subresource_box src_box;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		reshade::api::subresource_box dst_box;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (pSrcRect != nullptr)
		{
			convert_rect_to_box(pDstPoint, pSrcRect->right - pSrcRect->left, pSrcRect->bottom - pSrcRect->top, dst_box);
		}
		else
		{
			D3DSURFACE_DESC desc;
			pSrcSurface->GetDesc(&desc);

			convert_rect_to_box(pDstPoint, desc.Width, desc.Height, dst_box);
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this, src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box), dst_resource, dst_subresource, pDstPoint != nullptr ? &dst_box : nullptr, reshade::api::filter_mode::min_mag_mip_point))
			return D3D_OK;
	}
#endif

	return _orig->UpdateSurface(pSrcSurface, pSrcRect, pDstSurface, pDstPoint);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSrcTexture, IDirect3DBaseTexture9 *pDstTexture)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, to_handle(pSrcTexture), to_handle(pDstTexture)))
		return D3D_OK;
#endif

	return _orig->UpdateTexture(pSrcTexture, pDstTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTargetData(IDirect3DSurface9 *pSrcSurface, IDirect3DSurface9 *pDstSurface)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		uint32_t src_subresource;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this, src_resource, src_subresource, nullptr, dst_resource, dst_subresource, nullptr, reshade::api::filter_mode::min_mag_mip_point))
			return D3D_OK;
	}
#endif

	return _orig->GetRenderTargetData(pSrcSurface, pDstSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	return _implicit_swapchain->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::StretchRect(IDirect3DSurface9 *pSrcSurface, const RECT *pSrcRect, IDirect3DSurface9 *pDstSurface, const RECT *pDstRect, D3DTEXTUREFILTERTYPE Filter)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	assert(pSrcSurface != nullptr && pDstSurface != nullptr);

	if (reshade::has_addon_event<reshade::addon_event::copy_texture_region>() ||
		reshade::has_addon_event<reshade::addon_event::resolve_texture_region>())
	{
		D3DSURFACE_DESC desc;
		pSrcSurface->GetDesc(&desc);

		uint32_t src_subresource;
		const reshade::api::resource src_resource = get_resource_from_view(to_handle(pSrcSurface), &src_subresource);
		uint32_t dst_subresource;
		const reshade::api::resource dst_resource = get_resource_from_view(to_handle(pDstSurface), &dst_subresource);

		if (desc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			reshade::api::subresource_box src_box, dst_box;

			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
					src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box),
					dst_resource, dst_subresource, convert_rect_to_box(pDstRect, dst_box),
					Filter == D3DTEXF_NONE || Filter == D3DTEXF_POINT ? reshade::api::filter_mode::min_mag_mip_point : reshade::api::filter_mode::min_mag_mip_linear))
				return D3D_OK;
		}
		else
		{
			reshade::api::subresource_box src_box;

			if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this,
					src_resource, src_subresource, convert_rect_to_box(pSrcRect, src_box),
					dst_resource, dst_subresource, pDstRect != nullptr ? pDstRect->left : 0, pDstRect != nullptr ? pDstRect->top : 0, 0,
					reshade::d3d9::convert_format(desc.Format)))
				return D3D_OK;
		}
	}
#endif

	return _orig->StretchRect(pSrcSurface, pSrcRect, pDstSurface, pDstRect, Filter);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR Color)
{
#if RESHADE_ADDON
	if (const float color[4] = { ((Color >> 16) & 0xFF) / 255.0f, ((Color >> 8) & 0xFF) / 255.0f, (Color & 0xFF) / 255.0f, ((Color >> 24) & 0xFF) / 255.0f };
		reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, to_handle(pSurface), color, pRect != nullptr ? 1 : 0, reinterpret_cast<const reshade::api::rect *>(pRect)))
		return D3D_OK;
#endif

	return _orig->ColorFill(pSurface, pRect, Color);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, 0, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = _orig->CreateOffscreenPlainSurface(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.Pool, ppSurface, pSharedHandle);
#else
	const HRESULT hr = _orig->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::LockRect", vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::UnlockRect", vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_UnlockRect));
#endif

		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateOffscreenPlainSurface" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
	const HRESULT hr = _orig->SetRenderTarget(RenderTargetIndex, pRenderTarget);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) && (
		reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>() ||
		reshade::has_addon_event<reshade::addon_event::bind_viewports>()))
	{
		DWORD count = 0;
		com_ptr<IDirect3DSurface9> surface;
		reshade::api::resource_view rtvs[8] = {}, dsv = {};
		for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
		{
			if (FAILED(_orig->GetRenderTarget(i, &surface)))
				continue;

			// All surfaces that can be used as render target should be registered at this point
			rtvs[i] = to_handle(surface.get());
			count = i + 1;
		}
		if (SUCCEEDED(_orig->GetDepthStencilSurface(&surface)))
		{
			// All surfaces that can be used as depth-stencil should be registered at this point
			dsv = to_handle(surface.get());
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, count, rtvs, dsv);

		if (pRenderTarget != nullptr)
		{
			// Setting a new render target will cause the viewport to be set to the full size of the new render target
			// See https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-setrendertarget
			D3DSURFACE_DESC desc;
			pRenderTarget->GetDesc(&desc);

			const reshade::api::viewport viewport_data = {
				0.0f,
				0.0f,
				static_cast<float>(desc.Width),
				static_cast<float>(desc.Height),
				0.0f,
				1.0f
			};

			reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, 1, &viewport_data);
		}
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
	return _orig->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
	const HRESULT hr = _orig->SetDepthStencilSurface(pNewZStencil);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
	{
		DWORD count = 0;
		com_ptr<IDirect3DSurface9> surface;
		reshade::api::resource_view rtvs[8] = {};
		for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
		{
			if (FAILED(_orig->GetRenderTarget(i, &surface)))
				continue;

			rtvs[i] = to_handle(surface.get());
			count = i + 1;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, count, rtvs, to_handle(pNewZStencil));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
	return _orig->GetDepthStencilSurface(ppZStencilSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginScene()
{
	return _orig->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndScene()
{
	return _orig->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
#if RESHADE_ADDON
	if (Flags & (D3DCLEAR_TARGET) &&
		reshade::has_addon_event<reshade::addon_event::clear_render_target_view>())
	{
		com_ptr<IDirect3DSurface9> surface;
		for (DWORD i = 0; i < _caps.NumSimultaneousRTs; ++i, surface.reset())
		{
			if (FAILED(_orig->GetRenderTarget(i, &surface)))
				continue;

			if (const float color[4] = { ((Color >> 16) & 0xFF) / 255.0f, ((Color >> 8) & 0xFF) / 255.0f, (Color & 0xFF) / 255.0f, ((Color >> 24) & 0xFF) / 255.0f };
				reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, to_handle(surface.get()), color, Count, reinterpret_cast<const reshade::api::rect *>(pRects)))
				Flags &= ~(D3DCLEAR_TARGET);
		}
	}
	if (Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL) &&
		reshade::has_addon_event<reshade::addon_event::clear_depth_stencil_view>())
	{
		com_ptr<IDirect3DSurface9> surface;
		if (SUCCEEDED(_orig->GetDepthStencilSurface(&surface)))
		{
			if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, to_handle(surface.get()), Flags & D3DCLEAR_ZBUFFER ? &Z : nullptr, Flags & D3DCLEAR_STENCIL ? reinterpret_cast<const uint8_t *>(&Stencil) : nullptr, Count, reinterpret_cast<const reshade::api::rect *>(pRects)))
				Flags &= ~(D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL);
		}
	}

	if (Flags == 0)
		return D3D_OK;
#endif

	return _orig->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return _orig->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetViewport(const D3DVIEWPORT9 *pViewport)
{
	const HRESULT hr = _orig->SetViewport(pViewport);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_viewports>())
	{
		const reshade::api::viewport viewport_data = {
			static_cast<float>(pViewport->X),
			static_cast<float>(pViewport->Y),
			static_cast<float>(pViewport->Width),
			static_cast<float>(pViewport->Height),
			pViewport->MinZ,
			pViewport->MaxZ
		};

		reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, 1, &viewport_data);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
	return _orig->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaterial(const D3DMATERIAL9 *pMaterial)
{
	return _orig->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
	return _orig->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetLight(DWORD Index, const D3DLIGHT9 *pLight)
{
	return _orig->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *pLight)
{
	return _orig->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::LightEnable(DWORD Index, BOOL Enable)
{
	return _orig->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return _orig->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipPlane(DWORD Index, const float *pPlane)
{
	return _orig->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipPlane(DWORD Index, float *pPlane)
{
	return _orig->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	const HRESULT hr = _orig->SetRenderState(State, Value);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_pipeline_states>())
	{
		switch (State)
		{
		case D3DRS_BLENDOP:
		case D3DRS_BLENDOPALPHA:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_blend_op(static_cast<D3DBLENDOP>(Value)));
			break;
		case D3DRS_SRCBLEND:
		case D3DRS_DESTBLEND:
		case D3DRS_SRCBLENDALPHA:
		case D3DRS_DESTBLENDALPHA:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_blend_factor(static_cast<D3DBLEND>(Value)));
			break;
		case D3DRS_FILLMODE:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_fill_mode(static_cast<D3DFILLMODE>(Value)));
			break;
		case D3DRS_CULLMODE:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_cull_mode(static_cast<D3DCULL>(Value), false));
			break;
		case D3DRS_ZFUNC:
		case D3DRS_ALPHAFUNC:
		case D3DRS_STENCILFUNC:
		case D3DRS_CCW_STENCILFUNC:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_compare_op(static_cast<D3DCMPFUNC>(Value)));
			break;
		case D3DRS_STENCILFAIL:
		case D3DRS_STENCILZFAIL:
		case D3DRS_STENCILPASS:
		case D3DRS_CCW_STENCILFAIL:
		case D3DRS_CCW_STENCILZFAIL:
		case D3DRS_CCW_STENCILPASS:
			Value = static_cast<uint32_t>(reshade::d3d9::convert_stencil_op(static_cast<D3DSTENCILOP>(Value)));
			break;
		}

		const reshade::api::dynamic_state state = reshade::d3d9::convert_dynamic_state(State);
		const uint32_t value = Value;
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
	return _orig->GetRenderState(State, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB)
{
	return _orig->CreateStateBlock(Type, ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginStateBlock()
{
	return _orig->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
	return _orig->EndStateBlock(ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
	return _orig->GetTexture(Stage, ppTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
	const HRESULT hr = _orig->SetTexture(Stage, pTexture);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::push_descriptors>())
	{
		reshade::api::shader_stage shader_stage = reshade::api::shader_stage::pixel;
		if (Stage >= D3DVERTEXTEXTURESAMPLER0)
		{
			shader_stage = reshade::api::shader_stage::vertex;
			Stage -= D3DVERTEXTEXTURESAMPLER0;
		}

		// There are no sampler state objects in D3D9, so cannot deduce a handle to one here meaningfully
		reshade::api::sampler_with_resource_view descriptor_data = {
			reshade::api::sampler { 0 },
			reshade::api::resource_view { reinterpret_cast<uintptr_t>(pTexture) }
		};

		reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
			this,
			shader_stage,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, shader_stage == reshade::api::shader_stage::vertex ? 0 : 1,
			reshade::api::descriptor_set_update { {}, Stage, 0, 1, reshade::api::descriptor_type::sampler_with_resource_view, &descriptor_data });
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
	return _orig->GetTextureStageState(Stage, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	return _orig->SetTextureStageState(Stage, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
{
	return _orig->GetSamplerState(Sampler, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return _orig->SetSamplerState(Sampler, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ValidateDevice(DWORD *pNumPasses)
{
	return _orig->ValidateDevice(pNumPasses);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY *pEntries)
{
	return _orig->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return _orig->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return _orig->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
	return _orig->GetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetScissorRect(const RECT *pRect)
{
	const HRESULT hr = _orig->SetScissorRect(pRect);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, 1, reinterpret_cast<const reshade::api::rect *>(pRect));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetScissorRect(RECT *pRect)
{
	return _orig->GetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return _orig->SetSoftwareVertexProcessing(bSoftware);
}
BOOL    STDMETHODCALLTYPE Direct3DDevice9::GetSoftwareVertexProcessing()
{
	return _orig->GetSoftwareVertexProcessing();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetNPatchMode(float nSegments)
{
	return _orig->SetNPatchMode(nSegments);
}
float   STDMETHODCALLTYPE Direct3DDevice9::GetNPatchMode()
{
	return _orig->GetNPatchMode();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;

		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, StartVertex, 0))
		return D3D_OK;
#endif

	return _orig->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;

		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, StartIndex, BaseVertexIndex, 0))
		return D3D_OK;
#endif

	return _orig->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;

		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, 0, 0))
		return D3D_OK;
#endif

	return _orig->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#if RESHADE_ADDON
	if (PrimitiveType != _current_prim_type)
	{
		_current_prim_type = PrimitiveType;

		const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
		const uint32_t value = static_cast<uint32_t>(reshade::d3d9::convert_primitive_topology(PrimitiveType));
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, reshade::d3d9::calc_vertex_from_prim_count(PrimitiveType, PrimitiveCount), 1, 0, 0, 0))
		return D3D_OK;
#endif

	return _orig->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags)
{
	return _orig->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl)
{
#if RESHADE_ADDON
	auto desc = reshade::d3d9::convert_pipeline_desc(pVertexElements);
	std::vector<D3DVERTEXELEMENT9> internal_elements;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc, 0, nullptr))
	{
		reshade::d3d9::convert_pipeline_desc(desc, internal_elements);
		pVertexElements = internal_elements.data();
	}
#endif

	const HRESULT hr = _orig->CreateVertexDeclaration(pVertexElements, ppDecl);
	if (SUCCEEDED(hr))
	{
		assert(ppDecl != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, 0, nullptr, to_handle(*ppDecl));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateVertexDeclaration" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
	const HRESULT hr = _orig->SetVertexDeclaration(pDecl);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, to_handle(pDecl));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
	return _orig->GetVertexDeclaration(ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetFVF(DWORD FVF)
{
	return _orig->SetFVF(FVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFVF(DWORD *pFVF)
{
	return _orig->GetFVF(pFVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader)
{
#if RESHADE_ADDON
	if (pFunction == nullptr)
		return D3DERR_INVALIDCALL;

	// First token should be version token (https://docs.microsoft.com/windows-hardware/drivers/display/version-token), so verify its shader type
	assert((pFunction[0] >> 16) == 0xFFFE);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::vertex_shader };
	desc.graphics.vertex_shader.code = pFunction;

	// Find the end token to calculate token stream size (https://docs.microsoft.com/windows-hardware/drivers/display/end-token)
	desc.graphics.vertex_shader.code_size = sizeof(DWORD);
	for (int i = 0; pFunction[i] != 0x0000FFFF; ++i)
		desc.graphics.vertex_shader.code_size += sizeof(DWORD);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc, 0, nullptr))
	{
		pFunction = static_cast<const DWORD *>(desc.graphics.vertex_shader.code);
	}
#endif

	const HRESULT hr = _orig->CreateVertexShader(pFunction, ppShader);
	if (SUCCEEDED(hr))
	{
		assert(ppShader != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, 0, nullptr, to_handle(*ppShader));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreateVertexShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
	const HRESULT hr = _orig->SetVertexShader(pShader);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::vertex_shader, to_handle(pShader));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
	return _orig->GetVertexShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 2,
			StartRegister,
			Vector4fCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 3,
			StartRegister,
			Vector4iCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
	const HRESULT hr = _orig->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::vertex,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 4,
			StartRegister,
			BoolCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
	const HRESULT hr = _orig->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
	{
		const reshade::api::resource buffer = to_handle(pStreamData);
		const uint64_t offset_64 = OffsetInBytes;

		reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StreamNumber, 1, &buffer, &offset_64, &Stride);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride)
{
	return _orig->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
{
	return _orig->SetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT *Divider)
{
	return _orig->GetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
	const HRESULT hr = _orig->SetIndices(pIndexData);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr) &&
		reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
	{
		uint32_t index_size = 0;
		if (pIndexData != nullptr)
		{
			D3DINDEXBUFFER_DESC desc;
			pIndexData->GetDesc(&desc);
			index_size = (desc.Format == D3DFMT_INDEX16) ? 2 : 4;
		}

		reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, to_handle(pIndexData), 0, index_size);
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData)
{
	return _orig->GetIndices(ppIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader)
{
#if RESHADE_ADDON
	if (pFunction == nullptr)
		return D3DERR_INVALIDCALL;

	// First token should be version token (https://docs.microsoft.com/windows-hardware/drivers/display/version-token), so verify its shader type
	assert((pFunction[0] >> 16) == 0xFFFF);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::pixel_shader };
	desc.graphics.pixel_shader.code = pFunction;

	// Find the end token to calculate token stream size (https://docs.microsoft.com/windows-hardware/drivers/display/end-token)
	desc.graphics.pixel_shader.code_size = sizeof(DWORD);
	for (int i = 0; pFunction[i] != 0x0000FFFF; ++i)
		desc.graphics.pixel_shader.code_size += sizeof(DWORD);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc, 0, nullptr))
	{
		pFunction = static_cast<const DWORD *>(desc.graphics.pixel_shader.code);
	}
#endif

	const HRESULT hr = _orig->CreatePixelShader(pFunction, ppShader);
	if (SUCCEEDED(hr))
	{
		assert(ppShader != nullptr);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, 0, nullptr, to_handle(*ppShader));
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9::CreatePixelShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
	const HRESULT hr = _orig->SetPixelShader(pShader);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::pixel_shader, to_handle(pShader));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
	return _orig->GetPixelShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 5,
			StartRegister,
			Vector4fCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 6,
			StartRegister,
			Vector4iCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT BoolCount)
{
	const HRESULT hr = _orig->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::push_constants>(
			this,
			reshade::api::shader_stage::pixel,
			// See global pipeline layout specified in 'device_impl::on_init'
			reshade::d3d9::global_pipeline_layout, 7,
			StartRegister,
			BoolCount,
			reinterpret_cast<const uint32_t *>(pConstantData));
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawRectPatch(UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return _orig->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawTriPatch(UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return _orig->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DeletePatch(UINT Handle)
{
	return _orig->DeletePatch(Handle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery)
{
	return _orig->CreateQuery(Type, ppQuery);
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetConvolutionMonoKernel(width, height, rows, columns);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::present>(
		this,
		_implicit_swapchain,
		reinterpret_cast<const reshade::api::rect *>(pSourceRect),
		reinterpret_cast<const reshade::api::rect *>(pDestRect),
		pDirtyRegion != nullptr ? pDirtyRegion->rdh.nCount : 0,
		pDirtyRegion != nullptr ? reinterpret_cast<const reshade::api::rect *>(pDirtyRegion->Buffer) : nullptr);
#endif

	if (Direct3DSwapChain9::is_presenting_entire_surface(pSourceRect, hDestWindowOverride))
		_implicit_swapchain->on_present();

	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetGPUThreadPriority(INT *pPriority)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetGPUThreadPriority(pPriority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetGPUThreadPriority(INT Priority)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::WaitForVBlank(UINT iSwapChain)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->WaitForVBlank(0);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT32 NumResources)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckResourceResidency(pResourceArray, NumResources);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetMaximumFrameLatency(pMaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckDeviceState(HWND hDestinationWindow)
{
	assert(_extended_interface);
	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckDeviceState(hDestinationWindow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, Usage, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::render_target))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) && !Lockable ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		static_cast<IDirect3DDevice9Ex *>(_orig)->CreateRenderTargetEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Lockable, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);
#endif

		// In case surface was replaced with a texture resource
		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9Ex::CreateRenderTargetEx" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, Usage, Pool, D3DMULTISAMPLE_NONE, 0, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::general))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateOffscreenPlainSurfaceEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.Pool, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);

		if (reshade::has_addon_event<reshade::addon_event::map_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::LockRect", vtable_from_instance(surface), 13, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_LockRect));
		if (reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
			reshade::hooks::install("IDirect3DSurface9::UnlockRect", vtable_from_instance(surface), 14, reinterpret_cast<reshade::hook::address>(IDirect3DSurface9_UnlockRect));
#endif

		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::render_target,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::render_target,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9Ex::CreateOffscreenPlainSurfaceEx" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_extended_interface);

#if RESHADE_ADDON
	D3DSURFACE_DESC internal_desc = { Format, D3DRTYPE_SURFACE, Usage, D3DPOOL_DEFAULT, MultiSample, MultisampleQuality, Width, Height };
	auto desc = reshade::d3d9::convert_resource_desc(internal_desc, 1, _caps, pSharedHandle != nullptr);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, reshade::api::resource_usage::depth_stencil))
		reshade::d3d9::convert_resource_desc(desc, internal_desc, nullptr, _caps);

	const HRESULT hr = ((desc.usage & reshade::api::resource_usage::shader_resource) != 0) ?
		create_surface_replacement(internal_desc, ppSurface, pSharedHandle) :
		static_cast<IDirect3DDevice9Ex *>(_orig)->CreateDepthStencilSurfaceEx(internal_desc.Width, internal_desc.Height, internal_desc.Format, internal_desc.MultiSampleType, internal_desc.MultiSampleQuality, Discard, ppSurface, pSharedHandle, internal_desc.Usage);
#else
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
#endif
	if (SUCCEEDED(hr))
	{
		assert(ppSurface != nullptr);

#if RESHADE_ADDON
		IDirect3DSurface9 *const surface = *ppSurface;

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		const auto device_proxy = this;
		surface->SetPrivateData(__uuidof(Direct3DDevice9), &device_proxy, sizeof(device_proxy), 0);
#endif

		// In case surface was replaced with a texture resource
		const reshade::api::resource resource = get_resource_from_view(to_handle(surface));

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this,
			desc,
			nullptr,
			reshade::api::resource_usage::depth_stencil,
			resource);
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			resource,
			reshade::api::resource_usage::depth_stencil,
			reshade::api::resource_view_desc(desc.texture.format),
			to_handle(surface));

		// Add reference to device, to ensure the proxy device is not destroyed before this resource has been destroyed
		InterlockedIncrement(&_ref);

		register_destruction_callback_d3d9(reinterpret_cast<IDirect3DResource9 *>(resource.handle), [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, resource);

			const ULONG ref = InterlockedDecrement(&_ref);
			assert(ref != 0);
		});
		register_destruction_callback_d3d9(surface, [this, surface]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, to_handle(surface));
		}, to_handle(surface).handle == resource.handle ? 1 : 0);
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "IDirect3DDevice9Ex::CreateDepthStencilSurfaceEx" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	LOG(INFO) << "Redirecting " << "IDirect3DDevice9Ex::ResetEx" << '(' << "this = " << this << ", pPresentationParameters = " << pPresentationParameters << ", pFullscreenDisplayMode = " << pFullscreenDisplayMode << ')' << " ...";

	if (pPresentationParameters == nullptr)
		return D3DERR_INVALIDCALL;

	D3DDISPLAYMODEEX fullscreen_mode = { sizeof(fullscreen_mode) };
	if (pFullscreenDisplayMode != nullptr)
		fullscreen_mode = *pFullscreenDisplayMode;
	D3DPRESENT_PARAMETERS pp = *pPresentationParameters;
	dump_and_modify_present_parameters(pp, fullscreen_mode, _d3d.get(), _cp.AdapterOrdinal);

	// Release all resources before performing reset
	_implicit_swapchain->on_reset();
	device_impl::on_reset();

	assert(_extended_interface);
	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->ResetEx(&pp, pp.Windowed ? nullptr : &fullscreen_mode);

	// Update output values (see https://docs.microsoft.com/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-resetex)
	pPresentationParameters->BackBufferWidth = pp.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = pp.BackBufferHeight;
	pPresentationParameters->BackBufferFormat = pp.BackBufferFormat;
	pPresentationParameters->BackBufferCount = pp.BackBufferCount;

	if (FAILED(hr))
	{
		LOG(ERROR) << "IDirect3DDevice9Ex::ResetEx" << " failed with error code " << hr << '!';
		return hr;
	}

	device_impl::on_init(pp);
	_implicit_swapchain->on_init(pp);

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	if (iSwapChain != 0)
	{
		LOG(WARN) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";
		return D3DERR_INVALIDCALL;
	}

	assert(_extended_interface);
	assert(_implicit_swapchain->_extended_interface);
	return static_cast<IDirect3DSwapChain9Ex *>(_implicit_swapchain)->GetDisplayModeEx(pMode, pRotation);
}
