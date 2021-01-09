/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "runtime_d3d9.hpp"

Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original) :
	_orig(original),
	_extended_interface(0),
	_device(device),
	_runtime(new reshade::d3d9::runtime_d3d9(device->_orig, original, &device->_state))
{
	assert(_orig != nullptr && _device != nullptr);
}
Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original) :
	_orig(original),
	_extended_interface(1),
	_device(device),
	_runtime(new reshade::d3d9::runtime_d3d9(device->_orig, original, &device->_state))
{
	assert(_orig != nullptr && _device != nullptr);
}

bool Direct3DSwapChain9::is_presenting_entire_surface(const RECT *source_rect, HWND hwnd)
{
	if (source_rect == nullptr || hwnd == nullptr)
		return true;

	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);
	return source_rect->left == window_rect.left && source_rect->top == window_rect.top &&
	       source_rect->right == window_rect.right && source_rect->bottom == window_rect.bottom;
}

bool Direct3DSwapChain9::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DSwapChain9))
		return true;
	if (riid != __uuidof(IDirect3DSwapChain9Ex))
		return false;

	if (!_extended_interface)
	{
		IDirect3DSwapChain9Ex *new_interface = nullptr;
		if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&new_interface))))
			return false;
#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Upgraded IDirect3DSwapChain9 object " << this << " to IDirect3DSwapChain9Ex.";
#endif
		_orig->Release();
		_orig = new_interface;
		_extended_interface = true;
	}

	return true;
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
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
ULONG   STDMETHODCALLTYPE Direct3DSwapChain9::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE Direct3DSwapChain9::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	delete _runtime;

	const auto it = std::find(_device->_additional_swapchains.begin(), _device->_additional_swapchains.end(), this);
	if (it != _device->_additional_swapchains.end())
	{
		_device->_additional_swapchains.erase(it);
		_device->Release(); // Remove the reference that was added in 'Direct3DDevice9::CreateAdditionalSwapChain'
	}

	// Only release internal reference after the runtime has been destroyed, so any references it held are cleaned up at this point
	const ULONG ref_orig = _orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << " object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << " object " << this << '.';
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	// Only call into runtime if the entire surface is presented, to avoid partial updates messing up effects and the GUI
	if (is_presenting_entire_surface(pSourceRect, hDestWindowOverride))
		_runtime->on_present();
	_device->_state.reset(false);

	return _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetFrontBufferData(IDirect3DSurface9 *pDestSurface)
{
	return _orig->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	return _orig->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
	return _orig->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
	return _orig->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	if (ppDevice == nullptr)
		return D3DERR_INVALIDCALL;

	_device->AddRef();
	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	return _orig->GetPresentParameters(pPresentationParameters);
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetLastPresentCount(UINT *pLastPresentCount)
{
	assert(_extended_interface);
	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetLastPresentCount(pLastPresentCount);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics)
{
	assert(_extended_interface);
	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetPresentStats(pPresentationStatistics);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	assert(_extended_interface);
	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetDisplayModeEx(pMode, pRotation);
}
