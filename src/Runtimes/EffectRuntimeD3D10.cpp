#include "Log.hpp"
#include "EffectParserTree.hpp"
#include "EffectRuntimeD3D10.hpp"

#include <d3dcompiler.h>
#include <nanovg_d3d10.h>

// -----------------------------------------------------------------------------------------------------

inline bool operator ==(const D3D10_SAMPLER_DESC &left, const D3D10_SAMPLER_DESC &right)
{
	return std::memcmp(&left, &right, sizeof(D3D10_SAMPLER_DESC)) == 0;
}

namespace ReShade
{
	namespace
	{
		struct CSLock : boost::noncopyable
		{
			CSLock(CRITICAL_SECTION &cs) : mCS(cs)
			{
				EnterCriticalSection(&this->mCS);
			}
			~CSLock()
			{
				LeaveCriticalSection(&this->mCS);
			}

			CRITICAL_SECTION &mCS;
		};

		class D3D10EffectCompiler : private boost::noncopyable
		{
		public:
			D3D10EffectCompiler(const EffectTree &ast) : mAST(ast), mEffect(nullptr), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentInDeclaratorList(false), mCurrentGlobalSize(0), mCurrentGlobalStorageSize(0)
			{
			}

			bool Traverse(D3D10Effect *effect, std::string &errors)
			{
				this->mEffect = effect;
				this->mErrors.clear();
				this->mFatal = false;
				this->mCurrentSource.clear();

				// Global constant buffer
				this->mEffect->mConstantBuffers.push_back(nullptr);
				this->mEffect->mConstantStorages.push_back(nullptr);

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
					CD3D10_BUFFER_DESC globalsDesc(RoundToMultipleOf16(this->mCurrentGlobalSize), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
					D3D10_SUBRESOURCE_DATA globalsInitial;
					globalsInitial.pSysMem = this->mEffect->mConstantStorages[0];
					globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = this->mCurrentGlobalSize;
					this->mEffect->mEffectContext->mDevice->CreateBuffer(&globalsDesc, &globalsInitial, &this->mEffect->mConstantBuffers[0]);
				}

				errors += this->mErrors;

				return !this->mFatal;
			}

			static inline UINT RoundToMultipleOf16(UINT size)
			{
				return (size + 15) & ~15;
			}
			static D3D10_TEXTURE_ADDRESS_MODE LiteralToTextureAddress(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::CLAMP:
						return D3D10_TEXTURE_ADDRESS_CLAMP;
					case EffectNodes::Literal::REPEAT:
						return D3D10_TEXTURE_ADDRESS_WRAP;
					case EffectNodes::Literal::MIRROR:
						return D3D10_TEXTURE_ADDRESS_MIRROR;
					case EffectNodes::Literal::BORDER:
						return D3D10_TEXTURE_ADDRESS_BORDER;
				}
			}
			static D3D10_COMPARISON_FUNC LiteralToComparisonFunc(int value)
			{
				const D3D10_COMPARISON_FUNC conversion[] =
				{
					D3D10_COMPARISON_NEVER, D3D10_COMPARISON_ALWAYS, D3D10_COMPARISON_LESS, D3D10_COMPARISON_GREATER, D3D10_COMPARISON_LESS_EQUAL, D3D10_COMPARISON_GREATER_EQUAL, D3D10_COMPARISON_EQUAL, D3D10_COMPARISON_NOT_EQUAL
				};

				const unsigned int conversionIndex = value - EffectNodes::Literal::NEVER;

				assert(conversionIndex < ARRAYSIZE(conversion));

				return conversion[conversionIndex];
			}
			static D3D10_STENCIL_OP LiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::KEEP:
						return D3D10_STENCIL_OP_KEEP;
					case EffectNodes::Literal::ZERO:
						return D3D10_STENCIL_OP_ZERO;
					case EffectNodes::Literal::REPLACE:
						return D3D10_STENCIL_OP_REPLACE;
					case EffectNodes::Literal::INCR:
						return D3D10_STENCIL_OP_INCR;
					case EffectNodes::Literal::INCRSAT:
						return D3D10_STENCIL_OP_INCR_SAT;
					case EffectNodes::Literal::DECR:
						return D3D10_STENCIL_OP_DECR;
					case EffectNodes::Literal::DECRSAT:
						return D3D10_STENCIL_OP_DECR_SAT;
					case EffectNodes::Literal::INVERT:
						return D3D10_STENCIL_OP_INVERT;
				}
			}
			static D3D10_BLEND LiteralToBlend(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ZERO:
						return D3D10_BLEND_ZERO;
					case EffectNodes::Literal::ONE:
						return D3D10_BLEND_ONE;
					case EffectNodes::Literal::SRCCOLOR:
						return D3D10_BLEND_SRC_COLOR;
					case EffectNodes::Literal::SRCALPHA:
						return D3D10_BLEND_SRC_ALPHA;
					case EffectNodes::Literal::INVSRCCOLOR:
						return D3D10_BLEND_INV_SRC_COLOR;
					case EffectNodes::Literal::INVSRCALPHA:
						return D3D10_BLEND_INV_SRC_ALPHA;
					case EffectNodes::Literal::DESTCOLOR:
						return D3D10_BLEND_DEST_COLOR;
					case EffectNodes::Literal::DESTALPHA:
						return D3D10_BLEND_DEST_ALPHA;
					case EffectNodes::Literal::INVDESTCOLOR:
						return D3D10_BLEND_INV_DEST_COLOR;
					case EffectNodes::Literal::INVDESTALPHA:
						return D3D10_BLEND_INV_DEST_ALPHA;
				}
			}
			static D3D10_BLEND_OP LiteralToBlendOp(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ADD:
						return D3D10_BLEND_OP_ADD;
					case EffectNodes::Literal::SUBTRACT:
						return D3D10_BLEND_OP_SUBTRACT;
					case EffectNodes::Literal::REVSUBTRACT:
						return D3D10_BLEND_OP_REV_SUBTRACT;
					case EffectNodes::Literal::MIN:
						return D3D10_BLEND_OP_MIN;
					case EffectNodes::Literal::MAX:
						return D3D10_BLEND_OP_MAX;
				}
			}
			static DXGI_FORMAT LiteralToFormat(int value, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case EffectNodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						return DXGI_FORMAT_R8_UNORM;
					case EffectNodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						return DXGI_FORMAT_R32_FLOAT;
					case EffectNodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						return DXGI_FORMAT_R8G8_UNORM;
					case EffectNodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						return DXGI_FORMAT_R8G8B8A8_TYPELESS;
					case EffectNodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						return DXGI_FORMAT_R16G16B16A16_UNORM;
					case EffectNodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						return DXGI_FORMAT_R16G16B16A16_FLOAT;
					case EffectNodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						return DXGI_FORMAT_R32G32B32A32_FLOAT;
					case EffectNodes::Literal::DXT1:
						format = Effect::Texture::Format::DXT1;
						return DXGI_FORMAT_BC1_TYPELESS;
					case EffectNodes::Literal::DXT3:
						format = Effect::Texture::Format::DXT3;
						return DXGI_FORMAT_BC2_TYPELESS;
					case EffectNodes::Literal::DXT5:
						format = Effect::Texture::Format::DXT5;
						return DXGI_FORMAT_BC3_TYPELESS;
					case EffectNodes::Literal::LATC1:
						format = Effect::Texture::Format::LATC1;
						return DXGI_FORMAT_BC4_UNORM;
					case EffectNodes::Literal::LATC2:
						format = Effect::Texture::Format::LATC2;
						return DXGI_FORMAT_BC5_UNORM;
					default:
						format = Effect::Texture::Format::Unknown;
						return DXGI_FORMAT_UNKNOWN;
				}
			}
			static DXGI_FORMAT TypelessToLinearFormat(DXGI_FORMAT format)
			{
				switch (format)
				{
					case DXGI_FORMAT_R8G8B8A8_TYPELESS:
						return DXGI_FORMAT_R8G8B8A8_UNORM;
					case DXGI_FORMAT_BC1_TYPELESS:
						return DXGI_FORMAT_BC1_UNORM;
					case DXGI_FORMAT_BC2_TYPELESS:
						return DXGI_FORMAT_BC2_UNORM;
					case DXGI_FORMAT_BC3_TYPELESS:
						return DXGI_FORMAT_BC3_UNORM;
					default:
						return format;
				}
			}
			static DXGI_FORMAT TypelessToSRGBFormat(DXGI_FORMAT format)
			{
				switch (format)
				{
					case DXGI_FORMAT_R8G8B8A8_TYPELESS:
						return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
					case DXGI_FORMAT_BC1_TYPELESS:
						return DXGI_FORMAT_BC1_UNORM_SRGB;
					case DXGI_FORMAT_BC2_TYPELESS:
						return DXGI_FORMAT_BC2_UNORM_SRGB;
					case DXGI_FORMAT_BC3_TYPELESS:
						return DXGI_FORMAT_BC3_UNORM_SRGB;
					default:
						return format;
				}
			}
			
			static inline std::string PrintLocation(const EffectTree::Location &location)
			{
				return std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): ";
			}

			std::string PrintType(const EffectNodes::Type &type)
			{
				std::string res;

				switch (type.Class)
				{
					case EffectNodes::Type::Void:
						res += "void";
						break;
					case EffectNodes::Type::Bool:
						res += "bool";
						break;
					case EffectNodes::Type::Int:
						res += "int";
						break;
					case EffectNodes::Type::Uint:
						res += "uint";
						break;
					case EffectNodes::Type::Float:
						res += "float";
						break;
					case EffectNodes::Type::Sampler:
						res += "__sampler2D";
						break;
					case EffectNodes::Type::Struct:
						assert(type.Definition != EffectTree::Null);
						res += this->mAST[type.Definition].As<EffectNodes::Struct>().Name;
						break;
				}

				if (type.IsMatrix())
				{
					res += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
				}
				else if (type.IsVector())
				{
					res += std::to_string(type.Rows);
				}

				return res;
			}
			std::string PrintTypeWithQualifiers(const EffectNodes::Type &type)
			{
				std::string qualifiers;

				if (type.HasQualifier(EffectNodes::Type::Qualifier::Extern))
					qualifiers += "extern ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Static))
					qualifiers += "static ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Const))
					qualifiers += "const ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Volatile))
					qualifiers += "volatile ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Precise))
					qualifiers += "precise ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::NoInterpolation))
					qualifiers += "nointerpolation ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::NoPerspective))
					qualifiers += "noperspective ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Linear))
					qualifiers += "linear ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Centroid))
					qualifiers += "centroid ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::Sample))
					qualifiers += "sample ";
				if (type.HasQualifier(EffectNodes::Type::Qualifier::InOut))
					qualifiers += "inout ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::In))
					qualifiers += "in ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::Out))
					qualifiers += "out ";
				else if (type.HasQualifier(EffectNodes::Type::Qualifier::Uniform))
					qualifiers += "uniform ";

				return qualifiers + PrintType(type);
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
				else if (node.Is<InitializerList>())
				{
					Visit(node.As<InitializerList>());
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
				this->mCurrentSource += this->mAST[node.Reference].As<EffectNodes::Variable>().Name;
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
							this->mCurrentSource += std::to_string(node.Value.Uint[i]);
							break;
						case EffectNodes::Type::Float:
							this->mCurrentSource += std::to_string(node.Value.Float[i]) + "f";
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

				switch (node.Operator)
				{
					case EffectNodes::Expression::Negate:
						part1 = '-';
						break;
					case EffectNodes::Expression::BitNot:
						part1 = '~';
						break;
					case EffectNodes::Expression::LogicNot:
						part1 = '!';
						break;
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
						part1 = "abs(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Sign:
						part1 = "sign(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Rcp:
						part1 = "(1.0f / ";
						part2 = ")";
						break;
					case EffectNodes::Expression::All:
						part1 = "all(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Any:
						part1 = "any(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Sin:
						part1 = "sin(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Sinh:
						part1 = "sinh(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Cos:
						part1 = "cos(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Cosh:
						part1 = "cosh(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Tan:
						part1 = "tan(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Tanh:
						part1 = "tanh(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Asin:
						part1 = "asin(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Acos:
						part1 = "acos(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Atan:
						part1 = "atan(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Exp:
						part1 = "exp(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Exp2:
						part1 = "exp2(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Log:
						part1 = "log(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Log2:
						part1 = "log2(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Log10:
						part1 = "log10(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Sqrt:
						part1 = "sqrt(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Rsqrt:
						part1 = "rsqrt(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Ceil:
						part1 = "ceil(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Floor:
						part1 = "floor(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Frac:
						part1 = "frac(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Trunc:
						part1 = "trunc(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Round:
						part1 = "round(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Saturate:
						part1 = "saturate(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Radians:
						part1 = "radians(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Degrees:
						part1 = "degrees(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Noise:
						part1 = "noise(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Length:
						part1 = "length(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Normalize:
						part1 = "normalize(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Transpose:
						part1 = "transpose(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Determinant:
						part1 = "determinant(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Cast:
						part1 = PrintType(node.Type) + '(';
						part2 = ')';
						break;
					case EffectNodes::Expression::BitCastInt2Float:
						part1 = "asfloat(";
						part2 = ")";
						break;
					case EffectNodes::Expression::BitCastUint2Float:
						part1 = "asfloat(";
						part2 = ")";
						break;
					case EffectNodes::Expression::BitCastFloat2Int:
						part1 = "asint(";
						part2 = ")";
						break;
					case EffectNodes::Expression::BitCastFloat2Uint:
						part1 = "asuint(";
						part2 = ")";
						break;
					case EffectNodes::Expression::Add:
						part1 = '(';
						part2 = " + ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Subtract:
						part1 = '(';
						part2 = " - ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Multiply:
						part1 = '(';
						part2 = " * ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Divide:
						part1 = '(';
						part2 = " / ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Modulo:
						part1 = '(';
						part2 = " % ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Less:
						part1 = '(';
						part2 = " < ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Greater:
						part1 = '(';
						part2 = " > ";
						part3 = ')';
						break;
					case EffectNodes::Expression::LessOrEqual:
						part1 = '(';
						part2 = " <= ";
						part3 = ')';
						break;
					case EffectNodes::Expression::GreaterOrEqual:
						part1 = '(';
						part2 = " >= ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Equal:
						part1 = '(';
						part2 = " == ";
						part3 = ')';
						break;
					case EffectNodes::Expression::NotEqual:
						part1 = '(';
						part2 = " != ";
						part3 = ')';
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
						part1 = '(';
						part2 = " & ";
						part3 = ')';
						break;
					case EffectNodes::Expression::BitXor:
						part1 = '(';
						part2 = " ^ ";
						part3 = ')';
						break;
					case EffectNodes::Expression::BitOr:
						part1 = '(';
						part2 = " | ";
						part3 = ')';
						break;
					case EffectNodes::Expression::LogicAnd:
						part1 = '(';
						part2 = " && ";
						part3 = ')';
						break;
					case EffectNodes::Expression::LogicXor:
						part1 = '(';
						part2 = " ^^ ";
						part3 = ')';
						break;
					case EffectNodes::Expression::LogicOr:
						part1 = '(';
						part2 = " || ";
						part3 = ')';
						break;
					case EffectNodes::Expression::Mul:
						part1 = "mul(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Atan2:
						part1 = "atan2(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Dot:
						part1 = "dot(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Cross:
						part1 = "cross(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Distance:
						part1 = "distance(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Pow:
						part1 = "pow(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Modf:
						part1 = "modf(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Frexp:
						part1 = "frexp(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Ldexp:
						part1 = "ldexp(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Min:
						part1 = "min(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Max:
						part1 = "max(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Step:
						part1 = "step(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Reflect:
						part1 = "reflect(";
						part2 = ", ";
						part3 = ")";
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
						part1 = "__tex2D(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexLevel:
						part1 = "__tex2Dlod(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexGather:
						part1 = "__tex2Dgather(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexBias:
						part1 = "__tex2Dbias(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexFetch:
						part1 = "__tex2Dfetch(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexSize:
						part1 = "__tex2Dsize(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::Mad:
						part1 = "((";
						part2 = ") * (";
						part3 = ") + (";
						part4 = "))";
						break;
					case EffectNodes::Expression::SinCos:
						part1 = "sincos(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::Lerp:
						part1 = "lerp(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::Clamp:
						part1 = "clamp(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::SmoothStep:
						part1 = "smoothstep(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::Refract:
						part1 = "refract(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::FaceForward:
						part1 = "faceforward(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::Conditional:
						part1 = '(';
						part2 = " ? ";
						part3 = " : ";
						part4 = ')';
						break;
					case EffectNodes::Expression::TexOffset:
						part1 = "__tex2Doffset(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::TexLevelOffset:
						part1 = "__tex2Dlodoffset(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
					case EffectNodes::Expression::TexGatherOffset:
						part1 = "__tex2Dgatheroffset(";
						part2 = ", ";
						part3 = ", ";
						part4 = ")";
						break;
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

				this->mCurrentSource += ' ';
				Visit(this->mAST[node.Right]);
				this->mCurrentSource += ')';
			}
			void Visit(const EffectNodes::Call &node)
			{
				this->mCurrentSource += node.CalleeName;
				this->mCurrentSource += '(';

				if (node.Arguments != EffectTree::Null)
				{
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
				}

				this->mCurrentSource += ')';
			}
			void Visit(const EffectNodes::Constructor &node)
			{
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
			void Visit(const EffectNodes::InitializerList &node)
			{
				this->mCurrentSource += "{ ";

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

				this->mCurrentSource += " }";
			}
			void Visit(const EffectNodes::If &node)
			{
				if (node.Attributes != nullptr)
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += node.Attributes;
					this->mCurrentSource += ']';
				}

				this->mCurrentSource += "if (";
				Visit(this->mAST[node.Condition]);
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
				if (node.Attributes != nullptr)
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += node.Attributes;
					this->mCurrentSource += ']';
				}

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
				if (node.Attributes != nullptr)
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += node.Attributes;
					this->mCurrentSource += ']';
				}

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
				if (node.Attributes != nullptr)
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += node.Attributes;
					this->mCurrentSource += ']';
				}

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
						this->mCurrentSource += ' ';
						Visit(this->mAST[node.Value]);
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
			void Visit(const EffectNodes::Annotation &node, std::unordered_map<std::string, Effect::Annotation> &annotations)
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

				annotations.insert(std::make_pair(node.Name, data));

				if (node.NextAnnotation != EffectTree::Null)
				{
					Visit(this->mAST[node.NextAnnotation].As<EffectNodes::Annotation>(), annotations);
				}
			}
			void Visit(const EffectNodes::Struct &node)
			{
				this->mCurrentSource += "struct ";

				if (node.Name != nullptr)
				{
					this->mCurrentSource += node.Name;
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
				
					this->mCurrentSource += node.Name;
				}

				if (node.Type.IsArray())
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += (node.Type.ArrayLength >= 1) ? std::to_string(node.Type.ArrayLength) : "";
					this->mCurrentSource += ']';
				}

				if (node.Semantic != nullptr)
				{
					this->mCurrentSource += " : ";
					this->mCurrentSource += node.Semantic;
				}

				if (node.Initializer != EffectTree::Null)
				{
					this->mCurrentSource += " = ";

					Visit(this->mAST[node.Initializer]);
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
				D3D10_TEXTURE2D_DESC desc;
				desc.ArraySize = 1;
				desc.Width = (node.Properties[EffectNodes::Variable::Width] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Width]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				desc.Height = (node.Properties[EffectNodes::Variable::Height] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Height]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				desc.MipLevels = (node.Properties[EffectNodes::Variable::MipLevels] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MipLevels]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
				desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
				desc.Usage = D3D10_USAGE_DEFAULT;
				desc.CPUAccessFlags = desc.MiscFlags = 0;
				desc.SampleDesc.Count = 1;
				desc.SampleDesc.Quality = 0;
				
				Effect::Texture::Format format = Effect::Texture::Format::RGBA8;

				if (node.Properties[EffectNodes::Variable::Format] != 0)
				{
					desc.Format = LiteralToFormat(this->mAST[node.Properties[EffectNodes::Variable::Format]].As<EffectNodes::Literal>().Value.Uint[0], format);
				}

				std::unique_ptr<D3D10Texture> obj(new D3D10Texture(this->mEffect));
				obj->mDesc.Width = desc.Width;
				obj->mDesc.Height = desc.Height;
				obj->mDesc.Levels = desc.MipLevels;
				obj->mDesc.Format = format;
				obj->mRegister = this->mEffect->mShaderResources.size();
				obj->mTexture = nullptr;
				obj->mShaderResourceView[0] = nullptr;
				obj->mShaderResourceView[1] = nullptr;

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateTexture2D(&desc, nullptr, &obj->mTexture);

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateTexture2D' failed!\n";
					this->mFatal = true;
					return;
				}

				this->mCurrentSource += "Texture2D ";
				this->mCurrentSource += node.Name;
				this->mCurrentSource += " : register(t" + std::to_string(this->mEffect->mShaderResources.size()) + ")";

				D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
				srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MostDetailedMip = 0;
				srvdesc.Texture2D.MipLevels = desc.MipLevels;
				srvdesc.Format = TypelessToLinearFormat(desc.Format);

				hr = this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(obj->mTexture, &srvdesc, &obj->mShaderResourceView[0]);

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateShaderResourceView' failed!\n";
					this->mFatal = true;
					return;
				}

				srvdesc.Format = TypelessToSRGBFormat(desc.Format);

				if (srvdesc.Format != desc.Format)
				{
					this->mCurrentSource += ", __";
					this->mCurrentSource += node.Name;
					this->mCurrentSource += "SRGB : register(t" + std::to_string(this->mEffect->mShaderResources.size() + 1) + ")";

					hr = this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(obj->mTexture, &srvdesc, &obj->mShaderResourceView[1]);

					if (FAILED(hr))
					{
						this->mErrors += PrintLocation(node.Location) + "'CreateShaderResourceView' failed!\n";
						this->mFatal = true;
						return;
					}
				}

				this->mCurrentSource += ";\n";

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), obj->mAnnotations);
				}

				if (obj->mShaderResourceView[0] != nullptr)
				{
					this->mEffect->mShaderResources.push_back(obj->mShaderResourceView[0]);
				}
				if (obj->mShaderResourceView[1] != nullptr)
				{
					this->mEffect->mShaderResources.push_back(obj->mShaderResourceView[1]);
				}

				this->mEffect->mTextures.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void VisitSampler(const EffectNodes::Variable &node)
			{
				if (node.Properties[EffectNodes::Variable::Texture] == 0)
				{
					this->mErrors = PrintLocation(node.Location) + "Sampler '" + std::string(node.Name) + "' is missing required 'Texture' property.\n";
					this->mFatal;
					return;
				}

				D3D10_SAMPLER_DESC desc;
				desc.AddressU = (node.Properties[EffectNodes::Variable::AddressU] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressU]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.AddressV = (node.Properties[EffectNodes::Variable::AddressV] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressV]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.AddressW = (node.Properties[EffectNodes::Variable::AddressW] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressW]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.MipLODBias = (node.Properties[EffectNodes::Variable::MipLODBias] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MipLODBias]].As<EffectNodes::Literal>().Value.Float[0] : 0.0f;
				desc.MinLOD = (node.Properties[EffectNodes::Variable::MinLOD] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MinLOD]].As<EffectNodes::Literal>().Value.Float[0] : -FLT_MAX;
				desc.MaxLOD = (node.Properties[EffectNodes::Variable::MaxLOD] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MaxLOD]].As<EffectNodes::Literal>().Value.Float[0] : FLT_MAX;
				desc.MaxAnisotropy = (node.Properties[EffectNodes::Variable::MaxAnisotropy] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MaxAnisotropy]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				desc.ComparisonFunc = D3D10_COMPARISON_NEVER;

				UINT minfilter = EffectNodes::Literal::LINEAR, magfilter = EffectNodes::Literal::LINEAR, mipfilter = EffectNodes::Literal::LINEAR;

				if (node.Properties[EffectNodes::Variable::MinFilter] != 0)
				{
					minfilter = this->mAST[node.Properties[EffectNodes::Variable::MinFilter]].As<EffectNodes::Literal>().Value.Uint[0];
				}
				if (node.Properties[EffectNodes::Variable::MagFilter] != 0)
				{
					magfilter = this->mAST[node.Properties[EffectNodes::Variable::MagFilter]].As<EffectNodes::Literal>().Value.Uint[0];
				}
				if (node.Properties[EffectNodes::Variable::MipFilter] != 0)
				{
					mipfilter = this->mAST[node.Properties[EffectNodes::Variable::MipFilter]].As<EffectNodes::Literal>().Value.Uint[0];
				}

				if (minfilter == EffectNodes::Literal::ANISOTROPIC || magfilter == EffectNodes::Literal::ANISOTROPIC || mipfilter == EffectNodes::Literal::ANISOTROPIC)
				{
					desc.Filter = D3D10_FILTER_ANISOTROPIC;
				}
				else if (minfilter == EffectNodes::Literal::POINT && magfilter == EffectNodes::Literal::POINT && mipfilter == EffectNodes::Literal::LINEAR)
				{
					desc.Filter = D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				}
				else if (minfilter == EffectNodes::Literal::POINT && magfilter == EffectNodes::Literal::LINEAR && mipfilter == EffectNodes::Literal::POINT)
				{
					desc.Filter = D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
				}
				else if (minfilter == EffectNodes::Literal::POINT && magfilter == EffectNodes::Literal::LINEAR && mipfilter == EffectNodes::Literal::LINEAR)
				{
					desc.Filter = D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				}
				else if (minfilter == EffectNodes::Literal::LINEAR && magfilter == EffectNodes::Literal::POINT && mipfilter == EffectNodes::Literal::POINT)
				{
					desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				}
				else if (minfilter == EffectNodes::Literal::LINEAR && magfilter == EffectNodes::Literal::POINT && mipfilter == EffectNodes::Literal::LINEAR)
				{
					desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				}
				else if (minfilter == EffectNodes::Literal::LINEAR && magfilter == EffectNodes::Literal::LINEAR && mipfilter == EffectNodes::Literal::POINT)
				{
					desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				}
				else if (minfilter == EffectNodes::Literal::LINEAR && magfilter == EffectNodes::Literal::LINEAR && mipfilter == EffectNodes::Literal::LINEAR)
				{
					desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
				}
				else
				{
					desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
				}

				bool srgb = false;

				if (node.Properties[EffectNodes::Variable::SRGBTexture] != 0)
				{
					srgb = this->mAST[node.Properties[EffectNodes::Variable::SRGBTexture]].As<EffectNodes::Literal>().Value.Bool[0] != 0;
				}

				const char *texture = this->mAST[node.Properties[EffectNodes::Variable::Texture]].As<EffectNodes::Variable>().Name;

				auto it = this->mEffect->mSamplerDescs.find(desc);

				if (it == this->mEffect->mSamplerDescs.end())
				{
					ID3D10SamplerState *sampler = nullptr;

					HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateSamplerState(&desc, &sampler);

					if (FAILED(hr))
					{
						this->mErrors += PrintLocation(node.Location) + "'CreateSamplerState' failed!\n";
						this->mFatal = true;
						return;
					}

					this->mEffect->mSamplerStates.push_back(sampler);
					it = this->mEffect->mSamplerDescs.insert(std::make_pair(desc, this->mEffect->mSamplerStates.size() - 1)).first;

					this->mCurrentSource += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
				}

				this->mCurrentSource += "static const __sampler2D ";
				this->mCurrentSource += node.Name;
				this->mCurrentSource += " = { ";

				if (srgb)
				{
					this->mCurrentSource += "__";
					this->mCurrentSource += texture;
					this->mCurrentSource += "SRGB";
				}
				else
				{
					this->mCurrentSource += texture;
				}

				this->mCurrentSource += ", __SamplerState" + std::to_string(it->second) + " };\n";
			}
			void VisitUniform(const EffectNodes::Variable &node)
			{
				this->mCurrentGlobalConstants += PrintTypeWithQualifiers(node.Type);
				this->mCurrentGlobalConstants += ' ';
				this->mCurrentGlobalConstants += node.Name;

				if (node.Type.IsArray())
				{
					this->mCurrentGlobalConstants += '[';
					this->mCurrentGlobalConstants += (node.Type.ArrayLength >= 1) ? std::to_string(node.Type.ArrayLength) : "";
					this->mCurrentGlobalConstants += ']';
				}

				this->mCurrentGlobalConstants += ";\n";

				std::unique_ptr<D3D10Constant> obj(new D3D10Constant(this->mEffect));
				obj->mDesc.Rows = node.Type.Rows;
				obj->mDesc.Columns = node.Type.Cols;
				obj->mDesc.Elements = node.Type.ArrayLength;
				obj->mDesc.Fields = 0;
				obj->mDesc.Size = node.Type.Rows * node.Type.Cols;

				switch (node.Type.Class)
				{
					case EffectNodes::Type::Bool:
						obj->mDesc.Size *= sizeof(int);
						obj->mDesc.Type = Effect::Constant::Type::Bool;
						break;
					case EffectNodes::Type::Int:
						obj->mDesc.Size *= sizeof(int);
						obj->mDesc.Type = Effect::Constant::Type::Int;
						break;
					case EffectNodes::Type::Uint:
						obj->mDesc.Size *= sizeof(unsigned int);
						obj->mDesc.Type = Effect::Constant::Type::Uint;
						break;
					case EffectNodes::Type::Float:
						obj->mDesc.Size *= sizeof(float);
						obj->mDesc.Type = Effect::Constant::Type::Float;
						break;
				}

				const UINT alignment = 16 - (this->mCurrentGlobalSize % 16);
				this->mCurrentGlobalSize += (obj->mDesc.Size > alignment && (alignment != 16 || obj->mDesc.Size <= 16)) ? obj->mDesc.Size + alignment : obj->mDesc.Size;

				obj->mBuffer = 0;
				obj->mBufferOffset = this->mCurrentGlobalSize - obj->mDesc.Size;

				if (this->mCurrentGlobalSize >= this->mCurrentGlobalStorageSize)
				{
					this->mEffect->mConstantStorages[0] = static_cast<unsigned char *>(::realloc(this->mEffect->mConstantStorages[0], this->mCurrentGlobalStorageSize += 128));
				}

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), obj->mAnnotations);
				}

				if (node.Initializer != EffectTree::Null && this->mAST[node.Initializer].Is<EffectNodes::Literal>())
				{
					::memcpy(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, &this->mAST[node.Initializer].As<EffectNodes::Literal>().Value, obj->mDesc.Size);
				}
				else
				{
					::memset(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, 0, obj->mDesc.Size);
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

				this->mCurrentSource += "cbuffer ";
				this->mCurrentSource += node.Name;
				this->mCurrentSource += " : register(b" + std::to_string(this->mEffect->mConstantBuffers.size()) + ")";
				this->mCurrentSource += "\n{\n";

				this->mCurrentBlockName = node.Name;

				ID3D10Buffer *buffer = nullptr;
				unsigned char *storage = nullptr;
				UINT totalsize = 0, currentsize = 0;

				unsigned int fieldCount = 0;
				const EffectNodes::Variable *field = &this->mAST[structure.Fields].As<EffectNodes::Variable>();

				do
				{
					fieldCount++;

					Visit(*field);

					std::unique_ptr<D3D10Constant> obj(new D3D10Constant(this->mEffect));
					obj->mDesc.Rows = field->Type.Rows;
					obj->mDesc.Columns = field->Type.Cols;
					obj->mDesc.Elements = field->Type.ArrayLength;
					obj->mDesc.Fields = 0;
					obj->mDesc.Size = field->Type.Rows * field->Type.Cols;

					switch (field->Type.Class)
					{
						case EffectNodes::Type::Bool:
							obj->mDesc.Size *= sizeof(int);
							obj->mDesc.Type = Effect::Constant::Type::Bool;
							break;
						case EffectNodes::Type::Int:
							obj->mDesc.Size *= sizeof(int);
							obj->mDesc.Type = Effect::Constant::Type::Int;
							break;
						case EffectNodes::Type::Uint:
							obj->mDesc.Size *= sizeof(unsigned int);
							obj->mDesc.Type = Effect::Constant::Type::Uint;
							break;
						case EffectNodes::Type::Float:
							obj->mDesc.Size *= sizeof(float);
							obj->mDesc.Type = Effect::Constant::Type::Float;
							break;
					}

					const UINT alignment = 16 - (totalsize % 16);
					totalsize += (obj->mDesc.Size > alignment && (alignment != 16 || obj->mDesc.Size <= 16)) ? obj->mDesc.Size + alignment : obj->mDesc.Size;

					obj->mBuffer = this->mEffect->mConstantBuffers.size();
					obj->mBufferOffset = totalsize - obj->mDesc.Size;

					if (totalsize >= currentsize)
					{
						storage = static_cast<unsigned char *>(::realloc(storage, currentsize += 128));
					}

					if (field->Initializer != EffectTree::Null && this->mAST[field->Initializer].Is<EffectNodes::Literal>())
					{
						::memcpy(storage + obj->mBufferOffset, &this->mAST[field->Initializer].As<EffectNodes::Literal>().Value, obj->mDesc.Size);
					}
					else
					{
						::memset(storage + obj->mBufferOffset, 0, obj->mDesc.Size);
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

				std::unique_ptr<D3D10Constant> obj(new D3D10Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fieldCount;
				obj->mDesc.Size = totalsize;
				obj->mDesc.Type = Effect::Constant::Type::Struct;
				obj->mBuffer = this->mEffect->mConstantBuffers.size();
				obj->mBufferOffset = 0;

				if (node.Annotations != EffectTree::Null)
				{
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), obj->mAnnotations);
				}

				this->mEffect->mConstants.insert(std::make_pair(node.Name, std::move(obj)));

				D3D10_BUFFER_DESC desc;
				desc.ByteWidth = RoundToMultipleOf16(totalsize);
				desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
				desc.Usage = D3D10_USAGE_DYNAMIC;
				desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
				desc.MiscFlags = 0;

				D3D10_SUBRESOURCE_DATA initial;
				initial.pSysMem = storage;
				initial.SysMemPitch = initial.SysMemSlicePitch = totalsize;

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateBuffer(&desc, &initial, &buffer);

				if (SUCCEEDED(hr))
				{
					this->mEffect->mConstantBuffers.push_back(buffer);
					this->mEffect->mConstantStorages.push_back(storage);
				}
			}
			void Visit(const EffectNodes::Function &node)
			{
				this->mCurrentSource += PrintType(node.ReturnType);
				this->mCurrentSource += ' ';
				this->mCurrentSource += node.Name;
				this->mCurrentSource += '(';

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
								
				if (node.ReturnSemantic != nullptr)
				{
					this->mCurrentSource += " : ";
					this->mCurrentSource += node.ReturnSemantic;
				}

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
			}
			void Visit(const EffectNodes::Technique &node)
			{
				std::unique_ptr<D3D10Technique> obj(new D3D10Technique(this->mEffect));

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
					Visit(this->mAST[node.Annotations].As<EffectNodes::Annotation>(), obj->mAnnotations);
				}

				this->mEffect->mTechniques.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void Visit(const EffectNodes::Pass &node, std::vector<D3D10Technique::Pass> &passes)
			{
				D3D10Technique::Pass pass;
				pass.VS = nullptr;
				pass.PS = nullptr;
				pass.BS = nullptr;
				pass.DSS = nullptr;
				pass.StencilRef = 0;
				ZeroMemory(pass.RT, sizeof(pass.RT));
				pass.SR = this->mEffect->mShaderResources;

				if (node.States[EffectNodes::Pass::VertexShader] != 0)
				{
					VisitShader(this->mAST[node.States[EffectNodes::Pass::VertexShader]].As<EffectNodes::Function>(), EffectNodes::Pass::VertexShader, pass);
				}
				if (node.States[EffectNodes::Pass::PixelShader] != 0)
				{
					VisitShader(this->mAST[node.States[EffectNodes::Pass::PixelShader]].As<EffectNodes::Function>(), EffectNodes::Pass::PixelShader, pass);
				}

				int srgb = 0;

				if (node.States[EffectNodes::Pass::SRGBWriteEnable] != 0)
				{
					srgb = this->mAST[node.States[EffectNodes::Pass::SRGBWriteEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				}

				pass.RT[0] = this->mEffect->mEffectContext->mBackBufferTargets[srgb];

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

						D3D10Texture *texture = it->second.get();

						D3D10_TEXTURE2D_DESC desc;
						texture->mTexture->GetDesc(&desc);

						D3D10_RENDER_TARGET_VIEW_DESC rtvdesc;
						rtvdesc.Format = srgb ? TypelessToSRGBFormat(desc.Format) : TypelessToLinearFormat(desc.Format);
						rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;
						rtvdesc.Texture2D.MipSlice = 0;

						if (texture->mRenderTargetView[srgb] == nullptr)
						{
							HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateRenderTargetView(texture->mTexture, &rtvdesc, &texture->mRenderTargetView[srgb]);

							if (FAILED(hr))
							{
								this->mErrors += PrintLocation(node.Location) + "'CreateRenderTargetView' failed!\n";
							}
						}

						pass.RT[i] = texture->mRenderTargetView[srgb];
					}
				}

				D3D10_DEPTH_STENCIL_DESC ddesc;
				ddesc.DepthEnable = node.States[EffectNodes::Pass::DepthEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::DepthEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				ddesc.DepthWriteMask = (node.States[EffectNodes::Pass::DepthWriteMask] == 0 || this->mAST[node.States[EffectNodes::Pass::DepthWriteMask]].As<EffectNodes::Literal>().Value.Bool[0]) ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
				ddesc.DepthFunc = (node.States[EffectNodes::Pass::DepthFunc] != 0) ? LiteralToComparisonFunc(this->mAST[node.States[EffectNodes::Pass::DepthFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_COMPARISON_LESS;
				ddesc.StencilEnable = node.States[EffectNodes::Pass::StencilEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::StencilEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				ddesc.StencilReadMask = (node.States[EffectNodes::Pass::StencilReadMask] != 0) ? static_cast<UINT8>(this->mAST[node.States[EffectNodes::Pass::StencilReadMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF) : D3D10_DEFAULT_STENCIL_READ_MASK;
				ddesc.StencilWriteMask = (node.States[EffectNodes::Pass::StencilWriteMask] != 0) != 0 ? static_cast<UINT8>(this->mAST[node.States[EffectNodes::Pass::StencilWriteMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF) : D3D10_DEFAULT_STENCIL_WRITE_MASK;
				ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = (node.States[EffectNodes::Pass::StencilFunc] != 0) ? LiteralToComparisonFunc(this->mAST[node.States[EffectNodes::Pass::StencilFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_COMPARISON_ALWAYS;
				ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = (node.States[EffectNodes::Pass::StencilPass] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilPass]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_STENCIL_OP_KEEP;
				ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = (node.States[EffectNodes::Pass::StencilFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilFail]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_STENCIL_OP_KEEP;
				ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = (node.States[EffectNodes::Pass::StencilDepthFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilDepthFail]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_STENCIL_OP_KEEP;

				if (node.States[EffectNodes::Pass::StencilRef] != 0)
				{
					pass.StencilRef = this->mAST[node.States[EffectNodes::Pass::StencilRef]].As<EffectNodes::Literal>().Value.Uint[0];
				}

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateDepthStencilState(&ddesc, &pass.DSS);

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateDepthStencilState' failed!\n";
				}

				D3D10_BLEND_DESC bdesc;
				bdesc.AlphaToCoverageEnable = FALSE;
				bdesc.RenderTargetWriteMask[0] = (node.States[EffectNodes::Pass::ColorWriteMask] != 0) ? static_cast<UINT8>(this->mAST[node.States[EffectNodes::Pass::ColorWriteMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xF) : D3D10_COLOR_WRITE_ENABLE_ALL;
				bdesc.BlendEnable[0] = node.States[EffectNodes::Pass::BlendEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::BlendEnable]].As<EffectNodes::Literal>().Value.Bool[0];
				bdesc.BlendOp = (node.States[EffectNodes::Pass::BlendOp] != 0) ? LiteralToBlendOp(this->mAST[node.States[EffectNodes::Pass::BlendOp]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_BLEND_OP_ADD;
				bdesc.BlendOpAlpha = (node.States[EffectNodes::Pass::BlendOpAlpha] != 0) ? LiteralToBlendOp(this->mAST[node.States[EffectNodes::Pass::BlendOpAlpha]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_BLEND_OP_ADD;
				bdesc.SrcBlend = (node.States[EffectNodes::Pass::SrcBlend] != 0) ? LiteralToBlend(this->mAST[node.States[EffectNodes::Pass::SrcBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_BLEND_ONE;
				bdesc.DestBlend = (node.States[EffectNodes::Pass::DestBlend] != 0) ? LiteralToBlend(this->mAST[node.States[EffectNodes::Pass::DestBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : D3D10_BLEND_ZERO;

				for (UINT i = 1; i < 8; ++i)
				{
					bdesc.RenderTargetWriteMask[i] = bdesc.RenderTargetWriteMask[0];
					bdesc.BlendEnable[i] = bdesc.BlendEnable[0];
				}

				hr = this->mEffect->mEffectContext->mDevice->CreateBlendState(&bdesc, &pass.BS);

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateBlendState' failed!\n";
				}

				for (auto it = pass.SR.begin(), end = pass.SR.end(); it != end; ++it)
				{
					ID3D10Resource *res1, *res2;
					(*it)->GetResource(&res1);

					for (size_t i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					{
						if (pass.RT[i] == nullptr)
						{
							continue;
						}

						pass.RT[i]->GetResource(&res2);
						res2->Release();

						if (res1 == res2)
						{
							*it = nullptr;
							break;
						}
					}

					res1->Release();
				}

				passes.push_back(std::move(pass));
			}
			void VisitShader(const EffectNodes::Function &node, unsigned int type, D3D10Technique::Pass &pass)
			{
				const char *profile = nullptr;

				switch (type)
				{
					default:
						return;
					case EffectNodes::Pass::VertexShader:
						profile = "vs_4_0";
						break;
					case EffectNodes::Pass::PixelShader:
						profile = "ps_4_0";
						break;
				}

				UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
				flags |= D3DCOMPILE_DEBUG;
#endif

				std::string source =
					"struct __sampler2D { Texture2D t; SamplerState s; };\n"
					"inline float4 __tex2D(__sampler2D s, float2 c) { return s.t.Sample(s.s, c); }\n"
					"inline float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return s.t.Sample(s.s, c, offset); }\n"
					"inline float4 __tex2Dlod(__sampler2D s, float4 c) { return s.t.SampleLevel(s.s, c.xy, c.w); }\n"
					"inline float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return s.t.SampleLevel(s.s, c.xy, c.w, offset); }\n"
					"inline float4 __tex2Dgather(__sampler2D s, float2 c) { return s.t.Gather(s.s, c); }\n"
					"inline float4 __tex2Dgatheroffset(__sampler2D s, float2 c, int2 offset) { return s.t.Gather(s.s, c, offset); }\n"
					"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
					"inline float4 __tex2Dbias(__sampler2D s, float4 c) { return s.t.SampleBias(s.s, c.xy, c.w); }\n"
					"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n";

				if (!this->mCurrentGlobalConstants.empty())
				{
					source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + this->mCurrentGlobalConstants + "};\n";
				}

				source += this->mCurrentSource;

				LOG(TRACE) << "> Compiling shader '" << node.Name << "':\n\n" << source.c_str() << "\n";

				ID3DBlob *compiled = nullptr, *errors = nullptr;

				HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, node.Name, profile, flags, 0, &compiled, &errors);

				if (errors != nullptr)
				{
					this->mErrors += std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

					errors->Release();
				}

				if (FAILED(hr))
				{
					this->mFatal = true;
					return;
				}

				switch (type)
				{
					case EffectNodes::Pass::VertexShader:
						hr = this->mEffect->mEffectContext->mDevice->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.VS);
						break;
					case EffectNodes::Pass::PixelShader:
						hr = this->mEffect->mEffectContext->mDevice->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.PS);
						break;
				}

				compiled->Release();

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateShader' failed!\n";
					this->mFatal = true;
					return;
				}
			}

		private:
			const EffectTree &mAST;
			D3D10Effect *mEffect;
			std::string mCurrentSource;
			std::string mErrors;
			bool mFatal;
			std::string mCurrentGlobalConstants;
			UINT mCurrentGlobalSize, mCurrentGlobalStorageSize;
			std::string mCurrentBlockName;
			bool mCurrentInParameterBlock, mCurrentInFunctionBlock, mCurrentInDeclaratorList;
		};
	}

	// -----------------------------------------------------------------------------------------------------

	D3D10Runtime::D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain) : mDevice(device), mSwapChain(swapchain), mStateBlock(nullptr), mBackBuffer(nullptr), mBackBufferTexture(nullptr), mBackBufferTargets(), mBestDepthStencil(nullptr), mBestDepthStencilReplacement(nullptr), mDepthStencilShaderResourceView(nullptr), mDrawCallCounter(1), mLost(true)
	{
		InitializeCriticalSection(&this->mCS);

		this->mDevice->AddRef();
		this->mSwapChain->AddRef();

		HRESULT hr;
		IDXGIDevice *dxgidevice = nullptr;
		IDXGIAdapter *adapter = nullptr;

		hr = this->mDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgidevice));

		assert(SUCCEEDED(hr));

		hr = dxgidevice->GetAdapter(&adapter);
		dxgidevice->Release();

		assert(SUCCEEDED(hr));

		DXGI_ADAPTER_DESC desc;
		hr = adapter->GetDesc(&desc);
		adapter->Release();

		assert(SUCCEEDED(hr));
			
		this->mVendorId = desc.VendorId;
		this->mDeviceId = desc.DeviceId;
		this->mRendererId = 0xD3D10;

		D3D10_STATE_BLOCK_MASK mask;
		D3D10StateBlockMaskEnableAll(&mask);
		hr = D3D10CreateStateBlock(this->mDevice, &mask, &this->mStateBlock);

		assert(SUCCEEDED(hr));
	}
	D3D10Runtime::~D3D10Runtime()
	{
		DeleteCriticalSection(&this->mCS);

		assert(this->mLost);

		if (this->mStateBlock != nullptr)
		{
			this->mStateBlock->Release();
		}

		this->mDevice->Release();
		this->mSwapChain->Release();
	}

	bool D3D10Runtime::OnCreate(unsigned int width, unsigned int height)
	{
		HRESULT hr = this->mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&this->mBackBuffer));

		assert(SUCCEEDED(hr));

		D3D10_TEXTURE2D_DESC bbdesc;
		this->mBackBuffer->GetDesc(&bbdesc);

		bbdesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		bbdesc.BindFlags = D3D10_BIND_RENDER_TARGET;
		
		hr = this->mDevice->CreateTexture2D(&bbdesc, nullptr, &this->mBackBufferTexture);

		assert(SUCCEEDED(hr));

		D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
		rtdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rtdesc.ViewDimension = bbdesc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;
		rtdesc.Texture2D.MipSlice = 0;
		
		hr = this->mDevice->CreateRenderTargetView(this->mBackBufferTexture, &rtdesc, &this->mBackBufferTargets[0]);

		assert(SUCCEEDED(hr));

		rtdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		
		hr = this->mDevice->CreateRenderTargetView(this->mBackBufferTexture, &rtdesc, &this->mBackBufferTargets[1]);

		assert(SUCCEEDED(hr));

		this->mNVG = nvgCreateD3D10(this->mDevice, 0);

		this->mLost = false;

		return Runtime::OnCreate(width, height);
	}
	void D3D10Runtime::OnDelete()
	{
		Runtime::OnDelete();

		nvgDeleteD3D10(this->mNVG);

		if (this->mStateBlock != nullptr)
		{
			this->mStateBlock->Apply();
			this->mStateBlock->ReleaseAllDeviceObjects();
		}

		if (this->mBackBufferTargets[0] != nullptr)
		{
			this->mBackBufferTargets[0]->Release();
		}
		if (this->mBackBufferTargets[1] != nullptr)
		{
			this->mBackBufferTargets[1]->Release();
		}
		if (this->mBackBufferTexture != nullptr)
		{
			this->mBackBufferTexture->Release();
		}
		if (this->mBackBuffer != nullptr)
		{
			this->mBackBuffer->Release();
		}

		this->mDepthStencilTable.clear();

		if (this->mBestDepthStencilReplacement != nullptr)
		{
			this->mBestDepthStencilReplacement->Release();
		}
		if (this->mDepthStencilShaderResourceView != nullptr)
		{
			this->mDepthStencilShaderResourceView->Release();
		}

		this->mNVG = nullptr;
		this->mBackBuffer = nullptr;
		this->mBackBufferTexture = nullptr;
		this->mBackBufferTargets[0] = nullptr;
		this->mBackBufferTargets[1] = nullptr;
		this->mBestDepthStencil = nullptr;
		this->mBestDepthStencilReplacement = nullptr;
		this->mDepthStencilShaderResourceView = nullptr;

		this->mLost = true;
	}
	void D3D10Runtime::OnDraw(unsigned int vertices)
	{
		CSLock lock(this->mCS);

		ID3D10DepthStencilView *depthstencil = nullptr;
		this->mDevice->OMGetRenderTargets(0, nullptr, &depthstencil);

		if (depthstencil != nullptr)
		{
			depthstencil->Release();

			if (depthstencil == this->mBestDepthStencilReplacement)
			{
				depthstencil = this->mBestDepthStencil;
			}

			this->mDepthStencilTable[depthstencil].DrawCallCount = static_cast<FLOAT>(this->mDrawCallCounter++);
			this->mDepthStencilTable[depthstencil].DrawVerticesCount += vertices;
		}

		Runtime::OnDraw(vertices);
	}
	void D3D10Runtime::OnPresent()
	{
		DetectBestDepthStencil();

		this->mDrawCallCounter = 1;

		ID3D10RenderTargetView *stateBlockTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
		ID3D10DepthStencilView *stateBlockDepthStencil = nullptr;

		this->mStateBlock->Capture();
		this->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateBlockTargets, &stateBlockDepthStencil);

		this->mDevice->CopyResource(this->mBackBufferTexture, this->mBackBuffer);
		this->mDevice->OMSetRenderTargets(1, &this->mBackBufferTargets[0], nullptr);

		Runtime::OnPresent();

		if (this->mLost)
		{
			return;
		}

		this->mDevice->CopyResource(this->mBackBuffer, this->mBackBufferTexture);

		this->mStateBlock->Apply();
		this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateBlockTargets, stateBlockDepthStencil);

		for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			if (stateBlockTargets[i] != nullptr)
			{
				stateBlockTargets[i]->Release();
			}
		}
		if (stateBlockDepthStencil != nullptr)
		{
			stateBlockDepthStencil->Release();
		}
	}

	std::unique_ptr<Effect> D3D10Runtime::CreateEffect(const EffectTree &ast, std::string &errors) const
	{
		D3D10Effect *effect = new D3D10Effect(shared_from_this());
			
		D3D10EffectCompiler visitor(ast);
		
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
	void D3D10Runtime::CreateScreenshot(unsigned char *buffer, std::size_t size) const
	{
		D3D10_TEXTURE2D_DESC desc;
		this->mBackBufferTexture->GetDesc(&desc);

		ID3D10Texture2D *textureStaging = nullptr;

		D3D10_TEXTURE2D_DESC textureDesc;
		ZeroMemory(&textureDesc, sizeof(D3D10_TEXTURE2D_DESC));
		textureDesc.ArraySize = 1;
		textureDesc.Width = desc.Width;
		textureDesc.Height = desc.Height;
		textureDesc.Format = D3D10EffectCompiler::TypelessToLinearFormat(desc.Format);
		textureDesc.Usage = D3D10_USAGE_STAGING;
		textureDesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
		textureDesc.MipLevels = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;

		HRESULT hr = this->mDevice->CreateTexture2D(&textureDesc, nullptr, &textureStaging);

		if (FAILED(hr))
		{
			return;
		}

		if (desc.SampleDesc.Count > 1)
		{
			textureDesc.CPUAccessFlags = 0;
			textureDesc.Usage = D3D10_USAGE_DEFAULT;

			ID3D10Texture2D *textureResolve = nullptr;

			hr = this->mDevice->CreateTexture2D(&textureDesc, nullptr, &textureResolve);

			if (SUCCEEDED(hr))
			{
				this->mDevice->ResolveSubresource(textureResolve, 0, this->mBackBufferTexture, 0, textureDesc.Format);
				this->mDevice->CopyResource(textureStaging, textureResolve);

				textureResolve->Release();
			}
		}
		else
		{
			this->mDevice->CopyResource(textureStaging, this->mBackBufferTexture);
		}

		if (FAILED(hr))
		{
			textureStaging->Release();
			return;
		}
				
		D3D10_MAPPED_TEXTURE2D mapped;
		ZeroMemory(&mapped, sizeof(D3D10_MAPPED_TEXTURE2D));

		hr = textureStaging->Map(0, D3D10_MAP_READ, 0, &mapped);

		if (FAILED(hr))
		{
			textureStaging->Release();
			return;
		}

		const UINT pitch = desc.Width * 4;

		assert(size >= pitch * desc.Height);

		BYTE *pBuffer = buffer;
		BYTE *pMapped = static_cast<BYTE *>(mapped.pData);

		for (UINT y = 0; y < desc.Height; ++y)
		{
			CopyMemory(pBuffer, pMapped, std::min(pitch, static_cast<UINT>(mapped.RowPitch)));
			
			for (UINT x = 0; x < pitch; x += 4)
			{
				pBuffer[x + 3] = 0xFF;

				if (desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
				{
					std::swap(pBuffer[x + 0], pBuffer[x + 2]);
				}
			}
								
			pBuffer += pitch;
			pMapped += mapped.RowPitch;
		}

		textureStaging->Unmap(0);

		textureStaging->Release();
	}

	void D3D10Runtime::DetectBestDepthStencil()
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

		CSLock lock(this->mCS);

		if (this->mDepthStencilTable.empty())
		{
			return;
		}

		DXGI_SWAP_CHAIN_DESC desc;
		this->mSwapChain->GetDesc(&desc);

		ID3D10DepthStencilView *best = nullptr;
		D3D10DepthStencilInfo bestInfo = { 0 };

		for (auto &it : this->mDepthStencilTable)
		{
			if (it.second.DrawCallCount == 0)
			{
				continue;
			}

			if (((it.second.DrawVerticesCount * (1.2f - it.second.DrawCallCount / this->mDrawCallCounter)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / this->mDrawCallCounter))) && (it.second.Width ==  desc.BufferDesc.Width && it.second.Height ==  desc.BufferDesc.Height))
			{
				best = it.first;
				bestInfo = it.second;
			}

			it.second.DrawCallCount = it.second.DrawVerticesCount = 0;
		}

		if (best != nullptr && this->mBestDepthStencil != best)
		{
			this->mBestDepthStencil = best;

			if (this->mBestDepthStencilReplacement != nullptr)
			{
				this->mBestDepthStencilReplacement->Release();
				this->mBestDepthStencilReplacement = nullptr;
			}
			if (this->mDepthStencilShaderResourceView != nullptr)
			{
				this->mDepthStencilShaderResourceView->Release();
				this->mDepthStencilShaderResourceView = nullptr;
			}

			ID3D10Texture2D *texture = nullptr;
			best->GetResource(reinterpret_cast<ID3D10Resource **>(&texture));

			D3D10_TEXTURE2D_DESC textureDesc;
			texture->GetDesc(&textureDesc);

			HRESULT hr = S_OK;

			if ((textureDesc.BindFlags & D3D10_BIND_SHADER_RESOURCE) == 0 || textureDesc.Format != DXGI_FORMAT_R24G8_TYPELESS)
			{
				texture->Release();

				textureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
				textureDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;

				hr = this->mDevice->CreateTexture2D(&textureDesc, nullptr, &texture);

				if (SUCCEEDED(hr))
				{
					D3D10_DEPTH_STENCIL_VIEW_DESC depthstencilDesc;
					depthstencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
					depthstencilDesc.ViewDimension = textureDesc.SampleDesc.Count > 1 ? D3D10_DSV_DIMENSION_TEXTURE2DMS : D3D10_DSV_DIMENSION_TEXTURE2D;
					depthstencilDesc.Texture2D.MipSlice = 0;

					this->mDevice->CreateDepthStencilView(texture, &depthstencilDesc, &this->mBestDepthStencilReplacement);
				}
				else
				{
					LOG(ERROR) << "Failed to create depthstencil replacement texture.";
				}
			}

			if (SUCCEEDED(hr))
			{
				D3D10_SHADER_RESOURCE_VIEW_DESC textureviewDesc;
				textureviewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				textureviewDesc.ViewDimension = textureDesc.SampleDesc.Count > 1 ? D3D10_SRV_DIMENSION_TEXTURE2DMS : D3D10_SRV_DIMENSION_TEXTURE2D;
				textureviewDesc.Texture2D.MipLevels = 1;
				textureviewDesc.Texture2D.MostDetailedMip = 0;

				this->mDevice->CreateShaderResourceView(texture, &textureviewDesc, &this->mDepthStencilShaderResourceView);

				texture->Release();

				// Update auto depthstencil
				ID3D10RenderTargetView *rendertargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
				ID3D10DepthStencilView *depthstencil = nullptr;

				this->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, rendertargets, &depthstencil);

				if (depthstencil != nullptr)
				{
					if (depthstencil == this->mBestDepthStencil)
					{
						this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, rendertargets, this->mBestDepthStencilReplacement);
					}

					depthstencil->Release();
				}

				for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					if (rendertargets[i] != nullptr)
					{
						rendertargets[i]->Release();
					}
				}
			}
		}
	}
	void D3D10Runtime::CreateDepthStencil(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil)
	{
		CSLock lock(this->mCS);

		ID3D10Texture2D *texture = nullptr;
			
		const HRESULT hr = resource->QueryInterface(__uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&texture));

		if (SUCCEEDED(hr))
		{
			D3D10_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			D3D10DepthStencilInfo info;
			info.Width = desc.Width;
			info.Height = desc.Height;
			info.DrawCallCount = 0;

			this->mDepthStencilTable.emplace(depthstencil, info);

			texture->Release();
		}
	}
	void D3D10Runtime::ReplaceDepthStencil(ID3D10DepthStencilView **pDepthStencil)
	{
		ID3D10DepthStencilView *depthstencil = *pDepthStencil;

		CSLock lock(this->mCS);

		if (this->mBestDepthStencilReplacement != nullptr && depthstencil == this->mBestDepthStencil)
		{
			*pDepthStencil = this->mBestDepthStencilReplacement;
		}
	}

	D3D10Effect::D3D10Effect(std::shared_ptr<const D3D10Runtime> context) : mEffectContext(context), mConstantsDirty(true), mDepthStencilTexture(nullptr), mDepthStencilView(nullptr), mDepthStencil(nullptr), mRasterizerState(nullptr)
	{
		HRESULT hr;
		D3D10_TEXTURE2D_DESC dstdesc;
		this->mEffectContext->mBackBuffer->GetDesc(&dstdesc);

		dstdesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		dstdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_DEPTH_STENCIL;
		D3D10_SHADER_RESOURCE_VIEW_DESC dssdesc;
		dssdesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		dssdesc.ViewDimension = dstdesc.SampleDesc.Count > 1 ? D3D10_SRV_DIMENSION_TEXTURE2DMS : D3D10_SRV_DIMENSION_TEXTURE2D;
		dssdesc.Texture2D.MipLevels = 1;
		dssdesc.Texture2D.MostDetailedMip = 0;
		D3D10_DEPTH_STENCIL_VIEW_DESC dsdesc;
		dsdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsdesc.ViewDimension = dstdesc.SampleDesc.Count > 1 ? D3D10_DSV_DIMENSION_TEXTURE2DMS : D3D10_DSV_DIMENSION_TEXTURE2D;
		dsdesc.Texture2D.MipSlice = 0;

		hr = context->mDevice->CreateTexture2D(&dstdesc, nullptr, &this->mDepthStencilTexture);

		assert(SUCCEEDED(hr));

		hr = context->mDevice->CreateShaderResourceView(this->mDepthStencilTexture, &dssdesc, &this->mDepthStencilView);

		assert(SUCCEEDED(hr));

		hr = context->mDevice->CreateDepthStencilView(this->mDepthStencilTexture, &dsdesc, &this->mDepthStencil);

		assert(SUCCEEDED(hr));

		D3D10_RASTERIZER_DESC rsdesc;
		ZeroMemory(&rsdesc, sizeof(D3D10_RASTERIZER_DESC));
		rsdesc.FillMode = D3D10_FILL_SOLID;
		rsdesc.CullMode = D3D10_CULL_NONE;
		rsdesc.DepthClipEnable = TRUE;

		hr = context->mDevice->CreateRasterizerState(&rsdesc, &this->mRasterizerState);

		assert(SUCCEEDED(hr));
	}
	D3D10Effect::~D3D10Effect()
	{
		if (this->mRasterizerState != nullptr)
		{
			this->mRasterizerState->Release();
		}
		if (this->mDepthStencil != nullptr)
		{
			this->mDepthStencil->Release();
		}
		if (this->mDepthStencilView != nullptr)
		{
			this->mDepthStencilView->Release();
		}
		if (this->mDepthStencilTexture != nullptr)
		{
			this->mDepthStencilTexture->Release();
		}

		for (auto &it : this->mSamplerStates)
		{
			it->Release();
		}
			
		for (auto &it : this->mConstantBuffers)
		{
			if (it != nullptr)
			{
				it->Release();
			}
		}
		for (auto &it : this->mConstantStorages)
		{
			::free(it);
		}
	}

	const Effect::Texture *D3D10Effect::GetTexture(const std::string &name) const
	{
		auto it = this->mTextures.find(name);

		if (it == this->mTextures.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> D3D10Effect::GetTextureNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mTextures.size());

		for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}
	const Effect::Constant *D3D10Effect::GetConstant(const std::string &name) const
	{
		auto it = this->mConstants.find(name);

		if (it == this->mConstants.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> D3D10Effect::GetConstantNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mConstants.size());

		for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}
	const Effect::Technique *D3D10Effect::GetTechnique(const std::string &name) const
	{
		auto it = this->mTechniques.find(name);

		if (it == this->mTechniques.end())
		{
			return nullptr;
		}

		return it->second.get();
	}
	std::vector<std::string> D3D10Effect::GetTechniqueNames() const
	{
		std::vector<std::string> names;
		names.reserve(this->mTechniques.size());

		for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
		{
			names.push_back(it->first);
		}

		return names;
	}

	void D3D10Effect::ApplyConstants() const
	{
		for (size_t i = 0, count = this->mConstantBuffers.size(); i < count; ++i)
		{
			ID3D10Buffer *buffer = this->mConstantBuffers[i];
			const unsigned char *storage = this->mConstantStorages[i];

			if (buffer == nullptr)
			{
				continue;
			}

			void *data;

			HRESULT hr = buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &data);

			if (FAILED(hr))
			{
				continue;
			}

			D3D10_BUFFER_DESC desc;
			buffer->GetDesc(&desc);

			std::memcpy(data, storage, desc.ByteWidth);

			buffer->Unmap();
		}

		this->mConstantsDirty = false;
	}

	D3D10Texture::D3D10Texture(D3D10Effect *effect) : mEffect(effect), mTexture(nullptr), mShaderResourceView(), mRenderTargetView()
	{
	}
	D3D10Texture::~D3D10Texture()
	{
		if (this->mRenderTargetView[0] != nullptr)
		{
			this->mRenderTargetView[0]->Release();
		}
		if (this->mRenderTargetView[1] != nullptr)
		{
			this->mRenderTargetView[1]->Release();
		}

		if (this->mShaderResourceView[0] != nullptr)
		{
			this->mShaderResourceView[0]->Release();
		}
		if (this->mShaderResourceView[1] != nullptr)
		{
			this->mShaderResourceView[1]->Release();
		}

		if (this->mTexture != nullptr)
		{
			this->mTexture->Release();
		}
	}

	const Effect::Texture::Description D3D10Texture::GetDescription() const
	{
		return this->mDesc;
	}
	const Effect::Annotation D3D10Texture::GetAnnotation(const std::string &name) const
	{
		auto it = this->mAnnotations.find(name);

		if (it == this->mAnnotations.end())
		{
			return Effect::Annotation();
		}

		return it->second;
	}

	void D3D10Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
	{
		assert(data != nullptr || size == 0);

		if (size == 0) return;

		this->mEffect->mEffectContext->mDevice->UpdateSubresource(this->mTexture, level, nullptr, data, size / this->mDesc.Height, size);
	}
	void D3D10Texture::UpdateFromColorBuffer()
	{
		D3D10_TEXTURE2D_DESC desc;
		this->mEffect->mEffectContext->mBackBufferTexture->GetDesc(&desc);

		if (desc.SampleDesc.Count == 1)
		{
			this->mEffect->mEffectContext->mDevice->CopyResource(this->mTexture, this->mEffect->mEffectContext->mBackBufferTexture);
		}
		else
		{
			this->mEffect->mEffectContext->mDevice->ResolveSubresource(this->mTexture, 0, this->mEffect->mEffectContext->mBackBufferTexture, 0, D3D10EffectCompiler::TypelessToLinearFormat(desc.Format));
		}
	}
	void D3D10Texture::UpdateFromDepthBuffer()
	{
		if (this->mShaderResourceView[0] == this->mEffect->mEffectContext->mDepthStencilShaderResourceView)
		{
			return;
		}

		assert(this->mRenderTargetView[0] == nullptr && this->mRenderTargetView[1] == nullptr);

		if (this->mShaderResourceView[0] != nullptr)
		{
			this->mShaderResourceView[0]->Release();
		}
		if (this->mShaderResourceView[1] != nullptr)
		{
			this->mShaderResourceView[1]->Release();
		}
		if (this->mTexture != nullptr)
		{
			this->mTexture->Release();
		}

		ID3D10ShaderResourceView *prevView[2] = { this->mShaderResourceView[0], this->mShaderResourceView[1] };

		this->mTexture = nullptr;
		this->mShaderResourceView[0] = this->mEffect->mEffectContext->mDepthStencilShaderResourceView;
		this->mShaderResourceView[1] = nullptr;

		if (this->mShaderResourceView[0] != nullptr)
		{
			this->mShaderResourceView[0]->GetResource(reinterpret_cast<ID3D10Resource **>(&this->mTexture));

			D3D10_TEXTURE2D_DESC desc;
			this->mTexture->GetDesc(&desc);

			this->mDesc.Width = desc.Width;
			this->mDesc.Height = desc.Height;
			this->mDesc.Format = Effect::Texture::Format::Unknown;
			this->mDesc.Levels = desc.MipLevels;

			this->mShaderResourceView[0]->AddRef();
		}

		for (auto &technique : this->mEffect->mTechniques)
		{
			for (auto &pass : technique.second->mPasses)
			{
				for (auto &SR : pass.SR)
				{
					if (SR == prevView[0] || SR == prevView[1])
					{
						SR = this->mShaderResourceView[0];
					}
				}
			}
		}
	}

	D3D10Constant::D3D10Constant(D3D10Effect *effect) : mEffect(effect)
	{
	}
	D3D10Constant::~D3D10Constant()
	{
	}

	const Effect::Constant::Description D3D10Constant::GetDescription() const
	{
		return this->mDesc;
	}
	const Effect::Annotation D3D10Constant::GetAnnotation(const std::string &name) const
	{
		auto it = this->mAnnotations.find(name);

		if (it == this->mAnnotations.end())
		{
			return Effect::Annotation();
		}

		return it->second;
	}
	void D3D10Constant::GetValue(unsigned char *data, std::size_t size) const
	{
		size = std::min(size, this->mDesc.Size);

		const unsigned char *storage = this->mEffect->mConstantStorages[this->mBuffer] + this->mBufferOffset;

		std::memcpy(data, storage, size);
	}
	void D3D10Constant::SetValue(const unsigned char *data, std::size_t size)
	{
		size = std::min(size, this->mDesc.Size);

		unsigned char *storage = this->mEffect->mConstantStorages[this->mBuffer] + this->mBufferOffset;

		if (std::memcmp(storage, data, size) == 0)
		{
			return;
		}

		std::memcpy(storage, data, size);

		this->mEffect->mConstantsDirty = true;
	}

	D3D10Technique::D3D10Technique(D3D10Effect *effect) : mEffect(effect)
	{
	}
	D3D10Technique::~D3D10Technique()
	{
		for (auto &pass : this->mPasses)
		{
			if (pass.VS != nullptr)
			{
				pass.VS->Release();
			}
			if (pass.PS != nullptr)
			{
				pass.PS->Release();
			}
			if (pass.BS != nullptr)
			{
				pass.BS->Release();
			}
			if (pass.DSS != nullptr)
			{
				pass.DSS->Release();
			}
		}
	}

	const Effect::Annotation D3D10Technique::GetAnnotation(const std::string &name) const
	{
		auto it = this->mAnnotations.find(name);

		if (it == this->mAnnotations.end())
		{
			return Effect::Annotation();
		}

		return it->second;
	}

	bool D3D10Technique::Begin(unsigned int &passes) const
	{
		passes = static_cast<unsigned int>(this->mPasses.size());

		if (passes == 0)
		{
			return false;
		}

		ID3D10Device *device = this->mEffect->mEffectContext->mDevice;

		const uintptr_t null = 0;
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		device->IASetInputLayout(nullptr);
		device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		device->RSSetState(this->mEffect->mRasterizerState);

		device->VSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
		device->PSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
		device->VSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());
		device->PSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());

		assert(this->mEffect->mDepthStencil != nullptr);

		device->ClearDepthStencilView(this->mEffect->mDepthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0x00);

		return true;
	}
	void D3D10Technique::End() const
	{
		this->mEffect->mEffectContext->mDevice->OMSetRenderTargets(1, &this->mEffect->mEffectContext->mBackBufferTargets[0], nullptr);
	}
	void D3D10Technique::RenderPass(unsigned int index) const
	{
		if (this->mEffect->mConstantsDirty)
		{
			this->mEffect->ApplyConstants();
		}

		ID3D10Device *device = this->mEffect->mEffectContext->mDevice;
		const Pass &pass = this->mPasses[index];

		device->VSSetShader(pass.VS);
		device->GSSetShader(nullptr);
		device->PSSetShader(pass.PS);

		const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		device->OMSetBlendState(pass.BS, blendfactor, D3D10_DEFAULT_SAMPLE_MASK);
		device->OMSetDepthStencilState(pass.DSS, pass.StencilRef);
		device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, this->mEffect->mDepthStencil);

		for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			if (pass.RT[i] == nullptr)
			{
				continue;
			}

			const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			device->ClearRenderTargetView(pass.RT[i], color);
		}

		device->VSSetShaderResources(0, pass.SR.size(), pass.SR.data());
		device->PSSetShaderResources(0, pass.SR.size(), pass.SR.data());

		ID3D10Resource *rtres;
		pass.RT[0]->GetResource(&rtres);
		D3D10_TEXTURE2D_DESC rtdesc;
		static_cast<ID3D10Texture2D *>(rtres)->GetDesc(&rtdesc);
		rtres->Release();

		D3D10_VIEWPORT viewport;
		viewport.Width = rtdesc.Width;
		viewport.Height = rtdesc.Height;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		device->RSSetViewports(1, &viewport);

		device->Draw(3, 0);

		device->OMSetRenderTargets(0, nullptr, nullptr);

		ID3D10ShaderResourceView *null[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
		device->VSSetShaderResources(0, pass.SR.size(), null);
		device->PSSetShaderResources(0, pass.SR.size(), null);
	}
}