/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::d3d12
{
	extern const GUID pipeline_state_guid;
	extern const reshade::api::pipeline_state pipeline_states_graphics[33];

	void fill_pipeline_state_values(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, uint32_t (&values)[ARRAYSIZE(pipeline_states_graphics)]);

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
