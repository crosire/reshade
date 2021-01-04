/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "state_tracking.hpp"

namespace reshade::d3d9
{
	class runtime_d3d9 : public runtime
	{
	public:
		runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain, state_tracking *state_tracking);
		~runtime_d3d9();

		bool on_init();
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;
		void destroy_texture(texture &texture) override;

		void render_technique(technique &technique) override;

		state_block _app_state;
		state_tracking &_state_tracking;
		com_ptr<IDirect3D9> _d3d;
		const com_ptr<IDirect3DDevice9> _device;
		const com_ptr<IDirect3DSwapChain9> _swapchain;

		unsigned int _max_vertices = 0;
		unsigned int _num_samplers;
		unsigned int _num_simultaneous_rendertargets;
		unsigned int _behavior_flags;

		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<IDirect3DSurface9> _effect_stencil;
		com_ptr<IDirect3DVertexBuffer9> _effect_vertex_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_vertex_layout;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		struct imgui_resources
		{
			com_ptr<IDirect3DStateBlock9> state;

			com_ptr<IDirect3DIndexBuffer9> indices;
			com_ptr<IDirect3DVertexBuffer9> vertices;
			int num_indices = 0;
			int num_vertices = 0;
		} _imgui;
#endif

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_texture_bindings(com_ptr<IDirect3DSurface9> surface);

		com_ptr<IDirect3DTexture9> _depth_texture;
		com_ptr<IDirect3DSurface9> _depth_surface;

		bool _disable_intz = false;
		bool _reset_buffer_detection = false;
		IDirect3DSurface9 *_depth_surface_override = nullptr;
#endif
	};
}
