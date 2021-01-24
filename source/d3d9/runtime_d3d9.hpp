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
	class runtime_d3d9 : public runtime, api::api_data
	{
	public:
		runtime_d3d9(device_impl *device, IDirect3DSwapChain9 *swapchain);
		~runtime_d3d9();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::device *get_device() override { return _device_impl; }

		bool on_init();
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const override;

		void update_texture_bindings(const char *semantic, api::resource_view_handle srv) override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;
		void destroy_texture(texture &texture) override;

		void render_technique(technique &technique) override;

		device_impl *const _device_impl;
		const com_ptr<IDirect3DDevice9> _device;
		const com_ptr<IDirect3DSwapChain9> _swapchain;

		state_block _app_state;

		unsigned int _max_vertices = 0;

		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<IDirect3DSurface9> _effect_stencil;
		com_ptr<IDirect3DVertexBuffer9> _effect_vertex_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_vertex_layout;

		std::unordered_map<std::string, com_ptr<IDirect3DTexture9>> _texture_semantic_bindings;

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
	};
}