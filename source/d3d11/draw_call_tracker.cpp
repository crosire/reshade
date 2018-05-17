#include "draw_call_tracker.hpp"
#include "runtime.hpp"
#include <math.h>

namespace reshade::d3d11
{
	void draw_call_tracker::merge(const draw_call_tracker& source)
	{
		_global_counter.vertices += source.total_vertices();
		_global_counter.drawcalls += source.total_drawcalls();

		for (auto &[depthstencil, snapshot] : source._counters_per_used_depthstencil)
		{
			const auto destination_entry = _counters_per_used_depthstencil.find(depthstencil);

			_counters_per_used_depthstencil[depthstencil].counter.vertices += snapshot.counter.vertices;
			_counters_per_used_depthstencil[depthstencil].counter.drawcalls += snapshot.counter.drawcalls;
		}

		for (auto &[buffer, snapshot] : source._counters_per_constant_buffer)
		{
			const auto destination_entry = _counters_per_constant_buffer.find(buffer);

			_counters_per_constant_buffer[buffer].vertices += snapshot.vertices;
			_counters_per_constant_buffer[buffer].drawcalls += snapshot.drawcalls;
			_counters_per_constant_buffer[buffer].pixelshaders += snapshot.pixelshaders;
			_counters_per_constant_buffer[buffer].vertexshaders += snapshot.vertexshaders;
		}
	}

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
		_counters_per_used_depthstencil.clear();
		_counters_per_constant_buffer.clear();
	}

	void draw_call_tracker::on_draw(ID3D11DeviceContext *context, UINT vertices) {
		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

		ID3D11RenderTargetView *rendertargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
		ID3D11DepthStencilView *depthstencil = nullptr;

		context->OMGetRenderTargetsAndUnorderedAccessViews(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rendertargets, &depthstencil, 0, 0, nullptr);

		if (depthstencil == nullptr) {
			// this is a draw call with no depthstencil
			return;
		}

		const auto intermediate_snapshot = _counters_per_used_depthstencil.find(depthstencil);

		if (intermediate_snapshot != _counters_per_used_depthstencil.end())
		{
			intermediate_snapshot->second.counter.vertices += vertices;
			intermediate_snapshot->second.counter.drawcalls += 1;

			// Find the render targets, if they exist, and update their counts
			for (unsigned int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
				if (rendertargets[i] != nullptr) {
					auto it = intermediate_snapshot->second.additional_views.find(rendertargets[i]);
					if (it != intermediate_snapshot->second.additional_views.end()) {
						it->second.vertices += vertices;
						it->second.drawcalls += 1;
					}
					else {
						// This shouldn't happen - it means somehow someone has called "draw" with a rendertarget without calling track_rendertargets first
						assert(false);
					}
				}
			}
		}

		// Capture constant buffers that are used when depthstencils are drawn
		ID3D11Buffer *vscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = { nullptr };
		context->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, vscbuffers);

		for (unsigned int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
			// uses the default drawcalls = 0 the first time around.
			if (vscbuffers[i] != nullptr) {
				_counters_per_constant_buffer[vscbuffers[i]].vertexshaders += 1;
			}
		}

		ID3D11Buffer *pscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = { nullptr };
		context->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, pscbuffers);

		for (unsigned int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++) {
			// uses the default drawcalls = 0 the first time around.
			if (pscbuffers[i] != nullptr) {
				_counters_per_constant_buffer[pscbuffers[i]].pixelshaders += 1;
			}
		}

	}

	void draw_call_tracker::on_map(ID3D11DeviceContext *context, ID3D11Resource *pResource) {

		D3D11_RESOURCE_DIMENSION dim;
		pResource->GetType(&dim);

		if (dim == D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_BUFFER) {
			_counters_per_constant_buffer[reinterpret_cast<ID3D11Buffer*>(pResource)].mapped += 1;
		}
	}

	bool draw_call_tracker::check_depthstencil(ID3D11DepthStencilView *depthstencil) const
	{
		return _counters_per_used_depthstencil.find(depthstencil) != _counters_per_used_depthstencil.end();
	}

	void draw_call_tracker::track_rendertargets(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture, UINT numviews, ID3D11RenderTargetView *const *ppRenderTargetViews)
	{
		assert(depthstencil != nullptr);

		// if depthstencil is not already being tracked, this will create it
		_counters_per_used_depthstencil[depthstencil].depthstencil = _counters_per_used_depthstencil[depthstencil].depthstencil == nullptr ? depthstencil : _counters_per_used_depthstencil[depthstencil].depthstencil;
		_counters_per_used_depthstencil[depthstencil].texture = _counters_per_used_depthstencil[depthstencil].texture == nullptr ? texture : _counters_per_used_depthstencil[depthstencil].texture;

		for (unsigned int i = 0; i < numviews; i++) {
			// if the rendertarget isn't being tracked, this will create it
			_counters_per_used_depthstencil[depthstencil].additional_views[ppRenderTargetViews[i]].drawcalls += 1;
		}

	}

	void draw_call_tracker::update_tracked_depthtexture(ID3D11DepthStencilView *depthstencil, com_ptr<ID3D11Texture2D> texture) {
		auto it = _counters_per_used_depthstencil.find(depthstencil);
		if (it != _counters_per_used_depthstencil.end()) {
			it->second.texture = texture;
		}
	}

	std::pair<ID3D11DepthStencilView *, ID3D11Texture2D *> draw_call_tracker::find_best_depth_stencil(UINT width, UINT height, DXGI_FORMAT format)
	{
		const float aspect_ratio = float(width) / float(height);

		intermediate_snapshot_info  best_info = { };
		std::pair<ID3D11DepthStencilView *, ID3D11Texture2D *> best_match = { };

		for (const auto &[depthstencil, snapshot] : _counters_per_used_depthstencil)
		{

			if (snapshot.counter.drawcalls == 0 || snapshot.counter.vertices == 0)
			{
				continue;
			}


			if (snapshot.texture == nullptr)
			{
				com_ptr<ID3D11Texture2D> texture;
				com_ptr<ID3D11Resource> resource;
				depthstencil->GetResource(&resource);

				if (FAILED(resource->QueryInterface(&texture)))
				{
					continue;
				}

				best_info.texture = texture;
			}

			D3D11_TEXTURE2D_DESC desc;
			best_info.texture->GetDesc(&desc);
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

			if (snapshot.counter.drawcalls >= best_info.counter.drawcalls)
			{
				best_info = snapshot;
			}
		}

		// Warning: Reference to this object is lost after leaving the scope.
		// But that should be fine since either this tracker or the depth stencil should still have a reference on it left so that it stays alive.
		std::pair<ID3D11DepthStencilView *, ID3D11Texture2D *> response = {best_info.depthstencil.get(), best_info.texture.get()};

		return response;
	}
}
