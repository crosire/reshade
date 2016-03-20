#pragma once

#include "runtime.hpp"
#include "utils\com_ptr.hpp"

#include <d3d9.h>

namespace reshade
{
	struct d3d9_texture : texture
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};
	struct d3d9_sampler
	{
		DWORD States[12];
		d3d9_texture *Texture;
	};
	struct d3d9_pass : technique::pass
	{
		d3d9_pass() : samplers(), sampler_count(0), render_targets() { }

		com_ptr<IDirect3DVertexShader9> vertex_shader;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		d3d9_sampler samplers[16];
		DWORD sampler_count;
		com_ptr<IDirect3DStateBlock9> stateblock;
		IDirect3DSurface9 *render_targets[8];
	};

	class d3d9_runtime : public runtime
	{
	public:
		d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset() override;
		void on_present() override;
		void on_draw_call(D3DPRIMITIVETYPE type, UINT count);
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique &technique) override;
		void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);

		static void update_texture_datatype(texture *texture, texture::datatype source, const com_ptr<IDirect3DTexture9> &newtexture);

		com_ptr<IDirect3D9> _d3d;
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DSwapChain9> _swapchain;

		com_ptr<IDirect3DSurface9> _backbuffer, _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;
		UINT _constant_register_count;

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
		bool create_depthstencil_replacement(IDirect3DSurface9 *depthstencil);

		UINT _behavior_flags, _num_simultaneous_rendertargets;
		bool _is_multisampling_enabled;
		D3DFORMAT _backbuffer_format;
		com_ptr<IDirect3DStateBlock9> _stateblock;
		com_ptr<IDirect3DSurface9> _depthstencil, _depthstencil_replacement, _default_depthstencil;
		std::unordered_map<IDirect3DSurface9 *, depth_source_info> _depth_source_table;

		com_ptr<IDirect3DVertexBuffer9> _effect_triangle_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_triangle_layout;
	};
}
