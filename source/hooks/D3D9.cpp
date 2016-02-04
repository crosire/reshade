#include "log.hpp"
#include "hook_manager.hpp"
#include "hooks\D3D9.hpp"

#pragma region Undefine Function Names
#undef IDirect3D9_CreateDevice
#undef IDirect3D9Ex_CreateDeviceEx
#pragma endregion

namespace
{
	void dump_present_parameters(const D3DPRESENT_PARAMETERS &pp)
	{
		LOG(TRACE) << "> Dumping presentation parameters:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | BackBufferWidth                         | " << std::setw(39) << pp.BackBufferWidth << " |";
		LOG(TRACE) << "  | BackBufferHeight                        | " << std::setw(39) << pp.BackBufferHeight << " |";
		LOG(TRACE) << "  | BackBufferFormat                        | " << std::setw(39) << pp.BackBufferFormat << " |";
		LOG(TRACE) << "  | BackBufferCount                         | " << std::setw(39) << pp.BackBufferCount << " |";
		LOG(TRACE) << "  | MultiSampleType                         | " << std::setw(39) << pp.MultiSampleType << " |";
		LOG(TRACE) << "  | MultiSampleQuality                      | " << std::setw(39) << pp.MultiSampleQuality << " |";
		LOG(TRACE) << "  | SwapEffect                              | " << std::setw(39) << pp.SwapEffect << " |";
		LOG(TRACE) << "  | DeviceWindow                            | " << std::setw(39) << pp.hDeviceWindow << " |";
		LOG(TRACE) << "  | Windowed                                | " << std::setw(39) << (pp.Windowed != FALSE ? "TRUE" : "FALSE") << " |";
		LOG(TRACE) << "  | EnableAutoDepthStencil                  | " << std::setw(39) << (pp.EnableAutoDepthStencil ? "TRUE" : "FALSE") << " |";
		LOG(TRACE) << "  | AutoDepthStencilFormat                  | " << std::setw(39) << pp.AutoDepthStencilFormat << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::showbase << std::hex << pp.Flags << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  | FullScreen_RefreshRateInHz              | " << std::setw(39) << pp.FullScreen_RefreshRateInHz << " |";
		LOG(TRACE) << "  | PresentationInterval                    | " << std::setw(39) << std::showbase << std::hex << pp.PresentationInterval << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		if (pp.MultiSampleType != D3DMULTISAMPLE_NONE)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";
		}
	}
}

// IDirect3DSwapChain9
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DSwapChain9) ||
		riid == __uuidof(IDirect3DSwapChain9Ex))
	{
		#pragma region Update to IDirect3DSwapChain9Ex interface
		if (riid == __uuidof(IDirect3DSwapChain9Ex) && _interface_version < 1)
		{
			IDirect3DSwapChain9Ex *swapchainex = nullptr;

			if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&swapchainex))))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

			LOG(TRACE) << "Upgraded 'IDirect3DSwapChain9' object " << this << " to 'IDirect3DSwapChain9Ex'.";

			_orig = swapchainex;
			_interface_version = 1;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DSwapChain9::Release()
{
	if (--_ref == 0)
	{
		assert(_runtime != nullptr);

		_runtime->on_reset();

		_runtime.reset();

		const auto it = std::find(_device->_additional_swapchains.begin(), _device->_additional_swapchains.end(), this);

		if (it != _device->_additional_swapchains.end())
		{
			_device->_additional_swapchains.erase(it);

			_device->Release();
		}
	}

	ULONG ref = _orig->Release();

	if (_ref == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DSwapChain9" << (_interface_version > 0 ? "Ex" : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(_ref <= 0);

		LOG(TRACE) << "Destroyed 'IDirect3DSwapChain9" << (_interface_version > 0 ? "Ex" : "") << "' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(_runtime != nullptr);

	_runtime->on_present();

	return _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetFrontBufferData(IDirect3DSurface9 *pDestSurface)
{
	return _orig->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	return _orig->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
	return _orig->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
	return _orig->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDevice(IDirect3DDevice9 **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	_device->AddRef();

	*ppDevice = _device;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
	return _orig->GetPresentParameters(pPresentationParameters);
}

// IDirect3DSwapChain9Ex
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetLastPresentCount(UINT *pLastPresentCount)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetLastPresentCount(pLastPresentCount);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetPresentStats(D3DPRESENTSTATS *pPresentationStatistics)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetPresentStats(pPresentationStatistics);
}
HRESULT STDMETHODCALLTYPE Direct3DSwapChain9::GetDisplayModeEx(D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DSwapChain9Ex *>(_orig)->GetDisplayModeEx(pMode, pRotation);
}

// IDirect3DDevice9
HRESULT STDMETHODCALLTYPE Direct3DDevice9::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDirect3DDevice9) ||
		riid == __uuidof(IDirect3DDevice9Ex))
	{
		#pragma region Update to IDirect3DDevice9Ex interface
		if (riid == __uuidof(IDirect3DDevice9Ex) && _interface_version < 1)
		{
			IDirect3DDevice9Ex *deviceex = nullptr;

			if (FAILED(_orig->QueryInterface(IID_PPV_ARGS(&deviceex))))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

			LOG(TRACE) << "Upgraded 'IDirect3DDevice9' object " << this << " to 'IDirect3DDevice9Ex'.";

			_orig = deviceex;
			_interface_version = 1;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE Direct3DDevice9::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE Direct3DDevice9::Release()
{
	if (--_ref == 0)
	{
		if (_auto_depthstencil != nullptr)
		{
			_auto_depthstencil->Release();
			_auto_depthstencil = nullptr;
		}

		assert(_implicit_swapchain != nullptr);

		_implicit_swapchain->Release();
	}

	ULONG ref = _orig->Release();

	if (_ref == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDirect3DDevice9" << (_interface_version > 0 ? "Ex" : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(_ref <= 0);

		LOG(TRACE) << "Destroyed 'IDirect3DDevice9" << (_interface_version > 0 ? "Ex" : "") << "' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::TestCooperativeLevel()
{
	return _orig->TestCooperativeLevel();
}
UINT STDMETHODCALLTYPE Direct3DDevice9::GetAvailableTextureMem()
{
	return _orig->GetAvailableTextureMem();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EvictManagedResources()
{
	return _orig->EvictManagedResources();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
	return _orig->GetDirect3D(ppD3D9);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
	return _orig->GetDeviceCaps(pCaps);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);

	return _implicit_swapchain->GetDisplayMode(pMode);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	return _orig->GetCreationParameters(pParameters);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap)
{
	return _orig->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}
void STDMETHODCALLTYPE Direct3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags)
{
	return _orig->SetCursorPosition(X, Y, Flags);
}
BOOL STDMETHODCALLTYPE Direct3DDevice9::ShowCursor(BOOL bShow)
{
	return _orig->ShowCursor(bShow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9::CreateAdditionalSwapChain" << "(" << this << ", " << pPresentationParameters << ", " << ppSwapChain << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	dump_present_parameters(*pPresentationParameters);

	const HRESULT hr = _orig->CreateAdditionalSwapChain(pPresentationParameters, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3DDevice9::CreateAdditionalSwapChain' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}

	IDirect3DDevice9 *const device = _orig;
	IDirect3DSwapChain9 *const swapchain = *ppSwapChain;

	assert(swapchain != nullptr);

	D3DPRESENT_PARAMETERS pp;
	swapchain->GetPresentParameters(&pp);

	const auto runtime = std::make_shared<reshade::runtimes::d3d9_runtime>(device, swapchain);

	if (!runtime->on_init(pp))
	{
		LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
	}

	AddRef();

	const auto swapchain_proxy = new Direct3DSwapChain9(this, swapchain, runtime);

	_additional_swapchains.push_back(swapchain_proxy);
	*ppSwapChain = swapchain_proxy;

	LOG(TRACE) << "> Returned swap chain object: " << *ppSwapChain;

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **ppSwapChain)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	if (ppSwapChain == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);

	_implicit_swapchain->AddRef();

	*ppSwapChain = _implicit_swapchain;

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

	dump_present_parameters(*pPresentationParameters);

	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	const auto runtime = _implicit_swapchain->_runtime;

	runtime->on_reset();

	if (_auto_depthstencil != nullptr)
	{
		_auto_depthstencil->Release();
		_auto_depthstencil = nullptr;
	}

	const HRESULT hr = _orig->Reset(pPresentationParameters);

	if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDirect3DDevice9::Reset' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}

	D3DPRESENT_PARAMETERS pp;
	_implicit_swapchain->GetPresentParameters(&pp);

	if (!runtime->on_init(pp))
	{
		LOG(ERROR) << "Failed to recreate Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
	}

	if (pp.EnableAutoDepthStencil)
	{
		_orig->GetDepthStencilSurface(&_auto_depthstencil);
		SetDepthStencilSurface(_auto_depthstencil);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Present(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion)
{
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_present();

	return _orig->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);

	return _implicit_swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);

	return _implicit_swapchain->GetRasterStatus(pRasterStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
	return _orig->SetDialogBoxMode(bEnableDialogs);
}
void STDMETHODCALLTYPE Direct3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return;
	}

	return _orig->SetGammaRamp(0, Flags, pRamp);
}
void STDMETHODCALLTYPE Direct3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return;
	}

	return _orig->GetGammaRamp(0, pRamp);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle)
{
	return _orig->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle)
{
	return _orig->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle)
{
	return _orig->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle)
{
	if (_use_software_rendering)
	{
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}

	return _orig->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle)
{
	if (_use_software_rendering)
	{
		Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}

	return _orig->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return _orig->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return _orig->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint)
{
	return _orig->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture)
{
	return _orig->UpdateTexture(pSourceTexture, pDestinationTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTargetData(IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface)
{
	return _orig->GetRenderTargetData(pRenderTarget, pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9 *pDestSurface)
{
	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);

	return _implicit_swapchain->GetFrontBufferData(pDestSurface);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::StretchRect(IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
	return _orig->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ColorFill(IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color)
{
	return _orig->ColorFill(pSurface, pRect, color);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
	return _orig->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget)
{
	return _orig->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget)
{
	return _orig->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
	if (pNewZStencil != nullptr)
	{
		assert(_implicit_swapchain != nullptr);
		assert(_implicit_swapchain->_runtime != nullptr);

		_implicit_swapchain->_runtime->on_set_depthstencil_surface(pNewZStencil);

		for (auto swapchain : _additional_swapchains)
		{
			assert(swapchain->_runtime != nullptr);

			swapchain->_runtime->on_set_depthstencil_surface(pNewZStencil);
		}
	}

	return _orig->SetDepthStencilSurface(pNewZStencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
	const HRESULT hr = _orig->GetDepthStencilSurface(ppZStencilSurface);

	if (FAILED(hr))
	{
		return hr;
	}
	else if (*ppZStencilSurface != nullptr)
	{
		assert(_implicit_swapchain != nullptr);
		assert(_implicit_swapchain->_runtime != nullptr);

		_implicit_swapchain->_runtime->on_get_depthstencil_surface(*ppZStencilSurface);

		for (auto swapchain : _additional_swapchains)
		{
			assert(swapchain->_runtime);

			swapchain->_runtime->on_get_depthstencil_surface(*ppZStencilSurface);
		}
	}

	return D3D_OK;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginScene()
{
	return _orig->BeginScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndScene()
{
	return _orig->EndScene();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	return _orig->Clear(Count, pRects, Flags, Color, Z, Stencil);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->SetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
	return _orig->GetTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
	return _orig->MultiplyTransform(State, pMatrix);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetViewport(const D3DVIEWPORT9 *pViewport)
{
	return _orig->SetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
	return _orig->GetViewport(pViewport);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaterial(const D3DMATERIAL9 *pMaterial)
{
	return _orig->SetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
	return _orig->GetMaterial(pMaterial);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetLight(DWORD Index, const D3DLIGHT9 *pLight)
{
	return _orig->SetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *pLight)
{
	return _orig->GetLight(Index, pLight);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::LightEnable(DWORD Index, BOOL Enable)
{
	return _orig->LightEnable(Index, Enable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable)
{
	return _orig->GetLightEnable(Index, pEnable);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipPlane(DWORD Index, const float *pPlane)
{
	return _orig->SetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipPlane(DWORD Index, float *pPlane)
{
	return _orig->GetClipPlane(Index, pPlane);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
	return _orig->SetRenderState(State, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
	return _orig->GetRenderState(State, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB)
{
	return _orig->CreateStateBlock(Type, ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::BeginStateBlock()
{
	return _orig->BeginStateBlock();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
	return _orig->EndStateBlock(ppSB);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetClipStatus(const D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->SetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
	return _orig->GetClipStatus(pClipStatus);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
	return _orig->GetTexture(Stage, ppTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
	return _orig->SetTexture(Stage, pTexture);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
	return _orig->GetTextureStageState(Stage, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	return _orig->SetTextureStageState(Stage, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue)
{
	return _orig->GetSamplerState(Sampler, Type, pValue);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	return _orig->SetSamplerState(Sampler, Type, Value);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ValidateDevice(DWORD *pNumPasses)
{
	return _orig->ValidateDevice(pNumPasses);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY *pEntries)
{
	return _orig->SetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
	return _orig->GetPaletteEntries(PaletteNumber, pEntries);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
	return _orig->SetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
	return _orig->GetCurrentTexturePalette(PaletteNumber);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetScissorRect(const RECT *pRect)
{
	return _orig->SetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetScissorRect(RECT *pRect)
{
	return _orig->GetScissorRect(pRect);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
	return _orig->SetSoftwareVertexProcessing(bSoftware);
}
BOOL STDMETHODCALLTYPE Direct3DDevice9::GetSoftwareVertexProcessing()
{
	return _orig->GetSoftwareVertexProcessing();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetNPatchMode(float nSegments)
{
	return _orig->SetNPatchMode(nSegments);
}
float STDMETHODCALLTYPE Direct3DDevice9::GetNPatchMode()
{
	return _orig->GetNPatchMode();
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);

	for (auto swapchain : _additional_swapchains)
	{
		assert(swapchain->_runtime != nullptr);

		swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);
	}

	return _orig->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);

	for (auto swapchain : _additional_swapchains)
	{
		assert(swapchain->_runtime != nullptr);

		swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);
	}

	return _orig->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, PrimitiveCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);

	for (auto swapchain : _additional_swapchains)
	{
		assert(swapchain->_runtime != nullptr);

		swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);
	}

	return _orig->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);

	for (auto swapchain : _additional_swapchains)
	{
		assert(swapchain->_runtime != nullptr);

		swapchain->_runtime->on_draw_call(PrimitiveType, PrimitiveCount);
	}

	return _orig->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags)
{
	return _orig->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl)
{
	return _orig->CreateVertexDeclaration(pVertexElements, ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
	return _orig->SetVertexDeclaration(pDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
	return _orig->GetVertexDeclaration(ppDecl);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetFVF(DWORD FVF)
{
	return _orig->SetFVF(FVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetFVF(DWORD *pFVF)
{
	return _orig->GetFVF(pFVF);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateVertexShader(const DWORD *pFunction, IDirect3DVertexShader9 **ppShader)
{
	return _orig->CreateVertexShader(pFunction, ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
	return _orig->SetVertexShader(pShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
	return _orig->GetVertexShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	return _orig->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	return _orig->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetVertexShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount)
{
	return _orig->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetVertexShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
	return _orig->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *OffsetInBytes, UINT *pStride)
{
	return _orig->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
{
	return _orig->SetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT *Divider)
{
	return _orig->GetStreamSourceFreq(StreamNumber, Divider);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
	return _orig->SetIndices(pIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData)
{
	return _orig->GetIndices(ppIndexData);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreatePixelShader(const DWORD *pFunction, IDirect3DPixelShader9 **ppShader)
{
	return _orig->CreatePixelShader(pFunction, ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
	return _orig->SetPixelShader(pShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
	return _orig->GetPixelShader(ppShader);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantF(UINT StartRegister, const float *pConstantData, UINT Vector4fCount)
{
	return _orig->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantF(UINT StartRegister, float *pConstantData, UINT Vector4fCount)
{
	return _orig->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantI(UINT StartRegister, const int *pConstantData, UINT Vector4iCount)
{
	return _orig->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int *pConstantData, UINT Vector4iCount)
{
	return _orig->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetPixelShaderConstantB(UINT StartRegister, const BOOL *pConstantData, UINT  BoolCount)
{
	return _orig->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetPixelShaderConstantB(UINT StartRegister, BOOL *pConstantData, UINT BoolCount)
{
	return _orig->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawRectPatch(UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo)
{
	return _orig->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DrawTriPatch(UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo)
{
	return _orig->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::DeletePatch(UINT Handle)
{
	return _orig->DeletePatch(Handle);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery)
{
	return _orig->CreateQuery(Type, ppQuery);
}

// IDirect3DDevice9Ex
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetConvolutionMonoKernel(UINT width, UINT height, float *rows, float *columns)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetConvolutionMonoKernel(width, height, rows, columns);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ComposeRects(IDirect3DSurface9 *pSrc, IDirect3DSurface9 *pDst, IDirect3DVertexBuffer9 *pSrcRectDescs, UINT NumRects, IDirect3DVertexBuffer9 *pDstRectDescs, D3DCOMPOSERECTSOP Operation, int Xoffset, int Yoffset)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->ComposeRects(pSrc, pDst, pSrcRectDescs, NumRects, pDstRectDescs, Operation, Xoffset, Yoffset);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::PresentEx(const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags)
{
	assert(_interface_version >= 1);
	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	_implicit_swapchain->_runtime->on_present();

	return static_cast<IDirect3DDevice9Ex *>(_orig)->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetGPUThreadPriority(INT *pPriority)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetGPUThreadPriority(pPriority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetGPUThreadPriority(INT Priority)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::WaitForVBlank(UINT iSwapChain)
{
	assert(_interface_version >= 1);

	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	return static_cast<IDirect3DDevice9Ex *>(_orig)->WaitForVBlank(0);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckResourceResidency(IDirect3DResource9 **pResourceArray, UINT32 NumResources)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckResourceResidency(pResourceArray, NumResources);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->GetMaximumFrameLatency(pMaxLatency);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CheckDeviceState(HWND hDestinationWindow)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->CheckDeviceState(hDestinationWindow);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateRenderTargetEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->CreateRenderTargetEx(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->CreateOffscreenPlainSurfaceEx(Width, Height, Format, Pool, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage)
{
	assert(_interface_version >= 1);

	return static_cast<IDirect3DDevice9Ex *>(_orig)->CreateDepthStencilSurfaceEx(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle, Usage);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::ResetEx(D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	assert(_interface_version >= 1);

	LOG(INFO) << "Redirecting '" << "IDirect3DDevice9Ex::ResetEx" << "(" << this << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	dump_present_parameters(*pPresentationParameters);

	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_runtime != nullptr);

	const auto runtime = _implicit_swapchain->_runtime;

	runtime->on_reset();

	if (_auto_depthstencil != nullptr)
	{
		_auto_depthstencil->Release();
		_auto_depthstencil = nullptr;
	}

	const HRESULT hr = static_cast<IDirect3DDevice9Ex *>(_orig)->ResetEx(pPresentationParameters, pFullscreenDisplayMode);

	if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDirect3DDevice9Ex::ResetEx' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}

	D3DPRESENT_PARAMETERS pp;
	_implicit_swapchain->GetPresentParameters(&pp);

	if (!runtime->on_init(pp))
	{
		LOG(ERROR) << "Failed to recreate Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
	}

	if (pp.EnableAutoDepthStencil)
	{
		_orig->GetDepthStencilSurface(&_auto_depthstencil);
		SetDepthStencilSurface(_auto_depthstencil);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9::GetDisplayModeEx(UINT iSwapChain, D3DDISPLAYMODEEX *pMode, D3DDISPLAYROTATION *pRotation)
{
	assert(_interface_version >= 1);

	if (iSwapChain != 0)
	{
		LOG(WARNING) << "Access to multi-head swap chain at index " << iSwapChain << " is unsupported.";

		return D3DERR_INVALIDCALL;
	}

	assert(_implicit_swapchain != nullptr);
	assert(_implicit_swapchain->_interface_version >= 1);

	return static_cast<IDirect3DSwapChain9Ex *>(_implicit_swapchain)->GetDisplayModeEx(pMode, pRotation);
}

// IDirect3D9
HRESULT STDMETHODCALLTYPE IDirect3D9_CreateDevice(IDirect3D9 *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9::CreateDevice" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << std::showbase << std::hex << BehaviorFlags << std::dec << std::noshowbase << ", " << pPresentationParameters << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	dump_present_parameters(*pPresentationParameters);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;

	if (use_software_rendering)
	{
		LOG(WARNING) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	const HRESULT hr = reshade::hooks::call(&IDirect3D9_CreateDevice)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3D9::CreateDevice' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}
	
	IDirect3DDevice9 *const device = *ppReturnedDeviceInterface;

	if (use_software_rendering)
	{
		device->SetSoftwareVertexProcessing(TRUE);
	}

	if (DeviceType != D3DDEVTYPE_NULLREF)
	{
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);

		D3DPRESENT_PARAMETERS pp;
		swapchain->GetPresentParameters(&pp);

		const auto runtime = std::make_shared<reshade::runtimes::d3d9_runtime>(device, swapchain);

		if (!runtime->on_init(pp))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
		}

		const auto device_proxy = new Direct3DDevice9(device);
		const auto swapchain_proxy = new Direct3DSwapChain9(device_proxy, swapchain, runtime);

		device_proxy->_implicit_swapchain = swapchain_proxy;
		device_proxy->_use_software_rendering = use_software_rendering;
		*ppReturnedDeviceInterface = device_proxy;

		if (pp.EnableAutoDepthStencil)
		{
			device->GetDepthStencilSurface(&device_proxy->_auto_depthstencil);
			device_proxy->SetDepthStencilSurface(device_proxy->_auto_depthstencil);
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
	}

	LOG(TRACE) << "> Returned device object: " << *ppReturnedDeviceInterface;

	return D3D_OK;
}

// IDirect3D9Ex
HRESULT STDMETHODCALLTYPE IDirect3D9Ex_CreateDeviceEx(IDirect3D9Ex *pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode, IDirect3DDevice9Ex **ppReturnedDeviceInterface)
{
	LOG(INFO) << "Redirecting '" << "IDirect3D9Ex::CreateDeviceEx" << "(" << pD3D << ", " << Adapter << ", " << DeviceType << ", " << hFocusWindow << ", " << std::showbase << std::hex << BehaviorFlags << std::dec << std::noshowbase << ", " << pPresentationParameters << ", " << pFullscreenDisplayMode << ", " << ppReturnedDeviceInterface << ")' ...";

	if (pPresentationParameters == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	dump_present_parameters(*pPresentationParameters);

	const bool use_software_rendering = (BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) != 0;

	if (use_software_rendering)
	{
		LOG(WARNING) << "> Replacing 'D3DCREATE_SOFTWARE_VERTEXPROCESSING' flag with 'D3DCREATE_MIXED_VERTEXPROCESSING' to allow for hardware rendering ...";

		BehaviorFlags = (BehaviorFlags & ~D3DCREATE_SOFTWARE_VERTEXPROCESSING) | D3DCREATE_MIXED_VERTEXPROCESSING;
	}

	const HRESULT hr = reshade::hooks::call(&IDirect3D9Ex_CreateDeviceEx)(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, pFullscreenDisplayMode, ppReturnedDeviceInterface);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDirect3D9Ex::CreateDeviceEx' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}

	IDirect3DDevice9Ex *const device = *ppReturnedDeviceInterface;

	if (use_software_rendering)
	{
		device->SetSoftwareVertexProcessing(TRUE);
	}

	if (DeviceType != D3DDEVTYPE_NULLREF)
	{
		IDirect3DSwapChain9 *swapchain = nullptr;
		device->GetSwapChain(0, &swapchain);

		assert(swapchain != nullptr);

		D3DPRESENT_PARAMETERS pp;
		swapchain->GetPresentParameters(&pp);

		const auto runtime = std::make_shared<reshade::runtimes::d3d9_runtime>(device, swapchain);

		if (!runtime->on_init(pp))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << runtime.get() << ".";
		}

		const auto device_proxy = new Direct3DDevice9(device);
		const auto swapchain_proxy = new Direct3DSwapChain9(device_proxy, swapchain, runtime);

		device_proxy->_implicit_swapchain = swapchain_proxy;
		device_proxy->_use_software_rendering = use_software_rendering;
		*ppReturnedDeviceInterface = device_proxy;

		if (pp.EnableAutoDepthStencil)
		{
			device->GetDepthStencilSurface(&device_proxy->_auto_depthstencil);
			device_proxy->SetDepthStencilSurface(device_proxy->_auto_depthstencil);
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping device due to device type being 'D3DDEVTYPE_NULLREF'.";
	}

	LOG(TRACE) << "> Returned device object: " << *ppReturnedDeviceInterface;

	return D3D_OK;
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
	reshade::hooks::call(&D3DPERF_SetOptions)(0);
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

	IDirect3D9 *const res = reshade::hooks::call(&Direct3DCreate9)(SDKVersion);

	if (res == nullptr)
	{
		LOG(WARNING) << "> 'Direct3DCreate9' failed!";

		return nullptr;
	}

	reshade::hooks::install(VTABLE(res), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));

	LOG(TRACE) << "> Returned factory object: " << res;

	return res;
}
EXPORT HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex **ppD3D)
{
	LOG(INFO) << "Redirecting '" << "Direct3DCreate9Ex" << "(" << SDKVersion << ", " << ppD3D << ")' ...";

	const HRESULT hr = reshade::hooks::call(&Direct3DCreate9Ex)(SDKVersion, ppD3D);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'Direct3DCreate9Ex' failed with error code " << std::showbase << std::hex << hr << std::dec << std::noshowbase << "!";

		return hr;
	}

	reshade::hooks::install(VTABLE(*ppD3D), 16, reinterpret_cast<reshade::hook::address>(&IDirect3D9_CreateDevice));
	reshade::hooks::install(VTABLE(*ppD3D), 20, reinterpret_cast<reshade::hook::address>(&IDirect3D9Ex_CreateDeviceEx));

	LOG(TRACE) << "> Returned factory object: " << *ppD3D;

	return D3D_OK;
}
