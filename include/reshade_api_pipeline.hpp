/*
 * Copyright (C) 2021 Patrick Mours
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "reshade_api_format.hpp"

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

	/// <summary>
	/// A list of all possible dynamic render pipeline states that can be set. Support for these varies between render APIs.
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
		primitive_topology = 900,
		front_face_ccw = 908,
		depth_bias = 195,
		depth_bias_clamp = 909,
		depth_bias_slope_scaled = 175,
		depth_clip = 136,
		scissor_test = 174,
		antialiased_line = 176,

		// Multisample state

		multisample = 161,
		alpha_to_coverage = 907,
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
		front_stencil_func = 56,
	};

	/// <summary>
	/// An opaque handle to a pipeline layout object.
	/// <para>Depending on the render API this is really a pointer to a 'ID3D12RootSignature' or a 'VkPipelineLayout' handle.</para>
	/// </summary>
	typedef struct { uint64_t handle; } pipeline_layout;
} }
