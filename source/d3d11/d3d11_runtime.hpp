#pragma once

#include <d3d11_3.h>

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "com_ptr.hpp"
#include "critical_section.hpp"
#include "d3d11_stateblock.hpp"

namespace reshade
{
	struct d3d11_texture : texture
	{
		size_t shader_register = 0;
		com_ptr<ID3D11Texture2D> texture;
		com_ptr<ID3D11ShaderResourceView> srv[2];
		com_ptr<ID3D11RenderTargetView> rtv[2];
	};
	struct d3d11_pass : technique::pass
	{
		com_ptr<ID3D11VertexShader> vertex_shader;
		com_ptr<ID3D11PixelShader> pixel_shader;
		com_ptr<ID3D11BlendState> blend_state;
		com_ptr<ID3D11DepthStencilState> depth_stencil_state;
		UINT stencil_reference;
		bool clear_render_targets;
		ID3D11RenderTargetView *render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11ShaderResourceView *render_target_resources[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		D3D11_VIEWPORT viewport;
		std::vector<ID3D11ShaderResourceView *> shader_resources;
	};

	class d3d11_runtime : public runtime
	{
	public:
		d3d11_runtime(ID3D11Device *device, IDXGISwapChain *swapchain);

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_draw_call(ID3D11DeviceContext *context, unsigned int vertices);
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique &technique) override;
		void on_set_depthstencil_view(ID3D11DepthStencilView *&depthstencil);
		void on_get_depthstencil_view(ID3D11DepthStencilView *&depthstencil);
		void on_clear_depthstencil_view(ID3D11DepthStencilView *&depthstencil);
		void on_copy_resource(ID3D11Resource *&dest, ID3D11Resource *&source);

		void update_texture_datatype(d3d11_texture &texture, texture_type source, const com_ptr<ID3D11ShaderResourceView> &srv, const com_ptr<ID3D11ShaderResourceView> &srvSRGB);

		com_ptr<ID3D11Device> _device;
		com_ptr<ID3D11DeviceContext> _immediate_context;
		com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D11Texture2D> _backbuffer_texture;
		com_ptr<ID3D11ShaderResourceView> _backbuffer_texture_srv[2];
		com_ptr<ID3D11RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D11ShaderResourceView> _depthstencil_texture_srv;
		std::vector<ID3D11SamplerState *> _effect_sampler_states;
		std::vector<ID3D11ShaderResourceView *> _effect_shader_resources;
		com_ptr<ID3D11Buffer> _constant_buffer;
		UINT _constant_buffer_size = 0;

	private:
		struct depth_source_info
		{
			UINT width, height;
			FLOAT drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();
		bool init_imgui_resources();
		bool init_imgui_font_atlas();

		void screenshot(uint8_t *buffer) const override;
		bool update_effect(const fx::syntax_tree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;

		void render_draw_lists(ImDrawData *data) override;

		void detect_depth_source();
		bool create_depthstencil_replacement(ID3D11DepthStencilView *depthstencil);

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		utils::d3d11_stateblock _stateblock;
		com_ptr<ID3D11Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D11DepthStencilView> _depthstencil, _depthstencil_replacement;
		com_ptr<ID3D11Texture2D> _depthstencil_texture;
		com_ptr<ID3D11DepthStencilView> _default_depthstencil;
		std::unordered_map<ID3D11DepthStencilView *, depth_source_info> _depth_source_table;
		com_ptr<ID3D11VertexShader> _copy_vertex_shader;
		com_ptr<ID3D11PixelShader> _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler;
		utils::critical_section _cs;
		com_ptr<ID3D11RasterizerState> _effect_rasterizer_state;

		com_ptr<ID3D11Buffer> _imgui_vertex_buffer, _imgui_index_buffer;
		com_ptr<ID3D11VertexShader> _imgui_vertex_shader;
		com_ptr<ID3D11PixelShader> _imgui_pixel_shader;
		com_ptr<ID3D11InputLayout> _imgui_input_layout;
		com_ptr<ID3D11Buffer> _imgui_constant_buffer;
		com_ptr<ID3D11SamplerState> _imgui_texture_sampler;
		com_ptr<ID3D11RasterizerState> _imgui_rasterizer_state;
		com_ptr<ID3D11BlendState> _imgui_blend_state;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;
	};
}