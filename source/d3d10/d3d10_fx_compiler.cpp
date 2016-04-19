#include "log.hpp"
#include "d3d10_fx_compiler.hpp"

#include <d3dcompiler.h>

namespace reshade
{
	namespace
	{
		UINT roundto16(UINT size)
		{
			return (size + 15) & ~15;
		}
		D3D10_BLEND literal_to_blend_func(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::pass_declaration_node::states::ZERO:
					return D3D10_BLEND_ZERO;
				case fx::nodes::pass_declaration_node::states::ONE:
					return D3D10_BLEND_ONE;
			}

			return static_cast<D3D10_BLEND>(value);
		}
		D3D10_STENCIL_OP literal_to_stencil_op(unsigned int value)
		{
			if (value == fx::nodes::pass_declaration_node::states::ZERO)
			{
				return D3D10_STENCIL_OP_ZERO;
			}

			return static_cast<D3D10_STENCIL_OP>(value);
		}
		DXGI_FORMAT literal_to_format(unsigned int value, texture_format &name)
		{
			switch (value)
			{
				case fx::nodes::variable_declaration_node::properties::R8:
					name = texture_format::r8;
					return DXGI_FORMAT_R8_UNORM;
				case fx::nodes::variable_declaration_node::properties::R16F:
					name = texture_format::r16f;
					return DXGI_FORMAT_R16_FLOAT;
				case fx::nodes::variable_declaration_node::properties::R32F:
					name = texture_format::r32f;
					return DXGI_FORMAT_R32_FLOAT;
				case fx::nodes::variable_declaration_node::properties::RG8:
					name = texture_format::rg8;
					return DXGI_FORMAT_R8G8_UNORM;
				case fx::nodes::variable_declaration_node::properties::RG16:
					name = texture_format::rg16;
					return DXGI_FORMAT_R16G16_UNORM;
				case fx::nodes::variable_declaration_node::properties::RG16F:
					name = texture_format::rg16f;
					return DXGI_FORMAT_R16G16_FLOAT;
				case fx::nodes::variable_declaration_node::properties::RG32F:
					name = texture_format::rg32f;
					return DXGI_FORMAT_R32G32_FLOAT;
				case fx::nodes::variable_declaration_node::properties::RGBA8:
					name = texture_format::rgba8;
					return DXGI_FORMAT_R8G8B8A8_TYPELESS;
				case fx::nodes::variable_declaration_node::properties::RGBA16:
					name = texture_format::rgba16;
					return DXGI_FORMAT_R16G16B16A16_UNORM;
				case fx::nodes::variable_declaration_node::properties::RGBA16F:
					name = texture_format::rgba16f;
					return DXGI_FORMAT_R16G16B16A16_FLOAT;
				case fx::nodes::variable_declaration_node::properties::RGBA32F:
					name = texture_format::rgba32f;
					return DXGI_FORMAT_R32G32B32A32_FLOAT;
				case fx::nodes::variable_declaration_node::properties::DXT1:
					name = texture_format::dxt1;
					return DXGI_FORMAT_BC1_TYPELESS;
				case fx::nodes::variable_declaration_node::properties::DXT3:
					name = texture_format::dxt3;
					return DXGI_FORMAT_BC2_TYPELESS;
				case fx::nodes::variable_declaration_node::properties::DXT5:
					name = texture_format::dxt5;
					return DXGI_FORMAT_BC3_TYPELESS;
				case fx::nodes::variable_declaration_node::properties::LATC1:
					name = texture_format::latc1;
					return DXGI_FORMAT_BC4_UNORM;
				case fx::nodes::variable_declaration_node::properties::LATC2:
					name = texture_format::latc2;
					return DXGI_FORMAT_BC5_UNORM;
				default:
					name = texture_format::unknown;
					return DXGI_FORMAT_UNKNOWN;
			}
		}
		size_t D3D10_SAMPLER_DESC_HASH(const D3D10_SAMPLER_DESC &s)
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
			size_t h = 2166136261;

			for (size_t i = 0; i < sizeof(D3D10_SAMPLER_DESC); ++i)
			{
				h = (h * 16777619) ^ p[i];
			}

			return h;
		}
		std::string convert_semantic(const std::string &semantic)
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
	}

	d3d10_fx_compiler::d3d10_fx_compiler(d3d10_runtime *runtime, const fx::nodetree &ast, std::string &errors, bool skipoptimization) :
		_runtime(runtime),
		_success(true),
		_ast(ast),
		_errors(errors),
		_skip_shader_optimization(skipoptimization),
		_is_in_parameter_block(false),
		_is_in_function_block(false),
		_current_global_size(0)
	{
	}

	bool d3d10_fx_compiler::run()
	{
		for (auto node : _ast.structs)
		{
			visit(_global_code, static_cast<fx::nodes::struct_declaration_node *>(node));
		}
		for (auto node : _ast.uniforms)
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

			visit(_global_code, function);
		}
		for (auto node : _ast.techniques)
		{
			visit_technique(static_cast<fx::nodes::technique_declaration_node *>(node));
		}

		if (_current_global_size != 0)
		{
			CD3D10_BUFFER_DESC globalsDesc(roundto16(_current_global_size), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
			D3D10_SUBRESOURCE_DATA globalsInitial;
			globalsInitial.pSysMem = _runtime->get_uniform_value_storage().data();
			globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = _runtime->_constant_buffer_size = _current_global_size;
			_runtime->_device->CreateBuffer(&globalsDesc, &globalsInitial, &_runtime->_constant_buffer);
		}

		return _success;
	}

	void d3d10_fx_compiler::error(const fx::location &location, const std::string &message)
	{
		_success = false;

		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
	}
	void d3d10_fx_compiler::warning(const fx::location &location, const std::string &message)
	{
		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
	}

	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_node *node)
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

	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::type_node &type, bool with_qualifiers)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::lvalue_expression_node *node)
	{
		output << node->reference->unique_name;
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::literal_expression_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_sequence_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::unary_expression_node *node)
	{
		switch (node->op)
		{
			case fx::nodes::unary_expression_node::negate:
				output << '-';
				break;
			case fx::nodes::unary_expression_node::bitwise_not:
				output << "~";
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
			case fx::nodes::unary_expression_node::post_increase:
				output << "++";
				break;
			case fx::nodes::unary_expression_node::post_decrease:
				output << "--";
				break;
			case fx::nodes::unary_expression_node::cast:
				output << ')';
				break;
		}
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::binary_expression_node *node)
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
				part1 = "(";
				part2 = " << ";
				part3 = ")";
				break;
			case fx::nodes::binary_expression_node::right_shift:
				part1 = "(";
				part2 = " >> ";
				part3 = ")";
				break;
			case fx::nodes::binary_expression_node::bitwise_and:
				part1 = "(";
				part2 = " & ";
				part3 = ")";
				break;
			case fx::nodes::binary_expression_node::bitwise_or:
				part1 = "(";
				part2 = " | ";
				part3 = ")";
				break;
			case fx::nodes::binary_expression_node::bitwise_xor:
				part1 = "(";
				part2 = " ^ ";
				part3 = ")";
				break;
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::intrinsic_expression_node *node)
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
				part1 = "asfloat(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
				part1 = "asfloat(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::asin:
				part1 = "asin(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_float2int:
				part1 = "asint(";
				part2 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
				part1 = "asuint(";
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
			case fx::nodes::intrinsic_expression_node::tex2d:
				part1 = "__tex2D(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dfetch:
				part1 = "__tex2Dfetch(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dgather:
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
			case fx::nodes::intrinsic_expression_node::tex2dgatheroffset:
				if (node->arguments[3]->id == fx::nodeid::literal_expression && node->arguments[3]->type.is_integral())
				{
					const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[3])->value_int[0];

					output << "__tex2Dgatheroffset" << component << '(';
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
			case fx::nodes::intrinsic_expression_node::tex2dgrad:
				part1 = "__tex2Dgrad(";
				part2 = ", ";
				part3 = ", ";
				part4 = ", ";
				part5 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dlevel:
				part1 = "__tex2Dlod(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dleveloffset:
				part1 = "__tex2Dlodoffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2doffset:
				part1 = "__tex2Doffset(";
				part2 = ", ";
				part3 = ", ";
				part4 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dproj:
				part1 = "__tex2Dproj(";
				part2 = ", ";
				part3 = ")";
				break;
			case fx::nodes::intrinsic_expression_node::tex2dsize:
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::conditional_expression_node *node)
	{
		output << '(';
		visit(output, node->condition);
		output << " ? ";
		visit(output, node->expression_when_true);
		output << " : ";
		visit(output, node->expression_when_false);
		output << ')';
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::swizzle_expression_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::field_expression_node *node)
	{
		output << '(';

		visit(output, node->operand);

		output << '.' << node->field_reference->unique_name << ')';
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::assignment_expression_node *node)
	{
		output << '(';
		visit(output, node->left);
		output << ' ';

		switch (node->op)
		{
			case fx::nodes::assignment_expression_node::none:
				output << '=';
				break;
			case fx::nodes::assignment_expression_node::add:
				output << "+=";
				break;
			case fx::nodes::assignment_expression_node::subtract:
				output << "-=";
				break;
			case fx::nodes::assignment_expression_node::multiply:
				output << "*=";
				break;
			case fx::nodes::assignment_expression_node::divide:
				output << "/=";
				break;
			case fx::nodes::assignment_expression_node::modulo:
				output << "%=";
				break;
			case fx::nodes::assignment_expression_node::left_shift:
				output << "<<=";
				break;
			case fx::nodes::assignment_expression_node::right_shift:
				output << ">>=";
				break;
			case fx::nodes::assignment_expression_node::bitwise_and:
				output << "&=";
				break;
			case fx::nodes::assignment_expression_node::bitwise_or:
				output << "|=";
				break;
			case fx::nodes::assignment_expression_node::bitwise_xor:
				output << "^=";
				break;
		}

		output << ' ';
		visit(output, node->right);
		output << ')';
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::call_expression_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::constructor_expression_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::initializer_list_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::compound_statement_node *node)
	{
		output << "{\n";

		for (auto statement : node->statement_list)
		{
			visit(output, statement);
		}

		output << "}\n";
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::declarator_list_node *node, bool single_statement)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_statement_node *node)
	{
		visit(output, node->expression);

		output << ";\n";
	}
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::if_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::switch_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::case_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::for_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::while_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::return_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::jump_statement_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::struct_declaration_node *node)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type)
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
	void d3d10_fx_compiler::visit(std::stringstream &output, const fx::nodes::function_declaration_node *node)
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

	void d3d10_fx_compiler::visit_texture(const fx::nodes::variable_declaration_node *node)
	{
		const auto obj = new d3d10_texture();
		D3D10_TEXTURE2D_DESC texdesc = { };
		obj->name = node->name;
		obj->shader_register = _runtime->_effect_shader_resources.size();
		texdesc.Width = obj->width = node->properties.Width;
		texdesc.Height = obj->height = node->properties.Height;
		texdesc.MipLevels = obj->levels = node->properties.MipLevels;
		texdesc.ArraySize = 1;
		texdesc.Format = literal_to_format(node->properties.Format, obj->format);
		texdesc.SampleDesc.Count = 1;
		texdesc.SampleDesc.Quality = 0;
		texdesc.Usage = D3D10_USAGE_DEFAULT;
		texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
		texdesc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

		visit_annotation(node->annotations, *obj);

		if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
		{
			if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
			{
				warning(node->location, "texture properties on backbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(*obj, texture_type::backbuffer, _runtime->_backbuffer_texture_srv[0], _runtime->_backbuffer_texture_srv[1]);
		}
		else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
		{
			if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
			{
				warning(node->location, "texture properties on depthbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(*obj, texture_type::depthbuffer, _runtime->_depthstencil_texture_srv, nullptr);
		}
		else
		{
			if (texdesc.MipLevels == 0)
			{
				warning(node->location, "a texture cannot have 0 miplevels, changed it to 1");

				texdesc.MipLevels = 1;
			}

			HRESULT hr = _runtime->_device->CreateTexture2D(&texdesc, nullptr, &obj->texture);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateTexture2D' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc = { };
			srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
			srvdesc.Format = make_format_normal(texdesc.Format);

			hr = _runtime->_device->CreateShaderResourceView(obj->texture.get(), &srvdesc, &obj->srv[0]);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			srvdesc.Format = make_format_srgb(texdesc.Format);

			if (srvdesc.Format != texdesc.Format)
			{
				hr = _runtime->_device->CreateShaderResourceView(obj->texture.get(), &srvdesc, &obj->srv[1]);

				if (FAILED(hr))
				{
					error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}
			}
		}

		_global_code << "Texture2D " <<
			node->unique_name << " : register(t" << _runtime->_effect_shader_resources.size() << "), __" <<
			node->unique_name << "SRGB : register(t" << (_runtime->_effect_shader_resources.size() + 1) << ");\n";

		_runtime->_effect_shader_resources.push_back(obj->srv[0].get());
		_runtime->_effect_shader_resources.push_back(obj->srv[1].get());

		_runtime->add_texture(obj);
	}
	void d3d10_fx_compiler::visit_sampler(const fx::nodes::variable_declaration_node *node)
	{
		if (node->properties.Texture == nullptr)
		{
			error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
			return;
		}

		D3D10_SAMPLER_DESC desc;
		desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressU);
		desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressV);
		desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressW);
		desc.MipLODBias = node->properties.MipLODBias;
		desc.MinLOD = node->properties.MinLOD;
		desc.MaxLOD = node->properties.MaxLOD;
		desc.MaxAnisotropy = node->properties.MaxAnisotropy;
		desc.ComparisonFunc = D3D10_COMPARISON_NEVER;

		const UINT minfilter = node->properties.MinFilter;
		const UINT magfilter = node->properties.MagFilter;
		const UINT mipfilter = node->properties.MipFilter;

		if (minfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || magfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || mipfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC)
		{
			desc.Filter = D3D10_FILTER_ANISOTROPIC;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
		{
			desc.Filter = D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
		{
			desc.Filter = D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
		{
			desc.Filter = D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
		{
			desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
		{
			desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
		{
			desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		}
		else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
		{
			desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else
		{
			desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
		}

		const auto texture = static_cast<d3d10_texture *>(_runtime->find_texture(node->properties.Texture->name));

		if (texture == nullptr)
		{
			error(node->location, "texture '" + node->properties.Texture->name + "' for sampler '" + node->name + "' is missing due to previous error");
			return;
		}

		const size_t descHash = D3D10_SAMPLER_DESC_HASH(desc);
		auto it = _sampler_descs.find(descHash);

		if (it == _sampler_descs.end())
		{
			ID3D10SamplerState *sampler = nullptr;

			HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

			if (FAILED(hr))
			{
				error(node->location, "'ID3D10Device::CreateSamplerState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			_runtime->_effect_sampler_states.push_back(sampler);
			it = _sampler_descs.emplace(descHash, _runtime->_effect_sampler_states.size() - 1).first;

			_global_code << "SamplerState __SamplerState" << it->second << " : register(s" << it->second << ");\n";
		}

		_global_code << "static const __sampler2D " << node->unique_name << " = { ";

		if (node->properties.SRGBTexture && texture->srv[1] != nullptr)
		{
			_global_code << "__" << node->properties.Texture->unique_name << "SRGB";
		}
		else
		{
			_global_code << node->properties.Texture->unique_name;
		}

		_global_code << ", __SamplerState" << it->second << " };\n";
	}
	void d3d10_fx_compiler::visit_uniform(const fx::nodes::variable_declaration_node *node)
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

		const auto obj = new uniform();
		obj->name = node->name;
		obj->rows = node->type.rows;
		obj->columns = node->type.cols;
		obj->elements = node->type.array_length;
		obj->storage_size = node->type.rows * node->type.cols;

		switch (node->type.basetype)
		{
			case fx::nodes::type_node::datatype_bool:
				obj->basetype = uniform_datatype::bool_;
				obj->storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_int:
				obj->basetype = uniform_datatype::int_;
				obj->storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_uint:
				obj->basetype = uniform_datatype::uint_;
				obj->storage_size *= sizeof(unsigned int);
				break;
			case fx::nodes::type_node::datatype_float:
				obj->basetype = uniform_datatype::float_;
				obj->storage_size *= sizeof(float);
				break;
		}

		const UINT alignment = 16 - (_current_global_size % 16);
		_current_global_size += static_cast<UINT>((obj->storage_size > alignment && (alignment != 16 || obj->storage_size <= 16)) ? obj->storage_size + alignment : obj->storage_size);
		obj->storage_offset = _current_global_size - obj->storage_size;

		visit_annotation(node->annotations, *obj);

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (_current_global_size >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 128);
		}

		if (node->initializer_expression != nullptr && node->initializer_expression->id == fx::nodeid::literal_expression)
		{
			CopyMemory(uniform_storage.data() + obj->storage_offset, &static_cast<const fx::nodes::literal_expression_node *>(node->initializer_expression)->value_float, obj->storage_size);
		}
		else
		{
			ZeroMemory(uniform_storage.data() + obj->storage_offset, obj->storage_size);
		}

		_runtime->add_uniform(obj);
	}
	void d3d10_fx_compiler::visit_technique(const fx::nodes::technique_declaration_node *node)
	{
		const auto obj = new technique();
		obj->name = node->name;

		visit_annotation(node->annotation_list, *obj);

		for (auto pass : node->pass_list)
		{
			obj->passes.emplace_back(new d3d10_pass());
			visit_pass(pass, *static_cast<d3d10_pass *>(obj->passes.back().get()));
		}

		_runtime->add_technique(obj);
	}
	void d3d10_fx_compiler::visit_pass(const fx::nodes::pass_declaration_node *node, d3d10_pass &pass)
	{
		pass.stencil_reference = 0;
		pass.viewport.TopLeftX = pass.viewport.TopLeftY = pass.viewport.Width = pass.viewport.Height = 0;
		pass.viewport.MinDepth = 0.0f;
		pass.viewport.MaxDepth = 1.0f;
		ZeroMemory(pass.render_targets, sizeof(pass.render_targets));
		ZeroMemory(pass.render_target_resources, sizeof(pass.render_target_resources));
		pass.shader_resources = _runtime->_effect_shader_resources;

		if (node->states.VertexShader != nullptr)
		{
			visit_pass_shader(node->states.VertexShader, "vs", pass);
		}
		if (node->states.PixelShader != nullptr)
		{
			visit_pass_shader(node->states.PixelShader, "ps", pass);
		}

		const int target_index = node->states.SRGBWriteEnable ? 1 : 0;
		pass.render_targets[0] = _runtime->_backbuffer_rtv[target_index].get();
		pass.render_target_resources[0] = _runtime->_backbuffer_texture_srv[target_index].get();

		for (unsigned int i = 0; i < 8; i++)
		{
			if (node->states.RenderTargets[i] == nullptr)
			{
				continue;
			}

			const auto texture = static_cast<d3d10_texture *>(_runtime->find_texture(node->states.RenderTargets[i]->name));

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
				return;
			}

			D3D10_TEXTURE2D_DESC texture_desc;
			texture->texture->GetDesc(&texture_desc);

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
			rtvdesc.Format = node->states.SRGBWriteEnable ? make_format_srgb(texture_desc.Format) : make_format_normal(texture_desc.Format);
			rtvdesc.ViewDimension = texture_desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

			if (texture->rtv[target_index] == nullptr)
			{
				const HRESULT hr = _runtime->_device->CreateRenderTargetView(texture->texture.get(), &rtvdesc, &texture->rtv[target_index]);

				if (FAILED(hr))
				{
					warning(node->location, "'CreateRenderTargetView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				}
			}

			pass.render_targets[i] = texture->rtv[target_index].get();
			pass.render_target_resources[i] = texture->srv[target_index].get();
		}

		if (pass.viewport.Width == 0 && pass.viewport.Height == 0)
		{
			pass.viewport.Width = _runtime->frame_width();
			pass.viewport.Height = _runtime->frame_height();
		}

		D3D10_DEPTH_STENCIL_DESC ddesc = { };
		ddesc.DepthEnable = node->states.DepthEnable;
		ddesc.DepthWriteMask = node->states.DepthWriteMask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
		ddesc.DepthFunc = static_cast<D3D10_COMPARISON_FUNC>(node->states.DepthFunc);
		ddesc.StencilEnable = node->states.StencilEnable;
		ddesc.StencilReadMask = node->states.StencilReadMask;
		ddesc.StencilWriteMask = node->states.StencilWriteMask;
		ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D10_COMPARISON_FUNC>(node->states.StencilFunc);
		ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = literal_to_stencil_op(node->states.StencilOpPass);
		ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = literal_to_stencil_op(node->states.StencilOpFail);
		ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = literal_to_stencil_op(node->states.StencilOpDepthFail);
		pass.stencil_reference = node->states.StencilRef;

		HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.depth_stencil_state);

		if (FAILED(hr))
		{
			warning(node->location, "'ID3D10Device::CreateDepthStencilState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
		}

		D3D10_BLEND_DESC bdesc = { };
		bdesc.AlphaToCoverageEnable = FALSE;
		bdesc.RenderTargetWriteMask[0] = node->states.RenderTargetWriteMask;
		bdesc.BlendEnable[0] = node->states.BlendEnable;
		bdesc.BlendOp = static_cast<D3D10_BLEND_OP>(node->states.BlendOp);
		bdesc.BlendOpAlpha = static_cast<D3D10_BLEND_OP>(node->states.BlendOpAlpha);
		bdesc.SrcBlend = literal_to_blend_func(node->states.SrcBlend);
		bdesc.DestBlend = literal_to_blend_func(node->states.DestBlend);

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
	void d3d10_fx_compiler::visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, d3d10_pass &pass)
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
		source += _global_code.str();

		LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source.c_str() << "\n";

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
		com_ptr<ID3DBlob> compiled, errors;

		if (_skip_shader_optimization)
		{
			flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		}

		HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, node->unique_name.c_str(), profile.c_str(), flags, 0, &compiled, &errors);

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
