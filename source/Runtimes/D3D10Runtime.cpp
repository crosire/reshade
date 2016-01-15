#include "Log.hpp"
#include "D3D10Runtime.hpp"
#include "FX\ParserNodes.hpp"
#include "GUI.hpp"
#include "Input.hpp"

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
					for (auto &pass : Passes)
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
					SAFE_RELEASE(RenderTargetView[0]);
					SAFE_RELEASE(RenderTargetView[1]);
					SAFE_RELEASE(ShaderResourceView[0]);
					SAFE_RELEASE(ShaderResourceView[1]);

					SAFE_RELEASE(TextureInterface);
				}

				void ChangeDataSource(Source source, D3D10Runtime *runtime, ID3D10ShaderResourceView *srv, ID3D10ShaderResourceView *srvSRGB)
				{
					DataSource = source;

					if (srvSRGB == nullptr)
					{
						srvSRGB = srv;
					}

					if (srv == ShaderResourceView[0] && srvSRGB == ShaderResourceView[1])
					{
						return;
					}

					SAFE_RELEASE(RenderTargetView[0]);
					SAFE_RELEASE(RenderTargetView[1]);
					SAFE_RELEASE(ShaderResourceView[0]);
					SAFE_RELEASE(ShaderResourceView[1]);

					SAFE_RELEASE(TextureInterface);

					if (srv != nullptr)
					{
						ShaderResourceView[0] = srv;
						ShaderResourceView[0]->AddRef();
						ShaderResourceView[0]->GetResource(reinterpret_cast<ID3D10Resource **>(&TextureInterface));
						ShaderResourceView[1] = srvSRGB;
						ShaderResourceView[1]->AddRef();

						D3D10_TEXTURE2D_DESC texdesc;
						TextureInterface->GetDesc(&texdesc);

						Width = texdesc.Width;
						Height = texdesc.Height;
						Format = Texture::PixelFormat::Unknown;
						Levels = texdesc.MipLevels;
					}
					else
					{
						Width = Height = Levels = 0;
						Format = Texture::PixelFormat::Unknown;
					}

					// Update techniques shader resourceviews
					for (auto &technique : runtime->GetTechniques())
					{
						for (auto &pass : static_cast<D3D10Technique *>(technique.get())->Passes)
						{
							pass.SRV[ShaderRegister] = ShaderResourceView[0];
							pass.SRV[ShaderRegister + 1] = ShaderResourceView[1];
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
				D3D10EffectCompiler(const FX::nodetree &ast, bool skipoptimization = false) : _ast(ast), _runtime(nullptr), _isFatal(false), _skipShaderOptimization(skipoptimization), _currentInParameterBlock(false), _currentInFunctionBlock(false), _currentInForInitialization(0), _currentGlobalSize(0)
				{
				}

				bool Traverse(D3D10Runtime *runtime, std::string &errors)
				{
					_runtime = runtime;
					_errors.clear();
					_isFatal = false;
					_currentSource.clear();

					for (auto type : _ast.structs)
					{
						Visit(static_cast<FX::Nodes::Struct *>(type));
					}
					for (auto uniform : _ast.uniforms)
					{
						Visit(static_cast<FX::Nodes::Variable *>(uniform));
					}
					for (auto function : _ast.functions)
					{
						Visit(static_cast<FX::Nodes::Function *>(function));
					}
					for (auto technique : _ast.techniques)
					{
						Visit(static_cast<FX::Nodes::Technique *>(technique));
					}

					if (_currentGlobalSize != 0)
					{
						CD3D10_BUFFER_DESC globalsDesc(RoundToMultipleOf16(_currentGlobalSize), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
						D3D10_SUBRESOURCE_DATA globalsInitial;
						globalsInitial.pSysMem = _runtime->GetConstantStorage();
						globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = _runtime->_constantBufferSize = _currentGlobalSize;
						_runtime->_device->CreateBuffer(&globalsDesc, &globalsInitial, &_runtime->_constantBuffer);
					}

					errors += _errors;

					return !_isFatal;
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
				static inline std::string PrintLocation(const FX::location &location)
				{
					return location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): ";
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

					switch (node->id)
					{
						case FX::nodeid::Compound:
							Visit(static_cast<const FX::Nodes::Compound *>(node));
							break;
						case FX::nodeid::DeclaratorList:
							Visit(static_cast<const FX::Nodes::DeclaratorList *>(node));
							break;
						case FX::nodeid::ExpressionStatement:
							Visit(static_cast<const FX::Nodes::ExpressionStatement *>(node));
							break;
						case FX::nodeid::If:
							Visit(static_cast<const FX::Nodes::If *>(node));
							break;
						case FX::nodeid::Switch:
							Visit(static_cast<const FX::Nodes::Switch *>(node));
							break;
						case FX::nodeid::For:
							Visit(static_cast<const FX::Nodes::For *>(node));
							break;
						case FX::nodeid::While:
							Visit(static_cast<const FX::Nodes::While *>(node));
							break;
						case FX::nodeid::Return:
							Visit(static_cast<const FX::Nodes::Return *>(node));
							break;
						case FX::nodeid::Jump:
							Visit(static_cast<const FX::Nodes::Jump *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}
				void Visit(const FX::Nodes::Expression *node)
				{
					switch (node->id)
					{
						case FX::nodeid::LValue:
							Visit(static_cast<const FX::Nodes::LValue *>(node));
							break;
						case FX::nodeid::Literal:
							Visit(static_cast<const FX::Nodes::Literal *>(node));
							break;
						case FX::nodeid::Sequence:
							Visit(static_cast<const FX::Nodes::Sequence *>(node));
							break;
						case FX::nodeid::Unary:
							Visit(static_cast<const FX::Nodes::Unary *>(node));
							break;
						case FX::nodeid::Binary:
							Visit(static_cast<const FX::Nodes::Binary *>(node));
							break;
						case FX::nodeid::Intrinsic:
							Visit(static_cast<const FX::Nodes::Intrinsic *>(node));
							break;
						case FX::nodeid::Conditional:
							Visit(static_cast<const FX::Nodes::Conditional *>(node));
							break;
						case FX::nodeid::Swizzle:
							Visit(static_cast<const FX::Nodes::Swizzle *>(node));
							break;
						case FX::nodeid::FieldSelection:
							Visit(static_cast<const FX::Nodes::FieldSelection *>(node));
							break;
						case FX::nodeid::Assignment:
							Visit(static_cast<const FX::Nodes::Assignment *>(node));
							break;
						case FX::nodeid::Call:
							Visit(static_cast<const FX::Nodes::Call *>(node));
							break;
						case FX::nodeid::Constructor:
							Visit(static_cast<const FX::Nodes::Constructor *>(node));
							break;
						case FX::nodeid::InitializerList:
							Visit(static_cast<const FX::Nodes::InitializerList *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}

				void Visit(const FX::Nodes::Compound *node)
				{
					_currentSource += "{\n";

					for (auto statement : node->Statements)
					{
						Visit(statement);
					}

					_currentSource += "}\n";
				}
				void Visit(const FX::Nodes::DeclaratorList *node)
				{
					for (auto declarator : node->Declarators)
					{
						Visit(declarator);

						if (_currentInForInitialization)
						{
							_currentSource += ", ";
							_currentInForInitialization++;
						}
						else
						{
							_currentSource += ";\n";
						}
					}
				}
				void Visit(const FX::Nodes::ExpressionStatement *node)
				{
					Visit(node->Expression);

					_currentSource += ";\n";
				}
				void Visit(const FX::Nodes::If *node)
				{
					for (auto &attribute : node->Attributes)
					{
						_currentSource += '[';
						_currentSource += attribute;
						_currentSource += ']';
					}

					_currentSource += "if (";
					Visit(node->Condition);
					_currentSource += ")\n";

					if (node->StatementOnTrue != nullptr)
					{
						Visit(node->StatementOnTrue);
					}
					else
					{
						_currentSource += "\t;";
					}
					
					if (node->StatementOnFalse != nullptr)
					{
						_currentSource += "else\n";
						
						Visit(node->StatementOnFalse);
					}
				}
				void Visit(const FX::Nodes::Switch *node)
				{
					for (auto &attribute : node->Attributes)
					{
						_currentSource += '[';
						_currentSource += attribute;
						_currentSource += ']';
					}

					_currentSource += "switch (";
					Visit(node->Test);
					_currentSource += ")\n{\n";

					for (auto cases : node->Cases)
					{
						Visit(cases);
					}

					_currentSource += "}\n";
				}
				void Visit(const FX::Nodes::Case *node)
				{
					for (auto label : node->Labels)
					{
						if (label == nullptr)
						{
							_currentSource += "default";
						}
						else
						{
							_currentSource += "case ";

							Visit(label);
						}

						_currentSource += ":\n";
					}

					Visit(node->Statements);
				}
				void Visit(const FX::Nodes::For *node)
				{
					for (auto &attribute : node->Attributes)
					{
						_currentSource += '[';
						_currentSource += attribute;
						_currentSource += ']';
					}

					_currentSource += "for (";

					if (node->Initialization != nullptr)
					{
						_currentInForInitialization = 1;

						Visit(node->Initialization);

						_currentInForInitialization = 0;

						_currentSource.pop_back();
						_currentSource.pop_back();
					}

					_currentSource += "; ";
										
					if (node->Condition != nullptr)
					{
						Visit(node->Condition);
					}

					_currentSource += "; ";

					if (node->Increment != nullptr)
					{
						Visit(node->Increment);
					}

					_currentSource += ")\n";

					if (node->Statements != nullptr)
					{
						Visit(node->Statements);
					}
					else
					{
						_currentSource += "\t;";
					}
				}
				void Visit(const FX::Nodes::While *node)
				{
					for (auto &attribute : node->Attributes)
					{
						_currentSource += '[';
						_currentSource += attribute;
						_currentSource += ']';
					}

					if (node->DoWhile)
					{
						_currentSource += "do\n{\n";

						if (node->Statements != nullptr)
						{
							Visit(node->Statements);
						}

						_currentSource += "}\n";
						_currentSource += "while (";
						Visit(node->Condition);
						_currentSource += ");\n";
					}
					else
					{
						_currentSource += "while (";
						Visit(node->Condition);
						_currentSource += ")\n";

						if (node->Statements != nullptr)
						{
							Visit(node->Statements);
						}
						else
						{
							_currentSource += "\t;";
						}
					}
				}
				void Visit(const FX::Nodes::Return *node)
				{
					if (node->Discard)
					{
						_currentSource += "discard";
					}
					else
					{
						_currentSource += "return";

						if (node->Value != nullptr)
						{
							_currentSource += ' ';

							Visit(node->Value);
						}
					}

					_currentSource += ";\n";
				}
				void Visit(const FX::Nodes::Jump *node)
				{
					switch (node->Mode)
					{
						case FX::Nodes::Jump::Break:
							_currentSource += "break";
							break;
						case FX::Nodes::Jump::Continue:
							_currentSource += "continue";
							break;
					}

					_currentSource += ";\n";
				}

				void Visit(const FX::Nodes::LValue *node)
				{
					_currentSource += PrintName(node->Reference);
				}
				void Visit(const FX::Nodes::Literal *node)
				{
					if (!node->Type.IsScalar())
					{
						_currentSource += PrintType(node->Type);
						_currentSource += '(';
					}

					for (unsigned int i = 0; i < node->Type.Rows * node->Type.Cols; ++i)
					{
						switch (node->Type.BaseClass)
						{
							case FX::Nodes::Type::Class::Bool:
								_currentSource += node->Value.Int[i] ? "true" : "false";
								break;
							case FX::Nodes::Type::Class::Int:
								_currentSource += std::to_string(node->Value.Int[i]);
								break;
							case FX::Nodes::Type::Class::Uint:
								_currentSource += std::to_string(node->Value.Uint[i]);
								break;
							case FX::Nodes::Type::Class::Float:
								_currentSource += std::to_string(node->Value.Float[i]) + "f";
								break;
						}

						_currentSource += ", ";
					}

					_currentSource.pop_back();
					_currentSource.pop_back();

					if (!node->Type.IsScalar())
					{
						_currentSource += ')';
					}
				}
				void Visit(const FX::Nodes::Sequence *node)
				{
					_currentSource += '(';

					for (auto expression : node->Expressions)
					{
						Visit(expression);

						_currentSource += ", ";
					}
					
					_currentSource.pop_back();
					_currentSource.pop_back();

					_currentSource += ')';
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

					_currentSource += part1;
					Visit(node->Operand);
					_currentSource += part2;
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

					_currentSource += part1;
					Visit(node->Operands[0]);
					_currentSource += part2;
					Visit(node->Operands[1]);
					_currentSource += part3;
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
							if (node->Arguments[2]->id == FX::nodeid::Literal && node->Arguments[2]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[2])->Value.Int[0];

								_currentSource += "__tex2Dgather" + std::to_string(component) + "(";
								Visit(node->Arguments[0]);
								_currentSource += ", ";
								Visit(node->Arguments[1]);
								_currentSource += ")";
							}
							else
							{
								_errors += PrintLocation(node->location) + "error: texture gather component argument has to be constant.\n";
							}
							return;
						case FX::Nodes::Intrinsic::Op::Tex2DGatherOffset:
							if (node->Arguments[3]->id == FX::nodeid::Literal && node->Arguments[3]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[3])->Value.Int[0];

								_currentSource += "__tex2Dgatheroffset" + std::to_string(component) + "(";
								Visit(node->Arguments[0]);
								_currentSource += ", ";
								Visit(node->Arguments[1]);
								_currentSource += ", ";
								Visit(node->Arguments[2]);
								_currentSource += ")";
							}
							else
							{
								_errors += PrintLocation(node->location) + "error: texture gather component argument has to be constant.\n";
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

					_currentSource += part1;

					if (node->Arguments[0] != nullptr)
					{
						Visit(node->Arguments[0]);
					}

					_currentSource += part2;

					if (node->Arguments[1] != nullptr)
					{
						Visit(node->Arguments[1]);
					}

					_currentSource += part3;

					if (node->Arguments[2] != nullptr)
					{
						Visit(node->Arguments[2]);
					}

					_currentSource += part4;

					if (node->Arguments[3] != nullptr)
					{
						Visit(node->Arguments[3]);
					}

					_currentSource += part5;
				}
				void Visit(const FX::Nodes::Conditional *node)
				{
					_currentSource += '(';
					Visit(node->Condition);
					_currentSource += " ? ";
					Visit(node->ExpressionOnTrue);
					_currentSource += " : ";
					Visit(node->ExpressionOnFalse);
					_currentSource += ')';
				}
				void Visit(const FX::Nodes::Swizzle *node)
				{
					Visit(node->Operand);

					_currentSource += '.';

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
							_currentSource += swizzle[node->Mask[i]];
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
							_currentSource += swizzle[node->Mask[i]];
						}
					}
				}
				void Visit(const FX::Nodes::FieldSelection *node)
				{
					_currentSource += '(';

					Visit(node->Operand);

					_currentSource += '.';
					_currentSource += PrintName(node->Field);
					_currentSource += ')';
				}
				void Visit(const FX::Nodes::Assignment *node)
				{
					_currentSource += '(';
					Visit(node->Left);
					_currentSource += ' ';

					switch (node->Operator)
					{
						case FX::Nodes::Assignment::Op::None:
							_currentSource += '=';
							break;
						case FX::Nodes::Assignment::Op::Add:
							_currentSource += "+=";
							break;
						case FX::Nodes::Assignment::Op::Subtract:
							_currentSource += "-=";
							break;
						case FX::Nodes::Assignment::Op::Multiply:
							_currentSource += "*=";
							break;
						case FX::Nodes::Assignment::Op::Divide:
							_currentSource += "/=";
							break;
						case FX::Nodes::Assignment::Op::Modulo:
							_currentSource += "%=";
							break;
						case FX::Nodes::Assignment::Op::LeftShift:
							_currentSource += "<<=";
							break;
						case FX::Nodes::Assignment::Op::RightShift:
							_currentSource += ">>=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseAnd:
							_currentSource += "&=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseOr:
							_currentSource += "|=";
							break;
						case FX::Nodes::Assignment::Op::BitwiseXor:
							_currentSource += "^=";
							break;
					}

					_currentSource += ' ';
					Visit(node->Right);
					_currentSource += ')';
				}
				void Visit(const FX::Nodes::Call *node)
				{
					_currentSource += PrintName(node->Callee);
					_currentSource += '(';

					for (auto argument : node->Arguments)
					{
						Visit(argument);

						_currentSource += ", ";
					}

					if (!node->Arguments.empty())
					{
						_currentSource.pop_back();
						_currentSource.pop_back();
					}

					_currentSource += ')';
				}
				void Visit(const FX::Nodes::Constructor *node)
				{
					_currentSource += PrintType(node->Type);
					_currentSource += '(';

					for (auto argument : node->Arguments)
					{
						Visit(argument);

						_currentSource += ", ";
					}

					if (!node->Arguments.empty())
					{
						_currentSource.pop_back();
						_currentSource.pop_back();
					}

					_currentSource += ')';
				}
				void Visit(const FX::Nodes::InitializerList *node)
				{
					_currentSource += "{ ";

					for (auto expression : node->Values)
					{
						Visit(expression);

						_currentSource += ", ";
					}

					_currentSource += " }";
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
					_currentSource += "struct ";
					_currentSource += PrintName(node);
					_currentSource += "\n{\n";

					if (!node->Fields.empty())
					{
						for (auto field : node->Fields)
						{
							Visit(field);
						}
					}
					else
					{
						_currentSource += "float _dummy;\n";
					}

					_currentSource += "};\n";
				}
				void Visit(const FX::Nodes::Variable *node)
				{
					if (!(_currentInParameterBlock || _currentInFunctionBlock))
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

					if (_currentInForInitialization <= 1)
					{
						_currentSource += PrintTypeWithQualifiers(node->Type);
					}

					if (!node->Name.empty())
					{
						_currentSource += ' ' + PrintName(node);
					}

					if (node->Type.IsArray())
					{
						_currentSource += '[';
						_currentSource += (node->Type.ArrayLength >= 1) ? std::to_string(node->Type.ArrayLength) : "";
						_currentSource += ']';
					}

					if (!node->Semantic.empty())
					{
						_currentSource += " : " + ConvertSemantic(node->Semantic);
					}

					if (node->Initializer != nullptr)
					{
						_currentSource += " = ";

						Visit(node->Initializer);
					}

					if (!(_currentInParameterBlock || _currentInFunctionBlock))
					{
						_currentSource += ";\n";
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

					obj->ShaderRegister = _runtime->_effectShaderResources.size();
					obj->TextureInterface = nullptr;
					obj->ShaderResourceView[0] = nullptr;
					obj->ShaderResourceView[1] = nullptr;

					Visit(node->Annotations, *obj);

					if (node->Semantic == "COLOR" || node->Semantic == "SV_TARGET")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							_errors += PrintLocation(node->location) + "warning: texture property on backbuffer textures are ignored.\n";
						}

						obj->ChangeDataSource(D3D10Texture::Source::BackBuffer, _runtime, _runtime->_backbufferTextureSRV[0], _runtime->_backbufferTextureSRV[1]);
					}
					else if (node->Semantic == "DEPTH" || node->Semantic == "SV_DEPTH")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							_errors += PrintLocation(node->location) + "warning: texture property on depthbuffer textures are ignored.\n";
						}

						obj->ChangeDataSource(D3D10Texture::Source::DepthStencil, _runtime, _runtime->_depthStencilTextureSRV, nullptr);
					}
					else
					{
						obj->DataSource = D3D10Texture::Source::Memory;

						if (texdesc.MipLevels == 0)
						{
							_errors += PrintLocation(node->location) + "warning: a texture cannot have 0 miplevels, changed it to 1.\n";

							texdesc.MipLevels = 1;
						}

						HRESULT hr = _runtime->_device->CreateTexture2D(&texdesc, nullptr, &obj->TextureInterface);

						if (FAILED(hr))
						{
							_errors += PrintLocation(node->location) + "error: 'ID3D10Device::CreateTexture2D' failed with " + std::to_string(hr) + "!\n";
							_isFatal = true;
							return;
						}

						D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
						ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
						srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
						srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
						srvdesc.Format = MakeNonSRGBFormat(texdesc.Format);

						hr = _runtime->_device->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[0]);

						if (FAILED(hr))
						{
							_errors += PrintLocation(node->location) + "error: 'ID3D10Device::CreateShaderResourceView' failed with " + std::to_string(hr) + "!\n";
							_isFatal = true;
							return;
						}

						srvdesc.Format = MakeSRGBFormat(texdesc.Format);

						if (srvdesc.Format != texdesc.Format)
						{
							hr = _runtime->_device->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[1]);

							if (FAILED(hr))
							{
								_errors += PrintLocation(node->location) + "error: 'ID3D10Device::CreateShaderResourceView' failed with " + std::to_string(hr) + "!\n";
								_isFatal = true;
								return;
							}
						}
					}

					_currentSource += "Texture2D ";
					_currentSource += PrintName(node);
					_currentSource += " : register(t" + std::to_string(_runtime->_effectShaderResources.size()) + "), __";
					_currentSource += PrintName(node);
					_currentSource += "SRGB : register(t" + std::to_string(_runtime->_effectShaderResources.size() + 1) + ");\n";

					_runtime->_effectShaderResources.push_back(obj->ShaderResourceView[0]);
					_runtime->_effectShaderResources.push_back(obj->ShaderResourceView[1]);

					_runtime->AddTexture(obj);
				}
				void VisitSampler(const FX::Nodes::Variable *node)
				{
					if (node->Properties.Texture == nullptr)
					{
						_errors += PrintLocation(node->location) + "error: sampler '" + node->Name + "' is missing required 'Texture' required.\n";
						_isFatal = true;
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

					D3D10Texture *texture = static_cast<D3D10Texture *>(_runtime->GetTexture(node->Properties.Texture->Name));

					if (texture == nullptr)
					{
						_errors += PrintLocation(node->location) + "error: texture '" + node->Properties.Texture->Name + "' for sampler '" + std::string(node->Name) + "' is missing due to previous error.\n";
						_isFatal = true;
						return;
					}

					const size_t descHash = D3D10_SAMPLER_DESC_HASH(desc);
					auto it = _samplerDescs.find(descHash);

					if (it == _samplerDescs.end())
					{
						ID3D10SamplerState *sampler = nullptr;

						HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

						if (FAILED(hr))
						{
							_errors += PrintLocation(node->location) + "error: 'ID3D10Device::CreateSamplerState' failed with " + std::to_string(hr) + "!\n";
							_isFatal = true;
							return;
						}

						_runtime->_effectSamplerStates.push_back(sampler);
						it = _samplerDescs.emplace(descHash, _runtime->_effectSamplerStates.size() - 1).first;

						_currentSource += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
					}

					_currentSource += "static const __sampler2D ";
					_currentSource += PrintName(node);
					_currentSource += " = { ";

					if (node->Properties.SRGBTexture && texture->ShaderResourceView[1] != nullptr)
					{
						_currentSource += "__";
						_currentSource += PrintName(node->Properties.Texture);
						_currentSource += "SRGB";
					}
					else
					{
						_currentSource += PrintName(node->Properties.Texture);
					}

					_currentSource += ", __SamplerState" + std::to_string(it->second) + " };\n";
				}
				void VisitUniform(const FX::Nodes::Variable *node)
				{
					_currentGlobalConstants += PrintTypeWithQualifiers(node->Type);
					_currentGlobalConstants += ' ';
					_currentGlobalConstants += PrintName(node);

					if (node->Type.IsArray())
					{
						_currentGlobalConstants += '[';
						_currentGlobalConstants += (node->Type.ArrayLength >= 1) ? std::to_string(node->Type.ArrayLength) : "";
						_currentGlobalConstants += ']';
					}

					_currentGlobalConstants += ";\n";

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

					const UINT alignment = 16 - (_currentGlobalSize % 16);
					_currentGlobalSize += static_cast<UINT>((obj->StorageSize > alignment && (alignment != 16 || obj->StorageSize <= 16)) ? obj->StorageSize + alignment : obj->StorageSize);
					obj->StorageOffset = _currentGlobalSize - obj->StorageSize;

					Visit(node->Annotations, *obj);

					if (_currentGlobalSize >= _runtime->GetConstantStorageSize())
					{
						_runtime->EnlargeConstantStorage();
					}

					if (node->Initializer != nullptr && node->Initializer->id == FX::nodeid::Literal)
					{
						CopyMemory(_runtime->GetConstantStorage() + obj->StorageOffset, &static_cast<const FX::Nodes::Literal *>(node->Initializer)->Value, obj->StorageSize);
					}
					else
					{
						ZeroMemory(_runtime->GetConstantStorage() + obj->StorageOffset, obj->StorageSize);
					}

					_runtime->AddConstant(obj);
				}
				void Visit(const FX::Nodes::Function *node)
				{
					_currentSource += PrintType(node->ReturnType);
					_currentSource += ' ';
					_currentSource += PrintName(node);
					_currentSource += '(';

					_currentInParameterBlock = true;

					for (auto parameter : node->Parameters)
					{
						Visit(parameter);

						_currentSource += ", ";
					}

					_currentInParameterBlock = false;

					if (!node->Parameters.empty())
					{
						_currentSource.pop_back();
						_currentSource.pop_back();
					}

					_currentSource += ')';
								
					if (!node->ReturnSemantic.empty())
					{
						_currentSource += " : " + ConvertSemantic(node->ReturnSemantic);
					}

					_currentSource += '\n';

					_currentInFunctionBlock = true;

					Visit(node->Definition);

					_currentInFunctionBlock = false;
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

					_runtime->AddTechnique(obj);
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
					pass.SRV = _runtime->_effectShaderResources;

					if (node->States.VertexShader != nullptr)
					{
						VisitShader(node->States.VertexShader, "vs", pass);
					}
					if (node->States.PixelShader != nullptr)
					{
						VisitShader(node->States.PixelShader, "ps", pass);
					}

					const int targetIndex = node->States.SRGBWriteEnable ? 1 : 0;
					pass.RT[0] = _runtime->_backbufferTargets[targetIndex];
					pass.RTSRV[0] = _runtime->_backbufferTextureSRV[targetIndex];

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->States.RenderTargets[i] == nullptr)
						{
							continue;
						}

						D3D10Texture *texture = static_cast<D3D10Texture *>(_runtime->GetTexture(node->States.RenderTargets[i]->Name));

						if (texture == nullptr)
						{
							_isFatal = true;
							return;
						}

						D3D10_TEXTURE2D_DESC desc;
						texture->TextureInterface->GetDesc(&desc);

						if (pass.Viewport.Width != 0 && pass.Viewport.Height != 0 && (desc.Width != static_cast<unsigned int>(pass.Viewport.Width) || desc.Height != static_cast<unsigned int>(pass.Viewport.Height)))
						{
							_errors += PrintLocation(node->location) + "error: cannot use multiple rendertargets with different sized textures.\n";
							_isFatal = true;
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
							HRESULT hr = _runtime->_device->CreateRenderTargetView(texture->TextureInterface, &rtvdesc, &texture->RenderTargetView[targetIndex]);

							if (FAILED(hr))
							{
								_errors += PrintLocation(node->location) + "warning: 'CreateRenderTargetView' failed!\n";
							}
						}

						pass.RT[i] = texture->RenderTargetView[targetIndex];
						pass.RTSRV[i] = texture->ShaderResourceView[targetIndex];
					}

					if (pass.Viewport.Width == 0 && pass.Viewport.Height == 0)
					{
						pass.Viewport.Width = _runtime->GetBufferWidth();
						pass.Viewport.Height = _runtime->GetBufferHeight();
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

					HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.DSS);

					if (FAILED(hr))
					{
						_errors += PrintLocation(node->location) + "warning: 'ID3D10Device::CreateDepthStencilState' failed!\n";
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

					hr = _runtime->_device->CreateBlendState(&bdesc, &pass.BS);

					if (FAILED(hr))
					{
						_errors += PrintLocation(node->location) + "warning: 'ID3D10Device::CreateBlendState' failed!\n";
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
					
					if (SUCCEEDED(_runtime->_device->QueryInterface(&device1)))
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

					if (!_currentGlobalConstants.empty())
					{
						source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + _currentGlobalConstants + "};\n";
					}

					source += _currentSource;

					LOG(TRACE) << "> Compiling shader '" << node->Name << "':\n\n" << source.c_str() << "\n";

					UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (_skipShaderOptimization)
					{
						flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
					}

					HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, PrintName(node).c_str(), profile.c_str(), flags, 0, &compiled, &errors);

					if (errors != nullptr)
					{
						_errors += std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

						errors->Release();
					}

					if (FAILED(hr))
					{
						_isFatal = true;
						return;
					}

					if (shadertype == "vs")
					{
						hr = _runtime->_device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.VS);
					}
					else if (shadertype == "ps")
					{
						hr = _runtime->_device->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), &pass.PS);
					}

					compiled->Release();

					if (FAILED(hr))
					{
						_errors += PrintLocation(node->location) + "error: 'CreateShader' failed!\n";
						_isFatal = true;
						return;
					}
				}

			private:
				const FX::nodetree &_ast;
				D3D10Runtime *_runtime;
				std::string _currentSource;
				std::string _errors;
				bool _isFatal, _skipShaderOptimization;
				std::unordered_map<size_t, size_t> _samplerDescs;
				std::string _currentGlobalConstants;
				UINT _currentGlobalSize, _currentInForInitialization;
				bool _currentInParameterBlock, _currentInFunctionBlock;
			};

			UINT GetRendererId(ID3D10Device *device)
			{
				ID3D10Device1 *device1 = nullptr;

				if (SUCCEEDED(device->QueryInterface(&device1)))
				{
					device1->Release();

					return device1->GetFeatureLevel();
				}
				else
				{
					return D3D10_FEATURE_LEVEL_10_0;
				}
			}
		}

		class D3D10StateBlock
		{
		public:
			D3D10StateBlock(ID3D10Device *device)
			{
				ZeroMemory(this, sizeof(D3D10StateBlock));

				_device = device;
				_device->AddRef();
			}
			~D3D10StateBlock()
			{
				ReleaseAllDeviceObjects();

				_device->Release();
			}

			void Capture()
			{
				ReleaseAllDeviceObjects();

				_device->IAGetPrimitiveTopology(&_IAPrimitiveTopology);
				_device->IAGetInputLayout(&_IAInputLayout);

				_device->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				_device->IAGetIndexBuffer(&_IAIndexBuffer, &_IAIndexFormat, &_IAIndexOffset);

				_device->RSGetState(&_RSState);
				_device->RSGetViewports(&_RSNumViewports, nullptr);
				_device->RSGetViewports(&_RSNumViewports, _RSViewports);

				_device->VSGetShader(&_VS);
				_device->VSGetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _VSConstantBuffers);
				_device->VSGetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, _VSSamplerStates);
				_device->VSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _VSShaderResources);

				_device->GSGetShader(&_GS);

				_device->PSGetShader(&_PS);
				_device->PSGetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _PSConstantBuffers);
				_device->PSGetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, _PSSamplerStates);
				_device->PSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _PSShaderResources);

				_device->OMGetBlendState(&_OMBlendState, _OMBlendFactor, &_OMSampleMask);
				_device->OMGetDepthStencilState(&_OMDepthStencilState, &_OMStencilRef);
				_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, _OMRenderTargets, &_OMDepthStencil);
			}
			void Apply()
			{
				_device->IASetPrimitiveTopology(_IAPrimitiveTopology);
				_device->IASetInputLayout(_IAInputLayout);

				_device->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				_device->IASetIndexBuffer(_IAIndexBuffer, _IAIndexFormat, _IAIndexOffset);

				_device->RSSetState(_RSState);
				_device->RSSetViewports(_RSNumViewports, _RSViewports);

				_device->VSSetShader(_VS);
				_device->VSSetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _VSConstantBuffers);
				_device->VSSetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, _VSSamplerStates);
				_device->VSSetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _VSShaderResources);

				_device->GSSetShader(_GS);

				_device->PSSetShader(_PS);
				_device->PSSetConstantBuffers(0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _PSConstantBuffers);
				_device->PSSetSamplers(0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT, _PSSamplerStates);
				_device->PSSetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _PSShaderResources);

				_device->OMSetBlendState(_OMBlendState, _OMBlendFactor, _OMSampleMask);
				_device->OMSetDepthStencilState(_OMDepthStencilState, _OMStencilRef);
				_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, _OMRenderTargets, _OMDepthStencil);
			}
			void ReleaseAllDeviceObjects()
			{
				SAFE_RELEASE(_IAInputLayout);

				for (UINT i = 0; i < D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_IAVertexBuffers[i]);
				}

				SAFE_RELEASE(_IAIndexBuffer);

				SAFE_RELEASE(_VS);

				for (UINT i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSShaderResources[i]);
				}

				SAFE_RELEASE(_GS);

				SAFE_RELEASE(_RSState);

				SAFE_RELEASE(_PS);

				for (UINT i = 0; i < D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSShaderResources[i]);
				}

				SAFE_RELEASE(_OMBlendState);
				SAFE_RELEASE(_OMDepthStencilState);

				for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					SAFE_RELEASE(_OMRenderTargets[i]);
				}

				SAFE_RELEASE(_OMDepthStencil);
			}

		private:
			ID3D10Device *_device;

			ID3D10InputLayout *_IAInputLayout;
			D3D10_PRIMITIVE_TOPOLOGY _IAPrimitiveTopology;
			ID3D10Buffer *_IAVertexBuffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT _IAVertexStrides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT _IAVertexOffsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10Buffer *_IAIndexBuffer;
			DXGI_FORMAT _IAIndexFormat;
			UINT _IAIndexOffset;
			ID3D10VertexShader *_VS;
			ID3D10Buffer *_VSConstantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10SamplerState *_VSSamplerStates[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10ShaderResourceView *_VSShaderResources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10GeometryShader *_GS;
			ID3D10RasterizerState *_RSState;
			UINT _RSNumViewports;
			D3D10_VIEWPORT _RSViewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			ID3D10PixelShader *_PS;
			ID3D10Buffer *_PSConstantBuffers[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D10SamplerState *_PSSamplerStates[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D10ShaderResourceView *_PSShaderResources[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D10BlendState *_OMBlendState;
			FLOAT _OMBlendFactor[4];
			UINT _OMSampleMask;
			ID3D10DepthStencilState *_OMDepthStencilState;
			UINT _OMStencilRef;
			ID3D10RenderTargetView *_OMRenderTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D10DepthStencilView *_OMDepthStencil;
		};

		// ---------------------------------------------------------------------------------------------------

		D3D10Runtime::D3D10Runtime(ID3D10Device *device, IDXGISwapChain *swapchain) : Runtime(GetRendererId(device)), _device(device), _swapchain(swapchain), _backbufferFormat(DXGI_FORMAT_UNKNOWN), _multisamplingEnabled(false), _stateBlock(new D3D10StateBlock(device)), _backbuffer(nullptr), _backbufferResolved(nullptr), _backbufferTexture(nullptr), _backbufferTextureSRV(), _backbufferTargets(), _depthStencil(nullptr), _depthStencilReplacement(nullptr), _depthStencilTexture(nullptr), _depthStencilTextureSRV(nullptr), _copyVS(nullptr), _copyPS(nullptr), _copySampler(nullptr), _effectRasterizerState(nullptr), _constantBuffer(nullptr), _constantBufferSize(0)
		{
			_device->AddRef();
			_swapchain->AddRef();

			IDXGIDevice *dxgidevice = nullptr;
			IDXGIAdapter *adapter = nullptr;

			HRESULT hr = _device->QueryInterface(&dxgidevice);

			assert(SUCCEEDED(hr));

			hr = dxgidevice->GetAdapter(&adapter);

			dxgidevice->Release();

			assert(SUCCEEDED(hr));

			DXGI_ADAPTER_DESC desc;
			hr = adapter->GetDesc(&desc);

			adapter->Release();

			assert(SUCCEEDED(hr));
			
			_vendorId = desc.VendorId;
			_deviceId = desc.DeviceId;
		}
		D3D10Runtime::~D3D10Runtime()
		{
			_device->Release();
			_swapchain->Release();
		}

		bool D3D10Runtime::OnInit(const DXGI_SWAP_CHAIN_DESC &desc)
		{
			_width = desc.BufferDesc.Width;
			_height = desc.BufferDesc.Height;
			_backbufferFormat = desc.BufferDesc.Format;
			_multisamplingEnabled = desc.SampleDesc.Count > 1;
			_input.reset(new Input(desc.OutputWindow));

			#pragma region Get backbuffer rendertarget
			HRESULT hr = _swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&_backbuffer));

			assert(SUCCEEDED(hr));

			D3D10_TEXTURE2D_DESC texdesc;
			texdesc.Width = _width;
			texdesc.Height = _height;
			texdesc.ArraySize = texdesc.MipLevels = 1;
			texdesc.Format = D3D10EffectCompiler::MakeTypelessFormat(_backbufferFormat);
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D10_USAGE_DEFAULT;
			texdesc.BindFlags = D3D10_BIND_RENDER_TARGET;
			texdesc.MiscFlags = texdesc.CPUAccessFlags = 0;

			OSVERSIONINFOEX verinfoWin7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
			const bool isWindows7 = VerifyVersionInfo(&verinfoWin7, VER_MAJORVERSION | VER_MINORVERSION, VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;

			if (_multisamplingEnabled || D3D10EffectCompiler::MakeNonSRGBFormat(_backbufferFormat) != _backbufferFormat || !isWindows7)
			{
				hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbufferResolved);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer replacement (Width = " << texdesc.Width << ", Height = " << texdesc.Height << ", Format = " << texdesc.Format << ", SampleCount = " << texdesc.SampleDesc.Count << ", SampleQuality = " << texdesc.SampleDesc.Quality << ")! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_backbuffer);

					return false;
				}

				hr = _device->CreateRenderTargetView(_backbuffer, nullptr, &_backbufferTargets[2]);

				assert(SUCCEEDED(hr));
			}
			else
			{
				_backbufferResolved = _backbuffer;
				_backbufferResolved->AddRef();
			}
			#pragma endregion

			#pragma region Create backbuffer shader texture
			texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

			hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbufferTexture);

			if (SUCCEEDED(hr))
			{
				D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
				ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
				srvdesc.Format = D3D10EffectCompiler::MakeNonSRGBFormat(texdesc.Format);
				srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = texdesc.MipLevels;

				if (SUCCEEDED(hr))
				{
					hr = _device->CreateShaderResourceView(_backbufferTexture, &srvdesc, &_backbufferTextureSRV[0]);
				}
				else
				{
					LOG(TRACE) << "Failed to create backbuffer texture resource view (Format = " << srvdesc.Format << ")! HRESULT is '" << hr << "'.";
				}

				srvdesc.Format = D3D10EffectCompiler::MakeSRGBFormat(texdesc.Format);

				if (SUCCEEDED(hr))
				{
					hr = _device->CreateShaderResourceView(_backbufferTexture, &srvdesc, &_backbufferTextureSRV[1]);
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
				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSRV[0]);
				SAFE_RELEASE(_backbufferTextureSRV[1]);

				return false;
			}
			#pragma endregion

			D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
			ZeroMemory(&rtdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
			rtdesc.Format = D3D10EffectCompiler::MakeNonSRGBFormat(texdesc.Format);
			rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;

			hr = _device->CreateRenderTargetView(_backbufferResolved, &rtdesc, &_backbufferTargets[0]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSRV[0]);
				SAFE_RELEASE(_backbufferTextureSRV[1]);

				LOG(TRACE) << "Failed to create backbuffer rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

				return false;
			}

			rtdesc.Format = D3D10EffectCompiler::MakeSRGBFormat(texdesc.Format);

			hr = _device->CreateRenderTargetView(_backbufferResolved, &rtdesc, &_backbufferTargets[1]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSRV[0]);
				SAFE_RELEASE(_backbufferTextureSRV[1]);
				SAFE_RELEASE(_backbufferTargets[0]);

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
			hr = _device->CreateTexture2D(&dstdesc, nullptr, &dstexture);

			if (SUCCEEDED(hr))
			{
				hr = _device->CreateDepthStencilView(dstexture, nullptr, &_defaultDepthStencil);

				SAFE_RELEASE(dstexture);
			}
			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				return false;
			}
			#pragma endregion

			const BYTE vs[] = { 68, 88, 66, 67, 224, 206, 72, 137, 142, 185, 68, 219, 247, 216, 225, 132, 111, 78, 106, 20, 1, 0, 0, 0, 156, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 140, 0, 0, 0, 192, 0, 0, 0, 24, 1, 0, 0, 32, 2, 0, 0, 82, 68, 69, 70, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 0, 0, 0, 0, 4, 254, 255, 0, 1, 0, 0, 28, 0, 0, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 171, 171, 73, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 83, 86, 95, 86, 101, 114, 116, 101, 120, 73, 68, 0, 79, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 12, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 83, 72, 68, 82, 0, 1, 0, 0, 64, 0, 1, 0, 64, 0, 0, 0, 96, 0, 0, 4, 18, 16, 16, 0, 0, 0, 0, 0, 6, 0, 0, 0, 103, 0, 0, 4, 242, 32, 16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 101, 0, 0, 3, 50, 32, 16, 0, 1, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 32, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 6, 16, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 15, 50, 32, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 128, 191, 0, 0, 128, 63, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 5, 50, 32, 16, 0, 1, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 54, 0, 0, 8, 194, 32, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = _device->CreateVertexShader(vs, ARRAYSIZE(vs), &_copyVS);

			assert(SUCCEEDED(hr));

			const BYTE ps[] = { 68, 88, 66, 67, 93, 102, 148, 45, 34, 106, 51, 79, 54, 23, 136, 21, 27, 217, 232, 71, 1, 0, 0, 0, 116, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 208, 0, 0, 0, 40, 1, 0, 0, 92, 1, 0, 0, 248, 1, 0, 0, 82, 68, 69, 70, 148, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 28, 0, 0, 0, 0, 4, 255, 255, 0, 1, 0, 0, 98, 0, 0, 0, 92, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 95, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 1, 0, 0, 0, 13, 0, 0, 0, 115, 48, 0, 116, 48, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 73, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 3, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 79, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 83, 86, 95, 84, 97, 114, 103, 101, 116, 0, 171, 171, 83, 72, 68, 82, 148, 0, 0, 0, 64, 0, 0, 0, 37, 0, 0, 0, 90, 0, 0, 3, 0, 96, 16, 0, 0, 0, 0, 0, 88, 24, 0, 4, 0, 112, 16, 0, 0, 0, 0, 0, 85, 85, 0, 0, 98, 16, 0, 3, 50, 16, 16, 0, 1, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 69, 0, 0, 9, 242, 0, 16, 0, 0, 0, 0, 0, 70, 16, 16, 0, 1, 0, 0, 0, 70, 126, 16, 0, 0, 0, 0, 0, 0, 96, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 114, 32, 16, 0, 0, 0, 0, 0, 70, 2, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 130, 32, 16, 0, 0, 0, 0, 0, 1, 64, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = _device->CreatePixelShader(ps, ARRAYSIZE(ps), &_copyPS);

			assert(SUCCEEDED(hr));

			const D3D10_SAMPLER_DESC copysampdesc = { D3D10_FILTER_MIN_MAG_MIP_POINT, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP };
			hr = _device->CreateSamplerState(&copysampdesc, &_copySampler);

			assert(SUCCEEDED(hr));

			#pragma region Create effect objects
			D3D10_RASTERIZER_DESC rsdesc;
			ZeroMemory(&rsdesc, sizeof(D3D10_RASTERIZER_DESC));
			rsdesc.FillMode = D3D10_FILL_SOLID;
			rsdesc.CullMode = D3D10_CULL_NONE;
			rsdesc.DepthClipEnable = TRUE;

			hr = _device->CreateRasterizerState(&rsdesc, &_effectRasterizerState);

			assert(SUCCEEDED(hr));
			#pragma endregion

			_gui.reset(new GUI(this, nvgCreateD3D10(_device, 0)));

			return Runtime::OnInit();
		}
		void D3D10Runtime::OnReset()
		{
			if (!_isInitialized)
			{
				return;
			}

			Runtime::OnReset();

			// Destroy NanoVG
			NVGcontext *const nvg = _gui->GetContext();

			_gui.reset();

			nvgDeleteD3D10(nvg);

			// Destroy resources
			SAFE_RELEASE(_backbuffer);
			SAFE_RELEASE(_backbufferResolved);
			SAFE_RELEASE(_backbufferTexture);
			SAFE_RELEASE(_backbufferTextureSRV[0]);
			SAFE_RELEASE(_backbufferTextureSRV[1]);
			SAFE_RELEASE(_backbufferTargets[0]);
			SAFE_RELEASE(_backbufferTargets[1]);
			SAFE_RELEASE(_backbufferTargets[2]);

			SAFE_RELEASE(_depthStencil);
			SAFE_RELEASE(_depthStencilReplacement);
			SAFE_RELEASE(_depthStencilTexture);
			SAFE_RELEASE(_depthStencilTextureSRV);

			SAFE_RELEASE(_defaultDepthStencil);
			SAFE_RELEASE(_copyVS);
			SAFE_RELEASE(_copyPS);
			SAFE_RELEASE(_copySampler);

			SAFE_RELEASE(_effectRasterizerState);
		}
		void D3D10Runtime::OnResetEffect()
		{
			Runtime::OnResetEffect();

			for (auto it : _effectSamplerStates)
			{
				it->Release();
			}

			_effectSamplerStates.clear();
			_effectShaderResources.clear();

			SAFE_RELEASE(_constantBuffer);

			_constantBufferSize = 0;
		}
		void D3D10Runtime::OnPresent()
		{
			if (!_isInitialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (_stats.DrawCalls == 0)
			{
				return;
			}

			DetectDepthSource();

			// Capture device state
			_stateBlock->Capture();

			ID3D10RenderTargetView *stateblockTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
			ID3D10DepthStencilView *stateblockDepthStencil = nullptr;

			_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblockTargets, &stateblockDepthStencil);

			// Resolve backbuffer
			if (_backbufferResolved != _backbuffer)
			{
				_device->ResolveSubresource(_backbufferResolved, 0, _backbuffer, 0, _backbufferFormat);
			}

			// Apply post processing
			OnApplyEffect();

			// Reset rendertarget
			_device->OMSetRenderTargets(1, &_backbufferTargets[0], _defaultDepthStencil);

			const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
			_device->RSSetViewports(1, &viewport);

			// Apply presenting
			Runtime::OnPresent();

			// Copy to backbuffer
			if (_backbufferResolved != _backbuffer)
			{
				_device->OMSetRenderTargets(1, &_backbufferTargets[2], nullptr);
				_device->CopyResource(_backbufferTexture, _backbufferResolved);

				_device->VSSetShader(_copyVS);
				_device->PSSetShader(_copyPS);
				_device->PSSetSamplers(0, 1, &_copySampler);
				_device->PSSetShaderResources(0, 1, &_backbufferTextureSRV[D3D10EffectCompiler::MakeSRGBFormat(_backbufferFormat) == _backbufferFormat]);
				_device->Draw(3, 0);
			}

			// Apply previous device state
			_stateBlock->Apply();

			_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblockTargets, stateblockDepthStencil);

			_stateBlock->ReleaseAllDeviceObjects();

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
			_device->OMGetRenderTargets(0, nullptr, &depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == _defaultDepthStencil)
				{
					return;
				}
				else if (depthstencil == _depthStencilReplacement)
				{
					depthstencil = _depthStencil;
				}

				const auto it = _depthSourceTable.find(depthstencil);

				if (it != _depthSourceTable.end())
				{
					it->second.DrawCallCount = static_cast<FLOAT>(_stats.DrawCalls);
					it->second.DrawVerticesCount += vertices;
				}
			}
		}
		void D3D10Runtime::OnApplyEffect()
		{
			if (!_isEffectCompiled)
			{
				return;
			}

			// Setup real backbuffer
			_device->OMSetRenderTargets(1, &_backbufferTargets[0], nullptr);

			// Setup vertex input
			const uintptr_t null = 0;
			_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_device->IASetInputLayout(nullptr);
			_device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

			_device->RSSetState(_effectRasterizerState);

			// Disable unused pipeline stages
			_device->GSSetShader(nullptr);

			// Setup samplers
			_device->VSSetSamplers(0, static_cast<UINT>(_effectSamplerStates.size()), _effectSamplerStates.data());
			_device->PSSetSamplers(0, static_cast<UINT>(_effectSamplerStates.size()), _effectSamplerStates.data());

			// Setup shader constants
			_device->VSSetConstantBuffers(0, 1, &_constantBuffer);
			_device->PSSetConstantBuffers(0, 1, &_constantBuffer);

			// Apply post processing
			Runtime::OnApplyEffect();
		}
		void D3D10Runtime::OnApplyEffectTechnique(const Technique *technique)
		{
			Runtime::OnApplyEffectTechnique(technique);

			bool defaultDepthStencilCleared = false;

			// Update shader constants
			if (_constantBuffer != nullptr)
			{
				void *data = nullptr;

				const HRESULT hr = _constantBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &data);

				if (SUCCEEDED(hr))
				{
					CopyMemory(data, _uniformDataStorage.data(), _constantBufferSize);

					_constantBuffer->Unmap();
				}
				else
				{
					LOG(TRACE) << "Failed to map constant buffer! HRESULT is '" << hr << "'!";
				}
			}

			for (const auto &pass : static_cast<const D3D10Technique *>(technique)->Passes)
			{
				// Setup states
				_device->VSSetShader(pass.VS);
				_device->PSSetShader(pass.PS);

				const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				_device->OMSetBlendState(pass.BS, blendfactor, D3D10_DEFAULT_SAMPLE_MASK);
				_device->OMSetDepthStencilState(pass.DSS, pass.StencilRef);

				// Save backbuffer of previous pass
				_device->CopyResource(_backbufferTexture, _backbufferResolved);

				// Setup shader resources
				_device->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());
				_device->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());

				// Setup rendertargets
				if (pass.Viewport.Width == _width && pass.Viewport.Height == _height)
				{
					_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, _defaultDepthStencil);

					if (!defaultDepthStencilCleared)
					{
						defaultDepthStencilCleared = true;

						// Clear depthstencil
						_device->ClearDepthStencilView(_defaultDepthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0);
					}
				}
				else
				{
					_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, nullptr);
				}

				_device->RSSetViewports(1, &pass.Viewport);

				for (ID3D10RenderTargetView *const target : pass.RT)
				{
					if (target == nullptr)
					{
						continue;
					}

					const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					_device->ClearRenderTargetView(target, color);
				}

				// Draw triangle
				_device->Draw(3, 0);

				Runtime::OnDrawCall(3);

				// Reset rendertargets
				_device->OMSetRenderTargets(0, nullptr, nullptr);

				// Reset shader resources
				ID3D10ShaderResourceView *null[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
				_device->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);
				_device->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);

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
						_device->GenerateMips(resource);
					}
				}
			}
		}

		void D3D10Runtime::OnCreateDepthStencilView(ID3D10Resource *resource, ID3D10DepthStencilView *depthstencil)
		{
			assert(resource != nullptr);
			assert(depthstencil != nullptr);

			// Do not track default depthstencil
			if (!_isInitialized)
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
			if (desc.Width != _width || desc.Height != _height || desc.SampleDesc.Count > 1)
			{
				return;
			}

			LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

			// Begin tracking new depthstencil
			const DepthSourceInfo info = { desc.Width, desc.Height };
			_depthSourceTable.emplace(depthstencil, info);
		}
		void D3D10Runtime::OnDeleteDepthStencilView(ID3D10DepthStencilView *depthstencil)
		{
			assert(depthstencil != nullptr);

			const auto it = _depthSourceTable.find(depthstencil);
		
			if (it != _depthSourceTable.end())
			{
				LOG(TRACE) << "Removing depthstencil " << depthstencil << " from list of possible depth candidates ...";

				_depthSourceTable.erase(it);

				if (depthstencil == _depthStencil)
				{
					CreateDepthStencilReplacement(nullptr);
				}
			}
		}
		void D3D10Runtime::OnSetDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (_depthStencilReplacement != nullptr && depthstencil == _depthStencil)
			{
				depthstencil = _depthStencilReplacement;
			}
		}
		void D3D10Runtime::OnGetDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (_depthStencilReplacement != nullptr && depthstencil == _depthStencilReplacement)
			{
				depthstencil->Release();

				depthstencil = _depthStencil;
				depthstencil->AddRef();
			}
		}
		void D3D10Runtime::OnClearDepthStencilView(ID3D10DepthStencilView *&depthstencil)
		{
			if (_depthStencilReplacement != nullptr && depthstencil == _depthStencil)
			{
				depthstencil = _depthStencilReplacement;
			}
		}
		void D3D10Runtime::OnCopyResource(ID3D10Resource *&dest, ID3D10Resource *&source)
		{
			if (_depthStencilReplacement != nullptr)
			{
				ID3D10Resource *resource = nullptr;
				_depthStencil->GetResource(&resource);

				if (dest == resource)
				{
					dest = _depthStencilTexture;
				}
				if (source == resource)
				{
					source = _depthStencilTexture;
				}

				resource->Release();
			}
		}

		void D3D10Runtime::Screenshot(unsigned char *buffer) const
		{
			if (_backbufferFormat != DXGI_FORMAT_R8G8B8A8_UNORM && _backbufferFormat != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && _backbufferFormat != DXGI_FORMAT_B8G8R8A8_UNORM && _backbufferFormat != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbufferFormat << ".";
				return;
			}

			D3D10_TEXTURE2D_DESC texdesc;
			ZeroMemory(&texdesc, sizeof(D3D10_TEXTURE2D_DESC));
			texdesc.Width = _width;
			texdesc.Height = _height;
			texdesc.Format = _backbufferFormat;
			texdesc.MipLevels = 1;
			texdesc.ArraySize = 1;
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D10_USAGE_STAGING;
			texdesc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;

			ID3D10Texture2D *textureStaging = nullptr;
			HRESULT hr = _device->CreateTexture2D(&texdesc, nullptr, &textureStaging);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create staging texture for screenshot capture! HRESULT is '" << hr << "'.";
				return;
			}

			_device->CopyResource(textureStaging, _backbufferResolved);
				
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

					if (_backbufferFormat == DXGI_FORMAT_B8G8R8A8_UNORM || _backbufferFormat == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
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
		bool D3D10Runtime::UpdateEffect(const FX::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors)
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

			_device->UpdateSubresource(textureImpl->TextureInterface, 0, nullptr, data, static_cast<UINT>(size / texture->Height), static_cast<UINT>(size));

			if (texture->Levels > 1)
			{
				_device->GenerateMips(textureImpl->ShaderResourceView[0]);
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

			if (_multisamplingEnabled || _depthSourceTable.empty())
			{
				return;
			}

			DepthSourceInfo bestInfo = { 0 };
			ID3D10DepthStencilView *best = nullptr;

			for (auto &it : _depthSourceTable)
			{
				if (it.second.DrawCallCount == 0)
				{
					continue;
				}
				else if ((it.second.DrawVerticesCount * (1.2f - it.second.DrawCallCount / _stats.DrawCalls)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / _stats.DrawCalls)))
				{
					best = it.first;
					bestInfo = it.second;
				}

				it.second.DrawCallCount = it.second.DrawVerticesCount = 0;
			}

			if (best != nullptr && _depthStencil != best)
			{
				LOG(TRACE) << "Switched depth source to depthstencil " << best << ".";

				CreateDepthStencilReplacement(best);
			}
		}
		bool D3D10Runtime::CreateDepthStencilReplacement(ID3D10DepthStencilView *depthstencil)
		{
			SAFE_RELEASE(_depthStencil);
			SAFE_RELEASE(_depthStencilReplacement);
			SAFE_RELEASE(_depthStencilTexture);
			SAFE_RELEASE(_depthStencilTextureSRV);

			if (depthstencil != nullptr)
			{
				_depthStencil = depthstencil;
				_depthStencil->AddRef();
				_depthStencil->GetResource(reinterpret_cast<ID3D10Resource **>(&_depthStencilTexture));

				D3D10_TEXTURE2D_DESC texdesc;
				_depthStencilTexture->GetDesc(&texdesc);

				HRESULT hr = S_OK;

				if ((texdesc.BindFlags & D3D10_BIND_SHADER_RESOURCE) == 0)
				{
					SAFE_RELEASE(_depthStencilTexture);

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

					hr = _device->CreateTexture2D(&texdesc, nullptr, &_depthStencilTexture);

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

						hr = _device->CreateDepthStencilView(_depthStencilTexture, &dsvdesc, &_depthStencilReplacement);
					}
				}
				else
				{
					_depthStencilReplacement = _depthStencil;
					_depthStencilReplacement->AddRef();
				}

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_depthStencil);
					SAFE_RELEASE(_depthStencilTexture);

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

				hr = _device->CreateShaderResourceView(_depthStencilTexture, &srvdesc, &_depthStencilTextureSRV);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement resource view! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_depthStencil);
					SAFE_RELEASE(_depthStencilReplacement);
					SAFE_RELEASE(_depthStencilTexture);

					return false;
				}

				if (_depthStencil != _depthStencilReplacement)
				{
					// Update auto depthstencil
					ID3D10RenderTargetView *targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
					ID3D10DepthStencilView *depthstencil = nullptr;

					_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depthstencil);

					if (depthstencil != nullptr)
					{
						if (depthstencil == _depthStencil)
						{
							_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, _depthStencilReplacement);
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
			for (const auto &it : _textures)
			{
				D3D10Texture *texture = static_cast<D3D10Texture *>(it.get());

				if (texture->DataSource == D3D10Texture::Source::DepthStencil)
				{
					texture->ChangeDataSource(D3D10Texture::Source::DepthStencil, this, _depthStencilTextureSRV, nullptr);
				}
			}

			return true;
		}
	}
}
