/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <d3d11.h>
#include "com_ptr.hpp"

#define RESHADE_DX11_CAPTURE_DEPTH_BUFFERS 1
#define RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS 0

namespace reshade::d3d11
{
	class draw_call_tracker
	{
	public:
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		static bool filter_aspect_ratio;
		static bool preserve_depth_buffers;
		static unsigned int filter_depth_texture_format;
#endif

		draw_call_tracker(ID3D11DeviceContext *context, draw_call_tracker *tracker) :
			_context(context), _device_tracker(tracker) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		ID3D11Texture2D *current_depth_texture() const { return _depthstencil_clear_index.first; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		const auto &constant_buffer_counters() const { return _counters_per_constant_buffer; }
#endif

		void reset(bool release_resources);

		void merge(const draw_call_tracker &source);

		void on_map(ID3D11Resource *pResource);
		void on_draw(UINT vertices);

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		void track_render_targets(UINT num_views, ID3D11RenderTargetView *const *views, ID3D11DepthStencilView *dsv);
		void track_cleared_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv);

		bool update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc);

		static bool check_aspect_ratio(const D3D11_TEXTURE2D_DESC &desc, UINT width, UINT height);
		static bool check_texture_format(const D3D11_TEXTURE2D_DESC &desc);

		com_ptr<ID3D11Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D11Texture2D> override = nullptr, UINT clear_index_override = 0);
#endif

	private:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			UINT mapped = 0;
			UINT vs_uses = 0;
			UINT ps_uses = 0;
		};
		struct depthstencil_info
		{
			draw_stats stats;
			std::vector<draw_stats> clears;
			std::map<ID3D11RenderTargetView *, draw_stats> additional_views;
		};

		draw_call_tracker *const _device_tracker;
		ID3D11DeviceContext *const _context;
		draw_stats _stats;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		draw_stats _clear_stats;
		com_ptr<ID3D11Texture2D> _depthstencil_clear_texture;
		std::pair<ID3D11Texture2D *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
#endif
	};
}
