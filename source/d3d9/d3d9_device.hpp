/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "d3d9.hpp"
#include "com_ptr.hpp"
#include <vector>

struct Direct3DDevice9 : IDirect3DDevice9Ex
{
	explicit Direct3DDevice9(IDirect3DDevice9   *original) :
		_orig(original),
		_extended_interface(false) { }
	explicit Direct3DDevice9(IDirect3DDevice9Ex *original) :
		_orig(original),
		_extended_interface(true) { }

	Direct3DDevice9(const Direct3DDevice9 &) = delete;
	Direct3DDevice9 &operator=(const Direct3DDevice9 &) = delete;

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;
	#pragma endregion
	#pragma region IDirect3DDevice9
	virtual HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override;
	virtual UINT STDMETHODCALLTYPE GetAvailableTextureMem() override;
	virtual HRESULT STDMETHODCALLTYPE EvictManagedResources() override;
	virtual HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9 **ppD3D9) override;
	virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9 *pCaps) override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode) override;
	virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override;
	virtual HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap) override;
	virtual void STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags) override;
	virtual BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow) override;
	virtual HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain) override;
	virtual HRESULT STDMETHODCALLTYPE GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **ppSwapChain) override;
	virtual UINT STDMETHODCALLTYPE GetNumberOfSwapChains() override;
	virtual HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
	virtual HRESULT STDMETHODCALLTYPE Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion) override;
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
	virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus) override;
	virtual HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL bEnableDialogs) override;
	virtual void STDMETHODCALLTYPE SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp) override;
	virtual void STDMETHODCALLTYPE GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp) override;
	virtual HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint) override;
	virtual HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) override;
	virtual HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface) override;
	virtual HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface) override;
	virtual HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
	virtual HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color) override;
	virtual HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle) override;
	virtual HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget) override;
	virtual HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) override;
	virtual HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil) override;
	virtual HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) override;
	virtual HRESULT STDMETHODCALLTYPE BeginScene() override;
	virtual HRESULT STDMETHODCALLTYPE EndScene() override;
	virtual HRESULT STDMETHODCALLTYPE Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
	virtual HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override;
	virtual HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) override;
	virtual HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix) override;
	virtual HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9 *pViewport) override;
	virtual HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9 *pViewport) override;
	virtual HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9 *pMaterial) override;
	virtual HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9 *pMaterial) override;
	virtual HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, const D3DLIGHT9 *pLight) override;
	virtual HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT9 *pLight) override;
	virtual HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable) override;
	virtual HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL *pEnable) override;
	virtual HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, const float *pPlane) override;
	virtual HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float *pPlane) override;
	virtual HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override;
	virtual HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override;
	virtual HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB) override;
	virtual HRESULT STDMETHODCALLTYPE BeginStateBlock() override;
	virtual HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9 **ppSB) override;
	virtual HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus) override;
	virtual HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9 *pClipStatus) override;
	virtual HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) override;
	virtual HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) override;
	virtual HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) override;
	virtual HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
	virtual HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue) override;
	virtual HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override;
	virtual HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD *pNumPasses) override;
	virtual HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY *pEntries) override;
	virtual HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries) override;
	virtual HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber) override;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber) override;
	virtual HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT *pRect) override;
	virtual HRESULT STDMETHODCALLTYPE GetScissorRect(RECT *pRect) override;
	virtual HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL bSoftware) override;
	virtual BOOL STDMETHODCALLTYPE GetSoftwareVertexProcessing() override;
	virtual HRESULT STDMETHODCALLTYPE SetNPatchMode(float nSegments) override;
	virtual float STDMETHODCALLTYPE GetNPatchMode() override;
	virtual HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
	virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override;
	virtual HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
	virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
	virtual HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags) override;
	virtual HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl) override;
	virtual HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl) override;
	virtual HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl) override;
	virtual HRESULT STDMETHODCALLTYPE SetFVF(DWORD FVF) override;
	virtual HRESULT STDMETHODCALLTYPE GetFVF(DWORD *pFVF) override;
	virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader) override;
	virtual HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9 *pShader) override;
	virtual HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9 **ppShader) override;
	virtual HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) override;
	virtual HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) override;
	virtual HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) override;
	virtual HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) override;
	virtual HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride) override;
	virtual HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT StreamNumber, UINT Divider) override;
	virtual HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT StreamNumber, UINT *Divider) override;
	virtual HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9 *pIndexData) override;
	virtual HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9 **ppIndexData) override;
	virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader) override;
	virtual HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9 *pShader) override;
	virtual HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9 **ppShader) override;
	virtual HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount) override;
	virtual HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount) override;
	virtual HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount) override;
	virtual HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount) override;
	virtual	HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount) override;
	virtual HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo) override;
	virtual HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo) override;
	virtual HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) override;
	virtual HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery) override;
	#pragma endregion
	#pragma region IDirect3DDevice9Ex
	virtual HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns) override;
	virtual HRESULT STDMETHODCALLTYPE ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset) override;
	virtual HRESULT STDMETHODCALLTYPE PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags) override;
	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override;
	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override;
	virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(UINT iSwapChain) override;
	virtual HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT32 NumResources) override;
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;
	virtual HRESULT STDMETHODCALLTYPE CheckDeviceState(HWND hDestinationWindow) override;
	virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
	virtual HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
	virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage) override;
	virtual HRESULT STDMETHODCALLTYPE ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode) override;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) override;
	#pragma endregion

	LONG _ref = 1;
	IDirect3DDevice9 *_orig;
	bool _extended_interface;
	Direct3DSwapChain9 *_implicit_swapchain = nullptr;
	std::vector<Direct3DSwapChain9 *> _additional_swapchains;
	com_ptr<IDirect3DSurface9> _auto_depthstencil;
	bool _use_software_rendering = false;
};
