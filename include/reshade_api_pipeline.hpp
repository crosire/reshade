/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api_resource.hpp"
#include <cstddef>

namespace reshade { namespace api
{
	/// <summary>
	/// Flags that specify the shader stages in the render pipeline.
	/// </summary>
	enum class shader_stage : uint32_t
	{
		vertex = 0x1,
		hull = 0x2,
		domain = 0x4,
		geometry = 0x8,
		pixel = 0x10,
		compute = 0x20,

		amplification = 0x40,
		mesh = 0x80,

		raygen = 0x0100,
		any_hit = 0x0200,
		closest_hit = 0x0400,
		miss = 0x0800,
		intersection = 0x1000,
		callable = 0x2000,

		all = 0x7FFFFFFF,
		all_compute = compute,
		all_graphics = vertex | hull | domain | geometry | pixel | amplification | mesh,
		all_ray_tracing = raygen | any_hit | closest_hit | miss | intersection | callable
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(shader_stage);

	/// <summary>
	/// Flags that specify the pipeline stages in the render pipeline.
	/// </summary>
	enum class pipeline_stage : uint32_t
	{
		vertex_shader = 0x8,
		hull_shader = 0x10,
		domain_shader = 0x20,
		geometry_shader = 0x40,
		pixel_shader = 0x80,
		compute_shader = 0x800,

		amplification_shader = 0x80000,
		mesh_shader = 0x100000,

		ray_tracing_shader = 0x00200000,

		input_assembler = 0x2,
		stream_output = 0x4,
		rasterizer = 0x100,
		depth_stencil = 0x200,
		output_merger = 0x400,

		all = 0x7FFFFFFF,
		all_compute = compute_shader,
		all_graphics = vertex_shader | hull_shader | domain_shader | geometry_shader | pixel_shader | input_assembler | stream_output | rasterizer | depth_stencil | output_merger,
		all_ray_tracing = ray_tracing_shader,
		all_shader_stages = vertex_shader | hull_shader | domain_shader | geometry_shader | pixel_shader | compute_shader
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(pipeline_stage);

	/// <summary>
	/// Type of a descriptor.
	/// </summary>
	enum class descriptor_type : uint32_t
	{
		/// <summary>
		/// Descriptors are an array of <see cref="sampler"/>.
		/// </summary>
		sampler = 0,
		/// <summary>
		/// Descriptors are an array of <see cref="sampler_with_resource_view"/>.
		/// </summary>
		sampler_with_resource_view = 1,
		/// <summary>
		/// Descriptors are either of type <see cref="buffer_shader_resource_view"/> or <see cref="texture_shader_resource_view"/>.
		/// </summary>
		shader_resource_view = 2,
		/// <summary>
		/// Descriptors are either of type <see cref="buffer_unordered_access_view"/> or <see cref="texture_unordered_access_view"/>.
		/// </summary>
		unordered_access_view = 3,
		/// <summary>
		/// Descriptors are an array of <see cref="resource_view"/>.
		/// </summary>
		buffer_shader_resource_view = 4,
		/// <summary>
		/// Descriptors are an array of <see cref="resource_view"/>.
		/// </summary>
		buffer_unordered_access_view = 5,
		/// <summary>
		/// Descriptors are an array of <see cref="resource_view"/>.
		/// </summary>
		texture_shader_resource_view = shader_resource_view,
		/// <summary>
		/// Descriptors are an array of <see cref="resource_view"/>.
		/// </summary>
		texture_unordered_access_view = unordered_access_view,
		/// <summary>
		/// Descriptors are an array of <see cref="buffer_range"/>.
		/// </summary>
		constant_buffer = 6,
		/// <summary>
		/// Descriptors are an array of <see cref="buffer_range"/>.
		/// </summary>
		shader_storage_buffer = 7,
		/// <summary>
		/// Descriptors are an array of <see cref="resource_view"/>.
		/// </summary>
		acceleration_structure = 10
	};

	/// <summary>
	/// Type of a pipeline layout parameter.
	/// </summary>
	enum class pipeline_layout_param_type : uint32_t
	{
		push_constants = 1,
		descriptor_table = 0,
		descriptor_table_with_static_samplers = 4,
		push_descriptors = 2,
		push_descriptors_with_ranges = 3,
		push_descriptors_with_static_samplers = 5
	};

	/// <summary>
	/// Describes a range of constants in a pipeline layout.
	/// </summary>
	struct constant_range
	{
		/// <summary>
		/// OpenGL uniform buffer binding index.
		/// </summary>
		uint32_t binding = 0;
		/// <summary>
		/// D3D10/D3D11/D3D12 constant buffer register index.
		/// </summary>
		uint32_t dx_register_index = 0;
		/// <summary>
		/// D3D12 constant buffer register space.
		/// </summary>
		uint32_t dx_register_space = 0;
		/// <summary>
		/// Number of constants in this range (in 32-bit values).
		/// </summary>
		uint32_t count = 0;
		/// <summary>
		/// Shader pipeline stages that can make use of the constants in this range.
		/// </summary>
		shader_stage visibility = shader_stage::all;
	};

	/// <summary>
	/// Describes a range of descriptors of a descriptor table in a pipeline layout.
	/// </summary>
	struct descriptor_range
	{
		/// <summary>
		/// OpenGL/Vulkan binding index (<c>layout(binding=X)</c> in GLSL).
		/// In D3D this is equivalent to the offset (in descriptors) of this range in the descriptor table (since each binding can only have an array size of 1).
		/// </summary>
		uint32_t binding = 0;
		/// <summary>
		/// D3D9/D3D10/D3D11/D3D12 shader register index (<c>register(xX)</c> in HLSL).
		/// </summary>
		uint32_t dx_register_index = 0;
		/// <summary>
		/// D3D12 register space (<c>register(..., spaceX)</c> in HLSL).
		/// </summary>
		uint32_t dx_register_space = 0;
		/// <summary>
		/// Number of descriptors in this range.
		/// Set to -1 (UINT32_MAX) to indicate an unbounded range.
		/// </summary>
		uint32_t count = 0;
		/// <summary>
		/// Shader pipeline stages that can make use of the descriptors in this range.
		/// </summary>
		shader_stage visibility = shader_stage::all;
		/// <summary>
		/// Size of the array in case this is an array binding.
		/// Only meaningful in Vulkan, in OpenGL and other APIs this has to be 1 (since each array element is a separate binding there).
		/// If this is less than the total number of descriptors specified in <see cref="count"/>, then the remaining descriptors are assigned a separate binding (with an array size of 1), with the binding index incrementing with each additional descriptor.
		/// </summary>
		uint32_t array_size = 1;
		/// <summary>
		/// Type of the descriptors in this range.
		/// </summary>
		descriptor_type type = descriptor_type::sampler;
	};
	struct descriptor_range_with_static_samplers : public descriptor_range
	{
		/// <summary>
		/// Optional array of sampler descriptions to statically embed into the descriptor table when the descriptor type is <see cref="descriptor_type::sampler"/> or <see cref="descriptor_type::sampler_with_resource_view"/>.
		/// </summary>
		const sampler_desc *static_samplers = nullptr;
	};

	/// <summary>
	/// Describes a single parameter in a pipeline layout.
	/// </summary>
	struct pipeline_layout_param
	{
		constexpr pipeline_layout_param() : push_descriptors() {}
		constexpr pipeline_layout_param(const constant_range &push_constants) : type(pipeline_layout_param_type::push_constants), push_constants(push_constants) {}
		constexpr pipeline_layout_param(const descriptor_range &push_descriptors) : type(pipeline_layout_param_type::push_descriptors), push_descriptors(push_descriptors) {}
		constexpr pipeline_layout_param(const descriptor_range_with_static_samplers &push_descriptors) : type(pipeline_layout_param_type::push_descriptors_with_static_samplers), descriptor_table_with_static_samplers({ 1, &push_descriptors }) {}
		constexpr pipeline_layout_param(uint32_t count, const descriptor_range *ranges) : type(pipeline_layout_param_type::descriptor_table), descriptor_table({ count, ranges }) {}
		constexpr pipeline_layout_param(uint32_t count, const descriptor_range_with_static_samplers *ranges) : type(pipeline_layout_param_type::descriptor_table_with_static_samplers), descriptor_table_with_static_samplers({ count, ranges }) {}

		/// <summary>
		/// Type of the parameter.
		/// </summary>
		pipeline_layout_param_type type = pipeline_layout_param_type::push_descriptors;

		union
		{
			/// <summary>
			/// Used when parameter type is <see cref="pipeline_layout_param_type::push_constants"/>.
			/// </summary>
			constant_range push_constants;

			/// <summary>
			/// Used when parameter type is <see cref="pipeline_layout_param_type::push_descriptors"/>.
			/// </summary>
			descriptor_range push_descriptors;

			/// <summary>
			/// Used when parameter type is <see cref="pipeline_layout_param_type::descriptor_table"/> or <see cref="pipeline_layout_param_type::push_descriptors_with_ranges"/>.
			/// </summary>
			struct
			{
				uint32_t count;
				const descriptor_range *ranges;
			} descriptor_table;

			/// <summary>
			/// Used when parameter type is <see cref="pipeline_layout_param_type::descriptor_table_with_static_samplers"/> or <see cref="pipeline_layout_param_type::push_descriptors_with_static_samplers"/>.
			/// </summary>
			struct
			{
				uint32_t count;
				const descriptor_range_with_static_samplers *ranges;
			} descriptor_table_with_static_samplers;
		};
	};

	/// <summary>
	/// An opaque handle to a pipeline layout object.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: An opaque value.</item>
	/// <item>Direct3D 10: An opaque value.</item>
	/// <item>Direct3D 11: An opaque value.</item>
	/// <item>Direct3D 12: A pointer to a 'ID3D12RootSignature' object.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkPipelineLayout' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline_layout);

	/// <summary>
	/// Fill mode to use when rendering triangles.
	/// </summary>
	enum class fill_mode : uint32_t
	{
		solid = 0,
		wireframe = 1,
		point = 2
	};

	/// <summary>
	/// Indicates triangles facing a particular direction are not drawn.
	/// </summary>
	enum class cull_mode : uint32_t
	{
		none = 0,
		front = 1,
		back = 2,
		front_and_back = front | back
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(cull_mode);

	/// <summary>
	/// Logic operations.
	/// </summary>
	enum class logic_op : uint32_t
	{
		clear = 0,
		bitwise_and = 1,
		bitwise_and_reverse = 2,
		copy = 3,
		bitwise_and_inverted = 4,
		noop = 5,
		bitwise_xor = 6,
		bitwise_or = 7,
		bitwise_nor = 8,
		equivalent = 9,
		invert = 10,
		bitwise_or_reverse = 11,
		copy_inverted = 12,
		bitwise_or_inverted = 13,
		bitwise_nand = 14,
		set = 15
	};

	/// <summary>
	/// Color or alpha blending operations.
	/// </summary>
	enum class blend_op : uint32_t
	{
		add = 0,
		subtract = 1,
		reverse_subtract = 2,
		min = 3,
		max = 4
	};

	/// <summary>
	/// Blend factors in color or alpha blending operations, which modulate values between the pixel shader output and render target.
	/// </summary>
	enum class blend_factor : uint32_t
	{
		zero = 0,
		one = 1,
		source_color = 2,
		one_minus_source_color = 3,
		dest_color = 4,
		one_minus_dest_color = 5,
		source_alpha = 6,
		one_minus_source_alpha = 7,
		dest_alpha = 8,
		one_minus_dest_alpha = 9,
		constant_color = 10,
		one_minus_constant_color = 11,
		constant_alpha = 12,
		one_minus_constant_alpha = 13,
		source_alpha_saturate = 14,
		source1_color = 15,
		one_minus_source1_color = 16,
		source1_alpha = 17,
		one_minus_source1_alpha = 18
	};

	/// <summary>
	/// Stencil operations that can be performed during depth-stencil testing.
	/// </summary>
	enum class stencil_op : uint32_t
	{
		keep = 0,
		zero = 1,
		replace = 2,
		increment_saturate = 3,
		decrement_saturate = 4,
		invert = 5,
		increment = 6,
		decrement = 7
	};

	/// <summary>
	/// Specifies how the pipeline interprets vertex data that is bound to the vertex input stage and subsequently renders it.
	/// </summary>
	enum class primitive_topology : uint32_t
	{
		undefined = 0,

		point_list = 1,
		line_list = 2,
		line_strip = 3,
		triangle_list = 4,
		triangle_strip = 5,
		triangle_fan = 6,
		quad_list = 8,
		quad_strip = 9,
		line_list_adj = 10,
		line_strip_adj = 11,
		triangle_list_adj = 12,
		triangle_strip_adj = 13,

		patch_list_01_cp = 33,
		patch_list_02_cp,
		patch_list_03_cp,
		patch_list_04_cp,
		patch_list_05_cp,
		patch_list_06_cp,
		patch_list_07_cp,
		patch_list_08_cp,
		patch_list_09_cp,
		patch_list_10_cp,
		patch_list_11_cp,
		patch_list_12_cp,
		patch_list_13_cp,
		patch_list_14_cp,
		patch_list_15_cp,
		patch_list_16_cp,
		patch_list_17_cp,
		patch_list_18_cp,
		patch_list_19_cp,
		patch_list_20_cp,
		patch_list_21_cp,
		patch_list_22_cp,
		patch_list_23_cp,
		patch_list_24_cp,
		patch_list_25_cp,
		patch_list_26_cp,
		patch_list_27_cp,
		patch_list_28_cp,
		patch_list_29_cp,
		patch_list_30_cp,
		patch_list_31_cp,
		patch_list_32_cp
	};

	/// <summary>
	/// Describes a shader object.
	/// </summary>
	struct shader_desc
	{
		/// <summary>
		/// Shader source code or binary.
		/// </summary>
		const void *code = nullptr;
		/// <summary>
		/// Size (in bytes) of the shader source <see cref="code"/> or binary.
		/// </summary>
		size_t code_size = 0;
		/// <summary>
		/// Optional entry point name if the shader source <see cref="code"/> or binary contains multiple entry points.
		/// Can be <see langword="nullptr"/> if it does not, or to use the default "main" entry point.
		/// </summary>
		const char *entry_point = nullptr;

		/// <summary>
		/// Number of entries in the <see cref="spec_constant_ids"/> and <see cref="spec_constant_values"/> arrays.
		/// This is meaningful only when the shader binary is a SPIR-V module and is ignored otherwise.
		/// </summary>
		uint32_t spec_constants = 0;
		/// <summary>
		/// Pointer to an array of specialization constant indices.
		/// </summary>
		const uint32_t *spec_constant_ids = nullptr;
		/// <summary>
		/// Pointer to an array of constant values, one for each specialization constant index in <see cref="spec_constant_ids"/>.
		/// </summary>
		const uint32_t *spec_constant_values = nullptr;
	};

	/// <summary>
	/// Type of a ray tracing shader group.
	/// </summary>
	enum class shader_group_type
	{
		raygen = 0,
		miss = 3,
		hit_group_triangles = 1,
		hit_group_aabbs = 2,
		callable = 4,
	};

	/// <summary>
	/// Describes a ray tracing shader group.
	/// </summary>
	struct shader_group
	{
		shader_group() : hit_group() {}
		shader_group(shader_group_type type, uint32_t closest_hit_shader_index, uint32_t any_hit_shader_index = UINT32_MAX, uint32_t intersection_shader_index = UINT32_MAX) : type(type), hit_group({ closest_hit_shader_index, any_hit_shader_index, intersection_shader_index }) {}

		/// <summary>
		/// Type of the shader group.
		/// </summary>
		shader_group_type type = shader_group_type::raygen;

		union
		{
			/// <summary>
			/// Used when type is <see cref="shader_group_type::raygen"/>.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the shader in the ray generation shader pipeline subobject.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::raygen_shader"/>
				uint32_t shader_index = UINT32_MAX;
			} raygen;

			/// <summary>
			/// Used when type is <see cref="shader_group_type::miss"/>.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the shader in the miss shader pipeline subobject.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::miss_shader"/>
				uint32_t shader_index = UINT32_MAX;
			} miss;

			/// <summary>
			/// Used when type is <see cref="shader_group_type::hit_group_triangles"/> or <see cref="shader_group_type::hit_group_aabbs"/>.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the shader in the closest-hit shader pipeline subobject.
				/// Set to -1 (UINT32_MAX) to indicate that this shader type is not used in the hit group.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::closest_shader"/>
				uint32_t closest_hit_shader_index = UINT32_MAX;
				/// <summary>
				/// Index of the shader in the any-hit shader pipeline subobject.
				/// Set to -1 (UINT32_MAX) to indicate that this shader type is not used in the hit group.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::any_hit_shader"/>
				uint32_t any_hit_shader_index = UINT32_MAX;
				/// <summary>
				/// Index of the shader in the intersection shader pipeline subobject.
				/// Set to -1 (UINT32_MAX) to indicate that this shader type is not used in the hit group.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::intersection_shader"/>
				uint32_t intersection_shader_index = UINT32_MAX;
			} hit_group;

			/// <summary>
			/// Used when type is <see cref="shader_group_type::callable"/>.
			/// </summary>
			struct
			{
				/// <summary>
				/// Index of the shader in the callable shader pipeline subobject.
				/// </summary>
				/// <seealso cref="pipeline_subobject_type::callable_shader"/>
				uint32_t shader_index = UINT32_MAX;
			} callable;
		};
	};

	/// <summary>
	/// Describes a single element in the vertex layout for the input-assembler stage.
	/// </summary>
	struct input_element
	{
		/// <summary>
		/// GLSL attribute location associated with this element (<c>layout(location = X)</c>).
		/// </summary>
		uint32_t location = 0;
		/// <summary>
		/// HLSL semantic associated with this element.
		/// </summary>
		const char *semantic = nullptr;
		/// <summary>
		/// Optional index for the HLSL semantic (e.g. for "TEXCOORD1" set <see cref="semantic"/> to "TEXCOORD" and <see cref="semantic_index"/> to 1).
		/// </summary>
		uint32_t semantic_index = 0;
		/// <summary>
		/// Format of the element data.
		/// </summary>
		format format = format::unknown;
		/// <summary>
		/// Index of the vertex buffer binding.
		/// </summary>
		uint32_t buffer_binding = 0;
		/// <summary>
		/// Offset (in bytes) from the start of the vertex to this element.
		/// </summary>
		uint32_t offset = 0;
		/// <summary>
		/// Stride of the entire vertex (this has to be consistent for all elements per vertex buffer binding).
		/// Set to zero in case this is unknown.
		/// </summary>
		uint32_t stride = 0;
		/// <summary>
		/// Number of instances to draw using the same per-instance data before advancing by one element.
		/// This has to be consistent for all elements per vertex buffer binding.
		/// Set to zero to indicate that this element is per-vertex rather than per-instance.
		/// </summary>
		uint32_t instance_step_rate = 0;
	};

	/// <summary>
	/// Describes the state of the stream-output stage.
	/// </summary>
	struct stream_output_desc
	{
		/// <summary>
		/// Index of the stream output stream to be sent to the rasterizer stage.
		/// </summary>
		uint32_t rasterized_stream = 0;
	};

	/// <summary>
	/// Describes the state of the output-merger stage.
	/// </summary>
	struct blend_desc
	{
		/// <summary>
		/// Use alpha-to-coverage as a multisampling technique when setting a pixel to a render target.
		/// </summary>
		bool alpha_to_coverage_enable = false;
		/// <summary>
		/// Enable or disable blending for each render target.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		bool blend_enable[8] = { false, false, false, false, false, false, false, false };
		/// <summary>
		/// Enable or disable a logical operation for each render target.
		/// </summary>
		/// <seealso cref="device_caps::logic_op"/>
		bool logic_op_enable[8] = { false, false, false, false, false, false, false, false };
		/// <summary>
		/// Source to use for the RGB value that the pixel shader outputs.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_factor source_color_blend_factor[8] = { blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one };
		/// <summary>
		/// Destination to use for the current RGB value in the render target.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_factor dest_color_blend_factor[8] = { blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero };
		/// <summary>
		/// Operation to use to combine <see cref="source_color_blend_factor"/> and <see cref="dest_color_blend_factor"/>.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_op color_blend_op[8] = { blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add };
		/// <summary>
		/// Source to use for the alpha value that the pixel shader outputs.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_factor source_alpha_blend_factor[8] = { blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one, blend_factor::one };
		/// <summary>
		/// Destination to use for the current alpha value in the render target.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_factor dest_alpha_blend_factor[8] = { blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero, blend_factor::zero };
		/// <summary>
		/// Operation to use to combine <see cref="source_alpha_blend_factor"/> and <see cref="dest_alpha_blend_factor"/>.
		/// </summary>
		/// <seealso cref="device_caps::independent_blend"/>
		blend_op alpha_blend_op[8] = { blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add, blend_op::add };
		/// <summary>
		/// Constant RGBA value to use when <see cref="source_color_blend_factor"/> or <see cref="dest_color_blend_factor"/> is <see cref="blend_factor::constant_color"/>.
		/// </summary>
		float blend_constant[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		/// <summary>
		/// Logical operation for each render target. Ignored if <see cref="logic_op_enable"/> is <see langword="false"/>.
		/// </summary>
		/// <seealso cref="device_caps::logic_op"/>
		logic_op logic_op[8] = { logic_op::noop, logic_op::noop, logic_op::noop, logic_op::noop, logic_op::noop, logic_op::noop, logic_op::noop, logic_op::noop };
		/// <summary>
		/// A write mask specifying which color components are written to each render target. Bitwise combination of <c>0x1</c> for red, <c>0x2</c> for green, <c>0x4</c> for blue and <c>0x8</c> for alpha.
		/// </summary>
		uint8_t render_target_write_mask[8] = { 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF };
	};

	/// <summary>
	/// Describes the state of the rasterizer stage.
	/// </summary>
	struct rasterizer_desc
	{
		/// <summary>
		/// Fill mode to use when rendering triangles.
		/// </summary>
		/// <seealso cref="device_caps::fill_mode_non_solid"/>
		fill_mode fill_mode = fill_mode::solid;
		/// <summary>
		/// Triangles facing the specified direction are not drawn.
		/// </summary>
		cull_mode cull_mode = cull_mode::back;
		/// <summary>
		/// Determines if a triangle is front or back-facing.
		/// </summary>
		bool front_counter_clockwise = false;
		/// <summary>
		/// Depth value added to a given pixel.
		/// </summary>
		float depth_bias = 0.0f;
		/// <summary>
		/// Maximum depth bias of a pixel.
		/// </summary>
		float depth_bias_clamp = 0.0f;
		/// <summary>
		/// Scalar on the slope of a given pixel.
		/// </summary>
		float slope_scaled_depth_bias = 0.0f;
		/// <summary>
		/// Enable or disable clipping based on distance.
		/// </summary>
		bool depth_clip_enable = true;
		/// <summary>
		/// Enable or disable scissor testing (scissor rectangle culling).
		/// </summary>
		bool scissor_enable = false;
		/// <summary>
		/// Use the quadrilateral or alpha line anti-aliasing algorithm on multisample antialiasing render targets.
		/// </summary>
		bool multisample_enable = false;
		/// <summary>
		/// Enable or disable line antialiasing. Only applies if doing line drawing and <see cref="multisample_enable"/> is <see langword="false"/>.
		/// </summary>
		bool antialiased_line_enable = false;
		/// <summary>
		/// Enable or disable conservative rasterization mode.
		/// </summary>
		/// <seealso cref="device_caps::conservative_rasterization"/>
		uint32_t conservative_rasterization = 0;
	};

	/// <summary>
	/// Describes the state of the depth-stencil stage.
	/// </summary>
	struct depth_stencil_desc
	{
		/// <summary>
		/// Enable or disable depth testing.
		/// </summary>
		bool depth_enable = true;
		/// <summary>
		/// Enable or disable writes to the depth-stencil buffer.
		/// </summary>
		bool depth_write_mask = true;
		/// <summary>
		/// Comparison function to use to compare new depth value from a fragment against current depth value in the depth-stencil buffer.
		/// </summary>
		compare_op depth_func = compare_op::less;
		/// <summary>
		/// Enable or disable stencil testing.
		/// </summary>
		bool stencil_enable = false;
		/// <summary>
		/// Mask applied to stencil values read from the depth-stencil buffer for pixels whose surface normal is towards the camera.
		/// </summary>
		uint8_t front_stencil_read_mask = 0xFF;
		/// <summary>
		/// Mask applied to stencil values written to the depth-stencil buffer for pixels whose surface normal is towards the camera.
		/// </summary>
		uint8_t front_stencil_write_mask = 0xFF;
		/// <summary>
		/// Reference value to perform against when stencil testing pixels whose surface normal is towards the camera.
		/// </summary>
		uint8_t front_stencil_reference_value = 0;
		/// <summary>
		/// Comparison function to use to compare new stencil value from a fragment against current stencil value for pixels whose surface normal is facing towards the camera.
		/// </summary>
		compare_op front_stencil_func = compare_op::always;
		/// <summary>
		/// Stencil operation to perform when stencil testing and depth testing both pass for pixels whose surface normal is facing towards the camera.
		/// </summary>
		stencil_op front_stencil_pass_op = stencil_op::keep;
		/// <summary>
		/// Stencil operation to perform when stencil testing fails for pixels whose surface normal is towards the camera.
		/// </summary>
		stencil_op front_stencil_fail_op = stencil_op::keep;
		/// <summary>
		/// Stencil operation to perform when stencil testing passes and depth testing fails for pixels whose surface normal is facing towards the camera.
		/// </summary>
		stencil_op front_stencil_depth_fail_op = stencil_op::keep;
		/// <summary>
		/// Mask applied to stencil values read from the depth-stencil buffer for pixels whose surface normal is facing away from the camera.
		/// </summary>
		uint8_t back_stencil_read_mask = 0xFF;
		/// <summary>
		/// Mask applied to stencil values written to the depth-stencil buffer for pixels whose surface normal is facing away from the camera.
		/// </summary>
		uint8_t back_stencil_write_mask = 0xFF;
		/// <summary>
		/// Reference value to perform against when stencil testing pixels whose surface normal is facing away from the camera.
		/// </summary>
		uint8_t back_stencil_reference_value = 0;
		/// <summary>
		/// Comparison function to use to compare new stencil value from a fragment against current stencil value for pixels whose surface normal is facing away from the camera.
		/// </summary>
		compare_op back_stencil_func = compare_op::always;
		/// <summary>
		/// Stencil operation to perform when stencil testing and depth testing both pass for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_pass_op = stencil_op::keep;
		/// <summary>
		/// Stencil operation to perform when stencil testing fails for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_fail_op = stencil_op::keep;
		/// <summary>
		/// Stencil operation to perform when stencil testing passes and depth testing fails for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_depth_fail_op = stencil_op::keep;
	};

	/// <summary>
	/// Flags that specify additional parameters of a pipeline.
	/// </summary>
	enum class pipeline_flags : uint32_t
	{
		none = 0,
		library = (1 << 0),
		skip_triangles = (1 << 1),
		skip_aabbs = (1 << 2),
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(pipeline_flags);

	/// <summary>
	/// Type of a pipeline sub-object.
	/// </summary>
	enum class pipeline_subobject_type : uint32_t
	{
		unknown,

		/// <summary>
		/// Vertex shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::vertex"/>
		/// <seealso cref="pipeline_stage::vertex_shader"/>
		vertex_shader,
		/// <summary>
		/// Hull shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::hull"/>
		/// <seealso cref="pipeline_stage::hull_shader"/>
		hull_shader,
		/// <summary>
		/// Domain shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::domain"/>
		/// <seealso cref="pipeline_stage::domain_shader"/>
		domain_shader,
		/// <summary>
		/// Geometry shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::geometry"/>
		/// <seealso cref="pipeline_stage::geometry_shader"/>
		geometry_shader,
		/// <summary>
		/// Pixel shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::pixel"/>
		/// <seealso cref="pipeline_stage::pixel_shader"/>
		pixel_shader,
		/// <summary>
		/// Compute shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::compute"/>
		/// <seealso cref="pipeline_stage::compute_shader"/>
		compute_shader,
		/// <summary>
		/// Vertex layout for the input-assembler stage.
		/// Sub-object data is a pointer to an array of <see cref="input_element"/>.
		/// </summary>
		/// <seealso cref="pipeline_stage::input_assembler"/>
		input_layout,
		/// <summary>
		/// State of the stream-output stage.
		/// Sub-object data is a pointer to a <see cref="stream_output_desc"/>.
		/// </summary>
		/// <seealso cref="pipeline_stage::stream_output"/>
		stream_output_state,
		/// <summary>
		/// State of the output-merger stage.
		/// Sub-object data is a pointer to a <see cref="blend_desc"/>.
		/// </summary>
		/// <seealso cref="pipeline_stage::output_merger"/>
		blend_state,
		/// <summary>
		/// State of the rasterizer stage.
		/// Sub-object data is a pointer to a <see cref="rasterizer_desc"/>.
		/// </summary>
		/// <seealso cref="pipeline_stage::rasterizer"/>
		rasterizer_state,
		/// <summary>
		/// State of the depth-stencil stage.
		/// Sub-object data is a pointer to a <see cref="depth_stencil_desc"/>.
		/// </summary>
		/// <seealso cref="pipeline_stage::depth_stencil"/>
		depth_stencil_state,
		/// <summary>
		/// Primitive topology to use when rendering.
		/// Sub-object data is a pointer to a <see cref="primitive_topology"/> value.
		/// </summary>
		primitive_topology,
		/// <summary>
		/// Format of the depth-stencil view that may be used with this pipeline.
		/// Sub-object data is a pointer to a <see cref="format"/> value.
		/// </summary>
		depth_stencil_format,
		/// <summary>
		/// Formats of the render target views that may be used with this pipeline.
		/// Sub-object data is a pointer to an array of <see cref="format"/> values.
		/// </summary>
		render_target_formats,
		/// <summary>
		/// Mask applied to the coverage mask for a fragment during rasterization.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		sample_mask,
		/// <summary>
		/// Number of samples used in rasterization.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		sample_count,
		/// <summary>
		/// Maximum number of viewports that may be bound via <see cref="command_list::bind_viewports"/> with this pipeline.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		viewport_count,
		/// <summary>
		/// States that may be dynamically updated via <see cref="command_list::bind_pipeline_states"/> after binding this pipeline.
		/// Sub-object data is a pointer to an array of <see cref="dynamic_state"/> values.
		/// </summary>
		dynamic_pipeline_states,
		/// <summary>
		/// Maximum number of vertices a draw call with this pipeline will draw.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		max_vertex_count,
		/// <summary>
		/// Amplification shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::amplification"/>
		/// <seealso cref="pipeline_stage::amplification_shader"/>
		amplification_shader,
		/// <summary>
		/// Mesh shader to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::mesh"/>
		/// <seealso cref="pipeline_stage::mesh_shader"/>
		mesh_shader,
		/// <summary>
		/// Ray generation shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::raygen"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		raygen_shader,
		/// <summary>
		/// Any-hit shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::any_hit"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		any_hit_shader,
		/// <summary>
		/// Closest-hit shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::closest_hit"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		closest_hit_shader,
		/// <summary>
		/// Miss shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::miss"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		miss_shader,
		/// <summary>
		/// Intersection shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::intersection"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		intersection_shader,
		/// <summary>
		/// Callable shader(s) to use.
		/// Sub-object data is a pointer to a <see cref="shader_desc"/>.
		/// </summary>
		/// <seealso cref="shader_stage::callable"/>
		/// <seealso cref="pipeline_stage::ray_tracing_shader"/>
		callable_shader,
		/// <summary>
		/// Existing shader libraries added to this pipeline.
		/// Sub-object data is a pointer to an array of <see cref="pipeline"/> handles.
		/// </summary>
		libraries,
		/// <summary>
		/// Ray tracing shader groups to use.
		/// Sub-object data is a pointer to an array of <see cref="shader_group"/> values.
		/// </summary>
		shader_groups,
		/// <summary>
		/// Maximum payload size of shaders executed by this pipeline.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		max_payload_size,
		/// <summary>
		/// Maximum hit attribute size of shaders executed by this pipeline.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		max_attribute_size,
		/// <summary>
		/// Maximum recursion depth of shaders executed by this pipeline.
		/// Sub-object data is a pointer to a 32-bit unsigned integer value.
		/// </summary>
		max_recursion_depth,
		/// <summary>
		/// Additional pipeline creation flags.
		/// Sub-object data is a pointer to a <see cref="pipeline_flags"/> value.
		/// </summary>
		flags
	};

	/// <summary>
	/// Describes a pipeline sub-object.
	/// </summary>
	struct pipeline_subobject
	{
		/// <summary>
		/// Type of the specified sub-object <see cref="data"/>.
		/// </summary>
		pipeline_subobject_type type = pipeline_subobject_type::unknown;
		/// <summary>
		/// Number of sub-object descriptions.
		/// This should usually be 1, except for array sub-objects like <see cref="pipeline_subobject_type::render_target_formats"/> (where this specifies the number of render targets) or <see cref="pipeline_subobject_type::dynamic_pipeline_states"/>.
		/// </summary>
		uint32_t count = 0;
		/// <summary>
		/// Pointer to an array of sub-object descriptions (which should be as large as the specified <see cref="count"/>).
		/// Depending on the sub-object <see cref="type"/> this should be a pointer to a <see cref="shader_desc"/>, <see cref="blend_desc"/>, <see cref="rasterizer_desc"/>, <see cref="depth_stencil_desc"/> or ...
		/// </summary>
		void *data = nullptr;
	};

	/// <summary>
	/// An opaque handle to a pipeline state object.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: A pointer to a 'IDirect3D(...)Shader' object.</item>
	/// <item>Direct3D 10: A pointer to a 'ID3D10(...)(Shader/State)' object.</item>
	/// <item>Direct3D 11: A pointer to a 'ID3D11(...)(Shader/State)' object.</item>
	/// <item>Direct3D 12: A pointer to a 'ID3D12PipelineState' object.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkPipeline' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline);

	/// <summary>
	/// A constant buffer resource descriptor.
	/// </summary>
	struct buffer_range
	{
		/// <summary>
		/// Constant buffer resource.
		/// </summary>
		resource buffer = { 0 };
		/// <summary>
		/// Offset from the start of the buffer resource (in bytes).
		/// </summary>
		uint64_t offset = 0;
		/// <summary>
		/// Number of elements this range covers in the buffer resource (in bytes).
		/// Set to -1 (UINT64_MAX) to indicate that the whole buffer should be used.
		/// </summary>
		uint64_t size = UINT64_MAX;
	};

	/// <summary>
	/// A combined sampler and resource view descriptor.
	/// </summary>
	struct sampler_with_resource_view
	{
		/// <summary>
		/// Sampler to sampler the shader resource view with.
		/// </summary>
		sampler sampler = { 0 };
		/// <summary>
		/// Shader resource view.
		/// </summary>
		resource_view view = { 0 };
	};

	/// <summary>
	/// An opaque handle to a descriptor table in a descriptor heap.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: An opaque value.</item>
	/// <item>Direct3D 10: An opaque value.</item>
	/// <item>Direct3D 11: An opaque value.</item>
	/// <item>Direct3D 12: An opaque value.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkDescriptorSet' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_table);

	/// <summary>
	/// All information needed to copy descriptors between descriptor tables.
	/// </summary>
	struct descriptor_table_copy
	{
		/// <summary>
		/// Descriptor table to copy from.
		/// </summary>
		descriptor_table source_table = { 0 };
		/// <summary>
		/// Index of the binding in the source descriptor table.
		/// </summary>
		uint32_t source_binding = 0;
		/// <summary>
		/// Array index in the specified source binding to begin copying from.
		/// </summary>
		uint32_t source_array_offset = 0;
		/// <summary>
		/// Descriptor table to copy to.
		/// </summary>
		descriptor_table dest_table = { 0 };
		/// <summary>
		/// Index of the binding in the destination descriptor table.
		/// </summary>
		uint32_t dest_binding = 0;
		/// <summary>
		/// Array index in the specified destination binding to begin copying to.
		/// </summary>
		uint32_t dest_array_offset = 0;
		/// <summary>
		/// Number of descriptors to copy.
		/// </summary>
		uint32_t count = 0;
	};

	/// <summary>
	/// All information needed to update descriptors in a descriptor table.
	/// </summary>
	struct descriptor_table_update
	{
		/// <summary>
		/// Descriptor table to update.
		/// </summary>
		descriptor_table table = { 0 };
		/// <summary>
		/// OpenGL/Vulkan binding index in the descriptor set.
		/// In D3D this is equivalent to the offset (in descriptors) from the start of the table.
		/// </summary>
		uint32_t binding = 0;
		/// <summary>
		/// Array index in the specified <see cref="binding"/> to begin updating at.
		/// Only meaningful in Vulkan, in OpenGL and other APIs this has to be 0 (since each array element gets a separate binding).
		/// </summary>
		uint32_t array_offset = 0;
		/// <summary>
		/// Number of descriptors to update, starting at the specified <see cref="binding"/>.
		/// If the specified <see cref="binding"/> has fewer than <see cref="count"/> array elements starting from <see cref="array_offset"/>, then the remainder will be used to update the subsequent binding starting at array element zero, recursively.
		/// </summary>
		uint32_t count = 0;
		/// <summary>
		/// Type of the specified <see cref="descriptors"/>.
		/// </summary>
		descriptor_type type = descriptor_type::sampler;
		/// <summary>
		/// Pointer to an array of descriptors to update in the descriptor table (which should be as large as the specified <see cref="count"/>).
		/// Depending on the descriptor <see cref="type"/> this should be pointer to an array of <see cref="buffer_range"/>, <see cref="resource_view"/>, <see cref="sampler"/>, <see cref="sampler_with_resource_view"/> or <see cref="acceleration_structure"/>.
		/// </summary>
		const void *descriptors = nullptr;
	};

	/// <summary>
	/// An opaque handle to a descriptor heap.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: An opaque value.</item>
	/// <item>Direct3D 10: An opaque value.</item>
	/// <item>Direct3D 11: An opaque value.</item>
	/// <item>Direct3D 12: A pointer to a 'ID3D12DescriptorHeap' object.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkDescriptorPool' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_heap);

	/// <summary>
	/// Type of a query.
	/// </summary>
	enum class query_type
	{
		/// <summary>
		/// Number of samples that passed the depth and stencil tests between beginning and end of the query.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		occlusion = 0,
		/// <summary>
		/// Zero if no samples passed, one if at least one sample passed the depth and stencil tests between beginning and end of the query.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		binary_occlusion = 1,
		/// <summary>
		/// GPU timestamp at the frequency returned by <see cref="command_queue::get_timestamp_frequency"/>.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		timestamp = 2,
		/// <summary>
		/// Pipeline statistics (such as the number of shader invocations) between beginning and end of the query.
		/// Data is a structure of type <c>{ uint64_t ia_vertices; uint64_t ia_primitives; uint64_t vs_invocations; uint64_t gs_invocations; uint64_t gs_primitives; uint64_t invocations; uint64_t primitives; uint64_t ps_invocations; uint64_t hs_invocations; uint64_t ds_invocations; uint64_t cs_invocations; }</c>.
		/// </summary>
		pipeline_statistics = 3,
		/// <summary>
		/// Streaming output statistics for stream 0 between beginning and end of the query.
		/// Data is a structure of type <c>{ uint64_t primitives_written; uint64_t primitives_storage_needed; }</c>.
		/// </summary>
		stream_output_statistics_0 = 4,
		stream_output_statistics_1,
		stream_output_statistics_2,
		stream_output_statistics_3,
		/// <summary>
		/// Current size of the acceleration structure.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		/// <seealso cref="command_list::query_acceleration_structures"/>
		acceleration_structure_size = 100,
		/// <summary>
		/// Size of the acceleration structure after compaction.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		/// <seealso cref="command_list::query_acceleration_structures"/>
		acceleration_structure_compacted_size,
		/// <summary>
		/// Size of the serialization data of the acceleration structure.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		/// <seealso cref="command_list::query_acceleration_structures"/>
		acceleration_structure_serialization_size,
		/// <summary>
		/// Number of bottom-level acceleration structure pointers in the acceleration structure.
		/// Data is a 64-bit unsigned integer value.
		/// </summary>
		/// <seealso cref="command_list::query_acceleration_structures"/>
		acceleration_structure_bottom_level_acceleration_structure_pointers
	};

	/// <summary>
	/// An opaque handle to a query heap.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: An opaque value.</item>
	/// <item>Direct3D 10: An opaque value.</item>
	/// <item>Direct3D 11: An opaque value.</item>
	/// <item>Direct3D 12: A pointer to a 'ID3D12QueryHeap' object.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkQueryPool' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(query_heap);

	/// <summary>
	/// A list of all possible render pipeline states that can be set independent of pipeline state objects.
	/// <para>Support for these varies between graphics APIs (e.g. modern APIs like D3D12 and Vulkan support much less dynamic states than D3D9).</para>
	/// </summary>
	enum class dynamic_state
	{
		unknown = 0,

		alpha_test_enable = 15,
		alpha_reference_value = 24,
		alpha_func = 25,
		srgb_write_enable = 194,
		primitive_topology = 1000,
		sample_mask = 162,

		// Blend state

		alpha_to_coverage_enable = 1003,
		blend_enable = 27,
		logic_op_enable = 1004,
		source_color_blend_factor = 19,
		dest_color_blend_factor = 20,
		color_blend_op = 171,
		source_alpha_blend_factor = 207,
		dest_alpha_blend_factor = 208,
		alpha_blend_op = 209,
		blend_constant = 193,
		logic_op = 1005,
		render_target_write_mask = 168,

		// Rasterizer state

		fill_mode = 8,
		cull_mode = 22,
		front_counter_clockwise = 1001,
		depth_bias = 195,
		depth_bias_clamp = 1002,
		depth_bias_slope_scaled = 175,
		depth_clip_enable = 136,
		scissor_enable = 174,
		multisample_enable = 161,
		antialiased_line_enable = 176,

		// Depth-stencil state

		depth_enable = 7,
		depth_write_mask = 14,
		depth_func = 23,
		stencil_enable = 52,
		front_stencil_read_mask = 58,
		front_stencil_write_mask = 59,
		front_stencil_reference_value = 57,
		front_stencil_func = 56,
		front_stencil_pass_op = 55,
		front_stencil_fail_op = 53,
		front_stencil_depth_fail_op = 54,
		back_stencil_read_mask = 1006,
		back_stencil_write_mask = 1007,
		back_stencil_reference_value = 1008,
		back_stencil_func = 189,
		back_stencil_pass_op = 188,
		back_stencil_fail_op = 186,
		back_stencil_depth_fail_op = 187,

		// Ray tracing state

		ray_tracing_pipeline_stack_size = 2000
	};

	/// <summary>
	/// Describes a rectangle.
	/// </summary>
	struct rect
	{
		int32_t left = 0;
		int32_t top = 0;
		int32_t right = 0;
		int32_t bottom = 0;

		constexpr uint32_t width() const { return right - left; }
		constexpr uint32_t height() const { return bottom - top; }
	};

	/// <summary>
	/// Describes a render viewport.
	/// </summary>
	struct viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float min_depth = 0.0f;
		float max_depth = 1.0f;
	};

	/// <summary>
	/// Flags that specify additional parameters of a fence.
	/// </summary>
	enum class fence_flags : uint32_t
	{
		none = 0,
		shared = (1 << 1),
		shared_nt_handle = (1 << 11),
		non_monitored = (1 << 3)
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(fence_flags);

	/// <summary>
	/// An opaque handle to a fence synchronization object.
	/// <para>
	/// Depending on the graphics API this can be:
	/// <list type="bullet">
	/// <item>Direct3D 9: An opaque value.</item>
	/// <item>Direct3D 10: An opaque value.</item>
	/// <item>Direct3D 11: A pointer to a 'ID3D11Fence' object.</item>
	/// <item>Direct3D 12: A pointer to a 'ID3D12Fence' object.</item>
	/// <item>OpenGL: An opaque value.</item>
	/// <item>Vulkan: A 'VkSemaphore' handle.</item>
	/// </list>
	/// </para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(fence);
} }
