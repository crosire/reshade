/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <d3d9on12.h>

struct Direct3DDevice9;
struct D3D12Device;

struct DECLSPEC_UUID("C09BBC5E-FC80-4D9A-A46E-F67542343008") Direct3DDevice9On12 final : IDirect3DDevice9On12
{
	Direct3DDevice9On12(Direct3DDevice9 *device_9, D3D12Device *device_12, IDirect3DDevice9On12 *original);
	~Direct3DDevice9On12();

	Direct3DDevice9On12(const Direct3DDevice9On12 &) = delete;
	Direct3DDevice9On12 &operator=(const Direct3DDevice9On12 &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DDevice9On12
	HRESULT STDMETHODCALLTYPE GetD3D12Device(REFIID riid, void **ppvDevice) override;
	HRESULT STDMETHODCALLTYPE UnwrapUnderlyingResource(IDirect3DResource9 *pResource, ID3D12CommandQueue *pCommandQueue, REFIID riid, void **ppvResource12) override;
	HRESULT STDMETHODCALLTYPE ReturnUnderlyingResource(IDirect3DResource9 *pResource, UINT NumSync, UINT64 *pSignalValues, ID3D12Fence **ppFences) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	IDirect3DDevice9On12 *_orig;

	Direct3DDevice9 *const _parent_device_9;
	D3D12Device *const _parent_device_12;
};
