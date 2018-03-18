#include "draw_call_tracker.hpp"
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

			if (reshade::runtime::depth_buffer_retrieval_mode == reshade::runtime::depth_buffer_retrieval_mode::post_process)
			{
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
	}

	void draw_call_tracker::reset()
	{
		_counters.vertices = 0;
		_counters.drawcalls = 0;
		_counters_per_used_depthstencil.clear();
		_active_depth_texture.reset();
	}

	void draw_call_tracker::set_depth_texture(ID3D11Texture2D* depth_texture)
	{
		assert(depth_texture != nullptr);

		_active_depth_texture = depth_texture;
	}

	void draw_call_tracker::track_depthstencil(ID3D11DepthStencilView* depthstencil)
	{
		assert(depthstencil != nullptr);

		if (_counters_per_used_depthstencil.find(depthstencil) == _counters_per_used_depthstencil.end())
		{
			_counters_per_used_depthstencil.emplace(depthstencil, depthstencil_counter_info());
		}
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

	ID3D11DepthStencilView* draw_call_tracker::get_best_depth_stencil(const com_ptr<ID3D11Device> device, const com_ptr<ID3D11DeviceContext> devicecontext, UINT width, UINT height, DXGI_FORMAT depth_buffer_texture_format)
	{
		depthstencil_counter_info best_info = { 0 };
		ID3D11DepthStencilView *best_match = nullptr;
		float aspect_ratio = ((float)width) / ((float)height);

		for (auto it : _counters_per_used_depthstencil)
		{
			const auto depthstencil = it.first;
			auto &depthstencil_info = it.second;
			float twfactor = 1.0f;
			float thfactor = 1.0f;

			if (reshade::runtime::depth_buffer_retrieval_mode == reshade::runtime::depth_buffer_retrieval_mode::post_process)
			{
				if (depthstencil_info.drawcalls == 0 || depthstencil_info.vertices == 0)
				{
					continue;
				}
			}

			D3D11_DEPTH_STENCIL_VIEW_DESC desc;
			depthstencil.get()->GetDesc(&desc);

			// Determine depthstencil size on-the-fly. We execute this code in present, and this isn't slow anyway. 
			// Caching the size infos has the downside that if the game uses dynamic supersampling/resolution scaling, we end up
			// with a lot of depthstencils which are likely out of scope leading to memory leaks. 
			D3D11_TEXTURE2D_DESC texture_desc;
			com_ptr<ID3D11Resource> resource;
			com_ptr<ID3D11Texture2D> texture;
			depthstencil->GetResource(&resource);
			if (FAILED(resource->QueryInterface(&texture)))
			{
				continue;
			}
			texture->GetDesc(&texture_desc);

			if (texture_desc.Width != width)
			{
				twfactor = (float)width / texture_desc.Width;
			}
			if (texture_desc.Height != height)
			{
				thfactor = (float)height / texture_desc.Height;
			}

			// check aspect ratio. 
			float stencilaspectratio = ((float)texture_desc.Width) / ((float)texture_desc.Height);
			if (fabs(stencilaspectratio - aspect_ratio) > 0.1f)
			{
				// no match, not a good fit
				continue;
			}

			if (twfactor > 2.0f || thfactor > 2.0f)
			{
				continue;
			}

			if (twfactor < 0.5f || thfactor < 0.5f)
			{
				continue;
			}

			if (depth_buffer_texture_format != DXGI_FORMAT_UNKNOWN)
			{
				// we check the texture format, if filtered
				if (texture_desc.Format != depth_buffer_texture_format)
				{
					continue;
				}
			}

			if (reshade::runtime::depth_buffer_retrieval_mode == reshade::runtime::depth_buffer_retrieval_mode::post_process)
			{
				if (depthstencil_info.drawcalls >= best_info.drawcalls)
				{
					best_match = depthstencil.get();
					best_info.vertices = depthstencil_info.vertices;
					best_info.drawcalls = depthstencil_info.drawcalls;
				}
			}

			if (reshade::runtime::depth_buffer_retrieval_mode == reshade::runtime::depth_buffer_retrieval_mode::before_clearing_stage || reshade::runtime::depth_buffer_retrieval_mode == reshade::runtime::depth_buffer_retrieval_mode::at_om_stage)
			{
				best_match = depthstencil.get();
			}
		}

		return best_match;
	}

	ID3D11Texture2D* draw_call_tracker::get_depth_texture()
	{
		return _active_depth_texture.get();
	}

	draw_call_tracker::depthstencil_counter_info draw_call_tracker::get_counters()
	{
		return _counters;
	}
}
