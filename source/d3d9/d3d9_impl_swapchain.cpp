/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_swapchain.hpp"
#include "d3d9_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads

reshade::d3d9::swapchain_impl::swapchain_impl(device_impl *device, IDirect3DSwapChain9 *swapchain) :
	api_object_impl(swapchain, device, device),
	_app_state(device->_orig)
{
	_renderer_id = 0x9000;

	if (D3DADAPTER_IDENTIFIER9 adapter_desc;
		SUCCEEDED(device->_d3d->GetAdapterIdentifier(device->_cp.AdapterOrdinal, 0, &adapter_desc)))
	{
		_vendor_id = adapter_desc.VendorId;
		_device_id = adapter_desc.DeviceId;

		// Only the last 5 digits represents the version specific to a driver
		// See https://docs.microsoft.com/windows-hardware/drivers/display/version-numbers-for-display-drivers
		const DWORD driver_version = LOWORD(adapter_desc.DriverVersion.LowPart) + (HIWORD(adapter_desc.DriverVersion.LowPart) % 10) * 10000;
		LOG(INFO) << "Running on " << adapter_desc.Description << " Driver " << (driver_version / 100) << '.' << (driver_version % 100);
	}

	D3DPRESENT_PARAMETERS pp = {};
	_orig->GetPresentParameters(&pp);

	on_init(pp);
}
reshade::d3d9::swapchain_impl::~swapchain_impl()
{
	on_reset();
}

reshade::api::resource reshade::d3d9::swapchain_impl::get_back_buffer(uint32_t index)
{
	assert(index == 0);

	return to_handle(static_cast<IDirect3DResource9 *>(_backbuffer.get()));
}

bool reshade::d3d9::swapchain_impl::on_init(const D3DPRESENT_PARAMETERS &pp)
{
	// Get back buffer surface
	if (FAILED(_orig->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer)))
		return false;
	assert(_backbuffer != nullptr);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_swapchain>(this);
#endif

	return runtime::on_init(pp.hDeviceWindow);
}
void reshade::d3d9::swapchain_impl::on_reset()
{
	if (_backbuffer == nullptr)
		return;

	runtime::on_reset();

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_swapchain>(this);
#endif

	_backbuffer.reset();
}

void reshade::d3d9::swapchain_impl::on_present()
{
	const auto device_impl = static_cast<class device_impl *>(_device);

	if (!is_initialized() || FAILED(device_impl->_orig->BeginScene()))
		return;

	_app_state.capture();
	BOOL software_rendering_enabled = FALSE;
	if ((device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		software_rendering_enabled = device_impl->_orig->GetSoftwareVertexProcessing(),
		device_impl->_orig->SetSoftwareVertexProcessing(FALSE); // Disable software vertex processing since it is incompatible with programmable shaders

	runtime::on_present();

	// Apply previous state from application
	_app_state.apply_and_release();
	if ((device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		device_impl->_orig->SetSoftwareVertexProcessing(software_rendering_enabled);

	device_impl->_orig->EndScene();
}

#if RESHADE_FX
void reshade::d3d9::swapchain_impl::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	const auto device_impl = static_cast<class device_impl *>(_device);

	_app_state.capture();
	BOOL software_rendering_enabled = FALSE;
	if ((device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		software_rendering_enabled = device_impl->_orig->GetSoftwareVertexProcessing(),
		device_impl->_orig->SetSoftwareVertexProcessing(FALSE); // Disable software vertex processing since it is incompatible with programmable shaders

	runtime::render_effects(cmd_list, rtv, rtv_srgb);

	_app_state.apply_and_release();
	if ((device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		device_impl->_orig->SetSoftwareVertexProcessing(software_rendering_enabled);
}
#endif
