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
		struct cleared_depthstencil_info
		{
			com_ptr<ID3D12Resource> dsv_texture;
			com_ptr<ID3D12Resource> backup_texture;
		};
		struct intermediate_snapshot_info
		{
			draw_stats stats;
		};

		static bool filter_aspect_ratio;
		static bool preserve_depth_buffers;
		static bool preserve_stencil_buffers;
		static unsigned int depth_stencil_clear_index;
		static unsigned int filter_depth_texture_format;
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depth_texture; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }
#endif

		void merge(const draw_call_tracker &source);
		void reset();

		void on_draw(UINT vertices);

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void track_render_targets(com_ptr<ID3D12Resource> depthstencil);
		void track_cleared_depthstencil(ID3D12GraphicsCommandList *cmd_list, D3D12_CLEAR_FLAGS clear_flags, com_ptr<ID3D12Resource> depthstencil, UINT clear_index, class runtime_d3d12 *runtime);

		static bool check_aspect_ratio(const D3D12_RESOURCE_DESC &desc, UINT width, UINT height);
		static bool check_texture_format(const D3D12_RESOURCE_DESC &desc);

		com_ptr<ID3D12Resource> find_best_depth_texture(UINT width, UINT height);
#endif
		
	private:
		draw_stats _global_counter;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D12Resource> _current_depthstencil;
		std::map<UINT, cleared_depthstencil_info> _cleared_depth_textures;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D12Resource>, intermediate_snapshot_info> _counters_per_used_depth_texture;
#endif
	};
}
