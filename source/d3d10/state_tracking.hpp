/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <d3d10_1.h>
#include "com_ptr.hpp"

namespace reshade::d3d10
{
	class state_tracking
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			bool rect = false;
		};
		struct depthstencil_info
		{
			draw_stats total_stats;
			draw_stats current_stats; // Stats since last clear
			std::vector<draw_stats> clears;
		};

		explicit state_tracking(ID3D10Device *device) : _device(device) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

		void on_draw(UINT vertices);
#if RESHADE_DEPTH
		void on_clear_depthstencil(UINT clear_flags, ID3D10DepthStencilView *dsv, bool rect_draw_call = false);

		// Detection Settings
		bool preserve_depth_buffers = false;
		bool use_aspect_ratio_heuristics = true;
		std::pair<ID3D10Texture2D *, UINT> depthstencil_clear_index = { nullptr, 0 };

		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }

		com_ptr<ID3D10Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D10Texture2D> override = nullptr);
#endif

	private:
		draw_stats _stats;
		ID3D10Device *const _device;

#if RESHADE_DEPTH
		bool update_depthstencil_clear_texture(D3D10_TEXTURE2D_DESC desc);

		draw_stats _previous_stats;
		draw_stats _best_copy_stats;
		bool _first_empty_stats = true;
		com_ptr<ID3D10Texture2D> _depthstencil_clear_texture;
		std::unordered_map<com_ptr<ID3D10Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};
}
