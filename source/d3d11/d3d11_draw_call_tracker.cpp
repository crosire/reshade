#include "d3d11_draw_call_tracker.hpp"
#include "log.hpp"
#include "runtime.hpp"
#include <math.h>

namespace reshade::d3d11
{
	void d3d11_draw_call_tracker::merge(const d3d11_draw_call_tracker& source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depthstencil)
		{
			_counters_per_used_depthstencil[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depthstencil[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
		}

		for (auto source_entry : source._cleared_depth_textures)
		{
			const auto destination_entry = _cleared_depth_textures.find(source_entry.first);

			if (destination_entry == _cleared_depth_textures.end())
			{
				_cleared_depth_textures.emplace(source_entry.first, source_entry.second);
			}
		}
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		for (const auto &[buffer, snapshot] : source._counters_per_constant_buffer)
		{
			_counters_per_constant_buffer[buffer].vertices += snapshot.vertices;
			_counters_per_constant_buffer[buffer].drawcalls += snapshot.drawcalls;
			_counters_per_constant_buffer[buffer].ps_uses += snapshot.ps_uses;
			_counters_per_constant_buffer[buffer].vs_uses += snapshot.vs_uses;
		}
#endif
	}

	void d3d11_draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depthstencil.clear();
		_cleared_depth_textures.clear();
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		_counters_per_constant_buffer.clear();
#endif
	}

	void d3d11_draw_call_tracker::on_map(ID3D11Resource *resource)
	{
		UNREFERENCED_PARAMETER(resource);

#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		D3D11_RESOURCE_DIMENSION dim;
		resource->GetType(&dim);

		if (dim == D3D11_RESOURCE_DIMENSION_BUFFER)
		{
			_counters_per_constant_buffer[static_cast<ID3D11Buffer *>(resource)].mapped += 1;
		}
#endif
	}

	void d3d11_draw_call_tracker::on_draw(ID3D11DeviceContext *context, UINT vertices)
	{
		UNREFERENCED_PARAMETER(context);

		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D11RenderTargetView> targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D11DepthStencilView> depthstencil;

		context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D11RenderTargetView **>(targets), &depthstencil);

		if (depthstencil == nullptr)
			// This is a draw call with no depth stencil
			return;

		if (const auto intermediate_snapshot = _counters_per_used_depthstencil.find(depthstencil); intermediate_snapshot != _counters_per_used_depthstencil.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;

			// Find the render targets, if they exist, and update their counts
			for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
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
					LOG(ERROR) << "Draw has been called on an untracked render target.";
				}
			}
		}
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		// Capture constant buffers that are used when depth stencils are drawn
		com_ptr<ID3D11Buffer> vscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		context->VSGetConstantBuffers(0, ARRAYSIZE(vscbuffers), reinterpret_cast<ID3D11Buffer **>(vscbuffers));

		for (UINT i = 0; i < ARRAYSIZE(vscbuffers); i++)
		{
			// Uses the default drawcalls = 0 the first time around.
			if (vscbuffers[i] != nullptr)
			{
				_counters_per_constant_buffer[vscbuffers[i]].vs_uses += 1;
			}
		}

		com_ptr<ID3D11Buffer> pscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		context->PSGetConstantBuffers(0, ARRAYSIZE(pscbuffers), reinterpret_cast<ID3D11Buffer **>(pscbuffers));

		for (UINT i = 0; i < ARRAYSIZE(pscbuffers); i++)
		{
			// Uses the default drawcalls = 0 the first time around.
			if (pscbuffers[i] != nullptr)
			{
				_counters_per_constant_buffer[pscbuffers[i]].ps_uses += 1;
			}
		}
#endif
	}

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	bool d3d11_draw_call_tracker::check_depthstencil(ID3D11DepthStencilView *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}

	void d3d11_draw_call_tracker::track_rendertargets(ID3D11DepthStencilView *depthstencil, UINT num_views, ID3D11RenderTargetView *const *views)
	{
		assert(depthstencil != nullptr);

		if (_counters_per_used_depthstencil[depthstencil].depthstencil == nullptr)
			_counters_per_used_depthstencil[depthstencil].depthstencil = depthstencil;

		for (UINT i = 0; i < num_views; i++)
		{
			// If the render target isn't being tracked, this will create it
			_counters_per_used_depthstencil[depthstencil].additional_views[views[i]].drawcalls += 1;
		}
	}
	void d3d11_draw_call_tracker::track_depth_texture(UINT index, com_ptr<ID3D11Texture2D> src_texture, com_ptr<ID3D11DepthStencilView> src_depthstencil, com_ptr<ID3D11Texture2D> dest_texture, bool cleared)
	{
		// Function that keeps track of a cleared depth texture in an ordered map in order to retrieve it at the final rendering stage
		assert(src_texture != nullptr);

		// Gather some extra info for later display
		D3D11_TEXTURE2D_DESC src_texture_desc;
		src_texture->GetDesc(&src_texture_desc);

		// check if it is really a depth texture
		assert((src_texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

		// fill the ordered map with the saved depth texture
		if (const auto it = _cleared_depth_textures.find(index); it == _cleared_depth_textures.end())
		{
			_cleared_depth_textures.emplace(index, depth_texture_save_info{ src_texture, src_depthstencil, src_texture_desc, dest_texture, cleared });
		}
		else
		{
			it->second = depth_texture_save_info{ src_texture, src_depthstencil, src_texture_desc, dest_texture, cleared };
		}
	}

	d3d11_draw_call_tracker::intermediate_snapshot_info d3d11_draw_call_tracker::find_best_snapshot(UINT width, UINT height)
	{
		const float aspect_ratio = float(width) / float(height);

		intermediate_snapshot_info best_snapshot;

		for (auto &[depthstencil, snapshot] : _counters_per_used_depthstencil)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
			{
				continue;
			}

			if (snapshot.texture == nullptr)
			{
				com_ptr<ID3D11Resource> resource;
				depthstencil->GetResource(&resource);

				if (FAILED(resource->QueryInterface(&snapshot.texture)))
				{
					continue;
				}
			}

			D3D11_TEXTURE2D_DESC desc;
			snapshot.texture->GetDesc(&desc);

			assert((desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

			// Check aspect ratio
			const float width_factor = desc.Width != width ? float(width) / desc.Width : 1.0f;
			const float height_factor = desc.Height != height ? float(height) / desc.Height : 1.0f;
			const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

			if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 2.0f || height_factor > 2.0f || width_factor < 0.5f || height_factor < 0.5f)
			{
				// No match, not a good fit
				continue;
			}

			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
			{
				best_snapshot = snapshot;
			}
		}

		return best_snapshot;
	}

	void d3d11_draw_call_tracker::keep_cleared_depth_textures()
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

	ID3D11Texture2D *d3d11_draw_call_tracker::find_best_cleared_depth_buffer_texture(UINT depth_buffer_clearing_number)
	{
		// Function that selects the best cleared depth texture according to the clearing number defined in the configuration settings
		ID3D11Texture2D *best_match = nullptr;

		// Ensure to work only on the depth textures retrieved before the last depth stencil clearance
		keep_cleared_depth_textures();

		for (const auto &it : _cleared_depth_textures)
		{
			UINT i = it.first;
			auto &texture_counter_info = it.second;

			com_ptr<ID3D11Texture2D> texture;

			if (texture_counter_info.dest_texture == nullptr)
			{
				continue;
			}

			texture = texture_counter_info.dest_texture;

			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			if (depth_buffer_clearing_number != 0 && i > depth_buffer_clearing_number)
			{
				continue;
			}

			// The _cleared_dept_textures ordered map stores the depth textures, according to the order of clearing
			// if depth_buffer_clearing_number == 0, the auto select mode is defined, so the last cleared depth texture is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is greater or equal than it, the texture corresponding to this number is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is lower than it, the last cleared depth texture is retrieved
			best_match = texture.get();
		}

		return best_match;
	}
#endif
}