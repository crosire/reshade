/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "state_tracking.hpp"

namespace reshade::d3d10
{
	class runtime_d3d10 : public runtime
	{
	public:
		runtime_d3d10(ID3D10Device1 *device, IDXGISwapChain *swapchain, state_tracking *state_tracking);
		~runtime_d3d10();

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
		const com_ptr<ID3D10Device1> _device;
		const com_ptr<IDXGISwapChain> _swapchain;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D10Texture2D> _backbuffer;
		com_ptr<ID3D10Texture2D> _backbuffer_resolved;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2];

		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState>  _copy_sampler_state;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer;
		std::unordered_map<size_t, com_ptr<ID3D10SamplerState>> _effect_sampler_states;
		com_ptr<ID3D10DepthStencilView> _effect_stencil;
		std::vector<struct effect_data> _effect_data;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

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

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_texture_bindings(com_ptr<ID3D10Texture2D> texture);

		com_ptr<ID3D10Texture2D> _depth_texture;
		com_ptr<ID3D10ShaderResourceView> _depth_texture_srv;
		ID3D10Texture2D *_depth_texture_override = nullptr;
#endif
	};
}
