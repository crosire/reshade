#include "Log.hpp"
#include "D3D9Runtime.hpp"
#include "FX\ParserNodes.hpp"
#include "GUI.hpp"
#include "WindowWatcher.hpp"

#include <assert.h>
#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <nanovg_d3d9.h>
#include <unordered_set>
#include <boost\algorithm\string.hpp>

const D3DFORMAT D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));

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
			inline ULONG GetRefCount(IUnknown *object)
			{
				const ULONG ref = object->AddRef() - 1;

				object->Release();

				return ref;
			}

			struct D3D9Texture : public Texture
			{
				enum class Source
				{
					None,
					Memory,
					BackBuffer,
					DepthStencil
				};

				D3D9Texture() : DataSource(Source::None), TextureInterface(nullptr), SurfaceInterface(nullptr)
				{
				}
				~D3D9Texture()
				{
					SAFE_RELEASE(TextureInterface);
					SAFE_RELEASE(SurfaceInterface);
				}

				void ChangeDataSource(Source source, IDirect3DTexture9 *texture)
				{
					DataSource = source;

					if (TextureInterface == texture)
					{
						return;
					}

					SAFE_RELEASE(TextureInterface);
					SAFE_RELEASE(SurfaceInterface);

					if (texture != nullptr)
					{
						TextureInterface = texture;
						TextureInterface->AddRef();
						TextureInterface->GetSurfaceLevel(0, &SurfaceInterface);

						D3DSURFACE_DESC texdesc;
						SurfaceInterface->GetDesc(&texdesc);

						Width = texdesc.Width;
						Height = texdesc.Height;
						Format = Texture::PixelFormat::Unknown;
						Levels = TextureInterface->GetLevelCount();
					}
					else
					{
						Width = Height = Levels = 0;
						Format = Texture::PixelFormat::Unknown;
					}
				}

				Source DataSource;
				IDirect3DTexture9 *TextureInterface;
				IDirect3DSurface9 *SurfaceInterface;
			};
			struct D3D9Sampler
			{
				DWORD States[12];
				D3D9Texture *Texture;
			};
			struct D3D9Technique : public Technique
			{
				struct Pass
				{
					IDirect3DVertexShader9 *VS;
					IDirect3DPixelShader9 *PS;
					D3D9Sampler Samplers[16];
					DWORD SamplerCount;
					IDirect3DStateBlock9 *Stateblock;
					IDirect3DSurface9 *RT[8];
				};

				~D3D9Technique()
				{
					for (Pass &pass : Passes)
					{
						if (pass.Stateblock != nullptr) // TODO: Does it hold a reference to VS and PS?
						{
							pass.Stateblock->Release();
						}

						if (pass.VS != nullptr)
						{
							pass.VS->Release();
						}
						if (pass.PS != nullptr)
						{
							pass.PS->Release();
						}
					}
				}

				std::vector<Pass> Passes;
			};

			class D3D9EffectCompiler : private boost::noncopyable
			{
			public:
				D3D9EffectCompiler(const FX::NodeTree &ast, bool skipoptimization = false) : _ast(ast), _runtime(nullptr), _isFatal(false), _skipShaderOptimization(skipoptimization), _currentFunction(nullptr), _currentRegisterOffset(0)
				{
				}

				bool Compile(D3D9Runtime *runtime, std::string &errors)
				{
					_runtime = runtime;

					_isFatal = false;
					_errors.clear();

					_globalCode.clear();

					for (auto structure : _ast.Structs)
					{
						Visit(_globalCode, static_cast<FX::Nodes::Struct *>(structure));
					}

					for (auto uniform1 : _ast.Uniforms)
					{
						const auto uniform = static_cast<FX::Nodes::Variable *>(uniform1);

						if (uniform->Type.IsTexture())
						{
							VisitTexture(uniform);
						}
						else if (uniform->Type.IsSampler())
						{
							VisitSampler(uniform);
						}
						else if (uniform->Type.HasQualifier(FX::Nodes::Type::Qualifier::Uniform))
						{
							VisitUniform(uniform);
						}
						else
						{
							Visit(_globalCode, uniform);

							_globalCode += ";\n";
						}
					}

					for (auto function1 : _ast.Functions)
					{
						const auto function = static_cast<FX::Nodes::Function *>(function1);

						_currentFunction = function;

						Visit(_functions[function].SourceCode, function);
					}

					for (auto technique : _ast.Techniques)
					{
						VisitTechnique(static_cast<FX::Nodes::Technique *>(technique));
					}

					errors += _errors;

					return !_isFatal;
				}

				static inline bool IsPowerOf2(int x)
				{
					return ((x > 0) && ((x & (x - 1)) == 0));
				}
				static inline D3DBLEND LiteralToBlend(unsigned int value)
				{
					switch (value)
					{
						case FX::Nodes::Pass::States::ZERO:
							return D3DBLEND_ZERO;
						case FX::Nodes::Pass::States::ONE:
							return D3DBLEND_ONE;
					}

					return static_cast<D3DBLEND>(value);
				}
				static inline D3DSTENCILOP LiteralToStencilOp(unsigned int value)
				{
					if (value == FX::Nodes::Pass::States::ZERO)
					{
						return D3DSTENCILOP_ZERO;
					}

					return static_cast<D3DSTENCILOP>(value);
				}
				static D3DFORMAT LiteralToFormat(unsigned int value, Texture::PixelFormat &name)
				{
					switch (value)
					{
						case FX::Nodes::Variable::Properties::R8:
							name = Texture::PixelFormat::R8;
							return D3DFMT_A8R8G8B8;
						case FX::Nodes::Variable::Properties::R16F:
							name = Texture::PixelFormat::R16F;
							return D3DFMT_R16F;
						case FX::Nodes::Variable::Properties::R32F:
							name = Texture::PixelFormat::R32F;
							return D3DFMT_R32F;
						case FX::Nodes::Variable::Properties::RG8:
							name = Texture::PixelFormat::RG8;
							return D3DFMT_A8R8G8B8;
						case FX::Nodes::Variable::Properties::RG16:
							name = Texture::PixelFormat::RG16;
							return D3DFMT_G16R16;
						case FX::Nodes::Variable::Properties::RG16F:
							name = Texture::PixelFormat::RG16F;
							return D3DFMT_G16R16F;
						case FX::Nodes::Variable::Properties::RG32F:
							name = Texture::PixelFormat::RG32F;
							return D3DFMT_G32R32F;
						case FX::Nodes::Variable::Properties::RGBA8:
							name = Texture::PixelFormat::RGBA8;
							return D3DFMT_A8R8G8B8;  // D3DFMT_A8B8G8R8 appearently isn't supported by hardware very well
						case FX::Nodes::Variable::Properties::RGBA16:
							name = Texture::PixelFormat::RGBA16;
							return D3DFMT_A16B16G16R16;
						case FX::Nodes::Variable::Properties::RGBA16F:
							name = Texture::PixelFormat::RGBA16F;
							return D3DFMT_A16B16G16R16F;
						case FX::Nodes::Variable::Properties::RGBA32F:
							name = Texture::PixelFormat::RGBA32F;
							return D3DFMT_A32B32G32R32F;
						case FX::Nodes::Variable::Properties::DXT1:
							name = Texture::PixelFormat::DXT1;
							return D3DFMT_DXT1;
						case FX::Nodes::Variable::Properties::DXT3:
							name = Texture::PixelFormat::DXT3;
							return D3DFMT_DXT3;
						case FX::Nodes::Variable::Properties::DXT5:
							name = Texture::PixelFormat::DXT5;
							return D3DFMT_DXT5;
						case FX::Nodes::Variable::Properties::LATC1:
							name = Texture::PixelFormat::LATC1;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
						case FX::Nodes::Variable::Properties::LATC2:
							name = Texture::PixelFormat::LATC2;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
						default:
							name = Texture::PixelFormat::Unknown;
							return D3DFMT_UNKNOWN;
					}
				}
				static std::string ConvertSemantic(const std::string &semantic)
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
					else if (semantic == "VERTEXID")
					{
						return "TEXCOORD0";
					}

					return semantic;
				}

			private:
				void Error(const FX::Location &location, const char *message, ...)
				{
					char formatted[512];

					va_list args;
					va_start(args, message);
					vsprintf_s(formatted, message, args);
					va_end(args);

					_errors += location.Source + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): error: " + formatted + '\n';
					_isFatal = true;
				}
				void Warning(const FX::Location &location, const char *message, ...)
				{
					char formatted[512];

					va_list args;
					va_start(args, message);
					vsprintf_s(formatted, message, args);
					va_end(args);

					_errors += location.Source + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): warning: " + formatted + '\n';
				}

				void VisitType(std::string &source, const FX::Nodes::Type &type)
				{
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Static))
						source += "static ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Const))
						source += "const ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Volatile))
						source += "volatile ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Precise))
						source += "precise ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Linear))
						source += "linear ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::NoPerspective))
						source += "noperspective ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::Centroid))
						source += "centroid ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::NoInterpolation))
						source += "nointerpolation ";
					if (type.HasQualifier(FX::Nodes::Type::Qualifier::InOut))
						source += "inout ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::In))
						source += "in ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::Out))
						source += "out ";
					else if (type.HasQualifier(FX::Nodes::Type::Qualifier::Uniform))
						source += "uniform ";

					VisitTypeClass(source, type);
				}
				void VisitTypeClass(std::string &source, const FX::Nodes::Type &type)
				{
					switch (type.BaseClass)
					{
						case FX::Nodes::Type::Class::Void:
							source += "void";
							break;
						case FX::Nodes::Type::Class::Bool:
							source += "bool";
							break;
						case FX::Nodes::Type::Class::Int:
							source += "int";
							break;
						case FX::Nodes::Type::Class::Uint:
							source += "uint";
							break;
						case FX::Nodes::Type::Class::Float:
							source += "float";
							break;
						case FX::Nodes::Type::Class::Sampler2D:
							source += "__sampler2D";
							break;
						case FX::Nodes::Type::Class::Struct:
							VisitName(source, type.Definition);
							break;
					}

					if (type.IsMatrix())
					{
						source += std::to_string(type.Rows) + "x" + std::to_string(type.Cols);
					}
					else if (type.IsVector())
					{
						source += std::to_string(type.Rows);
					}
				}
				inline void VisitName(std::string &source, const FX::Nodes::Declaration *declaration)
				{
					source += boost::replace_all_copy(declaration->Namespace, "::", "_NS") + declaration->Name;
				}

				void Visit(std::string &output, const FX::Nodes::Statement *node)
				{
					if (node == nullptr)
					{
						return;
					}

					switch (node->NodeId)
					{
						case FX::Node::Id::Compound:
							Visit(output, static_cast<const FX::Nodes::Compound *>(node));
							break;
						case FX::Node::Id::DeclaratorList:
							Visit(output, static_cast<const FX::Nodes::DeclaratorList *>(node));
							break;
						case FX::Node::Id::ExpressionStatement:
							Visit(output, static_cast<const FX::Nodes::ExpressionStatement *>(node));
							break;
						case FX::Node::Id::If:
							Visit(output, static_cast<const FX::Nodes::If *>(node));
							break;
						case FX::Node::Id::Switch:
							Visit(output, static_cast<const FX::Nodes::Switch *>(node));
							break;
						case FX::Node::Id::For:
							Visit(output, static_cast<const FX::Nodes::For *>(node));
							break;
						case FX::Node::Id::While:
							Visit(output, static_cast<const FX::Nodes::While *>(node));
							break;
						case FX::Node::Id::Return:
							Visit(output, static_cast<const FX::Nodes::Return *>(node));
							break;
						case FX::Node::Id::Jump:
							Visit(output, static_cast<const FX::Nodes::Jump *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}
				void Visit(std::string &output, const FX::Nodes::Expression *node)
				{
					switch (node->NodeId)
					{
						case FX::Node::Id::LValue:
							Visit(output, static_cast<const FX::Nodes::LValue *>(node));
							break;
						case FX::Node::Id::Literal:
							Visit(output, static_cast<const FX::Nodes::Literal *>(node));
							break;
						case FX::Node::Id::Sequence:
							Visit(output, static_cast<const FX::Nodes::Sequence *>(node));
							break;
						case FX::Node::Id::Unary:
							Visit(output, static_cast<const FX::Nodes::Unary *>(node));
							break;
						case FX::Node::Id::Binary:
							Visit(output, static_cast<const FX::Nodes::Binary *>(node));
							break;
						case FX::Node::Id::Intrinsic:
							Visit(output, static_cast<const FX::Nodes::Intrinsic *>(node));
							break;
						case FX::Node::Id::Conditional:
							Visit(output, static_cast<const FX::Nodes::Conditional *>(node));
							break;
						case FX::Node::Id::Swizzle:
							Visit(output, static_cast<const FX::Nodes::Swizzle *>(node));
							break;
						case FX::Node::Id::FieldSelection:
							Visit(output, static_cast<const FX::Nodes::FieldSelection *>(node));
							break;
						case FX::Node::Id::Assignment:
							Visit(output, static_cast<const FX::Nodes::Assignment *>(node));
							break;
						case FX::Node::Id::Call:
							Visit(output, static_cast<const FX::Nodes::Call *>(node));
							break;
						case FX::Node::Id::Constructor:
							Visit(output, static_cast<const FX::Nodes::Constructor *>(node));
							break;
						case FX::Node::Id::InitializerList:
							Visit(output, static_cast<const FX::Nodes::InitializerList *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}

				void Visit(std::string &output, const FX::Nodes::Compound *node)
				{
					output += "{\n";

					for (auto statement : node->Statements)
					{
						Visit(output, statement);
					}

					output += "}\n";
				}
				void Visit(std::string &output, const FX::Nodes::DeclaratorList *node, bool singlestatement = false)
				{
					bool includetype = true;

					for (auto declarator : node->Declarators)
					{
						Visit(output, declarator, includetype);

						if (singlestatement)
						{
							output += ", ";

							includetype = false;
						}
						else
						{
							output += ";\n";
						}
					}

					if (singlestatement)
					{
						output.erase(output.end() - 2, output.end());

						output += ";\n";
					}
				}
				void Visit(std::string &output, const FX::Nodes::ExpressionStatement *node)
				{
					Visit(output, node->Expression);

					output += ";\n";
				}
				void Visit(std::string &output, const FX::Nodes::If *node)
				{
					for (auto &attribute : node->Attributes)
					{
						output += '[' + attribute + ']';
					}

					output += "if (";

					Visit(output, node->Condition);
					
					output += ")\n";

					if (node->StatementOnTrue != nullptr)
					{
						Visit(output, node->StatementOnTrue);
					}
					else
					{
						output += "\t;";
					}

					if (node->StatementOnFalse != nullptr)
					{
						output += "else\n";

						Visit(output, node->StatementOnFalse);
					}
				}
				void Visit(std::string &output, const FX::Nodes::Switch *node)
				{
					Warning(node->Location, "switch statements do not currently support fallthrough in Direct3D9!");

					output += "[unroll] do { ";
					
					VisitTypeClass(output, node->Test->Type);
						
					output += " __switch_condition = ";

					Visit(output, node->Test);

					output += ";\n";

					Visit(output, node->Cases[0]);

					for (size_t i = 1, count = node->Cases.size(); i < count; ++i)
					{
						output += "else ";

						Visit(output, node->Cases[i]);
					}

					output += "} while (false);\n";
				}
				void Visit(std::string &output, const FX::Nodes::Case *node)
				{
					output += "if (";

					for (auto label : node->Labels)
					{
						if (label == nullptr)
						{
							output += "true";
						}
						else
						{
							output += "__switch_condition == ";

							Visit(output, label);
						}

						output += " || ";
					}

					output.erase(output.end() - 4, output.end());

					output += ")";

					Visit(output, node->Statements);
				}
				void Visit(std::string &output, const FX::Nodes::For *node)
				{
					for (auto &attribute : node->Attributes)
					{
						output += '[' + attribute + ']';
					}

					output += "for (";

					if (node->Initialization != nullptr)
					{
						if (node->Initialization->NodeId == FX::Node::Id::DeclaratorList)
						{
							Visit(output, static_cast<FX::Nodes::DeclaratorList *>(node->Initialization), true);

							output.erase(output.end() - 2, output.end());
						}
						else
						{
							Visit(output, static_cast<FX::Nodes::ExpressionStatement *>(node->Initialization)->Expression);
						}
					}

					output += "; ";
										
					if (node->Condition != nullptr)
					{
						Visit(output, node->Condition);
					}

					output += "; ";

					if (node->Increment != nullptr)
					{
						Visit(output, node->Increment);
					}

					output += ")\n";

					if (node->Statements != nullptr)
					{
						Visit(output, node->Statements);
					}
					else
					{
						output += "\t;";
					}
				}
				void Visit(std::string &output, const FX::Nodes::While *node)
				{
					for (auto &attribute : node->Attributes)
					{
						output += '[' + attribute + ']';
					}

					if (node->DoWhile)
					{
						output += "do\n{\n";

						if (node->Statements != nullptr)
						{
							Visit(output, node->Statements);
						}

						output += "}\nwhile (";

						Visit(output, node->Condition);

						output += ");\n";
					}
					else
					{
						output += "while (";
						
						Visit(output, node->Condition);
						
						output += ")\n";

						if (node->Statements != nullptr)
						{
							Visit(output, node->Statements);
						}
						else
						{
							output += "\t;";
						}
					}
				}
				void Visit(std::string &output, const FX::Nodes::Return *node)
				{
					if (node->Discard)
					{
						output += "discard";
					}
					else
					{
						output += "return";

						if (node->Value != nullptr)
						{
							output += ' ';

							Visit(output, node->Value);
						}
					}

					output += ";\n";
				}
				void Visit(std::string &output, const FX::Nodes::Jump *node)
				{
					switch (node->Mode)
					{
						case FX::Nodes::Jump::Break:
							output += "break";
							break;
						case FX::Nodes::Jump::Continue:
							output += "continue";
							break;
					}

					output += ";\n";
				}

				void Visit(std::string &output, const FX::Nodes::LValue *node)
				{
					VisitName(output, node->Reference);

					if (node->Reference->Type.IsSampler() && (_samplers.find(node->Reference->Name) != _samplers.end()))
					{
						_functions.at(_currentFunction).SamplerDependencies.insert(node->Reference);
					}
				}
				void Visit(std::string &output, const FX::Nodes::Literal *node)
				{
					if (!node->Type.IsScalar())
					{
						VisitTypeClass(output, node->Type);
						
						output += '(';
					}

					for (unsigned int i = 0; i < node->Type.Rows * node->Type.Cols; ++i)
					{
						switch (node->Type.BaseClass)
						{
							case FX::Nodes::Type::Class::Bool:
								output += node->Value.Int[i] ? "true" : "false";
								break;
							case FX::Nodes::Type::Class::Int:
								output += std::to_string(node->Value.Int[i]);
								break;
							case FX::Nodes::Type::Class::Uint:
								output += std::to_string(node->Value.Uint[i]);
								break;
							case FX::Nodes::Type::Class::Float:
								output += std::to_string(node->Value.Float[i]) + "f";
								break;
						}

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					if (!node->Type.IsScalar())
					{
						output += ')';
					}
				}
				void Visit(std::string &output, const FX::Nodes::Sequence *node)
				{
					output += '(';

					for (auto expression : node->Expressions)
					{
						Visit(output, expression);

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					output += ')';
				}
				void Visit(std::string &output, const FX::Nodes::Unary *node)
				{
					std::string part1, part2;

					switch (node->Operator)
					{
						case FX::Nodes::Unary::Op::Negate:
							part1 = '-';
							break;
						case FX::Nodes::Unary::Op::BitwiseNot:
							part1 = "(4294967295 - ";
							part2 = ")";
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
							VisitTypeClass(part1, node->Type);
							part1 += '(';
							part2 = ')';
							break;
					}

					output += part1;
					Visit(output, node->Operand);
					output += part2;
				}
				void Visit(std::string &output, const FX::Nodes::Binary *node)
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
							part1 = "((";
							part2 = ") * exp2(";
							part3 = "))";
							break;
						case FX::Nodes::Binary::Op::RightShift:
							part1 = "floor((";
							part2 = ") / exp2(";
							part3 = "))";
							break;
						case FX::Nodes::Binary::Op::BitwiseAnd:
							if (node->Operands[1]->NodeId == FX::Node::Id::Literal && node->Operands[1]->Type.IsIntegral())
							{
								const unsigned int value = static_cast<const FX::Nodes::Literal *>(node->Operands[1])->Value.Uint[0];

								if (IsPowerOf2(value + 1))
								{
									output += "((" + std::to_string(value + 1) + ") * frac((";
									Visit(output, node->Operands[0]);
									output += ") / (" + std::to_string(value + 1) + ")))";
									return;
								}
								else if (IsPowerOf2(value))
								{
									output += "((((";
									Visit(output, node->Operands[0]);
									output += ") / (" + std::to_string(value) + ")) % 2) * " + std::to_string(value) + ")";
									return;
								}
							}
						case FX::Nodes::Binary::Op::BitwiseOr:
						case FX::Nodes::Binary::Op::BitwiseXor:
							Error(node->Location, "bitwise operations are not supported in Direct3D9");
							return;
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

					output += part1;
					Visit(output, node->Operands[0]);
					output += part2;
					Visit(output, node->Operands[1]);
					output += part3;
				}
				void Visit(std::string &output, const FX::Nodes::Intrinsic *node)
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
						case FX::Nodes::Intrinsic::Op::BitCastUint2Float:
						case FX::Nodes::Intrinsic::Op::BitCastFloat2Int:
						case FX::Nodes::Intrinsic::Op::BitCastFloat2Uint:
							Error(node->Location, "'asint', 'asuint' and 'asfloat' are not supported in Direct3D9");
							return;
						case FX::Nodes::Intrinsic::Op::Asin:
							part1 = "asin(";
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
							part1 = "tex2D((";
							part2 = ").s, ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DFetch:
							part1 = "tex2D((";
							part2 = ").s, float2(";
							part3 = "))";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DGather:
							if (node->Arguments[2]->NodeId == FX::Node::Id::Literal && node->Arguments[2]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[2])->Value.Int[0];

								output += "__tex2Dgather" + std::to_string(component) + "(";
								Visit(output, node->Arguments[0]);
								output += ", ";
								Visit(output, node->Arguments[1]);
								output += ")";
							}
							else
							{
								Error(node->Location, "texture gather component argument has to be constant");
							}
							return;
						case FX::Nodes::Intrinsic::Op::Tex2DGatherOffset:
							if (node->Arguments[3]->NodeId == FX::Node::Id::Literal && node->Arguments[3]->Type.IsIntegral())
							{
								const int component = static_cast<const FX::Nodes::Literal *>(node->Arguments[3])->Value.Int[0];

								output += "__tex2Dgather" + std::to_string(component) + "offset(";
								Visit(output, node->Arguments[0]);
								output += ", ";
								Visit(output, node->Arguments[1]);
								output += ", ";
								Visit(output, node->Arguments[2]);
								output += ")";
							}
							else
							{
								Error(node->Location, "texture gather component argument has to be constant");
							}
							return;
						case FX::Nodes::Intrinsic::Op::Tex2DGrad:
							part1 = "tex2Dgrad((";
							part2 = ").s, ";
							part3 = ", ";
							part4 = ", ";
							part5 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DLevel:
							part1 = "tex2Dlod((";
							part2 = ").s, ";
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
							part1 = "tex2Dproj((";
							part2 = ").s, ";
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

					output += part1;

					if (node->Arguments[0] != nullptr)
					{
						Visit(output, node->Arguments[0]);
					}

					output += part2;

					if (node->Arguments[1] != nullptr)
					{
						Visit(output, node->Arguments[1]);
					}

					output += part3;

					if (node->Arguments[2] != nullptr)
					{
						Visit(output, node->Arguments[2]);
					}

					output += part4;

					if (node->Arguments[3] != nullptr)
					{
						Visit(output, node->Arguments[3]);
					}

					output += part5;
				}
				void Visit(std::string &output, const FX::Nodes::Conditional *node)
				{
					output += '(';
					Visit(output, node->Condition);
					output += " ? ";
					Visit(output, node->ExpressionOnTrue);
					output += " : ";
					Visit(output, node->ExpressionOnFalse);
					output += ')';
				}
				void Visit(std::string &output, const FX::Nodes::Swizzle *node)
				{
					Visit(output, node->Operand);

					output += '.';

					if (node->Operand->Type.IsMatrix())
					{
						const char swizzle[16][5] = { "_m00", "_m01", "_m02", "_m03", "_m10", "_m11", "_m12", "_m13", "_m20", "_m21", "_m22", "_m23", "_m30", "_m31", "_m32", "_m33" };

						for (int i = 0; i < 4 && node->Mask[i] >= 0; ++i)
						{
							output += swizzle[node->Mask[i]];
						}
					}
					else
					{
						const char swizzle[4] = { 'x', 'y', 'z', 'w' };

						for (int i = 0; i < 4 && node->Mask[i] >= 0; ++i)
						{
							output += swizzle[node->Mask[i]];
						}
					}
				}
				void Visit(std::string &output, const FX::Nodes::FieldSelection *node)
				{
					output += '(';

					Visit(output, node->Operand);

					output += '.';

					VisitName(output, node->Field);

					output += ')';
				}
				void Visit(std::string &output, const FX::Nodes::Assignment *node)
				{
					std::string part1, part2, part3;

					switch (node->Operator)
					{
						case FX::Nodes::Assignment::Op::None:
							part2 = " = ";
							break;
						case FX::Nodes::Assignment::Op::Add:
							part2 = " += ";
							break;
						case FX::Nodes::Assignment::Op::Subtract:
							part2 = " -= ";
							break;
						case FX::Nodes::Assignment::Op::Multiply:
							part2 = " *= ";
							break;
						case FX::Nodes::Assignment::Op::Divide:
							part2 = " /= ";
							break;
						case FX::Nodes::Assignment::Op::Modulo:
							part2 = " %= ";
							break;
						case FX::Nodes::Assignment::Op::LeftShift:
							part1 = "((";
							part2 = ") *= pow(2, ";
							part3 = "))";
							break;
						case FX::Nodes::Assignment::Op::RightShift:
							part1 = "((";
							part2 = ") /= pow(2, ";
							part3 = "))";
							break;
						case FX::Nodes::Assignment::Op::BitwiseAnd:
						case FX::Nodes::Assignment::Op::BitwiseOr:
						case FX::Nodes::Assignment::Op::BitwiseXor:
							Error(node->Location, "bitwise operations are not supported in Direct3D9");
							return;
					}

					output += '(';

					output += part1;
					Visit(output, node->Left);
					output += part2;
					Visit(output, node->Right);
					output += part3;

					output += ')';
				}
				void Visit(std::string &output, const FX::Nodes::Call *node)
				{
					VisitName(output, node->Callee);

					output += '(';

					if (!node->Arguments.empty())
					{
						for (auto argument : node->Arguments)
						{
							Visit(output, argument);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';

					auto &info = _functions.at(_currentFunction);
					auto &infoCallee = _functions.at(node->Callee);
					
					info.SamplerDependencies.insert(infoCallee.SamplerDependencies.begin(), infoCallee.SamplerDependencies.end());

					for (auto dependency : infoCallee.FunctionDependencies)
					{
						if (std::find(info.FunctionDependencies.begin(), info.FunctionDependencies.end(), dependency) == info.FunctionDependencies.end())
						{
							info.FunctionDependencies.push_back(dependency);
						}
					}

					if (std::find(info.FunctionDependencies.begin(), info.FunctionDependencies.end(), node->Callee) == info.FunctionDependencies.end())
					{
						info.FunctionDependencies.push_back(node->Callee);
					}
				}
				void Visit(std::string &output, const FX::Nodes::Constructor *node)
				{
					VisitTypeClass(output, node->Type);

					output += '(';

					if (!node->Arguments.empty())
					{
						for (auto argument : node->Arguments)
						{
							Visit(output, argument);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';
				}
				void Visit(std::string &output, const FX::Nodes::InitializerList *node)
				{
					output += "{ ";

					for (auto expression : node->Values)
					{
						Visit(output, expression);

						output += ", ";
					}

					output += " }";
				}

				void Visit(std::string &output, const FX::Nodes::Struct *node)
				{
					output += "struct ";
					
					VisitName(output, node);

					output += "\n{\n";

					if (!node->Fields.empty())
					{
						for (auto field : node->Fields)
						{
							Visit(output, field);

							output += ";\n";
						}
					}
					else
					{
						output += "float _dummy;\n";
					}

					output += "};\n";
				}
				void Visit(std::string &output, const FX::Nodes::Variable *node, bool includetype = true, bool includesemantic = true)
				{
					if (includetype)
					{
						VisitType(output, node->Type);

						output += ' ';
					}

					VisitName(output, node);

					if (node->Type.IsArray())
					{
						output += '[' + ((node->Type.ArrayLength > 0) ? std::to_string(node->Type.ArrayLength) : "") + ']';
					}

					if (includesemantic && !node->Semantic.empty())
					{
						output += " : " + ConvertSemantic(node->Semantic);
					}

					if (node->Initializer != nullptr)
					{
						output += " = ";

						Visit(output, node->Initializer);
					}
				}
				void Visit(std::string &output, const FX::Nodes::Function *node)
				{
					VisitTypeClass(output, node->ReturnType);
					
					output += ' ';

					VisitName(output, node);

					output += '(';

					if (!node->Parameters.empty())
					{
						for (auto parameter : node->Parameters)
						{
							Visit(output, parameter, true, false);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ")\n";

					Visit(output, node->Definition);
				}

				template <typename T>
				void VisitAnnotation(const std::vector<FX::Nodes::Annotation> &annotations, T &object)
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
				void VisitTexture(const FX::Nodes::Variable *node)
				{
					D3D9Texture *const obj = new D3D9Texture();
					obj->Name = node->Name;
					UINT width = obj->Width = node->Properties.Width;
					UINT height = obj->Height = node->Properties.Height;
					UINT levels = obj->Levels = node->Properties.MipLevels;
					const D3DFORMAT format = LiteralToFormat(node->Properties.Format, obj->Format);

					VisitAnnotation(node->Annotations, *obj);

					if (node->Semantic == "COLOR" || node->Semantic == "SV_TARGET")
					{
						if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
						{
							Warning(node->Location, "texture properties on backbuffer textures are ignored");
						}

						obj->ChangeDataSource(D3D9Texture::Source::BackBuffer, _runtime->_backbufferTexture);
					}
					else if (node->Semantic == "DEPTH" || node->Semantic == "SV_DEPTH")
					{
						if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
						{
							Warning(node->Location, "texture properties on depthbuffer textures are ignored");
						}

						obj->ChangeDataSource(D3D9Texture::Source::DepthStencil, _runtime->_depthStencilTexture);
					}
					else
					{
						obj->DataSource = D3D9Texture::Source::Memory;

						DWORD usage = 0;
						D3DDEVICE_CREATION_PARAMETERS cp;
						_runtime->_device->GetCreationParameters(&cp);

						if (levels > 1)
						{
							if (_runtime->_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, format) == D3D_OK)
							{
								usage |= D3DUSAGE_AUTOGENMIPMAP;
								levels = 0;
							}
							else
							{
								Warning(node->Location, "autogenerated miplevels are not supported for this format");
							}
						}
						else if (levels == 0)
						{
							Warning(node->Location, "a texture cannot have 0 miplevels, changed it to 1");

							levels = 1;
						}
					
						HRESULT hr = _runtime->_d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format);

						if (SUCCEEDED(hr))
						{
							usage |= D3DUSAGE_RENDERTARGET;
						}

						hr = _runtime->_device->CreateTexture(width, height, levels, usage, format, D3DPOOL_DEFAULT, &obj->TextureInterface, nullptr);

						if (FAILED(hr))
						{
							Error(node->Location, "internal texture creation failed with '%u'!", hr);
							return;
						}

						hr = obj->TextureInterface->GetSurfaceLevel(0, &obj->SurfaceInterface);

						assert(SUCCEEDED(hr));
					}

					_runtime->AddTexture(obj);
				}
				void VisitSampler(const FX::Nodes::Variable *node)
				{
					if (node->Properties.Texture == nullptr)
					{
						Error(node->Location, "sampler '%s' is missing required 'Texture' property", node->Name);
						return;
					}

					D3D9Texture *const texture = static_cast<D3D9Texture *>(_runtime->GetTexture(node->Properties.Texture->Name));

					if (texture == nullptr)
					{
						_isFatal = true;
						return;
					}

					D3D9Sampler sampler;
					sampler.Texture = texture;
					sampler.States[D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressU);
					sampler.States[D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressV);
					sampler.States[D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressW);
					sampler.States[D3DSAMP_BORDERCOLOR] = 0;
					sampler.States[D3DSAMP_MINFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MinFilter);
					sampler.States[D3DSAMP_MAGFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MagFilter);
					sampler.States[D3DSAMP_MIPFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MipFilter);
					sampler.States[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&node->Properties.MipLODBias);
					sampler.States[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, node->Properties.MinLOD));
					sampler.States[D3DSAMP_MAXANISOTROPY] = node->Properties.MaxAnisotropy;
					sampler.States[D3DSAMP_SRGBTEXTURE] = node->Properties.SRGBTexture;

					_samplers[node->Name] = sampler;
				}
				void VisitUniform(const FX::Nodes::Variable *node)
				{
					VisitType(_globalCode, node->Type);
					
					_globalCode += ' ';

					VisitName(_globalCode, node);

					if (node->Type.IsArray())
					{
						_globalCode += '[' + ((node->Type.ArrayLength > 0) ? std::to_string(node->Type.ArrayLength) : "") + ']';
					}

					_globalCode += " : register(c" + std::to_string(_currentRegisterOffset / 4) + ");\n";

					Uniform *const obj = new Uniform();
					obj->Name = node->Name;
					obj->Rows = node->Type.Rows;
					obj->Columns = node->Type.Cols;
					obj->Elements = node->Type.ArrayLength;
					obj->StorageSize = obj->Rows * obj->Columns * std::max(1u, obj->Elements);

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

					const UINT regsize = static_cast<UINT>(static_cast<float>(obj->StorageSize) / sizeof(float));
					const UINT regalignment = 4 - (regsize % 4);

					obj->StorageOffset = _currentRegisterOffset * sizeof(float);
					_currentRegisterOffset += regsize + regalignment;

					VisitAnnotation(node->Annotations, *obj);

					if (_currentRegisterOffset * sizeof(float) >= _runtime->GetConstantStorageSize())
					{
						_runtime->EnlargeConstantStorage();
					}

					_runtime->_constantRegisterCount = _currentRegisterOffset / 4;

					if (node->Initializer != nullptr && node->Initializer->NodeId == FX::Node::Id::Literal)
					{
						CopyMemory(_runtime->GetConstantStorage() + obj->StorageOffset, &static_cast<const FX::Nodes::Literal *>(node->Initializer)->Value, obj->StorageSize);
					}
					else
					{
						ZeroMemory(_runtime->GetConstantStorage() + obj->StorageOffset, obj->StorageSize);
					}

					_runtime->AddConstant(obj);
				}
				void VisitTechnique(const FX::Nodes::Technique *node)
				{
					D3D9Technique *const obj = new D3D9Technique();
					obj->Name = node->Name;
					obj->PassCount = static_cast<unsigned int>(node->Passes.size());

					VisitAnnotation(node->Annotations, *obj);

					for (auto pass : node->Passes)
					{
						VisitTechniquePass(pass, obj->Passes);
					}

					_runtime->AddTechnique(obj);
				}
				void VisitTechniquePass(const FX::Nodes::Pass *node, std::vector<D3D9Technique::Pass> &passes)
				{
					D3D9Technique::Pass pass;
					ZeroMemory(&pass, sizeof(D3D9Technique::Pass));
					pass.RT[0] = _runtime->_backbufferResolved;

					std::string samplers;
					const char shaderTypes[2][3] = { "vs", "ps" };
					const FX::Nodes::Function *shaderFunctions[2] = { node->States.VertexShader, node->States.PixelShader };

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] == nullptr)
						{
							continue;
						}

						for (auto sampler : _functions.at(shaderFunctions[i]).SamplerDependencies)
						{
							pass.Samplers[pass.SamplerCount] = _samplers.at(sampler->Name);
							const auto *const texture = sampler->Properties.Texture;

							samplers += "sampler2D __Sampler";
							VisitName(samplers, sampler);
							samplers += " : register(s" + std::to_string(pass.SamplerCount++) + ");\n";
							samplers += "static const __sampler2D ";
							VisitName(samplers, sampler);
							samplers += " = { __Sampler";
							VisitName(samplers, sampler);

							if (texture->Semantic == "COLOR" || texture->Semantic == "SV_TARGET" || texture->Semantic == "DEPTH" || texture->Semantic == "SV_DEPTH")
							{
								samplers += ", float2(" + std::to_string(1.0f / _runtime->GetBufferWidth()) + ", " + std::to_string(1.0f / _runtime->GetBufferHeight()) + ")";
							}
							else
							{
								samplers += ", float2(" + std::to_string(1.0f / texture->Properties.Width) + ", " + std::to_string(1.0f / texture->Properties.Height) + ")";
							}

							samplers += " };\n";

							if (pass.SamplerCount == 16)
							{
								Error(node->Location, "maximum sampler count of 16 reached in pass '%s'", node->Name.c_str());
								return;
							}
						}
					}

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] != nullptr)
						{
							VisitTechniquePassShader(shaderFunctions[i], shaderTypes[i], samplers, pass);
						}
					}

					IDirect3DDevice9 *const device = _runtime->_device;
				
					const HRESULT hr = device->BeginStateBlock();

					if (FAILED(hr))
					{
						Error(node->Location, "internal pass stateblock creation failed with '%u'!", hr);
						return;
					}

					device->SetVertexShader(pass.VS);
					device->SetPixelShader(pass.PS);

					device->SetRenderState(D3DRS_ZENABLE, node->States.DepthEnable);
					device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
					device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
					device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
					device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
					device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
					device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
					device->SetRenderState(D3DRS_SRCBLEND, LiteralToBlend(node->States.SrcBlend));
					device->SetRenderState(D3DRS_DESTBLEND, LiteralToBlend(node->States.DestBlend));
					device->SetRenderState(D3DRS_ZFUNC, static_cast<D3DCMPFUNC>(node->States.DepthFunc));
					device->SetRenderState(D3DRS_ALPHAREF, 0);
					device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
					device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
					device->SetRenderState(D3DRS_FOGSTART, 0);
					device->SetRenderState(D3DRS_FOGEND, 1);
					device->SetRenderState(D3DRS_FOGDENSITY, 1);
					device->SetRenderState(D3DRS_ALPHABLENDENABLE, node->States.BlendEnable);
					device->SetRenderState(D3DRS_DEPTHBIAS, 0);
					device->SetRenderState(D3DRS_STENCILENABLE, node->States.StencilEnable);
					device->SetRenderState(D3DRS_STENCILPASS, LiteralToStencilOp(node->States.StencilOpPass));
					device->SetRenderState(D3DRS_STENCILFAIL, LiteralToStencilOp(node->States.StencilOpFail));
					device->SetRenderState(D3DRS_STENCILZFAIL, LiteralToStencilOp(node->States.StencilOpDepthFail));
					device->SetRenderState(D3DRS_STENCILFUNC, static_cast<D3DCMPFUNC>(node->States.StencilFunc));
					device->SetRenderState(D3DRS_STENCILREF, node->States.StencilRef);
					device->SetRenderState(D3DRS_STENCILMASK, node->States.StencilReadMask);
					device->SetRenderState(D3DRS_STENCILWRITEMASK, node->States.StencilWriteMask);
					device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
					device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
					device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
					device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
					device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
					device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
					device->SetRenderState(D3DRS_COLORWRITEENABLE, node->States.RenderTargetWriteMask);
					device->SetRenderState(D3DRS_BLENDOP, static_cast<D3DBLENDOP>(node->States.BlendOp));
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
					device->SetRenderState(D3DRS_SRGBWRITEENABLE, node->States.SRGBWriteEnable);
					device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
					device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
					device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
					device->SetRenderState(D3DRS_BLENDOPALPHA, static_cast<D3DBLENDOP>(node->States.BlendOpAlpha));
					device->SetRenderState(D3DRS_FOGENABLE, FALSE );
					device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
					device->SetRenderState(D3DRS_LIGHTING, FALSE);

					device->EndStateBlock(&pass.Stateblock);

					D3DCAPS9 caps;
					device->GetDeviceCaps(&caps);

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->States.RenderTargets[i] == nullptr)
						{
							continue;
						}

						if (i > caps.NumSimultaneousRTs)
						{
							Warning(node->Location, "device only supports %u simultaneous render targets, but pass '%s' uses more, which are ignored", caps.NumSimultaneousRTs, node->Name.c_str());
							break;
						}

						D3D9Texture *const texture = static_cast<D3D9Texture *>(_runtime->GetTexture(node->States.RenderTargets[i]->Name));

						if (texture == nullptr)
						{
							_isFatal = true;
							return;
						}

						pass.RT[i] = texture->SurfaceInterface;
					}

					passes.push_back(std::move(pass));
				}
				void VisitTechniquePassShader(const FX::Nodes::Function *node, const std::string &shadertype, const std::string &samplers, D3D9Technique::Pass &pass)
				{
					std::string source =
						"struct __sampler2D { sampler2D s; float2 pixelsize; };\n"
						"float4 __tex2Dgather0(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).r, tex2Dlod(s.s, float4(c, 0, 0)).r); }\n"
						"float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).g, tex2Dlod(s.s, float4(c, 0, 0)).g); }\n"
						"float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).b, tex2Dlod(s.s, float4(c, 0, 0)).b); }\n"
						"float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4(tex2Dlod(s.s, float4(c + float2(0, 1) * s.pixelsize, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 1) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c + float2(1, 0) * s.pixelsize.xy, 0, 0)).a, tex2Dlod(s.s, float4(c, 0, 0)).a); }\n"
						"float4 __tex2Dgather0offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather0(s, c + offset * s.pixelsize); }\n"
						"float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather1(s, c + offset * s.pixelsize); }\n"
						"float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather2(s, c + offset * s.pixelsize); }\n"
						"float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return __tex2Dgather3(s, c + offset * s.pixelsize); }\n"
						"float4 __tex2Dlodoffset(__sampler2D s, float4 c, int2 offset) { return tex2Dlod(s.s, c + float4(offset * s.pixelsize, 0, 0)); }\n"
						"float4 __tex2Doffset(__sampler2D s, float2 c, int2 offset) { return tex2D(s.s, c + offset * s.pixelsize); }\n"
						"int2 __tex2Dsize(__sampler2D s, int lod) { return int2(1 / s.pixelsize) / exp2(lod); }\n";

					if (shadertype == "vs")
					{
						source += "uniform float2 __TEXEL_SIZE__ : register(c255);\n";
					}
					if (shadertype == "ps")
					{
						source += "#define POSITION VPOS\n";
					}

					source += samplers;
					source += _globalCode;

					for (auto dependency : _functions.at(node).FunctionDependencies)
					{
						source += _functions.at(dependency).SourceCode;
					}

					source += _functions.at(node).SourceCode;

					std::string positionVariable, initialization;
					FX::Nodes::Type returnType = node->ReturnType;

					if (node->ReturnType.IsStruct())
					{
						for (auto field : node->ReturnType.Definition->Fields)
						{
							if (field->Semantic == "SV_POSITION" || field->Semantic == "POSITION")
							{
								positionVariable = "_return." + field->Name;
								break;
							}
							else if ((boost::starts_with(field->Semantic, "SV_TARGET") || boost::starts_with(field->Semantic, "COLOR")) && field->Type.Rows != 4)
							{
								Error(node->Location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
								return;
							}
						}
					}
					else
					{
						if (node->ReturnSemantic == "SV_POSITION" || node->ReturnSemantic == "POSITION")
						{
							positionVariable = "_return";
						}
						else if (boost::starts_with(node->ReturnSemantic, "SV_TARGET") || boost::starts_with(node->ReturnSemantic, "COLOR"))
						{
							returnType.Rows = 4;
						}
					}

					VisitTypeClass(source, returnType);
					
					source += " __main(";
				
					if (!node->Parameters.empty())
					{
						for (auto parameter : node->Parameters)
						{
							FX::Nodes::Type parameterType = parameter->Type;

							if (parameter->Type.HasQualifier(FX::Nodes::Type::Out))
							{
								if (parameterType.IsStruct())
								{
									for (auto field : parameterType.Definition->Fields)
									{
										if (field->Semantic == "SV_POSITION" || field->Semantic == "POSITION")
										{
											positionVariable = parameter->Name + '.' + field->Name;
											break;
										}
										else if ((boost::starts_with(field->Semantic, "SV_TARGET") || boost::starts_with(field->Semantic, "COLOR")) && field->Type.Rows != 4)
										{
											Error(node->Location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
											return;
										}
									}
								}
								else
								{
									if (parameter->Semantic == "SV_POSITION" || parameter->Semantic == "POSITION")
									{
										positionVariable = parameter->Name;
									}
									else if (boost::starts_with(parameter->Semantic, "SV_TARGET") || boost::starts_with(parameter->Semantic, "COLOR"))
									{
										parameterType.Rows = 4;

										initialization += parameter->Name + " = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
									}
								}
							}

							VisitType(source, parameterType);
						
							source += ' ' + parameter->Name;

							if (parameterType.IsArray())
							{
								source += '[' + ((parameterType.ArrayLength >= 1) ? std::to_string(parameterType.ArrayLength) : "") + ']';
							}

							if (!parameter->Semantic.empty())
							{
								source += " : " + ConvertSemantic(parameter->Semantic);
							}

							source += ", ";
						}

						source.erase(source.end() - 2, source.end());
					}

					source += ')';

					if (!node->ReturnSemantic.empty())
					{
						source += " : " + ConvertSemantic(node->ReturnSemantic);
					}

					source += "\n{\n";
					source += initialization;

					if (!node->ReturnType.IsVoid())
					{
						VisitTypeClass(source, returnType);
						
						source += " _return = ";
					}

					if (node->ReturnType.Rows != returnType.Rows)
					{
						source += "float4(";
					}

					VisitName(source, node);

					source += '(';

					if (!node->Parameters.empty())
					{
						for (auto parameter : node->Parameters)
						{
							source += parameter->Name;

							if (boost::starts_with(parameter->Semantic, "SV_TARGET") || boost::starts_with(parameter->Semantic, "COLOR"))
							{
								source += '.';

								const char swizzle[] = { 'x', 'y', 'z', 'w' };

								for (unsigned int i = 0; i < parameter->Type.Rows; ++i)
								{
									source += swizzle[i];
								}
							}

							source += ", ";
						}

						source.erase(source.end() - 2, source.end());
					}

					source += ')';

					if (node->ReturnType.Rows != returnType.Rows)
					{
						for (unsigned int i = 0; i < 4 - node->ReturnType.Rows; ++i)
						{
							source += ", 0.0f";
						}

						source += ')';
					}

					source += ";\n";
				
					if (shadertype == "vs")
					{
						source += positionVariable + ".xy += __TEXEL_SIZE__ * " + positionVariable + ".ww;\n";
					}

					if (!node->ReturnType.IsVoid())
					{
						source += "return _return;\n";
					}

					source += "}\n";

					LOG(TRACE) << "> Compiling shader '" << node->Name << "':\n\n" << source.c_str() << "\n";

					UINT flags = 0;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (_skipShaderOptimization)
					{
						flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
					}

					HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, "__main", (shadertype + "_3_0").c_str(), flags, 0, &compiled, &errors);

					if (errors != nullptr)
					{
						_errors.append(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

						errors->Release();
					}

					if (FAILED(hr))
					{
						_isFatal = true;
						return;
					}

					if (shadertype == "vs")
					{
						hr = _runtime->_device->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.VS);
					}
					else if (shadertype == "ps")
					{
						hr = _runtime->_device->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.PS);
					}

					compiled->Release();

					if (FAILED(hr))
					{
						Error(node->Location, "internal shader creation failed with '%u'!", hr);
						return;
					}
				}

			private:
				struct Function
				{
					std::string SourceCode;
					std::unordered_set<const FX::Nodes::Variable *> SamplerDependencies;
					std::vector<const FX::Nodes::Function *> FunctionDependencies;
				};

				D3D9Runtime *_runtime;
				const FX::NodeTree &_ast;
				bool _isFatal, _skipShaderOptimization;
				std::string _errors;
				std::string _globalCode;
				unsigned int _currentRegisterOffset;
				const FX::Nodes::Function *_currentFunction;
				std::unordered_map<std::string, D3D9Sampler> _samplers;
				std::unordered_map<const FX::Nodes::Function *, Function> _functions;
			};
		}

		// ---------------------------------------------------------------------------------------------------

		D3D9Runtime::D3D9Runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : _device(device), _swapchain(swapchain), _d3d(nullptr), _stateBlock(nullptr), _multisamplingEnabled(false), _backbufferFormat(D3DFMT_UNKNOWN), _backbuffer(nullptr), _backbufferResolved(nullptr), _backbufferTexture(nullptr), _backbufferTextureSurface(nullptr), _depthStencil(nullptr), _depthStencilReplacement(nullptr), _depthStencilTexture(nullptr), _defaultDepthStencil(nullptr), _effectTriangleBuffer(nullptr), _effectTriangleLayout(nullptr), _constantRegisterCount(0)
		{
			assert(_device != nullptr);
			assert(_swapchain != nullptr);

			_device->AddRef();
			_device->GetDirect3D(&_d3d);
			_swapchain->AddRef();

			assert(_d3d != nullptr);

			D3DCAPS9 caps;
			_device->GetDeviceCaps(&caps);

			D3DDEVICE_CREATION_PARAMETERS params;
			_device->GetCreationParameters(&params);

			D3DADAPTER_IDENTIFIER9 identifier;
			_d3d->GetAdapterIdentifier(params.AdapterOrdinal, 0, &identifier);

			_vendorId = identifier.VendorId;
			_deviceId = identifier.DeviceId;
			_rendererId = D3D_FEATURE_LEVEL_9_3;
			_behaviorFlags = params.BehaviorFlags;
			_numSimultaneousRTs = std::min(caps.NumSimultaneousRTs, static_cast<DWORD>(8));
		}
		D3D9Runtime::~D3D9Runtime()
		{
			_device->Release();
			_swapchain->Release();
			_d3d->Release();
		}

		bool D3D9Runtime::OnInit(const D3DPRESENT_PARAMETERS &pp)
		{
			_width = pp.BackBufferWidth;
			_height = pp.BackBufferHeight;
			_backbufferFormat = pp.BackBufferFormat;
			_multisamplingEnabled = pp.MultiSampleType != D3DMULTISAMPLE_NONE;
			_window.reset(new WindowWatcher(pp.hDeviceWindow));

			#pragma region Get backbuffer surface
			HRESULT hr = _swapchain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer);

			assert(SUCCEEDED(hr));

			if (pp.MultiSampleType != D3DMULTISAMPLE_NONE || (pp.BackBufferFormat == D3DFMT_X8R8G8B8 || pp.BackBufferFormat == D3DFMT_X8B8G8R8))
			{
				switch (pp.BackBufferFormat)
				{
					case D3DFMT_X8R8G8B8:
						_backbufferFormat = D3DFMT_A8R8G8B8;
						break;
					case D3DFMT_X8B8G8R8:
						_backbufferFormat = D3DFMT_A8B8G8R8;
						break;
				}

				hr = _device->CreateRenderTarget(_width, _height, _backbufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &_backbufferResolved, nullptr);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer resolve texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_backbuffer);

					return false;
				}
			}
			else
			{
				_backbufferResolved = _backbuffer;
				_backbufferResolved->AddRef();
			}
			#pragma endregion

			#pragma region Create backbuffer shader texture
			hr = _device->CreateTexture(_width, _height, 1, D3DUSAGE_RENDERTARGET, _backbufferFormat, D3DPOOL_DEFAULT, &_backbufferTexture, nullptr);

			if (SUCCEEDED(hr))
			{
				_backbufferTexture->GetSurfaceLevel(0, &_backbufferTextureSurface);
			}
			else
			{
				LOG(TRACE) << "Failed to create backbuffer texture! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);

				return false;
			}
			#pragma endregion

			#pragma region Create default depthstencil surface
			hr = _device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_defaultDepthStencil, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSurface);

				return nullptr;
			}
			#pragma endregion

			#pragma region Create effect stateblock and objects
			hr = _device->CreateStateBlock(D3DSBT_ALL, &_stateBlock);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create stateblock! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSurface);
				SAFE_RELEASE(_defaultDepthStencil);

				return false;
			}

			hr = _device->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effectTriangleBuffer, nullptr);

			if (SUCCEEDED(hr))
			{
				float *data = nullptr;

				hr = _effectTriangleBuffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

				assert(SUCCEEDED(hr));

				for (UINT i = 0; i < 3; ++i)
				{
					data[i] = static_cast<float>(i);
				}

				_effectTriangleBuffer->Unlock();
			}
			else
			{
				LOG(TRACE) << "Failed to create effect vertexbuffer! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSurface);
				SAFE_RELEASE(_defaultDepthStencil);
				SAFE_RELEASE(_stateBlock);

				return false;
			}

			const D3DVERTEXELEMENT9 declaration[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};

			hr = _device->CreateVertexDeclaration(declaration, &_effectTriangleLayout);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create effect vertex declaration! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbufferResolved);
				SAFE_RELEASE(_backbufferTexture);
				SAFE_RELEASE(_backbufferTextureSurface);
				SAFE_RELEASE(_defaultDepthStencil);
				SAFE_RELEASE(_stateBlock);
				SAFE_RELEASE(_effectTriangleBuffer);

				return false;
			}
			#pragma endregion

			_gui.reset(new GUI(this, nvgCreateD3D9(_device, 0)));

			return Runtime::OnInit();
		}
		void D3D9Runtime::OnReset()
		{
			if (!_isInitialized)
			{
				return;
			}

			Runtime::OnReset();

			// Destroy NanoVG
			NVGcontext *const nvg = _gui->GetContext();

			_gui.reset();

			nvgDeleteD3D9(nvg);

			// Destroy resources
			SAFE_RELEASE(_stateBlock);

			SAFE_RELEASE(_backbuffer);
			SAFE_RELEASE(_backbufferResolved);
			SAFE_RELEASE(_backbufferTexture);
			SAFE_RELEASE(_backbufferTextureSurface);

			SAFE_RELEASE(_depthStencil);
			SAFE_RELEASE(_depthStencilReplacement);
			SAFE_RELEASE(_depthStencilTexture);

			SAFE_RELEASE(_defaultDepthStencil);

			SAFE_RELEASE(_effectTriangleBuffer);
			SAFE_RELEASE(_effectTriangleLayout);

			// Clearing depth source table
			for (auto &it : _depthSourceTable)
			{
				LOG(TRACE) << "Removing depthstencil " << it.first << " from list of possible depth candidates ...";

				it.first->Release();
			}

			_depthSourceTable.clear();
		}
		void D3D9Runtime::OnPresent()
		{
			if (!_isInitialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}

			DetectDepthSource();

			// Begin post processing
			HRESULT hr = _device->BeginScene();

			if (FAILED(hr))
			{
				return;
			}

			// Capture device state
			_stateBlock->Capture();

			BOOL softwareRenderingEnabled;
			IDirect3DSurface9 *stateblockTargets[8] = { nullptr };
			IDirect3DSurface9 *stateblockDepthStencil = nullptr;

			for (DWORD target = 0; target < _numSimultaneousRTs; ++target)
			{
				_device->GetRenderTarget(target, &stateblockTargets[target]);
			}

			_device->GetDepthStencilSurface(&stateblockDepthStencil);

			if ((_behaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
			{
				softwareRenderingEnabled = _device->GetSoftwareVertexProcessing();

				_device->SetSoftwareVertexProcessing(FALSE);
			}

			// Resolve backbuffer
			if (_backbufferResolved != _backbuffer)
			{
				_device->StretchRect(_backbuffer, nullptr, _backbufferResolved, nullptr, D3DTEXF_NONE);
			}

			// Apply post processing
			OnApplyEffect();

			// Reset rendertarget
			_device->SetRenderTarget(0, _backbufferResolved);
			_device->SetDepthStencilSurface(_defaultDepthStencil);
			_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

			// Apply presenting
			Runtime::OnPresent();

			// Copy to backbuffer
			if (_backbufferResolved != _backbuffer)
			{
				_device->StretchRect(_backbufferResolved, nullptr, _backbuffer, nullptr, D3DTEXF_NONE);
			}

			// Apply previous device state
			_stateBlock->Apply();

			for (DWORD target = 0; target < _numSimultaneousRTs; ++target)
			{
				_device->SetRenderTarget(target, stateblockTargets[target]);

				SAFE_RELEASE(stateblockTargets[target]);
			}
			
			_device->SetDepthStencilSurface(stateblockDepthStencil);

			SAFE_RELEASE(stateblockDepthStencil);

			if ((_behaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
			{
				_device->SetSoftwareVertexProcessing(softwareRenderingEnabled);
			}

			// End post processing
			_device->EndScene();
		}
		void D3D9Runtime::OnDrawCall(D3DPRIMITIVETYPE type, UINT vertices)
		{
			switch (type)
			{
				case D3DPT_LINELIST:
					vertices *= 2;
					break;
				case D3DPT_LINESTRIP:
					vertices += 1;
					break;
				case D3DPT_TRIANGLELIST:
					vertices *= 3;
					break;
				case D3DPT_TRIANGLESTRIP:
				case D3DPT_TRIANGLEFAN:
					vertices += 2;
					break;
			}

			Runtime::OnDrawCall(vertices);

			IDirect3DSurface9 *depthstencil = nullptr;
			_device->GetDepthStencilSurface(&depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == _depthStencilReplacement)
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
		void D3D9Runtime::OnApplyEffect()
		{
			if (!_isEffectCompiled)
			{
				return;
			}

			_device->SetRenderTarget(0, _backbufferResolved);
			_device->SetDepthStencilSurface(nullptr);

			// Setup vertex input
			_device->SetStreamSource(0, _effectTriangleBuffer, 0, sizeof(float));
			_device->SetVertexDeclaration(_effectTriangleLayout);

			// Apply post processing
			Runtime::OnApplyEffect();
		}
		void D3D9Runtime::OnApplyEffectTechnique(const Technique *technique)
		{
			Runtime::OnApplyEffectTechnique(technique);

			bool defaultDepthStencilCleared = false;

			// Setup shader constants
			_device->SetVertexShaderConstantF(0, reinterpret_cast<const float *>(_uniformDataStorage.data()), _constantRegisterCount);
			_device->SetPixelShaderConstantF(0, reinterpret_cast<const float *>(_uniformDataStorage.data()), _constantRegisterCount);

			for (const auto &pass : static_cast<const D3D9Technique *>(technique)->Passes)
			{
				// Setup states
				pass.Stateblock->Apply();

				// Save backbuffer of previous pass
				_device->StretchRect(_backbufferResolved, nullptr, _backbufferTextureSurface, nullptr, D3DTEXF_NONE);

				// Setup shader resources
				for (DWORD sampler = 0; sampler < pass.SamplerCount; sampler++)
				{
					_device->SetTexture(sampler, pass.Samplers[sampler].Texture->TextureInterface);

					for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state++)
					{
						_device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(state), pass.Samplers[sampler].States[state]);
					}
				}

				// Setup rendertargets
				for (DWORD target = 0; target < _numSimultaneousRTs; target++)
				{
					_device->SetRenderTarget(target, pass.RT[target]);
				}

				D3DVIEWPORT9 viewport;
				_device->GetViewport(&viewport);

				const float texelsize[4] = { -1.0f / viewport.Width, 1.0f / viewport.Height };
				_device->SetVertexShaderConstantF(255, texelsize, 1);

				const bool backbufferSizedRendertarget = viewport.Width == _width && viewport.Height == _height;

				if (backbufferSizedRendertarget)
				{
					_device->SetDepthStencilSurface(_defaultDepthStencil);
				}
				else
				{
					_device->SetDepthStencilSurface(nullptr);
				}

				if (backbufferSizedRendertarget && !defaultDepthStencilCleared)
				{
					defaultDepthStencilCleared = true;

					// Clear depthstencil
					_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
				}
				else
				{
					_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
				}

				// Draw triangle
				_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

				Runtime::OnDrawCall(3);

				// Update shader resources
				for (IDirect3DSurface9 *const target : pass.RT)
				{
					if (target == nullptr || target == _backbufferResolved)
					{
						continue;
					}

					IDirect3DBaseTexture9 *texture = nullptr;

					if (SUCCEEDED(target->GetContainer(__uuidof(IDirect3DBaseTexture9), reinterpret_cast<void **>(&texture))))
					{
						if (texture->GetLevelCount() > 1)
						{
							texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
							texture->GenerateMipSubLevels();
						}

						texture->Release();
					}
				}
			}
		}

		void D3D9Runtime::OnSetDepthStencilSurface(IDirect3DSurface9 *&depthstencil)
		{
			if (_depthSourceTable.find(depthstencil) == _depthSourceTable.end())
			{
				D3DSURFACE_DESC desc;
				depthstencil->GetDesc(&desc);

				// Early depthstencil rejection
				if ((desc.Width < _width * 0.95 || desc.Width > _width * 1.05) || (desc.Height < _height * 0.95 || desc.Height > _height * 1.05) || desc.MultiSampleType != D3DMULTISAMPLE_NONE)
				{
					return;
				}
	
				LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

				depthstencil->AddRef();

				// Begin tracking new depthstencil
				const DepthSourceInfo info = { desc.Width, desc.Height };
				_depthSourceTable.emplace(depthstencil, info);
			}

			if (_depthStencilReplacement != nullptr && depthstencil == _depthStencil)
			{
				depthstencil = _depthStencilReplacement;
			}
		}
		void D3D9Runtime::OnGetDepthStencilSurface(IDirect3DSurface9 *&depthstencil)
		{
			if (_depthStencilReplacement != nullptr && depthstencil == _depthStencilReplacement)
			{
				depthstencil->Release();

				depthstencil = _depthStencil;
				depthstencil->AddRef();
			}
		}

		void D3D9Runtime::Screenshot(unsigned char *buffer) const
		{
			if (_backbufferFormat != D3DFMT_X8R8G8B8 && _backbufferFormat != D3DFMT_X8B8G8R8 && _backbufferFormat != D3DFMT_A8R8G8B8 && _backbufferFormat != D3DFMT_A8B8G8R8)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbufferFormat << ".";
				return;
			}

			IDirect3DSurface9 *screenshotSurface = nullptr;

			HRESULT hr = _device->CreateOffscreenPlainSurface(_width, _height, _backbufferFormat, D3DPOOL_SYSTEMMEM, &screenshotSurface, nullptr);

			if (FAILED(hr))
			{
				return;
			}

			// Copy screenshot data to surface
			hr = _device->GetRenderTargetData(_backbufferResolved, screenshotSurface);

			if (FAILED(hr))
			{
				screenshotSurface->Release();
				return;
			}

			D3DLOCKED_RECT screenshotLock;
			hr = screenshotSurface->LockRect(&screenshotLock, nullptr, D3DLOCK_READONLY);

			if (FAILED(hr))
			{
				screenshotSurface->Release();
				return;
			}

			BYTE *pMem = buffer;
			BYTE *pLocked = static_cast<BYTE *>(screenshotLock.pBits);

			const UINT pitch = _width * 4;

			// Copy screenshot data to memory
			for (UINT y = 0; y < _height; ++y)
			{
				CopyMemory(pMem, pLocked, std::min(pitch, static_cast<UINT>(screenshotLock.Pitch)));

				for (UINT x = 0; x < pitch; x += 4)
				{
					pMem[x + 3] = 0xFF;

					if (_backbufferFormat == D3DFMT_A8R8G8B8 || _backbufferFormat == D3DFMT_X8R8G8B8)
					{
						std::swap(pMem[x + 0], pMem[x + 2]);
					}
				}

				pMem += pitch;
				pLocked += screenshotLock.Pitch;
			}

			screenshotSurface->UnlockRect();

			screenshotSurface->Release();
		}
		bool D3D9Runtime::UpdateEffect(const FX::NodeTree &ast, const std::vector<std::string> &pragmas, std::string &errors)
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

			D3D9EffectCompiler visitor(ast, skipOptimization);

			return visitor.Compile(this, errors);
		}
		bool D3D9Runtime::UpdateTexture(Texture *texture, const unsigned char *data, size_t size)
		{
			D3D9Texture *const textureImpl = dynamic_cast<D3D9Texture *>(texture);

			assert(textureImpl != nullptr);
			assert(data != nullptr && size != 0);

			if (textureImpl->DataSource != D3D9Texture::Source::Memory)
			{
				return false;
			}

			D3DSURFACE_DESC desc;
			textureImpl->TextureInterface->GetLevelDesc(0, &desc);

			IDirect3DTexture9 *memTexture = nullptr;

			HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &memTexture, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create memory texture for texture updating! HRESULT is '" << hr << "'.";

				return false;
			}

			D3DLOCKED_RECT memLock;
			hr = memTexture->LockRect(0, &memLock, nullptr, 0);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to lock memory texture for texture updating! HRESULT is '" << hr << "'.";

				memTexture->Release();

				return false;
			}

			size = std::min<size_t>(size, memLock.Pitch * texture->Height);
			BYTE *pLocked = static_cast<BYTE *>(memLock.pBits);

			switch (texture->Format)
			{
				case Texture::PixelFormat::R8:
					for (size_t i = 0; i < size; i += 1, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = 0, pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case Texture::PixelFormat::RG8:
					for (size_t i = 0; i < size; i += 2, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case Texture::PixelFormat::RGBA8:
					for (size_t i = 0; i < size; i += 4, pLocked += 4)
					{
						pLocked[0] = data[i + 2], pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = data[i + 3];
					}
					break;
				default:
					CopyMemory(pLocked, data, size);
					break;
			}

			memTexture->UnlockRect(0);

			hr = _device->UpdateTexture(memTexture, textureImpl->TextureInterface);

			memTexture->Release();

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to update texture from memory texture! HRESULT is '" << hr << "'.";

				return false;
			}

			return true;
		}

		void D3D9Runtime::DetectDepthSource()
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
			IDirect3DSurface9 *best = nullptr;

			for (auto it = _depthSourceTable.begin(); it != _depthSourceTable.end(); ++it)
			{
				if (GetRefCount(it->first) == 1)
				{
					LOG(TRACE) << "Removing depthstencil " << it->first << " from list of possible depth candidates ...";

					it->first->Release();

					it = _depthSourceTable.erase(it);
					it = std::prev(it);
					continue;
				}

				if (it->second.DrawCallCount == 0)
				{
					continue;
				}
				else if ((it->second.DrawVerticesCount * (1.2f - it->second.DrawCallCount / _stats.DrawCalls)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / _stats.DrawCalls)))
				{
					best = it->first;
					bestInfo = it->second;
				}

				it->second.DrawCallCount = it->second.DrawVerticesCount = 0;
			}

			if (_depthStencil != best)
			{
				LOG(TRACE) << "Switched depth source to depthstencil " << best << ".";

				CreateDepthStencilReplacement(best);
			}
		}
		bool D3D9Runtime::CreateDepthStencilReplacement(IDirect3DSurface9 *depthstencil)
		{
			SAFE_RELEASE(_depthStencil);
			SAFE_RELEASE(_depthStencilReplacement);
			SAFE_RELEASE(_depthStencilTexture);

			if (depthstencil != nullptr)
			{
				_depthStencil = depthstencil;
				_depthStencil->AddRef();

				D3DSURFACE_DESC desc;
				_depthStencil->GetDesc(&desc);

				if (desc.Format != D3DFMT_INTZ)
				{
					const HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_INTZ, D3DPOOL_DEFAULT, &_depthStencilTexture, nullptr);

					if (SUCCEEDED(hr))
					{
						_depthStencilTexture->GetSurfaceLevel(0, &_depthStencilReplacement);

						// Update auto depthstencil
						IDirect3DSurface9 *depthstencil = nullptr;
						_device->GetDepthStencilSurface(&depthstencil);

						if (depthstencil != nullptr)
						{
							depthstencil->Release();

							if (depthstencil == _depthStencil)
							{
								_device->SetDepthStencilSurface(_depthStencilReplacement);
							}
						}
					}
					else
					{
						LOG(TRACE) << "Failed to create depthstencil replacement texture! HRESULT is '" << hr << "'. Are you missing support for the 'INTZ' format?";

						return false;
					}
				}
				else
				{
					_depthStencilReplacement = _depthStencil;
					_depthStencilReplacement->AddRef();
					_depthStencilReplacement->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void **>(&_depthStencilTexture));
				}
			}

			// Update effect textures
			for (const auto &it : _textures)
			{
				D3D9Texture *texture = static_cast<D3D9Texture *>(it.get());

				if (texture->DataSource == D3D9Texture::Source::DepthStencil)
				{
					texture->ChangeDataSource(D3D9Texture::Source::DepthStencil, _depthStencilTexture);
				}
			}

			return true;
		}
	}
}
