/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_d3d9.hpp"
#include "runtime_d3d9_objects.hpp"
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
void reshade::d3d9::runtime_impl::on_reset()
{
	runtime::on_reset();

	_app_state.release_state_block();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_texture.reset();
	_backbuffer_texture_surface.reset();

	_effect_stencil.reset();
	_max_effect_vertices = 0;
	_effect_vertex_buffer.reset();
	_effect_vertex_layout.reset();

	// Need to release all resources on a D3D9 device reset
	_texture_semantic_bindings.clear();

#if RESHADE_GUI
	_imgui.state.reset();
	_imgui.indices.reset();
	_imgui.vertices.reset();
	_imgui.num_indices = 0;
	_imgui.num_vertices = 0;
#endif
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

bool reshade::d3d9::runtime_impl::capture_screenshot(uint8_t *buffer) const
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

bool reshade::d3d9::runtime_impl::init_effect(size_t index)
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
			effect.errors += "Compute shaders are not supported in D3D9.";
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
		technique_init.sampler_states[info.binding][D3DSAMP_MAGFILTER] = 1 + ((static_cast<DWORD>(info.filter) & 0x0C) >> 2);
		technique_init.sampler_states[info.binding][D3DSAMP_MINFILTER] = 1 + ((static_cast<DWORD>(info.filter) & 0x30) >> 4);
		technique_init.sampler_states[info.binding][D3DSAMP_MIPFILTER] = 1 + ((static_cast<DWORD>(info.filter) & 0x03));
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
				if (k > _device_impl->_caps.NumSimultaneousRTs)
				{
					LOG(WARN) << "Device only supports " << _device_impl->_caps.NumSimultaneousRTs << " simultaneous render targets, but pass " << pass_index << " in technique '" << technique.name << "' uses more, which are ignored.";
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
	if (max_vertices > _max_effect_vertices)
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

		_max_effect_vertices = max_vertices;
	}

	return true;
}
void reshade::d3d9::runtime_impl::unload_effect(size_t index)
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
void reshade::d3d9::runtime_impl::unload_effects()
{
	for (technique &tech : _techniques)
	{
		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effects();
}

bool reshade::d3d9::runtime_impl::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	if (texture.semantic == "COLOR")
	{
		impl->texture = _backbuffer_texture;
		impl->surface = _backbuffer_texture_surface;
		return true;
	}
	else if (!texture.semantic.empty())
	{
		if (const auto it = _texture_semantic_bindings.find(texture.semantic);
			it != _texture_semantic_bindings.end())
			impl->texture = it->second;
		return true;
	}

	UINT levels = texture.levels;
	DWORD usage = 0;
	D3DFORMAT format = D3DFMT_UNKNOWN;

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
		if (_device_impl->_d3d->CheckDeviceFormat(_device_impl->_cp.AdapterOrdinal, _device_impl->_cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, format) == D3D_OK)
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
		if (_device_impl->_d3d->CheckDeviceFormat(_device_impl->_cp.AdapterOrdinal, _device_impl->_cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format) == D3D_OK)
		{
			usage |= D3DUSAGE_RENDERTARGET;
		}
		else
		{
			LOG(ERROR) << "Render target usage is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'.";
			return false;
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
void reshade::d3d9::runtime_impl::upload_texture(const texture &texture, const uint8_t *pixels)
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
void reshade::d3d9::runtime_impl::destroy_texture(texture &texture)
{
	delete static_cast<tex_data *>(texture.impl);
	texture.impl = nullptr;
}

void reshade::d3d9::runtime_impl::render_technique(technique &technique)
{
	const auto impl = static_cast<technique_data *>(technique.impl);

#ifndef NDEBUG
	std::wstring technique_name;
	technique_name.reserve(technique.name.size());
	utf8::unchecked::utf8to16(technique.name.begin(), technique.name.end(), std::back_inserter(technique_name));
	D3DPERF_BeginEvent(D3DCOLOR_COLORVALUE(0.8f, 0.0f, 0.8f, 1.0f), technique_name.c_str());
#endif

	invoke_addon_event<addon_event::reshade_before_effects>(this, _device_impl);

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

#ifndef NDEBUG
		std::wstring pass_name;
		pass_name.reserve(pass_info.name.size());
		if (pass_info.name.empty())
			pass_name = L"Pass " + std::to_wstring(pass_index);
		else
			utf8::unchecked::utf8to16(pass_info.name.begin(), pass_info.name.end(), std::back_inserter(pass_name));
		D3DPERF_BeginEvent(D3DCOLOR_COLORVALUE(0.8f, 0.8f, 0.8f, 1.0f), pass_name.c_str());
#endif

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
		for (DWORD target = 0; target < _device_impl->_caps.NumSimultaneousRTs; target++)
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

#ifndef NDEBUG
		D3DPERF_EndEvent();
#endif
	}

	invoke_addon_event<addon_event::reshade_after_effects>(this, _device_impl);

#ifndef NDEBUG
	D3DPERF_EndEvent();
#endif
}

void reshade::d3d9::runtime_impl::update_texture_bindings(const char *semantic, api::resource_view_handle srv)
{
	const auto new_texture = reinterpret_cast<IDirect3DTexture9 *>(srv.handle);
	assert(new_texture == nullptr || new_texture->GetType() == D3DRTYPE_TEXTURE);

	_texture_semantic_bindings[semantic] = new_texture;

	// Update all references to the new texture
	for (const texture &tex : _textures)
	{
		if (tex.impl == nullptr || tex.semantic != semantic)
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
						sampler_tex = new_texture;
		}

		tex_impl->texture = new_texture;
		tex_impl->surface = nullptr;
	}
}
