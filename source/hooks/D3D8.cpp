#include "log.hpp"
#include "hooks\D3D8.hpp"

#include <regex>
#include <sstream>
#include <assert.h>
#include <d3dx9shader.h>

// ---------------------------------------------------------------------------------------------------

namespace
{
	std::string GetErrorString(HRESULT hr)
	{
		std::stringstream res;

		switch (hr)
		{
			case E_INVALIDARG:
				res << "E_INVALIDARG";
				break;
			case D3DERR_NOTAVAILABLE:
				res << "D3DERR_NOTAVAILABLE";
				break;
			case D3DERR_INVALIDCALL:
				res << "D3DERR_INVALIDCALL";
				break;
			case D3DERR_INVALIDDEVICE:
				res << "D3DERR_INVALIDDEVICE";
				break;
			case D3DERR_DEVICEHUNG:
				res << "D3DERR_DEVICEHUNG";
				break;
			case D3DERR_DEVICELOST:
				res << "D3DERR_DEVICELOST";
				break;
			case D3DERR_DEVICENOTRESET:
				res << "D3DERR_DEVICENOTRESET";
				break;
			case D3DERR_WASSTILLDRAWING:
				res << "D3DERR_WASSTILLDRAWING";
				break;
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

	void ConvertCaps(D3DCAPS9 &input, D3DCAPS8 &output)
	{
		CopyMemory(&output, &input, sizeof(D3DCAPS8));

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
	void ConvertVolumeDesc(D3DVOLUME_DESC &input, D3DVOLUME_DESC8 &output)
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
	void ConvertSurfaceDesc(D3DSURFACE_DESC &input, D3DSURFACE_DESC8 &output)
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
	void ConvertPresentParameters(D3DPRESENT_PARAMETERS8 &input, D3DPRESENT_PARAMETERS &output)
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

		if (output.SwapEffect == D3DSWAPEFFECT_COPY_VSYNC)
		{
			output.SwapEffect = D3DSWAPEFFECT_COPY;
		}

		if (output.PresentationInterval == D3DPRESENT_RATE_UNLIMITED)
		{
			output.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
	}
	void ConvertAdapterIdentifier(D3DADAPTER_IDENTIFIER9 &input, D3DADAPTER_IDENTIFIER8 &output)
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

// IDirect3DTexture8
HRESULT STDMETHODCALLTYPE Direct3DTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(Direct3DResource8) ||
		riid == __uuidof(Direct3DBaseTexture8))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DTexture8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DTexture8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::SetPriority(DWORD PriorityNew)
{
	return _proxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetPriority()
{
	return _proxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DTexture8::PreLoad()
{
	_proxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DTexture8::GetType()
{
	return D3DRTYPE_TEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::SetLOD(DWORD LODNew)
{
	return _proxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetLOD()
{
	return _proxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DTexture8::GetLevelCount()
{
	return _proxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::GetLevelDesc(UINT Level, D3DSURFACE_DESC8 *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC desc;

	const auto hr = _proxy->GetLevelDesc(Level, &desc);

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

	*ppSurfaceLevel = nullptr;

	IDirect3DSurface9 *surface = nullptr;

	const auto hr = _proxy->GetSurfaceLevel(Level, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSurfaceLevel = new Direct3DSurface8(_device, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return _proxy->LockRect(Level, pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::UnlockRect(UINT Level)
{
	return _proxy->UnlockRect(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DTexture8::AddDirtyRect(CONST RECT *pDirtyRect)
{
	return _proxy->AddDirtyRect(pDirtyRect);
}

// IDirect3DCubeTexture8
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(Direct3DResource8) ||
		riid == __uuidof(Direct3DBaseTexture8))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DCubeTexture8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DCubeTexture8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::SetPriority(DWORD PriorityNew)
{
	return _proxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetPriority()
{
	return _proxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DCubeTexture8::PreLoad()
{
	_proxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DCubeTexture8::GetType()
{
	return D3DRTYPE_CUBETEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::SetLOD(DWORD LODNew)
{
	return _proxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetLOD()
{
	return _proxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DCubeTexture8::GetLevelCount()
{
	return _proxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::GetLevelDesc(UINT Level, D3DSURFACE_DESC8 *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC desc;

	const auto hr = _proxy->GetLevelDesc(Level, &desc);

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

	*ppCubeMapSurface = nullptr;

	IDirect3DSurface9 *surface = nullptr;

	const auto hr = _proxy->GetCubeMapSurface(FaceType, Level, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppCubeMapSurface = new Direct3DSurface8(_device, surface);;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return _proxy->LockRect(FaceType, Level, pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level)
{
	return _proxy->UnlockRect(FaceType, Level);
}
HRESULT STDMETHODCALLTYPE Direct3DCubeTexture8::AddDirtyRect(D3DCUBEMAP_FACES FaceType, CONST RECT *pDirtyRect)
{
	return _proxy->AddDirtyRect(FaceType, pDirtyRect);
}

// IDirect3DVolumeTexture8
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(Direct3DResource8) ||
		riid == __uuidof(Direct3DBaseTexture8))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVolumeTexture8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVolumeTexture8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::SetPriority(DWORD PriorityNew)
{
	return _proxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetPriority()
{
	return _proxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DVolumeTexture8::PreLoad()
{
	_proxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVolumeTexture8::GetType()
{
	return D3DRTYPE_VOLUMETEXTURE;
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::SetLOD(DWORD LODNew)
{
	return _proxy->SetLOD(LODNew);
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLOD()
{
	return _proxy->GetLOD();
}
DWORD STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLevelCount()
{
	return _proxy->GetLevelCount();
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::GetLevelDesc(UINT Level, D3DVOLUME_DESC8 *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DVOLUME_DESC desc;

	const auto hr = _proxy->GetLevelDesc(Level, &desc);

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

	*ppVolumeLevel = nullptr;

	IDirect3DVolume9 *volume = nullptr;

	const auto hr = _proxy->GetVolumeLevel(Level, &volume);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppVolumeLevel = new Direct3DVolume8(_device, volume);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags)
{
	return _proxy->LockBox(Level, pLockedVolume, pBox, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::UnlockBox(UINT Level)
{
	return _proxy->UnlockBox(Level);
}
HRESULT STDMETHODCALLTYPE Direct3DVolumeTexture8::AddDirtyBox(CONST D3DBOX *pDirtyBox)
{
	return _proxy->AddDirtyBox(pDirtyBox);
}

// IDirect3DSurface8
HRESULT STDMETHODCALLTYPE Direct3DSurface8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSurface8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSurface8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetContainer(REFIID riid, void **ppContainer)
{
	return _proxy->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::GetDesc(D3DSURFACE_DESC8 *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DSURFACE_DESC desc;

	const auto hr = _proxy->GetDesc(&desc);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertSurfaceDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::LockRect(D3DLOCKED_RECT *pLockedRect, CONST RECT *pRect, DWORD Flags)
{
	return _proxy->LockRect(pLockedRect, pRect, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DSurface8::UnlockRect()
{
	return _proxy->UnlockRect();
}

// IDirect3DVolume8
HRESULT STDMETHODCALLTYPE Direct3DVolume8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVolume8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVolume8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetContainer(REFIID riid, void **ppContainer)
{
	return _proxy->GetContainer(riid, ppContainer);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::GetDesc(D3DVOLUME_DESC8 *pDesc)
{
	if (pDesc == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DVOLUME_DESC desc;

	const auto hr = _proxy->GetDesc(&desc);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertVolumeDesc(desc, *pDesc);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::LockBox(D3DLOCKED_BOX *pLockedVolume, CONST D3DBOX *pBox, DWORD Flags)
{
	return _proxy->LockBox(pLockedVolume, pBox, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVolume8::UnlockBox()
{
	return _proxy->UnlockBox();
}

// IDirect3DVertexBuffer8
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(Direct3DResource8))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DVertexBuffer8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DVertexBuffer8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DVertexBuffer8::SetPriority(DWORD PriorityNew)
{
	return _proxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DVertexBuffer8::GetPriority()
{
	return _proxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DVertexBuffer8::PreLoad()
{
	_proxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVertexBuffer8::GetType()
{
	return D3DRTYPE_VERTEXBUFFER;
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags)
{
	return _proxy->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::Unlock()
{
	return _proxy->Unlock();
}
HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer8::GetDesc(D3DVERTEXBUFFER_DESC *pDesc)
{
	return _proxy->GetDesc(pDesc);
}

// IDirect3DIndexBuffer8
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(Direct3DResource8))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DIndexBuffer8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DIndexBuffer8::Release()
{
	const auto ref = _proxy->Release();

	if (--_ref == 0)
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

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::SetPrivateData(REFGUID refguid, CONST void *pData, DWORD SizeOfData, DWORD Flags)
{
	return _proxy->SetPrivateData(refguid, pData, SizeOfData, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::GetPrivateData(REFGUID refguid, void *pData, DWORD *pSizeOfData)
{
	return _proxy->GetPrivateData(refguid, pData, pSizeOfData);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::FreePrivateData(REFGUID refguid)
{
	return _proxy->FreePrivateData(refguid);
}
DWORD STDMETHODCALLTYPE Direct3DIndexBuffer8::SetPriority(DWORD PriorityNew)
{
	return _proxy->SetPriority(PriorityNew);
}
DWORD STDMETHODCALLTYPE Direct3DIndexBuffer8::GetPriority()
{
	return _proxy->GetPriority();
}
void STDMETHODCALLTYPE Direct3DIndexBuffer8::PreLoad()
{
	_proxy->PreLoad();
}
D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DIndexBuffer8::GetType()
{
	return D3DRTYPE_INDEXBUFFER;
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData, DWORD Flags)
{
	return _proxy->Lock(OffsetToLock, SizeToLock, reinterpret_cast<void **>(ppbData), Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::Unlock()
{
	return _proxy->Unlock();
}
HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer8::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
	return _proxy->GetDesc(pDesc);
}

// IDirect3DSwapChain8
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain8::AddRef()
{
	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain8::Release()
{
	const auto ref = _proxy->Release();

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
	UNREFERENCED_PARAMETER(pDirtyRegion);

	return _proxy->Present(pSourceRect, pDestRect, hDestWindowOverride, nullptr, 0);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain8::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer)
{
	if (ppBackBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppBackBuffer = nullptr;

	IDirect3DSurface9 *surface = nullptr;

	const auto hr = _proxy->GetBackBuffer(iBackBuffer, Type, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppBackBuffer = new Direct3DSurface8(_device, surface);

	return D3D_OK;
}

// IDirect3DDevice8
HRESULT STDMETHODCALLTYPE Direct3DDevice8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DDevice8::AddRef()
{
	_ref++;

	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DDevice8::Release()
{
	if (--_ref <= 2)
	{
		if (_current_rendertarget != nullptr)
		{
			_current_rendertarget->Release();
			_current_rendertarget = nullptr;
		}
		if (_current_depthstencil != nullptr)
		{
			_current_depthstencil->Release();
			_current_depthstencil = nullptr;
		}
	}

	const auto ref = _proxy->Release();

	if (_ref == 0 && ref != 0)
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
	return _proxy->TestCooperativeLevel();
}
UINT STDMETHODCALLTYPE Direct3DDevice8::GetAvailableTextureMem()
{
	return _proxy->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ResourceManagerDiscardBytes(DWORD Bytes)
{
	UNREFERENCED_PARAMETER(Bytes);

	return _proxy->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDirect3D(Direct3D8 **ppD3D8)
{
	if (ppD3D8 == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	_d3d->AddRef();

	*ppD3D8 = _d3d;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDeviceCaps(D3DCAPS8 *pCaps)
{
	if (pCaps == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DCAPS9 caps;

	const auto hr = _proxy->GetDeviceCaps(&caps);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertCaps(caps, *pCaps);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
	return _proxy->GetDisplayMode(0, pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return _proxy->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, Direct3DSurface8 *pCursorBitmap)
{
	if (pCursorBitmap == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return _proxy->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap->GetProxyInterface());
}
void STDMETHODCALLTYPE Direct3DDevice8::SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags)
{
	_proxy->SetCursorPosition(XScreenSpace, YScreenSpace, Flags);
}
BOOL STDMETHODCALLTYPE Direct3DDevice8::ShowCursor(BOOL bShow)
{
	return _proxy->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS8 *pPresentationParameters, Direct3DSwapChain8 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateAdditionalSwapChain" << "(" << this << ", " << pPresentationParameters << ", " << ppSwapChain << ")' ...";

	if (pPresentationParameters == nullptr || ppSwapChain == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppSwapChain = nullptr;

	D3DPRESENT_PARAMETERS pp;
	IDirect3DSwapChain9 *swapchain = nullptr;

	ConvertPresentParameters(*pPresentationParameters, pp);

	const auto hr = _proxy->CreateAdditionalSwapChain(&pp, &swapchain);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSwapChain = new Direct3DSwapChain8(this, swapchain);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Reset(D3DPRESENT_PARAMETERS8 *pPresentationParameters)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::Reset" << "(" << this << ", " << pPresentationParameters << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DPRESENT_PARAMETERS pp;
	ConvertPresentParameters(*pPresentationParameters, pp);

	if (_current_rendertarget != nullptr)
	{
		_current_rendertarget->Release();
		_current_rendertarget = nullptr;
	}
	if (_current_depthstencil != nullptr)
	{
		_current_depthstencil->Release();
		_current_depthstencil = nullptr;
	}

	const auto hr = _proxy->Reset(&pp);

	if (FAILED(hr))
	{
		return hr;
	}

	// Set default render target
	IDirect3DSurface9 *rendertarget = nullptr, *depthstencil = nullptr;
	Direct3DSurface8 *rendertarget_proxy = nullptr, *depthstencil_proxy = nullptr;

	_proxy->GetRenderTarget(0, &rendertarget);
	_proxy->GetDepthStencilSurface(&depthstencil);

	if (rendertarget != nullptr)
	{
		rendertarget_proxy = new Direct3DSurface8(this, rendertarget);

		rendertarget->Release();
	}
	if (depthstencil != nullptr)
	{
		depthstencil_proxy = new Direct3DSurface8(this, depthstencil);

		depthstencil->Release();
	}

	SetRenderTarget(rendertarget_proxy, depthstencil_proxy);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
	UNREFERENCED_PARAMETER(pDirtyRegion);

	return _proxy->Present(pSourceRect, pDestRect, hDestWindowOverride, nullptr);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, Direct3DSurface8 **ppBackBuffer)
{
	if (ppBackBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppBackBuffer = nullptr;

	IDirect3DSurface9 *surface = nullptr;

	const auto hr = _proxy->GetBackBuffer(0, iBackBuffer, Type, &surface);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppBackBuffer = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
	return _proxy->GetRasterStatus(0, pRasterStatus);
}
void STDMETHODCALLTYPE Direct3DDevice8::SetGammaRamp(DWORD Flags, CONST D3DGAMMARAMP *pRamp)
{
	_proxy->SetGammaRamp(0, Flags, pRamp);
}
void STDMETHODCALLTYPE Direct3DDevice8::GetGammaRamp(D3DGAMMARAMP *pRamp)
{
	_proxy->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, Direct3DTexture8 **ppTexture)
{
	if (ppTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppTexture = nullptr;

	if (Pool == D3DPOOL_DEFAULT)
	{
		D3DDEVICE_CREATION_PARAMETERS cp;
		_proxy->GetCreationParameters(&cp);

		if (SUCCEEDED(_d3d->GetProxyInterface()->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, Format)) && (Usage & D3DUSAGE_DYNAMIC) == 0)
		{
			Usage |= D3DUSAGE_RENDERTARGET;
		}
		else
		{
			Usage |= D3DUSAGE_DYNAMIC;
		}
	}

	IDirect3DTexture9 *texture = nullptr;

	const auto hr = _proxy->CreateTexture(Width, Height, Levels, Usage, Format, Pool, &texture, nullptr);

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

	*ppVolumeTexture = nullptr;

	IDirect3DVolumeTexture9 *texture = nullptr;

	const auto hr = _proxy->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, &texture, nullptr);

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

	*ppCubeTexture = nullptr;

	IDirect3DCubeTexture9 *texture = nullptr;

	const auto hr = _proxy->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, &texture, nullptr);

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

	*ppVertexBuffer = nullptr;

	if (Pool == D3DPOOL_MANAGED)
	{
		Pool = D3DPOOL_DEFAULT;
		Usage |= D3DUSAGE_DYNAMIC;
	}

	IDirect3DVertexBuffer9 *buffer = nullptr;

	const auto hr = _proxy->CreateVertexBuffer(Length, Usage, FVF, Pool, &buffer, nullptr);

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

	*ppIndexBuffer = nullptr;

	IDirect3DIndexBuffer9 *buffer = nullptr;

	const auto hr = _proxy->CreateIndexBuffer(Length, Usage, Format, Pool, &buffer, nullptr);

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

	*ppSurface = nullptr;

	DWORD quality_levels = 1;
	D3DDEVICE_CREATION_PARAMETERS params;
	_proxy->GetCreationParameters(&params);

	HRESULT hr = _d3d->GetProxyInterface()->CheckDeviceMultiSampleType(params.AdapterOrdinal, params.DeviceType, Format, FALSE, MultiSample, &quality_levels);

	if (FAILED(hr))
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	hr = _proxy->CreateRenderTarget(Width, Height, Format, MultiSample, quality_levels - 1, Lockable, &surface, nullptr);

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

	*ppSurface = nullptr;

	DWORD quality_levels = 1;
	D3DDEVICE_CREATION_PARAMETERS params;
	_proxy->GetCreationParameters(&params);

	HRESULT hr = _d3d->GetProxyInterface()->CheckDeviceMultiSampleType(params.AdapterOrdinal, params.DeviceType, Format, FALSE, MultiSample, &quality_levels);

	if (FAILED(hr))
	{
		return D3DERR_INVALIDCALL;
	}

	IDirect3DSurface9 *surface = nullptr;

	hr = _proxy->CreateDepthStencilSurface(Width, Height, Format, MultiSample, quality_levels - 1, _zbuffer_discarding, &surface, nullptr);

	if (FAILED(hr))
	{
		return hr;
	}

	*ppSurface = new Direct3DSurface8(this, surface);

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format, Direct3DSurface8 **ppSurface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateImageSurface" << "(" << this << ", " << Width << ", " << Height << ", " << Format << ", " << ppSurface << ")' ...";

	if (ppSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppSurface = nullptr;

	if (Format == D3DFMT_R8G8B8)
	{
		LOG(WARNING) << "> Replacing format 'D3DFMT_R8G8B8' with 'D3DFMT_X8R8G8B8' ...";

		Format = D3DFMT_X8R8G8B8;
	}

	IDirect3DSurface9 *surface = nullptr;

	const auto hr = _proxy->CreateOffscreenPlainSurface(Width, Height, Format, D3DPOOL_SYSTEMMEM, &surface, nullptr);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3DDevice8::CreateImageSurface' failed with '" << GetErrorString(hr) << "'!";

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

	D3DSURFACE_DESC desc_source, desc_destination;
	pSourceSurface->GetProxyInterface()->GetDesc(&desc_source);
	pDestinationSurface->GetProxyInterface()->GetDesc(&desc_destination);

	if (desc_source.Format != desc_destination.Format)
	{
		return D3DERR_INVALIDCALL;
	}

	HRESULT hr = D3DERR_INVALIDCALL;

	if (cRects == 0)
	{
		cRects = 1;
	}

	for (UINT i = 0; i < cRects; i++)
	{
		RECT rect_source, rect_destination;

		if (pSourceRectsArray != nullptr)
		{
			rect_source = pSourceRectsArray[i];
		}
		else
		{
			rect_source.left = 0;
			rect_source.right = desc_source.Width;
			rect_source.top = 0;
			rect_source.bottom = desc_source.Height;
		}

		if (pDestPointsArray != nullptr)
		{
			rect_destination.left = pDestPointsArray[i].x;
			rect_destination.right = rect_destination.left + (rect_source.right - rect_source.left);
			rect_destination.top = pDestPointsArray[i].y;
			rect_destination.bottom = rect_destination.top + (rect_source.bottom - rect_source.top);
		}
		else
		{
			rect_destination = rect_source;
		}

		if (desc_source.Pool == D3DPOOL_MANAGED || desc_destination.Pool != D3DPOOL_DEFAULT)
		{
			hr = D3DXLoadSurfaceFromSurface(pDestinationSurface->GetProxyInterface(), nullptr, &rect_destination, pSourceSurface->GetProxyInterface(), nullptr, &rect_source, D3DX_FILTER_NONE, 0);
		}
		else if (desc_source.Pool == D3DPOOL_DEFAULT)
		{
			hr = _proxy->StretchRect(pSourceSurface->GetProxyInterface(), &rect_source, pDestinationSurface->GetProxyInterface(), &rect_destination, D3DTEXF_NONE);
		}
		else if (desc_source.Pool == D3DPOOL_SYSTEMMEM)
		{
			const POINT pt = { rect_destination.left, rect_destination.top };

			hr = _proxy->UpdateSurface(pSourceSurface->GetProxyInterface(), &rect_source, pDestinationSurface->GetProxyInterface(), &pt);
		}

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to translate 'IDirect3DDevice8::CopyRects' call from '[" << desc_source.Width << "x" << desc_source.Height << ", " << desc_source.Format << ", " << desc_source.MultiSampleType << ", " << desc_source.Usage << ", " << desc_source.Pool << "]' to '[" << desc_destination.Width << "x" << desc_destination.Height << ", " << desc_destination.Format << ", " << desc_destination.MultiSampleType << ", " << desc_destination.Usage << ", " << desc_destination.Pool << "]'!";
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

	IDirect3DBaseTexture9 *basetexture_source, *basetexture_destination;

	switch (pSourceTexture->GetType())
	{
		case D3DRTYPE_TEXTURE:
			basetexture_source = static_cast<Direct3DTexture8 *>(pSourceTexture)->GetProxyInterface();
			basetexture_destination = static_cast<Direct3DTexture8 *>(pDestinationTexture)->GetProxyInterface();
			break;
		case D3DRTYPE_VOLUMETEXTURE:
			basetexture_source = static_cast<Direct3DVolumeTexture8 *>(pSourceTexture)->GetProxyInterface();
			basetexture_destination = static_cast<Direct3DVolumeTexture8 *>(pDestinationTexture)->GetProxyInterface();
			break;
		case D3DRTYPE_CUBETEXTURE:
			basetexture_source = static_cast<Direct3DCubeTexture8 *>(pSourceTexture)->GetProxyInterface();
			basetexture_destination = static_cast<Direct3DCubeTexture8 *>(pDestinationTexture)->GetProxyInterface();
			break;
		default:
			return D3DERR_INVALIDCALL;
	}

	return _proxy->UpdateTexture(basetexture_source, basetexture_destination);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetFrontBuffer(Direct3DSurface8 *pDestSurface)
{
	if (pDestSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return _proxy->GetFrontBufferData(0, pDestSurface->GetProxyInterface());
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetRenderTarget(Direct3DSurface8 *pRenderTarget, Direct3DSurface8 *pNewZStencil)
{
	HRESULT hr;

	if (pRenderTarget != nullptr)
	{
		hr = _proxy->SetRenderTarget(0, pRenderTarget->GetProxyInterface());

		if (FAILED(hr))
		{
			return hr;
		}

		if (_current_rendertarget != nullptr)
		{
			_current_rendertarget->Release();
		}

		_current_rendertarget = pRenderTarget;
		_current_rendertarget->AddRef();
	}

	if (pNewZStencil != nullptr)
	{
		hr = _proxy->SetDepthStencilSurface(pNewZStencil->GetProxyInterface());

		if (FAILED(hr))
		{
			return hr;
		}

		if (_current_depthstencil != nullptr)
		{
			_current_depthstencil->Release();
		}

		_current_depthstencil = pNewZStencil;
		_current_depthstencil->AddRef();
	}
	else
	{
		_proxy->SetDepthStencilSurface(nullptr);

		if (_current_depthstencil != nullptr)
		{
			_current_depthstencil->Release();
		}

		_current_depthstencil = nullptr;
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetRenderTarget(Direct3DSurface8 **ppRenderTarget)
{
	if (ppRenderTarget == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (_current_rendertarget != nullptr)
	{
		_current_rendertarget->AddRef();
	}

	*ppRenderTarget = _current_rendertarget;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetDepthStencilSurface(Direct3DSurface8 **ppZStencilSurface)
{
	if (ppZStencilSurface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (_current_depthstencil != nullptr)
	{
		_current_depthstencil->AddRef();
	}

	*ppZStencilSurface = _current_depthstencil;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::BeginScene()
{
	return _proxy->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::EndScene()
{
	return _proxy->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	return _proxy->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix)
{
	return _proxy->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return _proxy->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix)
{
	return _proxy->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetViewport(CONST D3DVIEWPORT8 *pViewport)
{
	return _proxy->SetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetViewport(D3DVIEWPORT8 *pViewport)
{
	return _proxy->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetMaterial(CONST D3DMATERIAL8 *pMaterial)
{
	return _proxy->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetMaterial(D3DMATERIAL8 *pMaterial)
{
	return _proxy->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetLight(DWORD Index, CONST D3DLIGHT8 *pLight)
{
	return _proxy->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetLight(DWORD Index, D3DLIGHT8 *pLight)
{
	return _proxy->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::LightEnable(DWORD Index, BOOL Enable)
{
	return _proxy->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return _proxy->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetClipPlane(DWORD Index, CONST float *pPlane)
{
	return _proxy->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetClipPlane(DWORD Index, float *pPlane)
{
	return _proxy->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	FLOAT biased;

	switch (static_cast<DWORD>(State))
	{
		case D3DRS_LINEPATTERN:
		case D3DRS_ZVISIBLE:
		case D3DRS_EDGEANTIALIAS:
		case D3DRS_PATCHSEGMENTS:
			return D3DERR_INVALIDCALL;
		case D3DRS_SOFTWAREVERTEXPROCESSING:
			return _proxy->SetSoftwareVertexProcessing(Value);
		case D3DRS_ZBIAS:
			biased = static_cast<FLOAT>(Value) * -0.000005f;
			Value = *reinterpret_cast<const DWORD *>(&biased);
		default:
			return _proxy->SetRenderState(State, Value);
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
			hr = _proxy->GetRenderState(D3DRS_DEPTHBIAS, pValue);
			*pValue = static_cast<DWORD>(*reinterpret_cast<const FLOAT *>(pValue) * -500000.0f);
			return hr;
		case D3DRS_SOFTWAREVERTEXPROCESSING:
			*pValue = _proxy->GetSoftwareVertexProcessing();
			return D3D_OK;
		case D3DRS_PATCHSEGMENTS:
			*pValue = 1;
			return D3D_OK;
		default:
			return _proxy->GetRenderState(State, pValue);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::BeginStateBlock()
{
	return _proxy->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::EndStateBlock(DWORD *pToken)
{
	if (pToken == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return _proxy->EndStateBlock(reinterpret_cast<IDirect3DStateBlock9 **>(pToken));
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

	return _proxy->CreateStateBlock(Type, reinterpret_cast<IDirect3DStateBlock9 **>(pToken));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetClipStatus(CONST D3DCLIPSTATUS8 *pClipStatus)
{
	return _proxy->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetClipStatus(D3DCLIPSTATUS8 *pClipStatus)
{
	return _proxy->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTexture(DWORD Stage, Direct3DBaseTexture8 **ppTexture)
{
	if (ppTexture == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppTexture = nullptr;

	IDirect3DBaseTexture9 *basetexture = nullptr;

	const auto hr = _proxy->GetTexture(Stage, &basetexture);

	if (FAILED(hr))
	{
		return hr;
	}

	if (basetexture != nullptr)
	{
		IDirect3DTexture9 *texture = nullptr;
		IDirect3DVolumeTexture9 *volumetexture = nullptr;
		IDirect3DCubeTexture9 *cubetexture = nullptr;

		switch (basetexture->GetType())
		{
			case D3DRTYPE_TEXTURE:
				basetexture->QueryInterface(IID_PPV_ARGS(&texture));
				*ppTexture = new Direct3DTexture8(this, texture);
				break;
			case D3DRTYPE_VOLUMETEXTURE:
				basetexture->QueryInterface(IID_PPV_ARGS(&volumetexture));
				*ppTexture = new Direct3DVolumeTexture8(this, volumetexture);
				break;
			case D3DRTYPE_CUBETEXTURE:
				basetexture->QueryInterface(IID_PPV_ARGS(&cubetexture));
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
		return _proxy->SetTexture(Stage, nullptr);
	}

	IDirect3DBaseTexture9 *basetexture;

	switch (pTexture->GetType())
	{
		case D3DRTYPE_TEXTURE:
			basetexture = static_cast<Direct3DTexture8 *>(pTexture)->GetProxyInterface();
			break;
		case D3DRTYPE_VOLUMETEXTURE:
			basetexture = static_cast<Direct3DVolumeTexture8 *>(pTexture)->GetProxyInterface();
			break;
		case D3DRTYPE_CUBETEXTURE:
			basetexture = static_cast<Direct3DCubeTexture8 *>(pTexture)->GetProxyInterface();
			break;
		default:
			return D3DERR_INVALIDCALL;
	}

	return _proxy->SetTexture(Stage, basetexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
	switch (static_cast<DWORD>(Type))
	{
		case D3DTSS_ADDRESSU:
			return _proxy->GetSamplerState(Stage, D3DSAMP_ADDRESSU, pValue);
		case D3DTSS_ADDRESSV:
			return _proxy->GetSamplerState(Stage, D3DSAMP_ADDRESSV, pValue);
		case D3DTSS_ADDRESSW:
			return _proxy->GetSamplerState(Stage, D3DSAMP_ADDRESSW, pValue);
		case D3DTSS_BORDERCOLOR:
			return _proxy->GetSamplerState(Stage, D3DSAMP_BORDERCOLOR, pValue);
		case D3DTSS_MAGFILTER:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MAGFILTER, pValue);
		case D3DTSS_MINFILTER:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MINFILTER, pValue);
		case D3DTSS_MIPFILTER:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MIPFILTER, pValue);
		case D3DTSS_MIPMAPLODBIAS:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MIPMAPLODBIAS, pValue);
		case D3DTSS_MAXMIPLEVEL:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MAXMIPLEVEL, pValue);
		case D3DTSS_MAXANISOTROPY:
			return _proxy->GetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, pValue);
		default:
			return _proxy->GetTextureStageState(Stage, Type, pValue);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	switch (static_cast<DWORD>(Type))
	{
		case D3DTSS_ADDRESSU:
			return _proxy->SetSamplerState(Stage, D3DSAMP_ADDRESSU, Value);
		case D3DTSS_ADDRESSV:
			return _proxy->SetSamplerState(Stage, D3DSAMP_ADDRESSV, Value);
		case D3DTSS_ADDRESSW:
			return _proxy->SetSamplerState(Stage, D3DSAMP_ADDRESSW, Value);
		case D3DTSS_BORDERCOLOR:
			return _proxy->SetSamplerState(Stage, D3DSAMP_BORDERCOLOR, Value);
		case D3DTSS_MAGFILTER:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MAGFILTER, Value);
		case D3DTSS_MINFILTER:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MINFILTER, Value);
		case D3DTSS_MIPFILTER:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MIPFILTER, Value);
		case D3DTSS_MIPMAPLODBIAS:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MIPMAPLODBIAS, Value);
		case D3DTSS_MAXMIPLEVEL:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MAXMIPLEVEL, Value);
		case D3DTSS_MAXANISOTROPY:
			return _proxy->SetSamplerState(Stage, D3DSAMP_MAXANISOTROPY, Value);
		default:
			return _proxy->SetTextureStageState(Stage, Type, Value);
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ValidateDevice(DWORD *pNumPasses)
{
	return _proxy->ValidateDevice(pNumPasses);
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
	return _proxy->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return _proxy->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return _proxy->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetCurrentTexturePalette(UINT *pPaletteNumber)
{
	return _proxy->GetCurrentTexturePalette(pPaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	return _proxy->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
	return _proxy->DrawIndexedPrimitive(PrimitiveType, _base_vertex_index, MinIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	return _proxy->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	return _proxy->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, Direct3DVertexBuffer8 *pDestBuffer, DWORD Flags)
{
	if (pDestBuffer == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return _proxy->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer->GetProxyInterface(), nullptr, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::CreateVertexShader(CONST DWORD *pDeclaration, CONST DWORD *pFunction, DWORD *pHandle, DWORD Usage)
{
	UNREFERENCED_PARAMETER(Usage);

	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::CreateVertexShader" << "(" << this << ", " << pDeclaration << ", " << pFunction << ", " << pHandle << ", " << Usage << ")' ...";

	if (pDeclaration == nullptr || pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*pHandle = 0;

	UINT i = 0;
	const UINT limit = 32;
	std::string constants;
	WORD stream = 0, offset = 0;
	DWORD inputs[limit];
	D3DVERTEXELEMENT9 elements[limit];

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
		const DWORD token_type = (token & D3DVSD_TOKENTYPEMASK) >> D3DVSD_TOKENTYPESHIFT;

		if (token == D3DVSD_END())
		{
			break;
		}
		else if (token_type == D3DVSD_TOKEN_STREAM)
		{
			stream = static_cast<WORD>((token & D3DVSD_STREAMNUMBERMASK) >> D3DVSD_STREAMNUMBERSHIFT);
			offset = 0;
		}
		else if (token_type == D3DVSD_TOKEN_STREAMDATA && !(token & 0x10000000))
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
		else if (token_type == D3DVSD_TOKEN_STREAMDATA && (token & 0x10000000))
		{
			offset += ((token & D3DVSD_SKIPCOUNTMASK) >> D3DVSD_SKIPCOUNTSHIFT) * sizeof(DWORD);
		}
		else if (token_type == D3DVSD_TOKEN_TESSELLATOR && !(token & 0x10000000))
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
		else if (token_type == D3DVSD_TOKEN_TESSELLATOR && (token & 0x10000000))
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
		else if (token_type == D3DVSD_TOKEN_CONSTMEM)
		{
			const DWORD count = 4 * ((token & D3DVSD_CONSTCOUNTMASK) >> D3DVSD_CONSTCOUNTSHIFT);
			DWORD address = (token & D3DVSD_CONSTADDRESSMASK) >> D3DVSD_CONSTADDRESSSHIFT;

			for (DWORD r = 0; r < count; r += 4, ++address)
			{
				constants += "    def c" + std::to_string(address) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 1])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 2])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 3])) + ", " + std::to_string(*reinterpret_cast<const float *>(&pDeclaration[r + 4])) + " /* vertex declaration constant */\n";
			}

			pDeclaration += count;
		}
		else
		{
			LOG(ERROR) << "> Failed because token type '" << token_type << "' is not supported!";

			return E_NOTIMPL;
		}

		++pDeclaration;
	}

	LOG(TRACE) << "  +----------+---------+---------+--------------+--------------+--------------+-------+";
	LOG(TRACE) << "  | Register | Stream  | Offset  | Type         | Method       | Usage        | Index |";
	LOG(TRACE) << "  +----------+---------+---------+--------------+--------------+--------------+-------+";

	for (UINT k = 0; k < i; k++)
	{
		LOG(TRACE) << "  | v" << std::setw(7) << inputs[k] << " | " << std::setw(7) << elements[k].Stream << " | " << std::setw(7) << elements[k].Offset << " | " << std::showbase << std::hex << std::setw(12) << static_cast<UINT>(elements[k].Type) << " | " << std::setw(12) << static_cast<UINT>(elements[k].Method) << " | " << std::setw(12) << static_cast<UINT>(elements[k].Usage) << std::dec << std::noshowbase << " | " << std::setw(5) << static_cast<UINT>(elements[k].UsageIndex) << " |";
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
			LOG(ERROR) << "> Failed because of version mismatch ('" << std::showbase << std::hex << *pFunction << std::dec << std::noshowbase << "')! Only 'vs_1_x' shaders are supported.";

			return D3DERR_INVALIDCALL;
		}

		ID3DXBuffer *disassembly = nullptr, *assembly = nullptr, *errors = nullptr;

		hr = D3DXDisassembleShader(pFunction, FALSE, nullptr, &disassembly);

		if (FAILED(hr))
		{
			LOG(ERROR) << "> Failed to disassemble shader with '" << GetErrorString(hr) << "'!";

			return hr;
		}

		std::string source(static_cast<const char *>(disassembly->GetBufferPointer()), disassembly->GetBufferSize() - 1);
		const size_t verpos = source.find("vs_1_");

		assert(verpos != std::string::npos);

		if (source.at(verpos + 5) == '0')
		{
			LOG(INFO) << "> Replacing version 'vs_1_0' with 'vs_1_1' ...";

			source.replace(verpos, 6, "vs_1_1");
		}

		size_t declpos = verpos + 7;

		for (UINT k = 0; k < i; k++)
		{
			std::string decl = "    ";

			switch (elements[k].Usage)
			{
				case D3DDECLUSAGE_POSITION:
					decl += "dcl_position";
					break;
				case D3DDECLUSAGE_BLENDWEIGHT:
					decl += "dcl_blendweight";
					break;
				case D3DDECLUSAGE_BLENDINDICES:
					decl += "dcl_blendindices";
					break;
				case D3DDECLUSAGE_NORMAL:
					decl += "dcl_normal";
					break;
				case D3DDECLUSAGE_PSIZE:
					decl += "dcl_psize";
					break;
				case D3DDECLUSAGE_COLOR:
					decl += "dcl_color";
					break;
				case D3DDECLUSAGE_TEXCOORD:
					decl += "dcl_texcoord";
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

		#pragma region Fill registers with default value
		constants += "    def c95, 0, 0, 0, 0\n";

		source.insert(declpos, constants);

		for (size_t j = 0; j < 2; j++)
		{
			const std::string reg = "oD" + std::to_string(j);

			if (source.find(reg) != std::string::npos)
			{
				source.insert(declpos + constants.size(), "    mov " + reg + ", c95 /* initialize output register " + reg + " */\n");
			}
		}
		for (size_t j = 0; j < 8; j++)
		{
			const std::string reg = "oT" + std::to_string(j);

			if (source.find(reg) != std::string::npos)
			{
				source.insert(declpos + constants.size(), "    mov " + reg + ", c95 /* initialize output register " + reg + " */\n");
			}
		}
		for (size_t j = 0; j < 12; j++)
		{
			const std::string reg = "r" + std::to_string(j);

			if (source.find(reg) != std::string::npos)
			{
				source.insert(declpos + constants.size(), "    mov " + reg + ", c95 /* initialize register " + reg + " */\n");
			}
		}
		#pragma endregion

		source = std::regex_replace(source, std::regex("    \\/\\/ vs\\.1\\.1\\n((?! ).+\\n)+"), "");
		source = std::regex_replace(source, std::regex("(oFog|oPts)\\.x"), "$1 /* removed swizzle */");
		source = std::regex_replace(source, std::regex("(add|sub|mul|min|max) (oFog|oPts), ([cr][0-9]+), (.+)\\n"), "$1 $2, $3.x /* added swizzle */, $4\n");
		source = std::regex_replace(source, std::regex("(add|sub|mul|min|max) (oFog|oPts), (.+), ([cr][0-9]+)\\n"), "$1 $2, $3, $4.x /* added swizzle */\n");
		source = std::regex_replace(source, std::regex("mov (oFog|oPts)(.*), (-?)([crv][0-9]+)(?!\\.)"), "mov $1$2, $3$4.x /* select single component */");

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

		hr = _proxy->CreateVertexShader(static_cast<const DWORD *>(assembly->GetBufferPointer()), &shader->shader);

		assembly->Release();
	}
	else
	{
		shader = new Direct3DVertexShader8();
		shader->shader = nullptr;

		hr = D3D_OK;
	}

	if (SUCCEEDED(hr))
	{
		hr = _proxy->CreateVertexDeclaration(elements, &shader->declaration);

		if (SUCCEEDED(hr))
		{
			*pHandle = reinterpret_cast<DWORD>(shader) | 0x80000000;
		}
		else
		{
			LOG(ERROR) << "> 'IDirect3DDevice9::CreateVertexDeclaration' failed with '" << GetErrorString(hr) << "'!";

			if (shader->shader != nullptr)
			{
				shader->shader->Release();
			}
		}
	}
	else
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::CreateVertexShader' failed with '" << GetErrorString(hr) << "'!";
	}

	if (FAILED(hr))
	{
		delete shader;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetVertexShader(DWORD Handle)
{
	HRESULT hr;

	if ((Handle & 0x80000000) == 0)
	{
		_proxy->SetVertexShader(nullptr);
		hr = _proxy->SetFVF(Handle);

		_current_vertex_shader = 0;
	}
	else
	{
		const auto shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000);

		hr = _proxy->SetVertexShader(shader->shader);
		_proxy->SetVertexDeclaration(shader->declaration);

		_current_vertex_shader = Handle;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShader(DWORD *pHandle)
{
	if (pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	if (_current_vertex_shader == 0)
	{
		return _proxy->GetFVF(pHandle);
	}
	else
	{
		*pHandle = _current_vertex_shader;

		return D3D_OK;
	}
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeleteVertexShader(DWORD Handle)
{
	if ((Handle & 0x80000000) == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	if (_current_vertex_shader == Handle)
	{
		SetVertexShader(0);
	}

	const auto shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000);

	if (shader->shader != nullptr)
	{
		shader->shader->Release();
	}
	if (shader->declaration != nullptr)
	{
		shader->declaration->Release();
	}

	delete shader;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetVertexShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount)
{
	return _proxy->SetVertexShaderConstantF(Register, static_cast<CONST float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetVertexShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount)
{
	return _proxy->GetVertexShaderConstantF(Register, static_cast<float *>(pConstantData), ConstantCount);
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

	const auto shader = reinterpret_cast<Direct3DVertexShader8 *>(Handle ^ 0x80000000)->shader;

	if (shader == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	LOG(WARNING) << "> Returning translated shader byte code.";

	return shader->GetFunction(pData, reinterpret_cast<UINT *>(pSizeOfData));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 *pStreamData, UINT Stride)
{
	if (pStreamData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	return _proxy->SetStreamSource(StreamNumber, pStreamData->GetProxyInterface(), 0, Stride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetStreamSource(UINT StreamNumber, Direct3DVertexBuffer8 **ppStreamData, UINT *pStride)
{
	if (ppStreamData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}
	else
	{
		*ppStreamData = nullptr;
	}

	UINT offset;
	IDirect3DVertexBuffer9 *source = nullptr;

	const auto hr = _proxy->GetStreamSource(StreamNumber, &source, &offset, pStride);

	if (FAILED(hr))
	{
		return hr;
	}

	if (source != nullptr)
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

	_base_vertex_index = static_cast<INT>(BaseVertexIndex);

	return _proxy->SetIndices(pIndexData->GetProxyInterface());
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetIndices(Direct3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex)
{
	if (ppIndexData == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppIndexData = nullptr;

	if (pBaseVertexIndex != nullptr)
	{
		*pBaseVertexIndex = static_cast<UINT>(_base_vertex_index);
	}

	IDirect3DIndexBuffer9 *source = nullptr;

	const auto hr = _proxy->GetIndices(&source);

	if (FAILED(hr))
	{
		return hr;
	}

	if (source != nullptr)
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

	*pHandle = 0;

	LOG(INFO) << "> Disassembling shader and translating assembly to Direct3D 9 compatible code ...";

	if (*pFunction < D3DPS_VERSION(1, 0) || *pFunction > D3DPS_VERSION(1, 4))
	{
		LOG(ERROR) << "> Failed because of version mismatch ('" << std::showbase << std::hex << *pFunction << std::dec << std::noshowbase << "')! Only 'ps_1_x' shaders are supported.";

		return D3DERR_INVALIDCALL;
	}

	ID3DXBuffer *disassembly = nullptr, *assembly = nullptr, *errors = nullptr;

	HRESULT hr = D3DXDisassembleShader(pFunction, FALSE, nullptr, &disassembly);

	if (FAILED(hr))
	{
		LOG(ERROR) << "> Failed to disassemble shader with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	std::string source(static_cast<const char *>(disassembly->GetBufferPointer()), disassembly->GetBufferSize() - 1);
	const size_t verpos = source.find("ps_1_");

	assert(verpos != std::string::npos);

	if (source.at(verpos + 5) == '0')
	{
		LOG(INFO) << "> Replacing version 'ps_1_0' with 'ps_1_1' ...";

		source.replace(verpos, 6, "ps_1_1");
	}

	source = std::regex_replace(source, std::regex("    \\/\\/ ps\\.1\\.[1-4]\\n((?! ).+\\n)+"), "");
	source = std::regex_replace(source, std::regex("(1?-)(c[0-9]+)"), "$2 /* removed modifier $1 */");
	source = std::regex_replace(source, std::regex("(c[0-9]+)(_bx2|_bias)"), "$1 /* removed modifier $2 */");

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

	hr = _proxy->CreatePixelShader(static_cast<const DWORD *>(assembly->GetBufferPointer()), reinterpret_cast<IDirect3DPixelShader9 **>(pHandle));

	if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::CreatePixelShader' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetPixelShader(DWORD Handle)
{
	_current_pixel_shader = Handle;

	return _proxy->SetPixelShader(reinterpret_cast<IDirect3DPixelShader9 *>(Handle));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShader(DWORD *pHandle)
{
	if (pHandle == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*pHandle = _current_pixel_shader;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeletePixelShader(DWORD Handle)
{
	if (Handle == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	if (_current_pixel_shader == Handle)
	{
		SetPixelShader(0);
	}

	reinterpret_cast<IDirect3DPixelShader9 *>(Handle)->Release();

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::SetPixelShaderConstant(DWORD Register, CONST void *pConstantData, DWORD ConstantCount)
{
	return _proxy->SetPixelShaderConstantF(Register, static_cast<CONST float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShaderConstant(DWORD Register, void *pConstantData, DWORD ConstantCount)
{
	return _proxy->GetPixelShaderConstantF(Register, static_cast<float *>(pConstantData), ConstantCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::GetPixelShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice8::GetPixelShaderFunction" << "(" << this << ", " << Handle << ", " << pData << ", " << pSizeOfData << ")' ...";

	if (Handle == 0)
	{
		return D3DERR_INVALIDCALL;
	}

	const auto shader = reinterpret_cast<IDirect3DPixelShader9 *>(Handle);

	LOG(WARNING) << "> Returning translated shader byte code.";

	return shader->GetFunction(pData, reinterpret_cast<UINT *>(pSizeOfData));
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawRectPatch(UINT Handle, CONST float *pNumSegs, CONST D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return _proxy->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DrawTriPatch(UINT Handle, CONST float *pNumSegs, CONST D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return _proxy->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice8::DeletePatch(UINT Handle)
{
	return _proxy->DeletePatch(Handle);
}

// IDirect3D8
HRESULT STDMETHODCALLTYPE Direct3D8::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown))
	{
		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _proxy->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3D8::AddRef()
{
	return _proxy->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3D8::Release()
{
	const auto ref = _proxy->Release();

	if (ref == 0)
	{
		FreeLibrary(_module);

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3D8::RegisterSoftwareDevice(void *pInitializeFunction)
{
	return _proxy->RegisterSoftwareDevice(pInitializeFunction);
}
UINT STDMETHODCALLTYPE Direct3D8::GetAdapterCount()
{
	return _proxy->GetAdapterCount();
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8 *pIdentifier)
{
	if (pIdentifier == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DADAPTER_IDENTIFIER9 identifier;

	if ((Flags & D3DENUM_NO_WHQL_LEVEL) == 0)
	{
		Flags |= D3DENUM_WHQL_LEVEL;
	}
	else
	{
		Flags ^= D3DENUM_NO_WHQL_LEVEL;
	}

	const auto hr = _proxy->GetAdapterIdentifier(Adapter, Flags, &identifier);

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

	for (auto format : formats)
	{
		count += _proxy->GetAdapterModeCount(Adapter, format);
	}

	return count;
}
HRESULT STDMETHODCALLTYPE Direct3D8::EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE *pMode)
{
	if (pMode == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	const D3DFORMAT formats[] = { D3DFMT_A8R8G8B8, D3DFMT_X8R8G8B8, D3DFMT_R5G6B5, D3DFMT_X1R5G5B5, D3DFMT_A1R5G5B5, D3DFMT_A2R10G10B10 };

	UINT offset = 0;

	for (auto format : formats)
	{
		const auto modes = _proxy->GetAdapterModeCount(Adapter, format);

		if (modes == 0)
		{
			continue;
		}

		if (Mode < offset + modes)
		{
			return _proxy->EnumAdapterModes(Adapter, format, Mode - offset, pMode);
		}

		offset += modes;
	}

	return D3DERR_INVALIDCALL;
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode)
{
	return _proxy->GetAdapterDisplayMode(Adapter, pMode);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceType(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
{
	return _proxy->CheckDeviceType(Adapter, CheckType, DisplayFormat, BackBufferFormat, bWindowed);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
	return _proxy->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType)
{
	return _proxy->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, nullptr);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
{
	return _proxy->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}
HRESULT STDMETHODCALLTYPE Direct3D8::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8 *pCaps)
{
	if (pCaps == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	D3DCAPS9 caps;

	const auto hr = _proxy->GetDeviceCaps(Adapter, DeviceType, &caps);

	if (FAILED(hr))
	{
		return hr;
	}

	ConvertCaps(caps, *pCaps);

	return D3D_OK;
}
HMONITOR STDMETHODCALLTYPE Direct3D8::GetAdapterMonitor(UINT Adapter)
{
	return _proxy->GetAdapterMonitor(Adapter);
}
HRESULT STDMETHODCALLTYPE Direct3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS8 *pPresentationParameters, Direct3DDevice8 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D8::CreateDevice" << "(" << this << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << BehaviorFlags << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr || ppReturnedDeviceInterface == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	*ppReturnedDeviceInterface = nullptr;

	D3DPRESENT_PARAMETERS pp;
	IDirect3DDevice9 *device = nullptr;

	ConvertPresentParameters(*pPresentationParameters, pp);

	const auto hr = _proxy->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, &pp, &device);

	if (FAILED(hr))
	{
		return hr;
	}

	const auto deviceProxy = new Direct3DDevice8(this, device, (pp.Flags & D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL) != 0);

	// Set default vertex declaration
	device->SetFVF(D3DFVF_XYZ);

	// Set default render target
	IDirect3DSurface9 *rendertarget = nullptr, *depthstencil = nullptr;
	Direct3DSurface8 *rendertarget_proxy = nullptr, *depthstencil_proxy = nullptr;

	device->GetRenderTarget(0, &rendertarget);
	device->GetDepthStencilSurface(&depthstencil);

	if (rendertarget != nullptr)
	{
		rendertarget_proxy = new Direct3DSurface8(deviceProxy, rendertarget);
	}
	if (depthstencil != nullptr)
	{
		depthstencil_proxy = new Direct3DSurface8(deviceProxy, depthstencil);
	}

	deviceProxy->SetRenderTarget(rendertarget_proxy, depthstencil_proxy);

	if (rendertarget_proxy != nullptr)
	{
		rendertarget_proxy->Release();
	}
	if (depthstencil_proxy != nullptr)
	{
		depthstencil_proxy->Release();
	}

	*ppReturnedDeviceInterface = deviceProxy;

	return D3D_OK;
}

// D3D8
extern "C" Direct3D8 *WINAPI Direct3DCreate8(UINT SDKVersion)
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
