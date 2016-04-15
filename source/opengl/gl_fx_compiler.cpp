#include "log.hpp"
#include "gl_fx_compiler.hpp"

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

namespace reshade
{
	namespace
	{
		GLenum literal_to_comp_func(unsigned int value)
		{
			switch (value)
			{
				default:
				case fx::nodes::pass_declaration_node::states::ALWAYS:
					return GL_ALWAYS;
				case fx::nodes::pass_declaration_node::states::NEVER:
					return GL_NEVER;
				case fx::nodes::pass_declaration_node::states::EQUAL:
					return GL_EQUAL;
				case fx::nodes::pass_declaration_node::states::NOTEQUAL:
					return GL_NOTEQUAL;
				case fx::nodes::pass_declaration_node::states::LESS:
					return GL_LESS;
				case fx::nodes::pass_declaration_node::states::LESSEQUAL:
					return GL_LEQUAL;
				case fx::nodes::pass_declaration_node::states::GREATER:
					return GL_GREATER;
				case fx::nodes::pass_declaration_node::states::GREATEREQUAL:
					return GL_GEQUAL;
			}
		}
		GLenum literal_to_blend_eq(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::pass_declaration_node::states::ADD:
					return GL_FUNC_ADD;
				case fx::nodes::pass_declaration_node::states::SUBTRACT:
					return GL_FUNC_SUBTRACT;
				case fx::nodes::pass_declaration_node::states::REVSUBTRACT:
					return GL_FUNC_REVERSE_SUBTRACT;
				case fx::nodes::pass_declaration_node::states::MIN:
					return GL_MIN;
				case fx::nodes::pass_declaration_node::states::MAX:
					return GL_MAX;
			}

			return GL_NONE;
		}
		GLenum literal_to_blend_func(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::pass_declaration_node::states::ZERO:
					return GL_ZERO;
				case fx::nodes::pass_declaration_node::states::ONE:
					return GL_ONE;
				case fx::nodes::pass_declaration_node::states::SRCCOLOR:
					return GL_SRC_COLOR;
				case fx::nodes::pass_declaration_node::states::SRCALPHA:
					return GL_SRC_ALPHA;
				case fx::nodes::pass_declaration_node::states::INVSRCCOLOR:
					return GL_ONE_MINUS_SRC_COLOR;
				case fx::nodes::pass_declaration_node::states::INVSRCALPHA:
					return GL_ONE_MINUS_SRC_ALPHA;
				case fx::nodes::pass_declaration_node::states::DESTCOLOR:
					return GL_DST_COLOR;
				case fx::nodes::pass_declaration_node::states::DESTALPHA:
					return GL_DST_ALPHA;
				case fx::nodes::pass_declaration_node::states::INVDESTCOLOR:
					return GL_ONE_MINUS_DST_COLOR;
				case fx::nodes::pass_declaration_node::states::INVDESTALPHA:
					return GL_ONE_MINUS_DST_ALPHA;
			}

			return GL_NONE;
		}
		GLenum literal_to_stencil_op(unsigned int value)
		{
			switch (value)
			{
				default:
				case fx::nodes::pass_declaration_node::states::KEEP:
					return GL_KEEP;
				case fx::nodes::pass_declaration_node::states::ZERO:
					return GL_ZERO;
				case fx::nodes::pass_declaration_node::states::REPLACE:
					return GL_REPLACE;
				case fx::nodes::pass_declaration_node::states::INCR:
					return GL_INCR_WRAP;
				case fx::nodes::pass_declaration_node::states::INCRSAT:
					return GL_INCR;
				case fx::nodes::pass_declaration_node::states::DECR:
					return GL_DECR_WRAP;
				case fx::nodes::pass_declaration_node::states::DECRSAT:
					return GL_DECR;
				case fx::nodes::pass_declaration_node::states::INVERT:
					return GL_INVERT;
			}
		}
		GLenum literal_to_wrap_mode(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::variable_declaration_node::properties::REPEAT:
					return GL_REPEAT;
				case fx::nodes::variable_declaration_node::properties::MIRROR:
					return GL_MIRRORED_REPEAT;
				case fx::nodes::variable_declaration_node::properties::CLAMP:
					return GL_CLAMP_TO_EDGE;
				case fx::nodes::variable_declaration_node::properties::BORDER:
					return GL_CLAMP_TO_BORDER;
			}

			return GL_NONE;
		}
		GLenum literal_to_filter_mode(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::variable_declaration_node::properties::POINT:
					return GL_NEAREST;
				case fx::nodes::variable_declaration_node::properties::LINEAR:
					return GL_LINEAR;
				case fx::nodes::variable_declaration_node::properties::ANISOTROPIC:
					return GL_LINEAR_MIPMAP_LINEAR;
			}

			return GL_NONE;
		}
		void literal_to_format(unsigned int value, GLenum &internalformat, GLenum &internalformatsrgb, texture::pixelformat &name)
		{
			switch (value)
			{
				case fx::nodes::variable_declaration_node::properties::R8:
					name = texture::pixelformat::r8;
					internalformat = internalformatsrgb = GL_R8;
					break;
				case fx::nodes::variable_declaration_node::properties::R16F:
					name = texture::pixelformat::r16f;
					internalformat = internalformatsrgb = GL_R16F;
					break;
				case fx::nodes::variable_declaration_node::properties::R32F:
					name = texture::pixelformat::r32f;
					internalformat = internalformatsrgb = GL_R32F;
					break;
				case fx::nodes::variable_declaration_node::properties::RG8:
					name = texture::pixelformat::rg8;
					internalformat = internalformatsrgb = GL_RG8;
					break;
				case fx::nodes::variable_declaration_node::properties::RG16:
					name = texture::pixelformat::rg16;
					internalformat = internalformatsrgb = GL_RG16;
					break;
				case fx::nodes::variable_declaration_node::properties::RG16F:
					name = texture::pixelformat::rg16f;
					internalformat = internalformatsrgb = GL_RG16F;
					break;
				case fx::nodes::variable_declaration_node::properties::RG32F:
					name = texture::pixelformat::rg32f;
					internalformat = internalformatsrgb = GL_RG32F;
					break;
				case fx::nodes::variable_declaration_node::properties::RGBA8:
					name = texture::pixelformat::rgba8;
					internalformat = GL_RGBA8;
					internalformatsrgb = GL_SRGB8_ALPHA8;
					break;
				case fx::nodes::variable_declaration_node::properties::RGBA16:
					name = texture::pixelformat::rgba16;
					internalformat = internalformatsrgb = GL_RGBA16;
					break;
				case fx::nodes::variable_declaration_node::properties::RGBA16F:
					name = texture::pixelformat::rgba16f;
					internalformat = internalformatsrgb = GL_RGBA16F;
					break;
				case fx::nodes::variable_declaration_node::properties::RGBA32F:
					name = texture::pixelformat::rgba32f;
					internalformat = internalformatsrgb = GL_RGBA32F;
					break;
				case fx::nodes::variable_declaration_node::properties::DXT1:
					name = texture::pixelformat::dxt1;
					internalformat = 0x83F1; // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
					internalformatsrgb = 0x8C4D; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
					break;
				case fx::nodes::variable_declaration_node::properties::DXT3:
					name = texture::pixelformat::dxt3;
					internalformat = 0x83F2; // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
					internalformatsrgb = 0x8C4E; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
					break;
				case fx::nodes::variable_declaration_node::properties::DXT5:
					name = texture::pixelformat::dxt5;
					internalformat = 0x83F3; // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
					internalformatsrgb = 0x8C4F; // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
					break;
				case fx::nodes::variable_declaration_node::properties::LATC1:
					name = texture::pixelformat::latc1;
					internalformat = internalformatsrgb = 0x8C70; // GL_COMPRESSED_LUMINANCE_LATC1_EXT
					break;
				case fx::nodes::variable_declaration_node::properties::LATC2:
					name = texture::pixelformat::latc2;
					internalformat = internalformatsrgb = 0x8C72; // GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT
					break;
				default:
					name = texture::pixelformat::unknown;
					internalformat = internalformatsrgb = GL_NONE;
					break;
			}
		}
		std::string escape_name(const std::string &name)
		{
			std::string res;

			if (name.compare(0, 3, "gl_") == 0 ||
				name == "common" || name == "partition" || name == "input" || name == "ouput" || name == "active" || name == "filter" || name == "superp" ||
				name == "invariant" || name == "lowp" || name == "mediump" || name == "highp" || name == "precision" || name == "patch" || name == "subroutine" ||
				name == "abs" || name == "sign" || name == "all" || name == "any" || name == "sin" || name == "sinh" || name == "cos" || name == "cosh" || name == "tan" || name == "tanh" || name == "asin" || name == "acos" || name == "atan" || name == "exp" || name == "exp2" || name == "log" || name == "log2" || name == "sqrt" || name == "inversesqrt" || name == "ceil" || name == "floor" || name == "fract" || name == "trunc" || name == "round" || name == "radians" || name == "degrees" || name == "length" || name == "normalize" || name == "transpose" || name == "determinant" || name == "intBitsToFloat" || name == "uintBitsToFloat" || name == "floatBitsToInt" || name == "floatBitsToUint" || name == "matrixCompMult" || name == "not" || name == "lessThan" || name == "greaterThan" || name == "lessThanEqual" || name == "greaterThanEqual" || name == "equal" || name == "notEqual" || name == "dot" || name == "cross" || name == "distance" || name == "pow" || name == "modf" || name == "frexp" || name == "ldexp" || name == "min" || name == "max" || name == "step" || name == "reflect" || name == "texture" || name == "textureOffset" || name == "fma" || name == "mix" || name == "clamp" || name == "smoothstep" || name == "refract" || name == "faceforward" || name == "textureLod" || name == "textureLodOffset" || name == "texelFetch" || name == "main")
			{
				res += '_';
			}

			res += name;

			size_t p;

			while ((p = res.find("__")) != std::string::npos)
			{
				res.replace(p, 2, "_US");
			}

			return res;
		}
		std::string escape_name_with_builtins(const std::string &name, const std::string &semantic, GLuint shadertype)
		{
			if (semantic == "SV_VERTEXID" || semantic == "VERTEXID")
			{
				return "gl_VertexID";
			}
			else if (semantic == "SV_INSTANCEID")
			{
				return "gl_InstanceID";
			}
			else if ((semantic == "SV_POSITION" || semantic == "POSITION") && shadertype == GL_VERTEX_SHADER)
			{
				return "gl_Position";
			}
			else if ((semantic == "SV_POSITION" || semantic == "VPOS") && shadertype == GL_FRAGMENT_SHADER)
			{
				return "gl_FragCoord";
			}
			else if ((semantic == "SV_DEPTH" || semantic == "DEPTH") && shadertype == GL_FRAGMENT_SHADER)
			{
				return "gl_FragDepth";
			}

			return escape_name(name);
		}
		std::pair<std::string, std::string> write_cast(const fx::nodes::type_node &from, const fx::nodes::type_node &to)
		{
			std::pair<std::string, std::string> code;

			if (from.basetype != to.basetype && !(from.is_matrix() && to.is_matrix()))
			{
				const fx::nodes::type_node type = { to.basetype, 0, from.rows, from.cols, 0, to.definition };

				switch (type.basetype)
				{
					case fx::nodes::type_node::datatype_bool:
						if (type.is_matrix())
							code.first += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
						else if (type.is_vector())
							code.first += "bvec" + std::to_string(type.rows);
						else
							code.first += "bool";
						break;
					case fx::nodes::type_node::datatype_int:
						if (type.is_matrix())
							code.first += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
						else if (type.is_vector())
							code.first += "ivec" + std::to_string(type.rows);
						else
							code.first += "int";
						break;
					case fx::nodes::type_node::datatype_uint:
						if (type.is_matrix())
							code.first += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
						else if (type.is_vector())
							code.first += "uvec" + std::to_string(type.rows);
						else
							code.first += "uint";
						break;
					case fx::nodes::type_node::datatype_float:
						if (type.is_matrix())
							code.first += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
						else if (type.is_vector())
							code.first += "vec" + std::to_string(type.rows);
						else
							code.first += "float";
						break;
					case fx::nodes::type_node::datatype_sampler:
						code.first += "sampler2D";
						break;
					case fx::nodes::type_node::datatype_struct:
						code.first += escape_name(type.definition->unique_name);
						break;
				}

				code.first += '(';
				code.second += ')';
			}

			if (from.rows > 0 && from.rows < to.rows)
			{
				const char subscript[4] = { 'x', 'y', 'z', 'w' };

				code.second += '.';

				for (unsigned int i = 0; i < from.rows; ++i)
				{
					code.second += subscript[i];
				}
				for (unsigned int i = from.rows; i < to.rows; ++i)
				{
					code.second += subscript[from.rows - 1];
				}
			}
			else if (from.rows > to.rows)
			{
				const char subscript[4] = { 'x', 'y', 'z', 'w' };

				code.second += '.';

				for (unsigned int i = 0; i < to.rows; ++i)
				{
					code.second += subscript[i];
				}
			}

			return code;
		}
	}

	gl_fx_compiler::gl_fx_compiler(gl_runtime *runtime, const fx::nodetree &ast, std::string &errors) :
		_runtime(runtime),
		_success(true),
		_ast(ast),
		_errors(errors),
		_current_function(nullptr),
		_current_uniform_size(0)
	{
	}

	bool gl_fx_compiler::run()
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
			std::stringstream function_code;

			_functions[_current_function = function];

			visit(function_code, function);

			_functions[function].code = function_code.str();
		}

		for (auto node : _ast.techniques)
		{
			visit_technique(static_cast<fx::nodes::technique_declaration_node *>(node));
		}

		if (_current_uniform_size != 0)
		{
			glGenBuffers(1, &_runtime->_effect_ubo);

			GLint previous = 0;
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

			glBindBuffer(GL_UNIFORM_BUFFER, _runtime->_effect_ubo);
			glBufferData(GL_UNIFORM_BUFFER, _runtime->get_uniform_value_storage().size(), _runtime->get_uniform_value_storage().data(), GL_DYNAMIC_DRAW);

			glBindBuffer(GL_UNIFORM_BUFFER, previous);
		}

		return _success;
	}

	void gl_fx_compiler::error(const fx::location &location, const std::string &message)
	{
		_success = false;

		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
	}
	void gl_fx_compiler::warning(const fx::location &location, const std::string &message)
	{
		_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
	}

	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::statement_node *node)
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_node *node)
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

	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::type_node &type, bool with_qualifiers)
	{
		if (with_qualifiers)
		{
			if (type.has_qualifier(fx::nodes::type_node::qualifier_linear))
				output << "smooth ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_noperspective))
				output << "noperspective ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_centroid))
				output << "centroid ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_nointerpolation))
				output << "flat ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_inout))
				output << "inout ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_in))
				output << "in ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_out))
				output << "out ";
			else if (type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
				output << "uniform ";
			if (type.has_qualifier(fx::nodes::type_node::qualifier_const))
				output << "const ";
		}

		switch (type.basetype)
		{
			case fx::nodes::type_node::datatype_void:
				output << "void";
				break;
			case fx::nodes::type_node::datatype_bool:
				if (type.is_matrix())
					output << "mat" << type.rows << 'x' << type.cols;
				else if (type.is_vector())
					output << "bvec" << type.rows;
				else
					output << "bool";
				break;
			case fx::nodes::type_node::datatype_int:
				if (type.is_matrix())
					output << "mat" << type.rows << 'x' << type.cols;
				else if (type.is_vector())
					output << "ivec" << type.rows;
				else
					output << "int";
				break;
			case fx::nodes::type_node::datatype_uint:
				if (type.is_matrix())
					output << "mat" << type.rows << 'x' << type.cols;
				else if (type.is_vector())
					output << "uvec" << type.rows;
				else
					output << "uint";
				break;
			case fx::nodes::type_node::datatype_float:
				if (type.is_matrix())
					output << "mat" << type.rows << 'x' << type.cols;
				else if (type.is_vector())
					output << "vec" << type.rows;
				else
					output << "float";
				break;
			case fx::nodes::type_node::datatype_sampler:
				output << "sampler2D";
				break;
			case fx::nodes::type_node::datatype_struct:
				output << escape_name(type.definition->unique_name);
				break;
		}
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::lvalue_expression_node *node)
	{
		output << escape_name(node->reference->unique_name);
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::literal_expression_node *node)
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
					output << node->value_uint[i] << 'u';
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_sequence_node *node)
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::unary_expression_node *node)
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
				if (node->type.is_vector())
				{
					const auto cast = write_cast(node->operand->type, node->type);

					output << "not(" + cast.first;
				}
				else
				{
					output << "!bool(";
				}
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
			case fx::nodes::unary_expression_node::logical_not:
				if (node->type.is_vector())
				{
					const auto cast = write_cast(node->operand->type, node->type);

					output << cast.second;
				}
				// fall through
			case fx::nodes::unary_expression_node::cast:
				output << ')';
				break;
		}
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::binary_expression_node *node)
	{
		const auto type1 = node->operands[0]->type;
		const auto type2 = node->operands[1]->type;
		auto type12 = type2.is_floating_point() ? type2 : type1;
		type12.rows = std::max(type1.rows, type2.rows);
		type12.cols = std::max(type1.cols, type2.cols);

		const auto cast1 = write_cast(type1, node->type), cast2 = write_cast(type2, node->type);
		const auto cast121 = write_cast(type1, type12), cast122 = write_cast(type2, type12);

		std::string part1, part2, part3;

		switch (node->op)
		{
			case fx::nodes::binary_expression_node::add:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " + " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::subtract:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " - " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::multiply:
				if (node->type.is_matrix())
				{
					part1 = "matrixCompMult(" + cast1.first;
					part2 = cast1.second + ", " + cast2.first;
					part3 = cast2.second + ')';
				}
				else
				{
					part1 = '(' + cast1.first;
					part2 = cast1.second + " * " + cast2.first;
					part3 = cast2.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::divide:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " / " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::modulo:
				if (node->type.is_floating_point())
				{
					part1 = "_fmod(" + cast1.first;
					part2 = cast1.second + ", " + cast2.first;
					part3 = cast2.second + ')';
				}
				else
				{
					part1 = '(' + cast1.first;
					part2 = cast1.second + " % " + cast2.first;
					part3 = cast2.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::less:
				if (node->type.is_vector())
				{
					part1 = "lessThan(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ')';
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " < " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::greater:
				if (node->type.is_vector())
				{
					part1 = "greaterThan(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ')';
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " > " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::less_equal:
				if (node->type.is_vector())
				{
					part1 = "lessThanEqual(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ')';
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " <= " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::greater_equal:
				if (node->type.is_vector())
				{
					part1 = "greaterThanEqual(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ')';
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " >= " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::equal:
				if (node->type.is_vector())
				{
					part1 = "equal(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ")";
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " == " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::not_equal:
				if (node->type.is_vector())
				{
					part1 = "notEqual(" + cast121.first;
					part2 = cast121.second + ", " + cast122.first;
					part3 = cast122.second + ")";
				}
				else
				{
					part1 = '(' + cast121.first;
					part2 = cast121.second + " != " + cast122.first;
					part3 = cast122.second + ')';
				}
				break;
			case fx::nodes::binary_expression_node::left_shift:
				part1 = '(';
				part2 = " << ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::right_shift:
				part1 = '(';
				part2 = " >> ";
				part3 = ')';
				break;
			case fx::nodes::binary_expression_node::bitwise_and:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " & " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::bitwise_or:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " | " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::bitwise_xor:
				part1 = '(' + cast1.first;
				part2 = cast1.second + " ^ " + cast2.first;
				part3 = cast2.second + ')';
				break;
			case fx::nodes::binary_expression_node::logical_and:
				part1 = '(' + cast121.first;
				part2 = cast121.second + " && " + cast122.first;
				part3 = cast122.second + ')';
				break;
			case fx::nodes::binary_expression_node::logical_or:
				part1 = '(' + cast121.first;
				part2 = cast121.second + " || " + cast122.first;
				part3 = cast122.second + ')';
				break;
			case fx::nodes::binary_expression_node::element_extract:
				if (type2.basetype != fx::nodes::type_node::datatype_uint)
				{
					part2 = "[uint(";
					part3 = ")]";
				}
				else
				{
					part2 = '[';
					part3 = ']';
				}
				break;
		}

		output << part1;
		visit(output, node->operands[0]);
		output << part2;
		visit(output, node->operands[1]);
		output << part3;
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::intrinsic_expression_node *node)
	{
		fx::nodes::type_node type1 = { fx::nodes::type_node::datatype_void }, type2, type3, type4, type12;
		std::pair<std::string, std::string> cast1, cast2, cast3, cast4, cast121, cast122;

		if (node->arguments[0] != nullptr)
		{
			cast1 = write_cast(type1 = node->arguments[0]->type, node->type);
		}
		if (node->arguments[1] != nullptr)
		{
			cast2 = write_cast(type2 = node->arguments[1]->type, node->type);

			type12 = type2.is_floating_point() ? type2 : type1;
			type12.rows = std::max(type1.rows, type2.rows);
			type12.cols = std::max(type1.cols, type2.cols);

			cast121 = write_cast(type1, type12);
			cast122 = write_cast(type2, type12);
		}
		if (node->arguments[2] != nullptr)
		{
			cast3 = write_cast(type3 = node->arguments[2]->type, node->type);
		}
		if (node->arguments[3] != nullptr)
		{
			cast4 = write_cast(type4 = node->arguments[2]->type, node->type);
		}

		switch (node->op)
		{
			case fx::nodes::intrinsic_expression_node::abs:
				output << "abs(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::acos:
				output << "acos(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::all:
				if (type1.is_vector())
				{
					output << "all(bvec" << type1.rows << '(';
					visit(output, node->arguments[0]);
					output << "))";
				}
				else
				{
					output << "bool(";
					visit(output, node->arguments[0]);
					output << ')';
				}
				break;
			case fx::nodes::intrinsic_expression_node::any:
				if (type1.is_vector())
				{
					output << "any(bvec" << type1.rows << '(';
					visit(output, node->arguments[0]);
					output << "))";
				}
				else
				{
					output << "bool(";
					visit(output, node->arguments[0]);
					output << ')';
				}
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_int2float:
				output << "intBitsToFloat(";

				if (type1.basetype != fx::nodes::type_node::datatype_int)
				{
					type1.basetype = fx::nodes::type_node::datatype_int;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (type1.basetype != fx::nodes::type_node::datatype_int)
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
				output << "uintBitsToFloat(";

				if (type1.basetype != fx::nodes::type_node::datatype_uint)
				{
					type1.basetype = fx::nodes::type_node::datatype_uint;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (type1.basetype != fx::nodes::type_node::datatype_uint)
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::asin:
				output << "asin(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_float2int:
				output << "floatBitsToInt(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
				output << "floatBitsToUint(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::atan:
				output << "atan(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::atan2:
				output << "atan(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::ceil:
				output << "ceil(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::clamp:
				output << "clamp(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ", " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::cos:
				output << "cos(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::cosh:
				output << "cosh(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::cross:
				output << "cross(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::ddx:
				output << "dFdx(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::ddy:
				output << "dFdy(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::degrees:
				output << "degrees(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::determinant:
				output << "determinant(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::distance:
				output << "distance(" << cast121.first;
				visit(output, node->arguments[0]);
				output << cast121.second << ", " << cast122.first;
				visit(output, node->arguments[1]);
				output << cast122.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::dot:
				output << "dot(" << cast121.first;
				visit(output, node->arguments[0]);
				output << cast121.second << ", " << cast122.first;
				visit(output, node->arguments[1]);
				output << cast122.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::exp:
				output << "exp(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::exp2:
				output << "exp2(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::faceforward:
				output << "faceforward(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ", " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::floor:
				output << "floor(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::frac:
				output << "fract(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::frexp:
				output << "frexp(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::fwidth:
				output << "fwidth(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::ldexp:
				output << "ldexp(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::length:
				output << "length(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::lerp:
				output << "mix(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ", " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::log:
				output << "log(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::log10:
				output << "(log2(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ") / ";
				visit(output, node->type, false);
				output << "(2.302585093))";
				break;
			case fx::nodes::intrinsic_expression_node::log2:
				output << "log2(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::mad:
				output << "(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << " * " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " + " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::max:
				output << "max(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::min:
				output << "min(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::modf:
				output << "modf(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::mul:
				output << '(';
				visit(output, node->arguments[0]);
				output << " * ";
				visit(output, node->arguments[1]);
				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::normalize:
				output << "normalize(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::pow:
				output << "pow(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::radians:
				output << "radians(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::rcp:
				output << '(';
				visit(output, node->type, false);
				output << "(1.0) / ";
				visit(output, node->arguments[0]);
				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::reflect:
				output << "reflect(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::refract:
				output << "refract(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ", float";
				visit(output, node->arguments[2]);
				output << "))";
				break;
			case fx::nodes::intrinsic_expression_node::round:
				output << "round(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::rsqrt:
				output << "inversesqrt(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::saturate:
				output << "clamp(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", 0.0, 1.0)";
				break;
			case fx::nodes::intrinsic_expression_node::sign:
				output << cast1.first << "sign(";
				visit(output, node->arguments[0]);
				output << ')' << cast1.second;
				break;
			case fx::nodes::intrinsic_expression_node::sin:
				output << "sin(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::sincos:
				output << "_sincos(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);

				if (!type1.is_floating_point())
				{
					output << ')';
				}

				output << ", ";
				visit(output, node->arguments[1]);
				output << ", ";
				visit(output, node->arguments[2]);
				output << ')';
				break;
			case fx::nodes::intrinsic_expression_node::sinh:
				output << "sinh(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::smoothstep:
				output << "smoothstep(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ", " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::sqrt:
				output << "sqrt(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::step:
				output << "step(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::tan:
				output << "tan(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::tanh:
				output << "tanh(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
			case fx::nodes::intrinsic_expression_node::tex2d:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);

				output << "texture(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec2(1.0, -1.0) + vec2(0.0, 1.0))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dfetch:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_int, 0, 4, 1 };
				cast2 = write_cast(type2, type2to);

				output << "_texelFetch(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * ivec2(1, -1) + ivec2(0, 1))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dgather:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);

				output << "textureGather(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec2(1.0, -1.0) + vec2(0.0, 1.0), int(";
				visit(output, node->arguments[2]);
				output << "))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dgatheroffset:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
				const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);
				cast3 = write_cast(type3, type3to);

				output << "textureGatherOffset(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << " * ivec2(1, -1), int(";
				visit(output, node->arguments[3]);
				output << "))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dgrad:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);
				cast3 = write_cast(type3, type2to);
				cast4 = write_cast(type4, type2to);

				output << "textureGrad(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << ", " << cast4.first;
				visit(output, node->arguments[3]);
				output << cast4.second << ")";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dlevel:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
				cast2 = write_cast(type2, type2to);

				output << "_textureLod(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dleveloffset:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
				const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);
				cast3 = write_cast(type3, type3to);

				output << "_textureLodOffset(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0), " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << " * ivec2(1, -1))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2doffset:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
				const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
				cast2 = write_cast(type2, type2to);
				cast3 = write_cast(type3, type3to);

				output << "textureOffset(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " << cast3.first;
				visit(output, node->arguments[2]);
				output << cast3.second << " * ivec2(1, -1))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dproj:
			{
				const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
				cast2 = write_cast(type2, type2to);

				output << "textureProj(";
				visit(output, node->arguments[0]);
				output << ", " << cast2.first;
				visit(output, node->arguments[1]);
				output << cast2.second << " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
				break;
			}
			case fx::nodes::intrinsic_expression_node::tex2dsize:
				output << "textureSize(";
				visit(output, node->arguments[0]);
				output << ", int(";
				visit(output, node->arguments[1]);
				output << "))";
				break;
			case fx::nodes::intrinsic_expression_node::transpose:
				output << "transpose(";

				if (!type1.is_floating_point())
				{
					type1.basetype = fx::nodes::type_node::datatype_float;
					visit(output, type1, false);
					output << '(';
				}

				visit(output, node->arguments[0]);
				output << ')';

				if (!type1.is_floating_point())
				{
					output << ')';
				}
				break;
			case fx::nodes::intrinsic_expression_node::trunc:
				output << "trunc(" << cast1.first;
				visit(output, node->arguments[0]);
				output << cast1.second << ')';
				break;
		}
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::conditional_expression_node *node)
	{
		output<< '(';

		if (node->condition->type.is_vector())
		{
			output << "all(bvec" + std::to_string(node->condition->type.rows) + '(';

			visit(output, node->condition);

			output << "))";
		}
		else
		{
			output << "bool(";

			visit(output, node->condition);

			output << ')';
		}

		const auto cast1 = write_cast(node->expression_when_true->type, node->type);
		const auto cast2 = write_cast(node->expression_when_false->type, node->type);

		output << " ? " << cast1.first;
		visit(output, node->expression_when_true);
		output << cast1.second << " : " << cast2.first;
		visit(output, node->expression_when_false);
		output << cast2.second << ')';
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::swizzle_expression_node *node)
	{
		visit(output, node->operand);

		if (node->operand->type.is_matrix())
		{
			if (node->mask[1] >= 0)
			{
				error(node->location, "multiple component matrix swizzeling is not supported in OpenGL");
				return;
			}

			const unsigned int row = node->mask[0] % 4;
			const unsigned int col = (node->mask[0] - row) / 4;

			output << '[' << row << "][" << col << ']';
		}
		else
		{
			const char swizzle[4] = {
				'x', 'y', 'z', 'w'
			};

			output << '.';

			for (unsigned int i = 0; i < 4 && node->mask[i] >= 0; i++)
			{
				output << swizzle[node->mask[i]];
			}
		}
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::field_expression_node *node)
	{
		output << '(';
		visit(output, node->operand);
		output << '.' << escape_name(node->field_reference->unique_name) << ')';
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::assignment_expression_node *node)
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

		const auto cast = write_cast(node->right->type, node->left->type);

		output << ' ' << cast.first;
		visit(output, node->right);
		output << cast.second << ')';
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::call_expression_node *node)
	{
		output << escape_name(node->callee->unique_name) << '(';

		for (size_t i = 0, count = node->arguments.size(); i < count; i++)
		{
			const auto arg = node->arguments[i];
			const auto cast = write_cast(arg->type, node->callee->parameter_list[i]->type);

			output << cast.first;
			visit(output, arg);
			output << cast.second;

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ')';

		auto &info = _functions.at(_current_function);
		auto &info_callee = _functions.at(node->callee);

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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::constructor_expression_node *node)
	{
		if (node->type.is_matrix())
		{
			output << "transpose(";
		}

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

		if (node->type.is_matrix())
		{
			output << ')';
		}
	}
	void gl_fx_compiler::visit(std::stringstream &, const fx::nodes::initializer_list_node *)
	{
		assert(false);
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::initializer_list_node *node, const fx::nodes::type_node &type)
	{
		visit(output, type, false);

		output << "[](";

		for (size_t i = 0, count = node->values.size(); i < count; i++)
		{
			const auto expression = node->values[i];

			if (expression->id == fx::nodeid::initializer_list)
			{
				visit(output, static_cast<fx::nodes::initializer_list_node *>(expression), node->type);
			}
			else
			{
				const auto cast = write_cast(expression->type, type);

				output << cast.first;
				visit(output, expression);
				output << cast.second;
			}

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ')';
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::compound_statement_node *node)
	{
		output << "{\n";

		for (auto statement : node->statement_list)
		{
			visit(output, statement);
		}

		output << "}\n";
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::declarator_list_node *node, bool single_statement)
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::expression_statement_node *node)
	{
		visit(output, node->expression);

		output << ";\n";
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::if_statement_node *node)
	{
		const fx::nodes::type_node typeto = { fx::nodes::type_node::datatype_bool, 0, 1, 1 };
		const auto cast = write_cast(node->condition->type, typeto);

		output << "if (" << cast.first;

		visit(output, node->condition);

		output << cast.second << ")\n";

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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::switch_statement_node *node)
	{
		output << "switch (";

		visit(output, node->test_expression);

		output << ")\n{\n";

		for (auto currcase : node->case_list)
		{
			visit(output, currcase);
		}

		output << "}\n";
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::case_statement_node *node)
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::for_statement_node *node)
	{
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::while_statement_node *node)
	{
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::return_statement_node *node)
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
				assert(_current_function != nullptr);

				const auto cast = write_cast(node->return_value->type, _current_function->return_type);

				output << ' ' << cast.first;
				visit(output, node->return_value);
				output << cast.second;
			}
		}

		output << ";\n";
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::jump_statement_node *node)
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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::struct_declaration_node *node)
	{
		output << "struct " << escape_name(node->unique_name) << "\n{\n";

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
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type)
	{
		if (with_type)
		{
			visit(output, node->type);
			output << ' ';
		}

		output << escape_name(node->unique_name);

		if (node->type.is_array())
		{
			output << '[';

			if (node->type.array_length > 0)
			{
				output << node->type.array_length;
			}

			output << ']';
		}

		if (node->initializer_expression != nullptr)
		{
			output << " = ";

			if (node->initializer_expression->id == fx::nodeid::initializer_list)
			{
				visit(output, static_cast<fx::nodes::initializer_list_node *>(node->initializer_expression), node->type);
			}
			else
			{
				const auto cast = write_cast(node->initializer_expression->type, node->type);

				output << cast.first;
				visit(output, node->initializer_expression);
				output << cast.second;
			}
		}
	}
	void gl_fx_compiler::visit(std::stringstream &output, const fx::nodes::function_declaration_node *node)
	{
		_current_function = node;

		visit(output, node->return_type, false);

		output << ' ' << escape_name(node->unique_name) << '(';

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			visit(output, node->parameter_list[i]);

			if (i < count - 1)
			{
				output << ", ";
			}
		}

		output << ")\n";

		visit(output, node->definition);

		_current_function = nullptr;
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

	void gl_fx_compiler::visit_texture(const fx::nodes::variable_declaration_node *node)
	{
		const auto obj = new gl_texture();
		obj->name = node->name;
		GLuint width = obj->width = node->properties.Width;
		GLuint height = obj->height = node->properties.Height;
		GLuint levels = obj->levels = node->properties.MipLevels;

		GLenum internalformat = GL_RGBA8, internalformatSRGB = GL_SRGB8_ALPHA8;
		literal_to_format(node->properties.Format, internalformat, internalformatSRGB, obj->format);

		if (levels == 0)
		{
			warning(node->location, "a texture cannot have 0 miplevels, changed it to 1");

			levels = 1;
		}

		visit_annotation(node->annotations, *obj);

		if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
		{
			if (width != 1 || height != 1 || levels != 1 || internalformat != GL_RGBA8)
			{
				warning(node->location, "texture property on backbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(obj, texture::datatype::backbuffer, _runtime->_backbuffer_texture[0], _runtime->_backbuffer_texture[1]);
		}
		else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
		{
			if (width != 1 || height != 1 || levels != 1 || internalformat != GL_RGBA8)
			{
				warning(node->location, "texture property on depthbuffer textures are ignored");
			}

			_runtime->update_texture_datatype(obj, texture::datatype::depthbuffer, _runtime->_depth_texture, 0);
		}
		else
		{
			GLCHECK(glGenTextures(2, obj->id));

			GLint previous = 0, previousFBO = 0;
			GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));
			GLCHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFBO));

			GLCHECK(glBindTexture(GL_TEXTURE_2D, obj->id[0]));
			GLCHECK(glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height));
			GLCHECK(glTextureView(obj->id[1], GL_TEXTURE_2D, obj->id[0], internalformatSRGB, 0, levels, 0, 1));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

			// Clear texture to black
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _runtime->_blit_fbo));
			GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, obj->id[0], 0));
			GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT1));
			const GLuint clearColor[4] = { 0, 0, 0, 0 };
			GLCHECK(glClearBufferuiv(GL_COLOR, 0, clearColor));
			GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousFBO));
		}

		_runtime->add_texture(obj);
	}
	void gl_fx_compiler::visit_sampler(const fx::nodes::variable_declaration_node *node)
	{
		if (node->properties.Texture == nullptr)
		{
			error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
			return;
		}

		const auto texture = static_cast<gl_texture *>(_runtime->find_texture(node->properties.Texture->name));

		if (texture == nullptr)
		{
			error(node->location, "texture not found");
			return;
		}

		gl_sampler sampler;
		sampler.id = 0;
		sampler.texture = texture;
		sampler.is_srgb = node->properties.SRGBTexture;

		GLenum minfilter = literal_to_filter_mode(node->properties.MinFilter);
		const GLenum mipfilter = literal_to_filter_mode(node->properties.MipFilter);

		if (minfilter == GL_NEAREST && mipfilter == GL_NEAREST)
		{
			minfilter = GL_NEAREST_MIPMAP_NEAREST;
		}
		else if (minfilter == GL_NEAREST && mipfilter == GL_LINEAR)
		{
			minfilter = GL_NEAREST_MIPMAP_LINEAR;
		}
		else if (minfilter == GL_LINEAR && mipfilter == GL_NEAREST)
		{
			minfilter = GL_LINEAR_MIPMAP_NEAREST;
		}
		else if (minfilter == GL_LINEAR && mipfilter == GL_LINEAR)
		{
			minfilter = GL_LINEAR_MIPMAP_LINEAR;
		}

		GLCHECK(glGenSamplers(1, &sampler.id));
		GLCHECK(glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_S, literal_to_wrap_mode(node->properties.AddressU)));
		GLCHECK(glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_T, literal_to_wrap_mode(node->properties.AddressV)));
		GLCHECK(glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_R, literal_to_wrap_mode(node->properties.AddressW)));
		GLCHECK(glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, minfilter));
		GLCHECK(glSamplerParameteri(sampler.id, GL_TEXTURE_MAG_FILTER, literal_to_filter_mode(node->properties.MagFilter)));
		GLCHECK(glSamplerParameterf(sampler.id, GL_TEXTURE_LOD_BIAS, node->properties.MipLODBias));
		GLCHECK(glSamplerParameterf(sampler.id, GL_TEXTURE_MIN_LOD, node->properties.MinLOD));
		GLCHECK(glSamplerParameterf(sampler.id, GL_TEXTURE_MAX_LOD, node->properties.MaxLOD));
		GLCHECK(glSamplerParameterf(sampler.id, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */, static_cast<GLfloat>(node->properties.MaxAnisotropy)));

		_global_code << "layout(binding = " << _runtime->_effect_samplers.size() << ") uniform sampler2D " << escape_name(node->unique_name) << ";\n";

		_runtime->_effect_samplers.push_back(std::move(sampler));
	}
	void gl_fx_compiler::visit_uniform(const fx::nodes::variable_declaration_node *node)
	{
		visit(_global_uniforms, node->type, true);

		_global_uniforms << ' ' << escape_name(node->unique_name);

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
		obj->storage_size = obj->rows * obj->columns * std::max(1u, obj->elements);

		switch (node->type.basetype)
		{
			case fx::nodes::type_node::datatype_bool:
				obj->basetype = uniform::datatype::bool_;
				obj->storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_int:
				obj->basetype = uniform::datatype::int_;
				obj->storage_size *= sizeof(int);
				break;
			case fx::nodes::type_node::datatype_uint:
				obj->basetype = uniform::datatype::uint_;
				obj->storage_size *= sizeof(unsigned int);
				break;
			case fx::nodes::type_node::datatype_float:
				obj->basetype = uniform::datatype::float_;
				obj->storage_size *= sizeof(float);
				break;
		}

		const size_t alignment = 16 - (_current_uniform_size % 16);
		_current_uniform_size += (obj->storage_size > alignment && (alignment != 16 || obj->storage_size <= 16)) ? obj->storage_size + alignment : obj->storage_size;
		obj->storage_offset = _current_uniform_size - obj->storage_size;

		visit_annotation(node->annotations, *obj);

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (_current_uniform_size >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 128);
		}

		if (node->initializer_expression != nullptr && node->initializer_expression->id == fx::nodeid::literal_expression)
		{
			std::memcpy(uniform_storage.data() + obj->storage_offset, &static_cast<const fx::nodes::literal_expression_node *>(node->initializer_expression)->value_float, obj->storage_size);
		}
		else
		{
			std::memset(uniform_storage.data() + obj->storage_offset, 0, obj->storage_size);
		}

		_runtime->add_uniform(obj);
	}
	void gl_fx_compiler::visit_technique(const fx::nodes::technique_declaration_node *node)
	{
		const auto obj = new technique();
		obj->name = node->name;

		visit_annotation(node->annotation_list, *obj);

		for (auto pass : node->pass_list)
		{
			obj->passes.emplace_back(new gl_pass());
			visit_pass(pass, *static_cast<gl_pass *>(obj->passes.back().get()));
		}

		_runtime->add_technique(obj);
	}
	void gl_fx_compiler::visit_pass(const fx::nodes::pass_declaration_node *node, gl_pass &pass)
	{
		pass.color_mask[0] = (node->states.RenderTargetWriteMask & (1 << 0)) != 0;
		pass.color_mask[1] = (node->states.RenderTargetWriteMask & (1 << 1)) != 0;
		pass.color_mask[2] = (node->states.RenderTargetWriteMask & (1 << 2)) != 0;
		pass.color_mask[3] = (node->states.RenderTargetWriteMask & (1 << 3)) != 0;
		pass.depth_test = node->states.DepthEnable;
		pass.depth_mask = node->states.DepthWriteMask;
		pass.depth_func = literal_to_comp_func(node->states.DepthFunc);
		pass.stencil_test = node->states.StencilEnable;
		pass.stencil_read_mask = node->states.StencilReadMask;
		pass.stencil_mask = node->states.StencilWriteMask;
		pass.stencil_func = literal_to_comp_func(node->states.StencilFunc);
		pass.stencil_op_z_pass = literal_to_stencil_op(node->states.StencilOpPass);
		pass.stencil_op_fail = literal_to_stencil_op(node->states.StencilOpFail);
		pass.stencil_op_z_fail = literal_to_stencil_op(node->states.StencilOpDepthFail);
		pass.blend = node->states.BlendEnable;
		pass.blend_eq_color = literal_to_blend_eq(node->states.BlendOp);
		pass.blend_eq_alpha = literal_to_blend_eq(node->states.BlendOpAlpha);
		pass.blend_src = literal_to_blend_func(node->states.SrcBlend);
		pass.blend_dest = literal_to_blend_func(node->states.DestBlend);
		pass.stencil_reference = node->states.StencilRef;
		pass.srgb = node->states.SRGBWriteEnable;

		GLCHECK(glGenFramebuffers(1, &pass.fbo));
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.fbo));

		bool backbufferFramebuffer = true;

		for (unsigned int i = 0; i < 8; ++i)
		{
			if (node->states.RenderTargets[i] == nullptr)
			{
				continue;
			}

			const auto texture = static_cast<const gl_texture *>(_runtime->find_texture(node->states.RenderTargets[i]->name));

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
				return;
			}

			if (pass.viewport_width != 0 && pass.viewport_height != 0 && (texture->width != static_cast<unsigned int>(pass.viewport_width) || texture->height != static_cast<unsigned int>(pass.viewport_height)))
			{
				error(node->location, "cannot use multiple render targets with different sized textures");
				return;
			}
			else
			{
				pass.viewport_width = texture->width;
				pass.viewport_height = texture->height;
			}

			backbufferFramebuffer = false;

			GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->id[pass.srgb], 0));

			pass.draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
			pass.draw_textures[i] = texture->id[pass.srgb];
		}

		if (backbufferFramebuffer)
		{
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[0]));

			pass.draw_buffers[0] = GL_COLOR_ATTACHMENT0;
			pass.draw_textures[0] = _runtime->_backbuffer_texture[1];

			RECT rect;
			GetClientRect(WindowFromDC(_runtime->_hdc), &rect);

			pass.viewport_width = static_cast<GLsizei>(rect.right - rect.left);
			pass.viewport_height = static_cast<GLsizei>(rect.bottom - rect.top);
		}

		GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[1]));

		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		GLuint shaders[2] = { 0, 0 };
		GLenum shader_types[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
		const fx::nodes::function_declaration_node *shader_functions[2] = { node->states.VertexShader, node->states.PixelShader };

		pass.program = glCreateProgram();

		for (unsigned int i = 0; i < 2; i++)
		{
			if (shader_functions[i] != nullptr)
			{
				shaders[i] = glCreateShader(shader_types[i]);

				visit_pass_shader(shader_functions[i], shader_types[i], shaders[i]);

				GLCHECK(glAttachShader(pass.program, shaders[i]));
			}
		}

		GLCHECK(glLinkProgram(pass.program));

		for (unsigned int i = 0; i < 2; i++)
		{
			GLCHECK(glDetachShader(pass.program, shaders[i]));
			GLCHECK(glDeleteShader(shaders[i]));
		}

		GLint status = GL_FALSE;

		GLCHECK(glGetProgramiv(pass.program, GL_LINK_STATUS, &status));

		if (status == GL_FALSE)
		{
			GLint logsize = 0;
			GLCHECK(glGetProgramiv(pass.program, GL_INFO_LOG_LENGTH, &logsize));

			std::string log(logsize, '\0');
			GLCHECK(glGetProgramInfoLog(pass.program, logsize, nullptr, &log.front()));

			GLCHECK(glDeleteProgram(pass.program));

			_errors += log;
			error(node->location, "program linking failed");
			return;
		}
	}
	void gl_fx_compiler::visit_pass_shader(const fx::nodes::function_declaration_node *node, GLuint shadertype, GLuint &shader)
	{
		std::stringstream source;

		source <<
			"#version 430\n"
			"float _fmod(float x, float y) { return x - y * trunc(x / y); }"
			"vec2 _fmod(vec2 x, vec2 y) { return x - y * trunc(x / y); }"
			"vec3 _fmod(vec3 x, vec3 y) { return x - y * trunc(x / y); }"
			"vec4 _fmod(vec4 x, vec4 y) { return x - y * trunc(x / y); }"
			"mat2 _fmod(mat2 x, mat2 y) { return x - matrixCompMult(y, mat2(trunc(x[0] / y[0]), trunc(x[1] / y[1]))); }"
			"mat3 _fmod(mat3 x, mat3 y) { return x - matrixCompMult(y, mat3(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]))); }"
			"mat4 _fmod(mat4 x, mat4 y) { return x - matrixCompMult(y, mat4(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]), trunc(x[3] / y[3]))); }\n"
			"void _sincos(float x, out float s, out float c) { s = sin(x), c = cos(x); }"
			"void _sincos(vec2 x, out vec2 s, out vec2 c) { s = sin(x), c = cos(x); }"
			"void _sincos(vec3 x, out vec3 s, out vec3 c) { s = sin(x), c = cos(x); }"
			"void _sincos(vec4 x, out vec4 s, out vec4 c) { s = sin(x), c = cos(x); }\n"
			"vec4 _textureLod(sampler2D s, vec4 c) { return textureLod(s, c.xy, c.w); }\n"
			"vec4 _texelFetch(sampler2D s, ivec4 c) { return texelFetch(s, c.xy, c.w); }\n"
			"#define _textureLodOffset(s, c, offset) textureLodOffset(s, (c).xy, (c).w, offset)\n";

		source << "layout(std140, binding = 0) uniform _GLOBAL_\n{\n" << _global_uniforms.str() << "};\n";

		if (shadertype != GL_FRAGMENT_SHADER)
		{
			source << "#define discard\n";
		}

		source << _global_code.str();

		for (auto dependency : _functions.at(node).dependencies)
		{
			source << _functions.at(dependency).code;
		}

		source << _functions.at(node).code;

		for (auto parameter : node->parameter_list)
		{
			if (parameter->type.is_struct())
			{
				for (auto field : parameter->type.definition->field_list)
				{
					visit_shader_param(source, field->type, parameter->type.qualifiers, "_param_" + parameter->name + "_" + field->name, field->semantic, shadertype);
				}
			}
			else
			{
				visit_shader_param(source, parameter->type, parameter->type.qualifiers, "_param_" + parameter->name, parameter->semantic, shadertype);
			}
		}

		if (node->return_type.is_struct())
		{
			for (auto field : node->return_type.definition->field_list)
			{
				visit_shader_param(source, field->type, fx::nodes::type_node::qualifier_out, "_return_" + field->name, field->semantic, shadertype);
			}
		}
		else if (!node->return_type.is_void())
		{
			visit_shader_param(source, node->return_type, fx::nodes::type_node::qualifier_out, "_return", node->return_semantic, shadertype);
		}

		source << "void main()\n{\n";

		for (auto parameter : node->parameter_list)
		{
			for (int i = 0, array_length = std::max(1, parameter->type.array_length); i < array_length; i++)
			{
				if (parameter->type.is_struct())
				{
					visit(source, parameter->type, false);

					source << " _param_" << parameter->name;

					if (parameter->type.is_array())
					{
						source << i;
					}

					source << " = ";

					visit(source, parameter->type, false);

					source << '(';

					for (size_t k = 0, count = parameter->type.definition->field_list.size(); k < count; k++)
					{
						const auto field = parameter->type.definition->field_list[k];

						source << escape_name_with_builtins("_param_" + parameter->name + "_" + field->name + (parameter->type.is_array() ? std::to_string(i) : ""), field->semantic, shadertype);

						if (k < count - 1)
						{
							source << ", ";
						}
					}

					source << ");\n";
				}
				else if (
					parameter->semantic.compare(0, 5, "COLOR") == 0 ||
					parameter->semantic.compare(0, 9, "SV_TARGET") == 0)
				{
					source << " _param_" << parameter->name;
					
					if (parameter->type.is_array())
					{
						source << i;
					}

					source << " = vec4(0, 0, 0, 1);\n";
				}
			}

			if (parameter->type.is_array())
			{
				visit(source, parameter->type, false);

				source << " _param_" << parameter->name << "[] = ";

				visit(source, parameter->type, false);

				source << "[](";

				for (int i = 0, count = parameter->type.array_length; i < count; i++)
				{
					source << "_param_" << parameter->name << i;

					if (i < count - 1)
					{
						source << ", ";
					}
				}

				source << ");\n";
			}
		}

		if (node->return_type.is_struct())
		{
			visit(source, node->return_type, false);

			source << ' ';
		}

		if (!node->return_type.is_void())
		{
			source << "_return = ";

			if (node->return_type.rows < 4 && (
				node->return_semantic.compare(0, 5, "COLOR") == 0 ||
				node->return_semantic.compare(0, 9, "SV_TARGET") == 0))
			{
				const std::string swizzle[3] = { "x", "xy", "xyz" };

				source << "vec4(0, 0, 0, 1);\n_return." << swizzle[node->return_type.rows - 1] << " = ";
			}
		}

		source << escape_name(node->unique_name) << '(';

		for (size_t i = 0, count = node->parameter_list.size(); i < count; i++)
		{
			const auto parameter = node->parameter_list[i];

			source << escape_name_with_builtins("_param_" + parameter->name, parameter->semantic, shadertype);

			if (parameter->type.rows < 4 && (
				parameter->semantic.compare(0, 5, "COLOR") == 0 ||
				parameter->semantic.compare(0, 9, "SV_TARGET") == 0))
			{
				const std::string swizzle[3] = { "x", "xy", "xyz" };

				source << '.' << swizzle[parameter->type.rows - 1];
			}

			if (i < count - 1)
			{
				source << ", ";
			}
		}

		source << ");\n";

		for (auto parameter : node->parameter_list)
		{
			if (!parameter->type.has_qualifier(fx::nodes::type_node::qualifier_out))
			{
				continue;
			}

			if (parameter->type.is_array())
			{
				for (int i = 0; i < parameter->type.array_length; i++)
				{
					source << "_param_" << parameter->name << i << " = _param_" + parameter->name + "[" + std::to_string(i) + "];\n";
				}
			}

			for (int i = 0; i < std::max(1, parameter->type.array_length); i++)
			{
				if (parameter->type.is_struct())
				{
					for (auto field : parameter->type.definition->field_list)
					{
						source << "_param_" << parameter->name << '_' << field->name;

						if (parameter->type.is_array())
						{
							source << i;
						}

						source << " = _param_" << parameter->name << '.' << field->name;

						if (parameter->type.is_array())
						{
							source << '[' << i << ']';
						}

						source << ";\n";
					}
				}
			}
		}

		if (node->return_type.is_struct())
		{
			for (auto field : node->return_type.definition->field_list)
			{
				source << escape_name_with_builtins("_return_" + field->name, field->semantic, shadertype) << " = _return." << field->name << ";\n";
			}
		}

		if (shadertype == GL_VERTEX_SHADER)
		{
			source << "gl_Position = gl_Position * vec4(1.0, 1.0, 2.0, 1.0) + vec4(0.0, 0.0, -gl_Position.w, 0.0);\n";
		}

		source << "}\n";

		const auto source_str = source.str();

		LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source_str.c_str() << "\n";

		GLint status = GL_FALSE;
		const GLchar *src = source_str.c_str();
		const GLsizei len = static_cast<GLsizei>(source_str.size());

		GLCHECK(glShaderSource(shader, 1, &src, &len));
		GLCHECK(glCompileShader(shader));
		GLCHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

		if (status == GL_FALSE)
		{
			GLint logsize = 0;
			GLCHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logsize));

			std::string log(logsize, '\0');
			GLCHECK(glGetShaderInfoLog(shader, logsize, nullptr, &log.front()));

			_errors += log;
			error(node->location, "internal shader compilation failed");
		}
	}
	void gl_fx_compiler::visit_shader_param(std::stringstream &output, fx::nodes::type_node type, unsigned int qualifier, const std::string &name, const std::string &semantic, GLuint shadertype)
	{
		type.qualifiers = static_cast<unsigned int>(qualifier);

		unsigned long location = 0;

		for (int i = 0, array_length = std::max(1, type.array_length); i < array_length; ++i)
		{
			if (!escape_name_with_builtins(std::string(), semantic, shadertype).empty())
			{
				continue;
			}
			else if (semantic.compare(0, 5, "COLOR") == 0)
			{
				type.rows = 4;

				location = strtoul(semantic.c_str() + 5, nullptr, 10);
			}
			else if (semantic.compare(0, 8, "TEXCOORD") == 0)
			{
				location = strtoul(semantic.c_str() + 8, nullptr, 10) + 1;
			}
			else if (semantic.compare(0, 9, "SV_TARGET") == 0)
			{
				type.rows = 4;

				location = strtoul(semantic.c_str() + 9, nullptr, 10);
			}

			output << "layout(location = " << (location + i) << ") ";

			visit(output, type, true);

			output << ' ' + name;

			if (type.is_array())
			{
				output << i;
			}

			output << ";\n";
		}
	}
}
