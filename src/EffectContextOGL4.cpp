#include "Effect.hpp"
#include "EffectParser.hpp"
#include "EffectContext.hpp"

#include <gl\gl3w.h>

// -----------------------------------------------------------------------------------------------------

#ifdef _DEBUG
	#define GLCHECK(call) { glGetError(); call; GLenum __e = glGetError(); if (__e != GL_NO_ERROR) { char __m[1024]; sprintf_s(__m, "OpenGL Error %x at line %d: %s", __e, __LINE__, #call); MessageBoxA(nullptr, __m, 0, MB_ICONERROR); } }
#else
	#define GLCHECK(call) call
#endif

namespace ReShade
{
	namespace
	{
		class													OGL4EffectContext : public EffectContext, public std::enable_shared_from_this<OGL4EffectContext>
		{
			friend struct OGL4Effect;
			friend struct OGL4Texture;
			friend struct OGL4Constant;
			friend struct OGL4Technique;
			friend class ASTVisitor;

		public:
			OGL4EffectContext(HDC device, HGLRC context);
			~OGL4EffectContext(void);

			void												GetDimension(unsigned int &width, unsigned int &height) const;

			std::unique_ptr<Effect>								Compile(const EffectTree &ast, std::string &errors);

		private:
			HDC													mDeviceContext;
			HGLRC												mRenderContext;
		};
		struct													OGL4Effect : public Effect
		{
			friend struct OGL4Texture;
			friend struct OGL4Sampler;
			friend struct OGL4Constant;
			friend struct OGL4Technique;

			OGL4Effect(std::shared_ptr<OGL4EffectContext> context);
			~OGL4Effect(void);

			Texture *											GetTexture(const std::string &name);
			const Texture *										GetTexture(const std::string &name) const;
			std::vector<std::string>							GetTextureNames(void) const;
			Constant *											GetConstant(const std::string &name);
			const Constant *									GetConstant(const std::string &name) const;
			std::vector<std::string>							GetConstantNames(void) const;
			Technique *											GetTechnique(const std::string &name);
			const Technique *									GetTechnique(const std::string &name) const;
			std::vector<std::string>							GetTechniqueNames(void) const;

			std::shared_ptr<OGL4EffectContext>					mEffectContext;
			std::unordered_map<std::string, std::unique_ptr<OGL4Texture>> mTextures;
			std::vector<std::shared_ptr<OGL4Sampler>>			mSamplers;
			std::unordered_map<std::string, std::unique_ptr<OGL4Constant>> mConstants;
			std::unordered_map<std::string, std::unique_ptr<OGL4Technique>> mTechniques;
			GLuint												mDefaultVAO, mDefaultVBO, mDefaultFBO;
			std::vector<GLuint>									mUniformBuffers;
			std::vector<std::pair<unsigned char *, GLsizeiptr>>	mUniformStorages;
			mutable bool										mUniformDirty;
		};
		struct													OGL4Texture : public Effect::Texture
		{
			OGL4Texture(OGL4Effect *effect);
			~OGL4Texture(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Resize(const Description &desc);
			void												Update(unsigned int level, const unsigned char *data, std::size_t size);
			void												UpdateFromColorBuffer(void);
			void												UpdateFromDepthBuffer(void);

			OGL4Effect *										mEffect;
			Description											mDesc;
			UINT												mDimension;
			GLenum												mInternalFormat;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			GLenum												mTarget;
			GLuint												mID, mSRGBView;
		};
		struct													OGL4Sampler
		{
			OGL4Sampler(void) : mID(0)
			{
			}
			~OGL4Sampler(void)
			{
				glDeleteSamplers(1, &this->mID);
			}

			GLuint												mID;
			OGL4Texture *										mTexture;
			bool												mSRGB;
		};
		struct													OGL4Constant : public Effect::Constant
		{
			OGL4Constant(OGL4Effect *effect);
			~OGL4Constant(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;
			void												GetValue(unsigned char *data, std::size_t size) const;
			void												SetValue(const unsigned char *data, std::size_t size);

			OGL4Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::size_t											mBuffer, mBufferOffset;
		};
		struct													OGL4Technique : public Effect::Technique
		{
			struct												Pass
			{
				GLuint											Program;
				GLuint											Framebuffer;
				GLint											StencilRef;
				GLuint											StencilMask, StencilReadMask;
				GLenum											DrawBuffers[8], CullFace, PolygonMode, BlendEqColor, BlendEqAlpha, BlendFuncSrc, BlendFuncDest, DepthFunc, StencilFunc, StencilOpFail, StencilOpZFail, StencilOpZPass, FrontFace;
				GLboolean										ScissorTest, Blend, DepthMask, DepthTest, StencilTest, ColorMaskR, ColorMaskG, ColorMaskB, ColorMaskA, SampleAlphaToCoverage;
			};

			OGL4Technique(OGL4Effect *effect);
			~OGL4Technique(void);

			const Description									GetDescription(void) const;
			const Effect::Annotation							GetAnnotation(const std::string &name) const;

			bool												Begin(unsigned int &passes) const;
			void												End(void) const;
			void												RenderPass(unsigned int index) const;

			OGL4Effect *										mEffect;
			Description											mDesc;
			std::unordered_map<std::string, Effect::Annotation>	mAnnotations;
			std::vector<Pass>									mPasses;
			mutable Pass										mStateblock;
		};

		GLenum													TextureBindingFromTarget(GLenum target)
		{
			switch (target)
			{
				default:
					return target;
				case GL_TEXTURE_1D:
					return GL_TEXTURE_BINDING_1D;
				case GL_TEXTURE_1D_ARRAY:
					return GL_TEXTURE_BINDING_1D_ARRAY;
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
					return GL_TEXTURE_BINDING_CUBE_MAP;
				case GL_TEXTURE_CUBE_MAP_ARRAY:
					return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
			}
		}

		class													ASTVisitor
		{
		public:
			ASTVisitor(const EffectTree &ast, std::string &errors) : mAST(ast), mErrors(errors), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentGlobalSize(0)
			{
			}

			bool												Traverse(OGL4Effect *effect)
			{
				this->mEffect = effect;
				this->mFatal = false;
				this->mCurrentSource.clear();

				// Global uniform buffer
				this->mEffect->mUniformBuffers.push_back(0);
				this->mEffect->mUniformStorages.push_back(std::make_pair(nullptr, 0));

				const auto &root = this->mAST[EffectTree::Root].As<Nodes::Aggregate>();

				for (size_t i = 0; i < root.Length; ++i)
				{
					Visit(this->mAST[root.Find(this->mAST, i)]);
				}

				if (this->mCurrentGlobalSize != 0)
				{
					glGenBuffers(1, &this->mEffect->mUniformBuffers[0]);

					GLint previous = 0;
					glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

					glBindBuffer(GL_UNIFORM_BUFFER, this->mEffect->mUniformBuffers[0]);
					glBufferData(GL_UNIFORM_BUFFER, this->mEffect->mUniformStorages[0].second, this->mEffect->mUniformStorages[0].first, GL_DYNAMIC_DRAW);
					glBindBuffer(GL_UNIFORM_BUFFER, previous);
				}

				return !this->mFatal;
			}

		private:
			void												operator =(const ASTVisitor &);

			static GLenum										ConvertLiteralToTextureFilter(int value)
			{
				switch (value)
				{
					default:
						return GL_NONE;
					case Nodes::Literal::POINT:
						return GL_NEAREST;
					case Nodes::Literal::LINEAR:
						return GL_LINEAR;
					case Nodes::Literal::ANISOTROPIC:
						return GL_LINEAR_MIPMAP_LINEAR;
				}
			}
			static GLenum										ConvertLiteralToTextureWrap(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::CLAMP:
						return GL_CLAMP_TO_EDGE;
					case Nodes::Literal::REPEAT:
						return GL_REPEAT;
					case Nodes::Literal::MIRROR:
						return GL_MIRRORED_REPEAT;
					case Nodes::Literal::BORDER:
						return GL_CLAMP_TO_BORDER;
				}
			}
			static GLenum										ConvertLiteralToCullFace(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::NONE:
						return GL_NONE;
					case Nodes::Literal::FRONT:
						return GL_FRONT;
					case Nodes::Literal::BACK:
						return GL_BACK;
				}
			}
			static GLenum										ConvertLiteralToPolygonMode(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::SOLID:
						return GL_FILL;
					case Nodes::Literal::WIREFRAME:
						return GL_LINE;
				}
			}
			static GLenum										ConvertLiteralToCompFunc(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ALWAYS:
						return GL_ALWAYS;
					case Nodes::Literal::NEVER:
						return GL_NEVER;
					case Nodes::Literal::EQUAL:
						return GL_EQUAL;
					case Nodes::Literal::NOTEQUAL:
						return GL_NOTEQUAL;
					case Nodes::Literal::LESS:
						return GL_LESS;
					case Nodes::Literal::LESSEQUAL:
						return GL_LEQUAL;
					case Nodes::Literal::GREATER:
						return GL_GREATER;
					case Nodes::Literal::GREATEREQUAL:
						return GL_GEQUAL;
				}
			}
			static GLenum										ConvertLiteralToStencilOp(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::KEEP:
						return GL_KEEP;
					case Nodes::Literal::ZERO:
						return GL_ZERO;
					case Nodes::Literal::REPLACE:
						return GL_REPLACE;
					case Nodes::Literal::INCR:
						return GL_INCR_WRAP;
					case Nodes::Literal::INCRSAT:
						return GL_INCR;
					case Nodes::Literal::DECR:
						return GL_DECR_WRAP;
					case Nodes::Literal::DECRSAT:
						return GL_DECR;
					case Nodes::Literal::INVERT:
						return GL_INVERT;
				}
			}
			static GLenum										ConvertLiteralToBlendFunc(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ZERO:
						return GL_ZERO;
					case Nodes::Literal::ONE:
						return GL_ONE;
					case Nodes::Literal::SRCCOLOR:
						return GL_SRC_COLOR;
					case Nodes::Literal::SRCALPHA:
						return GL_SRC_ALPHA;
					case Nodes::Literal::INVSRCCOLOR:
						return GL_ONE_MINUS_SRC_COLOR;
					case Nodes::Literal::INVSRCALPHA:
						return GL_ONE_MINUS_SRC_ALPHA;
					case Nodes::Literal::DESTCOLOR:
						return GL_DST_COLOR;
					case Nodes::Literal::DESTALPHA:
						return GL_DST_ALPHA;
					case Nodes::Literal::INVDESTCOLOR:
						return GL_ONE_MINUS_DST_COLOR;
					case Nodes::Literal::INVDESTALPHA:
						return GL_ONE_MINUS_DST_ALPHA;
				}
			}
			static GLenum										ConvertLiteralToBlendEquation(int value)
			{
				switch (value)
				{
					default:
					case Nodes::Literal::ADD:
						return GL_FUNC_ADD;
					case Nodes::Literal::SUBSTRACT:
						return GL_FUNC_SUBTRACT;
					case Nodes::Literal::REVSUBSTRACT:
						return GL_FUNC_REVERSE_SUBTRACT;
					case Nodes::Literal::MIN:
						return GL_MIN;
					case Nodes::Literal::MAX:
						return GL_MAX;
				}
			}
			static void											ConvertLiteralToFormat(int value, GLenum &internalformat, GLenum &internalformatsrgb, Effect::Texture::Format &format)
			{
				switch (value)
				{
					case Nodes::Literal::R8:
						format = Effect::Texture::Format::R8;
						internalformat = internalformatsrgb = GL_R8;
						break;
					case Nodes::Literal::R32F:
						format = Effect::Texture::Format::R32F;
						internalformat = internalformatsrgb = GL_R32F;
						break;
					case Nodes::Literal::RG8:
						format = Effect::Texture::Format::RG8;
						internalformat = internalformatsrgb = GL_RG8;
						break;
					case Nodes::Literal::RGBA8:
						format = Effect::Texture::Format::RGBA8;
						internalformat = GL_RGBA8;
						internalformatsrgb = GL_SRGB8_ALPHA8;
						break;
					case Nodes::Literal::RGBA16:
						format = Effect::Texture::Format::RGBA16;
						internalformat = internalformatsrgb = GL_RGBA16;
						break;
					case Nodes::Literal::RGBA16F:
						format = Effect::Texture::Format::RGBA16F;
						internalformat = internalformatsrgb = GL_RGBA16F;
						break;
					case Nodes::Literal::RGBA32F:
						format = Effect::Texture::Format::RGBA32F;
						internalformat = internalformatsrgb = GL_RGBA32F;
						break;
					case Nodes::Literal::DXT1:
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
						format = Effect::Texture::Format::DXT1;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
						break;
					case Nodes::Literal::DXT3:
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
						format = Effect::Texture::Format::DXT3;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
						break;
					case Nodes::Literal::DXT5:
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
						format = Effect::Texture::Format::DXT5;
						internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
						internalformatsrgb = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
						break;
					case Nodes::Literal::LATC1:
#define GL_COMPRESSED_LUMINANCE_LATC1_EXT 0x8C70
						format = Effect::Texture::Format::LATC1;
						internalformat = internalformatsrgb = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
						break;
					case Nodes::Literal::LATC2:
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
			static std::string									FixName(const std::string &name)
			{
				std::string res;

				if (name == "gl_" ||
					name == "output" ||
					name == "input" ||
					name == "active" ||
					name == "filter" ||
					name == "main")
				{
					res += '_';
				}

				res += name;

				return res;
			}
			static std::string									FixNameWithSemantic(const std::string &name, const std::string &semantic)
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
			static void											FixImplicitCast(const Nodes::Type &from, const Nodes::Type &to, std::string &before, std::string &after)
			{
				if (from.Class != to.Class)
				{
					switch (to.Class)
					{
						case Nodes::Type::Bool:
							before += to.Rows > 1 ? "bvec" + std::to_string(to.Rows) : "bool";
							before += "(";
							after += ")";
							break;
						case Nodes::Type::Float:
							before += to.Rows > 1 ? "vec" + std::to_string(to.Rows) : "float";
							before += "(";
							after += ")";
							break;
					}
				}

				if (from.Rows > 0 && from.Rows < to.Rows)
				{
					after += ".";
					char sub[4] = { 'x', 'y', 'z', 'w' };

					for (unsigned int i = 0; i < from.Rows; ++i)
					{
						after += sub[i];
					}
					for (unsigned int i = from.Rows; i < to.Rows; ++i)
					{
						after += sub[from.Rows - 1];
					}
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
				if (type.HasQualifier(Nodes::Type::Qualifier::GroupShared))
					this->mCurrentSource += "shared ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Const) || type.HasQualifier(Nodes::Type::Qualifier::Static))
					this->mCurrentSource += "const ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Volatile))
					this->mCurrentSource += "volatile ";
				if (type.HasQualifier(Nodes::Type::Qualifier::NoInterpolation))
					this->mCurrentSource += "flat ";
				if (type.HasQualifier(Nodes::Type::Qualifier::NoPerspective))
					this->mCurrentSource += "noperspective ";
				if (type.HasQualifier(Nodes::Type::Qualifier::Linear))
					this->mCurrentSource += "smooth ";
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
					this->mCurrentSource += "layout(row_major) ";
				else if (type.HasQualifier(Nodes::Type::Qualifier::ColumnMajor))
					this->mCurrentSource += "layout(column_major) ";

				switch (type.Class)
				{
					case Nodes::Type::Void:
						this->mCurrentSource += "void";
						break;
					case Nodes::Type::Bool:
						if (type.IsMatrix())
							this->mCurrentSource += "bmat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += "bvec" + std::to_string(type.Rows);
						else
							this->mCurrentSource += "bool";
						break;
					case Nodes::Type::Int:
						if (type.IsMatrix())
							this->mCurrentSource += "imat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += "ivec" + std::to_string(type.Rows);
						else
							this->mCurrentSource += "int";
						break;
					case Nodes::Type::Uint:
						if (type.IsMatrix())
							this->mCurrentSource += "umat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += "uvec" + std::to_string(type.Rows);
						else
							this->mCurrentSource += "uint";
						break;
					case Nodes::Type::Half:
					case Nodes::Type::Float:
						if (type.IsMatrix())
							this->mCurrentSource += "mat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += "vec" + std::to_string(type.Rows);
						else
							this->mCurrentSource += "float";
						break;
					case Nodes::Type::Double:
						if (type.IsMatrix())
							this->mCurrentSource += "dmat" + std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
						else if (type.IsVector())
							this->mCurrentSource += "dvec" + std::to_string(type.Rows);
						else
							this->mCurrentSource += "double";
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
						this->mCurrentSource += FixName(this->mAST[type.Definition].As<Nodes::Struct>().Name);
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
						this->mCurrentSource += std::to_string(data.Value.AsFloat);
						break;
					case Nodes::Type::Double:
						this->mCurrentSource += std::to_string(data.Value.AsDouble) + "lf";
						break;
				}
			}
			void												VisitReference(const Nodes::Reference &data)
			{
				const auto &symbol = this->mAST[data.Symbol].As<Nodes::Variable>();

				this->mCurrentSource += FixName(symbol.Name);
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
						VisitType(data.Type);
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

				const auto &left = this->mAST[data.Left].As<Nodes::Expression>();
				const auto &right = this->mAST[data.Right].As<Nodes::Expression>();

				std::string implicitBefore, implicitAfter;
				FixImplicitCast(left.Type, right.Type, implicitBefore, implicitAfter);

				if (data.Operator == Nodes::Operator::Modulo && (left.Type.IsFloatingPoint() || right.Type.IsFloatingPoint()))
				{
					this->mCurrentSource += "fmod(";
					this->mCurrentSource += implicitBefore;
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += implicitAfter;
					this->mCurrentSource += ", ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ")";
				}
				else if (data.Operator == Nodes::Operator::Multiply && (left.Type.IsMatrix() || right.Type.IsMatrix()))
				{
					this->mCurrentSource += "matrixCompMult(";
					this->mCurrentSource += implicitBefore;
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += implicitAfter;
					this->mCurrentSource += ", ";
					Visit(this->mAST[data.Right]);
					this->mCurrentSource += ")";
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
					this->mCurrentSource += implicitBefore;
					Visit(this->mAST[data.Left]);
					this->mCurrentSource += implicitAfter;
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

				std::string implicitBefore, implicitAfter;
				FixImplicitCast(this->mAST[data.Right].As<Nodes::Expression>().Type, this->mAST[data.Left].As<Nodes::Expression>().Type, implicitBefore, implicitAfter);

				this->mCurrentSource += ' ';
				this->mCurrentSource += implicitBefore;
				Visit(this->mAST[data.Right]);
				this->mCurrentSource += implicitAfter;
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

				this->mCurrentSource += FixName(callee.Name);
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

				this->mCurrentSource += FixName(this->mAST[data.Callee].As<Nodes::Variable>().Name);
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
					this->mCurrentSource += FixName(data.Name);
				}
				else
				{
					this->mCurrentSource += "_" + std::to_string(std::rand() * 100 + std::rand() * 10 + std::rand());
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
			
				auto type = data.Type;
				type.Qualifiers &= ~static_cast<unsigned int>(Nodes::Type::Qualifier::Uniform);
				VisitType(type);

				if (data.Name != nullptr)
				{
					this->mCurrentSource += ' ';

					if (!this->mCurrentBlockName.empty())
					{
						this->mCurrentSource += this->mCurrentBlockName + '_';
					}
				
					this->mCurrentSource += FixName(data.Name);
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

				if (!this->mCurrentInParameterBlock)
				{
					if (data.HasInitializer())
					{
						this->mCurrentSource += " = ";

						const auto &initializer = this->mAST[data.Initializer];

						if (initializer.Is<Nodes::Aggregate>())
						{
							Nodes::Type type = data.Type;
							type.Qualifiers = 0;
							VisitType(type);
							this->mCurrentSource += "(";

							for (size_t i = 0; i < initializer.As<Nodes::Aggregate>().Length; ++i)
							{
								Visit(this->mAST[initializer.As<Nodes::Aggregate>().Find(this->mAST, i)]);

								this->mCurrentSource += ", ";
							}

							this->mCurrentSource += ")";
						}
						else
						{
							std::string implicitBefore, implicitAfter;
							FixImplicitCast(initializer.As<Nodes::Expression>().Type, data.Type, implicitBefore, implicitAfter);

							this->mCurrentSource += implicitBefore;
							Visit(initializer);
							this->mCurrentSource += implicitAfter;
						}
					}

					this->mCurrentSource += ";\n";
				}
			}
			void												VisitSamplerDeclaration(const Nodes::Variable &data)
			{
				std::shared_ptr<OGL4Sampler> obj = std::make_shared<OGL4Sampler>();
				obj->mSRGB = false;

				glGenSamplers(1, &obj->mID);

				if (data.HasInitializer())
				{
					#pragma region Fill Description
					const auto &states = this->mAST[data.Initializer].As<Nodes::Aggregate>();
					int mipfilter = 0;

					for (size_t i = 0; i < states.Length; ++i)
					{
						const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();
					
						switch (state.Type)
						{
							case Nodes::State::Texture:
								obj->mTexture = this->mEffect->mTextures.at(this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name).get();
								break;
							case Nodes::State::MinFilter:
								glSamplerParameteri(obj->mID, GL_TEXTURE_MIN_FILTER, ConvertLiteralToTextureFilter(state.Value.AsInt));
								break;
							case Nodes::State::MagFilter:
								glSamplerParameteri(obj->mID, GL_TEXTURE_MAG_FILTER, ConvertLiteralToTextureFilter(state.Value.AsInt));
								break;
							case Nodes::State::MipFilter:
								mipfilter = state.Value.AsInt;
								break;
							case Nodes::State::AddressU:
								glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_S, ConvertLiteralToTextureWrap(state.Value.AsInt));
								break;
							case Nodes::State::AddressV:
								glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_T, ConvertLiteralToTextureWrap(state.Value.AsInt));
								break;
							case Nodes::State::AddressW:
								glSamplerParameteri(obj->mID, GL_TEXTURE_WRAP_R, ConvertLiteralToTextureWrap(state.Value.AsInt));
								break;
							case Nodes::State::MipLODBias:
								glSamplerParameterf(obj->mID, GL_TEXTURE_LOD_BIAS, state.Value.AsFloat);
								break;
							case Nodes::State::MinLOD:
								glSamplerParameterf(obj->mID, GL_TEXTURE_MIN_LOD, state.Value.AsFloat);
								break;
							case Nodes::State::MaxLOD:
								glSamplerParameterf(obj->mID, GL_TEXTURE_MAX_LOD, state.Value.AsFloat);
								break;
							case Nodes::State::MaxAnisotropy:
	#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
								glSamplerParameterf(obj->mID, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<GLfloat>(state.Value.AsInt));
								break;
							case Nodes::State::SRGB:
								obj->mSRGB = state.Value.AsInt != 0;
								break;
						}
					}

					if (mipfilter != 0)
					{
						GLint minfilter;
						glGetSamplerParameteriv(obj->mID, GL_TEXTURE_MIN_FILTER, &minfilter);

						switch (mipfilter)
						{
							case Nodes::Literal::POINT:
							{
								switch (minfilter)
								{
									case GL_NEAREST:
										minfilter = GL_NEAREST_MIPMAP_NEAREST;
										break;
									case GL_LINEAR:
										minfilter = GL_LINEAR_MIPMAP_NEAREST;
										break;
								}
								break;
							}
							case Nodes::Literal::LINEAR:
							{
								switch (minfilter)
								{
									case GL_NEAREST:
										minfilter = GL_LINEAR_MIPMAP_NEAREST;
										break;
									case GL_LINEAR:
										minfilter = GL_LINEAR_MIPMAP_LINEAR;
										break;
								}
								break;
							}
						}

						glSamplerParameteri(obj->mID, GL_TEXTURE_MIN_FILTER, minfilter);
					}
					#pragma endregion
				}

				if (obj->mTexture == nullptr)
				{
					return;
				}

				this->mCurrentSource += "layout(binding = " + std::to_string(this->mEffect->mSamplers.size()) + ") uniform sampler" + std::to_string(obj->mTexture->mDimension) + "D ";
				this->mCurrentSource += FixName(data.Name);
				this->mCurrentSource += ";\n";

				this->mEffect->mSamplers.push_back(obj);
			}
			void												VisitTextureDeclaration(const Nodes::Variable &data)
			{
				GLuint texture[2] = { 0, 0 };
				GLsizei width = 1, height = 1, depth = 1, levels = 1;
				GLenum internalformat = GL_RGBA8, internalformatSRGB = GL_SRGB8_ALPHA8;
				Effect::Texture::Format format = Effect::Texture::Format::RGBA8;
			
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
								ConvertLiteralToFormat(state.Value.AsInt, internalformat, internalformatSRGB, format);
								break;
						}
					}
					#pragma endregion
				}

				glGenTextures(2, texture);
			
				auto obj = std::unique_ptr<OGL4Texture>(new OGL4Texture(this->mEffect));
				obj->mDesc.Width = width;
				obj->mDesc.Height = height;
				obj->mDesc.Depth = depth;
				obj->mDesc.Levels = levels;
				obj->mDesc.Format = format;
				obj->mID = texture[0];
				obj->mSRGBView = texture[1];

				GLint previous = 0;

				switch (data.Type.Class)
				{
					case Nodes::Type::Texture1D:
					{
						obj->mTarget = GL_TEXTURE_1D;
						obj->mDimension = 1;

						glGetIntegerv(GL_TEXTURE_BINDING_1D, &previous);
						glBindTexture(GL_TEXTURE_1D, texture[0]);
						glTexStorage1D(GL_TEXTURE_1D, levels, internalformat, width);
						break;
					}
					case Nodes::Type::Texture2D:
					{
						obj->mTarget = GL_TEXTURE_2D;
						obj->mDimension = 2;

						glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
						glBindTexture(GL_TEXTURE_2D, texture[0]);
						glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
						break;
					}
					case Nodes::Type::Texture3D:
					{
						obj->mTarget = GL_TEXTURE_3D;
						obj->mDimension = 3;

						glGetIntegerv(GL_TEXTURE_BINDING_3D, &previous);
						glBindTexture(GL_TEXTURE_3D, texture[0]);
						glTexStorage3D(GL_TEXTURE_3D, levels, internalformat, width, height, depth);
						break;
					}
				}

				glTextureView(texture[1], obj->mTarget, texture[0], internalformatSRGB, 0, levels, 0, depth);
				glBindTexture(obj->mTarget, previous);

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

				auto obj = std::unique_ptr<OGL4Constant>(new OGL4Constant(this->mEffect));
				obj->mDesc = desc;
				obj->mBuffer = 0;
				obj->mBufferOffset = this->mCurrentGlobalSize - desc.Size;

				if (this->mCurrentGlobalSize >= static_cast<std::size_t>(this->mEffect->mUniformStorages[0].second))
				{
					this->mEffect->mUniformStorages[0].first = static_cast<unsigned char *>(::realloc(this->mEffect->mUniformStorages[0].first, this->mEffect->mUniformStorages[0].second += 128));
				}

				if (data.Initializer && this->mAST[data.Initializer].Is<Nodes::Literal>())
				{
					std::memcpy(this->mEffect->mUniformStorages[0].first + obj->mBufferOffset, &this->mAST[data.Initializer].As<Nodes::Literal>().Value, desc.Size);
				}
				else
				{
					std::memset(this->mEffect->mUniformStorages[0].first + obj->mBufferOffset, 0, desc.Size);
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

				this->mCurrentSource += "layout(std140, binding = " + std::to_string(this->mEffect->mUniformBuffers.size()) + ") uniform ";
				this->mCurrentSource += FixName(data.Name);
				this->mCurrentSource += "\n{\n";
				this->mCurrentBlockName = data.Name;

				GLuint buffer;
				unsigned char *storage = nullptr;
				GLsizeiptr totalsize = 0, currentsize = 0;

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

					auto obj = std::unique_ptr<OGL4Constant>(new OGL4Constant(this->mEffect));
					obj->mDesc = desc;
					obj->mBuffer = this->mEffect->mUniformBuffers.size();
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

				auto obj = std::unique_ptr<OGL4Constant>(new OGL4Constant(this->mEffect));
				obj->mDesc.Rows = 0;
				obj->mDesc.Columns = 0;
				obj->mDesc.Elements = 0;
				obj->mDesc.Fields = fields.Length;
				obj->mDesc.Size = totalsize;
				obj->mDesc.Type = Effect::Constant::Type::Struct;
				obj->mBuffer = this->mEffect->mUniformBuffers.size();
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

				glGenBuffers(1, &buffer);

				GLint previous = 0;
				glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previous);

				glBindBuffer(GL_UNIFORM_BUFFER, buffer);
				glBufferData(GL_UNIFORM_BUFFER, totalsize, storage, GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, previous);

				this->mEffect->mUniformBuffers.push_back(buffer);
				this->mEffect->mUniformStorages.push_back(std::make_pair(storage, totalsize));
			}
			void												VisitFunctionDeclaration(const Nodes::Function &data)
			{
				auto type = data.ReturnType;
				type.Qualifiers &= ~static_cast<int>(Nodes::Type::Qualifier::Const);
				VisitType(type);

				this->mCurrentSource += ' ';
				this->mCurrentSource += FixName(data.Name);
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
				auto obj = std::unique_ptr<OGL4Technique>(new OGL4Technique(this->mEffect));

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
			void												VisitPassDeclaration(const Nodes::Pass &data, std::vector<OGL4Technique::Pass> &passes)
			{
				if (data.States == 0)
				{
					return;
				}
			
				OGL4Technique::Pass info;
				ZeroMemory(&info, sizeof(OGL4Technique::Pass));
				info.Program = glCreateProgram();
				info.ColorMaskR = info.ColorMaskG = info.ColorMaskB = info.ColorMaskA = GL_TRUE;
				info.CullFace = GL_BACK;
				info.DepthFunc = GL_LESS;
				info.DepthMask = GL_TRUE;
				info.PolygonMode = GL_FILL;
				info.StencilFunc = GL_ALWAYS;
				info.StencilRef = 0;
				info.StencilMask = 0xFFFFFFFF;
				info.StencilReadMask = 0xFFFFFFFF;
				info.StencilOpFail = info.StencilOpZFail = info.StencilOpZPass = GL_KEEP;
				info.SampleAlphaToCoverage = GL_FALSE;
				info.BlendFuncSrc = GL_ONE;
				info.BlendFuncDest = GL_ZERO;
				info.BlendEqColor = info.BlendEqAlpha = GL_FUNC_ADD;
				info.DrawBuffers[0] = GL_BACK;
				info.FrontFace = GL_CCW;

				GLuint shaders[6] = { 0 };

				const auto &states = this->mAST[data.States].As<Nodes::Aggregate>();

				for (size_t i = 0; i < states.Length; ++i)
				{
					const auto &state = this->mAST[states.Find(this->mAST, i)].As<Nodes::State>();

					switch (state.Type)
					{
						case Nodes::State::VertexShader:
						case Nodes::State::PixelShader:
						{
							GLuint shader;
							VisitPassShaderDeclaration(state, shader);
							glAttachShader(info.Program, shader);
							shaders[state.Type - Nodes::State::VertexShader] = shader;
							break;
						}
						case Nodes::State::RenderTarget0:
						case Nodes::State::RenderTarget1:
						case Nodes::State::RenderTarget2:
						case Nodes::State::RenderTarget3:
						case Nodes::State::RenderTarget4:
						case Nodes::State::RenderTarget5:
						case Nodes::State::RenderTarget6:
						case Nodes::State::RenderTarget7:
						{
							if (info.Framebuffer == 0)
							{
								glGenFramebuffers(1, &info.Framebuffer);
							}

							const GLuint index = state.Type - Nodes::State::RenderTarget0;
							const char *name = this->mAST[state.Value.AsNode].As<Nodes::Variable>().Name;
							const OGL4Texture *texture = this->mEffect->mTextures.at(name).get();

							glBindFramebuffer(GL_FRAMEBUFFER, info.Framebuffer);
							glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, texture->mID, 0);

#ifdef _DEBUG
							GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
							assert(status == GL_FRAMEBUFFER_COMPLETE);
#endif

							glBindFramebuffer(GL_FRAMEBUFFER, 0);

							info.DrawBuffers[index] = GL_COLOR_ATTACHMENT0 + index;
							break;
						}
						case Nodes::State::RenderTargetWriteMask:
						{
							const GLuint mask = static_cast<GLuint>(state.Value.AsInt);

							info.ColorMaskR = mask & (1 << 0);
							info.ColorMaskG = mask & (1 << 1);
							info.ColorMaskB = mask & (1 << 2);
							info.ColorMaskA = mask & (1 << 3);
							break;
						}
						case Nodes::State::CullMode:
							info.CullFace = ConvertLiteralToCullFace(state.Value.AsInt);
							break;
						case Nodes::State::FillMode:
							info.PolygonMode = ConvertLiteralToPolygonMode(state.Value.AsInt);
							break;
						case Nodes::State::ScissorEnable:
							info.ScissorTest = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::DepthEnable:
							info.DepthTest = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::DepthFunc:
							info.DepthFunc = ConvertLiteralToCompFunc(state.Value.AsInt);
							break;
						case Nodes::State::DepthWriteMask:
							info.DepthMask = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::StencilEnable:
							info.StencilTest = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::StencilReadMask:
							info.StencilReadMask = state.Value.AsInt & 0xFF;
							break;
						case Nodes::State::StencilWriteMask:
							info.StencilMask = state.Value.AsInt & 0xFF;
							break;
						case Nodes::State::StencilFunc:
							info.StencilFunc = ConvertLiteralToCompFunc(state.Value.AsInt);
							break;
						case Nodes::State::StencilPass:
							info.StencilOpZPass = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::StencilFail:
							info.StencilOpFail = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::StencilZFail:
							info.StencilOpZFail = ConvertLiteralToStencilOp(state.Value.AsInt);
							break;
						case Nodes::State::AlphaToCoverageEnable:
							info.SampleAlphaToCoverage = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::BlendEnable:
							info.Blend = static_cast<GLboolean>(state.Value.AsInt);
							break;
						case Nodes::State::BlendOp:
							info.BlendEqColor = ConvertLiteralToBlendEquation(state.Value.AsInt);
							break;
						case Nodes::State::BlendOpAlpha:
							info.BlendEqAlpha = ConvertLiteralToBlendEquation(state.Value.AsInt);
							break;
						case Nodes::State::SrcBlend:
							info.BlendFuncSrc = ConvertLiteralToBlendFunc(state.Value.AsInt);
							break;
						case Nodes::State::DestBlend:
							info.BlendFuncDest = ConvertLiteralToBlendFunc(state.Value.AsInt);
							break;
					}
				}

				glLinkProgram(info.Program);

				for (size_t i = 0; i < ARRAYSIZE(shaders); ++i)
				{
					if (shaders[i] == 0)
					{
						continue;
					}

					glDetachShader(info.Program, shaders[i]);
					glDeleteShader(shaders[i]);
				}

				GLint status = GL_FALSE;
				glGetProgramiv(info.Program, GL_LINK_STATUS, &status);

				if (status == GL_FALSE)
				{
					GLint logsize = 0;
					glGetProgramiv(info.Program, GL_INFO_LOG_LENGTH, &logsize);

					std::string log(logsize, '\0');
					glGetProgramInfoLog(info.Program, logsize, nullptr, &log.front());

					glDeleteProgram(info.Program);

					this->mErrors += log;
					this->mFatal = true;
					return;
				}

				passes.push_back(info);
			}
			void												VisitShaderVariable(Nodes::Type::Qualifier qualifier, Nodes::Type type, const std::string &name, const char *semantic, std::string &source)
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

				const std::string backup = this->mCurrentSource;
				this->mCurrentSource.clear();
				type.Qualifiers = static_cast<unsigned int>(qualifier);
				VisitType(type);
				source += this->mCurrentSource;
				this->mCurrentSource = backup;

				source += " " + name + ";\n";
			}
			void												VisitPassShaderDeclaration(const Nodes::State &data, GLuint &shader)
			{
				std::string source =
					"#version 430\n"
					"#extension GL_EXT_shader_implicit_conversions : enable\n"
					"#define lerp(a, b, t) mix(a, b, t)\n"
					"#define saturate(x) clamp(x, 0.0, 1.0)\n"
					"#define rsqrt(x) inversesqrt(x)\n"
					"float rcp(float x) { return 1.0 / x; } vec2 rcp(vec2 x) { return 1.0.xx / x; } vec3 rcp(vec3 x) { return 1.0.xxx / x; } vec4 rcp(vec4 x) { return 1.0.xxxx / x; } mat2 rcp(mat2 x) { return mat2(1.0) / x; } mat3 rcp(mat3 x) { return mat3(1.0) / x; } mat4 rcp(mat4 x) { return mat4(1.0) / x; }\n"
					"#define frac(x) fract(x)\n"
					"#define fmod(x) ((x) - (y) * trunc((x) / (y)))\n"
					"#define ddx(x) dFdx(x)\n#define ddy(x) dFdy(-(x))\n"
					"#define mul(a, b) ((a) * (b))\n"
					"#define mad(m, a, b) ((m) * (a) + (b))\n"
					"#define log10(x) (log(x) / 2.302585093)\n"
					"#define atan2(x, y) atan(x, y)\n"
					"void sincos(float x, out float s, out float c) { s = sin(x); c = cos(x); }\n"
					"#define asint(x) floatBitsToInt(x)\n#define asuint(x) floatBitsToUint(x)\n#define asfloat(x) uintBitsToFloat(x)"
					"float f16tof32(uint x) { return uintBitsToFloat(((x & 0x8000) << 16) | (((x & 0x7C00) + 0x1C000) << 13) | ((x & 0x03FF) << 13)); }\nuint f32tof16(float x) { uint f = floatBitsToUint(x); return ((f >> 16) & 0x8000) | ((((f & 0x7F800000) - 0x38000000) >> 13) & 0x7C00) | ((f >> 13) & 0x03FF); }\n"
					"vec4  tex1D(sampler1D s, float c) { return texture(s, c); }\n"
					"vec4  tex1D(sampler1D s, float c, int offset) { return textureOffset(s, c, offset); }\n"
					"vec4  tex1Dlod(sampler1D s, vec4 c) { return textureLod(s, c.x, c.w); }\n"
					"vec4  tex1Dlod(sampler1D s, vec4 c, int offset) { return textureLodOffset(s, c.x, c.w, offset); }\n"
					"vec4  tex1Dfetch(sampler1D t, ivec4 c) { return texelFetch(t, c.x, c.w); }\n"
					"vec4  tex1Dfetch(sampler1D t, ivec4 c, int offset) { return texelFetchOffset(t, c.x, c.w, offset); }\n"
					"vec4  tex1Dbias(sampler1D s, vec4 c) { return textureOffset(s, c.x, 0, c.w); }\n"
					"vec4  tex1Dbias(sampler1D s, vec4 c, int offset) { return textureOffset(s, c.x, offset, c.w); }\n"
					"int   tex1Dsize(sampler1D t, int lod) { return textureSize(t, lod); }\n"
					"vec4  tex2D(sampler2D s, vec2 c) { return texture(s, c); }\n"
					"vec4  tex2D(sampler2D s, vec2 c, int offset) { return textureOffset(s, c, offset.xx); }\n"
					"vec4  tex2Dlod(sampler2D s, vec4 c) { return textureLod(s, c.xy, c.w); }\n"
					"vec4  tex2Dlod(sampler2D s, vec4 c, int offset) { return textureLodOffset(s, c.xy, c.w, offset.xx); }\n"
					"vec4  tex2Dfetch(sampler2D t, ivec4 c) { return texelFetch(t, c.xy, c.w); }\n"
					"vec4  tex2Dfetch(sampler2D t, ivec4 c, int offset) { return texelFetchOffset(t, c.xy, c.w, offset.xx); }\n"
					"vec4  tex2Dbias(sampler2D s, vec4 c) { return textureOffset(s, c.xy, ivec2(0), c.w); }\n"
					"vec4  tex2Dbias(sampler2D s, vec4 c, int offset) { return textureOffset(s, c.xy, offset.xx, c.w); }\n"
					"ivec2 tex2Dsize(sampler2D t, int lod) { return textureSize(t, lod); }\n"
					"vec4  tex3D(sampler3D s, vec3 c) { return texture(s, c); }\n"
					"vec4  tex3D(sampler3D s, vec3 c, int offset) { return textureOffset(s, c, offset.xxx); }\n"
					"vec4  tex3Dlod(sampler3D s, vec4 c) { return textureLod(s, c.xyz, c.w); }\n"
					"vec4  tex3Dlod(sampler3D s, vec4 c, int offset) { return textureLodOffset(s, c.xyz, c.w, offset.xxx); }\n"
					"vec4  tex3Dfetch(sampler3D t, ivec4 c) { return texelFetch(t, c.xyz, c.w); }\n"
					"vec4  tex3Dfetch(sampler3D t, ivec4 c, int offset) { return texelFetchOffset(t, c.xyz, c.w, offset.xxx); }\n"
					"vec4  tex3Dbias(sampler3D s, vec4 c) { return textureOffset(s, c.xyz, ivec3(0), c.w); }\n"
					"vec4  tex3Dbias(sampler3D s, vec4 c, int offset) { return textureOffset(s, c.xyz, offset.xxx, c.w); }\n"
					"ivec3 tex3Dsize(sampler3D t, int lod) { return textureSize(t, lod); }\n"
					+ this->mCurrentSource;

				const auto &callee = this->mAST[data.Value.AsNode].As<Nodes::Function>();

				if (callee.HasArguments())
				{
					const auto &arguments = this->mAST[callee.Arguments].As<Nodes::Aggregate>();

					for (size_t i = 0; i < arguments.Length; ++i)
					{
						const auto &arg = this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>();

						if (arg.Type.IsStruct())
						{
							const auto &fields = this->mAST[this->mAST[arg.Type.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

							for (size_t k = 0; k < fields.Length; ++k)
							{
								const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

								VisitShaderVariable(static_cast<Nodes::Type::Qualifier>(arg.Type.Qualifiers), field.Type, "_param_" + std::string(arg.Name != nullptr ? arg.Name : std::to_string(i)) + "_" + std::string(field.Name), arg.Semantic, source);
							}
						}
						else
						{
							VisitShaderVariable(static_cast<Nodes::Type::Qualifier>(arg.Type.Qualifiers), arg.Type, "_param_" + std::string(arg.Name != nullptr ? arg.Name : std::to_string(i)), arg.Semantic, source);
						}
					}
				}

				if (callee.ReturnType.IsStruct())
				{
					const auto &fields = this->mAST[this->mAST[callee.ReturnType.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

					for (size_t i = 0; i < fields.Length; ++i)
					{
						const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

						VisitShaderVariable(Nodes::Type::Qualifier::Out, field.Type, "_return_" + std::string(field.Name), field.Semantic, source);
					}
				}
				else if (callee.ReturnType.Class != Nodes::Type::Void)
				{
					VisitShaderVariable(Nodes::Type::Qualifier::Out, callee.ReturnType, "_return", callee.ReturnSemantic, source);
				}

				source += "void main()\n{\n";

				if (callee.HasArguments())
				{
					const auto &arguments = this->mAST[callee.Arguments].As<Nodes::Aggregate>();
				
					for (size_t i = 0; i < arguments.Length; ++i)
					{
						const auto &arg = this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>();

						if (arg.Type.IsStruct())
						{
							VisitType(arg.Type);
							source += " _param_" + std::string(arg.Name != nullptr ? arg.Name : std::to_string(i)) + " = " + this->mAST[arg.Type.Definition].As<Nodes::Struct>().Name + "(";

							if (this->mAST[arg.Type.Definition].As<Nodes::Struct>().Fields != 0)
							{
								const auto &fields = this->mAST[this->mAST[arg.Type.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

								for (size_t k = 0; k < fields.Length; ++k)
								{
									const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

									source += "_param_" + std::string(arg.Name != nullptr ? arg.Name : std::to_string(i)) + "_" + std::string(field.Name) + ", ";
								}

								source.pop_back();
								source.pop_back();
							}

							source += ")\n;";
						}
					}
				}
				if (callee.ReturnType.IsStruct())
				{
					VisitType(callee.ReturnType);
					source += " ";
				}

				if (callee.ReturnType.Class != Nodes::Type::Void)
				{
					source += "_return = ";
				}

				source += callee.Name;
				source += "(";

				if (callee.HasArguments())
				{
					const auto &arguments = this->mAST[callee.Arguments].As<Nodes::Aggregate>();
				
					for (size_t i = 0; i < arguments.Length; ++i)
					{
						const auto &arg = this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>();

						if (arg.Name == nullptr)
						{
							source += "_param_" + std::to_string(i) + ", ";
						}
						else if (strcmp(arg.Semantic, "SV_VERTEXID") == 0)
						{
							source += "gl_VertexID, ";
						}
						else if (strcmp(arg.Semantic, "SV_INSTANCEID") == 0)
						{
							source += "gl_InstanceID, ";
						}
						else if (strcmp(arg.Semantic, "SV_POSITION") == 0 && data.Type == Nodes::State::VertexShader)
						{
							source += "gl_Position, ";
						}
						else if (strcmp(arg.Semantic, "SV_POSITION") == 0 && data.Type == Nodes::State::PixelShader)
						{
							source += "gl_FragCoord, ";
						}
						else if (strcmp(arg.Semantic, "SV_DEPTH") == 0 && data.Type == Nodes::State::PixelShader)
						{
							source += "gl_FragDepth, ";
						}
						else
						{
							source += "_param_" + std::string(arg.Name) + ", ";
						}
					}

					source.pop_back();
					source.pop_back();
				}

				source += ");\n";

				if (callee.HasArguments())
				{
					const auto &arguments = this->mAST[callee.Arguments].As<Nodes::Aggregate>();
				
					for (size_t i = 0; i < arguments.Length; ++i)
					{
						const auto &arg = this->mAST[arguments.Find(this->mAST, i)].As<Nodes::Variable>();

						if (arg.Type.IsStruct())
						{
							if (this->mAST[arg.Type.Definition].As<Nodes::Struct>().Fields != 0)
							{
								const auto &fields = this->mAST[this->mAST[arg.Type.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

								for (size_t k = 0; k < fields.Length; ++k)
								{
									const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();
									const std::string paramname = "_param_" + std::string(arg.Name != nullptr ? arg.Name : std::to_string(i));

									source += paramname + "_" + std::string(field.Name) + " = " + paramname + ";\n";
								}
							}
						}
					}
				}
				if (callee.ReturnType.IsStruct())
				{
					const auto &fields = this->mAST[this->mAST[callee.ReturnType.Definition].As<Nodes::Struct>().Fields].As<Nodes::Aggregate>();

					for (size_t i = 0; i < fields.Length; ++i)
					{
						const auto &field = this->mAST[fields.Find(this->mAST, i)].As<Nodes::Variable>();

						source += "_return_" + std::string(field.Name) + " = _return." + std::string(field.Name) + ";\n";
					}
				}
			
				if (data.Type == Nodes::State::VertexShader)
				{
					source += "gl_Position = gl_Position * vec4(1.0, -1.0, 2.0, 1.0) - vec4(0.0, 0.0, gl_Position.w, 0.0);\n";
				}

				source += "}\n";

				GLenum type;

				switch (data.Type)
				{
					case Nodes::State::VertexShader:
						type = GL_VERTEX_SHADER;
						break;
					case Nodes::State::PixelShader:
						type = GL_FRAGMENT_SHADER;
						break;
					default:
						return;
				}

				shader = glCreateShader(type);

				const GLchar *src = source.c_str();
				const GLsizei len = static_cast<GLsizei>(source.length());

				glShaderSource(shader, 1, &src, &len);
				glCompileShader(shader);

				GLint status;
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
			OGL4Effect *										mEffect;
			std::string &										mErrors;
			bool												mFatal;
			std::string											mCurrentSource;
			std::string											mCurrentGlobalConstants;
			std::size_t											mCurrentGlobalSize;
			std::string											mCurrentBlockName;
			bool												mCurrentInParameterBlock, mCurrentInFunctionBlock;
		};

		// -----------------------------------------------------------------------------------------------------

		OGL4EffectContext::OGL4EffectContext(HDC device, HGLRC context) : mDeviceContext(device), mRenderContext(context)
		{
		}
		OGL4EffectContext::~OGL4EffectContext(void)
		{
		}

		void													OGL4EffectContext::GetDimension(unsigned int &width, unsigned int &height) const
		{
			RECT rect;
			HWND hwnd = ::WindowFromDC(this->mDeviceContext);
			::GetClientRect(hwnd, &rect);

			width = static_cast<unsigned int>(rect.right - rect.left);
			height = static_cast<unsigned int>(rect.bottom - rect.top);
		}

		std::unique_ptr<Effect>									OGL4EffectContext::Compile(const EffectTree &ast, std::string &errors)
		{
			OGL4Effect *effect = new OGL4Effect(shared_from_this());
			
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

		OGL4Effect::OGL4Effect(std::shared_ptr<OGL4EffectContext> context) : mEffectContext(context), mDefaultVAO(0), mDefaultVBO(0), mDefaultFBO(0), mUniformDirty(true)
		{
			GLCHECK(glGenVertexArrays(1, &this->mDefaultVAO));
			GLCHECK(glGenBuffers(1, &this->mDefaultVBO));
			GLCHECK(glGenFramebuffers(1, &this->mDefaultFBO));

			GLint prevBuffer = 0;
			GLCHECK(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevBuffer));
			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, this->mDefaultVBO));
			GLCHECK(glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW));
			GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, prevBuffer));
		}
		OGL4Effect::~OGL4Effect(void)
		{
			GLCHECK(glDeleteVertexArrays(1, &this->mDefaultVAO));
			GLCHECK(glDeleteBuffers(1, &this->mDefaultVBO));
			GLCHECK(glDeleteFramebuffers(1, &this->mDefaultFBO));			
			GLCHECK(glDeleteBuffers(this->mUniformBuffers.size(), &this->mUniformBuffers.front()));
		}

		Effect::Texture *										OGL4Effect::GetTexture(const std::string &name)
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Texture *									OGL4Effect::GetTexture(const std::string &name) const
		{
			auto it = this->mTextures.find(name);

			if (it == this->mTextures.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								OGL4Effect::GetTextureNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTextures.size());

			for (auto it = this->mTextures.begin(), end = this->mTextures.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		Effect::Constant *										OGL4Effect::GetConstant(const std::string &name)
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Constant *								OGL4Effect::GetConstant(const std::string &name) const
		{
			auto it = this->mConstants.find(name);

			if (it == this->mConstants.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								OGL4Effect::GetConstantNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mConstants.size());

			for (auto it = this->mConstants.begin(), end = this->mConstants.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}
		Effect::Technique *										OGL4Effect::GetTechnique(const std::string &name)
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		const Effect::Technique *								OGL4Effect::GetTechnique(const std::string &name) const
		{
			auto it = this->mTechniques.find(name);

			if (it == this->mTechniques.end())
			{
				return nullptr;
			}

			return it->second.get();
		}
		std::vector<std::string>								OGL4Effect::GetTechniqueNames(void) const
		{
			std::vector<std::string> names;
			names.reserve(this->mTechniques.size());

			for (auto it = this->mTechniques.begin(), end = this->mTechniques.end(); it != end; ++it)
			{
				names.push_back(it->first);
			}

			return names;
		}

		OGL4Texture::OGL4Texture(OGL4Effect *effect) : mEffect(effect), mID(0)
		{
		}
		OGL4Texture::~OGL4Texture(void)
		{
			GLCHECK(glDeleteTextures(1, &this->mID));
			GLCHECK(glDeleteTextures(1, &this->mSRGBView));
		}

		const Effect::Texture::Description						OGL4Texture::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								OGL4Texture::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													OGL4Texture::Resize(const Description &desc)
		{
			GLenum internalformat, internalformatSRGB;

			switch (desc.Format)
			{
				case Texture::Format::R8:
					internalformat = internalformatSRGB = GL_R8;
					break;
				case Texture::Format::R32F:
					internalformat = internalformatSRGB = GL_R32F;
					break;
				case Texture::Format::RG8:
					internalformat = internalformatSRGB = GL_RG8;
					break;
				case Texture::Format::RGBA8:
					internalformat = GL_RGBA8;
					internalformatSRGB = GL_SRGB8_ALPHA8;
					break;
				case Texture::Format::RGBA16:
					internalformat = internalformatSRGB = GL_RGBA16;
					break;
				case Texture::Format::RGBA16F:
					internalformat = internalformatSRGB = GL_RGBA16F;
					break;
				case Texture::Format::RGBA32F:
					internalformat = internalformatSRGB = GL_RGBA32F;
					break;
				case Texture::Format::DXT1:
					internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					internalformatSRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
					break;
				case Texture::Format::DXT3:
					internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
					internalformatSRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
					break;
				case Texture::Format::DXT5:
					internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					internalformatSRGB = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
					break;
				case Texture::Format::LATC1:
					internalformat = internalformatSRGB = GL_COMPRESSED_LUMINANCE_LATC1_EXT;
					break;
				case Texture::Format::LATC2:
					internalformat = internalformatSRGB = GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT;
					break;
				default:
					return false;
			}

			GLuint texture[2] = { 0, 0 }, previous = 0;
			GLCHECK(glGetIntegerv(TextureBindingFromTarget(this->mTarget), reinterpret_cast<GLint *>(&previous)));

			GLCHECK(glGenTextures(2, texture));
			GLCHECK(glBindTexture(this->mTarget, texture[0]));

			switch (this->mDimension)
			{
				case 1:
					GLCHECK(glTexStorage1D(this->mTarget, desc.Levels, internalformat, desc.Width));
					break;
				case 2:
					GLCHECK(glTexStorage2D(this->mTarget, desc.Levels, internalformat, desc.Width, desc.Height));
					break;
				case 3:
					GLCHECK(glTexStorage3D(this->mTarget, desc.Levels, internalformat, desc.Width, desc.Height, desc.Depth));
					break;
			}

			GLCHECK(glTextureView(texture[1], this->mTarget, texture[0], internalformatSRGB, 0, desc.Levels, 0, desc.Depth));
			GLCHECK(glBindTexture(this->mTarget, previous));

			GLCHECK(glDeleteTextures(1, &this->mID));
			GLCHECK(glDeleteTextures(1, &this->mSRGBView));

			this->mID = texture[0];
			this->mSRGBView = texture[1];
			this->mDesc = desc;
			this->mInternalFormat = internalformat;

			return true;
		}
		void													OGL4Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
		{
			GLCHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

			GLint previous = 0;
			GLCHECK(glGetIntegerv(TextureBindingFromTarget(this->mTarget), &previous));

			GLCHECK(glBindTexture(this->mTarget, this->mID));

			if (this->mDesc.Format >= Texture::Format::DXT1 && this->mDesc.Format <= Texture::Format::LATC2)
			{
				switch (this->mDimension)
				{
					case 1:
						GLCHECK(glCompressedTexSubImage1D(this->mTarget, level, 0, this->mDesc.Width, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), data));
						break;
					case 2:
						GLCHECK(glCompressedTexSubImage2D(this->mTarget, level, 0, 0, this->mDesc.Width, this->mDesc.Height, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), data));
						break;
					case 3:
						GLCHECK(glCompressedTexSubImage3D(this->mTarget, level, 0, 0, 0, this->mDesc.Width, this->mDesc.Height, this->mDesc.Depth, GL_UNSIGNED_BYTE, static_cast<GLsizei>(size), data));
						break;
				}
			}
			else
			{
				GLenum format = GL_NONE;

				switch (this->mDesc.Format)
				{
					case Texture::Format::R8:
					case Texture::Format::R32F:
						format = GL_RED;
						break;
					case Texture::Format::RG8:
						format = GL_RG;
						break;
					case Texture::Format::RGBA8:
					case Texture::Format::RGBA16:
					case Texture::Format::RGBA16F:
					case Texture::Format::RGBA32F:
						format = GL_RGBA;
						break;
				}

				switch (this->mDimension)
				{
					case 1:
						GLCHECK(glTexSubImage1D(this->mTarget, level, 0, this->mDesc.Width, format, GL_UNSIGNED_BYTE, data));
						break;
					case 2:
						GLCHECK(glTexSubImage2D(this->mTarget, level, 0, 0, this->mDesc.Width, this->mDesc.Height, format, GL_UNSIGNED_BYTE, data));
						break;
					case 3:
						GLCHECK(glTexSubImage3D(this->mTarget, level, 0, 0, 0, this->mDesc.Width, this->mDesc.Height, this->mDesc.Depth, format, GL_UNSIGNED_BYTE, data));
						break;
				}
			}

			GLCHECK(glBindTexture(this->mTarget, previous));
		}
		void													OGL4Texture::UpdateFromColorBuffer(void)
		{
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->mEffect->mDefaultFBO));
			GLCHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->mTarget, this->mID, 0));

#ifdef _DEBUG
			GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
			assert(status == GL_FRAMEBUFFER_COMPLETE);
#endif

			GLCHECK(glReadBuffer(GL_BACK));
			GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));

			unsigned int backWidth = 0, backHeight = 0;
			this->mEffect->mEffectContext->GetDimension(backWidth, backHeight);

			GLCHECK(glBlitFramebuffer(0, 0, backWidth, backHeight, 0, 0, this->mDesc.Width, this->mDesc.Height, GL_COLOR_BUFFER_BIT, GL_NEAREST));
		}
		void													OGL4Texture::UpdateFromDepthBuffer(void)
		{
			GLCHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));
			GLCHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->mEffect->mDefaultFBO));
			GLCHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, this->mTarget, this->mID, 0));

			GLCHECK(glReadBuffer(GL_BACK));
			GLCHECK(glDrawBuffer(GL_COLOR_ATTACHMENT0));

			unsigned int backWidth = 0, backHeight = 0;
			this->mEffect->mEffectContext->GetDimension(backWidth, backHeight);

			GLCHECK(glBlitFramebuffer(0, 0, backWidth, backHeight, 0, 0, this->mDesc.Width, this->mDesc.Height, GL_DEPTH_BUFFER_BIT, GL_NEAREST));
		}

		OGL4Constant::OGL4Constant(OGL4Effect *effect) : mEffect(effect)
		{
		}
		OGL4Constant::~OGL4Constant(void)
		{
		}

		const Effect::Constant::Description						OGL4Constant::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								OGL4Constant::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}
		void													OGL4Constant::GetValue(unsigned char *data, std::size_t size) const
		{
			size = std::min(size, this->mDesc.Size);

			const unsigned char *storage = this->mEffect->mUniformStorages[this->mBuffer].first + this->mBufferOffset;

			std::memcpy(data, storage, size);
		}
		void													OGL4Constant::SetValue(const unsigned char *data, std::size_t size)
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

		OGL4Technique::OGL4Technique(OGL4Effect *effect) : mEffect(effect)
		{
		}
		OGL4Technique::~OGL4Technique(void)
		{
			for (auto &pass : this->mPasses)
			{
				GLCHECK(glDeleteProgram(pass.Program));
				GLCHECK(glDeleteFramebuffers(1, &pass.Framebuffer));
			}
		}

		const Effect::Technique::Description					OGL4Technique::GetDescription(void) const
		{
			return this->mDesc;
		}
		const Effect::Annotation								OGL4Technique::GetAnnotation(const std::string &name) const
		{
			auto it = this->mAnnotations.find(name);

			if (it == this->mAnnotations.end())
			{
				return Effect::Annotation();
			}

			return it->second;
		}

		bool													OGL4Technique::Begin(unsigned int &passes) const
		{
			passes = static_cast<unsigned int>(this->mPasses.size());

			GLCHECK(glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&this->mStateblock.Program)));
			GLCHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&this->mStateblock.Framebuffer)));
			GLCHECK(this->mStateblock.DepthTest = glIsEnabled(GL_DEPTH_TEST));
			GLCHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &this->mStateblock.DepthMask));
			GLCHECK(this->mStateblock.StencilTest = glIsEnabled(GL_STENCIL_TEST));
			GLCHECK(this->mStateblock.ScissorTest = glIsEnabled(GL_SCISSOR_TEST));
			GLCHECK(glGetIntegerv(GL_FRONT_FACE, reinterpret_cast<GLint *>(&this->mStateblock.FrontFace)));
			GLCHECK(glGetIntegerv(GL_POLYGON_MODE, reinterpret_cast<GLint *>(&this->mStateblock.PolygonMode)));
			GLCHECK(this->mStateblock.CullFace = glIsEnabled(GL_CULL_FACE));
			GLCHECK(this->mStateblock.SampleAlphaToCoverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
			GLCHECK(this->mStateblock.Blend = glIsEnabled(GL_BLEND));
			GLCHECK(glGetIntegerv(GL_STENCIL_VALUE_MASK, reinterpret_cast<GLint *>(&this->mStateblock.StencilReadMask)));
			GLCHECK(glGetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint *>(&this->mStateblock.StencilMask)));
			GLCHECK(glGetIntegerv(GL_STENCIL_FUNC, reinterpret_cast<GLint *>(&this->mStateblock.StencilFunc)));
			GLCHECK(glGetIntegerv(GL_STENCIL_FAIL, reinterpret_cast<GLint *>(&this->mStateblock.StencilOpFail)));
			GLCHECK(glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, reinterpret_cast<GLint *>(&this->mStateblock.StencilOpZFail)));
			GLCHECK(glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, reinterpret_cast<GLint *>(&this->mStateblock.StencilOpZPass)));
			GLCHECK(glGetIntegerv(GL_STENCIL_REF, &this->mStateblock.StencilRef));

			GLCHECK(glBindVertexArray(this->mEffect->mDefaultVAO));
			GLCHECK(glBindVertexBuffer(0, this->mEffect->mDefaultVBO, 0, sizeof(float)));		

			for (GLuint i = 0, count = static_cast<GLuint>(this->mEffect->mSamplers.size()); i < count; ++i)
			{
				const auto &sampler = this->mEffect->mSamplers[i];
				const auto &texture = sampler->mTexture;

				GLCHECK(glActiveTexture(GL_TEXTURE0 + i));
				GLCHECK(glBindTexture(texture->mTarget, sampler->mSRGB ? texture->mSRGBView : texture->mID));
				GLCHECK(glBindSampler(i, sampler->mID));
			}
			for (GLuint i = 0, count = static_cast<GLuint>(this->mEffect->mUniformBuffers.size()); i < count; ++i)
			{
				GLCHECK(glBindBufferBase(GL_UNIFORM_BUFFER, i, this->mEffect->mUniformBuffers[i]));
			}

			return true;
		}
		void													OGL4Technique::End(void) const
		{
#define glEnableb(cap, value) if ((value)) glEnable(cap); else glDisable(cap);

			GLCHECK(glUseProgram(this->mStateblock.Program));
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, this->mStateblock.Framebuffer));
			GLCHECK(glEnableb(GL_DEPTH_TEST, this->mStateblock.DepthTest));
			GLCHECK(glDepthMask(this->mStateblock.DepthMask));
			GLCHECK(glEnableb(GL_STENCIL_TEST, this->mStateblock.StencilTest));
			GLCHECK(glEnableb(GL_SCISSOR_TEST, this->mStateblock.ScissorTest));
			GLCHECK(glFrontFace(this->mStateblock.FrontFace));
			GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, this->mStateblock.PolygonMode));
			GLCHECK(glEnableb(GL_CULL_FACE, this->mStateblock.CullFace));
			GLCHECK(glEnableb(GL_SAMPLE_ALPHA_TO_COVERAGE, this->mStateblock.SampleAlphaToCoverage));
			GLCHECK(glEnableb(GL_BLEND, this->mStateblock.Blend));
			GLCHECK(glStencilMask(this->mStateblock.StencilMask));
			GLCHECK(glStencilFunc(this->mStateblock.StencilFunc, this->mStateblock.StencilRef, this->mStateblock.StencilReadMask));
			GLCHECK(glStencilOp(this->mStateblock.StencilOpFail, this->mStateblock.StencilOpZFail, this->mStateblock.StencilOpZPass));
		}
		void													OGL4Technique::RenderPass(unsigned int index) const
		{
			if (this->mEffect->mUniformDirty)
			{
				for (GLuint i = 0, count = static_cast<GLuint>(this->mEffect->mUniformBuffers.size()); i < count; ++i)
				{
					if (this->mEffect->mUniformBuffers[i] == 0)
					{
						continue;
					}

					GLCHECK(glBindBuffer(GL_UNIFORM_BUFFER, this->mEffect->mUniformBuffers[i]));
					GLCHECK(glBufferSubData(GL_UNIFORM_BUFFER, 0, this->mEffect->mUniformStorages[i].second, this->mEffect->mUniformStorages[i].first));
				}
			}

			const Pass &pass = this->mPasses[index];

			GLCHECK(glUseProgram(pass.Program));
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, pass.Framebuffer));

			if (pass.Framebuffer == 0)
			{
				GLCHECK(glDrawBuffer(GL_BACK));
			}
			else
			{
				GLCHECK(glDrawBuffers(8, pass.DrawBuffers));
			}

			GLCHECK(glEnableb(GL_SCISSOR_TEST, pass.ScissorTest));
			GLCHECK(glFrontFace(pass.FrontFace));
			GLCHECK(glPolygonMode(GL_FRONT_AND_BACK, pass.PolygonMode));

			if (pass.CullFace != GL_NONE)
			{
				GLCHECK(glEnable(GL_CULL_FACE));
				GLCHECK(glCullFace(pass.CullFace));
			}
			else
			{
				GLCHECK(glDisable(GL_CULL_FACE));
			}

			GLCHECK(glColorMask(pass.ColorMaskR, pass.ColorMaskG, pass.ColorMaskB, pass.ColorMaskA));

			if (pass.SampleAlphaToCoverage)
			{
				GLCHECK(glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE));
			}
			else
			{
				GLCHECK(glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE));
			}

			if (pass.Blend)
			{
				GLCHECK(glEnable(GL_BLEND));
				GLCHECK(glBlendFunc(pass.BlendFuncSrc, pass.BlendFuncDest));
				GLCHECK(glBlendEquationSeparate(pass.BlendEqColor, pass.BlendEqAlpha));
			}
			else
			{
				GLCHECK(glDisable(GL_BLEND));
			}

			if (pass.DepthMask)
			{
				GLCHECK(glEnable(GL_DEPTH_TEST));
				GLCHECK(glDepthMask(GL_TRUE));
				GLCHECK(glDepthFunc(pass.DepthTest ? pass.DepthFunc : GL_ALWAYS));
			}
			else
			{
				GLCHECK(glDepthMask(GL_FALSE));

				if (pass.DepthTest)
				{
					GLCHECK(glEnable(GL_DEPTH_TEST));
					GLCHECK(glDepthFunc(pass.DepthFunc));
				}
				else
				{
					GLCHECK(glDisable(GL_DEPTH_TEST));
				}
			}

			if (pass.StencilTest)
			{
				GLCHECK(glEnable(GL_STENCIL_TEST));
				GLCHECK(glStencilFunc(pass.StencilFunc, pass.StencilRef, pass.StencilReadMask));
				GLCHECK(glStencilOp(pass.StencilOpFail, pass.StencilOpZFail, pass.StencilOpZPass));
				GLCHECK(glStencilMask(pass.StencilMask));
			}
			else
			{
				GLCHECK(glDisable(GL_STENCIL_TEST));
			}

			GLCHECK(glDrawArrays(GL_TRIANGLES, 0, 3));
		}
	}

	// -----------------------------------------------------------------------------------------------------

	std::shared_ptr<EffectContext>								CreateEffectContext(HDC hdc, HGLRC hglrc)
	{
		assert(hdc != nullptr && hglrc != nullptr);
		assert(wglGetCurrentDC() == hdc && wglGetCurrentContext() == hglrc);

		gl3wInit();

		if (!gl3wIsSupported(4, 3))
		{
			return nullptr;
		}

		return std::make_shared<OGL4EffectContext>(hdc, hglrc);
	}
}