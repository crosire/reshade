/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <D3D12Downlevel.h>

struct DECLSPEC_UUID("918B5021-E085-430C-BA85-D9C0EFE6FAA0") D3D12DeviceDownlevel : ID3D12DeviceDownlevel
{
	D3D12DeviceDownlevel(D3D12Device *device, ID3D12DeviceDownlevel *original);

	D3D12DeviceDownlevel(const D3D12DeviceDownlevel &) = delete;
	D3D12DeviceDownlevel &operator=(const D3D12DeviceDownlevel &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

	#pragma region ID3D12DeviceDownlevel
	HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo) override;
	#pragma endregion

	ULONG _ref = 1;
	ID3D12DeviceDownlevel *_orig;
};
