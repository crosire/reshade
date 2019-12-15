/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_d3d10.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "../dxgi/format_utils.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <d3dcompiler.h>

namespace reshade::d3d10
{
	struct d3d10_tex_data : base_object
	{
		com_ptr<ID3D10Texture2D> texture;
		com_ptr<ID3D10RenderTargetView> rtv[2];
		com_ptr<ID3D10ShaderResourceView> srv[2];
	};
	struct d3d10_pass_data : base_object
	{
		com_ptr<ID3D10BlendState> blend_state;
		com_ptr<ID3D10DepthStencilState> depth_stencil_state;
		com_ptr<ID3D10PixelShader> pixel_shader;
		com_ptr<ID3D10VertexShader> vertex_shader;
		com_ptr<ID3D10RenderTargetView> render_targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D10ShaderResourceView> render_target_resources[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		D3D10_VIEWPORT viewport;
		std::vector<com_ptr<ID3D10ShaderResourceView>> shader_resources;
	};
	struct d3d10_technique_data : base_object
	{
		bool query_in_flight = false;
		com_ptr<ID3D10Query> timestamp_disjoint;
		com_ptr<ID3D10Query> timestamp_query_beg;
		com_ptr<ID3D10Query> timestamp_query_end;
		std::vector<com_ptr<ID3D10SamplerState>> sampler_states;
		std::vector<com_ptr<ID3D10ShaderResourceView>> texture_bindings;
	};
}

reshade::d3d10::runtime_d3d10::runtime_d3d10(ID3D10Device1 *device, IDXGISwapChain *swapchain) :
	_device(device), _swapchain(swapchain),
	_app_state(device)
{
	assert(device != nullptr);
	assert(swapchain != nullptr);

	com_ptr<IDXGIDevice> dxgi_device;
	_device->QueryInterface(&dxgi_device);
	com_ptr<IDXGIAdapter> dxgi_adapter;
	dxgi_device->GetAdapter(&dxgi_adapter);

	_renderer_id = device->GetFeatureLevel();
	if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
		_vendor_id = desc.VendorId, _device_id = desc.DeviceId;

#if RESHADE_GUI && RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	subscribe_to_ui("DX10", [this]() { draw_depth_debug_menu(); });
#endif
#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	subscribe_to_load_config([this](const ini_file &config) {
		config.get("DX10_BUFFER_DETECTION", "DepthBufferRetrievalMode", _preserve_depth_buffers);
		config.get("DX10_BUFFER_DETECTION", "DepthBufferClearingNumber", _depth_clear_index_override);
		config.get("DX10_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);

		if (_depth_clear_index_override == 0)
			// Zero is not a valid clear index, since it disables depth buffer preservation
			_depth_clear_index_override = std::numeric_limits<UINT>::max();
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("DX10_BUFFER_DETECTION", "DepthBufferRetrievalMode", _preserve_depth_buffers);
		config.set("DX10_BUFFER_DETECTION", "DepthBufferClearingNumber", _depth_clear_index_override);
		config.set("DX10_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);
	});
#endif
}
reshade::d3d10::runtime_d3d10::~runtime_d3d10()
{
	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d10::runtime_d3d10::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc)
{
	RECT window_rect = {};
	GetClientRect(swap_desc.OutputWindow, &window_rect);

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_color_bit_depth = dxgi_format_color_depth(swap_desc.BufferDesc.Format);
	_backbuffer_format = swap_desc.BufferDesc.Format;

	// Get back buffer texture
	HRESULT hr = _swapchain->GetBuffer(0, IID_PPV_ARGS(&_backbuffer));
	assert(SUCCEEDED(hr));

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
	OSVERSIONINFOEX verinfo_windows7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
	const bool is_windows7 = VerifyVersionInfo(&verinfo_windows7, VER_MAJORVERSION | VER_MINORVERSION,
		VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;

	if (swap_desc.SampleDesc.Count > 1 ||
		make_dxgi_format_normal(_backbuffer_format) != _backbuffer_format ||
		!is_windows7)
	{
		if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_resolved)))
			return false;

		hr = _device->CreateRenderTargetView(_backbuffer.get(), nullptr, &_backbuffer_rtv[2]);
		assert(SUCCEEDED(hr));
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

	// Create back buffer shader texture
	tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	if (SUCCEEDED(hr = _device->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_texture)))
	{
		D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = make_dxgi_format_normal(tex_desc.Format);
		srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
		hr = _device->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv[0]);
		srv_desc.Format = make_dxgi_format_srgb(tex_desc.Format);
		hr = _device->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv[1]);
	}

	if (FAILED(hr))
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
	const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
	if (FAILED(_device->CreateVertexShader(vs.data, vs.data_size, &_copy_vertex_shader)))
		return false;
	const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);
	if (FAILED(_device->CreatePixelShader(ps.data, ps.data_size, &_copy_pixel_shader)))
		return false;

	{   D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
		if (FAILED(_device->CreateSamplerState(&desc, &_copy_sampler_state)))
			return false;
	}

	// Create effect states
	{   D3D10_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D10_FILL_SOLID;
		desc.CullMode = D3D10_CULL_NONE;
		desc.DepthClipEnable = TRUE;
		if (FAILED(_device->CreateRasterizerState(&desc, &_effect_rasterizer_state)))
			return false;
	}

	// Create effect depth stencil texture
	tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	tex_desc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	com_ptr<ID3D10Texture2D> effect_depthstencil_texture;
	if (FAILED(_device->CreateTexture2D(&tex_desc, nullptr, &effect_depthstencil_texture)))
		return false;
	if (FAILED(_device->CreateDepthStencilView(effect_depthstencil_texture.get(), nullptr, &_effect_depthstencil)))
		return false;

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d10::runtime_d3d10::on_reset()
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
	_depth_texture.reset();
	_depth_texture_srv.reset();

	_copy_vertex_shader.reset();
	_copy_pixel_shader.reset();
	_copy_sampler_state.reset();

	_effect_depthstencil.reset();
	_effect_rasterizer_state.reset();

#if RESHADE_GUI
	_imgui_index_buffer_size = 0;
	_imgui_index_buffer.reset();
	_imgui_vertex_buffer_size = 0;
	_imgui_vertex_buffer.reset();
	_imgui_vertex_shader.reset();
	_imgui_pixel_shader.reset();
	_imgui_input_layout.reset();
	_imgui_constant_buffer.reset();
	_imgui_texture_sampler.reset();
	_imgui_rasterizer_state.reset();
	_imgui_blend_state.reset();
	_imgui_depthstencil_state.reset();
#endif

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	_has_depth_texture = false;
	_depth_texture_override = nullptr;
#endif
}

void reshade::d3d10::runtime_d3d10::on_present(buffer_detection &tracker)
{
	if (!_is_initialized)
		return;

	_vertices = tracker.total_vertices();
	_drawcalls = tracker.total_drawcalls();

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	_current_tracker = &tracker;
	assert(_depth_clear_index_override != 0);
	update_depthstencil_texture(_has_high_network_activity ? nullptr :
		tracker.find_best_depth_texture(_filter_aspect_ratio ? _width : 0, _height, _depth_texture_override, _preserve_depth_buffers ? _depth_clear_index_override : 0));
#endif

	_app_state.capture();

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		_device->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, _backbuffer_format);

	// Setup real back buffer
	auto rtv = _backbuffer_rtv[0].get();
	 _device->OMSetRenderTargets(1, &rtv, nullptr);

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
		_device->RSSetState(_effect_rasterizer_state.get());
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

bool reshade::d3d10::runtime_d3d10::capture_screenshot(uint8_t *buffer) const
{
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
	if (FAILED(_device->CreateTexture2D(&desc, nullptr, &intermediate)))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture!";
		return false;
	}

	_device->CopyResource(intermediate.get(), _backbuffer_resolved.get());

	D3D10_MAPPED_TEXTURE2D mapped;
	if (FAILED(intermediate->Map(0, D3D10_MAP_READ, 0, &mapped)))
		return false;
	auto mapped_data = static_cast<const uint8_t *>(mapped.pData);

	for (uint32_t y = 0, pitch = _width * 4; y < _height; y++, buffer += pitch, mapped_data += mapped.RowPitch)
	{
		if (_backbuffer_format == DXGI_FORMAT_R10G10B10A2_UNORM ||
			_backbuffer_format == DXGI_FORMAT_R10G10B10A2_UINT)
		{
			for (uint32_t x = 0; x < pitch; x += 4)
			{
				const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_data + x);
				// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
				buffer[x + 0] = ((rgba & 0x3FF) / 4) & 0xFF;
				buffer[x + 1] = (((rgba & 0xFFC00) >> 10) / 4) & 0xFF;
				buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) / 4) & 0xFF;
				buffer[x + 3] = 0xFF;
			}
		}
		else
		{
			std::memcpy(buffer, mapped_data, pitch);

			for (uint32_t x = 0; x < pitch; x += 4)
			{
				buffer[x + 3] = 0xFF; // Clear alpha channel
				if (_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
					_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
					std::swap(buffer[x + 0], buffer[x + 2]); // Format is BGRA, but output should be RGBA, so flip channels
			}
		}
	}

	intermediate->Unmap(0);

	return true;
}

bool reshade::d3d10::runtime_d3d10::init_effect(size_t index)
{
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_47.dll");
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_43.dll");

	if (_d3d_compiler == nullptr)
	{
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\"). Make sure you have the DirectX end-user runtime (June 2010) installed or a newer version of the library in the application directory.";
		return false;
	}

	effect &effect = _effects[index];

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	const std::string hlsl = effect.preamble + effect.module.hlsl;
	std::unordered_map<std::string, com_ptr<IUnknown>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const auto &entry_point : effect.module.entry_points)
	{
		com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
		std::string profile = entry_point.is_pixel_shader ? "ps" : "vs";

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

		HRESULT hr = D3DCompile(
			hlsl.c_str(), hlsl.size(),
			nullptr, nullptr, nullptr,
			entry_point.name.c_str(),
			profile.c_str(),
			D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&d3d_compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(d3d_compiled->GetBufferPointer(), d3d_compiled->GetBufferSize(), 0, nullptr, &d3d_disassembled)))
			effect.assembly[entry_point.name] = std::string(static_cast<const char *>(d3d_disassembled->GetBufferPointer()));

		// Create runtime shader objects from the compiled DX byte code
		if (entry_point.is_pixel_shader)
			hr = _device->CreatePixelShader(d3d_compiled->GetBufferPointer(), d3d_compiled->GetBufferSize(), reinterpret_cast<ID3D10PixelShader **>(&entry_points[entry_point.name]));
		else
			hr = _device->CreateVertexShader(d3d_compiled->GetBufferPointer(), d3d_compiled->GetBufferSize(), reinterpret_cast<ID3D10VertexShader **>(&entry_points[entry_point.name]));

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader for entry point '" << entry_point.name << "'. "
				"HRESULT is " << hr << '.';
			return false;
		}
	}

	if (index >= _effect_constant_buffers.size())
		_effect_constant_buffers.resize(index + 1);

	if (!effect.uniform_data_storage.empty())
	{
		const D3D10_BUFFER_DESC desc = { static_cast<UINT>(effect.uniform_data_storage.size()), D3D10_USAGE_DYNAMIC, D3D10_BIND_CONSTANT_BUFFER, D3D10_CPU_ACCESS_WRITE };
		const D3D10_SUBRESOURCE_DATA init_data = { effect.uniform_data_storage.data(), desc.ByteWidth };

		if (HRESULT hr = _device->CreateBuffer(&desc, &init_data, &_effect_constant_buffers[index]); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create constant buffer for effect file " << effect.source_file << ". "
				"HRESULT is " << hr << '.';
			return false;
		}
	}

	d3d10_technique_data technique_init;

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding >= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind sampler '" << info.unique_name << "' since it exceeds the maximum number of allowed sampler slots in D3D10 (" << info.binding << ", allowed are up to " << D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT << ").";
			return false;
		}
		if (info.texture_binding >= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind texture '" << info.texture_name << "' since it exceeds the maximum number of allowed resource slots in D3D10 (" << info.texture_binding << ", allowed are up to " << D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT << ").";
			return false;
		}

		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		if (existing_texture == _textures.end())
			return false;

		D3D10_SAMPLER_DESC desc = {};
		desc.Filter = static_cast<D3D10_FILTER>(info.filter);
		desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_u);
		desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_v);
		desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(info.address_w);
		desc.MipLODBias = info.lod_bias;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D10_COMPARISON_NEVER;
		desc.MinLOD = info.min_lod;
		desc.MaxLOD = info.max_lod;

		// Generate hash for sampler description
		size_t desc_hash = 2166136261;
		for (size_t i = 0; i < sizeof(desc); ++i)
			desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

		auto it = _effect_sampler_states.find(desc_hash);
		if (it == _effect_sampler_states.end())
		{
			com_ptr<ID3D10SamplerState> sampler;

			if (HRESULT hr = _device->CreateSamplerState(&desc, &sampler); FAILED(hr))
			{
				LOG(ERROR) << "Failed to create sampler state for sampler '" << info.unique_name << "' ("
					"Filter = " << desc.Filter << ", "
					"AddressU = " << desc.AddressU << ", "
					"AddressV = " << desc.AddressV << ", "
					"AddressW = " << desc.AddressW << ", "
					"MipLODBias = " << desc.MipLODBias << ", "
					"MinLOD = " << desc.MinLOD << ", "
					"MaxLOD = " << desc.MaxLOD << ")! "
					"HRESULT is " << hr << '.';
				return false;
			}

			it = _effect_sampler_states.emplace(desc_hash, std::move(sampler)).first;
		}

		technique_init.sampler_states.resize(std::max(technique_init.sampler_states.size(), size_t(info.binding + 1)));
		technique_init.texture_bindings.resize(std::max(technique_init.texture_bindings.size(), size_t(info.texture_binding + 1)));

		technique_init.sampler_states[info.binding] = it->second;
		technique_init.texture_bindings[info.texture_binding] = existing_texture->impl->as<d3d10_tex_data>()->srv[info.srgb ? 1 : 0];
	}

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		technique.impl = std::make_unique<d3d10_technique_data>(technique_init);
		const auto technique_data = technique.impl->as<d3d10_technique_data>();

		D3D10_QUERY_DESC query_desc = {};
		query_desc.Query = D3D10_QUERY_TIMESTAMP;
		_device->CreateQuery(&query_desc, &technique_data->timestamp_query_beg);
		_device->CreateQuery(&query_desc, &technique_data->timestamp_query_end);
		query_desc.Query = D3D10_QUERY_TIMESTAMP_DISJOINT;
		_device->CreateQuery(&query_desc, &technique_data->timestamp_disjoint);

		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			technique.passes_data.push_back(std::make_unique<d3d10_pass_data>());

			auto &pass = *technique.passes_data.back()->as<d3d10_pass_data>();
			const auto &pass_info = technique.passes[pass_index];

			entry_points.at(pass_info.ps_entry_point)->QueryInterface(&pass.pixel_shader);
			entry_points.at(pass_info.vs_entry_point)->QueryInterface(&pass.vertex_shader);

			pass.viewport.MaxDepth = 1.0f;
			pass.viewport.Width = pass_info.viewport_width;
			pass.viewport.Height = pass_info.viewport_height;

			pass.shader_resources = technique_data->texture_bindings;

			const int target_index = pass_info.srgb_write_enable ? 1 : 0;
			pass.render_targets[0] = _backbuffer_rtv[target_index];
			pass.render_target_resources[0] = _backbuffer_texture_srv[target_index];

			for (unsigned int k = 0; k < 8; k++)
			{
				if (pass_info.render_target_names[k].empty())
					continue; // Skip unbound render targets

				const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				});
				if (render_target_texture == _textures.end())
					return assert(false), false;

				const auto texture_impl = render_target_texture->impl->as<d3d10_tex_data>();
				assert(texture_impl != nullptr);

				D3D10_TEXTURE2D_DESC desc;
				texture_impl->texture->GetDesc(&desc);

				D3D10_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = pass_info.srgb_write_enable ? make_dxgi_format_srgb(desc.Format) : make_dxgi_format_normal(desc.Format);
				rtv_desc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

				if (texture_impl->rtv[target_index] == nullptr)
				{
					if (HRESULT hr = _device->CreateRenderTargetView(texture_impl->texture.get(), &rtv_desc, &texture_impl->rtv[target_index]); FAILED(hr))
					{
						LOG(ERROR) << "Failed to create render target view for texture '" << render_target_texture->unique_name << "' ("
							"Format = " << rtv_desc.Format << ")! "
							"HRESULT is " << hr << '.';
						return false;
					}
				}

				pass.render_targets[k] = texture_impl->rtv[target_index];
				pass.render_target_resources[k] = texture_impl->srv[target_index];
			}

			if (pass.viewport.Width == 0)
			{
				pass.viewport.Width = frame_width();
				pass.viewport.Height = frame_height();
			}

			{   D3D10_BLEND_DESC desc = {};
				desc.BlendEnable[0] = pass_info.blend_enable;

				const auto literal_to_blend_func = [](unsigned int value) {
					switch (value) {
					case 0:
						return D3D10_BLEND_ZERO;
					default:
					case 1:
						return D3D10_BLEND_ONE;
					case 2:
						return D3D10_BLEND_SRC_COLOR;
					case 4:
						return D3D10_BLEND_INV_SRC_COLOR;
					case 3:
						return D3D10_BLEND_SRC_ALPHA;
					case 5:
						return D3D10_BLEND_INV_SRC_ALPHA;
					case 6:
						return D3D10_BLEND_DEST_ALPHA;
					case 7:
						return D3D10_BLEND_INV_DEST_ALPHA;
					case 8:
						return D3D10_BLEND_DEST_COLOR;
					case 9:
						return D3D10_BLEND_INV_DEST_COLOR;
					}
				};

				desc.SrcBlend = literal_to_blend_func(pass_info.src_blend);
				desc.DestBlend = literal_to_blend_func(pass_info.dest_blend);
				desc.BlendOp = static_cast<D3D10_BLEND_OP>(pass_info.blend_op);
				desc.SrcBlendAlpha = literal_to_blend_func(pass_info.src_blend_alpha);
				desc.DestBlendAlpha = literal_to_blend_func(pass_info.dest_blend_alpha);
				desc.BlendOpAlpha = static_cast<D3D10_BLEND_OP>(pass_info.blend_op_alpha);
				desc.RenderTargetWriteMask[0] = pass_info.color_write_mask;

				for (UINT i = 1; i < 8; ++i)
				{
					desc.BlendEnable[i] = desc.BlendEnable[0];
					desc.RenderTargetWriteMask[i] = desc.RenderTargetWriteMask[0];
				}

				if (HRESULT hr = _device->CreateBlendState(&desc, &pass.blend_state); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create blend state for pass " << pass_index << " in technique '" << technique.name << "'! "
						"HRESULT is " << hr << '.';
					return false;
				}
			}

			// Rasterizer state is the same for all passes
			assert(_effect_rasterizer_state != nullptr);

			{   D3D10_DEPTH_STENCIL_DESC desc = {};
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D10_COMPARISON_ALWAYS;

				const auto literal_to_stencil_op = [](unsigned int value) {
					switch (value) {
					default:
					case 1:
						return D3D10_STENCIL_OP_KEEP;
					case 0:
						return D3D10_STENCIL_OP_ZERO;
					case 3:
						return D3D10_STENCIL_OP_REPLACE;
					case 4:
						return D3D10_STENCIL_OP_INCR_SAT;
					case 5:
						return D3D10_STENCIL_OP_DECR_SAT;
					case 6:
						return D3D10_STENCIL_OP_INVERT;
					case 7:
						return D3D10_STENCIL_OP_INCR;
					case 8:
						return D3D10_STENCIL_OP_DECR;
					}
				};

				desc.StencilEnable = pass_info.stencil_enable;
				desc.StencilReadMask = pass_info.stencil_read_mask;
				desc.StencilWriteMask = pass_info.stencil_write_mask;
				desc.FrontFace.StencilFailOp = literal_to_stencil_op(pass_info.stencil_op_fail);
				desc.FrontFace.StencilDepthFailOp = literal_to_stencil_op(pass_info.stencil_op_depth_fail);
				desc.FrontFace.StencilPassOp = literal_to_stencil_op(pass_info.stencil_op_pass);
				desc.FrontFace.StencilFunc = static_cast<D3D10_COMPARISON_FUNC>(pass_info.stencil_comparison_func);
				desc.BackFace = desc.FrontFace;
				if (HRESULT hr = _device->CreateDepthStencilState(&desc, &pass.depth_stencil_state); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create depth stencil state for pass " << pass_index << " in technique '" << technique.name << "'! "
						"HRESULT is " << hr << '.';
					return false;
				}
			}

			for (auto &srv : pass.shader_resources)
			{
				if (srv == nullptr)
					continue;

				com_ptr<ID3D10Resource> res1;
				srv->GetResource(&res1);

				for (const auto &rtv : pass.render_targets)
				{
					if (rtv == nullptr)
						continue;

					com_ptr<ID3D10Resource> res2;
					rtv->GetResource(&res2);

					if (res1 == res2)
					{
						srv.reset();
						break;
					}
				}
			}
		}
	}

	return true;
}
void reshade::d3d10::runtime_d3d10::unload_effect(size_t index)
{
	runtime::unload_effect(index);

	if (index < _effect_constant_buffers.size())
		_effect_constant_buffers[index].reset();
}
void reshade::d3d10::runtime_d3d10::unload_effects()
{
	runtime::unload_effects();

	_effect_sampler_states.clear();
	_effect_constant_buffers.clear();
}

bool reshade::d3d10::runtime_d3d10::init_texture(texture &texture)
{
	texture.impl = std::make_unique<d3d10_tex_data>();
	const auto impl = texture.impl->as<d3d10_tex_data>();

	switch (texture.impl_reference)
	{
	case texture_reference::back_buffer:
		impl->srv[0] = _backbuffer_texture_srv[0];
		impl->srv[1] = _backbuffer_texture_srv[1];
		return true;
	case texture_reference::depth_buffer:
		impl->srv[0] = _depth_texture_srv;
		impl->srv[1] = _depth_texture_srv;
		return true;
	}

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = texture.width;
	desc.Height = texture.height;
	desc.MipLevels = texture.levels;
	desc.ArraySize = 1;
	desc.SampleDesc = { 1, 0 };
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

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

	if (HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &impl->texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "' ("
			"Width = " << desc.Width << ", "
			"Height = " << desc.Height << ", "
			"Format = " << desc.Format << ")! "
			"HRESULT is " << hr << '.';
		return false;
	}

	D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = make_dxgi_format_normal(desc.Format);
	srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = desc.MipLevels;

	if (HRESULT hr = _device->CreateShaderResourceView(impl->texture.get(), &srv_desc, &impl->srv[0]); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create shader resource view for texture '" << texture.unique_name << "' ("
			"Format = " << srv_desc.Format << ")! "
			"HRESULT is " << hr << '.';
		return false;
	}

	srv_desc.Format = make_dxgi_format_srgb(desc.Format);

	if (srv_desc.Format != desc.Format)
	{
		if (HRESULT hr = _device->CreateShaderResourceView(impl->texture.get(), &srv_desc, &impl->srv[1]); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << texture.unique_name << "' ("
				"Format = " << srv_desc.Format << ")! "
				"HRESULT is " << hr << '.';
			return false;
		}
	}
	else
	{
		impl->srv[1] = impl->srv[0];
	}

	return true;
}
void reshade::d3d10::runtime_d3d10::upload_texture(texture &texture, const uint8_t *pixels)
{
	const auto impl = texture.impl->as<d3d10_tex_data>();
	assert(impl != nullptr && texture.impl_reference == texture_reference::none && pixels != nullptr);

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
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << '!';
		return;
	}

	_device->UpdateSubresource(impl->texture.get(), 0, nullptr, pixels, upload_pitch, upload_pitch * texture.height);

	if (texture.levels > 1)
		_device->GenerateMips(impl->srv[0].get());
}

void reshade::d3d10::runtime_d3d10::render_technique(technique &technique)
{
	d3d10_technique_data &technique_data = *technique.impl->as<d3d10_technique_data>();

	// Evaluate queries
	if (technique_data.query_in_flight)
	{
		UINT64 timestamp0, timestamp1;
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;

		if (technique_data.timestamp_disjoint->GetData(&disjoint, sizeof(disjoint), D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
			technique_data.timestamp_query_beg->GetData(&timestamp0, sizeof(timestamp0), D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
			technique_data.timestamp_query_end->GetData(&timestamp1, sizeof(timestamp1), D3D10_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
		{
			if (!disjoint.Disjoint)
				technique.average_gpu_duration.append((timestamp1 - timestamp0) * 1'000'000'000 / disjoint.Frequency);
			technique_data.query_in_flight = false;
		}
	}

	if (!technique_data.query_in_flight)
	{
		technique_data.timestamp_disjoint->Begin();
		technique_data.timestamp_query_beg->End();
	}

	bool is_default_depthstencil_cleared = false;

	// Setup vertex input
	const uintptr_t null = 0;
	_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_device->IASetInputLayout(nullptr);
	_device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

	_device->RSSetState(_effect_rasterizer_state.get());

	// Setup samplers
	_device->VSSetSamplers(0, static_cast<UINT>(technique_data.sampler_states.size()), reinterpret_cast<ID3D10SamplerState *const *>(technique_data.sampler_states.data()));
	_device->PSSetSamplers(0, static_cast<UINT>(technique_data.sampler_states.size()), reinterpret_cast<ID3D10SamplerState *const *>(technique_data.sampler_states.data()));

	// Setup shader constants
	if (const auto constant_buffer = _effect_constant_buffers[technique.effect_index].get();
		constant_buffer != nullptr)
	{
		void *mapped;
		if (HRESULT hr = constant_buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped); SUCCEEDED(hr))
		{
			D3D10_BUFFER_DESC desc = {};
			constant_buffer->GetDesc(&desc);

			std::memcpy(mapped, _effects[technique.effect_index].uniform_data_storage.data(), desc.ByteWidth);
			constant_buffer->Unmap();
		}
		else
		{
			LOG(ERROR) << "Failed to map constant buffer! HRESULT is " << hr << '.';
		}

		_device->VSSetConstantBuffers(0, 1, &constant_buffer);
		_device->PSSetConstantBuffers(0, 1, &constant_buffer);
	}

	// Disable unused pipeline stages
	_device->GSSetShader(nullptr);

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<d3d10_pass_data>();

		// Setup states
		_device->VSSetShader(pass_data.vertex_shader.get());
		_device->PSSetShader(pass_data.pixel_shader.get());

		_device->OMSetBlendState(pass_data.blend_state.get(), nullptr, D3D10_DEFAULT_SAMPLE_MASK);
		_device->OMSetDepthStencilState(pass_data.depth_stencil_state.get(), pass_info.stencil_reference_value);

		// Save back buffer of previous pass
		_device->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		// Setup shader resources
		_device->VSSetShaderResources(0, static_cast<UINT>(pass_data.shader_resources.size()), reinterpret_cast<ID3D10ShaderResourceView *const *>(pass_data.shader_resources.data()));
		_device->PSSetShaderResources(0, static_cast<UINT>(pass_data.shader_resources.size()), reinterpret_cast<ID3D10ShaderResourceView *const *>(pass_data.shader_resources.data()));

		// Setup render targets
		if (pass_data.viewport.Width == _width && pass_data.viewport.Height == _height)
		{
			_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D10RenderTargetView *const *>(pass_data.render_targets), _effect_depthstencil.get());

			if (!is_default_depthstencil_cleared)
			{
				is_default_depthstencil_cleared = true;

				_device->ClearDepthStencilView(_effect_depthstencil.get(), D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0);
			}
		}
		else
		{
			_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D10RenderTargetView *const *>(pass_data.render_targets), nullptr);
		}

		_device->RSSetViewports(1, &pass_data.viewport);

		if (pass_info.clear_render_targets)
		{
			for (const auto &target : pass_data.render_targets)
			{
				if (target != nullptr)
				{
					const float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					_device->ClearRenderTargetView(target.get(), color);
				}
			}
		}

		// Draw triangle
		_device->Draw(pass_info.num_vertices, 0);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		// Reset render targets
		_device->OMSetRenderTargets(0, nullptr, nullptr);

		// Reset shader resources
		ID3D10ShaderResourceView *null_srv[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
		_device->VSSetShaderResources(0, static_cast<UINT>(pass_data.shader_resources.size()), null_srv);
		_device->PSSetShaderResources(0, static_cast<UINT>(pass_data.shader_resources.size()), null_srv);

		// Update shader resources
		for (const auto &resource : pass_data.render_target_resources)
		{
			if (resource == nullptr)
				continue;

			D3D10_SHADER_RESOURCE_VIEW_DESC resource_desc;
			resource->GetDesc(&resource_desc);

			if (resource_desc.Texture2D.MipLevels > 1)
				_device->GenerateMips(resource.get());
		}
	}

	if (!technique_data.query_in_flight)
	{
		technique_data.timestamp_query_end->End();
		technique_data.timestamp_disjoint->End();
		technique_data.query_in_flight = true;
	}
}

#if RESHADE_GUI
bool reshade::d3d10::runtime_d3d10::init_imgui_resources()
{
	{   const resources::data_resource vs = resources::load_data_resource(IDR_IMGUI_VS);
		if (FAILED(_device->CreateVertexShader(vs.data, vs.data_size, &_imgui_vertex_shader)))
			return false;

		const D3D10_INPUT_ELEMENT_DESC input_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, pos), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, uv ), D3D10_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col), D3D10_INPUT_PER_VERTEX_DATA, 0 },
		};
		if (FAILED(_device->CreateInputLayout(input_layout, _countof(input_layout), vs.data, vs.data_size, &_imgui_input_layout)))
			return false;
	}

	{   const resources::data_resource ps = resources::load_data_resource(IDR_IMGUI_PS);
		if (FAILED(_device->CreatePixelShader(ps.data, ps.data_size, &_imgui_pixel_shader)))
			return false;
	}

	{   D3D10_BUFFER_DESC desc = {};
		desc.ByteWidth = 16 * sizeof(float);
		desc.Usage = D3D10_USAGE_IMMUTABLE;
		desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;

		// Setup orthographic projection matrix
		const float ortho_projection[16] = {
			2.0f / _width, 0.0f,   0.0f, 0.0f,
			0.0f, -2.0f / _height, 0.0f, 0.0f,
			0.0f,          0.0f,   0.5f, 0.0f,
			-1.f,          1.0f,   0.5f, 1.0f
		};

		D3D10_SUBRESOURCE_DATA initial_data = {};
		initial_data.pSysMem = ortho_projection;
		initial_data.SysMemPitch = sizeof(ortho_projection);
		if (FAILED(_device->CreateBuffer(&desc, &initial_data, &_imgui_constant_buffer)))
			return false;
	}

	{   D3D10_BLEND_DESC desc = {};
		desc.BlendEnable[0] = true;
		desc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
		desc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
		desc.BlendOp = D3D10_BLEND_OP_ADD;
		desc.SrcBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
		desc.DestBlendAlpha = D3D10_BLEND_ZERO;
		desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
		desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(_device->CreateBlendState(&desc, &_imgui_blend_state)))
			return false;
	}

	{   D3D10_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D10_FILL_SOLID;
		desc.CullMode = D3D10_CULL_NONE;
		desc.ScissorEnable = true;
		desc.DepthClipEnable = true;
		if (FAILED(_device->CreateRasterizerState(&desc, &_imgui_rasterizer_state)))
			return false;
	}

	{   D3D10_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = false;
		desc.StencilEnable = false;
		if (FAILED(_device->CreateDepthStencilState(&desc, &_imgui_depthstencil_state)))
			return false;
	}

	{   D3D10_SAMPLER_DESC desc = {};
		desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
		if (FAILED(_device->CreateSamplerState(&desc, &_imgui_texture_sampler)))
			return false;
	}

	return true;
}

void reshade::d3d10::runtime_d3d10::render_imgui_draw_data(ImDrawData *draw_data)
{
	assert(draw_data->DisplayPos.x == 0 && draw_data->DisplaySize.x == _width);
	assert(draw_data->DisplayPos.y == 0 && draw_data->DisplaySize.y == _height);

	// Create and grow vertex/index buffers if needed
	if (_imgui_index_buffer_size < draw_data->TotalIdxCount)
	{
		_imgui_index_buffer.reset();
		_imgui_index_buffer_size = draw_data->TotalIdxCount + 10000;

		D3D10_BUFFER_DESC desc = {};
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui_index_buffer_size * sizeof(ImDrawIdx);
		desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui_index_buffer)))
			return;
	}
	if (_imgui_vertex_buffer_size < draw_data->TotalVtxCount)
	{
		_imgui_vertex_buffer.reset();
		_imgui_vertex_buffer_size = draw_data->TotalVtxCount + 5000;

		D3D10_BUFFER_DESC desc = {};
		desc.Usage = D3D10_USAGE_DYNAMIC;
		desc.ByteWidth = _imgui_vertex_buffer_size * sizeof(ImDrawVert);
		desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;

		if (FAILED(_device->CreateBuffer(&desc, nullptr, &_imgui_vertex_buffer)))
			return;
	}

	ImDrawIdx *idx_dst; ImDrawVert *vtx_dst;
	if (FAILED(_imgui_index_buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, reinterpret_cast<void **>(&idx_dst))) ||
		FAILED(_imgui_vertex_buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, reinterpret_cast<void **>(&vtx_dst))))
		return;

	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];
		std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		idx_dst += draw_list->IdxBuffer.Size;
		vtx_dst += draw_list->VtxBuffer.Size;
	}

	_imgui_index_buffer->Unmap();
	_imgui_vertex_buffer->Unmap();

	// Setup render state and render draw lists
	_device->IASetInputLayout(_imgui_input_layout.get());
	_device->IASetIndexBuffer(_imgui_index_buffer.get(), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	const UINT stride = sizeof(ImDrawVert), offset = 0;
	ID3D10Buffer *const vertex_buffers[] = { _imgui_vertex_buffer.get() };
	_device->IASetVertexBuffers(0, ARRAYSIZE(vertex_buffers), vertex_buffers, &stride, &offset);
	_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_device->VSSetShader(_imgui_vertex_shader.get());
	ID3D10Buffer *const constant_buffers[] = { _imgui_constant_buffer.get() };
	_device->VSSetConstantBuffers(0, ARRAYSIZE(constant_buffers), constant_buffers);
	_device->GSSetShader(nullptr);
	_device->PSSetShader(_imgui_pixel_shader.get());
	ID3D10SamplerState *const samplers[] = { _imgui_texture_sampler.get() };
	_device->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
	_device->RSSetState(_imgui_rasterizer_state.get());
	const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
	_device->RSSetViewports(1, &viewport);
	const FLOAT blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	_device->OMSetBlendState(_imgui_blend_state.get(), blend_factor, D3D10_DEFAULT_SAMPLE_MASK);
	_device->OMSetDepthStencilState(_imgui_depthstencil_state.get(), 0);
	ID3D10RenderTargetView *const render_targets[] = { _backbuffer_rtv[0].get() };
	_device->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const D3D10_RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x),
				static_cast<LONG>(cmd.ClipRect.y),
				static_cast<LONG>(cmd.ClipRect.z),
				static_cast<LONG>(cmd.ClipRect.w)
			};
			_device->RSSetScissorRects(1, &scissor_rect);

			ID3D10ShaderResourceView *const texture_view =
				static_cast<const d3d10_tex_data *>(cmd.TextureId)->srv[0].get();
			_device->PSSetShaderResources(0, 1, &texture_view);

			_device->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset);
		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}
#endif

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
void reshade::d3d10::runtime_d3d10::draw_depth_debug_menu()
{
	if (ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		assert(_current_tracker != nullptr);

		bool modified = false;
		modified |= ImGui::Checkbox("Use aspect ratio heuristics", &_filter_aspect_ratio);
		modified |= ImGui::Checkbox("Copy depth buffers before clear operation", &_preserve_depth_buffers);

		if (modified) // Detection settings have changed, reset heuristic
			_current_tracker->reset(true);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		for (const auto &[dsv_texture, snapshot] : _current_tracker->depth_buffer_counters())
		{
			char label[512] = "";
			sprintf_s(label, "%s0x%p", (dsv_texture == _depth_texture || dsv_texture == _current_tracker->current_depth_texture() ? "> " : "  "), dsv_texture.get());

			D3D10_TEXTURE2D_DESC desc;
			dsv_texture->GetDesc(&desc);

			const bool msaa = desc.SampleDesc.Count > 1;
			if (msaa) // Disable widget for MSAA textures
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			}

			if (bool value = (_depth_texture_override == dsv_texture);
				ImGui::Checkbox(label, &value))
				_depth_texture_override = value ? dsv_texture.get() : nullptr;

			ImGui::SameLine();
			ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
				desc.Width, desc.Height, snapshot.total_stats.drawcalls, snapshot.total_stats.vertices, (msaa ? " MSAA" : ""));

			if (_preserve_depth_buffers && dsv_texture == _current_tracker->current_depth_texture())
			{
				for (UINT clear_index = 1; clear_index <= snapshot.clears.size(); ++clear_index)
				{
					sprintf_s(label, "%s  CLEAR %2u", (clear_index == _current_tracker->current_clear_index() ? "> " : "  "), clear_index);

					if (bool value = (_depth_clear_index_override == clear_index);
						ImGui::Checkbox(label, &value))
					{
						_depth_clear_index_override = value ? clear_index : std::numeric_limits<UINT>::max();
						modified = true;
					}

					ImGui::SameLine();
					ImGui::Text("%*s|           | %5u draw calls ==> %8u vertices |",
						sizeof(dsv_texture) - 4, "", // Add space to fill pointer length
						snapshot.clears[clear_index - 1].drawcalls, snapshot.clears[clear_index - 1].vertices);
				}
			}

			if (msaa)
			{
				ImGui::PopStyleColor();
				ImGui::PopItemFlag();
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (modified)
			runtime::save_config();
	}
}

void reshade::d3d10::runtime_d3d10::update_depthstencil_texture(com_ptr<ID3D10Texture2D> texture)
{
	if (texture == _depth_texture)
		return;

	_depth_texture = std::move(texture);
	_depth_texture_srv.reset();
	_has_depth_texture = false;

	if (_depth_texture != nullptr)
	{
		D3D10_TEXTURE2D_DESC tex_desc;
		_depth_texture->GetDesc(&tex_desc);

		assert((tex_desc.BindFlags & D3D10_BIND_SHADER_RESOURCE) != 0);

		D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Format = make_dxgi_format_normal(tex_desc.Format);

		if (HRESULT hr = _device->CreateShaderResourceView(_depth_texture.get(), &srv_desc, &_depth_texture_srv); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create depth stencil resource view! HRESULT is " << hr << '.';
			return;
		}
	}

	_has_depth_texture = true;

	// Update all references to the new texture
	for (auto &tex : _textures)
	{
		if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
		{
			const auto texture_impl = tex.impl->as<d3d10_tex_data>();
			assert(texture_impl != nullptr);

			// Update references in technique list
			for (const auto &technique : _techniques)
				for (const auto &pass : technique.passes_data)
					for (auto &srv : pass->as<d3d10_pass_data>()->shader_resources)
						if (srv == texture_impl->srv[0] || srv == texture_impl->srv[1])
							srv = _depth_texture_srv;

			texture_impl->srv[0] = _depth_texture_srv;
			texture_impl->srv[1] = _depth_texture_srv;
		}
	}
}
#endif
