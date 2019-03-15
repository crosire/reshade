/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "draw_call_tracker.hpp"
#include <mutex>

inline reshade::log::message &operator<<(reshade::log::message &m, REFIID riid)
{
	OLECHAR riid_string[40];
	StringFromGUID2(riid, riid_string, ARRAYSIZE(riid_string));

	return m << riid_string;
}

namespace reshade::d3d11
{
	struct d3d11_tex_data : base_object
	{
		com_ptr<ID3D11Texture2D> texture;
		com_ptr<ID3D11ShaderResourceView> srv[2];
		com_ptr<ID3D11RenderTargetView> rtv[2];
	};
	struct d3d11_pass_data : base_object
	{
		com_ptr<ID3D11VertexShader> vertex_shader;
		com_ptr<ID3D11PixelShader> pixel_shader;
		com_ptr<ID3D11BlendState> blend_state;
		com_ptr<ID3D11DepthStencilState> depth_stencil_state;
		UINT stencil_reference;
		bool clear_render_targets;
		com_ptr<ID3D11RenderTargetView> render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D11ShaderResourceView> render_target_resources[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		D3D11_VIEWPORT viewport;
		std::vector<com_ptr<ID3D11ShaderResourceView>> shader_resources;
	};
	struct d3d11_technique_data : base_object
	{
		bool query_in_flight = false;
		com_ptr<ID3D11Query> timestamp_disjoint;
		com_ptr<ID3D11Query> timestamp_query_beg;
		com_ptr<ID3D11Query> timestamp_query_end;
		std::vector<com_ptr<ID3D11SamplerState>> sampler_states;
		std::vector<com_ptr<ID3D11ShaderResourceView>> texture_bindings;
		ptrdiff_t uniform_storage_offset = 0;
		ptrdiff_t uniform_storage_index = -1;
	};

	class runtime_d3d11 : public runtime
	{
	public:
		runtime_d3d11(ID3D11Device *device, IDXGISwapChain *swapchain);
		~runtime_d3d11();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present(draw_call_tracker& tracker);

		void capture_frame(uint8_t *buffer) const override;
		void update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture);

		bool compile_effect(effect_data &effect) override;
		void unload_effects() override;

		void render_technique(const technique &technique) override;

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D11Texture2D> select_depth_texture_save(D3D11_TEXTURE2D_DESC &texture_desc);
#endif

		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _immediate_context;
		com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv[2];
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11ShaderResourceView> _depthstencil_texture_srv;
		std::unordered_map<size_t, com_ptr<ID3D11SamplerState>> _effect_sampler_states;
		std::vector<com_ptr<ID3D11Buffer>> _constant_buffers;

		bool depth_buffer_before_clear = false;
		bool extended_depth_buffer_detection = false;
		unsigned int cleared_depth_buffer_index = 0;
		int depth_buffer_texture_format = 0; // No depth buffer texture format filter by default

	private:
		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();

		bool add_sampler(const reshadefx::sampler_info &info, d3d11_technique_data &technique_init);
		bool init_texture(texture &info) override;
		bool init_technique(technique &info, const d3d11_technique_data &technique_init, const std::unordered_map<std::string, com_ptr<ID3D11VertexShader>> &vs_entry_points, const std::unordered_map<std::string, com_ptr<ID3D11PixelShader>> &ps_entry_points);

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		void detect_depth_source(draw_call_tracker& tracker);
		bool create_depthstencil_replacement(ID3D11DepthStencilView *depthstencil, ID3D11Texture2D *texture);
#endif

		struct depth_texture_save_info
		{
			com_ptr<ID3D11Texture2D> src_texture;
			D3D11_TEXTURE2D_DESC src_texture_desc;
			com_ptr<ID3D11Texture2D> dest_texture;
			bool cleared = false;
		};

		std::map<UINT, depth_texture_save_info> _displayed_depth_textures;
		std::unordered_map<UINT, com_ptr<ID3D11Texture2D>> _depth_texture_saves;

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		state_block _stateblock;
		com_ptr<ID3D11Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D11DepthStencilView> _depthstencil, _depthstencil_replacement;
		ID3D11DepthStencilView *_best_depth_stencil_overwrite = nullptr;
		com_ptr<ID3D11Texture2D> _depthstencil_texture;
		com_ptr<ID3D11DepthStencilView> _default_depthstencil;
		com_ptr<ID3D11VertexShader> _copy_vertex_shader;
		com_ptr<ID3D11PixelShader> _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler;
		std::mutex _mutex;
		com_ptr<ID3D11RasterizerState> _effect_rasterizer_state;

		com_ptr<ID3D11Buffer> _imgui_vertex_buffer, _imgui_index_buffer;
		com_ptr<ID3D11VertexShader> _imgui_vertex_shader;
		com_ptr<ID3D11PixelShader> _imgui_pixel_shader;
		com_ptr<ID3D11InputLayout> _imgui_input_layout;
		com_ptr<ID3D11Buffer> _imgui_constant_buffer;
		com_ptr<ID3D11SamplerState> _imgui_texture_sampler;
		com_ptr<ID3D11RasterizerState> _imgui_rasterizer_state;
		com_ptr<ID3D11BlendState> _imgui_blend_state;
		com_ptr<ID3D11DepthStencilState> _imgui_depthstencil_state;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;
		draw_call_tracker _current_tracker;

		HMODULE _d3d_compiler = nullptr;
	};
}
