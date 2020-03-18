/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "buffer_detection.hpp"
#include "dxgi/format_utils.hpp"
#include <mutex>
#include <cmath>

static std::mutex s_global_mutex;

void reshade::d3d12::buffer_detection::init(ID3D12Device *device, ID3D12GraphicsCommandList *cmd_list, const buffer_detection_context *context)
{
	_device = device;
	_context = context;
	_cmd_list = cmd_list;
}

void reshade::d3d12::buffer_detection::reset()
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_current_depthstencil.reset();
	_has_indirect_drawcalls = false;
	_counters_per_used_depth_texture.clear();
#endif
}
void reshade::d3d12::buffer_detection_context::reset(bool release_resources)
{
	buffer_detection::reset();

#if RESHADE_DEPTH
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

void reshade::d3d12::buffer_detection::merge(const buffer_detection &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DEPTH
	// Executing a command list in a different command list inherits state
	_current_depthstencil = source._current_depthstencil;

	_best_copy_stats = source._best_copy_stats;
	_has_indirect_drawcalls |= source._has_indirect_drawcalls;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.total_stats.vertices += snapshot.total_stats.vertices;
		target_snapshot.total_stats.drawcalls += snapshot.total_stats.drawcalls;
		target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
		target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
	}
#endif
}

void reshade::d3d12::buffer_detection::on_draw(UINT vertices)
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
void reshade::d3d12::buffer_detection::on_set_depthstencil(D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	_current_depthstencil = _context->resource_from_handle(dsv);
}
void reshade::d3d12::buffer_detection::on_clear_depthstencil(D3D12_CLEAR_FLAGS clear_flags, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	assert(_context != nullptr);

	if ((clear_flags & D3D12_CLEAR_FLAG_DEPTH) == 0)
		return;

	com_ptr<ID3D12Resource> dsv_texture = _context->resource_from_handle(dsv);
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_clear_index.first)
		return;

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	// Update stats with data from previous frame
	if (counters.current_stats.drawcalls == 0)
		counters.current_stats = _context->_previous_stats;

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (_context->_depthstencil_clear_index.second == std::numeric_limits<UINT>::max() ?
		counters.current_stats.vertices > _best_copy_stats.vertices :
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.clears.size() == _context->_depthstencil_clear_index.second)
	{
		_best_copy_stats = counters.current_stats;

		D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
		transition.Transition.pResource = dsv_texture.get();
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE; // A resource has to be in this state for 'ID3D12GraphicsCommandList::ClearDepthStencilView', so can assume it here
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		_cmd_list->ResourceBarrier(1, &transition);

		// Do not need to insert a resource barrier for '_depthstencil_clear_texture' here, since it is in D3D12_RESOURCE_STATE_COMMON, which is implicitly promoted to D3D12_RESOURCE_STATE_COPY_DEST
		_cmd_list->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());

		std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
		_cmd_list->ResourceBarrier(1, &transition);
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

void reshade::d3d12::buffer_detection_context::on_create_dsv(ID3D12Resource *dsv_texture, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	const std::lock_guard<std::mutex> lock(s_global_mutex);
	_depthstencil_resources_by_handle[handle.ptr] = dsv_texture;
}
com_ptr<ID3D12Resource> reshade::d3d12::buffer_detection_context::resource_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
{
	if (handle.ptr == 0)
		return nullptr;

	const std::lock_guard<std::mutex> lock(s_global_mutex);
	if (const auto it = _depthstencil_resources_by_handle.find(handle.ptr);
		it != _depthstencil_resources_by_handle.end())
		return it->second;
	return nullptr;
}

bool reshade::d3d12::buffer_detection_context::update_depthstencil_clear_texture(ID3D12CommandQueue *queue, D3D12_RESOURCE_DESC desc)
{
	assert(_device != nullptr);

	if (_depthstencil_clear_texture != nullptr)
	{
		const D3D12_RESOURCE_DESC existing_desc = _depthstencil_clear_texture->GetDesc();

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use

		// Texture may still be in use on device, so wait for all operations to finish before destroying it
		{	com_ptr<ID3D12Fence> fence;
			if (FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
				return false;
			const HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fence_event == nullptr)
				return false;

			assert(queue != nullptr);
			queue->Signal(fence.get(), 1);
			fence->SetEventOnCompletion(1, fence_event);
			WaitForSingleObject(fence_event, INFINITE);
			CloseHandle(fence_event);
		}

		_depthstencil_clear_texture.reset();
	}

	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.Format = make_dxgi_format_typeless(desc.Format);

	D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

	if (HRESULT hr = _device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_depthstencil_clear_texture)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

com_ptr<ID3D12Resource> reshade::d3d12::buffer_detection_context::update_depth_texture(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list, UINT width, UINT height, ID3D12Resource *override, UINT clear_index_override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D12Resource> best_match;

	if (override != nullptr)
	{
		best_match = override;
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

			if (width != 0 && height != 0)
			{
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

	_depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };

	if (best_match == nullptr || !update_depthstencil_clear_texture(queue, best_match->GetDesc()))
		return nullptr;

	if (clear_index_override != 0)
	{
		_previous_stats = best_snapshot.current_stats;
		_depthstencil_clear_index.first = best_match.get();

		if (clear_index_override <= best_snapshot.clears.size())
		{
			_depthstencil_clear_index.second = clear_index_override;
		}
	}
	else
	{
		// TODO: Fix resource state transition
		list->CopyResource(_context->_depthstencil_clear_texture.get(), best_match.get());
	}

	return _depthstencil_clear_texture;
}
#endif
