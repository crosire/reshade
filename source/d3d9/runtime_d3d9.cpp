/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "ini_file.hpp"
#include "runtime_d3d9.hpp"
#include "runtime_objects.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <d3dcompiler.h>

namespace reshade::d3d9
{
	struct d3d9_tex_data : base_object
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};
	struct d3d9_pass_data : base_object
	{
		com_ptr<IDirect3DStateBlock9> stateblock;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		IDirect3DSurface9 *render_targets[8] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
	};
	struct d3d9_technique_data : base_object
	{
		DWORD num_samplers = 0;
		DWORD sampler_states[16][12] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
		DWORD constant_register_count = 0;
		size_t uniform_storage_offset = 0;
	};
}

reshade::d3d9::runtime_d3d9::runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) :
	_device(device), _swapchain(swapchain),
	_app_state(device)
{
	assert(device != nullptr);
	assert(swapchain != nullptr);

	_device->GetDirect3D(&_d3d);
	assert(_d3d != nullptr);

	D3DCAPS9 caps = {};
	_device->GetDeviceCaps(&caps);
	D3DDEVICE_CREATION_PARAMETERS creation_params = {};
	_device->GetCreationParameters(&creation_params);
	D3DADAPTER_IDENTIFIER9 adapter_desc = {};
	_d3d->GetAdapterIdentifier(creation_params.AdapterOrdinal, 0, &adapter_desc);

	_vendor_id = adapter_desc.VendorId;
	_device_id = adapter_desc.DeviceId;
	_renderer_id = 0x9000;

	_num_samplers = caps.MaxSimultaneousTextures;
	_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, DWORD(8));
	_behavior_flags = creation_params.BehaviorFlags;

#if RESHADE_GUI && RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
	subscribe_to_ui("DX9", [this]() { draw_depth_debug_menu(); });
#endif
#if RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
	subscribe_to_load_config([this](const ini_file &config) {
		config.get("DX9_BUFFER_DETECTION", "DisableINTZ", _disable_intz);
		config.get("DX9_BUFFER_DETECTION", "PreserveDepthBuffer", _preserve_depth_buffers);
		config.get("DX9_BUFFER_DETECTION", "PreserveDepthBufferIndex", _depth_clear_index_override);
		config.get("DX9_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);

		if (_depth_clear_index_override == 0)
			// Zero is not a valid clear index, since it disables depth buffer preservation
			_depth_clear_index_override = std::numeric_limits<UINT>::max();
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("DX9_BUFFER_DETECTION", "DisableINTZ", _disable_intz);
		config.set("DX9_BUFFER_DETECTION", "PreserveDepthBuffer", _preserve_depth_buffers);
		config.set("DX9_BUFFER_DETECTION", "PreserveDepthBufferIndex", _depth_clear_index_override);
		config.set("DX9_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);
	});
#endif
}
reshade::d3d9::runtime_d3d9::~runtime_d3d9()
{
	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d9::runtime_d3d9::on_init(const D3DPRESENT_PARAMETERS &pp)
{
	RECT window_rect = {};
	GetClientRect(pp.hDeviceWindow, &window_rect);

	_width = pp.BackBufferWidth;
	_height = pp.BackBufferHeight;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_color_bit_depth = pp.BackBufferFormat == D3DFMT_A2B10G10R10 || pp.BackBufferFormat == D3DFMT_A2R10G10B10 ? 10 : 8;
	_backbuffer_format = pp.BackBufferFormat;

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

	// Create effect depth stencil surface
	if (FAILED(_device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_effect_depthstencil, nullptr)))
		return false;

	// Create vertex buffer which holds vertex indices
	// TODO: Need to increase this for when users sets the "VertexCount" pass state to something greater than 3
	if (FAILED(_device->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effect_triangle_buffer, nullptr)))
		return false;
	if (float *data; SUCCEEDED(_effect_triangle_buffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0)))
	{
		for (unsigned int i = 0; i < 3; i++)
			data[i] = static_cast<float>(i);
		_effect_triangle_buffer->Unlock();
	}
	else
		return false;

	const D3DVERTEXELEMENT9 declaration[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	if (FAILED(_device->CreateVertexDeclaration(declaration, &_effect_triangle_layout)))
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
	_depthstencil.reset();
	_depthstencil_texture.reset();

	_effect_depthstencil.reset();
	_effect_triangle_buffer.reset();
	_effect_triangle_layout.reset();

#if RESHADE_GUI
	_imgui_state.reset();
	_imgui_index_buffer.reset();
	_imgui_vertex_buffer.reset();
	_imgui_index_buffer_size = 0;
	_imgui_vertex_buffer_size = 0;
#endif

#if RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
	_depthstencil_override = nullptr;
#endif
}

void reshade::d3d9::runtime_d3d9::on_present(buffer_detection &tracker)
{
	if (!_is_initialized || FAILED(_device->BeginScene()))
		return;

	_vertices = tracker.total_vertices();
	_drawcalls = tracker.total_drawcalls();
	_current_tracker = &tracker;

#if RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
	tracker.disable_intz = _disable_intz;

	assert(_depth_clear_index_override != 0);
	update_depthstencil_texture(_has_high_network_activity ? nullptr :
		tracker.find_best_depth_surface(_filter_aspect_ratio ? _width : 0, _height, _depthstencil_override, _preserve_depth_buffers ? _depth_clear_index_override : 0));
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

	_device->EndScene();
}

bool reshade::d3d9::runtime_d3d9::capture_screenshot(uint8_t *buffer) const
{
	// Create a surface in system memory, copy back buffer data into it and lock it for reading
	com_ptr<IDirect3DSurface9> intermediate;
	if (FAILED(_device->CreateOffscreenPlainSurface(_width, _height, _backbuffer_format, D3DPOOL_SYSTEMMEM, &intermediate, nullptr)))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture!";
		return false;
	}

	_device->GetRenderTargetData(_backbuffer_resolved.get(), intermediate.get());

	D3DLOCKED_RECT mapped;
	if (FAILED(intermediate->LockRect(&mapped, nullptr, D3DLOCK_READONLY)))
		return false;
	auto mapped_data = static_cast<const uint8_t *>(mapped.pBits);

	for (uint32_t y = 0, pitch = _width * 4; y < _height; y++, buffer += pitch, mapped_data += mapped.Pitch)
	{
		if (_backbuffer_format == D3DFMT_A2R10G10B10 ||
			_backbuffer_format == D3DFMT_A2B10G10R10)
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
			memcpy(buffer, mapped_data, pitch);

			for (uint32_t x = 0; x < pitch; x += 4)
			{
				buffer[x + 3] = 0xFF; // Clear alpha channel
				if (_backbuffer_format == D3DFMT_A8R8G8B8 || _backbuffer_format == D3DFMT_X8R8G8B8)
					std::swap(buffer[x + 0], buffer[x + 2]); // Format is BGRA, but output should be RGBA, so flip channels
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
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\"). Make sure you have the DirectX end-user runtime (June 2010) installed or a newer version of the library in the application directory.";
		return false;
	}

	effect &effect = _loaded_effects[index];

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	// Add specialization constant defines to source code
	effect.preamble += "#define COLOR_PIXEL_SIZE 1.0 / " + std::to_string(_width) + ", 1.0 / " + std::to_string(_height) + "\n"
		"#define DEPTH_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
		"#define SV_TARGET_PIXEL_SIZE COLOR_PIXEL_SIZE\n"
		"#define SV_DEPTH_PIXEL_SIZE COLOR_PIXEL_SIZE\n";

	const std::string hlsl_vs = effect.preamble + effect.module.hlsl;
	const std::string hlsl_ps = effect.preamble + "#define POSITION VPOS\n" + effect.module.hlsl;

	std::unordered_map<std::string, com_ptr<IUnknown>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (auto &entry_point : effect.module.entry_points)
	{
		entry_point.assembly.clear();

		com_ptr<ID3DBlob> compiled, d3d_errors;
		const std::string &hlsl = entry_point.is_pixel_shader ? hlsl_ps : hlsl_vs;

		HRESULT hr = D3DCompile(
			hlsl.c_str(), hlsl.size(),
			nullptr, nullptr, nullptr,
			entry_point.name.c_str(),
			entry_point.is_pixel_shader ? "ps_3_0" : "vs_3_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(compiled->GetBufferPointer(), compiled->GetBufferSize(), 0, nullptr, &d3d_disassembled)))
			entry_point.assembly = std::string(static_cast<const char *>(d3d_disassembled->GetBufferPointer()));

		// Create runtime shader objects from the compiled DX byte code
		if (entry_point.is_pixel_shader)
			hr = _device->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), reinterpret_cast<IDirect3DPixelShader9 **>(&entry_points[entry_point.name]));
		else
			hr = _device->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), reinterpret_cast<IDirect3DVertexShader9 **>(&entry_points[entry_point.name]));

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create shader for entry point '" << entry_point.name << "'. "
				"HRESULT is " << hr << '.';
			return false;
		}
	}

	d3d9_technique_data technique_init;
	technique_init.constant_register_count = static_cast<DWORD>((effect.storage_size + 15) / 16);
	technique_init.uniform_storage_offset = effect.storage_offset;

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding > ARRAYSIZE(technique_init.sampler_states))
			return false;

		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		if (existing_texture == _textures.end())
			return false;

		// Since textures with auto-generated mipmap levels do not have a mipmap maximum, limit the bias here so this is not as obvious
		assert(existing_texture->levels > 0);
		const float lod_bias = std::min(existing_texture->levels - 1.0f, info.lod_bias);

		technique_init.num_samplers = std::max(technique_init.num_samplers, DWORD(info.binding + 1));

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

		technique_init.sampler_textures[info.binding] = existing_texture->impl->as<d3d9_tex_data>()->texture.get();
	}

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		// Copy construct new technique implementation instead of move because effect may contain multiple techniques
		technique.impl = std::make_unique<d3d9_technique_data>(technique_init);
		const auto technique_data = technique.impl->as<d3d9_technique_data>();

		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			technique.passes_data.push_back(std::make_unique<d3d9_pass_data>());

			auto &pass = *technique.passes_data.back()->as<d3d9_pass_data>();
			const auto &pass_info = technique.passes[pass_index];

			entry_points.at(pass_info.ps_entry_point)->QueryInterface(&pass.pixel_shader);
			entry_points.at(pass_info.vs_entry_point)->QueryInterface(&pass.vertex_shader);

			pass.render_targets[0] = _backbuffer_resolved.get();

			for (size_t k = 0; k < ARRAYSIZE(pass.sampler_textures); ++k)
				pass.sampler_textures[k] = technique_data->sampler_textures[k];

			HRESULT hr = _device->BeginStateBlock();

			if (SUCCEEDED(hr))
			{
				_device->SetVertexShader(pass.vertex_shader.get());
				_device->SetPixelShader(pass.pixel_shader.get());

				const auto literal_to_blend_func = [](unsigned int value) {
					switch (value)
					{
					case 0:
						return D3DBLEND_ZERO;
					default:
					case 1:
						return D3DBLEND_ONE;
					case 2:
						return D3DBLEND_SRCCOLOR;
					case 4:
						return D3DBLEND_INVSRCCOLOR;
					case 3:
						return D3DBLEND_SRCALPHA;
					case 5:
						return D3DBLEND_INVSRCALPHA;
					case 6:
						return D3DBLEND_DESTALPHA;
					case 7:
						return D3DBLEND_INVDESTALPHA;
					case 8:
						return D3DBLEND_DESTCOLOR;
					case 9:
						return D3DBLEND_INVDESTCOLOR;
					}
				};
				const auto literal_to_stencil_op = [](unsigned int value) {
					switch (value)
					{
					default:
					case 1:
						return D3DSTENCILOP_KEEP;
					case 0:
						return D3DSTENCILOP_ZERO;
					case 3:
						return D3DSTENCILOP_REPLACE;
					case 4:
						return D3DSTENCILOP_INCRSAT;
					case 5:
						return D3DSTENCILOP_DECRSAT;
					case 6:
						return D3DSTENCILOP_INVERT;
					case 7:
						return D3DSTENCILOP_INCR;
					case 8:
						return D3DSTENCILOP_DECR;
					}
				};

				_device->SetRenderState(D3DRS_ZENABLE, false);
				_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
				_device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
				_device->SetRenderState(D3DRS_ZWRITEENABLE, true);
				_device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
				_device->SetRenderState(D3DRS_LASTPIXEL, true);
				_device->SetRenderState(D3DRS_SRCBLEND, literal_to_blend_func(pass_info.src_blend));
				_device->SetRenderState(D3DRS_DESTBLEND, literal_to_blend_func(pass_info.dest_blend));
				_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				_device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
				_device->SetRenderState(D3DRS_ALPHAREF, 0);
				_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
				_device->SetRenderState(D3DRS_DITHERENABLE, false);
				_device->SetRenderState(D3DRS_ALPHABLENDENABLE, pass_info.blend_enable);
				_device->SetRenderState(D3DRS_FOGENABLE, false);
				_device->SetRenderState(D3DRS_SPECULARENABLE, false);
				_device->SetRenderState(D3DRS_STENCILENABLE, pass_info.stencil_enable);
				_device->SetRenderState(D3DRS_STENCILFAIL, literal_to_stencil_op(pass_info.stencil_op_fail));
				_device->SetRenderState(D3DRS_STENCILZFAIL, literal_to_stencil_op(pass_info.stencil_op_depth_fail));
				_device->SetRenderState(D3DRS_STENCILPASS, literal_to_stencil_op(pass_info.stencil_op_pass));
				_device->SetRenderState(D3DRS_STENCILFUNC, static_cast<D3DCMPFUNC>(pass_info.stencil_comparison_func));
				_device->SetRenderState(D3DRS_STENCILREF, pass_info.stencil_reference_value);
				_device->SetRenderState(D3DRS_STENCILMASK, pass_info.stencil_read_mask);
				_device->SetRenderState(D3DRS_STENCILWRITEMASK, pass_info.stencil_write_mask);
				_device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
				_device->SetRenderState(D3DRS_CLIPPING, false);
				_device->SetRenderState(D3DRS_LIGHTING, false);
				_device->SetRenderState(D3DRS_COLORVERTEX, false);
				_device->SetRenderState(D3DRS_LOCALVIEWER, true);
				_device->SetRenderState(D3DRS_NORMALIZENORMALS, false);
				_device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				_device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
				_device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				_device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				_device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
				_device->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DRS_CLIPPLANEENABLE);
				// D3DRS_POINTSIZE
				// D3DRS_POINTSPRITEENABLE
				// D3DRS_POINTSCALEENABLE
				// D3DRS_MULTISAMPLEANTIALIAS
				// D3DRS_MULTISAMPLEMASK
				_device->SetRenderState(D3DRS_COLORWRITEENABLE, pass_info.color_write_mask);
				_device->SetRenderState(D3DRS_TWEENFACTOR, 0);
				_device->SetRenderState(D3DRS_BLENDOP, static_cast<D3DBLENDOP>(pass_info.blend_op));
				_device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
				_device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
				_device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, false);
				_device->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, false);
				_device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, false);
				_device->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
				_device->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
				_device->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
				_device->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
				_device->SetRenderState(D3DRS_COLORWRITEENABLE1, pass_info.color_write_mask); // See https://docs.microsoft.com/en-us/windows/win32/direct3d9/multiple-render-targets
				_device->SetRenderState(D3DRS_COLORWRITEENABLE2, pass_info.color_write_mask);
				_device->SetRenderState(D3DRS_COLORWRITEENABLE3, pass_info.color_write_mask);
				_device->SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
				_device->SetRenderState(D3DRS_SRGBWRITEENABLE, pass_info.srgb_write_enable);
				_device->SetRenderState(D3DRS_DEPTHBIAS, 0);
				_device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
				_device->SetRenderState(D3DRS_SRCBLENDALPHA, literal_to_blend_func(pass_info.src_blend_alpha));
				_device->SetRenderState(D3DRS_DESTBLENDALPHA, literal_to_blend_func(pass_info.dest_blend_alpha));
				_device->SetRenderState(D3DRS_BLENDOPALPHA, static_cast<D3DBLENDOP>(pass_info.blend_op_alpha));

				hr = _device->EndStateBlock(&pass.stateblock);
			}

			if (FAILED(hr))
			{
				LOG(ERROR) << "Failed to create state block for pass " << pass_index << " in technique '" << technique.name << "'. "
					"HRESULT is " << hr << '.';
				return false;
			}

			for (unsigned int k = 0; k < 8; ++k)
			{
				if (pass_info.render_target_names[k].empty())
					continue; // Skip unbound render targets

				if (k > _num_simultaneous_rendertargets)
				{
					LOG(WARN) << "Device only supports " << _num_simultaneous_rendertargets << " simultaneous render targets, but pass " << pass_index << " in technique '" << technique.name << "' uses more, which are ignored";
					break;
				}

				const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				});
				if (render_target_texture == _textures.end())
					return false;

				const auto texture_impl = render_target_texture->impl->as<d3d9_tex_data>();
				assert(texture_impl != nullptr);

				// Unset textures that are used as render target
				for (DWORD s = 0; s < technique_data->num_samplers; ++s)
					if (pass.sampler_textures[s] == texture_impl->texture)
						pass.sampler_textures[s] = nullptr;

				pass.render_targets[k] = texture_impl->surface.get();
			}
		}
	}

	return true;
}

bool reshade::d3d9::runtime_d3d9::init_texture(texture &texture)
{
	texture.impl = std::make_unique<d3d9_tex_data>();
	const auto texture_data = texture.impl->as<d3d9_tex_data>();

	switch (texture.impl_reference)
	{
	case texture_reference::back_buffer:
		texture_data->texture = _backbuffer_texture;
		texture_data->surface = _backbuffer_texture_surface;
		return true;
	case texture_reference::depth_buffer:
		texture_data->texture = _depthstencil_texture;
		texture_data->surface = _depthstencil;
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
		format = D3DFMT_A8R8G8B8;
		break;
	case reshadefx::texture_format::r16f:
		format = D3DFMT_R16F;
		break;
	case reshadefx::texture_format::r32f:
		format = D3DFMT_R32F;
		break;
	case reshadefx::texture_format::rg8:
		format = D3DFMT_A8R8G8B8;
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
			LOG(WARN) << "Auto-generated mipmap levels are not supported for the format of texture '" << texture.unique_name << "'.";
		}
	}

	// Make texture a render target if format allows it
	HRESULT hr = _d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format);
	if (SUCCEEDED(hr))
		usage |= D3DUSAGE_RENDERTARGET;

	hr = _device->CreateTexture(texture.width, texture.height, levels, usage, format, D3DPOOL_DEFAULT, &texture_data->texture, nullptr);
	if (FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "' ("
			"Width = " << texture.width << ", "
			"Height = " << texture.height << ", "
			"Levels = " << levels << ", "
			"Usage = " << usage << ", "
			"Format = " << format << ")! "
			"HRESULT is " << hr << '.';
		return false;
	}

	hr = texture_data->texture->GetSurfaceLevel(0, &texture_data->surface);
	assert(SUCCEEDED(hr));

	return true;
}
void reshade::d3d9::runtime_d3d9::upload_texture(texture &texture, const uint8_t *pixels)
{
	assert(texture.impl_reference == texture_reference::none && pixels != nullptr);

	const auto texture_impl = texture.impl->as<d3d9_tex_data>();
	assert(texture_impl != nullptr);

	D3DSURFACE_DESC desc; texture_impl->texture->GetLevelDesc(0, &desc); // Get D3D texture format
	com_ptr<IDirect3DTexture9> intermediate;
	if (FAILED(_device->CreateTexture(texture.width, texture.height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &intermediate, nullptr)))
	{
		LOG(ERROR) << "Failed to create system memory texture for texture updating!";
		return;
	}

	D3DLOCKED_RECT mapped;
	if (FAILED(intermediate->LockRect(0, &mapped, nullptr, 0)))
		return;
	auto mapped_data = static_cast<uint8_t *>(mapped.pBits);

	switch (texture.format)
	{
	case reshadefx::texture_format::r8: // These are actually D3DFMT_A8R8G8B8, see 'init_texture'
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
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << '!';
		break;
	}

	intermediate->UnlockRect(0);

	if (HRESULT hr = _device->UpdateTexture(intermediate.get(), texture_impl->texture.get()); FAILED(hr))
	{
		LOG(ERROR) << "Failed to update texture from system memory texture! HRESULT is " << hr << '.';
		return;
	}
}

void reshade::d3d9::runtime_d3d9::render_technique(technique &technique)
{
	auto &technique_data = *technique.impl->as<d3d9_technique_data>();

	bool is_default_depthstencil_cleared = false;

	// Setup vertex input
	_device->SetStreamSource(0, _effect_triangle_buffer.get(), 0, sizeof(float));
	_device->SetVertexDeclaration(_effect_triangle_layout.get());

	// Setup shader constants
	if (technique_data.constant_register_count > 0)
	{
		const auto uniform_storage_data = reinterpret_cast<const float *>(_uniform_data_storage.data() + technique_data.uniform_storage_offset);
		_device->SetPixelShaderConstantF(0, uniform_storage_data, technique_data.constant_register_count);
		_device->SetVertexShaderConstantF(0, uniform_storage_data, technique_data.constant_register_count);
	}

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<d3d9_pass_data>();

		// Setup states
		pass_data.stateblock->Apply();

		// Save back buffer of previous pass
		_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer_texture_surface.get(), nullptr, D3DTEXF_NONE);

		// Setup shader resources
		for (DWORD s = 0; s < technique_data.num_samplers; s++)
		{
			_device->SetTexture(s, pass_data.sampler_textures[s]);

			// Need to bind textures to vertex shader samplers too
			// See https://docs.microsoft.com/de-de/windows/win32/direct3d9/vertex-textures-in-vs-3-0
			if (s < 4)
				_device->SetTexture(D3DVERTEXTEXTURESAMPLER0 + s, pass_data.sampler_textures[s]);

			for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state++)
			{
				_device->SetSamplerState(s, static_cast<D3DSAMPLERSTATETYPE>(state), technique_data.sampler_states[s][state]);

				if (s < 4) // vs_3_0 supports up to four samplers in vertex shaders
					_device->SetSamplerState(D3DVERTEXTEXTURESAMPLER0 + s, static_cast<D3DSAMPLERSTATETYPE>(state), technique_data.sampler_states[s][state]);
			}
		}

		// Setup render targets
		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
			_device->SetRenderTarget(target, pass_data.render_targets[target]);

		D3DVIEWPORT9 viewport;
		_device->GetViewport(&viewport);

		const float texelsize[4] = { -1.0f / viewport.Width, 1.0f / viewport.Height };
		_device->SetVertexShaderConstantF(255, texelsize, 1);

		const bool is_viewport_sized = viewport.Width == _width && viewport.Height == _height;

		_device->SetDepthStencilSurface(is_viewport_sized ? _effect_depthstencil.get() : nullptr);

		if (is_viewport_sized && !is_default_depthstencil_cleared)
		{
			is_default_depthstencil_cleared = true;

			_device->Clear(0, nullptr, (pass_info.clear_render_targets ? D3DCLEAR_TARGET : 0) | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
		}
		else if (pass_info.clear_render_targets)
		{
			_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
		}

		// Draw triangle
		if (pass_info.num_vertices % 3)
			_device->DrawPrimitive(D3DPT_POINTLIST, 0, pass_info.num_vertices);
		else
			_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, pass_info.num_vertices / 3);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		// Update shader resources
		for (const auto target : pass_data.render_targets)
		{
			if (target == nullptr || target == _backbuffer_resolved)
				continue;

			com_ptr<IDirect3DBaseTexture9> texture;

			if (SUCCEEDED(target->GetContainer(IID_PPV_ARGS(&texture))) && texture->GetLevelCount() > 1)
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

		hr = _device->EndStateBlock(&_imgui_state);
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
	if (_imgui_index_buffer_size < draw_data->TotalIdxCount)
	{
		_imgui_index_buffer.reset();
		_imgui_index_buffer_size = draw_data->TotalIdxCount + 10000;

		if (FAILED(_device->CreateIndexBuffer(_imgui_index_buffer_size * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &_imgui_index_buffer, nullptr)))
			return;
	}
	if (_imgui_vertex_buffer_size < draw_data->TotalVtxCount)
	{
		_imgui_vertex_buffer.reset();
		_imgui_vertex_buffer_size = draw_data->TotalVtxCount + 5000;

		if (FAILED(_device->CreateVertexBuffer(_imgui_vertex_buffer_size * sizeof(ImDrawVert9), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, &_imgui_vertex_buffer, nullptr)))
			return;
	}

	ImDrawVert9 *vtx_dst; ImDrawIdx *idx_dst;
	if (FAILED(_imgui_index_buffer->Lock(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx), reinterpret_cast<void **>(&idx_dst), D3DLOCK_DISCARD)) ||
		FAILED(_imgui_vertex_buffer->Lock(0, draw_data->TotalVtxCount * sizeof(ImDrawVert9), reinterpret_cast<void **>(&vtx_dst), D3DLOCK_DISCARD)))
		return;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawVert &vtx : draw_list->VtxBuffer)
		{
			vtx_dst->x = vtx.pos.x;
			vtx_dst->y = vtx.pos.y;
			vtx_dst->z = 0.0f;

			// RGBA --> ARGB for Direct3D 9
			vtx_dst->col = (vtx.col & 0xFF00FF00) | ((vtx.col & 0xFF0000) >> 16) | ((vtx.col & 0xFF) << 16);

			vtx_dst->u = vtx.uv.x;
			vtx_dst->v = vtx.uv.y;

			++vtx_dst; // Next vertex
		}

		memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.size() * sizeof(ImDrawIdx));
		idx_dst += draw_list->IdxBuffer.size();
	}

	_imgui_index_buffer->Unlock();
	_imgui_vertex_buffer->Unlock();

	// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
	_imgui_state->Apply();
	_device->SetIndices(_imgui_index_buffer.get());
	_device->SetStreamSource(0, _imgui_vertex_buffer.get(), 0, sizeof(ImDrawVert9));
	for (unsigned int i = 0; i < _num_samplers; i++)
		_device->SetTexture(i, nullptr);
	_device->SetRenderTarget(0, _backbuffer_resolved.get());
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

			_device->SetTexture(0, static_cast<const d3d9_tex_data *>(cmd.TextureId)->texture.get());

			_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vtx_offset, 0, draw_list->VtxBuffer.size(), idx_offset, cmd.ElemCount / 3);

			idx_offset += cmd.ElemCount;
		}

		vtx_offset += draw_list->VtxBuffer.size();
	}
}
#endif

#if RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
void reshade::d3d9::runtime_d3d9::draw_depth_debug_menu()
{
	if (ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		assert(_current_tracker != nullptr);

		bool modified = false;
		modified |= ImGui::Checkbox("Disable replacement with INTZ format", &_disable_intz);

		modified |= ImGui::Checkbox("Use aspect ratio heuristics", &_filter_aspect_ratio);
		modified |= ImGui::Checkbox("Copy depth buffers before clear operation", &_preserve_depth_buffers);

		if (modified) // Detection settings have changed, reset heuristic
			_current_tracker->reset(true);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		LOG(INFO) << "------";

		for (const auto &[ds_surface, snapshot] : _current_tracker->depth_buffer_counters())
		{
			char label[512] = "";
			sprintf_s(label, "%s0x%p", (ds_surface == _current_tracker->current_depth_surface() ? "> " : "  "), ds_surface.get());

			D3DSURFACE_DESC desc;
			ds_surface->GetDesc(&desc);

			const bool msaa = desc.MultiSampleType != D3DMULTISAMPLE_NONE;
			if (msaa) // Disable widget for MSAA textures
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
			}

			if (bool value = (_depthstencil_override == ds_surface);
				ImGui::Checkbox(label, &value))
				_depthstencil_override = value ? ds_surface.get() : nullptr;

			ImGui::SameLine();
			ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
				desc.Width, desc.Height, snapshot.stats.drawcalls, snapshot.stats.vertices, (msaa ? " MSAA" : ""));

			if (_preserve_depth_buffers && ds_surface == _current_tracker->current_depth_surface())
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
						sizeof(ds_surface) - 4, "", // Add space to fill pointer length
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

void reshade::d3d9::runtime_d3d9::update_depthstencil_texture(com_ptr<IDirect3DSurface9> depthstencil)
{
	if (depthstencil == _depthstencil)
		return;

	_depthstencil = std::move(depthstencil);
	_depthstencil_texture.reset();

	if (_depthstencil != nullptr)
	{
		if (HRESULT hr = _depthstencil->GetContainer(IID_PPV_ARGS(&_depthstencil_texture)); FAILED(hr))
		{
			LOG(ERROR) << "Failed to retrieve texture from depth surface! HRESULT is " << hr << '.';
			return;
		}
	}

	// Update all references to the new texture
	for (auto &tex : _textures)
	{
		if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
		{
			const auto texture_impl = tex.impl->as<d3d9_tex_data>();
			assert(texture_impl != nullptr);

			// Update references in technique list
			for (const auto &technique : _techniques)
				for (const auto &pass : technique.passes_data)
					for (auto &sampler_tex : pass->as<d3d9_pass_data>()->sampler_textures)
						if (sampler_tex == texture_impl->texture)
							sampler_tex = _depthstencil_texture.get();

			texture_impl->surface = _depthstencil;
			texture_impl->texture = _depthstencil_texture;
		}
	}
}
#endif
