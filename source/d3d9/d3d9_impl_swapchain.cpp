/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "addon_manager.hpp"

reshade::d3d9::swapchain_impl::swapchain_impl(device_impl *device, IDirect3DSwapChain9 *swapchain) :
	api_object_impl(swapchain, device, device)
{
	on_init();
}
reshade::d3d9::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::d3d9::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(static_cast<IDirect3DResource9 *>(_back_buffer.get()));
}

bool reshade::d3d9::swapchain_impl::on_init()
{
	// Get back buffer surface
	if (FAILED(_orig->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_back_buffer)))
		return false;
	assert(_back_buffer != nullptr);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	// Always retrieve present parameters from device, since the application may have set 'hDeviceWindow' to zero and instead used 'hFocusWindow' (e.g. Resident Evil 4)
	// Retrieving them from the device ensures that 'hDeviceWindow' is always set (with either the original value from 'hDeviceWindow' or 'hFocusWindow')
	D3DPRESENT_PARAMETERS pp = {};
	_orig->GetPresentParameters(&pp);
	assert(pp.hDeviceWindow != nullptr);

	return runtime::on_init(pp.hDeviceWindow);
}
void reshade::d3d9::swapchain_impl::on_reset()
{
	if (_back_buffer == nullptr)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	_back_buffer.reset();
}

void reshade::d3d9::swapchain_impl::on_present()
{
	const auto device = static_cast<device_impl *>(_device);

	if (FAILED(device->_orig->BeginScene()))
		return;

	runtime::on_present(device);

	device->_orig->EndScene();
}
