/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_d3d10.hpp"
#include "runtime_d3d10_objects.hpp"
#include "dxgi/format_utils.hpp"
#include <d3dcompiler.h>

extern bool is_windows7();

reshade::d3d10::runtime_impl::runtime_impl(device_impl *device, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device(device->_orig),
	_device_impl(device),
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

	RECT window_rect = {};
	GetClientRect(swap_desc.OutputWindow, &window_rect);

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_color_bit_depth = dxgi_format_color_depth(swap_desc.BufferDesc.Format);
	_backbuffer_format = swap_desc.BufferDesc.Format;

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;

	D3D10_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = _width;
	tex_desc.Height = _height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = make_dxgi_format_typeless(_backbuffer_format);
	tex_desc.SampleDesc = { 1, 0 };
	tex_desc.Usage = D3D10_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D10_BIND_RENDER_TARGET;

	// Creating a render target view for the back buffer fails on Windows 8+, so use a intermediate texture there
	if (swap_desc.SampleDesc.Count > 1 ||
		make_dxgi_format_normal(_backbuffer_format) != _backbuffer_format ||
		!is_windows7())
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
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_texture)))
		return false;

	D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = make_dxgi_format_normal(tex_desc.Format);
	srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
	if (FAILED(_device->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv[0])))
		return false;
	srv_desc.Format = make_dxgi_format_srgb(tex_desc.Format);
	if (FAILED(_device->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv[1])))
		return false;

	D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = make_dxgi_format_normal(tex_desc.Format);
	rtv_desc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
	if (FAILED(_device->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[0])))
		return false;
	rtv_desc.Format = make_dxgi_format_srgb(tex_desc.Format);
	if (FAILED(_device->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[1])))
		return false;

	// Create copy states
	if (_copy_pixel_shader == nullptr)
	{
		const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
		if (FAILED(_device->CreatePixelShader(ps.data, ps.data_size, &_copy_pixel_shader)))
			return false;
	}
	if (_copy_vertex_shader == nullptr)
	{
		const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
		if (FAILED(_device->CreateVertexShader(vs.data, vs.data_size, &_copy_vertex_shader)))
			return false;
	}

	if (_copy_sampler_state == nullptr)
	{
		D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;

		if (FAILED(_device->CreateSamplerState(&desc, &_copy_sampler_state)))
			return false;
	}

	// Create effect states
	if (_effect_rasterizer == nullptr)
	{
		D3D10_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D10_FILL_SOLID;
		desc.CullMode = D3D10_CULL_NONE;
		desc.DepthClipEnable = TRUE;

		if (FAILED(_device->CreateRasterizerState(&desc, &_effect_rasterizer)))
			return false;
	}

	// Create effect depth-stencil texture
	tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	tex_desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	com_ptr<ID3D10Texture2D> effect_depthstencil_texture;
	if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &effect_depthstencil_texture)))
		return false;
	if (FAILED(_device->CreateDepthStencilView(effect_depthstencil_texture.get(), nullptr, &_effect_stencil)))
		return false;

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

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
	_backbuffer_texture_srv[0].reset();
	_backbuffer_texture_srv[1].reset();

	_effect_stencil.reset();

#if RESHADE_GUI
	_imgui.cb.reset();
	_imgui.indices.reset();
	_imgui.vertices.reset();
	_imgui.num_indices = 0;
	_imgui.num_vertices = 0;
#endif
}

void reshade::d3d10::runtime_impl::on_present()
{
	if (!_is_initialized)
		return;

	_app_state.capture();

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		_device->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, _backbuffer_format);

	update_and_render_effects();
	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
	{
		_device->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		_device->IASetInputLayout(nullptr);
		const uintptr_t null = 0;
		_device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_device->VSSetShader(_copy_vertex_shader.get());
		_device->GSSetShader(nullptr);
		_device->PSSetShader(_copy_pixel_shader.get());
		ID3D10SamplerState *const samplers[] = { _copy_sampler_state.get() };
		_device->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
		ID3D10ShaderResourceView *const srvs[] = { _backbuffer_texture_srv[make_dxgi_format_srgb(_backbuffer_format) == _backbuffer_format].get() };
		_device->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		_device->RSSetState(_effect_rasterizer.get());
		const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
		_device->RSSetViewports(1, &viewport);
		_device->OMSetBlendState(nullptr, nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		_device->OMSetDepthStencilState(nullptr, D3D10_DEFAULT_STENCIL_REFERENCE);
		ID3D10RenderTargetView *const render_targets[] = { _backbuffer_rtv[2].get() };
		_device->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

		_device->Draw(3, 0);
	}

	// Apply previous state from application
	_app_state.apply_and_release();
}

bool reshade::d3d10::runtime_impl::capture_screenshot(uint8_t *buffer) const
{
	if (_color_bit_depth != 8 && _color_bit_depth != 10)
	{
		if (const char *format_string = format_to_string(_backbuffer_format); format_string != nullptr)
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << format_string << '!';
		else
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << _backbuffer_format << '!';
		return false;
	}

	// Create a texture in system memory, copy back buffer data into it and map it for reading
	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = _width;
	desc.Height = _height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = _backbuffer_format;
	desc.SampleDesc = { 1, 0 };
	desc.Usage = D3D10_USAGE_STAGING;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

	com_ptr<ID3D10Texture2D> intermediate;
	if (HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &intermediate); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width << ", Height = " << desc.Height << ", Format = " << desc.Format;
		return false;
	}

	_device->CopyResource(intermediate.get(), _backbuffer_resolved.get());

	D3D10_MAPPED_TEXTURE2D mapped;
	if (FAILED(intermediate->Map(0, D3D10_MAP_READ, 0, &mapped)))
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

	intermediate->Unmap(0);

	return true;
}

bool reshade::d3d10::runtime_impl::init_effect(size_t index)
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

	effect &effect = _effects[index];

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	const std::string hlsl = effect.preamble + effect.module.hlsl;
	std::unordered_map<std::string, com_ptr<IUnknown>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
	{
		HRESULT hr = E_FAIL;

		std::string profile;
		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			profile = "vs";
			break;
		case reshadefx::shader_type::ps:
			profile = "ps";
			break;
		case reshadefx::shader_type::cs:
			effect.errors += "Compute shaders are not supported in D3D10.";
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
		attributes += "entrypoint=" + entry_point.name + ';';
		attributes += "profile=" + profile + ';';
		attributes += "flags=" + std::to_string(compile_flags) + ';';

		const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
		std::vector<char> cso;
		if (!load_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]))
		{
			com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
			hr = D3DCompile(
				hlsl.data(), hlsl.size(),
				nullptr, nullptr, nullptr,
				entry_point.name.c_str(),
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
				effect.assembly[entry_point.name].assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

			save_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]);
		}

		// Create runtime shader objects from the compiled DX byte code
		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			hr = _device->CreateVertexShader(cso.data(), cso.size(), reinterpret_cast<ID3D10VertexShader **>(&entry_points[entry_point.name]));
			break;
		case reshadefx::shader_type::ps:
			hr = _device->CreatePixelShader(cso.data(), cso.size(), reinterpret_cast<ID3D10PixelShader **>(&entry_points[entry_point.name]));
			break;
		}

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader for entry point '" << entry_point.name << "'! HRESULT is " << hr << '.';
			return false;
		}
	}

	if (index >= _effect_data.size())
		_effect_data.resize(index + 1);
	effect_data &effect_data = _effect_data[index];

	if (!effect.uniform_data_storage.empty())
	{
		const D3D10_BUFFER_DESC desc = { static_cast<UINT>(effect.uniform_data_storage.size()), D3D10_USAGE_DYNAMIC, D3D10_BIND_CONSTANT_BUFFER, D3D10_CPU_ACCESS_WRITE };
		const D3D10_SUBRESOURCE_DATA initial_data = { effect.uniform_data_storage.data(), desc.ByteWidth };

		if (HRESULT hr = _device->CreateBuffer(&desc, &initial_data, &effect_data.cb); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create constant buffer for effect file '" << effect.source_file << "'! HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << desc.ByteWidth;
			return false;
		}
	}

	technique_data technique_init;
	technique_init.sampler_states.resize(effect.module.num_sampler_bindings);
	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding >= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind sampler '" << info.unique_name << "' since it exceeds the maximum number of allowed sampler slots in " << "D3D10" << " (" << info.binding << ", allowed are up to " << D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT << ").";
			return false;
		}

		if (technique_init.sampler_states[info.binding] == nullptr)
		{
			D3D10_SAMPLER_DESC desc;
			desc.Filter = static_cast<D3D10_FILTER>(info.filter);
			desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_u);
			desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_v);
			desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_w);
			desc.MipLODBias = info.lod_bias;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D10_COMPARISON_NEVER;
			std::memset(desc.BorderColor, 0, sizeof(desc.BorderColor));
			desc.MinLOD = info.min_lod;
			desc.MaxLOD = info.max_lod;

			// Generate hash for sampler description
			size_t desc_hash = 2166136261;
			for (size_t i = 0; i < sizeof(desc); ++i)
				desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

			std::unordered_map<size_t, com_ptr<ID3D10SamplerState>>::iterator it = _effect_sampler_states.find(desc_hash);
			if (it == _effect_sampler_states.end())
			{
				com_ptr<ID3D10SamplerState> sampler;
				if (HRESULT hr = _device->CreateSamplerState(&desc, &sampler); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create sampler state for sampler '" << info.unique_name << "'! HRESULT is " << hr << '.';
					LOG(DEBUG) << "> Details: Filter = " << desc.Filter << ", AddressU = " << desc.AddressU << ", AddressV = " << desc.AddressV << ", AddressW = " << desc.AddressW << ", MipLODBias = " << desc.MipLODBias << ", MinLOD = " << desc.MinLOD << ", MaxLOD = " << desc.MaxLOD;
					return false;
				}

				it = _effect_sampler_states.emplace(desc_hash, std::move(sampler)).first;
			}

			technique_init.sampler_states[info.binding] = it->second;
		}
	}

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		auto impl = new technique_data(technique_init);
		technique.impl = impl;

		D3D10_QUERY_DESC query_desc = {};
		query_desc.Query = D3D10_QUERY_TIMESTAMP;
		_device->CreateQuery(&query_desc, &impl->timestamp_query_beg);
		_device->CreateQuery(&query_desc, &impl->timestamp_query_end);
		query_desc.Query = D3D10_QUERY_TIMESTAMP_DISJOINT;
		_device->CreateQuery(&query_desc, &impl->timestamp_disjoint);

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			pass_data &pass_data = impl->passes[pass_index];
			reshadefx::pass_info &pass_info = technique.passes[pass_index];

			entry_points.at(pass_info.ps_entry_point)->QueryInterface(&pass_data.pixel_shader);
			entry_points.at(pass_info.vs_entry_point)->QueryInterface(&pass_data.vertex_shader);

			const int srgb_index = pass_info.srgb_write_enable ? 1 : 0;

			for (UINT k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
			{
				tex_data *const tex_impl = static_cast<tex_data *>(
					look_up_texture_by_name(pass_info.render_target_names[k]).impl);

				D3D10_TEXTURE2D_DESC desc;
				tex_impl->texture->GetDesc(&desc);

				D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = pass_info.srgb_write_enable ?
					make_dxgi_format_srgb(desc.Format) :
					make_dxgi_format_normal(desc.Format);
				rtv_desc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

				// Create render target view for texture on demand when it is first used
				if (tex_impl->rtv[srgb_index] == nullptr)
				{
					if (HRESULT hr = _device->CreateRenderTargetView(tex_impl->texture.get(), &rtv_desc, &tex_impl->rtv[srgb_index]); FAILED(hr))
					{
						LOG(ERROR) << "Failed to create render target view for texture '" << pass_info.render_target_names[k] << "'! HRESULT is " << hr << '.';
						LOG(DEBUG) << "> Details: Format = " << rtv_desc.Format << ", ViewDimension = " << rtv_desc.ViewDimension;
						return false;
					}
				}

				pass_data.render_targets[k] = tex_impl->rtv[srgb_index];
				pass_data.modified_resources.push_back(tex_impl->srv[srgb_index]);
			}

			if (pass_info.render_target_names[0].empty())
			{
				pass_info.viewport_width = _width;
				pass_info.viewport_height = _height;
				pass_data.render_targets[0] = _backbuffer_rtv[srgb_index];
				pass_data.modified_resources.push_back(_backbuffer_texture_srv[srgb_index]);
			}

			{   D3D10_BLEND_DESC desc;
				desc.AlphaToCoverageEnable = FALSE;
				desc.BlendEnable[0] = pass_info.blend_enable;

				const auto convert_blend_op = [](reshadefx::pass_blend_op value) {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return D3D10_BLEND_OP_ADD;
					case reshadefx::pass_blend_op::subtract: return D3D10_BLEND_OP_SUBTRACT;
					case reshadefx::pass_blend_op::rev_subtract: return D3D10_BLEND_OP_REV_SUBTRACT;
					case reshadefx::pass_blend_op::min: return D3D10_BLEND_OP_MIN;
					case reshadefx::pass_blend_op::max: return D3D10_BLEND_OP_MAX;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) {
					switch (value) {
					case reshadefx::pass_blend_func::zero: return D3D10_BLEND_ZERO;
					default:
					case reshadefx::pass_blend_func::one: return D3D10_BLEND_ONE;
					case reshadefx::pass_blend_func::src_color: return D3D10_BLEND_SRC_COLOR;
					case reshadefx::pass_blend_func::src_alpha: return D3D10_BLEND_SRC_ALPHA;
					case reshadefx::pass_blend_func::inv_src_color: return D3D10_BLEND_INV_SRC_COLOR;
					case reshadefx::pass_blend_func::inv_src_alpha: return D3D10_BLEND_INV_SRC_ALPHA;
					case reshadefx::pass_blend_func::dst_color: return D3D10_BLEND_DEST_COLOR;
					case reshadefx::pass_blend_func::dst_alpha: return D3D10_BLEND_DEST_ALPHA;
					case reshadefx::pass_blend_func::inv_dst_color: return D3D10_BLEND_INV_DEST_COLOR;
					case reshadefx::pass_blend_func::inv_dst_alpha: return D3D10_BLEND_INV_DEST_ALPHA;
					}
				};

				desc.SrcBlend = convert_blend_func(pass_info.src_blend);
				desc.DestBlend = convert_blend_func(pass_info.dest_blend);
				desc.BlendOp = convert_blend_op(pass_info.blend_op);
				desc.SrcBlendAlpha = convert_blend_func(pass_info.src_blend_alpha);
				desc.DestBlendAlpha = convert_blend_func(pass_info.dest_blend_alpha);
				desc.BlendOpAlpha = convert_blend_op(pass_info.blend_op_alpha);
				desc.RenderTargetWriteMask[0] = pass_info.color_write_mask;

				for (UINT i = 1; i < 8; ++i)
				{
					desc.BlendEnable[i] = desc.BlendEnable[0];
					desc.RenderTargetWriteMask[i] = desc.RenderTargetWriteMask[0];
				}

				if (HRESULT hr = _device->CreateBlendState(&desc, &pass_data.blend_state); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create blend state for pass " << pass_index << " in technique '" << technique.name << "'! HRESULT is " << hr << '.';
					return false;
				}
			}

			// Rasterizer state is the same for all passes
			assert(_effect_rasterizer != nullptr);

			{   D3D10_DEPTH_STENCIL_DESC desc;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D10_COMPARISON_ALWAYS;

				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
					switch (value) {
					case reshadefx::pass_stencil_op::zero: return D3D10_STENCIL_OP_ZERO;
					default:
					case reshadefx::pass_stencil_op::keep: return D3D10_STENCIL_OP_KEEP;
					case reshadefx::pass_stencil_op::invert: return D3D10_STENCIL_OP_INVERT;
					case reshadefx::pass_stencil_op::replace: return D3D10_STENCIL_OP_REPLACE;
					case reshadefx::pass_stencil_op::incr: return D3D10_STENCIL_OP_INCR;
					case reshadefx::pass_stencil_op::incr_sat: return D3D10_STENCIL_OP_INCR_SAT;
					case reshadefx::pass_stencil_op::decr: return D3D10_STENCIL_OP_DECR;
					case reshadefx::pass_stencil_op::decr_sat: return D3D10_STENCIL_OP_DECR_SAT;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) {
					switch (value)
					{
					case reshadefx::pass_stencil_func::never: return D3D10_COMPARISON_NEVER;
					case reshadefx::pass_stencil_func::equal: return D3D10_COMPARISON_EQUAL;
					case reshadefx::pass_stencil_func::not_equal: return D3D10_COMPARISON_NOT_EQUAL;
					case reshadefx::pass_stencil_func::less: return D3D10_COMPARISON_LESS;
					case reshadefx::pass_stencil_func::less_equal: return D3D10_COMPARISON_LESS_EQUAL;
					case reshadefx::pass_stencil_func::greater: return D3D10_COMPARISON_GREATER;
					case reshadefx::pass_stencil_func::greater_equal: return D3D10_COMPARISON_GREATER_EQUAL;
					default:
					case reshadefx::pass_stencil_func::always: return D3D10_COMPARISON_ALWAYS;
					}
				};

				desc.StencilEnable = pass_info.stencil_enable;
				desc.StencilReadMask = pass_info.stencil_read_mask;
				desc.StencilWriteMask = pass_info.stencil_write_mask;
				desc.FrontFace.StencilFailOp = convert_stencil_op(pass_info.stencil_op_fail);
				desc.FrontFace.StencilDepthFailOp = convert_stencil_op(pass_info.stencil_op_depth_fail);
				desc.FrontFace.StencilPassOp = convert_stencil_op(pass_info.stencil_op_pass);
				desc.FrontFace.StencilFunc = convert_stencil_func(pass_info.stencil_comparison_func);
				desc.BackFace = desc.FrontFace;

				if (HRESULT hr = _device->CreateDepthStencilState(&desc, &pass_data.depth_stencil_state); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create depth-stencil state for pass " << pass_index << " in technique '" << technique.name << "'! HRESULT is " << hr << '.';
					return false;
				}
			}

			pass_data.srvs.resize(effect.module.num_texture_bindings);
			for (const reshadefx::sampler_info &info : pass_info.samplers)
			{
				if (info.texture_binding >= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
				{
					LOG(ERROR) << "Cannot bind texture '" << info.texture_name << "' since it exceeds the maximum number of allowed resource slots in " << "D3D10" << " (" << info.texture_binding << ", allowed are up to " << D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT << ").";
					return false;
				}

				tex_data *const tex_impl = static_cast<tex_data *>(
					look_up_texture_by_name(info.texture_name).impl);

				pass_data.srvs[info.texture_binding] = tex_impl->srv[info.srgb ? 1 : 0];
			}
		}
	}

	return true;
}
void reshade::d3d10::runtime_impl::unload_effect(size_t index)
{
	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);

	if (index < _effect_data.size())
		_effect_data[index].cb.reset();
}
void reshade::d3d10::runtime_impl::unload_effects()
{
	for (technique &tech : _techniques)
	{
		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effects();

	_effect_data.clear();
	_effect_sampler_states.clear();
}

bool reshade::d3d10::runtime_impl::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	if (texture.semantic == "COLOR")
	{
		impl->srv[0] = _backbuffer_texture_srv[0];
		impl->srv[1] = _backbuffer_texture_srv[1];
		return true;
	}
	else if (!texture.semantic.empty())
	{
		if (const auto it = _texture_semantic_bindings.find(texture.semantic);
			it != _texture_semantic_bindings.end())
			impl->srv[0] = impl->srv[1] = it->second;
		return true;
	}

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = texture.width;
	desc.Height = texture.height;
	desc.MipLevels = texture.levels;
	desc.ArraySize = 1;
	desc.SampleDesc = { 1, 0 };
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

	if (texture.levels > 1)
		desc.MiscFlags |= D3D10_RESOURCE_MISC_GENERATE_MIPS; // Requires D3D10_BIND_RENDER_TARGET as well
	if (texture.render_target || texture.levels > 1)
		desc.BindFlags |= D3D10_BIND_RENDER_TARGET;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		desc.Format = DXGI_FORMAT_R8_UNORM;
		break;
	case reshadefx::texture_format::r16f:
		desc.Format = DXGI_FORMAT_R16_FLOAT;
		break;
	case reshadefx::texture_format::r32f:
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		break;
	case reshadefx::texture_format::rg8:
		desc.Format = DXGI_FORMAT_R8G8_UNORM;
		break;
	case reshadefx::texture_format::rg16:
		desc.Format = DXGI_FORMAT_R16G16_UNORM;
		break;
	case reshadefx::texture_format::rg16f:
		desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		break;
	case reshadefx::texture_format::rg32f:
		desc.Format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case reshadefx::texture_format::rgba8:
		desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case reshadefx::texture_format::rgba16:
		desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		break;
	case reshadefx::texture_format::rgba16f:
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case reshadefx::texture_format::rgba32f:
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case reshadefx::texture_format::rgb10a2:
		desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		break;
	}

	// Clear texture to zero since by default its contents are undefined
	std::vector<uint8_t> zero_data(desc.Width * desc.Height * 16);
	std::vector<D3D10_SUBRESOURCE_DATA> initial_data(desc.MipLevels);
	for (UINT level = 0, width = desc.Width; level < desc.MipLevels; ++level, width /= 2)
	{
		initial_data[level].pSysMem = zero_data.data();
		initial_data[level].SysMemPitch = width * 16;
	}

	if (HRESULT hr = _device->CreateTexture2D(&desc, initial_data.data(), &impl->texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width << ", Height = " << desc.Height << ", Levels = " << desc.MipLevels << ", Format = " << desc.Format << ", BindFlags = " << std::hex << desc.BindFlags << std::dec;
		return false;
	}

	D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = make_dxgi_format_normal(desc.Format);
	srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = desc.MipLevels;

	if (HRESULT hr = _device->CreateShaderResourceView(impl->texture.get(), &srv_desc, &impl->srv[0]); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create shader resource view for texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Format = " << srv_desc.Format << ", ViewDimension = " << srv_desc.ViewDimension << ", Levels = " << srv_desc.Texture2D.MipLevels;
		return false;
	}

	srv_desc.Format = make_dxgi_format_srgb(desc.Format);

	if (srv_desc.Format != desc.Format)
	{
		if (HRESULT hr = _device->CreateShaderResourceView(impl->texture.get(), &srv_desc, &impl->srv[1]); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Format = " << srv_desc.Format << ", ViewDimension = " << srv_desc.ViewDimension << ", Levels = " << srv_desc.Texture2D.MipLevels;
			return false;
		}
	}
	else
	{
		impl->srv[1] = impl->srv[0];
	}

	return true;
}
void reshade::d3d10::runtime_impl::upload_texture(const texture &texture, const uint8_t *pixels)
{
	auto impl = static_cast<tex_data *>(texture.impl);
	assert(impl != nullptr && texture.semantic.empty() && pixels != nullptr);

	unsigned int upload_pitch;
	std::vector<uint8_t> upload_data;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		upload_pitch = texture.width;
		upload_data.resize(upload_pitch * texture.height);
		for (uint32_t i = 0, k = 0; i < texture.width * texture.height * 4; i += 4, k += 1)
			upload_data[k] = pixels[i];
		pixels = upload_data.data();
		break;
	case reshadefx::texture_format::rg8:
		upload_pitch = texture.width * 2;
		upload_data.resize(upload_pitch * texture.height);
		for (uint32_t i = 0, k = 0; i < texture.width * texture.height * 4; i += 4, k += 2)
			upload_data[k + 0] = pixels[i + 0],
			upload_data[k + 1] = pixels[i + 1];
		pixels = upload_data.data();
		break;
	case reshadefx::texture_format::rgba8:
		upload_pitch = texture.width * 4;
		break;
	default:
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'!";
		return;
	}

	_device->UpdateSubresource(impl->texture.get(), 0, nullptr, pixels, upload_pitch, upload_pitch * texture.height);

	if (texture.levels > 1)
		_device->GenerateMips(impl->srv[0].get());
}
void reshade::d3d10::runtime_impl::destroy_texture(texture &texture)
{
	delete static_cast<tex_data *>(texture.impl);
	texture.impl = nullptr;
}

void reshade::d3d10::runtime_impl::render_technique(technique &technique)
{
	const auto impl = static_cast<technique_data *>(technique.impl);
	effect_data &effect_data = _effect_data[technique.effect_index];

	// Evaluate queries
	if (impl->query_in_flight)
	{
		UINT64 timestamp0, timestamp1;
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;

		if (impl->timestamp_disjoint->GetData(&disjoint,    sizeof(disjoint),   D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
			impl->timestamp_query_beg->GetData(&timestamp0, sizeof(timestamp0), D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
			impl->timestamp_query_end->GetData(&timestamp1, sizeof(timestamp1), D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
		{
			if (!disjoint.Disjoint)
				technique.average_gpu_duration.append((timestamp1 - timestamp0) * 1'000'000'000 / disjoint.Frequency);
			impl->query_in_flight = false;
		}
	}

	if (!impl->query_in_flight)
	{
		impl->timestamp_disjoint->Begin();
		impl->timestamp_query_beg->End();
	}

	RESHADE_ADDON_EVENT(reshade_before_effects, this, _device_impl);

	// Setup vertex input (no explicit vertices are provided, so bind to null)
	const uintptr_t null = 0;
	_device->IASetInputLayout(nullptr);
	_device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

	_device->RSSetState(_effect_rasterizer.get());

	// Setup samplers
	_device->VSSetSamplers(0, static_cast<UINT>(impl->sampler_states.size()), reinterpret_cast<ID3D10SamplerState *const *>(impl->sampler_states.data()));
	_device->PSSetSamplers(0, static_cast<UINT>(impl->sampler_states.size()), reinterpret_cast<ID3D10SamplerState *const *>(impl->sampler_states.data()));

	// Setup shader constants
	if (ID3D10Buffer *const cb = effect_data.cb.get(); cb != nullptr)
	{
		if (void *mapped;
			SUCCEEDED(cb->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			std::memcpy(mapped, _effects[technique.effect_index].uniform_data_storage.data(), _effects[technique.effect_index].uniform_data_storage.size());
			cb->Unmap();
		}

		_device->VSSetConstantBuffers(0, 1, &cb);
		_device->PSSetConstantBuffers(0, 1, &cb);
	}

	// Disable unused pipeline stages
	_device->GSSetShader(nullptr);

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			// Save back buffer of previous pass
			_device->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());
		}

		const pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

		_device->VSSetShader(pass_data.vertex_shader.get());
		_device->PSSetShader(pass_data.pixel_shader.get());

		_device->OMSetBlendState(pass_data.blend_state.get(), nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		_device->OMSetDepthStencilState(pass_data.depth_stencil_state.get(), pass_info.stencil_reference_value);

		// Setup render targets
		if (pass_info.viewport_width == _width && pass_info.viewport_height == _height)
		{
			_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D10RenderTargetView *const *>(pass_data.render_targets), pass_info.stencil_enable ? _effect_stencil.get() : nullptr);

			if (pass_info.stencil_enable && !is_effect_stencil_cleared)
			{
				is_effect_stencil_cleared = true;

				_device->ClearDepthStencilView(_effect_stencil.get(), D3D10_CLEAR_STENCIL, 1.0f, 0);
			}
		}
		else
		{
			_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D10RenderTargetView *const *>(pass_data.render_targets), nullptr);
		}

		// Setup shader resources after binding render targets, to ensure any OM bindings by the application are unset at this point
		// Otherwise a slot referencing a resource still bound to the OM would be filled with NULL, which can happen with the depth buffer (https://docs.microsoft.com/windows/win32/api/d3d10/nf-d3d10-id3d10device-pssetshaderresources)
		_device->VSSetShaderResources(0, static_cast<UINT>(pass_data.srvs.size()), reinterpret_cast<ID3D10ShaderResourceView *const *>(pass_data.srvs.data()));
		_device->PSSetShaderResources(0, static_cast<UINT>(pass_data.srvs.size()), reinterpret_cast<ID3D10ShaderResourceView *const *>(pass_data.srvs.data()));

		if (pass_info.clear_render_targets)
		{
			for (const com_ptr<ID3D10RenderTargetView> &target : pass_data.render_targets)
			{
				if (target == nullptr)
					break; // Render targets can only be set consecutively
				const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				_device->ClearRenderTargetView(target.get(), color);
			}
		}

		const D3D10_VIEWPORT viewport = { 0, 0, pass_info.viewport_width, pass_info.viewport_height, 0.0f, 1.0f };
		_device->RSSetViewports(1, &viewport);

		// Draw primitives
		D3D10_PRIMITIVE_TOPOLOGY topology;
		switch (pass_info.topology)
		{
		case reshadefx::primitive_topology::point_list:
			topology = D3D10_PRIMITIVE_TOPOLOGY_POINTLIST;
			break;
		case reshadefx::primitive_topology::line_list:
			topology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case reshadefx::primitive_topology::line_strip:
			topology = D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		default:
		case reshadefx::primitive_topology::triangle_list:
			topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case reshadefx::primitive_topology::triangle_strip:
			topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		}
		_device->IASetPrimitiveTopology(topology);
		_device->Draw(pass_info.num_vertices, 0);

		// Reset render targets
		_device->OMSetRenderTargets(0, nullptr, nullptr);

		// Reset shader resources
		ID3D10ShaderResourceView *null_srv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
		_device->VSSetShaderResources(0, static_cast<UINT>(pass_data.srvs.size()), null_srv);
		_device->PSSetShaderResources(0, static_cast<UINT>(pass_data.srvs.size()), null_srv);

		needs_implicit_backbuffer_copy = false;

		// Generate mipmaps for modified resources
		for (const com_ptr<ID3D10ShaderResourceView> &resource : pass_data.modified_resources)
		{
			if (resource == _backbuffer_texture_srv[0] ||
				resource == _backbuffer_texture_srv[1])
			{
				needs_implicit_backbuffer_copy = true;
				break;
			}

			D3D10_SHADER_RESOURCE_VIEW_DESC resource_desc;
			resource->GetDesc(&resource_desc);

			if (resource_desc.Texture2D.MipLevels > 1)
				_device->GenerateMips(resource.get());
		}
	}

	RESHADE_ADDON_EVENT(reshade_after_effects, this, _device_impl);

	if (!impl->query_in_flight)
	{
		impl->timestamp_query_end->End();
		impl->timestamp_disjoint->End();
	}

	impl->query_in_flight = true;
}

void reshade::d3d10::runtime_impl::update_texture_bindings(const char *semantic, api::resource_view_handle api_srv)
{
	const auto new_srv = reinterpret_cast<ID3D10ShaderResourceView *>(api_srv.handle);

	_texture_semantic_bindings[semantic] = new_srv;

	// Update all references to the new texture
	for (const texture &tex : _textures)
	{
		if (tex.impl == nullptr || tex.semantic != semantic)
			continue;
		const auto tex_impl = static_cast<tex_data *>(tex.impl);
		if (tex_impl->srv[0] == new_srv)
			continue;

		// Update references in technique list
		for (const technique &tech : _techniques)
		{
			const auto tech_impl = static_cast<technique_data *>(tech.impl);
			if (tech_impl == nullptr)
				continue;

			for (pass_data &pass_data : tech_impl->passes)
				// Replace all occurances of the old resource view with the new one
				for (com_ptr<ID3D10ShaderResourceView> &srv : pass_data.srvs)
					if (tex_impl->srv[0] == srv || tex_impl->srv[1] == srv)
						srv = new_srv;
		}

		tex_impl->srv[0] = tex_impl->srv[1] = new_srv;
	}
}
