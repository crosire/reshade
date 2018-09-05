/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "variant.hpp"
#include "runtime_objects.hpp"
#include "spirv_helpers.hpp"

namespace reshadefx
{
	enum class nodeid
	{
		unknown,

		lvalue_expression,
		literal_expression,
		unary_expression,
		binary_expression,
		intrinsic_expression,
		conditional_expression,
		assignment_expression,
		expression_sequence,
		call_expression,
		constructor_expression,
		swizzle_expression,
		field_expression,
		initializer_list,

		compound_statement,
		expression_statement,
		if_statement,
		switch_statement,
		case_statement,
		for_statement,
		while_statement,
		return_statement,
		jump_statement,

		annotation,
		declarator_list,
		variable_declaration,
		struct_declaration,
		function_declaration,
		pass_declaration,
		technique_declaration,
	};

	class node abstract
	{
		void operator=(const node &) = delete;

	public:
		const nodeid id;
		location location;

	protected:
		explicit node(nodeid id) : id(id), location() { }
	};
}

namespace reshadefx::nodes
{
	struct type_node
	{
		enum datatype
		{
			datatype_void,
			datatype_bool,
			datatype_int,
			datatype_uint,
			datatype_float,
			datatype_string,
			datatype_sampler,
			datatype_texture,
			datatype_struct,
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

		static unsigned int rank(const type_node &src, const type_node &dst);

		inline bool is_array() const { return array_length != 0; }
		inline bool is_matrix() const { return rows >= 1 && cols > 1; }
		inline bool is_vector() const { return rows > 1 && !is_matrix(); }
		inline bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
		inline bool is_numeric() const { return is_boolean() || is_integral() || is_floating_point(); }
		inline bool is_void() const { return basetype == datatype_void; }
		inline bool is_boolean() const { return basetype == datatype_bool; }
		inline bool is_integral() const { return basetype == datatype_int || basetype == datatype_uint; }
		inline bool is_floating_point() const { return basetype == datatype_float; }
		inline bool is_texture() const { return basetype == datatype_texture; }
		inline bool is_sampler() const { return basetype == datatype_sampler; }
		inline bool is_struct() const { return basetype == datatype_struct; }
		inline bool has_qualifier(qualifier qualifier) const { return (qualifiers & qualifier) == qualifier; }

		datatype basetype;
		unsigned int qualifiers;
		unsigned int rows : 4, cols : 4;
		int array_length;
		struct struct_declaration_node *definition;
	};

	struct expression_node abstract : public node
	{
		type_node type;

	protected:
		expression_node(nodeid id) : node(id) { }
	};
	struct statement_node abstract : public node
	{
		std::vector<std::string> attributes;

	protected:
		statement_node(nodeid id) : node(id) { }
	};
	struct declaration_node abstract : public node
	{
		std::string name, unique_name;

	protected:
		declaration_node(nodeid id) : node(id) { }
	};

	// Expressions
	struct lvalue_expression_node : public expression_node
	{
		lvalue_expression_node() : expression_node(nodeid::lvalue_expression) { }

		const struct variable_declaration_node *reference;
	};
	struct literal_expression_node : public expression_node
	{
		literal_expression_node() : expression_node(nodeid::literal_expression) { }

		union
		{
			int value_int[16];
			unsigned int value_uint[16];
			float value_float[16];
		};

		std::string value_string;
	};
	struct unary_expression_node : public expression_node
	{
		enum op
		{
			none,
			negate,
			bitwise_not,
			logical_not,
			pre_increase,
			pre_decrease,
			post_increase,
			post_decrease,
			cast,
		};

		unary_expression_node() : expression_node(nodeid::unary_expression) { }

		op op;
		expression_node *operand;
	};
	struct binary_expression_node : public expression_node
	{
		enum op
		{
			none,
			add,
			subtract,
			multiply,
			divide,
			modulo,
			less,
			greater,
			less_equal,
			greater_equal,
			equal,
			not_equal,
			left_shift,
			right_shift,
			bitwise_or,
			bitwise_xor,
			bitwise_and,
			logical_or,
			logical_and,
			element_extract
		};

		binary_expression_node() : expression_node(nodeid::binary_expression) { }

		op op;
		expression_node *operands[2];
	};
	struct intrinsic_expression_node : public expression_node
	{
		enum op
		{
			none,
			abs,
			acos,
			all,
			any,
			bitcast_int2float,
			bitcast_uint2float,
			asin,
			bitcast_float2int,
			bitcast_float2uint,
			atan,
			atan2,
			ceil,
			clamp,
			cos,
			cosh,
			cross,
			ddx,
			ddy,
			degrees,
			determinant,
			distance,
			dot,
			exp,
			exp2,
			faceforward,
			floor,
			frac,
			frexp,
			fwidth,
			isinf,
			isnan,
			ldexp,
			length,
			lerp,
			log,
			log10,
			log2,
			mad,
			max,
			min,
			modf,
			mul,
			normalize,
			pow,
			radians,
			rcp,
			reflect,
			refract,
			round,
			rsqrt,
			saturate,
			sign,
			sin,
			sincos,
			sinh,
			smoothstep,
			sqrt,
			step,
			tan,
			tanh,
			texture,
			texture_fetch,
			texture_gather,
			texture_gather_offset,
			texture_gradient,
			texture_level,
			texture_level_offset,
			texture_offset,
			texture_projection,
			texture_size,
			transpose,
			trunc
		};

		intrinsic_expression_node() : expression_node(nodeid::intrinsic_expression) { }

		op op;
		expression_node *arguments[4];
	};
	struct conditional_expression_node : public expression_node
	{
		conditional_expression_node() : expression_node(nodeid::conditional_expression) { }

		expression_node *condition;
		expression_node *expression_when_true, *expression_when_false;
	};
	struct assignment_expression_node : public expression_node
	{
		enum op
		{
			none,
			add,
			subtract,
			multiply,
			divide,
			modulo,
			bitwise_and,
			bitwise_or,
			bitwise_xor,
			left_shift,
			right_shift
		};

		assignment_expression_node() : expression_node(nodeid::assignment_expression) { }

		op op;
		expression_node *left, *right;
	};
	struct expression_sequence_node : public expression_node
	{
		expression_sequence_node() : expression_node(nodeid::expression_sequence) { }

		std::vector<expression_node *> expression_list;
	};
	struct call_expression_node : public expression_node
	{
		call_expression_node() : expression_node(nodeid::call_expression) { }

		std::string callee_name;
		const struct function_declaration_node *callee;
		std::vector<expression_node *> arguments;
	};
	struct constructor_expression_node : public expression_node
	{
		constructor_expression_node() : expression_node(nodeid::constructor_expression) { }

		std::vector<expression_node *> arguments;
	};
	struct swizzle_expression_node : public expression_node
	{
		swizzle_expression_node() : expression_node(nodeid::swizzle_expression) { }

		expression_node *operand;
		signed char mask[4];
	};
	struct field_expression_node : public expression_node
	{
		field_expression_node() : expression_node(nodeid::field_expression) { }

		expression_node *operand;
		variable_declaration_node *field_reference;
	};
	struct initializer_list_node : public expression_node
	{
		initializer_list_node() : expression_node(nodeid::initializer_list) { }

		std::vector<expression_node *> values;
	};

	// Statements
	struct compound_statement_node : public statement_node
	{
		compound_statement_node() : statement_node(nodeid::compound_statement) { }

		std::vector<statement_node *> statement_list;
	};
	struct expression_statement_node : public statement_node
	{
		expression_statement_node() : statement_node(nodeid::expression_statement) { }

		expression_node *expression;
	};
	struct if_statement_node : public statement_node
	{
		if_statement_node() : statement_node(nodeid::if_statement) { }

		expression_node *condition;
		statement_node *statement_when_true, *statement_when_false;
	};
	struct case_statement_node : public statement_node
	{
		case_statement_node() : statement_node(nodeid::case_statement) { }

		statement_node *statement_list;
		std::vector<literal_expression_node *> labels;
	};
	struct switch_statement_node : public statement_node
	{
		switch_statement_node() : statement_node(nodeid::switch_statement) { }

		expression_node *test_expression;
		std::vector<case_statement_node *> case_list;
	};
	struct for_statement_node : public statement_node
	{
		for_statement_node() : statement_node(nodeid::for_statement) { }

		statement_node *init_statement;
		expression_node *condition, *increment_expression;
		statement_node *statement_list;
	};
	struct while_statement_node : public statement_node
	{
		while_statement_node() : statement_node(nodeid::while_statement) { }

		bool is_do_while;
		expression_node *condition;
		statement_node *statement_list;
	};
	struct return_statement_node : public statement_node
	{
		return_statement_node() : statement_node(nodeid::return_statement) { }

		bool is_discard;
		expression_node *return_value;
	};
	struct jump_statement_node : public statement_node
	{
		jump_statement_node() : statement_node(nodeid::jump_statement) { }

		bool is_break, is_continue;
	};

	// Declarations
	struct variable_declaration_node : public declaration_node
	{
		variable_declaration_node() : declaration_node(nodeid::variable_declaration) { }

		type_node type;
		std::unordered_map<std::string, reshade::variant> annotation_list;
		std::string semantic;
		expression_node *initializer_expression;

		struct
		{
			const variable_declaration_node *texture;
			unsigned int width = 1, height = 1, depth = 1, levels = 1;
			bool srgb_texture;
			reshade::texture_format format = reshade::texture_format::rgba8;
			reshade::texture_filter filter = reshade::texture_filter::min_mag_mip_linear;
			reshade::texture_address_mode address_u = reshade::texture_address_mode::clamp;
			reshade::texture_address_mode address_v = reshade::texture_address_mode::clamp;
			reshade::texture_address_mode address_w = reshade::texture_address_mode::clamp;
			float min_lod, max_lod = FLT_MAX, lod_bias;
		} properties;
	};
	struct declarator_list_node : public statement_node
	{
		declarator_list_node() : statement_node(nodeid::declarator_list) { }

		std::vector<variable_declaration_node *> declarator_list;
	};
	struct struct_declaration_node : public declaration_node
	{
		struct_declaration_node() : declaration_node(nodeid::struct_declaration) { }

		std::vector<variable_declaration_node *> field_list;
	};
	struct function_declaration_node : public declaration_node
	{
		function_declaration_node() : declaration_node(nodeid::function_declaration) { }

		type_node return_type;
		std::vector<variable_declaration_node *> parameter_list;
		std::string return_semantic;
		compound_statement_node *definition;
	};
	struct pass_declaration_node : public declaration_node
	{
		enum : unsigned int
		{
			NONE = 0,

			ZERO = 0,
			ONE = 1,
			SRCCOLOR,
			INVSRCCOLOR,
			SRCALPHA,
			INVSRCALPHA,
			DESTALPHA,
			INVDESTALPHA,
			DESTCOLOR,
			INVDESTCOLOR,

			ADD = 1,
			SUBTRACT,
			REVSUBTRACT,
			MIN,
			MAX,

			KEEP = 1,
			REPLACE = 3,
			INCRSAT,
			DECRSAT,
			INVERT,
			INCR,
			DECR,

			NEVER = 1,
			LESS,
			EQUAL,
			LESSEQUAL,
			GREATER,
			NOTEQUAL,
			GREATEREQUAL,
			ALWAYS
		};

		pass_declaration_node() : declaration_node(nodeid::pass_declaration) { }

		const variable_declaration_node *render_targets[8];
		const function_declaration_node *vertex_shader, *pixel_shader;
		bool clear_render_targets = true, srgb_write_enable, blend_enable, stencil_enable;
		unsigned char color_write_mask = 0xF, stencil_read_mask = 0xFF, stencil_write_mask = 0xFF;
		unsigned int blend_op = ADD, blend_op_alpha = ADD, src_blend = ONE, dest_blend = ZERO, src_blend_alpha = ONE, dest_blend_alpha = ZERO;
		unsigned int stencil_comparison_func = ALWAYS, stencil_reference_value, stencil_op_pass = KEEP, stencil_op_fail = KEEP, stencil_op_depth_fail = KEEP;
	};
	struct technique_declaration_node : public declaration_node
	{
		technique_declaration_node() : declaration_node(nodeid::technique_declaration) { }

		std::unordered_map<std::string, reshade::variant> annotation_list;
		std::vector<pass_declaration_node *> pass_list;
	};
}
