/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"

D3D12GraphicsCommandList::D3D12GraphicsCommandList(D3D12Device *device, ID3D12GraphicsCommandList *original) :
	command_list_impl(device, original),
	_interface_version(0),
	_device(device)
{
	assert(_orig != nullptr && _device != nullptr);
}

bool D3D12GraphicsCommandList::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12Object) ||
		riid == __uuidof(ID3D12DeviceChild) ||
		riid == __uuidof(ID3D12CommandList))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D12GraphicsCommandList),
		__uuidof(ID3D12GraphicsCommandList1),
		__uuidof(ID3D12GraphicsCommandList2),
		__uuidof(ID3D12GraphicsCommandList3),
		__uuidof(ID3D12GraphicsCommandList4),
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
			LOG(DEBUG) << "Upgraded ID3D12GraphicsCommandList" << _interface_version << " object " << this << " to ID3D12GraphicsCommandList" << version << '.';
#endif
			_orig->Release();
			_orig = static_cast<ID3D12GraphicsCommandList *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::QueryInterface(REFIID riid, void **ppvObj)
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
ULONG   STDMETHODCALLTYPE D3D12GraphicsCommandList::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12GraphicsCommandList::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if 0
	LOG(DEBUG) << "Destroying " << "ID3D12GraphicsCommandList" << interface_version << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D12GraphicsCommandList" << interface_version << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::GetDevice(REFIID riid, void **ppvDevice)
{
	return _device->QueryInterface(riid, ppvDevice);
}

D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE D3D12GraphicsCommandList::GetType()
{
	return _orig->GetType();
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Close()
{
	return _orig->Close();
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState)
{
	RESHADE_ADDON_EVENT(reset_command_list, this);
	return _orig->Reset(pAllocator, pInitialState);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
	_orig->ClearState(pPipelineState);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	RESHADE_ADDON_EVENT(draw, this, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	_orig->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	RESHADE_ADDON_EVENT(draw_indexed, this, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	_orig->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	RESHADE_ADDON_EVENT(dispatch, this, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	_orig->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
	_orig->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox)
{
	_orig->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource)
{
	_orig->CopyResource(pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTiles(ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
	_orig->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	_orig->IASetPrimitiveTopology(PrimitiveTopology);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

	static_assert(sizeof(D3D12_VIEWPORT) == (sizeof(float) * 6));
	RESHADE_ADDON_EVENT(set_viewports, this, 0, NumViewports, reinterpret_cast<const float *>(pViewports));
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	assert(NumRects < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	int32_t rect_data[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE * 4];
	for (UINT i = 0, k = 0; i < NumRects; ++i, k += 4)
	{
		rect_data[k + 0] = static_cast<int32_t>(pRects[i].left);
		rect_data[k + 1] = static_cast<int32_t>(pRects[i].top);
		rect_data[k + 2] = static_cast<int32_t>(pRects[i].right - pRects[i].left);
		rect_data[k + 3] = static_cast<int32_t>(pRects[i].bottom - pRects[i].top);
	}
	RESHADE_ADDON_EVENT(set_scissor_rects, this, 0, NumRects, rect_data);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
	_orig->OMSetBlendFactor(BlendFactor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
	_orig->OMSetStencilRef(StencilRef);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
	_orig->SetPipelineState(pPipelineState);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers)
{
	_orig->ResourceBarrier(NumBarriers, pBarriers);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList *pCommandList)
{
	assert(pCommandList != nullptr);

	// Get original command list pointer from proxy object
	const auto command_list_proxy = static_cast<D3D12GraphicsCommandList *>(pCommandList);

	RESHADE_ADDON_EVENT(execute_secondary_command_list, this, command_list_proxy);

	_orig->ExecuteBundle(command_list_proxy->_orig);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
	_orig->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
	_orig->SetComputeRootSignature(pRootSignature);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
	_orig->SetGraphicsRootSignature(pRootSignature);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	_orig->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	_orig->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView)
{
	_orig->IASetIndexBuffer(pView);

#if RESHADE_ADDON
	reshade::api::resource_handle buffer = { 0 };
	uint64_t offset = 0;
	if (pView != nullptr)
	{
		ID3D12Resource *resource = nullptr;
		_device_impl->resolve_gpu_address(pView->BufferLocation, &resource, &offset);
		buffer = { reinterpret_cast<uintptr_t>(resource) };
	}
	RESHADE_ADDON_EVENT(set_index_buffer, this, buffer, offset);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
	_orig->IASetVertexBuffers(StartSlot, NumViews, pViews);

#if RESHADE_ADDON
	assert(NumViews < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
	reshade::api::resource_handle buffers[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint64_t offsets[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
	{
		ID3D12Resource *resource = nullptr;
		if (pViews != nullptr)
			_device_impl->resolve_gpu_address(pViews[i].BufferLocation, &resource, &offsets[i]);
		buffers[i] = { reinterpret_cast<uintptr_t>(resource) };
	}
	RESHADE_ADDON_EVENT(set_vertex_buffers, this, StartSlot, NumViews, buffers, offsets);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
	_orig->SOSetTargets(StartSlot, NumViews, pViews);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, D3D12_CPU_DESCRIPTOR_HANDLE const *pDepthStencilDescriptor)
{
	_orig->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

#if RESHADE_ADDON
	assert(NumRenderTargetDescriptors < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
	reshade::api::resource_view_handle rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < NumRenderTargetDescriptors; ++i)
		rtvs[i] = { RTsSingleHandleToDescriptorRange ? pRenderTargetDescriptors->ptr + i * _device->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] : pRenderTargetDescriptors[i].ptr };
	const reshade::api::resource_view_handle dsv = { pDepthStencilDescriptor != nullptr ? pDepthStencilDescriptor->ptr : 0 };
	RESHADE_ADDON_EVENT(set_render_targets_and_depth_stencil, this, NumRenderTargetDescriptors, rtvs, dsv);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
	RESHADE_ADDON_EVENT(clear_depth_stencil, this, reshade::api::resource_view_handle { DepthStencilView.ptr }, static_cast<uint32_t>(ClearFlags), Depth, Stencil);
	_orig->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects)
{
	RESHADE_ADDON_EVENT(clear_render_target, this, reshade::api::resource_view_handle { RenderTargetView.ptr }, ColorRGBA);
	_orig->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
	_orig->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
	_orig->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DiscardResource(ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion)
{
	_orig->DiscardResource(pResource, pRegion);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index)
{
	_orig->BeginQuery(pQueryHeap, Type, Index);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index)
{
	_orig->EndQuery(pQueryHeap, Type, Index);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource *pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
	_orig->ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation)
{
	_orig->SetPredication(pBuffer, AlignedBufferOffset, Operation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetMarker(UINT Metadata, const void *pData, UINT Size)
{
	_orig->SetMarker(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void *pData, UINT Size)
{
	_orig->BeginEvent(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndEvent()
{
	_orig->EndEvent();
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteIndirect(ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset)
{
	RESHADE_ADDON_EVENT(draw_or_dispatch_indirect, this, reshade::addon_event::draw_or_dispatch_indirect, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pArgumentBuffer) }, ArgumentBufferOffset, MaxCommandCount, 0);
	_orig->ExecuteIndirect(pCommandSignature, MaxCommandCount, pArgumentBuffer, ArgumentBufferOffset, pCountBuffer, CountBufferOffset);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::AtomicCopyBufferUINT(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->AtomicCopyBufferUINT(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, ppDependentResources, pDependentSubresourceRanges);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::AtomicCopyBufferUINT64(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource *const *ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->AtomicCopyBufferUINT64(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, ppDependentResources, pDependentSubresourceRanges);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetDepthBounds(FLOAT Min, FLOAT Max)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->OMSetDepthBounds(Min, Max);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetSamplePositions(UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION *pSamplePositions)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveSubresourceRegion(ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource, D3D12_RECT *pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->ResolveSubresourceRegion(pDstResource, DstSubresource, DstX, DstY, pSrcResource, SrcSubresource, pSrcRect, Format, ResolveMode);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetViewInstanceMask(UINT Mask)
{
	assert(_interface_version >= 1);
	static_cast<ID3D12GraphicsCommandList1 *>(_orig)->SetViewInstanceMask(Mask);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::WriteBufferImmediate(UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes)
{
	assert(_interface_version >= 2);
	static_cast<ID3D12GraphicsCommandList2 *>(_orig)->WriteBufferImmediate(Count, pParams, pModes);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetProtectedResourceSession(ID3D12ProtectedResourceSession *pProtectedResourceSession)
{
	assert(_interface_version >= 3);
	static_cast<ID3D12GraphicsCommandList3 *>(_orig)->SetProtectedResourceSession(pProtectedResourceSession);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginRenderPass(UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

#if RESHADE_ADDON
	assert(NumRenderTargets < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
	reshade::api::resource_view_handle rtvs[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < NumRenderTargets; ++i)
		rtvs[i] = { pRenderTargets->cpuDescriptor.ptr + i * _device->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] };
	const reshade::api::resource_view_handle dsv = { pDepthStencil != nullptr ? pDepthStencil->cpuDescriptor.ptr : 0 };
	RESHADE_ADDON_EVENT(set_render_targets_and_depth_stencil, this, NumRenderTargets, rtvs, dsv);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndRenderPass(void)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->EndRenderPass();
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::InitializeMetaCommand(ID3D12MetaCommand *pMetaCommand, const void *pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->InitializeMetaCommand(pMetaCommand, pInitializationParametersData, InitializationParametersDataSizeInBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteMetaCommand(ID3D12MetaCommand *pMetaCommand, const void *pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->ExecuteMetaCommand(pMetaCommand, pExecutionParametersData, ExecutionParametersDataSizeInBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BuildRaytracingAccelerationStructure(pDesc, NumPostbuildInfoDescs, pPostbuildInfoDescs);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EmitRaytracingAccelerationStructurePostbuildInfo(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->EmitRaytracingAccelerationStructurePostbuildInfo(pDesc, NumSourceAccelerationStructures, pSourceAccelerationStructureData);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyRaytracingAccelerationStructure(D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->CopyRaytracingAccelerationStructure(DestAccelerationStructureData, SourceAccelerationStructureData, Mode);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState1(ID3D12StateObject *pStateObject)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->SetPipelineState1(pStateObject);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DispatchRays(const D3D12_DISPATCH_RAYS_DESC *pDesc)
{
	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->DispatchRays(pDesc);
}
