#pragma once

#include <d3d11.h>
#include <map>
#include "com_ptr.hpp"

namespace reshade::d3d11
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
		struct intermediate_snapshot_info
		{
			ID3D11DepthStencilView *depthstencil = nullptr; // No need to use a 'com_ptr' here since '_counters_per_used_depthstencil' already keeps a reference
			draw_stats stats;
			com_ptr<ID3D11Texture2D> texture;
			std::map<ID3D11RenderTargetView *, draw_stats> additional_views;
		};

		UINT total_vertices() const { return _global_counter.vertices; }
		UINT total_drawcalls() const { return _global_counter.drawcalls; }

		const auto &depthstencil_counters() const { return _counters_per_used_depthstencil; }
		const auto &cleared_depth_textures() const { return _cleared_depth_textures; }
		const auto &constant_counters() const { return _counters_per_constant_buffer; }

		void merge(const draw_call_tracker &source);
		void reset();

		void on_map(ID3D11Resource *pResource);
		void on_draw(ID3D11DeviceContext *context, UINT vertices);

		bool check_depthstencil(ID3D11DepthStencilView *depthstencil) const;
		void track_rendertargets(ID3D11DepthStencilView *depthstencil, UINT num_views, ID3D11RenderTargetView *const *views);
		void track_depth_texture(UINT index, com_ptr<ID3D11Texture2D> src_texture, com_ptr<ID3D11DepthStencilView> src_depthstencil, com_ptr<ID3D11Texture2D> dest_texture, bool cleared);

		void keep_cleared_depth_textures();

		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height, DXGI_FORMAT format);
		ID3D11Texture2D *find_best_cleared_depth_buffer_texture(DXGI_FORMAT format, UINT depth_buffer_clearing_number);

	private:
		struct depth_texture_save_info
		{
			com_ptr<ID3D11Texture2D> src_texture;
			com_ptr<ID3D11DepthStencilView> src_depthstencil;
			D3D11_TEXTURE2D_DESC src_texture_desc;
			com_ptr<ID3D11Texture2D> dest_texture;
			bool cleared = false;
		};

		draw_stats _global_counter;
		// Use "std::map" instead of "std::unordered_map" so that the iteration order is guaranteed
		std::map<com_ptr<ID3D11DepthStencilView>, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
		std::map<UINT, depth_texture_save_info> _cleared_depth_textures;
	};
}
