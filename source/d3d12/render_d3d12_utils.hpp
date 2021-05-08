/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::d3d12
{
	void convert_blend_op(api::blend_op value, D3D12_BLEND_OP &internal_value);
	void convert_blend_factor(api::blend_factor value, D3D12_BLEND &internal_value);
	void convert_fill_mode(api::fill_mode value, D3D12_FILL_MODE &internal_value);
	void convert_cull_mode(api::cull_mode value, D3D12_CULL_MODE &internal_value);
	void convert_compare_op(api::compare_op value, D3D12_COMPARISON_FUNC &internal_value);
	void convert_stencil_op(api::stencil_op value, D3D12_STENCIL_OP &internal_value);

	auto convert_descriptor_type(api::descriptor_type type)->D3D12_DESCRIPTOR_RANGE_TYPE;
	auto convert_descriptor_type_to_heap_type(api::descriptor_type type)->D3D12_DESCRIPTOR_HEAP_TYPE;

	D3D12_RESOURCE_STATES convert_resource_usage_to_states(api::resource_usage usage);

	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_SAMPLER_DESC &internal_desc);

	void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc, D3D12_HEAP_PROPERTIES &heap_props);
	api::resource_desc convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props = { D3D12_HEAP_TYPE_CUSTOM });

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
}
