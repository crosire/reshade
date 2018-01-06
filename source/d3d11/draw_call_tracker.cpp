#include "draw_call_tracker.hpp"
#include "com_ptr.hpp"
#include <math.h>


namespace reshade::d3d11
{

	draw_call_tracker::draw_call_tracker() :
		_drawcalls(0),
		_vertices(0)
	{ }

	draw_call_tracker::~draw_call_tracker() { }

	void draw_call_tracker::track_depthstencil(ID3D11DepthStencilView* to_track)
	{
		if (nullptr != to_track)
		{
			if (_counters_per_used_depthstencil.find(to_track) == _counters_per_used_depthstencil.end())
			{
				const depthstencil_counter_info counters = {};
				_counters_per_used_depthstencil.emplace(to_track, counters);
			}
		}
	}

	void draw_call_tracker::log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices)
	{
		assert(nullptr != depthstencil && drawcalls > 0);

		const auto counters = _counters_per_used_depthstencil.find(depthstencil);
		if (counters == _counters_per_used_depthstencil.end())
		{
			return;
		}
		counters->second.drawcall_count += drawcalls;
		counters->second.vertices_count += vertices;
	}

	void draw_call_tracker::reset()
	{
		_counters_per_used_depthstencil.clear();
		_drawcalls = 0;
		_vertices = 0;
	}

	void draw_call_tracker::merge(draw_call_tracker& source)
	{
		_drawcalls += source.drawcalls();
		_vertices += source.vertices();

		for (auto source_entry : source._counters_per_used_depthstencil)
		{
			auto destination_entry = _counters_per_used_depthstencil.find(source_entry.first);
			if (destination_entry == _counters_per_used_depthstencil.end())
			{
				const depthstencil_counter_info counters = { source_entry.second.drawcall_count, source_entry.second.vertices_count };
				_counters_per_used_depthstencil.emplace(source_entry.first, counters);
			}
			else
			{
				destination_entry->second.drawcall_count += source_entry.second.drawcall_count;
				destination_entry->second.vertices_count += source_entry.second.vertices_count;
			}
		}
	}

	ID3D11DepthStencilView* draw_call_tracker::get_best_depth_stencil(UINT width, UINT height)
	{
		depthstencil_counter_info best_info = { 0 };
		ID3D11DepthStencilView *best_match = nullptr;
		float aspect_ratio = ((float)width) / ((float)height);

		for (auto it : _counters_per_used_depthstencil)
		{
			const auto depthstencil = it.first;
			auto &depthstencil_info = it.second;
			if (depthstencil_info.drawcall_count == 0)
			{
				continue;
			}

			// Determine depthstencil size on-the-fly. We execute this code in present, and this isn't slow anyway. 
			// Caching the size infos has the downside that if the game uses dynamic supersampling/resolution scaling, we end up
			// with a lot of depthstencils which are likely out of scope leading to memory leaks. 
			D3D11_TEXTURE2D_DESC texture_desc;
			com_ptr<ID3D11Resource> resource;
			com_ptr<ID3D11Texture2D> texture;
			depthstencil->GetResource(&resource);
			if (FAILED(resource->QueryInterface(&texture)))
			{
				continue;
			}
			texture->GetDesc(&texture_desc);

			bool size_mismatch = false;
			// if the size of the depth stencil has the size of the window, we'll look at its counters. If it doesn't we'll try some heuristics, like
			// aspect ratio equivalence or whether the height is 1 off, which is sometimes the case in some games. 
			if (texture_desc.Width != width)
			{
				size_mismatch = true;
			}
			if (texture_desc.Height != height)
			{
				// check if height is uneven, some games round depthsize height to an even number.
				if (texture_desc.Height != (height - 1) && texture_desc.Height != (height + 1))
				{
					// not a matching height
					size_mismatch = true;
				}
			}
			if (size_mismatch)
			{
				// check aspect ratio. 
				float stencilaspectratio = ((float)texture_desc.Width) / ((float)texture_desc.Height);
				if (fabs(stencilaspectratio - aspect_ratio) > 0.1f)
				{
					// still no match, not a good fit
					continue;
				}
			}
			if (depthstencil_info.drawcall_count >= best_info.drawcall_count)
			{
				best_match = depthstencil.get();
				best_info = depthstencil_info;
			}
		}
		return best_match;
	}
}