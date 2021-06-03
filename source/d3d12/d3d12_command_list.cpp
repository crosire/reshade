/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "reshade_api_type_convert.hpp"

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
#if 0
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
#if RESHADE_ADDON
	if (_has_open_render_pass)
	{
		reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(this);
		_has_open_render_pass = false;
	}
#endif

	return _orig->Close();
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Reset(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState)
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::reset_command_list>(this);
#endif

	_current_root_signature[0] = nullptr;
	_current_root_signature[1] = nullptr;
	_current_descriptor_heaps[0] = nullptr;
	_current_descriptor_heaps[1] = nullptr;

	const HRESULT hr = _orig->Reset(pAllocator, pInitialState);
#if RESHADE_ADDON
	if (SUCCEEDED(hr))
	{
		// TODO: Figure out if this is compute or graphics pipeline state object
		reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::all_graphics, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pInitialState) });
	}
#endif
	return hr;
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearState(ID3D12PipelineState *pPipelineState)
{
	_orig->ClearState(pPipelineState);
	// TODO: Call events with cleared state
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::dispatch>(this, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ))
		return;
#endif
	_orig->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource *pDstBuffer, UINT64 DstOffset, ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(this,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcBuffer) }, SrcOffset,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pDstBuffer) }, DstOffset, NumBytes))
		return;
#endif
	_orig->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox)
{
#if RESHADE_ADDON
	int32_t dst_box[6] = { static_cast<int32_t>(DstX), static_cast<int32_t>(DstY), static_cast<int32_t>(DstZ) };

	if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX && pDst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
	{
		if (pSrcBox != nullptr)
		{
			dst_box[3] = dst_box[0] + pSrcBox->right - pSrcBox->left;
			dst_box[4] = dst_box[1] + pSrcBox->bottom - pSrcBox->top;
			dst_box[5] = dst_box[2] + pSrcBox->back - pSrcBox->front;
		}
		else
		{
			// TODO: Destination box size is not implemented (would have to get it from the resource)
			assert(DstX == 0 && DstY == 0 && DstZ == 0);
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pSrc->pResource) }, pSrc->SubresourceIndex, reinterpret_cast<const int32_t *>(pSrcBox),
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDst->pResource) }, pDst->SubresourceIndex, DstX != 0 || DstY != 0 || DstZ != 0 ? dst_box : nullptr, reshade::api::filter_type::min_mag_mip_point))
			return;
	}
	else if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT && pDst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
	{
		if (pSrcBox != nullptr)
		{
			dst_box[3] = dst_box[0] + pSrcBox->right - pSrcBox->left;
			dst_box[4] = dst_box[1] + pSrcBox->bottom - pSrcBox->top;
			dst_box[5] = dst_box[2] + pSrcBox->back - pSrcBox->front;
		}
		else
		{
			dst_box[3] = dst_box[0] + pSrc->PlacedFootprint.Footprint.Width;
			dst_box[4] = dst_box[1] + pSrc->PlacedFootprint.Footprint.Height;
			dst_box[5] = dst_box[2] + pSrc->PlacedFootprint.Footprint.Depth;
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_to_texture>(this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pSrc->pResource) }, pSrc->PlacedFootprint.Offset, pSrc->PlacedFootprint.Footprint.Width, pSrc->PlacedFootprint.Footprint.Height,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDst->pResource) }, pDst->SubresourceIndex, dst_box))
			return;
	}
	else if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX && pDst->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
	{
		static_assert(sizeof(D3D12_BOX) == (sizeof(int32_t) * 6));

		assert(DstX == 0 && DstY == 0 && DstZ == 0); // TODO

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_to_buffer>(this,
			reshade::api::resource { reinterpret_cast<uintptr_t>(pSrc->pResource) }, pSrc->SubresourceIndex, reinterpret_cast<const int32_t *>(pSrcBox),
			reshade::api::resource { reinterpret_cast<uintptr_t>(pDst->pResource) }, pDst->PlacedFootprint.Offset, pDst->PlacedFootprint.Footprint.Width, pDst->PlacedFootprint.Footprint.Height))
			return;
	}
#endif
	_orig->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }))
		return;
#endif
	_orig->CopyResource(pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTiles(ID3D12Resource *pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
	_orig->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource *pDstResource, UINT DstSubresource, ID3D12Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, nullptr,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, nullptr, reshade::d3d12::convert_format(Format)))
		return;
#endif
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	_orig->IASetPrimitiveTopology(PrimitiveTopology);

#if RESHADE_ADDON
	const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, reinterpret_cast<const uint32_t *>(&PrimitiveTopology));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

#if RESHADE_ADDON
	static_assert(sizeof(D3D12_VIEWPORT) == (sizeof(float) * 6));

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, NumViewports, reinterpret_cast<const float *>(pViewports));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	static_assert(sizeof(D3D12_RECT) == (sizeof(int32_t) * 4));

	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, NumRects, reinterpret_cast<const int32_t *>(pRects));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
	_orig->OMSetBlendFactor(BlendFactor);

#if RESHADE_ADDON
	const reshade::api::dynamic_state state = reshade::api::dynamic_state::blend_constant;
	const uint32_t value = (BlendFactor == nullptr) ? 0xFFFFFFFF : // Default blend factor is { 1, 1, 1, 1 }
		((static_cast<uint32_t>(BlendFactor[0] * 255.f) & 0xFF)) |
		((static_cast<uint32_t>(BlendFactor[1] * 255.f) & 0xFF) << 8) |
		((static_cast<uint32_t>(BlendFactor[2] * 255.f) & 0xFF) << 16) |
		((static_cast<uint32_t>(BlendFactor[3] * 255.f) & 0xFF) << 24);
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
	_orig->OMSetStencilRef(StencilRef);

#if RESHADE_ADDON
	const reshade::api::dynamic_state state = reshade::api::dynamic_state::stencil_reference_value;
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &StencilRef);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState *pPipelineState)
{
	_orig->SetPipelineState(pPipelineState);

#if RESHADE_ADDON
	// TODO: Figure out if this is compute or graphics pipeline state object
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::all_graphics, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pPipelineState) });
#endif
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

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::execute_secondary_command_list>(this, command_list_proxy);
#endif

	_orig->ExecuteBundle(command_list_proxy->_orig);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap *const *ppDescriptorHeaps)
{
	_orig->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);

	assert(NumDescriptorHeaps <= 2);

	for (UINT i = 0; i < 2; ++i)
		_current_descriptor_heaps[i] = i < NumDescriptorHeaps ? ppDescriptorHeaps[i] : nullptr;
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature *pRootSignature)
{
	_orig->SetComputeRootSignature(pRootSignature);

	_current_root_signature[1] = pRootSignature;
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature)
{
	_orig->SetGraphicsRootSignature(pRootSignature);

	_current_root_signature[0] = pRootSignature;
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	_orig->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);

#if RESHADE_ADDON
	static_assert(sizeof(BaseDescriptor) == sizeof(reshade::api::descriptor_set));

	reshade::invoke_addon_event<reshade::addon_event::bind_descriptor_sets>(
		this,
		reshade::api::shader_stage::all_compute,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[1]) },
		RootParameterIndex,
		1,
		reinterpret_cast<const reshade::api::descriptor_set *>(&BaseDescriptor.ptr));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	_orig->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);

#if RESHADE_ADDON
	static_assert(sizeof(BaseDescriptor) == sizeof(reshade::api::descriptor_set));

	reshade::invoke_addon_event<reshade::addon_event::bind_descriptor_sets>(
		this,
		reshade::api::shader_stage::all_graphics,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[0]) },
		RootParameterIndex,
		1,
		reinterpret_cast<const reshade::api::descriptor_set *>(&BaseDescriptor.ptr));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_compute,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[1]) },
		RootParameterIndex,
		DestOffsetIn32BitValues,
		1,
		&SrcData);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_graphics,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[0]) },
		RootParameterIndex,
		DestOffsetIn32BitValues,
		1,
		&SrcData);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_compute,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[1]) },
		RootParameterIndex,
		DestOffsetIn32BitValues,
		Num32BitValuesToSet,
		static_cast<const uint32_t *>(pSrcData));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_graphics,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[0]) },
		RootParameterIndex,
		DestOffsetIn32BitValues,
		Num32BitValuesToSet,
		static_cast<const uint32_t *>(pSrcData));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

#if RESHADE_ADDON
	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

	reshade::api::resource buffer = { 0 };
	uint64_t offset = 0;
	_device_impl->resolve_gpu_address(BufferLocation, &buffer, &offset);

	// TODO: Constant buffer offset
	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		reshade::api::shader_stage::all_compute,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[1]) },
		RootParameterIndex,
		reshade::api::descriptor_type::constant_buffer,
		0,
		1,
		&buffer);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

#if RESHADE_ADDON
	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::push_descriptors)].empty())
		return;

	reshade::api::resource buffer = { 0 };
	uint64_t offset = 0;
	_device_impl->resolve_gpu_address(BufferLocation, &buffer, &offset);

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		reshade::api::shader_stage::all_graphics,
		reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(_current_root_signature[0]) },
		RootParameterIndex,
		reshade::api::descriptor_type::constant_buffer,
		0,
		1,
		&buffer);
#endif
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
	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::bind_index_buffer)].empty())
		return;

	reshade::api::resource buffer = { 0 };
	uint64_t offset = 0;
	uint32_t index_size = 0;
	if (pView != nullptr)
	{
		_device_impl->resolve_gpu_address(pView->BufferLocation, &buffer, &offset);
		index_size = pView->Format == DXGI_FORMAT_R16_UINT ? 2 : 4;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, buffer, offset, index_size);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews)
{
	_orig->IASetVertexBuffers(StartSlot, NumViews, pViews);

#if RESHADE_ADDON
	assert(NumViews <= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

	if (reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::bind_vertex_buffers)].empty())
		return;

	reshade::api::resource buffers[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint64_t offsets[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint32_t strides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
	{
		_device_impl->resolve_gpu_address(pViews[i].BufferLocation, &buffers[i], &offsets[i]);
		strides[i] = pViews[i].StrideInBytes;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StartSlot, NumViews, buffers, offsets, strides);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews)
{
	_orig->SOSetTargets(StartSlot, NumViews, pViews);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, D3D12_CPU_DESCRIPTOR_HANDLE const *pDepthStencilDescriptor)
{
#if RESHADE_ADDON
	if (_has_open_render_pass)
	{
		reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(this);
		_has_open_render_pass = false;
	}
#endif

	_orig->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

#if RESHADE_ADDON
	assert(NumRenderTargetDescriptors <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	_current_pass->count = NumRenderTargetDescriptors;
	std::memcpy(_current_pass->rtv, pRenderTargetDescriptors, (RTsSingleHandleToDescriptorRange ? 1 : NumRenderTargetDescriptors) * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));
	_current_pass->dsv = pDepthStencilDescriptor != nullptr ? *pDepthStencilDescriptor : D3D12_CPU_DESCRIPTOR_HANDLE { 0 };
	_current_pass->rtv_is_single_handle_to_range = RTsSingleHandleToDescriptorRange;

	if (NumRenderTargetDescriptors != 0 || pDepthStencilDescriptor != nullptr)
	{
		_has_open_render_pass = true;
		reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(this, reshade::api::render_pass { reinterpret_cast<uintptr_t>(_current_pass) });
	}
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	static_assert(
		(UINT)reshade::api::attachment_type::depth == (D3D12_CLEAR_FLAG_DEPTH << 1) &&
		(UINT)reshade::api::attachment_type::stencil == (D3D12_CLEAR_FLAG_STENCIL << 1));

	if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, reshade::api::resource_view { DepthStencilView.ptr }, static_cast<reshade::api::attachment_type>(ClearFlags << 1), Depth, Stencil, NumRects, reinterpret_cast<const int32_t *>(pRects)))
		return;
#endif
	_orig->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, reshade::api::resource_view { RenderTargetView.ptr }, ColorRGBA, NumRects, reinterpret_cast<const int32_t *>(pRects)))
		return;
#endif
	_orig->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_unordered_access_view_uint>(this, reshade::api::resource_view { ViewCPUHandle.ptr }, Values, NumRects, reinterpret_cast<const int32_t *>(pRects)))
		return;
#endif
	_orig->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_unordered_access_view_float>(this, reshade::api::resource_view { ViewCPUHandle.ptr }, Values, NumRects, reinterpret_cast<const int32_t *>(pRects)))
		return;
#endif
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
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(this, 0, reshade::api::resource { reinterpret_cast<uintptr_t>(pArgumentBuffer) }, ArgumentBufferOffset, MaxCommandCount, 0))
		return;
#endif
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
#if RESHADE_ADDON
	int32_t src_box[6];
	const int32_t dst_offset[3] = { static_cast<int32_t>(DstX), static_cast<int32_t>(DstY), 0 };
	if (pSrcRect != nullptr)
	{
		src_box[0] = pSrcRect->left;
		src_box[1] = pSrcRect->top;
		src_box[2] = 0;
		src_box[3] = pSrcRect->right;
		src_box[4] = pSrcRect->bottom;
		src_box[5] = 1;
	}

	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, pSrcRect != nullptr ? src_box : nullptr,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, DstX != 0 || DstY != 0 ? dst_offset : nullptr,
		reshade::d3d12::convert_format(Format)))
		return;
#endif

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
#if RESHADE_ADDON
	if (_has_open_render_pass)
	{
		reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(this);
	}

	// Use clear events with explicit resource view references here, since this is invoked before render pass begin
	if (!reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::clear_depth_stencil_view)].empty() ||
		!reshade::addon::event_list[static_cast<uint32_t>(reshade::addon_event::clear_render_target_view)].empty())
	{
		for (UINT i = 0; i < NumRenderTargets; ++i)
		{
			if (pRenderTargets[i].BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
			{
				// Cannot be skipped
				reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, reshade::api::resource_view { pRenderTargets[i].cpuDescriptor.ptr }, pRenderTargets[i].BeginningAccess.Clear.ClearValue.Color, 0, nullptr);
			}
		}

		if (pDepthStencil != nullptr)
		{
			D3D12_DEPTH_STENCIL_VALUE clear_value = {};
			reshade::api::attachment_type clear_flags = {};

			if (pDepthStencil->DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
			{
				clear_flags |= reshade::api::attachment_type::depth;
				clear_value.Depth = pDepthStencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth;
			}
			if (pDepthStencil->StencilBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
			{
				clear_flags |= reshade::api::attachment_type::stencil;
				clear_value.Stencil = pDepthStencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil;
			}

			if (pDepthStencil->DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR ||
				pDepthStencil->StencilBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
			{
				// Cannot be skipped
				reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, reshade::api::resource_view { pDepthStencil->cpuDescriptor.ptr }, clear_flags, clear_value.Depth, clear_value.Stencil, 0, nullptr);
			}
		}
	}
#endif

	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);

#if RESHADE_ADDON
	assert(NumRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	_current_pass->count = NumRenderTargets;
	for (UINT i = 0; i < NumRenderTargets; ++i)
		_current_pass->rtv[i] = { pRenderTargets->cpuDescriptor.ptr + i * _device->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] };
	_current_pass->dsv = (pDepthStencil != nullptr) ? pDepthStencil->cpuDescriptor : D3D12_CPU_DESCRIPTOR_HANDLE { 0 };
	_current_pass->rtv_is_single_handle_to_range = FALSE;

	_has_open_render_pass = true;
	reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(this, reshade::api::render_pass { reinterpret_cast<uintptr_t>(_current_pass) });
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndRenderPass(void)
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(this);
	_has_open_render_pass = false;
#endif

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
