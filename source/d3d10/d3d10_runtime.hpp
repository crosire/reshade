/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d10_1.h>
#include "runtime.hpp"
#include "d3d10_stateblock.hpp"

namespace reshade::d3d10
{
	struct d3d10_tex_data : base_object
	{
		com_ptr<ID3D10Texture2D> texture;
		com_ptr<ID3D10ShaderResourceView> srv[2];
		com_ptr<ID3D10RenderTargetView> rtv[2];
	};
	struct d3d10_pass_data : base_object
	{
		com_ptr<ID3D10VertexShader> vertex_shader;
		com_ptr<ID3D10PixelShader> pixel_shader;
		com_ptr<ID3D10BlendState> blend_state;
		com_ptr<ID3D10DepthStencilState> depth_stencil_state;
		UINT stencil_reference;
		bool clear_render_targets;
		ID3D10RenderTargetView *render_targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D10ShaderResourceView *render_target_resources[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		D3D10_VIEWPORT viewport;
		std::vector<ID3D10ShaderResourceView *> shader_resources;
	};

	class d3d10_runtime : public runtime
	{
	public:
		d3d10_runtime(ID3D10Device1 *device, IDXGISwapChain *swapchain);

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_reset_effect() override;
		void on_present();
		void on_draw_call(UINT vertices);
		void on_set_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_get_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_clear_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_copy_resource(ID3D10Resource *&dest, ID3D10Resource *&source);

		void capture_frame(uint8_t *buffer) const override;
		bool load_effect(const reshadefx::syntax_tree &ast, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;

		void render_technique(const technique &technique) override;
		void render_imgui_draw_data(ImDrawData *data) override;

		com_ptr<ID3D10Device1> _device;
		com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2], _depthstencil_texture_srv;
		std::vector<ID3D10SamplerState *> _effect_sampler_states;
		std::unordered_map<size_t, size_t> _effect_sampler_descs;
		std::vector<ID3D10ShaderResourceView *> _effect_shader_resources;
		std::vector<com_ptr<ID3D10Buffer>> _constant_buffers;

	private:
		struct depth_source_info
		{
			UINT width, height;
			UINT drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();
		bool init_imgui_resources();
		bool init_imgui_font_atlas();

		void detect_depth_source();
		bool create_depthstencil_replacement(ID3D10DepthStencilView *depthstencil);

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		d3d10_stateblock _stateblock;
		com_ptr<ID3D10Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D10DepthStencilView> _depthstencil, _depthstencil_replacement;
		com_ptr<ID3D10Texture2D> _depthstencil_texture;
		com_ptr<ID3D10DepthStencilView> _default_depthstencil;
		std::unordered_map<ID3D10DepthStencilView *, depth_source_info> _depth_source_table;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer_state;

		com_ptr<ID3D10Buffer> _imgui_vertex_buffer, _imgui_index_buffer;
		com_ptr<ID3D10VertexShader> _imgui_vertex_shader;
		com_ptr<ID3D10PixelShader> _imgui_pixel_shader;
		com_ptr<ID3D10InputLayout> _imgui_input_layout;
		com_ptr<ID3D10Buffer> _imgui_constant_buffer;
		com_ptr<ID3D10SamplerState> _imgui_texture_sampler;
		com_ptr<ID3D10RasterizerState> _imgui_rasterizer_state;
		com_ptr<ID3D10BlendState> _imgui_blend_state;
		com_ptr<ID3D10DepthStencilState> _imgui_depthstencil_state;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;
	};
}
