#include "EffectParser.hpp"

#include <algorithm>
#include <functional>
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
					this->Op = op;
					this->Function.Name = name;
					this->Function.ReturnType.BaseClass = returntype;
					this->Function.ReturnType.Rows = returnrows;
					this->Function.ReturnType.Cols = returncols;
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols)
				{
					this->Op = op;
					this->Function.Name = name;
					this->Function.ReturnType.BaseClass = returntype;
					this->Function.ReturnType.Rows = returnrows;
					this->Function.ReturnType.Cols = returncols;
					this->Arguments[0].Type.BaseClass = arg0type;
					this->Arguments[0].Type.Rows = arg0rows;
					this->Arguments[0].Type.Cols = arg0cols;

					this->Function.Parameters.push_back(&Arguments[0]);
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols, Nodes::Type::Class arg1type, unsigned int arg1rows, unsigned int arg1cols)
				{
					this->Op = op;
					this->Function.Name = name;
					this->Function.ReturnType.BaseClass = returntype;
					this->Function.ReturnType.Rows = returnrows;
					this->Function.ReturnType.Cols = returncols;
					this->Arguments[0].Type.BaseClass = arg0type;
					this->Arguments[0].Type.Rows = arg0rows;
					this->Arguments[0].Type.Cols = arg0cols;
					this->Arguments[1].Type.BaseClass = arg1type;
					this->Arguments[1].Type.Rows = arg1rows;
					this->Arguments[1].Type.Cols = arg1cols;

					this->Function.Parameters.push_back(&Arguments[0]);
					this->Function.Parameters.push_back(&Arguments[1]);
				}
				explicit Intrinsic(const std::string &name, Nodes::Intrinsic::Op op, Nodes::Type::Class returntype, unsigned int returnrows, unsigned int returncols, Nodes::Type::Class arg0type, unsigned int arg0rows, unsigned int arg0cols, Nodes::Type::Class arg1type, unsigned int arg1rows, unsigned int arg1cols, Nodes::Type::Class arg2type, unsigned int arg2rows, unsigned int arg2cols)
				{
					this->Op = op;
					this->Function.Name = name;
					this->Function.ReturnType.BaseClass = returntype;
					this->Function.ReturnType.Rows = returnrows;
					this->Function.ReturnType.Cols = returncols;
					this->Arguments[0].Type.BaseClass = arg0type;
					this->Arguments[0].Type.Rows = arg0rows;
					this->Arguments[0].Type.Cols = arg0cols;
					this->Arguments[1].Type.BaseClass = arg1type;
					this->Arguments[1].Type.Rows = arg1rows;
					this->Arguments[1].Type.Cols = arg1cols;
					this->Arguments[2].Type.BaseClass = arg2type;
					this->Arguments[2].Type.Rows = arg2rows;
					this->Arguments[2].Type.Cols = arg2cols;

					this->Function.Parameters.push_back(&Arguments[0]);
					this->Function.Parameters.push_back(&Arguments[1]);
					this->Function.Parameters.push_back(&Arguments[2]);
				}

				Nodes::Intrinsic::Op Op;
				Nodes::Function Function;
				Nodes::Variable Arguments[3];
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
				Intrinsic("tex2Dbias",			Nodes::Intrinsic::Op::Tex2DBias,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tex2Dfetch",			Nodes::Intrinsic::Op::Tex2DFetch,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Int,   2, 1),
				Intrinsic("tex2Dgather",		Nodes::Intrinsic::Op::Tex2DGather,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1),
				Intrinsic("tex2Dgatheroffset",	Nodes::Intrinsic::Op::Tex2DGatherOffset,	Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int, 2, 1),
				Intrinsic("tex2Dlod",			Nodes::Intrinsic::Op::Tex2DLevel,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1),
				Intrinsic("tex2Dlodoffset",		Nodes::Intrinsic::Op::Tex2DLevelOffset,		Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Int, 2, 1),
				Intrinsic("tex2Doffset",		Nodes::Intrinsic::Op::Tex2DOffset,			Nodes::Type::Class::Float, 4, 1, Nodes::Type::Class::Sampler2D, 0, 0, Nodes::Type::Class::Float, 2, 1, Nodes::Type::Class::Int, 2, 1),
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

			void ScalarLiteralCast(const Nodes::Literal *from, std::size_t i, int &to)
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
				}
			}
			void ScalarLiteralCast(const Nodes::Literal *from, std::size_t i, unsigned int &to)
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
				}
			}
			void ScalarLiteralCast(const Nodes::Literal *from, std::size_t i, float &to)
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
				}
			}
			void VectorLiteralCast(const Nodes::Literal *from, std::size_t k, Nodes::Literal *to, std::size_t j)
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
				}
			}

			unsigned int GetTypeRank(const Nodes::Type &src, const Nodes::Type &dst)
			{
				if (src.IsArray() != dst.IsArray() || src.ArrayLength != dst.ArrayLength)
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
				for (std::size_t i = 0, count = call->Arguments.size(); i < count; ++i)
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

				const std::size_t count = call->Arguments.size();

				unsigned int *const function1Ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				unsigned int *const function2Ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
				const bool function1Viable = GetCallRanks(call, function1, function1Ranks);
				const bool function2Viable = GetCallRanks(call, function2, function2Ranks);

				if (!(function1Viable && function2Viable))
				{
					if (function1Viable)
					{
						return -1;
					}
					else if (function2Viable)
					{
						return +1;
					}
					else
					{
						return 0;
					}
				}
		
				std::sort(function1Ranks, function1Ranks + count, std::greater<unsigned int>());
				std::sort(function2Ranks, function2Ranks + count, std::greater<unsigned int>());
		
				for (std::size_t i = 0; i < count; ++i)
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
		}

		Parser::Parser(Lexer &lexer, Tree &ast) : mAST(ast), mLexer(lexer), mBackupLexer(lexer), mOrigLexer(lexer), mCurrentScope(0)
		{
			Consume();
		}

		bool Parser::Parse()
		{
			bool success = true;

			while (!Peek(Lexer::Token::Id::EndOfStream))
			{
				if (!ParseTopLevel())
				{
					success = false;
					break;
				}
			}

			this->mOrigLexer = this->mLexer;

			return success;
		}

		void Parser::Backup()
		{
			this->mBackupLexer = this->mLexer;
			this->mBackupToken = this->mNextToken;
		}
		void Parser::Restore()
		{
			this->mLexer = this->mBackupLexer;
			this->mNextToken = this->mBackupToken;
		}

		bool Parser::AcceptIdentifier(std::string &identifier)
		{
			if (Accept(Lexer::Token::Id::Identifier))
			{
				identifier = this->mToken.GetRawData();

				return true;
			}

			return false;
		}
		bool Parser::Expect(Lexer::Token::Id token)
		{
			if (!Accept(token))
			{
				this->mLexer.Error(this->mNextToken.GetLocation(), 3000, "syntax error: unexpected '%s', expected '%s'", this->mNextToken.GetName().c_str(), Lexer::Token::GetName(token).c_str());

				return false;
			}

			return true;
		}
		bool Parser::ExpectIdentifier(std::string &identifier)
		{
			if (Expect(Lexer::Token::Id::Identifier))
			{
				identifier = this->mToken.GetRawData();

				return true;
			}

			return false;
		}

		// Types
		bool Parser::AcceptTypeClass(Nodes::Type &type)
		{
			type.Definition = nullptr;
			type.ArrayLength = 0;

			if (Peek(Lexer::Token::Id::Identifier))
			{
				type.Rows = type.Cols = 0;
				type.BaseClass = Nodes::Type::Class::Struct;

				Node *const symbol = FindSymbol(this->mNextToken.GetRawData());

				if (symbol != nullptr && symbol->NodeId == Node::Id::Struct)
				{
					type.Definition = static_cast<Nodes::Struct *>(symbol);

					Consume();
				}
				else
				{
					return false;
				}
			}
			else if (ParseStruct(type.Definition))
			{
				type.Rows = type.Cols = 0;
				type.BaseClass = Nodes::Type::Class::Struct;
			}
			else if (Accept(Lexer::Token::Id::Vector))
			{
				type.Rows = 4, type.Cols = 1;
				type.BaseClass = Nodes::Type::Class::Float;

				if (Accept('<'))
				{
					if (!AcceptTypeClass(type))
					{
						this->mLexer.Error(this->mNextToken.GetLocation(), 3000, "syntax error: unexpected '%s', expected vector element type", this->mNextToken.GetName().c_str());

						return false;
					}
					
					if (!type.IsScalar())
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3122, "vector element type must be a scalar type");

						return false;
					}

					if (!(Expect(',') && Expect(Lexer::Token::Id::IntLiteral)))
					{
						return false;
					}
					
					if (this->mToken.GetLiteral<int>() < 1 || this->mToken.GetLiteral<int>() > 4)
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3052, "vector dimension must be between 1 and 4");

						return false;
					}

					type.Rows = this->mToken.GetLiteral<int>();

					if (!Expect('>'))
					{
						return false;
					}
				}
			}
			else if (Accept(Lexer::Token::Id::Matrix))
			{
				type.Rows = 4, type.Cols = 4;
				type.BaseClass = Nodes::Type::Class::Float;

				if (Accept('<'))
				{
					if (!AcceptTypeClass(type))
					{
						this->mLexer.Error(this->mNextToken.GetLocation(), 3000, "syntax error: unexpected '%s', expected matrix element type", this->mNextToken.GetName().c_str());

						return false;
					}
					
					if (!type.IsScalar())
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3123, "matrix element type must be a scalar type");

						return false;
					}

					if (!(Expect(',') && Expect(Lexer::Token::Id::IntLiteral)))
					{
						return false;
					}
					
					if (this->mToken.GetLiteral<int>() < 1 || this->mToken.GetLiteral<int>() > 4)
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.Rows = this->mToken.GetLiteral<int>();

					if (!(Expect(',') && Expect(Lexer::Token::Id::IntLiteral)))
					{
						return false;
					}
					
					if (this->mToken.GetLiteral<int>() < 1 || this->mToken.GetLiteral<int>() > 4)
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3053, "matrix dimensions must be between 1 and 4");

						return false;
					}

					type.Cols = this->mToken.GetLiteral<int>();

					if (!Expect('>'))
					{
						return false;
					}
				}
			}
			else
			{
				type.Rows = type.Cols = 0;

				switch (this->mNextToken)
				{
					case Lexer::Token::Id::Void:
						type.BaseClass = Nodes::Type::Class::Void;
						break;
					case Lexer::Token::Id::Bool:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Bool4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Bool;
						break;
					case Lexer::Token::Id::Int:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Int4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Int;
						break;
					case Lexer::Token::Id::Uint:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Uint4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Uint;
						break;
					case Lexer::Token::Id::Float:
						type.Rows = 1, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float2:
						type.Rows = 2, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float2x2:
						type.Rows = 2, type.Cols = 2;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float3:
						type.Rows = 3, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float3x3:
						type.Rows = 3, type.Cols = 3;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float4:
						type.Rows = 4, type.Cols = 1;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::Float4x4:
						type.Rows = 4, type.Cols = 4;
						type.BaseClass = Nodes::Type::Class::Float;
						break;
					case Lexer::Token::Id::String:
						type.BaseClass = Nodes::Type::Class::String;
						break;
					case Lexer::Token::Id::Texture1D:
						type.BaseClass = Nodes::Type::Class::Texture1D;
						break;
					case Lexer::Token::Id::Texture2D:
						type.BaseClass = Nodes::Type::Class::Texture2D;
						break;
					case Lexer::Token::Id::Texture3D:
						type.BaseClass = Nodes::Type::Class::Texture3D;
						break;
					case Lexer::Token::Id::Sampler1D:
						type.BaseClass = Nodes::Type::Class::Sampler1D;
						break;
					case Lexer::Token::Id::Sampler2D:
						type.BaseClass = Nodes::Type::Class::Sampler2D;
						break;
					case Lexer::Token::Id::Sampler3D:
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
			if (Accept(Lexer::Token::Id::Extern))
			{
				qualifiers |= Nodes::Type::Qualifier::Extern;
			}
			if (Accept(Lexer::Token::Id::Static))
			{
				qualifiers |= Nodes::Type::Qualifier::Static;
			}
			if (Accept(Lexer::Token::Id::Uniform))
			{
				qualifiers |= Nodes::Type::Qualifier::Uniform;
			}
			if (Accept(Lexer::Token::Id::Volatile))
			{
				qualifiers |= Nodes::Type::Qualifier::Volatile;
			}
			if (Accept(Lexer::Token::Id::Precise))
			{
				qualifiers |= Nodes::Type::Qualifier::Precise;
			}

			if (Accept(Lexer::Token::Id::In))
			{
				qualifiers |= Nodes::Type::Qualifier::In;
			}
			if (Accept(Lexer::Token::Id::Out))
			{
				qualifiers |= Nodes::Type::Qualifier::Out;
			}
			if (Accept(Lexer::Token::Id::InOut))
			{
				qualifiers |= Nodes::Type::Qualifier::InOut;
			}

			// Modifiers
			if (Accept(Lexer::Token::Id::Const))
			{
				qualifiers |= Nodes::Type::Qualifier::Const;
			}

			// Interpolation
			if (Accept(Lexer::Token::Id::Linear))
			{
				qualifiers |= Nodes::Type::Qualifier::Linear;
			}
			if (Accept(Lexer::Token::Id::NoPerspective))
			{
				qualifiers |= Nodes::Type::Qualifier::NoPerspective;
			}
			if (Accept(Lexer::Token::Id::Centroid))
			{
				qualifiers |= Nodes::Type::Qualifier::Centroid;
			}
			if (Accept(Lexer::Token::Id::NoInterpolation))
			{
				qualifiers |= Nodes::Type::Qualifier::NoInterpolation;
			}

			if (qualifiers == 0)
			{
				return false;
			}
			else if ((type.Qualifiers & qualifiers) == qualifiers)
			{
				this->mLexer.Warning(this->mToken.GetLocation(), 3048, "duplicate usages specified");
			}

			type.Qualifiers |= qualifiers;

			AcceptTypeQualifiers(type);

			return true;
		}
		bool Parser::ParseType(Nodes::Type &type)
		{
			type.Qualifiers = 0;

			AcceptTypeQualifiers(type);

			const Lexer::Location location = this->mNextToken.GetLocation();

			if (!AcceptTypeClass(type))
			{
				return false;
			}

			if (type.IsIntegral() && (type.HasQualifier(Nodes::Type::Qualifier::Centroid) || type.HasQualifier(Nodes::Type::Qualifier::NoPerspective)))
			{
				this->mLexer.Error(location, 4576, "signature specifies invalid interpolation mode for integer component type");
				
				return false;
			}

			if (type.HasQualifier(Nodes::Type::Qualifier::Centroid) && !type.HasQualifier(Nodes::Type::Qualifier::NoPerspective))
			{
				type.Qualifiers |= Nodes::Type::Qualifier::Linear;
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
				std::string attribute;

				if (ExpectIdentifier(attribute) && Expect(']'))
				{
					attributes.push_back(attribute);
				}
			}

			const Lexer::Location location = this->mNextToken.GetLocation();

			if (Peek('{'))
			{
				if (!ParseStatementCompound(statement, scoped))
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
			if (Accept(Lexer::Token::Id::If))
			{
				Nodes::If *const newstatement = this->mAST.CreateNode<Nodes::If>(location);
				newstatement->Attributes = attributes;

				if (!Expect('('))
				{
					return false;
				}

				const Lexer::Location conditionLocation = this->mNextToken.GetLocation();
				
				if (!(ParseExpression(newstatement->Condition) && Expect(')')))
				{
					return false;
				}

				if (!newstatement->Condition->Type.IsScalar())
				{
					this->mLexer.Error(conditionLocation, 3019, "if statement conditional expressions must evaluate to a scalar");

					return false;
				}

				if (!ParseStatement(newstatement->StatementOnTrue))
				{
					return false;
				}

				statement = newstatement;

				if (Accept(Lexer::Token::Id::Else))
				{
					return ParseStatement(newstatement->StatementOnFalse);
				}

				return true;
			}
			#pragma endregion

			#pragma region Switch
			if (Accept(Lexer::Token::Id::Switch))
			{
				Nodes::Switch *const newstatement = this->mAST.CreateNode<Nodes::Switch>(location);
				newstatement->Attributes = attributes;

				if (!Expect('('))
				{
					return false;
				}

				const Lexer::Location conditionLocation = this->mNextToken.GetLocation();
				
				if (!(ParseExpression(newstatement->Test) && Expect(')') && Expect('{')))
				{
					return false;
				}

				if (!newstatement->Test->Type.IsScalar())
				{
					this->mLexer.Error(conditionLocation, 3019, "switch statement expression must evaluate to a scalar");

					return false;
				}

				while (!Peek('}') && !Peek(Lexer::Token::Id::EndOfStream))
				{
					Nodes::Case *const casenode = this->mAST.CreateNode<Nodes::Case>(Lexer::Location());

					while (Accept(Lexer::Token::Id::Case) || Accept(Lexer::Token::Id::Default))
					{
						Nodes::Expression *label = nullptr;

						if (this->mToken == Lexer::Token::Id::Case)
						{
							const Lexer::Location labelLocation = this->mNextToken.GetLocation();

							if (!ParseExpression(label))
							{
								return false;
							}

							if (label->NodeId != Node::Id::Literal || !label->Type.IsNumeric())
							{
								this->mLexer.Error(labelLocation, 3020, "non-numeric case expression");
								
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

					casenode->Location = casenode->Labels[0]->Location;

					if (!ParseStatement(casenode->Statements))
					{
						return false;
					}

					newstatement->Cases.push_back(casenode);
				}

				if (newstatement->Cases.empty())
				{
					this->mLexer.Warning(location, 5002, "switch statement contains no 'case' or 'default' labels");

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
			if (Accept(Lexer::Token::Id::For))
			{
				Nodes::For *const newstatement = this->mAST.CreateNode<Nodes::For>(location);
				newstatement->Attributes = attributes;

				if (!Expect('('))
				{
					return false;
				}

				EnterScope();

				if (!ParseDeclaration(reinterpret_cast<Nodes::DeclaratorList *&>(newstatement->Initialization)))
				{
					Nodes::Expression *expression = nullptr;
					
					if (ParseExpression(expression))
					{
						Nodes::ExpressionStatement *const initialization = this->mAST.CreateNode<Nodes::ExpressionStatement>(expression->Location);
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
			if (Accept(Lexer::Token::Id::While))
			{
				Nodes::While *const newstatement = this->mAST.CreateNode<Nodes::While>(location);
				newstatement->Attributes = attributes;
				newstatement->DoWhile = false;

				if (!Expect('('))
				{
					return false;
				}

				EnterScope();
				
				if (!(ParseExpression(newstatement->Condition) && Expect(')') && ParseStatement(newstatement->Statements, false)))
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
			if (Accept(Lexer::Token::Id::Do))
			{
				Nodes::While *const newstatement = this->mAST.CreateNode<Nodes::While>(location);
				newstatement->Attributes = attributes;
				newstatement->DoWhile = true;

				if (!(ParseStatement(newstatement->Statements) && Expect(Lexer::Token::Id::While) && Expect('(') && ParseExpression(newstatement->Condition) && Expect(')') && Expect(';')))
				{
					return false;
				}

				statement = newstatement;

				return true;
			}
			#pragma endregion

			#pragma region Break
			if (Accept(Lexer::Token::Id::Break))
			{
				Nodes::Jump *const newstatement = this->mAST.CreateNode<Nodes::Jump>(location);
				newstatement->Attributes = attributes;
				newstatement->Mode = Nodes::Jump::Break;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Continue
			if (Accept(Lexer::Token::Id::Continue))
			{
				Nodes::Jump *const newstatement = this->mAST.CreateNode<Nodes::Jump>(location);
				newstatement->Attributes = attributes;
				newstatement->Mode = Nodes::Jump::Continue;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Return
			if (Accept(Lexer::Token::Id::Return))
			{
				Nodes::Return *const newstatement = this->mAST.CreateNode<Nodes::Return>(location);
				newstatement->Attributes = attributes;
				newstatement->Discard = false;

				const Nodes::Function *const parent = static_cast<const Nodes::Function *>(this->mParentStack.top());

				if (!Peek(';'))
				{
					if (!ParseExpression(newstatement->Value))
					{
						return false;
					}

					if (parent->ReturnType.IsVoid())
					{
						this->mLexer.Error(location, 3079, "void functions cannot return a value");

						Accept(';');

						return false;
					}

					if (newstatement->Value->Type.Rows > parent->ReturnType.Rows || newstatement->Value->Type.Cols > parent->ReturnType.Cols)
					{
						this->mLexer.Warning(location, 3206, "implicit truncation of vector type");
					}
				}
				else if (!parent->ReturnType.IsVoid())
				{
					this->mLexer.Error(location, 3080, "function must return a value");

					Accept(';');

					return false;
				}

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Discard
			if (Accept(Lexer::Token::Id::Discard))
			{
				Nodes::Return *const newstatement = this->mAST.CreateNode<Nodes::Return>(location);
				newstatement->Attributes = attributes;
				newstatement->Discard = true;

				statement = newstatement;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Declaration
			Nodes::DeclaratorList *declaration = nullptr;

			if (ParseDeclaration(declaration))
			{
				declaration->Attributes = attributes;

				statement = declaration;

				return Expect(';');
			}
			#pragma endregion

			#pragma region Expression
			Nodes::Expression *expression = nullptr;

			if (ParseExpression(expression))
			{
				Nodes::ExpressionStatement *const newstatement = this->mAST.CreateNode<Nodes::ExpressionStatement>(location);
				newstatement->Attributes = attributes;
				newstatement->Expression = expression;

				statement = newstatement;

				return Expect(';');
			}
			else
			{
				while (!Accept(';') && !Peek(Lexer::Token::Id::EndOfStream))
				{
					Consume();
				}
			}
			#pragma endregion

			return false;
		}
		bool Parser::ParseStatementCompound(Nodes::Statement *&statement, bool scoped)
		{
			if (!Expect('{'))
			{
				return false;
			}

			Nodes::Compound *const compound = this->mAST.CreateNode<Nodes::Compound>(this->mToken.GetLocation());

			if (scoped)
			{
				EnterScope();
			}

			while (!Peek('}') && !Peek(Lexer::Token::Id::EndOfStream))
			{
				Nodes::Statement *statement = nullptr;

				if (!ParseStatement(statement))
				{
					if (scoped)
					{
						LeaveScope();
					}

					while (!Accept('}') && !Peek(Lexer::Token::Id::EndOfStream))
					{
						Consume();
					}

					return false;
				}

				compound->Statements.push_back(statement);
			}

			if (scoped)
			{
				LeaveScope();
			}

			statement = compound;

			return Expect('}');
		}

		// Expressions
		bool Parser::AcceptUnaryOp(Nodes::Unary::Op &op)
		{
			switch (this->mNextToken)
			{
				case Lexer::Token::Id::Exclaim:
					op = Nodes::Unary::Op::LogicalNot;
					break;
				case Lexer::Token::Id::Plus:
					op = Nodes::Unary::Op::None;
					break;
				case Lexer::Token::Id::Minus:
					op = Nodes::Unary::Op::Negate;
					break;
				case Lexer::Token::Id::Tilde:
					op = Nodes::Unary::Op::BitwiseNot;
					break;
				case Lexer::Token::Id::PlusPlus:
					op = Nodes::Unary::Op::Increase;
					break;
				case Lexer::Token::Id::MinusMinus:
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
			switch (this->mNextToken)
			{
				case Lexer::Token::Id::PlusPlus:
					op = Nodes::Unary::Op::PostIncrease;
					break;
				case Lexer::Token::Id::MinusMinus:
					op = Nodes::Unary::Op::PostDecrease;
					break;
				default:
					return false;
			}

			Consume();

			return true;
		}
		bool Parser::PeekMultaryOp(Nodes::Binary::Op &op, unsigned int &precedence)
		{
			switch (this->mNextToken)
			{
				case Lexer::Token::Id::Percent:
					op = Nodes::Binary::Op::Modulo;
					precedence = 11;
					break;
				case Lexer::Token::Id::Ampersand:
					op = Nodes::Binary::Op::BitwiseAnd;
					precedence = 6;
					break;
				case Lexer::Token::Id::Star:
					op = Nodes::Binary::Op::Multiply;
					precedence = 11;
					break;
				case Lexer::Token::Id::Plus:
					op = Nodes::Binary::Op::Add;
					precedence = 10;
					break;
				case Lexer::Token::Id::Minus:
					op = Nodes::Binary::Op::Subtract;
					precedence = 10;
					break;
				case Lexer::Token::Id::Slash:
					op = Nodes::Binary::Op::Divide;
					precedence = 11;
					break;
				case Lexer::Token::Id::Less:
					op = Nodes::Binary::Op::Less;
					precedence = 8;
					break;
				case Lexer::Token::Id::Greater:
					op = Nodes::Binary::Op::Greater;
					precedence = 8;
					break;
				case Lexer::Token::Id::Question:
					op = Nodes::Binary::Op::None;
					precedence = 1;
					break;
				case Lexer::Token::Id::Caret:
					op = Nodes::Binary::Op::BitwiseXor;
					precedence = 5;
					break;
				case Lexer::Token::Id::Pipe:
					op = Nodes::Binary::Op::BitwiseOr;
					precedence = 4;
					break;
				case Lexer::Token::Id::ExclaimEqual:
					op = Nodes::Binary::Op::NotEqual;
					precedence = 7;
					break;
				case Lexer::Token::Id::AmpersandAmpersand:
					op = Nodes::Binary::Op::LogicalAnd;
					precedence = 3;
					break;
				case Lexer::Token::Id::LessLess:
					op = Nodes::Binary::Op::LeftShift;
					precedence = 9;
					break;
				case Lexer::Token::Id::LessEqual:
					op = Nodes::Binary::Op::LessOrEqual;
					precedence = 8;
					break;
				case Lexer::Token::Id::EqualEqual:
					op = Nodes::Binary::Op::Equal;
					precedence = 7;
					break;
				case Lexer::Token::Id::GreaterGreater:
					op = Nodes::Binary::Op::RightShift;
					precedence = 9;
					break;
				case Lexer::Token::Id::GreaterEqual:
					op = Nodes::Binary::Op::GreaterOrEqual;
					precedence = 8;
					break;
				case Lexer::Token::Id::PipePipe:
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
			switch (this->mNextToken)
			{
				case Lexer::Token::Id::Equal:
					op = Nodes::Assignment::Op::None;
					break;
				case Lexer::Token::Id::PercentEqual:
					op = Nodes::Assignment::Op::Modulo;
					break;
				case Lexer::Token::Id::AmpersandEqual:
					op = Nodes::Assignment::Op::BitwiseAnd;
					break;
				case Lexer::Token::Id::StarEqual:
					op = Nodes::Assignment::Op::Multiply;
					break;
				case Lexer::Token::Id::PlusEqual:
					op = Nodes::Assignment::Op::Add;
					break;
				case Lexer::Token::Id::MinusEqual:
					op = Nodes::Assignment::Op::Subtract;
					break;
				case Lexer::Token::Id::SlashEqual:
					op = Nodes::Assignment::Op::Divide;
					break;
				case Lexer::Token::Id::LessLessEqual:
					op = Nodes::Assignment::Op::LeftShift;
					break;
				case Lexer::Token::Id::GreaterGreaterEqual:
					op = Nodes::Assignment::Op::RightShift;
					break;
				case Lexer::Token::Id::CaretEqual:
					op = Nodes::Assignment::Op::BitwiseXor;
					break;
				case Lexer::Token::Id::PipeEqual:
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
				Nodes::Sequence *const sequence = this->mAST.CreateNode<Nodes::Sequence>(node->Location);
				sequence->Expressions.push_back(node);

				node = sequence;
			}

			while (Accept(','))
			{
				Nodes::Expression *expression = nullptr;

				if (!ParseExpressionAssignment(expression))
				{
					return false;
				}

				Nodes::Sequence *const sequence = static_cast<Nodes::Sequence *>(node);
				sequence->Expressions.push_back(expression);
			}

			return true;
		}
		bool Parser::ParseExpressionUnary(Nodes::Expression *&node)
		{
			Nodes::Type type;
			Nodes::Unary::Op op;
			const Node *symbol = nullptr;
			std::string identifier;
			Lexer::Location location = this->mNextToken.GetLocation();

			#pragma region Prefix
			if (AcceptUnaryOp(op))
			{
				location = this->mNextToken.GetLocation();

				if (!ParseExpressionUnary(node))
				{
					return false;
				}

				if (!node->Type.IsScalar() && !node->Type.IsVector() && !node->Type.IsMatrix())
				{
					this->mLexer.Error(location, 3022, "scalar, vector, or matrix expected");

					return false;
				}

				if (op != Nodes::Unary::Op::None)
				{
					if (op == Nodes::Unary::Op::BitwiseNot && !node->Type.IsIntegral())
					{
						this->mLexer.Error(location, 3082, "int or unsigned int type required");

						return false;
					}
					else if ((op == Nodes::Unary::Op::Increase || op == Nodes::Unary::Op::Decrease) && (node->Type.HasQualifier(Nodes::Type::Qualifier::Const) || node->Type.HasQualifier(Nodes::Type::Qualifier::Uniform)))
					{
						this->mLexer.Error(location, 3025, "l-value specifies const object");

						return false;
					}
					
					Nodes::Unary *const newexpression = this->mAST.CreateNode<Nodes::Unary>(location);
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
					else
					{
						if (!(Expect(')') && ParseExpressionUnary(node)))
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
								this->mLexer.Error(location, 3017, "cannot convert these vector types");
								
								return false;
							}

							if (node->Type.Rows > type.Rows || node->Type.Cols > type.Cols)
							{
								this->mLexer.Warning(location, 3206, "implicit truncation of vector type");
							}

							Nodes::Unary *const castexpression = this->mAST.CreateNode<Nodes::Unary>(location);
							type.Qualifiers = Nodes::Type::Qualifier::Const;
							castexpression->Type = type;
							castexpression->Operator = Nodes::Unary::Op::Cast;
							castexpression->Operand = node;

							node = FoldConstantExpression(castexpression);

							return true;
						}
						else
						{
							this->mLexer.Error(location, 3017, "cannot convert non-numeric types");

							return false;
						}
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
			else if (Accept(Lexer::Token::Id::True))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Bool;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = 1;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::False))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Bool;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = 0;

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::IntLiteral))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Int;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Int[0] = this->mToken.GetLiteral<int>();

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::UintLiteral))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Uint;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Uint[0] = this->mToken.GetLiteral<unsigned int>();

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::FloatLiteral))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Float;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Float[0] = this->mToken.GetLiteral<float>();

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::DoubleLiteral))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::Float;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 1, literal->Type.ArrayLength = 0;
				literal->Value.Float[0] = static_cast<float>(this->mToken.GetLiteral<double>());

				node = literal;
				type = literal->Type;
			}
			else if (Accept(Lexer::Token::Id::StringLiteral))
			{
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(location);
				literal->Type.BaseClass = Nodes::Type::Class::String;
				literal->Type.Qualifiers = Nodes::Type::Qualifier::Const;
				literal->Type.Rows = literal->Type.Cols = 0, literal->Type.ArrayLength = 0;
				literal->StringValue = this->mToken.GetLiteral<std::string>();

				while (Accept(Lexer::Token::Id::StringLiteral))
				{
					literal->StringValue += this->mToken.GetLiteral<std::string>();
				}

				node = literal;
				type = literal->Type;
			}
			else if (AcceptTypeClass(type))
			{
				if (!type.IsNumeric())
				{
					this->mLexer.Error(location, 3037, "constructors only defined for numeric base types");

					return false;
				}

				if (!Expect('('))
				{
					return false;
				}

				if (Accept(')'))
				{
					this->mLexer.Error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				Nodes::Constructor *const constructor = this->mAST.CreateNode<Nodes::Constructor>(location);
				type.Qualifiers = Nodes::Type::Qualifier::Const;
				constructor->Type = type;

				unsigned int elements = 0;

				while (!Peek(')'))
				{
					if (!constructor->Arguments.empty() && !Expect(','))
					{
						return false;
					}

					Nodes::Expression *argument = nullptr;
					const Lexer::Location argumentLocation = this->mNextToken.GetLocation();

					if (!ParseExpressionAssignment(argument))
					{
						return false;
					}

					if (!argument->Type.IsNumeric())
					{
						this->mLexer.Error(argumentLocation, 3017, "cannot convert non-numeric types");

						return false;
					}

					elements += argument->Type.Rows * argument->Type.Cols;

					constructor->Arguments.push_back(argument);
				}

				if (!Expect(')'))
				{
					return false;
				}

				if (elements != type.Rows * type.Cols)
				{
					this->mLexer.Error(location, 3014, "incorrect number of arguments to numeric-type constructor");

					return false;
				}

				if (constructor->Arguments.size() > 1)
				{
					node = constructor;
				}
				else
				{
					Nodes::Unary *const castexpression = this->mAST.CreateNode<Nodes::Unary>(constructor->Location);
					castexpression->Type = type;
					castexpression->Operator = Nodes::Unary::Op::Cast;
					castexpression->Operand = constructor->Arguments[0];

					node = castexpression;
				}

				node = FoldConstantExpression(node);
			}
			else if (ExpectIdentifier(identifier))
			{
				symbol = FindSymbol(identifier);

				if (Accept('('))
				{
					if (symbol != nullptr && symbol->NodeId == Node::Id::Variable)
					{
						this->mLexer.Error(location, 3005, "identifier '%s' represents a variable, not a function", identifier.c_str());

						return false;
					}

					Nodes::Call *const callexpression = this->mAST.CreateNode<Nodes::Call>(location);
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

						callexpression->Arguments.push_back(argument);
					}

					if (!Expect(')'))
					{
						return false;
					}

					bool undeclared = symbol == nullptr, intrinsic = false, ambiguous = false;

					if (!ResolveCall(callexpression, intrinsic, ambiguous))
					{
						if (undeclared && !intrinsic)
						{
							this->mLexer.Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());
						}
						else if (ambiguous)
						{
							this->mLexer.Error(location, 3067, "ambiguous function call to '%s'", identifier.c_str());
						}
						else
						{
							this->mLexer.Error(location, 3013, "no matching function overload for '%s'", identifier.c_str());
						}

						return false;
					}

					if (intrinsic)
					{
						Nodes::Intrinsic *const newexpression = this->mAST.CreateNode<Nodes::Intrinsic>(callexpression->Location);
						newexpression->Type = callexpression->Type;
						newexpression->Operator = static_cast<Nodes::Intrinsic::Op>(reinterpret_cast<unsigned int>(callexpression->Callee));

						if (callexpression->Arguments.size() > 0)
						{
							newexpression->Arguments[0] = callexpression->Arguments[0];
						}
						if (callexpression->Arguments.size() > 1)
						{
							newexpression->Arguments[1] = callexpression->Arguments[1];
						}
						if (callexpression->Arguments.size() > 2)
						{
							newexpression->Arguments[2] = callexpression->Arguments[2];
						}

						node = FoldConstantExpression(newexpression);
					}
					else
					{
						const Node *const parent = this->mParentStack.empty() ? nullptr : this->mParentStack.top();

						if (parent == callexpression->Callee)
						{
							this->mLexer.Error(location, 3500, "recursive function calls are not allowed");

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
						this->mLexer.Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

						return false;
					}

					if (symbol->NodeId != Node::Id::Variable)
					{
						this->mLexer.Error(location, 3005, "identifier '%s' represents a function, not a variable", identifier.c_str());

						return false;
					}

					Nodes::LValue *const newexpression = this->mAST.CreateNode<Nodes::LValue>(location);
					newexpression->Reference = static_cast<const Nodes::Variable *>(symbol);
					newexpression->Type = newexpression->Reference->Type;

					node = FoldConstantExpression(newexpression);
					type = node->Type;
				}
			}
			else
			{
				return false;
			}
			#pragma endregion

			#pragma region Postfix
			while (!Peek(Lexer::Token::Id::EndOfStream))
			{
				if (AcceptPostfixOp(op))
				{
					if (!type.IsScalar() && !type.IsVector() && !type.IsMatrix())
					{
						this->mLexer.Error(location, 3022, "scalar, vector, or matrix expected");

						return false;
					}

					if (type.HasQualifier(Nodes::Type::Qualifier::Const) || type.HasQualifier(Nodes::Type::Qualifier::Uniform))
					{
						this->mLexer.Error(location, 3025, "l-value specifies const object");

						return false;
					}

					Nodes::Unary *const newexpression = this->mAST.CreateNode<Nodes::Unary>(location);
					newexpression->Type = type;
					newexpression->Type.Qualifiers |= Nodes::Type::Qualifier::Const;
					newexpression->Operator = op;
					newexpression->Operand = node;
		
					node = newexpression;
					type = node->Type;
				}
				else if (Accept('.'))
				{
					std::string subscript;

					if (!ExpectIdentifier(subscript))
					{
						return false;
					}

					location = this->mToken.GetLocation();

					if (Accept('('))
					{
						if (!type.IsStruct() || type.IsArray())
						{
							this->mLexer.Error(location, 3087, "object does not have methods");
						}
						else
						{
							this->mLexer.Error(location, 3088, "structures do not have methods");
						}

						return false;
					}
					else if (type.IsArray())
					{
						this->mLexer.Error(location, 3018, "invalid subscript on array");

						return false;
					}
					else if (type.IsVector())
					{
						const std::size_t length = subscript.size();

						if (length > 4)
						{
							this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						bool constant = false;
						signed char offsets[4] = { -1, -1, -1, -1 };
						enum { xyzw, rgba, stpq } set[4];

						for (std::size_t i = 0; i < length; ++i)
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
									this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());
									return false;
							}

							if (i > 0 && (set[i] != set[i - 1]))
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}
							if (static_cast<unsigned int>(offsets[i]) >= type.Rows)
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());
	
								return false;
							}
		
							for (std::size_t k = 0; k < i; ++k)
							{
								if (offsets[k] == offsets[i])
								{
									constant = true;
									break;
								}
							}
						}

						Nodes::Swizzle *const newexpression = this->mAST.CreateNode<Nodes::Swizzle>(location);
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
						const std::size_t length = subscript.size();

						if (length < 3)
						{
							this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						bool constant = false;
						signed char offsets[4] = { -1, -1, -1, -1 };
						const unsigned int set = subscript[1] == 'm';
						const char coefficient = static_cast<char>(!set);

						for (std::size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
						{
							if (subscript[i] != '_' || subscript[i + set + 1] < '0' + coefficient || subscript[i + set + 1] > '3' + coefficient || subscript[i + set + 2] < '0' + coefficient || subscript[i + set + 2] > '3' + coefficient)
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}
							if (set && subscript[i + 1] != 'm')
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s', mixed swizzle sets", subscript.c_str());

								return false;
							}

							const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
							const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

							if ((row >= type.Rows || col >= type.Cols) || j > 3)
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s', swizzle out of range", subscript.c_str());

								return false;
							}

							offsets[j] = static_cast<unsigned char>(row * 4 + col);
		
							for (std::size_t k = 0; k < j; ++k)
							{
								if (offsets[k] == offsets[j])
								{
									constant = true;
									break;
								}
							}
						}

						Nodes::Swizzle *const newexpression = this->mAST.CreateNode<Nodes::Swizzle>(this->mToken.GetLocation());
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

						for (Nodes::Variable *currfield : type.Definition->Fields)
						{
							if (currfield->Name == subscript)
							{
								field = currfield;
								break;
							}
						}

						if (field == nullptr)
						{
							this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

							return false;
						}

						Nodes::FieldSelection *const newexpression = this->mAST.CreateNode<Nodes::FieldSelection>(location);
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
						const std::size_t length = subscript.size();

						for (std::size_t i = 0; i < length; ++i)
						{
							if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
							{
								this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

								return false;
							}

							offsets[i] = 0;
						}

						Nodes::Swizzle *const newexpression = this->mAST.CreateNode<Nodes::Swizzle>(location);
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
						this->mLexer.Error(location, 3018, "invalid subscript '%s'", subscript.c_str());

						return false;
					}
				}
				else if (Accept('['))
				{
					if (!type.IsArray() && !type.IsVector() && !type.IsMatrix())
					{
						this->mLexer.Error(location, 3121, "array, matrix, vector, or indexable object type expected in index expression");

						return false;
					}

					Nodes::Binary *const newexpression = this->mAST.CreateNode<Nodes::Binary>(location);
					newexpression->Type = type;
					newexpression->Operator = Nodes::Binary::Op::ElementExtract;
					newexpression->Operands[0] = node;

					location = this->mNextToken.GetLocation();

					if (!ParseExpression(newexpression->Operands[1]))
					{
						return false;
					}

					if (!newexpression->Operands[1]->Type.IsScalar())
					{
						this->mLexer.Error(location, 3120, "invalid type for index - index must be a scalar");

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
			const Lexer::Location leftLocation = this->mNextToken.GetLocation();

			if (!ParseExpressionUnary(left))
			{
				return false;
			}

			Nodes::Binary::Op op;
			unsigned int rightPrecedence;

			while (PeekMultaryOp(op, rightPrecedence))
			{
				bool boolean = false;
				Lexer::Location rightLocation = this->mNextToken.GetLocation();
				Nodes::Expression *right1 = nullptr, *right2 = nullptr;

				if (rightPrecedence <= leftPrecedence)
				{
					break;
				}
				else
				{
					Consume();
				}

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
							this->mLexer.Error(rightLocation, 3020, "type mismatch");

							return false;
						}
					}
					else if (op == Nodes::Binary::Op::BitwiseAnd || op == Nodes::Binary::Op::BitwiseOr || op == Nodes::Binary::Op::BitwiseXor)
					{
						if (!left->Type.IsIntegral())
						{
							this->mLexer.Error(leftLocation, 3082, "int or unsigned int type required");

							return false;
						}
						if (!right1->Type.IsIntegral())
						{
							this->mLexer.Error(rightLocation, 3082, "int or unsigned int type required");

							return false;
						}
					}
					else
					{
						boolean = op == Nodes::Binary::Op::LogicalAnd || op == Nodes::Binary::Op::LogicalOr || op == Nodes::Binary::Op::Less || op == Nodes::Binary::Op::Greater || op == Nodes::Binary::Op::LessOrEqual || op == Nodes::Binary::Op::GreaterOrEqual;

						if (!left->Type.IsScalar() && !left->Type.IsVector() && !left->Type.IsMatrix())
						{
							this->mLexer.Error(leftLocation, 3022, "scalar, vector, or matrix expected");

							return false;
						}
						if (!right1->Type.IsScalar() && !right1->Type.IsVector() && !right1->Type.IsMatrix())
						{
							this->mLexer.Error(rightLocation, 3022, "scalar, vector, or matrix expected");

							return false;
						}
					}

					Nodes::Binary *const newexpression = this->mAST.CreateNode<Nodes::Binary>(leftLocation);
					newexpression->Operator = op;
					newexpression->Operands[0] = left;
					newexpression->Operands[1] = right1;

					right2 = right1, right1 = left, rightLocation = leftLocation;
					left = newexpression;
				}
				else
				{
					if (!left->Type.IsScalar() && !left->Type.IsVector())
					{
						this->mLexer.Error(leftLocation, 3022, "boolean or vector expression expected");

						return false;
					}

					if (!(ParseExpression(right1) && Expect(':') && ParseExpressionAssignment(right2)))
					{
						return false;
					}
					
					if (right1->Type.IsArray() || right2->Type.IsArray() || right1->Type.Definition != right2->Type.Definition)
					{
						this->mLexer.Error(leftLocation, 3020, "type mismatch between conditional values");

						return false;
					}

					Nodes::Conditional *const newexpression = this->mAST.CreateNode<Nodes::Conditional>(leftLocation);
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
						this->mLexer.Warning(rightLocation, 3206, "implicit truncation of vector type");
					}
					if (right2->Type.Rows > right1->Type.Rows || right2->Type.Cols > right1->Type.Cols)
					{
						this->mLexer.Warning(this->mToken.GetLocation(), 3206, "implicit truncation of vector type");
					}
				}

				left = FoldConstantExpression(left);
			}

			return true;
		}
		bool Parser::ParseExpressionAssignment(Nodes::Expression *&node)
		{
			Nodes::Expression *left, *right;
			const Lexer::Location leftLocation = this->mNextToken.GetLocation();

			if (!ParseExpressionMultary(left))
			{
				return false;
			}

			node = left;

			Nodes::Assignment::Op op;

			if (AcceptAssignmentOp(op))
			{
				const Lexer::Location rightLocation = this->mNextToken.GetLocation();

				if (!ParseExpressionMultary(right))
				{
					return false;
				}

				if (left->Type.HasQualifier(Nodes::Type::Qualifier::Const) || left->Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					this->mLexer.Error(leftLocation, 3025, "l-value specifies const object");

					return false;
				}

				if (left->Type.IsArray() || right->Type.IsArray() || left->Type.Definition != right->Type.Definition || ((left->Type.Rows > right->Type.Rows || left->Type.Cols > right->Type.Cols) && !(right->Type.Rows == 1 && right->Type.Cols == 1)))
				{
					this->mLexer.Error(rightLocation, 3020, "type mismatch");

					return false;
				}

				if (right->Type.Rows > left->Type.Rows || right->Type.Cols > left->Type.Cols)
				{
					this->mLexer.Warning(rightLocation, 3206, "implicit truncation of vector type");
				}

				Nodes::Assignment *const assignment = this->mAST.CreateNode<Nodes::Assignment>(left->Location);
				assignment->Type = left->Type;
				assignment->Operator = op;
				assignment->Left = left;
				assignment->Right = right;

				node = assignment;
			}

			return true;
		}

		// Declarations
		bool Parser::ParseTopLevel()
		{
			Nodes::Type type = { Nodes::Type::Class::Void };

			if (Peek(Lexer::Token::Id::Technique))
			{
				Nodes::Technique *technique = nullptr;

				if (!ParseTechnique(technique))
				{
					return false;
				}

				this->mAST.Techniques.push_back(technique);
			}
			else if (Peek(Lexer::Token::Id::Struct))
			{
				Nodes::Struct *structure = nullptr;

				if (!ParseStruct(structure))
				{
					return false;
				}

				if (Peek(Lexer::Token::Id::Identifier))
				{
					std::string name;
					unsigned int count = 0;

					type.BaseClass = Nodes::Type::Class::Struct;
					type.Definition = structure;

					while (!Peek(';'))
					{
						if (count++ > 0 && !Expect(','))
						{
							return false;
						}

						if (!ExpectIdentifier(name))
						{
							return false;
						}

						Nodes::Variable *variable = nullptr;

						if (!ParseVariableResidue(type, name, variable))
						{
							return false;
						}

						this->mAST.Uniforms.push_back(variable);
					}
				}

				if (!Expect(';'))
				{
					return false;
				}
			}
			else if (ParseType(type))
			{
				std::string name;
				const Lexer::Location location = this->mToken.GetLocation();

				if (!ExpectIdentifier(name))
				{
					return false;
				}

				if (Peek('('))
				{
					Nodes::Function *function = nullptr;

					if (!ParseFunctionResidue(type, name, function))
					{
						return false;
					}

					this->mAST.Functions.push_back(function);
				}
				else
				{
					unsigned int count = 0;

					while (!Peek(';'))
					{
						if (count++ > 0 && !(Expect(',') && ExpectIdentifier(name)))
						{
							return false;
						}

						Nodes::Variable *variable = nullptr;

						if (!ParseVariableResidue(type, name, variable))
						{
							return false;
						}

						this->mAST.Uniforms.push_back(variable);
					}

					if (!Expect(';'))
					{
						return false;
					}
				}
			}
			else if (!Accept(';'))
			{
				return false;
			}

			return true;
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
				else if (ParseExpression(expression) && Expect(']'))
				{
					if (expression->NodeId != Node::Id::Literal || !(expression->Type.IsScalar() && expression->Type.IsIntegral()))
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3058, "array dimensions must be literal scalar expressions");
						return false;
					}

					size = static_cast<Nodes::Literal *>(expression)->Value.Int[0];

					if (size < 1 || size > 65536)
					{
						this->mLexer.Error(this->mToken.GetLocation(), 3059, "array dimension must be between 1 and 65536");
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

				if (!ExpectIdentifier(annotation.Name))
				{
					return false;
				}

				Nodes::Expression *expression = nullptr;
		
				if (!(Expect('=') && ParseExpressionAssignment(expression) && Expect(';')))
				{
					return false;
				}

				if (expression->NodeId != Node::Id::Literal)
				{
					continue;
				}

				annotation.Value = static_cast<Nodes::Literal *>(expression);

				annotations.push_back(annotation);
			}

			return Expect('>');
		}
		bool Parser::ParseStruct(Nodes::Struct *&structure)
		{
			if (!Accept(Lexer::Token::Id::Struct))
			{
				return false;
			}

			const Lexer::Location location = this->mToken.GetLocation();

			structure = this->mAST.CreateNode<Nodes::Struct>(location);

			if (AcceptIdentifier(structure->Name))
			{
				if (!InsertSymbol(structure))
				{
					this->mLexer.Error(this->mToken.GetLocation(), 3003, "redefinition of '%s'", structure->Name.c_str());

					return false;
				}
			}
			else
			{
				structure->Name = "__anonymous_struct_" + std::to_string(this->mToken.GetLocation().Line) + '_' + std::to_string(this->mToken.GetLocation().Column);
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
					return false;
				}

				if (type.IsVoid())
				{
					this->mLexer.Error(this->mNextToken.GetLocation(), 3038, "struct members cannot be void");
					
					return false;
				}
				if (type.HasQualifier(Nodes::Type::Qualifier::In) || type.HasQualifier(Nodes::Type::Qualifier::Out))
				{
					this->mLexer.Error(this->mNextToken.GetLocation(), 3055, "struct members cannot be declared 'in' or 'out'");
					
					return false;
				}

				unsigned int count = 0;

				do
				{
					if (count++ > 0 && !Expect(','))
					{
						return false;
					}

					Nodes::Variable *field = this->mAST.CreateNode<Nodes::Variable>(this->mNextToken.GetLocation());

					if (!ExpectIdentifier(field->Name))
					{
						return false;
					}

					field->Type = type;

					ParseArray(field->Type.ArrayLength);
					
					if (Accept(':'))
					{
						if (!ExpectIdentifier(field->Semantic))
						{
							return false;
						}

						boost::to_upper(field->Semantic);
					}

					structure->Fields.push_back(field);
				}
				while (!Peek(';'));

				if (!Expect(';'))
				{
					return false;
				}
			}

			if (structure->Fields.empty())
			{
				this->mLexer.Warning(location, 5001, "struct has no members");
			}

			this->mAST.Types.push_back(structure);

			return Expect('}');
		}
		bool Parser::ParseDeclaration(Nodes::DeclaratorList *&declarators)
		{
			Nodes::Type type;
			std::string name;

			const Lexer::Location location = this->mNextToken.GetLocation();

			if (!ParseType(type))
			{
				return false;
			}

			declarators = this->mAST.CreateNode<Nodes::DeclaratorList>(location);

			unsigned int count = 0;

			do
			{
				if (count++ > 0 && !Expect(','))
				{
					return false;
				}

				if (!ExpectIdentifier(name))
				{
					return false;
				}

				Nodes::Variable *declarator = nullptr;

				if (!ParseVariableResidue(type, name, declarator))
				{
					return false;
				}

				declarators->Declarators.push_back(declarator);
			}
			while (!Peek(';'));

			return true;
		}
		bool Parser::ParseFunctionResidue(Nodes::Type &type, const std::string &name, Nodes::Function *&function)
		{
			const Lexer::Location location = this->mToken.GetLocation();

			if (!Expect('('))
			{
				return false;
			}

			if (type.Qualifiers != 0)
			{
				this->mLexer.Error(location, 3047, "function return type cannot have any qualifiers");
			
				return false;
			}

			function = this->mAST.CreateNode<Nodes::Function>(location);

			function->ReturnType = type;
			function->ReturnType.Qualifiers = Nodes::Type::Qualifier::Const;
			function->Name = name;

			InsertSymbol(function);

			EnterScope(function);

			while (!Peek(')'))
			{
				if (!function->Parameters.empty() && !Expect(','))
				{
					LeaveScope();

					return false;
				}

				Nodes::Variable *const parameter = this->mAST.CreateNode<Nodes::Variable>(Lexer::Location());

				if (!ParseType(parameter->Type) || !ExpectIdentifier(parameter->Name))
				{
					LeaveScope();

					return false;
				}

				const Lexer::Location parameterLocation = this->mToken.GetLocation();
				parameter->Location = parameterLocation;

				if (parameter->Type.IsVoid())
				{
					LeaveScope();

					this->mLexer.Error(parameterLocation, 3038, "function parameters cannot be void");

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Extern))
				{
					LeaveScope();

					this->mLexer.Error(parameterLocation, 3006, "function parameters cannot be declared 'extern'");

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Static))
				{
					LeaveScope();

					this->mLexer.Error(parameterLocation, 3007, "function parameters cannot be declared 'static'");

					return false;
				}
				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					LeaveScope();

					this->mLexer.Error(parameterLocation, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");

					return false;
				}

				if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Out))
				{
					if (parameter->Type.HasQualifier(Nodes::Type::Qualifier::Const))
					{
						LeaveScope();

						this->mLexer.Error(parameterLocation, 3046, "output parameters cannot be declared 'const'");
						
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
					LeaveScope();

					this->mLexer.Error(parameterLocation, 3003, "redefinition of '%s'", parameter->Name.c_str());

					return false;
				}
							
				if (Accept(':'))
				{
					if (!ExpectIdentifier(parameter->Semantic))
					{
						LeaveScope();

						return false;
					}

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
				if (!ExpectIdentifier(function->ReturnSemantic))
				{
					LeaveScope();

					return false;
				}

				boost::to_upper(function->ReturnSemantic);

				if (type.IsVoid())
				{
					this->mLexer.Error(this->mToken.GetLocation(), 3076, "void function cannot have a semantic");

					return false;
				}
			}

			if (!ParseStatementCompound(reinterpret_cast<Nodes::Statement *&>(function->Definition)))
			{
				LeaveScope();

				return false;
			}

			LeaveScope();

			return true;
		}
		bool Parser::ParseVariableResidue(Nodes::Type &type, const std::string &name, Nodes::Variable *&variable)
		{
			const Lexer::Location location = this->mToken.GetLocation();

			if (type.IsVoid())
			{
				this->mLexer.Error(location, 3038, "variables cannot be void");

				return false;
			}
			else if (type.HasQualifier(Nodes::Type::Qualifier::In) || type.HasQualifier(Nodes::Type::Qualifier::Out))
			{
				this->mLexer.Error(location, 3055, "variables cannot be declared 'in' or 'out'");
				
				return false;
			}

			const Node *const parent = this->mParentStack.empty() ? nullptr : this->mParentStack.top();

			if (parent == nullptr)
			{
				if (!type.HasQualifier(Nodes::Type::Qualifier::Static))
				{
					if (!type.HasQualifier(Nodes::Type::Qualifier::Uniform) && !(type.IsTexture() || type.IsSampler()))
					{
						this->mLexer.Warning(location, 5000, "global variables are considered 'uniform' by default");
					}

					type.Qualifiers |= Nodes::Type::Qualifier::Extern | Nodes::Type::Qualifier::Uniform;
				}
			}
			else
			{
				if (type.HasQualifier(Nodes::Type::Qualifier::Extern))
				{
					this->mLexer.Error(location, 3006, "local variables cannot be declared 'extern'");
					
					return false;
				}
				if (type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					this->mLexer.Error(location, 3047, "local variables cannot be declared 'uniform'");
				
					return false;
				}
			}

			variable = this->mAST.CreateNode<Nodes::Variable>(location);

			variable->Type = type;
			variable->Name = name;

			ParseArray(variable->Type.ArrayLength);

			if (!InsertSymbol(variable))
			{
				this->mLexer.Error(location, 3003, "redefinition of '%s'", name.c_str());

				return false;
			}

			if (Accept(':'))
			{
				if (!ExpectIdentifier(variable->Semantic))
				{
					return false;
				}

				boost::to_upper(variable->Semantic);
			}

			ParseAnnotations(variable->Annotations);

			if (Accept('='))
			{
				const Lexer::Location location = this->mToken.GetLocation();

				if (!ParseVariableAssignment(variable->Initializer))
				{
					return false;
				}

				if (parent == nullptr && variable->Initializer->NodeId != Node::Id::Literal)
				{
					this->mLexer.Error(location, 3011, "initial value must be a literal expression");
						
					return false;
				}

				if ((variable->Initializer->Type.Rows < type.Rows || variable->Initializer->Type.Cols < type.Cols) && !variable->Initializer->Type.IsScalar())
				{
					this->mLexer.Error(location, 3017, "cannot implicitly convert these vector types");
						
					return false;
				}

				if (variable->Initializer->Type.Rows > type.Rows || variable->Initializer->Type.Cols > type.Cols)
				{
					this->mLexer.Warning(location, 3206, "implicit truncation of vector type");
				}
			}
			else if (type.IsNumeric())
			{
				if (type.HasQualifier(Nodes::Type::Qualifier::Const))
				{
					this->mLexer.Error(location, 3012, "missing initial value for '%s'", name.c_str());

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
				Nodes::InitializerList *const initializerlist = this->mAST.CreateNode<Nodes::InitializerList>(this->mToken.GetLocation());

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
						while (!Accept('}') && !Peek(Lexer::Token::Id::EndOfStream))
						{
							Consume();
						}

						return false;
					}

					if (expression->NodeId == Node::Id::InitializerList && static_cast<Nodes::InitializerList *>(expression)->Values.empty())
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
				std::string name;

				if (!ExpectIdentifier(name))
				{
					return false;
				}

				Nodes::Expression *value = nullptr;
				const Lexer::Location location = this->mToken.GetLocation();

				if (!(Expect('=') && ParseVariablePropertiesExpression(value) && Expect(';')))
				{
					return false;
				}

				if (name == "Texture")
				{
					if (value->NodeId != Node::Id::LValue || static_cast<Nodes::LValue *>(value)->Reference->NodeId != Node::Id::Variable || !static_cast<Nodes::LValue *>(value)->Reference->Type.IsTexture() || static_cast<Nodes::LValue *>(value)->Reference->Type.IsArray())
					{
						this->mLexer.Error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					variable->Properties.Texture = static_cast<Nodes::LValue *>(value)->Reference;
				}
				else
				{
					if (value->NodeId != Node::Id::Literal)
					{
						this->mLexer.Error(location, 3011, "value must be a literal expression");

						return false;
					}

					Nodes::Literal *const valueLiteral = static_cast<Nodes::Literal *>(value);

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
						this->mLexer.Error(location, 3004, "unrecognized property '%s'", name.c_str());

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
			std::string identifier;
			const Lexer::Location location = this->mNextToken.GetLocation();

			if (AcceptIdentifier(identifier))
			{
				static const std::unordered_map<std::string, unsigned int> sEnums = boost::assign::map_list_of
					("NONE", Nodes::Variable::Properties::NONE)
					("POINT", Nodes::Variable::Properties::POINT)
					("LINEAR", Nodes::Variable::Properties::LINEAR)
					("ANISOTROPIC", Nodes::Variable::Properties::ANISOTROPIC)
					("CLAMP", Nodes::Variable::Properties::CLAMP)
					("REPEAT", Nodes::Variable::Properties::REPEAT)
					("MIRROR", Nodes::Variable::Properties::MIRROR)
					("BORDER", Nodes::Variable::Properties::BORDER)
					("R8", Nodes::Variable::Properties::R8)
					("R32F", Nodes::Variable::Properties::R32F)
					("RG8", Nodes::Variable::Properties::RG8)
					("R8G8", Nodes::Variable::Properties::RG8)
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
					Nodes::Literal *const newexpression = this->mAST.CreateNode<Nodes::Literal>(location);
					newexpression->Type.BaseClass = Nodes::Type::Class::Uint;
					newexpression->Type.Rows = newexpression->Type.Cols = 1, newexpression->Type.ArrayLength = 0;
					newexpression->Value.Uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				const Node *symbol = FindSymbol(identifier);
					
				if (symbol == nullptr)
				{
					this->mLexer.Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

					return false;
				}

				Nodes::LValue *const newexpression = this->mAST.CreateNode<Nodes::LValue>(location);
				newexpression->Reference = static_cast<const Nodes::Variable *>(symbol);
				newexpression->Type = newexpression->Reference->Type;

				expression = newexpression;

				return true;
			}

			return ParseExpressionMultary(expression);
		}
		bool Parser::ParseTechnique(Nodes::Technique *&technique)
		{
			if (!Accept(Lexer::Token::Id::Technique))
			{
				return false;
			}

			technique = this->mAST.CreateNode<Nodes::Technique>(this->mToken.GetLocation());

			if (!ExpectIdentifier(technique->Name))
			{
				return false;
			}

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

				technique->Passes.push_back(pass);
			}

			return Expect('}');
		}
		bool Parser::ParseTechniquePass(Nodes::Pass *&pass)
		{
			if (!Accept(Lexer::Token::Id::Pass))
			{
				return false;
			}

			pass = this->mAST.CreateNode<Nodes::Pass>(this->mToken.GetLocation());

			AcceptIdentifier(pass->Name);
			ParseAnnotations(pass->Annotations);

			if (!Expect('{'))
			{
				return false;
			}

			while (!Peek('}'))
			{
				std::string passstate;
				Nodes::Expression *value = nullptr;
				const Lexer::Location location = this->mNextToken.GetLocation();

				if (!(ExpectIdentifier(passstate) && Expect('=') && ParseTechniquePassExpression(value) && Expect(';')))
				{
					return false;
				}

				if (passstate == "VertexShader" || passstate == "PixelShader")
				{
					if (value->NodeId != Node::Id::LValue || static_cast<Nodes::LValue *>(value)->Reference->NodeId != Node::Id::Function)
					{
						this->mLexer.Error(location, 3020, "type mismatch, expected function name");

						return false;
					}

					(passstate[0] == 'V' ? pass->States.VertexShader : pass->States.PixelShader) = reinterpret_cast<const Nodes::Function *>(static_cast<Nodes::LValue *>(value)->Reference);
				}
				else if (boost::starts_with(passstate, "RenderTarget"))
				{
					std::size_t index = 0;

					if (passstate.size() == 13)
					{
						index = passstate[12] - '0';
					}
					if (passstate.size() > 13 || index >= 8)
					{
						return false;
					}

					if (value->NodeId != Node::Id::LValue || static_cast<Nodes::LValue *>(value)->Reference->NodeId != Node::Id::Variable || !static_cast<Nodes::LValue *>(value)->Reference->Type.IsTexture() || static_cast<Nodes::LValue *>(value)->Reference->Type.IsArray())
					{
						this->mLexer.Error(location, 3020, "type mismatch, expected texture name");

						return false;
					}

					pass->States.RenderTargets[index] = static_cast<Nodes::LValue *>(value)->Reference;
				}
				else
				{
					if (value->NodeId != Node::Id::Literal)
					{
						this->mLexer.Error(location, 3011, "pass state value must be a literal expression");

						return false;
					}

					Nodes::Literal *const valueLiteral = static_cast<Nodes::Literal *>(value);

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
						this->mLexer.Error(location, 3004, "unrecognized pass state '%s'", passstate.c_str());

						return false;
					}
				}
			}

			return Expect('}');
		}
		bool Parser::ParseTechniquePassExpression(Nodes::Expression *&expression)
		{
			std::string identifier;
			const Lexer::Location location = this->mNextToken.GetLocation();

			if (AcceptIdentifier(identifier))
			{
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
					Nodes::Literal *const newexpression = this->mAST.CreateNode<Nodes::Literal>(location);
					newexpression->Type.BaseClass = Nodes::Type::Class::Uint;
					newexpression->Type.Rows = newexpression->Type.Cols = 1, newexpression->Type.ArrayLength = 0;
					newexpression->Value.Uint[0] = it->second;

					expression = newexpression;

					return true;
				}

				const Node *symbol = FindSymbol(identifier);
					
				if (symbol == nullptr)
				{
					this->mLexer.Error(location, 3004, "undeclared identifier '%s'", identifier.c_str());

					return false;
				}

				Nodes::LValue *const newexpression = this->mAST.CreateNode<Nodes::LValue>(location);
				newexpression->Reference = static_cast<const Nodes::Variable *>(symbol);
				newexpression->Type = symbol->NodeId == Node::Id::Function ? static_cast<const Nodes::Function *>(symbol)->ReturnType : newexpression->Reference->Type;

				expression = newexpression;

				return true;
			}
			
			return ParseExpressionMultary(expression);
		}

		// Symbol Table
		void Parser::EnterScope(Node *parent)
		{
			if (parent != nullptr || this->mParentStack.empty())
			{
				this->mParentStack.push(parent);
			}
			else
			{
				this->mParentStack.push(this->mParentStack.top());
			}

			this->mCurrentScope++;
		}
		void Parser::LeaveScope()
		{
			for (auto it = this->mSymbolStack.begin(), end = this->mSymbolStack.end(); it != end; ++it)
			{
				auto &scopes = it->second;

				if (scopes.empty())
				{
					continue;
				}
		
				for (auto it = scopes.begin(); it != scopes.end();)
				{
					if (it->first >= this->mCurrentScope)
					{
						it = scopes.erase(it);
					}
					else
					{
						++it;
					}
				}
			}

			this->mParentStack.pop();

			this->mCurrentScope--;
		}
		bool Parser::InsertSymbol(Node *symbol)
		{
			std::string name;

			switch (symbol->NodeId)
			{
				case Node::Id::Struct:
					name = static_cast<Nodes::Struct *>(symbol)->Name;
					break;
				case Node::Id::Variable:
					name = static_cast<Nodes::Variable *>(symbol)->Name;
					break;
				case Node::Id::Function:
					name = static_cast<Nodes::Function *>(symbol)->Name;
					break;
				default:
					return false;
			}

			if (symbol->NodeId != Node::Id::Function && FindSymbol(name, this->mCurrentScope, true) != nullptr)
			{
				return false;
			}
	
			this->mSymbolStack[name].push_back(std::make_pair(this->mCurrentScope, symbol));

			return true;
		}
		Node *Parser::FindSymbol(const std::string &name) const
		{
			return FindSymbol(name, this->mCurrentScope, false);
		}
		Node *Parser::FindSymbol(const std::string &name, unsigned int scope, bool exclusive) const
		{
			const auto it = this->mSymbolStack.find(name);

			if (it == this->mSymbolStack.end() || it->second.empty())
			{
				return nullptr;
			}

			Node *result = nullptr;
			const auto &scopes = it->second;
	
			for (auto it = scopes.rbegin(), end = scopes.rend(); it != end; ++it)
			{
				if (it->first > scope)
				{
					continue;
				}
				if (exclusive && it->first < scope)
				{
					continue;
				}
		
				if (it->second->NodeId == Node::Id::Variable || it->second->NodeId == Node::Id::Struct)
				{
					return it->second;
				}
				else if (result == nullptr)
				{
					result = it->second;
				}
			}
	
			return result;
		}
		bool Parser::ResolveCall(Nodes::Call *call, bool &isIntrinsic, bool &isAmbiguous) const
		{
			isIntrinsic = false;
			isAmbiguous = false;

			unsigned int overloadCount = 0;
			const Nodes::Function *overload = nullptr;
			Nodes::Intrinsic::Op intrinsicOp = Nodes::Intrinsic::Op::None;

			const auto it = this->mSymbolStack.find(call->CalleeName);

			if (it != this->mSymbolStack.end() && !it->second.empty())
			{
				const auto &scopes = it->second;

				for (auto it = scopes.rbegin(), end = scopes.rend(); it != end; ++it)
				{
					if (it->first > this->mCurrentScope || it->second->NodeId != Node::Id::Function)
					{
						continue;
					}

					const Nodes::Function *function = static_cast<Nodes::Function *>(it->second);

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
					}
					else if (comparison == 0)
					{
						++overloadCount;
					}
				}
			}

			for (auto &intrinsic : sIntrinsics)
			{
				if (intrinsic.Function.Name == call->CalleeName)
				{
					if (call->Arguments.size() != intrinsic.Function.Parameters.size())
					{
						isIntrinsic = overloadCount == 0;
						break;
					}

					const int comparison = CompareFunctions(call, &intrinsic.Function, overload);

					if (comparison < 0)
					{
						overload = &intrinsic.Function;
						overloadCount = 1;

						isIntrinsic = true;
						intrinsicOp = intrinsic.Op;
					}
					else if (comparison == 0)
					{
						++overloadCount;
					}
				}
			}

			if (overloadCount == 1)
			{
				call->Type = overload->ReturnType;

				if (isIntrinsic)
				{
					call->Callee = reinterpret_cast<Nodes::Function *>(static_cast<unsigned int>(intrinsicOp));
				}
				else
				{
					call->Callee = overload;
				}

				return true;
			}
			else
			{
				isAmbiguous = overloadCount > 1;

				return false;
			}
		}
		Nodes::Expression *Parser::FoldConstantExpression(Nodes::Expression *expression)
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

			if (expression->NodeId == Node::Id::Unary)
			{
				Nodes::Unary *const unaryexpression = static_cast<Nodes::Unary *>(expression);

				if (unaryexpression->Operand->NodeId != Node::Id::Literal)
				{
					return expression;
				}

				Nodes::Literal *const operand = static_cast<Nodes::Literal *>(unaryexpression->Operand);

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
			else if (expression->NodeId == Node::Id::Binary)
			{
				Nodes::Binary *const binaryexpression = static_cast<Nodes::Binary *>(expression);

				if (binaryexpression->Operands[0]->NodeId != Node::Id::Literal || binaryexpression->Operands[1]->NodeId != Node::Id::Literal)
				{
					return expression;
				}

				Nodes::Literal *const left = static_cast<Nodes::Literal *>(binaryexpression->Operands[0]);
				Nodes::Literal *const right = static_cast<Nodes::Literal *>(binaryexpression->Operands[1]);
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
			else if (expression->NodeId == Node::Id::Intrinsic)
			{
				Nodes::Intrinsic *const intrinsicexpression = static_cast<Nodes::Intrinsic *>(expression);

				if ((intrinsicexpression->Arguments[0] != nullptr && intrinsicexpression->Arguments[0]->NodeId != Node::Id::Literal) || (intrinsicexpression->Arguments[1] != nullptr && intrinsicexpression->Arguments[1]->NodeId != Node::Id::Literal) || (intrinsicexpression->Arguments[2] != nullptr && intrinsicexpression->Arguments[2]->NodeId != Node::Id::Literal))
				{
					return expression;
				}

				Nodes::Literal *const operand = static_cast<Nodes::Literal *>(intrinsicexpression->Arguments[0]);
				Nodes::Literal *const left = operand;
				Nodes::Literal *const right = static_cast<Nodes::Literal *>(intrinsicexpression->Arguments[1]);

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
			else if (expression->NodeId == Node::Id::Constructor)
			{
				Nodes::Constructor *const constructor = static_cast<Nodes::Constructor *>(expression);

				for (auto argument : constructor->Arguments)
				{
					if (argument->NodeId != Node::Id::Literal)
					{
						return expression;
					}
				}

				unsigned int k = 0;
				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(constructor->Location);
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
			else if (expression->NodeId == Node::Id::LValue)
			{
				const Nodes::Variable *variable = static_cast<Nodes::LValue *>(expression)->Reference;

				if (variable->Initializer == nullptr || !(variable->Initializer->NodeId == Node::Id::Literal && variable->Type.HasQualifier(Nodes::Type::Qualifier::Const)))
				{
					return expression;
				}

				Nodes::Literal *const literal = this->mAST.CreateNode<Nodes::Literal>(expression->Location);
				literal->Type = expression->Type;
				literal->Value = static_cast<const Nodes::Literal *>(variable->Initializer)->Value;

				expression = literal;
			}

			return expression;
		}
	}
}