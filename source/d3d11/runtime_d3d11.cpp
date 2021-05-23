/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_d3d11.hpp"
#include "runtime_objects.hpp"
#include "reshade_api_type_utils.hpp"
#include <d3dcompiler.h>

extern bool is_windows7();

extern const char *dxgi_format_to_string(DXGI_FORMAT format);

reshade::d3d11::runtime_impl::runtime_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device(device->_orig),
	_immediate_context(immediate_context->_orig),
	_device_impl(device),
	_immediate_context_impl(immediate_context),
	_app_state(device->_orig)
{
	_renderer_id = _device->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(_device->QueryInterface(&dxgi_device)))
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

	if (_orig != nullptr && !on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << this << '!';
}
reshade::d3d11::runtime_impl::~runtime_impl()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d11::runtime_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	return on_init(swap_desc);
}
bool reshade::d3d11::runtime_impl::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc)
{
	_width = _window_width = swap_desc.BufferDesc.Width;
	_height = _window_height = swap_desc.BufferDesc.Height;
	_backbuffer_format = swap_desc.BufferDesc.Format;

	// Only need to handle swap chain formats
	switch (_backbuffer_format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		_color_bit_depth = 8;
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		_color_bit_depth = 10;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		_color_bit_depth = 16;
		break;
	}

	if (swap_desc.OutputWindow != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(swap_desc.OutputWindow, &window_rect);
		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
	}

	// Get back buffer texture (skip when there is no swap chain, in which case it should already have been set in 'on_present')
	if (_orig != nullptr && FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;
	assert(_backbuffer != nullptr);

	D3D11_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = _width;
	tex_desc.Height = _height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = convert_format(api::format_to_typeless(convert_format(_backbuffer_format)));
	tex_desc.SampleDesc = { 1, 0 };
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	// Creating a render target view for the back buffer fails on Windows 8+, so use a intermediate texture there
	if (_orig != nullptr && (
		swap_desc.SampleDesc.Count > 1 ||
		convert_format(api::format_to_default_typed(convert_format(_backbuffer_format))) != _backbuffer_format ||
		!is_windows7()))
	{
		if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_resolved)))
			return false;
		if (FAILED(_device->CreateRenderTargetView(_backbuffer.get(), nullptr, &_backbuffer_rtv[2])))
			return false;
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

	// Create back buffer shader texture
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_texture)))
		return false;
	_device_impl->set_debug_name({ reinterpret_cast<uintptr_t>(_backbuffer_texture.get()) }, "ReShade back buffer");

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = convert_format(api::format_to_default_typed(convert_format(tex_desc.Format)));
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
	if (FAILED(_device->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv)))
		return false;

	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = convert_format(api::format_to_default_typed(convert_format(tex_desc.Format)));
	rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (FAILED(_device->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[0])))
		return false;
	rtv_desc.Format = convert_format(api::format_to_default_typed_srgb(convert_format(tex_desc.Format)));
	if (FAILED(_device->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[1])))
		return false;

	// Create copy states
	if (_copy_pixel_shader == nullptr)
	{
		const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
		if (FAILED(_device->CreatePixelShader(ps.data, ps.data_size, nullptr, &_copy_pixel_shader)))
			return false;
	}
	if (_copy_vertex_shader == nullptr)
	{
		const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
		if (FAILED(_device->CreateVertexShader(vs.data, vs.data_size, nullptr, &_copy_vertex_shader)))
			return false;
	}

	if (_copy_sampler_state == nullptr)
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

		if (FAILED(_device->CreateSamplerState(&desc, &_copy_sampler_state)))
			return false;
	}

	// Create effect states
	if (_effect_rasterizer == nullptr)
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;
		desc.DepthClipEnable = TRUE;

		if (FAILED(_device->CreateRasterizerState(&desc, &_effect_rasterizer)))
			return false;
	}

	// Clear reference to make Unreal Engine 4 happy (which checks the reference count)
	_backbuffer->Release();

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d11::runtime_impl::on_reset()
{
	if (_backbuffer != nullptr)
	{
		unsigned int add_references = 0;
		// Resident Evil 3 releases all references to the back buffer before calling 'IDXGISwapChain::ResizeBuffers', even ones it does not own
		// Releasing the references ReShade owns would then make the count negative, which consequently breaks DXGI validation, so reset those references here
		if (_backbuffer.ref_count() == 0)
			add_references = _backbuffer == _backbuffer_resolved ? 2 : 1;
		// Add the reference back that was released because of Unreal Engine 4
		else if (_is_initialized)
			add_references = 1;

		for (unsigned int i = 0; i < add_references; ++i)
			_backbuffer->AddRef();
	}

	runtime::on_reset();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_rtv[0].reset();
	_backbuffer_rtv[1].reset();
	_backbuffer_rtv[2].reset();
	_backbuffer_texture.reset();
	_backbuffer_texture_srv.reset();
}

void reshade::d3d11::runtime_impl::on_present()
{
	if (!_is_initialized)
		return;

	_app_state.capture(_immediate_context.get());

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		_immediate_context->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, _backbuffer_format);

	update_and_render_effects();
	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
	{
		_immediate_context->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		_immediate_context->IASetInputLayout(nullptr);
		const uintptr_t null = 0;
		_immediate_context->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D11Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		_immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_immediate_context->VSSetShader(_copy_vertex_shader.get(), nullptr, 0);
		_immediate_context->HSSetShader(nullptr, nullptr, 0);
		_immediate_context->DSSetShader(nullptr, nullptr, 0);
		_immediate_context->GSSetShader(nullptr, nullptr, 0);
		_immediate_context->PSSetShader(_copy_pixel_shader.get(), nullptr, 0);
		ID3D11SamplerState *const samplers[] = { _copy_sampler_state.get() };
		_immediate_context->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
		ID3D11ShaderResourceView *const srvs[] = { _backbuffer_texture_srv.get() };
		_immediate_context->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		_immediate_context->RSSetState(_effect_rasterizer.get());
		const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(_width), static_cast<FLOAT>(_height), 0.0f, 1.0f };
		_immediate_context->RSSetViewports(1, &viewport);
		_immediate_context->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		_immediate_context->OMSetDepthStencilState(nullptr, D3D11_DEFAULT_STENCIL_REFERENCE);
		ID3D11RenderTargetView *const render_targets[] = { _backbuffer_rtv[2].get() };
		_immediate_context->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

		_immediate_context->Draw(3, 0);
	}

	// Apply previous state from application
	_app_state.apply_and_release();
}
bool reshade::d3d11::runtime_impl::on_layer_submit(UINT eye, ID3D11Texture2D *source, const float bounds[4], ID3D11Texture2D **target)
{
	assert(eye < 2 && source != nullptr);

	D3D11_TEXTURE2D_DESC source_desc;
	source->GetDesc(&source_desc);

	if (source_desc.SampleDesc.Count > 1)
		return false; // When the resource is multisampled, 'CopySubresourceRegion' can only copy whole subresources

	D3D11_BOX source_region = { 0, 0, 0, source_desc.Width, source_desc.Height, 1 };
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

	if (target_width != _width || region_height != _height || source_desc.Format != _backbuffer_format)
	{
		on_reset();

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Width = target_width;
		swap_desc.BufferDesc.Height = region_height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.SampleDesc = source_desc.SampleDesc;
		swap_desc.BufferCount = 1;

		source_desc.Width = target_width;
		source_desc.Height = region_height;
		source_desc.MipLevels = 1;
		source_desc.ArraySize = 1;
		source_desc.Format = convert_format(api::format_to_typeless(convert_format(source_desc.Format)));
		source_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		if (HRESULT hr = _device->CreateTexture2D(&source_desc, nullptr, &_backbuffer); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create region texture!" << " HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << source_desc.Width << ", Height = " << source_desc.Height << ", Format = " << source_desc.Format;
			return false;
		}

		if (!on_init(swap_desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << this << '!';
			return false;
		}
	}

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	_immediate_context->CopySubresourceRegion(_backbuffer.get(), 0, eye * region_width, 0, 0, source, source_desc.ArraySize == 2 ? eye : 0, &source_region);

	*target = _backbuffer.get();

	return true;
}

bool reshade::d3d11::runtime_impl::capture_screenshot(uint8_t *buffer) const
{
	if (_color_bit_depth != 8 && _color_bit_depth != 10)
	{
		if (const char *format_string = dxgi_format_to_string(_backbuffer_format); format_string != nullptr)
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << format_string << '!';
		else
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << _backbuffer_format << '!';
		return false;
	}

	// Create a texture in system memory, copy back buffer data into it and map it for reading
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = _width;
	desc.Height = _height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = _backbuffer_format;
	desc.SampleDesc = { 1, 0 };
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	com_ptr<ID3D11Texture2D> intermediate;
	if (HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &intermediate); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width << ", Height = " << desc.Height << ", Format = " << desc.Format;
		return false;
	}
	_device_impl->set_debug_name({ reinterpret_cast<uintptr_t>(intermediate.get()) }, "ReShade screenshot texture");

	_immediate_context->CopyResource(intermediate.get(), _backbuffer_resolved.get());

	D3D11_MAPPED_SUBRESOURCE mapped;
	if (FAILED(_immediate_context->Map(intermediate.get(), 0, D3D11_MAP_READ, 0, &mapped)))
		return false;
	auto mapped_data = static_cast<const uint8_t *>(mapped.pData);

	for (uint32_t y = 0, pitch = _width * 4; y < _height; y++, buffer += pitch, mapped_data += mapped.RowPitch)
	{
		if (_color_bit_depth == 10)
		{
			for (uint32_t x = 0; x < pitch; x += 4)
			{
				const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_data + x);
				// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
				buffer[x + 0] = ( (rgba & 0x000003FF)        /  4) & 0xFF;
				buffer[x + 1] = (((rgba & 0x000FFC00) >> 10) /  4) & 0xFF;
				buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) /  4) & 0xFF;
				buffer[x + 3] = (((rgba & 0xC0000000) >> 30) * 85) & 0xFF;
			}
		}
		else
		{
			std::memcpy(buffer, mapped_data, pitch);

			if (_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
				_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			{
				// Format is BGRA, but output should be RGBA, so flip channels
				for (uint32_t x = 0; x < pitch; x += 4)
					std::swap(buffer[x + 0], buffer[x + 2]);
			}
		}
	}

	_immediate_context->Unmap(intermediate.get(), 0);

	return true;
}

bool reshade::d3d11::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso)
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
		profile = "cs";
		// Feature level 10 and 10.1 support a limited form of DirectCompute, but it does not have support for RWTexture2D, so it is not useful here
		// See https://docs.microsoft.com/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader
		if (_renderer_id < D3D_FEATURE_LEVEL_11_0)
		{
			effect.errors += "Compute shaders are not supported in D3D10.";
			return false;
		}
		break;
	}

	switch (_renderer_id)
	{
	default:
	case D3D_FEATURE_LEVEL_11_0:
		profile += "_5_0";
		break;
	case D3D_FEATURE_LEVEL_10_1:
		profile += "_4_1";
		break;
	case D3D_FEATURE_LEVEL_10_0:
		profile += "_4_0";
		break;
	case D3D_FEATURE_LEVEL_9_1:
	case D3D_FEATURE_LEVEL_9_2:
		profile += "_4_0_level_9_1";
		break;
	case D3D_FEATURE_LEVEL_9_3:
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
