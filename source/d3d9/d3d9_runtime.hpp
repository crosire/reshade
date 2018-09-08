/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d9.h>
#include "runtime.hpp"
#include "com_ptr.hpp"

namespace reshade::d3d9
{
	struct d3d9_sampler
	{
		DWORD states[12];
		struct d3d9_tex_data *texture;
	};

	struct d3d9_tex_data : base_object
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};
	struct d3d9_pass_data : base_object
	{
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		d3d9_sampler samplers[16] = { };
		DWORD sampler_count = 0;
		com_ptr<IDirect3DStateBlock9> stateblock;
		bool clear_render_targets = false;
		IDirect3DSurface9 *render_targets[8] = { };
	};

	class d3d9_runtime : public runtime
	{
	public:
		d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset();
		void on_present();
		void on_draw_call(D3DPRIMITIVETYPE type, UINT count);
		void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);

		void capture_frame(uint8_t *buffer) const override;
		bool load_effect(const reshadefx::spirv_module &module, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture, texture_reference id);

		void render_technique(const technique &technique) override;
		void render_imgui_draw_data(ImDrawData *data) override;

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
		bool init_imgui_font_atlas();

		void draw_debug_menu();

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
	};
}
