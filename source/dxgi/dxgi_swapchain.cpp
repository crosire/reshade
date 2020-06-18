/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d10/runtime_d3d10.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d11/d3d11_device_context.hpp"
#include "d3d11/runtime_d3d11.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "d3d12/runtime_d3d12.hpp"
#include <algorithm>

extern UINT query_device(IUnknown *&device, com_ptr<IUnknown> &device_proxy);

thread_local bool g_in_dxgi_runtime = false;

DXGISwapChain::DXGISwapChain(D3D10Device *device, IDXGISwapChain  *original, const std::shared_ptr<reshade::runtime> &runtime) :
	_orig(original),
	_interface_version(0),
	_direct3d_device(device, false), // Explicitly add a reference to the device
	_direct3d_version(10),
	_runtime(runtime) {
	assert(_orig != nullptr && _direct3d_device != nullptr && _runtime != nullptr);
}
DXGISwapChain::DXGISwapChain(D3D10Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
	_orig(original),
	_interface_version(1),
	_direct3d_device(device, false),
	_direct3d_version(10),
	_runtime(runtime) {
	assert(_orig != nullptr && _direct3d_device != nullptr && _runtime != nullptr);
}
DXGISwapChain::DXGISwapChain(D3D11Device *device, IDXGISwapChain  *original, const std::shared_ptr<reshade::runtime> &runtime) :
	_orig(original),
	_interface_version(0),
	_direct3d_device(device, false),
	_direct3d_version(11),
	_runtime(runtime) {
	assert(_orig != nullptr && _direct3d_device != nullptr && _runtime != nullptr);
}
DXGISwapChain::DXGISwapChain(D3D11Device *device, IDXGISwapChain1 *original, const std::shared_ptr<reshade::runtime> &runtime) :
	_orig(original),
	_interface_version(1),
	_direct3d_device(device, false),
	_direct3d_version(11),
	_runtime(runtime) {
	assert(_orig != nullptr && _direct3d_device != nullptr && _runtime != nullptr);
}
DXGISwapChain::DXGISwapChain(D3D12Device *device, IDXGISwapChain3 *original, const std::shared_ptr<reshade::runtime> &runtime) :
	_orig(original),
	_interface_version(3),
	_direct3d_device(device, false),
	_direct3d_version(12),
	_runtime(runtime) {
	assert(_orig != nullptr && _direct3d_device != nullptr && _runtime != nullptr);
}

void DXGISwapChain::runtime_reset()
{
	const std::lock_guard<std::mutex> lock(_runtime_mutex);

	switch (_direct3d_version)
	{
	case 10:
		std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_reset();
		break;
	case 11:
		std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_reset();
		break;
	case 12:
		std::static_pointer_cast<reshade::d3d12::runtime_d3d12>(_runtime)->on_reset();
		break;
	}
}
void DXGISwapChain::runtime_resize()
{
	DXGI_SWAP_CHAIN_DESC desc;
	// Get description from IDXGISwapChain interface, since later versions are slightly different
	if (FAILED(_orig->GetDesc(&desc)))
		return;

	const std::lock_guard<std::mutex> lock(_runtime_mutex);

	bool initialized = false;
	switch (_direct3d_version)
	{
	case 10:
		initialized = std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_init(desc);
		break;
	case 11:
		initialized = std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_init(desc);
		break;
	case 12:
		initialized = std::static_pointer_cast<reshade::d3d12::runtime_d3d12>(_runtime)->on_init(desc);
		break;
	}

	if (!initialized)
		LOG(ERROR) << "Failed to recreate Direct3D " << _direct3d_version << " runtime environment on runtime " << _runtime.get() << '.';
}
void DXGISwapChain::runtime_present(UINT flags)
{
	// Some D3D11 games test presentation for timing and composition purposes
	// These calls are not rendering related, but rather a status request for the D3D runtime and as such should be ignored
	if (flags & DXGI_PRESENT_TEST)
		return;

	// Synchronize access to runtime to avoid race conditions between 'load_effects' and 'unload_effects' causing crashes
	// This is necessary because Resident Evil 3 calls DXGI functions simultaneously from multiple threads (which is technically illegal)
	const std::lock_guard<std::mutex> lock(_runtime_mutex);

	switch (_direct3d_version)
	{
	case 10: {
		const auto device = static_cast<D3D10Device *>(_direct3d_device.get());
		std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_present();
		device->_buffer_detection.reset(false);
		break; }
	case 11: {
		const auto device = static_cast<D3D11Device *>(_direct3d_device.get());
		std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_present();
		device->_immediate_context->_buffer_detection.reset(false);
		break; }
	case 12: {
		const auto device = static_cast<D3D12Device *>(_direct3d_device.get());
		std::static_pointer_cast<reshade::d3d12::runtime_d3d12>(_runtime)->on_present();
		device->_buffer_detection.reset(false);
		break; }
	}
}

void DXGISwapChain::handle_runtime_loss(HRESULT hr)
{
	if (!_runtime->is_initialized())
		return;

	// Handle scenarios where device is lost and just clean up all resources
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		LOG(ERROR) << "Device was lost with " << hr << ". Destroying all resources and disabling ReShade.";

		if (hr == DXGI_ERROR_DEVICE_REMOVED)
		{
			HRESULT reason = DXGI_ERROR_INVALID_CALL;
			switch (_direct3d_version)
			{
			case 10:
				reason = static_cast<D3D10Device *>(_direct3d_device.get())->GetDeviceRemovedReason();
				break;
			case 11:
				reason = static_cast<D3D11Device *>(_direct3d_device.get())->GetDeviceRemovedReason();
				break;
			case 12:
				reason = static_cast<D3D12Device *>(_direct3d_device.get())->GetDeviceRemovedReason();
				break;
			}

			LOG(ERROR) << "> Device removal reason is " << reason << '.';
		}

		runtime_reset();
	}
}

bool DXGISwapChain::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(IDXGIObject) ||
		riid == __uuidof(IDXGIDeviceSubObject))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(IDXGISwapChain),
		__uuidof(IDXGISwapChain1),
		__uuidof(IDXGISwapChain2),
		__uuidof(IDXGISwapChain3),
		__uuidof(IDXGISwapChain4),
	};

	for (unsigned int version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgraded IDXGISwapChain" << _interface_version << " object " << this << " to IDXGISwapChain" << version << '.';
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
		return _orig->Release(), ref;

	switch (_direct3d_version)
	{
	case 10:
		std::static_pointer_cast<reshade::d3d10::runtime_d3d10>(_runtime)->on_reset();
		static_cast<D3D10Device *>(_direct3d_device.get())->_buffer_detection.reset(true);
		break; 
	case 11:
		std::static_pointer_cast<reshade::d3d11::runtime_d3d11>(_runtime)->on_reset();
		static_cast<D3D11Device *>(_direct3d_device.get())->_immediate_context->_buffer_detection.reset(true);
		break;
	case 12:
		std::static_pointer_cast<reshade::d3d12::runtime_d3d12>(_runtime)->on_reset();
		static_cast<D3D12Device *>(_direct3d_device.get())->_buffer_detection.reset(true); // Release any live references to depth buffers etc.
		break;
	}

	_runtime.reset();
	_direct3d_device.reset();

	// Only release internal reference after the runtime has been reset, so any references it held are cleaned up at this point
	const ULONG ref_orig = _orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for IDXGISwapChain" << _interface_version << " object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed IDXGISwapChain" << _interface_version << " object " << this << '.';
#endif
	delete this;

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
	runtime_present(Flags);
	if (_force_vsync)
		SyncInterval = 1;

	g_in_dxgi_runtime = true;
	const HRESULT hr = _orig->Present(SyncInterval, Flags);
	g_in_dxgi_runtime = false;
	handle_runtime_loss(hr);
	return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void **ppSurface)
{
	return _orig->GetBuffer(Buffer, riid, ppSurface);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget)
{
	LOG(INFO) << "Redirecting IDXGISwapChain::SetFullscreenState" << '(' << "this = " << this << ", Fullscreen = " << (Fullscreen ? "TRUE" : "FALSE") << ", pTarget = " << pTarget << ')' << " ...";

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
	LOG(INFO) << "Redirecting IDXGISwapChain::ResizeBuffers" << '('
		<<   "this = " << this
		<< ", BufferCount = " << BufferCount
		<< ", Width = " << Width
		<< ", Height = " << Height
		<< ", NewFormat = " << NewFormat
		<< ", SwapChainFlags = " << std::hex << SwapChainFlags << std::dec
		<< ')' << " ...";

	runtime_reset();
	if (_force_resolution[0] != 0 &&
		_force_resolution[1] != 0)
		Width = _force_resolution[0],
		Height = _force_resolution[1];
	if (_force_10_bit_format)
		NewFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

	const HRESULT hr = _orig->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
	if (hr == DXGI_ERROR_INVALID_CALL) // Ignore invalid call errors since the device is still in a usable state afterwards
	{
		LOG(WARN) << "IDXGISwapChain::ResizeBuffers failed with error code DXGI_ERROR_INVALID_CALL!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "IDXGISwapChain::ResizeBuffers failed with error code " << hr << '!';
		return hr;
	}

	runtime_resize();

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
	runtime_present(PresentFlags);
	if (_force_vsync)
		SyncInterval = 1;

	assert(_interface_version >= 1);
	g_in_dxgi_runtime = true;
	const HRESULT hr = static_cast<IDXGISwapChain1 *>(_orig)->Present1(SyncInterval, PresentFlags, pPresentParameters);
	g_in_dxgi_runtime = false;
	handle_runtime_loss(hr);
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
	assert(_interface_version >= 3);
	return static_cast<IDXGISwapChain3 *>(_orig)->SetColorSpace1(ColorSpace);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask, IUnknown *const *ppPresentQueue)
{
	LOG(INFO) << "Redirecting IDXGISwapChain3::ResizeBuffers1" << '('
		<<   "this = " << this
		<< ", BufferCount = " << BufferCount
		<< ", Width = " << Width
		<< ", Height = " << Height
		<< ", Format = " << Format
		<< ", SwapChainFlags = " << std::hex << SwapChainFlags << std::dec
		<< ", pCreationNodeMask = " << pCreationNodeMask
		<< ", ppPresentQueue = " << ppPresentQueue
		<< ')' << " ...";

	runtime_reset();
	if (_force_resolution[0] != 0 &&
		_force_resolution[1] != 0)
		Width = _force_resolution[0],
		Height = _force_resolution[1];
	if (_force_10_bit_format)
		Format = DXGI_FORMAT_R10G10B10A2_UNORM;

	// Need to extract the original command queue object from the proxies passed in
	assert(ppPresentQueue != nullptr);
	std::vector<IUnknown *> present_queues(BufferCount);
	for (UINT i = 0; i < BufferCount; ++i)
	{
		present_queues[i] = ppPresentQueue[i];
		com_ptr<IUnknown> command_queue_proxy;
		query_device(present_queues[i], command_queue_proxy);
	}

	assert(_interface_version >= 3);
	const HRESULT hr = static_cast<IDXGISwapChain3 *>(_orig)->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, present_queues.data());
	if (hr == DXGI_ERROR_INVALID_CALL)
	{
		LOG(WARN) << "IDXGISwapChain3::ResizeBuffers1 failed with error code DXGI_ERROR_INVALID_CALL!";
	}
	else if (FAILED(hr))
	{
		LOG(ERROR) << "IDXGISwapChain3::ResizeBuffers1 failed with error code " << hr << '!';
		return hr;
	}

	runtime_resize();

	return hr;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData)
{
	assert(_interface_version >= 4);
	return static_cast<IDXGISwapChain4 *>(_orig)->SetHDRMetaData(Type, Size, pMetaData);
}
