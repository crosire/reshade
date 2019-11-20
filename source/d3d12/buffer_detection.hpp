/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <unordered_map>
#include <d3d12.h>
#include "com_ptr.hpp"

#define RESHADE_DX12_CAPTURE_DEPTH_BUFFERS 1

namespace reshade::d3d12
{
	class buffer_detection
	{
	public:
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		static bool filter_aspect_ratio;
		static bool preserve_depth_buffers;
		static unsigned int filter_depth_texture_format;
#endif

		buffer_detection(ID3D12Device *device, buffer_detection *tracker) :
			_device(device), _device_tracker(tracker) {}

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		ID3D12Resource *current_depth_texture() const { return _depthstencil_clear_index.first; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
#endif

		void reset(bool release_resources);

		void merge(const buffer_detection &source);

		void on_draw(UINT vertices);

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void on_create_dsv(ID3D12Resource *dsv_texture, D3D12_CPU_DESCRIPTOR_HANDLE handle);

		void track_render_targets(D3D12_CPU_DESCRIPTOR_HANDLE dsv);
		void track_cleared_depthstencil(ID3D12GraphicsCommandList *cmd_list, D3D12_CLEAR_FLAGS clear_flags, D3D12_CPU_DESCRIPTOR_HANDLE dsv);

		bool update_depthstencil_clear_texture(D3D12_RESOURCE_DESC desc);

		static bool check_aspect_ratio(const D3D12_RESOURCE_DESC &desc, UINT width, UINT height);
		static bool check_texture_format(const D3D12_RESOURCE_DESC &desc);

		com_ptr<ID3D12Resource> find_best_depth_texture(UINT width, UINT height,
			com_ptr<ID3D12Resource> override = nullptr, UINT clear_index_override = 0);
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

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D12Resource> resource_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
#endif

		ID3D12Device *const _device;
		buffer_detection *const _device_tracker;
		draw_stats _stats;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		draw_stats _clear_stats;
		com_ptr<ID3D12Resource> _current_depthstencil;
		com_ptr<ID3D12Resource> _depthstencil_clear_texture;
		std::pair<ID3D12Resource *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		// Do not hold a reference to the resources here
		std::unordered_map<SIZE_T, ID3D12Resource *> _depthstencil_resources_by_handle;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D12Resource>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};
}
