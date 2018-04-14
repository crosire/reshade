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
	}

	void draw_call_tracker::reset()
	{
		_counters.vertices = 0;
		_counters.drawcalls = 0;
		_counters_per_used_depthstencil.clear();
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
}
