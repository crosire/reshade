/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "state_tracking.hpp"
#include <cmath>

static constexpr auto D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
static constexpr auto D3DFMT_DF16 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '1', '6'));
static constexpr auto D3DFMT_DF24 = static_cast<D3DFORMAT>(MAKEFOURCC('D', 'F', '2', '4'));

void reshade::d3d9::state_tracking::reset(bool release_resources)
{
	_stats = { 0, 0 };
#if RESHADE_DEPTH
	_counters_per_used_depth_surface.clear();

	if (release_resources)
	{
		for (size_t i = 0; i < _depthstencil_replacement.size(); ++i)
			update_depthstencil_replacement(nullptr, i);
		_depthstencil_original.reset(); // Reset this after all replacements have been released, so that 'update_depthstencil_replacement' was able to bind it if necessary
		_depthstencil_replacement.clear();
	}
	else if (preserve_depth_buffers && !_depthstencil_replacement.empty() && _depthstencil_replacement[0] != nullptr)
	{
		com_ptr<IDirect3DSurface9> depthstencil;
		_device->GetDepthStencilSurface(&depthstencil);

		// Clear the first replacement at the end of the frame, since any clear performed by the application was redirected to a different one
		// Do not have to do this to the others, since the first operation on any of them is a clear anyway (see 'on_clear_depthstencil')
		_device->SetDepthStencilSurface(_depthstencil_replacement[0].get());
		_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.0f, 0);

		// Keep the depth-stencil surface set to the first replacement (because of the above 'SetDepthStencilSurface' call) if the original one we want to replace was set, so starting next frame it is the one used again
		if (std::find(_depthstencil_replacement.begin(), _depthstencil_replacement.end(), depthstencil) == _depthstencil_replacement.end() && depthstencil != _depthstencil_original)
			_device->SetDepthStencilSurface(depthstencil.get());
	}
#else
	UNREFERENCED_PARAMETER(release_resources);
#endif
}

void reshade::d3d9::state_tracking::on_draw(D3DPRIMITIVETYPE type, UINT vertices)
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
	if (std::find(_depthstencil_replacement.begin(), _depthstencil_replacement.end(), depthstencil) != _depthstencil_replacement.end())
		depthstencil = _depthstencil_original;

	// Update draw statistics for tracked depth-stencil surfaces
	auto &counters = _counters_per_used_depth_surface[depthstencil];
	counters.total_stats.vertices += vertices;
	counters.total_stats.drawcalls += 1;

	if (preserve_depth_buffers)
	{
		counters.current_stats.vertices += vertices;
		counters.current_stats.drawcalls += 1;
		_device->GetViewport(&counters.current_stats.viewport);
	}
#endif
}

#if RESHADE_DEPTH
void reshade::d3d9::state_tracking::on_set_depthstencil(IDirect3DSurface9 *&depthstencil)
{
	if (depthstencil == nullptr || depthstencil != _depthstencil_original)
		return;

	const size_t replacement_index = preserve_depth_buffers ?
		_counters_per_used_depth_surface[_depthstencil_original].clears.size() : 0;

	// Replace application depth-stencil surface with our custom one
	if (_depthstencil_replacement[replacement_index] != nullptr)
		depthstencil = _depthstencil_replacement[replacement_index].get();
}
void reshade::d3d9::state_tracking::on_get_depthstencil(IDirect3DSurface9 *&depthstencil)
{
	if (std::find(_depthstencil_replacement.begin(), _depthstencil_replacement.end(), depthstencil) == _depthstencil_replacement.end())
		return;

	// The call to IDirect3DDevice9::GetDepthStencilSurface increased the reference count, so release that before replacing
	depthstencil->Release();

	// Return original application depth-stencil surface
	depthstencil = _depthstencil_original.get();
	_depthstencil_original->AddRef();
}

void reshade::d3d9::state_tracking::on_clear_depthstencil(UINT clear_flags)
{
	if ((clear_flags & D3DCLEAR_ZBUFFER) == 0 || !preserve_depth_buffers)
		return; // Ignore clears that do not affect the depth buffer (e.g. color or stencil clears)

	com_ptr<IDirect3DSurface9> depthstencil;
	_device->GetDepthStencilSurface(&depthstencil);
	assert(depthstencil != nullptr);

	if (std::find(_depthstencil_replacement.begin(), _depthstencil_replacement.end(), depthstencil) == _depthstencil_replacement.end() && depthstencil != _depthstencil_original)
		return; // Can only avoid clear of the replacement surface

	auto &counters = _counters_per_used_depth_surface[_depthstencil_original];

	// Ignore clears when there was no meaningful workload
	// Also triggers when '_preserve_depth_buffers' is false, since no clear stats are recorded then
	if (counters.current_stats.vertices <= 4 || counters.current_stats.drawcalls == 0)
		return;

	counters.clears.push_back(counters.current_stats);

	// Reset draw call stats for clears
	counters.current_stats = { 0, 0 };

	// Create a new replacement surface if necessary
	const size_t replacement_index = counters.clears.size();
	if (update_depthstencil_replacement(std::move(_depthstencil_original), replacement_index))
	{
		// Bind it immediately so the clear is not performed on the previous one, but the new one instead
		_device->SetDepthStencilSurface(_depthstencil_replacement[replacement_index].get());
	}
}

bool reshade::d3d9::state_tracking::update_depthstencil_replacement(com_ptr<IDirect3DSurface9> depthstencil, size_t index)
{
	if (index >= _depthstencil_replacement.size())
		_depthstencil_replacement.resize(index + 1);

	com_ptr<IDirect3DSurface9> current_replacement =
		std::move(_depthstencil_replacement[index]);
	assert(_depthstencil_replacement[index] == nullptr);

	// First unbind the depth-stencil replacement from the device, since it may be destroyed below
	com_ptr<IDirect3DSurface9> current_depthstencil;
	_device->GetDepthStencilSurface(&current_depthstencil);
	if (current_depthstencil != nullptr && current_depthstencil == current_replacement)
		_device->SetDepthStencilSurface(_depthstencil_original.get());

	if (depthstencil == nullptr)
		return true; // Abort early if this update just destroyed the existing replacement

	_depthstencil_original.reset();

	D3DSURFACE_DESC desc;
	depthstencil->GetDesc(&desc);

	if (check_texture_format(desc) && (!preserve_depth_buffers || index == 0))
	{
		// Format already supports shader access, so no need to replace
		// Also never replace the original buffer when preserving, since game (e.g. Dead Space) may use it as a texture binding later, so have to fill it
		if (preserve_depth_buffers)
			_depthstencil_original = std::move(depthstencil);
		return true;
	}

	if (disable_intz)
		// Disable replacement with a texture of the INTZ format (which can have lower precision)
		return false;

	if (current_replacement != nullptr)
	{
		D3DSURFACE_DESC replacement_desc;
		current_replacement->GetDesc(&replacement_desc);

		if (desc.Width == replacement_desc.Width && desc.Height == replacement_desc.Height)
		{
			_depthstencil_original = std::move(depthstencil);
			// Replacement already matches dimensions, so can re-use
			_depthstencil_replacement[index] = std::move(current_replacement);
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
		LOG(ERROR) << "Failed to create depth replacement texture! HRESULT is " << hr << '.';
		return false;
	}

	// The surface holds a reference to the texture, so it is safe to let that go out of scope
	texture->GetSurfaceLevel(0, &_depthstencil_replacement[index]);

	// Update current depth-stencil in case it matches the one we want to replace
	_depthstencil_original = std::move(depthstencil);
	if (current_depthstencil == _depthstencil_original)
		_device->SetDepthStencilSurface(_depthstencil_replacement[index].get());

	return true;
}

bool reshade::d3d9::state_tracking::check_aspect_ratio(UINT width_to_check, UINT height_to_check, UINT width, UINT height)
{
	assert(width != 0 && height != 0);
	return (width_to_check >= std::floor(width * 0.95f) && width_to_check <= std::ceil(width * 1.05f))
		&& (height_to_check >= std::floor(height * 0.95f) && height_to_check <= std::ceil(height * 1.05f));
}
bool reshade::d3d9::state_tracking::check_texture_format(const D3DSURFACE_DESC &desc)
{
	// Binding a depth-stencil surface as a texture to a shader is only supported on the following custom formats:
	return desc.Format == D3DFMT_INTZ || desc.Format == D3DFMT_DF16 || desc.Format == D3DFMT_DF24;
}

com_ptr<IDirect3DSurface9> reshade::d3d9::state_tracking::find_best_depth_surface(UINT width, UINT height, com_ptr<IDirect3DSurface9> override)
{
	bool no_replacement = true;
	depthstencil_info best_snapshot;
	com_ptr<IDirect3DSurface9> best_match = std::move(override);
	com_ptr<IDirect3DSurface9> best_replacement = _depthstencil_replacement.empty() ? nullptr : _depthstencil_replacement[0];

	if (best_match != nullptr)
	{
		best_snapshot = _counters_per_used_depth_surface[best_match];

		D3DSURFACE_DESC desc;
		best_match->GetDesc(&desc);
		no_replacement = check_texture_format(desc);
	}
	else
	{
		for (const auto &[surface, snapshot] : _counters_per_used_depth_surface)
		{
			if (snapshot.total_stats.drawcalls == 0 || snapshot.total_stats.vertices == 0)
				continue; // Skip unused

			D3DSURFACE_DESC desc;
			surface->GetDesc(&desc);
			assert((desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0);

			if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
				continue; // MSAA depth buffers are not supported since they would have to be moved into a plain surface before attaching to a shader slot

			if (use_aspect_ratio_heuristics && !check_aspect_ratio(desc.Width, desc.Height, width, height))
				continue; // Not a good fit

			const auto curr_weight = snapshot.total_stats.vertices * (1.2f - static_cast<float>(snapshot.total_stats.drawcalls) / _stats.drawcalls);
			const auto best_weight = best_snapshot.total_stats.vertices * (1.2f - static_cast<float>(best_snapshot.total_stats.drawcalls) / _stats.vertices);
			if (curr_weight >= best_weight)
			{
				best_match = surface;
				best_snapshot = snapshot;

				// Do not need to replace if format already supports shader access
				no_replacement = check_texture_format(desc);
			}
		}
	}

	if (preserve_depth_buffers && best_match != nullptr)
	{
		if (!_depthstencil_replacement.empty())
		{
			if (depthstencil_clear_index != 0 && depthstencil_clear_index <= best_snapshot.clears.size())
			{
				const UINT clear_index = depthstencil_clear_index - 1;
				best_replacement = _depthstencil_replacement[clear_index];
			}
			else
			{
				// Default to the last replacement (which was filled after the last clear call of the frame)
				best_replacement = _depthstencil_replacement[best_snapshot.clears.size()];

				UINT last_vertices = 0;

				for (UINT clear_index = 0; clear_index < best_snapshot.clears.size(); clear_index++)
				{
					const auto &snapshot = best_snapshot.clears[clear_index];

					// Skip render passes that only drew to a subset of the depth-stencil surface
					if (width != 0 && height != 0 && snapshot.viewport.Width != 0 && snapshot.viewport.Height != 0 &&
						!check_aspect_ratio(snapshot.viewport.Width, snapshot.viewport.Height, width, height))
						continue;

					// Fix for Source Engine games: Add a weight in order not to select the first db instance if it is related to the background scene
					int mult = (clear_index > 0) ? 10 : 1;
					if (mult * snapshot.vertices >= last_vertices)
					{
						last_vertices = mult * snapshot.vertices;
						best_replacement = _depthstencil_replacement[clear_index];
					}
				}
			}
		}

		// Always need to replace if preserving on clears, unless this is the first clear and game is using INTZ itself already
		no_replacement = !_depthstencil_replacement.empty() && best_replacement == nullptr;
	}

	assert(best_match == nullptr || best_match != best_replacement);

	if (no_replacement) // This also happens when 'best_match' is a nullptr
		return best_match;
	if (best_match == _depthstencil_original)
		return best_replacement;

	// Depth buffers that do not use a shader-readable format need a replacement
	if (update_depthstencil_replacement(std::move(best_match), 0))
	{
		// Replacement takes effect starting with the next frame
		return _depthstencil_replacement[0];
	}

	return nullptr;
}
#endif
