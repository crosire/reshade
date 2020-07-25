/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_expression.hpp"

namespace reshadefx
{
	/// <summary>
	/// A list of supported image formats.
	/// </summary>
	enum class texture_format
	{
		unknown,

		r8,
		r16f,
		r32f,
		rg8,
		rg16,
		rg16f,
		rg32f,
		rgba8,
		rgba16,
		rgba16f,
		rgba32f,
		rgb10a2,
	};

	/// <summary>
	/// A filtering type used for texture lookups.
	/// </summary>
	enum class texture_filter
	{
		min_mag_mip_point = 0,
		min_mag_point_mip_linear = 0x1,
		min_point_mag_linear_mip_point = 0x4,
		min_point_mag_mip_linear = 0x5,
		min_linear_mag_mip_point = 0x10,
		min_linear_mag_point_mip_linear = 0x11,
		min_mag_linear_mip_point = 0x14,
		min_mag_mip_linear = 0x15
	};

	/// <summary>
	/// Specifies behavior of sampling with texture coordinates outside an image.
	/// </summary>
	enum class texture_address_mode
	{
		wrap = 1,
		mirror = 2,
		clamp = 3,
		border = 4
	};

	/// <summary>
	/// Specifies RGB or alpha blending operations.
	/// </summary>
	enum class pass_blend_op : uint8_t
	{
		add = 1,
		subtract,
		rev_subtract,
		min,
		max,
	};

	/// <summary>
	/// Specifies blend factors, which modulate values between the pixel shader output and render target.
	/// </summary>
	enum class pass_blend_func : uint8_t
	{
		zero = 0,
		one = 1,
		src_color,
		src_alpha,
		inv_src_color,
		inv_src_alpha,
		dst_color,
		dst_alpha,
		inv_dst_color,
		inv_dst_alpha,
	};

	/// <summary>
	/// Specifies the stencil operations that can be performed during depth-stencil testing.
	/// </summary>
	enum class pass_stencil_op : uint8_t
	{
		zero,
		keep,
		invert,
		replace,
		incr,
		incr_sat,
		decr,
		decr_sat,
	};

	/// <summary>
	/// Specifies comparison options for depth-stencil testing.
	/// </summary>
	enum class pass_stencil_func : uint8_t
	{
		never,
		equal,
		not_equal,
		less,
		less_equal,
		greater,
		greater_equal,
		always,
	};

	/// <summary>
	/// Specifies the possible primitives.
	/// </summary>
	enum class primitive_topology : uint8_t
	{
		point_list = 1,
		line_list,
		line_strip,
		triangle_list,
		triangle_strip,
	};

	/// <summary>
	/// A struct type defined in the shader code.
	/// </summary>
	struct struct_info
	{
		std::string name;
		std::string unique_name;
		std::vector<struct struct_member_info> member_list;
		uint32_t definition = 0;
	};

	/// <summary>
	/// A struct field defined in the shader code.
	/// </summary>
	struct struct_member_info
	{
		reshadefx::type type;
		std::string name;
		std::string semantic;
		reshadefx::location location;
		uint32_t definition = 0;
	};

	/// <summary>
	/// An annotation attached to a variable.
	/// </summary>
	struct annotation
	{
		reshadefx::type type;
		std::string name;
		reshadefx::constant value;
	};

	/// <summary>
	/// A texture defined in the shader code.
	/// </summary>
	struct texture_info
	{
		uint32_t id = 0;
		uint32_t binding = 0;
		std::string semantic;
		std::string unique_name;
		std::vector<annotation> annotations;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t levels = 1;
		texture_format format = texture_format::rgba8;
		bool render_target = false;
		bool storage_access = false;
	};

	/// <summary>
	/// A texture sampler defined in the shader code.
	/// </summary>
	struct sampler_info
	{
		uint32_t id = 0;
		uint32_t binding = 0;
		uint32_t texture_binding = 0;
		std::string unique_name;
		std::string texture_name;
		std::vector<annotation> annotations;
		texture_filter filter = texture_filter::min_mag_mip_linear;
		texture_address_mode address_u = texture_address_mode::clamp;
		texture_address_mode address_v = texture_address_mode::clamp;
		texture_address_mode address_w = texture_address_mode::clamp;
		float min_lod = -3.402823466e+38f; // FLT_MAX
		float max_lod = +3.402823466e+38f;
		float lod_bias = 0.0f;
		uint8_t srgb = false;
	};

	/// <summary>
	/// A texture storage object defined in the shader code.
	/// </summary>
	struct storage_info
	{
		uint32_t id = 0;
		uint32_t binding = 0;
		std::string unique_name;
		std::string texture_name;
	};

	/// <summary>
	/// An uniform variable defined in the shader code.
	/// </summary>
	struct uniform_info
	{
		std::string name;
		reshadefx::type type;
		uint32_t size = 0;
		uint32_t offset = 0;
		std::vector<annotation> annotations;
		bool has_initializer_value = false;
		reshadefx::constant initializer_value;
	};

	/// <summary>
	/// Type of a shader.
	/// </summary>
	enum class shader_type
	{
		vs,
		ps,
		cs,
	};

	/// <summary>
	/// A shader entry point function.
	/// </summary>
	struct entry_point
	{
		std::string name;
		shader_type type;
	};

	/// <summary>
	/// A function defined in the shader code.
	/// </summary>
	struct function_info
	{
		uint32_t definition;
		std::string name;
		std::string unique_name;
		reshadefx::type return_type;
		std::string return_semantic;
		std::vector<struct_member_info> parameter_list;
	};

	/// <summary>
	/// A render pass with all its state info.
	/// </summary>
	struct pass_info
	{
		std::string render_target_names[8] = {};
		std::string vs_entry_point;
		std::string ps_entry_point;
		std::string cs_entry_point;
		uint8_t clear_render_targets = false;
		uint8_t srgb_write_enable = false;
		uint8_t blend_enable = false;
		uint8_t stencil_enable = false;
		uint8_t color_write_mask = 0xF;
		uint8_t stencil_read_mask = 0xFF;
		uint8_t stencil_write_mask = 0xFF;
		pass_blend_op blend_op = pass_blend_op::add;
		pass_blend_op blend_op_alpha = pass_blend_op::add;
		pass_blend_func src_blend = pass_blend_func::one;
		pass_blend_func dest_blend = pass_blend_func::zero;
		pass_blend_func src_blend_alpha = pass_blend_func::one;
		pass_blend_func dest_blend_alpha = pass_blend_func::zero;
		pass_stencil_func stencil_comparison_func = pass_stencil_func::always;
		uint32_t stencil_reference_value = 0;
		pass_stencil_op stencil_op_pass = pass_stencil_op::keep;
		pass_stencil_op stencil_op_fail = pass_stencil_op::keep;
		pass_stencil_op stencil_op_depth_fail = pass_stencil_op::keep;
		uint32_t num_vertices = 3;
		primitive_topology topology = primitive_topology::triangle_list;
		uint32_t viewport_width = 0;
		uint32_t viewport_height = 0;
	};

	/// <summary>
	/// A collection of passes that make up an effect.
	/// </summary>
	struct technique_info
	{
		std::string name;
		std::vector<pass_info> passes;
		std::vector<annotation> annotations;
	};

	/// <summary>
	/// In-memory representation of an effect file.
	/// </summary>
	struct module
	{
		std::string hlsl;
		std::vector<uint32_t> spirv;

		std::vector<entry_point> entry_points;
		std::vector<texture_info> textures;
		std::vector<sampler_info> samplers;
		std::vector<storage_info> storages;
		std::vector<uniform_info> uniforms, spec_constants;
		std::vector<technique_info> techniques;

		uint32_t total_uniform_size = 0;
		uint32_t num_texture_bindings = 0;
		uint32_t num_sampler_bindings = 0;
		uint32_t num_storage_bindings = 0;
	};
}
