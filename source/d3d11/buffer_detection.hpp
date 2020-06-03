/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <d3d11.h>
#include "com_ptr.hpp"

namespace reshade::d3d11
{
	class buffer_detection
	{
	public:
		void init(ID3D11DeviceContext *device, const class buffer_detection_context *context);
		void reset();

		void merge(const buffer_detection &source);

		void on_draw(UINT vertices);

#if RESHADE_DEPTH
		void before_draw(UINT vertices);
		void on_OM_set_render_targets(ID3D11DepthStencilView *dsv);
		void on_clear_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv);
#endif

#if RESHADE_WIREFRAME
		const bool get_wireframe_mode();
#endif

	protected:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};
		struct depthstencil_info
		{
			draw_stats total_stats;
			draw_stats current_stats; // Stats since last clear
			draw_stats rect_stats; // Output merger stats since last OM binding
			std::vector<draw_stats> clears;
			std::vector<draw_stats> rectangles;
		};

		ID3D11DeviceContext *_device = nullptr;
		const buffer_detection_context *_context = nullptr;
		draw_stats _stats;
#if RESHADE_DEPTH
		draw_stats _best_copy_stats;
		bool _preserve_cleared_depth_buffers = false;
		bool _preserve_depth_buffers_hidden_by_rectangle = false;
		bool _has_indirect_drawcalls = false;
		bool _depth_stencil_cleared = false;
		bool _depth_buffer_cleared = false;
		bool _new_OM_stage = false;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};

	class buffer_detection_context : public buffer_detection
	{
		friend class buffer_detection;

	public:
		explicit buffer_detection_context(ID3D11DeviceContext *context) { init(context, nullptr); }

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

#if RESHADE_WIREFRAME
		void set_wireframe_mode(bool value);
#endif

#if RESHADE_DEPTH
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		UINT current_hidden_by_rectangle_index() const { return _depthstencil_hidden_by_rectangle_index.second; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		ID3D11Texture2D *current_depth_texture() const { return _depthstencil_clear_index.first; }
		ID3D11Texture2D *current_hidden_by_rectangle_depth_texture() const { return _depthstencil_hidden_by_rectangle_index.first; }

		com_ptr<ID3D11Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D11Texture2D> override = nullptr, UINT clear_index_override = 0, UINT rectangle_index_override = 0);
#endif

	private:
#if RESHADE_DEPTH
		bool update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc);
		bool update_depthstencil_hidden_by_rectangle_texture(D3D11_TEXTURE2D_DESC desc);

		draw_stats _previous_stats;
		com_ptr<ID3D11Texture2D> _depthstencil_clear_texture;
		com_ptr<ID3D11Texture2D> _depthstencil_hidden_by_rectangle_texture;
		std::pair<ID3D11Texture2D *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		std::pair<ID3D11Texture2D *, UINT> _depthstencil_hidden_by_rectangle_index = { nullptr, std::numeric_limits<UINT>::max() };
#endif

#if RESHADE_WIREFRAME
		bool _wireframe_mode;
#endif
	};
}
