/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <unordered_map>
#include <d3d12.h>
#include "com_ptr.hpp"

namespace reshade::d3d12
{
	class buffer_detection
	{
	public:
		void init(ID3D12Device *device, ID3D12GraphicsCommandList *cmd_list, const class buffer_detection_context *context);
		void reset();

		void merge(const buffer_detection &source);

		void on_draw(UINT vertices);

#if RESHADE_DEPTH
		void on_set_depthstencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv);
		void on_clear_depthstencil(D3D12_CLEAR_FLAGS clear_flags, D3D12_CPU_DESCRIPTOR_HANDLE dsv);
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
			std::vector<draw_stats> clears;
		};

		ID3D12Device *_device = nullptr;
		ID3D12GraphicsCommandList *_cmd_list = nullptr;
		const buffer_detection_context *_context = nullptr;
		draw_stats _stats;
#if RESHADE_DEPTH
		draw_stats _best_copy_stats;
		com_ptr<ID3D12Resource> _current_depthstencil;
		bool _has_indirect_drawcalls = false;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D12Resource>, depthstencil_info> _counters_per_used_depth_texture;
#endif
	};

	class buffer_detection_context : public buffer_detection
	{
		friend class buffer_detection;

	public:
		explicit buffer_detection_context(ID3D12Device *device) { init(device, nullptr, nullptr); }

		UINT total_vertices() const { return _stats.vertices; }
		UINT total_drawcalls() const { return _stats.drawcalls; }

		void reset(bool release_resources);

#if RESHADE_DEPTH
		UINT current_clear_index() const { return _depthstencil_clear_index.second; }
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		ID3D12Resource *current_depth_texture() const { return _depthstencil_clear_index.first; }

		void on_create_dsv(ID3D12Resource *dsv_texture, D3D12_CPU_DESCRIPTOR_HANDLE handle);

		com_ptr<ID3D12Resource> update_depth_texture(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list,
			UINT width, UINT height,
			ID3D12Resource *override = nullptr, UINT clear_index_override = 0);
#endif

	private:
#if RESHADE_DEPTH
		bool update_depthstencil_clear_texture(ID3D12CommandQueue *queue, D3D12_RESOURCE_DESC desc);

		com_ptr<ID3D12Resource> resource_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

		draw_stats _previous_stats;
		com_ptr<ID3D12Resource> _depthstencil_clear_texture;
		std::pair<ID3D12Resource *, UINT> _depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		// Do not hold a reference to the resources here
		std::unordered_map<SIZE_T, ID3D12Resource *> _depthstencil_resources_by_handle;
#endif
	};
}
