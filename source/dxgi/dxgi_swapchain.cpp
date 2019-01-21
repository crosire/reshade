/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "dxgi_swapchain.hpp"
#include "../d3d11/d3d11_device_context.hpp"
#include "d3d10/runtime_d3d10.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include <algorithm>

void DXGISwapChain::perform_present(UINT PresentFlags)
{
	// Some D3D11 games test presentation for timing and composition purposes.
	// These calls are NOT rendering-related, but rather a status request for the D3D runtime and as such should be ignored.
	if (!(PresentFlags & DXGI_PRESENT_TEST))
	{
		switch (_direct3d_version)
		{
		case 10:
			assert(_runtime != nullptr);
			std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_present(static_cast<D3D10Device *>(_direct3d_device)->_draw_call_tracker);
			clear_drawcall_stats();
			break;
		case 11:
			assert(_runtime != nullptr);
			std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_present(static_cast<D3D11Device *>(_direct3d_device)->_immediate_context->_draw_call_tracker);
			clear_drawcall_stats();
			break;
		}
	}
}

void DXGISwapChain::clear_drawcall_stats()
{
	const auto device_d3d10_proxy = static_cast<D3D10Device *>(_direct3d_device);
	const auto device_d3d11_proxy = static_cast<D3D11Device *>(_direct3d_device);

	switch (_direct3d_version)
	{
	case 10:			
		device_d3d10_proxy->clear_drawcall_stats();
		break;
	case 11:			
		const auto immediate_context_proxy = device_d3d11_proxy->_immediate_context;
		immediate_context_proxy->clear_drawcall_stats();
		device_d3d11_proxy->clear_drawcall_stats();
		break;
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
		riid == __uuidof(IDXGISwapChain3) ||
		riid == __uuidof(IDXGISwapChain4))
	{
		#pragma region Update to IDXGISwapChain1 interface
		if (riid == __uuidof(IDXGISwapChain1) && _interface_version < 1)
		{
			IDXGISwapChain1 *swapchain1 = nullptr;

			if (FAILED(_orig->QueryInterface(&swapchain1)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'IDXGISwapChain' object " << this << " to 'IDXGISwapChain1'.";
#endif
			_orig = swapchain1;
			_interface_version = 1;
		}
		#pragma endregion
		#pragma region Update to IDXGISwapChain2 interface
		if (riid == __uuidof(IDXGISwapChain2) && _interface_version < 2)
		{
			IDXGISwapChain2 *swapchain2 = nullptr;

			if (FAILED(_orig->QueryInterface(&swapchain2)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'IDXGISwapChain" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'IDXGISwapChain2'.";
#endif
			_orig = swapchain2;
			_interface_version = 2;
		}
		#pragma endregion
		#pragma region Update to IDXGISwapChain3 interface
		if (riid == __uuidof(IDXGISwapChain3) && _interface_version < 3)
		{
			IDXGISwapChain3 *swapchain3 = nullptr;

			if (FAILED(_orig->QueryInterface(&swapchain3)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'IDXGISwapChain" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'IDXGISwapChain3'.";
#endif
			_orig = swapchain3;
			_interface_version = 3;
		}
		#pragma endregion
		#pragma region Update to IDXGISwapChain4 interface
		if (riid == __uuidof(IDXGISwapChain4) && _interface_version < 4)
		{
			IDXGISwapChain4 *swapchain4 = nullptr;

			if (FAILED(_orig->QueryInterface(&swapchain4)))
			{
				return E_NOINTERFACE;
			}

			_orig->Release();

#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded 'IDXGISwapChain" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " to 'IDXGISwapChain4'.";
#endif
			_orig = swapchain4;
			_interface_version = 4;
		}
		#pragma endregion

		AddRef();

		*ppvObj = this;

		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG STDMETHODCALLTYPE DXGISwapChain::AddRef()
{
	_ref++;

	return _orig->AddRef();
}
ULONG STDMETHODCALLTYPE DXGISwapChain::Release()
{
	if (--_ref == 0)
	{
		switch (_direct3d_version)
		{
			case 10:
			{
				assert(_runtime != nullptr);

				auto &runtimes = static_cast<D3D10Device *>(_direct3d_device)->_runtimes;
				const auto runtime = std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime);
				runtime->on_reset();

				runtimes.erase(std::remove(runtimes.begin(), runtimes.end(), runtime), runtimes.end());
				break;
			}
			case 11:
			{
				assert(_runtime != nullptr);

				clear_drawcall_stats();

				auto &runtimes = static_cast<D3D11Device *>(_direct3d_device)->_runtimes;
				const auto runtime = std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime);
				runtime->on_reset();

				runtimes.erase(std::remove(runtimes.begin(), runtimes.end(), runtime), runtimes.end());
				break;
			}
		}

		_runtime.reset();

		_direct3d_device->Release();
	}

	ULONG ref = _orig->Release();

	if (_ref == 0 && ref != 0)
	{
		LOG(WARNING) << "Reference count for 'IDXGISwapChain" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << " is inconsistent: " << ref << ", but expected 0.";

		ref = 0;
	}

	if (ref == 0)
	{
		assert(_ref <= 0);

#if RESHADE_VERBOSE_LOG
		LOG(DEBUG) << "Destroyed 'IDXGISwapChain" << (_interface_version > 0 ? std::to_string(_interface_version) : "") << "' object " << this << ".";
#endif
		delete this;
	}

	return ref;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown)
{
	return _orig->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetParent(REFIID riid, void **ppParent)
{
	return _orig->GetParent(riid, ppParent);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDevice(REFIID riid, void **ppDevice)
{
	if (ppDevice == nullptr)
	{
		return DXGI_ERROR_INVALID_CALL;
	}

	return _direct3d_device->QueryInterface(riid, ppDevice);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	perform_present(Flags);
	return _orig->Present(SyncInterval, Flags);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
	return _orig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::SetFullscreenState" << "(" << this << ", " << (Fullscreen != FALSE ? "TRUE" : "FALSE") << ", " << pTarget << ")' ...";

	return _orig->SetFullscreenState(Fullscreen, pTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget)
{
	return _orig->GetFullscreenState(pFullscreen, ppTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc)
{
	return _orig->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	LOG(INFO) << "Redirecting '" << "IDXGISwapChain::ResizeBuffers" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << NewFormat << ", " << std::hex << SwapChainFlags << std::dec << ")' ...";

	switch (_direct3d_version)
	{
		case 10:
			assert(_runtime != nullptr);
			std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_reset();
			break;
		case 11:
			assert(_runtime != nullptr);
			clear_drawcall_stats();
			std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_reset();
			break;
	}

	const HRESULT hr = _orig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (hr == DXGI_ERROR_INVALID_CALL)
	{
		LOG(WARNING) << "> 'IDXGISwapChain::ResizeBuffers' failed with 'DXGI_ERROR_INVALID_CALL'!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDXGISwapChain::ResizeBuffers' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	DXGI_SWAP_CHAIN_DESC desc;
	_orig->GetDesc(&desc);

	bool initialized = false;

	switch (_direct3d_version)
	{
		case 10:
			assert(_runtime != nullptr);
			initialized = std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_init(desc);
			break;
		case 11:
			assert(_runtime != nullptr);
			initialized = std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_init(desc);
			break;
	}

	if (!initialized)
	{
		LOG(ERROR) << "Failed to recreate Direct3D " << _direct3d_version << " runtime environment on runtime " << _runtime.get() << ".";
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters)
{
	return _orig->ResizeTarget(pNewTargetParameters);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetContainingOutput(IDXGIOutput **ppOutput)
{
	return _orig->GetContainingOutput(ppOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats)
{
	return _orig->GetFrameStatistics(pStats);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetLastPresentCount(UINT *pLastPresentCount)
{
	return _orig->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetDesc1(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetFullscreenDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetHwnd(HWND *pHwnd)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetHwnd(pHwnd);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetCoreWindow(REFIID refiid, void **ppUnk)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetCoreWindow(refiid, ppUnk);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters)
{
	assert(_interface_version >= 1);
	perform_present(PresentFlags);
	return static_cast<IDXGISwapChain1 *>(_orig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
}
BOOL STDMETHODCALLTYPE DXGISwapChain::IsTemporaryMonoSupported()
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetBackgroundColor(const DXGI_RGBA *pColor)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->SetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBackgroundColor(DXGI_RGBA *pColor)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetRotation(DXGI_MODE_ROTATION Rotation)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->SetRotation(Rotation);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRotation(DXGI_MODE_ROTATION *pRotation)
{
	assert(_interface_version >= 1);

	return static_cast<IDXGISwapChain1 *>(_orig)->GetRotation(pRotation);
}

// IDXGISwapChain2
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetSourceSize(UINT Width, UINT Height)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->SetSourceSize(Width, Height);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetSourceSize(UINT *pWidth, UINT *pHeight)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->GetSourceSize(pWidth, pHeight);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMaximumFrameLatency(UINT MaxLatency)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMaximumFrameLatency(UINT *pMaxLatency)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->GetMaximumFrameLatency(pMaxLatency);
}
HANDLE STDMETHODCALLTYPE DXGISwapChain::GetFrameLatencyWaitableObject()
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->GetFrameLatencyWaitableObject();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->SetMatrixTransform(pMatrix);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix)
{
	assert(_interface_version >= 2);

	return static_cast<IDXGISwapChain2 *>(_orig)->GetMatrixTransform(pMatrix);
}

// IDXGISwapChain3
UINT STDMETHODCALLTYPE DXGISwapChain::GetCurrentBackBufferIndex()
{
	assert(_interface_version >= 3);

	return static_cast<IDXGISwapChain3 *>(_orig)->GetCurrentBackBufferIndex();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport)
{
	assert(_interface_version >= 3);

	return static_cast<IDXGISwapChain3 *>(_orig)->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
	assert(_interface_version >= 3);

	return static_cast<IDXGISwapChain3 *>(_orig)->SetColorSpace1(ColorSpace);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue)
{
	assert(_interface_version >= 3);

	LOG(INFO) << "Redirecting '" << "IDXGISwapChain3::ResizeBuffers1" << "(" << this << ", " << BufferCount << ", " << Width << ", " << Height << ", " << Format << ", " << std::hex << SwapChainFlags << std::dec << ", " << pCreationNodeMask << ", " << ppPresentQueue << ")' ...";

	switch (_direct3d_version)
	{
		case 10:
			assert(_runtime != nullptr);
			std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_reset();
			break;
		case 11:
			assert(_runtime != nullptr);
			clear_drawcall_stats();
			std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_reset();
			break;
	}

	const HRESULT hr = static_cast<IDXGISwapChain3 *>(_orig)->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);

	if (hr == DXGI_ERROR_INVALID_CALL)
	{
		LOG(WARNING) << "> 'IDXGISwapChain3::ResizeBuffers1' failed with 'DXGI_ERROR_INVALID_CALL'!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "> 'IDXGISwapChain3::ResizeBuffers1' failed with error code " << std::hex << hr << std::dec << "!";

		return hr;
	}

	DXGI_SWAP_CHAIN_DESC desc;
	_orig->GetDesc(&desc);

	bool initialized = false;

	switch (_direct3d_version)
	{
		case 10:
			assert(_runtime != nullptr);
			initialized = std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_init(desc);
			break;
		case 11:
			assert(_runtime != nullptr);
			initialized = std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_init(desc);
			break;
	}

	if (!initialized)
	{
		LOG(ERROR) << "Failed to recreate Direct3D " << _direct3d_version << " runtime environment on runtime " << _runtime.get() << ".";
	}

	return hr;
}

// IDXGISwapChain5
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData)
{
	assert(_interface_version >= 4);

	return static_cast<IDXGISwapChain4 *>(_orig)->SetHDRMetaData(Type, Size, pMetaData);
}
