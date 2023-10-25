/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <D3D12Downlevel.h>

struct D3D12Device;

struct DECLSPEC_UUID("918B5021-E085-430C-BA85-D9C0EFE6FAA0") D3D12DeviceDownlevel final : ID3D12DeviceDownlevel
{
	D3D12DeviceDownlevel(D3D12Device *device, ID3D12DeviceDownlevel *original);

	D3D12DeviceDownlevel(const D3D12DeviceDownlevel &) = delete;
	D3D12DeviceDownlevel &operator=(const D3D12DeviceDownlevel &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12DeviceDownlevel
	HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo) override;
	#pragma endregion

	ID3D12DeviceDownlevel *const _orig;
	D3D12Device *const _parent_device;
};
