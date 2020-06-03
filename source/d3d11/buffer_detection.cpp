/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "buffer_detection.hpp"
#include "dxgi/format_utils.hpp"
#include <cmath>

#if RESHADE_DEPTH
static inline com_ptr<ID3D11Texture2D> texture_from_dsv(ID3D11DepthStencilView *dsv)
{
	if (dsv == nullptr)
		return nullptr;
	com_ptr<ID3D11Resource> resource;
	dsv->GetResource(&resource);
	com_ptr<ID3D11Texture2D> texture;
	resource->QueryInterface(&texture);
	return texture;
}
#endif

void reshade::d3d11::buffer_detection::init(ID3D11DeviceContext *device, const buffer_detection_context *context)
{
	_device = device;
	_context = context;
}

void reshade::d3d11::buffer_detection::reset()
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_has_indirect_drawcalls = false;
	_depth_buffer_cleared = false;
	_depth_stencil_cleared = false;
	_counters_per_used_depth_texture.clear();
#endif
}
void reshade::d3d11::buffer_detection_context::reset(bool release_resources)
{
	buffer_detection::reset();

#if RESHADE_DEPTH
	assert((_depthstencil_clear_texture == nullptr && _depthstencil_hidden_by_rectangle_texture == nullptr) || _context == this);

	if (release_resources)
	{
		assert(_context == this);

		_preserve_cleared_depth_buffers = false;
		_preserve_depth_buffers_hidden_by_rectangle = false;
		_previous_stats = { 0, 0 };
		_depthstencil_clear_texture.reset();
		_depthstencil_hidden_by_rectangle_texture.reset();
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d11::buffer_detection::merge(const buffer_detection &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DEPTH
	_best_copy_stats = source._best_copy_stats;
	_preserve_cleared_depth_buffers |= source._preserve_cleared_depth_buffers;
	_preserve_depth_buffers_hidden_by_rectangle |= source._preserve_depth_buffers_hidden_by_rectangle;
	_has_indirect_drawcalls |= source._has_indirect_drawcalls;
	_depth_buffer_cleared |= _depth_buffer_cleared;
	_depth_stencil_cleared |= source._depth_stencil_cleared;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.total_stats.vertices += snapshot.total_stats.vertices;
		target_snapshot.total_stats.drawcalls += snapshot.total_stats.drawcalls;
		target_snapshot.current_stats.vertices += snapshot.current_stats.vertices;
		target_snapshot.current_stats.drawcalls += snapshot.current_stats.drawcalls;
		target_snapshot.rect_stats.vertices += snapshot.rect_stats.vertices;
		target_snapshot.rect_stats.drawcalls += snapshot.rect_stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
		target_snapshot.rectangles.insert(target_snapshot.rectangles.end(), snapshot.rectangles.begin(), snapshot.rectangles.end());
	}
#endif
}

void reshade::d3d11::buffer_detection::before_draw(UINT vertices)
{
#if RESHADE_DEPTH
	if (!_preserve_depth_buffers_hidden_by_rectangle)
		return;

	// a rectangle drawing should habe only 6 vertices (two triangles)
	if (vertices != 6)
		return;

	// only check the rectangles that are drawn at the beginning of an Output Merger Stage, if the depthstencil has been previously prepared (it has been cleared)
	if (_new_OM_stage == false || _depth_stencil_cleared == false)
		return;

	// if the depth bufer has previously been cleared, it is not necessary to make a copy of the depth buffer
	if (_depth_buffer_cleared == true)
		return;

	com_ptr<ID3D11DepthStencilView> depthstencil;
	_device->OMGetRenderTargets(0, nullptr, &depthstencil);

	const auto dsv_texture = texture_from_dsv(depthstencil.get());
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_hidden_by_rectangle_index.first)
		return; // This is a draw call with no depth-stencil bound

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	counters.rectangles.push_back(counters.rect_stats);
	_depth_stencil_cleared = false;
	_new_OM_stage = false;

	// Make a backup copy of the depth texture before it is cleared
	if (_context->_depthstencil_hidden_by_rectangle_index.second == std::numeric_limits<UINT>::max() ||
		// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
		counters.rectangles.size() == _context->_depthstencil_hidden_by_rectangle_index.second)
	{
		_device->CopyResource(_context->_depthstencil_hidden_by_rectangle_texture.get(), dsv_texture.get());
	}

	counters.rect_stats = { 0, 0 };
#endif
}

void reshade::d3d11::buffer_detection::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	com_ptr<ID3D11DepthStencilView> depthstencil;
	_device->OMGetRenderTargets(0, nullptr, &depthstencil);

	const auto dsv_texture = texture_from_dsv(depthstencil.get());
	if (dsv_texture == nullptr)
		return; // This is a draw call with no depth-stencil bound

	auto &counters = _counters_per_used_depth_texture[dsv_texture];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices;
	counters.current_stats.drawcalls += 1;
	counters.rect_stats.vertices += vertices;
	counters.rect_stats.drawcalls += 1;

	if (vertices == 0)
		_has_indirect_drawcalls = true;
#endif
}

void reshade::d3d11::buffer_detection::on_OM_set_render_targets(ID3D11DepthStencilView *dsv)
{
	assert(_context != nullptr);

	if (dsv == nullptr)
		return;

	com_ptr<ID3D11Texture2D> dsv_texture = texture_from_dsv(dsv);
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_hidden_by_rectangle_index.first)
		return;

	_new_OM_stage = true;
	_depth_buffer_cleared = false;
}

#if RESHADE_DEPTH
void reshade::d3d11::buffer_detection::on_clear_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv)
{
	assert(_context != nullptr);
	_depth_stencil_cleared = true;

	if ((clear_flags & D3D11_CLEAR_DEPTH) == 0)
		return;

	_depth_buffer_cleared = true;

	com_ptr<ID3D11Texture2D> dsv_texture = texture_from_dsv(dsv);
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_clear_index.first)
		return;

	auto &counters = _counters_per_used_depth_texture[dsv_texture];

	// Update stats with data from previous frame
	if (counters.current_stats.drawcalls == 0)
		counters.current_stats = _context->_previous_stats;

	// Ignore clears when there was no meaningful workload
	if (counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Make a backup copy of the depth texture before it is cleared
	if (_context->_depthstencil_clear_index.second == std::numeric_limits<UINT>::max() ?
		counters.current_stats.vertices > _best_copy_stats.vertices :
	// This is not really correct, since clears may accumulate over multiple command lists, but it's unlikely that the same depth-stencil is used in more than one
	counters.clears.size() == _context->_depthstencil_clear_index.second)
	{
		_best_copy_stats = counters.current_stats;

		_device->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

bool reshade::d3d11::buffer_detection_context::update_depthstencil_clear_texture(D3D11_TEXTURE2D_DESC desc)
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

	assert(_device != nullptr);

	desc.Format = make_dxgi_format_typeless(desc.Format);
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	com_ptr<ID3D11Device> device;
	_device->GetDevice(&device);

	if (HRESULT hr = device->CreateTexture2D(&desc, nullptr, &_depthstencil_clear_texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

bool reshade::d3d11::buffer_detection_context::update_depthstencil_hidden_by_rectangle_texture(D3D11_TEXTURE2D_DESC desc)
{
	if (_depthstencil_hidden_by_rectangle_texture != nullptr)
	{
		D3D11_TEXTURE2D_DESC existing_desc;
		_depthstencil_hidden_by_rectangle_texture->GetDesc(&existing_desc);

		if (desc.Width == existing_desc.Width && desc.Height == existing_desc.Height && desc.Format == existing_desc.Format)
			return true; // Texture already matches dimensions, so can re-use
		else
			_depthstencil_hidden_by_rectangle_texture.reset();
	}

	assert(_device != nullptr);

	desc.Format = make_dxgi_format_typeless(desc.Format);
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	com_ptr<ID3D11Device> device;
	_device->GetDevice(&device);

	if (HRESULT hr = device->CreateTexture2D(&desc, nullptr, &_depthstencil_hidden_by_rectangle_texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth-stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

com_ptr<ID3D11Texture2D> reshade::d3d11::buffer_detection_context::find_best_depth_texture(UINT width, UINT height, com_ptr<ID3D11Texture2D> override, UINT clear_index_override, UINT rectangle_index_override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D11Texture2D> best_match;

	_preserve_cleared_depth_buffers = (clear_index_override > 0);
	_preserve_depth_buffers_hidden_by_rectangle = (rectangle_index_override > 0);

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

			D3D11_TEXTURE2D_DESC desc; dsv_texture->GetDesc(&desc);
			assert((desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			assert((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0);

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

			if (!_has_indirect_drawcalls ?
				// Choose snapshot with the most vertices, since that is likely to contain the main scene
				snapshot.total_stats.vertices > best_snapshot.total_stats.vertices :
			// Or check draw calls, since vertices may not be accurate if application is using indirect draw calls
			snapshot.total_stats.drawcalls > best_snapshot.total_stats.drawcalls)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;

				// fallback => if no rectangle were found, display one instance in the UI
				if (snapshot.rectangles.empty())
					snapshot.rectangles.push_back(snapshot.total_stats);
			}
		}
	}

	if (_preserve_depth_buffers_hidden_by_rectangle && best_match != nullptr)
	{
		_previous_stats = best_snapshot.current_stats;
		_depthstencil_hidden_by_rectangle_index = { best_match.get(), std::numeric_limits<UINT>::max() };

		if (rectangle_index_override <= best_snapshot.rectangles.size())
		{
			_depthstencil_hidden_by_rectangle_index.second = rectangle_index_override;
		}

		D3D11_TEXTURE2D_DESC desc;
		best_match->GetDesc(&desc);

		if (!best_snapshot.rectangles.empty())
		{
			if (update_depthstencil_hidden_by_rectangle_texture(desc))
				return _depthstencil_hidden_by_rectangle_texture;
		}
		// fallback => if no rectangle were found, try to retrieve the last copy of the cleared depth buffer
		else
		{
			_depthstencil_clear_index = { best_match.get(), std::numeric_limits<UINT>::max() };

			if (update_depthstencil_clear_texture(desc))
				return _depthstencil_clear_texture;
		}
	}
	else if (_preserve_cleared_depth_buffers && best_match != nullptr)
	{
		_previous_stats = best_snapshot.current_stats;
		_depthstencil_clear_index = { best_match.get(), std::numeric_limits<UINT>::max() };

		if (clear_index_override <= best_snapshot.clears.size())
		{
			_depthstencil_clear_index.second = clear_index_override;
		}

		D3D11_TEXTURE2D_DESC desc;
		best_match->GetDesc(&desc);

		if (update_depthstencil_clear_texture(desc))
		{
			return _depthstencil_clear_texture;
		}
	}

	_depthstencil_clear_index = { nullptr, std::numeric_limits<UINT>::max() };
	_depthstencil_hidden_by_rectangle_index = { nullptr, std::numeric_limits<UINT>::max() };

	return best_match;
}
#endif
