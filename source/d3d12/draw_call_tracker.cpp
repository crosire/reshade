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

		if (source.current_depthstencil == nullptr)
			return;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		if(_counters_per_used_depthstencil[source.current_depthstencil].depthstencil != nullptr)
		{
			_counters_per_used_depthstencil[source.current_depthstencil].stats.vertices += source.total_vertices();
			_counters_per_used_depthstencil[source.current_depthstencil].stats.drawcalls += source.total_drawcalls();
		}

		for (const auto &[index, depth_texture_save_info] : source._cleared_depth_textures)
			_cleared_depth_textures[index] = depth_texture_save_info;
#endif
	}

	void draw_call_tracker::reset(bool all)
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
		current_depthstencil.reset();
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depthstencil.clear();
		_cleared_depth_textures.clear();
#endif

		if (all)
		{
			_dsv_heap_handles.clear();
			_depthstencil_resources_by_handle.clear();
		}
	}

	void draw_call_tracker::track_dsv_heap_handles(com_ptr<ID3D12DescriptorHeap> dsvHeap, UINT dsvHandleSize)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = dsvHeap->GetDesc();
		D3D12_CPU_DESCRIPTOR_HANDLE hdl = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_dsv_heap_handles.push_back(hdl);

		for (unsigned int i = 0; i < heapDesc.NumDescriptors; i++)
			_dsv_heap_handles.push_back(D3D12_CPU_DESCRIPTOR_HANDLE{ hdl.ptr + i * dsvHandleSize });
	}

	void draw_call_tracker::track_depthstencil_resource_by_handle(D3D12_CPU_DESCRIPTOR_HANDLE pDescriptor, com_ptr<ID3D12Resource> pDepthStencil)
	{
		if (pDepthStencil == nullptr)
			return;

		if (retrieve_descriptor_handle(pDescriptor).ptr != 0 && pDepthStencil != nullptr)
			_depthstencil_resources_by_handle[pDescriptor.ptr] = pDepthStencil;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE draw_call_tracker::retrieve_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE descHandle)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE result = D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };

		for (size_t i = 0; i < _dsv_heap_handles.size(); ++i)
		{
			if (_dsv_heap_handles[i].ptr == descHandle.ptr)
				return _dsv_heap_handles[i];
		}

		return result;
	}

	com_ptr<ID3D12Resource> draw_call_tracker::retrieve_depthstencil_from_handle(D3D12_CPU_DESCRIPTOR_HANDLE depthstencilView)
	{
		return _depthstencil_resources_by_handle[depthstencilView.ptr];
	}
	
	void draw_call_tracker::on_draw(ID3D12Device *device, UINT vertices)
	{
		UNREFERENCED_PARAMETER(device);

		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D12Resource> targets[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D12Resource> depthstencil = current_depthstencil;

		if (depthstencil == nullptr)
			// This is a draw call with no depth stencil
			return;

		if (const auto intermediate_snapshot = _counters_per_used_depthstencil.find(depthstencil); intermediate_snapshot != _counters_per_used_depthstencil.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;

			// Find the render targets, if they exist, and update their counts
			/*for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			{
				// Ignore empty slots
				if (targets[i] == nullptr)
					continue;

				if (const auto it = intermediate_snapshot->second.additional_views.find(targets[i].get()); it != intermediate_snapshot->second.additional_views.end())
				{
					it->second.vertices += vertices;
					it->second.drawcalls += 1;
				}
				else
				{
					// This shouldn't happen - it means somehow someone has called 'on_draw' with a render target without calling 'track_rendertargets' first
					assert(false);
				}
			}*/
		}
#endif
	}

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	bool draw_call_tracker::check_depthstencil(ID3D12Resource *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}
	bool draw_call_tracker::check_depth_texture_format(int formatIdx, ID3D12Resource *depthstencil)
	{
		assert(depthstencil != nullptr);

		// Do not check format if all formats are allowed (index zero is DXGI_FORMAT_UNKNOWN)
		if (formatIdx == DXGI_FORMAT_UNKNOWN)
			return true;

		// Retrieve texture associated with depth stencil
		D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();

		const DXGI_FORMAT depth_texture_formats[] = {
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_R16_TYPELESS,
			DXGI_FORMAT_R32_TYPELESS,
			DXGI_FORMAT_R24G8_TYPELESS,
			DXGI_FORMAT_R32G8X24_TYPELESS
		};

		assert(formatIdx > DXGI_FORMAT_UNKNOWN && formatIdx < ARRAYSIZE(depth_texture_formats));

		return make_dxgi_format_typeless(desc.Format) == depth_texture_formats[formatIdx];
	}

	void draw_call_tracker::track_rendertargets(int format_index, ID3D12Resource *depthstencil)
	{
		assert(depthstencil != nullptr);

		if (!check_depth_texture_format(format_index, depthstencil))
			return;

		if (_counters_per_used_depthstencil[depthstencil].depthstencil == nullptr)
			_counters_per_used_depthstencil[depthstencil].depthstencil = depthstencil;
	}
	void draw_call_tracker::track_depth_texture(int format_index, UINT index, com_ptr<ID3D12Resource> src_texture, com_ptr<ID3D12Resource> src_depthstencil, com_ptr<ID3D12Resource> dest_texture, bool cleared)
	{
		// Function that keeps track of a cleared depth texture in an ordered map in order to retrieve it at the final rendering stage
		assert(src_texture != nullptr);

		if (!check_depth_texture_format(format_index, src_depthstencil.get()))
			return;

		// Gather some extra info for later display
		D3D12_RESOURCE_DESC src_texture_desc = src_depthstencil->GetDesc();

		// check if it is really a depth texture
		assert((src_texture_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

		// fill the ordered map with the saved depth texture
		_cleared_depth_textures[index] = depth_texture_save_info { src_texture, src_depthstencil, src_texture_desc, dest_texture, cleared };
	}

	draw_call_tracker::intermediate_snapshot_info draw_call_tracker::find_best_snapshot(UINT width, UINT height)
	{
		const float aspect_ratio = float(width) / float(height);
		intermediate_snapshot_info best_snapshot;

		for (auto &[depthstencil, snapshot] : _counters_per_used_depthstencil)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue;

			if (snapshot.texture == nullptr)
			{
				com_ptr<ID3D12Resource> resource;
				snapshot.texture = depthstencil.get();
			}

			D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();

			assert((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0);

			// Check aspect ratio
			const float width_factor = desc.Width != width ? float(width) / desc.Width : 1.0f;
			const float height_factor = desc.Height != height ? float(height) / desc.Height : 1.0f;
			const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

			if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 2.0f || height_factor > 2.0f || width_factor < 0.5f || height_factor < 0.5f)
				continue; // No match, not a good fit

			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
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
