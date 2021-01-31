/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::d3d9
{
	struct tex_data
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};

	struct pass_data
	{
		com_ptr<IDirect3DStateBlock9> stateblock;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		com_ptr<IDirect3DVertexShader9> vertex_shader;
		IDirect3DSurface9 *render_targets[8] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
	};

	struct technique_data
	{
		DWORD num_samplers = 0;
		DWORD sampler_states[16][12] = {};
		IDirect3DTexture9 *sampler_textures[16] = {};
		DWORD constant_register_count = 0;
		std::vector<pass_data> passes;
	};
}
