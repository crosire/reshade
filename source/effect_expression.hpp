/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

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
	struct type
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
			q_precise = 1 << 4,
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
		/// Calculate the ranking between two types which can be used to select the best matching function overload. The higher the rank, the better the match.
		/// </summary>
		static unsigned int rank(const type &src, const type &dst);

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
		std::string string_data;
		// Optional additional elements if this is an array constant
		std::vector<constant> array_data;
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
				op_index,
				op_swizzle,
				op_member,
			};

			op_type type;
			struct type from, to;
			uint32_t index;
			signed char swizzle[4];
		};

		struct type type = {};
		uint32_t base = 0;
		constant constant = {};
		bool is_lvalue = false;
		bool is_constant = false;
		location location;
		std::vector<operation> ops;

		void reset_to_lvalue(const struct location &loc, uint32_t in_base, const struct type &in_type);
		void reset_to_rvalue(const struct location &loc, uint32_t in_base, const struct type &in_type);

		void reset_to_rvalue_constant(const struct location &loc, bool data);
		void reset_to_rvalue_constant(const struct location &loc, float data);
		void reset_to_rvalue_constant(const struct location &loc, int32_t data);
		void reset_to_rvalue_constant(const struct location &loc, uint32_t data);
		void reset_to_rvalue_constant(const struct location &loc, std::string data);
		void reset_to_rvalue_constant(const struct location &loc, struct constant data, const struct type &in_type);

		void add_cast_operation(const reshadefx::type &type);
		void add_member_access(unsigned int index, const reshadefx::type &type);
		void add_static_index_access(class codegen *codegen, unsigned int index);
		void add_dynamic_index_access(class codegen *codegen, uint32_t index_expression);
		void add_swizzle_access(const signed char swizzle[4], unsigned int length);

		void evaluate_constant_expression(enum class tokenid op);
		void evaluate_constant_expression(enum class tokenid op, const reshadefx::constant &rhs);
	};


	struct struct_info
	{
		std::string name;
		std::string unique_name;
		std::vector<struct struct_member_info> member_list;
		uint32_t definition = 0;
	};

	struct struct_member_info
	{
		type type;
		std::string name;
		std::string semantic;
		location location;
		uint32_t definition = 0;
	};

	struct uniform_info
	{
		std::string name;
		type type;
		uint32_t member_index = 0;
		uint32_t size = 0;
		uint32_t offset = 0;
		std::unordered_map<std::string, std::pair<struct type, constant>> annotations;
		bool has_initializer_value = false;
		constant initializer_value;
	};

	struct texture_info
	{
		uint32_t id = 0;
		std::string semantic;
		std::string unique_name;
		std::unordered_map<std::string, std::pair<type, constant>> annotations;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t levels = 1;
		uint32_t format = 8; // RGBA8
	};

	struct sampler_info
	{
		uint32_t id = 0;
		uint32_t set = 0;
		uint32_t binding = 0;
		std::string unique_name;
		std::string texture_name;
		std::unordered_map<std::string, std::pair<type, constant>> annotations;
		uint32_t filter = 0x1; // LINEAR
		uint32_t address_u = 3; // CLAMP
		uint32_t address_v = 3;
		uint32_t address_w = 3;
		float min_lod = -FLT_MAX;
		float max_lod = +FLT_MAX;
		float lod_bias = 0.0f;
		uint8_t srgb = false;
	};

	struct pass_info
	{
		std::string render_target_names[8] = {};
		std::string vs_entry_point;
		std::string ps_entry_point;
		uint8_t clear_render_targets = true;
		uint8_t srgb_write_enable = false;
		uint8_t blend_enable = false;
		uint8_t stencil_enable = false;
		uint8_t color_write_mask = 0xF;
		uint8_t stencil_read_mask = 0xFF;
		uint8_t stencil_write_mask = 0xFF;
		uint32_t blend_op = 1; // ADD
		uint32_t blend_op_alpha = 1; // ADD
		uint32_t src_blend = 1; // ONE
		uint32_t dest_blend = 0; // ZERO
		uint32_t src_blend_alpha = 1; // ONE
		uint32_t dest_blend_alpha = 0; // ZERO
		uint32_t stencil_comparison_func = 8; // ALWAYS
		uint32_t stencil_reference_value = 0;
		uint32_t stencil_op_pass = 1; // KEEP
		uint32_t stencil_op_fail = 1; // KEEP
		uint32_t stencil_op_depth_fail = 1; // KEEP
	};

	struct technique_info
	{
		std::string name;
		std::vector<pass_info> passes;
		std::unordered_map<std::string, std::pair<type, constant>> annotations;
	};

	struct function_info
	{
		uint32_t definition;
		std::string name;
		std::string unique_name;
		type return_type;
		std::string return_semantic;
		std::vector<struct_member_info> parameter_list;
	};
}
