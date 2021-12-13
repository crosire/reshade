/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_device.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "dll_log.hpp"

D3D12DescriptorHeap::D3D12DescriptorHeap(D3D12Device *device, ID3D12DescriptorHeap *original) :
	_orig(original),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);
}

bool D3D12DescriptorHeap::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12DeviceChild) ||
		riid == __uuidof(ID3D12Pageable) ||
		riid == __uuidof(ID3D12DescriptorHeap))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12DescriptorHeap::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12DescriptorHeap::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
#if 0
	LOG(DEBUG) << "Destroying " << "ID3D12DescriptorHeap" << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D12DescriptorHeap" << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::GetDevice(REFIID riid, void **ppvDevice)
{
	return _device->QueryInterface(riid, ppvDevice);
}

D3D12_DESCRIPTOR_HEAP_DESC  STDMETHODCALLTYPE D3D12DescriptorHeap::GetDesc()
{
	return _orig->GetDesc();
}
D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE D3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart()
{
	assert(_internal_base_cpu_handle.ptr != 0);
	return _internal_base_cpu_handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE D3D12DescriptorHeap::GetGPUDescriptorHandleForHeapStart()
{
	return _internal_base_gpu_handle;
}

void D3D12DescriptorHeap::initialize_descriptor_base_handle(UINT heap_index)
{
	_orig_base_cpu_handle = _orig->GetCPUDescriptorHandleForHeapStart();
	_internal_base_cpu_handle = { 0 };

	assert(heap_index < (1 << (32 - 24)));
	static_assert(D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2 < (1 << 24));
	_internal_base_cpu_handle.ptr |= heap_index << 24;

	const D3D12_DESCRIPTOR_HEAP_DESC heap_desc = _orig->GetDesc();
	assert(heap_desc.Type <= 0x3);
	_internal_base_cpu_handle.ptr |= heap_desc.Type << 20;
	assert(heap_desc.Flags <= 0x1);
	_internal_base_cpu_handle.ptr |= heap_desc.Flags << 23;

	if (heap_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		_orig_base_gpu_handle = _orig->GetGPUDescriptorHandleForHeapStart();
		_internal_base_gpu_handle.ptr = _internal_base_cpu_handle.ptr;
	}
}

void reshade::d3d12::device_impl::get_descriptor_pool_offset(api::descriptor_set set, uint32_t binding, uint32_t array_offset, api::descriptor_pool *pool, uint32_t *offset) const
{
	assert(set.handle != 0 && array_offset == 0);

	const SIZE_T heap_index = (set.handle >> 24) & 0xFF;
	D3D12DescriptorHeap *const heap = static_cast<D3D12DescriptorHeap *>(_heaps[heap_index]);
	const D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>((set.handle >> 20) & 0x3);

	*pool = { reinterpret_cast<uintptr_t>(heap->_orig) };
	// Increment size is 1 (see 'D3D12Device::GetDescriptorHandleIncrementSize'), so can just subtract to get offset
	*offset = static_cast<uint32_t>(set.handle - heap->_internal_base_cpu_handle.ptr) + binding;
}

D3D12_CPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_cpu_descriptor_handle(api::descriptor_set set, D3D12_DESCRIPTOR_HEAP_TYPE &type) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = { static_cast<SIZE_T>(set.handle & 0xFFFFFFFF) };
	type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>((handle.ptr >> 20) & 0x3);

	return convert_to_original_cpu_descriptor_handle(handle, type);
}
D3D12_GPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_gpu_descriptor_handle(api::descriptor_set set) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = { static_cast<UINT64>(set.handle & 0xFFFFFFFF) };

	return convert_to_original_gpu_descriptor_handle(handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
	const SIZE_T heap_index = (handle.ptr >> 24) & 0xFF;
	assert(type == static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>((handle.ptr >> 20) & 0x3));

	return offset_descriptor_handle(static_cast<D3D12DescriptorHeap *>(_heaps[heap_index])->_orig_base_cpu_handle, handle.ptr & 0xFFFFF, type);
}
D3D12_GPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_gpu_descriptor_handle(D3D12_GPU_DESCRIPTOR_HANDLE handle) const
{
	const SIZE_T heap_index = (handle.ptr >> 24) & 0xFF;
	const D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>((handle.ptr >> 20) & 0x3);

	return offset_descriptor_handle(static_cast<D3D12DescriptorHeap *>(_heaps[heap_index])->_orig_base_gpu_handle, handle.ptr & 0xFFFFF, type);
}
