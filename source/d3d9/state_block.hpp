/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
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

		bool init_state_block();
		void release_state_block();

		void capture();
		void apply_and_release();

	private:
		void release_all_device_objects();

		com_ptr<IDirect3DDevice9> _device;
		com_ptr<IDirect3DStateBlock9> _state_block;
		UINT _num_simultaneous_rendertargets;
		D3DVIEWPORT9 _viewport;
		com_ptr<IDirect3DSurface9> _depth_stencil;
		com_ptr<IDirect3DSurface9> _render_targets[8];
	};
}
