/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "state_tracking.hpp"

namespace reshade::d3d11
{
	class runtime_d3d11 : public runtime
	{
	public:
		runtime_d3d11(ID3D11Device *device, IDXGISwapChain *swapchain, state_tracking_context *state_tracking);
		~runtime_d3d11();

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

		void set_debug_name(ID3D11DeviceChild *object, LPCWSTR name) const;

		state_block _app_state;
		state_tracking_context &_state_tracking;
		const com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _immediate_context;
		const com_ptr<IDXGISwapChain> _swapchain;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D11Texture2D> _backbuffer;
		com_ptr<ID3D11Texture2D> _backbuffer_resolved;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv[2];

		com_ptr<ID3D11PixelShader> _copy_pixel_shader;
		com_ptr<ID3D11VertexShader> _copy_vertex_shader;
		com_ptr<ID3D11SamplerState>  _copy_sampler_state;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D11RasterizerState> _effect_rasterizer;
		std::unordered_map<size_t, com_ptr<ID3D11SamplerState>> _effect_sampler_states;
		com_ptr<ID3D11DepthStencilView> _effect_stencil;
		std::vector<struct effect_data> _effect_data;

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

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_texture_bindings(com_ptr<ID3D11Texture2D> texture);

		com_ptr<ID3D11Texture2D> _depth_texture;
		com_ptr<ID3D11ShaderResourceView> _depth_texture_srv;
		ID3D11Texture2D *_depth_texture_override = nullptr;
#endif
	};
}
