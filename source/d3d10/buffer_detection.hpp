/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <d3d10_1.h>
#include "com_ptr.hpp"

namespace reshade::d3d10
{
	class buffer_detection
	{
	public:
		explicit buffer_detection(ID3D10Device *device) : _device(device) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

		void on_draw(UINT vertices);

#if RESHADE_DEPTH
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		ID3D10Texture2D *current_depth_texture() const { return _depthstencil_clear_index.first; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }

		void on_clear_depthstencil(UINT clear_flags, ID3D10DepthStencilView *dsv);

		com_ptr<ID3D10Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D10Texture2D> override = nullptr, UINT clear_index_override = 0);
#endif

	private:
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

#if RESHADE_DEPTH
		bool update_depthstencil_clear_texture(D3D10_TEXTURE2D_DESC desc);
#endif

		ID3D10Device *const _device;
		draw_stats _stats;
#if RESHADE_DEPTH
		draw_stats _previous_stats;
		draw_stats _best_copy_stats;
		com_ptr<ID3D10Texture2D> _depthstencil_clear_texture;
		std::pair<ID3D10Texture2D *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D10Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};
}
