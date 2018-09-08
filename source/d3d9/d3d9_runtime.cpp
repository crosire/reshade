/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d9_runtime.hpp"
#include "d3d9_effect_compiler.hpp"
#include "effect_lexer.hpp"
#include "input.hpp"
#include <imgui.h>
#include <algorithm>

const auto D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
const auto D3DFMT_DF16 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '1', '6'));
const auto D3DFMT_DF24 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '2', '4'));

namespace reshade::d3d9
{
	d3d9_runtime::d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) :
		runtime(0x9300), _device(device), _swapchain(swapchain)
	{
		assert(device != nullptr);
		assert(swapchain != nullptr);

		_device->GetDirect3D(&_d3d);

		assert(_d3d != nullptr);

		D3DCAPS9 caps;
		D3DADAPTER_IDENTIFIER9 adapter_desc;
		D3DDEVICE_CREATION_PARAMETERS creation_params;

		_device->GetDeviceCaps(&caps);
		_device->GetCreationParameters(&creation_params);
		_d3d->GetAdapterIdentifier(creation_params.AdapterOrdinal, 0, &adapter_desc);

		_vendor_id = adapter_desc.VendorId;
		_device_id = adapter_desc.DeviceId;
		_behavior_flags = creation_params.BehaviorFlags;
		_num_samplers = caps.MaxSimultaneousTextures;
		_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, DWORD(8));

		subscribe_to_menu("DX9", [this]() { draw_debug_menu(); });
		subscribe_to_load_config([this](const ini_file& config) {
			config.get("DX9_BUFFER_DETECTION", "DisableINTZ", _disable_intz);
		});
		subscribe_to_save_config([this](ini_file& config) {
			config.set("DX9_BUFFER_DETECTION", "DisableINTZ", _disable_intz);
		});
	}

	bool d3d9_runtime::init_backbuffer_texture()
	{
		// Get back buffer surface
		HRESULT hr = _swapchain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer);

		assert(SUCCEEDED(hr));

		if (_is_multisampling_enabled ||
			(_backbuffer_format == D3DFMT_X8R8G8B8 || _backbuffer_format == D3DFMT_X8B8G8R8))
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

			hr = _device->CreateRenderTarget(_width, _height, _backbuffer_format, D3DMULTISAMPLE_NONE, 0, FALSE, &_backbuffer_resolved, nullptr);

			if (FAILED(hr))
			{
				LOG(ERROR) << "Failed to create back buffer resolve texture! HRESULT is '" << std::hex << hr << std::dec << "'.";
				return false;
			}
		}
		else
		{
			_backbuffer_resolved = _backbuffer;
		}

		// Create back buffer shader texture
		hr = _device->CreateTexture(_width, _height, 1, D3DUSAGE_RENDERTARGET, _backbuffer_format, D3DPOOL_DEFAULT, &_backbuffer_texture, nullptr);

		if (SUCCEEDED(hr))
		{
			_backbuffer_texture->GetSurfaceLevel(0, &_backbuffer_texture_surface);
		}
		else
		{
			LOG(ERROR) << "Failed to create back buffer texture! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		return true;
	}
	bool d3d9_runtime::init_default_depth_stencil()
	{
		// Create default depth stencil surface
		HRESULT hr = _device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_default_depthstencil, nullptr);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create default depth stencil! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		return true;
	}
	bool d3d9_runtime::init_fx_resources()
	{
		HRESULT hr = _device->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effect_triangle_buffer, nullptr);

		if (SUCCEEDED(hr))
		{
			float *data = nullptr;

			hr = _effect_triangle_buffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

			assert(SUCCEEDED(hr));

			for (unsigned int i = 0; i < 3; i++)
			{
				data[i] = static_cast<float>(i);
			}

			_effect_triangle_buffer->Unlock();
		}
		else
		{
			LOG(ERROR) << "Failed to create effect vertex buffer! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		const D3DVERTEXELEMENT9 declaration[] = {
			{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
			D3DDECL_END()
		};

		hr = _device->CreateVertexDeclaration(declaration, &_effect_triangle_layout);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create effect vertex declaration! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		return true;
	}
	bool d3d9_runtime::init_imgui_font_atlas()
	{
		int width, height, bits_per_pixel;
		unsigned char *pixels;

		ImGui::SetCurrentContext(_imgui_context);
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bits_per_pixel);

		D3DLOCKED_RECT font_atlas_rect;
		com_ptr<IDirect3DTexture9> font_atlas;

		HRESULT hr = _device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &font_atlas, nullptr);

		if (FAILED(hr) || FAILED(font_atlas->LockRect(0, &font_atlas_rect, nullptr, 0)))
		{
			LOG(ERROR) << "Failed to create font atlas texture! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		for (int y = 0; y < height; y++)
		{
			std::memcpy(static_cast<BYTE *>(font_atlas_rect.pBits) + font_atlas_rect.Pitch * y, pixels + (width * bits_per_pixel) * y, width * bits_per_pixel);
		}

		font_atlas->UnlockRect(0);

		d3d9_tex_data obj = { };
		obj.texture = font_atlas;

		_imgui_font_atlas_texture = std::make_unique<d3d9_tex_data>(obj);

		if (hr = _device->BeginStateBlock(); SUCCEEDED(hr))
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
			_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

			const D3DMATRIX identity_mat = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			};
			const D3DMATRIX ortho_projection = {
				2.0f / _width, 0.0f, 0.0f, 0.0f,
				0.0f, -2.0f / _height, 0.0f, 0.0f,
				0.0f, 0.0f, 0.5f, 0.0f,
				-(_width + 1.0f) / _width, (_height + 1.0f) / _height, 0.5f, 1.0f
			};

			_device->SetTransform(D3DTS_WORLD, &identity_mat);
			_device->SetTransform(D3DTS_VIEW, &identity_mat);
			_device->SetTransform(D3DTS_PROJECTION, &ortho_projection);

			hr = _device->EndStateBlock(&_imgui_state);
		}

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create state block! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		return true;
	}

	bool d3d9_runtime::on_init(const D3DPRESENT_PARAMETERS &pp)
	{
		_width = pp.BackBufferWidth;
		_height = pp.BackBufferHeight;
		_backbuffer_format = pp.BackBufferFormat;
		_is_multisampling_enabled = pp.MultiSampleType != D3DMULTISAMPLE_NONE;
		_input = input::register_window(pp.hDeviceWindow);

		if (FAILED(_device->CreateStateBlock(D3DSBT_ALL, &_app_state)))
		{
			return false;
		}

		if (!init_backbuffer_texture() ||
			!init_default_depth_stencil() ||
			!init_fx_resources() ||
			!init_imgui_font_atlas())
		{
			return false;
		}

		return runtime::on_init();
	}
	void d3d9_runtime::on_reset()
	{
		if (!is_initialized())
		{
			return;
		}

		runtime::on_reset();

		// Destroy resources
		_app_state.reset();

		_backbuffer.reset();
		_backbuffer_resolved.reset();
		_backbuffer_texture.reset();
		_backbuffer_texture_surface.reset();

		_depthstencil.reset();
		_depthstencil_replacement.reset();
		_depthstencil_texture.reset();

		_default_depthstencil.reset();

		_effect_triangle_buffer.reset();
		_effect_triangle_layout.reset();

		_imgui_vertex_buffer.reset();
		_imgui_index_buffer.reset();
		_imgui_vertex_buffer_size = 0;
		_imgui_index_buffer_size = 0;

		_imgui_state.reset();

		// Clear depth source table
		for (auto &it : _depth_source_table)
		{
			it.first->Release();
		}

		_depth_source_table.clear();
	}
	void d3d9_runtime::on_present()
	{
		if (!is_initialized())
			return;

		// Begin post processing
		if (FAILED(_device->BeginScene()))
			return;

		detect_depth_source();

		// Capture device state
		_app_state->Capture();

		BOOL software_rendering_enabled;
		D3DVIEWPORT9 viewport;
		com_ptr<IDirect3DSurface9> stateblock_rendertargets[8], stateblock_depthstencil;

		_device->GetViewport(&viewport);

		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
		{
			_device->GetRenderTarget(target, &stateblock_rendertargets[target]);
		}

		_device->GetDepthStencilSurface(&stateblock_depthstencil);

		if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		{
			software_rendering_enabled = _device->GetSoftwareVertexProcessing();

			_device->SetSoftwareVertexProcessing(FALSE);
		}

		// Resolve back buffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->StretchRect(_backbuffer.get(), nullptr, _backbuffer_resolved.get(), nullptr, D3DTEXF_NONE);
		}

		// Apply post processing
		if (is_effect_loaded())
		{
			_device->SetRenderTarget(0, _backbuffer_resolved.get());
			_device->SetDepthStencilSurface(nullptr);

			// Setup vertex input
			_device->SetStreamSource(0, _effect_triangle_buffer.get(), 0, sizeof(float));
			_device->SetVertexDeclaration(_effect_triangle_layout.get());

			on_present_effect();
		}

		// Apply presenting
		runtime::on_present();

		// Copy to back buffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer.get(), nullptr, D3DTEXF_NONE);
		}

		// Apply previous device state
		_app_state->Apply();

		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
		{
			_device->SetRenderTarget(target, stateblock_rendertargets[target].get());
		}

		_device->SetDepthStencilSurface(stateblock_depthstencil.get());

		_device->SetViewport(&viewport);

		if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		{
			_device->SetSoftwareVertexProcessing(software_rendering_enabled);
		}

		// End post processing
		_device->EndScene();
	}
	void d3d9_runtime::on_draw_call(D3DPRIMITIVETYPE type, UINT vertices)
	{
		switch (type)
		{
			case D3DPT_LINELIST:
				vertices *= 2;
				break;
			case D3DPT_LINESTRIP:
				vertices += 1;
				break;
			case D3DPT_TRIANGLELIST:
				vertices *= 3;
				break;
			case D3DPT_TRIANGLESTRIP:
			case D3DPT_TRIANGLEFAN:
				vertices += 2;
				break;
		}

		_vertices += vertices;
		_drawcalls += 1;

		com_ptr<IDirect3DSurface9> depthstencil;
		_device->GetDepthStencilSurface(&depthstencil);

		if (depthstencil != nullptr)
		{
			if (depthstencil == _depthstencil_replacement)
			{
				depthstencil = _depthstencil;
			}

			const auto it = _depth_source_table.find(depthstencil.get());

			if (it != _depth_source_table.end())
			{
				it->second.drawcall_count = _drawcalls;
				it->second.vertices_count += vertices;
			}
		}
	}
	void d3d9_runtime::on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil)
	{
		if (_depth_source_table.find(depthstencil) == _depth_source_table.end())
		{
			D3DSURFACE_DESC desc;
			depthstencil->GetDesc(&desc);

			// Early rejection
			if ( desc.MultiSampleType != D3DMULTISAMPLE_NONE ||
				(desc.Width < _width * 0.95 || desc.Width > _width * 1.05) ||
				(desc.Height < _height * 0.95 || desc.Height > _height * 1.05))
			{
				return;
			}
	
			depthstencil->AddRef();

			// Begin tracking
			const depth_source_info info = { desc.Width, desc.Height };
			_depth_source_table.emplace(depthstencil, info);
		}

		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
		{
			depthstencil = _depthstencil_replacement.get();
		}
	}
	void d3d9_runtime::on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil)
	{
		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil_replacement)
		{
			depthstencil->Release();

			depthstencil = _depthstencil.get();

			depthstencil->AddRef();
		}
	}

	void d3d9_runtime::capture_frame(uint8_t *buffer) const
	{
		if (_backbuffer_format != D3DFMT_X8R8G8B8 &&
			_backbuffer_format != D3DFMT_X8B8G8R8 &&
			_backbuffer_format != D3DFMT_A8R8G8B8 &&
			_backbuffer_format != D3DFMT_A8B8G8R8)
		{
			LOG(WARNING) << "Screenshots are not supported for back buffer format " << _backbuffer_format << ".";
			return;
		}

		HRESULT hr;
		com_ptr<IDirect3DSurface9> screenshot_surface;
		hr = _device->CreateOffscreenPlainSurface(_width, _height, _backbuffer_format, D3DPOOL_SYSTEMMEM, &screenshot_surface, nullptr);

		if (FAILED(hr))
		{
			return;
		}

		hr = _device->GetRenderTargetData(_backbuffer_resolved.get(), screenshot_surface.get());

		if (FAILED(hr))
		{
			return;
		}

		D3DLOCKED_RECT mapped_rect;
		hr = screenshot_surface->LockRect(&mapped_rect, nullptr, D3DLOCK_READONLY);

		if (FAILED(hr))
		{
			return;
		}

		auto mapped_data = static_cast<BYTE *>(mapped_rect.pBits);
		const UINT pitch = _width * 4;

		for (UINT y = 0; y < _height; y++)
		{
			std::memcpy(buffer, mapped_data, std::min(pitch, static_cast<UINT>(mapped_rect.Pitch)));

			for (UINT x = 0; x < pitch; x += 4)
			{
				buffer[x + 3] = 0xFF;

				if (_backbuffer_format == D3DFMT_A8R8G8B8 || _backbuffer_format == D3DFMT_X8R8G8B8)
				{
					std::swap(buffer[x + 0], buffer[x + 2]);
				}
			}

			buffer += pitch;
			mapped_data += mapped_rect.Pitch;
		}

		screenshot_surface->UnlockRect();
	}
	bool d3d9_runtime::load_effect(const reshadefx::spirv_module &module, std::string &errors)
	{
		return d3d9_effect_compiler(this, module, errors, false).run();
	}
	bool d3d9_runtime::update_texture(texture &texture, const uint8_t *data)
	{
		if (texture.impl_reference != texture_reference::none)
		{
			return false;
		}

		const auto texture_impl = texture.impl->as<d3d9_tex_data>();

		assert(data != nullptr);
		assert(texture_impl != nullptr);

		D3DSURFACE_DESC desc;
		texture_impl->texture->GetLevelDesc(0, &desc);

		HRESULT hr;
		com_ptr<IDirect3DTexture9> mem_texture;
		hr = _device->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &mem_texture, nullptr);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to create memory texture for texture updating! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		D3DLOCKED_RECT mapped_rect;
		hr = mem_texture->LockRect(0, &mapped_rect, nullptr, 0);

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to lock memory texture for texture updating! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		const UINT size = std::min(texture.width * 4, static_cast<UINT>(mapped_rect.Pitch)) * texture.height;
		auto mapped_data = static_cast<BYTE *>(mapped_rect.pBits);

		switch (texture.format)
		{
			case texture_format::r8:
				for (UINT i = 0; i < size; i += 4, mapped_data += 4)
					mapped_data[0] = 0,
					mapped_data[1] = 0,
					mapped_data[2] = data[i],
					mapped_data[3] = 0;
				break;
			case texture_format::rg8:
				for (UINT i = 0; i < size; i += 4, mapped_data += 4)
					mapped_data[0] = 0,
					mapped_data[1] = data[i + 1],
					mapped_data[2] = data[i],
					mapped_data[3] = 0;
				break;
			case texture_format::rgba8:
				for (UINT i = 0; i < size; i += 4, mapped_data += 4)
					mapped_data[0] = data[i + 2],
					mapped_data[1] = data[i + 1],
					mapped_data[2] = data[i],
					mapped_data[3] = data[i + 3];
				break;
			default:
				std::memcpy(mapped_data, data, size);
				break;
		}

		mem_texture->UnlockRect(0);

		hr = _device->UpdateTexture(mem_texture.get(), texture_impl->texture.get());

		if (FAILED(hr))
		{
			LOG(ERROR) << "Failed to update texture from memory texture! HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		return true;
	}
	bool d3d9_runtime::update_texture_reference(texture &texture, texture_reference id)
	{
		com_ptr<IDirect3DTexture9> new_reference;

		switch (id)
		{
			case texture_reference::back_buffer:
				new_reference = _backbuffer_texture;
				break;
			case texture_reference::depth_buffer:
				new_reference = _depthstencil_texture;
				break;
			default:
				return false;
		}

		texture.impl_reference = id;

		const auto texture_impl = texture.impl->as<d3d9_tex_data>();

		assert(texture_impl != nullptr);

		if (new_reference == texture_impl->texture)
		{
			return true;
		}

		texture_impl->surface.reset();

		if (new_reference == nullptr)
		{
			texture_impl->texture.reset();

			texture.width = texture.height = texture.levels = 0;
			texture.format = texture_format::unknown;
		}
		else
		{
			texture_impl->texture = new_reference;
			new_reference->GetSurfaceLevel(0, &texture_impl->surface);

			D3DSURFACE_DESC desc;
			texture_impl->surface->GetDesc(&desc);

			texture.width = desc.Width;
			texture.height = desc.Height;
			texture.format = texture_format::unknown;
			texture.levels = new_reference->GetLevelCount();
		}

		return true;
	}

	void d3d9_runtime::render_technique(const technique &technique)
	{
		bool is_default_depthstencil_cleared = false;

		// Setup shader constants
		if (technique.uniform_storage_index >= 0)
		{
			const auto uniform_storage_data = reinterpret_cast<const float *>(get_uniform_value_storage().data() + technique.uniform_storage_offset);
			_device->SetVertexShaderConstantF(0, uniform_storage_data, static_cast<UINT>(technique.uniform_storage_index));
			_device->SetPixelShaderConstantF(0, uniform_storage_data, static_cast<UINT>(technique.uniform_storage_index));
		}

		for (const auto &pass_object : technique.passes)
		{
			const d3d9_pass_data &pass = *pass_object->as<d3d9_pass_data>();

			// Setup states
			pass.stateblock->Apply();

			// Save back buffer of previous pass
			_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer_texture_surface.get(), nullptr, D3DTEXF_NONE);

			// Setup shader resources
			for (DWORD sampler = 0; sampler < pass.sampler_count; sampler++)
			{
				_device->SetTexture(sampler, pass.samplers[sampler].texture ? pass.samplers[sampler].texture->texture.get() : nullptr);

				for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state++)
				{
					_device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(state), pass.samplers[sampler].states[state]);
				}
			}

			// Setup render targets
			for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
			{
				_device->SetRenderTarget(target, pass.render_targets[target]);
			}

			D3DVIEWPORT9 viewport;
			_device->GetViewport(&viewport);

			//const float texelsize[4] = { -1.0f / viewport.Width, 1.0f / viewport.Height };
			//_device->SetVertexShaderConstantF(255, texelsize, 1);

			const bool is_viewport_sized = viewport.Width == _width && viewport.Height == _height;

			_device->SetDepthStencilSurface(is_viewport_sized ? _default_depthstencil.get() : nullptr);

			if (is_viewport_sized && !is_default_depthstencil_cleared)
			{
				is_default_depthstencil_cleared = true;

				_device->Clear(0, nullptr, (pass.clear_render_targets ? D3DCLEAR_TARGET : 0) | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
			}
			else if (pass.clear_render_targets)
			{
				_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
			}

			// Draw triangle
			_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

			_vertices += 3;
			_drawcalls += 1;

			// Update shader resources
			for (const auto target : pass.render_targets)
			{
				if (target == nullptr || target == _backbuffer_resolved)
				{
					continue;
				}

				com_ptr<IDirect3DBaseTexture9> texture;

				if (SUCCEEDED(target->GetContainer(IID_PPV_ARGS(&texture))) && texture->GetLevelCount() > 1)
				{
					texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
					texture->GenerateMipSubLevels();
				}
			}
		}
	}
	void d3d9_runtime::render_imgui_draw_data(ImDrawData *draw_data)
	{
		// Fixed-function vertex layout
		struct vertex
		{
			float x, y, z;
			D3DCOLOR col;
			float u, v;
		};

		// Create and grow buffers if needed
		if (_imgui_vertex_buffer == nullptr ||
			_imgui_vertex_buffer_size < draw_data->TotalVtxCount)
		{
			_imgui_vertex_buffer.reset();
			_imgui_vertex_buffer_size = draw_data->TotalVtxCount + 5000;

			if (FAILED(_device->CreateVertexBuffer(_imgui_vertex_buffer_size * sizeof(vertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_DEFAULT, &_imgui_vertex_buffer, nullptr)))
			{
				return;
			}
		}
		if (_imgui_index_buffer == nullptr ||
			_imgui_index_buffer_size < draw_data->TotalIdxCount)
		{
			_imgui_index_buffer.reset();
			_imgui_index_buffer_size = draw_data->TotalIdxCount + 10000;

			if (FAILED(_device->CreateIndexBuffer(_imgui_index_buffer_size * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &_imgui_index_buffer, nullptr)))
			{
				return;
			}
		}

		vertex *vtx_dst;
		ImDrawIdx *idx_dst;

		if (FAILED(_imgui_vertex_buffer->Lock(0, draw_data->TotalVtxCount * sizeof(vertex), reinterpret_cast<void **>(&vtx_dst), D3DLOCK_DISCARD)) ||
			FAILED(_imgui_index_buffer->Lock(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx), reinterpret_cast<void **>(&idx_dst), D3DLOCK_DISCARD)))
		{
			return;
		}

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList *const cmd_list = draw_data->CmdLists[n];

			for (auto vtx_src = cmd_list->VtxBuffer.begin(); vtx_src != cmd_list->VtxBuffer.end(); vtx_src++, vtx_dst++)
			{
				vtx_dst->x = vtx_src->pos.x;
				vtx_dst->y = vtx_src->pos.y;
				vtx_dst->z = 0.0f;

				// RGBA --> ARGB for Direct3D 9
				vtx_dst->col = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);

				vtx_dst->u = vtx_src->uv.x;
				vtx_dst->v = vtx_src->uv.y;
			}

			std::memcpy(idx_dst, &cmd_list->IdxBuffer.front(), cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx));

			idx_dst += cmd_list->IdxBuffer.size();
		}

		_imgui_vertex_buffer->Unlock();
		_imgui_index_buffer->Unlock();

		// Setup render state: fixed-pipeline, alpha-blending, no face culling, no depth testing
		_device->SetRenderTarget(0, _backbuffer_resolved.get());
		for (DWORD target = 1; target < _num_simultaneous_rendertargets; target++)
		{
			_device->SetRenderTarget(target, nullptr);
		}
		_device->SetDepthStencilSurface(nullptr);
		_device->SetStreamSource(0, _imgui_vertex_buffer.get(), 0, sizeof(vertex));
		_device->SetIndices(_imgui_index_buffer.get());
		_imgui_state->Apply();

		// Render command lists
		UINT vtx_offset = 0, idx_offset = 0;

		for (UINT i = 0; i < _num_samplers; i++)
			_device->SetTexture(i, nullptr);

		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList *const cmd_list = draw_data->CmdLists[n];

			for (const ImDrawCmd *cmd = cmd_list->CmdBuffer.begin(); cmd != cmd_list->CmdBuffer.end(); idx_offset += cmd->ElemCount, cmd++)
			{
				const RECT scissor_rect = {
					static_cast<LONG>(cmd->ClipRect.x),
					static_cast<LONG>(cmd->ClipRect.y),
					static_cast<LONG>(cmd->ClipRect.z),
					static_cast<LONG>(cmd->ClipRect.w)
				};

				_device->SetTexture(0, static_cast<const d3d9_tex_data *>(cmd->TextureId)->texture.get());
				_device->SetScissorRect(&scissor_rect);

				_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vtx_offset, 0, cmd_list->VtxBuffer.size(), idx_offset, cmd->ElemCount / 3);
			}

			vtx_offset += cmd_list->VtxBuffer.size();
		}
	}

	void d3d9_runtime::draw_debug_menu()
	{
		if (ImGui::CollapsingHeader("Buffer Detection", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Checkbox("Disable replacement with INTZ format", &_disable_intz))
			{
				runtime::save_config();

				// Force depth-stencil replacement recreation
				_depthstencil = nullptr;
			}

			for (const auto &it : _depth_source_table)
			{
				ImGui::Text("%s0x%p | %u draw calls ==> %u vertices", (it.first == _depthstencil ? "> " : "  "), it.first, it.second.drawcall_count, it.second.vertices_count);
			}
		}
	}

	void d3d9_runtime::detect_depth_source()
	{
		static int cooldown = 0, traffic = 0;

		if (cooldown-- > 0)
		{
			traffic += g_network_traffic > 0;
			return;
		}
		else
		{
			cooldown = 30;

			if (traffic > 10)
			{
				traffic = 0;
				create_depthstencil_replacement(nullptr);
				return;
			}
			else
			{
				traffic = 0;
			}
		}

		if (_is_multisampling_enabled || _depth_source_table.empty())
		{
			return;
		}

		depth_source_info best_info = { 0 };
		IDirect3DSurface9 *best_match = nullptr;

		for (auto it = _depth_source_table.begin(); it != _depth_source_table.end();)
		{
			const auto depthstencil = it->first;
			auto &depthstencil_info = it->second;

			if ((depthstencil->AddRef(), depthstencil->Release()) == 1)
			{
				depthstencil->Release();

				it = _depth_source_table.erase(it);
				continue;
			}
			else
			{
				++it;
			}

			if (depthstencil_info.drawcall_count == 0)
			{
				continue;
			}

			if ((depthstencil_info.vertices_count * (1.2f - float(depthstencil_info.drawcall_count) / _drawcalls)) >= (best_info.vertices_count * (1.2f - float(best_info.drawcall_count) / _drawcalls)))
			{
				best_match = depthstencil;
				best_info = depthstencil_info;
			}

			depthstencil_info.drawcall_count = depthstencil_info.vertices_count = 0;
		}

		if (best_match != nullptr && _depthstencil != best_match)
		{
			create_depthstencil_replacement(best_match);
		}
	}

	bool d3d9_runtime::create_depthstencil_replacement(IDirect3DSurface9 *depthstencil)
	{
		_depthstencil.reset();
		_depthstencil_replacement.reset();
		_depthstencil_texture.reset();

		if (depthstencil != nullptr)
		{
			D3DSURFACE_DESC desc;
			depthstencil->GetDesc(&desc);

			_depthstencil = depthstencil;

			if (!_disable_intz &&
				desc.Format != D3DFMT_INTZ &&
				desc.Format != D3DFMT_DF16 &&
				desc.Format != D3DFMT_DF24)
			{
				D3DDISPLAYMODE displaymode;
				_swapchain->GetDisplayMode(&displaymode);
				D3DDEVICE_CREATION_PARAMETERS creation_params;
				_device->GetCreationParameters(&creation_params);

				desc.Format = D3DFMT_UNKNOWN;
				const D3DFORMAT formats[] = { D3DFMT_INTZ, D3DFMT_DF24, D3DFMT_DF16 };

				for (const auto format : formats)
				{
					if (SUCCEEDED(_d3d->CheckDeviceFormat(creation_params.AdapterOrdinal, creation_params.DeviceType, displaymode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format)))
					{
						desc.Format = format;
						break;
					}
				}

				if (desc.Format == D3DFMT_UNKNOWN)
				{
					LOG(ERROR) << "Your graphics card is missing support for at least one of the 'INTZ', 'DF24' or 'DF16' texture formats. Cannot create depth replacement texture.";

					return false;
				}

				const HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, desc.Format, D3DPOOL_DEFAULT, &_depthstencil_texture, nullptr);

				if (SUCCEEDED(hr))
				{
					_depthstencil_texture->GetSurfaceLevel(0, &_depthstencil_replacement);

					// Update auto depth stencil
					com_ptr<IDirect3DSurface9> current_depthstencil;
					_device->GetDepthStencilSurface(&current_depthstencil);

					if (current_depthstencil != nullptr)
					{
						if (current_depthstencil == _depthstencil)
						{
							_device->SetDepthStencilSurface(_depthstencil_replacement.get());
						}
					}
				}
				else
				{
					LOG(ERROR) << "Failed to create depth replacement texture! HRESULT is '" << std::hex << hr << std::dec << "'.";

					return false;
				}
			}
			else
			{
				_depthstencil_replacement = _depthstencil;

				const HRESULT hr = _depthstencil_replacement->GetContainer(IID_PPV_ARGS(&_depthstencil_texture));

				if (FAILED(hr))
				{
					LOG(ERROR) << "Failed to retrieve texture from depth surface! HRESULT is '" << std::hex << hr << std::dec << "'.";

					return false;
				}
			}
		}

		// Update effect textures
		for (auto &texture : _textures)
		{
			if (texture.impl_reference == texture_reference::depth_buffer)
			{
				update_texture_reference(texture, texture_reference::depth_buffer);
			}
		}

		return true;
	}
}
