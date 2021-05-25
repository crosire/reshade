/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_d3d10.hpp"
#include "runtime_objects.hpp"
#include "reshade_api_type_utils.hpp"
#include <d3dcompiler.h>

extern bool is_windows7();

extern const char *dxgi_format_to_string(DXGI_FORMAT format);

reshade::d3d10::runtime_impl::runtime_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device),
	_app_state(device->_orig)
{
	_renderer_id = _device_impl->_orig->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(_device_impl->_orig->QueryInterface(&dxgi_device)))
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

	if (!on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 10 runtime environment on runtime " << this << '!';
}
reshade::d3d10::runtime_impl::~runtime_impl()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d10::runtime_impl::on_init()
{
	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	_width = _window_width = swap_desc.BufferDesc.Width;
	_height = _window_height = swap_desc.BufferDesc.Height;
	_backbuffer_format = convert_format(swap_desc.BufferDesc.Format);

	if (swap_desc.OutputWindow != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(swap_desc.OutputWindow, &window_rect);

		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
	}

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;

	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = _width;
	tex_desc.Height = _height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = convert_format(api::format_to_typeless(_backbuffer_format));
	tex_desc.SampleDesc = { 1, 0 };
	tex_desc.Usage = D3D10_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D10_BIND_RENDER_TARGET;

	// Creating a render target view for the back buffer fails on Windows 8+, so use a intermediate texture there
	if (swap_desc.SampleDesc.Count > 1 ||
		api::format_to_default_typed(_backbuffer_format) != _backbuffer_format ||
		!is_windows7())
	{
		if (FAILED(_device_impl->_orig->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_resolved)))
			return false;
		if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer.get(), nullptr, &_backbuffer_rtv[2])))
			return false;
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

	// Create back buffer shader texture
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	if (FAILED(_device_impl->_orig->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_texture)))
		return false;

	D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = convert_format(api::format_to_default_typed(_backbuffer_format));
	srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
	if (FAILED(_device_impl->_orig->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv)))
		return false;

	D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = convert_format(api::format_to_default_typed(_backbuffer_format));
	rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
	if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[0])))
		return false;
	rtv_desc.Format = convert_format(api::format_to_default_typed_srgb(_backbuffer_format));
	if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[1])))
		return false;

	// Create copy states
	if (_copy_pixel_shader == nullptr)
	{
		const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
		if (FAILED(_device_impl->_orig->CreatePixelShader(ps.data, ps.data_size, &_copy_pixel_shader)))
			return false;
	}
	if (_copy_vertex_shader == nullptr)
	{
		const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
		if (FAILED(_device_impl->_orig->CreateVertexShader(vs.data, vs.data_size, &_copy_vertex_shader)))
			return false;
	}

	if (_copy_sampler_state == nullptr)
	{
		D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;

		if (FAILED(_device_impl->_orig->CreateSamplerState(&desc, &_copy_sampler_state)))
			return false;
	}

	// Create effect states
	if (_effect_rasterizer == nullptr)
	{
		D3D10_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D10_FILL_SOLID;
		desc.CullMode = D3D10_CULL_NONE;
		desc.DepthClipEnable = TRUE;

		if (FAILED(_device_impl->_orig->CreateRasterizerState(&desc, &_effect_rasterizer)))
			return false;
	}

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d10::runtime_impl::on_reset()
{
	runtime::on_reset();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_rtv[0].reset();
	_backbuffer_rtv[1].reset();
	_backbuffer_rtv[2].reset();
	_backbuffer_texture.reset();
	_backbuffer_texture_srv.reset();
}

void reshade::d3d10::runtime_impl::on_present()
{
	if (!_is_initialized)
		return;

	ID3D10Device *const immediate_context = _device_impl->_orig;
	_app_state.capture();

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		immediate_context->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, convert_format(_backbuffer_format));

	update_and_render_effects();
	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
	{
		immediate_context->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		immediate_context->IASetInputLayout(nullptr);
		const uintptr_t null = 0;
		immediate_context->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		immediate_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		immediate_context->VSSetShader(_copy_vertex_shader.get());
		immediate_context->GSSetShader(nullptr);
		immediate_context->PSSetShader(_copy_pixel_shader.get());
		ID3D10SamplerState *const samplers[] = { _copy_sampler_state.get() };
		immediate_context->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
		ID3D10ShaderResourceView *const srvs[] = { _backbuffer_texture_srv.get() };
		immediate_context->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		immediate_context->RSSetState(_effect_rasterizer.get());
		const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
		immediate_context->RSSetViewports(1, &viewport);
		immediate_context->OMSetBlendState(nullptr, nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		immediate_context->OMSetDepthStencilState(nullptr, D3D10_DEFAULT_STENCIL_REFERENCE);
		ID3D10RenderTargetView *const render_targets[] = { _backbuffer_rtv[2].get() };
		immediate_context->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

		immediate_context->Draw(3, 0);
	}

	// Apply previous state from application
	_app_state.apply_and_release();
}

bool reshade::d3d10::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso)
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

	const std::string hlsl = effect.preamble + effect.module.hlsl;
	HRESULT hr = E_FAIL;

	std::string profile;
	switch (type)
	{
	case api::shader_stage::vertex:
		profile = "vs";
		break;
	case api::shader_stage::pixel:
		profile = "ps";
		break;
	case api::shader_stage::compute:
		assert(false);
		return false;
	}

	switch (_renderer_id)
	{
	case D3D10_FEATURE_LEVEL_10_1:
		profile += "_4_1";
		break;
	default:
	case D3D10_FEATURE_LEVEL_10_0:
		profile += "_4_0";
		break;
	case D3D10_FEATURE_LEVEL_9_1:
	case D3D10_FEATURE_LEVEL_9_2:
		profile += "_4_0_level_9_1";
		break;
	case D3D10_FEATURE_LEVEL_9_3:
		profile += "_4_0_level_9_3";
		break;
	}

	UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
	compile_flags |= (_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1);
#ifndef NDEBUG
	compile_flags |= D3DCOMPILE_DEBUG;
#endif

	std::string attributes;
	attributes += "entrypoint=" + entry_point + ';';
	attributes += "profile=" + profile + ';';
	attributes += "flags=" + std::to_string(compile_flags) + ';';

	const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
	if (!load_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]))
	{
		com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
		hr = D3DCompile(
			hlsl.data(), hlsl.size(),
			nullptr, nullptr, nullptr,
			entry_point.c_str(),
			profile.c_str(),
			compile_flags, 0,
			&d3d_compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		cso.resize(d3d_compiled->GetBufferSize());
		std::memcpy(cso.data(), d3d_compiled->GetBufferPointer(), cso.size());

		if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &d3d_disassembled)))
			effect.assembly[entry_point].assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

		save_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]);
	}

	return true;
}
