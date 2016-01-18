#include "Parser.hpp"

#include <cstdarg>
#include <algorithm>
#include <functional>
#include <stack>
#include <unordered_map>
#include <boost\assign\list_of.hpp>
#include <boost\algorithm\string.hpp>

namespace reshade
{
	namespace fx
	{
		namespace
		{
			#pragma region Intrinsics
			struct intrinsic
			{
				explicit intrinsic(const std::string &name, enum nodes::intrinsic_expression_node::op op, nodes::type_node::datatype returntype, unsigned int returnrows, unsigned int returncols) : op(op)
				{
					function.name = name;
					function.return_type.basetype = returntype;
					function.return_type.rows = returnrows;
					function.return_type.cols = returncols;
				}
				explicit intrinsic(const std::string &name, enum nodes::intrinsic_expression_node::op op, nodes::type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, nodes::type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols) : op(op)
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
				explicit intrinsic(const std::string &name, enum nodes::intrinsic_expression_node::op op, nodes::type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, nodes::type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, nodes::type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols) : op(op)
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
				explicit intrinsic(const std::string &name, enum nodes::intrinsic_expression_node::op op, nodes::type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, nodes::type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, nodes::type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols, nodes::type_node::datatype arg2type, unsigned int arg2rows, unsigned int arg2cols) : op(op)
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
				explicit intrinsic(const std::string &name, enum nodes::intrinsic_expression_node::op op, nodes::type_node::datatype returntype, unsigned int returnrows, unsigned int returncols, nodes::type_node::datatype arg0type, unsigned int arg0rows, unsigned int arg0cols, nodes::type_node::datatype arg1type, unsigned int arg1rows, unsigned int arg1cols, nodes::type_node::datatype arg2type, unsigned int arg2rows, unsigned int arg2cols, nodes::type_node::datatype arg3type, unsigned int arg3rows, unsigned int arg3cols) : op(op)
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

				enum nodes::intrinsic_expression_node::op op;
				nodes::function_declaration_node function;
				nodes::variable_declaration_node arguments[4];
			};

			const intrinsic _intrinsics[] =
			{
				intrinsic("abs", 				nodes::intrinsic_expression_node::abs, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("abs", 				nodes::intrinsic_expression_node::abs, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("abs", 				nodes::intrinsic_expression_node::abs, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("abs", 				nodes::intrinsic_expression_node::abs, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("acos", 				nodes::intrinsic_expression_node::acos, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("acos", 				nodes::intrinsic_expression_node::acos, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("acos", 				nodes::intrinsic_expression_node::acos, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("acos", 				nodes::intrinsic_expression_node::acos, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("all", 				nodes::intrinsic_expression_node::all, 					nodes::type_node::datatype_bool,  1, 1, nodes::type_node::datatype_bool,  1, 1),
				intrinsic("all", 				nodes::intrinsic_expression_node::all, 					nodes::type_node::datatype_bool,  2, 1, nodes::type_node::datatype_bool,  2, 1),
				intrinsic("all", 				nodes::intrinsic_expression_node::all, 					nodes::type_node::datatype_bool,  3, 1, nodes::type_node::datatype_bool,  3, 1),
				intrinsic("all", 				nodes::intrinsic_expression_node::all, 					nodes::type_node::datatype_bool,  4, 1, nodes::type_node::datatype_bool,  4, 1),
				intrinsic("any", 				nodes::intrinsic_expression_node::any, 					nodes::type_node::datatype_bool,  1, 1, nodes::type_node::datatype_bool,  1, 1),
				intrinsic("any", 				nodes::intrinsic_expression_node::any, 					nodes::type_node::datatype_bool,  2, 1, nodes::type_node::datatype_bool,  2, 1),
				intrinsic("any", 				nodes::intrinsic_expression_node::any, 					nodes::type_node::datatype_bool,  3, 1, nodes::type_node::datatype_bool,  3, 1),
				intrinsic("any", 				nodes::intrinsic_expression_node::any, 					nodes::type_node::datatype_bool,  4, 1, nodes::type_node::datatype_bool,  4, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_int2float,	nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_int,   1, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_int2float,	nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_int,   2, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_int2float,	nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_int,   3, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_int2float,	nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_int,   4, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_uint2float,	nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_uint,  1, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_uint2float,	nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_uint,  2, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_uint2float,	nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_uint,  3, 1),
				intrinsic("asfloat", 			nodes::intrinsic_expression_node::bitcast_uint2float,	nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_uint,  4, 1),
				intrinsic("asin", 				nodes::intrinsic_expression_node::asin, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("asin", 				nodes::intrinsic_expression_node::asin, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("asin", 				nodes::intrinsic_expression_node::asin, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("asin", 				nodes::intrinsic_expression_node::asin, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("asint", 				nodes::intrinsic_expression_node::bitcast_float2int,	nodes::type_node::datatype_int,   1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("asint", 				nodes::intrinsic_expression_node::bitcast_float2int,	nodes::type_node::datatype_int,   2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("asint", 				nodes::intrinsic_expression_node::bitcast_float2int,	nodes::type_node::datatype_int,   3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("asint", 				nodes::intrinsic_expression_node::bitcast_float2int,	nodes::type_node::datatype_int,   4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("asuint", 			nodes::intrinsic_expression_node::bitcast_float2uint,	nodes::type_node::datatype_uint,  1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("asuint", 			nodes::intrinsic_expression_node::bitcast_float2uint,	nodes::type_node::datatype_uint,  2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("asuint", 			nodes::intrinsic_expression_node::bitcast_float2uint,	nodes::type_node::datatype_uint,  3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("asuint", 			nodes::intrinsic_expression_node::bitcast_float2uint,	nodes::type_node::datatype_uint,  4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("atan", 				nodes::intrinsic_expression_node::atan, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("atan", 				nodes::intrinsic_expression_node::atan, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("atan", 				nodes::intrinsic_expression_node::atan, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("atan", 				nodes::intrinsic_expression_node::atan, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("atan2", 				nodes::intrinsic_expression_node::atan2, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("atan2", 				nodes::intrinsic_expression_node::atan2, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("atan2", 				nodes::intrinsic_expression_node::atan2, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("atan2", 				nodes::intrinsic_expression_node::atan2, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("ceil", 				nodes::intrinsic_expression_node::ceil, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("ceil", 				nodes::intrinsic_expression_node::ceil, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("ceil", 				nodes::intrinsic_expression_node::ceil, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("ceil", 				nodes::intrinsic_expression_node::ceil, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("clamp", 				nodes::intrinsic_expression_node::clamp, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("clamp", 				nodes::intrinsic_expression_node::clamp, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("clamp", 				nodes::intrinsic_expression_node::clamp, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("clamp", 				nodes::intrinsic_expression_node::clamp, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("cos", 				nodes::intrinsic_expression_node::cos, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("cos", 				nodes::intrinsic_expression_node::cos, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("cos", 				nodes::intrinsic_expression_node::cos, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("cos", 				nodes::intrinsic_expression_node::cos, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("cosh", 				nodes::intrinsic_expression_node::cosh, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("cosh", 				nodes::intrinsic_expression_node::cosh, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("cosh", 				nodes::intrinsic_expression_node::cosh, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("cosh", 				nodes::intrinsic_expression_node::cosh, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("cross", 				nodes::intrinsic_expression_node::cross, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("ddx", 				nodes::intrinsic_expression_node::ddx,					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("ddx", 				nodes::intrinsic_expression_node::ddx,					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("ddx", 				nodes::intrinsic_expression_node::ddx,					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("ddx", 				nodes::intrinsic_expression_node::ddx,					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("ddy", 				nodes::intrinsic_expression_node::ddy,					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("ddy", 				nodes::intrinsic_expression_node::ddy,					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("ddy", 				nodes::intrinsic_expression_node::ddy,					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("ddy", 				nodes::intrinsic_expression_node::ddy,					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("degrees", 			nodes::intrinsic_expression_node::degrees, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("degrees", 			nodes::intrinsic_expression_node::degrees, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("degrees", 			nodes::intrinsic_expression_node::degrees, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("degrees", 			nodes::intrinsic_expression_node::degrees, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("determinant",		nodes::intrinsic_expression_node::determinant,			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 2),
				intrinsic("determinant",		nodes::intrinsic_expression_node::determinant,			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 3),
				intrinsic("determinant", 		nodes::intrinsic_expression_node::determinant,			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 4),
				intrinsic("distance", 			nodes::intrinsic_expression_node::distance,				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("distance", 			nodes::intrinsic_expression_node::distance,				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("distance", 			nodes::intrinsic_expression_node::distance, 			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("distance", 			nodes::intrinsic_expression_node::distance, 			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("dot", 				nodes::intrinsic_expression_node::dot, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("dot", 				nodes::intrinsic_expression_node::dot, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("dot", 				nodes::intrinsic_expression_node::dot, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("dot", 				nodes::intrinsic_expression_node::dot, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("exp", 				nodes::intrinsic_expression_node::exp, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("exp", 				nodes::intrinsic_expression_node::exp, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("exp", 				nodes::intrinsic_expression_node::exp, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("exp", 				nodes::intrinsic_expression_node::exp, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("exp2", 				nodes::intrinsic_expression_node::exp2, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("exp2", 				nodes::intrinsic_expression_node::exp2, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("exp2", 				nodes::intrinsic_expression_node::exp2, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("exp2", 				nodes::intrinsic_expression_node::exp2, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("faceforward",		nodes::intrinsic_expression_node::faceforward,			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("faceforward",		nodes::intrinsic_expression_node::faceforward,			nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("faceforward",		nodes::intrinsic_expression_node::faceforward,			nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("faceforward",		nodes::intrinsic_expression_node::faceforward, 			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("floor", 				nodes::intrinsic_expression_node::floor, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("floor", 				nodes::intrinsic_expression_node::floor, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("floor", 				nodes::intrinsic_expression_node::floor, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("floor", 				nodes::intrinsic_expression_node::floor, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("frac", 				nodes::intrinsic_expression_node::frac, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("frac", 				nodes::intrinsic_expression_node::frac, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("frac", 				nodes::intrinsic_expression_node::frac, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("frac", 				nodes::intrinsic_expression_node::frac, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("frexp", 				nodes::intrinsic_expression_node::frexp, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("frexp", 				nodes::intrinsic_expression_node::frexp, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("frexp", 				nodes::intrinsic_expression_node::frexp, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("frexp", 				nodes::intrinsic_expression_node::frexp, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("fwidth", 			nodes::intrinsic_expression_node::fwidth, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("fwidth", 			nodes::intrinsic_expression_node::fwidth, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("fwidth", 			nodes::intrinsic_expression_node::fwidth, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("fwidth", 			nodes::intrinsic_expression_node::fwidth, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("ldexp", 				nodes::intrinsic_expression_node::ldexp, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("ldexp", 				nodes::intrinsic_expression_node::ldexp, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("ldexp", 				nodes::intrinsic_expression_node::ldexp, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("ldexp", 				nodes::intrinsic_expression_node::ldexp, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("length", 			nodes::intrinsic_expression_node::length, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("length", 			nodes::intrinsic_expression_node::length, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("length", 			nodes::intrinsic_expression_node::length, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("length", 			nodes::intrinsic_expression_node::length, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("lerp",				nodes::intrinsic_expression_node::lerp, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("lerp",				nodes::intrinsic_expression_node::lerp, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("lerp",				nodes::intrinsic_expression_node::lerp, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("lerp",				nodes::intrinsic_expression_node::lerp, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("log", 				nodes::intrinsic_expression_node::log, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("log", 				nodes::intrinsic_expression_node::log, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("log", 				nodes::intrinsic_expression_node::log, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("log", 				nodes::intrinsic_expression_node::log, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("log10", 				nodes::intrinsic_expression_node::log10, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("log10", 				nodes::intrinsic_expression_node::log10, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("log10", 				nodes::intrinsic_expression_node::log10, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("log10", 				nodes::intrinsic_expression_node::log10, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("log2", 				nodes::intrinsic_expression_node::log2, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("log2", 				nodes::intrinsic_expression_node::log2, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("log2", 				nodes::intrinsic_expression_node::log2, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("log2", 				nodes::intrinsic_expression_node::log2, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("mad", 				nodes::intrinsic_expression_node::mad, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mad", 				nodes::intrinsic_expression_node::mad, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("mad", 				nodes::intrinsic_expression_node::mad, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("mad", 				nodes::intrinsic_expression_node::mad, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("max", 				nodes::intrinsic_expression_node::max, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("max", 				nodes::intrinsic_expression_node::max, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("max",				nodes::intrinsic_expression_node::max, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("max", 				nodes::intrinsic_expression_node::max, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("min", 				nodes::intrinsic_expression_node::min, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("min", 				nodes::intrinsic_expression_node::min, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("min", 				nodes::intrinsic_expression_node::min, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("min", 				nodes::intrinsic_expression_node::min, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("modf", 				nodes::intrinsic_expression_node::modf, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("modf", 				nodes::intrinsic_expression_node::modf, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("modf", 				nodes::intrinsic_expression_node::modf, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("modf", 				nodes::intrinsic_expression_node::modf, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 2, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 2, 2),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 3, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 3, 3),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 4, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 4, 4),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 2, nodes::type_node::datatype_float, 2, 2, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 3, nodes::type_node::datatype_float, 3, 3, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 4, nodes::type_node::datatype_float, 4, 4, nodes::type_node::datatype_float, 1, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 2),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 3),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 4),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 2, nodes::type_node::datatype_float, 2, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 3, nodes::type_node::datatype_float, 3, 1),
				intrinsic("mul", 				nodes::intrinsic_expression_node::mul, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 4, nodes::type_node::datatype_float, 4, 1),
				intrinsic("normalize", 			nodes::intrinsic_expression_node::normalize, 			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("normalize", 			nodes::intrinsic_expression_node::normalize, 			nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("normalize", 			nodes::intrinsic_expression_node::normalize, 			nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("normalize", 			nodes::intrinsic_expression_node::normalize, 			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("pow", 				nodes::intrinsic_expression_node::pow, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("pow", 				nodes::intrinsic_expression_node::pow, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("pow", 				nodes::intrinsic_expression_node::pow, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("pow", 				nodes::intrinsic_expression_node::pow, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("radians", 			nodes::intrinsic_expression_node::radians, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("radians", 			nodes::intrinsic_expression_node::radians, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("radians", 			nodes::intrinsic_expression_node::radians, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("radians", 			nodes::intrinsic_expression_node::radians, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("rcp", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("rcp", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("rcp", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("rcp", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("reflect",			nodes::intrinsic_expression_node::reflect, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("reflect",			nodes::intrinsic_expression_node::reflect, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("reflect",			nodes::intrinsic_expression_node::reflect, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("reflect",			nodes::intrinsic_expression_node::reflect, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("refract",			nodes::intrinsic_expression_node::refract, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("refract",			nodes::intrinsic_expression_node::refract, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("refract",			nodes::intrinsic_expression_node::refract, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("refract",			nodes::intrinsic_expression_node::refract, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("round", 				nodes::intrinsic_expression_node::round, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("round", 				nodes::intrinsic_expression_node::round, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("round", 				nodes::intrinsic_expression_node::round, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("round", 				nodes::intrinsic_expression_node::round, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("rsqrt", 				nodes::intrinsic_expression_node::rsqrt, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("rsqrt", 				nodes::intrinsic_expression_node::rsqrt, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("rsqrt", 				nodes::intrinsic_expression_node::rsqrt, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("rsqrt", 				nodes::intrinsic_expression_node::rsqrt, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("saturate", 			nodes::intrinsic_expression_node::saturate,				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("saturate", 			nodes::intrinsic_expression_node::saturate,				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("saturate", 			nodes::intrinsic_expression_node::saturate,				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("saturate", 			nodes::intrinsic_expression_node::saturate, 			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("sign", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_int,   1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("sign", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_int,   2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("sign", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_int,   3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("sign", 				nodes::intrinsic_expression_node::sign, 				nodes::type_node::datatype_int,   4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("sin", 				nodes::intrinsic_expression_node::sin, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("sin", 				nodes::intrinsic_expression_node::sin, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("sin", 				nodes::intrinsic_expression_node::sin, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("sin", 				nodes::intrinsic_expression_node::sin, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("sincos",				nodes::intrinsic_expression_node::sincos, 				nodes::type_node::datatype_void,  0, 0, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("sincos",				nodes::intrinsic_expression_node::sincos, 				nodes::type_node::datatype_void,  0, 0, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("sincos",				nodes::intrinsic_expression_node::sincos, 				nodes::type_node::datatype_void,  0, 0, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("sincos",				nodes::intrinsic_expression_node::sincos, 				nodes::type_node::datatype_void,  0, 0, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("sinh", 				nodes::intrinsic_expression_node::sinh, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("sinh", 				nodes::intrinsic_expression_node::sinh, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("sinh", 				nodes::intrinsic_expression_node::sinh, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("sinh", 				nodes::intrinsic_expression_node::sinh, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("smoothstep",			nodes::intrinsic_expression_node::smoothstep, 			nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("smoothstep",			nodes::intrinsic_expression_node::smoothstep, 			nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("smoothstep",			nodes::intrinsic_expression_node::smoothstep, 			nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("smoothstep",			nodes::intrinsic_expression_node::smoothstep, 			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("sqrt", 				nodes::intrinsic_expression_node::sqrt, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("sqrt", 				nodes::intrinsic_expression_node::sqrt, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("sqrt", 				nodes::intrinsic_expression_node::sqrt, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("sqrt", 				nodes::intrinsic_expression_node::sqrt, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("step", 				nodes::intrinsic_expression_node::step, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("step", 				nodes::intrinsic_expression_node::step, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("step",				nodes::intrinsic_expression_node::step, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("step",				nodes::intrinsic_expression_node::step, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("tan", 				nodes::intrinsic_expression_node::tan, 					nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("tan", 				nodes::intrinsic_expression_node::tan, 					nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("tan", 				nodes::intrinsic_expression_node::tan, 					nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("tan", 				nodes::intrinsic_expression_node::tan, 					nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("tanh", 				nodes::intrinsic_expression_node::tanh, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("tanh", 				nodes::intrinsic_expression_node::tanh, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("tanh", 				nodes::intrinsic_expression_node::tanh, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("tanh", 				nodes::intrinsic_expression_node::tanh, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
				intrinsic("tex2D",				nodes::intrinsic_expression_node::tex2d,				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 2, 1),
				intrinsic("tex2Dfetch",			nodes::intrinsic_expression_node::tex2dfetch,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_int,   2, 1),
				intrinsic("tex2Dgather",		nodes::intrinsic_expression_node::tex2dgather,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_int,   1, 1),
				intrinsic("tex2Dgatheroffset",	nodes::intrinsic_expression_node::tex2dgatheroffset,	nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_int,   2, 1, nodes::type_node::datatype_int,   1, 1),
				intrinsic("tex2Dgrad",			nodes::intrinsic_expression_node::tex2dgrad,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("tex2Dlod",			nodes::intrinsic_expression_node::tex2dlevel,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 4, 1),
				intrinsic("tex2Dlodoffset",		nodes::intrinsic_expression_node::tex2dleveloffset,		nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_int,   2, 1),
				intrinsic("tex2Doffset",		nodes::intrinsic_expression_node::tex2doffset,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_int,   2, 1),
				intrinsic("tex2Dproj",			nodes::intrinsic_expression_node::tex2dproj,			nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_float, 4, 1),
				intrinsic("tex2Dsize",			nodes::intrinsic_expression_node::tex2dsize,			nodes::type_node::datatype_int,   2, 1, nodes::type_node::datatype_sampler, 0, 0, nodes::type_node::datatype_int,   1, 1),
				intrinsic("transpose", 			nodes::intrinsic_expression_node::transpose, 			nodes::type_node::datatype_float, 2, 2, nodes::type_node::datatype_float, 2, 2),
				intrinsic("transpose", 			nodes::intrinsic_expression_node::transpose, 			nodes::type_node::datatype_float, 3, 3, nodes::type_node::datatype_float, 3, 3),
				intrinsic("transpose",			nodes::intrinsic_expression_node::transpose, 			nodes::type_node::datatype_float, 4, 4, nodes::type_node::datatype_float, 4, 4),
				intrinsic("trunc", 				nodes::intrinsic_expression_node::trunc, 				nodes::type_node::datatype_float, 1, 1, nodes::type_node::datatype_float, 1, 1),
				intrinsic("trunc", 				nodes::intrinsic_expression_node::trunc, 				nodes::type_node::datatype_float, 2, 1, nodes::type_node::datatype_float, 2, 1),
				intrinsic("trunc", 				nodes::intrinsic_expression_node::trunc, 				nodes::type_node::datatype_float, 3, 1, nodes::type_node::datatype_float, 3, 1),
				intrinsic("trunc",				nodes::intrinsic_expression_node::trunc, 				nodes::type_node::datatype_float, 4, 1, nodes::type_node::datatype_float, 4, 1),
			};
			#pragma endregion

			void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, int &to)
			{
				switch (from->type.basetype)
				{
					case nodes::type_node::datatype_bool:
					case nodes::type_node::datatype_int:
					case nodes::type_node::datatype_uint:
						to = from->value_int[i];
						break;
					case nodes::type_node::datatype_float:
						to = static_cast<int>(from->value_float[i]);
						break;
					default:
						to = 0;
						break;
				}
			}
			void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, unsigned int &to)
			{
				switch (from->type.basetype)
				{
					case nodes::type_node::datatype_bool:
					case nodes::type_node::datatype_int:
					case nodes::type_node::datatype_uint:
						to = from->value_uint[i];
						break;
					case nodes::type_node::datatype_float:
						to = static_cast<unsigned int>(from->value_float[i]);
						break;
					default:
						to = 0;
						break;
				}
			}
			void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, float &to)
			{
				switch (from->type.basetype)
				{
					case nodes::type_node::datatype_bool:
					case nodes::type_node::datatype_int:
						to = static_cast<float>(from->value_int[i]);
						break;
					case nodes::type_node::datatype_uint:
						to = static_cast<float>(from->value_uint[i]);
						break;
					case nodes::type_node::datatype_float:
						to = from->value_float[i];
						break;
					default:
						to = 0;
						break;
				}
			}
			void vector_literal_cast(const nodes::literal_expression_node *from, size_t k, nodes::literal_expression_node *to, size_t j)
			{
				switch (to->type.basetype)
				{
					case nodes::type_node::datatype_bool:
					case nodes::type_node::datatype_int:
						scalar_literal_cast(from, j, to->value_int[k]);
						break;
					case nodes::type_node::datatype_uint:
						scalar_literal_cast(from, j, to->value_uint[k]);
						break;
					case nodes::type_node::datatype_float:
						scalar_literal_cast(from, j, to->value_float[k]);
						break;
					default:
						memcpy(to->value_uint, from->value_uint, sizeof(from->value_uint));
						break;
				}
			}

			unsigned int get_type_rank(const nodes::type_node &src, const nodes::type_node &dst)
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
			bool get_call_ranks(const nodes::call_expression_node *call, const nodes::function_declaration_node *function, unsigned int ranks[])
			{
				for (size_t i = 0, count = call->arguments.size(); i < count; ++i)
				{
					ranks[i] = get_type_rank(call->arguments[i]->type, function->parameter_list[i]->type);

					if (ranks[i] == 0)
					{
						return false;
					}
				}

				return true;
			}
			int compare_functions(const nodes::call_expression_node *call, const nodes::function_declaration_node *function1, const nodes::function_declaration_node *function2)
			{
				if (function2 == nullptr)
				{
					return -1;
				}

				const size_t count = call->arguments.size();

				unsigned int *const function1_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				unsigned int *const function2_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				const bool function1Viable = get_call_ranks(call, function1, function1_ranks);
				const bool function2Viable = get_call_ranks(call, function2, function2_ranks);

				if (!(function1Viable && function2Viable))
				{
					return function2Viable - function1Viable;
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

			const char *get_token_name(lexer::tokenid tokid)
			{
				switch (tokid)
				{
					default:
					case lexer::tokenid::unknown:
						return "unknown";
					case lexer::tokenid::end_of_file:
						return "end of file";
					case lexer::tokenid::exclaim:
						return "!";
					case lexer::tokenid::hash:
						return "#";
					case lexer::tokenid::dollar:
						return "$";
					case lexer::tokenid::percent:
						return "%";
					case lexer::tokenid::ampersand:
						return "&";
					case lexer::tokenid::parenthesis_open:
						return "(";
					case lexer::tokenid::parenthesis_close:
						return ")";
					case lexer::tokenid::star:
						return "*";
					case lexer::tokenid::plus:
						return "+";
					case lexer::tokenid::comma:
						return ",";
					case lexer::tokenid::minus:
						return "-";
					case lexer::tokenid::dot:
						return ".";
					case lexer::tokenid::slash:
						return "/";
					case lexer::tokenid::colon:
						return ":";
					case lexer::tokenid::semicolon:
						return ";";
					case lexer::tokenid::less:
						return "<";
					case lexer::tokenid::equal:
						return "=";
					case lexer::tokenid::greater:
						return ">";
					case lexer::tokenid::question:
						return "?";
					case lexer::tokenid::at:
						return "@";
					case lexer::tokenid::bracket_open:
						return "[";
					case lexer::tokenid::backslash:
						return "\\";
					case lexer::tokenid::bracket_close:
						return "]";
					case lexer::tokenid::caret:
						return "^";
					case lexer::tokenid::brace_open:
						return "{";
					case lexer::tokenid::pipe:
						return "|";
					case lexer::tokenid::brace_close:
						return "}";
					case lexer::tokenid::tilde:
						return "~";
					case lexer::tokenid::exclaim_equal:
						return "!=";
					case lexer::tokenid::percent_equal:
						return "%=";
					case lexer::tokenid::ampersand_ampersand:
						return "&&";
					case lexer::tokenid::ampersand_equal:
						return "&=";
					case lexer::tokenid::star_equal:
						return "*=";
					case lexer::tokenid::plus_plus:
						return "++";
					case lexer::tokenid::plus_equal:
						return "+=";
					case lexer::tokenid::minus_minus:
						return "--";
					case lexer::tokenid::minus_equal:
						return "-=";
					case lexer::tokenid::arrow:
						return "->";
					case lexer::tokenid::ellipsis:
						return "...";
					case lexer::tokenid::slash_equal:
						return "|=";
					case lexer::tokenid::colon_colon:
						return "::";
					case lexer::tokenid::less_less_equal:
						return "<<=";
					case lexer::tokenid::less_less:
						return "<<";
					case lexer::tokenid::less_equal:
						return "<=";
					case lexer::tokenid::equal_equal:
						return "==";
					case lexer::tokenid::greater_greater_equal:
						return ">>=";
					case lexer::tokenid::greater_greater:
						return ">>";
					case lexer::tokenid::greater_equal:
						return ">=";
					case lexer::tokenid::caret_equal:
						return "^=";
					case lexer::tokenid::pipe_equal:
						return "|=";
					case lexer::tokenid::pipe_pipe:
						return "||";
					case lexer::tokenid::identifier:
						return "identifier";
					case lexer::tokenid::reserved:
						return "reserved word";
					case lexer::tokenid::true_literal:
						return "true";
					case lexer::tokenid::false_literal:
						return "false";
					case lexer::tokenid::int_literal:
					case lexer::tokenid::uint_literal:
						return "integral literal";
					case lexer::tokenid::float_literal:
					case lexer::tokenid::double_literal:
						return "floating point literal";
					case lexer::tokenid::string_literal:
						return "string literal";
					case lexer::tokenid::namespace_:
						return "namespace";
					case lexer::tokenid::struct_:
						return "struct";
					case lexer::tokenid::technique:
						return "technique";
					case lexer::tokenid::pass:
						return "pass";
					case lexer::tokenid::for_:
						return "for";
					case lexer::tokenid::while_:
						return "while";
					case lexer::tokenid::do_:
						return "do";
					case lexer::tokenid::if_:
						return "if";
					case lexer::tokenid::else_:
						return "else";
					case lexer::tokenid::switch_:
						return "switch";
					case lexer::tokenid::case_:
						return "case";
					case lexer::tokenid::default_:
						return "default";
					case lexer::tokenid::break_:
						return "break";
					case lexer::tokenid::continue_:
						return "continue";
					case lexer::tokenid::return_:
						return "return";
					case lexer::tokenid::discard_:
						return "discard";
					case lexer::tokenid::extern_:
						return "extern";
					case lexer::tokenid::static_:
						return "static";
					case lexer::tokenid::uniform_:
						return "uniform";
					case lexer::tokenid::volatile_:
						return "volatile";
					case lexer::tokenid::precise:
						return "precise";
					case lexer::tokenid::in:
						return "in";
					case lexer::tokenid::out:
						return "out";
					case lexer::tokenid::inout:
						return "inout";
					case lexer::tokenid::const_:
						return "const";
					case lexer::tokenid::linear:
						return "linear";
					case lexer::tokenid::noperspective:
						return "noperspective";
					case lexer::tokenid::centroid:
						return "centroid";
					case lexer::tokenid::nointerpolation:
						return "nointerpolation";
					case lexer::tokenid::void_:
						return "void";
					case lexer::tokenid::bool_:
					case lexer::tokenid::bool2:
					case lexer::tokenid::bool3:
					case lexer::tokenid::bool4:
					case lexer::tokenid::bool2x2:
					case lexer::tokenid::bool3x3:
					case lexer::tokenid::bool4x4:
						return "bool";
					case lexer::tokenid::int_:
					case lexer::tokenid::int2:
					case lexer::tokenid::int3:
					case lexer::tokenid::int4:
					case lexer::tokenid::int2x2:
					case lexer::tokenid::int3x3:
					case lexer::tokenid::int4x4:
						return "int";
					case lexer::tokenid::uint_:
					case lexer::tokenid::uint2:
					case lexer::tokenid::uint3:
					case lexer::tokenid::uint4:
					case lexer::tokenid::uint2x2:
					case lexer::tokenid::uint3x3:
					case lexer::tokenid::uint4x4:
						return "uint";
					case lexer::tokenid::float_:
					case lexer::tokenid::float2:
					case lexer::tokenid::float3:
					case lexer::tokenid::float4:
					case lexer::tokenid::float2x2:
					case lexer::tokenid::float3x3:
					case lexer::tokenid::float4x4:
						return "float";
					case lexer::tokenid::vector:
						return "vector";
					case lexer::tokenid::matrix:
						return "matrix";
					case lexer::tokenid::string_:
						return "string";
					case lexer::tokenid::texture1d:
						return "texture1D";
					case lexer::tokenid::texture2d:
						return "texture2D";
					case lexer::tokenid::texture3d:
						return "texture3D";
					case lexer::tokenid::sampler1d:
						return "sampler1D";
					case lexer::tokenid::sampler2d:
						return "sampler2D";
					case lexer::tokenid::sampler3d:
						return "sampler3D";
				}
			}
		}

		class parser
		{
		public:
			explicit parser(const std::string &input, nodetree &ast, std::string &errors);

			bool parse();

		protected:
			void backup();
			void restore();

			bool peek(char tok) const
			{
				return peek(static_cast<lexer::tokenid>(tok));
			}
			bool peek(lexer::tokenid tokid) const;
			void consume();
			void consume_until(char token)
			{
				consume_until(static_cast<lexer::tokenid>(token));
			}
			void consume_until(lexer::tokenid tokid);
			bool accept(char tok)
			{
				return accept(static_cast<lexer::tokenid>(tok));
			}
			bool accept(lexer::tokenid tokid);
			bool expect(char tok)
			{
				return expect(static_cast<lexer::tokenid>(tok));
			}
			bool expect(lexer::tokenid tokid);

		private:
			void error(const location &location, unsigned int code, const char *message, ...);
			void warning(const location &location, unsigned int code, const char *message, ...);

			// Types
			bool accept_type_class(nodes::type_node &type);
			bool accept_type_qualifiers(nodes::type_node &type);
			bool parse_type(nodes::type_node &type);

			// Expressions
			bool accept_unary_op(enum nodes::unary_expression_node::op &op);
			bool accept_postfix_op(enum nodes::unary_expression_node::op &op);
			bool peek_multary_op(enum nodes::binary_expression_node::op &op, unsigned int &precedence) const;
			bool accept_assignment_op(enum nodes::assignment_expression_node::op &op);
			bool parse_expression(nodes::expression_node *&expression);
			bool parse_expression_unary(nodes::expression_node *&expression);
			bool parse_expression_multary(nodes::expression_node *&expression, unsigned int precedence = 0);
			bool parse_expression_assignment(nodes::expression_node *&expression);

			// Statements
			bool parse_statement(nodes::statement_node *&statement, bool scoped = true);
			bool parse_statement_block(nodes::statement_node *&statement, bool scoped = true);
			bool parse_statement_declarator_list(nodes::statement_node *&statement);

			// Declarations
			bool parse_top_level();
			bool parse_namespace();
			bool parse_array(int &size);
			bool parse_annotations(std::vector<nodes::annotation_node> &annotations);
			bool parse_struct(nodes::struct_declaration_node *&structure);
			bool parse_function_residue(nodes::type_node &type, std::string name, nodes::function_declaration_node *&function);
			bool parse_variable_residue(nodes::type_node &type, std::string name, nodes::variable_declaration_node *&variable, bool global = false);
			bool parse_variable_assignment(nodes::expression_node *&expression);
			bool parse_variable_properties(nodes::variable_declaration_node *variable);
			bool parse_variable_properties_expression(nodes::expression_node *&expression);
			bool parse_technique(nodes::technique_declaration_node *&technique);
			bool parse_technique_pass(nodes::pass_declaration_node *&pass);
			bool parse_technique_pass_expression(nodes::expression_node *&expression);

			// Symbol Table
			struct scope
			{
				std::string name;
				unsigned int level, namespace_level;
			};
			typedef nodes::declaration_node symbol;

			void enter_scope(symbol *parent = nullptr);
			void enter_namespace(const std::string &name);
			void leave_scope();
			void leave_namespace();
			bool insert_symbol(symbol *symbol, bool global = false);
			symbol *find_symbol(const std::string &name) const;
			symbol *find_symbol(const std::string &name, const scope &scope, bool exclusive = false) const;
			bool resolve_call(nodes::call_expression_node *call, const scope &scope, bool &intrinsic, bool &ambiguous) const;
			nodes::expression_node *fold_constant_expression(nodes::expression_node *expression) const;

			nodetree *_ast;
			std::string *_errors;
			std::unique_ptr<lexer> _lexer, _lexer_backup;
			lexer::token _token, _token_next, _token_backup;
			scope _current_scope;
			std::stack<symbol *> _parent_stack;
			std::unordered_map<std::string, std::vector<std::pair<scope, symbol *>>> _symbol_stack;
		};

		bool parse(const std::string &source, nodetree &ast, std::string &errors)
		{
			return parser(source, ast, errors).parse();
		}

		parser::parser(const std::string &input, nodetree &ast, std::string &errors) : _ast(&ast), _errors(&errors), _lexer(new lexer(input))
		{
			_current_scope.name = "::";
			_current_scope.level = 0;
			_current_scope.namespace_level = 0;
		}

		void parser::backup()
		{
			_lexer.swap(_lexer_backup);
			_lexer.reset(new lexer(*_lexer_backup));
			_token_backup = _token_next;
		}
		void parser::restore()
		{
			_lexer.swap(_lexer_backup);
			_token_next = _token_backup;
		}

		bool parser::peek(lexer::tokenid tokid) const
		{
			return _token_next.id == tokid;
		}
		void parser::consume()
		{
			_token = _token_next;
			_token_next = _lexer->lex();
		}
		void parser::consume_until(lexer::tokenid tokid)
		{
			while (!accept(tokid) && !peek(lexer::tokenid::end_of_file))
			{
				consume();
			}
		}
		bool parser::accept(lexer::tokenid tokid)
		{
			if (peek(tokid))
			{
				consume();

				return true;
			}

			return false;
		}
		bool parser::expect(lexer::tokenid tokid)
		{
			if (!accept(tokid))
			{
				error(_token_next.location, 3000, "syntax error: unexpected '%s', expected '%s'", get_token_name(_token_next.id), get_token_name(tokid));

				return false;
			}

			return true;
		}

		void parser::error(const location &location, unsigned int code, const char *message, ...)
		{
			*_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

			if (code == 0)
			{
				*_errors += "error: ";
			}
			else
			{
				*_errors += "error X" + std::to_string(code) + ": ";
			}

			char formatted[512];

			va_list args;
			va_start(args, message);
			vsprintf_s(formatted, message, args);
			va_end(args);

			*_errors += formatted;
			*_errors += '\n';
		}
		void parser::warning(const location &location, unsigned int code, const char *message, ...)
		{
			*_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

			if (code == 0)
			{
				*_errors += "warning: ";
			}
			else
			{
				*_errors += "warning X" + std::to_string(code) + ": ";
			}

			char formatted[512];

			va_list args;
			va_start(args, message);
			vsprintf_s(formatted, message, args);
			va_end(args);

			*_errors += formatted;
			*_errors += '\n';
		}

		// Types
		bool parser::accept_type_class(nodes::type_node &type)
		{
			type.definition = nullptr;
			type.array_length = 0;

			if (peek(lexer::tokenid::identifier))
			{
				type.rows = type.cols = 0;
				type.basetype = nodes::type_node::datatype_struct;

				const auto symbol = find_symbol(_token_next.literal_as_string);

				if (symbol != nullptr && symbol->id == nodeid::struct_declaration)
				{
					type.definition = static_cast<nodes::struct_declaration_node *>(symbol);

					consume();
				}
				else
				{
					return false;
				}
			}
			else if (accept(lexer::tokenid::vector))
			{
				type.rows = 4, type.cols = 1;
				type.basetype = nodes::type_node::datatype_float;

				if (accept('<'))
				{
					if (!accept_type_class(type))
					{
						error(_token_next.location, 3000, "syntax error: unexpected '%s', expected vector element type", get_token_name(_token_next.id));

						return false;
					}

					if (!type.is_scalar())
					{
						error(_token.location, 3122, "vector element type must be a scalar type");

						return false;
					}

					if (!(expect(',') && expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						error(_token.location, 3052, "vector dimension must be between 1 and 4");

						return false;
					}

					type.rows = _token.literal_as_int;

					if (!expect('>'))
					{
						return false;
					}
				}
			}
			else if (accept(lexer::tokenid::matrix))
			{
				type.rows = 4, type.cols = 4;
				type.basetype = nodes::type_node::datatype_float;

				if (accept('<'))
				{
					if (!accept_type_class(type))
					{
						error(_token_next.location, 3000, "syntax error: unexpected '%s', expected matrix element type", get_token_name(_token_next.id));

						return false;
					}

					if (!type.is_scalar())
					{
						error(_token.location, 3123, "matrix element type must be a scalar type");

						return false;
					}

					if (!(expect(',') && expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.rows = _token.literal_as_int;

					if (!(expect(',') && expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.cols = _token.literal_as_int;

					if (!expect('>'))
					{
						return false;
					}
				}
			}
			else
			{
				type.rows = type.cols = 0;

				switch (_token_next.id)
				{
					case lexer::tokenid::void_:
						type.basetype = nodes::type_node::datatype_void;
						break;
					case lexer::tokenid::bool_:
						type.rows = 1, type.cols = 1;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool2:
						type.rows = 2, type.cols = 1;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool2x2:
						type.rows = 2, type.cols = 2;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool3:
						type.rows = 3, type.cols = 1;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool3x3:
						type.rows = 3, type.cols = 3;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool4:
						type.rows = 4, type.cols = 1;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::bool4x4:
						type.rows = 4, type.cols = 4;
						type.basetype = nodes::type_node::datatype_bool;
						break;
					case lexer::tokenid::int_:
						type.rows = 1, type.cols = 1;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int2:
						type.rows = 2, type.cols = 1;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int2x2:
						type.rows = 2, type.cols = 2;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int3:
						type.rows = 3, type.cols = 1;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int3x3:
						type.rows = 3, type.cols = 3;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int4:
						type.rows = 4, type.cols = 1;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::int4x4:
						type.rows = 4, type.cols = 4;
						type.basetype = nodes::type_node::datatype_int;
						break;
					case lexer::tokenid::uint_:
						type.rows = 1, type.cols = 1;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint2:
						type.rows = 2, type.cols = 1;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint2x2:
						type.rows = 2, type.cols = 2;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint3:
						type.rows = 3, type.cols = 1;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint3x3:
						type.rows = 3, type.cols = 3;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint4:
						type.rows = 4, type.cols = 1;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::uint4x4:
						type.rows = 4, type.cols = 4;
						type.basetype = nodes::type_node::datatype_uint;
						break;
					case lexer::tokenid::float_:
						type.rows = 1, type.cols = 1;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float2:
						type.rows = 2, type.cols = 1;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float2x2:
						type.rows = 2, type.cols = 2;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float3:
						type.rows = 3, type.cols = 1;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float3x3:
						type.rows = 3, type.cols = 3;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float4:
						type.rows = 4, type.cols = 1;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::float4x4:
						type.rows = 4, type.cols = 4;
						type.basetype = nodes::type_node::datatype_float;
						break;
					case lexer::tokenid::string_:
						type.basetype = nodes::type_node::datatype_string;
						break;
					case lexer::tokenid::texture1d:
					case lexer::tokenid::texture2d:
					case lexer::tokenid::texture3d:
						type.basetype = nodes::type_node::datatype_texture;
						break;
					case lexer::tokenid::sampler1d:
					case lexer::tokenid::sampler2d:
					case lexer::tokenid::sampler3d:
						type.basetype = nodes::type_node::datatype_sampler;
						break;
					default:
						return false;
				}

				consume();
			}

			return true;
		}
		bool parser::accept_type_qualifiers(nodes::type_node &type)
		{
			unsigned int qualifiers = 0;

			// Storage
			if (accept(lexer::tokenid::extern_))
			{
				qualifiers |= nodes::type_node::qualifier_extern;
			}
			if (accept(lexer::tokenid::static_))
			{
				qualifiers |= nodes::type_node::qualifier_static;
			}
			if (accept(lexer::tokenid::uniform_))
			{
				qualifiers |= nodes::type_node::qualifier_uniform;
			}
			if (accept(lexer::tokenid::volatile_))
			{
				qualifiers |= nodes::type_node::qualifier_volatile;
			}
			if (accept(lexer::tokenid::precise))
			{
				qualifiers |= nodes::type_node::qualifier_precise;
			}

			if (accept(lexer::tokenid::in))
			{
				qualifiers |= nodes::type_node::qualifier_in;
			}
			if (accept(lexer::tokenid::out))
			{
				qualifiers |= nodes::type_node::qualifier_out;
			}
			if (accept(lexer::tokenid::inout))
			{
				qualifiers |= nodes::type_node::qualifier_inout;
			}

			// Modifiers
			if (accept(lexer::tokenid::const_))
			{
				qualifiers |= nodes::type_node::qualifier_const;
			}

			// Interpolation
			if (accept(lexer::tokenid::linear))
			{
				qualifiers |= nodes::type_node::qualifier_linear;
			}
			if (accept(lexer::tokenid::noperspective))
			{
				qualifiers |= nodes::type_node::qualifier_noperspective;
			}
			if (accept(lexer::tokenid::centroid))
			{
				qualifiers |= nodes::type_node::qualifier_centroid;
			}
			if (accept(lexer::tokenid::nointerpolation))
			{
				qualifiers |= nodes::type_node::qualifier_nointerpolation;
			}

			if (qualifiers == 0)
			{
				return false;
			}
			if ((type.qualifiers & qualifiers) == qualifiers)
			{
				warning(_token.location, 3048, "duplicate usages specified");
			}

			type.qualifiers |= qualifiers;

			accept_type_qualifiers(type);

			return true;
		}
		bool parser::parse_type(nodes::type_node &type)
		{
			type.qualifiers = 0;

			accept_type_qualifiers(type);

			const auto location = _token_next.location;

			if (!accept_type_class(type))
			{
				return false;
			}

			if (type.is_integral() && (type.has_qualifier(nodes::type_node::qualifier_centroid) || type.has_qualifier(nodes::type_node::qualifier_noperspective)))
			{
				error(location, 4576, "signature specifies invalid interpolation mode for integer component type");

				return false;
			}

			if (type.has_qualifier(nodes::type_node::qualifier_centroid) && !type.has_qualifier(nodes::type_node::qualifier_noperspective))
			{
				type.qualifiers |= nodes::type_node::qualifier_linear;
			}

			return true;
		}

		// Expressions
		bool parser::accept_unary_op(enum nodes::unary_expression_node::op &op)
		{
			switch (_token_next.id)
			{
				case lexer::tokenid::exclaim:
					op = nodes::unary_expression_node::logical_not;
					break;
				case lexer::tokenid::plus:
					op = nodes::unary_expression_node::none;
					break;
				case lexer::tokenid::minus:
					op = nodes::unary_expression_node::negate;
					break;
				case lexer::tokenid::tilde:
					op = nodes::unary_expression_node::bitwise_not;
					break;
				case lexer::tokenid::plus_plus:
					op = nodes::unary_expression_node::pre_increase;
					break;
				case lexer::tokenid::minus_minus:
					op = nodes::unary_expression_node::pre_decrease;
					break;
				default:
					return false;
			}

			consume();

			return true;
		}
		bool parser::accept_postfix_op(enum nodes::unary_expression_node::op &op)
		{
			switch (_token_next.id)
			{
				case lexer::tokenid::plus_plus:
					op = nodes::unary_expression_node::post_increase;
					break;
				case lexer::tokenid::minus_minus:
					op = nodes::unary_expression_node::post_decrease;
					break;
				default:
					return false;
			}

			consume();

			return true;
		}
		bool parser::peek_multary_op(enum nodes::binary_expression_node::op &op, unsigned int &precedence) const
		{
			switch (_token_next.id)
			{
				case lexer::tokenid::percent:
					op = nodes::binary_expression_node::modulo;
					precedence = 11;
					break;
				case lexer::tokenid::ampersand:
					op = nodes::binary_expression_node::bitwise_and;
					precedence = 6;
					break;
				case lexer::tokenid::star:
					op = nodes::binary_expression_node::multiply;
					precedence = 11;
					break;
				case lexer::tokenid::plus:
					op = nodes::binary_expression_node::add;
					precedence = 10;
					break;
				case lexer::tokenid::minus:
					op = nodes::binary_expression_node::subtract;
					precedence = 10;
					break;
				case lexer::tokenid::slash:
					op = nodes::binary_expression_node::divide;
					precedence = 11;
					break;
				case lexer::tokenid::less:
					op = nodes::binary_expression_node::less;
					precedence = 8;
					break;
				case lexer::tokenid::greater:
					op = nodes::binary_expression_node::greater;
					precedence = 8;
					break;
				case lexer::tokenid::question:
					op = nodes::binary_expression_node::none;
					precedence = 1;
					break;
				case lexer::tokenid::caret:
					op = nodes::binary_expression_node::bitwise_xor;
					precedence = 5;
					break;
				case lexer::tokenid::pipe:
					op = nodes::binary_expression_node::bitwise_or;
					precedence = 4;
					break;
				case lexer::tokenid::exclaim_equal:
					op = nodes::binary_expression_node::not_equal;
					precedence = 7;
					break;
				case lexer::tokenid::ampersand_ampersand:
					op = nodes::binary_expression_node::logical_and;
					precedence = 3;
					break;
				case lexer::tokenid::less_less:
					op = nodes::binary_expression_node::left_shift;
					precedence = 9;
					break;
				case lexer::tokenid::less_equal:
					op = nodes::binary_expression_node::less_equal;
					precedence = 8;
					break;
				case lexer::tokenid::equal_equal:
					op = nodes::binary_expression_node::equal;
					precedence = 7;
					break;
				case lexer::tokenid::greater_greater:
					op = nodes::binary_expression_node::right_shift;
					precedence = 9;
					break;
				case lexer::tokenid::greater_equal:
					op = nodes::binary_expression_node::greater_equal;
					precedence = 8;
					break;
				case lexer::tokenid::pipe_pipe:
					op = nodes::binary_expression_node::logical_or;
					precedence = 2;
					break;
				default:
					return false;
			}

			return true;
		}
		bool parser::accept_assignment_op(enum nodes::assignment_expression_node::op &op)
		{
			switch (_token_next.id)
			{
				case lexer::tokenid::equal:
					op = nodes::assignment_expression_node::none;
					break;
				case lexer::tokenid::percent_equal:
					op = nodes::assignment_expression_node::modulo;
					break;
				case lexer::tokenid::ampersand_equal:
					op = nodes::assignment_expression_node::bitwise_and;
					break;
				case lexer::tokenid::star_equal:
					op = nodes::assignment_expression_node::multiply;
					break;
				case lexer::tokenid::plus_equal:
					op = nodes::assignment_expression_node::add;
					break;
				case lexer::tokenid::minus_equal:
					op = nodes::assignment_expression_node::subtract;
					break;
				case lexer::tokenid::slash_equal:
					op = nodes::assignment_expression_node::divide;
					break;
				case lexer::tokenid::less_less_equal:
					op = nodes::assignment_expression_node::left_shift;
					break;
				case lexer::tokenid::greater_greater_equal:
					op = nodes::assignment_expression_node::right_shift;
					break;
				case lexer::tokenid::caret_equal:
					op = nodes::assignment_expression_node::bitwise_xor;
					break;
				case lexer::tokenid::pipe_equal:
					op = nodes::assignment_expression_node::bitwise_or;
					break;
				default:
					return false;
			}

			consume();

			return true;
		}
		bool parser::parse_expression(nodes::expression_node *&node)
		{
			if (!parse_expression_assignment(node))
			{
				return false;
			}

			if (peek(','))
			{
				const auto sequence = _ast->make_node<nodes::expression_sequence_node>(node->location);
				sequence->expression_list.push_back(node);

				while (accept(','))
				{
					nodes::expression_node *expression = nullptr;

					if (!parse_expression_assignment(expression))
					{
						return false;
					}

					sequence->expression_list.push_back(std::move(expression));
				}

				sequence->type = sequence->expression_list.back()->type;

				node = sequence;
			}

			return true;
		}
		bool parser::parse_expression_unary(nodes::expression_node *&node)
		{
			nodes::type_node type;
			enum nodes::unary_expression_node::op op;
			auto location = _token_next.location;

			#pragma region Prefix
			if (accept_unary_op(op))
			{
				if (!parse_expression_unary(node))
				{
					return false;
				}

				if (!node->type.is_scalar() && !node->type.is_vector() && !node->type.is_matrix())
				{
					error(node->location, 3022, "scalar, vector, or matrix expected");

					return false;
				}

				if (op != nodes::unary_expression_node::none)
				{
					if (op == nodes::unary_expression_node::bitwise_not && !node->type.is_integral())
					{
						error(node->location, 3082, "int or unsigned int type required");

						return false;
					}
					else if ((op == nodes::unary_expression_node::pre_increase || op == nodes::unary_expression_node::pre_decrease) && (node->type.has_qualifier(nodes::type_node::qualifier_const) || node->type.has_qualifier(nodes::type_node::qualifier_uniform)))
					{
						error(node->location, 3025, "l-value specifies const object");

						return false;
					}

					const auto newexpression = _ast->make_node<nodes::unary_expression_node>(location);
					newexpression->type = node->type;
					newexpression->op = op;
					newexpression->operand = node;

					node = fold_constant_expression(newexpression);
				}

				type = node->type;
			}
			else if (accept('('))
			{
				backup();

				if (accept_type_class(type))
				{
					if (peek('('))
					{
						restore();
					}
					else if (expect(')'))
					{
						if (!parse_expression_unary(node))
						{
							return false;
						}

						if (node->type.basetype == type.basetype && (node->type.rows == type.rows && node->type.cols == type.cols) && !(node->type.is_array() || type.is_array()))
						{
							return true;
						}
						else if (node->type.is_numeric() && type.is_numeric())
						{
							if ((node->type.rows < type.rows || node->type.cols < type.cols) && !node->type.is_scalar())
							{
								error(location, 3017, "cannot convert these vector types");

								return false;
							}

							if (node->type.rows > type.rows || node->type.cols > type.cols)
							{
								warning(location, 3206, "implicit truncation of vector type");
							}

							const auto castexpression = _ast->make_node<nodes::unary_expression_node>(location);
							type.qualifiers = nodes::type_node::qualifier_const;
							castexpression->type = type;
							castexpression->op = nodes::unary_expression_node::cast;
							castexpression->operand = node;

							node = fold_constant_expression(castexpression);

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
						return false;
					}
				}

				if (!parse_expression(node))
				{
					return false;
				}

				if (!expect(')'))
				{
					return false;
				}

				type = node->type;
			}
			else if (accept(lexer::tokenid::true_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_bool;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_int[0] = 1;

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::false_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_bool;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_int[0] = 0;

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::int_literal))
			{
				nodes::literal_expression_node *const literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_int;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_int[0] = _token.literal_as_int;

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::uint_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_uint;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_uint[0] = _token.literal_as_uint;

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::float_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_float;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_float[0] = _token.literal_as_float;

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::double_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_float;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
				literal->value_float[0] = static_cast<float>(_token.literal_as_double);

				node = literal;
				type = literal->type;
			}
			else if (accept(lexer::tokenid::string_literal))
			{
				const auto literal = _ast->make_node<nodes::literal_expression_node>(location);
				literal->type.basetype = nodes::type_node::datatype_string;
				literal->type.qualifiers = nodes::type_node::qualifier_const;
				literal->type.rows = literal->type.cols = 0, literal->type.array_length = 0;
				literal->value_string = _token.literal_as_string;

				while (accept(lexer::tokenid::string_literal))
				{
					literal->value_string += _token.literal_as_string;
				}

				node = literal;
				type = literal->type;
			}
			else if (accept_type_class(type))
			{
				if (!expect('('))
				{
					return false;
				}

				if (!type.is_numeric())
				{
					error(location, 3037, "constructors only defined for numeric base types");

					return false;
				}

				if (accept(')'))
				{
					error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				const auto constructor = _ast->make_node<nodes::constructor_expression_node>(location);
				constructor->type = type;
				constructor->type.qualifiers = nodes::type_node::qualifier_const;

				unsigned int elements = 0;

				while (!peek(')'))
				{
					if (!constructor->arguments.empty() && !expect(','))
					{
						return false;
					}

					nodes::expression_node *argument = nullptr;

					if (!parse_expression_assignment(argument))
					{
						return false;
					}

					if (!argument->type.is_numeric())
					{
						error(argument->location, 3017, "cannot convert non-numeric types");

						return false;
					}

					elements += argument->type.rows * argument->type.cols;

					constructor->arguments.push_back(std::move(argument));
				}

				if (!expect(')'))
				{
					return false;
				}

				if (elements != type.rows * type.cols)
				{
					error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				if (constructor->arguments.size() > 1)
				{
					node = constructor;
					type = constructor->type;
				}
				else
				{
					const auto castexpression = _ast->make_node<nodes::unary_expression_node>(constructor->location);
					castexpression->type = type;
					castexpression->op = nodes::unary_expression_node::cast;
					castexpression->operand = constructor->arguments[0];

					node = castexpression;
				}

				node = fold_constant_expression(node);
			}
			else
			{
				scope scope;
				bool exclusive;
				std::string identifier;

				if (accept(lexer::tokenid::colon_colon))
				{
					scope.namespace_level = scope.level = 0;
					exclusive = true;
				}
				else
				{
					scope = _current_scope;
					exclusive = false;
				}

				if (exclusive ? expect(lexer::tokenid::identifier) : accept(lexer::tokenid::identifier))
				{
					identifier = _token.literal_as_string;
				}
				else
				{
					return false;
				}

				while (accept(lexer::tokenid::colon_colon))
				{
					if (!expect(lexer::tokenid::identifier))
					{
						return false;
					}

					identifier += "::" + _token.literal_as_string;
				}

				const auto symbol = find_symbol(identifier, scope, exclusive);

				if (accept('('))
				{
					if (symbol != nullptr && symbol->id == nodeid::variable_declaration)
					{
						error(location, 3005, "identifier '%s' represents a variable, not a function", identifier.c_str());

						return false;
					}

					const auto callexpression = _ast->make_node<nodes::call_expression_node>(location);
					callexpression->callee_name = identifier;

					while (!peek(')'))
					{
						if (!callexpression->arguments.empty() && !expect(','))
						{
							return false;
						}

						nodes::expression_node *argument = nullptr;

						if (!parse_expression_assignment(argument))
						{
							return false;
						}

						callexpression->arguments.push_back(std::move(argument));
					}

					if (!expect(')'))
					{
						return false;
					}

					bool undeclared = symbol == nullptr, intrinsic = false, ambiguous = false;

					if (!resolve_call(callexpression, scope, intrinsic, ambiguous))
					{
						if (undeclared && !intrinsic)
						{
							error(location, 3004, "undeclared identifier '%s'", identifier.c_str());
						}
						else if (ambiguous)
						{
							error(location, 3067, "ambiguous function call to '%s'", identifier.c_str());
						}
						else
						{
							error(location, 3013, "no matching function overload for '%s'", identifier.c_str());
						}

						return false;
					}

					if (intrinsic)
					{
						const auto newexpression = _ast->make_node<nodes::intrinsic_expression_node>(callexpression->location);
						newexpression->type = callexpression->type;
						newexpression->op = static_cast<enum nodes::intrinsic_expression_node::op>(reinterpret_cast<unsigned int>(callexpression->callee));

						for (size_t i = 0, count = std::min(callexpression->arguments.size(), sizeof(newexpression->arguments) / sizeof(*newexpression->arguments)); i < count; ++i)
						{
							newexpression->arguments[i] = callexpression->arguments[i];
						}

						node = fold_constant_expression(newexpression);
					}
					else
					{
						const auto parent = _parent_stack.empty() ? nullptr : _parent_stack.top();

						if (parent == callexpression->callee)
						{
							error(location, 3500, "recursive function calls are not allowed");

							return false;
						}

						node = callexpression;
					}

					type = node->type;
				}
				else
				{
					if (symbol == nullptr)
					{
						error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

						return false;
					}

					if (symbol->id != nodeid::variable_declaration)
					{
						error(location, 3005, "identifier '%s' represents a function, not a variable", identifier.c_str());

						return false;
					}

					const auto newexpression = _ast->make_node<nodes::lvalue_expression_node>(location);
					newexpression->reference = static_cast<const nodes::variable_declaration_node *>(symbol);
					newexpression->type = newexpression->reference->type;

					node = fold_constant_expression(newexpression);
					type = node->type;
				}
			}
			#pragma endregion

			#pragma region Postfix
			while (!peek(lexer::tokenid::end_of_file))
			{
				location = _token_next.location;

				if (accept_postfix_op(op))
				{
					if (!type.is_scalar() && !type.is_vector() && !type.is_matrix())
					{
						error(node->location, 3022, "scalar, vector, or matrix expected");

						return false;
					}

					if (type.has_qualifier(nodes::type_node::qualifier_const) || type.has_qualifier(nodes::type_node::qualifier_uniform))
					{
						error(node->location, 3025, "l-value specifies const object");

						return false;
					}

					const auto newexpression = _ast->make_node<nodes::unary_expression_node>(location);
					newexpression->type = type;
					newexpression->type.qualifiers |= nodes::type_node::qualifier_const;
					newexpression->op = op;
					newexpression->operand = node;

					node = newexpression;
					type = node->type;
				}
				else if (accept('.'))
				{
					if (!expect(lexer::tokenid::identifier))
					{
						return false;
					}

					location = _token.location;
					const auto subscript = _token.literal_as_string;

					if (accept('('))
					{
						if (!type.is_struct() || type.is_array())
						{
							error(location, 3087, "object does not have methods");
						}
						else
						{
							error(location, 3088, "structures do not have methods");
						}

						return false;
					}
					else if (type.is_array())
					{
						error(location, 3018, "invalid subscript on array");

						return false;
					}
					else if (type.is_vector())
					{
						const size_t length = subscript.size();

						if (length > 4)
						{
							error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

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
									error(location, 3018, "invalid subscript '%s'", subscript.c_str());
									return false;
							}

							if (i > 0 && (set[i] != set[i - 1]))
							{
								error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}
							if (static_cast<unsigned int>(offsets[i]) >= type.rows)
							{
								error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());

								return false;
							}

							for (size_t k = 0; k < i; ++k)
							{
								if (offsets[k] == offsets[i])
								{
									constant = true;
									break;
								}
							}
						}

						const auto newexpression = _ast->make_node<nodes::swizzle_expression_node>(location);
						newexpression->type = type;
						newexpression->type.rows = static_cast<unsigned int>(length);
						newexpression->operand = node;
						newexpression->mask[0] = offsets[0];
						newexpression->mask[1] = offsets[1];
						newexpression->mask[2] = offsets[2];
						newexpression->mask[3] = offsets[3];

						if (constant || type.has_qualifier(nodes::type_node::qualifier_uniform))
						{
							newexpression->type.qualifiers |= nodes::type_node::qualifier_const;
							newexpression->type.qualifiers &= ~nodes::type_node::qualifier_uniform;
						}

						node = fold_constant_expression(newexpression);
						type = node->type;
					}
					else if (type.is_matrix())
					{
						const size_t length = subscript.size();

						if (length < 3)
						{
							error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						bool constant = false;
						signed char offsets[4] = { -1, -1, -1, -1 };
						const unsigned int set = subscript[1] == 'm';
						const char coefficient = static_cast<char>(!set);

						for (size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
						{
							if (subscript[i] != '_' || subscript[i + set + 1] < '0' + coefficient || subscript[i + set + 1] > '3' + coefficient || subscript[i + set + 2] < '0' + coefficient || subscript[i + set + 2] > '3' + coefficient)
							{
								error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}
							if (set && subscript[i + 1] != 'm')
							{
								error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}

							const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
							const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

							if ((row >= type.rows || col >= type.cols) || j > 3)
							{
								error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());

								return false;
							}

							offsets[j] = static_cast<unsigned char>(row * 4 + col);

							for (size_t k = 0; k < j; ++k)
							{
								if (offsets[k] == offsets[j])
								{
									constant = true;
									break;
								}
							}
						}

						const auto newexpression = _ast->make_node<nodes::swizzle_expression_node>(_token.location);
						newexpression->type = type;
						newexpression->type.rows = static_cast<unsigned int>(length / (3 + set));
						newexpression->type.cols = 1;
						newexpression->operand = node;
						newexpression->mask[0] = offsets[0];
						newexpression->mask[1] = offsets[1];
						newexpression->mask[2] = offsets[2];
						newexpression->mask[3] = offsets[3];

						if (constant || type.has_qualifier(nodes::type_node::qualifier_uniform))
						{
							newexpression->type.qualifiers |= nodes::type_node::qualifier_const;
							newexpression->type.qualifiers &= ~nodes::type_node::qualifier_uniform;
						}

						node = fold_constant_expression(newexpression);
						type = node->type;
					}
					else if (type.is_struct())
					{
						nodes::variable_declaration_node *field = nullptr;

						for (auto currfield : type.definition->field_list)
						{
							if (currfield->name == subscript)
							{
								field = currfield;
								break;
							}
						}

						if (field == nullptr)
						{
							error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						const auto newexpression = _ast->make_node<nodes::field_expression_node>(location);
						newexpression->type = field->type;
						newexpression->operand = node;
						newexpression->field_reference = field;

						if (type.has_qualifier(nodes::type_node::qualifier_uniform))
						{
							newexpression->type.qualifiers |= nodes::type_node::qualifier_const;
							newexpression->type.qualifiers &= ~nodes::type_node::qualifier_uniform;
						}

						node = newexpression;
						type = node->type;
					}
					else if (type.is_scalar())
					{
						signed char offsets[4] = { -1, -1, -1, -1 };
						const size_t length = subscript.size();

						for (size_t i = 0; i < length; ++i)
						{
							if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
							{
								error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}

							offsets[i] = 0;
						}

						const auto newexpression = _ast->make_node<nodes::swizzle_expression_node>(location);
						newexpression->type = type;
						newexpression->type.qualifiers |= nodes::type_node::qualifier_const;
						newexpression->type.rows = static_cast<unsigned int>(length);
						newexpression->operand = node;
						newexpression->mask[0] = offsets[0];
						newexpression->mask[1] = offsets[1];
						newexpression->mask[2] = offsets[2];
						newexpression->mask[3] = offsets[3];

						node = newexpression;
						type = node->type;
					}
					else
					{
						error(location, 3018, "invalid subscript '%s'", subscript.c_str());

						return false;
					}
				}
				else if (accept('['))
				{
					if (!type.is_array() && !type.is_vector() && !type.is_matrix())
					{
						error(node->location, 3121, "array, matrix, vector, or indexable object type expected in index expression");

						return false;
					}

					const auto newexpression = _ast->make_node<nodes::binary_expression_node>(location);
					newexpression->type = type;
					newexpression->op = nodes::binary_expression_node::element_extract;
					newexpression->operands[0] = node;

					if (!parse_expression(newexpression->operands[1]))
					{
						return false;
					}

					if (!newexpression->operands[1]->type.is_scalar())
					{
						error(newexpression->operands[1]->location, 3120, "invalid type for index - index must be a scalar");

						return false;
					}

					if (type.is_array())
					{
						newexpression->type.array_length = 0;
					}
					else if (type.is_matrix())
					{
						newexpression->type.rows = newexpression->type.cols;
						newexpression->type.cols = 1;
					}
					else if (type.is_vector())
					{
						newexpression->type.rows = 1;
					}

					node = fold_constant_expression(newexpression);
					type = node->type;

					if (!expect(']'))
					{
						return false;
					}
				}
				else
				{
					break;
				}
			}
			#pragma endregion

			return true;
		}
		bool parser::parse_expression_multary(nodes::expression_node *&left, unsigned int left_precedence)
		{
			if (!parse_expression_unary(left))
			{
				return false;
			}

			enum nodes::binary_expression_node::op op;
			unsigned int right_precedence;

			while (peek_multary_op(op, right_precedence))
			{
				if (right_precedence <= left_precedence)
				{
					break;
				}

				consume();

				bool boolean = false;
				nodes::expression_node *right1 = nullptr, *right2 = nullptr;

				if (op != nodes::binary_expression_node::none)
				{
					if (!parse_expression_multary(right1, right_precedence))
					{
						return false;
					}

					if (op == nodes::binary_expression_node::equal || op == nodes::binary_expression_node::not_equal)
					{
						boolean = true;

						if (left->type.is_array() || right1->type.is_array() || left->type.definition != right1->type.definition)
						{
							error(right1->location, 3020, "type mismatch");

							return false;
						}
					}
					else if (op == nodes::binary_expression_node::bitwise_and || op == nodes::binary_expression_node::bitwise_or || op == nodes::binary_expression_node::bitwise_xor)
					{
						if (!left->type.is_integral())
						{
							error(left->location, 3082, "int or unsigned int type required");

							return false;
						}
						if (!right1->type.is_integral())
						{
							error(right1->location, 3082, "int or unsigned int type required");

							return false;
						}
					}
					else
					{
						boolean = op == nodes::binary_expression_node::logical_and || op == nodes::binary_expression_node::logical_or || op == nodes::binary_expression_node::less || op == nodes::binary_expression_node::greater || op == nodes::binary_expression_node::less_equal || op == nodes::binary_expression_node::greater_equal;

						if (!left->type.is_scalar() && !left->type.is_vector() && !left->type.is_matrix())
						{
							error(left->location, 3022, "scalar, vector, or matrix expected");

							return false;
						}
						if (!right1->type.is_scalar() && !right1->type.is_vector() && !right1->type.is_matrix())
						{
							error(right1->location, 3022, "scalar, vector, or matrix expected");

							return false;
						}
					}

					const auto newexpression = _ast->make_node<nodes::binary_expression_node>(left->location);
					newexpression->op = op;
					newexpression->operands[0] = left;
					newexpression->operands[1] = right1;

					right2 = right1, right1 = left;
					left = newexpression;
				}
				else
				{
					if (!left->type.is_scalar() && !left->type.is_vector())
					{
						error(left->location, 3022, "boolean or vector expression expected");

						return false;
					}

					if (!(parse_expression(right1) && expect(':') && parse_expression_assignment(right2)))
					{
						return false;
					}

					if (right1->type.is_array() || right2->type.is_array() || right1->type.definition != right2->type.definition)
					{
						error(left->location, 3020, "type mismatch between conditional values");

						return false;
					}

					const auto newexpression = _ast->make_node<nodes::conditional_expression_node>(left->location);
					newexpression->condition = left;
					newexpression->expression_when_true = right1;
					newexpression->expression_when_false = right2;

					left = newexpression;
				}

				if (boolean)
				{
					left->type.basetype = nodes::type_node::datatype_bool;
				}
				else
				{
					left->type.basetype = std::max(right1->type.basetype, right2->type.basetype);
				}

				if ((right1->type.rows == 1 && right2->type.cols == 1) || (right2->type.rows == 1 && right2->type.cols == 1))
				{
					left->type.rows = std::max(right1->type.rows, right2->type.rows);
					left->type.cols = std::max(right1->type.cols, right2->type.cols);
				}
				else
				{
					left->type.rows = std::min(right1->type.rows, right2->type.rows);
					left->type.cols = std::min(right1->type.cols, right2->type.cols);

					if (right1->type.rows > right2->type.rows || right1->type.cols > right2->type.cols)
					{
						warning(right1->location, 3206, "implicit truncation of vector type");
					}
					if (right2->type.rows > right1->type.rows || right2->type.cols > right1->type.cols)
					{
						warning(right2->location, 3206, "implicit truncation of vector type");
					}
				}

				left = fold_constant_expression(left);
			}

			return true;
		}
		bool parser::parse_expression_assignment(nodes::expression_node *&left)
		{
			if (!parse_expression_multary(left))
			{
				return false;
			}

			enum nodes::assignment_expression_node::op op;

			if (accept_assignment_op(op))
			{
				nodes::expression_node *right = nullptr;

				if (!parse_expression_multary(right))
				{
					return false;
				}

				if (left->type.has_qualifier(nodes::type_node::qualifier_const) || left->type.has_qualifier(nodes::type_node::qualifier_uniform))
				{
					error(left->location, 3025, "l-value specifies const object");

					return false;
				}

				if (left->type.is_array() || right->type.is_array() || !get_type_rank(left->type, right->type))
				{
					error(right->location, 3020, "cannot convert these types");

					return false;
				}

				if (right->type.rows > left->type.rows || right->type.cols > left->type.cols)
				{
					warning(right->location, 3206, "implicit truncation of vector type");
				}

				const auto assignment = _ast->make_node<nodes::assignment_expression_node>(left->location);
				assignment->type = left->type;
				assignment->op = op;
				assignment->left = left;
				assignment->right = right;

				left = assignment;
			}

			return true;
		}

		// Statements
		bool parser::parse_statement(nodes::statement_node *&statement, bool scoped)
		{
			std::vector<std::string> attributes;

			// Attributes
			while (accept('['))
			{
				if (expect(lexer::tokenid::identifier))
				{
					const auto attribute = _token.literal_as_string;

					if (expect(']'))
					{
						attributes.push_back(attribute);
					}
				}
				else
				{
					accept(']');
				}
			}

			if (peek('{'))
			{
				if (!parse_statement_block(statement, scoped))
				{
					return false;
				}

				statement->attributes = attributes;

				return true;
			}

			if (accept(';'))
			{
				statement = nullptr;

				return true;
			}

			#pragma region If
			if (accept(lexer::tokenid::if_))
			{
				const auto newstatement = _ast->make_node<nodes::if_statement_node>(_token.location);
				newstatement->attributes = attributes;

				if (!(expect('(') && parse_expression(newstatement->condition) && expect(')')))
				{
					return false;
				}

				if (!newstatement->condition->type.is_scalar())
				{
					error(newstatement->condition->location, 3019, "if statement conditional expressions must evaluate to a scalar");

					return false;
				}

				if (!parse_statement(newstatement->statement_when_true))
				{
					return false;
				}

				statement = newstatement;

				if (accept(lexer::tokenid::else_))
				{
					return parse_statement(newstatement->statement_when_false);
				}

				return true;
			}
			#pragma endregion

			#pragma region Switch
			if (accept(lexer::tokenid::switch_))
			{
				const auto newstatement = _ast->make_node<nodes::switch_statement_node>(_token.location);
				newstatement->attributes = attributes;

				if (!(expect('(') && parse_expression(newstatement->test_expression) && expect(')')))
				{
					return false;
				}

				if (!newstatement->test_expression->type.is_scalar())
				{
					error(newstatement->test_expression->location, 3019, "switch statement expression must evaluate to a scalar");

					return false;
				}

				if (!expect('{'))
				{
					return false;
				}

				while (!peek('}') && !peek(lexer::tokenid::end_of_file))
				{
					const auto casenode = _ast->make_node<nodes::case_statement_node>(struct location());

					while (accept(lexer::tokenid::case_) || accept(lexer::tokenid::default_))
					{
						nodes::expression_node *label = nullptr;

						if (_token.id == lexer::tokenid::case_)
						{
							if (!parse_expression(label))
							{
								return false;
							}

							if (label->id != nodeid::literal_expression || !label->type.is_numeric())
							{
								error(label->location, 3020, "non-numeric case expression");

								return false;
							}
						}

						if (!expect(':'))
						{
							return false;
						}

						casenode->labels.push_back(static_cast<nodes::literal_expression_node *>(label));
					}

					if (casenode->labels.empty())
					{
						return false;
					}

					casenode->location = casenode->labels[0]->location;

					if (!parse_statement(casenode->statement_list))
					{
						return false;
					}

					newstatement->case_list.push_back(casenode);
				}

				if (newstatement->case_list.empty())
				{
					warning(newstatement->location, 5002, "switch statement contains no 'case' or 'default' labels");

					statement = nullptr;
				}
				else
				{
					statement = newstatement;
				}

				return expect('}');
			}
			#pragma endregion

			#pragma region For
			if (accept(lexer::tokenid::for_))
			{
				const auto newstatement = _ast->make_node<nodes::for_statement_node>(_token.location);
				newstatement->attributes = attributes;

				if (!expect('('))
				{
					return false;
				}

				enter_scope();

				if (!parse_statement_declarator_list(newstatement->init_statement))
				{
					nodes::expression_node *expression = nullptr;

					if (parse_expression(expression))
					{
						const auto initialization = _ast->make_node<nodes::expression_statement_node>(expression->location);
						initialization->expression = expression;

						newstatement->init_statement = initialization;
					}
				}

				if (!expect(';'))
				{
					leave_scope();

					return false;
				}

				parse_expression(newstatement->condition);

				if (!expect(';'))
				{
					leave_scope();

					return false;
				}

				parse_expression(newstatement->increment_expression);

				if (!expect(')'))
				{
					leave_scope();

					return false;
				}

				if (!newstatement->condition->type.is_scalar())
				{
					error(newstatement->condition->location, 3019, "scalar value expected");

					return false;
				}

				if (!parse_statement(newstatement->statement_list, false))
				{
					leave_scope();

					return false;
				}

				leave_scope();

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region While
			if (accept(lexer::tokenid::while_))
			{
				const auto newstatement = _ast->make_node<nodes::while_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_do_while = false;

				enter_scope();

				if (!(expect('(') && parse_expression(newstatement->condition) && expect(')')))
				{
					leave_scope();

					return false;
				}

				if (!newstatement->condition->type.is_scalar())
				{
					error(newstatement->condition->location, 3019, "scalar value expected");

					leave_scope();

					return false;
				}

				if (!parse_statement(newstatement->statement_list, false))
				{
					leave_scope();

					return false;
				}

				leave_scope();

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region DoWhile
			if (accept(lexer::tokenid::do_))
			{
				const auto newstatement = _ast->make_node<nodes::while_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_do_while = true;

				if (!(parse_statement(newstatement->statement_list) && expect(lexer::tokenid::while_) && expect('(') && parse_expression(newstatement->condition) && expect(')') && expect(';')))
				{
					return false;
				}

				if (!newstatement->condition->type.is_scalar())
				{
					error(newstatement->condition->location, 3019, "scalar value expected");

					return false;
				}

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region Break
			if (accept(lexer::tokenid::break_))
			{
				const auto newstatement = _ast->make_node<nodes::jump_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_break = true;

				statement = newstatement;

				return expect(';');
			}
			#pragma endregion

			#pragma region Continue
			if (accept(lexer::tokenid::continue_))
			{
				const auto newstatement = _ast->make_node<nodes::jump_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_continue = true;

				statement = newstatement;

				return expect(';');
			}
			#pragma endregion

			#pragma region Return
			if (accept(lexer::tokenid::return_))
			{
				const auto newstatement = _ast->make_node<nodes::return_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_discard = false;

				const auto parent = static_cast<const nodes::function_declaration_node *>(_parent_stack.top());

				if (!peek(';'))
				{
					if (!parse_expression(newstatement->return_value))
					{
						return false;
					}

					if (parent->return_type.is_void())
					{
						error(newstatement->location, 3079, "void functions cannot return a value");

						accept(';');

						return false;
					}

					if (!get_type_rank(newstatement->return_value->type, parent->return_type))
					{
						error(newstatement->location, 3017, "expression does not match function return type");

						return false;
					}

					if (newstatement->return_value->type.rows > parent->return_type.rows || newstatement->return_value->type.cols > parent->return_type.cols)
					{
						warning(newstatement->location, 3206, "implicit truncation of vector type");
					}
				}
				else if (!parent->return_type.is_void())
				{
					error(newstatement->location, 3080, "function must return a value");

					accept(';');

					return false;
				}

				statement = newstatement;

				return expect(';');
			}
			#pragma endregion

			#pragma region Discard
			if (accept(lexer::tokenid::discard_))
			{
				const auto newstatement = _ast->make_node<nodes::return_statement_node>(_token.location);
				newstatement->attributes = attributes;
				newstatement->is_discard = true;

				statement = newstatement;

				return expect(';');
			}
			#pragma endregion

			#pragma region Declaration
			if (parse_statement_declarator_list(statement))
			{
				statement->attributes = attributes;

				return expect(';');
			}
			#pragma endregion

			#pragma region expression_node
			nodes::expression_node *expression = nullptr;

			if (parse_expression(expression))
			{
				const auto newstatement = _ast->make_node<nodes::expression_statement_node>(expression->location);
				newstatement->attributes = attributes;
				newstatement->expression = expression;

				statement = newstatement;

				return expect(';');
			}
			#pragma endregion

			error(_token_next.location, 3000, "syntax error: unexpected '%s'", get_token_name(_token_next.id));

			consume_until(';');

			return false;
		}
		bool parser::parse_statement_block(nodes::statement_node *&statement, bool scoped)
		{
			if (!expect('{'))
			{
				return false;
			}

			const auto compound = _ast->make_node<nodes::compound_statement_node>(_token.location);

			if (scoped)
			{
				enter_scope();
			}

			while (!peek('}') && !peek(lexer::tokenid::end_of_file))
			{
				nodes::statement_node *compound_statement = nullptr;

				if (!parse_statement(compound_statement))
				{
					if (scoped)
					{
						leave_scope();
					}

					unsigned level = 0;

					while (!peek(lexer::tokenid::end_of_file))
					{
						if (accept('{'))
						{
							++level;
						}
						else if (accept('}'))
						{
							if (level-- == 0)
							{
								break;
							}
						}
						else
						{
							consume();
						}
					}

					return false;
				}

				compound->statement_list.push_back(compound_statement);
			}

			if (scoped)
			{
				leave_scope();
			}

			statement = compound;

			return expect('}');
		}
		bool parser::parse_statement_declarator_list(nodes::statement_node *&statement)
		{
			nodes::type_node type;

			const auto location = _token_next.location;

			if (!parse_type(type))
			{
				return false;
			}

			unsigned int count = 0;
			const auto declarators = _ast->make_node<nodes::declarator_list_node>(location);

			do
			{
				if (count++ > 0 && !expect(','))
				{
					return false;
				}

				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				nodes::variable_declaration_node *declarator = nullptr;

				if (!parse_variable_residue(type, _token.literal_as_string, declarator))
				{
					return false;
				}

				declarators->declarator_list.push_back(std::move(declarator));
			}
			while (!peek(';'));

			statement = declarators;

			return true;
		}

		// Declarations
		bool parser::parse()
		{
			consume();

			while (!peek(lexer::tokenid::end_of_file))
			{
				if (!parse_top_level())
				{
					return false;
				}
			}

			return true;
		}
		bool parser::parse_top_level()
		{
			nodes::type_node type = { nodes::type_node::datatype_void };

			if (peek(lexer::tokenid::namespace_))
			{
				return parse_namespace();
			}
			else if (peek(lexer::tokenid::struct_))
			{
				nodes::struct_declaration_node *structure = nullptr;

				if (!parse_struct(structure))
				{
					return false;
				}

				if (!expect(';'))
				{
					return false;
				}
			}
			else if (peek(lexer::tokenid::technique))
			{
				nodes::technique_declaration_node *technique = nullptr;

				if (!parse_technique(technique))
				{
					return false;
				}

				_ast->techniques.push_back(std::move(technique));
			}
			else if (parse_type(type))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				if (peek('('))
				{
					nodes::function_declaration_node *function = nullptr;

					if (!parse_function_residue(type, _token.literal_as_string, function))
					{
						return false;
					}

					_ast->functions.push_back(std::move(function));
				}
				else
				{
					unsigned int count = 0;

					do
					{
						if (count++ > 0 && !(expect(',') && expect(lexer::tokenid::identifier)))
						{
							return false;
						}

						nodes::variable_declaration_node *variable = nullptr;

						if (!parse_variable_residue(type, _token.literal_as_string, variable, true))
						{
							consume_until(';');

							return false;
						}

						_ast->uniforms.push_back(std::move(variable));
					}
					while (!peek(';'));

					if (!expect(';'))
					{
						return false;
					}
				}
			}
			else if (!accept(';'))
			{
				consume();

				error(_token.location, 3000, "syntax error: unexpected '%s'", get_token_name(_token.id));

				return false;
			}

			return true;
		}
		bool parser::parse_namespace()
		{
			if (!accept(lexer::tokenid::namespace_))
			{
				return false;
			}

			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			const auto name = _token.literal_as_string;

			if (!expect('{'))
			{
				return false;
			}

			enter_namespace(name);

			bool success = true;

			while (!peek('}'))
			{
				if (!parse_top_level())
				{
					success = false;
					break;
				}
			}

			leave_namespace();

			return success && expect('}');
		}
		bool parser::parse_array(int &size)
		{
			size = 0;

			if (accept('['))
			{
				nodes::expression_node *expression;

				if (accept(']'))
				{
					size = -1;

					return true;
				}
				if (parse_expression(expression) && expect(']'))
				{
					if (expression->id != nodeid::literal_expression || !(expression->type.is_scalar() && expression->type.is_integral()))
					{
						error(expression->location, 3058, "array dimensions must be literal scalar expressions");

						return false;
					}

					size = static_cast<nodes::literal_expression_node *>(expression)->value_int[0];

					if (size < 1 || size > 65536)
					{
						error(expression->location, 3059, "array dimension must be between 1 and 65536");

						return false;
					}

					return true;
				}
			}

			return false;
		}
		bool parser::parse_annotations(std::vector<nodes::annotation_node> &annotations)
		{
			if (!accept('<'))
			{
				return false;
			}

			while (!peek('>'))
			{
				nodes::type_node type;

				accept_type_class(type);

				nodes::annotation_node annotation;

				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				annotation.name = _token.literal_as_string;
				annotation.location = _token.location;

				nodes::expression_node *expression = nullptr;

				if (!(expect('=') && parse_expression_assignment(expression) && expect(';')))
				{
					return false;
				}

				if (expression->id != nodeid::literal_expression)
				{
					error(expression->location, 3011, "value must be a literal expression");

					continue;
				}

				annotation.value = static_cast<nodes::literal_expression_node *>(expression);

				annotations.push_back(std::move(annotation));
			}

			return expect('>');
		}
		bool parser::parse_struct(nodes::struct_declaration_node *&structure)
		{
			if (!accept(lexer::tokenid::struct_))
			{
				return false;
			}

			structure = _ast->make_node<nodes::struct_declaration_node>(_token.location);
			structure->Namespace = _current_scope.name;

			if (accept(lexer::tokenid::identifier))
			{
				structure->name = _token.literal_as_string;

				if (!insert_symbol(structure, true))
				{
					error(_token.location, 3003, "redefinition of '%s'", structure->name.c_str());

					return false;
				}
			}
			else
			{
				structure->name = "__anonymous_struct_" + std::to_string(structure->location.line) + '_' + std::to_string(structure->location.column);
			}

			if (!expect('{'))
			{
				return false;
			}

			while (!peek('}'))
			{
				nodes::type_node type;

				if (!parse_type(type))
				{
					error(_token_next.location, 3000, "syntax error: unexpected '%s', expected struct member type", get_token_name(_token_next.id));

					consume_until('}');

					return false;
				}

				if (type.is_void())
				{
					error(_token_next.location, 3038, "struct members cannot be void");

					consume_until('}');

					return false;
				}
				if (type.has_qualifier(nodes::type_node::qualifier_in) || type.has_qualifier(nodes::type_node::qualifier_out))
				{
					error(_token_next.location, 3055, "struct members cannot be declared 'in' or 'out'");

					consume_until('}');

					return false;
				}

				unsigned int count = 0;

				do
				{
					if (count++ > 0 && !expect(','))
					{
						consume_until('}');

						return false;
					}

					if (!expect(lexer::tokenid::identifier))
					{
						consume_until('}');

						return false;
					}

					const auto field = _ast->make_node<nodes::variable_declaration_node>(_token.location);
					field->name = _token.literal_as_string;
					field->type = type;

					parse_array(field->type.array_length);

					if (accept(':'))
					{
						if (!expect(lexer::tokenid::identifier))
						{
							consume_until('}');

							return false;
						}

						field->semantic = _token.literal_as_string;
						boost::to_upper(field->semantic);
					}

					structure->field_list.push_back(std::move(field));
				}
				while (!peek(';'));

				if (!expect(';'))
				{
					consume_until('}');

					return false;
				}
			}

			if (structure->field_list.empty())
			{
				warning(structure->location, 5001, "struct has no members");
			}

			_ast->structs.push_back(structure);

			return expect('}');
		}
		bool parser::parse_function_residue(nodes::type_node &type, std::string name, nodes::function_declaration_node *&function)
		{
			const auto location = _token.location;

			if (!expect('('))
			{
				return false;
			}

			if (type.qualifiers != 0)
			{
				error(location, 3047, "function return type cannot have any qualifiers");

				return false;
			}

			function = _ast->make_node<nodes::function_declaration_node>(location);
			function->return_type = type;
			function->return_type.qualifiers = nodes::type_node::qualifier_const;
			function->name = name;
			function->Namespace = _current_scope.name;

			insert_symbol(function, true);

			enter_scope(function);

			while (!peek(')'))
			{
				if (!function->parameter_list.empty() && !expect(','))
				{
					leave_scope();

					return false;
				}

				const auto parameter = _ast->make_node<nodes::variable_declaration_node>(struct location());

				if (!parse_type(parameter->type))
				{
					leave_scope();

					error(_token_next.location, 3000, "syntax error: unexpected '%s', expected parameter type", get_token_name(_token_next.id));

					return false;
				}

				if (!expect(lexer::tokenid::identifier))
				{
					leave_scope();

					return false;
				}

				parameter->name = _token.literal_as_string;
				parameter->location = _token.location;

				if (parameter->type.is_void())
				{
					error(parameter->location, 3038, "function parameters cannot be void");

					leave_scope();

					return false;
				}
				if (parameter->type.has_qualifier(nodes::type_node::qualifier_extern))
				{
					error(parameter->location, 3006, "function parameters cannot be declared 'extern'");

					leave_scope();

					return false;
				}
				if (parameter->type.has_qualifier(nodes::type_node::qualifier_static))
				{
					error(parameter->location, 3007, "function parameters cannot be declared 'static'");

					leave_scope();

					return false;
				}
				if (parameter->type.has_qualifier(nodes::type_node::qualifier_uniform))
				{
					error(parameter->location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");

					leave_scope();

					return false;
				}

				if (parameter->type.has_qualifier(nodes::type_node::qualifier_out))
				{
					if (parameter->type.has_qualifier(nodes::type_node::qualifier_const))
					{
						error(parameter->location, 3046, "output parameters cannot be declared 'const'");

						leave_scope();

						return false;
					}
				}
				else
				{
					parameter->type.qualifiers |= nodes::type_node::qualifier_in;
				}

				parse_array(parameter->type.array_length);

				if (!insert_symbol(parameter))
				{
					error(parameter->location, 3003, "redefinition of '%s'", parameter->name.c_str());

					leave_scope();

					return false;
				}

				if (accept(':'))
				{
					if (!expect(lexer::tokenid::identifier))
					{
						leave_scope();

						return false;
					}

					parameter->semantic = _token.literal_as_string;
					boost::to_upper(parameter->semantic);
				}

				function->parameter_list.push_back(parameter);
			}

			if (!expect(')'))
			{
				leave_scope();

				return false;
			}

			if (accept(':'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					leave_scope();

					return false;
				}

				function->return_semantic = _token.literal_as_string;
				boost::to_upper(function->return_semantic);

				if (type.is_void())
				{
					error(_token.location, 3076, "void function cannot have a semantic");

					return false;
				}
			}

			if (!parse_statement_block(reinterpret_cast<nodes::statement_node *&>(function->definition)))
			{
				leave_scope();

				return false;
			}

			leave_scope();

			return true;
		}
		bool parser::parse_variable_residue(nodes::type_node &type, std::string name, nodes::variable_declaration_node *&variable, bool global)
		{
			auto location = _token.location;

			if (type.is_void())
			{
				error(location, 3038, "variables cannot be void");

				return false;
			}
			if (type.has_qualifier(nodes::type_node::qualifier_in) || type.has_qualifier(nodes::type_node::qualifier_out))
			{
				error(location, 3055, "variables cannot be declared 'in' or 'out'");

				return false;
			}

			const auto parent = _parent_stack.empty() ? nullptr : _parent_stack.top();

			if (parent == nullptr)
			{
				if (!type.has_qualifier(nodes::type_node::qualifier_static))
				{
					if (!type.has_qualifier(nodes::type_node::qualifier_uniform) && !(type.is_texture() || type.is_sampler()))
					{
						warning(location, 5000, "global variables are considered 'uniform' by default");
					}

					type.qualifiers |= nodes::type_node::qualifier_extern | nodes::type_node::qualifier_uniform;
				}
			}
			else
			{
				if (type.has_qualifier(nodes::type_node::qualifier_extern))
				{
					error(location, 3006, "local variables cannot be declared 'extern'");

					return false;
				}
				if (type.has_qualifier(nodes::type_node::qualifier_uniform))
				{
					error(location, 3047, "local variables cannot be declared 'uniform'");

					return false;
				}

				if (type.is_texture() || type.is_sampler())
				{
					error(location, 3038, "local variables cannot be textures or samplers");

					return false;
				}
			}

			parse_array(type.array_length);

			variable = _ast->make_node<nodes::variable_declaration_node>(location);
			variable->type = type;
			variable->name = name;

			if (global)
			{
				variable->Namespace = _current_scope.name;
			}

			if (!insert_symbol(variable, global))
			{
				error(location, 3003, "redefinition of '%s'", name.c_str());

				return false;
			}

			if (accept(':'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				variable->semantic = _token.literal_as_string;
				boost::to_upper(variable->semantic);
			}

			parse_annotations(variable->annotations);

			if (accept('='))
			{
				location = _token.location;

				if (!parse_variable_assignment(variable->initializer_expression))
				{
					return false;
				}

				if (parent == nullptr && variable->initializer_expression->id != nodeid::literal_expression)
				{
					error(location, 3011, "initial value must be a literal expression");

					return false;
				}

				if (variable->initializer_expression->id == nodeid::initializer_list && type.is_numeric())
				{
					const auto nullval = _ast->make_node<nodes::literal_expression_node>(location);
					nullval->type.basetype = type.basetype;
					nullval->type.qualifiers = nodes::type_node::qualifier_const;
					nullval->type.rows = type.rows, nullval->type.cols = type.cols, nullval->type.array_length = 0;

					const auto initializerlist = static_cast<nodes::initializer_list_node *>(variable->initializer_expression);

					while (initializerlist->type.array_length < type.array_length)
					{
						initializerlist->type.array_length++;
						initializerlist->values.push_back(nullval);
					}
				}

				if (!get_type_rank(variable->initializer_expression->type, type))
				{
					error(location, 3017, "initial value does not match variable type");

					return false;
				}
				if ((variable->initializer_expression->type.rows < type.rows || variable->initializer_expression->type.cols < type.cols) && !variable->initializer_expression->type.is_scalar())
				{
					error(location, 3017, "cannot implicitly convert these vector types");

					return false;
				}

				if (variable->initializer_expression->type.rows > type.rows || variable->initializer_expression->type.cols > type.cols)
				{
					warning(location, 3206, "implicit truncation of vector type");
				}
			}
			else if (type.is_numeric())
			{
				if (type.has_qualifier(nodes::type_node::qualifier_const))
				{
					error(location, 3012, "missing initial value for '%s'", name.c_str());

					return false;
				}
			}
			else if (peek('{'))
			{
				if (!parse_variable_properties(variable))
				{
					return false;
				}
			}

			return true;
		}
		bool parser::parse_variable_assignment(nodes::expression_node *&expression)
		{
			if (accept('{'))
			{
				const auto initializerlist = _ast->make_node<nodes::initializer_list_node>(_token.location);

				while (!peek('}'))
				{
					if (!initializerlist->values.empty() && !expect(','))
					{
						return false;
					}

					if (peek('}'))
					{
						break;
					}

					if (!parse_variable_assignment(expression))
					{
						consume_until('}');

						return false;
					}

					if (expression->id == nodeid::initializer_list && static_cast<nodes::initializer_list_node *>(expression)->values.empty())
					{
						continue;
					}

					initializerlist->values.push_back(expression);
				}

				if (!initializerlist->values.empty())
				{
					initializerlist->type = initializerlist->values[0]->type;
					initializerlist->type.array_length = static_cast<int>(initializerlist->values.size());
				}

				expression = initializerlist;

				return expect('}');
			}
			else if (parse_expression_assignment(expression))
			{
				return true;
			}

			return false;
		}
		bool parser::parse_variable_properties(nodes::variable_declaration_node *variable)
		{
			if (!expect('{'))
			{
				return false;
			}

			while (!peek('}'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				const auto name = _token.literal_as_string;
				const auto location = _token.location;

				nodes::expression_node *value = nullptr;

				if (!(expect('=') && parse_variable_properties_expression(value) && expect(';')))
				{
					return false;
				}

				if (name == "Texture")
				{
					if (value->id != nodeid::lvalue_expression || static_cast<nodes::lvalue_expression_node *>(value)->reference->id != nodeid::variable_declaration || !static_cast<nodes::lvalue_expression_node *>(value)->reference->type.is_texture() || static_cast<nodes::lvalue_expression_node *>(value)->reference->type.is_array())
					{
						error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					variable->properties.Texture = static_cast<nodes::lvalue_expression_node *>(value)->reference;
				}
				else
				{
					if (value->id != nodeid::literal_expression)
					{
						error(location, 3011, "value must be a literal expression");

						return false;
					}

					const auto valueLiteral = static_cast<nodes::literal_expression_node *>(value);

					if (name == "Width")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.Width);
					}
					else if (name == "Height")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.Height);
					}
					else if (name == "Depth")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.Depth);
					}
					else if (name == "MipLevels")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MipLevels);
					}
					else if (name == "Format")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.Format);
					}
					else if (name == "SRGBTexture" || name == "SRGBReadEnable")
					{
						variable->properties.SRGBTexture = valueLiteral->value_int[0] != 0;
					}
					else if (name == "AddressU")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.AddressU);
					}
					else if (name == "AddressV")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.AddressV);
					}
					else if (name == "AddressW")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.AddressW);
					}
					else if (name == "MinFilter")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MinFilter);
					}
					else if (name == "MagFilter")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MagFilter);
					}
					else if (name == "MipFilter")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MipFilter);
					}
					else if (name == "MaxAnisotropy")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MaxAnisotropy);
					}
					else if (name == "MinLOD" || name == "MaxMipLevel")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MinLOD);
					}
					else if (name == "MaxLOD")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MaxLOD);
					}
					else if (name == "MipLODBias" || name == "MipMapLodBias")
					{
						scalar_literal_cast(valueLiteral, 0, variable->properties.MipLODBias);
					}
					else
					{
						error(location, 3004, "unrecognized property '%s'", name.c_str());

						return false;
					}
				}
			}

			if (!expect('}'))
			{
				return false;
			}

			return true;
		}
		bool parser::parse_variable_properties_expression(nodes::expression_node *&expression)
		{
			backup();

			if (accept(lexer::tokenid::identifier))
			{
				const auto identifier = _token.literal_as_string;
				const auto location = _token.location;

				static const std::unordered_map<std::string, unsigned int> sEnums = boost::assign::map_list_of
					("NONE", nodes::variable_declaration_node::properties::NONE)
					("POINT", nodes::variable_declaration_node::properties::POINT)
					("LINEAR", nodes::variable_declaration_node::properties::LINEAR)
					("ANISOTROPIC", nodes::variable_declaration_node::properties::ANISOTROPIC)
					("CLAMP", nodes::variable_declaration_node::properties::CLAMP)
					("WRAP", nodes::variable_declaration_node::properties::REPEAT)
					("REPEAT", nodes::variable_declaration_node::properties::REPEAT)
					("MIRROR", nodes::variable_declaration_node::properties::MIRROR)
					("BORDER", nodes::variable_declaration_node::properties::BORDER)
					("R8", nodes::variable_declaration_node::properties::R8)
					("R16F", nodes::variable_declaration_node::properties::R16F)
					("R32F", nodes::variable_declaration_node::properties::R32F)
					("RG8", nodes::variable_declaration_node::properties::RG8)
					("R8G8", nodes::variable_declaration_node::properties::RG8)
					("RG16", nodes::variable_declaration_node::properties::RG16)
					("R16G16", nodes::variable_declaration_node::properties::RG16)
					("RG16F", nodes::variable_declaration_node::properties::RG16F)
					("R16G16F", nodes::variable_declaration_node::properties::RG16F)
					("RG32F", nodes::variable_declaration_node::properties::RG32F)
					("R32G32F", nodes::variable_declaration_node::properties::RG32F)
					("RGBA8", nodes::variable_declaration_node::properties::RGBA8)
					("R8G8B8A8", nodes::variable_declaration_node::properties::RGBA8)
					("RGBA16", nodes::variable_declaration_node::properties::RGBA16)
					("R16G16B16A16", nodes::variable_declaration_node::properties::RGBA16)
					("RGBA16F", nodes::variable_declaration_node::properties::RGBA16F)
					("R16G16B16A16F", nodes::variable_declaration_node::properties::RGBA16F)
					("RGBA32F", nodes::variable_declaration_node::properties::RGBA32F)
					("R32G32B32A32F", nodes::variable_declaration_node::properties::RGBA32F)
					("DXT1", nodes::variable_declaration_node::properties::DXT1)
					("DXT3", nodes::variable_declaration_node::properties::DXT3)
					("DXT4", nodes::variable_declaration_node::properties::DXT5)
					("LATC1", nodes::variable_declaration_node::properties::LATC1)
					("LATC2", nodes::variable_declaration_node::properties::LATC2);

				const auto it = sEnums.find(boost::to_upper_copy(identifier));

				if (it != sEnums.end())
				{
					const auto newexpression = _ast->make_node<nodes::literal_expression_node>(location);
					newexpression->type.basetype = nodes::type_node::datatype_uint;
					newexpression->type.rows = newexpression->type.cols = 1, newexpression->type.array_length = 0;
					newexpression->value_uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				restore();
			}

			return parse_expression_multary(expression);
		}
		bool parser::parse_technique(nodes::technique_declaration_node *&technique)
		{
			if (!accept(lexer::tokenid::technique))
			{
				return false;
			}

			const auto location = _token.location;

			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			technique = _ast->make_node<nodes::technique_declaration_node>(location);
			technique->name = _token.literal_as_string;
			technique->Namespace = _current_scope.name;

			parse_annotations(technique->annotation_list);

			if (!expect('{'))
			{
				return false;
			}

			while (!peek('}'))
			{
				nodes::pass_declaration_node *pass = nullptr;

				if (!parse_technique_pass(pass))
				{
					return false;
				}

				technique->pass_list.push_back(std::move(pass));
			}

			return expect('}');
		}
		bool parser::parse_technique_pass(nodes::pass_declaration_node *&pass)
		{
			if (!accept(lexer::tokenid::pass))
			{
				return false;
			}

			pass = _ast->make_node<nodes::pass_declaration_node>(_token.location);

			if (accept(lexer::tokenid::identifier))
			{
				pass->name = _token.literal_as_string;
			}

			parse_annotations(pass->annotation_list);

			if (!expect('{'))
			{
				return false;
			}

			while (!peek('}'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				const auto passstate = _token.literal_as_string;
				const auto location = _token.location;

				nodes::expression_node *value = nullptr;

				if (!(expect('=') && parse_technique_pass_expression(value) && expect(';')))
				{
					return false;
				}

				if (passstate == "VertexShader" || passstate == "PixelShader")
				{
					if (value->id != nodeid::lvalue_expression || static_cast<nodes::lvalue_expression_node *>(value)->reference->id != nodeid::function_declaration)
					{
						error(location, 3020, "type mismatch, expected function name");

						return false;
					}

					(passstate[0] == 'V' ? pass->states.VertexShader : pass->states.PixelShader) = reinterpret_cast<const nodes::function_declaration_node *>(static_cast<nodes::lvalue_expression_node *>(value)->reference);
				}
				else if (boost::starts_with(passstate, "RenderTarget") && (passstate == "RenderTarget" || (passstate[12] >= '0' && passstate[12] < '8')))
				{
					size_t index = 0;

					if (passstate.size() == 13)
					{
						index = passstate[12] - '0';
					}

					if (value->id != nodeid::lvalue_expression || static_cast<nodes::lvalue_expression_node *>(value)->reference->id != nodeid::variable_declaration || static_cast<nodes::lvalue_expression_node *>(value)->reference->type.basetype != nodes::type_node::datatype_texture || static_cast<nodes::lvalue_expression_node *>(value)->reference->type.is_array())
					{
						error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					pass->states.RenderTargets[index] = static_cast<nodes::lvalue_expression_node *>(value)->reference;
				}
				else
				{
					if (value->id != nodeid::literal_expression)
					{
						error(location, 3011, "pass state value must be a literal expression");

						return false;
					}

					const auto valueLiteral = static_cast<nodes::literal_expression_node *>(value);

					if (passstate == "SRGBWriteEnable")
					{
						pass->states.SRGBWriteEnable = valueLiteral->value_int[0] != 0;
					}
					else if (passstate == "BlendEnable" || passstate == "AlphaBlendEnable")
					{
						pass->states.BlendEnable = valueLiteral->value_int[0] != 0;
					}
					else if (passstate == "DepthEnable" || passstate == "ZEnable")
					{
						pass->states.DepthEnable = valueLiteral->value_int[0] != 0;
					}
					else if (passstate == "StencilEnable")
					{
						pass->states.StencilEnable = valueLiteral->value_int[0] != 0;
					}
					else if (passstate == "RenderTargetWriteMask" || passstate == "ColorWriteMask")
					{
						unsigned int mask = 0;
						scalar_literal_cast(valueLiteral, 0, mask);

						pass->states.RenderTargetWriteMask = mask & 0xFF;
					}
					else if (passstate == "DepthWriteMask" || passstate == "ZWriteEnable")
					{
						pass->states.DepthWriteMask = valueLiteral->value_int[0] != 0;
					}
					else if (passstate == "StencilReadMask" || passstate == "StencilMask")
					{
						unsigned int mask = 0;
						scalar_literal_cast(valueLiteral, 0, mask);

						pass->states.StencilReadMask = mask & 0xFF;
					}
					else if (passstate == "StencilWriteMask")
					{
						unsigned int mask = 0;
						scalar_literal_cast(valueLiteral, 0, mask);

						pass->states.StencilWriteMask = mask & 0xFF;
					}
					else if (passstate == "BlendOp")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.BlendOp);
					}
					else if (passstate == "BlendOpAlpha")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.BlendOpAlpha);
					}
					else if (passstate == "SrcBlend")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.SrcBlend);
					}
					else if (passstate == "DestBlend")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.DestBlend);
					}
					else if (passstate == "DepthFunc" || passstate == "ZFunc")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.DepthFunc);
					}
					else if (passstate == "StencilFunc")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.StencilFunc);
					}
					else if (passstate == "StencilRef")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.StencilRef);
					}
					else if (passstate == "StencilPass" || passstate == "StencilPassOp")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.StencilOpPass);
					}
					else if (passstate == "StencilFail" || passstate == "StencilFailOp")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.StencilOpFail);
					}
					else if (passstate == "StencilZFail" || passstate == "StencilDepthFail" || passstate == "StencilDepthFailOp")
					{
						scalar_literal_cast(valueLiteral, 0, pass->states.StencilOpDepthFail);
					}
					else
					{
						error(location, 3004, "unrecognized pass state '%s'", passstate.c_str());

						return false;
					}
				}
			}

			return expect('}');
		}
		bool parser::parse_technique_pass_expression(nodes::expression_node *&expression)
		{
			scope scope;
			bool exclusive;

			if (accept(lexer::tokenid::colon_colon))
			{
				scope.namespace_level = scope.level = 0;
				exclusive = true;
			}
			else
			{
				scope = _current_scope;
				exclusive = false;
			}

			if (exclusive ? expect(lexer::tokenid::identifier) : accept(lexer::tokenid::identifier))
			{
				auto identifier = _token.literal_as_string;
				const auto location = _token.location;

				static const std::unordered_map<std::string, unsigned int> sEnums = boost::assign::map_list_of
					("NONE", nodes::pass_declaration_node::states::NONE)
					("ZERO", nodes::pass_declaration_node::states::ZERO)
					("ONE", nodes::pass_declaration_node::states::ONE)
					("SRCCOLOR", nodes::pass_declaration_node::states::SRCCOLOR)
					("SRCALPHA", nodes::pass_declaration_node::states::SRCALPHA)
					("INVSRCCOLOR", nodes::pass_declaration_node::states::INVSRCCOLOR)
					("INVSRCALPHA", nodes::pass_declaration_node::states::INVSRCALPHA)
					("DESTCOLOR", nodes::pass_declaration_node::states::DESTCOLOR)
					("DESTALPHA", nodes::pass_declaration_node::states::DESTALPHA)
					("INVDESTCOLOR", nodes::pass_declaration_node::states::INVDESTCOLOR)
					("INVDESTALPHA", nodes::pass_declaration_node::states::INVDESTALPHA)
					("ADD", nodes::pass_declaration_node::states::ADD)
					("SUBTRACT", nodes::pass_declaration_node::states::SUBTRACT)
					("REVSUBTRACT", nodes::pass_declaration_node::states::REVSUBTRACT)
					("MIN", nodes::pass_declaration_node::states::MIN)
					("MAX", nodes::pass_declaration_node::states::MAX)
					("KEEP", nodes::pass_declaration_node::states::KEEP)
					("REPLACE", nodes::pass_declaration_node::states::REPLACE)
					("INVERT", nodes::pass_declaration_node::states::INVERT)
					("INCR", nodes::pass_declaration_node::states::INCR)
					("INCRSAT", nodes::pass_declaration_node::states::INCRSAT)
					("DECR", nodes::pass_declaration_node::states::DECR)
					("DECRSAT", nodes::pass_declaration_node::states::DECRSAT)
					("NEVER", nodes::pass_declaration_node::states::NEVER)
					("ALWAYS", nodes::pass_declaration_node::states::ALWAYS)
					("LESS", nodes::pass_declaration_node::states::LESS)
					("GREATER", nodes::pass_declaration_node::states::GREATER)
					("LEQUAL", nodes::pass_declaration_node::states::LESSEQUAL)
					("LESSEQUAL", nodes::pass_declaration_node::states::LESSEQUAL)
					("GEQUAL", nodes::pass_declaration_node::states::GREATEREQUAL)
					("GREATEREQUAL", nodes::pass_declaration_node::states::GREATEREQUAL)
					("EQUAL", nodes::pass_declaration_node::states::EQUAL)
					("NEQUAL", nodes::pass_declaration_node::states::NOTEQUAL)
					("NOTEQUAL", nodes::pass_declaration_node::states::NOTEQUAL);

				const auto it = sEnums.find(boost::to_upper_copy(identifier));

				if (it != sEnums.end())
				{
					const auto newexpression = _ast->make_node<nodes::literal_expression_node>(location);
					newexpression->type.basetype = nodes::type_node::datatype_uint;
					newexpression->type.rows = newexpression->type.cols = 1, newexpression->type.array_length = 0;
					newexpression->value_uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				while (accept(lexer::tokenid::colon_colon) && expect(lexer::tokenid::identifier))
				{
					identifier += "::" + _token.literal_as_string;
				}

				const auto symbol = find_symbol(identifier, scope, exclusive);

				if (symbol == nullptr)
				{
					error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

					return false;
				}

				const auto newexpression = _ast->make_node<nodes::lvalue_expression_node>(location);
				newexpression->reference = static_cast<const nodes::variable_declaration_node *>(symbol);
				newexpression->type = symbol->id == nodeid::function_declaration ? static_cast<const nodes::function_declaration_node *>(symbol)->return_type : newexpression->reference->type;

				expression = newexpression;

				return true;
			}

			return parse_expression_multary(expression);
		}

		// Symbol Table
		void parser::enter_scope(symbol *parent)
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
		void parser::enter_namespace(const std::string &name)
		{
			_current_scope.name += name + "::";
			_current_scope.level++;
			_current_scope.namespace_level++;
		}
		void parser::leave_scope()
		{
			assert(_current_scope.level > 0);

			for (auto it1 = _symbol_stack.begin(), end = _symbol_stack.end(); it1 != end; ++it1)
			{
				auto &scopes = it1->second;

				if (scopes.empty())
				{
					continue;
				}

				for (auto it2 = scopes.begin(); it2 != scopes.end();)
				{
					if (it2->first.level > it2->first.namespace_level && it2->first.level >= _current_scope.level)
					{
						it2 = scopes.erase(it2);
					}
					else
					{
						++it2;
					}
				}
			}

			_parent_stack.pop();

			_current_scope.level--;
		}
		void parser::leave_namespace()
		{
			assert(_current_scope.level > 0);
			assert(_current_scope.namespace_level > 0);

			_current_scope.name.erase(_current_scope.name.substr(0, _current_scope.name.size() - 2).rfind("::") + 2);
			_current_scope.level--;
			_current_scope.namespace_level--;
		}
		bool parser::insert_symbol(symbol *symbol, bool global)
		{
			if (symbol->id != nodeid::function_declaration && find_symbol(symbol->name, _current_scope, true))
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
		parser::symbol *parser::find_symbol(const std::string &name) const
		{
			return find_symbol(name, _current_scope, false);
		}
		parser::symbol *parser::find_symbol(const std::string &name, const scope &scope, bool exclusive) const
		{
			const auto it = _symbol_stack.find(name);

			if (it == _symbol_stack.end() || it->second.empty())
			{
				return nullptr;
			}

			symbol *result = nullptr;
			const auto &scopes = it->second;

			for (auto it2 = scopes.rbegin(), end = scopes.rend(); it2 != end; ++it2)
			{
				if (it2->first.level > scope.level || it2->first.namespace_level > scope.namespace_level || (it2->first.namespace_level == scope.namespace_level && it2->first.name != scope.name))
				{
					continue;
				}
				if (exclusive && it2->first.level < scope.level)
				{
					continue;
				}

				if (it2->second->id == nodeid::variable_declaration || it2->second->id == nodeid::struct_declaration)
				{
					return it2->second;
				}
				if (result == nullptr)
				{
					result = it2->second;
				}
			}

			return result;
		}
		bool parser::resolve_call(nodes::call_expression_node *call, const scope &scope, bool &is_intrinsic, bool &is_ambiguous) const
		{
			is_intrinsic = false;
			is_ambiguous = false;

			unsigned int overload_count = 0, overload_namespace = scope.namespace_level;
			const nodes::function_declaration_node *overload = nullptr;
			auto intrinsic_op = nodes::intrinsic_expression_node::none;

			const auto it = _symbol_stack.find(call->callee_name);

			if (it != _symbol_stack.end() && !it->second.empty())
			{
				const auto &scopes = it->second;

				for (auto it2 = scopes.rbegin(), end = scopes.rend(); it2 != end; ++it2)
				{
					if (it2->first.level > scope.level || it2->first.namespace_level > scope.namespace_level || it2->second->id != nodeid::function_declaration)
					{
						continue;
					}

					const auto function = static_cast<nodes::function_declaration_node *>(it2->second);

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
						overload_namespace = it2->first.namespace_level;
					}
					else if (comparison == 0 && overload_namespace == it2->first.namespace_level)
					{
						++overload_count;
					}
				}
			}

			if (overload_count == 0)
			{
				for (auto &intrinsic : _intrinsics)
				{
					if (intrinsic.function.name == call->callee_name)
					{
						if (call->arguments.size() != intrinsic.function.parameter_list.size())
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
			}

			if (overload_count == 1)
			{
				call->type = overload->return_type;

				if (is_intrinsic)
				{
					call->callee = reinterpret_cast<nodes::function_declaration_node *>(static_cast<unsigned int>(intrinsic_op));
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
		nodes::expression_node *parser::fold_constant_expression(nodes::expression_node *expression) const
		{
			#pragma region Helpers
	#define DOFOLDING1(op) \
			for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; ++i) \
				switch (operand->type.basetype) \
				{ \
					case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
						switch (expression->type.basetype) \
						{ \
							case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
								operand->value_int[i] = static_cast<int>(op(operand->value_int[i])); break; \
							case nodes::type_node::datatype_float: \
								operand->value_float[i] = static_cast<float>(op(operand->value_int[i])); break; \
						} \
						break; \
					case nodes::type_node::datatype_float: \
						switch (expression->type.basetype) \
						{ \
							case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
								operand->value_int[i] = static_cast<int>(op(operand->value_float[i])); break; \
							case nodes::type_node::datatype_float: \
								operand->value_float[i] = static_cast<float>(op(operand->value_float[i])); break; \
						} \
						break; \
				} \
			expression = operand;

	#define DOFOLDING2(op) { \
			nodes::literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case nodes::type_node::datatype_bool:  case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
						switch (right->type.basetype) \
						{ \
							case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
								result.value_int[i] = left->value_int[leftScalar ? 0 : i] op right->value_int[rightScalar ? 0 : i]; \
								break; \
							case nodes::type_node::datatype_float: \
								result.value_float[i] = static_cast<float>(left->value_int[!leftScalar * i]) op right->value_float[!rightScalar * i]; \
								break; \
						} \
						break; \
					case nodes::type_node::datatype_float: \
						result.value_float[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (left->value_float[!leftScalar * i] op right->value_float[!rightScalar * i]) : (left->value_float[!leftScalar * i] op static_cast<float>(right->value_int[!rightScalar * i])); \
						break; \
				} \
			left->type = expression->type; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; }
	#define DOFOLDING2_INT(op) { \
			nodes::literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
			{ \
				result.value_int[i] = left->value_int[!leftScalar * i] op right->value_int[!rightScalar * i]; \
			} \
			left->type = expression->type; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; }
	#define DOFOLDING2_BOOL(op) { \
			nodes::literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
						result.value_int[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (static_cast<float>(left->value_int[!leftScalar * i]) op right->value_float[!rightScalar * i]) : (left->value_int[!leftScalar * i] op right->value_int[!rightScalar * i]); \
						break; \
					case nodes::type_node::datatype_float: \
						result.value_int[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (left->value_float[!leftScalar * i] op static_cast<float>(right->value_int[!rightScalar * i])) : (left->value_float[!leftScalar * i] op right->value_float[!rightScalar * i]); \
						break; \
				} \
			left->type = expression->type; \
			left->type.basetype = nodes::type_node::datatype_bool; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; }
	#define DOFOLDING2_FLOAT(op) { \
			nodes::literal_expression_node result; \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case nodes::type_node::datatype_bool:  case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
						result.value_float[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (static_cast<float>(left->value_int[!leftScalar * i]) op right->value_float[!rightScalar * i]) : (left->value_int[leftScalar ? 0 : i] op right->value_int[rightScalar ? 0 : i]); \
						break; \
					case nodes::type_node::datatype_float: \
						result.value_float[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (left->value_float[!leftScalar * i] op right->value_float[!rightScalar * i]) : (left->value_float[!leftScalar * i] op static_cast<float>(right->value_int[!rightScalar * i])); \
						break; \
				} \
			left->type = expression->type; \
			left->type.basetype = nodes::type_node::datatype_float; \
			memcpy(left->value_uint, result.value_uint, sizeof(result.value_uint)); \
			expression = left; }

	#define DOFOLDING2_FUNCTION(op) \
			for (unsigned int i = 0; i < expression->type.rows * expression->type.cols; ++i) \
				switch (left->type.basetype) \
				{ \
					case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
						switch (right->type.basetype) \
						{ \
							case nodes::type_node::datatype_bool: case nodes::type_node::datatype_int: case nodes::type_node::datatype_uint: \
								left->value_int[i] = static_cast<int>(op(left->value_int[i], right->value_int[i])); \
								break; \
							case nodes::type_node::datatype_float: \
								left->value_float[i] = static_cast<float>(op(static_cast<float>(left->value_int[i]), right->value_float[i])); \
								break; \
						} \
						break; \
					case nodes::type_node::datatype_float: \
						left->value_float[i] = (right->type.basetype == nodes::type_node::datatype_float) ? (static_cast<float>(op(left->value_float[i], right->value_float[i]))) : (static_cast<float>(op(left->value_float[i], static_cast<float>(right->value_int[i])))); \
						break; \
				} \
			left->type = expression->type; \
			expression = left;
			#pragma endregion

			if (expression->id == nodeid::unary_expression)
			{
				const auto unaryexpression = static_cast<nodes::unary_expression_node *>(expression);

				if (unaryexpression->operand->id != nodeid::literal_expression)
				{
					return expression;
				}

				const auto operand = static_cast<nodes::literal_expression_node *>(unaryexpression->operand);

				switch (unaryexpression->op)
				{
					case nodes::unary_expression_node::negate:
						DOFOLDING1(-);
						break;
					case nodes::unary_expression_node::bitwise_not:
						for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; ++i)
						{
							operand->value_int[i] = ~operand->value_int[i];
						}
						expression = operand;
						break;
					case nodes::unary_expression_node::logical_not:
						for (unsigned int i = 0; i < operand->type.rows * operand->type.cols; ++i)
						{
							operand->value_int[i] = (operand->type.basetype == nodes::type_node::datatype_float) ? !operand->value_float[i] : !operand->value_int[i];
						}
						operand->type.basetype = nodes::type_node::datatype_bool;
						expression = operand;
						break;
					case nodes::unary_expression_node::cast:
					{
						nodes::literal_expression_node old = *operand;
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
				const auto binaryexpression = static_cast<nodes::binary_expression_node *>(expression);

				if (binaryexpression->operands[0]->id != nodeid::literal_expression || binaryexpression->operands[1]->id != nodeid::literal_expression)
				{
					return expression;
				}

				const auto left = static_cast<nodes::literal_expression_node *>(binaryexpression->operands[0]);
				const auto right = static_cast<nodes::literal_expression_node *>(binaryexpression->operands[1]);
				const bool leftScalar = left->type.rows * left->type.cols == 1;
				const bool rightScalar = right->type.rows * right->type.cols == 1;

				switch (binaryexpression->op)
				{
					case nodes::binary_expression_node::add:
						DOFOLDING2(+);
						break;
					case nodes::binary_expression_node::subtract:
						DOFOLDING2(-);
						break;
					case nodes::binary_expression_node::multiply:
						DOFOLDING2(*);
						break;
					case nodes::binary_expression_node::divide:
						DOFOLDING2_FLOAT(/);
						break;
					case nodes::binary_expression_node::modulo:
						DOFOLDING2_FUNCTION(std::fmod);
						break;
					case nodes::binary_expression_node::less:
						DOFOLDING2_BOOL(<);
						break;
					case nodes::binary_expression_node::greater:
						DOFOLDING2_BOOL(>);
						break;
					case nodes::binary_expression_node::less_equal:
						DOFOLDING2_BOOL(<=);
						break;
					case nodes::binary_expression_node::greater_equal:
						DOFOLDING2_BOOL(>=);
						break;
					case nodes::binary_expression_node::equal:
						DOFOLDING2_BOOL(==);
						break;
					case nodes::binary_expression_node::not_equal:
						DOFOLDING2_BOOL(!=);
						break;
					case nodes::binary_expression_node::left_shift:
						DOFOLDING2_INT(<<);
						break;
					case nodes::binary_expression_node::right_shift:
						DOFOLDING2_INT(>>);
						break;
					case nodes::binary_expression_node::bitwise_and:
						DOFOLDING2_INT(&);
						break;
					case nodes::binary_expression_node::bitwise_or:
						DOFOLDING2_INT(|);
						break;
					case nodes::binary_expression_node::bitwise_xor:
						DOFOLDING2_INT(^);
						break;
					case nodes::binary_expression_node::logical_and:
						DOFOLDING2_BOOL(&&);
						break;
					case nodes::binary_expression_node::logical_or:
						DOFOLDING2_BOOL(||);
						break;
				}
			}
			else if (expression->id == nodeid::intrinsic_expression)
			{
				const auto intrinsicexpression = static_cast<nodes::intrinsic_expression_node *>(expression);

				if ((intrinsicexpression->arguments[0] != nullptr && intrinsicexpression->arguments[0]->id != nodeid::literal_expression) || (intrinsicexpression->arguments[1] != nullptr && intrinsicexpression->arguments[1]->id != nodeid::literal_expression) || (intrinsicexpression->arguments[2] != nullptr && intrinsicexpression->arguments[2]->id != nodeid::literal_expression))
				{
					return expression;
				}

				const auto operand = static_cast<nodes::literal_expression_node *>(intrinsicexpression->arguments[0]);
				const auto left = operand;
				const auto right = static_cast<nodes::literal_expression_node *>(intrinsicexpression->arguments[1]);

				switch (intrinsicexpression->op)
				{
					case nodes::intrinsic_expression_node::abs:
						DOFOLDING1(std::abs);
						break;
					case nodes::intrinsic_expression_node::sin:
						DOFOLDING1(std::sin);
						break;
					case nodes::intrinsic_expression_node::sinh:
						DOFOLDING1(std::sinh);
						break;
					case nodes::intrinsic_expression_node::cos:
						DOFOLDING1(std::cos);
						break;
					case nodes::intrinsic_expression_node::cosh:
						DOFOLDING1(std::cosh);
						break;
					case nodes::intrinsic_expression_node::tan:
						DOFOLDING1(std::tan);
						break;
					case nodes::intrinsic_expression_node::tanh:
						DOFOLDING1(std::tanh);
						break;
					case nodes::intrinsic_expression_node::asin:
						DOFOLDING1(std::asin);
						break;
					case nodes::intrinsic_expression_node::acos:
						DOFOLDING1(std::acos);
						break;
					case nodes::intrinsic_expression_node::atan:
						DOFOLDING1(std::atan);
						break;
					case nodes::intrinsic_expression_node::exp:
						DOFOLDING1(std::exp);
						break;
					case nodes::intrinsic_expression_node::log:
						DOFOLDING1(std::log);
						break;
					case nodes::intrinsic_expression_node::log10:
						DOFOLDING1(std::log10);
						break;
					case nodes::intrinsic_expression_node::sqrt:
						DOFOLDING1(std::sqrt);
						break;
					case nodes::intrinsic_expression_node::ceil:
						DOFOLDING1(std::ceil);
						break;
					case nodes::intrinsic_expression_node::floor:
						DOFOLDING1(std::floor);
						break;
					case nodes::intrinsic_expression_node::atan2:
						DOFOLDING2_FUNCTION(std::atan2);
						break;
					case nodes::intrinsic_expression_node::pow:
						DOFOLDING2_FUNCTION(std::pow);
						break;
					case nodes::intrinsic_expression_node::min:
						DOFOLDING2_FUNCTION(std::min);
						break;
					case nodes::intrinsic_expression_node::max:
						DOFOLDING2_FUNCTION(std::max);
						break;
				}
			}
			else if (expression->id == nodeid::constructor_expression)
			{
				const auto constructor = static_cast<nodes::constructor_expression_node *>(expression);

				for (auto argument : constructor->arguments)
				{
					if (argument->id != nodeid::literal_expression)
					{
						return expression;
					}
				}

				unsigned int k = 0;
				const auto literal = _ast->make_node<nodes::literal_expression_node>(constructor->location);
				literal->type = constructor->type;

				for (auto argument : constructor->arguments)
				{
					for (unsigned int j = 0; j < argument->type.rows * argument->type.cols; ++k, ++j)
					{
						vector_literal_cast(static_cast<nodes::literal_expression_node *>(argument), k, literal, j);
					}
				}

				expression = literal;
			}
			else if (expression->id == nodeid::lvalue_expression)
			{
				const auto variable = static_cast<nodes::lvalue_expression_node *>(expression)->reference;

				if (variable->initializer_expression == nullptr || !(variable->initializer_expression->id == nodeid::literal_expression && variable->type.has_qualifier(nodes::type_node::qualifier_const)))
				{
					return expression;
				}

				const auto literal = _ast->make_node<nodes::literal_expression_node>(expression->location);
				literal->type = expression->type;
				memcpy(literal->value_uint, static_cast<const nodes::literal_expression_node *>(variable->initializer_expression)->value_uint, sizeof(literal->value_uint));

				expression = literal;
			}

			return expression;
		}
	}
}
