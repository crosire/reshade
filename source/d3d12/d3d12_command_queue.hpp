#pragma once

#include "d3d12.hpp"

struct D3D12CommandQueue : ID3D12CommandQueue
{
	D3D12CommandQueue(D3D12Device *device, ID3D12CommandQueue *original) :
		_orig(original),
		_device(device) { }

	D3D12CommandQueue(const D3D12CommandQueue &) = delete;
	D3D12CommandQueue &operator=(const D3D12CommandQueue &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12Object
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;
	#pragma endregion
	#pragma region ID3D12DeviceChild
	virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppvDevice) override;
	#pragma endregion
	#pragma region ID3D12CommandQueue
	virtual void STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags) override;
	virtual void STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) override;
	virtual void STDMETHODCALLTYPE ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists) override;
	virtual void STDMETHODCALLTYPE SetMarker(UINT Metadata, const void *pData, UINT Size) override;
	virtual void STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void *pData, UINT Size) override;
	virtual void STDMETHODCALLTYPE EndEvent() override;
	virtual HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence *pFence, UINT64 Value) override;
	virtual HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence *pFence, UINT64 Value) override;
	virtual HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64 *pFrequency) override;
	virtual HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp) override;
	virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() override;
	#pragma endregion

	ULONG _ref = 1;
	ID3D12CommandQueue *const _orig;
	D3D12Device *const _device;
};
