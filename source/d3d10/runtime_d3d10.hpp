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
		bool init_backbuffer_texture();
		bool init_default_depth_stencil();

		bool init_texture(texture &info) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;
		bool update_texture_reference(texture &texture);
		void update_texture_references(texture_reference type);

		bool compile_effect(effect_data &effect) override;
		void unload_effects() override;

		bool add_sampler(const reshadefx::sampler_info &info, struct d3d10_technique_data &technique_init);
		bool init_technique(technique &info, const struct d3d10_technique_data &technique_init, const std::unordered_map<std::string, com_ptr<IUnknown>> &entry_points);

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
		void update_depthstencil_texture(com_ptr<ID3D10Texture2D> texture);

		bool _filter_aspect_ratio = true;
		bool _preserve_depth_buffers = false;
		UINT _depth_clear_index_override = std::numeric_limits<UINT>::max();
		ID3D10Texture2D *_depth_texture_override = nullptr;
#endif

		const com_ptr<ID3D10Device1> _device;
		const com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2];
		com_ptr<ID3D10ShaderResourceView> _depth_texture_srv;
		std::unordered_map<size_t, com_ptr<ID3D10SamplerState>> _effect_sampler_states;
		std::vector<com_ptr<ID3D10Buffer>> _constant_buffers;

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		state_block _app_state;
		com_ptr<ID3D10Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D10Texture2D> _depth_texture;
		com_ptr<ID3D10DepthStencilView> _default_depthstencil;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer_state;

		HMODULE _d3d_compiler = nullptr;

		buffer_detection *_current_tracker = nullptr;

#if RESHADE_GUI
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
	};
}
