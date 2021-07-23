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
		all_compute = compute,
		all_graphics = vertex | hull | domain | geometry | pixel,
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(shader_stage);

	/// <summary>
	/// The available shader source formats.
	/// <para>Support for these varies between render APIs (e.g. D3D accepts DXBC, but no GLSL, the reverse of which is true for OpenGL).</para>
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
	/// A list of all possible render pipeline states that can be set dynamically independent of pipeline state objects.
	/// <para>Support for these varies between render APIs (e.g. modern APIs like D3D12 and Vulkan support much less dynamic states than D3D9).</para>
	/// </summary>
	enum class dynamic_state : uint32_t
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
		color_blend_op = 171,
		src_color_blend_factor = 19,
		dst_color_blend_factor = 20,
		alpha_blend_op = 209,
		src_alpha_blend_factor = 207,
		dst_alpha_blend_factor = 208,
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
		back_stencil_depth_fail_op = 187,
	};

	/// <summary>
	/// A list of flags that represent the available pipeline stages in the render pipeline.
	/// </summary>
	enum class pipeline_stage : uint32_t
	{
		/// <summary>
		/// The pipeline stage where vertex and index buffers are consumed.
		/// Corresponds to <see cref="pipeline_desc::graphics::input_layout"/>.
		/// </summary>
		input_assembler = 0x00000004,
		/// <summary>
		/// The vertex shader stage.
		/// Corresponds to <see cref="pipeline_desc::graphics::vertex_shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::vertex"/>
		vertex_shader = 0x00000008,
		/// <summary>
		/// The hull shader stage.
		/// Corresponds to <see cref="pipeline_desc::graphics::hull_shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::hull"/>
		hull_shader = 0x00000010,
		/// <summary>
		/// The domain shader stage.
		/// Corresponds to <see cref="pipeline_desc::graphics::domain_shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::domain"/>
		domain_shader = 0x00000020,
		/// <summary>
		/// The geometry shader stage.
		/// Corresponds to <see cref="pipeline_desc::graphics::geometry_shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::geometry"/>
		geometry_shader = 0x00000040,
		/// <summary>
		/// The pixel shader stage.
		/// Corresponds to <see cref="pipeline_desc::graphics::pixel_shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::pixel"/>
		pixel_shader = 0x00000080,
		/// <summary>
		/// The pipeline stage where rasterization happens and early depth and stencil tests are performed.
		/// Corresponds to <see cref="pipeline_desc::graphics::rasterizer_state"/>.
		/// </summary>
		rasterizer = 0x00000100,
		/// <summary>
		/// The pipeline stage where late depth and stencil tests are performed.
		/// Corresponds to <see cref="pipeline_desc::graphics::depth_stencil_state"/>.
		/// </summary>
		depth_stencil = 0x00000200,
		/// <summary>
		/// The pipeline stage where the final color values are output from the pipeline and written to the render targets.
		/// Corresponds to <see cref="pipeline_desc::graphics::blend_state"/>.
		/// </summary>
		output_merger = 0x00000400,
		/// <summary>
		/// The compute shader stage.
		/// Corresponds to <see cref="pipeline_desc::compute::shader"/>.
		/// </summary>
		/// <seealso cref="shader_stage::compute"/>
		compute_shader = 0x00000800,

		/// <summary>
		/// Full compute pipeline state.
		/// </summary>
		all_compute = compute_shader,
		/// <summary>
		/// Full graphics pipeline state.
		/// </summary>
		all_graphics = 0x00008000,
		/// <summary>
		/// Program pipeline state.
		/// </summary>
		all_shader_stages = vertex_shader | hull_shader | domain_shader | geometry_shader | pixel_shader | compute_shader,
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(pipeline_stage);

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
	/// This is compatible with 'VkCullModeFlags'.
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
	/// This is compatible with 'VkLogicOp'.
	/// </summary>
	enum class logic_op : uint32_t
	{
		clear = 0,
		bitwise_and = 1,
		bitwise_and_reverse = 2,
		bitwise_and_inverted = 4,
		copy = 3,
		copy_inverted = 12,
		noop = 5,
		bitwise_or = 7,
		bitwise_or_reverse = 11,
		bitwise_or_inverted = 13,
		bitwise_xor = 6,
		bitwise_nor = 8,
		equivalent = 9,
		invert = 10,
		bitwise_nand = 14,
		set = 15
	};

	/// <summary>
	/// The available color or alpha blending operations.
	/// This is compatible with 'VkBlendOp'.
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
	/// This is compatible with 'VkBlendFactor'.
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
	/// This is compatible with 'VkStencilOp'.
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
	/// Specifies how the pipeline interprets vertex data that is bound to the vertex input stage and subsequently renders it.
	/// This is compatible with 'D3D_PRIMITIVE_TOPOLOGY'.
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
	/// The available query types.
	/// </summary>
	enum class query_type
	{
		occlusion = 0,
		binary_occlusion = 1,
		timestamp = 2,
		pipeline_statistics = 3
	};

	/// <summary>
	/// The render pass attachment types that can be cleared by <see cref="command_list_impl::clear_attachments"/>.
	/// This is compatible with 'VkImageAspectFlags'.
	/// </summary>
	enum class attachment_type
	{
		color = 0x1,
		depth = 0x2,
		stencil = 0x4
	};
	RESHADE_DEFINE_ENUM_FLAG_OPERATORS(attachment_type);

	/// <summary>
	/// The available descriptor types.
	/// This is mostly compatible with 'VkDescriptorType'.
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
	/// The available indirect command types.
	/// </summary>
	enum class indirect_command
	{
		unknown,
		draw,
		draw_indexed,
		dispatch
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
		/// The D3D10/D3D11/D3D12 constant buffer register index.
		/// </summary>
		uint32_t dx_register_index;
		/// <summary>
		/// The D3D12 register space.
		/// </summary>
		uint32_t dx_register_space;
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
		uint32_t dx_register_index;
		/// <summary>
		/// The D3D12 register space (<c>register(..., spaceX)</c> in HLSL).
		/// </summary>
		uint32_t dx_register_space;
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
	/// An opaque handle to a pipeline state object.
	/// <para>In D3D9, D3D10, D3D11 or D3D12 this is a pointer to a 'IDirect3D(...)Shader', 'ID3D10(...)(Shader/State)', 'ID3D11(...)(Shader/State)' or 'ID3D12PipelineState' object, in Vulkan a 'VkPipeline' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline);

	/// <summary>
	/// An opaque handle to a pipeline layout object.
	/// <para>In D3D12 this is a pointer to a 'ID3D12RootSignature' object, in Vulkan a 'VkPipelineLayout' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(pipeline_layout);

	/// <summary>
	/// An opaque handle to a descriptor set layout object.
	/// <para>In Vulkan this is a 'VkDescriptorSetLayout' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_set_layout);

	/// <summary>
	/// An opaque handle to a query pool.
	/// <para>In D3D12 this is a pointer to a 'ID3D12QueryHeap' object, in Vulkan a 'VkQueryPool' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(query_pool);

	/// <summary>
	/// An opaque handle to a render pass.
	/// <para>In Vulkan this is a 'VkRenderPass' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(render_pass);

	/// <summary>
	/// An opaque handle to a framebuffer object.
	/// <para>In OpenGL this is a FBO handle, in Vulkan a 'VkFramebuffer' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(framebuffer);

	/// <summary>
	/// An opaque handle to a descriptor set.
	/// <para>In D3D12 this is a 'D3D12_GPU_DESCRIPTOR_HANDLE', in Vulkan a 'VkDescriptorSet' handle.</para>
	/// </summary>
	RESHADE_DEFINE_HANDLE(descriptor_set);

	/// <summary>
	/// Describes the color blend state of the output stage.
	/// </summary>
	struct blend_desc
	{
		/// <summary>
		/// Specifies whether to use alpha-to-coverage as a multisampling technique when setting a pixel to a render target.
		/// </summary>
		bool alpha_to_coverage_enable;
		/// <summary>
		/// Specifies whether to enable (or disable) blending for each render target.
		/// </summary>
		bool blend_enable[8];
		/// <summary>
		/// Specifies whether to enable (or disable) a logical operation for each render target.
		/// </summary>
		bool logic_op_enable[8];
		/// <summary>
		/// Defines how to combine the <see cref="src_color_blend_factor"/> and <see cref="dst_color_blend_factor"/> operations.
		/// </summary>
		blend_op color_blend_op[8];
		/// <summary>
		/// Specifies the operation to perform on the RGB value that the pixel shader outputs.
		/// </summary>
		blend_factor src_color_blend_factor[8];
		/// <summary>
		/// Specifies the operation to perform on the current RGB value in the render target.
		/// </summary>
		blend_factor dst_color_blend_factor[8];
		/// <summary>
		/// Defines how to combine the <see cref="src_alpha_blend_factor"/> and <see cref="dst_alpha_blend_factor"/> operations.
		/// </summary>
		blend_op alpha_blend_op[8];
		/// <summary>
		/// Specifies the operation to perform on the alpha value that the pixel shader outputs.
		/// </summary>
		blend_factor src_alpha_blend_factor[8];
		/// <summary>
		/// Specifies the operation to perform on the current alpha value in the render target.
		/// </summary>
		blend_factor dst_alpha_blend_factor[8];
		/// <summary>
		/// Specifies the logical operation to configure for each render target. Ignored if <see cref="logic_op_enable"/> is <c>false</c>.
		/// </summary>
		logic_op logic_op[8];
		/// <summary>
		/// The constant RGBA value used when <see cref="src_color_blend_factor"/> or <see cref="dst_color_blend_factor"/> is <see cref="blend_factor::constant_color"/>.
		/// </summary>
		uint32_t blend_constant;
		/// <summary>
		/// A write mask specifying which color components are written to each render target.
		/// Combination of <c>0x1</c> for red, <c>0x2</c> for green, <c>0x4</c> for blue and <c>0x8</c> for alpha.
		/// </summary>
		uint8_t render_target_write_mask[8];
	};

	/// <summary>
	/// Describes the state of the rasterizer stage.
	/// </summary>
	struct rasterizer_desc
	{
		/// <summary>
		/// Specifies the fill mode to use when rendering.
		/// </summary>
		fill_mode fill_mode;
		/// <summary>
		/// Specifies that triangles facing the specified direction are not drawn.
		/// </summary>
		cull_mode cull_mode;
		/// <summary>
		/// Determines if a triangle is front or back-facing.
		/// </summary>
		bool front_counter_clockwise;
		/// <summary>
		/// Depth value added to a given pixel.
		/// </summary>
		float depth_bias;
		/// <summary>
		/// Maximum depth bias of a pixel.
		/// </summary>
		float depth_bias_clamp;
		/// <summary>
		/// Scalar on the slope of a given pixel.
		/// </summary>
		float slope_scaled_depth_bias;
		/// <summary>
		/// Specifies whether to enable clipping based on distance.
		/// </summary>
		bool depth_clip_enable;
		/// <summary>
		/// Specifies whether to enable scissor rectangle culling.
		/// </summary>
		bool scissor_enable;
		/// <summary>
		/// Specifies whether to use the quadrilateral or alpha line anti-aliasing algorithm on multisample_enable antialiasing render targets.
		/// </summary>
		bool multisample_enable;
		/// <summary>
		/// Specifies whether to enable line antialiasing. Only applies if doing line drawing and <see cref="multisample_enable"/> is <c>false</c>.
		/// </summary>
		bool antialiased_line_enable;
	};

	/// <summary>
	/// Describes the depth-stencil state of the output stage.
	/// </summary>
	struct depth_stencil_desc
	{
		/// <summary>
		/// Specifies whether to enable depth testing.
		/// </summary>
		bool depth_enable;
		/// <summary>
		/// Specifies whether writes to the depth-stencil buffer are enabled.
		/// </summary>
		bool depth_write_mask;
		/// <summary>
		/// Specifies the function that compares depth data against existing depth data.
		/// </summary>
		compare_op depth_func;
		/// <summary>
		/// Specifies whether to enable stencil testing.
		/// </summary>
		bool stencil_enable;
		/// <summary>
		/// A mask applied when reading stencil data from the depth-stencil buffer.
		/// </summary>
		uint8_t stencil_read_mask;
		/// <summary>
		/// A mask applied when writing stencil data to the depth-stencil buffer.
		/// </summary>
		uint8_t stencil_write_mask;
		/// <summary>
		/// Reference value to perform against when doing stencil testing.
		/// </summary>
		uint8_t stencil_reference_value;
		/// <summary>
		/// Specifies the function that compares stencil data against existing stencil data for pixels whose surface normal is facing towards the camera.
		/// </summary>
		compare_op front_stencil_func;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing and depth testing both pass for pixels whose surface normal is facing towards the camera.
		/// </summary>
		stencil_op front_stencil_pass_op;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing fails for pixels whose surface normal is towards the camera.
		/// </summary>
		stencil_op front_stencil_fail_op;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing passes and depth testing fails for pixels whose surface normal is facing towards the camera.
		/// </summary>
		stencil_op front_stencil_depth_fail_op;
		/// <summary>
		/// Specifies the function that compares stencil data against existing stencil data for pixels whose surface normal is facing away from the camera.
		/// </summary>
		compare_op back_stencil_func;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing and depth testing both pass for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_pass_op;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing fails for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_fail_op;
		/// <summary>
		/// Specifies the stencil operation to perform when stencil testing passes and depth testing fails for pixels whose surface normal is facing away from the camera.
		/// </summary>
		stencil_op back_stencil_depth_fail_op;
	};

	/// <summary>
	/// Describes a single element in the the layout of the vertex buffer data for the vertex input stage.
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
		/// Set to zero in case this is unknown.
		/// </summary>
		uint32_t stride;
		/// <summary>
		/// The number of instances to draw using the same per-instance data before advancing by one element (this has to be consistent for all elements per vertex buffer binding).
		/// Set to zero to indicate that this element is per-vertex rather than per-instance.
		/// </summary>
		uint32_t instance_step_rate;
	};

	/// <summary>
	/// Describes a shader module.
	/// </summary>
	struct shader_desc
	{
		/// <summary>
		/// The shader source code.
		/// </summary>
		const void *code;
		/// <summary>
		/// The size (in bytes) of the shader source code.
		/// </summary>
		size_t code_size;
		/// <summary>
		/// The format of the shader source <see cref="code"/>.
		/// </summary>
		shader_format format;
		/// <summary>
		/// Optional entry point name if the shader source code contains multiple entry points. Can be <c>nullptr</c> if it does not.
		/// </summary>
		const char *entry_point;
		/// <summary>
		/// The number of entries in the <see cref="spec_constant_ids"/> and <see cref="spec_constant_values"/> arrays.
		/// This is meaningful only when <see cref="format"/> is <see cref="shader_format::spirv"/> and is ignored otherwise.
		/// </summary>
		uint32_t num_spec_constants;
		/// <summary>
		/// Pointer to an array of specialization constant indices.
		/// </summary>
		const uint32_t *spec_constant_ids;
		/// <summary>
		/// Pointer to an array of constant values, one for each specialization constant index in <see cref="spec_constant_ids"/>.
		/// </summary>
		const uint32_t *spec_constant_values;
	};

	/// <summary>
	/// Describes a pipeline state object.
	/// </summary>
	struct pipeline_desc
	{
		/// <summary>
		/// The type of the pipeline state object.
		/// </summary>
		pipeline_stage type;
		/// <summary>
		/// The descriptor and constant layout of the pipeline.
		/// </summary>
		pipeline_layout layout;

		union
		{
			/// <summary>
			/// Used when pipeline type is <see cref="pipeline_stage::all_compute"/> or a subset of it.
			/// </summary>
			struct
			{
				/// <summary>
				/// The compute shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::compute"/>
				/// <seealso cref="pipeline_stage::compute_shader"/>
				shader_desc shader;
			} compute;

			/// <summary>
			/// Used when pipeline type is <see cref="pipeline_stage::all_graphics"/> or a subset of it.
			/// </summary>
			struct
			{
				/// <summary>
				/// The vertex shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::vertex"/>
				/// <seealso cref="pipeline_stage::vertex_shader"/>
				shader_desc vertex_shader;
				/// <summary>
				/// The optional hull shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::hull"/>
				/// <seealso cref="pipeline_stage::hull_shader"/>
				shader_desc hull_shader;
				/// <summary>
				/// The optional domain shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::domain"/>
				/// <seealso cref="pipeline_stage::domain_shader"/>
				shader_desc domain_shader;
				/// <summary>
				/// The optional geometry shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::geometry"/>
				/// <seealso cref="pipeline_stage::geometry_shader"/>
				shader_desc geometry_shader;
				/// <summary>
				/// The pixel shader module to use.
				/// </summary>
				/// <seealso cref="shader_stage::pixel"/>
				/// <seealso cref="pipeline_stage::pixel_shader"/>
				shader_desc pixel_shader;

				/// <summary>
				/// Describes the layout of the vertex buffer data for the vertex input stage. 
				/// Elements following one with the format set to <see cref="format::unknown"/> will be ignored (which is used to terminate this list).
				/// </summary>
				/// <seealso cref="pipeline_stage::input_assembler"/>
				input_layout_element input_layout[16];

				/// <summary>
				/// Describes the blend state of the output stage.
				/// </summary>
				/// <seealso cref="pipeline_stage::output_merger"/>
				blend_desc blend_state;

				/// <summary>
				/// Describes the state of the rasterizer stage.
				/// </summary>
				/// <seealso cref="pipeline_stage::rasterizer"/>
				rasterizer_desc rasterizer_state;

				/// <summary>
				/// Describes thel state of the depth-stencil stage.
				/// </summary>
				/// <seealso cref="pipeline_stage::depth_stencil"/>
				depth_stencil_desc depth_stencil_state;

				/// <summary>
				/// The sample mask for the blend state.
				/// </summary>
				uint32_t sample_mask;
				/// <summary>
				/// The number of samples per pixel.
				/// </summary>
				uint32_t sample_count;

				/// <summary>
				/// The primitive topology to use when rendering.
				/// </summary>
				primitive_topology topology;

				/// <summary>
				/// The maximum number of viewports that may be bound via <see cref="command_list::bind_viewports"/> with this pipeline.
				/// </summary>
				uint32_t viewport_count;

				/// <summary>
				/// A render pass that describes the format of the render target and depth-stencil views that may be used with this pipeline.
				/// </summary>
				render_pass render_pass_template;

				/// <summary>
				/// A list of all pipeline states that may be dynamically updated via <see cref="command_list::bind_pipeline_states"/>.
				/// Elements following one set to <see cref="dynamic_state::unknown"/> will be ignored (which is used to terminate this list).
				/// </summary>
				dynamic_state dynamic_states[32];
			} graphics;
		};
	};

	/// <summary>
	/// Describes the data structure returned by <see cref="query_type::pipeline_statistics"/> queries.
	/// </summary>
	struct pipeline_statistics
	{
		uint64_t input_assembler_vertices;
		uint64_t input_assembler_primitives;
		uint64_t vertex_shader_invocations;
		uint64_t geometry_shader_invocations;
		uint64_t geometry_shader_primitives;
		uint64_t clipping_invocations;
		uint64_t clipping_primitives;
		uint64_t pixel_shader_invocations;
		uint64_t hull_shader_invocations;
		uint64_t domain_shader_invocations;
		uint64_t compute_shader_invocations;
	};

	/// <summary>
	/// Describes a pipeline layout.
	/// </summary>
	struct pipeline_layout_desc
	{
		uint32_t num_set_layouts;
		const descriptor_set_layout *set_layouts;
		uint32_t num_constant_ranges;
		const constant_range *constant_ranges;
	};

	/// <summary>
	/// Describes a descriptor set layout.
	/// </summary>
	struct descriptor_set_layout_desc
	{
		uint32_t num_ranges;
		const descriptor_range *ranges;
		bool push_descriptors;
	};

	/// <summary>
	/// Describes a render pass.
	/// </summary>
	struct render_pass_desc
	{
		format depth_stencil_format;
		format render_targets_format[8];
		uint16_t samples;
	};

	/// <summary>
	/// Describes a framebuffer object.
	/// </summary>
	struct framebuffer_desc
	{
		render_pass render_pass_template;
		resource_view depth_stencil;
		resource_view render_targets[8];
	};

	/// <summary>
	/// All information needed to copy descriptors between descriptor sets.
	/// </summary>
	struct copy_descriptor_set
	{
		descriptor_set src_set;
		uint32_t src_binding;
		uint32_t src_array_offset;
		descriptor_set dst_set;
		uint32_t dst_binding;
		uint32_t dst_array_offset;
		uint32_t count;
		descriptor_type type;
	};

	/// <summary>
	/// All information needed to update descriptors in a single descriptor set.
	/// </summary>
	struct write_descriptor_set
	{
		descriptor_set set;
		uint32_t binding;
		uint32_t array_offset;
		descriptor_type type;

		struct
		{
			sampler sampler;
			resource_view view;
			resource resource;
			uint64_t offset;
			uint64_t size;
		} descriptor;
	};

	/// <summary>
	/// A combined sampler and resource view descriptor.
	/// </summary>
	struct sampler_with_resource_view
	{
		sampler sampler;
		resource_view view;
	};
} }
