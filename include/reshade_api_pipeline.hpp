/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api_resource.hpp"

namespace reshade::api
{
	/// <summary>
	/// A list of flags that represent the available shader stages in the render pipeline.
	/// </summary>
	enum class shader_stage : uint32_t
	{
		vertex = 0x1,
		hull = 0x2,
		domain = 0x4,
		geometry = 0x8,
		pixel = 0x10,
		compute = 0x20,

		all = 0x7FFFFFFF,
		all_compute = compute,
		all_graphics = vertex | hull | domain | geometry | pixel
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(shader_stage);

	/// <summary>
	/// A list of flags that represent the available pipeline stages in the render pipeline.
	/// </summary>
	enum class pipeline_stage : uint32_t
	{
		vertex_shader = 0x8,
		hull_shader = 0x10,
		domain_shader = 0x20,
		geometry_shader = 0x40,
		pixel_shader = 0x80,
		compute_shader = 0x800,

		input_assembler = 0x2,
		stream_output = 0x4,
		rasterizer = 0x100,
		depth_stencil = 0x200,
		output_merger = 0x400,

		all = 0x7FFFFFFF,
		all_compute = compute_shader,
		all_graphics = vertex_shader | hull_shader | domain_shader | geometry_shader | pixel_shader | input_assembler | stream_output | rasterizer | depth_stencil | output_merger,
		all_shader_stages = vertex_shader | hull_shader | domain_shader | geometry_shader | pixel_shader | compute_shader
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(pipeline_stage);

	/// <summary>
	/// The available descriptor types.
	/// </summary>
	enum class descriptor_type : uint32_t
	{
		sampler = 0,
		sampler_with_resource_view = 1,
		shader_resource_view = 2,
		unordered_access_view = 3,
		constant_buffer = 6,
		shader_storage_buffer = 7
	};

	/// <summary>
	/// The available pipeline layout parameter types.
	/// </summary>
	enum class pipeline_layout_param_type : uint32_t
	{
		push_constants = 1,
		push_descriptors = 2,
		descriptor_set = 0
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
	/// Describes a range of descriptors in a descriptor set layout.
	/// </summary>
	struct descriptor_range
	{
		/// <summary>
		/// OpenGL/Vulkan binding index (<c>layout(binding=X)</c> in GLSL).
		/// In D3D this is equivalent to the offset (in descriptors) of this range in the descriptor set (since each binding can only have an array size of 1).
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

	/// <summary>
	/// Describes a single parameter in a pipeline layout.
	/// </summary>
	struct pipeline_layout_param
	{
		constexpr pipeline_layout_param() : push_descriptors() {}
		constexpr pipeline_layout_param(const constant_range &push_constants) : type(pipeline_layout_param_type::push_constants), push_constants(push_constants) {}
		constexpr pipeline_layout_param(const descriptor_range &push_descriptors) : type(pipeline_layout_param_type::push_descriptors), push_descriptors(push_descriptors) {}
		constexpr pipeline_layout_param(uint32_t count, const descriptor_range *ranges) : type(pipeline_layout_param_type::descriptor_set), descriptor_set({ count, ranges }) {}

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
			/// Used when parameter type is <see cref="pipeline_layout_param_type::descriptor_set"/>.
			/// </summary>
			struct
			{
				uint32_t count;
				const descriptor_range *ranges;
			} descriptor_set;
		};
	};

	/// <summary>
	/// An opaque handle to a pipeline layout object.
	/// <para>In D3D12 this is a pointer to a 'ID3D12RootSignature' object, in Vulkan a 'VkPipelineLayout' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline_layout);

	/// <summary>
	/// The fill mode to use when rendering triangles.
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
	/// The available logic operations.
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
	/// The available color or alpha blending operations.
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
	/// The available blend factors in color or alpha blending operations.
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
	/// The available stencil operations that can be performed during depth-stencil testing.
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
		/// Mask applied to stencil values read from the depth-stencil buffer.
		/// </summary>
		uint8_t stencil_read_mask = 0xFF;
		/// <summary>
		/// Mask applied to stencil values written to the depth-stencil buffer.
		/// </summary>
		uint8_t stencil_write_mask = 0xFF;
		/// <summary>
		/// Reference value to perform against when doing stencil testing.
		/// </summary>
		uint8_t stencil_reference_value = 0;
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
	/// The available pipeline sub-object types.
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
		max_vertex_count
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
	/// <para>In D3D9, D3D10, D3D11 or D3D12 this is a pointer to a 'IDirect3D(...)Shader', 'ID3D10(...)(Shader/State)', 'ID3D11(...)(Shader/State)' or 'ID3D12PipelineState' object, in Vulkan a 'VkPipeline' handle.</para>
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
	/// An opaque handle to a descriptor set.
	/// <para>In Vulkan this is a 'VkDescriptorSet' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_set);

	/// <summary>
	/// All information needed to copy descriptors between descriptor sets.
	/// </summary>
	struct descriptor_set_copy
	{
		/// <summary>
		/// Descriptor set to copy from.
		/// </summary>
		descriptor_set source_set = { 0 };
		/// <summary>
		/// Index of the binding in the source descriptor set.
		/// </summary>
		uint32_t source_binding = 0;
		/// <summary>
		/// Array index in the specified source binding to begin copying from.
		/// </summary>
		uint32_t source_array_offset = 0;
		/// <summary>
		/// Descriptor set to copy to.
		/// </summary>
		descriptor_set dest_set = { 0 };
		/// <summary>
		/// Index of the binding in the destination descriptor set.
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
	/// All information needed to update descriptors in a descriptor set.
	/// </summary>
	struct descriptor_set_update
	{
		/// <summary>
		/// Descriptor set to update.
		/// </summary>
		descriptor_set set = { 0 };
		/// <summary>
		/// OpenGL/Vulkan binding index in the descriptor set.
		/// In D3D this is equivalent to the offset (in descriptors) from the start of the set.
		/// </summary>
		uint32_t binding = 0;
		/// <summary>
		/// Array index in the specified <see cref="binding"/> to begin updating at.
		/// Only meaningful in Vulkan, in OpenGL and other APIs this has to be 0 (since each GLSL array element gets a separate binding index).
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
		/// Pointer to an array of descriptors to update in the set (which should be as large as the specified <see cref="count"/>).
		/// Depending on the descriptor <see cref="type"/> this should be pointer to an array of <see cref="buffer_range"/>, <see cref="resource_view"/>, <see cref="sampler"/> or <see cref="sampler_with_resource_view"/>.
		/// </summary>
		const void *descriptors = nullptr;
	};

	/// <summary>
	/// An opaque handle to a descriptor pool.
	/// <para>In D3D12 this is a pointer to a 'ID3D12DescriptorHeap' object, in Vulkan a 'VkDescriptorPool' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_pool);

	/// <summary>
	/// The available query types.
	/// </summary>
	enum class query_type
	{
		occlusion = 0,
		binary_occlusion = 1,
		timestamp = 2,
		pipeline_statistics = 3,
		stream_output_statistics_0 = 4,
		stream_output_statistics_1,
		stream_output_statistics_2,
		stream_output_statistics_3
	};

	/// <summary>
	/// An opaque handle to a query pool.
	/// <para>In D3D12 this is a pointer to a 'ID3D12QueryHeap' object, in Vulkan a 'VkQueryPool' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(query_pool);

	/// <summary>
	/// A list of all possible render pipeline states that can be set independent of pipeline state objects.
	/// <para>Support for these varies between render APIs (e.g. modern APIs like D3D12 and Vulkan support much less dynamic states than D3D9).</para>
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
		logic_op = 1005,
		blend_constant = 193,
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
		stencil_read_mask = 58,
		stencil_write_mask = 59,
		stencil_reference_value = 57,
		front_stencil_func = 56,
		front_stencil_pass_op = 55,
		front_stencil_fail_op = 53,
		front_stencil_depth_fail_op = 54,
		back_stencil_func = 189,
		back_stencil_pass_op = 188,
		back_stencil_fail_op = 186,
		back_stencil_depth_fail_op = 187
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
}
