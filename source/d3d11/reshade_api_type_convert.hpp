/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::d3d11
{
	struct pipeline_impl
	{
		com_ptr<ID3D11VertexShader> vs;
		com_ptr<ID3D11HullShader> hs;
		com_ptr<ID3D11DomainShader> ds;
		com_ptr<ID3D11GeometryShader> gs;
		com_ptr<ID3D11PixelShader> ps;

		com_ptr<ID3D11InputLayout> input_layout;
		com_ptr<ID3D11BlendState> blend_state;
		com_ptr<ID3D11RasterizerState> rasterizer_state;
		com_ptr<ID3D11DepthStencilState> depth_stencil_state;

		D3D11_PRIMITIVE_TOPOLOGY topology;
		UINT sample_mask;
		UINT stencil_reference_value;
		FLOAT blend_constant[4];

		void apply(ID3D11DeviceContext *ctx) const;
	};

	struct pipeline_layout_impl
	{
		std::vector<UINT> shader_registers;
	};

	struct descriptor_set_layout_impl
	{
		api::descriptor_range range;
	};

	struct query_pool_impl
	{
		std::vector<com_ptr<ID3D11Query>> queries;
	};

	struct framebuffer_impl
	{
		UINT count;
		ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11DepthStencilView *dsv;
	};

	struct descriptor_set_impl
	{
		api::descriptor_type type;
		std::vector<uint64_t> descriptors;
	};

	auto convert_format(api::format format) -> DXGI_FORMAT;
	auto convert_format(DXGI_FORMAT format) -> api::format;

	void convert_sampler_desc(const api::sampler_desc &desc, D3D11_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D11_SAMPLER_DESC &internal_desc);

	void convert_resource_desc(const api::resource_desc &desc, D3D11_BUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE1D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE2D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D11_TEXTURE3D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D11_BUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D11_TEXTURE1D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D11_TEXTURE2D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D11_TEXTURE3D_DESC &internal_desc);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_RENDER_TARGET_VIEW_DESC1 &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 &internal_desc);

	auto convert_logic_op(api::logic_op value) -> D3D11_LOGIC_OP;
	auto convert_blend_op(api::blend_op value) -> D3D11_BLEND_OP;
	auto convert_blend_factor(api::blend_factor value) -> D3D11_BLEND;
	auto convert_fill_mode(api::fill_mode value) -> D3D11_FILL_MODE;
	auto convert_cull_mode(api::cull_mode value) -> D3D11_CULL_MODE;
	auto convert_compare_op(api::compare_op value) -> D3D11_COMPARISON_FUNC;
	auto convert_compare_op(D3D11_COMPARISON_FUNC value) ->api::compare_op;
	auto convert_stencil_op(api::stencil_op value) -> D3D11_STENCIL_OP;
	auto convert_primitive_topology(api::primitive_topology value) -> D3D11_PRIMITIVE_TOPOLOGY;
	auto convert_query_type(api::query_type value) -> D3D11_QUERY;
}
