/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "d3d9_impl_swapchain.hpp"

struct Direct3DDevice9;

struct DECLSPEC_UUID("BC52FCE4-1EAC-40C8-84CF-863600BBAA01") Direct3DSwapChain9 final : IDirect3DSwapChain9Ex, public reshade::d3d9::swapchain_impl
{
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9   *original);
	Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9Ex *original);
	~Direct3DSwapChain9();

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

	void on_init([[maybe_unused]] bool resize);
	void on_reset([[maybe_unused]] bool resize);
	void on_present(const RECT *source_rect, [[maybe_unused]] const RECT *dest_rect, HWND window_override, [[maybe_unused]] const RGNDATA *dirty_region, DWORD flags);
	void on_finish_present(HRESULT hr);

	bool check_and_upgrade_interface(REFIID riid);

	LONG _ref = 1;
	bool _extended_interface;
	Direct3DDevice9 *const _device;
	bool _is_initialized = false;
	bool _was_still_drawing_last_frame = false;
};
