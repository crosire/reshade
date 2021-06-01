/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_command_queue.hpp"
#include "reshade_api_swapchain.hpp"
#include "reshade_api_type_convert.hpp"
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

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(this);
#endif

	// Default to three back buffers for d3d12on7
	_backbuffers.resize(3);

	if (_orig != nullptr && !on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
}
reshade::d3d12::swapchain_impl::~swapchain_impl()
{
	on_reset();

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this);
#endif
}

void reshade::d3d12::swapchain_impl::get_back_buffer(uint32_t index, api::resource *out)
{
	*out = { reinterpret_cast<uintptr_t>(_backbuffers[index].get()) };
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
	}

	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_backbuffer_format = convert_format(swap_desc.BufferDesc.Format);

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d12::swapchain_impl::on_reset()
{
	runtime::on_reset();

	// Make sure none of the resources below are currently in use (provided the runtime was initialized previously)
	_device->wait_idle();

	_backbuffers.clear();
}

void reshade::d3d12::swapchain_impl::on_present()
{
	if (!is_initialized())
		return;

	// There is no swap chain in d3d12on7
	if (_orig != nullptr)
		_swap_index = _orig->GetCurrentBackBufferIndex();

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

		_backbuffers[_swap_index]  = source;

		// Do not initialize before all back buffers have been set
		// The first to be set is at index 1 due to the addition above, so it is sufficient to check the last to be set, which will be at index 0
		if (_backbuffers[0] != nullptr)
		{
			const D3D12_RESOURCE_DESC source_desc = source->GetDesc();

			_width = static_cast<UINT>(source_desc.Width);
			_height = source_desc.Height;
			_backbuffer_format = convert_format(source_desc.Format);

			if (!runtime::on_init(hwnd))
			{
				LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
				return false;
			}
		}
	}

	// Is not initialized the first 3 frames, but that is fine, since 'on_present' does a 'is_initialized' check
	on_present();
	return true;
}
bool reshade::d3d12::swapchain_impl::on_layer_submit(UINT eye, ID3D12Resource *source, const float bounds[4], ID3D12Resource **target)
{
	assert(eye < 2 && source != nullptr);

	D3D12_RESOURCE_DESC source_desc = source->GetDesc();

	if (source_desc.SampleDesc.Count > 1)
		return false; // When the resource is multisampled, 'CopyTextureRegion' can only copy whole subresources

	D3D12_BOX source_region = { 0, 0, 0, static_cast<UINT>(source_desc.Width), source_desc.Height, 1 };
	if (bounds != nullptr)
	{
		source_region.left = static_cast<UINT>(source_desc.Width * std::min(bounds[0], bounds[2]));
		source_region.top  = static_cast<UINT>(source_desc.Height * std::min(bounds[1], bounds[3]));
		source_region.right = static_cast<UINT>(source_desc.Width * std::max(bounds[0], bounds[2]));
		source_region.bottom = static_cast<UINT>(source_desc.Height * std::max(bounds[1], bounds[3]));
	}

	const UINT region_width = source_region.right - source_region.left;
	const UINT target_width = region_width * 2;
	const UINT region_height = source_region.bottom - source_region.top;

	if (region_width == 0 || region_height == 0)
		return false;

	if (target_width != _width || region_height != _height || convert_format(source_desc.Format) != _backbuffer_format)
	{
		on_reset();

		_backbuffers.resize(1);

		source_desc.Width = target_width;
		source_desc.Height = region_height;
		source_desc.DepthOrArraySize = 1;
		source_desc.MipLevels = 1;
		source_desc.Format = convert_format(api::format_to_typeless(convert_format(source_desc.Format)));

		const D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

		if (HRESULT hr = static_cast<device_impl *>(_device)->_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &source_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_backbuffers[0])); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create region texture!" << " HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << source_desc.Width << ", Height = " << source_desc.Height << ", Format = " << source_desc.Format << ", Flags = " << std::hex << source_desc.Flags << std::dec;
			return false;
		}

		_is_vr = true;
		_width = target_width;
		_height = region_height;
		_backbuffer_format = convert_format(source_desc.Format);

		if (!runtime::on_init(nullptr))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
			return false;
		}
	}

	ID3D12GraphicsCommandList *const cmd_list = static_cast<command_list_immediate_impl *>(static_cast<command_queue_impl *>(_graphics_queue)->get_immediate_command_list())->begin_commands();

	D3D12_RESOURCE_BARRIER transitions[2];
	transitions[0] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transitions[0].Transition.pResource = source;
	transitions[0].Transition.Subresource = 0;
	transitions[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	transitions[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	transitions[1] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transitions[1].Transition.pResource = _backbuffers[0].get();
	transitions[1].Transition.Subresource = 0;
	transitions[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	transitions[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	cmd_list->ResourceBarrier(2, transitions);

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	D3D12_TEXTURE_COPY_LOCATION src_location;
	src_location.pResource = source;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = source_desc.DepthOrArraySize == 2 ? eye : 0;
	D3D12_TEXTURE_COPY_LOCATION dst_location;
	dst_location.pResource = _backbuffers[0].get();
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_location.SubresourceIndex = 0;
	cmd_list->CopyTextureRegion(&dst_location, eye * region_width, 0, 0, &src_location, &source_region);

	std::swap(transitions[0].Transition.StateBefore, transitions[0].Transition.StateAfter);
	std::swap(transitions[1].Transition.StateBefore, transitions[1].Transition.StateAfter);
	cmd_list->ResourceBarrier(2, transitions);

	*target = _backbuffers[0].get();

	return true;
}
