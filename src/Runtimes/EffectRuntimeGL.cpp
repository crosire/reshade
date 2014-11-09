#include "Log.hpp"
#include "EffectParserTree.hpp"
#include "EffectRuntimeGL.hpp"

#include <boost\algorithm\string.hpp>
#include <nanovg_gl.h>

// -----------------------------------------------------------------------------------------------------

namespace ReShade
{
	namespace
	{
		class GLEffectCompiler : private boost::noncopyable
		{
		public:
			GLEffectCompiler(const EffectTree &ast) : mAST(ast), mEffect(nullptr), mCurrentFunction(EffectTree::Null), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentInDeclaratorList(false), mCurrentGlobalSize(0), mCurrentGlobalStorageSize(0)
			{
			}

			bool Traverse(GLEffect *effect, std::string &errors)
			{
				this->mEffect = effect;
				this->mErrors.clear();
				this->mFatal = false;
				this->mCurrentSource.clear();

				// Global uniform buffer
				this->mEffect->mUniformBuffers.push_back(0);
				this->mEffect->mUniformStorages.push_back(std::make_pair(nullptr, 0));

				const EffectNodes::Root *node = &this->mAST[EffectTree::Root].As<EffectNodes::Root>();

				do
				{
					Visit(*node);

					if (node->NextDeclaration != EffectTree::Null)
					{
						node = &this->mAST[node->NextDeclaration].As<EffectNodes::Root>();
					}
					else
					{
						node = nullptr;
					}
				}
				while (node != nullptr);

				if (this->mCurrentGlobalSize != 0)
				{
					glGenBuffers(1, &this->mEffect->mUniformBuffers[0]);

					GLint previous = 0;
					glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

					glBindBuffer(GL_UNIFORM_BUFFER, this->mEffect->mUniformBuffers[0]);
					glBufferData(GL_UNIFORM_BUFFER, this->mEffect->mUniformStorages[0].second, this->mEffect->mUniformStorages[0].first, GL_DYNAMIC_DRAW);
					
					glBindBuffer(GL_UNIFORM_BUFFER, previous);
				}

				errors += this->mErrors;

				return !this->mFatal;
			}

			static GLenum LiteralToTextureFilter(int value)
			{
				switch (value)
				{
					default:
						return GL_NONE;
					case EffectNodes::Literal::POINT:
						return GL_NEAREST;
					case EffectNodes::Literal::LINEAR:
						return GL_LINEAR;
					case EffectNodes::Literal::ANISOTROPIC:
						return GL_LINEAR_MIPMAP_LINEAR;
				}
			}
			static GLenum LiteralToTextureWrap(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::CLAMP:
						return GL_CLAMP_TO_EDGE;
					case EffectNodes::Literal::REPEAT:
						return GL_REPEAT;
					case EffectNodes::Literal::MIRROR:
						return GL_MIRRORED_REPEAT;
					case EffectNodes::Literal::BORDER:
						return GL_CLAMP_TO_BORDER;
				}
			}
			static GLenum LiteralToCompFunc(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ALWAYS:
						return GL_ALWAYS;
					case EffectNodes::Literal::NEVER:
						return GL_NEVER;
					case EffectNodes::Literal::EQUAL:
						return GL_EQUAL;
					case EffectNodes::Literal::NOTEQUAL:
						return GL_NOTEQUAL;
					case EffectNodes::Literal::LESS:
						return GL_LESS;
					case EffectNodes::Literal::LESSEQUAL:
						return GL_LEQUAL;
					case EffectNodes::Literal::GREATER:
						return GL_GREATER;
					case EffectNodes::Literal::GREATEREQUAL:
						return GL_GEQUAL;
				}
			}
			static GLenum LiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::KEEP:
						return GL_KEEP;
					case EffectNodes::Literal::ZERO:
						return GL_ZERO;
					case EffectNodes::Literal::REPLACE:
						return GL_REPLACE;
					case EffectNodes::Literal::INCR:
						return GL_INCR_WRAP;
					case EffectNodes::Literal::INCRSAT:
						return GL_INCR;
					case EffectNodes::Literal::DECR:
						return GL_DECR_WRAP;
					case EffectNodes::Literal::DECRSAT:
						return GL_DECR;
					case EffectNodes::Literal::INVERT:
						return GL_INVERT;
				}
			}
			static GLenum LiteralToBlendFunc(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ZERO:
						return GL_ZERO;
					case EffectNodes::Literal::ONE:
						return GL_ONE;
					case EffectNodes::Literal::SRCCOLOR:
						return GL_SRC_COLOR;
					case EffectNodes::Literal::SRCALPHA:
						return GL_SRC_ALPHA;
					case EffectNodes::Literal::INVSRCCOLOR:
						return GL_ONE_MINUS_SRC_COLOR;
					case EffectNodes::Literal::INVSRCALPHA:
						return GL_ONE_MINUS_SRC_ALPHA;
					case EffectNodes::Literal::DESTCOLOR:
						return GL_DST_COLOR;
					case EffectNodes::Literal::DESTALPHA:
						return GL_DST_ALPHA;
					case EffectNodes::Literal::INVDESTCOLOR:
						return GL_ONE_MINUS_DST_COLOR;
					case EffectNodes::Literal::INVDESTALPHA:
						return GL_ONE_MINUS_DST_ALPHA;
				}
			}
			static GLenum LiteralToBlendEq(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ADD:
						return GL_FUNC_ADD;
					case EffectNodes::Literal::SUBTRACT:
						return GL_FUNC_SUBTRACT;
					case EffectNodes::Literal::REVSUBTRACT:
						return GL_FUNC_REVERSE_SUBTRACT;
					case EffectNodes::Literal::MIN:
						return GL_MIN;
					case EffectNodes::Literal::MAX:
						return GL_MAX;
				}
			}
			static void LiteralToFormat(int value, GLenum &internalformat, GLenum &internalformatsrgb, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case EffectNodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						internalformat = internalformatsrgb = GL_R8;
						break;
					case EffectNodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						internalformat = internalformatsrgb = GL_R32F;
						break;
					case EffectNodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						internalformat = internalformatsrgb = GL_RG8;
						break;
					case EffectNodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						internalformat = GL_RGBA8;
						internalformatsrgb = GL_SRGB8_ALPHA8;
						break;
					case EffectNodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						internalformat = internalformatsrgb = GL_RGBA16;
						break;
					case EffectNodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						internalformat = internalformatsrgb = GL_RGBA16F;
						break;
					case EffectNodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						internalformat = internalformatsrgb = GL_RGBA32F;
						break;
					case EffectNodes::Literal::DXT1:
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
						format = Effect::Texture::Format::DXT1;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
						break;
					case EffectNodes::Literal::DXT3:
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
						format = Effect::Texture::Format::DXT3;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
						break;
					case EffectNodes::Literal::DXT5:
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
						format = Effect::Texture::Format::DXT5;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
						break;
					case EffectNodes::Literal::LATC1:
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT 0x8C70
						format = Effect::Texture::Format::LATC1;
						internalformat = internalformatsrgb = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
						break;
					case EffectNodes::Literal::LATC2:
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT 0x8C72
						format = Effect::Texture::Format::LATC2;
						internalformat = internalformatsrgb = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
						break;
					default:
						format = Effect::Texture::Format::Unknown;
						internalformat = internalformatsrgb = GL_NONE;
						break;
				}
			}

			static inline std::string PrintLocation(const EffectTree::Location &location)
			{
				return std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): ";
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

				res += name;

				return res;
			}
			static std::string FixNameWithSemantic(const std::string &name, const std::string &semantic)
			{
				if (semantic == "SV_VERTEXID")
				{
					return "glVertexID";
				}
				else if (semantic == "SV_POSITION")
				{
					return "glPosition";
				}

				return FixName(name);
			}
			std::string PrintType(const EffectNodes::Type &type)
			{
				switch (type.Class)
				{
					default:
						return "";
					case EffectNodes::Type::Void:
						return "void";
					case EffectNodes::Type::Bool:
						if (type.IsMatrix())
							return "mat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							return "bvec" + std::to_string(type.Rows);
						else
							return "bool";
					case EffectNodes::Type::Int:
						if (type.IsMatrix())
							return "mat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							return "ivec" + std::to_string(type.Rows);
						else
							return "int";
					case EffectNodes::Type::Uint:
						if (type.IsMatrix())
							return "mat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							return "uvec" + std::to_string(type.Rows);
						else
							return "uint";
					case EffectNodes::Type::Float:
						if (type.IsMatrix())
							return "mat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							return "vec" + std::to_string(type.Rows);
						else
							return "float";
					case EffectNodes::Type::Sampler:
						return "sampler2D";
					case EffectNodes::Type::Struct:
						assert(type.Definition != EffectTree::Null);
						return FixName(this->mAST[type.Definition].As<EffectNodes::Struct>().Name);
				}
			}
			std::string PrintTypeWithQualifiers(const EffectNodes::Type &type)
			{
				std::string qualifiers;

				if (type.HasQualifier(EffectNodes::Type::Qualifier::NoInterpolation))
					qualifiers += "flat ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::NoPerspective))
					qualifiers += "noperspective ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Linear))
					qualifiers += "smooth ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Sample))
					qualifiers += "sample ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Centroid))
					qualifiers += "centroid ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::InOut))
					qualifiers += "inout ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::In))
					qualifiers += "in ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::Out))
					qualifiers += "out ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::Uniform))
					qualifiers += "uniform ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Const))
					qualifiers += "const ";

				return qualifiers + PrintType(type);
			}
			std::pair<std::string, std::string> PrintCast(const EffectNodes::Type &from, const EffectNodes::Type &to)
			{
				std::pair<std::string, std::string> code;

				if (from.Class != to.Class && !(from.IsMatrix() && to.IsMatrix()))
				{
					const EffectNodes::Type type = { to.Class, 0, from.Rows, from.Cols, 0, to.Definition };

					code.first += PrintType(type) + "(";
					code.second += ")";
				}

				if (from.Rows > 0 && from.Rows < to.Rows)
				{
					const char subscript[4] = { 'x', 'y', 'z', 'w' };

					code.second += '.';

					for (unsigned int i = 0; i < from.Rows; ++i)
					{
						code.second += subscript[i];
					}
					for (unsigned int i = from.Rows; i < to.Rows; ++i)
					{
						code.second += subscript[from.Rows - 1];
					}
				}
				else if (from.Rows > to.Rows)
				{
					const char subscript[4] = { 'x', 'y', 'z', 'w' };

					code.second += '.';

					for (unsigned int i = 0; i < to.Rows; ++i)
					{
						code.second += subscript[i];
					}
				}

				return code;
			}

			void Visit(const EffectTree::Node &node)
			{
				using namespace EffectNodes;

				if (node.Is<Variable>())
				{
					Visit(node.As<Variable>());
				}
				else if (node.Is<Function>())
				{
					Visit(node.As<Function>());
				}
				else if (node.Is<Literal>())
				{
					Visit(node.As<Literal>());
				}
				else if (node.Is<LValue>())
				{
					Visit(node.As<LValue>());
				}
				else if (node.Is<Expression>())
				{
					Visit(node.As<Expression>());
				}
				else if (node.Is<Swizzle>())
				{
					Visit(node.As<Swizzle>());
				}
				else if (node.Is<Assignment>())
				{
					Visit(node.As<Assignment>());
				}
				else if (node.Is<Sequence>())
				{
					Visit(node.As<Sequence>());
				}
				else if (node.Is<Constructor>())
				{
					Visit(node.As<Constructor>());
				}
				else if (node.Is<Call>())
				{
					Visit(node.As<Call>());
				}
				else if (node.Is<ExpressionStatement>())
				{
					Visit(node.As<ExpressionStatement>());
				}
				else if (node.Is<DeclarationStatement>())
				{
					Visit(node.As<DeclarationStatement>());
				}
				else if (node.Is<If>())
				{
					Visit(node.As<If>());
				}
				else if (node.Is<Switch>())
				{
					Visit(node.As<Switch>());
				}
				else if (node.Is<For>())
				{
					Visit(node.As<For>());
				}
				else if (node.Is<While>())
				{
					Visit(node.As<While>());
				}
				else if (node.Is<Return>())
				{
					Visit(node.As<Return>());
				}
				else if (node.Is<Jump>())
				{
					Visit(node.As<Jump>());
				}
				else if (node.Is<StatementBlock>())
				{
					Visit(node.As<StatementBlock>());
				}
				else if (node.Is<Struct>())
				{
					Visit(node.As<Struct>());
				}
				else if (node.Is<Technique>())
				{
					Visit(node.As<Technique>());
				}
				else if (!node.Is<Root>())
				{
					assert(false);
				}
			}
			void Visit(const EffectNodes::LValue &node)
			{
				this->mCurrentSource += FixName(this->mAST[node.Reference].As<EffectNodes::Variable>().Name);
			}
			void Visit(const EffectNodes::Literal &node)
			{
				if (!node.Type.IsScalar())
				{
					this->mCurrentSource += PrintType(node.Type);
					this->mCurrentSource += '(';
				}

				for (unsigned int i = 0; i < node.Type.Rows * node.Type.Cols; ++i)
				{
					switch (node.Type.Class)
					{
						case EffectNodes::Type::Bool:
							this->mCurrentSource += node.Value.Bool[i] ? "true" : "false";
							break;
						case EffectNodes::Type::Int:
							this->mCurrentSource += std::to_string(node.Value.Int[i]);
							break;
						case EffectNodes::Type::Uint:
							this->mCurrentSource += std::to_string(node.Value.Uint[i]) + "u";
							break;
						case EffectNodes::Type::Float:
							this->mCurrentSource += std::to_string(node.Value.Float[i]);
							break;
					}

					this->mCurrentSource += ", ";
				}

				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();

				if (!node.Type.IsScalar())
				{
					this->mCurrentSource += ')';
				}
			}
			void Visit(const EffectNodes::Expression &node)
			{
				std::string part1, part2, part3, part4;
				std::pair<std::string, std::string> cast1, cast2, cast3, cast121, cast122;
				EffectNodes::Type type1, type2, type3, type12;

				cast1 = PrintCast(type1 = this->mAST[node.Operands[0]].As<EffectNodes::RValue>().Type, node.Type);

				if (node.Operands[1] != 0)
				{
					cast2 = PrintCast(type2 = this->mAST[node.Operands[1]].As<EffectNodes::RValue>().Type, node.Type);

					type12 = type2.IsFloatingPoint() ? type2 : type1;
					type12.Rows = std::max(type1.Rows, type2.Rows);
					type12.Cols = std::max(type1.Cols, type2.Cols);
					cast121 = PrintCast(type1, type12), cast122 = PrintCast(type2, type12);
				}
				if (node.Operands[2] != 0)
				{
					cast3 = PrintCast(type3 = this->mAST[node.Operands[2]].As<EffectNodes::RValue>().Type, node.Type);
				}

				switch (node.Operator)
				{
					case EffectNodes::Expression::Negate:
						part1 = '-';
						break;
					case EffectNodes::Expression::BitNot:
						part1 = '~';
						break;
					case EffectNodes::Expression::LogicNot:
					{
						if (node.Type.IsVector())
						{
							part1 = "not(" + cast1.first;
							part2 = cast1.second + ')';
						}
						else
						{
							part1 = '!';
						}
						break;
					}
					case EffectNodes::Expression::Increase:
						part1 = "++";
						break;
					case EffectNodes::Expression::Decrease:
						part1 = "--";
						break;
					case EffectNodes::Expression::PostIncrease:
						part2 = "++";
						break;
					case EffectNodes::Expression::PostDecrease:
						part2 = "--";
						break;
					case EffectNodes::Expression::Abs:
						part1 = "abs(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Sign:
						part1 = cast1.first + "sign(";
						part2 = ')' + cast1.second;
						break;
					case EffectNodes::Expression::Rcp:
						part1 = '(' + PrintType(node.Type) + "(1.0) / ";
						part2 = ')';
						break;
					case EffectNodes::Expression::All:
					{
						if (type1.IsVector())
						{
							part1 = "all(bvec" + std::to_string(type1.Rows) + '(';
							part2 = "))";
						}
						else
						{
							part1 = "bool(";
							part2 = ')';
						}
						break;
					}
					case EffectNodes::Expression::Any:
					{
						if (type1.IsVector())
						{
							part1 = "any(bvec" + std::to_string(type1.Rows) + '(';
							part2 = "))";
						}
						else
						{
							part1 = "bool(";
							part2 = ')';
						}
						break;
					}
					case EffectNodes::Expression::Sin:
						part1 = "sin(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Sinh:
						part1 = "sinh(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Cos:
						part1 = "cos(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Cosh:
						part1 = "cosh(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Tan:
						part1 = "tan(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Tanh:
						part1 = "tanh(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Asin:
						part1 = "asin(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Acos:
						part1 = "acos(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Atan:
						part1 = "atan(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Exp:
						part1 = "exp(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Exp2:
						part1 = "exp2(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Log:
						part1 = "log(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Log2:
						part1 = "log2(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Log10:
						part1 = "(log2(" + cast1.first;
						part2 = cast1.second + ") / " + PrintType(node.Type) + "(2.302585093))";
						break;
					case EffectNodes::Expression::Sqrt:
						part1 = "sqrt(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Rsqrt:
						part1 = "inversesqrt(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Ceil:
						part1 = "ceil(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Floor:
						part1 = "floor(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Frac:
						part1 = "fract(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Trunc:
						part1 = "trunc(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Round:
						part1 = "round(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Saturate:
						part1 = "clamp(" + cast1.first;
						part2 = cast1.second + ", 0.0, 1.0)";
						break;
					case EffectNodes::Expression::Radians:
						part1 = "radians(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Degrees:
						part1 = "degrees(" + cast1.first;
						part2 = cast1.second + ')';
						break;
					case EffectNodes::Expression::Noise:
					{
						part1 = "noise1(";

						if (!type1.IsFloatingPoint())
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Length:
					{
						part1 = "length(";

						if (!type1.IsFloatingPoint())
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Normalize:
					{
						part1 = "normalize(";

						if (!type1.IsFloatingPoint())
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Transpose:
					{
						part1 = "transpose(";

						if (!type1.IsFloatingPoint())
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Determinant:
					{
						part1 = "determinant(";

						if (!type1.IsFloatingPoint())
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Cast:
						part1 = PrintType(node.Type) + '(';
						part2 = ')';
						break;
					case EffectNodes::Expression::BitCastInt2Float:
					{
						part1 = "intBitsToFloat(";

						if (type1.Class != EffectNodes::Type::Int)
						{
							type1.Class = EffectNodes::Type::Int;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::BitCastUint2Float:
					{
						part1 = "uintBitsToFloat(";

						if (type1.Class != EffectNodes::Type::Uint)
						{
							type1.Class = EffectNodes::Type::Uint;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::BitCastFloat2Int:
					{
						part1 = "floatBitsToInt(";

						if (type1.Class != EffectNodes::Type::Float)
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::BitCastFloat2Uint:
					{
						part1 = "floatBitsToUint(";

						if (type1.Class != EffectNodes::Type::Float)
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 += ')';
						break;
					}
					case EffectNodes::Expression::Add:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " + " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Subtract:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " - " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Multiply:
						if (node.Type.IsMatrix())
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
					case EffectNodes::Expression::Divide:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " / " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Modulo:
						if (node.Type.IsFloatingPoint())
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
					case EffectNodes::Expression::Less:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::Greater:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::LessOrEqual:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::GreaterOrEqual:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::Equal:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::NotEqual:
						if (node.Type.IsVector())
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
					case EffectNodes::Expression::LeftShift:
						part1 = '(';
						part2 = " << ";
						part3 = ')';
						break;
					case EffectNodes::Expression::RightShift:
						part1 = '(';
						part2 = " >> ";
						part3 = ')';
						break;
					case EffectNodes::Expression::BitAnd:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " & " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::BitXor:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " ^ " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::BitOr:
						part1 = '(' + cast1.first;
						part2 = cast1.second + " | " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::LogicAnd:
						part1 = '(' + cast121.first;
						part2 = cast121.second + " && " + cast122.first;
						part3 = cast122.second + ')';
						break;
					case EffectNodes::Expression::LogicXor:
						part1 = '(' + cast121.first;
						part2 = cast121.second + " ^^ " + cast122.first;
						part3 = cast122.second + ')';
						break;
					case EffectNodes::Expression::LogicOr:
						part1 = '(' + cast121.first;
						part2 = cast121.second + " || " + cast122.first;
						part3 = cast122.second + ')';
						break;
					case EffectNodes::Expression::Mul:
						part1 = '(';
						part2 = " * ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Atan2:
						part1 = "atan(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Dot:
						part1 = "dot(" + cast121.first;
						part2 = cast121.second + ", " + cast122.first;
						part3 = cast122.second + ')';
						break;
					case EffectNodes::Expression::Cross:
						part1 = "cross(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Distance:
						part1 = "distance(" + cast121.first;
						part2 = cast121.second + ", " + cast122.first;
						part3 = cast122.second + ')';
						break;
					case EffectNodes::Expression::Pow:
						part1 = "pow(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Modf:
						part1 = "modf(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Frexp:
						part1 = "frexp(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Ldexp:
						part1 = "ldexp(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Min:
						part1 = "min(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Max:
						part1 = "max(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Step:
						part1 = "step(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Reflect:
						part1 = "reflect(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ')';
						break;
					case EffectNodes::Expression::Extract:
						part2 = '[';
						part3 = ']';
						break;
					case EffectNodes::Expression::Field:
						this->mCurrentSource += '(';
						Visit(this->mAST[node.Operands[0]]);
						this->mCurrentSource += (this->mAST[node.Operands[0]].Is<EffectNodes::LValue>() && this->mAST[node.Operands[0]].As<EffectNodes::LValue>().Type.HasQualifier(EffectNodes::Type::Uniform)) ? '_' : '.';
						this->mCurrentSource += this->mAST[node.Operands[1]].As<EffectNodes::Variable>().Name;
						this->mCurrentSource += ')';
						return;
					case EffectNodes::Expression::Tex:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);

						part1 = "texture(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0))";
						break;
					}
					case EffectNodes::Expression::TexLevel:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 4, 1 };
						cast2 = PrintCast(type2, type2to);

						part1 = "_textureLod(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
						break;
					}
					case EffectNodes::Expression::TexGather:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);

						part1 = "textureGather(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0))";
						break;
					}
					case EffectNodes::Expression::TexBias:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 4, 1 };
						cast2 = PrintCast(type2, type2to);

						part1 = "_textureBias(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0))";
						break;
					}
					case EffectNodes::Expression::TexFetch:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Int, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);

						part1 = "texelFetch(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * ivec2(1, -1) + ivec2(0, 1))";
						break;
					}
					case EffectNodes::Expression::TexSize:
						part1 = "textureSize(";
						part2 = ", int(";
						part3 = "))";
						break;
					case EffectNodes::Expression::Mad:
						part1 = "(" + cast1.first;
						part2 = cast1.second + " * " + cast2.first;
						part3 = cast2.second + " + " + cast3.first;
						part4 = cast3.second + ')';
						break;
					case EffectNodes::Expression::SinCos:
					{
						part1 = "_sincos(";

						if (type1.Class != EffectNodes::Type::Float)
						{
							type1.Class = EffectNodes::Type::Float;
							part1 += PrintType(type1) + '(';
							part2 = ')';
						}

						part2 = ", ";
						part3 = ", ";
						part4 = ')';
						break;
					}
					case EffectNodes::Expression::Lerp:
						part1 = "mix(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ", " + cast3.first;
						part4 = cast3.second + ')';
						break;
					case EffectNodes::Expression::Clamp:
						part1 = "clamp(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ", " + cast3.first;
						part4 = cast3.second + ')';
						break;
					case EffectNodes::Expression::SmoothStep:
						part1 = "smoothstep(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ", " + cast3.first;
						part4 = cast3.second + ')';
						break;
					case EffectNodes::Expression::Refract:
						part1 = "refract(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ", float";
						part4 = "))";
						break;
					case EffectNodes::Expression::FaceForward:
						part1 = "faceforward(" + cast1.first;
						part2 = cast1.second + ", " + cast2.first;
						part3 = cast2.second + ", " + cast3.first;
						part4 = cast3.second + ')';
						break;
					case EffectNodes::Expression::Conditional:
					{
						part1 = '(';

						if (this->mAST[node.Operands[0]].As<EffectNodes::RValue>().Type.IsVector())
						{
							part1 += "all(bvec" + std::to_string(this->mAST[node.Operands[0]].As<EffectNodes::RValue>().Type.Rows) + '(';
							part2 = "))";
						}
						else
						{
							part1 += "bool(";
							part2 = ')';
						}

						part2 += " ? " + cast2.first;
						part3 = cast2.second + " : " + cast3.first;
						part4 = cast3.second + ')';
						break;
					}
					case EffectNodes::Expression::TexOffset:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 2, 1 };
						const EffectNodes::Type type3to = { EffectNodes::Type::Int, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);
						cast3 = PrintCast(type3, type3to);

						part1 = "textureOffset(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " + cast3.first;
						part4 = cast3.second + " * ivec2(1, -1))";
						break;
					}
					case EffectNodes::Expression::TexLevelOffset:
					{	
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 4, 1 };
						const EffectNodes::Type type3to = { EffectNodes::Type::Int, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);
						cast3 = PrintCast(type3, type3to);

						part1 = "_textureLodOffset(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec4(1.0, -1.0, 1.0, 1.0) + vec4(0.0, 1.0, 0.0, 0.0), " + cast3.first;
						part4 = cast3.second + " * ivec2(1, -1))";
						break;
					}
					case EffectNodes::Expression::TexGatherOffset:
					{
						const EffectNodes::Type type2to = { EffectNodes::Type::Float, 0, 2, 1 };
						const EffectNodes::Type type3to = { EffectNodes::Type::Int, 0, 2, 1 };
						cast2 = PrintCast(type2, type2to);
						cast3 = PrintCast(type3, type3to);

						part1 = "textureGatherOffset(";
						part2 = ", " + cast2.first;
						part3 = cast2.second + " * vec2(1.0, -1.0) + vec2(0.0, 1.0), " + cast3.first;
						part4 = cast3.second + " * ivec2(1, -1))";
						break;
					}
				}

				this->mCurrentSource += part1;
				Visit(this->mAST[node.Operands[0]]);
				this->mCurrentSource += part2;

				if (node.Operands[1] != 0)
				{
					Visit(this->mAST[node.Operands[1]]);
				}

				this->mCurrentSource += part3;

				if (node.Operands[2] != 0)
				{
					Visit(this->mAST[node.Operands[2]]);
				}

				this->mCurrentSource += part4;
			}
			void Visit(const EffectNodes::Sequence &node)
			{
				const EffectNodes::RValue *expression = &this->mAST[node.Expressions].As<EffectNodes::RValue>();

				do
				{
					Visit(*expression);

					if (expression->NextExpression != EffectTree::Null)
					{
						this->mCurrentSource += ", ";

						expression = &this->mAST[expression->NextExpression].As<EffectNodes::RValue>();
					}
					else
					{
						expression = nullptr;
					}
				}
				while (expression != nullptr);
			}
			void Visit(const EffectNodes::Assignment &node)
			{
				this->mCurrentSource += '(';
				Visit(this->mAST[node.Left]);
				this->mCurrentSource += ' ';

				switch (node.Operator)
				{
					case EffectNodes::Expression::None:
						this->mCurrentSource += '=';
						break;
					case EffectNodes::Expression::Add:
						this->mCurrentSource += "+=";
						break;
					case EffectNodes::Expression::Subtract:
						this->mCurrentSource += "-=";
						break;
					case EffectNodes::Expression::Multiply:
						this->mCurrentSource += "*=";
						break;
					case EffectNodes::Expression::Divide:
						this->mCurrentSource += "/=";
						break;
					case EffectNodes::Expression::Modulo:
						this->mCurrentSource += "%=";
						break;
					case EffectNodes::Expression::LeftShift:
						this->mCurrentSource += "<<=";
						break;
					case EffectNodes::Expression::RightShift:
						this->mCurrentSource += ">>=";
						break;
					case EffectNodes::Expression::BitAnd:
						this->mCurrentSource += "&=";
						break;
					case EffectNodes::Expression::BitXor:
						this->mCurrentSource += "^=";
						break;
					case EffectNodes::Expression::BitOr:
						this->mCurrentSource += "|=";
						break;
				}

				const std::pair<std::string, std::string> cast = PrintCast(this->mAST[node.Right].As<EffectNodes::RValue>().Type, this->mAST[node.Left].As<EffectNodes::RValue>().Type);

				this->mCurrentSource += ' ';
				this->mCurrentSource += cast.first;
				Visit(this->mAST[node.Right]);
				this->mCurrentSource += cast.second;
				this->mCurrentSource += ')';
			}
			void Visit(const EffectNodes::Call &node)
			{
				this->mCurrentSource += FixName(node.CalleeName);
				this->mCurrentSource += '(';

				if (node.Arguments != EffectTree::Null)
				{
					const EffectNodes::RValue *argument = &this->mAST[node.Arguments].As<EffectNodes::RValue>();
					const EffectNodes::Variable *parameter = &this->mAST[this->mAST[node.Callee].As<EffectNodes::Function>().Parameters].As<EffectNodes::Variable>();

					do
					{
						const std::pair<std::string , std::string> cast = PrintCast(argument->Type, parameter->Type);

						this->mCurrentSource += cast.first;
						Visit(*argument);
						this->mCurrentSource += cast.second;

						if (argument->NextExpression != EffectTree::Null)
						{
							this->mCurrentSource += ", ";

							argument = &this->mAST[argument->NextExpression].As<EffectNodes::RValue>();
							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							argument = nullptr;
							parameter = nullptr;
						}
					}
					while (argument != nullptr && parameter != nullptr);
				}

				this->mCurrentSource += ')';
			}
			void Visit(const EffectNodes::Constructor &node)
			{
				if (node.Type.IsMatrix())
				{
					this->mCurrentSource += "transpose(";
				}

				this->mCurrentSource += PrintType(node.Type);
				this->mCurrentSource += '(';

				const EffectNodes::RValue *argument = &this->mAST[node.Arguments].As<EffectNodes::RValue>();

				do
				{
					Visit(*argument);

					if (argument->NextExpression != EffectTree::Null)
					{
						this->mCurrentSource += ", ";

						argument = &this->mAST[argument->NextExpression].As<EffectNodes::RValue>();
					}
					else
					{
						argument = nullptr;
					}
				}
				while (argument != nullptr);

				this->mCurrentSource += ')';

				if (node.Type.IsMatrix())
				{
					this->mCurrentSource += ')';
				}
			}
			void Visit(const EffectNodes::Swizzle &node)
			{
				const EffectNodes::RValue &left = this->mAST[node.Operands[0]].As<EffectNodes::RValue>();

				Visit(left);

				this->mCurrentSource += '.';

				if (left.Type.IsMatrix())
				{
					const char swizzle[16][5] =
					{
						"_m00", "_m01", "_m02", "_m03",
						"_m10", "_m11", "_m12", "_m13",
						"_m20", "_m21", "_m22", "_m23",
						"_m30", "_m31", "_m32", "_m33"
					};

					for (int i = 0; i < 4 && node.Mask[i] >= 0; ++i)
					{
						this->mCurrentSource += swizzle[node.Mask[i]];
					}
				}
				else
				{
					const char swizzle[4] =
					{
						'x', 'y', 'z', 'w'
					};

					for (int i = 0; i < 4 && node.Mask[i] >= 0; ++i)
					{
						this->mCurrentSource += swizzle[node.Mask[i]];
					}
				}
			}
			void Visit(const EffectNodes::InitializerList &node, const EffectNodes::Type &type)
			{
				this->mCurrentSource += PrintType(type);
				this->mCurrentSource += "[]";
				this->mCurrentSource += '(';

				const EffectNodes::RValue *expression = &this->mAST[node.Expressions].As<EffectNodes::RValue>();

				do
				{
					const auto cast = PrintCast(expression->Type, type);

					this->mCurrentSource += cast.first;
					Visit(*expression);
					this->mCurrentSource += cast.second;

					if (expression->NextExpression != EffectTree::Null)
					{
						this->mCurrentSource += ", ";

						expression = &this->mAST[expression->NextExpression].As<EffectNodes::RValue>();
					}
					else
					{
						expression = nullptr;
					}
				}
				while (expression != nullptr);

				this->mCurrentSource += ')';
			}
			void Visit(const EffectNodes::If &node)
			{
				const EffectNodes::Type typeto = { EffectNodes::Type::Bool, 0, 1, 1 };
				const auto cast = PrintCast(this->mAST[node.Condition].As<EffectNodes::RValue>().Type, typeto);

				this->mCurrentSource += "if (";
				this->mCurrentSource += cast.first;
				Visit(this->mAST[node.Condition]);
				this->mCurrentSource += cast.second;
				this->mCurrentSource += ")\n";

				if (node.StatementOnTrue != EffectTree::Null)
				{
					Visit(this->mAST[node.StatementOnTrue]);
				}
				else
				{
					this->mCurrentSource += "\t;";
				}
				if (node.StatementOnFalse != EffectTree::Null)
				{
					this->mCurrentSource += "else\n";
					Visit(this->mAST[node.StatementOnFalse]);
				}
			}
			void Visit(const EffectNodes::Switch &node)
			{
				this->mCurrentSource += "switch (";
				Visit(this->mAST[node.Test]);
				this->mCurrentSource += ")\n{\n";

				const EffectNodes::Case *cases = &this->mAST[node.Cases].As<EffectNodes::Case>();

				do
				{
					Visit(*cases);

					if (cases->NextCase != EffectTree::Null)
					{
						cases = &this->mAST[cases->NextCase].As<EffectNodes::Case>();
					}
					else
					{
						cases = nullptr;
					}
				}
				while (cases != nullptr);

				this->mCurrentSource += "}\n";
			}
			void Visit(const EffectNodes::Case &node)
			{
				const EffectNodes::RValue *label = &this->mAST[node.Labels].As<EffectNodes::RValue>();

				do
				{
					if (label->Is<EffectNodes::Expression>())
					{
						this->mCurrentSource += "default";
					}
					else
					{
						this->mCurrentSource += "case ";
						Visit(label->As<EffectNodes::Literal>());
					}

					this->mCurrentSource += ":\n";

					if (label->NextExpression != EffectTree::Null)
					{
						label = &this->mAST[label->NextExpression].As<EffectNodes::RValue>();
					}
					else
					{
						label = nullptr;
					}
				}
				while (label != nullptr);

				Visit(this->mAST[node.Statements].As<EffectNodes::StatementBlock>());
			}
			void Visit(const EffectNodes::For &node)
			{
				this->mCurrentSource += "for (";

				if (node.Initialization != EffectTree::Null)
				{
					Visit(this->mAST[node.Initialization]);
				}
				else
				{
					this->mCurrentSource += "; ";
				}
										
				if (node.Condition != EffectTree::Null)
				{
					Visit(this->mAST[node.Condition]);
				}

				this->mCurrentSource += "; ";

				if (node.Iteration != EffectTree::Null)
				{
					Visit(this->mAST[node.Iteration]);
				}

				this->mCurrentSource += ")\n";

				if (node.Statements != EffectTree::Null)
				{
					Visit(this->mAST[node.Statements]);
				}
				else
				{
					this->mCurrentSource += "\t;";
				}
			}
			void Visit(const EffectNodes::While &node)
			{
				if (node.DoWhile)
				{
					this->mCurrentSource += "do\n{\n";

					if (node.Statements != EffectTree::Null)
					{
						Visit(this->mAST[node.Statements]);
					}

					this->mCurrentSource += "}\n";
					this->mCurrentSource += "while (";
					Visit(this->mAST[node.Condition]);
					this->mCurrentSource += ");\n";
				}
				else
				{
					this->mCurrentSource += "while (";
					Visit(this->mAST[node.Condition]);
					this->mCurrentSource += ")\n";

					if (node.Statements != EffectTree::Null)
					{
						Visit(this->mAST[node.Statements]);
					}
					else
					{
						this->mCurrentSource += "\t;";
					}
				}
			}
			void Visit(const EffectNodes::Return &node)
			{
				if (node.Discard)
				{
					this->mCurrentSource += "discard";
				}
				else
				{
					this->mCurrentSource += "return";

					if (node.Value != EffectTree::Null)
					{
						const auto cast = PrintCast(this->mAST[node.Value].As<EffectNodes::RValue>().Type, this->mAST[this->mCurrentFunction].As<EffectNodes::Function>().ReturnType);

						this->mCurrentSource += ' ';
						this->mCurrentSource += cast.first;
						Visit(this->mAST[node.Value]);
						this->mCurrentSource += cast.second;
					}
				}

				this->mCurrentSource += ";\n";
			}
			void Visit(const EffectNodes::Jump &node)
			{
				switch (node.Mode)
				{
					case EffectNodes::Jump::Break:
						this->mCurrentSource += "break";
						break;
					case EffectNodes::Jump::Continue:
						this->mCurrentSource += "continue";
						break;
				}

				this->mCurrentSource += ";\n";
			}
			void Visit(const EffectNodes::ExpressionStatement &node)
			{
				if (node.Expression != EffectTree::Null)
				{
					Visit(this->mAST[node.Expression]);
				}

				this->mCurrentSource += ";\n";
			}
			void Visit(const EffectNodes::DeclarationStatement &node)
			{
				Visit(this->mAST[node.Declaration]);
			}
			void Visit(const EffectNodes::StatementBlock &node)
			{
				this->mCurrentSource += "{\n";

				if (node.Statements != EffectTree::Null)
				{
					const EffectNodes::Statement *statement = &this->mAST[node.Statements].As<EffectNodes::Statement>();

					do
					{
						Visit(*statement);

						if (statement->NextStatement != EffectTree::Null)
						{
							statement = &this->mAST[statement->NextStatement].As<EffectNodes::Statement>();
						}
						else
						{
							statement = nullptr;
						}
					}
					while (statement != nullptr);
				}

				this->mCurrentSource += "}\n";
			}
			void Visit(const EffectNodes::Annotation &node, GLTexture &texture)
			{
				Effect::Annotation data;
				const auto &value = this->mAST[node.Value].As<EffectNodes::Literal>();

				switch (value.Type.Class)
				{
					case EffectNodes::Type::Bool:
						data = value.Value.Bool[0] != 0;
						break;
					case EffectNodes::Type::Int:
						data = value.Value.Int[0];
						break;
					case EffectNodes::Type::Uint:
						data = value.Value.Uint[0];
						break;
					case EffectNodes::Type::Float:
						data = value.Value.Float[0];
						break;
					case EffectNodes::Type::String:
						data = value.Value.String;
						break;
				}

				texture.AddAnnotation(node.Name, data);

				if (node.NextAnnotation != EffectTree::Null)
				{
					Visit(this->mAST[node.NextAnnotation].As<EffectNodes::Annotation>(), texture);
				}
			}
			void Visit(const EffectNodes::Annotation &node, GLConstant &constant)
			{
				Effect::Annotation data;
				const auto &value = this->mAST[node.Value].As<EffectNodes::Literal>();

				switch (value.Type.Class)
				{
					case EffectNodes::Type::Bool:
						data = value.Value.Bool[0] != 0;
						break;
					case EffectNodes::Type::Int:
						data = value.Value.Int[0];
						break;
					case EffectNodes::Type::Uint:
						data = value.Value.Uint[0];
						break;
					case EffectNodes::Type::Float:
						data = value.Value.Float[0];
						break;
					case EffectNodes::Type::String:
						data = value.Value.String;
						break;
				}

				constant.AddAnnotation(node.Name, data);

				if (node.NextAnnotation != EffectTree::Null)
				{
					Visit(this->mAST[node.NextAnnotation].As<EffectNodes::Annotation>(), constant);
				}
			}
			void Visit(const EffectNodes::Annotation &node, GLTechnique &technique)
			{
				Effect::Annotation data;
				const auto &value = this->mAST[node.Value].As<EffectNodes::Literal>();

				switch (value.Type.Class)
				{
					case EffectNodes::Type::Bool:
						data = value.Value.Bool[0] != 0;
						break;
					case EffectNodes::Type::Int:
						data = value.Value.Int[0];
						break;
					case EffectNodes::Type::Uint:
						data = value.Value.Uint[0];
						break;
					case EffectNodes::Type::Float:
						data = value.Value.Float[0];
						break;
					case EffectNodes::Type::String:
						data = value.Value.String;
						break;
				}

				technique.AddAnnotation(node.Name, data);

				if (node.NextAnnotation != EffectTree::Null)
				{
					Visit(this->mAST[node.NextAnnotation].As<EffectNodes::Annotation>(), technique);
				}
			}
			void Visit(const EffectNodes::Struct &node)
			{
				this->mCurrentSource += "struct ";

				if (node.Name != nullptr)
				{
					this->mCurrentSource += FixName(node.Name);
				}
				else
				{
					this->mCurrentSource += "_" + std::to_string(std::rand() * 100 + std::rand() * 10 + std::rand());
				}

				this->mCurrentSource += "\n{\n";

				if (node.Fields != EffectTree::Null)
				{
					Visit(this->mAST[node.Fields].As<EffectNodes::Variable>());
				}
				else
				{
					this->mCurrentSource += "float _dummy;\n";
				}

				this->mCurrentSource += "};\n";
			}
			void Visit(const EffectNodes::Variable &node)
			{
				if (!(this->mCurrentInParameterBlock || this->mCurrentInFunctionBlock))
				{
					if (node.Type.IsStruct() && node.Type.HasQualifier(EffectNodes::Type::Qualifier::Uniform))
					{
						VisitUniformBuffer(node);
						return;
					}
					else if (node.Type.IsTexture())
					{
						VisitTexture(node);
						return;
					}
					else if (node.Type.IsSampler())
					{
						VisitSampler(node);
						return;
					}
					else if (node.Type.HasQualifier(EffectNodes::Type::Qualifier::Uniform))
					{
						VisitUniform(node);
						return;
					}
				}

				if (!this->mCurrentInDeclaratorList)
				{
					this->mCurrentSource += PrintTypeWithQualifiers(node.Type);
				}

				if (node.Name != nullptr)
				{
					this->mCurrentSource += ' ';

					if (!this->mCurrentBlockName.empty())
					{
						this->mCurrentSource += this->mCurrentBlockName + '_';
					}
				
					this->mCurrentSource += FixName(node.Name);
				}

				if (node.Type.IsArray())
				{
					this->mCurrentSource += '[' + ((node.Type.ArrayLength >= 1) ? std::to_string(node.Type.ArrayLength) : "") + ']';
				}

				if (node.Initializer != EffectTree::Null)
				{
					this->mCurrentSource += " = ";

					if (this->mAST[node.Initializer].Is<EffectNodes::InitializerList>())
					{
						Visit(this->mAST[node.Initializer].As<EffectNodes::InitializerList>(), node.Type);
					}
					else
					{
						const auto cast = PrintCast(this->mAST[node.Initializer].As<EffectNodes::RValue>().Type, node.Type);

						this->mCurrentSource += cast.first;
						Visit(this->mAST[node.Initializer]);
						this->mCurrentSource += cast.second;
					}
				}

				if (node.NextDeclarator != EffectTree::Null)
				{
					const auto &next = this->mAST[node.NextDeclarator].As<EffectNodes::Variable>();

					if (next.Type.Class == node.Type.Class && next.Type.Rows == node.Type.Rows && next.Type.Cols == node.Type.Rows && next.Type.Definition == node.Type.Definition)
					{
						this->mCurrentSource += ", ";

						this->mCurrentInDeclaratorList = true;

						Visit(next);

						this->mCurrentInDeclaratorList = false;
					}
					else
					{
						this->mCurrentSource += ";\n";

						Visit(next);
					}
				}
				else if (!this->mCurrentInParameterBlock)
				{
					this->mCurrentSource += ";\n";
				}
			}
			void VisitTexture(const EffectNodes::Variable &node)
			{			
				const GLsizei width = (node.Properties[EffectNodes::Variable::Width] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Width]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				const GLsizei height = (node.Properties[EffectNodes::Variable::Height] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Height]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				const GLsizei levels = (node.Properties[EffectNodes::Variable::MipLevels] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MipLevels]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				GLenum internalformat = GL_RGBA8, internalformatSRGB = GL_SRGB8_ALPHA8;
				Effect::Texture::Format format = Effect::Texture::Format::RGBA8;

				if (node.Properties[EffectNodes::Variable::Format] != 0)
				{
					LiteralToFormat(this->mAST[node.Properties[EffectNodes::Variable::Format]].As<EffectNodes::Literal>().Value.Uint[0], internalformat, internalformatSRGB, format);
				}

				GLTexture::Description objdesc;
				objdesc.Width = width;
				objdesc.Height = height;
				objdesc.Levels = levels;
				objdesc.Format = format;

				std::unique_ptr<GLTexture> obj(new GLTexture(this->mEffect, objdesc));

				GLCHECK(glGenTextures(2, obj->mID));

				GLint previous = 0;
				GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

				GLCHECK(glBindTexture(GL_TEXTURE_2D, obj->mID[0]));
				GLCHECK(glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height));
				const std::vector<unsigned char> nullpixels(width * height, 0);
				GLCHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, nullpixels.data()));
				GLCHECK(glTextureView(obj->mID[1], GL_TEXTURE_2D, obj->mID[0], internalformatSRGB, 0, levels, 0, 1));

				GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), *obj);
				}

				this->mEffect->mTextures.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void VisitSampler(const EffectNodes::Variable &node)
			{
				if (node.Properties[EffectNodes::Variable::Texture] == 0)
				{
					this->mErrors = PrintLocation(node.Location) + "Sampler '" + std::string(node.Name) + "' is missing required 'Texture' required.\n";
					this->mFatal;
					return;
				}

				std::shared_ptr<GLSampler> obj = std::make_shared<GLSampler>();
				obj->mTexture = this->mEffect->mTextures.at(this->mAST[node.Properties[EffectNodes::Variable::Texture]].As<EffectNodes::Variable>().Name).get();
				obj->mSRGB = node.Properties[EffectNodes::Variable::SRGBTexture] != 0 && this->mAST[node.Properties[EffectNodes::Variable::SRGBTexture]].As<EffectNodes::Literal>().Value.Bool[0] != 0;

				GLCHECK(glGenSamplers(1, &obj->mID));

				GLenum minfilter = (node.Properties[EffectNodes::Variable::MinFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MinFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_LINEAR;
				const GLenum mipfilter = (node.Properties[EffectNodes::Variable::MipFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MipFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_LINEAR;
				
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

				GLCHECK(glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_S, (node.Properties[EffectNodes::Variable::AddressU] != 0) ? LiteralToTextureWrap(this->mAST[node.Properties[EffectNodes::Variable::AddressU]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_CLAMP_TO_EDGE));
				GLCHECK(glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_T, (node.Properties[EffectNodes::Variable::AddressV] != 0) ? LiteralToTextureWrap(this->mAST[node.Properties[EffectNodes::Variable::AddressV]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_CLAMP_TO_EDGE));
				GLCHECK(glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_R, (node.Properties[EffectNodes::Variable::AddressW] != 0) ? LiteralToTextureWrap(this->mAST[node.Properties[EffectNodes::Variable::AddressW]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_CLAMP_TO_EDGE));
				GLCHECK(glSamplerParameteri(obj->mID, GL_TEXTURE_MIN_FILTER, minfilter));
				GLCHECK(glSamplerParameteri(obj->mID, GL_TEXTURE_MAG_FILTER, (node.Properties[EffectNodes::Variable::MagFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MagFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_LINEAR));
				GLCHECK(glSamplerParameterf(obj->mID, GL_TEXTURE_LOD_BIAS, (node.Properties[EffectNodes::Variable::MipLODBias] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MipLODBias]].As<EffectNodes::Literal>().Value.Float[0] : 0.0f));
				GLCHECK(glSamplerParameterf(obj->mID, GL_TEXTURE_MIN_LOD, (node.Properties[EffectNodes::Variable::MinLOD] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MinLOD]].As<EffectNodes::Literal>().Value.Float[0] : -1000.0f));
				GLCHECK(glSamplerParameterf(obj->mID, GL_TEXTURE_MAX_LOD, (node.Properties[EffectNodes::Variable::MaxLOD] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MaxLOD]].As<EffectNodes::Literal>().Value.Float[0] : 1000.0f));
	#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
				GLCHECK(glSamplerParameterf(obj->mID, GL_TEXTURE_MAX_ANISOTROPY_EXT, (node.Properties[EffectNodes::Variable::MaxAnisotropy] != 0) ? static_cast<GLfloat>(this->mAST[node.Properties[EffectNodes::Variable::MaxAnisotropy]].As<EffectNodes::Literal>().Value.Uint[0]) : 1.0f));

				this->mCurrentSource += "layout(binding = " + std::to_string(this->mEffect->mSamplers.size()) + ") uniform sampler2D ";
				this->mCurrentSource += FixName(node.Name);
				this->mCurrentSource += ";\n";

				this->mEffect->mSamplers.push_back(obj);
			}
			void VisitUniform(const EffectNodes::Variable &node)
			{
				this->mCurrentGlobalConstants += PrintTypeWithQualifiers(node.Type);
				this->mCurrentGlobalConstants += ' ';

				if (!this->mCurrentBlockName.empty())
				{
					this->mCurrentGlobalConstants += this->mCurrentBlockName + '_';
				}
				
				this->mCurrentGlobalConstants += node.Name;

				if (node.Type.IsArray())
				{
					this->mCurrentGlobalConstants += '[';
					this->mCurrentGlobalConstants += (node.Type.ArrayLength >= 1) ? std::to_string(node.Type.ArrayLength) : "";
					this->mCurrentGlobalConstants += ']';
				}

				this->mCurrentGlobalConstants += ";\n";

				GLConstant::Description objdesc;
				objdesc.Rows = node.Type.Rows;
				objdesc.Columns = node.Type.Cols;
				objdesc.Elements = node.Type.ArrayLength;
				objdesc.Fields = 0;
				objdesc.Size = node.Type.Rows * node.Type.Cols;

				switch (node.Type.Class)
				{
					case EffectNodes::Type::Bool:
						objdesc.Size *= sizeof(int);
						objdesc.Type = Effect::Constant::Type::Bool;
						break;
					case EffectNodes::Type::Int:
						objdesc.Size *= sizeof(int);
						objdesc.Type = Effect::Constant::Type::Int;
						break;
					case EffectNodes::Type::Uint:
						objdesc.Size *= sizeof(unsigned int);
						objdesc.Type = Effect::Constant::Type::Uint;
						break;
					case EffectNodes::Type::Float:
						objdesc.Size *= sizeof(float);
						objdesc.Type = Effect::Constant::Type::Float;
						break;
				}

				std::unique_ptr<GLConstant> obj(new GLConstant(this->mEffect, objdesc));

				const std::size_t alignment = 16 - (this->mCurrentGlobalSize % 16);
				this->mCurrentGlobalSize += (objdesc.Size > alignment && (alignment != 16 || objdesc.Size <= 16)) ? objdesc.Size + alignment : objdesc.Size;

				obj->mBuffer = 0;
				obj->mBufferOffset = this->mCurrentGlobalSize - objdesc.Size;

				if (this->mCurrentGlobalSize >= this->mEffect->mUniformStorages[0].second)
				{
					this->mEffect->mUniformStorages[0].first = static_cast<unsigned char *>(::realloc(this->mEffect->mUniformStorages[0].first, this->mEffect->mUniformStorages[0].second += 128));
				}

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), *obj);
				}

				if (node.Initializer != EffectTree::Null && this->mAST[node.Initializer].Is<EffectNodes::Literal>())
				{
					std::memcpy(this->mEffect->mUniformStorages[0].first + obj->mBufferOffset, &this->mAST[node.Initializer].As<EffectNodes::Literal>().Value, objdesc.Size);
				}
				else
				{
					std::memset(this->mEffect->mUniformStorages[0].first + obj->mBufferOffset, 0, objdesc.Size);
				}

				this->mEffect->mConstants.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void VisitUniformBuffer(const EffectNodes::Variable &node)
			{
				const auto &structure = this->mAST[node.Type.Definition].As<EffectNodes::Struct>();

				if (!structure.Fields != EffectTree::Null)
				{
					return;
				}

				this->mCurrentSource += "layout(std140, binding = " + std::to_string(this->mEffect->mUniformBuffers.size()) + ") uniform ";
				this->mCurrentSource += FixName(node.Name);
				this->mCurrentSource += "\n{\n";

				this->mCurrentBlockName = node.Name;

				unsigned char *storage = nullptr;
				std::size_t totalsize = 0, currentsize = 0;

				unsigned int fieldCount = 0;
				const EffectNodes::Variable *field = &this->mAST[structure.Fields].As<EffectNodes::Variable>();

				do
				{
					fieldCount++;

					Visit(*field);

					GLConstant::Description objdesc;
					objdesc.Rows = field->Type.Rows;
					objdesc.Columns = field->Type.Cols;
					objdesc.Elements = field->Type.ArrayLength;
					objdesc.Fields = 0;
					objdesc.Size = field->Type.Rows * field->Type.Cols;

					switch (field->Type.Class)
					{
						case EffectNodes::Type::Bool:
							objdesc.Size *= sizeof(int);
							objdesc.Type = Effect::Constant::Type::Bool;
							break;
						case EffectNodes::Type::Int:
							objdesc.Size *= sizeof(int);
							objdesc.Type = Effect::Constant::Type::Int;
							break;
						case EffectNodes::Type::Uint:
							objdesc.Size *= sizeof(unsigned int);
							objdesc.Type = Effect::Constant::Type::Uint;
							break;
						case EffectNodes::Type::Float:
							objdesc.Size *= sizeof(float);
							objdesc.Type = Effect::Constant::Type::Float;
							break;
					}

					std::unique_ptr<GLConstant> obj(new GLConstant(this->mEffect, objdesc));
					const std::size_t alignment = 16 - (totalsize % 16);
					totalsize += (objdesc.Size > alignment && (alignment != 16 || objdesc.Size <= 16)) ? objdesc.Size + alignment : objdesc.Size;

					obj->mBuffer = this->mEffect->mUniformBuffers.size();
					obj->mBufferOffset = totalsize - objdesc.Size;

					if (totalsize >= currentsize)
					{
						storage = static_cast<unsigned char *>(::realloc(storage, currentsize += 128));
					}

					if (field->Initializer != EffectTree::Null && this->mAST[field->Initializer].Is<EffectNodes::Literal>())
					{
						std::memcpy(storage + obj->mBufferOffset, &this->mAST[field->Initializer].As<EffectNodes::Literal>().Value, objdesc.Size);
					}
					else
					{
						std::memset(storage + obj->mBufferOffset, 0, objdesc.Size);
					}

					this->mEffect->mConstants.insert(std::make_pair(std::string(node.Name) + '.' + std::string(field->Name), std::move(obj)));

					if (field->NextDeclarator != EffectTree::Null)
					{
						field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
					}
					else
					{
						field = nullptr;
					}
				}
				while (field != nullptr);

				this->mCurrentBlockName.clear();

				this->mCurrentSource += "};\n";

				GLConstant::Description objdesc;
				objdesc.Type = Effect::Constant::Type::Struct;
				objdesc.Rows = 0;
				objdesc.Columns = 0;
				objdesc.Elements = 0;
				objdesc.Fields = fieldCount;
				objdesc.Size = totalsize;

				std::unique_ptr<GLConstant> obj(new GLConstant(this->mEffect, objdesc));
				obj->mBuffer = this->mEffect->mUniformBuffers.size();
				obj->mBufferOffset = 0;

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), *obj);
				}

				this->mEffect->mConstants.insert(std::make_pair(node.Name, std::move(obj)));

				GLuint buffer = 0;
				glGenBuffers(1, &buffer);

				GLint previous = 0;
				glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

				glBindBuffer(GL_UNIFORM_BUFFER, buffer);
				glBufferData(GL_UNIFORM_BUFFER, totalsize, storage, GL_DYNAMIC_DRAW);

				glBindBuffer(GL_UNIFORM_BUFFER, previous);

				this->mEffect->mUniformBuffers.push_back(buffer);
				this->mEffect->mUniformStorages.push_back(std::make_pair(storage, totalsize));
			}
			void Visit(const EffectNodes::Function &node)
			{
				this->mCurrentSource += PrintType(node.ReturnType);
				this->mCurrentSource += ' ';
				this->mCurrentSource += FixName(node.Name);
				this->mCurrentSource += '(';

				this->mCurrentFunction = node.Index;

				if (node.Parameters != EffectTree::Null)
				{
					this->mCurrentInParameterBlock = true;

					const EffectNodes::Variable *parameter = &this->mAST[node.Parameters].As<EffectNodes::Variable>();

					do
					{
						Visit(*parameter);

						if (parameter->NextDeclaration != EffectTree::Null)
						{
							this->mCurrentSource += ", ";

							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							parameter = nullptr;
						}
					}
					while (parameter != nullptr);

					this->mCurrentInParameterBlock = false;
				}

				this->mCurrentSource += ')';

				if (node.Definition != EffectTree::Null)
				{
					this->mCurrentSource += '\n';

					this->mCurrentInFunctionBlock = true;

					Visit(this->mAST[node.Definition].As<EffectNodes::StatementBlock>());

					this->mCurrentInFunctionBlock = false;
				}
				else
				{
					this->mCurrentSource += ";\n";
				}

				this->mCurrentFunction = EffectTree::Null;
			}
			void Visit(const EffectNodes::Technique &node)
			{
				std::unique_ptr<GLTechnique> obj(new GLTechnique(this->mEffect));

				const EffectNodes::Pass *pass = &this->mAST[node.Passes].As<EffectNodes::Pass>();

				do
				{
					Visit(*pass, obj->mPasses);

					if (pass->NextPass != EffectTree::Null)
					{
						pass = &this->mAST[pass->NextPass].As<EffectNodes::Pass>();
					}
					else
					{
						pass = nullptr;
					}
				}
				while (pass != nullptr);

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), *obj);
				}

				this->mEffect->mTechniques.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void Visit(const EffectNodes::Pass &node, std::vector<GLTechnique::Pass> &passes)
			{
				GLTechnique::Pass pass;
				ZeroMemory(&pass, sizeof(GLTechnique::Pass));

				if (node.States[EffectNodes::Pass::ColorWriteMask] != 0)
				{
					const GLuint mask = this->mAST[node.States[EffectNodes::Pass::ColorWriteMask]].As<EffectNodes::Literal>().Value.Uint[0];

					pass.ColorMaskR = (mask & (1 << 0)) != 0;
					pass.ColorMaskG = (mask & (1 << 1)) != 0;
					pass.ColorMaskB = (mask & (1 << 2)) != 0;
					pass.ColorMaskA = (mask & (1 << 3)) != 0;
				}
				else
				{
					pass.ColorMaskR = pass.ColorMaskG = pass.ColorMaskB = pass.ColorMaskA = GL_TRUE;
				}

				pass.DepthTest = node.States[EffectNodes::Pass::DepthEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::DepthEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				pass.DepthMask = node.States[EffectNodes::Pass::DepthWriteMask] == 0 || this->mAST[node.States[EffectNodes::Pass::DepthWriteMask]].As<EffectNodes::Literal>().Value.Bool[0];
				pass.DepthFunc = (node.States[EffectNodes::Pass::DepthFunc] != 0) ? LiteralToCompFunc(this->mAST[node.States[EffectNodes::Pass::DepthFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_LESS;
				pass.StencilTest = node.States[EffectNodes::Pass::StencilEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::StencilEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				pass.StencilReadMask = (node.States[EffectNodes::Pass::StencilReadMask] != 0) ? static_cast<UINT8>(this->mAST[node.States[EffectNodes::Pass::StencilReadMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF) : 0xFFFFFFFF;
				pass.StencilMask = (node.States[EffectNodes::Pass::StencilWriteMask] != 0) != 0 ? static_cast<UINT8>(this->mAST[node.States[EffectNodes::Pass::StencilWriteMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF) : 0xFFFFFFFF;
				pass.StencilFunc = (node.States[EffectNodes::Pass::StencilFunc] != 0) ? LiteralToCompFunc(this->mAST[node.States[EffectNodes::Pass::StencilFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_ALWAYS;
				pass.StencilOpZPass = (node.States[EffectNodes::Pass::StencilPass] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilPass]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_KEEP;
				pass.StencilOpFail = (node.States[EffectNodes::Pass::StencilFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilFail]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_KEEP;
				pass.StencilOpZFail = (node.States[EffectNodes::Pass::StencilDepthFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilDepthFail]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_KEEP;
				pass.Blend = node.States[EffectNodes::Pass::BlendEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::BlendEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				pass.BlendEqColor = (node.States[EffectNodes::Pass::BlendOp] != 0) ? LiteralToBlendEq(this->mAST[node.States[EffectNodes::Pass::BlendOp]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_FUNC_ADD;
				pass.BlendEqAlpha = (node.States[EffectNodes::Pass::BlendOpAlpha] != 0) ? LiteralToBlendEq(this->mAST[node.States[EffectNodes::Pass::BlendOpAlpha]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_FUNC_ADD;
				pass.BlendFuncSrc = (node.States[EffectNodes::Pass::SrcBlend] != 0) ? LiteralToBlendFunc(this->mAST[node.States[EffectNodes::Pass::SrcBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_ONE;
				pass.BlendFuncDest = (node.States[EffectNodes::Pass::DestBlend] != 0) ? LiteralToBlendFunc(this->mAST[node.States[EffectNodes::Pass::DestBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : GL_ZERO;

				if (node.States[EffectNodes::Pass::StencilRef] != 0)
				{
					pass.StencilRef = this->mAST[node.States[EffectNodes::Pass::StencilRef]].As<EffectNodes::Literal>().Value.Uint[0];
				}
				if (node.States[EffectNodes::Pass::SRGBWriteEnable] != 0)
				{
					pass.FramebufferSRGB = this->mAST[node.States[EffectNodes::Pass::SRGBWriteEnable]].As<EffectNodes::Literal>().Value.Bool[0] != 0;
				}

				GLCHECK(glGenFramebuffers(1, &pass.Framebuffer));
				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.Framebuffer));

				bool backbufferFramebuffer = true;

				for (unsigned int i = 0; i < 8; ++i)
				{
					if (node.States[EffectNodes::Pass::RenderTarget0 + i] != 0)
					{
						const auto &variable = this->mAST[node.States[EffectNodes::Pass::RenderTarget0 + i]].As<EffectNodes::Variable>();

						const auto it = this->mEffect->mTextures.find(variable.Name);

						if (it == this->mEffect->mTextures.end())
						{
							this->mFatal = true;
							return;
						}

						const GLTexture *texture = it->second.get();
						const GLTexture::Description desc = texture->GetDescription();

						if ((desc.Width != static_cast<unsigned int>(pass.ViewportWidth) || desc.Height != static_cast<unsigned int>(pass.ViewportHeight)) && !(pass.ViewportWidth == 0 && pass.ViewportHeight == 0))
						{
							this->mErrors += PrintLocation(node.Location) + "Cannot use multiple rendertargets with different sized textures.\n";
							this->mFatal = true;
							return;
						}

						backbufferFramebuffer = false;

						GLCHECK(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->mID[pass.FramebufferSRGB], 0));

						pass.DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
						pass.ViewportWidth = desc.Width;
						pass.ViewportHeight = desc.Height;
					}
				}

				if (backbufferFramebuffer)
				{
					GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, this->mEffect->mEffectContext->mBackBufferRBO));

					RECT rect;
					GetClientRect(WindowFromDC(this->mEffect->mEffectContext->mDeviceContext), &rect);

					pass.DrawBuffers[0] = GL_COLOR_ATTACHMENT0;
					pass.ViewportWidth = static_cast<GLsizei>(rect.right - rect.left);
					pass.ViewportHeight = static_cast<GLsizei>(rect.bottom - rect.top);
				}

				GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->mEffect->mDepthStencil));

				assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

				pass.Program = glCreateProgram();

				GLuint shaders[2] = { 0, 0 };

				if (node.States[EffectNodes::Pass::VertexShader] != 0)
				{
					shaders[0] = VisitShader(this->mAST[node.States[EffectNodes::Pass::VertexShader]].As<EffectNodes::Function>(), EffectNodes::Pass::VertexShader);

					if (shaders[0] != 0)
					{
						GLCHECK(glAttachShader(pass.Program, shaders[0]));
					}
				}
				if (node.States[EffectNodes::Pass::PixelShader] != 0)
				{
					shaders[1] = VisitShader(this->mAST[node.States[EffectNodes::Pass::PixelShader]].As<EffectNodes::Function>(), EffectNodes::Pass::PixelShader);

					if (shaders[1] != 0)
					{
						GLCHECK(glAttachShader(pass.Program, shaders[1]));
					}
				}

				GLCHECK(glLinkProgram(pass.Program));

				for (unsigned int i = 0; i < 2; ++i)
				{
					if (shaders[i] == 0)
					{
						continue;
					}

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

					this->mErrors += log;
					this->mFatal = true;
					return;
				}

				passes.push_back(std::move(pass));
			}
			GLuint VisitShader(const EffectNodes::Function &node, unsigned int type)
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
					"#define _textureLodOffset(s, c, offset) textureLodOffset(s, (c).xy, (c).w, offset)\n"
					"vec4 _textureBias(sampler2D s, vec4 c) { return textureOffset(s, c.xy, ivec2(0), c.w); }\n";

				if (!this->mCurrentGlobalConstants.empty())
				{
					source += "layout(std140, binding = 0) uniform _GLOBAL_\n{\n" + this->mCurrentGlobalConstants + "};\n";
				}

				if (type != EffectNodes::Pass::PixelShader)
				{
					source += "#define discard\n";
				}

				source += this->mCurrentSource;

				if (node.Parameters != EffectTree::Null)
				{
					const EffectNodes::Variable *parameter = &this->mAST[node.Parameters].As<EffectNodes::Variable>();

					do
					{
						if (parameter->Type.IsStruct())
						{
							const EffectTree::Index fields = this->mAST[parameter->Type.Definition].As<EffectNodes::Struct>().Fields;

							if (fields != EffectTree::Null)
							{
								const EffectNodes::Variable *field = &this->mAST[fields].As<EffectNodes::Variable>();

								do
								{
									VisitShaderVariable(parameter->Type.Qualifiers, field->Type, "_param_" + std::string(parameter->Name) + "_" + std::string(field->Name), field->Semantic, source);

									if (field->NextDeclarator != EffectTree::Null)
									{
										field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
									}
									else
									{
										field = nullptr;
									}
								}
								while (field != nullptr);
							}
						}
						else
						{
							VisitShaderVariable(parameter->Type.Qualifiers, parameter->Type, "_param_" + std::string(parameter->Name), parameter->Semantic, source);
						}

						if (parameter->NextDeclaration != EffectTree::Null)
						{
							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							parameter = nullptr;
						}
					}
					while (parameter != nullptr);
				}

				if (node.ReturnType.IsStruct())
				{
					const EffectTree::Index fields = this->mAST[node.ReturnType.Definition].As<EffectNodes::Struct>().Fields;

					if (fields != EffectTree::Null)
					{
						const EffectNodes::Variable *field = &this->mAST[fields].As<EffectNodes::Variable>();

						do
						{
							VisitShaderVariable(EffectNodes::Type::Out, field->Type, "_return_" + std::string(field->Name), field->Semantic, source);

							if (field->NextDeclarator != EffectTree::Null)
							{
								field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
							}
							else
							{
								field = nullptr;
							}
						}
						while (field != nullptr);
					}
				}
				else if (!node.ReturnType.IsVoid())
				{
					VisitShaderVariable(EffectNodes::Type::Out, node.ReturnType, "_return", node.ReturnSemantic, source);
				}

				source += "void main()\n{\n";

				if (node.Parameters != EffectTree::Null)
				{
					const EffectNodes::Variable *parameter = &this->mAST[node.Parameters].As<EffectNodes::Variable>();

					do
					{
						if (parameter->Type.IsStruct())
						{
							source += PrintType(parameter->Type) + " _param_" + std::string(parameter->Name) + " = " + this->mAST[parameter->Type.Definition].As<EffectNodes::Struct>().Name + "(";

							const EffectTree::Index fields = this->mAST[parameter->Type.Definition].As<EffectNodes::Struct>().Fields;

							if (fields != EffectTree::Null)
							{
								const EffectNodes::Variable *field = &this->mAST[fields].As<EffectNodes::Variable>();

								do
								{
									if (field->Semantic == nullptr)
									{
										source += "_param_" + std::string(parameter->Name) + "_" + std::string(field->Name);
									}
									else if (::strcmp(field->Semantic, "SV_VERTEXID") == 0)
									{
										source += "gl_VertexID";
									}
									else if (::strcmp(field->Semantic, "SV_INSTANCEID") == 0)
									{
										source += "gl_InstanceID";
									}
									else if (::strcmp(field->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::VertexShader)
									{
										source += "gl_Position";
									}
									else if (::strcmp(field->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::PixelShader)
									{
										source += "gl_FragCoord";
									}
									else if (::strcmp(field->Semantic, "SV_DEPTH") == 0 && type == EffectNodes::Pass::PixelShader)
									{
										source += "gl_FragDepth";
									}
									else
									{
										source += "_param_" + std::string(parameter->Name) + "_" + std::string(field->Name);
									}

									if (field->NextDeclarator != EffectTree::Null)
									{
										source += ", ";

										field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
									}
									else
									{
										field = nullptr;
									}
								}
								while (field != nullptr);
							}

							source += ");\n";
						}

						if (parameter->NextDeclaration != EffectTree::Null)
						{
							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							parameter = nullptr;
						}
					}
					while (parameter != nullptr);
				}
				if (node.ReturnType.IsStruct())
				{
					source += PrintType(node.ReturnType);
					source += " ";
				}

				if (!node.ReturnType.IsVoid())
				{
					source += "_return = ";
				}

				source += FixName(node.Name);
				source += "(";

				if (node.Parameters != EffectTree::Null)
				{
					const EffectNodes::Variable *parameter = &this->mAST[node.Parameters].As<EffectNodes::Variable>();

					do
					{
						if (parameter->Semantic == nullptr)
						{
							source += "_param_" + std::string(parameter->Name);
						}
						else if (::strcmp(parameter->Semantic, "SV_VERTEXID") == 0)
						{
							source += "gl_VertexID";
						}
						else if (::strcmp(parameter->Semantic, "SV_INSTANCEID") == 0)
						{
							source += "gl_InstanceID";
						}
						else if (::strcmp(parameter->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::VertexShader)
						{
							source += "gl_Position";
						}
						else if (::strcmp(parameter->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::PixelShader)
						{
							source += "gl_FragCoord";
						}
						else if (::strcmp(parameter->Semantic, "SV_DEPTH") == 0 && type == EffectNodes::Pass::PixelShader)
						{
							source += "gl_FragDepth";
						}
						else
						{
							source += "_param_" + std::string(parameter->Name);
						}

						if (parameter->NextDeclaration != EffectTree::Null)
						{
							source += ", ";

							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							parameter = nullptr;
						}
					}
					while (parameter != nullptr);
				}

				source += ");\n";

				if (node.Parameters != EffectTree::Null)
				{
					const EffectNodes::Variable *parameter = &this->mAST[node.Parameters].As<EffectNodes::Variable>();
				
					do
					{
						if (parameter->Type.IsStruct() && parameter->Type.HasQualifier(EffectNodes::Type::Qualifier::Out))
						{
							const EffectTree::Index fields = this->mAST[parameter->Type.Definition].As<EffectNodes::Struct>().Fields;

							if (fields != EffectTree::Null)
							{
								const EffectNodes::Variable *field = &this->mAST[fields].As<EffectNodes::Variable>();

								do
								{
									source += "_param_" + std::string(parameter->Name) + "_" + std::string(field->Name) + " = " + "_param_" + std::string(parameter->Name) + "." + std::string(field->Name) + ";\n";

									if (field->NextDeclarator != EffectTree::Null)
									{
										source += ", ";

										field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
									}
									else
									{
										field = nullptr;
									}
								}
								while (field != nullptr);
							}
						}

						if (parameter->NextDeclaration != EffectTree::Null)
						{
							parameter = &this->mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
						}
						else
						{
							parameter = nullptr;
						}
					}
					while (parameter != nullptr);
				}

				if (node.ReturnType.IsStruct())
				{
					const EffectTree::Index fields = this->mAST[node.ReturnType.Definition].As<EffectNodes::Struct>().Fields;

					if (fields != EffectTree::Null)
					{
						const EffectNodes::Variable *field = &this->mAST[fields].As<EffectNodes::Variable>();

						do
						{
							if (::strcmp(field->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::VertexShader)
							{
								source += "gl_Position";
							}
							else if (::strcmp(field->Semantic, "SV_POSITION") == 0 && type == EffectNodes::Pass::PixelShader)
							{
								source += "gl_FragCoord";
							}
							else if (::strcmp(field->Semantic, "SV_DEPTH") == 0 && type == EffectNodes::Pass::PixelShader)
							{
								source += "gl_FragDepth";
							}
							else
							{
								source += "_return_" + std::string(field->Name);
							}

							source += " = _return." + std::string(field->Name) + ";\n";

							if (field->NextDeclarator != EffectTree::Null)
							{
								field = &this->mAST[field->NextDeclarator].As<EffectNodes::Variable>();
							}
							else
							{
								field = nullptr;
							}
						}
						while (field != nullptr);
					}
				}
			
				if (type == EffectNodes::Pass::VertexShader)
				{
					source += "gl_Position = gl_Position * vec4(1.0, 1.0, 2.0, 1.0) + vec4(0.0, 0.0, -gl_Position.w, 0.0);\n";
				}
				/*if (type == EffectNodes::Pass::PixelShader)
				{
					source += "gl_FragDepth = clamp(gl_FragDepth, 0.0, 1.0);\n";
				}*/

				source += "}\n";

				GLuint shader;

				switch (type)
				{
					default:
						return 0;
					case EffectNodes::Pass::VertexShader:
						shader = glCreateShader(GL_VERTEX_SHADER);
						break;
					case EffectNodes::Pass::PixelShader:
						shader = glCreateShader(GL_FRAGMENT_SHADER);
						break;
				}

				const GLchar *src = source.c_str();
				const GLsizei len = static_cast<GLsizei>(source.length());

				LOG(TRACE) << "> Compiling shader '" << node.Name << "':\n\n" << source.c_str() << "\n";

				glShaderSource(shader, 1, &src, &len);
				glCompileShader(shader);

				GLint status = GL_FALSE;

				glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

				if (status == GL_FALSE)
				{
					GLint logsize = 0;
					glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logsize);

					std::string log(logsize, '\0');
					glGetShaderInfoLog(shader, logsize, nullptr, &log.front());

					glDeleteShader(shader);

					this->mErrors += log;
					this->mFatal = true;
					return 0;
				}

				return shader;
			}
			void VisitShaderVariable(unsigned int qualifier, EffectNodes::Type type, const std::string &name, const char *semantic, std::string &source)
			{
				unsigned int location = 0;

				if (semantic == nullptr)
				{
					return;
				}
				else if (::strcmp(semantic, "SV_VERTEXID") == 0 || ::strcmp(semantic, "SV_INSTANCEID") == 0 || ::strcmp(semantic, "SV_POSITION") == 0)
				{
					return;
				}
				else if (::strstr(semantic, "SV_TARGET") == semantic)
				{
					location = static_cast<unsigned int>(::strtol(semantic + 9, nullptr, 10));
				}
				else if (::strstr(semantic, "TEXCOORD") == semantic)
				{
					location = static_cast<unsigned int>(::strtol(semantic + 8, nullptr, 10)) + 1;
				}

				source += "layout(location = " + std::to_string(location) + ") ";

				type.Qualifiers = static_cast<unsigned int>(qualifier);

				source += PrintTypeWithQualifiers(type) + ' ' + name;

				if (type.IsArray())
				{
					source += "[" + (type.ArrayLength >= 1 ? std::to_string(type.ArrayLength) : "") + "]";
				}

				source += ";\n";
			}

		private:
			const EffectTree &mAST;
			GLEffect *mEffect;
			std::string mCurrentSource;
			std::string mErrors;
			bool mFatal;
			std::string mCurrentGlobalConstants;
			std::size_t mCurrentGlobalSize, mCurrentGlobalStorageSize;
			std::string mCurrentBlockName;
			EffectTree::Index mCurrentFunction;
			bool mCurrentInParameterBlock, mCurrentInFunctionBlock, mCurrentInDeclaratorList;
		};

		GLenum TargetToBinding(GLenum target)
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
		inline void FlipBC1Block(unsigned char *block)
		{
			// BC1 Block:
			//  [0-1]  color 0
			//  [2-3]  color 1
			//  [4-7]  color indices

			std::swap(block[4], block[7]);
			std::swap(block[5], block[6]);
		}
		inline void FlipBC2Block(unsigned char *block)
		{
			// BC2 Block:
			//  [0-7]  alpha indices
			//  [8-15] color block

			std::swap(block[0], block[6]);
			std::swap(block[1], block[7]);
			std::swap(block[2], block[4]);
			std::swap(block[3], block[5]);

			FlipBC1Block(block + 8);
		}
		inline void FlipBC4Block(unsigned char *block)
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
		inline void FlipBC3Block(unsigned char *block)
		{
			// BC3 Block:
			//  [0-7]  alpha block
			//  [8-15] color block

			FlipBC4Block(block);
			FlipBC1Block(block + 8);
		}
		inline void FlipBC5Block(unsigned char *block)
		{
			// BC5 Block:
			//  [0-7]  red block
			//  [8-15] green block

			FlipBC4Block(block);
			FlipBC4Block(block + 8);
		}
		void FlipImageData(const Effect::Texture::Description &desc, unsigned char *data)
		{
			typedef void (*FlipBlockFunc)(unsigned char *block);

			std::size_t blocksize = 0;
			bool compressed = false;
			FlipBlockFunc compressedFunc = nullptr;

			switch (desc.Format)
			{
				case Effect::Texture::Format::R8:
					blocksize = 1;
					break;
				case Effect::Texture::Format::RG8:
					blocksize = 2;
					break;
				case Effect::Texture::Format::R32F:
				case Effect::Texture::Format::RGBA8:
					blocksize = 4;
					break;
				case Effect::Texture::Format::RGBA16:
				case Effect::Texture::Format::RGBA16F:
					blocksize = 8;
					break;
				case Effect::Texture::Format::RGBA32F:
					blocksize = 16;
					break;
				case Effect::Texture::Format::DXT1:
					blocksize = 8;
					compressed = true;
					compressedFunc = &FlipBC1Block;
					break;
				case Effect::Texture::Format::DXT3:
					blocksize = 16;
					compressed = true;
					compressedFunc = &FlipBC2Block;
					break;
				case Effect::Texture::Format::DXT5:
					blocksize = 16;
					compressed = true;
					compressedFunc = &FlipBC3Block;
					break;
				case Effect::Texture::Format::LATC1:
					blocksize = 8;
					compressed = true;
					compressedFunc = &FlipBC4Block;
					break;
				case Effect::Texture::Format::LATC2:
					blocksize = 16;
					compressed = true;
					compressedFunc = &FlipBC5Block;
					break;
				default:
					return;
			}

			if (compressed)
			{
				const std::size_t w = (desc.Width + 3) / 4;
				const std::size_t h = (desc.Height + 3) / 4;
				const std::size_t stride = w * blocksize;

				for (std::size_t y = 0; y < h; ++y)
				{
					unsigned char *dataLine = data + stride * (h - 1 - y);

					for (std::size_t x = 0; x < stride; x += blocksize)
					{
						compressedFunc(dataLine + x);
					}
				}
			}
			else
			{
				const std::size_t w = desc.Width;
				const std::size_t h = desc.Height;
				const std::size_t stride = w * blocksize;
				unsigned char *templine = static_cast<unsigned char *>(::alloca(stride));

				for (std::size_t y = 0; 2 * y < h; ++y)
				{
					unsigned char *line1 = data + stride * y;
					unsigned char *line2 = data + stride * (h - 1 - y);

					std::memcpy(templine, line1, stride);
					std::memcpy(line1, line2, stride);
					std::memcpy(line2, templine, stride);
				}
			}
		}
	}

	// -----------------------------------------------------------------------------------------------------

	GLRuntime::GLRuntime(HDC device, HGLRC context) : mDeviceContext(device), mRenderContext(context), mBackBufferFBO(0), mBackBufferRBO(0), mDepthStencilFBO(0), mDepthStencilTexture(0), mBlitFBO(0), mLost(true), mPresenting(false)
	{
		assert(device != nullptr);
		assert(context != nullptr);

		this->mRendererId = 0x61;

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
						this->mVendorId = std::stoi(id.substr(8, 4));
						this->mDeviceId = std::stoi(id.substr(17, 4));
					}
					break;
				}
			}
		}

		if (this->mVendorId == 0)
		{
			const char *name = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

			if (name != nullptr)
			{
				if (boost::contains(name, "NVIDIA"))
				{
					this->mVendorId = 0x10DE;
				}
				else if (boost::contains(name, "AMD") || boost::contains(name, "ATI"))
				{
					this->mVendorId = 0x1002;
				}
				else if (boost::contains(name, "Intel"))
				{
					this->mVendorId = 0x8086;
				}
			}
		}
	}
	GLRuntime::~GLRuntime()
	{
		assert(this->mLost);
	}

	bool GLRuntime::OnCreate(unsigned int width, unsigned int height)
	{
		this->mStateBlock.Capture();

		GLCHECK(glGenRenderbuffers(1, &this->mBackBufferRBO));
		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, this->mBackBufferRBO));
		GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height));
		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

		GLCHECK(glGenFramebuffers(1, &this->mBackBufferFBO));
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, this->mBackBufferFBO));
		GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, this->mBackBufferRBO));
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		GLCHECK(glGenFramebuffers(1, &this->mBlitFBO));

		GLFramebufferInfo defaultfbo = { 0 };
		defaultfbo.DepthStencilWidth = width;
		defaultfbo.DepthStencilHeight = height;
		defaultfbo.DepthStencilFormat = GL_DEPTH24_STENCIL8;
		defaultfbo.DepthStencilTarget = GL_FRAMEBUFFER_DEFAULT;
		this->mFramebufferTable[0] = defaultfbo;

		GLCHECK(glGenTextures(1, &this->mDepthStencilTexture));
		GLCHECK(glBindTexture(GL_TEXTURE_2D, this->mDepthStencilTexture));
		GLCHECK(glTexStorage2D(GL_TEXTURE_2D, 1, defaultfbo.DepthStencilFormat, defaultfbo.DepthStencilWidth, defaultfbo.DepthStencilHeight));
		GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));

		this->mNVG = nvgCreateGL3(0);

		this->mLost = false;

		const bool res = Runtime::OnCreate(width, height);

		this->mStateBlock.Apply();

		return res;
	}
	void GLRuntime::OnDelete()
	{
		if (!this->mPresenting)
		{
			this->mStateBlock.Capture();
		}

		Runtime::OnDelete();

		nvgDeleteGL3(this->mNVG);

		this->mNVG = nullptr;

		GLCHECK(glDeleteFramebuffers(1, &this->mBackBufferFBO));			
		GLCHECK(glDeleteFramebuffers(1, &this->mBlitFBO));			
		GLCHECK(glDeleteRenderbuffers(1, &this->mBackBufferRBO));
		GLCHECK(glDeleteTextures(1, &this->mDepthStencilTexture));

		this->mBackBufferFBO = 0;
		this->mBackBufferRBO = 0;
		this->mDepthStencilFBO = 0;
		this->mDepthStencilTexture = 0;
		this->mBlitFBO = 0;

		this->mFramebufferTable.clear();

		this->mStateBlock.Apply();

		this->mLost = true;
		this->mPresenting = false;
	}
	void GLRuntime::OnDraw(unsigned int vertices)
	{
		Runtime::OnDraw(vertices);

		GLint fbo = 0;
		GLCHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo));

		const auto it = this->mFramebufferTable.find(fbo);

		if (it != this->mFramebufferTable.end())
		{
			it->second.DrawCallCount = static_cast<GLfloat>(this->mLastDrawCalls++);
			it->second.DrawVerticesCount += vertices;
		}
	}
	void GLRuntime::OnPresent()
	{
		if (this->mLost)
		{
			return;
		}

		DetectBestDepthStencil();

		// Capture states
		this->mStateBlock.Capture();

		// Copy depthbuffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, this->mDepthStencilFBO));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->mBlitFBO));
		GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, this->mDepthStencilTexture, 0));
		assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		GLCHECK(glBlitFramebuffer(0, 0, this->mWidth, this->mHeight, 0, 0, this->mWidth, this->mHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST));
		GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 0, 0));
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

		// Apply post processing
		this->mPresenting = true;
		Runtime::OnPresent();
		this->mPresenting = false;

		if (this->mLost)
		{
			return;
		}

		// Apply states
		this->mStateBlock.Apply();
	}

	std::unique_ptr<Effect> GLRuntime::CreateEffect(const EffectTree &ast, std::string &errors) const
	{
		GLEffect *effect = new GLEffect(shared_from_this());
			
		GLEffectCompiler visitor(ast);
		
		if (visitor.Traverse(effect, errors))
		{
			return std::unique_ptr<Effect>(effect);
		}
		else
		{
			delete effect;

			return nullptr;
		}
	}
	void GLRuntime::CreateScreenshot(unsigned char *buffer, std::size_t size) const
	{
		GLCHECK(glReadBuffer(GL_BACK));

		const unsigned int pitch = this->mWidth * 4;

		assert(size >= pitch * this->mHeight);

		GLCHECK(glReadPixels(0, 0, static_cast<GLsizei>(this->mWidth), static_cast<GLsizei>(this->mHeight), GL_RGBA, GL_UNSIGNED_BYTE, buffer));
			
		for (unsigned int y = 0; y * 2 < this->mHeight; ++y)
		{
			const unsigned int i1 = y * pitch;
			const unsigned int i2 = (this->mHeight - 1 - y) * pitch;

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

	void GLRuntime::CreateDepthStencil(GLenum target, GLenum objecttarget, GLuint object, GLint level)
	{
		if (object == 0)
		{
			return;
		}

		// Get current framebuffer
		GLint fbo = 0;
		GLCHECK(glGetIntegerv(TargetToBinding(target), &fbo));

		assert(fbo != 0);

		GLFramebufferInfo &info = this->mFramebufferTable[fbo];
		info.DepthStencilName = object;
		info.DepthStencilTarget = objecttarget;

		if (objecttarget == GL_RENDERBUFFER)
		{
			GLint previous = 0;
			GLCHECK(glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous));

			// Get depthstencil parameters from renderbuffer
			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, object));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &info.DepthStencilWidth));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &info.DepthStencilHeight));
			GLCHECK(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &info.DepthStencilFormat));

			GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, previous));
		}
		else
		{
			if (objecttarget == GL_TEXTURE)
			{
				objecttarget = GL_TEXTURE_2D;
			}

			GLint previous = 0;
			GLCHECK(glGetIntegerv(TargetToBinding(objecttarget), &previous));

			// Get depthstencil parameters from texture
			GLCHECK(glBindTexture(objecttarget, object));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_WIDTH, &info.DepthStencilWidth));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_HEIGHT, &info.DepthStencilHeight));
			GLCHECK(glGetTexLevelParameteriv(objecttarget, level, GL_TEXTURE_INTERNAL_FORMAT, &info.DepthStencilFormat));
			
			GLCHECK(glBindTexture(objecttarget, previous));
		}
	}
	void GLRuntime::DetectBestDepthStencil()
	{
		static int cooldown = 0;

		if (cooldown-- > 0)
		{
			return;
		}
		else
		{
			cooldown = 30;
		}

		GLuint best = 0;
		GLFramebufferInfo bestInfo = { 0 };

		for (auto &it : this->mFramebufferTable)
		{
			if (it.second.DrawCallCount == 0)
			{
				continue;
			}

			if (((it.second.DrawVerticesCount * (1.2f - it.second.DrawCallCount / this->mLastDrawCalls)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / this->mLastDrawCalls))) && (it.second.DepthStencilWidth == static_cast<GLint>(this->mWidth) && it.second.DepthStencilHeight == static_cast<GLint>(this->mHeight)))
			{
				best = it.first;
				bestInfo = it.second;
			}

			it.second.DrawCallCount = it.second.DrawVerticesCount = 0;
		}

		if (best == 0)
		{
			bestInfo = this->mFramebufferTable.at(0);
		}

		if (this->mDepthStencilFBO != best)
		{
			GLint previous = 0;
			GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

			GLCHECK(glDeleteTextures(1, &this->mDepthStencilTexture));

			this->mDepthStencilFBO = best;
			this->mDepthStencilTexture = 0;

			GLCHECK(glGenTextures(1, &this->mDepthStencilTexture));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, this->mDepthStencilTexture));
			GLCHECK(glTexStorage2D(GL_TEXTURE_2D, 1, bestInfo.DepthStencilFormat, bestInfo.DepthStencilWidth, bestInfo.DepthStencilHeight));

			GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));
		}
	}

	GLEffect::GLEffect(std::shared_ptr<const GLRuntime> context) : mEffectContext(context), mDefaultVAO(0), mDefaultVBO(0), mDepthStencil(0), mUniformDirty(true)
	{
		GLCHECK(glGenVertexArrays(1, &this->mDefaultVAO));
		GLCHECK(glGenBuffers(1, &this->mDefaultVBO));
		GLCHECK(glGenRenderbuffers(1, &this->mDepthStencil));

		GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, this->mDefaultVBO));
		GLCHECK(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW));
		GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));

		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, this->mDepthStencil));
		GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, this->mEffectContext->mWidth, this->mEffectContext->mHeight));
		GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	}
	GLEffect::~GLEffect()
	{
		GLCHECK(glDeleteVertexArrays(1, &this->mDefaultVAO));
		GLCHECK(glDeleteBuffers(1, &this->mDefaultVBO));
		GLCHECK(glDeleteRenderbuffers(1, &this->mDepthStencil));
		GLCHECK(glDeleteBuffers(this->mUniformBuffers.size(), &this->mUniformBuffers.front()));
	}

	const Effect::Texture *GLEffect::GetTexture(const std::string &name) const
	{
		auto it = this->mTextures.find(name);

		if (it == this->mTextures.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> GLEffect::GetTextureNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mTextures.size());

		for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}
	const Effect::Constant *GLEffect::GetConstant(const std::string &name) const
	{
		auto it = this->mConstants.find(name);

		if (it == this->mConstants.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> GLEffect::GetConstantNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mConstants.size());

		for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}
	const Effect::Technique *GLEffect::GetTechnique(const std::string &name) const
	{
		auto it = this->mTechniques.find(name);

		if (it == this->mTechniques.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> GLEffect::GetTechniqueNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mTechniques.size());

		for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}

	void GLEffect::UpdateConstants() const
	{
		if (!this->mUniformDirty)
		{
			return;
		}

		for (GLsizei buffer = 0, bufferCount = static_cast<GLsizei>(this->mUniformBuffers.size()); buffer < bufferCount; ++buffer)
		{
			if (this->mUniformBuffers[buffer] == 0)
			{
				continue;
			}

			GLCHECK(glBindBuffer(GL_UNIFORM_BUFFER, this->mUniformBuffers[buffer]));
			GLCHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, this->mUniformStorages[buffer].second, this->mUniformStorages[buffer].first));
		}

		this->mUniformDirty = false;
	}

	GLTexture::GLTexture(GLEffect *effect, const Description &desc) : Texture(desc), mEffect(effect), mID(), mNoDelete(false)
	{
	}
	GLTexture::~GLTexture()
	{
		if (!this->mNoDelete)
		{
			GLCHECK(glDeleteTextures(2, this->mID));
		}
	}

	bool GLTexture::Update(unsigned int level, const unsigned char *data, std::size_t size)
	{
		if (data == nullptr || size == 0 || level > this->mDesc.Levels)
		{
			return false;
		}

		GLCHECK(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_ROWS, 0));
		GLCHECK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0));

		GLint previous = 0;
		GLCHECK(glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous));

		GLCHECK(glBindTexture(GL_TEXTURE_2D, this->mID[0]));

		const std::unique_ptr<unsigned char[]> dataFlipped(new unsigned char[size]);
		std::memcpy(dataFlipped.get(), data, size);

		// Flip image vertically
		FlipImageData(this->mDesc, dataFlipped.get());

		if (this->mDesc.Format >= Texture::Format::DXT1 && this->mDesc.Format <= Texture::Format::LATC2)
		{
			GLCHECK(glCompressedTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, this->mDesc.Width, this->mDesc.Height, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), dataFlipped.get()));
		}
		else
		{
			GLint dataAlignment = 4;
			GLenum dataFormat = GL_RGBA, dataType = GL_UNSIGNED_BYTE;
				
			switch (this->mDesc.Format)
			{
				case Texture::Format::R8:
					dataFormat = GL_RED;
					dataAlignment = 1;
					break;
				case Texture::Format::R32F:
					dataType = GL_FLOAT;
					dataFormat = GL_RED;
					break;
				case Texture::Format::RG8:
					dataFormat = GL_RG;
					dataAlignment = 2;
					break;
				case Texture::Format::RGBA16:
				case Texture::Format::RGBA16F:
					dataType = GL_UNSIGNED_SHORT;
					dataAlignment = 2;
					break;
				case Texture::Format::RGBA32F:
					dataType = GL_FLOAT;
					break;
			}

			GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, dataAlignment));
			GLCHECK(glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, this->mDesc.Width, this->mDesc.Height, dataFormat, dataType, dataFlipped.get()));
			GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
		}

		GLCHECK(glBindTexture(GL_TEXTURE_2D, previous));

		return true;
	}
	void GLTexture::UpdateFromColorBuffer()
	{
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, this->mEffect->mEffectContext->mBackBufferFBO));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->mEffect->mEffectContext->mBlitFBO));
		GLCHECK(glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->mID[0], 0));
		assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
		GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glBlitFramebuffer(0, 0, this->mEffect->mEffectContext->mWidth, this->mEffect->mEffectContext->mHeight, 0, 0, this->mDesc.Width, this->mDesc.Height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
	}
	void GLTexture::UpdateFromDepthBuffer()
	{
		if (this->mID[0] == this->mEffect->mEffectContext->mDepthStencilTexture || this->mEffect->mEffectContext->mDepthStencilTexture == 0)
		{
			return;
		}
		else if (!this->mNoDelete)
		{
			GLCHECK(glDeleteTextures(2, this->mID));
		}

		// Replace with depth texture
		this->mID[0] = this->mID[1] = this->mEffect->mEffectContext->mDepthStencilTexture;
		this->mNoDelete = true;
	}

	GLConstant::GLConstant(GLEffect *effect, const Description &desc) : Constant(desc), mEffect(effect)
	{
	}
	GLConstant::~GLConstant()
	{
	}

	void GLConstant::GetValue(unsigned char *data, std::size_t size) const
	{
		size = std::min(size, this->mDesc.Size);

		std::memcpy(data, this->mEffect->mUniformStorages[this->mBuffer].first + this->mBufferOffset, size);
	}
	void GLConstant::SetValue(const unsigned char *data, std::size_t size)
	{
		size = std::min(size, this->mDesc.Size);

		unsigned char *storage = this->mEffect->mUniformStorages[this->mBuffer].first + this->mBufferOffset;

		if (std::memcmp(storage, data, size) == 0)
		{
			return;
		}

		std::memcpy(storage, data, size);

		this->mEffect->mUniformDirty = true;
	}

	GLTechnique::GLTechnique(GLEffect *effect) : Technique(), mEffect(effect)
	{
	}
	GLTechnique::~GLTechnique()
	{
		for (auto &pass : this->mPasses)
		{
			GLCHECK(glDeleteProgram(pass.Program));
			GLCHECK(glDeleteFramebuffers(1, &pass.Framebuffer));
		}
	}

	bool GLTechnique::Begin(unsigned int &passes) const
	{
		passes = static_cast<unsigned int>(this->mPasses.size());

		if (passes == 0)
		{
			return false;
		}

		// Copy backbuffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->mEffect->mEffectContext->mBackBufferFBO));
		GLCHECK(glReadBuffer(GL_BACK));
		GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glBlitFramebuffer(0, 0, this->mEffect->mEffectContext->mWidth, this->mEffect->mEffectContext->mHeight, 0, 0, this->mEffect->mEffectContext->mWidth, this->mEffect->mEffectContext->mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));

		// Setup vertex input
		GLCHECK(glBindVertexArray(this->mEffect->mDefaultVAO));
		GLCHECK(glBindVertexBuffer(0, this->mEffect->mDefaultVBO, 0, sizeof(float)));		

		// Setup shader resources
		for (GLsizei sampler = 0, samplerCount = static_cast<GLsizei>(this->mEffect->mSamplers.size()); sampler < samplerCount; ++sampler)
		{
			GLCHECK(glActiveTexture(GL_TEXTURE0 + sampler));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, this->mEffect->mSamplers[sampler]->mTexture->mID[this->mEffect->mSamplers[sampler]->mSRGB]));
			GLCHECK(glBindSampler(sampler, this->mEffect->mSamplers[sampler]->mID));
		}

		// Setup shader constants
		for (GLsizei buffer = 0, bufferCount = static_cast<GLsizei>(this->mEffect->mUniformBuffers.size()); buffer < bufferCount; ++buffer)
		{
			GLCHECK(glBindBufferBase(GL_UNIFORM_BUFFER, buffer, this->mEffect->mUniformBuffers[buffer]));
		}

		return true;
	}
	void GLTechnique::End() const
	{
		// Copy to backbuffer
		GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, this->mEffect->mEffectContext->mBackBufferFBO));
		GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
		GLCHECK(glReadBuffer(GL_COLOR_ATTACHMENT0));
		GLCHECK(glDrawBuffer(GL_BACK));
		GLCHECK(glBlitFramebuffer(0, 0, this->mEffect->mEffectContext->mWidth, this->mEffect->mEffectContext->mHeight, 0, 0, this->mEffect->mEffectContext->mWidth, this->mEffect->mEffectContext->mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST));

		// Reset rendertarget
		GLCHECK(glBindSampler(0, 0));
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}
	void GLTechnique::RenderPass(unsigned int index) const
	{
		const Pass &pass = this->mPasses[index];

		// Update shader constants
		this->mEffect->UpdateConstants();

		// Setup states
		GLCHECK(glUseProgram(pass.Program));
		GLCHECK(glEnableb(GL_FRAMEBUFFER_SRGB, pass.FramebufferSRGB));
		GLCHECK(glDisable(GL_SCISSOR_TEST));
		GLCHECK(glFrontFace(GL_CCW));
		GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
		GLCHECK(glDisable(GL_CULL_FACE));
		GLCHECK(glColorMask(pass.ColorMaskR, pass.ColorMaskG, pass.ColorMaskB, pass.ColorMaskA));
		GLCHECK(glEnableb(GL_BLEND, pass.Blend));
		GLCHECK(glBlendFunc(pass.BlendFuncSrc, pass.BlendFuncDest));
		GLCHECK(glBlendEquationSeparate(pass.BlendEqColor, pass.BlendEqAlpha));
		GLCHECK(glEnableb(GL_DEPTH_TEST, pass.DepthTest));
		GLCHECK(glDepthMask(pass.DepthMask));
		GLCHECK(glDepthFunc(pass.DepthFunc));
		GLCHECK(glEnableb(GL_STENCIL_TEST, pass.StencilTest));
		GLCHECK(glStencilFunc(pass.StencilFunc, pass.StencilRef, pass.StencilReadMask));
		GLCHECK(glStencilOp(pass.StencilOpFail, pass.StencilOpZFail, pass.StencilOpZPass));
		GLCHECK(glStencilMask(pass.StencilMask));

		// Setup rendertargets
		GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.Framebuffer));
		GLCHECK(glDrawBuffers(8, pass.DrawBuffers));
		GLCHECK(glViewport(0, 0, pass.ViewportWidth, pass.ViewportHeight));

		for (GLuint i = 0; i < 8; ++i)
		{
			if (pass.DrawBuffers[i] == GL_NONE)
			{
				continue;
			}

			const GLfloat color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			GLCHECK(glClearBufferfv(GL_COLOR, i, color));
		}

		// Draw triangle
		GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
	}
}