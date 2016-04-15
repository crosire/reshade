#include "symbol_table.hpp"
#include "syntax_tree_nodes.hpp"

#include <assert.h>
#include <algorithm>
#include <functional>

namespace reshade
{
	namespace fx
	{
		using namespace nodes;

		namespace
		{
			struct intrinsic
			{
				intrinsic(const std::string &name, enum intrinsic_expression_node::op op, type_node::datatype returntype, unsigned int returnrows, unsigned int returncols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
				}
				intrinsic(const std::string &name, enum intrinsic_expression_node::op op, type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
					arguments[0].type.basetype = arg0type;
					arguments[0].type.rows = arg0rows;
					arguments[0].type.cols = arg0cols;

					function.parameter_list.push_back(&arguments[0]);
				}
				intrinsic(const std::string &name, enum intrinsic_expression_node::op op, type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
					arguments[0].type.basetype = arg0type;
					arguments[0].type.rows = arg0rows;
					arguments[0].type.cols = arg0cols;
					arguments[1].type.basetype = arg1type;
					arguments[1].type.rows = arg1rows;
					arguments[1].type.cols = arg1cols;

					function.parameter_list.push_back(&arguments[0]);
					function.parameter_list.push_back(&arguments[1]);
				}
				intrinsic(const std::string &name, enum intrinsic_expression_node::op op, type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols, type_node::datatype arg2type, unsigned int arg2rows, unsigned int arg2cols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
					arguments[0].type.basetype = arg0type;
					arguments[0].type.rows = arg0rows;
					arguments[0].type.cols = arg0cols;
					arguments[1].type.basetype = arg1type;
					arguments[1].type.rows = arg1rows;
					arguments[1].type.cols = arg1cols;
					arguments[2].type.basetype = arg2type;
					arguments[2].type.rows = arg2rows;
					arguments[2].type.cols = arg2cols;

					function.parameter_list.push_back(&arguments[0]);
					function.parameter_list.push_back(&arguments[1]);
					function.parameter_list.push_back(&arguments[2]);
				}
				intrinsic(const std::string &name, enum intrinsic_expression_node::op op, type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols, type_node::datatype arg2type, unsigned int arg2rows, unsigned int arg2cols, type_node::datatype arg3type, unsigned int arg3rows, unsigned int arg3cols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
					arguments[0].type.basetype = arg0type;
					arguments[0].type.rows = arg0rows;
					arguments[0].type.cols = arg0cols;
					arguments[1].type.basetype = arg1type;
					arguments[1].type.rows = arg1rows;
					arguments[1].type.cols = arg1cols;
					arguments[2].type.basetype = arg2type;
					arguments[2].type.rows = arg2rows;
					arguments[2].type.cols = arg2cols;
					arguments[3].type.basetype = arg3type;
					arguments[3].type.rows = arg3rows;
					arguments[3].type.cols = arg3cols;

					function.parameter_list.push_back(&arguments[0]);
					function.parameter_list.push_back(&arguments[1]);
					function.parameter_list.push_back(&arguments[2]);
					function.parameter_list.push_back(&arguments[3]);
				}

				enum intrinsic_expression_node::op op;
				function_declaration_node function;
				variable_declaration_node arguments[4];
			};

			const intrinsic s_intrinsics[] = {
				intrinsic("abs", intrinsic_expression_node::abs, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("abs", intrinsic_expression_node::abs, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("abs", intrinsic_expression_node::abs, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("abs", intrinsic_expression_node::abs, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("acos", intrinsic_expression_node::acos, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("acos", intrinsic_expression_node::acos, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("acos", intrinsic_expression_node::acos, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("acos", intrinsic_expression_node::acos, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("all", intrinsic_expression_node::all, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 1, 1),
				intrinsic("all", intrinsic_expression_node::all, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 2, 1),
				intrinsic("all", intrinsic_expression_node::all, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 3, 1),
				intrinsic("all", intrinsic_expression_node::all, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 4, 1),
				intrinsic("any", intrinsic_expression_node::any, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 1, 1),
				intrinsic("any", intrinsic_expression_node::any, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 2, 1),
				intrinsic("any", intrinsic_expression_node::any, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 3, 1),
				intrinsic("any", intrinsic_expression_node::any, type_node::datatype_bool, 1, 1, type_node::datatype_bool, 4, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_int2float, type_node::datatype_float, 1, 1, type_node::datatype_int, 1, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_int2float, type_node::datatype_float, 2, 1, type_node::datatype_int, 2, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_int2float, type_node::datatype_float, 3, 1, type_node::datatype_int, 3, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_int2float, type_node::datatype_float, 4, 1, type_node::datatype_int, 4, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_uint2float,	type_node::datatype_float, 1, 1, type_node::datatype_uint, 1, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_uint2float,	type_node::datatype_float, 2, 1, type_node::datatype_uint, 2, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_uint2float,	type_node::datatype_float, 3, 1, type_node::datatype_uint, 3, 1),
				intrinsic("asfloat", intrinsic_expression_node::bitcast_uint2float,	type_node::datatype_float, 4, 1, type_node::datatype_uint, 4, 1),
				intrinsic("asin", intrinsic_expression_node::asin, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("asin", intrinsic_expression_node::asin, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("asin", intrinsic_expression_node::asin, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("asin", intrinsic_expression_node::asin, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("asint", intrinsic_expression_node::bitcast_float2int, type_node::datatype_int, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("asint", intrinsic_expression_node::bitcast_float2int, type_node::datatype_int, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("asint", intrinsic_expression_node::bitcast_float2int, type_node::datatype_int, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("asint", intrinsic_expression_node::bitcast_float2int, type_node::datatype_int, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("asuint", intrinsic_expression_node::bitcast_float2uint, type_node::datatype_uint,  1, 1, type_node::datatype_float, 1, 1),
				intrinsic("asuint", intrinsic_expression_node::bitcast_float2uint, type_node::datatype_uint,  2, 1, type_node::datatype_float, 2, 1),
				intrinsic("asuint", intrinsic_expression_node::bitcast_float2uint, type_node::datatype_uint,  3, 1, type_node::datatype_float, 3, 1),
				intrinsic("asuint", intrinsic_expression_node::bitcast_float2uint, type_node::datatype_uint,  4, 1, type_node::datatype_float, 4, 1),
				intrinsic("atan", intrinsic_expression_node::atan, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("atan", intrinsic_expression_node::atan, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("atan", intrinsic_expression_node::atan, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("atan", intrinsic_expression_node::atan, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("atan2", intrinsic_expression_node::atan2, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("atan2", intrinsic_expression_node::atan2, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("atan2", intrinsic_expression_node::atan2, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("atan2", intrinsic_expression_node::atan2, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("ceil", intrinsic_expression_node::ceil, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("ceil", intrinsic_expression_node::ceil, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("ceil", intrinsic_expression_node::ceil, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("ceil", intrinsic_expression_node::ceil, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("clamp", intrinsic_expression_node::clamp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("clamp", intrinsic_expression_node::clamp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("clamp", intrinsic_expression_node::clamp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("clamp", intrinsic_expression_node::clamp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("cos", intrinsic_expression_node::cos, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("cos", intrinsic_expression_node::cos, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("cos", intrinsic_expression_node::cos, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("cos", intrinsic_expression_node::cos, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("cosh", intrinsic_expression_node::cosh, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("cosh", intrinsic_expression_node::cosh, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("cosh", intrinsic_expression_node::cosh, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("cosh", intrinsic_expression_node::cosh, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("cross", intrinsic_expression_node::cross, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("ddx", intrinsic_expression_node::ddx, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("ddx", intrinsic_expression_node::ddx, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("ddx", intrinsic_expression_node::ddx, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("ddx", intrinsic_expression_node::ddx, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("ddy", intrinsic_expression_node::ddy, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("ddy", intrinsic_expression_node::ddy, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("ddy", intrinsic_expression_node::ddy, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("ddy", intrinsic_expression_node::ddy, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("degrees", intrinsic_expression_node::degrees, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("degrees", intrinsic_expression_node::degrees, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("degrees", intrinsic_expression_node::degrees, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("degrees", intrinsic_expression_node::degrees, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("determinant", intrinsic_expression_node::determinant, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 2),
				intrinsic("determinant", intrinsic_expression_node::determinant, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 3),
				intrinsic("determinant", intrinsic_expression_node::determinant, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 4),
				intrinsic("distance", intrinsic_expression_node::distance, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("distance", intrinsic_expression_node::distance, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("distance", intrinsic_expression_node::distance, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("distance", intrinsic_expression_node::distance, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("dot", intrinsic_expression_node::dot, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("dot", intrinsic_expression_node::dot, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("dot", intrinsic_expression_node::dot, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("dot", intrinsic_expression_node::dot, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("exp", intrinsic_expression_node::exp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("exp", intrinsic_expression_node::exp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("exp", intrinsic_expression_node::exp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("exp", intrinsic_expression_node::exp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("exp2", intrinsic_expression_node::exp2, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("exp2", intrinsic_expression_node::exp2, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("exp2", intrinsic_expression_node::exp2, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("exp2", intrinsic_expression_node::exp2, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("faceforward", intrinsic_expression_node::faceforward, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("faceforward", intrinsic_expression_node::faceforward, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("faceforward", intrinsic_expression_node::faceforward, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("faceforward", intrinsic_expression_node::faceforward, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("floor", intrinsic_expression_node::floor, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("floor", intrinsic_expression_node::floor, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("floor", intrinsic_expression_node::floor, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("floor", intrinsic_expression_node::floor, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("frac", intrinsic_expression_node::frac, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("frac", intrinsic_expression_node::frac, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("frac", intrinsic_expression_node::frac, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("frac", intrinsic_expression_node::frac, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("frexp", intrinsic_expression_node::frexp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("frexp", intrinsic_expression_node::frexp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("frexp", intrinsic_expression_node::frexp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("frexp", intrinsic_expression_node::frexp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("fwidth", intrinsic_expression_node::fwidth, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("fwidth", intrinsic_expression_node::fwidth, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("fwidth", intrinsic_expression_node::fwidth, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("fwidth", intrinsic_expression_node::fwidth, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("ldexp", intrinsic_expression_node::ldexp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("ldexp", intrinsic_expression_node::ldexp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("ldexp", intrinsic_expression_node::ldexp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("ldexp", intrinsic_expression_node::ldexp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("length", intrinsic_expression_node::length, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("length", intrinsic_expression_node::length, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 1),
				intrinsic("length", intrinsic_expression_node::length, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 1),
				intrinsic("length", intrinsic_expression_node::length, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 1),
				intrinsic("lerp", intrinsic_expression_node::lerp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("lerp", intrinsic_expression_node::lerp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("lerp", intrinsic_expression_node::lerp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("lerp", intrinsic_expression_node::lerp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("log", intrinsic_expression_node::log, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("log", intrinsic_expression_node::log, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("log", intrinsic_expression_node::log, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("log", intrinsic_expression_node::log, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("log10", intrinsic_expression_node::log10, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("log10", intrinsic_expression_node::log10, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("log10", intrinsic_expression_node::log10, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("log10", intrinsic_expression_node::log10, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("log2", intrinsic_expression_node::log2, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("log2", intrinsic_expression_node::log2, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("log2", intrinsic_expression_node::log2, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("log2", intrinsic_expression_node::log2, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("mad", intrinsic_expression_node::mad, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("mad", intrinsic_expression_node::mad, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("mad", intrinsic_expression_node::mad, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("mad", intrinsic_expression_node::mad, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("max", intrinsic_expression_node::max, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("max", intrinsic_expression_node::max, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("max", intrinsic_expression_node::max, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("max", intrinsic_expression_node::max, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("min", intrinsic_expression_node::min, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("min", intrinsic_expression_node::min, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("min", intrinsic_expression_node::min, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("min", intrinsic_expression_node::min, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("modf", intrinsic_expression_node::modf, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("modf", intrinsic_expression_node::modf, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("modf", intrinsic_expression_node::modf, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("modf", intrinsic_expression_node::modf, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 2, type_node::datatype_float, 1, 1, type_node::datatype_float, 2, 2),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 3, type_node::datatype_float, 1, 1, type_node::datatype_float, 3, 3),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 4, type_node::datatype_float, 1, 1, type_node::datatype_float, 4, 4),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 2, type_node::datatype_float, 2, 2, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 3, type_node::datatype_float, 3, 3, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 4, type_node::datatype_float, 4, 4, type_node::datatype_float, 1, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 2),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 3),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 4),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 2, type_node::datatype_float, 2, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 3, type_node::datatype_float, 3, 1),
				intrinsic("mul", intrinsic_expression_node::mul, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 4, type_node::datatype_float, 4, 1),
				intrinsic("normalize", intrinsic_expression_node::normalize, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("normalize", intrinsic_expression_node::normalize, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("normalize", intrinsic_expression_node::normalize, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("normalize", intrinsic_expression_node::normalize, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("pow", intrinsic_expression_node::pow, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("pow", intrinsic_expression_node::pow, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("pow", intrinsic_expression_node::pow, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("pow", intrinsic_expression_node::pow, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("radians", intrinsic_expression_node::radians, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("radians", intrinsic_expression_node::radians, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("radians", intrinsic_expression_node::radians, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("radians", intrinsic_expression_node::radians, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("rcp", intrinsic_expression_node::rcp, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("rcp", intrinsic_expression_node::rcp, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("rcp", intrinsic_expression_node::rcp, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("rcp", intrinsic_expression_node::rcp, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("reflect", intrinsic_expression_node::reflect, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("reflect", intrinsic_expression_node::reflect, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("reflect", intrinsic_expression_node::reflect, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("reflect", intrinsic_expression_node::reflect, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("refract", intrinsic_expression_node::refract, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("refract", intrinsic_expression_node::refract, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("refract", intrinsic_expression_node::refract, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("refract", intrinsic_expression_node::refract, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("round", intrinsic_expression_node::round, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("round", intrinsic_expression_node::round, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("round", intrinsic_expression_node::round, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("round", intrinsic_expression_node::round, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("rsqrt", intrinsic_expression_node::rsqrt, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("rsqrt", intrinsic_expression_node::rsqrt, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("rsqrt", intrinsic_expression_node::rsqrt, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("rsqrt", intrinsic_expression_node::rsqrt, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("saturate", intrinsic_expression_node::saturate, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("saturate", intrinsic_expression_node::saturate, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("saturate", intrinsic_expression_node::saturate, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("saturate", intrinsic_expression_node::saturate, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("sign", intrinsic_expression_node::sign, type_node::datatype_int, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("sign", intrinsic_expression_node::sign, type_node::datatype_int, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("sign", intrinsic_expression_node::sign, type_node::datatype_int, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("sign", intrinsic_expression_node::sign, type_node::datatype_int, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("sin", intrinsic_expression_node::sin, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("sin", intrinsic_expression_node::sin, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("sin", intrinsic_expression_node::sin, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("sin", intrinsic_expression_node::sin, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("sincos", intrinsic_expression_node::sincos, type_node::datatype_void, 0, 0, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("sincos", intrinsic_expression_node::sincos, type_node::datatype_void, 0, 0, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("sincos", intrinsic_expression_node::sincos, type_node::datatype_void, 0, 0, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("sincos", intrinsic_expression_node::sincos, type_node::datatype_void, 0, 0, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("sinh", intrinsic_expression_node::sinh, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("sinh", intrinsic_expression_node::sinh, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("sinh", intrinsic_expression_node::sinh, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("sinh", intrinsic_expression_node::sinh, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("smoothstep", intrinsic_expression_node::smoothstep, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("smoothstep", intrinsic_expression_node::smoothstep, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("smoothstep", intrinsic_expression_node::smoothstep, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("smoothstep", intrinsic_expression_node::smoothstep, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("sqrt", intrinsic_expression_node::sqrt, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("sqrt", intrinsic_expression_node::sqrt, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("sqrt", intrinsic_expression_node::sqrt, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("sqrt", intrinsic_expression_node::sqrt, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("step", intrinsic_expression_node::step, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("step", intrinsic_expression_node::step, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("step", intrinsic_expression_node::step, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("step", intrinsic_expression_node::step, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("tan", intrinsic_expression_node::tan, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("tan", intrinsic_expression_node::tan, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("tan", intrinsic_expression_node::tan, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("tan", intrinsic_expression_node::tan, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("tanh", intrinsic_expression_node::tanh, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("tanh", intrinsic_expression_node::tanh, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("tanh", intrinsic_expression_node::tanh, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("tanh", intrinsic_expression_node::tanh, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
				intrinsic("tex2D", intrinsic_expression_node::tex2d, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 2, 1),
				intrinsic("tex2Dfetch", intrinsic_expression_node::tex2dfetch, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_int, 4, 1),
				intrinsic("tex2Dgather", intrinsic_expression_node::tex2dgather, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 2, 1, type_node::datatype_int, 1, 1),
				intrinsic("tex2Dgatheroffset", intrinsic_expression_node::tex2dgatheroffset, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 2, 1, type_node::datatype_int, 2, 1, type_node::datatype_int, 1, 1),
				intrinsic("tex2Dgrad", intrinsic_expression_node::tex2dgrad, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("tex2Dlod", intrinsic_expression_node::tex2dlevel, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 4, 1),
				intrinsic("tex2Dlodoffset", intrinsic_expression_node::tex2dleveloffset, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 4, 1, type_node::datatype_int, 2, 1),
				intrinsic("tex2Doffset", intrinsic_expression_node::tex2doffset, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 2, 1, type_node::datatype_int,  2, 1),
				intrinsic("tex2Dproj", intrinsic_expression_node::tex2dproj, type_node::datatype_float, 4, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_float, 4, 1),
				intrinsic("tex2Dsize", intrinsic_expression_node::tex2dsize, type_node::datatype_int, 2, 1, type_node::datatype_sampler, 0, 0, type_node::datatype_int, 1, 1),
				intrinsic("transpose", intrinsic_expression_node::transpose, type_node::datatype_float, 2, 2, type_node::datatype_float, 2, 2),
				intrinsic("transpose", intrinsic_expression_node::transpose, type_node::datatype_float, 3, 3, type_node::datatype_float, 3, 3),
				intrinsic("transpose", intrinsic_expression_node::transpose, type_node::datatype_float, 4, 4, type_node::datatype_float, 4, 4),
				intrinsic("trunc", intrinsic_expression_node::trunc, type_node::datatype_float, 1, 1, type_node::datatype_float, 1, 1),
				intrinsic("trunc", intrinsic_expression_node::trunc, type_node::datatype_float, 2, 1, type_node::datatype_float, 2, 1),
				intrinsic("trunc", intrinsic_expression_node::trunc, type_node::datatype_float, 3, 1, type_node::datatype_float, 3, 1),
				intrinsic("trunc", intrinsic_expression_node::trunc, type_node::datatype_float, 4, 1, type_node::datatype_float, 4, 1),
			};
		}

		unsigned int nodes::type_node::rank(const type_node &src, const type_node &dst)
		{
			if (src.is_array() != dst.is_array() || (src.array_length != dst.array_length && src.array_length > 0 && dst.array_length > 0))
			{
				return 0;
			}
			if (src.is_struct() || dst.is_struct())
			{
				return src.definition == dst.definition;
			}
			if (src.basetype == dst.basetype && src.rows == dst.rows && src.cols == dst.cols)
			{
				return 1;
			}
			if (!src.is_numeric() || !dst.is_numeric())
			{
				return 0;
			}

			const int ranks[4][4] =
			{
				{ 0, 5, 5, 5 },
				{ 4, 0, 3, 5 },
				{ 4, 2, 0, 5 },
				{ 4, 4, 4, 0 }
			};

			assert(src.basetype <= 4 && dst.basetype <= 4);

			const int rank = ranks[static_cast<unsigned int>(src.basetype) - 1][static_cast<unsigned int>(dst.basetype) - 1] << 2;

			if (src.is_scalar() && dst.is_vector())
			{
				return rank | 2;
			}
			if (src.is_vector() && dst.is_scalar() || (src.is_vector() == dst.is_vector() && src.rows > dst.rows && src.cols >= dst.cols))
			{
				return rank | 32;
			}
			if (src.is_vector() != dst.is_vector() || src.is_matrix() != dst.is_matrix() || src.rows * src.cols != dst.rows * dst.cols)
			{
				return 0;
			}

			return rank;
		}
		bool get_call_ranks(const call_expression_node *call, const function_declaration_node *function, unsigned int ranks[])
		{
			for (size_t i = 0, count = call->arguments.size(); i < count; ++i)
			{
				ranks[i] = type_node::rank(call->arguments[i]->type, function->parameter_list[i]->type);

				if (ranks[i] == 0)
				{
					return false;
				}
			}

			return true;
		}
		int compare_functions(const call_expression_node *call, const function_declaration_node *function1, const function_declaration_node *function2)
		{
			if (function2 == nullptr)
			{
				return -1;
			}

			const size_t count = call->arguments.size();

			const auto function1_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
			const bool function1_viable = get_call_ranks(call, function1, function1_ranks);
			const auto function2_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
			const bool function2_viable = get_call_ranks(call, function2, function2_ranks);

			if (!(function1_viable && function2_viable))
			{
				return function2_viable - function1_viable;
			}

			std::sort(function1_ranks, function1_ranks + count, std::greater<unsigned int>());
			std::sort(function2_ranks, function2_ranks + count, std::greater<unsigned int>());

			for (size_t i = 0; i < count; ++i)
			{
				if (function1_ranks[i] < function2_ranks[i])
				{
					return -1;
				}
				else if (function2_ranks[i] < function1_ranks[i])
				{
					return +1;
				}
			}

			return 0;
		}

		symbol_table::symbol_table()
		{
			_current_scope.name = "::";
			_current_scope.level = 0;
			_current_scope.namespace_level = 0;
		}

		void symbol_table::enter_scope(symbol parent)
		{
			if (parent != nullptr || _parent_stack.empty())
			{
				_parent_stack.push(parent);
			}
			else
			{
				_parent_stack.push(_parent_stack.top());
			}

			_current_scope.level++;
		}
		void symbol_table::enter_namespace(const std::string &name)
		{
			_current_scope.name += name + "::";
			_current_scope.level++;
			_current_scope.namespace_level++;
		}
		void symbol_table::leave_scope()
		{
			assert(_current_scope.level > 0);

			for (auto &symbol : _symbol_stack)
			{
				auto &scope_list = symbol.second;

				for (auto scope_it = scope_list.begin(); scope_it != scope_list.end();)
				{
					if (scope_it->first.level > scope_it->first.namespace_level &&
					    scope_it->first.level >= _current_scope.level)
					{
						scope_it = scope_list.erase(scope_it);
					}
					else
					{
						++scope_it;
					}
				}
			}

			_parent_stack.pop();

			_current_scope.level--;
		}
		void symbol_table::leave_namespace()
		{
			assert(_current_scope.level > 0);
			assert(_current_scope.namespace_level > 0);

			_current_scope.name.erase(_current_scope.name.substr(0, _current_scope.name.size() - 2).rfind("::") + 2);
			_current_scope.level--;
			_current_scope.namespace_level--;
		}

		bool symbol_table::insert(symbol symbol, bool global)
		{
			if (symbol->id != nodeid::function_declaration && find(symbol->name, _current_scope, true))
			{
				return false;
			}

			if (global)
			{
				scope scope = { "", 0, 0 };

				for (size_t pos = 0; pos != std::string::npos; pos = _current_scope.name.find("::", pos))
				{
					pos += 2;

					scope.name = _current_scope.name.substr(0, pos);

					_symbol_stack[_current_scope.name.substr(pos) + symbol->name].emplace_back(scope, symbol);

					scope.level = ++scope.namespace_level;
				}
			}
			else
			{
				_symbol_stack[symbol->name].emplace_back(_current_scope, symbol);
			}

			return true;
		}
		symbol symbol_table::find(const std::string &name) const
		{
			return find(name, _current_scope, false);
		}
		symbol symbol_table::find(const std::string &name, const scope &scope, bool exclusive) const
		{
			const auto it = _symbol_stack.find(name);

			if (it == _symbol_stack.end() || it->second.empty())
			{
				return nullptr;
			}

			symbol result = nullptr;
			const auto &scope_list = it->second;

			for (auto scope_it = scope_list.rbegin(), end = scope_list.rend(); scope_it != end; ++scope_it)
			{
				if (scope_it->first.level > scope.level ||
				    scope_it->first.namespace_level > scope.namespace_level ||
				   (scope_it->first.namespace_level == scope.namespace_level && scope_it->first.name != scope.name))
				{
					continue;
				}
				if (exclusive && scope_it->first.level < scope.level)
				{
					continue;
				}

				if (scope_it->second->id == nodeid::variable_declaration || scope_it->second->id == nodeid::struct_declaration)
				{
					return scope_it->second;
				}
				if (result == nullptr)
				{
					result = scope_it->second;
				}
			}

			return result;
		}
		bool symbol_table::resolve_call(call_expression_node *call, const scope &scope, bool &is_intrinsic, bool &is_ambiguous) const
		{
			is_intrinsic = false;
			is_ambiguous = false;

			unsigned int overload_count = 0, overload_namespace = scope.namespace_level;
			const function_declaration_node *overload = nullptr;
			auto intrinsic_op = intrinsic_expression_node::none;

			const auto it = _symbol_stack.find(call->callee_name);

			if (it != _symbol_stack.end() && !it->second.empty())
			{
				const auto &scope_list = it->second;

				for (auto scope_it = scope_list.rbegin(), end = scope_list.rend(); scope_it != end; ++scope_it)
				{
					if (scope_it->first.level > scope.level ||
					    scope_it->first.namespace_level > scope.namespace_level ||
					    scope_it->second->id != nodeid::function_declaration)
					{
						continue;
					}

					const auto function = static_cast<function_declaration_node *>(scope_it->second);

					if (function->parameter_list.empty())
					{
						if (call->arguments.empty())
						{
							overload = function;
							overload_count = 1;
							break;
						}
						else
						{
							continue;
						}
					}
					else if (call->arguments.size() != function->parameter_list.size())
					{
						continue;
					}

					const int comparison = compare_functions(call, function, overload);

					if (comparison < 0)
					{
						overload = function;
						overload_count = 1;
						overload_namespace = scope_it->first.namespace_level;
					}
					else if (comparison == 0 && overload_namespace == scope_it->first.namespace_level)
					{
						++overload_count;
					}
				}
			}

			if (overload_count == 0)
			{
				for (auto &intrinsic : s_intrinsics)
				{
					if (intrinsic.function.name != call->callee_name)
					{
						continue;
					}
					if (intrinsic.function.parameter_list.size() != call->arguments.size())
					{
						is_intrinsic = overload_count == 0;
						break;
					}

					const int comparison = compare_functions(call, &intrinsic.function, overload);

					if (comparison < 0)
					{
						overload = &intrinsic.function;
						overload_count = 1;

						is_intrinsic = true;
						intrinsic_op = intrinsic.op;
					}
					else if (comparison == 0 && overload_namespace == 0)
					{
						++overload_count;
					}
				}
			}

			if (overload_count == 1)
			{
				call->type = overload->return_type;

				if (is_intrinsic)
				{
					assert(intrinsic_op < 0xFF);

					call->callee = nullptr;
					call->callee_name = static_cast<char>(intrinsic_op);
				}
				else
				{
					call->callee = overload;
					call->callee_name = overload->name;
				}

				return true;
			}
			else
			{
				is_ambiguous = overload_count > 1;

				return false;
			}
		}
	}
}
