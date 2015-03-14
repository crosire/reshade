#include "Log.hpp"
#include "HookManager.hpp"

#include <d3d9.h>
#include <d3dx9shader.h>
#include <initguid.h>
#include <boost\algorithm\string\replace.hpp>

// -----------------------------------------------------------------------------------------------------

namespace
{
	#pragma region Direct3D8 Types
	namespace D3D8
	{
		typedef D3DCLIPSTATUS9 D3DCLIPSTATUS8;
		typedef D3DCUBEMAP_FACES D3DCUBEMAP_FACES;
		typedef D3DDEVICE_CREATION_PARAMETERS D3DDEVICE_CREATION_PARAMETERS;
		typedef D3DDISPLAYMODE D3DDISPLAYMODE;
		typedef D3DINDEXBUFFER_DESC D3DINDEXBUFFER_DESC;
		typedef D3DLIGHT9 D3DLIGHT8;
		typedef D3DLOCKED_BOX D3DLOCKED_BOX;
		typedef D3DLOCKED_RECT D3DLOCKED_RECT;
		typedef D3DMATERIAL9 D3DMATERIAL8;
		typedef D3DRASTER_STATUS D3DRASTER_STATUS;
		typedef D3DRECTPATCH_INFO D3DRECTPATCH_INFO;
		typedef D3DTRIPATCH_INFO D3DTRIPATCH_INFO;
		typedef D3DVERTEXBUFFER_DESC D3DVERTEXBUFFER_DESC;
		typedef D3DVIEWPORT9 D3DVIEWPORT8;

		struct D3DADAPTER_IDENTIFIER8
		{
			char Driver[MAX_DEVICE_IDENTIFIER_STRING];
			char Description[MAX_DEVICE_IDENTIFIER_STRING];
			LARGE_INTEGER DriverVersion;
			DWORD VendorId, DeviceId, SubSysId;
			DWORD Revision;
			GUID DeviceIdentifier;
			DWORD WHQLLevel;
		};
		struct D3DCAPS8
		{
			D3DDEVTYPE DeviceType;
			UINT AdapterOrdinal;
			DWORD Caps, Caps2, Caps3;
			DWORD PresentationIntervals;
			DWORD CursorCaps;
			DWORD DevCaps;
			DWORD PrimitiveMiscCaps;
			DWORD RasterCaps;
			DWORD ZCmpCaps;
			DWORD SrcBlendCaps;
			DWORD DestBlendCaps;
			DWORD AlphaCmpCaps;
			DWORD ShadeCaps;
			DWORD TextureCaps;
			DWORD TextureFilterCaps;
			DWORD CubeTextureFilterCaps;
			DWORD VolumeTextureFilterCaps;
			DWORD TextureAddressCaps;
			DWORD VolumeTextureAddressCaps;
			DWORD LineCaps;
			DWORD MaxTextureWidth, MaxTextureHeight;
			DWORD MaxVolumeExtent;
			DWORD MaxTextureRepeat;
			DWORD MaxTextureAspectRatio;
			DWORD MaxAnisotropy;
			float MaxVertexW;
			float GuardBandLeft, GuardBandTop, GuardBandRight, GuardBandBottom;
			float ExtentsAdjust;
			DWORD StencilCaps;
			DWORD FVFCaps;
			DWORD TextureOpCaps;
			DWORD MaxTextureBlendStages;
			DWORD MaxSimultaneousTextures;
			DWORD VertexProcessingCaps;
			DWORD MaxActiveLights;
			DWORD MaxUserClipPlanes;
			DWORD MaxVertexBlendMatrices;
			DWORD MaxVertexBlendMatrixIndex;
			float MaxPointSize;
			DWORD MaxPrimitiveCount;
			DWORD MaxVertexIndex;
			DWORD MaxStreams;
			DWORD MaxStreamStride;
			DWORD VertexShaderVersion;
			DWORD MaxVertexShaderConst;
			DWORD PixelShaderVersion;
			float MaxPixelShaderValue;
		};
		struct D3DPRESENT_PARAMETERS
		{
			UINT BackBufferWidth, BackBufferHeight;
			D3DFORMAT BackBufferFormat;
			UINT BackBufferCount;
			D3DMULTISAMPLE_TYPE MultiSampleType;
			D3DSWAPEFFECT SwapEffect;
			HWND hDeviceWindow;
			BOOL Windowed;
			BOOL EnableAutoDepthStencil;
			D3DFORMAT AutoDepthStencilFormat;
			DWORD Flags;
			UINT FullScreen_RefreshRateInHz, FullScreen_PresentationInterval;
		};
		struct D3DSURFACE_DESC
		{
			D3DFORMAT Format;
			D3DRESOURCETYPE Type;
			DWORD Usage;
			D3DPOOL Pool;
			UINT Size;
			D3DMULTISAMPLE_TYPE MultiSampleType;
			UINT Width, Height;
		};
		struct D3DVOLUME_DESC
		{
			D3DFORMAT Format;
			D3DRESOURCETYPE Type;
			DWORD Usage;
			D3DPOOL Pool;
			UINT Size;
			UINT Width, Height, Depth;
		};
	}
	namespace D3D9
	{
		typedef D3DADAPTER_IDENTIFIER9 D3DADAPTER_IDENTIFIER9;
		typedef D3DCAPS9 D3DCAPS9;
		typedef D3DCLIPSTATUS9 D3DCLIPSTATUS9;
		typedef D3DCUBEMAP_FACES D3DCUBEMAP_FACES;
		typedef D3DDEVICE_CREATION_PARAMETERS D3DDEVICE_CREATION_PARAMETERS;
		typedef D3DDISPLAYMODE D3DDISPLAYMODE;
		typedef D3DINDEXBUFFER_DESC D3DINDEXBUFFER_DESC;
		typedef D3DLIGHT9 D3DLIGHT9;
		typedef D3DLOCKED_BOX D3DLOCKED_BOX;
		typedef D3DLOCKED_RECT D3DLOCKED_RECT;
		typedef D3DMATERIAL9 D3DMATERIAL9;
		typedef D3DPRESENT_PARAMETERS D3DPRESENT_PARAMETERS;
		typedef D3DRASTER_STATUS D3DRASTER_STATUS;
		typedef D3DRECTPATCH_INFO D3DRECTPATCH_INFO;
		typedef D3DSURFACE_DESC D3DSURFACE_DESC;
		typedef D3DTRIPATCH_INFO D3DTRIPATCH_INFO;
		typedef D3DVERTEXBUFFER_DESC D3DVERTEXBUFFER_DESC;
		typedef D3DVIEWPORT9 D3DVIEWPORT9;
		typedef D3DVOLUME_DESC D3DVOLUME_DESC;
	}

	DEFINE_GUID(IID_IDirect3D8, 0x1DD9E8DA, 0x1C77, 0x4D40, 0xB0, 0xCF, 0x98, 0xFE, 0xFD, 0xFF, 0x95, 0x12);
	DEFINE_GUID(IID_IDirect3DDevice8, 0x7385E5DF, 0x8FE8, 0x41D5, 0x86, 0xB6, 0xD7, 0xB4, 0x85, 0x47, 0xB6, 0xCF);
	DEFINE_GUID(IID_IDirect3DSwapChain8, 0x928C088B, 0x76B9, 0x4C6B, 0xA5, 0x36, 0xA5, 0x90, 0x85, 0x38, 0x76, 0xCD);
	DEFINE_GUID(IID_IDirect3DResource8, 0x1B36BB7B, 0x9B7, 0x410A, 0xB4, 0x45, 0x7D, 0x14, 0x30, 0xD7, 0xB3, 0x3F);
	DEFINE_GUID(IID_IDirect3DBaseTexture8, 0xB4211CFA, 0x51B9, 0x4A9F, 0xAB, 0x78, 0xDB, 0x99, 0xB2, 0xBB, 0x67, 0x8E);
	DEFINE_GUID(IID_IDirect3DTexture8, 0xE4CDD575, 0x2866, 0x4F01, 0xB1, 0x2E, 0x7E, 0xEC, 0xE1, 0xEC, 0x93, 0x58);
	DEFINE_GUID(IID_IDirect3DVolumeTexture8, 0x4B8AAAFA, 0x140F, 0x42BA, 0x91, 0x31, 0x59, 0x7E, 0xAF, 0xAA, 0x2E, 0xAD);
	DEFINE_GUID(IID_IDirect3DCubeTexture8, 0x3EE5B968, 0x2ACA, 0x4C34, 0x8B, 0xB5, 0x7E, 0x0C, 0x3D, 0x19, 0xB7, 0x50);
	DEFINE_GUID(IID_IDirect3DSurface8, 0xb96EEBCA, 0xB326, 0x4EA5, 0x88, 0x2F, 0x2F, 0xF5, 0xBA, 0xE0, 0x21, 0xDD);
	DEFINE_GUID(IID_IDirect3DVolume8, 0xBD7349F5, 0x14F1, 0x42E4, 0x9C, 0x79, 0x97, 0x23, 0x80, 0xDB, 0x40, 0xC0);
	DEFINE_GUID(IID_IDirect3DVertexBuffer8, 0x8AEEEAC7, 0x05F9, 0x44D4, 0xB5, 0x91, 0x00, 0x0B, 0x0D, 0xF1, 0xCB, 0x95);
	DEFINE_GUID(IID_IDirect3DIndexBuffer8, 0x0E689C9A, 0x053D, 0x44A0, 0x9D, 0x92, 0xDB, 0x0E, 0x3D, 0x75, 0x0F, 0x86);
	#pragma endregion

	struct Direct3D8 : public IUnknown, private boost::noncopyable
	{
		friend struct Direct3DDevice8;

		Direct3D8(HMODULE hModule, IDirect3D9 *proxyD3D) : mModule(hModule), mProxy(proxyD3D)
		{
			assert(hModule != nullptr);
			assert(proxyD3D != nullptr);
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void *pInitializeFunction);
		virtual UINT STDMETHODCALLTYPE GetAdapterCount();
		virtual HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3D8::D3DADAPTER_IDENTIFIER8 *pIdentifier);
		virtual UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter);
		virtual HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter, UINT Mode, D3D8::D3DDISPLAYMODE *pMode);
		virtual HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3D8::D3DDISPLAYMODE *pMode);
		virtual HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed);
		virtual HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
		virtual HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType);
		virtual HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
		virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3D8::D3DCAPS8 *pCaps);
		virtual HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter);
		virtual HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters, Direct3DDevice8 **ppReturnedDeviceInterface);

		HMODULE mModule;
		IDirect3D9 *mProxy;
	};
	struct Direct3DDevice8 : public IUnknown, private boost::noncopyable
	{
		friend struct Direct3DSwapChain8;
		friend struct Direct3DResource8;
		friend struct Direct3DBaseTexture8;
		friend struct Direct3DTexture8;
		friend struct Direct3DVolumeTexture8;
		friend struct Direct3DCubeTexture8;
		friend struct Direct3DSurface8;
		friend struct Direct3DVolume8;
		friend struct Direct3DVertexBuffer8;
		friend struct Direct3DIndexBuffer8;

		Direct3DDevice8(Direct3D8 *d3d, IDirect3DDevice9 *proxyDevice, BOOL ZBufferDiscarding = FALSE) : mRef(1), mD3D(d3d), mProxy(proxyDevice), mBaseVertexIndex(0), mZBufferDiscarding(ZBufferDiscarding), mCurrentVertexShader(0), mCurrentPixelShader(0), mCurrentRenderTarget(nullptr), mCurrentDepthStencil(nullptr)
		{
			assert(d3d != nullptr);
			assert(proxyDevice != nullptr);
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE TestCooperativeLevel();
		virtual UINT STDMETHODCALLTYPE GetAvailableTextureMem();
		virtual HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes);
		virtual HRESULT STDMETHODCALLTYPE GetDirect3D(Direct3D8 **ppD3D8);
		virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3D8::D3DCAPS8 *pCaps);
		virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(D3D8::D3DDISPLAYMODE *pMode);
		virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(D3D8::D3DDEVICE_CREATION_PARAMETERS *pParameters);
		virtual HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, Direct3DSurface8 *pCursorBitmap);
		virtual void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags);
		virtual BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow);
		virtual HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters, Direct3DSwapChain8 **ppSwapChain);
		virtual HRESULT STDMETHODCALLTYPE Reset(D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters);
		virtual HRESULT STDMETHODCALLTYPE Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion);
		virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer);
		virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(D3D8::D3DRASTER_STATUS *pRasterStatus);
		virtual void STDMETHODCALLTYPE SetGammaRamp(DWORD Flags, CONST D3DGAMMARAMP *pRamp);
		virtual void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP *pRamp);
		virtual HRESULT STDMETHODCALLTYPE CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DTexture8 **ppTexture);
		virtual HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DVolumeTexture8 **ppVolumeTexture);
		virtual HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DCubeTexture8 **ppCubeTexture);
		virtual HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, Direct3DVertexBuffer8 **ppVertexBuffer);
		virtual HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DIndexBuffer8 **ppIndexBuffer);
		virtual HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable, Direct3DSurface8 **ppSurface);
		virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, Direct3DSurface8 **ppSurface);
		virtual HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, Direct3DSurface8 **ppSurface);
		virtual HRESULT STDMETHODCALLTYPE CopyRects(Direct3DSurface8 *pSourceSurface, CONST RECT *pSourceRectsArray, UINT cRects, Direct3DSurface8 *pDestinationSurface, CONST POINT *pDestPointsArray);
		virtual HRESULT STDMETHODCALLTYPE UpdateTexture(Direct3DBaseTexture8 *pSourceTexture, Direct3DBaseTexture8 *pDestinationTexture);
		virtual HRESULT STDMETHODCALLTYPE GetFrontBuffer(Direct3DSurface8 *pDestSurface);
		virtual HRESULT STDMETHODCALLTYPE SetRenderTarget(Direct3DSurface8 *pRenderTarget, Direct3DSurface8 *pNewZStencil);
		virtual HRESULT STDMETHODCALLTYPE GetRenderTarget(Direct3DSurface8 **ppRenderTarget);
		virtual HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(Direct3DSurface8 **ppZStencilSurface);
		virtual HRESULT STDMETHODCALLTYPE BeginScene();
		virtual HRESULT STDMETHODCALLTYPE EndScene();
		virtual HRESULT STDMETHODCALLTYPE Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
		virtual HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix);
		virtual HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix);
		virtual HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix);
		virtual HRESULT STDMETHODCALLTYPE SetViewport(CONST D3D8::D3DVIEWPORT8 *pViewport);
		virtual HRESULT STDMETHODCALLTYPE GetViewport(D3D8::D3DVIEWPORT8 *pViewport);
		virtual HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3D8::D3DMATERIAL8 *pMaterial);
		virtual HRESULT STDMETHODCALLTYPE GetMaterial(D3D8::D3DMATERIAL8 *pMaterial);
		virtual HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, CONST D3D8::D3DLIGHT8 *pLight);
		virtual HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3D8::D3DLIGHT8 *pLight);
		virtual HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, BOOL Enable);
		virtual HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index, BOOL *pEnable);
		virtual HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD Index, CONST float *pPlane);
		virtual HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float *pPlane);
		virtual HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
		virtual HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue);
		virtual HRESULT STDMETHODCALLTYPE BeginStateBlock();
		virtual HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD *pToken);
		virtual HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token);
		virtual HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token);
		virtual HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token);
		virtual HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type, DWORD *pToken);
		virtual HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3D8::D3DCLIPSTATUS8 *pClipStatus);
		virtual HRESULT STDMETHODCALLTYPE GetClipStatus(D3D8::D3DCLIPSTATUS8 *pClipStatus);
		virtual HRESULT STDMETHODCALLTYPE GetTexture(DWORD Stage, Direct3DBaseTexture8 **ppTexture);
		virtual HRESULT STDMETHODCALLTYPE SetTexture(DWORD Stage, Direct3DBaseTexture8 *pTexture);
		virtual HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue);
		virtual HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
		virtual HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD *pNumPasses);
		virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID, void *pDevInfoStruct, DWORD DevInfoStructSize);
		virtual HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY *pEntries);
		virtual HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries);
		virtual HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT PaletteNumber);
		virtual HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT *PaletteNumber);
		virtual HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
		virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount);
		virtual HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
		virtual HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
		virtual HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, Direct3DVertexBuffer8 *pDestBuffer, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD *pDeclaration, CONST DWORD *pFunction, DWORD *pHandle, DWORD Usage);
		virtual HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle);
		virtual HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD *pHandle);
		virtual HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle);
		virtual HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount);
		virtual HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount);
		virtual HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD Handle, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 *pStreamData, UINT Stride);
		virtual HRESULT STDMETHODCALLTYPE GetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 **ppStreamData, UINT *pStride);
		virtual HRESULT STDMETHODCALLTYPE SetIndices(Direct3DIndexBuffer8 *pIndexData, UINT BaseVertexIndex);
		virtual HRESULT STDMETHODCALLTYPE GetIndices(Direct3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex);
		virtual HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD *pFunction, DWORD *pHandle);
		virtual HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle);
		virtual HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD *pHandle);
		virtual HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD Handle);
		virtual HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount);
		virtual HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount);
		virtual HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle, CONST float *pNumSegs, CONST D3D8::D3DRECTPATCH_INFO *pRectPatchInfo);
		virtual HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle, CONST float *pNumSegs, CONST D3D8::D3DTRIPATCH_INFO *pTriPatchInfo);
		virtual HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);

		ULONG mRef;
		Direct3D8 *mD3D;
		IDirect3DDevice9 *mProxy;
		INT mBaseVertexIndex;
		const BOOL mZBufferDiscarding;
		DWORD mCurrentVertexShader, mCurrentPixelShader;
		Direct3DSurface8 *mCurrentRenderTarget, *mCurrentDepthStencil;
	};
	struct Direct3DSwapChain8 : public IUnknown, private boost::noncopyable
	{
		friend struct Direct3DDevice8;

		Direct3DSwapChain8(Direct3DDevice8 *device, IDirect3DSwapChain9 *proxySwapChain) : mDevice(device), mProxy(proxySwapChain)
		{
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion);
		virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer);

		Direct3DDevice8 *mDevice;
		IDirect3DSwapChain9 *mProxy;
	};
	struct Direct3DResource8 : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) = 0;
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) = 0;
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) = 0;
		virtual DWORD STDMETHODCALLTYPE GetPriority() = 0;
		virtual void STDMETHODCALLTYPE PreLoad() = 0;
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() = 0;
	};
	struct Direct3DBaseTexture8 : public Direct3DResource8
	{
		virtual DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) = 0;
		virtual DWORD STDMETHODCALLTYPE GetLOD() = 0;
		virtual DWORD STDMETHODCALLTYPE GetLevelCount() = 0;
	};
	struct Direct3DTexture8 : public Direct3DBaseTexture8, private boost::noncopyable
	{
		Direct3DTexture8(Direct3DDevice8 *device, IDirect3DTexture9 *proxyTexture) : mRef(1), mDevice(device), mProxy(proxyTexture)
		{
			assert(device != nullptr);
			assert(proxyTexture != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DTexture8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice) override;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) override;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) override;
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
		virtual DWORD STDMETHODCALLTYPE GetPriority() override;
		virtual void STDMETHODCALLTYPE PreLoad() override;
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;

		virtual DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
		virtual DWORD STDMETHODCALLTYPE GetLOD() override;
		virtual DWORD STDMETHODCALLTYPE GetLevelCount() override;

		virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3D8::D3DSURFACE_DESC *pDesc);
		virtual HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, Direct3DSurface8 **ppSurfaceLevel);
		virtual HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level);
		virtual HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT *pDirtyRect);

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DTexture9 *mProxy;
	};
	struct Direct3DVolumeTexture8 : public Direct3DBaseTexture8, private boost::noncopyable
	{
		Direct3DVolumeTexture8(Direct3DDevice8 *device, IDirect3DVolumeTexture9 *proxyVolumeTexture) : mRef(1), mDevice(device), mProxy(proxyVolumeTexture)
		{
			assert(device != nullptr);
			assert(proxyVolumeTexture != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DVolumeTexture8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice) override;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) override;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) override;
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
		virtual DWORD STDMETHODCALLTYPE GetPriority() override;
		virtual void STDMETHODCALLTYPE PreLoad() override;
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;

		virtual DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
		virtual DWORD STDMETHODCALLTYPE GetLOD() override;
		virtual DWORD STDMETHODCALLTYPE GetLevelCount() override;

		virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3D8::D3DVOLUME_DESC *pDesc);
		virtual HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, Direct3DVolume8 **ppVolumeLevel);
		virtual HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3D8::D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level);
		virtual HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX *pDirtyBox);

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DVolumeTexture9 *mProxy;
	};
	struct Direct3DCubeTexture8 : public Direct3DBaseTexture8, private boost::noncopyable
	{
		Direct3DCubeTexture8(Direct3DDevice8 *device, IDirect3DCubeTexture9 *proxyCubeTexture) : mRef(1), mDevice(device), mProxy(proxyCubeTexture)
		{
			assert(device != nullptr);
			assert(proxyCubeTexture != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DCubeTexture8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice) override;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) override;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) override;
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
		virtual DWORD STDMETHODCALLTYPE GetPriority() override;
		virtual void STDMETHODCALLTYPE PreLoad() override;
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;

		virtual DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
		virtual DWORD STDMETHODCALLTYPE GetLOD() override;
		virtual DWORD STDMETHODCALLTYPE GetLevelCount() override;

		virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3D8::D3DSURFACE_DESC *pDesc);
		virtual HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, Direct3DSurface8 **ppCubeMapSurface);
		virtual HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level);
		virtual HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect);

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DCubeTexture9 *mProxy;
	};
	struct Direct3DSurface8 : public IUnknown, private boost::noncopyable
	{
		Direct3DSurface8(Direct3DDevice8 *device, IDirect3DSurface9 *proxySurface) : mRef(1), mDevice(device), mProxy(proxySurface)
		{
			assert(device != nullptr);
			assert(proxySurface != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DSurface8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
		virtual HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer);
		virtual HRESULT STDMETHODCALLTYPE GetDesc(D3D8::D3DSURFACE_DESC *pDesc);
		virtual HRESULT STDMETHODCALLTYPE LockRect(D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE UnlockRect();

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DSurface9 *mProxy;
	};
	struct Direct3DVolume8 : public IUnknown, private boost::noncopyable
	{
		Direct3DVolume8(Direct3DDevice8 *device, IDirect3DVolume9 *proxyVolume) : mRef(1), mDevice(device), mProxy(proxyVolume)
		{
			assert(device != nullptr);
			assert(proxyVolume != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DVolume8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
		virtual HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer);
		virtual HRESULT STDMETHODCALLTYPE GetDesc(D3D8::D3DVOLUME_DESC *pDesc);
		virtual HRESULT STDMETHODCALLTYPE LockBox(D3D8::D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE UnlockBox();

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DVolume9 *mProxy;
	};
	struct Direct3DVertexBuffer8 : public Direct3DResource8, private boost::noncopyable
	{
		Direct3DVertexBuffer8(Direct3DDevice8 *device, IDirect3DVertexBuffer9 *proxyVertexBuffer) : mRef(1), mDevice(device), mProxy(proxyVertexBuffer)
		{
			assert(device != nullptr);
			assert(proxyVertexBuffer != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DVertexBuffer8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew);
		virtual DWORD STDMETHODCALLTYPE GetPriority();
		virtual void STDMETHODCALLTYPE PreLoad();
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

		virtual HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE Unlock();
		virtual HRESULT STDMETHODCALLTYPE GetDesc(D3D8::D3DVERTEXBUFFER_DESC *pDesc);

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DVertexBuffer9 *mProxy;
	};
	struct Direct3DIndexBuffer8 : public Direct3DResource8, private boost::noncopyable
	{
		Direct3DIndexBuffer8(Direct3DDevice8 *device, IDirect3DIndexBuffer9 *proxyIndexBuffer) : mRef(1), mDevice(device), mProxy(proxyIndexBuffer)
		{
			assert(device != nullptr);
			assert(proxyIndexBuffer != nullptr);

			this->mDevice->mRef++;
		}
		~Direct3DIndexBuffer8()
		{
			this->mDevice->mRef--;
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
		virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
		virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew);
		virtual DWORD STDMETHODCALLTYPE GetPriority();
		virtual void STDMETHODCALLTYPE PreLoad();
		virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

		virtual HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags);
		virtual HRESULT STDMETHODCALLTYPE Unlock();
		virtual HRESULT STDMETHODCALLTYPE GetDesc(D3D8::D3DINDEXBUFFER_DESC *pDesc);

		ULONG mRef;
		Direct3DDevice8 *mDevice;
		IDirect3DIndexBuffer9 *mProxy;
	};
	struct Direct3DVertexShader8
	{
		IDirect3DVertexShader9 *mShader;
		IDirect3DVertexDeclaration9 *mDeclaration;
	};

	std::string GetErrorString(HRESULT hr)
	{
		std::stringstream res;

		switch (hr)
		{
			case E_INVALIDARG:
				res << "E_INVALIDARG";
			case D3DERR_NOTAVAILABLE:
				res << "D3DERR_NOTAVAILABLE";
			case D3DERR_INVALIDCALL:
				res << "D3DERR_INVALIDCALL";
			case D3DERR_INVALIDDEVICE:
				res << "D3DERR_INVALIDDEVICE";
			case D3DERR_DEVICEHUNG:
				res << "D3DERR_DEVICEHUNG";
			case D3DERR_DEVICELOST:
				res << "D3DERR_DEVICELOST";
			case D3DERR_DEVICENOTRESET:
				res << "D3DERR_DEVICENOTRESET";
			case D3DERR_WASSTILLDRAWING:
				res << "D3DERR_WASSTILLDRAWING";
			case D3DXERR_INVALIDDATA:
				res << "D3DXERR_INVALIDDATA";
				break;
			default:
				res << std::showbase << std::hex << hr;
				break;
		}

		return res.str();
	}
	UINT CalculateTextureSize(UINT width, UINT height, UINT depth, D3DFORMAT format)
	{
#define D3DFMT_W11V11U10 65

		switch (static_cast<DWORD>(format))
		{
			default:
			case D3DFMT_UNKNOWN:
				return 0;
			case D3DFMT_R3G3B2:
			case D3DFMT_A8:
			case D3DFMT_P8:
			case D3DFMT_L8:
			case D3DFMT_A4L4:
				return width * height * depth;
			case D3DFMT_R5G6B5:
			case D3DFMT_X1R5G5B5:
			case D3DFMT_A1R5G5B5:
			case D3DFMT_A4R4G4B4:
			case D3DFMT_A8R3G3B2:
			case D3DFMT_X4R4G4B4:
			case D3DFMT_A8P8:
			case D3DFMT_A8L8:
			case D3DFMT_V8U8:
			case D3DFMT_L6V5U5:
			case D3DFMT_D16_LOCKABLE:
			case D3DFMT_D15S1:
			case D3DFMT_D16:
			case D3DFMT_UYVY:
			case D3DFMT_YUY2:
				return width * 2 * height * depth;
			case D3DFMT_R8G8B8:
				return width * 3 * height * depth;
			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
			case D3DFMT_A2B10G10R10:
			case D3DFMT_A8B8G8R8:
			case D3DFMT_X8B8G8R8:
			case D3DFMT_G16R16:
			case D3DFMT_X8L8V8U8:
			case D3DFMT_Q8W8V8U8:
			case D3DFMT_V16U16:
			case D3DFMT_W11V11U10:
			case D3DFMT_A2W10V10U10:
			case D3DFMT_D32:
			case D3DFMT_D24S8:
			case D3DFMT_D24X8:
			case D3DFMT_D24X4S4:
				return width * 4 * height * depth;
			case D3DFMT_DXT1:
				assert(depth <= 1);
				return ((width + 3) >> 2) * ((height + 3) >> 2) * 8;
			case D3DFMT_DXT2:
			case D3DFMT_DXT3:
			case D3DFMT_DXT4:
			case D3DFMT_DXT5:
				assert(depth <= 1);
				return ((width + 3) >> 2) * ((height + 3) >> 2) * 16;
		}
	}

	void ConvertCaps(D3D9::D3DCAPS9 &input, D3D8::D3DCAPS8 &output)
	{
		CopyMemory(&output, &input, sizeof(D3D8::D3DCAPS8));

#define D3DCAPS2_CANRENDERWINDOWED 0x00080000L
#define D3DPRASTERCAPS_ZBIAS 0x00004000L

		output.Caps2 |= D3DCAPS2_CANRENDERWINDOWED;
		output.RasterCaps |= D3DPRASTERCAPS_ZBIAS;
		output.StencilCaps &= ~D3DSTENCILCAPS_TWOSIDED;
		output.VertexShaderVersion = D3DVS_VERSION(1, 1);
		output.PixelShaderVersion = D3DPS_VERSION(1, 4);

		if (output.MaxVertexShaderConst > 256)
		{
			output.MaxVertexShaderConst = 256;
		}
	}
	void ConvertVolumeDesc(D3D9::D3DVOLUME_DESC &input, D3D8::D3DVOLUME_DESC &output)
	{
		output.Format = input.Format;
		output.Type = input.Type;
		output.Usage = input.Usage;
		output.Pool = input.Pool;
		output.Size = CalculateTextureSize(input.Width, input.Height, input.Depth, input.Format);
		output.Width = input.Width;
		output.Height = input.Height;
		output.Depth = input.Depth;
	}
	void ConvertSurfaceDesc(D3D9::D3DSURFACE_DESC &input, D3D8::D3DSURFACE_DESC &output)
	{
		output.Format = input.Format;
		output.Type = input.Type;
		output.Usage = input.Usage;
		output.Pool = input.Pool;
		output.Size = CalculateTextureSize(input.Width, input.Height, 1, input.Format);
		output.MultiSampleType = input.MultiSampleType;
		output.Width = input.Width;
		output.Height = input.Height;

		if (output.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
		{
			output.MultiSampleType = D3DMULTISAMPLE_NONE;
		}
	}
	void ConvertPresentParameters(D3D8::D3DPRESENT_PARAMETERS &input, D3D9::D3DPRESENT_PARAMETERS &output)
	{
		output.BackBufferWidth = input.BackBufferWidth;
		output.BackBufferHeight = input.BackBufferHeight;
		output.BackBufferFormat = input.BackBufferFormat;
		output.BackBufferCount = input.BackBufferCount;
		output.MultiSampleType = input.MultiSampleType;
		output.MultiSampleQuality = 0;
		output.SwapEffect = input.SwapEffect;
		output.hDeviceWindow = input.hDeviceWindow;
		output.Windowed = input.Windowed;
		output.EnableAutoDepthStencil = input.EnableAutoDepthStencil;
		output.AutoDepthStencilFormat = input.AutoDepthStencilFormat;
		output.Flags = input.Flags;
		output.FullScreen_RefreshRateInHz = input.FullScreen_RefreshRateInHz;
		output.PresentationInterval = input.FullScreen_PresentationInterval;

#define D3DSWAPEFFECT_COPY_VSYNC 4
#define D3DPRESENT_RATE_UNLIMITED 0x7FFFFFFF
	
		if (output.SwapEffect == D3DSWAPEFFECT_COPY_VSYNC)
		{
			output.SwapEffect = D3DSWAPEFFECT_COPY;
		}

		if (output.PresentationInterval == D3DPRESENT_RATE_UNLIMITED)
		{
			output.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
	}
	void ConvertAdapterIdentifier(D3D9::D3DADAPTER_IDENTIFIER9 &input, D3D8::D3DADAPTER_IDENTIFIER8 &output)
	{
		CopyMemory(output.Driver, input.Driver, MAX_DEVICE_IDENTIFIER_STRING);
		CopyMemory(output.Description, input.Description, MAX_DEVICE_IDENTIFIER_STRING);
		output.DriverVersion = input.DriverVersion;
		output.VendorId = input.VendorId;
		output.DeviceId = input.DeviceId;
		output.SubSysId = input.SubSysId;
		output.Revision = input.Revision;
		output.DeviceIdentifier = input.DeviceIdentifier;
		output.WHQLLevel = input.WHQLLevel;
	}
}

// -----------------------------------------------------------------------------------------------------

// IDirect3DTexture8
HRESULT STDMETHODCALLTYPE Direct3DTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DResource8 || riid == IID_IDirect3DBaseTexture8 || riid == IID_IDirect3DTexture8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DTexture8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DTexture8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::SetPriority(DWORD PriorityNew)
{
	return this->mProxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetPriority()
{
	return this->mProxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DTexture8::PreLoad()
{
	this->mProxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DTexture8::GetType()
{
	return D3DRTYPE_TEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::SetLOD(DWORD LODNew)
{
	return this->mProxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetLOD()
{
	return this->mProxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetLevelCount()
{
	return this->mProxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetLevelDesc(UINT Level, D3D8::D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DSURFACE_DESC desc;

	const HRESULT hr = this->mProxy->GetLevelDesc(Level, &desc);

	if (FAILED(hr))
	{
		return hr;
	}
	
	ConvertSurfaceDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetSurfaceLevel(UINT Level, Direct3DSurface8 **ppSurfaceLevel)
{
	if (ppSurfaceLevel == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	const HRESULT hr = this->mProxy->GetSurfaceLevel(Level, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSurfaceLevel = new Direct3DSurface8(this->mDevice, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::LockRect(UINT Level, D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return this->mProxy->LockRect(Level, pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::UnlockRect(UINT Level)
{
	return this->mProxy->UnlockRect(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::AddDirtyRect(CONST RECT *pDirtyRect)
{
	return this->mProxy->AddDirtyRect(pDirtyRect);
}

// IDirect3DVolumeTexture8
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DResource8 || riid == IID_IDirect3DBaseTexture8 || riid == IID_IDirect3DVolumeTexture8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVolumeTexture8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVolumeTexture8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::SetPriority(DWORD PriorityNew)
{
	return this->mProxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetPriority()
{
	return this->mProxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DVolumeTexture8::PreLoad()
{
	this->mProxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVolumeTexture8::GetType()
{
	return D3DRTYPE_VOLUMETEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::SetLOD(DWORD LODNew)
{
	return this->mProxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLOD()
{
	return this->mProxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLevelCount()
{
	return this->mProxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLevelDesc(UINT Level, D3D8::D3DVOLUME_DESC *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DVOLUME_DESC desc;

	const HRESULT hr = this->mProxy->GetLevelDesc(Level, &desc);

	if (FAILED(hr))
	{
		return hr;
	}
	
	ConvertVolumeDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetVolumeLevel(UINT Level, Direct3DVolume8 **ppVolumeLevel)
{
	if (ppVolumeLevel == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DVolume9 *volume = nullptr;

	const HRESULT hr = this->mProxy->GetVolumeLevel(Level, &volume);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppVolumeLevel = new Direct3DVolume8(this->mDevice, volume);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::LockBox(UINT Level, D3D8::D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags)
{
	return this->mProxy->LockBox(Level, pLockedVolume, pBox, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::UnlockBox(UINT Level)
{
	return this->mProxy->UnlockBox(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::AddDirtyBox(CONST D3DBOX *pDirtyBox)
{
	return this->mProxy->AddDirtyBox(pDirtyBox);
}

// IDirect3DCubeTexture8
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DResource8 || riid == IID_IDirect3DBaseTexture8 || riid == IID_IDirect3DCubeTexture8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DCubeTexture8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DCubeTexture8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::SetPriority(DWORD PriorityNew)
{
	return this->mProxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetPriority()
{
	return this->mProxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DCubeTexture8::PreLoad()
{
	this->mProxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DCubeTexture8::GetType()
{
	return D3DRTYPE_CUBETEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::SetLOD(DWORD LODNew)
{
	return this->mProxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetLOD()
{
	return this->mProxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetLevelCount()
{
	return this->mProxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetLevelDesc(UINT Level, D3D8::D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DSURFACE_DESC desc;

	const HRESULT hr = this->mProxy->GetLevelDesc(Level, &desc);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertSurfaceDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, Direct3DSurface8 **ppCubeMapSurface)
{
	if (ppCubeMapSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	const HRESULT hr = this->mProxy->GetCubeMapSurface(FaceType, Level, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppCubeMapSurface = new Direct3DSurface8(this->mDevice, surface);;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return this->mProxy->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level)
{
	return this->mProxy->UnlockRect(FaceType, Level);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect)
{
	return this->mProxy->AddDirtyRect(FaceType, pDirtyRect);
}

// IDirect3DSurface8
HRESULT STDMETHODCALLTYPE Direct3DSurface8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DSurface8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSurface8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSurface8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetContainer(REFIID riid, void **ppContainer)
{
	return this->mProxy->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetDesc(D3D8::D3DSURFACE_DESC *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DSURFACE_DESC desc;

	const HRESULT hr = this->mProxy->GetDesc(&desc);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertSurfaceDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::LockRect(D3D8::D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return this->mProxy->LockRect(pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::UnlockRect()
{
	return this->mProxy->UnlockRect();
}

// IDirect3DVolume8
HRESULT STDMETHODCALLTYPE Direct3DVolume8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DVolume8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVolume8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVolume8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetContainer(REFIID riid, void **ppContainer)
{
	return this->mProxy->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetDesc(D3D8::D3DVOLUME_DESC *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DVOLUME_DESC desc;

	const HRESULT hr = this->mProxy->GetDesc(&desc);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertVolumeDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::LockBox(D3D8::D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags)
{
	return this->mProxy->LockBox(pLockedVolume, pBox, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::UnlockBox()
{
	return this->mProxy->UnlockBox();
}

// IDirect3DVertexBuffer8
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DResource8 || riid == IID_IDirect3DVertexBuffer8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVertexBuffer8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVertexBuffer8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DVertexBuffer8::SetPriority(DWORD PriorityNew)
{
	return this->mProxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DVertexBuffer8::GetPriority()
{
	return this->mProxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DVertexBuffer8::PreLoad()
{
	this->mProxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVertexBuffer8::GetType()
{
	return D3DRTYPE_VERTEXBUFFER;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags)
{
	return this->mProxy->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::Unlock()
{
	return this->mProxy->Unlock();
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::GetDesc(D3D8::D3DVERTEXBUFFER_DESC *pDesc)
{
	return this->mProxy->GetDesc(pDesc);
}

// IDirect3DIndexBuffer8
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_IDirect3DResource8 || riid == IID_IDirect3DIndexBuffer8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DIndexBuffer8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DIndexBuffer8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (--this->mRef == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::GetDevice(Direct3DDevice8 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mDevice->AddRef();

	*ppDevice = this->mDevice;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return this->mProxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return this->mProxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::FreePrivateData(REFGUID refguid)
{
	return this->mProxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DIndexBuffer8::SetPriority(DWORD PriorityNew)
{
	return this->mProxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DIndexBuffer8::GetPriority()
{
	return this->mProxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DIndexBuffer8::PreLoad()
{
	this->mProxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DIndexBuffer8::GetType()
{
	return D3DRTYPE_INDEXBUFFER;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags)
{
	return this->mProxy->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::Unlock()
{
	return this->mProxy->Unlock();
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::GetDesc(D3D8::D3DINDEXBUFFER_DESC *pDesc)
{
	return this->mProxy->GetDesc(pDesc);
}

// IDirect3DSwapChain8
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	
	if (riid == IID_IUnknown || riid == IID_IDirect3DSwapChain8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain8::AddRef()
{
	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
	UNREFERENCED_PARAMETER(pDirtyRegion);

	return this->mProxy->Present(pSourceRect, pDestRect, hDestWindowOverride, nullptr, 0);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer)
{
	if (ppBackBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	const HRESULT hr = this->mProxy->GetBackBuffer(iBackBuffer, Type, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppBackBuffer = new Direct3DSurface8(this->mDevice, surface);
		
	return D3D_OK;
}

// IDirect3DDevice8
HRESULT STDMETHODCALLTYPE Direct3DDevice8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	
	if (riid == IID_IUnknown || riid == IID_IDirect3DDevice8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DDevice8::AddRef()
{
	this->mRef++;

	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DDevice8::Release()
{
	if (--this->mRef <= 2)
	{
		if (this->mCurrentRenderTarget != nullptr)
		{
			this->mCurrentRenderTarget->Release();
			this->mCurrentRenderTarget = nullptr;
		}
		if (this->mCurrentDepthStencil != nullptr)
		{
			this->mCurrentDepthStencil->Release();
			this->mCurrentDepthStencil = nullptr;
		}
	}

	const ULONG ref = this->mProxy->Release();

	if (this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DDevice8' object " << this << " (" << ref << ") is inconsistent.";
	}

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::TestCooperativeLevel()
{
	return this->mProxy->TestCooperativeLevel();
}
UINT STDMETHODCALLTYPE Direct3DDevice8::GetAvailableTextureMem()
{
	return this->mProxy->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ResourceManagerDiscardBytes(DWORD Bytes)
{
	UNREFERENCED_PARAMETER(Bytes);

	return this->mProxy->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDirect3D(Direct3D8 **ppD3D8)
{
	if (ppD3D8 == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mD3D->AddRef();

	*ppD3D8 = this->mD3D;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDeviceCaps(D3D8::D3DCAPS8 *pCaps)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::GetDeviceCaps" << "(" << this << ", " << pCaps << ")' ...";

	if (pCaps == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DCAPS9 caps;

	const HRESULT hr = this->mProxy->GetDeviceCaps(&caps);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertCaps(caps, *pCaps);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDisplayMode(D3D8::D3DDISPLAYMODE *pMode)
{
	return this->mProxy->GetDisplayMode(0, pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetCreationParameters(D3D8::D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return this->mProxy->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, Direct3DSurface8 *pCursorBitmap)
{
	if (pCursorBitmap == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap->mProxy);
}
void STDMETHODCALLTYPE Direct3DDevice8::SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags)
{
	this->mProxy->SetCursorPosition(XScreenSpace, YScreenSpace, Flags);
}
BOOL STDMETHODCALLTYPE Direct3DDevice8::ShowCursor(BOOL bShow)
{
	return this->mProxy->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateAdditionalSwapChain(D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters, Direct3DSwapChain8 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateAdditionalSwapChain" << "(" << this << ", " << pPresentationParameters << ", " << ppSwapChain << ")' ...";

	if (pPresentationParameters == nullptr || ppSwapChain == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DPRESENT_PARAMETERS pp;
	IDirect3DSwapChain9 *swapchain = nullptr;

	ConvertPresentParameters(*pPresentationParameters, pp);

	const HRESULT hr = this->mProxy->CreateAdditionalSwapChain(&pp, &swapchain);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSwapChain = new Direct3DSwapChain8(this, swapchain);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Reset(D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::Reset" << "(" << this << ", " << pPresentationParameters << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DPRESENT_PARAMETERS pp;
	ConvertPresentParameters(*pPresentationParameters, pp);

	if (this->mCurrentRenderTarget != nullptr)
	{
		this->mCurrentRenderTarget->Release();
		this->mCurrentRenderTarget = nullptr;
	}
	if (this->mCurrentDepthStencil != nullptr)
	{
		this->mCurrentDepthStencil->Release();
		this->mCurrentDepthStencil = nullptr;
	}

	const HRESULT hr = this->mProxy->Reset(&pp);

	if (FAILED(hr))
	{
		return hr;
	}

	// Set default rendertarget
	IDirect3DSurface9 *rendertarget = nullptr, *depthstencil = nullptr;
	Direct3DSurface8 *rendertargetProxy = nullptr, *depthstencilProxy = nullptr;

	this->mProxy->GetRenderTarget(0, &rendertarget);
	this->mProxy->GetDepthStencilSurface(&depthstencil);

	if (rendertarget != nullptr)
	{
		rendertargetProxy = new Direct3DSurface8(this, rendertarget);

		rendertarget->Release();
	}
	if (depthstencil != nullptr)
	{
		depthstencilProxy = new Direct3DSurface8(this, depthstencil);

		depthstencil->Release();
	}

	SetRenderTarget(rendertargetProxy, depthstencilProxy);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
	UNREFERENCED_PARAMETER(pDirtyRegion);

	return this->mProxy->Present(pSourceRect, pDestRect, hDestWindowOverride, nullptr);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer)
{
	if (ppBackBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	const HRESULT hr = this->mProxy->GetBackBuffer(0, iBackBuffer, Type, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppBackBuffer = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetRasterStatus(D3D8::D3DRASTER_STATUS *pRasterStatus)
{
	return this->mProxy->GetRasterStatus(0, pRasterStatus);
}
void STDMETHODCALLTYPE Direct3DDevice8::SetGammaRamp(DWORD Flags, CONST D3DGAMMARAMP *pRamp)
{
	this->mProxy->SetGammaRamp(0, Flags, pRamp);
}
void STDMETHODCALLTYPE Direct3DDevice8::GetGammaRamp(D3DGAMMARAMP *pRamp)
{
	this->mProxy->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DTexture8 **ppTexture)
{
	if (ppTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (Pool == D3DPOOL_DEFAULT)
	{
		D3DDEVICE_CREATION_PARAMETERS cp;
		this->mProxy->GetCreationParameters(&cp);

		if (SUCCEEDED(this->mD3D->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, Format)) && (Usage & D3DUSAGE_DYNAMIC) == 0)
		{
			Usage |= D3DUSAGE_RENDERTARGET;
		}
		else
		{
			Usage |= D3DUSAGE_DYNAMIC;
		}
	}

	IDirect3DTexture9 *texture = nullptr;

	const HRESULT hr = this->mProxy->CreateTexture(Width, Height, Levels, Usage, Format, Pool, &texture, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppTexture = new Direct3DTexture8(this, texture);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DVolumeTexture8 **ppVolumeTexture)
{
	if (ppVolumeTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DVolumeTexture9 *texture = nullptr;

	const HRESULT hr = this->mProxy->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, &texture, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppVolumeTexture = new Direct3DVolumeTexture8(this, texture);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DCubeTexture8 **ppCubeTexture)
{
	if (ppCubeTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DCubeTexture9 *texture = nullptr;

	const HRESULT hr = this->mProxy->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, &texture, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppCubeTexture = new Direct3DCubeTexture8(this, texture);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, Direct3DVertexBuffer8 **ppVertexBuffer)
{
	if (ppVertexBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DVertexBuffer9 *buffer = nullptr;

	const HRESULT hr = this->mProxy->CreateVertexBuffer(Length, Usage, FVF, Pool, &buffer, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppVertexBuffer = new Direct3DVertexBuffer8(this, buffer);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DIndexBuffer8 **ppIndexBuffer)
{
	if (ppIndexBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DIndexBuffer9 *buffer = nullptr;

	const HRESULT hr = this->mProxy->CreateIndexBuffer(Length, Usage, Format, Pool, &buffer, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppIndexBuffer = new Direct3DIndexBuffer8(this, buffer);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable, Direct3DSurface8 **ppSurface)
{
	if (ppSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	DWORD qualityLevels = 1;
	D3DDEVICE_CREATION_PARAMETERS params;
	this->mProxy->GetCreationParameters(&params);

	HRESULT hr = this->mD3D->mProxy->CheckDeviceMultiSampleType(params.AdapterOrdinal, params.DeviceType, Format, FALSE, MultiSample, &qualityLevels);

	if (FAILED(hr))
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	hr = this->mProxy->CreateRenderTarget(Width, Height, Format, MultiSample, qualityLevels - 1, Lockable, &surface, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSurface = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, Direct3DSurface8 **ppSurface)
{
	if (ppSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	DWORD qualityLevels = 1;
	D3DDEVICE_CREATION_PARAMETERS params;
	this->mProxy->GetCreationParameters(&params);

	HRESULT hr = this->mD3D->mProxy->CheckDeviceMultiSampleType(params.AdapterOrdinal, params.DeviceType, Format, FALSE, MultiSample, &qualityLevels);

	if (FAILED(hr))
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	hr = this->mProxy->CreateDepthStencilSurface(Width, Height, Format, MultiSample, qualityLevels - 1, this->mZBufferDiscarding, &surface, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSurface = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, Direct3DSurface8 **ppSurface)
{
	if (ppSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	const HRESULT hr = this->mProxy->CreateOffscreenPlainSurface(Width, Height, Format, D3DPOOL_SYSTEMMEM, &surface, nullptr);

	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to translate 'IDirect3DDevice8::CreateImageSurface' call for '[" << Width << "x" << Height << ", " << Format << "]'!";

		return hr;
	}

	*ppSurface = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CopyRects(Direct3DSurface8 *pSourceSurface, CONST RECT *pSourceRectsArray, UINT cRects, Direct3DSurface8 *pDestinationSurface, CONST POINT *pDestPointsArray)
{
	if (pSourceSurface == nullptr || pDestinationSurface == nullptr || pSourceSurface == pDestinationSurface)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DSURFACE_DESC descSource, descDestination;
	pSourceSurface->mProxy->GetDesc(&descSource);
	pDestinationSurface->mProxy->GetDesc(&descDestination);

	if (descSource.Format != descDestination.Format)
	{
		return D3DERR_INVALIDCALL;
	}

	HRESULT hr = D3DERR_INVALIDCALL;

	if (cRects == 0)
	{
		cRects = 1;
	}

	for (UINT i = 0; i < cRects; ++i)
	{
		RECT rectSource, rectDestination;

		if (pSourceRectsArray != nullptr)
		{
			rectSource = pSourceRectsArray[i];
		}
		else
		{
			rectSource.left = 0;
			rectSource.right = descSource.Width;
			rectSource.top = 0;
			rectSource.bottom = descSource.Height;
		}

		if (pDestPointsArray != nullptr)
		{
			rectDestination.left = pDestPointsArray[i].x;
			rectDestination.right = rectDestination.left + (rectSource.right - rectSource.left);
			rectDestination.top = pDestPointsArray[i].y;
			rectDestination.bottom = rectDestination.top + (rectSource.bottom - rectSource.top);
		}
		else
		{
			rectDestination = rectSource;
		}

		if (descSource.Pool == D3DPOOL_MANAGED || descDestination.Pool != D3DPOOL_DEFAULT)
		{
			hr = D3DXLoadSurfaceFromSurface(pDestinationSurface->mProxy, nullptr, &rectDestination, pSourceSurface->mProxy, nullptr, &rectSource, D3DX_FILTER_NONE, 0);
		}
		else if (descSource.Pool == D3DPOOL_DEFAULT)
		{
			hr = this->mProxy->StretchRect(pSourceSurface->mProxy, &rectSource, pDestinationSurface->mProxy, &rectDestination, D3DTEXF_NONE);
		}
		else if (descSource.Pool == D3DPOOL_SYSTEMMEM)
		{
			const POINT pt = { rectDestination.left, rectDestination.top };

			hr = this->mProxy->UpdateSurface(pSourceSurface->mProxy, &rectSource, pDestinationSurface->mProxy, &pt);
		}

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to translate 'IDirect3DDevice8::CopyRects' call from '[" << descSource.Width << "x" << descSource.Height << ", " << descSource.Format << ", " << descSource.MultiSampleType << ", " << descSource.Usage << ", " << descSource.Pool << "]' to '[" << descDestination.Width << "x" << descDestination.Height << ", " << descDestination.Format << ", " << descDestination.MultiSampleType << ", " << descDestination.Usage << ", " << descDestination.Pool << "]'!";
			break;
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::UpdateTexture(Direct3DBaseTexture8 *pSourceTexture, Direct3DBaseTexture8 *pDestinationTexture)
{
	if (pSourceTexture == nullptr || pDestinationTexture == nullptr || pSourceTexture->GetType() != pDestinationTexture->GetType())
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DBaseTexture9 *basetextureSource = nullptr, *basetextureDestination = nullptr;

	switch (pSourceTexture->GetType())
	{
		case D3DRTYPE_TEXTURE:
			basetextureSource = static_cast<Direct3DTexture8 *>(pSourceTexture)->mProxy;
			basetextureDestination = static_cast<Direct3DTexture8 *>(pDestinationTexture)->mProxy;
			break;
		case D3DRTYPE_VOLUMETEXTURE:
			basetextureSource = static_cast<Direct3DVolumeTexture8 *>(pSourceTexture)->mProxy;
			basetextureDestination = static_cast<Direct3DVolumeTexture8 *>(pDestinationTexture)->mProxy;
			break;
		case D3DRTYPE_CUBETEXTURE:
			basetextureSource = static_cast<Direct3DCubeTexture8 *>(pSourceTexture)->mProxy;
			basetextureDestination = static_cast<Direct3DCubeTexture8 *>(pDestinationTexture)->mProxy;
			break;
	}

	return this->mProxy->UpdateTexture(basetextureSource, basetextureDestination);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetFrontBuffer(Direct3DSurface8 *pDestSurface)
{
	if (pDestSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->GetFrontBufferData(0, pDestSurface->mProxy);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetRenderTarget(Direct3DSurface8 *pRenderTarget, Direct3DSurface8 *pNewZStencil)
{
	HRESULT hr;

	if (pRenderTarget != nullptr)
	{
		hr = this->mProxy->SetRenderTarget(0, pRenderTarget->mProxy);

		if (FAILED(hr))
		{
			return hr;
		}

		if (this->mCurrentRenderTarget != nullptr)
		{
			this->mCurrentRenderTarget->Release();
		}

		this->mCurrentRenderTarget = pRenderTarget;
		this->mCurrentRenderTarget->AddRef();
	}

	if (pNewZStencil != nullptr)
	{
		hr = this->mProxy->SetDepthStencilSurface(pNewZStencil->mProxy);

		if (FAILED(hr))
		{
			return hr;
		}

		if (this->mCurrentDepthStencil != nullptr)
		{
			this->mCurrentDepthStencil->Release();
		}

		this->mCurrentDepthStencil = pNewZStencil;
		this->mCurrentDepthStencil->AddRef();
	}
	else
	{
		hr = this->mProxy->SetDepthStencilSurface(nullptr);

		if (this->mCurrentDepthStencil != nullptr)
		{
			this->mCurrentDepthStencil->Release();
		}

		this->mCurrentDepthStencil = nullptr;
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetRenderTarget(Direct3DSurface8 **ppRenderTarget)
{
	if (ppRenderTarget == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (this->mCurrentRenderTarget != nullptr)
	{
		this->mCurrentRenderTarget->AddRef();
	}

	*ppRenderTarget = this->mCurrentRenderTarget;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDepthStencilSurface(Direct3DSurface8 **ppZStencilSurface)
{
	if (ppZStencilSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (this->mCurrentDepthStencil != nullptr)
	{
		this->mCurrentDepthStencil->AddRef();
	}

	*ppZStencilSurface = this->mCurrentDepthStencil;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::BeginScene()
{
	return this->mProxy->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::EndScene()
{
	return this->mProxy->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	return this->mProxy->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix)
{
	return this->mProxy->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return this->mProxy->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix)
{
	return this->mProxy->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetViewport(CONST D3D8::D3DVIEWPORT8 *pViewport)
{
	return this->mProxy->SetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetViewport(D3D8::D3DVIEWPORT8 *pViewport)
{
	return this->mProxy->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetMaterial(CONST D3D8::D3DMATERIAL8 *pMaterial)
{
	return this->mProxy->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetMaterial(D3D8::D3DMATERIAL8 *pMaterial)
{
	return this->mProxy->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetLight(DWORD Index, CONST D3D8::D3DLIGHT8 *pLight)
{
	return this->mProxy->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetLight(DWORD Index, D3D8::D3DLIGHT8 *pLight)
{
	return this->mProxy->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::LightEnable(DWORD Index, BOOL Enable)
{
	return this->mProxy->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return this->mProxy->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetClipPlane(DWORD Index, CONST float *pPlane)
{
	return this->mProxy->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetClipPlane(DWORD Index, float *pPlane)
{
	return this->mProxy->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
#define D3DRS_LINEPATTERN 10
#define D3DRS_ZVISIBLE 30
#define D3DRS_EDGEANTIALIAS 40
#define D3DRS_ZBIAS 47
#define D3DRS_SOFTWAREVERTEXPROCESSING 153
#define D3DRS_PATCHSEGMENTS 164

	FLOAT biased;

	switch (static_cast<DWORD>(State))
	{
		case D3DRS_LINEPATTERN:
		case D3DRS_ZVISIBLE:
		case D3DRS_EDGEANTIALIAS:
		case D3DRS_PATCHSEGMENTS:
			return D3DERR_INVALIDCALL;
		case D3DRS_SOFTWAREVERTEXPROCESSING:
			return this->mProxy->SetSoftwareVertexProcessing(Value);
		case D3DRS_ZBIAS:
			biased = static_cast<FLOAT>(Value) * -0.000005f;
			Value = *reinterpret_cast<const DWORD *>(&biased);
		default:
			return this->mProxy->SetRenderState(State, Value);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
	if (pValue == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	HRESULT hr;
	*pValue = 0;

	switch (static_cast<DWORD>(State))
	{
		case D3DRS_LINEPATTERN:
		case D3DRS_ZVISIBLE:
		case D3DRS_EDGEANTIALIAS:
			return D3DERR_INVALIDCALL;
		case D3DRS_ZBIAS:
			hr = this->mProxy->GetRenderState(D3DRS_DEPTHBIAS, pValue);
			*pValue = static_cast<DWORD>(*reinterpret_cast<const FLOAT *>(pValue) * -500000.0f);
			return hr;
		case D3DRS_SOFTWAREVERTEXPROCESSING:
			*pValue = this->mProxy->GetSoftwareVertexProcessing();
			return D3D_OK;
		case D3DRS_PATCHSEGMENTS:
			*pValue = 1;
			return D3D_OK;
		default:
			return this->mProxy->GetRenderState(State, pValue);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::BeginStateBlock()
{
	return this->mProxy->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::EndStateBlock(DWORD *pToken)
{
	if (pToken == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->EndStateBlock(reinterpret_cast<IDirect3DStateBlock9 **>(pToken));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ApplyStateBlock(DWORD Token)
{
	if (Token == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Apply();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CaptureStateBlock(DWORD Token)
{
	if (Token == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	return reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Capture();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeleteStateBlock(DWORD Token)
{
	if (Token == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	reinterpret_cast<IDirect3DStateBlock9 *>(Token)->Release();

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateStateBlock(D3DSTATEBLOCKTYPE Type, DWORD *pToken)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateStateBlock" << "(" << Type << ", " << pToken << ")' ...";

	if (pToken == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->CreateStateBlock(Type, reinterpret_cast<IDirect3DStateBlock9 **>(pToken));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetClipStatus(CONST D3D8::D3DCLIPSTATUS8 *pClipStatus)
{
	return this->mProxy->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetClipStatus(D3D8::D3DCLIPSTATUS8 *pClipStatus)
{
	return this->mProxy->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTexture(DWORD Stage, Direct3DBaseTexture8 **ppTexture)
{
	if (ppTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DBaseTexture9 *basetexture = nullptr;

	const HRESULT hr = this->mProxy->GetTexture(Stage, &basetexture);

	if (FAILED(hr))
	{
		return hr;
	}

	if (basetexture == nullptr)
	{
		*ppTexture = nullptr;
	}
	else
	{
		IDirect3DTexture9 *texture = nullptr;
		IDirect3DVolumeTexture9 *volumetexture = nullptr;
		IDirect3DCubeTexture9 *cubetexture = nullptr;

		switch (basetexture->GetType())
		{
			case D3DRTYPE_TEXTURE:
				basetexture->QueryInterface(__uuidof(IDirect3DTexture9), reinterpret_cast<void **>(&texture));
				*ppTexture = new Direct3DTexture8(this, texture);
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				basetexture->QueryInterface(__uuidof(IDirect3DVolumeTexture9), reinterpret_cast<void **>(&volumetexture));
				*ppTexture = new Direct3DVolumeTexture8(this, volumetexture);
				break;
			case D3DRTYPE_CUBETEXTURE:
				basetexture->QueryInterface(__uuidof(IDirect3DCubeTexture9), reinterpret_cast<void **>(&cubetexture));
				*ppTexture = new Direct3DCubeTexture8(this, cubetexture);
				break;
		}

		basetexture->Release();
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetTexture(DWORD Stage, Direct3DBaseTexture8 *pTexture)
{
	if (pTexture == nullptr)
	{
		return this->mProxy->SetTexture(Stage, nullptr);
	}
	else
	{
		IDirect3DBaseTexture9 *basetexture = nullptr;

		switch (pTexture->GetType())
		{
			case D3DRTYPE_TEXTURE:
				basetexture = static_cast<Direct3DTexture8 *>(pTexture)->mProxy;
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				basetexture = static_cast<Direct3DVolumeTexture8 *>(pTexture)->mProxy;
				break;
			case D3DRTYPE_CUBETEXTURE:
				basetexture = static_cast<Direct3DCubeTexture8 *>(pTexture)->mProxy;
				break;
		}

		return this->mProxy->SetTexture(Stage, basetexture);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
#define D3DTSS_ADDRESSU 13
#define D3DTSS_ADDRESSV 14
#define D3DTSS_ADDRESSW 25
#define D3DTSS_BORDERCOLOR 15
#define D3DTSS_MAGFILTER 16
#define D3DTSS_MINFILTER 17
#define D3DTSS_MIPFILTER 18
#define D3DTSS_MIPMAPLODBIAS 19
#define D3DTSS_MAXMIPLEVEL 20
#define D3DTSS_MAXANISOTROPY 21

	switch (static_cast<DWORD>(Type))
	{
		case D3DTSS_ADDRESSU:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_ADDRESSU, pValue);
		case D3DTSS_ADDRESSV:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_ADDRESSV, pValue);
		case D3DTSS_ADDRESSW:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_ADDRESSW, pValue);
		case D3DTSS_BORDERCOLOR:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_BORDERCOLOR, pValue);
		case D3DTSS_MAGFILTER:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MAGFILTER, pValue);
		case D3DTSS_MINFILTER:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MINFILTER, pValue);
		case D3DTSS_MIPFILTER:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MIPFILTER, pValue);
		case D3DTSS_MIPMAPLODBIAS:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MIPMAPLODBIAS, pValue);
		case D3DTSS_MAXMIPLEVEL:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MAXMIPLEVEL, pValue);
		case D3DTSS_MAXANISOTROPY:
			return this->mProxy->GetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, pValue);
		default:
			return this->mProxy->GetTextureStageState(Stage, Type, pValue);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	switch (static_cast<DWORD>(Type))
	{
		case D3DTSS_ADDRESSU:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
		case D3DTSS_ADDRESSV:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_ADDRESSV, Value);
		case D3DTSS_ADDRESSW:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_ADDRESSW, Value);
		case D3DTSS_BORDERCOLOR:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_BORDERCOLOR, Value);
		case D3DTSS_MAGFILTER:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MAGFILTER, Value);
		case D3DTSS_MINFILTER:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MINFILTER, Value);
		case D3DTSS_MIPFILTER:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MIPFILTER, Value);
		case D3DTSS_MIPMAPLODBIAS:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MIPMAPLODBIAS, Value);
		case D3DTSS_MAXMIPLEVEL:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MAXMIPLEVEL, Value);
		case D3DTSS_MAXANISOTROPY:
			return this->mProxy->SetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, Value);
		default:
			return this->mProxy->SetTextureStageState(Stage, Type, Value);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ValidateDevice(DWORD *pNumPasses)
{
	return this->mProxy->ValidateDevice(pNumPasses);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetInfo(DWORD DevInfoID, void *pDevInfoStruct, DWORD DevInfoStructSize)
{
	UNREFERENCED_PARAMETER(DevInfoID);
	UNREFERENCED_PARAMETER(pDevInfoStruct);
	UNREFERENCED_PARAMETER(DevInfoStructSize);

	LOG(WARNING) << "Redirecting '" << "IDirect3DDevice8::GetInfo" << "(" << this << ", " << DevInfoID << ", " << pDevInfoStruct << ", " << DevInfoStructSize << ")' ...";

	return S_FALSE;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY *pEntries)
{
	return this->mProxy->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return this->mProxy->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return this->mProxy->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetCurrentTexturePalette(UINT *pPaletteNumber)
{
	return this->mProxy->GetCurrentTexturePalette(pPaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	return this->mProxy->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
	return this->mProxy->DrawIndexedPrimitive(PrimitiveType, this->mBaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	return this->mProxy->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	return this->mProxy->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, Direct3DVertexBuffer8 *pDestBuffer, DWORD Flags)
{
	if (pDestBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer->mProxy, nullptr, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateVertexShader(CONST DWORD *pDeclaration, CONST DWORD *pFunction, DWORD *pHandle, DWORD Usage)
{
	UNREFERENCED_PARAMETER(Usage);

	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateVertexShader" << "(" << this << ", " << pDeclaration << ", " << pFunction << ", " << pHandle << ", " << Usage << ")' ...";

	if (pDeclaration == nullptr || pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	UINT i = 0;
	const UINT limit = 32;
	std::string constants;
	WORD stream = 0, offset = 0;
	DWORD inputs[limit];
	D3DVERTEXELEMENT9 elements[limit];

#define D3DVSD_TOKEN_STREAM 1
#define D3DVSD_TOKEN_STREAMDATA 2
#define D3DVSD_TOKEN_TESSELLATOR 3
#define D3DVSD_TOKEN_CONSTMEM 4
#define D3DVSD_TOKENTYPESHIFT 29
#define D3DVSD_TOKENTYPEMASK (0x7 << D3DVSD_TOKENTYPESHIFT)
#define D3DVSD_STREAMNUMBERSHIFT 0
#define D3DVSD_STREAMNUMBERMASK (0xF << D3DVSD_STREAMNUMBERSHIFT)
#define D3DVSD_VERTEXREGSHIFT 0
#define D3DVSD_VERTEXREGMASK (0x1F << D3DVSD_VERTEXREGSHIFT)
#define D3DVSD_VERTEXREGINSHIFT 20
#define D3DVSD_VERTEXREGINMASK (0xF << D3DVSD_VERTEXREGINSHIFT)
#define D3DVSD_DATATYPESHIFT 16
#define D3DVSD_DATATYPEMASK (0xF << D3DVSD_DATATYPESHIFT)
#define D3DVSD_SKIPCOUNTSHIFT 16
#define D3DVSD_SKIPCOUNTMASK (0xF << D3DVSD_SKIPCOUNTSHIFT)
#define D3DVSD_CONSTCOUNTSHIFT 25
#define D3DVSD_CONSTCOUNTMASK (0xF << D3DVSD_CONSTCOUNTSHIFT)
#define D3DVSD_CONSTADDRESSSHIFT 0
#define D3DVSD_CONSTADDRESSMASK (0x7F << D3DVSD_CONSTADDRESSSHIFT)

	LOG(INFO) << "> Translating vertex declaration ...";

	const BYTE sTypes[][2] =
	{
		{ D3DDECLTYPE_FLOAT1, 4 },
		{ D3DDECLTYPE_FLOAT2, 8 },
		{ D3DDECLTYPE_FLOAT3, 12 },
		{ D3DDECLTYPE_FLOAT4, 16 },
		{ D3DDECLTYPE_D3DCOLOR, 4 },
		{ D3DDECLTYPE_UBYTE4, 4 },
		{ D3DDECLTYPE_SHORT2, 4 },
		{ D3DDECLTYPE_SHORT4, 8 },
		{ D3DDECLTYPE_UBYTE4N, 4 },
		{ D3DDECLTYPE_SHORT2N, 4 },
		{ D3DDECLTYPE_SHORT4N, 8 },
		{ D3DDECLTYPE_USHORT2N, 4 },
		{ D3DDECLTYPE_USHORT4N, 8 },
		{ D3DDECLTYPE_UDEC3, 6 },
		{ D3DDECLTYPE_DEC3N, 6 },
		{ D3DDECLTYPE_FLOAT16_2, 8 },
		{ D3DDECLTYPE_FLOAT16_4, 16 }
	};
	const BYTE sAddressUsage[][2] =
	{
		{ D3DDECLUSAGE_POSITION, 0 },
		{ D3DDECLUSAGE_BLENDWEIGHT, 0 },
		{ D3DDECLUSAGE_BLENDINDICES, 0 },
		{ D3DDECLUSAGE_NORMAL, 0 },
		{ D3DDECLUSAGE_PSIZE, 0 },
		{ D3DDECLUSAGE_COLOR, 0 },
		{ D3DDECLUSAGE_COLOR, 1 },
		{ D3DDECLUSAGE_TEXCOORD, 0 },
		{ D3DDECLUSAGE_TEXCOORD, 1 },
		{ D3DDECLUSAGE_TEXCOORD, 2 },
		{ D3DDECLUSAGE_TEXCOORD, 3 },
		{ D3DDECLUSAGE_TEXCOORD, 4 },
		{ D3DDECLUSAGE_TEXCOORD, 5 },
		{ D3DDECLUSAGE_TEXCOORD, 6 },
		{ D3DDECLUSAGE_TEXCOORD, 7 },
		{ D3DDECLUSAGE_POSITION, 1 },
		{ D3DDECLUSAGE_NORMAL, 1 }
	};

	while (i < limit)
	{
		const DWORD token = *pDeclaration;
		const DWORD tokenType = (token & D3DVSD_TOKENTYPEMASK) >> D3DVSD_TOKENTYPESHIFT;

#define D3DVSD_END() 0xFFFFFFFF

		if (token == D3DVSD_END())
		{
			break;
		}
		else if (tokenType == D3DVSD_TOKEN_STREAM)
		{
			stream = static_cast<WORD>((token & D3DVSD_STREAMNUMBERMASK) >> D3DVSD_STREAMNUMBERSHIFT);
			offset = 0;
		}
		else if (tokenType == D3DVSD_TOKEN_STREAMDATA && !(token & 0x10000000))
		{		
			elements[i].Stream = stream;
			elements[i].Offset = offset;
			const DWORD type = (token & D3DVSD_DATATYPEMASK) >> D3DVSD_DATATYPESHIFT;
			elements[i].Type = sTypes[type][0];
			offset += sTypes[type][1];
			elements[i].Method = D3DDECLMETHOD_DEFAULT;
			const DWORD address = (token & D3DVSD_VERTEXREGMASK) >> D3DVSD_VERTEXREGSHIFT;
			elements[i].Usage = sAddressUsage[address][0];
			elements[i].UsageIndex = sAddressUsage[address][1];

			inputs[i++] = address;
		}
		else if (tokenType == D3DVSD_TOKEN_STREAMDATA && (token & 0x10000000))
		{
			offset += ((token & D3DVSD_SKIPCOUNTMASK) >> D3DVSD_SKIPCOUNTSHIFT) * sizeof(DWORD);
		}
		else if (tokenType == D3DVSD_TOKEN_TESSELLATOR && !(token & 0x10000000))
		{
			elements[i].Stream = stream;
			elements[i].Offset = offset;

			const DWORD input = (token & D3DVSD_VERTEXREGINMASK) >> D3DVSD_VERTEXREGINSHIFT;

			for (UINT r = 0; r < i; ++r)
			{
				if (elements[r].Usage == sAddressUsage[input][0] && elements[r].UsageIndex == sAddressUsage[input][1])
				{
					elements[i].Stream = elements[r].Stream;
					elements[i].Offset = elements[r].Offset;
					break;
				}
			}

			elements[i].Type = D3DDECLTYPE_FLOAT3;
			elements[i].Method = D3DDECLMETHOD_CROSSUV;
			const DWORD address = (token & 0xF);
			elements[i].Usage = sAddressUsage[address][0];
			elements[i].UsageIndex = sAddressUsage[address][1];

			inputs[i++] = address;
		}
		else if (tokenType == D3DVSD_TOKEN_TESSELLATOR && (token & 0x10000000))
		{
			elements[i].Stream = 0;
			elements[i].Offset = 0;
			elements[i].Type = D3DDECLTYPE_UNUSED;
			elements[i].Method = D3DDECLMETHOD_UV;
			const DWORD address = (token & 0xF);
			elements[i].Usage = sAddressUsage[address][0];
			elements[i].UsageIndex = sAddressUsage[address][1];

			inputs[i++] = address;
		}
		else if (tokenType == D3DVSD_TOKEN_CONSTMEM)
		{
			const DWORD count = 4 * ((token & D3DVSD_CONSTCOUNTMASK) >> D3DVSD_CONSTCOUNTSHIFT);
			DWORD address = (token & D3DVSD_CONSTADDRESSMASK) >> D3DVSD_CONSTADDRESSSHIFT;

			for (DWORD r = 0; r < count; r += 4, ++address)
			{
				constants += "def c" + std::to_string(address) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 1])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 2])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 3])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 4])) + '\n';
			}

			pDeclaration += count;
		}
		else
		{
			LOG(WARNING) << "> Failed because token type '" << tokenType << "' is not supported!";

			return E_NOTIMPL;
		}
		
		++pDeclaration;
	}

	LOG(TRACE) << "  +----------+---------+---------+--------------+--------------+--------------+-------+";
	LOG(TRACE) << "  | Register | Stream  | Offset  | Type         | Method       | Usage        | Index |";
	LOG(TRACE) << "  +----------+---------+---------+--------------+--------------+--------------+-------+";

	for (UINT k = 0; k < i; ++k)
	{
		LOG(TRACE) << "  | " << "v" << std::left << std::setw(7) << inputs[k] << " | " << std::setw(7) << elements[k].Stream << " | " << std::setw(7) << elements[k].Offset << " | 0x" << std::hex << std::setw(10) << static_cast<UINT>(elements[k].Type) << " | 0x" << std::setw(10) << static_cast<UINT>(elements[k].Method) << " | 0x" << std::setw(10) << static_cast<UINT>(elements[k].Usage) << std::dec << " | " << std::setw(5) << static_cast<UINT>(elements[k].UsageIndex) << std::internal << " |";
	}

	LOG(TRACE) << "  +----------+---------+---------+--------------+--------------+--------------+-------+";

	const D3DVERTEXELEMENT9 terminator = D3DDECL_END();
	elements[i] = terminator;

	HRESULT hr;
	Direct3DVertexShader8 *shader;

	if (pFunction != nullptr)
	{
		LOG(INFO) << "> Disassembling shader and translating assembly to Direct3D 9 compatible code ...";

		if (*pFunction < D3DVS_VERSION(1, 0) || *pFunction > D3DVS_VERSION(1, 1))
		{
			LOG(WARNING) << "> Failed because of version mismatch ('" << *pFunction << "')! Only 'vs_1_x' shaders are supported.";

			return D3DERR_INVALIDCALL;
		}

		ID3DXBuffer *disassembly = nullptr, *assembly = nullptr, *errors = nullptr;

		hr = D3DXDisassembleShader(pFunction, FALSE, nullptr, &disassembly);

		if (FAILED(hr))
		{
			LOG(ERROR) << "> Failed to disassemble shader with '" << GetErrorString(hr) << "'!";

			return hr;
		}

		std::string source(static_cast<const char *>(disassembly->GetBufferPointer()), disassembly->GetBufferSize());
		const std::size_t verpos = source.find("vs_1_");

		assert(verpos != std::string::npos);

		if (source.at(verpos + 5) == '0')
		{
			LOG(INFO) << "> Replacing version 'vs_1_0' with 'vs_1_1' ...";

			source.replace(verpos, 6, "vs_1_1");
		}

		std::size_t declpos = verpos + 7;

		for (UINT k = 0; k < i; ++k)
		{
			std::string decl;

			switch (elements[k].Usage)
			{
				case D3DDECLUSAGE_POSITION:
					decl = "dcl_position";
					break;
				case D3DDECLUSAGE_BLENDWEIGHT:
					decl = "dcl_blendweight";
					break;
				case D3DDECLUSAGE_BLENDINDICES:
					decl = "dcl_blendindices";
					break;
				case D3DDECLUSAGE_NORMAL:
					decl = "dcl_normal";
					break;
				case D3DDECLUSAGE_PSIZE:
					decl = "dcl_psize";
					break;
				case D3DDECLUSAGE_COLOR:
					decl = "dcl_color";
					break;
				case D3DDECLUSAGE_TEXCOORD:
					decl = "dcl_texcoord";
					break;
			}

			if (elements[k].UsageIndex > 0)
			{
				decl += std::to_string(elements[k].UsageIndex);
			}

			decl += " v" + std::to_string(inputs[k]) + '\n';

			source.insert(declpos, decl);
			declpos += decl.length();
		}

		constants += "def c95, 0, 0, 0, 0\n";

		source.insert(declpos, constants);
		source.insert(declpos + constants.size(), "mov r0, c95\nmov r1, c95\nmov r2, c95\nmov r3, c95\nmov r4, c95\nmov r5, c95\nmov r6, c95\nmov r7, c95\nmov r8, c95\nmov r9, c95\nmov r10, c95\nmov r11, c95\n");

		if (source.find("oT") != std::string::npos)
		{
			source.insert(declpos + constants.size(), "mov oT0, c95\nmov oT1, c95\nmov oT2, c95\nmov oT3, c95\nmov oT4, c95\nmov oT5, c95\nmov oT6, c95\nmov oT7, c95\n");
		}

		boost::algorithm::replace_all(source, "oFog.x", "oFog");
		boost::algorithm::replace_all(source, "oPts.x", "oPts");

		LOG(TRACE) << "> Dumping translated shader assembly:\n\n" << source;

		hr = D3DXAssembleShader(source.data(), static_cast<UINT>(source.size()), nullptr, nullptr, 0, &assembly, &errors);

		disassembly->Release();

		if (FAILED(hr))
		{
			if (errors != nullptr)
			{
				LOG(ERROR) << "> Failed to reassemble shader:\n\n" << static_cast<const char *>(errors->GetBufferPointer());

				errors->Release();
			}
			else
			{
				LOG(ERROR) << "> Failed to reassemble shader with '" << GetErrorString(hr) << "'!";
			}

			return hr;
		}

		shader = new Direct3DVertexShader8();

		hr = this->mProxy->CreateVertexShader(static_cast<const DWORD *>(assembly->GetBufferPointer()), &shader->mShader);

		assembly->Release();
	}
	else
	{
		shader = new Direct3DVertexShader8();
		shader->mShader = nullptr;

		hr = D3D_OK;
	}

	if (SUCCEEDED(hr))
	{
		hr = this->mProxy->CreateVertexDeclaration(elements, &shader->mDeclaration);

		if (SUCCEEDED(hr))
		{
			*pHandle = reinterpret_cast<DWORD>(shader) | 0x80000000;
		}
		else
		{
			LOG(ERROR) << "> 'IDirect3DDevice9::CreateVertexDeclaration' failed with '" << GetErrorString(hr) << "'!";

			if (shader->mShader != nullptr)
			{
				shader->mShader->Release();
			}
		}
	}
	else
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::CreateVertexShader' failed with '" << GetErrorString(hr) << "'!";
	}
		
	if (FAILED(hr))
	{
		*pHandle = 0;

		delete shader;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetVertexShader(DWORD Handle)
{
	if (Handle == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	HRESULT hr;
	
	if ((Handle & 0x80000000) == 0)
	{
		this->mProxy->SetVertexShader(nullptr);
		hr = this->mProxy->SetFVF(Handle);

		this->mCurrentVertexShader = 0;
	}
	else
	{
		Direct3DVertexShader8 *const shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000);

		hr = this->mProxy->SetVertexShader(shader->mShader);
		this->mProxy->SetVertexDeclaration(shader->mDeclaration);
	
		this->mCurrentVertexShader = Handle;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShader(DWORD *pHandle)
{
	if (pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (this->mCurrentVertexShader == 0)
	{
		return this->mProxy->GetFVF(pHandle);
	}
	else
	{
		*pHandle = this->mCurrentVertexShader;

		return D3D_OK;
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeleteVertexShader(DWORD Handle)
{
	if ((Handle & 0x80000000) == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	if (this->mCurrentVertexShader == Handle)
	{
		SetVertexShader(0);
	}

	Direct3DVertexShader8 *const shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000);

	if (shader->mShader != nullptr)
	{
		shader->mShader->Release();
	}
	if (shader->mDeclaration != nullptr)
	{
		shader->mDeclaration->Release();
	}

	delete shader;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetVertexShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount)
{
	return this->mProxy->SetVertexShaderConstantF(Register, static_cast<CONST float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount)
{
	return this->mProxy->GetVertexShaderConstantF(Register, static_cast<float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShaderDeclaration(DWORD Handle, void *pData, DWORD *pSizeOfData)
{
	UNREFERENCED_PARAMETER(Handle);
	UNREFERENCED_PARAMETER(pData);
	UNREFERENCED_PARAMETER(pSizeOfData);

	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::GetVertexShaderDeclaration" << "(" << this << ", " << Handle << ", " << pData << ", " << pSizeOfData << ")' ...";
	LOG(ERROR) << "> 'IDirect3DDevice8::GetVertexShaderDeclaration' is not implemented!";

	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::GetVertexShaderFunction" << "(" << this << ", " << Handle << ", " << pData << ", " << pSizeOfData << ")' ...";

	if ((Handle & 0x80000000) == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DVertexShader9 *const shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000)->mShader;

	if (shader == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	LOG(WARNING) << "> Returning translated shader bytecode.";

	return shader->GetFunction(pData, reinterpret_cast<UINT *>(pSizeOfData));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 *pStreamData, UINT Stride)
{
	if (pStreamData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return this->mProxy->SetStreamSource(StreamNumber, pStreamData->mProxy, 0, Stride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 **ppStreamData, UINT *pStride)
{
	if (ppStreamData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	UINT offset;
	IDirect3DVertexBuffer9 *source = nullptr;

	const HRESULT hr = this->mProxy->GetStreamSource(StreamNumber, &source, &offset, pStride);

	if (FAILED(hr))
	{
		return hr;
	}

	if (source == nullptr)
	{
		*ppStreamData = nullptr;
	}
	else
	{
		*ppStreamData = new Direct3DVertexBuffer8(this, source);
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetIndices(Direct3DIndexBuffer8 *pIndexData, UINT BaseVertexIndex)
{
	if (pIndexData == nullptr || BaseVertexIndex > 0x7FFFFFFF)
	{
		return D3DERR_INVALIDCALL;
	}

	this->mBaseVertexIndex = static_cast<INT>(BaseVertexIndex);

	return this->mProxy->SetIndices(pIndexData->mProxy);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetIndices(Direct3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex)
{
	if (ppIndexData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (pBaseVertexIndex != nullptr)
	{
		*pBaseVertexIndex = static_cast<UINT>(this->mBaseVertexIndex);
	}

	IDirect3DIndexBuffer9 *source = nullptr;

	const HRESULT hr = this->mProxy->GetIndices(&source);

	if (FAILED(hr))
	{
		return hr;
	}

	if (source == nullptr)
	{
		*ppIndexData = nullptr;
	}
	else
	{
		*ppIndexData = new Direct3DIndexBuffer8(this, source);
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreatePixelShader(CONST DWORD *pFunction, DWORD *pHandle)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreatePixelShader" << "(" << this << ", " << pFunction << ", " << pHandle << ")' ...";

	if (pFunction == nullptr || pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	LOG(INFO) << "> Disassembling shader and translating assembly to Direct3D 9 compatible code ...";

	if (*pFunction < D3DPS_VERSION(1, 0) || *pFunction > D3DPS_VERSION(1, 4))
	{
		LOG(WARNING) << "> Failed because of version mismatch ('" << *pFunction << "')! Only 'ps_1_x' shaders are supported.";

		return D3DERR_INVALIDCALL;
	}

	std::vector<DWORD> assembly;
	ID3DXBuffer *disassembly = nullptr;

	while (*pFunction != 0x0000FFFF)
	{
		assembly.push_back(*pFunction++);
	}

	assembly.push_back(0x0000FFFF);

	HRESULT hr = D3DXDisassembleShader(assembly.data(), FALSE, nullptr, &disassembly);

	if (FAILED(hr))
	{
		LOG(ERROR) << "> Failed to disassemble shader with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	std::string source(static_cast<const char *>(disassembly->GetBufferPointer()), disassembly->GetBufferSize());
	const std::size_t verpos = source.find("ps_1_");

	assert(verpos != std::string::npos);

	if (source.at(verpos + 5) == '0')
	{
		LOG(INFO) << "> Replacing version 'ps_1_0' with 'ps_1_1' ...";

		assembly[0] = D3DPS_VERSION(1, 1);
		source.replace(verpos, 6, "ps_1_1");
	}

	LOG(TRACE) << "> Dumping translated shader assembly:\n\n" << source;

	disassembly->Release();

	hr = this->mProxy->CreatePixelShader(assembly.data(), reinterpret_cast<IDirect3DPixelShader9 **>(pHandle));

	if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::CreatePixelShader' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetPixelShader(DWORD Handle)
{
	this->mCurrentPixelShader = Handle;

	return this->mProxy->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(Handle));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShader(DWORD *pHandle)
{
	if (pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*pHandle = this->mCurrentPixelShader;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeletePixelShader(DWORD Handle)
{
	if (Handle == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	if (this->mCurrentPixelShader == Handle)
	{
		SetPixelShader(0);
	}

	reinterpret_cast<IDirect3DPixelShader9 *>(Handle)->Release();

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetPixelShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount)
{
	return this->mProxy->SetPixelShaderConstantF(Register, static_cast<CONST float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount)
{
	return this->mProxy->GetPixelShaderConstantF(Register, static_cast<float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::GetPixelShaderFunction" << "(" << this << ", " << Handle << ", " << pData << ", " << pSizeOfData << ")' ...";

	if (Handle == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DPixelShader9 *const shader = reinterpret_cast<IDirect3DPixelShader9 *>(Handle);

	LOG(WARNING) << "> Returning translated shader bytecode.";

	return shader->GetFunction(pData, reinterpret_cast<UINT *>(pSizeOfData));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawRectPatch(UINT Handle, CONST float *pNumSegs, CONST D3D8::D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return this->mProxy->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawTriPatch(UINT Handle, CONST float *pNumSegs, CONST D3D8::D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return this->mProxy->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeletePatch(UINT Handle)
{
	return this->mProxy->DeletePatch(Handle);
}

// IDirect3D8
HRESULT STDMETHODCALLTYPE Direct3D8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	
	if (riid == IID_IUnknown || riid == IID_IDirect3D8)
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mProxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3D8::AddRef()
{
	return this->mProxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3D8::Release()
{
	const ULONG ref = this->mProxy->Release();

	if (ref == 0)
	{
		FreeLibrary(this->mModule);

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3D8::RegisterSoftwareDevice(void *pInitializeFunction)
{
	return this->mProxy->RegisterSoftwareDevice(pInitializeFunction);
}
UINT STDMETHODCALLTYPE Direct3D8::GetAdapterCount()
{
	return this->mProxy->GetAdapterCount();
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3D8::D3DADAPTER_IDENTIFIER8 *pIdentifier)
{
	if (pIdentifier == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DADAPTER_IDENTIFIER9 identifier;

#define D3DENUM_NO_WHQL_LEVEL 0x00000002L

	if ((Flags & D3DENUM_NO_WHQL_LEVEL) == 0)
	{
		Flags |= D3DENUM_WHQL_LEVEL;
	}
	else
	{
		Flags ^= D3DENUM_NO_WHQL_LEVEL;
	}

	const HRESULT hr = this->mProxy->GetAdapterIdentifier(Adapter, Flags, &identifier);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertAdapterIdentifier(identifier, *pIdentifier);

	return D3D_OK;
}
UINT STDMETHODCALLTYPE Direct3D8::GetAdapterModeCount(UINT Adapter)
{
	const D3DFORMAT formats[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A2R10G10B10 };

	UINT count = 0;

	for (const D3DFORMAT format : formats)
	{
		count += this->mProxy->GetAdapterModeCount(Adapter, format);
	}

	return count;
}
HRESULT STDMETHODCALLTYPE Direct3D8::EnumAdapterModes(UINT Adapter, UINT Mode, D3D8::D3DDISPLAYMODE *pMode)
{
	if (pMode == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	const D3DFORMAT formats[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A2R10G10B10 };

	UINT offset = 0;

	for (const D3DFORMAT format : formats)
	{
		const UINT modes = this->mProxy->GetAdapterModeCount(Adapter, format);

		if (modes == 0)
		{
			continue;
		}

		if (Mode < offset + modes)
		{
			return this->mProxy->EnumAdapterModes(Adapter, format, Mode - offset, pMode);
		}

		offset += modes;
	}

	return D3DERR_INVALIDCALL;
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetAdapterDisplayMode(UINT Adapter, D3D8::D3DDISPLAYMODE *pMode)
{
	return this->mProxy->GetAdapterDisplayMode(Adapter, pMode);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceType(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
{
	return this->mProxy->CheckDeviceType(Adapter, CheckType, DisplayFormat, BackBufferFormat, bWindowed);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
	return this->mProxy->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType)
{
	return this->mProxy->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, nullptr);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
{
	return this->mProxy->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3D8::D3DCAPS8 *pCaps)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D8::GetDeviceCaps" << "(" << this << ", " << Adapter << ", " << DeviceType << ", " << pCaps << ")' ...";

	if (pCaps == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DCAPS9 caps;

	const HRESULT hr = this->mProxy->GetDeviceCaps(Adapter, DeviceType, &caps);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertCaps(caps, *pCaps);

	return D3D_OK;
}
HMONITOR STDMETHODCALLTYPE Direct3D8::GetAdapterMonitor(UINT Adapter)
{
	return this->mProxy->GetAdapterMonitor(Adapter);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3D8::D3DPRESENT_PARAMETERS *pPresentationParameters, Direct3DDevice8 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D8::CreateDevice" << "(" << this << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr || ppReturnedDeviceInterface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3D9::D3DPRESENT_PARAMETERS pp;
	IDirect3DDevice9 *device = nullptr;

	ConvertPresentParameters(*pPresentationParameters, pp);

	const HRESULT hr = this->mProxy->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, &device);

	if (FAILED(hr))
	{
		return hr;
	}

	Direct3DDevice8 *const deviceProxy = new Direct3DDevice8(this, device, (pp.Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL) != 0);

	// Set default vertex declaration
	device->SetFVF(D3DFVF_XYZ);

	// Set default rendertarget
	IDirect3DSurface9 *rendertarget = nullptr, *depthstencil = nullptr;
	Direct3DSurface8 *rendertargetProxy = nullptr, *depthstencilProxy = nullptr;

	device->GetRenderTarget(0, &rendertarget);
	device->GetDepthStencilSurface(&depthstencil);

	if (rendertarget != nullptr)
	{
		rendertargetProxy = new Direct3DSurface8(deviceProxy, rendertarget);

		rendertarget->Release();
	}
	if (depthstencil != nullptr)
	{
		depthstencilProxy = new Direct3DSurface8(deviceProxy, depthstencil);

		depthstencil->Release();
	}

	deviceProxy->SetRenderTarget(rendertargetProxy, depthstencilProxy);

	*ppReturnedDeviceInterface = deviceProxy;

	return D3D_OK;
}

// D3D8
EXPORT Direct3D8 *WINAPI Direct3DCreate8(UINT SDKVersion)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate8" << "(" << SDKVersion << ")' ...";
	LOG(INFO) << "> Passing on to 'Direct3DCreate9':";

	const HMODULE module = LoadLibraryA("d3d9.dll");

	if (module == nullptr)
	{
		LOG(ERROR) << "Failed to load Direct3D 9 module!";

		return nullptr;
	}
	
	IDirect3D9 *const res = Direct3DCreate9(D3D_SDK_VERSION);

	if (res == nullptr)
	{
		return nullptr;
	}

	return new Direct3D8(module, res);
}