#include "draw_call_tracker.hpp"
#include "log.hpp"
#include "dxgi/format_utils.hpp"
#include <math.h>

namespace reshade::d3d12
{
	void draw_call_tracker::merge(const draw_call_tracker& source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depthstencil)
		{
			_counters_per_used_depthstencil[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depthstencil[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
			_counters_per_used_depthstencil[depthstencil].depthstencil = snapshot.depthstencil;
		}

		for (const auto &[index, depth_texture_save_info] : source._cleared_depth_textures)
			_cleared_depth_textures[index] = depth_texture_save_info;
#endif
	}

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
		_current_depthstencil.reset();
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depthstencil.clear();
		_cleared_depth_textures.clear();
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

		if (const auto intermediate_snapshot = _counters_per_used_depthstencil.find(_current_depthstencil); intermediate_snapshot != _counters_per_used_depthstencil.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;
		}
#endif
	}

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	bool draw_call_tracker::check_depthstencil(ID3D12Resource *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}
	bool draw_call_tracker::check_depth_texture_format(int format_index, ID3D12Resource *depthstencil)
	{
		assert(depthstencil != nullptr);

		// Do not check format if all formats are allowed (index zero is DXGI_FORMAT_UNKNOWN)
		if (format_index == DXGI_FORMAT_UNKNOWN)
			return true;

		// Retrieve texture associated with depth stencil
		const D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();

		const DXGI_FORMAT depth_texture_formats[] = {
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_R16_TYPELESS,
			DXGI_FORMAT_R32_TYPELESS,
			DXGI_FORMAT_R24G8_TYPELESS,
			DXGI_FORMAT_R32G8X24_TYPELESS
		};

		assert(format_index > DXGI_FORMAT_UNKNOWN && format_index < ARRAYSIZE(depth_texture_formats));

		return make_dxgi_format_typeless(desc.Format) == depth_texture_formats[format_index];
	}

	void draw_call_tracker::track_rendertargets(int format_index, ID3D12Resource *depthstencil)
	{
		assert(depthstencil != nullptr);

		if (!check_depth_texture_format(format_index, depthstencil))
			return;

		if (_counters_per_used_depthstencil[depthstencil].depthstencil == nullptr)
			_counters_per_used_depthstencil[depthstencil].depthstencil = depthstencil;
	}
	void draw_call_tracker::track_depth_texture(int format_index, UINT index, com_ptr<ID3D12Resource> src_texture, com_ptr<ID3D12Resource> dest_texture, bool cleared)
	{
		// Function that keeps track of a cleared depth texture in an ordered map in order to retrieve it at the final rendering stage
		assert(src_texture != nullptr);

		if (!check_depth_texture_format(format_index, src_texture.get()))
			return;

		// Gather some extra info for later display
		const D3D12_RESOURCE_DESC src_texture_desc = src_texture->GetDesc();

		// Check if it is really a depth texture
		assert((src_texture_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

		// Fill the ordered map with the saved depth texture
		_cleared_depth_textures[index] = depth_texture_save_info { src_texture, src_texture_desc, dest_texture, cleared };
	}

	draw_call_tracker::intermediate_snapshot_info draw_call_tracker::find_best_snapshot(UINT width, UINT height)
	{
		const float aspect_ratio = float(width) / float(height);
		intermediate_snapshot_info best_snapshot;

		for (auto &[depthstencil, snapshot] : _counters_per_used_depthstencil)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue;

			const D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();

			assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

			// Check aspect ratio
			const float width_factor = float(width) / float(desc.Width);
			const float height_factor = float(height) / float(desc.Height);
			const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

			if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
				continue; // No match, not a good fit

			if (snapshot.stats.vertices >= best_snapshot.stats.vertices)
				best_snapshot = snapshot;
		}

		return best_snapshot;
	}

	void draw_call_tracker::keep_cleared_depth_textures()
	{
		// Function that keeps only the depth textures that has been retrieved before the last depth stencil clearance
		std::map<UINT, depth_texture_save_info>::reverse_iterator it = _cleared_depth_textures.rbegin();

		// Reverse loop on the cleared depth textures map
		while (it != _cleared_depth_textures.rend())
		{
			// Exit if the last cleared depth stencil is found
			if (it->second.cleared)
				return;

			// Remove the depth texture if it was retrieved after the last clearance of the depth stencil
			it = std::map<UINT, depth_texture_save_info>::reverse_iterator(_cleared_depth_textures.erase(std::next(it).base()));
		}
	}

	ID3D12Resource *draw_call_tracker::find_best_cleared_depth_buffer_texture(UINT clear_index)
	{
		// Function that selects the best cleared depth texture according to the clearing number defined in the configuration settings
		ID3D12Resource *best_match = nullptr;

		// Ensure to work only on the depth textures retrieved before the last depth stencil clearance
		keep_cleared_depth_textures();

		for (const auto &it : _cleared_depth_textures)
		{
			UINT i = it.first;
			auto &texture_counter_info = it.second;

			com_ptr<ID3D12Resource> texture;
			if (texture_counter_info.dest_texture == nullptr)
				continue;
			texture = texture_counter_info.dest_texture;

			if (clear_index != 0 && i > clear_index)
				continue;

			// The _cleared_dept_textures ordered map stores the depth textures, according to the order of clearing
			// if clear_index == 0, the auto select mode is defined, so the last cleared depth texture is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is greater or equal than it, the texture corresponding to this number is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is lower than it, the last cleared depth texture is retrieved
			best_match = texture.get();
		}

		return best_match;
	}
#endif
}
