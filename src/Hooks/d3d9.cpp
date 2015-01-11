#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\RuntimeD3D9.hpp"

#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx

// -----------------------------------------------------------------------------------------------------

namespace
{
	struct Direct3DDevice9 : public IDirect3DDevice9Ex, private boost::noncopyable
	{
		friend struct Direct3DSwapChain9;

		Direct3DDevice9(IDirect3DDevice9 *originalDevice) : mRef(1), mOrig(originalDevice), mImplicitSwapChain(nullptr), mAutoDepthStencil(nullptr)
		{
			assert(originalDevice != nullptr);
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

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

		ULONG mRef;
		IDirect3DDevice9 *const mOrig;
		Direct3DSwapChain9 *mImplicitSwapChain;
		std::vector<Direct3DSwapChain9 *> mAdditionalSwapChains;
		IDirect3DSurface9 *mAutoDepthStencil;
	};
	struct Direct3DSwapChain9 : public IDirect3DSwapChain9Ex, private boost::noncopyable
	{
		friend struct Direct3DDevice9;

		Direct3DSwapChain9(Direct3DDevice9 *device, IDirect3DSwapChain9 *originalSwapChain, const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> &runtime) : mRef(1), mDevice(device), mOrig(originalSwapChain), mRuntime(runtime)
		{
			assert(device != nullptr);
			assert(originalSwapChain != nullptr);
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion, DWORD dwFlags) override;
		virtual HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9 *pDestSurface) override;
		virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer) override;
		virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override;
		virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *pMode) override;
		virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9 **ppDevice) override;
		virtual HRESULT STDMETHODCALLTYPE GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) override;

		virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;
		virtual HRESULT STDMETHODCALLTYPE GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics) override;
		virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation) override;

		ULONG mRef;
		Direct3DDevice9 *const mDevice;
		IDirect3DSwapChain9 *const mOrig;
		std::shared_ptr<ReShade::Runtimes::D3D9Runtime> mRuntime;
	};

	LPCSTR GetErrorString(HRESULT hr)
	{
		switch (hr)
		{
			case E_INVALIDARG:
				return "E_INVALIDARG";
			case D3DERR_NOTAVAILABLE:
				return "D3DERR_NOTAVAILABLE";
			case D3DERR_INVALIDCALL:
				return "D3DERR_INVALIDCALL";
			case D3DERR_INVALIDDEVICE:
				return "D3DERR_INVALIDDEVICE";
			case D3DERR_DEVICEHUNG:
				return "D3DERR_DEVICEHUNG";
			case D3DERR_DEVICELOST:
				return "D3DERR_DEVICELOST";
			case D3DERR_DEVICENOTRESET:
				return "D3DERR_DEVICENOTRESET";
			case D3DERR_WASSTILLDRAWING:
				return "D3DERR_WASSTILLDRAWING";
			default:
				__declspec(thread) static CHAR buf[20];
				sprintf_s(buf, "0x%lx", hr);
				return buf;
		}
	}
	void DumpPresentParameters(const D3DPRESENT_PARAMETERS *pp)
	{
		LOG(TRACE) << "> Dumping Presentation Parameters:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		char value[40];
		sprintf_s(value, "%-39u", pp->BackBufferWidth);
		LOG(TRACE) << "  | BackBufferWidth                         | " << value << " |";
		sprintf_s(value, "%-39u", pp->BackBufferHeight);
		LOG(TRACE) << "  | BackBufferHeight                        | " << value << " |";
		sprintf_s(value, "%-39u", pp->BackBufferFormat);
		LOG(TRACE) << "  | BackBufferFormat                        | " << value << " |";
		sprintf_s(value, "%-39u", pp->BackBufferCount);
		LOG(TRACE) << "  | BackBufferCount                         | " << value << " |";
		sprintf_s(value, "%-39u", pp->MultiSampleType);
		LOG(TRACE) << "  | MultiSampleType                         | " << value << " |";
		sprintf_s(value, "%-39u", pp->MultiSampleQuality);
		LOG(TRACE) << "  | MultiSampleQuality                      | " << value << " |";
		sprintf_s(value, "%-39u", pp->SwapEffect);
		LOG(TRACE) << "  | SwapEffect                              | " << value << " |";
		sprintf_s(value, "0x%016IX                     ", static_cast<const void *>(pp->hDeviceWindow));
		LOG(TRACE) << "  | hDeviceWindow                           | " << value << " |";
		sprintf_s(value, "%-39d", pp->Windowed);
		LOG(TRACE) << "  | Windowed                                | " << value << " |";
		sprintf_s(value, "%-39d", pp->EnableAutoDepthStencil);
		LOG(TRACE) << "  | EnableAutoDepthStencil                  | " << value << " |";
		sprintf_s(value, "%-39u", pp->AutoDepthStencilFormat);
		LOG(TRACE) << "  | AutoDepthStencilFormat                  | " << value << " |";
		sprintf_s(value, "0x%016X                     ", pp->Flags);
		LOG(TRACE) << "  | Flags                                   | " << value << " |";
		sprintf_s(value, "%-39u", pp->FullScreen_RefreshRateInHz);
		LOG(TRACE) << "  | FullScreen_RefreshRateInHz              | " << value << " |";
		sprintf_s(value, "%-39u", pp->PresentationInterval);
		LOG(TRACE) << "  | PresentationInterval                    | " << value << " |";

		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
	}
	void AdjustPresentParameters(D3DPRESENT_PARAMETERS *pp)
	{
		DumpPresentParameters(pp);

		if (pp->SwapEffect != D3DSWAPEFFECT_COPY && pp->PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE && pp->BackBufferCount < 2)
		{
			LOG(WARNING) << "> Forcing tripple buffering.";

			pp->BackBufferCount = 2;
		}

		if (pp->MultiSampleType != D3DMULTISAMPLE_NONE)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";
		}
	}
}

// -----------------------------------------------------------------------------------------------------

// IDirect3DSwapChain9
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
{
	const HRESULT hr = this->mOrig->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr) && (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DSwapChain9) || riid == __uuidof(IDirect3DSwapChain9Ex)))
	{
		this->mRef++;

		*ppvObj = this;
	}

	return hr;
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::Release()
{
	if (--this->mRef == 0)
	{
		assert(this->mRuntime != nullptr);

		this->mRuntime->OnDeleteInternal();
		this->mRuntime.reset();
	}

	const ULONG ref = this->mOrig->Release();

	if (this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DSwapChain9' object (" << ref << ") is inconsistent.";
	}

	if (ref == 0)
	{
		const auto it = std::find(this->mDevice->mAdditionalSwapChains.begin(), this->mDevice->mAdditionalSwapChains.end(), this);

		if (it != this->mDevice->mAdditionalSwapChains.end())
		{
			this->mDevice->mAdditionalSwapChains.erase(it);
		}

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(this->mRuntime != nullptr);

	this->mRuntime->OnPresentInternal();

	return this->mOrig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetFrontBufferData(IDirect3DSurface9 *pDestSurface)
{
	return this->mOrig->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	return this->mOrig->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
	return this->mOrig->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
	return this->mOrig->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	return this->mOrig->GetPresentParameters(pPresentationParameters);
}

// IDirect3DSwapChain9Ex
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetLastPresentCount(UINT *pLastPresentCount)
{
	return static_cast<IDirect3DSwapChain9Ex *>(this->mOrig)->GetLastPresentCount(pLastPresentCount);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics)
{
	return static_cast<IDirect3DSwapChain9Ex *>(this->mOrig)->GetPresentStats(pPresentationStatistics);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	return static_cast<IDirect3DSwapChain9Ex *>(this->mOrig)->GetDisplayModeEx(pMode, pRotation);
}

// IDirect3DDevice9
HRESULT STDMETHODCALLTYPE Direct3DDevice9::QueryInterface(REFIID riid, void **ppvObj)
{
	const HRESULT hr = this->mOrig->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr) && (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DDevice9) || riid == __uuidof(IDirect3DDevice9Ex)))
	{
		this->mRef++;

		*ppvObj = this;
	}

	return hr;
}
ULONG STDMETHODCALLTYPE Direct3DDevice9::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DDevice9::Release()
{
	if (--this->mRef == 0)
	{
		if (this->mAutoDepthStencil != nullptr)
		{
			this->mAutoDepthStencil->Release();
			this->mAutoDepthStencil = nullptr;
		}

		assert(this->mImplicitSwapChain != nullptr);

		this->mImplicitSwapChain->Release();
		this->mImplicitSwapChain = nullptr;
	}

	const ULONG ref = this->mOrig->Release();

	if (this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DDevice9' object (" << ref << ") is inconsistent.";
	}

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::TestCooperativeLevel()
{
	return this->mOrig->TestCooperativeLevel();
}
UINT STDMETHODCALLTYPE Direct3DDevice9::GetAvailableTextureMem()
{
	return this->mOrig->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EvictManagedResources()
{
	return this->mOrig->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
	return this->mOrig->GetDirect3D(ppD3D9);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
	return this->mOrig->GetDeviceCaps(pCaps);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mOrig->GetDisplayMode(0, pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return this->mOrig->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap)
{
	return this->mOrig->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}
void STDMETHODCALLTYPE Direct3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags)
{
	return this->mOrig->SetCursorPosition(X, Y, Flags);
}
BOOL STDMETHODCALLTYPE Direct3DDevice9::ShowCursor(BOOL bShow)
{
	return this->mOrig->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9::CreateAdditionalSwapChain" << "(" << this << ", " << pPresentationParameters << ", " << ppSwapChain << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	AdjustPresentParameters(pPresentationParameters);

	const HRESULT hr = this->mOrig->CreateAdditionalSwapChain(pPresentationParameters, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		IDirect3DDevice9 *device = this->mOrig;
		IDirect3DSwapChain9 *swapchain = *ppSwapChain;

		assert(swapchain != nullptr);

		D3DPRESENT_PARAMETERS pp;
		swapchain->GetPresentParameters(&pp);

		const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D9Runtime>(device, swapchain);

		if (!runtime->OnCreateInternal(pp))
		{
			LOG(ERROR) << "Failed to initialize Direct3D9 renderer!";
		}

		Direct3DSwapChain9 *const swapchainProxy = new Direct3DSwapChain9(this, swapchain, runtime);

		this->mAdditionalSwapChains.push_back(swapchainProxy);
		*ppSwapChain = swapchainProxy;
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3DDevice9::CreateAdditionalSwapChain' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **ppSwapChain)
{
	if (iSwapChain != 0 || ppSwapChain == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	assert(this->mImplicitSwapChain != nullptr);
			
	this->mImplicitSwapChain->AddRef();

	*ppSwapChain = this->mImplicitSwapChain;

	return D3D_OK;
}
UINT STDMETHODCALLTYPE Direct3DDevice9::GetNumberOfSwapChains()
{
	return 1;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9::Reset" << "(" << this << ", " << pPresentationParameters << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	AdjustPresentParameters(pPresentationParameters);

	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> runtime = this->mImplicitSwapChain->mRuntime;

	runtime->OnDeleteInternal();

	if (this->mAutoDepthStencil != nullptr)
	{
		this->mAutoDepthStencil->Release();
		this->mAutoDepthStencil = nullptr;
	}

	const HRESULT hr = this->mOrig->Reset(pPresentationParameters);

	if (SUCCEEDED(hr))
	{
		D3DPRESENT_PARAMETERS pp;
		this->mImplicitSwapChain->GetPresentParameters(&pp);

		if (!runtime->OnCreateInternal(pp))
		{
			LOG(ERROR) << "Failed to reset Direct3D9 renderer!";
		}

		if (pp.EnableAutoDepthStencil != FALSE)
		{
			this->mOrig->GetDepthStencilSurface(&this->mAutoDepthStencil);
			SetDepthStencilSurface(this->mAutoDepthStencil);
		}
	}
	else
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::Reset' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnPresentInternal();

	return this->mOrig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	assert(this->mImplicitSwapChain != nullptr);

	return this->mImplicitSwapChain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mOrig->GetRasterStatus(0, pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return this->mOrig->SetDialogBoxMode(bEnableDialogs);
}
void STDMETHODCALLTYPE Direct3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		return;
	}

	return this->mOrig->SetGammaRamp(0, Flags, pRamp);
}
void STDMETHODCALLTYPE Direct3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		return;
	}

	return this->mOrig->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint)
{
	return this->mOrig->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture)
{
	return this->mOrig->UpdateTexture(pSourceTexture, pDestinationTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface)
{
	return this->mOrig->GetRenderTargetData(pRenderTarget, pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mOrig->GetFrontBufferData(0, pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
	return this->mOrig->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color)
{
	return this->mOrig->ColorFill(pSurface, pRect, color);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return this->mOrig->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
	return this->mOrig->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
	return this->mOrig->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
	if (pNewZStencil != nullptr)
	{
		assert(this->mImplicitSwapChain != nullptr);
		assert(this->mImplicitSwapChain->mRuntime != nullptr);

		this->mImplicitSwapChain->mRuntime->OnSetDepthStencilSurface(pNewZStencil);

		for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
		{
			assert(swapchain->mRuntime != nullptr);

			swapchain->mRuntime->OnSetDepthStencilSurface(pNewZStencil);
		}
	}

	return this->mOrig->SetDepthStencilSurface(pNewZStencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
	const HRESULT hr = this->mOrig->GetDepthStencilSurface(ppZStencilSurface);

	if (SUCCEEDED(hr))
	{
		assert(this->mImplicitSwapChain != nullptr);
		assert(this->mImplicitSwapChain->mRuntime != nullptr);

		this->mImplicitSwapChain->mRuntime->OnGetDepthStencilSurface(*ppZStencilSurface);

		for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
		{
			assert(swapchain->mRuntime);

			swapchain->mRuntime->OnGetDepthStencilSurface(*ppZStencilSurface);
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginScene()
{
	return this->mOrig->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndScene()
{
	return this->mOrig->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	return this->mOrig->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return this->mOrig->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return this->mOrig->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return this->mOrig->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetViewport(const D3DVIEWPORT9 *pViewport)
{
	return this->mOrig->SetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
	return this->mOrig->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaterial(const D3DMATERIAL9 *pMaterial)
{
	return this->mOrig->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
	return this->mOrig->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetLight(DWORD Index, const D3DLIGHT9 *pLight)
{
	return this->mOrig->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *pLight)
{
	return this->mOrig->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::LightEnable(DWORD Index, BOOL Enable)
{
	return this->mOrig->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return this->mOrig->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipPlane(DWORD Index, const float *pPlane)
{
	return this->mOrig->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipPlane(DWORD Index, float *pPlane)
{
	return this->mOrig->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	return this->mOrig->SetRenderState(State, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
	return this->mOrig->GetRenderState(State, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB)
{
	return this->mOrig->CreateStateBlock(Type, ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginStateBlock()
{
	return this->mOrig->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
	return this->mOrig->EndStateBlock(ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus)
{
	return this->mOrig->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
	return this->mOrig->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
	return this->mOrig->GetTexture(Stage, ppTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
	return this->mOrig->SetTexture(Stage, pTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
	return this->mOrig->GetTextureStageState(Stage, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	return this->mOrig->SetTextureStageState(Stage, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
{
	return this->mOrig->GetSamplerState(Sampler, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return this->mOrig->SetSamplerState(Sampler, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ValidateDevice(DWORD *pNumPasses)
{
	return this->mOrig->ValidateDevice(pNumPasses);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY *pEntries)
{
	return this->mOrig->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return this->mOrig->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return this->mOrig->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
	return this->mOrig->GetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetScissorRect(const RECT *pRect)
{
	return this->mOrig->SetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetScissorRect(RECT *pRect)
{
	return this->mOrig->GetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return this->mOrig->SetSoftwareVertexProcessing(bSoftware);
}
BOOL STDMETHODCALLTYPE Direct3DDevice9::GetSoftwareVertexProcessing()
{
	return this->mOrig->GetSoftwareVertexProcessing();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetNPatchMode(float nSegments)
{
	return this->mOrig->SetNPatchMode(nSegments);
}
float STDMETHODCALLTYPE Direct3DDevice9::GetNPatchMode()
{
	return this->mOrig->GetNPatchMode();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);

	for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
	{
		assert(swapchain->mRuntime != nullptr);

		swapchain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);
	}

	return this->mOrig->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);

	for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
	{
		assert(swapchain->mRuntime != nullptr);

		swapchain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);
	}

	return this->mOrig->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);

	for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
	{
		assert(swapchain->mRuntime != nullptr);

		swapchain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);
	}

	return this->mOrig->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);

	for (Direct3DSwapChain9 *swapchain : this->mAdditionalSwapChains)
	{
		assert(swapchain->mRuntime != nullptr);

		swapchain->mRuntime->OnDrawInternal(PrimitiveType, PrimitiveCount);
	}

	return this->mOrig->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags)
{
	return this->mOrig->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl)
{
	return this->mOrig->CreateVertexDeclaration(pVertexElements, ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
	return this->mOrig->SetVertexDeclaration(pDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
	return this->mOrig->GetVertexDeclaration(ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetFVF(DWORD FVF)
{
	return this->mOrig->SetFVF(FVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFVF(DWORD *pFVF)
{
	return this->mOrig->GetFVF(pFVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader)
{
	return this->mOrig->CreateVertexShader(pFunction, ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
	return this->mOrig->SetVertexShader(pShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
	return this->mOrig->GetVertexShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	return this->mOrig->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return this->mOrig->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	return this->mOrig->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return this->mOrig->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount)
{
	return this->mOrig->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return this->mOrig->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
	return this->mOrig->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride)
{
	return this->mOrig->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
{
	return this->mOrig->SetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT *Divider)
{
	return this->mOrig->GetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
	return this->mOrig->SetIndices(pIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData)
{
	return this->mOrig->GetIndices(ppIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader)
{
	return this->mOrig->CreatePixelShader(pFunction, ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
	return this->mOrig->SetPixelShader(pShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
	return this->mOrig->GetPixelShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	return this->mOrig->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return this->mOrig->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	return this->mOrig->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return this->mOrig->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount)
{
	return this->mOrig->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return this->mOrig->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawRectPatch(UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return this->mOrig->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawTriPatch(UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return this->mOrig->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DeletePatch(UINT Handle)
{
	return this->mOrig->DeletePatch(Handle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery)
{
	return this->mOrig->CreateQuery(Type, ppQuery);
}

// IDirect3DDevice9Ex
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->SetConvolutionMonoKernel(width, height, rows, columns);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	this->mImplicitSwapChain->mRuntime->OnPresentInternal();

	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetGPUThreadPriority(INT *pPriority)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->GetGPUThreadPriority(pPriority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetGPUThreadPriority(INT Priority)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::WaitForVBlank(UINT iSwapChain)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->WaitForVBlank(0);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT32 NumResources)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->CheckResourceResidency(pResourceArray, NumResources);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaximumFrameLatency(UINT MaxLatency)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->GetMaximumFrameLatency(pMaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckDeviceState(HWND hDestinationWindow)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->CheckDeviceState(hDestinationWindow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9Ex::ResetEx" << "(" << this << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	AdjustPresentParameters(pPresentationParameters);

	assert(this->mImplicitSwapChain != nullptr);
	assert(this->mImplicitSwapChain->mRuntime != nullptr);

	const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> runtime = this->mImplicitSwapChain->mRuntime;

	runtime->OnDeleteInternal();

	if (this->mAutoDepthStencil != nullptr)
	{
		this->mAutoDepthStencil->Release();
		this->mAutoDepthStencil = nullptr;
	}

	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(this->mOrig)->ResetEx(pPresentationParameters, pFullscreenDisplayMode);

	if (SUCCEEDED(hr))
	{
		D3DPRESENT_PARAMETERS pp;
		this->mImplicitSwapChain->GetPresentParameters(&pp);

		if (!runtime->OnCreateInternal(pp))
		{
			LOG(ERROR) << "Failed to reset Direct3D9 renderer!";
		}

		if (pp.EnableAutoDepthStencil != FALSE)
		{
			this->mOrig->GetDepthStencilSurface(&this->mAutoDepthStencil);
			SetDepthStencilSurface(this->mAutoDepthStencil);
		}
	}
	else
	{
		LOG(ERROR) << "> 'IDirect3DDevice9Ex::ResetEx' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	if (iSwapChain != 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return static_cast<IDirect3DDevice9Ex *>(this->mOrig)->GetDisplayModeEx(0, pMode, pRotation);
}

// IDirect3D9
HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9::CreateDevice" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	AdjustPresentParameters(pPresentationParameters);

	const HRESULT hr = ReShade::Hooks::Call(&IDirect3D9_CreateDevice)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (SUCCEEDED(hr))
	{
		if (DeviceType != D3DDEVTYPE_NULLREF)
		{
			IDirect3DDevice9 *device = *ppReturnedDeviceInterface;
			IDirect3DSwapChain9 *swapchain = nullptr;
			device->GetSwapChain(0, &swapchain);

			assert(swapchain != nullptr);
		
			D3DPRESENT_PARAMETERS pp;
			swapchain->GetPresentParameters(&pp);

			const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D9Runtime>(device, swapchain);

			if (!runtime->OnCreateInternal(pp))
			{
				LOG(ERROR) << "Failed to initialize Direct3D9 renderer!";
			}

			Direct3DDevice9 *const deviceProxy = new Direct3DDevice9(device);
			Direct3DSwapChain9 *const swapchainProxy = new Direct3DSwapChain9(deviceProxy, swapchain, runtime);

			deviceProxy->mImplicitSwapChain = swapchainProxy;
			*ppReturnedDeviceInterface = deviceProxy;

			if (pp.EnableAutoDepthStencil != FALSE)
			{
				device->GetDepthStencilSurface(&deviceProxy->mAutoDepthStencil);
				deviceProxy->SetDepthStencilSurface(deviceProxy->mAutoDepthStencil);
			}
		}
		else
		{
			LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3D9::CreateDevice' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// IDirect3D9Ex
HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9Ex::CreateDeviceEx" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	AdjustPresentParameters(pPresentationParameters);

	const HRESULT hr = ReShade::Hooks::Call(&IDirect3D9Ex_CreateDeviceEx)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (SUCCEEDED(hr))
	{
		if (DeviceType != D3DDEVTYPE_NULLREF)
		{
			IDirect3DDevice9Ex *device = *ppReturnedDeviceInterface;
			IDirect3DSwapChain9 *swapchain = nullptr;
			device->GetSwapChain(0, &swapchain);

			assert(swapchain != nullptr);

			D3DPRESENT_PARAMETERS pp;
			swapchain->GetPresentParameters(&pp);

			const std::shared_ptr<ReShade::Runtimes::D3D9Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D9Runtime>(device, swapchain);

			if (!runtime->OnCreateInternal(pp))
			{
				LOG(ERROR) << "Failed to initialize Direct3D9 renderer!";
			}

			Direct3DDevice9 *const deviceProxy = new Direct3DDevice9(device);
			Direct3DSwapChain9 *const swapchainProxy = new Direct3DSwapChain9(deviceProxy, swapchain, runtime);

			deviceProxy->mImplicitSwapChain = swapchainProxy;
			*ppReturnedDeviceInterface = deviceProxy;

			if (pp.EnableAutoDepthStencil != FALSE)
			{
				device->GetDepthStencilSurface(&deviceProxy->mAutoDepthStencil);
				deviceProxy->SetDepthStencilSurface(deviceProxy->mAutoDepthStencil);
			}
		}
		else
		{
			LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDirect3D9Ex::CreateDeviceEx' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// PIX
EXPORT int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);

	return 0;
}
EXPORT int WINAPI D3DPERF_EndEvent()
{
	return 0;
}
EXPORT void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
EXPORT void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName)
{
	UNREFERENCED_PARAMETER(col);
	UNREFERENCED_PARAMETER(wszName);
}
EXPORT BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
	return FALSE;
}
EXPORT void WINAPI D3DPERF_SetOptions(DWORD dwOptions)
{
	UNREFERENCED_PARAMETER(dwOptions);

#ifdef _DEBUG
	static const auto trampoline = ReShade::Hooks::Call(&D3DPERF_SetOptions);

	trampoline(0);
#endif
}
EXPORT DWORD WINAPI D3DPERF_GetStatus()
{
	return 0;
}

// D3D9
EXPORT IDirect3D9 *WINAPI Direct3DCreate9(UINT SDKVersion)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9" << "(" << SDKVersion << ")' ...";

	IDirect3D9 *res = ReShade::Hooks::Call(&Direct3DCreate9)(SDKVersion);

	if (res != nullptr)
	{
		ReShade::Hooks::Install(VTABLE(res)[16], reinterpret_cast<ReShade::Hook::Function>(&IDirect3D9_CreateDevice));
	}
	else
	{
		LOG(WARNING) << "> 'Direct3DCreate9' failed!";
	}

	return res;
}
EXPORT HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9Ex" << "(" << SDKVersion << ", " << ppD3D << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&Direct3DCreate9Ex)(SDKVersion, ppD3D);

	if (SUCCEEDED(hr))
	{
		ReShade::Hooks::Install(VTABLE(*ppD3D)[16], reinterpret_cast<ReShade::Hook::Function>(&IDirect3D9_CreateDevice));
		ReShade::Hooks::Install(VTABLE(*ppD3D)[20], reinterpret_cast<ReShade::Hook::Function>(&IDirect3D9Ex_CreateDeviceEx));
	}
	else
	{
		LOG(WARNING) << "> 'Direct3DCreate9Ex' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}