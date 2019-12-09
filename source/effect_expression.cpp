/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_lexer.hpp"
#include "effect_codegen.hpp"
#include <cmath> // fmodf
#include <cassert>
#include <cstring> // memcpy, memset
#include <algorithm> // std::min, std::max

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
	// First try to simplify the cast with a swizzle operation (only works with scalars and vectors)
	if (type.cols == 1 && cast_type.cols == 1 && type.rows != cast_type.rows)
	{
		signed char swizzle[] = { 0, 1, 2, 3 };
		// Ignore components in a demotion cast
		for (unsigned int i = cast_type.rows; i < 4; ++i)
			swizzle[i] = -1;
		// Use the last component to fill in a promotion cast
		for (unsigned int i = type.rows; i < cast_type.rows; ++i)
			swizzle[i] = swizzle[type.rows - 1];

		add_swizzle_access(swizzle, cast_type.rows);
	}

	if (type == cast_type)
		return; // There is nothing more to do if the expression is already of the target type at this point

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
	assert(type.is_struct());

	chain.push_back({ operation::op_member, type, in_type, index });

	// The type is now the type of the member that was accessed
	type = in_type;
	is_constant = false;
}
void reshadefx::expression::add_dynamic_index_access(uint32_t index_expression)
{
	assert(type.is_numeric() && !is_constant);

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

	chain.push_back({ operation::op_dynamic_index, prev_type, type, index_expression });
}
void reshadefx::expression::add_constant_index_access(unsigned int index)
{
	assert(type.is_numeric() && !type.is_scalar());

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

	if (is_constant)
	{
		if (prev_type.is_array())
		{
			constant = constant.array_data[index];
		}
		else if (prev_type.is_matrix()) // Indexing into a matrix returns a row of it as a vector
		{
			for (unsigned int i = 0; i < 4; ++i)
				constant.as_uint[i] = constant.as_uint[index * 4 + i];
		}
		else // Indexing into a vector returns the element as a scalar
		{
			constant.as_uint[0] = constant.as_uint[index];
		}
	}
	else
	{
		chain.push_back({ operation::op_constant_index, prev_type, type, index });
	}
}
void reshadefx::expression::add_swizzle_access(const signed char swizzle[4], unsigned int length)
{
	assert(type.is_numeric() && !type.is_array());

	const auto prev_type = type;

	type.rows = length;
	type.cols = 1;

	if (is_constant)
	{
		assert(constant.array_data.empty());

		uint32_t data[16];
		std::memcpy(data, &constant.as_uint[0], sizeof(data));
		for (unsigned int i = 0; i < length; ++i)
			constant.as_uint[i] = data[swizzle[i]];
		std::memset(&constant.as_uint[length], 0, sizeof(uint32_t) * (16 - length)); // Clear the rest of the constant
	}
	else if (length == 1 && prev_type.is_vector()) // Use indexing when possible since the code generation logic is simpler in SPIR-V
	{
		chain.push_back({ operation::op_constant_index, prev_type, type, static_cast<uint32_t>(swizzle[0]) });
	}
	else
	{
		chain.push_back({ operation::op_swizzle, prev_type, type, 0, { swizzle[0], swizzle[1], swizzle[2], swizzle[3] } });
	}
}

bool reshadefx::expression::evaluate_constant_expression(reshadefx::tokenid op)
{
	if (!is_constant)
		return false;

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

	return true;
}
bool reshadefx::expression::evaluate_constant_expression(reshadefx::tokenid op, const reshadefx::constant &rhs)
{
	if (!is_constant)
		return false;

	switch (op)
	{
	case tokenid::percent:
		if (type.is_floating_point()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				// Floating point modulo with zero is defined and results in NaN
				if (rhs.as_float[i] == 0)
					constant.as_float[i] = std::numeric_limits<float>::quiet_NaN();
				else
					constant.as_float[i] = std::fmodf(constant.as_float[i], rhs.as_float[i]);
		}
		else if (type.is_signed()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				// Integer modulo with zero on the other hand is not defined, so do not fold this expression in that case
				if (rhs.as_int[i] == 0)
					return false;
				else
					constant.as_int[i] %= rhs.as_int[i];
		}
		else {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_uint[i] == 0)
					return false;
				else
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
				// Floating point division by zero is well defined and results in infinity or NaN
				constant.as_float[i] /= rhs.as_float[i];
		}
		else if (type.is_signed()) {
			for (unsigned int i = 0; i < type.components(); ++i)
				// Integer division by zero on the other hand is not defined, so do not fold this expression in that case
				if (rhs.as_int[i] == 0)
					return false;
				else
					constant.as_int[i] /= rhs.as_int[i];
		}
		else {
			for (unsigned int i = 0; i < type.components(); ++i)
				if (rhs.as_uint[i] == 0)
					return false;
				else
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
		type.base = type::t_bool; // Logic operations change the type to boolean
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
		type.base = type::t_bool;
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
		type.base = type::t_bool;
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
		type.base = type::t_bool;
		break;
	case tokenid::equal_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] == rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] == rhs.as_uint[i];
		type.base = type::t_bool;
		break;
	case tokenid::exclaim_equal:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_float[i] != rhs.as_float[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] = constant.as_uint[i] != rhs.as_uint[i];
		type.base = type::t_bool;
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

	return true;
}
