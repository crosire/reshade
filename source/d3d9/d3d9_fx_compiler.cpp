#include "log.hpp"
#include "d3d9_fx_compiler.hpp"

#include <d3dcompiler.h>

namespace reshade
{
	namespace
	{
		bool is_pow2(int x)
		{
			return ((x > 0) && ((x & (x - 1)) == 0));
		}
		D3DBLEND literal_to_blend_func(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::pass_declaration_node::ZERO:
					return D3DBLEND_ZERO;
				case fx::nodes::pass_declaration_node::ONE:
					return D3DBLEND_ONE;
			}

			return static_cast<D3DBLEND>(value);
		}
		D3DSTENCILOP literal_to_stencil_op(unsigned int value)
		{
			if (value == fx::nodes::pass_declaration_node::ZERO)
			{
				return D3DSTENCILOP_ZERO;
			}

			return static_cast<D3DSTENCILOP>(value);
		}
		D3DFORMAT literal_to_format(texture_format value)
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
		std::string convert_semantic(const std::string &semantic)
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
	}

	d3d9_fx_compiler::d3d9_fx_compiler(d3d9_runtime *runtime, const fx::syntax_tree &ast, std::string &errors, bool skipoptimization) :
		_runtime(runtime),
		_success(true),
		_ast(ast),
		_errors(errors),
		_skip_shader_optimization(skipoptimization),
		_current_function(nullptr)
	{
	}

	bool d3d9_fx_compiler::run()
	{
		for (auto node : _ast.structs)
		{
			visit(_global_code, static_cast<fx::nodes::struct_declaration_node *>(node));
		}

		for (auto node : _ast.variables)
		{
			const auto uniform = static_cast<fx::nodes::variable_declaration_node *>(node);

			if (uniform->type.is_texture())
			{
				visit_texture(uniform);
			}
			else if (uniform->type.is_sampler())
			{
				visit_sampler(uniform);
			}
			else if (uniform->type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
			{
				visit_uniform(uniform);
			}
			else
			{
				visit(_global_code, uniform);

				_global_code << ";\n";
			}
		}

		for (auto node : _ast.functions)
		{
			const auto function = static_cast<fx::nodes::function_declaration_node *>(node);
			std::stringstream function_code;

			_functions[_current_function = function];

			visit(function_code, function);

			_functions[function].code = function_code.str();
		}

		for (auto node : _ast.techniques)
		{
			visit_technique(static_cast<fx::nodes::technique_declaration_node *>(node));
		}

		return _success;
	}

	void d3d9_fx_compiler::error(const fx::location &location, const std::string &message)
	{
		_success = false;

		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
	}
	void d3d9_fx_compiler::warning(const fx::location &location, const std::string &message)
	{
		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
	}

	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::statement_node *node)
	{
		if (node == nullptr)
		{
			return;
		}

		switch (node->id)
		{
			case fx::nodeid::compound_statement:
				visit(output, static_cast<const fx::nodes::compound_statement_node *>(node));
				break;
			case fx::nodeid::declarator_list:
				visit(output, static_cast<const fx::nodes::declarator_list_node *>(node), false);
				break;
			case fx::nodeid::expression_statement:
				visit(output, static_cast<const fx::nodes::expression_statement_node *>(node));
				break;
			case fx::nodeid::if_statement:
				visit(output, static_cast<const fx::nodes::if_statement_node *>(node));
				break;
			case fx::nodeid::switch_statement:
				visit(output, static_cast<const fx::nodes::switch_statement_node *>(node));
				break;
			case fx::nodeid::for_statement:
				visit(output, static_cast<const fx::nodes::for_statement_node *>(node));
				break;
			case fx::nodeid::while_statement:
				visit(output, static_cast<const fx::nodes::while_statement_node *>(node));
				break;
			case fx::nodeid::return_statement:
				visit(output, static_cast<const fx::nodes::return_statement_node *>(node));
				break;
			case fx::nodeid::jump_statement:
				visit(output, static_cast<const fx::nodes::jump_statement_node *>(node));
				break;
			default:
				assert(false);
		}
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_node *node)
	{
		assert(node != nullptr);

		switch (node->id)
		{
			case fx::nodeid::lvalue_expression:
				visit(output, static_cast<const fx::nodes::lvalue_expression_node *>(node));
				break;
			case fx::nodeid::literal_expression:
				visit(output, static_cast<const fx::nodes::literal_expression_node *>(node));
				break;
			case fx::nodeid::expression_sequence:
				visit(output, static_cast<const fx::nodes::expression_sequence_node *>(node));
				break;
			case fx::nodeid::unary_expression:
				visit(output, static_cast<const fx::nodes::unary_expression_node *>(node));
				break;
			case fx::nodeid::binary_expression:
				visit(output, static_cast<const fx::nodes::binary_expression_node *>(node));
				break;
			case fx::nodeid::intrinsic_expression:
				visit(output, static_cast<const fx::nodes::intrinsic_expression_node *>(node));
				break;
			case fx::nodeid::conditional_expression:
				visit(output, static_cast<const fx::nodes::conditional_expression_node *>(node));
				break;
			case fx::nodeid::swizzle_expression:
				visit(output, static_cast<const fx::nodes::swizzle_expression_node *>(node));
				break;
			case fx::nodeid::field_expression:
				visit(output, static_cast<const fx::nodes::field_expression_node *>(node));
				break;
			case fx::nodeid::initializer_list:
				visit(output, static_cast<const fx::nodes::initializer_list_node *>(node));
				break;
			case fx::nodeid::assignment_expression:
				visit(output, static_cast<const fx::nodes::assignment_expression_node *>(node));
				break;
			case fx::nodeid::call_expression:
				visit(output, static_cast<const fx::nodes::call_expression_node *>(node));
				break;
			case fx::nodeid::constructor_expression:
				visit(output, static_cast<const fx::nodes::constructor_expression_node *>(node));
				break;
			default:
				assert(false);
		}
	}

	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::type_node &type, bool with_qualifiers)
	{
		if (with_qualifiers)
		{
			if (type.has_qualifier(fx::nodes::type_node::qualifier_static))
				output << "static ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_const))
				output << "const ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_volatile))
				output << "volatile ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_precise))
				output << "precise ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_linear))
				output << "linear ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_noperspective))
				output << "noperspective ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_centroid))
				output << "centroid ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_nointerpolation))
				output << "nointerpolation ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_inout))
				output << "inout ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_in))
				output << "in ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_out))
				output << "out ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
				output << "uniform ";
		}

		switch (type.basetype)
		{
			case fx::nodes::type_node::datatype_void:
				output << "void";
				break;
			case fx::nodes::type_node::datatype_bool:
				output << "bool";
				break;
			case fx::nodes::type_node::datatype_int:
				output << "int";
				break;
			case fx::nodes::type_node::datatype_uint:
				output << "uint";
				break;
			case fx::nodes::type_node::datatype_float:
				output << "float";
				break;
			case fx::nodes::type_node::datatype_sampler:
				output << "__sampler2D";
				break;
			case fx::nodes::type_node::datatype_struct:
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::lvalue_expression_node *node)
	{
		output << node->reference->unique_name;

		if (node->reference->type.is_sampler() && _samplers.find(node->reference->name) != _samplers.end())
		{
			_functions.at(_current_function).sampler_dependencies.insert(node->reference);
		}
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::literal_expression_node *node)
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
				case fx::nodes::type_node::datatype_bool:
					output << (node->value_int[i] ? "true" : "false");
					break;
				case fx::nodes::type_node::datatype_int:
					output << node->value_int[i];
					break;
				case fx::nodes::type_node::datatype_uint:
					output << node->value_uint[i];
					break;
				case fx::nodes::type_node::datatype_float:
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_sequence_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::unary_expression_node *node)
	{
		switch (node->op)
		{
			case fx::nodes::unary_expression_node::negate:
				output << '-';
				break;
			case fx::nodes::unary_expression_node::bitwise_not:
				output << "(4294967295 - ";
				break;
			case fx::nodes::unary_expression_node::logical_not:
				output << '!';
				break;
			case fx::nodes::unary_expression_node::pre_increase:
				output << "++";
				break;
			case fx::nodes::unary_expression_node::pre_decrease:
				output << "--";
				break;
			case fx::nodes::unary_expression_node::cast:
				visit(output, node->type, false);
				output << '(';
				break;
		}

		visit(output, node->operand);

		switch (node->op)
		{
			case fx::nodes::unary_expression_node::cast:
			case fx::nodes::unary_expression_node::bitwise_not:
				output << ')';
				break;
			case fx::nodes::unary_expression_node::post_increase:
				output << "++";
				break;
			case fx::nodes::unary_expression_node::post_decrease:
				output << "--";
				break;
		}
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::binary_expression_node *node)
	{
		std::string part1, part2, part3;

		switch (node->op)
		{
			case fx::nodes::binary_expression_node::add:
				part1 = '(';
				part2 = " + ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::subtract:
				part1 = '(';
				part2 = " - ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::multiply:
				part1 = '(';
				part2 = " * ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::divide:
				part1 = '(';
				part2 = " / ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::modulo:
				part1 = '(';
				part2 = " % ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::less:
				part1 = '(';
				part2 = " < ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::greater:
				part1 = '(';
				part2 = " > ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::less_equal:
				part1 = '(';
				part2 = " <= ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::greater_equal:
				part1 = '(';
				part2 = " >= ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::equal:
				part1 = '(';
				part2 = " == ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::not_equal:
				part1 = '(';
				part2 = " != ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::left_shift:
				part1 = "((";
				part2 = ") * exp2(";
				part3 = "))";
				break;
			case fx::nodes::binary_expression_node::right_shift:
				part1 = "floor((";
				part2 = ") / exp2(";
				part3 = "))";
				break;
			case fx::nodes::binary_expression_node::bitwise_and:
				if (node->operands[1]->id == fx::nodeid::literal_expression && node->operands[1]->type.is_integral())
				{
					const unsigned int value = static_cast<const fx::nodes::literal_expression_node *>(node->operands[1])->value_uint[0];

					if (is_pow2(value + 1))
					{
						output << "(("  << (value + 1) << ") * frac((";
						visit(output, node->operands[0]);
						output << ") / (" << (value + 1) << ")))";
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
			case fx::nodes::binary_expression_node::bitwise_or:
			case fx::nodes::binary_expression_node::bitwise_xor:
				error(node->location, "bitwise operations are not supported in Direct3D9");
				return;
			case fx::nodes::binary_expression_node::logical_and:
				part1 = '(';
				part2 = " && ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::logical_or:
				part1 = '(';
				part2 = " || ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::element_extract:
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::intrinsic_expression_node *node)
	{
		std::string part1, part2, part3, part4, part5;

		switch (node->op)
		{
			case fx::nodes::intrinsic_expression_node::abs:
				part1 = "abs(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::acos:
				part1 = "acos(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::all:
				part1 = "all(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::any:
				part1 = "any(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_int2float:
			case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
			case fx::nodes::intrinsic_expression_node::bitcast_float2int:
			case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
				error(node->location, "'asint', 'asuint' and 'asfloat' are not supported in Direct3D9");
				return;
			case fx::nodes::intrinsic_expression_node::asin:
				part1 = "asin(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::atan:
				part1 = "atan(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::atan2:
				part1 = "atan2(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::ceil:
				part1 = "ceil(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::clamp:
				part1 = "clamp(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::cos:
				part1 = "cos(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::cosh:
				part1 = "cosh(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::cross:
				part1 = "cross(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::ddx:
				part1 = "ddx(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::ddy:
				part1 = "ddy(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::degrees:
				part1 = "degrees(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::determinant:
				part1 = "determinant(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::distance:
				part1 = "distance(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::dot:
				part1 = "dot(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::exp:
				part1 = "exp(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::exp2:
				part1 = "exp2(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::faceforward:
				part1 = "faceforward(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::floor:
				part1 = "floor(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::frac:
				part1 = "frac(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::frexp:
				part1 = "frexp(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::fwidth:
				part1 = "fwidth(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::ldexp:
				part1 = "ldexp(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::length:
				part1 = "length(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::lerp:
				part1 = "lerp(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::log:
				part1 = "log(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::log10:
				part1 = "log10(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::log2:
				part1 = "log2(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::mad:
				part1 = "((";
				part2 = ") * (";
				part3 = ") + (";
				part4 = "))";
				break;
			case fx::nodes::intrinsic_expression_node::max:
				part1 = "max(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::min:
				part1 = "min(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::modf:
				part1 = "modf(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::mul:
				part1 = "mul(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::normalize:
				part1 = "normalize(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::pow:
				part1 = "pow(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::radians:
				part1 = "radians(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::rcp:
				part1 = "(1.0f / ";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::reflect:
				part1 = "reflect(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::refract:
				part1 = "refract(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::round:
				part1 = "round(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::rsqrt:
				part1 = "rsqrt(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::saturate:
				part1 = "saturate(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::sign:
				part1 = "sign(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::sin:
				part1 = "sin(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::sincos:
				part1 = "sincos(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::sinh:
				part1 = "sinh(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::smoothstep:
				part1 = "smoothstep(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::sqrt:
				part1 = "sqrt(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::step:
				part1 = "step(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tan:
				part1 = "tan(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tanh:
				part1 = "tanh(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture:
				part1 = "tex2D((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_fetch:
				part1 = "tex2Dlod((";
				part2 = ").s, float4(";
				part3 = "))";
				break;
			case fx::nodes::intrinsic_expression_node::texture_gather:
				if (node->arguments[2]->id == fx::nodeid::literal_expression && node->arguments[2]->type.is_integral())
				{
					const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[2])->value_int[0];

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
			case fx::nodes::intrinsic_expression_node::texture_gather_offset:
				if (node->arguments[3]->id == fx::nodeid::literal_expression && node->arguments[3]->type.is_integral())
				{
					const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[3])->value_int[0];

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
			case fx::nodes::intrinsic_expression_node::texture_gradient:
				part1 = "tex2Dgrad((";
				part2 = ").s, ";
				part3 = ", ";
				part4 = ", ";
				part5 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_level:
				part1 = "tex2Dlod((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_level_offset:
				part1 = "__tex2Dlodoffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_offset:
				part1 = "__tex2Doffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_projection:
				part1 = "tex2Dproj((";
				part2 = ").s, ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::texture_size:
				part1 = "__tex2Dsize(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::transpose:
				part1 = "transpose(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::trunc:
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::conditional_expression_node *node)
	{
		output << '(';
		visit(output, node->condition);
		output << " ? ";
		visit(output, node->expression_when_true);
		output << " : ";
		visit(output, node->expression_when_false);
		output << ')';
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::swizzle_expression_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::field_expression_node *node)
	{
		output << '(';
		visit(output, node->operand);
		output << '.' << node->field_reference->unique_name << ')';
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::assignment_expression_node *node)
	{
		std::string part1, part2, part3;

		switch (node->op)
		{
			case fx::nodes::assignment_expression_node::none:
				part2 = " = ";
				break;
			case fx::nodes::assignment_expression_node::add:
				part2 = " += ";
				break;
			case fx::nodes::assignment_expression_node::subtract:
				part2 = " -= ";
				break;
			case fx::nodes::assignment_expression_node::multiply:
				part2 = " *= ";
				break;
			case fx::nodes::assignment_expression_node::divide:
				part2 = " /= ";
				break;
			case fx::nodes::assignment_expression_node::modulo:
				part2 = " %= ";
				break;
			case fx::nodes::assignment_expression_node::left_shift:
				part1 = "((";
				part2 = ") *= pow(2, ";
				part3 = "))";
				break;
			case fx::nodes::assignment_expression_node::right_shift:
				part1 = "((";
				part2 = ") /= pow(2, ";
				part3 = "))";
				break;
			case fx::nodes::assignment_expression_node::bitwise_and:
			case fx::nodes::assignment_expression_node::bitwise_or:
			case fx::nodes::assignment_expression_node::bitwise_xor:
				error(node->location, "bitwise operations are not supported in Direct3D9");
				return;
		}

		output << '(' << part1;
		visit(output, node->left);
		output << part2;
		visit(output, node->right);
		output << part3 << ')';
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::call_expression_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::constructor_expression_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::initializer_list_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::compound_statement_node *node)
	{
		output << "{\n";

		for (auto statement : node->statement_list)
		{
			visit(output, statement);
		}

		output << "}\n";
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::declarator_list_node *node, bool single_statement)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_statement_node *node)
	{
		visit(output, node->expression);

		output << ";\n";
	}
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::if_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::switch_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::case_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::for_statement_node *node)
	{
		for (const auto &attribute : node->attributes)
		{
			output << '[' << attribute << ']';
		}

		output << "for (";

		if (node->init_statement != nullptr)
		{
			if (node->init_statement->id == fx::nodeid::declarator_list)
			{
				visit(output, static_cast<fx::nodes::declarator_list_node *>(node->init_statement), true);

				output.seekp(-2, std::ios_base::end);
			}
			else
			{
				visit(output, static_cast<fx::nodes::expression_statement_node *>(node->init_statement)->expression);
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::while_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::return_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::jump_statement_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::struct_declaration_node *node)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type, bool with_semantic)
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
	void d3d9_fx_compiler::visit(std::stringstream &output, const fx::nodes::function_declaration_node *node)
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

	template <typename T>
	void visit_annotation(const std::vector<fx::nodes::annotation_node> &annotations, T &object)
	{
		for (auto &annotation : annotations)
		{
			switch (annotation.value->type.basetype)
			{
				case fx::nodes::type_node::datatype_bool:
				case fx::nodes::type_node::datatype_int:
					object.annotations[annotation.name] = annotation.value->value_int;
					break;
				case fx::nodes::type_node::datatype_uint:
					object.annotations[annotation.name] = annotation.value->value_uint;
					break;
				case fx::nodes::type_node::datatype_float:
					object.annotations[annotation.name] = annotation.value->value_float;
					break;
				case fx::nodes::type_node::datatype_string:
					object.annotations[annotation.name] = annotation.value->value_string;
					break;
			}
		}
	}

	void d3d9_fx_compiler::visit_texture(const fx::nodes::variable_declaration_node *node)
	{
		const auto obj = new d3d9_texture();
		obj->name = node->name;
		UINT width = obj->width = node->properties.width;
		UINT height = obj->height = node->properties.height;
		UINT levels = obj->levels = node->properties.levels;
		const D3DFORMAT format = literal_to_format(obj->format = node->properties.format);

		visit_annotation(node->annotations, *obj);

		if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
		{
			if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
			{
				warning(node->location, "texture properties on backbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(*obj, texture_type::backbuffer, _runtime->_backbuffer_texture);
		}
		else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
		{
			if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
			{
				warning(node->location, "texture properties on depthbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(*obj, texture_type::depthbuffer, _runtime->_depthstencil_texture);
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
			else if (levels == 0)
			{
				warning(node->location, "a texture cannot have 0 miplevels, changed it to 1");

				levels = 1;
			}

			HRESULT hr = _runtime->_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format);

			if (SUCCEEDED(hr))
			{
				usage |= D3DUSAGE_RENDERTARGET;
			}

			hr = _runtime->_device->CreateTexture(width, height, levels, usage, format, D3DPOOL_DEFAULT, &obj->texture, nullptr);

			if (FAILED(hr))
			{
				error(node->location, "internal texture creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			hr = obj->texture->GetSurfaceLevel(0, &obj->surface);

			assert(SUCCEEDED(hr));
		}

		_runtime->add_texture(obj);
	}
	void d3d9_fx_compiler::visit_sampler(const fx::nodes::variable_declaration_node *node)
	{
		if (node->properties.texture == nullptr)
		{
			error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
			return;
		}

		const auto texture = static_cast<d3d9_texture *>(_runtime->find_texture(node->properties.texture->name));

		if (texture == nullptr)
		{
			error(node->location, "texture not found");
			return;
		}

		d3d9_sampler sampler;
		sampler.Texture = texture;
		sampler.States[D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_u);
		sampler.States[D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_v);
		sampler.States[D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(node->properties.address_w);
		sampler.States[D3DSAMP_BORDERCOLOR] = 0;
		sampler.States[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&node->properties.lod_bias);
		sampler.States[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, node->properties.min_lod));
		sampler.States[D3DSAMP_MAXANISOTROPY] = node->properties.max_anisotropy;
		sampler.States[D3DSAMP_SRGBTEXTURE] = node->properties.srgb_texture;

		if (node->properties.filter == texture_filter::anisotropic)
		{
			sampler.States[D3DSAMP_MINFILTER] = sampler.States[D3DSAMP_MAGFILTER] = sampler.States[D3DSAMP_MIPFILTER] = D3DTEXF_ANISOTROPIC;
		}
		else
		{
			sampler.States[D3DSAMP_MINFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0x30) >> 4);
			sampler.States[D3DSAMP_MAGFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0xC) >> 2);
			sampler.States[D3DSAMP_MIPFILTER] = 1 + ((static_cast<unsigned int>(node->properties.filter) & 0x3));
		}

		_samplers[node->name] = sampler;
	}
	void d3d9_fx_compiler::visit_uniform(const fx::nodes::variable_declaration_node *node)
	{
		visit(_global_code, node->type);

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

		_global_code << " : register(c" << _runtime->_constant_register_count << ");\n";

		uniform obj;
		obj.name = node->name;
		obj.rows = node->type.rows;
		obj.columns = node->type.cols;
		obj.elements = node->type.array_length;
		obj.storage_size = obj.rows * obj.columns * std::max(1u, obj.elements);

		switch (node->type.basetype)
		{
			case fx::nodes::type_node::datatype_bool:
				obj.basetype = uniform_datatype::bool_;
				obj.storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_int:
				obj.basetype = uniform_datatype::int_;
				obj.storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_uint:
				obj.basetype = uniform_datatype::uint_;
				obj.storage_size *= sizeof(unsigned int);
				break;
			case fx::nodes::type_node::datatype_float:
				obj.basetype = uniform_datatype::float_;
				obj.storage_size *= sizeof(float);
				break;
		}

		const UINT regsize = static_cast<UINT>(static_cast<float>(obj.storage_size) / 4);
		const UINT regalignment = 4 - (regsize % 4);

		obj.storage_offset = _runtime->_constant_register_count * 16;
		_runtime->_constant_register_count += (regsize + regalignment) * 4;

		visit_annotation(node->annotations, obj);

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (_runtime->_constant_register_count * 16 >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 4096);
		}

		if (node->initializer_expression != nullptr && node->initializer_expression->id == fx::nodeid::literal_expression)
		{
			CopyMemory(uniform_storage.data() + obj.storage_offset, &static_cast<const fx::nodes::literal_expression_node *>(node->initializer_expression)->value_float, obj.storage_size);
		}
		else
		{
			ZeroMemory(uniform_storage.data() + obj.storage_offset, obj.storage_size);
		}

		_runtime->add_uniform(std::move(obj));
	}
	void d3d9_fx_compiler::visit_technique(const fx::nodes::technique_declaration_node *node)
	{
		technique obj;
		obj.name = node->name;

		visit_annotation(node->annotation_list, obj);

		for (auto pass : node->pass_list)
		{
			obj.passes.emplace_back(new d3d9_pass());
			visit_pass(pass, *static_cast<d3d9_pass *>(obj.passes.back().get()));
		}

		_runtime->add_technique(std::move(obj));
	}
	void d3d9_fx_compiler::visit_pass(const fx::nodes::pass_declaration_node *node, d3d9_pass & pass)
	{
		pass.render_targets[0] = _runtime->_backbuffer_resolved.get();
		pass.clear_render_targets = node->clear_render_targets;

		std::string samplers;
		const char shader_types[2][3] = { "vs", "ps" };
		const fx::nodes::function_declaration_node *shader_functions[2] = { node->vertex_shader, node->pixel_shader };

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

				samplers += "sampler2D __Sampler";
				samplers += sampler->unique_name;
				samplers += " : register(s" + std::to_string(pass.sampler_count++) + ");\n";
				samplers += "static const __sampler2D ";
				samplers += sampler->unique_name;
				samplers += " = { __Sampler";
				samplers += sampler->unique_name;

				if (texture->semantic == "COLOR" || texture->semantic == "SV_TARGET" || texture->semantic == "DEPTH" || texture->semantic == "SV_DEPTH")
				{
					samplers += ", float2(" + std::to_string(1.0f / _runtime->frame_width()) + ", " + std::to_string(1.0f / _runtime->frame_height()) + ")";
				}
				else
				{
					samplers += ", float2(" + std::to_string(1.0f / texture->properties.width) + ", " + std::to_string(1.0f / texture->properties.height) + ")";
				}

				samplers += " };\n";

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
				visit_pass_shader(shader_functions[i], shader_types[i], samplers, pass);
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

		device->SetRenderState(D3DRS_ZENABLE, node->depth_enable);
		device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, literal_to_blend_func(node->src_blend));
		device->SetRenderState(D3DRS_DESTBLEND, literal_to_blend_func(node->dest_blend));
		device->SetRenderState(D3DRS_ZFUNC, static_cast<D3DCMPFUNC>(node->depth_comparison_func));
		device->SetRenderState(D3DRS_ALPHAREF, 0);
		device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
		device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
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
		device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
		device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
		device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, node->color_write_mask);
		device->SetRenderState(D3DRS_BLENDOP, static_cast<D3DBLENDOP>(node->blend_op));
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
		device->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
		device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
		device->SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
		device->SetRenderState(D3DRS_COLORWRITEENABLE1, 0x0000000F);
		device->SetRenderState(D3DRS_COLORWRITEENABLE2, 0x0000000F);
		device->SetRenderState(D3DRS_COLORWRITEENABLE3, 0x0000000F);
		device->SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, node->srgb_write_enable);
		device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
		device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
		device->SetRenderState(D3DRS_BLENDOPALPHA, static_cast<D3DBLENDOP>(node->blend_op_alpha));
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);

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

			const auto texture = static_cast<d3d9_texture *>(_runtime->find_texture(node->render_targets[i]->name));

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
				return;
			}

			pass.render_targets[i] = texture->surface.get();
		}
	}
	void d3d9_fx_compiler::visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, const std::string &samplers, d3d9_pass &pass)
	{
		std::stringstream source;

		source <<
			"struct __sampler2D { sampler2D s; float2 pixelsize; };\n"
			"float4 __tex2Dgather0(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c, 0, 0)).r); }\n"
			"float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c, 0, 0)).g); }\n"
			"float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c, 0, 0)).b); }\n"
			"float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c, 0, 0)).a); }\n"
			"float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather0(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather1(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather2(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather3(s, c + offset * s.pixelsize); }\n"
			"float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return tex2Dlod(s.s, c + float4(offset * s.pixelsize, 0, 0)); }\n"
			"float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return tex2D(s.s, c + offset * s.pixelsize); }\n"
			"int2 __tex2Dsize(__sampler2D s, int lod) { return int2(1 / s.pixelsize) / exp2(lod); }\n";

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
		fx::nodes::type_node return_type = node->return_type;

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
			fx::nodes::type_node parameter_type = parameter->type;

			if (parameter->type.has_qualifier(fx::nodes::type_node::qualifier_out))
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

		const auto source_str = source.str();

		LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source_str.c_str() << "\n";

		UINT flags = 0;
		com_ptr<ID3DBlob> compiled, errors;

		if (_skip_shader_optimization)
		{
			flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}

		HRESULT hr = D3DCompile(source_str.c_str(), source_str.size(), nullptr, nullptr, nullptr, "__main", (shadertype + "_3_0").c_str(), flags, 0, &compiled, &errors);

		if (errors != nullptr)
		{
			_errors.append(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());
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
