/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

#include <d3d12.h>

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

class D3D12Device;

class D3D12DeviceExt : public IUnknown
{
public:
	D3D12DeviceExt(D3D12Device *device, IUnknown *original);

	D3D12DeviceExt(const D3D12DeviceExt &) = delete;
	D3D12DeviceExt &operator=(const D3D12DeviceExt &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12DeviceExt
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device);
	virtual BOOL    STDMETHODCALLTYPE GetExtensionSupport(UINT extension);
	virtual HRESULT STDMETHODCALLTYPE CreateCubinComputeShaderWithName(const void *cubin_data, UINT32 cubin_size, UINT32 block_x, UINT32 block_y, UINT32 block_z, const char *shader_name, void **handle);
	virtual HRESULT STDMETHODCALLTYPE DestroyCubinComputeShader(void *handle);
	virtual HRESULT STDMETHODCALLTYPE GetCudaTextureObject(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle);
	virtual HRESULT STDMETHODCALLTYPE GetCudaSurfaceObject(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle);
	virtual HRESULT STDMETHODCALLTYPE CaptureUAVInfo(void *uav_info);
	#pragma endregion
	#pragma region ID3D12DeviceExt1
	virtual HRESULT STDMETHODCALLTYPE CreateResourceFromBorrowedHandle(const D3D12_RESOURCE_DESC1 *desc, UINT64 vk_handle, ID3D12Resource **resource);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfoEx(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_index, UINT32 *vk_queue_flags, UINT32 *vk_queue_family);
	#pragma endregion
	#pragma region ID3D12DeviceExt2
	virtual BOOL    STDMETHODCALLTYPE SupportsCubin64bit();
	virtual HRESULT STDMETHODCALLTYPE CreateCubinComputeShaderExV2(void *params);
	virtual HRESULT STDMETHODCALLTYPE GetCudaMergedTextureSamplerObject(D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params);
	virtual HRESULT STDMETHODCALLTYPE GetCudaIndependentDescriptorObject(D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params);
	#pragma endregion
	#pragma region ID3D12DeviceExt3
	virtual BOOL    STDMETHODCALLTYPE SupportsAGSExtension(UINT agsExtension);
	virtual HRESULT STDMETHODCALLTYPE SetAGSUAVSlot(UINT uavSlot);
	#pragma endregion
	#pragma region ID3D12DeviceExt4
	virtual BOOL    STDMETHODCALLTYPE IsNvShaderExtnOpCodeSupported(UINT32 op_code);
	virtual HRESULT STDMETHODCALLTYPE SetNvShaderExtnSlotSpace(UINT32 uav_slot, UINT32 uav_space, BOOL local_thread);
	#pragma endregion
	#pragma region ID3D12DeviceExt5
	virtual BOOL    STDMETHODCALLTYPE SetCreatePipelineStateFlagsNVAPI(UINT pipeline_state_flags);
	#pragma endregion

	IUnknown *_orig;

private:
	D3D12Device *const _parent_device;
};

class D3D12DXVKInteropDevice : public IUnknown
{
public:
	D3D12DXVKInteropDevice(D3D12Device *device, IUnknown *original);

	D3D12DXVKInteropDevice(const D3D12DXVKInteropDevice &) = delete;
	D3D12DXVKInteropDevice &operator=(const D3D12DXVKInteropDevice &) = delete;

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D12DXVKInteropDevice
	virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(REFIID iid, void **object);
	virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(UINT *extension_count, const char **extensions);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(UINT *extension_count, const char **extensions);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(const void **features);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfo(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_family);
	virtual void    STDMETHODCALLTYPE GetVulkanImageLayout(ID3D12Resource *resource, D3D12_RESOURCE_STATES state, UINT *vk_layout);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset);
	virtual HRESULT STDMETHODCALLTYPE LockCommandQueue(ID3D12CommandQueue *queue);
	virtual HRESULT STDMETHODCALLTYPE UnlockCommandQueue(ID3D12CommandQueue *queue);
	#pragma endregion
	#pragma region ID3D12DXVKInteropDevice1
	virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo1(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset, UINT *format);
	virtual HRESULT STDMETHODCALLTYPE CreateInteropCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, UINT32 vk_queue_family_index, ID3D12CommandQueue **ppCommandQueue);
	virtual HRESULT STDMETHODCALLTYPE CreateInteropCommandAllocator(D3D12_COMMAND_LIST_TYPE type, UINT32 vk_queue_family_index, ID3D12CommandAllocator **ppAllocator);
	virtual HRESULT STDMETHODCALLTYPE BeginVkCommandBufferInterop(ID3D12CommandList *pCmdList, void *vk_command_buffer);
	virtual HRESULT STDMETHODCALLTYPE EndVkCommandBufferInterop(ID3D12CommandList *pCmdList);
	#pragma endregion
	#pragma region ID3D12DXVKInteropDevice2
	virtual HRESULT STDMETHODCALLTYPE LockVulkanQueue(ID3D12CommandQueue *queue);
	virtual HRESULT STDMETHODCALLTYPE UnlockVulkanQueue(ID3D12CommandQueue *queue);
	#pragma endregion
	#pragma region ID3D12DXVKInteropDevice3
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHeapInfo(ID3D12Heap *heap, UINT64 *vk_memory, UINT64 *heap_offset, UINT32 *vk_memory_type);
	#pragma endregion

	IUnknown *_orig;

private:
	D3D12Device *const _parent_device;
};

#endif
