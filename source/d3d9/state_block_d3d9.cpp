/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_block_d3d9.hpp"

reshade::d3d9::state_block::state_block(IDirect3DDevice9 *device) :
	_device(device)
{
#ifdef RESHADE_TEST_APPLICATION
	// Avoid errors from the D3D9 debug runtime because the other slots return D3DERR_NOTFOUND with the test application
	_num_simultaneous_rts = 1;
#else
	D3DCAPS9 caps;
	_device->GetDeviceCaps(&caps);
	_num_simultaneous_rts = caps.NumSimultaneousRTs;
	if (_num_simultaneous_rts > ARRAYSIZE(_render_targets))
		_num_simultaneous_rts = ARRAYSIZE(_render_targets);
#endif
}
reshade::d3d9::state_block::~state_block()
{
	release_all_device_objects();
}

void reshade::d3d9::state_block::capture()
{
	// Getting the clip plane only works on devices created without the D3DCREATE_PUREDEVICE flag
	if (SUCCEEDED(_device->GetClipPlane(0, _clip_plane_0)))
	{
		// For some reason state block capturing behaves weirdly if a clip plane is set, so clear it to zero before ...
		const float zero_clip_plane[4] = { 0, 0, 0, 0 };
		_device->SetClipPlane(0, zero_clip_plane);
	}

	_state_block->Capture();

	_device->GetViewport(&_viewport);

	for (DWORD target = 0; target < _num_simultaneous_rts; target++)
		_device->GetRenderTarget(target, &_render_targets[target]);
	_device->GetDepthStencilSurface(&_depth_stencil);
}
void reshade::d3d9::state_block::apply_and_release()
{
	_state_block->Apply();

	// Reset clip plane to the original value again if there was one
	if (_clip_plane_0[0] != 0 || _clip_plane_0[1] != 0 || _clip_plane_0[2] != 0 || _clip_plane_0[3] != 0)
	{
		_device->SetClipPlane(0, _clip_plane_0);
	}

	for (DWORD target = 0; target < _num_simultaneous_rts; target++)
		_device->SetRenderTarget(target, _render_targets[target].get());
	_device->SetDepthStencilSurface(_depth_stencil.get());

	// Set viewport after render targets have been set, since 'SetRenderTarget' causes the viewport to be set to the full size of the render target
	_device->SetViewport(&_viewport);

	release_all_device_objects();
}

bool reshade::d3d9::state_block::init_state_block()
{
	return SUCCEEDED(_device->CreateStateBlock(D3DSBT_ALL, &_state_block));
}
void reshade::d3d9::state_block::release_state_block()
{
	_state_block.reset();
}
void reshade::d3d9::state_block::release_all_device_objects()
{
	_depth_stencil.reset();
	for (auto &render_target : _render_targets)
		render_target.reset();
}
