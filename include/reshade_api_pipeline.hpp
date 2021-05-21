/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_resource.hpp"

namespace reshade { namespace api
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
		all_graphics = 0x1F
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(shader_stage);

	/// <summary>
	/// The available shader source formats.
	/// Support for these varies between render APIs (e.g. D3D generally accepts DXBC, but no GLSL, the reverse of which is true for OpenGL).
	/// </summary>
	enum class shader_format : uint32_t
	{
		/// <summary>
		/// DirectX Bytecode
		/// </summary>
		dxbc,
		/// <summary>
		/// DirectX Intermediate Language
		/// </summary>
		/// <remarks>https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst</remarks>
		dxil,
		/// <summary>
		/// SPIR-V
		/// </summary>
		/// <remarks>https://www.khronos.org/spir/</remarks>
		spirv,
		/// <summary>
		/// High-level shader language
		/// </summary>
		/// <remarks>https://docs.microsoft.com/windows/win32/direct3dhlsl</remarks>
		hlsl,
		/// <summary>
		/// OpenGL Shading Language
		/// </summary>
		/// <remarks>https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language</remarks>
		glsl
	};

	/// <summary>
	/// The available pipeline state object types.
	/// Support for these varies between render APIs (e.g. modern APIs like D3D12 and Vulkan do not support the partial pipeline types).
	/// </summary>
	enum class pipeline_type : uint32_t
	{
		unknown = 0,

		/// <summary>
		/// Full compute pipeline state
		/// </summary>
		compute,
		/// <summary>
		/// Full graphics pipeline state
		/// </summary>
		graphics,

		/// <summary>
		/// Partial pipeline state which binds all shader modules
		/// </summary>
		graphics_shaders,
		/// <summary>
		/// Partial pipeline state which only binds a vertex shader module
		/// </summary>
		graphics_vertex_shader,
		/// <summary>
		/// Partial pipeline state which only binds a hull shader module
		/// </summary>
		graphics_hull_shader,
		/// <summary>
		/// Partial pipeline state which only binds a domain shader module
		/// </summary>
		graphics_domain_shader,
		/// <summary>
		/// Partial pipeline state which only binds a geometry shader module
		/// </summary>
		graphics_geometry_shader,
		/// <summary>
		/// Partial pipeline state which only binds a pixel shader module
		/// </summary>
		graphics_pixel_shader,
		/// <summary>
		/// Partial pipeline state which only binds the input layout
		/// </summary>
		graphics_input_layout,
		/// <summary>
		/// Partial pipeline state which only binds blend state
		/// </summary>
		graphics_blend_state,
		/// <summary>
		/// Partial pipeline state which only binds rasterizer state
		/// </summary>
		graphics_rasterizer_state,
		/// <summary>
		/// Partial pipeline state which only binds depth-stencil state
		/// </summary>
		graphics_depth_stencil_state
	};

	/// <summary>
	/// A list of all possible dynamic render pipeline states that can be set.
	/// Mostly compatible with 'D3DRENDERSTATETYPE'.
	/// <para>Support for these varies between render APIs (e.g. modern APIs like D3D12 and Vulkan typically only support a selected few dynamic states).</para>
	/// </summary>
	enum class pipeline_state : uint32_t
	{
		unknown = 0,

		alpha_test = 15,
		alpha_reference_value = 24,
		alpha_func = 25,
		srgb_write = 194,

		// Blend state

		blend_enable = 27,
		blend_constant = 193,
		src_color_blend_factor = 19,
		dst_color_blend_factor = 20,
		color_blend_op = 171,
		src_alpha_blend_factor = 207,
		dst_alpha_blend_factor = 208,
		alpha_blend_op = 209,
		render_target_write_mask = 168,

		// Rasterizer state

		fill_mode = 8,
		cull_mode = 22,
		primitive_topology = 1000,
		front_face_ccw = 1001,
		depth_bias = 195,
		depth_bias_clamp = 1002,
		depth_bias_slope_scaled = 175,
		depth_clip = 136,
		scissor_test = 174,
		antialiased_line = 176,

		// Multisample state

		multisample = 161,
		alpha_to_coverage = 1003,
		sample_mask = 162,

		// Depth-stencil state

		depth_test = 7,
		depth_write_mask = 14,
		depth_func = 23,
		stencil_test = 52,
		stencil_read_mask = 58,
		stencil_write_mask = 59,
		stencil_reference_value = 57,
		back_stencil_fail = 186,
		back_stencil_depth_fail = 187,
		back_stencil_pass = 188,
		back_stencil_func = 189,
		front_stencil_fail = 53,
		front_stencil_depth_fail = 54,
		front_stencil_pass = 55,
		front_stencil_func = 56
	};

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
	/// Compatible with 'VkCullModeFlags'.
	/// </summary>
	enum class cull_mode : uint32_t
	{
		none = 0,
		front = 1,
		back = 2,
		front_and_back = 3
	};

	/// <summary>
	/// The available color or alpha blending operations.
	/// Compatible with 'VkBlendOp'.
	/// </summary>
	enum class blend_op : uint32_t
	{
		add = 0,
		subtract = 1,
		rev_subtract = 2,
		min = 3,
		max = 4
	};

	/// <summary>
	/// The available blend factors used in blending operations.
	/// Compatible with 'VkBlendFactor'.
	/// </summary>
	enum class blend_factor : uint32_t
	{
		zero = 0,
		one = 1,
		src_color = 2,
		inv_src_color = 3,
		dst_color = 4,
		inv_dst_color = 5,
		src_alpha = 6,
		inv_src_alpha = 7,
		dst_alpha = 8,
		inv_dst_alpha = 9,
		constant_color = 10,
		inv_constant_color = 11,
		constant_alpha = 12,
		inv_constant_alpha = 13,
		src_alpha_sat = 14,
		src1_color = 15,
		inv_src1_color = 16,
		src1_alpha = 17,
		inv_src1_alpha = 18
	};

	/// <summary>
	/// The available stencil operations that can be performed during depth-stencil testing.
	/// Compatible with 'VkStencilOp'.
	/// </summary>
	enum class stencil_op : uint32_t
	{
		keep = 0,
		zero = 1,
		replace = 2,
		incr_sat = 3,
		decr_sat = 4,
		invert = 5,
		incr = 6,
		decr = 7
	};

	/// <summary>
	/// The available comparion types.
	/// Compatible with 'VkCompareOp'.
	/// </summary>
	enum class compare_op : uint32_t
	{
		never = 0,
		less = 1,
		equal = 2,
		less_equal = 3,
		greater = 4,
		not_equal = 5,
		greater_equal = 6,
		always = 7
	};

	/// <summary>
	/// Specifies how the pipeline interprets vertex data that is bound to the input-assembler stage and subsequently renders it.
	/// Compatible with 'D3D_PRIMITIVE_TOPOLOGY'.
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
	/// Describes a single element in the the layout of the vertex buffer data for the input-assembler stage.
	/// </summary>
	struct input_layout_element
	{
		/// <summary>
		/// The GLSL attribute location associated with this element (<c>layout(location = X)</c>).
		/// </summary>
		uint32_t location;
		/// <summary>
		/// The HLSL semantic associated with this element.
		/// </summary>
		const char *semantic;
		/// <summary>
		/// Optional index for the HLSL semantic (for "TEXCOORD1" set <see cref="semantic"/> to "TEXCOORD" and <see cref="semantic_index"/> to 1).
		/// </summary>
		uint32_t semantic_index;
		/// <summary>
		/// The format of the element data.
		/// </summary>
		format format;
		/// <summary>
		/// The input slot (index of the vertex buffer binding).
		/// </summary>
		uint32_t buffer_binding;
		/// <summary>
		/// Offset (in bytes) from the start of the vertex to this element.
		/// </summary>
		uint32_t offset;
		/// <summary>
		/// Stride of the entire vertex (this has to be consistent for all elements per vertex buffer binding).
		/// </summary>
		uint32_t stride;
		/// <summary>
		/// The number of instances to draw using the same per-instance data before advancing by one element (this has to be consistent for all elements per vertex buffer binding).
		/// Set to zero to indicate that this element is per-vertex rather than per-instance.
		/// </summary>
		uint32_t instance_step_rate;
	};

	/// <summary>
	/// The available query types.
	/// Mostly compatible with 'D3D12_QUERY_TYPE'.
	/// </summary>
	enum class query_type
	{
		occlusion = 0,
		binary_occlusion = 1,
		timestamp = 2,
		pipeline_statistics = 3
	};

	/// <summary>
	/// The available descriptor types.
	/// Mostly compatible with 'VkDescriptorType'.
	/// </summary>
	enum class descriptor_type
	{
		sampler = 0,
		sampler_with_resource_view = 1,
		shader_resource_view = 2,
		unordered_access_view = 3,
		constant_buffer = 6
	};

	/// <summary>
	/// Specifies a range of constants in a pipeline layout.
	/// </summary>
	struct constant_range
	{
		/// <summary>
		/// The push constant offset (in 32-bit values, only used by Vulkan).
		/// </summary>
		uint32_t offset;
		/// <summary>
		/// D3D10/D3D11/D3D12 constant buffer register index.
		/// </summary>
		uint32_t dx_shader_register;
		/// <summary>
		/// The numer of constants in this range (in 32-bit values).
		/// </summary>
		uint32_t count;
		/// <summary>
		/// The shader pipeline stages that make use of the constants in this range.
		/// </summary>
		shader_stage visibility;
	};

	/// <summary>
	/// Specifies a range of descriptors in a descriptor set layout.
	/// </summary>
	struct descriptor_range
	{
		/// <summary>
		/// The index in the descriptor set (<c>layout(binding=X)</c> in GLSL).
		/// </summary>
		uint32_t binding;
		/// <summary>
		/// The D3D9/D3D10/D3D11/D3D12 shader register index (<c>register(xX)</c> in HLSL).
		/// </summary>
		uint32_t dx_shader_register;
		/// <summary>
		/// The type of the descriptors in this range.
		/// </summary>
		descriptor_type type;
		/// <summary>
		/// The number of descriptors in the set starting from the binding index.
		/// </summary>
		uint32_t count;
		/// <summary>
		/// The shader pipeline stages that make use of the descriptors in this range.
		/// </summary>
		shader_stage visibility;
	};

	/// <summary>
	/// A combined sampler and resource view descriptor.
	/// </summary>
	struct sampler_with_resource_view
	{
		sampler sampler;
		resource_view view;
	};

	/// <summary>
	/// An opaque handle to a pipeline state object.
	/// <para>Depending on the render API this is really a pointer to a 'ID3D12PipelineState' object or a 'VkPipeline' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline);

	/// <summary>
	/// An opaque handle to a shader module.
	/// <para>Depending on the render API this is really a pointer to a 'IDirect3D(...)Shader9', 'ID3D10(...)Shader', 'ID3D11(...)Shader' or a 'VkShaderModule'.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(shader_module);

	/// <summary>
	/// An opaque handle to a pipeline layout object.
	/// <para>Depending on the render API this is really a pointer to a 'ID3D12RootSignature' object or a 'VkPipelineLayout' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline_layout);

	/// <summary>
	/// An opaque handle to a descriptor set layout object.
	/// <para>Depending on the render API this is really a 'VkDescriptorSetLayout' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_set_layout);

	/// <summary>
	/// An opaque handle to a query pool.
	/// <para>Depending on the render API this is really a pointer to a 'ID3D12QueryHeap' or a 'VkQueryPool' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(query_pool);

	/// <summary>
	/// An opaque handle to a descriptor set.
	/// <para>Depending on the render API this is really a 'D3D12_GPU_DESCRIPTOR_HANDLE' or a 'VkDescriptorSet' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_set);

	/// <summary>
	/// Describes a pipeline state object.
	/// </summary>
	struct pipeline_desc
	{
		/// <summary>
		/// The type of the pipeline state object.
		/// </summary>
		pipeline_type type;
		/// <summary>
		/// The descriptor and constant layout of the pipeline.
		/// </summary>
		pipeline_layout layout;

		union
		{
			// Used when pipeline type is <see cref="pipeline_type::compute"/>.
			struct
			{
				/// <summary>
				/// The compute shader module (of type <see cref="shader_stage::compute"/>) to use.
				/// </summary>
				shader_module shader;
			} compute;
			// Used when pipeline type is <see cref="pipeline_type::graphics"/> or any other graphics type.
			struct
			{
				/// <summary>
				/// The vertex shader module (of type <see cref="shader_stage::vertex"/>) to use.
				/// </summary>
				shader_module vertex_shader;
				/// <summary>
				/// The optional hull shader module (of type <see cref="shader_stage::hull"/>) to use, or zero.
				/// </summary>
				shader_module hull_shader;
				/// <summary>
				/// The optional domain shader module (of type <see cref="shader_stage::domain"/>) to use, or zero.
				/// </summary>
				shader_module domain_shader;
				/// <summary>
				/// The optional geometry shader module (of type <see cref="shader_stage::geometry"/>) to use, or zero.
				/// </summary>
				shader_module geometry_shader;
				/// <summary>
				/// The pixel shader module (of type <see cref="shader_stage::pixel"/>) to use.
				/// </summary>
				shader_module pixel_shader;

				/// <summary>
				/// Describes the layout of the vertex buffer data for the input-assembler stage. 
				/// Elements following one with the format set to <see cref="format::unknown"/> will be ignored (which can be used to terminate this list).
				/// </summary>
				input_layout_element input_layout[16];

				// Describes the blend state of the output-merger stage.
				struct
				{
					uint32_t num_viewports;
					uint32_t num_render_targets;
					format render_target_format[8];
					bool blend_enable[8];
					uint32_t blend_constant;
					blend_factor src_color_blend_factor[8];
					blend_factor dst_color_blend_factor[8];
					blend_op color_blend_op[8];
					blend_factor src_alpha_blend_factor[8];
					blend_factor dst_alpha_blend_factor[8];
					blend_op alpha_blend_op[8];
					uint8_t render_target_write_mask[8];
				} blend_state;

				// Describes the rasterizer state of the rasterizer stage.
				struct
				{
					fill_mode fill_mode;
					cull_mode cull_mode;
					primitive_topology topology;
					bool front_counter_clockwise;
					float depth_bias;
					float depth_bias_clamp;
					float slope_scaled_depth_bias;
					bool depth_clip;
					bool scissor_test;
					bool antialiased_line;
				} rasterizer_state;

				// Describes the multisampling state.
				struct
				{
					bool multisample;
					bool alpha_to_coverage;
					uint32_t sample_mask;
					uint32_t sample_count;
				} multisample_state;

				// Describes the depth-stencil state of the output-merger stage.
				struct
				{
					format depth_stencil_format;
					bool depth_test;
					bool depth_write_mask;
					compare_op depth_func;
					bool stencil_test;
					uint8_t stencil_read_mask;
					uint8_t stencil_write_mask;
					uint8_t stencil_reference_value;
					stencil_op back_stencil_fail_op;
					stencil_op back_stencil_depth_fail_op;
					stencil_op back_stencil_pass_op;
					compare_op back_stencil_func;
					stencil_op front_stencil_fail_op;
					stencil_op front_stencil_depth_fail_op;
					stencil_op front_stencil_pass_op;
					compare_op front_stencil_func;
				} depth_stencil_state;

				// A list of all pipeline states that are dynamically set via "command_list::set_pipeline_states".
				uint32_t num_dynamic_states;
				const pipeline_state *dynamic_states;
			} graphics;
		};
	};

	/// <summary>
	/// All information needed to update a single descriptor in a descriptor set.
	/// </summary>
	struct descriptor_update
	{
		descriptor_set set;
		uint32_t binding;
		descriptor_type type;

		struct : public sampler_with_resource_view
		{
			resource resource;
		} descriptor;
	};
} }
