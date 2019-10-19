#pragma once

#include <map>
#include <d3d12.h>
#include "com_ptr.hpp"

#define RESHADE_DX12_CAPTURE_DEPTH_BUFFERS 1

namespace reshade::d3d12
{
	class draw_call_tracker
	{
	public:
		struct draw_stats
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
			UINT mapped = 0;
			UINT vs_uses = 0;
			UINT ps_uses = 0;
		};

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		struct intermediate_snapshot_info
		{
			ID3D12Resource *depthstencil = nullptr; // No need to use a 'com_ptr' here since '_counters_per_used_depthstencil' already keeps a reference
			draw_stats stats;
			std::map<ID3D12Resource *, draw_stats> additional_views;
		};
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depthstencil; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }
#endif

		void merge(const draw_call_tracker &source);
		void reset();

		void on_draw(UINT vertices);

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void track_rendertargets(int format_index, ID3D12Resource *depthstencil);
		void track_depth_texture(int format_index, UINT index, com_ptr<ID3D12Resource> src_texture, com_ptr<ID3D12Resource> dest_texture, bool cleared);

		void keep_cleared_depth_textures();

		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height);
		ID3D12Resource *find_best_cleared_depth_buffer_texture(UINT clear_index);
#endif

		com_ptr<ID3D12Resource> _current_depthstencil;
		
	private:
		struct depth_texture_save_info
		{
			com_ptr<ID3D12Resource> src_texture;
			D3D12_RESOURCE_DESC src_texture_desc;
			com_ptr<ID3D12Resource> dest_texture;
			bool cleared = false;
		};

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		bool check_depthstencil(ID3D12Resource *depthstencil) const;
		bool check_depth_texture_format(int format_index, ID3D12Resource *depthstencil);
#endif

		draw_stats _global_counter;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D12Resource>, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<UINT, depth_texture_save_info> _cleared_depth_textures;
#endif
	};
}
