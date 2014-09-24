#include "Log.hpp"
#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectContext.hpp"

#include <d3d9.h>
#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <boost\algorithm\string.hpp>

// -----------------------------------------------------------------------------------------------------

#define SAFE_RELEASE(p)											{ if ((p)) (p)->Release(); (p) = nullptr; }

namespace ReShade
{
	namespace
	{
		class													D3D9EffectContext : public EffectContext, public std::enable_shared_from_this<D3D9EffectContext>
		{
			friend struct D3D9Effect;
			friend struct D3D9Texture;
			friend struct D3D9Constant;
			friend struct D3D9Technique;
			friend class ASTVisitor;

		public:
			D3D9EffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain);
			~D3D9EffectContext(void);

			void												GetDimension(unsigned int &width, unsigned int &height) const;

			std::unique_ptr<Effect>								Compile(const EffectTree &ast, std::string &errors);

		private:
			IDirect3DDevice9 *									mDevice;
			IDirect3DSwapChain9 *								mSwapChain;
		};
		struct													D3D9Effect : public Effect
		{
			friend struct D3D9Texture;
			friend struct D3D9Sampler;
			friend struct D3D9Constant;
			friend struct D3D9Technique;

			D3D9Effect(std::shared_ptr<D3D9EffectContext> context);
			~D3D9Effect(void);

			Texture *											GetTexture(const std::string &name);
			const Texture *										GetTexture(const std::string &name) const;
			std::vector<std::string>							GetTextureNames(void) const;
			Constant *											GetConstant(const std::string &name);
			const Constant *									GetConstant(const std::string &name) const;
			std::vector<std::string>							GetConstantNames(void) const;
			Technique *											GetTechnique(const std::string &name);
			const Technique *									GetTechnique(const std::string &name) const;
			std::vector<std::string>							GetTechniqueNames(void) const;

			std::shared_ptr<D3D9EffectContext>					mEffectContext;
			std::unordered_map<std::string,std::unique_ptr<D3D9Texture>> mTextures;
			std::vector<D3D9Sampler>							mSamplers;
			std::unordered_map<std::string, std::unique_ptr<D3D9Constant>> mConstants;
			std::unordered_map<std::string, std::unique_ptr<D3D9Technique>> mTechniques;
			IDirect3DSurface9 *									mBackBuffer;
			IDirect3DStateBlock9 *								mStateBlock;
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

			bool												Resize(const Description &desc);
			void												Update(unsigned int level, const unsigned char *data, std::size_t size);
			void												UpdateFromColorBuffer(void);
			void												UpdateFromDepthBuffer(void);

			D3D9Effect *										mEffect;
			Description											mDesc;
			UINT												mDimension;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			union
			{
				IDirect3DTexture9 *									mTexture;
				IDirect3DVolumeTexture9 *							mVolumeTexture;
			};
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
				IDirect3DSurface9 *								RT[4];
			};

			D3D9Technique(D3D9Effect *effect);
			~D3D9Technique(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Begin(unsigned int &passes) const;
			void												End(void) const;
			void												RenderPass(unsigned int index) const;

			D3D9Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::vector<Pass>									mPasses;
		};

		inline bool												IsPowerOf2(int x)
		{
			return ((x > 0) && ((x & (x - 1)) == 0));
		}

		class													ASTVisitor
		{
		public:
			ASTVisitor(const EffectTree &ast, std::string &errors) : mAST(ast), mEffect(nullptr), mErrors(errors), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentRegisterIndex(0), mCurrentStorageSize(0), mCurrentPositionVariableType(0)
			{
			}

			bool												Traverse(D3D9Effect *effect)
			{
				this->mEffect = effect;
				this->mFatal = false;
				this->mCurrentSource.clear();

				const auto &root = this->mAST[EffectTree::Root].As<Nodes::Aggregate>();

				for (size_t i = 0; i < root.Length; ++i)
				{
					Visit(this->mAST[root.Find(this->mAST, i)]);
				}

				IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;
				device->BeginStateBlock();
				device->SetVertexShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);
				device->SetPixelShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);
			
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

				return !this->mFatal;
			}

		private:
			void												operator =(const ASTVisitor &);

			static D3DTEXTUREFILTERTYPE							ConvertLiteralToTextureFilter(int value)
			{
				switch (value)
				{
					default:
						return D3DTEXF_NONE;
					case Nodes::Literal::POINT:
						return D3DTEXF_POINT;
					case Nodes::Literal::LINEAR:
						return D3DTEXF_LINEAR;
					case Nodes::Literal::ANISOTROPIC:
						return D3DTEXF_ANISOTROPIC;
				}
			}
			static D3DTEXTUREADDRESS							ConvertLiteralToTextureAddress(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::CLAMP:
						return D3DTADDRESS_CLAMP;
					case Nodes::Literal::REPEAT:
						return D3DTADDRESS_WRAP;
					case Nodes::Literal::MIRROR:
						return D3DTADDRESS_MIRROR;
					case Nodes::Literal::BORDER:
						return D3DTADDRESS_BORDER;
				}
			}
			static D3DCULL										ConvertLiteralToCullMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::NONE:
						return D3DCULL_NONE;
					case Nodes::Literal::FRONT:
						return D3DCULL_CW;
					case Nodes::Literal::BACK:
						return D3DCULL_CCW;
				}
			}
			static D3DFILLMODE									ConvertLiteralToFillMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::SOLID:
						return D3DFILL_SOLID;
					case Nodes::Literal::WIREFRAME:
						return D3DFILL_WIREFRAME;
				}
			}
			static D3DCMPFUNC									ConvertLiteralToCompFunc(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ALWAYS:
						return D3DCMP_ALWAYS;
					case Nodes::Literal::NEVER:
						return D3DCMP_NEVER;
					case Nodes::Literal::EQUAL:
						return D3DCMP_EQUAL;
					case Nodes::Literal::NOTEQUAL:
						return D3DCMP_NOTEQUAL;
					case Nodes::Literal::LESS:
						return D3DCMP_LESS;
					case Nodes::Literal::LESSEQUAL:
						return D3DCMP_LESSEQUAL;
					case Nodes::Literal::GREATER:
						return D3DCMP_GREATER;
					case Nodes::Literal::GREATEREQUAL:
						return D3DCMP_GREATEREQUAL;
				}
			}
			static D3DSTENCILOP									ConvertLiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::KEEP:
						return D3DSTENCILOP_KEEP;
					case Nodes::Literal::ZERO:
						return D3DSTENCILOP_ZERO;
					case Nodes::Literal::REPLACE:
						return D3DSTENCILOP_REPLACE;
					case Nodes::Literal::INCR:
						return D3DSTENCILOP_INCR;
					case Nodes::Literal::INCRSAT:
						return D3DSTENCILOP_INCRSAT;
					case Nodes::Literal::DECR:
						return D3DSTENCILOP_DECR;
					case Nodes::Literal::DECRSAT:
						return D3DSTENCILOP_DECRSAT;
					case Nodes::Literal::INVERT:
						return D3DSTENCILOP_INVERT;
				}
			}
			static D3DBLEND										ConvertLiteralToBlend(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ZERO:
						return D3DBLEND_ZERO;
					case Nodes::Literal::ONE:
						return D3DBLEND_ONE;
					case Nodes::Literal::SRCCOLOR:
						return D3DBLEND_SRCCOLOR;
					case Nodes::Literal::SRCALPHA:
						return D3DBLEND_SRCALPHA;
					case Nodes::Literal::INVSRCCOLOR:
						return D3DBLEND_INVSRCCOLOR;
					case Nodes::Literal::INVSRCALPHA:
						return D3DBLEND_INVSRCALPHA;
					case Nodes::Literal::DESTCOLOR:
						return D3DBLEND_DESTCOLOR;
					case Nodes::Literal::DESTALPHA:
						return D3DBLEND_DESTALPHA;
					case Nodes::Literal::INVDESTCOLOR:
						return D3DBLEND_INVDESTCOLOR;
					case Nodes::Literal::INVDESTALPHA:
						return D3DBLEND_INVDESTALPHA;
				}
			}
			static D3DBLENDOP									ConvertLiteralToBlendOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ADD:
						return D3DBLENDOP_ADD;
					case Nodes::Literal::SUBSTRACT:
						return D3DBLENDOP_SUBTRACT;
					case Nodes::Literal::REVSUBSTRACT:
						return D3DBLENDOP_REVSUBTRACT;
					case Nodes::Literal::MIN:
						return D3DBLENDOP_MIN;
					case Nodes::Literal::MAX:
						return D3DBLENDOP_MAX;
				}
			}
			static D3DFORMAT									ConvertLiteralToFormat(int value, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case Nodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						return D3DFMT_L8;
					case Nodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						return D3DFMT_R32F;
					case Nodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						return D3DFMT_A8L8;
					case Nodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						return D3DFMT_A8R8G8B8;
					case Nodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						return D3DFMT_A16B16G16R16;
					case Nodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						return D3DFMT_A16B16G16R16F;
					case Nodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						return D3DFMT_A32B32G32R32F;
					case Nodes::Literal::DXT1:
						format = Effect::Texture::Format::DXT1;
						return D3DFMT_DXT1;
					case Nodes::Literal::DXT3:
						format = Effect::Texture::Format::DXT3;
						return D3DFMT_DXT3;
					case Nodes::Literal::DXT5:
						format = Effect::Texture::Format::DXT5;
						return D3DFMT_DXT5;
					case Nodes::Literal::LATC1:
						format = Effect::Texture::Format::LATC1;
						return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
					case Nodes::Literal::LATC2:
						format = Effect::Texture::Format::LATC2;
						return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
					default:
						format = Effect::Texture::Format::Unknown;
						return D3DFMT_UNKNOWN;
				}
			}
			static std::string									ConvertSemantic(const std::string &semantic)
			{
				if (boost::istarts_with(semantic, "SV_"))
				{
					if (boost::iequals(semantic, "SV_Position"))
					{
						return "POSITION";
					}
					else if (boost::istarts_with(semantic, "SV_Target"))
					{
						return "COLOR" + semantic.substr(9);
					}
					else if (boost::iequals(semantic, "SV_Depth"))
					{
						return "DEPTH";
					}
				}

				return semantic;
			}
		
			void												Visit(const EffectTree::Node &node)
			{
				switch (node.Type)
				{
					case Nodes::Aggregate::NodeType:
						VisitAggregate(node.As<Nodes::Aggregate>());
						break;
					case Nodes::ExpressionSequence::NodeType:
						VisitExpressionSequence(node.As<Nodes::ExpressionSequence>());
						break;
					case Nodes::Literal::NodeType:
						VisitLiteral(node.As<Nodes::Literal>());
						break;
					case Nodes::Reference::NodeType:
						VisitReference(node.As<Nodes::Reference>());
						break;
					case Nodes::UnaryExpression::NodeType:
						VisitUnaryExpression(node.As<Nodes::UnaryExpression>());
						break;
					case Nodes::BinaryExpression::NodeType:
						VisitBinaryExpression(node.As<Nodes::BinaryExpression>());
						break;
					case Nodes::Assignment::NodeType:
						VisitAssignmentExpression(node.As<Nodes::Assignment>());
						break;
					case Nodes::Conditional::NodeType:
						VisitConditionalExpression(node.As<Nodes::Conditional>());
						break;
					case Nodes::Call::NodeType:
						VisitCallExpression(node.As<Nodes::Call>());
						break;
					case Nodes::Constructor::NodeType:
						VisitConstructorExpression(node.As<Nodes::Constructor>());
						break;
					case Nodes::Field::NodeType:
						VisitFieldExpression(node.As<Nodes::Field>());
						break;
					case Nodes::Swizzle::NodeType:
						VisitSwizzleExpression(node.As<Nodes::Swizzle>());
						break;
					case Nodes::StatementBlock::NodeType:
						VisitStatementBlock(node.As<Nodes::StatementBlock>());
						break;
					case Nodes::ExpressionStatement::NodeType:
						VisitExpressionStatement(node.As<Nodes::ExpressionStatement>());
						break;
					case Nodes::DeclarationStatement::NodeType:
						VisitDeclarationStatement(node.As<Nodes::DeclarationStatement>());
						break;
					case Nodes::Selection::NodeType:
						VisitSelectionStatement(node.As<Nodes::Selection>());
						break;
					case Nodes::Iteration::NodeType:
						VisitIterationStatement(node.As<Nodes::Iteration>());
						break;
					case Nodes::Jump::NodeType:
						VisitJumpStatement(node.As<Nodes::Jump>());
						break;
					case Nodes::Struct::NodeType:
						VisitStructDeclaration(node.As<Nodes::Struct>());
						break;
					case Nodes::Variable::NodeType:
						VisitVariableDeclaration(node.As<Nodes::Variable>());
						break;
					case Nodes::Function::NodeType:
						VisitFunctionDeclaration(node.As<Nodes::Function>());
						break;
					case Nodes::Technique::NodeType:
						VisitTechniqueDeclaration(node.As<Nodes::Technique>());
						break;
				}
			}
			void												VisitType(const Nodes::Type &type)
			{
				if (type.HasQualifier(Nodes::Type::Qualifier::Static) || (type.HasQualifier(Nodes::Type::Qualifier::Const) && !this->mCurrentInParameterBlock && !this->mCurrentInFunctionBlock))
					this->mCurrentSource += "static ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Const))
					this->mCurrentSource += "const ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Volatile))
					this->mCurrentSource += "volatile ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Precise))
					this->mCurrentSource += "precise ";
				if (type.HasQualifier(Nodes::Type::Qualifier::NoInterpolation))
					this->mCurrentSource += "nointerpolation ";
				if (type.HasQualifier(Nodes::Type::Qualifier::NoPerspective))
					this->mCurrentSource += "noperspective ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Linear))
					this->mCurrentSource += "linear ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Centroid))
					this->mCurrentSource += "centroid ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Sample))
					this->mCurrentSource += "sample ";
				if (type.HasQualifier(Nodes::Type::Qualifier::InOut))
					this->mCurrentSource += "inout ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::In))
					this->mCurrentSource += "in ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::Out))
					this->mCurrentSource += "out ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::Uniform))
					this->mCurrentSource += "uniform ";
				if (type.HasQualifier(Nodes::Type::Qualifier::RowMajor))
					this->mCurrentSource += "row_major ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::ColumnMajor))
					this->mCurrentSource += "column_major ";

				switch (type.Class)
				{
					case Nodes::Type::Void:
						this->mCurrentSource += "void";
						break;
					case Nodes::Type::Bool:
						this->mCurrentSource += "bool";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Int:
					case Nodes::Type::Uint:
						this->mCurrentSource += "int";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Half:
						this->mCurrentSource += "half";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Float:
						this->mCurrentSource += "float";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Double:
						this->mCurrentSource += "double";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Sampler1D:
						this->mCurrentSource += "sampler1D";
						break;
					case Nodes::Type::Sampler2D:
						this->mCurrentSource += "sampler2D";
						break;
					case Nodes::Type::Sampler3D:
						this->mCurrentSource += "sampler3D";
						break;
					case Nodes::Type::Struct:
						this->mCurrentSource += this->mAST[type.Definition].As<Nodes::Struct>().Name;
						break;
				}
			}
			void												VisitAggregate(const Nodes::Aggregate &data)
			{
				for (size_t i = 0; i < data.Length; ++i)
				{
					Visit(this->mAST[data.Find(this->mAST, i)]);
				}
			}
			void												VisitExpressionSequence(const Nodes::ExpressionSequence &data)
			{
				for (size_t i = 0; i < data.Length; ++i)
				{
					Visit(this->mAST[data.Find(this->mAST, i)]);

					this->mCurrentSource += ", ";
				}

				this->mCurrentSource.pop_back();
				this->mCurrentSource.pop_back();
			}
			void												VisitLiteral(const Nodes::Literal &data)
			{
				switch (data.Type.Class)
				{
					case Nodes::Type::Bool:
						this->mCurrentSource += data.Value.AsInt ? "true" : "false";
						break;
					case Nodes::Type::Int:
						this->mCurrentSource += std::to_string(data.Value.AsInt);
						break;
					case Nodes::Type::Uint:
						this->mCurrentSource += std::to_string(data.Value.AsUint);
						break;
					case Nodes::Type::Float:
						this->mCurrentSource += std::to_string(data.Value.AsFloat) + "f";
						break;
					case Nodes::Type::Double:
						this->mCurrentSource += std::to_string(data.Value.AsDouble) + "lf";
						break;
				}
			}
			void												VisitReference(const Nodes::Reference &data)
			{
				const auto &symbol = this->mAST[data.Symbol].As<Nodes::Variable>();

				this->mCurrentSource += symbol.Name;
			}
			void												VisitUnaryExpression(const Nodes::UnaryExpression &data)
			{
				switch (data.Operator)
				{
					case Nodes::Operator::Plus:
						this->mCurrentSource += '+';
						break;
					case Nodes::Operator::Minus:
						this->mCurrentSource += '-';
						break;
					case Nodes::Operator::BitwiseNot:
						this->mCurrentSource += "(4294967295 - ";
						break;
					case Nodes::Operator::LogicalNot:
						this->mCurrentSource += '!';
						break;
					case Nodes::Operator::Increase:
						this->mCurrentSource += "++";
						break;
					case Nodes::Operator::Decrease:
						this->mCurrentSource += "--";
						break;
					case Nodes::Operator::Cast:
						this->mCurrentSource += '(';
						VisitType(data.Type);
						this->mCurrentSource += ')';
						break;
				}

				this->mCurrentSource += '(';

				Visit(this->mAST[data.Operand]);

				switch (data.Operator)
				{
					case Nodes::Operator::BitwiseNot:
						this->mCurrentSource += ')';
						break;
					case Nodes::Operator::PostIncrease:
						this->mCurrentSource += "++";
						break;
					case Nodes::Operator::PostDecrease:
						this->mCurrentSource += "--";
						break;
				}

				this->mCurrentSource += ')';
			}
			void												VisitBinaryExpression(const Nodes::BinaryExpression &data)
			{
				if (data.Operator == Nodes::Operator::LeftShift)
				{
					this->mCurrentSource += "((";
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ") * pow(2, ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += "))";
				}
				else if (data.Operator == Nodes::Operator::RightShift)
				{
					this->mCurrentSource += "floor((";
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ") / pow(2, ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += "))";
				}
				else if (data.Operator == Nodes::Operator::BitwiseAnd)
				{
					const auto &right = this->mAST[data.Right];

					if (right.Is<Nodes::Literal>() && right.As<Nodes::Literal>().Type.IsIntegral())
					{
						if (IsPowerOf2(right.As<Nodes::Literal>().Value.AsInt + 1))
						{
							this->mCurrentSource += "((" + std::to_string(right.As<Nodes::Literal>().Value.AsInt + 1) + ") * frac((";
							Visit(this->mAST[data.Left]);
							this->mCurrentSource += ") / (" + std::to_string(right.As<Nodes::Literal>().Value.AsInt + 1) + ")))";
							return;
						}
						else if (IsPowerOf2(right.As<Nodes::Literal>().Value.AsInt))
						{
							this->mCurrentSource += "((((";
							Visit(this->mAST[data.Left]);
							this->mCurrentSource += ") / (" + std::to_string(right.As<Nodes::Literal>().Value.AsInt) + ")) % 2) * " + std::to_string(right.As<Nodes::Literal>().Value.AsInt) + ")";
							return;
						}
					}

					const auto &location = this->mAST[data.Right].Location;

					this->mErrors += std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): Bitwise operations are not supported on legacy targets!";
					this->mFatal = true;
				}
				else if (data.Operator == Nodes::Operator::BitwiseXor || data.Operator == Nodes::Operator::BitwiseOr)
				{
					const auto &location = this->mAST[data.Right].Location;

					this->mErrors += std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): Bitwise operations are not supported on legacy targets!";
					this->mFatal = true;
				}
				else if (data.Operator == Nodes::Operator::Index)
				{
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += '[';
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ']';
				}
				else
				{
					this->mCurrentSource += '(';
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ' ';

					switch (data.Operator)
					{
						case Nodes::Operator::Add:
							this->mCurrentSource += '+';
							break;
						case Nodes::Operator::Substract:
							this->mCurrentSource += '-';
							break;
						case Nodes::Operator::Multiply:
							this->mCurrentSource += '*';
							break;
						case Nodes::Operator::Divide:
							this->mCurrentSource += '/';
							break;
						case Nodes::Operator::Modulo:
							this->mCurrentSource += '%';
							break;
						case Nodes::Operator::LogicalAnd:
							this->mCurrentSource += "&&";
							break;
						case Nodes::Operator::LogicalXor:
							this->mCurrentSource += "^^";
							break;
						case Nodes::Operator::LogicalOr:
							this->mCurrentSource += "||";
							break;
						case Nodes::Operator::Less:
							this->mCurrentSource += '<';
							break;
						case Nodes::Operator::Greater:
							this->mCurrentSource += '>';
							break;
						case Nodes::Operator::LessOrEqual:
							this->mCurrentSource += "<=";
							break;
						case Nodes::Operator::GreaterOrEqual:
							this->mCurrentSource += ">=";
							break;
						case Nodes::Operator::Equal:
							this->mCurrentSource += "==";
							break;
						case Nodes::Operator::Unequal:
							this->mCurrentSource += "!=";
							break;
					}

					this->mCurrentSource += ' ';
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ')';
				}
			}
			void												VisitAssignmentExpression(const Nodes::Assignment &data)
			{
				if (data.Operator == Nodes::Operator::LeftShift)
				{
					this->mCurrentSource += "((";
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ") *= pow(2, ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += "))";
				}
				else if (data.Operator == Nodes::Operator::RightShift)
				{
					this->mCurrentSource += "((";
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ") /= pow(2, ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += "))";
				}
				else if (data.Operator == Nodes::Operator::BitwiseAnd || data.Operator == Nodes::Operator::BitwiseXor || data.Operator == Nodes::Operator::BitwiseOr)
				{
					const auto &location = this->mAST[data.Right].Location;

					this->mErrors += std::string(location.Source != nullptr ? location.Source : "") + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): Bitwise operations are not supported on legacy targets!";
					this->mFatal = true;
					return;
				}
				else
				{
					this->mCurrentSource += '(';
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += ' ';

					switch (data.Operator)
					{
						case Nodes::Operator::Add:
							this->mCurrentSource += "+=";
							break;
						case Nodes::Operator::Substract:
							this->mCurrentSource += "-=";
							break;
						case Nodes::Operator::Multiply:
							this->mCurrentSource += "*=";
							break;
						case Nodes::Operator::Divide:
							this->mCurrentSource += "/=";
							break;
						case Nodes::Operator::Modulo:
							this->mCurrentSource += "%=";
							break;
						case Nodes::Operator::None:
							this->mCurrentSource += '=';
							break;
					}

					this->mCurrentSource += ' ';
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ')';
				}
			}
			void												VisitConditionalExpression(const Nodes::Conditional &data)
			{
				this->mCurrentSource += '(';
				Visit(this->mAST[data.Condition]);
				this->mCurrentSource += " ? ";
				Visit(this->mAST[data.ExpressionTrue]);
				this->mCurrentSource += " : ";
				Visit(this->mAST[data.ExpressionFalse]);
				this->mCurrentSource += ')';
			}
			void												VisitCallExpression(const Nodes::Call &data)
			{
				const auto &callee = this->mAST[data.Callee].As<Nodes::Function>();

				if (data.Left != 0)
				{
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += '.';
				}

				this->mCurrentSource += callee.Name;
				this->mCurrentSource += '(';

				if (data.Parameters != 0)
				{
					const auto &parameters = this->mAST[data.Parameters].As<Nodes::Aggregate>();

					for (size_t i = 0; i < parameters.Length; ++i)
					{
						Visit(this->mAST[parameters.Find(this->mAST, i)]);

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();
				}

				this->mCurrentSource += ")";
			}
			void												VisitConstructorExpression(const Nodes::Constructor &data)
			{
				auto type = data.Type;
				type.Qualifiers = 0;			
				VisitType(type);

				this->mCurrentSource += '(';

				if (data.Parameters != 0)
				{
					const auto &parameters = this->mAST[data.Parameters].As<Nodes::Aggregate>();

					for (size_t i = 0; i < parameters.Length; ++i)
					{
						Visit(this->mAST[parameters.Find(this->mAST, i)]);

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();
				}

				this->mCurrentSource += ')';
			}
			void												VisitFieldExpression(const Nodes::Field &data)
			{
				const auto &left = this->mAST[data.Left];

				Visit(left);

				if (!left.Is<Nodes::Reference>() || !left.As<Nodes::Reference>().Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
				{
					this->mCurrentSource += '.';
				}
				else
				{
					this->mCurrentSource += '_';
				}

				this->mCurrentSource += this->mAST[data.Callee].As<Nodes::Variable>().Name;
			}
			void												VisitSwizzleExpression(const Nodes::Swizzle &data)
			{
				const auto &left = this->mAST[data.Left];

				Visit(left);

				this->mCurrentSource += '.';

				if (left.As<Nodes::Expression>().Type.IsMatrix())
				{
					const char swizzle[16][5] =
					{
						"_m00", "_m01", "_m02", "_m03",
						"_m10", "_m11", "_m12", "_m13",
						"_m20", "_m21", "_m22", "_m23",
						"_m30", "_m31", "_m32", "_m33"
					};

					for (int i = 0; i < 4 && data.Offsets[i] >= 0; ++i)
					{
						this->mCurrentSource += swizzle[data.Offsets[i]];
					}
				}
				else
				{
					const char swizzle[4] =
					{
						'x', 'y', 'z', 'w'
					};

					for (int i = 0; i < 4 && data.Offsets[i] >= 0; ++i)
					{
						this->mCurrentSource += swizzle[data.Offsets[i]];
					}
				}
			}
			void												VisitStatementBlock(const Nodes::StatementBlock &data)
			{
				this->mCurrentSource += "{\n";

				for (size_t i = 0; i < data.Length; ++i)
				{
					Visit(this->mAST[data.Find(this->mAST, i)]);
				}

				this->mCurrentSource += "}\n";
			}
			void												VisitExpressionStatement(const Nodes::ExpressionStatement &data)
			{
				Visit(this->mAST[data.Expression]);
				this->mCurrentSource += ";\n";
			}
			void												VisitDeclarationStatement(const Nodes::DeclarationStatement &data)
			{
				Visit(this->mAST[data.Declaration]);
			}
			void												VisitSelectionStatement(const Nodes::Selection &data)
			{
				switch (data.Mode)
				{
					case Nodes::Selection::If:
					{
						switch (data.Attributes)
						{
							case Nodes::Attribute::Flatten:
								this->mCurrentSource += "[flatten] ";
								break;
							case Nodes::Attribute::Branch:
								this->mCurrentSource += "[branch] ";
								break;
						}

						this->mCurrentSource += "if (";
						Visit(this->mAST[data.Condition]);
						this->mCurrentSource += ")\n";

						if (data.StatementTrue != 0)
						{
							Visit(this->mAST[data.StatementTrue]);

							if (data.StatementFalse != 0)
							{
								this->mCurrentSource += "else\n";
								Visit(this->mAST[data.StatementFalse]);
							}
						}
						else
						{
							this->mCurrentSource += "\t;";
						}					
						break;
					}
					case Nodes::Selection::Switch:
					{
						this->mErrors += "Warning: Switch statements do not support fallthrough in Direct3D9!\n";

						this->mCurrentSource += "[unroll] do { ";
						VisitType(this->mAST[data.Condition].As<Nodes::Expression>().Type);
						this->mCurrentSource += " __switch_condition = ";
						Visit(this->mAST[data.Condition]);
						this->mCurrentSource += ";\n";

						this->mCurrentSwitchCaseIndex = 0;
						this->mCurrentSwitchDefault = 0;

						Visit(this->mAST[data.Statement]);

						if (this->mCurrentSwitchDefault != 0)
						{
							if (this->mCurrentSwitchCaseIndex != 0)
							{
								this->mCurrentSource += "else\n";
							}

							Visit(this->mAST[this->mCurrentSwitchDefault]);
						}

						this->mCurrentSource += "} while (false);\n";
						break;
					}
					case Nodes::Selection::Case:
					{
						const auto &cases = this->mAST[data.Condition].As<Nodes::Aggregate>();

						std::vector<EffectTree::Index> items;
						items.reserve(cases.Length);

						for (size_t i = 0; i < cases.Length; ++i)
						{
							EffectTree::Index itemIndex = cases.Find(this->mAST, i);
							const auto &item = this->mAST[itemIndex];

							if (item.Is<Nodes::Expression>())
							{
								this->mCurrentSwitchDefault = data.Statement;
								return;
							}
							else
							{
								items.push_back(itemIndex);
							}
						}

						if (this->mCurrentSwitchCaseIndex != 0)
						{
							this->mCurrentSource += "else ";
						}

						this->mCurrentSource += "if (";

						for (auto begin = items.cbegin(), end = items.cend(), it = begin; it != end; ++it)
						{
							this->mCurrentSwitchCaseIndex++;

							if (it != begin)
							{
								this->mCurrentSource += "|| ";
							}

							this->mCurrentSource += "__switch_condition == (";
							Visit(this->mAST[*it]);
							this->mCurrentSource += ")";
						}

						this->mCurrentSource += ")\n";
						Visit(this->mAST[data.Statement]);
						break;
					}
				}
			}
			void												VisitIterationStatement(const Nodes::Iteration &data)
			{
				switch (data.Mode)
				{
					case Nodes::Iteration::For:
					{
						switch (data.Attributes)
						{
							case Nodes::Attribute::Unroll:
								this->mCurrentSource += "[unroll] ";
								break;
							case Nodes::Attribute::Loop:
								this->mCurrentSource += "[loop] ";
								break;
							case Nodes::Attribute::FastOpt:
								this->mCurrentSource += "[fastopt] ";
								break;
						}

						this->mCurrentSource += "for (";

						if (data.Initialization != 0)
						{
							Visit(this->mAST[data.Initialization]);

							this->mCurrentSource.pop_back();
							this->mCurrentSource.pop_back();
						}

						this->mCurrentSource += "; ";
										
						if (data.Condition != 0)
						{
							Visit(this->mAST[data.Condition]);
						}

						this->mCurrentSource += "; ";

						if (data.Expression != 0)
						{
							Visit(this->mAST[data.Expression]);
						}

						this->mCurrentSource += ")\n";

						if (data.Statement != 0)
						{
							Visit(this->mAST[data.Statement]);
						}
						else
						{
							this->mCurrentSource += "\t;";
						}
						break;
					}
					case Nodes::Iteration::Mode::While:
					{
						switch (data.Attributes)
						{
							case Nodes::Attribute::Unroll:
								this->mCurrentSource += "[unroll] ";
								break;
							case Nodes::Attribute::Loop:
								this->mCurrentSource += "[loop] ";
								break;
							case Nodes::Attribute::FastOpt:
								this->mCurrentSource += "[fastopt] ";
								break;
						}

						this->mCurrentSource += "while (";
						Visit(this->mAST[data.Condition]);
						this->mCurrentSource += ")\n";

						if (data.Statement != 0)
						{
							Visit(this->mAST[data.Statement]);
						}
						else
						{
							this->mCurrentSource += "\t;";
						}
					}
					case Nodes::Iteration::Mode::DoWhile:
					{
						switch (data.Attributes)
						{
							case Nodes::Attribute::FastOpt:
								this->mCurrentSource += "[fastopt] ";
								break;
						}

						this->mCurrentSource += "do\n{\n";
						Visit(this->mAST[data.Statement]);
						this->mCurrentSource += "}\n";
						this->mCurrentSource += "while (";
						Visit(this->mAST[data.Condition]);
						this->mCurrentSource += ");\n";
					}
				}
			}
			void												VisitJumpStatement(const Nodes::Jump &data)
			{
				switch (data.Mode)
				{
					case Nodes::Jump::Break:
					{
						this->mCurrentSource += "break";
						break;
					}
					case Nodes::Jump::Continue:
					{
						this->mCurrentSource += "continue";
						break;
					}
					case Nodes::Jump::Return:
					{
						switch (this->mCurrentPositionVariableType)
						{
							case 1:
							{
								this->mCurrentSource += this->mCurrentPositionVariable + ".xy += _TEXEL_OFFSET_ * " + this->mCurrentPositionVariable + ".ww;\n";
							}
							default:
							{
								this->mCurrentSource += "return";

								if (data.Expression != 0)
								{
									this->mCurrentSource += ' ';
									Visit(this->mAST[data.Expression]);
								}
								break;
							}
							case 2:
							{
								const auto &expression = this->mAST[data.Expression];

								VisitType(expression.As<Nodes::Expression>().Type);
								this->mCurrentSource += " _return = ";
								Visit(expression);
								this->mCurrentSource += ";\n";
								this->mCurrentSource += "_return" + this->mCurrentPositionVariable + ".xy += _TEXEL_OFFSET_ * _return" + this->mCurrentPositionVariable + ".ww;\n";
								this->mCurrentSource += "return _return";
								break;
							}
						}
						break;
					}
					case Nodes::Jump::Discard:
					{
						this->mCurrentSource += "discard";
						break;
					}
				}

				this->mCurrentSource += ";\n";
			}
			void												VisitStructDeclaration(const Nodes::Struct &data)
			{
				this->mCurrentSource += "struct ";

				if (data.Name != nullptr)
				{
					this->mCurrentSource += data.Name;
				}

				this->mCurrentSource += "\n{\n";

				if (data.Fields != 0)
				{
					const auto &fields = this->mAST[data.Fields].As<Nodes::Aggregate>();

					for (size_t i = 0; i < fields.Length; ++i)
					{
						Visit(this->mAST[fields.Find(this->mAST, i)]);
					}
				}
				else
				{
					this->mCurrentSource += "float _dummy;\n";
				}

				this->mCurrentSource += "};\n";
			}
			void												VisitVariableDeclaration(const Nodes::Variable &data)
			{
				if (!(this->mCurrentInParameterBlock || this->mCurrentInFunctionBlock))
				{
					if (data.Type.IsStruct() && data.Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
					{
						VisitUniformBufferDeclaration(data);
						return;
					}
					else if (data.Type.IsSampler())
					{
						VisitSamplerDeclaration(data);
						return;
					}
					else if (data.Type.IsTexture())
					{
						VisitTextureDeclaration(data);
						return;
					}
					else if (data.Type.HasQualifier(Nodes::Type::Qualifier::Uniform))
					{
						VisitUniformSingleDeclaration(data);
						return;
					}
				}

				VisitType(data.Type);

				if (data.HasSemantic() && boost::iequals(data.Semantic, "SV_VertexID"))
				{
					this->mCurrentSource += " __VERTEXID";
				}
				else if (data.Name != nullptr)
				{
					this->mCurrentSource += ' ';

					if (!this->mCurrentBlockName.empty())
					{
						this->mCurrentSource += this->mCurrentBlockName + '_';
					}
				
					this->mCurrentSource += data.Name;
				}

				if (data.Type.IsArray())
				{
					for (size_t i = 0; i < data.Type.ElementsDimension; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += (data.Type.Elements[i] > 0) ? std::to_string(data.Type.Elements[i]) : "";
						this->mCurrentSource += ']';
					}
				}

				if (data.HasSemantic())
				{
					this->mCurrentSource += " : ";
					this->mCurrentSource += data.Semantic;
				}

				if (!this->mCurrentInParameterBlock)
				{
					if (data.HasInitializer())
					{
						this->mCurrentSource += " = ";

						const auto &initializer = this->mAST[data.Initializer];

						if (initializer.Is<Nodes::Aggregate>())
						{
							this->mCurrentSource += "{ ";

							for (size_t i = 0; i < initializer.As<Nodes::Aggregate>().Length; ++i)
							{
								Visit(this->mAST[initializer.As<Nodes::Aggregate>().Find(this->mAST, i)]);

								this->mCurrentSource += ", ";
							}

							this->mCurrentSource += " }";
						}
						else
						{
							Visit(initializer);
						}
					}

					this->mCurrentSource += ";\n";
				}
			}
			void												VisitSamplerDeclaration(const Nodes::Variable &data)
			{
				D3D9Sampler sampler;
				sampler.mStates[D3DSAMP_ADDRESSU] = D3DTADDRESS_WRAP;
				sampler.mStates[D3DSAMP_ADDRESSV] = D3DTADDRESS_WRAP;
				sampler.mStates[D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
				sampler.mStates[D3DSAMP_BORDERCOLOR] = 0;
				sampler.mStates[D3DSAMP_MINFILTER] = D3DTEXF_POINT;
				sampler.mStates[D3DSAMP_MAGFILTER] = D3DTEXF_POINT;
				sampler.mStates[D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
				sampler.mStates[D3DSAMP_MIPMAPLODBIAS] = 0;
				sampler.mStates[D3DSAMP_MAXMIPLEVEL] = 0;
				sampler.mStates[D3DSAMP_MAXANISOTROPY] = 1;
				sampler.mStates[D3DSAMP_SRGBTEXTURE] = FALSE;
			
				if (data.HasInitializer())
				{
					#pragma region Fill Description
					const auto &states = this->mAST[data.Initializer].As<Nodes::Aggregate>();

					for (size_t i = 0; i < states.Length; ++i)
					{
						const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();
					
						switch (state.Type)
						{
							case Nodes::State::Texture:
								try
								{
									sampler.mTexture = this->mEffect->mTextures.at(this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name).get();
									break;
								}
								catch (...)
								{
									return;
								}
							case Nodes::State::MinFilter:
								sampler.mStates[D3DSAMP_MINFILTER] = ConvertLiteralToTextureFilter(state.Value.AsInt);
								break;
							case Nodes::State::MagFilter:
								sampler.mStates[D3DSAMP_MAGFILTER] = ConvertLiteralToTextureFilter(state.Value.AsInt);
								break;
							case Nodes::State::MipFilter:
								sampler.mStates[D3DSAMP_MIPFILTER] = ConvertLiteralToTextureFilter(state.Value.AsInt);
								break;
							case Nodes::State::AddressU:
							case Nodes::State::AddressV:
							case Nodes::State::AddressW:
								sampler.mStates[D3DSAMP_ADDRESSU + (state.Type - Nodes::State::AddressU)] = ConvertLiteralToTextureAddress(state.Value.AsInt);
								break;
							case Nodes::State::MipLODBias:
								sampler.mStates[D3DSAMP_MIPMAPLODBIAS] = static_cast<DWORD>(state.Value.AsFloat);
								break;
							case Nodes::State::MaxLOD:
								sampler.mStates[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(state.Value.AsFloat);
								break;
							case Nodes::State::MaxAnisotropy:
								sampler.mStates[D3DSAMP_MAXANISOTROPY] = state.Value.AsInt;
								break;
							case Nodes::State::SRGB:
								sampler.mStates[D3DSAMP_SRGBTEXTURE] = state.Value.AsInt;
								break;
						}
					}
					#pragma endregion
				}

				this->mCurrentSource += "sampler" + std::to_string(sampler.mTexture->mDimension) + "D ";
				this->mCurrentSource += data.Name;
				this->mCurrentSource += " : register(s" + std::to_string(this->mEffect->mSamplers.size()) + ");\n";

				this->mEffect->mSamplers.push_back(sampler);
			}
			void												VisitTextureDeclaration(const Nodes::Variable &data)
			{
				IDirect3DTexture9 *texture = nullptr;
				UINT width = 1, height = 1, depth = 1, levels = 1;
				D3DFORMAT format1 = D3DFMT_A8R8G8B8;
				Effect::Texture::Format format2 = Effect::Texture::Format::RGBA8;
			
				if (data.HasInitializer())
				{
					#pragma region Fill Description
					const auto &states = this->mAST[data.Initializer].As<Nodes::Aggregate>();

					for (size_t i = 0; i < states.Length; ++i)
					{
						const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();
					
						switch (state.Type)
						{
							case Nodes::State::Width:
								width = state.Value.AsInt;
								break;
							case Nodes::State::Height:
								height = state.Value.AsInt;
								break;
							case Nodes::State::Depth:
								depth = state.Value.AsInt;
								break;
							case Nodes::State::MipLevels:
								levels = state.Value.AsInt;
								break;
							case Nodes::State::Format:
								format1 = ConvertLiteralToFormat(state.Value.AsInt, format2);
								break;
						}
					}
					#pragma endregion
				}

				HRESULT hr = E_FAIL;
			
				switch (data.Type.Class)
				{
					case Nodes::Type::Texture1D:
					case Nodes::Type::Texture2D:
					{
						hr = this->mEffect->mEffectContext->mDevice->CreateTexture(width, height, levels, format1 == D3DFMT_A8R8G8B8 ? D3DUSAGE_RENDERTARGET : 0, format1, D3DPOOL_DEFAULT, &texture, nullptr);
						break;
					}
					case Nodes::Type::Texture3D:
					{
						hr = this->mEffect->mEffectContext->mDevice->CreateVolumeTexture(width, height, depth, levels, 0, format1, D3DPOOL_DEFAULT, reinterpret_cast<IDirect3DVolumeTexture9 **>(&texture), nullptr);
						break;
					}
				}

				if (SUCCEEDED(hr))
				{
					auto obj = std::unique_ptr<D3D9Texture>(new D3D9Texture(this->mEffect));
					obj->mDesc.Width = width;
					obj->mDesc.Height = height;
					obj->mDesc.Depth = depth;
					obj->mDesc.Levels = levels;
					obj->mDesc.Format = format2;
					obj->mDimension = data.Type.Class - Nodes::Type::Texture1D + 1;
					obj->mTexture = texture;

					if (obj->mDimension < 3)
					{
						obj->mTexture->GetSurfaceLevel(0, &obj->mSurface);
					}

					if (data.Annotations != 0)
					{
						const auto &annotations = this->mAST[data.Annotations].As<Nodes::Aggregate>();

						for (size_t i = 0; i < annotations.Length; ++i)
						{
							VisitAnnotation(this->mAST[annotations.Find(this->mAST, i)].As<Nodes::Annotation>(), obj->mAnnotations);
						}
					}

					this->mEffect->mTextures.insert(std::make_pair(data.Name, std::move(obj)));
				}
				else
				{
					this->mFatal = true;
				}
			}
			void												VisitUniformSingleDeclaration(const Nodes::Variable &data)
			{
				VisitType(data.Type);

				if (data.Name != nullptr)
				{
					this->mCurrentSource += ' ';

					if (!this->mCurrentBlockName.empty())
					{
						this->mCurrentSource += this->mCurrentBlockName + '_';
					}
				
					this->mCurrentSource += data.Name;
				}

				if (data.Type.IsArray())
				{
					for (size_t i = 0; i < data.Type.ElementsDimension; ++i)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += (data.Type.Elements[i] > 0) ? std::to_string(data.Type.Elements[i]) : "";
						this->mCurrentSource += ']';
					}
				}

				this->mCurrentSource += ";\n";

				Effect::Constant::Description desc;
				desc.Rows = data.Type.Rows;
				desc.Columns = data.Type.Cols;
				desc.Elements = data.Type.Elements[0] + data.Type.Elements[1] + data.Type.Elements[2] + data.Type.Elements[3] + data.Type.Elements[4] + data.Type.Elements[5] + data.Type.Elements[6] + data.Type.Elements[7] + data.Type.Elements[8] + data.Type.Elements[9] + data.Type.Elements[10] + data.Type.Elements[11] + data.Type.Elements[12] + data.Type.Elements[13] + data.Type.Elements[14] + data.Type.Elements[15];
				desc.Fields = 0;
				desc.Size = std::max(data.Type.Rows, 1U) * std::max(data.Type.Cols, 1U);

				switch (data.Type.Class)
				{
					case Nodes::Type::Bool:
						desc.Size *= sizeof(int);
						desc.Type = Effect::Constant::Type::Bool;
						break;
					case Nodes::Type::Int:
						desc.Size *= sizeof(int);
						desc.Type = Effect::Constant::Type::Int;
						break;
					case Nodes::Type::Uint:
						desc.Size *= sizeof(unsigned int);
						desc.Type = Effect::Constant::Type::Uint;
						break;
					case Nodes::Type::Half:
						desc.Size *= sizeof(float) / 2;
						desc.Type = Effect::Constant::Type::Half;
						break;
					case Nodes::Type::Float:
						desc.Size *= sizeof(float);
						desc.Type = Effect::Constant::Type::Float;
						break;
					case Nodes::Type::Double:
						desc.Size *= sizeof(double);
						desc.Type = Effect::Constant::Type::Double;
						break;
				}

				auto obj = std::unique_ptr<D3D9Constant>(new D3D9Constant(this->mEffect));
				obj->mDesc = desc;
				obj->mRegisterOffset = this->mCurrentRegisterIndex;

				const float registersize = static_cast<float>(desc.Size) / (4 * sizeof(float));
				const UINT alignment = static_cast<UINT>((registersize - static_cast<UINT>(registersize)) > 0);
				this->mCurrentRegisterIndex += static_cast<UINT>(registersize) + alignment;

				if ((this->mCurrentRegisterIndex * sizeof(float) * 4) >= this->mCurrentStorageSize)
				{
					this->mEffect->mConstantStorage = static_cast<float *>(::realloc(this->mEffect->mConstantStorage, this->mCurrentStorageSize += sizeof(float) * 4 * 16));
				}

				if (data.Initializer && this->mAST[data.Initializer].Is<Nodes::Literal>())
				{
					std::memcpy(this->mEffect->mConstantStorage + obj->mRegisterOffset * 4, &this->mAST[data.Initializer].As<Nodes::Literal>().Value, desc.Size);
				}
				else
				{
					std::memset(this->mEffect->mConstantStorage + obj->mRegisterOffset * 4, 0, desc.Size);
				}

				this->mEffect->mConstants.insert(std::make_pair(this->mCurrentBlockName.empty() ? data.Name : (this->mCurrentBlockName + '.' + data.Name), std::move(obj)));
				this->mEffect->mConstantRegisterCount = this->mCurrentRegisterIndex;
			}
			void												VisitUniformBufferDeclaration(const Nodes::Variable &data)
			{
				const auto &structure = this->mAST[data.Type.Definition].As<Nodes::Struct>();

				if (structure.Fields == 0)
				{
					return;
				}

				this->mCurrentBlockName = data.Name;

				const auto &fields = this->mAST[structure.Fields].As<Nodes::Aggregate>();

				UINT previousIndex = this->mCurrentRegisterIndex;

				for (size_t i = 0; i < fields.Length; ++i)
				{
					const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

					VisitUniformSingleDeclaration(field);
				}

				auto obj = std::unique_ptr<D3D9Constant>(new D3D9Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fields.Length;
				obj->mDesc.Size = (this->mCurrentRegisterIndex - previousIndex) * sizeof(float) * 4;
				obj->mDesc.Type = Effect::Constant::Type::Struct;

				if (data.Annotations != 0)
				{
					const auto &annotations = this->mAST[data.Annotations].As<Nodes::Aggregate>();

					for (size_t i = 0; i < annotations.Length; ++i)
					{
						VisitAnnotation(this->mAST[annotations.Find(this->mAST, i)].As<Nodes::Annotation>(), obj->mAnnotations);
					}
				}

				this->mEffect->mConstants.insert(std::make_pair(data.Name, std::move(obj)));

				this->mCurrentBlockName.clear();
			}
			void												VisitFunctionDeclaration(const Nodes::Function &data)
			{
				auto type = data.ReturnType;
				type.Qualifiers &= ~static_cast<int>(Nodes::Type::Qualifier::Const);
				VisitType(type);

				this->mCurrentSource += ' ';
				this->mCurrentSource += data.Name;
				this->mCurrentSource += '(';

				std::string vertexid;

				if (data.HasArguments())
				{
					const auto &arguments = this->mAST[data.Arguments].As<Nodes::Aggregate>();

					this->mCurrentInParameterBlock = true;

					for (size_t i = 0; i < arguments.Length; ++i)
					{
						const auto &argument = this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>();

						if (argument.HasSemantic() && boost::iequals(argument.Semantic, "SV_VertexID"))
						{
							vertexid = argument.Name;
							this->mCurrentSource += "float __VERTEXID : TEXCOORD0";
						}
						else
						{
							if (argument.Type.HasQualifier(Nodes::Type::Qualifier::Out))
							{
								if (argument.Type.IsStruct() && this->mAST[argument.Type.Definition].As<Nodes::Struct>().Fields != 0)
								{
									const auto &fields = this->mAST[this->mAST[argument.Type.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

									for (size_t k = 0; k < fields.Length; ++k)
									{
										const auto &field = this->mAST[fields.Find(this->mAST, k)].As<Nodes::Variable>();

										if (field.HasSemantic() && boost::iequals(field.Semantic, "SV_Position"))
										{
											this->mCurrentPositionVariable = argument.Name;
											this->mCurrentPositionVariable += '.';
											this->mCurrentPositionVariable += field.Name;
											this->mCurrentPositionVariableType = 1;
											break;
										}
									}
								}
								else if (argument.HasSemantic() && boost::iequals(argument.Semantic, "SV_Position"))
								{
									this->mCurrentPositionVariable = argument.Name;
									this->mCurrentPositionVariableType = 1;
								}
							}

							VisitVariableDeclaration(argument);
						}

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();

					this->mCurrentInParameterBlock = false;
				}

				this->mCurrentSource += ')';
								
				if (data.ReturnSemantic != nullptr)
				{
					this->mCurrentSource += " : ";
					this->mCurrentSource += ConvertSemantic(data.ReturnSemantic);

					if (boost::iequals(data.ReturnSemantic, "SV_Position"))
					{
						this->mCurrentPositionVariable = "";
						this->mCurrentPositionVariableType = 2;
					}
				}
				else if (data.ReturnType.IsStruct() && this->mAST[data.ReturnType.Definition].As<Nodes::Struct>().Fields != 0)
				{
					const auto &fields = this->mAST[this->mAST[data.ReturnType.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

					for (size_t k = 0; k < fields.Length; ++k)
					{
						const auto &field = this->mAST[fields.Find(this->mAST, k)].As<Nodes::Variable>();

						if (field.HasSemantic() && boost::iequals(field.Semantic, "SV_Position"))
						{
							this->mCurrentPositionVariable = '.';
							this->mCurrentPositionVariable += field.Name;
							this->mCurrentPositionVariableType = 2;
							break;
						}
					}
				}

				if (data.HasDefinition())
				{
					this->mCurrentSource += '\n';
					this->mCurrentInFunctionBlock = true;
					this->mCurrentSource += "{\n";

					if (!vertexid.empty())
					{
						this->mCurrentSource += "const int " + vertexid + " = (int)__VERTEXID;\n";
					}

					Visit(this->mAST[data.Definition]);

					if (this->mCurrentPositionVariableType == 1)
					{
						this->mCurrentSource += this->mCurrentPositionVariable + ".xy += _TEXEL_OFFSET_ * " + this->mCurrentPositionVariable + ".ww;\n";
					}

					this->mCurrentSource += "}\n";
					this->mCurrentInFunctionBlock = false;
				}
				else
				{
					this->mCurrentSource += ";\n";
				}

				this->mCurrentPositionVariableType = 0;
			}
			void												VisitTechniqueDeclaration(const Nodes::Technique &data)
			{
				auto obj = std::unique_ptr<D3D9Technique>(new D3D9Technique(this->mEffect));

				if (data.Passes != 0)
				{
					const auto &passes = this->mAST[data.Passes].As<Nodes::Aggregate>();

					obj->mDesc.Passes.reserve(passes.Length);
					obj->mPasses.reserve(passes.Length);

					for (size_t i = 0; i < passes.Length; ++i)
					{
						auto &pass = this->mAST[passes.Find(this->mAST, i)].As<Nodes::Pass>();

						obj->mDesc.Passes.push_back(pass.Name != nullptr ? pass.Name : "");

						VisitPassDeclaration(pass, obj->mPasses);
					}
				}

				if (data.Annotations != 0)
				{
					const auto &annotations = this->mAST[data.Annotations].As<Nodes::Aggregate>();

					for (size_t i = 0; i < annotations.Length; ++i)
					{
						VisitAnnotation(this->mAST[annotations.Find(this->mAST, i)].As<Nodes::Annotation>(), obj->mAnnotations);
					}
				}

				this->mEffect->mTechniques.insert(std::make_pair(data.Name, std::move(obj)));
			}
			void												VisitPassDeclaration(const Nodes::Pass &data, std::vector<D3D9Technique::Pass> &passes)
			{
				if (data.States == 0)
				{
					return;
				}
				
				D3D9Technique::Pass info;
				ZeroMemory(&info, sizeof(D3D9Technique::Pass));
				info.RT[0] = this->mEffect->mBackBuffer;

				IDirect3DDevice9 *device = this->mEffect->mEffectContext->mDevice;
				device->BeginStateBlock();

				device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
				device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
				device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
				device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
				device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
				device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
				device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
				device->SetRenderState(D3DRS_ALPHAREF, 0);
				device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
				device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
				device->SetRenderState(D3DRS_FOGSTART, 0);
				device->SetRenderState(D3DRS_FOGEND, 1);
				device->SetRenderState(D3DRS_FOGDENSITY, 1);
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_DEPTHBIAS, 0);
				device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
				device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
				device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
				device->SetRenderState(D3DRS_STENCILREF, 0);
				device->SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
				device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
				device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
				device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
				device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
				device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
				device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
				device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
				device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
				device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
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
				device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
				device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
				device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
				device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
				device->SetRenderState(D3DRS_FOGENABLE, FALSE );
				device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				device->SetRenderState(D3DRS_LIGHTING, FALSE);

				const auto &states = this->mAST[data.States].As<Nodes::Aggregate>();

				for (size_t i = 0; i < states.Length; ++i)
				{
					const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();

					switch (state.Type)
					{
						case Nodes::State::VertexShader:
							VisitPassShaderDeclaration(state, info);
							device->SetVertexShader(info.VS);
							break;
						case Nodes::State::PixelShader:
							VisitPassShaderDeclaration(state, info);
							device->SetPixelShader(info.PS);
							break;
						case Nodes::State::RenderTarget0:
						case Nodes::State::RenderTarget1:
						case Nodes::State::RenderTarget2:
						case Nodes::State::RenderTarget3:
						{
							const char *name = this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name;
							info.RT[state.Type - Nodes::State::RenderTarget0] = this->mEffect->mTextures.at(name)->mSurface;
							break;
						}
						case Nodes::State::RenderTarget4:
						case Nodes::State::RenderTarget5:
						case Nodes::State::RenderTarget6:
						case Nodes::State::RenderTarget7:
							this->mErrors += "Direct3D9 supports only up to 4 simultaneous rendertargets!\n";
							return;
						case Nodes::State::RenderTargetWriteMask:
							device->SetRenderState(D3DRS_COLORWRITEENABLE, state.Value.AsInt);
							break;
						case Nodes::State::CullMode:
							device->SetRenderState(D3DRS_CULLMODE, ConvertLiteralToCullMode(state.Value.AsInt));
							break;
						case Nodes::State::FillMode:
							device->SetRenderState(D3DRS_FILLMODE, ConvertLiteralToFillMode(state.Value.AsInt));
							break;
						case Nodes::State::ScissorEnable:
							device->SetRenderState(D3DRS_SCISSORTESTENABLE, state.Value.AsInt);
							break;
						case Nodes::State::DepthEnable:
							device->SetRenderState(D3DRS_ZENABLE, state.Value.AsInt);
							break;
						case Nodes::State::DepthFunc:
							device->SetRenderState(D3DRS_ZFUNC, ConvertLiteralToCompFunc(state.Value.AsInt));
							break;
						case Nodes::State::DepthWriteMask:
							device->SetRenderState(D3DRS_ZWRITEENABLE, state.Value.AsInt);
							break;
						case Nodes::State::StencilEnable:
							device->SetRenderState(D3DRS_STENCILENABLE, state.Value.AsInt);
							break;
						case Nodes::State::StencilReadMask:
							device->SetRenderState(D3DRS_STENCILMASK, state.Value.AsInt & 0xFF);
							break;
						case Nodes::State::StencilWriteMask:
							device->SetRenderState(D3DRS_STENCILWRITEMASK, state.Value.AsInt & 0xFF);
							break;
						case Nodes::State::StencilFunc:
							device->SetRenderState(D3DRS_STENCILFUNC, ConvertLiteralToCompFunc(state.Value.AsInt));
							break;
						case Nodes::State::StencilPass:
							device->SetRenderState(D3DRS_STENCILPASS, ConvertLiteralToStencilOp(state.Value.AsInt));
							break;
						case Nodes::State::StencilFail:
							device->SetRenderState(D3DRS_STENCILFAIL, ConvertLiteralToStencilOp(state.Value.AsInt));
							break;
						case Nodes::State::StencilZFail:
							device->SetRenderState(D3DRS_STENCILZFAIL, ConvertLiteralToStencilOp(state.Value.AsInt));
							break;
						case Nodes::State::StencilRef:
							device->SetRenderState(D3DRS_STENCILREF, state.Value.AsInt);
							break;
						case Nodes::State::BlendEnable:
							device->SetRenderState(D3DRS_ALPHABLENDENABLE, state.Value.AsInt);
							break;
						case Nodes::State::BlendOp:
							device->SetRenderState(D3DRS_BLENDOP, ConvertLiteralToBlendOp(state.Value.AsInt));
							break;
						case Nodes::State::BlendOpAlpha:
							device->SetRenderState(D3DRS_BLENDOPALPHA, ConvertLiteralToBlendOp(state.Value.AsInt));
							break;
						case Nodes::State::SrcBlend:
							device->SetRenderState(D3DRS_SRCBLEND, ConvertLiteralToBlend(state.Value.AsInt));
							break;
						case Nodes::State::DestBlend:
							device->SetRenderState(D3DRS_DESTBLEND, ConvertLiteralToBlend(state.Value.AsInt));
							break;
						case Nodes::State::SRGBWriteEnable:
							device->SetRenderState(D3DRS_SRGBWRITEENABLE, static_cast<DWORD>(state.Value.AsInt));
							break;
					}
				}

				device->EndStateBlock(&info.Stateblock);

				passes.push_back(info);
			}
			void												VisitPassShaderDeclaration(const Nodes::State &state, D3D9Technique::Pass &info)
			{
				std::string profile;

				switch (state.Type)
				{
					case Nodes::State::VertexShader:
						profile = "vs_3_0";
						break;
					case Nodes::State::PixelShader:
						profile = "ps_3_0";
						break;
					default:
						return;
				}

				UINT flags = 0;
#ifdef _DEBUG
				flags |= D3DCOMPILE_DEBUG;
#endif

				std::string source =
					"#define mad(m, a, b) ((m) * (a) + (b))\n"
					"#define tex1Doffset(s, c, offset) tex1D(s, (c) + (offset))\n"
					"#define tex1Dfetch(s, c) tex1D(s, float(c))\n"
					"#define tex2Doffset(s, c, offset) tex2D(s, (c) + (offset).xx)\n"
					"#define tex2Dfetch(s, c) tex2D(s, float2(c))\n"
					"#define tex3Doffset(s, c, offset) tex3D(s, (c) + (offset).xxx)\n"
					"#define tex3Dfetch(s, c) tex3D(s, float3(c))\n"
					"uniform float2 _TEXEL_OFFSET_ : register(c223);\n" + this->mCurrentSource;

				const char *entry = this->mAST[state.Value.AsNode].As<Nodes::Function>().Name;

				LOG(TRACE) << "> Compiling shader '" << entry << "':\n\n" << source.c_str() << "\n";

				ID3DBlob *compiled, *errors;
				HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, entry, profile.c_str(), flags, 0, &compiled, &errors);

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

				switch (state.Type)
				{
					case Nodes::State::VertexShader:
						hr = this->mEffect->mEffectContext->mDevice->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &info.VS);
						break;
					case Nodes::State::PixelShader:
						hr = this->mEffect->mEffectContext->mDevice->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &info.PS);
						break;
				}

				compiled->Release();

				if (FAILED(hr))
				{
					this->mFatal = true;
					return;
				}
			}
			void												VisitAnnotation(const Nodes::Annotation &annotation, std::unordered_map<std::string, Effect::Annotation> &annotations)
			{
				Effect::Annotation value;

				switch (annotation.Type.Class)
				{
					case Nodes::Type::Bool:
						value = annotation.Value.AsInt != 0;
						break;
					case Nodes::Type::Int:
					case Nodes::Type::Uint:
						value = annotation.Value.AsInt;
						break;
					case Nodes::Type::Float:
						value = annotation.Value.AsFloat;
						break;
					case Nodes::Type::Double:
						value = annotation.Value.AsDouble;
						break;
					case Nodes::Type::String:
						value = annotation.Value.AsString;
						break;
				}

				annotations.insert(std::make_pair(annotation.Name, value));
			}

		private:
			const EffectTree &									mAST;
			D3D9Effect *										mEffect;
			std::string											mCurrentSource;
			std::string &										mErrors;
			bool												mFatal;
			std::string											mCurrentBlockName;
			UINT												mCurrentRegisterIndex, mCurrentStorageSize;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
			std::size_t											mCurrentSwitchCaseIndex;
			EffectTree::Index									mCurrentSwitchDefault;
			std::string											mCurrentPositionVariable;
			int													mCurrentPositionVariableType;
		};

		// -----------------------------------------------------------------------------------------------------

		D3D9EffectContext::D3D9EffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : mDevice(device), mSwapChain(swapchain)
		{
			this->mDevice->AddRef();
			this->mSwapChain->AddRef();
		}
		D3D9EffectContext::~D3D9EffectContext(void)
		{
			this->mDevice->Release();
			this->mSwapChain->Release();
		}

		void													D3D9EffectContext::GetDimension(unsigned int &width, unsigned int &height) const
		{
			IDirect3DSurface9 *buffer;
			D3DSURFACE_DESC desc;
			this->mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &buffer);
			buffer->GetDesc(&desc);
			buffer->Release();

			width = desc.Width;
			height = desc.Height;
		}

		std::unique_ptr<Effect>									D3D9EffectContext::Compile(const EffectTree &ast, std::string &errors)
		{
			D3D9Effect *effect = new D3D9Effect(shared_from_this());
			
			ASTVisitor visitor(ast, errors);
		
			if (visitor.Traverse(effect))
			{
				return std::unique_ptr<Effect>(effect);
			}
			else
			{
				delete effect;

				return nullptr;
			}
		}

		D3D9Effect::D3D9Effect(std::shared_ptr<D3D9EffectContext> context) : mEffectContext(context), mBackBuffer(nullptr), mStateBlock(nullptr), mDepthStencil(nullptr)
		{
			HRESULT hr;

			context->mDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &this->mBackBuffer);
			context->mDevice->CreateStateBlock(D3DSBT_ALL, &this->mStateBlock);
			
			D3DSURFACE_DESC desc;
			this->mBackBuffer->GetDesc(&desc);
			
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
			SAFE_RELEASE(this->mBackBuffer);
			SAFE_RELEASE(this->mVertexDeclaration);
			SAFE_RELEASE(this->mVertexBuffer);
			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mStateBlock);
			SAFE_RELEASE(this->mShaderResourceStateblock);
		}

		Effect::Texture *										D3D9Effect::GetTexture(const std::string &name)
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
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
		Effect::Constant *										D3D9Effect::GetConstant(const std::string &name)
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
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
		Effect::Technique *										D3D9Effect::GetTechnique(const std::string &name)
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
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

		bool													D3D9Texture::Resize(const Description &desc)
		{
			HRESULT hr;
			D3DFORMAT format = D3DFMT_UNKNOWN;
			IDirect3DBaseTexture9 *texture = nullptr;

			switch (desc.Format)
			{
				case Texture::Format::R8:
					format = D3DFMT_L8;
					break;
				case Effect::Texture::Format::R32F:
					format = D3DFMT_R32F;
					break;
				case Texture::Format::RG8:
					format = D3DFMT_A8L8;
					break;
				case Texture::Format::RGBA8:
					format = D3DFMT_A8B8G8R8;
					break;
				case Texture::Format::RGBA16:
					format = D3DFMT_A16B16G16R16;
					break;
				case Texture::Format::RGBA16F:
					format = D3DFMT_A16B16G16R16F;
					break;
				case Texture::Format::RGBA32F:
					format = D3DFMT_A32B32G32R32F;
					break;
				case Texture::Format::DXT1:
					format = D3DFMT_DXT1;
					break;
				case Texture::Format::DXT3:
					format = D3DFMT_DXT3;
					break;
				case Texture::Format::DXT5:
					format = D3DFMT_DXT5;
					break;
				case Texture::Format::LATC1:
					format = static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
					break;
				case Texture::Format::LATC2:
					format = static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
					break;
			}

			if (this->mDimension == 3)
			{
				hr = this->mEffect->mEffectContext->mDevice->CreateVolumeTexture(desc.Width, desc.Height, desc.Depth, desc.Levels, 0, format, D3DPOOL_DEFAULT, reinterpret_cast<IDirect3DVolumeTexture9 **>(&texture), nullptr);
			}
			else
			{
				hr = this->mEffect->mEffectContext->mDevice->CreateTexture(desc.Width, desc.Height, desc.Levels, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, reinterpret_cast<IDirect3DTexture9 **>(&texture), nullptr);
			}

			if (FAILED(hr))
			{
				return false;
			}

			if (this->mSurface != nullptr)
			{
				IDirect3DSurface9 *surface;
				static_cast<IDirect3DTexture9 *>(texture)->GetSurfaceLevel(0, &surface);

				for (auto &technique : this->mEffect->mTechniques)
				{
					for (auto &pass : technique.second->mPasses)
					{
						for (UINT i = 0; i < 4; ++i)
						{
							if (pass.RT[i] == this->mSurface)
							{
								pass.RT[i] = surface;
							}
						}
					}
				}

				this->mSurface->Release();
				this->mSurface = surface;
			}

			SAFE_RELEASE(this->mTexture);
			this->mTexture = static_cast<IDirect3DTexture9 *>(texture);

			return true;
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
		
			HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateTexture(desc.Width, desc.Height, 1, desc.Usage, desc.Format, D3DPOOL_SYSTEMMEM, &system, nullptr);

			if (FAILED(hr))
			{
				textureSurface->Release();
				return;
			}

			system->GetSurfaceLevel(0, &systemSurface);
			system->Release();
		
			D3DLOCKED_RECT mapped;
			hr = systemSurface->LockRect(&mapped, nullptr, D3DLOCK_DISCARD);

			if (FAILED(hr))
			{
				systemSurface->Release();
				textureSurface->Release();
				return;
			}

			::memcpy(mapped.pBits, data, size);

			systemSurface->UnlockRect();

			hr = this->mEffect->mEffectContext->mDevice->UpdateSurface(systemSurface, nullptr, textureSurface, nullptr);

			systemSurface->Release();
			textureSurface->Release();
		}
		void													D3D9Texture::UpdateFromColorBuffer(void)
		{
			this->mEffect->mEffectContext->mDevice->StretchRect(this->mEffect->mBackBuffer, nullptr, this->mSurface, nullptr, D3DTEXF_NONE);
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

		const Effect::Technique::Description					D3D9Technique::GetDescription(void) const
		{
			return this->mDesc;
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

			passes = static_cast<unsigned int>(this->mDesc.Passes.size());

			if (passes == 0 || FAILED(this->mEffect->mStateBlock->Capture()) || FAILED(device->BeginScene()))
			{
				return false;
			}

			device->GetDepthStencilSurface(&this->mEffect->mStateBlockDepthStencil);
			device->GetRenderTarget(0, &this->mEffect->mStateBlockRenderTarget);

			device->SetVertexDeclaration(this->mEffect->mVertexDeclaration);
			device->SetStreamSource(0, this->mEffect->mVertexBuffer, 0, sizeof(float));

			device->SetDepthStencilSurface(this->mEffect->mDepthStencil);
			device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0x0);

			this->mEffect->mShaderResourceStateblock->Apply();

			D3DVIEWPORT9 viewport;
			device->GetViewport(&viewport);
			const float texel_offset[4] = { -1.0f / viewport.Width, 1.0f / viewport.Height };
			device->SetVertexShaderConstantF(223, texel_offset, 1);

			return true;
		}
		void													D3D9Technique::End(void) const
		{
			this->mEffect->mEffectContext->mDevice->SetDepthStencilSurface(this->mEffect->mStateBlockDepthStencil);
			SAFE_RELEASE(this->mEffect->mStateBlockDepthStencil);
			this->mEffect->mEffectContext->mDevice->SetRenderTarget(0, this->mEffect->mStateBlockRenderTarget);
			SAFE_RELEASE(this->mEffect->mStateBlockRenderTarget);

			this->mEffect->mEffectContext->mDevice->EndScene();
			this->mEffect->mStateBlock->Apply();
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

			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
		}
	}

	// -----------------------------------------------------------------------------------------------------

	std::shared_ptr<EffectContext>								CreateEffectContext(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain)
	{
		assert(device != nullptr && swapchain != nullptr);

		return std::make_shared<D3D9EffectContext>(device, swapchain);
	}
}