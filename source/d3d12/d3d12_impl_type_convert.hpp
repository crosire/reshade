/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <d3d12.h>

namespace reshade::d3d12
{
	extern const GUID extra_data_guid;

	struct render_pass_impl
	{
		UINT count;
		DXGI_FORMAT rtv_format[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
		DXGI_FORMAT dsv_format;
		DXGI_SAMPLE_DESC sample_desc;
		std::vector<api::attachment_desc> attachments;
	};

	struct framebuffer_impl
	{
		UINT count;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
		D3D12_CPU_DESCRIPTOR_HANDLE dsv;
		BOOL rtv_is_single_handle_to_range;
	};

	struct pipeline_graphics_impl
	{
		D3D12_PRIMITIVE_TOPOLOGY topology;
		FLOAT blend_constant[4];
	};

	struct descriptor_set_layout_impl
	{
		std::vector<api::descriptor_range> ranges;
	};

	auto convert_format(api::format format) -> DXGI_FORMAT;
	auto convert_format(DXGI_FORMAT format) -> api::format;

	D3D12_RESOURCE_STATES convert_resource_usage_to_states(api::resource_usage usage);

	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_SAMPLER_DESC &internal_desc);

	void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc, D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS &heap_flags);
	inline void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc)
	{
		D3D12_HEAP_FLAGS dummy_heap_flags;
		D3D12_HEAP_PROPERTIES dummy_heap_props;
		convert_resource_desc(desc, internal_desc, dummy_heap_props, dummy_heap_flags);
	}
	api::resource_desc convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props = {}, D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);

	void convert_pipeline_desc(const api::pipeline_desc &desc, D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc);
	void convert_pipeline_desc(const api::pipeline_desc &desc, D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc);
	api::pipeline_desc convert_pipeline_desc(const D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc);
	api::pipeline_desc convert_pipeline_desc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc);

	auto convert_logic_op(api::logic_op value) -> D3D12_LOGIC_OP;
	auto convert_logic_op(D3D12_LOGIC_OP value) -> api::logic_op;
	auto convert_blend_op(api::blend_op value) -> D3D12_BLEND_OP;
	auto convert_blend_op(D3D12_BLEND_OP value) -> api::blend_op;
	auto convert_blend_factor(api::blend_factor value) -> D3D12_BLEND;
	auto convert_blend_factor(D3D12_BLEND value) -> api::blend_factor;
	auto convert_fill_mode(api::fill_mode value) -> D3D12_FILL_MODE;
	auto convert_fill_mode(D3D12_FILL_MODE value) -> api::fill_mode;
	auto convert_cull_mode(api::cull_mode value) -> D3D12_CULL_MODE;
	auto convert_cull_mode(D3D12_CULL_MODE value) -> api::cull_mode;
	auto convert_compare_op(api::compare_op value) -> D3D12_COMPARISON_FUNC;
	auto convert_compare_op(D3D12_COMPARISON_FUNC value) -> api::compare_op;
	auto convert_stencil_op(api::stencil_op value) -> D3D12_STENCIL_OP;
	auto convert_stencil_op(D3D12_STENCIL_OP value) -> api::stencil_op;
	auto convert_primitive_topology(api::primitive_topology value) -> D3D12_PRIMITIVE_TOPOLOGY;
	auto convert_primitive_topology(D3D12_PRIMITIVE_TOPOLOGY value) -> api::primitive_topology;
	auto convert_primitive_topology_type(api::primitive_topology value) -> D3D12_PRIMITIVE_TOPOLOGY_TYPE;
	auto convert_primitive_topology_type(D3D12_PRIMITIVE_TOPOLOGY_TYPE value) -> api::primitive_topology;

	auto convert_query_type(api::query_type type) -> D3D12_QUERY_TYPE;
	auto convert_query_type(D3D12_QUERY_TYPE type) -> api::query_type;
	auto convert_query_type_to_heap_type(api::query_type type) -> D3D12_QUERY_HEAP_TYPE;

	auto convert_descriptor_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_RANGE_TYPE;
	auto convert_descriptor_type(D3D12_DESCRIPTOR_RANGE_TYPE type) -> api::descriptor_type;
	auto convert_descriptor_type_to_heap_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_HEAP_TYPE;

	auto convert_shader_visibility(D3D12_SHADER_VISIBILITY visibility) -> api::shader_stage;
}
