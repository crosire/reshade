#include "draw_call_tracker.hpp"
#include "dxgi/format_utils.hpp"
#include "runtime_d3d10.hpp"
#include <math.h>

namespace reshade::d3d10
{
	static inline com_ptr<ID3D10Texture2D> texture_from_dsv(ID3D10DepthStencilView *dsv)
	{
		if (dsv == nullptr)
			return nullptr;
		com_ptr<ID3D10Resource> resource;
		dsv->GetResource(&resource);
		com_ptr<ID3D10Texture2D> texture;
		resource->QueryInterface(&texture);
		return texture;
	}

	void draw_call_tracker::reset()
	{
		_global_counter.vertices = 0;
		_global_counter.drawcalls = 0;
#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
		_cleared_depth_textures.clear();
		_counters_per_used_depth_texture.clear();
#endif
#if RESHADE_DX10_CAPTURE_CONSTANT_BUFFERS
		_counters_per_constant_buffer.clear();
#endif
	}

	void draw_call_tracker::on_map(ID3D10Resource *resource)
	{
		UNREFERENCED_PARAMETER(resource);

#if RESHADE_DX10_CAPTURE_CONSTANT_BUFFERS
		D3D10_RESOURCE_DIMENSION dim;
		resource->GetType(&dim);

		if (dim == D3D10_RESOURCE_DIMENSION_BUFFER)
			_counters_per_constant_buffer[static_cast<ID3D10Buffer *>(resource)].mapped += 1;
#endif
	}

	void draw_call_tracker::on_draw(ID3D10Device *device, UINT vertices)
	{
		_global_counter.vertices += vertices;
		_global_counter.drawcalls += 1;

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D10RenderTargetView> targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		com_ptr<ID3D10DepthStencilView> depthstencil;
		device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D10RenderTargetView **>(targets), &depthstencil);

		const auto dsv_texture = texture_from_dsv(depthstencil.get());
		if (dsv_texture == nullptr)
			return; // This is a draw call with no depth stencil

		if (const auto intermediate_snapshot = _counters_per_used_depth_texture.find(dsv_texture);
			intermediate_snapshot != _counters_per_used_depth_texture.end())
		{
			intermediate_snapshot->second.stats.vertices += vertices;
			intermediate_snapshot->second.stats.drawcalls += 1;

			// Find the render targets, if they exist, and update their counts
			for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
			{
				if (targets[i] == nullptr)
					continue; // Ignore empty slots

				if (const auto it = intermediate_snapshot->second.additional_views.find(targets[i].get());
					it != intermediate_snapshot->second.additional_views.end())
				{
					it->second.vertices += vertices;
					it->second.drawcalls += 1;
				}
				else
				{
					// This shouldn't happen - it means somehow 'on_draw' was called with a render target without calling 'track_render_targets' on it first
					assert(false);
				}
			}
		}
#endif
#if RESHADE_DX10_CAPTURE_CONSTANT_BUFFERS
		// Capture constant buffers that are used when depth stencils are drawn
		com_ptr<ID3D10Buffer> vscbuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		device->VSGetConstantBuffers(0, ARRAYSIZE(vscbuffers), reinterpret_cast<ID3D10Buffer **>(vscbuffers));

		for (UINT i = 0; i < ARRAYSIZE(vscbuffers); i++)
			// Uses the default drawcalls = 0 the first time around.
			if (vscbuffers[i] != nullptr)
				_counters_per_constant_buffer[vscbuffers[i]].vs_uses += 1;

		com_ptr<ID3D10Buffer> pscbuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		device->PSGetConstantBuffers(0, ARRAYSIZE(pscbuffers), reinterpret_cast<ID3D10Buffer **>(pscbuffers));

		for (UINT i = 0; i < ARRAYSIZE(pscbuffers); i++)
			// Uses the default drawcalls = 0 the first time around.
			if (pscbuffers[i] != nullptr)
				_counters_per_constant_buffer[pscbuffers[i]].ps_uses += 1;
#endif
	}

#if RESHADE_DX10_CAPTURE_DEPTH_BUFFERS
	bool draw_call_tracker::filter_aspect_ratio = true;
	bool draw_call_tracker::preserve_depth_buffers = false;
	bool draw_call_tracker::preserve_stencil_buffers = false;
	unsigned int draw_call_tracker::depth_stencil_clear_index = 0;
	unsigned int draw_call_tracker::filter_depth_texture_format = 0;

	void draw_call_tracker::track_render_targets(UINT num_views, ID3D10RenderTargetView *const *views, ID3D10DepthStencilView *dsv)
	{
		const auto dsv_texture = texture_from_dsv(dsv);
		if (dsv_texture == nullptr)
			return;

		// Add new entry for this DSV
		auto &counters = _counters_per_used_depth_texture[dsv_texture];

		for (UINT i = 0; i < num_views; i++)
		{
			// If the render target isn't being tracked, this will create it
			counters.additional_views[views[i]].drawcalls += 1;
		}
	}
	void draw_call_tracker::track_cleared_depthstencil(ID3D10Device *device, UINT clear_flags, ID3D10DepthStencilView *dsv, UINT clear_index, runtime_d3d10 *runtime)
	{
		if (!(preserve_depth_buffers && (clear_flags & D3D10_CLEAR_DEPTH)) &&
			!(preserve_stencil_buffers && (clear_flags & D3D10_CLEAR_STENCIL)))
			return;

		com_ptr<ID3D10Texture2D> dsv_texture = texture_from_dsv(dsv);
		com_ptr<ID3D10Texture2D> backup_texture;
		if (dsv_texture == nullptr)
			return;

		D3D10_TEXTURE2D_DESC desc;
		dsv_texture->GetDesc(&desc);
		assert((desc.BindFlags & D3D10_BIND_DEPTH_STENCIL) != 0);

		if (desc.SampleDesc.Count > 1)
			return; // Ignore MSAA textures
		if (!check_texture_format(desc))
			return;

		// Make a backup copy of depth textures that are cleared by the application
		if ((depth_stencil_clear_index == 0) || (clear_index == depth_stencil_clear_index))
		{
			backup_texture = runtime->create_compatible_texture(desc);
			if (backup_texture == nullptr)
				return;

			device->CopyResource(backup_texture.get(), dsv_texture.get());
		}

		_cleared_depth_textures.insert({ clear_index, { std::move(dsv_texture), std::move(backup_texture) } });
	}

	bool draw_call_tracker::check_aspect_ratio(const D3D10_TEXTURE2D_DESC &desc, UINT width, UINT height)
	{
		if (!filter_aspect_ratio)
			return true;

		const float aspect_ratio = float(width) / float(height);
		const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

		const float width_factor = float(width) / float(desc.Width);
		const float height_factor = float(height) / float(desc.Height);

		return !(fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f);
	}
	bool draw_call_tracker::check_texture_format(const D3D10_TEXTURE2D_DESC &desc)
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

	com_ptr<ID3D10Texture2D> draw_call_tracker::find_best_depth_texture(UINT width, UINT height)
	{
		com_ptr<ID3D10Texture2D> best_texture;

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

				D3D10_TEXTURE2D_DESC desc;
				snapshot.dsv_texture->GetDesc(&desc);

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

				D3D10_TEXTURE2D_DESC desc;
				dsv_texture->GetDesc(&desc);
				assert((desc.BindFlags & D3D10_BIND_DEPTH_STENCIL) != 0);

				if (desc.SampleDesc.Count > 1)
					continue; // Ignore MSAA textures, since they would need to be resolved first
				if (!check_aspect_ratio(desc, width, height))
					continue; // Not a good fit
				if (!check_texture_format(desc))
					continue;

				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				if (snapshot.stats.vertices >= best_snapshot.stats.vertices)
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
