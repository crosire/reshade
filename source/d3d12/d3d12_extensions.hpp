/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

struct D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS
{
	void *pNext;
	D3D12_CPU_DESCRIPTOR_HANDLE texDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE smpDesc;
	UINT64 textureHandle;
};

struct D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS
{
	void *pNext;
	enum D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_TYPE type;
	D3D12_CPU_DESCRIPTOR_HANDLE desc;
	UINT64 handle;
};

// See https://github.com/HansKristian-Work/vkd3d-proton/blob/master/include/vkd3d_device_vkd3d_ext.idl
HRESULT ID3D12DeviceExt_GetCudaTextureObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle);
HRESULT ID3D12DeviceExt_GetCudaSurfaceObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle);

HRESULT ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject(IUnknown *device_ext, D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params);
HRESULT ID3D12DeviceExt2_GetCudaIndependentDescriptorObject(IUnknown *device_ext, D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params);

#endif

class D3D12Device;

// Pass-through wrapper for command queues created through 'ID3D12DXVKInteropDevice1::CreateInteropCommandQueue'.
// Command lists created through the ReShade device are proxies, which vkd3d-proton does not recognize, so they have to be unwrapped before being submitted to such a queue.
// This does not use the full 'D3D12CommandQueue' proxy, since these queues may belong to a Vulkan queue family ReShade cannot submit its own work to.
class DECLSPEC_UUID("5E3A2F71-6C0D-4B8E-9A41-2D7C8B6F0E93") D3D12InteropCommandQueue final : public ID3D12CommandQueue
{
public:
	D3D12InteropCommandQueue(D3D12Device *device, ID3D12CommandQueue *original);
	~D3D12InteropCommandQueue();

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

	D3D12Device *const _device;
	ID3D12CommandQueue *const _orig;
	LONG _ref = 1;
};

// ABI-compatible proxy for the vkd3d-proton ID3D12DXVKInteropDevice interface family, which DXVK-NVAPI uses for optical flow (and with it DLSS Frame Generation 3.x).
// Every method that takes a command queue or command list is given the original object instead of a ReShade proxy, since vkd3d-proton cannot resolve a proxy and dereferences a null pointer otherwise.
// See https://github.com/HansKristian-Work/vkd3d-proton/blob/master/include/vkd3d_device_vkd3d_ext.idl
class D3D12DXVKInteropDevice final : public IUnknown
{
public:
	explicit D3D12DXVKInteropDevice(D3D12Device *device);
	~D3D12DXVKInteropDevice();

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion

	// ID3D12DXVKInteropDevice
	virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(REFIID iid, void **object);
	virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(UINT *extension_count, const char **extensions);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(UINT *extension_count, const char **extensions);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(const void **features);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfo(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_family);
	virtual void    STDMETHODCALLTYPE GetVulkanImageLayout(ID3D12Resource *resource, D3D12_RESOURCE_STATES state, UINT32 *vk_layout);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset);
	virtual HRESULT STDMETHODCALLTYPE LockCommandQueue(ID3D12CommandQueue *queue);
	virtual HRESULT STDMETHODCALLTYPE UnlockCommandQueue(ID3D12CommandQueue *queue);

	// ID3D12DXVKInteropDevice1
	virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo1(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset, UINT32 *format);
	virtual HRESULT STDMETHODCALLTYPE CreateInteropCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc, UINT32 vk_queue_family_index, ID3D12CommandQueue **queue);
	virtual HRESULT STDMETHODCALLTYPE CreateInteropCommandAllocator(D3D12_COMMAND_LIST_TYPE type, UINT32 vk_queue_family_index, ID3D12CommandAllocator **allocator);
	virtual HRESULT STDMETHODCALLTYPE BeginVkCommandBufferInterop(ID3D12CommandList *command_list, void *vk_command_buffer);
	virtual HRESULT STDMETHODCALLTYPE EndVkCommandBufferInterop(ID3D12CommandList *command_list);

	// ID3D12DXVKInteropDevice2
	virtual HRESULT STDMETHODCALLTYPE LockVulkanQueue(ID3D12CommandQueue *queue);
	virtual HRESULT STDMETHODCALLTYPE UnlockVulkanQueue(ID3D12CommandQueue *queue);

	// ID3D12DXVKInteropDevice3
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHeapInfo(ID3D12Heap *heap, UINT64 *vk_memory, UINT64 *heap_offset, UINT32 *vk_memory_type);

private:
	HRESULT check_and_upgrade_interface(REFIID riid);

	D3D12Device *const _parent_device;
	IUnknown *_orig = nullptr;
	unsigned short _interface_version = 0;
};

// Returns the original object behind a ReShade command queue proxy (or an interop command queue wrapper), or the argument itself if it is not one.
// The result is not reference counted, it is kept alive by the argument.
ID3D12CommandQueue *unwrap_command_queue(ID3D12CommandQueue *queue);
// Returns the original object behind a ReShade command list proxy, or the argument itself if it is not one.
ID3D12CommandList *unwrap_command_list(ID3D12CommandList *command_list);
