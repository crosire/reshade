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
		explicit location(std::string source, unsigned int line, unsigned int column = 1) : source(std::move(source)), line(line), column(column) { }

		std::string source;
		unsigned int line, column;
	};

	/// <summary>
	/// Structure which encapsulates a parsed value type
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
		/// Calculate the ranking between two types which can be used to select the best matching function overload. The higher the rank, the better the match. A value of zero indicates that the types are not compatible.
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
				op_member,
				op_swizzle,
			};

			op_type op;
			type from, to;
			uint32_t index;
			signed char swizzle[4];
		};

		type type = {};
		uint32_t base = 0;
		constant constant = {};
		bool is_lvalue = false;
		bool is_constant = false;
		location location;
		std::vector<operation> chain;

		/// <summary>
		/// Initialize the expression to a l-value.
		/// </summary>
		/// <param name="loc">The code location of the expression.</param>
		/// <param name="base">The ID of the l-value.</param>
		/// <param name="type">The value type of the expression result.</param>
		void reset_to_lvalue(const reshadefx::location &loc, uint32_t base, const reshadefx::type &type);
		/// <summary>
		/// Initialize the expression to a r-value.
		/// </summary>
		/// <param name="loc">The code location of the expression.</param>
		/// <param name="base">The ID of the r-value.</param>
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
		/// Add a struct member lookup to the current access chain.
		/// </summary>
		/// <param name="index">The index of the member to dereference.</param>
		/// <param name="type">The value type of the member.</param>
		void add_member_access(unsigned int index, const reshadefx::type &type);
		/// <summary>
		/// 
		/// </summary>
		/// <param name="codegen"></param>
		/// <param name="index"></param>
		void add_static_index_access(class codegen *codegen, unsigned int index);
		void add_dynamic_index_access(class codegen *codegen, uint32_t index_expression);
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
		void evaluate_constant_expression(enum class tokenid op);
		/// <summary>
		/// Apply a binary operation to this constant expression.
		/// </summary>
		/// <param name="op">The binary operator to apply.</param>
		/// <param name="rhs">The constant to use as right-hand side of the binary operation.</param>
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
		uint32_t size = 0;
		uint32_t offset = 0;
		std::unordered_map<std::string, std::pair<reshadefx::type, constant>> annotations;
		bool has_initializer_value = false;
		constant initializer_value;
	};

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

		dxt1,
		dxt3,
		dxt5,
		latc1,
		latc2
	};

	enum class texture_address_mode
	{
		wrap = 1,
		mirror = 2,
		clamp = 3,
		border = 4
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
		texture_format format = texture_format::rgba8;
	};

	struct sampler_info
	{
		uint32_t id = 0;
		uint32_t set = 0;
		uint32_t binding = 0;
		std::string unique_name;
		std::string texture_name;
		std::unordered_map<std::string, std::pair<type, constant>> annotations;
		texture_filter filter = texture_filter::min_mag_mip_linear;
		texture_address_mode address_u = texture_address_mode::clamp;
		texture_address_mode address_v = texture_address_mode::clamp;
		texture_address_mode address_w = texture_address_mode::clamp;
		float min_lod = -FLT_MAX;
		float max_lod = +FLT_MAX;
		float lod_bias = 0.0f;
		uint8_t srgb = false;
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
		uint32_t viewport_width = 0;
		uint32_t viewport_height = 0;
	};

	struct technique_info
	{
		std::string name;
		std::vector<pass_info> passes;
		std::unordered_map<std::string, std::pair<type, constant>> annotations;
	};


	/// <summary>
	/// In-memory representation of an effect file.
	/// </summary>
	struct module
	{
		std::string hlsl;
		std::vector<uint32_t> spirv;
		std::vector<texture_info> textures;
		std::vector<sampler_info> samplers;
		std::vector<uniform_info> uniforms, spec_constants;
		std::vector<technique_info> techniques;
		std::vector<std::pair<std::string, bool>> entry_points;
	};
}
