/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d9_device.hpp"
#include "d3d9_swapchain.hpp"
#include "runtime_d3d9.hpp"

// IDirect3DSwapChain9
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DSwapChain9) ||
		riid == __uuidof(IDirect3DSwapChain9Ex))
	{
		#pragma region Update to IDirect3DSwapChain9Ex interface
		if (!_extended_interface && riid == __uuidof(IDirect3DSwapChain9Ex))
		{
			IDirect3DSwapChain9Ex *swapchainex = nullptr;

			if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&swapchainex))))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'IDirect3DSwapChain9' object " << this << " to 'IDirect3DSwapChain9Ex'.";
#endif
			_orig = swapchainex;
			_extended_interface = true;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::Release()
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

			_device->Release();
		}
	}

	ULONG ref = _orig->Release();

	if (_ref == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(_ref <= 0);

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Destroyed 'IDirect3DSwapChain9" << (_extended_interface ? "Ex" : "") << "' object " << this << ".";
#endif
		delete this;
	}

	return ref;
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
	{
		return D3DERR_INVALIDCALL;
	}

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	return _orig->GetPresentParameters(pPresentationParameters);
}

// IDirect3DSwapChain9Ex
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
