/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

namespace reshade::d3d9
{
	struct pipeline_impl
	{
		com_ptr<IDirect3DStateBlock9> state_block;
		D3DPRIMITIVETYPE prim_type;
	};

	struct pipeline_layout_impl
	{
		std::vector<UINT> shader_registers;
	};

	struct query_pool_impl
	{
		reshade::api::query_type type;
		std::vector<com_ptr<IDirect3DQuery9>> queries;
	};

	struct descriptor_set_impl
	{
		reshade::api::descriptor_type type;
		std::vector<uint64_t> descriptors;
		std::vector<reshade::api::sampler_with_resource_view> sampler_with_resource_views;
	};

	struct descriptor_set_layout_impl
	{
		reshade::api::descriptor_range range;
	};

	auto convert_format(api::format format, bool lockable = false) -> D3DFORMAT;
	auto convert_format(D3DFORMAT d3d_format) -> api::format;

	void convert_memory_heap_to_d3d_pool(api::memory_heap heap, D3DPOOL &d3d_pool);
	void convert_d3d_pool_to_memory_heap(D3DPOOL d3d_pool, api::memory_heap &heap);

	void convert_resource_usage_to_d3d_usage(api::resource_usage usage, DWORD &d3d_usage);
	void convert_d3d_usage_to_resource_usage(DWORD d3d_usage, api::resource_usage &usage);

	void convert_resource_desc(const api::resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels = nullptr);
	void convert_resource_desc(const api::resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels = nullptr);
	void convert_resource_desc(const api::resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels = 1);
	api::resource_desc convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, const D3DCAPS9 &caps);
	api::resource_desc convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc);

	auto convert_blend_op(api::blend_op value) -> D3DBLENDOP;
	auto convert_blend_factor(api::blend_factor value) -> D3DBLEND;
	auto convert_fill_mode(api::fill_mode value) -> D3DFILLMODE;
	auto convert_cull_mode(api::cull_mode value, bool front_counter_clockwise) -> D3DCULL;
	auto convert_compare_op(api::compare_op value) -> D3DCMPFUNC;
	auto convert_stencil_op(api::stencil_op value) -> D3DSTENCILOP;
	auto convert_primitive_topology(api::primitive_topology value) -> D3DPRIMITIVETYPE;
	auto convert_query_type(api::query_type value) -> D3DQUERYTYPE;

	UINT calc_vertex_from_prim_count(D3DPRIMITIVETYPE type, UINT count);
	UINT calc_prim_from_vertex_count(D3DPRIMITIVETYPE type, UINT count);
}
