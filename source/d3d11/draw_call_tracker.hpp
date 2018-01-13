#pragma once

#include <d3d11.h>
#include <unordered_map>
#include "com_ptr.hpp"

namespace reshade::d3d11
{
	class draw_call_tracker
	{
	public:
		UINT vertices() const { return _counters.vertices; }
		UINT drawcalls() const { return _counters.drawcalls; }

		void merge(const draw_call_tracker& source);
		void reset();

		void track_depthstencil(ID3D11DepthStencilView* to_track);
		void log_drawcalls(UINT drawcalls, UINT vertices);
		void log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices);

		ID3D11DepthStencilView* get_best_depth_stencil(UINT width, UINT height);

	private:
		struct depthstencil_counter_info
		{
			UINT vertices = 0;
			UINT drawcalls = 0;
		};

		depthstencil_counter_info _counters;
		std::unordered_map<com_ptr<ID3D11DepthStencilView>, depthstencil_counter_info> _counters_per_used_depthstencil;
	};
}
