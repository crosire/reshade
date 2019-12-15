/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "buffer_detection.hpp"

namespace reshade { enum class texture_reference; }
namespace reshadefx { struct sampler_info; }

namespace reshade::d3d10
{
	class runtime_d3d10 : public runtime
	{
	public:
		runtime_d3d10(ID3D10Device1 *device, IDXGISwapChain *swapchain);
		~runtime_d3d10();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present(buffer_detection &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;

		void render_technique(technique &technique) override;

		state_block _app_state;
		const com_ptr<ID3D10Device1> _device;
		const com_ptr<IDXGISwapChain> _swapchain;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D10Texture2D> _backbuffer;
		com_ptr<ID3D10Texture2D> _backbuffer_resolved;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2];
		com_ptr<ID3D10Texture2D> _depth_texture;
		com_ptr<ID3D10ShaderResourceView> _depth_texture_srv;

		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState>  _copy_sampler_state;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer_state;
		com_ptr<ID3D10DepthStencilView> _effect_depthstencil;
		std::vector<com_ptr<ID3D10Buffer>> _effect_constant_buffers;
		std::unordered_map<size_t, com_ptr<ID3D10SamplerState>> _effect_sampler_states;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		int _imgui_index_buffer_size = 0;
		com_ptr<ID3D10Buffer> _imgui_index_buffer;
		int _imgui_vertex_buffer_size = 0;
		com_ptr<ID3D10Buffer> _imgui_vertex_buffer;
		com_ptr<ID3D10VertexShader> _imgui_vertex_shader;
		com_ptr<ID3D10PixelShader> _imgui_pixel_shader;
		com_ptr<ID3D10InputLayout> _imgui_input_layout;
		com_ptr<ID3D10Buffer> _imgui_constant_buffer;
		com_ptr<ID3D10SamplerState> _imgui_texture_sampler;
		com_ptr<ID3D10RasterizerState> _imgui_rasterizer_state;
		com_ptr<ID3D10BlendState> _imgui_blend_state;
		com_ptr<ID3D10DepthStencilState> _imgui_depthstencil_state;
#endif

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
		void draw_depth_debug_menu();
		void update_depthstencil_texture(com_ptr<ID3D10Texture2D> texture);

		bool _filter_aspect_ratio = true;
		bool _preserve_depth_buffers = false;
		UINT _depth_clear_index_override = std::numeric_limits<UINT>::max();
		ID3D10Texture2D *_depth_texture_override = nullptr;
		buffer_detection *_current_tracker = nullptr;
#endif
	};
}
