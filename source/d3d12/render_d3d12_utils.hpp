/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::d3d12
{
	D3D12_RESOURCE_STATES convert_resource_usage_to_states(api::resource_usage usage);

	void convert_memory_usage(api::memory_usage mem_usage, D3D12_HEAP_TYPE &heap_type);
	api::memory_usage convert_memory_usage(D3D12_HEAP_TYPE heap_type);

	void convert_resource_desc(api::resource_type type, const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc);
	std::pair<api::resource_type, api::resource_desc> convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
}
