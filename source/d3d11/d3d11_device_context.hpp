/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <atomic>
#include "d3d11.hpp"
#include "draw_call_tracker.hpp"

struct D3D11DeviceContext : ID3D11DeviceContext3
{
	D3D11DeviceContext(D3D11Device *device, ID3D11DeviceContext  *original) :
		_orig(original),
		_interface_version(0),
		_device(device) { }
	D3D11DeviceContext(D3D11Device *device, ID3D11DeviceContext1 *original) :
		_orig(original),
		_interface_version(1),
		_device(device) { }
	D3D11DeviceContext(D3D11Device *device, ID3D11DeviceContext2 *original) :
		_orig(original),
		_interface_version(2),
		_device(device) { }
	D3D11DeviceContext(D3D11Device *device, ID3D11DeviceContext3 *original) :
		_orig(original),
		_interface_version(3),
		_device(device) { }

	D3D11DeviceContext(const D3D11DeviceContext &) = delete;
	D3D11DeviceContext &operator=(const D3D11DeviceContext &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D11DeviceChild
	virtual void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) override;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	#pragma endregion
	#pragma region ID3D11DeviceContext
	virtual void STDMETHODCALLTYPE VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE VSSetShader(ID3D11VertexShader *pVertexShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override;
	virtual void STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertexLocation) override;
	virtual HRESULT STDMETHODCALLTYPE Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource) override;
	virtual void STDMETHODCALLTYPE Unmap(ID3D11Resource *pResource, UINT Subresource) override;
	virtual void STDMETHODCALLTYPE PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout *pInputLayout) override;
	virtual void STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) override;
	virtual void STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override;
	virtual void STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override;
	virtual void STDMETHODCALLTYPE GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) override;
	virtual void STDMETHODCALLTYPE VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE Begin(ID3D11Asynchronous *pAsync) override;
	virtual void STDMETHODCALLTYPE End(ID3D11Asynchronous *pAsync) override;
	virtual HRESULT STDMETHODCALLTYPE GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override;
	virtual void STDMETHODCALLTYPE SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue) override;
	virtual void STDMETHODCALLTYPE GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView) override;
	virtual void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts) override;
	virtual void STDMETHODCALLTYPE OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) override;
	virtual void STDMETHODCALLTYPE OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) override;
	virtual void STDMETHODCALLTYPE SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE DrawAuto() override;
	virtual void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override;
	virtual void STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override;
	virtual void STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override;
	virtual void STDMETHODCALLTYPE DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override;
	virtual void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState *pRasterizerState) override;
	virtual void STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports) override;
	virtual void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) override;
	virtual void STDMETHODCALLTYPE CopySubresourceRegion(ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox) override;
	virtual void STDMETHODCALLTYPE CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource) override;
	virtual void STDMETHODCALLTYPE UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) override;
	virtual void STDMETHODCALLTYPE CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) override;
	virtual void STDMETHODCALLTYPE ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) override;
	virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4]) override;
	virtual void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4]) override;
	virtual void STDMETHODCALLTYPE ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) override;
	virtual void STDMETHODCALLTYPE GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) override;
	virtual void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD) override;
	virtual FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource *pResource) override;
	virtual void STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) override;
	virtual void STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override;
	virtual void STDMETHODCALLTYPE HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts) override;
	virtual void STDMETHODCALLTYPE CSSetShader(ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) override;
	virtual void STDMETHODCALLTYPE CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout **ppInputLayout) override;
	virtual void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) override;
	virtual void STDMETHODCALLTYPE GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) override;
	virtual void STDMETHODCALLTYPE VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue) override;
	virtual void STDMETHODCALLTYPE GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView) override;
	virtual void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override;
	virtual void STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask) override;
	virtual void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef) override;
	virtual void STDMETHODCALLTYPE SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets) override;
	virtual void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState **ppRasterizerState) override;
	virtual void STDMETHODCALLTYPE RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports) override;
	virtual void STDMETHODCALLTYPE RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects) override;
	virtual void STDMETHODCALLTYPE HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override;
	virtual void STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) override;
	virtual void STDMETHODCALLTYPE CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE ClearState() override;
	virtual void STDMETHODCALLTYPE Flush() override;
	virtual UINT STDMETHODCALLTYPE GetContextFlags() override;
	virtual HRESULT STDMETHODCALLTYPE FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override;
	virtual D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() override;
	#pragma endregion
	#pragma region ID3D11DeviceContext1
	virtual void STDMETHODCALLTYPE CopySubresourceRegion1(ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags) override;
	virtual void STDMETHODCALLTYPE UpdateSubresource1(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) override;
	virtual void STDMETHODCALLTYPE DiscardResource(ID3D11Resource *pResource) override;
	virtual void STDMETHODCALLTYPE DiscardView(ID3D11View *pResourceView) override;
	virtual void STDMETHODCALLTYPE VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant, const UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants) override;
	virtual void STDMETHODCALLTYPE SwapDeviceContextState(ID3DDeviceContextState *pState, ID3DDeviceContextState **ppPreviousState) override;
	virtual void STDMETHODCALLTYPE ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT NumRects) override;
	virtual void STDMETHODCALLTYPE DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT NumRects) override;
	#pragma endregion
	#pragma region ID3D11DeviceContext2
	virtual HRESULT STDMETHODCALLTYPE UpdateTileMappings(ID3D11Resource *pTiledResource, UINT NumTiledResourceRegions, const D3D11_TILED_RESOURCE_COORDINATE *pTiledResourceRegionStartCoordinates, const D3D11_TILE_REGION_SIZE *pTiledResourceRegionSizes, ID3D11Buffer *pTilePool, UINT NumRanges, const UINT *pRangeFlags, const UINT *pTilePoolStartOffsets, const UINT *pRangeTileCounts, UINT Flags) override;
	virtual HRESULT STDMETHODCALLTYPE CopyTileMappings(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestRegionStartCoordinate, ID3D11Resource *pSourceTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pSourceRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, UINT Flags) override;
	virtual void STDMETHODCALLTYPE CopyTiles(ID3D11Resource *pTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pTileRegionSize, ID3D11Buffer *pBuffer, UINT64 BufferStartOffsetInBytes, UINT Flags) override;
	virtual void STDMETHODCALLTYPE UpdateTiles(ID3D11Resource *pDestTiledResource, const D3D11_TILED_RESOURCE_COORDINATE *pDestTileRegionStartCoordinate, const D3D11_TILE_REGION_SIZE *pDestTileRegionSize, const void *pSourceTileData, UINT Flags) override;
	virtual HRESULT STDMETHODCALLTYPE ResizeTilePool(ID3D11Buffer *pTilePool, UINT64 NewSizeInBytes) override;
	virtual void STDMETHODCALLTYPE TiledResourceBarrier(ID3D11DeviceChild *pTiledResourceOrViewAccessBeforeBarrier, ID3D11DeviceChild *pTiledResourceOrViewAccessAfterBarrier) override;
	virtual BOOL STDMETHODCALLTYPE IsAnnotationEnabled() override;
	virtual void STDMETHODCALLTYPE SetMarkerInt(LPCWSTR pLabel, INT Data) override;
	virtual void STDMETHODCALLTYPE BeginEventInt(LPCWSTR pLabel, INT Data) override;
	virtual void STDMETHODCALLTYPE EndEvent() override;
	#pragma endregion
	#pragma region ID3D11DeviceContext3
	virtual void STDMETHODCALLTYPE Flush1(D3D11_CONTEXT_TYPE ContextType, HANDLE hEvent) override;
	virtual void STDMETHODCALLTYPE SetHardwareProtectionState(BOOL HwProtectionEnable) override;
	virtual void STDMETHODCALLTYPE GetHardwareProtectionState(BOOL *pHwProtectionEnable) override;
	#pragma endregion

	void log_drawcall(UINT vertices);
	void clear_drawcall_stats();

	void track_active_depthstencil(ID3D11DepthStencilView* pDepthStencilView);
	void track_cleared_depthstencil(ID3D11DepthStencilView* pDepthStencilView);
	bool _save_depth_texture(ID3D11DepthStencilView *pDepthStencilView);

	LONG _ref = 1;
	ID3D11DeviceContext *_orig;
	bool _replace_depth_texture = false;
	unsigned int _interface_version;
	D3D11Device *const _device;
	com_ptr<ID3D11DepthStencilView> _active_depthstencil;
	reshade::d3d11::draw_call_tracker _draw_call_tracker;
};
