/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_d3d11.hpp"
#include "state_block_d3d11.hpp"

namespace reshade::d3d11
{
	class runtime_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		runtime_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain);
		~runtime_impl();

		api::device *get_device() override { return _device_impl; }
		api::command_queue *get_command_queue() override { return _immediate_context_impl; }

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

		const com_ptr<ID3D11Device> _device;
		const com_ptr<ID3D11DeviceContext> _immediate_context;
		device_impl *const _device_impl;
		device_context_impl *const _immediate_context_impl;

		state_block _app_state;

		com_ptr<ID3D11PixelShader> _copy_pixel_shader;
		com_ptr<ID3D11VertexShader> _copy_vertex_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler_state;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D11Texture2D> _backbuffer;
		com_ptr<ID3D11Texture2D> _backbuffer_resolved;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv[2];

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D11RasterizerState> _effect_rasterizer;
		std::unordered_map<size_t, com_ptr<ID3D11SamplerState>> _effect_sampler_states;
		std::vector<struct effect_data> _effect_data;
		com_ptr<ID3D11DepthStencilView> _effect_stencil;

		std::unordered_map<std::string, com_ptr<ID3D11ShaderResourceView>> _texture_semantic_bindings;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		struct imgui_resources
		{
			com_ptr<ID3D11Buffer> cb;
			com_ptr<ID3D11VertexShader> vs;
			com_ptr<ID3D11RasterizerState> rs;
			com_ptr<ID3D11PixelShader> ps;
			com_ptr<ID3D11SamplerState> ss;
			com_ptr<ID3D11BlendState> bs;
			com_ptr<ID3D11DepthStencilState> ds;
			com_ptr<ID3D11InputLayout> layout;

			com_ptr<ID3D11Buffer> indices;
			com_ptr<ID3D11Buffer> vertices;
			int num_indices = 0;
			int num_vertices = 0;
		} _imgui;
#endif
	};
}
