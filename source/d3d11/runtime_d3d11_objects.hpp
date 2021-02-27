/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::d3d11
{
	struct tex_data
	{
		com_ptr<ID3D11Texture2D> texture;
		com_ptr<ID3D11RenderTargetView> rtv[2];
		com_ptr<ID3D11ShaderResourceView> srv[2];
		com_ptr<ID3D11UnorderedAccessView> uav;
	};

	struct pass_data
	{
		com_ptr<ID3D11BlendState> blend_state;
		com_ptr<ID3D11DepthStencilState> depth_stencil_state;
		com_ptr<ID3D11PixelShader> pixel_shader;
		com_ptr<ID3D11VertexShader> vertex_shader;
		com_ptr<ID3D11ComputeShader> compute_shader;
		com_ptr<ID3D11RenderTargetView> render_targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		std::vector<com_ptr<ID3D11ShaderResourceView>> srvs, modified_resources;
		std::vector<com_ptr<ID3D11UnorderedAccessView>> uavs;
	};

	struct effect_data
	{
		com_ptr<ID3D11Buffer> cb;
	};

	struct technique_data
	{
		bool query_in_flight = false;
		com_ptr<ID3D11Query> timestamp_disjoint;
		com_ptr<ID3D11Query> timestamp_query_beg;
		com_ptr<ID3D11Query> timestamp_query_end;
		std::vector<com_ptr<ID3D11SamplerState>> sampler_states;
		std::vector<pass_data> passes;
	};
}
