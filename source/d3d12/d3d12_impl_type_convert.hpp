/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d12.h>
#include "reshade_api_pipeline.hpp"
#include <vector>

namespace reshade::d3d12
{
	static_assert(sizeof(D3D12_BOX) == sizeof(api::subresource_box));
	static_assert(sizeof(D3D12_RECT) == sizeof(api::rect));
	static_assert(sizeof(D3D12_VIEWPORT) == sizeof(api::viewport));
	static_assert(sizeof(D3D12_GPU_DESCRIPTOR_HANDLE) == sizeof(api::descriptor_table));

	struct pipeline_extra_data
	{
		D3D12_PRIMITIVE_TOPOLOGY topology;
		FLOAT blend_constant[4];
	};

	struct pipeline_layout_extra_data
	{
		const std::pair<D3D12_DESCRIPTOR_HEAP_TYPE, UINT> *ranges;
	};

	struct query_heap_extra_data
	{
		UINT size;
		ID3D12Resource *readback_resource;
		std::pair<ID3D12Fence *, UINT64> *fences;
	};

	extern const GUID extra_data_guid;

	auto convert_format(api::format format) -> DXGI_FORMAT;
	auto convert_format(DXGI_FORMAT format) -> api::format;

	auto convert_color_space(api::color_space type) -> DXGI_COLOR_SPACE_TYPE;
	auto convert_color_space(DXGI_COLOR_SPACE_TYPE type) -> api::color_space;

	auto convert_access_to_usage(D3D12_BARRIER_ACCESS access) -> api::resource_usage;
	auto convert_barrier_layout_to_usage(D3D12_BARRIER_LAYOUT layout) -> api::resource_usage;
	auto convert_resource_states_to_usage(D3D12_RESOURCE_STATES states) -> api::resource_usage;

	auto convert_usage_to_access(api::resource_usage usage) -> D3D12_BARRIER_ACCESS;
	auto convert_usage_to_resource_states(api::resource_usage usage) -> D3D12_RESOURCE_STATES;

	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC &internal_desc);
	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_SAMPLER_DESC2 &internal_desc);
	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_STATIC_SAMPLER_DESC &internal_desc);
	void convert_sampler_desc(const api::sampler_desc &desc, D3D12_STATIC_SAMPLER_DESC1 &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_SAMPLER_DESC2 &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_STATIC_SAMPLER_DESC &internal_desc);
	api::sampler_desc convert_sampler_desc(const D3D12_STATIC_SAMPLER_DESC1 &internal_desc);

	void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc, D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS &heap_flags);
	void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC1 &internal_desc, D3D12_HEAP_PROPERTIES &heap_props, D3D12_HEAP_FLAGS &heap_flags);
	inline void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc)
	{
		D3D12_HEAP_FLAGS dummy_heap_flags;
		D3D12_HEAP_PROPERTIES dummy_heap_props;
		convert_resource_desc(desc, internal_desc, dummy_heap_props, dummy_heap_flags);
	}
	inline void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC1 &internal_desc)
	{
		D3D12_HEAP_FLAGS dummy_heap_flags;
		D3D12_HEAP_PROPERTIES dummy_heap_props;
		convert_resource_desc(desc, internal_desc, dummy_heap_props, dummy_heap_flags);
	}
	api::resource_desc convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props = {}, D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE);
	api::resource_desc convert_resource_desc(const D3D12_RESOURCE_DESC1 &internal_desc, const D3D12_HEAP_PROPERTIES &heap_props = {}, D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RESOURCE_DESC &resource_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);

	void convert_shader_desc(const api::shader_desc &desc, D3D12_SHADER_BYTECODE &internal_desc);
	api::shader_desc convert_shader_desc(const D3D12_SHADER_BYTECODE &internal_desc);

	void convert_input_layout_desc(uint32_t count, const api::input_element *elements, std::vector<D3D12_INPUT_ELEMENT_DESC> &internal_elements);
	std::vector<api::input_element> convert_input_layout_desc(const D3D12_INPUT_LAYOUT_DESC &internal_desc);

	void convert_stream_output_desc(const api::stream_output_desc &desc, D3D12_STREAM_OUTPUT_DESC &internal_desc);
	api::stream_output_desc convert_stream_output_desc(const D3D12_STREAM_OUTPUT_DESC &internal_desc);
	void convert_blend_desc(const api::blend_desc &desc, D3D12_BLEND_DESC &internal_desc);
	api::blend_desc convert_blend_desc(const D3D12_BLEND_DESC &internal_desc);
	void convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC &internal_desc);
	void convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC1 &internal_desc);
	void convert_rasterizer_desc(const api::rasterizer_desc &desc, D3D12_RASTERIZER_DESC2 &internal_desc);
	api::rasterizer_desc convert_rasterizer_desc(const D3D12_RASTERIZER_DESC &internal_desc);
	api::rasterizer_desc convert_rasterizer_desc(const D3D12_RASTERIZER_DESC1 &internal_desc);
	api::rasterizer_desc convert_rasterizer_desc(const D3D12_RASTERIZER_DESC2 &internal_desc);
	void convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC &internal_desc);
	void convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC1 &internal_desc);
	void convert_depth_stencil_desc(const api::depth_stencil_desc &desc, D3D12_DEPTH_STENCIL_DESC2 &internal_desc);
	api::depth_stencil_desc convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC &internal_desc);
	api::depth_stencil_desc convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC1 &internal_desc);
	api::depth_stencil_desc convert_depth_stencil_desc(const D3D12_DEPTH_STENCIL_DESC2 &internal_desc);

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
	auto convert_query_heap_type_to_type(D3D12_QUERY_HEAP_TYPE type) -> api::query_type;

	auto convert_descriptor_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_RANGE_TYPE;
	auto convert_descriptor_type(D3D12_DESCRIPTOR_RANGE_TYPE type) -> api::descriptor_type;
	auto convert_descriptor_type_to_heap_type(api::descriptor_type type) -> D3D12_DESCRIPTOR_HEAP_TYPE;

	auto convert_shader_visibility(D3D12_SHADER_VISIBILITY visibility) -> api::shader_stage;

	auto convert_render_pass_load_op(api::render_pass_load_op value) -> D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE;
	auto convert_render_pass_load_op(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE value) -> api::render_pass_load_op;
	auto convert_render_pass_store_op(api::render_pass_store_op value) -> D3D12_RENDER_PASS_ENDING_ACCESS_TYPE;
	auto convert_render_pass_store_op(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE value) -> api::render_pass_store_op;

	auto convert_fence_flags(api::fence_flags value) -> D3D12_FENCE_FLAGS;

	auto convert_pipeline_flags(api::pipeline_flags value) -> D3D12_RAYTRACING_PIPELINE_FLAGS;
	auto convert_pipeline_flags(D3D12_RAYTRACING_PIPELINE_FLAGS value) -> api::pipeline_flags;
	auto convert_acceleration_structure_type(api::acceleration_structure_type value) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE;
	auto convert_acceleration_structure_type(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE value) -> api::acceleration_structure_type;
	auto convert_acceleration_structure_copy_mode(api::acceleration_structure_copy_mode value) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE;
	auto convert_acceleration_structure_copy_mode(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE value) -> api::acceleration_structure_copy_mode;
	auto convert_acceleration_structure_build_flags(api::acceleration_structure_build_flags value, api::acceleration_structure_build_mode mode) -> D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS;
	auto convert_acceleration_structure_build_flags(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS value) -> api::acceleration_structure_build_flags;

	void convert_acceleration_structure_build_input(const api::acceleration_structure_build_input &build_input, D3D12_RAYTRACING_GEOMETRY_DESC &geometry);
	api::acceleration_structure_build_input convert_acceleration_structure_build_input(const D3D12_RAYTRACING_GEOMETRY_DESC &geometry);

	inline auto to_handle(ID3D12Resource *ptr) { return api::resource { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { return api::resource_view { static_cast<uint64_t>(handle.ptr) }; }
	inline auto to_handle(ID3D12PipelineState *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D12StateObject *ptr) { return api::pipeline { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D12RootSignature *ptr) { return api::pipeline_layout { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D12QueryHeap *ptr) { return api::query_heap { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D12DescriptorHeap *ptr) { return api::descriptor_heap { reinterpret_cast<uintptr_t>(ptr) }; }
	inline auto to_handle(ID3D12Fence *ptr) { return api::fence { reinterpret_cast<uintptr_t>(ptr) }; }
}

#pragma warning(push)
#pragma warning(disable : 4324)
template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename T>
struct alignas(void *) D3D12_PIPELINE_STATE_STREAM
{
	D3D12_PIPELINE_STATE_STREAM() {}
	D3D12_PIPELINE_STATE_STREAM(const T &data) : data(data) {}

	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
	T data = {};
};
#pragma warning(pop)

typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature *> D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_VS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_PS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_DS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_HS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_GS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_CS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT, D3D12_STREAM_OUTPUT_DESC> D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC> D3D12_PIPELINE_STATE_STREAM_BLEND_DESC;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT> D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC> D3D12_PIPELINE_STATE_STREAM_RASTERIZER;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC> D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC> D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE> D3D12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE> D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY> D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT> D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC> D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK, UINT> D3D12_PIPELINE_STATE_STREAM_NODE_MASK;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO, D3D12_CACHED_PIPELINE_STATE> D3D12_PIPELINE_STATE_STREAM_CACHED_PSO;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS, D3D12_PIPELINE_STATE_FLAGS> D3D12_PIPELINE_STATE_STREAM_FLAGS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, D3D12_DEPTH_STENCIL_DESC1> D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING, D3D12_VIEW_INSTANCING_DESC> D3D12_PIPELINE_STATE_STREAM_VIEW_INSTANCING;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_AS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE> D3D12_PIPELINE_STATE_STREAM_MS;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2, D3D12_DEPTH_STENCIL_DESC2> D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1, D3D12_RASTERIZER_DESC1> D3D12_PIPELINE_STATE_STREAM_RASTERIZER1;
typedef D3D12_PIPELINE_STATE_STREAM<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2, D3D12_RASTERIZER_DESC2> D3D12_PIPELINE_STATE_STREAM_RASTERIZER2;
