/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_block.hpp"
#include "draw_call_tracker.hpp"

namespace reshade { enum class texture_reference; }
namespace reshadefx { struct sampler_info; }

namespace reshade::d3d9
{
	class runtime_d3d9 : public runtime
	{
	public:
		runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
		~runtime_d3d9();

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset();
		void on_present(draw_call_tracker &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_backbuffer_texture();
		bool init_default_depth_stencil();
		bool init_fullscreen_triangle_resources();

		bool init_texture(texture &info) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;
		bool update_texture_reference(texture &texture);
		void update_texture_references(texture_reference type);

		bool compile_effect(effect_data &effect) override;

		bool add_sampler(const reshadefx::sampler_info &info, struct d3d9_technique_data &technique_init);
		bool init_technique(technique &info, const struct d3d9_technique_data &technique_init, const std::unordered_map<std::string, com_ptr<IUnknown>> &entry_points);

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

#if RESHADE_DX9_CAPTURE_DEPTH_BUFFERS
		void update_depthstencil_texture(com_ptr<IDirect3DSurface9> depthstencil);

		IDirect3DSurface9 *_depthstencil_override = nullptr;
#endif

		com_ptr<IDirect3D9> _d3d;
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DSwapChain9> _swapchain;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DSurface9> _depthstencil;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;

		unsigned int _num_samplers;
		unsigned int _num_simultaneous_rendertargets;
		unsigned int _behavior_flags;
		bool _is_multisampling_enabled = false;
		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		state_block _app_state;
		com_ptr<IDirect3DSurface9> _default_depthstencil;

		com_ptr<IDirect3DVertexBuffer9> _effect_triangle_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_triangle_layout;

		HMODULE _d3d_compiler = nullptr;

		draw_call_tracker *_current_tracker = nullptr;

#if RESHADE_GUI
		com_ptr<IDirect3DStateBlock9> _imgui_state;
		com_ptr<IDirect3DIndexBuffer9> _imgui_index_buffer;
		com_ptr<IDirect3DVertexBuffer9> _imgui_vertex_buffer;
		int _imgui_index_buffer_size = 0;
		int _imgui_vertex_buffer_size = 0;
#endif
	};
}
