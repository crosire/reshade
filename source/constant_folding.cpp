#include "syntax_tree.hpp"
#include "constant_folding.hpp"

#include <algorithm>

namespace reshade
{
	namespace fx
	{
		using namespace nodes;

		void scalar_literal_cast(const literal_expression_node *from, size_t i, int &to)
		{
			switch (from->type.basetype)
			{
				case type_node::datatype_bool:
				case type_node::datatype_int:
				case type_node::datatype_uint:
					to = from->value_int[i];
					break;
				case type_node::datatype_float:
					to = static_cast<int>(from->value_float[i]);
					break;
				default:
					to = 0;
					break;
			}
		}
		void scalar_literal_cast(const literal_expression_node *from, size_t i, unsigned int &to)
		{
			switch (from->type.basetype)
			{
				case type_node::datatype_bool:
				case type_node::datatype_int:
				case type_node::datatype_uint:
					to = from->value_uint[i];
					break;
				case type_node::datatype_float:
					to = static_cast<unsigned int>(from->value_float[i]);
					break;
				default:
					to = 0;
					break;
			}
		}
		void scalar_literal_cast(const literal_expression_node *from, size_t i, float &to)
		{
			switch (from->type.basetype)
			{
				case type_node::datatype_bool:
				case type_node::datatype_int:
					to = static_cast<float>(from->value_int[i]);
					break;
				case type_node::datatype_uint:
					to = static_cast<float>(from->value_uint[i]);
					break;
				case type_node::datatype_float:
					to = from->value_float[i];
					break;
				default:
					to = 0;
					break;
			}
		}
		void vector_literal_cast(const literal_expression_node *from, size_t k, literal_expression_node *to, size_t j)
		{
			switch (to->type.basetype)
			{
				case type_node::datatype_bool:
				case type_node::datatype_int:
					scalar_literal_cast(from, j, to->value_int[k]);
					break;
				case type_node::datatype_uint:
					scalar_literal_cast(from, j, to->value_uint[k]);
					break;
				case type_node::datatype_float:
					scalar_literal_cast(from, j, to->value_float[k]);
					break;
				default:
					memcpy(to->value_uint, from->value_uint, sizeof(from->value_uint));
					break;
			}
		}

		expression_node *fold_constant_expression(nodetree &ast, expression_node *expression)
		{
#define DOFOLDING1(op) \
		{ \
			for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; ++i) \
				switch (operand->type.basetype) \
				{ \
					case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
						switch (expression->type.basetype) \
						{ \
							case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
								operand->value_int[i] = static_cast<int>(op(operand->value_int[i])); break; \
							case type_node::datatype_float: \
								operand->value_float[i] = static_cast<float>(op(operand->value_int[i])); break; \
						} \
						break; \
					case type_node::datatype_float: \
						switch (expression->type.basetype) \
						{ \
							case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
								operand->value_int[i] = static_cast<int>(op(operand->value_float[i])); break; \
							case type_node::datatype_float: \
								operand->value_float[i] = static_cast<float>(op(operand->value_float[i])); break; \
						} \
						break; \
				} \
			expression = operand; \
		}
#define DOFOLDING2(op) \
		{ \
			literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case type_node::datatype_bool:  case type_node::datatype_int: case type_node::datatype_uint: \
						switch (right->type.basetype) \
						{ \
							case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
								result.value_int[i] = left->value_int[left_scalar ? 0 : i] op right->value_int[right_scalar ? 0 : i]; \
								break; \
							case type_node::datatype_float: \
								result.value_float[i] = static_cast<float>(left->value_int[!left_scalar * i]) op right->value_float[!right_scalar * i]; \
								break; \
						} \
						break; \
					case type_node::datatype_float: \
						result.value_float[i] = (right->type.basetype == type_node::datatype_float) ? (left->value_float[!left_scalar * i] op right->value_float[!right_scalar * i]) : (left->value_float[!left_scalar * i] op static_cast<float>(right->value_int[!right_scalar * i])); \
						break; \
				} \
			left->type = expression->type; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; \
		}
#define DOFOLDING2_INT(op) \
		{ \
			literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
			{ \
				result.value_int[i] = left->value_int[!left_scalar * i] op right->value_int[!right_scalar * i]; \
			} \
			left->type = expression->type; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; \
		}
#define DOFOLDING2_BOOL(op) \
		{ \
			literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
						result.value_int[i] = (right->type.basetype == type_node::datatype_float) ? (static_cast<float>(left->value_int[!left_scalar * i]) op right->value_float[!right_scalar * i]) : (left->value_int[!left_scalar * i] op right->value_int[!right_scalar * i]); \
						break; \
					case type_node::datatype_float: \
						result.value_int[i] = (right->type.basetype == type_node::datatype_float) ? (left->value_float[!left_scalar * i] op static_cast<float>(right->value_int[!right_scalar * i])) : (left->value_float[!left_scalar * i] op right->value_float[!right_scalar * i]); \
						break; \
				} \
			left->type = expression->type; \
			left->type.basetype = type_node::datatype_bool; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; \
		}
#define DOFOLDING2_FLOAT(op) \
		{ \
			literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case type_node::datatype_bool:  case type_node::datatype_int: case type_node::datatype_uint: \
						result.value_float[i] = (right->type.basetype == type_node::datatype_float) ? (static_cast<float>(left->value_int[!left_scalar * i]) op right->value_float[!right_scalar * i]) : (left->value_int[left_scalar ? 0 : i] op right->value_int[right_scalar ? 0 : i]); \
						break; \
					case type_node::datatype_float: \
						result.value_float[i] = (right->type.basetype == type_node::datatype_float) ? (left->value_float[!left_scalar * i] op right->value_float[!right_scalar * i]) : (left->value_float[!left_scalar * i] op static_cast<float>(right->value_int[!right_scalar * i])); \
						break; \
				} \
			left->type = expression->type; \
			left->type.basetype = type_node::datatype_float; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; \
		}
#define DOFOLDING2_FUNCTION(op) \
		{ \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
						switch (right->type.basetype) \
						{ \
							case type_node::datatype_bool: case type_node::datatype_int: case type_node::datatype_uint: \
								left->value_int[i] = static_cast<int>(op(left->value_int[i], right->value_int[i])); \
								break; \
							case type_node::datatype_float: \
								left->value_float[i] = static_cast<float>(op(static_cast<float>(left->value_int[i]), right->value_float[i])); \
								break; \
						} \
						break; \
					case type_node::datatype_float: \
						left->value_float[i] = (right->type.basetype == type_node::datatype_float) ? (static_cast<float>(op(left->value_float[i], right->value_float[i]))) : (static_cast<float>(op(left->value_float[i], static_cast<float>(right->value_int[i])))); \
						break; \
				} \
			left->type = expression->type; \
			expression = left; \
		}

			if (expression->id == nodeid::unary_expression)
			{
				const auto unaryexpression = static_cast<unary_expression_node *>(expression);

				if (unaryexpression->operand->id != nodeid::literal_expression)
				{
					return expression;
				}

				const auto operand = static_cast<literal_expression_node *>(unaryexpression->operand);

				switch (unaryexpression->op)
				{
					case unary_expression_node::negate:
						DOFOLDING1(-);
						break;
					case unary_expression_node::bitwise_not:
						for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; i++)
						{
							operand->value_int[i] = ~operand->value_int[i];
						}
						expression = operand;
						break;
					case unary_expression_node::logical_not:
						for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; i++)
						{
							operand->value_int[i] = (operand->type.basetype == type_node::datatype_float) ? !operand->value_float[i] : !operand->value_int[i];
						}
						operand->type.basetype = type_node::datatype_bool;
						expression = operand;
						break;
					case unary_expression_node::cast:
					{
						literal_expression_node old = *operand;
						operand->type = expression->type;
						expression = operand;

						for (unsigned int i = 0, size = std::min(old.type.rows * old.type.cols, operand->type.rows * operand->type.cols); i < size; ++i)
						{
							vector_literal_cast(&old, i, operand, i);
						}
						break;
					}
				}
			}
			else if (expression->id == nodeid::binary_expression)
			{
				const auto binaryexpression = static_cast<binary_expression_node *>(expression);

				if (binaryexpression->operands[0]->id != nodeid::literal_expression || binaryexpression->operands[1]->id != nodeid::literal_expression)
				{
					return expression;
				}

				const auto left = static_cast<literal_expression_node *>(binaryexpression->operands[0]);
				const auto right = static_cast<literal_expression_node *>(binaryexpression->operands[1]);
				const bool left_scalar = left->type.rows * left->type.cols == 1;
				const bool right_scalar = right->type.rows * right->type.cols == 1;

				switch (binaryexpression->op)
				{
					case binary_expression_node::add:
						DOFOLDING2(+);
						break;
					case binary_expression_node::subtract:
						DOFOLDING2(-);
						break;
					case binary_expression_node::multiply:
						DOFOLDING2(*);
						break;
					case binary_expression_node::divide:
						if (right->value_uint[0] == 0)
						{
							return expression;
						}
						DOFOLDING2_FLOAT(/ );
						break;
					case binary_expression_node::modulo:
						DOFOLDING2_FUNCTION(std::fmod);
						break;
					case binary_expression_node::less:
						DOFOLDING2_BOOL(<);
						break;
					case binary_expression_node::greater:
						DOFOLDING2_BOOL(>);
						break;
					case binary_expression_node::less_equal:
						DOFOLDING2_BOOL(<= );
						break;
					case binary_expression_node::greater_equal:
						DOFOLDING2_BOOL(>= );
						break;
					case binary_expression_node::equal:
						DOFOLDING2_BOOL(== );
						break;
					case binary_expression_node::not_equal:
						DOFOLDING2_BOOL(!= );
						break;
					case binary_expression_node::left_shift:
						DOFOLDING2_INT(<< );
						break;
					case binary_expression_node::right_shift:
						DOFOLDING2_INT(>> );
						break;
					case binary_expression_node::bitwise_and:
						DOFOLDING2_INT(&);
						break;
					case binary_expression_node::bitwise_or:
						DOFOLDING2_INT(| );
						break;
					case binary_expression_node::bitwise_xor:
						DOFOLDING2_INT(^);
						break;
					case binary_expression_node::logical_and:
						DOFOLDING2_BOOL(&&);
						break;
					case binary_expression_node::logical_or:
						DOFOLDING2_BOOL(|| );
						break;
				}
			}
			else if (expression->id == nodeid::intrinsic_expression)
			{
				const auto intrinsicexpression = static_cast<intrinsic_expression_node *>(expression);

				if ((intrinsicexpression->arguments[0] != nullptr && intrinsicexpression->arguments[0]->id != nodeid::literal_expression) || (intrinsicexpression->arguments[1] != nullptr && intrinsicexpression->arguments[1]->id != nodeid::literal_expression) || (intrinsicexpression->arguments[2] != nullptr && intrinsicexpression->arguments[2]->id != nodeid::literal_expression))
				{
					return expression;
				}

				const auto operand = static_cast<literal_expression_node *>(intrinsicexpression->arguments[0]);
				const auto left = operand;
				const auto right = static_cast<literal_expression_node *>(intrinsicexpression->arguments[1]);

				switch (intrinsicexpression->op)
				{
					case intrinsic_expression_node::abs:
						DOFOLDING1(std::abs);
						break;
					case intrinsic_expression_node::sin:
						DOFOLDING1(std::sin);
						break;
					case intrinsic_expression_node::sinh:
						DOFOLDING1(std::sinh);
						break;
					case intrinsic_expression_node::cos:
						DOFOLDING1(std::cos);
						break;
					case intrinsic_expression_node::cosh:
						DOFOLDING1(std::cosh);
						break;
					case intrinsic_expression_node::tan:
						DOFOLDING1(std::tan);
						break;
					case intrinsic_expression_node::tanh:
						DOFOLDING1(std::tanh);
						break;
					case intrinsic_expression_node::asin:
						DOFOLDING1(std::asin);
						break;
					case intrinsic_expression_node::acos:
						DOFOLDING1(std::acos);
						break;
					case intrinsic_expression_node::atan:
						DOFOLDING1(std::atan);
						break;
					case intrinsic_expression_node::exp:
						DOFOLDING1(std::exp);
						break;
					case intrinsic_expression_node::log:
						DOFOLDING1(std::log);
						break;
					case intrinsic_expression_node::log10:
						DOFOLDING1(std::log10);
						break;
					case intrinsic_expression_node::sqrt:
						DOFOLDING1(std::sqrt);
						break;
					case intrinsic_expression_node::ceil:
						DOFOLDING1(std::ceil);
						break;
					case intrinsic_expression_node::floor:
						DOFOLDING1(std::floor);
						break;
					case intrinsic_expression_node::atan2:
						DOFOLDING2_FUNCTION(std::atan2);
						break;
					case intrinsic_expression_node::pow:
						DOFOLDING2_FUNCTION(std::pow);
						break;
					case intrinsic_expression_node::min:
						DOFOLDING2_FUNCTION(std::min);
						break;
					case intrinsic_expression_node::max:
						DOFOLDING2_FUNCTION(std::max);
						break;
				}
			}
			else if (expression->id == nodeid::constructor_expression)
			{
				const auto constructor = static_cast<constructor_expression_node *>(expression);

				for (auto argument : constructor->arguments)
				{
					if (argument->id != nodeid::literal_expression)
					{
						return expression;
					}
				}

				unsigned int k = 0;
				const auto literal = ast.make_node<literal_expression_node>(constructor->location);
				literal->type = constructor->type;

				for (auto argument : constructor->arguments)
				{
					for (unsigned int j = 0; j < argument->type.rows * argument->type.cols; ++k, ++j)
					{
						vector_literal_cast(static_cast<literal_expression_node *>(argument), k, literal, j);
					}
				}

				expression = literal;
			}
			else if (expression->id == nodeid::lvalue_expression)
			{
				const auto variable = static_cast<lvalue_expression_node *>(expression)->reference;

				if (variable->initializer_expression == nullptr || !(variable->initializer_expression->id == nodeid::literal_expression && variable->type.has_qualifier(type_node::qualifier_const)))
				{
					return expression;
				}

				const auto literal = ast.make_node<literal_expression_node>(expression->location);
				literal->type = expression->type;
				expression = literal;

				for (unsigned int i = 0, size = std::min(variable->initializer_expression->type.rows * variable->initializer_expression->type.cols, literal->type.rows * literal->type.cols); i < size; ++i)
				{
					vector_literal_cast(static_cast<const literal_expression_node *>(variable->initializer_expression), i, literal, i);
				}
			}

			return expression;
		}
	}
}