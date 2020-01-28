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

namespace reshade::d3d9
{
	class runtime_d3d9 : public runtime
	{
	public:
		runtime_d3d9(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
		~runtime_d3d9();

		bool on_init(const D3DPRESENT_PARAMETERS &pp);
		void on_reset();
		void on_present(buffer_detection &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_effect(size_t index) override;

		bool init_texture(texture &info) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;

		void render_technique(technique &technique) override;

		state_block _app_state;
		com_ptr<IDirect3D9> _d3d;
		const com_ptr<IDirect3DDevice9> _device;
		const com_ptr<IDirect3DSwapChain9> _swapchain;
		unsigned int _max_vertices = 0;
		unsigned int _num_samplers;
		unsigned int _num_simultaneous_rendertargets;
		unsigned int _behavior_flags;

		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DSurface9> _depthstencil;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<IDirect3DSurface9> _effect_depthstencil;
		com_ptr<IDirect3DVertexBuffer9> _effect_vertex_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_vertex_layout;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		com_ptr<IDirect3DStateBlock9> _imgui_state;
		com_ptr<IDirect3DIndexBuffer9> _imgui_index_buffer;
		com_ptr<IDirect3DVertexBuffer9> _imgui_vertex_buffer;
		int _imgui_index_buffer_size = 0;
		int _imgui_vertex_buffer_size = 0;
#endif

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depthstencil_texture(com_ptr<IDirect3DSurface9> depthstencil);

		bool _disable_intz = false;
		bool _filter_aspect_ratio = true;
		bool _preserve_depth_buffers = false;
		UINT _depth_clear_index_override = std::numeric_limits<UINT>::max();
		IDirect3DSurface9 *_depthstencil_override = nullptr;
		buffer_detection *_current_tracker = nullptr;
#endif
	};
}
