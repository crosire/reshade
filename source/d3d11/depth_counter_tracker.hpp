#pragma once

#include <d3d11.h>
#include <unordered_map>

struct depthstencil_size
{
	UINT width, height = 0;
};

class depth_counter_tracker
{
public:
	depth_counter_tracker();
	~depth_counter_tracker();

	void reset();
	void track_depthstencil(ID3D11DepthStencilView* to_track);
	void log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices);
	ID3D11DepthStencilView* depth_counter_tracker::get_best_depth_stencil(std::unordered_map<ID3D11DepthStencilView*, depthstencil_size> const& depthstencils, UINT width, UINT height);
	void merge(depth_counter_tracker& source);

	UINT drawcalls() { return _drawcalls; }
	UINT vertices() { return _vertices; }

private:
	struct depthstencil_counter_info
	{
		UINT drawcall_count = 0;
		UINT vertices_count = 0;
	};

	depthstencil_size get_depth_stencil_size(std::unordered_map<ID3D11DepthStencilView*, depthstencil_size> const& depthstencils, ID3D11DepthStencilView* view);

	std::unordered_map<ID3D11DepthStencilView*, depthstencil_counter_info> _counters_per_used_depthstencil;	// a depthstencil that's used on this context is added to this table and on every draw call the counters are updated for the active depthstencil.
	UINT _drawcalls = 0;
	UINT _vertices = 0;
};
