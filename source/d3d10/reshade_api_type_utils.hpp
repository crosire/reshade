/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::d3d10
{
	struct pipeline_impl
	{
		com_ptr<ID3D10VertexShader> vs;
		com_ptr<ID3D10GeometryShader> gs;
		com_ptr<ID3D10PixelShader> ps;

		com_ptr<ID3D10InputLayout> input_layout;
		com_ptr<ID3D10BlendState> blend_state;
		com_ptr<ID3D10RasterizerState> rasterizer_state;
		com_ptr<ID3D10DepthStencilState> depth_stencil_state;

		D3D10_PRIMITIVE_TOPOLOGY topology;
		UINT sample_mask;
		UINT stencil_reference_value;
		FLOAT blend_constant[4];

		void apply(ID3D10Device *ctx) const
		{
			ctx->VSSetShader(vs.get());
			ctx->GSSetShader(gs.get());
			ctx->PSSetShader(ps.get());
			ctx->IASetInputLayout(input_layout.get());
			ctx->IASetPrimitiveTopology(topology);
			ctx->OMSetBlendState(blend_state.get(), blend_constant, sample_mask);
			ctx->RSSetState(rasterizer_state.get());
			ctx->OMSetDepthStencilState(depth_stencil_state.get(), stencil_reference_value);
		}
	};

	struct pipeline_layout_impl
	{
		std::vector<UINT> shader_registers;
	};

	struct query_pool_impl
	{
		std::vector<com_ptr<ID3D10Query>> queries;
	};

	struct descriptor_set_impl
	{
		reshade::api::descriptor_type type;
		std::vector<uint64_t> descriptors;
	};

	struct descriptor_set_layout_impl
	{
		reshade::api::descriptor_range range;
	};

	auto convert_format(api::format format) -> DXGI_FORMAT;
	auto convert_format(DXGI_FORMAT format) -> api::format;

	void convert_sampler_desc(const api::sampler_desc &desc, D3D10_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D10_SAMPLER_DESC &internal_desc);

	void convert_resource_desc(const api::resource_desc &desc, D3D10_BUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE1D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE2D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE3D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_BUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE1D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE2D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE3D_DESC &internal_desc);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D10_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);

	auto convert_blend_op(api::blend_op value) -> D3D10_BLEND_OP;
	auto convert_blend_factor(api::blend_factor value) -> D3D10_BLEND;
	auto convert_fill_mode(api::fill_mode value) -> D3D10_FILL_MODE;
	auto convert_cull_mode(api::cull_mode value) -> D3D10_CULL_MODE;
	auto convert_compare_op(api::compare_op value) -> D3D10_COMPARISON_FUNC;
	auto convert_compare_op(D3D10_COMPARISON_FUNC value) -> api::compare_op;
	auto convert_stencil_op(api::stencil_op value) -> D3D10_STENCIL_OP;
	auto convert_primitive_topology(api::primitive_topology value) -> D3D10_PRIMITIVE_TOPOLOGY;
	auto convert_query_type(api::query_type value) -> D3D10_QUERY;
}
