/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d11_4.h>

struct DXGIDevice;
struct D3D11DeviceContext;

struct DECLSPEC_UUID("72299288-2C68-4AD8-945D-2BFB5AA9C609") D3D11Device : ID3D11Device5
{
	D3D11Device(IDXGIDevice1 *dxgi_device, ID3D11Device *original);

	D3D11Device(const D3D11Device &) = delete;
	D3D11Device &operator=(const D3D11Device &) = delete;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	ULONG   STDMETHODCALLTYPE AddRef() override;
	ULONG   STDMETHODCALLTYPE Release() override;

	#pragma region ID3D11Device
	HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer) override;
	HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D) override;
	HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D) override;
	HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D) override;
	HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView) override;
	HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUAView) override;
	HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView) override;
	HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView) override;
	HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout) override;
	HRESULT STDMETHODCALLTYPE CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader) override;
	HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader) override;
	HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader) override;
	HRESULT STDMETHODCALLTYPE CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader) override;
	HRESULT STDMETHODCALLTYPE CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) override;
	HRESULT STDMETHODCALLTYPE CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader) override;
	HRESULT STDMETHODCALLTYPE CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader) override;
	HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) override;
	HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState) override;
	HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState) override;
	HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState) override;
	HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState) override;
	HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery) override;
	HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc, ID3D11Predicate **ppPredicate) override;
	HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter) override;
	HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext) override;
	HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource) override;
	HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport) override;
	HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) override;
	void    STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo) override;
	HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) override;
	HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override;
	UINT    STDMETHODCALLTYPE GetCreationFlags() override;
	HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override;
	void    STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) override;
	HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override;
	UINT    STDMETHODCALLTYPE GetExceptionMode() override;
	D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() override;
	#pragma endregion
	#pragma region ID3D11Device1
	void    STDMETHODCALLTYPE GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext) override;
	HRESULT STDMETHODCALLTYPE CreateDeferredContext1(UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext) override;
	HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc, ID3D11BlendState1 **ppBlendState);
	HRESULT STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState) override;
	HRESULT STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface, D3D_FEATURE_LEVEL *pChosenFeatureLevel, ID3DDeviceContextState **ppContextState) override;
	HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource, REFIID returnedInterface, void **ppResource) override;
	HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface, void **ppResource) override;
	#pragma endregion
	#pragma region ID3D11Device2
	void    STDMETHODCALLTYPE GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext) override;
	HRESULT STDMETHODCALLTYPE CreateDeferredContext2(UINT ContextFlags, ID3D11DeviceContext2 **ppDeferredContext) override;
	void    STDMETHODCALLTYPE GetResourceTiling(ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D11_PACKED_MIP_DESC *pPackedMipDesc, D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips) override;
	HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT *pNumQualityLevels) override;
	#pragma endregion
	#pragma region ID3D11Device3
	HRESULT STDMETHODCALLTYPE CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc1, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D1 **ppTexture2D) override;
	HRESULT STDMETHODCALLTYPE CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1, const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D1 **ppTexture3D) override;
	HRESULT STDMETHODCALLTYPE CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc, ID3D11RasterizerState2 **ppRasterizerState) override;
	HRESULT STDMETHODCALLTYPE CreateShaderResourceView1(ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1, ID3D11ShaderResourceView1 **ppSRView1) override;
	HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView1(ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1, ID3D11UnorderedAccessView1 **ppUAView1) override;
	HRESULT STDMETHODCALLTYPE CreateRenderTargetView1(ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1, ID3D11RenderTargetView1 **ppRTView1) override;
	HRESULT STDMETHODCALLTYPE CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1, ID3D11Query1 **ppQuery1) override;
	void    STDMETHODCALLTYPE GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext) override;
	HRESULT STDMETHODCALLTYPE CreateDeferredContext3(UINT ContextFlags, ID3D11DeviceContext3 **ppDeferredContext) override;
	void    STDMETHODCALLTYPE WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) override;
	void    STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch, ID3D11Resource *pSrcResource, UINT SrcSubresource, const D3D11_BOX *pSrcBox) override;
	#pragma endregion
	#pragma region ID3D11Device4
	HRESULT STDMETHODCALLTYPE RegisterDeviceRemovedEvent( HANDLE hEvent, DWORD *pdwCookie) override;
	void    STDMETHODCALLTYPE UnregisterDeviceRemoved(DWORD dwCookie) override;
	#pragma endregion
	#pragma region ID3D11Device5
	HRESULT STDMETHODCALLTYPE OpenSharedFence(HANDLE hFence, REFIID ReturnedInterface, void **ppFence) override;
	HRESULT STDMETHODCALLTYPE CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID ReturnedInterface, void **ppFence) override;
	#pragma endregion

	bool check_and_upgrade_interface(REFIID riid);

	LONG _ref = 1;
	ID3D11Device *_orig;
	unsigned int _interface_version;
	DXGIDevice *const _dxgi_device;
	D3D11DeviceContext *_immediate_context = nullptr;
};
