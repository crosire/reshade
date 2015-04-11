#include "Log.hpp"
#include "HookManager.hpp"
#include "Runtimes\D3D10Runtime.hpp"
#include "Runtimes\D3D11Runtime.hpp"

#include <dxgi.h>
#include <dxgi1_2.h>

#ifndef _NTDEF_
	typedef LONG NTSTATUS;
#endif

// -----------------------------------------------------------------------------------------------------

namespace
{
	struct DXGIDevice : public IDXGIDevice2, private boost::noncopyable
	{
		DXGIDevice(IDXGIDevice *originalDevice, IUnknown *direct3DBridge, ID3D10Device *direct3DDevice) : mRef(1), mOrig(originalDevice), mInterfaceVersion(0), mDirect3DBridge(direct3DBridge), mDirect3DDevice(direct3DDevice), mDirect3DVersion(10)
		{
			assert(originalDevice != nullptr);
			assert(direct3DBridge != nullptr);
			assert(direct3DDevice != nullptr);
		}
		DXGIDevice(IDXGIDevice *originalDevice, IUnknown *direct3DBridge, ID3D11Device *direct3DDevice) : mRef(1), mOrig(originalDevice), mInterfaceVersion(0), mDirect3DBridge(direct3DBridge), mDirect3DDevice(direct3DDevice), mDirect3DVersion(11)
		{
			assert(originalDevice != nullptr);
			assert(direct3DBridge != nullptr);
			assert(direct3DDevice != nullptr);
		}

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override;
		virtual ULONG STDMETHODCALLTYPE AddRef() override;
		virtual ULONG STDMETHODCALLTYPE Release() override;

		virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override;
		virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override;
		virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override;
		virtual HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override;

		virtual HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter) override;
		virtual HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface) override;
		virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources) override;
		virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override;
		virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override;

		virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override;
		virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override;

		virtual HRESULT STDMETHODCALLTYPE OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority) override;
		virtual HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded) override;
		virtual HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) override;

		LONG mRef;
		IDXGIDevice *mOrig;
		unsigned int mInterfaceVersion;
		IUnknown *const mDirect3DBridge, *const mDirect3DDevice;
		const unsigned int mDirect3DVersion;
	};
	struct DXGISwapChain : public IDXGISwapChain1, private boost::noncopyable
	{
		DXGISwapChain(DXGIDevice *device, IDXGISwapChain *originalSwapChain, const std::shared_ptr<ReShade::Runtime> &runtime, const DXGI_SAMPLE_DESC &samples) : mRef(1), mOrig(originalSwapChain), mInterfaceVersion(0), mDevice(device), mOrigSamples(samples), mRuntime(runtime)
		{
			assert(device != nullptr);
			assert(originalSwapChain != nullptr);
		}
		DXGISwapChain(DXGIDevice *device, IDXGISwapChain1 *originalSwapChain, const std::shared_ptr<ReShade::Runtime> &runtime, const DXGI_SAMPLE_DESC &samples) : mRef(1), mOrig(originalSwapChain), mInterfaceVersion(1), mDevice(device), mOrigSamples(samples), mRuntime(runtime)
		{
			assert(device != nullptr);
			assert(originalSwapChain != nullptr);
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

		LONG mRef;
		IDXGISwapChain *mOrig;
		unsigned int mInterfaceVersion;
		DXGIDevice *const mDevice;
		const DXGI_SAMPLE_DESC mOrigSamples;
		std::shared_ptr<ReShade::Runtime> mRuntime;
	};

	std::string GetErrorString(HRESULT hr)
	{
		std::stringstream res;

		switch (hr)
		{
			case DXGI_ERROR_INVALID_CALL:
				res << "DXGI_ERROR_INVALID_CALL";
				break;
			case DXGI_ERROR_UNSUPPORTED:
				res << "DXGI_ERROR_UNSUPPORTED";
				break;
			default:
				res << std::showbase << std::hex << hr;
				break;
		}

		return res.str();
	}
	void DumpSwapChainDescription(const DXGI_SWAP_CHAIN_DESC &desc)
	{
		LOG(TRACE) << "> Dumping swapchain description:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::left;
		LOG(TRACE) << "  | Width                                   | " << std::setw(39) << desc.BufferDesc.Width << " |";
		LOG(TRACE) << "  | Height                                  | " << std::setw(39) << desc.BufferDesc.Height << " |";
		LOG(TRACE) << "  | RefreshRate                             | " << std::setw(19) << desc.BufferDesc.RefreshRate.Numerator << ' ' << std::setw(19) << desc.BufferDesc.RefreshRate.Denominator << " |";
		LOG(TRACE) << "  | Format                                  | " << std::setw(39) << desc.BufferDesc.Format << " |";
		LOG(TRACE) << "  | ScanlineOrdering                        | " << std::setw(39) << desc.BufferDesc.ScanlineOrdering << " |";
		LOG(TRACE) << "  | Scaling                                 | " << std::setw(39) << desc.BufferDesc.Scaling << " |";
		LOG(TRACE) << "  | SampleCount                             | " << std::setw(39) << desc.SampleDesc.Count << " |";
		LOG(TRACE) << "  | SampleQuality                           | " << std::setw(39) << desc.SampleDesc.Quality << " |";
		LOG(TRACE) << "  | BufferUsage                             | " << std::setw(39) << desc.BufferUsage << " |";
		LOG(TRACE) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount << " |";
		LOG(TRACE) << "  | OutputWindow                            | " << std::setw(39) << desc.OutputWindow << " |";
		LOG(TRACE) << "  | Windowed                                | " << std::setw(39) << (desc.Windowed != FALSE ? "TRUE" : "FALSE") << " |";
		LOG(TRACE) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::showbase << std::hex << desc.Flags << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::internal;

		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";
		}
	}
	void DumpSwapChainDescription(const DXGI_SWAP_CHAIN_DESC1 &desc)
	{
		LOG(TRACE) << "> Dumping swapchain description:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::left;
		LOG(TRACE) << "  | Width                                   | " << std::setw(39) << desc.Width << " |";
		LOG(TRACE) << "  | Height                                  | " << std::setw(39) << desc.Height << " |";
		LOG(TRACE) << "  | Format                                  | " << std::setw(39) << desc.Format << " |";
		LOG(TRACE) << "  | Stereo                                  | " << std::setw(39) << (desc.Stereo != FALSE ? "TRUE" : "FALSE") << " |";
		LOG(TRACE) << "  | SampleCount                             | " << std::setw(39) << desc.SampleDesc.Count << " |";
		LOG(TRACE) << "  | SampleQuality                           | " << std::setw(39) << desc.SampleDesc.Quality << " |";
		LOG(TRACE) << "  | BufferUsage                             | " << std::setw(39) << desc.BufferUsage << " |";
		LOG(TRACE) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount << " |";
		LOG(TRACE) << "  | Scaling                                 | " << std::setw(39) << desc.Scaling << " |";
		LOG(TRACE) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect << " |";
		LOG(TRACE) << "  | AlphaMode                               | " << std::setw(39) << desc.AlphaMode << " |";
		LOG(TRACE) << "  | Flags                                   | " << std::setw(39) << std::showbase << std::hex << desc.Flags << std::dec << std::noshowbase << " |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+" << std::internal;

		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";
		}
	}
}

#pragma region DXGI Bridge
class DXGID3D10Bridge : public IUnknown, private boost::noncopyable
{
public:
	static const IID sIID;

public:
	IDXGIDevice *GetDXGIDevice();
	ID3D10Device *GetOriginalD3D10Device();

	void AddRuntime(const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> &runtime);
	void RemoveRuntime(const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> &runtime);

private:
	ULONG mRef;
	ID3D10Device *mD3D10Device;
};
class DXGID3D11Bridge : public IUnknown, private boost::noncopyable
{
public:
	static const IID sIID;
	
public:
	IDXGIDevice *GetDXGIDevice();
	ID3D11Device *GetOriginalD3D11Device();

	void AddRuntime(const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> &runtime);
	void RemoveRuntime(const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> &runtime);

private:
	ULONG mRef;
	ID3D11Device *mD3D11Device;
};

IDXGIDevice *DXGID3D10Bridge::GetDXGIDevice()
{
	IDXGIDevice *device = nullptr;

	if (FAILED(GetOriginalD3D10Device()->QueryInterface(&device)))
	{
		return nullptr;
	}

	AddRef();

	return new DXGIDevice(device, this, this->mD3D10Device);
}
IDXGIDevice *DXGID3D11Bridge::GetDXGIDevice()
{
	IDXGIDevice *device = nullptr;

	if (FAILED(GetOriginalD3D11Device()->QueryInterface(&device)))
	{
		return nullptr;
	}

	AddRef();

	return new DXGIDevice(device, this, this->mD3D11Device);
}
#pragma endregion

// -----------------------------------------------------------------------------------------------------

// IDXGISwapChain
HRESULT STDMETHODCALLTYPE DXGISwapChain::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) || riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1))
	{
		#pragma region Update to IDXGISwapChain1 interface
		if (riid == __uuidof(IDXGISwapChain1) && this->mInterfaceVersion < 1)
		{
			IDXGISwapChain1 *swapchain1 = nullptr;

			const HRESULT hr = this->mOrig->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void **>(&swapchain1));

			if (FAILED(hr))
			{
				return hr;
			}

			this->mOrig->Release();

			this->mOrig = swapchain1;
			this->mInterfaceVersion = 1;

			LOG(TRACE) << "Upgraded 'IDXGISwapChain' object " << this << " to 'IDXGISwapChain1'.";
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mOrig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGISwapChain::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGISwapChain::Release()
{
	if (--this->mRef == 0)
	{
		#pragma region Cleanup Resources
		assert(this->mRuntime != nullptr);
		assert(this->mDevice->mDirect3DBridge != nullptr);

		switch (this->mDevice->mDirect3DVersion)
		{
			case 10:
				std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnDeleteInternal();
				static_cast<DXGID3D10Bridge *>(this->mDevice->mDirect3DBridge)->RemoveRuntime(std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime));
				break;
			case 11:
				std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnDeleteInternal();
				static_cast<DXGID3D11Bridge *>(this->mDevice->mDirect3DBridge)->RemoveRuntime(std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime));
				break;
		}

		this->mRuntime.reset();

		this->mDevice->Release();
		#pragma endregion
	}

	ULONG ref = this->mOrig->Release();

	if (this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDXGISwapChain" << (this->mInterfaceVersion >= 1 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " is inconsistent: " << ref << " vs " << this->mRef << ".";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'IDXGISwapChain" << (this->mInterfaceVersion >= 1 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << ".";

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
	if (ppDevice == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	return this->mDevice->QueryInterface(riid, ppDevice);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	assert(this->mRuntime != nullptr);

	switch (this->mDevice->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnPresentInternal(SyncInterval);
			break;
		case 11:
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnPresentInternal(SyncInterval);
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

		switch (this->mDevice->mDirect3DVersion)
		{
			case 10:
				std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnGetBackBuffer(reinterpret_cast<ID3D10Texture2D *&>(texture));
				break;
			case 11:
				std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnGetBackBuffer(reinterpret_cast<ID3D11Texture2D *&>(texture));
				break;
		}

		if (texture != nullptr)
		{
			return texture->QueryInterface(riid, ppSurface);
		}
	}

	return this->mOrig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::SetFullscreenState" << "(" << this << ", " << (Fullscreen != FALSE ? "TRUE" : "FALSE") << ", " << pTarget << ")' ...";

	return this->mOrig->SetFullscreenState(Fullscreen, pTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget)
{
	return this->mOrig->GetFullscreenState(pFullscreen, ppTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	const HRESULT hr = this->mOrig->GetDesc(pDesc);

	if (FAILED(hr))
	{
		return hr;
	}

	pDesc->SampleDesc = this->mOrigSamples;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << std::showbase << std::hex << SwapChainFlags << std::dec << std::noshowbase << ")' ...";

	assert(this->mRuntime != nullptr);

	switch (this->mDevice->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnDeleteInternal();
			break;
		case 11:
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnDeleteInternal();
			break;
	}

	const HRESULT hr = this->mOrig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (hr == DXGI_ERROR_INVALID_CALL)
	{
		LOG(WARNING) << "> 'IDXGISwapChain::ResizeBuffers' failed with 'DXGI_ERROR_INVALID_CALL'!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDXGISwapChain::ResizeBuffers' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGI_SWAP_CHAIN_DESC desc;
	this->mOrig->GetDesc(&desc);
	desc.SampleDesc = this->mOrigSamples;

	bool created = false;

	switch (this->mDevice->mDirect3DVersion)
	{
		case 10:
			created = std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnCreateInternal(desc);
			break;
		case 11:
			created = std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnCreateInternal(desc);
			break;
	}

	if (!created)
	{
		LOG(ERROR) << "Failed to resize Direct3D" << this->mDevice->mDirect3DVersion << " renderer! Check tracelog for details.";
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
	assert(this->mInterfaceVersion >= 1);

	const HRESULT hr = static_cast<IDXGISwapChain1 *>(this->mOrig)->GetDesc1(pDesc);

	if (FAILED(hr))
	{
		return hr;
	}

	pDesc->SampleDesc = this->mOrigSamples;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetFullscreenDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetHwnd(HWND *pHwnd)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetHwnd(pHwnd);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetCoreWindow(REFIID refiid, void **ppUnk)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetCoreWindow(refiid, ppUnk);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	assert(this->mInterfaceVersion >= 1);
	assert(this->mRuntime != nullptr);

	switch (this->mDevice->mDirect3DVersion)
	{
		case 10:
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnPresentInternal(SyncInterval);
			break;
		case 11:
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnPresentInternal(SyncInterval);
			break;
	}

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
}
BOOL STDMETHODCALLTYPE DXGISwapChain::IsTemporaryMonoSupported()
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetBackgroundColor(const DXGI_RGBA *pColor)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBackgroundColor(DXGI_RGBA *pColor)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetRotation(DXGI_MODE_ROTATION Rotation)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->SetRotation(Rotation);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRotation(DXGI_MODE_ROTATION *pRotation)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetRotation(pRotation);
}

// IDXGIDevice
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}

	if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDevice) || riid == __uuidof(IDXGIDevice1) || riid == __uuidof(IDXGIDevice2))
	{
		#pragma region Update to IDXGIDevice1 interface
		if (riid == __uuidof(IDXGIDevice1) && this->mInterfaceVersion < 1)
		{
			IDXGIDevice1 *device1 = nullptr;

			const HRESULT hr = this->mOrig->QueryInterface(__uuidof(IDXGIDevice1), reinterpret_cast<void **>(&device1));

			if (FAILED(hr))
			{
				return hr;
			}

			this->mOrig->Release();

			this->mOrig = device1;
			this->mInterfaceVersion = 1;

			LOG(TRACE) << "Upgraded 'IDXGIDevice' object " << this << " to 'IDXGIDevice1'.";
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice2 interface
		if (riid == __uuidof(IDXGIDevice2) && this->mInterfaceVersion < 2)
		{
			IDXGIDevice2 *device2 = nullptr;

			const HRESULT hr = this->mOrig->QueryInterface(__uuidof(IDXGIDevice2), reinterpret_cast<void **>(&device2));

			if (FAILED(hr))
			{
				return hr;
			}

			this->mOrig->Release();

			this->mOrig = device2;
			this->mInterfaceVersion = 2;

			LOG(TRACE) << "Upgraded 'IDXGIDevice' object " << this << " to 'IDXGIDevice2'.";
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	switch (this->mDirect3DVersion)
	{
		case 10:
			if (riid == DXGID3D10Bridge::sIID || riid == __uuidof(ID3D10Device) || riid == __uuidof(ID3D10Device1))
			{
				return this->mDirect3DDevice->QueryInterface(riid, ppvObj);
			}
			break;
		case 11:
			if (riid == DXGID3D11Bridge::sIID || riid == __uuidof(ID3D11Device) || riid == __uuidof(ID3D11Device1))
			{
				return this->mDirect3DDevice->QueryInterface(riid, ppvObj);
			}
			break;
	}

	return this->mOrig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGIDevice::AddRef()
{
	this->mRef++;

	this->mDirect3DDevice->AddRef();

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGIDevice::Release()
{
	if (--this->mRef == 0)
	{
		this->mDirect3DBridge->Release();
	}
	else if (this->mDirect3DDevice->Release() == 0)
	{
		return 0;
	}

	ULONG ref = this->mOrig->Release();

	if (this->mRef == 0 && ref != 1)
	{
		LOG(WARNING) << "Reference count for 'IDXGIDevice" << (this->mInterfaceVersion >= 1 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " is inconsistent: " << ref << " vs " << this->mRef << ".";

		ref = 1;
	}

	if (ref == 1)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'IDXGIDevice" << (this->mInterfaceVersion >= 1 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << ".";

		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return this->mOrig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return this->mOrig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return this->mOrig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetParent(REFIID riid, void **ppParent)
{
	return this->mOrig->GetParent(riid, ppParent);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter)
{
	return this->mOrig->GetAdapter(pAdapter);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::CreateSurface(const DXGI_SURFACE_DESC *pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE *pSharedResource, IDXGISurface **ppSurface)
{
	return this->mOrig->CreateSurface(pDesc, NumSurfaces, Usage, pSharedResource, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryResourceResidency(IUnknown *const *ppResources, DXGI_RESIDENCY *pResidencyStatus, UINT NumResources)
{
	return this->mOrig->QueryResourceResidency(ppResources, pResidencyStatus, NumResources);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::SetGPUThreadPriority(INT Priority)
{
	return this->mOrig->SetGPUThreadPriority(Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetGPUThreadPriority(INT *pPriority)
{
	return this->mOrig->GetGPUThreadPriority(pPriority);
}

// IDXGIDevice1
HRESULT STDMETHODCALLTYPE DXGIDevice::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGIDevice1 *>(this->mOrig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(this->mInterfaceVersion >= 1);

	return static_cast<IDXGIDevice1 *>(this->mOrig)->GetMaximumFrameLatency(pMaxLatency);
}

// IDXGIDevice2
HRESULT STDMETHODCALLTYPE DXGIDevice::OfferResources(UINT NumResources, IDXGIResource *const *ppResources, DXGI_OFFER_RESOURCE_PRIORITY Priority)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGIDevice2 *>(this->mOrig)->OfferResources(NumResources, ppResources, Priority);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::ReclaimResources(UINT NumResources, IDXGIResource *const *ppResources, BOOL *pDiscarded)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGIDevice2 *>(this->mOrig)->ReclaimResources(NumResources, ppResources, pDiscarded);
}
HRESULT STDMETHODCALLTYPE DXGIDevice::EnqueueSetEvent(HANDLE hEvent)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGIDevice2 *>(this->mOrig)->EnqueueSetEvent(hEvent);
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DumpSwapChainDescription(*pDesc);

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory_CreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory::CreateSwapChain' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGID3D10Bridge *bridgeD3D10 = nullptr;
	DXGID3D11Bridge *bridgeD3D11 = nullptr;
	DXGIDevice *deviceProxy = nullptr;
	IDXGISwapChain *const swapchain = *ppSwapChain;

	swapchain->GetDesc(&desc);
	desc.SampleDesc = pDesc->SampleDesc;

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D10Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D10))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(bridgeD3D10->GetOriginalD3D10Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 renderer! Check tracelog for details.";
		}

		bridgeD3D10->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D11Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D11))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(bridgeD3D11->GetOriginalD3D11Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 renderer! Check tracelog for details.";
		}

		bridgeD3D11->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D11->Release();
	}
	else
	{
		LOG(WARNING) << "> Skipping swapchain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swapchain object: " << *ppSwapChain;

	return S_OK;
}

// IDXGIFactory2
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DumpSwapChainDescription(*pDesc);

	DXGI_SWAP_CHAIN_DESC1 desc1 = *pDesc;
	desc1.SampleDesc.Count = 1;
	desc1.SampleDesc.Quality = 0;

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc1, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForHwnd' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGID3D10Bridge *bridgeD3D10 = nullptr;
	DXGID3D11Bridge *bridgeD3D11 = nullptr;
	DXGIDevice *deviceProxy = nullptr;
	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);
	desc.SampleDesc = pDesc->SampleDesc;

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D10Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D10))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(bridgeD3D10->GetOriginalD3D10Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 renderer! Check tracelog for details.";
		}

		bridgeD3D10->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D11Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D11))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(bridgeD3D11->GetOriginalD3D11Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 renderer! Check tracelog for details.";
		}

		bridgeD3D11->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D11->Release();
	}
	else
	{
		LOG(WARNING) << "> Skipping swapchain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swapchain object: " << *ppSwapChain;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DumpSwapChainDescription(*pDesc);

	DXGI_SWAP_CHAIN_DESC1 desc1 = *pDesc;
	desc1.SampleDesc.Count = 1;
	desc1.SampleDesc.Quality = 0;

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc1, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForCoreWindow' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGID3D10Bridge *bridgeD3D10 = nullptr;
	DXGID3D11Bridge *bridgeD3D11 = nullptr;
	DXGIDevice *deviceProxy = nullptr;
	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);
	desc.SampleDesc = pDesc->SampleDesc;

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D10Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D10))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(bridgeD3D10->GetOriginalD3D10Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 renderer! Check tracelog for details.";
		}

		bridgeD3D10->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D11Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D11))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(bridgeD3D11->GetOriginalD3D11Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 renderer! Check tracelog for details.";
		}

		bridgeD3D11->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D11->Release();
	}
	else
	{
		LOG(WARNING) << "> Skipping swapchain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swapchain object: " << *ppSwapChain;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	if (pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	DumpSwapChainDescription(*pDesc);

	DXGI_SWAP_CHAIN_DESC1 desc1 = *pDesc;
	desc1.SampleDesc.Count = 1;
	desc1.SampleDesc.Quality = 0;

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, pDevice, &desc1, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForComposition' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGID3D10Bridge *bridgeD3D10 = nullptr;
	DXGID3D11Bridge *bridgeD3D11 = nullptr;
	DXGIDevice *deviceProxy = nullptr;
	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);
	desc.SampleDesc = pDesc->SampleDesc;

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swapchain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D10Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D10))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(bridgeD3D10->GetOriginalD3D10Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 renderer! Check tracelog for details.";
		}

		bridgeD3D10->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(DXGID3D11Bridge::sIID, reinterpret_cast<void **>(&bridgeD3D11))) && SUCCEEDED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&deviceProxy))))
	{
		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(bridgeD3D11->GetOriginalD3D11Device(), swapchain);

		if (!runtime->OnCreateInternal(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 renderer! Check tracelog for details.";
		}

		bridgeD3D11->AddRuntime(runtime);

		*ppSwapChain = new DXGISwapChain(deviceProxy, swapchain, runtime, desc.SampleDesc);

		bridgeD3D11->Release();
	}
	else
	{
		LOG(WARNING) << "> Skipping swapchain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swapchain object: " << *ppSwapChain;

	return S_OK;
}

// DXGI
EXPORT HRESULT WINAPI DXGIDumpJournal()
{
	assert(false);

	return E_NOTIMPL;
}
EXPORT HRESULT WINAPI DXGIReportAdapterConfiguration()
{
	assert(false);

	return E_NOTIMPL;
}
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
EXPORT HRESULT WINAPI DXGID3D10CreateLayeredDevice(void *pUnknown1, void *pUnknown2, void *pUnknown3, void *pUnknown4, void *pUnknown5)
{
	UNREFERENCED_PARAMETER(pUnknown1);
	UNREFERENCED_PARAMETER(pUnknown2);
	UNREFERENCED_PARAMETER(pUnknown3);
	UNREFERENCED_PARAMETER(pUnknown4);
	UNREFERENCED_PARAMETER(pUnknown5);

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

EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory" << "(" << guid << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory)[10], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2)[15], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2)[16], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2)[24], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}
EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory1" << "(" << guid << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory1)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory)[10], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2)[15], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2)[16], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2)[24], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}
EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR guid[40];
	StringFromGUID2(riid, guid, ARRAYSIZE(guid));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory2" << "(" << flags << ", " << guid << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory)[10], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2)[15], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2)[16], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2)[24], reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}