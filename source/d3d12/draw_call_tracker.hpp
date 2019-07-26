#pragma once

#include <map>
#include "com_ptr.hpp"
#include <d3d12.h>

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

		struct depthstencil_infos
		{
			ID3D12Resource *texture = nullptr;
			D3D12_RESOURCE_DESC texture_desc;
			D3D12_HEAP_PROPERTIES texture_heapProperties;
			D3D12_HEAP_FLAGS texture_heapFlags;
			D3D12_CLEAR_VALUE texture_clearValue;
			D3D12_CPU_DESCRIPTOR_HANDLE depthstencilView;
			D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
		};

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		struct intermediate_snapshot_info
		{
			ID3D12Resource *depthstencil = nullptr; // No need to use a 'com_ptr' here since '_counters_per_used_depthstencil' already keeps a reference
			draw_stats stats;
			com_ptr<ID3D12Resource> texture;
			std::map<ID3D12Resource *, draw_stats> additional_views;
		};
#endif

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }
		const auto &depthstencil_resources_by_handle() const { return _depthstencil_resources_by_handle; }

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		const auto &depth_buffer_counters() const { return _counters_per_used_depthstencil; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }
#endif

		void merge(const draw_call_tracker &source);
		void reset(bool all = false);

		void on_map(ID3D12Resource *pResource);
		void on_draw(com_ptr<ID3D12Resource> current_depthstencil, UINT vertices);

		com_ptr<ID3D12Resource> _depthstencil;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void track_depthstencil_resource_by_handle(D3D12_CPU_DESCRIPTOR_HANDLE pDescriptor, com_ptr<ID3D12Resource> pDepthStencil);
		void track_rendertargets(int formatIdx, ID3D12Resource *depthstencil);
		void track_depth_texture(int formatIdx, UINT index, com_ptr<ID3D12Resource> srcTexture, com_ptr<ID3D12Resource> srcDepthstencil, com_ptr<ID3D12Resource> destTexture, bool cleared);

		void keep_cleared_depth_textures();

		com_ptr<ID3D12Resource> retrieve_depthstencil_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE depthstencilView);
		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height);
		ID3D12Resource *find_best_cleared_depth_buffer_texture(UINT clearIdx);
#endif
		
	private:
		struct depth_texture_save_info
		{
			com_ptr<ID3D12Resource> src_texture;
			com_ptr<ID3D12Resource> src_depthstencil;
			D3D12_RESOURCE_DESC src_texture_desc;
			com_ptr<ID3D12Resource> dest_texture;
			bool cleared = false;
		};

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		bool check_depthstencil(ID3D12Resource *depthstencil) const;
		bool check_depth_texture_format(int formatIdx, ID3D12Resource *depthstencil);
#endif

		draw_stats _global_counter;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		std::map<size_t, com_ptr<ID3D12Resource>> _depthstencil_resources_by_handle;

		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D12Resource>, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<UINT, depth_texture_save_info> _cleared_depth_textures;
#endif
	};
}
