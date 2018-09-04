/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "source_location.hpp"
#include <vector>
#include <unordered_map>
#include <spirv.hpp>

namespace reshadefx
{
	struct spv_type
	{
		enum datatype : unsigned int
		{
			datatype_void,
			datatype_bool,
			datatype_int,
			datatype_uint,
			datatype_float,
			datatype_string,
			datatype_struct,
			datatype_sampler,
			datatype_texture,
			datatype_function,
		};
		enum qualifier : unsigned int
		{
			// Storage
			qualifier_extern = 1 << 0,
			qualifier_static = 1 << 1,
			qualifier_uniform = 1 << 2,
			qualifier_volatile = 1 << 3,
			qualifier_precise = 1 << 4,
			qualifier_in = 1 << 5,
			qualifier_out = 1 << 6,
			qualifier_inout = qualifier_in | qualifier_out,

			// Modifier
			qualifier_const = 1 << 8,

			// Interpolation
			qualifier_linear = 1 << 10,
			qualifier_noperspective = 1 << 11,
			qualifier_centroid = 1 << 12,
			qualifier_nointerpolation = 1 << 13,
		};

		static spv_type merge(const spv_type &lhs, const spv_type &rhs);
		static unsigned int rank(const spv_type &src, const spv_type &dst);

		bool has(qualifier qualifier) const { return (qualifiers & qualifier) == qualifier; }
		bool is_array() const { return array_length != 0; }
		bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
		bool is_vector() const { return rows > 1 && cols == 1; }
		bool is_matrix() const { return rows >= 1 && cols > 1; }
		bool is_signed() const { return base == datatype_int || base == datatype_float; }
		bool is_numeric() const { return is_boolean() || is_integral() || is_floating_point(); }
		bool is_void() const { return base == datatype_void; }
		bool is_boolean() const { return base == datatype_bool; }
		bool is_integral() const { return base == datatype_int || base == datatype_uint; }
		bool is_floating_point() const { return base == datatype_float; }
		bool is_struct() const { return base == datatype_struct; }
		bool is_texture() const { return base == datatype_texture; }
		bool is_sampler() const { return base == datatype_sampler; }
		bool is_function() const { return base == datatype_function; }

		unsigned int components() const { return rows * cols; }

		datatype base = datatype_void;
		unsigned int rows : 4;
		unsigned int cols : 4;
		unsigned int qualifiers = 0;
		bool is_pointer = false;
		bool is_input = false;
		bool is_output = false;
		int array_length = 0;
		spv::Id definition = 0;
	};

	struct spv_constant
	{
		union
		{
			float as_float[16];
			int32_t as_int[16];
			uint32_t as_uint[16];
		};
		std::string as_string;
		std::vector<spv_constant> as_array;
	};

	struct spv_expression
	{
		enum op_type
		{
			cast,
			index,
			swizzle
		};
		struct operation
		{
			op_type type;
			spv_type from, to;
			spv::Id index;
			signed char swizzle[4];
		};

		spv::Id base = 0;
		std::vector<operation> ops;
		spv_type type;
		location location;
		spv_constant constant;
		bool is_lvalue = false;
		bool is_constant = false;

		void reset_to_lvalue(spv::Id in_base, const spv_type &in_type, const struct location &loc)
		{
			//assert(in_base != 0);
			//assert(in_type.is_pointer);

			base = in_base;
			type = in_type;
			type.is_pointer = false;
			location = loc;
			ops.clear();
			is_lvalue = true;
			is_constant = false;
		}
		void reset_to_rvalue(spv::Id in_base, const spv_type &in_type, const struct location &loc)
		{
			base = in_base;
			type = in_type;
			type.qualifiers |= spv_type::qualifier_const;
			location = loc;
			ops.clear();
			is_lvalue = false;
			is_constant = false;
		}
		void reset_to_rvalue_constant(const spv_type &in_type, const struct location &loc, uint32_t data)
		{
			type = in_type;
			type.qualifiers |= spv_type::qualifier_const;
			location = loc;
			ops.clear();
			is_lvalue = false;
			is_constant = true;
			constant = {};
			constant.as_uint[0] = data;
		}
		void reset_to_rvalue_constant(const spv_type &in_type, const struct location &loc, std::string &&data)
		{
			type = in_type;
			type.qualifiers |= spv_type::qualifier_const;
			location = loc;
			ops.clear();
			is_lvalue = false;
			is_constant = true;
			constant = {};
			constant.as_string = data;
		}
		void reset_to_rvalue_constant(const spv_type &in_type, const struct location &loc, const spv_constant &data)
		{
			type = in_type;
			type.qualifiers |= spv_type::qualifier_const;
			location = loc;
			ops.clear();
			is_lvalue = false;
			is_constant = true;
			constant = data;
		}
	};

	struct spv_struct_member_info
	{
		spv_type type;
		std::string name;
		spv::BuiltIn builtin = spv::BuiltInMax;
		unsigned int semantic_index = 0;
		std::vector<spv::Decoration> decorations;
	};

	struct spv_struct_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		std::vector<spv_struct_member_info> member_list;
	};

	struct spv_function_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		spv_type return_type;
		spv::BuiltIn return_builtin;
		unsigned int return_semantic_index = 0;
		std::vector<spv_struct_member_info> parameter_list;
		spv::Id entry_point = 0;
	};

	struct spv_variable_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		std::string semantic;
		spv::BuiltIn builtin = spv::BuiltInMax;
		unsigned int semantic_index = 0;
		std::unordered_map<std::string, spv_constant> annotation_list;
		spv::Id texture;
		unsigned int width = 1, height = 1, levels = 1;
		bool srgb_texture;
		unsigned int format = 0;
		unsigned int filter = 0;
		unsigned int address_u = 0;
		unsigned int address_v = 0;
		unsigned int address_w = 0;
		float min_lod, max_lod = FLT_MAX, lod_bias;
	};

	struct spv_pass_info
	{
		spv::Id render_targets[8] = {};
		std::string vs_entry_point, ps_entry_point;
		bool clear_render_targets = true, srgb_write_enable, blend_enable, stencil_enable;
		unsigned char color_write_mask = 0xF, stencil_read_mask = 0xFF, stencil_write_mask = 0xFF;
		unsigned int blend_op = 1, blend_op_alpha = 1, src_blend = 1, dest_blend = 0, src_blend_alpha = 1, dest_blend_alpha = 0;
		unsigned int stencil_comparison_func = 1, stencil_reference_value, stencil_op_pass = 1, stencil_op_fail = 1, stencil_op_depth_fail = 1;
	};

	struct spv_technique_info
	{
		std::string name, unique_name;
		std::unordered_map<std::string, spv_constant> annotation_list;
		std::vector<spv_pass_info> pass_list;
	};
}
