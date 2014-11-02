#include "Log.hpp"
#include "Runtime.hpp"
#include "Effect.hpp"
#include "EffectTree.hpp"

#include <d3d9.h>
#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <vector>
#include <unordered_map>
#include <boost\algorithm\string.hpp>
#include <nanovg_d3d9.h>

// -----------------------------------------------------------------------------------------------------

#define SAFE_RELEASE(p)											{ if ((p)) (p)->Release(); (p) = nullptr; }

namespace ReShade
{
	namespace
	{
		class													D3D9EffectContext : public Runtime, public std::enable_shared_from_this<D3D9EffectContext>
		{
			friend struct D3D9Effect;
			friend struct D3D9Texture;
			friend struct D3D9Constant;
			friend struct D3D9Technique;
			friend class D3D9EffectCompiler;

		public:
			D3D9EffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
			~D3D9EffectContext(void);

			virtual bool										OnCreate(unsigned int width, unsigned int height) override;
			virtual void										OnDelete() override;
			virtual void										OnPresent() override;

			virtual std::unique_ptr<Effect>						CreateEffect(const EffectTree &ast, std::string &errors) const override;
			virtual void										CreateScreenshot(unsigned char *buffer, std::size_t size) const override;

		private:
			IDirect3DDevice9 *									mDevice;
			IDirect3DSwapChain9 *								mSwapChain;
			IDirect3DStateBlock9 *								mStateBlock;
			IDirect3DSurface9 *									mBackBuffer;
			IDirect3DSurface9 *									mBackBufferNotMultisampled;
		};
		struct													D3D9Effect : public Effect
		{
			friend struct D3D9Texture;
			friend struct D3D9Sampler;
			friend struct D3D9Constant;
			friend struct D3D9Technique;

			D3D9Effect(std::shared_ptr<const D3D9EffectContext> context);
			~D3D9Effect(void);

			const Texture *										GetTexture(const std::string &name) const;
			std::vector<std::string>							GetTextureNames(void) const;
			const Constant *									GetConstant(const std::string &name) const;
			std::vector<std::string>							GetConstantNames(void) const;
			const Technique *									GetTechnique(const std::string &name) const;
			std::vector<std::string>							GetTechniqueNames(void) const;

			std::shared_ptr<const D3D9EffectContext>			mEffectContext;
			std::unordered_map<std::string,std::unique_ptr<D3D9Texture>> mTextures;
			std::vector<D3D9Sampler>							mSamplers;
			std::unordered_map<std::string, std::unique_ptr<D3D9Constant>> mConstants;
			std::unordered_map<std::string, std::unique_ptr<D3D9Technique>> mTechniques;
			IDirect3DSurface9 *									mStateBlockDepthStencil, *mStateBlockRenderTarget;
			IDirect3DStateBlock9 *								mShaderResourceStateblock;
			IDirect3DSurface9 *									mDepthStencil;
			IDirect3DVertexDeclaration9 *						mVertexDeclaration;
			IDirect3DVertexBuffer9 *							mVertexBuffer;
			float *												mConstantStorage;
			UINT												mConstantRegisterCount;
		};
		struct													D3D9Texture : public Effect::Texture
		{
			D3D9Texture(D3D9Effect *effect);
			~D3D9Texture(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			void												Update(unsigned int level, const unsigned char *data, std::size_t size);
			void												UpdateFromColorBuffer(void);
			void												UpdateFromDepthBuffer(void);

			D3D9Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			IDirect3DTexture9 *									mTexture;
			IDirect3DSurface9 *									mSurface;
		};
		struct													D3D9Sampler
		{
			DWORD												mStates[12];
			D3D9Texture *										mTexture;
		};
		struct													D3D9Constant : public Effect::Constant
		{
			D3D9Constant(D3D9Effect *effect);
			~D3D9Constant(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;
			void												GetValue(unsigned char *data, std::size_t size) const;
			void												SetValue(const unsigned char *data, std::size_t size);

			D3D9Effect *										mEffect;
			Description											mDesc;
			UINT												mRegisterOffset;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
		};
		struct													D3D9Technique : public Effect::Technique
		{
			struct												Pass
			{
				IDirect3DVertexShader9 *						VS;
				IDirect3DPixelShader9 *							PS;
				IDirect3DStateBlock9 *							Stateblock;
				IDirect3DSurface9 *								RT[8];
			};

			D3D9Technique(D3D9Effect *effect);
			~D3D9Technique(void);

			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Begin(unsigned int &passes) const;
			void												End(void) const;
			void												RenderPass(unsigned int index) const;

			D3D9Effect *										mEffect;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::vector<Pass>									mPasses;
		};

		class													D3D9EffectCompiler
		{
		public:
			D3D9EffectCompiler(const EffectTree &ast) : mAST(ast), mEffect(nullptr), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentRegisterIndex(0), mCurrentStorageSize(0)
			{
			}

			bool												Traverse(D3D9Effect *effect, std::string &errors)
			{
				this->mEffect = effect;
				this->mErrors.clear();
				this->mFatal = false;
				this->mCurrentSource.clear();

				const auto &root = this->mAST[EffectTree::Root].As<EffectNodes::List>();

				for (unsigned int i = 0; i < root.Length; ++i)
				{
					this->mAST[root[i]].Accept(*this);
				}

				IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;
				device->BeginStateBlock();
			
				for (DWORD i = 0, count = static_cast<DWORD>(this->mEffect->mSamplers.size()); i < count; ++i)
				{
					const auto &sampler = this->mEffect->mSamplers[i];

					device->SetTexture(i, sampler.mTexture->mTexture);

					device->SetSamplerState(i, D3DSAMP_ADDRESSU, sampler.mStates[D3DSAMP_ADDRESSU]);
					device->SetSamplerState(i, D3DSAMP_ADDRESSV, sampler.mStates[D3DSAMP_ADDRESSV]);
					device->SetSamplerState(i, D3DSAMP_ADDRESSW, sampler.mStates[D3DSAMP_ADDRESSW]);
					device->SetSamplerState(i, D3DSAMP_MAGFILTER, sampler.mStates[D3DSAMP_MAGFILTER]);
					device->SetSamplerState(i, D3DSAMP_MINFILTER, sampler.mStates[D3DSAMP_MINFILTER]);
					device->SetSamplerState(i, D3DSAMP_MIPFILTER, sampler.mStates[D3DSAMP_MIPFILTER]);
					device->SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, sampler.mStates[D3DSAMP_MIPMAPLODBIAS]);
					device->SetSamplerState(i, D3DSAMP_MAXMIPLEVEL, sampler.mStates[D3DSAMP_MAXMIPLEVEL]);
					device->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, sampler.mStates[D3DSAMP_MAXANISOTROPY]);
					device->SetSamplerState(i, D3DSAMP_SRGBTEXTURE, sampler.mStates[D3DSAMP_SRGBTEXTURE]);
				}

				device->EndStateBlock(&this->mEffect->mShaderResourceStateblock);

				errors += this->mErrors;

				return !this->mFatal;
			}

			static inline bool									IsPowerOf2(int x)
			{
				return ((x > 0) && ((x & (x - 1)) == 0));
			}
			static D3DTEXTUREFILTERTYPE							LiteralToTextureFilter(int value)
			{
				switch (value)
				{
					default:
						return D3DTEXF_NONE;
					case EffectNodes::Literal::POINT:
						return D3DTEXF_POINT;
					case EffectNodes::Literal::LINEAR:
						return D3DTEXF_LINEAR;
					case EffectNodes::Literal::ANISOTROPIC:
						return D3DTEXF_ANISOTROPIC;
				}
			}
			static D3DTEXTUREADDRESS							LiteralToTextureAddress(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::CLAMP:
						return D3DTADDRESS_CLAMP;
					case EffectNodes::Literal::REPEAT:
						return D3DTADDRESS_WRAP;
					case EffectNodes::Literal::MIRROR:
						return D3DTADDRESS_MIRROR;
					case EffectNodes::Literal::BORDER:
						return D3DTADDRESS_BORDER;
				}
			}
			static D3DCMPFUNC									LiteralToCmpFunc(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ALWAYS:
						return D3DCMP_ALWAYS;
					case EffectNodes::Literal::NEVER:
						return D3DCMP_NEVER;
					case EffectNodes::Literal::EQUAL:
						return D3DCMP_EQUAL;
					case EffectNodes::Literal::NOTEQUAL:
						return D3DCMP_NOTEQUAL;
					case EffectNodes::Literal::LESS:
						return D3DCMP_LESS;
					case EffectNodes::Literal::LESSEQUAL:
						return D3DCMP_LESSEQUAL;
					case EffectNodes::Literal::GREATER:
						return D3DCMP_GREATER;
					case EffectNodes::Literal::GREATEREQUAL:
						return D3DCMP_GREATEREQUAL;
				}
			}
			static D3DSTENCILOP									LiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::KEEP:
						return D3DSTENCILOP_KEEP;
					case EffectNodes::Literal::ZERO:
						return D3DSTENCILOP_ZERO;
					case EffectNodes::Literal::REPLACE:
						return D3DSTENCILOP_REPLACE;
					case EffectNodes::Literal::INCR:
						return D3DSTENCILOP_INCR;
					case EffectNodes::Literal::INCRSAT:
						return D3DSTENCILOP_INCRSAT;
					case EffectNodes::Literal::DECR:
						return D3DSTENCILOP_DECR;
					case EffectNodes::Literal::DECRSAT:
						return D3DSTENCILOP_DECRSAT;
					case EffectNodes::Literal::INVERT:
						return D3DSTENCILOP_INVERT;
				}
			}
			static D3DBLEND										LiteralToBlend(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ZERO:
						return D3DBLEND_ZERO;
					case EffectNodes::Literal::ONE:
						return D3DBLEND_ONE;
					case EffectNodes::Literal::SRCCOLOR:
						return D3DBLEND_SRCCOLOR;
					case EffectNodes::Literal::SRCALPHA:
						return D3DBLEND_SRCALPHA;
					case EffectNodes::Literal::INVSRCCOLOR:
						return D3DBLEND_INVSRCCOLOR;
					case EffectNodes::Literal::INVSRCALPHA:
						return D3DBLEND_INVSRCALPHA;
					case EffectNodes::Literal::DESTCOLOR:
						return D3DBLEND_DESTCOLOR;
					case EffectNodes::Literal::DESTALPHA:
						return D3DBLEND_DESTALPHA;
					case EffectNodes::Literal::INVDESTCOLOR:
						return D3DBLEND_INVDESTCOLOR;
					case EffectNodes::Literal::INVDESTALPHA:
						return D3DBLEND_INVDESTALPHA;
				}
			}
			static D3DBLENDOP									LiteralToBlendOp(int value)
			{
				switch (value)
				{
					default:
					case EffectNodes::Literal::ADD:
						return D3DBLENDOP_ADD;
					case EffectNodes::Literal::SUBTRACT:
						return D3DBLENDOP_SUBTRACT;
					case EffectNodes::Literal::REVSUBTRACT:
						return D3DBLENDOP_REVSUBTRACT;
					case EffectNodes::Literal::MIN:
						return D3DBLENDOP_MIN;
					case EffectNodes::Literal::MAX:
						return D3DBLENDOP_MAX;
				}
			}
			static D3DFORMAT									LiteralToFormat(int value, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case EffectNodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						return D3DFMT_A8R8G8B8;
					case EffectNodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						return D3DFMT_R32F;
					case EffectNodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						return D3DFMT_A8R8G8B8;
					case EffectNodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						return D3DFMT_A8R8G8B8;  // D3DFMT_A8B8G8R8 appearently isn't supported by hardware very well
					case EffectNodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						return D3DFMT_A16B16G16R16;
					case EffectNodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						return D3DFMT_A16B16G16R16F;
					case EffectNodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						return D3DFMT_A32B32G32R32F;
					case EffectNodes::Literal::DXT1:
						format = Effect::Texture::Format::DXT1;
						return D3DFMT_DXT1;
					case EffectNodes::Literal::DXT3:
						format = Effect::Texture::Format::DXT3;
						return D3DFMT_DXT3;
					case EffectNodes::Literal::DXT5:
						format = Effect::Texture::Format::DXT5;
						return D3DFMT_DXT5;
					case EffectNodes::Literal::LATC1:
						format = Effect::Texture::Format::LATC1;
						return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
					case EffectNodes::Literal::LATC2:
						format = Effect::Texture::Format::LATC2;
						return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
					default:
						format = Effect::Texture::Format::Unknown;
						return D3DFMT_UNKNOWN;
				}
			}
			static std::string									ConvertSemantic(const std::string &semantic)
			{
				if (boost::starts_with(semantic, "SV_"))
				{
					if (semantic == "SV_VERTEXID")
					{
						return "TEXCOORD0";
					}
					else if (semantic == "SV_POSITION")
					{
						return "POSITION";
					}
					else if (boost::starts_with(semantic, "SV_TARGET"))
					{
						return "COLOR" + semantic.substr(9);
					}
					else if (semantic == "SV_DEPTH")
					{
						return "DEPTH";
					}
				}

				return semantic;
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
						res += "sampler2D";
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
						part1 = "(4294967295 - ";
						part2 = ")";
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
					case EffectNodes::Expression::BitCastUint2Float:
					case EffectNodes::Expression::BitCastFloat2Int:
					case EffectNodes::Expression::BitCastFloat2Uint:
						this->mErrors += PrintLocation(node.Location) + "Bitwise casts are not supported on legacy targets!\n";
						this->mFatal = true;
						return;
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
						part1 = "((";
						part2 = ") * exp2(";
						part3 = "))";
						break;
					case EffectNodes::Expression::RightShift:
						part1 = "floor((";
						part2 = ") / exp2(";
						part3 = "))";
						break;
					case EffectNodes::Expression::BitAnd:
					{
						const auto &right = this->mAST[node.Operands[1]];

						if (right.Is<EffectNodes::Literal>() && right.As<EffectNodes::Literal>().Type.IsIntegral())
						{
							const unsigned int value = right.As<EffectNodes::Literal>().Value.Uint[0];

							if (IsPowerOf2(value + 1))
							{
								this->mCurrentSource += "((" + std::to_string(value + 1) + ") * frac((";
								this->mAST[node.Operands[0]].Accept(*this);
								this->mCurrentSource += ") / (" + std::to_string(value + 1) + ")))";
								return;
							}
							else if (IsPowerOf2(value))
							{
								this->mCurrentSource += "((((";
								this->mAST[node.Operands[0]].Accept(*this);
								this->mCurrentSource += ") / (" + std::to_string(value) + ")) % 2) * " + std::to_string(value) + ")";
								return;
							}
						}
					}
					case EffectNodes::Expression::BitXor:
					case EffectNodes::Expression::BitOr:
						this->mErrors += PrintLocation(node.Location) + "Bitwise operations are not supported on legacy targets!\n";
						this->mFatal = true;
						return;
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
						part1 = "tex2D(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexLevel:
						part1 = "tex2Dlod(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexGather:
						part1 = "__tex2Dgather(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexBias:
						part1 = "tex2Dbias(";
						part2 = ", ";
						part3 = ")";
						break;
					case EffectNodes::Expression::TexFetch:
						part1 = "tex2D(";
						part2 = ", float2(";
						part3 = "))";
						break;
					case EffectNodes::Expression::TexSize:
						this->mErrors += PrintLocation(node.Location) + "Texture size query is not supported on legacy targets!\n";
						this->mFatal = true;
						return;
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
						part1 = "tex2D(";
						part2 = ", ";
						part3 = " + (";
						part4 = ") * _PIXEL_SIZE_.xy)";
						break;
					case EffectNodes::Expression::TexLevelOffset:
						part1 = "tex2Dlod(";
						part2 = ", ";
						part3 = " + float4((";
						part4 = ") * _PIXEL_SIZE_.xy, 0, 0))";
						break;
					case EffectNodes::Expression::TexGatherOffset:
						part1 = "__tex2Dgather(";
						part2 = ", ";
						part3 = " + (";
						part4 = ") * _PIXEL_SIZE_.xy)";
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
				std::string part1, part2, part3;

				switch (node.Operator)
				{
					case EffectNodes::Expression::None:
						part2 = " = ";
						break;
					case EffectNodes::Expression::Add:
						part2 = " += ";
						break;
					case EffectNodes::Expression::Subtract:
						part2 = " -= ";
						break;
					case EffectNodes::Expression::Multiply:
						part2 = " *= ";
						break;
					case EffectNodes::Expression::Divide:
						part2 = " /= ";
						break;
					case EffectNodes::Expression::Modulo:
						part2 = " %= ";
						break;
					case EffectNodes::Expression::LeftShift:
						part1 = "((";
						part2 = ") *= pow(2, ";
						part3 = "))";
						break;
					case EffectNodes::Expression::RightShift:
						part1 = "((";
						part2 = ") /= pow(2, ";
						part3 = "))";
						break;
					case EffectNodes::Expression::BitAnd:
					case EffectNodes::Expression::BitXor:
					case EffectNodes::Expression::BitOr:
						this->mErrors += PrintLocation(node.Location) + "Bitwise operations are not supported on legacy targets!\n";
						this->mFatal = true;
						return;
				}

				this->mCurrentSource += '(';
				this->mCurrentSource += part1;
				this->mAST[node.Left].Accept(*this);
				this->mCurrentSource += part2;
				this->mAST[node.Right].Accept(*this);
				this->mCurrentSource += part3;
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
				this->mErrors += PrintLocation(node.Location) + "Warning: Switch statements do not currently support fallthrough in Direct3D9!\n";

				this->mCurrentSource += "[unroll] do { ";
				this->mCurrentSource += PrintType(this->mAST[node.Test].As<EffectNodes::RValue>().Type);
				this->mCurrentSource += " __switch_condition = ";
				this->mAST[node.Test].Accept(*this);
				this->mCurrentSource += ";\n";

				const auto &cases = this->mAST[node.Cases].As<EffectNodes::List>();

				for (unsigned int i = 0; i < cases.Length; ++i)
				{
					this->mCurrentCaseIndex = i;

					this->mAST[cases[i]].As<EffectNodes::Case>().Accept(*this);
				}

				this->mCurrentSource += "} while (false);\n";
			}
			void												Visit(const EffectNodes::Case &node)
			{
				const auto &labels = this->mAST[node.Labels].As<EffectNodes::List>();

				if (this->mCurrentCaseIndex != 0)
				{
					this->mCurrentSource += "else ";
				}

				this->mCurrentSource += "if (";

				for (unsigned int i = 0; i < labels.Length; ++i)
				{
					const auto &label = this->mAST[labels[i]];

					if (label.Is<EffectNodes::Expression>())
					{
						this->mCurrentSource += "true";
					}
					else
					{
						this->mCurrentSource += "__switch_condition == ";
						label.As<EffectNodes::Literal>().Accept(*this);
					}

					this->mCurrentSource += " || ";
				}

				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();

				this->mCurrentSource += ")";

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

				if (!this->mCurrentInParameterBlock && node.Semantic != nullptr)
				{
					this->mCurrentSource += " : " + ConvertSemantic(node.Semantic);
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
				const UINT width = (node.Properties[EffectNodes::Variable::Width] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Width]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				const UINT height = (node.Properties[EffectNodes::Variable::Height] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::Height]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				const UINT levels = (node.Properties[EffectNodes::Variable::MipLevels] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MipLevels]].As<EffectNodes::Literal>().Value.Uint[0] : 1;

				D3DFORMAT d3dformat = D3DFMT_A8R8G8B8;
				Effect::Texture::Format format = Effect::Texture::Format::RGBA8;

				if (node.Properties[EffectNodes::Variable::Format] != 0)
				{
					d3dformat = LiteralToFormat(this->mAST[node.Properties[EffectNodes::Variable::Format]].As<EffectNodes::Literal>().Value.Uint[0], format);
				}

				std::unique_ptr<D3D9Texture> obj(new D3D9Texture(this->mEffect));
				obj->mDesc.Width = width;
				obj->mDesc.Height = height;
				obj->mDesc.Levels = levels;
				obj->mDesc.Format = format;

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateTexture(width, height, levels, d3dformat == D3DFMT_A8R8G8B8 ? D3DUSAGE_RENDERTARGET : 0, d3dformat, D3DPOOL_DEFAULT, &obj->mTexture, nullptr);

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'CreateTexture' failed!\n";
					this->mFatal = true;
					return;
				}

				obj->mTexture->GetSurfaceLevel(0, &obj->mSurface);

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

				this->mEffect->mTextures.insert(std::make_pair(node.Name, std::move(obj)));
			}
			void												VisitSampler(const EffectNodes::Variable &node)
			{
				if (node.Properties[EffectNodes::Variable::Texture] == 0)
				{
					this->mErrors = PrintLocation(node.Location) + "Sampler '" + std::string(node.Name) + "' is missing required 'Texture' property.\n";
					this->mFatal = true;
					return;
				}

				D3D9Sampler sampler;
				sampler.mStates[D3DSAMP_ADDRESSU] = (node.Properties[EffectNodes::Variable::AddressU] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressU]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTADDRESS_CLAMP;
				sampler.mStates[D3DSAMP_ADDRESSV] = (node.Properties[EffectNodes::Variable::AddressV] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressV]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTADDRESS_CLAMP;
				sampler.mStates[D3DSAMP_ADDRESSW] = (node.Properties[EffectNodes::Variable::AddressW] != 0) ? LiteralToTextureAddress(this->mAST[node.Properties[EffectNodes::Variable::AddressW]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTADDRESS_CLAMP;
				sampler.mStates[D3DSAMP_BORDERCOLOR] = 0;
				sampler.mStates[D3DSAMP_MINFILTER] = (node.Properties[EffectNodes::Variable::MinFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MinFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTEXF_LINEAR;
				sampler.mStates[D3DSAMP_MAGFILTER] = (node.Properties[EffectNodes::Variable::MagFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MagFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTEXF_LINEAR;
				sampler.mStates[D3DSAMP_MIPFILTER] = (node.Properties[EffectNodes::Variable::MipFilter] != 0) ? LiteralToTextureFilter(this->mAST[node.Properties[EffectNodes::Variable::MipFilter]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DTEXF_LINEAR;
				sampler.mStates[D3DSAMP_MIPMAPLODBIAS] = (node.Properties[EffectNodes::Variable::MipLODBias] != 0) ? static_cast<DWORD>(this->mAST[node.Properties[EffectNodes::Variable::MipLODBias]].As<EffectNodes::Literal>().Value.Float[0]) : 0;
				sampler.mStates[D3DSAMP_MAXMIPLEVEL] = (node.Properties[EffectNodes::Variable::MaxLOD] != 0) ? static_cast<DWORD>(this->mAST[node.Properties[EffectNodes::Variable::MaxLOD]].As<EffectNodes::Literal>().Value.Float[0]) : 0;
				sampler.mStates[D3DSAMP_MAXANISOTROPY] = (node.Properties[EffectNodes::Variable::MaxAnisotropy] != 0) ? this->mAST[node.Properties[EffectNodes::Variable::MaxAnisotropy]].As<EffectNodes::Literal>().Value.Uint[0] : 1;
				sampler.mStates[D3DSAMP_SRGBTEXTURE] = node.Properties[EffectNodes::Variable::SRGBTexture] != 0 && this->mAST[node.Properties[EffectNodes::Variable::SRGBTexture]].As<EffectNodes::Literal>().Value.Bool[0];

				const char *texture = this->mAST[node.Properties[EffectNodes::Variable::Texture]].As<EffectNodes::Variable>().Name;

				const auto it = this->mEffect->mTextures.find(texture);

				if (it == this->mEffect->mTextures.end())
				{
					this->mErrors = PrintLocation(node.Location) + "Texture '" + std::string(texture) + "' for sampler '" + std::string(node.Name) + "' is missing.\n";
				}

				sampler.mTexture = it->second.get();

				this->mCurrentSource += "sampler2D ";
				this->mCurrentSource += node.Name;
				this->mCurrentSource += " : register(s" + std::to_string(this->mEffect->mSamplers.size()) + ");\n";

				this->mEffect->mSamplers.push_back(sampler);
			}
			void												VisitUniform(const EffectNodes::Variable &node)
			{
				this->mCurrentSource += PrintTypeWithQualifiers(node.Type);
				this->mCurrentSource += ' ';

				if (!this->mCurrentBlockName.empty())
				{
					this->mCurrentSource += this->mCurrentBlockName + '_';
				}
				
				this->mCurrentSource += node.Name;

				if (node.Type.IsArray())
				{
					this->mCurrentSource += '[';
					this->mCurrentSource += (node.Type.ArrayLength >= 1) ? std::to_string(node.Type.ArrayLength) : "";
					this->mCurrentSource += ']';
				}

				this->mCurrentSource += ";\n";

				std::unique_ptr<D3D9Constant> obj(new D3D9Constant(this->mEffect));
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

				obj->mRegisterOffset = this->mCurrentRegisterIndex;

				const UINT registersize = static_cast<UINT>(static_cast<float>(obj->mDesc.Size) / (4 * sizeof(float)));
				const UINT alignment = (registersize - registersize) > 0;
				this->mCurrentRegisterIndex += registersize + alignment;

				if ((this->mCurrentRegisterIndex * sizeof(float) * 4) >= this->mCurrentStorageSize)
				{
					this->mEffect->mConstantStorage = static_cast<float *>(::realloc(this->mEffect->mConstantStorage, this->mCurrentStorageSize += sizeof(float) * 4 * 16));
				}

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

				if (node.HasInitializer())
				{
					std::memcpy(this->mEffect->mConstantStorage + obj->mRegisterOffset * 4, &this->mAST[node.Initializer].As<EffectNodes::Literal>().Value, obj->mDesc.Size);
				}
				else
				{
					std::memset(this->mEffect->mConstantStorage + obj->mRegisterOffset * 4, 0, obj->mDesc.Size);
				}

				this->mEffect->mConstants.insert(std::make_pair(this->mCurrentBlockName.empty() ? node.Name : (this->mCurrentBlockName + '.' + node.Name), std::move(obj)));
				this->mEffect->mConstantRegisterCount = this->mCurrentRegisterIndex + 1;
			}
			void												VisitUniformBuffer(const EffectNodes::Variable &node)
			{
				const auto &structure = this->mAST[node.Type.Definition].As<EffectNodes::Struct>();

				if (structure.HasFields())
				{
					return;
				}

				this->mCurrentBlockName = node.Name;

				const UINT previousRegisterIndex = this->mCurrentRegisterIndex;
				const auto &fields = this->mAST[structure.Fields].As<EffectNodes::List>();

				for (unsigned int i = 0; i < fields.Length; ++i)
				{
					VisitUniform(this->mAST[fields[i]].As<EffectNodes::Variable>());
				}

				this->mCurrentBlockName.clear();

				std::unique_ptr<D3D9Constant> obj(new D3D9Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fields.Length;
				obj->mDesc.Size = (this->mCurrentRegisterIndex - previousRegisterIndex) * sizeof(float) * 4;
				obj->mDesc.Type = Effect::Constant::Type::Struct;

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
				std::unique_ptr<D3D9Technique> obj(new D3D9Technique(this->mEffect));

				const auto &passes = this->mAST[node.Passes].As<EffectNodes::List>();

				obj->mPasses.reserve(passes.Length);

				this->mCurrentPasses = &obj->mPasses;

				for (unsigned int i = 0; i < passes.Length; ++i)
				{
					const auto &pass = this->mAST[passes[i]].As<EffectNodes::Pass>();

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
				D3D9Technique::Pass pass;
				ZeroMemory(&pass, sizeof(D3D9Technique::Pass));
				pass.RT[0] = this->mEffect->mEffectContext->mBackBufferNotMultisampled;

				if (node.States[EffectNodes::Pass::VertexShader] != 0)
				{
					VisitShader(this->mAST[node.States[EffectNodes::Pass::VertexShader]].As<EffectNodes::Function>(), EffectNodes::Pass::VertexShader, pass);
				}
				if (node.States[EffectNodes::Pass::PixelShader] != 0)
				{
					VisitShader(this->mAST[node.States[EffectNodes::Pass::PixelShader]].As<EffectNodes::Function>(), EffectNodes::Pass::PixelShader, pass);
				}

				IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;
				
				D3DCAPS9 caps;
				device->GetDeviceCaps(&caps);

				HRESULT hr = device->BeginStateBlock();

				if (FAILED(hr))
				{
					this->mErrors += PrintLocation(node.Location) + "'BeginStateBlock' failed!\n";
					this->mFatal = true;
					return;
				}

				device->SetVertexShader(pass.VS);
				device->SetPixelShader(pass.PS);

				device->SetRenderState(D3DRS_ZENABLE, node.States[EffectNodes::Pass::DepthEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::DepthEnable]].As<EffectNodes::Literal>().Value.Bool[0]);
				device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
				device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
				device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
				device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
				device->SetRenderState(D3DRS_SRCBLEND, (node.States[EffectNodes::Pass::SrcBlend] != 0) ? LiteralToBlend(this->mAST[node.States[EffectNodes::Pass::SrcBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DBLEND_ONE);
				device->SetRenderState(D3DRS_DESTBLEND, (node.States[EffectNodes::Pass::DestBlend] != 0) ? LiteralToBlend(this->mAST[node.States[EffectNodes::Pass::DestBlend]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DBLEND_ZERO);
				device->SetRenderState(D3DRS_ZFUNC, (node.States[EffectNodes::Pass::DepthFunc] != 0) ? LiteralToCmpFunc(this->mAST[node.States[EffectNodes::Pass::DepthFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DCMP_LESS);
				device->SetRenderState(D3DRS_ALPHAREF, 0);
				device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
				device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
				device->SetRenderState(D3DRS_FOGSTART, 0);
				device->SetRenderState(D3DRS_FOGEND, 1);
				device->SetRenderState(D3DRS_FOGDENSITY, 1);
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, node.States[EffectNodes::Pass::BlendEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::BlendEnable]].As<EffectNodes::Literal>().Value.Bool[0]);
				device->SetRenderState(D3DRS_DEPTHBIAS, 0);
				device->SetRenderState(D3DRS_STENCILENABLE, node.States[EffectNodes::Pass::StencilEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::StencilEnable]].As<EffectNodes::Literal>().Value.Bool[0]);
				device->SetRenderState(D3DRS_STENCILPASS, (node.States[EffectNodes::Pass::StencilPass] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilPass]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILFAIL, (node.States[EffectNodes::Pass::StencilFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilFail]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILZFAIL, (node.States[EffectNodes::Pass::StencilDepthFail] != 0) ? LiteralToStencilOp(this->mAST[node.States[EffectNodes::Pass::StencilDepthFail]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILFUNC, (node.States[EffectNodes::Pass::StencilFunc] != 0) ? LiteralToCmpFunc(this->mAST[node.States[EffectNodes::Pass::StencilFunc]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DCMP_ALWAYS);
				device->SetRenderState(D3DRS_STENCILREF, (node.States[EffectNodes::Pass::StencilRef] != 0) ? this->mAST[node.States[EffectNodes::Pass::StencilRef]].As<EffectNodes::Literal>().Value.Uint[0] : 0);
				device->SetRenderState(D3DRS_STENCILMASK, (node.States[EffectNodes::Pass::StencilReadMask] != 0) ? this->mAST[node.States[EffectNodes::Pass::StencilReadMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF : 0xFFFFFFFF);
				device->SetRenderState(D3DRS_STENCILWRITEMASK, (node.States[EffectNodes::Pass::StencilWriteMask] != 0) ? this->mAST[node.States[EffectNodes::Pass::StencilWriteMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xFF : 0xFFFFFFFF);
				device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
				device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
				device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
				device->SetRenderState(D3DRS_COLORWRITEENABLE, (node.States[EffectNodes::Pass::ColorWriteMask] != 0) ? this->mAST[node.States[EffectNodes::Pass::ColorWriteMask]].As<EffectNodes::Literal>().Value.Uint[0] & 0xF : 0xF);
				device->SetRenderState(D3DRS_BLENDOP, (node.States[EffectNodes::Pass::BlendOp] != 0) ? LiteralToBlendOp(this->mAST[node.States[EffectNodes::Pass::BlendOp]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DBLENDOP_ADD);
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
				device->SetRenderState(D3DRS_SRGBWRITEENABLE, node.States[EffectNodes::Pass::SRGBWriteEnable] != 0 && this->mAST[node.States[EffectNodes::Pass::SRGBWriteEnable]].As<EffectNodes::Literal>().Value.Bool[0]);
				device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
				device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
				device->SetRenderState(D3DRS_BLENDOPALPHA, (node.States[EffectNodes::Pass::BlendOpAlpha] != 0) ? LiteralToBlendOp(this->mAST[node.States[EffectNodes::Pass::BlendOpAlpha]].As<EffectNodes::Literal>().Value.Uint[0]) : D3DBLENDOP_ADD);
				device->SetRenderState(D3DRS_FOGENABLE, FALSE );
				device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				device->SetRenderState(D3DRS_LIGHTING, FALSE);

				device->EndStateBlock(&pass.Stateblock);

				for (unsigned int i = 0; i < 8; ++i)
				{
					if (node.States[EffectNodes::Pass::RenderTarget0 + i] != 0)
					{
						if (i > caps.NumSimultaneousRTs)
						{
							this->mErrors += PrintLocation(node.Location) + "Device only supports " + std::to_string(caps.NumSimultaneousRTs) + " simultaneous render targets, but more are in use.\n";
							break;
						}

						const auto &variable = this->mAST[node.States[EffectNodes::Pass::RenderTarget0 + i]].As<EffectNodes::Variable>();
	
						const auto it = this->mEffect->mTextures.find(variable.Name);

						if (it == this->mEffect->mTextures.end())
						{
							this->mFatal = true;
							return;
						}
						
						D3D9Texture *texture = it->second.get();

						pass.RT[i] = texture->mSurface;
					}
				}

				assert(this->mCurrentPasses != nullptr);

				this->mCurrentPasses->push_back(std::move(pass));
			}
			void												VisitShader(const EffectNodes::Function &node, unsigned int type, D3D9Technique::Pass &pass)
			{
				const char *profile = nullptr;

				switch (type)
				{
					default:
						return;
					case EffectNodes::Pass::VertexShader:
						profile = "vs_3_0";
						break;
					case EffectNodes::Pass::PixelShader:
						profile = "ps_3_0";
						break;
				}

				UINT flags = 0;
#ifdef _DEBUG
				flags |= D3DCOMPILE_DEBUG;
#endif

				std::string source =
					"uniform float4 _PIXEL_SIZE_ : register(c223);\n"
					"float4 __tex2Dgather(sampler2D s, float2 c) { return float4(tex2D(s, c + float2(0, 1) * _PIXEL_SIZE_.xy).r, tex2D(s, c + float2(1, 1) * _PIXEL_SIZE_.xy).r, tex2D(s, c + float2(1, 0) * _PIXEL_SIZE_.xy).r, tex2D(s, c).r); }\n";

				source += this->mCurrentSource;

				std::string positionVariable, initialization;
				EffectNodes::Type returnType = node.ReturnType;

				if (node.ReturnType.IsStruct() && this->mAST[node.ReturnType.Definition].As<EffectNodes::Struct>().HasFields())
				{
					const auto &fields = this->mAST[this->mAST[node.ReturnType.Definition].As<EffectNodes::Struct>().Fields].As<EffectNodes::List>();

					for (unsigned int k = 0; k < fields.Length; ++k)
					{
						const auto &field = this->mAST[fields[k]].As<EffectNodes::Variable>();

						if (field.Semantic == nullptr)
						{
							continue;
						}
						
						if (boost::equals(field.Semantic, "SV_POSITION"))
						{
							positionVariable = "_return.";
							positionVariable += field.Name;
							break;
						}
						else if (boost::starts_with(field.Semantic, "SV_TARGET") && field.Type.Rows != 4)
						{
							this->mErrors += PrintLocation(node.Location) + "'SV_Target' must be a four-component vector when used inside structs on legacy targets.";
							this->mFatal = true;
							return;
						}
					}
				}
				else if (node.ReturnSemantic != nullptr)
				{
					if (boost::equals(node.ReturnSemantic, "SV_POSITION"))
					{
						positionVariable = "_return";
					}
					else if (boost::starts_with(node.ReturnSemantic, "SV_TARGET"))
					{
						returnType.Rows = 4;
					}
				}

				source += PrintType(returnType) + ' ' + "__main" + '(';
				
				if (node.HasParameters())
				{
					const auto &parameters = this->mAST[node.Parameters].As<EffectNodes::List>();

					for (unsigned int i = 0; i < parameters.Length; ++i)
					{
						const auto &parameter = this->mAST[parameters[i]].As<EffectNodes::Variable>();
						EffectNodes::Type parameterType = parameter.Type;

						if (parameter.Type.HasQualifier(EffectNodes::Type::Out))
						{
							if (parameter.Type.IsStruct() && this->mAST[parameter.Type.Definition].As<EffectNodes::Struct>().HasFields())
							{
								const auto &fields = this->mAST[this->mAST[parameter.Type.Definition].As<EffectNodes::Struct>().Fields].As<EffectNodes::List>();

								for (unsigned int k = 0; k < fields.Length; ++k)
								{
									const auto &field = this->mAST[fields[k]].As<EffectNodes::Variable>();

									if (field.Semantic == nullptr)
									{
										continue;
									}
									
									if (boost::equals(field.Semantic, "SV_POSITION"))
									{
										positionVariable = parameter.Name;
										positionVariable += '.';
										positionVariable += field.Name;
										break;
									}
									else if (boost::starts_with(field.Semantic, "SV_TARGET") && field.Type.Rows != 4)
									{
										this->mErrors += PrintLocation(node.Location) + "'SV_Target' must be a four-component vector when used inside structs on legacy targets.";
										this->mFatal = true;
										return;
									}
								}
							}
							else if (parameter.Semantic != nullptr)
							{
								if (boost::equals(parameter.Semantic, "SV_POSITION"))
								{
									positionVariable = parameter.Name;
								}
								else if (boost::starts_with(parameter.Semantic, "SV_TARGET"))
								{
									parameterType.Rows = 4;

									initialization += parameter.Name;
									initialization += " = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
								}
							}
						}

						source += PrintTypeWithQualifiers(parameterType) + ' ' + parameter.Name;

						if (parameterType.IsArray())
						{
							source += '[';
							source += (parameterType.ArrayLength >= 1) ? std::to_string(parameterType.ArrayLength) : "";
							source += ']';
						}

						if (parameter.Semantic != nullptr)
						{
							source += " : " + ConvertSemantic(parameter.Semantic);
						}

						source += ", ";
					}

					source.pop_back();
					source.pop_back();
				}

				source += ')';

				if (node.ReturnSemantic != nullptr)
				{
					source += " : " + ConvertSemantic(node.ReturnSemantic);
				}

				source += "\n{\n";
				source += initialization;

				if (!node.ReturnType.IsVoid())
				{
					source += PrintType(returnType) + " _return = ";
				}

				if (node.ReturnType.Rows != returnType.Rows)
				{
					source += "float4(";
				}

				source += node.Name;
				source += '(';

				if (node.HasParameters())
				{
					const auto &parameters = this->mAST[node.Parameters].As<EffectNodes::List>();
				
					for (unsigned int i = 0; i < parameters.Length; ++i)
					{
						const auto &parameter = this->mAST[parameters[i]].As<EffectNodes::Variable>();

						source += parameter.Name;

						if (parameter.Semantic != nullptr && boost::starts_with(parameter.Semantic, "SV_TARGET"))
						{
							source += '.';

							const char swizzle[] = { 'x', 'y', 'z', 'w' };

							for (unsigned int i = 0; i < parameter.Type.Rows; ++i)
							{
								source += swizzle[i];
							}
						}

						source += ", ";
					}

					source.pop_back();
					source.pop_back();
				}

				source += ')';

				if (node.ReturnType.Rows != returnType.Rows)
				{
					for (unsigned int i = 0; i < 4 - node.ReturnType.Rows; ++i)
					{
						source += ", 0.0f";
					}

					source += ')';
				}

				source += ";\n";
				
				if (type == EffectNodes::Pass::VertexShader)
				{
					source += positionVariable + ".xy += _PIXEL_SIZE_.zw * " + positionVariable + ".ww;\n";
				}

				if (!node.ReturnType.IsVoid())
				{
					source += "return _return;\n";
				}

				source += "}\n";

				LOG(TRACE) << "> Compiling shader '" << node.Name << "':\n\n" << source.c_str() << "\n";

				ID3DBlob *compiled = nullptr, *errors = nullptr;

				HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, "__main", profile, flags, 0, &compiled, &errors);

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
						hr = this->mEffect->mEffectContext->mDevice->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.VS);
						break;
					case EffectNodes::Pass::PixelShader:
						hr = this->mEffect->mEffectContext->mDevice->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.PS);
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
			D3D9Effect *										mEffect;
			std::string											mCurrentSource;
			std::string											mErrors;
			bool												mFatal;
			std::string											mCurrentBlockName;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
			unsigned int										mCurrentCaseIndex, mCurrentRegisterIndex, mCurrentStorageSize;
			std::unordered_map<std::string, Effect::Annotation> *mCurrentAnnotations;
			std::vector<D3D9Technique::Pass> *					mCurrentPasses;
		};

		// -----------------------------------------------------------------------------------------------------

		D3D9EffectContext::D3D9EffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : mDevice(device), mSwapChain(swapchain), mStateBlock(nullptr), mBackBuffer(nullptr), mBackBufferNotMultisampled(nullptr)
		{
			this->mDevice->AddRef();
			this->mSwapChain->AddRef();

			IDirect3D9 *d3d = nullptr;
			this->mDevice->GetDirect3D(&d3d);

			D3DDEVICE_CREATION_PARAMETERS params;
			this->mDevice->GetCreationParameters(&params);

			D3DADAPTER_IDENTIFIER9 identifier;
			d3d->GetAdapterIdentifier(params.AdapterOrdinal, 0, &identifier);
			d3d->Release();
			
			this->mVendorId = identifier.VendorId;
			this->mDeviceId = identifier.DeviceId;
			this->mRendererId = 0xD3D9;

			this->mDevice->CreateStateBlock(D3DSBT_ALL, &this->mStateBlock);
		}
		D3D9EffectContext::~D3D9EffectContext(void)
		{
			SAFE_RELEASE(this->mStateBlock);

			this->mDevice->Release();
			this->mSwapChain->Release();
		}

		bool													D3D9EffectContext::OnCreate(unsigned int width, unsigned int height)
		{
			this->mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &this->mBackBuffer);

			D3DSURFACE_DESC bbdesc;
			this->mBackBuffer->GetDesc(&bbdesc);

			if (bbdesc.MultiSampleType != D3DMULTISAMPLE_NONE)
			{
				this->mDevice->CreateRenderTarget(bbdesc.Width, bbdesc.Height, bbdesc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &this->mBackBufferNotMultisampled, nullptr);
			}
			else
			{
				this->mBackBufferNotMultisampled = this->mBackBuffer;
			}

			this->mNVG = nvgCreateD3D9(this->mDevice, 0);

			return Runtime::OnCreate(width, height);
		}
		void													D3D9EffectContext::OnDelete()
		{
			Runtime::OnDelete();

			nvgDeleteD3D9(this->mNVG);
			this->mNVG = nullptr;

			if (this->mBackBufferNotMultisampled != this->mBackBuffer)
			{
				this->mBackBufferNotMultisampled->Release();
			}

			SAFE_RELEASE(this->mBackBuffer);
		}
		void													D3D9EffectContext::OnPresent()
		{
			this->mDevice->BeginScene();
			this->mStateBlock->Capture();

			if (this->mBackBufferNotMultisampled != this->mBackBuffer)
			{
				this->mDevice->StretchRect(this->mBackBuffer, nullptr, this->mBackBufferNotMultisampled, nullptr, D3DTEXF_NONE);
			}

			this->mDevice->SetRenderTarget(0, this->mBackBufferNotMultisampled);

			Runtime::OnPresent();

			if (this->mBackBufferNotMultisampled != this->mBackBuffer)
			{
				this->mDevice->StretchRect(this->mBackBufferNotMultisampled, nullptr, this->mBackBuffer, nullptr, D3DTEXF_NONE);
			}

			this->mStateBlock->Apply();
			this->mDevice->EndScene();
		}

		std::unique_ptr<Effect>									D3D9EffectContext::CreateEffect(const EffectTree &ast, std::string &errors) const
		{
			D3D9Effect *effect = new D3D9Effect(shared_from_this());
			
			D3D9EffectCompiler visitor(ast);
		
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
		void													D3D9EffectContext::CreateScreenshot(unsigned char *buffer, std::size_t size) const
		{
			IDirect3DSurface9 *backbuffer = nullptr;
			IDirect3DSurface9 *surfaceSystem = nullptr;

			HRESULT hr = this->mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);

			if (FAILED(hr))
			{
				return;
			}

			D3DSURFACE_DESC desc;
			backbuffer->GetDesc(&desc);

			if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_X8B8G8R8 && desc.Format != D3DFMT_A8R8G8B8 && desc.Format != D3DFMT_A8B8G8R8)
			{
				backbuffer->Release();
				return;
			}

			hr = this->mDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &surfaceSystem, nullptr);

			if (FAILED(hr))
			{
				backbuffer->Release();
				return;
			}

			if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
			{
				IDirect3DSurface9 *surfaceResolve = nullptr;

				hr = this->mDevice->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &surfaceResolve, nullptr);				

				if (SUCCEEDED(hr))
				{
					hr = this->mDevice->StretchRect(backbuffer, nullptr, surfaceResolve, nullptr, D3DTEXF_NONE);

					if (SUCCEEDED(hr))
					{
						hr = this->mDevice->GetRenderTargetData(surfaceResolve, surfaceSystem);
					}

					surfaceResolve->Release();
				}
			}
			else
			{
				hr = this->mDevice->GetRenderTargetData(backbuffer, surfaceSystem);
			}

			backbuffer->Release();

			if (FAILED(hr))
			{
				surfaceSystem->Release();
				return;
			}

			D3DLOCKED_RECT mapped;
			ZeroMemory(&mapped, sizeof(D3DLOCKED_RECT));

			// Lock surface
			hr = surfaceSystem->LockRect(&mapped, nullptr, D3DLOCK_READONLY);

			if (FAILED(hr))
			{
				surfaceSystem->Release();
				return;
			}

			const UINT pitch = desc.Width * 4;

			BYTE *pBuffer = buffer;
			BYTE *pMapped = static_cast<BYTE *>(mapped.pBits);

			for (UINT y = 0; y < desc.Height; ++y)
			{
				CopyMemory(pBuffer, pMapped, std::min(pitch, static_cast<UINT>(mapped.Pitch)));

				for (UINT x = 0; x < pitch; x += 4)
				{
					pBuffer[x + 3] = 0xFF;

					if (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8)
					{
						std::swap(pBuffer[x + 0], pBuffer[x + 2]);
					}
				}
							
				pBuffer += pitch;
				pMapped += mapped.Pitch;
			}

			surfaceSystem->UnlockRect();

			surfaceSystem->Release();
		}

		D3D9Effect::D3D9Effect(std::shared_ptr<const D3D9EffectContext> context) : mEffectContext(context), mDepthStencil(nullptr)
		{
			HRESULT hr;

			D3DSURFACE_DESC desc;
			this->mEffectContext->mBackBuffer->GetDesc(&desc);
			
			hr = context->mDevice->CreateDepthStencilSurface(desc.Width, desc.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &this->mDepthStencil, nullptr);

			assert(SUCCEEDED(hr));

			this->mConstantRegisterCount = 0;
			this->mConstantStorage = nullptr;
			this->mShaderResourceStateblock = nullptr;

			D3DVERTEXELEMENT9 declaration[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};

			hr = context->mDevice->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &this->mVertexBuffer, nullptr);

			if (FAILED(hr))
			{
				assert(false);
				return;
			}

			hr = context->mDevice->CreateVertexDeclaration(declaration, &this->mVertexDeclaration);

			if (FAILED(hr))
			{
				assert(false);

				SAFE_RELEASE(this->mVertexBuffer);
				return;
			}

			float *data;

			hr = this->mVertexBuffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

			if (FAILED(hr))
			{
				assert(false);

				SAFE_RELEASE(this->mVertexBuffer);
				SAFE_RELEASE(this->mVertexDeclaration);
				return;
			}

			for (UINT i = 0; i < 3; ++i)
			{
				data[i] = static_cast<float>(i);
			}

			this->mVertexBuffer->Unlock();
		}
		D3D9Effect::~D3D9Effect(void)
		{
			SAFE_RELEASE(this->mVertexDeclaration);
			SAFE_RELEASE(this->mVertexBuffer);
			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mShaderResourceStateblock);
		}

		const Effect::Texture *									D3D9Effect::GetTexture(const std::string &name) const
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D9Effect::GetTextureNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTextures.size());

			for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		const Effect::Constant *								D3D9Effect::GetConstant(const std::string &name) const
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D9Effect::GetConstantNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mConstants.size());

			for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		const Effect::Technique *								D3D9Effect::GetTechnique(const std::string &name) const
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D9Effect::GetTechniqueNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTechniques.size());

			for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}

		D3D9Texture::D3D9Texture(D3D9Effect *effect) : mEffect(effect), mTexture(nullptr), mSurface(nullptr)
		{
		}
		D3D9Texture::~D3D9Texture(void)
		{
			SAFE_RELEASE(this->mTexture);
			SAFE_RELEASE(this->mSurface);
		}

		const Effect::Texture::Description						D3D9Texture::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D9Texture::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		void													D3D9Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
		{
			assert(data != nullptr || size == 0);

			if (size == 0) return;

			IDirect3DTexture9 *system;
			IDirect3DSurface9 *textureSurface, *systemSurface;
			this->mTexture->GetSurfaceLevel(level, &textureSurface);

			D3DSURFACE_DESC desc;
			textureSurface->GetDesc(&desc);
		
			HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &system, nullptr);

			if (FAILED(hr))
			{
				textureSurface->Release();
				return;
			}

			system->GetSurfaceLevel(0, &systemSurface);
			system->Release();
		
			D3DLOCKED_RECT mapped;
			hr = systemSurface->LockRect(&mapped, nullptr, 0);

			if (FAILED(hr))
			{
				systemSurface->Release();
				textureSurface->Release();
				return;
			}

			size = std::min<size_t>(size, mapped.Pitch * this->mDesc.Height);
			unsigned char *p = static_cast<unsigned char *>(mapped.pBits);

			switch (this->mDesc.Format)
			{
				case Format::R8:
					for (std::size_t i = 0; i < size; i += 1)
					{
						*p++ = 0;
						*p++ = 0;
						*p++ = data[i];
						*p++ = 0;
					}
					break;
				case Format::RG8:
					for (std::size_t i = 0; i < size; i += 2)
					{
						*p++ = 0;
						*p++ = data[i + 1];
						*p++ = data[i + 0];
						*p++ = 0;
					}
					break;
				case Format::RGBA8:
					for (std::size_t i = 0; i < size; i += 4)
					{
						*p++ = data[i + 2];
						*p++ = data[i + 1];
						*p++ = data[i + 0];
						*p++ = data[i + 3];
					}
					break;
				default:
					::memcpy(p, data, size);
					break;
			}

			systemSurface->UnlockRect();

			hr = this->mEffect->mEffectContext->mDevice->UpdateSurface(systemSurface, nullptr, textureSurface, nullptr);

			systemSurface->Release();
			textureSurface->Release();
		}
		void													D3D9Texture::UpdateFromColorBuffer(void)
		{
			this->mEffect->mEffectContext->mDevice->StretchRect(this->mEffect->mEffectContext->mBackBufferNotMultisampled, nullptr, this->mSurface, nullptr, D3DTEXF_NONE);
		}
		void													D3D9Texture::UpdateFromDepthBuffer(void)
		{
		}

		D3D9Constant::D3D9Constant(D3D9Effect *effect) : mEffect(effect), mRegisterOffset(0)
		{
		}
		D3D9Constant::~D3D9Constant(void)
		{
		}

		const Effect::Constant::Description						D3D9Constant::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D9Constant::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}
		void													D3D9Constant::GetValue(unsigned char *data, std::size_t size) const
		{
			std::memcpy(data, this->mEffect->mConstantStorage + (this->mRegisterOffset * 4), size);
		}
		void													D3D9Constant::SetValue(const unsigned char *data, std::size_t size)
		{
			std::memcpy(this->mEffect->mConstantStorage + (this->mRegisterOffset * 4), data, size);
		}

		D3D9Technique::D3D9Technique(D3D9Effect *effect) : mEffect(effect)
		{
		}
		D3D9Technique::~D3D9Technique(void)
		{
			for (Pass &pass : this->mPasses)
			{
				SAFE_RELEASE(pass.Stateblock);
				SAFE_RELEASE(pass.VS);
				SAFE_RELEASE(pass.PS);
			}
		}

		const Effect::Annotation								D3D9Technique::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													D3D9Technique::Begin(unsigned int &passes) const
		{
			IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;

			passes = static_cast<unsigned int>(this->mPasses.size());

			if (passes == 0)
			{
				return false;
			}

			device->GetRenderTarget(0, &this->mEffect->mStateBlockRenderTarget);
			device->GetDepthStencilSurface(&this->mEffect->mStateBlockDepthStencil);

			device->SetDepthStencilSurface(this->mEffect->mDepthStencil);
			device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0x0);

			device->SetVertexDeclaration(this->mEffect->mVertexDeclaration);
			device->SetStreamSource(0, this->mEffect->mVertexBuffer, 0, sizeof(float));

			this->mEffect->mShaderResourceStateblock->Apply();

			device->SetVertexShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);
			device->SetPixelShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);

			D3DVIEWPORT9 viewport;
			device->GetViewport(&viewport);
			const float pixel_size[4] = { +1.0f / viewport.Width, +1.0f / viewport.Height, -1.0f / viewport.Width, +1.0f / viewport.Height };
			device->SetVertexShaderConstantF(223, pixel_size, 1);
			device->SetPixelShaderConstantF(223, pixel_size, 1);

			return true;
		}
		void													D3D9Technique::End(void) const
		{
			IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;

			this->mEffect->mEffectContext->mStateBlock->Apply();

			device->SetDepthStencilSurface(this->mEffect->mStateBlockDepthStencil);
			SAFE_RELEASE(this->mEffect->mStateBlockDepthStencil);
			device->SetRenderTarget(0, this->mEffect->mStateBlockRenderTarget);
			SAFE_RELEASE(this->mEffect->mStateBlockRenderTarget);
		}
		void													D3D9Technique::RenderPass(unsigned int index) const
		{
			IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;
			const Pass &pass = this->mPasses[index];

			pass.Stateblock->Apply();

			for (DWORD i = 0; i < 4; ++i)
			{
				device->SetRenderTarget(i, pass.RT[i]);
			}

			device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 0.0f, 0x0);

			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
		}
	}

	// -----------------------------------------------------------------------------------------------------

	std::shared_ptr<Runtime>									CreateEffectRuntime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain)
	{
		assert(device != nullptr && swapchain != nullptr);

		return std::make_shared<D3D9EffectContext>(device, swapchain);
	}
}