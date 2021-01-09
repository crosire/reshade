/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_config.hpp"
#include "runtime_d3d9.hpp"
#include "runtime_objects.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <d3dcompiler.h>

namespace reshade::d3d9
{
	struct tex_data
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};

	struct pass_data
	{
		com_ptr<IDirect3DStateBlock9> stateblock;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		IDirect3DSurface9 *render_targets[8] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
	};

	struct technique_data
	{
		DWORD num_samplers = 0;
		DWORD sampler_states[16][12] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
		DWORD constant_register_count = 0;
		std::vector<pass_data> passes;
	};
}

reshade::d3d9::runtime_d3d9::runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain, state_tracking *state_tracking) :
	_app_state(device), _state_tracking(*state_tracking), _device(device), _swapchain(swapchain)
{
	assert(device != nullptr && swapchain != nullptr && state_tracking != nullptr);

	_device->GetDirect3D(&_d3d);
	assert(_d3d != nullptr);

	D3DCAPS9 caps = {};
	_device->GetDeviceCaps(&caps);
	D3DDEVICE_CREATION_PARAMETERS creation_params = {};
	_device->GetCreationParameters(&creation_params);

	_renderer_id = 0x9000;

	if (D3DADAPTER_IDENTIFIER9 adapter_desc;
		SUCCEEDED(_d3d->GetAdapterIdentifier(creation_params.AdapterOrdinal, 0, &adapter_desc)))
	{
		_vendor_id = adapter_desc.VendorId;
		_device_id = adapter_desc.DeviceId;

		// Only the last 5 digits represents the version specific to a driver
		// See https://docs.microsoft.com/windows-hardware/drivers/display/version-numbers-for-display-drivers
		const DWORD driver_version = LOWORD(adapter_desc.DriverVersion.LowPart) + (HIWORD(adapter_desc.DriverVersion.LowPart) % 10) * 10000;
		LOG(INFO) << "Running on " << adapter_desc.Description << " Driver " << (driver_version / 100) << '.' << (driver_version % 100);
	}

	_num_samplers = caps.MaxSimultaneousTextures;
	_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, static_cast<DWORD>(8));
	_behavior_flags = creation_params.BehaviorFlags;

#if RESHADE_GUI
	subscribe_to_ui("D3D9", [this]() {
#if RESHADE_DEPTH
		draw_depth_debug_menu();
#endif
	});
#endif
#if RESHADE_DEPTH
	subscribe_to_load_config([this](const ini_file &config) {
		config.get("DEPTH", "DisableINTZ", _disable_intz);
		config.get("DEPTH", "DepthCopyBeforeClears", _state_tracking.preserve_depth_buffers);
		config.get("DEPTH", "DepthCopyAtClearIndex", _state_tracking.depthstencil_clear_index);
		config.get("DEPTH", "UseAspectRatioHeuristics", _state_tracking.use_aspect_ratio_heuristics);

		if (_state_tracking.depthstencil_clear_index == std::numeric_limits<UINT>::max())
			_state_tracking.depthstencil_clear_index  = 0;
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("DEPTH", "DisableINTZ", _disable_intz);
		config.set("DEPTH", "DepthCopyBeforeClears", _state_tracking.preserve_depth_buffers);
		config.set("DEPTH", "DepthCopyAtClearIndex", _state_tracking.depthstencil_clear_index);
		config.set("DEPTH", "UseAspectRatioHeuristics", _state_tracking.use_aspect_ratio_heuristics);
	});
#endif

	if (!on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 9 runtime environment on runtime " << this << '!';
}
reshade::d3d9::runtime_d3d9::~runtime_d3d9()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d9::runtime_d3d9::on_init()
{
	// Retrieve present parameters here, instead using the ones passed in during creation, to get correct values for 'BackBufferWidth' and 'BackBufferHeight'
	// They may otherwise still be set to zero (which is valid for creation)
	D3DPRESENT_PARAMETERS pp;
	if (FAILED(_swapchain->GetPresentParameters(&pp)))
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
	HRESULT hr = _swapchain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer);
	assert(SUCCEEDED(hr));

	if (pp.MultiSampleType != D3DMULTISAMPLE_NONE || (pp.BackBufferFormat == D3DFMT_X8R8G8B8 || pp.BackBufferFormat == D3DFMT_X8B8G8R8))
	{
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

	// Create back buffer shader texture
	if (FAILED(_device->CreateTexture(_width, _height, 1, D3DUSAGE_RENDERTARGET, _backbuffer_format, D3DPOOL_DEFAULT, &_backbuffer_texture, nullptr)))
		return false;

	hr = _backbuffer_texture->GetSurfaceLevel(0, &_backbuffer_texture_surface);
	assert(SUCCEEDED(hr));

	// Create effect depth-stencil surface
	if (FAILED(_device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_effect_stencil, nullptr)))
		return false;

	// Create vertex layout for vertex buffer which holds vertex indices
	const D3DVERTEXELEMENT9 declaration[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	if (FAILED(_device->CreateVertexDeclaration(declaration, &_effect_vertex_layout)))
		return false;

	// Create state block object
	if (!_app_state.init_state_block())
		return false;

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(pp.hDeviceWindow);
}
void reshade::d3d9::runtime_d3d9::on_reset()
{
	runtime::on_reset();

	_app_state.release_state_block();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_texture.reset();
	_backbuffer_texture_surface.reset();

	_max_vertices = 0;
	_effect_stencil.reset();
	_effect_vertex_buffer.reset();
	_effect_vertex_layout.reset();

#if RESHADE_GUI
	_imgui.state.reset();
	_imgui.indices.reset();
	_imgui.vertices.reset();
	_imgui.num_indices = 0;
	_imgui.num_vertices = 0;
#endif

#if RESHADE_DEPTH
	_depth_texture.reset();
	_depth_surface.reset();

	_has_depth_texture = false;
	_depth_surface_override = nullptr;
#endif
}

void reshade::d3d9::runtime_d3d9::on_present()
{
	if (!_is_initialized || FAILED(_device->BeginScene()))
		return;

	_vertices = _state_tracking.total_vertices();
	_drawcalls = _state_tracking.total_drawcalls();

#if RESHADE_DEPTH
	// Disable INTZ replacement while high network activity is detected, since the option is not available in the UI then, but artifacts may occur without it
	_state_tracking.disable_intz = _disable_intz || _has_high_network_activity;

	update_depth_texture_bindings(_has_high_network_activity ? nullptr :
		_state_tracking.find_best_depth_surface(_width, _height, _depth_surface_override));
#endif

	_app_state.capture();
	BOOL software_rendering_enabled = FALSE;
	if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
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
	if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		_device->SetSoftwareVertexProcessing(software_rendering_enabled);

#if RESHADE_DEPTH
	// Can only reset the tracker after the state block has been applied, to ensure current depth-stencil binding is updated correctly
	if (_reset_buffer_detection)
	{
		_state_tracking.reset(true);
		_reset_buffer_detection = false;
	}
#endif

	_device->EndScene();
}

bool reshade::d3d9::runtime_d3d9::capture_screenshot(uint8_t *buffer) const
{
	if (_color_bit_depth != 8 && _color_bit_depth != 10)
	{
		LOG(ERROR) << "Screenshots are not supported for back buffer format " << _backbuffer_format << '!';
		return false;
	}

	// Create a surface in system memory, copy back buffer data into it and lock it for reading
	com_ptr<IDirect3DSurface9> intermediate;
	if (HRESULT hr = _device->CreateOffscreenPlainSurface(_width, _height, _backbuffer_format, D3DPOOL_SYSTEMMEM, &intermediate, nullptr); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << _width << ", Height = " << _height << ", Format = " << _backbuffer_format;
		return false;
	}

	if (FAILED(_device->GetRenderTargetData(_backbuffer_resolved.get(), intermediate.get())))
		return false;

	D3DLOCKED_RECT mapped;
	if (FAILED(intermediate->LockRect(&mapped, nullptr, D3DLOCK_READONLY)))
		return false;
	auto mapped_data = static_cast<const uint8_t *>(mapped.pBits);

	for (uint32_t y = 0, pitch = _width * 4; y < _height; y++, buffer += pitch, mapped_data += mapped.Pitch)
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
				if (_backbuffer_format == D3DFMT_A2R10G10B10)
					std::swap(buffer[x + 0], buffer[x + 2]);
			}
		}
		else
		{
			std::memcpy(buffer, mapped_data, pitch);

			if (_backbuffer_format == D3DFMT_A8R8G8B8 ||
				_backbuffer_format == D3DFMT_X8R8G8B8)
			{
				// Format is BGRA, but output should be RGBA, so flip channels
				for (uint32_t x = 0; x < pitch; x += 4)
					std::swap(buffer[x + 0], buffer[x + 2]);
			}
		}
	}

	intermediate->UnlockRect();

	return true;
}

bool reshade::d3d9::runtime_d3d9::init_effect(size_t index)
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

	std::unordered_map<std::string, com_ptr<IUnknown>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
	{
		HRESULT hr = E_FAIL;

		std::string profile;
		com_ptr<ID3DBlob> compiled, d3d_errors;

		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			profile = "vs_3_0";
			break;
		case reshadefx::shader_type::ps:
			profile = "ps_3_0";
			break;
		case reshadefx::shader_type::cs:
			effect.errors += "Compute shaders are not supported in ";
			effect.errors += "D3D9";
			effect.errors += '.';
			return false;
		}

		std::string attributes;
		attributes += "entrypoint=" + entry_point.name + ';';
		attributes += "profile=" + profile + ';';
		attributes += "flags=" + std::to_string(_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1) + ';';

		const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
		std::vector<char> cso;
		if (!load_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]))
		{
			hr = D3DCompile(
				hlsl.data(), hlsl.size(), nullptr,
				entry_point.type == reshadefx::shader_type::ps ? ps_defines : nullptr,
				nullptr,
				entry_point.name.c_str(),
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
				effect.assembly[entry_point.name].assign(static_cast<const char *>(disassembled->GetBufferPointer()), disassembled->GetBufferSize() - 1);

			save_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]);
		}

		// Create runtime shader objects from the compiled DX byte code
		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			hr = _device->CreateVertexShader(reinterpret_cast<const DWORD *>(cso.data()), reinterpret_cast<IDirect3DVertexShader9 **>(&entry_points[entry_point.name]));
			break;
		case reshadefx::shader_type::ps:
			hr = _device->CreatePixelShader(reinterpret_cast<const DWORD *>(cso.data()), reinterpret_cast<IDirect3DPixelShader9 **>(&entry_points[entry_point.name]));
			break;
		}

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader for entry point '" << entry_point.name << "'! HRESULT is " << hr << '.';
			return false;
		}
	}

	technique_data technique_init;
	assert(effect.module.num_texture_bindings == 0);
	assert(effect.module.num_storage_bindings == 0);
	technique_init.num_samplers = effect.module.num_sampler_bindings;
	technique_init.constant_register_count = static_cast<DWORD>((effect.uniform_data_storage.size() + 15) / 16);

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding >= ARRAYSIZE(technique_init.sampler_states))
		{
			LOG(ERROR) << "Cannot bind sampler '" << info.unique_name << "' since it exceeds the maximum number of allowed sampler slots in " << "D3D9" << " (" << info.binding << ", allowed are up to " << ARRAYSIZE(technique_init.sampler_states) << ").";
			return false;
		}

		const texture &texture = look_up_texture_by_name(info.texture_name);

		technique_init.sampler_textures[info.binding] = static_cast<tex_data *>(texture.impl)->texture.get();

		// Since textures with auto-generated mipmap levels do not have a mipmap maximum, limit the bias here so this is not as obvious
		assert(texture.levels > 0);
		const float lod_bias = std::min(texture.levels - 1.0f, info.lod_bias);

		technique_init.sampler_states[info.binding][D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(info.address_u);
		technique_init.sampler_states[info.binding][D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(info.address_v);
		technique_init.sampler_states[info.binding][D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(info.address_w);
		technique_init.sampler_states[info.binding][D3DSAMP_BORDERCOLOR] = 0;
		technique_init.sampler_states[info.binding][D3DSAMP_MAGFILTER] = 1 + ((static_cast<unsigned int>(info.filter) & 0x0C) >> 2);
		technique_init.sampler_states[info.binding][D3DSAMP_MINFILTER] = 1 + ((static_cast<unsigned int>(info.filter) & 0x30) >> 4);
		technique_init.sampler_states[info.binding][D3DSAMP_MIPFILTER] = 1 + ((static_cast<unsigned int>(info.filter) & 0x03));
		technique_init.sampler_states[info.binding][D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&lod_bias);
		technique_init.sampler_states[info.binding][D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, info.min_lod));
		technique_init.sampler_states[info.binding][D3DSAMP_MAXANISOTROPY] = 1;
		technique_init.sampler_states[info.binding][D3DSAMP_SRGBTEXTURE] = info.srgb;
	}

	UINT max_vertices = 3;

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		auto impl = new technique_data(technique_init);
		technique.impl = impl;

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			pass_data &pass_data = impl->passes[pass_index];
			const reshadefx::pass_info &pass_info = technique.passes[pass_index];

			max_vertices = std::max(max_vertices, pass_info.num_vertices);

			entry_points.at(pass_info.ps_entry_point)->QueryInterface(&pass_data.pixel_shader);
			entry_points.at(pass_info.vs_entry_point)->QueryInterface(&pass_data.vertex_shader);

			pass_data.render_targets[0] = _backbuffer_resolved.get();

			for (UINT k = 0; k < ARRAYSIZE(pass_data.sampler_textures); ++k)
				pass_data.sampler_textures[k] = impl->sampler_textures[k];

			for (UINT k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
			{
				if (k > _num_simultaneous_rendertargets)
				{
					LOG(WARN) << "Device only supports " << _num_simultaneous_rendertargets << " simultaneous render targets, but pass " << pass_index << " in technique '" << technique.name << "' uses more, which are ignored.";
					break;
				}

				tex_data *const tex_impl = static_cast<tex_data *>(
					look_up_texture_by_name(pass_info.render_target_names[k]).impl);

				// Unset textures that are used as render target
				for (DWORD s = 0; s < impl->num_samplers; ++s)
					if (tex_impl->texture == pass_data.sampler_textures[s])
						pass_data.sampler_textures[s] = nullptr;

				pass_data.render_targets[k] = tex_impl->surface.get();
			}

			HRESULT hr = _device->BeginStateBlock();
			if (SUCCEEDED(hr))
			{
				_device->SetVertexShader(pass_data.vertex_shader.get());
				_device->SetPixelShader(pass_data.pixel_shader.get());

				const auto convert_blend_op = [](reshadefx::pass_blend_op value) {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return D3DBLENDOP_ADD;
					case reshadefx::pass_blend_op::subtract: return D3DBLENDOP_SUBTRACT;
					case reshadefx::pass_blend_op::rev_subtract: return D3DBLENDOP_REVSUBTRACT;
					case reshadefx::pass_blend_op::min: return D3DBLENDOP_MIN;
					case reshadefx::pass_blend_op::max: return D3DBLENDOP_MAX;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_func::one: return D3DBLEND_ONE;
					case reshadefx::pass_blend_func::zero: return D3DBLEND_ZERO;
					case reshadefx::pass_blend_func::src_color: return D3DBLEND_SRCCOLOR;
					case reshadefx::pass_blend_func::src_alpha: return D3DBLEND_SRCALPHA;
					case reshadefx::pass_blend_func::inv_src_color: return D3DBLEND_INVSRCCOLOR;
					case reshadefx::pass_blend_func::inv_src_alpha: return D3DBLEND_INVSRCALPHA;
					case reshadefx::pass_blend_func::dst_alpha: return D3DBLEND_DESTALPHA;
					case reshadefx::pass_blend_func::dst_color: return D3DBLEND_DESTCOLOR;
					case reshadefx::pass_blend_func::inv_dst_alpha: return D3DBLEND_INVDESTALPHA;
					case reshadefx::pass_blend_func::inv_dst_color: return D3DBLEND_INVDESTCOLOR;
					}
				};
				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
					switch (value)
					{
					default:
					case reshadefx::pass_stencil_op::keep: return D3DSTENCILOP_KEEP;
					case reshadefx::pass_stencil_op::zero: return D3DSTENCILOP_ZERO;
					case reshadefx::pass_stencil_op::invert: return D3DSTENCILOP_INVERT;
					case reshadefx::pass_stencil_op::replace: return D3DSTENCILOP_REPLACE;
					case reshadefx::pass_stencil_op::incr: return D3DSTENCILOP_INCR;
					case reshadefx::pass_stencil_op::incr_sat: return D3DSTENCILOP_INCRSAT;
					case reshadefx::pass_stencil_op::decr: return D3DSTENCILOP_DECR;
					case reshadefx::pass_stencil_op::decr_sat: return D3DSTENCILOP_DECRSAT;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) {
					switch (value)
					{
					default:
					case reshadefx::pass_stencil_func::always: return D3DCMP_ALWAYS;
					case reshadefx::pass_stencil_func::never: return D3DCMP_NEVER;
					case reshadefx::pass_stencil_func::equal: return D3DCMP_EQUAL;
					case reshadefx::pass_stencil_func::not_equal: return D3DCMP_NOTEQUAL;
					case reshadefx::pass_stencil_func::less: return D3DCMP_LESS;
					case reshadefx::pass_stencil_func::less_equal: return D3DCMP_LESSEQUAL;
					case reshadefx::pass_stencil_func::greater: return D3DCMP_GREATER;
					case reshadefx::pass_stencil_func::greater_equal: return D3DCMP_GREATEREQUAL;
					}
				};

				_device->SetRenderState(D3DRS_ZENABLE, FALSE);
				_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
				// D3DRS_SHADEMODE
				_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
				_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				_device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
				_device->SetRenderState(D3DRS_SRCBLEND, convert_blend_func(pass_info.src_blend));
				_device->SetRenderState(D3DRS_DESTBLEND, convert_blend_func(pass_info.dest_blend));
				_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
				// D3DRS_ALPHAREF
				// D3DRS_ALPHAFUNC
				_device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
				_device->SetRenderState(D3DRS_ALPHABLENDENABLE, pass_info.blend_enable);
				_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
				_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
				// D3DRS_FOGCOLOR
				// D3DRS_FOGTABLEMODE
				// D3DRS_FOGSTART
				// D3DRS_FOGEND
				// D3DRS_FOGDENSITY
				// D3DRS_RANGEFOGENABLE
				_device->SetRenderState(D3DRS_STENCILENABLE, pass_info.stencil_enable);
				_device->SetRenderState(D3DRS_STENCILFAIL, convert_stencil_op(pass_info.stencil_op_fail));
				_device->SetRenderState(D3DRS_STENCILZFAIL, convert_stencil_op(pass_info.stencil_op_depth_fail));
				_device->SetRenderState(D3DRS_STENCILPASS, convert_stencil_op(pass_info.stencil_op_pass));
				_device->SetRenderState(D3DRS_STENCILFUNC, convert_stencil_func(pass_info.stencil_comparison_func));
				_device->SetRenderState(D3DRS_STENCILREF, pass_info.stencil_reference_value);
				_device->SetRenderState(D3DRS_STENCILMASK, pass_info.stencil_read_mask);
				_device->SetRenderState(D3DRS_STENCILWRITEMASK, pass_info.stencil_write_mask);
				// D3DRS_TEXTUREFACTOR
				// D3DRS_WRAP0 - D3DRS_WRAP7
				_device->SetRenderState(D3DRS_CLIPPING, FALSE);
				_device->SetRenderState(D3DRS_LIGHTING, FALSE);
				// D3DRS_AMBIENT
				// D3DRS_FOGVERTEXMODE
				_device->SetRenderState(D3DRS_COLORVERTEX, FALSE);
				// D3DRS_LOCALVIEWER
				_device->SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
				_device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				_device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
				_device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				_device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				_device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
				_device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
				// D3DRS_POINTSIZE
				// D3DRS_POINTSIZE_MIN
				// D3DRS_POINTSPRITEENABLE
				// D3DRS_POINTSCALEENABLE
				// D3DRS_POINTSCALE_A - D3DRS_POINTSCALE_C
				// D3DRS_MULTISAMPLEANTIALIAS
				// D3DRS_MULTISAMPLEMASK
				// D3DRS_PATCHEDGESTYLE
				// D3DRS_DEBUGMONITORTOKEN
				// D3DRS_POINTSIZE_MAX
				// D3DRS_INDEXEDVERTEXBLENDENABLE
				_device->SetRenderState(D3DRS_COLORWRITEENABLE, pass_info.color_write_mask);
				// D3DRS_TWEENFACTOR
				_device->SetRenderState(D3DRS_BLENDOP, convert_blend_op(pass_info.blend_op));
				// D3DRS_POSITIONDEGREE
				// D3DRS_NORMALDEGREE
				_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
				_device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
				// D3DRS_MINTESSELLATIONLEVEL
				// D3DRS_MAXTESSELLATIONLEVEL
				// D3DRS_ADAPTIVETESS_X - D3DRS_ADAPTIVETESS_W
				_device->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
				_device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
				// D3DRS_CCW_STENCILFAIL
				// D3DRS_CCW_STENCILZFAIL
				// D3DRS_CCW_STENCILPASS
				// D3DRS_CCW_STENCILFUNC
				_device->SetRenderState(D3DRS_COLORWRITEENABLE1, pass_info.color_write_mask); // See https://docs.microsoft.com/en-us/windows/win32/direct3d9/multiple-render-targets
				_device->SetRenderState(D3DRS_COLORWRITEENABLE2, pass_info.color_write_mask);
				_device->SetRenderState(D3DRS_COLORWRITEENABLE3, pass_info.color_write_mask);
				_device->SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
				_device->SetRenderState(D3DRS_SRGBWRITEENABLE, pass_info.srgb_write_enable);
				_device->SetRenderState(D3DRS_DEPTHBIAS, 0);
				// D3DRS_WRAP8 - D3DRS_WRAP15
				_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
				_device->SetRenderState(D3DRS_SRCBLENDALPHA, convert_blend_func(pass_info.src_blend_alpha));
				_device->SetRenderState(D3DRS_DESTBLENDALPHA, convert_blend_func(pass_info.dest_blend_alpha));
				_device->SetRenderState(D3DRS_BLENDOPALPHA, convert_blend_op(pass_info.blend_op_alpha));

				hr = _device->EndStateBlock(&pass_data.stateblock);
			}

			if (FAILED(hr))
			{
				LOG(ERROR) << "Failed to create state block for pass " << pass_index << " in technique '" << technique.name << "'! HRESULT is " << hr << '.';
				return false;
			}
		}
	}

	// Update vertex buffer which holds vertex indices
	if (max_vertices > _max_vertices)
	{
		_effect_vertex_buffer.reset();

		if (FAILED(_device->CreateVertexBuffer(max_vertices * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effect_vertex_buffer, nullptr)))
			return false;

		if (float *data;
			SUCCEEDED(_effect_vertex_buffer->Lock(0, max_vertices * sizeof(float), reinterpret_cast<void **>(&data), 0)))
		{
			for (UINT i = 0; i < max_vertices; i++)
				data[i] = static_cast<float>(i);
			_effect_vertex_buffer->Unlock();
		}

		_max_vertices = max_vertices;
	}

	return true;
}
void reshade::d3d9::runtime_d3d9::unload_effect(size_t index)
{
	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);
}
void reshade::d3d9::runtime_d3d9::unload_effects()
{
	for (technique &tech : _techniques)
	{
		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effects();
}

bool reshade::d3d9::runtime_d3d9::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	if (texture.semantic == "COLOR")
	{
		impl->texture = _backbuffer_texture;
		impl->surface = _backbuffer_texture_surface;
		return true;
	}
	if (texture.semantic == "DEPTH")
	{
#if RESHADE_DEPTH
		impl->texture = _depth_texture;
		impl->surface = _depth_surface;
#endif
		return true;
	}

	UINT levels = texture.levels;
	DWORD usage = 0;
	D3DFORMAT format = D3DFMT_UNKNOWN;
	D3DDEVICE_CREATION_PARAMETERS cp;
	_device->GetCreationParameters(&cp);

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		format = D3DFMT_X8R8G8B8; // Use 4-component format so that green/blue components are returned as zero and alpha as one (to match behavior from other APIs)
		break;
	case reshadefx::texture_format::r16f:
		format = D3DFMT_R16F;
		break;
	case reshadefx::texture_format::r32f:
		format = D3DFMT_R32F;
		break;
	case reshadefx::texture_format::rg8:
		format = D3DFMT_X8R8G8B8;
		break;
	case reshadefx::texture_format::rg16:
		format = D3DFMT_G16R16;
		break;
	case reshadefx::texture_format::rg16f:
		format = D3DFMT_G16R16F;
		break;
	case reshadefx::texture_format::rg32f:
		format = D3DFMT_G32R32F;
		break;
	case reshadefx::texture_format::rgba8:
		format = D3DFMT_A8R8G8B8;
		break;
	case reshadefx::texture_format::rgba16:
		format = D3DFMT_A16B16G16R16;
		break;
	case reshadefx::texture_format::rgba16f:
		format = D3DFMT_A16B16G16R16F;
		break;
	case reshadefx::texture_format::rgba32f:
		format = D3DFMT_A32B32G32R32F;
		break;
	case reshadefx::texture_format::rgb10a2:
		format = D3DFMT_A2B10G10R10;
		break;
	}

	if (levels > 1)
	{
		// Enable auto-generated mipmaps if the format supports it
		if (_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, format) == D3D_OK)
		{
			usage |= D3DUSAGE_AUTOGENMIPMAP;
			levels = 0;
		}
		else
		{
			LOG(WARN) << "Auto-generated mipmap levels are not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'.";
		}
	}

	if (texture.render_target)
	{
		// Make texture a render target if format allows it
		if (_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format) == D3D_OK)
		{
			usage |= D3DUSAGE_RENDERTARGET;
		}
		else
		{
			LOG(WARN) << "Render target usage is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'.";
		}
	}

	HRESULT hr = _device->CreateTexture(texture.width, texture.height, levels, usage, format, D3DPOOL_DEFAULT, &impl->texture, nullptr);
	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << texture.width << ", Height = " << texture.height << ", Levels = " << levels << ", Usage = " << usage << ", Format = " << format;
		return false;
	}

	hr = impl->texture->GetSurfaceLevel(0, &impl->surface);
	assert(SUCCEEDED(hr));

	// Clear texture to zero since by default its contents are undefined
	if (usage & D3DUSAGE_RENDERTARGET)
		_device->ColorFill(impl->surface.get(), nullptr, D3DCOLOR_ARGB(0, 0, 0, 0));

	return true;
}
void reshade::d3d9::runtime_d3d9::upload_texture(const texture &texture, const uint8_t *pixels)
{
	auto impl = static_cast<tex_data *>(texture.impl);
	assert(impl != nullptr && texture.semantic.empty() && pixels != nullptr);

	D3DSURFACE_DESC desc; impl->texture->GetLevelDesc(0, &desc); // Get D3D texture format
	com_ptr<IDirect3DTexture9> intermediate;
	if (HRESULT hr = _device->CreateTexture(texture.width, texture.height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &intermediate, nullptr); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for updating texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << texture.width << ", Height = " << texture.height << ", Levels = " << "1" << ", Usage = " << "0" << ", Format = " << desc.Format;
		return;
	}

	D3DLOCKED_RECT mapped;
	if (FAILED(intermediate->LockRect(0, &mapped, nullptr, 0)))
		return;
	auto mapped_data = static_cast<uint8_t *>(mapped.pBits);

	switch (texture.format)
	{
	case reshadefx::texture_format::r8: // These are actually D3DFMT_X8R8G8B8, see 'init_texture'
		for (uint32_t y = 0, pitch = texture.width * 4; y < texture.height; ++y, mapped_data += mapped.Pitch, pixels += pitch)
			for (uint32_t x = 0; x < pitch; x += 4)
				mapped_data[x + 0] = 0, // Set green and blue channel to zero
				mapped_data[x + 1] = 0,
				mapped_data[x + 2] = pixels[x + 0],
				mapped_data[x + 3] = 0xFF;
		break;
	case reshadefx::texture_format::rg8:
		for (uint32_t y = 0, pitch = texture.width * 4; y < texture.height; ++y, mapped_data += mapped.Pitch, pixels += pitch)
			for (uint32_t x = 0; x < pitch; x += 4)
				mapped_data[x + 0] = 0, // Set blue channel to zero
				mapped_data[x + 1] = pixels[x + 1],
				mapped_data[x + 2] = pixels[x + 0],
				mapped_data[x + 3] = 0xFF;
		break;
	case reshadefx::texture_format::rgba8:
		for (uint32_t y = 0, pitch = texture.width * 4; y < texture.height; ++y, mapped_data += mapped.Pitch, pixels += pitch)
			for (uint32_t x = 0; x < pitch; x += 4)
				mapped_data[x + 0] = pixels[x + 2], // Flip RGBA input to BGRA
				mapped_data[x + 1] = pixels[x + 1],
				mapped_data[x + 2] = pixels[x + 0],
				mapped_data[x + 3] = pixels[x + 3];
		break;
	default:
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'!";
		break;
	}

	intermediate->UnlockRect(0);

	if (HRESULT hr = _device->UpdateTexture(intermediate.get(), impl->texture.get()); FAILED(hr))
	{
		LOG(ERROR) << "Failed to update texture '" << texture.unique_name << "' from system memory texture! HRESULT is " << hr << '.';
		return;
	}
}
void reshade::d3d9::runtime_d3d9::destroy_texture(texture &texture)
{
	delete static_cast<tex_data *>(texture.impl);
	texture.impl = nullptr;
}

void reshade::d3d9::runtime_d3d9::render_technique(technique &technique)
{
	const auto impl = static_cast<technique_data *>(technique.impl);

	// Setup vertex input (used to have a vertex ID as vertex shader input)
	_device->SetStreamSource(0, _effect_vertex_buffer.get(), 0, sizeof(float));
	_device->SetVertexDeclaration(_effect_vertex_layout.get());

	// Setup shader constants
	if (impl->constant_register_count != 0)
	{
		const auto uniform_storage_data = reinterpret_cast<const float *>(_effects[technique.effect_index].uniform_data_storage.data());
		_device->SetPixelShaderConstantF(0, uniform_storage_data, impl->constant_register_count);
		_device->SetVertexShaderConstantF(0, uniform_storage_data, impl->constant_register_count);
	}

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			// Save back buffer of previous pass
			_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer_texture_surface.get(), nullptr, D3DTEXF_NONE);
		}

		const pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

		// Setup state
		pass_data.stateblock->Apply();

		// Setup shader resources
		for (DWORD s = 0; s < impl->num_samplers; s++)
		{
			_device->SetTexture(s, pass_data.sampler_textures[s]);

			// Need to bind textures to vertex shader samplers too
			// See https://docs.microsoft.com/windows/win32/direct3d9/vertex-textures-in-vs-3-0
			if (s < 4)
				_device->SetTexture(D3DVERTEXTEXTURESAMPLER0 + s, pass_data.sampler_textures[s]);

			for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state++)
			{
				_device->SetSamplerState(s, static_cast<D3DSAMPLERSTATETYPE>(state), impl->sampler_states[s][state]);

				if (s < 4) // vs_3_0 supports up to four samplers in vertex shaders
					_device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0 + s, static_cast<D3DSAMPLERSTATETYPE>(state), impl->sampler_states[s][state]);
			}
		}

		// Setup render targets (and viewport, which is implicitly updated by 'SetRenderTarget')
		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
			_device->SetRenderTarget(target, pass_data.render_targets[target]);

		D3DVIEWPORT9 viewport;
		_device->GetViewport(&viewport);
		_device->SetDepthStencilSurface(viewport.Width == _width && viewport.Height == _height && pass_info.stencil_enable ? _effect_stencil.get() : nullptr);

		if (pass_info.stencil_enable && viewport.Width == _width && viewport.Height == _height && !is_effect_stencil_cleared)
		{
			is_effect_stencil_cleared = true;

			_device->Clear(0, nullptr, (pass_info.clear_render_targets ? D3DCLEAR_TARGET : 0) | D3DCLEAR_STENCIL, 0, 1.0f, 0);
		}
		else if (pass_info.clear_render_targets)
		{
			_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
		}

		// Set __TEXEL_SIZE__ constant (see effect_codegen_hlsl.cpp)
		const float texel_size[4] = {
			-1.0f / viewport.Width,
			 1.0f / viewport.Height
		};
		_device->SetVertexShaderConstantF(255, texel_size, 1);

		// Draw primitives
		UINT primitive_count = pass_info.num_vertices;
		D3DPRIMITIVETYPE topology;
		switch (pass_info.topology)
		{
		case reshadefx::primitive_topology::point_list:
			topology = D3DPT_POINTLIST;
			break;
		case reshadefx::primitive_topology::line_list:
			topology = D3DPT_LINELIST;
			primitive_count /= 2;
			break;
		case reshadefx::primitive_topology::line_strip:
			topology = D3DPT_LINESTRIP;
			primitive_count -= 1;
			break;
		default:
		case reshadefx::primitive_topology::triangle_list:
			topology = D3DPT_TRIANGLELIST;
			primitive_count /= 3;
			break;
		case reshadefx::primitive_topology::triangle_strip:
			topology = D3DPT_TRIANGLESTRIP;
			primitive_count -= 2;
			break;
		}
		_device->DrawPrimitive(topology, 0, primitive_count);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		needs_implicit_backbuffer_copy = false;

		// Generate mipmaps for modified resources
		for (IDirect3DSurface9 *target : pass_data.render_targets)
		{
			if (target == nullptr)
				break;

			if (target == _backbuffer_resolved)
			{
				needs_implicit_backbuffer_copy = true;
				break;
			}

			if (com_ptr<IDirect3DBaseTexture9> texture;
				SUCCEEDED(target->GetContainer(IID_PPV_ARGS(&texture))) && texture->GetLevelCount() > 1)
			{
				texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
				texture->GenerateMipSubLevels();
			}
		}
	}
}

#if RESHADE_GUI
bool reshade::d3d9::runtime_d3d9::init_imgui_resources()
{
	HRESULT hr = _device->BeginStateBlock();
	if (SUCCEEDED(hr))
	{
		_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
		_device->SetPixelShader(nullptr);
		_device->SetVertexShader(nullptr);
		_device->SetRenderState(D3DRS_ZENABLE, false);
		_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
		_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		_device->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		_device->SetRenderState(D3DRS_FOGENABLE, false);
		_device->SetRenderState(D3DRS_STENCILENABLE, false);
		_device->SetRenderState(D3DRS_CLIPPING, false);
		_device->SetRenderState(D3DRS_LIGHTING, false);
		_device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
		_device->SetRenderState(D3DRS_SCISSORTESTENABLE, true);
		_device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		_device->SetRenderState(D3DRS_SRGBWRITEENABLE, false);
		_device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
		_device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		_device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		_device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		_device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
		_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		_device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
		_device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
		_device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

		hr = _device->EndStateBlock(&_imgui.state);
	}

	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create state block! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

void reshade::d3d9::runtime_d3d9::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Fixed-function vertex layout
	struct ImDrawVert9
	{
		float x, y, z;
		D3DCOLOR col;
		float u, v;
	};

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices < draw_data->TotalIdxCount)
	{
		_imgui.indices.reset();
		_imgui.num_indices = draw_data->TotalIdxCount + 10000;

		if (FAILED(_device->CreateIndexBuffer(_imgui.num_indices * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &_imgui.indices, nullptr)))
			return;
	}
	if (_imgui.num_vertices < draw_data->TotalVtxCount)
	{
		_imgui.vertices.reset();
		_imgui.num_vertices = draw_data->TotalVtxCount + 5000;

		if (FAILED(_device->CreateVertexBuffer(_imgui.num_vertices * sizeof(ImDrawVert9), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, &_imgui.vertices, nullptr)))
			return;
	}

	if (ImDrawIdx *idx_dst;
		SUCCEEDED(_imgui.indices->Lock(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx), reinterpret_cast<void **>(&idx_dst), D3DLOCK_DISCARD)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.size() * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.size();
		}

		_imgui.indices->Unlock();
	}
	if (ImDrawVert9 *vtx_dst;
		SUCCEEDED(_imgui.vertices->Lock(0, draw_data->TotalVtxCount * sizeof(ImDrawVert9), reinterpret_cast<void **>(&vtx_dst), D3DLOCK_DISCARD)))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];

			for (const ImDrawVert &vtx : draw_list->VtxBuffer)
			{
				vtx_dst->x = vtx.pos.x;
				vtx_dst->y = vtx.pos.y;
				vtx_dst->z = 0.0f;

				// RGBA --> ARGB
				vtx_dst->col = (vtx.col & 0xFF00FF00) | ((vtx.col & 0xFF0000) >> 16) | ((vtx.col & 0xFF) << 16);

				vtx_dst->u = vtx.uv.x;
				vtx_dst->v = vtx.uv.y;

				++vtx_dst; // Next vertex
			}
		}

		_imgui.vertices->Unlock();
	}

	// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
	_imgui.state->Apply();
	_device->SetIndices(_imgui.indices.get());
	_device->SetStreamSource(0, _imgui.vertices.get(), 0, sizeof(ImDrawVert9));
	_device->SetRenderTarget(0, _backbuffer_resolved.get());

	// Clear unused bindings
	for (unsigned int i = 0; i < _num_samplers; i++)
		_device->SetTexture(i, nullptr);
	for (unsigned int i = 1; i < _num_simultaneous_rendertargets; i++)
		_device->SetRenderTarget(i, nullptr);
	_device->SetDepthStencilSurface(nullptr);

	const D3DMATRIX identity_mat = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	const D3DMATRIX ortho_projection = {
		2.0f / draw_data->DisplaySize.x, 0.0f,   0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y,  0.0f, 0.0f,
		0.0f,                            0.0f,   0.5f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x + 1.0f) / draw_data->DisplaySize.x, // Bake half-pixel offset into projection matrix
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y + 1.0f) / draw_data->DisplaySize.y, 0.5f, 1.0f
	};

	_device->SetTransform(D3DTS_VIEW, &identity_mat);
	_device->SetTransform(D3DTS_WORLD, &identity_mat);
	_device->SetTransform(D3DTS_PROJECTION, &ortho_projection);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.y - draw_data->DisplayPos.y),
				static_cast<LONG>(cmd.ClipRect.z - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.w - draw_data->DisplayPos.y)
			};
			_device->SetScissorRect(&scissor_rect);

			_device->SetTexture(0, static_cast<const tex_data *>(cmd.TextureId)->texture.get());

			_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, cmd.VtxOffset + vtx_offset, 0, draw_list->VtxBuffer.Size, cmd.IdxOffset + idx_offset, cmd.ElemCount / 3);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}
#endif

#if RESHADE_DEPTH
void reshade::d3d9::runtime_d3d9::draw_depth_debug_menu()
{
	if (!ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (_has_high_network_activity)
	{
		ImGui::TextColored(ImColor(204, 204, 0), "High network activity discovered.\nAccess to depth buffers is disabled to prevent exploitation.");
		return;
	}

	assert(!_reset_buffer_detection);

	// Do NOT reset tracker within state block capture scope, since the state block may otherwise bind the replacement depth-stencil after it has been destroyed during that reset
	_reset_buffer_detection |= ImGui::Checkbox("Disable replacement with INTZ format", &_disable_intz);

	_reset_buffer_detection |= ImGui::Checkbox("Use aspect ratio heuristics", &_state_tracking.use_aspect_ratio_heuristics);
	_reset_buffer_detection |= ImGui::Checkbox("Copy depth buffer before clear operations", &_state_tracking.preserve_depth_buffers);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// Sort pointer list so that added/removed items do not change the UI much
	std::vector<std::pair<IDirect3DSurface9 *, state_tracking::depthstencil_info>> sorted_buffers;
	sorted_buffers.reserve(_state_tracking.depth_buffer_counters().size());
	for (const auto &[ds_surface, snapshot] : _state_tracking.depth_buffer_counters())
		sorted_buffers.push_back({ ds_surface.get(), snapshot });
	std::sort(sorted_buffers.begin(), sorted_buffers.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	for (const auto &[ds_surface, snapshot] : sorted_buffers)
	{
		char label[512] = "";
		sprintf_s(label, "%s0x%p", (ds_surface == _state_tracking.current_depth_surface() || ds_surface == _depth_surface ? "> " : "  "), ds_surface);

		D3DSURFACE_DESC desc;
		ds_surface->GetDesc(&desc);

		const bool msaa = desc.MultiSampleType != D3DMULTISAMPLE_NONE;
		if (msaa) // Disable widget for MSAA textures
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}

		if (bool value = (_depth_surface_override == ds_surface);
			ImGui::Checkbox(label, &value))
		{
			_depth_surface_override = value ? ds_surface : nullptr;
			_reset_buffer_detection = true;
		}

		ImGui::SameLine();
		ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
			desc.Width, desc.Height, snapshot.total_stats.drawcalls, snapshot.total_stats.vertices, (msaa ? " MSAA" : ""));

		if (_state_tracking.preserve_depth_buffers && ds_surface == _state_tracking.current_depth_surface())
		{
			for (UINT clear_index = 1; clear_index <= snapshot.clears.size(); ++clear_index)
			{
				sprintf_s(label, "%s  CLEAR %2u", (clear_index == _state_tracking.depthstencil_clear_index ? "> " : "  "), clear_index);

				if (bool value = (_state_tracking.depthstencil_clear_index == clear_index);
					ImGui::Checkbox(label, &value))
				{
					_state_tracking.depthstencil_clear_index = value ? clear_index : 0;
					_reset_buffer_detection = true;
				}

				ImGui::SameLine();
				ImGui::Text("%*s|           | %5u draw calls ==> %8u vertices |",
					sizeof(ds_surface) == 8 ? 8 : 0, "", // Add space to fill pointer length
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

	if (_reset_buffer_detection)
		runtime::save_config();
}

void reshade::d3d9::runtime_d3d9::update_depth_texture_bindings(com_ptr<IDirect3DSurface9> depth_surface)
{
	if (_has_high_network_activity)
		depth_surface.reset();

	if (depth_surface == _depth_surface)
		return;

	_depth_texture.reset();
	_depth_surface = std::move(depth_surface);
	_has_depth_texture = false;

	if (_depth_surface != nullptr)
	{
		if (HRESULT hr = _depth_surface->GetContainer(IID_PPV_ARGS(&_depth_texture)); FAILED(hr))
		{
			LOG(ERROR) << "Failed to retrieve texture from depth surface! HRESULT is " << hr << '.';
		}
		else
		{
			_has_depth_texture = true;
		}
	}

	// Update all references to the new texture
	for (const texture &tex : _textures)
	{
		if (tex.impl == nullptr || tex.semantic != "DEPTH")
			continue;
		const auto tex_impl = static_cast<tex_data *>(tex.impl);

		// Update references in technique list
		for (const technique &tech : _techniques)
		{
			const auto tech_impl = static_cast<technique_data *>(tech.impl);
			if (tech_impl == nullptr)
				continue;

			for (pass_data &pass_data : tech_impl->passes)
				// Replace all occurances of the old texture with the new one
				for (IDirect3DTexture9 *&sampler_tex : pass_data.sampler_textures)
					if (tex_impl->texture == sampler_tex)
						sampler_tex = _depth_texture.get();
		}

		tex_impl->texture = _depth_texture;
		tex_impl->surface = _depth_surface;
	}
}
#endif
