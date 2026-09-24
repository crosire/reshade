/*
 * Copyright (C) 2026 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_extensions.hpp"
#include "com_utils.hpp"
#include "hook_manager.hpp"

HRESULT ID3D12DeviceExt_GetCudaTextureObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE srv_handle, D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle, UINT32 *cuda_texture_handle)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		srv_handle = device_proxy->convert_to_original_cpu_descriptor_handle(srv_handle);
		sampler_handle = device_proxy->convert_to_original_cpu_descriptor_handle(sampler_handle);
	}

	return reshade::hooks::call(ID3D12DeviceExt_GetCudaTextureObject, reshade::hooks::vtable_from_instance(device_ext) + 7)(device_ext, srv_handle, sampler_handle, cuda_texture_handle);
}

HRESULT ID3D12DeviceExt_GetCudaSurfaceObject(IUnknown *device_ext, D3D12_CPU_DESCRIPTOR_HANDLE uav_handle, UINT32 *cuda_surface_handle)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		uav_handle = device_proxy->convert_to_original_cpu_descriptor_handle(uav_handle);
	}

	return reshade::hooks::call(ID3D12DeviceExt_GetCudaSurfaceObject, reshade::hooks::vtable_from_instance(device_ext) + 8)(device_ext, uav_handle, cuda_surface_handle);
}

HRESULT ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject(IUnknown *device_ext, D3D12_GET_CUDA_MERGED_TEXTURE_SAMPLER_OBJECT_PARAMS *params)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		params->texDesc = device_proxy->convert_to_original_cpu_descriptor_handle(params->texDesc);
		params->smpDesc = device_proxy->convert_to_original_cpu_descriptor_handle(params->smpDesc);
	}

	return reshade::hooks::call(ID3D12DeviceExt2_GetCudaMergedTextureSamplerObject, reshade::hooks::vtable_from_instance(device_ext) + 14)(device_ext, params);
}

HRESULT ID3D12DeviceExt2_GetCudaIndependentDescriptorObject(IUnknown *device_ext, D3D12_GET_CUDA_INDEPENDENT_DESCRIPTOR_OBJECT_PARAMS *params)
{
	com_ptr<ID3D12Device> device;
	device_ext->QueryInterface(IID_PPV_ARGS(&device));
	assert(device != nullptr);

	if (const auto device_proxy = get_private_pointer_d3dx<D3D12Device>(device.get()))
	{
		params->desc = device_proxy->convert_to_original_cpu_descriptor_handle(params->desc);
	}

	return reshade::hooks::call(ID3D12DeviceExt2_GetCudaIndependentDescriptorObject, reshade::hooks::vtable_from_instance(device_ext) + 15)(device_ext, params);
}

#endif

#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_extensions.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "com_ptr.hpp"
#include "com_utils.hpp"
#include <cstddef>

namespace
{
	template <typename T>
	T interop_vtable_entry(IUnknown *object, std::size_t index)
	{
		return reinterpret_cast<T>((*reinterpret_cast<void ***>(object))[index]);
	}
}

ID3D12CommandQueue *unwrap_command_queue(ID3D12CommandQueue *queue)
{
	if (queue == nullptr)
		return nullptr;

	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(queue->QueryInterface(&command_queue_proxy)))
		return command_queue_proxy->_orig;
	if (com_ptr<D3D12InteropCommandQueue> interop_queue;
		SUCCEEDED(queue->QueryInterface(&interop_queue)))
		return interop_queue->_orig;

	return queue;
}
ID3D12CommandList *unwrap_command_list(ID3D12CommandList *command_list)
{
	if (command_list == nullptr)
		return nullptr;

	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(command_list->QueryInterface(&command_list_proxy)))
		return command_list_proxy->_orig;

	return command_list;
}

D3D12InteropCommandQueue::D3D12InteropCommandQueue(D3D12Device *device, ID3D12CommandQueue *original) :
	_device(device),
	_orig(original)
{
	assert(_device != nullptr && _orig != nullptr);

	_device->AddRef();
}
D3D12InteropCommandQueue::~D3D12InteropCommandQueue()
{
	_orig->Release();
	_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12DeviceChild) ||
		riid == __uuidof(ID3D12Pageable) ||
		riid == __uuidof(ID3D12CommandQueue))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Interface ID to query the original object from a proxy object
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
		return S_OK;
	}

	// Refuse newer queue interfaces rather than hand out the original queue, which would bypass the command list unwrapping in 'ExecuteCommandLists'
	if (riid == __uuidof(ID3D12CommandQueue1))
	{
		*ppvObj = nullptr;
		return E_NOINTERFACE;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D12InteropCommandQueue::AddRef()
{
	return InterlockedIncrement(&_ref);
}
ULONG STDMETHODCALLTYPE D3D12InteropCommandQueue::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref == 0)
		delete this;
	return ref;
}

HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::GetDevice(REFIID riid, void **ppvDevice)
{
	// Return the device proxy, so that interop interfaces queried from it are proxied too
	return _device->QueryInterface(riid, ppvDevice);
}

void    STDMETHODCALLTYPE D3D12InteropCommandQueue::UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
	_orig->UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, pHeap, NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}
void    STDMETHODCALLTYPE D3D12InteropCommandQueue::CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
	_orig->CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags);
}
void    STDMETHODCALLTYPE D3D12InteropCommandQueue::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
	temp_mem<ID3D12CommandList *> command_lists(NumCommandLists);
	for (UINT i = 0; i < NumCommandLists; ++i)
		command_lists[i] = unwrap_command_list(ppCommandLists[i]);

	_orig->ExecuteCommandLists(NumCommandLists, command_lists.p);
}
void    STDMETHODCALLTYPE D3D12InteropCommandQueue::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
	_orig->SetMarker(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12InteropCommandQueue::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
	_orig->BeginEvent(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12InteropCommandQueue::EndEvent()
{
	_orig->EndEvent();
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
	return _orig->Signal(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
	return _orig->Wait(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::GetTimestampFrequency(UINT64 *pFrequency)
{
	return _orig->GetTimestampFrequency(pFrequency);
}
HRESULT STDMETHODCALLTYPE D3D12InteropCommandQueue::GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp)
{
	return _orig->GetClockCalibration(pGpuTimestamp, pCpuTimestamp);
}
D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE D3D12InteropCommandQueue::GetDesc()
{
	return _orig->GetDesc();
}

D3D12DXVKInteropDevice::D3D12DXVKInteropDevice(D3D12Device *device) :
	_parent_device(device)
{
	assert(_parent_device != nullptr);
}
D3D12DXVKInteropDevice::~D3D12DXVKInteropDevice()
{
	if (_orig != nullptr)
		_orig->Release();
}

HRESULT D3D12DXVKInteropDevice::check_and_upgrade_interface(REFIID riid)
{
	static constexpr IID iid_lookup[] = {
		IID_ID3D12DXVKInteropDevice,
		IID_ID3D12DXVKInteropDevice1,
		IID_ID3D12DXVKInteropDevice2,
		IID_ID3D12DXVKInteropDevice3,
	};

	for (unsigned short version = 0; version < sizeof(iid_lookup) / sizeof(iid_lookup[0]); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (_orig == nullptr || version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			const HRESULT hr = _parent_device->_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface));
			if (FAILED(hr))
				return hr;

			if (_orig != nullptr)
				_orig->Release();

			_orig = new_interface;
			_interface_version = version;
		}

		return S_OK;
	}

	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == IID_ID3D12DXVKInteropDevice ||
		riid == IID_ID3D12DXVKInteropDevice1 ||
		riid == IID_ID3D12DXVKInteropDevice2 ||
		riid == IID_ID3D12DXVKInteropDevice3)
	{
		const HRESULT hr = check_and_upgrade_interface(riid);
		if (FAILED(hr))
		{
			*ppvObj = nullptr;
			return hr;
		}

		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Preserve the identity of the parent D3D12 device proxy for IUnknown and all other interfaces.
	return _parent_device->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D12DXVKInteropDevice::AddRef()
{
	return _parent_device->AddRef();
}
ULONG STDMETHODCALLTYPE D3D12DXVKInteropDevice::Release()
{
	return _parent_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDXGIAdapter(REFIID iid, void **object)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, REFIID, void **);
	return interop_vtable_entry<function_type>(_orig, 3)(_orig, iid, object);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetInstanceExtensions(UINT *extension_count, const char **extensions)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, UINT *, const char **);
	return interop_vtable_entry<function_type>(_orig, 4)(_orig, extension_count, extensions);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDeviceExtensions(UINT *extension_count, const char **extensions)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, UINT *, const char **);
	return interop_vtable_entry<function_type>(_orig, 5)(_orig, extension_count, extensions);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetDeviceFeatures(const void **features)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, const void **);
	return interop_vtable_entry<function_type>(_orig, 6)(_orig, features);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanHandles(void *vk_instance, void *vk_physical_device, void *vk_device)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, void *, void *, void *);
	return interop_vtable_entry<function_type>(_orig, 7)(_orig, vk_instance, vk_physical_device, vk_device);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanQueueInfo(ID3D12CommandQueue *queue, void *vk_queue, UINT32 *vk_queue_family)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *, void *, UINT32 *);
	return interop_vtable_entry<function_type>(_orig, 8)(_orig, unwrap_command_queue(queue), vk_queue, vk_queue_family);
}
void    STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanImageLayout(ID3D12Resource *resource, D3D12_RESOURCE_STATES state, UINT32 *vk_layout)
{
	using function_type = void (STDMETHODCALLTYPE *)(IUnknown *, ID3D12Resource *, D3D12_RESOURCE_STATES, UINT32 *);
	interop_vtable_entry<function_type>(_orig, 9)(_orig, resource, state, vk_layout);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanResourceInfo(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12Resource *, UINT64 *, UINT64 *);
	return interop_vtable_entry<function_type>(_orig, 10)(_orig, resource, vk_handle, buffer_offset);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::LockCommandQueue(ID3D12CommandQueue *queue)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *);
	return interop_vtable_entry<function_type>(_orig, 11)(_orig, unwrap_command_queue(queue));
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::UnlockCommandQueue(ID3D12CommandQueue *queue)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *);
	return interop_vtable_entry<function_type>(_orig, 12)(_orig, unwrap_command_queue(queue));
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanResourceInfo1(ID3D12Resource *resource, UINT64 *vk_handle, UINT64 *buffer_offset, UINT32 *format)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12Resource *, UINT64 *, UINT64 *, UINT32 *);
	return interop_vtable_entry<function_type>(_orig, 13)(_orig, resource, vk_handle, buffer_offset, format);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::CreateInteropCommandQueue(const D3D12_COMMAND_QUEUE_DESC *desc, UINT32 vk_queue_family_index, ID3D12CommandQueue **queue)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, const D3D12_COMMAND_QUEUE_DESC *, UINT32, ID3D12CommandQueue **);
	const HRESULT hr = interop_vtable_entry<function_type>(_orig, 14)(_orig, desc, vk_queue_family_index, queue);
	if (SUCCEEDED(hr) && queue != nullptr && *queue != nullptr)
		*queue = new D3D12InteropCommandQueue(_parent_device, *queue);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::CreateInteropCommandAllocator(D3D12_COMMAND_LIST_TYPE type, UINT32 vk_queue_family_index, ID3D12CommandAllocator **allocator)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, D3D12_COMMAND_LIST_TYPE, UINT32, ID3D12CommandAllocator **);
	return interop_vtable_entry<function_type>(_orig, 15)(_orig, type, vk_queue_family_index, allocator);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::BeginVkCommandBufferInterop(ID3D12CommandList *command_list, void *vk_command_buffer)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandList *, void *);
	return interop_vtable_entry<function_type>(_orig, 16)(_orig, unwrap_command_list(command_list), vk_command_buffer);
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::EndVkCommandBufferInterop(ID3D12CommandList *command_list)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandList *);
	return interop_vtable_entry<function_type>(_orig, 17)(_orig, unwrap_command_list(command_list));
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::LockVulkanQueue(ID3D12CommandQueue *queue)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *);
	return interop_vtable_entry<function_type>(_orig, 18)(_orig, unwrap_command_queue(queue));
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::UnlockVulkanQueue(ID3D12CommandQueue *queue)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12CommandQueue *);
	return interop_vtable_entry<function_type>(_orig, 19)(_orig, unwrap_command_queue(queue));
}
HRESULT STDMETHODCALLTYPE D3D12DXVKInteropDevice::GetVulkanHeapInfo(ID3D12Heap *heap, UINT64 *vk_memory, UINT64 *heap_offset, UINT32 *vk_memory_type)
{
	using function_type = HRESULT (STDMETHODCALLTYPE *)(IUnknown *, ID3D12Heap *, UINT64 *, UINT64 *, UINT32 *);
	return interop_vtable_entry<function_type>(_orig, 20)(_orig, heap, vk_memory, heap_offset, vk_memory_type);
}
