#include "Log.hpp"
#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectContext.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>

// -----------------------------------------------------------------------------------------------------

#define SAFE_RELEASE(p)											{ if ((p)) (p)->Release(); (p) = nullptr; }

namespace std
{
	template <> struct											hash<D3D11_SAMPLER_DESC>
	{
		inline std::size_t										operator()(const D3D11_SAMPLER_DESC &s) const 
		{
			const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
			std::size_t h = 2166136261;

			for (std::size_t i = 0; i < sizeof(D3D11_SAMPLER_DESC); ++i)
			{
				h = (h * 16777619) ^ p[i];
			}

			return h;
		}
	};

	inline bool													operator ==(const D3D11_SAMPLER_DESC &left, const D3D11_SAMPLER_DESC &right)
	{
		return std::memcmp(&left, &right, sizeof(D3D11_SAMPLER_DESC)) == 0;
	}
}
namespace ReShade
{
	namespace
	{
		class													D3D11EffectContext : public EffectContext, public std::enable_shared_from_this<D3D11EffectContext>
		{
			friend struct D3D11Effect;
			friend struct D3D11Texture;
			friend struct D3D11Constant;
			friend struct D3D11Technique;
			friend class ASTVisitor;

		public:
			D3D11EffectContext(ID3D11Device *device, IDXGISwapChain *swapchain);
			~D3D11EffectContext(void);

			void												GetDimension(unsigned int &width, unsigned int &height) const;

			std::unique_ptr<Effect>								Compile(const EffectTree &ast, std::string &errors);

		private:
			ID3D11Device *										mDevice;
			ID3D11DeviceContext *								mImmediateContext;
			IDXGISwapChain *									mSwapChain;
		};
		struct													D3D11Effect : public Effect
		{
			friend struct D3D11Texture;
			friend struct D3D11Constant;
			friend struct D3D11Technique;

			D3D11Effect(std::shared_ptr<D3D11EffectContext> context);
			~D3D11Effect(void);

			Texture *											GetTexture(const std::string &name);
			const Texture *										GetTexture(const std::string &name) const;
			std::vector<std::string>							GetTextureNames(void) const;
			Constant *											GetConstant(const std::string &name);
			const Constant *									GetConstant(const std::string &name) const;
			std::vector<std::string>							GetConstantNames(void) const;
			Technique *											GetTechnique(const std::string &name);
			const Technique *									GetTechnique(const std::string &name) const;
			std::vector<std::string>							GetTechniqueNames(void) const;

			void												ApplyConstants(void) const;

			std::shared_ptr<D3D11EffectContext>					mEffectContext;
			std::unordered_map<std::string, std::unique_ptr<D3D11Texture>> mTextures;
			std::unordered_map<std::string, std::unique_ptr<D3D11Constant>> mConstants;
			std::unordered_map<std::string, std::unique_ptr<D3D11Technique>> mTechniques;
			ID3D11Texture2D *									mBackBufferTexture;
			ID3D11RenderTargetView *							mBackBufferTarget;
			ID3D11Texture2D *									mDepthStencilTexture;
			ID3D11ShaderResourceView *							mDepthStencilView;
			ID3D11DepthStencilView *							mDepthStencil;
			std::unordered_map<D3D11_SAMPLER_DESC, size_t>		mSamplerDescs;
			std::vector<ID3D11SamplerState *>					mSamplerStates;
			std::vector<ID3D11ShaderResourceView *>				mShaderResources;
			std::vector<ID3D11Buffer *>							mConstantBuffers;
			std::vector<unsigned char *>						mConstantStorages;
			mutable bool										mConstantsDirty;
		};
		struct													D3D11Texture : public Effect::Texture
		{
			D3D11Texture(D3D11Effect *effect);
			~D3D11Texture(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Resize(const Description &desc);
			void												Update(unsigned int level, const unsigned char *data, std::size_t size);
			void												UpdateFromColorBuffer(void);
			void												UpdateFromDepthBuffer(void);

			D3D11Effect *										mEffect;
			Description											mDesc;
			unsigned int										mDimension, mRegister;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			union
			{
				ID3D11Resource *								mTexture;
				ID3D11Texture1D *								mTexture1D;
				ID3D11Texture2D *								mTexture2D;
				ID3D11Texture3D *								mTexture3D;
			};
			ID3D11ShaderResourceView *							mShaderResourceView[2];
			ID3D11RenderTargetView *							mRenderTargetView[2];
		};
		struct													D3D11Constant : public Effect::Constant
		{
			D3D11Constant(D3D11Effect *effect);
			~D3D11Constant(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;
			void												GetValue(unsigned char *data, std::size_t size) const;
			void												SetValue(const unsigned char *data, std::size_t size);

			D3D11Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::size_t											mBuffer, mBufferOffset;
		};
		struct													D3D11Technique : public Effect::Technique
		{
			struct												Pass
			{
				ID3D11VertexShader *							VS;
				ID3D11RasterizerState *							RS;
				ID3D11PixelShader *								PS;
				ID3D11BlendState *								BS;
				ID3D11DepthStencilState *						DSS;
				ID3D11RenderTargetView *						RT[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
				std::vector<ID3D11ShaderResourceView *>			SR;
			};

			D3D11Technique(D3D11Effect *effect);
			~D3D11Technique(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Begin(unsigned int &passes) const;
			void												End(void) const;
			void												RenderPass(unsigned int index) const;

			D3D11Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			ID3D11DeviceContext *								mDeferredContext;
			std::vector<Pass>									mPasses;
		};

		inline UINT												RoundToMultipleOf16(UINT size)
		{
			return (size + 15) & ~15;
		}
		inline DXGI_FORMAT										TypelessToLinearFormat(DXGI_FORMAT format)
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
		inline DXGI_FORMAT										TypelessToSRGBFormat(DXGI_FORMAT format)
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

		class													ASTVisitor
		{
		public:
			ASTVisitor(const EffectTree &ast, std::string &errors) : mAST(ast), mEffect(nullptr), mErrors(errors), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentGlobalSize(0), mCurrentGlobalStorageSize(0)
			{
			}

			bool												Traverse(D3D11Effect *effect)
			{
				this->mEffect = effect;
				this->mFatal = false;
				this->mCurrentSource.clear();

				// Global constant buffer
				this->mEffect->mConstantBuffers.push_back(nullptr);
				this->mEffect->mConstantStorages.push_back(nullptr);

				const auto &root = this->mAST[EffectTree::Root].As<Nodes::Aggregate>();

				for (size_t i = 0; i < root.Length; ++i)
				{
					Visit(this->mAST[root.Find(this->mAST, i)]);
				}

				if (this->mCurrentGlobalSize != 0)
				{
					CD3D11_BUFFER_DESC globalsDesc(RoundToMultipleOf16(this->mCurrentGlobalSize), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
					D3D11_SUBRESOURCE_DATA globalsInitial;
					globalsInitial.pSysMem = this->mEffect->mConstantStorages[0];
					globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = this->mCurrentGlobalSize;
					this->mEffect->mEffectContext->mDevice->CreateBuffer(&globalsDesc, &globalsInitial, &this->mEffect->mConstantBuffers[0]);
				}

				return !this->mFatal;
			}

		private:
			void												operator =(const ASTVisitor &);

			static D3D11_TEXTURE_ADDRESS_MODE					ConvertLiteralToTextureAddress(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::CLAMP:
						return D3D11_TEXTURE_ADDRESS_CLAMP;
					case Nodes::Literal::REPEAT:
						return D3D11_TEXTURE_ADDRESS_WRAP;
					case Nodes::Literal::MIRROR:
						return D3D11_TEXTURE_ADDRESS_MIRROR;
					case Nodes::Literal::BORDER:
						return D3D11_TEXTURE_ADDRESS_BORDER;
				}
			}
			static D3D11_CULL_MODE								ConvertLiteralToCullMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::NONE:
						return D3D11_CULL_NONE;
					case Nodes::Literal::FRONT:
						return D3D11_CULL_FRONT;
					case Nodes::Literal::BACK:
						return D3D11_CULL_BACK;
				}
			}
			static D3D11_FILL_MODE								ConvertLiteralToFillMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::SOLID:
						return D3D11_FILL_SOLID;
					case Nodes::Literal::WIREFRAME:
						return D3D11_FILL_WIREFRAME;
				}
			}
			static D3D11_COMPARISON_FUNC						ConvertLiteralToComparison(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ALWAYS:
						return D3D11_COMPARISON_ALWAYS;
					case Nodes::Literal::NEVER:
						return D3D11_COMPARISON_NEVER;
					case Nodes::Literal::EQUAL:
						return D3D11_COMPARISON_EQUAL;
					case Nodes::Literal::NOTEQUAL:
						return D3D11_COMPARISON_NOT_EQUAL;
					case Nodes::Literal::LESS:
						return D3D11_COMPARISON_LESS;
					case Nodes::Literal::LESSEQUAL:
						return D3D11_COMPARISON_LESS_EQUAL;
					case Nodes::Literal::GREATER:
						return D3D11_COMPARISON_GREATER;
					case Nodes::Literal::GREATEREQUAL:
						return D3D11_COMPARISON_GREATER_EQUAL;
				}
			}
			static D3D11_STENCIL_OP								ConvertLiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::KEEP:
						return D3D11_STENCIL_OP_KEEP;
					case Nodes::Literal::ZERO:
						return D3D11_STENCIL_OP_ZERO;
					case Nodes::Literal::REPLACE:
						return D3D11_STENCIL_OP_REPLACE;
					case Nodes::Literal::INCR:
						return D3D11_STENCIL_OP_INCR;
					case Nodes::Literal::INCRSAT:
						return D3D11_STENCIL_OP_INCR_SAT;
					case Nodes::Literal::DECR:
						return D3D11_STENCIL_OP_DECR;
					case Nodes::Literal::DECRSAT:
						return D3D11_STENCIL_OP_DECR_SAT;
					case Nodes::Literal::INVERT:
						return D3D11_STENCIL_OP_INVERT;
				}
			}
			static D3D11_BLEND									ConvertLiteralToBlend(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ZERO:
						return D3D11_BLEND_ZERO;
					case Nodes::Literal::ONE:
						return D3D11_BLEND_ONE;
					case Nodes::Literal::SRCCOLOR:
						return D3D11_BLEND_SRC_COLOR;
					case Nodes::Literal::SRCALPHA:
						return D3D11_BLEND_SRC_ALPHA;
					case Nodes::Literal::INVSRCCOLOR:
						return D3D11_BLEND_INV_SRC_COLOR;
					case Nodes::Literal::INVSRCALPHA:
						return D3D11_BLEND_INV_SRC_ALPHA;
					case Nodes::Literal::DESTCOLOR:
						return D3D11_BLEND_DEST_COLOR;
					case Nodes::Literal::DESTALPHA:
						return D3D11_BLEND_DEST_ALPHA;
					case Nodes::Literal::INVDESTCOLOR:
						return D3D11_BLEND_INV_DEST_COLOR;
					case Nodes::Literal::INVDESTALPHA:
						return D3D11_BLEND_INV_DEST_ALPHA;
				}
			}
			static D3D11_BLEND_OP								ConvertLiteralToBlendOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ADD:
						return D3D11_BLEND_OP_ADD;
					case Nodes::Literal::SUBSTRACT:
						return D3D11_BLEND_OP_SUBTRACT;
					case Nodes::Literal::REVSUBSTRACT:
						return D3D11_BLEND_OP_REV_SUBTRACT;
					case Nodes::Literal::MIN:
						return D3D11_BLEND_OP_MIN;
					case Nodes::Literal::MAX:
						return D3D11_BLEND_OP_MAX;
				}
			}
			static DXGI_FORMAT									ConvertLiteralToFormat(int value, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case Nodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						return DXGI_FORMAT_R8_UNORM;
					case Nodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						return DXGI_FORMAT_R32_FLOAT;
					case Nodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						return DXGI_FORMAT_R8G8_UNORM;
					case Nodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						return DXGI_FORMAT_R8G8B8A8_TYPELESS;
					case Nodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						return DXGI_FORMAT_R16G16B16A16_UNORM;
					case Nodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						return DXGI_FORMAT_R16G16B16A16_FLOAT;
					case Nodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						return DXGI_FORMAT_R32G32B32A32_FLOAT;
					case Nodes::Literal::DXT1:
						format = Effect::Texture::Format::DXT1;
						return DXGI_FORMAT_BC1_TYPELESS;
					case Nodes::Literal::DXT3:
						format = Effect::Texture::Format::DXT3;
						return DXGI_FORMAT_BC2_TYPELESS;
					case Nodes::Literal::DXT5:
						format = Effect::Texture::Format::DXT5;
						return DXGI_FORMAT_BC3_TYPELESS;
					case Nodes::Literal::LATC1:
						format = Effect::Texture::Format::LATC1;
						return DXGI_FORMAT_BC4_UNORM;
					case Nodes::Literal::LATC2:
						format = Effect::Texture::Format::LATC2;
						return DXGI_FORMAT_BC5_UNORM;
					default:
						format = Effect::Texture::Format::Unknown;
						return DXGI_FORMAT_UNKNOWN;
				}
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
				if (type.HasQualifier(Nodes::Type::Qualifier::Extern))
					this->mCurrentSource += "extern ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Static) || (type.HasQualifier(Nodes::Type::Qualifier::Const) && !this->mCurrentInParameterBlock && !this->mCurrentInFunctionBlock))
					this->mCurrentSource += "static ";
				if (type.HasQualifier(Nodes::Type::Qualifier::GroupShared))
					this->mCurrentSource += "groupshared ";
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
				if (type.HasQualifier(Nodes::Type::Qualifier::Unorm))
					this->mCurrentSource += "unorm ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::Snorm))
					this->mCurrentSource += "snorm ";

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
						this->mCurrentSource += "int";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Uint:
						this->mCurrentSource += "uint";

						if (type.IsMatrix())
							this->mCurrentSource += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += std::to_string(type.Rows);
						break;
					case Nodes::Type::Half:
						this->mCurrentSource += "min16float";

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
						this->mCurrentSource += "__sampler1D";
						break;
					case Nodes::Type::Sampler2D:
						this->mCurrentSource += "__sampler2D";
						break;
					case Nodes::Type::Sampler3D:
						this->mCurrentSource += "__sampler3D";
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
						this->mCurrentSource += '~';
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
				this->mCurrentSource += '(';
				Visit(this->mAST[data.Left]);

				if (data.Operator == Nodes::Operator::Index)
				{
					this->mCurrentSource += '[';
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ']';
				}
				else
				{
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
						case Nodes::Operator::LeftShift:
							this->mCurrentSource += "<<";
							break;
						case Nodes::Operator::RightShift:
							this->mCurrentSource += ">>";
							break;
						case Nodes::Operator::BitwiseAnd:
							this->mCurrentSource += '&';
							break;
						case Nodes::Operator::BitwiseXor:
							this->mCurrentSource += '^';
							break;
						case Nodes::Operator::BitwiseOr:
							this->mCurrentSource += '|';
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
				}

				this->mCurrentSource += ')';
			}
			void												VisitAssignmentExpression(const Nodes::Assignment &data)
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
					case Nodes::Operator::LeftShift:
						this->mCurrentSource += "<<=";
						break;
					case Nodes::Operator::RightShift:
						this->mCurrentSource += ">>=";
						break;
					case Nodes::Operator::BitwiseAnd:
						this->mCurrentSource += "&=";
						break;
					case Nodes::Operator::BitwiseXor:
						this->mCurrentSource += "^=";
						break;
					case Nodes::Operator::BitwiseOr:
						this->mCurrentSource += "|=";
						break;
					case Nodes::Operator::None:
						this->mCurrentSource += '=';
						break;
				}

				this->mCurrentSource += ' ';
				Visit(this->mAST[data.Right]);
				this->mCurrentSource += ')';
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

				if (::strcmp(callee.Name, "tex1D") == 0 ||
					::strcmp(callee.Name, "tex1Dlod") == 0 ||
					::strcmp(callee.Name, "tex1Dfetch") == 0 ||
					::strcmp(callee.Name, "tex1Dbias") == 0 ||
					::strcmp(callee.Name, "tex2D") == 0 ||
					::strcmp(callee.Name, "tex2Dlod") == 0 ||
					::strcmp(callee.Name, "tex2Dfetch") == 0 ||
					::strcmp(callee.Name, "tex2Dbias") == 0 ||
					::strcmp(callee.Name, "tex3D") == 0 ||
					::strcmp(callee.Name, "tex3Dlod") == 0 ||
					::strcmp(callee.Name, "tex3Dfetch") == 0 ||
					::strcmp(callee.Name, "tex3Dbias") == 0)
				{
					this->mCurrentSource += "__";
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
						switch (data.Attributes)
						{
							case Nodes::Attribute::Flatten:
								this->mCurrentSource += "[flatten] ";
								break;
							case Nodes::Attribute::Branch:
								this->mCurrentSource += "[branch] ";
								break;
							case Nodes::Attribute::ForceCase:
								this->mCurrentSource += "[forcecase] ";
								break;
						}

						this->mCurrentSource += "switch (";
						Visit(this->mAST[data.Condition]);
						this->mCurrentSource += ")\n{\n";
						Visit(this->mAST[data.Statement]);
						this->mCurrentSource += "}\n";
						break;
					}
					case Nodes::Selection::Case:
					{
						const auto &cases = this->mAST[data.Condition].As<Nodes::Aggregate>();

						for (size_t i = 0; i < cases.Length; ++i)
						{
							const auto &item = this->mAST[cases.Find(this->mAST, i)];

							if (item.Is<Nodes::Expression>())
							{
								this->mCurrentSource += "default";
							}
							else
							{
								this->mCurrentSource += "case ";
								Visit(item);
							}

							this->mCurrentSource += ":\n";
						}

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
						this->mCurrentSource += "return";

						if (data.Expression != 0)
						{
							this->mCurrentSource += ' ';
							Visit(this->mAST[data.Expression]);
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
				const char *textureName = nullptr;
				const D3D11Texture *texture = nullptr;
				CD3D11_SAMPLER_DESC desc(D3D11_DEFAULT);
				bool sRGB = false;

				if (data.HasInitializer())
				{
					#pragma region Fill Description
					UINT filterMin = Nodes::Literal::LINEAR, filterMag = Nodes::Literal::LINEAR, filterMip = Nodes::Literal::LINEAR;

					const auto &states = this->mAST[data.Initializer].As<Nodes::Aggregate>();

					for (size_t i = 0; i < states.Length; ++i)
					{
						const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();
					
						switch (state.Type)
						{
							case Nodes::State::Texture:
								textureName = this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name;
								texture = this->mEffect->mTextures.at(textureName).get();
								break;
							case Nodes::State::MinFilter:
								filterMin = state.Value.AsInt;
								break;
							case Nodes::State::MagFilter:
								filterMag = state.Value.AsInt;
								break;
							case Nodes::State::MipFilter:
								filterMip = state.Value.AsInt;
								break;
							case Nodes::State::AddressU:
							case Nodes::State::AddressV:
							case Nodes::State::AddressW:
								(&desc.AddressU)[state.Type - Nodes::State::AddressU] = ConvertLiteralToTextureAddress(state.Value.AsInt);
								break;
							case Nodes::State::MipLODBias:
								desc.MipLODBias = state.Value.AsFloat; // << Make sure this is a float
								break;
							case Nodes::State::MinLOD:
								desc.MinLOD = state.Value.AsFloat;
								break;
							case Nodes::State::MaxLOD:
								desc.MaxLOD = state.Value.AsFloat;
								break;
							case Nodes::State::MaxAnisotropy:
								desc.MaxAnisotropy = state.Value.AsInt;
								break;
							case Nodes::State::SRGB:
								sRGB = state.Value.AsInt != 0;
								break;
						}
					}

					if (filterMin == Nodes::Literal::ANISOTROPIC || filterMag == Nodes::Literal::ANISOTROPIC || filterMip == Nodes::Literal::ANISOTROPIC)
					{
						desc.Filter = D3D11_FILTER_ANISOTROPIC;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
					}
					else
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
					}
					#pragma endregion
				}

				if (texture == nullptr)
				{
					return;
				}

				auto it = this->mEffect->mSamplerDescs.find(desc);

				if (it == this->mEffect->mSamplerDescs.end())
				{
					ID3D11SamplerState *sampler;

					HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateSamplerState(&desc, &sampler);

					if (FAILED(hr))
					{
						this->mFatal = true;
						return;
					}

					this->mEffect->mSamplerStates.push_back(sampler);
					it = this->mEffect->mSamplerDescs.insert(std::make_pair(desc, this->mEffect->mSamplerStates.size() - 1)).first;

					this->mCurrentSource += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
				}

				this->mCurrentSource += "static const __sampler" + std::to_string(texture->mDimension) + "D ";
				this->mCurrentSource += data.Name;
				this->mCurrentSource += " = { ";

				if (sRGB)
				{
					this->mCurrentSource += "__SRGB_";
				}

				this->mCurrentSource += textureName;
				this->mCurrentSource += ", __SamplerState" + std::to_string(it->second) + " };\n";
			}
			void												VisitTextureDeclaration(const Nodes::Variable &data)
			{
				ID3D11Resource *texture = nullptr;
				ID3D11ShaderResourceView *shaderresource[2] = { nullptr, nullptr };
				UINT width = 1, height = 1, depth = 1, levels = 1;
				DXGI_FORMAT format1 = DXGI_FORMAT_R8G8B8A8_TYPELESS;
				D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
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
					{
						this->mCurrentSource += "Texture1D";

						CD3D11_TEXTURE1D_DESC desc(format1, width, 1, levels, D3D11_BIND_SHADER_RESOURCE);
						srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
						srvdesc.Texture1D.MostDetailedMip = 0;
						srvdesc.Texture1D.MipLevels = levels;

						hr = this->mEffect->mEffectContext->mDevice->CreateTexture1D(&desc, nullptr, reinterpret_cast<ID3D11Texture1D **>(&texture));
						break;
					}
					case Nodes::Type::Texture2D:
					{
						this->mCurrentSource += "Texture2D";

						CD3D11_TEXTURE2D_DESC desc(format1, width, height, 1, levels, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
						srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
						srvdesc.Texture2D.MostDetailedMip = 0;
						srvdesc.Texture2D.MipLevels = levels;

						hr = this->mEffect->mEffectContext->mDevice->CreateTexture2D(&desc, nullptr, reinterpret_cast<ID3D11Texture2D **>(&texture));
						break;
					}
					case Nodes::Type::Texture3D:
					{
						this->mCurrentSource += "Texture3D";

						CD3D11_TEXTURE3D_DESC desc(format1, width, height, depth, levels, D3D11_BIND_SHADER_RESOURCE);
						srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
						srvdesc.Texture3D.MostDetailedMip = 0;
						srvdesc.Texture3D.MipLevels = levels;

						hr = this->mEffect->mEffectContext->mDevice->CreateTexture3D(&desc, nullptr, reinterpret_cast<ID3D11Texture3D **>(&texture));
						break;
					}
				}

				this->mCurrentSource += ' ';

				if (SUCCEEDED(hr))
				{
					srvdesc.Format = TypelessToLinearFormat(format1);
					hr = this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(texture, &srvdesc, &shaderresource[0]);

					this->mCurrentSource += data.Name;
					this->mCurrentSource += " : register(t" + std::to_string(this->mEffect->mShaderResources.size()) + ")";

					if (FAILED(hr))
					{
						texture->Release();
						return;
					}

					srvdesc.Format = TypelessToSRGBFormat(format1);

					if (srvdesc.Format != format1)
					{
						this->mCurrentSource += ", __SRGB_";
						this->mCurrentSource += data.Name;
						this->mCurrentSource += " : register(t" + std::to_string(this->mEffect->mShaderResources.size() + 1) + ")";

						hr = this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(texture, &srvdesc, &shaderresource[1]);

						if (FAILED(hr))
						{
							shaderresource[0]->Release();
							texture->Release();
						}
					}
				}

				this->mCurrentSource += ";\n";

				if (SUCCEEDED(hr))
				{
					auto obj = std::unique_ptr<D3D11Texture>(new D3D11Texture(this->mEffect));
					obj->mDesc.Width = width;
					obj->mDesc.Height = height;
					obj->mDesc.Depth = depth;
					obj->mDesc.Levels = levels;
					obj->mDesc.Format = format2;
					obj->mDimension = data.Type.Class - Nodes::Type::Texture1D + 1;
					obj->mRegister = this->mEffect->mShaderResources.size();
					obj->mTexture = texture;
					obj->mShaderResourceView[0] = shaderresource[0];
					obj->mShaderResourceView[1] = shaderresource[1];

					if (data.Annotations != 0)
					{
						const auto &annotations = this->mAST[data.Annotations].As<Nodes::Aggregate>();

						for (size_t i = 0; i < annotations.Length; ++i)
						{
							VisitAnnotation(this->mAST[annotations.Find(this->mAST, i)].As<Nodes::Annotation>(), obj->mAnnotations);
						}
					}

					this->mEffect->mTextures.insert(std::make_pair(data.Name, std::move(obj)));
					this->mEffect->mShaderResources.push_back(shaderresource[0]);

					if (shaderresource[1] != nullptr)
					{
						this->mEffect->mShaderResources.push_back(shaderresource[1]);
					}
				}
				else
				{
					this->mFatal = true;
				}
			}
			void												VisitUniformSingleDeclaration(const Nodes::Variable &data)
			{
				const std::string temp = this->mCurrentSource;
				this->mCurrentSource.clear();
				VisitType(data.Type);
				this->mCurrentGlobalConstants += this->mCurrentSource;
				this->mCurrentSource = temp;

				if (data.Name != nullptr)
				{
					this->mCurrentGlobalConstants += ' ';

					if (!this->mCurrentBlockName.empty())
					{
						this->mCurrentGlobalConstants += this->mCurrentBlockName + '_';
					}
				
					this->mCurrentGlobalConstants += data.Name;
				}

				if (data.Type.IsArray())
				{
					for (size_t i = 0; i < data.Type.ElementsDimension; ++i)
					{
						this->mCurrentGlobalConstants += '[';
						this->mCurrentGlobalConstants += (data.Type.Elements[i] > 0) ? std::to_string(data.Type.Elements[i]) : "";
						this->mCurrentGlobalConstants += ']';
					}
				}

				this->mCurrentGlobalConstants += ";\n";

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

				const UINT alignment = 16 - (this->mCurrentGlobalSize % 16);
				this->mCurrentGlobalSize += (desc.Size > alignment && (alignment != 16 || desc.Size <= 16)) ? desc.Size + alignment : desc.Size;

				auto obj = std::unique_ptr<D3D11Constant>(new D3D11Constant(this->mEffect));
				obj->mDesc = desc;
				obj->mBuffer = 0;
				obj->mBufferOffset = this->mCurrentGlobalSize - desc.Size;

				if (this->mCurrentGlobalSize >= this->mCurrentGlobalStorageSize)
				{
					this->mEffect->mConstantStorages[0] = static_cast<unsigned char *>(::realloc(this->mEffect->mConstantStorages[0], this->mCurrentGlobalStorageSize += 128));
				}

				if (data.Initializer && this->mAST[data.Initializer].Is<Nodes::Literal>())
				{
					std::memcpy(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, &this->mAST[data.Initializer].As<Nodes::Literal>().Value, desc.Size);
				}
				else
				{
					std::memset(this->mEffect->mConstantStorages[0] + obj->mBufferOffset, 0, desc.Size);
				}

				this->mEffect->mConstants.insert(std::make_pair(data.Name, std::move(obj)));
			}
			void												VisitUniformBufferDeclaration(const Nodes::Variable &data)
			{
				const auto &structure = this->mAST[data.Type.Definition].As<Nodes::Struct>();

				if (structure.Fields == 0)
				{
					return;
				}

				this->mCurrentSource += "cbuffer ";
				this->mCurrentSource += data.Name;
				this->mCurrentSource += " : register(b" + std::to_string(this->mEffect->mConstantBuffers.size()) + ")";
				this->mCurrentSource += "\n{\n";
				this->mCurrentBlockName = data.Name;

				ID3D11Buffer *buffer;
				unsigned char *storage = nullptr;
				UINT totalsize = 0, currentsize = 0;

				const auto &fields = this->mAST[structure.Fields].As<Nodes::Aggregate>();

				for (size_t i = 0; i < fields.Length; ++i)
				{
					const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

					VisitVariableDeclaration(field);

					Effect::Constant::Description desc;
					desc.Rows = field.Type.Rows;
					desc.Columns = field.Type.Cols;
					desc.Elements = field.Type.Elements[0] + field.Type.Elements[1] + field.Type.Elements[2] + field.Type.Elements[3] + field.Type.Elements[4] + field.Type.Elements[5] + field.Type.Elements[6] + field.Type.Elements[7] + field.Type.Elements[8] + field.Type.Elements[9] + field.Type.Elements[10] + field.Type.Elements[11] + field.Type.Elements[12] + field.Type.Elements[13] + field.Type.Elements[14] + field.Type.Elements[15];
					desc.Fields = 0;
					desc.Size = std::max(field.Type.Rows, 1U) * std::max(field.Type.Cols, 1U);

					switch (field.Type.Class)
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

					const UINT alignment = 16 - (totalsize % 16);
					totalsize += (desc.Size > alignment && (alignment != 16 || desc.Size <= 16)) ? desc.Size + alignment : desc.Size;

					auto obj = std::unique_ptr<D3D11Constant>(new D3D11Constant(this->mEffect));
					obj->mDesc = desc;
					obj->mBuffer = this->mEffect->mConstantBuffers.size();
					obj->mBufferOffset = totalsize - desc.Size;

					if (totalsize >= currentsize)
					{
						storage = static_cast<unsigned char *>(::realloc(storage, currentsize += 128));
					}

					if (field.Initializer && this->mAST[field.Initializer].Is<Nodes::Literal>())
					{
						std::memcpy(storage + obj->mBufferOffset, &this->mAST[field.Initializer].As<Nodes::Literal>().Value, desc.Size);
					}
					else
					{
						std::memset(storage + obj->mBufferOffset, 0, desc.Size);
					}

					this->mEffect->mConstants.insert(std::make_pair(std::string(data.Name) + '.' + std::string(field.Name), std::move(obj)));
				}

				auto obj = std::unique_ptr<D3D11Constant>(new D3D11Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fields.Length;
				obj->mDesc.Size = totalsize;
				obj->mDesc.Type = Effect::Constant::Type::Struct;
				obj->mBuffer = this->mEffect->mConstantBuffers.size();
				obj->mBufferOffset = 0;

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
				this->mCurrentSource += "};\n";

				CD3D11_BUFFER_DESC desc(RoundToMultipleOf16(totalsize), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
				D3D11_SUBRESOURCE_DATA initial;
				initial.pSysMem = storage;
				initial.SysMemPitch = initial.SysMemSlicePitch = totalsize;

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateBuffer(&desc, &initial, &buffer);

				if (SUCCEEDED(hr))
				{
					this->mEffect->mConstantBuffers.push_back(buffer);
					this->mEffect->mConstantStorages.push_back(storage);
				}
			}
			void												VisitFunctionDeclaration(const Nodes::Function &data)
			{
				auto type = data.ReturnType;
				type.Qualifiers &= ~static_cast<int>(Nodes::Type::Qualifier::Const);
				VisitType(type);

				this->mCurrentSource += ' ';
				this->mCurrentSource += data.Name;
				this->mCurrentSource += '(';

				if (data.HasArguments())
				{
					const auto &arguments = this->mAST[data.Arguments].As<Nodes::Aggregate>();

					this->mCurrentInParameterBlock = true;

					for (size_t i = 0; i < arguments.Length; ++i)
					{
						VisitVariableDeclaration(this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>());

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
					this->mCurrentSource += data.ReturnSemantic;
				}

				if (data.HasDefinition())
				{
					this->mCurrentSource += '\n';
					this->mCurrentInFunctionBlock = true;

					Visit(this->mAST[data.Definition]);

					this->mCurrentInFunctionBlock = false;
				}
				else
				{
					this->mCurrentSource += ";\n";
				}
			}
			void												VisitTechniqueDeclaration(const Nodes::Technique &data)
			{
				auto obj = std::unique_ptr<D3D11Technique>(new D3D11Technique(this->mEffect));

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
			void												VisitPassDeclaration(const Nodes::Pass &data, std::vector<D3D11Technique::Pass> &passes)
			{
				if (data.States == 0)
				{
					return;
				}

				D3D11Technique::Pass info;
				info.VS = nullptr;
				info.PS = nullptr;
				info.BS = nullptr;
				info.DSS = nullptr;
				info.RS = nullptr;
				ZeroMemory(info.RT, sizeof(info.RT));
				CD3D11_RASTERIZER_DESC rdesc(D3D11_DEFAULT);
				CD3D11_DEPTH_STENCIL_DESC ddesc(D3D11_DEFAULT);
				CD3D11_BLEND_DESC bdesc(D3D11_DEFAULT);
				int srgb = 0;
				ID3D11RenderTargetView *targets[8][2] = { this->mEffect->mBackBufferTarget, this->mEffect->mBackBufferTarget };
				info.SR = this->mEffect->mShaderResources;

				const auto &states = this->mAST[data.States].As<Nodes::Aggregate>();

				for (size_t i = 0; i < states.Length; ++i)
				{
					const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();

					switch (state.Type)
					{
						case Nodes::State::VertexShader:
						case Nodes::State::PixelShader:
							VisitPassShaderDeclaration(state, info);
							break;
						case Nodes::State::RenderTarget0:
						case Nodes::State::RenderTarget1:
						case Nodes::State::RenderTarget2:
						case Nodes::State::RenderTarget3:
						case Nodes::State::RenderTarget4:
						case Nodes::State::RenderTarget5:
						case Nodes::State::RenderTarget6:
						case Nodes::State::RenderTarget7:
						{
							const char *name = this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name;
							auto texture = this->mEffect->mTextures.at(name).get();
							D3D11_TEXTURE2D_DESC desc;
							texture->mTexture2D->GetDesc(&desc);
							D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
							rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
							rtvdesc.Texture2D.MipSlice = 0;

							if (texture->mRenderTargetView[0] == nullptr)
							{
								rtvdesc.Format = TypelessToLinearFormat(desc.Format);
								this->mEffect->mEffectContext->mDevice->CreateRenderTargetView(texture->mTexture, &rtvdesc, &texture->mRenderTargetView[0]);
							}
							if (texture->mRenderTargetView[1] == nullptr)
							{
								rtvdesc.Format = TypelessToSRGBFormat(desc.Format);

								if (desc.Format != desc.Format)
								{
									this->mEffect->mEffectContext->mDevice->CreateRenderTargetView(texture->mTexture, &rtvdesc, &texture->mRenderTargetView[1]);
								}
							}

							targets[state.Type - Nodes::State::RenderTarget0][0] = texture->mRenderTargetView[0];
							targets[state.Type - Nodes::State::RenderTarget0][1] = texture->mRenderTargetView[1];
							break;
						}
						case Nodes::State::RenderTargetWriteMask:
							bdesc.RenderTarget[0].RenderTargetWriteMask = static_cast<UINT8>(state.Value.AsInt);
							break;
						case Nodes::State::CullMode:
							rdesc.CullMode = ConvertLiteralToCullMode(state.Value.AsInt);
							break;
						case Nodes::State::FillMode:
							rdesc.FillMode = ConvertLiteralToFillMode(state.Value.AsInt);
							break;
						case Nodes::State::ScissorEnable:
							rdesc.ScissorEnable = state.Value.AsInt;
							break;
						case Nodes::State::DepthEnable:
							ddesc.DepthEnable = state.Value.AsInt;
							break;
						case Nodes::State::DepthFunc:
							ddesc.DepthFunc = ConvertLiteralToComparison(state.Value.AsInt);
							break;
						case Nodes::State::DepthWriteMask:
							ddesc.DepthWriteMask = state.Value.AsInt ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
							break;
						case Nodes::State::StencilEnable:
							ddesc.StencilEnable = state.Value.AsInt;
							break;
						case Nodes::State::StencilReadMask:
							ddesc.StencilReadMask = static_cast<UINT8>(state.Value.AsInt & 0xFF);
							break;
						case Nodes::State::StencilWriteMask:
							ddesc.StencilWriteMask = static_cast<UINT8>(state.Value.AsInt & 0xFF);
							break;
						case Nodes::State::StencilFunc:
							ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = ConvertLiteralToComparison(state.Value.AsInt);
							break;
						case Nodes::State::StencilPass:
							ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::StencilFail:
							ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::StencilZFail:
							ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::AlphaToCoverageEnable:
							bdesc.AlphaToCoverageEnable = state.Value.AsInt;
							break;
						case Nodes::State::BlendEnable:
							bdesc.RenderTarget[0].BlendEnable = state.Value.AsInt;
							break;
						case Nodes::State::BlendOp:
							bdesc.RenderTarget[0].BlendOp = ConvertLiteralToBlendOp(state.Value.AsInt);
							break;
						case Nodes::State::BlendOpAlpha:
							bdesc.RenderTarget[0].BlendOpAlpha = ConvertLiteralToBlendOp(state.Value.AsInt);
							break;
						case Nodes::State::SrcBlend:
							bdesc.RenderTarget[0].SrcBlend = ConvertLiteralToBlend(state.Value.AsInt);
							break;
						case Nodes::State::DestBlend:
							bdesc.RenderTarget[0].DestBlend = ConvertLiteralToBlend(state.Value.AsInt);
							break;
						case Nodes::State::SRGBWriteEnable:
							srgb = state.Value.AsInt;
							break;
					}
				}

				this->mEffect->mEffectContext->mDevice->CreateRasterizerState(&rdesc, &info.RS);
				this->mEffect->mEffectContext->mDevice->CreateDepthStencilState(&ddesc, &info.DSS);
				this->mEffect->mEffectContext->mDevice->CreateBlendState(&bdesc, &info.BS);

				for (size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					info.RT[i] = targets[i][srgb];
				}
				for (auto it = info.SR.begin(), end = info.SR.end(); it != end; ++it)
				{
					ID3D11Resource *res1, *res2;
					(*it)->GetResource(&res1);

					for (size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					{
						if (info.RT[i] == nullptr)
						{
							continue;
						}

						info.RT[i]->GetResource(&res2);

						if (res1 == res2)
						{
							*it = nullptr;
							break;
						}

						res2->Release();
					}

					res1->Release();
				}

				passes.push_back(info);
			}
			void												VisitPassShaderDeclaration(const Nodes::State &state, D3D11Technique::Pass &info)
			{
				std::string profile;

				switch (this->mEffect->mEffectContext->mDevice->GetFeatureLevel())
				{
					default:
					case D3D_FEATURE_LEVEL_11_0:
						profile = "5_0";
						break;
					case D3D_FEATURE_LEVEL_10_1:
						profile = "4_1";
						break;
					case D3D_FEATURE_LEVEL_10_0:
						profile = "4_0";
						break;
					case D3D_FEATURE_LEVEL_9_1:
					case D3D_FEATURE_LEVEL_9_2:
						profile = "4_0_level_9_1";
						break;
					case D3D_FEATURE_LEVEL_9_3:
						profile = "4_0_level_9_3";
						break;
				}
				switch (state.Type)
				{
					case Nodes::State::VertexShader:
						profile = "vs_" + profile;
						break;
					case Nodes::State::PixelShader:
						profile = "ps_" + profile;
						break;
					default:
						return;
				}

				UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
				flags |= D3DCOMPILE_DEBUG;
#endif

				std::string source =
					"struct __sampler1D { Texture1D t; SamplerState s; };\n"
					"inline float4 __tex1D(__sampler1D s, float c) { return s.t.Sample(s.s, c); }\n"
					"inline float4 __tex1Doffset(__sampler1D s, float c, int offset) { return s.t.Sample(s.s, c, offset); }\n"
					"inline float4 __tex1Dlod(__sampler1D s, float4 c) { return s.t.SampleLevel(s.s, c.x, c.w); }\n"
					"inline float4 __tex1Dfetch(__sampler1D s, int4 c) { return s.t.Load(c.xw); }\n"
					"inline float4 __tex1Dbias(__sampler1D s, float4 c) { return s.t.SampleBias(s.s, c.x, c.w); }\n"
					"inline int __tex1Dsize(__sampler1D s, int lod) { uint w, l; s.t.GetDimensions(lod, w, l); return w; }\n"
					"struct __sampler2D { Texture2D t; SamplerState s; };\n"
					"inline float4 __tex2D(__sampler2D s, float2 c) { return s.t.Sample(s.s, c); }\n"
					"inline float4 __tex2Doffset(__sampler2D s, float2 c, int offset) { return s.t.Sample(s.s, c, offset.xx); }\n"
					"inline float4 __tex2Dlod(__sampler2D s, float4 c) { return s.t.SampleLevel(s.s, c.xy, c.w); }\n"
					"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
					"inline float4 __tex2Dbias(__sampler2D s, float4 c) { return s.t.SampleBias(s.s, c.xy, c.w); }\n"
					"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n"
					"struct __sampler3D { Texture3D t; SamplerState s; };\n"
					"inline float4 __tex3D(__sampler3D s, float3 c) { return s.t.Sample(s.s, c); }\n"
					"inline float4 __tex3Doffset(__sampler3D s, float3 c, int offset) { return s.t.Sample(s.s, c, offset.xxx); }\n"
					"inline float4 __tex3Dlod(__sampler3D s, float4 c) { return s.t.SampleLevel(s.s, c.xyz, c.w); }\n"
					"inline float4 __tex3Dfetch(__sampler3D s, int4 c) { return s.t.Load(c.xyzw); }\n"
					"inline float4 __tex3Dbias(__sampler3D s, float4 c) { return s.t.SampleBias(s.s, c.xyz, c.w); }\n"
					"inline int3 __tex3Dsize(__sampler3D s, int lod) { uint w, h, d, l; s.t.GetDimensions(lod, w, h, d, l); return int3(w, h, d); }\n"
					"cbuffer __GLOBAL__ : register(b0)\n{\n" + this->mCurrentGlobalConstants + "};\n" + this->mCurrentSource;

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
						hr = this->mEffect->mEffectContext->mDevice->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &info.VS);
						break;
					case Nodes::State::PixelShader:
						hr = this->mEffect->mEffectContext->mDevice->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &info.PS);
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
			D3D11Effect *										mEffect;
			std::string											mCurrentSource;
			std::string &										mErrors;
			bool												mFatal;
			std::string											mCurrentGlobalConstants;
			size_t												mCurrentGlobalSize, mCurrentGlobalStorageSize;
			std::string											mCurrentBlockName;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
		};

		// -----------------------------------------------------------------------------------------------------

		D3D11EffectContext::D3D11EffectContext(ID3D11Device *device, IDXGISwapChain *swapchain) : mDevice(device), mSwapChain(swapchain)
		{
			this->mDevice->AddRef();
			this->mDevice->GetImmediateContext(&this->mImmediateContext);
			this->mSwapChain->AddRef();
		}
		D3D11EffectContext::~D3D11EffectContext(void)
		{
			this->mDevice->Release();
			this->mImmediateContext->Release();
			this->mSwapChain->Release();
		}

		void													D3D11EffectContext::GetDimension(unsigned int &width, unsigned int &height) const
		{
			ID3D11Texture2D *buffer;
			D3D11_TEXTURE2D_DESC desc;
			this->mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&buffer));
			buffer->GetDesc(&desc);
			buffer->Release();

			width = desc.Width;
			height = desc.Height;
		}

		std::unique_ptr<Effect>									D3D11EffectContext::Compile(const EffectTree &ast, std::string &errors)
		{
			D3D11Effect *effect = new D3D11Effect(shared_from_this());
			
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

		D3D11Effect::D3D11Effect(std::shared_ptr<D3D11EffectContext> context) : mEffectContext(context), mConstantsDirty(true)
		{
			context->mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&this->mBackBufferTexture));

			D3D11_TEXTURE2D_DESC dstdesc;
			this->mBackBufferTexture->GetDesc(&dstdesc);
			dstdesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
			dstdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
			D3D11_SHADER_RESOURCE_VIEW_DESC dssdesc;
			ZeroMemory(&dssdesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			dssdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			dssdesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			dssdesc.Texture2D.MipLevels = 1;
			D3D11_DEPTH_STENCIL_VIEW_DESC dsdesc;
			ZeroMemory(&dsdesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
			dsdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			context->mDevice->CreateTexture2D(&dstdesc, nullptr, &this->mDepthStencilTexture);
			context->mDevice->CreateShaderResourceView(this->mDepthStencilTexture, &dssdesc, &this->mDepthStencilView);
			context->mDevice->CreateDepthStencilView(this->mDepthStencilTexture, &dsdesc, &this->mDepthStencil);

			context->mDevice->CreateRenderTargetView(this->mBackBufferTexture, nullptr, &this->mBackBufferTarget);
		}
		D3D11Effect::~D3D11Effect(void)
		{
			this->mDepthStencil->Release();
			this->mDepthStencilView->Release();
			this->mDepthStencilTexture->Release();
			this->mBackBufferTarget->Release();
			this->mBackBufferTexture->Release();

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

		Effect::Texture *										D3D11Effect::GetTexture(const std::string &name)
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Texture *									D3D11Effect::GetTexture(const std::string &name) const
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D11Effect::GetTextureNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTextures.size());

			for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		Effect::Constant *										D3D11Effect::GetConstant(const std::string &name)
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Constant *								D3D11Effect::GetConstant(const std::string &name) const
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D11Effect::GetConstantNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mConstants.size());

			for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		Effect::Technique *										D3D11Effect::GetTechnique(const std::string &name)
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Technique *								D3D11Effect::GetTechnique(const std::string &name) const
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								D3D11Effect::GetTechniqueNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTechniques.size());

			for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}

		void													D3D11Effect::ApplyConstants(void) const
		{
			for (size_t i = 0, count = this->mConstantBuffers.size(); i < count; ++i)
			{
				ID3D11Buffer *buffer = this->mConstantBuffers[i];
				const unsigned char *storage = this->mConstantStorages[i];

				if (buffer == nullptr)
				{
					continue;
				}

				D3D11_MAPPED_SUBRESOURCE mapped;

				HRESULT hr = this->mEffectContext->mImmediateContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

				if (FAILED(hr))
				{
					continue;
				}

				std::memcpy(mapped.pData, storage, mapped.RowPitch);

				this->mEffectContext->mImmediateContext->Unmap(buffer, 0);
			}

			this->mConstantsDirty = false;
		}

		D3D11Texture::D3D11Texture(D3D11Effect *effect) : mEffect(effect), mTexture(nullptr), mShaderResourceView(), mRenderTargetView()
		{
		}
		D3D11Texture::~D3D11Texture(void)
		{
			SAFE_RELEASE(this->mRenderTargetView[0]);
			SAFE_RELEASE(this->mRenderTargetView[1]);
			SAFE_RELEASE(this->mShaderResourceView[0]);
			SAFE_RELEASE(this->mShaderResourceView[1]);
			SAFE_RELEASE(this->mTexture);
		}

		const Effect::Texture::Description						D3D11Texture::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D11Texture::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													D3D11Texture::Resize(const Effect::Texture::Description &desc)
		{
			HRESULT hr = E_FAIL;
			ID3D11Resource *texture = nullptr;
			ID3D11ShaderResourceView *shaderresource[2] = { nullptr, nullptr };
			ID3D11RenderTargetView *rendertarget[2] = { nullptr, nullptr };
			D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
			D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

			switch (desc.Format)
			{
				case Effect::Texture::Format::R8:
					format = DXGI_FORMAT_R8_UNORM;
					break;
				case Effect::Texture::Format::R32F:
					format = DXGI_FORMAT_R32_FLOAT;
					break;
				case Effect::Texture::Format::RG8:
					format = DXGI_FORMAT_R8G8_UNORM;
					break;
				case Effect::Texture::Format::RGBA8:
					format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
					break;
				case Effect::Texture::Format::RGBA16:
					format = DXGI_FORMAT_R16G16B16A16_UNORM;
					break;
				case Effect::Texture::Format::RGBA16F:
					format = DXGI_FORMAT_R16G16B16A16_FLOAT;
					break;
				case Effect::Texture::Format::RGBA32F:
					format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					break;
				case Texture::Format::DXT1:
					format = DXGI_FORMAT_BC1_TYPELESS;
					break;
				case Texture::Format::DXT3:
					format = DXGI_FORMAT_BC2_TYPELESS;
					break;
				case Texture::Format::DXT5:
					format = DXGI_FORMAT_BC3_TYPELESS;
					break;
				case Texture::Format::LATC1:
					format = DXGI_FORMAT_BC4_UNORM;
					break;
				case Texture::Format::LATC2:
					format = DXGI_FORMAT_BC5_UNORM;
					break;
			}

			switch (this->mDimension)
			{
				case 1:
				{
					CD3D11_TEXTURE1D_DESC desc(format, desc.Width, 1, desc.Levels, D3D11_BIND_SHADER_RESOURCE);
					srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
					srvdesc.Texture1D.MostDetailedMip = 0;
					srvdesc.Texture1D.MipLevels = desc.MipLevels;

					hr = this->mEffect->mEffectContext->mDevice->CreateTexture1D(&desc, nullptr, reinterpret_cast<ID3D11Texture1D **>(&texture));
					break;
				}
				case 2:
				{
					CD3D11_TEXTURE2D_DESC desc(format, desc.Width, desc.Height, 1, desc.Levels, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
					srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					srvdesc.Texture2D.MostDetailedMip = 0;
					srvdesc.Texture2D.MipLevels = desc.MipLevels;
					rtvdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					rtvdesc.Texture2D.MipSlice = 0;

					hr = this->mEffect->mEffectContext->mDevice->CreateTexture2D(&desc, nullptr, reinterpret_cast<ID3D11Texture2D **>(&texture));
					break;
				}
				case 3:
				{
					CD3D11_TEXTURE3D_DESC desc(format, desc.Width, desc.Height, desc.Depth, desc.Levels, D3D11_BIND_SHADER_RESOURCE);
					srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
					srvdesc.Texture3D.MostDetailedMip = 0;
					srvdesc.Texture3D.MipLevels = desc.MipLevels;

					hr = this->mEffect->mEffectContext->mDevice->CreateTexture3D(&desc, nullptr, reinterpret_cast<ID3D11Texture3D **>(&texture));
					break;
				}
			}
		
			if (FAILED(hr))
			{
				return false;
			}

			srvdesc.Format = rtvdesc.Format = TypelessToLinearFormat(format);

			if (this->mShaderResourceView[0] != nullptr)
			{
				hr = this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(texture, &srvdesc, &shaderresource[0]);

				if (FAILED(hr))
				{
					texture->Release();
					return false;
				}
			}
			if (this->mRenderTargetView[0] != nullptr)
			{
				hr = this->mEffect->mEffectContext->mDevice->CreateRenderTargetView(texture, &rtvdesc, &rendertarget[0]);

				if (FAILED(hr))
				{
					texture->Release();
					SAFE_RELEASE(shaderresource[0]);
					return false;
				}
			}

			srvdesc.Format = rtvdesc.Format = TypelessToSRGBFormat(format);

			if (this->mShaderResourceView[1] != nullptr)
			{
				if (srvdesc.Format == format || FAILED(this->mEffect->mEffectContext->mDevice->CreateShaderResourceView(texture, &srvdesc, &shaderresource[1])))
				{
					texture->Release();
					SAFE_RELEASE(shaderresource[0]);
					SAFE_RELEASE(rendertarget[0]);
					return false;
				}
			}
			if (this->mRenderTargetView[1] != nullptr)
			{
				hr = this->mEffect->mEffectContext->mDevice->CreateRenderTargetView(texture, &rtvdesc, &rendertarget[1]);

				if (FAILED(hr))
				{
					texture->Release();
					SAFE_RELEASE(shaderresource[0]);
					SAFE_RELEASE(shaderresource[1]);
					SAFE_RELEASE(rendertarget[0]);
					return false;
				}
			}

			for (auto &technique : this->mEffect->mTechniques)
			{
				for (auto &pass : technique.second->mPasses)
				{
					for (auto &srv : pass.SR)
					{
						if (srv == this->mShaderResourceView[0])
						{
							srv = shaderresource[0];
						}
						else if (srv == this->mShaderResourceView[1])
						{
							srv = shaderresource[1];
						}
					}
					for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					{
						if (pass.RT[i] == this->mRenderTargetView[0])
						{
							pass.RT[i] = rendertarget[0];
						}
						else if (pass.RT[i] == this->mRenderTargetView[1])
						{
							pass.RT[i] = rendertarget[1];
						}
					}
				}
			}

			SAFE_RELEASE(this->mTexture);
			SAFE_RELEASE(this->mShaderResourceView[0]);
			SAFE_RELEASE(this->mShaderResourceView[1]);
			SAFE_RELEASE(this->mRenderTargetView[0]);
			SAFE_RELEASE(this->mRenderTargetView[1]);

			this->mDesc = desc;
			this->mTexture = texture;
			this->mShaderResourceView[0] = shaderresource[0];
			this->mShaderResourceView[1] = shaderresource[1];
			this->mRenderTargetView[0] = rendertarget[0];
			this->mRenderTargetView[1] = rendertarget[1];

			this->mEffect->mShaderResources[this->mRegister] = shaderresource[0];
			
			if (shaderresource[1] != nullptr)
			{
				this->mEffect->mShaderResources[this->mRegister + 1] = shaderresource[1];
			}

			return true;
		}
		void													D3D11Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
		{
			assert(data != nullptr || size == 0);

			if (size == 0) return;

			this->mEffect->mEffectContext->mImmediateContext->UpdateSubresource(this->mTexture, level, nullptr, data, size / (this->mDesc.Height * this->mDesc.Depth), size / this->mDesc.Depth);
		}
		void													D3D11Texture::UpdateFromColorBuffer(void)
		{
			assert(this->mDimension == 2);
			D3D11_TEXTURE2D_DESC desc;
			this->mEffect->mBackBufferTexture->GetDesc(&desc);

			if (desc.SampleDesc.Count == 1)
			{
				this->mEffect->mEffectContext->mImmediateContext->CopyResource(this->mTexture, this->mEffect->mBackBufferTexture);
			}
			else
			{
				this->mEffect->mEffectContext->mImmediateContext->ResolveSubresource(this->mTexture, 0, this->mEffect->mBackBufferTexture, 0, desc.Format);
			}
		}
		void													D3D11Texture::UpdateFromDepthBuffer(void)
		{
		}

		D3D11Constant::D3D11Constant(D3D11Effect *effect) : mEffect(effect)
		{
		}
		D3D11Constant::~D3D11Constant(void)
		{
		}

		const Effect::Constant::Description						D3D11Constant::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D11Constant::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}
		void													D3D11Constant::GetValue(unsigned char *data, std::size_t size) const
		{
			size = std::min(size, this->mDesc.Size);

			const unsigned char *storage = this->mEffect->mConstantStorages[this->mBuffer] + this->mBufferOffset;

			std::memcpy(data, storage, size);
		}
		void													D3D11Constant::SetValue(const unsigned char *data, std::size_t size)
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

		D3D11Technique::D3D11Technique(D3D11Effect *effect) : mEffect(effect)
		{
			this->mEffect->mEffectContext->mDevice->CreateDeferredContext(0, &this->mDeferredContext);
		}
		D3D11Technique::~D3D11Technique(void)
		{
			SAFE_RELEASE(this->mDeferredContext);

			for (auto &pass : this->mPasses)
			{
				SAFE_RELEASE(pass.VS);
				SAFE_RELEASE(pass.RS);
				SAFE_RELEASE(pass.PS);
				SAFE_RELEASE(pass.BS);
			}
		}

		const Effect::Technique::Description					D3D11Technique::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								D3D11Technique::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													D3D11Technique::Begin(unsigned int &passes) const
		{
			passes = static_cast<unsigned int>(this->mPasses.size());

			if (passes == 0)
			{
				return false;
			}

			const uintptr_t null = 0;
			this->mDeferredContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			this->mDeferredContext->IASetInputLayout(nullptr);
			this->mDeferredContext->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D11Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

			this->mDeferredContext->VSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
			this->mDeferredContext->PSSetSamplers(0, this->mEffect->mSamplerStates.size(), this->mEffect->mSamplerStates.data());
			this->mDeferredContext->VSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());
			this->mDeferredContext->PSSetConstantBuffers(0, this->mEffect->mConstantBuffers.size(), this->mEffect->mConstantBuffers.data());

			this->mDeferredContext->ClearDepthStencilView(this->mEffect->mDepthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0x00);

			return true;
		}
		void													D3D11Technique::End(void) const
		{
			ID3D11CommandList *list;
			this->mDeferredContext->FinishCommandList(FALSE, &list);

			this->mEffect->mEffectContext->mImmediateContext->ExecuteCommandList(list, TRUE);
			list->Release();
		}
		void													D3D11Technique::RenderPass(unsigned int index) const
		{
			if (this->mEffect->mConstantsDirty)
			{
				this->mEffect->ApplyConstants();
			}

			const Pass &pass = this->mPasses[index];
			
			this->mDeferredContext->VSSetShader(pass.VS, nullptr, 0);
			this->mDeferredContext->VSSetShaderResources(0, pass.SR.size(), pass.SR.data());
			this->mDeferredContext->HSSetShader(nullptr, nullptr, 0);
			this->mDeferredContext->DSSetShader(nullptr, nullptr, 0);
			this->mDeferredContext->GSSetShader(nullptr, nullptr, 0);
			this->mDeferredContext->RSSetState(pass.RS);
			this->mDeferredContext->PSSetShader(pass.PS, nullptr, 0);
			this->mDeferredContext->PSSetShaderResources(0, pass.SR.size(), pass.SR.data());

			const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			this->mDeferredContext->OMSetBlendState(pass.BS, blendfactor, D3D11_DEFAULT_SAMPLE_MASK);
			this->mDeferredContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, this->mEffect->mDepthStencil);

			for (size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				if (pass.RT[i] == nullptr)
				{
					continue;
				}

				const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				this->mDeferredContext->ClearRenderTargetView(pass.RT[i], color);
			}

			ID3D11Resource *rtres;
			pass.RT[0]->GetResource(&rtres);
			D3D11_TEXTURE2D_DESC rtdesc;
			static_cast<ID3D11Texture2D *>(rtres)->GetDesc(&rtdesc);
			rtres->Release();

			D3D11_VIEWPORT viewport;
			viewport.Width = static_cast<FLOAT>(rtdesc.Width);
			viewport.Height = static_cast<FLOAT>(rtdesc.Height);
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			this->mDeferredContext->RSSetViewports(1, &viewport);

			this->mDeferredContext->Draw(3, 0);
		}
	}

	// -----------------------------------------------------------------------------------------------------

	std::shared_ptr<EffectContext>								CreateEffectContext(ID3D11Device *device, IDXGISwapChain *swapchain)
	{
		assert (device != nullptr && swapchain != nullptr);

		return std::make_shared<D3D11EffectContext>(device, swapchain);
	}
}