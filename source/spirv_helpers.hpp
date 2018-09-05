/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>

namespace reshadefx
{
	/// <summary>
	/// Structure which keeps track of a code location
	/// </summary>
	struct location
	{
		location() : line(1), column(1) { }
		explicit location(unsigned int line, unsigned int column = 1) : line(line), column(column) { }
		explicit location(const std::string &source, unsigned int line, unsigned int column = 1) : source(source), line(line), column(column) { }

		std::string source;
		unsigned int line, column;
	};

	/// <summary>
	/// Structure which encapsulates a parsed type instance
	/// </summary>
	struct spirv_type
	{
		enum datatype : uint8_t
		{
			t_void,
			t_bool,
			t_int,
			t_uint,
			t_float,
			t_string,
			t_struct,
			t_sampler,
			t_texture,
			t_function,
		};
		enum qualifier : uint32_t
		{
			q_extern = 1 << 0,
			q_static = 1 << 1,
			q_uniform = 1 << 2,
			q_volatile = 1 << 3,
			q_in = 1 << 5,
			q_out = 1 << 6,
			q_inout = q_in | q_out,
			q_const = 1 << 8,
			q_linear = 1 << 10,
			q_noperspective = 1 << 11,
			q_centroid = 1 << 12,
			q_nointerpolation = 1 << 13,
		};

		/// <summary>
		/// Get the result type of an operation involving the two input types.
		/// </summary>
		static spirv_type merge(const spirv_type &lhs, const spirv_type &rhs);

		/// <summary>
		/// Calculate the ranking between two types which can be used to select the best matching function overload. The higher the rank, the better the match.
		/// </summary>
		static unsigned int rank(const spirv_type &src, const spirv_type &dst);

		bool has(qualifier x) const { return (qualifiers & x) == x; }
		bool is_array() const { return array_length != 0; }
		bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
		bool is_vector() const { return rows > 1 && cols == 1; }
		bool is_matrix() const { return rows >= 1 && cols > 1; }
		bool is_signed() const { return base == t_int || base == t_float; }
		bool is_numeric() const { return is_boolean() || is_integral() || is_floating_point(); }
		bool is_void() const { return base == t_void; }
		bool is_boolean() const { return base == t_bool; }
		bool is_integral() const { return base == t_int || base == t_uint; }
		bool is_floating_point() const { return base == t_float; }
		bool is_struct() const { return base == t_struct; }
		bool is_texture() const { return base == t_texture; }
		bool is_sampler() const { return base == t_sampler; }
		bool is_function() const { return base == t_function; }

		unsigned int components() const { return rows * cols; }

		datatype base; // These are initialized in the type parsing routine
		unsigned int rows : 4;
		unsigned int cols : 4;
		unsigned int qualifiers : 24;
		bool is_ptr = false;
		bool is_input = false;
		bool is_output = false;
		int array_length = 0;
		unsigned int definition = 0;
	};

	/// <summary>
	/// Structure which encapsulates a parsed constant value
	/// </summary>
	struct spirv_constant
	{
		union
		{
			float as_float[16];
			int32_t as_int[16];
			uint32_t as_uint[16];
		};

		// Optional string associated with this constant
		std::string string_data;
		// Optional additional elements if this is an array constant
		std::vector<spirv_constant> array_data;
	};

	/// <summary>
	/// Structures which keeps track of the access chain of an expression
	/// </summary>
	struct spirv_expression
	{
		struct operation
		{
			unsigned int type;
			spirv_type from, to;
			uint32_t index;
			signed char swizzle[4];
		};

		spirv_type type = {};
		unsigned int base = 0;
		spirv_constant constant = {};
		bool is_lvalue = false;
		bool is_constant = false;
		location location;
		std::vector<operation> ops;

		void reset_to_lvalue(const struct location &loc, unsigned int in_base, const spirv_type &in_type)
		{
			type = in_type;
			type.is_ptr = false;
			base = in_base;
			location = loc;
			is_lvalue = true;
			is_constant = false;
			ops.clear();
		}
		void reset_to_rvalue(const struct location &loc, unsigned int in_base, const spirv_type &in_type)
		{
			type = in_type;
			type.qualifiers |= spirv_type::q_const;
			base = in_base;
			location = loc;
			is_lvalue = false;
			is_constant = false;
			ops.clear();
		}

		void reset_to_rvalue_constant(const struct location &loc, bool data)
		{
			type = { spirv_type::t_bool, 1, 1, spirv_type::q_const };
			base = 0; constant = {}; constant.as_uint[0] = data;
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
		void reset_to_rvalue_constant(const struct location &loc, float data)
		{
			type = { spirv_type::t_float, 1, 1, spirv_type::q_const };
			base = 0; constant = {}; constant.as_float[0] = data;
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
		void reset_to_rvalue_constant(const struct location &loc, int32_t data)
		{
			type = { spirv_type::t_int,  1, 1, spirv_type::q_const };
			base = 0; constant = {}; constant.as_int[0] = data;
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
		void reset_to_rvalue_constant(const struct location &loc, uint32_t data)
		{
			type = { spirv_type::t_uint, 1, 1, spirv_type::q_const };
			base = 0; constant = {}; constant.as_uint[0] = data;
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
		void reset_to_rvalue_constant(const struct location &loc, std::string data)
		{
			type = { spirv_type::t_string, 0, 0, spirv_type::q_const };
			base = 0; constant = {}; constant.string_data = std::move(data);
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
		void reset_to_rvalue_constant(const struct location &loc, spirv_constant data, const spirv_type &in_type)
		{
			type = in_type;
			type.qualifiers |= spirv_type::q_const;
			base = 0; constant = std::move(data);
			location = loc;
			is_lvalue = false;
			is_constant = true;
			ops.clear();
		}
	};
}
