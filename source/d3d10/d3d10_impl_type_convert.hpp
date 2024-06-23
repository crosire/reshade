/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d10_1.h>
#include "com_ptr.hpp"
#include "reshade_api_pipeline.hpp"
#include <vector>
#include <limits>

namespace reshade::d3d10
{
	static_assert(sizeof(D3D10_BOX) == sizeof(api::subresource_box));
	static_assert(sizeof(D3D10_RECT) == sizeof(api::rect));
	static_assert(sizeof(D3D10_SUBRESOURCE_DATA) == sizeof(api::subresource_data));

	struct pipeline_impl
	{
		void apply(ID3D10Device *ctx, api::pipeline_stage stages) const;

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
	};

	struct descriptor_table_impl
	{
		api::descriptor_type type;
		uint32_t count;
		uint32_t base_binding;
		std::vector<uint64_t> descriptors;
	};

	struct pipeline_layout_impl
	{
		std::vector<api::descriptor_range> ranges;
	};

	struct query_heap_impl
	{
		std::vector<com_ptr<ID3D10Query>> queries;
	};

	struct fence_impl
	{
		uint64_t current_value;
		com_ptr<ID3D10Query> event_queries[8];
	};

	auto convert_format(api::format format) -> DXGI_FORMAT;
	auto convert_format(DXGI_FORMAT format) -> api::format;

	auto convert_access_flags(api::map_access access) -> D3D10_MAP;
	api::map_access convert_access_flags(D3D10_MAP map_type);

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

	void convert_input_element(const api::input_element &desc, D3D10_INPUT_ELEMENT_DESC &internal_desc);
	api::input_element convert_input_element(const D3D10_INPUT_ELEMENT_DESC &internal_desc);

	void convert_blend_desc(const api::blend_desc &desc, D3D10_BLEND_DESC &internal_desc);
	void convert_blend_desc(const api::blend_desc &desc, D3D10_BLEND_DESC1 &internal_desc);
	api::blend_desc convert_blend_desc(const D3D10_BLEND_DESC *internal_desc);
	api::blend_desc convert_blend_desc(const D3D10_BLEND_DESC1 *internal_desc);
	void convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D10_RASTERIZER_DESC &internal_desc);
	api::rasterizer_desc convert_rasterizer_desc(const D3D10_RASTERIZER_DESC *internal_desc);
	void convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D10_DEPTH_STENCIL_DESC &internal_desc);
	api::depth_stencil_desc convert_depth_stencil_desc(const D3D10_DEPTH_STENCIL_DESC *internal_desc);

	auto convert_blend_op(api::blend_op value) -> D3D10_BLEND_OP;
	auto convert_blend_op(D3D10_BLEND_OP value) -> api::blend_op;
	auto convert_blend_factor(api::blend_factor value) -> D3D10_BLEND;
	auto convert_blend_factor(D3D10_BLEND value) -> api::blend_factor;
	auto convert_fill_mode(api::fill_mode value) -> D3D10_FILL_MODE;
	auto convert_fill_mode(D3D10_FILL_MODE value) -> api::fill_mode;
	auto convert_cull_mode(api::cull_mode value) -> D3D10_CULL_MODE;
	auto convert_cull_mode(D3D10_CULL_MODE value) -> api::cull_mode;
	auto convert_compare_op(api::compare_op value) -> D3D10_COMPARISON_FUNC;
	auto convert_compare_op(D3D10_COMPARISON_FUNC value) ->api::compare_op;
	auto convert_stencil_op(api::stencil_op value) -> D3D10_STENCIL_OP;
	auto convert_stencil_op(D3D10_STENCIL_OP value) -> api::stencil_op;
	auto convert_primitive_topology(api::primitive_topology value) -> D3D10_PRIMITIVE_TOPOLOGY;
	auto convert_primitive_topology(D3D10_PRIMITIVE_TOPOLOGY value) -> api::primitive_topology;
	auto convert_query_type(api::query_type value) -> D3D10_QUERY;

	inline auto to_handle(ID3D10SamplerState *ptr) { return api::sampler { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10Resource *ptr) { return api::resource { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10View *ptr) { return api::resource_view { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10InputLayout *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10VertexShader *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10GeometryShader *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10PixelShader *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10BlendState *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10RasterizerState *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D10DepthStencilState *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDXGIKeyedMutex *ptr) { return api::fence { reinterpret_cast<uintptr_t>(ptr) }; }
}
