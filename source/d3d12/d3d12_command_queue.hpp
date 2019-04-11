/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d12.h>

struct __declspec(uuid("2C576D2A-0C1C-4D1D-AD7C-BC4FAEC15ABC")) D3D12CommandQueue : ID3D12CommandQueue
{
	D3D12CommandQueue(D3D12Device *device, ID3D12CommandQueue *original);

	D3D12CommandQueue(const D3D12CommandQueue &) = delete;
	D3D12CommandQueue &operator=(const D3D12CommandQueue &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

	#pragma region ID3D12Object
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;
	#pragma endregion
	#pragma region ID3D12DeviceChild
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppvDevice) override;
	#pragma endregion
	#pragma region ID3D12CommandQueue
	void    STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags) override;
	void    STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) override;
	void    STDMETHODCALLTYPE ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists) override;
	void    STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size) override;
	void    STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size) override;
	void    STDMETHODCALLTYPE EndEvent() override;
	HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence *pFence, UINT64 Value) override;
	HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence *pFence, UINT64 Value) override;
	HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *pFrequency) override;
	HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp) override;
	D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() override;
	#pragma endregion

	ULONG _ref = 1;
	ID3D12CommandQueue *const _orig;
	D3D12Device *const _device;
};
