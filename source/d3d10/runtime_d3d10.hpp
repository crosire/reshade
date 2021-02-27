/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_d3d10.hpp"
#include "state_block_d3d10.hpp"

namespace reshade::d3d10
{
	class runtime_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		runtime_impl(device_impl *device, IDXGISwapChain *swapchain);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _device_impl; }

		bool on_init();
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const final;

		void update_texture_bindings(const char *semantic, api::resource_view_handle srv) final;

	private:
		bool init_effect(size_t index) final;
		void unload_effect(size_t index) final;
		void unload_effects() final;

		bool init_texture(texture &texture) final;
		void upload_texture(const texture &texture, const uint8_t *pixels) final;
		void destroy_texture(texture &texture) final;

		void render_technique(technique &technique) final;

		const com_ptr<ID3D10Device1> _device;
		device_impl *const _device_impl;

		state_block _app_state;

		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler_state;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D10Texture2D> _backbuffer;
		com_ptr<ID3D10Texture2D> _backbuffer_resolved;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2];

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer;
		std::unordered_map<size_t, com_ptr<ID3D10SamplerState>> _effect_sampler_states;
		std::vector<struct effect_data> _effect_data;
		com_ptr<ID3D10DepthStencilView> _effect_stencil;

		std::unordered_map<std::string, com_ptr<ID3D10ShaderResourceView>> _texture_semantic_bindings;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) final;

		struct imgui_resources
		{
			com_ptr<ID3D10Buffer> cb;
			com_ptr<ID3D10VertexShader> vs;
			com_ptr<ID3D10RasterizerState> rs;
			com_ptr<ID3D10PixelShader> ps;
			com_ptr<ID3D10SamplerState> ss;
			com_ptr<ID3D10BlendState> bs;
			com_ptr<ID3D10DepthStencilState> ds;
			com_ptr<ID3D10InputLayout> layout;

			com_ptr<ID3D10Buffer> indices;
			com_ptr<ID3D10Buffer> vertices;
			int num_indices = 0;
			int num_vertices = 0;
		} _imgui;
#endif
	};
}
