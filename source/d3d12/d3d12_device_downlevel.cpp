/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "d3d12_device_downlevel.hpp"

D3D12DeviceDownlevel::D3D12DeviceDownlevel(D3D12Device *device, ID3D12DeviceDownlevel *original) :
	_orig(original),
	_parent_device(device)
{
	assert(_orig != nullptr && _parent_device != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12DeviceDownlevel::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	// IUnknown is handled by D3D12Device
	if (riid == __uuidof(this) ||
		riid == __uuidof(ID3D12DeviceDownlevel))
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

	return _parent_device->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12DeviceDownlevel::AddRef()
{
	return _parent_device->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12DeviceDownlevel::Release()
{
	return _parent_device->Release();
}

HRESULT STDMETHODCALLTYPE D3D12DeviceDownlevel::QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
	return _orig->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}
