/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "state_block.hpp"
#include <algorithm>

reshade::d3d9::state_block::state_block(IDirect3DDevice9 *device)
{
	ZeroMemory(this, sizeof(*this));

	_device = device;

	D3DCAPS9 caps;
	device->GetDeviceCaps(&caps);
	_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, DWORD(8));
}
reshade::d3d9::state_block::~state_block()
{
	release_all_device_objects();
}

void reshade::d3d9::state_block::capture()
{
	_state_block->Capture();

	_device->GetViewport(&_viewport);

	for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
		_device->GetRenderTarget(target, &_render_targets[target]);
	_device->GetDepthStencilSurface(&_depth_stencil);
}
void reshade::d3d9::state_block::apply_and_release()
{
	_state_block->Apply();

	for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
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
