#include "depth_counter_tracker.hpp"
#include "com_ptr.hpp"
#include <math.h>

depth_counter_tracker::depth_counter_tracker() : 
	_drawcalls(0), 
	_vertices(0) 
{ }

depth_counter_tracker::~depth_counter_tracker() { }

void depth_counter_tracker::track_depthstencil(ID3D11DepthStencilView* to_track)
{
	if (nullptr != to_track)
	{
		if (_counters_per_used_depthstencil.find(to_track) == _counters_per_used_depthstencil.end())
		{
			const depthstencil_counter_info counters = {};
			to_track->AddRef();
			_counters_per_used_depthstencil.emplace(to_track, counters);
		}
	}
}

void depth_counter_tracker::log_drawcalls(ID3D11DepthStencilView* depthstencil, UINT drawcalls, UINT vertices)
{
	if (nullptr == depthstencil || drawcalls==0)
	{
		return;
	}
	const auto counters = _counters_per_used_depthstencil.find(depthstencil);
	if (counters == _counters_per_used_depthstencil.end())
	{
		return;
	}
	counters->second.drawcall_count += drawcalls;
	counters->second.vertices_count += vertices;
}

void depth_counter_tracker::reset()
{
	for (auto it : _counters_per_used_depthstencil)
	{
		it.first->Release();
	}
	_counters_per_used_depthstencil.clear();
	_drawcalls = 0;
	_vertices = 0;
}

void depth_counter_tracker::merge(depth_counter_tracker& source)
{
	_drawcalls += source.drawcalls();
	_vertices += source.vertices();

	for (auto source_entry : source._counters_per_used_depthstencil)
	{
		auto destination_entry = _counters_per_used_depthstencil.find(source_entry.first);
		if (destination_entry == _counters_per_used_depthstencil.end())
		{
			const depthstencil_counter_info counters = { source_entry.second.drawcall_count, source_entry.second.vertices_count };
			source_entry.first->AddRef();
			_counters_per_used_depthstencil.emplace(source_entry.first, counters);
		}
		else
		{
			destination_entry->second.drawcall_count += source_entry.second.drawcall_count;
			destination_entry->second.vertices_count += source_entry.second.vertices_count;
		}
	}
}

ID3D11DepthStencilView* depth_counter_tracker::get_best_depth_stencil(UINT width, UINT height)
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
		depthstencil_size sizeinfo;
		D3D11_TEXTURE2D_DESC texture_desc;
		com_ptr<ID3D11Resource> resource;
		com_ptr<ID3D11Texture2D> texture;
		depthstencil->GetResource(&resource);
		if (!FAILED(resource->QueryInterface(&texture)))
		{
			texture->GetDesc(&texture_desc);
			sizeinfo.width = texture_desc.Width;
			sizeinfo.height = texture_desc.Height;
		}

		bool size_mismatch = false;
		// if the size of the depth stencil has the size of the window, we'll look at its counters. If it doesn't we'll try some heuristics, like
		// aspect ratio equivalence or whether the height is 1 off, which is sometimes the case in some games. 
		if (sizeinfo.width != width)
		{
			size_mismatch = true;
		}
		if (sizeinfo.height != height)
		{
			// check if height is uneven, some games round depthsize height to an even number.
			if (sizeinfo.height != (height - 1) && sizeinfo.height != (height + 1))
			{
				// not a matching height
				size_mismatch = true;
			}
		}
		if (size_mismatch)
		{
			// check aspect ratio. 
			float stencilaspectratio = ((float)sizeinfo.width) / ((float)sizeinfo.height);
			if (fabs(stencilaspectratio - aspect_ratio) > 0.1f)
			{
				// still no match, not a good fit
				continue;
			}
		}
		if (depthstencil_info.drawcall_count >= best_info.drawcall_count)
		{
			best_match = depthstencil;
			best_info = depthstencil_info;
		}
	}
	return best_match;
}

