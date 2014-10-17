#include "Log.hpp"
#include "Runtime.hpp"
#include "HookManager.hpp"

#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>

// -----------------------------------------------------------------------------------------------------

namespace
{
	struct														DXGISwapChain : public IDXGISwapChain1
	{
		DXGISwapChain(IDXGIFactory *pFactory, IDXGISwapChain *pOriginalSwapChain, const std::shared_ptr<ReShade::Runtime> runtime) : mRef(1), mFactory(pFactory), mOrig(pOriginalSwapChain), mRuntime(runtime)
		{
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG	STDMETHODCALLTYPE AddRef(void) override;
		virtual ULONG	STDMETHODCALLTYPE Release(void) override;

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
		virtual BOOL	STDMETHODCALLTYPE IsTemporaryMonoSupported(void) override;
		virtual HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) override;
		virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override;
		virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override;
		virtual HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
		virtual HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override;

		ULONG mRef;
		IDXGIFactory *mFactory;
		IDXGISwapChain *mOrig;
		std::shared_ptr<ReShade::Runtime> mRuntime;
	};

	LPCSTR														GetErrorString(HRESULT hr)
	{
		switch (hr)
		{
			default:
				__declspec(thread) static CHAR buf[20];
				sprintf_s(buf, "0x%lx", hr);
				return buf;
			case DXGI_ERROR_INVALID_CALL:
				return "DXGI_ERROR_INVALID_CALL";
			case DXGI_ERROR_UNSUPPORTED:
				return "DXGI_ERROR_UNSUPPORTED";
		}
	}
}
namespace ReShade
{
	extern std::shared_ptr<ReShade::Runtime>					CreateEffectRuntime(ID3D10Device *device, IDXGISwapChain *swapchain);
	extern std::shared_ptr<ReShade::Runtime>					CreateEffectRuntime(ID3D11Device *device, IDXGISwapChain *swapchain);
}

// -----------------------------------------------------------------------------------------------------

// IDXGISwapChain
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::QueryInterface(REFIID riid, void **ppvObj)
{
	HRESULT hr = this->mOrig->QueryInterface(riid, ppvObj);

	if (SUCCEEDED(hr) && (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) || riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1)))
	{
		this->mRef++;

		*ppvObj = this;
	}

	return hr;
}
ULONG STDMETHODCALLTYPE 										DXGISwapChain::AddRef(void)
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE 										DXGISwapChain::Release(void)
{
	if (--this->mRef == 0)
	{
		assert(this->mRuntime != nullptr);

		this->mRuntime.reset();
	}

	ULONG ref = this->mOrig->Release();

	if (ref == 0)
	{
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return this->mOrig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return this->mOrig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return this->mOrig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetParent(REFIID riid, void **ppParent)
{
	return this->mOrig->GetParent(riid, ppParent);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetDevice(REFIID riid, void **ppDevice)
{
	return this->mOrig->GetDevice(riid, ppDevice);
}
HRESULT STDMETHODCALLTYPE										DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	assert(this->mRuntime != nullptr);

	this->mRuntime->OnPostProcess();
	this->mRuntime->OnPresent();

	return this->mOrig->Present(SyncInterval, Flags);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
	return this->mOrig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE										DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::SetFullscreenState" << "(" << this << ", " << Fullscreen << ", " << pTarget << ")' ...";

	return this->mOrig->SetFullscreenState(Fullscreen, pTarget);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget)
{
	return this->mOrig->GetFullscreenState(pFullscreen, ppTarget);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	return this->mOrig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE										DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << SwapChainFlags << ")' ...";

	switch (NewFormat)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' ...";
			NewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' ...";
			NewFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	assert(this->mRuntime != nullptr);

	this->mRuntime->OnDelete();

	HRESULT hr = this->mOrig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(hr) || hr == DXGI_ERROR_INVALID_CALL)
	{
		if (hr == DXGI_ERROR_INVALID_CALL)
		{
			LOG(WARNING) << "'IDXGISwapChain::ResizeBuffers' failed with 'DXGI_ERROR_INVALID_CALL'!";
		}

		DXGI_SWAP_CHAIN_DESC desc;
		this->mOrig->GetDesc(&desc);

		this->mRuntime->OnCreate(desc.BufferDesc.Width, desc.BufferDesc.Height);
	}
	else
	{
		LOG(ERROR) << "'IDXGISwapChain::ResizeBuffers' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters)
{
	return this->mOrig->ResizeTarget(pNewTargetParameters);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetContainingOutput(IDXGIOutput **ppOutput)
{
	return this->mOrig->GetContainingOutput(ppOutput);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats)
{
	return this->mOrig->GetFrameStatistics(pStats);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetLastPresentCount(UINT *pLastPresentCount)	
{
	return this->mOrig->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetDesc1(pDesc);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetFullscreenDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetHwnd(HWND *pHwnd)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetHwnd(pHwnd);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetCoreWindow(REFIID refiid, void **ppUnk)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetCoreWindow(refiid, ppUnk);
}
HRESULT STDMETHODCALLTYPE										DXGISwapChain::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	assert(this->mRuntime != nullptr);

	this->mRuntime->OnPostProcess();
	this->mRuntime->OnPresent();

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
}
BOOL STDMETHODCALLTYPE 											DXGISwapChain::IsTemporaryMonoSupported(void)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::SetBackgroundColor(const DXGI_RGBA *pColor)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetBackgroundColor(DXGI_RGBA *pColor)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::SetRotation(DXGI_MODE_ROTATION Rotation)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetRotation(Rotation);
}
HRESULT STDMETHODCALLTYPE 										DXGISwapChain::GetRotation(DXGI_MODE_ROTATION *pRotation)
{
	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRotation(pRotation);
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE										IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;

	switch (desc.BufferDesc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' ...";
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' ...";
			desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory_CreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);

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

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.BufferDesc.Width, desc.BufferDesc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.BufferDesc.Width, desc.BufferDesc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

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
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";
	
	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

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

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

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
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);

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

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

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
HRESULT STDMETHODCALLTYPE										IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	assert(pDesc != nullptr);

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

	switch (desc.Format)
	{
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM' with 'DXGI_FORMAT_R8G8B8A8_UNORM' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			LOG(WARNING) << "> Replacing buffer format 'DXGI_FORMAT_B8G8R8A8_UNORM_SRGB' with 'DXGI_FORMAT_R8G8B8A8_UNORM_SRGB' ...";
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			break;
	}

	HRESULT hr = ReHook::Call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);

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

		swapchain->GetDesc1(&desc);

		if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D10, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 10 renderer on swapchain " << swapchain << "!";
			}

			deviceD3D10->Release();
		}
		else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
		{
			const std::shared_ptr<ReShade::Runtime> runtime = ReShade::CreateEffectRuntime(deviceD3D11, swapchain);

			if (runtime != nullptr)
			{
				runtime->OnCreate(desc.Width, desc.Height);

				*ppSwapChain = new DXGISwapChain(pFactory, swapchain, runtime);
			}
			else
			{
				LOG(ERROR) << "Failed to initialize Direct3D 11 renderer on swapchain " << swapchain << "!";
			}

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
EXPORT NTSTATUS WINAPI											D3DKMTCreateAllocation(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryResourceInfo(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTOpenResource(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyAllocation(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetAllocationPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryAllocationResidency(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateDevice(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyDevice(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateContext(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroyContext(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCreateSynchronizationObject(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTDestroySynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTWaitForSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSignalSynchronizationObject(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTLock(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTUnlock(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetDisplayModeList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetDisplayMode(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetMultisampleMethodList(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTPresent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTRender(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetRuntimeData(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTQueryAdapterInfo(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTOpenAdapterFromHdc(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTCloseAdapter(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetSharedPrimaryHandle(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTEscape(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetVidPnSourceOwner(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTWaitForVerticalBlankEvent(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetGammaRamp(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetDeviceState(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetContextSchedulingPriority(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTGetContextSchedulingPriority(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}
EXPORT NTSTATUS WINAPI											D3DKMTSetDisplayPrivateDriverFormat(const void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return STATUS_ENTRYPOINT_NOT_FOUND;
}

// DXGI
EXPORT HRESULT WINAPI											DXGID3D10CreateDevice(HMODULE hModule, IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags, void *pUnknown, void **ppDevice)
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
EXPORT HRESULT WINAPI											DXGID3D10CreateLayeredDevice(BYTE pUnknown[20])
{
	UNREFERENCED_PARAMETER(pUnknown);

	assert(false);

	return E_NOTIMPL;
}
EXPORT SIZE_T WINAPI											DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return 0;
}
EXPORT HRESULT WINAPI											DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	UNREFERENCED_PARAMETER(pLayers);
	UNREFERENCED_PARAMETER(NumLayers);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											DXGIReportAdapterConfiguration(void)
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											DXGIDumpJournal(void)
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											OpenAdapter10(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI											OpenAdapter10_2(void *pData)
{
	UNREFERENCED_PARAMETER(pData);

	assert(false);

	return E_NOTIMPL;
}

EXPORT HRESULT WINAPI											CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory" << "(" << guid << ", " << ppFactory << ")' ...";
	
	HRESULT hr = ReHook::Call(&CreateDXGIFactory)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI											CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory1" << "(" << guid << ", " << ppFactory << ")' ...";

	HRESULT hr = ReHook::Call(&CreateDXGIFactory1)(riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}
EXPORT HRESULT WINAPI											CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	::StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory2" << "(" << flags << ", " << guid << ", " << ppFactory << ")' ...";

	HRESULT hr = ReHook::Call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (SUCCEEDED(hr))
	{
		ReHook::Register(VTable(*ppFactory, 10), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory_CreateSwapChain));

		if (riid == __uuidof(IDXGIFactory2))
		{
			ReHook::Register(VTable(*ppFactory, 15), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
			ReHook::Register(VTable(*ppFactory, 16), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
			ReHook::Register(VTable(*ppFactory, 24), reinterpret_cast<ReHook::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));
		}
	}
	else
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with '" << GetErrorString(hr) << "'!";
	}

	return hr;
}