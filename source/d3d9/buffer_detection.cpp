/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "buffer_detection.hpp"
#include <cmath>

static constexpr auto D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
static constexpr auto D3DFMT_DF16 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '1', '6'));
static constexpr auto D3DFMT_DF24 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '2', '4'));

void reshade::d3d9::buffer_detection::reset(bool release_resources)
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_best_copy_stats = { 0, 0 };
	_first_empty_stats = true;
	_depth_stencil_cleared = false;
	_counters_per_used_depth_surface.clear();

	if (release_resources)
	{
		_previous_stats = { 0, 0 };
		update_depthstencil_replacement(nullptr);
		_depthstencil_replacement.reset();
		_depthstencil_original.reset();
		depthstencil_clear_index.first.release();
	}
	else if (preserve_depth_buffers && _depthstencil_replacement != nullptr)
	{
		com_ptr<IDirect3DSurface9> depthstencil;
		_device->GetDepthStencilSurface(&depthstencil);

		// Clear the replacement at the end of the frame, since the clear performed by the application was only applied to the original one
		_device->SetDepthStencilSurface(_depthstencil_replacement.get());
		_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

		_device->SetDepthStencilSurface(depthstencil.get());
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d9::buffer_detection::on_draw(D3DPRIMITIVETYPE type, UINT vertices)
{
	switch (type)
	{
		case D3DPT_LINELIST:
			vertices *= 2;
			break;
		case D3DPT_LINESTRIP:
			vertices += 1;
			break;
		case D3DPT_TRIANGLELIST:
			vertices *= 3;
			break;
		case D3DPT_TRIANGLESTRIP:
		case D3DPT_TRIANGLEFAN:
			vertices += 2;
			break;
	}

	_stats.vertices += vertices;
	_stats.drawcalls += 1;

#if RESHADE_DEPTH
	com_ptr<IDirect3DSurface9> depthstencil;
	_device->GetDepthStencilSurface(&depthstencil);

	if (depthstencil == nullptr)
		return; // This is a draw call with no depth-stencil bound

	depthstencil = (depthstencil == _depthstencil_replacement) ? _depthstencil_original : depthstencil;

	// Update draw statistics for tracked depth-stencil surfaces
	auto &counters = _counters_per_used_depth_surface[depthstencil];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;
	counters.current_stats.vertices += vertices;
	counters.current_stats.drawcalls += 1;
#endif

#if RESHADE_WIREFRAME
	if (_wireframe_mode == true)
		_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	else
		_device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif
}

#if RESHADE_DEPTH
void reshade::d3d9::buffer_detection::on_set_depthstencil(IDirect3DSurface9 *&depthstencil)
{
	if (_depthstencil_replacement == nullptr)
		return;

	if (depthstencil != _depthstencil_original)
		return;

	if (// Replace surface before targeted clear, so that all draw calls until this clear are bounded to the surface used by Reshade
		_counters_per_used_depth_surface[_depthstencil_original].clears.size() < internal_clear_index)
	{
		// Replace application depth-stencil surface with our custom one
		depthstencil = _depthstencil_replacement.get();
	}
}
void reshade::d3d9::buffer_detection::on_get_depthstencil(IDirect3DSurface9 *&depthstencil)
{
	if (_depthstencil_replacement == nullptr)
		return;

	if (depthstencil != _depthstencil_replacement)
		return;// The call to IDirect3DDevice9::GetDepthStencilSurface increased the reference count, so release that before replacing

	depthstencil->Release();
	// Return the original application depth-stencil surface in case the game engine needs it
	depthstencil = _depthstencil_original.get();
	_depthstencil_original->AddRef();
}

void reshade::d3d9::buffer_detection::on_clear_depthstencil(UINT clear_flags)
{
	_depth_stencil_cleared = true;

	if ((clear_flags & D3DCLEAR_ZBUFFER) == 0 || !preserve_depth_buffers)
		return;

	com_ptr<IDirect3DSurface9> depthstencil;
	_device->GetDepthStencilSurface(&depthstencil);

	depthstencil = (depthstencil == _depthstencil_replacement) ? _depthstencil_original : depthstencil;

	if (depthstencil == nullptr || _depthstencil_replacement == nullptr || depthstencil != depthstencil_clear_index.first)
		return;

	// always bound the stats to the original depthstencil surface
	auto &counters = _counters_per_used_depth_surface[depthstencil];

	// Update stats with data from previous frame
	if (counters.current_stats.drawcalls == 0 && _first_empty_stats)
	{
		counters.current_stats = _previous_stats;
		_first_empty_stats = false;
	}

	if (counters.current_stats.vertices <= 4 || counters.current_stats.drawcalls == 0) // Also triggers when '_preserve_depth_buffers' is false, since no clear stats are recorded then
		return; // Ignore clears when there was no meaningful workload since the last one

	if (depthstencil != _depthstencil_original && depthstencil != _depthstencil_replacement)
		return; // Can only avoid clear of the replacement surface

	counters.clears.push_back(counters.current_stats);

	if (internal_clear_index == counters.clears.size())
	{
		_best_copy_stats = counters.current_stats;

		// Bind the original surface again so the clear is not performed on the replacement
		_device->SetDepthStencilSurface(_depthstencil_original.get());
	}

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };
}

void reshade::d3d9::buffer_detection::after_clear_depthstencil()
{
	if (_depthstencil_replacement == nullptr)
		return;

	com_ptr<IDirect3DSurface9> depthstencil;
	_device->GetDepthStencilSurface(&depthstencil);

	if (depthstencil != _depthstencil_original)
		return;

	if (// Replace surface before targeted clear, so that all draw calls until this clear are bounded to the surface used by Reshade
		_counters_per_used_depth_surface[_depthstencil_original].clears.size() < internal_clear_index)
	{
		// Replace application depth-stencil surface with our custom one
		_device->SetDepthStencilSurface(_depthstencil_replacement.get());
	}
}

bool reshade::d3d9::buffer_detection::update_depthstencil_replacement(com_ptr<IDirect3DSurface9> depthstencil)
{
	com_ptr<IDirect3DSurface9> current_replacement =
		std::move(_depthstencil_replacement);
	assert(_depthstencil_replacement == nullptr);

	// First unbind the depth-stencil replacement from the device, since it may be destroyed below
	com_ptr<IDirect3DSurface9> current_depthstencil;
	_device->GetDepthStencilSurface(&current_depthstencil);
	if (current_depthstencil == current_replacement)
		_device->SetDepthStencilSurface(_depthstencil_original.get());

	_depthstencil_original.reset();

	if (depthstencil == nullptr)
		return true; // Abort early if this update just destroyed the existing replacement

	D3DSURFACE_DESC desc;
	depthstencil->GetDesc(&desc);

	if (check_texture_format(desc) && !preserve_depth_buffers)
		return true; // Format already support shader access, so no need to replace
	else if (disable_intz)
		// Disable replacement with a texture of the INTZ format (which can have lower precision)
		return false;

	_depthstencil_original = std::move(depthstencil);

	if (current_replacement != nullptr)
	{
		D3DSURFACE_DESC replacement_desc;
		current_replacement->GetDesc(&replacement_desc);

		if (desc.Width == replacement_desc.Width && desc.Height == replacement_desc.Height)
		{
			// Replacement already matches dimensions, so can re-use
			_depthstencil_replacement = std::move(current_replacement);
			return true;
		}
		else
		{
			current_replacement.reset();
		}
	}

	// Check hardware support for INTZ (or alternative) format
	com_ptr<IDirect3D9> d3d;
	_device->GetDirect3D(&d3d);
	D3DDEVICE_CREATION_PARAMETERS cp;
	_device->GetCreationParameters(&cp);

	desc.Format = D3DFMT_UNKNOWN;
	const D3DFORMAT formats[] = { D3DFMT_INTZ, D3DFMT_DF24, D3DFMT_DF16 };
	for (const auto format : formats)
	{
		if (SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format)))
		{
			desc.Format = format;
			break;
		}
	}

	if (desc.Format == D3DFMT_UNKNOWN)
	{
		LOG(ERROR) << "Your graphics card is missing support for at least one of the 'INTZ', 'DF24' or 'DF16' texture formats. Cannot create depth replacement texture.";
		return false;
	}

	com_ptr<IDirect3DTexture9> texture;

	if (HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, desc.Format, D3DPOOL_DEFAULT, &texture, nullptr); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create depth texture replacement! HRESULT is " << hr << '.';
		return false;
	}

	// The surface holds a reference to the texture, so it is safe to let that go out of scope
	texture->GetSurfaceLevel(0, &_depthstencil_replacement);

	// Update current depth-stencil in case it matches the one we want to replace
	if (current_depthstencil == _depthstencil_original)
		_device->SetDepthStencilSurface(_depthstencil_replacement.get());

	return true;
}

bool reshade::d3d9::buffer_detection::check_texture_format(const D3DSURFACE_DESC &desc)
{
	// Binding a depth-stencil surface as a texture to a shader is only supported on the following custom formats:
	return desc.Format == D3DFMT_INTZ || desc.Format == D3DFMT_DF16 || desc.Format == D3DFMT_DF24;
}

com_ptr<IDirect3DSurface9> reshade::d3d9::buffer_detection::find_best_depth_surface(UINT width, UINT height, com_ptr<IDirect3DSurface9> override)
{
	bool no_replacement = true;
	depthstencil_info best_snapshot;
	com_ptr<IDirect3DSurface9> best_match = std::move(override);

	if (best_match != nullptr)
	{
		best_snapshot = _counters_per_used_depth_surface[best_match];

		// Always replace when there is an override surface
		no_replacement = false;
	}
	else
	{
		for (const auto &[surface, snapshot] : _counters_per_used_depth_surface)
		{
			if (snapshot.total_stats.drawcalls == 0)
				continue; // Skip unused

			D3DSURFACE_DESC desc;
			surface->GetDesc(&desc);
			assert((desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0);

			if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
				continue; // MSAA depth buffers are not supported since they would have to be moved into a plain surface before attaching to a shader slot

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
				best_match = surface;
				best_snapshot = snapshot;

				// Do not need to replace if format already support shader access
				no_replacement = check_texture_format(desc);
			}
		}
	}

	depthstencil_clear_index.first = best_match.get();

	if (preserve_depth_buffers && best_match != nullptr)
	{
		_previous_stats = best_snapshot.current_stats;

		// Always need to replace if preserving on clears
		no_replacement = false;

		if (depthstencil_clear_index.second != 0)
		{
			internal_clear_index = depthstencil_clear_index.second;
		}
		else
		{
			internal_clear_index = std::numeric_limits<UINT>::max();
			UINT last_vertices = 0;

			for (UINT clear_index = 0; clear_index < best_snapshot.clears.size(); clear_index++)
			{
				const auto &snapshot = best_snapshot.clears[clear_index];

				// Fix for source engine games: Add a weight in order not to select the first db instance if it is related to the background scene
				int mult = (clear_index > 0) ? 10 : 1;
				if (mult * snapshot.vertices >= last_vertices)
				{
					last_vertices = mult * snapshot.vertices;
					internal_clear_index = clear_index + 1;
				}
			}
		}
	}

	assert(best_match == nullptr || best_match != _depthstencil_replacement);

	if (no_replacement)
		return best_match;
	if (best_match == _depthstencil_original)
		return _depthstencil_replacement;

	// Depth buffers that do not use a shader-readable format need a replacement
	update_depthstencil_replacement(std::move(best_match));

	return _depthstencil_replacement; // Replacement takes effect starting with the next frame
}
#endif

#if RESHADE_WIREFRAME

void reshade::d3d9::buffer_detection::set_wireframe_mode(bool value)
{
	_wireframe_mode = value;
}

const bool reshade::d3d9::buffer_detection::get_wireframe_mode()
{
	return _wireframe_mode;
}

#endif
