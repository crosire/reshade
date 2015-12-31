#pragma once

#include <d3d9.h>

#pragma region Direct3D8 Types

#define D3DFMT_W11V11U10 65

#define D3DRS_LINEPATTERN 10
#define D3DRS_ZVISIBLE 30
#define D3DRS_EDGEANTIALIAS 40
#define D3DRS_ZBIAS 47
#define D3DRS_SOFTWAREVERTEXPROCESSING 153
#define D3DRS_PATCHSEGMENTS 164

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
#define D3DVSD_END() 0xFFFFFFFF

#define D3DENUM_NO_WHQL_LEVEL 0x00000002L
#define D3DSWAPEFFECT_COPY_VSYNC 4
#define D3DPRESENT_RATE_UNLIMITED 0x7FFFFFFF
#define D3DCAPS2_CANRENDERWINDOWED 0x00080000L
#define D3DPRASTERCAPS_ZBIAS 0x00004000L

typedef D3DCLIPSTATUS9 D3DCLIPSTATUS8;
typedef D3DLIGHT9 D3DLIGHT8;
typedef D3DMATERIAL9 D3DMATERIAL8;
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
struct D3DPRESENT_PARAMETERS8
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
struct D3DSURFACE_DESC8
{
	D3DFORMAT Format;
	D3DRESOURCETYPE Type;
	DWORD Usage;
	D3DPOOL Pool;
	UINT Size;
	D3DMULTISAMPLE_TYPE MultiSampleType;
	UINT Width, Height;
};
struct D3DVOLUME_DESC8
{
	D3DFORMAT Format;
	D3DRESOURCETYPE Type;
	DWORD Usage;
	D3DPOOL Pool;
	UINT Size;
	UINT Width, Height, Depth;
};

#pragma endregion

class __declspec(uuid("1DD9E8DA-1C77-4D40-B0CF-98FEFDFF9512")) Direct3D8;
class __declspec(uuid("7385E5DF-8FE8-41D5-86B6-D7B48547B6CF")) Direct3DDevice8;
class __declspec(uuid("928C088B-76B9-4C6B-A536-A590853876CD")) Direct3DSwapChain8;
class __declspec(uuid("1B36BB7B-09B7-410A-B445-7D1430D7B33F")) Direct3DResource8;
class __declspec(uuid("B4211CFA-51B9-4A9F-AB78-DB99B2BB678E")) Direct3DBaseTexture8;
class __declspec(uuid("E4CDD575-2866-4F01-B12E-7EECE1EC9358")) Direct3DTexture8;
class __declspec(uuid("3EE5B968-2ACA-4C34-8BB5-7E0C3D19B750")) Direct3DCubeTexture8;
class __declspec(uuid("4B8AAAFA-140F-42BA-9131-597EAFAA2EAD")) Direct3DVolumeTexture8;
class __declspec(uuid("B96EEBCA-B326-4EA5-882F-2FF5BAE021DD")) Direct3DSurface8;
class __declspec(uuid("BD7349F5-14F1-42E4-9C79-972380DB40C0")) Direct3DVolume8;
class __declspec(uuid("8AEEEAC7-05F9-44D4-B591-000B0DF1CB95")) Direct3DVertexBuffer8;
class __declspec(uuid("0E689C9A-053D-44A0-9D92-DB0E3D750F86")) Direct3DIndexBuffer8;

class Direct3D8 : IUnknown
{
	Direct3D8(const Direct3D8 &);
	Direct3D8 &operator=(const Direct3D8 &);

public:
	Direct3D8(HMODULE hModule, IDirect3D9 *proxyInterface) : mModule(hModule), mProxy(proxyInterface)
	{
	}

	IDirect3D9 *GetProxyInterface() const
	{
		return this->mProxy;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void *pInitializeFunction);
	virtual UINT STDMETHODCALLTYPE GetAdapterCount();
	virtual HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8 *pIdentifier);
	virtual UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter);
	virtual HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE *pMode);
	virtual HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode);
	virtual HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed);
	virtual HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
	virtual HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType);
	virtual HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8 *pCaps);
	virtual HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter);
	virtual HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS8 *pPresentationParameters, Direct3DDevice8 **ppReturnedDeviceInterface);

private:
	HMODULE mModule;
	IDirect3D9 *const mProxy;
};
class Direct3DDevice8 : IUnknown
{
	Direct3DDevice8(const Direct3DDevice8 &);
	Direct3DDevice8 &operator=(const Direct3DDevice8 &);

public:
	Direct3DDevice8(Direct3D8 *d3d, IDirect3DDevice9 *proxyInterface, BOOL ZBufferDiscarding = FALSE) : mRef(1), mD3D(d3d), mProxy(proxyInterface), mBaseVertexIndex(0), mZBufferDiscarding(ZBufferDiscarding), mCurrentVertexShader(0), mCurrentPixelShader(0), mCurrentRenderTarget(nullptr), mCurrentDepthStencil(nullptr)
	{
		this->mD3D->AddRef();
	}
	~Direct3DDevice8()
	{
		this->mD3D->Release();
	}

	IDirect3DDevice9 *GetProxyInterface() const
	{
		return this->mProxy;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE TestCooperativeLevel();
	virtual UINT STDMETHODCALLTYPE GetAvailableTextureMem();
	virtual HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes);
	virtual HRESULT STDMETHODCALLTYPE GetDirect3D(Direct3D8 **ppD3D8);
	virtual HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8 *pCaps);
	virtual HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *pMode);
	virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters);
	virtual HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, Direct3DSurface8 *pCursorBitmap);
	virtual void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags);
	virtual BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow);
	virtual HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS8 *pPresentationParameters, Direct3DSwapChain8 **ppSwapChain);
	virtual HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS8 *pPresentationParameters);
	virtual HRESULT STDMETHODCALLTYPE Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion);
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer);
	virtual HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS *pRasterStatus);
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
	virtual HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT8 *pViewport);
	virtual HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8 *pViewport);
	virtual HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL8 *pMaterial);
	virtual HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8 *pMaterial);
	virtual HRESULT STDMETHODCALLTYPE SetLight(DWORD Index, CONST D3DLIGHT8 *pLight);
	virtual HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT8 *pLight);
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
	virtual HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS8 *pClipStatus);
	virtual HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8 *pClipStatus);
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
	virtual HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT Handle, CONST float *pNumSegs, CONST D3DRECTPATCH_INFO *pRectPatchInfo);
	virtual HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT Handle, CONST float *pNumSegs, CONST D3DTRIPATCH_INFO *pTriPatchInfo);
	virtual HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle);

private:
	ULONG mRef;
	Direct3D8 *const mD3D;
	IDirect3DDevice9 *const mProxy;
	INT mBaseVertexIndex;
	const BOOL mZBufferDiscarding;
	DWORD mCurrentVertexShader, mCurrentPixelShader;
	Direct3DSurface8 *mCurrentRenderTarget, *mCurrentDepthStencil;
};
class Direct3DSwapChain8 : IUnknown
{
	Direct3DSwapChain8(const Direct3DSwapChain8 &);
	Direct3DSwapChain8 &operator=(const Direct3DSwapChain8 &);

public:
	Direct3DSwapChain8(Direct3DDevice8 *device, IDirect3DSwapChain9 *proxyInterface) : mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DSwapChain8()
	{
		this->mDevice->Release();
	}

	IDirect3DSwapChain9 *GetProxyInterface() const
	{
		return this->mProxy;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion);
	virtual HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer);

private:
	Direct3DDevice8 *const mDevice;
	IDirect3DSwapChain9 *const mProxy;
};
class Direct3DResource8 : IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData) = 0;
	virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) = 0;
	virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) = 0;
	virtual DWORD STDMETHODCALLTYPE GetPriority() = 0;
	virtual void STDMETHODCALLTYPE PreLoad() = 0;
	virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() = 0;
};
class Direct3DBaseTexture8 : public Direct3DResource8
{
public:
	virtual DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) = 0;
	virtual DWORD STDMETHODCALLTYPE GetLOD() = 0;
	virtual DWORD STDMETHODCALLTYPE GetLevelCount() = 0;
};
class Direct3DTexture8 : public Direct3DBaseTexture8
{
	Direct3DTexture8(const Direct3DTexture8 &);
	Direct3DTexture8 &operator=(const Direct3DTexture8 &);

public:
	Direct3DTexture8(Direct3DDevice8 *device, IDirect3DTexture9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DTexture8()
	{
		this->mDevice->Release();
	}

	IDirect3DTexture9 *GetProxyInterface() const
	{
		return this->mProxy;
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

	virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC8 *pDesc);
	virtual HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, Direct3DSurface8 **ppSurfaceLevel);
	virtual HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level);
	virtual HRESULT STDMETHODCALLTYPE AddDirtyRect(CONST RECT *pDirtyRect);

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DTexture9 *const mProxy;
};
class Direct3DCubeTexture8 : public Direct3DBaseTexture8
{
	Direct3DCubeTexture8(const Direct3DCubeTexture8 &);
	Direct3DCubeTexture8 &operator=(const Direct3DCubeTexture8 &);

public:
	Direct3DCubeTexture8(Direct3DDevice8 *device, IDirect3DCubeTexture9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DCubeTexture8()
	{
		this->mDevice->Release();
	}

	IDirect3DCubeTexture9 *GetProxyInterface() const
	{
		return this->mProxy;
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

	virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC8 *pDesc);
	virtual HRESULT STDMETHODCALLTYPE GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, Direct3DSurface8 **ppCubeMapSurface);
	virtual HRESULT STDMETHODCALLTYPE LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level);
	virtual HRESULT STDMETHODCALLTYPE AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect);

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DCubeTexture9 *const mProxy;
};
class Direct3DVolumeTexture8 : public Direct3DBaseTexture8
{
	Direct3DVolumeTexture8(const Direct3DVolumeTexture8 &);
	Direct3DVolumeTexture8 &operator=(const Direct3DVolumeTexture8 &);

public:
	Direct3DVolumeTexture8(Direct3DDevice8 *device, IDirect3DVolumeTexture9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DVolumeTexture8()
	{
		this->mDevice->Release();
	}

	IDirect3DVolumeTexture9 *GetProxyInterface() const
	{
		return this->mProxy;
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

	virtual HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DVOLUME_DESC8 *pDesc);
	virtual HRESULT STDMETHODCALLTYPE GetVolumeLevel(UINT Level, Direct3DVolume8 **ppVolumeLevel);
	virtual HRESULT STDMETHODCALLTYPE LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE UnlockBox(UINT Level);
	virtual HRESULT STDMETHODCALLTYPE AddDirtyBox(CONST D3DBOX *pDirtyBox);

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DVolumeTexture9 *const mProxy;
};
class Direct3DSurface8 : IUnknown
{
	Direct3DSurface8(const Direct3DSurface8 &);
	Direct3DSurface8 &operator=(const Direct3DSurface8 &);

public:
	Direct3DSurface8(Direct3DDevice8 *device, IDirect3DSurface9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DSurface8()
	{
		this->mDevice->Release();
	}

	IDirect3DSurface9 *GetProxyInterface() const
	{
		return this->mProxy;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
	virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
	virtual HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer);
	virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC8 *pDesc);
	virtual HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE UnlockRect();

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DSurface9 *const mProxy;
};
class Direct3DVolume8 : IUnknown
{
	Direct3DVolume8(const Direct3DVolume8 &);
	Direct3DVolume8 &operator=(const Direct3DVolume8 &);

public:
	Direct3DVolume8(Direct3DDevice8 *device, IDirect3DVolume9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DVolume8()
	{
		this->mDevice->Release();
	}

	IDirect3DVolume9 *GetProxyInterface() const
	{
		return this->mProxy;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
	virtual ULONG STDMETHODCALLTYPE AddRef() override;
	virtual ULONG STDMETHODCALLTYPE Release() override;

	virtual HRESULT STDMETHODCALLTYPE GetDevice(Direct3DDevice8 **ppDevice);
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData);
	virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid);
	virtual HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void **ppContainer);
	virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC8 *pDesc);
	virtual HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE UnlockBox();

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DVolume9 *const mProxy;
};
class Direct3DVertexBuffer8 : public Direct3DResource8
{
	Direct3DVertexBuffer8(const Direct3DVertexBuffer8 &);
	Direct3DVertexBuffer8 &operator=(const Direct3DVertexBuffer8 &);

public:
	Direct3DVertexBuffer8(Direct3DDevice8 *device, IDirect3DVertexBuffer9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DVertexBuffer8()
	{
		this->mDevice->Release();
	}

	IDirect3DVertexBuffer9 *GetProxyInterface() const
	{
		return this->mProxy;
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

	virtual HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE Unlock();
	virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *pDesc);

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DVertexBuffer9 *const mProxy;
};
class Direct3DIndexBuffer8 : public Direct3DResource8
{
	Direct3DIndexBuffer8(const Direct3DIndexBuffer8 &);
	Direct3DIndexBuffer8 &operator=(const Direct3DIndexBuffer8 &);

public:
	Direct3DIndexBuffer8(Direct3DDevice8 *device, IDirect3DIndexBuffer9 *proxyInterface) : mRef(1), mDevice(device), mProxy(proxyInterface)
	{
		this->mDevice->AddRef();
	}
	~Direct3DIndexBuffer8()
	{
		this->mDevice->Release();
	}

	IDirect3DIndexBuffer9 *GetProxyInterface() const
	{
		return this->mProxy;
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

	virtual HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags);
	virtual HRESULT STDMETHODCALLTYPE Unlock();
	virtual HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *pDesc);

private:
	ULONG mRef;
	Direct3DDevice8 *const mDevice;
	IDirect3DIndexBuffer9 *const mProxy;
};

struct Direct3DVertexShader8
{
	IDirect3DVertexShader9 *Shader;
	IDirect3DVertexDeclaration9 *Declaration;
};
