#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\EffectRuntimeD3D10.hpp"
#include "Runtimes\EffectRuntimeD3D11.hpp"

#include <dxgi.h>
#include <dxgi1_2.h>

// -----------------------------------------------------------------------------------------------------

namespace
{
	struct DXGISwapChain : public IDXGISwapChain1, private boost::noncopyable
	{
		DXGISwapChain(IDXGIFactory *pFactory, IDXGISwapChain *pOriginalSwapChain, const std::shared_ptr<ReShade::D3D10Runtime> runtime, const DXGI_SAMPLE_DESC &samples) : mRef(1), mFactory(pFactory), mOrig(pOriginalSwapChain), mOrigSamples(samples), mDirect3DVersion(10), mRuntime(runtime)
		{
		}
		DXGISwapChain(IDXGIFactory *pFactory, IDXGISwapChain *pOriginalSwapChain, const std::shared_ptr<ReShade::D3D11Runtime> runtime, const DXGI_SAMPLE_DESC &samples) : mRef(1), mFactory(pFactory), mOrig(pOriginalSwapChain), mOrigSamples(samples), mDirect3DVersion(11), mRuntime(runtime)
		{
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
		virtual HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;

		virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice) override;

		virtual HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override;
		virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) override;
		virtual HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) override;
		virtual HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) override;
		virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) override;
		virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override;
		virtual HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) override;
		virtual HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput) override;
		virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override;
		virtual HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override;

		virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) override;
		virtual HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) override;
		virtual HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) override;
		virtual HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk) override;
		virtual HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) override;
		virtual BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
		virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) override;
		virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override;
		virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override;
		virtual HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
		virtual HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override;

		ULONG mRef;
		IDXGIFactory *const mFactory;
		IDXGISwapChain *const mOrig;
		const DXGI_SAMPLE_DESC mOrigSamples;
		const unsigned int mDirect3DVersion;
		std::shared_ptr<ReShade::Runtime> mRuntime;
	};

	LPCSTR GetErrorString(HRESULT hr)
	{
		switch (hr)
		{
			case DXGI_ERROR_INVALID_CALL:
				return "DXGI_ERROR_INVALID_CALL";
			case DXGI_ERROR_UNSUPPORTED:
				return "DXGI_ERROR_UNSUPPORTED";
			default:
				__declspec(thread) static CHAR buf[20];
				sprintf_s(buf, "0x%lx", hr);
				return buf;
		}
	}
	bool AdjustPresentParameters(DXGI_SWAP_CHAIN_DESC &desc)
	{
		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";

			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
		}

		return true;
	}
	bool AdjustPresentParameters(DXGI_SWAP_CHAIN_DESC1 &desc)
	{
		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";

			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
		}

		return true;
	}

	static const GUID sRuntimeGUID = { 0xff97cb62, 0x2b9e, 0x4792, { 0xb2, 0x87, 0x82, 0x3a, 0x71, 0x9, 0x57, 0x20 } };
}

// -----------------------------------------------------------------------------------------------------

// IDXGISwapChain
HRESULT STDMETHODCALLTYPE DXGISwapChain::QueryInterface(REFIID riid, void **ppvObj)
{
	const HRESULT hr = this->mOrig->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr) && (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) || riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1)))
	{
		this->mRef++;

		*ppvObj = this;
	}

	return hr;
}
ULONG STDMETHODCALLTYPE DXGISwapChain::AddRef()
{
	++this->mRef;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGISwapChain::Release()
{
	if (--this->mRef == 0)
	{
		assert(this->mRuntime != nullptr);

		if (this->mDirect3DVersion == 10)
		{
			std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->OnDeleteInternal();
		}
		else if (this->mDirect3DVersion == 11)
		{
			std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->OnDeleteInternal();
		}

		this->mRuntime.reset();
	}

	const ULONG ref = this->mOrig->Release();

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return this->mOrig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return this->mOrig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return this->mOrig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetParent(REFIID riid, void **ppParent)
{
	return this->mOrig->GetParent(riid, ppParent);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDevice(REFIID riid, void **ppDevice)
{
	return this->mOrig->GetDevice(riid, ppDevice);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	assert(this->mRuntime != nullptr);

	switch (this->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->OnPresentInternal();
			break;
		case 11:
			std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->OnPresentInternal();
			break;
	}

	return this->mOrig->Present(SyncInterval, Flags);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
	if (ppSurface == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	if (Buffer == 0)
	{
		IUnknown *texture = nullptr;

		switch (this->mDirect3DVersion)
		{
			case 10:
				assert(riid == __uuidof(ID3D10Texture2D));
				std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->ReplaceBackBuffer(reinterpret_cast<ID3D10Texture2D *&>(texture));
				break;
			case 11:
				assert(riid == __uuidof(ID3D11Texture2D));
				std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->ReplaceBackBuffer(reinterpret_cast<ID3D11Texture2D *&>(texture));
				break;
		}

		if (texture != nullptr)
		{
			texture->AddRef();

			*ppSurface = texture;

			return S_OK;
		}
	}

	return this->mOrig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::SetFullscreenState" << "(" << this << ", " << Fullscreen << ", " << pTarget << ")' ...";

	return this->mOrig->SetFullscreenState(Fullscreen, pTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget)
{
	return this->mOrig->GetFullscreenState(pFullscreen, ppTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	const HRESULT hr = this->mOrig->GetDesc(pDesc);

	if (SUCCEEDED(hr))
	{
		pDesc->SampleDesc = this->mOrigSamples;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << SwapChainFlags << ")' ...";

	DXGI_SWAP_CHAIN_DESC desc;
	this->mOrig->GetDesc(&desc);

	desc.BufferCount = BufferCount;
	desc.BufferDesc.Width = Width;
	desc.BufferDesc.Height = Height;
	desc.BufferDesc.Format = NewFormat;

	if (!AdjustPresentParameters(desc))
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	assert(this->mRuntime != nullptr);

	switch (this->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->OnDeleteInternal();
			break;
		case 11:
			std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->OnDeleteInternal();
			break;
	}

	const HRESULT hr = this->mOrig->ResizeBuffers(desc.BufferCount, desc.BufferDesc.Width, desc.BufferDesc.Height, desc.BufferDesc.Format, SwapChainFlags);

	if (SUCCEEDED(hr) || hr == DXGI_ERROR_INVALID_CALL)
	{
		if (hr == DXGI_ERROR_INVALID_CALL)
		{
			LOG(WARNING) << "'IDXGISwapChain::ResizeBuffers' failed with 'DXGI_ERROR_INVALID_CALL'!";
		}

		this->mOrig->GetDesc(&desc);
		desc.SampleDesc = this->mOrigSamples;

		switch (this->mDirect3DVersion)
		{
			case 10:
				std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->OnCreateInternal(desc);
				break;
			case 11:
				std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->OnCreateInternal(desc);
				break;
		}
	}
	else
	{
		LOG(ERROR) << "'IDXGISwapChain::ResizeBuffers' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters)
{
	return this->mOrig->ResizeTarget(pNewTargetParameters);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetContainingOutput(IDXGIOutput **ppOutput)
{
	return this->mOrig->GetContainingOutput(ppOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats)
{
	return this->mOrig->GetFrameStatistics(pStats);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetLastPresentCount(UINT *pLastPresentCount)	
{
	return this->mOrig->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	const HRESULT hr = static_cast<IDXGISwapChain1 *>(this->mOrig)->GetDesc1(pDesc);

	if (SUCCEEDED(hr))
	{
		pDesc->SampleDesc = this->mOrigSamples;
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetFullscreenDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetHwnd(HWND *pHwnd)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetHwnd(pHwnd);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetCoreWindow(REFIID refiid, void **ppUnk)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetCoreWindow(refiid, ppUnk);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	assert(this->mRuntime != nullptr);

	switch (this->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::D3D10Runtime>(this->mRuntime)->OnPresentInternal();
			break;
		case 11:
			std::static_pointer_cast<ReShade::D3D11Runtime>(this->mRuntime)->OnPresentInternal();
			break;
	}

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
}
BOOL STDMETHODCALLTYPE DXGISwapChain::IsTemporaryMonoSupported()
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetBackgroundColor(const DXGI_RGBA *pColor)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBackgroundColor(DXGI_RGBA *pColor)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetRotation(DXGI_MODE_ROTATION Rotation)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetRotation(Rotation);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRotation(DXGI_MODE_ROTATION *pRotation)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRotation(pRotation);
}

// ID3D10Device
void STDMETHODCALLTYPE ID3D10Device_DrawIndexed(ID3D10Device *pDevice, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_DrawIndexed);

	ReShade::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

	assert(runtime != nullptr);

	runtime->OnDrawInternal(IndexCount);

	trampoline(pDevice, IndexCount, StartIndexLocation, BaseVertexLocation);
}
void STDMETHODCALLTYPE ID3D10Device_Draw(ID3D10Device *pDevice, UINT VertexCount, UINT StartVertexLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_Draw);

	ReShade::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

	assert(runtime != nullptr);

	runtime->OnDrawInternal(VertexCount);

	trampoline(pDevice, VertexCount, StartVertexLocation);
}
void STDMETHODCALLTYPE ID3D10Device_DrawIndexedInstanced(ID3D10Device *pDevice, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_DrawIndexedInstanced);

	ReShade::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

	assert(runtime != nullptr);

	runtime->OnDrawInternal(IndexCountPerInstance * InstanceCount);

	trampoline(pDevice, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D10Device_DrawInstanced(ID3D10Device *pDevice, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_DrawInstanced);

	ReShade::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

	assert(runtime != nullptr);

	runtime->OnDrawInternal(VertexCountPerInstance * InstanceCount);

	trampoline(pDevice, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D10Device_OMSetRenderTargets(ID3D10Device *pDevice, UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_OMSetRenderTargets);

	if (pDepthStencilView != nullptr)
	{
		ReShade::D3D10Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

		assert(runtime != nullptr);

		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDevice, NumViews, ppRenderTargetViews, pDepthStencilView);
}
void STDMETHODCALLTYPE ID3D10Device_CopyResource(ID3D10Device *pDevice, ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_CopyResource);

	ReShade::D3D10Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

	assert(runtime != nullptr);

	runtime->ReplaceDepthStencilResource(pDstResource);
	runtime->ReplaceDepthStencilResource(pSrcResource);

	trampoline(pDevice, pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE ID3D10Device_ClearDepthStencilView(ID3D10Device *pDevice, ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_ClearDepthStencilView);

	if (pDepthStencilView != nullptr)
	{
		ReShade::D3D10Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

		assert(runtime != nullptr);

		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDevice, pDepthStencilView, ClearFlags, Depth, Stencil);
}
HRESULT STDMETHODCALLTYPE ID3D10Device_CreateDepthStencilView(ID3D10Device *pDevice, ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
	static const auto trampoline = ReHook::Call(&ID3D10Device_CreateDepthStencilView);

	const HRESULT hr = trampoline(pDevice, pResource, pDesc, ppDepthStencilView);

	if (SUCCEEDED(hr))
	{
		ReShade::D3D10Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

		assert(runtime != nullptr);

		runtime->OnCreateDepthStencil(pResource, *ppDepthStencilView);
	}

	return hr;
}

// ID3D11Device
HRESULT STDMETHODCALLTYPE ID3D11Device_CreateDepthStencilView(ID3D11Device *pDevice, ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView)
{
	static const auto trampoline = ReHook::Call(&ID3D11Device_CreateDepthStencilView);

	const HRESULT hr = trampoline(pDevice, pResource, pDesc, ppDepthStencilView);

	if (SUCCEEDED(hr))
	{
		ReShade::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		pDevice->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));

		assert(runtime != nullptr);

		runtime->OnCreateDepthStencil(pResource, *ppDepthStencilView);
	}

	return hr;
}

// ID3D11DeviceContext
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawIndexed(ID3D11DeviceContext *pDeviceContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_DrawIndexed);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
	device->Release();

	assert(runtime != nullptr);

	runtime->OnDrawInternal(pDeviceContext, IndexCount);

	trampoline(pDeviceContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_Draw(ID3D11DeviceContext *pDeviceContext, UINT VertexCount, UINT StartVertexLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_Draw);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
	device->Release();

	assert(runtime != nullptr);

	runtime->OnDrawInternal(pDeviceContext, VertexCount);

	trampoline(pDeviceContext, VertexCount, StartVertexLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawIndexedInstanced(ID3D11DeviceContext *pDeviceContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_DrawIndexedInstanced);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
	device->Release();

	assert(runtime != nullptr);

	runtime->OnDrawInternal(pDeviceContext, IndexCountPerInstance * InstanceCount);

	trampoline(pDeviceContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_DrawInstanced(ID3D11DeviceContext *pDeviceContext, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_DrawInstanced);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
	device->Release();

	assert(runtime != nullptr);

	runtime->OnDrawInternal(pDeviceContext, VertexCountPerInstance * InstanceCount);

	trampoline(pDeviceContext, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargets(ID3D11DeviceContext *pDeviceContext, UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_OMSetRenderTargets);

	if (pDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
		device->Release();

		assert(runtime != nullptr);

		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDeviceContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(ID3D11DeviceContext *pDeviceContext, UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews, const UINT *pUAVInitialCounts)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews);

	if (pDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
		device->Release();

		assert(runtime != nullptr);

		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDeviceContext, NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_CopyResource(ID3D11DeviceContext *pDeviceContext, ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_CopyResource);

	ID3D11Device *device = nullptr;
	pDeviceContext->GetDevice(&device);

	assert(device != nullptr);

	ReShade::D3D11Runtime *runtime = nullptr;
	UINT size = sizeof(runtime);
	device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
	device->Release();

	assert(runtime != nullptr);

	runtime->ReplaceDepthStencilResource(pDstResource);
	runtime->ReplaceDepthStencilResource(pSrcResource);

	trampoline(pDeviceContext, pDstResource, pSrcResource);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_ClearDepthStencilView(ID3D11DeviceContext *pDeviceContext, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	static const auto trampoline = ReHook::Call(&ID3D11DeviceContext_ClearDepthStencilView);

	if (pDepthStencilView != nullptr)
	{
		ID3D11Device *device = nullptr;
		pDeviceContext->GetDevice(&device);

		assert(device != nullptr);

		ReShade::D3D11Runtime *runtime = nullptr;
		UINT size = sizeof(runtime);
		device->GetPrivateData(sRuntimeGUID, &size, reinterpret_cast<void *>(&runtime));
		device->Release();

		assert(runtime != nullptr);

		runtime->ReplaceDepthStencil(pDepthStencilView);
	}

	trampoline(pDeviceContext, pDepthStencilView, ClearFlags, Depth, Stencil);
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;

	if (!AdjustPresentParameters(desc))
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	const HRESULT hr = ReHook::Call(&IDXGIFactory_CreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain *swapchain = *ppSwapChain;

		swapchain->GetDesc(&desc);
		desc.SampleDesc = pDesc->SampleDesc;

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			ReHook::Register(VTABLE(deviceD3D10, 8), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexed));
			ReHook::Register(VTABLE(deviceD3D10, 9), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_Draw));
			ReHook::Register(VTABLE(deviceD3D10, 14), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 15), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 24), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceD3D10, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CopyResource));
			ReHook::Register(VTABLE(deviceD3D10, 36), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
			ReHook::Register(VTABLE(deviceD3D10, 77), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CreateDepthStencilView));

			const std::shared_ptr<ReShade::D3D10Runtime> runtime = std::make_shared<ReShade::D3D10Runtime>(deviceD3D10, swapchain);

			ReShade::D3D10Runtime *runtimePtr = runtime.get();
			deviceD3D10->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			ID3D11DeviceContext *deviceContext = nullptr;
			deviceD3D11->GetImmediateContext(&deviceContext);

			ReHook::Register(VTABLE(deviceD3D11, 10), reinterpret_cast<ReHook::Hook::Function>(&ID3D11Device_CreateDepthStencilView));
			ReHook::Register(VTABLE(deviceContext, 12), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexed));
			ReHook::Register(VTABLE(deviceContext, 13), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_Draw));
			ReHook::Register(VTABLE(deviceContext, 20), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceContext, 21), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawInstanced));
			ReHook::Register(VTABLE(deviceContext, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceContext, 34), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews));
			ReHook::Register(VTABLE(deviceContext, 47), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_CopyResource));
			ReHook::Register(VTABLE(deviceContext, 53), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_ClearDepthStencilView));

			const std::shared_ptr<ReShade::D3D11Runtime> runtime = std::make_shared<ReShade::D3D11Runtime>(deviceD3D11, swapchain);

			ReShade::D3D11Runtime *runtimePtr = runtime.get();
			deviceD3D11->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceContext->Release();
			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory::CreateSwapChain' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// IDXGIFactory2
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";
	
	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	if (!AdjustPresentParameters(desc))
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	const HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		DXGI_SWAP_CHAIN_DESC desc;
		swapchain->GetDesc(&desc);
		desc.SampleDesc = pDesc->SampleDesc;

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			ReHook::Register(VTABLE(deviceD3D10, 8), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexed));
			ReHook::Register(VTABLE(deviceD3D10, 9), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_Draw));
			ReHook::Register(VTABLE(deviceD3D10, 14), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 15), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 24), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceD3D10, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CopyResource));
			ReHook::Register(VTABLE(deviceD3D10, 36), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
			ReHook::Register(VTABLE(deviceD3D10, 77), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CreateDepthStencilView));

			const std::shared_ptr<ReShade::D3D10Runtime> runtime = std::make_shared<ReShade::D3D10Runtime>(deviceD3D10, swapchain);

			ReShade::D3D10Runtime *runtimePtr = runtime.get();
			deviceD3D10->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			ID3D11DeviceContext *deviceContext = nullptr;
			deviceD3D11->GetImmediateContext(&deviceContext);

			ReHook::Register(VTABLE(deviceD3D11, 10), reinterpret_cast<ReHook::Hook::Function>(&ID3D11Device_CreateDepthStencilView));
			ReHook::Register(VTABLE(deviceContext, 12), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexed));
			ReHook::Register(VTABLE(deviceContext, 13), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_Draw));
			ReHook::Register(VTABLE(deviceContext, 20), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceContext, 21), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawInstanced));
			ReHook::Register(VTABLE(deviceContext, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceContext, 34), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews));
			ReHook::Register(VTABLE(deviceContext, 47), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_CopyResource));
			ReHook::Register(VTABLE(deviceContext, 53), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_ClearDepthStencilView));

			const std::shared_ptr<ReShade::D3D11Runtime> runtime = std::make_shared<ReShade::D3D11Runtime>(deviceD3D11, swapchain);

			ReShade::D3D11Runtime *runtimePtr = runtime.get();
			deviceD3D11->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceContext->Release();
			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForHwnd' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	if (!AdjustPresentParameters(desc))
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	const HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		DXGI_SWAP_CHAIN_DESC desc;
		swapchain->GetDesc(&desc);
		desc.SampleDesc = pDesc->SampleDesc;

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			ReHook::Register(VTABLE(deviceD3D10, 8), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexed));
			ReHook::Register(VTABLE(deviceD3D10, 9), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_Draw));
			ReHook::Register(VTABLE(deviceD3D10, 14), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 15), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 24), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceD3D10, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CopyResource));
			ReHook::Register(VTABLE(deviceD3D10, 36), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
			ReHook::Register(VTABLE(deviceD3D10, 77), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CreateDepthStencilView));

			const std::shared_ptr<ReShade::D3D10Runtime> runtime = std::make_shared<ReShade::D3D10Runtime>(deviceD3D10, swapchain);

			ReShade::D3D10Runtime *runtimePtr = runtime.get();
			deviceD3D10->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			ID3D11DeviceContext *deviceContext = nullptr;
			deviceD3D11->GetImmediateContext(&deviceContext);

			ReHook::Register(VTABLE(deviceD3D11, 10), reinterpret_cast<ReHook::Hook::Function>(&ID3D11Device_CreateDepthStencilView));
			ReHook::Register(VTABLE(deviceContext, 12), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexed));
			ReHook::Register(VTABLE(deviceContext, 13), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_Draw));
			ReHook::Register(VTABLE(deviceContext, 20), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceContext, 21), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawInstanced));
			ReHook::Register(VTABLE(deviceContext, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceContext, 34), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews));
			ReHook::Register(VTABLE(deviceContext, 47), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_CopyResource));
			ReHook::Register(VTABLE(deviceContext, 53), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_ClearDepthStencilView));

			const std::shared_ptr<ReShade::D3D11Runtime> runtime = std::make_shared<ReShade::D3D11Runtime>(deviceD3D11, swapchain);

			ReShade::D3D11Runtime *runtimePtr = runtime.get();
			deviceD3D11->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceContext->Release();
			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForCoreWindow' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	if (!AdjustPresentParameters(desc))
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	const HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);

	if (SUCCEEDED(hr))
	{
		#pragma region Skip uninteresting swapchains
		if ((pDesc->BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
		{
			LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";

			return hr;
		}
		#pragma endregion

		ID3D10Device *deviceD3D10 = nullptr;
		ID3D11Device *deviceD3D11 = nullptr;
		IDXGISwapChain1 *swapchain = *ppSwapChain;

		DXGI_SWAP_CHAIN_DESC desc;
		swapchain->GetDesc(&desc);
		desc.SampleDesc = pDesc->SampleDesc;

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			ReHook::Register(VTABLE(deviceD3D10, 8), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexed));
			ReHook::Register(VTABLE(deviceD3D10, 9), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_Draw));
			ReHook::Register(VTABLE(deviceD3D10, 14), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 15), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_DrawInstanced));
			ReHook::Register(VTABLE(deviceD3D10, 24), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceD3D10, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CopyResource));
			ReHook::Register(VTABLE(deviceD3D10, 36), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_ClearDepthStencilView));
			ReHook::Register(VTABLE(deviceD3D10, 77), reinterpret_cast<ReHook::Hook::Function>(&ID3D10Device_CreateDepthStencilView));

			const std::shared_ptr<ReShade::D3D10Runtime> runtime = std::make_shared<ReShade::D3D10Runtime>(deviceD3D10, swapchain);

			ReShade::D3D10Runtime *runtimePtr = runtime.get();
			deviceD3D10->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			ID3D11DeviceContext *deviceContext = nullptr;
			deviceD3D11->GetImmediateContext(&deviceContext);

			ReHook::Register(VTABLE(deviceD3D11, 10), reinterpret_cast<ReHook::Hook::Function>(&ID3D11Device_CreateDepthStencilView));
			ReHook::Register(VTABLE(deviceContext, 12), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexed));
			ReHook::Register(VTABLE(deviceContext, 13), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_Draw));
			ReHook::Register(VTABLE(deviceContext, 20), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawIndexedInstanced));
			ReHook::Register(VTABLE(deviceContext, 21), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_DrawInstanced));
			ReHook::Register(VTABLE(deviceContext, 33), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargets));
			ReHook::Register(VTABLE(deviceContext, 34), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews));
			ReHook::Register(VTABLE(deviceContext, 47), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_CopyResource));
			ReHook::Register(VTABLE(deviceContext, 53), reinterpret_cast<ReHook::Hook::Function>(&ID3D11DeviceContext_ClearDepthStencilView));

			const std::shared_ptr<ReShade::D3D11Runtime> runtime = std::make_shared<ReShade::D3D11Runtime>(deviceD3D11, swapchain);

			ReShade::D3D11Runtime *runtimePtr = runtime.get();
			deviceD3D11->SetPrivateData(sRuntimeGUID, sizeof(runtimePtr), reinterpret_cast<const void *>(&runtimePtr));

			runtime->OnCreateInternal(desc);

			*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime, desc.SampleDesc);

			deviceContext->Release();
			deviceD3D11->Release();
		}
		else
		{
			LOG(WARNING) << "> Swapchain was created without a Direct3D device. Skipping ...";
		}
	}
	else
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForComposition' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}

// D3DKMT
EXPORT NTSTATUS WINAPI D3DKMTCreateAllocation(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTQueryResourceInfo(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTOpenResource(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTDestroyAllocation(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetAllocationPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTQueryAllocationResidency(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTCreateDevice(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTDestroyDevice(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTCreateContext(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTDestroyContext(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTCreateSynchronizationObject(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTWaitForSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSignalSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTLock(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTUnlock(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetDisplayModeList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetDisplayMode(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetMultisampleMethodList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTPresent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTRender(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetRuntimeData(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTQueryAdapterInfo(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTOpenAdapterFromHdc(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTCloseAdapter(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetSharedPrimaryHandle(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTEscape(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetVidPnSourceOwner(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTWaitForVerticalBlankEvent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetGammaRamp(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetDeviceState(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetContextSchedulingPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTGetContextSchedulingPriority(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI D3DKMTSetDisplayPrivateDriverFormat(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}

// DXGI
EXPORT HRESULT WINAPI DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, void **ppDevice)
{
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(pFactory);
	UNREFERENCED_PARAMETER(pAdapter);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(pUnknown);
	UNREFERENCED_PARAMETER(ppDevice);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI DXGID3D10CreateLayeredDevice(BYTE pUnknown[20])
{
	UNREFERENCED_PARAMETER(pUnknown);

	assert(false);

	return E_NOTIMPL;
}
EXPORT SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return 0;
}
EXPORT HRESULT WINAPI DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI DXGIReportAdapterConfiguration()
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI DXGIDumpJournal()
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI OpenAdapter10(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI OpenAdapter10_2(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}

EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory" << "(" << guid << ", " << ppFactory << ")' ...";
	
	const HRESULT hr = ReHook::Call(&CreateDXGIFactory)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTABLE(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory1" << "(" << guid << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReHook::Call(&CreateDXGIFactory1)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTABLE(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory2" << "(" << flags << ", " << guid << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReHook::Call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTABLE(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));

		if (riid == __uuidof(IDXGIFactory2))
		{
			ReHook::Register(VTABLE(*ppFactory, 15), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
			ReHook::Register(VTABLE(*ppFactory, 16), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
			ReHook::Register(VTABLE(*ppFactory, 24), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));
		}
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}