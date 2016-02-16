#pragma once

#include "runtime.hpp"
#include "utils\com_ptr.hpp"
#include "utils\d3d10_stateblock.hpp"

#include <algorithm>
#include <d3d10_1.h>

namespace reshade
{
	class d3d10_runtime : public runtime
	{
	public:
		d3d10_runtime(ID3D10Device *device, IDXGISwapChain *swapchain);

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_draw_call(UINT vertices) override;
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique *technique) override;
		void on_set_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_get_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_clear_depthstencil_view(ID3D10DepthStencilView *&depthstencil);
		void on_copy_resource(ID3D10Resource *&dest, ID3D10Resource *&source);

		void update_texture_datatype(texture *texture, texture::datatype source, const com_ptr<ID3D10ShaderResourceView> &srv, const com_ptr<ID3D10ShaderResourceView> &srvSRGB);

		com_ptr<ID3D10Device> _device;
		com_ptr<IDXGISwapChain> _swapchain;

		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv[2], _depthstencil_texture_srv;
		std::vector<ID3D10SamplerState *> _effect_sampler_states;
		std::vector<ID3D10ShaderResourceView *> _effect_shader_resources;
		com_ptr<ID3D10Buffer> _constant_buffer;
		UINT _constant_buffer_size;

	private:
		struct depth_source_info
		{
			UINT width, height;
			FLOAT drawcall_count, vertices_count;
		};

		void screenshot(unsigned char *buffer) const override;
		bool update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
		bool update_texture(texture *texture, const unsigned char *data, size_t size) override;

		void detect_depth_source();
		bool create_depthstencil_replacement(ID3D10DepthStencilView *depthstencil);

		bool _is_multisampling_enabled;
		DXGI_FORMAT _backbuffer_format;
		utils::d3d10_stateblock _stateblock;
		com_ptr<ID3D10Texture2D> _backbuffer, _backbuffer_resolved;
		com_ptr<ID3D10DepthStencilView> _depthstencil, _depthstencil_replacement;
		com_ptr<ID3D10Texture2D> _depthstencil_texture;
		com_ptr<ID3D10DepthStencilView> _default_depthstencil;
		std::unordered_map<ID3D10DepthStencilView *, depth_source_info> _depth_source_table;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer_state;
	};
}