/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_runtime.hpp"
#include "d3d9_effect_compiler.hpp"
#include <assert.h>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <d3dcompiler.h>

namespace reshadefx
{
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
}

namespace reshade::d3d9
{
	using namespace reshadefx;
	using namespace reshadefx::nodes;

	static inline bool is_pow2(int x)
	{
		return ((x > 0) && ((x & (x - 1)) == 0));
	}
	static D3DBLEND literal_to_blend_func(unsigned int value)
	{
		switch (value)
		{
			case pass_declaration_node::ZERO:
				return D3DBLEND_ZERO;
			default:
			case pass_declaration_node::ONE:
				return D3DBLEND_ONE;
			case pass_declaration_node::SRCCOLOR:
				return D3DBLEND_SRCCOLOR;
			case pass_declaration_node::INVSRCCOLOR:
				return D3DBLEND_INVSRCCOLOR;
			case pass_declaration_node::SRCALPHA:
				return D3DBLEND_SRCALPHA;
			case pass_declaration_node::INVSRCALPHA:
				return D3DBLEND_INVSRCALPHA;
			case pass_declaration_node::DESTALPHA:
				return D3DBLEND_DESTALPHA;
			case pass_declaration_node::INVDESTALPHA:
				return D3DBLEND_INVDESTALPHA;
			case pass_declaration_node::DESTCOLOR:
				return D3DBLEND_DESTCOLOR;
			case pass_declaration_node::INVDESTCOLOR:
				return D3DBLEND_INVDESTCOLOR;
		}
	}
	static D3DSTENCILOP literal_to_stencil_op(unsigned int value)
	{
		switch (value)
		{
			default:
			case pass_declaration_node::KEEP:
				return D3DSTENCILOP_KEEP;
			case pass_declaration_node::ZERO:
				return D3DSTENCILOP_ZERO;
			case pass_declaration_node::REPLACE:
				return D3DSTENCILOP_REPLACE;
			case pass_declaration_node::INCRSAT:
				return D3DSTENCILOP_INCRSAT;
			case pass_declaration_node::DECRSAT:
				return D3DSTENCILOP_DECRSAT;
			case pass_declaration_node::INVERT:
				return D3DSTENCILOP_INVERT;
			case pass_declaration_node::INCR:
				return D3DSTENCILOP_INCR;
			case pass_declaration_node::DECR:
				return D3DSTENCILOP_DECR;
		}
	}
	static D3DFORMAT literal_to_format(texture_format value)
	{
		switch (value)
		{
			case texture_format::r8:
				return D3DFMT_A8R8G8B8;
			case texture_format::r16f:
				return D3DFMT_R16F;
			case texture_format::r32f:
				return D3DFMT_R32F;
			case texture_format::rg8:
				return D3DFMT_A8R8G8B8;
			case texture_format::rg16:
				return D3DFMT_G16R16;
			case texture_format::rg16f:
				return D3DFMT_G16R16F;
			case texture_format::rg32f:
				return D3DFMT_G32R32F;
			case texture_format::rgba8:
				return D3DFMT_A8R8G8B8;
			case texture_format::rgba16:
				return D3DFMT_A16B16G16R16;
			case texture_format::rgba16f:
				return D3DFMT_A16B16G16R16F;
			case texture_format::rgba32f:
				return D3DFMT_A32B32G32R32F;
			case texture_format::dxt1:
				return D3DFMT_DXT1;
			case texture_format::dxt3:
				return D3DFMT_DXT3;
			case texture_format::dxt5:
				return D3DFMT_DXT5;
			case texture_format::latc1:
				return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
			case texture_format::latc2:
				return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
		}

		return D3DFMT_UNKNOWN;
	}
	static std::string convert_semantic(const std::string &semantic)
	{
		if (semantic.compare(0, 3, "SV_") == 0)
		{
			if (semantic == "SV_VERTEXID")
			{
				return "TEXCOORD0";
			}
			else if (semantic == "SV_POSITION")
			{
				return "POSITION";
			}
			else if (semantic.compare(0, 9, "SV_TARGET") == 0)
			{
				return "COLOR" + semantic.substr(9);
			}
			else if (semantic == "SV_DEPTH")
			{
				return "DEPTH";
			}
		}
		else if (semantic == "VERTEXID")
		{
			return "TEXCOORD0";
		}

		return semantic;
	}

	d3d9_effect_compiler::d3d9_effect_compiler(d3d9_runtime *runtime, const syntax_tree &ast, std::string &errors, bool skipoptimization) :
		_runtime(runtime),
		_ast(ast),
		_errors(errors),
		_skip_shader_optimization(skipoptimization),
		_current_function(nullptr)
	{
#if RESHADE_DUMP_NATIVE_SHADERS
		if (_ast.techniques.size() == 0)
			return;
		_dump_filename = _ast.techniques[0]->location.source;
		_dump_filename = "ReShade-ShaderDump-" + _dump_filename.filename_without_extension().string() + ".hlsl";

		std::ofstream(_dump_filename.string(), std::ios::trunc);
#endif
	}

	bool d3d9_effect_compiler::run()
	{
		_d3dcompiler_module = LoadLibraryW(L"d3dcompiler_47.dll");

		if (_d3dcompiler_module == nullptr)
		{
			_d3dcompiler_module = LoadLibraryW(L"d3dcompiler_43.dll");
		}
		if (_d3dcompiler_module == nullptr)
		{
			_errors += "Unable to load D3DCompiler library. Make sure you have the DirectX end-user runtime (June 2010) installed or a newer version of the library in the application directory.\n";
			return false;
		}

		_uniform_storage_offset = _runtime->get_uniform_value_storage().size();

		for (auto node : _ast.structs)
		{
			visit(_global_code, node);
		}

		for (auto uniform : _ast.variables)
		{
			if (uniform->type.is_texture())
			{
				visit_texture(uniform);
			}
			else if (uniform->type.is_sampler())
			{
				visit_sampler(uniform);
			}
			else if (uniform->type.has_qualifier(type_node::qualifier_uniform))
			{
				visit_uniform(uniform);
			}
			else
			{
				visit(_global_code, uniform);

				_global_code << ";\n";
			}
		}

		for (auto function : _ast.functions)
		{
			std::stringstream function_code;

			_functions[_current_function = function];

			visit(function_code, function);

			_functions[function].code = function_code.str();
		}

		for (auto technique : _ast.techniques)
		{
			visit_technique(technique);
		}

		FreeLibrary(_d3dcompiler_module);

		return _success;
	}

	void d3d9_effect_compiler::error(const location &location, const std::string &message)
	{
		_success = false;

		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
	}
	void d3d9_effect_compiler::warning(const location &location, const std::string &message)
	{
		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
	}

	void d3d9_effect_compiler::visit(std::stringstream &output, const statement_node *node)
	{
		if (node == nullptr)
		{
			return;
		}

		switch (node->id)
		{
			case nodeid::compound_statement:
				visit(output, static_cast<const compound_statement_node *>(node));
				break;
			case nodeid::declarator_list:
				visit(output, static_cast<const declarator_list_node *>(node), false);
				break;
			case nodeid::expression_statement:
				visit(output, static_cast<const expression_statement_node *>(node));
				break;
			case nodeid::if_statement:
				visit(output, static_cast<const if_statement_node *>(node));
				break;
			case nodeid::switch_statement:
				visit(output, static_cast<const switch_statement_node *>(node));
				break;
			case nodeid::for_statement:
				visit(output, static_cast<const for_statement_node *>(node));
				break;
			case nodeid::while_statement:
				visit(output, static_cast<const while_statement_node *>(node));
				break;
			case nodeid::return_statement:
				visit(output, static_cast<const return_statement_node *>(node));
				break;
			case nodeid::jump_statement:
				visit(output, static_cast<const jump_statement_node *>(node));
				break;
			default:
				assert(false);
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const expression_node *node)
	{
		assert(node != nullptr);

		switch (node->id)
		{
			case nodeid::lvalue_expression:
				visit(output, static_cast<const lvalue_expression_node *>(node));
				break;
			case nodeid::literal_expression:
				visit(output, static_cast<const literal_expression_node *>(node));
				break;
			case nodeid::expression_sequence:
				visit(output, static_cast<const expression_sequence_node *>(node));
				break;
			case nodeid::unary_expression:
				visit(output, static_cast<const unary_expression_node *>(node));
				break;
			case nodeid::binary_expression:
				visit(output, static_cast<const binary_expression_node *>(node));
				break;
			case nodeid::intrinsic_expression:
				visit(output, static_cast<const intrinsic_expression_node *>(node));
				break;
			case nodeid::conditional_expression:
				visit(output, static_cast<const conditional_expression_node *>(node));
				break;
			case nodeid::swizzle_expression:
				visit(output, static_cast<const swizzle_expression_node *>(node));
				break;
			case nodeid::field_expression:
				visit(output, static_cast<const field_expression_node *>(node));
				break;
			case nodeid::initializer_list:
				visit(output, static_cast<const initializer_list_node *>(node));
				break;
			case nodeid::assignment_expression:
				visit(output, static_cast<const assignment_expression_node *>(node));
				break;
			case nodeid::call_expression:
				visit(output, static_cast<const call_expression_node *>(node));
				break;
			case nodeid::constructor_expression:
				visit(output, static_cast<const constructor_expression_node *>(node));
				break;
			default:
				assert(false);
		}
	}

	void d3d9_effect_compiler::visit(std::stringstream &output, const type_node &type, bool with_qualifiers)
	{
		if (with_qualifiers)
		{
			if (type.has_qualifier(type_node::qualifier_static))
				output << "static ";
			if (type.has_qualifier(type_node::qualifier_const))
				output << "const ";
			if (type.has_qualifier(type_node::qualifier_volatile))
				output << "volatile ";
			if (type.has_qualifier(type_node::qualifier_precise))
				output << "precise ";
			if (type.has_qualifier(type_node::qualifier_linear))
				output << "linear ";
			if (type.has_qualifier(type_node::qualifier_noperspective))
				output << "noperspective ";
			if (type.has_qualifier(type_node::qualifier_centroid))
				output << "centroid ";
			if (type.has_qualifier(type_node::qualifier_nointerpolation))
				output << "nointerpolation ";
			if (type.has_qualifier(type_node::qualifier_inout))
				output << "inout ";
			else if (type.has_qualifier(type_node::qualifier_in))
				output << "in ";
			else if (type.has_qualifier(type_node::qualifier_out))
				output << "out ";
			else if (type.has_qualifier(type_node::qualifier_uniform))
				output << "uniform ";
		}

		switch (type.basetype)
		{
			case type_node::datatype_void:
				output << "void";
				break;
			case type_node::datatype_bool:
				output << "bool";
				break;
			case type_node::datatype_int:
				output << "int";
				break;
			case type_node::datatype_uint:
				output << "uint";
				break;
			case type_node::datatype_float:
				output << "float";
				break;
			case type_node::datatype_sampler:
				output << "__sampler2D";
				break;
			case type_node::datatype_struct:
				output << type.definition->unique_name;
				break;
		}

		if (type.is_matrix())
		{
			output << type.rows << 'x' << type.cols;
		}
		else if (type.is_vector())
		{
			output << type.rows;
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const lvalue_expression_node *node)
	{
		output << node->reference->unique_name;

		if (node->reference->type.is_sampler() && _samplers.find(node->reference->name) != _samplers.end())
		{
			_functions.at(_current_function).sampler_dependencies.insert(node->reference);
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const literal_expression_node *node)
	{
		if (!node->type.is_scalar())
		{
			visit(output, node->type, false);

			output << '(';
		}

		for (size_t i = 0, count = node->type.rows * node->type.cols; i < count; i++)
		{
			switch (node->type.basetype)
			{
				case type_node::datatype_bool:
					output << (node->value_int[i] ? "true" : "false");
					break;
				case type_node::datatype_int:
					output << node->value_int[i];
					break;
				case type_node::datatype_uint:
					output << node->value_uint[i];
					break;
				case type_node::datatype_float:
					output << std::setprecision(8) << std::fixed << node->value_float[i];
					break;
			}

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		if (!node->type.is_scalar())
		{
			output << ')';
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const expression_sequence_node *node)
	{
		output << '(';

		for (size_t i = 0, count = node->expression_list.size(); i < count; i++)
		{
			visit(output, node->expression_list[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ')';
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const unary_expression_node *node)
	{
		switch (node->op)
		{
			case unary_expression_node::negate:
				output << '-';
				break;
			case unary_expression_node::bitwise_not:
				output << "(4294967295 - ";
				break;
			case unary_expression_node::logical_not:
				output << '!';
				break;
			case unary_expression_node::pre_increase:
				output << "++";
				break;
			case unary_expression_node::pre_decrease:
				output << "--";
				break;
			case unary_expression_node::cast:
				visit(output, node->type, false);
				output << '(';
				break;
		}

		visit(output, node->operand);

		switch (node->op)
		{
			case unary_expression_node::cast:
			case unary_expression_node::bitwise_not:
				output << ')';
				break;
			case unary_expression_node::post_increase:
				output << "++";
				break;
			case unary_expression_node::post_decrease:
				output << "--";
				break;
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const binary_expression_node *node)
	{
		std::string part1, part2, part3;

		switch (node->op)
		{
			case binary_expression_node::add:
				part1 = '(';
				part2 = " + ";
				part3 = ')';
				break;
			case binary_expression_node::subtract:
				part1 = '(';
				part2 = " - ";
				part3 = ')';
				break;
			case binary_expression_node::multiply:
				part1 = '(';
				part2 = " * ";
				part3 = ')';
				break;
			case binary_expression_node::divide:
				part1 = '(';
				part2 = " / ";
				part3 = ')';
				break;
			case binary_expression_node::modulo:
				part1 = '(';
				part2 = " % ";
				part3 = ')';
				break;
			case binary_expression_node::less:
				part1 = '(';
				part2 = " < ";
				part3 = ')';
				break;
			case binary_expression_node::greater:
				part1 = '(';
				part2 = " > ";
				part3 = ')';
				break;
			case binary_expression_node::less_equal:
				part1 = '(';
				part2 = " <= ";
				part3 = ')';
				break;
			case binary_expression_node::greater_equal:
				part1 = '(';
				part2 = " >= ";
				part3 = ')';
				break;
			case binary_expression_node::equal:
				part1 = '(';
				part2 = " == ";
				part3 = ')';
				break;
			case binary_expression_node::not_equal:
				part1 = '(';
				part2 = " != ";
				part3 = ')';
				break;
			case binary_expression_node::left_shift:
				part1 = "((";
				part2 = ") * exp2(";
				part3 = "))";
				break;
			case binary_expression_node::right_shift:
				part1 = "floor((";
				part2 = ") / exp2(";
				part3 = "))";
				break;
			case binary_expression_node::bitwise_and:
				if (node->operands[1]->id == nodeid::literal_expression && node->operands[1]->type.is_integral() && node->operands[1]->type.is_scalar())
				{
					const unsigned int value = static_cast<const literal_expression_node *>(node->operands[1])->value_uint[0];

					if (is_pow2(value + 1))
					{
						output << "(("  << (value + 1) << ") * frac((";
						visit(output, node->operands[0]);
						output << ") / (" << (value + 1) << ".0)))";
						return;
					}
					else if (is_pow2(value))
					{
						output << "((((";
						visit(output, node->operands[0]);
						output << ") / (" << value << ")) % 2) * " << value << ")";
						return;
					}
				}
			case binary_expression_node::bitwise_or:
			case binary_expression_node::bitwise_xor:
				error(node->location, "bitwise operations are not supported in Direct3D9");
				return;
			case binary_expression_node::logical_and:
				part1 = '(';
				part2 = " && ";
				part3 = ')';
				break;
			case binary_expression_node::logical_or:
				part1 = '(';
				part2 = " || ";
				part3 = ')';
				break;
			case binary_expression_node::element_extract:
				part2 = '[';
				part3 = ']';
				break;
		}

		output << part1;
		visit(output, node->operands[0]);
		output << part2;
		visit(output, node->operands[1]);
		output << part3;
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const intrinsic_expression_node *node)
	{
		std::string part1, part2, part3, part4, part5;

		switch (node->op)
		{
			case intrinsic_expression_node::abs:
				part1 = "abs(";
				part2 = ")";
				break;
			case intrinsic_expression_node::acos:
				part1 = "acos(";
				part2 = ")";
				break;
			case intrinsic_expression_node::all:
				part1 = "all(";
				part2 = ")";
				break;
			case intrinsic_expression_node::any:
				part1 = "any(";
				part2 = ")";
				break;
			case intrinsic_expression_node::bitcast_int2float:
			case intrinsic_expression_node::bitcast_uint2float:
			case intrinsic_expression_node::bitcast_float2int:
			case intrinsic_expression_node::bitcast_float2uint:
				error(node->location, "'asint', 'asuint' and 'asfloat' are not supported in Direct3D9");
				return;
			case intrinsic_expression_node::asin:
				part1 = "asin(";
				part2 = ")";
				break;
			case intrinsic_expression_node::atan:
				part1 = "atan(";
				part2 = ")";
				break;
			case intrinsic_expression_node::atan2:
				part1 = "atan2(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::ceil:
				part1 = "ceil(";
				part2 = ")";
				break;
			case intrinsic_expression_node::clamp:
				part1 = "clamp(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::cos:
				part1 = "cos(";
				part2 = ")";
				break;
			case intrinsic_expression_node::cosh:
				part1 = "cosh(";
				part2 = ")";
				break;
			case intrinsic_expression_node::cross:
				part1 = "cross(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::ddx:
				part1 = "ddx(";
				part2 = ")";
				break;
			case intrinsic_expression_node::ddy:
				part1 = "ddy(";
				part2 = ")";
				break;
			case intrinsic_expression_node::degrees:
				part1 = "degrees(";
				part2 = ")";
				break;
			case intrinsic_expression_node::determinant:
				part1 = "determinant(";
				part2 = ")";
				break;
			case intrinsic_expression_node::distance:
				part1 = "distance(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::dot:
				part1 = "dot(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::exp:
				part1 = "exp(";
				part2 = ")";
				break;
			case intrinsic_expression_node::exp2:
				part1 = "exp2(";
				part2 = ")";
				break;
			case intrinsic_expression_node::faceforward:
				part1 = "faceforward(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::floor:
				part1 = "floor(";
				part2 = ")";
				break;
			case intrinsic_expression_node::frac:
				part1 = "frac(";
				part2 = ")";
				break;
			case intrinsic_expression_node::frexp:
				part1 = "frexp(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::fwidth:
				part1 = "fwidth(";
				part2 = ")";
				break;
			case intrinsic_expression_node::isinf:
				part1 = "isinf(";
				part2 = ")";
				break;
			case intrinsic_expression_node::isnan:
				part1 = "isnan(";
				part2 = ")";
				break;
			case intrinsic_expression_node::ldexp:
				part1 = "ldexp(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::length:
				part1 = "length(";
				part2 = ")";
				break;
			case intrinsic_expression_node::lerp:
				part1 = "lerp(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::log:
				part1 = "log(";
				part2 = ")";
				break;
			case intrinsic_expression_node::log10:
				part1 = "log10(";
				part2 = ")";
				break;
			case intrinsic_expression_node::log2:
				part1 = "log2(";
				part2 = ")";
				break;
			case intrinsic_expression_node::mad:
				part1 = "((";
				part2 = ") * (";
				part3 = ") + (";
				part4 = "))";
				break;
			case intrinsic_expression_node::max:
				part1 = "max(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::min:
				part1 = "min(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::modf:
				part1 = "modf(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::mul:
				part1 = "mul(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::normalize:
				part1 = "normalize(";
				part2 = ")";
				break;
			case intrinsic_expression_node::pow:
				part1 = "pow(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::radians:
				part1 = "radians(";
				part2 = ")";
				break;
			case intrinsic_expression_node::rcp:
				part1 = "(1.0f / ";
				part2 = ")";
				break;
			case intrinsic_expression_node::reflect:
				part1 = "reflect(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::refract:
				part1 = "refract(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::round:
				part1 = "round(";
				part2 = ")";
				break;
			case intrinsic_expression_node::rsqrt:
				part1 = "rsqrt(";
				part2 = ")";
				break;
			case intrinsic_expression_node::saturate:
				part1 = "saturate(";
				part2 = ")";
				break;
			case intrinsic_expression_node::sign:
				part1 = "sign(";
				part2 = ")";
				break;
			case intrinsic_expression_node::sin:
				part1 = "sin(";
				part2 = ")";
				break;
			case intrinsic_expression_node::sincos:
				part1 = "sincos(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::sinh:
				part1 = "sinh(";
				part2 = ")";
				break;
			case intrinsic_expression_node::smoothstep:
				part1 = "smoothstep(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::sqrt:
				part1 = "sqrt(";
				part2 = ")";
				break;
			case intrinsic_expression_node::step:
				part1 = "step(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::tan:
				part1 = "tan(";
				part2 = ")";
				break;
			case intrinsic_expression_node::tanh:
				part1 = "tanh(";
				part2 = ")";
				break;
			case intrinsic_expression_node::texture:
				part1 = "tex2D((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case intrinsic_expression_node::texture_fetch:
				part1 = "__tex2Dfetch(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::texture_gather:
				if (node->arguments[2]->id == nodeid::literal_expression && node->arguments[2]->type.is_integral())
				{
					const int component = static_cast<const literal_expression_node *>(node->arguments[2])->value_int[0];

					output << "__tex2Dgather" << component << '(';
					visit(output, node->arguments[0]);
					output << ", ";
					visit(output, node->arguments[1]);
					output << ')';
				}
				else
				{
					error(node->location, "texture gather component argument has to be constant");
				}
				return;
			case intrinsic_expression_node::texture_gather_offset:
				if (node->arguments[3]->id == nodeid::literal_expression && node->arguments[3]->type.is_integral())
				{
					const int component = static_cast<const literal_expression_node *>(node->arguments[3])->value_int[0];

					output << "__tex2Dgather" << component << "offset(";
					visit(output, node->arguments[0]);
					output << ", ";
					visit(output, node->arguments[1]);
					output << ", ";
					visit(output, node->arguments[2]);
					output << ')';
				}
				else
				{
					error(node->location, "texture gather component argument has to be constant");
				}
				return;
			case intrinsic_expression_node::texture_gradient:
				part1 = "tex2Dgrad((";
				part2 = ").s, ";
				part3 = ", ";
				part4 = ", ";
				part5 = ")";
				break;
			case intrinsic_expression_node::texture_level:
				part1 = "tex2Dlod((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case intrinsic_expression_node::texture_level_offset:
				part1 = "__tex2Dlodoffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::texture_offset:
				part1 = "__tex2Doffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case intrinsic_expression_node::texture_projection:
				part1 = "tex2Dproj((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case intrinsic_expression_node::texture_size:
				part1 = "__tex2Dsize(";
				part2 = ", ";
				part3 = ")";
				break;
			case intrinsic_expression_node::transpose:
				part1 = "transpose(";
				part2 = ")";
				break;
			case intrinsic_expression_node::trunc:
				part1 = "trunc(";
				part2 = ")";
				break;
		}

		output << part1;

		if (node->arguments[0] != nullptr)
		{
			visit(output, node->arguments[0]);
		}

		output << part2;

		if (node->arguments[1] != nullptr)
		{
			visit(output, node->arguments[1]);
		}

		output << part3;

		if (node->arguments[2] != nullptr)
		{
			visit(output, node->arguments[2]);
		}

		output << part4;

		if (node->arguments[3] != nullptr)
		{
			visit(output, node->arguments[3]);
		}

		output << part5;
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const conditional_expression_node *node)
	{
		output << '(';
		visit(output, node->condition);
		output << " ? ";
		visit(output, node->expression_when_true);
		output << " : ";
		visit(output, node->expression_when_false);
		output << ')';
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const swizzle_expression_node *node)
	{
		visit(output, node->operand);

		output << '.';

		if (node->operand->type.is_matrix())
		{
			const char swizzle[16][5] = {
				"_m00", "_m01", "_m02", "_m03",
				"_m10", "_m11", "_m12", "_m13",
				"_m20", "_m21", "_m22", "_m23",
				"_m30", "_m31", "_m32", "_m33"
			};

			for (unsigned int i = 0; i < 4 && node->mask[i] >= 0; i++)
			{
				output << swizzle[node->mask[i]];
			}
		}
		else
		{
			const char swizzle[4] = {
				'x', 'y', 'z', 'w'
			};

			for (unsigned int i = 0; i < 4 && node->mask[i] >= 0; i++)
			{
				output << swizzle[node->mask[i]];
			}
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const field_expression_node *node)
	{
		output << '(';
		visit(output, node->operand);
		output << '.' << node->field_reference->unique_name << ')';
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const assignment_expression_node *node)
	{
		std::string part1, part2, part3;

		switch (node->op)
		{
			case assignment_expression_node::none:
				part2 = " = ";
				break;
			case assignment_expression_node::add:
				part2 = " += ";
				break;
			case assignment_expression_node::subtract:
				part2 = " -= ";
				break;
			case assignment_expression_node::multiply:
				part2 = " *= ";
				break;
			case assignment_expression_node::divide:
				part2 = " /= ";
				break;
			case assignment_expression_node::modulo:
				part2 = " %= ";
				break;
			case assignment_expression_node::left_shift:
				part1 = "((";
				part2 = ") *= pow(2, ";
				part3 = "))";
				break;
			case assignment_expression_node::right_shift:
				part1 = "((";
				part2 = ") /= pow(2, ";
				part3 = "))";
				break;
			case assignment_expression_node::bitwise_and:
			case assignment_expression_node::bitwise_or:
			case assignment_expression_node::bitwise_xor:
				error(node->location, "bitwise operations are not supported in Direct3D9");
				return;
		}

		output << '(' << part1;
		visit(output, node->left);
		output << part2;
		visit(output, node->right);
		output << part3 << ')';
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const call_expression_node *node)
	{
		output << node->callee->unique_name << '(';

		for (size_t i = 0, count = node->arguments.size(); i < count; i++)
		{
			visit(output, node->arguments[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ')';

		auto &info = _functions.at(_current_function);
		auto &info_callee = _functions.at(node->callee);

		info.sampler_dependencies.insert(info_callee.sampler_dependencies.begin(), info_callee.sampler_dependencies.end());

		for (auto dependency : info_callee.dependencies)
		{
			if (std::find(info.dependencies.begin(), info.dependencies.end(), dependency) == info.dependencies.end())
			{
				info.dependencies.push_back(dependency);
			}
		}

		if (std::find(info.dependencies.begin(), info.dependencies.end(), node->callee) == info.dependencies.end())
		{
			info.dependencies.push_back(node->callee);
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const constructor_expression_node *node)
	{
		visit(output, node->type, false);
		output << '(';

		for (size_t i = 0, count = node->arguments.size(); i < count; i++)
		{
			visit(output, node->arguments[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ')';
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const initializer_list_node *node)
	{
		output << "{ ";

		for (size_t i = 0, count = node->values.size(); i < count; i++)
		{
			visit(output, node->values[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << " }";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const compound_statement_node *node)
	{
		output << "{\n";

		for (auto statement : node->statement_list)
		{
			visit(output, statement);
		}

		output << "}\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const declarator_list_node *node, bool single_statement)
	{
		bool with_type = true;

		for (size_t i = 0, count = node->declarator_list.size(); i < count; i++)
		{
			visit(output, node->declarator_list[i], with_type);

			if (i < count - 1)
			{
				output << (single_statement ? ", " : ";\n");
			}
			if (single_statement)
			{
				with_type = false;
			}
		}

		output << ";\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const expression_statement_node *node)
	{
		visit(output, node->expression);

		output << ";\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const if_statement_node *node)
	{
		for (const auto &attribute : node->attributes)
		{
			output << '[' << attribute << ']';
		}

		output << "if (";

		visit(output, node->condition);

		output << ")\n";

		if (node->statement_when_true != nullptr)
		{
			visit(output, node->statement_when_true);
		}
		else
		{
			output << "\t;";
		}

		if (node->statement_when_false != nullptr)
		{
			output << "else\n";

			visit(output, node->statement_when_false);
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const switch_statement_node *node)
	{
		warning(node->location, "switch statements do not currently support fall-through in Direct3D9!");

		output << "[unroll] do { ";

		visit(output, node->test_expression->type, false);

		output << " __switch_condition = ";

		visit(output, node->test_expression);

		output << ";\n";

		visit(output, node->case_list[0]);

		for (size_t i = 1, count = node->case_list.size(); i < count; i++)
		{
			output << "else ";

			visit(output, node->case_list[i]);
		}

		output << "} while (false);\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const case_statement_node *node)
	{
		output << "if (";

		for (size_t i = 0, count = node->labels.size(); i < count; i++)
		{
			if (node->labels[i] == nullptr)
			{
				output << "true";
			}
			else
			{
				output << "__switch_condition == ";
				visit(output, node->labels[i]);
			}

			if (i < count - 1)
			{
				output << " || ";
			}
		}

		output << ")";

		visit(output, node->statement_list);
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const for_statement_node *node)
	{
		for (const auto &attribute : node->attributes)
		{
			output << '[' << attribute << ']';
		}

		output << "for (";

		if (node->init_statement != nullptr)
		{
			if (node->init_statement->id == nodeid::declarator_list)
			{
				visit(output, static_cast<declarator_list_node *>(node->init_statement), true);

				output.seekp(-2, std::ios_base::end);
			}
			else
			{
				visit(output, static_cast<expression_statement_node *>(node->init_statement)->expression);
			}
		}

		output << "; ";

		if (node->condition != nullptr)
		{
			visit(output, node->condition);
		}

		output << "; ";

		if (node->increment_expression != nullptr)
		{
			visit(output, node->increment_expression);
		}

		output << ")\n";

		if (node->statement_list != nullptr)
		{
			visit(output, node->statement_list);
		}
		else
		{
			output << "\t;";
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const while_statement_node *node)
	{
		for (const auto &attribute : node->attributes)
		{
			output << '[' << attribute << ']';
		}

		if (node->is_do_while)
		{
			output << "do\n{\n";

			if (node->statement_list != nullptr)
			{
				visit(output, node->statement_list);
			}

			output << "}\nwhile (";

			visit(output, node->condition);

			output << ");\n";
		}
		else
		{
			output << "while (";

			visit(output, node->condition);

			output << ")\n";

			if (node->statement_list != nullptr)
			{
				visit(output, node->statement_list);
			}
			else
			{
				output << "\t;";
			}
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const return_statement_node *node)
	{
		if (node->is_discard)
		{
			output << "discard";
		}
		else
		{
			output << "return";

			if (node->return_value != nullptr)
			{
				output << ' ';

				visit(output, node->return_value);
			}
		}

		output << ";\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const jump_statement_node *node)
	{
		if (node->is_break)
		{
			output << "break;\n";
		}
		else if (node->is_continue)
		{
			output << "continue;\n";
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const struct_declaration_node *node)
	{
		output << "struct " << node->unique_name << "\n{\n";

		if (!node->field_list.empty())
		{
			for (auto field : node->field_list)
			{
				visit(output, field);

				output << ";\n";
			}
		}
		else
		{
			output << "float _dummy;\n";
		}

		output << "};\n";
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const variable_declaration_node *node, bool with_type, bool with_semantic)
	{
		if (with_type)
		{
			visit(output, node->type);
			output << ' ';
		}

		output << node->unique_name;

		if (node->type.is_array())
		{
			output << '[';

			if (node->type.array_length > 0)
			{
				output << node->type.array_length;
			}

			output << ']';
		}

		if (with_semantic && !node->semantic.empty())
		{
			output << " : " << convert_semantic(node->semantic);
		}

		if (node->initializer_expression != nullptr)
		{
			output << " = ";

			visit(output, node->initializer_expression);
		}
	}
	void d3d9_effect_compiler::visit(std::stringstream &output, const function_declaration_node *node)
	{
		visit(output, node->return_type, false);

		output << ' ' << node->unique_name << '(';

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			visit(output, node->parameter_list[i], true, false);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ")\n";

		visit(output, node->definition);
	}

	void d3d9_effect_compiler::visit_texture(const variable_declaration_node *node)
	{
		const auto existing_texture = _runtime->find_texture(node->unique_name);

		if (existing_texture != nullptr)
		{
			if (node->semantic.empty() && (
				existing_texture->width != node->properties.width ||
				existing_texture->height != node->properties.height ||
				existing_texture->levels != node->properties.levels ||
				existing_texture->format != node->properties.format))
			{
				error(node->location, existing_texture->effect_filename + " already created a texture with the same name but different dimensions; textures are shared across all effects, so either rename the variable or adjust the dimensions so they match");
			}
			return;
		}

		texture obj;
		obj.impl = std::make_unique<d3d9_tex_data>();
		const auto obj_data = obj.impl->as<d3d9_tex_data>();
		obj.name = node->name;
		obj.unique_name = node->unique_name;
		obj.annotations = node->annotation_list;
		UINT width = obj.width = node->properties.width;
		UINT height = obj.height = node->properties.height;
		UINT levels = obj.levels = node->properties.levels;
		const D3DFORMAT format = literal_to_format(obj.format = node->properties.format);

		if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
		{
			_runtime->update_texture_reference(obj, texture_reference::back_buffer);
		}
		else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
		{
			_runtime->update_texture_reference(obj, texture_reference::depth_buffer);
		}
		else if (!node->semantic.empty())
		{
			error(node->location, "invalid semantic");
			return;
		}
		else
		{
			DWORD usage = 0;
			D3DDEVICE_CREATION_PARAMETERS cp;
			_runtime->_device->GetCreationParameters(&cp);

			if (levels > 1)
			{
				if (_runtime->_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, format) == D3D_OK)
				{
					usage |= D3DUSAGE_AUTOGENMIPMAP;
					levels = 0;
				}
				else
				{
					warning(node->location, "autogenerated miplevels are not supported for this format");
				}
			}

			HRESULT hr = _runtime->_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format);

			if (SUCCEEDED(hr))
			{
				usage |= D3DUSAGE_RENDERTARGET;
			}

			hr = _runtime->_device->CreateTexture(width, height, levels, usage, format, D3DPOOL_DEFAULT, &obj_data->texture, nullptr);

			if (FAILED(hr))
			{
				error(node->location, "internal texture creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			hr = obj_data->texture->GetSurfaceLevel(0, &obj_data->surface);

			assert(SUCCEEDED(hr));
		}

		_runtime->add_texture(std::move(obj));
	}
	void d3d9_effect_compiler::visit_sampler(const variable_declaration_node *node)
	{
		const auto texture = _runtime->find_texture(node->properties.texture->unique_name);

		if (texture == nullptr)
		{
			error(node->location, "texture not found");
			return;
		}

		d3d9_sampler sampler;
		sampler.texture = texture->impl->as<d3d9_tex_data>();
		sampler.states[D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_u);
		sampler.states[D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_v);
		sampler.states[D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_w);
		sampler.states[D3DSAMP_BORDERCOLOR] = 0;
		sampler.states[D3DSAMP_MAGFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0x0C) >> 2);
		sampler.states[D3DSAMP_MINFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0x30) >> 4);
		sampler.states[D3DSAMP_MIPFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0x03));
		sampler.states[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&node->properties.lod_bias);
		sampler.states[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, node->properties.min_lod));
		sampler.states[D3DSAMP_MAXANISOTROPY] = 1;
		sampler.states[D3DSAMP_SRGBTEXTURE] = node->properties.srgb_texture;

		_samplers[node->name] = sampler;
	}
	void d3d9_effect_compiler::visit_uniform(const variable_declaration_node *node)
	{
		auto type = node->type;
		type.basetype = type_node::datatype_float;
		visit(_global_code, type);

		_global_code << ' ' << node->unique_name;

		if (node->type.is_array())
		{
			_global_code << '[';

			if (node->type.array_length > 0)
			{
				_global_code << node->type.array_length;
			}

			_global_code << ']';
		}

		_global_code << " : register(c" << _constant_register_count << ");\n";

		uniform obj;
		obj.name = node->name;
		obj.unique_name = node->unique_name;
		obj.basetype = uniform_datatype::floating_point;
		obj.displaytype = static_cast<uniform_datatype>(node->type.basetype - 1);
		obj.rows = node->type.rows;
		obj.columns = node->type.cols;
		obj.elements = node->type.array_length;
		obj.storage_size = obj.rows * obj.columns * std::max(1u, obj.elements) * 4;
		obj.annotations = node->annotation_list;

		obj.storage_offset = _uniform_storage_offset + _constant_register_count * 16;
		_constant_register_count += (obj.storage_size / 4 + 4 - ((obj.storage_size / 4) % 4)) / 4;

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (_uniform_storage_offset + _constant_register_count * 16 >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 4096);
		}

		if (node->initializer_expression != nullptr && node->initializer_expression->id == nodeid::literal_expression)
		{
			for (size_t i = 0; i < obj.storage_size / 4; i++)
			{
				scalar_literal_cast(static_cast<const literal_expression_node *>(node->initializer_expression), i, reinterpret_cast<float *>(uniform_storage.data() + obj.storage_offset)[i]);
			}
		}
		else
		{
			ZeroMemory(uniform_storage.data() + obj.storage_offset, obj.storage_size);
		}

		_runtime->add_uniform(std::move(obj));
	}
	void d3d9_effect_compiler::visit_technique(const technique_declaration_node *node)
	{
		technique obj;
		obj.name = node->name;
		obj.annotations = node->annotation_list;

		if (_constant_register_count != 0)
		{
			obj.uniform_storage_index = _constant_register_count;
			obj.uniform_storage_offset = _uniform_storage_offset;
		}

		for (auto pass : node->pass_list)
		{
			obj.passes.emplace_back(std::make_unique<d3d9_pass_data>());
			visit_pass(pass, *static_cast<d3d9_pass_data *>(obj.passes.back().get()));
		}

		_runtime->add_technique(std::move(obj));
	}
	void d3d9_effect_compiler::visit_pass(const pass_declaration_node *node, d3d9_pass_data & pass)
	{
		pass.render_targets[0] = _runtime->_backbuffer_resolved.get();
		pass.clear_render_targets = node->clear_render_targets;

		std::string samplers[2];
		const char shader_types[2][3] = { "vs", "ps" };
		const function_declaration_node *shader_functions[2] = { node->vertex_shader, node->pixel_shader };

		for (unsigned int i = 0; i < 2; i++)
		{
			if (shader_functions[i] == nullptr)
			{
				continue;
			}

			for (auto sampler : _functions.at(shader_functions[i]).sampler_dependencies)
			{
				pass.samplers[pass.sampler_count] = _samplers.at(sampler->name);
				const auto *const texture = sampler->properties.texture;

				samplers[i] += "sampler2D __Sampler";
				samplers[i] += sampler->unique_name;
				samplers[i] += " : register(s" + std::to_string(pass.sampler_count++) + ");\n";
				samplers[i] += "static const __sampler2D ";
				samplers[i] += sampler->unique_name;
				samplers[i] += " = { __Sampler";
				samplers[i] += sampler->unique_name;

				if (texture->semantic == "COLOR" || texture->semantic == "SV_TARGET" || texture->semantic == "DEPTH" || texture->semantic == "SV_DEPTH")
				{
					samplers[i] += ", float2(" + std::to_string(1.0f / _runtime->frame_width()) + ", " + std::to_string(1.0f / _runtime->frame_height()) + ")";
				}
				else
				{
					samplers[i] += ", float2(" + std::to_string(1.0f / texture->properties.width) + ", " + std::to_string(1.0f / texture->properties.height) + ")";
				}

				samplers[i] += " };\n";

				if (pass.sampler_count == 16)
				{
					error(node->location, "maximum sampler count of 16 reached in pass '" + node->name + "'");
					return;
				}
			}
		}

		for (unsigned int i = 0; i < 2; ++i)
		{
			if (shader_functions[i] != nullptr)
			{
				visit_pass_shader(shader_functions[i], shader_types[i], samplers[i], pass);
			}
		}

		const auto &device = _runtime->_device;
		const HRESULT hr = device->BeginStateBlock();

		if (FAILED(hr))
		{
			error(node->location, "internal pass stateblock creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			return;
		}

		device->SetVertexShader(pass.vertex_shader.get());
		device->SetPixelShader(pass.pixel_shader.get());

		device->SetRenderState(D3DRS_ZENABLE, false);
		device->SetRenderState(D3DRS_SPECULARENABLE, false);
		device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		device->SetRenderState(D3DRS_ZWRITEENABLE, true);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, false);
		device->SetRenderState(D3DRS_LASTPIXEL, true);
		device->SetRenderState(D3DRS_SRCBLEND, literal_to_blend_func(node->src_blend));
		device->SetRenderState(D3DRS_DESTBLEND, literal_to_blend_func(node->dest_blend));
		device->SetRenderState(D3DRS_ALPHAREF, 0);
		device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
		device->SetRenderState(D3DRS_DITHERENABLE, false);
		device->SetRenderState(D3DRS_FOGSTART, 0);
		device->SetRenderState(D3DRS_FOGEND, 1);
		device->SetRenderState(D3DRS_FOGDENSITY, 1);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, node->blend_enable);
		device->SetRenderState(D3DRS_DEPTHBIAS, 0);
		device->SetRenderState(D3DRS_STENCILENABLE, node->stencil_enable);
		device->SetRenderState(D3DRS_STENCILPASS, literal_to_stencil_op(node->stencil_op_pass));
		device->SetRenderState(D3DRS_STENCILFAIL, literal_to_stencil_op(node->stencil_op_fail));
		device->SetRenderState(D3DRS_STENCILZFAIL, literal_to_stencil_op(node->stencil_op_depth_fail));
		device->SetRenderState(D3DRS_STENCILFUNC, static_cast<D3DCMPFUNC>(node->stencil_comparison_func));
		device->SetRenderState(D3DRS_STENCILREF, node->stencil_reference_value);
		device->SetRenderState(D3DRS_STENCILMASK, node->stencil_read_mask);
		device->SetRenderState(D3DRS_STENCILWRITEMASK, node->stencil_write_mask);
		device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
		device->SetRenderState(D3DRS_LOCALVIEWER, true);
		device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
		device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, node->color_write_mask);
		device->SetRenderState(D3DRS_BLENDOP, static_cast<D3DBLENDOP>(node->blend_op));
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
		device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, false);
		device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, false);
		device->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
		device->SetRenderState(D3DRS_COLORWRITEENABLE1, 0x0000000F);
		device->SetRenderState(D3DRS_COLORWRITEENABLE2, 0x0000000F);
		device->SetRenderState(D3DRS_COLORWRITEENABLE3, 0x0000000F);
		device->SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, node->srgb_write_enable);
		device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
		device->SetRenderState(D3DRS_SRCBLENDALPHA, literal_to_blend_func(node->src_blend_alpha));
		device->SetRenderState(D3DRS_DESTBLENDALPHA, literal_to_blend_func(node->dest_blend_alpha));
		device->SetRenderState(D3DRS_BLENDOPALPHA, static_cast<D3DBLENDOP>(node->blend_op_alpha));
		device->SetRenderState(D3DRS_FOGENABLE, false);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_LIGHTING, false);

		device->EndStateBlock(&pass.stateblock);

		D3DCAPS9 caps;
		device->GetDeviceCaps(&caps);

		for (unsigned int i = 0; i < 8; ++i)
		{
			if (node->render_targets[i] == nullptr)
			{
				continue;
			}

			if (i > caps.NumSimultaneousRTs)
			{
				warning(node->location, "device only supports " + std::to_string(caps.NumSimultaneousRTs) + " simultaneous render targets, but pass '" + node->name + "' uses more, which are ignored");
				break;
			}

			const auto texture = _runtime->find_texture(node->render_targets[i]->unique_name);

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
				return;
			}

			pass.render_targets[i] = texture->impl->as<d3d9_tex_data>()->surface.get();
		}
	}
	void d3d9_effect_compiler::visit_pass_shader(const function_declaration_node *node, const std::string &shadertype, const std::string &samplers, d3d9_pass_data &pass)
	{
		std::stringstream source;

		source <<
			"#pragma warning(disable: 3571)\n"
			"struct __sampler2D { sampler2D s; float2 pixelsize; };\n"
			"float4 __tex2Dfetch(__sampler2D s, int4 c) { return tex2Dlod(s.s, float4((c.xy * exp2(c.w) + 0.5 /* half pixel offset */) * s.pixelsize, 0, c.w)); }\n"
			"float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return tex2Dlod(s.s, c + float4(offset * s.pixelsize, 0, 0)); }\n"
			"float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return tex2D(s.s, c + offset * s.pixelsize); }\n"
			"int2 __tex2Dsize(__sampler2D s, int lod) { return int2(1 / s.pixelsize) / exp2(lod); }\n"
			"float4 __tex2Dgather0(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c, 0, 0)).r); }\n"
			"float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c, 0, 0)).g); }\n"
			"float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c, 0, 0)).b); }\n"
			"float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c, 0, 0)).a); }\n"
			"float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather0(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather1(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather2(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather3(s, c + offset * s.pixelsize); }\n";

		if (shadertype == "vs")
		{
			source << "uniform float2 __TEXEL_SIZE__ : register(c255);\n";
		}
		if (shadertype == "ps")
		{
			source << "#define POSITION VPOS\n";
		}

		source << samplers;
		source << _global_code.str();

		for (auto dependency : _functions.at(node).dependencies)
		{
			source << _functions.at(dependency).code;
		}

		source << _functions.at(node).code;

		std::string position_variable, initialization;
		type_node return_type = node->return_type;

		if (node->return_type.is_struct())
		{
			for (auto field : node->return_type.definition->field_list)
			{
				if (field->semantic == "SV_POSITION" ||
					field->semantic == "POSITION")
				{
					position_variable = "_return." + field->name;
					break;
				}
				else if (
					field->type.rows != 4 && (
					field->semantic.compare(0, 9, "SV_TARGET") == 0 ||
					field->semantic.compare(0, 5, "COLOR") == 0))
				{
					error(node->location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
					return;
				}
			}
		}
		else
		{
			if (node->return_semantic == "SV_POSITION" ||
				node->return_semantic == "POSITION")
			{
				position_variable = "_return";
			}
			else if (
				node->return_semantic.compare(0, 9, "SV_TARGET") == 0 ||
				node->return_semantic.compare(0, 5, "COLOR") == 0)
			{
				return_type.rows = 4;
			}
		}

		visit(source, return_type, false);

		source << " __main(";

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			const auto parameter = node->parameter_list[i];
			type_node parameter_type = parameter->type;

			if (parameter->type.has_qualifier(type_node::qualifier_out))
			{
				if (parameter_type.is_struct())
				{
					for (auto field : parameter_type.definition->field_list)
					{
						if (field->semantic == "SV_POSITION" ||
							field->semantic == "POSITION")
						{
							position_variable = parameter->name + '.' + field->name;
							break;
						}
						else if (
							field->type.rows != 4 && (
							field->semantic.compare(0, 9, "SV_TARGET") == 0 ||
							field->semantic.compare(0, 5, "COLOR") == 0))
						{
							error(node->location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
							return;
						}
					}
				}
				else
				{
					if (parameter->semantic == "SV_POSITION" ||
						parameter->semantic == "POSITION")
					{
						position_variable = parameter->name;
					}
					else if (
						parameter->semantic.compare(0, 9, "SV_TARGET") == 0 ||
						parameter->semantic.compare(0, 5, "COLOR") == 0)
					{
						parameter_type.rows = 4;

						initialization += parameter->name + " = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
					}
				}
			}

			visit(source, parameter_type);

			source << ' ' << parameter->name;

			if (parameter_type.is_array())
			{
				source << '[';

				if (parameter_type.array_length > 0)
				{
					source << parameter_type.array_length;
				}

				source << ']';
			}

			if (!parameter->semantic.empty())
			{
				source << " : " << convert_semantic(parameter->semantic);
			}

			if (i < count - 1)
			{
				source << ", ";
			}
		}

		source << ')';

		if (!node->return_semantic.empty())
		{
			source << " : " << convert_semantic(node->return_semantic);
		}

		source << "\n{\n" << initialization;

		if (!node->return_type.is_void())
		{
			visit(source, return_type, false);

			source << " _return = ";
		}

		if (node->return_type.rows != return_type.rows)
		{
			source << "float4(";
		}

		source << node->unique_name << '(';

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			const auto parameter = node->parameter_list[i];

			source << parameter->name;

			if (parameter->semantic.compare(0, 9, "SV_TARGET") == 0 ||
				parameter->semantic.compare(0, 5, "COLOR") == 0)
			{
				source << '.';

				const char swizzle[] = { 'x', 'y', 'z', 'w' };

				for (unsigned int k = 0; k < parameter->type.rows; k++)
				{
					source << swizzle[k];
				}
			}

			if (i < count - 1)
			{
				source << ", ";
			}
		}

		source << ')';

		if (node->return_type.rows != return_type.rows)
		{
			for (unsigned int i = 0; i < 4 - node->return_type.rows; i++)
			{
				source << ", 0.0f";
			}

			source << ')';
		}

		source << ";\n";

		if (shadertype == "vs")
		{
			source << position_variable << ".xy += __TEXEL_SIZE__ * " << position_variable << ".ww;\n";
		}

		if (!node->return_type.is_void())
		{
			source << "return _return;\n";
		}

		source << "}\n";

		const std::string source_str = source.str();

#if RESHADE_DUMP_NATIVE_SHADERS
		if (!_dumped_shaders.count(node->unique_name))
		{
			std::ofstream dumpfile(_dump_filename.string(), std::ios::app);

			if (dumpfile.is_open())
			{
				dumpfile << "#ifdef RESHADE_SHADER_" << shadertype << "_" << node->unique_name << std::endl << source_str << "#endif" << std::endl << std::endl;

				_dumped_shaders.insert(node->unique_name);
			}
		}
#endif

		UINT flags = 0;
		com_ptr<ID3DBlob> compiled, errors;

		if (_skip_shader_optimization)
		{
			flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}

		const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3dcompiler_module, "D3DCompile"));
		HRESULT hr = D3DCompile(source_str.c_str(), source_str.size(), nullptr, nullptr, nullptr, "__main", (shadertype + "_3_0").c_str(), flags, 0, &compiled, &errors);

		if (errors != nullptr)
		{
			_errors.append(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize() - 1);
		}

		if (FAILED(hr))
		{
			error(node->location, "internal shader compilation failed");
			return;
		}

		if (shadertype == "vs")
		{
			hr = _runtime->_device->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.vertex_shader);
		}
		else if (shadertype == "ps")
		{
			hr = _runtime->_device->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.pixel_shader);
		}

		if (FAILED(hr))
		{
			error(node->location, "internal shader creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			return;
		}
	}
}
