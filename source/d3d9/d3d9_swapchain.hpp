/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d9.h>
#include <memory> // std::shared_ptr

struct Direct3DDevice9;
namespace reshade::d3d9 { class runtime_d3d9; }

struct DECLSPEC_UUID("BC52FCE4-1EAC-40C8-84CF-863600BBAA01") Direct3DSwapChain9 : IDirect3DSwapChain9Ex
{
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime);
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original, const std::shared_ptr<reshade::d3d9::runtime_d3d9> &runtime);

	Direct3DSwapChain9(const Direct3DSwapChain9 &) = delete;
	Direct3DSwapChain9 &operator=(const Direct3DSwapChain9 &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

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
	IDirect3DSwapChain9 *_orig;
	bool _extended_interface;
	Direct3DDevice9 *const _device;
	std::shared_ptr<reshade::d3d9::runtime_d3d9> _runtime;
};
