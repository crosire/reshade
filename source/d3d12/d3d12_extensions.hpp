/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#if RESHADE_ADDON >= 2

class D3D12Device;

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

// ABI-compatible proxy for the vkd3d-proton ID3D12DeviceExt interface family.
// See https://github.com/HansKristian-Work/vkd3d-proton/blob/master/include/vkd3d_device_vkd3d_ext.idl
class D3D12DeviceExt final : public IUnknown
{
public:
	explicit D3D12DeviceExt(D3D12Device *device);
	~D3D12DeviceExt();

	#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;
	#pragma endregion

	// ID3D12DeviceExt
	virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device);
	virtual BOOL    STDMETHODCALLTYPE GetExtensionSupport(UINT extension);
	virtual HRESULT STDMETHODCALLTYPE CreateCubinComputeShaderWithName(const void *cubin_data, UINT32 cubin_size, UINT32 block_x, UINT32 block_y, UINT32 block_z, const char *shader_name, void **handle);
	virtual HRESULT STDMETHODCALLTYPE DestroyCubinComputeShader(void *handle);
	virtual HRESULT STDMETHODCALLTYPE GetCudaTextureObject(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle);
	virtual HRESULT STDMETHODCALLTYPE GetCudaSurfaceObject(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle);
	virtual HRESULT STDMETHODCALLTYPE CaptureUAVInfo(void *uav_info);

	// ID3D12DeviceExt1
	virtual HRESULT STDMETHODCALLTYPE CreateResourceFromBorrowedHandle(const D3D12_RESOURCE_DESC1 *desc, UINT64 vk_handle, ID3D12Resource **resource);
	virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfoEx(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_index, UINT32 *vk_queue_flags, UINT32 *vk_queue_family);

	// ID3D12DeviceExt2
	virtual BOOL    STDMETHODCALLTYPE SupportsCubin64bit();
	virtual HRESULT STDMETHODCALLTYPE CreateCubinComputeShaderExV2(void *params);
	virtual HRESULT STDMETHODCALLTYPE GetCudaMergedTextureSamplerObject(D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params);
	virtual HRESULT STDMETHODCALLTYPE GetCudaIndependentDescriptorObject(D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params);

	// ID3D12DeviceExt3-5
	virtual BOOL    STDMETHODCALLTYPE SupportsAGSExtension(UINT ags_extension);
	virtual HRESULT STDMETHODCALLTYPE SetAGSUAVSlot(UINT uav_slot);
	virtual BOOL    STDMETHODCALLTYPE IsNvShaderExtnOpCodeSupported(UINT32 op_code);
	virtual HRESULT STDMETHODCALLTYPE SetNvShaderExtnSlotSpace(UINT32 uav_slot, UINT32 uav_space, BOOL local_thread);
	virtual BOOL    STDMETHODCALLTYPE SetCreatePipelineStateFlagsNVAPI(UINT pipeline_state_flags);

private:
	HRESULT check_and_upgrade_interface(REFIID riid);

	D3D12Device *const _parent_device;
	IUnknown *_orig = nullptr;
	unsigned short _interface_version = 0;
};

#endif
