/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "buffer_detection.hpp"
#include "../dxgi/format_utils.hpp"
#include <math.h>

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
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
	_stats.vertices = 0;
	_stats.drawcalls = 0;
#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	_clear_stats.vertices = 0;
	_clear_stats.drawcalls = 0;
	_counters_per_used_depth_texture.clear();
#endif
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
	_counters_per_constant_buffer.clear();
#endif
}
void reshade::d3d11::buffer_detection_context::reset(bool release_resources)
{
	buffer_detection::reset();

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	if (release_resources)
	{
		assert(_context == this);

		_depthstencil_clear_texture.reset();
	}
#endif
}

void reshade::d3d11::buffer_detection::merge(const buffer_detection &source)
{
	_stats.vertices += source._stats.vertices;
	_stats.drawcalls += source._stats.drawcalls;

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	_clear_stats.vertices += source._clear_stats.vertices;
	_clear_stats.drawcalls += source._clear_stats.drawcalls;

	for (const auto &[dsv_texture, snapshot] : source._counters_per_used_depth_texture)
	{
		auto &target_snapshot = _counters_per_used_depth_texture[dsv_texture];
		target_snapshot.stats.vertices += snapshot.stats.vertices;
		target_snapshot.stats.drawcalls += snapshot.stats.drawcalls;

		target_snapshot.clears.insert(target_snapshot.clears.end(), snapshot.clears.begin(), snapshot.clears.end());
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

void reshade::d3d11::buffer_detection::on_map(ID3D11Resource *resource)
{
	UNREFERENCED_PARAMETER(resource);

#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
	D3D11_RESOURCE_DIMENSION dim;
	resource->GetType(&dim);

	if (dim == D3D11_RESOURCE_DIMENSION_BUFFER)
		_counters_per_constant_buffer[static_cast<ID3D11Buffer *>(resource)].mapped += 1;
#endif
}

void reshade::d3d11::buffer_detection::on_draw(UINT vertices)
{
	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
	com_ptr<ID3D11RenderTargetView> targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	com_ptr<ID3D11DepthStencilView> depthstencil;
	_device->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, reinterpret_cast<ID3D11RenderTargetView **>(targets), &depthstencil);

	const auto dsv_texture = texture_from_dsv(depthstencil.get());
	if (dsv_texture == nullptr)
		return; // This is a draw call with no depth stencil bound

	_clear_stats.vertices += vertices;
	_clear_stats.drawcalls += 1;

	if (const auto intermediate_snapshot = _counters_per_used_depth_texture.find(dsv_texture);
		intermediate_snapshot != _counters_per_used_depth_texture.end())
	{
		intermediate_snapshot->second.stats.vertices += vertices;
		intermediate_snapshot->second.stats.drawcalls += 1;

		// Find the render targets, if they exist, and update their counts
		for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
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
#if RESHADE_DX11_CAPTURE_CONSTANT_BUFFERS
	// Capture constant buffers that are used when depth stencils are drawn
	com_ptr<ID3D11Buffer> vscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	_device->VSGetConstantBuffers(0, ARRAYSIZE(vscbuffers), reinterpret_cast<ID3D11Buffer **>(vscbuffers));

	for (UINT i = 0; i < ARRAYSIZE(vscbuffers); i++)
		// Uses the default drawcalls = 0 the first time around.
		if (vscbuffers[i] != nullptr)
			_counters_per_constant_buffer[vscbuffers[i]].vs_uses += 1;

	com_ptr<ID3D11Buffer> pscbuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	_device->PSGetConstantBuffers(0, ARRAYSIZE(pscbuffers), reinterpret_cast<ID3D11Buffer **>(pscbuffers));

	for (UINT i = 0; i < ARRAYSIZE(pscbuffers); i++)
		// Uses the default drawcalls = 0 the first time around.
		if (pscbuffers[i] != nullptr)
			_counters_per_constant_buffer[pscbuffers[i]].ps_uses += 1;
#endif
}

#if RESHADE_DX11_CAPTURE_DEPTH_BUFFERS
void reshade::d3d11::buffer_detection::track_render_targets(UINT num_views, ID3D11RenderTargetView *const *views, ID3D11DepthStencilView *dsv)
{
	com_ptr<ID3D11Texture2D> dsv_texture = texture_from_dsv(dsv);
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
void reshade::d3d11::buffer_detection::track_cleared_depthstencil(UINT clear_flags, ID3D11DepthStencilView *dsv)
{
	assert(_context != nullptr);

	// Reset draw call stats for clears
	auto current_stats = _clear_stats;
	_clear_stats.vertices = 0;
	_clear_stats.drawcalls = 0;

	if ((clear_flags & D3D11_CLEAR_DEPTH) == 0)
		return;

	com_ptr<ID3D11Texture2D> dsv_texture = texture_from_dsv(dsv);
	if (dsv_texture == nullptr || dsv_texture != _context->_depthstencil_clear_index.first)
		return;

	auto &clears = _counters_per_used_depth_texture[dsv_texture].clears;
	clears.push_back(current_stats);

	// Make a backup copy of the depth texture before it is cleared
	// TODO: This is not correct, since clears may accumulate over multiple command lists
	if (clears.size() == _context->_depthstencil_clear_index.second)
	{
		_device->CopyResource(_context->_depthstencil_clear_texture.get(), dsv_texture.get());
	}
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

	desc.Format = make_dxgi_format_typeless(desc.Format);
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	com_ptr<ID3D11Device> device;
	_device->GetDevice(&device);

	if (HRESULT hr = device->CreateTexture2D(&desc, nullptr, &_depthstencil_clear_texture); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth stencil texture! HRESULT is " << hr << '.';
		return false;
	}

	return true;
}

com_ptr<ID3D11Texture2D> reshade::d3d11::buffer_detection_context::find_best_depth_texture(UINT width, UINT height, com_ptr<ID3D11Texture2D> override, UINT clear_index_override)
{
	depthstencil_info best_snapshot;
	com_ptr<ID3D11Texture2D> best_match;

	if (override != nullptr)
	{
		best_match = std::move(override);
		best_snapshot = _counters_per_used_depth_texture[best_match];
	}
	else
	{
		for (auto &[dsv_texture, snapshot] : _counters_per_used_depth_texture)
		{
			if (snapshot.stats.drawcalls == 0 || snapshot.stats.vertices == 0)
				continue; // Skip unused

			D3D11_TEXTURE2D_DESC desc;
			dsv_texture->GetDesc(&desc);
			assert((desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);

			if (desc.SampleDesc.Count > 1)
				continue; // Ignore MSAA textures, since they would need to be resolved first

			if (width != 0 && height != 0)
			{
				const float aspect_ratio = float(width) / float(height);
				const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

				const float width_factor = float(width) / float(desc.Width);
				const float height_factor = float(height) / float(desc.Height);

				if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
					continue; // Not a good fit
			}

			// Choose snapshot with the most draw calls, since vertices may not be accurate if application is using indirect draw calls
			if (snapshot.stats.drawcalls >= best_snapshot.stats.drawcalls)
			{
				best_match = dsv_texture;
				best_snapshot = snapshot;
			}
		}
	}

	if (clear_index_override != 0 && best_match != nullptr)
	{
		_depthstencil_clear_index = { best_match.get(), std::numeric_limits<UINT>::max() };

		if (clear_index_override <= best_snapshot.clears.size())
		{
			_depthstencil_clear_index.second = clear_index_override;
		}
		else
		{
			UINT last_vertices = 0;

			for (UINT clear_index = 0; clear_index < best_snapshot.clears.size(); ++clear_index)
			{
				const auto &snapshot = best_snapshot.clears[clear_index];

				if (snapshot.vertices >= last_vertices)
				{
					last_vertices = snapshot.vertices;
					_depthstencil_clear_index.second = clear_index + 1;
				}
			}
		}

		D3D11_TEXTURE2D_DESC desc;
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
