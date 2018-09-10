/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include <assert.h>
#include <algorithm>
#include <functional>

struct on_scope_exit
{
	template <typename F>
	on_scope_exit(F lambda)
		: leave(lambda) { }
	~on_scope_exit() { leave(); }
	std::function<void()> leave;
};

static inline uintptr_t align(uintptr_t address, size_t alignment)
{
	return (address % alignment != 0) ? address + alignment - address % alignment : address;
}

static reshadefx::constant evaluate_constant_expression(reshadefx::tokenid op, const reshadefx::type &type, const reshadefx::constant &lhs)
{
	using namespace reshadefx;

	reshadefx::constant res = {};

	switch (op)
	{
	case tokenid::exclaim:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] = !lhs.as_uint[i];
		break;
	case tokenid::minus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] = -lhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_int[i] = -lhs.as_int[i];
		break;
	case tokenid::tilde:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] = ~lhs.as_uint[i];
		break;
	}

	return res;
}
static reshadefx::constant evaluate_constant_expression(reshadefx::tokenid op, const reshadefx::type &type, const reshadefx::constant &lhs, const reshadefx::constant &rhs)
{
	using namespace reshadefx;

	reshadefx::constant res = lhs;

	switch (op)
	{
	case tokenid::percent:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] = fmodf(lhs.as_float[i], rhs.as_float[i]);
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_int[i] %= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] %= rhs.as_uint[i];
		break;
	case tokenid::star:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] *= rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] *= rhs.as_uint[i];
		break;
	case tokenid::plus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] += rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] += rhs.as_uint[i];
		break;
	case tokenid::minus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] -= rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] -= rhs.as_uint[i];
		break;
	case tokenid::slash:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_float[i] /= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_int[i] /= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] /= rhs.as_uint[i];
		break;
	case tokenid::ampersand:
	case tokenid::ampersand_ampersand:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] &= rhs.as_uint[i];
		break;
	case tokenid::pipe:
	case tokenid::pipe_pipe:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] |= rhs.as_uint[i];
		break;
	case tokenid::caret:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] ^= rhs.as_uint[i];
		break;
	case tokenid::less:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] < rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_int[i] < rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] < rhs.as_uint[i];
		break;
	case tokenid::less_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] <= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_int[i] <= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] <= rhs.as_uint[i];
		break;
	case tokenid::greater:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] > rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_int[i] > rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] > rhs.as_uint[i];
		break;
	case tokenid::greater_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] >= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_int[i] >= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] >= rhs.as_uint[i];
		break;
	case tokenid::equal_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] == rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] == rhs.as_uint[i];
		break;
	case tokenid::exclaim_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_float[i] != rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] = lhs.as_uint[i] != rhs.as_uint[i];
		break;
	case tokenid::less_less:
		for (unsigned int i = 0; i < type.components(); ++i)
			res.as_uint[i] <<= rhs.as_uint[i];
		break;
	case tokenid::greater_greater:
		if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_int[i] >>= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				res.as_uint[i] >>= rhs.as_uint[i];
		break;
	}

	return res;
}

// -- Parsing -- //

bool reshadefx::parser::parse(const std::string &input)
{
	_lexer.reset(new lexer(input));
	_lexer_backup.reset();

	consume();

	bool success = true;

	while (!peek(tokenid::end_of_file))
		if (!parse_top())
			success = false;

	// Create global uniform buffer object
	if (_global_ubo_type != 0)
	{
		std::vector<spv::Id> member_types;
		for (const auto &uniform : _uniforms)
			member_types.push_back(uniform.type_id);

		define_struct(_global_ubo_type, "$Globals", {}, member_types);
		add_decoration(_global_ubo_type, spv::DecorationBlock);
		add_decoration(_global_ubo_type, spv::DecorationBinding, { 0 });
		add_decoration(_global_ubo_type, spv::DecorationDescriptorSet, { 0 });

		define_variable(_global_ubo_variable, "$Globals", {}, { type::t_struct, 0, 0, type::q_uniform, true, false, false, 0, _global_ubo_type }, spv::StorageClassUniform);
	}

	return success;
}

// -- Error Handling -- //

void reshadefx::parser::error(const location &location, unsigned int code, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

	if (code == 0)
	{
		_errors += "error: ";
	}
	else
	{
		_errors += "error X" + std::to_string(code) + ": ";
	}

	_errors += message + '\n';
}
void reshadefx::parser::warning(const location &location, unsigned int code, const std::string &message)
{
	_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

	if (code == 0)
	{
		_errors += "warning: ";
	}
	else
	{
		_errors += "warning X" + std::to_string(code) + ": ";
	}

	_errors += message + '\n';
}

// -- Token Management -- //

void reshadefx::parser::backup()
{
	_lexer.swap(_lexer_backup);
	_lexer.reset(new lexer(*_lexer_backup));
	_token_backup = _token_next;
}
void reshadefx::parser::restore()
{
	_lexer.swap(_lexer_backup);
	_token_next = _token_backup;
}

bool reshadefx::parser::peek(tokenid tokid) const
{
	return _token_next.id == tokid;
}
void reshadefx::parser::consume()
{
	_token = _token_next;
	_token_next = _lexer->lex();
}
void reshadefx::parser::consume_until(tokenid tokid)
{
	while (!accept(tokid) && !peek(tokenid::end_of_file))
	{
		consume();
	}
}
bool reshadefx::parser::accept(tokenid tokid)
{
	if (peek(tokid))
	{
		consume();
		return true;
	}

	return false;
}
bool reshadefx::parser::expect(tokenid tokid)
{
	if (!accept(tokid))
	{
		error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected '" + token::id_to_name(tokid) + "'");
		return false;
	}

	return true;
}

// -- Type Parsing -- //

bool reshadefx::parser::accept_type_class(type &type)
{
	type.rows = type.cols = 0;

	if (peek(tokenid::identifier))
	{
		type.base = type::t_struct;

		const symbol symbol = find_symbol(_token_next.literal_as_string);

		if (symbol.id && symbol.op == spv::OpTypeStruct)
		{
			type.definition = symbol.id;

			consume();
			return true;
		}

		return false;
	}
	else if (accept(tokenid::vector))
	{
		type.base = type::t_float;
		type.rows = 4, type.cols = 1;

		if (accept('<'))
		{
			if (!accept_type_class(type)) // This overwrites the base type again
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected vector element type"), false;
			else if (!type.is_scalar())
				return error(_token.location, 3122, "vector element type must be a scalar type"), false;

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3052, "vector dimension must be between 1 and 4"), false;

			type.rows = _token.literal_as_int;

			if (!expect('>'))
				return false;
		}

		return true;
	}
	else if (accept(tokenid::matrix))
	{
		type.base = type::t_float;
		type.rows = 4, type.cols = 4;

		if (accept('<'))
		{
			if (!accept_type_class(type)) // This overwrites the base type again
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected matrix element type"), false;
			else if (!type.is_scalar())
				return error(_token.location, 3123, "matrix element type must be a scalar type"), false;

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

			type.rows = _token.literal_as_int;

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

			type.cols = _token.literal_as_int;

			if (!expect('>'))
				return false;
		}

		return true;
	}

	switch (_token_next.id)
	{
	case tokenid::void_:
		type.base = type::t_void;
		break;
	case tokenid::bool_:
	case tokenid::bool2:
	case tokenid::bool3:
	case tokenid::bool4:
		type.base = type::t_bool;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::bool_);
		type.cols = 1;
		break;
	case tokenid::bool2x2:
	case tokenid::bool3x3:
	case tokenid::bool4x4:
		type.base = type::t_bool;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::bool2x2);
		type.cols = type.rows;
		break;
	case tokenid::int_:
	case tokenid::int2:
	case tokenid::int3:
	case tokenid::int4:
		type.base = type::t_int;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::int_);
		type.cols = 1;
		break;
	case tokenid::int2x2:
	case tokenid::int3x3:
	case tokenid::int4x4:
		type.base = type::t_int;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::int2x2);
		type.cols = type.rows;
		break;
	case tokenid::uint_:
	case tokenid::uint2:
	case tokenid::uint3:
	case tokenid::uint4:
		type.base = type::t_uint;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::uint_);
		type.cols = 1;
		break;
	case tokenid::uint2x2:
	case tokenid::uint3x3:
	case tokenid::uint4x4:
		type.base = type::t_uint;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::uint2x2);
		type.cols = type.rows;
		break;
	case tokenid::float_:
	case tokenid::float2:
	case tokenid::float3:
	case tokenid::float4:
		type.base = type::t_float;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::float_);
		type.cols = 1;
		break;
	case tokenid::float2x2:
	case tokenid::float3x3:
	case tokenid::float4x4:
		type.base = type::t_float;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::float2x2);
		type.cols = type.rows;
		break;
	case tokenid::string_:
		type.base = type::t_string;
		break;
	case tokenid::texture:
		type.base = type::t_texture;
		break;
	case tokenid::sampler:
		type.base = type::t_sampler;
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_type_qualifiers(type &type)
{
	unsigned int qualifiers = 0;

	// Storage
	if (accept(tokenid::extern_))
		qualifiers |= type::q_extern;
	if (accept(tokenid::static_))
		qualifiers |= type::q_static;
	if (accept(tokenid::uniform_))
		qualifiers |= type::q_uniform;
	if (accept(tokenid::volatile_))
		qualifiers |= type::q_volatile;
	if (accept(tokenid::precise))
		qualifiers |= type::q_precise;

	if (accept(tokenid::in))
		qualifiers |= type::q_in;
	if (accept(tokenid::out))
		qualifiers |= type::q_out;
	if (accept(tokenid::inout))
		qualifiers |= type::q_inout;

	// Modifiers
	if (accept(tokenid::const_))
		qualifiers |= type::q_const;

	// Interpolation
	if (accept(tokenid::linear))
		qualifiers |= type::q_linear;
	if (accept(tokenid::noperspective))
		qualifiers |= type::q_noperspective;
	if (accept(tokenid::centroid))
		qualifiers |= type::q_centroid;
	if (accept(tokenid::nointerpolation))
		qualifiers |= type::q_nointerpolation;

	if (qualifiers == 0)
		return false;
	if ((type.qualifiers & qualifiers) == qualifiers)
		warning(_token.location, 3048, "duplicate usages specified");

	type.qualifiers |= qualifiers;

	// Continue parsing potential additional qualifiers until no more are found
	accept_type_qualifiers(type);

	return true;
}

bool reshadefx::parser::parse_type(type &type)
{
	type.qualifiers = 0;

	accept_type_qualifiers(type);

	if (!accept_type_class(type))
		return false;

	if (type.is_integral() && (type.has(type::q_centroid) || type.has(type::q_noperspective)))
		return error(_token.location, 4576, "signature specifies invalid interpolation mode for integer component type"), false;
	else if (type.has(type::q_centroid) && !type.has(type::q_noperspective))
		type.qualifiers |= type::q_linear;

	return true;
}
bool reshadefx::parser::parse_array_size(type &type)
{
	// Reset array length to zero before checking if one exists
	type.array_length = 0;

	if (accept('['))
	{
		// Length expression should be literal, so no instructions to store anywhere
		spirv_basic_block block;

		if (accept(']'))
		{
			// No length expression, so this is an unsized array
			type.array_length = -1;
		}
		else if (expression expression; parse_expression(block, expression) && expect(']'))
		{
			if (!expression.is_constant || !(expression.type.is_scalar() && expression.type.is_integral()))
				return error(expression.location, 3058, "array dimensions must be literal scalar expressions"), false;

			type.array_length = expression.constant.as_uint[0];

			if (type.array_length < 1 || type.array_length > 65536)
				return error(expression.location, 3059, "array dimension must be between 1 and 65536"), false;
		}
		else
		{
			return false;
		}
	}

	return true;
}

// -- Expression Parsing -- //

bool reshadefx::parser::accept_unary_op()
{
	switch (_token_next.id)
	{
	case tokenid::exclaim: // !x (logical not)
	case tokenid::plus: // +x
	case tokenid::minus: // -x (negate)
	case tokenid::tilde: // ~x (bitwise not)
	case tokenid::plus_plus: // ++x
	case tokenid::minus_minus: // --x
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_postfix_op()
{
	switch (_token_next.id)
	{
	case tokenid::plus_plus: // ++x
	case tokenid::minus_minus: // --x
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::peek_multary_op(unsigned int &precedence) const
{
	// Precedence values taken from https://cppreference.com/w/cpp/language/operator_precedence
	switch (_token_next.id)
	{
	case tokenid::question: precedence = 1; break; // x ? a : b
	case tokenid::pipe_pipe: precedence = 2; break; // a || b (logical or)
	case tokenid::ampersand_ampersand: precedence = 3; break; // a && b (logical and)
	case tokenid::pipe: precedence = 4; break; // a | b (bitwise or)
	case tokenid::caret: precedence = 5; break; // a ^ b (bitwise xor)
	case tokenid::ampersand: precedence = 6; break; // a & b (bitwise and)
	case tokenid::equal_equal: precedence = 7; break; // a == b (equal)
	case tokenid::exclaim_equal: precedence = 7; break; // a != b (not equal)
	case tokenid::less: precedence = 8; break; // a < b
	case tokenid::greater: precedence = 8; break; // a > b
	case tokenid::less_equal: precedence = 8; break; // a <= b
	case tokenid::greater_equal: precedence = 8; break; // a >= b
	case tokenid::less_less: precedence = 9; break; // a << b (left shift)
	case tokenid::greater_greater: precedence = 9; break; // a >> b (right shift)
	case tokenid::plus: precedence = 10; break; // a + b (add)
	case tokenid::minus: precedence = 10; break; // a - b (subtract)
	case tokenid::star: precedence = 11; break; // a * b (multiply)
	case tokenid::slash: precedence = 11; break; // a / b (divide)
	case tokenid::percent: precedence = 11; break; // a % b (modulo)
	default:
		return false;
	}

	// Do not consume token yet since the expression may be skipped due to precedence
	return true;
}
bool reshadefx::parser::accept_assignment_op()
{
	switch (_token_next.id)
	{
	case tokenid::equal: // a = b
	case tokenid::percent_equal: // a %= b
	case tokenid::ampersand_equal: // a &= b
	case tokenid::star_equal: // a *= b
	case tokenid::plus_equal: // a += b
	case tokenid::minus_equal: // a -= b
	case tokenid::slash_equal: // a /= b
	case tokenid::less_less_equal: // a <<= b
	case tokenid::greater_greater_equal: // a >>= b
	case tokenid::caret_equal: // a ^= b
	case tokenid::pipe_equal: // a |= b
		break;
	default:
		return false;
	}

	consume();

	return true;
}

bool reshadefx::parser::parse_expression(spirv_basic_block &block, expression &exp)
{
	// Parse first expression
	if (!parse_expression_assignment(block, exp))
		return false;

	// Continue parsing if an expression sequence is next (in the form "a, b, c, ...")
	while (accept(','))
		// Overwrite 'exp' since conveniently the last expression in the sequence is the result
		if (!parse_expression_assignment(block, exp))
			return false;

	return true;
}
bool reshadefx::parser::parse_expression_unary(spirv_basic_block &block, expression &exp)
{
	auto location = _token_next.location;

	#pragma region Prefix Expression
	// Check if a prefix operator exists
	if (accept_unary_op())
	{
		// Remember the operator token before parsing the expression that follows it
		const tokenid op = _token.id;

		// Parse the actual expression
		if (!parse_expression_unary(block, exp))
			return false;

		// Unary operators are only valid on basic types
		if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
			return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;

		// Special handling for the "++" and "--" operators
		if (op == tokenid::plus_plus || op == tokenid::minus_minus)
		{
			if (exp.type.has(type::q_const) || exp.type.has(type::q_uniform) || !exp.is_lvalue)
				return error(location, 3025, "l-value specifies const object"), false;

			// Create a constant one in the type of the expression
			constant one = {};
			for (unsigned int i = 0; i < exp.type.components(); ++i)
				if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] = 1u;

			const auto result = add_binary_operation(block, location, op, exp.type, access_chain_load(block, exp), convert_constant(exp.type, one));

			// The "++" and "--" operands modify the source variable, so store result back into it
			access_chain_store(block, exp, result, exp.type);
		}
		else if (op != tokenid::plus) // Ignore "+" operator since it does not actually do anything
		{
			// The "~" bitwise operator is only valid on integral types
			if (op == tokenid::tilde && !exp.type.is_integral())
				return error(exp.location, 3082, "int or unsigned int type required"), false;
			// The logical not operator expects a boolean type as input, so perform cast if necessary
			if (op == tokenid::exclaim && !exp.type.is_boolean())
				add_cast_operation(exp, { type::t_bool, exp.type.rows, exp.type.cols }); // The result type will be boolean as well

			if (exp.is_constant) // Constant expressions can be evaluated at compile time
			{
				exp.constant = evaluate_constant_expression(op, exp.type, exp.constant);
			}
			else
			{
				const auto result = add_unary_operation(block, location, op, exp.type, access_chain_load(block, exp));

				exp.reset_to_rvalue(location, result, exp.type);
			}
		}
	}
	else if (accept('('))
	{
		backup();

		// Check if this is a C-style cast expression
		if (type cast_type; accept_type_class(cast_type))
		{
			if (peek('('))
			{
				// This is not a C-style cast but a constructor call, so need to roll-back and parse that instead
				restore();
			}
			else if (expect(')'))
			{
				// Parse expression behind cast operator
				if (!parse_expression_unary(block, exp))
					return false;

				// Check if the types already match, in which case there is nothing to do
				if (exp.type.base == cast_type.base && (exp.type.rows == cast_type.rows && exp.type.cols == cast_type.cols) && !(exp.type.is_array() || cast_type.is_array()))
					return true;

				// Check if a cast between the types is valid
				if (!exp.type.is_numeric() || !cast_type.is_numeric())
					return error(location, 3017, "cannot convert non-numeric types"), false;
				else if (exp.type.components() < cast_type.components() && !exp.type.is_scalar())
					return error(location, 3017, "cannot convert these vector types"), false;

				add_cast_operation(exp, cast_type);
				return true;
			}
			else
			{
				// Type name was not followed by a closing parenthesis
				return false;
			}
		}

		// Parse expression between the parentheses
		if (!parse_expression(block, exp) || !expect(')'))
			return false;
	}
	else if (accept('{'))
	{
		std::vector<expression> elements;

		bool is_constant = true;
		type composite_type = { type::t_bool, 1, 1 };

		while (!peek('}'))
		{
			// There should be a comma between arguments
			if (!elements.empty() && !expect(','))
				return consume_until('}'), false;

			// Initializer lists might contain a comma at the end, so break out of the loop if nothing follows afterwards
			if (peek('}'))
				break;

			// Parse the argument expression
			if (!parse_expression_assignment(block, elements.emplace_back()))
				return consume_until('}'), false;

			expression &element = elements.back();

			is_constant &= element.is_constant; // Result is only constant if all arguments are constant
			composite_type = type::merge(composite_type, element.type);
		}

		// Constant arrays can be constructed at compile time
		if (is_constant)
		{
			constant res = {};
			for (expression &element : elements)
			{
				add_cast_operation(element, composite_type);
				res.array_data.push_back(element.constant);
			}

			composite_type.array_length = elements.size();

			exp.reset_to_rvalue_constant(location, std::move(res), composite_type);
		}
		else
		{
			composite_type.array_length = elements.size();

			const auto result = construct_type(block, composite_type, elements);

			exp.reset_to_rvalue(location, result, composite_type);
		}

		return expect('}');
	}
	else if (accept(tokenid::true_literal))
	{
		exp.reset_to_rvalue_constant(location, true);
	}
	else if (accept(tokenid::false_literal))
	{
		exp.reset_to_rvalue_constant(location, false);
	}
	else if (accept(tokenid::int_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_int);
	}
	else if (accept(tokenid::uint_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_uint);
	}
	else if (accept(tokenid::float_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_float);
	}
	else if (accept(tokenid::double_literal))
	{
		// Convert double literal to float literal for now
		warning(location, 5000, "double literal truncated to float literal");

		exp.reset_to_rvalue_constant(location, static_cast<float>(_token.literal_as_double));
	}
	else if (accept(tokenid::string_literal))
	{
		std::string value = std::move(_token.literal_as_string);

		// Multiple string literals in sequence are concatenated into a single string literal
		while (accept(tokenid::string_literal))
			value += _token.literal_as_string;

		exp.reset_to_rvalue_constant(location, std::move(value));
	}
	else if (type type; accept_type_class(type)) // Check if this is a constructor call expression
	{
		if (!expect('('))
			return false;
		if (!type.is_numeric())
			return error(location, 3037, "constructors only defined for numeric base types"), false;

		// Empty constructors do not exist
		if (accept(')'))
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		// Parse entire argument expression list
		bool is_constant = true;
		unsigned int num_components = 0;
		std::vector<expression> arguments;

		while (!peek(')'))
		{
			// There should be a comma between arguments
			if (!arguments.empty() && !expect(','))
				return false;

			// Parse the argument expression
			if (!parse_expression_assignment(block, arguments.emplace_back()))
				return false;

			expression &argument = arguments.back();

			// Constructors are only defined for numeric base types
			if (!argument.type.is_numeric())
				return error(argument.location, 3017, "cannot convert non-numeric types"), false;

			is_constant &= argument.is_constant; // Result is only constant if all arguments are constant
			num_components += argument.type.components();
		}

		// The list should be terminated with a parenthesis
		if (!expect(')'))
			return false;

		// The total number of argument elements needs to match the number of elements in the result type
		if (num_components != type.components())
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		assert(num_components > 0 && num_components <= 16 && !type.is_array());

		if (is_constant) // Constants can be converted at compile time
		{
			constant res = {};
			unsigned int i = 0;
			for (expression &argument : arguments)
			{
				add_cast_operation(argument, { type.base, argument.type.rows, argument.type.cols });
				for (unsigned int k = 0; k < argument.type.components(); ++k)
					res.as_uint[i++] = argument.constant.as_uint[k];
			}

			exp.reset_to_rvalue_constant(location, std::move(res), type);
		}
		else if (arguments.size() > 1)
		{
			const auto result = construct_type(block, type, arguments);

			exp.reset_to_rvalue(location, result, type);
		}
		else // A constructor call with a single argument is identical to a cast
		{
			assert(!arguments.empty());

			// Reset expression to only argument and add cast to expression access chain
			exp = std::move(arguments[0]), add_cast_operation(exp, type);
		}
	}
	else // At this point only identifiers are left to check and resolve
	{
		// Starting an identifier with '::' restricts the symbol search to the global namespace level
		const bool exclusive = accept(tokenid::colon_colon);

		std::string identifier;

		if (exclusive ? expect(tokenid::identifier) : accept(tokenid::identifier))
			identifier = std::move(_token.literal_as_string);
		else
			return false; // Warning: This may leave the expression path without issuing an error, so need to catch that at the target side!

		// Can concatenate multiple '::' to force symbol search for a specific namespace level
		while (accept(tokenid::colon_colon))
		{
			if (!expect(tokenid::identifier))
				return false;
			identifier += "::" + _token.literal_as_string;
		}

		// Figure out which scope to start searching in
		scope scope = { "::", 0, 0 };
		if (!exclusive) scope = current_scope();

		// Lookup name in the symbol table
		symbol symbol = find_symbol(identifier, scope, exclusive);

		// Check if this is a function call or variable reference
		if (accept('('))
		{
			// Can only call symbols that are functions, but do not abort yet if no symbol was found since the identifier may reference an intrinsic
			if (symbol.id && symbol.op != spv::OpFunction)
				return error(location, 3005, "identifier '" + identifier + "' represents a variable, not a function"), false;

			// Parse entire argument expression list
			std::vector<expression> arguments;

			while (!peek(')'))
			{
				// There should be a comma between arguments
				if (!arguments.empty() && !expect(','))
					return false;

				// Parse the argument expression
				if (!parse_expression_assignment(block, arguments.emplace_back()))
					return false;
			}

			// The list should be terminated with a parenthesis
			if (!expect(')'))
				return false;

			// Try to resolve the call by searching through both function symbols and intrinsics
			bool undeclared = !symbol.id, ambiguous = false;

			if (!resolve_function_call(identifier, arguments, scope, symbol, ambiguous))
			{
				if (undeclared && symbol.op == spv::OpFunctionCall)
					error(location, 3004, "undeclared identifier '" + identifier + "'"); // This catches no matching intrinsic overloads as well
				else if (ambiguous)
					error(location, 3067, "ambiguous function call to '" + identifier + "'");
				else
					error(location, 3013, "no matching function overload for '" + identifier + "'");
				return false;
			}

			assert(symbol.function != nullptr);

			std::vector<expression> parameters(arguments.size());

			// We need to allocate some temporary variables to pass in and load results from pointer parameters
			for (size_t i = 0; i < arguments.size(); ++i)
			{
				const auto &param_type = symbol.function->parameter_list[i].type;

				if (arguments[i].type.components() > param_type.components())
					warning(arguments[i].location, 3206, "implicit truncation of vector type");

				auto target_type = param_type;
				target_type.is_ptr = false;
				add_cast_operation(arguments[i], target_type);

				if (param_type.is_ptr)
				{
					const auto variable = make_id();
					define_variable(variable, nullptr, arguments[i].location, param_type, spv::StorageClassFunction);
					parameters[i].reset_to_lvalue(arguments[i].location, variable, param_type);
				}
				else
				{
					parameters[i].reset_to_rvalue(arguments[i].location, access_chain_load(block, arguments[i]), param_type);
				}
			}

			// Copy in parameters from the argument access chains to parameter variables
			for (size_t i = 0; i < arguments.size(); ++i)
				if (parameters[i].is_lvalue && parameters[i].type.has(type::q_in)) // Only do this for pointer parameters as discovered above
					access_chain_store(block, parameters[i], access_chain_load(block, arguments[i]), arguments[i].type);

			// Check if the call resolving found an intrinsic or function
			if (symbol.intrinsic)
			{
				// This is an intrinsic, so invoke it
				const auto result = symbol.intrinsic(*this, block, parameters);

				exp.reset_to_rvalue(location, result, symbol.type);
			}
			else
			{
				// This is a function symbol, so add a call to it
				spirv_instruction &call = add_instruction(block, location, spv::OpFunctionCall, convert_type(symbol.type))
					.add(symbol.id); // Function
				for (size_t i = 0; i < parameters.size(); ++i)
					call.add(parameters[i].base); // Arguments

				exp.reset_to_rvalue(location, call.result, symbol.type);
			}

			// Copy out parameters from parameter variables back to the argument access chains
			for (size_t i = 0; i < arguments.size(); ++i)
				if (parameters[i].is_lvalue && parameters[i].type.has(type::q_out)) // Only do this for pointer parameters as discovered above
					access_chain_store(block, arguments[i], access_chain_load(block, parameters[i]), arguments[i].type);
		}
		else
		{
			// Show error if no symbol matching the identifier was found
			if (!symbol.op) // Note: 'symbol.id' is zero for constants, so have to check 'symbol.op', which cannot be zero
				return error(location, 3004, "undeclared identifier '" + identifier + "'"), false;
			else if (symbol.op == spv::OpVariable)
				// Simply return the pointer to the variable, dereferencing is done on site where necessary
				exp.reset_to_lvalue(location, symbol.id, symbol.type);
			else if (symbol.op == spv::OpConstant)
				// Constants are loaded into the access chain
				exp.reset_to_rvalue_constant(location, symbol.constant, symbol.type);
			else if (symbol.op == spv::OpAccessChain) {
				// Uniform variables need to be dereferenced
				exp.reset_to_lvalue(location, symbol.id, { type::t_struct, 0, 0, 0, false, false, false, 0, symbol.id });
				add_member_access(exp, reinterpret_cast<uintptr_t>(symbol.function), symbol.type); // The member index was stored in the 'function' field
			}
			else // Can only reference variables and constants by name, functions need to be called
				return error(location, 3005, "identifier '" + identifier + "' represents a function, not a variable"), false;
		}
	}
	#pragma endregion

	#pragma region Postfix Expression
	while (!peek(tokenid::end_of_file))
	{
		location = _token_next.location;

		// Check if a postfix operator exists
		if (accept_postfix_op())
		{
			// Unary operators are only valid on basic types
			if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;
			if (exp.type.has(type::q_const) || exp.type.has(type::q_uniform) || !exp.is_lvalue)
				return error(exp.location, 3025, "l-value specifies const object"), false;

			// Create a constant one in the type of the expression
			constant one = {};
			for (unsigned int i = 0; i < exp.type.components(); ++i)
				if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] = 1u;

			const auto result = add_binary_operation(block, location, _token.id, exp.type, access_chain_load(block, exp), convert_constant(exp.type, one));

			// The "++" and "--" operands modify the source variable, so store result back into it
			access_chain_store(block, exp, result, exp.type);

			// All postfix operators return a r-value rather than a l-value to the variable
			exp.reset_to_rvalue(location, result, exp.type);
		}
		else if (accept('.'))
		{
			if (!expect(tokenid::identifier))
				return false;

			location = std::move(_token.location);
			const auto subscript = std::move(_token.literal_as_string);

			if (accept('(')) // Methods (function calls on types) are not supported right now
			{
				if (!exp.type.is_struct() || exp.type.is_array())
					error(location, 3087, "object does not have methods");
				else
					error(location, 3088, "structures do not have methods");
				return false;
			}
			else if (exp.type.is_array()) // Arrays do not have subscripts
			{
				error(location, 3018, "invalid subscript on array");
				return false;
			}
			else if (exp.type.is_vector())
			{
				const size_t length = subscript.size();
				if (length > 4)
					return error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long"), false;

				bool is_const = false;
				signed char offsets[4] = { -1, -1, -1, -1 };
				enum { xyzw, rgba, stpq } set[4];

				for (size_t i = 0; i < length; ++i)
				{
					switch (subscript[i])
					{
					case 'x': offsets[i] = 0, set[i] = xyzw; break;
					case 'y': offsets[i] = 1, set[i] = xyzw; break;
					case 'z': offsets[i] = 2, set[i] = xyzw; break;
					case 'w': offsets[i] = 3, set[i] = xyzw; break;
					case 'r': offsets[i] = 0, set[i] = rgba; break;
					case 'g': offsets[i] = 1, set[i] = rgba; break;
					case 'b': offsets[i] = 2, set[i] = rgba; break;
					case 'a': offsets[i] = 3, set[i] = rgba; break;
					case 's': offsets[i] = 0, set[i] = stpq; break;
					case 't': offsets[i] = 1, set[i] = stpq; break;
					case 'p': offsets[i] = 2, set[i] = stpq; break;
					case 'q': offsets[i] = 3, set[i] = stpq; break;
					default:
						return error(location, 3018, "invalid subscript '" + subscript + "'"), false;
					}

					if (i > 0 && (set[i] != set[i - 1]))
						return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;
					if (static_cast<unsigned int>(offsets[i]) >= exp.type.rows)
						return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

					// The result is not modifiable if a swizzle appears multiple times
					for (size_t k = 0; k < i; ++k)
						if (offsets[k] == offsets[i]) {
							is_const = true;
							break;
						}
				}

				// Add swizzle to current access chain
				add_swizzle_access(exp, offsets, length);

				if (is_const || exp.type.has(type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_matrix())
			{
				const size_t length = subscript.size();
				if (length < 3)
					return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

				bool is_const = false;
				signed char offsets[4] = { -1, -1, -1, -1 };
				const unsigned int set = subscript[1] == 'm';
				const int coefficient = !set;

				for (size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
				{
					if (subscript[i] != '_' || subscript[i + set + 1] < '0' + coefficient || subscript[i + set + 1] > '3' + coefficient || subscript[i + set + 2] < '0' + coefficient || subscript[i + set + 2] > '3' + coefficient)
						return error(location, 3018, "invalid subscript '" + subscript + "'"), false;
					if (set && subscript[i + 1] != 'm')
						return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;

					const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
					const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

					if ((row >= exp.type.rows || col >= exp.type.cols) || j > 3)
						return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

					offsets[j] = static_cast<unsigned char>(row * 4 + col);

					// The result is not modifiable if a swizzle appears multiple times
					for (size_t k = 0; k < j; ++k)
						if (offsets[k] == offsets[j]) {
							is_const = true;
							break;
						}
				}

				// Add swizzle to current access chain
				add_swizzle_access(exp, offsets, length / (3 + set));

				if (is_const || exp.type.has(type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_struct())
			{
				// Find member with matching name is structure definition
				size_t member_index = 0;
				for (const spirv_struct_member_info &member : _structs[exp.type.definition].member_list) {
					if (member.name == subscript)
						break;
					++member_index;
				}

				if (member_index >= _structs[exp.type.definition].member_list.size())
					return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

				// Add field index to current access chain
				add_member_access(exp, member_index, _structs[exp.type.definition].member_list[member_index].type);

				if (exp.type.has(type::q_uniform)) // Member access to uniform structure is not modifiable
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_scalar())
			{
				const size_t length = subscript.size();
				if (length > 4)
					return error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long"), false;

				for (size_t i = 0; i < length; ++i)
					if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
						return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

				// Promote scalar to vector type using cast
				auto target_type = exp.type;
				target_type.rows = static_cast<unsigned int>(length);

				add_cast_operation(exp, target_type);
			}
			else
			{
				error(location, 3018, "invalid subscript '" + subscript + "'");
				return false;
			}
		}
		else if (accept('['))
		{
			if (!exp.type.is_array() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(_token.location, 3121, "array, matrix, vector, or indexable object type expected in index expression"), false;

			// Parse index expression
			expression index;
			if (!parse_expression(block, index) || !expect(']'))
				return false;
			else if (!index.type.is_scalar() || !index.type.is_integral())
				return error(index.location, 3120, "invalid type for index - index must be an integer scalar"), false;

			// Check array bounds if known
			if (index.is_constant && exp.type.array_length > 0 && index.constant.as_uint[0] >= static_cast<unsigned int>(exp.type.array_length))
				return error(index.location, 3504, "array index out of bounds"), false;

			// Add index expression to current access chain
			if (index.is_constant)
				add_static_index_access(exp, index.constant.as_uint[0]);
			else
				add_dynamic_index_access(exp, access_chain_load(block, index));
		}
		else
		{
			break;
		}
	}
	#pragma endregion

	return true;
}
bool reshadefx::parser::parse_expression_multary(spirv_basic_block &block, expression &lhs, unsigned int left_precedence)
{
	// Parse left hand side of the expression
	if (!parse_expression_unary(block, lhs))
		return false;

	// Check if an operator exists so that this is a binary or ternary expression
	unsigned int right_precedence;

	while (peek_multary_op(right_precedence))
	{
		// Only process this operator if it has a lower precedence than the current operation, otherwise leave it for later and abort
		if (right_precedence <= left_precedence)
			break;

		// Finally consume the operator token
		consume();

		const tokenid op = _token.id;

		// Check if this is a binary or ternary operation
		if (op != tokenid::question)
		{
			#pragma region Binary Expression
			// Parse the right hand side of the binary operation
			expression rhs; spirv_basic_block rhs_block;
			if (!parse_expression_multary(rhs_block, rhs, right_precedence))
				return false;

			// Deduce the result base type based on implicit conversion rules
			type type = type::merge(lhs.type, rhs.type);
			bool is_bool_result = false;

			// Do some error checking depending on the operator
			if (op == tokenid::equal_equal || op == tokenid::exclaim_equal)
			{
				// Equality checks return a boolean value
				is_bool_result = true;

				// Cannot check equality between incompatible types
				if (lhs.type.is_array() || rhs.type.is_array() || lhs.type.definition != rhs.type.definition)
					return error(rhs.location, 3020, "type mismatch"), false;
			}
			else if (op == tokenid::ampersand || op == tokenid::pipe || op == tokenid::caret)
			{
				// Cannot perform bitwise operations on non-integral types
				if (!lhs.type.is_integral())
					return error(lhs.location, 3082, "int or unsigned int type required"), false;
				if (!rhs.type.is_integral())
					return error(rhs.location, 3082, "int or unsigned int type required"), false;
			}
			else
			{
				if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
					type.base = type::t_bool;

				// Logical operations return a boolean value
				if (op == tokenid::less || op == tokenid::less_equal || op == tokenid::greater || op == tokenid::greater_equal)
					is_bool_result = true;

				// Cannot perform arithmetic operations on non-basic types
				if (!lhs.type.is_scalar() && !lhs.type.is_vector() && !lhs.type.is_matrix())
					return error(lhs.location, 3022, "scalar, vector, or matrix expected"), false;
				if (!rhs.type.is_scalar() && !rhs.type.is_vector() && !rhs.type.is_matrix())
					return error(rhs.location, 3022, "scalar, vector, or matrix expected"), false;
			}

			// Perform implicit type conversion
			if (lhs.type.components() > type.components())
				warning(lhs.location, 3206, "implicit truncation of vector type");
			if (rhs.type.components() > type.components())
				warning(rhs.location, 3206, "implicit truncation of vector type");

			add_cast_operation(lhs, type);
			add_cast_operation(rhs, type);

			// Constant expressions can be evaluated at compile time
			if (lhs.is_constant && rhs.is_constant)
			{
				lhs.reset_to_rvalue_constant(lhs.location, evaluate_constant_expression(op, type, lhs.constant, rhs.constant), type);
				continue;
			}

			const auto lhs_value = access_chain_load(block, lhs);

#if RESHADEFX_SHORT_CIRCUIT
			// Short circuit for logical && and || operators
			if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
			{
				const auto merge_label = make_id();
				const auto lhs_block_label = _current_block;
				const auto rhs_block_label = make_id();

				if (op == tokenid::ampersand_ampersand)
				{
					// Emit "if ( lhs) result = rhs" for && expression
					leave_block_and_branch_conditional(block, lhs_value, rhs_block_label, merge_label);
				}
				else
				{
					// Emit "if (!lhs) result = rhs" for || expression
					const auto not_lhs_value = add_unary_operation(block, lhs.location, tokenid::exclaim, type, lhs_value);
					leave_block_and_branch_conditional(block, not_lhs_value, rhs_block_label, merge_label);
				}

				enter_block(block, rhs_block_label);
				block.append(std::move(rhs_block));
				// Only load value of right hand side expression after entering the second block
				const auto rhs_value = access_chain_load(block, rhs);
				leave_block_and_branch(block, merge_label);

				enter_block(block, merge_label);

				const auto result_value = add_phi_operation(block, type, lhs_value, lhs_block_label, rhs_value, rhs_block_label);

				lhs.reset_to_rvalue(lhs.location, result_value, type);
				continue;
			}
#endif
			block.append(std::move(rhs_block));

			const auto rhs_value = access_chain_load(block, rhs);

			// Certain operations return a boolean type instead of the type of the input expressions
			if (is_bool_result)
				type = { type::t_bool, type.rows, type.cols };

			const auto result_value = add_binary_operation(block, lhs.location, op, type, lhs.type, lhs_value, rhs_value);

			if (type.has(type::q_precise))
				add_decoration(result_value, spv::DecorationNoContraction);

			lhs.reset_to_rvalue(lhs.location, result_value, type);
			#pragma endregion
		}
		else
		{
			#pragma region Ternary Expression
			// A conditional expression needs a scalar or vector type condition
			if (!lhs.type.is_scalar() && !lhs.type.is_vector())
				return error(lhs.location, 3022, "boolean or vector expression expected"), false;

			// Parse the first part of the right hand side of the ternary operation
			expression true_exp; spirv_basic_block true_block;
			if (!parse_expression(true_block, true_exp))
				return false;

			if (!expect(':'))
				return false;

			// Parse the second part of the right hand side of the ternary operation
			expression false_exp; spirv_basic_block false_block;
			if (!parse_expression_assignment(false_block, false_exp))
				return false;

			// Check that the condition dimension matches that of at least one side
			if (lhs.type.is_vector() && lhs.type.rows != true_exp.type.rows && lhs.type.cols != true_exp.type.cols)
				return error(lhs.location, 3020, "dimension of conditional does not match value"), false;

			// Check that the two value expressions can be converted between each other
			if (true_exp.type.array_length != false_exp.type.array_length || true_exp.type.definition != false_exp.type.definition)
				return error(false_exp.location, 3020, "type mismatch between conditional values"), false;

			// Deduce the result base type based on implicit conversion rules
			const type type = type::merge(true_exp.type, false_exp.type);

			if (true_exp.type.components() > type.components())
				warning(true_exp.location, 3206, "implicit truncation of vector type");
			if (false_exp.type.components() > type.components())
				warning(false_exp.location, 3206, "implicit truncation of vector type");

			add_cast_operation(lhs, { type::t_bool, type.rows, 1 });
			add_cast_operation(true_exp, type);
			add_cast_operation(false_exp, type);

			// Load condition value from expression
			const auto condition_value = access_chain_load(block, lhs);

#if RESHADEFX_SHORT_CIRCUIT
			const auto merge_label = make_id();
			const auto true_block_label = make_id();
			const auto false_block_label = make_id();

			add_instruction_without_result(block, lhs.location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(spv::SelectionControlMaskNone); // Selection Control

			leave_block_and_branch_conditional(block, condition_value, true_block_label, false_block_label);

			enter_block(block, true_block_label);
			block.append(std::move(true_block));
			// Only load true expression value after entering the first block
			const auto true_value = access_chain_load(block, true_exp);
			leave_block_and_branch(block, merge_label);

			enter_block(block, false_block_label);
			block.append(std::move(false_block));
			// Only load false expression value after entering the second block
			const auto false_value = access_chain_load(block, false_exp);
			leave_block_and_branch(block, merge_label);

			enter_block(block, merge_label);

			const auto result_value = add_phi_operation(block, type, true_value, true_block_label, false_value, false_block_label);
#else
			block.append(std::move(true_block));
			block.append(std::move(false_block));

			const auto true_value = access_chain_load(block, true_exp);
			const auto false_value = access_chain_load(block, false_exp);

			const auto result_value = add_ternary_operation(block, lhs.location, op, type, condition_value, true_value, false_value);
#endif
			lhs.reset_to_rvalue(lhs.location, result_value, type);
			#pragma endregion
		}
	}

	return true;
}
bool reshadefx::parser::parse_expression_assignment(spirv_basic_block &block, expression &lhs)
{
	// Parse left hand side of the expression
	if (!parse_expression_multary(block, lhs))
		return false;

	// Check if an operator exists so that this is an assignment
	if (accept_assignment_op())
	{
		// Remember the operator token before parsing the expression that follows it
		const tokenid op = _token.id;

		// Parse right hand side of the assignment expression
		expression rhs;
		if (!parse_expression_multary(block, rhs))
			return false;

		// Check if the assignment is valid
		if (lhs.type.has(type::q_const) || lhs.type.has(type::q_uniform) || !lhs.is_lvalue)
			return error(lhs.location, 3025, "l-value specifies const object"), false;
		if (lhs.type.array_length != rhs.type.array_length || !type::rank(lhs.type, rhs.type))
			return error(rhs.location, 3020, "cannot convert these types"), false;
		// Cannot perform bitwise operations on non-integral types
		if (!lhs.type.is_integral() && (op == tokenid::ampersand_equal || op == tokenid::pipe_equal || op == tokenid::caret_equal))
			return error(lhs.location, 3082, "int or unsigned int type required"), false;

		// Perform implicit type conversion of right hand side value
		if (rhs.type.components() > lhs.type.components())
			warning(rhs.location, 3206, "implicit truncation of vector type");

		add_cast_operation(rhs, lhs.type);

		auto result = access_chain_load(block, rhs);

		// Check if this is an assignment with an additional arithmetic instruction
		if (op != tokenid::equal)
		{
			// Handle arithmetic assignment operation
			result = add_binary_operation(block, lhs.location, op, lhs.type, access_chain_load(block, lhs), result);

			if (lhs.type.has(type::q_precise))
				add_decoration(result, spv::DecorationNoContraction);
		}

		// Write result back to variable
		access_chain_store(block, lhs, result, lhs.type);

		// Return the result value since you can write assignments within expressions
		lhs.reset_to_rvalue(lhs.location, result, lhs.type);
	}

	return true;
}

bool reshadefx::parser::parse_annotations(std::unordered_map<std::string, std::string> &annotations)
{
	// Check if annotations exist and return early if none do
	if (!accept('<'))
		return true;

	bool success = true;

	while (!peek('>'))
	{
		if (type type; accept_type_class(type))
			warning(_token.location, 4717, "type prefixes for annotations are deprecated and ignored");

		if (!expect(tokenid::identifier))
			return false;

		const auto name = std::move(_token.literal_as_string);

		expression expression;

		// Pass in temporary basic block, the expression should be constant
		if (spirv_basic_block block; !expect('=') || !parse_expression_unary(block, expression) || !expect(';'))
			return false;

		if (!expression.is_constant) {
			error(expression.location, 3011, "value must be a literal expression");
			success = false; // Continue parsing annotations despite the error, since the syntax is still correct
			continue;
		}

		switch (expression.type.base)
		{
		case type::t_int:
			annotations[name] = std::to_string(expression.constant.as_int[0]);
			break;
		case type::t_uint:
			annotations[name] = std::to_string(expression.constant.as_uint[0]);
			break;
		case type::t_float:
			annotations[name] = std::to_string(expression.constant.as_float[0]);
			break;
		case type::t_string:
			annotations[name] = expression.constant.string_data;
			break;
		}
	}

	return expect('>') && success;
}

// -- Statement & Declaration Parsing -- //

bool reshadefx::parser::parse_statement(spirv_basic_block &block, bool scoped)
{
	if (_current_block == 0)
		return error(_token_next.location, 0, "unreachable code"), false;

	int loop_control = spv::LoopControlMaskNone;
	int selection_control = spv::SelectionControlMaskNone;

	// Read any loop and branch control attributes first
	while (accept('['))
	{
		const auto attribute = std::move(_token_next.literal_as_string);

		if (!expect(tokenid::identifier) || !expect(']'))
			return false;

		if (attribute == "unroll")
			loop_control |= spv::LoopControlUnrollMask;
		else if (attribute == "loop")
			loop_control |= spv::LoopControlDontUnrollMask;
		else if (attribute == "flatten")
			selection_control |= spv::SelectionControlFlattenMask;
		else if (attribute == "branch")
			selection_control |= spv::SelectionControlDontFlattenMask;
		else
			warning(_token.location, 0, "unknown attribute");

		if ((loop_control & (spv::LoopControlUnrollMask | spv::LoopControlDontUnrollMask)) == (spv::LoopControlUnrollMask | spv::LoopControlDontUnrollMask))
			return error(_token.location, 3524, "can't use loop and unroll attributes together"), false;
		if ((selection_control & (spv::SelectionControlFlattenMask | spv::SelectionControlDontFlattenMask)) == (spv::SelectionControlFlattenMask | spv::SelectionControlDontFlattenMask))
			return error(_token.location, 3524, "can't use branch and flatten attributes together"), false;
	}

	if (peek('{')) // Parse statement block
		return parse_statement_block(block, scoped);
	else if (accept(';')) // Ignore empty statements
		return true;

	// Most statements with the exception of declarations are only valid inside functions
	if (_current_function != std::numeric_limits<size_t>::max())
	{
		const auto location = _token_next.location;

		#pragma region If
		if (accept(tokenid::if_))
		{
			const auto merge_label = make_id();
			const auto true_block_label = make_id();
			const auto false_block_label = make_id();

			expression condition;
			if (!expect('(') || !parse_expression(block, condition) || !expect(')'))
				return false;
			else if (!condition.type.is_scalar())
				return error(condition.location, 3019, "if statement conditional expressions must evaluate to a scalar"), false;

			// Load condition and convert to boolean value as required by 'OpBranchConditional'
			add_cast_operation(condition, { type::t_bool, 1, 1 });

			const auto condition_value = access_chain_load(block, condition);

			add_instruction_without_result(block, location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(selection_control); // Selection Control

			leave_block_and_branch_conditional(block, condition_value, true_block_label, false_block_label);

			{ // Then block of the if statement
				enter_block(block, true_block_label);

				if (!parse_statement(block, true))
					return false;

				leave_block_and_branch(block, merge_label);
			}
			{ // Else block of the if statement
				enter_block(block, false_block_label);

				if (accept(tokenid::else_) && !parse_statement(block, true))
					return false;

				leave_block_and_branch(block, merge_label);
			}

			enter_block(block, merge_label);

			return true;
		}
		#pragma endregion

		#pragma region Switch
		if (accept(tokenid::switch_))
		{
			const auto merge_label = make_id();
			auto default_label = merge_label; // The default case jumps to the end of the switch statement if not overwritten

			expression selector;
			if (!expect('(') || !parse_expression(block, selector) || !expect(')'))
				return false;
			else if (!selector.type.is_scalar())
				return error(selector.location, 3019, "switch statement expression must evaluate to a scalar"), false;

			// Load selector and convert to integral value as required by 'OpSwitch'
			add_cast_operation(selector, { type::t_int, 1, 1 });

			const auto selector_value = access_chain_load(block, selector);

			// A switch statement leaves the current control flow block
			_current_block = 0;

			add_instruction_without_result(block, location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(selection_control); // Selection Control

			spirv_instruction &switch_instruction = add_instruction_without_result(block, location, spv::OpSwitch)
				.add(selector_value);

			if (!expect('{'))
				return false;

			_loop_break_target_stack.push_back(merge_label);
			on_scope_exit _([this]() { _loop_break_target_stack.pop_back(); });

			bool success = true;
			spv::Id current_block = 0;
			unsigned int num_case_labels = 0;
			std::vector<spv::Id> case_literal_and_labels;

			spirv_basic_block switch_body_block;

			while (!peek('}') && !peek(tokenid::end_of_file))
			{
				if (peek(tokenid::case_) || peek(tokenid::default_))
				{
					current_block = make_id();

					// Handle fall-through case
					if (num_case_labels != 0)
						leave_block_and_branch(switch_body_block, current_block);

					enter_block(switch_body_block, current_block);
				}

				while (accept(tokenid::case_) || accept(tokenid::default_))
				{
					if (_token.id == tokenid::case_)
					{
						expression case_label;
						if (!parse_expression(switch_body_block, case_label))
							return consume_until('}'), false;
						else if (!case_label.type.is_scalar() || !case_label.type.is_integral() || !case_label.is_constant)
							return error(case_label.location, 3020, "invalid type for case expression - value must be an integer scalar"), consume_until('}'), false;

						// Check for duplicate case values
						for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
						{
							if (case_literal_and_labels[i] == case_label.constant.as_uint[0])
							{
								error(case_label.location, 3532, "duplicate case " + std::to_string(case_label.constant.as_uint[0]));
								success = false;
								break;
							}
						}

						case_literal_and_labels.push_back(case_label.constant.as_uint[0]);
						case_literal_and_labels.push_back(current_block);
					}
					else
					{
						// Check if the default label was already changed by a previous 'default' statement
						if (default_label != merge_label)
						{
							error(_token.location, 3532, "duplicate default in switch statement");
							success = false;
						}

						default_label = current_block;
					}

					if (!expect(':'))
						return consume_until('}'), false;

					num_case_labels++;
				}

				// It is valid for no statement to follow if this is the last label in the switch body
				if (accept('}'))
					break;

				if (!parse_statement(switch_body_block, true))
					return consume_until('}'), false;
			}

			// May have left the switch body without an explicit 'break' at the end, so handle that case now
			leave_block_and_branch(switch_body_block, merge_label);

			if (num_case_labels == 0)
				warning(location, 5002, "switch statement contains no 'case' or 'default' labels");

			// Add all case labels to the switch instruction (reference is still valid because all other instructions were written to the intermediate 'switch_body_block' in the mean time)
			switch_instruction.add(default_label);
			switch_instruction.add(case_literal_and_labels.begin(), case_literal_and_labels.end());

			block.append(std::move(switch_body_block));

			enter_block(block, merge_label);

			return expect('}') && success;
		}
		#pragma endregion

		#pragma region For
		if (accept(tokenid::for_))
		{
			if (!expect('('))
				return false;

			enter_scope();
			on_scope_exit _([this]() { leave_scope(); });

			// Parse initializer first
			if (type type; parse_type(type))
			{
				unsigned int count = 0;
				do { // There may be multiple declarations behind a type, so loop through them
					if (count++ > 0 && !expect(','))
						return false;
					if (!expect(tokenid::identifier) || !parse_variable(type, std::move(_token.literal_as_string), block))
						return false;
				} while (!peek(';'));
			}
			else // Initializer can also contain an expression if not a variable declaration list
			{
				expression expression;
				parse_expression(block, expression); // It is valid for there to be no initializer expression, so ignore result
			}

			if (!expect(';'))
				return false;

			const auto header_label = make_id(); // Pointer to the loop merge instruction
			const auto loop_label = make_id(); // Pointer to the main loop body block
			const auto merge_label = make_id(); // Pointer to the end of the loop
			const auto continue_label = make_id(); // Pointer to the continue block
			const auto condition_label = make_id(); // Pointer to the condition check

			leave_block_and_branch(block, header_label);

			{ // Begin loop block
				enter_block(block, header_label);

				add_instruction_without_result(block, location, spv::OpLoopMerge)
					.add(merge_label) // Merge Block
					.add(continue_label) // Continue Target
					.add(loop_control); // Loop Control

				leave_block_and_branch(block, condition_label);
			}

			{ // Parse condition block
				enter_block(block, condition_label);

				expression condition;
				if (parse_expression(block, condition))
				{
					if (!condition.type.is_scalar())
						return error(condition.location, 3019, "scalar value expected"), false;

					// Evaluate condition and branch to the right target
					add_cast_operation(condition, { type::t_bool, 1, 1 });

					const auto condition_value = access_chain_load(block, condition);

					leave_block_and_branch_conditional(block, condition_value, loop_label, merge_label);
				}
				else // It is valid for there to be no condition expression
				{
					leave_block_and_branch(block, loop_label);
				}

				if (!expect(';'))
					return false;
			}

			spirv_basic_block continue_block;
			{ // Parse loop continue block into separate block so it can be appended to the end down the line
				enter_block(continue_block, continue_label);

				expression continue_exp;
				parse_expression(continue_block, continue_exp); // It is valid for there to be no continue expression, so ignore result

				if (!expect(')'))
					return false;

				// Branch back to the loop header at the end of the continue block
				leave_block_and_branch(continue_block, header_label);
			}

			{ // Parse loop body block
				enter_block(block, loop_label);

				_loop_break_target_stack.push_back(merge_label);
				_loop_continue_target_stack.push_back(continue_label);

				if (!parse_statement(block, false))
				{
					_loop_break_target_stack.pop_back();
					_loop_continue_target_stack.pop_back();
					return false;
				}

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				leave_block_and_branch(block, continue_label);
			}

			// Append continue block after the main block
			block.append(std::move(continue_block));

			// Add merge block label to the end of the loop
			enter_block(block, merge_label);

			return true;
		}
		#pragma endregion

		#pragma region While
		if (accept(tokenid::while_))
		{
			enter_scope();
			on_scope_exit _([this]() { leave_scope(); });

			const auto header_label = make_id();
			const auto loop_label = make_id();
			const auto merge_label = make_id();
			const auto continue_label = make_id();
			const auto condition_label = make_id();

			// End current block by branching to the next label
			leave_block_and_branch(block, header_label);

			{ // Begin loop block
				enter_block(block, header_label);

				add_instruction_without_result(block, location, spv::OpLoopMerge)
					.add(merge_label) // Merge Block
					.add(continue_label) // Continue Target
					.add(loop_control); // Loop Control

				leave_block_and_branch(block, condition_label);
			}

			{ // Parse condition block
				enter_block(block, condition_label);

				expression condition;
				if (!expect('(') || !parse_expression(block, condition) || !expect(')'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				add_cast_operation(condition, { type::t_bool, 1, 1 });

				const auto condition_value = access_chain_load(block, condition);

				leave_block_and_branch_conditional(block, condition_value, loop_label, merge_label);
			}

			{ // Parse loop body block
				enter_block(block, loop_label);

				_loop_break_target_stack.push_back(merge_label);
				_loop_continue_target_stack.push_back(continue_label);

				if (!parse_statement(block, false))
				{
					_loop_break_target_stack.pop_back();
					_loop_continue_target_stack.pop_back();
					return false;
				}

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				leave_block_and_branch(block, continue_label);
			}

			{ // Branch back to the loop header in empty continue block
				enter_block(block, continue_label);

				leave_block_and_branch(block, header_label);
			}

			// Add merge block label to the end of the loop
			enter_block(block, merge_label);

			return true;
		}
		#pragma endregion

		#pragma region DoWhile
		if (accept(tokenid::do_))
		{
			const auto header_label = make_id();
			const auto loop_label = make_id();
			const auto merge_label = make_id();
			const auto continue_label = make_id();

			// End current block by branching to the next label
			leave_block_and_branch(block, header_label);

			{ // Begin loop block
				enter_block(block, header_label);

				add_instruction_without_result(block, location, spv::OpLoopMerge)
					.add(merge_label) // Merge Block
					.add(continue_label) // Continue Target
					.add(loop_control); // Loop Control

				leave_block_and_branch(block, loop_label);
			}

			{ // Parse loop body block
				enter_block(block, loop_label);

				_loop_break_target_stack.push_back(merge_label);
				_loop_continue_target_stack.push_back(continue_label);

				if (!parse_statement(block, true))
				{
					_loop_break_target_stack.pop_back();
					_loop_continue_target_stack.pop_back();
					return false;
				}

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				leave_block_and_branch(block, continue_label);
			}

			{ // Continue block does the condition evaluation
				enter_block(block, continue_label);

				expression condition;
				if (!expect(tokenid::while_) || !expect('(') || !parse_expression(block, condition) || !expect(')') || !expect(';'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				add_cast_operation(condition, { type::t_bool, 1, 1 });

				const auto condition_value = access_chain_load(block, condition);

				leave_block_and_branch_conditional(block, condition_value, header_label, merge_label);
			}

			// Add merge block label to the end of the loop
			enter_block(block, merge_label);

			return true;
		}
		#pragma endregion

		#pragma region Break
		if (accept(tokenid::break_))
		{
			if (_loop_break_target_stack.empty())
				return error(location, 3518, "break must be inside loop"), false;

			// Branch to the break target of the inner most loop on the stack
			leave_block_and_branch(block, _loop_break_target_stack.back());

			return expect(';');
		}
		#pragma endregion

		#pragma region Continue
		if (accept(tokenid::continue_))
		{
			if (_loop_continue_target_stack.empty())
				return error(location, 3519, "continue must be inside loop"), false;

			// Branch to the continue target of the inner most loop on the stack
			leave_block_and_branch(block, _loop_continue_target_stack.back());

			return expect(';');
		}
		#pragma endregion

		#pragma region Return
		if (accept(tokenid::return_))
		{
			const auto parent = _functions[_current_function].get();

			if (!peek(';'))
			{
				expression expression;
				if (!parse_expression(block, expression))
					return consume_until(';'), false;

				// Cannot return to void
				if (parent->return_type.is_void())
					// Consume the semicolon that follows the return expression so that parsing may continue
					return error(location, 3079, "void functions cannot return a value"), accept(';'), false;

				// Cannot return arrays from a function
				if (expression.type.is_array() || !type::rank(expression.type, parent->return_type))
					return error(location, 3017, "expression does not match function return type"), accept(';'), false;

				// Load return value and perform implicit cast to function return type
				if (expression.type.components() > parent->return_type.components())
					warning(expression.location, 3206, "implicit truncation of vector type");

				add_cast_operation(expression, parent->return_type);

				const auto return_value = access_chain_load(block, expression);

				leave_block_and_return(block, return_value);
			}
			else if (!parent->return_type.is_void())
			{
				// No return value was found, but the function expects one
				error(location, 3080, "function must return a value");

				// Consume the semicolon that follows the return expression so that parsing may continue
				accept(';');

				return false;
			}
			else
			{
				leave_block_and_return(block, 0);
			}

			return expect(';');
		}
		#pragma endregion

		#pragma region Discard
		if (accept(tokenid::discard_))
		{
			// Leave the current function block
			leave_block_and_kill(block);

			return expect(';');
		}
		#pragma endregion
	}

	#pragma region Declaration
	// Handle variable declarations
	if (type type; parse_type(type))
	{
		unsigned int count = 0;
		do { // There may be multiple declarations behind a type, so loop through them
			if (count++ > 0 && !expect(','))
				// Try to consume the rest of the declaration so that parsing may continue despite the error
				return consume_until(';'), false;
			if (!expect(tokenid::identifier) || !parse_variable(type, std::move(_token.literal_as_string), block))
				return consume_until(';'), false;
		} while (!peek(';'));

		return expect(';');
	}
	#pragma endregion

	// Handle expression statements
	if (expression expression; parse_expression(block, expression))
		return expect(';'); // A statement has to be terminated with a semicolon

	// No token should come through here, since all statements and expressions should have been handled above, so this is an error in the syntax
	error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "'");

	// Gracefully consume any remaining characters until the statement would usually end, so that parsing may continue despite the error
	consume_until(';');

	return false;
}
bool reshadefx::parser::parse_statement_block(spirv_basic_block &block, bool scoped)
{
	if (!expect('{'))
		return false;

	if (scoped)
		enter_scope();

	// Parse statements until the end of the block is reached
	while (!peek('}') && !peek(tokenid::end_of_file))
	{
		if (!parse_statement(block, true))
		{
			if (scoped)
				leave_scope();

			// Ignore the rest of this block
			unsigned int level = 0;

			while (!peek(tokenid::end_of_file))
			{
				if (accept('{'))
				{
					++level;
				}
				else if (accept('}'))
				{
					if (level-- == 0)
						break;
				} // These braces are necessary to match the 'else' to the correct 'if' statement
				else
				{
					consume();
				}
			}

			return false;
		}
	}

	if (scoped)
		leave_scope();

	return expect('}');
}

bool reshadefx::parser::parse_top()
{
	if (accept(tokenid::namespace_))
	{
		// Anonymous namespaces are not supported right now, so an identifier is a must
		if (!expect(tokenid::identifier))
			return false;

		const auto name = std::move(_token.literal_as_string);

		if (!expect('{'))
			return false;

		enter_namespace(name);

		bool success = true;
		// Recursively parse top level statements until the namespace is closed again
		while (!peek('}')) // Empty namespaces are valid
			if (!parse_top())
				success = false; // Continue parsing even after encountering an error

		leave_namespace();

		return expect('}') && success;
	}
	else if (accept(tokenid::struct_)) // Structure keyword found, parse the structure definition
	{
		if (!parse_struct() || !expect(';')) // Structure definitions are terminated with a semicolon
			return false;
	}
	else if (accept(tokenid::technique)) // Technique keyword found, parse the technique definition
	{
		if (!parse_technique())
			return false;
	}
	else if (type type; parse_type(type)) // Type found, this can be either a variable or a function declaration
	{
		if (!expect(tokenid::identifier))
			return false;

		if (peek('('))
		{
			const auto name = std::move(_token.literal_as_string);
			// This is definitely a function declaration, so parse it
			if (!parse_function(type, name)) {
				// Insert dummy function into symbol table, so later references can be resolved despite the error
				insert_symbol(name, { spv::OpFunction, std::numeric_limits<spv::Id>::max(), { type::t_function } }, true);
				return false;
			}
		}
		else
		{
			// There may be multiple variable names after the type, handle them all
			unsigned int count = 0;
			// Global variables can't have non-constant initializers, so pass in a throwaway basic block
			spirv_basic_block block;
			do {
				if (count++ > 0 && !(expect(',') && expect(tokenid::identifier)))
					return false;
				const auto name = std::move(_token.literal_as_string);
				if (!parse_variable(type, name, block, true)) {
					// Insert dummy variable into symbol table, so later references can be resolved despite the error
					insert_symbol(name, { spv::OpVariable, std::numeric_limits<spv::Id>::max(), type }, true);
					return consume_until(';'), false; // Skip the rest of the statement in case of an error
				}
			} while (!peek(';'));

			if (!expect(';')) // Variable declarations are terminated with a semicolon
				return false;
		}
	}
	else if (!accept(';')) // Ignore single semicolons in the source
	{
		consume(); // Unexpected token in source stream, consume and report an error about it
		error(_token.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token.id) + "'");
		return false;
	}

	return true;
}

bool reshadefx::parser::parse_struct()
{
	const auto location = std::move(_token.location);

	spirv_struct_info info;

	// The structure name is optional
	if (accept(tokenid::identifier))
		info.name = _token.literal_as_string;
	else
		info.name = "__anonymous_struct_" + std::to_string(location.line) + '_' + std::to_string(location.column);

	info.unique_name = 'S' + current_scope().name + info.name;
	std::replace(info.unique_name.begin(), info.unique_name.end(), ':', '_');

	if (!expect('{'))
		return false;

	std::vector<spv::Id> member_types;

	while (!peek('}')) // Empty structures are possible
	{
		// Parse structure members
		type member_type;
		if (!parse_type(member_type))
			return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected struct member type"), consume_until('}'), false;

		if (member_type.is_void())
			return error(_token_next.location, 3038, "struct members cannot be void"), consume_until('}'), false;
		if (member_type.has(type::q_in) || member_type.has(type::q_out))
			return error(_token_next.location, 3055, "struct members cannot be declared 'in' or 'out'"), consume_until('}'), false;

		if (member_type.is_struct()) // Nesting structures would make input/output argument flattening more complicated, so prevent it for now
			return error(_token_next.location, 3090, "nested struct members are not supported"), consume_until('}'), false;

		unsigned int count = 0;
		do {
			if (count++ > 0 && !expect(','))
				return consume_until('}'), false;
			if (!expect(tokenid::identifier))
				return consume_until('}'), false;

			spirv_struct_member_info member_info;
			member_info.name = std::move(_token.literal_as_string);
			member_info.type = member_type;

			// Modify member specific type, so that following members in the declaration list are not affected by this
			if (!parse_array_size(member_info.type))
				return consume_until('}'), false;

			// Structure members may have semantics to use them as input/output types
			if (accept(':'))
			{
				if (!expect(tokenid::identifier))
					return consume_until('}'), false;

				member_info.semantic = std::move(_token.literal_as_string);
				// Make semantic upper case to simplify comparison later on
				std::transform(member_info.semantic.begin(), member_info.semantic.end(), member_info.semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
			}

			// Add member type to list
			member_types.push_back(convert_type(member_info.type));

			// Save member name and type for book keeping
			info.member_list.push_back(std::move(member_info));
		} while (!peek(';'));

		if (!expect(';'))
			return consume_until('}'), false;
	}

	// Empty structures are valid, but not usually intended, so emit a warning
	if (member_types.empty())
		warning(location, 5001, "struct has no members");

	// Define the structure now that information about all the member types was gathered
	info.definition = make_id();
	define_struct(info.definition, info.unique_name.c_str(), location, member_types);

	_structs[info.definition] = info;

	for (uint32_t i = 0; i < info.member_list.size(); ++i)
	{
		add_member_name(info.definition, i, info.member_list[i].name.c_str());
	}

	// Insert the symbol into the symbol table
	const symbol symbol = { spv::OpTypeStruct, info.definition };

	if (!insert_symbol(info.name, symbol, true))
		return error(location, 3003, "redefinition of '" + info.name + "'"), false;

	return expect('}');
}

bool reshadefx::parser::parse_function(type type, std::string name)
{
	const auto location = std::move(_token.location);

	if (!expect('(')) // Functions always have a parameter list
		return false;
	if (type.qualifiers != 0)
		return error(location, 3047, "function return type cannot have any qualifiers"), false;

	spirv_function_info &info = *_functions.emplace_back(new spirv_function_info());
	info.name = name;
	info.unique_name = 'F' + current_scope().name + name;
	std::replace(info.unique_name.begin(), info.unique_name.end(), ':', '_');
	info.return_type = type;

	// Add function instruction and insert the symbol into the symbol table
	define_function(info.definition = make_id(), info.unique_name.c_str(), location, type);

	// Enter function scope
	enter_scope(); on_scope_exit _([this]() { leave_scope(); leave_function(); });

	while (!peek(')'))
	{
		if (!info.parameter_list.empty() && !expect(','))
			return false;

		spirv_struct_member_info param;

		if (!parse_type(param.type))
			return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected parameter type"), false;

		if (!expect(tokenid::identifier))
			return false;

		param.name = _token.literal_as_string;
		const auto param_location = _token.location;

		if (param.type.is_void())
			return error(param_location, 3038, "function parameters cannot be void"), false;
		if (param.type.has(type::q_extern))
			return error(param_location, 3006, "function parameters cannot be declared 'extern'"), false;
		if (param.type.has(type::q_static))
			return error(param_location, 3007, "function parameters cannot be declared 'static'"), false;
		if (param.type.has(type::q_uniform))
			return error(param_location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead"), false;

		if (param.type.has(type::q_out) && param.type.has(type::q_const))
			return error(param_location, 3046, "output parameters cannot be declared 'const'"), false;
		else if (!param.type.has(type::q_out))
			param.type.qualifiers |= type::q_in; // Function parameters are implicitly 'in' if not explicitly defined as 'out'

		if (!parse_array_size(param.type))
			return false;

		// Handle parameter type semantic
		if (accept(':'))
		{
			if (!expect(tokenid::identifier))
				return false;

			param.semantic = std::move(_token.literal_as_string);
			// Make semantic upper case to simplify comparison later on
			std::transform(param.semantic.begin(), param.semantic.end(), param.semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
		}

		param.type.is_ptr = true;

		const auto definition = make_id();
		define_function_param(definition, param.name.c_str(), param_location, param.type);

		if (!insert_symbol(param.name, { spv::OpVariable, definition, param.type }))
			return error(param_location, 3003, "redefinition of '" + param.name + "'"), false;

		info.parameter_list.push_back(std::move(param));
	}

	if (!expect(')'))
		return false;

	// Handle return type semantic
	if (accept(':'))
	{
		if (!expect(tokenid::identifier))
			return false;
		if (type.is_void())
			return error(_token.location, 3076, "void function cannot have a semantic"), false;

		info.return_semantic = std::move(_token.literal_as_string);
		// Make semantic upper case to simplify comparison later on
		std::transform(info.return_semantic.begin(), info.return_semantic.end(), info.return_semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
	}

	// Check if this is a function declaration without a body
	if (accept(';'))
		return error(location, 3510, "function is missing an implementation"), false;

	// A function has to start with a new block
	enter_block(_functions2[_current_function].variables, make_id());

	const bool success = parse_statement_block(_functions2[_current_function].definition, false);

	// Add implicit return statement to the end of functions
	if (_current_block != 0)
		leave_block_and_return(_functions2[_current_function].definition, 0);

	// Insert the symbol into the symbol table
	symbol symbol = { spv::OpFunction, info.definition, { type::t_function } };
	symbol.function = &info;

	if (!insert_symbol(name, symbol, true))
		return error(location, 3003, "redefinition of '" + name + "'"), false;

	return success;
}

bool reshadefx::parser::parse_variable(type type, std::string name, spirv_basic_block &block, bool global)
{
	const auto location = std::move(_token.location);

	if (type.is_void())
		return error(location, 3038, "variables cannot be void"), false;
	if (type.has(type::q_in) || type.has(type::q_out))
		return error(location, 3055, "variables cannot be declared 'in' or 'out'"), false;

	// Local and global variables have different requirements
	if (global)
	{
		// Check that type qualifier combinations are valid
		if (type.has(type::q_static))
		{
			// Global variables that are 'static' cannot be of another storage class
			if (type.has(type::q_uniform))
				return error(location, 3007, "uniform global variables cannot be declared 'static'"), false;
			// The 'volatile qualifier is only valid memory object declarations that are storage images or uniform blocks
			if (type.has(type::q_volatile))
				return error(location, 3008, "global variables cannot be declared 'volatile'"), false;
		}
		else
		{
			// Make all global variables 'uniform' by default, since they should be externally visible without the 'static' keyword
			if (!type.has(type::q_uniform) && !(type.is_texture() || type.is_sampler()))
				warning(location, 5000, "global variables are considered 'uniform' by default");

			// Global variables that are not 'static' are always 'extern' and 'uniform'
			type.qualifiers |= type::q_extern | type::q_uniform;

			// It is invalid to make 'uniform' variables constant, since they can be modified externally
			if (type.has(type::q_const))
				return error(location, 3035, "variables which are 'uniform' cannot be declared 'const'"), false;
		}
	}
	else
	{
		if (type.has(type::q_extern))
			return error(location, 3006, "local variables cannot be declared 'extern'"), false;
		if (type.has(type::q_uniform))
			return error(location, 3047, "local variables cannot be declared 'uniform'"), false;

		if (type.is_texture() || type.is_sampler())
			return error(location, 3038, "local variables cannot be textures or samplers"), false;
	}

	// The variable name may be followed by an optional array size expression
	if (!parse_array_size(type))
		return false;

	std::string semantic;
	expression initializer;
	spirv_texture_info texture_info;
	spirv_sampler_info sampler_info;

	if (accept(':'))
	{
		if (!expect(tokenid::identifier))
			return false;
		else if (!global) // Only global variables can have a semantic
			return error(_token.location, 3043, "local variables cannot have semantics"), false;

		semantic = std::move(_token.literal_as_string);
		// Make semantic upper case to simplify comparison later on
		std::transform(semantic.begin(), semantic.end(), semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
	}
	else
	{
		// Global variables can have optional annotations
		if (global && !parse_annotations(texture_info.annotations))
			return false;

		// Variables without a semantic may have an optional initializer
		if (accept('='))
		{
			if (!parse_expression_assignment(block, initializer))
				return false;

			if (global && !initializer.is_constant) // TODO: This could be resolved by initializing these at the beginning of the entry point
				return error(initializer.location, 3011, "initial value must be a literal expression"), false;

			// Check type compatibility
			if ((type.array_length >= 0 && initializer.type.array_length != type.array_length) || !type::rank(initializer.type, type))
				return error(initializer.location, 3017, "initial value does not match variable type"), false;
			if ((initializer.type.rows < type.rows || initializer.type.cols < type.cols) && !initializer.type.is_scalar())
				return error(initializer.location, 3017, "cannot implicitly convert these vector types"), false;

			// Deduce array size from the initializer expression
			if (initializer.type.is_array())
				type.array_length = initializer.type.array_length;

			// Perform implicit cast from initializer expression to variable type
			if (initializer.type.components() > type.components())
				warning(initializer.location, 3206, "implicit truncation of vector type");

			add_cast_operation(initializer, type);
		}
		else if (type.is_numeric()) // Numeric variables without an initializer need special handling
		{
			if (type.has(type::q_const)) // Constants have to have an initial value
				return error(location, 3012, "missing initial value for '" + name + "'"), false;
			else if (!type.has(type::q_uniform)) // Zero initialize all global variables
				initializer.reset_to_rvalue_constant(location, {}, type);
		}
		else if (global && accept('{')) // Textures and samplers can have a property block attached to their declaration
		{
			while (!peek('}'))
			{
				if (!expect(tokenid::identifier))
					return consume_until('}'), false;

				const auto property_name = std::move(_token.literal_as_string);
				const auto property_location = std::move(_token.location);

				if (!expect('='))
					return consume_until('}'), false;

				backup();

				expression expression;

				if (accept(tokenid::identifier)) // Handle special enumeration names for property values
				{
					// Transform identifier to uppercase to do case-insensitive comparison
					std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), [](char c) { return static_cast<char>(toupper(c)); });

					static const std::pair<const char *, uint32_t> s_values[] = {
						{ "NONE", 0 }, { "POINT", 0 }, { "LINEAR", 1 }, { "ANISOTROPIC", 3 },
						{ "WRAP", 1 }, { "REPEAT", 1 }, { "MIRROR", 2 }, { "CLAMP", 3 }, { "BORDER", 4 },
						{ "R8", 1 }, { "R16F", 2 }, { "R32F", 3 }, { "RG8", 4 }, { "R8G8", 4 }, { "RG16", 5 }, { "R16G16", 5 }, { "RG16F", 6 }, { "R16G16F", 6 }, { "RG32F", 7 }, { "R32G32F", 7 },
						{ "RGBA8", 8 }, { "R8G8B8A8", 8 }, { "RGBA16", 9 }, { "R16G16B16A16", 9 }, { "RGBA16F", 10 }, { "R16G16B16A16F", 10 }, { "RGBA32F", 11 }, { "R32G32B32A32F", 11 },
						{ "DXT1", 12 }, { "DXT3", 13 }, { "DXT4", 14 }, { "LATC1", 15 }, { "LATC2", 16 },
					};

					// Look up identifier in list of possible enumeration names
					const auto it = std::find_if(std::begin(s_values), std::end(s_values),
						[this](const auto &it) { return it.first == _token.literal_as_string; });

					if (it != std::end(s_values))
						expression.reset_to_rvalue_constant(_token.location, it->second);
					else // No match found, so rewind to parser state before the identifier was consumed and try parsing it as a normal expression
						restore();
				}

				// Parse right hand side as normal expression if no special enumeration name was matched already
				if (spirv_basic_block temp_block; !expression.is_constant && !parse_expression_multary(temp_block, expression))
					return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected expression"), consume_until('}'), false;

				if (property_name == "Texture")
				{
					if (!expression.type.is_texture())
						return error(expression.location, 3020, "type mismatch, expected texture name"), consume_until('}'), false;

					sampler_info.texture_name = std::find_if(_textures.begin(), _textures.end(),
						[&expression](const auto &it) { return it.id == expression.base; })->unique_name;
				}
				else
				{
					if (!expression.is_constant || !expression.type.is_scalar())
						return error(expression.location, 3538, "value must be a literal scalar expression"), consume_until('}'), false;

					// All states below expect the value to be of an unsigned integer type
					add_cast_operation(expression, { type::t_uint, 1, 1 });
					const unsigned int value = expression.constant.as_uint[0];

					if (property_name == "Width")
						texture_info.width = value > 0 ? value : 1;
					else if (property_name == "Height")
						texture_info.height = value > 0 ? value : 1;
					else if (property_name == "MipLevels")
						texture_info.levels = value > 0 ? value : 1;
					else if (property_name == "Format")
						texture_info.format = value;
					else if (property_name == "SRGBTexture" || property_name == "SRGBReadEnable")
						sampler_info.srgb = value != 0;
					else if (property_name == "AddressU")
						sampler_info.address_u = value;
					else if (property_name == "AddressV")
						sampler_info.address_v = value;
					else if (property_name == "AddressW")
						sampler_info.address_w = value;
					else if (property_name == "MinFilter")
						sampler_info.filter = (sampler_info.filter & 0x0F) | ((value << 4) & 0x30); // Combine sampler filter components into a single filter enumeration value
					else if (property_name == "MagFilter")
						sampler_info.filter = (sampler_info.filter & 0x33) | ((value << 2) & 0x0C);
					else if (property_name == "MipFilter")
						sampler_info.filter = (sampler_info.filter & 0x3C) | (value & 0x03);
					else if (property_name == "MinLOD" || property_name == "MaxMipLevel")
						sampler_info.min_lod = static_cast<float>(value);
					else if (property_name == "MaxLOD")
						sampler_info.max_lod = static_cast<float>(value);
					else if (property_name == "MipLODBias" || property_name == "MipMapLodBias")
						sampler_info.lod_bias = static_cast<float>(value);
					else
						return error(property_location, 3004, "unrecognized property '" + property_name + "'"), consume_until('}'), false;
				}

				if (!expect(';'))
					return consume_until('}'), false;
			}

			if (!expect('}'))
				return false;
		}
	}

	symbol symbol;

	if (type.is_numeric() && type.has(type::q_const) && initializer.is_constant) // Variables with a constant initializer and constant type are named constants
	{
		// Named constants are special symbols
		symbol = { spv::OpConstant, 0, type, initializer.constant };
	}
	else if (type.is_texture())
	{
		assert(global);

		symbol = { spv::OpVariable, make_id(), type };

		// Add namespace scope to avoid name clashes
		texture_info.unique_name = 'V' + current_scope().name + name;
		std::replace(texture_info.unique_name.begin(), texture_info.unique_name.end(), ':', '_');

		texture_info.id = symbol.id;
		texture_info.semantic = std::move(semantic);

		_textures.push_back(texture_info);
	}
	else if (type.is_sampler()) // Samplers are actually combined image samplers
	{
		assert(global);

		if (sampler_info.texture_name.empty())
			return error(location, 3012, "missing 'Texture' property for '" + name + "'"), false;

		type.is_ptr = true; // Variables are always pointers

		symbol = { spv::OpVariable, make_id(), type };

		// Add namespace scope to avoid name clashes
		sampler_info.unique_name = 'V' + current_scope().name + name;
		std::replace(sampler_info.unique_name.begin(), sampler_info.unique_name.end(), ':', '_');

		sampler_info.id = symbol.id;
		sampler_info.set = 1;
		sampler_info.binding = _current_sampler_binding;
		sampler_info.annotations = std::move(texture_info.annotations);

		_samplers.push_back(sampler_info);

		define_variable(symbol.id, sampler_info.unique_name.c_str(), location, type, spv::StorageClassUniformConstant);

		add_decoration(symbol.id, spv::DecorationBinding, { _current_sampler_binding++ });
		add_decoration(symbol.id, spv::DecorationDescriptorSet, { 1 });
	}
	else if (type.has(type::q_uniform)) // Uniform variables are put into a global uniform buffer structure
	{
		assert(global);

		if (_global_ubo_type == 0)
			_global_ubo_type = make_id();
		if (_global_ubo_variable == 0)
			_global_ubo_variable = make_id();

		// Convert boolean uniform variables to integer type so that they have a defined size
		if (type.is_boolean())
			type.base = type::t_uint;

		symbol = { spv::OpAccessChain, _global_ubo_variable, type };

		// Add namespace scope to avoid name clashes
		std::string unique_name = 'U' + current_scope().name + name;
		std::replace(unique_name.begin(), unique_name.end(), ':', '_');

		spirv_uniform_info uniform_info;
		uniform_info.name = name;
		uniform_info.type_id = convert_type(type);
		uniform_info.struct_type_id = _global_ubo_type;

		uniform_info.annotations = std::move(texture_info.annotations);

		const size_t member_index = uniform_info.member_index = _uniforms.size();

		_uniforms.push_back(std::move(uniform_info));

		symbol.function = reinterpret_cast<const spirv_function_info *>(member_index);

		add_member_name(_global_ubo_type, member_index, unique_name.c_str());

		// GLSL specification on std140 layout:
		// 1. If the member is a scalar consuming N basic machine units, the base alignment is N.
		// 2. If the member is a two- or four-component vector with components consuming N basic machine units, the base alignment is 2N or 4N, respectively.
		// 3. If the member is a three-component vector with components consuming N basic machine units, the base alignment is 4N.
		size_t size = 4 * (type.rows == 3 ? 4 : type.rows) * type.cols * std::max(1, type.array_length);
		size_t alignment = size;
		_global_ubo_offset = align(_global_ubo_offset, alignment);
		add_member_decoration(_global_ubo_type, member_index, spv::DecorationOffset, { _global_ubo_offset });
		_global_ubo_offset += size;
	}
	else // All other variables are separate entities
	{
		type.is_ptr = true; // Variables are always pointers

		symbol = { spv::OpVariable, make_id(), type };

		// Update global variable names to contain the namespace scope to avoid name clashes
		std::string unique_name = global ? 'V' + current_scope().name + name : name;
		std::replace(unique_name.begin(), unique_name.end(), ':', '_');

		if (initializer.is_constant) // The initializer expression for 'OpVariable' must be a constant
		{
			define_variable(symbol.id, unique_name.c_str(), location, type, global ? spv::StorageClassPrivate : spv::StorageClassFunction, convert_constant(initializer.type, initializer.constant));
		}
		else // Non-constant initializers are explicitly stored in the variable at the definition location instead
		{
			const auto initializer_value = access_chain_load(block, initializer);

			define_variable(symbol.id, unique_name.c_str(), location, type, global ? spv::StorageClassPrivate : spv::StorageClassFunction);

			if (initializer_value != 0)
			{
				assert(!global); // Global variables cannot have a dynamic initializer

				expression variable; variable.reset_to_lvalue(location, symbol.id, type);

				access_chain_store(block, variable, initializer_value, initializer.type);
			}
		}
	}

	// Insert the symbol into the symbol table
	if (!insert_symbol(name, symbol, global))
		return error(location, 3003, "redefinition of '" + name + "'"), false;

	return true;
}

bool reshadefx::parser::parse_technique()
{
	if (!expect(tokenid::identifier))
		return false;

	spirv_technique_info info;
	info.name = std::move(_token.literal_as_string);

	if (!parse_annotations(info.annotations) || !expect('{'))
		return false;

	while (!peek('}'))
	{
		if (spirv_pass_info pass; parse_technique_pass(pass))
			info.passes.push_back(std::move(pass));
		else if (!peek(tokenid::pass)) // If there is another pass definition following, try to parse that despite the error
			return consume_until('}'), false;
	}

	_techniques.push_back(std::move(info));

	return expect('}');
}

bool reshadefx::parser::parse_technique_pass(spirv_pass_info &info)
{
	if (!expect(tokenid::pass))
		return false;

	// Passes can have an optional name, so consume and ignore that if it exists
	accept(tokenid::identifier);

	if (!expect('{'))
		return false;

	while (!peek('}'))
	{
		// Parse pass states
		if (!expect(tokenid::identifier))
			return consume_until('}'), false;

		auto location = std::move(_token.location);
		const auto state = std::move(_token.literal_as_string);

		if (!expect('='))
			return consume_until('}'), false;

		const bool is_shader_state = state == "VertexShader" || state == "PixelShader";
		const bool is_texture_state = state.compare(0, 12, "RenderTarget") == 0 && (state.size() == 12 || (state[12] >= '0' && state[12] < '8'));

		// Shader and render target assignment looks up values in the symbol table, so handle those separately from the other states
		if (is_shader_state || is_texture_state)
		{
			// Starting an identifier with '::' restricts the symbol search to the global namespace level
			const bool exclusive = accept(tokenid::colon_colon);

			std::string identifier;

			if (expect(tokenid::identifier))
				identifier = std::move(_token.literal_as_string);
			else
				return consume_until('}'), false;

			// Can concatenate multiple '::' to force symbol search for a specific namespace level
			while (accept(tokenid::colon_colon))
			{
				if (!expect(tokenid::identifier))
					return consume_until('}'), false;

				identifier += "::" + _token.literal_as_string;
			}

			location = std::move(_token.location);

			// Figure out which scope to start searching in
			scope scope = { "::", 0, 0 };
			if (!exclusive) scope = current_scope();

			// Lookup name in the symbol table
			const symbol symbol = find_symbol(identifier, scope, exclusive);

			if (is_shader_state)
			{
				if (!symbol.id)
					return error(location, 3501, "undeclared identifier '" + identifier + "', expected function name"), consume_until('}'), false;
				else if (!symbol.type.is_function())
					return error(location, 3020, "type mismatch, expected function name"), consume_until('}'), false;

				// Ignore invalid functions that were added during error recovery
				if (symbol.id == 0xFFFFFFFF)
					return consume_until('}'), false;

				const bool is_vs = state[0] == 'V';
				const bool is_ps = state[0] == 'P';

				// Look up the matching function info for this function definition
				spirv_function_info *const function_info = std::find_if(_functions.begin(), _functions.end(),
					[&symbol](const auto &it) { return it->definition == symbol.id; })->get();

				// We need to generate a special entry point function which translates between function parameters and input/output variables
				if (function_info->entry_point == 0)
				{
					std::vector<spv::Id> inputs_and_outputs, call_params;

					// Generate the glue entry point function
					function_info->entry_point = make_id();
					define_function(function_info->entry_point, nullptr, location, { type::t_void });

					enter_block(_functions2[_current_function].variables, make_id());

					spirv_basic_block &section = _functions2[_current_function].definition;

					const auto semantic_to_builtin = [is_ps](const std::string &semantic, spv::BuiltIn &builtin) {
						builtin = spv::BuiltInMax;
						if (semantic == "SV_POSITION")
							builtin = is_ps ? spv::BuiltInFragCoord : spv::BuiltInPosition;
						if (semantic == "SV_POINTSIZE")
							builtin = spv::BuiltInPointSize;
						if (semantic == "SV_DEPTH")
							builtin = spv::BuiltInFragDepth;
						if (semantic == "VERTEXID" || semantic == "SV_VERTEXID")
							builtin = spv::BuiltInVertexId;
						return builtin != spv::BuiltInMax;
					};

					const auto create_input_param = [this, &section, &call_params](const spirv_struct_member_info &param) {
						const spv::Id function_variable = make_id();
						define_variable(function_variable, nullptr, {}, param.type, spv::StorageClassFunction);
						call_params.push_back(function_variable);
						return function_variable;
					};
					const auto create_input_variable = [this, &inputs_and_outputs, &semantic_to_builtin](const spirv_struct_member_info &param) {
						type input_type = param.type;
						input_type.is_input = true;
						input_type.is_ptr = true;

						const spv::Id input_variable = make_id();
						define_variable(input_variable, nullptr, {}, input_type, spv::StorageClassInput);

						if (spv::BuiltIn builtin; semantic_to_builtin(param.semantic, builtin))
							add_builtin(input_variable, builtin);
						else
						{
							uint32_t location = 0;
							if (param.semantic.size() >= 5 && param.semantic.compare(0, 5, "COLOR") == 0)
								location = std::strtol(param.semantic.substr(5).c_str(), nullptr, 10);
							else if (param.semantic.size() >= 9 && param.semantic.compare(0, 9, "SV_TARGET") == 0)
								location = std::strtol(param.semantic.substr(9).c_str(), nullptr, 10);
							else if (param.semantic.size() >= 8 && param.semantic.compare(0, 8, "TEXCOORD") == 0)
								location = std::strtol(param.semantic.substr(8).c_str(), nullptr, 10);
							else if (const auto it = _semantic_to_location.find(param.semantic); it != _semantic_to_location.end())
								location = it->second;
							else
								_semantic_to_location[param.semantic] = location = _current_semantic_location++;

							add_decoration(input_variable, spv::DecorationLocation, { location });
						}

						if (param.type.has(type::q_noperspective))
							add_decoration(input_variable, spv::DecorationNoPerspective);
						if (param.type.has(type::q_centroid))
							add_decoration(input_variable, spv::DecorationCentroid);
						if (param.type.has(type::q_nointerpolation))
							add_decoration(input_variable, spv::DecorationFlat);

						inputs_and_outputs.push_back(input_variable);
						return input_variable;
					};
					const auto create_output_param = [this, &section, &call_params](const spirv_struct_member_info &param) {
						const spv::Id function_variable = make_id();
						define_variable(function_variable, nullptr, {}, param.type, spv::StorageClassFunction);
						call_params.push_back(function_variable);
						return function_variable;
					};
					const auto create_output_variable = [this, &inputs_and_outputs, &semantic_to_builtin](const spirv_struct_member_info &param) {
						type output_type = param.type;
						output_type.is_output = true;
						output_type.is_ptr = true;

						const spv::Id output_variable = make_id();
						define_variable(output_variable, nullptr, {}, output_type, spv::StorageClassOutput);

						if (spv::BuiltIn builtin; semantic_to_builtin(param.semantic, builtin))
							add_builtin(output_variable, builtin);
						else
						{
							uint32_t location = 0;
							if (param.semantic.size() >= 5 && param.semantic.compare(0, 5, "COLOR") == 0)
								location = std::strtol(param.semantic.substr(5).c_str(), nullptr, 10);
							else if (param.semantic.size() >= 9 && param.semantic.compare(0, 9, "SV_TARGET") == 0)
								location = std::strtol(param.semantic.substr(9).c_str(), nullptr, 10);
							else if (param.semantic.size() >= 8 && param.semantic.compare(0, 8, "TEXCOORD") == 0)
								location = std::strtol(param.semantic.substr(8).c_str(), nullptr, 10);
							else if (const auto it = _semantic_to_location.find(param.semantic); it != _semantic_to_location.end())
								location = it->second;
							else
								_semantic_to_location[param.semantic] = location = _current_semantic_location++;

							add_decoration(output_variable, spv::DecorationLocation, { location });
						}

						if (param.type.has(type::q_noperspective))
							add_decoration(output_variable, spv::DecorationNoPerspective);
						if (param.type.has(type::q_centroid))
							add_decoration(output_variable, spv::DecorationCentroid);
						if (param.type.has(type::q_nointerpolation))
							add_decoration(output_variable, spv::DecorationFlat);

						inputs_and_outputs.push_back(output_variable);
						return output_variable;
					};

					// Handle input parameters
					for (const spirv_struct_member_info &param : function_info->parameter_list)
					{
						if (param.type.has(type::q_out))
						{
							create_output_param(param);

							// Flatten structure parameters
							if (param.type.is_struct())
							{
								for (const auto &member : _structs[param.type.definition].member_list)
								{
									create_output_variable(member);
								}
							}
							else
							{
								create_output_variable(param);
							}
						}
						else
						{
							const spv::Id param_variable = create_input_param(param);

							// Flatten structure parameters
							if (param.type.is_struct())
							{
								std::vector<spv::Id> elements;

								for (const auto &member : _structs[param.type.definition].member_list)
								{
									const spv::Id input_variable = create_input_variable(member);

									type value_type = member.type;
									value_type.is_ptr = false;

									const spv::Id value = add_instruction(section, {}, spv::OpLoad, convert_type(value_type))
										.add(input_variable)
										.result;
									elements.push_back(value);
								}

								type composite_type = param.type;
								composite_type.is_ptr = false;
								spirv_instruction &construct = add_instruction(section, {}, spv::OpCompositeConstruct, convert_type(composite_type));
								for (auto elem : elements)
									construct.add(elem);
								const spv::Id composite_value = construct.result;

								add_instruction_without_result(section, {}, spv::OpStore)
									.add(param_variable)
									.add(composite_value);
							}
							else
							{
								const spv::Id input_variable = create_input_variable(param);

								type value_type = param.type;
								value_type.is_ptr = false;

								const spv::Id value = add_instruction(section, {}, spv::OpLoad, convert_type(value_type))
									.add(input_variable)
									.result;
								add_instruction_without_result(section, {}, spv::OpStore)
									.add(param_variable)
									.add(value);
							}
						}
					}

					spirv_instruction &call = add_instruction(section, location, spv::OpFunctionCall, convert_type(function_info->return_type))
						.add(function_info->definition);
					for (auto elem : call_params)
						call.add(elem);
					const spv::Id call_result = call.result;

					size_t param_index = 0;
					size_t inputs_and_outputs_index = 0;
					for (const spirv_struct_member_info &param : function_info->parameter_list)
					{
						if (param.type.has(type::q_out))
						{
							type value_type = param.type;
							value_type.is_ptr = false;

							const spv::Id value = add_instruction(section, {}, spv::OpLoad, convert_type(value_type))
								.add(call_params[param_index++])
								.result;

							if (param.type.is_struct())
							{
								size_t member_index = 0;
								for (const auto &member : _structs[param.type.definition].member_list)
								{
									const spv::Id member_value = add_instruction(section, {}, spv::OpCompositeExtract, convert_type(member.type))
										.add(value)
										.add(member_index++)
										.result;
									add_instruction_without_result(section, {}, spv::OpStore)
										.add(inputs_and_outputs[inputs_and_outputs_index++])
										.add(member_value);
								}
							}
							else
							{
								add_instruction_without_result(section, {}, spv::OpStore)
									.add(inputs_and_outputs[inputs_and_outputs_index++])
									.add(value);
							}
						}
						else
						{
							param_index++;
							inputs_and_outputs_index += param.type.is_struct() ? _structs[param.type.definition].member_list.size() : 1;
						}
					}

					if (function_info->return_type.is_struct())
					{
						size_t member_index = 0;
						for (const auto &member : _structs[function_info->return_type.definition].member_list)
						{
							spv::Id result = create_output_variable(member);

							spv::Id member_result = add_instruction(section, {}, spv::OpCompositeExtract, convert_type(member.type))
								.add(call_result)
								.add(member_index)
								.result;

							add_instruction_without_result(section, {}, spv::OpStore)
								.add(result)
								.add(member_result);

							member_index++;
						}
					}
					else if (!function_info->return_type.is_void())
					{
						type ptr_type = function_info->return_type;
						ptr_type.is_output = true;
						ptr_type.is_ptr = true;

						const spv::Id result = make_id();
						define_variable(result, nullptr, location, ptr_type, spv::StorageClassOutput);

						if (spv::BuiltIn builtin; semantic_to_builtin(function_info->return_semantic, builtin))
							add_builtin(result, builtin);
						else
						{
							uint32_t semantic_location = 0;
							if (function_info->return_semantic.size() >= 5 && function_info->return_semantic.compare(0, 5, "COLOR") == 0)
								semantic_location = std::strtol(function_info->return_semantic.substr(5).c_str(), nullptr, 10);
							else if (function_info->return_semantic.size() >= 9 && function_info->return_semantic.compare(0, 9, "SV_TARGET") == 0)
								semantic_location = std::strtol(function_info->return_semantic.substr(9).c_str(), nullptr, 10);
							else if (function_info->return_semantic.size() >= 8 && function_info->return_semantic.compare(0, 8, "TEXCOORD") == 0)
								semantic_location = std::strtol(function_info->return_semantic.substr(8).c_str(), nullptr, 10);
							else if (const auto it = _semantic_to_location.find(function_info->return_semantic); it != _semantic_to_location.end())
								semantic_location = it->second;
							else
								_semantic_to_location[function_info->return_semantic] = semantic_location = _current_semantic_location++;

							add_decoration(result, spv::DecorationLocation, { semantic_location });
						}

						inputs_and_outputs.push_back(result);

						add_instruction_without_result(section, {}, spv::OpStore)
							.add(result)
							.add(call_result);
					}

					leave_block_and_return(section, 0);
					leave_function();

					// Add entry point
					add_entry_point(function_info->name.c_str(), function_info->entry_point, is_vs ? spv::ExecutionModelVertex : spv::ExecutionModelFragment, inputs_and_outputs);
				}

				if (is_vs)
					info.vs_entry_point = function_info->name;
					//std::copy_n(function_info->name.c_str(), std::min(function_info->name.size() + 1, sizeof(info.vs_entry_point)), info.vs_entry_point);
				if (is_ps)
					info.ps_entry_point = function_info->name;
					//std::copy_n(function_info->name.c_str(), std::min(function_info->name.size() + 1, sizeof(info.ps_entry_point)), info.ps_entry_point);
			}
			else
			{
				assert(is_texture_state);

				if (!symbol.id)
					return error(location, 3004, "undeclared identifier '" + identifier + "', expected texture name"), consume_until('}'), false;
				else if (!symbol.type.is_texture())
					return error(location, 3020, "type mismatch, expected texture name"), consume_until('}'), false;

				const size_t target_index = state.size() > 12 ? (state[12] - '0') : 0;

				info.render_target_names[target_index] = std::find_if(_textures.begin(), _textures.end(),
					[&symbol](const auto &it) { return it.id == symbol.id; })->unique_name;
			}
		}
		else // Handle the rest of the pass states
		{
			backup();

			expression expression;

			if (accept(tokenid::identifier)) // Handle special enumeration names for pass states
			{
				// Transform identifier to uppercase to do case-insensitive comparison
				std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), [](char c) { return static_cast<char>(toupper(c)); });

				static const std::pair<const char *, uint32_t> s_enum_values[] = {
					{ "NONE", 0 }, { "ZERO", 0 }, { "ONE", 1 },
					{ "SRCCOLOR", 2 }, { "SRCALPHA", 3 }, { "INVSRCCOLOR", 4 }, { "INVSRCALPHA", 5 }, { "DESTCOLOR", 8 }, { "DESTALPHA", 6 }, { "INVDESTCOLOR", 9 }, { "INVDESTALPHA", 7 },
					{ "ADD", 1 }, { "SUBTRACT", 2 }, { "REVSUBTRACT", 3 }, { "MIN", 4 }, { "MAX", 5 },
					{ "KEEP", 1 }, { "REPLACE", 3 }, { "INVERT", 6 }, { "INCR", 7 }, { "INCRSAT", 4 }, { "DECR", 8 }, { "DECRSAT", 5 },
					{ "NEVER", 1 }, { "ALWAYS", 8 }, { "LESS", 2 }, { "GREATER", 5 }, { "LEQUAL", 4 }, { "LESSEQUAL", 4 }, { "GEQUAL", 7 }, { "GREATEREQUAL", 7 }, { "EQUAL", 3 }, { "NEQUAL", 6 }, { "NOTEQUAL", 6 },
				};

				// Look up identifier in list of possible enumeration names
				const auto it = std::find_if(std::begin(s_enum_values), std::end(s_enum_values),
					[this](const auto &it) { return it.first == _token.literal_as_string; });

				if (it != std::end(s_enum_values))
					expression.reset_to_rvalue_constant(_token.location, it->second);
				else // No match found, so rewind to parser state before the identifier was consumed and try parsing it as a normal expression
					restore();
			}

			// Parse right hand side as normal expression if no special enumeration name was matched already
			if (spirv_basic_block temp_block; !expression.is_constant && !parse_expression_multary(temp_block, expression))
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected expression"), consume_until('}'), false;
			else if (!expression.is_constant || !expression.type.is_scalar())
				return error(expression.location, 3011, "pass state value must be a literal scalar expression"), consume_until('}'), false;

			// All states below expect the value to be of an unsigned integer type
			add_cast_operation(expression, { type::t_uint, 1, 1 });
			const unsigned int value = expression.constant.as_uint[0];

			if (state == "SRGBWriteEnable")
				info.srgb_write_enable = value != 0;
			else if (state == "BlendEnable")
				info.blend_enable = value != 0;
			else if (state == "StencilEnable")
				info.stencil_enable = value != 0;
			else if (state == "ClearRenderTargets")
				info.clear_render_targets = value != 0;
			else if (state == "RenderTargetWriteMask" || state == "ColorWriteMask")
				info.color_write_mask = value & 0xFF;
			else if (state == "StencilReadMask" || state == "StencilMask")
				info.stencil_read_mask = value & 0xFF;
			else if (state == "StencilWriteMask")
				info.stencil_write_mask = value & 0xFF;
			else if (state == "BlendOp")
				info.blend_op = value;
			else if (state == "BlendOpAlpha")
				info.blend_op_alpha = value;
			else if (state == "SrcBlend")
				info.src_blend = value;
			else if (state == "SrcBlendAlpha")
				info.src_blend_alpha = value;
			else if (state == "DestBlend")
				info.dest_blend = value;
			else if (state == "DestBlendAlpha")
				info.dest_blend_alpha = value;
			else if (state == "StencilFunc")
				info.stencil_comparison_func = value;
			else if (state == "StencilRef")
				info.stencil_reference_value = value;
			else if (state == "StencilPass" || state == "StencilPassOp")
				info.stencil_op_pass = value;
			else if (state == "StencilFail" || state == "StencilFailOp")
				info.stencil_op_fail = value;
			else if (state == "StencilZFail" || state == "StencilDepthFail" || state == "StencilDepthFailOp")
				info.stencil_op_depth_fail = value;
			else
				return error(location, 3004, "unrecognized pass state '" + state + "'"), consume_until('}'), false;
		}

		if (!expect(';'))
			return consume_until('}'), false;
	}

	return expect('}');
}
