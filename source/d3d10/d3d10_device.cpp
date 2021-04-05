/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d10_device.hpp"
#include "dxgi/dxgi_device.hpp"
#include "render_d3d10_utils.hpp"

D3D10Device::D3D10Device(IDXGIDevice1 *dxgi_device, ID3D10Device1 *original) :
	device_impl(original),
	_dxgi_device(new DXGIDevice(dxgi_device, this))
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D10Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D10Device), sizeof(device_proxy), &device_proxy);
}

bool D3D10Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		// IUnknown is handled by DXGIDevice
		riid == __uuidof(ID3D10Device) ||
		riid == __uuidof(ID3D10Device1))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE D3D10Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Note: Objects must have an identity, so use DXGIDevice for IID_IUnknown
	// See https://docs.microsoft.com/windows/desktop/com/rules-for-implementing-queryinterface
	if (_dxgi_device->check_and_upgrade_interface(riid))
	{
		_dxgi_device->AddRef();
		*ppvObj = _dxgi_device;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D10Device::AddRef()
{
	_orig->AddRef();

	// Add references to other objects that are coupled with the device
	_dxgi_device->AddRef();

	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D10Device::Release()
{
	// Release references to other objects that are coupled with the device
	_dxgi_device->Release();

	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	const auto orig = _orig;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "ID3D10Device1" << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D10Device1" << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

void    STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShader(ID3D10PixelShader *pPixelShader)
{
	_orig->PSSetShader(pPixelShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShader(ID3D10VertexShader *pVertexShader)
{
	_orig->VSSetShader(pVertexShader);
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(
		[this](reshade::api::command_list *, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
			assert(instances == 1 && first_instance == 0);
			_orig->DrawIndexed(indices, first_index, vertex_offset);
		}, this, IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}
void    STDMETHODCALLTYPE D3D10Device::Draw(UINT VertexCount, UINT StartVertexLocation)
{
	reshade::invoke_addon_event<reshade::addon_event::draw>(
		[this](reshade::api::command_list *, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) {
			assert(instances == 1 && first_instance == 0);
			_orig->Draw(vertices, first_vertex);
		}, this, VertexCount, 1, StartVertexLocation, 0);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::IASetInputLayout(ID3D10InputLayout *pInputLayout)
{
	_orig->IASetInputLayout(pInputLayout);
}
void    STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	assert(NumBuffers <= D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
	reshade::api::resource_handle buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint64_t offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < NumBuffers; ++i)
	{
		buffers[i] = { reinterpret_cast<uintptr_t>(ppVertexBuffers[i]) };
		offsets[i] = pOffsets[i];
	}

	reshade::invoke_addon_event<reshade::addon_event::set_vertex_buffers>(
		[this, pStrides](reshade::api::command_list *, uint32_t first, uint32_t count, const reshade::api::resource_handle *buffers, const uint32_t *strides, const uint64_t *offsets) {
			ID3D10Buffer *buffer_ptrs[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT uint_offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			for (UINT i = 0; i < count; ++i)
			{
				buffer_ptrs[i] = reinterpret_cast<ID3D10Buffer *>(buffers[i].handle);
				uint_offsets[i] = static_cast<UINT>(offsets[i]);
			}
			_orig->IASetVertexBuffers(first, count, buffer_ptrs, strides, uint_offsets);
		}, this, StartSlot, NumBuffers, buffers, pStrides, offsets);
}
void    STDMETHODCALLTYPE D3D10Device::IASetIndexBuffer(ID3D10Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	reshade::invoke_addon_event<reshade::addon_event::set_index_buffer>(
		[this, Format](reshade::api::command_list *, reshade::api::resource_handle buffer, uint32_t format, uint64_t offset) {
			_orig->IASetIndexBuffer(reinterpret_cast<ID3D10Buffer *>(buffer.handle), static_cast<DXGI_FORMAT>(format), static_cast<UINT>(offset));
		}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pIndexBuffer) }, static_cast<uint32_t>(Format), Offset);
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(
		[this](reshade::api::command_list *, uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
			_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
		}, this, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	reshade::invoke_addon_event<reshade::addon_event::draw>(
		[this](reshade::api::command_list *, uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) {
			_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
		}, this, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShader(ID3D10GeometryShader *pShader)
{
	_orig->GSSetShader(pShader);
}
void    STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	_orig->IASetPrimitiveTopology(Topology);
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::SetPredication(ID3D10Predicate *pPredicate, BOOL PredicateValue)
{
	_orig->SetPredication(pPredicate, PredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::OMSetRenderTargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
#ifdef WIN64
	static_assert(sizeof(*ppRenderTargetViews) == sizeof(reshade::api::resource_view_handle));
	const auto rtvs = reinterpret_cast<const reshade::api::resource_view_handle *>(ppRenderTargetViews);
#else
	assert(NumViews <= D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT);
	reshade::api::resource_view_handle rtvs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		rtvs[i] = { reinterpret_cast<uintptr_t>(ppRenderTargetViews[i]) };
#endif
	const reshade::api::resource_view_handle dsv = { reinterpret_cast<uintptr_t>(pDepthStencilView) };

	reshade::invoke_addon_event<reshade::addon_event::set_render_targets_and_depth_stencil>(
		[this](reshade::api::command_list *, uint32_t count, const reshade::api::resource_view_handle *rtvs, reshade::api::resource_view_handle dsv) {
#ifdef WIN64
			_orig->OMSetRenderTargets(count, reinterpret_cast<ID3D10RenderTargetView *const *>(rtvs), reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle));
#else
			ID3D10RenderTargetView *rtv_ptrs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			for (UINT i = 0; i < count; ++i)
				rtv_ptrs[i] = reinterpret_cast<ID3D10RenderTargetView *>(rtvs[i].handle);
			_orig->OMSetRenderTargets(count, rtv_ptrs, reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle));
#endif
		}, this, NumViews, rtvs, dsv);
}
void    STDMETHODCALLTYPE D3D10Device::OMSetBlendState(ID3D10BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	_orig->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}
void    STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(ID3D10DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	_orig->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}
void    STDMETHODCALLTYPE D3D10Device::SOSetTargets(UINT NumBuffers, ID3D10Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	_orig->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::DrawAuto()
{
	_orig->DrawAuto();
}
void    STDMETHODCALLTYPE D3D10Device::RSSetState(ID3D10RasterizerState *pRasterizerState)
{
	_orig->RSSetState(pRasterizerState);
}
void    STDMETHODCALLTYPE D3D10Device::RSSetViewports(UINT NumViewports, const D3D10_VIEWPORT *pViewports)
{
	assert(NumViewports <= D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	float viewport_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE * 6];
	for (UINT i = 0, k = 0; i < NumViewports; ++i, k += 6)
	{
		viewport_data[k + 0] = static_cast<float>(pViewports[i].TopLeftX);
		viewport_data[k + 1] = static_cast<float>(pViewports[i].TopLeftY);
		viewport_data[k + 2] = static_cast<float>(pViewports[i].Width);
		viewport_data[k + 3] = static_cast<float>(pViewports[i].Height);
		viewport_data[k + 4] = pViewports[i].MinDepth;
		viewport_data[k + 5] = pViewports[i].MaxDepth;
	}

	reshade::invoke_addon_event<reshade::addon_event::set_viewports>(
		[this](reshade::api::command_list *, uint32_t first, uint32_t count, const float *viewports) {
			assert(first == 0);
			D3D10_VIEWPORT viewport_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			for (UINT i = 0, k = 0; i < count; ++i, k += 6)
			{
				viewport_data[i].TopLeftX = static_cast<INT>(viewports[k + 0]);
				viewport_data[i].TopLeftY = static_cast<INT>(viewports[k + 1]);
				viewport_data[i].Width   = static_cast<UINT>(viewports[k + 2]);
				viewport_data[i].Height  = static_cast<UINT>(viewports[k + 3]);
				viewport_data[i].MinDepth = viewports[k + 4];
				viewport_data[i].MaxDepth = viewports[k + 5];
			}
			_orig->RSSetViewports(count, viewport_data);
		}, this, 0, NumViewports, viewport_data);
}
void    STDMETHODCALLTYPE D3D10Device::RSSetScissorRects(UINT NumRects, const D3D10_RECT *pRects)
{
	assert(NumRects <= D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	int32_t rect_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE * 4];
	for (UINT i = 0, k = 0; i < NumRects; ++i, k += 4)
	{
		rect_data[k + 0] = static_cast<int32_t>(pRects[i].left);
		rect_data[k + 1] = static_cast<int32_t>(pRects[i].top);
		rect_data[k + 2] = static_cast<int32_t>(pRects[i].right - pRects[i].left);
		rect_data[k + 3] = static_cast<int32_t>(pRects[i].bottom - pRects[i].top);
	}

	reshade::invoke_addon_event<reshade::addon_event::set_scissor_rects>(
		[this](reshade::api::command_list *, uint32_t first, uint32_t count, const int32_t *rects) {
			assert(first == 0);
			D3D10_RECT rect_data[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			for (UINT i = 0, k = 0; i < count; ++i, k += 4)
			{
				rect_data[i].left = rects[k + 0];
				rect_data[i].top  = rects[k + 1];
				rect_data[i].right = rect_data[i].left + rects[k + 2];
				rect_data[i].bottom = rect_data[i].top + rects[k + 3];
			}
			_orig->RSSetScissorRects(count, rect_data);
		}, this, 0, NumRects, rect_data);
}
void    STDMETHODCALLTYPE D3D10Device::CopySubresourceRegion(ID3D10Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D10Resource *pSrcResource, UINT SrcSubresource, const D3D10_BOX *pSrcBox)
{
	_orig->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
void    STDMETHODCALLTYPE D3D10Device::CopyResource(ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
	_orig->CopyResource(pDstResource, pSrcResource);
}
void    STDMETHODCALLTYPE D3D10Device::UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
	_orig->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void    STDMETHODCALLTYPE D3D10Device::ClearRenderTargetView(ID3D10RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
	reshade::invoke_addon_event<reshade::addon_event::clear_render_target>(
		[this](reshade::api::command_list *, reshade::api::resource_view_handle rtv, const float color[4]) {
			_orig->ClearRenderTargetView(reinterpret_cast<ID3D10RenderTargetView *>(rtv.handle), color);
		}, this, reshade::api::resource_view_handle { reinterpret_cast<uintptr_t>(pRenderTargetView) }, ColorRGBA);
}
void    STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil>(
		[this](reshade::api::command_list *, reshade::api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) {
			_orig->ClearDepthStencilView(reinterpret_cast<ID3D10DepthStencilView *>(dsv.handle), clear_flags, depth, stencil);
		}, this, reshade::api::resource_view_handle { reinterpret_cast<uintptr_t>(pDepthStencilView) }, ClearFlags, Depth, Stencil);
}
void    STDMETHODCALLTYPE D3D10Device::GenerateMips(ID3D10ShaderResourceView *pShaderResourceView)
{
	_orig->GenerateMips(pShaderResourceView);
}
void    STDMETHODCALLTYPE D3D10Device::ResolveSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, ID3D10Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShader(ID3D10PixelShader **ppPixelShader)
{
	_orig->PSGetShader(ppPixelShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShader(ID3D10VertexShader **ppVertexShader)
{
	_orig->VSGetShader(ppVertexShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetInputLayout(ID3D10InputLayout **ppInputLayout)
{
	_orig->IAGetInputLayout(ppInputLayout);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
{
	_orig->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetIndexBuffer(ID3D10Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset)
{
	_orig->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShader(ID3D10GeometryShader **ppGeometryShader)
{
	_orig->GSGetShader(ppGeometryShader);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
	_orig->IAGetPrimitiveTopology(pTopology);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::GetPredication(ID3D10Predicate **ppPredicate, BOOL *pPredicateValue)
{
	_orig->GetPredication(ppPredicate, pPredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetRenderTargets(UINT NumViews, ID3D10RenderTargetView **ppRenderTargetViews, ID3D10DepthStencilView **ppDepthStencilView)
{
	_orig->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetBlendState(ID3D10BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask)
{
	_orig->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetDepthStencilState(ID3D10DepthStencilState **ppDepthStencilState, UINT *pStencilRef)
{
	_orig->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
void    STDMETHODCALLTYPE D3D10Device::SOGetTargets(UINT NumBuffers, ID3D10Buffer **ppSOTargets, UINT *pOffsets)
{
	_orig->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetState(ID3D10RasterizerState **ppRasterizerState)
{
	_orig->RSGetState(ppRasterizerState);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetViewports(UINT *NumViewports, D3D10_VIEWPORT *pViewports)
{
	_orig->RSGetViewports(NumViewports, pViewports);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetScissorRects(UINT *NumRects, D3D10_RECT *pRects)
{
	_orig->RSGetScissorRects(NumRects, pRects);
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetDeviceRemovedReason()
{
	return _orig->GetDeviceRemovedReason();
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetExceptionMode(UINT RaiseFlags)
{
	return _orig->SetExceptionMode(RaiseFlags);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetExceptionMode()
{
	return _orig->GetExceptionMode();
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
void    STDMETHODCALLTYPE D3D10Device::ClearState()
{
	_orig->ClearState();
}
void    STDMETHODCALLTYPE D3D10Device::Flush()
{
	_orig->Flush();
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBuffer(const D3D10_BUFFER_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Buffer **ppBuffer)
{
	assert(pDesc != nullptr && ppBuffer != nullptr);
	D3D10_BUFFER_DESC new_desc = *pDesc;

	HRESULT hr = S_OK;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, pInitialData](reshade::api::device *, reshade::api::resource_type type, const reshade::api::resource_desc &desc, reshade::api::memory_usage mem_usage, reshade::api::resource_handle *out) {
			assert(type == reshade::api::resource_type::buffer);
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			ID3D10Buffer *buffer = nullptr;
			hr = _orig->CreateBuffer(&new_desc, pInitialData, &buffer);
			if (SUCCEEDED(hr))
			{
				_resources.register_object(buffer);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateBuffer" << " failed with error code " << hr << '.';
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | ByteWidth                               | " << std::setw(39) << new_desc.ByteWidth << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(buffer) };
		}, this, reshade::api::resource_type::buffer, reshade::d3d10::convert_resource_desc(new_desc), reshade::d3d10::convert_memory_usage(new_desc.Usage), &out);
	*ppBuffer = reinterpret_cast<ID3D10Buffer *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(const D3D10_TEXTURE1D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture1D **ppTexture1D)
{
	assert(pDesc != nullptr && ppTexture1D != nullptr);
	D3D10_TEXTURE1D_DESC new_desc = *pDesc;

	HRESULT hr = S_OK;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, pInitialData](reshade::api::device *, reshade::api::resource_type type, const reshade::api::resource_desc &desc, reshade::api::memory_usage mem_usage, reshade::api::resource_handle *out) {
			assert(type == reshade::api::resource_type::texture_1d);
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			ID3D10Texture1D *texture = nullptr;
			hr = _orig->CreateTexture1D(&new_desc, pInitialData, &texture);
			if (SUCCEEDED(hr))
			{
				_resources.register_object(texture);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture1D" << " failed with error code " << hr << '.';
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(texture) };
		}, this, reshade::api::resource_type::texture_1d, reshade::d3d10::convert_resource_desc(new_desc), reshade::d3d10::convert_memory_usage(new_desc.Usage), &out);
	*ppTexture1D = reinterpret_cast<ID3D10Texture1D *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(const D3D10_TEXTURE2D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture2D **ppTexture2D)
{
	assert(pDesc != nullptr && ppTexture2D != nullptr);
	D3D10_TEXTURE2D_DESC new_desc = *pDesc;

	HRESULT hr = S_OK;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, pInitialData](reshade::api::device *, reshade::api::resource_type type, const reshade::api::resource_desc &desc, reshade::api::memory_usage mem_usage, reshade::api::resource_handle *out) {
			assert(type == reshade::api::resource_type::texture_2d);
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			ID3D10Texture2D *texture = nullptr;
			hr = _orig->CreateTexture2D(&new_desc, pInitialData, &texture);
			if (SUCCEEDED(hr))
			{
				_resources.register_object(texture);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture2D" << " failed with error code " << hr << '.';
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | ArraySize                               | " << std::setw(39) << new_desc.ArraySize << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(texture) };
		}, this, reshade::api::resource_type::texture_2d, reshade::d3d10::convert_resource_desc(new_desc), reshade::d3d10::convert_memory_usage(new_desc.Usage), &out);
	*ppTexture2D = reinterpret_cast<ID3D10Texture2D *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(const D3D10_TEXTURE3D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture3D **ppTexture3D)
{
	assert(pDesc != nullptr && ppTexture3D != nullptr);
	D3D10_TEXTURE3D_DESC new_desc = *pDesc;

	HRESULT hr = S_OK;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, pInitialData](reshade::api::device *, reshade::api::resource_type type, const reshade::api::resource_desc &desc, reshade::api::memory_usage mem_usage, reshade::api::resource_handle *out) {
			assert(type == reshade::api::resource_type::texture_3d);
			reshade::d3d10::convert_resource_desc(desc, new_desc);

			ID3D10Texture3D *texture = nullptr;
			hr = _orig->CreateTexture3D(&new_desc, pInitialData, &texture);
			if (SUCCEEDED(hr))
			{
				_resources.register_object(texture);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateTexture3D" << " failed with error code " << hr << '.';
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | Depth                                   | " << std::setw(39) << new_desc.Depth << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | Usage                                   | " << std::setw(39) << new_desc.Usage << " |";
				LOG(DEBUG) << "  | BindFlags                               | " << std::setw(39) << std::hex << new_desc.BindFlags << std::dec << " |";
				LOG(DEBUG) << "  | CPUAccessFlags                          | " << std::setw(39) << std::hex << new_desc.CPUAccessFlags << std::dec << " |";
				LOG(DEBUG) << "  | MiscFlags                               | " << std::setw(39) << std::hex << new_desc.MiscFlags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(texture) };
		}, this, reshade::api::resource_type::texture_3d, reshade::d3d10::convert_resource_desc(new_desc), reshade::d3d10::convert_memory_usage(new_desc.Usage), &out);
	*ppTexture3D = reinterpret_cast<ID3D10Texture3D *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D10ShaderResourceView **ppSRView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_SHADER_RESOURCE_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_SRV_DIMENSION_UNKNOWN };

	HRESULT hr = S_OK;
	reshade::api::resource_view_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
			assert(usage_type == reshade::api::resource_usage::shader_resource);
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			ID3D10ShaderResourceView *srv = nullptr;
			hr = _orig->CreateShaderResourceView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_SRV_DIMENSION_UNKNOWN ? &new_desc : nullptr, &srv);
			if (SUCCEEDED(hr))
			{
				_views.register_object(srv);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateShaderResourceView" << " failed with error code " << hr << '.';
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(srv) };
		}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, reshade::d3d10::convert_resource_view_desc(new_desc), &out);
	*ppSRView = reinterpret_cast<ID3D10ShaderResourceView *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc, ID3D10RenderTargetView **ppRTView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_RENDER_TARGET_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_RTV_DIMENSION_UNKNOWN };

	HRESULT hr = S_OK;
	reshade::api::resource_view_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
			assert(usage_type == reshade::api::resource_usage::render_target);
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			ID3D10RenderTargetView *rtv = nullptr;
			hr = _orig->CreateRenderTargetView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_RTV_DIMENSION_UNKNOWN ? &new_desc : nullptr, &rtv);
			if (SUCCEEDED(hr))
			{
				_views.register_object(rtv);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateRenderTargetView" << " failed with error code " << hr << '.';
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(rtv) };
		}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, reshade::d3d10::convert_resource_view_desc(new_desc), &out);
	*ppRTView = reinterpret_cast<ID3D10RenderTargetView *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_DEPTH_STENCIL_VIEW_DESC new_desc =
		pDesc != nullptr ? *pDesc : D3D10_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_DSV_DIMENSION_UNKNOWN };

	HRESULT hr = S_OK;
	reshade::api::resource_view_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
			assert(usage_type == reshade::api::resource_usage::depth_stencil);
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			ID3D10DepthStencilView *dsv = nullptr;
			hr = _orig->CreateDepthStencilView(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_DSV_DIMENSION_UNKNOWN ? &new_desc : nullptr, &dsv);
			if (SUCCEEDED(hr))
			{
				_views.register_object(dsv);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device::CreateDepthStencilView" << " failed with error code " << hr << '.';
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(dsv) };
		}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, reshade::d3d10::convert_resource_view_desc(new_desc), &out);
	*ppDepthStencilView = reinterpret_cast<ID3D10DepthStencilView *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D10InputLayout **ppInputLayout)
{
	return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader)
{
	HRESULT hr = S_OK;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppVertexShader](reshade::api::device *, const void *code, size_t code_size) {
			hr = _orig->CreateVertexShader(code, code_size, ppVertexShader);
		}, this, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader)
{
	HRESULT hr = S_OK;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppGeometryShader](reshade::api::device *, const void *code, size_t code_size) {
			hr = _orig->CreateGeometryShader(code, code_size, ppGeometryShader);
		}, this, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D10_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, UINT OutputStreamStride, ID3D10GeometryShader **ppGeometryShader)
{
	HRESULT hr = S_OK;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader](reshade::api::device *, const void *code, size_t code_size) {
			hr = _orig->CreateGeometryShaderWithStreamOutput(code, code_size, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);
		}, this, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader)
{
	HRESULT hr = S_OK;
	reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
		[this, &hr, ppPixelShader](reshade::api::device *, const void *code, size_t code_size) {
			hr = _orig->CreatePixelShader(code, code_size, ppPixelShader);
		}, this, pShaderBytecode, BytecodeLength);
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(const D3D10_BLEND_DESC *pBlendStateDesc, ID3D10BlendState **ppBlendState)
{
	return _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D10DepthStencilState **ppDepthStencilState)
{
	return _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(const D3D10_RASTERIZER_DESC *pRasterizerDesc, ID3D10RasterizerState **ppRasterizerState)
{
	return _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(const D3D10_SAMPLER_DESC *pSamplerDesc, ID3D10SamplerState **ppSamplerState)
{
	return _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateQuery(const D3D10_QUERY_DESC *pQueryDesc, ID3D10Query **ppQuery)
{
	return _orig->CreateQuery(pQueryDesc, ppQuery);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePredicate(const D3D10_QUERY_DESC *pPredicateDesc, ID3D10Predicate **ppPredicate)
{
	return _orig->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateCounter(const D3D10_COUNTER_DESC *pCounterDesc, ID3D10Counter **ppCounter)
{
	return _orig->CreateCounter(pCounterDesc, ppCounter);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
	return _orig->CheckFormatSupport(Format, pFormatSupport);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels)
{
	return _orig->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}
void    STDMETHODCALLTYPE D3D10Device::CheckCounterInfo(D3D10_COUNTER_INFO *pCounterInfo)
{
	_orig->CheckCounterInfo(pCounterInfo);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckCounter(const D3D10_COUNTER_DESC *pDesc, D3D10_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength)
{
	return _orig->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetCreationFlags()
{
	return _orig->GetCreationFlags();
}
HRESULT STDMETHODCALLTYPE D3D10Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource)
{
	return _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}
void    STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(UINT Width, UINT Height)
{
	_orig->SetTextFilterSize(Width, Height);
}
void    STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(UINT *pWidth, UINT *pHeight)
{
	_orig->GetTextFilterSize(pWidth, pHeight);
}

HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D10ShaderResourceView1 **ppSRView)
{
	if (pResource == nullptr)
		return E_INVALIDARG;

	D3D10_SHADER_RESOURCE_VIEW_DESC1 new_desc =
		pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D10_1_SRV_DIMENSION_UNKNOWN };

	HRESULT hr = S_OK;
	reshade::api::resource_view_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
		[this, &hr, &new_desc](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
			assert(usage_type == reshade::api::resource_usage::shader_resource);
			reshade::d3d10::convert_resource_view_desc(desc, new_desc);

			ID3D10ShaderResourceView1 *srv = nullptr;
			hr = _orig->CreateShaderResourceView1(reinterpret_cast<ID3D10Resource *>(resource.handle), new_desc.ViewDimension != D3D10_1_SRV_DIMENSION_UNKNOWN ? &new_desc : nullptr, &srv);
			if (SUCCEEDED(hr))
			{
				_views.register_object(srv);
			}
#if RESHADE_ADDON || RESHADE_VERBOSE_LOG
			else
			{
				LOG(WARN) << "ID3D10Device1::CreateShaderResourceView1" << " failed with error code " << hr << '.';
			}
#endif
			*out = { reinterpret_cast<uintptr_t>(srv) };
		}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, reshade::d3d10::convert_resource_view_desc(new_desc), &out);
	*ppSRView = reinterpret_cast<ID3D10ShaderResourceView1 *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(const D3D10_BLEND_DESC1 *pBlendStateDesc, ID3D10BlendState1 **ppBlendState)
{
	return _orig->CreateBlendState1(pBlendStateDesc, ppBlendState);
}
D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE D3D10Device::GetFeatureLevel()
{
	return _orig->GetFeatureLevel();
}
