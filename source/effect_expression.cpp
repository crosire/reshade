/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_expression.hpp"
#include "effect_lexer.hpp"
#include "effect_codegen.hpp"
#include <assert.h>

reshadefx::type reshadefx::type::merge(const type &lhs, const type &rhs)
{
	type result = { std::max(lhs.base, rhs.base) };

	// If one side of the expression is scalar, it needs to be promoted to the same dimension as the other side
	if ((lhs.rows == 1 && lhs.cols == 1) || (rhs.rows == 1 && rhs.cols == 1))
	{
		result.rows = std::max(lhs.rows, rhs.rows);
		result.cols = std::max(lhs.cols, rhs.cols);
	}
	else // Otherwise dimensions match or one side is truncated to match the other one
	{
		result.rows = std::min(lhs.rows, rhs.rows);
		result.cols = std::min(lhs.cols, rhs.cols);
	}

	// Some qualifiers propagate to the result
	result.qualifiers = (lhs.qualifiers & type::q_precise) | (rhs.qualifiers & type::q_precise);

	return result;
}

void reshadefx::expression::reset_to_lvalue(const reshadefx::location &loc, reshadefx::codegen::id in_base, const reshadefx::type &in_type)
{
	type = in_type;
	base = in_base;
	location = loc;
	is_lvalue = true;
	is_constant = false;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue(const reshadefx::location &loc, reshadefx::codegen::id in_base, const reshadefx::type &in_type)
{
	type = in_type;
	type.qualifiers |= type::q_const;
	base = in_base;
	location = loc;
	is_lvalue = false;
	is_constant = false;
	chain.clear();
}

void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, bool data)
{
	type = { type::t_bool, 1, 1, type::q_const };
	base = 0; constant = {}; constant.as_uint[0] = data;
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, float data)
{
	type = { type::t_float, 1, 1, type::q_const };
	base = 0; constant = {}; constant.as_float[0] = data;
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, int32_t data)
{
	type = { type::t_int,  1, 1, type::q_const };
	base = 0; constant = {}; constant.as_int[0] = data;
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, uint32_t data)
{
	type = { type::t_uint, 1, 1, type::q_const };
	base = 0; constant = {}; constant.as_uint[0] = data;
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, std::string data)
{
	type = { type::t_string, 0, 0, type::q_const };
	base = 0; constant = {}; constant.string_data = std::move(data);
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}
void reshadefx::expression::reset_to_rvalue_constant(const reshadefx::location &loc, reshadefx::constant data, const reshadefx::type &in_type)
{
	type = in_type;
	type.qualifiers |= type::q_const;
	base = 0; constant = std::move(data);
	location = loc;
	is_lvalue = false;
	is_constant = true;
	chain.clear();
}

void reshadefx::expression::add_cast_operation(const reshadefx::type &cast_type)
{
	if (type == cast_type)
		return; // There is nothing to do if the expression is already of the target type

	if (cast_type.is_scalar() && !type.is_scalar())
	{
		// A vector or matrix to scalar cast is equivalent to a swizzle
		const signed char swizzle[] = { 0, -1, -1, -1 };
		add_swizzle_access(swizzle, 1);
		return;
	}

	if (is_constant)
	{
		const auto cast_constant = [](reshadefx::constant &constant, const reshadefx::type &from, const reshadefx::type &to) {
			// Handle scalar to vector promotion first
			if (from.is_scalar() && !to.is_scalar())
				for (unsigned int i = 1; i < to.components(); ++i)
					constant.as_uint[i] = constant.as_uint[0];

			// Next check whether the type needs casting as well (and don't convert between signed/unsigned, since that is handled by the union)
			if (from.base == to.base || from.is_floating_point() == to.is_floating_point())
				return;

			if (!to.is_floating_point())
				for (unsigned int i = 0; i < to.components(); ++i)
					constant.as_uint[i] = static_cast<int>(constant.as_float[i]);
			else
				for (unsigned int i = 0; i < to.components(); ++i)
					constant.as_float[i] = static_cast<float>(constant.as_int[i]);
		};

		for (auto &element : constant.array_data)
			cast_constant(element, type, cast_type);

		cast_constant(constant, type, cast_type);
	}
	else
	{
		assert(!type.is_array() && !cast_type.is_array());

		chain.push_back({ operation::op_cast, type, cast_type });
	}

	type = cast_type;
}
void reshadefx::expression::add_member_access(unsigned int index, const reshadefx::type &in_type)
{
	const auto prev_type = type;

	type = in_type;
	is_constant = false;

	chain.push_back({ operation::op_member, prev_type, type, index });
}
void reshadefx::expression::add_static_index_access(reshadefx::codegen *codegen, unsigned int index)
{
	// A static index into a non-constant expression is functionally equivalent to a dynamic index
	if (!is_constant)
	{
		constant.as_uint[0] = index;

		return add_dynamic_index_access(codegen, codegen->emit_constant({ type::t_uint, 1, 1 }, constant));
	}

	if (type.is_array())
	{
		type.array_length = 0;

		constant = constant.array_data[index];
	}
	else if (type.is_matrix()) // Indexing into a matrix returns a row of it as a vector
	{
		type.rows = type.cols;
		type.cols = 1;

		for (unsigned int i = 0; i < 4; ++i)
			constant.as_uint[i] = constant.as_uint[index * 4 + i];
	}
	else if (type.is_vector()) // Indexing into a vector returns the element as a scalar
	{
		type.rows = 1;

		constant.as_uint[0] = constant.as_uint[index];
	}
	else
	{
		// There is no other type possible which allows indexing
		assert(false);
	}
}
void reshadefx::expression::add_dynamic_index_access(reshadefx::codegen *codegen, reshadefx::codegen::id index_expression)
{
	auto prev_type = type;

	if (type.is_array())
	{
		type.array_length = 0;
	}
	else if (type.is_matrix())
	{
		type.rows = type.cols;
		type.cols = 1;
	}
	else if (type.is_vector())
	{
		type.rows = 1;
	}

	// To handle a dynamic index into a constant means we need to create a local variable first or else any of the indexing instructions do not work
	if (is_constant)
	{
		base = codegen->define_variable(location, prev_type, std::string(), false, codegen->emit_constant(prev_type, constant));

		// We converted the constant to a l-value reference to a new variable
		is_lvalue = true;
	}

	chain.push_back({ operation::op_index, prev_type, type, index_expression });

	is_constant = false;
}
void reshadefx::expression::add_swizzle_access(const signed char in_swizzle[4], unsigned int length)
{
	const auto prev_type = type;

	type.rows = length;
	type.cols = 1;

	if (is_constant)
	{
		assert(constant.array_data.empty());

		uint32_t data[16];
		memcpy(data, &constant.as_uint[0], sizeof(data));
		for (unsigned int i = 0; i < length; ++i)
			constant.as_uint[i] = data[in_swizzle[i]];
		memset(&constant.as_uint[length], 0, sizeof(uint32_t) * (16 - length)); // Clear the rest of the constant
	}
	else
	{
		chain.push_back({ operation::op_swizzle, prev_type, type, 0, { in_swizzle[0], in_swizzle[1], in_swizzle[2], in_swizzle[3] } });
	}
}

void reshadefx::expression::evaluate_constant_expression(reshadefx::tokenid op)
{
	assert(is_constant);

	switch (op)
	{
	case tokenid::exclaim:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] = !constant.as_uint[i];
		break;
	case tokenid::minus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] = -constant.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_int[i] = -constant.as_int[i];
		break;
	case tokenid::tilde:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] = ~constant.as_uint[i];
		break;
	}
}
void reshadefx::expression::evaluate_constant_expression(reshadefx::tokenid op, const reshadefx::constant &rhs)
{
	assert(is_constant);

	switch (op)
	{
	case tokenid::percent:
		if (type.is_floating_point()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_float[i] != 0)
					constant.as_float[i] = fmodf(constant.as_float[i], rhs.as_float[i]);
		}
		else if (type.is_signed()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_int[i] != 0)
					constant.as_int[i] %= rhs.as_int[i];
		}
		else {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_uint[i] != 0)
					constant.as_uint[i] %= rhs.as_uint[i];
		}
		break;
	case tokenid::star:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] *= rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] *= rhs.as_uint[i];
		break;
	case tokenid::plus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] += rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] += rhs.as_uint[i];
		break;
	case tokenid::minus:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] -= rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] -= rhs.as_uint[i];
		break;
	case tokenid::slash:
		if (type.is_floating_point()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_float[i] != 0)
					constant.as_float[i] /= rhs.as_float[i];
		}
		else if (type.is_signed()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_int[i] != 0)
					constant.as_int[i] /= rhs.as_int[i];
		}
		else {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_uint[i] != 0)
					constant.as_uint[i] /= rhs.as_uint[i];
		}
		break;
	case tokenid::ampersand:
	case tokenid::ampersand_ampersand:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] &= rhs.as_uint[i];
		break;
	case tokenid::pipe:
	case tokenid::pipe_pipe:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] |= rhs.as_uint[i];
		break;
	case tokenid::caret:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] ^= rhs.as_uint[i];
		break;
	case tokenid::less:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] < rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_int[i] < rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] < rhs.as_uint[i];
		break;
	case tokenid::less_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] <= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_int[i] <= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] <= rhs.as_uint[i];
		break;
	case tokenid::greater:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] > rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_int[i] > rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] > rhs.as_uint[i];
		break;
	case tokenid::greater_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] >= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_int[i] >= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] >= rhs.as_uint[i];
		break;
	case tokenid::equal_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] == rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] == rhs.as_uint[i];
		break;
	case tokenid::exclaim_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] != rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] != rhs.as_uint[i];
		break;
	case tokenid::less_less:
		for (unsigned int i = 0; i < type.components(); ++i)
			constant.as_uint[i] <<= rhs.as_uint[i];
		break;
	case tokenid::greater_greater:
		if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_int[i] >>= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] >>= rhs.as_uint[i];
		break;
	}
}
