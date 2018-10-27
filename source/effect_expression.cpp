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

void reshadefx::expression::add_cast_operation(const reshadefx::type &in_type)
{
	if (type == in_type)
		return; // There is nothing to do if the expression is already of the target type

	if (is_constant)
	{
		const auto cast_constant = [](reshadefx::constant &constant, const reshadefx::type &from, const reshadefx::type &to) {
			if (to.is_integral() || to.is_boolean())
			{
				if (!from.is_integral())
				{
					for (unsigned int i = 0; i < 16; ++i)
						constant.as_uint[i] = static_cast<int>(constant.as_float[i]);
				}
				else
				{
					// int 2 uint
				}
			}
			else
			{
				if (!from.is_integral() && !from.is_boolean())
				{
					// Scalar to vector promotion
					assert(from.is_floating_point() && from.is_scalar() && to.is_vector());
					for (unsigned int i = 1; i < to.components(); ++i)
						constant.as_float[i] = constant.as_float[0];
				}
				else
				{
					if (from.is_scalar())
					{
						const float value = static_cast<float>(static_cast<int>(constant.as_uint[0]));
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_float[i] = value;
					}
					else
					{
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_float[i] = static_cast<float>(static_cast<int>(constant.as_uint[i]));
					}
				}
			}
		};

		for (size_t i = 0; i < constant.array_data.size(); ++i)
		{
			cast_constant(constant.array_data[i], type, in_type);
		}

		cast_constant(constant, type, in_type);
	}
	else
	{
		if (type.is_vector() && in_type.is_scalar())
		{
			ops.push_back({ operation::op_swizzle, type, in_type, 0, { 0, -1, -1, -1 } });
		}
		else
		{
			ops.push_back({ operation::op_cast, type, in_type });
		}
	}

	type = in_type;
	//is_lvalue = false; // Can't do this because of 'if (chain.is_lvalue)' check in 'add_load_operation'
}
void reshadefx::expression::add_member_access(uint32_t index, const reshadefx::type &in_type)
{
	reshadefx::type target_type = in_type;
	target_type.is_ptr = true;

	ops.push_back({ operation::op_member, type, target_type, index });

	type = in_type;
	is_constant = false;
}
void reshadefx::expression::add_static_index_access(reshadefx::codegen *codegen, uint32_t index)
{
	if (!is_constant)
	{
		constant.as_uint[0] = index;
		memset(&constant.as_uint[1], 0, sizeof(uint32_t) * 15); // Clear the reset of the constant
		return add_dynamic_index_access(codegen, codegen->emit_constant({ type::t_uint, 1, 1 }, constant));
	}

	if (type.is_array())
	{
		type.array_length = 0;

		constant = constant.array_data[index];
	}
	else if (type.is_matrix()) // Indexing into a matrix returns a row of it as a vector
	{
		type.rows = type.cols, type.cols = 1;

		for (unsigned int i = 0; i < 4; ++i)
			constant.as_uint[i] = constant.as_uint[index * 4 + i];
		memset(&constant.as_uint[4], 0, sizeof(uint32_t) * 12); // Clear the reset of the constant
	}
	else if (type.is_vector()) // Indexing into a vector returns the element as a scalar
	{
		type.rows = 1;

		constant.as_uint[0] = constant.as_uint[index];
		memset(&constant.as_uint[1], 0, sizeof(uint32_t) * 15); // Clear the reset of the constant
	}
	else
	{
		assert(false);
	}
}
void reshadefx::expression::add_dynamic_index_access(reshadefx::codegen *codegen, reshadefx::codegen::id index_expression)
{
	reshadefx::type target_type = type;
	if (target_type.is_array())
		target_type.array_length = 0;
	else if (target_type.is_matrix())
		target_type.rows = target_type.cols,
		target_type.cols = 1;
	else if (target_type.is_vector())
		target_type.rows = 1;

	target_type.is_ptr = true; // OpAccessChain returns a pointer

	if (is_constant)
	{
		auto index_constant = codegen->emit_constant(type, constant);
		type.is_ptr = true;
		base = codegen->define_variable(location, type, nullptr, false, index_constant);
		type.is_ptr = false;
		is_lvalue = true;
	}

	ops.push_back({ operation::op_index, type, target_type, index_expression });

	type = target_type;
	type.is_ptr = false;
	is_constant = false;
}
void reshadefx::expression::add_swizzle_access(signed char in_swizzle[4], size_t length)
{
	// Check for redundant swizzle and ignore them
	//if (type.is_vector() && length == type.rows)
	//{
	//	bool ignore = true;
	//	for (unsigned int i = 0; i < length; ++i)
	//		if (in_swizzle[i] != i)
	//			ignore = false;
	//	if (ignore)
	//		return;
	//}

	reshadefx::type target_type = type;
	target_type.rows = static_cast<unsigned int>(length);
	target_type.cols = 1;

	if (is_constant)
	{
		uint32_t data[16];
		memcpy(data, &constant.as_uint[0], sizeof(data));
		for (size_t i = 0; i < length; ++i)
			constant.as_uint[i] = data[in_swizzle[i]];
		memset(&constant.as_uint[length], 0, sizeof(uint32_t) * (16 - length)); // Clear the reset of the constant
	}
	else
	{
		ops.push_back({ operation::op_swizzle, type, target_type, 0, { in_swizzle[0], in_swizzle[1], in_swizzle[2], in_swizzle[3] } });
	}

	type = target_type;
	type.is_ptr = false;
}

void reshadefx::expression::evaluate_constant_expression(reshadefx::tokenid op)
{
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
	switch (op)
	{
	case tokenid::percent:
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] = fmodf(constant.as_float[i], rhs.as_float[i]);
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_int[i] %= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] %= rhs.as_uint[i];
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
		if (type.is_floating_point())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_float[i] /= rhs.as_float[i];
		else if (type.is_signed())
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_int[i] /= rhs.as_int[i];
		else
			for (unsigned int i = 0; i < type.components(); ++i)
				constant.as_uint[i] /= rhs.as_uint[i];
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
