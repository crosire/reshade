/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "state_tracking.hpp"
#include "dxgi/format_utils.hpp"
#include <cmath>

#if RESHADE_DEPTH
static inline com_ptr<ID3D11Texture2D> texture_from_dsv(ID3D11DepthStencilView *dsv)
{
	if (dsv == nullptr)
		return nullptr;
	com_ptr<ID3D11Resource> resource;
	dsv->GetResource(&resource);
	assert(resource != nullptr);
	com_ptr<ID3D11Texture2D> texture;
	resource->QueryInterface(&texture);
	return texture;
}
#endif

void reshade::d3d11::state_tracking::init(ID3D11DeviceContext *device_context, const state_tracking_context *context)
{
	_device_context = device_context;
	_context = context;
}

void reshade::d3d11::state_tracking::reset()
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_first_empty_stats = true;
	_has_indirect_drawcalls = false;
	_counters_per_used_depth_texture.clear();
#endif
}
void reshade::d3d11::state_tracking_context::reset(bool release_resources)
{
	state_tracking::reset();

#if RESHADE_DEPTH
	assert(_depthstencil_clear_texture == nullptr || _context == this);

	if (release_resources)
	{
		assert(_context == this);

		_previous_stats = { 0, 0 };
		_depthstencil_clear_texture.reset();
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d11::state_tracking::merge(const state_tracking &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DEPTH
	if (_first_empty_stats)
		_first_empty_stats = source._first_empty_stats;
	_has_indirect_drawcalls |= source._has_indirect_drawcalls;

	if (source._best_copy_stats.vertices > _best_copy_stats.vertices)
		_best_copy_stats = source._best_copy_stats;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.total_stats.vertices += snapshot.total_stats.vertices;
		target_snapshot.total_stats.drawcalls += snapshot.total_stats.drawcalls;
		target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
		target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
	}
#endif
}

void reshade::d3d11::state_tracking::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	com_ptr<ID3D11DepthStencilView> depthstencil;
	_device_context->OMGetRenderTargets(0, nullptr, &depthstencil);

	const auto dsv_texture = texture_from_dsv(depthstencil.get());
	if (dsv_texture == nullptr)
		return; // This is a draw call with no depth-stencil bound

	// See 'D3D11DeviceContext::DrawInstancedIndirect' and 'D3D11DeviceContext::DrawIndexedInstancedIndirect'
	if (vertices == 0)
		_has_indirect_drawcalls = true;

	// Check if this draw call likely represets a fullscreen rectangle (one or two triangles), which would clear the depth-stencil
	if (_context->preserve_depth_buffers && vertices <= 6)
	{
		D3D11_RASTERIZER_DESC rs_desc;
		com_ptr<ID3D11RasterizerState> rs;
		_device_context->RSGetState(&rs);
		assert(rs != nullptr);
		rs->GetDesc(&rs_desc);

		UINT stencil_ref_value;
		D3D11_DEPTH_STENCIL_DESC dss_desc;
		com_ptr<ID3D11DepthStencilState> dss;
		_device_context->OMGetDepthStencilState(&dss, &stencil_ref_value);
		assert(dss != nullptr);
		dss->GetDesc(&dss_desc);

		if (rs_desc.CullMode == D3D11_CULL_NONE && dss_desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL && dss_desc.DepthEnable == TRUE && dss_desc.DepthFunc == D3D11_COMPARISON_ALWAYS)
		{
			on_clear_depthstencil(D3D11_CLEAR_DEPTH, depthstencil.get(), true);
		}
	}

	auto &counters = _counters_per_used_depth_texture[dsv_texture];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices;
	counters.current_stats.drawcalls += 1;
#endif
}

#if RESHADE_DEPTH
void reshade::d3d11::state_tracking::on_clear_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv, bool fullscreen_draw_call)
{
	assert(_context != nullptr);

	if ((clear_flags & D3D11_CLEAR_DEPTH) == 0 || !_context->preserve_depth_buffers)
		return;

	com_ptr<ID3D11Texture2D> dsv_texture = texture_from_dsv(dsv);
	if (dsv_texture == nullptr || _context->_depthstencil_clear_texture == nullptr || dsv_texture != _context->depthstencil_clear_index.first)
		return;

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	// Update stats with data from previous frame
	if (!fullscreen_draw_call && counters.current_stats.drawcalls == 0 && _first_empty_stats)
	{
		counters.current_stats = _context->_previous_stats;
		_first_empty_stats = false;
	}

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	if (fullscreen_draw_call)
		counters.current_stats.rect = true;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (_context->depthstencil_clear_index.second == 0 ?
		// If clear index override is set to zero, always copy any suitable buffers
		fullscreen_draw_call || counters.current_stats.vertices > _best_copy_stats.vertices :
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.clears.size() == _context->depthstencil_clear_index.second)
	{
		// Since clears from fullscreen draw calls are selected based on their order (last one wins), their stats are ignored for the regular clear heuristic
		if (!fullscreen_draw_call)
			_best_copy_stats = counters.current_stats;

		_device_context->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

bool reshade::d3d11::state_tracking_context::update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc)
{
	if (_depthstencil_clear_texture != nullptr)
	{
		D3D11_TEXTURE2D_DESC existing_desc;
		_depthstencil_clear_texture->GetDesc(&existing_desc);

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use
		else
			_depthstencil_clear_texture.reset();
	}

	assert(_device_context != nullptr);

	desc.Format = make_dxgi_format_typeless(desc.Format);
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	com_ptr<ID3D11Device> device;
	_device_context->GetDevice(&device);

	if (HRESULT hr = device->CreateTexture2D(&desc, nullptr, &_depthstencil_clear_texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

std::vector<std::pair<ID3D11Texture2D*, reshade::d3d11::state_tracking::depthstencil_info>> reshade::d3d11::state_tracking_context::sorted_counters_per_used_depthstencil()
{
	struct pair_wrapper
	{
		int display_count;
		ID3D11Texture2D* wrapped_dsv_texture;
		depthstencil_info wrapped_depthstencil_info;
		UINT64 depthstencil_width;
		UINT64 depthstencil_height;
	};

	std::vector<pair_wrapper> sorted_counters_per_buffer;
	sorted_counters_per_buffer.reserve(_counters_per_used_depth_texture.size());

	for (const auto& [dsv_texture, snapshot] : _counters_per_used_depth_texture)
	{
		auto dsv_texture_instance = dsv_texture.get();
		D3D11_TEXTURE2D_DESC desc;
		dsv_texture->GetDesc(&desc);
		// get the display counter, if any of this texture.
		auto entry = _shown_count_per_depthstencil_address.find(dsv_texture_instance);
		if (entry == _shown_count_per_depthstencil_address.end())
		{
			sorted_counters_per_buffer.push_back({ 1, dsv_texture_instance, snapshot, desc.Width, desc.Height });
		}
		else
		{
			auto shown_count = entry->second;
			sorted_counters_per_buffer.push_back({ ++shown_count, dsv_texture_instance, snapshot, desc.Width, desc.Height });
		}
	}
	// sort it
	std::sort(sorted_counters_per_buffer.begin(), sorted_counters_per_buffer.end(), [](const pair_wrapper& a, const pair_wrapper& b)
	{
		return (a.display_count > b.display_count) ||
			(a.display_count == b.display_count &&
				(a.depthstencil_width > b.depthstencil_width ||
					(a.depthstencil_width == b.depthstencil_width && a.depthstencil_height > b.depthstencil_height))) ||
			(a.depthstencil_width == b.depthstencil_width && a.depthstencil_height == b.depthstencil_height &&
				a.wrapped_dsv_texture < b.wrapped_dsv_texture);
	});
	// build a new vector with the sorted elements
	std::vector<std::pair<ID3D11Texture2D*, state_tracking::depthstencil_info>> to_return;
	to_return.reserve(sorted_counters_per_buffer.size());
	_shown_count_per_depthstencil_address.clear();
	for (const auto& [display_count, dsv_texture, snapshot, w, h] : sorted_counters_per_buffer)
	{
		to_return.push_back({ dsv_texture, snapshot });
		_shown_count_per_depthstencil_address[dsv_texture] = display_count;
	}
	return to_return;
}

com_ptr<ID3D11Texture2D> reshade::d3d11::state_tracking_context::find_best_depth_texture(UINT width, UINT height, com_ptr<ID3D11Texture2D> override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D11Texture2D> best_match = std::move(override);
	if (best_match != nullptr)
	{
		best_snapshot = _counters_per_used_depth_texture[best_match];
	}
	else
	{
		for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
		{
			if (snapshot.total_stats.drawcalls == 0)
				continue; // Skip unused

			D3D11_TEXTURE2D_DESC desc;
			dsv_texture->GetDesc(&desc);
			assert((desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			assert((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0);

			if (use_aspect_ratio_heuristics)
			{
				assert(width != 0 && height != 0);
				const float w = static_cast<float>(width);
				const float w_ratio = w / desc.Width;
				const float h = static_cast<float>(height);
				const float h_ratio = h / desc.Height;
				const float aspect_ratio = (w / h) - (static_cast<float>(desc.Width) / desc.Height);

				if (std::fabs(aspect_ratio) > 0.1f || w_ratio > 1.85f || h_ratio > 1.85f || w_ratio < 0.5f || h_ratio < 0.5f)
					continue; // Not a good fit
			}

			if (!_has_indirect_drawcalls ?
				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				snapshot.total_stats.vertices > best_snapshot.total_stats.vertices :
				// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
				snapshot.total_stats.drawcalls > best_snapshot.total_stats.drawcalls)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;
			}
		}
	}

	depthstencil_clear_index.first = best_match.get();

	if (preserve_depth_buffers && best_match != nullptr)
	{
		_previous_stats = best_snapshot.current_stats;

		D3D11_TEXTURE2D_DESC desc;
		best_match->GetDesc(&desc);

		if (update_depthstencil_clear_texture(desc))
		{
			return _depthstencil_clear_texture;
		}
	}

	return best_match;
}
#endif

