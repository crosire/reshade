#include "Log.hpp"
#include "HookManager.hpp"
#include "Hooks\D3D12.hpp"

#include <sstream>
#include <assert.h>

#define EXPORT extern "C"

// ---------------------------------------------------------------------------------------------------

namespace
{
	std::string GetErrorString(HRESULT hr)
	{
		std::stringstream res;

		switch (hr)
		{
			case E_FAIL:
				res << "E_FAIL";
				break;
			case E_NOTIMPL:
				res << "E_NOTIMPL";
				break;
			case E_INVALIDARG:
				res << "E_INVALIDARG";
				break;
			case DXGI_ERROR_UNSUPPORTED:
				res << "DXGI_ERROR_UNSUPPORTED";
				break;
			default:
				res << std::showbase << std::hex << hr;
				break;
		}

		return res.str();
	}
}

// ID3D12CommandQueue
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
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

	return this->mOrig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D12CommandQueue::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE D3D12CommandQueue::Release()
{
	ULONG ref = this->mOrig->Release();

	if (--this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'ID3D12CommandQueue' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'ID3D12CommandQueue' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return this->mOrig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return this->mOrig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return this->mOrig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetName(LPCWSTR Name)
{
	return this->mOrig->SetName(Name);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetDevice(REFIID riid, void **ppvDevice)
{
	if (ppvDevice == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	return this->mDevice->QueryInterface(riid, ppvDevice);
}
void STDMETHODCALLTYPE D3D12CommandQueue::UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags, const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
	this->mOrig->UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, pHeap, NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}
void STDMETHODCALLTYPE D3D12CommandQueue::CopyTileMappings(ID3D12Resource *pDstResource, const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate, ID3D12Resource *pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
	this->mOrig->CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags);
}
void STDMETHODCALLTYPE D3D12CommandQueue::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
	this->mOrig->ExecuteCommandLists(NumCommandLists, ppCommandLists);
}
void STDMETHODCALLTYPE D3D12CommandQueue::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
	this->mOrig->SetMarker(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12CommandQueue::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
	this->mOrig->BeginEvent(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12CommandQueue::EndEvent()
{
	this->mOrig->EndEvent();
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Signal(ID3D12Fence *pFence, UINT64 Value)
{
	return this->mOrig->Signal(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Wait(ID3D12Fence *pFence, UINT64 Value)
{
	return this->mOrig->Wait(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetTimestampFrequency(UINT64 *pFrequency)
{
	return this->mOrig->GetTimestampFrequency(pFrequency);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp)
{
	return this->mOrig->GetClockCalibration(pGpuTimestamp, pCpuTimestamp);
}
D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE D3D12CommandQueue::GetDesc()
{
	return this->mOrig->GetDesc();
}

// ID3D12Device
HRESULT STDMETHODCALLTYPE D3D12Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12Device))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mOrig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE D3D12Device::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE D3D12Device::Release()
{
	ULONG ref = this->mOrig->Release();

	if (--this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'ID3D12Device' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'ID3D12Device' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE D3D12Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return this->mOrig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return this->mOrig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return this->mOrig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetName(LPCWSTR Name)
{
	return this->mOrig->SetName(Name);
}
UINT STDMETHODCALLTYPE D3D12Device::GetNodeCount()
{
	return this->mOrig->GetNodeCount();
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid, void **ppCommandQueue)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "ID3D12Device::CreateCommandQueue" << "(" << this << ", " << pDesc << ", " << riidString << ", " << ppCommandQueue << ")' ...";

	if (ppCommandQueue == nullptr)
	{
		return E_INVALIDARG;
	}

	const HRESULT hr = this->mOrig->CreateCommandQueue(pDesc, riid, ppCommandQueue);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'ID3D12Device::CreateCommandQueue' failed with '" << GetErrorString(hr) << "'!";
	}
	else if (riid == __uuidof(ID3D12CommandQueue))
	{
		*ppCommandQueue = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator)
{
	return this->mOrig->CreateCommandAllocator(type, riid, ppCommandAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	return this->mOrig->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	return this->mOrig->CreateComputePipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **ppCommandList)
{
	return this->mOrig->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return this->mOrig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
	return this->mOrig->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
}
UINT STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
	return this->mOrig->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **ppvRootSignature)
{
	return this->mOrig->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature);
}
void STDMETHODCALLTYPE D3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateConstantBufferView(pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateShaderResourceView(pResource, pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateRenderTargetView(pResource, pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateDepthStencilView(pResource, pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	this->mOrig->CreateSampler(pDesc, DestDescriptor);
}
void STDMETHODCALLTYPE D3D12Device::CopyDescriptors(UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts, const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts, const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
	this->mOrig->CopyDescriptors(NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType);
}
void STDMETHODCALLTYPE D3D12Device::CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
	this->mOrig->CopyDescriptorsSimple(NumDescriptors, DestDescriptorRangeStart, SrcDescriptorRangeStart, DescriptorHeapsType);
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs)
{
	return this->mOrig->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}
D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE D3D12Device::GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType)
{
	return this->mOrig->GetCustomHeapProperties(nodeMask, heapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
	return this->mOrig->CreateCommittedResource(pHeapProperties, HeapFlags, pResourceDesc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
	return this->mOrig->CreateHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	return this->mOrig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	return this->mOrig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
	return this->mOrig->CreateSharedHandle(pObject, pAttributes, Access, Name, pHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
	return this->mOrig->OpenSharedHandle(NTHandle, riid, ppvObj);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
	return this->mOrig->OpenSharedHandleByName(Name, Access, pNTHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
	return this->mOrig->MakeResident(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
	return this->mOrig->Evict(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence)
{
	return this->mOrig->CreateFence(InitialValue, Flags, riid, ppFence);
}
HRESULT STDMETHODCALLTYPE D3D12Device::GetDeviceRemovedReason()
{
	return this->mOrig->GetDeviceRemovedReason();
}
void STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints(const D3D12_RESOURCE_DESC *pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts, UINT *pNumRows, UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes)
{
	this->mOrig->GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
	return this->mOrig->CreateQueryHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetStablePowerState(BOOL Enable)
{
	return this->mOrig->SetStablePowerState(Enable);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc, ID3D12RootSignature *pRootSignature, REFIID riid, void **ppvCommandSignature)
{
	return this->mOrig->CreateCommandSignature(pDesc, pRootSignature, riid, ppvCommandSignature);
}
void STDMETHODCALLTYPE D3D12Device::GetResourceTiling(ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D12_PACKED_MIP_INFO *pPackedMipDesc, D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
	this->mOrig->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips, pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}
LUID STDMETHODCALLTYPE D3D12Device::GetAdapterLuid()
{
	return this->mOrig->GetAdapterLuid();
}

// D3D12
EXPORT HRESULT WINAPI D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "D3D12CreateDevice" << "(" << pAdapter << ", " << std::showbase << std::hex << MinimumFeatureLevel << std::dec << std::noshowbase << ", " << riidString << ", " << ppDevice << ")' ...";

#ifdef _DEBUG
	/*ID3D12Debug *debug = nullptr;

	#pragma comment(lib, "d3d12.lib")

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
	{
		debug->EnableDebugLayer();

		debug->Release();
	}*/
#endif

	const HRESULT hr = ReShade::Hooks::Call(&D3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'D3D12CreateDevice' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	if (ppDevice != nullptr && riid == __uuidof(ID3D12Device))
	{
		assert(*ppDevice != nullptr);

		D3D12Device *const deviceProxy = new D3D12Device(static_cast<ID3D12Device *>(*ppDevice));

		*ppDevice = deviceProxy;

		LOG(TRACE) << "> Returned device objects: " << deviceProxy;
	}

	return hr;
}
