/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d9_impl_state_block.hpp"

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

	D3DDEVICE_CREATION_PARAMETERS cp = {};
	device->GetCreationParameters(&cp);
	_vertex_processing = cp.BehaviorFlags & (D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MIXED_VERTEXPROCESSING);
}
reshade::d3d9::state_block::~state_block()
{
}

void reshade::d3d9::state_block::capture()
{
	assert(_state_block == nullptr);

	if (SUCCEEDED(_device->CreateStateBlock(D3DSBT_ALL, &_state_block)))
		_state_block->Capture();
	else
		assert(false);

	_device->GetViewport(&_viewport);

	for (DWORD target = 0; target < _num_simultaneous_rts; target++)
		_device->GetRenderTarget(target, &_render_targets[target]);
	_device->GetDepthStencilSurface(&_depth_stencil);

	_device->GetRenderState(D3DRS_SRGBWRITEENABLE, &_srgb_write);
	_device->GetSamplerState(0, D3DSAMP_SRGBTEXTURE, &_srgb_texture);

	if (0 != (_vertex_processing & D3DCREATE_MIXED_VERTEXPROCESSING))
	{
		_vertex_processing |= _device->GetSoftwareVertexProcessing();
		_device->SetSoftwareVertexProcessing(FALSE); // Disable software vertex processing since it is incompatible with programmable shaders
	}
}
void reshade::d3d9::state_block::apply_and_release()
{
	if (_state_block != nullptr)
		_state_block->Apply();

	// Release state block every time, so that all references to captured vertex and index buffers, textures, etc. are released again
	_state_block.reset();

	if (0 != (_vertex_processing & D3DCREATE_MIXED_VERTEXPROCESSING))
	{
		_device->SetSoftwareVertexProcessing(_vertex_processing & (TRUE | FALSE));
		_vertex_processing &= ~(TRUE | FALSE);
	}

	// This should technically be captured and applied by the state block already ...
	// But Steam overlay messes it up somehow and this is neceesary to fix the screen darkening in Portal when the Steam overlay is showing a notification popup and the ReShade overlay is open at the same time
	_device->SetRenderState(D3DRS_SRGBWRITEENABLE, _srgb_write);
	_device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, _srgb_texture);

	for (DWORD i = 0; i < _num_simultaneous_rts; i++)
		_device->SetRenderTarget(i, _render_targets[i].get());
	_device->SetDepthStencilSurface(_depth_stencil.get());

	// Set viewport after render targets have been set, since 'SetRenderTarget' causes the viewport to be set to the full size of the render target
	_device->SetViewport(&_viewport);

	for (auto &render_target : _render_targets)
		render_target.reset();
	_depth_stencil.reset();
}
