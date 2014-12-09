#include "EffectOptimizer.hpp"

#include <cmath>

namespace ReShade
{
	namespace
	{
		template <typename T>
		inline T rcp(T val)
		{
			return static_cast<T>(1) / val;
		}
		template <typename T>
		inline T exp2(T val)
		{
			return static_cast<T>(std::pow(static_cast<T>(2), val));
		}
		template <typename T>
		inline T log2(T val)
		{
			return static_cast<T>(std::log(val) / std::log(static_cast<T>(2)));
		}
		template <typename T>
		inline T rsqrt(T val)
		{
			return static_cast<T>(static_cast<T>(1) / std::sqrt(val));
		}
		template <typename T>
		inline T frac(T val)
		{
			return static_cast<T>(val - std::floor(val));
		}
		template <typename T>
		inline T trunc(T val)
		{
			return static_cast<T>(val > 0 ? std::floor(val) : std::ceil(val));
		}
		template <typename T>
		inline T round(T val)
		{
			return static_cast<T>(frac(val) < 0.5f ? std::floor(val) : std::ceil(val));
		}
		template <typename T>
		inline T saturate(T val)
		{
			return std::max(static_cast<T>(0), std::min(val, static_cast<T>(1)));
		}
		template <typename T>
		inline T radians(T val)
		{
			return static_cast<T>(val * (3.14159265358979323846f / 180));
		}
		template <typename T>
		inline T degrees(T val)
		{
			return static_cast<T>(val * (180 / 3.14159265358979323846f));
		}
	}

	EffectTree::Index OptimizeExpression(EffectTree &ast, EffectTree::Index index)
	{
		EffectNodes::Expression &expression = ast[index].As<EffectNodes::Expression>(); \

		#pragma region Constant Folding
#define FOLD_UNARY(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &operand = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < operand.Type.Rows * operand.Type.Cols; ++i) \
			{ \
				switch (operand.Type.Class) \
				{ \
					case EffectNodes::Type::Bool: \
					case EffectNodes::Type::Int: \
					case EffectNodes::Type::Uint: \
						switch (expression.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								operand.Value.Int[i] = static_cast<int>(op(operand.Value.Int[i])); \
								break; \
							case EffectNodes::Type::Float: \
								operand.Value.Float[i] = static_cast<float>(op(operand.Value.Int[i])); \
								break; \
						} \
						break; \
					case EffectNodes::Type::Float: \
						switch (expression.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								operand.Value.Int[i] = static_cast<int>(op(operand.Value.Float[i])); \
								break; \
							case EffectNodes::Type::Float: \
								operand.Value.Float[i] = static_cast<float>(op(operand.Value.Float[i])); \
								break; \
						} \
						break; \
				} \
			} \
			\
			index = operand.Index; \
		}
#define FOLD_UNARY_INT(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &operand = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < operand.Type.Rows * operand.Type.Cols; ++i) \
			{ \
				operand.Value.Int[i] = op(operand.Value.Int[i]); \
			} \
			\
			index = operand.Index; \
			\
		}
#define FOLD_UNARY_BOOL(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &operand = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < operand.Type.Rows * operand.Type.Cols; ++i) \
			{ \
				switch (operand.Type.Class) \
				{ \
					case EffectNodes::Type::Bool: \
					case EffectNodes::Type::Int: \
					case EffectNodes::Type::Uint: \
						operand.Value.Bool[i] = op(operand.Value.Int[i]); \
						break; \
					case EffectNodes::Type::Float: \
						operand.Value.Bool[i] = op(operand.Value.Float[i]); \
						break; \
				} \
			} \
			\
			operand.Type.Class = EffectNodes::Type::Bool; \
			index = operand.Index; \
			\
		}
#define FOLD_UNARY_FUNCTION(func) FOLD_UNARY(func)

#define FOLD_BINARY(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>() && ast[expression.Operands[1]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &left = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			EffectNodes::Literal &right = ast[expression.Operands[1]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < expression.Type.Rows * expression.Type.Cols; ++i) \
			{ \
				switch (left.Type.Class) \
				{ \
					case EffectNodes::Type::Bool: \
					case EffectNodes::Type::Int: \
					case EffectNodes::Type::Uint: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Int[i] = left.Value.Int[i] op right.Value.Int[i]; \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Float[i] = static_cast<float>(left.Value.Int[i]) op right.Value.Float[i]; \
								break; \
						} \
						break; \
					case EffectNodes::Type::Float: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Float[i] = left.Value.Float[i] op static_cast<float>(right.Value.Int[i]); \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Float[i] = left.Value.Float[i] op right.Value.Float[i]; \
								break; \
						} \
						break; \
				} \
			} \
			\
			left.Type = expression.Type; \
			index = left.Index; \
		}
#define FOLD_BINARY_INT(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>() && ast[expression.Operands[1]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &left = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			EffectNodes::Literal &right = ast[expression.Operands[1]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < expression.Type.Rows * expression.Type.Cols; ++i) \
			{ \
				left.Value.Bool[i] = left.Value.Int[i] op right.Value.Int[i]; \
			} \
			\
			left.Type = expression.Type; \
			index = left.Index; \
		}
#define FOLD_BINARY_BOOL(op) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>() && ast[expression.Operands[1]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &left = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			EffectNodes::Literal &right = ast[expression.Operands[1]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < expression.Type.Rows * expression.Type.Cols; ++i) \
			{ \
				switch (left.Type.Class) \
				{ \
					case EffectNodes::Type::Bool: \
					case EffectNodes::Type::Int: \
					case EffectNodes::Type::Uint: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Bool[i] = left.Value.Int[i] op right.Value.Int[i]; \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Bool[i] = static_cast<float>(left.Value.Int[i]) op right.Value.Float[i]; \
								break; \
						} \
						break; \
					case EffectNodes::Type::Float: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Bool[i] = left.Value.Float[i] op static_cast<float>(right.Value.Int[i]); \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Bool[i] = left.Value.Float[i] op right.Value.Float[i]; \
								break; \
						} \
						break; \
				} \
			} \
			\
			left.Type = expression.Type; \
			left.Type.Class = EffectNodes::Type::Bool;\
			index = left.Index; \
		}

#define FOLD_BINARY_FUNCTION(func) \
		if (ast[expression.Operands[0]].Is<EffectNodes::Literal>() && ast[expression.Operands[1]].Is<EffectNodes::Literal>()) \
		{ \
			EffectNodes::Literal &left = ast[expression.Operands[0]].As<EffectNodes::Literal>(); \
			EffectNodes::Literal &right = ast[expression.Operands[1]].As<EffectNodes::Literal>(); \
			\
			for (unsigned int i = 0; i < expression.Type.Rows * expression.Type.Cols; ++i) \
			{ \
				switch (left.Type.Class) \
				{ \
					case EffectNodes::Type::Bool: \
					case EffectNodes::Type::Int: \
					case EffectNodes::Type::Uint: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Int[i] = static_cast<int>(func(left.Value.Int[i], right.Value.Int[i])); \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Float[i] = static_cast<float>(func(static_cast<float>(left.Value.Int[i]), right.Value.Float[i])); \
								break; \
						} \
						break; \
					case EffectNodes::Type::Float: \
						switch (right.Type.Class) \
						{ \
							case EffectNodes::Type::Bool: \
							case EffectNodes::Type::Int: \
							case EffectNodes::Type::Uint: \
								left.Value.Float[i] = static_cast<float>(func(left.Value.Float[i], static_cast<float>(right.Value.Int[i]))); \
								break; \
							case EffectNodes::Type::Float: \
								left.Value.Float[i] = static_cast<float>(func(left.Value.Float[i], right.Value.Float[i])); \
								break; \
						} \
						break; \
				} \
			} \
			\
			left.Type = expression.Type; \
			index = left.Index; \
		}
		#pragma endregion

		switch (expression.Operator)
		{
			case EffectNodes::Expression::Negate:
				FOLD_UNARY(-);
				break;
			case EffectNodes::Expression::BitNot:
				FOLD_UNARY_INT(~);
				break;
			case EffectNodes::Expression::LogicNot:
				FOLD_UNARY_BOOL(!);
				break;
			case EffectNodes::Expression::Increase:
				FOLD_UNARY(++);
				break;
			case EffectNodes::Expression::Decrease:
				FOLD_UNARY(--);
				break;
			case EffectNodes::Expression::Abs:
				FOLD_UNARY_FUNCTION(std::abs);
				break;
			case EffectNodes::Expression::Rcp:
				FOLD_UNARY_FUNCTION(rcp);
				break;
			case EffectNodes::Expression::Sin:
				FOLD_UNARY_FUNCTION(std::sin);
				break;
			case EffectNodes::Expression::Sinh:
				FOLD_UNARY_FUNCTION(std::sinh);
				break;
			case EffectNodes::Expression::Cos:
				FOLD_UNARY_FUNCTION(std::cos);
				break;
			case EffectNodes::Expression::Cosh:
				FOLD_UNARY_FUNCTION(std::cosh);
				break;
			case EffectNodes::Expression::Tan:
				FOLD_UNARY_FUNCTION(std::tan);
				break;
			case EffectNodes::Expression::Tanh:
				FOLD_UNARY_FUNCTION(std::tanh);
				break;
			case EffectNodes::Expression::Asin:
				FOLD_UNARY_FUNCTION(std::asin);
				break;
			case EffectNodes::Expression::Acos:
				FOLD_UNARY_FUNCTION(std::acos);
				break;
			case EffectNodes::Expression::Atan:
				FOLD_UNARY_FUNCTION(std::atan);
				break;
			case EffectNodes::Expression::Exp:
				FOLD_UNARY_FUNCTION(std::exp);
				break;
			case EffectNodes::Expression::Exp2:
				FOLD_UNARY_FUNCTION(exp2);
				break;
			case EffectNodes::Expression::Log:
				FOLD_UNARY_FUNCTION(std::log);
				break;
			case EffectNodes::Expression::Log2:
				FOLD_UNARY_FUNCTION(log2);
				break;
			case EffectNodes::Expression::Log10:
				FOLD_UNARY_FUNCTION(std::log10);
				break;
			case EffectNodes::Expression::Sqrt:
				FOLD_UNARY_FUNCTION(std::sqrt);
				break;
			case EffectNodes::Expression::Rsqrt:
				FOLD_UNARY_FUNCTION(rsqrt);
				break;
			case EffectNodes::Expression::Ceil:
				FOLD_UNARY_FUNCTION(std::ceil);
				break;
			case EffectNodes::Expression::Floor:
				FOLD_UNARY_FUNCTION(std::floor);
				break;
			case EffectNodes::Expression::Frac:
				FOLD_UNARY_FUNCTION(frac);
				break;
			case EffectNodes::Expression::Trunc:
				FOLD_UNARY_FUNCTION(trunc);
				break;
			case EffectNodes::Expression::Round:
				FOLD_UNARY_FUNCTION(round);
				break;
			case EffectNodes::Expression::Saturate:
				FOLD_UNARY_FUNCTION(saturate);
				break;
			case EffectNodes::Expression::Radians:
				FOLD_UNARY_FUNCTION(radians);
				break;
			case EffectNodes::Expression::Degrees:
				FOLD_UNARY_FUNCTION(degrees);
				break;
			case EffectNodes::Expression::Cast:
				if (ast[expression.Operands[0]].Is<EffectNodes::Literal>())
				{
					EffectNodes::Literal &node = ast[expression.Operands[0]].As<EffectNodes::Literal>();
					EffectNodes::Literal value = node;
					node.Type = expression.Type;

					for (unsigned int i = 0, size = std::min(node.Type.Rows * node.Type.Cols, value.Type.Rows * value.Type.Cols); i < size; ++i)
					{
						EffectNodes::Literal::Cast(value, i, node, i);
					}

					index = node.Index;
				}
				break;
			case EffectNodes::Expression::Add:
				FOLD_BINARY(+);
				break;
			case EffectNodes::Expression::Subtract:
				FOLD_BINARY(-);
				break;
			case EffectNodes::Expression::Multiply:
				FOLD_BINARY(*);
				break;
			case EffectNodes::Expression::Divide:
				FOLD_BINARY(/);
				break;
			case EffectNodes::Expression::Modulo:
				FOLD_BINARY_FUNCTION(std::fmod);
				break;
			case EffectNodes::Expression::Less:
				FOLD_BINARY_BOOL(<);
				break;
			case EffectNodes::Expression::Greater:
				FOLD_BINARY_BOOL(>);
				break;
			case EffectNodes::Expression::LessOrEqual:
				FOLD_BINARY_BOOL(<=);
				break;
			case EffectNodes::Expression::GreaterOrEqual:
				FOLD_BINARY_BOOL(>=);
				break;
			case EffectNodes::Expression::Equal:
				FOLD_BINARY_BOOL(==);
				break;
			case EffectNodes::Expression::NotEqual:
				FOLD_BINARY_BOOL(!=);
				break;
			case EffectNodes::Expression::LeftShift:
				FOLD_BINARY_INT(<<);
				break;
			case EffectNodes::Expression::RightShift:
				FOLD_BINARY_INT(>>);
				break;
			case EffectNodes::Expression::BitAnd:
				FOLD_BINARY_INT(&);
				break;
			case EffectNodes::Expression::BitXor:
				FOLD_BINARY_INT(^);
				break;
			case EffectNodes::Expression::BitOr:
				FOLD_BINARY_INT(|);
				break;
			case EffectNodes::Expression::LogicAnd:
				FOLD_BINARY_BOOL(&&);
				break;
			case EffectNodes::Expression::LogicXor:
				FOLD_BINARY_BOOL(!=);
				break;
			case EffectNodes::Expression::LogicOr:
				FOLD_BINARY_BOOL(||);
				break;
			case EffectNodes::Expression::Atan2:
				FOLD_BINARY_FUNCTION(std::atan2);
				break;
			case EffectNodes::Expression::Pow:
				FOLD_BINARY_FUNCTION(std::pow);
				break;
			case EffectNodes::Expression::Min:
				FOLD_BINARY_FUNCTION(std::min);
				break;
			case EffectNodes::Expression::Max:
				FOLD_BINARY_FUNCTION(std::max);
				break;
			case EffectNodes::Expression::Step:
				FOLD_BINARY(>);
				break;
		}

		return index;
	}
}