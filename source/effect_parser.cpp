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

spv::BuiltIn semantic_to_builtin(std::string &semantic, unsigned int &index)
{
	std::transform(semantic.begin(), semantic.end(), semantic.begin(), ::toupper);

	if (semantic == "SV_POSITION")
		return spv::BuiltInPosition;
	if (semantic == "SV_POINTSIZE")
		return spv::BuiltInPointSize;
	if (semantic == "SV_DEPTH")
		return spv::BuiltInFragDepth;
	if (semantic == "VERTEXID" || semantic == "SV_VERTEXID")
		return spv::BuiltInVertexId;
	if (semantic.size() > 9 && semantic.compare(0, 9, "SV_TARGET") == 0)
		index = std::stol(semantic.substr(9));
	if (semantic.size() > 8 && semantic.compare(0, 8, "TEXCOORD") == 0)
		index = std::stol(semantic.substr(8));
	return spv::BuiltInMax;
}

static inline uintptr_t align(uintptr_t address, size_t alignment)
{
	return (address % alignment != 0) ? address + alignment - address % alignment : address;
}

static reshadefx::spirv_constant evaluate_constant_expression(spv::Op op, const reshadefx::spirv_expression &exp)
{
	assert(exp.is_constant);

	reshadefx::spirv_constant constant_data = {};

	switch (op)
	{
	case spv::OpLogicalNot:
		for (unsigned int i = 0; i < exp.type.components(); ++i)
			constant_data.as_uint[i] = !exp.constant.as_uint[i];
		break;
	case spv::OpFNegate:
		for (unsigned int i = 0; i < exp.type.components(); ++i)
			constant_data.as_float[i] = -exp.constant.as_float[i];
		break;
	case spv::OpSNegate:
		for (unsigned int i = 0; i < exp.type.components(); ++i)
			constant_data.as_int[i] = -exp.constant.as_int[i];
		break;
	case spv::OpNot:
		for (unsigned int i = 0; i < exp.type.components(); ++i)
			constant_data.as_uint[i] = ~exp.constant.as_uint[i];
		break;
	}

	return constant_data;
}
static reshadefx::spirv_constant evaluate_constant_expression(spv::Op op, const reshadefx::spirv_expression &lhs, const reshadefx::spirv_expression &rhs)
{
	assert(lhs.is_constant && rhs.is_constant);

	reshadefx::spirv_constant constant_data = lhs.constant;

	switch (op)
	{
	case spv::OpFRem:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_float[i] = fmodf(lhs.constant.as_float[i], rhs.constant.as_float[i]);
		break;
	case spv::OpSRem:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_int[i] %= rhs.constant.as_int[i];
		break;
	case spv::OpUMod:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] %= rhs.constant.as_uint[i];
		break;
	case spv::OpFMul:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_float[i] *= rhs.constant.as_float[i];
		break;
	case spv::OpIMul:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] *= rhs.constant.as_uint[i];
		break;
	case spv::OpFAdd:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_float[i] += rhs.constant.as_float[i];
		break;
	case spv::OpIAdd:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] += rhs.constant.as_uint[i];
		break;
	case spv::OpFSub:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_float[i] -= rhs.constant.as_float[i];
		break;
	case spv::OpISub:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] -= rhs.constant.as_uint[i];
		break;
	case spv::OpFDiv:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_float[i] /= rhs.constant.as_float[i];
		break;
	case spv::OpSDiv:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_int[i] /= rhs.constant.as_int[i];
		break;
	case spv::OpUDiv:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] /= rhs.constant.as_uint[i];
		break;
	case spv::OpLogicalAnd:
	case spv::OpBitwiseAnd:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] &= rhs.constant.as_uint[i];
		break;
	case spv::OpLogicalOr:
	case spv::OpBitwiseOr:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] |= rhs.constant.as_uint[i];
		break;
	case spv::OpBitwiseXor:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] ^= rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdLessThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] < rhs.constant.as_float[i];
		break;
	case spv::OpSLessThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_int[i] < rhs.constant.as_int[i];
		break;
	case spv::OpULessThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] < rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdLessThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] <= rhs.constant.as_float[i];
		break;
	case spv::OpSLessThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_int[i] <= rhs.constant.as_int[i];
		break;
	case spv::OpULessThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] <= rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdGreaterThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] > rhs.constant.as_float[i];
		break;
	case spv::OpSGreaterThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_int[i] > rhs.constant.as_int[i];
		break;
	case spv::OpUGreaterThan:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] > rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdGreaterThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] >= rhs.constant.as_float[i];
		break;
	case spv::OpSGreaterThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_int[i] >= rhs.constant.as_int[i];
		break;
	case spv::OpUGreaterThanEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] >= rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] == rhs.constant.as_float[i];
	case spv::OpIEqual:
	case spv::OpLogicalEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] == rhs.constant.as_uint[i];
		break;
	case spv::OpFOrdNotEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_float[i] != rhs.constant.as_float[i];
	case spv::OpINotEqual:
	case spv::OpLogicalNotEqual:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] = lhs.constant.as_uint[i] != rhs.constant.as_uint[i];
		break;
	case spv::OpShiftLeftLogical:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] <<= rhs.constant.as_uint[i];
		break;
	case spv::OpShiftRightArithmetic:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_int[i] >>= rhs.constant.as_int[i];
		break;
	case spv::OpShiftRightLogical:
		for (unsigned int i = 0; i < lhs.type.components(); ++i)
			constant_data.as_uint[i] >>= rhs.constant.as_uint[i];
		break;
	}

	return constant_data;
}

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
		for (auto &member : _uniforms.member_list)
		{
			assert(member.type.has(spirv_type::q_uniform));
			member_types.push_back(convert_type(member.type));
		}

		define_struct(_global_ubo_type, "$Globals", {}, member_types);
		add_decoration(_global_ubo_type, spv::DecorationBlock);
		add_decoration(_global_ubo_type, spv::DecorationDescriptorSet, { 0 });

		define_variable(_global_ubo_variable, "$Globals", {}, { spirv_type::t_struct, 0, 0, spirv_type::q_uniform, true, false, false, 0, _global_ubo_type }, spv::StorageClassUniform);
	}

	//std::ofstream s("test.spv", std::ios::binary | std::ios::out);
	//write_module(s);

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

bool reshadefx::parser::accept_type_class(spirv_type &type)
{
	type.rows = type.cols = 0;

	if (peek(tokenid::identifier))
	{
		type.base = spirv_type::t_struct;

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
		type.base = spirv_type::t_float;
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
		type.base = spirv_type::t_float;
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
		type.base = spirv_type::t_void;
		break;
	case tokenid::bool_:
	case tokenid::bool2:
	case tokenid::bool3:
	case tokenid::bool4:
		type.base = spirv_type::t_bool;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::bool_);
		type.cols = 1;
		break;
	case tokenid::bool2x2:
	case tokenid::bool3x3:
	case tokenid::bool4x4:
		type.base = spirv_type::t_bool;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::bool2x2);
		type.cols = type.rows;
		break;
	case tokenid::int_:
	case tokenid::int2:
	case tokenid::int3:
	case tokenid::int4:
		type.base = spirv_type::t_int;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::int_);
		type.cols = 1;
		break;
	case tokenid::int2x2:
	case tokenid::int3x3:
	case tokenid::int4x4:
		type.base = spirv_type::t_int;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::int2x2);
		type.cols = type.rows;
		break;
	case tokenid::uint_:
	case tokenid::uint2:
	case tokenid::uint3:
	case tokenid::uint4:
		type.base = spirv_type::t_uint;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::uint_);
		type.cols = 1;
		break;
	case tokenid::uint2x2:
	case tokenid::uint3x3:
	case tokenid::uint4x4:
		type.base = spirv_type::t_uint;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::uint2x2);
		type.cols = type.rows;
		break;
	case tokenid::float_:
	case tokenid::float2:
	case tokenid::float3:
	case tokenid::float4:
		type.base = spirv_type::t_float;
		type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::float_);
		type.cols = 1;
		break;
	case tokenid::float2x2:
	case tokenid::float3x3:
	case tokenid::float4x4:
		type.base = spirv_type::t_float;
		type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::float2x2);
		type.cols = type.rows;
		break;
	case tokenid::string_:
		type.base = spirv_type::t_string;
		break;
	case tokenid::texture:
		type.base = spirv_type::t_texture;
		break;
	case tokenid::sampler:
		type.base = spirv_type::t_sampler;
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_type_qualifiers(spirv_type &type)
{
	unsigned int qualifiers = 0;

	// Storage
	if (accept(tokenid::extern_))
		qualifiers |= spirv_type::q_extern;
	if (accept(tokenid::static_))
		qualifiers |= spirv_type::q_static;
	if (accept(tokenid::uniform_))
		qualifiers |= spirv_type::q_uniform;
	if (accept(tokenid::volatile_))
		qualifiers |= spirv_type::q_volatile;

	if (accept(tokenid::in))
		qualifiers |= spirv_type::q_in;
	if (accept(tokenid::out))
		qualifiers |= spirv_type::q_out;
	if (accept(tokenid::inout))
		qualifiers |= spirv_type::q_inout;

	// Modifiers
	if (accept(tokenid::const_))
		qualifiers |= spirv_type::q_const;

	// Interpolation
	if (accept(tokenid::linear))
		qualifiers |= spirv_type::q_linear;
	if (accept(tokenid::noperspective))
		qualifiers |= spirv_type::q_noperspective;
	if (accept(tokenid::centroid))
		qualifiers |= spirv_type::q_centroid;
	if (accept(tokenid::nointerpolation))
		qualifiers |= spirv_type::q_nointerpolation;

	if (qualifiers == 0)
		return false;
	if ((type.qualifiers & qualifiers) == qualifiers)
		warning(_token.location, 3048, "duplicate usages specified");

	type.qualifiers |= qualifiers;

	// Continue parsing potential additional qualifiers until no more are found
	accept_type_qualifiers(type);

	return true;
}

bool reshadefx::parser::parse_type(spirv_type &type)
{
	type.qualifiers = 0;

	accept_type_qualifiers(type);

	if (!accept_type_class(type))
		return false;

	if (type.is_integral() && (type.has(spirv_type::q_centroid) || type.has(spirv_type::q_noperspective)))
		return error(_token.location, 4576, "signature specifies invalid interpolation mode for integer component type"), false;
	else if (type.has(spirv_type::q_centroid) && !type.has(spirv_type::q_noperspective))
		type.qualifiers |= spirv_type::q_linear;

	return true;
}
bool reshadefx::parser::parse_array_size(spirv_type &type)
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
		else if (spirv_expression expression; parse_expression(block, expression) && expect(']'))
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

bool reshadefx::parser::accept_unary_op(spv::Op &op)
{
	switch (_token_next.id)
	{
	case tokenid::exclaim: op = spv::OpLogicalNot; break;
	case tokenid::plus: op = spv::OpNop; break;
	case tokenid::minus: op = spv::OpFNegate; break;
	case tokenid::tilde: op = spv::OpNot; break;
	case tokenid::plus_plus: op = spv::OpFAdd; break;
	case tokenid::minus_minus: op = spv::OpFSub; break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_postfix_op(const spirv_type &type, spv::Op &op)
{
	switch (_token_next.id)
	{
	case tokenid::plus_plus:
		op = type.is_integral() ? spv::OpIAdd : spv::OpFAdd; break;
	case tokenid::minus_minus:
		op = type.is_integral() ? spv::OpISub : spv::OpFSub; break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::peek_multary_op(spv::Op &op, unsigned int &precedence) const
{
	// Precedence values taken from https://cppreference.com/w/cpp/language/operator_precedence
	switch (_token_next.id)
	{
	case tokenid::percent: op = spv::OpFRem; precedence = 11; break;
	case tokenid::ampersand: op = spv::OpBitwiseAnd; precedence = 6; break;
	case tokenid::star: op = spv::OpFMul; precedence = 11; break;
	case tokenid::plus: op = spv::OpFAdd; precedence = 10; break;
	case tokenid::minus: op = spv::OpFSub; precedence = 10; break;
	case tokenid::slash: op = spv::OpFDiv; precedence = 11; break;
	case tokenid::less: op = spv::OpFOrdLessThan; precedence = 8; break;
	case tokenid::greater: op = spv::OpFOrdGreaterThan; precedence = 8; break;
	case tokenid::question: op = spv::OpSelect; precedence = 1; break;
	case tokenid::caret: op = spv::OpBitwiseXor; precedence = 5; break;
	case tokenid::pipe: op = spv::OpBitwiseOr; precedence = 4; break;
	case tokenid::exclaim_equal: op = spv::OpLogicalNotEqual; precedence = 7; break;
	case tokenid::ampersand_ampersand: op = spv::OpLogicalAnd; precedence = 3; break;
	case tokenid::less_less: op = spv::OpShiftLeftLogical; precedence = 9; break;
	case tokenid::less_equal: op = spv::OpFOrdLessThanEqual; precedence = 8; break;
	case tokenid::equal_equal: op = spv::OpLogicalEqual; precedence = 7; break;
	case tokenid::greater_greater: op = spv::OpShiftRightLogical; precedence = 9; break;
	case tokenid::greater_equal: op = spv::OpFOrdGreaterThanEqual; precedence = 8; break;
	case tokenid::pipe_pipe: op = spv::OpLogicalOr; precedence = 2; break;
	default:
		return false;
	}

	// Do not consume token yet since the expression may be skipped due to precedence
	return true;
}
bool reshadefx::parser::accept_assignment_op(const spirv_type &type, spv::Op &op)
{
	switch (_token_next.id)
	{
	case tokenid::equal:
		op = spv::OpNop; break; // Assignment without an additional operation
	case tokenid::percent_equal:
		op = type.is_integral() ? type.is_signed() ? spv::OpSRem : spv::OpUMod : spv::OpFRem; break;
	case tokenid::ampersand_equal:
		op = spv::OpBitwiseAnd; break;
	case tokenid::star_equal:
		op = type.is_integral() ? spv::OpIMul : spv::OpFMul; break;
	case tokenid::plus_equal:
		op = type.is_integral() ? spv::OpIAdd : spv::OpFAdd; break;
	case tokenid::minus_equal:
		op = type.is_integral() ? spv::OpISub : spv::OpFSub; break;
	case tokenid::slash_equal:
		op = type.is_integral() ? type.is_signed() ? spv::OpSDiv : spv::OpUDiv : spv::OpFDiv; break;
	case tokenid::less_less_equal:
		op = spv::OpShiftLeftLogical; break;
	case tokenid::greater_greater_equal:
		op = type.is_signed() ? spv::OpShiftRightArithmetic : spv::OpShiftRightLogical; break;
	case tokenid::caret_equal:
		op = spv::OpBitwiseXor; break;
	case tokenid::pipe_equal:
		op = spv::OpBitwiseOr; break;
	default:
		return false;
	}

	consume();

	return true;
}

bool reshadefx::parser::parse_expression(spirv_basic_block &block, spirv_expression &exp)
{
	// Parse first expression
	if (!parse_expression_assignment(block, exp))
		return false;

	// Continue parsing if an expression sequence is next
	while (accept(','))
		// Overwrite since the last expression in the sequence is the result
		if (!parse_expression_assignment(block, exp))
			return false;

	return true;
}
bool reshadefx::parser::parse_expression_unary(spirv_basic_block &block, spirv_expression &exp)
{
	auto location = _token_next.location;

	#pragma region Prefix
	// Check if a prefix operator exists
	spv::Op op;
	if (accept_unary_op(op))
	{
		// Parse the actual expression
		if (!parse_expression_unary(block, exp))
			return false;

		// Unary operators are only valid on basic types
		if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
			return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;

		// Ignore "+" operator since it does not actually do anything
		if (op != spv::OpNop)
		{
			// The "~" bitwise operator is only valid on integral types
			if (op == spv::OpNot && !exp.type.is_integral())
				return error(exp.location, 3082, "int or unsigned int type required"), false;

			// Choose correct operator depending on the input expression type
			if (exp.type.is_integral())
			{
				switch (op)
				{
				case spv::OpFNegate:
					op = spv::OpSNegate;
					break;
				case spv::OpFAdd:
					op = spv::OpIAdd;
					break;
				case spv::OpFSub:
					op = spv::OpISub;
					break;
				}
			}

			// Load the right hand side value if it was not yet resolved at this point
			const spv::Id value = access_chain_load(block, exp);
			assert(value != 0);

			// Special handling for the "++" and "--" operators
			if (op == spv::OpFAdd || op == spv::OpFSub || op == spv::OpIAdd || op == spv::OpISub)
			{
				if (exp.type.has(spirv_type::q_const) || exp.type.has(spirv_type::q_uniform) || !exp.is_lvalue)
					return error(location, 3025, "l-value specifies const object"), false;

				// Create a constant one in the type of the expression
				spirv_constant one = {};
				for (unsigned int i = 0; i < exp.type.components(); ++i)
					if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] =  1u;
				const spv::Id constant = convert_constant(exp.type, one);

				spv::Id result = add_intruction(block, location, op, convert_type(exp.type))
					.add(value) // Operand 1
					.add(constant) // Operand 2
					.result; // Result ID

				// The "++" and "--" operands modify the source variable, so store result back into it
				access_chain_store(block, exp, result, exp.type);
			}
			else // All other prefix operators return a new r-value
			{
				// The 'OpLogicalNot' operator expects a boolean type as input, so perform cast if necessary
				if (op == spv::OpLogicalNot && !exp.type.is_boolean())
					add_cast_operation(exp, { spirv_type::t_bool, exp.type.rows, exp.type.cols }); // The result type will be boolean as well

				if (exp.is_constant)
				{
					exp.reset_to_rvalue_constant(location, evaluate_constant_expression(op, exp), exp.type);
				}
				else
				{
					spv::Id result = add_intruction(block, location, op, convert_type(exp.type))
						.add(value) // Operand
						.result; // Result ID

					exp.reset_to_rvalue(location, result, exp.type);
				}
			}
		}
	}
	else if (accept('('))
	{
		backup();

		// Check if this is a C-style cast expression
		if (spirv_type cast_type; accept_type_class(cast_type))
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

				// Can only cast between numeric types
				if (exp.type.is_numeric() && cast_type.is_numeric())
				{
					if (exp.type.components() < cast_type.components() && !exp.type.is_scalar())
						return error(location, 3017, "cannot convert these vector types"), false;

					add_cast_operation(exp, cast_type);
					return true;
				}
				else
				{
					error(location, 3017, "cannot convert non-numeric types");
					return false;
				}
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
		std::vector<spirv_expression> elements;

		bool constant = true;
		spirv_type composite_type = { spirv_type::t_bool, 1, 1 };

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

			spirv_expression &element = elements.back();

			constant &= element.is_constant; // Result is only constant if all arguments are constant
			composite_type = spirv_type::merge(composite_type, element.type);
		}

		if (constant)
		{
			spirv_constant constant_data = {};
			for (spirv_expression &element : elements)
			{
				add_cast_operation(element, composite_type);
				constant_data.array_data.push_back(element.constant);
			}

			composite_type.array_length = elements.size();

			exp.reset_to_rvalue_constant(location, std::move(constant_data), composite_type);
		}
		else
		{
			std::vector<spv::Id> ids;
			for (spirv_expression &element : elements)
			{
				add_cast_operation(element, composite_type);
				ids.push_back(access_chain_load(block, element));
				assert(ids.back() != 0);
			}

			composite_type.array_length = elements.size();

			const spv::Id result = add_intruction(block, location, spv::OpCompositeConstruct, convert_type(composite_type))
				.add(ids.begin(), ids.end())
				.result;

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
	else if (spirv_type type; accept_type_class(type)) // Check if this is a constructor call expression
	{
		if (!expect('('))
			return false;
		if (!type.is_numeric())
			return error(location, 3037, "constructors only defined for numeric base types"), false;

		// Empty constructors do not exist
		if (accept(')'))
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		// Parse entire argument expression list
		bool constant = true;
		unsigned int num_components = 0;
		std::vector<spirv_expression> arguments;

		while (!peek(')'))
		{
			// There should be a comma between arguments
			if (!arguments.empty() && !expect(','))
				return false;

			// Parse the argument expression
			if (!parse_expression_assignment(block, arguments.emplace_back()))
				return false;

			spirv_expression &argument = arguments.back();

			// Constructors are only defined for numeric base types
			if (!argument.type.is_numeric())
				return error(argument.location, 3017, "cannot convert non-numeric types"), false;

			constant &= argument.is_constant; // Result is only constant if all arguments are constant
			num_components += argument.type.components();
		}

		// The list should be terminated with a parenthesis
		if (!expect(')'))
			return false;

		// The total number of argument elements needs to match the number of elements in the result type
		if (num_components != type.components())
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		assert(num_components > 0 && num_components <= 16 && !type.is_array());

		if (constant) // Constants can be converted at compile time
		{
			spirv_constant constant_data = {};

			unsigned int i = 0;
			for (spirv_expression &argument : arguments)
			{
				add_cast_operation(argument, { type.base, argument.type.rows, argument.type.cols });
				for (unsigned int k = 0; k < argument.type.components(); ++k)
					constant_data.as_uint[i++] = argument.constant.as_uint[k];
			}

			exp.reset_to_rvalue_constant(location, std::move(constant_data), type);
		}
		else if (arguments.size() > 1)
		{
			const spv::Id result = construct_type(block, type, arguments);

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
			std::vector<spirv_expression> arguments;

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

			std::vector<spirv_expression> parameters(arguments.size());

			// We need to allocate some temporary variables to pass in and load results from pointer parameters
			for (size_t i = 0; i < arguments.size(); ++i)
			{
				const spirv_type &param_type = symbol.function->parameter_list[i].type;

				if (arguments[i].type.components() > param_type.components())
					warning(arguments[i].location, 3206, "implicit truncation of vector type");

				spirv_type target_type = param_type;
				target_type.is_ptr = false;
				add_cast_operation(arguments[i], target_type);

				if (param_type.is_ptr)
				{
					const spv::Id variable = make_id();
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
				if (parameters[i].is_lvalue && parameters[i].type.has(spirv_type::q_in)) // Only do this for pointer parameters as discovered above
					access_chain_store(block, parameters[i], access_chain_load(block, arguments[i]), arguments[i].type);

			// Check if the call resolving found an intrinsic or function
			if (symbol.intrinsic)
			{
				// This is an intrinsic, so invoke it
				const spv::Id result = symbol.intrinsic(*this, block, parameters);

				exp.reset_to_rvalue(location, result, symbol.type);
			}
			else
			{
				// This is a function symbol, so add a call to it
				spirv_instruction &call = add_intruction(block, location, spv::OpFunctionCall, convert_type(symbol.type))
					.add(symbol.id); // Function
				for (size_t i = 0; i < parameters.size(); ++i)
					call.add(parameters[i].base); // Arguments

				exp.reset_to_rvalue(location, call.result, symbol.type);
			}

			// Copy out parameters from parameter variables back to the argument access chains
			for (size_t i = 0; i < arguments.size(); ++i)
				if (parameters[i].is_lvalue && parameters[i].type.has(spirv_type::q_out)) // Only do this for pointer parameters as discovered above
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
				exp.reset_to_lvalue(location, symbol.id, { spirv_type::t_struct, 0, 0, 0, false, false, false, 0, symbol.id });
				add_member_access(exp, reinterpret_cast<uintptr_t>(symbol.function), symbol.type); // The member index was stored in the 'function' field
			}
			else // Can only reference variables and constants by name, functions need to be called
				return error(location, 3005, "identifier '" + identifier + "' represents a function, not a variable"), false;
		}
	}
	#pragma endregion

	#pragma region Postfix
	while (!peek(tokenid::end_of_file))
	{
		location = _token_next.location;

		// Check if a postfix operator exists
		if (accept_postfix_op(exp.type, op))
		{
			// Unary operators are only valid on basic types
			if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;
			else if (exp.type.has(spirv_type::q_const) || exp.type.has(spirv_type::q_uniform) || !exp.is_lvalue)
				return error(exp.location, 3025, "l-value specifies const object"), false;

			// Load current value from expression
			const spv::Id value = access_chain_load(block, exp);
			assert(value != 0);

			// Create a constant one in the type of the expression
			spirv_constant one = {};
			for (unsigned int i = 0; i < exp.type.components(); ++i)
				if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] = 1u;
			const spv::Id constant = convert_constant(exp.type, one);

			spv::Id result = add_intruction(block, location, op, convert_type(exp.type))
				.add(value) // Operand 1
				.add(constant) // Operand 2
				.result; // Result ID

			// The "++" and "--" operands modify the source variable, so store result back into it
			access_chain_store(block, exp, result, exp.type);

			// All postfix operators return a r-value
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

				bool constant = false;
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
							constant = true;
							break;
						}
				}

				// Add swizzle to current access chain
				add_swizzle_access(exp, offsets, length);

				if (constant || exp.type.has(spirv_type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | spirv_type::q_const) & ~spirv_type::q_uniform;
			}
			else if (exp.type.is_matrix())
			{
				const size_t length = subscript.size();
				if (length < 3)
					return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

				bool constant = false;
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
							constant = true;
							break;
						}
				}

				// Add swizzle to current access chain
				add_swizzle_access(exp, offsets, length / (3 + set));

				if (constant || exp.type.has(spirv_type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | spirv_type::q_const) & ~spirv_type::q_uniform;
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

				if (exp.type.has(spirv_type::q_uniform)) // Member access to uniform structure is not modifiable
					exp.type.qualifiers = (exp.type.qualifiers | spirv_type::q_const) & ~spirv_type::q_uniform;
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
				spirv_type target_type = exp.type;
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
			spirv_expression index;
			if (!parse_expression(block, index) || !expect(']'))
				return false;
			else if (!index.type.is_scalar() || !index.type.is_integral())
				return error(index.location, 3120, "invalid type for index - index must be a scalar"), false;

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
bool reshadefx::parser::parse_expression_multary(spirv_basic_block &block, spirv_expression &lhs, unsigned int left_precedence)
{
	// Parse left hand side of the expression
	if (!parse_expression_unary(block, lhs))
		return false;

	// Check if an operator exists so that this is a binary or ternary expression
	spv::Op op;
	unsigned int right_precedence;

	while (peek_multary_op(op, right_precedence))
	{
		// Only process this operator if it has a lower precedence than the current operation, otherwise leave it for later and abort
		if (right_precedence <= left_precedence)
			break;

		// Finally consume the operator token
		consume();

		// Check if this is a binary or ternary operation
		if (op != spv::OpSelect)
		{
			// Parse the right hand side of the binary operation
			spirv_expression rhs;
			spirv_basic_block rhs_block;
			if (!parse_expression_multary(rhs_block, rhs, right_precedence))
				return false;

			// Deduce the result base type based on implicit conversion rules
			spirv_type type = spirv_type::merge(lhs.type, rhs.type);
			bool boolean_result = false;

			// Do some error checking depending on the operator
			if (op == spv::OpLogicalEqual || op == spv::OpLogicalNotEqual)
			{
				// Select operator matching the argument types
				if (type.is_integral() || type.is_floating_point())
				{
					switch (op)
					{
					case spv::OpLogicalEqual:
						op = type.is_integral() ? spv::OpIEqual : spv::OpFOrdEqual;
						break;
					case spv::OpLogicalNotEqual:
						op = type.is_integral() ? spv::OpINotEqual : spv::OpFOrdNotEqual;
						break;
					}
				}

				// Equality checks return a boolean value
				boolean_result = true;

				// Cannot check equality between incompatible types
				if (lhs.type.is_array() || rhs.type.is_array() || lhs.type.definition != rhs.type.definition)
					return error(rhs.location, 3020, "type mismatch"), false;
			}
			else if (op == spv::OpBitwiseAnd || op == spv::OpBitwiseOr || op == spv::OpBitwiseXor)
			{
				// Cannot perform bitwise operations on non-integral types
				if (!lhs.type.is_integral())
					return error(lhs.location, 3082, "int or unsigned int type required"), false;
				if (!rhs.type.is_integral())
					return error(rhs.location, 3082, "int or unsigned int type required"), false;
			}
			else
			{
				if (op == spv::OpLogicalAnd || op == spv::OpLogicalOr)
					type.base = spirv_type::t_bool;

				// Logical operations return a boolean value
				if (op == spv::OpFOrdLessThan || op == spv::OpFOrdGreaterThan ||
					op == spv::OpFOrdLessThanEqual || op == spv::OpFOrdGreaterThanEqual)
					boolean_result = true;

				// Select operator matching the argument types
				if (type.is_integral())
				{
					switch (op)
					{
					case spv::OpFRem:
						op = type.is_signed() ? spv::OpSRem : spv::OpUMod;
						break;
					case spv::OpFMul:
						op = spv::OpIMul;
						break;
					case spv::OpFAdd:
						op = spv::OpIAdd;
						break;
					case spv::OpFSub:
						op = spv::OpISub;
						break;
					case spv::OpFDiv:
						op = type.is_signed() ? spv::OpSDiv : spv::OpUDiv;
						break;
					case spv::OpFOrdLessThan:
						op = type.is_signed() ? spv::OpSLessThan : spv::OpULessThan;
						break;
					case spv::OpFOrdGreaterThan:
						op = type.is_signed() ? spv::OpSGreaterThan : spv::OpUGreaterThan;
						break;
					case spv::OpFOrdLessThanEqual:
						op = type.is_signed() ? spv::OpSLessThanEqual : spv::OpULessThanEqual;
						break;
					case spv::OpFOrdGreaterThanEqual:
						op = type.is_signed() ? spv::OpSGreaterThanEqual : spv::OpUGreaterThanEqual;
						break;
					case spv::OpShiftRightLogical:
						op = type.is_signed() ? spv::OpShiftRightArithmetic : spv::OpShiftRightLogical;
						break;
					}
				}

				// Cannot perform arithmetic operations on non-basic types
				if (!lhs.type.is_scalar() && !lhs.type.is_vector() && !lhs.type.is_matrix())
					return error(lhs.location, 3022, "scalar, vector, or matrix expected"), false;
				if (!rhs.type.is_scalar() && !rhs.type.is_vector() && !rhs.type.is_matrix())
					return error(rhs.location, 3022, "scalar, vector, or matrix expected"), false;
			}

			if (lhs.type.components() > type.components())
				warning(lhs.location, 3206, "implicit truncation of vector type");
			if (rhs.type.components() > type.components())
				warning(rhs.location, 3206, "implicit truncation of vector type");

			if (lhs.is_constant && rhs.is_constant)
			{
				add_cast_operation(lhs, type);
				add_cast_operation(rhs, type);

				lhs.reset_to_rvalue_constant(lhs.location, evaluate_constant_expression(op, lhs, rhs), type);
			}
			else
			{
				// Load values and perform implicit type conversions
				add_cast_operation(lhs, type);
				const spv::Id lhs_value = access_chain_load(block, lhs);
				assert(lhs_value != 0);

#if RESHADEFX_SHORT_CIRCUIT
				// Short circuit for logical && and || operators
				if (op == spv::OpLogicalAnd || op == spv::OpLogicalOr)
				{
					const spv::Id merge_label = make_id();
					const spv::Id parent0_label = _current_block;
					const spv::Id parent1_label = make_id();

					if (op == spv::OpLogicalAnd)
					{
						// Emit "if ( lhs) result = rhs"
						leave_block_and_branch_conditional(block, lhs_value, parent1_label, merge_label);
					}
					else
					{
						// Emit "if (!lhs) result = rhs"
						const spv::Id cond = add_intruction(block, lhs.location, spv::OpLogicalNot, convert_type(type))
							.add(lhs_value)
							.result;
						leave_block_and_branch_conditional(block, cond, parent1_label, merge_label);
					}

					enter_block(block, parent1_label);

					block.append(std::move(rhs_block));

					add_cast_operation(rhs, type);
					const spv::Id rhs_value = access_chain_load(block, rhs);
					assert(rhs_value != 0);

					leave_block_and_branch(block, merge_label);

					enter_block(block, merge_label);

					spv::Id result = add_intruction(block, lhs.location, spv::OpPhi, convert_type(type))
						.add(lhs_value) // Variable 0
						.add(parent0_label) // Parent 0
						.add(rhs_value) // Variable 1
						.add(parent1_label) // Parent 1
						.result;

					lhs.reset_to_rvalue(result, type, lhs.location);
				}
				else
#endif
				{
					block.append(std::move(rhs_block));

					add_cast_operation(rhs, type);
					const spv::Id rhs_value = access_chain_load(block, rhs);
					assert(rhs_value != 0);

					// Certain operations return a boolean type instead of the type of the input expressions
					if (boolean_result)
						type = { spirv_type::t_bool, type.rows, type.cols };

					spv::Id result = add_intruction(block, lhs.location, op, convert_type(type))
						.add(lhs_value) // Operand 1
						.add(rhs_value) // Operand 2
						.result; // Result ID

					lhs.reset_to_rvalue(lhs.location, result, type);
				}
			}
		}
		else
		{
			// A conditional expression needs a scalar or vector type condition
			if (!lhs.type.is_scalar() && !lhs.type.is_vector())
				return error(lhs.location, 3022, "boolean or vector expression expected"), false;

			// Parse the first part of the right hand side of the ternary operation
			spirv_expression true_exp; spirv_basic_block true_block;
			if (!parse_expression(true_block, true_exp))
				return false;

			if (!expect(':'))
				return false;

			// Parse the second part of the right hand side of the ternary operation
			spirv_expression false_exp; spirv_basic_block false_block;
			if (!parse_expression_assignment(false_block, false_exp))
				return false;

			// Check that the condition dimension matches that of at least one side
			if (lhs.type.is_vector() && lhs.type.rows != true_exp.type.rows && lhs.type.cols != true_exp.type.cols)
				return error(lhs.location, 3020, "dimension of conditional does not match value"), false;

			// Check that the two value expressions can be converted between each other
			if (true_exp.type.array_length != false_exp.type.array_length || true_exp.type.definition != false_exp.type.definition)
				return error(false_exp.location, 3020, "type mismatch between conditional values"), false;

			// Deduce the result base type based on implicit conversion rules
			const spirv_type type = spirv_type::merge(true_exp.type, false_exp.type);

			if (true_exp.type.components() > type.components())
				warning(true_exp.location, 3206, "implicit truncation of vector type");
			if (false_exp.type.components() > type.components())
				warning(false_exp.location, 3206, "implicit truncation of vector type");

#if RESHADEFX_SHORT_CIRCUIT
			const spv::Id true_label = make_id();
			const spv::Id false_label = make_id();
			const spv::Id merge_label = make_id();

			add_cast_operation(lhs, { spirv_type::t_bool, lhs.type.rows, 1 });
			const spv::Id condition_value = access_chain_load(block, lhs);
			assert(condition_value != 0);

			add_instruction_without_result(block, lhs.location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(spv::SelectionControlMaskNone); // Selection Control

			leave_block_and_branch_conditional(block, condition_value, true_label, false_label);

			enter_block(block, true_label);
			block.append(std::move(true_block));
			add_cast_operation(true_exp, type);
			const spv::Id true_value = access_chain_load(block, true_exp);
			assert(true_value != 0);
			leave_block_and_branch(block, merge_label);

			enter_block(block, false_label);
			block.append(std::move(false_block));
			add_cast_operation(false_exp, type);
			const spv::Id false_value = access_chain_load(block, false_exp);
			assert(false_value != 0);
			leave_block_and_branch(block, merge_label);

			enter_block(block, merge_label);

			spv::Id result = add_intruction(block, lhs.location, spv::OpPhi, convert_type(type))
				.add(true_value) // Variable 0
				.add(true_label) // Parent 0
				.add(false_value) // Variable 1
				.add(false_label) // Parent 1
				.result;
#else
			block.append(std::move(true_block));
			block.append(std::move(false_block));

			// Load values and perform implicit type conversions
			add_cast_operation(lhs, { spirv_type::t_bool, type.rows, 1 });
			const spv::Id condition_value = access_chain_load(block, lhs);
			assert(condition_value != 0);
			add_cast_operation(true_exp, type);
			const spv::Id true_value = access_chain_load(block, true_exp);
			assert(true_value != 0);
			add_cast_operation(false_exp, type);
			const spv::Id false_value = access_chain_load(block, false_exp);
			assert(false_value != 0);

			spv::Id result = add_intruction(block, lhs.location, spv::OpSelect, convert_type(type))
				.add(condition_value) // Condition
				.add(true_value) // Object 1
				.add(false_value) // Object 2
				.result; // Result ID
#endif
			lhs.reset_to_rvalue(lhs.location, result, type);
		}
	}

	return true;
}
bool reshadefx::parser::parse_expression_assignment(spirv_basic_block &block, spirv_expression &lhs)
{
	// Parse left hand side of the expression
	if (!parse_expression_multary(block, lhs))
		return false;

	// Check if an operator exists so that this is an assignment
	spv::Op op;
	if (accept_assignment_op(lhs.type, op))
	{
		// Parse right hand side of the assignment expression
		spirv_expression rhs;
		if (!parse_expression_multary(block, rhs))
			return false;

		// Cannot assign to constants and uniform variables
		if (lhs.type.has(spirv_type::q_const) || lhs.type.has(spirv_type::q_uniform) || !lhs.is_lvalue)
			return error(lhs.location, 3025, "l-value specifies const object"), false;

		// Cannot assign between incompatible types
		if (lhs.type.array_length != rhs.type.array_length || !spirv_type::rank(lhs.type, rhs.type))
			return error(rhs.location, 3020, "cannot convert these types"), false;
		else if (rhs.type.components() > lhs.type.components())
			warning(rhs.location, 3206, "implicit truncation of vector type");

		// Load value of right hand side and perform implicit type conversion
		add_cast_operation(rhs, lhs.type);
		spv::Id rhs_value = access_chain_load(block, rhs);
		assert(rhs_value != 0);

		// Check if this is an assignment with an additional arithmetic instruction
		if (op != spv::OpNop)
		{
			// Load value from left hand side as well to use in the operation
			const spv::Id lhs_value = access_chain_load(block, lhs);
			assert(lhs_value != 0);

			// Handle arithmetic assignment operation
			spv::Id result = add_intruction(block, lhs.location, op, convert_type(lhs.type))
				.add(lhs_value) // Operand 1
				.add(rhs_value) // Operand 2
				.result; // Result ID

			// The result of the operation should now be stored in the variable
			rhs_value = result;
		}

		// Write result back to variable
		access_chain_store(block, lhs, rhs_value, lhs.type);

		// Return the result value since you can write assignments within expressions
		lhs.reset_to_rvalue(lhs.location, rhs_value, lhs.type);
	}

	return true;
}

bool reshadefx::parser::parse_annotations(std::unordered_map<std::string, spirv_constant> &annotations)
{
	// Check if annotations exist and return early if none do
	if (!accept('<'))
		return true;

	bool success = true;

	while (!peek('>'))
	{
		if (spirv_type type; accept_type_class(type))
			warning(_token.location, 4717, "type prefixes for annotations are deprecated and ignored");

		if (!expect(tokenid::identifier))
			return false;

		const auto name = std::move(_token.literal_as_string);

		spirv_expression expression;

		// Pass in temporary basic block, the expression should be constant
		if (spirv_basic_block block; !expect('=') || !parse_expression_unary(block, expression) || !expect(';'))
			return false;

		if (!expression.is_constant) {
			error(expression.location, 3011, "value must be a literal expression");
			success = false; // Continue parsing annotations despite the error, since the syntax is still correct
			continue;
		}

		annotations[name] = expression.constant;
	}

	return expect('>') && success;
}

// -- Statement & Declaration Parsing -- //

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
	else if (spirv_type type; parse_type(type)) // Type found, this can be either a variable or a function declaration
	{
		if (!expect(tokenid::identifier))
			return false;

		if (peek('('))
		{
			const auto name = std::move(_token.literal_as_string);
			// This is definitely a function declaration, so parse it
			if (!parse_function(type, name)) {
				// Insert dummy function into symbol table, so later references can be resolved despite the error
				insert_symbol(name, { spv::OpFunction, std::numeric_limits<spv::Id>::max(), { spirv_type::t_function } }, true);
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

bool reshadefx::parser::parse_statement(spirv_basic_block &block, bool scoped)
{
	if (_current_block == 0)
		return error(_token_next.location, 3000, "statements are valid only inside a code block"), false;

	unsigned int loop_control = spv::LoopControlMaskNone;
	unsigned int selection_control = spv::SelectionControlMaskNone;

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
		else if (attribute == "branch")
			selection_control |= spv::SelectionControlDontFlattenMask;
		else if (attribute == "flatten")
			selection_control |= spv::SelectionControlFlattenMask;
		else
			warning(_token.location, 0, "unknown attribute");
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
			const spv::Id true_label = make_id();
			const spv::Id false_label = make_id();
			const spv::Id merge_label = make_id();

			spirv_expression condition;
			if (!expect('(') || !parse_expression(block, condition) || !expect(')'))
				return false;
			else if (!condition.type.is_scalar())
				return error(condition.location, 3019, "if statement conditional expressions must evaluate to a scalar"), false;

			// Load condition and convert to boolean value as required by 'OpBranchConditional'
			add_cast_operation(condition, { spirv_type::t_bool, 1, 1 });
			const spv::Id condition_value = access_chain_load(block, condition);
			assert(condition_value != 0);

			add_instruction_without_result(block, location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(selection_control); // Selection Control

			leave_block_and_branch_conditional(block, condition_value, true_label, false_label);

			{ // Then block of the if statement
				enter_block(block, true_label);

				if (!parse_statement(block, true))
					return false;

				leave_block_and_branch(block, merge_label);
			}
			{ // Else block of the if statement
				enter_block(block, false_label);

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
			const spv::Id merge_label = make_id();
			spv::Id default_label = merge_label; // The default case jumps to the end of the switch statement if not overwritten

			spirv_expression selector;
			if (!expect('(') || !parse_expression(block, selector) || !expect(')'))
				return false;
			else if (!selector.type.is_scalar())
				return error(selector.location, 3019, "switch statement expression must evaluate to a scalar"), false;

			// Load selector and convert to integral value as required by 'OpSwitch'
			add_cast_operation(selector, { spirv_type::t_int, 1, 1 });
			const spv::Id selector_value = access_chain_load(block, selector);
			assert(selector_value != 0);

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
						spirv_expression case_label;
						if (!parse_expression(switch_body_block, case_label))
							return consume_until('}'), false;
						else if (!case_label.type.is_scalar() || !case_label.is_constant)
							return error(case_label.location, 3020, "non-numeric case expression"), consume_until('}'), false;

						case_literal_and_labels.push_back(case_label.constant.as_uint[0]); // This can be floating point too, which are casted here via the constant union
						case_literal_and_labels.push_back(current_block);
					}
					else
					{
						default_label = current_block;
					}

					if (!expect(':'))
						return consume_until('}'), false;

					num_case_labels++;
				}

				if (!parse_statement(switch_body_block, true))
					return consume_until('}'), false;
			}

			if (num_case_labels == 0)
				warning(location, 5002, "switch statement contains no 'case' or 'default' labels");

			// Add all case labels to the switch instruction (reference is still valid because all other instructions were written to the intermediate 'switch_body_block' in the mean time)
			switch_instruction.add(default_label);
			switch_instruction.add(case_literal_and_labels.begin(), case_literal_and_labels.end());

			block.append(std::move(switch_body_block));

			enter_block(block, merge_label);

			return expect('}');
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
			if (spirv_type type; parse_type(type))
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
				spirv_expression expression;
				parse_expression(block, expression); // It is valid for there to be no initializer expression, so ignore result
			}

			if (!expect(';'))
				return false;

			const spv::Id header_label = make_id(); // Pointer to the loop merge instruction
			const spv::Id loop_label = make_id(); // Pointer to the main loop body block
			const spv::Id merge_label = make_id(); // Pointer to the end of the loop
			const spv::Id continue_label = make_id(); // Pointer to the continue block
			const spv::Id condition_label = make_id(); // Pointer to the condition check

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

				spirv_expression condition;
				if (parse_expression(block, condition))
				{
					if (!condition.type.is_scalar())
						return error(condition.location, 3019, "scalar value expected"), false;

					// Evaluate condition and branch to the right target
					add_cast_operation(condition, { spirv_type::t_bool, 1, 1 });
					const spv::Id condition_value = access_chain_load(block, condition);
					assert(condition_value != 0);

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

				spirv_expression continue_exp;
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

			const spv::Id header_label = make_id();
			const spv::Id loop_label = make_id();
			const spv::Id merge_label = make_id();
			const spv::Id continue_label = make_id();
			const spv::Id condition_label = make_id();

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

				spirv_expression condition;
				if (!expect('(') || !parse_expression(block, condition) || !expect(')'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				add_cast_operation(condition, { spirv_type::t_bool, 1, 1 });
				const spv::Id condition_value = access_chain_load(block, condition);
				assert(condition_value != 0);

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
			const spv::Id header_label = make_id();
			const spv::Id loop_label = make_id();
			const spv::Id merge_label = make_id();
			const spv::Id continue_label = make_id();

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

				spirv_expression condition;
				if (!expect(tokenid::while_) || !expect('(') || !parse_expression(block, condition) || !expect(')') || !expect(';'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				add_cast_operation(condition, { spirv_type::t_bool, 1, 1 });
				const spv::Id condition_value = access_chain_load(block, condition);
				assert(condition_value != 0);

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
				spirv_expression expression;
				if (!parse_expression(block, expression))
					return consume_until(';'), false;

				// Cannot return to void
				if (parent->return_type.is_void())
					// Consume the semicolon that follows the return expression so that parsing may continue
					return error(location, 3079, "void functions cannot return a value"), accept(';'), false;

				// Cannot return arrays from a function
				if (expression.type.is_array() || !spirv_type::rank(expression.type, parent->return_type))
					return error(location, 3017, "expression does not match function return type"), accept(';'), false;
				else if (expression.type.components() > parent->return_type.components())
					warning(expression.location, 3206, "implicit truncation of vector type");

				// Load return value and perform implicit cast to function return type
				add_cast_operation(expression, parent->return_type);
				const spv::Id return_value = access_chain_load(block, expression);
				assert(return_value != 0);

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
	if (spirv_type type; parse_type(type))
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
	if (spirv_expression expression; parse_expression(block, expression))
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
		spirv_type member_type;
		if (!parse_type(member_type))
			return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected struct member type"), consume_until('}'), false;

		if (member_type.is_void())
			return error(_token_next.location, 3038, "struct members cannot be void"), consume_until('}'), false;
		if (member_type.has(spirv_type::q_in) || member_type.has(spirv_type::q_out))
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

				member_info.builtin = semantic_to_builtin(_token.literal_as_string, member_info.semantic_index);
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
bool reshadefx::parser::parse_function(spirv_type type, std::string name)
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
		if (param.type.has(spirv_type::q_extern))
			return error(param_location, 3006, "function parameters cannot be declared 'extern'"), false;
		if (param.type.has(spirv_type::q_static))
			return error(param_location, 3007, "function parameters cannot be declared 'static'"), false;
		if (param.type.has(spirv_type::q_uniform))
			return error(param_location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead"), false;

		if (param.type.has(spirv_type::q_out) && param.type.has(spirv_type::q_const))
			return error(param_location, 3046, "output parameters cannot be declared 'const'"), false;
		else if (!param.type.has(spirv_type::q_out))
			param.type.qualifiers |= spirv_type::q_in; // Function parameters are implicitly 'in' if not explicitly defined as 'out'

		if (!parse_array_size(param.type))
			return false;

		// Handle parameter type semantic
		if (accept(':'))
		{
			if (!expect(tokenid::identifier))
				return false;

			param.builtin = semantic_to_builtin(_token.literal_as_string, param.semantic_index);
		}

		param.type.is_ptr = true;

		const spv::Id definition = make_id();
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

		info.return_builtin = semantic_to_builtin(_token.literal_as_string, info.return_semantic_index);
	}

	// A function has to start with a new block
	enter_block(_functions2[_current_function].variables, make_id());

	const bool success = parse_statement_block(_functions2[_current_function].definition, false);

	// Add implicit return statement to the end of functions
	if (_current_block != 0)
		leave_block_and_return(_functions2[_current_function].definition, 0);

	// Insert the symbol into the symbol table
	symbol symbol = { spv::OpFunction, info.definition, { spirv_type::t_function } };
	symbol.function = &info;

	if (!insert_symbol(name, symbol, true))
		return error(location, 3003, "redefinition of '" + name + "'"), false;

	return success;
}
bool reshadefx::parser::parse_variable(spirv_type type, std::string name, spirv_basic_block &block, bool global)
{
	const auto location = std::move(_token.location);

	spirv_variable_info info;

	if (type.is_void())
		return error(location, 3038, "variables cannot be void"), false;
	if (type.has(spirv_type::q_in) || type.has(spirv_type::q_out))
		return error(location, 3055, "variables cannot be declared 'in' or 'out'"), false;

	// Check that qualifier combinations are valid
	if (global)
	{
		if (type.has(spirv_type::q_static))
		{
			if (type.has(spirv_type::q_uniform))
				return error(location, 3007, "uniform global variables cannot be declared 'static'"), false;
		}
		else
		{
			if (!type.has(spirv_type::q_uniform) && !(type.is_texture() || type.is_sampler()))
				warning(location, 5000, "global variables are considered 'uniform' by default");

			if (type.has(spirv_type::q_const))
				return error(location, 3035, "variables which are 'uniform' cannot be declared 'const'"), false;

			// Global variables that are not 'static' are always 'extern' and 'uniform'
			type.qualifiers |= spirv_type::q_extern | spirv_type::q_uniform;
		}
	}
	else
	{
		if (type.has(spirv_type::q_extern))
			return error(location, 3006, "local variables cannot be declared 'extern'"), false;
		if (type.has(spirv_type::q_uniform))
			return error(location, 3047, "local variables cannot be declared 'uniform'"), false;

		if (type.is_texture() || type.is_sampler())
			return error(location, 3038, "local variables cannot be textures or samplers"), false;
	}

	// The variable name may be followed by an optional array size expression
	if (!parse_array_size(type))
		return false;

	info.name = name;
	info.unique_name = global ? (type.has(spirv_type::q_uniform) ? 'U' : 'V') + current_scope().name + name : name;
	std::replace(info.unique_name.begin(), info.unique_name.end(), ':', '_');

	spirv_expression initializer;
	std::unordered_map<std::string, spirv_constant> annotation_list;

	if (accept(':'))
	{
		if (!expect(tokenid::identifier))
			return false;
		else if (!global) // Only global variables can have a semantic
			return error(_token.location, 3043, "local variables cannot have semantics"), false;

		info.builtin = semantic_to_builtin(_token.literal_as_string, info.semantic_index);
		info.semantic = std::move(_token.literal_as_string);
	}
	else
	{
		// Global variables can have optional annotations
		if (global && !parse_annotations(annotation_list))
			return false;

		// Variables without a semantic may have an optional initializer
		if (accept('='))
		{
			if (!parse_expression_assignment(block, initializer))
				return false;

			if (global && !initializer.is_constant) // TODO: This could be resolved by initializing these at the beginning of the entry point
				return error(initializer.location, 3011, "initial value must be a literal expression"), false;

			// Check type compatibility
			if ((type.array_length >= 0 && initializer.type.array_length != type.array_length) || !spirv_type::rank(initializer.type, type))
				return error(initializer.location, 3017, "initial value does not match variable type"), false;
			if ((initializer.type.rows < type.rows || initializer.type.cols < type.cols) && !initializer.type.is_scalar())
				return error(initializer.location, 3017, "cannot implicitly convert these vector types"), false;
			else if (initializer.type.components() > type.components())
				warning(initializer.location, 3206, "implicit truncation of vector type");

			// Deduce array size from the initializer expression
			if (initializer.type.is_array())
				type.array_length = initializer.type.array_length;

			// Perform implicit cast from initializer expression to variable type
			add_cast_operation(initializer, type);
		}
		else if (type.is_numeric()) // Numeric variables without an initializer need special handling
		{
			if (type.has(spirv_type::q_const)) // Constants have to have an initial value
				return error(location, 3012, "missing initial value for '" + name + "'"), false;
			else if (!type.has(spirv_type::q_uniform)) // Zero initialize all global variables
				initializer.reset_to_rvalue_constant(location, {}, type);
		}
		else if (peek('{')) // Non-numeric variables can have a property block
		{
			if (!parse_variable_properties(info))
				return false;
		}
	}

	symbol symbol;

	if (type.is_numeric() && type.has(spirv_type::q_const) && initializer.is_constant) // Variables with a constant initializer and constant type are named constants
	{
		symbol = { spv::OpConstant, 0, type, initializer.constant };
	}
	else if (type.is_texture()) // Textures are not written to the output
	{
		symbol = { spv::OpVariable, make_id(), type };

		_texture_semantics[symbol.id] = info.semantic;
	}
	else if (type.is_sampler()) // Samplers are actually combined image samplers
	{
		if (info.texture == 0)
			return error(location, 3012, "missing 'Texture' property for '" + name + "'"), false;

		info.semantic = _texture_semantics[info.texture];

		type.is_ptr = true;

		info.definition = make_id();
		define_variable(info.definition, info.unique_name.c_str(), location, type, global ? spv::StorageClassUniformConstant : spv::StorageClassFunction);

		if (!info.semantic.empty())
			add_decoration(info.definition, spv::DecorationHlslSemanticGOOGLE, info.semantic.c_str());

		symbol = { spv::OpVariable, info.definition, type };
	}
	else if (type.has(spirv_type::q_uniform)) // Uniform variables are put into a global uniform buffer structure
	{
		if (_global_ubo_type == 0)
			_global_ubo_type = make_id();
		if (_global_ubo_variable == 0)
			_global_ubo_variable = make_id();

		// Convert boolean uniform variables to integer type so that they have a defined size
		if (type.is_boolean())
			type.base = spirv_type::t_uint;

		spirv_struct_member_info member;
		member.name = name;
		member.type = type;
		member.builtin = info.builtin;

		const size_t member_index = _uniforms.member_list.size();

		_uniforms.member_list.push_back(std::move(member));

		symbol = { spv::OpAccessChain, _global_ubo_variable, type };

		symbol.function = reinterpret_cast<const spirv_function_info *>(member_index);

		add_member_name(_global_ubo_type, member_index, name.c_str());

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
		type.is_ptr = true;

		info.definition = make_id();

		if (initializer.is_constant) // The initializer expression for 'OpVariable' must be a constant
		{
			define_variable(info.definition, info.unique_name.c_str(), location, type, global ? spv::StorageClassPrivate : spv::StorageClassFunction, convert_constant(initializer.type, initializer.constant));
		}
		else // Non-constant initializers are explicitly stored in the variable at the definition location instead
		{
			const spv::Id initializer_value = access_chain_load(block, initializer);

			define_variable(info.definition, info.unique_name.c_str(), location, type, global ? spv::StorageClassPrivate : spv::StorageClassFunction);

			if (initializer_value != 0)
			{
				assert(!global); // Global variables cannot have a dynamic initializer

				spirv_expression variable; variable.reset_to_lvalue(location, info.definition, type);

				access_chain_store(block, variable, initializer_value, initializer.type);
			}
		}

		symbol = { spv::OpVariable, info.definition, type };
	}

	// Insert the symbol into the symbol table
	if (!insert_symbol(name, symbol, global))
		return error(location, 3003, "redefinition of '" + name + "'"), false;

	return true;
}
bool reshadefx::parser::parse_variable_properties(spirv_variable_info &props)
{
	if (!expect('{'))
		return false;

	while (!peek('}'))
	{
		if (!expect(tokenid::identifier))
			return consume_until('}'), false;

		const auto name = std::move(_token.literal_as_string);
		const auto location = std::move(_token.location);

		if (!expect('='))
			return consume_until('}'), false;

		backup();

		spirv_expression expression;

		if (accept(tokenid::identifier)) // Handle special enumeration names for property values
		{
			// Transform identifier to uppercase to do case-insensitive comparison
			std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), ::toupper);

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
		if (spirv_basic_block block; !expression.is_constant && !parse_expression_multary(block, expression))
			return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected expression"), consume_until('}'), false;

		if (name == "Texture")
		{
			if (!expression.type.is_texture())
				return error(location, 3020, "type mismatch, expected texture name"), consume_until('}'), false;

			props.texture = expression.base;
		}
		else
		{
			if (!expression.is_constant || !expression.type.is_scalar())
				return error(expression.location, 3011, "value must be a literal scalar expression"), consume_until('}'), false;

			// All states below expect the value to be of an unsigned integer type
			add_cast_operation(expression, { spirv_type::t_uint, 1, 1 });
			const unsigned int value = expression.constant.as_uint[0];

			if (name == "Width")
				props.width = value > 0 ? value : 1;
			else if (name == "Height")
				props.height = value > 0 ? value : 1;
			else if (name == "MipLevels")
				props.levels = value > 0 ? value : 1;
			else if (name == "Format")
				props.format = value;
			else if (name == "SRGBTexture" || name == "SRGBReadEnable")
				props.srgb_texture = value != 0;
			else if (name == "AddressU")
				props.address_u = value;
			else if (name == "AddressV")
				props.address_v = value;
			else if (name == "AddressW")
				props.address_w = value;
			else if (name == "MinFilter")
				props.filter = (props.filter & 0x0F) | ((value << 4) & 0x30);
			else if (name == "MagFilter")
				props.filter = (props.filter & 0x33) | ((value << 2) & 0x0C);
			else if (name == "MipFilter")
				props.filter = (props.filter & 0x3C) | (value & 0x03);
			else if (name == "MinLOD" || name == "MaxMipLevel")
				props.min_lod = static_cast<float>(value);
			else if (name == "MaxLOD")
				props.max_lod = static_cast<float>(value);
			else if (name == "MipLODBias" || name == "MipMapLodBias")
				props.lod_bias = static_cast<float>(value);
			else
				return error(location, 3004, "unrecognized property '" + name + "'"), consume_until('}'), false;
		}

		if (!expect(';'))
			return consume_until('}'), false;
	}

	return expect('}');
}

bool reshadefx::parser::parse_technique()
{
	if (!expect(tokenid::identifier))
		return false;

	const auto name = std::move(_token.literal_as_string);
	std::vector<spirv_pass_info> pass_list;
	std::unordered_map<std::string, spirv_constant> annotation_list;

	if (!parse_annotations(annotation_list) || !expect('{'))
		return false;

	while (!peek('}'))
	{
		if (spirv_pass_info pass; parse_technique_pass(pass))
			pass_list.push_back(std::move(pass));
		else if (!peek(tokenid::pass)) // If there is another pass definition following, try to parse that despite the error
			return consume_until('}'), false;
	}

	_encoded_data += 'T' + name + ':' + std::to_string(pass_list.size()) + ':';

	for (const auto &pass : pass_list)
	{
		_encoded_data += pass.vs_entry_point + '|' + pass.ps_entry_point + '|';
	}

	_encoded_data.pop_back();

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
					return error(location, 3004, "undeclared identifier '" + identifier + "', expected function name"), consume_until('}'), false;
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
					define_function(function_info->entry_point, nullptr, location, { spirv_type::t_void });

					enter_block(_functions2[_current_function].variables, make_id());

					spirv_basic_block &section = _functions2[_current_function].definition;

					const auto create_input_param = [this, &section, &call_params](const spirv_struct_member_info &param) {
						const spv::Id function_variable = make_id();
						define_variable(function_variable, nullptr, {}, param.type, spv::StorageClassFunction);
						call_params.push_back(function_variable);
						return function_variable;
					};
					const auto create_input_variable = [this, &inputs_and_outputs, is_ps](const spirv_struct_member_info &param) {
						spirv_type input_type = param.type;
						input_type.is_input = true;
						input_type.is_ptr = true;

						const spv::Id input_variable = make_id();
						define_variable(input_variable, nullptr, {}, input_type, spv::StorageClassInput);

						if (is_ps && param.builtin == spv::BuiltInPosition)
							add_builtin(input_variable, spv::BuiltInFragCoord);
						else if (param.builtin != spv::BuiltInMax)
							add_builtin(input_variable, param.builtin);
						else
							add_decoration(input_variable, spv::DecorationLocation, { param.semantic_index });

						if (param.type.has(spirv_type::q_noperspective))
							add_decoration(input_variable, spv::DecorationNoPerspective);
						if (param.type.has(spirv_type::q_centroid))
							add_decoration(input_variable, spv::DecorationCentroid);
						if (param.type.has(spirv_type::q_nointerpolation))
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
					const auto create_output_variable = [this, &inputs_and_outputs](const spirv_struct_member_info &param) {
						spirv_type output_type = param.type;
						output_type.is_output = true;
						output_type.is_ptr = true;

						const spv::Id output_variable = make_id();
						define_variable(output_variable, nullptr, {}, output_type, spv::StorageClassOutput);

						if (param.builtin != spv::BuiltInMax)
							add_builtin(output_variable, param.builtin);
						else
							add_decoration(output_variable, spv::DecorationLocation, { param.semantic_index });

						if (param.type.has(spirv_type::q_noperspective))
							add_decoration(output_variable, spv::DecorationNoPerspective);
						if (param.type.has(spirv_type::q_centroid))
							add_decoration(output_variable, spv::DecorationCentroid);
						if (param.type.has(spirv_type::q_nointerpolation))
							add_decoration(output_variable, spv::DecorationFlat);

						inputs_and_outputs.push_back(output_variable);
						return output_variable;
					};

					// Handle input parameters
					for (const spirv_struct_member_info &param : function_info->parameter_list)
					{
						if (param.type.has(spirv_type::q_out))
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

									spirv_type value_type = member.type;
									value_type.is_ptr = false;

									const spv::Id value = add_intruction(section, {}, spv::OpLoad, convert_type(value_type))
										.add(input_variable)
										.result;
									elements.push_back(value);
								}

								spirv_type composite_type = param.type;
								composite_type.is_ptr = false;
								spirv_instruction &construct = add_intruction(section, {}, spv::OpCompositeConstruct, convert_type(composite_type));
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

								spirv_type value_type = param.type;
								value_type.is_ptr = false;

								const spv::Id value = add_intruction(section, {}, spv::OpLoad, convert_type(value_type))
									.add(input_variable)
									.result;
								add_instruction_without_result(section, {}, spv::OpStore)
									.add(param_variable)
									.add(value);
							}
						}
					}

					spirv_instruction &call = add_intruction(section, location, spv::OpFunctionCall, convert_type(function_info->return_type))
						.add(function_info->definition);
					for (auto elem : call_params)
						call.add(elem);
					const spv::Id call_result = call.result;

					size_t param_index = 0;
					size_t inputs_and_outputs_index = 0;
					for (const spirv_struct_member_info &param : function_info->parameter_list)
					{
						if (param.type.has(spirv_type::q_out))
						{
							spirv_type value_type = param.type;
							value_type.is_ptr = false;

							const spv::Id value = add_intruction(section, {}, spv::OpLoad, convert_type(value_type))
								.add(call_params[param_index++])
								.result;

							if (param.type.is_struct())
							{
								size_t member_index = 0;
								for (const auto &member : _structs[param.type.definition].member_list)
								{
									const spv::Id member_value = add_intruction(section, {}, spv::OpCompositeExtract, convert_type(member.type))
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

							spv::Id member_result = add_intruction(section, {}, spv::OpCompositeExtract, convert_type(member.type))
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
						spirv_type ptr_type = function_info->return_type;
						ptr_type.is_output = true;
						ptr_type.is_ptr = true;

						const spv::Id result = make_id();
						define_variable(result, nullptr, location, ptr_type, spv::StorageClassOutput);

						if (function_info->return_builtin != spv::BuiltInMax)
							add_builtin(result, function_info->return_builtin);
						else
							add_decoration(result, spv::DecorationLocation, { function_info->return_semantic_index });

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
				if (is_ps)
					info.ps_entry_point = function_info->name;
			}
			else
			{
				if (!symbol.id)
					return error(location, 3004, "undeclared identifier '" + identifier + "', expected texture name"), consume_until('}'), false;
				else if (!symbol.type.is_texture())
					return error(location, 3020, "type mismatch, expected texture name"), consume_until('}'), false;

				const size_t target_index = state.size() > 12 ? (state[12] - '0') : 0;

				info.render_targets[target_index] = symbol.id;
			}
		}
		else // Handle the rest of the pass states
		{
			backup();

			spirv_expression expression;

			if (accept(tokenid::identifier)) // Handle special enumeration names for pass states
			{
				// Transform identifier to uppercase to do case-insensitive comparison
				std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), ::toupper);

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
			if (spirv_basic_block block; !expression.is_constant && !parse_expression_multary(block, expression))
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected expression"), consume_until('}'), false;
			else if (!expression.is_constant || !expression.type.is_scalar())
				return error(expression.location, 3011, "pass state value must be a literal scalar expression"), consume_until('}'), false;

			// All states below expect the value to be of an unsigned integer type
			add_cast_operation(expression, { spirv_type::t_uint, 1, 1 });
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
