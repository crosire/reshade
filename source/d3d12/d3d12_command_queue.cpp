/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"

D3D12CommandQueue::D3D12CommandQueue(D3D12Device *device, ID3D12CommandQueue *original) :
	_orig(original),
	_interface_version(0),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);
}

bool D3D12CommandQueue::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12DeviceChild) ||
		riid == __uuidof(ID3D12Pageable))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D12CommandQueue),
	};

	for (unsigned int version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded ID3D12CommandQueue" << _interface_version << " object " << this << " to ID3D12CommandQueue" << version << '.';
#endif
			_orig->Release();
			_orig = static_cast<ID3D12CommandQueue *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Special case for d3d12on7
	if (riid == __uuidof(ID3D12CommandQueueDownlevel))
	{
		if (ID3D12CommandQueueDownlevel *downlevel = nullptr; // Not a 'com_ptr' since D3D12CommandQueueDownlevel will take ownership
			_downlevel == nullptr && SUCCEEDED(_orig->QueryInterface(&downlevel)))
			_downlevel = new D3D12CommandQueueDownlevel(this, downlevel);
		if (_downlevel != nullptr)
			return _downlevel->QueryInterface(riid, ppvObj);
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueue::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueue::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	if (_downlevel != nullptr)
		_downlevel->Release();

	const ULONG ref_orig = _orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for ID3D12CommandQueue" << _interface_version << " object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D12CommandQueue" << _interface_version << " object " << this << ".";
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetDevice(REFIID riid, void **ppvDevice)
{
	return _device->QueryInterface(riid, ppvDevice);
}

void    STDMETHODCALLTYPE D3D12CommandQueue::UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
	_orig->UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, pHeap, NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
	_orig->CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
	// The synchronization definition of 'ExecuteCommandLists' is equivalent to an aliasing barrier, see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource#notes-on-the-aliasing-barrier
	// TODO (need a command list for this): _device->_state.on_aliasing(D3D12_RESOURCE_ALIASING_BARRIER { nullptr, nullptr });

	std::vector<ID3D12CommandList *> command_lists(NumCommandLists);
	for (UINT i = 0; i < NumCommandLists; i++)
	{
		assert(ppCommandLists[i] != nullptr);

		if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
			SUCCEEDED(ppCommandLists[i]->QueryInterface(&command_list_proxy)))
		{
			// Get original command list pointer from proxy object
			command_lists[i] = command_list_proxy->_orig;

			// Merge command list trackers into device one
			_device->_state.merge(command_list_proxy->_state);
		}
		else
		{
			// This can be a compute command list too, which have no proxy object
			command_lists[i] = ppCommandLists[i];
		}
	}

	_orig->ExecuteCommandLists(NumCommandLists, command_lists.data());
}
void    STDMETHODCALLTYPE D3D12CommandQueue::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
	_orig->SetMarker(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
	_orig->BeginEvent(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::EndEvent()
{
	_orig->EndEvent();
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
	return _orig->Signal(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
	return _orig->Wait(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetTimestampFrequency(UINT64 *pFrequency)
{
	return _orig->GetTimestampFrequency(pFrequency);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp)
{
	return _orig->GetClockCalibration(pGpuTimestamp, pCpuTimestamp);
}
D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE D3D12CommandQueue::GetDesc()
{
	return _orig->GetDesc();
}
