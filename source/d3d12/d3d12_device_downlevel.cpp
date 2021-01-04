/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device_downlevel.hpp"

D3D12DeviceDownlevel::D3D12DeviceDownlevel(D3D12Device *, ID3D12DeviceDownlevel *original) :
	_orig(original)
{
	assert(_orig != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12DeviceDownlevel::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12DeviceDownlevel))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12DeviceDownlevel::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12DeviceDownlevel::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	const ULONG ref_orig = _orig->Release();
	if (ref_orig > 1) // Verify internal reference count against one instead of zero because parent device still holds a reference
		LOG(WARN) << "Reference count for ID3D12DeviceDownlevel object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D12DeviceDownlevel object " << this << ".";
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12DeviceDownlevel::QueryVideoMemoryInfo(UINT NodeIndex, DXGI_MEMORY_SEGMENT_GROUP MemorySegmentGroup, DXGI_QUERY_VIDEO_MEMORY_INFO *pVideoMemoryInfo)
{
	return _orig->QueryVideoMemoryInfo(NodeIndex, MemorySegmentGroup, pVideoMemoryInfo);
}
