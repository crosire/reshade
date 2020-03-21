/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <d3d9.h>
#include "com_ptr.hpp"

namespace reshade::d3d9
{
	class buffer_detection
	{
	public:
		explicit buffer_detection(IDirect3DDevice9 *device) : _device(device) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

		void on_draw(D3DPRIMITIVETYPE type, UINT primitives);

#if RESHADE_DEPTH
		bool disable_intz = false;

		UINT current_clear_index() const { return _depthstencil_clear_index; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_surface; }
		IDirect3DSurface9 *current_depth_surface() const { return _depthstencil_original.get(); }
		IDirect3DSurface9 *current_depth_replacement() const { return _depthstencil_replacement.get(); }

		void on_set_depthstencil(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil(IDirect3DSurface9 *&depthstencil);

		void on_clear_depthstencil(UINT clear_flags);

		com_ptr<IDirect3DSurface9> find_best_depth_surface(UINT width, UINT height,
			com_ptr<IDirect3DSurface9> override = nullptr, UINT clear_index_override = 0);
#endif

	private:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};
		struct depthstencil_info
		{
			draw_stats stats;
			std::vector<draw_stats> clears;
		};

#if RESHADE_DEPTH
		bool check_aspect_ratio(const D3DSURFACE_DESC &desc, UINT width, UINT height);
		bool check_texture_format(const D3DSURFACE_DESC &desc);

		bool update_depthstencil_replacement(com_ptr<IDirect3DSurface9> depthstencil);
#endif

		IDirect3DDevice9 *const _device;
		draw_stats _stats;
#if RESHADE_DEPTH
		bool _preserve_depth_buffers = false;
		draw_stats _clear_stats;
		UINT _depthstencil_clear_index = std::numeric_limits<UINT>::max();
		com_ptr<IDirect3DSurface9> _depthstencil_original;
		com_ptr<IDirect3DSurface9> _depthstencil_replacement;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<IDirect3DSurface9>, depthstencil_info> _counters_per_used_depth_surface;
#endif
	};
}
