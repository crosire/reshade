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
	_counters_per_used_depthstencil.clear();
	_drawcalls = 0;
	_vertices = 0;
}

void depth_counter_tracker::merge(depth_counter_tracker& source)
{
	_drawcalls += source.drawcalls();
	_vertices += source.vertices();

	// copy entries in source into this, if key is already there, update counters accordingly. 
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
	
ID3D11DepthStencilView* depth_counter_tracker::get_best_depth_stencil(std::unordered_map<ID3D11DepthStencilView*, depthstencil_size> const& depthstencils, UINT width, UINT height)
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
		auto sizeinfo = get_depth_stencil_size(depthstencils, depthstencil);
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

depthstencil_size depth_counter_tracker::get_depth_stencil_size(std::unordered_map<ID3D11DepthStencilView*, depthstencil_size> const& depthstencils, ID3D11DepthStencilView* view)
{
	// no need for a lock, as it's called from the present call. 
	depthstencil_size to_return;
	if (nullptr == view)
	{
		return to_return;
	}

	auto entry = depthstencils.find(view);
	if (entry == depthstencils.end())
	{
		return to_return;
	}

	const auto depthstencil = entry->first;
	auto &size_info = entry->second;
	if ((depthstencil->AddRef(), depthstencil->Release()) == 1)
	{
		// already out of scope, ignore, so return default. Clean up later on will remove this one. 
		return to_return;
	}
	else
	{
		to_return = size_info;
	}
	return to_return;
}
