/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d10/d3d10_impl_swapchain.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/d3d11_impl_swapchain.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "d3d12/d3d12_impl_swapchain.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "addon_manager.hpp"
#include "runtime_manager.hpp"

#if RESHADE_ADDON
extern bool modify_swapchain_desc(reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC &desc, UINT &sync_interval);
extern bool modify_swapchain_desc(reshade::api::device_api api, DXGI_SWAP_CHAIN_DESC1 &desc, UINT &sync_interval, DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc, HWND window);
#endif

extern reshade::api::device_api query_device(IUnknown *&device, com_ptr<IUnknown> &device_proxy);

// Needs to be set whenever a DXGI call can end up in 'CDXGISwapChain::EnsureChildDeviceInternal', to avoid hooking internal D3D device creation
thread_local bool g_in_dxgi_runtime = false;

// SpecialK uses this private data GUID to track the current swap chain color space, so just do the same
inline constexpr GUID SKID_SwapChainColorSpace = { 0x18b57e4, 0x1493, 0x4953, { 0xad, 0xf2, 0xde, 0x6d, 0x99, 0xcc, 0x5, 0xe5 } }; // {018B57E4-1493-4953-ADF2-DE6D99CC05E5}

DXGISwapChain::DXGISwapChain(D3D10Device *device, IDXGISwapChain  *original) :
	_orig(original),
	_interface_version(0),
	_direct3d_device(static_cast<ID3D10Device *>(device)),
	_direct3d_command_queue(nullptr),
	_direct3d_version(reshade::api::device_api::d3d10),
	_impl(new reshade::d3d10::swapchain_impl(device, original))
{
	assert(_orig != nullptr && _direct3d_device != nullptr);
	// Explicitly add a reference to the device, to ensure it stays valid for the lifetime of this swap chain object
	_direct3d_device->AddRef();

	reshade::create_effect_runtime(_impl, device);
	on_init(false);
}
DXGISwapChain::DXGISwapChain(D3D10Device *device, IDXGISwapChain1 *original) :
	_orig(original),
	_interface_version(1),
	_direct3d_device(static_cast<ID3D10Device *>(device)),
	_direct3d_command_queue(nullptr),
	_direct3d_version(reshade::api::device_api::d3d10),
	_impl(new reshade::d3d10::swapchain_impl(device, original))
{
	assert(_orig != nullptr && _direct3d_device != nullptr);
	_direct3d_device->AddRef();

	reshade::create_effect_runtime(_impl, device);
	on_init(false);
}
DXGISwapChain::DXGISwapChain(D3D11Device *device, IDXGISwapChain  *original) :
	_orig(original),
	_interface_version(0),
	_direct3d_device(static_cast<ID3D11Device *>(device)),
	_direct3d_command_queue(nullptr),
	_direct3d_version(reshade::api::device_api::d3d11),
	_impl(new reshade::d3d11::swapchain_impl(device, original))
{
	assert(_orig != nullptr && _direct3d_device != nullptr);
	_direct3d_device->AddRef();

	reshade::create_effect_runtime(_impl, device->_immediate_context);
	on_init(false);
}
DXGISwapChain::DXGISwapChain(D3D11Device *device, IDXGISwapChain1 *original) :
	_orig(original),
	_interface_version(1),
	_direct3d_device(static_cast<ID3D11Device *>(device)),
	_direct3d_command_queue(nullptr),
	_direct3d_version(reshade::api::device_api::d3d11),
	_impl(new reshade::d3d11::swapchain_impl(device, original))
{
	assert(_orig != nullptr && _direct3d_device != nullptr);
	_direct3d_device->AddRef();

	reshade::create_effect_runtime(_impl, device->_immediate_context);
	on_init(false);
}
DXGISwapChain::DXGISwapChain(D3D12CommandQueue *command_queue, IDXGISwapChain3 *original) :
	_orig(original),
	_interface_version(3),
	_direct3d_device(static_cast<ID3D12Device *>(command_queue->_device)), // Get the device instead of the command queue, so that 'IDXGISwapChain::GetDevice' works
	_direct3d_command_queue(command_queue),
	_direct3d_version(reshade::api::device_api::d3d12),
	_impl(new reshade::d3d12::swapchain_impl(command_queue->_device, original))
{
	assert(_orig != nullptr && _direct3d_device != nullptr && _direct3d_command_queue != nullptr);
	_direct3d_device->AddRef();
	// Add reference to command queue as well to ensure it is kept alive for the lifetime of the effect runtime
	_direct3d_command_queue->AddRef();

	for (size_t i = 0; i < std::size(_direct3d_command_queue_per_back_buffer); ++i)
		_direct3d_command_queue_per_back_buffer[i] = _direct3d_command_queue;

	reshade::create_effect_runtime(_impl, command_queue);
	on_init(false);
}
DXGISwapChain::~DXGISwapChain()
{
	on_reset(false);
	reshade::destroy_effect_runtime(_impl);

	// Destroy effect runtime first to release all internal references to device objects
	switch (_direct3d_version)
	{
	case reshade::api::device_api::d3d10:
		delete static_cast<reshade::d3d10::swapchain_impl *>(_impl);
		break;
	case reshade::api::device_api::d3d11:
		delete static_cast<reshade::d3d11::swapchain_impl *>(_impl);
		break;
	case reshade::api::device_api::d3d12:
		delete static_cast<reshade::d3d12::swapchain_impl *>(_impl);
		break;
	}

	// Release the explicit reference to the device that was added in the 'DXGISwapChain' constructor above now that the effect runtime was destroyed and is longer referencing it
	if (_direct3d_command_queue != nullptr)
		_direct3d_command_queue->Release();
	_direct3d_device->Release();
}

bool DXGISwapChain::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDeviceSubObject))
		return true;

	static constexpr IID iid_lookup[] = {
		__uuidof(IDXGISwapChain),
		__uuidof(IDXGISwapChain1),
		__uuidof(IDXGISwapChain2),
		__uuidof(IDXGISwapChain3),
		__uuidof(IDXGISwapChain4),
	};

	for (unsigned short version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Upgrading IDXGISwapChain%hu object %p to IDXGISwapChain%hu.", _interface_version, this, version);
#endif
			_orig->Release();
			_orig = static_cast<IDXGISwapChain *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE DXGISwapChain::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE DXGISwapChain::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if RESHADE_VERBOSE_LOG
	reshade::log::message(reshade::log::level::debug, "Destroying IDXGISwapChain%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	// Resident Evil 3 releases the swap chain without first leaving fullscreen, which causes the DXGI runtime to throw an exception
	if (BOOL fullscreen = FALSE;
		SUCCEEDED(orig->GetFullscreenState(&fullscreen, nullptr)) && fullscreen)
	{
		reshade::log::message(reshade::log::level::warning, "Attempted to destroy swap chain while still in fullscreen mode.");
	}
	else
	{
		// Only release internal reference after the effect runtime has been destroyed, so any references it held are cleaned up at this point
		const ULONG ref_orig = orig->Release();
		if (ref_orig != 0) // Verify internal reference count
			reshade::log::message(reshade::log::level::warning, "Reference count for IDXGISwapChain%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
	}

	return 0;
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
	return _direct3d_device->QueryInterface(riid, ppDevice);
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
	on_present(Flags);

#if RESHADE_ADDON
	if (_sync_interval != UINT_MAX)
		SyncInterval = _sync_interval;
#endif

	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->Present(SyncInterval, Flags);
	g_in_dxgi_runtime = false;

	handle_device_loss(hr);

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
	return _orig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGISwapChain::SetFullscreenState(this = %p, Fullscreen = %s, pTarget = %p) ...",
		this, Fullscreen ? "TRUE" : "FALSE", pTarget);

#if RESHADE_ADDON
	_current_fullscreen_state = -1;

	HMONITOR hmonitor = nullptr;
	if (pTarget != nullptr)
	{
		DXGI_OUTPUT_DESC output_desc = {};
		pTarget->GetDesc(&output_desc);
		hmonitor = output_desc.Monitor;
	}

	if (reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(_impl, Fullscreen != FALSE, hmonitor))
	{
		_current_fullscreen_state = Fullscreen;
		return S_OK;
	}
#endif

	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;
	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->SetFullscreenState(Fullscreen, pTarget);
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget)
{
#if RESHADE_ADDON
	if (_current_fullscreen_state >= 0)
	{
		if (pFullscreen != nullptr)
			*pFullscreen = _current_fullscreen_state;
		if (ppTarget != nullptr)
			_orig->GetContainingOutput(ppTarget);
		return S_OK;
	}
#endif

	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;
	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->GetFullscreenState(pFullscreen, ppTarget);
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc)
{
#if RESHADE_ADDON
	if (_orig_desc.BufferCount != 0)
	{
		assert(pDesc != nullptr);

		*pDesc = _orig_desc;
		return S_OK;
	}
#endif

	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;
	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->GetDesc(pDesc);
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGISwapChain::ResizeBuffers(this = %p, BufferCount = %u, Width = %u, Height = %u, NewFormat = %d, SwapChainFlags = %#x) ...",
		this, BufferCount, Width, Height, static_cast<int>(NewFormat), SwapChainFlags);

	on_reset(true);

	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;

	// Handle update of the swap chain description
#if RESHADE_ADDON
	const DXGI_SWAP_CHAIN_DESC prev_orig_desc = _orig_desc;
	{
		g_in_dxgi_runtime = true;
		DXGI_SWAP_CHAIN_DESC desc = {};
		_orig->GetDesc(&desc);
		g_in_dxgi_runtime = was_in_dxgi_runtime;

		// Forcefully restore the parameters that are willingly left to their previous value to the original value we cached, not the last one overriden by the addon, as that's not what the application expects
		if (BufferCount == 0)
			BufferCount = _orig_desc.BufferCount;
		if (NewFormat == DXGI_FORMAT_UNKNOWN)
			NewFormat = _orig_desc.BufferDesc.Format;

		desc.BufferCount = _orig_desc.BufferCount = BufferCount;
		desc.BufferDesc.Width = _orig_desc.BufferDesc.Width = Width;
		desc.BufferDesc.Height = _orig_desc.BufferDesc.Height = Height;
		desc.BufferDesc.Format = _orig_desc.BufferDesc.Format = NewFormat;
		desc.Flags = _orig_desc.Flags = SwapChainFlags;

		if (modify_swapchain_desc(_direct3d_version, desc, _sync_interval))
		{
			_orig_desc.BufferCount = desc.BufferCount;

			BufferCount = desc.BufferCount;
			Width = desc.BufferDesc.Width;
			Height = desc.BufferDesc.Height;
			NewFormat = desc.BufferDesc.Format;
			SwapChainFlags = desc.Flags;
		}
		// Disable using the original desc
		else
		{
			_orig_desc.BufferCount = 0;
		}
	}
#endif

	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) && (Width == 0 || Height == 0))
	{
		DXGI_SWAP_CHAIN_DESC desc = {};
		_orig->GetDesc(&desc);
		_orig_desc.BufferDesc.Width = desc.BufferDesc.Width;
		_orig_desc.BufferDesc.Height = desc.BufferDesc.Height;
	}
	else if (FAILED(hr))
	{
		_orig_desc = prev_orig_desc;
	}
#endif
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	if (SUCCEEDED(hr))
	{
		on_init(true);
	}
	else if (hr == DXGI_ERROR_INVALID_CALL) // Ignore invalid call errors since the device is still in a usable state afterwards
	{
		reshade::log::message(reshade::log::level::warning, "IDXGISwapChain::ResizeBuffers failed with error code DXGI_ERROR_INVALID_CALL.");
		on_init(true);
	}
	else
	{
		reshade::log::message(reshade::log::level::error, "IDXGISwapChain::ResizeBuffers failed with error code %s!", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters)
{
	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;
	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->ResizeTarget(pNewTargetParameters);
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	return hr;
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

HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc)
{
	assert(_interface_version >= 1);

#if RESHADE_ADDON
	if (_orig_desc.BufferCount != 0)
	{
		assert(pDesc != nullptr);

		pDesc->Width = _orig_desc.BufferDesc.Width;
		pDesc->Height = _orig_desc.BufferDesc.Height;
		pDesc->Format = _orig_desc.BufferDesc.Format;
		pDesc->Stereo = FALSE; // For now we don't carry this information
		pDesc->SampleDesc = _orig_desc.SampleDesc;
		pDesc->BufferUsage = _orig_desc.BufferUsage;
		pDesc->BufferCount = _orig_desc.BufferCount;
		pDesc->Scaling = _orig_desc.BufferDesc.Scaling == DXGI_MODE_SCALING_CENTERED ? DXGI_SCALING_ASPECT_RATIO_STRETCH : DXGI_SCALING_STRETCH;
		pDesc->SwapEffect = _orig_desc.SwapEffect;
		pDesc->AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		pDesc->Flags = _orig_desc.Flags;
		return S_OK;
	}
#endif

	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;
	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGISwapChain1 *>(_orig)->GetDesc1(pDesc);
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	return hr;
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
	on_present(PresentFlags, pPresentParameters);

#if RESHADE_ADDON
	if (_sync_interval != UINT_MAX)
		SyncInterval = _sync_interval;
#endif

	assert(_interface_version >= 1);
	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGISwapChain1 *>(_orig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
	g_in_dxgi_runtime = false;

	handle_device_loss(hr);

	return hr;
}
BOOL    STDMETHODCALLTYPE DXGISwapChain::IsTemporaryMonoSupported()
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
HANDLE  STDMETHODCALLTYPE DXGISwapChain::GetFrameLatencyWaitableObject()
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

UINT    STDMETHODCALLTYPE DXGISwapChain::GetCurrentBackBufferIndex()
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
#if RESHADE_VERBOSE_LOG
	const char *color_space_string = nullptr;
	switch (ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
		color_space_string = "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709";
		break;
	case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
		color_space_string = "DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709";
		break;
	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
		color_space_string = "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020";
		break;
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
		color_space_string = "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020";
		break;
	}

	if (color_space_string != nullptr)
		reshade::log::message(reshade::log::level::info, "Redirecting IDXGISwapChain3::SetColorSpace1(ColorSpace = %s) ...", color_space_string);
	else
#endif
		reshade::log::message(reshade::log::level::info, "Redirecting IDXGISwapChain3::SetColorSpace1(ColorSpace = %d) ...", static_cast<int>(ColorSpace));

	DXGI_COLOR_SPACE_TYPE prev_color_space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT prev_color_space_size = sizeof(prev_color_space);
	_orig->GetPrivateData(SKID_SwapChainColorSpace, &prev_color_space_size, &prev_color_space);

	if (ColorSpace != prev_color_space)
		on_reset(true);

	assert(_interface_version >= 3);
	assert(!g_in_dxgi_runtime);
	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGISwapChain3 *>(_orig)->SetColorSpace1(ColorSpace);
	g_in_dxgi_runtime = false;
	if (SUCCEEDED(hr))
	{
		_orig->SetPrivateData(SKID_SwapChainColorSpace, sizeof(ColorSpace), &ColorSpace);
	}

	if (ColorSpace != prev_color_space)
		on_init(true);

	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting IDXGISwapChain3::ResizeBuffers1(this = %p, BufferCount = %u, Width = %u, Height = %u, Format = %d, SwapChainFlags = %#x, pCreationNodeMask = %p, ppPresentQueue = %p) ...",
		this, BufferCount, Width, Height, static_cast<int>(NewFormat), SwapChainFlags, pCreationNodeMask, ppPresentQueue);

	on_reset(true);

	assert(_interface_version >= 3);
	const bool was_in_dxgi_runtime = g_in_dxgi_runtime;

	// Handle update of the swap chain description
#if RESHADE_ADDON
	const DXGI_SWAP_CHAIN_DESC prev_orig_desc = _orig_desc;
	{
		g_in_dxgi_runtime = true;
		HWND hwnd = nullptr;
		static_cast<IDXGISwapChain3 *>(_orig)->GetHwnd(&hwnd);
		DXGI_SWAP_CHAIN_DESC1 desc = {};
		static_cast<IDXGISwapChain3 *>(_orig)->GetDesc1(&desc);
		BOOL fullscreen = FALSE;
		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
		_orig->GetFullscreenState(&fullscreen, nullptr);
		g_in_dxgi_runtime = was_in_dxgi_runtime;

		// Forcefully restore the parameters that are willingly left to their previous value to the original value we cached, not the last one overriden by the addon, as that's not what the application expects
		if (BufferCount != 0)
			BufferCount = _orig_desc.BufferCount;
		if (NewFormat == DXGI_FORMAT_UNKNOWN)
			NewFormat = _orig_desc.BufferDesc.Format;

		desc.BufferCount = _orig_desc.BufferCount = BufferCount;
		desc.Width = _orig_desc.BufferDesc.Width = Width;
		desc.Height = _orig_desc.BufferDesc.Height = Height;
		desc.Format = _orig_desc.BufferDesc.Format = NewFormat;
		desc.Flags = _orig_desc.Flags = SwapChainFlags;

		fullscreen_desc.Windowed = !fullscreen;

		if (modify_swapchain_desc(_direct3d_version, desc, _sync_interval, &fullscreen_desc, hwnd))
		{
			_orig_desc.BufferCount = desc.BufferCount;

			BufferCount = desc.BufferCount;
			Width = desc.Width;
			Height = desc.Height;
			NewFormat = desc.Format;
			SwapChainFlags = desc.Flags;
		}
		// Disable using the original desc
		else
		{
			_orig_desc.BufferCount = 0;
		}
	}
#endif

	// Need to extract the original command queue object from the proxies passed in
	assert(ppPresentQueue != nullptr);
	temp_mem<IUnknown *> present_queues(BufferCount);
	for (UINT i = 0; i < BufferCount; ++i)
	{
		present_queues[i] = ppPresentQueue[i];
		com_ptr<IUnknown> command_queue_proxy;
		query_device(present_queues[i], command_queue_proxy);
	}

	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGISwapChain3 *>(_orig)->ResizeBuffers1(BufferCount, Width, Height, NewFormat, SwapChainFlags, pCreationNodeMask, present_queues.p);
#if RESHADE_ADDON
	if (SUCCEEDED(hr) && (Width == 0 || Height == 0))
	{
		DXGI_SWAP_CHAIN_DESC desc = {};
		_orig->GetDesc(&desc);
		_orig_desc.BufferDesc.Width = desc.BufferDesc.Width;
		_orig_desc.BufferDesc.Height = desc.BufferDesc.Height;
	}
	else if (FAILED(hr))
	{
		_orig_desc = prev_orig_desc;
	}
#endif
	g_in_dxgi_runtime = was_in_dxgi_runtime;
	if (SUCCEEDED(hr))
	{
		on_init(true);
	}
	else if (hr == DXGI_ERROR_INVALID_CALL)
	{
		reshade::log::message(reshade::log::level::warning, "IDXGISwapChain3::ResizeBuffers1 failed with error code DXGI_ERROR_INVALID_CALL.");
		on_init(true);
	}
	else
	{
		reshade::log::message(reshade::log::level::error, "IDXGISwapChain3::ResizeBuffers1 failed with error code %s!", reshade::log::hr_to_string(hr).c_str());
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData)
{
	// assert(_interface_version >= 4); // Red Dead Redemption 2 incorrectly calls this on a 'IDXGISwapChain3' object
#if 0
	return static_cast<IDXGISwapChain4 *>(_orig)->SetHDRMetaData(Type, Size, pMetaData);
#else
	UNREFERENCED_PARAMETER(Type);
	UNREFERENCED_PARAMETER(Size);
	UNREFERENCED_PARAMETER(pMetaData);
	// It is no longer recommended for apps to explicitly set HDR metadata, so just prevent this altogether (see also https://learn.microsoft.com/windows/win32/api/dxgi1_5/nf-dxgi1_5-idxgiswapchain4-sethdrmetadata)
	return S_OK;
#endif
}

class unique_direct3d_device_lock : std::unique_lock<std::recursive_mutex>
{
public:
	unique_direct3d_device_lock(IUnknown *direct3d_device, reshade::api::device_api direct3d_version, std::recursive_mutex &mutex) : unique_lock(mutex)
	{
		switch (direct3d_version)
		{
		case reshade::api::device_api::d3d10:
			// 'ID3D10Multithread' and 'ID3D11Multithread' are the same interface
			static_cast<D3D10Device *>(static_cast<ID3D10Device *>(direct3d_device))->_orig->QueryInterface(&multithread);
			break;
		case reshade::api::device_api::d3d11:
			static_cast<D3D11Device *>(static_cast<ID3D11Device *>(direct3d_device))->_immediate_context->_orig->QueryInterface(&multithread);
			break;
		}

		if (multithread != nullptr)
		{
			was_protected = multithread->GetMultithreadProtected();
			multithread->SetMultithreadProtected(TRUE);
			multithread->Enter();
		}
	}
	~unique_direct3d_device_lock()
	{
		if (multithread != nullptr)
		{
			multithread->Leave();
			multithread->SetMultithreadProtected(was_protected);
		}
	}

private:
	com_ptr<ID3D11Multithread> multithread;
	BOOL was_protected = FALSE;
};

void DXGISwapChain::on_init(bool resize)
{
	assert(!_is_initialized);

	const unique_direct3d_device_lock lock(_direct3d_device, _direct3d_version, _direct3d_version == reshade::api::device_api::d3d12 ? static_cast<D3D12CommandQueue *>(_direct3d_command_queue)->_mutex : _impl_mutex);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(_impl, resize);

	BOOL fullscreen = FALSE;
	com_ptr<IDXGIOutput> output;
	GetFullscreenState(&fullscreen, &output);

	HMONITOR hmonitor = nullptr;
	if (output != nullptr)
	{
		DXGI_OUTPUT_DESC output_desc = {};
		output->GetDesc(&output_desc);
		hmonitor = output_desc.Monitor;
	}

	reshade::invoke_addon_event<reshade::addon_event::set_fullscreen_state>(_impl, fullscreen != FALSE, hmonitor);
#else
	UNREFERENCED_PARAMETER(resize);
#endif

	reshade::init_effect_runtime(_impl);

	_is_initialized = true;
}
void DXGISwapChain::on_reset(bool resize)
{
	if (!_is_initialized)
		return;

	const unique_direct3d_device_lock lock(_direct3d_device, _direct3d_version, _direct3d_version == reshade::api::device_api::d3d12 ? static_cast<D3D12CommandQueue *>(_direct3d_command_queue)->_mutex : _impl_mutex);

	reshade::reset_effect_runtime(_impl);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(_impl, resize);
#else
	UNREFERENCED_PARAMETER(resize);
#endif

	_is_initialized = false;
}

void DXGISwapChain::on_present(UINT flags, [[maybe_unused]] const DXGI_PRESENT_PARAMETERS *params)
{
	// Some D3D11 games test presentation for timing and composition purposes
	// These calls are not rendering related, but rather a status request for the D3D runtime and as such should be ignored
	if ((flags & DXGI_PRESENT_TEST) != 0)
		return;

	// Also skip when the same frame is presented multiple times
	if ((flags & DXGI_PRESENT_DO_NOT_WAIT) != 0 && _was_still_drawing_last_frame)
		return;
	assert(!_was_still_drawing_last_frame);

	assert(_is_initialized);

	// Synchronize access to effect runtime to avoid race conditions between 'load_effects' and 'destroy_effects' causing crashes
	// This is necessary because Resident Evil 3 calls D3D11 and DXGI functions simultaneously from multiple threads
	// In case of D3D12, also synchronize access to the command queue while events are invoked and the immediate command list may be accessed
	const unique_direct3d_device_lock lock(_direct3d_device, _direct3d_version, _direct3d_version == reshade::api::device_api::d3d12 ? static_cast<D3D12CommandQueue *>(_direct3d_command_queue)->_mutex : _impl_mutex);

	switch (_direct3d_version)
	{
	case reshade::api::device_api::d3d10:
#if RESHADE_ADDON
		// Behave as if immediate command list is flushed
		reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(
			static_cast<D3D10Device *>(static_cast<ID3D10Device *>(_direct3d_device)),
			static_cast<D3D10Device *>(static_cast<ID3D10Device *>(_direct3d_device)));

		reshade::invoke_addon_event<reshade::addon_event::present>(
			static_cast<D3D10Device *>(static_cast<ID3D10Device *>(_direct3d_device)),
			_impl,
			nullptr,
			nullptr,
			params != nullptr ? params->DirtyRectsCount : 0,
			params != nullptr ? reinterpret_cast<const reshade::api::rect *>(params->pDirtyRects) : nullptr);
#endif
		reshade::present_effect_runtime(_impl);
		break;
	case reshade::api::device_api::d3d11:
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::execute_command_list>(
			static_cast<D3D11Device *>(static_cast<ID3D11Device *>(_direct3d_device))->_immediate_context,
			static_cast<D3D11Device *>(static_cast<ID3D11Device *>(_direct3d_device))->_immediate_context);

		reshade::invoke_addon_event<reshade::addon_event::present>(
			static_cast<D3D11Device *>(static_cast<ID3D11Device *>(_direct3d_device))->_immediate_context,
			_impl,
			nullptr,
			nullptr,
			params != nullptr ? params->DirtyRectsCount : 0,
			params != nullptr ? reinterpret_cast<const reshade::api::rect *>(params->pDirtyRects) : nullptr);
#endif
		reshade::present_effect_runtime(_impl);
		break;
	case reshade::api::device_api::d3d12:
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(
			static_cast<D3D12CommandQueue *>(_direct3d_command_queue),
			_impl,
			nullptr,
			nullptr,
			params != nullptr ? params->DirtyRectsCount : 0,
			params != nullptr ? reinterpret_cast<const reshade::api::rect *>(params->pDirtyRects) : nullptr);
#endif
		reshade::present_effect_runtime(_impl);
		static_cast<D3D12CommandQueue *>(_direct3d_command_queue)->flush_immediate_command_list();
		break;
	}
}

void DXGISwapChain::handle_device_loss(HRESULT hr)
{
	_was_still_drawing_last_frame = (hr == DXGI_ERROR_WAS_STILL_DRAWING);

	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		reshade::log::message(reshade::log::level::error, "Device was lost with %s!", reshade::log::hr_to_string(hr).c_str());

		if (hr == DXGI_ERROR_DEVICE_REMOVED)
		{
			HRESULT reason = DXGI_ERROR_INVALID_CALL;
			switch (_direct3d_version)
			{
			case reshade::api::device_api::d3d10:
				reason = static_cast<ID3D10Device *>(_direct3d_device)->GetDeviceRemovedReason();
				break;
			case reshade::api::device_api::d3d11:
				reason = static_cast<ID3D11Device *>(_direct3d_device)->GetDeviceRemovedReason();
				break;
			case reshade::api::device_api::d3d12:
				reason = static_cast<ID3D12Device *>(_direct3d_device)->GetDeviceRemovedReason();
				break;
			}

			reshade::log::message(reshade::log::level::error, "> Device removal reason is %s.", reshade::log::hr_to_string(reason).c_str());
		}
	}
}
