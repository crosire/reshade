/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::d3d10
{
	extern const reshade::api::pipeline_state pipeline_states_blend[11];
	extern const reshade::api::pipeline_state pipeline_states_rasterizer[10];
	extern const reshade::api::pipeline_state pipeline_states_depth_stencil[15];

	void fill_pipeline_state_values(ID3D10BlendState *state, const FLOAT factor[4], UINT sample_mask, uint32_t (&values)[ARRAYSIZE(pipeline_states_blend)]);
	void fill_pipeline_state_values(ID3D10RasterizerState *state, uint32_t (&values)[ARRAYSIZE(pipeline_states_rasterizer)]);
	void fill_pipeline_state_values(ID3D10DepthStencilState *state, UINT ref, uint32_t (&values)[ARRAYSIZE(pipeline_states_depth_stencil)]);

	void convert_blend_op(api::blend_op value, D3D10_BLEND_OP &internal_value);
	void convert_blend_factor(api::blend_factor value, D3D10_BLEND &internal_value);
	void convert_fill_mode(api::fill_mode value, D3D10_FILL_MODE &internal_value);
	void convert_cull_mode(api::cull_mode value, D3D10_CULL_MODE &internal_value);
	void convert_compare_op(api::compare_op value, D3D10_COMPARISON_FUNC &internal_value);
	void convert_stencil_op(api::stencil_op value, D3D10_STENCIL_OP &internal_value);
	auto convert_primitive_topology(api::primitive_topology value) -> D3D10_PRIMITIVE_TOPOLOGY;

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
}
