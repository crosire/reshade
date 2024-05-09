/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d9.h>
#include "com_ptr.hpp"
#include "reshade_api_pipeline.hpp"
#include <vector>

namespace reshade::d3d9
{
	static_assert(sizeof(D3DBOX) == sizeof(api::subresource_box));
	static_assert(sizeof(D3DRECT) == sizeof(api::rect));

	struct sampler_impl
	{
		DWORD state[11];
	};

	struct pipeline_impl
	{
		com_ptr<IDirect3DStateBlock9> state_block;
		D3DPRIMITIVETYPE prim_type;
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
		api::query_type type;
		std::vector<com_ptr<IDirect3DQuery9>> queries;
	};

	struct fence_impl
	{
		uint64_t current_value;
		com_ptr<IDirect3DQuery9> event_queries[8];
	};

	constexpr api::resource global_index_buffer = { 0xFFFFFFFFFFFFFFF1 };
	constexpr api::resource global_vertex_buffer = { 0xFFFFFFFFFFFFFFF2 };

	constexpr api::pipeline_layout global_pipeline_layout = { 0xFFFFFFFFFFFFFFFF };

	auto convert_format(api::format format, BOOL lockable = FALSE) -> D3DFORMAT;
	auto convert_format(D3DFORMAT d3d_format, BOOL *lockable = nullptr) -> api::format;

	void convert_memory_heap_to_d3d_pool(api::memory_heap heap, D3DPOOL &d3d_pool);
	void convert_d3d_pool_to_memory_heap(D3DPOOL d3d_pool, api::memory_heap &heap);

	void convert_resource_usage_to_d3d_usage(api::resource_usage usage, DWORD &d3d_usage);
	void convert_d3d_usage_to_resource_usage(DWORD d3d_usage, api::resource_usage &usage);

	auto convert_access_flags(api::map_access access) -> DWORD;
	api::map_access convert_access_flags(DWORD lock_flags);

	void convert_sampler_desc(const api::sampler_desc &desc, DWORD state[11]);
	api::sampler_desc convert_sampler_desc(const DWORD state[11]);

	void convert_resource_desc(const api::resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels, const D3DCAPS9 &caps);
	void convert_resource_desc(const api::resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels, BOOL *lockable, const D3DCAPS9 &caps);
	void convert_resource_desc(const api::resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels = 1, bool shared_handle = false);
	api::resource_desc convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, BOOL lockable, const D3DCAPS9 &caps, bool shared_handle = false);
	api::resource_desc convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc, bool shared_handle = false);
	api::resource_desc convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc, bool shared_handle = false);

	void convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<D3DVERTEXELEMENT9> &internal_elements);
	std::vector<api::input_element> convert_input_layout_desc(const D3DVERTEXELEMENT9 *internal_elements);

	auto convert_blend_op(D3DBLENDOP value) -> api::blend_op;
	auto convert_blend_op(api::blend_op value) -> D3DBLENDOP;
	auto convert_blend_factor(D3DBLEND value) -> api::blend_factor;
	auto convert_blend_factor(api::blend_factor value) -> D3DBLEND;
	auto convert_fill_mode(D3DFILLMODE value) ->api::fill_mode;
	auto convert_fill_mode(api::fill_mode value) -> D3DFILLMODE;
	auto convert_cull_mode(D3DCULL value, bool front_counter_clockwise) -> api::cull_mode;
	auto convert_cull_mode(api::cull_mode value, bool front_counter_clockwise) -> D3DCULL;
	auto convert_compare_op(D3DCMPFUNC value) -> api::compare_op;
	auto convert_compare_op(api::compare_op value) -> D3DCMPFUNC;
	auto convert_stencil_op(D3DSTENCILOP value) -> api::stencil_op;
	auto convert_stencil_op(api::stencil_op value) -> D3DSTENCILOP;
	auto convert_primitive_topology(D3DPRIMITIVETYPE value) -> api::primitive_topology;
	auto convert_primitive_topology(api::primitive_topology value) -> D3DPRIMITIVETYPE;
	auto convert_query_type(api::query_type value) -> D3DQUERYTYPE;
	auto convert_dynamic_state(D3DRENDERSTATETYPE value) -> api::dynamic_state;
	auto convert_dynamic_state(api::dynamic_state value) -> D3DRENDERSTATETYPE;

	UINT calc_vertex_from_prim_count(D3DPRIMITIVETYPE type, UINT count);
	UINT calc_prim_from_vertex_count(D3DPRIMITIVETYPE type, UINT count);

	inline auto to_handle(IDirect3DResource9 *ptr) { return api::resource { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDirect3DVolume9 *ptr) { return api::resource_view { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDirect3DSurface9 *ptr) { return api::resource_view { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDirect3DVertexDeclaration9 *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDirect3DVertexShader9 *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(IDirect3DPixelShader9 *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
}
