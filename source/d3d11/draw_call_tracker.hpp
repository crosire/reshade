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
		const auto &constant_counters() const { return _counters_per_constant_buffer; }

		void merge(const draw_call_tracker &source);
		void reset();

		void on_map(ID3D11Resource *pResource);
		void on_draw(ID3D11DeviceContext *context, UINT vertices);

		bool check_depthstencil(ID3D11DepthStencilView *depthstencil) const;
		void track_rendertargets(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture, UINT num_views, ID3D11RenderTargetView *const *views);
		void update_tracked_depthtexture(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture);

		intermediate_snapshot_info find_best_snapshot(UINT width, UINT height, DXGI_FORMAT format);

	private:
		draw_stats _global_counter;

		std::map<com_ptr<ID3D11DepthStencilView>, intermediate_snapshot_info> _counters_per_used_depthstencil;
		std::map<com_ptr<ID3D11Buffer>, draw_stats> _counters_per_constant_buffer;
	};
}
