#pragma once

#include <d3d9.h>

#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "com_ptr.hpp"

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
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		d3d9_sampler samplers[16] = { };
		DWORD sampler_count = 0;
		com_ptr<IDirect3DStateBlock9> stateblock;
		bool clear_render_targets = false;
		IDirect3DSurface9 *render_targets[8] = { };
	};

	class d3d9_runtime : public runtime
	{
	public:
		d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_draw_call(D3DPRIMITIVETYPE type, UINT count);
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique &technique) override;
		void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);

		static void update_texture_datatype(d3d9_texture &texture, texture_type source, const com_ptr<IDirect3DTexture9> &newtexture);

		com_ptr<IDirect3D9> _d3d;
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DSwapChain9> _swapchain;

		com_ptr<IDirect3DSurface9> _backbuffer, _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;
		UINT _constant_register_count = 0;

	private:
		struct depth_source_info
		{
			UINT width, height;
			FLOAT drawcall_count, vertices_count;
		};

		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fx_resources();
		bool init_imgui_font_atlas();

		void screenshot(unsigned char *buffer) const override;
		bool update_effect(const fx::syntax_tree &ast, const std::vector<std::string> &pragmas, std::string &errors) override;
		bool update_texture(texture &texture, const unsigned char *data, size_t size) override;

		void render_draw_lists(ImDrawData *data) override;

		void detect_depth_source();
		bool create_depthstencil_replacement(IDirect3DSurface9 *depthstencil);

		UINT _behavior_flags, _num_simultaneous_rendertargets;
		bool _is_multisampling_enabled = false;
		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DStateBlock9> _stateblock;
		com_ptr<IDirect3DSurface9> _depthstencil, _depthstencil_replacement, _default_depthstencil;
		std::unordered_map<IDirect3DSurface9 *, depth_source_info> _depth_source_table;

		com_ptr<IDirect3DVertexBuffer9> _effect_triangle_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_triangle_layout;

		com_ptr<IDirect3DVertexBuffer9> _imgui_vertex_buffer;
		com_ptr<IDirect3DIndexBuffer9> _imgui_index_buffer;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;
	};
}
