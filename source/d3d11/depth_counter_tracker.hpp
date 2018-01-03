#pragma once

#include <d3d11.h>
#include <unordered_map>

class depth_counter_tracker
{
public:
	depth_counter_tracker();
	~depth_counter_tracker();

	void reset();
	void track_depthstencil(ID3D11DepthStencilView* to_track);
	void log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices);
	ID3D11DepthStencilView* depth_counter_tracker::get_best_depth_stencil(UINT width, UINT height);
	void merge(depth_counter_tracker& source);

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

	std::unordered_map<ID3D11DepthStencilView*, depthstencil_counter_info> _counters_per_used_depthstencil;	
	UINT _drawcalls = 0;
	UINT _vertices = 0;
};
