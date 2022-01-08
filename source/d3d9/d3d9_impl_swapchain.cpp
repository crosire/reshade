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
reshade::api::resource reshade::d3d9::swapchain_impl::get_back_buffer_resolved(uint32_t index)
{
	assert(index == 0);

	return to_handle(static_cast<IDirect3DResource9 *>(_backbuffer_resolved.get()));
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

	_width = pp.BackBufferWidth;
	_height = pp.BackBufferHeight;
	_back_buffer_format = convert_format(pp.BackBufferFormat);

	if (pp.MultiSampleType != D3DMULTISAMPLE_NONE || (pp.BackBufferFormat == D3DFMT_X8R8G8B8 || pp.BackBufferFormat == D3DFMT_X8B8G8R8))
	{
		// Some effects rely on there being an alpha channel available, so create custom back buffer if that is not the case
		switch (_back_buffer_format)
		{
		case api::format::r8g8b8x8_unorm:
			_back_buffer_format = api::format::r8g8b8a8_unorm;
			break;
		case api::format::b8g8r8x8_unorm:
			_back_buffer_format = api::format::b8g8r8a8_unorm;
			break;
		}

		if (FAILED(static_cast<device_impl *>(_device)->_orig->CreateRenderTarget(_width, _height, convert_format(_back_buffer_format), D3DMULTISAMPLE_NONE, 0, FALSE, &_backbuffer_resolved, nullptr)))
		{
			LOG(ERROR) << "Failed to create back buffer resolve render target!";
			return false;
		}
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

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
	_backbuffer_resolved.reset();
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

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		device_impl->_orig->StretchRect(_backbuffer.get(), nullptr, _backbuffer_resolved.get(), nullptr, D3DTEXF_NONE);

	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		device_impl->_orig->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer.get(), nullptr, D3DTEXF_NONE);

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
