/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d9.h>
#include "com_ptr.hpp"

namespace reshade::d3d9
{
	class state_block
	{
	public:
		explicit state_block(IDirect3DDevice9 *device);
		~state_block();

		void capture();
		void apply_and_release();

	private:
		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DStateBlock9> _state_block;
		UINT _num_simultaneous_rts;
		D3DVIEWPORT9 _viewport = {};
		com_ptr<IDirect3DSurface9> _render_targets[8];
		com_ptr<IDirect3DSurface9> _depth_stencil;
		DWORD _srgb_write = FALSE;
		DWORD _srgb_texture = FALSE;
		DWORD _vertex_processing = 0;
	};
}
