/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "d3d12_impl_command_queue.hpp"

struct D3D12Device;
struct D3D12CommandQueueDownlevel;

struct DECLSPEC_UUID("2C576D2A-0C1C-4D1D-AD7C-BC4FAEC15ABC") D3D12CommandQueue final : ID3D12CommandQueue1, public reshade::d3d12::command_queue_impl
{
	D3D12CommandQueue(D3D12Device *device, ID3D12CommandQueue *original);
	~D3D12CommandQueue();

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
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
	#pragma region ID3D12CommandQueue
	HRESULT STDMETHODCALLTYPE SetProcessPriority(D3D12_COMMAND_QUEUE_PROCESS_PRIORITY Priority) override;
	HRESULT STDMETHODCALLTYPE GetProcessPriority(D3D12_COMMAND_QUEUE_PROCESS_PRIORITY *pOutValue) override;
	HRESULT STDMETHODCALLTYPE SetGlobalPriority(D3D12_COMMAND_QUEUE_GLOBAL_PRIORITY Priority) override;
	HRESULT STDMETHODCALLTYPE GetGlobalPriority(D3D12_COMMAND_QUEUE_GLOBAL_PRIORITY *pOutValue) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	ULONG _ref = 1;
	unsigned short _interface_version = 0;
	D3D12Device *const _device;
	D3D12CommandQueueDownlevel *_downlevel = nullptr;
};
