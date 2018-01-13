#include "draw_call_tracker.hpp"
#include <math.h>

namespace reshade::d3d11
{
	void draw_call_tracker::merge(const draw_call_tracker& source)
	{
		_counters.vertices += source.vertices();
		_counters.drawcalls += source.drawcalls();

		for (auto source_entry : source._counters_per_used_depthstencil)
		{
			const auto destination_entry = _counters_per_used_depthstencil.find(source_entry.first);

			if (destination_entry == _counters_per_used_depthstencil.end())
			{
				_counters_per_used_depthstencil.emplace(source_entry.first, source_entry.second);
			}
			else
			{
				destination_entry->second.vertices += source_entry.second.vertices;
				destination_entry->second.drawcalls += source_entry.second.drawcalls;
			}
		}
	}

	void draw_call_tracker::reset()
	{
		_counters.vertices = 0;
		_counters.drawcalls = 0;
		_counters_per_used_depthstencil.clear();
	}

	void draw_call_tracker::track_depthstencil(ID3D11DepthStencilView* depthstencil)
	{
		assert(depthstencil != nullptr);

		if (_counters_per_used_depthstencil.find(depthstencil) == _counters_per_used_depthstencil.end())
		{
			_counters_per_used_depthstencil.emplace(depthstencil, depthstencil_counter_info());
		}
	}

	void draw_call_tracker::log_drawcalls(UINT drawcalls, UINT vertices)
	{
		_counters.vertices += vertices;
		_counters.drawcalls += drawcalls;
	}
	void draw_call_tracker::log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices)
	{
		assert(depthstencil != nullptr && drawcalls > 0);

		const auto counters = _counters_per_used_depthstencil.find(depthstencil);

		if (counters != _counters_per_used_depthstencil.end())
		{
			counters->second.vertices += vertices;
			counters->second.drawcalls += drawcalls;
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
			if (depthstencil_info.drawcalls == 0)
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
			if (depthstencil_info.drawcalls >= best_info.drawcalls)
			{
				best_match = depthstencil.get();
				best_info = depthstencil_info;
			}
		}
		return best_match;
	}
}
