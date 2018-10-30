/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "d3d9.hpp"
#include <memory>

struct Direct3DSwapChain9 : IDirect3DSwapChain9Ex
{
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime) :
		_orig(original),
		_extended_interface(false),
		_device(device),
		_runtime(runtime) { }
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime) :
		_orig(original),
		_extended_interface(true),
		_device(device),
		_runtime(runtime) { }

	Direct3DSwapChain9(const Direct3DSwapChain9 &) = delete;
	Direct3DSwapChain9 &operator=(const Direct3DSwapChain9 &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DSwapChain9
	virtual HRESULT STDMETHODCALLTYPE Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags) override;
	virtual HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9 *pDestSurface) override;
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
	virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *pMode) override;
	virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
	virtual HRESULT STDMETHODCALLTYPE GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
	#pragma endregion
	#pragma region IDirect3DSwapChain9Ex
	virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics) override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) override;
	#pragma endregion

	LONG _ref = 1;
	IDirect3DSwapChain9 *_orig;
	bool _extended_interface;
	Direct3DDevice9 *const _device;
	std::shared_ptr<reshade::d3d9::runtime_d3d9> _runtime;
};
