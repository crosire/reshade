/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "runtime_d3d9.hpp"

Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime) :
	_orig(original),
	_extended_interface(0),
	_device(device),
	_runtime(runtime) {}
Direct3DSwapChain9::Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime) :
	_orig(original),
	_extended_interface(1),
	_device(device),
	_runtime(runtime) {}

bool Direct3DSwapChain9::check_and_upgrade_interface(REFIID riid)
{
	if (_extended_interface || riid != __uuidof(IDirect3DSwapChain9Ex))
		return true;

	IDirect3DSwapChain9Ex *new_interface = nullptr;
	if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&new_interface))))
		return false;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Upgraded IDirect3DSwapChain9 object " << this << " to IDirect3DSwapChain9Ex.";
#endif
	_orig->Release();
	_orig = new_interface;
	_extended_interface = true;

	return true;
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DSwapChain9) ||
		riid == __uuidof(IDirect3DSwapChain9Ex))
	{
		if (!check_and_upgrade_interface(riid))
			return E_NOINTERFACE;

		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DSwapChain9::AddRef()
{
	++_ref;

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DSwapChain9::Release()
{
	if (--_ref == 0)
	{
		assert(_runtime != nullptr);
		_runtime->on_reset();
		_runtime.reset();

		const auto it = std::find(_device->_additional_swapchains.begin(), _device->_additional_swapchains.end(), this);
		if (it != _device->_additional_swapchains.end())
		{
			_device->_additional_swapchains.erase(it);
			_device->Release(); // Remove the reference that was added in 'Direct3DDevice9::CreateAdditionalSwapChain'
		}
	}

	const ULONG ref = _orig->Release();

	if (ref != 0 && _ref != 0)
		return ref;
	else if (ref != 0)
		LOG(WARN) << "Reference count for IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << " object " << this << " is inconsistent: " << ref << ", but expected 0.";

	assert(_ref <= 0);
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << " object " << this << '.';
#endif
	delete this;
	return 0;
}

HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(_runtime != nullptr);
	_runtime->on_present();

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
