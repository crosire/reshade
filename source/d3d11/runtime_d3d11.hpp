/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "draw_call_tracker.hpp"
#include <mutex>

namespace reshade { enum class texture_reference; }
namespace reshadefx { struct sampler_info; }

namespace reshade::d3d11
{
	class runtime_d3d11 : public runtime
	{
	public:
		runtime_d3d11(ID3D11Device *device, IDXGISwapChain *swapchain);
		~runtime_d3d11();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present(draw_call_tracker& tracker);

		void on_set_depthstencil_view(ID3D11DepthStencilView *&depthstencil);
		void on_get_depthstencil_view(ID3D11DepthStencilView *&depthstencil);
		void on_clear_depthstencil_view(ID3D11DepthStencilView *&depthstencil);

		bool capture_screenshot(uint8_t *buffer) const override;

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D11Texture2D> select_depth_texture_save(D3D11_TEXTURE2D_DESC texture_desc);
#endif

		bool depth_buffer_before_clear = false;
		bool depth_buffer_more_copies = false;
		bool extended_depth_buffer_detection = false;
		unsigned int cleared_depth_buffer_index = 0;
		int depth_buffer_texture_format = 0; // No depth buffer texture format filter by default

	private:
		bool init_backbuffer_texture();
		bool init_default_depth_stencil();

		bool init_texture(texture &info) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;
		bool update_texture_reference(texture &texture);
		void update_texture_references(texture_reference type);

		bool compile_effect(effect_data &effect) override;
		void unload_effects() override;

		bool add_sampler(const reshadefx::sampler_info &info, struct d3d11_technique_data &technique_init);
		bool init_technique(technique &info, const struct d3d11_technique_data &technique_init, const std::unordered_map<std::string, com_ptr<IUnknown>> &entry_points);

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		void detect_depth_source(draw_call_tracker& tracker);
		bool create_depthstencil_replacement(ID3D11DepthStencilView *depthstencil, ID3D11Texture2D *texture);

		struct depth_texture_save_info
		{
			com_ptr<ID3D11Texture2D> src_texture;
			D3D11_TEXTURE2D_DESC src_texture_desc;
			com_ptr<ID3D11Texture2D> dest_texture;
			bool cleared = false;
		};
#endif

		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _immediate_context;
		com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv[2];
		com_ptr<ID3D11ShaderResourceView> _depthstencil_texture_srv;
		std::unordered_map<size_t, com_ptr<ID3D11SamplerState>> _effect_sampler_states;
		std::vector<com_ptr<ID3D11Buffer>> _constant_buffers;

		std::map<UINT, depth_texture_save_info> _displayed_depth_textures;
		std::unordered_map<UINT, com_ptr<ID3D11Texture2D>> _depth_texture_saves;

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		state_block _app_state;
		com_ptr<ID3D11Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D11DepthStencilView> _depthstencil, _depthstencil_replacement;
		ID3D11DepthStencilView *_best_depthstencil_overwrite = nullptr;
		com_ptr<ID3D11Texture2D> _depthstencil_texture;
		com_ptr<ID3D11DepthStencilView> _default_depthstencil;
		com_ptr<ID3D11VertexShader> _copy_vertex_shader;
		com_ptr<ID3D11PixelShader> _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler;
		std::mutex _mutex;
		com_ptr<ID3D11RasterizerState> _effect_rasterizer_state;

		int _imgui_index_buffer_size = 0;
		com_ptr<ID3D11Buffer> _imgui_index_buffer;
		int _imgui_vertex_buffer_size = 0;
		com_ptr<ID3D11Buffer> _imgui_vertex_buffer;
		com_ptr<ID3D11VertexShader> _imgui_vertex_shader;
		com_ptr<ID3D11PixelShader> _imgui_pixel_shader;
		com_ptr<ID3D11InputLayout> _imgui_input_layout;
		com_ptr<ID3D11Buffer> _imgui_constant_buffer;
		com_ptr<ID3D11SamplerState> _imgui_texture_sampler;
		com_ptr<ID3D11RasterizerState> _imgui_rasterizer_state;
		com_ptr<ID3D11BlendState> _imgui_blend_state;
		com_ptr<ID3D11DepthStencilState> _imgui_depthstencil_state;
		draw_call_tracker *_current_tracker = nullptr;

		HMODULE _d3d_compiler = nullptr;
	};
}
