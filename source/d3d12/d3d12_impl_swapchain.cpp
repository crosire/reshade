/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_queue.hpp"
#include "d3d12_impl_swapchain.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include <CoreWindow.h>

reshade::d3d12::swapchain_impl::swapchain_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain) :
	api_object_impl(swapchain, device, queue)
{
	_renderer_id = D3D_FEATURE_LEVEL_12_0;

	// There is no swap chain in d3d12on7
	if (com_ptr<IDXGIFactory4> factory;
		_orig != nullptr && SUCCEEDED(_orig->GetParent(IID_PPV_ARGS(&factory))))
	{
		const LUID luid = device->_orig->GetAdapterLuid();

		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&dxgi_adapter))))
		{
			if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
			{
				_vendor_id = desc.VendorId;
				_device_id = desc.DeviceId;

				LOG(INFO) << "Running on " << desc.Description;
			}
		}
	}

	// Default to three back buffers for d3d12on7
	_backbuffers.resize(3);

	if (_orig != nullptr)
		on_init();
}
reshade::d3d12::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::d3d12::swapchain_impl::get_back_buffer(uint32_t index)
{
	return to_handle(_backbuffers[index].get());
}
reshade::api::resource reshade::d3d12::swapchain_impl::get_back_buffer_resolved(uint32_t index)
{
	return to_handle(_backbuffers[index].get());
}

uint32_t reshade::d3d12::swapchain_impl::get_back_buffer_count() const
{
	return static_cast<uint32_t>(_backbuffers.size());
}
uint32_t reshade::d3d12::swapchain_impl::get_current_back_buffer_index() const
{
	return _swap_index;
}

bool reshade::d3d12::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	// Get description from IDXGISwapChain interface, since later versions are slightly different
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	// Update window handle in swap chain description for UWP applications
	if (HWND hwnd = nullptr; SUCCEEDED(_orig->GetHwnd(&hwnd)))
		swap_desc.OutputWindow = hwnd;
	else if (com_ptr<ICoreWindowInterop> window_interop; // Get window handle of the core window
		SUCCEEDED(_orig->GetCoreWindow(IID_PPV_ARGS(&window_interop))) && SUCCEEDED(window_interop->get_WindowHandle(&hwnd)))
		swap_desc.OutputWindow = hwnd;

	if (swap_desc.SampleDesc.Count > 1)
	{
		LOG(WARN) << "Multisampled swap chains are unsupported with D3D12.";
		return false;
	}

	// Get back buffer textures
	_backbuffers.resize(swap_desc.BufferCount);
	for (UINT i = 0; i < swap_desc.BufferCount; ++i)
	{
		if (FAILED(_orig->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i]))))
			return false;
		assert(_backbuffers[i] != nullptr);
	}

	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_back_buffer_format = convert_format(swap_desc.BufferDesc.Format);

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d12::swapchain_impl::on_reset()
{
	if (_backbuffers.empty())
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	// Make sure none of the resources below are currently in use (provided the runtime was initialized previously)
	_graphics_queue->wait_idle();

	_backbuffers.clear();
}

void reshade::d3d12::swapchain_impl::on_present()
{
	// There is no swap chain in d3d12on7
	if (_orig != nullptr)
		_swap_index = _orig->GetCurrentBackBufferIndex();

	if (!is_initialized())
		return;

	runtime::on_present();
}

bool reshade::d3d12::swapchain_impl::on_present(ID3D12Resource *source, HWND hwnd)
{
	assert(source != nullptr);

	_swap_index = (_swap_index + 1) % 3;

	// Update source texture render target view
	if (_backbuffers[_swap_index] != source)
	{
		runtime::on_reset();

#if RESHADE_ADDON
		if (_backbuffers[0] != nullptr)
		{
			invoke_addon_event<addon_event::destroy_swapchain>(this);
		}
#endif

		_backbuffers[_swap_index]  = source;

		// Do not initialize before all back buffers have been set
		// The first to be set is at index 1 due to the addition above, so it is sufficient to check the last to be set, which will be at index 0
		if (_backbuffers[0] != nullptr)
		{
			const D3D12_RESOURCE_DESC source_desc = source->GetDesc();

			_width = static_cast<UINT>(source_desc.Width);
			_height = source_desc.Height;
			_back_buffer_format = convert_format(source_desc.Format);

#if RESHADE_ADDON
			invoke_addon_event<addon_event::init_swapchain>(this);
#endif

			if (!runtime::on_init(hwnd))
				return false;
		}
	}

	// Is not initialized the first 3 frames, but that is fine, since 'on_present' does a 'is_initialized' check
	on_present();
	return true;
}
