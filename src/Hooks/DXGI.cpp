#include "Log.hpp"
#include "HookManager.hpp"
#include "Hooks\DXGI.hpp"

#include <sstream>
#include <assert.h>

#define EXPORT extern "C"

// ---------------------------------------------------------------------------------------------------

namespace
{
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
		LOG(TRACE) << "> Dumping swap chain description:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
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
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depth buffer access, which was therefore disabled.";
		}
	}
	void DumpSwapChainDescription(const DXGI_SWAP_CHAIN_DESC1 &desc)
	{
		LOG(TRACE) << "> Dumping swap chain description:";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
		LOG(TRACE) << "  | Parameter                               | Value                                   |";
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";
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
		LOG(TRACE) << "  +-----------------------------------------+-----------------------------------------+";

		if (desc.SampleDesc.Count > 1)
		{
			LOG(WARNING) << "> Multisampling is enabled. This is not compatible with depthbuffer access, which was therefore disabled.";
		}
	}
}

// IDXGISwapChain
HRESULT STDMETHODCALLTYPE DXGISwapChain::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDeviceSubObject) ||
		riid == __uuidof(IDXGISwapChain) ||
		riid == __uuidof(IDXGISwapChain1) ||
		riid == __uuidof(IDXGISwapChain2) ||
		riid == __uuidof(IDXGISwapChain3))
	{
		#pragma region Update to IDXGISwapChain1 interface
		if (riid == __uuidof(IDXGISwapChain1) && this->mInterfaceVersion < 1)
		{
			IDXGISwapChain1 *swapchain1 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&swapchain1)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGISwapChain' object " << this << " to 'IDXGISwapChain1'.";

			this->mOrig = swapchain1;
			this->mInterfaceVersion = 1;
		}
		#pragma endregion
		#pragma region Update to IDXGISwapChain2 interface
		if (riid == __uuidof(IDXGISwapChain2) && this->mInterfaceVersion < 2)
		{
			IDXGISwapChain2 *swapchain2 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&swapchain2)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGISwapChain" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " to 'IDXGISwapChain2'.";

			this->mOrig = swapchain2;
			this->mInterfaceVersion = 2;
		}
		#pragma endregion
		#pragma region Update to IDXGISwapChain3 interface
		if (riid == __uuidof(IDXGISwapChain3) && this->mInterfaceVersion < 3)
		{
			IDXGISwapChain3 *swapchain3 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&swapchain3)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGISwapChain" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " to 'IDXGISwapChain3'.";

			this->mOrig = swapchain3;
			this->mInterfaceVersion = 3;
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
		switch (this->mDirect3DVersion)
		{
			case 10:
			{
				assert(this->mRuntime != nullptr);

				const auto runtime = std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime);
				std::vector<std::shared_ptr<ReShade::Runtimes::D3D10Runtime>> &runtimes = static_cast<D3D10Device *>(this->mDirect3DDevice)->mRuntimes;

				runtime->OnReset();

				runtimes.erase(std::remove(runtimes.begin(), runtimes.end(), runtime), runtimes.end());
				break;
			}
			case 11:
			{
				assert(this->mRuntime != nullptr);

				const auto runtime = std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime);
				std::vector<std::shared_ptr<ReShade::Runtimes::D3D11Runtime>> &runtimes = static_cast<D3D11Device *>(this->mDirect3DDevice)->mRuntimes;

				runtime->OnReset();

				runtimes.erase(std::remove(runtimes.begin(), runtimes.end(), runtime), runtimes.end());
				break;
			}
			case 12:
			{
				assert(this->mRuntime != nullptr);

				const auto runtime = std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime);

				runtime->OnReset();
				break;
			}
		}

		this->mRuntime.reset();

		this->mDirect3DDevice->Release();
	}

	ULONG ref = this->mOrig->Release();

	if (this->mRef == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDXGISwapChain" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'IDXGISwapChain" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << ".";

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

	return this->mDirect3DDevice->QueryInterface(riid, ppDevice);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnPresent();
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnPresent();
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnPresent();
			break;
	}

	return this->mOrig->Present(SyncInterval, Flags);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
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
	return this->mOrig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << std::showbase << std::hex << SwapChainFlags << std::dec << std::noshowbase << ")' ...";

	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnReset();
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnReset();
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnReset();
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

	bool initialized = false;

	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnInit(desc);
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnInit(desc);
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnInit(desc);
			break;
	}

	if (!initialized)
	{
		LOG(ERROR) << "Failed to recreate Direct3D" << this->mDirect3DVersion << " runtime environment on runtime " << this->mRuntime.get() << ".";
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

	return static_cast<IDXGISwapChain1 *>(this->mOrig)->GetDesc1(pDesc);
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

	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnPresent();
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnPresent();
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnPresent();
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

// IDXGISwapChain2
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetSourceSize(UINT Width, UINT Height)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->SetSourceSize(Width, Height);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetSourceSize(UINT *pWidth, UINT *pHeight)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->GetSourceSize(pWidth, pHeight);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->GetMaximumFrameLatency(pMaxLatency);
}
HANDLE STDMETHODCALLTYPE DXGISwapChain::GetFrameLatencyWaitableObject()
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->GetFrameLatencyWaitableObject();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->SetMatrixTransform(pMatrix);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix)
{
	assert(this->mInterfaceVersion >= 2);

	return static_cast<IDXGISwapChain2 *>(this->mOrig)->GetMatrixTransform(pMatrix);
}

// IDXGISwapChain3
UINT STDMETHODCALLTYPE DXGISwapChain::GetCurrentBackBufferIndex()
{
	assert(this->mInterfaceVersion >= 3);

	return static_cast<IDXGISwapChain3 *>(this->mOrig)->GetCurrentBackBufferIndex();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport)
{
	assert(this->mInterfaceVersion >= 3);

	return static_cast<IDXGISwapChain3 *>(this->mOrig)->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
	assert(this->mInterfaceVersion >= 3);

	return static_cast<IDXGISwapChain3 *>(this->mOrig)->SetColorSpace1(ColorSpace);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue)
{
	assert(this->mInterfaceVersion >= 3);

	LOG(INFO) << "Redirecting '" << "IDXGISwapChain3::ResizeBuffers1" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << Format << ", " << std::showbase << std::hex << SwapChainFlags << std::dec << std::noshowbase << ", " << pCreationNodeMask << ", " << ppPresentQueue << ")' ...";

	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnReset();
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnReset();
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnReset();
			break;
	}

	const HRESULT hr = static_cast<IDXGISwapChain3 *>(this->mOrig)->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);

	if (hr == DXGI_ERROR_INVALID_CALL)
	{
		LOG(WARNING) << "> 'IDXGISwapChain3::ResizeBuffers1' failed with 'DXGI_ERROR_INVALID_CALL'!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDXGISwapChain3::ResizeBuffers1' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	DXGI_SWAP_CHAIN_DESC desc;
	this->mOrig->GetDesc(&desc);

	bool initialized = false;

	switch (this->mDirect3DVersion)
	{
		case 10:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D10Runtime>(this->mRuntime)->OnInit(desc);
			break;
		case 11:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D11Runtime>(this->mRuntime)->OnInit(desc);
			break;
		case 12:
			assert(this->mRuntime != nullptr);
			initialized = std::static_pointer_cast<ReShade::Runtimes::D3D12Runtime>(this->mRuntime)->OnInit(desc);
			break;
	}

	if (!initialized)
	{
		LOG(ERROR) << "Failed to recreate Direct3D" << this->mDirect3DVersion << " runtime environment on runtime " << this->mRuntime.get() << ".";
	}

	return hr;
}

// IDXGIDevice
HRESULT STDMETHODCALLTYPE DXGIDevice::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
	{
		return E_POINTER;
	}
	else if (
		riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDevice) ||
		riid == __uuidof(IDXGIDevice1) ||
		riid == __uuidof(IDXGIDevice2) ||
		riid == __uuidof(IDXGIDevice3))
	{
		#pragma region Update to IDXGIDevice1 interface
		if (riid == __uuidof(IDXGIDevice1) && this->mInterfaceVersion < 1)
		{
			IDXGIDevice1 *device1 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&device1)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGIDevice' object " << this << " to 'IDXGIDevice1'.";

			this->mOrig = device1;
			this->mInterfaceVersion = 1;
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice2 interface
		if (riid == __uuidof(IDXGIDevice2) && this->mInterfaceVersion < 2)
		{
			IDXGIDevice2 *device2 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&device2)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGIDevice" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " to 'IDXGIDevice2'.";

			this->mOrig = device2;
			this->mInterfaceVersion = 2;
		}
		#pragma endregion
		#pragma region Update to IDXGIDevice3 interface
		if (riid == __uuidof(IDXGIDevice3) && this->mInterfaceVersion < 3)
		{
			IDXGIDevice3 *device3 = nullptr;

			if (FAILED(this->mOrig->QueryInterface(&device3)))
			{
				return E_NOINTERFACE;
			}

			this->mOrig->Release();

			LOG(TRACE) << "Upgraded 'IDXGIDevice" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " to 'IDXGIDevice3'.";

			this->mOrig = device3;
			this->mInterfaceVersion = 3;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return this->mDirect3DDevice->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGIDevice::AddRef()
{
	this->mRef++;

	return this->mOrig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGIDevice::Release()
{
	ULONG ref = this->mOrig->Release();

	if (--this->mRef == 0 && ref != 1)
	{
		LOG(WARNING) << "Reference count for 'IDXGIDevice" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 1.";

		ref = 1;
	}

	if (ref == 1)
	{
		assert(this->mRef <= 0);

		LOG(TRACE) << "Destroyed 'IDXGIDevice" << (this->mInterfaceVersion > 0 ? std::to_string(this->mInterfaceVersion) : "") << "' object " << this << ".";

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

// IDXGIDevice3
void STDMETHODCALLTYPE DXGIDevice::Trim()
{
	assert(this->mInterfaceVersion >= 3);

	return static_cast<IDXGIDevice3 *>(this->mOrig)->Trim();
}

// IDXGIFactory
HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory::CreateSwapChain" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << ppSwapChain << ")' ...";

	IUnknown *deviceOrig = pDevice;
	D3D10Device *deviceD3D10 = nullptr;
	D3D11Device *deviceD3D11 = nullptr;
	D3D12CommandQueue *commandqueueD3D12 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
	{
		deviceOrig = deviceD3D10->mOrig;

		deviceD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
	{
		deviceOrig = deviceD3D11->mOrig;

		deviceD3D11->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&commandqueueD3D12)))
	{
		deviceOrig = commandqueueD3D12->mOrig;

		commandqueueD3D12->Release();
	}

	DumpSwapChainDescription(*pDesc);

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory_CreateSwapChain)(pFactory, deviceOrig, pDesc, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory::CreateSwapChain' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGISwapChain *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (deviceD3D10 != nullptr)
	{
		deviceD3D10->AddRef();

		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(deviceD3D10->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D10->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D10, swapchain, runtime);
	}
	else if (deviceD3D11 != nullptr)
	{
		deviceD3D11->AddRef();

		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(deviceD3D11->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D11->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D11, swapchain, runtime);
	}
	else if (commandqueueD3D12 != nullptr)
	{
		IDXGISwapChain3 *swapchain3 = nullptr;
		
		if (SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			commandqueueD3D12->AddRef();

			const auto runtime = std::make_shared<ReShade::Runtimes::D3D12Runtime>(commandqueueD3D12->mDevice->mOrig, commandqueueD3D12->mOrig, swapchain3);

			if (!runtime->OnInit(desc))
			{
				LOG(ERROR) << "Failed to initialize Direct3D12 runtime environment on runtime " << runtime.get() << ".";
			}

			*ppSwapChain = new DXGISwapChain(commandqueueD3D12, swapchain, runtime);

			swapchain3->Release();
		}
		else
		{
			LOG(WARNING) << "> Skipping swap chain because it is missing support for the 'IDXGISwapChain3' interface.";
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swap chain object: " << *ppSwapChain;

	return S_OK;
}

// IDXGIFactory2
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForHwnd" << "(" << pFactory << ", " << pDevice << ", " << hWnd << ", " << pDesc << ", " << pFullscreenDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *deviceOrig = pDevice;
	D3D10Device *deviceD3D10 = nullptr;
	D3D11Device *deviceD3D11 = nullptr;
	D3D12CommandQueue *commandqueueD3D12 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
	{
		deviceOrig = deviceD3D10->mOrig;

		deviceD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
	{
		deviceOrig = deviceD3D11->mOrig;

		deviceD3D11->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&commandqueueD3D12)))
	{
		deviceOrig = commandqueueD3D12->mOrig;

		commandqueueD3D12->Release();
	}

	DumpSwapChainDescription(*pDesc);

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForHwnd)(pFactory, deviceOrig, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForHwnd' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (deviceD3D10 != nullptr)
	{
		deviceD3D10->AddRef();

		const std::shared_ptr<ReShade::Runtimes::D3D10Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(deviceD3D10->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D10->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D10, swapchain, runtime);
	}
	else if (deviceD3D11 != nullptr)
	{
		deviceD3D11->AddRef();

		const std::shared_ptr<ReShade::Runtimes::D3D11Runtime> runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(deviceD3D11->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D11->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D11, swapchain, runtime);
	}
	else if (commandqueueD3D12 != nullptr)
	{
		IDXGISwapChain3 *swapchain3 = nullptr;

		if (SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			commandqueueD3D12->AddRef();

			const auto runtime = std::make_shared<ReShade::Runtimes::D3D12Runtime>(commandqueueD3D12->mDevice->mOrig, commandqueueD3D12->mOrig, swapchain3);

			if (!runtime->OnInit(desc))
			{
				LOG(ERROR) << "Failed to initialize Direct3D12 runtime environment on runtime " << runtime.get() << ".";
			}

			*ppSwapChain = new DXGISwapChain(commandqueueD3D12, swapchain, runtime);

			swapchain3->Release();
		}
		else
		{
			LOG(WARNING) << "> Skipping swap chain because it is missing support for the 'IDXGISwapChain3' interface.";
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swap chain object: " << *ppSwapChain;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForCoreWindow" << "(" << pFactory << ", " << pDevice << ", " << pWindow << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *deviceOrig = pDevice;
	D3D10Device *deviceD3D10 = nullptr;
	D3D11Device *deviceD3D11 = nullptr;
	D3D12CommandQueue *commandqueueD3D12 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
	{
		deviceOrig = deviceD3D10->mOrig;

		deviceD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
	{
		deviceOrig = deviceD3D11->mOrig;

		deviceD3D11->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&commandqueueD3D12)))
	{
		deviceOrig = commandqueueD3D12->mOrig;

		commandqueueD3D12->Release();
	}

	DumpSwapChainDescription(*pDesc);

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForCoreWindow)(pFactory, deviceOrig, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForCoreWindow' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (deviceD3D10 != nullptr)
	{
		deviceD3D10->AddRef();

		const auto runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(deviceD3D10->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D10->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D10, swapchain, runtime);
	}
	else if (deviceD3D11 != nullptr)
	{
		deviceD3D11->AddRef();

		const auto runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(deviceD3D11->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D11->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D11, swapchain, runtime);
	}
	else if (commandqueueD3D12 != nullptr)
	{
		IDXGISwapChain3 *swapchain3 = nullptr;

		if (SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			commandqueueD3D12->AddRef();

			const auto runtime = std::make_shared<ReShade::Runtimes::D3D12Runtime>(commandqueueD3D12->mDevice->mOrig, commandqueueD3D12->mOrig, swapchain3);

			if (!runtime->OnInit(desc))
			{
				LOG(ERROR) << "Failed to initialize Direct3D12 runtime environment on runtime " << runtime.get() << ".";
			}

			*ppSwapChain = new DXGISwapChain(commandqueueD3D12, swapchain, runtime);

			swapchain3->Release();
		}
		else
		{
			LOG(WARNING) << "> Skipping swap chain because it is missing support for the 'IDXGISwapChain3' interface.";
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swap chain object: " << *ppSwapChain;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	LOG(INFO) << "Redirecting '" << "IDXGIFactory2::CreateSwapChainForComposition" << "(" << pFactory << ", " << pDevice << ", " << pDesc << ", " << pRestrictToOutput << ", " << ppSwapChain << ")' ...";

	IUnknown *deviceOrig = pDevice;
	D3D10Device *deviceD3D10 = nullptr;
	D3D11Device *deviceD3D11 = nullptr;
	D3D12CommandQueue *commandqueueD3D12 = nullptr;

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D10)))
	{
		deviceOrig = deviceD3D10->mOrig;

		deviceD3D10->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&deviceD3D11)))
	{
		deviceOrig = deviceD3D11->mOrig;

		deviceD3D11->Release();
	}
	else if (SUCCEEDED(pDevice->QueryInterface(&commandqueueD3D12)))
	{
		deviceOrig = commandqueueD3D12->mOrig;

		commandqueueD3D12->Release();
	}

	DumpSwapChainDescription(*pDesc);

	const HRESULT hr = ReShade::Hooks::Call(&IDXGIFactory2_CreateSwapChainForComposition)(pFactory, deviceOrig, pDesc, pRestrictToOutput, ppSwapChain);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'IDXGIFactory2::CreateSwapChainForComposition' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGISwapChain1 *const swapchain = *ppSwapChain;

	DXGI_SWAP_CHAIN_DESC desc;
	swapchain->GetDesc(&desc);

	if ((desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARNING) << "> Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (deviceD3D10 != nullptr)
	{
		deviceD3D10->AddRef();

		const auto runtime = std::make_shared<ReShade::Runtimes::D3D10Runtime>(deviceD3D10->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D10 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D10->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D10, swapchain, runtime);
	}
	else if (deviceD3D11 != nullptr)
	{
		deviceD3D11->AddRef();

		const auto runtime = std::make_shared<ReShade::Runtimes::D3D11Runtime>(deviceD3D11->mOrig, swapchain);

		if (!runtime->OnInit(desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D11 runtime environment on runtime " << runtime.get() << ".";
		}

		deviceD3D11->mRuntimes.push_back(runtime);

		*ppSwapChain = new DXGISwapChain(deviceD3D11, swapchain, runtime);
	}
	else if (commandqueueD3D12 != nullptr)
	{
		IDXGISwapChain3 *swapchain3 = nullptr;

		if (SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			commandqueueD3D12->AddRef();

			const auto runtime = std::make_shared<ReShade::Runtimes::D3D12Runtime>(commandqueueD3D12->mDevice->mOrig, commandqueueD3D12->mOrig, swapchain3);

			if (!runtime->OnInit(desc))
			{
				LOG(ERROR) << "Failed to initialize Direct3D12 runtime environment on runtime " << runtime.get() << ".";
			}

			*ppSwapChain = new DXGISwapChain(commandqueueD3D12, swapchain, runtime);

			swapchain3->Release();
		}
		else
		{
			LOG(WARNING) << "> Skipping swap chain because it is missing support for the 'IDXGISwapChain3' interface.";
		}
	}
	else
	{
		LOG(WARNING) << "> Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

	LOG(TRACE) << "> Returned swap chain object: " << *ppSwapChain;

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
	return ReShade::Hooks::Call(&DXGID3D10CreateDevice)(hModule, pFactory, pAdapter, Flags, pUnknown, ppDevice);
}
EXPORT HRESULT WINAPI DXGID3D10CreateLayeredDevice(void *pUnknown1, void *pUnknown2, void *pUnknown3, void *pUnknown4, void *pUnknown5)
{
	return ReShade::Hooks::Call(&DXGID3D10CreateLayeredDevice)(pUnknown1, pUnknown2, pUnknown3, pUnknown4, pUnknown5);
}
EXPORT SIZE_T WINAPI DXGID3D10GetLayeredDeviceSize(const void *pLayers, UINT NumLayers)
{
	return ReShade::Hooks::Call(&DXGID3D10GetLayeredDeviceSize)(pLayers, NumLayers);
}
EXPORT HRESULT WINAPI DXGID3D10RegisterLayers(const void *pLayers, UINT NumLayers)
{
	return ReShade::Hooks::Call(&DXGID3D10RegisterLayers)(pLayers, NumLayers);
}

EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory" << "(" << riidString << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory), 10, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2), 15, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2), 16, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2), 24, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}
EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory1" << "(" << riidString << ", " << ppFactory << ")' ...";

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory1)(riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory1' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory), 10, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2), 15, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2), 16, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2), 24, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}
EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT flags, REFIID riid, void **ppFactory)
{
	OLECHAR riidString[40];
	StringFromGUID2(riid, riidString, ARRAYSIZE(riidString));

	LOG(INFO) << "Redirecting '" << "CreateDXGIFactory2" << "(" << flags << ", " << riidString << ", " << ppFactory << ")' ...";

#ifdef _DEBUG
	flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	const HRESULT hr = ReShade::Hooks::Call(&CreateDXGIFactory2)(flags, riid, ppFactory);

	if (FAILED(hr))
	{
		LOG(WARNING) << "> 'CreateDXGIFactory2' failed with '" << GetErrorString(hr) << "'!";

		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);
	IDXGIFactory2 *factory2 = nullptr;

	ReShade::Hooks::Install(VTABLE(factory), 10, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory_CreateSwapChain));

	if (SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		ReShade::Hooks::Install(VTABLE(factory2), 15, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForHwnd));
		ReShade::Hooks::Install(VTABLE(factory2), 16, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForCoreWindow));
		ReShade::Hooks::Install(VTABLE(factory2), 24, reinterpret_cast<ReShade::Hook::Function>(&IDXGIFactory2_CreateSwapChainForComposition));

		factory2->Release();
	}

	LOG(TRACE) << "> Returned factory object: " << *ppFactory;

	return S_OK;
}
