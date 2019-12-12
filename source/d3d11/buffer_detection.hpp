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
	class buffer_detection
	{
	public:
		void init(ID3D11DeviceContext *device, const class buffer_detection_context *context = nullptr);
		void reset();

		void merge(const buffer_detection &source);

		void on_map(ID3D11Resource *pResource);
		void on_draw(UINT vertices);

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		void on_clear_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv);
#endif

	protected:
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
			draw_stats total_stats;
			draw_stats current_stats; // Stats since last clear
			std::vector<draw_stats> clears;
		};

		ID3D11DeviceContext *_device = nullptr;
		const buffer_detection_context *_context = nullptr;
		draw_stats _stats;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		draw_stats _best_copy_stats;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11Texture2D>, depthstencil_info> _counters_per_used_depth_texture;
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
#endif
	};

	class buffer_detection_context : public buffer_detection
	{
		friend class buffer_detection;

	public:
		explicit buffer_detection_context(ID3D11DeviceContext *context) { init(context); }

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		ID3D11Texture2D *current_depth_texture() const { return _depthstencil_clear_index.first; }

		com_ptr<ID3D11Texture2D> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D11Texture2D> override = nullptr, UINT clear_index_override = 0);
#endif

#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		const auto &constant_buffer_counters() const { return _counters_per_constant_buffer; }
#endif

	private:
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		bool update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc);

		draw_stats _previous_stats;
		com_ptr<ID3D11Texture2D> _depthstencil_clear_texture;
		std::pair<ID3D11Texture2D *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
#endif
	};
}
