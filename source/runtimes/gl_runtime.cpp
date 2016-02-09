#include "log.hpp"
#include "gl_runtime.hpp"
#include "fx\ast.hpp"
#include "fx\compiler.hpp"
#include "gui.hpp"
#include "input.hpp"

#include <assert.h>
#include <nanovg_gl.h>
#include <boost\algorithm\string.hpp>

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT 0x8C70
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT 0x8C72

// ---------------------------------------------------------------------------------------------------

namespace reshade
{
	namespace runtimes
	{
		struct gl_texture : public texture
		{
			gl_texture() : ID()
			{
			}
			~gl_texture()
			{
				if (basetype == datatype::image)
				{
					GLCHECK(glDeleteTextures(2, ID));
				}
			}

			void change_datatype(datatype source, GLuint texture, GLuint textureSRGB)
			{
				if (basetype == datatype::image)
				{
					GLCHECK(glDeleteTextures(2, ID));
				}

				basetype = source;

				if (textureSRGB == 0)
				{
					textureSRGB = texture;
				}

				if (ID[0] == texture && ID[1] == textureSRGB)
				{
					return;
				}

				ID[0] = texture;
				ID[1] = textureSRGB;
			}

			GLuint ID[2];
		};
		struct gl_sampler
		{
			GLuint ID;
			gl_texture *Texture;
			bool SRGB;
		};
		struct gl_technique : public technique
		{
			struct Pass
			{
				GLuint Program;
				GLuint Framebuffer, DrawTextures[8];
				GLint StencilRef;
				GLuint StencilMask, StencilReadMask;
				GLsizei ViewportWidth, ViewportHeight;
				GLenum DrawBuffers[8], BlendEqColor, BlendEqAlpha, BlendFuncSrc, BlendFuncDest, DepthFunc, StencilFunc, StencilOpFail, StencilOpZFail, StencilOpZPass;
				GLboolean FramebufferSRGB, Blend, DepthMask, DepthTest, StencilTest, ColorMaskR, ColorMaskG, ColorMaskB, ColorMaskA;
			};

			~gl_technique()
			{
				for (auto &pass : passes)
				{
					GLCHECK(glDeleteProgram(pass.Program));
					GLCHECK(glDeleteFramebuffers(1, &pass.Framebuffer));
				}
			}

			std::vector<Pass> passes;
		};

		namespace
		{
			class gl_fx_compiler : fx::compiler
			{
			public:
				gl_fx_compiler(gl_runtime *runtime, const fx::nodetree &ast, std::string &errors) : compiler(ast, errors), _runtime(runtime), _current_function(nullptr), _current_uniform_size(0)
				{
				}

				bool run() override
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
							visit(_global_code, uniform, true);

							_global_code += ";\n";
						}
					}

					for (auto node : _ast.functions)
					{
						const auto function = static_cast<fx::nodes::function_declaration_node *>(node);

						_current_function = function;

						visit(_functions[function].code, function);
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

				static GLenum LiteralToCompFunc(unsigned int value)
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
				static GLenum LiteralToBlendEq(unsigned int value)
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
				static GLenum LiteralToBlendFunc(unsigned int value)
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
				static GLenum LiteralToStencilOp(unsigned int value)
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
				static GLenum LiteralToTextureWrap(unsigned int value)
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
				static GLenum LiteralToTextureFilter(unsigned int value)
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
				static void LiteralToFormat(unsigned int value, GLenum &internalformat, GLenum &internalformatsrgb, texture::pixelformat &name)
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
							internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
							internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
							break;
						case fx::nodes::variable_declaration_node::properties::DXT3:
							name = texture::pixelformat::dxt3;
							internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
							internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
							break;
						case fx::nodes::variable_declaration_node::properties::DXT5:
							name = texture::pixelformat::dxt5;
							internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
							internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
							break;
						case fx::nodes::variable_declaration_node::properties::LATC1:
							name = texture::pixelformat::latc1;
							internalformat = internalformatsrgb = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
							break;
						case fx::nodes::variable_declaration_node::properties::LATC2:
							name = texture::pixelformat::latc2;
							internalformat = internalformatsrgb = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
							break;
						default:
							name = texture::pixelformat::unknown;
							internalformat = internalformatsrgb = GL_NONE;
							break;
					}
				}
				static std::string FixName(const std::string &name)
				{
					std::string res;

					if (boost::starts_with(name, "gl_") ||
						name == "common" || name == "partition" || name == "input" || name == "ouput" || name == "active" || name == "filter" || name == "superp" ||
						name == "invariant" || name == "lowp" || name == "mediump" || name == "highp" || name == "precision" || name == "patch" || name == "subroutine" ||
						name == "abs" || name == "sign" || name == "all" || name == "any" || name == "sin" || name == "sinh" || name == "cos" || name == "cosh" || name == "tan" || name == "tanh" || name == "asin" || name == "acos" || name == "atan" || name == "exp" || name == "exp2" || name == "log" || name == "log2" || name == "sqrt" || name == "inversesqrt" || name == "ceil" || name == "floor" || name == "fract" || name == "trunc" || name == "round" || name == "radians" || name == "degrees" || name == "length" || name == "normalize" || name == "transpose" || name == "determinant" || name == "intBitsToFloat" || name == "uintBitsToFloat" || name == "floatBitsToInt" || name == "floatBitsToUint" || name == "matrixCompMult" || name == "not" || name == "lessThan" || name == "greaterThan" || name == "lessThanEqual" || name == "greaterThanEqual" || name == "equal" || name == "notEqual" || name == "dot" || name == "cross" || name == "distance" || name == "pow" || name == "modf" || name == "frexp" || name == "ldexp" || name == "min" || name == "max" || name == "step" || name == "reflect" || name == "texture" || name == "textureOffset" || name == "fma" || name == "mix" || name == "clamp" || name == "smoothstep" || name == "refract" || name == "faceforward" || name == "textureLod" || name == "textureLodOffset" || name == "texelFetch" || name == "main")
					{
						res += '_';
					}

					res += boost::replace_all_copy(name, "__", "_US");

					return res;
				}
				static std::string FixNameWithSemantic(const std::string &name, const std::string &semantic, GLuint shadertype)
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

					return FixName(name);
				}

			private:
				std::pair<std::string, std::string> write_cast(const fx::nodes::type_node &from, const fx::nodes::type_node &to)
				{
					std::pair<std::string, std::string> code;

					if (from.basetype != to.basetype && !(from.is_matrix() && to.is_matrix()))
					{
						const fx::nodes::type_node type = { to.basetype, 0, from.rows, from.cols, 0, to.definition };

						visit(code.first, type, true);
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

				using compiler::visit;
				void visit(std::string &output, const fx::nodes::type_node &type, bool with_qualifiers = true) override
				{
					if (with_qualifiers)
					{
						if (type.has_qualifier(fx::nodes::type_node::qualifier_linear))
							output += "smooth ";
						if (type.has_qualifier(fx::nodes::type_node::qualifier_noperspective))
							output += "noperspective ";
						if (type.has_qualifier(fx::nodes::type_node::qualifier_centroid))
							output += "centroid ";
						if (type.has_qualifier(fx::nodes::type_node::qualifier_nointerpolation))
							output += "flat ";
						if (type.has_qualifier(fx::nodes::type_node::qualifier_inout))
							output += "inout ";
						else if (type.has_qualifier(fx::nodes::type_node::qualifier_in))
							output += "in ";
						else if (type.has_qualifier(fx::nodes::type_node::qualifier_out))
							output += "out ";
						else if (type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
							output += "uniform ";
						if (type.has_qualifier(fx::nodes::type_node::qualifier_const))
							output += "const ";
					}

					switch (type.basetype)
					{
						case fx::nodes::type_node::datatype_void:
							output += "void";
							break;
						case fx::nodes::type_node::datatype_bool:
							if (type.is_matrix())
								output += "mat" + std::to_string(type.rows) + "x" + std::to_string(type.cols);
							else if (type.is_vector())
								output += "bvec" + std::to_string(type.rows);
							else
								output += "bool";
							break;
						case fx::nodes::type_node::datatype_int:
							if (type.is_matrix())
								output += "mat" + std::to_string(type.rows) + "x" + std::to_string(type.cols);
							else if (type.is_vector())
								output += "ivec" + std::to_string(type.rows);
							else
								output += "int";
							break;
						case fx::nodes::type_node::datatype_uint:
							if (type.is_matrix())
								output += "mat" + std::to_string(type.rows) + "x" + std::to_string(type.cols);
							else if (type.is_vector())
								output += "uvec" + std::to_string(type.rows);
							else
								output += "uint";
							break;
						case fx::nodes::type_node::datatype_float:
							if (type.is_matrix())
								output += "mat" + std::to_string(type.rows) + "x" + std::to_string(type.cols);
							else if (type.is_vector())
								output += "vec" + std::to_string(type.rows);
							else
								output += "float";
							break;
						case fx::nodes::type_node::datatype_sampler:
							output += "sampler2D";
							break;
						case fx::nodes::type_node::datatype_struct:
							output += FixName(type.definition->unique_name);
							break;
					}
				}
				void visit(std::string &output, const fx::nodes::lvalue_expression_node *node) override
				{
					output += FixName(node->reference->unique_name);
				}
				void visit(std::string &output, const fx::nodes::literal_expression_node *node) override
				{
					if (!node->type.is_scalar())
					{
						visit(output, node->type, false);

						output += '(';
					}

					for (unsigned int i = 0; i < node->type.rows * node->type.cols; ++i)
					{
						switch (node->type.basetype)
						{
							case fx::nodes::type_node::datatype_bool:
								output += node->value_int[i] ? "true" : "false";
								break;
							case fx::nodes::type_node::datatype_int:
								output += std::to_string(node->value_int[i]);
								break;
							case fx::nodes::type_node::datatype_uint:
								output += std::to_string(node->value_uint[i]) + "u";
								break;
							case fx::nodes::type_node::datatype_float:
								output += std::to_string(node->value_float[i]);
								break;
						}

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					if (!node->type.is_scalar())
					{
						output += ')';
					}
				}
				void visit(std::string &output, const fx::nodes::expression_sequence_node *node) override
				{
					output += '(';

					for (auto expression : node->expression_list)
					{
						visit(output, expression);

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::unary_expression_node *node) override
				{
					std::string part1, part2;

					switch (node->op)
					{
						case fx::nodes::unary_expression_node::negate:
							part1 = '-';
							break;
						case fx::nodes::unary_expression_node::bitwise_not:
							part1 = "~";
							break;
						case fx::nodes::unary_expression_node::logical_not:
							if (node->type.is_vector())
							{
								const auto cast = write_cast(node->operand->type, node->type);

								part1 = "not(" + cast.first;
								part2 = cast.second + ')';
							}
							else
							{
								part1 = "!bool(";
								part2 = ')';
							}
							break;
						case fx::nodes::unary_expression_node::pre_increase:
							part1 = "++";
							break;
						case fx::nodes::unary_expression_node::pre_decrease:
							part1 = "--";
							break;
						case fx::nodes::unary_expression_node::post_increase:
							part2 = "++";
							break;
						case fx::nodes::unary_expression_node::post_decrease:
							part2 = "--";
							break;
						case fx::nodes::unary_expression_node::cast:
							visit(part1, node->type, false);
							part1 += '(';
							part2 = ')';
							break;
					}

					output += part1;
					visit(output, node->operand);
					output += part2;
				}
				void visit(std::string &output, const fx::nodes::binary_expression_node *node) override
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

					output += part1;
					visit(output, node->operands[0]);
					output += part2;
					visit(output, node->operands[1]);
					output += part3;
				}
				void visit(std::string &output, const fx::nodes::intrinsic_expression_node *node) override
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

					std::string part1, part2, part3, part4, part5;

					switch (node->op)
					{
						case fx::nodes::intrinsic_expression_node::abs:
							part1 = "abs(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::acos:
							part1 = "acos(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::all:
							if (type1.is_vector())
							{
								part1 = "all(bvec" + std::to_string(type1.rows) + '(';
								part2 = "))";
							}
							else
							{
								part1 = "bool(";
								part2 = ')';
							}
							break;
						case fx::nodes::intrinsic_expression_node::any:
							if (type1.is_vector())
							{
								part1 = "any(bvec" + std::to_string(type1.rows) + '(';
								part2 = "))";
							}
							else
							{
								part1 = "bool(";
								part2 = ')';
							}
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_int2float:
							part1 = "intBitsToFloat(";

							if (type1.basetype != fx::nodes::type_node::datatype_int)
							{
								type1.basetype = fx::nodes::type_node::datatype_int;

								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
							part1 = "uintBitsToFloat(";

							if (type1.basetype != fx::nodes::type_node::datatype_uint)
							{
								type1.basetype = fx::nodes::type_node::datatype_uint;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::asin:
							part1 = "asin(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_float2int:
							part1 = "floatBitsToInt(";

							if (type1.basetype != fx::nodes::type_node::datatype_float)
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
							part1 = "floatBitsToUint(";

							if (type1.basetype != fx::nodes::type_node::datatype_float)
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::atan:
							part1 = "atan(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::atan2:
							part1 = "atan(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::ceil:
							part1 = "ceil(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::clamp:
							part1 = "clamp(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ", " + cast3.first;
							part4 = cast3.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::cos:
							part1 = "cos(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::cosh:
							part1 = "cosh(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::cross:
							part1 = "cross(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::ddx:
							part1 = "dFdx(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::ddy:
							part1 = "dFdy(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::degrees:
							part1 = "degrees(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::determinant:
							part1 = "determinant(";

							if (!type1.is_floating_point())
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::distance:
							part1 = "distance(" + cast121.first;
							part2 = cast121.second + ", " + cast122.first;
							part3 = cast122.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::dot:
							part1 = "dot(" + cast121.first;
							part2 = cast121.second + ", " + cast122.first;
							part3 = cast122.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::exp:
							part1 = "exp(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::exp2:
							part1 = "exp2(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::faceforward:
							part1 = "faceforward(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ", " + cast3.first;
							part4 = cast3.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::floor:
							part1 = "floor(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::frac:
							part1 = "fract(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::frexp:
							part1 = "frexp(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::fwidth:
							part1 = "fwidth(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::ldexp:
							part1 = "ldexp(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::length:
							part1 = "length(";

							if (!type1.is_floating_point())
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::lerp:
							part1 = "mix(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ", " + cast3.first;
							part4 = cast3.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::log:
							part1 = "log(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::log10:
							part1 = "(log2(" + cast1.first;
							part2 = cast1.second + ") / ";
							visit(part2, node->type, false);
							part2 += "(2.302585093))";
							break;
						case fx::nodes::intrinsic_expression_node::log2:
							part1 = "log2(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::mad:
							part1 = "(" + cast1.first;
							part2 = cast1.second + " * " + cast2.first;
							part3 = cast2.second + " + " + cast3.first;
							part4 = cast3.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::max:
							part1 = "max(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::min:
							part1 = "min(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::modf:
							part1 = "modf(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::mul:
							part1 = '(';
							part2 = " * ";
							part3 = ')';
							break;
						case fx::nodes::intrinsic_expression_node::normalize:
							part1 = "normalize(";

							if (!type1.is_floating_point())
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::pow:
							part1 = "pow(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::radians:
							part1 = "radians(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::rcp:
							part1 = '(';
							visit(part1, node->type, false);
							part1 += "(1.0) / ";
							part2 = ')';
							break;
						case fx::nodes::intrinsic_expression_node::reflect:
							part1 = "reflect(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::refract:
							part1 = "refract(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ", float";
							part4 = "))";
							break;
						case fx::nodes::intrinsic_expression_node::round:
							part1 = "round(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::rsqrt:
							part1 = "inversesqrt(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::saturate:
							part1 = "clamp(" + cast1.first;
							part2 = cast1.second + ", 0.0, 1.0)";
							break;
						case fx::nodes::intrinsic_expression_node::sign:
							part1 = cast1.first + "sign(";
							part2 = ')' + cast1.second;
							break;
						case fx::nodes::intrinsic_expression_node::sin:
							part1 = "sin(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::sincos:
							part1 = "_sincos(";

							if (type1.basetype != fx::nodes::type_node::datatype_float)
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 = ", ";
							part3 = ", ";
							part4 = ')';
							break;
						case fx::nodes::intrinsic_expression_node::sinh:
							part1 = "sinh(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::smoothstep:
							part1 = "smoothstep(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ", " + cast3.first;
							part4 = cast3.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::sqrt:
							part1 = "sqrt(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::step:
							part1 = "step(" + cast1.first;
							part2 = cast1.second + ", " + cast2.first;
							part3 = cast2.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::tan:
							part1 = "tan(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::tanh:
							part1 = "tanh(" + cast1.first;
							part2 = cast1.second + ')';
							break;
						case fx::nodes::intrinsic_expression_node::tex2d:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);

							part1 = "texture(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dfetch:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);

							part1 = "texelFetch(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * ivec2(1, -1) + ivec2(0, 1))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dgather:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);

							part1 = "textureGather(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), int(";
							part4 = "))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dgatheroffset:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
							const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);
							cast3 = write_cast(type3, type3to);

							part1 = "textureGatherOffset(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " + cast3.first;
							part4 = cast3.second + " * ivec2(1, -1), int(";
							part5 = "))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dgrad:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);
							cast3 = write_cast(type3, type2to);
							cast4 = write_cast(type4, type2to);

							part1 = "textureGrad(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " + cast3.first;
							part4 = cast3.second + ", " + cast4.first;
							part5 = cast4.second + ")";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dlevel:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
							cast2 = write_cast(type2, type2to);

							part1 = "_textureLod(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dleveloffset:
						{	
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
							const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);
							cast3 = write_cast(type3, type3to);

							part1 = "_textureLodOffset(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0), " + cast3.first;
							part4 = cast3.second + " * ivec2(1, -1))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2doffset:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 2, 1 };
							const fx::nodes::type_node type3to = { fx::nodes::type_node::datatype_int, 0, 2, 1 };
							cast2 = write_cast(type2, type2to);
							cast3 = write_cast(type3, type3to);

							part1 = "textureOffset(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " + cast3.first;
							part4 = cast3.second + " * ivec2(1, -1))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dproj:
						{
							const fx::nodes::type_node type2to = { fx::nodes::type_node::datatype_float, 0, 4, 1 };
							cast2 = write_cast(type2, type2to);

							part1 = "textureProj(";
							part2 = ", " + cast2.first;
							part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
							break;
						}
						case fx::nodes::intrinsic_expression_node::tex2dsize:
							part1 = "textureSize(";
							part2 = ", int(";
							part3 = "))";
							break;
						case fx::nodes::intrinsic_expression_node::transpose:
							part1 = "transpose(";

							if (!type1.is_floating_point())
							{
								type1.basetype = fx::nodes::type_node::datatype_float;
								
								visit(part1, type1, false);
								part1 += '(';
								part2 = ')';
							}

							part2 += ')';
							break;
						case fx::nodes::intrinsic_expression_node::trunc:
							part1 = "trunc(" + cast1.first;
							part2 = cast1.second + ')';
							break;
					}

					output += part1;

					if (node->arguments[0] != nullptr)
					{
						visit(output, node->arguments[0]);
					}

					output += part2;

					if (node->arguments[1] != nullptr)
					{
						visit(output, node->arguments[1]);
					}

					output += part3;

					if (node->arguments[2] != nullptr)
					{
						visit(output, node->arguments[2]);
					}

					output += part4;

					if (node->arguments[3] != nullptr)
					{
						visit(output, node->arguments[3]);
					}

					output += part5;
				}
				void visit(std::string &output, const fx::nodes::conditional_expression_node *node) override
				{
					output += '(';

					if (node->condition->type.is_vector())
					{
						output += "all(bvec" + std::to_string(node->condition->type.rows) + '(';
						
						visit(output, node->condition);
						
						output += "))";
					}
					else
					{
						output += "bool(";
						
						visit(output, node->condition);
						
						output += ')';
					}

					const std::pair<std::string, std::string> cast1 = write_cast(node->expression_when_true->type, node->type);
					const std::pair<std::string, std::string> cast2 = write_cast(node->expression_when_false->type, node->type);

					output += " ? " + cast1.first;
					visit(output, node->expression_when_true);
					output += cast1.second + " : " + cast2.first;
					visit(output, node->expression_when_false);
					output += cast2.second + ')';
				}
				void visit(std::string &output, const fx::nodes::swizzle_expression_node *node) override
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

						output += '[' + std::to_string(row) + "][" + std::to_string(col) + ']';
					}
					else
					{
						const char swizzle[4] = { 'x', 'y', 'z', 'w' };

						output += '.';

						for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
						{
							output += swizzle[node->mask[i]];
						}
					}
				}
				void visit(std::string &output, const fx::nodes::field_expression_node *node) override
				{
					output += '(';

					visit(output, node->operand);

					output += '.';
					output += FixName(node->field_reference->unique_name);
					output += ')';
				}
				void visit(std::string &output, const fx::nodes::assignment_expression_node *node) override
				{
					output += '(';
					
					visit(output, node->left);
					
					output += ' ';

					switch (node->op)
					{
						case fx::nodes::assignment_expression_node::none:
							output += '=';
							break;
						case fx::nodes::assignment_expression_node::add:
							output += "+=";
							break;
						case fx::nodes::assignment_expression_node::subtract:
							output += "-=";
							break;
						case fx::nodes::assignment_expression_node::multiply:
							output += "*=";
							break;
						case fx::nodes::assignment_expression_node::divide:
							output += "/=";
							break;
						case fx::nodes::assignment_expression_node::modulo:
							output += "%=";
							break;
						case fx::nodes::assignment_expression_node::left_shift:
							output += "<<=";
							break;
						case fx::nodes::assignment_expression_node::right_shift:
							output += ">>=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_and:
							output += "&=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_or:
							output += "|=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_xor:
							output += "^=";
							break;
					}

					const std::pair<std::string, std::string> cast = write_cast(node->right->type, node->left->type);

					output += ' ' + cast.first;
					
					visit(output, node->right);
					
					output += cast.second + ')';
				}
				void visit(std::string &output, const fx::nodes::call_expression_node *node) override
				{
					output += FixName(node->callee->unique_name) + '(';

					if (!node->arguments.empty())
					{
						for (size_t i = 0, count = node->arguments.size(); i < count; ++i)
						{
							const auto argument = node->arguments[i];
							const auto parameter = node->callee->parameter_list[i];

							const std::pair<std::string , std::string> cast = write_cast(argument->type, parameter->type);

							output += cast.first;

							visit(output, argument);

							output += cast.second + ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';

					auto &info = _functions.at(_current_function);
					auto &infoCallee = _functions.at(node->callee);
					
					for (auto dependency : infoCallee.dependencies)
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
				void visit(std::string &output, const fx::nodes::constructor_expression_node *node) override
				{
					if (node->type.is_matrix())
					{
						output += "transpose(";
					}

					visit(output, node->type, false);
					output += '(';

					if (!node->arguments.empty())
					{
						for (auto argument : node->arguments)
						{
							visit(output, argument);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';

					if (node->type.is_matrix())
					{
						output += ')';
					}
				}
				void visit(std::string &output, const fx::nodes::initializer_list_node *node) override
				{
					assert(false);
				}
				void visit(std::string &output, const fx::nodes::initializer_list_node *node, const fx::nodes::type_node &type)
				{
					visit(output, type, false);
					
					output += "[](";

					if (!node->values.empty())
					{
						for (auto expression : node->values)
						{
							if (expression->id == fx::nodeid::initializer_list)
							{
								visit(output, static_cast<fx::nodes::initializer_list_node *>(expression), node->type);
							}
							else
							{
								const auto cast = write_cast(expression->type, type);

								output += cast.first;
							
								visit(output, expression);
							
								output += cast.second;
							}

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::compound_statement_node *node) override
				{
					output += "{\n";

					for (auto statement : node->statement_list)
					{
						visit(output, statement);
					}

					output += "}\n";
				}
				void visit(std::string &output, const fx::nodes::declarator_list_node *node, bool single_statement = false) override
				{
					bool with_type = true;

					for (auto declarator : node->declarator_list)
					{
						visit(output, declarator, with_type);

						if (single_statement)
						{
							output += ", ";

							with_type = false;
						}
						else
						{
							output += ";\n";
						}
					}

					if (single_statement)
					{
						output.erase(output.end() - 2, output.end());

						output += ";\n";
					}
				}
				void visit(std::string &output, const fx::nodes::expression_statement_node *node) override
				{
					visit(output, node->expression);

					output += ";\n";
				}
				void visit(std::string &output, const fx::nodes::if_statement_node *node) override
				{
					const fx::nodes::type_node typeto = { fx::nodes::type_node::datatype_bool, 0, 1, 1 };
					const auto cast = write_cast(node->condition->type, typeto);

					output += "if (" + cast.first;

					visit(output, node->condition);

					output += cast.second + ")\n";

					if (node->statement_when_true != nullptr)
					{
						visit(output, node->statement_when_true);
					}
					else
					{
						output += "\t;";
					}

					if (node->statement_when_false != nullptr)
					{
						output += "else\n";

						visit(output, node->statement_when_false);
					}
				}
				void visit(std::string &output, const fx::nodes::switch_statement_node *node) override
				{
					output += "switch (";

					visit(output, node->test_expression);

					output += ")\n{\n";

					for (auto currcase : node->case_list)
					{
						visit(output, currcase);
					}

					output += "}\n";
				}
				void visit(std::string &output, const fx::nodes::case_statement_node *node) override
				{
					for (auto label : node->labels)
					{
						if (label == nullptr)
						{
							output += "default";
						}
						else
						{
							output += "case ";

							visit(output, label);
						}

						output += ":\n";
					}

					visit(output, node->statement_list);
				}
				void visit(std::string &output, const fx::nodes::for_statement_node *node) override
				{
					output += "for (";

					if (node->init_statement != nullptr)
					{
						if (node->init_statement->id == fx::nodeid::declarator_list)
						{
							visit(output, static_cast<fx::nodes::declarator_list_node *>(node->init_statement), true);

							output.erase(output.end() - 2, output.end());
						}
						else
						{
							visit(output, static_cast<fx::nodes::expression_statement_node *>(node->init_statement)->expression);
						}
					}

					output += "; ";

					if (node->condition != nullptr)
					{
						visit(output, node->condition);
					}

					output += "; ";

					if (node->increment_expression != nullptr)
					{
						visit(output, node->increment_expression);
					}

					output += ")\n";

					if (node->statement_list != nullptr)
					{
						visit(output, node->statement_list);
					}
					else
					{
						output += "\t;";
					}
				}
				void visit(std::string &output, const fx::nodes::while_statement_node *node) override
				{
					if (node->is_do_while)
					{
						output += "do\n{\n";

						if (node->statement_list != nullptr)
						{
							visit(output, node->statement_list);
						}

						output += "}\nwhile (";

						visit(output, node->condition);

						output += ");\n";
					}
					else
					{
						output += "while (";

						visit(output, node->condition);

						output += ")\n";

						if (node->statement_list != nullptr)
						{
							visit(output, node->statement_list);
						}
						else
						{
							output += "\t;";
						}
					}
				}
				void visit(std::string &output, const fx::nodes::return_statement_node *node) override
				{
					if (node->is_discard)
					{
						output += "discard";
					}
					else
					{
						output += "return";

						if (node->return_value != nullptr)
						{
							assert(_current_function != nullptr);

							const auto cast = write_cast(node->return_value->type, _current_function->return_type);

							output += ' ' + cast.first;

							visit(output, node->return_value);

							output += cast.second;
						}
					}

					output += ";\n";
				}
				void visit(std::string &output, const fx::nodes::jump_statement_node *node) override
				{
					if (node->is_break)
					{
						output += "break;\n";
					}
					else if (node->is_continue)
					{
						output += "continue;\n";
					}
				}
				void visit(std::string &output, const fx::nodes::struct_declaration_node *node)
				{
					output += "struct " + FixName(node->unique_name) + "\n{\n";

					if (!node->field_list.empty())
					{
						for (auto field : node->field_list)
						{
							visit(output, field, true);

							output += ";\n";
						}
					}
					else
					{
						output += "float _dummy;\n";
					}

					output += "};\n";
				}
				void visit(std::string &output, const fx::nodes::variable_declaration_node *node, bool with_type = true)
				{
					if (with_type)
					{
						visit(output, node->type);
					}

					output += ' ' + FixName(node->unique_name);

					if (node->type.is_array())
					{
						output += '[' + ((node->type.array_length >= 1) ? std::to_string(node->type.array_length) : "") + ']';
					}

					if (node->initializer_expression != nullptr)
					{
						output += " = ";

						if (node->initializer_expression->id == fx::nodeid::initializer_list)
						{
							visit(output, static_cast<fx::nodes::initializer_list_node *>(node->initializer_expression), node->type);
						}
						else
						{
							const auto cast = write_cast(node->initializer_expression->type, node->type);

							output += cast.first;
							
							visit(output, node->initializer_expression);
							
							output += cast.second;
						}
					}
				}
				void visit(std::string &output, const fx::nodes::function_declaration_node *node)
				{
					_current_function = node;

					visit(output, node->return_type, false);

					output += ' ' + FixName(node->unique_name) + '(';

					if (!node->parameter_list.empty())
					{
						for (auto parameter : node->parameter_list)
						{
							visit(output, parameter, true);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ")\n";

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
				void visit_texture(const fx::nodes::variable_declaration_node *node)
				{
					gl_texture *const obj = new gl_texture();
					obj->name = node->name;
					GLuint width = obj->width = node->properties.Width;
					GLuint height = obj->height = node->properties.Height;
					GLuint levels = obj->levels = node->properties.MipLevels;

					GLenum internalformat = GL_RGBA8, internalformatSRGB = GL_SRGB8_ALPHA8;
					LiteralToFormat(node->properties.Format, internalformat, internalformatSRGB, obj->format);

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

						obj->change_datatype(texture::datatype::backbuffer, _runtime->_backbuffer_texture[0], _runtime->_backbuffer_texture[1]);
					}
					else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
					{
						if (width != 1 || height != 1 || levels != 1 || internalformat != GL_RGBA8)
						{
							warning(node->location, "texture property on depthbuffer textures are ignored");
						}

						obj->change_datatype(texture::datatype::depthbuffer, _runtime->_depth_texture, 0);
					}
					else
					{
						GLCHECK(glGenTextures(2, obj->ID));

						GLint previous = 0, previousFBO = 0;
						GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));
						GLCHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFBO));

						GLCHECK(glBindTexture(GL_TEXTURE_2D, obj->ID[0]));
						GLCHECK(glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height));
						GLCHECK(glTextureView(obj->ID[1], GL_TEXTURE_2D, obj->ID[0], internalformatSRGB, 0, levels, 0, 1));
						GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

						// Clear texture to black
						GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _runtime->_blit_fbo));
						GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, obj->ID[0], 0));
						GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT1));
						const GLuint clearColor[4] = { 0, 0, 0, 0 };
						GLCHECK(glClearBufferuiv(GL_COLOR, 0, clearColor));
						GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, 0));
						GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousFBO));
					}

					_runtime->add_texture(obj);
				}
				void visit_sampler(const fx::nodes::variable_declaration_node *node)
				{
					if (node->properties.Texture == nullptr)
					{
						error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
						return;
					}

					gl_texture *const texture = static_cast<gl_texture *>(_runtime->find_texture(node->properties.Texture->name));

					if (texture == nullptr)
					{
						error(node->location, "texture not found");
						return;
					}

					gl_sampler sampler;
					sampler.ID = 0;
					sampler.Texture = texture;
					sampler.SRGB = node->properties.SRGBTexture;

					GLenum minfilter = LiteralToTextureFilter(node->properties.MinFilter);
					const GLenum mipfilter = LiteralToTextureFilter(node->properties.MipFilter);
				
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

					GLCHECK(glGenSamplers(1, &sampler.ID));
					GLCHECK(glSamplerParameteri(sampler.ID, GL_TEXTURE_WRAP_S, LiteralToTextureWrap(node->properties.AddressU)));
					GLCHECK(glSamplerParameteri(sampler.ID, GL_TEXTURE_WRAP_T, LiteralToTextureWrap(node->properties.AddressV)));
					GLCHECK(glSamplerParameteri(sampler.ID, GL_TEXTURE_WRAP_R, LiteralToTextureWrap(node->properties.AddressW)));
					GLCHECK(glSamplerParameteri(sampler.ID, GL_TEXTURE_MIN_FILTER, minfilter));
					GLCHECK(glSamplerParameteri(sampler.ID, GL_TEXTURE_MAG_FILTER, LiteralToTextureFilter(node->properties.MagFilter)));
					GLCHECK(glSamplerParameterf(sampler.ID, GL_TEXTURE_LOD_BIAS, node->properties.MipLODBias));
					GLCHECK(glSamplerParameterf(sampler.ID, GL_TEXTURE_MIN_LOD, node->properties.MinLOD));
					GLCHECK(glSamplerParameterf(sampler.ID, GL_TEXTURE_MAX_LOD, node->properties.MaxLOD));
					GLCHECK(glSamplerParameterf(sampler.ID, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<GLfloat>(node->properties.MaxAnisotropy)));

					_global_code += "layout(binding = " + std::to_string(_runtime->_effect_samplers.size()) + ") uniform sampler2D ";
					_global_code += FixName(node->unique_name);
					_global_code += ";\n";

					_runtime->_effect_samplers.push_back(std::move(sampler));
				}
				void visit_uniform(const fx::nodes::variable_declaration_node *node)
				{
					visit(_global_uniforms, node->type, true);

					_global_uniforms += ' ';			
					_global_uniforms += FixName(node->unique_name);

					if (node->type.is_array())
					{
						_global_uniforms += '[' + ((node->type.array_length >= 1) ? std::to_string(node->type.array_length) : "") + ']';
					}

					_global_uniforms += ";\n";

					uniform *const obj = new uniform();
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
				void visit_technique(const fx::nodes::technique_declaration_node *node)
				{
					gl_technique *const obj = new gl_technique();
					obj->name = node->name;
					obj->pass_count = static_cast<unsigned int>(node->pass_list.size());

					visit_annotation(node->annotation_list, *obj);

					for (auto pass : node->pass_list)
					{
						visit_pass(pass, obj->passes);
					}

					_runtime->add_technique(obj);
				}
				void visit_pass(const fx::nodes::pass_declaration_node *node, std::vector<gl_technique::Pass> &passes)
				{
					gl_technique::Pass pass;
					ZeroMemory(&pass, sizeof(gl_technique::Pass));
					pass.ColorMaskR = (node->states.RenderTargetWriteMask & (1 << 0)) != 0;
					pass.ColorMaskG = (node->states.RenderTargetWriteMask & (1 << 1)) != 0;
					pass.ColorMaskB = (node->states.RenderTargetWriteMask & (1 << 2)) != 0;
					pass.ColorMaskA = (node->states.RenderTargetWriteMask & (1 << 3)) != 0;
					pass.DepthTest = node->states.DepthEnable;
					pass.DepthMask = node->states.DepthWriteMask;
					pass.DepthFunc = LiteralToCompFunc(node->states.DepthFunc);
					pass.StencilTest = node->states.StencilEnable;
					pass.StencilReadMask = node->states.StencilReadMask;
					pass.StencilMask = node->states.StencilWriteMask;
					pass.StencilFunc = LiteralToCompFunc(node->states.StencilFunc);
					pass.StencilOpZPass = LiteralToStencilOp(node->states.StencilOpPass);
					pass.StencilOpFail = LiteralToStencilOp(node->states.StencilOpFail);
					pass.StencilOpZFail = LiteralToStencilOp(node->states.StencilOpDepthFail);
					pass.Blend = node->states.BlendEnable;
					pass.BlendEqColor = LiteralToBlendEq(node->states.BlendOp);
					pass.BlendEqAlpha = LiteralToBlendEq(node->states.BlendOpAlpha);
					pass.BlendFuncSrc = LiteralToBlendFunc(node->states.SrcBlend);
					pass.BlendFuncDest = LiteralToBlendFunc(node->states.DestBlend);
					pass.StencilRef = node->states.StencilRef;
					pass.FramebufferSRGB = node->states.SRGBWriteEnable;

					GLCHECK(glGenFramebuffers(1, &pass.Framebuffer));
					GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.Framebuffer));

					bool backbufferFramebuffer = true;

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->states.RenderTargets[i] == nullptr)
						{
							continue;
						}

						const gl_texture *const texture = static_cast<gl_texture *>(_runtime->find_texture(node->states.RenderTargets[i]->name));

						if (texture == nullptr)
						{
							error(node->location, "texture not found");
							return;
						}

						if (pass.ViewportWidth != 0 && pass.ViewportHeight != 0 && (texture->width != static_cast<unsigned int>(pass.ViewportWidth) || texture->height != static_cast<unsigned int>(pass.ViewportHeight)))
						{
							error(node->location, "cannot use multiple rendertargets with different sized textures");
							return;
						}
						else
						{
							pass.ViewportWidth = texture->width;
							pass.ViewportHeight = texture->height;
						}

						backbufferFramebuffer = false;

						GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->ID[pass.FramebufferSRGB], 0));

						pass.DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
						pass.DrawTextures[i] = texture->ID[pass.FramebufferSRGB];
					}

					if (backbufferFramebuffer)
					{
						GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[0]));

						pass.DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
						pass.DrawTextures[0] = _runtime->_backbuffer_texture[1];

						RECT rect;
						GetClientRect(WindowFromDC(_runtime->_hdc), &rect);

						pass.ViewportWidth = static_cast<GLsizei>(rect.right - rect.left);
						pass.ViewportHeight = static_cast<GLsizei>(rect.bottom - rect.top);
					}

					GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _runtime->_default_backbuffer_rbo[1]));

					assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

					GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

					GLuint shaders[2] = { 0, 0 };
					GLenum shaderTypes[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
					const fx::nodes::function_declaration_node *shaderFunctions[2] = { node->states.VertexShader, node->states.PixelShader };

					pass.Program = glCreateProgram();

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] != nullptr)
						{
							shaders[i] = glCreateShader(shaderTypes[i]);

							visit_pass_shader(shaderFunctions[i], shaderTypes[i], shaders[i]);

							GLCHECK(glAttachShader(pass.Program, shaders[i]));
						}
					}

					GLCHECK(glLinkProgram(pass.Program));

					for (unsigned int i = 0; i < 2; ++i)
					{
						GLCHECK(glDetachShader(pass.Program, shaders[i]));
						GLCHECK(glDeleteShader(shaders[i]));
					}

					GLint status = GL_FALSE;

					GLCHECK(glGetProgramiv(pass.Program, GL_LINK_STATUS, &status));

					if (status == GL_FALSE)
					{
						GLint logsize = 0;
						GLCHECK(glGetProgramiv(pass.Program, GL_INFO_LOG_LENGTH, &logsize));

						std::string log(logsize, '\0');
						GLCHECK(glGetProgramInfoLog(pass.Program, logsize, nullptr, &log.front()));

						GLCHECK(glDeleteProgram(pass.Program));

						_errors += log;
						error(node->location, "program linking failed");
						return;
					}

					passes.push_back(std::move(pass));
				}
				void visit_pass_shader(const fx::nodes::function_declaration_node *node, GLuint shadertype, GLuint &shader)
				{
					std::string source =
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
						"#define _textureLodOffset(s, c, offset) textureLodOffset(s, (c).xy, (c).w, offset)\n";

					if (!_global_uniforms.empty())
					{
						source += "layout(std140, binding = 0) uniform _GLOBAL_\n{\n" + _global_uniforms + "};\n";
					}

					if (shadertype != GL_FRAGMENT_SHADER)
					{
						source += "#define discard\n";
					}

					source += _global_code;

					for (auto dependency : _functions.at(node).dependencies)
					{
						source += _functions.at(dependency).code;
					}

					source += _functions.at(node).code;

					for (auto parameter : node->parameter_list)
					{
						if (parameter->type.is_struct())
						{
							for (auto field : parameter->type.definition->field_list)
							{
								visitShaderParameter(source, field->type, parameter->type.qualifiers, "_param_" + parameter->name + "_" + field->name, field->semantic, shadertype);
							}
						}
						else
						{
							visitShaderParameter(source, parameter->type, parameter->type.qualifiers, "_param_" + parameter->name, parameter->semantic, shadertype);
						}
					}

					if (node->return_type.is_struct())
					{
						for (auto field : node->return_type.definition->field_list)
						{
							visitShaderParameter(source, field->type, fx::nodes::type_node::qualifier_out, "_return_" + field->name, field->semantic, shadertype);
						}
					}
					else if (!node->return_type.is_void())
					{
						visitShaderParameter(source, node->return_type, fx::nodes::type_node::qualifier_out, "_return", node->return_semantic, shadertype);
					}

					source += "void main()\n{\n";

					for (auto parameter : node->parameter_list)
					{
						for (int i = 0, arraylength = std::max(1, parameter->type.array_length); i < arraylength; ++i)
						{
							if (parameter->type.is_struct())
							{
								visit(source, parameter->type, false);

								source += " _param_" + parameter->name + (parameter->type.is_array() ? std::to_string(i) : "") + " = ";

								visit(source, parameter->type, false);

								source += "(";

								if (!parameter->type.definition->field_list.empty())
								{
									for (auto field : parameter->type.definition->field_list)
									{
										source += FixNameWithSemantic("_param_" + parameter->name + "_" + field->name + (parameter->type.is_array() ? std::to_string(i) : ""), field->semantic, shadertype) + ", ";
									}

									source.erase(source.end() - 2, source.end());
								}

								source += ");\n";
							}
							else if (boost::starts_with(parameter->semantic, "COLOR") || boost::starts_with(parameter->semantic, "SV_TARGET"))
							{
								source += " _param_" + parameter->name + (parameter->type.is_array() ? std::to_string(i) : "") + " = vec4(0, 0, 0, 1);\n";
							}
						}

						if (parameter->type.is_array())
						{
							visit(source, parameter->type, false);

							source += " _param_" + parameter->name + "[] = ";

							visit(source, parameter->type, false);

							source += "[](";

							for (int i = 0; i < parameter->type.array_length; ++i)
							{
								source += "_param_" + parameter->name + std::to_string(i) + ", ";
							}

							source.erase(source.end() - 2, source.end());

							source += ");\n";
						}
					}

					if (node->return_type.is_struct())
					{
						visit(source, node->return_type, false);

						source += ' ';
					}

					if (!node->return_type.is_void())
					{
						source += "_return = ";

						if ((boost::starts_with(node->return_semantic, "COLOR") || boost::starts_with(node->return_semantic, "SV_TARGET")) && node->return_type.rows < 4)
						{
							const std::string swizzle[3] = { "x", "xy", "xyz" };

							source += "vec4(0, 0, 0, 1);\n_return." + swizzle[node->return_type.rows - 1] + " = ";
						}
					}

					source += FixName(node->unique_name) + '(';

					if (!node->parameter_list.empty())
					{
						for (auto parameter : node->parameter_list)
						{
							source += FixNameWithSemantic("_param_" + parameter->name, parameter->semantic, shadertype);

							if ((boost::starts_with(parameter->semantic, "COLOR") || boost::starts_with(parameter->semantic, "SV_TARGET")) && parameter->type.rows < 4)
							{
								const std::string swizzle[3] = { "x", "xy", "xyz" };

								source += "." + swizzle[parameter->type.rows - 1];
							}

							source += ", ";
						}

						source.erase(source.end() - 2, source.end());
					}

					source += ");\n";

					for (auto parameter : node->parameter_list)
					{
						if (!parameter->type.has_qualifier(fx::nodes::type_node::qualifier_out))
						{
							continue;
						}

						if (parameter->type.is_array())
						{
							for (int i = 0; i < parameter->type.array_length; ++i)
							{
								source += "_param_" + parameter->name + std::to_string(i) + " = _param_" + parameter->name + "[" + std::to_string(i) + "];\n";
							}
						}

						for (int i = 0; i < std::max(1, parameter->type.array_length); ++i)
						{
							if (parameter->type.is_struct())
							{
								for (auto field : parameter->type.definition->field_list)
								{
									source += "_param_" + parameter->name + "_" + field->name + (parameter->type.is_array() ? std::to_string(i) : "") + " = " + "_param_" + parameter->name + "." + field->name + (parameter->type.is_array() ? "[" + std::to_string(i) + "]" : "") + ";\n";
								}
							}
						}
					}

					if (node->return_type.is_struct())
					{
						for (auto field : node->return_type.definition->field_list)
						{
							source += FixNameWithSemantic("_return_" + field->name, field->semantic, shadertype) + " = _return." + field->name + ";\n";
						}
					}
			
					if (shadertype == GL_VERTEX_SHADER)
					{
						source += "gl_Position = gl_Position * vec4(1.0, 1.0, 2.0, 1.0) + vec4(0.0, 0.0, -gl_Position.w, 0.0);\n";
					}

					source += "}\n";

					LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source.c_str() << "\n";

					GLint status = GL_FALSE;
					const GLchar *src = source.c_str();
					const GLsizei len = static_cast<GLsizei>(source.length());

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
				void visitShaderParameter(std::string &source, fx::nodes::type_node type, unsigned int qualifier, const std::string &name, const std::string &semantic, GLuint shadertype)
				{
					type.qualifiers = static_cast<unsigned int>(qualifier);

					unsigned int location = 0;

					for (int i = 0, arraylength = std::max(1, type.array_length); i < arraylength; ++i)
					{
						if (!FixNameWithSemantic(std::string(), semantic, shadertype).empty())
						{
							continue;
						}
						else if (boost::starts_with(semantic, "COLOR"))
						{
							type.rows = 4;

							location = static_cast<unsigned int>(::strtol(semantic.c_str() + 5, nullptr, 10));
						}
						else if (boost::starts_with(semantic, "TEXCOORD"))
						{
							location = static_cast<unsigned int>(::strtol(semantic.c_str() + 8, nullptr, 10)) + 1;
						}
						else if (boost::starts_with(semantic, "SV_TARGET"))
						{
							type.rows = 4;

							location = static_cast<unsigned int>(::strtol(semantic.c_str() + 9, nullptr, 10));
						}

						source += "layout(location = " + std::to_string(location + i) + ") ";

						visit(source, type, true);

						source += ' ' + name;

						if (type.is_array())
						{
							source += std::to_string(i);
						}

						source += ";\n";
					}
				}

				struct function
				{
					std::string code;
					std::vector<const fx::nodes::function_declaration_node *> dependencies;
				};

				gl_runtime *_runtime;
				std::string _global_code, _global_uniforms;
				size_t _current_uniform_size;
				const fx::nodes::function_declaration_node *_current_function;
				std::unordered_map<const fx::nodes::function_declaration_node *, function> _functions;
			};

			GLenum target_to_binding(GLenum target)
			{
				switch (target)
				{
					case GL_FRAMEBUFFER:
						return GL_FRAMEBUFFER_BINDING;
					case GL_READ_FRAMEBUFFER:
						return GL_READ_FRAMEBUFFER_BINDING;
					case GL_DRAW_FRAMEBUFFER:
						return GL_DRAW_FRAMEBUFFER_BINDING;
					case GL_RENDERBUFFER:
						return GL_RENDERBUFFER_BINDING;
					case GL_TEXTURE_2D:
						return GL_TEXTURE_BINDING_2D;
					case GL_TEXTURE_2D_ARRAY:
						return GL_TEXTURE_BINDING_2D_ARRAY;
					case GL_TEXTURE_2D_MULTISAMPLE:
						return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
					case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
						return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
					case GL_TEXTURE_3D:
						return GL_TEXTURE_BINDING_3D;
					case GL_TEXTURE_CUBE_MAP:
					case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
					case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
					case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
					case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
					case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
					case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
						return GL_TEXTURE_BINDING_CUBE_MAP;
					case GL_TEXTURE_CUBE_MAP_ARRAY:
						return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
					default:
						return GL_NONE;
				}
			}
			inline void flip_bc1_block(unsigned char *block)
			{
				// BC1 Block:
				//  [0-1]  color 0
				//  [2-3]  color 1
				//  [4-7]  color indices

				std::swap(block[4], block[7]);
				std::swap(block[5], block[6]);
			}
			inline void flip_bc2_block(unsigned char *block)
			{
				// BC2 Block:
				//  [0-7]  alpha indices
				//  [8-15] color block

				std::swap(block[0], block[6]);
				std::swap(block[1], block[7]);
				std::swap(block[2], block[4]);
				std::swap(block[3], block[5]);

				flip_bc1_block(block + 8);
			}
			inline void flip_bc4_block(unsigned char *block)
			{
				// BC4 Block:
				//  [0]    red 0
				//  [1]    red 1
				//  [2-7]  red indices

				const unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
				const unsigned int line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
				const unsigned int line_1_0 = ((line_0_1 & 0x000FFF) << 12) | ((line_0_1 & 0xFFF000) >> 12);
				const unsigned int line_3_2 = ((line_2_3 & 0x000FFF) << 12) | ((line_2_3 & 0xFFF000) >> 12);
				block[2] = static_cast<unsigned char>((line_3_2 & 0xFF));
				block[3] = static_cast<unsigned char>((line_3_2 & 0xFF00) >> 8);
				block[4] = static_cast<unsigned char>((line_3_2 & 0xFF0000) >> 16);
				block[5] = static_cast<unsigned char>((line_1_0 & 0xFF));
				block[6] = static_cast<unsigned char>((line_1_0 & 0xFF00) >> 8);
				block[7] = static_cast<unsigned char>((line_1_0 & 0xFF0000) >> 16);
			}
			inline void flip_bc3_block(unsigned char *block)
			{
				// BC3 Block:
				//  [0-7]  alpha block
				//  [8-15] color block

				flip_bc4_block(block);
				flip_bc1_block(block + 8);
			}
			inline void flip_bc5_block(unsigned char *block)
			{
				// BC5 Block:
				//  [0-7]  red block
				//  [8-15] green block

				flip_bc4_block(block);
				flip_bc4_block(block + 8);
			}
			void flip_image_data(const texture *texture, unsigned char *data)
			{
				typedef void (*FlipBlockFunc)(unsigned char *block);

				size_t blocksize = 0;
				bool compressed = false;
				FlipBlockFunc compressedFunc = nullptr;

				switch (texture->format)
				{
					case texture::pixelformat::r8:
						blocksize = 1;
						break;
					case texture::pixelformat::rg8:
						blocksize = 2;
						break;
					case texture::pixelformat::r32f:
					case texture::pixelformat::rgba8:
						blocksize = 4;
						break;
					case texture::pixelformat::rgba16:
					case texture::pixelformat::rgba16f:
						blocksize = 8;
						break;
					case texture::pixelformat::rgba32f:
						blocksize = 16;
						break;
					case texture::pixelformat::dxt1:
						blocksize = 8;
						compressed = true;
						compressedFunc = &flip_bc1_block;
						break;
					case texture::pixelformat::dxt3:
						blocksize = 16;
						compressed = true;
						compressedFunc = &flip_bc2_block;
						break;
					case texture::pixelformat::dxt5:
						blocksize = 16;
						compressed = true;
						compressedFunc = &flip_bc3_block;
						break;
					case texture::pixelformat::latc1:
						blocksize = 8;
						compressed = true;
						compressedFunc = &flip_bc4_block;
						break;
					case texture::pixelformat::latc2:
						blocksize = 16;
						compressed = true;
						compressedFunc = &flip_bc5_block;
						break;
					default:
						return;
				}

				if (compressed)
				{
					const size_t w = (texture->width + 3) / 4;
					const size_t h = (texture->height + 3) / 4;
					const size_t stride = w * blocksize;

					for (size_t y = 0; y < h; ++y)
					{
						unsigned char *dataLine = data + stride * (h - 1 - y);

						for (size_t x = 0; x < stride; x += blocksize)
						{
							compressedFunc(dataLine + x);
						}
					}
				}
				else
				{
					const size_t w = texture->width;
					const size_t h = texture->height;
					const size_t stride = w * blocksize;
					unsigned char *templine = static_cast<unsigned char *>(::alloca(stride));

					for (size_t y = 0; 2 * y < h; ++y)
					{
						unsigned char *line1 = data + stride * y;
						unsigned char *line2 = data + stride * (h - 1 - y);

						std::memcpy(templine, line1, stride);
						std::memcpy(line1, line2, stride);
						std::memcpy(line2, templine, stride);
					}
				}
			}

			unsigned int get_renderer_id()
			{
				GLint major = 0, minor = 0;
				GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &major));
				GLCHECK(glGetIntegerv(GL_MAJOR_VERSION, &minor));

				return 0x10000 | (major << 12) | (minor << 8);
			}
		}

		// ---------------------------------------------------------------------------------------------------

		gl_runtime::gl_runtime(HDC device) : runtime(get_renderer_id()), _hdc(device), _reference_count(1), _default_backbuffer_fbo(0), _default_backbuffer_rbo(), _backbuffer_texture(), _depth_source_fbo(0), _depth_source(0), _depth_texture(0), _blit_fbo(0), _default_vao(0), _default_vbo(0), _effect_ubo(0)
		{
			_vendor_id = 0;
			_device_id = 0;
			input::register_window(WindowFromDC(_hdc), _input);

			// Get vendor and device information on NVIDIA Optimus devices
			if (GetModuleHandleA("nvd3d9wrap.dll") == nullptr && GetModuleHandleA("nvd3d9wrapx.dll") == nullptr)
			{
				DISPLAY_DEVICEA dd;
				dd.cb = sizeof(DISPLAY_DEVICEA);

				for (DWORD i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0) != FALSE; ++i)
				{
					if ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0)
					{
						const std::string id = dd.DeviceID;

						if (id.length() > 20)
						{
							_vendor_id = std::stoi(id.substr(8, 4), nullptr, 16);
							_device_id = std::stoi(id.substr(17, 4), nullptr, 16);
						}
						break;
					}
				}
			}

			// Get vendor and device information on general devices
			if (_vendor_id == 0)
			{
				const char *const name = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

				if (name != nullptr)
				{
					if (boost::contains(name, "NVIDIA"))
					{
						_vendor_id = 0x10DE;
					}
					else if (boost::contains(name, "AMD") || boost::contains(name, "ATI"))
					{
						_vendor_id = 0x1002;
					}
					else if (boost::contains(name, "Intel"))
					{
						_vendor_id = 0x8086;
					}
				}
			}
		}

		bool gl_runtime::on_init(unsigned int width, unsigned int height)
		{
			assert(width != 0 && height != 0);

			_width = width;
			_height = height;

			// Clear errors
			GLenum status = glGetError();

			_stateblock.capture();

			#pragma region Generate backbuffer targets
			GLCHECK(glGenRenderbuffers(2, _default_backbuffer_rbo));

			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[0]));
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, _default_backbuffer_rbo[1]));
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

			status = glGetError();

			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

			if (status != GL_NO_ERROR)
			{
				LOG(TRACE) << "Failed to create backbuffer renderbuffer with error code " << status;

				GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

				return false;
			}

			GLCHECK(glGenFramebuffers(1, &_default_backbuffer_fbo));

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo));
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _default_backbuffer_rbo[0]));
			GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _default_backbuffer_rbo[1]));

			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				LOG(TRACE) << "Failed to create backbuffer framebuffer object with status code " << status;

				GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
				GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

				return false;
			}

			GLCHECK(glGenTextures(2, _backbuffer_texture));

			GLCHECK(glBindTexture(GL_TEXTURE_2D, _backbuffer_texture[0]));
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
			glTextureView(_backbuffer_texture[1], GL_TEXTURE_2D, _backbuffer_texture[0], GL_SRGB8_ALPHA8, 0, 1, 0, 1);
		
			status = glGetError();

			GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

			if (status != GL_NO_ERROR)
			{
				LOG(TRACE) << "Failed to create backbuffer texture with error code " << status;

				GLCHECK(glDeleteTextures(2, _backbuffer_texture));
				GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
				GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

				return false;
			}
			#pragma endregion

			#pragma region Generate depthbuffer targets
			const depth_source_info defaultdepth = { static_cast<GLint>(width), static_cast<GLint>(height), 0, GL_DEPTH24_STENCIL8 };

			_depth_source_table[0] = defaultdepth;

			LOG(TRACE) << "Switched depth source to default depthstencil.";

			GLCHECK(glGenTextures(1, &_depth_texture));

			GLCHECK(glBindTexture(GL_TEXTURE_2D, _depth_texture));
			glTexStorage2D(GL_TEXTURE_2D, 1, defaultdepth.format, defaultdepth.width, defaultdepth.height);

			status = glGetError();

			GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

			if (status != GL_NO_ERROR)
			{
				LOG(TRACE) << "Failed to create depth texture with error code " << status;

				GLCHECK(glDeleteTextures(1, &_depth_texture));
				GLCHECK(glDeleteTextures(2, _backbuffer_texture));
				GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
				GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

				return false;
			}
			#pragma endregion

			GLCHECK(glGenFramebuffers(1, &_blit_fbo));

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo));
			GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0));
			GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _backbuffer_texture[1], 0));

			status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

			if (status != GL_FRAMEBUFFER_COMPLETE)
			{
				LOG(TRACE) << "Failed to create blit framebuffer object with status code " << status;

				GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
				GLCHECK(glDeleteTextures(1, &_depth_texture));
				GLCHECK(glDeleteTextures(2, _backbuffer_texture));
				GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
				GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));

				return false;
			}


			GLCHECK(glGenVertexArrays(1, &_default_vao));
			GLCHECK(glGenBuffers(1, &_default_vbo));

			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, _default_vbo));
			GLCHECK(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW));
			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));


			_gui.reset(new gui(this, nvgCreateGL3(0)));

			_stateblock.apply();

			return runtime::on_init();
		}
		void gl_runtime::on_reset()
		{
			if (!_is_initialized)
			{
				return;
			}

			runtime::on_reset();

			// Destroy NanoVG
			NVGcontext *const nvg = _gui->context();

			_gui.reset();

			nvgDeleteGL3(nvg);

			// Destroy resources
			GLCHECK(glDeleteBuffers(1, &_default_vbo));
			GLCHECK(glDeleteBuffers(1, &_effect_ubo));
			GLCHECK(glDeleteVertexArrays(1, &_default_vao));
			GLCHECK(glDeleteFramebuffers(1, &_default_backbuffer_fbo));
			GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
			GLCHECK(glDeleteFramebuffers(1, &_blit_fbo));
			GLCHECK(glDeleteRenderbuffers(2, _default_backbuffer_rbo));
			GLCHECK(glDeleteTextures(2, _backbuffer_texture));
			GLCHECK(glDeleteTextures(1, &_depth_texture));

			_default_vbo = 0;
			_effect_ubo = 0;
			_default_vao = 0;
			_default_backbuffer_fbo = 0;
			_depth_source_fbo = 0;
			_blit_fbo = 0;
			_default_backbuffer_rbo[0] = 0;
			_default_backbuffer_rbo[1] = 0;
			_backbuffer_texture[0] = 0;
			_backbuffer_texture[1] = 0;
			_depth_texture = 0;

			_depth_source = 0;
		}
		void gl_runtime::on_reset_effect()
		{
			runtime::on_reset_effect();

			for (auto &sampler : _effect_samplers)
			{
				GLCHECK(glDeleteSamplers(1, &sampler.ID));
			}

			_effect_samplers.clear();
		}
		void gl_runtime::on_present()
		{
			if (!_is_initialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (_drawcalls == 0)
			{
				return;
			}

			detect_depth_source();

			// Capture states
			_stateblock.capture();

			// Copy backbuffer
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _default_backbuffer_fbo));
			GLCHECK(glReadBuffer(GL_BACK));
			GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
			GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

			// Copy depthbuffer
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _depth_source_fbo));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo));
			GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_DEPTH_BUFFER_BIT, GL_NEAREST));

			// Apply post processing
			on_apply_effect();

			glDisable(GL_FRAMEBUFFER_SRGB);

			// Reset rendertarget and copy to backbuffer
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
			GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
			GLCHECK(glDrawBuffer(GL_BACK));
			GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
			GLCHECK(glViewport(0, 0, _width, _height));

			// Apply presenting
			runtime::on_present();

			// Apply states
			_stateblock.apply();
		}
		void gl_runtime::on_draw_call(unsigned int vertices)
		{
			runtime::on_draw_call(vertices);

			GLint fbo = 0, object = 0, objecttarget = GL_NONE;
			GLCHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo));

			if (fbo != 0)
			{
				GLCHECK(glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objecttarget));

				if (objecttarget == GL_NONE)
				{
					return;
				}
				else
				{
					GLCHECK(glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object));
				}
			}

			const auto it = _depth_source_table.find(object | (objecttarget == GL_RENDERBUFFER ? 0x80000000 : 0));

			if (it != _depth_source_table.end())
			{
				it->second.drawcall_count = static_cast<GLfloat>(_drawcalls);
				it->second.vertices_count += vertices;
			}
		}
		void gl_runtime::on_apply_effect()
		{
			if (!_is_effect_compiled)
			{
				return;
			}

			// Setup vertex input
			GLCHECK(glBindVertexArray(_default_vao));
			GLCHECK(glBindVertexBuffer(0, _default_vbo, 0, sizeof(float)));

			// Setup shader resources
			for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
			{
				GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, _effect_samplers[sampler].Texture->ID[_effect_samplers[sampler].SRGB]));
				GLCHECK(glBindSampler(sampler, _effect_samplers[sampler].ID));
			}

			// Setup shader constants
			GLCHECK(glBindBufferBase(GL_UNIFORM_BUFFER, 0, _effect_ubo));

			// Apply post processing
			runtime::on_apply_effect();

			// Reset states
			GLCHECK(glBindSampler(0, 0));
		}
		void gl_runtime::on_apply_effect_technique(const technique *technique)
		{
			runtime::on_apply_effect_technique(technique);

			// Clear depthstencil
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _default_backbuffer_fbo));
			GLCHECK(glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0));

			// Update shader constants
			if (_effect_ubo != 0)
			{
				auto &uniform_storage = get_uniform_value_storage();
				GLCHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, uniform_storage.size(), uniform_storage.data()));
			}

			for (const auto &pass : static_cast<const gl_technique *>(technique)->passes)
			{
				// Setup states
				GLCHECK(glUseProgram(pass.Program));
				GLCHECK(pass.FramebufferSRGB ? glEnable(GL_FRAMEBUFFER_SRGB) : glDisable(GL_FRAMEBUFFER_SRGB));
				GLCHECK(glDisable(GL_SCISSOR_TEST));
				GLCHECK(glFrontFace(GL_CCW));
				GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
				GLCHECK(glDisable(GL_CULL_FACE));
				GLCHECK(glColorMask(pass.ColorMaskR, pass.ColorMaskG, pass.ColorMaskB, pass.ColorMaskA));
				GLCHECK(pass.Blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND));
				GLCHECK(glBlendFunc(pass.BlendFuncSrc, pass.BlendFuncDest));
				GLCHECK(glBlendEquationSeparate(pass.BlendEqColor, pass.BlendEqAlpha));
				GLCHECK(pass.DepthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));
				GLCHECK(glDepthMask(pass.DepthMask));
				GLCHECK(glDepthFunc(pass.DepthFunc));
				GLCHECK(pass.StencilTest ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST));
				GLCHECK(glStencilFunc(pass.StencilFunc, pass.StencilRef, pass.StencilReadMask));
				GLCHECK(glStencilOp(pass.StencilOpFail, pass.StencilOpZFail, pass.StencilOpZPass));
				GLCHECK(glStencilMask(pass.StencilMask));

				// Save backbuffer of previous pass
				GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, _default_backbuffer_fbo));
				GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _blit_fbo));
				GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
				GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
				GLCHECK(glBlitFramebuffer(0, 0, _width, _height, 0, 0, _width, _height, GL_COLOR_BUFFER_BIT, GL_NEAREST));

				// Setup rendertargets
				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.Framebuffer));
				GLCHECK(glDrawBuffers(8, pass.DrawBuffers));
				GLCHECK(glViewport(0, 0, pass.ViewportWidth, pass.ViewportHeight));

				for (GLuint k = 0; k < 8; k++)
				{
					if (pass.DrawBuffers[k] == GL_NONE)
					{
						continue;
					}

					const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					GLCHECK(glClearBufferfv(GL_COLOR, k, color));
				}

				// Draw triangle
				GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 3));

				// Update shader resources
				for (GLuint id : pass.DrawTextures)
				{
					for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(_effect_samplers.size()); sampler < samplerCount; sampler++)
					{
						const gl_texture *const texture = _effect_samplers[sampler].Texture;

						if (texture->levels > 1 && (texture->ID[0] == id || texture->ID[1] == id))
						{
							GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
							GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
						}
					}
				}
			}
		}

		void gl_runtime::on_fbo_attachment(GLenum target, GLenum attachment, GLenum objecttarget, GLuint object, GLint level)
		{
			if (object == 0 || (attachment != GL_DEPTH_ATTACHMENT && attachment != GL_DEPTH_STENCIL_ATTACHMENT))
			{
				return;
			}

			// Get current framebuffer
			GLint fbo = 0;
			GLCHECK(glGetIntegerv(target_to_binding(target), &fbo));

			assert(fbo != 0);

			if (static_cast<GLuint>(fbo) == _default_backbuffer_fbo || static_cast<GLuint>(fbo) == _depth_source_fbo || static_cast<GLuint>(fbo) == _blit_fbo)
			{
				return;
			}

			const GLuint id = object | (objecttarget == GL_RENDERBUFFER ? 0x80000000 : 0);
		
			if (_depth_source_table.find(id) != _depth_source_table.end())
			{
				return;
			}

			depth_source_info info = { 0, 0, 0, GL_NONE };

			if (objecttarget == GL_RENDERBUFFER)
			{
				GLint previous = 0;
				GLCHECK(glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous));

				// Get depthstencil parameters from renderbuffer
				GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, object));
				GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &info.width));
				GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &info.height));
				GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &info.format));

				GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, previous));
			}
			else
			{
				if (objecttarget == GL_TEXTURE)
				{
					objecttarget = GL_TEXTURE_2D;
				}

				GLint previous = 0;
				GLCHECK(glGetIntegerv(target_to_binding(objecttarget), &previous));

				// Get depthstencil parameters from texture
				GLCHECK(glBindTexture(objecttarget, object));
				info.level = level;
				GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_WIDTH, &info.width));
				GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_HEIGHT, &info.height));
				GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_INTERNAL_FORMAT, &info.format));
			
				GLCHECK(glBindTexture(objecttarget, previous));
			}

			LOG(TRACE) << "Adding framebuffer " << fbo << " attachment " << object << " (Attachment Type: " << attachment << ", Object Type: " << objecttarget << ", Width: " << info.width << ", Height: " << info.height << ", Format: " << info.format << ") to list of possible depth candidates ...";

			_depth_source_table.emplace(id, info);
		}

		void gl_runtime::screenshot(unsigned char *buffer) const
		{
			GLCHECK(glReadBuffer(GL_BACK));
			GLCHECK(glReadPixels(0, 0, static_cast<GLsizei>(_width), static_cast<GLsizei>(_height), GL_RGBA, GL_UNSIGNED_BYTE, buffer));

			// Flip image
			const unsigned int pitch = _width * 4;

			for (unsigned int y = 0; y * 2 < _height; ++y)
			{
				const unsigned int i1 = y * pitch;
				const unsigned int i2 = (_height - 1 - y) * pitch;

				for (unsigned int x = 0; x < pitch; x += 4)
				{
					buffer[i1 + x + 3] = 0xFF;
					buffer[i2 + x + 3] = 0xFF;

					std::swap(buffer[i1 + x + 0], buffer[i2 + x + 0]);
					std::swap(buffer[i1 + x + 1], buffer[i2 + x + 1]);
					std::swap(buffer[i1 + x + 2], buffer[i2 + x + 2]);
				}
			}
		}
		bool gl_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &/*pragmas*/, std::string &errors)
		{
			return gl_fx_compiler(this, ast, errors).run();
		}
		bool gl_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
		{
			gl_texture *const textureImpl = dynamic_cast<gl_texture *>(texture);

			assert(textureImpl != nullptr);
			assert(data != nullptr && size > 0);

			if (textureImpl->basetype != texture::datatype::image)
			{
				return false;
			}

			GLCHECK(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
			GLCHECK(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0));
			GLCHECK(glPixelStorei(GL_UNPACK_SKIP_ROWS, 0));
			GLCHECK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0));

			GLint previous = 0;
			GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

			// Copy image data
			const std::unique_ptr<unsigned char[]> dataFlipped(new unsigned char[size]);
			std::memcpy(dataFlipped.get(), data, size);

			// Flip image data vertically
			flip_image_data(texture, dataFlipped.get());

			// Bind and update texture
			GLCHECK(glBindTexture(GL_TEXTURE_2D, textureImpl->ID[0]));

			if (texture->format >= texture::pixelformat::dxt1 && texture->format <= texture::pixelformat::latc2)
			{
				GLCHECK(glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), dataFlipped.get()));
			}
			else
			{
				GLint dataAlignment = 4;
				GLenum dataFormat = GL_RGBA, dataType = GL_UNSIGNED_BYTE;

				switch (texture->format)
				{
					case texture::pixelformat::r8:
						dataFormat = GL_RED;
						dataAlignment = 1;
						break;
					case texture::pixelformat::r16f:
						dataType = GL_UNSIGNED_SHORT;
						dataFormat = GL_RED;
						dataAlignment = 2;
						break;
					case texture::pixelformat::r32f:
						dataType = GL_FLOAT;
						dataFormat = GL_RED;
						break;
					case texture::pixelformat::rg8:
						dataFormat = GL_RG;
						dataAlignment = 2;
						break;
					case texture::pixelformat::rg16:
					case texture::pixelformat::rg16f:
						dataType = GL_UNSIGNED_SHORT;
						dataFormat = GL_RG;
						dataAlignment = 2;
						break;
					case texture::pixelformat::rg32f:
						dataType = GL_FLOAT;
						dataFormat = GL_RG;
						break;
					case texture::pixelformat::rgba16:
					case texture::pixelformat::rgba16f:
						dataType = GL_UNSIGNED_SHORT;
						dataAlignment = 2;
						break;
					case texture::pixelformat::rgba32f:
						dataType = GL_FLOAT;
						break;
				}

				GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, dataAlignment));
				GLCHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, dataFormat, dataType, dataFlipped.get()));
				GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
			}

			if (texture->levels > 1)
			{
				GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));
			}

			GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

			return true;
		}

		void gl_runtime::detect_depth_source()
		{
			static int cooldown = 0, traffic = 0;

			if (cooldown-- > 0)
			{
				traffic += s_network_upload > 0;
				return;
			}
			else
			{
				cooldown = 30;

				if (traffic > 10)
				{
					traffic = 0;
					_depth_source = 0;
					create_depth_texture(0, 0, GL_NONE);
					return;
				}
				else
				{
					traffic = 0;
				}
			}

			GLuint best_match = 0;
			depth_source_info best_info = { 0, 0, 0, GL_NONE };

			for (auto &it : _depth_source_table)
			{
				if (it.second.drawcall_count == 0)
				{
					continue;
				}
				else if (((it.second.vertices_count * (1.2f - it.second.drawcall_count / _drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _drawcalls))) && ((it.second.width > _width * 0.95 && it.second.width < _width * 1.05) && (it.second.height > _height * 0.95 && it.second.height < _height * 1.05)))
				{
					best_match = it.first;
					best_info = it.second;
				}

				it.second.drawcall_count = it.second.vertices_count = 0;
			}

			if (best_match == 0)
			{
				best_info = _depth_source_table.at(0);
			}

			if (_depth_source != best_match || _depth_texture == 0)
			{
				const depth_source_info &previousInfo = _depth_source_table.at(_depth_source);

				if ((best_info.width != previousInfo.width || best_info.height != previousInfo.height || best_info.format != previousInfo.format) || _depth_texture == 0)
				{
					// Resize depth texture
					create_depth_texture(best_info.width, best_info.height, best_info.format);
				}

				GLint previousFBO = 0;
				GLCHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO));

				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _blit_fbo));
				GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _depth_texture, 0));

				assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

				_depth_source = best_match;

				if (best_match != 0)
				{
					if (_depth_source_fbo == 0)
					{
						GLCHECK(glGenFramebuffers(1, &_depth_source_fbo));
					}

					GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, _depth_source_fbo));

					if ((best_match & 0x80000000) != 0)
					{
						best_match ^= 0x80000000;

						LOG(TRACE) << "Switched depth source to renderbuffer " << best_match << ".";

						GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, best_match));
					}
					else
					{
						LOG(TRACE) << "Switched depth source to texture " << best_match << ".";

						GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, best_match, best_info.level));
					}

					const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

					if (status != GL_FRAMEBUFFER_COMPLETE)
					{
						LOG(TRACE) << "Failed to create depth source framebuffer with status code " << status << ".";

						GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
						_depth_source_fbo = 0;
					}
				}
				else
				{
					LOG(TRACE) << "Switched depth source to default framebuffer.";

					GLCHECK(glDeleteFramebuffers(1, &_depth_source_fbo));
					_depth_source_fbo = 0;
				}

				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, previousFBO));
			}
		}
		void gl_runtime::create_depth_texture(GLuint width, GLuint height, GLenum format)
		{
			GLCHECK(glDeleteTextures(1, &_depth_texture));

			if (format != GL_NONE)
			{
				GLint previousTex = 0;
				GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex));

				// Clear errors
				GLenum status = glGetError();

				GLCHECK(glGenTextures(1, &_depth_texture));

				if (format == GL_DEPTH_STENCIL)
				{
					format = GL_UNSIGNED_INT_24_8;
				}

				GLCHECK(glBindTexture(GL_TEXTURE_2D, _depth_texture));
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				status = glGetError();

				if (status != GL_NO_ERROR)
				{
					LOG(ERROR) << "Failed to create depth texture for format " << format << " with error code " << status;

					GLCHECK(glDeleteTextures(1, &_depth_texture));

					_depth_texture = 0;
				}

				GLCHECK(glBindTexture(GL_TEXTURE_2D, previousTex));
			}
			else
			{
				_depth_texture = 0;
			}

			// Update effect textures
			for (const auto &it : _textures)
			{
				gl_texture *texture = static_cast<gl_texture *>(it.get());

				if (texture->basetype == texture::datatype::depthbuffer)
				{
					texture->change_datatype(texture::datatype::depthbuffer, _depth_texture, 0);
				}
			}
		}
	}
}
