#include "draw_call_tracker.hpp"
#include "log.hpp"
#include "runtime.hpp"
#include <math.h>

namespace reshade::d3d11
{
	void draw_call_tracker::merge(const draw_call_tracker& source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		for (const auto &[depthstencil, snapshot] : source._counters_per_used_depthstencil)
		{
			_counters_per_used_depthstencil[depthstencil].stats.vertices += snapshot.stats.vertices;
			_counters_per_used_depthstencil[depthstencil].stats.drawcalls += snapshot.stats.drawcalls;
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

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
		_counters_per_used_depthstencil.clear();
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
		_counters_per_constant_buffer.clear();
#endif
	}

	void draw_call_tracker::on_map(ID3D11Resource *resource)
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

	void draw_call_tracker::on_draw(ID3D11DeviceContext *context, UINT vertices)
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
	bool draw_call_tracker::check_depthstencil(ID3D11DepthStencilView *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}

	void draw_call_tracker::track_rendertargets(ID3D11DepthStencilView *depthstencil, UINT num_views, ID3D11RenderTargetView *const *views)
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

	void draw_call_tracker::update_tracked_depthtexture(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture)
	{
		if (const auto it = _counters_per_used_depthstencil.find(depthstencil); it != _counters_per_used_depthstencil.end())
		{
			it->second.texture = texture;
		}
	}

	draw_call_tracker::intermediate_snapshot_info draw_call_tracker::find_best_snapshot(UINT width, UINT height, DXGI_FORMAT format)
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

			// Check texture format
			if (format != DXGI_FORMAT_UNKNOWN && desc.Format != format)
			{
				// No match, texture format does not equal filter
				continue;
			}

			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
			{
				best_snapshot = snapshot;
			}
		}

		return best_snapshot;
	}
#endif
}
