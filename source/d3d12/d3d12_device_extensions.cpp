/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_device_extensions.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp"

D3D12DeviceExt::D3D12DeviceExt(D3D12Device *device, IUnknown *original) :
	_orig(original),
	_parent_device(device)
{
	assert(_orig != nullptr && _parent_device != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12DeviceExt::QueryInterface(REFIID riid, void **ppvObj)
{
	return _parent_device->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12DeviceExt::AddRef()
{
	return _parent_device->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12DeviceExt::Release()
{
	return _parent_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device)
{
	return static_cast<D3D12DeviceExt *>(_orig)->GetVulkanHandles(vk_instance, vk_physical_device, vk_device);
}
BOOL    STDMETHODCALLTYPE D3D12DeviceExt::GetExtensionSupport(UINT extension)
{
	return static_cast<D3D12DeviceExt *>(_orig)->GetExtensionSupport(extension);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateCubinComputeShaderWithName(const void *cubin_data, UINT32 cubin_size, UINT32 block_x, UINT32 block_y, UINT32 block_z, const char *shader_name, void **handle)
{
	return static_cast<D3D12DeviceExt *>(_orig)->CreateCubinComputeShaderWithName(cubin_data, cubin_size, block_x, block_y, block_z, shader_name, handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::DestroyCubinComputeShader(void *handle)
{
	return static_cast<D3D12DeviceExt *>(_orig)->DestroyCubinComputeShader(handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaTextureObject(D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle)
{
	srv_handle = _parent_device->convert_to_original_cpu_descriptor_handle(srv_handle);
	sampler_handle = _parent_device->convert_to_original_cpu_descriptor_handle(sampler_handle);

	return static_cast<D3D12DeviceExt *>(_orig)->GetCudaTextureObject(srv_handle, sampler_handle, cuda_texture_handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaSurfaceObject(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle)
{
	uav_handle = _parent_device->convert_to_original_cpu_descriptor_handle(uav_handle);

	return static_cast<D3D12DeviceExt *>(_orig)->GetCudaSurfaceObject(uav_handle, cuda_surface_handle);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CaptureUAVInfo(void *uav_info)
{
	return static_cast<D3D12DeviceExt *>(_orig)->CaptureUAVInfo(uav_info);
}

HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateResourceFromBorrowedHandle(const D3D12_RESOURCE_DESC1 *desc, UINT64 vk_handle, ID3D12Resource **resource)
{
	return static_cast<D3D12DeviceExt *>(_orig)->CreateResourceFromBorrowedHandle(desc, vk_handle, resource);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetVulkanQueueInfoEx(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_index, UINT32 *vk_queue_flags, UINT32 *vk_queue_family)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DeviceExt *>(_orig)->GetVulkanQueueInfoEx(queue, vk_queue, vk_queue_index, vk_queue_flags, vk_queue_family);
}

BOOL    STDMETHODCALLTYPE D3D12DeviceExt::SupportsCubin64bit()
{
	return static_cast<D3D12DeviceExt *>(_orig)->SupportsCubin64bit();
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::CreateCubinComputeShaderExV2(void *params)
{
	return static_cast<D3D12DeviceExt *>(_orig)->CreateCubinComputeShaderExV2(params);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaMergedTextureSamplerObject(D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params)
{
	params->texDesc = _parent_device->convert_to_original_cpu_descriptor_handle(params->texDesc);
	params->smpDesc = _parent_device->convert_to_original_cpu_descriptor_handle(params->smpDesc);

	return static_cast<D3D12DeviceExt *>(_orig)->GetCudaMergedTextureSamplerObject(params);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::GetCudaIndependentDescriptorObject(D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params)
{
	params->desc = _parent_device->convert_to_original_cpu_descriptor_handle(params->desc);

	return static_cast<D3D12DeviceExt *>(_orig)->GetCudaIndependentDescriptorObject(params);
}

BOOL    STDMETHODCALLTYPE D3D12DeviceExt::SupportsAGSExtension(UINT agsExtension)
{
	return static_cast<D3D12DeviceExt *>(_orig)->SupportsAGSExtension(agsExtension);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::SetAGSUAVSlot(UINT uavSlot)
{
	return static_cast<D3D12DeviceExt *>(_orig)->SetAGSUAVSlot(uavSlot);
}

BOOL    STDMETHODCALLTYPE D3D12DeviceExt::IsNvShaderExtnOpCodeSupported(UINT32 op_code)
{
	return static_cast<D3D12DeviceExt *>(_orig)->IsNvShaderExtnOpCodeSupported(op_code);
}
HRESULT STDMETHODCALLTYPE D3D12DeviceExt::SetNvShaderExtnSlotSpace(UINT32 uav_slot, UINT32 uav_space, BOOL local_thread)
{
	return static_cast<D3D12DeviceExt *>(_orig)->SetNvShaderExtnSlotSpace(uav_slot, uav_space, local_thread);
}

BOOL    STDMETHODCALLTYPE D3D12DeviceExt::SetCreatePipelineStateFlagsNVAPI(UINT pipeline_state_flags)
{
	return static_cast<D3D12DeviceExt *>(_orig)->SetCreatePipelineStateFlagsNVAPI(pipeline_state_flags);
}

D3D12DXVKInteropDevice::D3D12DXVKInteropDevice(D3D12Device *device, IUnknown *original) :
	_orig(original),
	_parent_device(device)
{
	assert(_orig != nullptr && _parent_device != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	return _parent_device->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12DXVKInteropDevice::AddRef()
{
	return _parent_device->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12DXVKInteropDevice::Release()
{
	return _parent_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDXGIAdapter(REFIID iid, void **object)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetDXGIAdapter(iid, object);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetInstanceExtensions(UINT *extension_count, const char **extensions)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetInstanceExtensions(extension_count, extensions);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDeviceExtensions(UINT *extension_count, const char **extensions)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetDeviceExtensions(extension_count, extensions);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDeviceFeatures(const void **features)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetDeviceFeatures(features);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanHandles(vk_instance, vk_physical_device, vk_device);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanQueueInfo(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_family)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanQueueInfo(queue, vk_queue, vk_queue_family);
}
void    STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanImageLayout(ID3D12Resource *resource, D3D12_RESOURCE_STATES state, UINT *vk_layout)
{
	static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanImageLayout(resource, state, vk_layout);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanResourceInfo(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanResourceInfo(resource, vk_handle, buffer_offset);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::LockCommandQueue(ID3D12CommandQueue *queue)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->LockCommandQueue(queue);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::UnlockCommandQueue(ID3D12CommandQueue *queue)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->UnlockCommandQueue(queue);
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanResourceInfo1(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset, UINT *format)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanResourceInfo1(resource, vk_handle, buffer_offset, format);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::CreateInteropCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, UINT32 vk_queue_family_index, ID3D12CommandQueue **ppCommandQueue)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting ID3D12DXVKInteropDevice1::CreateInteropCommandQueue(this = %p, pDesc = %p, vk_queue_family_index = %u, ppCommandQueue = %p) ...",
		this, pDesc, vk_queue_family_index, ppCommandQueue);

	const HRESULT hr = static_cast<D3D12DXVKInteropDevice *>(_orig)->CreateInteropCommandQueue(pDesc, vk_queue_family_index, ppCommandQueue);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandQueue != nullptr);

		const auto command_queue_proxy = new D3D12CommandQueue(_parent_device, *ppCommandQueue);

#if RESHADE_VERBOSE_LOG
		reshade::log::message(reshade::log::level::debug, "Returning ID3D12CommandQueue object %p (%p).", command_queue_proxy, command_queue_proxy->_orig);
#endif
		*ppCommandQueue = command_queue_proxy;
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12DXVKInteropDevice1::CreateInteropCommandQueue failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::CreateInteropCommandAllocator(D3D12_COMMAND_LIST_TYPE type, UINT32 vk_queue_family_index, ID3D12CommandAllocator **ppAllocator)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->CreateInteropCommandAllocator(type, vk_queue_family_index, ppAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::BeginVkCommandBufferInterop(ID3D12CommandList *pCmdList, void *vk_command_buffer)
{
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pCmdList->QueryInterface(&command_list_proxy)))
		pCmdList = command_list_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->BeginVkCommandBufferInterop(pCmdList, vk_command_buffer);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::EndVkCommandBufferInterop(ID3D12CommandList *pCmdList)
{
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pCmdList->QueryInterface(&command_list_proxy)))
		pCmdList = command_list_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->EndVkCommandBufferInterop(pCmdList);
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::LockVulkanQueue(ID3D12CommandQueue *queue)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->LockVulkanQueue(queue);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::UnlockVulkanQueue(ID3D12CommandQueue *queue)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		queue = command_queue_proxy->_orig;

	return static_cast<D3D12DXVKInteropDevice *>(_orig)->UnlockVulkanQueue(queue);
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanHeapInfo(ID3D12Heap *heap, UINT64 *vk_memory, UINT64 *heap_offset, UINT32 *vk_memory_type)
{
	return static_cast<D3D12DXVKInteropDevice *>(_orig)->GetVulkanHeapInfo(heap, vk_memory, heap_offset, vk_memory_type);
}

#endif
