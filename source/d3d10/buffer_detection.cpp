/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "buffer_detection.hpp"
#include "dxgi/format_utils.hpp"
#include <cmath>

#if RESHADE_DEPTH
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
#endif

void reshade::d3d10::buffer_detection::reset(bool release_resources)
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_counters_per_used_depth_texture.clear();

	if (release_resources)
	{
		_previous_stats = { 0, 0 };
		_depthstencil_clear_texture.reset();
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d10::buffer_detection::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	com_ptr<ID3D10DepthStencilView> depthstencil;
	_device->OMGetRenderTargets(0, nullptr, &depthstencil);

	const auto dsv_texture = texture_from_dsv(depthstencil.get());
	if (dsv_texture == nullptr)
		return; // This is a draw call with no depth-stencil bound

	auto &counters = _counters_per_used_depth_texture[dsv_texture];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices;
	counters.current_stats.drawcalls += 1;
#endif
}

#if RESHADE_DEPTH
void reshade::d3d10::buffer_detection::on_clear_depthstencil(UINT clear_flags, ID3D10DepthStencilView *dsv)
{
	if ((clear_flags & D3D10_CLEAR_DEPTH) == 0)
		return;

	com_ptr<ID3D10Texture2D> dsv_texture = texture_from_dsv(dsv);
	if (dsv_texture == nullptr || dsv_texture != _depthstencil_clear_index.first)
		return;

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	// Update stats with data from previous frame
	if (counters.current_stats.drawcalls == 0)
		counters.current_stats = _previous_stats;

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (_depthstencil_clear_index.second == std::numeric_limits<UINT>::max() ?
		counters.current_stats.vertices > _best_copy_stats.vertices :
		counters.clears.size() == _depthstencil_clear_index.second)
	{
		_best_copy_stats = counters.current_stats;

		_device->CopyResource(_depthstencil_clear_texture.get(), dsv_texture.get());
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

bool reshade::d3d10::buffer_detection::update_depthstencil_clear_texture(D3D10_TEXTURE2D_DESC desc)
{
	if (_depthstencil_clear_texture != nullptr)
	{
		D3D10_TEXTURE2D_DESC existing_desc;
		_depthstencil_clear_texture->GetDesc(&existing_desc);

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use
		else
			_depthstencil_clear_texture.reset();
	}

	desc.Format = make_dxgi_format_typeless(desc.Format);
	desc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;

	if (HRESULT hr = _device->CreateTexture2D(&desc, nullptr, &_depthstencil_clear_texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

com_ptr<ID3D10Texture2D> reshade::d3d10::buffer_detection::find_best_depth_texture(UINT width, UINT height, com_ptr<ID3D10Texture2D> override, UINT clear_index_override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D10Texture2D> best_match;

	if (override != nullptr)
	{
		best_match = std::move(override);
		best_snapshot = _counters_per_used_depth_texture[best_match];
	}
	else
	{
		for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
		{
			if (snapshot.total_stats.drawcalls == 0)
				continue; // Skip unused

			D3D10_TEXTURE2D_DESC desc; dsv_texture->GetDesc(&desc);
			assert((desc.BindFlags & D3D10_BIND_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			assert((desc.BindFlags & D3D10_BIND_SHADER_RESOURCE) != 0);

			if (width != 0 && height != 0)
			{
				const float w = static_cast<float>(width);
				const float w_ratio = w / desc.Width;
				const float h = static_cast<float>(height);
				const float h_ratio = h / desc.Height;
				const float aspect_ratio = (w / h) - (static_cast<float>(desc.Width) / desc.Height);

				if (std::fabs(aspect_ratio) > 0.1f || w_ratio > 1.85f || h_ratio > 1.85f || w_ratio < 0.5f || h_ratio < 0.5f)
					continue; // Not a good fit
			}

			// Choose snapshot with the most vertices, since that is likely to contain the main scene
			if (snapshot.total_stats.vertices > best_snapshot.total_stats.vertices)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;
			}
		}
	}

	if (clear_index_override != 0 && best_match != nullptr)
	{
		_previous_stats = best_snapshot.current_stats;
		_depthstencil_clear_index = { best_match.get(), std::numeric_limits<UINT>::max() };

		if (clear_index_override <= best_snapshot.clears.size())
		{
			_depthstencil_clear_index.second = clear_index_override;
		}

		D3D10_TEXTURE2D_DESC desc;
		best_match->GetDesc(&desc);

		if (update_depthstencil_clear_texture(desc))
		{
			return _depthstencil_clear_texture;
		}
	}

	_depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };

	return best_match;
}
#endif
