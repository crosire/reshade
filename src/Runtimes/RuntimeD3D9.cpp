#include "Log.hpp"
#include "RuntimeD3D9.hpp"
#include "EffectTree.hpp"

#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <boost\algorithm\string.hpp>
#include <nanovg_d3d9.h>
#include <unordered_set>

#define D3DFMT_INTZ static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'))

// -----------------------------------------------------------------------------------------------------

namespace ReShade
{
	namespace Runtimes
	{
		namespace
		{
			class D3D9EffectCompiler : private boost::noncopyable
			{
			public:
				D3D9EffectCompiler(const FX::Tree &ast, bool skipoptimization = false) : mAST(ast), mEffect(nullptr), mFatal(false), mSkipShaderOptimization(skipoptimization), mCurrentFunction(nullptr), mCurrentRegisterOffset(0), mCurrentStorageSize(0)
				{
				}

				bool Compile(D3D9Effect *effect, std::string &errors)
				{
					this->mEffect = effect;

					this->mFatal = false;
					this->mErrors.clear();

					this->mGlobalCode.clear();

					for (auto structure : this->mAST.Types)
					{
						Visit(this->mGlobalCode, structure);
					}

					for (auto uniform : this->mAST.Uniforms)
					{
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
							Visit(this->mGlobalCode, uniform);

							this->mGlobalCode += ";\n";
						}
					}

					for (auto function : this->mAST.Functions)
					{
						this->mCurrentFunction = function;

						Visit(this->mFunctions[function].SourceCode, function);
					}

					for (auto technique : this->mAST.Techniques)
					{
						VisitTechnique(technique);
					}

					errors += this->mErrors;

					return !this->mFatal;
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
				static D3DFORMAT LiteralToFormat(unsigned int value, FX::Effect::Texture::Format &name)
				{
					switch (value)
					{
						case FX::Nodes::Variable::Properties::R8:
							name = FX::Effect::Texture::Format::R8;
							return D3DFMT_A8R8G8B8;
						case FX::Nodes::Variable::Properties::R16F:
							name = FX::Effect::Texture::Format::R16F;
							return D3DFMT_R16F;
						case FX::Nodes::Variable::Properties::R32F:
							name = FX::Effect::Texture::Format::R32F;
							return D3DFMT_R32F;
						case FX::Nodes::Variable::Properties::RG8:
							name = FX::Effect::Texture::Format::RG8;
							return D3DFMT_A8R8G8B8;
						case FX::Nodes::Variable::Properties::RG16:
							name = FX::Effect::Texture::Format::RG16;
							return D3DFMT_G16R16;
						case FX::Nodes::Variable::Properties::RG16F:
							name = FX::Effect::Texture::Format::RG16F;
							return D3DFMT_G16R16F;
						case FX::Nodes::Variable::Properties::RG32F:
							name = FX::Effect::Texture::Format::RG32F;
							return D3DFMT_G32R32F;
						case FX::Nodes::Variable::Properties::RGBA8:
							name = FX::Effect::Texture::Format::RGBA8;
							return D3DFMT_A8R8G8B8;  // D3DFMT_A8B8G8R8 appearently isn't supported by hardware very well
						case FX::Nodes::Variable::Properties::RGBA16:
							name = FX::Effect::Texture::Format::RGBA16;
							return D3DFMT_A16B16G16R16;
						case FX::Nodes::Variable::Properties::RGBA16F:
							name = FX::Effect::Texture::Format::RGBA16F;
							return D3DFMT_A16B16G16R16F;
						case FX::Nodes::Variable::Properties::RGBA32F:
							name = FX::Effect::Texture::Format::RGBA32F;
							return D3DFMT_A32B32G32R32F;
						case FX::Nodes::Variable::Properties::DXT1:
							name = FX::Effect::Texture::Format::DXT1;
							return D3DFMT_DXT1;
						case FX::Nodes::Variable::Properties::DXT3:
							name = FX::Effect::Texture::Format::DXT3;
							return D3DFMT_DXT3;
						case FX::Nodes::Variable::Properties::DXT5:
							name = FX::Effect::Texture::Format::DXT5;
							return D3DFMT_DXT5;
						case FX::Nodes::Variable::Properties::LATC1:
							name = FX::Effect::Texture::Format::LATC1;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
						case FX::Nodes::Variable::Properties::LATC2:
							name = FX::Effect::Texture::Format::LATC2;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
						default:
							name = FX::Effect::Texture::Format::Unknown;
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

					this->mErrors += location.Source + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): error: " + formatted + '\n';
					this->mFatal = true;
				}
				void Warning(const FX::Location &location, const char *message, ...)
				{
					char formatted[512];

					va_list args;
					va_start(args, message);
					vsprintf_s(formatted, message, args);
					va_end(args);

					this->mErrors += location.Source + "(" + std::to_string(location.Line) + ", " + std::to_string(location.Column) + "): warning: " + formatted + '\n';
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
							source += "sampler2D";
							break;
						case FX::Nodes::Type::Class::Struct:
							source += type.Definition->Name;
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

					for (std::size_t i = 1, count = node->Cases.size(); i < count; ++i)
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
					output += node->Reference->Name;

					if (node->Reference->Type.IsSampler() && (this->mSamplers.find(node->Reference->Name) != this->mSamplers.end()))
					{
						this->mFunctions.at(this->mCurrentFunction).SamplerDependencies.insert(node->Reference->Name);
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
					for (auto expression : node->Expressions)
					{
						Visit(output, expression);

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());
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
					std::string part1, part2, part3, part4;

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
							part1 = "tex2D(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DFetch:
							part1 = "tex2D(";
							part2 = ", float2(";
							part3 = "))";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DGather:
							part1 = "__tex2Dgather(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DGatherOffset:
							part1 = "__tex2Dgather(";
							part2 = ", ";
							part3 = " + (";
							part4 = ") * _PIXEL_SIZE_.xy)";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DLevel:
							part1 = "tex2Dlod(";
							part2 = ", ";
							part3 = ")";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DLevelOffset:
							part1 = "tex2Dlod(";
							part2 = ", ";
							part3 = " + float4((";
							part4 = ") * _PIXEL_SIZE_.xy, 0, 0))";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DOffset:
							part1 = "tex2D(";
							part2 = ", ";
							part3 = " + (";
							part4 = ") * _PIXEL_SIZE_.xy)";
							break;
						case FX::Nodes::Intrinsic::Op::Tex2DSize:
							Error(node->Location, "'tex2Dsize' is not supported in Direct3D9");
							return;
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

					if (node->Field->Type.HasQualifier(FX::Nodes::Type::Qualifier::Uniform))
					{
						output += '_';
					}
					else
					{
						output += '.';
					}

					output += node->Field->Name;
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
					output += node->CalleeName + '(';

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

					auto &info = this->mFunctions.at(this->mCurrentFunction);
					auto &infoCallee = this->mFunctions.at(node->Callee);
					
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
					output += "struct " + node->Name + "\n{\n";

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

					output += node->Name;

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
					
					output += ' ' + node->Name + '(';

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
						FX::Effect::Annotation data;

						switch (annotation.Value->Type.BaseClass)
						{
							case FX::Nodes::Type::Class::Bool:
							case FX::Nodes::Type::Class::Int:
								data = annotation.Value->Value.Int;
								break;
							case FX::Nodes::Type::Class::Uint:
								data = annotation.Value->Value.Uint;
								break;
							case FX::Nodes::Type::Class::Float:
								data = annotation.Value->Value.Float;
								break;
							case FX::Nodes::Type::Class::String:
								data = annotation.Value->StringValue;
								break;
						}

						object.AddAnnotation(annotation.Name, data);
					}
				}
				void VisitTexture(const FX::Nodes::Variable *node)
				{
					D3D9Texture::Description desc;
					desc.Width = node->Properties.Width;
					desc.Height = node->Properties.Height;
					UINT levels = desc.Levels = node->Properties.MipLevels;
					const D3DFORMAT format = LiteralToFormat(node->Properties.Format, desc.Format);
					D3D9Texture *const obj = new D3D9Texture(this->mEffect, desc);

					VisitAnnotation(node->Annotations, *obj);

					if (node->Semantic == "COLOR" || node->Semantic == "SV_TARGET")
					{
						obj->mSource = D3D9Texture::Source::BackBuffer;
						obj->ChangeSource(this->mEffect->mRuntime->mBackBufferTexture);
					}
					else if (node->Semantic == "DEPTH" || node->Semantic == "SV_DEPTH")
					{
						obj->mSource = D3D9Texture::Source::DepthStencil;
						obj->ChangeSource(this->mEffect->mRuntime->mDepthStencilTexture);
					}

					if (obj->mSource != D3D9Texture::Source::Memory)
					{
						if (desc.Width != 1 || desc.Height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
						{
							Warning(node->Location, "texture properties on backbuffer textures are ignored");
						}
					}
					else
					{
						HRESULT hr;
						DWORD usage = 0;

						IDirect3D9 *const factory = this->mEffect->mRuntime->mDirect3D;
						IDirect3DDevice9 *const device = this->mEffect->mRuntime->mDevice;

						D3DDEVICE_CREATION_PARAMETERS cp;
						device->GetCreationParameters(&cp);

						if (levels > 1)
						{
							hr = factory->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, format);

							if (hr == D3D_OK)
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
					
						hr = factory->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, format);

						if (SUCCEEDED(hr))
						{
							usage |= D3DUSAGE_RENDERTARGET;
						}

						hr = this->mEffect->mRuntime->mDevice->CreateTexture(desc.Width, desc.Height, levels, usage, format, D3DPOOL_DEFAULT, &obj->mTexture, nullptr);

						if (FAILED(hr))
						{
							Error(node->Location, "internal texture creation failed with '%u'!", hr);
							return;
						}

						hr = obj->mTexture->GetSurfaceLevel(0, &obj->mTextureSurface);

						assert(SUCCEEDED(hr));
					}

					this->mEffect->AddTexture(node->Name, obj);
				}
				void VisitSampler(const FX::Nodes::Variable *node)
				{
					if (node->Properties.Texture == nullptr)
					{
						Error(node->Location, "sampler '%s' is missing required 'Texture' property", node->Name);
						return;
					}

					D3D9Texture *const texture = static_cast<D3D9Texture *>(this->mEffect->GetTexture(node->Properties.Texture->Name));

					if (texture == nullptr)
					{
						this->mFatal = true;
						return;
					}

					D3D9Sampler sampler;
					sampler.mTexture = texture;
					sampler.mStates[D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressU);
					sampler.mStates[D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressV);
					sampler.mStates[D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(node->Properties.AddressW);
					sampler.mStates[D3DSAMP_BORDERCOLOR] = 0;
					sampler.mStates[D3DSAMP_MINFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MinFilter);
					sampler.mStates[D3DSAMP_MAGFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MagFilter);
					sampler.mStates[D3DSAMP_MIPFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->Properties.MipFilter);
					sampler.mStates[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&node->Properties.MipLODBias);
					sampler.mStates[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, node->Properties.MinLOD));
					sampler.mStates[D3DSAMP_MAXANISOTROPY] = node->Properties.MaxAnisotropy;
					sampler.mStates[D3DSAMP_SRGBTEXTURE] = node->Properties.SRGBTexture;

					this->mSamplers[node->Name] = sampler;
				}
				void VisitUniform(const FX::Nodes::Variable *node)
				{
					VisitType(this->mGlobalCode, node->Type);
					
					this->mGlobalCode += ' ';
					this->mGlobalCode += node->Name;

					if (node->Type.IsArray())
					{
						this->mGlobalCode += '[' + ((node->Type.ArrayLength > 0) ? std::to_string(node->Type.ArrayLength) : "") + ']';
					}

					this->mGlobalCode += " : register(c" + std::to_string(this->mCurrentRegisterOffset / 4) + ");\n";

					D3D9Constant::Description desc;
					desc.Rows = node->Type.Rows;
					desc.Columns = node->Type.Cols;
					desc.Elements = node->Type.ArrayLength;
					desc.StorageSize = desc.Rows * desc.Columns * std::max(1u, desc.Elements);

					switch (node->Type.BaseClass)
					{
						case FX::Nodes::Type::Class::Bool:
							desc.Type = FX::Effect::Constant::Type::Bool;
							desc.StorageSize *= sizeof(int);
							break;
						case FX::Nodes::Type::Class::Int:
							desc.Type = FX::Effect::Constant::Type::Int;
							desc.StorageSize *= sizeof(int);
							break;
						case FX::Nodes::Type::Class::Uint:
							desc.Type = FX::Effect::Constant::Type::Uint;
							desc.StorageSize *= sizeof(unsigned int);
							break;
						case FX::Nodes::Type::Class::Float:
							desc.Type = FX::Effect::Constant::Type::Float;
							desc.StorageSize *= sizeof(float);
							break;
					}

					const UINT regsize = static_cast<UINT>(static_cast<float>(desc.StorageSize) / sizeof(float));
					const UINT regalignment = 4 - (regsize % 4);

					desc.StorageOffset = this->mCurrentRegisterOffset;
					this->mCurrentRegisterOffset += regsize + regalignment;

					D3D9Constant *const obj = new D3D9Constant(this->mEffect, desc);

					VisitAnnotation(node->Annotations, *obj);

					if (this->mCurrentRegisterOffset * sizeof(float) >= this->mCurrentStorageSize)
					{
						this->mEffect->mConstantStorage = static_cast<float *>(::realloc(this->mEffect->mConstantStorage, this->mCurrentStorageSize += sizeof(float) * 64));
					}

					this->mEffect->mConstantRegisterCount = this->mCurrentRegisterOffset / 4;

					if (node->Initializer != nullptr && node->Initializer->NodeId == FX::Node::Id::Literal)
					{
						CopyMemory(this->mEffect->mConstantStorage + desc.StorageOffset, &static_cast<const FX::Nodes::Literal *>(node->Initializer)->Value, desc.StorageSize);
					}
					else
					{
						ZeroMemory(this->mEffect->mConstantStorage + desc.StorageOffset, desc.StorageSize);
					}

					this->mEffect->AddConstant(node->Name, obj);
				}
				void VisitTechnique(const FX::Nodes::Technique *node)
				{
					D3D9Technique::Description desc;
					desc.Passes = static_cast<unsigned int>(node->Passes.size());

					D3D9Technique *const obj = new D3D9Technique(this->mEffect, desc);

					VisitAnnotation(node->Annotations, *obj);

					for (auto pass : node->Passes)
					{
						VisitTechniquePass(pass, obj->mPasses);
					}

					this->mEffect->AddTechnique(node->Name, obj);
				}
				void VisitTechniquePass(const FX::Nodes::Pass *node, std::vector<D3D9Technique::Pass> &passes)
				{
					D3D9Technique::Pass pass;
					ZeroMemory(&pass, sizeof(D3D9Technique::Pass));
					pass.RT[0] = this->mEffect->mRuntime->mBackBufferResolved;

					std::string samplers;
					const char shaderTypes[2][3] = { "vs", "ps" };
					const FX::Nodes::Function *shaderFunctions[2] = { node->States.VertexShader, node->States.PixelShader };

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] == nullptr)
						{
							continue;
						}

						for (auto sampler : this->mFunctions.at(shaderFunctions[i]).SamplerDependencies)
						{
							pass.Samplers[pass.SamplerCount] = this->mSamplers.at(sampler);
							samplers += "sampler2D " + sampler + " : register(s" + std::to_string(pass.SamplerCount++) + ");\n";

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

					IDirect3DDevice9 *const device = this->mEffect->mRuntime->mDevice;
				
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

						D3D9Texture *const texture = static_cast<D3D9Texture *>(this->mEffect->GetTexture(node->States.RenderTargets[i]->Name));

						if (texture == nullptr)
						{
							this->mFatal = true;
							return;
						}

						pass.RT[i] = texture->mTextureSurface;
					}

					passes.push_back(std::move(pass));
				}
				void VisitTechniquePassShader(const FX::Nodes::Function *node, const std::string &shadertype, const std::string &samplers, D3D9Technique::Pass &pass)
				{
					std::string source =
						"uniform float4 _PIXEL_SIZE_ : register(c223);\n"
						"float4 __tex2Dgather(sampler2D s, float2 c) { return float4(tex2Dlod(s, float4(c + float2(0, 1) * _PIXEL_SIZE_.xy, 0, 0)).r, tex2Dlod(s, float4(c + float2(1, 1) * _PIXEL_SIZE_.xy, 0, 0)).r, tex2Dlod(s, float4(c + float2(1, 0) * _PIXEL_SIZE_.xy, 0, 0)).r, tex2Dlod(s, float4(c, 0, 0)).r); }\n";

					if (shadertype == "ps")
					{
						source += "#define POSITION VPOS\n";
					}

					source += samplers;
					source += this->mGlobalCode;

					for (auto dependency : this->mFunctions.at(node).FunctionDependencies)
					{
						source += this->mFunctions.at(dependency).SourceCode;
					}

					source += this->mFunctions.at(node).SourceCode;

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

					source += node->Name;
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
						source += positionVariable + ".xy += _PIXEL_SIZE_.zw * " + positionVariable + ".ww;\n";
					}

					if (!node->ReturnType.IsVoid())
					{
						source += "return _return;\n";
					}

					source += "}\n";

					LOG(TRACE) << "> Compiling shader '" << node->Name << "':\n\n" << source.c_str() << "\n";

					UINT flags = 0;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (this->mSkipShaderOptimization)
					{
						flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
					}

					HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, "__main", (shadertype + "_3_0").c_str(), flags, 0, &compiled, &errors);

					if (errors != nullptr)
					{
						this->mErrors.append(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

						errors->Release();
					}

					if (FAILED(hr))
					{
						this->mFatal = true;
						return;
					}

					if (shadertype == "vs")
					{
						hr = this->mEffect->mRuntime->mDevice->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.VS);
					}
					else if (shadertype == "ps")
					{
						hr = this->mEffect->mRuntime->mDevice->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.PS);
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
					std::unordered_set<std::string> SamplerDependencies;
					std::vector<const FX::Nodes::Function *> FunctionDependencies;
				};

				D3D9Effect *mEffect;
				const FX::Tree &mAST;
				bool mFatal, mSkipShaderOptimization;
				std::string mErrors;
				std::string mGlobalCode;
				unsigned int mCurrentRegisterOffset, mCurrentStorageSize;
				const FX::Nodes::Function *mCurrentFunction;
				std::unordered_map<std::string, D3D9Sampler> mSamplers;
				std::unordered_map<const FX::Nodes::Function *, Function> mFunctions;
			};

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
		}

		// -----------------------------------------------------------------------------------------------------

		D3D9Runtime::D3D9Runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : mDevice(device), mSwapChain(swapchain), mDirect3D(nullptr), mDeviceCaps(), mStateBlock(nullptr), mBackBuffer(nullptr), mBackBufferResolved(nullptr), mBackBufferTexture(nullptr), mBackBufferTextureSurface(nullptr), mDepthStencil(nullptr), mDepthStencilReplacement(nullptr), mDepthStencilTexture(nullptr), mDefaultDepthStencil(nullptr), mLost(true)
		{
			assert(this->mDevice != nullptr);
			assert(this->mSwapChain != nullptr);

			this->mDevice->AddRef();
			this->mDevice->GetDirect3D(&this->mDirect3D);
			this->mSwapChain->AddRef();

			assert(this->mDirect3D != nullptr);

			this->mDevice->GetDeviceCaps(&this->mDeviceCaps);
			this->mDeviceCaps.NumSimultaneousRTs = std::min(this->mDeviceCaps.NumSimultaneousRTs, static_cast<DWORD>(8));

			ZeroMemory(&this->mPresentParams, sizeof(D3DPRESENT_PARAMETERS));

			D3DDEVICE_CREATION_PARAMETERS params;
			this->mDevice->GetCreationParameters(&params);

			D3DADAPTER_IDENTIFIER9 identifier;
			this->mDirect3D->GetAdapterIdentifier(params.AdapterOrdinal, 0, &identifier);

			this->mVendorId = identifier.VendorId;
			this->mDeviceId = identifier.DeviceId;
			this->mRendererId = D3D_FEATURE_LEVEL_9_3;
		}
		D3D9Runtime::~D3D9Runtime()
		{
			assert(this->mLost);

			this->mDevice->Release();
			this->mSwapChain->Release();
			this->mDirect3D->Release();
		}

		bool D3D9Runtime::OnCreateInternal(const D3DPRESENT_PARAMETERS &params)
		{
			this->mPresentParams = params;

			HRESULT hr = this->mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &this->mBackBuffer);

			assert(SUCCEEDED(hr));

			if (this->mPresentParams.MultiSampleType != D3DMULTISAMPLE_NONE || (this->mPresentParams.BackBufferFormat == D3DFMT_X8R8G8B8 || this->mPresentParams.BackBufferFormat == D3DFMT_X8B8G8R8))
			{
				switch (this->mPresentParams.BackBufferFormat)
				{
					case D3DFMT_X8R8G8B8:
						this->mPresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
						break;
					case D3DFMT_X8B8G8R8:
						this->mPresentParams.BackBufferFormat = D3DFMT_A8B8G8R8;
						break;
				}

				hr = this->mDevice->CreateRenderTarget(this->mPresentParams.BackBufferWidth, this->mPresentParams.BackBufferHeight, this->mPresentParams.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &this->mBackBufferResolved, nullptr);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer resolve texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(this->mBackBuffer);

					return false;
				}
			}
			else
			{
				this->mBackBufferResolved = this->mBackBuffer;
				this->mBackBufferResolved->AddRef();
			}

			hr = this->mDevice->CreateTexture(this->mPresentParams.BackBufferWidth, this->mPresentParams.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET, this->mPresentParams.BackBufferFormat, D3DPOOL_DEFAULT, &this->mBackBufferTexture, nullptr);

			if (SUCCEEDED(hr))
			{
				this->mBackBufferTexture->GetSurfaceLevel(0, &this->mBackBufferTextureSurface);
			}
			else
			{
				LOG(TRACE) << "Failed to create backbuffer texture! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);

				return false;
			}

			hr = this->mDevice->CreateDepthStencilSurface(this->mPresentParams.BackBufferWidth, this->mPresentParams.BackBufferHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &this->mDefaultDepthStencil, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);
				SAFE_RELEASE(this->mBackBufferTexture);
				SAFE_RELEASE(this->mBackBufferTextureSurface);

				return nullptr;
			}

			hr = this->mDevice->CreateStateBlock(D3DSBT_ALL, &this->mStateBlock);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create stateblock! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(this->mBackBuffer);
				SAFE_RELEASE(this->mBackBufferResolved);
				SAFE_RELEASE(this->mBackBufferTexture);
				SAFE_RELEASE(this->mBackBufferTextureSurface);
				SAFE_RELEASE(this->mDefaultDepthStencil);

				return false;
			}

			this->mNVG = nvgCreateD3D9(this->mDevice, 0);

			this->mLost = false;

			Runtime::OnCreate(this->mPresentParams.BackBufferWidth, this->mPresentParams.BackBufferHeight);

			return true;
		}
		void D3D9Runtime::OnDeleteInternal()
		{
			Runtime::OnDelete();

			nvgDeleteD3D9(this->mNVG);

			this->mNVG = nullptr;

			SAFE_RELEASE(this->mStateBlock);

			SAFE_RELEASE(this->mBackBuffer);
			SAFE_RELEASE(this->mBackBufferResolved);
			SAFE_RELEASE(this->mBackBufferTexture);
			SAFE_RELEASE(this->mBackBufferTextureSurface);

			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mDepthStencilReplacement);
			SAFE_RELEASE(this->mDepthStencilTexture);

			SAFE_RELEASE(this->mDefaultDepthStencil);

			ZeroMemory(&this->mPresentParams, sizeof(D3DPRESENT_PARAMETERS));

			this->mDepthSourceTable.clear();

			this->mLost = true;
		}
		void D3D9Runtime::OnDrawInternal(D3DPRIMITIVETYPE type, UINT count)
		{
			UINT vertices = count;

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

			Runtime::OnDraw(vertices);

			IDirect3DSurface9 *depthstencil = nullptr;
			this->mDevice->GetDepthStencilSurface(&depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == this->mDepthStencilReplacement)
				{
					depthstencil = this->mDepthStencil;
				}

				const auto it = this->mDepthSourceTable.find(depthstencil);

				if (it != this->mDepthSourceTable.end())
				{
					it->second.DrawCallCount = static_cast<FLOAT>(this->mLastDrawCalls);
					it->second.DrawVerticesCount += vertices;
				}
			}
		}
		void D3D9Runtime::OnPresentInternal()
		{
			if (this->mLost)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}

			DetectDepthSource();

			// Begin post processing
			HRESULT hr = this->mDevice->BeginScene();

			if (FAILED(hr))
			{
				return;
			}

			// Capture device state
			this->mStateBlock->Capture();

			IDirect3DSurface9 *stateblockTargets[8] = { nullptr };
			IDirect3DSurface9 *stateblockDepthStencil = nullptr;

			for (DWORD target = 0, targetCount = std::min(this->mDeviceCaps.NumSimultaneousRTs, static_cast<DWORD>(8)); target < targetCount; ++target)
			{
				this->mDevice->GetRenderTarget(target, &stateblockTargets[target]);
			}

			this->mDevice->GetDepthStencilSurface(&stateblockDepthStencil);

			// Resolve backbuffer
			if (this->mBackBufferResolved != this->mBackBuffer)
			{
				this->mDevice->StretchRect(this->mBackBuffer, nullptr, this->mBackBufferResolved, nullptr, D3DTEXF_NONE);
			}

			this->mDevice->SetRenderTarget(0, this->mBackBufferResolved);
			this->mDevice->SetDepthStencilSurface(nullptr);

			// Apply post processing
			Runtime::OnPostProcess();

			// Reset rendertarget
			this->mDevice->SetRenderTarget(0, this->mBackBufferResolved);
			this->mDevice->SetDepthStencilSurface(this->mDefaultDepthStencil);
			this->mDevice->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

			// Apply presenting
			Runtime::OnPresent();

			if (this->mLost)
			{
				return;
			}

			// Copy to backbuffer
			if (this->mBackBufferResolved != this->mBackBuffer)
			{
				this->mDevice->StretchRect(this->mBackBufferResolved, nullptr, this->mBackBuffer, nullptr, D3DTEXF_NONE);
			}

			// Apply previous device state
			this->mStateBlock->Apply();

			for (DWORD target = 0, targetCount = std::min(this->mDeviceCaps.NumSimultaneousRTs, static_cast<DWORD>(8)); target < targetCount; ++target)
			{
				this->mDevice->SetRenderTarget(target, stateblockTargets[target]);

				SAFE_RELEASE(stateblockTargets[target]);
			}
			
			this->mDevice->SetDepthStencilSurface(stateblockDepthStencil);

			SAFE_RELEASE(stateblockDepthStencil);

			// End post processing
			this->mDevice->EndScene();
		}
		void D3D9Runtime::OnDeleteDepthStencilSurface(IDirect3DSurface9 *depthstencil)
		{
			assert(depthstencil != nullptr);

			const auto it = this->mDepthSourceTable.find(depthstencil);

			if (it != this->mDepthSourceTable.end())
			{
				LOG(TRACE) << "Removing depthstencil " << depthstencil << " from list of possible depth candidates ...";

				this->mDepthSourceTable.erase(it);
			}
		}
		void D3D9Runtime::OnSetDepthStencilSurface(IDirect3DSurface9 *&depthstencil)
		{
			if (this->mDepthSourceTable.find(depthstencil) == this->mDepthSourceTable.end())
			{
				D3DSURFACE_DESC desc;
				depthstencil->GetDesc(&desc);

				// Early depthstencil rejection
				if ((desc.Width < this->mPresentParams.BackBufferWidth * 0.95 || desc.Width > this->mPresentParams.BackBufferWidth * 1.05) || (desc.Height < this->mPresentParams.BackBufferHeight * 0.95 || desc.Height > this->mPresentParams.BackBufferHeight * 1.05) || desc.MultiSampleType != D3DMULTISAMPLE_NONE)
				{
					return;
				}
	
				LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

				// Begin tracking new depthstencil
				const DepthSourceInfo info = { desc.Width, desc.Height };
				this->mDepthSourceTable.emplace(depthstencil, info);
			}

			if (this->mDepthStencilReplacement != nullptr && depthstencil == this->mDepthStencil)
			{
				depthstencil = this->mDepthStencilReplacement;
			}
		}
		void D3D9Runtime::OnGetDepthStencilSurface(IDirect3DSurface9 *&depthstencil)
		{
			if (this->mDepthStencilReplacement != nullptr && depthstencil == this->mDepthStencilReplacement)
			{
				depthstencil->Release();

				depthstencil = this->mDepthStencil;
				depthstencil->AddRef();
			}
		}

		void D3D9Runtime::DetectDepthSource()
		{
			static int cooldown = 0, traffic = 0;

			if (cooldown-- > 0)
			{
				traffic += (sNetworkUpload + sNetworkDownload) > 0;
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

			if (this->mPresentParams.MultiSampleType != D3DMULTISAMPLE_NONE || this->mDepthSourceTable.empty())
			{
				return;
			}

			DepthSourceInfo bestInfo = { 0 };
			IDirect3DSurface9 *best = nullptr;

			for (auto &it : this->mDepthSourceTable)
			{
				if (it.second.DrawCallCount == 0)
				{
					continue;
				}
				else if ((it.second.DrawVerticesCount * (1.2f - it.second.DrawCallCount / this->mLastDrawCalls)) >= (bestInfo.DrawVerticesCount * (1.2f - bestInfo.DrawCallCount / this->mLastDrawCalls)))
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
		bool D3D9Runtime::CreateDepthStencilReplacement(IDirect3DSurface9 *depthstencil)
		{
			SAFE_RELEASE(this->mDepthStencil);
			SAFE_RELEASE(this->mDepthStencilReplacement);
			SAFE_RELEASE(this->mDepthStencilTexture);

			if (depthstencil != nullptr)
			{
				this->mDepthStencil = depthstencil;
				this->mDepthStencil->AddRef();

				D3DSURFACE_DESC desc;
				this->mDepthStencil->GetDesc(&desc);

				if (desc.Format != D3DFMT_INTZ)
				{
					const HRESULT hr = this->mDevice->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_INTZ, D3DPOOL_DEFAULT, &this->mDepthStencilTexture, nullptr);

					if (SUCCEEDED(hr))
					{
						this->mDepthStencilTexture->GetSurfaceLevel(0, &this->mDepthStencilReplacement);

						// Update auto depthstencil
						IDirect3DSurface9 *depthstencil = nullptr;
						this->mDevice->GetDepthStencilSurface(&depthstencil);

						if (depthstencil != nullptr)
						{
							depthstencil->Release();

							if (depthstencil == this->mDepthStencil)
							{
								this->mDevice->SetDepthStencilSurface(this->mDepthStencilReplacement);
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
					this->mDepthStencilReplacement = this->mDepthStencil;
					this->mDepthStencilReplacement->AddRef();
					this->mDepthStencilReplacement->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void **>(&this->mDepthStencilTexture));
				}
			}

			// Update effect textures
			D3D9Effect *effect = static_cast<D3D9Effect *>(this->mEffect.get());
		
			if (effect != nullptr)
			{
				for (auto &it : effect->mTextures)
				{
					D3D9Texture *texture = static_cast<D3D9Texture *>(it.second.get());

					if (texture->mSource == D3D9Texture::Source::DepthStencil)
					{
						texture->ChangeSource(this->mDepthStencilTexture);
					}
				}
			}

			return true;
		}

		std::unique_ptr<FX::Effect> D3D9Runtime::CompileEffect(const FX::Tree &ast, std::string &errors) const
		{
			std::unique_ptr<D3D9Effect> effect(new D3D9Effect(shared_from_this()));
			
			D3D9EffectCompiler visitor(ast, this->mSkipShaderOptimization);
		
			if (!visitor.Compile(effect.get(), errors))
			{
				return nullptr;
			}

			HRESULT hr = this->mDevice->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &effect->mVertexBuffer, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create effect vertexbuffer! HRESULT is '" << hr << "'.";

				return nullptr;
			}

			const D3DVERTEXELEMENT9 declaration[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};

			hr = this->mDevice->CreateVertexDeclaration(declaration, &effect->mVertexDeclaration);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create effect vertexdeclaration! HRESULT is '" << hr << "'.";

				return nullptr;
			}

			float *data = nullptr;

			hr = effect->mVertexBuffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to lock created effect vertexbuffer! HRESULT is '" << hr << "'.";

				return nullptr;
			}

			for (UINT i = 0; i < 3; ++i)
			{
				data[i] = static_cast<float>(i);
			}

			effect->mVertexBuffer->Unlock();

			return std::unique_ptr<FX::Effect>(effect.release());
		}
		void D3D9Runtime::CreateScreenshot(unsigned char *buffer, std::size_t size) const
		{
			if (this->mPresentParams.BackBufferFormat != D3DFMT_X8R8G8B8 && this->mPresentParams.BackBufferFormat != D3DFMT_X8B8G8R8 && this->mPresentParams.BackBufferFormat != D3DFMT_A8R8G8B8 && this->mPresentParams.BackBufferFormat != D3DFMT_A8B8G8R8)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << this->mPresentParams.BackBufferFormat << ".";
				return;
			}

			const UINT pitch = this->mPresentParams.BackBufferWidth * 4;

			if (size < static_cast<std::size_t>(pitch * this->mPresentParams.BackBufferHeight))
			{
				return;
			}

			IDirect3DSurface9 *screenshotSurface = nullptr;
			HRESULT hr = this->mDevice->CreateOffscreenPlainSurface(this->mPresentParams.BackBufferWidth, this->mPresentParams.BackBufferHeight, this->mPresentParams.BackBufferFormat, D3DPOOL_SYSTEMMEM, &screenshotSurface, nullptr);

			if (FAILED(hr))
			{
				return;
			}

			// Copy screenshot data to surface
			hr = this->mDevice->GetRenderTargetData(this->mBackBufferResolved, screenshotSurface);

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

			// Copy screenshot data to memory
			for (UINT y = 0; y < this->mPresentParams.BackBufferHeight; ++y)
			{
				CopyMemory(pMem, pLocked, std::min(pitch, static_cast<UINT>(screenshotLock.Pitch)));

				for (UINT x = 0; x < pitch; x += 4)
				{
					pMem[x + 3] = 0xFF;

					if (this->mPresentParams.BackBufferFormat == D3DFMT_A8R8G8B8 || this->mPresentParams.BackBufferFormat == D3DFMT_X8R8G8B8)
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

		D3D9Effect::D3D9Effect(std::shared_ptr<const D3D9Runtime> runtime) : mRuntime(runtime), mVertexBuffer(nullptr), mVertexDeclaration(nullptr), mConstantRegisterCount(0), mConstantStorage(nullptr)
		{
		}
		D3D9Effect::~D3D9Effect()
		{
			SAFE_RELEASE(this->mVertexBuffer);
			SAFE_RELEASE(this->mVertexDeclaration);
		}

		void D3D9Effect::Enter() const
		{
			IDirect3DDevice9 *const device = this->mRuntime->mDevice;

			// Setup vertex input
			device->SetStreamSource(0, this->mVertexBuffer, 0, sizeof(float));
			device->SetVertexDeclaration(this->mVertexDeclaration);
		}
		void D3D9Effect::Leave() const
		{
		}

		D3D9Texture::D3D9Texture(D3D9Effect *effect, const Description &desc) : Texture(desc), mEffect(effect), mSource(Source::Memory), mTexture(nullptr), mTextureSurface(nullptr)
		{
		}
		D3D9Texture::~D3D9Texture()
		{
			SAFE_RELEASE(this->mTexture);
			SAFE_RELEASE(this->mTextureSurface);
		}

		bool D3D9Texture::Update(unsigned int level, const unsigned char *data, std::size_t size)
		{
			if (data == nullptr || size == 0 || level > this->mDesc.Levels || this->mSource != Source::Memory)
			{
				return false;
			}

			IDirect3DSurface9 *textureSurface = nullptr;
			HRESULT hr = this->mTexture->GetSurfaceLevel(level, &textureSurface);

			if (FAILED(hr))
			{
				return false;
			}

			D3DSURFACE_DESC desc;
			textureSurface->GetDesc(&desc);
		
			IDirect3DTexture9 *memTexture = nullptr;
			hr = this->mEffect->mRuntime->mDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &memTexture, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create memory texture for texture updating! HRESULT is '" << hr << "'.";

				textureSurface->Release();

				return false;
			}

			IDirect3DSurface9 *memSurface;
			memTexture->GetSurfaceLevel(0, &memSurface);

			memTexture->Release();

			D3DLOCKED_RECT memLock;
			hr = memSurface->LockRect(&memLock, nullptr, 0);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to lock memory texture for texture updating! HRESULT is '" << hr << "'.";

				memSurface->Release();
				textureSurface->Release();

				return false;
			}

			size = std::min<size_t>(size, memLock.Pitch * this->mDesc.Height);
			BYTE *pLocked = static_cast<BYTE *>(memLock.pBits);

			switch (this->mDesc.Format)
			{
				case Format::R8:
					for (std::size_t i = 0; i < size; i += 1, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = 0, pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case Format::RG8:
					for (std::size_t i = 0; i < size; i += 2, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case Format::RGBA8:
					for (std::size_t i = 0; i < size; i += 4, pLocked += 4)
					{
						pLocked[0] = data[i + 2], pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = data[i + 3];
					}
					break;
				default:
					CopyMemory(pLocked, data, size);
					break;
			}

			memSurface->UnlockRect();

			hr = this->mEffect->mRuntime->mDevice->UpdateSurface(memSurface, nullptr, textureSurface, nullptr);

			memSurface->Release();
			textureSurface->Release();

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to update texture from memory texture! HRESULT is '" << hr << "'.";

				return false;
			}

			return true;
		}
		void D3D9Texture::ChangeSource(IDirect3DTexture9 *texture)
		{
			if (this->mTexture == texture)
			{
				return;
			}

			SAFE_RELEASE(this->mTexture);
			SAFE_RELEASE(this->mTextureSurface);

			if (texture != nullptr)
			{
				this->mTexture = texture;
				this->mTexture->AddRef();
				this->mTexture->GetSurfaceLevel(0, &this->mTextureSurface);

				D3DSURFACE_DESC texdesc;
				this->mTextureSurface->GetDesc(&texdesc);

				this->mDesc.Width = texdesc.Width;
				this->mDesc.Height = texdesc.Height;
				this->mDesc.Format = FX::Effect::Texture::Format::Unknown;
				this->mDesc.Levels = this->mTexture->GetLevelCount();
			}
			else
			{
				this->mDesc.Width = this->mDesc.Height = this->mDesc.Levels = 0;
				this->mDesc.Format = FX::Effect::Texture::Format::Unknown;
			}
		}

		D3D9Constant::D3D9Constant(D3D9Effect *effect, const Description &desc) : Constant(desc), mEffect(effect)
		{
		}
		D3D9Constant::~D3D9Constant()
		{
		}

		void D3D9Constant::GetValue(unsigned char *data, std::size_t size) const
		{
			size = std::min(size, this->mDesc.StorageSize);

			assert(this->mEffect->mConstantStorage != nullptr);

			CopyMemory(data, this->mEffect->mConstantStorage + this->mDesc.StorageOffset, size);
		}
		void D3D9Constant::SetValue(const unsigned char *data, std::size_t size)
		{
			size = std::min(size, this->mDesc.StorageSize);

			assert(this->mEffect->mConstantStorage != nullptr);

			CopyMemory(this->mEffect->mConstantStorage + this->mDesc.StorageOffset, data, size);
		}

		D3D9Technique::D3D9Technique(D3D9Effect *effect, const Description &desc) : Technique(desc), mEffect(effect)
		{
		}
		D3D9Technique::~D3D9Technique()
		{
			for (Pass &pass : this->mPasses)
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

		void D3D9Technique::RenderPass(unsigned int index) const
		{
			const std::shared_ptr<const D3D9Runtime> runtime = this->mEffect->mRuntime;
			IDirect3DDevice9 *device = runtime->mDevice;
			const D3D9Technique::Pass &pass = this->mPasses[index];

			if (index == 0)
			{
				// Clear depthstencil
				assert(runtime->mDefaultDepthStencil != nullptr);

				device->SetDepthStencilSurface(runtime->mDefaultDepthStencil);
				device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
			}

			// Setup states
			pass.Stateblock->Apply();

			// Save backbuffer of previous pass
			device->StretchRect(runtime->mBackBufferResolved, nullptr, runtime->mBackBufferTextureSurface, nullptr, D3DTEXF_NONE);

			// Setup shader resources
			for (DWORD sampler = 0; sampler < pass.SamplerCount; ++sampler)
			{
				device->SetTexture(sampler, pass.Samplers[sampler].mTexture->mTexture);

				for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; ++state)
				{
					device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(state), pass.Samplers[sampler].mStates[state]);
				}
			}

			// Setup rendertargets
			for (DWORD target = 0, targetCount = std::min(runtime->mDeviceCaps.NumSimultaneousRTs, static_cast<DWORD>(8)); target < targetCount; ++target)
			{
				device->SetRenderTarget(target, pass.RT[target]);
			}

			device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 0.0f, 0);

			D3DVIEWPORT9 viewport;
			device->GetViewport(&viewport);

			device->SetDepthStencilSurface((viewport.Width == runtime->mPresentParams.BackBufferWidth && viewport.Height == runtime->mPresentParams.BackBufferHeight) ? runtime->mDefaultDepthStencil : nullptr);

			const float pixelsize[4] = { 1.0f / viewport.Width, 1.0f / viewport.Height, -1.0f / viewport.Width, 1.0f / viewport.Height };

			// Setup shader constants
			device->SetVertexShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);
			device->SetVertexShaderConstantF(223, pixelsize, 1);
			device->SetPixelShaderConstantF(0, this->mEffect->mConstantStorage, this->mEffect->mConstantRegisterCount);
			device->SetPixelShaderConstantF(223, pixelsize, 1);

			// Draw triangle
			device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

			const_cast<D3D9Runtime *>(runtime.get())->Runtime::OnDraw(3);

			// Update shader resources
			for (IDirect3DSurface9 *target : pass.RT)
			{
				if (target == nullptr || target == runtime->mBackBufferResolved)
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
}