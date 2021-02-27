/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::d3d10
{
	struct tex_data
	{
		com_ptr<ID3D10Texture2D> texture;
		com_ptr<ID3D10RenderTargetView> rtv[2];
		com_ptr<ID3D10ShaderResourceView> srv[2];
	};

	struct pass_data
	{
		com_ptr<ID3D10BlendState> blend_state;
		com_ptr<ID3D10DepthStencilState> depth_stencil_state;
		com_ptr<ID3D10PixelShader> pixel_shader;
		com_ptr<ID3D10VertexShader> vertex_shader;
		com_ptr<ID3D10RenderTargetView> render_targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
		std::vector<com_ptr<ID3D10ShaderResourceView>> srvs, modified_resources;
	};

	struct effect_data
	{
		com_ptr<ID3D10Buffer> cb;
	};

	struct technique_data
	{
		bool query_in_flight = false;
		com_ptr<ID3D10Query> timestamp_disjoint;
		com_ptr<ID3D10Query> timestamp_query_beg;
		com_ptr<ID3D10Query> timestamp_query_end;
		std::vector<com_ptr<ID3D10SamplerState>> sampler_states;
		std::vector<pass_data> passes;
	};
}
