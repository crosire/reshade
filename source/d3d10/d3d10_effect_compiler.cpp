/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d10_runtime.hpp"
#include "d3d10_effect_compiler.hpp"
#include <assert.h>
#include <iomanip>
#include <algorithm>
#include <d3dcompiler.h>

namespace reshade::d3d10
{
	using namespace reshadefx;
	using namespace reshadefx::nodes;

	static inline size_t roundto16(size_t size)
	{
		return (size + 15) & ~15;
	}
	static D3D10_BLEND literal_to_blend_func(unsigned int value)
	{
		switch (value)
		{
			case pass_declaration_node::ZERO:
				return D3D10_BLEND_ZERO;
			case pass_declaration_node::ONE:
				return D3D10_BLEND_ONE;
		}

		return static_cast<D3D10_BLEND>(value);
	}
	static D3D10_STENCIL_OP literal_to_stencil_op(unsigned int value)
	{
		if (value == pass_declaration_node::ZERO)
		{
			return D3D10_STENCIL_OP_ZERO;
		}

		return static_cast<D3D10_STENCIL_OP>(value);
	}
	static DXGI_FORMAT literal_to_format(texture_format value)
	{
		switch (value)
		{
			case texture_format::r8:
				return DXGI_FORMAT_R8_UNORM;
			case texture_format::r16f:
				return DXGI_FORMAT_R16_FLOAT;
			case texture_format::r32f:
				return DXGI_FORMAT_R32_FLOAT;
			case texture_format::rg8:
				return DXGI_FORMAT_R8G8_UNORM;
			case texture_format::rg16:
				return DXGI_FORMAT_R16G16_UNORM;
			case texture_format::rg16f:
				return DXGI_FORMAT_R16G16_FLOAT;
			case texture_format::rg32f:
				return DXGI_FORMAT_R32G32_FLOAT;
			case texture_format::rgba8:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case texture_format::rgba16:
				return DXGI_FORMAT_R16G16B16A16_UNORM;
			case texture_format::rgba16f:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case texture_format::rgba32f:
				return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case texture_format::dxt1:
				return DXGI_FORMAT_BC1_TYPELESS;
			case texture_format::dxt3:
				return DXGI_FORMAT_BC2_TYPELESS;
			case texture_format::dxt5:
				return DXGI_FORMAT_BC3_TYPELESS;
			case texture_format::latc1:
				return DXGI_FORMAT_BC4_UNORM;
			case texture_format::latc2:
				return DXGI_FORMAT_BC5_UNORM;
		}

		return DXGI_FORMAT_UNKNOWN;
	}
	static std::string convert_semantic(const std::string &semantic)
	{
		if (semantic == "VERTEXID")
		{
			return "SV_VERTEXID";
		}
		else if (semantic == "POSITION" || semantic == "VPOS")
		{
			return "SV_POSITION";
		}
		else if (semantic.compare(0, 5, "COLOR") == 0)
		{
			return "SV_TARGET" + semantic.substr(5);
		}
		else if (semantic == "DEPTH")
		{
			return "SV_DEPTH";
		}

		return semantic;
	}
	DXGI_FORMAT make_format_srgb(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
				return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
				return DXGI_FORMAT_BC1_UNORM_SRGB;
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
				return DXGI_FORMAT_BC2_UNORM_SRGB;
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
				return DXGI_FORMAT_BC3_UNORM_SRGB;
			default:
				return format;
		}
	}
	DXGI_FORMAT make_format_normal(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				return DXGI_FORMAT_BC1_UNORM;
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				return DXGI_FORMAT_BC2_UNORM;
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				return DXGI_FORMAT_BC3_UNORM;
			default:
				return format;
		}
	}
	DXGI_FORMAT make_format_typeless(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				return DXGI_FORMAT_BC1_TYPELESS;
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				return DXGI_FORMAT_BC2_TYPELESS;
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				return DXGI_FORMAT_BC3_TYPELESS;
			default:
				return format;
		}
	}

	d3d10_effect_compiler::d3d10_effect_compiler(d3d10_runtime *runtime, const syntax_tree &ast, std::string &errors, bool skipoptimization) :
		_runtime(runtime),
		_ast(ast),
		_errors(errors),
		_skip_shader_optimization(skipoptimization)
	{
	}

	bool d3d10_effect_compiler::run()
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
			visit(_global_code, function);
		}
		for (auto technique : _ast.techniques)
		{
			visit_technique(technique);
		}

		if (_constant_buffer_size != 0)
		{
			_constant_buffer_size = roundto16(_constant_buffer_size);
			_runtime->get_uniform_value_storage().resize(_uniform_storage_offset + _constant_buffer_size);

			const CD3D10_BUFFER_DESC globals_desc(static_cast<UINT>(_constant_buffer_size), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
			const D3D10_SUBRESOURCE_DATA globals_initial = { _runtime->get_uniform_value_storage().data(), static_cast<UINT>(_constant_buffer_size) };

			com_ptr<ID3D10Buffer> constant_buffer;
			_runtime->_device->CreateBuffer(&globals_desc, &globals_initial, &constant_buffer);

			_runtime->_constant_buffers.push_back(std::move(constant_buffer));
		}

		FreeLibrary(_d3dcompiler_module);

		return _success;
	}

	void d3d10_effect_compiler::error(const location &location, const std::string &message)
	{
		_success = false;

		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
	}
	void d3d10_effect_compiler::warning(const location &location, const std::string &message)
	{
		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
	}

	void d3d10_effect_compiler::visit(std::stringstream &output, const statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const expression_node *node)
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

	void d3d10_effect_compiler::visit(std::stringstream &output, const type_node &type, bool with_qualifiers)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const lvalue_expression_node *node)
	{
		output << node->reference->unique_name;
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const literal_expression_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const expression_sequence_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const unary_expression_node *node)
	{
		switch (node->op)
		{
			case unary_expression_node::negate:
				output << '-';
				break;
			case unary_expression_node::bitwise_not:
				output << "~";
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
			case unary_expression_node::post_increase:
				output << "++";
				break;
			case unary_expression_node::post_decrease:
				output << "--";
				break;
			case unary_expression_node::cast:
				output << ')';
				break;
		}
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const binary_expression_node *node)
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
				part1 = "(";
				part2 = " << ";
				part3 = ")";
				break;
			case binary_expression_node::right_shift:
				part1 = "(";
				part2 = " >> ";
				part3 = ")";
				break;
			case binary_expression_node::bitwise_and:
				part1 = "(";
				part2 = " & ";
				part3 = ")";
				break;
			case binary_expression_node::bitwise_or:
				part1 = "(";
				part2 = " | ";
				part3 = ")";
				break;
			case binary_expression_node::bitwise_xor:
				part1 = "(";
				part2 = " ^ ";
				part3 = ")";
				break;
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const intrinsic_expression_node *node)
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
				part1 = "asfloat(";
				part2 = ")";
				break;
			case intrinsic_expression_node::bitcast_uint2float:
				part1 = "asfloat(";
				part2 = ")";
				break;
			case intrinsic_expression_node::asin:
				part1 = "asin(";
				part2 = ")";
				break;
			case intrinsic_expression_node::bitcast_float2int:
				part1 = "asint(";
				part2 = ")";
				break;
			case intrinsic_expression_node::bitcast_float2uint:
				part1 = "asuint(";
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
				part1 = "__tex2D(";
				part2 = ", ";
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
				part1 = "__tex2Dgrad(";
				part2 = ", ";
				part3 = ", ";
				part4 = ", ";
				part5 = ")";
				break;
			case intrinsic_expression_node::texture_level:
				part1 = "__tex2Dlod(";
				part2 = ", ";
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
				part1 = "__tex2Dproj(";
				part2 = ", ";
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const conditional_expression_node *node)
	{
		output << '(';
		visit(output, node->condition);
		output << " ? ";
		visit(output, node->expression_when_true);
		output << " : ";
		visit(output, node->expression_when_false);
		output << ')';
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const swizzle_expression_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const field_expression_node *node)
	{
		output << '(';

		visit(output, node->operand);

		output << '.' << node->field_reference->unique_name << ')';
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const assignment_expression_node *node)
	{
		output << '(';
		visit(output, node->left);
		output << ' ';

		switch (node->op)
		{
			case assignment_expression_node::none:
				output << '=';
				break;
			case assignment_expression_node::add:
				output << "+=";
				break;
			case assignment_expression_node::subtract:
				output << "-=";
				break;
			case assignment_expression_node::multiply:
				output << "*=";
				break;
			case assignment_expression_node::divide:
				output << "/=";
				break;
			case assignment_expression_node::modulo:
				output << "%=";
				break;
			case assignment_expression_node::left_shift:
				output << "<<=";
				break;
			case assignment_expression_node::right_shift:
				output << ">>=";
				break;
			case assignment_expression_node::bitwise_and:
				output << "&=";
				break;
			case assignment_expression_node::bitwise_or:
				output << "|=";
				break;
			case assignment_expression_node::bitwise_xor:
				output << "^=";
				break;
		}

		output << ' ';
		visit(output, node->right);
		output << ')';
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const call_expression_node *node)
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
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const constructor_expression_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const initializer_list_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const compound_statement_node *node)
	{
		output << "{\n";

		for (auto statement : node->statement_list)
		{
			visit(output, statement);
		}

		output << "}\n";
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const declarator_list_node *node, bool single_statement)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const expression_statement_node *node)
	{
		visit(output, node->expression);

		output << ";\n";
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const if_statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const switch_statement_node *node)
	{
		for (const auto &attribute : node->attributes)
		{
			output << '[' << attribute << ']';
		}

		output << "switch (";
		visit(output, node->test_expression);
		output << ")\n{\n";

		for (auto cases : node->case_list)
		{
			visit(output, cases);
		}

		output << "}\n";
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const case_statement_node *node)
	{
		for (auto label : node->labels)
		{
			if (label == nullptr)
			{
				output << "default";
			}
			else
			{
				output << "case ";

				visit(output, label);
			}

			output << ":\n";
		}

		visit(output, node->statement_list);
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const for_statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const while_statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const return_statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const jump_statement_node *node)
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
	void d3d10_effect_compiler::visit(std::stringstream &output, const struct_declaration_node *node)
	{
		output << "struct " << node->unique_name << "\n{\n";

		if (!node->field_list.empty())
		{
			for (auto field : node->field_list)
			{
				visit(output, field);
			}
		}
		else
		{
			output << "float _dummy;\n";
		}

		output << "};\n";
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const variable_declaration_node *node, bool with_type)
	{
		if (with_type)
		{
			visit(output, node->type);

			output << ' ';
		}

		if (!node->name.empty())
		{
			output << node->unique_name;
		}

		if (node->type.is_array())
		{
			output << '[';

			if (node->type.array_length > 0)
			{
				output << node->type.array_length;
			}

			output << ']';
		}

		if (!node->semantic.empty())
		{
			output << " : " << convert_semantic(node->semantic);
		}

		if (node->initializer_expression != nullptr)
		{
			output << " = ";

			visit(output, node->initializer_expression);
		}

		if (!(_is_in_parameter_block || _is_in_function_block))
		{
			output << ";\n";
		}
	}
	void d3d10_effect_compiler::visit(std::stringstream &output, const function_declaration_node *node)
	{
		visit(output, node->return_type, false);

		output << ' ' << node->unique_name << '(';

		_is_in_parameter_block = true;

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			visit(output, node->parameter_list[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		_is_in_parameter_block = false;

		output << ')';

		if (!node->return_semantic.empty())
		{
			output << " : " << convert_semantic(node->return_semantic);
		}

		output << '\n';

		_is_in_function_block = true;

		visit(output, node->definition);

		_is_in_function_block = false;
	}

	void d3d10_effect_compiler::visit_texture(const variable_declaration_node *node)
	{
		texture obj;
		D3D10_TEXTURE2D_DESC texdesc = { };
		obj.name = node->name;
		obj.unique_name = node->unique_name;
		obj.annotations = node->annotation_list;
		texdesc.Width = obj.width = node->properties.width;
		texdesc.Height = obj.height = node->properties.height;
		texdesc.MipLevels = obj.levels = node->properties.levels;
		texdesc.ArraySize = 1;
		texdesc.Format = literal_to_format(obj.format = node->properties.format);
		texdesc.SampleDesc.Count = 1;
		texdesc.SampleDesc.Quality = 0;
		texdesc.Usage = D3D10_USAGE_DEFAULT;
		texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		texdesc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

		size_t texture_register_index, texture_register_index_srgb;

		if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
		{
			obj.width = _runtime->frame_width();
			obj.height = _runtime->frame_height();
			obj.impl_reference = texture_reference::back_buffer;

			texture_register_index = 0;
			texture_register_index_srgb = 1;
		}
		else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
		{
			obj.width = _runtime->frame_width();
			obj.height = _runtime->frame_height();
			obj.impl_reference = texture_reference::depth_buffer;

			texture_register_index = 2;
			texture_register_index_srgb = 2;
		}
		else
		{
			obj.impl = std::make_unique<d3d10_tex_data>();
			const auto obj_data = obj.impl->as<d3d10_tex_data>();

			HRESULT hr = _runtime->_device->CreateTexture2D(&texdesc, nullptr, &obj_data->texture);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateTexture2D' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc = { };
			srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
			srvdesc.Format = make_format_normal(texdesc.Format);

			hr = _runtime->_device->CreateShaderResourceView(obj_data->texture.get(), &srvdesc, &obj_data->srv[0]);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			srvdesc.Format = make_format_srgb(texdesc.Format);

			texture_register_index = _runtime->_effect_shader_resources.size();
			_runtime->_effect_shader_resources.push_back(obj_data->srv[0].get());

			if (srvdesc.Format != texdesc.Format)
			{
				hr = _runtime->_device->CreateShaderResourceView(obj_data->texture.get(), &srvdesc, &obj_data->srv[1]);

				if (FAILED(hr))
				{
					error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}

				texture_register_index_srgb = _runtime->_effect_shader_resources.size();
				_runtime->_effect_shader_resources.push_back(obj_data->srv[1].get());
			}
			else
			{
				texture_register_index_srgb = texture_register_index;
			}
		}

		_global_code << "Texture2D " <<
			node->unique_name << " : register(t" << texture_register_index << "), __" <<
			node->unique_name << "SRGB : register(t" << texture_register_index_srgb << ");\n";

		_runtime->add_texture(std::move(obj));
	}
	void d3d10_effect_compiler::visit_sampler(const variable_declaration_node *node)
	{
		D3D10_SAMPLER_DESC desc = { };
		desc.Filter = static_cast<D3D10_FILTER>(node->properties.filter);
		desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.address_u);
		desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.address_v);
		desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.address_w);
		desc.MipLODBias = node->properties.lod_bias;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D10_COMPARISON_NEVER;
		desc.MinLOD = node->properties.min_lod;
		desc.MaxLOD = node->properties.max_lod;

		const auto texture = _runtime->find_texture(node->properties.texture->name);

		if (texture == nullptr)
		{
			error(node->location, "texture '" + node->properties.texture->name + "' for sampler '" + node->name + "' is missing due to previous error");
			return;
		}

		size_t desc_hash = 2166136261;
		for (size_t i = 0; i < sizeof(desc); ++i)
			desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

		auto it = _runtime->_effect_sampler_descs.find(desc_hash);

		if (it == _runtime->_effect_sampler_descs.end())
		{
			ID3D10SamplerState *sampler = nullptr;

			HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateSamplerState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			_runtime->_effect_sampler_states.push_back(sampler);
			it = _runtime->_effect_sampler_descs.emplace(desc_hash, _runtime->_effect_sampler_states.size() - 1).first;
		}

		_global_code << "static const __sampler2D " << node->unique_name << " = { ";

		if (node->properties.srgb_texture)
		{
			_global_code << "__" << node->properties.texture->unique_name << "SRGB";
		}
		else
		{
			_global_code << node->properties.texture->unique_name;
		}

		_global_code << ", __SamplerState" << it->second << " };\n";
	}
	void d3d10_effect_compiler::visit_uniform(const variable_declaration_node *node)
	{
		visit(_global_uniforms, node->type);

		_global_uniforms << ' ' << node->unique_name;

		if (node->type.is_array())
		{
			_global_uniforms << '[';

			if (node->type.array_length > 0)
			{
				_global_uniforms << node->type.array_length;
			}

			_global_uniforms << ']';
		}

		_global_uniforms << ";\n";

		uniform obj;
		obj.name = node->name;
		obj.unique_name = node->unique_name;
		obj.basetype = obj.displaytype = static_cast<uniform_datatype>(node->type.basetype - 1);
		obj.rows = node->type.rows;
		obj.columns = node->type.cols;
		obj.elements = node->type.array_length;
		obj.storage_size = node->type.rows * node->type.cols * std::max(1u, obj.elements) * 4;
		obj.annotations = node->annotation_list;

		const UINT alignment = 16 - (_constant_buffer_size % 16);
		_constant_buffer_size += static_cast<UINT>((obj.storage_size > alignment && (alignment != 16 || obj.storage_size <= 16)) ? obj.storage_size + alignment : obj.storage_size);
		obj.storage_offset = _uniform_storage_offset + _constant_buffer_size - obj.storage_size;

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (_uniform_storage_offset + _constant_buffer_size >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 128);
		}

		if (node->initializer_expression != nullptr && node->initializer_expression->id == nodeid::literal_expression)
		{
			CopyMemory(uniform_storage.data() + obj.storage_offset, &static_cast<const literal_expression_node *>(node->initializer_expression)->value_float, obj.storage_size);
		}
		else
		{
			ZeroMemory(uniform_storage.data() + obj.storage_offset, obj.storage_size);
		}

		_runtime->add_uniform(std::move(obj));
	}
	void d3d10_effect_compiler::visit_technique(const technique_declaration_node *node)
	{
		technique obj;
		obj.name = node->name;
		obj.annotations = node->annotation_list;

		if (_constant_buffer_size != 0)
		{
			obj.uniform_storage_index = _runtime->_constant_buffers.size();
			obj.uniform_storage_offset = _uniform_storage_offset;
		}

		for (auto pass : node->pass_list)
		{
			obj.passes.emplace_back(std::make_unique<d3d10_pass_data>());
			visit_pass(pass, *static_cast<d3d10_pass_data *>(obj.passes.back().get()));
		}

		_runtime->add_technique(std::move(obj));
	}
	void d3d10_effect_compiler::visit_pass(const pass_declaration_node *node, d3d10_pass_data &pass)
	{
		pass.stencil_reference = 0;
		pass.viewport.TopLeftX = pass.viewport.TopLeftY = pass.viewport.Width = pass.viewport.Height = 0;
		pass.viewport.MinDepth = 0.0f;
		pass.viewport.MaxDepth = 1.0f;
		pass.clear_render_targets = node->clear_render_targets;
		ZeroMemory(pass.render_targets, sizeof(pass.render_targets));
		ZeroMemory(pass.render_target_resources, sizeof(pass.render_target_resources));
		pass.shader_resources = _runtime->_effect_shader_resources;

		if (node->vertex_shader != nullptr)
		{
			visit_pass_shader(node->vertex_shader, "vs", pass);
		}
		if (node->pixel_shader != nullptr)
		{
			visit_pass_shader(node->pixel_shader, "ps", pass);
		}

		const int target_index = node->srgb_write_enable ? 1 : 0;
		pass.render_targets[0] = _runtime->_backbuffer_rtv[target_index].get();
		pass.render_target_resources[0] = _runtime->_backbuffer_texture_srv[target_index].get();

		for (unsigned int i = 0; i < 8; i++)
		{
			if (node->render_targets[i] == nullptr)
			{
				continue;
			}

			const auto texture = _runtime->find_texture(node->render_targets[i]->name);

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
				return;
			}

			const auto texture_impl = texture->impl->as<d3d10_tex_data>();

			D3D10_TEXTURE2D_DESC texture_desc;
			texture_impl->texture->GetDesc(&texture_desc);

			if (pass.viewport.Width != 0 && pass.viewport.Height != 0 && (texture_desc.Width != static_cast<unsigned int>(pass.viewport.Width) || texture_desc.Height != static_cast<unsigned int>(pass.viewport.Height)))
			{
				error(node->location, "cannot use multiple rendertargets with different sized textures");
				return;
			}
			else
			{
				pass.viewport.Width = texture_desc.Width;
				pass.viewport.Height = texture_desc.Height;
			}

			D3D10_RENDER_TARGET_VIEW_DESC rtvdesc = { };
			rtvdesc.Format = node->srgb_write_enable ? make_format_srgb(texture_desc.Format) : make_format_normal(texture_desc.Format);
			rtvdesc.ViewDimension = texture_desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

			if (texture_impl->rtv[target_index] == nullptr)
			{
				const HRESULT hr = _runtime->_device->CreateRenderTargetView(texture_impl->texture.get(), &rtvdesc, &texture_impl->rtv[target_index]);

				if (FAILED(hr))
				{
					warning(node->location, "'CreateRenderTargetView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				}
			}

			pass.render_targets[i] = texture_impl->rtv[target_index].get();
			pass.render_target_resources[i] = texture_impl->srv[target_index].get();
		}

		if (pass.viewport.Width == 0 && pass.viewport.Height == 0)
		{
			pass.viewport.Width = _runtime->frame_width();
			pass.viewport.Height = _runtime->frame_height();
		}

		D3D10_DEPTH_STENCIL_DESC ddesc = { };
		ddesc.DepthEnable = FALSE;
		ddesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
		ddesc.DepthFunc = D3D10_COMPARISON_ALWAYS;
		ddesc.StencilEnable = node->stencil_enable;
		ddesc.StencilReadMask = node->stencil_read_mask;
		ddesc.StencilWriteMask = node->stencil_write_mask;
		ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D10_COMPARISON_FUNC>(node->stencil_comparison_func);
		ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = literal_to_stencil_op(node->stencil_op_pass);
		ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = literal_to_stencil_op(node->stencil_op_fail);
		ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = literal_to_stencil_op(node->stencil_op_depth_fail);
		pass.stencil_reference = node->stencil_reference_value;

		HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.depth_stencil_state);

		if (FAILED(hr))
		{
			warning(node->location, "'ID3D10Device::CreateDepthStencilState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
		}

		D3D10_BLEND_DESC bdesc = { };
		bdesc.AlphaToCoverageEnable = FALSE;
		bdesc.RenderTargetWriteMask[0] = node->color_write_mask;
		bdesc.BlendEnable[0] = node->blend_enable;
		bdesc.BlendOp = static_cast<D3D10_BLEND_OP>(node->blend_op);
		bdesc.BlendOpAlpha = static_cast<D3D10_BLEND_OP>(node->blend_op_alpha);
		bdesc.SrcBlend = literal_to_blend_func(node->src_blend);
		bdesc.DestBlend = literal_to_blend_func(node->dest_blend);

		for (UINT i = 1; i < 8; i++)
		{
			bdesc.RenderTargetWriteMask[i] = bdesc.RenderTargetWriteMask[0];
			bdesc.BlendEnable[i] = bdesc.BlendEnable[0];
		}

		hr = _runtime->_device->CreateBlendState(&bdesc, &pass.blend_state);

		if (FAILED(hr))
		{
			warning(node->location, "'ID3D10Device::CreateBlendState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
		}

		for (auto &srv : pass.shader_resources)
		{
			if (srv == nullptr)
			{
				continue;
			}

			com_ptr<ID3D10Resource> res1;
			srv->GetResource(&res1);

			for (auto rtv : pass.render_targets)
			{
				if (rtv == nullptr)
				{
					continue;
				}

				com_ptr<ID3D10Resource> res2;
				rtv->GetResource(&res2);

				if (res1 == res2)
				{
					srv = nullptr;
					break;
				}
			}
		}
	}
	void d3d10_effect_compiler::visit_pass_shader(const function_declaration_node *node, const std::string &shadertype, d3d10_pass_data &pass)
	{
		com_ptr<ID3D10Device1> device1;
		D3D10_FEATURE_LEVEL1 featurelevel = D3D10_FEATURE_LEVEL_10_0;

		if (SUCCEEDED(_runtime->_device->QueryInterface(&device1)))
		{
			featurelevel = device1->GetFeatureLevel();
		}

		std::string profile = shadertype;

		switch (featurelevel)
		{
			case D3D10_FEATURE_LEVEL_10_1:
				profile += "_4_1";
				break;
			default:
			case D3D10_FEATURE_LEVEL_10_0:
				profile += "_4_0";
				break;
			case D3D10_FEATURE_LEVEL_9_1:
			case D3D10_FEATURE_LEVEL_9_2:
				profile += "_4_0_level_9_1";
				break;
			case D3D10_FEATURE_LEVEL_9_3:
				profile += "_4_0_level_9_3";
				break;
		}

		std::string source =
			"#pragma warning(disable: 3571)\n"
			"struct __sampler2D { Texture2D t; SamplerState s; };\n"
			"inline float4 __tex2D(__sampler2D s, float2 c) { return s.t.Sample(s.s, c); }\n"
			"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
			"inline float4 __tex2Dgrad(__sampler2D s, float2 c, float2 ddx, float2 ddy) { return s.t.SampleGrad(s.s, c, ddx, ddy); }\n"
			"inline float4 __tex2Dlod(__sampler2D s, float4 c) { return s.t.SampleLevel(s.s, c.xy, c.w); }\n"
			"inline float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return s.t.SampleLevel(s.s, c.xy, c.w, offset); }\n"
			"inline float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return s.t.Sample(s.s, c, offset); }\n"
			"inline float4 __tex2Dproj(__sampler2D s, float4 c) { return s.t.Sample(s.s, c.xy / c.w); }\n"
			"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n";

		if (featurelevel >= D3D10_FEATURE_LEVEL_10_1)
		{
			source +=
				"inline float4 __tex2Dgather0(__sampler2D s, float2 c) { return s.t.Gather(s.s, c); }\n"
				"inline float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return s.t.Gather(s.s, c, offset); }\n";
		}
		else
		{
			source +=
				"inline float4 __tex2Dgather0(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).r, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).r, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).r, s.t.SampleLevel(s.s, c, 0).r); }\n"
				"inline float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).r, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).r, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).r, s.t.SampleLevel(s.s, c, 0, offset).r); }\n";
		}

		source +=
			"inline float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0).g); }\n"
			"inline float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0, offset).g); }\n"
			"inline float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0).b); }\n"
			"inline float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0, offset).b); }\n"
			"inline float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0).a); }\n"
			"inline float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0, offset).a); }\n";

		source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + _global_uniforms.str() + "};\n";

		for (const auto &samplerdesc : _runtime->_effect_sampler_descs)
		{
			source += "SamplerState __SamplerState" + std::to_string(samplerdesc.second) + " : register(s" + std::to_string(samplerdesc.second) + ");\n";
		}

		source += _global_code.str();

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
		com_ptr<ID3DBlob> compiled, errors;

		if (_skip_shader_optimization)
		{
			flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}

		const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3dcompiler_module, "D3DCompile"));
		HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, node->unique_name.c_str(), profile.c_str(), flags, 0, &compiled, &errors);

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
			hr = _runtime->_device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.vertex_shader);
		}
		else if (shadertype == "ps")
		{
			hr = _runtime->_device->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.pixel_shader);
		}

		if (FAILED(hr))
		{
			error(node->location, "'CreateShader' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			return;
		}
	}
}
