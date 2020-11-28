/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <d3d9.h>
#include "com_ptr.hpp"

namespace reshade::d3d9
{
	class state_tracking
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};
		struct clear_stats : public draw_stats
		{
			D3DVIEWPORT9 viewport = {};
		};
		struct depthstencil_info
		{
			draw_stats total_stats;
			clear_stats current_stats; // Stats since last clear
			std::vector<clear_stats> clears;
		};

		explicit state_tracking(IDirect3DDevice9 *device) : _device(device) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

		void on_draw(D3DPRIMITIVETYPE type, UINT primitives);
#if RESHADE_DEPTH
		void on_set_depthstencil(IDirect3DSurface9 *&depthstencil);
		void on_get_depthstencil(IDirect3DSurface9 *&depthstencil);
		void on_clear_depthstencil(UINT clear_flags);

		// Detection Settings
		bool disable_intz = false;
		bool preserve_depth_buffers = false;
		bool use_aspect_ratio_heuristics = true;
		UINT depthstencil_clear_index = 0;

		const auto &depth_buffer_counters() const { return _counters_per_used_depth_surface; }
		IDirect3DSurface9 *current_depth_surface() const { return _depthstencil_original.get(); }
		IDirect3DSurface9 *current_depth_replacement() const { return _depthstencil_replacement[0].get(); }

		com_ptr<IDirect3DSurface9> find_best_depth_surface(UINT width, UINT height,
			com_ptr<IDirect3DSurface9> override = nullptr);
#endif

	private:
		draw_stats _stats;
		IDirect3DDevice9 *const _device;

#if RESHADE_DEPTH
		bool check_aspect_ratio(UINT width_to_check, UINT height_to_check, UINT width, UINT height);
		bool check_texture_format(const D3DSURFACE_DESC &desc);

		bool update_depthstencil_replacement(com_ptr<IDirect3DSurface9> depthstencil, size_t index);

		com_ptr<IDirect3DSurface9> _depthstencil_original;
		std::vector<com_ptr<IDirect3DSurface9>> _depthstencil_replacement;
		std::unordered_map<com_ptr<IDirect3DSurface9>, depthstencil_info> _counters_per_used_depth_surface;
#endif
	};
}
