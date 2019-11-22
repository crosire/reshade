/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "buffer_detection.hpp"
#include "../dxgi/format_utils.hpp"
#include <mutex>
#include <math.h>

static std::mutex s_global_mutex;

void reshade::d3d12::buffer_detection::init(ID3D12Device *device, const buffer_detection_context *context)
{
	_device = device;
	_context = context;
}

void reshade::d3d12::buffer_detection::reset()
{
	_stats.vertices = 0;
	_stats.drawcalls = 0;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	_clear_stats.vertices = 0;
	_clear_stats.drawcalls = 0;
	_current_depthstencil.reset();
	_counters_per_used_depth_texture.clear();
#endif
}
void reshade::d3d12::buffer_detection_context::reset(bool release_resources)
{
	buffer_detection::reset();

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	if (release_resources)
	{
		_depthstencil_clear_texture.reset();
		_depthstencil_resources_by_handle.clear();
	}
#endif
}

void reshade::d3d12::buffer_detection::merge(const buffer_detection &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	_clear_stats.vertices += source._clear_stats.vertices;
	_clear_stats.drawcalls += source._clear_stats.drawcalls;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.stats.vertices += snapshot.stats.vertices;
		target_snapshot.stats.drawcalls += snapshot.stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
	}
#endif
}

void reshade::d3d12::buffer_detection::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	if (_current_depthstencil == nullptr)
		return; // This is a draw call with no depth stencil bound

	_clear_stats.vertices += vertices;
	_clear_stats.drawcalls += 1;

	if (const auto intermediate_snapshot = _counters_per_used_depth_texture.find(_current_depthstencil);
		intermediate_snapshot != _counters_per_used_depth_texture.end())
	{
		intermediate_snapshot->second.stats.vertices += vertices;
		intermediate_snapshot->second.stats.drawcalls += 1;
	}
#endif
}

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
void reshade::d3d12::buffer_detection::track_render_targets(D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	_current_depthstencil = _context->resource_from_handle(dsv);

	if (_current_depthstencil == nullptr)
		return;

	// Add new entry for this DSV
	_counters_per_used_depth_texture[_current_depthstencil];
}
void reshade::d3d12::buffer_detection::track_cleared_depthstencil(ID3D12GraphicsCommandList *cmd_list, D3D12_CLEAR_FLAGS clear_flags, D3D12_CPU_DESCRIPTOR_HANDLE dsv)
{
	assert(_context != nullptr);

	// Reset draw call stats for clears
	auto current_stats = _clear_stats;
	_clear_stats.vertices = 0;
	_clear_stats.drawcalls = 0;

	if (current_stats.vertices == 0 || current_stats.drawcalls == 0)
		return; // Ignore clears when there was no meaningful workload since the last one

	if ((clear_flags & D3D12_CLEAR_FLAG_DEPTH) == 0)
		return;

	com_ptr<ID3D12Resource> dsv_texture = _context->resource_from_handle(dsv);
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_clear_index.first)
		return;

	auto &clears = _counters_per_used_depth_texture[dsv_texture].clears;
	clears.push_back(current_stats);

	// Make a backup copy of the depth texture before it is cleared
	// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth stencil is used in more than one
	if (clears.size() == _context->_depthstencil_clear_index.second)
	{
		D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
		transition.Transition.pResource = _context->_depthstencil_clear_texture.get();
		transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		cmd_list->ResourceBarrier(1, &transition);

		cmd_list->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());

		transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		cmd_list->ResourceBarrier(1, &transition);
	}
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

bool reshade::d3d12::buffer_detection_context::update_depthstencil_clear_texture(D3D12_RESOURCE_DESC desc)
{
	if (_depthstencil_clear_texture != nullptr)
	{
		const D3D12_RESOURCE_DESC existing_desc = _depthstencil_clear_texture->GetDesc();

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use
		else
			_depthstencil_clear_texture.reset();
	}

	desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Format = make_dxgi_format_typeless(desc.Format);

	D3D12_CLEAR_VALUE clear_value = {};
	clear_value.Format = make_dxgi_format_dsv(desc.Format);
	clear_value.DepthStencil = { 1.0f, 0x0 };
	D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

	if (HRESULT hr = _device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&_depthstencil_clear_texture)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

com_ptr<ID3D12Resource> reshade::d3d12::buffer_detection_context::find_best_depth_texture(UINT width, UINT height, com_ptr<ID3D12Resource> override, UINT clear_index_override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D12Resource> best_match;

	if (override != nullptr)
	{
		best_match = std::move(override);
		best_snapshot = _counters_per_used_depth_texture[best_match];
	}
	else
	{
		for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue; // Skip unused

			const D3D12_RESOURCE_DESC desc = dsv_texture->GetDesc();
			assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			if (width != 0 && height != 0)
			{
				const float aspect_ratio = float(width) / float(height);
				const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

				const float width_factor = float(width) / float(desc.Width);
				const float height_factor = float(height) / float(desc.Height);

				if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
					continue; // Not a good fit
			}

			// Choose snapshot with the most draw calls, since vertices may not be accurate if application is using indirect draw calls
			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;
			}
		}
	}

	if (clear_index_override != 0 && best_match != nullptr)
	{
		_depthstencil_clear_index = { best_match.get(), std::numeric_limits<UINT>::max() };

		if (clear_index_override <= best_snapshot.clears.size())
		{
			_depthstencil_clear_index.second = clear_index_override;
		}
		else
		{
			UINT last_vertices = 0;

			for (UINT clear_index = 0; clear_index < best_snapshot.clears.size(); ++clear_index)
			{
				const auto &snapshot = best_snapshot.clears[clear_index];

				if (snapshot.vertices >= last_vertices)
				{
					last_vertices = snapshot.vertices;
					_depthstencil_clear_index.second = clear_index + 1;
				}
			}
		}

		if (update_depthstencil_clear_texture(best_match->GetDesc()))
		{
			return _depthstencil_clear_texture;
		}
		else
		{
			_depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
		}
	}

	return best_match;
}
#endif
