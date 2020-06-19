/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vector>
#include <d3d9.h>
#include "com_ptr.hpp"

namespace reshade::d3d9
{
	class buffer_detection
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};
		struct depthstencil_info
		{
			draw_stats total_stats;
			draw_stats current_stats; // Stats since last clear
			std::vector<draw_stats> clears;
		};

		explicit buffer_detection(IDirect3DDevice9 *device) : _device(device) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

		void on_draw(D3DPRIMITIVETYPE type, UINT primitives);
#if RESHADE_DEPTH
		void on_set_depthstencil(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil(IDirect3DSurface9 *&depthstencil);
		void on_clear_depthstencil(UINT clear_flags);
		void after_clear_depthstencil();

		// Detection Settings
		bool disable_intz = false;
		bool preserve_depth_buffers = false;
		std::pair<com_ptr<IDirect3DSurface9>, UINT> depthstencil_clear_index = { nullptr, 0 };
		UINT internal_clear_index = 0;

		const auto &depth_buffer_counters() const { return _counters_per_used_depth_surface; }
		IDirect3DSurface9 *current_depth_surface() const { return _depthstencil_original.get(); }
		IDirect3DSurface9 *current_depth_replacement() const { return _depthstencil_replacement.get(); }

		com_ptr<IDirect3DSurface9> find_best_depth_surface(UINT width, UINT height,
			com_ptr<IDirect3DSurface9> override = nullptr);
#endif

#if RESHADE_WIREFRAME
		const bool get_wireframe_mode();
		void set_wireframe_mode(bool value);
#endif

	private:
		draw_stats _stats;
		IDirect3DDevice9 *const _device;

#if RESHADE_DEPTH
		bool check_texture_format(const D3DSURFACE_DESC &desc);

		bool update_depthstencil_replacement(com_ptr<IDirect3DSurface9> depthstencil);

		draw_stats _previous_stats;
		draw_stats _best_copy_stats;
		bool _first_empty_stats = true;
		bool _depth_stencil_cleared = false;
		com_ptr<IDirect3DSurface9> _depthstencil_original;
		com_ptr<IDirect3DSurface9> _depthstencil_replacement;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<IDirect3DSurface9>, depthstencil_info> _counters_per_used_depth_surface;
#endif

#if RESHADE_WIREFRAME
		bool _wireframe_mode;
#endif
	};
}
