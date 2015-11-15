#include "Log.hpp"
#include "D3D10Runtime.hpp"
#include "FX\ParserNodes.hpp"
#include "GUI.hpp"
#include "WindowWatcher.hpp"

#include <assert.h>
#include <d3dcompiler.h>
#include <nanovg_d3d10.h>
#include <boost\algorithm\string.hpp>

// ---------------------------------------------------------------------------------------------------

namespace ReShade
{
	namespace Runtimes
	{
		namespace
		{
			template <typename T>
			inline ULONG SAFE_RELEASE(T *&object)
			{
				if (object == nullptr)
				{
					return 0;
				}

				const ULONG ref = object->Release();

				object = nullptr;

				return ref;
			}

			struct D3D10Technique : public Technique
			{
				struct Pass
				{
					ID3D10VertexShader *VS;
					ID3D10PixelShader *PS;
					ID3D10BlendState *BS;
					ID3D10DepthStencilState *DSS;
					UINT StencilRef;
					ID3D10RenderTargetView *RT[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
					ID3D10ShaderResourceView *RTSRV[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
					D3D10_VIEWPORT Viewport;
					std::vector<ID3D10ShaderResourceView *> SRV;
				};

				~D3D10Technique()
				{
					for (auto &pass : this->Passes)
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

				std::vector<Pass> Passes;
			};
			struct D3D10Texture : public Texture
			{
				enum class Source
				{
					None,
					Memory,
					BackBuffer,
					DepthStencil
				};

				D3D10Texture() : DataSource(Source::None), ShaderRegister(0), TextureInterface(nullptr), ShaderResourceView(), RenderTargetView()
				{
				}
				~D3D10Texture()
				{
					SAFE_RELEASE(this->RenderTargetView[0]);
					SAFE_RELEASE(this->RenderTargetView[1]);
					SAFE_RELEASE(this->ShaderResourceView[0]);
					SAFE_RELEASE(this->ShaderResourceView[1]);

					SAFE_RELEASE(this->TextureInterface);
				}

				void ChangeDataSource(Source source, D3D10Runtime *runtime, ID3D10ShaderResourceView *srv, ID3D10ShaderResourceView *srvSRGB)
				{
					this->DataSource = source;

					if (srvSRGB == nullptr)
					{
						srvSRGB = srv;
					}

					if (srv == this->ShaderResourceView[0] && srvSRGB == this->ShaderResourceView[1])
					{
						return;
					}

					SAFE_RELEASE(this->RenderTargetView[0]);
					SAFE_RELEASE(this->RenderTargetView[1]);
					SAFE_RELEASE(this->ShaderResourceView[0]);
					SAFE_RELEASE(this->ShaderResourceView[1]);

					SAFE_RELEASE(this->TextureInterface);

					if (srv != nullptr)
					{
						this->ShaderResourceView[0] = srv;
						this->ShaderResourceView[0]->AddRef();
						this->ShaderResourceView[0]->GetResource(reinterpret_cast<ID3D10Resource **>(&this->TextureInterface));
						this->ShaderResourceView[1] = srvSRGB;
						this->ShaderResourceView[1]->AddRef();

						D3D10_TEXTURE2D_DESC texdesc;
						this->TextureInterface->GetDesc(&texdesc);

						this->Width = texdesc.Width;
						this->Height = texdesc.Height;
						this->Format = Texture::PixelFormat::Unknown;
						this->Levels = texdesc.MipLevels;
					}
					else
					{
						this->Width = this->Height = this->Levels = 0;
						this->Format = Texture::PixelFormat::Unknown;
					}

					// Update techniques shader resourceviews
					for (auto &technique : runtime->GetTechniques())
					{
						for (auto &pass : static_cast<D3D10Technique *>(technique.get())->Passes)
						{
							pass.SRV[this->ShaderRegister] = this->ShaderResourceView[0];
							pass.SRV[this->ShaderRegister + 1] = this->ShaderResourceView[1];
						}
					}
				}

				Source DataSource;
				size_t ShaderRegister;
				ID3D10Texture2D *TextureInterface;
				ID3D10ShaderResourceView *ShaderResourceView[2];
				ID3D10RenderTargetView *RenderTargetView[2];
			};

			class D3D10EffectCompiler : private boost::noncopyable
			{
			public:
				D3D10EffectCompiler(const FX::NodeTree &ast, bool skipoptimization = false) : mAST(ast), mRuntime(nullptr), mFatal(false), mSkipShaderOptimization(skipoptimization), mCurrentInParameterBlock(false), mCurrentInFunctionBlock(false), mCurrentInForInitialization(0), mCurrentGlobalSize(0)
				{
				}

				bool Traverse(D3D10Runtime *runtime, std::string &errors)
				{
					this->mRuntime = runtime;
					this->mErrors.clear();
					this->mFatal = false;
					this->mCurrentSource.clear();

					for (auto type : this->mAST.Structs)
					{
						Visit(static_cast<FX::Nodes::Struct *>(type));
					}
					for (auto uniform : this->mAST.Uniforms)
					{
						Visit(static_cast<FX::Nodes::Variable *>(uniform));
					}
					for (auto function : this->mAST.Functions)
					{
						Visit(static_cast<FX::Nodes::Function *>(function));
					}
					for (auto technique : this->mAST.Techniques)
					{
						Visit(static_cast<FX::Nodes::Technique *>(technique));
					}

					if (this->mCurrentGlobalSize != 0)
					{
						CD3D10_BUFFER_DESC globalsDesc(RoundToMultipleOf16(this->mCurrentGlobalSize), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
						D3D10_SUBRESOURCE_DATA globalsInitial;
						globalsInitial.pSysMem = this->mRuntime->GetConstantStorage();
						globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = this->mRuntime->mConstantBufferSize = this->mCurrentGlobalSize;
						this->mRuntime->mDevice->CreateBuffer(&globalsDesc, &globalsInitial, &this->mRuntime->mConstantBuffer);
					}

					errors += this->mErrors;

					return !this->mFatal;
				}

				static inline UINT RoundToMultipleOf16(UINT size)
				{
					return (size + 15) & ~15;
				}
				static D3D10_STENCIL_OP LiteralToStencilOp(unsigned int value)
				{
					if (value == FX::Nodes::Pass::States::ZERO)
					{
						return D3D10_STENCIL_OP_ZERO;
					}
							
					return static_cast<D3D10_STENCIL_OP>(value);
				}
				static D3D10_BLEND LiteralToBlend(unsigned int value)
				{
					switch (value)
					{
						case FX::Nodes::Pass::States::ZERO:
							return D3D10_BLEND_ZERO;
						case FX::Nodes::Pass::States::ONE:
							return D3D10_BLEND_ONE;
					}

					return static_cast<D3D10_BLEND>(value);
				}
				static DXGI_FORMAT LiteralToFormat(unsigned int value, Texture::PixelFormat &name)
				{
					switch (value)
					{
						case FX::Nodes::Variable::Properties::R8:
							name = Texture::PixelFormat::R8;
							return DXGI_FORMAT_R8_UNORM;
						case FX::Nodes::Variable::Properties::R16F:
							name = Texture::PixelFormat::R16F;
							return DXGI_FORMAT_R16_FLOAT;
						case FX::Nodes::Variable::Properties::R32F:
							name = Texture::PixelFormat::R32F;
							return DXGI_FORMAT_R32_FLOAT;
						case FX::Nodes::Variable::Properties::RG8:
							name = Texture::PixelFormat::RG8;
							return DXGI_FORMAT_R8G8_UNORM;
						case FX::Nodes::Variable::Properties::RG16:
							name = Texture::PixelFormat::RG16;
							return DXGI_FORMAT_R16G16_UNORM;
						case FX::Nodes::Variable::Properties::RG16F:
							name = Texture::PixelFormat::RG16F;
							return DXGI_FORMAT_R16G16_FLOAT;
						case FX::Nodes::Variable::Properties::RG32F:
							name = Texture::PixelFormat::RG32F;
							return DXGI_FORMAT_R32G32_FLOAT;
						case FX::Nodes::Variable::Properties::RGBA8:
							name = Texture::PixelFormat::RGBA8;
							return DXGI_FORMAT_R8G8B8A8_TYPELESS;
						case FX::Nodes::Variable::Properties::RGBA16:
							name = Texture::PixelFormat::RGBA16;
							return DXGI_FORMAT_R16G16B16A16_UNORM;
						case FX::Nodes::Variable::Properties::RGBA16F:
							name = Texture::PixelFormat::RGBA16F;
							return DXGI_FORMAT_R16G16B16A16_FLOAT;
						case FX::Nodes::Variable::Properties::RGBA32F:
							name = Texture::PixelFormat::RGBA32F;
							return DXGI_FORMAT_R32G32B32A32_FLOAT;
						case FX::Nodes::Variable::Properties::DXT1:
							name = Texture::PixelFormat::DXT1;
							return DXGI_FORMAT_BC1_TYPELESS;
						case FX::Nodes::Variable::Properties::DXT3:
							name = Texture::PixelFormat::DXT3;
							return DXGI_FORMAT_BC2_TYPELESS;
						case FX::Nodes::Variable::Properties::DXT5:
							name = Texture::PixelFormat::DXT5;
							return DXGI_FORMAT_BC3_TYPELESS;
						case FX::Nodes::Variable::Properties::LATC1:
							name = Texture::PixelFormat::LATC1;
							return DXGI_FORMAT_BC4_UNORM;
						case FX::Nodes::Variable::Properties::LATC2:
							name = Texture::PixelFormat::LATC2;
							return DXGI_FORMAT_BC5_UNORM;
						default:
							name = Texture::PixelFormat::Unknown;
							return DXGI_FORMAT_UNKNOWN;
					}
				}
				static DXGI_FORMAT MakeTypelessFormat(DXGI_FORMAT format)
				{
					switch (format)
					{
						case DXGI_FORMAT_R8G8B8A8_UNORM:
						case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
							return DXGI_FORMAT_R8G8B8A8_TYPELESS;
						case DXGI_FORMAT_BC1_UNORM:
						case DXGI_FORMAT_BC1_UNORM_SRGB:
							return DXGI_FORMAT_BC1_TYPELESS;
						case DXGI_FORMAT_BC2_UNORM:
						case DXGI_FORMAT_BC2_UNORM_SRGB:
							return DXGI_FORMAT_BC2_TYPELESS;
						case DXGI_FORMAT_BC3_UNORM:
						case DXGI_FORMAT_BC3_UNORM_SRGB:
							return DXGI_FORMAT_BC3_TYPELESS;
						default:
							return format;
					}
				}
				static DXGI_FORMAT MakeSRGBFormat(DXGI_FORMAT format)
				{
					switch (format)
					{
						case DXGI_FORMAT_R8G8B8A8_TYPELESS:
						case DXGI_FORMAT_R8G8B8A8_UNORM:
							return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
						case DXGI_FORMAT_BC1_TYPELESS:
						case DXGI_FORMAT_BC1_UNORM:
							return DXGI_FORMAT_BC1_UNORM_SRGB;
						case DXGI_FORMAT_BC2_TYPELESS:
						case DXGI_FORMAT_BC2_UNORM:
							return DXGI_FORMAT_BC2_UNORM_SRGB;
						case DXGI_FORMAT_BC3_TYPELESS:
						case DXGI_FORMAT_BC3_UNORM:
							return DXGI_FORMAT_BC3_UNORM_SRGB;
						default:
							return format;
					}
				}
				static DXGI_FORMAT MakeNonSRGBFormat(DXGI_FORMAT format)
				{
					switch (format)
					{
						case DXGI_FORMAT_R8G8B8A8_TYPELESS:
						case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
							return DXGI_FORMAT_R8G8B8A8_UNORM;
						case DXGI_FORMAT_BC1_TYPELESS:
						case DXGI_FORMAT_BC1_UNORM_SRGB:
							return DXGI_FORMAT_BC1_UNORM;
						case DXGI_FORMAT_BC2_TYPELESS:
						case DXGI_FORMAT_BC2_UNORM_SRGB:
							return DXGI_FORMAT_BC2_UNORM;
						case DXGI_FORMAT_BC3_TYPELESS:
						case DXGI_FORMAT_BC3_UNORM_SRGB:
							return DXGI_FORMAT_BC3_UNORM;
						default:
							return format;
					}
				}
				static size_t D3D10_SAMPLER_DESC_HASH(const D3D10_SAMPLER_DESC &s) 
				{
					const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
					size_t h = 2166136261;

					for (size_t i = 0; i < sizeof(D3D10_SAMPLER_DESC); ++i)
					{
						h = (h * 16777619) ^ p[i];
					}

					return h;
				}

				static std::string ConvertSemantic(const std::string &semantic)
				{
					if (semantic == "VERTEXID")
					{
						return "SV_VERTEXID";
					}
					else if (semantic == "POSITION" || semantic == "VPOS")
					{
						return "SV_POSITION";
					}
					else if (boost::starts_with(semantic, "COLOR"))
					{
						return "SV_TARGET" + semantic.substr(5);
					}
					else if (semantic == "DEPTH")
					{
						return "SV_DEPTH";
					}

					return semantic;
				}
				static inline std::string PrintLocation(const FX::Location &location)
				{
					return location.Source + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): ";
				}
				std::string PrintType(const FX::Nodes::Type &type)
				{
					std::string res;

					switch (type.BaseClass)
					{
						case FX::Nodes::Type::Class::Void:
							res += "void";
							break;
						case FX::Nodes::Type::Class::Bool:
							res += "bool";
							break;
						case FX::Nodes::Type::Class::Int:
							res += "int";
							break;
						case FX::Nodes::Type::Class::Uint:
							res += "uint";
							break;
						case FX::Nodes::Type::Class::Float:
							res += "float";
							break;
						case FX::Nodes::Type::Class::Sampler2D:
							res += "__sampler2D";
							break;
						case FX::Nodes::Type::Class::Struct:
							res += PrintName(type.Definition);
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
				std::string PrintTypeWithQualifiers(const FX::Nodes::Type &type)
				{
					std::string qualifiers;

					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Extern))
						qualifiers += "extern ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Static))
						qualifiers += "static ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Const))
						qualifiers += "const ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Volatile))
						qualifiers += "volatile ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Precise))
						qualifiers += "precise ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Linear))
						qualifiers += "linear ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::NoPerspective))
						qualifiers += "noperspective ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Centroid))
						qualifiers += "centroid ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::NoInterpolation))
						qualifiers += "nointerpolation ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::InOut))
						qualifiers += "inout ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::In))
						qualifiers += "in ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::Out))
						qualifiers += "out ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::Uniform))
						qualifiers += "uniform ";

					return qualifiers + PrintType(type);
				}
				inline std::string PrintName(const FX::Nodes::Declaration *declaration)
				{
					return boost::replace_all_copy(declaration->Namespace, "::", "_NS") + declaration->Name;
				}

				void Visit(const FX::Nodes::Statement *node)
				{
					if (node == nullptr)
					{
						return;
					}

					switch (node->NodeId)
					{
						case FX::Node::Id::Compound:
							Visit(static_cast<const FX::Nodes::Compound *>(node));
							break;
						case FX::Node::Id::DeclaratorList:
							Visit(static_cast<const FX::Nodes::DeclaratorList *>(node));
							break;
						case FX::Node::Id::ExpressionStatement:
							Visit(static_cast<const FX::Nodes::ExpressionStatement *>(node));
							break;
						case FX::Node::Id::If:
							Visit(static_cast<const FX::Nodes::If *>(node));
							break;
						case FX::Node::Id::Switch:
							Visit(static_cast<const FX::Nodes::Switch *>(node));
							break;
						case FX::Node::Id::For:
							Visit(static_cast<const FX::Nodes::For *>(node));
							break;
						case FX::Node::Id::While:
							Visit(static_cast<const FX::Nodes::While *>(node));
							break;
						case FX::Node::Id::Return:
							Visit(static_cast<const FX::Nodes::Return *>(node));
							break;
						case FX::Node::Id::Jump:
							Visit(static_cast<const FX::Nodes::Jump *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}
				void Visit(const FX::Nodes::Expression *node)
				{
					switch (node->NodeId)
					{
						case FX::Node::Id::LValue:
							Visit(static_cast<const FX::Nodes::LValue *>(node));
							break;
						case FX::Node::Id::Literal:
							Visit(static_cast<const FX::Nodes::Literal *>(node));
							break;
						case FX::Node::Id::Sequence:
							Visit(static_cast<const FX::Nodes::Sequence *>(node));
							break;
						case FX::Node::Id::Unary:
							Visit(static_cast<const FX::Nodes::Unary *>(node));
							break;
						case FX::Node::Id::Binary:
							Visit(static_cast<const FX::Nodes::Binary *>(node));
							break;
						case FX::Node::Id::Intrinsic:
							Visit(static_cast<const FX::Nodes::Intrinsic *>(node));
							break;
						case FX::Node::Id::Conditional:
							Visit(static_cast<const FX::Nodes::Conditional *>(node));
							break;
						case FX::Node::Id::Swizzle:
							Visit(static_cast<const FX::Nodes::Swizzle *>(node));
							break;
						case FX::Node::Id::FieldSelection:
							Visit(static_cast<const FX::Nodes::FieldSelection *>(node));
							break;
						case FX::Node::Id::Assignment:
							Visit(static_cast<const FX::Nodes::Assignment *>(node));
							break;
						case FX::Node::Id::Call:
							Visit(static_cast<const FX::Nodes::Call *>(node));
							break;
						case FX::Node::Id::Constructor:
							Visit(static_cast<const FX::Nodes::Constructor *>(node));
							break;
						case FX::Node::Id::InitializerList:
							Visit(static_cast<const FX::Nodes::InitializerList *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}

				void Visit(const FX::Nodes::Compound *node)
				{
					this->mCurrentSource += "{\n";

					for (auto statement : node->Statements)
					{
						Visit(statement);
					}

					this->mCurrentSource += "}\n";
				}
				void Visit(const FX::Nodes::DeclaratorList *node)
				{
					for (auto declarator : node->Declarators)
					{
						Visit(declarator);

						if (this->mCurrentInForInitialization)
						{
							this->mCurrentSource += ", ";
							this->mCurrentInForInitialization++;
						}
						else
						{
							this->mCurrentSource += ";\n";
						}
					}
				}
				void Visit(const FX::Nodes::ExpressionStatement *node)
				{
					Visit(node->Expression);

					this->mCurrentSource += ";\n";
				}
				void Visit(const FX::Nodes::If *node)
				{
					for (auto &attribute : node->Attributes)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += attribute;
						this->mCurrentSource += ']';
					}

					this->mCurrentSource += "if (";
					Visit(node->Condition);
					this->mCurrentSource += ")\n";

					if (node->StatementOnTrue != nullptr)
					{
						Visit(node->StatementOnTrue);
					}
					else
					{
						this->mCurrentSource += "\t;";
					}
					
					if (node->StatementOnFalse != nullptr)
					{
						this->mCurrentSource += "else\n";
						
						Visit(node->StatementOnFalse);
					}
				}
				void Visit(const FX::Nodes::Switch *node)
				{
					for (auto &attribute : node->Attributes)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += attribute;
						this->mCurrentSource += ']';
					}

					this->mCurrentSource += "switch (";
					Visit(node->Test);
					this->mCurrentSource += ")\n{\n";

					for (auto cases : node->Cases)
					{
						Visit(cases);
					}

					this->mCurrentSource += "}\n";
				}
				void Visit(const FX::Nodes::Case *node)
				{
					for (auto label : node->Labels)
					{
						if (label == nullptr)
						{
							this->mCurrentSource += "default";
						}
						else
						{
							this->mCurrentSource += "case ";

							Visit(label);
						}

						this->mCurrentSource += ":\n";
					}

					Visit(node->Statements);
				}
				void Visit(const FX::Nodes::For *node)
				{
					for (auto &attribute : node->Attributes)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += attribute;
						this->mCurrentSource += ']';
					}

					this->mCurrentSource += "for (";

					if (node->Initialization != nullptr)
					{
						this->mCurrentInForInitialization = 1;

						Visit(node->Initialization);

						this->mCurrentInForInitialization = 0;

						this->mCurrentSource.pop_back();
						this->mCurrentSource.pop_back();
					}

					this->mCurrentSource += "; ";
										
					if (node->Condition != nullptr)
					{
						Visit(node->Condition);
					}

					this->mCurrentSource += "; ";

					if (node->Increment != nullptr)
					{
						Visit(node->Increment);
					}

					this->mCurrentSource += ")\n";

					if (node->Statements != nullptr)
					{
						Visit(node->Statements);
					}
					else
					{
						this->mCurrentSource += "\t;";
					}
				}
				void Visit(const FX::Nodes::While *node)
				{
					for (auto &attribute : node->Attributes)
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += attribute;
						this->mCurrentSource += ']';
					}

					if (node->DoWhile)
					{
						this->mCurrentSource += "do\n{\n";

						if (node->Statements != nullptr)
						{
							Visit(node->Statements);
						}

						this->mCurrentSource += "}\n";
						this->mCurrentSource += "while (";
						Visit(node->Condition);
						this->mCurrentSource += ");\n";
					}
					else
					{
						this->mCurrentSource += "while (";
						Visit(node->Condition);
						this->mCurrentSource += ")\n";

						if (node->Statements != nullptr)
						{
							Visit(node->Statements);
						}
						else
						{
							this->mCurrentSource += "\t;";
						}
					}
				}
				void Visit(const FX::Nodes::Return *node)
				{
					if (node->Discard)
					{
						this->mCurrentSource += "discard";
					}
					else
					{
						this->mCurrentSource += "return";

						if (node->Value != nullptr)
						{
							this->mCurrentSource += ' ';

							Visit(node->Value);
						}
					}

					this->mCurrentSource += ";\n";
				}
				void Visit(const FX::Nodes::Jump *node)
				{
					switch (node->Mode)
					{
						case FX::Nodes::Jump::Break:
							this->mCurrentSource += "break";
							break;
						case FX::Nodes::Jump::Continue:
							this->mCurrentSource += "continue";
							break;
					}

					this->mCurrentSource += ";\n";
				}

				void Visit(const FX::Nodes::LValue *node)
				{
					this->mCurrentSource += PrintName(node->Reference);
				}
				void Visit(const FX::Nodes::Literal *node)
				{
					if (!node->Type.IsScalar())
					{
						this->mCurrentSource += PrintType(node->Type);
						this->mCurrentSource += '(';
					}

					for (unsigned int i = 0; i < node->Type.Rows * node->Type.Cols; ++i)
					{
						switch (node->Type.BaseClass)
						{
							case FX::Nodes::Type::Class::Bool:
								this->mCurrentSource += node->Value.Int[i] ? "true" : "false";
								break;
							case FX::Nodes::Type::Class::Int:
								this->mCurrentSource += std::to_string(node->Value.Int[i]);
								break;
							case FX::Nodes::Type::Class::Uint:
								this->mCurrentSource += std::to_string(node->Value.Uint[i]);
								break;
							case FX::Nodes::Type::Class::Float:
								this->mCurrentSource += std::to_string(node->Value.Float[i]) + "f";
								break;
						}

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();

					if (!node->Type.IsScalar())
					{
						this->mCurrentSource += ')';
					}
				}
				void Visit(const FX::Nodes::Sequence *node)
				{
					this->mCurrentSource += '(';

					for (auto expression : node->Expressions)
					{
						Visit(expression);

						this->mCurrentSource += ", ";
					}
					
					this->mCurrentSource.pop_back();
					this->mCurrentSource.pop_back();

					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::Unary *node)
				{
					std::string part1, part2;

					switch (node->Operator)
					{
						case FX::Nodes::Unary::Op::Negate:
							part1 = '-';
							break;
						case FX::Nodes::Unary::Op::BitwiseNot:
							part1 = "~";
							break;
						case FX::Nodes::Unary::Op::LogicalNot:
							part1 = '!';
							break;
						case FX::Nodes::Unary::Op::Increase:
							part1 = "++";
							break;
						case FX::Nodes::Unary::Op::Decrease:
							part1 = "--";
							break;
						case FX::Nodes::Unary::Op::PostIncrease:
							part2 = "++";
							break;
						case FX::Nodes::Unary::Op::PostDecrease:
							part2 = "--";
							break;
						case FX::Nodes::Unary::Op::Cast:
							part1 = PrintType(node->Type) + '(';
							part2 = ')';
							break;
					}

					this->mCurrentSource += part1;
					Visit(node->Operand);
					this->mCurrentSource += part2;
				}
				void Visit(const FX::Nodes::Binary *node)
				{
					std::string part1, part2, part3;

					switch (node->Operator)
					{
						case FX::Nodes::Binary::Op::Add:
							part1 = '(';
							part2 = " + ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Subtract:
							part1 = '(';
							part2 = " - ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Multiply:
							part1 = '(';
							part2 = " * ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Divide:
							part1 = '(';
							part2 = " / ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Modulo:
							part1 = '(';
							part2 = " % ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Less:
							part1 = '(';
							part2 = " < ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Greater:
							part1 = '(';
							part2 = " > ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::LessOrEqual:
							part1 = '(';
							part2 = " <= ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::GreaterOrEqual:
							part1 = '(';
							part2 = " >= ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::Equal:
							part1 = '(';
							part2 = " == ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::NotEqual:
							part1 = '(';
							part2 = " != ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::LeftShift:
							part1 = "(";
							part2 = " << ";
							part3 = ")";
							break;
						case FX::Nodes::Binary::Op::RightShift:
							part1 = "(";
							part2 = " >> ";
							part3 = ")";
							break;
						case FX::Nodes::Binary::Op::BitwiseAnd:
							part1 = "(";
							part2 = " & ";
							part3 = ")";
							break;
						case FX::Nodes::Binary::Op::BitwiseOr:
							part1 = "(";
							part2 = " | ";
							part3 = ")";
							break;
						case FX::Nodes::Binary::Op::BitwiseXor:
							part1 = "(";
							part2 = " ^ ";
							part3 = ")";
							break;
						case FX::Nodes::Binary::Op::LogicalAnd:
							part1 = '(';
							part2 = " && ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::LogicalOr:
							part1 = '(';
							part2 = " || ";
							part3 = ')';
							break;
						case FX::Nodes::Binary::Op::ElementExtract:
							part2 = '[';
							part3 = ']';
							break;
					}

					this->mCurrentSource += part1;
					Visit(node->Operands[0]);
					this->mCurrentSource += part2;
					Visit(node->Operands[1]);
					this->mCurrentSource += part3;
				}
				void Visit(const FX::Nodes::Intrinsic *node)
				{
					std::string part1, part2, part3, part4, part5;

					switch (node->Operator)
					{
						case FX::Nodes::Intrinsic::Op::Abs:
							part1 = "abs(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Acos:
							part1 = "acos(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::All:
							part1 = "all(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Any:
							part1 = "any(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::BitCastInt2Float:
							part1 = "asfloat(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::BitCastUint2Float:
							part1 = "asfloat(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Asin:
							part1 = "asin(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::BitCastFloat2Int:
							part1 = "asint(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::BitCastFloat2Uint:
							part1 = "asuint(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Atan:
							part1 = "atan(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Atan2:
							part1 = "atan2(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Ceil:
							part1 = "ceil(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Clamp:
							part1 = "clamp(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Cos:
							part1 = "cos(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Cosh:
							part1 = "cosh(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Cross:
							part1 = "cross(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::PartialDerivativeX:
							part1 = "ddx(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::PartialDerivativeY:
							part1 = "ddy(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Degrees:
							part1 = "degrees(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Determinant:
							part1 = "determinant(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Distance:
							part1 = "distance(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Dot:
							part1 = "dot(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Exp:
							part1 = "exp(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Exp2:
							part1 = "exp2(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::FaceForward:
							part1 = "faceforward(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Floor:
							part1 = "floor(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Frac:
							part1 = "frac(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Frexp:
							part1 = "frexp(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Fwidth:
							part1 = "fwidth(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Ldexp:
							part1 = "ldexp(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Length:
							part1 = "length(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Lerp:
							part1 = "lerp(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Log:
							part1 = "log(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Log10:
							part1 = "log10(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Log2:
							part1 = "log2(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Mad:
							part1 = "((";
							part2 = ") * (";
							part3 = ") + (";
							part4 = "))";
							break;
						case FX::Nodes::Intrinsic::Op::Max:
							part1 = "max(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Min:
							part1 = "min(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Modf:
							part1 = "modf(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Mul:
							part1 = "mul(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Normalize:
							part1 = "normalize(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Pow:
							part1 = "pow(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Radians:
							part1 = "radians(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Rcp:
							part1 = "(1.0f / ";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Reflect:
							part1 = "reflect(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Refract:
							part1 = "refract(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Round:
							part1 = "round(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Rsqrt:
							part1 = "rsqrt(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Saturate:
							part1 = "saturate(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Sign:
							part1 = "sign(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Sin:
							part1 = "sin(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::SinCos:
							part1 = "sincos(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Sinh:
							part1 = "sinh(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::SmoothStep:
							part1 = "smoothstep(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Sqrt:
							part1 = "sqrt(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Step:
							part1 = "step(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tan:
							part1 = "tan(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tanh:
							part1 = "tanh(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2D:
							part1 = "__tex2D(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DFetch:
							part1 = "__tex2Dfetch(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DGather:
							if (node->Arguments[2]->NodeId == FX::Node::Id::Literal && node->Arguments[2]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[2])->Value.Int[0];

								this->mCurrentSource += "__tex2Dgather" + std::to_string(component) + "(";
								Visit(node->Arguments[0]);
								this->mCurrentSource += ", ";
								Visit(node->Arguments[1]);
								this->mCurrentSource += ")";
							}
							else
							{
								this->mErrors += PrintLocation(node->Location) + "error: texture gather component argument has to be constant.\n";
							}
							return;
						case FX::Nodes::Intrinsic::Op::Tex2DGatherOffset:
							if (node->Arguments[3]->NodeId == FX::Node::Id::Literal && node->Arguments[3]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[3])->Value.Int[0];

								this->mCurrentSource += "__tex2Dgatheroffset" + std::to_string(component) + "(";
								Visit(node->Arguments[0]);
								this->mCurrentSource += ", ";
								Visit(node->Arguments[1]);
								this->mCurrentSource += ", ";
								Visit(node->Arguments[2]);
								this->mCurrentSource += ")";
							}
							else
							{
								this->mErrors += PrintLocation(node->Location) + "error: texture gather component argument has to be constant.\n";
							}
							return;
						case FX::Nodes::Intrinsic::Op::Tex2DGrad:
							part1 = "__tex2Dgrad(";
							part2 = ", ";
							part3 = ", ";
							part4 = ", ";
							part5 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DLevel:
							part1 = "__tex2Dlod(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DLevelOffset:
							part1 = "__tex2Dlodoffset(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DOffset:
							part1 = "__tex2Doffset(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DProj:
							part1 = "__tex2Dproj(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DSize:
							part1 = "__tex2Dsize(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Transpose:
							part1 = "transpose(";
							part2 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Trunc:
							part1 = "trunc(";
							part2 = ")";
							break;
					}

					this->mCurrentSource += part1;

					if (node->Arguments[0] != nullptr)
					{
						Visit(node->Arguments[0]);
					}

					this->mCurrentSource += part2;

					if (node->Arguments[1] != nullptr)
					{
						Visit(node->Arguments[1]);
					}

					this->mCurrentSource += part3;

					if (node->Arguments[2] != nullptr)
					{
						Visit(node->Arguments[2]);
					}

					this->mCurrentSource += part4;

					if (node->Arguments[3] != nullptr)
					{
						Visit(node->Arguments[3]);
					}

					this->mCurrentSource += part5;
				}
				void Visit(const FX::Nodes::Conditional *node)
				{
					this->mCurrentSource += '(';
					Visit(node->Condition);
					this->mCurrentSource += " ? ";
					Visit(node->ExpressionOnTrue);
					this->mCurrentSource += " : ";
					Visit(node->ExpressionOnFalse);
					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::Swizzle *node)
				{
					Visit(node->Operand);

					this->mCurrentSource += '.';

					if (node->Operand->Type.IsMatrix())
					{
						const char swizzle[16][5] =
						{
							"_m00", "_m01", "_m02", "_m03",
							"_m10", "_m11", "_m12", "_m13",
							"_m20", "_m21", "_m22", "_m23",
							"_m30", "_m31", "_m32", "_m33"
						};

						for (int i = 0; i < 4 && node->Mask[i] >= 0; ++i)
						{
							this->mCurrentSource += swizzle[node->Mask[i]];
						}
					}
					else
					{
						const char swizzle[4] =
						{
							'x', 'y', 'z', 'w'
						};

						for (int i = 0; i < 4 && node->Mask[i] >= 0; ++i)
						{
							this->mCurrentSource += swizzle[node->Mask[i]];
						}
					}
				}
				void Visit(const FX::Nodes::FieldSelection *node)
				{
					this->mCurrentSource += '(';

					Visit(node->Operand);

					this->mCurrentSource += '.';
					this->mCurrentSource += PrintName(node->Field);
					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::Assignment *node)
				{
					this->mCurrentSource += '(';
					Visit(node->Left);
					this->mCurrentSource += ' ';

					switch (node->Operator)
					{
						case FX::Nodes::Assignment::Op::None:
							this->mCurrentSource += '=';
							break;
						case FX::Nodes::Assignment::Op::Add:
							this->mCurrentSource += "+=";
							break;
						case FX::Nodes::Assignment::Op::Subtract:
							this->mCurrentSource += "-=";
							break;
						case FX::Nodes::Assignment::Op::Multiply:
							this->mCurrentSource += "*=";
							break;
						case FX::Nodes::Assignment::Op::Divide:
							this->mCurrentSource += "/=";
							break;
						case FX::Nodes::Assignment::Op::Modulo:
							this->mCurrentSource += "%=";
							break;
						case FX::Nodes::Assignment::Op::LeftShift:
							this->mCurrentSource += "<<=";
							break;
						case FX::Nodes::Assignment::Op::RightShift:
							this->mCurrentSource += ">>=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseAnd:
							this->mCurrentSource += "&=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseOr:
							this->mCurrentSource += "|=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseXor:
							this->mCurrentSource += "^=";
							break;
					}

					this->mCurrentSource += ' ';
					Visit(node->Right);
					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::Call *node)
				{
					this->mCurrentSource += PrintName(node->Callee);
					this->mCurrentSource += '(';

					for (auto argument : node->Arguments)
					{
						Visit(argument);

						this->mCurrentSource += ", ";
					}

					if (!node->Arguments.empty())
					{
						this->mCurrentSource.pop_back();
						this->mCurrentSource.pop_back();
					}

					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::Constructor *node)
				{
					this->mCurrentSource += PrintType(node->Type);
					this->mCurrentSource += '(';

					for (auto argument : node->Arguments)
					{
						Visit(argument);

						this->mCurrentSource += ", ";
					}

					if (!node->Arguments.empty())
					{
						this->mCurrentSource.pop_back();
						this->mCurrentSource.pop_back();
					}

					this->mCurrentSource += ')';
				}
				void Visit(const FX::Nodes::InitializerList *node)
				{
					this->mCurrentSource += "{ ";

					for (auto expression : node->Values)
					{
						Visit(expression);

						this->mCurrentSource += ", ";
					}

					this->mCurrentSource += " }";
				}

				template <typename T>
				void Visit(const std::vector<FX::Nodes::Annotation> &annotations, T &object)
				{
					for (auto &annotation : annotations)
					{
						switch (annotation.Value->Type.BaseClass)
						{
							case FX::Nodes::Type::Class::Bool:
							case FX::Nodes::Type::Class::Int:
								object.Annotations[annotation.Name] = annotation.Value->Value.Int;
								break;
							case FX::Nodes::Type::Class::Uint:
								object.Annotations[annotation.Name] = annotation.Value->Value.Uint;
								break;
							case FX::Nodes::Type::Class::Float:
								object.Annotations[annotation.Name] = annotation.Value->Value.Float;
								break;
							case FX::Nodes::Type::Class::String:
								object.Annotations[annotation.Name] = annotation.Value->StringValue;
								break;
						}
					}
				}
				void Visit(const FX::Nodes::Struct *node)
				{
					this->mCurrentSource += "struct ";
					this->mCurrentSource += PrintName(node);
					this->mCurrentSource += "\n{\n";

					if (!node->Fields.empty())
					{
						for (auto field : node->Fields)
						{
							Visit(field);
						}
					}
					else
					{
						this->mCurrentSource += "float _dummy;\n";
					}

					this->mCurrentSource += "};\n";
				}
				void Visit(const FX::Nodes::Variable *node)
				{
					if (!(this->mCurrentInParameterBlock || this->mCurrentInFunctionBlock))
					{
						if (node->Type.IsTexture())
						{
							VisitTexture(node);
							return;
						}
						else if (node->Type.IsSampler())
						{
							VisitSampler(node);
							return;
						}
						else if (node->Type.HasQualifier(FX::Nodes::Type::Qualifier::Uniform))
						{
							VisitUniform(node);
							return;
						}
					}

					if (this->mCurrentInForInitialization <= 1)
					{
						this->mCurrentSource += PrintTypeWithQualifiers(node->Type);
					}

					if (!node->Name.empty())
					{
						this->mCurrentSource += ' ' + PrintName(node);
					}

					if (node->Type.IsArray())
					{
						this->mCurrentSource += '[';
						this->mCurrentSource += (node->Type.ArrayLength >= 1) ? std::to_string(node->Type.ArrayLength) : "";
						this->mCurrentSource += ']';
					}

					if (!node->Semantic.empty())
					{
						this->mCurrentSource += " : " + ConvertSemantic(node->Semantic);
					}

					if (node->Initializer != nullptr)
					{
						this->mCurrentSource += " = ";

						Visit(node->Initializer);
					}

					if (!(this->mCurrentInParameterBlock || this->mCurrentInFunctionBlock))
					{
						this->mCurrentSource += ";\n";
					}
				}
				void VisitTexture(const FX::Nodes::Variable *node)
				{
					D3D10_TEXTURE2D_DESC texdesc;
					ZeroMemory(&texdesc, sizeof(D3D10_TEXTURE2D_DESC));

					D3D10Texture *obj = new D3D10Texture();
					obj->Name = node->Name;
					texdesc.Width = obj->Width = node->Properties.Width;
					texdesc.Height = obj->Height = node->Properties.Height;
					texdesc.MipLevels = obj->Levels = node->Properties.MipLevels;
					texdesc.ArraySize = 1;
					texdesc.Format = LiteralToFormat(node->Properties.Format, obj->Format);
					texdesc.SampleDesc.Count = 1;
					texdesc.SampleDesc.Quality = 0;
					texdesc.Usage = D3D10_USAGE_DEFAULT;
					texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
					texdesc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

					obj->ShaderRegister = this->mRuntime->mEffectShaderResources.size();
					obj->TextureInterface = nullptr;
					obj->ShaderResourceView[0] = nullptr;
					obj->ShaderResourceView[1] = nullptr;

					Visit(node->Annotations, *obj);

					if (node->Semantic == "COLOR" || node->Semantic == "SV_TARGET")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							this->mErrors += PrintLocation(node->Location) + "warning: texture property on backbuffer textures are ignored.\n";
						}

						obj->ChangeDataSource(D3D10Texture::Source::BackBuffer, this->mRuntime, this->mRuntime->mBackBufferTextureSRV[0], this->mRuntime->mBackBufferTextureSRV[1]);
					}
					else if (node->Semantic == "DEPTH" || node->Semantic == "SV_DEPTH")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							this->mErrors += PrintLocation(node->Location) + "warning: texture property on depthbuffer textures are ignored.\n";
						}

						obj->ChangeDataSource(D3D10Texture::Source::DepthStencil, this->mRuntime, this->mRuntime->mDepthStencilTextureSRV, nullptr);
					}
					else
					{
						obj->DataSource = D3D10Texture::Source::Memory;

						if (texdesc.MipLevels == 0)
						{
							this->mErrors += PrintLocation(node->Location) + "warning: a texture cannot have 0 miplevels, changed it to 1.\n";

							texdesc.MipLevels = 1;
						}

						HRESULT hr = this->mRuntime->mDevice->CreateTexture2D(&texdesc, nullptr, &obj->TextureInterface);

						if (FAILED(hr))
						{
							this->mErrors += PrintLocation(node->Location) + "error: 'ID3D10Device::CreateTexture2D' failed with " + std::to_string(hr) + "!\n";
							this->mFatal = true;
							return;
						}

						D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
						ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
						srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
						srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
						srvdesc.Format = MakeNonSRGBFormat(texdesc.Format);

						hr = this->mRuntime->mDevice->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[0]);

						if (FAILED(hr))
						{
							this->mErrors += PrintLocation(node->Location) + "error: 'ID3D10Device::CreateShaderResourceView' failed with " + std::to_string(hr) + "!\n";
							this->mFatal = true;
							return;
						}

						srvdesc.Format = MakeSRGBFormat(texdesc.Format);

						if (srvdesc.Format != texdesc.Format)
						{
							hr = this->mRuntime->mDevice->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[1]);

							if (FAILED(hr))
							{
								this->mErrors += PrintLocation(node->Location) + "error: 'ID3D10Device::CreateShaderResourceView' failed with " + std::to_string(hr) + "!\n";
								this->mFatal = true;
								return;
							}
						}
					}

					this->mCurrentSource += "Texture2D ";
					this->mCurrentSource += PrintName(node);
					this->mCurrentSource += " : register(t" + std::to_string(this->mRuntime->mEffectShaderResources.size()) + "), __";
					this->mCurrentSource += PrintName(node);
					this->mCurrentSource += "SRGB : register(t" + std::to_string(this->mRuntime->mEffectShaderResources.size() + 1) + ");\n";

					this->mRuntime->mEffectShaderResources.push_back(obj->ShaderResourceView[0]);
					this->mRuntime->mEffectShaderResources.push_back(obj->ShaderResourceView[1]);

					this->mRuntime->AddTexture(obj);
				}
				void VisitSampler(const FX::Nodes::Variable *node)
				{
					if (node->Properties.Texture == nullptr)
					{
						this->mErrors += PrintLocation(node->Location) + "error: sampler '" + node->Name + "' is missing required 'Texture' required.\n";
						this->mFatal = true;
						return;
					}

					D3D10_SAMPLER_DESC desc;
					desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->Properties.AddressU);
					desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->Properties.AddressV);
					desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->Properties.AddressW);
					desc.MipLODBias = node->Properties.MipLODBias;
					desc.MinLOD = node->Properties.MinLOD;
					desc.MaxLOD = node->Properties.MaxLOD;
					desc.MaxAnisotropy = node->Properties.MaxAnisotropy;
					desc.ComparisonFunc = D3D10_COMPARISON_NEVER;

					const UINT minfilter = node->Properties.MinFilter;
					const UINT magfilter = node->Properties.MagFilter;
					const UINT mipfilter = node->Properties.MipFilter;

					if (minfilter == FX::Nodes::Variable::Properties::ANISOTROPIC || magfilter == FX::Nodes::Variable::Properties::ANISOTROPIC || mipfilter == FX::Nodes::Variable::Properties::ANISOTROPIC)
					{
						desc.Filter = D3D10_FILTER_ANISOTROPIC;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::POINT && magfilter == FX::Nodes::Variable::Properties::POINT && mipfilter == FX::Nodes::Variable::Properties::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::POINT && magfilter == FX::Nodes::Variable::Properties::LINEAR && mipfilter == FX::Nodes::Variable::Properties::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::POINT && magfilter == FX::Nodes::Variable::Properties::LINEAR && mipfilter == FX::Nodes::Variable::Properties::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::LINEAR && magfilter == FX::Nodes::Variable::Properties::POINT && mipfilter == FX::Nodes::Variable::Properties::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::LINEAR && magfilter == FX::Nodes::Variable::Properties::POINT && mipfilter == FX::Nodes::Variable::Properties::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::LINEAR && magfilter == FX::Nodes::Variable::Properties::LINEAR && mipfilter == FX::Nodes::Variable::Properties::POINT)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
					}
					else if (minfilter == FX::Nodes::Variable::Properties::LINEAR && magfilter == FX::Nodes::Variable::Properties::LINEAR && mipfilter == FX::Nodes::Variable::Properties::LINEAR)
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
					}
					else
					{
						desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
					}

					D3D10Texture *texture = static_cast<D3D10Texture *>(this->mRuntime->GetTexture(node->Properties.Texture->Name));

					if (texture == nullptr)
					{
						this->mErrors += PrintLocation(node->Location) + "error: texture '" + node->Properties.Texture->Name + "' for sampler '" + std::string(node->Name) + "' is missing due to previous error.\n";
						this->mFatal = true;
						return;
					}

					const size_t descHash = D3D10_SAMPLER_DESC_HASH(desc);
					auto it = this->mSamplerDescs.find(descHash);

					if (it == this->mSamplerDescs.end())
					{
						ID3D10SamplerState *sampler = nullptr;

						HRESULT hr = this->mRuntime->mDevice->CreateSamplerState(&desc, &sampler);

						if (FAILED(hr))
						{
							this->mErrors += PrintLocation(node->Location) + "error: 'ID3D10Device::CreateSamplerState' failed with " + std::to_string(hr) + "!\n";
							this->mFatal = true;
							return;
						}

						this->mRuntime->mEffectSamplerStates.push_back(sampler);
						it = this->mSamplerDescs.emplace(descHash, this->mRuntime->mEffectSamplerStates.size() - 1).first;

						this->mCurrentSource += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
					}

					this->mCurrentSource += "static const __sampler2D ";
					this->mCurrentSource += PrintName(node);
					this->mCurrentSource += " = { ";

					if (node->Properties.SRGBTexture && texture->ShaderResourceView[1] != nullptr)
					{
						this->mCurrentSource += "__";
						this->mCurrentSource += PrintName(node->Properties.Texture);
						this->mCurrentSource += "SRGB";
					}
					else
					{
						this->mCurrentSource += PrintName(node->Properties.Texture);
					}

					this->mCurrentSource += ", __SamplerState" + std::to_string(it->second) + " };\n";
				}
				void VisitUniform(const FX::Nodes::Variable *node)
				{
					this->mCurrentGlobalConstants += PrintTypeWithQualifiers(node->Type);
					this->mCurrentGlobalConstants += ' ';
					this->mCurrentGlobalConstants += PrintName(node);

					if (node->Type.IsArray())
					{
						this->mCurrentGlobalConstants += '[';
						this->mCurrentGlobalConstants += (node->Type.ArrayLength >= 1) ? std::to_string(node->Type.ArrayLength) : "";
						this->mCurrentGlobalConstants += ']';
					}

					this->mCurrentGlobalConstants += ";\n";

					Uniform *obj = new Uniform();
					obj->Name = node->Name;
					obj->Rows = node->Type.Rows;
					obj->Columns = node->Type.Cols;
					obj->Elements = node->Type.ArrayLength;
					obj->StorageSize = node->Type.Rows * node->Type.Cols;

					switch (node->Type.BaseClass)
					{
						case FX::Nodes::Type::Class::Bool:
							obj->BaseType = Uniform::Type::Bool;
							obj->StorageSize *= sizeof(int);
							break;
						case FX::Nodes::Type::Class::Int:
							obj->BaseType = Uniform::Type::Int;
							obj->StorageSize *= sizeof(int);
							break;
						case FX::Nodes::Type::Class::Uint:
							obj->BaseType = Uniform::Type::Uint;
							obj->StorageSize *= sizeof(unsigned int);
							break;
						case FX::Nodes::Type::Class::Float:
							obj->BaseType = Uniform::Type::Float;
							obj->StorageSize *= sizeof(float);
							break;
					}

					const UINT alignment = 16 - (this->mCurrentGlobalSize % 16);
					this->mCurrentGlobalSize += static_cast<UINT>((obj->StorageSize > alignment && (alignment != 16 || obj->StorageSize <= 16)) ? obj->StorageSize + alignment : obj->StorageSize);
					obj->StorageOffset = this->mCurrentGlobalSize - obj->StorageSize;

					Visit(node->Annotations, *obj);

					if (this->mCurrentGlobalSize >= this->mRuntime->GetConstantStorageSize())
					{
						this->mRuntime->EnlargeConstantStorage();
					}

					if (node->Initializer != nullptr && node->Initializer->NodeId == FX::Node::Id::Literal)
					{
						CopyMemory(this->mRuntime->GetConstantStorage() + obj->StorageOffset, &static_cast<const FX::Nodes::Literal *>(node->Initializer)->Value, obj->StorageSize);
					}
					else
					{
						ZeroMemory(this->mRuntime->GetConstantStorage() + obj->StorageOffset, obj->StorageSize);
					}

					this->mRuntime->AddConstant(obj);
				}
				void Visit(const FX::Nodes::Function *node)
				{
					this->mCurrentSource += PrintType(node->ReturnType);
					this->mCurrentSource += ' ';
					this->mCurrentSource += PrintName(node);
					this->mCurrentSource += '(';

					this->mCurrentInParameterBlock = true;

					for (auto parameter : node->Parameters)
					{
						Visit(parameter);

						this->mCurrentSource += ", ";
					}

					this->mCurrentInParameterBlock = false;

					if (!node->Parameters.empty())
					{
						this->mCurrentSource.pop_back();
						this->mCurrentSource.pop_back();
					}

					this->mCurrentSource += ')';
								
					if (!node->ReturnSemantic.empty())
					{
						this->mCurrentSource += " : " + ConvertSemantic(node->ReturnSemantic);
					}

					this->mCurrentSource += '\n';

					this->mCurrentInFunctionBlock = true;

					Visit(node->Definition);

					this->mCurrentInFunctionBlock = false;
				}
				void Visit(const FX::Nodes::Technique *node)
				{
					D3D10Technique *obj = new D3D10Technique();
					obj->Name = node->Name;
					obj->PassCount = static_cast<unsigned int>(node->Passes.size());

					Visit(node->Annotations, *obj);

					for (auto pass : node->Passes)
					{
						Visit(pass, obj->Passes);
					}

					this->mRuntime->AddTechnique(obj);
				}
				void Visit(const FX::Nodes::Pass *node, std::vector<D3D10Technique::Pass> &passes)
				{
					D3D10Technique::Pass pass;
					pass.VS = nullptr;
					pass.PS = nullptr;
					pass.BS = nullptr;
					pass.DSS = nullptr;
					pass.StencilRef = 0;
					pass.Viewport.TopLeftX = pass.Viewport.TopLeftY = pass.Viewport.Width = pass.Viewport.Height = 0;
					pass.Viewport.MinDepth = 0.0f;
					pass.Viewport.MaxDepth = 1.0f;
					ZeroMemory(pass.RT, sizeof(pass.RT));
					ZeroMemory(pass.RTSRV, sizeof(pass.RTSRV));
					pass.SRV = this->mRuntime->mEffectShaderResources;

					if (node->States.VertexShader != nullptr)
					{
						VisitShader(node->States.VertexShader, "vs", pass);
					}
					if (node->States.PixelShader != nullptr)
					{
						VisitShader(node->States.PixelShader, "ps", pass);
					}

					const int targetIndex = node->States.SRGBWriteEnable ? 1 : 0;
					pass.RT[0] = this->mRuntime->mBackBufferTargets[targetIndex];
					pass.RTSRV[0] = this->mRuntime->mBackBufferTextureSRV[targetIndex];

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->States.RenderTargets[i] == nullptr)
						{
							continue;
						}

						D3D10Texture *texture = static_cast<D3D10Texture *>(this->mRuntime->GetTexture(node->States.RenderTargets[i]->Name));

						if (texture == nullptr)
						{
							this->mFatal = true;
							return;
						}

						D3D10_TEXTURE2D_DESC desc;
						texture->TextureInterface->GetDesc(&desc);

						if (pass.Viewport.Width != 0 && pass.Viewport.Height != 0 && (desc.Width != static_cast<unsigned int>(pass.Viewport.Width) || desc.Height != static_cast<unsigned int>(pass.Viewport.Height)))
						{
							this->mErrors += PrintLocation(node->Location) + "error: cannot use multiple rendertargets with different sized textures.\n";
							this->mFatal = true;
							return;
						}
						else
						{
							pass.Viewport.Width = desc.Width;
							pass.Viewport.Height = desc.Height;
						}

						D3D10_RENDER_TARGET_VIEW_DESC rtvdesc;
						ZeroMemory(&rtvdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
						rtvdesc.Format = node->States.SRGBWriteEnable ? MakeSRGBFormat(desc.Format) : MakeNonSRGBFormat(desc.Format);
						rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

						if (texture->RenderTargetView[targetIndex] == nullptr)
						{
							HRESULT hr = this->mRuntime->mDevice->CreateRenderTargetView(texture->TextureInterface, &rtvdesc, &texture->RenderTargetView[targetIndex]);

							if (FAILED(hr))
							{
								this->mErrors += PrintLocation(node->Location) + "warning: 'CreateRenderTargetView' failed!\n";
							}
						}

						pass.RT[i] = texture->RenderTargetView[targetIndex];
						pass.RTSRV[i] = texture->ShaderResourceView[targetIndex];
					}

					if (pass.Viewport.Width == 0 && pass.Viewport.Height == 0)
					{
						pass.Viewport.Width = this->mRuntime->GetBufferWidth();
						pass.Viewport.Height = this->mRuntime->GetBufferHeight();
					}

					D3D10_DEPTH_STENCIL_DESC ddesc;
					ddesc.DepthEnable = node->States.DepthEnable;
					ddesc.DepthWriteMask = node->States.DepthWriteMask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
					ddesc.DepthFunc = static_cast<D3D10_COMPARISON_FUNC>(node->States.DepthFunc);
					ddesc.StencilEnable = node->States.StencilEnable;
					ddesc.StencilReadMask = node->States.StencilReadMask;
					ddesc.StencilWriteMask = node->States.StencilWriteMask;
					ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D10_COMPARISON_FUNC>(node->States.StencilFunc);
					ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = LiteralToStencilOp(node->States.StencilOpPass);
					ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = LiteralToStencilOp(node->States.StencilOpFail);
					ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = LiteralToStencilOp(node->States.StencilOpDepthFail);
					pass.StencilRef = node->States.StencilRef;

					HRESULT hr = this->mRuntime->mDevice->CreateDepthStencilState(&ddesc, &pass.DSS);

					if (FAILED(hr))
					{
						this->mErrors += PrintLocation(node->Location) + "warning: 'ID3D10Device::CreateDepthStencilState' failed!\n";
					}

					D3D10_BLEND_DESC bdesc;
					bdesc.AlphaToCoverageEnable = FALSE;
					bdesc.RenderTargetWriteMask[0] = node->States.RenderTargetWriteMask;
					bdesc.BlendEnable[0] = node->States.BlendEnable;
					bdesc.BlendOp = static_cast<D3D10_BLEND_OP>(node->States.BlendOp);
					bdesc.BlendOpAlpha = static_cast<D3D10_BLEND_OP>(node->States.BlendOpAlpha);
					bdesc.SrcBlend = LiteralToBlend(node->States.SrcBlend);
					bdesc.DestBlend = LiteralToBlend(node->States.DestBlend);

					for (UINT i = 1; i < 8; ++i)
					{
						bdesc.RenderTargetWriteMask[i] = bdesc.RenderTargetWriteMask[0];
						bdesc.BlendEnable[i] = bdesc.BlendEnable[0];
					}

					hr = this->mRuntime->mDevice->CreateBlendState(&bdesc, &pass.BS);

					if (FAILED(hr))
					{
						this->mErrors += PrintLocation(node->Location) + "warning: 'ID3D10Device::CreateBlendState' failed!\n";
					}

					for (ID3D10ShaderResourceView *&srv : pass.SRV)
					{
						if (srv == nullptr)
						{
							continue;
						}

						ID3D10Resource *res1, *res2;
						srv->GetResource(&res1);
						res1->Release();

						for (ID3D10RenderTargetView *rtv : pass.RT)
						{
							if (rtv == nullptr)
							{
								continue;
							}

							rtv->GetResource(&res2);
							res2->Release();

							if (res1 == res2)
							{
								srv = nullptr;
								break;
							}
						}
					}

					passes.push_back(std::move(pass));
				}
				void VisitShader(const FX::Nodes::Function *node, const std::string &shadertype, D3D10Technique::Pass &pass)
				{
					ID3D10Device1 *device1 = nullptr;
					D3D10_FEATURE_LEVEL1 featurelevel = D3D10_FEATURE_LEVEL_10_0;
					
					if (SUCCEEDED(this->mRuntime->mDevice->QueryInterface(&device1)))
					{
						featurelevel = device1->GetFeatureLevel();

						device1->Release();
					}

					std::string profile = shadertype;

					switch (featurelevel)
					{
						case D3D10_FEATURE_LEVEL_10_1:
							profile += "_4_1";
							break;
						default:
						case D3D10_FEATURE_LEVEL_10_0:
							profile += "_4_0";
							break;
						case D3D10_FEATURE_LEVEL_9_1:
						case D3D10_FEATURE_LEVEL_9_2:
							profile += "_4_0_level_9_1";
							break;
						case D3D10_FEATURE_LEVEL_9_3:
							profile += "_4_0_level_9_3";
							break;
					}

					std::string source =
						"struct __sampler2D { Texture2D t; SamplerState s; };\n"
						"inline float4 __tex2D(__sampler2D s, float2 c) { return s.t.Sample(s.s, c); }\n"
						"inline float4 __tex2Dfetch(__sampler2D s, int4 c) { return s.t.Load(c.xyw); }\n"
						"inline float4 __tex2Dgrad(__sampler2D s, float2 c, float2 ddx, float2 ddy) { return s.t.SampleGrad(s.s, c, ddx, ddy); }\n"
						"inline float4 __tex2Dlod(__sampler2D s, float4 c) { return s.t.SampleLevel(s.s, c.xy, c.w); }\n"
						"inline float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return s.t.SampleLevel(s.s, c.xy, c.w, offset); }\n"
						"inline float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return s.t.Sample(s.s, c, offset); }\n"
						"inline float4 __tex2Dproj(__sampler2D s, float4 c) { return s.t.Sample(s.s, c.xy / c.w); }\n"
						"inline int2 __tex2Dsize(__sampler2D s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n";

					if (featurelevel >= D3D10_FEATURE_LEVEL_10_1)
					{
						source +=
							"inline float4 __tex2Dgather0(__sampler2D s, float2 c) { return s.t.Gather(s.s, c); }\n"
							"inline float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return s.t.Gather(s.s, c, offset); }\n";
					}
					else
					{
						source +=
							"inline float4 __tex2Dgather0(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).r, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).r, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).r, s.t.SampleLevel(s.s, c, 0).r); }\n"
							"inline float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).r, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).r, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).r, s.t.SampleLevel(s.s, c, 0, offset).r); }\n";
					}

					source +=
						"inline float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0).g); }\n"
						"inline float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0, offset).g); }\n"
						"inline float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0).b); }\n"
						"inline float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0, offset).b); }\n"
						"inline float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0).a); }\n"
						"inline float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0, offset).a); }\n";

					if (!this->mCurrentGlobalConstants.empty())
					{
						source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + this->mCurrentGlobalConstants + "};\n";
					}

					source += this->mCurrentSource;

					LOG(TRACE) << "> Compiling shader '" << node->Name << "':\n\n" << source.c_str() << "\n";

					UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (this->mSkipShaderOptimization)
					{
						flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
					}

					HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, PrintName(node).c_str(), profile.c_str(), flags, 0, &compiled, &errors);

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

					if (shadertype == "vs")
					{
						hr = this->mRuntime->mDevice->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.VS);
					}
					else if (shadertype == "ps")
					{
						hr = this->mRuntime->mDevice->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.PS);
					}

					compiled->Release();

					if (FAILED(hr))
					{
						this->mErrors += PrintLocation(node->Location) + "error: 'CreateShader' failed!\n";
						this->mFatal = true;
						return;
					}
				}

			private:
				const FX::NodeTree &mAST;
				D3D10Runtime *mRuntime;
				std::string mCurrentSource;
				std::string mErrors;
				bool mFatal, mSkipShaderOptimization;
				std::unordered_map<size_t, size_t> mSamplerDescs;
				std::string mCurrentGlobalConstants;
				UINT mCurrentGlobalSize, mCurrentInForInitialization;
				bool mCurrentInParameterBlock, mCurrentInFunctionBlock;
			};
		}

		class D3D10StateBlock
		{
		public:
			D3D10StateBlock(ID3D10Device *device)
			{
				ZeroMemory(this, sizeof(D3D10StateBlock));

				this->mDevice = device;
				this->mDevice->AddRef();
			}
			~D3D10StateBlock()
			{
				ReleaseAllDeviceObjects();

				this->mDevice->Release();
			}

			void Capture()
			{
				ReleaseAllDeviceObjects();

				this->mDevice->IAGetPrimitiveTopology(&this->mIAPrimitiveTopology);
				this->mDevice->IAGetInputLayout(&this->mIAInputLayout);

				this->mDevice->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, this->mIAVertexBuffers, this->mIAVertexStrides, this->mIAVertexOffsets);
				this->mDevice->IAGetIndexBuffer(&this->mIAIndexBuffer, &this->mIAIndexFormat, &this->mIAIndexOffset);

				this->mDevice->RSGetState(&this->mRSState);
				this->mDevice->RSGetViewports(&this->mRSNumViewports, nullptr);
				this->mDevice->RSGetViewports(&this->mRSNumViewports, this->mRSViewports);

				this->mDevice->VSGetShader(&this->mVS);
				this->mDevice->VSGetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, this->mVSConstantBuffers);
				this->mDevice->VSGetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, this->mVSSamplerStates);
				this->mDevice->VSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, this->mVSShaderResources);

				this->mDevice->GSGetShader(&this->mGS);

				this->mDevice->PSGetShader(&this->mPS);
				this->mDevice->PSGetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, this->mPSConstantBuffers);
				this->mDevice->PSGetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, this->mPSSamplerStates);
				this->mDevice->PSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, this->mPSShaderResources);

				this->mDevice->OMGetBlendState(&this->mOMBlendState, this->mOMBlendFactor, &this->mOMSampleMask);
				this->mDevice->OMGetDepthStencilState(&this->mOMDepthStencilState, &this->mOMStencilRef);
				this->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, this->mOMRenderTargets, &this->mOMDepthStencil);
			}
			void Apply()
			{
				this->mDevice->IASetPrimitiveTopology(this->mIAPrimitiveTopology);
				this->mDevice->IASetInputLayout(this->mIAInputLayout);

				this->mDevice->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, this->mIAVertexBuffers, this->mIAVertexStrides, this->mIAVertexOffsets);
				this->mDevice->IASetIndexBuffer(this->mIAIndexBuffer, this->mIAIndexFormat, this->mIAIndexOffset);

				this->mDevice->RSSetState(this->mRSState);
				this->mDevice->RSSetViewports(this->mRSNumViewports, this->mRSViewports);

				this->mDevice->VSSetShader(this->mVS);
				this->mDevice->VSSetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, this->mVSConstantBuffers);
				this->mDevice->VSSetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, this->mVSSamplerStates);
				this->mDevice->VSSetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, this->mVSShaderResources);

				this->mDevice->GSSetShader(this->mGS);

				this->mDevice->PSSetShader(this->mPS);
				this->mDevice->PSSetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, this->mPSConstantBuffers);
				this->mDevice->PSSetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, this->mPSSamplerStates);
				this->mDevice->PSSetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, this->mPSShaderResources);

				this->mDevice->OMSetBlendState(this->mOMBlendState, this->mOMBlendFactor, this->mOMSampleMask);
				this->mDevice->OMSetDepthStencilState(this->mOMDepthStencilState, this->mOMStencilRef);
				this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, this->mOMRenderTargets, this->mOMDepthStencil);
			}
			void ReleaseAllDeviceObjects()
			{
				SAFE_RELEASE(this->mIAInputLayout);

				for (UINT i = 0; i < D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mIAVertexBuffers[i]);
				}

				SAFE_RELEASE(this->mIAIndexBuffer);

				SAFE_RELEASE(this->mVS);

				for (UINT i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mVSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mVSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mVSShaderResources[i]);
				}

				SAFE_RELEASE(this->mGS);

				SAFE_RELEASE(this->mRSState);

				SAFE_RELEASE(this->mPS);

				for (UINT i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mPSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mPSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(this->mPSShaderResources[i]);
				}

				SAFE_RELEASE(this->mOMBlendState);
				SAFE_RELEASE(this->mOMDepthStencilState);

				for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					SAFE_RELEASE(this->mOMRenderTargets[i]);
				}

				SAFE_RELEASE(this->mOMDepthStencil);
			}

		private:
			ID3D10Device *mDevice;

			ID3D10InputLayout *mIAInputLayout;
			D3D10_PRIMITIVE_TOPOLOGY mIAPrimitiveTopology;
			ID3D10Buffer *mIAVertexBuffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT mIAVertexStrides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT mIAVertexOffsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10Buffer *mIAIndexBuffer;
			DXGI_FORMAT mIAIndexFormat;
			UINT mIAIndexOffset;
			ID3D10VertexShader *mVS;
			ID3D10Buffer *mVSConstantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10SamplerState *mVSSamplerStates[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10ShaderResourceView *mVSShaderResources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10GeometryShader *mGS;
			ID3D10RasterizerState *mRSState;
			UINT mRSNumViewports;
			D3D10_VIEWPORT mRSViewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			ID3D10PixelShader *mPS;
			ID3D10Buffer *mPSConstantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10SamplerState *mPSSamplerStates[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10ShaderResourceView *mPSShaderResources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10BlendState *mOMBlendState;
			FLOAT mOMBlendFactor[4];
			UINT mOMSampleMask;
			ID3D10DepthStencilState *mOMDepthStencilState;
			UINT mOMStencilRef;
			ID3D10RenderTargetView *mOMRenderTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D10DepthStencilView *mOMDepthStencil;
		};

		// ---------------------------------------------------------------------------------------------------

		D3D10Runtime::D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain) : mDevice(device), mSwapChain(swapchain), mBackBufferFormat(DXGI_FORMAT_UNKNOWN), mMultisamplingEnabled(false), mStateBlock(new D3D10StateBlock(device)), mBackBuffer(nullptr), mBackBufferResolved(nullptr), mBackBufferTexture(nullptr), mBackBufferTextureSRV(), mBackBufferTargets(), mDepthStencil(nullptr), mDepthStencilReplacement(nullptr), mDepthStencilTexture(nullptr), mDepthStencilTextureSRV(nullptr), mCopyVS(nullptr), mCopyPS(nullptr), mCopySampler(nullptr), mEffectRasterizerState(nullptr), mConstantBuffer(nullptr), mConstantBufferSize(0)
		{
			assert(this->mDevice != nullptr);
			assert(this->mSwapChain != nullptr);

			this->mDevice->AddRef();
			this->mSwapChain->AddRef();

			IDXGIDevice *dxgidevice = nullptr;
			IDXGIAdapter *adapter = nullptr;

			HRESULT hr = this->mDevice->QueryInterface(&dxgidevice);

			assert(SUCCEEDED(hr));

			hr = dxgidevice->GetAdapter(&adapter);

			dxgidevice->Release();

			assert(SUCCEEDED(hr));

			DXGI_ADAPTER_DESC desc;
			hr = adapter->GetDesc(&desc);

			adapter->Release();

			assert(SUCCEEDED(hr));

			ID3D10Device1 *device1 = nullptr;

			hr = this->mDevice->QueryInterface(&device1);

			if (SUCCEEDED(hr))
			{
				this->mRendererId = device1->GetFeatureLevel();

				device1->Release();
			}
			else
			{
				this->mRendererId = D3D10_FEATURE_LEVEL_10_0;
			}
			
			this->mVendorId = desc.VendorId;
			this->mDeviceId = desc.DeviceId;
		}
		D3D10Runtime::~D3D10Runtime()
		{
			this->mDevice->Release();
			this->mSwapChain->Release();
		}

		bool D3D10Runtime::OnInit(const DXGI_SWAP_CHAIN_DESC &desc)
		{
			this->mWidth = desc.BufferDesc.Width;
			this->mHeight = desc.BufferDesc.Height;
			this->mBackBufferFormat = desc.BufferDesc.Format;
			this->mMultisamplingEnabled = desc.SampleDesc.Count > 1;
			this->mWindow.reset(new WindowWatcher(desc.OutputWindow));

			#pragma region Get backbuffer rendertarget
			HRESULT hr = this->mSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&this->mBackBuffer));

			assert(SUCCEEDED(hr));

			D3D10_TEXTURE2D_DESC texdesc;
			texdesc.Width = this->mWidth;
			texdesc.Height = this->mHeight;
			texdesc.ArraySize = texdesc.MipLevels = 1;
			texdesc.Format = D3D10EffectCompiler::MakeTypelessFormat(this->mBackBufferFormat);
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D10_USAGE_DEFAULT;
			texdesc.BindFlags = D3D10_BIND_RENDER_TARGET;
			texdesc.MiscFlags = texdesc.CPUAccessFlags = 0;

			OSVERSIONINFOEX verinfoWin7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
			const bool isWindows7 = VerifyVersionInfo(&verinfoWin7, VER_MAJORVERSION | VER_MINORVERSION, VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;

			if (this->mMultisamplingEnabled || D3D10EffectCompiler::MakeNonSRGBFormat(this->mBackBufferFormat) != this->mBackBufferFormat || !isWindows7)
			{
				hr = this->mDevice->CreateTexture2D(&texdesc, nullptr, &this->mBackBufferResolved);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer replacement (Width = " << texdesc.Width << ", Height = " << texdesc.Height << ", Format = " << texdesc.Format << ", SampleCount = " << texdesc.SampleDesc.Count << ", SampleQuality = " << texdesc.SampleDesc.Quality << ")! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(this->mBackBuffer);

					return false;
				}

				hr = this->mDevice->CreateRenderTargetView(this->mBackBuffer, nullptr, &this->mBackBufferTargets[2]);

				assert(SUCCEEDED(hr));
			}
			else
			{
				this->mBackBufferResolved = this->mBackBuffer;
				this->mBackBufferResolved->AddRef();
			}
			#pragma endregion

			#pragma region Create backbuffer shader texture
			texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

			hr = this->mDevice->CreateTexture2D(&texdesc, nullptr, &this->mBackBufferTexture);

			if (SUCCEEDED(hr))
			{
				D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
				ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
				srvdesc.Format = D3D10EffectCompiler::MakeNonSRGBFormat(texdesc.Format);
				srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = texdesc.MipLevels;

				if (SUCCEEDED(hr))
				{
					hr = this->mDevice->CreateShaderResourceView(this->mBackBufferTexture, &srvdesc, &this->mBackBufferTextureSRV[0]);
				}
				else
				{
					LOG(TRACE) << "Failed to create backbuffer texture resource view (Format = " << srvdesc.Format << ")! HRESULT is '" << hr << "'.";
				}

				srvdesc.Format = D3D10EffectCompiler::MakeSRGBFormat(texdesc.Format);

				if (SUCCEEDED(hr))
				{
					hr = this->mDevice->CreateShaderResourceView(this->mBackBufferTexture, &srvdesc, &this->mBackBufferTextureSRV[1]);
				}
				else
				{
					LOG(TRACE) << "Failed to create backbuffer SRGB texture resource view (Format = " << srvdesc.Format << ")! HRESULT is '" << hr << "'.";
				}
			}
			else
			{
				LOG(TRACE) << "Failed to create backbuffer texture (Width = " << texdesc.Width << ", Height = " << texdesc.Height << ", Format = " << texdesc.Format << ", SampleCount = " << texdesc.SampleDesc.Count << ", SampleQuality = " << texdesc.SampleDesc.Quality << ")! HRESULT is '" << hr << "'.";
			}

			if (FAILED(hr))
			{
				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);
				SAFE_RELEASE(this->mBackBufferTexture);
				SAFE_RELEASE(this->mBackBufferTextureSRV[0]);
				SAFE_RELEASE(this->mBackBufferTextureSRV[1]);

				return false;
			}
			#pragma endregion

			D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
			ZeroMemory(&rtdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
			rtdesc.Format = D3D10EffectCompiler::MakeNonSRGBFormat(texdesc.Format);
			rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;

			hr = this->mDevice->CreateRenderTargetView(this->mBackBufferResolved, &rtdesc, &this->mBackBufferTargets[0]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);
				SAFE_RELEASE(this->mBackBufferTexture);
				SAFE_RELEASE(this->mBackBufferTextureSRV[0]);
				SAFE_RELEASE(this->mBackBufferTextureSRV[1]);

				LOG(TRACE) << "Failed to create backbuffer rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

				return false;
			}

			rtdesc.Format = D3D10EffectCompiler::MakeSRGBFormat(texdesc.Format);

			hr = this->mDevice->CreateRenderTargetView(this->mBackBufferResolved, &rtdesc, &this->mBackBufferTargets[1]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);
				SAFE_RELEASE(this->mBackBufferTexture);
				SAFE_RELEASE(this->mBackBufferTextureSRV[0]);
				SAFE_RELEASE(this->mBackBufferTextureSRV[1]);
				SAFE_RELEASE(this->mBackBufferTargets[0]);

				LOG(TRACE) << "Failed to create backbuffer SRGB rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

				return false;
			}

			#pragma region Create default depthstencil surface
			D3D10_TEXTURE2D_DESC dstdesc;
			ZeroMemory(&dstdesc, sizeof(D3D10_TEXTURE2D_DESC));
			dstdesc.Width = desc.BufferDesc.Width;
			dstdesc.Height = desc.BufferDesc.Height;
			dstdesc.MipLevels = 1;
			dstdesc.ArraySize = 1;
			dstdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dstdesc.SampleDesc.Count = 1;
			dstdesc.SampleDesc.Quality = 0;
			dstdesc.Usage = D3D10_USAGE_DEFAULT;
			dstdesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;

			ID3D10Texture2D *dstexture = nullptr;
			hr = this->mDevice->CreateTexture2D(&dstdesc, nullptr, &dstexture);

			if (SUCCEEDED(hr))
			{
				hr = this->mDevice->CreateDepthStencilView(dstexture, nullptr, &this->mDefaultDepthStencil);

				SAFE_RELEASE(dstexture);
			}
			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				return false;
			}
			#pragma endregion

			const BYTE vs[] = { 68, 88, 66, 67, 224, 206, 72, 137, 142, 185, 68, 219, 247, 216, 225, 132, 111, 78, 106, 20, 1, 0, 0, 0, 156, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 140, 0, 0, 0, 192, 0, 0, 0, 24, 1, 0, 0, 32, 2, 0, 0, 82, 68, 69, 70, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 0, 0, 0, 0, 4, 254, 255, 0, 1, 0, 0, 28, 0, 0, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 171, 171, 73, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 83, 86, 95, 86, 101, 114, 116, 101, 120, 73, 68, 0, 79, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 12, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 83, 72, 68, 82, 0, 1, 0, 0, 64, 0, 1, 0, 64, 0, 0, 0, 96, 0, 0, 4, 18, 16, 16, 0, 0, 0, 0, 0, 6, 0, 0, 0, 103, 0, 0, 4, 242, 32, 16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 101, 0, 0, 3, 50, 32, 16, 0, 1, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 32, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 6, 16, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 15, 50, 32, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 128, 191, 0, 0, 128, 63, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 5, 50, 32, 16, 0, 1, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 54, 0, 0, 8, 194, 32, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = this->mDevice->CreateVertexShader(vs, ARRAYSIZE(vs), &this->mCopyVS);

			assert(SUCCEEDED(hr));

			const BYTE ps[] = { 68, 88, 66, 67, 93, 102, 148, 45, 34, 106, 51, 79, 54, 23, 136, 21, 27, 217, 232, 71, 1, 0, 0, 0, 116, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 208, 0, 0, 0, 40, 1, 0, 0, 92, 1, 0, 0, 248, 1, 0, 0, 82, 68, 69, 70, 148, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 28, 0, 0, 0, 0, 4, 255, 255, 0, 1, 0, 0, 98, 0, 0, 0, 92, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 95, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 1, 0, 0, 0, 13, 0, 0, 0, 115, 48, 0, 116, 48, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 73, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 3, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 79, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 83, 86, 95, 84, 97, 114, 103, 101, 116, 0, 171, 171, 83, 72, 68, 82, 148, 0, 0, 0, 64, 0, 0, 0, 37, 0, 0, 0, 90, 0, 0, 3, 0, 96, 16, 0, 0, 0, 0, 0, 88, 24, 0, 4, 0, 112, 16, 0, 0, 0, 0, 0, 85, 85, 0, 0, 98, 16, 0, 3, 50, 16, 16, 0, 1, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 69, 0, 0, 9, 242, 0, 16, 0, 0, 0, 0, 0, 70, 16, 16, 0, 1, 0, 0, 0, 70, 126, 16, 0, 0, 0, 0, 0, 0, 96, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 114, 32, 16, 0, 0, 0, 0, 0, 70, 2, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 130, 32, 16, 0, 0, 0, 0, 0, 1, 64, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = this->mDevice->CreatePixelShader(ps, ARRAYSIZE(ps), &this->mCopyPS);

			assert(SUCCEEDED(hr));

			const D3D10_SAMPLER_DESC copysampdesc = { D3D10_FILTER_MIN_MAG_MIP_POINT, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP };
			hr = this->mDevice->CreateSamplerState(&copysampdesc, &this->mCopySampler);

			assert(SUCCEEDED(hr));

			#pragma region Create effect objects
			D3D10_RASTERIZER_DESC rsdesc;
			ZeroMemory(&rsdesc, sizeof(D3D10_RASTERIZER_DESC));
			rsdesc.FillMode = D3D10_FILL_SOLID;
			rsdesc.CullMode = D3D10_CULL_NONE;
			rsdesc.DepthClipEnable = TRUE;

			hr = this->mDevice->CreateRasterizerState(&rsdesc, &this->mEffectRasterizerState);

			assert(SUCCEEDED(hr));
			#pragma endregion

			this->mGUI.reset(new GUI(this, nvgCreateD3D10(this->mDevice, 0)));

			return Runtime::OnInit();
		}
		void D3D10Runtime::OnReset()
		{
			if (!this->mIsInitialized)
			{
				return;
			}

			Runtime::OnReset();

			// Destroy NanoVG
			NVGcontext *const nvg = this->mGUI->GetContext();

			this->mGUI.reset();

			nvgDeleteD3D10(nvg);

			// Destroy resources
			SAFE_RELEASE(this->mBackBuffer);
			SAFE_RELEASE(this->mBackBufferResolved);
			SAFE_RELEASE(this->mBackBufferTexture);
			SAFE_RELEASE(this->mBackBufferTextureSRV[0]);
			SAFE_RELEASE(this->mBackBufferTextureSRV[1]);
			SAFE_RELEASE(this->mBackBufferTargets[0]);
			SAFE_RELEASE(this->mBackBufferTargets[1]);
			SAFE_RELEASE(this->mBackBufferTargets[2]);

			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mDepthStencilReplacement);
			SAFE_RELEASE(this->mDepthStencilTexture);
			SAFE_RELEASE(this->mDepthStencilTextureSRV);

			SAFE_RELEASE(this->mDefaultDepthStencil);
			SAFE_RELEASE(this->mCopyVS);
			SAFE_RELEASE(this->mCopyPS);
			SAFE_RELEASE(this->mCopySampler);

			SAFE_RELEASE(this->mEffectRasterizerState);
		}
		void D3D10Runtime::OnResetEffect()
		{
			Runtime::OnResetEffect();

			for (ID3D10SamplerState *it : this->mEffectSamplerStates)
			{
				it->Release();
			}

			this->mEffectSamplerStates.clear();
			this->mEffectShaderResources.clear();

			SAFE_RELEASE(this->mConstantBuffer);

			this->mConstantBufferSize = 0;
		}
		void D3D10Runtime::OnPresent()
		{
			if (!this->mIsInitialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (this->mStats.DrawCalls == 0)
			{
				return;
			}

			DetectDepthSource();

			// Capture device state
			this->mStateBlock->Capture();

			ID3D10RenderTargetView *stateblockTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
			ID3D10DepthStencilView *stateblockDepthStencil = nullptr;

			this->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblockTargets, &stateblockDepthStencil);

			// Resolve backbuffer
			if (this->mBackBufferResolved != this->mBackBuffer)
			{
				this->mDevice->ResolveSubresource(this->mBackBufferResolved, 0, this->mBackBuffer, 0, this->mBackBufferFormat);
			}

			// Apply post processing
			OnApplyEffect();

			// Reset rendertarget
			this->mDevice->OMSetRenderTargets(1, &this->mBackBufferTargets[0], this->mDefaultDepthStencil);

			const D3D10_VIEWPORT viewport = { 0, 0, this->mWidth, this->mHeight, 0.0f, 1.0f };
			this->mDevice->RSSetViewports(1, &viewport);

			// Apply presenting
			Runtime::OnPresent();

			// Copy to backbuffer
			if (this->mBackBufferResolved != this->mBackBuffer)
			{
				this->mDevice->OMSetRenderTargets(1, &this->mBackBufferTargets[2], nullptr);
				this->mDevice->CopyResource(this->mBackBufferTexture, this->mBackBufferResolved);

				this->mDevice->VSSetShader(this->mCopyVS);
				this->mDevice->PSSetShader(this->mCopyPS);
				this->mDevice->PSSetSamplers(0, 1, &this->mCopySampler);
				this->mDevice->PSSetShaderResources(0, 1, &this->mBackBufferTextureSRV[D3D10EffectCompiler::MakeSRGBFormat(this->mBackBufferFormat) == this->mBackBufferFormat]);
				this->mDevice->Draw(3, 0);
			}

			// Apply previous device state
			this->mStateBlock->Apply();

			this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblockTargets, stateblockDepthStencil);

			this->mStateBlock->ReleaseAllDeviceObjects();

			for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			{
				SAFE_RELEASE(stateblockTargets[i]);
			}

			SAFE_RELEASE(stateblockDepthStencil);
		}
		void D3D10Runtime::OnDrawCall(UINT vertices)
		{
			Runtime::OnDrawCall(vertices);

			ID3D10DepthStencilView *depthstencil = nullptr;
			this->mDevice->OMGetRenderTargets(0, nullptr, &depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == this->mDefaultDepthStencil)
				{
					return;
				}
				else if (depthstencil == this->mDepthStencilReplacement)
				{
					depthstencil = this->mDepthStencil;
				}

				const auto it = this->mDepthSourceTable.find(depthstencil);

				if (it != this->mDepthSourceTable.end())
				{
					it->second.DrawCallCount = static_cast<FLOAT>(this->mStats.DrawCalls);
					it->second.DrawVerticesCount += vertices;
				}
			}
		}
		void D3D10Runtime::OnApplyEffect()
		{
			if (!this->mIsEffectCompiled)
			{
				return;
			}

			// Setup real backbuffer
			this->mDevice->OMSetRenderTargets(1, &this->mBackBufferTargets[0], nullptr);

			// Setup vertex input
			const uintptr_t null = 0;
			this->mDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			this->mDevice->IASetInputLayout(nullptr);
			this->mDevice->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

			this->mDevice->RSSetState(this->mEffectRasterizerState);

			// Disable unused pipeline stages
			this->mDevice->GSSetShader(nullptr);

			// Setup samplers
			this->mDevice->VSSetSamplers(0, static_cast<UINT>(this->mEffectSamplerStates.size()), this->mEffectSamplerStates.data());
			this->mDevice->PSSetSamplers(0, static_cast<UINT>(this->mEffectSamplerStates.size()), this->mEffectSamplerStates.data());

			// Setup shader constants
			this->mDevice->VSSetConstantBuffers(0, 1, &this->mConstantBuffer);
			this->mDevice->PSSetConstantBuffers(0, 1, &this->mConstantBuffer);

			// Apply post processing
			Runtime::OnApplyEffect();
		}
		void D3D10Runtime::OnApplyEffectTechnique(const Technique *technique)
		{
			Runtime::OnApplyEffectTechnique(technique);

			bool defaultDepthStencilCleared = false;

			// Update shader constants
			if (this->mConstantBuffer != nullptr)
			{
				void *data = nullptr;

				const HRESULT hr = this->mConstantBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &data);

				if (SUCCEEDED(hr))
				{
					CopyMemory(data, this->mUniformDataStorage.data(), this->mConstantBufferSize);

					this->mConstantBuffer->Unmap();
				}
				else
				{
					LOG(TRACE) << "Failed to map constant buffer! HRESULT is '" << hr << "'!";
				}
			}

			for (const auto &pass : static_cast<const D3D10Technique *>(technique)->Passes)
			{
				// Setup states
				this->mDevice->VSSetShader(pass.VS);
				this->mDevice->PSSetShader(pass.PS);

				const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				this->mDevice->OMSetBlendState(pass.BS, blendfactor, D3D10_DEFAULT_SAMPLE_MASK);
				this->mDevice->OMSetDepthStencilState(pass.DSS, pass.StencilRef);

				// Save backbuffer of previous pass
				this->mDevice->CopyResource(this->mBackBufferTexture, this->mBackBufferResolved);

				// Setup shader resources
				this->mDevice->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());
				this->mDevice->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());

				// Setup rendertargets
				if (pass.Viewport.Width == this->mWidth && pass.Viewport.Height == this->mHeight)
				{
					this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, this->mDefaultDepthStencil);

					if (!defaultDepthStencilCleared)
					{
						defaultDepthStencilCleared = true;

						// Clear depthstencil
						this->mDevice->ClearDepthStencilView(this->mDefaultDepthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0);
					}
				}
				else
				{
					this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, nullptr);
				}

				this->mDevice->RSSetViewports(1, &pass.Viewport);

				for (ID3D10RenderTargetView *const target : pass.RT)
				{
					if (target == nullptr)
					{
						continue;
					}

					const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					this->mDevice->ClearRenderTargetView(target, color);
				}

				// Draw triangle
				this->mDevice->Draw(3, 0);

				Runtime::OnDrawCall(3);

				// Reset rendertargets
				this->mDevice->OMSetRenderTargets(0, nullptr, nullptr);

				// Reset shader resources
				ID3D10ShaderResourceView *null[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
				this->mDevice->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);
				this->mDevice->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);

				// Update shader resources
				for (ID3D10ShaderResourceView *const resource : pass.RTSRV)
				{
					if (resource == nullptr)
					{
						continue;
					}

					D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
					resource->GetDesc(&srvdesc);

					if (srvdesc.Texture2D.MipLevels > 1)
					{
						this->mDevice->GenerateMips(resource);
					}
				}
			}
		}

		void D3D10Runtime::OnCreateDepthStencilView(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil)
		{
			assert(resource != nullptr);
			assert(depthstencil != nullptr);

			// Do not track default depthstencil
			if (!this->mIsInitialized)
			{
				return;
			}

			ID3D10Texture2D *texture = nullptr;
			const HRESULT hr = resource->QueryInterface(__uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&texture));

			if (FAILED(hr))
			{
				return;
			}

			D3D10_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			SAFE_RELEASE(texture);

			// Early depthstencil rejection
			if (desc.Width != this->mWidth || desc.Height != this->mHeight || desc.SampleDesc.Count > 1)
			{
				return;
			}

			LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

			// Begin tracking new depthstencil
			const DepthSourceInfo info = { desc.Width, desc.Height };
			this->mDepthSourceTable.emplace(depthstencil, info);
		}
		void D3D10Runtime::OnDeleteDepthStencilView(ID3D10DepthStencilView *depthstencil)
		{
			assert(depthstencil != nullptr);

			const auto it = this->mDepthSourceTable.find(depthstencil);
		
			if (it != this->mDepthSourceTable.end())
			{
				LOG(TRACE) << "Removing depthstencil " << depthstencil << " from list of possible depth candidates ...";

				this->mDepthSourceTable.erase(it);

				if (depthstencil == this->mDepthStencil)
				{
					CreateDepthStencilReplacement(nullptr);
				}
			}
		}
		void D3D10Runtime::OnSetDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (this->mDepthStencilReplacement != nullptr && depthstencil == this->mDepthStencil)
			{
				depthstencil = this->mDepthStencilReplacement;
			}
		}
		void D3D10Runtime::OnGetDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (this->mDepthStencilReplacement != nullptr && depthstencil == this->mDepthStencilReplacement)
			{
				depthstencil->Release();

				depthstencil = this->mDepthStencil;
				depthstencil->AddRef();
			}
		}
		void D3D10Runtime::OnClearDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (this->mDepthStencilReplacement != nullptr && depthstencil == this->mDepthStencil)
			{
				depthstencil = this->mDepthStencilReplacement;
			}
		}
		void D3D10Runtime::OnCopyResource(ID3D10Resource *&dest, ID3D10Resource *&source)
		{
			if (this->mDepthStencilReplacement != nullptr)
			{
				ID3D10Resource *resource = nullptr;
				this->mDepthStencil->GetResource(&resource);

				if (dest == resource)
				{
					dest = this->mDepthStencilTexture;
				}
				if (source == resource)
				{
					source = this->mDepthStencilTexture;
				}

				resource->Release();
			}
		}

		void D3D10Runtime::Screenshot(unsigned char *buffer) const
		{
			if (this->mBackBufferFormat != DXGI_FORMAT_R8G8B8A8_UNORM && this->mBackBufferFormat != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && this->mBackBufferFormat != DXGI_FORMAT_B8G8R8A8_UNORM && this->mBackBufferFormat != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << this->mBackBufferFormat << ".";
				return;
			}

			D3D10_TEXTURE2D_DESC texdesc;
			ZeroMemory(&texdesc, sizeof(D3D10_TEXTURE2D_DESC));
			texdesc.Width = this->mWidth;
			texdesc.Height = this->mHeight;
			texdesc.Format = this->mBackBufferFormat;
			texdesc.MipLevels = 1;
			texdesc.ArraySize = 1;
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D10_USAGE_STAGING;
			texdesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

			ID3D10Texture2D *textureStaging = nullptr;
			HRESULT hr = this->mDevice->CreateTexture2D(&texdesc, nullptr, &textureStaging);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create staging texture for screenshot capture! HRESULT is '" << hr << "'.";
				return;
			}

			this->mDevice->CopyResource(textureStaging, this->mBackBufferResolved);
				
			D3D10_MAPPED_TEXTURE2D mapped;
			hr = textureStaging->Map(0, D3D10_MAP_READ, 0, &mapped);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to map staging texture with screenshot capture! HRESULT is '" << hr << "'.";

				textureStaging->Release();
				return;
			}

			BYTE *pMem = buffer;
			BYTE *pMapped = static_cast<BYTE *>(mapped.pData);

			const UINT pitch = texdesc.Width * 4;

			for (UINT y = 0; y < texdesc.Height; ++y)
			{
				CopyMemory(pMem, pMapped, std::min(pitch, static_cast<UINT>(mapped.RowPitch)));
			
				for (UINT x = 0; x < pitch; x += 4)
				{
					pMem[x + 3] = 0xFF;

					if (this->mBackBufferFormat == DXGI_FORMAT_B8G8R8A8_UNORM || this->mBackBufferFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
					{
						std::swap(pMem[x + 0], pMem[x + 2]);
					}
				}
								
				pMem += pitch;
				pMapped += mapped.RowPitch;
			}

			textureStaging->Unmap(0);

			textureStaging->Release();
		}
		bool D3D10Runtime::UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors)
		{
			bool skipOptimization = false;

			for (const std::string &pragma : pragmas)
			{
				if (!boost::istarts_with(pragma, "reshade "))
				{
					continue;
				}

				const std::string command = pragma.substr(8);

				if (boost::iequals(command, "skipoptimization") || boost::iequals(command, "nooptimization"))
				{
					skipOptimization = true;
				}
			}

			D3D10EffectCompiler visitor(ast, skipOptimization);

			return visitor.Traverse(this, errors);
		}
		bool D3D10Runtime::UpdateTexture(Texture *texture, const unsigned char *data, size_t size)
		{
			D3D10Texture *textureImpl = dynamic_cast<D3D10Texture *>(texture);

			assert(textureImpl != nullptr);
			assert(data != nullptr && size != 0);

			if (textureImpl->DataSource != D3D10Texture::Source::Memory)
			{
				return false;
			}

			assert(texture->Height != 0);

			this->mDevice->UpdateSubresource(textureImpl->TextureInterface, 0, nullptr, data, static_cast<UINT>(size / texture->Height), static_cast<UINT>(size));

			if (texture->Levels > 1)
			{
				this->mDevice->GenerateMips(textureImpl->ShaderResourceView[0]);
			}

			return true;
		}

		void D3D10Runtime::DetectDepthSource()
		{
			static int cooldown = 0, traffic = 0;

			if (cooldown-- > 0)
			{
				traffic += NetworkUpload > 0;
				return;
			}
			else
			{
				cooldown = 30;

				if (traffic > 10)
				{
					traffic = 0;
					CreateDepthStencilReplacement(nullptr);
					return;
				}
				else
				{
					traffic = 0;
				}
			}

			if (this->mMultisamplingEnabled || this->mDepthSourceTable.empty())
			{
				return;
			}

			DepthSourceInfo bestInfo = { 0 };
			ID3D10DepthStencilView *best = nullptr;

			for (auto &it : this->mDepthSourceTable)
			{
				if (it.second.DrawCallCount == 0)
				{
					continue;
				}
				else if ((it.second.DrawVerticesCount * (1.2f - it.second.DrawCallCount / this->mStats.DrawCalls)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / this->mStats.DrawCalls)))
				{
					best = it.first;
					bestInfo = it.second;
				}

				it.second.DrawCallCount = it.second.DrawVerticesCount = 0;
			}

			if (best != nullptr && this->mDepthStencil != best)
			{
				LOG(TRACE) << "Switched depth source to depthstencil " << best << ".";

				CreateDepthStencilReplacement(best);
			}
		}
		bool D3D10Runtime::CreateDepthStencilReplacement(ID3D10DepthStencilView *depthstencil)
		{
			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mDepthStencilReplacement);
			SAFE_RELEASE(this->mDepthStencilTexture);
			SAFE_RELEASE(this->mDepthStencilTextureSRV);

			if (depthstencil != nullptr)
			{
				this->mDepthStencil = depthstencil;
				this->mDepthStencil->AddRef();
				this->mDepthStencil->GetResource(reinterpret_cast<ID3D10Resource **>(&this->mDepthStencilTexture));

				D3D10_TEXTURE2D_DESC texdesc;
				this->mDepthStencilTexture->GetDesc(&texdesc);

				HRESULT hr = S_OK;

				if ((texdesc.BindFlags & D3D10_BIND_SHADER_RESOURCE) == 0)
				{
					SAFE_RELEASE(this->mDepthStencilTexture);

					switch (texdesc.Format)
					{
						case DXGI_FORMAT_R16_TYPELESS:
						case DXGI_FORMAT_D16_UNORM:
							texdesc.Format = DXGI_FORMAT_R16_TYPELESS;
							break;
						case DXGI_FORMAT_R32_TYPELESS:
						case DXGI_FORMAT_D32_FLOAT:
							texdesc.Format = DXGI_FORMAT_R32_TYPELESS;
							break;
						default:
						case DXGI_FORMAT_R24G8_TYPELESS:
						case DXGI_FORMAT_D24_UNORM_S8_UINT:
							texdesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
							break;
						case DXGI_FORMAT_R32G8X24_TYPELESS:
						case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
							texdesc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
							break;
					}

					texdesc.BindFlags = D3D10_BIND_DEPTH_STENCIL | D3D10_BIND_SHADER_RESOURCE;

					hr = this->mDevice->CreateTexture2D(&texdesc, nullptr, &this->mDepthStencilTexture);

					if (SUCCEEDED(hr))
					{
						D3D10_DEPTH_STENCIL_VIEW_DESC dsvdesc;
						ZeroMemory(&dsvdesc, sizeof(D3D10_DEPTH_STENCIL_VIEW_DESC));
						dsvdesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;

						switch (texdesc.Format)
						{
							case DXGI_FORMAT_R16_TYPELESS:
								dsvdesc.Format = DXGI_FORMAT_D16_UNORM;
								break;
							case DXGI_FORMAT_R32_TYPELESS:
								dsvdesc.Format = DXGI_FORMAT_D32_FLOAT;
								break;
							case DXGI_FORMAT_R24G8_TYPELESS:
								dsvdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
								break;
							case DXGI_FORMAT_R32G8X24_TYPELESS:
								dsvdesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
								break;
						}

						hr = this->mDevice->CreateDepthStencilView(this->mDepthStencilTexture, &dsvdesc, &this->mDepthStencilReplacement);
					}
				}
				else
				{
					this->mDepthStencilReplacement = this->mDepthStencil;
					this->mDepthStencilReplacement->AddRef();
				}

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(this->mDepthStencil);
					SAFE_RELEASE(this->mDepthStencilTexture);

					return false;
				}

				D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
				ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
				srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = 1;

				switch (texdesc.Format)
				{
					case DXGI_FORMAT_R16_TYPELESS:
						srvdesc.Format = DXGI_FORMAT_R16_FLOAT;
						break;
					case DXGI_FORMAT_R32_TYPELESS:
						srvdesc.Format = DXGI_FORMAT_R32_FLOAT;
						break;
					case DXGI_FORMAT_R24G8_TYPELESS:
						srvdesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
						break;
					case DXGI_FORMAT_R32G8X24_TYPELESS:
						srvdesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
						break;
				}

				hr = this->mDevice->CreateShaderResourceView(this->mDepthStencilTexture, &srvdesc, &this->mDepthStencilTextureSRV);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement resource view! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(this->mDepthStencil);
					SAFE_RELEASE(this->mDepthStencilReplacement);
					SAFE_RELEASE(this->mDepthStencilTexture);

					return false;
				}

				if (this->mDepthStencil != this->mDepthStencilReplacement)
				{
					// Update auto depthstencil
					ID3D10RenderTargetView *targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
					ID3D10DepthStencilView *depthstencil = nullptr;

					this->mDevice->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depthstencil);

					if (depthstencil != nullptr)
					{
						if (depthstencil == this->mDepthStencil)
						{
							this->mDevice->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, this->mDepthStencilReplacement);
						}

						depthstencil->Release();
					}

					for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					{
						SAFE_RELEASE(targets[i]);
					}
				}
			}

			// Update effect textures
			for (const auto &it : this->mTextures)
			{
				D3D10Texture *texture = static_cast<D3D10Texture *>(it.get());

				if (texture->DataSource == D3D10Texture::Source::DepthStencil)
				{
					texture->ChangeDataSource(D3D10Texture::Source::DepthStencil, this, this->mDepthStencilTextureSRV, nullptr);
				}
			}

			return true;
		}
	}
}
