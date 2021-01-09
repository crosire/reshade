/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <d3d11_4.h>
#include "com_ptr.hpp"

namespace reshade::d3d11
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

		void init(ID3D11DeviceContext *device_context, const class state_tracking_context *context);
		void reset();

		void merge(const state_tracking &source);

		void on_draw(UINT vertices);
#if RESHADE_DEPTH
		void on_set_render_targets();
		void on_clear_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv, bool rect_draw_call = false);
#endif

	protected:
		draw_stats _stats;
		ID3D11DeviceContext *_device_context = nullptr;
		const state_tracking_context *_context = nullptr;
#if RESHADE_DEPTH
		draw_stats _best_copy_stats;
		bool _first_empty_stats = true;
		bool _has_indirect_drawcalls = false;
		std::unordered_map<com_ptr<ID3D11Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};

	class state_tracking_context : public state_tracking
	{
		friend class state_tracking;

	public:
		explicit state_tracking_context(ID3D11DeviceContext *context) { init(context, nullptr); }

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

#if RESHADE_DEPTH
		// Detection Settings
		bool preserve_depth_buffers = false;
		bool use_aspect_ratio_heuristics = true;
		std::pair<ID3D11Texture2D *, UINT> depthstencil_clear_index = { nullptr, 0 };

		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		std::vector<std::pair<ID3D11Texture2D*, depthstencil_info>> sorted_counters_per_used_depthstencil();

		com_ptr<ID3D11Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D11Texture2D> override = nullptr);
#endif

	private:
#if RESHADE_DEPTH
		bool update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc);

		draw_stats _previous_stats;
		com_ptr<ID3D11Texture2D> _depthstencil_clear_texture;
		std::unordered_map<ID3D11Texture2D *, int> _shown_count_per_depthstencil_address;
#endif
	};
}
