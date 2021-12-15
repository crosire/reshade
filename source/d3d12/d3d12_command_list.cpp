/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp"
#include <malloc.h>

using reshade::d3d12::to_handle;

D3D12GraphicsCommandList::D3D12GraphicsCommandList(D3D12Device *device, ID3D12GraphicsCommandList *original) :
	command_list_impl(device, original),
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
			LOG(DEBUG) << "Upgrading ID3D12GraphicsCommandList" << _interface_version << " object " << this << " to ID3D12GraphicsCommandList" << version << '.';
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
	{
		_orig->Release();
		return ref;
	}

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
		// Only invoke event if there actually is an initial state to bind, otherwise expect things were already handled by the 'reset_command_list' event above
		if (pInitialState != nullptr)
			reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::all, to_handle(pInitialState));
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
	if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(this, to_handle(pSrcBuffer), SrcOffset, to_handle(pDstBuffer), DstOffset, NumBytes))
		return;
#endif
	_orig->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc, const D3D12_BOX *pSrcBox)
{
#if RESHADE_ADDON
	assert(pSrc != nullptr && pDst != nullptr);

	if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX && pDst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
	{
		bool use_dst_box = DstX != 0 || DstY != 0 || DstZ != 0;
		reshade::api::subresource_box dst_box;

		if (use_dst_box)
		{
			dst_box.left = static_cast<int32_t>(DstX);
			dst_box.top = static_cast<int32_t>(DstY);
			dst_box.front = static_cast<int32_t>(DstZ);

			if (pSrcBox != nullptr)
			{
				dst_box.right = dst_box.left + pSrcBox->right - pSrcBox->left;
				dst_box.bottom = dst_box.top + pSrcBox->bottom - pSrcBox->top;
				dst_box.back = dst_box.front + pSrcBox->back - pSrcBox->front;
			}
			else
			{
				use_dst_box = false;
				// TODO: Destination box size is not implemented (would have to get it from the resource)
				assert(DstX == 0 && DstY == 0 && DstZ == 0);
			}
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(
				this,
				to_handle(pSrc->pResource),
				pSrc->SubresourceIndex,
				reinterpret_cast<const reshade::api::subresource_box *>(pSrcBox),
				to_handle(pDst->pResource),
				pDst->SubresourceIndex,
				use_dst_box ? &dst_box : nullptr,
				reshade::api::filter_mode::min_mag_mip_point))
			return;
	}
	else if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT && pDst->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
	{
		reshade::api::subresource_box dst_box;
		dst_box.left = static_cast<int32_t>(DstX);
		dst_box.top = static_cast<int32_t>(DstY);
		dst_box.front = static_cast<int32_t>(DstZ);

		if (pSrcBox != nullptr)
		{
			dst_box.right = dst_box.left + pSrcBox->right - pSrcBox->left;
			dst_box.bottom = dst_box.top + pSrcBox->bottom - pSrcBox->top;
			dst_box.back = dst_box.front + pSrcBox->back - pSrcBox->front;
		}
		else
		{
			dst_box.right = dst_box.left + pSrc->PlacedFootprint.Footprint.Width;
			dst_box.bottom = dst_box.top + pSrc->PlacedFootprint.Footprint.Height;
			dst_box.back = dst_box.front + pSrc->PlacedFootprint.Footprint.Depth;
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_to_texture>(
				this,
				to_handle(pSrc->pResource),
				pSrc->PlacedFootprint.Offset,
				pSrc->PlacedFootprint.Footprint.Width,
				pSrc->PlacedFootprint.Footprint.Height,
				to_handle(pDst->pResource),
				pDst->SubresourceIndex,
				&dst_box))
			return;
	}
	else if (pSrc->Type == D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX && pDst->Type == D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
	{
		assert(DstX == 0 && DstY == 0 && DstZ == 0);

		if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_to_buffer>(
				this,
				to_handle(pSrc->pResource),
				pSrc->SubresourceIndex, reinterpret_cast<const reshade::api::subresource_box *>(pSrcBox),
				to_handle(pDst->pResource),
				pDst->PlacedFootprint.Offset,
				pDst->PlacedFootprint.Footprint.Width,
				pDst->PlacedFootprint.Footprint.Height))
			return;
	}
#endif

	_orig->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyResource(ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, to_handle(pSrcResource), to_handle(pDstResource)))
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
	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this, to_handle(pSrcResource), SrcSubresource, nullptr, to_handle(pDstResource), DstSubresource, 0, 0, reshade::d3d12::convert_format(Format)))
		return;
#endif
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	_orig->IASetPrimitiveTopology(PrimitiveTopology);

#if RESHADE_ADDON
	const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
	const uint32_t value = static_cast<uint32_t>(reshade::d3d12::convert_primitive_topology(PrimitiveTopology));
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &value);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, NumViewports, reinterpret_cast<const reshade::api::viewport *>(pViewports));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects));
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
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::all, to_handle(pPipelineState));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers)
{
	_orig->ResourceBarrier(NumBarriers, pBarriers);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::barrier>())
		return;

	temp_mem<reshade::api::resource> resources(NumBarriers);
	temp_mem<reshade::api::resource_usage> old_state(NumBarriers), new_state(NumBarriers);
	for (UINT i = 0; i < NumBarriers; ++i)
	{
		switch (pBarriers[i].Type)
		{
		case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
			resources[i] = to_handle(pBarriers[i].Transition.pResource);
			old_state[i] = static_cast<reshade::api::resource_usage>(pBarriers[i].Transition.StateBefore);
			new_state[i] = static_cast<reshade::api::resource_usage>(pBarriers[i].Transition.StateAfter);
			break;
		case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
			resources[i] = to_handle(pBarriers[i].Aliasing.pResourceAfter != nullptr ? pBarriers[i].Aliasing.pResourceAfter : pBarriers[i].Aliasing.pResourceBefore);
			old_state[i] = reshade::api::resource_usage::general;
			new_state[i] = reshade::api::resource_usage::general;
			break;
		case D3D12_RESOURCE_BARRIER_TYPE_UAV:
			resources[i] = to_handle(pBarriers[i].UAV.pResource);
			old_state[i] = reshade::api::resource_usage::unordered_access;
			new_state[i] = reshade::api::resource_usage::unordered_access;
			break;
		}
	}

	reshade::invoke_addon_event<reshade::addon_event::barrier>(this, NumBarriers, resources.p, old_state.p, new_state.p);
#endif
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
	assert(NumDescriptorHeaps <= 2);

#if RESHADE_ADDON
	temp_mem<ID3D12DescriptorHeap *, 2> heaps(NumDescriptorHeaps);
	for (UINT i = 0; i < NumDescriptorHeaps; ++i)
		heaps[i] = static_cast<D3D12DescriptorHeap *>(ppDescriptorHeaps[i])->_orig;

	_orig->SetDescriptorHeaps(NumDescriptorHeaps, heaps.p);

	for (UINT i = 0; i < 2; ++i)
		_current_descriptor_heaps[i] = i < NumDescriptorHeaps ? heaps[i] : nullptr;
#else
	_orig->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);

	for (UINT i = 0; i < 2; ++i)
		_current_descriptor_heaps[i] = i < NumDescriptorHeaps ? ppDescriptorHeaps[i] : nullptr;
#endif
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
#if RESHADE_ADDON
	_orig->SetComputeRootDescriptorTable(RootParameterIndex, _device->convert_to_original_gpu_descriptor_handle(BaseDescriptor));

	const reshade::api::descriptor_set set = { BaseDescriptor.ptr };
	reshade::invoke_addon_event<reshade::addon_event::bind_descriptor_sets>(
		this,
		reshade::api::shader_stage::all_compute,
		to_handle(_current_root_signature[1]),
		RootParameterIndex,
		1, &set);
#else
	_orig->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
#if RESHADE_ADDON
	_orig->SetGraphicsRootDescriptorTable(RootParameterIndex, _device->convert_to_original_gpu_descriptor_handle(BaseDescriptor));

	const reshade::api::descriptor_set set = { BaseDescriptor.ptr };
	reshade::invoke_addon_event<reshade::addon_event::bind_descriptor_sets>(
		this,
		reshade::api::shader_stage::all_graphics,
		to_handle(_current_root_signature[0]),
		RootParameterIndex,
		1, &set);
#else
	_orig->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_compute,
		to_handle(_current_root_signature[1]),
		RootParameterIndex,
		DestOffsetIn32BitValues,
		1, &SrcData);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_graphics,
		to_handle(_current_root_signature[0]),
		RootParameterIndex,
		DestOffsetIn32BitValues,
		1, &SrcData);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_compute,
		to_handle(_current_root_signature[1]),
		RootParameterIndex,
		DestOffsetIn32BitValues,
		Num32BitValuesToSet, static_cast<const uint32_t *>(pSrcData));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void *pSrcData, UINT DestOffsetIn32BitValues)
{
	_orig->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::push_constants>(
		this,
		reshade::api::shader_stage::all_graphics,
		to_handle(_current_root_signature[0]),
		RootParameterIndex,
		DestOffsetIn32BitValues,
		Num32BitValuesToSet, static_cast<const uint32_t *>(pSrcData));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

	reshade::api::buffer_range buffer_range;
	if (!_device_impl->resolve_gpu_address(BufferLocation, &buffer_range.buffer, &buffer_range.offset))
		return;
	buffer_range.size = UINT64_MAX;

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		reshade::api::shader_stage::all_compute,
		to_handle(_current_root_signature[1]),
		RootParameterIndex,
		reshade::api::descriptor_set_update { {}, 0, 0, 1, reshade::api::descriptor_type::constant_buffer, &buffer_range });
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	_orig->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);

#if RESHADE_ADDON
	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

	reshade::api::buffer_range buffer_range;
	if (!_device_impl->resolve_gpu_address(BufferLocation, &buffer_range.buffer, &buffer_range.offset))
		return;
	buffer_range.size = UINT64_MAX;

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(
		this,
		reshade::api::shader_stage::all_graphics,
		to_handle(_current_root_signature[0]),
		RootParameterIndex,
		reshade::api::descriptor_set_update { {}, 0, 0, 1, reshade::api::descriptor_type::constant_buffer, &buffer_range });
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
	if (!reshade::has_addon_event<reshade::addon_event::bind_index_buffer>())
		return;

	reshade::api::resource buffer = { 0 };
	uint64_t offset = 0;
	uint32_t index_size = 0;

	if (pView != nullptr)
	{
		if (!_device_impl->resolve_gpu_address(pView->BufferLocation, &buffer, &offset))
			return;
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

	if (!reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
		return;

	temp_mem<reshade::api::resource, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> buffers(NumViews);
	temp_mem<uint64_t, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> offsets(NumViews);
	temp_mem<uint32_t, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> strides(NumViews);
	for (UINT i = 0; i < NumViews; ++i)
	{
		if (!_device_impl->resolve_gpu_address(pViews[i].BufferLocation, &buffers[i], &offsets[i]))
			return;
		strides[i] = pViews[i].StrideInBytes;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, StartSlot, NumViews, buffers.p, offsets.p, strides.p);
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
	assert(NumRenderTargetDescriptors <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
		return;

	temp_mem<reshade::api::resource_view, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs(NumRenderTargetDescriptors);
	for (UINT i = 0; i < NumRenderTargetDescriptors; ++i)
		rtvs[i] = to_handle(RTsSingleHandleToDescriptorRange ? _device->offset_descriptor_handle(*pRenderTargetDescriptors, i, D3D12_DESCRIPTOR_HEAP_TYPE_RTV) : pRenderTargetDescriptors[i]);

	reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, NumRenderTargetDescriptors, rtvs.p, to_handle(pDepthStencilDescriptor != nullptr ? *pDepthStencilDescriptor : D3D12_CPU_DESCRIPTOR_HANDLE {}));
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, to_handle(DepthStencilView), ClearFlags & D3D12_CLEAR_FLAG_DEPTH ? &Depth : nullptr, ClearFlags & D3D12_CLEAR_FLAG_STENCIL ? &Stencil : nullptr, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects)))
		return;
#endif
	_orig->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, to_handle(RenderTargetView), ColorRGBA, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects)))
		return;
#endif
	_orig->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_unordered_access_view_uint>(this, to_handle(ViewCPUHandle), Values, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects)))
		return;

	_orig->ClearUnorderedAccessViewUint(
		_device_impl->convert_to_original_gpu_descriptor_handle(ViewGPUHandleInCurrentHeap),
		_device_impl->convert_to_original_cpu_descriptor_handle(ViewCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
		pResource, Values, NumRects, pRects);
#else
	_orig->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
#endif
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_unordered_access_view_float>(this, to_handle(ViewCPUHandle), Values, NumRects, reinterpret_cast<const reshade::api::rect *>(pRects)))
		return;

	_orig->ClearUnorderedAccessViewFloat(
		_device_impl->convert_to_original_gpu_descriptor_handle(ViewGPUHandleInCurrentHeap),
		_device_impl->convert_to_original_cpu_descriptor_handle(ViewCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
		pResource, Values, NumRects, pRects);
#else
	_orig->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
#endif
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
	if (reshade::invoke_addon_event<reshade::addon_event::draw_or_dispatch_indirect>(this, reshade::api::indirect_command::unknown, to_handle(pArgumentBuffer), ArgumentBufferOffset, MaxCommandCount, 0))
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
	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this, to_handle(pSrcResource), SrcSubresource, reinterpret_cast<const reshade::api::rect *>(pSrcRect), to_handle(pDstResource), DstSubresource, DstX, DstY, reshade::d3d12::convert_format(Format)))
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
	assert(NumRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

	temp_mem<reshade::api::render_pass_render_target_desc, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rts(NumRenderTargets);
	for (UINT i = 0; i < NumRenderTargets; ++i)
	{
		rts[i].view = to_handle(pRenderTargets[i].cpuDescriptor);
		rts[i].load_op = reshade::d3d12::convert_render_pass_load_op(pRenderTargets[i].BeginningAccess.Type);
		rts[i].store_op = reshade::d3d12::convert_render_pass_store_op(pRenderTargets[i].EndingAccess.Type);
		std::copy_n(pRenderTargets[i].BeginningAccess.Clear.ClearValue.Color, 4, rts[i].clear_color);
	}

	reshade::api::render_pass_depth_stencil_desc ds;
	if (pDepthStencil != nullptr)
	{
		ds.view = to_handle(pDepthStencil->cpuDescriptor);
		ds.depth_load_op = reshade::d3d12::convert_render_pass_load_op(pDepthStencil->DepthBeginningAccess.Type);
		ds.depth_store_op = reshade::d3d12::convert_render_pass_store_op(pDepthStencil->DepthEndingAccess.Type);
		ds.stencil_load_op = reshade::d3d12::convert_render_pass_load_op(pDepthStencil->StencilBeginningAccess.Type);
		ds.stencil_store_op = reshade::d3d12::convert_render_pass_store_op(pDepthStencil->StencilEndingAccess.Type);
		ds.clear_depth = pDepthStencil->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth;
		ds.clear_stencil = pDepthStencil->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil;
	}

	reshade::invoke_addon_event<reshade::addon_event::begin_render_pass>(this, NumRenderTargets, rts.p, pDepthStencil != nullptr ? &ds : nullptr);
#endif

	assert(_interface_version >= 4);
	static_cast<ID3D12GraphicsCommandList4 *>(_orig)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndRenderPass(void)
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::end_render_pass>(this);
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
