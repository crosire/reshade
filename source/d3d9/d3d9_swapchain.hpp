/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime_d3d9.hpp"

struct Direct3DDevice9;

struct DECLSPEC_UUID("BC52FCE4-1EAC-40C8-84CF-863600BBAA01") Direct3DSwapChain9 final : IDirect3DSwapChain9Ex, public reshade::d3d9::runtime_impl
{
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original);
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original);

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DSwapChain9
	HRESULT STDMETHODCALLTYPE Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags) override;
	HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9 *pDestSurface) override;
	HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
	HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override;
	HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *pMode) override;
	HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
	HRESULT STDMETHODCALLTYPE GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
	#pragma endregion
	#pragma region IDirect3DSwapChain9Ex
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;
	HRESULT STDMETHODCALLTYPE GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics) override;
	HRESULT STDMETHODCALLTYPE GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) override;
	#pragma endregion

	static bool is_presenting_entire_surface(const RECT *source_rect, HWND hwnd);

	bool check_and_upgrade_interface(REFIID riid);

	LONG _ref = 1;
	bool _extended_interface;
	Direct3DDevice9 *const _device;
};
