/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d10_impl_device.hpp"
#include "d3d10_impl_swapchain.hpp"
#include "d3d10_impl_type_convert.hpp"
#include "addon_manager.hpp"

reshade::d3d10::swapchain_impl::swapchain_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain, device, device)
{
	on_init();
}
reshade::d3d10::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::d3d10::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(_back_buffer.get());
}

bool reshade::d3d10::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_back_buffer))))
		return false;
	assert(_back_buffer != nullptr);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d10::swapchain_impl::on_reset()
{
	if (_back_buffer == nullptr)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	_back_buffer.reset();
}
