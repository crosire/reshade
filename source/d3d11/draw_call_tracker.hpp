#pragma once

#include <d3d11.h>
#include <unordered_map>
#include <com_ptr.hpp>

namespace reshade::d3d11
{
	struct draw_call_tracker_hasher
	{
		std::size_t operator()(const com_ptr<ID3D11DepthStencilView>& p) const
		{
			using std::hash;
			return hash<ID3D11DepthStencilView*>()(p.get());
		}
	};


	class draw_call_tracker
	{
	public:
		draw_call_tracker();
		~draw_call_tracker();

		void reset();
		void track_depthstencil(ID3D11DepthStencilView* to_track);
		void log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices);
		ID3D11DepthStencilView* draw_call_tracker::get_best_depth_stencil(UINT width, UINT height);
		void merge(draw_call_tracker& source);

		UINT drawcalls() { return _drawcalls; }
		void update_drawcalls(UINT amount) { _drawcalls += amount; }
		UINT vertices() { return _vertices; }
		void update_vertices(UINT amount) { _vertices += amount; }

	private:
		struct depthstencil_counter_info
		{
			UINT drawcall_count = 0;
			UINT vertices_count = 0;
		};

		std::unordered_map<com_ptr<ID3D11DepthStencilView>, depthstencil_counter_info, draw_call_tracker_hasher> _counters_per_used_depthstencil;
		UINT _drawcalls = 0;
		UINT _vertices = 0;
	};
}