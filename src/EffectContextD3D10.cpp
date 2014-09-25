#include "Log.hpp"
#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectContext.hpp"

#include <d3d10_1.h>
#include <d3dcompiler.h>

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
			friend class ASTVisitor;

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
			ID3D10Texture2D *									mBackBufferTexture;
			ID3D10RenderTargetView *							mBackBufferTarget;
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
				ID3D10RasterizerState *							RS;
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

			bool												Traverse(D3D10Effect *effect)
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
					CD3D10_BUFFER_DESC globalsDesc(RoundToMultipleOf16(this->mCurrentGlobalSize), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
					D3D10_SUBRESOURCE_DATA globalsInitial;
					globalsInitial.pSysMem = this->mEffect->mConstantStorages[0];
					globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = this->mCurrentGlobalSize;
					this->mEffect->mEffectContext->mDevice->CreateBuffer(&globalsDesc, &globalsInitial, &this->mEffect->mConstantBuffers[0]);
				}

				return !this->mFatal;
			}

		private:
			void												operator =(const ASTVisitor &);

			static D3D10_TEXTURE_ADDRESS_MODE					ConvertLiteralToTextureAddress(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::CLAMP:
						return D3D10_TEXTURE_ADDRESS_CLAMP;
					case Nodes::Literal::REPEAT:
						return D3D10_TEXTURE_ADDRESS_WRAP;
					case Nodes::Literal::MIRROR:
						return D3D10_TEXTURE_ADDRESS_MIRROR;
					case Nodes::Literal::BORDER:
						return D3D10_TEXTURE_ADDRESS_BORDER;
				}
			}
			static D3D10_CULL_MODE								ConvertLiteralToCullMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::NONE:
						return D3D10_CULL_NONE;
					case Nodes::Literal::FRONT:
						return D3D10_CULL_FRONT;
					case Nodes::Literal::BACK:
						return D3D10_CULL_BACK;
				}
			}
			static D3D10_FILL_MODE								ConvertLiteralToFillMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::SOLID:
						return D3D10_FILL_SOLID;
					case Nodes::Literal::WIREFRAME:
						return D3D10_FILL_WIREFRAME;
				}
			}
			static D3D10_COMPARISON_FUNC						ConvertLiteralToComparison(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ALWAYS:
						return D3D10_COMPARISON_ALWAYS;
					case Nodes::Literal::NEVER:
						return D3D10_COMPARISON_NEVER;
					case Nodes::Literal::EQUAL:
						return D3D10_COMPARISON_EQUAL;
					case Nodes::Literal::NOTEQUAL:
						return D3D10_COMPARISON_NOT_EQUAL;
					case Nodes::Literal::LESS:
						return D3D10_COMPARISON_LESS;
					case Nodes::Literal::LESSEQUAL:
						return D3D10_COMPARISON_LESS_EQUAL;
					case Nodes::Literal::GREATER:
						return D3D10_COMPARISON_GREATER;
					case Nodes::Literal::GREATEREQUAL:
						return D3D10_COMPARISON_GREATER_EQUAL;
				}
			}
			static D3D10_STENCIL_OP								ConvertLiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::KEEP:
						return D3D10_STENCIL_OP_KEEP;
					case Nodes::Literal::ZERO:
						return D3D10_STENCIL_OP_ZERO;
					case Nodes::Literal::REPLACE:
						return D3D10_STENCIL_OP_REPLACE;
					case Nodes::Literal::INCR:
						return D3D10_STENCIL_OP_INCR;
					case Nodes::Literal::INCRSAT:
						return D3D10_STENCIL_OP_INCR_SAT;
					case Nodes::Literal::DECR:
						return D3D10_STENCIL_OP_DECR;
					case Nodes::Literal::DECRSAT:
						return D3D10_STENCIL_OP_DECR_SAT;
					case Nodes::Literal::INVERT:
						return D3D10_STENCIL_OP_INVERT;
				}
			}
			static D3D10_BLEND									ConvertLiteralToBlend(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ZERO:
						return D3D10_BLEND_ZERO;
					case Nodes::Literal::ONE:
						return D3D10_BLEND_ONE;
					case Nodes::Literal::SRCCOLOR:
						return D3D10_BLEND_SRC_COLOR;
					case Nodes::Literal::SRCALPHA:
						return D3D10_BLEND_SRC_ALPHA;
					case Nodes::Literal::INVSRCCOLOR:
						return D3D10_BLEND_INV_SRC_COLOR;
					case Nodes::Literal::INVSRCALPHA:
						return D3D10_BLEND_INV_SRC_ALPHA;
					case Nodes::Literal::DESTCOLOR:
						return D3D10_BLEND_DEST_COLOR;
					case Nodes::Literal::DESTALPHA:
						return D3D10_BLEND_DEST_ALPHA;
					case Nodes::Literal::INVDESTCOLOR:
						return D3D10_BLEND_INV_DEST_COLOR;
					case Nodes::Literal::INVDESTALPHA:
						return D3D10_BLEND_INV_DEST_ALPHA;
				}
			}
			static D3D10_BLEND_OP								ConvertLiteralToBlendOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ADD:
						return D3D10_BLEND_OP_ADD;
					case Nodes::Literal::SUBSTRACT:
						return D3D10_BLEND_OP_SUBTRACT;
					case Nodes::Literal::REVSUBSTRACT:
						return D3D10_BLEND_OP_REV_SUBTRACT;
					case Nodes::Literal::MIN:
						return D3D10_BLEND_OP_MIN;
					case Nodes::Literal::MAX:
						return D3D10_BLEND_OP_MAX;
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
					case Nodes::Type::Sampler:
						this->mCurrentSource += "__sampler2D";
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
					::strcmp(callee.Name, "tex1Doffset") == 0 ||
					::strcmp(callee.Name, "tex1Dlod") == 0 ||
					::strcmp(callee.Name, "tex1Dfetch") == 0 ||
					::strcmp(callee.Name, "tex1Dbias") == 0 ||
					::strcmp(callee.Name, "tex2D") == 0 ||
					::strcmp(callee.Name, "tex2Doffset") == 0 ||
					::strcmp(callee.Name, "tex2Dlod") == 0 ||
					::strcmp(callee.Name, "tex2Dfetch") == 0 ||
					::strcmp(callee.Name, "tex2Dbias") == 0 ||
					::strcmp(callee.Name, "tex3D") == 0 ||
					::strcmp(callee.Name, "tex3Doffset") == 0 ||
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
						break;
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
						break;
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
				const D3D10Texture *texture = nullptr;

				D3D10_SAMPLER_DESC desc;
				desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
				desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
				desc.MipLODBias = 0;
				desc.MaxAnisotropy = 1;
				desc.ComparisonFunc = D3D10_COMPARISON_NEVER;
				desc.BorderColor[0] = 1.0f;
				desc.BorderColor[1] = 1.0f;
				desc.BorderColor[2] = 1.0f;
				desc.BorderColor[3] = 1.0f;
				desc.MinLOD = -FLT_MAX;
				desc.MaxLOD = FLT_MAX;
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
						desc.Filter = D3D10_FILTER_ANISOTROPIC;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::POINT && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::POINT && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
					}
					else if (filterMin == Nodes::Literal::LINEAR && filterMag == Nodes::Literal::LINEAR && filterMip == Nodes::Literal::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
					}
					else
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
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
					ID3D10SamplerState *sampler;

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

				this->mCurrentSource += "static const __sampler2D ";
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
				ID3D10Texture2D *texture = nullptr;
				ID3D10ShaderResourceView *shaderresource[2] = { nullptr };
				UINT width = 1, height = 1, levels = 1;
				DXGI_FORMAT format1 = DXGI_FORMAT_R8G8B8A8_TYPELESS;
				D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
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

				this->mCurrentSource += "Texture2D ";

				CD3D10_TEXTURE2D_DESC desc(format1, width, height, 1, levels, D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET);
				srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MostDetailedMip = 0;
				srvdesc.Texture2D.MipLevels = levels;

				HRESULT hr = this->mEffect->mEffectContext->mDevice->CreateTexture2D(&desc, nullptr, &texture);

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
					auto obj = std::unique_ptr<D3D10Texture>(new D3D10Texture(this->mEffect));
					obj->mDesc.Width = width;
					obj->mDesc.Height = height;
					obj->mDesc.Levels = levels;
					obj->mDesc.Format = format2;
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

				auto obj = std::unique_ptr<D3D10Constant>(new D3D10Constant(this->mEffect));
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

				ID3D10Buffer *buffer;
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

					auto obj = std::unique_ptr<D3D10Constant>(new D3D10Constant(this->mEffect));
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

				auto obj = std::unique_ptr<D3D10Constant>(new D3D10Constant(this->mEffect));
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

				CD3D10_BUFFER_DESC desc(RoundToMultipleOf16(totalsize), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
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
				auto obj = std::unique_ptr<D3D10Technique>(new D3D10Technique(this->mEffect));

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
			void												VisitPassDeclaration(const Nodes::Pass &data, std::vector<D3D10Technique::Pass> &passes)
			{
				if (data.States == 0)
				{
					return;
				}

				D3D10Technique::Pass info;
				info.VS = nullptr;
				info.PS = nullptr;
				info.BS = nullptr;
				info.DSS = nullptr;
				info.StencilRef = 1;
				info.RS = nullptr;
				ZeroMemory(info.RT, sizeof(info.RT));
				D3D10_RASTERIZER_DESC rdesc;
				rdesc.FillMode = D3D10_FILL_SOLID;
				rdesc.CullMode = D3D10_CULL_BACK;
				rdesc.FrontCounterClockwise = FALSE;
				rdesc.DepthBias = D3D10_DEFAULT_DEPTH_BIAS;
				rdesc.DepthBiasClamp = D3D10_DEFAULT_DEPTH_BIAS_CLAMP;
				rdesc.SlopeScaledDepthBias = D3D10_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
				rdesc.DepthClipEnable = TRUE;
				rdesc.ScissorEnable = FALSE;
				rdesc.MultisampleEnable = FALSE;
				rdesc.AntialiasedLineEnable = FALSE;
				D3D10_DEPTH_STENCIL_DESC ddesc;
				ddesc.DepthEnable = TRUE;
				ddesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ALL;
				ddesc.DepthFunc = D3D10_COMPARISON_LESS;
				ddesc.StencilEnable = FALSE;
				ddesc.StencilReadMask = D3D10_DEFAULT_STENCIL_READ_MASK;
				ddesc.StencilWriteMask = D3D10_DEFAULT_STENCIL_WRITE_MASK;
				const D3D10_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D10_STENCIL_OP_KEEP, D3D10_STENCIL_OP_KEEP, D3D10_STENCIL_OP_KEEP, D3D10_COMPARISON_ALWAYS };
				ddesc.FrontFace = defaultStencilOp;
				ddesc.BackFace = defaultStencilOp;
				D3D10_BLEND_DESC bdesc;
				bdesc.AlphaToCoverageEnable = FALSE;
				for (size_t i = 0; i < 8; ++i) bdesc.BlendEnable[i] = FALSE;
				bdesc.SrcBlend = bdesc.SrcBlendAlpha = D3D10_BLEND_ONE;
				bdesc.DestBlend = bdesc.DestBlendAlpha = D3D10_BLEND_ZERO;
				bdesc.BlendOp = bdesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
				for (size_t i = 0; i < 8; ++i) bdesc.RenderTargetWriteMask[i] = D3D10_COLOR_WRITE_ENABLE_ALL;
				int srgb = 0;
				ID3D10RenderTargetView *targets[8][2] = { this->mEffect->mBackBufferTarget, this->mEffect->mBackBufferTarget };
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
							D3D10_TEXTURE2D_DESC desc;
							texture->mTexture->GetDesc(&desc);
							D3D10_RENDER_TARGET_VIEW_DESC rtvdesc;
							rtvdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
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
							for (size_t i = 0; i < 8; ++i) bdesc.RenderTargetWriteMask[i] = static_cast<UINT8>(state.Value.AsInt);
							break;
						case Nodes::State::DepthEnable:
							ddesc.DepthEnable = state.Value.AsInt;
							break;
						case Nodes::State::DepthFunc:
							ddesc.DepthFunc = ConvertLiteralToComparison(state.Value.AsInt);
							break;
						case Nodes::State::DepthWriteMask:
							ddesc.DepthWriteMask = state.Value.AsInt ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
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
						case Nodes::State::StencilRef:
							info.StencilRef = state.Value.AsInt;
							break;
						case Nodes::State::BlendEnable:
							for (size_t i = 0; i < 8; ++i) bdesc.BlendEnable[i] = state.Value.AsInt;
							break;
						case Nodes::State::BlendOp:
							bdesc.BlendOp = ConvertLiteralToBlendOp(state.Value.AsInt);
							break;
						case Nodes::State::BlendOpAlpha:
							bdesc.BlendOpAlpha = ConvertLiteralToBlendOp(state.Value.AsInt);
							break;
						case Nodes::State::SrcBlend:
							bdesc.SrcBlend = ConvertLiteralToBlend(state.Value.AsInt);
							break;
						case Nodes::State::DestBlend:
							bdesc.DestBlend = ConvertLiteralToBlend(state.Value.AsInt);
							break;
						case Nodes::State::SRGBWriteEnable:
							srgb = state.Value.AsInt;
							break;
					}
				}

				this->mEffect->mEffectContext->mDevice->CreateRasterizerState(&rdesc, &info.RS);
				this->mEffect->mEffectContext->mDevice->CreateDepthStencilState(&ddesc, &info.DSS);
				this->mEffect->mEffectContext->mDevice->CreateBlendState(&bdesc, &info.BS);

				for (size_t i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					info.RT[i] = targets[i][srgb];
				}
				for (auto it = info.SR.begin(), end = info.SR.end(); it != end; ++it)
				{
					ID3D10Resource *res1, *res2;
					(*it)->GetResource(&res1);

					for (size_t i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
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
			void												VisitPassShaderDeclaration(const Nodes::State &state, D3D10Technique::Pass &info)
			{
				LPCSTR profile;

				switch (state.Type)
				{
					case Nodes::State::VertexShader:
						profile = "vs_4_0";
						break;
					case Nodes::State::PixelShader:
						profile = "ps_4_0";
						break;
					default:
						return;
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
					"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
					"inline float4 __tex2Dbias(__sampler2D s, float4 c) { return s.t.SampleBias(s.s, c.xy, c.w); }\n"
					"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n"
					"cbuffer __GLOBAL__ : register(b0)\n{\n" + this->mCurrentGlobalConstants + "};\n" + this->mCurrentSource;

				const char *entry = this->mAST[state.Value.AsNode].As<Nodes::Function>().Name;

				LOG(TRACE) << "> Compiling shader '" << entry << "':\n\n" << source.c_str() << "\n";

				ID3DBlob *compiled, *errors;
				HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, entry, profile, flags, 0, &compiled, &errors);

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
						hr = this->mEffect->mEffectContext->mDevice->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &info.VS);
						break;
					case Nodes::State::PixelShader:
						hr = this->mEffect->mEffectContext->mDevice->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &info.PS);
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
			D3D10Effect *										mEffect;
			std::string											mCurrentSource;
			std::string &										mErrors;
			bool												mFatal;
			std::string											mCurrentGlobalConstants;
			size_t												mCurrentGlobalSize, mCurrentGlobalStorageSize;
			std::string											mCurrentBlockName;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
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

		D3D10Effect::D3D10Effect(std::shared_ptr<D3D10EffectContext> context) : mEffectContext(context), mConstantsDirty(true)
		{
			context->mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&this->mBackBufferTexture));

			D3D10_STATE_BLOCK_MASK mask;
			D3D10StateBlockMaskEnableAll(&mask);
			D3D10CreateStateBlock(context->mDevice, &mask, &this->mStateblock);

			D3D10_TEXTURE2D_DESC dstdesc;
			this->mBackBufferTexture->GetDesc(&dstdesc);
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

			context->mDevice->CreateRenderTargetView(this->mBackBufferTexture, nullptr, &this->mBackBufferTarget);
		}
		D3D10Effect::~D3D10Effect(void)
		{
			SAFE_RELEASE(this->mStateblock);
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
				SAFE_RELEASE(pass.RS);
				SAFE_RELEASE(pass.PS);
				SAFE_RELEASE(pass.BS);
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
			device->RSSetState(pass.RS);

			const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			device->OMSetBlendState(pass.BS, blendfactor, D3D10_DEFAULT_SAMPLE_MASK);
			device->OMSetDepthStencilState(pass.DSS, pass.StencilRef);
			device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, this->mEffect->mDepthStencil);

			for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				if (pass.RT[i] == nullptr || pass.RT[i] == this->mEffect->mBackBufferTarget)
				{
					continue;
				}

				const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
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