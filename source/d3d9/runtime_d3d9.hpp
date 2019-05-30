/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "effect_expression.hpp"
#include "state_block.hpp"

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
		void on_present();

		void on_draw_primitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
		void on_draw_indexed_primitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount);
		void on_draw_primitive_up(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
		void on_draw_indexed_primitive_up(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
		void on_draw_call(com_ptr<IDirect3DSurface9> depthstencil, D3DPRIMITIVETYPE type, unsigned int count);

		void on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil);
		void on_clear_depthstencil_surface(IDirect3DSurface9 *depthstencil);

		void capture_screenshot(uint8_t *buffer) const override;

	private:
		struct depth_source_info
		{
			com_ptr<IDirect3DSurface9> depthstencil;
			UINT width, height;
			UINT drawcall_count, vertices_count;
		};

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

		bool check_depthstencil_size(const D3DSURFACE_DESC &desc);
		bool check_depthstencil_size(const D3DSURFACE_DESC &desc, const D3DSURFACE_DESC &compared_desc);

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

		void detect_depth_source();
		bool create_depthstencil_replacement(const com_ptr<IDirect3DSurface9> &depthstencil);

		void weapon_or_cockpit_fix(const com_ptr<IDirect3DSurface9> depthstencil, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
		void weapon_or_cockpit_fix(const com_ptr<IDirect3DSurface9> depthstencil, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount);
		void create_fixed_viewport(const D3DVIEWPORT9 mViewport);

		com_ptr<IDirect3D9> _d3d;
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DSwapChain9> _swapchain;

		com_ptr<IDirect3DSurface9> _backbuffer;
		com_ptr<IDirect3DSurface9> _backbuffer_resolved;
		com_ptr<IDirect3DTexture9> _backbuffer_texture;
		com_ptr<IDirect3DSurface9> _backbuffer_texture_surface;
		com_ptr<IDirect3DTexture9> _depthstencil_texture;

		unsigned int _num_samplers;
		unsigned int _num_simultaneous_rendertargets;
		unsigned int _behavior_flags;
		bool _disable_intz = false;
		bool _preserve_depth_buffer = false;
		bool _auto_preserve = true;
		size_t _preserve_starting_index = 0;
		size_t _adjusted_preserve_starting_index = 0;
		bool _source_engine_fix = false;
		bool _brute_force_fix = false;
		bool _focus_on_best_original_depthstencil_source = false;
		bool _disable_depth_buffer_size_restriction = false;
		bool _init_depthbuffer_detection = true;
		bool _is_good_viewport = true;
		bool _is_best_original_depthstencil_source = true;
		unsigned int _db_vertices = 0;
		unsigned int _db_drawcalls = 0;
		unsigned int _current_db_vertices = 0;
		unsigned int _current_db_drawcalls = 0;
		bool _is_multisampling_enabled = false;
		D3DFORMAT _backbuffer_format = D3DFMT_UNKNOWN;
		state_block _app_state;
		com_ptr<IDirect3DSurface9> _depthstencil;
		com_ptr<IDirect3DSurface9> _depthstencil_replacement;
		com_ptr<IDirect3DSurface9> _default_depthstencil;
		std::unordered_map<com_ptr<IDirect3DSurface9>, depth_source_info> _depth_source_table;
		std::vector<depth_source_info> _depth_buffer_table;

		com_ptr<IDirect3DVertexBuffer9> _effect_triangle_buffer;
		com_ptr<IDirect3DVertexDeclaration9> _effect_triangle_layout;

		com_ptr<IDirect3DStateBlock9> _imgui_state;
		com_ptr<IDirect3DVertexBuffer9> _imgui_vertex_buffer;
		com_ptr<IDirect3DIndexBuffer9> _imgui_index_buffer;
		int _imgui_vertex_buffer_size = 0, _imgui_index_buffer_size = 0;

		HMODULE _d3d_compiler = nullptr;
	};
}
