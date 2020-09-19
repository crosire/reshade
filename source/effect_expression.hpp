/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_token.hpp"

namespace reshadefx
{
	/// <summary>
	/// Structure which encapsulates a parsed value type
	/// </summary>
	struct type
	{
		enum datatype : uint8_t
		{
			t_void,
			t_bool,
			t_min16int,
			t_int,
			t_min16uint,
			t_uint,
			t_min16float,
			t_float,
			t_string,
			t_struct,
			t_sampler,
			t_storage,
			t_texture,
			t_function,
		};
		enum qualifier : uint32_t
		{
			q_extern = 1 << 0,
			q_static = 1 << 1,
			q_uniform = 1 << 2,
			q_volatile = 1 << 3,
			q_precise = 1 << 4,
			q_groupshared = 1 << 14,
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
		static type merge(const type &lhs, const type &rhs);

		/// <summary>
		/// Calculate the ranking between two types which can be used to select the best matching function overload. The higher the rank, the better the match. A value of zero indicates that the types are not compatible.
		/// </summary>
		static unsigned int rank(const type &src, const type &dst);

		/// <summary>
		/// Returns a human-readable description of this type definition.
		/// </summary>
		std::string description() const;

		bool has(qualifier x) const { return (qualifiers & x) == x; }
		bool is_array() const { return array_length != 0; }
		bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
		bool is_vector() const { return rows > 1 && cols == 1; }
		bool is_matrix() const { return rows >= 1 && cols > 1; }
		bool is_signed() const { return base == t_min16int || base == t_int || base == t_min16float || base == t_float; }
		bool is_numeric() const { return base >= t_bool && base <= t_float; }
		bool is_void() const { return base == t_void; }
		bool is_boolean() const { return base == t_bool; }
		bool is_integral() const { return base >= t_bool && base <= t_uint; }
		bool is_floating_point() const { return base == t_min16float || base == t_float; }
		bool is_struct() const { return base == t_struct; }
		bool is_texture() const { return base == t_texture; }
		bool is_sampler() const { return base == t_sampler; }
		bool is_storage() const { return base == t_storage; }
		bool is_function() const { return base == t_function; }

		unsigned int precision() const { return base == t_min16int || base == t_min16uint || base == t_min16float ? 16 : 32; }
		unsigned int components() const { return rows * cols; }

		friend inline bool operator==(const type &lhs, const type &rhs)
		{
			return lhs.base == rhs.base && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition;
		}
		friend inline bool operator!=(const type &lhs, const type &rhs)
		{
			return !operator==(lhs, rhs);
		}

		datatype base = t_void; // Underlying base type ('int', 'float', ...)
		unsigned int rows = 0; // Number of rows if this is a vector type
		unsigned int cols = 0; // Number of columns if this is a matrix type
		unsigned int qualifiers = 0; // Bit mask of all the qualifiers decorating the type
		int array_length = 0; // Negative if an unsized array, otherwise the number of elements if this is an array type
		uint32_t definition = 0; // ID of the matching struct if this is a struct type
	};

	/// <summary>
	/// Structure which encapsulates a parsed constant value
	/// </summary>
	struct constant
	{
		union
		{
			float as_float[16];
			int32_t as_int[16];
			uint32_t as_uint[16];
		};

		// Optional string associated with this constant
		std::string string_data = {};
		// Optional additional elements if this is an array constant
		std::vector<constant> array_data = {};
	};

	/// <summary>
	/// Structures which keeps track of the access chain of an expression
	/// </summary>
	struct expression
	{
		struct operation
		{
			enum op_type
			{
				op_cast,
				op_member,
				op_dynamic_index,
				op_constant_index,
				op_swizzle,
			};

			op_type op;
			reshadefx::type from, to;
			uint32_t index = 0;
			signed char swizzle[4] = {};
		};

		uint32_t base = 0;
		reshadefx::type type = {};
		reshadefx::constant constant = {};
		bool is_lvalue = false;
		bool is_constant = false;
		reshadefx::location location;
		std::vector<operation> chain;

		/// <summary>
		/// Initialize the expression to a l-value.
		/// </summary>
		/// <param name="loc">The code location of the expression.</param>
		/// <param name="base">The SSA ID of the l-value.</param>
		/// <param name="type">The value type of the expression result.</param>
		void reset_to_lvalue(const reshadefx::location &loc, uint32_t base, const reshadefx::type &type);
		/// <summary>
		/// Initialize the expression to a r-value.
		/// </summary>
		/// <param name="loc">The code location of the expression.</param>
		/// <param name="base">The SSA ID of the r-value.</param>
		/// <param name="type">The value type of the expression result.</param>
		void reset_to_rvalue(const reshadefx::location &loc, uint32_t base, const reshadefx::type &type);

		/// <summary>
		/// Initialize the expression to a constant value.
		/// </summary>
		/// <param name="loc">The code location of the constant expression.</param>
		/// <param name="data">The constant value.</param>
		void reset_to_rvalue_constant(const reshadefx::location &loc, bool data);
		void reset_to_rvalue_constant(const reshadefx::location &loc, float data);
		void reset_to_rvalue_constant(const reshadefx::location &loc, int32_t data);
		void reset_to_rvalue_constant(const reshadefx::location &loc, uint32_t data);
		void reset_to_rvalue_constant(const reshadefx::location &loc, std::string data);
		void reset_to_rvalue_constant(const reshadefx::location &loc, reshadefx::constant data, const reshadefx::type &type);

		/// <summary>
		/// Add a cast operation to the current access chain.
		/// </summary>
		/// <param name="type">The type to cast the expression to.</param>
		void add_cast_operation(const reshadefx::type &type);
		/// <summary>
		/// Add a structure member lookup to the current access chain.
		/// </summary>
		/// <param name="index">The index of the member to dereference.</param>
		/// <param name="type">The value type of the member.</param>
		void add_member_access(unsigned int index, const reshadefx::type &type);
		/// <summary>
		/// Add an index operation to the current access chain.
		/// </summary>
		/// <param name="index_expression">The SSA ID of the indexing value.</param>
		void add_dynamic_index_access(uint32_t index_expression);
		/// <summary>
		/// Add an constant index operation to the current access chain.
		/// </summary>
		/// <param name="index">The constant indexing value.</param>
		void add_constant_index_access(unsigned int index);
		/// <summary>
		/// Add a swizzle operation to the current access chain.
		/// </summary>
		/// <param name="swizzle">The swizzle for each component. -1 = unused, 0 = x, 1 = y, 2 = z, 3 = w.</param>
		/// <param name="length">The number of components in the swizzle. The maximum is 4.</param>
		void add_swizzle_access(const signed char swizzle[4], unsigned int length);

		/// <summary>
		/// Apply an unary operation to this constant expression.
		/// </summary>
		/// <param name="op">The unary operator to apply.</param>
		bool evaluate_constant_expression(reshadefx::tokenid op);
		/// <summary>
		/// Apply a binary operation to this constant expression.
		/// </summary>
		/// <param name="op">The binary operator to apply.</param>
		/// <param name="rhs">The constant to use as right-hand side of the binary operation.</param>
		bool evaluate_constant_expression(reshadefx::tokenid op, const reshadefx::constant &rhs);
	};
}
