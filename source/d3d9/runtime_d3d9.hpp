/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_d3d9.hpp"
#include "state_block_d3d9.hpp"

namespace reshade::d3d9
{
	class runtime_impl : public api::api_object_impl<IDirect3DSwapChain9 *, runtime>
	{
	public:
		runtime_impl(device_impl *device, IDirect3DSwapChain9 *swapchain);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _device_impl; }

		bool on_init();
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const final;

		void update_texture_bindings(const char *semantic, api::resource_view srv) final;

	private:
		bool init_effect(size_t index) final;
		void unload_effect(size_t index) final;
		void unload_effects() final;

		bool init_texture(texture &texture) final;
		void upload_texture(const texture &texture, const uint8_t *pixels) final;
		void destroy_texture(texture &texture) final;

		void render_technique(technique &technique) final;

		const com_ptr<IDirect3DDevice9> _device;
		device_impl *const _device_impl;

		state_block _app_state;

		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<IDirect3DSurface9> _effect_stencil;
		unsigned int _max_effect_vertices = 0;
		com_ptr<IDirect3DVertexBuffer9> _effect_vertex_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_vertex_layout;

		std::unordered_map<std::string, com_ptr<IDirect3DTexture9>> _texture_semantic_bindings;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) final;

		struct imgui_resources
		{
			com_ptr<IDirect3DStateBlock9> state;

			com_ptr<IDirect3DIndexBuffer9> indices;
			com_ptr<IDirect3DVertexBuffer9> vertices;
			int num_indices = 0;
			int num_vertices = 0;
		} _imgui;
#endif
	};
}
