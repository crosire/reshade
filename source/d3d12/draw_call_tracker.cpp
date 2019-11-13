/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "draw_call_tracker.hpp"
#include "dxgi/format_utils.hpp"
#include "runtime_d3d12.hpp"
#include <mutex>
#include <math.h>

namespace reshade::d3d12
{
	static std::mutex s_global_mutex;

	void draw_call_tracker::merge(const draw_call_tracker &source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		for (const auto &[clear_index, snapshot] : source._cleared_depth_textures)
		{
			_cleared_depth_textures[clear_index] = snapshot;
		}

		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depth_texture)
		{
			_counters_per_used_depth_texture[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depth_texture[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
		}
#endif
	}

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		_current_depthstencil.reset();
		_cleared_depth_textures.clear();
		_counters_per_used_depth_texture.clear();
#endif
	}

	void draw_call_tracker::on_draw(UINT vertices)
	{
		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		if (_current_depthstencil == nullptr)
			// This is a draw call with no depth stencil
			return;

		if (const auto intermediate_snapshot = _counters_per_used_depth_texture.find(_current_depthstencil);
			intermediate_snapshot != _counters_per_used_depth_texture.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;
		}
#endif
	}

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	bool draw_call_tracker::filter_aspect_ratio = true;
	bool draw_call_tracker::preserve_depth_buffers = false;
	bool draw_call_tracker::preserve_stencil_buffers = false;
	unsigned int draw_call_tracker::depth_stencil_clear_index = 0;
	unsigned int draw_call_tracker::filter_depth_texture_format = 0;

	void draw_call_tracker::track_render_targets(com_ptr<ID3D12Resource> depthstencil)
	{
		_current_depthstencil = depthstencil;

		if (depthstencil == nullptr)
			return;

		// Add new entry for this DSV
		_counters_per_used_depth_texture[depthstencil];
	}
	void draw_call_tracker::track_cleared_depthstencil(ID3D12GraphicsCommandList *cmd_list, D3D12_CLEAR_FLAGS clear_flags, com_ptr<ID3D12Resource> texture, UINT clear_index, runtime_d3d12 *runtime)
	{
		if (texture == nullptr)
			return;

		if (!(preserve_depth_buffers && (clear_flags & D3D12_CLEAR_FLAG_DEPTH)) &&
			!(preserve_stencil_buffers && (clear_flags & D3D12_CLEAR_FLAG_STENCIL)))
			return;

		const D3D12_RESOURCE_DESC desc = texture->GetDesc();
		assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

		if (desc.SampleDesc.Count > 1)
			return; // Ignore MSAA textures
		if (!check_texture_format(desc))
			return;

		com_ptr<ID3D12Resource> backup_texture;

		// Make a backup copy of depth textures that are cleared by the application
		if ((depth_stencil_clear_index == 0) || (clear_index == depth_stencil_clear_index))
		{
			assert(cmd_list != nullptr);

			const std::lock_guard<std::mutex> lock(s_global_mutex);

			backup_texture = runtime->create_compatible_texture(desc);
			if (backup_texture == nullptr)
				return;

			D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			transition.Transition.pResource = backup_texture.get();
			transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
			transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			cmd_list->ResourceBarrier(1, &transition);

			cmd_list->CopyResource(backup_texture.get(), texture.get());

			transition.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
			cmd_list->ResourceBarrier(1, &transition);
		}

		_cleared_depth_textures.insert({ clear_index, { std::move(texture), std::move(backup_texture) } });
	}

	bool draw_call_tracker::check_aspect_ratio(const D3D12_RESOURCE_DESC &desc, UINT width, UINT height)
	{
		if (!filter_aspect_ratio)
			return true;

		const float aspect_ratio = float(width) / float(height);
		const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

		const float width_factor = float(width) / float(desc.Width);
		const float height_factor = float(height) / float(desc.Height);

		return !(fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f);
	}
	bool draw_call_tracker::check_texture_format(const D3D12_RESOURCE_DESC &desc)
	{
		const DXGI_FORMAT depth_texture_formats[] = {
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_R16_TYPELESS,
			DXGI_FORMAT_R32_TYPELESS,
			DXGI_FORMAT_R24G8_TYPELESS,
			DXGI_FORMAT_R32G8X24_TYPELESS
		};

		if (filter_depth_texture_format == 0 ||
			filter_depth_texture_format >= ARRAYSIZE(depth_texture_formats))
			return true; // All formats are allowed

		return make_dxgi_format_typeless(desc.Format) == depth_texture_formats[filter_depth_texture_format];
	}

	com_ptr<ID3D12Resource> draw_call_tracker::find_best_depth_texture(UINT width, UINT height)
	{
		com_ptr<ID3D12Resource> best_texture;

		if (preserve_depth_buffers || preserve_stencil_buffers)
		{
			if (const auto it = _cleared_depth_textures.find(depth_stencil_clear_index);
				depth_stencil_clear_index != 0 && it != _cleared_depth_textures.end())
				return it->second.backup_texture;

			for (const auto &[clear_index, snapshot] : _cleared_depth_textures)
			{
				assert(snapshot.dsv_texture != nullptr);

				if (snapshot.backup_texture == nullptr)
					continue;

				const D3D12_RESOURCE_DESC desc = snapshot.dsv_texture->GetDesc();

				if (!check_aspect_ratio(desc, width, height))
					continue;

				best_texture = snapshot.backup_texture;
			}
		}
		else
		{
			intermediate_snapshot_info best_snapshot;

			for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
			{
				if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
					continue; // Skip unused

				const D3D12_RESOURCE_DESC desc = dsv_texture->GetDesc();
				assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

				if (desc.SampleDesc.Count > 1)
					continue; // Ignore MSAA textures, since they would need to be resolved first
				if (!check_aspect_ratio(desc, width, height))
					continue; // Not a good fit
				if (!check_texture_format(desc))
					continue;

				// Choose snapshot with the most draw calls, since vertices may not be accurate if application is using indirect draw calls
				if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
				{
					best_texture = dsv_texture;
					best_snapshot = snapshot;
				}
			}
		}

		return best_texture;
	}
#endif
}
