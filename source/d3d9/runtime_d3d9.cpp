/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_d3d9.hpp"
#include "runtime_objects.hpp"
#include <d3dcompiler.h>

reshade::d3d9::runtime_impl::runtime_impl(device_impl *device, IDirect3DSwapChain9 *swapchain) :
	api_object_impl(swapchain),
	_device(device->_orig),
	_device_impl(device),
	_app_state(device->_orig)
{
	_renderer_id = 0x9000;

	if (D3DADAPTER_IDENTIFIER9 adapter_desc;
		SUCCEEDED(_device_impl->_d3d->GetAdapterIdentifier(_device_impl->_cp.AdapterOrdinal, 0, &adapter_desc)))
	{
		_vendor_id = adapter_desc.VendorId;
		_device_id = adapter_desc.DeviceId;

		// Only the last 5 digits represents the version specific to a driver
		// See https://docs.microsoft.com/windows-hardware/drivers/display/version-numbers-for-display-drivers
		const DWORD driver_version = LOWORD(adapter_desc.DriverVersion.LowPart) + (HIWORD(adapter_desc.DriverVersion.LowPart) % 10) * 10000;
		LOG(INFO) << "Running on " << adapter_desc.Description << " Driver " << (driver_version / 100) << '.' << (driver_version % 100);
	}

	if (!on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << this << '!';
}
reshade::d3d9::runtime_impl::~runtime_impl()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d9::runtime_impl::on_init()
{
	// Retrieve present parameters here, instead using the ones passed in during creation, to get correct values for 'BackBufferWidth' and 'BackBufferHeight'
	// They may otherwise still be set to zero (which is valid for creation)
	D3DPRESENT_PARAMETERS pp;
	if (FAILED(_orig->GetPresentParameters(&pp)))
		return false;

	RECT window_rect = {};
	GetClientRect(pp.hDeviceWindow, &window_rect);

	_width = pp.BackBufferWidth;
	_height = pp.BackBufferHeight;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_backbuffer_format = pp.BackBufferFormat;

	switch (_backbuffer_format)
	{
	default:
		_color_bit_depth = 0;
		break;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		_color_bit_depth = 5;
		break;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_X8B8G8R8:
		_color_bit_depth = 8;
		break;
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_A2B10G10R10_XR_BIAS:
		_color_bit_depth = 10;
		break;
	}

	// Get back buffer surface
	HRESULT hr = _orig->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer);
	assert(SUCCEEDED(hr));

	if (pp.MultiSampleType != D3DMULTISAMPLE_NONE || (pp.BackBufferFormat == D3DFMT_X8R8G8B8 || pp.BackBufferFormat == D3DFMT_X8B8G8R8))
	{
		// Some effects rely on there being an alpha channel available, so create custom back buffer in case that is not the case
		switch (_backbuffer_format)
		{
		case D3DFMT_X8R8G8B8:
			_backbuffer_format = D3DFMT_A8R8G8B8;
			break;
		case D3DFMT_X8B8G8R8:
			_backbuffer_format = D3DFMT_A8B8G8R8;
			break;
		}

		if (FAILED(_device->CreateRenderTarget(_width, _height, _backbuffer_format, D3DMULTISAMPLE_NONE, 0, FALSE, &_backbuffer_resolved, nullptr)))
			return false;
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

	// Create state block object
	if (!_app_state.init_state_block())
		return false;

	return runtime::on_init(pp.hDeviceWindow);
}
void reshade::d3d9::runtime_impl::on_reset()
{
	runtime::on_reset();

	_app_state.release_state_block();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
}

void reshade::d3d9::runtime_impl::on_present()
{
	if (!_is_initialized || FAILED(_device->BeginScene()))
		return;

	_app_state.capture();
	BOOL software_rendering_enabled = FALSE;
	if ((_device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		software_rendering_enabled = _device->GetSoftwareVertexProcessing(),
		_device->SetSoftwareVertexProcessing(FALSE); // Disable software vertex processing since it is incompatible with programmable shaders

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		_device->StretchRect(_backbuffer.get(), nullptr, _backbuffer_resolved.get(), nullptr, D3DTEXF_NONE);

	update_and_render_effects();
	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer.get(), nullptr, D3DTEXF_NONE);

	// Apply previous state from application
	_app_state.apply_and_release();
	if ((_device_impl->_cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		_device->SetSoftwareVertexProcessing(software_rendering_enabled);

	_device->EndScene();
}

bool reshade::d3d9::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso)
{
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_43.dll");

	if (_d3d_compiler == nullptr)
	{
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\")!" << " Make sure you have the DirectX end-user runtime (June 2010) installed or a newer version of the library in the application directory.";
		return false;
	}

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	// Add specialization constant defines to source code
	const std::string hlsl =
		"#define COLOR_PIXEL_SIZE 1.0 / " + std::to_string(_width) + ", 1.0 / " + std::to_string(_height) + "\n"
		"#define DEPTH_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
		"#define SV_DEPTH_PIXEL_SIZE DEPTH_PIXEL_SIZE\n"
		"#define SV_TARGET_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
		"#line 1\n" + // Reset line number, so it matches what is shown when viewing the generated code
		effect.preamble +
		effect.module.hlsl;

	// Overwrite position semantic in pixel shaders
	const D3D_SHADER_MACRO ps_defines[] = {
		{ "POSITION", "VPOS" }, { nullptr, nullptr }
	};

	HRESULT hr = E_FAIL;

	std::string profile;
	com_ptr<ID3DBlob> compiled, d3d_errors;

	switch (type)
	{
	case api::shader_stage::vertex:
		profile = "vs_3_0";
		break;
	case api::shader_stage::pixel:
		profile = "ps_3_0";
		break;
	case api::shader_stage::compute:
		assert(false);
		return false;
	}

	std::string attributes;
	attributes += "entrypoint=" + entry_point + ';';
	attributes += "profile=" + profile + ';';
	attributes += "flags=" + std::to_string(_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1) + ';';

	const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
	if (!load_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]))
	{
		hr = D3DCompile(
			hlsl.data(), hlsl.size(), nullptr,
			type == api::shader_stage::pixel ? ps_defines : nullptr,
			nullptr,
			entry_point.c_str(),
			profile.c_str(),
			_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1, 0,
			&compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		cso.resize(compiled->GetBufferSize());
		std::memcpy(cso.data(), compiled->GetBufferPointer(), cso.size());

		if (com_ptr<ID3DBlob> disassembled; SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &disassembled)))
			effect.assembly[entry_point].assign(static_cast<const char *>(disassembled->GetBufferPointer()), disassembled->GetBufferSize() - 1);

		save_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]);
	}

	return true;
}
