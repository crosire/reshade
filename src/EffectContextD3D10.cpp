#include "Log.hpp"
#include "Effect.hpp"
#include "EffectContext.hpp"
#include "EffectTree.hpp"

#include <d3d10_1.h>
#include <d3dcompiler.h>
#include <vector>
#include <unordered_map>

// -----------------------------------------------------------------------------------------------------

#define SAFE_RELEASE(p)											{ if ((p)) (p)->Release(); (p) = nullptr; }

namespace std
{
	template <> struct											hash<D3D10_SAMPLER_DESC>
	{
		inline std::size_t										operator()(const D3D10_SAMPLER_DESC &s) const 
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
			std::size_t h = 2166136261;

			for (std::size_t i = 0; i < sizeof(D3D10_SAMPLER_DESC); ++i)
			{
				h = (h * 16777619) ^ p[i];
			}

			return h;
		}
	};

	inline bool													operator ==(const D3D10_SAMPLER_DESC &left, const D3D10_SAMPLER_DESC &right)
	{
		return std::memcmp(&left, &right, sizeof(D3D10_SAMPLER_DESC)) == 0;
	}
}
namespace ReShade
{
	namespace
	{
		class													D3D10EffectContext : public EffectContext, public std::enable_shared_from_this<D3D10EffectContext>
		{
			friend struct D3D10Effect;
			friend struct D3D10Texture;
			friend struct D3D10Constant;
			friend struct D3D10Technique;
			friend class D3D10EffectCompiler;

		public:
			D3D10EffectContext(ID3D10Device *device, IDXGISwapChain *swapchain);
			~D3D10EffectContext(void);

			void												GetDimension(unsigned int &width, unsigned int &height) const;
		
			std::unique_ptr<Effect>								Compile(const EffectTree &ast, std::string &errors);

		private:
			ID3D10Device *										mDevice;
			IDXGISwapChain *									mSwapChain;
		};
		struct													D3D10Effect : public Effect
		{
			friend struct D3D10Texture;
			friend struct D3D10Constant;
			friend struct D3D10Technique;

			D3D10Effect(std::shared_ptr<D3D10EffectContext> context);
			~D3D10Effect(void);

			const Texture *										GetTexture(const std::string &name) const;
			std::vector<std::string>							GetTextureNames(void) const;
			const Constant *									GetConstant(const std::string &name) const;
			std::vector<std::string>							GetConstantNames(void) const;
			const Technique *									GetTechnique(const std::string &name) const;
			std::vector<std::string>							GetTechniqueNames(void) const;

			void												ApplyConstants(void) const;

			std::shared_ptr<D3D10EffectContext>					mEffectContext;
			std::unordered_map<std::string, std::unique_ptr<D3D10Texture>> mTextures;
			std::unordered_map<std::string, std::unique_ptr<D3D10Constant>> mConstants;
			std::unordered_map<std::string, std::unique_ptr<D3D10Technique>> mTechniques;
			ID3D10Texture2D *									mBackBuffer;
			ID3D10Texture2D *									mBackBufferTexture;
			ID3D10RenderTargetView *							mBackBufferTargets[2];
			ID3D10Texture2D *									mDepthStencilTexture;
			ID3D10ShaderResourceView *							mDepthStencilView;
			ID3D10DepthStencilView *							mDepthStencil;
			ID3D10StateBlock *									mStateblock;
			ID3D10RenderTargetView *							mStateblockTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			std::unordered_map<D3D10_SAMPLER_DESC, size_t>		mSamplerDescs;
			std::vector<ID3D10SamplerState *>					mSamplerStates;
			std::vector<ID3D10ShaderResourceView *>				mShaderResources;
			std::vector<ID3D10Buffer *>							mConstantBuffers;
			std::vector<unsigned char *>						mConstantStorages;
			mutable bool										mConstantsDirty;
		};
		struct													D3D10Texture : public Effect::Texture
		{
			D3D10Texture(D3D10Effect *effect);
			~D3D10Texture(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			void												Update(unsigned int level, const unsigned char *data, std::size_t size);
			void												UpdateFromColorBuffer(void);
			void												UpdateFromDepthBuffer(void);

			D3D10Effect *										mEffect;
			Description											mDesc;
			unsigned int										mRegister;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			ID3D10Texture2D *									mTexture;
			ID3D10ShaderResourceView *							mShaderResourceView[2];
			ID3D10RenderTargetView *							mRenderTargetView[2];
		};
		struct													D3D10Constant : public Effect::Constant
		{
			D3D10Constant(D3D10Effect *effect);
			~D3D10Constant(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;
			void												GetValue(unsigned char *data, std::size_t size) const;
			void												SetValue(const unsigned char *data, std::size_t size);

			D3D10Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::size_t											mBuffer, mBufferOffset;
		};
		struct													D3D10Technique : public Effect::Technique
		{
			struct												Pass
			{
				ID3D10VertexShader *							VS;
				ID3D10PixelShader *								PS;
				ID3D10BlendState *								BS;
				ID3D10DepthStencilState *						DSS;
				UINT											StencilRef;
				ID3D10RenderTargetView *						RT[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
				std::vector<ID3D10ShaderResourceView *>			SR;
			};

			D3D10Technique(D3D10Effect *effect);
			~D3D10Technique(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Begin(unsigned int &passes) const;
			void												End(void) const;
			void												RenderPass(unsigned int index) const;

			D3D10Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::vector<Pass>									mPasses;
		};

		class													D3D10EffectCompiler
		{
		public:
			D3D10EffectCompiler(const EffectTree &ast) : mAST(ast), mEffect(nullptr), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentGlobalSize(0), mCurrentGlobalStorageSize(0)
			{
			}

			bool												Traverse(D3D10Effect *effect, std::string &errors)
			{
				this->mEffect = effect;
				this->mErrors.clear();
				this->mFatal = false;
				this->mCurrentSource.clear();

				// Global constant buffer
				this->mEffect->mConstantBuffers.push_back(nullptr);
				this->mEffect->mConstantStorages.push_back(nullptr);

				const auto &root = this->mAST[EffectTree::Root].As<EffectNodes::List>();

				for (unsigned int i = 0; i < root.Length; ++i)
				{
					this->mAST[root[i]].Accept(*this);
				}

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

			static inline UINT									RoundToMultipleOf16(UINT size)
			{
				return (size + 15) & ~15;
			}
			static D3D10_TEXTURE_ADDRESS_MODE					LiteralToTextureAddress(int value)
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
			static D3D10_COMPARISON_FUNC						LiteralToComparisonFunc(int value)
			{
				const D3D10_COMPARISON_FUNC conversion[] =
				{
					D3D10_COMPARISON_NEVER, D3D10_COMPARISON_ALWAYS, D3D10_COMPARISON_LESS, D3D10_COMPARISON_GREATER, D3D10_COMPARISON_LESS_EQUAL, D3D10_COMPARISON_GREATER_EQUAL, D3D10_COMPARISON_EQUAL, D3D10_COMPARISON_NOT_EQUAL
				};

				const unsigned int conversionIndex = value - EffectNodes::Literal::NEVER;

				assert(conversionIndex < ARRAYSIZE(conversion));

				return conversion[conversionIndex];
			}
			static D3D10_STENCIL_OP								LiteralToStencilOp(int value)
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
			static D3D10_BLEND									LiteralToBlend(int value)
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
			static D3D10_BLEND_OP								LiteralToBlendOp(int value)
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
			static DXGI_FORMAT									LiteralToFormat(int value, Effect::Texture::Format &format)
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
			static DXGI_FORMAT									TypelessToLinearFormat(DXGI_FORMAT format)
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
			static DXGI_FORMAT									TypelessToSRGBFormat(DXGI_FORMAT format)
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
			
			static inline std::string							PrintLocation(const EffectTree::Location &location)
			{
				return std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): ";
			}

			std::string											PrintType(const EffectNodes::Type &type)
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
			std::string											PrintTypeWithQualifiers(const EffectNodes::Type &type)
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

			void												Visit(const EffectNodes::LValue &node)
			{
				this->mCurrentSource += this->mAST[node.Reference].As<EffectNodes::Variable>().Name;
			}
			void												Visit(const EffectNodes::Literal &node)
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
			void												Visit(const EffectNodes::Expression &node)
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
						this->mAST[node.Operands[0]].Accept(*this);
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
				}

				this->mCurrentSource += part1;
				this->mAST[node.Operands[0]].Accept(*this);
				this->mCurrentSource += part2;

				if (node.Operands[1] != 0)
				{
					this->mAST[node.Operands[1]].Accept(*this);
				}

				this->mCurrentSource += part3;

				if (node.Operands[2] != 0)
				{
					this->mAST[node.Operands[2]].Accept(*this);
				}

				this->mCurrentSource += part4;
			}
			void												Visit(const EffectNodes::Sequence &node)
			{
				for (unsigned int i = 0; i < node.Length; ++i)
				{
					this->mAST[node[i]].Accept(*this);

					this->mCurrentSource += ", ";
				}

				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();
			}
			void												Visit(const EffectNodes::Assignment &node)
			{
				this->mCurrentSource += '(';
				this->mAST[node.Left].Accept(*this);
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
				this->mAST[node.Right].Accept(*this);
				this->mCurrentSource += ')';
			}
			void												Visit(const EffectNodes::Call &node)
			{
				this->mCurrentSource += node.CalleeName;
				this->mCurrentSource += '(';

				if (node.HasArguments())
				{
					const auto &arguments = this->mAST[node.Arguments].As<EffectNodes::List>();

					for (unsigned int i = 0; i < arguments.Length; ++i)
					{
						this->mAST[arguments[i]].Accept(*this);

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();
				}

				this->mCurrentSource += ')';
			}
			void												Visit(const EffectNodes::Constructor &node)
			{
				this->mCurrentSource += PrintType(node.Type);
				this->mCurrentSource += '(';

				const auto &arguments = this->mAST[node.Arguments].As<EffectNodes::List>();

				for (unsigned int i = 0; i < arguments.Length; ++i)
				{
					this->mAST[arguments[i]].Accept(*this);

					this->mCurrentSource += ", ";
				}

				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();

				this->mCurrentSource += ')';
			}
			void												Visit(const EffectNodes::Swizzle &node)
			{
				const EffectNodes::RValue &left = this->mAST[node.Operands[0]].As<EffectNodes::RValue>();

				left.Accept(*this);

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
			void												Visit(const EffectNodes::If &node)
			{
				if (node.HasAttributes())
				{
					const auto &attributes = this->mAST[node.Attributes].As<EffectNodes::List>();

					for (unsigned int i = 0; i < attributes.Length; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += this->mAST[attributes[i]].As<EffectNodes::Literal>().Value.String;
						this->mCurrentSource += ']';
					}
				}

				this->mCurrentSource += "if (";
				this->mAST[node.Condition].Accept(*this);
				this->mCurrentSource += ")\n";

				if (node.StatementOnTrue != 0)
				{
					this->mAST[node.StatementOnTrue].Accept(*this);
				}
				else
				{
					this->mCurrentSource += "\t;";
				}
				if (node.StatementOnFalse != 0)
				{
					this->mCurrentSource += "else\n";
					this->mAST[node.StatementOnFalse].Accept(*this);
				}
			}
			void												Visit(const EffectNodes::Switch &node)
			{
				if (node.HasAttributes())
				{
					const auto &attributes = this->mAST[node.Attributes].As<EffectNodes::List>();

					for (unsigned int i = 0; i < attributes.Length; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += this->mAST[attributes[i]].As<EffectNodes::Literal>().Value.String;
						this->mCurrentSource += ']';
					}
				}

				this->mCurrentSource += "switch (";
				this->mAST[node.Test].Accept(*this);
				this->mCurrentSource += ")\n{\n";

				const auto &cases = this->mAST[node.Cases].As<EffectNodes::List>();

				for (unsigned int i = 0; i < cases.Length; ++i)
				{
					this->mAST[cases[i]].As<EffectNodes::Case>().Accept(*this);
				}

				this->mCurrentSource += "}\n";
			}
			void												Visit(const EffectNodes::Case &node)
			{
				const auto &labels = this->mAST[node.Labels].As<EffectNodes::List>();

				for (unsigned int i = 0; i < labels.Length; ++i)
				{
					const auto &label = this->mAST[labels[i]];

					if (label.Is<EffectNodes::Expression>())
					{
						this->mCurrentSource += "default";
					}
					else
					{
						this->mCurrentSource += "case ";
						label.As<EffectNodes::Literal>().Accept(*this);
					}

					this->mCurrentSource += ":\n";
				}

				this->mAST[node.Statements].As<EffectNodes::StatementBlock>().Accept(*this);
			}
			void												Visit(const EffectNodes::For &node)
			{
				if (node.HasAttributes())
				{
					const auto &attributes = this->mAST[node.Attributes].As<EffectNodes::List>();

					for (unsigned int i = 0; i < attributes.Length; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += this->mAST[attributes[i]].As<EffectNodes::Literal>().Value.String;
						this->mCurrentSource += ']';
					}
				}

				this->mCurrentSource += "for (";

				if (node.Initialization != 0)
				{
					this->mAST[node.Initialization].Accept(*this);

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();
				}

				this->mCurrentSource += "; ";
										
				if (node.Condition != 0)
				{
					this->mAST[node.Condition].Accept(*this);
				}

				this->mCurrentSource += "; ";

				if (node.Iteration != 0)
				{
					this->mAST[node.Iteration].Accept(*this);
				}

				this->mCurrentSource += ")\n";

				if (node.Statements != 0)
				{
					this->mAST[node.Statements].Accept(*this);
				}
				else
				{
					this->mCurrentSource += "\t;";
				}
			}
			void												Visit(const EffectNodes::While &node)
			{
				if (node.HasAttributes())
				{
					const auto &attributes = this->mAST[node.Attributes].As<EffectNodes::List>();

					for (unsigned int i = 0; i < attributes.Length; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += this->mAST[attributes[i]].As<EffectNodes::Literal>().Value.String;
						this->mCurrentSource += ']';
					}
				}

				if (node.DoWhile)
				{
					this->mCurrentSource += "do\n{\n";

					if (node.Statements != 0)
					{
						this->mAST[node.Statements].Accept(*this);
					}

					this->mCurrentSource += "}\n";
					this->mCurrentSource += "while (";
					this->mAST[node.Condition].Accept(*this);
					this->mCurrentSource += ");\n";
				}
				else
				{
					this->mCurrentSource += "while (";
					this->mAST[node.Condition].Accept(*this);
					this->mCurrentSource += ")\n";

					if (node.Statements != 0)
					{
						this->mAST[node.Statements].Accept(*this);
					}
					else
					{
						this->mCurrentSource += "\t;";
					}
				}
			}
			void												Visit(const EffectNodes::Jump &node)
			{
				switch (node.Mode)
				{
					case EffectNodes::Jump::Return:
						this->mCurrentSource += "return";

						if (node.Value != 0)
						{
							this->mCurrentSource += ' ';
							this->mAST[node.Value].Accept(*this);
						}
						break;
					case EffectNodes::Jump::Break:
						this->mCurrentSource += "break";
						break;
					case EffectNodes::Jump::Continue:
						this->mCurrentSource += "continue";
						break;
					case EffectNodes::Jump::Discard:
						this->mCurrentSource += "discard";
						break;
				}

				this->mCurrentSource += ";\n";
			}
			void												Visit(const EffectNodes::ExpressionStatement &node)
			{
				if (node.Expression != 0)
				{
					this->mAST[node.Expression].Accept(*this);
				}

				this->mCurrentSource += ";\n";
			}
			void												Visit(const EffectNodes::StatementBlock &node)
			{
				this->mCurrentSource += "{\n";

				for (unsigned int i = 0; i < node.Length; ++i)
				{
					this->mAST[node[i]].Accept(*this);
				}

				this->mCurrentSource += "}\n";
			}
			void												Visit(const EffectNodes::Annotation &node)
			{
				Effect::Annotation annotation;
				const auto &value = this->mAST[node.Value].As<EffectNodes::Literal>();

				switch (value.Type.Class)
				{
					case EffectNodes::Type::Bool:
						annotation = value.Value.Bool[0] != 0;
						break;
					case EffectNodes::Type::Int:
						annotation = value.Value.Int[0];
						break;
					case EffectNodes::Type::Uint:
						annotation = value.Value.Uint[0];
						break;
					case EffectNodes::Type::Float:
						annotation = value.Value.Float[0];
						break;
					case EffectNodes::Type::String:
						annotation = value.Value.String;
						break;
				}

				assert(this->mCurrentAnnotations != nullptr);

				this->mCurrentAnnotations->insert(std::make_pair(node.Name, annotation));
			}
			void												Visit(const EffectNodes::Struct &node)
			{
				this->mCurrentSource += "struct ";

				if (node.Name != nullptr)
				{
					this->mCurrentSource += node.Name;
				}

				this->mCurrentSource += "\n{\n";

				if (node.HasFields())
				{
					const auto &fields = this->mAST[node.Fields].As<EffectNodes::List>();

					for (unsigned int i = 0; i < fields.Length; ++i)
					{
						this->mAST[fields[i]].As<EffectNodes::Variable>().Accept(*this);
					}
				}
				else
				{
					this->mCurrentSource += "float _dummy;\n";
				}

				this->mCurrentSource += "};\n";
			}
			void												Visit(const EffectNodes::Variable &node)
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

				this->mCurrentSource += PrintTypeWithQualifiers(node.Type);

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

				if (node.HasInitializer())
				{
					this->mCurrentSource += " = ";

					this->mAST[node.Initializer].Accept(*this);
				}

				if (!this->mCurrentInParameterBlock)
				{
					this->mCurrentSource += ";\n";
				}
			}
			void												VisitTexture(const EffectNodes::Variable &node)
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

				if (node.HasAnnotations())
				{
					const auto &annotations = this->mAST[node.Annotations].As<EffectNodes::List>();

					this->mCurrentAnnotations = &obj->mAnnotations;

					for (unsigned int i = 0; i < annotations.Length; ++i)
					{
						this->mAST[annotations[i]].As<EffectNodes::Annotation>().Accept(*this);
					}

					this->mCurrentAnnotations = nullptr;
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
			void												VisitSampler(const EffectNodes::Variable &node)
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
			void												VisitUniform(const EffectNodes::Variable &node)
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

				if (node.HasInitializer())
				{
					::memcpy(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, &this->mAST[node.Initializer].As<EffectNodes::Literal>().Value, obj->mDesc.Size);
				}
				else
				{
					::memset(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, 0, obj->mDesc.Size);
				}

				this->mEffect->mConstants.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void												VisitUniformBuffer(const EffectNodes::Variable &node)
			{
				const auto &structure = this->mAST[node.Type.Definition].As<EffectNodes::Struct>();

				if (!structure.HasFields())
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

				const auto &fields = this->mAST[structure.Fields].As<EffectNodes::List>();

				for (unsigned int i = 0; i < fields.Length; ++i)
				{
					const auto &field = this->mAST[fields[i]].As<EffectNodes::Variable>();

					field.Accept(*this);

					std::unique_ptr<D3D10Constant> obj(new D3D10Constant(this->mEffect));
					obj->mDesc.Rows = field.Type.Rows;
					obj->mDesc.Columns = field.Type.Cols;
					obj->mDesc.Elements = field.Type.ArrayLength;
					obj->mDesc.Fields = 0;
					obj->mDesc.Size = field.Type.Rows * field.Type.Cols;

					switch (field.Type.Class)
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

					if (field.HasInitializer())
					{
						::memcpy(storage + obj->mBufferOffset, &this->mAST[field.Initializer].As<EffectNodes::Literal>().Value, obj->mDesc.Size);
					}
					else
					{
						::memset(storage + obj->mBufferOffset, 0, obj->mDesc.Size);
					}

					this->mEffect->mConstants.insert(std::make_pair(std::string(node.Name) + '.' + std::string(field.Name), std::move(obj)));
				}

				this->mCurrentBlockName.clear();

				this->mCurrentSource += "};\n";

				std::unique_ptr<D3D10Constant> obj(new D3D10Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fields.Length;
				obj->mDesc.Size = totalsize;
				obj->mDesc.Type = Effect::Constant::Type::Struct;
				obj->mBuffer = this->mEffect->mConstantBuffers.size();
				obj->mBufferOffset = 0;

				if (node.HasAnnotations())
				{
					const auto &annotations = this->mAST[node.Annotations].As<EffectNodes::List>();

					this->mCurrentAnnotations = &obj->mAnnotations;

					for (unsigned int i = 0; i < annotations.Length; ++i)
					{
						this->mAST[annotations[i]].As<EffectNodes::Annotation>().Accept(*this);
					}

					this->mCurrentAnnotations = nullptr;
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
			void												Visit(const EffectNodes::Function &node)
			{
				this->mCurrentSource += PrintType(node.ReturnType);
				this->mCurrentSource += ' ';
				this->mCurrentSource += node.Name;
				this->mCurrentSource += '(';

				if (node.HasParameters())
				{
					const auto &parameters = this->mAST[node.Parameters].As<EffectNodes::List>();

					this->mCurrentInParameterBlock = true;

					for (unsigned int i = 0; i < parameters.Length; ++i)
					{
						this->mAST[parameters[i]].As<EffectNodes::Variable>().Accept(*this);

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();

					this->mCurrentInParameterBlock = false;
				}

				this->mCurrentSource += ')';
								
				if (node.ReturnSemantic != nullptr)
				{
					this->mCurrentSource += " : ";
					this->mCurrentSource += node.ReturnSemantic;
				}

				if (node.HasDefinition())
				{
					this->mCurrentSource += '\n';
					this->mCurrentInFunctionBlock = true;

					this->mAST[node.Definition].As<EffectNodes::StatementBlock>().Accept(*this);

					this->mCurrentInFunctionBlock = false;
				}
				else
				{
					this->mCurrentSource += ";\n";
				}
			}
			void												Visit(const EffectNodes::Technique &node)
			{
				std::unique_ptr<D3D10Technique> obj(new D3D10Technique(this->mEffect));

				const auto &passes = this->mAST[node.Passes].As<EffectNodes::List>();

				obj->mDesc.Passes.reserve(passes.Length);
				obj->mPasses.reserve(passes.Length);

				this->mCurrentPasses = &obj->mPasses;

				for (unsigned int i = 0; i < passes.Length; ++i)
				{
					const auto &pass = this->mAST[passes[i]].As<EffectNodes::Pass>();

					obj->mDesc.Passes.push_back(pass.Name != nullptr ? pass.Name : "");
					
					pass.Accept(*this);
				}

				this->mCurrentPasses = nullptr;

				if (node.HasAnnotations())
				{
					const auto &annotations = this->mAST[node.Annotations].As<EffectNodes::List>();

					this->mCurrentAnnotations = &obj->mAnnotations;

					for (unsigned int i = 0; i < annotations.Length; ++i)
					{
						this->mAST[annotations[i]].As<EffectNodes::Annotation>().Accept(*this);
					}

					this->mCurrentAnnotations = nullptr;
				}

				this->mEffect->mTechniques.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void												Visit(const EffectNodes::Pass &node)
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

				pass.RT[0] = this->mEffect->mBackBufferTargets[srgb];

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
						rtvdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
						rtvdesc.Texture2D.MipSlice = 0;
						rtvdesc.Format = srgb ? TypelessToSRGBFormat(desc.Format) : TypelessToLinearFormat(desc.Format);

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

						if (res1 == res2)
						{
							*it = nullptr;
							break;
						}

						res2->Release();
					}

					res1->Release();
				}

				assert(this->mCurrentPasses != nullptr);

				this->mCurrentPasses->push_back(std::move(pass));
			}
			void												VisitShader(const EffectNodes::Function &node, unsigned int type, D3D10Technique::Pass &pass)
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
					"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
					"inline float4 __tex2Dbias(__sampler2D s, float4 c) { return s.t.SampleBias(s.s, c.xy, c.w); }\n"
					"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n"
					"cbuffer __GLOBAL__ : register(b0)\n{\n" + this->mCurrentGlobalConstants + "};\n" + this->mCurrentSource;

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
			const EffectTree &									mAST;
			D3D10Effect *										mEffect;
			std::string											mCurrentSource;
			std::string											mErrors;
			bool												mFatal;
			std::string											mCurrentGlobalConstants;
			UINT												mCurrentGlobalSize, mCurrentGlobalStorageSize;
			std::string											mCurrentBlockName;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
			std::unordered_map<std::string, Effect::Annotation> *mCurrentAnnotations;
			std::vector<D3D10Technique::Pass> *					mCurrentPasses;
		};

		// -----------------------------------------------------------------------------------------------------

		D3D10EffectContext::D3D10EffectContext(ID3D10Device *device, IDXGISwapChain *swapchain) : mDevice(device), mSwapChain(swapchain)
		{
			this->mDevice->AddRef();
			this->mSwapChain->AddRef();
		}
		D3D10EffectContext::~D3D10EffectContext(void)
		{
			this->mDevice->Release();
			this->mSwapChain->Release();
		}

		void													D3D10EffectContext::GetDimension(unsigned int &width, unsigned int &height) const
		{
			ID3D10Texture2D *buffer;
			D3D10_TEXTURE2D_DESC desc;
			this->mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&buffer));
			buffer->GetDesc(&desc);
			buffer->Release();

			width = desc.Width;
			height = desc.Height;
		}

		std::unique_ptr<Effect>									D3D10EffectContext::Compile(const EffectTree &ast, std::string &errors)
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

		D3D10Effect::D3D10Effect(std::shared_ptr<D3D10EffectContext> context) : mEffectContext(context), mConstantsDirty(true)
		{
			context->mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&this->mBackBuffer));

			D3D10_STATE_BLOCK_MASK mask;
			D3D10StateBlockMaskEnableAll(&mask);
			D3D10CreateStateBlock(context->mDevice, &mask, &this->mStateblock);

			D3D10_TEXTURE2D_DESC dstdesc;
			this->mBackBuffer->GetDesc(&dstdesc);
			dstdesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
			dstdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_DEPTH_STENCIL;
			D3D10_SHADER_RESOURCE_VIEW_DESC dssdesc;
			ZeroMemory(&dssdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
			dssdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			dssdesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			dssdesc.Texture2D.MipLevels = 1;
			D3D10_DEPTH_STENCIL_VIEW_DESC dsdesc;
			ZeroMemory(&dsdesc, sizeof(D3D10_DEPTH_STENCIL_VIEW_DESC));
			dsdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsdesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
			context->mDevice->CreateTexture2D(&dstdesc, nullptr, &this->mDepthStencilTexture);
			context->mDevice->CreateShaderResourceView(this->mDepthStencilTexture, &dssdesc, &this->mDepthStencilView);
			context->mDevice->CreateDepthStencilView(this->mDepthStencilTexture, &dsdesc, &this->mDepthStencil);

			dstdesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
			dstdesc.BindFlags = D3D10_BIND_RENDER_TARGET;
			context->mDevice->CreateTexture2D(&dstdesc, nullptr, &this->mBackBufferTexture);
			D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
			ZeroMemory(&rtdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
			rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
			rtdesc.Texture2D.MipSlice = 0;
			rtdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			context->mDevice->CreateRenderTargetView(this->mBackBufferTexture, &rtdesc, &this->mBackBufferTargets[0]);
			rtdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			context->mDevice->CreateRenderTargetView(this->mBackBufferTexture, &rtdesc, &this->mBackBufferTargets[1]);
		}
		D3D10Effect::~D3D10Effect(void)
		{
			SAFE_RELEASE(this->mStateblock);
			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mDepthStencilView);
			SAFE_RELEASE(this->mDepthStencilTexture);
			SAFE_RELEASE(this->mBackBufferTargets[0]);
			SAFE_RELEASE(this->mBackBufferTexture);
			SAFE_RELEASE(this->mBackBuffer);

			for (auto &it : this->mSamplerStates)
			{
				it->Release();
			}
			
			for (auto &it : this->mConstantBuffers)
			{
				SAFE_RELEASE(it);
			}
			for (auto &it : this->mConstantStorages)
			{
				::free(it);
			}
		}

		const Effect::Texture *									D3D10Effect::GetTexture(const std::string &name) const
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D10Effect::GetTextureNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTextures.size());

			for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		const Effect::Constant *								D3D10Effect::GetConstant(const std::string &name) const
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D10Effect::GetConstantNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mConstants.size());

			for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		const Effect::Technique *								D3D10Effect::GetTechnique(const std::string &name) const
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D10Effect::GetTechniqueNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTechniques.size());

			for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}

		void													D3D10Effect::ApplyConstants(void) const
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
		D3D10Texture::~D3D10Texture(void)
		{
			SAFE_RELEASE(this->mRenderTargetView[0]);
			SAFE_RELEASE(this->mRenderTargetView[1]);
			SAFE_RELEASE(this->mShaderResourceView[0]);
			SAFE_RELEASE(this->mShaderResourceView[1]);
			SAFE_RELEASE(this->mTexture);
		}

		const Effect::Texture::Description						D3D10Texture::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D10Texture::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		void													D3D10Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
		{
			assert(data != nullptr || size == 0);

			if (size == 0) return;

			this->mEffect->mEffectContext->mDevice->UpdateSubresource(this->mTexture, level, nullptr, data, size / this->mDesc.Height, size);
		}
		void													D3D10Texture::UpdateFromColorBuffer(void)
		{
			// Must be called after D3D10Technique::Begin!

			D3D10_TEXTURE2D_DESC desc;
			this->mEffect->mBackBufferTexture->GetDesc(&desc);

			if (desc.SampleDesc.Count == 1)
			{
				this->mEffect->mEffectContext->mDevice->CopyResource(this->mTexture, this->mEffect->mBackBufferTexture);
			}
			else
			{
				this->mEffect->mEffectContext->mDevice->ResolveSubresource(this->mTexture, 0, this->mEffect->mBackBufferTexture, 0, desc.Format);
			}
		}
		void													D3D10Texture::UpdateFromDepthBuffer(void)
		{
		}

		D3D10Constant::D3D10Constant(D3D10Effect *effect) : mEffect(effect)
		{
		}
		D3D10Constant::~D3D10Constant(void)
		{
		}

		const Effect::Constant::Description						D3D10Constant::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D10Constant::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}
		void													D3D10Constant::GetValue(unsigned char *data, std::size_t size) const
		{
			size = std::min(size, this->mDesc.Size);

			const unsigned char *storage = this->mEffect->mConstantStorages[this->mBuffer] + this->mBufferOffset;

			std::memcpy(data, storage, size);
		}
		void													D3D10Constant::SetValue(const unsigned char *data, std::size_t size)
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
		D3D10Technique::~D3D10Technique(void)
		{
			for (auto &pass : this->mPasses)
			{
				SAFE_RELEASE(pass.VS);
				SAFE_RELEASE(pass.PS);
				SAFE_RELEASE(pass.BS);
				SAFE_RELEASE(pass.DSS);
			}
		}

		const Effect::Technique::Description					D3D10Technique::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D10Technique::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													D3D10Technique::Begin(unsigned int &passes) const
		{
			passes = static_cast<unsigned int>(this->mPasses.size());

			if (passes == 0 || FAILED(this->mEffect->mStateblock->Capture()))
			{
				return false;
			}

			this->mEffect->mEffectContext->mDevice->CopyResource(this->mEffect->mBackBufferTexture, this->mEffect->mBackBuffer);

			this->mEffect->mEffectContext->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, this->mEffect->mStateblockTargets, nullptr);

			ID3D10Device *device = this->mEffect->mEffectContext->mDevice;
			const uintptr_t null = 0;
			device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			device->IASetInputLayout(nullptr);
			device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

			device->VSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
			device->PSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
			device->VSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());
			device->PSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());

			device->ClearDepthStencilView(this->mEffect->mDepthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0x00);

			return true;
		}
		void													D3D10Technique::End(void) const
		{
			this->mEffect->mEffectContext->mDevice->CopyResource(this->mEffect->mBackBuffer, this->mEffect->mBackBufferTexture);

			this->mEffect->mStateblock->Apply();
			this->mEffect->mEffectContext->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, this->mEffect->mStateblockTargets, nullptr);

			for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				SAFE_RELEASE(this->mEffect->mStateblockTargets[i]);
			}
		}
		void													D3D10Technique::RenderPass(unsigned int index) const
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
			device->RSSetState(nullptr);

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
		}
	}

	// -----------------------------------------------------------------------------------------------------

	std::shared_ptr<EffectContext>								CreateEffectContext(ID3D10Device *device, IDXGISwapChain *swapchain)
	{
		assert (device != nullptr && swapchain != nullptr);

		return std::make_shared<D3D10EffectContext>(device, swapchain);
	}
}