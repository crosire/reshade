#include "draw_call_tracker.hpp"
#include "runtime.hpp"
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

		for (auto source_entry : source._cleared_depth_textures)
		{
			const auto destination_entry = _cleared_depth_textures.find(source_entry.first);

			if (destination_entry == _cleared_depth_textures.end())
			{
				_cleared_depth_textures.emplace(source_entry.first, source_entry.second);
			}
		}
	}

	void draw_call_tracker::reset()
	{
		_counters.vertices = 0;
		_counters.drawcalls = 0;
		_counters_per_used_depthstencil.clear();
		_cleared_depth_textures.clear();
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

	bool draw_call_tracker::check_depthstencil(ID3D11DepthStencilView *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}
	void draw_call_tracker::track_depthstencil(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture)
	{
		assert(depthstencil != nullptr);

		if (!check_depthstencil(depthstencil))
		{
			_counters_per_used_depthstencil.emplace(depthstencil, depthstencil_counter_info { 0, 0, texture });
		}
	}

	std::pair<ID3D11DepthStencilView *, ID3D11Texture2D *> draw_call_tracker::find_best_depth_stencil(UINT width, UINT height, DXGI_FORMAT format)
	{
		const float aspect_ratio = float(width) / float(height);

		depthstencil_counter_info best_info = { };
		std::pair<ID3D11DepthStencilView *, ID3D11Texture2D *> best_match = { };

		for (const auto &it : _counters_per_used_depthstencil)
		{
			const auto depthstencil = it.first;
			auto &depthstencil_info = it.second;

			if (depthstencil_info.drawcalls == 0 || depthstencil_info.vertices == 0)
			{
				continue;
			}

			com_ptr<ID3D11Texture2D> texture;

			if (depthstencil_info.texture != nullptr)
			{
				texture = depthstencil_info.texture;
			}
			else
			{
				com_ptr<ID3D11Resource> resource;
				depthstencil->GetResource(&resource);

				if (FAILED(resource->QueryInterface(&texture)))
				{
					continue;
				}
			}

			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

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

			// Check texture format
			if (format != DXGI_FORMAT_UNKNOWN && desc.Format != format)
			{
				// No match, texture format does not equal filter
				continue;
			}

			if (depthstencil_info.drawcalls >= best_info.drawcalls)
			{
				best_match.first = depthstencil.get();
				// Warning: Reference to this object is lost after leaving the scope.
				// But that should be fine since either this tracker or the depth stencil should still have a reference on it left so that it stays alive.
				best_match.second = texture.get();

				best_info.vertices = depthstencil_info.vertices;
				best_info.drawcalls = depthstencil_info.drawcalls;
			}
		}

		return best_match;
	}
	/* function that keeps track of a cleared depth texture in an ordered map, in order to retrieve it at the final rendering stage */
	/* gathers some extra infos in order to display it on a select window */
	void draw_call_tracker::track_depth_texture(UINT index, com_ptr<ID3D11Texture2D> src_texture, com_ptr<ID3D11Texture2D> dest_texture)
	{
		assert(src_texture != nullptr);

		D3D11_TEXTURE2D_DESC src_texture_desc;
		src_texture->GetDesc(&src_texture_desc);

		// check if it is really a depth texture
		assert((src_texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

		const auto it = _cleared_depth_textures.find(index);

		// fill the ordered map with the saved depth texture
		if (it == _cleared_depth_textures.end())
		{
			_cleared_depth_textures.emplace(index, depth_texture_save_info{ src_texture, src_texture_desc, dest_texture });
		}
		else
		{
			it->second = depth_texture_save_info{ src_texture, src_texture_desc, dest_texture };
		}
	}
	/* function that selects the best cleared depth texture according to the clearing number defined in the configuration settings */
	ID3D11Texture2D *draw_call_tracker::find_best_cleared_depth_buffer_texture(DXGI_FORMAT format, UINT depth_buffer_clearing_number)
	{
		ID3D11Texture2D *best_match = nullptr;

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

			// Check texture format
			if (format != DXGI_FORMAT_UNKNOWN && desc.Format != format)
			{
				// No match, texture format does not equal filter
				continue;
			}

			if (depth_buffer_clearing_number != 0 && i > depth_buffer_clearing_number)
			{
				continue;
			}

			// the _cleared_dept_textures ordered map stores the depth textures, according to the order of clearing
			// if depth_buffer_clearing_number == 0, the auto select mode is defined, so the last cleared depth texture is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is greater or equal than it, the texture corresponding to this number is retrieved
			// if the user selects a clearing number and the number of cleared depth textures is lower than it, the last cleared depth texture is retrieved
			best_match = texture.get();
		}

		return best_match;
	}
}
