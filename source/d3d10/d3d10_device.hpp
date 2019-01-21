/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "d3d10.hpp"
#include "draw_call_tracker.hpp"

struct D3D10Device : ID3D10Device1
{
	explicit D3D10Device(ID3D10Device1 *original) :
		_orig(original) { }

	D3D10Device(const D3D10Device &) = delete;
	D3D10Device &operator=(const D3D10Device &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region ID3D10Device
	virtual void STDMETHODCALLTYPE VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE PSSetShader(ID3D10PixelShader *pPixelShader) override;
	virtual void STDMETHODCALLTYPE PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE VSSetShader(ID3D10VertexShader *pVertexShader) override;
	virtual void STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override;
	virtual void STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertexLocation) override;
	virtual void STDMETHODCALLTYPE PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE IASetInputLayout(ID3D10InputLayout *pInputLayout) override;
	virtual void STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE IASetIndexBuffer(ID3D10Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) override;
	virtual void STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override;
	virtual void STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override;
	virtual void STDMETHODCALLTYPE GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE GSSetShader(ID3D10GeometryShader *pShader) override;
	virtual void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY Topology) override;
	virtual void STDMETHODCALLTYPE VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE SetPredication(ID3D10Predicate *pPredicate, BOOL PredicateValue) override;
	virtual void STDMETHODCALLTYPE GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers) override;
	virtual void STDMETHODCALLTYPE OMSetRenderTargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView) override;
	virtual void STDMETHODCALLTYPE OMSetBlendState(ID3D10BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) override;
	virtual void STDMETHODCALLTYPE OMSetDepthStencilState(ID3D10DepthStencilState *pDepthStencilState, UINT StencilRef) override;
	virtual void STDMETHODCALLTYPE SOSetTargets(UINT NumBuffers, ID3D10Buffer *const *ppSOTargets, const UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE DrawAuto() override;
	virtual void STDMETHODCALLTYPE RSSetState(ID3D10RasterizerState *pRasterizerState) override;
	virtual void STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D10_VIEWPORT *pViewports) override;
	virtual void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D10_RECT *pRects) override;
	virtual void STDMETHODCALLTYPE CopySubresourceRegion(ID3D10Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D10Resource *pSrcResource, UINT SrcSubresource, const D3D10_BOX *pSrcBox) override;
	virtual void STDMETHODCALLTYPE CopyResource(ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource) override;
	virtual void STDMETHODCALLTYPE UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) override;
	virtual void STDMETHODCALLTYPE ClearRenderTargetView(ID3D10RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) override;
	virtual void STDMETHODCALLTYPE ClearDepthStencilView(ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) override;
	virtual void STDMETHODCALLTYPE GenerateMips(ID3D10ShaderResourceView *pShaderResourceView) override;
	virtual void STDMETHODCALLTYPE ResolveSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, ID3D10Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) override;
	virtual void STDMETHODCALLTYPE VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE PSGetShader(ID3D10PixelShader **ppPixelShader) override;
	virtual void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE VSGetShader(ID3D10VertexShader **ppVertexShader) override;
	virtual void STDMETHODCALLTYPE PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE IAGetInputLayout(ID3D10InputLayout **ppInputLayout) override;
	virtual void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D10Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) override;
	virtual void STDMETHODCALLTYPE GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers) override;
	virtual void STDMETHODCALLTYPE GSGetShader(ID3D10GeometryShader **ppGeometryShader) override;
	virtual void STDMETHODCALLTYPE IAGetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY *pTopology) override;
	virtual void STDMETHODCALLTYPE VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE GetPredication(ID3D10Predicate **ppPredicate, BOOL *pPredicateValue) override;
	virtual void STDMETHODCALLTYPE GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews) override;
	virtual void STDMETHODCALLTYPE GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers) override;
	virtual void STDMETHODCALLTYPE OMGetRenderTargets(UINT NumViews, ID3D10RenderTargetView **ppRenderTargetViews, ID3D10DepthStencilView **ppDepthStencilView) override;
	virtual void STDMETHODCALLTYPE OMGetBlendState(ID3D10BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask) override;
	virtual void STDMETHODCALLTYPE OMGetDepthStencilState(ID3D10DepthStencilState **ppDepthStencilState, UINT *pStencilRef) override;
	virtual void STDMETHODCALLTYPE SOGetTargets(UINT NumBuffers, ID3D10Buffer **ppSOTargets, UINT *pOffsets) override;
	virtual void STDMETHODCALLTYPE RSGetState(ID3D10RasterizerState **ppRasterizerState) override;
	virtual void STDMETHODCALLTYPE RSGetViewports(UINT *NumViewports, D3D10_VIEWPORT *pViewports) override;
	virtual void STDMETHODCALLTYPE RSGetScissorRects(UINT *NumRects, D3D10_RECT *pRects) override;
	virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override;
	virtual HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override;
	virtual UINT STDMETHODCALLTYPE GetExceptionMode() override;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	virtual void STDMETHODCALLTYPE ClearState() override;
	virtual void STDMETHODCALLTYPE Flush() override;
	virtual HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D10_BUFFER_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Buffer **ppBuffer) override;
	virtual HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D10_TEXTURE1D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture1D **ppTexture1D) override;
	virtual HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D10_TEXTURE2D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture2D **ppTexture2D) override;
	virtual HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D10_TEXTURE3D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture3D **ppTexture3D) override;
	virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D10ShaderResourceView **ppSRView) override;
	virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc, ID3D10RenderTargetView **ppRTView) override;
	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView) override;
	virtual HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D10InputLayout **ppInputLayout) override;
	virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader) override;
	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader) override;
	virtual HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D10_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, UINT OutputStreamStride, ID3D10GeometryShader **ppGeometryShader) override;
	virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader) override;
	virtual HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D10_BLEND_DESC *pBlendStateDesc, ID3D10BlendState **ppBlendState) override;
	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D10DepthStencilState **ppDepthStencilState) override;
	virtual HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D10_RASTERIZER_DESC *pRasterizerDesc, ID3D10RasterizerState **ppRasterizerState) override;
	virtual HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D10_SAMPLER_DESC *pSamplerDesc, ID3D10SamplerState **ppSamplerState) override;
	virtual HRESULT STDMETHODCALLTYPE CreateQuery(const D3D10_QUERY_DESC *pQueryDesc, ID3D10Query **ppQuery) override;
	virtual HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D10_QUERY_DESC *pPredicateDesc, ID3D10Predicate **ppPredicate) override;
	virtual HRESULT STDMETHODCALLTYPE CreateCounter(const D3D10_COUNTER_DESC *pCounterDesc, ID3D10Counter **ppCounter) override;
	virtual HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport) override;
	virtual HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) override;
	virtual void STDMETHODCALLTYPE CheckCounterInfo(D3D10_COUNTER_INFO *pCounterInfo) override;
	virtual HRESULT STDMETHODCALLTYPE CheckCounter(const D3D10_COUNTER_DESC *pDesc, D3D10_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) override;
	virtual UINT STDMETHODCALLTYPE GetCreationFlags() override;
	virtual HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource) override;
	virtual void STDMETHODCALLTYPE SetTextFilterSize(UINT Width, UINT Height) override;
	virtual void STDMETHODCALLTYPE GetTextFilterSize(UINT *pWidth, UINT *pHeight) override;
	#pragma endregion
	#pragma region ID3D10Device1
	virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView1(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D10ShaderResourceView1 **ppSRView) override;
	virtual HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D10_BLEND_DESC1 *pBlendStateDesc, ID3D10BlendState1 **ppBlendState) override;
	virtual D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE GetFeatureLevel() override;
	#pragma endregion

	void clear_drawcall_stats();

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	bool save_depth_texture(ID3D10DepthStencilView *pDepthStencilView, bool cleared);

	void track_active_rendertargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView);
	void track_cleared_depthstencil(ID3D10DepthStencilView* pDepthStencilView);
#endif

	LONG _ref = 1;
	ID3D10Device1 *_orig;
	struct DXGIDevice *_dxgi_device = nullptr;
	std::vector<std::shared_ptr<reshade::d3d10::runtime_d3d10>> _runtimes;
	com_ptr<ID3D10DepthStencilView> _active_depthstencil;
	reshade::d3d10::draw_call_tracker _draw_call_tracker;
	unsigned int _clear_DSV_iter = 1;
};
