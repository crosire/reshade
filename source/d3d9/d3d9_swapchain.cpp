/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "addon_manager.hpp"
#include "runtime_manager.hpp"

bool Direct3DSwapChain9::is_presenting_entire_surface(const RECT *source_rect, HWND hwnd)
{
	if (source_rect == nullptr || hwnd == nullptr)
		return true;

	RECT window_rect = {};
	GetClientRect(hwnd, &window_rect);
	return source_rect->left == window_rect.left && source_rect->top == window_rect.top &&
	       source_rect->right == window_rect.right && source_rect->bottom == window_rect.bottom;
}

Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original) :
	swapchain_impl(device, original),
	_extended_interface(0),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);

	reshade::create_effect_runtime(this, device);
	on_init();
}
Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original) :
	Direct3DSwapChain9(device, static_cast<IDirect3DSwapChain9 *>(original))
{
	_extended_interface = 1;
}
Direct3DSwapChain9::~Direct3DSwapChain9()
{
	on_reset();
	reshade::destroy_effect_runtime(this);
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
		LOG(DEBUG) << "Upgrading IDirect3DSwapChain9 object " << this << " to IDirect3DSwapChain9Ex.";
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
	{
		_orig->Release();
		return ref;
	}

	const auto it = std::find(_device->_additional_swapchains.begin(), _device->_additional_swapchains.end(), this);
	if (it != _device->_additional_swapchains.end())
	{
		_device->_additional_swapchains.erase(it);
		_device->Release(); // Remove the reference that was added in 'Direct3DDevice9::CreateAdditionalSwapChain'
	}

	const auto orig = _orig;
	const bool extended_interface = _extended_interface;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "IDirect3DSwapChain9" << (extended_interface ? "Ex" : "") << " object " << this << " (" << orig << ").";
#endif
	this->~Direct3DSwapChain9();

	// Only release internal reference after the effect runtime has been destroyed, so any references it held are cleaned up at this point
	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "IDirect3DSwapChain9" << (extended_interface ? "Ex" : "") << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";

	operator delete(this, sizeof(Direct3DSwapChain9));
	return 0;
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	// Skip when no presentation is requested
	if (((dwFlags & D3DPRESENT_DONOTFLIP) == 0) &&
		// Also skip when the same frame is presented multiple times
		((dwFlags & D3DPRESENT_DONOTWAIT) == 0 || !_was_still_drawing_last_frame))
	{
		assert(!_was_still_drawing_last_frame);

		on_present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}

	const HRESULT hr = _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	handle_device_loss(hr);

	return hr;
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

void Direct3DSwapChain9::on_init()
{
	assert(!_is_initialized);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(this);

	D3DPRESENT_PARAMETERS pp = {};
	_orig->GetPresentParameters(&pp);
	reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(this, pp.Windowed == FALSE, nullptr);
#endif

	reshade::init_effect_runtime(this);

	_is_initialized = true;
}
void Direct3DSwapChain9::on_reset()
{
	// May be called without a previous call to 'on_init' if a device reset had failed
	if (!_is_initialized)
		return;

	reshade::reset_effect_runtime(this);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this);
#endif

	_back_buffer.reset();

	_is_initialized = false;
}

void Direct3DSwapChain9::on_present(const RECT *source_rect, [[maybe_unused]] const RECT *dest_rect, HWND window_override, [[maybe_unused]] const RGNDATA *dirty_region)
{
	assert(_is_initialized);

	if (SUCCEEDED(_device->_orig->BeginScene()))
	{
		_hwnd = window_override;

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(
			_device,
			this,
			reinterpret_cast<const reshade::api::rect *>(source_rect),
			reinterpret_cast<const reshade::api::rect *>(dest_rect),
			dirty_region != nullptr ? dirty_region->rdh.nCount : 0,
			dirty_region != nullptr ? reinterpret_cast<const reshade::api::rect *>(dirty_region->Buffer) : nullptr);
#endif

		// Only call into the effect runtime if the entire surface is presented, to avoid partial updates messing up effects and the GUI
		if (is_presenting_entire_surface(source_rect, window_override))
			reshade::present_effect_runtime(this, _device);

		_hwnd = nullptr;

		_device->_orig->EndScene();
	}
}

void Direct3DSwapChain9::handle_device_loss(HRESULT hr)
{
	_was_still_drawing_last_frame = (hr == D3DERR_WASSTILLDRAWING);

	// Ignore D3DERR_DEVICELOST, since it can frequently occur when minimizing out of exclusive fullscreen
	if (hr == D3DERR_DEVICEREMOVED || hr == D3DERR_DEVICEHUNG)
	{
		LOG(ERROR) << "Device was lost with " << hr << '!';
		// Do not clean up resources, since application has to call 'IDirect3DDevice9::Reset' anyway, which will take care of that
	}
}
