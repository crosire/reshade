/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "effect_expression.hpp"
#include "com_ptr.hpp"
#include <d3d9.h>

namespace reshade::d3d9
{
	struct d3d9_tex_data : base_object
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};
	struct d3d9_pass_data : base_object
	{
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		com_ptr<IDirect3DStateBlock9> stateblock;
		bool clear_render_targets = false;
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

	class runtime_d3d9 : public runtime
	{
	public:
		runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
		~runtime_d3d9();

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset();
		void on_present();
		void on_draw_call(D3DPRIMITIVETYPE type, UINT count);
		void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);

		void capture_frame(uint8_t *buffer) const override;
		void update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture);

		bool compile_effect(effect_data &effect) override;

		void render_technique(const technique &technique) override;

		com_ptr<IDirect3D9> _d3d;
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DSwapChain9> _swapchain;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;

	private:
		struct depth_source_info
		{
			UINT width, height;
			UINT drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();

		bool add_sampler(const reshadefx::sampler_info &info, d3d9_technique_data &technique_init);
		bool init_texture(texture &info) override;
		bool init_technique(technique &info, const d3d9_technique_data &technique_init, const std::unordered_map<std::string, com_ptr<IDirect3DVertexShader9>> &vs_entry_points, const std::unordered_map<std::string, com_ptr<IDirect3DPixelShader9>> &ps_entry_points);

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

		void detect_depth_source();
		bool create_depthstencil_replacement(IDirect3DSurface9 *depthstencil);

		UINT _behavior_flags;
		UINT _num_samplers;
		UINT _num_simultaneous_rendertargets;
		bool _disable_intz = false;
		bool _is_multisampling_enabled = false;
		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DStateBlock9> _app_state;
		com_ptr<IDirect3DSurface9> _depthstencil;
		com_ptr<IDirect3DSurface9> _depthstencil_replacement;
		com_ptr<IDirect3DSurface9> _default_depthstencil;
		std::unordered_map<IDirect3DSurface9 *, depth_source_info> _depth_source_table;

		com_ptr<IDirect3DVertexBuffer9> _effect_triangle_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_triangle_layout;

		com_ptr<IDirect3DStateBlock9> _imgui_state;
		com_ptr<IDirect3DVertexBuffer9> _imgui_vertex_buffer;
		com_ptr<IDirect3DIndexBuffer9> _imgui_index_buffer;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;

		HMODULE _d3d_compiler = nullptr;
	};
}
