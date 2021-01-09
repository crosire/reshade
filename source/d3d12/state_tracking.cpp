/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "state_tracking.hpp"
#include "dxgi/format_utils.hpp"
#include <mutex>
#include <cmath>

static std::mutex s_global_mutex;

void reshade::d3d12::state_tracking::init(ID3D12Device *device, ID3D12GraphicsCommandList *cmd_list, const state_tracking_context *context)
{
	_device = device;
	_context = context;
	_cmd_list = cmd_list;
}

void reshade::d3d12::state_tracking::reset()
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_current_depthstencil.reset();
	_first_empty_stats = true;
	_has_indirect_drawcalls = false;
	_counters_per_used_depth_texture.clear();
#endif
}
void reshade::d3d12::state_tracking_context::reset(bool release_resources)
{
	state_tracking::reset();

#if RESHADE_DEPTH
	assert(_depthstencil_clear_texture == nullptr || _context == this);

	if (release_resources)
	{
		assert(_context == this);

		_previous_stats = { 0, 0 };

		// Can only destroy this when it is guaranteed to no longer be in use
		_depthstencil_clear_texture.reset();
		_depthstencil_resources_by_handle.clear();
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d12::state_tracking::merge(const state_tracking &source)
{
	// Lock here when this was called from 'D3D12CommandQueue::ExecuteCommandLists' in case there are multiple command queues
	std::unique_lock<std::mutex> lock(s_global_mutex, std::defer_lock);
	if (_context == this)
		lock.lock();

	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DEPTH
	// Executing a command list in a different command list inherits state
	_current_depthstencil = source._current_depthstencil;

	if (_first_empty_stats)
		_first_empty_stats = source._first_empty_stats;
	_has_indirect_drawcalls |= source._has_indirect_drawcalls;

	if (source._best_copy_stats.vertices > _best_copy_stats.vertices)
		_best_copy_stats = source._best_copy_stats;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.total_stats.vertices += snapshot.total_stats.vertices;
		target_snapshot.total_stats.drawcalls += snapshot.total_stats.drawcalls;
		target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
		target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());

		// Only update state if a transition happened in this command list
		if (snapshot.current_state != D3D12_RESOURCE_STATE_COMMON)
			target_snapshot.current_state = snapshot.current_state;

		target_snapshot.copied_due_to_aliasing |= snapshot.copied_due_to_aliasing;
	}
#endif
}

void reshade::d3d12::state_tracking::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	if (_current_depthstencil == nullptr)
		return; // This is a draw call with no depth-stencil bound

	auto &counters = _counters_per_used_depth_texture[_current_depthstencil];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices;
	counters.current_stats.drawcalls += 1;

	if (vertices == 0)
		_has_indirect_drawcalls = true;
#endif
}

#if RESHADE_DEPTH
void reshade::d3d12::state_tracking::on_aliasing(const D3D12_RESOURCE_ALIASING_BARRIER &aliasing)
{
	if (_context->_depthstencil_clear_texture == nullptr || _context->depthstencil_clear_index.first == nullptr || _context->preserve_depth_buffers)
		return;
	if (aliasing.pResourceBefore != nullptr && aliasing.pResourceBefore != _context->depthstencil_clear_index.first)
		return;
	// Lock here when this was called from 'D3D12CommandQueue::ExecuteCommandLists' in case there are multiple command queues
	std::unique_lock<std::mutex> lock(s_global_mutex, std::defer_lock);
	if (_context == this)
		lock.lock();

	if (_context->_placed_depthstencil_resources.find(_context->depthstencil_clear_index.first) == _context->_placed_depthstencil_resources.end())
		return;

	auto &counters = _counters_per_used_depth_texture[_context->depthstencil_clear_index.first];
	if (counters.current_stats.drawcalls == 0)
		return;

	const D3D12_RESOURCE_STATES state = counters.current_state != D3D12_RESOURCE_STATE_COMMON ? counters.current_state : D3D12_RESOURCE_STATE_DEPTH_WRITE;

	if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
		transition.Transition.pResource = _context->depthstencil_clear_index.first;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		transition.Transition.StateBefore = state;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		_cmd_list->ResourceBarrier(1, &transition);
	}

	_cmd_list->CopyResource(_context->_depthstencil_clear_texture.get(), _context->depthstencil_clear_index.first);

	if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
		transition.Transition.pResource = _context->depthstencil_clear_index.first;
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		transition.Transition.StateAfter = state;
		_cmd_list->ResourceBarrier(1, &transition);
	}

	counters.copied_due_to_aliasing = true;
}
void reshade::d3d12::state_tracking::on_transition(const D3D12_RESOURCE_TRANSITION_BARRIER &transition)
{
	if ((transition.pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
		return;

	D3D12_RESOURCE_STATES &current_state = _counters_per_used_depth_texture[transition.pResource].current_state;
	assert(current_state == transition.StateBefore || current_state == D3D12_RESOURCE_STATE_COMMON);
	current_state = transition.StateAfter;
}

void reshade::d3d12::state_tracking::on_set_depthstencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	_current_depthstencil = _context->resource_from_handle(dsv);
}

void reshade::d3d12::state_tracking::on_clear_depthstencil(D3D12_CLEAR_FLAGS clear_flags, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	assert(_context != nullptr);

	if ((clear_flags & D3D12_CLEAR_FLAG_DEPTH) == 0 || !_context->preserve_depth_buffers)
		return;

	com_ptr<ID3D12Resource> dsv_texture = _context->resource_from_handle(dsv);
	if (dsv_texture == nullptr || _context->_depthstencil_clear_texture == nullptr || dsv_texture != _context->depthstencil_clear_index.first)
		return;

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	// Update stats with data from previous frame
	if (counters.current_stats.drawcalls == 0 && _first_empty_stats)
	{
		counters.current_stats = _context->_previous_stats;
		_first_empty_stats = false;
	}

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (_context->depthstencil_clear_index.second == 0 ?
		// If clear index override is set to zero, always copy any suitable buffers
		counters.current_stats.vertices > _best_copy_stats.vertices :
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.clears.size() == _context->depthstencil_clear_index.second)
	{
		_best_copy_stats = counters.current_stats;

		const D3D12_RESOURCE_STATES state = counters.current_state != D3D12_RESOURCE_STATE_COMMON ? counters.current_state : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			transition.Transition.pResource = dsv_texture.get();
			transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transition.Transition.StateBefore = state;
			transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			_cmd_list->ResourceBarrier(1, &transition);
		}

		_cmd_list->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());

		if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			transition.Transition.pResource = dsv_texture.get();
			transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
			transition.Transition.StateAfter = state;
			_cmd_list->ResourceBarrier(1, &transition);
		}
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

std::vector<std::pair<ID3D12Resource*, reshade::d3d12::state_tracking::depthstencil_info>> reshade::d3d12::state_tracking_context::sorted_counters_per_used_depthstencil()
{
	struct pair_wrapper
	{
		int display_count;
		ID3D12Resource* wrapped_dsv_texture;
		depthstencil_info wrapped_depthstencil_info;
		UINT64 depthstencil_width;
		UINT64 depthstencil_height;
	};

	std::vector<pair_wrapper> sorted_counters_per_buffer;
	sorted_counters_per_buffer.reserve(_counters_per_used_depth_texture.size());
	
	for (const auto& [dsv_texture, snapshot] : _counters_per_used_depth_texture)
	{
		auto dsv_texture_instance = dsv_texture.get();
		const D3D12_RESOURCE_DESC desc = dsv_texture->GetDesc();
		// get the display counter, if any of this texture.
		auto entry = _shown_count_per_depthstencil_address.find(dsv_texture_instance);
		if(entry == _shown_count_per_depthstencil_address.end())
		{
			sorted_counters_per_buffer.push_back({ 1, dsv_texture_instance, snapshot, desc.Width, desc.Height });
		}
		else
		{
			auto shown_count = entry->second;
			sorted_counters_per_buffer.push_back({ ++shown_count, dsv_texture_instance, snapshot, desc.Width, desc.Height });
		}
	}
	
	std::sort(sorted_counters_per_buffer.begin(), sorted_counters_per_buffer.end(), [](const pair_wrapper& a, const pair_wrapper& b)
	{
		return (a.display_count > b.display_count) ||
			(a.display_count == b.display_count &&
				(a.depthstencil_width > b.depthstencil_width ||
					(a.depthstencil_width == b.depthstencil_width && a.depthstencil_height > b.depthstencil_height))) ||
			(a.depthstencil_width == b.depthstencil_width && a.depthstencil_height == b.depthstencil_height &&
				a.wrapped_dsv_texture < b.wrapped_dsv_texture);
	});
	// build a new vector with the sorted elements to return
	std::vector<std::pair<ID3D12Resource*, state_tracking::depthstencil_info>> to_return;
	to_return.reserve(sorted_counters_per_buffer.size());
	// store the state of the counters as we've collected them as the new state for the next frame.
	_shown_count_per_depthstencil_address.clear();
	for(const auto& [display_count, dsv_texture, snapshot, w, h] : sorted_counters_per_buffer)
	{
		to_return.push_back({ dsv_texture, snapshot });
		_shown_count_per_depthstencil_address[dsv_texture] = display_count;
	}
	return to_return;
}

void reshade::d3d12::state_tracking_context::on_create_dsv(ID3D12Resource *dsv_texture, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	const std::lock_guard<std::mutex> lock(s_global_mutex);
	_depthstencil_resources_by_handle[handle.ptr] = dsv_texture;
}
void reshade::d3d12::state_tracking_context::on_create_placed_resource(ID3D12Resource *resource, ID3D12Heap *, UINT64)
{
	if ((resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) == 0)
		return;

	const std::lock_guard<std::mutex> lock(s_global_mutex);
	_placed_depthstencil_resources.insert(resource);
}

com_ptr<ID3D12Resource> reshade::d3d12::state_tracking_context::resource_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
	if (handle.ptr == 0)
		return nullptr;

	const std::lock_guard<std::mutex> lock(s_global_mutex);
	if (const auto it = _depthstencil_resources_by_handle.find(handle.ptr);
		it != _depthstencil_resources_by_handle.end())
		return it->second;
	return nullptr;
}

bool reshade::d3d12::state_tracking_context::update_depthstencil_clear_texture(D3D12_RESOURCE_DESC desc)
{
	assert(_device != nullptr);

	if (_depthstencil_clear_texture != nullptr)
	{
		const D3D12_RESOURCE_DESC existing_desc = _depthstencil_clear_texture->GetDesc();

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use

		// Runtime using this texture should have added its own reference, so can safely release this one here
		_depthstencil_clear_texture.reset();
	}

	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.Format = make_dxgi_format_typeless(desc.Format);

	D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

	if (HRESULT hr = _device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_depthstencil_clear_texture)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}
	_depthstencil_clear_texture->SetName(L"ReShade depth copy texture");

	return true;
}

com_ptr<ID3D12Resource> reshade::d3d12::state_tracking_context::update_depth_texture(ID3D12GraphicsCommandList *list, UINT width, UINT height, ID3D12Resource *override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D12Resource> best_match = override;
	if (best_match != nullptr)
	{
		best_snapshot = _counters_per_used_depth_texture[best_match];
	}
	else
	{
		for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
		{
			if (snapshot.total_stats.drawcalls == 0)
				continue; // Skip unused

			const D3D12_RESOURCE_DESC desc = dsv_texture->GetDesc();
			assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			if (use_aspect_ratio_heuristics)
			{
				assert(width != 0 && height != 0);
				const float w = static_cast<float>(width);
				const float w_ratio = w / desc.Width;
				const float h = static_cast<float>(height);
				const float h_ratio = h / desc.Height;
				const float aspect_ratio = (w / h) - (static_cast<float>(desc.Width) / desc.Height);

				if (std::fabs(aspect_ratio) > 0.1f || w_ratio > 1.85f || h_ratio > 1.85f || w_ratio < 0.5f || h_ratio < 0.5f)
					continue; // Not a good fit
			}

			if (!_has_indirect_drawcalls ?
				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				snapshot.total_stats.vertices > best_snapshot.total_stats.vertices :
				// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
				snapshot.total_stats.drawcalls > best_snapshot.total_stats.drawcalls)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;
			}
		}
	}

	const bool has_changed = depthstencil_clear_index.first != best_match;
	depthstencil_clear_index.first = best_match.get();

	if (best_match == nullptr || !update_depthstencil_clear_texture(best_match->GetDesc()))
		return nullptr;

	if (preserve_depth_buffers)
	{
		_previous_stats = best_snapshot.current_stats;
	}
	else
	{
		// Do not copy at end of frame if already copied due to aliasing, or it is not guaranteed that the resource is not aliased
		const std::lock_guard<std::mutex> lock(s_global_mutex);
		if (!best_snapshot.copied_due_to_aliasing && (!has_changed || _placed_depthstencil_resources.find(depthstencil_clear_index.first) == _placed_depthstencil_resources.end()))
		{
			const D3D12_RESOURCE_STATES state = best_snapshot.current_state != D3D12_RESOURCE_STATE_COMMON ? best_snapshot.current_state : D3D12_RESOURCE_STATE_DEPTH_WRITE;

			if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
			{
				D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
				transition.Transition.pResource = best_match.get();
				transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transition.Transition.StateBefore = state;
				transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
				list->ResourceBarrier(1, &transition);
			}

			list->CopyResource(_context->_depthstencil_clear_texture.get(), best_match.get());

			if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
			{
				D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
				transition.Transition.pResource = best_match.get();
				transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
				transition.Transition.StateAfter = state;
				list->ResourceBarrier(1, &transition);
			}
		}
	}

	best_snapshot.copied_due_to_aliasing = false;

	return _depthstencil_clear_texture;
}
#endif
