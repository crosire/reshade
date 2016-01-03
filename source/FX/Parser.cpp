#include "Parser.hpp"

#include <cstdarg>
#include <algorithm>
#include <functional>
#include <stack>
#include <unordered_map>
#include <boost\assign\list_of.hpp>
#include <boost\algorithm\string.hpp>

namespace ReShade
{
	namespace FX
	{
		namespace
		{
			#pragma region Intrinsics
			struct Intrinsic
			{
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols)
				{
					Op = op;
					Function.Name = name;
					Function.ReturnType.BaseClass = returntype;
					Function.ReturnType.Rows = returnrows;
					Function.ReturnType.Cols = returncols;
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols)
				{
					Op = op;
					Function.Name = name;
					Function.ReturnType.BaseClass = returntype;
					Function.ReturnType.Rows = returnrows;
					Function.ReturnType.Cols = returncols;
					Arguments[0].Type.BaseClass = arg0type;
					Arguments[0].Type.Rows = arg0rows;
					Arguments[0].Type.Cols = arg0cols;

					Function.Parameters.push_back(&Arguments[0]);
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols, Nodes::Type::Class arg1type, unsigned int arg1rows, unsigned int arg1cols)
				{
					Op = op;
					Function.Name = name;
					Function.ReturnType.BaseClass = returntype;
					Function.ReturnType.Rows = returnrows;
					Function.ReturnType.Cols = returncols;
					Arguments[0].Type.BaseClass = arg0type;
					Arguments[0].Type.Rows = arg0rows;
					Arguments[0].Type.Cols = arg0cols;
					Arguments[1].Type.BaseClass = arg1type;
					Arguments[1].Type.Rows = arg1rows;
					Arguments[1].Type.Cols = arg1cols;

					Function.Parameters.push_back(&Arguments[0]);
					Function.Parameters.push_back(&Arguments[1]);
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols, Nodes::Type::Class arg1type, unsigned int arg1rows, unsigned int arg1cols, Nodes::Type::Class arg2type, unsigned int arg2rows, unsigned int arg2cols)
				{
					Op = op;
					Function.Name = name;
					Function.ReturnType.BaseClass = returntype;
					Function.ReturnType.Rows = returnrows;
					Function.ReturnType.Cols = returncols;
					Arguments[0].Type.BaseClass = arg0type;
					Arguments[0].Type.Rows = arg0rows;
					Arguments[0].Type.Cols = arg0cols;
					Arguments[1].Type.BaseClass = arg1type;
					Arguments[1].Type.Rows = arg1rows;
					Arguments[1].Type.Cols = arg1cols;
					Arguments[2].Type.BaseClass = arg2type;
					Arguments[2].Type.Rows = arg2rows;
					Arguments[2].Type.Cols = arg2cols;

					Function.Parameters.push_back(&Arguments[0]);
					Function.Parameters.push_back(&Arguments[1]);
					Function.Parameters.push_back(&Arguments[2]);
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols, Nodes::Type::Class arg1type, unsigned int arg1rows, unsigned int arg1cols, Nodes::Type::Class arg2type, unsigned int arg2rows, unsigned int arg2cols, Nodes::Type::Class arg3type, unsigned int arg3rows, unsigned int arg3cols)
				{
					Op = op;
					Function.Name = name;
					Function.ReturnType.BaseClass = returntype;
					Function.ReturnType.Rows = returnrows;
					Function.ReturnType.Cols = returncols;
					Arguments[0].Type.BaseClass = arg0type;
					Arguments[0].Type.Rows = arg0rows;
					Arguments[0].Type.Cols = arg0cols;
					Arguments[1].Type.BaseClass = arg1type;
					Arguments[1].Type.Rows = arg1rows;
					Arguments[1].Type.Cols = arg1cols;
					Arguments[2].Type.BaseClass = arg2type;
					Arguments[2].Type.Rows = arg2rows;
					Arguments[2].Type.Cols = arg2cols;
					Arguments[3].Type.BaseClass = arg3type;
					Arguments[3].Type.Rows = arg3rows;
					Arguments[3].Type.Cols = arg3cols;

					Function.Parameters.push_back(&Arguments[0]);
					Function.Parameters.push_back(&Arguments[1]);
					Function.Parameters.push_back(&Arguments[2]);
					Function.Parameters.push_back(&Arguments[3]);
				}

				Nodes::Intrinsic::Op Op;
				Nodes::Function Function;
				Nodes::Variable Arguments[4];
			};

			const Intrinsic sIntrinsics[] =
			{
				Intrinsic("abs", 				Nodes::Intrinsic::Op::Abs, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("abs", 				Nodes::Intrinsic::Op::Abs, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("abs", 				Nodes::Intrinsic::Op::Abs, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("abs", 				Nodes::Intrinsic::Op::Abs, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("acos", 				Nodes::Intrinsic::Op::Acos, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("acos", 				Nodes::Intrinsic::Op::Acos, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("acos", 				Nodes::Intrinsic::Op::Acos, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("acos", 				Nodes::Intrinsic::Op::Acos, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("all", 				Nodes::Intrinsic::Op::All, 					Nodes::Type::Class::Bool,  1, 1, Nodes::Type::Class::Bool,  1, 1),
				Intrinsic("all", 				Nodes::Intrinsic::Op::All, 					Nodes::Type::Class::Bool,  2, 1, Nodes::Type::Class::Bool,  2, 1),
				Intrinsic("all", 				Nodes::Intrinsic::Op::All, 					Nodes::Type::Class::Bool,  3, 1, Nodes::Type::Class::Bool,  3, 1),
				Intrinsic("all", 				Nodes::Intrinsic::Op::All, 					Nodes::Type::Class::Bool,  4, 1, Nodes::Type::Class::Bool,  4, 1),
				Intrinsic("any", 				Nodes::Intrinsic::Op::Any, 					Nodes::Type::Class::Bool,  1, 1, Nodes::Type::Class::Bool,  1, 1),
				Intrinsic("any", 				Nodes::Intrinsic::Op::Any, 					Nodes::Type::Class::Bool,  2, 1, Nodes::Type::Class::Bool,  2, 1),
				Intrinsic("any", 				Nodes::Intrinsic::Op::Any, 					Nodes::Type::Class::Bool,  3, 1, Nodes::Type::Class::Bool,  3, 1),
				Intrinsic("any", 				Nodes::Intrinsic::Op::Any, 					Nodes::Type::Class::Bool,  4, 1, Nodes::Type::Class::Bool,  4, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastInt2Float,		Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Int,   1, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastInt2Float,		Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int,   2, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastInt2Float,		Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Int,   3, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastInt2Float,		Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Int,   4, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastUint2Float,	Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Uint,  1, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastUint2Float,	Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Uint,  2, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastUint2Float,	Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Uint,  3, 1),
				Intrinsic("asfloat", 			Nodes::Intrinsic::Op::BitCastUint2Float,	Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Uint,  4, 1),
				Intrinsic("asin", 				Nodes::Intrinsic::Op::Asin, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("asin", 				Nodes::Intrinsic::Op::Asin, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("asin", 				Nodes::Intrinsic::Op::Asin, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("asin", 				Nodes::Intrinsic::Op::Asin, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("asint", 				Nodes::Intrinsic::Op::BitCastFloat2Int,		Nodes::Type::Class::Int,   1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("asint", 				Nodes::Intrinsic::Op::BitCastFloat2Int,		Nodes::Type::Class::Int,   2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("asint", 				Nodes::Intrinsic::Op::BitCastFloat2Int,		Nodes::Type::Class::Int,   3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("asint", 				Nodes::Intrinsic::Op::BitCastFloat2Int,		Nodes::Type::Class::Int,   4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("asuint", 			Nodes::Intrinsic::Op::BitCastFloat2Uint,	Nodes::Type::Class::Uint,  1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("asuint", 			Nodes::Intrinsic::Op::BitCastFloat2Uint,	Nodes::Type::Class::Uint,  2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("asuint", 			Nodes::Intrinsic::Op::BitCastFloat2Uint,	Nodes::Type::Class::Uint,  3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("asuint", 			Nodes::Intrinsic::Op::BitCastFloat2Uint,	Nodes::Type::Class::Uint,  4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("atan", 				Nodes::Intrinsic::Op::Atan, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("atan", 				Nodes::Intrinsic::Op::Atan, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("atan", 				Nodes::Intrinsic::Op::Atan, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("atan", 				Nodes::Intrinsic::Op::Atan, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("atan2", 				Nodes::Intrinsic::Op::Atan2, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("atan2", 				Nodes::Intrinsic::Op::Atan2, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("atan2", 				Nodes::Intrinsic::Op::Atan2, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("atan2", 				Nodes::Intrinsic::Op::Atan2, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("ceil", 				Nodes::Intrinsic::Op::Ceil, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("ceil", 				Nodes::Intrinsic::Op::Ceil, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("ceil", 				Nodes::Intrinsic::Op::Ceil, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("ceil", 				Nodes::Intrinsic::Op::Ceil, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("clamp", 				Nodes::Intrinsic::Op::Clamp, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("clamp", 				Nodes::Intrinsic::Op::Clamp, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("clamp", 				Nodes::Intrinsic::Op::Clamp, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("clamp", 				Nodes::Intrinsic::Op::Clamp, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("cos", 				Nodes::Intrinsic::Op::Cos, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("cos", 				Nodes::Intrinsic::Op::Cos, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("cos", 				Nodes::Intrinsic::Op::Cos, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("cos", 				Nodes::Intrinsic::Op::Cos, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("cosh", 				Nodes::Intrinsic::Op::Cosh, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("cosh", 				Nodes::Intrinsic::Op::Cosh, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("cosh", 				Nodes::Intrinsic::Op::Cosh, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("cosh", 				Nodes::Intrinsic::Op::Cosh, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("cross", 				Nodes::Intrinsic::Op::Cross, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("ddx", 				Nodes::Intrinsic::Op::PartialDerivativeX,	Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("ddx", 				Nodes::Intrinsic::Op::PartialDerivativeX,	Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("ddx", 				Nodes::Intrinsic::Op::PartialDerivativeX,	Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("ddx", 				Nodes::Intrinsic::Op::PartialDerivativeX,	Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("ddy", 				Nodes::Intrinsic::Op::PartialDerivativeY,	Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("ddy", 				Nodes::Intrinsic::Op::PartialDerivativeY,	Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("ddy", 				Nodes::Intrinsic::Op::PartialDerivativeY,	Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("ddy", 				Nodes::Intrinsic::Op::PartialDerivativeY,	Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("degrees", 			Nodes::Intrinsic::Op::Degrees, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("degrees", 			Nodes::Intrinsic::Op::Degrees, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("degrees", 			Nodes::Intrinsic::Op::Degrees, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("degrees", 			Nodes::Intrinsic::Op::Degrees, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("determinant",		Nodes::Intrinsic::Op::Determinant,			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 2),
				Intrinsic("determinant",		Nodes::Intrinsic::Op::Determinant,			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 3),
				Intrinsic("determinant", 		Nodes::Intrinsic::Op::Determinant,			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 4),
				Intrinsic("distance", 			Nodes::Intrinsic::Op::Distance,				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("distance", 			Nodes::Intrinsic::Op::Distance,				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("distance", 			Nodes::Intrinsic::Op::Distance, 			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("distance", 			Nodes::Intrinsic::Op::Distance, 			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("dot", 				Nodes::Intrinsic::Op::Dot, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("dot", 				Nodes::Intrinsic::Op::Dot, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("dot", 				Nodes::Intrinsic::Op::Dot, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("dot", 				Nodes::Intrinsic::Op::Dot, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("exp", 				Nodes::Intrinsic::Op::Exp, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("exp", 				Nodes::Intrinsic::Op::Exp, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("exp", 				Nodes::Intrinsic::Op::Exp, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("exp", 				Nodes::Intrinsic::Op::Exp, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("exp2", 				Nodes::Intrinsic::Op::Exp2, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("exp2", 				Nodes::Intrinsic::Op::Exp2, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("exp2", 				Nodes::Intrinsic::Op::Exp2, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("exp2", 				Nodes::Intrinsic::Op::Exp2, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("faceforward",		Nodes::Intrinsic::Op::FaceForward,			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("faceforward",		Nodes::Intrinsic::Op::FaceForward,			Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("faceforward",		Nodes::Intrinsic::Op::FaceForward,			Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("faceforward",		Nodes::Intrinsic::Op::FaceForward, 			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("floor", 				Nodes::Intrinsic::Op::Floor, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("floor", 				Nodes::Intrinsic::Op::Floor, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("floor", 				Nodes::Intrinsic::Op::Floor, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("floor", 				Nodes::Intrinsic::Op::Floor, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("frac", 				Nodes::Intrinsic::Op::Frac, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("frac", 				Nodes::Intrinsic::Op::Frac, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("frac", 				Nodes::Intrinsic::Op::Frac, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("frac", 				Nodes::Intrinsic::Op::Frac, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("frexp", 				Nodes::Intrinsic::Op::Frexp, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("frexp", 				Nodes::Intrinsic::Op::Frexp, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("frexp", 				Nodes::Intrinsic::Op::Frexp, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("frexp", 				Nodes::Intrinsic::Op::Frexp, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("fwidth", 			Nodes::Intrinsic::Op::Fwidth, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("fwidth", 			Nodes::Intrinsic::Op::Fwidth, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("fwidth", 			Nodes::Intrinsic::Op::Fwidth, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("fwidth", 			Nodes::Intrinsic::Op::Fwidth, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("ldexp", 				Nodes::Intrinsic::Op::Ldexp, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("ldexp", 				Nodes::Intrinsic::Op::Ldexp, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("ldexp", 				Nodes::Intrinsic::Op::Ldexp, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("ldexp", 				Nodes::Intrinsic::Op::Ldexp, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("length", 			Nodes::Intrinsic::Op::Length, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("length", 			Nodes::Intrinsic::Op::Length, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("length", 			Nodes::Intrinsic::Op::Length, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("length", 			Nodes::Intrinsic::Op::Length, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("lerp",				Nodes::Intrinsic::Op::Lerp, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("lerp",				Nodes::Intrinsic::Op::Lerp, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("lerp",				Nodes::Intrinsic::Op::Lerp, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("lerp",				Nodes::Intrinsic::Op::Lerp, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("log", 				Nodes::Intrinsic::Op::Log, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("log", 				Nodes::Intrinsic::Op::Log, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("log", 				Nodes::Intrinsic::Op::Log, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("log", 				Nodes::Intrinsic::Op::Log, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("log10", 				Nodes::Intrinsic::Op::Log10, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("log10", 				Nodes::Intrinsic::Op::Log10, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("log10", 				Nodes::Intrinsic::Op::Log10, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("log10", 				Nodes::Intrinsic::Op::Log10, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("log2", 				Nodes::Intrinsic::Op::Log2, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("log2", 				Nodes::Intrinsic::Op::Log2, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("log2", 				Nodes::Intrinsic::Op::Log2, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("log2", 				Nodes::Intrinsic::Op::Log2, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("mad", 				Nodes::Intrinsic::Op::Mad, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mad", 				Nodes::Intrinsic::Op::Mad, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("mad", 				Nodes::Intrinsic::Op::Mad, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("mad", 				Nodes::Intrinsic::Op::Mad, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("max", 				Nodes::Intrinsic::Op::Max, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("max", 				Nodes::Intrinsic::Op::Max, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("max",				Nodes::Intrinsic::Op::Max, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("max", 				Nodes::Intrinsic::Op::Max, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("min", 				Nodes::Intrinsic::Op::Min, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("min", 				Nodes::Intrinsic::Op::Min, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("min", 				Nodes::Intrinsic::Op::Min, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("min", 				Nodes::Intrinsic::Op::Min, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("modf", 				Nodes::Intrinsic::Op::Modf, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("modf", 				Nodes::Intrinsic::Op::Modf, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("modf", 				Nodes::Intrinsic::Op::Modf, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("modf", 				Nodes::Intrinsic::Op::Modf, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 2, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 2, 2),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 3, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 3, 3),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 4, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 4, 4),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 2, Nodes::Type::Class::Float, 2, 2, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 3, Nodes::Type::Class::Float, 3, 3, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 4, Nodes::Type::Class::Float, 4, 4, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 2),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 3),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 4),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 2, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 3, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("mul", 				Nodes::Intrinsic::Op::Mul, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 4, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("normalize", 			Nodes::Intrinsic::Op::Normalize, 			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("normalize", 			Nodes::Intrinsic::Op::Normalize, 			Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("normalize", 			Nodes::Intrinsic::Op::Normalize, 			Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("normalize", 			Nodes::Intrinsic::Op::Normalize, 			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("pow", 				Nodes::Intrinsic::Op::Pow, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("pow", 				Nodes::Intrinsic::Op::Pow, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("pow", 				Nodes::Intrinsic::Op::Pow, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("pow", 				Nodes::Intrinsic::Op::Pow, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("radians", 			Nodes::Intrinsic::Op::Radians, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("radians", 			Nodes::Intrinsic::Op::Radians, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("radians", 			Nodes::Intrinsic::Op::Radians, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("radians", 			Nodes::Intrinsic::Op::Radians, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("rcp", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("rcp", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("rcp", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("rcp", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("reflect",			Nodes::Intrinsic::Op::Reflect, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("reflect",			Nodes::Intrinsic::Op::Reflect, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("reflect",			Nodes::Intrinsic::Op::Reflect, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("reflect",			Nodes::Intrinsic::Op::Reflect, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("refract",			Nodes::Intrinsic::Op::Refract, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("refract",			Nodes::Intrinsic::Op::Refract, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("refract",			Nodes::Intrinsic::Op::Refract, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("refract",			Nodes::Intrinsic::Op::Refract, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("round", 				Nodes::Intrinsic::Op::Round, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("round", 				Nodes::Intrinsic::Op::Round, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("round", 				Nodes::Intrinsic::Op::Round, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("round", 				Nodes::Intrinsic::Op::Round, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("rsqrt", 				Nodes::Intrinsic::Op::Rsqrt, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("rsqrt", 				Nodes::Intrinsic::Op::Rsqrt, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("rsqrt", 				Nodes::Intrinsic::Op::Rsqrt, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("rsqrt", 				Nodes::Intrinsic::Op::Rsqrt, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("saturate", 			Nodes::Intrinsic::Op::Saturate,				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("saturate", 			Nodes::Intrinsic::Op::Saturate,				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("saturate", 			Nodes::Intrinsic::Op::Saturate,				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("saturate", 			Nodes::Intrinsic::Op::Saturate, 			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("sign", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Int,   1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("sign", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Int,   2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("sign", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Int,   3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("sign", 				Nodes::Intrinsic::Op::Sign, 				Nodes::Type::Class::Int,   4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("sin", 				Nodes::Intrinsic::Op::Sin, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("sin", 				Nodes::Intrinsic::Op::Sin, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("sin", 				Nodes::Intrinsic::Op::Sin, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("sin", 				Nodes::Intrinsic::Op::Sin, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("sincos",				Nodes::Intrinsic::Op::SinCos, 				Nodes::Type::Class::Void,  0, 0, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("sincos",				Nodes::Intrinsic::Op::SinCos, 				Nodes::Type::Class::Void,  0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("sincos",				Nodes::Intrinsic::Op::SinCos, 				Nodes::Type::Class::Void,  0, 0, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("sincos",				Nodes::Intrinsic::Op::SinCos, 				Nodes::Type::Class::Void,  0, 0, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("sinh", 				Nodes::Intrinsic::Op::Sinh, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("sinh", 				Nodes::Intrinsic::Op::Sinh, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("sinh", 				Nodes::Intrinsic::Op::Sinh, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("sinh", 				Nodes::Intrinsic::Op::Sinh, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("smoothstep",			Nodes::Intrinsic::Op::SmoothStep, 			Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("smoothstep",			Nodes::Intrinsic::Op::SmoothStep, 			Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("smoothstep",			Nodes::Intrinsic::Op::SmoothStep, 			Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("smoothstep",			Nodes::Intrinsic::Op::SmoothStep, 			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("sqrt", 				Nodes::Intrinsic::Op::Sqrt, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("sqrt", 				Nodes::Intrinsic::Op::Sqrt, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("sqrt", 				Nodes::Intrinsic::Op::Sqrt, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("sqrt", 				Nodes::Intrinsic::Op::Sqrt, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("step", 				Nodes::Intrinsic::Op::Step, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("step", 				Nodes::Intrinsic::Op::Step, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("step",				Nodes::Intrinsic::Op::Step, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("step",				Nodes::Intrinsic::Op::Step, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tan", 				Nodes::Intrinsic::Op::Tan, 					Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("tan", 				Nodes::Intrinsic::Op::Tan, 					Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("tan", 				Nodes::Intrinsic::Op::Tan, 					Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("tan", 				Nodes::Intrinsic::Op::Tan, 					Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tanh", 				Nodes::Intrinsic::Op::Tanh, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("tanh", 				Nodes::Intrinsic::Op::Tanh, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("tanh", 				Nodes::Intrinsic::Op::Tanh, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("tanh", 				Nodes::Intrinsic::Op::Tanh, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tex2D",				Nodes::Intrinsic::Op::Tex2D,				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("tex2Dfetch",			Nodes::Intrinsic::Op::Tex2DFetch,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Int,   2, 1),
				Intrinsic("tex2Dgather",		Nodes::Intrinsic::Op::Tex2DGather,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int,   1, 1),
				Intrinsic("tex2Dgatheroffset",	Nodes::Intrinsic::Op::Tex2DGatherOffset,	Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int,   2, 1, Nodes::Type::Class::Int,   1, 1),
				Intrinsic("tex2Dgrad",			Nodes::Intrinsic::Op::Tex2DGrad,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("tex2Dlod",			Nodes::Intrinsic::Op::Tex2DLevel,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tex2Dlodoffset",		Nodes::Intrinsic::Op::Tex2DLevelOffset,		Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Int,   2, 1),
				Intrinsic("tex2Doffset",		Nodes::Intrinsic::Op::Tex2DOffset,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int,   2, 1),
				Intrinsic("tex2Dproj",			Nodes::Intrinsic::Op::Tex2D,				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tex2Dsize",			Nodes::Intrinsic::Op::Tex2DSize,			Nodes::Type::Class::Int,   2, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Int,   1, 1),
				Intrinsic("transpose", 			Nodes::Intrinsic::Op::Transpose, 			Nodes::Type::Class::Float, 2, 2, Nodes::Type::Class::Float, 2, 2),
				Intrinsic("transpose", 			Nodes::Intrinsic::Op::Transpose, 			Nodes::Type::Class::Float, 3, 3, Nodes::Type::Class::Float, 3, 3),
				Intrinsic("transpose",			Nodes::Intrinsic::Op::Transpose, 			Nodes::Type::Class::Float, 4, 4, Nodes::Type::Class::Float, 4, 4),
				Intrinsic("trunc", 				Nodes::Intrinsic::Op::Trunc, 				Nodes::Type::Class::Float, 1, 1, Nodes::Type::Class::Float, 1, 1),
				Intrinsic("trunc", 				Nodes::Intrinsic::Op::Trunc, 				Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("trunc", 				Nodes::Intrinsic::Op::Trunc, 				Nodes::Type::Class::Float, 3, 1, Nodes::Type::Class::Float, 3, 1),
				Intrinsic("trunc", 				Nodes::Intrinsic::Op::Trunc, 				Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Float, 4, 1),
			};
			#pragma endregion

			void ScalarLiteralCast(const Nodes::Literal *from, size_t i, int &to)
			{
				switch (from->Type.BaseClass)
				{
					case Nodes::Type::Class::Bool:
					case Nodes::Type::Class::Int:
					case Nodes::Type::Class::Uint:
						to = from->Value.Int[i];
						break;
					case Nodes::Type::Class::Float:
						to = static_cast<int>(from->Value.Float[i]);
						break;
					default:
						to = 0;
						break;
				}
			}
			void ScalarLiteralCast(const Nodes::Literal *from, size_t i, unsigned int &to)
			{
				switch (from->Type.BaseClass)
				{
					case Nodes::Type::Class::Bool:
					case Nodes::Type::Class::Int:
					case Nodes::Type::Class::Uint:
						to = from->Value.Uint[i];
						break;
					case Nodes::Type::Class::Float:
						to = static_cast<unsigned int>(from->Value.Float[i]);
						break;
					default:
						to = 0;
						break;
				}
			}
			void ScalarLiteralCast(const Nodes::Literal *from, size_t i, float &to)
			{
				switch (from->Type.BaseClass)
				{
					case Nodes::Type::Class::Bool:
					case Nodes::Type::Class::Int:
						to = static_cast<float>(from->Value.Int[i]);
						break;
					case Nodes::Type::Class::Uint:
						to = static_cast<float>(from->Value.Uint[i]);
						break;
					case Nodes::Type::Class::Float:
						to = from->Value.Float[i];
						break;
					default:
						to = 0;
						break;
				}
			}
			void VectorLiteralCast(const Nodes::Literal *from, size_t k, Nodes::Literal *to, size_t j)
			{
				switch (to->Type.BaseClass)
				{
					case Nodes::Type::Class::Bool:
					case Nodes::Type::Class::Int:
						ScalarLiteralCast(from, j, to->Value.Int[k]);
						break;
					case Nodes::Type::Class::Uint:
						ScalarLiteralCast(from, j, to->Value.Uint[k]);
						break;
					case Nodes::Type::Class::Float:
						ScalarLiteralCast(from, j, to->Value.Float[k]);
						break;
					default:
						to->Value = from->Value;
						break;
				}
			}

			unsigned int GetTypeRank(const Nodes::Type &src, const Nodes::Type &dst)
			{
				if (src.IsArray() != dst.IsArray() || (src.ArrayLength != dst.ArrayLength && src.ArrayLength > 0 && dst.ArrayLength > 0))
				{
					return 0;
				}
				if (src.IsStruct() || dst.IsStruct())
				{
					return src.Definition == dst.Definition;
				}
				if (src.BaseClass == dst.BaseClass && src.Rows == dst.Rows && src.Cols == dst.Cols)
				{
					return 1;
				}
				if (!src.IsNumeric() || !dst.IsNumeric())
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

				const int rank = ranks[static_cast<unsigned int>(src.BaseClass) - 1][static_cast<unsigned int>(dst.BaseClass) - 1] << 2;

				if (src.IsScalar() && dst.IsVector())
				{
					return rank | 2;
				}
				if (src.IsVector() && dst.IsScalar() || (src.IsVector() == dst.IsVector() && src.Rows > dst.Rows && src.Cols >= dst.Cols))
				{
					return rank | 32;
				}
				if (src.IsVector() != dst.IsVector() || src.IsMatrix() != dst.IsMatrix() || src.Rows * src.Cols != dst.Rows * dst.Cols)
				{
					return 0;
				}

				return rank;
			}
			bool GetCallRanks(const Nodes::Call *call, const Nodes::Function *function, unsigned int ranks[])
			{
				for (size_t i = 0, count = call->Arguments.size(); i < count; ++i)
				{
					ranks[i] = GetTypeRank(call->Arguments[i]->Type, function->Parameters[i]->Type);

					if (ranks[i] == 0)
					{
						return false;
					}
				}

				return true;
			}
			int CompareFunctions(const Nodes::Call *call, const Nodes::Function *function1, const Nodes::Function *function2)
			{
				if (function2 == nullptr)
				{
					return -1;
				}

				const size_t count = call->Arguments.size();

				unsigned int *const function1Ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				unsigned int *const function2Ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				const bool function1Viable = GetCallRanks(call, function1, function1Ranks);
				const bool function2Viable = GetCallRanks(call, function2, function2Ranks);

				if (!(function1Viable && function2Viable))
				{
					return function2Viable - function1Viable;
				}

				std::sort(function1Ranks, function1Ranks + count, std::greater<unsigned int>());
				std::sort(function2Ranks, function2Ranks + count, std::greater<unsigned int>());

				for (size_t i = 0; i < count; ++i)
				{
					if (function1Ranks[i] < function2Ranks[i])
					{
						return -1;
					}
					else if (function2Ranks[i] < function1Ranks[i])
					{
						return +1;
					}
				}

				return 0;
			}

			const char *GetTokenName(lexer::tokenid tokid)
			{
				switch (tokid)
				{
					default:
					case lexer::tokenid::unknown:
						return "unknown";
					case lexer::tokenid::end_of_stream:
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

		class Parser
		{
		public:
			explicit Parser(const std::string &input, nodetree &ast, std::string &errors);

			bool Parse();

		protected:
			void Backup();
			void Restore();

			bool Peek(char tok) const
			{
				return Peek(static_cast<lexer::tokenid>(tok));
			}
			bool Peek(lexer::tokenid tokid) const;
			void Consume();
			void ConsumeUntil(char token)
			{
				ConsumeUntil(static_cast<lexer::tokenid>(token));
			}
			void ConsumeUntil(lexer::tokenid tokid);
			bool Accept(char tok)
			{
				return Accept(static_cast<lexer::tokenid>(tok));
			}
			bool Accept(lexer::tokenid tokid);
			bool Expect(char tok)
			{
				return Expect(static_cast<lexer::tokenid>(tok));
			}
			bool Expect(lexer::tokenid tokid);

		private:
			void Error(const location &location, unsigned int code, const char *message, ...);
			void Warning(const location &location, unsigned int code, const char *message, ...);

			// Types
			bool AcceptTypeClass(Nodes::Type &type);
			bool AcceptTypeQualifiers(Nodes::Type &type);
			bool ParseType(Nodes::Type &type);

			// Expressions
			bool AcceptUnaryOp(Nodes::Unary::Op &op);
			bool AcceptPostfixOp(Nodes::Unary::Op &op);
			bool PeekMultaryOp(Nodes::Binary::Op &op, unsigned int &precedence) const;
			bool AcceptAssignmentOp(Nodes::Assignment::Op &op);
			bool ParseExpression(Nodes::Expression *&expression);
			bool ParseExpressionUnary(Nodes::Expression *&expression);
			bool ParseExpressionMultary(Nodes::Expression *&expression, unsigned int precedence = 0);
			bool ParseExpressionAssignment(Nodes::Expression *&expression);

			// Statements
			bool ParseStatement(Nodes::Statement *&statement, bool scoped = true);
			bool ParseStatementBlock(Nodes::Statement *&statement, bool scoped = true);
			bool ParseStatementDeclaratorList(Nodes::Statement *&statement);

			// Declarations
			bool ParseTopLevel();
			bool ParseNamespace();
			bool ParseArray(int &size);
			bool ParseAnnotations(std::vector<Nodes::Annotation> &annotations);
			bool ParseStruct(Nodes::Struct *&structure);
			bool ParseFunctionResidue(Nodes::Type &type, std::string name, Nodes::Function *&function);
			bool ParseVariableResidue(Nodes::Type &type, std::string name, Nodes::Variable *&variable, bool global = false);
			bool ParseVariableAssignment(Nodes::Expression *&expression);
			bool ParseVariableProperties(Nodes::Variable *variable);
			bool ParseVariablePropertiesExpression(Nodes::Expression *&expression);
			bool ParseTechnique(Nodes::Technique *&technique);
			bool ParseTechniquePass(Nodes::Pass *&pass);
			bool ParseTechniquePassExpression(Nodes::Expression *&expression);

			// Symbol Table
			struct Scope
			{
				std::string Name;
				unsigned int Level, NamespaceLevel;
			};
			typedef Nodes::Declaration Symbol;

			void EnterScope(Symbol *parent = nullptr);
			void EnterNamespace(const std::string &name);
			void LeaveScope();
			void LeaveNamespace();
			bool InsertSymbol(Symbol *symbol, bool global = false);
			Symbol *FindSymbol(const std::string &name) const;
			Symbol *FindSymbol(const std::string &name, const Scope &scope, bool exclusive = false) const;
			bool ResolveCall(Nodes::Call *call, const Scope &scope, bool &intrinsic, bool &ambiguouss) const;
			Nodes::Expression *FoldConstantExpression(Nodes::Expression *expression) const;

			nodetree *_ast;
			std::string *_errors;
			std::unique_ptr<lexer> _lexer, _lexerBackup;
			lexer::token _token, _tokenNext, _tokenBackup;
			Scope _currentScope;
			std::stack<Symbol *> _parentStack;
			std::unordered_map<std::string, std::vector<std::pair<Scope, Symbol *>>> _symbolStack;
		};

		bool parse(const std::string &source, nodetree &ast, std::string &errors)
		{
			return Parser(source, ast, errors).Parse();
		}

		Parser::Parser(const std::string &input, nodetree &ast, std::string &errors) : _ast(&ast), _errors(&errors), _lexer(new lexer(input))
		{
			_currentScope.Name = "::";
			_currentScope.Level = 0;
			_currentScope.NamespaceLevel = 0;
		}

		void Parser::Backup()
		{
			_lexer.swap(_lexerBackup);
			_lexer.reset(new lexer(*_lexerBackup));
			_tokenBackup = _tokenNext;
		}
		void Parser::Restore()
		{
			_lexer.swap(_lexerBackup);
			_tokenNext = _tokenBackup;
		}

		bool Parser::Peek(lexer::tokenid tokid) const
		{
			return _tokenNext.id == tokid;
		}
		void Parser::Consume()
		{
			_token = _tokenNext;
			_tokenNext = _lexer->lex();
		}
		void Parser::ConsumeUntil(lexer::tokenid tokid)
		{
			while (!Accept(tokid) && !Peek(lexer::tokenid::end_of_stream))
			{
				Consume();
			}
		}
		bool Parser::Accept(lexer::tokenid tokid)
		{
			if (Peek(tokid))
			{
				Consume();

				return true;
			}

			return false;
		}
		bool Parser::Expect(lexer::tokenid tokid)
		{
			if (!Accept(tokid))
			{
				Error(_tokenNext.location, 3000, "syntax error: unexpected '%s', expected '%s'", GetTokenName(_tokenNext.id), GetTokenName(tokid));

				return false;
			}

			return true;
		}

		void Parser::Error(const location &location, unsigned int code, const char *message, ...)
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
		void Parser::Warning(const location &location, unsigned int code, const char *message, ...)
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
		bool Parser::AcceptTypeClass(Nodes::Type &type)
		{
			type.Definition = nullptr;
			type.ArrayLength = 0;

			if (Peek(lexer::tokenid::identifier))
			{
				type.Rows = type.Cols = 0;
				type.BaseClass = Nodes::Type::Class::Struct;

				const auto symbol = FindSymbol(_tokenNext.literal_as_string);

				if (symbol != nullptr && symbol->id == nodeid::Struct)
				{
					type.Definition = static_cast<Nodes::Struct *>(symbol);

					Consume();
				}
				else
				{
					return false;
				}
			}
			else if (Accept(lexer::tokenid::vector))
			{
				type.Rows = 4, type.Cols = 1;
				type.BaseClass = Nodes::Type::Class::Float;

				if (Accept('<'))
				{
					if (!AcceptTypeClass(type))
					{
						Error(_tokenNext.location, 3000, "syntax error: unexpected '%s', expected vector element type", GetTokenName(_tokenNext.id));

						return false;
					}

					if (!type.IsScalar())
					{
						Error(_token.location, 3122, "vector element type must be a scalar type");

						return false;
					}

					if (!(Expect(',') && Expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						Error(_token.location, 3052, "vector dimension must be between 1 and 4");

						return false;
					}

					type.Rows = _token.literal_as_int;

					if (!Expect('>'))
					{
						return false;
					}
				}
			}
			else if (Accept(lexer::tokenid::matrix))
			{
				type.Rows = 4, type.Cols = 4;
				type.BaseClass = Nodes::Type::Class::Float;

				if (Accept('<'))
				{
					if (!AcceptTypeClass(type))
					{
						Error(_tokenNext.location, 3000, "syntax error: unexpected '%s', expected matrix element type", GetTokenName(_tokenNext.id));

						return false;
					}

					if (!type.IsScalar())
					{
						Error(_token.location, 3123, "matrix element type must be a scalar type");

						return false;
					}

					if (!(Expect(',') && Expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						Error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.Rows = _token.literal_as_int;

					if (!(Expect(',') && Expect(lexer::tokenid::int_literal)))
					{
						return false;
					}

					if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					{
						Error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.Cols = _token.literal_as_int;

					if (!Expect('>'))
					{
						return false;
					}
				}
			}
			else
			{
				type.Rows = type.Cols = 0;

				switch (_tokenNext.id)
				{
					case lexer::tokenid::void_:
						type.BaseClass = Nodes::Type::Class::Void;
						break;
					case lexer::tokenid::bool_:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::bool4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case lexer::tokenid::int_:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::int4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case lexer::tokenid::uint_:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::uint4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case lexer::tokenid::float_:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::float4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case lexer::tokenid::string_:
						type.BaseClass = Nodes::Type::Class::String;
						break;
					case lexer::tokenid::texture1d:
						type.BaseClass = Nodes::Type::Class::Texture1D;
						break;
					case lexer::tokenid::texture2d:
						type.BaseClass = Nodes::Type::Class::Texture2D;
						break;
					case lexer::tokenid::texture3d:
						type.BaseClass = Nodes::Type::Class::Texture3D;
						break;
					case lexer::tokenid::sampler1d:
						type.BaseClass = Nodes::Type::Class::Sampler1D;
						break;
					case lexer::tokenid::sampler2d:
						type.BaseClass = Nodes::Type::Class::Sampler2D;
						break;
					case lexer::tokenid::sampler3d:
						type.BaseClass = Nodes::Type::Class::Sampler3D;
						break;
					default:
						return false;
				}

				Consume();
			}

			return true;
		}
		bool Parser::AcceptTypeQualifiers(Nodes::Type &type)
		{
			unsigned int qualifiers = 0;

			// Storage
			if (Accept(lexer::tokenid::extern_))
			{
				qualifiers |= Nodes::Type::Qualifier::Extern;
			}
			if (Accept(lexer::tokenid::static_))
			{
				qualifiers |= Nodes::Type::Qualifier::Static;
			}
			if (Accept(lexer::tokenid::uniform_))
			{
				qualifiers |= Nodes::Type::Qualifier::Uniform;
			}
			if (Accept(lexer::tokenid::volatile_))
			{
				qualifiers |= Nodes::Type::Qualifier::Volatile;
			}
			if (Accept(lexer::tokenid::precise))
			{
				qualifiers |= Nodes::Type::Qualifier::Precise;
			}

			if (Accept(lexer::tokenid::in))
			{
				qualifiers |= Nodes::Type::Qualifier::In;
			}
			if (Accept(lexer::tokenid::out))
			{
				qualifiers |= Nodes::Type::Qualifier::Out;
			}
			if (Accept(lexer::tokenid::inout))
			{
				qualifiers |= Nodes::Type::Qualifier::InOut;
			}

			// Modifiers
			if (Accept(lexer::tokenid::const_))
			{
				qualifiers |= Nodes::Type::Qualifier::Const;
			}

			// Interpolation
			if (Accept(lexer::tokenid::linear))
			{
				qualifiers |= Nodes::Type::Qualifier::Linear;
			}
			if (Accept(lexer::tokenid::noperspective))
			{
				qualifiers |= Nodes::Type::Qualifier::NoPerspective;
			}
			if (Accept(lexer::tokenid::centroid))
			{
				qualifiers |= Nodes::Type::Qualifier::Centroid;
			}
			if (Accept(lexer::tokenid::nointerpolation))
			{
				qualifiers |= Nodes::Type::Qualifier::NoInterpolation;
			}

			if (qualifiers == 0)
			{
				return false;
			}
			if ((type.Qualifiers & qualifiers) == qualifiers)
			{
				Warning(_token.location, 3048, "duplicate usages specified");
			}

			type.Qualifiers |= qualifiers;

			AcceptTypeQualifiers(type);

			return true;
		}
		bool Parser::ParseType(Nodes::Type &type)
		{
			type.Qualifiers = 0;

			AcceptTypeQualifiers(type);

			const auto location = _tokenNext.location;

			if (!AcceptTypeClass(type))
			{
				return false;
			}

			if (type.IsIntegral() && (type.HasQualifier(Nodes::Type::Qualifier::Centroid) || type.HasQualifier(Nodes::Type::Qualifier::NoPerspective)))
			{
				Error(location, 4576, "signature specifies invalid interpolation mode for integer component type");

				return false;
			}

			if (type.HasQualifier(Nodes::Type::Qualifier::Centroid) && !type.HasQualifier(Nodes::Type::Qualifier::NoPerspective))
			{
				type.Qualifiers |= Nodes::Type::Qualifier::Linear;
			}

			return true;
		}

		// Expressions
		bool Parser::AcceptUnaryOp(Nodes::Unary::Op &op)
		{
			switch (_tokenNext.id)
			{
				case lexer::tokenid::exclaim:
					op = Nodes::Unary::Op::LogicalNot;
					break;
				case lexer::tokenid::plus:
					op = Nodes::Unary::Op::None;
					break;
				case lexer::tokenid::minus:
					op = Nodes::Unary::Op::Negate;
					break;
				case lexer::tokenid::tilde:
					op = Nodes::Unary::Op::BitwiseNot;
					break;
				case lexer::tokenid::plus_plus:
					op = Nodes::Unary::Op::Increase;
					break;
				case lexer::tokenid::minus_minus:
					op = Nodes::Unary::Op::Decrease;
					break;
				default:
					return false;
			}

			Consume();

			return true;
		}
		bool Parser::AcceptPostfixOp(Nodes::Unary::Op &op)
		{
			switch (_tokenNext.id)
			{
				case lexer::tokenid::plus_plus:
					op = Nodes::Unary::Op::PostIncrease;
					break;
				case lexer::tokenid::minus_minus:
					op = Nodes::Unary::Op::PostDecrease;
					break;
				default:
					return false;
			}

			Consume();

			return true;
		}
		bool Parser::PeekMultaryOp(Nodes::Binary::Op &op, unsigned int &precedence) const
		{
			switch (_tokenNext.id)
			{
				case lexer::tokenid::percent:
					op = Nodes::Binary::Op::Modulo;
					precedence = 11;
					break;
				case lexer::tokenid::ampersand:
					op = Nodes::Binary::Op::BitwiseAnd;
					precedence = 6;
					break;
				case lexer::tokenid::star:
					op = Nodes::Binary::Op::Multiply;
					precedence = 11;
					break;
				case lexer::tokenid::plus:
					op = Nodes::Binary::Op::Add;
					precedence = 10;
					break;
				case lexer::tokenid::minus:
					op = Nodes::Binary::Op::Subtract;
					precedence = 10;
					break;
				case lexer::tokenid::slash:
					op = Nodes::Binary::Op::Divide;
					precedence = 11;
					break;
				case lexer::tokenid::less:
					op = Nodes::Binary::Op::Less;
					precedence = 8;
					break;
				case lexer::tokenid::greater:
					op = Nodes::Binary::Op::Greater;
					precedence = 8;
					break;
				case lexer::tokenid::question:
					op = Nodes::Binary::Op::None;
					precedence = 1;
					break;
				case lexer::tokenid::caret:
					op = Nodes::Binary::Op::BitwiseXor;
					precedence = 5;
					break;
				case lexer::tokenid::pipe:
					op = Nodes::Binary::Op::BitwiseOr;
					precedence = 4;
					break;
				case lexer::tokenid::exclaim_equal:
					op = Nodes::Binary::Op::NotEqual;
					precedence = 7;
					break;
				case lexer::tokenid::ampersand_ampersand:
					op = Nodes::Binary::Op::LogicalAnd;
					precedence = 3;
					break;
				case lexer::tokenid::less_less:
					op = Nodes::Binary::Op::LeftShift;
					precedence = 9;
					break;
				case lexer::tokenid::less_equal:
					op = Nodes::Binary::Op::LessOrEqual;
					precedence = 8;
					break;
				case lexer::tokenid::equal_equal:
					op = Nodes::Binary::Op::Equal;
					precedence = 7;
					break;
				case lexer::tokenid::greater_greater:
					op = Nodes::Binary::Op::RightShift;
					precedence = 9;
					break;
				case lexer::tokenid::greater_equal:
					op = Nodes::Binary::Op::GreaterOrEqual;
					precedence = 8;
					break;
				case lexer::tokenid::pipe_pipe:
					op = Nodes::Binary::Op::LogicalOr;
					precedence = 2;
					break;
				default:
					return false;
			}

			return true;
		}
		bool Parser::AcceptAssignmentOp(Nodes::Assignment::Op &op)
		{
			switch (_tokenNext.id)
			{
				case lexer::tokenid::equal:
					op = Nodes::Assignment::Op::None;
					break;
				case lexer::tokenid::percent_equal:
					op = Nodes::Assignment::Op::Modulo;
					break;
				case lexer::tokenid::ampersand_equal:
					op = Nodes::Assignment::Op::BitwiseAnd;
					break;
				case lexer::tokenid::star_equal:
					op = Nodes::Assignment::Op::Multiply;
					break;
				case lexer::tokenid::plus_equal:
					op = Nodes::Assignment::Op::Add;
					break;
				case lexer::tokenid::minus_equal:
					op = Nodes::Assignment::Op::Subtract;
					break;
				case lexer::tokenid::slash_equal:
					op = Nodes::Assignment::Op::Divide;
					break;
				case lexer::tokenid::less_less_equal:
					op = Nodes::Assignment::Op::LeftShift;
					break;
				case lexer::tokenid::greater_greater_equal:
					op = Nodes::Assignment::Op::RightShift;
					break;
				case lexer::tokenid::caret_equal:
					op = Nodes::Assignment::Op::BitwiseXor;
					break;
				case lexer::tokenid::pipe_equal:
					op = Nodes::Assignment::Op::BitwiseOr;
					break;
				default:
					return false;
			}

			Consume();

			return true;
		}
		bool Parser::ParseExpression(Nodes::Expression *&node)
		{
			if (!ParseExpressionAssignment(node))
			{
				return false;
			}

			if (Peek(','))
			{
				const auto sequence = _ast->make_node<Nodes::Sequence>(node->location);
				sequence->Expressions.push_back(node);

				while (Accept(','))
				{
					Nodes::Expression *expression = nullptr;

					if (!ParseExpressionAssignment(expression))
					{
						return false;
					}

					sequence->Expressions.push_back(std::move(expression));
				}

				sequence->Type = sequence->Expressions.back()->Type;

				node = sequence;
			}

			return true;
		}
		bool Parser::ParseExpressionUnary(Nodes::Expression *&node)
		{
			Nodes::Type type;
			Nodes::Unary::Op op;
			auto location = _tokenNext.location;

			#pragma region Prefix
			if (AcceptUnaryOp(op))
			{
				if (!ParseExpressionUnary(node))
				{
					return false;
				}

				if (!node->Type.IsScalar() && !node->Type.IsVector() && !node->Type.IsMatrix())
				{
					Error(node->location, 3022, "scalar, vector, or matrix expected");

					return false;
				}

				if (op != Nodes::Unary::Op::None)
				{
					if (op == Nodes::Unary::Op::BitwiseNot && !node->Type.IsIntegral())
					{
						Error(node->location, 3082, "int or unsigned int type required");

						return false;
					}
					else if ((op == Nodes::Unary::Op::Increase || op == Nodes::Unary::Op::Decrease) && (node->Type.HasQualifier(Nodes::Type::Qualifier::Const) || node->Type.HasQualifier(Nodes::Type::Qualifier::Uniform)))
					{
						Error(node->location, 3025, "l-value specifies const object");

						return false;
					}

					const auto newexpression = _ast->make_node<Nodes::Unary>(location);
					newexpression->Type = node->Type;
					newexpression->Operator = op;
					newexpression->Operand = node;

					node = FoldConstantExpression(newexpression);
				}

				type = node->Type;
			}
			else if (Accept('('))
			{
				Backup();

				if (AcceptTypeClass(type))
				{
					if (Peek('('))
					{
						Restore();
					}
					else if (Expect(')'))
					{
						if (!ParseExpressionUnary(node))
						{
							return false;
						}

						if (node->Type.BaseClass == type.BaseClass && (node->Type.Rows == type.Rows && node->Type.Cols == type.Cols) && !(node->Type.IsArray() || type.IsArray()))
						{
							return true;
						}
						else if (node->Type.IsNumeric() && type.IsNumeric())
						{
							if ((node->Type.Rows < type.Rows || node->Type.Cols < type.Cols) && !node->Type.IsScalar())
							{
								Error(location, 3017, "cannot convert these vector types");

								return false;
							}

							if (node->Type.Rows > type.Rows || node->Type.Cols > type.Cols)
							{
								Warning(location, 3206, "implicit truncation of vector type");
							}

							const auto castexpression = _ast->make_node<Nodes::Unary>(location);
							type.Qualifiers = Nodes::Type::Qualifier::Const;
							castexpression->Type = type;
							castexpression->Operator = Nodes::Unary::Op::Cast;
							castexpression->Operand = node;

							node = FoldConstantExpression(castexpression);

							return true;
						}
						else
						{
							Error(location, 3017, "cannot convert non-numeric types");

							return false;
						}
					}
					else
					{
						return false;
					}
				}

				if (!ParseExpression(node))
				{
					return false;
				}

				if (!Expect(')'))
				{
					return false;
				}

				type = node->Type;
			}
			else if (Accept(lexer::tokenid::true_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Bool;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = 1;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::false_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Bool;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = 0;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::int_literal))
			{
				Nodes::Literal *const literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Int;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = _token.literal_as_int;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::uint_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Uint;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Uint[0] = _token.literal_as_uint;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::float_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Float;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Float[0] = _token.literal_as_float;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::double_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Float;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Float[0] = static_cast<float>(_token.literal_as_double);

				node = literal;
				type = literal->Type;
			}
			else if (Accept(lexer::tokenid::string_literal))
			{
				const auto literal = _ast->make_node<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::String;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 0, literal->Type.ArrayLength = 0;
				literal->StringValue = _token.literal_as_string;

				while (Accept(lexer::tokenid::string_literal))
				{
					literal->StringValue += _token.literal_as_string;
				}

				node = literal;
				type = literal->Type;
			}
			else if (AcceptTypeClass(type))
			{
				if (!Expect('('))
				{
					return false;
				}

				if (!type.IsNumeric())
				{
					Error(location, 3037, "constructors only defined for numeric base types");

					return false;
				}

				if (Accept(')'))
				{
					Error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				const auto constructor = _ast->make_node<Nodes::Constructor>(location);
				constructor->Type = type;
				constructor->Type.Qualifiers = Nodes::Type::Qualifier::Const;

				unsigned int elements = 0;

				while (!Peek(')'))
				{
					if (!constructor->Arguments.empty() && !Expect(','))
					{
						return false;
					}

					Nodes::Expression *argument = nullptr;

					if (!ParseExpressionAssignment(argument))
					{
						return false;
					}

					if (!argument->Type.IsNumeric())
					{
						Error(argument->location, 3017, "cannot convert non-numeric types");

						return false;
					}

					elements += argument->Type.Rows * argument->Type.Cols;

					constructor->Arguments.push_back(std::move(argument));
				}

				if (!Expect(')'))
				{
					return false;
				}

				if (elements != type.Rows * type.Cols)
				{
					Error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				if (constructor->Arguments.size() > 1)
				{
					node = constructor;
					type = constructor->Type;
				}
				else
				{
					const auto castexpression = _ast->make_node<Nodes::Unary>(constructor->location);
					castexpression->Type = type;
					castexpression->Operator = Nodes::Unary::Op::Cast;
					castexpression->Operand = constructor->Arguments[0];

					node = castexpression;
				}

				node = FoldConstantExpression(node);
			}
			else
			{
				Scope scope;
				bool exclusive;
				std::string identifier;

				if (Accept(lexer::tokenid::colon_colon))
				{
					scope.NamespaceLevel = scope.Level = 0;
					exclusive = true;
				}
				else
				{
					scope = _currentScope;
					exclusive = false;
				}

				if (exclusive ? Expect(lexer::tokenid::identifier) : Accept(lexer::tokenid::identifier))
				{
					identifier = _token.literal_as_string;
				}
				else
				{
					return false;
				}

				while (Accept(lexer::tokenid::colon_colon))
				{
					if (!Expect(lexer::tokenid::identifier))
					{
						return false;
					}

					identifier += "::" + _token.literal_as_string;
				}

				const auto symbol = FindSymbol(identifier, scope, exclusive);

				if (Accept('('))
				{
					if (symbol != nullptr && symbol->id == nodeid::Variable)
					{
						Error(location, 3005, "identifier '%s' represents a variable, not a function", identifier.c_str());

						return false;
					}

					const auto callexpression = _ast->make_node<Nodes::Call>(location);
					callexpression->CalleeName = identifier;

					while (!Peek(')'))
					{
						if (!callexpression->Arguments.empty() && !Expect(','))
						{
							return false;
						}

						Nodes::Expression *argument = nullptr;

						if (!ParseExpressionAssignment(argument))
						{
							return false;
						}

						callexpression->Arguments.push_back(std::move(argument));
					}

					if (!Expect(')'))
					{
						return false;
					}

					bool undeclared = symbol == nullptr, intrinsic = false, ambiguous = false;

					if (!ResolveCall(callexpression, scope, intrinsic, ambiguous))
					{
						if (undeclared && !intrinsic)
						{
							Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());
						}
						else if (ambiguous)
						{
							Error(location, 3067, "ambiguous function call to '%s'", identifier.c_str());
						}
						else
						{
							Error(location, 3013, "no matching function overload for '%s'", identifier.c_str());
						}

						return false;
					}

					if (intrinsic)
					{
						const auto newexpression = _ast->make_node<Nodes::Intrinsic>(callexpression->location);
						newexpression->Type = callexpression->Type;
						newexpression->Operator = static_cast<Nodes::Intrinsic::Op>(reinterpret_cast<unsigned int>(callexpression->Callee));

						for (size_t i = 0, count = std::min(callexpression->Arguments.size(), sizeof(newexpression->Arguments) / sizeof(*newexpression->Arguments)); i < count; ++i)
						{
							newexpression->Arguments[i] = callexpression->Arguments[i];
						}

						node = FoldConstantExpression(newexpression);
					}
					else
					{
						const auto parent = _parentStack.empty() ? nullptr : _parentStack.top();

						if (parent == callexpression->Callee)
						{
							Error(location, 3500, "recursive function calls are not allowed");

							return false;
						}

						node = callexpression;
					}

					type = node->Type;
				}
				else
				{
					if (symbol == nullptr)
					{
						Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

						return false;
					}

					if (symbol->id != nodeid::Variable)
					{
						Error(location, 3005, "identifier '%s' represents a function, not a variable", identifier.c_str());

						return false;
					}

					const auto newexpression = _ast->make_node<Nodes::LValue>(location);
					newexpression->Reference = static_cast<const Nodes::Variable *>(symbol);
					newexpression->Type = newexpression->Reference->Type;

					node = FoldConstantExpression(newexpression);
					type = node->Type;
				}
			}
			#pragma endregion

			#pragma region Postfix
			while (!Peek(lexer::tokenid::end_of_stream))
			{
				location = _tokenNext.location;

				if (AcceptPostfixOp(op))
				{
					if (!type.IsScalar() && !type.IsVector() && !type.IsMatrix())
					{
						Error(node->location, 3022, "scalar, vector, or matrix expected");

						return false;
					}

					if (type.HasQualifier(Nodes::Type::Qualifier::Const) || type.HasQualifier(Nodes::Type::Qualifier::Uniform))
					{
						Error(node->location, 3025, "l-value specifies const object");

						return false;
					}

					const auto newexpression = _ast->make_node<Nodes::Unary>(location);
					newexpression->Type = type;
					newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
					newexpression->Operator = op;
					newexpression->Operand = node;

					node = newexpression;
					type = node->Type;
				}
				else if (Accept('.'))
				{
					if (!Expect(lexer::tokenid::identifier))
					{
						return false;
					}

					location = _token.location;
					const auto subscript = _token.literal_as_string;

					if (Accept('('))
					{
						if (!type.IsStruct() || type.IsArray())
						{
							Error(location, 3087, "object does not have methods");
						}
						else
						{
							Error(location, 3088, "structures do not have methods");
						}

						return false;
					}
					else if (type.IsArray())
					{
						Error(location, 3018, "invalid subscript on array");

						return false;
					}
					else if (type.IsVector())
					{
						const size_t length = subscript.size();

						if (length > 4)
						{
							Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

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
									Error(location, 3018, "invalid subscript '%s'", subscript.c_str());
									return false;
							}

							if (i > 0 && (set[i] != set[i - 1]))
							{
								Error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}
							if (static_cast<unsigned int>(offsets[i]) >= type.Rows)
							{
								Error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());

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

						const auto newexpression = _ast->make_node<Nodes::Swizzle>(location);
						newexpression->Type = type;
						newexpression->Type.Rows = static_cast<unsigned int>(length);
						newexpression->Operand = node;
						newexpression->Mask[0] = offsets[0];
						newexpression->Mask[1] = offsets[1];
						newexpression->Mask[2] = offsets[2];
						newexpression->Mask[3] = offsets[3];

						if (constant || type.HasQualifier(Nodes::Type::Qualifier::Uniform))
						{
							newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
							newexpression->Type.Qualifiers &= ~Nodes::Type::Qualifier::Uniform;
						}

						node = FoldConstantExpression(newexpression);
						type = node->Type;
					}
					else if (type.IsMatrix())
					{
						const size_t length = subscript.size();

						if (length < 3)
						{
							Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

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
								Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}
							if (set && subscript[i + 1] != 'm')
							{
								Error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}

							const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
							const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

							if ((row >= type.Rows || col >= type.Cols) || j > 3)
							{
								Error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());

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

						const auto newexpression = _ast->make_node<Nodes::Swizzle>(_token.location);
						newexpression->Type = type;
						newexpression->Type.Rows = static_cast<unsigned int>(length / (3 + set));
						newexpression->Type.Cols = 1;
						newexpression->Operand = node;
						newexpression->Mask[0] = offsets[0];
						newexpression->Mask[1] = offsets[1];
						newexpression->Mask[2] = offsets[2];
						newexpression->Mask[3] = offsets[3];

						if (constant || type.HasQualifier(Nodes::Type::Qualifier::Uniform))
						{
							newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
							newexpression->Type.Qualifiers &= ~Nodes::Type::Qualifier::Uniform;
						}

						node = FoldConstantExpression(newexpression);
						type = node->Type;
					}
					else if (type.IsStruct())
					{
						Nodes::Variable *field = nullptr;

						for (auto currfield : type.Definition->Fields)
						{
							if (currfield->Name == subscript)
							{
								field = currfield;
								break;
							}
						}

						if (field == nullptr)
						{
							Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						const auto newexpression = _ast->make_node<Nodes::FieldSelection>(location);
						newexpression->Type = field->Type;
						newexpression->Operand = node;
						newexpression->Field = field;

						if (type.HasQualifier(Nodes::Type::Qualifier::Uniform))
						{
							newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
							newexpression->Type.Qualifiers &= ~Nodes::Type::Qualifier::Uniform;
						}

						node = newexpression;
						type = node->Type;
					}
					else if (type.IsScalar())
					{
						signed char offsets[4] = { -1, -1, -1, -1 };
						const size_t length = subscript.size();

						for (size_t i = 0; i < length; ++i)
						{
							if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
							{
								Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}

							offsets[i] = 0;
						}

						const auto newexpression = _ast->make_node<Nodes::Swizzle>(location);
						newexpression->Type = type;
						newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
						newexpression->Type.Rows = static_cast<unsigned int>(length);
						newexpression->Operand = node;
						newexpression->Mask[0] = offsets[0];
						newexpression->Mask[1] = offsets[1];
						newexpression->Mask[2] = offsets[2];
						newexpression->Mask[3] = offsets[3];

						node = newexpression;
						type = node->Type;
					}
					else
					{
						Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

						return false;
					}
				}
				else if (Accept('['))
				{
					if (!type.IsArray() && !type.IsVector() && !type.IsMatrix())
					{
						Error(node->location, 3121, "array, matrix, vector, or indexable object type expected in index expression");

						return false;
					}

					const auto newexpression = _ast->make_node<Nodes::Binary>(location);
					newexpression->Type = type;
					newexpression->Operator = Nodes::Binary::Op::ElementExtract;
					newexpression->Operands[0] = node;

					if (!ParseExpression(newexpression->Operands[1]))
					{
						return false;
					}

					if (!newexpression->Operands[1]->Type.IsScalar())
					{
						Error(newexpression->Operands[1]->location, 3120, "invalid type for index - index must be a scalar");

						return false;
					}

					if (type.IsArray())
					{
						newexpression->Type.ArrayLength = 0;
					}
					else if (type.IsMatrix())
					{
						newexpression->Type.Rows = newexpression->Type.Cols;
						newexpression->Type.Cols = 1;
					}
					else if (type.IsVector())
					{
						newexpression->Type.Rows = 1;
					}

					node = FoldConstantExpression(newexpression);
					type = node->Type;

					if (!Expect(']'))
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
		bool Parser::ParseExpressionMultary(Nodes::Expression *&left, unsigned int leftPrecedence)
		{
			if (!ParseExpressionUnary(left))
			{
				return false;
			}

			Nodes::Binary::Op op;
			unsigned int rightPrecedence;

			while (PeekMultaryOp(op, rightPrecedence))
			{
				if (rightPrecedence <= leftPrecedence)
				{
					break;
				}

				Consume();

				bool boolean = false;
				Nodes::Expression *right1 = nullptr, *right2 = nullptr;

				if (op != Nodes::Binary::Op::None)
				{
					if (!ParseExpressionMultary(right1, rightPrecedence))
					{
						return false;
					}

					if (op == Nodes::Binary::Op::Equal || op == Nodes::Binary::Op::NotEqual)
					{
						boolean = true;

						if (left->Type.IsArray() || right1->Type.IsArray() || left->Type.Definition != right1->Type.Definition)
						{
							Error(right1->location, 3020, "type mismatch");

							return false;
						}
					}
					else if (op == Nodes::Binary::Op::BitwiseAnd || op == Nodes::Binary::Op::BitwiseOr || op == Nodes::Binary::Op::BitwiseXor)
					{
						if (!left->Type.IsIntegral())
						{
							Error(left->location, 3082, "int or unsigned int type required");

							return false;
						}
						if (!right1->Type.IsIntegral())
						{
							Error(right1->location, 3082, "int or unsigned int type required");

							return false;
						}
					}
					else
					{
						boolean = op == Nodes::Binary::Op::LogicalAnd || op == Nodes::Binary::Op::LogicalOr || op == Nodes::Binary::Op::Less || op == Nodes::Binary::Op::Greater || op == Nodes::Binary::Op::LessOrEqual || op == Nodes::Binary::Op::GreaterOrEqual;

						if (!left->Type.IsScalar() && !left->Type.IsVector() && !left->Type.IsMatrix())
						{
							Error(left->location, 3022, "scalar, vector, or matrix expected");

							return false;
						}
						if (!right1->Type.IsScalar() && !right1->Type.IsVector() && !right1->Type.IsMatrix())
						{
							Error(right1->location, 3022, "scalar, vector, or matrix expected");

							return false;
						}
					}

					const auto newexpression = _ast->make_node<Nodes::Binary>(left->location);
					newexpression->Operator = op;
					newexpression->Operands[0] = left;
					newexpression->Operands[1] = right1;

					right2 = right1, right1 = left;
					left = newexpression;
				}
				else
				{
					if (!left->Type.IsScalar() && !left->Type.IsVector())
					{
						Error(left->location, 3022, "boolean or vector expression expected");

						return false;
					}

					if (!(ParseExpression(right1) && Expect(':') && ParseExpressionAssignment(right2)))
					{
						return false;
					}

					if (right1->Type.IsArray() || right2->Type.IsArray() || right1->Type.Definition != right2->Type.Definition)
					{
						Error(left->location, 3020, "type mismatch between conditional values");

						return false;
					}

					const auto newexpression = _ast->make_node<Nodes::Conditional>(left->location);
					newexpression->Condition = left;
					newexpression->ExpressionOnTrue = right1;
					newexpression->ExpressionOnFalse = right2;

					left = newexpression;
				}

				if (boolean)
				{
					left->Type.BaseClass = Nodes::Type::Class::Bool;
				}
				else
				{
					left->Type.BaseClass = std::max(right1->Type.BaseClass, right2->Type.BaseClass);
				}

				if ((right1->Type.Rows == 1 && right2->Type.Cols == 1) || (right2->Type.Rows == 1 && right2->Type.Cols == 1))
				{
					left->Type.Rows = std::max(right1->Type.Rows, right2->Type.Rows);
					left->Type.Cols = std::max(right1->Type.Cols, right2->Type.Cols);
				}
				else
				{
					left->Type.Rows = std::min(right1->Type.Rows, right2->Type.Rows);
					left->Type.Cols = std::min(right1->Type.Cols, right2->Type.Cols);

					if (right1->Type.Rows > right2->Type.Rows || right1->Type.Cols > right2->Type.Cols)
					{
						Warning(right1->location, 3206, "implicit truncation of vector type");
					}
					if (right2->Type.Rows > right1->Type.Rows || right2->Type.Cols > right1->Type.Cols)
					{
						Warning(right2->location, 3206, "implicit truncation of vector type");
					}
				}

				left = FoldConstantExpression(left);
			}

			return true;
		}
		bool Parser::ParseExpressionAssignment(Nodes::Expression *&left)
		{
			if (!ParseExpressionMultary(left))
			{
				return false;
			}

			Nodes::Assignment::Op op;

			if (AcceptAssignmentOp(op))
			{
				Nodes::Expression *right = nullptr;

				if (!ParseExpressionMultary(right))
				{
					return false;
				}

				if (left->Type.HasQualifier(Nodes::Type::Qualifier::Const) || left->Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					Error(left->location, 3025, "l-value specifies const object");

					return false;
				}

				if (left->Type.IsArray() || right->Type.IsArray() || !GetTypeRank(left->Type, right->Type))
				{
					Error(right->location, 3020, "cannot convert these types");

					return false;
				}

				if (right->Type.Rows > left->Type.Rows || right->Type.Cols > left->Type.Cols)
				{
					Warning(right->location, 3206, "implicit truncation of vector type");
				}

				const auto assignment = _ast->make_node<Nodes::Assignment>(left->location);
				assignment->Type = left->Type;
				assignment->Operator = op;
				assignment->Left = left;
				assignment->Right = right;

				left = assignment;
			}

			return true;
		}

		// Statements
		bool Parser::ParseStatement(Nodes::Statement *&statement, bool scoped)
		{
			std::vector<std::string> attributes;

			// Attributes
			while (Accept('['))
			{
				if (Expect(lexer::tokenid::identifier))
				{
					const auto attribute = _token.literal_as_string;

					if (Expect(']'))
					{
						attributes.push_back(attribute);
					}
				}
				else
				{
					Accept(']');
				}
			}

			if (Peek('{'))
			{
				if (!ParseStatementBlock(statement, scoped))
				{
					return false;
				}

				statement->Attributes = attributes;

				return true;
			}

			if (Accept(';'))
			{
				statement = nullptr;

				return true;
			}

			#pragma region If
			if (Accept(lexer::tokenid::if_))
			{
				const auto newstatement = _ast->make_node<Nodes::If>(_token.location);
				newstatement->Attributes = attributes;

				if (!(Expect('(') && ParseExpression(newstatement->Condition) && Expect(')')))
				{
					return false;
				}

				if (!newstatement->Condition->Type.IsScalar())
				{
					Error(newstatement->Condition->location, 3019, "if statement conditional expressions must evaluate to a scalar");

					return false;
				}

				if (!ParseStatement(newstatement->StatementOnTrue))
				{
					return false;
				}

				statement = newstatement;

				if (Accept(lexer::tokenid::else_))
				{
					return ParseStatement(newstatement->StatementOnFalse);
				}

				return true;
			}
			#pragma endregion

			#pragma region Switch
			if (Accept(lexer::tokenid::switch_))
			{
				const auto newstatement = _ast->make_node<Nodes::Switch>(_token.location);
				newstatement->Attributes = attributes;

				if (!(Expect('(') && ParseExpression(newstatement->Test) && Expect(')')))
				{
					return false;
				}

				if (!newstatement->Test->Type.IsScalar())
				{
					Error(newstatement->Test->location, 3019, "switch statement expression must evaluate to a scalar");

					return false;
				}

				if (!Expect('{'))
				{
					return false;
				}

				while (!Peek('}') && !Peek(lexer::tokenid::end_of_stream))
				{
					const auto casenode = _ast->make_node<Nodes::Case>(struct location());

					while (Accept(lexer::tokenid::case_) || Accept(lexer::tokenid::default_))
					{
						Nodes::Expression *label = nullptr;

						if (_token.id == lexer::tokenid::case_)
						{
							if (!ParseExpression(label))
							{
								return false;
							}

							if (label->id != nodeid::Literal || !label->Type.IsNumeric())
							{
								Error(label->location, 3020, "non-numeric case expression");

								return false;
							}
						}

						if (!Expect(':'))
						{
							return false;
						}

						casenode->Labels.push_back(static_cast<Nodes::Literal *>(label));
					}

					if (casenode->Labels.empty())
					{
						return false;
					}

					casenode->location = casenode->Labels[0]->location;

					if (!ParseStatement(casenode->Statements))
					{
						return false;
					}

					newstatement->Cases.push_back(casenode);
				}

				if (newstatement->Cases.empty())
				{
					Warning(newstatement->location, 5002, "switch statement contains no 'case' or 'default' labels");

					statement = nullptr;
				}
				else
				{
					statement = newstatement;
				}

				return Expect('}');
			}
			#pragma endregion

			#pragma region For
			if (Accept(lexer::tokenid::for_))
			{
				const auto newstatement = _ast->make_node<Nodes::For>(_token.location);
				newstatement->Attributes = attributes;

				if (!Expect('('))
				{
					return false;
				}

				EnterScope();

				if (!ParseStatementDeclaratorList(newstatement->Initialization))
				{
					Nodes::Expression *expression = nullptr;

					if (ParseExpression(expression))
					{
						const auto initialization = _ast->make_node<Nodes::ExpressionStatement>(expression->location);
						initialization->Expression = expression;

						newstatement->Initialization = initialization;
					}
				}

				if (!Expect(';'))
				{
					LeaveScope();

					return false;
				}

				ParseExpression(newstatement->Condition);

				if (!Expect(';'))
				{
					LeaveScope();

					return false;
				}

				ParseExpression(newstatement->Increment);

				if (!Expect(')'))
				{
					LeaveScope();

					return false;
				}

				if (!newstatement->Condition->Type.IsScalar())
				{
					Error(newstatement->Condition->location, 3019, "scalar value expected");

					return false;
				}

				if (!ParseStatement(newstatement->Statements, false))
				{
					LeaveScope();

					return false;
				}

				LeaveScope();

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region While
			if (Accept(lexer::tokenid::while_))
			{
				const auto newstatement = _ast->make_node<Nodes::While>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->DoWhile = false;

				EnterScope();

				if (!(Expect('(') && ParseExpression(newstatement->Condition) && Expect(')')))
				{
					LeaveScope();

					return false;
				}

				if (!newstatement->Condition->Type.IsScalar())
				{
					Error(newstatement->Condition->location, 3019, "scalar value expected");

					LeaveScope();

					return false;
				}

				if (!ParseStatement(newstatement->Statements, false))
				{
					LeaveScope();

					return false;
				}

				LeaveScope();

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region DoWhile
			if (Accept(lexer::tokenid::do_))
			{
				const auto newstatement = _ast->make_node<Nodes::While>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->DoWhile = true;

				if (!(ParseStatement(newstatement->Statements) && Expect(lexer::tokenid::while_) && Expect('(') && ParseExpression(newstatement->Condition) && Expect(')') && Expect(';')))
				{
					return false;
				}

				if (!newstatement->Condition->Type.IsScalar())
				{
					Error(newstatement->Condition->location, 3019, "scalar value expected");

					return false;
				}

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region Break
			if (Accept(lexer::tokenid::break_))
			{
				const auto newstatement = _ast->make_node<Nodes::Jump>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->Mode = Nodes::Jump::Break;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Continue
			if (Accept(lexer::tokenid::continue_))
			{
				const auto newstatement = _ast->make_node<Nodes::Jump>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->Mode = Nodes::Jump::Continue;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Return
			if (Accept(lexer::tokenid::return_))
			{
				const auto newstatement = _ast->make_node<Nodes::Return>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->Discard = false;

				const auto parent = static_cast<const Nodes::Function *>(_parentStack.top());

				if (!Peek(';'))
				{
					if (!ParseExpression(newstatement->Value))
					{
						return false;
					}

					if (parent->ReturnType.IsVoid())
					{
						Error(newstatement->location, 3079, "void functions cannot return a value");

						Accept(';');

						return false;
					}

					if (!GetTypeRank(newstatement->Value->Type, parent->ReturnType))
					{
						Error(newstatement->location, 3017, "expression does not match function return type");

						return false;
					}

					if (newstatement->Value->Type.Rows > parent->ReturnType.Rows || newstatement->Value->Type.Cols > parent->ReturnType.Cols)
					{
						Warning(newstatement->location, 3206, "implicit truncation of vector type");
					}
				}
				else if (!parent->ReturnType.IsVoid())
				{
					Error(newstatement->location, 3080, "function must return a value");

					Accept(';');

					return false;
				}

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Discard
			if (Accept(lexer::tokenid::discard_))
			{
				const auto newstatement = _ast->make_node<Nodes::Return>(_token.location);
				newstatement->Attributes = attributes;
				newstatement->Discard = true;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Declaration
			if (ParseStatementDeclaratorList(statement))
			{
				statement->Attributes = attributes;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Expression
			Nodes::Expression *expression = nullptr;

			if (ParseExpression(expression))
			{
				const auto newstatement = _ast->make_node<Nodes::ExpressionStatement>(expression->location);
				newstatement->Attributes = attributes;
				newstatement->Expression = expression;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			Error(_tokenNext.location, 3000, "syntax error: unexpected '%s'", GetTokenName(_tokenNext.id));

			ConsumeUntil(';');

			return false;
		}
		bool Parser::ParseStatementBlock(Nodes::Statement *&statement, bool scoped)
		{
			if (!Expect('{'))
			{
				return false;
			}

			const auto compound = _ast->make_node<Nodes::Compound>(_token.location);

			if (scoped)
			{
				EnterScope();
			}

			while (!Peek('}') && !Peek(lexer::tokenid::end_of_stream))
			{
				Nodes::Statement *compoundStatement = nullptr;

				if (!ParseStatement(compoundStatement))
				{
					if (scoped)
					{
						LeaveScope();
					}

					unsigned level = 0;

					while (!Peek(lexer::tokenid::end_of_stream))
					{
						if (Accept('{'))
						{
							++level;
						}
						else if (Accept('}'))
						{
							if (level-- == 0)
							{
								break;
							}
						}
						else
						{
							Consume();
						}
					}

					return false;
				}

				compound->Statements.push_back(compoundStatement);
			}

			if (scoped)
			{
				LeaveScope();
			}

			statement = compound;

			return Expect('}');
		}
		bool Parser::ParseStatementDeclaratorList(Nodes::Statement *&statement)
		{
			Nodes::Type type;

			const auto location = _tokenNext.location;

			if (!ParseType(type))
			{
				return false;
			}

			unsigned int count = 0;
			const auto declarators = _ast->make_node<Nodes::DeclaratorList>(location);

			do
			{
				if (count++ > 0 && !Expect(','))
				{
					return false;
				}

				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				Nodes::Variable *declarator = nullptr;

				if (!ParseVariableResidue(type, _token.literal_as_string, declarator))
				{
					return false;
				}

				declarators->Declarators.push_back(std::move(declarator));
			}
			while (!Peek(';'));

			statement = declarators;

			return true;
		}

		// Declarations
		bool Parser::Parse()
		{
			Consume();

			while (!Peek(lexer::tokenid::end_of_stream))
			{
				if (!ParseTopLevel())
				{
					return false;
				}
			}

			return true;
		}
		bool Parser::ParseTopLevel()
		{
			Nodes::Type type = { Nodes::Type::Class::Void };

			if (Peek(lexer::tokenid::namespace_))
			{
				return ParseNamespace();
			}
			else if (Peek(lexer::tokenid::struct_))
			{
				Nodes::Struct *structure = nullptr;

				if (!ParseStruct(structure))
				{
					return false;
				}

				if (!Expect(';'))
				{
					return false;
				}
			}
			else if (Peek(lexer::tokenid::technique))
			{
				Nodes::Technique *technique = nullptr;

				if (!ParseTechnique(technique))
				{
					return false;
				}

				_ast->techniques.push_back(std::move(technique));
			}
			else if (ParseType(type))
			{
				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				if (Peek('('))
				{
					Nodes::Function *function = nullptr;

					if (!ParseFunctionResidue(type, _token.literal_as_string, function))
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
						if (count++ > 0 && !(Expect(',') && Expect(lexer::tokenid::identifier)))
						{
							return false;
						}

						Nodes::Variable *variable = nullptr;

						if (!ParseVariableResidue(type, _token.literal_as_string, variable, true))
						{
							ConsumeUntil(';');

							return false;
						}

						_ast->uniforms.push_back(std::move(variable));
					}
					while (!Peek(';'));

					if (!Expect(';'))
					{
						return false;
					}
				}
			}
			else if (!Accept(';'))
			{
				Consume();

				Error(_token.location, 3000, "syntax error: unexpected '%s'", GetTokenName(_token.id));

				return false;
			}

			return true;
		}
		bool Parser::ParseNamespace()
		{
			if (!Accept(lexer::tokenid::namespace_))
			{
				return false;
			}

			if (!Expect(lexer::tokenid::identifier))
			{
				return false;
			}

			const auto name = _token.literal_as_string;

			if (!Expect('{'))
			{
				return false;
			}

			EnterNamespace(name);

			bool success = true;

			while (!Peek('}'))
			{
				if (!ParseTopLevel())
				{
					success = false;
					break;
				}
			}

			LeaveNamespace();

			return success && Expect('}');
		}
		bool Parser::ParseArray(int &size)
		{
			size = 0;

			if (Accept('['))
			{
				Nodes::Expression *expression;

				if (Accept(']'))
				{
					size = -1;

					return true;
				}
				if (ParseExpression(expression) && Expect(']'))
				{
					if (expression->id != nodeid::Literal || !(expression->Type.IsScalar() && expression->Type.IsIntegral()))
					{
						Error(expression->location, 3058, "array dimensions must be literal scalar expressions");

						return false;
					}

					size = static_cast<Nodes::Literal *>(expression)->Value.Int[0];

					if (size < 1 || size > 65536)
					{
						Error(expression->location, 3059, "array dimension must be between 1 and 65536");

						return false;
					}

					return true;
				}
			}

			return false;
		}
		bool Parser::ParseAnnotations(std::vector<Nodes::Annotation> &annotations)
		{
			if (!Accept('<'))
			{
				return false;
			}

			while (!Peek('>'))
			{
				Nodes::Type type;

				AcceptTypeClass(type);

				Nodes::Annotation annotation;

				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				annotation.Name = _token.literal_as_string;
				annotation.location = _token.location;

				Nodes::Expression *expression = nullptr;

				if (!(Expect('=') && ParseExpressionAssignment(expression) && Expect(';')))
				{
					return false;
				}

				if (expression->id != nodeid::Literal)
				{
					Error(expression->location, 3011, "value must be a literal expression");

					continue;
				}

				annotation.Value = static_cast<Nodes::Literal *>(expression);

				annotations.push_back(std::move(annotation));
			}

			return Expect('>');
		}
		bool Parser::ParseStruct(Nodes::Struct *&structure)
		{
			if (!Accept(lexer::tokenid::struct_))
			{
				return false;
			}

			structure = _ast->make_node<Nodes::Struct>(_token.location);
			structure->Namespace = _currentScope.Name;

			if (Accept(lexer::tokenid::identifier))
			{
				structure->Name = _token.literal_as_string;

				if (!InsertSymbol(structure, true))
				{
					Error(_token.location, 3003, "redefinition of '%s'", structure->Name.c_str());

					return false;
				}
			}
			else
			{
				structure->Name = "__anonymous_struct_" + std::to_string(structure->location.line) + '_' + std::to_string(structure->location.column);
			}

			if (!Expect('{'))
			{
				return false;
			}

			while (!Peek('}'))
			{
				Nodes::Type type;

				if (!ParseType(type))
				{
					Error(_tokenNext.location, 3000, "syntax error: unexpected '%s', expected struct member type", GetTokenName(_tokenNext.id));

					ConsumeUntil('}');

					return false;
				}

				if (type.IsVoid())
				{
					Error(_tokenNext.location, 3038, "struct members cannot be void");

					ConsumeUntil('}');

					return false;
				}
				if (type.HasQualifier(Nodes::Type::Qualifier::In) || type.HasQualifier(Nodes::Type::Qualifier::Out))
				{
					Error(_tokenNext.location, 3055, "struct members cannot be declared 'in' or 'out'");

					ConsumeUntil('}');

					return false;
				}

				unsigned int count = 0;

				do
				{
					if (count++ > 0 && !Expect(','))
					{
						ConsumeUntil('}');

						return false;
					}

					if (!Expect(lexer::tokenid::identifier))
					{
						ConsumeUntil('}');

						return false;
					}

					const auto field = _ast->make_node<Nodes::Variable>(_token.location);
					field->Name = _token.literal_as_string;
					field->Type = type;

					ParseArray(field->Type.ArrayLength);

					if (Accept(':'))
					{
						if (!Expect(lexer::tokenid::identifier))
						{
							ConsumeUntil('}');

							return false;
						}

						field->Semantic = _token.literal_as_string;
						boost::to_upper(field->Semantic);
					}

					structure->Fields.push_back(std::move(field));
				}
				while (!Peek(';'));

				if (!Expect(';'))
				{
					ConsumeUntil('}');

					return false;
				}
			}

			if (structure->Fields.empty())
			{
				Warning(structure->location, 5001, "struct has no members");
			}

			_ast->structs.push_back(structure);

			return Expect('}');
		}
		bool Parser::ParseFunctionResidue(Nodes::Type &type, std::string name, Nodes::Function *&function)
		{
			const auto location = _token.location;

			if (!Expect('('))
			{
				return false;
			}

			if (type.Qualifiers != 0)
			{
				Error(location, 3047, "function return type cannot have any qualifiers");

				return false;
			}

			function = _ast->make_node<Nodes::Function>(location);
			function->ReturnType = type;
			function->ReturnType.Qualifiers = Nodes::Type::Qualifier::Const;
			function->Name = name;
			function->Namespace = _currentScope.Name;

			InsertSymbol(function, true);

			EnterScope(function);

			while (!Peek(')'))
			{
				if (!function->Parameters.empty() && !Expect(','))
				{
					LeaveScope();

					return false;
				}

				const auto parameter = _ast->make_node<Nodes::Variable>(struct location());

				if (!ParseType(parameter->Type))
				{
					LeaveScope();

					Error(_tokenNext.location, 3000, "syntax error: unexpected '%s', expected parameter type", GetTokenName(_tokenNext.id));

					return false;
				}

				if (!Expect(lexer::tokenid::identifier))
				{
					LeaveScope();

					return false;
				}

				parameter->Name = _token.literal_as_string;
				parameter->location = _token.location;

				if (parameter->Type.IsVoid())
				{
					Error(parameter->location, 3038, "function parameters cannot be void");

					LeaveScope();

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Extern))
				{
					Error(parameter->location, 3006, "function parameters cannot be declared 'extern'");

					LeaveScope();

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Static))
				{
					Error(parameter->location, 3007, "function parameters cannot be declared 'static'");

					LeaveScope();

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					Error(parameter->location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");

					LeaveScope();

					return false;
				}

				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Out))
				{
					if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Const))
					{
						Error(parameter->location, 3046, "output parameters cannot be declared 'const'");

						LeaveScope();

						return false;
					}
				}
				else
				{
					parameter->Type.Qualifiers |= Nodes::Type::Qualifier::In;
				}

				ParseArray(parameter->Type.ArrayLength);

				if (!InsertSymbol(parameter))
				{
					Error(parameter->location, 3003, "redefinition of '%s'", parameter->Name.c_str());

					LeaveScope();

					return false;
				}

				if (Accept(':'))
				{
					if (!Expect(lexer::tokenid::identifier))
					{
						LeaveScope();

						return false;
					}

					parameter->Semantic = _token.literal_as_string;
					boost::to_upper(parameter->Semantic);
				}

				function->Parameters.push_back(parameter);
			}

			if (!Expect(')'))
			{
				LeaveScope();

				return false;
			}

			if (Accept(':'))
			{
				if (!Expect(lexer::tokenid::identifier))
				{
					LeaveScope();

					return false;
				}

				function->ReturnSemantic = _token.literal_as_string;
				boost::to_upper(function->ReturnSemantic);

				if (type.IsVoid())
				{
					Error(_token.location, 3076, "void function cannot have a semantic");

					return false;
				}
			}

			if (!ParseStatementBlock(reinterpret_cast<Nodes::Statement *&>(function->Definition)))
			{
				LeaveScope();

				return false;
			}

			LeaveScope();

			return true;
		}
		bool Parser::ParseVariableResidue(Nodes::Type &type, std::string name, Nodes::Variable *&variable, bool global)
		{
			auto location = _token.location;

			if (type.IsVoid())
			{
				Error(location, 3038, "variables cannot be void");

				return false;
			}
			if (type.HasQualifier(Nodes::Type::Qualifier::In) || type.HasQualifier(Nodes::Type::Qualifier::Out))
			{
				Error(location, 3055, "variables cannot be declared 'in' or 'out'");

				return false;
			}

			const auto parent = _parentStack.empty() ? nullptr : _parentStack.top();

			if (parent == nullptr)
			{
				if (!type.HasQualifier(Nodes::Type::Qualifier::Static))
				{
					if (!type.HasQualifier(Nodes::Type::Qualifier::Uniform) && !(type.IsTexture() || type.IsSampler()))
					{
						Warning(location, 5000, "global variables are considered 'uniform' by default");
					}

					type.Qualifiers |= Nodes::Type::Qualifier::Extern | Nodes::Type::Qualifier::Uniform;
				}
			}
			else
			{
				if (type.HasQualifier(Nodes::Type::Qualifier::Extern))
				{
					Error(location, 3006, "local variables cannot be declared 'extern'");

					return false;
				}
				if (type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					Error(location, 3047, "local variables cannot be declared 'uniform'");

					return false;
				}

				if (type.IsTexture() || type.IsSampler())
				{
					Error(location, 3038, "local variables cannot be textures or samplers");

					return false;
				}
			}

			ParseArray(type.ArrayLength);

			variable = _ast->make_node<Nodes::Variable>(location);
			variable->Type = type;
			variable->Name = name;

			if (global)
			{
				variable->Namespace = _currentScope.Name;
			}

			if (!InsertSymbol(variable, global))
			{
				Error(location, 3003, "redefinition of '%s'", name.c_str());

				return false;
			}

			if (Accept(':'))
			{
				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				variable->Semantic = _token.literal_as_string;
				boost::to_upper(variable->Semantic);
			}

			ParseAnnotations(variable->Annotations);

			if (Accept('='))
			{
				location = _token.location;

				if (!ParseVariableAssignment(variable->Initializer))
				{
					return false;
				}

				if (parent == nullptr && variable->Initializer->id != nodeid::Literal)
				{
					Error(location, 3011, "initial value must be a literal expression");

					return false;
				}

				if (variable->Initializer->id == nodeid::InitializerList && type.IsNumeric())
				{
					const auto nullval = _ast->make_node<Nodes::Literal>(location);
					nullval->Type.BaseClass = type.BaseClass;
					nullval->Type.Qualifiers = Nodes::Type::Qualifier::Const;
					nullval->Type.Rows = type.Rows, nullval->Type.Cols = type.Cols, nullval->Type.ArrayLength = 0;

					const auto initializerlist = static_cast<Nodes::InitializerList *>(variable->Initializer);

					while (initializerlist->Type.ArrayLength < type.ArrayLength)
					{
						initializerlist->Type.ArrayLength++;
						initializerlist->Values.push_back(nullval);
					}
				}

				if (!GetTypeRank(variable->Initializer->Type, type))
				{
					Error(location, 3017, "initial value does not match variable type");

					return false;
				}
				if ((variable->Initializer->Type.Rows < type.Rows || variable->Initializer->Type.Cols < type.Cols) && !variable->Initializer->Type.IsScalar())
				{
					Error(location, 3017, "cannot implicitly convert these vector types");

					return false;
				}

				if (variable->Initializer->Type.Rows > type.Rows || variable->Initializer->Type.Cols > type.Cols)
				{
					Warning(location, 3206, "implicit truncation of vector type");
				}
			}
			else if (type.IsNumeric())
			{
				if (type.HasQualifier(Nodes::Type::Qualifier::Const))
				{
					Error(location, 3012, "missing initial value for '%s'", name.c_str());

					return false;
				}
			}
			else if (Peek('{'))
			{
				if (!ParseVariableProperties(variable))
				{
					return false;
				}
			}

			return true;
		}
		bool Parser::ParseVariableAssignment(Nodes::Expression *&expression)
		{
			if (Accept('{'))
			{
				const auto initializerlist = _ast->make_node<Nodes::InitializerList>(_token.location);

				while (!Peek('}'))
				{
					if (!initializerlist->Values.empty() && !Expect(','))
					{
						return false;
					}

					if (Peek('}'))
					{
						break;
					}

					if (!ParseVariableAssignment(expression))
					{
						ConsumeUntil('}');

						return false;
					}

					if (expression->id == nodeid::InitializerList && static_cast<Nodes::InitializerList *>(expression)->Values.empty())
					{
						continue;
					}

					initializerlist->Values.push_back(expression);
				}

				if (!initializerlist->Values.empty())
				{
					initializerlist->Type = initializerlist->Values[0]->Type;
					initializerlist->Type.ArrayLength = static_cast<int>(initializerlist->Values.size());
				}

				expression = initializerlist;

				return Expect('}');
			}
			else if (ParseExpressionAssignment(expression))
			{
				return true;
			}

			return false;
		}
		bool Parser::ParseVariableProperties(Nodes::Variable *variable)
		{
			if (!Expect('{'))
			{
				return false;
			}

			while (!Peek('}'))
			{
				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				const auto name = _token.literal_as_string;
				const auto location = _token.location;

				Nodes::Expression *value = nullptr;

				if (!(Expect('=') && ParseVariablePropertiesExpression(value) && Expect(';')))
				{
					return false;
				}

				if (name == "Texture")
				{
					if (value->id != nodeid::LValue || static_cast<Nodes::LValue *>(value)->Reference->id != nodeid::Variable || !static_cast<Nodes::LValue *>(value)->Reference->Type.IsTexture() || static_cast<Nodes::LValue *>(value)->Reference->Type.IsArray())
					{
						Error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					variable->Properties.Texture = static_cast<Nodes::LValue *>(value)->Reference;
				}
				else
				{
					if (value->id != nodeid::Literal)
					{
						Error(location, 3011, "value must be a literal expression");

						return false;
					}

					const auto valueLiteral = static_cast<Nodes::Literal *>(value);

					if (name == "Width")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.Width);
					}
					else if (name == "Height")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.Height);
					}
					else if (name == "Depth")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.Depth);
					}
					else if (name == "MipLevels")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MipLevels);
					}
					else if (name == "Format")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.Format);
					}
					else if (name == "SRGBTexture" || name == "SRGBReadEnable")
					{
						variable->Properties.SRGBTexture = valueLiteral->Value.Int[0] != 0;
					}
					else if (name == "AddressU")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.AddressU);
					}
					else if (name == "AddressV")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.AddressV);
					}
					else if (name == "AddressW")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.AddressW);
					}
					else if (name == "MinFilter")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MinFilter);
					}
					else if (name == "MagFilter")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MagFilter);
					}
					else if (name == "MipFilter")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MipFilter);
					}
					else if (name == "MaxAnisotropy")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MaxAnisotropy);
					}
					else if (name == "MinLOD" || name == "MaxMipLevel")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MinLOD);
					}
					else if (name == "MaxLOD")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MaxLOD);
					}
					else if (name == "MipLODBias" || name == "MipMapLodBias")
					{
						ScalarLiteralCast(valueLiteral, 0, variable->Properties.MipLODBias);
					}
					else
					{
						Error(location, 3004, "unrecognized property '%s'", name.c_str());

						return false;
					}
				}
			}

			if (!Expect('}'))
			{
				return false;
			}

			return true;
		}
		bool Parser::ParseVariablePropertiesExpression(Nodes::Expression *&expression)
		{
			Backup();

			if (Accept(lexer::tokenid::identifier))
			{
				const auto identifier = _token.literal_as_string;
				const auto location = _token.location;

				static const std::unordered_map<std::string, unsigned int> sEnums = boost::assign::map_list_of
					("NONE", Nodes::Variable::Properties::NONE)
					("POINT", Nodes::Variable::Properties::POINT)
					("LINEAR", Nodes::Variable::Properties::LINEAR)
					("ANISOTROPIC", Nodes::Variable::Properties::ANISOTROPIC)
					("CLAMP", Nodes::Variable::Properties::CLAMP)
					("WRAP", Nodes::Variable::Properties::REPEAT)
					("REPEAT", Nodes::Variable::Properties::REPEAT)
					("MIRROR", Nodes::Variable::Properties::MIRROR)
					("BORDER", Nodes::Variable::Properties::BORDER)
					("R8", Nodes::Variable::Properties::R8)
					("R16F", Nodes::Variable::Properties::R16F)
					("R32F", Nodes::Variable::Properties::R32F)
					("RG8", Nodes::Variable::Properties::RG8)
					("R8G8", Nodes::Variable::Properties::RG8)
					("RG16", Nodes::Variable::Properties::RG16)
					("R16G16", Nodes::Variable::Properties::RG16)
					("RG16F", Nodes::Variable::Properties::RG16F)
					("R16G16F", Nodes::Variable::Properties::RG16F)
					("RG32F", Nodes::Variable::Properties::RG32F)
					("R32G32F", Nodes::Variable::Properties::RG32F)
					("RGBA8", Nodes::Variable::Properties::RGBA8)
					("R8G8B8A8", Nodes::Variable::Properties::RGBA8)
					("RGBA16", Nodes::Variable::Properties::RGBA16)
					("R16G16B16A16", Nodes::Variable::Properties::RGBA16)
					("RGBA16F", Nodes::Variable::Properties::RGBA16F)
					("R16G16B16A16F", Nodes::Variable::Properties::RGBA16F)
					("RGBA32F", Nodes::Variable::Properties::RGBA32F)
					("R32G32B32A32F", Nodes::Variable::Properties::RGBA32F)
					("DXT1", Nodes::Variable::Properties::DXT1)
					("DXT3", Nodes::Variable::Properties::DXT3)
					("DXT4", Nodes::Variable::Properties::DXT5)
					("LATC1", Nodes::Variable::Properties::LATC1)
					("LATC2", Nodes::Variable::Properties::LATC2);

				const auto it = sEnums.find(boost::to_upper_copy(identifier));

				if (it != sEnums.end())
				{
					const auto newexpression = _ast->make_node<Nodes::Literal>(location);
					newexpression->Type.BaseClass = Nodes::Type::Class::Uint;
					newexpression->Type.Rows = newexpression->Type.Cols = 1, newexpression->Type.ArrayLength = 0;
					newexpression->Value.Uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				Restore();
			}

			return ParseExpressionMultary(expression);
		}
		bool Parser::ParseTechnique(Nodes::Technique *&technique)
		{
			if (!Accept(lexer::tokenid::technique))
			{
				return false;
			}

			const auto location = _token.location;

			if (!Expect(lexer::tokenid::identifier))
			{
				return false;
			}

			technique = _ast->make_node<Nodes::Technique>(location);
			technique->Name = _token.literal_as_string;
			technique->Namespace = _currentScope.Name;

			ParseAnnotations(technique->Annotations);

			if (!Expect('{'))
			{
				return false;
			}

			while (!Peek('}'))
			{
				Nodes::Pass *pass = nullptr;

				if (!ParseTechniquePass(pass))
				{
					return false;
				}

				technique->Passes.push_back(std::move(pass));
			}

			return Expect('}');
		}
		bool Parser::ParseTechniquePass(Nodes::Pass *&pass)
		{
			if (!Accept(lexer::tokenid::pass))
			{
				return false;
			}

			pass = _ast->make_node<Nodes::Pass>(_token.location);

			if (Accept(lexer::tokenid::identifier))
			{
				pass->Name = _token.literal_as_string;
			}

			ParseAnnotations(pass->Annotations);

			if (!Expect('{'))
			{
				return false;
			}

			while (!Peek('}'))
			{
				if (!Expect(lexer::tokenid::identifier))
				{
					return false;
				}

				const auto passstate = _token.literal_as_string;
				const auto location = _token.location;

				Nodes::Expression *value = nullptr;

				if (!(Expect('=') && ParseTechniquePassExpression(value) && Expect(';')))
				{
					return false;
				}

				if (passstate == "VertexShader" || passstate == "PixelShader")
				{
					if (value->id != nodeid::LValue || static_cast<Nodes::LValue *>(value)->Reference->id != nodeid::Function)
					{
						Error(location, 3020, "type mismatch, expected function name");

						return false;
					}

					(passstate[0] == 'V' ? pass->States.VertexShader : pass->States.PixelShader) = reinterpret_cast<const Nodes::Function *>(static_cast<Nodes::LValue *>(value)->Reference);
				}
				else if (boost::starts_with(passstate, "RenderTarget") && (passstate == "RenderTarget" || (passstate[12] >= '0' && passstate[12] < '8')))
				{
					size_t index = 0;

					if (passstate.size() == 13)
					{
						index = passstate[12] - '0';
					}

					if (value->id != nodeid::LValue || static_cast<Nodes::LValue *>(value)->Reference->id != nodeid::Variable || static_cast<Nodes::LValue *>(value)->Reference->Type.BaseClass != Nodes::Type::Class::Texture2D || static_cast<Nodes::LValue *>(value)->Reference->Type.IsArray())
					{
						Error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					pass->States.RenderTargets[index] = static_cast<Nodes::LValue *>(value)->Reference;
				}
				else
				{
					if (value->id != nodeid::Literal)
					{
						Error(location, 3011, "pass state value must be a literal expression");

						return false;
					}

					const auto valueLiteral = static_cast<Nodes::Literal *>(value);

					if (passstate == "SRGBWriteEnable")
					{
						pass->States.SRGBWriteEnable = valueLiteral->Value.Int[0] != 0;
					}
					else if (passstate == "BlendEnable" || passstate == "AlphaBlendEnable")
					{
						pass->States.BlendEnable = valueLiteral->Value.Int[0] != 0;
					}
					else if (passstate == "DepthEnable" || passstate == "ZEnable")
					{
						pass->States.DepthEnable = valueLiteral->Value.Int[0] != 0;
					}
					else if (passstate == "StencilEnable")
					{
						pass->States.StencilEnable = valueLiteral->Value.Int[0] != 0;
					}
					else if (passstate == "RenderTargetWriteMask" || passstate == "ColorWriteMask")
					{
						unsigned int mask = 0;
						ScalarLiteralCast(valueLiteral, 0, mask);

						pass->States.RenderTargetWriteMask = mask & 0xFF;
					}
					else if (passstate == "DepthWriteMask" || passstate == "ZWriteEnable")
					{
						pass->States.DepthWriteMask = valueLiteral->Value.Int[0] != 0;
					}
					else if (passstate == "StencilReadMask" || passstate == "StencilMask")
					{
						unsigned int mask = 0;
						ScalarLiteralCast(valueLiteral, 0, mask);

						pass->States.StencilReadMask = mask & 0xFF;
					}
					else if (passstate == "StencilWriteMask")
					{
						unsigned int mask = 0;
						ScalarLiteralCast(valueLiteral, 0, mask);

						pass->States.StencilWriteMask = mask & 0xFF;
					}
					else if (passstate == "BlendOp")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.BlendOp);
					}
					else if (passstate == "BlendOpAlpha")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.BlendOpAlpha);
					}
					else if (passstate == "SrcBlend")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.SrcBlend);
					}
					else if (passstate == "DestBlend")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.DestBlend);
					}
					else if (passstate == "DepthFunc" || passstate == "ZFunc")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.DepthFunc);
					}
					else if (passstate == "StencilFunc")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.StencilFunc);
					}
					else if (passstate == "StencilRef")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.StencilRef);
					}
					else if (passstate == "StencilPass" || passstate == "StencilPassOp")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.StencilOpPass);
					}
					else if (passstate == "StencilFail" || passstate == "StencilFailOp")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.StencilOpFail);
					}
					else if (passstate == "StencilZFail" || passstate == "StencilDepthFail" || passstate == "StencilDepthFailOp")
					{
						ScalarLiteralCast(valueLiteral, 0, pass->States.StencilOpDepthFail);
					}
					else
					{
						Error(location, 3004, "unrecognized pass state '%s'", passstate.c_str());

						return false;
					}
				}
			}

			return Expect('}');
		}
		bool Parser::ParseTechniquePassExpression(Nodes::Expression *&expression)
		{
			Scope scope;
			bool exclusive;

			if (Accept(lexer::tokenid::colon_colon))
			{
				scope.NamespaceLevel = scope.Level = 0;
				exclusive = true;
			}
			else
			{
				scope = _currentScope;
				exclusive = false;
			}

			if (exclusive ? Expect(lexer::tokenid::identifier) : Accept(lexer::tokenid::identifier))
			{
				auto identifier = _token.literal_as_string;
				const auto location = _token.location;

				static const std::unordered_map<std::string, unsigned int> sEnums = boost::assign::map_list_of
					("NONE", Nodes::Pass::States::NONE)
					("ZERO", Nodes::Pass::States::ZERO)
					("ONE", Nodes::Pass::States::ONE)
					("SRCCOLOR", Nodes::Pass::States::SRCCOLOR)
					("SRCALPHA", Nodes::Pass::States::SRCALPHA)
					("INVSRCCOLOR", Nodes::Pass::States::INVSRCCOLOR)
					("INVSRCALPHA", Nodes::Pass::States::INVSRCALPHA)
					("DESTCOLOR", Nodes::Pass::States::DESTCOLOR)
					("DESTALPHA", Nodes::Pass::States::DESTALPHA)
					("INVDESTCOLOR", Nodes::Pass::States::INVDESTCOLOR)
					("INVDESTALPHA", Nodes::Pass::States::INVDESTALPHA)
					("ADD", Nodes::Pass::States::ADD)
					("SUBTRACT", Nodes::Pass::States::SUBTRACT)
					("REVSUBTRACT", Nodes::Pass::States::REVSUBTRACT)
					("MIN", Nodes::Pass::States::MIN)
					("MAX", Nodes::Pass::States::MAX)
					("KEEP", Nodes::Pass::States::KEEP)
					("REPLACE", Nodes::Pass::States::REPLACE)
					("INVERT", Nodes::Pass::States::INVERT)
					("INCR", Nodes::Pass::States::INCR)
					("INCRSAT", Nodes::Pass::States::INCRSAT)
					("DECR", Nodes::Pass::States::DECR)
					("DECRSAT", Nodes::Pass::States::DECRSAT)
					("NEVER", Nodes::Pass::States::NEVER)
					("ALWAYS", Nodes::Pass::States::ALWAYS)
					("LESS", Nodes::Pass::States::LESS)
					("GREATER", Nodes::Pass::States::GREATER)
					("LEQUAL", Nodes::Pass::States::LESSEQUAL)
					("LESSEQUAL", Nodes::Pass::States::LESSEQUAL)
					("GEQUAL", Nodes::Pass::States::GREATEREQUAL)
					("GREATEREQUAL", Nodes::Pass::States::GREATEREQUAL)
					("EQUAL", Nodes::Pass::States::EQUAL)
					("NEQUAL", Nodes::Pass::States::NOTEQUAL)
					("NOTEQUAL", Nodes::Pass::States::NOTEQUAL);

				const auto it = sEnums.find(boost::to_upper_copy(identifier));

				if (it != sEnums.end())
				{
					const auto newexpression = _ast->make_node<Nodes::Literal>(location);
					newexpression->Type.BaseClass = Nodes::Type::Class::Uint;
					newexpression->Type.Rows = newexpression->Type.Cols = 1, newexpression->Type.ArrayLength = 0;
					newexpression->Value.Uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				while (Accept(lexer::tokenid::colon_colon) && Expect(lexer::tokenid::identifier))
				{
					identifier += "::" + _token.literal_as_string;
				}

				const auto symbol = FindSymbol(identifier, scope, exclusive);

				if (symbol == nullptr)
				{
					Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

					return false;
				}

				const auto newexpression = _ast->make_node<Nodes::LValue>(location);
				newexpression->Reference = static_cast<const Nodes::Variable *>(symbol);
				newexpression->Type = symbol->id == nodeid::Function ? static_cast<const Nodes::Function *>(symbol)->ReturnType : newexpression->Reference->Type;

				expression = newexpression;

				return true;
			}

			return ParseExpressionMultary(expression);
		}

		// Symbol Table
		void Parser::EnterScope(Symbol *parent)
		{
			if (parent != nullptr || _parentStack.empty())
			{
				_parentStack.push(parent);
			}
			else
			{
				_parentStack.push(_parentStack.top());
			}

			_currentScope.Level++;
		}
		void Parser::EnterNamespace(const std::string &name)
		{
			_currentScope.Name += name + "::";
			_currentScope.Level++;
			_currentScope.NamespaceLevel++;
		}
		void Parser::LeaveScope()
		{
			assert(_currentScope.Level > 0);

			for (auto it1 = _symbolStack.begin(), end = _symbolStack.end(); it1 != end; ++it1)
			{
				auto &scopes = it1->second;

				if (scopes.empty())
				{
					continue;
				}

				for (auto it2 = scopes.begin(); it2 != scopes.end();)
				{
					if (it2->first.Level > it2->first.NamespaceLevel && it2->first.Level >= _currentScope.Level)
					{
						it2 = scopes.erase(it2);
					}
					else
					{
						++it2;
					}
				}
			}

			_parentStack.pop();

			_currentScope.Level--;
		}
		void Parser::LeaveNamespace()
		{
			assert(_currentScope.Level > 0);
			assert(_currentScope.NamespaceLevel > 0);

			_currentScope.Name.erase(_currentScope.Name.substr(0, _currentScope.Name.size() - 2).rfind("::") + 2);
			_currentScope.Level--;
			_currentScope.NamespaceLevel--;
		}
		bool Parser::InsertSymbol(Symbol *symbol, bool global)
		{
			if (symbol->id != nodeid::Function && FindSymbol(symbol->Name, _currentScope, true))
			{
				return false;
			}

			if (global)
			{
				Scope scope = { "", 0, 0 };

				for (size_t pos = 0; pos != std::string::npos; pos = _currentScope.Name.find("::", pos))
				{
					pos += 2;

					scope.Name = _currentScope.Name.substr(0, pos);

					_symbolStack[_currentScope.Name.substr(pos) + symbol->Name].emplace_back(scope, symbol);

					scope.Level = ++scope.NamespaceLevel;
				}
			}
			else
			{
				_symbolStack[symbol->Name].emplace_back(_currentScope, symbol);
			}

			return true;
		}
		Parser::Symbol *Parser::FindSymbol(const std::string &name) const
		{
			return FindSymbol(name, _currentScope, false);
		}
		Parser::Symbol *Parser::FindSymbol(const std::string &name, const Scope &scope, bool exclusive) const
		{
			const auto it = _symbolStack.find(name);

			if (it == _symbolStack.end() || it->second.empty())
			{
				return nullptr;
			}

			Symbol *result = nullptr;
			const auto &scopes = it->second;

			for (auto it2 = scopes.rbegin(), end = scopes.rend(); it2 != end; ++it2)
			{
				if (it2->first.Level > scope.Level || it2->first.NamespaceLevel > scope.NamespaceLevel || (it2->first.NamespaceLevel == scope.NamespaceLevel && it2->first.Name != scope.Name))
				{
					continue;
				}
				if (exclusive && it2->first.Level < scope.Level)
				{
					continue;
				}

				if (it2->second->id == nodeid::Variable || it2->second->id == nodeid::Struct)
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
		bool Parser::ResolveCall(Nodes::Call *call, const Scope &scope, bool &is_intrinsic, bool &is_ambiguous) const
		{
			is_intrinsic = false;
			is_ambiguous = false;

			unsigned int overloadCount = 0, overloadNamespace = scope.NamespaceLevel;
			const Nodes::Function *overload = nullptr;
			Nodes::Intrinsic::Op intrinsicOp = Nodes::Intrinsic::Op::None;

			const auto it = _symbolStack.find(call->CalleeName);

			if (it != _symbolStack.end() && !it->second.empty())
			{
				const auto &scopes = it->second;

				for (auto it2 = scopes.rbegin(), end = scopes.rend(); it2 != end; ++it2)
				{
					if (it2->first.Level > scope.Level || it2->first.NamespaceLevel > scope.NamespaceLevel || it2->second->id != nodeid::Function)
					{
						continue;
					}

					const Nodes::Function *function = static_cast<Nodes::Function *>(it2->second);

					if (function->Parameters.empty())
					{
						if (call->Arguments.empty())
						{
							overload = function;
							overloadCount = 1;
							break;
						}
						else
						{
							continue;
						}
					}
					else if (call->Arguments.size() != function->Parameters.size())
					{
						continue;
					}

					const int comparison = CompareFunctions(call, function, overload);

					if (comparison < 0)
					{
						overload = function;
						overloadCount = 1;
						overloadNamespace = it2->first.NamespaceLevel;
					}
					else if (comparison == 0 && overloadNamespace == it2->first.NamespaceLevel)
					{
						++overloadCount;
					}
				}
			}

			if (overloadCount == 0)
			{
				for (auto &intrinsic : sIntrinsics)
				{
					if (intrinsic.Function.Name == call->CalleeName)
					{
						if (call->Arguments.size() != intrinsic.Function.Parameters.size())
						{
							is_intrinsic = overloadCount == 0;
							break;
						}

						const int comparison = CompareFunctions(call, &intrinsic.Function, overload);

						if (comparison < 0)
						{
							overload = &intrinsic.Function;
							overloadCount = 1;

							is_intrinsic = true;
							intrinsicOp = intrinsic.Op;
						}
						else if (comparison == 0 && overloadNamespace == 0)
						{
							++overloadCount;
						}
					}
				}
			}

			if (overloadCount == 1)
			{
				call->Type = overload->ReturnType;

				if (is_intrinsic)
				{
					call->Callee = reinterpret_cast<Nodes::Function *>(static_cast<unsigned int>(intrinsicOp));
				}
				else
				{
					call->Callee = overload;
					call->CalleeName = overload->Name;
				}

				return true;
			}
			else
			{
				is_ambiguous = overloadCount > 1;

				return false;
			}
		}
		Nodes::Expression *Parser::FoldConstantExpression(Nodes::Expression *expression) const
		{
			#pragma region Helpers
	#define DOFOLDING1(op) \
			for (unsigned int i = 0; i < operand->Type.Rows * operand->Type.Cols; ++i) \
				switch (operand->Type.BaseClass) \
				{ \
					case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
						switch (expression->Type.BaseClass) \
						{ \
							case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
								operand->Value.Int[i] = static_cast<int>(op(operand->Value.Int[i])); break; \
							case Nodes::Type::Class::Float: \
								operand->Value.Float[i] = static_cast<float>(op(operand->Value.Int[i])); break; \
						} \
						break; \
					case Nodes::Type::Class::Float: \
						switch (expression->Type.BaseClass) \
						{ \
							case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
								operand->Value.Int[i] = static_cast<int>(op(operand->Value.Float[i])); break; \
							case Nodes::Type::Class::Float: \
								operand->Value.Float[i] = static_cast<float>(op(operand->Value.Float[i])); break; \
						} \
						break; \
				} \
			expression = operand;

	#define DOFOLDING2(op) { \
			union Nodes::Literal::Value result = { 0 }; \
			for (unsigned int i = 0; i < expression->Type.Rows * expression->Type.Cols; ++i) \
				switch (left->Type.BaseClass) \
				{ \
					case Nodes::Type::Class::Bool:  case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
						switch (right->Type.BaseClass) \
						{ \
							case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
								result.Int[i] = left->Value.Int[leftScalar ? 0 : i] op right->Value.Int[rightScalar ? 0 : i]; \
								break; \
							case Nodes::Type::Class::Float: \
								result.Float[i] = static_cast<float>(left->Value.Int[!leftScalar * i]) op right->Value.Float[!rightScalar * i]; \
								break; \
						} \
						break; \
					case Nodes::Type::Class::Float: \
						result.Float[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (left->Value.Float[!leftScalar * i] op right->Value.Float[!rightScalar * i]) : (left->Value.Float[!leftScalar * i] op static_cast<float>(right->Value.Int[!rightScalar * i])); \
						break; \
				} \
			left->Type = expression->Type; \
			left->Value = result; \
			expression = left; }
	#define DOFOLDING2_INT(op) { \
			union Nodes::Literal::Value result = { 0 }; \
			for (unsigned int i = 0; i < expression->Type.Rows * expression->Type.Cols; ++i) \
			{ \
				result.Int[i] = left->Value.Int[!leftScalar * i] op right->Value.Int[!rightScalar * i]; \
			} \
			left->Type = expression->Type; \
			left->Value = result; \
			expression = left; }
	#define DOFOLDING2_BOOL(op) { \
			union Nodes::Literal::Value result = { 0 }; \
			for (unsigned int i = 0; i < expression->Type.Rows * expression->Type.Cols; ++i) \
				switch (left->Type.BaseClass) \
				{ \
					case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
						result.Int[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (static_cast<float>(left->Value.Int[!leftScalar * i]) op right->Value.Float[!rightScalar * i]) : (left->Value.Int[!leftScalar * i] op right->Value.Int[!rightScalar * i]); \
						break; \
					case Nodes::Type::Class::Float: \
						result.Int[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (left->Value.Float[!leftScalar * i] op static_cast<float>(right->Value.Int[!rightScalar * i])) : (left->Value.Float[!leftScalar * i] op right->Value.Float[!rightScalar * i]); \
						break; \
				} \
			left->Type = expression->Type; \
			left->Type.BaseClass = Nodes::Type::Class::Bool; \
			left->Value = result; \
			expression = left; }
	#define DOFOLDING2_FLOAT(op) { \
			union Nodes::Literal::Value result = { 0 }; \
			for (unsigned int i = 0; i < expression->Type.Rows * expression->Type.Cols; ++i) \
				switch (left->Type.BaseClass) \
				{ \
					case Nodes::Type::Class::Bool:  case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
						result.Float[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (static_cast<float>(left->Value.Int[!leftScalar * i]) op right->Value.Float[!rightScalar * i]) : (left->Value.Int[leftScalar ? 0 : i] op right->Value.Int[rightScalar ? 0 : i]); \
						break; \
					case Nodes::Type::Class::Float: \
						result.Float[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (left->Value.Float[!leftScalar * i] op right->Value.Float[!rightScalar * i]) : (left->Value.Float[!leftScalar * i] op static_cast<float>(right->Value.Int[!rightScalar * i])); \
						break; \
				} \
			left->Type = expression->Type; \
			left->Type.BaseClass = Nodes::Type::Class::Float; \
			left->Value = result; \
			expression = left; }

	#define DOFOLDING2_FUNCTION(op) \
			for (unsigned int i = 0; i < expression->Type.Rows * expression->Type.Cols; ++i) \
				switch (left->Type.BaseClass) \
				{ \
					case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
						switch (right->Type.BaseClass) \
						{ \
							case Nodes::Type::Class::Bool: case Nodes::Type::Class::Int: case Nodes::Type::Class::Uint: \
								left->Value.Int[i] = static_cast<int>(op(left->Value.Int[i], right->Value.Int[i])); \
								break; \
							case Nodes::Type::Class::Float: \
								left->Value.Float[i] = static_cast<float>(op(static_cast<float>(left->Value.Int[i]), right->Value.Float[i])); \
								break; \
						} \
						break; \
					case Nodes::Type::Class::Float: \
						left->Value.Float[i] = (right->Type.BaseClass == Nodes::Type::Class::Float) ? (static_cast<float>(op(left->Value.Float[i], right->Value.Float[i]))) : (static_cast<float>(op(left->Value.Float[i], static_cast<float>(right->Value.Int[i])))); \
						break; \
				} \
			left->Type = expression->Type; \
			expression = left;
			#pragma endregion

			if (expression->id == nodeid::Unary)
			{
				const auto unaryexpression = static_cast<Nodes::Unary *>(expression);

				if (unaryexpression->Operand->id != nodeid::Literal)
				{
					return expression;
				}

				const auto operand = static_cast<Nodes::Literal *>(unaryexpression->Operand);

				switch (unaryexpression->Operator)
				{
					case Nodes::Unary::Op::Negate:
						DOFOLDING1(-);
						break;
					case Nodes::Unary::Op::BitwiseNot:
						for (unsigned int i = 0; i < operand->Type.Rows * operand->Type.Cols; ++i)
						{
							operand->Value.Int[i] = ~operand->Value.Int[i];
						}
						expression = operand;
						break;
					case Nodes::Unary::Op::LogicalNot:
						for (unsigned int i = 0; i < operand->Type.Rows * operand->Type.Cols; ++i)
						{
							operand->Value.Int[i] = (operand->Type.BaseClass == Nodes::Type::Class::Float) ? !operand->Value.Float[i] : !operand->Value.Int[i];
						}
						operand->Type.BaseClass = Nodes::Type::Class::Bool;
						expression = operand;
						break;
					case Nodes::Unary::Op::Cast:
					{
						Nodes::Literal old = *operand;
						operand->Type = expression->Type;
						expression = operand;

						for (unsigned int i = 0, size = std::min(old.Type.Rows * old.Type.Cols, operand->Type.Rows * operand->Type.Cols); i < size; ++i)
						{
							VectorLiteralCast(&old, i, operand, i);
						}
						break;
					}
				}
			}
			else if (expression->id == nodeid::Binary)
			{
				const auto binaryexpression = static_cast<Nodes::Binary *>(expression);

				if (binaryexpression->Operands[0]->id != nodeid::Literal || binaryexpression->Operands[1]->id != nodeid::Literal)
				{
					return expression;
				}

				const auto left = static_cast<Nodes::Literal *>(binaryexpression->Operands[0]);
				const auto right = static_cast<Nodes::Literal *>(binaryexpression->Operands[1]);
				const bool leftScalar = left->Type.Rows * left->Type.Cols == 1;
				const bool rightScalar = right->Type.Rows * right->Type.Cols == 1;

				switch (binaryexpression->Operator)
				{
					case Nodes::Binary::Op::Add:
						DOFOLDING2(+);
						break;
					case Nodes::Binary::Op::Subtract:
						DOFOLDING2(-);
						break;
					case Nodes::Binary::Op::Multiply:
						DOFOLDING2(*);
						break;
					case Nodes::Binary::Op::Divide:
						DOFOLDING2_FLOAT(/);
						break;
					case Nodes::Binary::Op::Modulo:
						DOFOLDING2_FUNCTION(std::fmod);
						break;
					case Nodes::Binary::Op::Less:
						DOFOLDING2_BOOL(<);
						break;
					case Nodes::Binary::Op::Greater:
						DOFOLDING2_BOOL(>);
						break;
					case Nodes::Binary::Op::LessOrEqual:
						DOFOLDING2_BOOL(<=);
						break;
					case Nodes::Binary::Op::GreaterOrEqual:
						DOFOLDING2_BOOL(>=);
						break;
					case Nodes::Binary::Op::Equal:
						DOFOLDING2_BOOL(==);
						break;
					case Nodes::Binary::Op::NotEqual:
						DOFOLDING2_BOOL(!=);
						break;
					case Nodes::Binary::Op::LeftShift:
						DOFOLDING2_INT(<<);
						break;
					case Nodes::Binary::Op::RightShift:
						DOFOLDING2_INT(>>);
						break;
					case Nodes::Binary::Op::BitwiseAnd:
						DOFOLDING2_INT(&);
						break;
					case Nodes::Binary::Op::BitwiseOr:
						DOFOLDING2_INT(|);
						break;
					case Nodes::Binary::Op::BitwiseXor:
						DOFOLDING2_INT(^);
						break;
					case Nodes::Binary::Op::LogicalAnd:
						DOFOLDING2_BOOL(&&);
						break;
					case Nodes::Binary::Op::LogicalOr:
						DOFOLDING2_BOOL(||);
						break;
				}
			}
			else if (expression->id == nodeid::Intrinsic)
			{
				const auto intrinsicexpression = static_cast<Nodes::Intrinsic *>(expression);

				if ((intrinsicexpression->Arguments[0] != nullptr && intrinsicexpression->Arguments[0]->id != nodeid::Literal) || (intrinsicexpression->Arguments[1] != nullptr && intrinsicexpression->Arguments[1]->id != nodeid::Literal) || (intrinsicexpression->Arguments[2] != nullptr && intrinsicexpression->Arguments[2]->id != nodeid::Literal))
				{
					return expression;
				}

				const auto operand = static_cast<Nodes::Literal *>(intrinsicexpression->Arguments[0]);
				const auto left = operand;
				const auto right = static_cast<Nodes::Literal *>(intrinsicexpression->Arguments[1]);

				switch (intrinsicexpression->Operator)
				{
					case Nodes::Intrinsic::Op::Abs:
						DOFOLDING1(std::abs);
						break;
					case Nodes::Intrinsic::Op::Sin:
						DOFOLDING1(std::sin);
						break;
					case Nodes::Intrinsic::Op::Sinh:
						DOFOLDING1(std::sinh);
						break;
					case Nodes::Intrinsic::Op::Cos:
						DOFOLDING1(std::cos);
						break;
					case Nodes::Intrinsic::Op::Cosh:
						DOFOLDING1(std::cosh);
						break;
					case Nodes::Intrinsic::Op::Tan:
						DOFOLDING1(std::tan);
						break;
					case Nodes::Intrinsic::Op::Tanh:
						DOFOLDING1(std::tanh);
						break;
					case Nodes::Intrinsic::Op::Asin:
						DOFOLDING1(std::asin);
						break;
					case Nodes::Intrinsic::Op::Acos:
						DOFOLDING1(std::acos);
						break;
					case Nodes::Intrinsic::Op::Atan:
						DOFOLDING1(std::atan);
						break;
					case Nodes::Intrinsic::Op::Exp:
						DOFOLDING1(std::exp);
						break;
					case Nodes::Intrinsic::Op::Log:
						DOFOLDING1(std::log);
						break;
					case Nodes::Intrinsic::Op::Log10:
						DOFOLDING1(std::log10);
						break;
					case Nodes::Intrinsic::Op::Sqrt:
						DOFOLDING1(std::sqrt);
						break;
					case Nodes::Intrinsic::Op::Ceil:
						DOFOLDING1(std::ceil);
						break;
					case Nodes::Intrinsic::Op::Floor:
						DOFOLDING1(std::floor);
						break;
					case Nodes::Intrinsic::Op::Atan2:
						DOFOLDING2_FUNCTION(std::atan2);
						break;
					case Nodes::Intrinsic::Op::Pow:
						DOFOLDING2_FUNCTION(std::pow);
						break;
					case Nodes::Intrinsic::Op::Min:
						DOFOLDING2_FUNCTION(std::min);
						break;
					case Nodes::Intrinsic::Op::Max:
						DOFOLDING2_FUNCTION(std::max);
						break;
				}
			}
			else if (expression->id == nodeid::Constructor)
			{
				const auto constructor = static_cast<Nodes::Constructor *>(expression);

				for (auto argument : constructor->Arguments)
				{
					if (argument->id != nodeid::Literal)
					{
						return expression;
					}
				}

				unsigned int k = 0;
				const auto literal = _ast->make_node<Nodes::Literal>(constructor->location);
				literal->Type = constructor->Type;

				for (auto argument : constructor->Arguments)
				{
					for (unsigned int j = 0; j < argument->Type.Rows * argument->Type.Cols; ++k, ++j)
					{
						VectorLiteralCast(static_cast<Nodes::Literal *>(argument), k, literal, j);
					}
				}

				expression = literal;
			}
			else if (expression->id == nodeid::LValue)
			{
				const auto variable = static_cast<Nodes::LValue *>(expression)->Reference;

				if (variable->Initializer == nullptr || !(variable->Initializer->id == nodeid::Literal && variable->Type.HasQualifier(Nodes::Type::Qualifier::Const)))
				{
					return expression;
				}

				const auto literal = _ast->make_node<Nodes::Literal>(expression->location);
				literal->Type = expression->Type;
				literal->Value = static_cast<const Nodes::Literal *>(variable->Initializer)->Value;

				expression = literal;
			}

			return expression;
		}
	}
}
