/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#if RESHADE_ADDON >= 2

#include "d3d12_device.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "dll_log.hpp"

D3D12DescriptorHeap::D3D12DescriptorHeap(ID3D12Device *device, ID3D12DescriptorHeap *original) :
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

	// Interface ID to query the original object from a proxy object
	constexpr GUID IID_UnwrappedObject = { 0x7f2c9a11, 0x3b4e, 0x4d6a, { 0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42 } }; // {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42}
	if (riid == IID_UnwrappedObject)
	{
		_orig->AddRef();
		*ppvObj = _orig;
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
	reshade::log::message(reshade::log::level::debug, "Destroying ID3D12DescriptorHeap object %p (%p).", this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for ID3D12DescriptorHeap object %p (%p) is inconsistent (%lu).", this, orig, ref_orig);
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
	return _orig->GetGPUDescriptorHandleForHeapStart();
}

#endif
