/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_swapchain.hpp"
#include "d3d10_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads

reshade::d3d10::swapchain_impl::swapchain_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain, device, device),
	_app_state(device->_orig)
{
	_renderer_id = device->_orig->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(device->_orig->QueryInterface(&dxgi_device)))
	{
		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter)))
		{
			if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
			{
				_vendor_id = desc.VendorId;
				_device_id = desc.DeviceId;

				LOG(INFO) << "Running on " << desc.Description;
			}
		}
	}

	if (_orig != nullptr)
		on_init();
}
reshade::d3d10::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::d3d10::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(_backbuffer.get());
}

bool reshade::d3d10::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;
	assert(_backbuffer != nullptr);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d10::swapchain_impl::on_reset()
{
	if (_backbuffer == nullptr)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	_backbuffer.reset();
}

void reshade::d3d10::swapchain_impl::on_present()
{
	if (!is_initialized())
		return;

	_app_state.capture();

	runtime::on_present();

	// Apply previous state from application
	_app_state.apply_and_release();
}

#if RESHADE_FX
void reshade::d3d10::swapchain_impl::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	_app_state.capture();

	runtime::render_effects(cmd_list, rtv, rtv_srgb);

	_app_state.apply_and_release();
}
#endif
