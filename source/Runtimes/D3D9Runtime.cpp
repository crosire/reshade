#include "Log.hpp"
#include "D3D9Runtime.hpp"
#include "FX\ParserNodes.hpp"
#include "GUI.hpp"
#include "Input.hpp"

#include <assert.h>
#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <nanovg_d3d9.h>
#include <unordered_set>
#include <boost\algorithm\string.hpp>

const D3DFORMAT D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));

// ---------------------------------------------------------------------------------------------------

namespace reshade
{
	namespace runtimes
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

			struct d3d9_texture : public texture
			{
				d3d9_texture() : TextureInterface(nullptr), SurfaceInterface(nullptr)
				{
				}
				~d3d9_texture()
				{
					SAFE_RELEASE(TextureInterface);
					SAFE_RELEASE(SurfaceInterface);
				}

				void change_data_source(datatype source, IDirect3DTexture9 *texture)
				{
					basetype = source;

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

						width = texdesc.Width;
						height = texdesc.Height;
						format = texture::pixelformat::unknown;
						levels = TextureInterface->GetLevelCount();
					}
					else
					{
						width = height = levels = 0;
						format = texture::pixelformat::unknown;
					}
				}

				IDirect3DTexture9 *TextureInterface;
				IDirect3DSurface9 *SurfaceInterface;
			};
			struct d3d9_sampler
			{
				DWORD States[12];
				d3d9_texture *Texture;
			};
			struct d3d9_pass
			{
				IDirect3DVertexShader9 *VS;
				IDirect3DPixelShader9 *PS;
				d3d9_sampler Samplers[16];
				DWORD SamplerCount;
				IDirect3DStateBlock9 *Stateblock;
				IDirect3DSurface9 *RT[8];
			};
			struct d3d9_technique : public technique
			{
				~d3d9_technique()
				{
					for (auto &pass : passes)
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

				std::vector<d3d9_pass> passes;
			};

			class d3d9_fx_compiler
			{
				d3d9_fx_compiler(const d3d9_fx_compiler &);
				d3d9_fx_compiler &operator=(const d3d9_fx_compiler &);

			public:
				d3d9_fx_compiler(const fx::nodetree &ast, bool skipoptimization = false) : _ast(ast), _runtime(nullptr), _is_fatal(false), _skip_shader_optimization(skipoptimization), _current_function(nullptr), _currentRegisterOffset(0)
				{
				}

				bool compile(d3d9_runtime *runtime, std::string &errors)
				{
					_runtime = runtime;

					_is_fatal = false;
					_errors.clear();

					_global_code.clear();

					for (auto structure : _ast.structs)
					{
						visit(_global_code, static_cast<fx::nodes::struct_declaration_node *>(structure));
					}

					for (auto uniform1 : _ast.uniforms)
					{
						const auto uniform = static_cast<fx::nodes::variable_declaration_node *>(uniform1);

						if (uniform->type.is_texture())
						{
							visit_texture(uniform);
						}
						else if (uniform->type.is_sampler())
						{
							visit_sampler(uniform);
						}
						else if (uniform->type.has_qualifier(fx::nodes::type_node::uniform_))
						{
							visit_uniform(uniform);
						}
						else
						{
							visit(_global_code, uniform);

							_global_code += ";\n";
						}
					}

					for (auto function1 : _ast.functions)
					{
						const auto function = static_cast<fx::nodes::function_declaration_node *>(function1);

						_current_function = function;

						visit(_functions[function].SourceCode, function);
					}

					for (auto technique : _ast.techniques)
					{
						visit_technique(static_cast<fx::nodes::technique_declaration_node *>(technique));
					}

					errors += _errors;

					return !_is_fatal;
				}

				static inline bool IsPowerOf2(int x)
				{
					return ((x > 0) && ((x & (x - 1)) == 0));
				}
				static inline D3DBLEND LiteralToBlend(unsigned int value)
				{
					switch (value)
					{
						case fx::nodes::pass_declaration_node::states::ZERO:
							return D3DBLEND_ZERO;
						case fx::nodes::pass_declaration_node::states::ONE:
							return D3DBLEND_ONE;
					}

					return static_cast<D3DBLEND>(value);
				}
				static inline D3DSTENCILOP LiteralToStencilOp(unsigned int value)
				{
					if (value == fx::nodes::pass_declaration_node::states::ZERO)
					{
						return D3DSTENCILOP_ZERO;
					}

					return static_cast<D3DSTENCILOP>(value);
				}
				static D3DFORMAT LiteralToFormat(unsigned int value, texture::pixelformat &name)
				{
					switch (value)
					{
						case fx::nodes::variable_declaration_node::properties::R8:
							name = texture::pixelformat::r8;
							return D3DFMT_A8R8G8B8;
						case fx::nodes::variable_declaration_node::properties::R16F:
							name = texture::pixelformat::r16f;
							return D3DFMT_R16F;
						case fx::nodes::variable_declaration_node::properties::R32F:
							name = texture::pixelformat::r32f;
							return D3DFMT_R32F;
						case fx::nodes::variable_declaration_node::properties::RG8:
							name = texture::pixelformat::rg8;
							return D3DFMT_A8R8G8B8;
						case fx::nodes::variable_declaration_node::properties::RG16:
							name = texture::pixelformat::rg16;
							return D3DFMT_G16R16;
						case fx::nodes::variable_declaration_node::properties::RG16F:
							name = texture::pixelformat::rg16f;
							return D3DFMT_G16R16F;
						case fx::nodes::variable_declaration_node::properties::RG32F:
							name = texture::pixelformat::rg32f;
							return D3DFMT_G32R32F;
						case fx::nodes::variable_declaration_node::properties::RGBA8:
							name = texture::pixelformat::rgba8;
							return D3DFMT_A8R8G8B8;  // D3DFMT_A8B8G8R8 appearently isn't supported by hardware very well
						case fx::nodes::variable_declaration_node::properties::RGBA16:
							name = texture::pixelformat::rgba16;
							return D3DFMT_A16B16G16R16;
						case fx::nodes::variable_declaration_node::properties::RGBA16F:
							name = texture::pixelformat::rgba16f;
							return D3DFMT_A16B16G16R16F;
						case fx::nodes::variable_declaration_node::properties::RGBA32F:
							name = texture::pixelformat::rgba32f;
							return D3DFMT_A32B32G32R32F;
						case fx::nodes::variable_declaration_node::properties::DXT1:
							name = texture::pixelformat::dxt1;
							return D3DFMT_DXT1;
						case fx::nodes::variable_declaration_node::properties::DXT3:
							name = texture::pixelformat::dxt3;
							return D3DFMT_DXT3;
						case fx::nodes::variable_declaration_node::properties::DXT5:
							name = texture::pixelformat::dxt5;
							return D3DFMT_DXT5;
						case fx::nodes::variable_declaration_node::properties::LATC1:
							name = texture::pixelformat::latc1;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
						case fx::nodes::variable_declaration_node::properties::LATC2:
							name = texture::pixelformat::latc2;
							return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
						default:
							name = texture::pixelformat::unknown;
							return D3DFMT_UNKNOWN;
					}
				}
				static std::string convert_semantic(const std::string &semantic)
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
				void error(const fx::location &location, const char *message, ...)
				{
					char formatted[512];

					va_list args;
					va_start(args, message);
					vsprintf_s(formatted, message, args);
					va_end(args);

					_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + formatted + '\n';
					_is_fatal = true;
				}
				void warning(const fx::location &location, const char *message, ...)
				{
					char formatted[512];

					va_list args;
					va_start(args, message);
					vsprintf_s(formatted, message, args);
					va_end(args);

					_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + formatted + '\n';
				}

				void visit_type(std::string &source, const fx::nodes::type_node &type)
				{
					if (type.has_qualifier(fx::nodes::type_node::static_))
						source += "static ";
					if (type.has_qualifier(fx::nodes::type_node::const_))
						source += "const ";
					if (type.has_qualifier(fx::nodes::type_node::volatile_))
						source += "volatile ";
					if (type.has_qualifier(fx::nodes::type_node::precise))
						source += "precise ";
					if (type.has_qualifier(fx::nodes::type_node::linear))
						source += "linear ";
					if (type.has_qualifier(fx::nodes::type_node::noperspective))
						source += "noperspective ";
					if (type.has_qualifier(fx::nodes::type_node::centroid))
						source += "centroid ";
					if (type.has_qualifier(fx::nodes::type_node::nointerpolation))
						source += "nointerpolation ";
					if (type.has_qualifier(fx::nodes::type_node::inout))
						source += "inout ";
					else if (type.has_qualifier(fx::nodes::type_node::in))
						source += "in ";
					else if (type.has_qualifier(fx::nodes::type_node::out))
						source += "out ";
					else if (type.has_qualifier(fx::nodes::type_node::uniform_))
						source += "uniform ";

					visit_type_class(source, type);
				}
				void visit_type_class(std::string &source, const fx::nodes::type_node &type)
				{
					switch (type.basetype)
					{
						case fx::nodes::type_node::void_:
							source += "void";
							break;
						case fx::nodes::type_node::bool_:
							source += "bool";
							break;
						case fx::nodes::type_node::int_:
							source += "int";
							break;
						case fx::nodes::type_node::uint_:
							source += "uint";
							break;
						case fx::nodes::type_node::float_:
							source += "float";
							break;
						case fx::nodes::type_node::sampler2d:
							source += "__sampler2D";
							break;
						case fx::nodes::type_node::struct_:
							visit_name(source, type.definition);
							break;
					}

					if (type.is_matrix())
					{
						source += std::to_string(type.rows) + "x" + std::to_string(type.cols);
					}
					else if (type.is_vector())
					{
						source += std::to_string(type.rows);
					}
				}
				inline void visit_name(std::string &source, const fx::nodes::declaration_node *declaration)
				{
					source += boost::replace_all_copy(declaration->Namespace, "::", "_NS") + declaration->name;
				}

				void visit(std::string &output, const fx::nodes::statement_node *node)
				{
					if (node == nullptr)
					{
						return;
					}

					switch (node->id)
					{
						case fx::nodeid::compound_statement:
							visit(output, static_cast<const fx::nodes::compound_statement_node *>(node));
							break;
						case fx::nodeid::declarator_list:
							visit(output, static_cast<const fx::nodes::declarator_list_node *>(node));
							break;
						case fx::nodeid::expression_statement:
							visit(output, static_cast<const fx::nodes::expression_statement_node *>(node));
							break;
						case fx::nodeid::if_statement:
							visit(output, static_cast<const fx::nodes::if_statement_node *>(node));
							break;
						case fx::nodeid::switch_statement:
							visit(output, static_cast<const fx::nodes::switch_statement_node *>(node));
							break;
						case fx::nodeid::for_statement:
							visit(output, static_cast<const fx::nodes::for_statement_node *>(node));
							break;
						case fx::nodeid::while_statement:
							visit(output, static_cast<const fx::nodes::while_statement_node *>(node));
							break;
						case fx::nodeid::return_statement:
							visit(output, static_cast<const fx::nodes::return_statement_node *>(node));
							break;
						case fx::nodeid::jump_statement:
							visit(output, static_cast<const fx::nodes::jump_statement_node *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}
				void visit(std::string &output, const fx::nodes::expression_node *node)
				{
					switch (node->id)
					{
						case fx::nodeid::lvalue_expression:
							visit(output, static_cast<const fx::nodes::lvalue_expression_node *>(node));
							break;
						case fx::nodeid::literal_expression:
							visit(output, static_cast<const fx::nodes::literal_expression_node *>(node));
							break;
						case fx::nodeid::expression_sequence:
							visit(output, static_cast<const fx::nodes::expression_sequence_node *>(node));
							break;
						case fx::nodeid::unary_expression:
							visit(output, static_cast<const fx::nodes::unary_expression_node *>(node));
							break;
						case fx::nodeid::binary_expression:
							visit(output, static_cast<const fx::nodes::binary_expression_node *>(node));
							break;
						case fx::nodeid::intrinsic_expression:
							visit(output, static_cast<const fx::nodes::intrinsic_expression_node *>(node));
							break;
						case fx::nodeid::conditional_expression:
							visit(output, static_cast<const fx::nodes::conditional_expression_node *>(node));
							break;
						case fx::nodeid::swizzle_expression:
							visit(output, static_cast<const fx::nodes::swizzle_expression_node *>(node));
							break;
						case fx::nodeid::field_expression:
							visit(output, static_cast<const fx::nodes::field_expression_node *>(node));
							break;
						case fx::nodeid::assignment_expression:
							visit(output, static_cast<const fx::nodes::assignment_expression_node *>(node));
							break;
						case fx::nodeid::call_expression:
							visit(output, static_cast<const fx::nodes::call_expression_node *>(node));
							break;
						case fx::nodeid::constructor_expression:
							visit(output, static_cast<const fx::nodes::constructor_expression_node *>(node));
							break;
						case fx::nodeid::initializer_list:
							visit(output, static_cast<const fx::nodes::initializer_list_node *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}

				void visit(std::string &output, const fx::nodes::compound_statement_node *node)
				{
					output += "{\n";

					for (auto statement : node->statement_list)
					{
						visit(output, statement);
					}

					output += "}\n";
				}
				void visit(std::string &output, const fx::nodes::declarator_list_node *node, bool singlestatement = false)
				{
					bool includetype = true;

					for (auto declarator : node->declarator_list)
					{
						visit(output, declarator, includetype);

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
				void visit(std::string &output, const fx::nodes::expression_statement_node *node)
				{
					visit(output, node->expression);

					output += ";\n";
				}
				void visit(std::string &output, const fx::nodes::if_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						output += '[' + attribute + ']';
					}

					output += "if (";

					visit(output, node->condition);
					
					output += ")\n";

					if (node->statement_when_true != nullptr)
					{
						visit(output, node->statement_when_true);
					}
					else
					{
						output += "\t;";
					}

					if (node->statement_when_false != nullptr)
					{
						output += "else\n";

						visit(output, node->statement_when_false);
					}
				}
				void visit(std::string &output, const fx::nodes::switch_statement_node *node)
				{
					warning(node->location, "switch statements do not currently support fallthrough in Direct3D9!");

					output += "[unroll] do { ";
					
					visit_type_class(output, node->test_expression->type);
						
					output += " __switch_condition = ";

					visit(output, node->test_expression);

					output += ";\n";

					visit(output, node->case_list[0]);

					for (size_t i = 1, count = node->case_list.size(); i < count; ++i)
					{
						output += "else ";

						visit(output, node->case_list[i]);
					}

					output += "} while (false);\n";
				}
				void visit(std::string &output, const fx::nodes::case_statement_node *node)
				{
					output += "if (";

					for (auto label : node->labels)
					{
						if (label == nullptr)
						{
							output += "true";
						}
						else
						{
							output += "__switch_condition == ";

							visit(output, label);
						}

						output += " || ";
					}

					output.erase(output.end() - 4, output.end());

					output += ")";

					visit(output, node->statement_list);
				}
				void visit(std::string &output, const fx::nodes::for_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						output += '[' + attribute + ']';
					}

					output += "for (";

					if (node->init_statement != nullptr)
					{
						if (node->init_statement->id == fx::nodeid::declarator_list)
						{
							visit(output, static_cast<fx::nodes::declarator_list_node *>(node->init_statement), true);

							output.erase(output.end() - 2, output.end());
						}
						else
						{
							visit(output, static_cast<fx::nodes::expression_statement_node *>(node->init_statement)->expression);
						}
					}

					output += "; ";
										
					if (node->condition != nullptr)
					{
						visit(output, node->condition);
					}

					output += "; ";

					if (node->increment_expression != nullptr)
					{
						visit(output, node->increment_expression);
					}

					output += ")\n";

					if (node->statement_list != nullptr)
					{
						visit(output, node->statement_list);
					}
					else
					{
						output += "\t;";
					}
				}
				void visit(std::string &output, const fx::nodes::while_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						output += '[' + attribute + ']';
					}

					if (node->is_do_while)
					{
						output += "do\n{\n";

						if (node->statement_list != nullptr)
						{
							visit(output, node->statement_list);
						}

						output += "}\nwhile (";

						visit(output, node->condition);

						output += ");\n";
					}
					else
					{
						output += "while (";
						
						visit(output, node->condition);
						
						output += ")\n";

						if (node->statement_list != nullptr)
						{
							visit(output, node->statement_list);
						}
						else
						{
							output += "\t;";
						}
					}
				}
				void visit(std::string &output, const fx::nodes::return_statement_node *node)
				{
					if (node->is_discard)
					{
						output += "discard";
					}
					else
					{
						output += "return";

						if (node->return_value != nullptr)
						{
							output += ' ';

							visit(output, node->return_value);
						}
					}

					output += ";\n";
				}
				void visit(std::string &output, const fx::nodes::jump_statement_node *node)
				{
					if (node->is_break)
					{
						output += "break;\n";
					}
					else if (node->is_continue)
					{
						output += "continue;\n";
					}
				}

				void visit(std::string &output, const fx::nodes::lvalue_expression_node *node)
				{
					visit_name(output, node->reference);

					if (node->reference->type.is_sampler() && (_samplers.find(node->reference->name) != _samplers.end()))
					{
						_functions.at(_current_function).SamplerDependencies.insert(node->reference);
					}
				}
				void visit(std::string &output, const fx::nodes::literal_expression_node *node)
				{
					if (!node->type.is_scalar())
					{
						visit_type_class(output, node->type);
						
						output += '(';
					}

					for (unsigned int i = 0; i < node->type.rows * node->type.cols; ++i)
					{
						switch (node->type.basetype)
						{
							case fx::nodes::type_node::bool_:
								output += node->value_int[i] ? "true" : "false";
								break;
							case fx::nodes::type_node::int_:
								output += std::to_string(node->value_int[i]);
								break;
							case fx::nodes::type_node::uint_:
								output += std::to_string(node->value_uint[i]);
								break;
							case fx::nodes::type_node::float_:
								output += std::to_string(node->value_float[i]) + "f";
								break;
						}

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					if (!node->type.is_scalar())
					{
						output += ')';
					}
				}
				void visit(std::string &output, const fx::nodes::expression_sequence_node *node)
				{
					output += '(';

					for (auto expression : node->expression_list)
					{
						visit(output, expression);

						output += ", ";
					}

					output.erase(output.end() - 2, output.end());

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::unary_expression_node *node)
				{
					std::string part1, part2;

					switch (node->op)
					{
						case fx::nodes::unary_expression_node::negate:
							part1 = '-';
							break;
						case fx::nodes::unary_expression_node::bitwise_not:
							part1 = "(4294967295 - ";
							part2 = ")";
							break;
						case fx::nodes::unary_expression_node::logical_not:
							part1 = '!';
							break;
						case fx::nodes::unary_expression_node::pre_increase:
							part1 = "++";
							break;
						case fx::nodes::unary_expression_node::pre_decrease:
							part1 = "--";
							break;
						case fx::nodes::unary_expression_node::post_increase:
							part2 = "++";
							break;
						case fx::nodes::unary_expression_node::post_decrease:
							part2 = "--";
							break;
						case fx::nodes::unary_expression_node::cast:
							visit_type_class(part1, node->type);
							part1 += '(';
							part2 = ')';
							break;
					}

					output += part1;
					visit(output, node->operand);
					output += part2;
				}
				void visit(std::string &output, const fx::nodes::binary_expression_node *node)
				{
					std::string part1, part2, part3;

					switch (node->op)
					{
						case fx::nodes::binary_expression_node::add:
							part1 = '(';
							part2 = " + ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::subtract:
							part1 = '(';
							part2 = " - ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::multiply:
							part1 = '(';
							part2 = " * ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::divide:
							part1 = '(';
							part2 = " / ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::modulo:
							part1 = '(';
							part2 = " % ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::less:
							part1 = '(';
							part2 = " < ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::greater:
							part1 = '(';
							part2 = " > ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::less_equal:
							part1 = '(';
							part2 = " <= ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::greater_equal:
							part1 = '(';
							part2 = " >= ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::equal:
							part1 = '(';
							part2 = " == ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::not_equal:
							part1 = '(';
							part2 = " != ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::left_shift:
							part1 = "((";
							part2 = ") * exp2(";
							part3 = "))";
							break;
						case fx::nodes::binary_expression_node::right_shift:
							part1 = "floor((";
							part2 = ") / exp2(";
							part3 = "))";
							break;
						case fx::nodes::binary_expression_node::bitwise_and:
							if (node->operands[1]->id == fx::nodeid::literal_expression && node->operands[1]->type.is_integral())
							{
								const unsigned int value = static_cast<const fx::nodes::literal_expression_node *>(node->operands[1])->value_uint[0];

								if (IsPowerOf2(value + 1))
								{
									output += "((" + std::to_string(value + 1) + ") * frac((";
									visit(output, node->operands[0]);
									output += ") / (" + std::to_string(value + 1) + ")))";
									return;
								}
								else if (IsPowerOf2(value))
								{
									output += "((((";
									visit(output, node->operands[0]);
									output += ") / (" + std::to_string(value) + ")) % 2) * " + std::to_string(value) + ")";
									return;
								}
							}
						case fx::nodes::binary_expression_node::bitwise_or:
						case fx::nodes::binary_expression_node::bitwise_xor:
							error(node->location, "bitwise operations are not supported in Direct3D9");
							return;
						case fx::nodes::binary_expression_node::logical_and:
							part1 = '(';
							part2 = " && ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::logical_or:
							part1 = '(';
							part2 = " || ";
							part3 = ')';
							break;
						case fx::nodes::binary_expression_node::element_extract:
							part2 = '[';
							part3 = ']';
							break;
					}

					output += part1;
					visit(output, node->operands[0]);
					output += part2;
					visit(output, node->operands[1]);
					output += part3;
				}
				void visit(std::string &output, const fx::nodes::intrinsic_expression_node *node)
				{
					std::string part1, part2, part3, part4, part5;

					switch (node->op)
					{
						case fx::nodes::intrinsic_expression_node::abs:
							part1 = "abs(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::acos:
							part1 = "acos(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::all:
							part1 = "all(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::any:
							part1 = "any(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_int2float:
						case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
						case fx::nodes::intrinsic_expression_node::bitcast_float2int:
						case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
							error(node->location, "'asint', 'asuint' and 'asfloat' are not supported in Direct3D9");
							return;
						case fx::nodes::intrinsic_expression_node::asin:
							part1 = "asin(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::atan:
							part1 = "atan(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::atan2:
							part1 = "atan2(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::ceil:
							part1 = "ceil(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::clamp:
							part1 = "clamp(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::cos:
							part1 = "cos(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::cosh:
							part1 = "cosh(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::cross:
							part1 = "cross(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::ddx:
							part1 = "ddx(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::ddy:
							part1 = "ddy(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::degrees:
							part1 = "degrees(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::determinant:
							part1 = "determinant(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::distance:
							part1 = "distance(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::dot:
							part1 = "dot(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::exp:
							part1 = "exp(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::exp2:
							part1 = "exp2(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::faceforward:
							part1 = "faceforward(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::floor:
							part1 = "floor(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::frac:
							part1 = "frac(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::frexp:
							part1 = "frexp(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::fwidth:
							part1 = "fwidth(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::ldexp:
							part1 = "ldexp(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::length:
							part1 = "length(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::lerp:
							part1 = "lerp(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::log:
							part1 = "log(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::log10:
							part1 = "log10(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::log2:
							part1 = "log2(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::mad:
							part1 = "((";
							part2 = ") * (";
							part3 = ") + (";
							part4 = "))";
							break;
						case fx::nodes::intrinsic_expression_node::max:
							part1 = "max(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::min:
							part1 = "min(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::modf:
							part1 = "modf(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::mul:
							part1 = "mul(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::normalize:
							part1 = "normalize(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::pow:
							part1 = "pow(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::radians:
							part1 = "radians(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::rcp:
							part1 = "(1.0f / ";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::reflect:
							part1 = "reflect(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::refract:
							part1 = "refract(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::round:
							part1 = "round(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::rsqrt:
							part1 = "rsqrt(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::saturate:
							part1 = "saturate(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::sign:
							part1 = "sign(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::sin:
							part1 = "sin(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::sincos:
							part1 = "sincos(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::sinh:
							part1 = "sinh(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::smoothstep:
							part1 = "smoothstep(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::sqrt:
							part1 = "sqrt(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::step:
							part1 = "step(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tan:
							part1 = "tan(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tanh:
							part1 = "tanh(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2d:
							part1 = "tex2D((";
							part2 = ").s, ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dfetch:
							part1 = "tex2D((";
							part2 = ").s, float2(";
							part3 = "))";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dgather:
							if (node->arguments[2]->id == fx::nodeid::literal_expression && node->arguments[2]->type.is_integral())
							{
								const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[2])->value_int[0];

								output += "__tex2Dgather" + std::to_string(component) + "(";
								visit(output, node->arguments[0]);
								output += ", ";
								visit(output, node->arguments[1]);
								output += ")";
							}
							else
							{
								error(node->location, "texture gather component argument has to be constant");
							}
							return;
						case fx::nodes::intrinsic_expression_node::tex2dgatheroffset:
							if (node->arguments[3]->id == fx::nodeid::literal_expression && node->arguments[3]->type.is_integral())
							{
								const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[3])->value_int[0];

								output += "__tex2Dgather" + std::to_string(component) + "offset(";
								visit(output, node->arguments[0]);
								output += ", ";
								visit(output, node->arguments[1]);
								output += ", ";
								visit(output, node->arguments[2]);
								output += ")";
							}
							else
							{
								error(node->location, "texture gather component argument has to be constant");
							}
							return;
						case fx::nodes::intrinsic_expression_node::tex2dgrad:
							part1 = "tex2Dgrad((";
							part2 = ").s, ";
							part3 = ", ";
							part4 = ", ";
							part5 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dlevel:
							part1 = "tex2Dlod((";
							part2 = ").s, ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dleveloffset:
							part1 = "__tex2Dlodoffset(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2doffset:
							part1 = "__tex2Doffset(";
							part2 = ", ";
							part3 = ", ";
							part4 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dproj:
							part1 = "tex2Dproj((";
							part2 = ").s, ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dsize:
							part1 = "__tex2Dsize(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::transpose:
							part1 = "transpose(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::trunc:
							part1 = "trunc(";
							part2 = ")";
							break;
					}

					output += part1;

					if (node->arguments[0] != nullptr)
					{
						visit(output, node->arguments[0]);
					}

					output += part2;

					if (node->arguments[1] != nullptr)
					{
						visit(output, node->arguments[1]);
					}

					output += part3;

					if (node->arguments[2] != nullptr)
					{
						visit(output, node->arguments[2]);
					}

					output += part4;

					if (node->arguments[3] != nullptr)
					{
						visit(output, node->arguments[3]);
					}

					output += part5;
				}
				void visit(std::string &output, const fx::nodes::conditional_expression_node *node)
				{
					output += '(';
					visit(output, node->condition);
					output += " ? ";
					visit(output, node->expression_when_true);
					output += " : ";
					visit(output, node->expression_when_false);
					output += ')';
				}
				void visit(std::string &output, const fx::nodes::swizzle_expression_node *node)
				{
					visit(output, node->operand);

					output += '.';

					if (node->operand->type.is_matrix())
					{
						const char swizzle[16][5] = { "_m00", "_m01", "_m02", "_m03", "_m10", "_m11", "_m12", "_m13", "_m20", "_m21", "_m22", "_m23", "_m30", "_m31", "_m32", "_m33" };

						for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
						{
							output += swizzle[node->mask[i]];
						}
					}
					else
					{
						const char swizzle[4] = { 'x', 'y', 'z', 'w' };

						for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
						{
							output += swizzle[node->mask[i]];
						}
					}
				}
				void visit(std::string &output, const fx::nodes::field_expression_node *node)
				{
					output += '(';

					visit(output, node->operand);

					output += '.';

					visit_name(output, node->field_reference);

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::assignment_expression_node *node)
				{
					std::string part1, part2, part3;

					switch (node->op)
					{
						case fx::nodes::assignment_expression_node::none:
							part2 = " = ";
							break;
						case fx::nodes::assignment_expression_node::add:
							part2 = " += ";
							break;
						case fx::nodes::assignment_expression_node::subtract:
							part2 = " -= ";
							break;
						case fx::nodes::assignment_expression_node::multiply:
							part2 = " *= ";
							break;
						case fx::nodes::assignment_expression_node::divide:
							part2 = " /= ";
							break;
						case fx::nodes::assignment_expression_node::modulo:
							part2 = " %= ";
							break;
						case fx::nodes::assignment_expression_node::left_shift:
							part1 = "((";
							part2 = ") *= pow(2, ";
							part3 = "))";
							break;
						case fx::nodes::assignment_expression_node::right_shift:
							part1 = "((";
							part2 = ") /= pow(2, ";
							part3 = "))";
							break;
						case fx::nodes::assignment_expression_node::bitwise_and:
						case fx::nodes::assignment_expression_node::bitwise_or:
						case fx::nodes::assignment_expression_node::bitwise_xor:
							error(node->location, "bitwise operations are not supported in Direct3D9");
							return;
					}

					output += '(';

					output += part1;
					visit(output, node->left);
					output += part2;
					visit(output, node->right);
					output += part3;

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::call_expression_node *node)
				{
					visit_name(output, node->callee);

					output += '(';

					if (!node->arguments.empty())
					{
						for (auto argument : node->arguments)
						{
							visit(output, argument);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';

					auto &info = _functions.at(_current_function);
					auto &infoCallee = _functions.at(node->callee);
					
					info.SamplerDependencies.insert(infoCallee.SamplerDependencies.begin(), infoCallee.SamplerDependencies.end());

					for (auto dependency : infoCallee.FunctionDependencies)
					{
						if (std::find(info.FunctionDependencies.begin(), info.FunctionDependencies.end(), dependency) == info.FunctionDependencies.end())
						{
							info.FunctionDependencies.push_back(dependency);
						}
					}

					if (std::find(info.FunctionDependencies.begin(), info.FunctionDependencies.end(), node->callee) == info.FunctionDependencies.end())
					{
						info.FunctionDependencies.push_back(node->callee);
					}
				}
				void visit(std::string &output, const fx::nodes::constructor_expression_node *node)
				{
					visit_type_class(output, node->type);

					output += '(';

					if (!node->arguments.empty())
					{
						for (auto argument : node->arguments)
						{
							visit(output, argument);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ')';
				}
				void visit(std::string &output, const fx::nodes::initializer_list_node *node)
				{
					output += "{ ";

					for (auto expression : node->values)
					{
						visit(output, expression);

						output += ", ";
					}

					output += " }";
				}

				void visit(std::string &output, const fx::nodes::struct_declaration_node *node)
				{
					output += "struct ";
					
					visit_name(output, node);

					output += "\n{\n";

					if (!node->field_list.empty())
					{
						for (auto field : node->field_list)
						{
							visit(output, field);

							output += ";\n";
						}
					}
					else
					{
						output += "float _dummy;\n";
					}

					output += "};\n";
				}
				void visit(std::string &output, const fx::nodes::variable_declaration_node *node, bool includetype = true, bool includesemantic = true)
				{
					if (includetype)
					{
						visit_type(output, node->type);

						output += ' ';
					}

					visit_name(output, node);

					if (node->type.is_array())
					{
						output += '[' + ((node->type.array_length > 0) ? std::to_string(node->type.array_length) : "") + ']';
					}

					if (includesemantic && !node->semantic.empty())
					{
						output += " : " + convert_semantic(node->semantic);
					}

					if (node->initializer_expression != nullptr)
					{
						output += " = ";

						visit(output, node->initializer_expression);
					}
				}
				void visit(std::string &output, const fx::nodes::function_declaration_node *node)
				{
					visit_type_class(output, node->return_type);
					
					output += ' ';

					visit_name(output, node);

					output += '(';

					if (!node->parameter_list.empty())
					{
						for (auto parameter : node->parameter_list)
						{
							visit(output, parameter, true, false);

							output += ", ";
						}

						output.erase(output.end() - 2, output.end());
					}

					output += ")\n";

					visit(output, node->definition);
				}

				template <typename T>
				void visit_annotation(const std::vector<fx::nodes::annotation_node> &annotations, T &object)
				{
					for (auto &annotation : annotations)
					{
						switch (annotation.value->type.basetype)
						{
							case fx::nodes::type_node::bool_:
							case fx::nodes::type_node::int_:
								object.annotations[annotation.name] = annotation.value->value_int;
								break;
							case fx::nodes::type_node::uint_:
								object.annotations[annotation.name] = annotation.value->value_uint;
								break;
							case fx::nodes::type_node::float_:
								object.annotations[annotation.name] = annotation.value->value_float;
								break;
							case fx::nodes::type_node::string_:
								object.annotations[annotation.name] = annotation.value->value_string;
								break;
						}
					}
				}
				void visit_texture(const fx::nodes::variable_declaration_node *node)
				{
					d3d9_texture *const obj = new d3d9_texture();
					obj->name = node->name;
					UINT width = obj->width = node->properties.Width;
					UINT height = obj->height = node->properties.Height;
					UINT levels = obj->levels = node->properties.MipLevels;
					const D3DFORMAT format = LiteralToFormat(node->properties.Format, obj->format);

					visit_annotation(node->annotations, *obj);

					if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
					{
						if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
						{
							warning(node->location, "texture properties on backbuffer textures are ignored");
						}

						obj->change_data_source(texture::datatype::backbuffer, _runtime->_backbuffer_texture);
					}
					else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
					{
						if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
						{
							warning(node->location, "texture properties on depthbuffer textures are ignored");
						}

						obj->change_data_source(texture::datatype::depthbuffer, _runtime->_depthstencil_texture);
					}
					else
					{
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
								warning(node->location, "autogenerated miplevels are not supported for this format");
							}
						}
						else if (levels == 0)
						{
							warning(node->location, "a texture cannot have 0 miplevels, changed it to 1");

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
							error(node->location, "internal texture creation failed with '%u'!", hr);
							return;
						}

						hr = obj->TextureInterface->GetSurfaceLevel(0, &obj->SurfaceInterface);

						assert(SUCCEEDED(hr));
					}

					_runtime->add_texture(obj);
				}
				void visit_sampler(const fx::nodes::variable_declaration_node *node)
				{
					if (node->properties.Texture == nullptr)
					{
						error(node->location, "sampler '%s' is missing required 'Texture' property", node->name);
						return;
					}

					d3d9_texture *const texture = static_cast<d3d9_texture *>(_runtime->get_texture(node->properties.Texture->name));

					if (texture == nullptr)
					{
						_is_fatal = true;
						return;
					}

					d3d9_sampler sampler;
					sampler.Texture = texture;
					sampler.States[D3DSAMP_ADDRESSU] = static_cast<D3DTEXTUREADDRESS>(node->properties.AddressU);
					sampler.States[D3DSAMP_ADDRESSV] = static_cast<D3DTEXTUREADDRESS>(node->properties.AddressV);
					sampler.States[D3DSAMP_ADDRESSW] = static_cast<D3DTEXTUREADDRESS>(node->properties.AddressW);
					sampler.States[D3DSAMP_BORDERCOLOR] = 0;
					sampler.States[D3DSAMP_MINFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->properties.MinFilter);
					sampler.States[D3DSAMP_MAGFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->properties.MagFilter);
					sampler.States[D3DSAMP_MIPFILTER] = static_cast<D3DTEXTUREFILTERTYPE>(node->properties.MipFilter);
					sampler.States[D3DSAMP_MIPMAPLODBIAS] = *reinterpret_cast<const DWORD *>(&node->properties.MipLODBias);
					sampler.States[D3DSAMP_MAXMIPLEVEL] = static_cast<DWORD>(std::max(0.0f, node->properties.MinLOD));
					sampler.States[D3DSAMP_MAXANISOTROPY] = node->properties.MaxAnisotropy;
					sampler.States[D3DSAMP_SRGBTEXTURE] = node->properties.SRGBTexture;

					_samplers[node->name] = sampler;
				}
				void visit_uniform(const fx::nodes::variable_declaration_node *node)
				{
					visit_type(_global_code, node->type);
					
					_global_code += ' ';

					visit_name(_global_code, node);

					if (node->type.is_array())
					{
						_global_code += '[' + ((node->type.array_length > 0) ? std::to_string(node->type.array_length) : "") + ']';
					}

					_global_code += " : register(c" + std::to_string(_currentRegisterOffset / 4) + ");\n";

					uniform *const obj = new uniform();
					obj->name = node->name;
					obj->rows = node->type.rows;
					obj->columns = node->type.cols;
					obj->elements = node->type.array_length;
					obj->storage_size = obj->rows * obj->columns * std::max(1u, obj->elements);

					switch (node->type.basetype)
					{
						case fx::nodes::type_node::bool_:
							obj->basetype = uniform::datatype::bool_;
							obj->storage_size *= sizeof(int);
							break;
						case fx::nodes::type_node::int_:
							obj->basetype = uniform::datatype::int_;
							obj->storage_size *= sizeof(int);
							break;
						case fx::nodes::type_node::uint_:
							obj->basetype = uniform::datatype::uint_;
							obj->storage_size *= sizeof(unsigned int);
							break;
						case fx::nodes::type_node::float_:
							obj->basetype = uniform::datatype::float_;
							obj->storage_size *= sizeof(float);
							break;
					}

					const UINT regsize = static_cast<UINT>(static_cast<float>(obj->storage_size) / sizeof(float));
					const UINT regalignment = 4 - (regsize % 4);

					obj->storage_offset = _currentRegisterOffset * sizeof(float);
					_currentRegisterOffset += regsize + regalignment;

					visit_annotation(node->annotations, *obj);

					if (_currentRegisterOffset * sizeof(float) >= _runtime->get_uniform_data_storage_size())
					{
						_runtime->enlarge_uniform_data_storage();
					}

					_runtime->_constant_register_count = _currentRegisterOffset / 4;

					if (node->initializer_expression != nullptr && node->initializer_expression->id == fx::nodeid::literal_expression)
					{
						CopyMemory(_runtime->get_uniform_data_storage() + obj->storage_offset, &static_cast<const fx::nodes::literal_expression_node *>(node->initializer_expression)->value_float, obj->storage_size);
					}
					else
					{
						ZeroMemory(_runtime->get_uniform_data_storage() + obj->storage_offset, obj->storage_size);
					}

					_runtime->add_uniform(obj);
				}
				void visit_technique(const fx::nodes::technique_declaration_node *node)
				{
					d3d9_technique *const obj = new d3d9_technique();
					obj->name = node->name;
					obj->pass_count = static_cast<unsigned int>(node->pass_list.size());

					visit_annotation(node->annotation_list, *obj);

					for (auto pass : node->pass_list)
					{
						visit_pass(pass, obj->passes);
					}

					_runtime->add_technique(obj);
				}
				void visit_pass(const fx::nodes::pass_declaration_node *node, std::vector<d3d9_pass> &passes)
				{
					d3d9_pass pass;
					ZeroMemory(&pass, sizeof(d3d9_pass));
					pass.RT[0] = _runtime->_backbuffer_resolved;

					std::string samplers;
					const char shaderTypes[2][3] = { "vs", "ps" };
					const fx::nodes::function_declaration_node *shaderFunctions[2] = { node->states.VertexShader, node->states.PixelShader };

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] == nullptr)
						{
							continue;
						}

						for (auto sampler : _functions.at(shaderFunctions[i]).SamplerDependencies)
						{
							pass.Samplers[pass.SamplerCount] = _samplers.at(sampler->name);
							const auto *const texture = sampler->properties.Texture;

							samplers += "sampler2D __Sampler";
							visit_name(samplers, sampler);
							samplers += " : register(s" + std::to_string(pass.SamplerCount++) + ");\n";
							samplers += "static const __sampler2D ";
							visit_name(samplers, sampler);
							samplers += " = { __Sampler";
							visit_name(samplers, sampler);

							if (texture->semantic == "COLOR" || texture->semantic == "SV_TARGET" || texture->semantic == "DEPTH" || texture->semantic == "SV_DEPTH")
							{
								samplers += ", float2(" + std::to_string(1.0f / _runtime->buffer_width()) + ", " + std::to_string(1.0f / _runtime->buffer_height()) + ")";
							}
							else
							{
								samplers += ", float2(" + std::to_string(1.0f / texture->properties.Width) + ", " + std::to_string(1.0f / texture->properties.Height) + ")";
							}

							samplers += " };\n";

							if (pass.SamplerCount == 16)
							{
								error(node->location, "maximum sampler count of 16 reached in pass '%s'", node->name.c_str());
								return;
							}
						}
					}

					for (unsigned int i = 0; i < 2; ++i)
					{
						if (shaderFunctions[i] != nullptr)
						{
							visit_pass_shader(shaderFunctions[i], shaderTypes[i], samplers, pass);
						}
					}

					IDirect3DDevice9 *const device = _runtime->_device;
				
					const HRESULT hr = device->BeginStateBlock();

					if (FAILED(hr))
					{
						error(node->location, "internal pass stateblock creation failed with '%u'!", hr);
						return;
					}

					device->SetVertexShader(pass.VS);
					device->SetPixelShader(pass.PS);

					device->SetRenderState(D3DRS_ZENABLE, node->states.DepthEnable);
					device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
					device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
					device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
					device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
					device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
					device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
					device->SetRenderState(D3DRS_SRCBLEND, LiteralToBlend(node->states.SrcBlend));
					device->SetRenderState(D3DRS_DESTBLEND, LiteralToBlend(node->states.DestBlend));
					device->SetRenderState(D3DRS_ZFUNC, static_cast<D3DCMPFUNC>(node->states.DepthFunc));
					device->SetRenderState(D3DRS_ALPHAREF, 0);
					device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
					device->SetRenderState(D3DRS_DITHERENABLE, FALSE);
					device->SetRenderState(D3DRS_FOGSTART, 0);
					device->SetRenderState(D3DRS_FOGEND, 1);
					device->SetRenderState(D3DRS_FOGDENSITY, 1);
					device->SetRenderState(D3DRS_ALPHABLENDENABLE, node->states.BlendEnable);
					device->SetRenderState(D3DRS_DEPTHBIAS, 0);
					device->SetRenderState(D3DRS_STENCILENABLE, node->states.StencilEnable);
					device->SetRenderState(D3DRS_STENCILPASS, LiteralToStencilOp(node->states.StencilOpPass));
					device->SetRenderState(D3DRS_STENCILFAIL, LiteralToStencilOp(node->states.StencilOpFail));
					device->SetRenderState(D3DRS_STENCILZFAIL, LiteralToStencilOp(node->states.StencilOpDepthFail));
					device->SetRenderState(D3DRS_STENCILFUNC, static_cast<D3DCMPFUNC>(node->states.StencilFunc));
					device->SetRenderState(D3DRS_STENCILREF, node->states.StencilRef);
					device->SetRenderState(D3DRS_STENCILMASK, node->states.StencilReadMask);
					device->SetRenderState(D3DRS_STENCILWRITEMASK, node->states.StencilWriteMask);
					device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
					device->SetRenderState(D3DRS_LOCALVIEWER, TRUE);
					device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
					device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
					device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
					device->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
					device->SetRenderState(D3DRS_COLORWRITEENABLE, node->states.RenderTargetWriteMask);
					device->SetRenderState(D3DRS_BLENDOP, static_cast<D3DBLENDOP>(node->states.BlendOp));
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
					device->SetRenderState(D3DRS_SRGBWRITEENABLE, node->states.SRGBWriteEnable);
					device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
					device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
					device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
					device->SetRenderState(D3DRS_BLENDOPALPHA, static_cast<D3DBLENDOP>(node->states.BlendOpAlpha));
					device->SetRenderState(D3DRS_FOGENABLE, FALSE );
					device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
					device->SetRenderState(D3DRS_LIGHTING, FALSE);

					device->EndStateBlock(&pass.Stateblock);

					D3DCAPS9 caps;
					device->GetDeviceCaps(&caps);

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->states.RenderTargets[i] == nullptr)
						{
							continue;
						}

						if (i > caps.NumSimultaneousRTs)
						{
							warning(node->location, "device only supports %u simultaneous render targets, but pass '%s' uses more, which are ignored", caps.NumSimultaneousRTs, node->name.c_str());
							break;
						}

						d3d9_texture *const texture = static_cast<d3d9_texture *>(_runtime->get_texture(node->states.RenderTargets[i]->name));

						if (texture == nullptr)
						{
							_is_fatal = true;
							return;
						}

						pass.RT[i] = texture->SurfaceInterface;
					}

					passes.push_back(std::move(pass));
				}
				void visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, const std::string &samplers, d3d9_pass &pass)
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
					source += _global_code;

					for (auto dependency : _functions.at(node).FunctionDependencies)
					{
						source += _functions.at(dependency).SourceCode;
					}

					source += _functions.at(node).SourceCode;

					std::string positionVariable, initialization;
					fx::nodes::type_node returnType = node->return_type;

					if (node->return_type.is_struct())
					{
						for (auto field : node->return_type.definition->field_list)
						{
							if (field->semantic == "SV_POSITION" || field->semantic == "POSITION")
							{
								positionVariable = "_return." + field->name;
								break;
							}
							else if ((boost::starts_with(field->semantic, "SV_TARGET") || boost::starts_with(field->semantic, "COLOR")) && field->type.rows != 4)
							{
								error(node->location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
								return;
							}
						}
					}
					else
					{
						if (node->return_semantic == "SV_POSITION" || node->return_semantic == "POSITION")
						{
							positionVariable = "_return";
						}
						else if (boost::starts_with(node->return_semantic, "SV_TARGET") || boost::starts_with(node->return_semantic, "COLOR"))
						{
							returnType.rows = 4;
						}
					}

					visit_type_class(source, returnType);
					
					source += " __main(";
				
					if (!node->parameter_list.empty())
					{
						for (auto parameter : node->parameter_list)
						{
							fx::nodes::type_node parameterType = parameter->type;

							if (parameter->type.has_qualifier(fx::nodes::type_node::out))
							{
								if (parameterType.is_struct())
								{
									for (auto field : parameterType.definition->field_list)
									{
										if (field->semantic == "SV_POSITION" || field->semantic == "POSITION")
										{
											positionVariable = parameter->name + '.' + field->name;
											break;
										}
										else if ((boost::starts_with(field->semantic, "SV_TARGET") || boost::starts_with(field->semantic, "COLOR")) && field->type.rows != 4)
										{
											error(node->location, "'SV_Target' must be a four-component vector when used inside structs in Direct3D9");
											return;
										}
									}
								}
								else
								{
									if (parameter->semantic == "SV_POSITION" || parameter->semantic == "POSITION")
									{
										positionVariable = parameter->name;
									}
									else if (boost::starts_with(parameter->semantic, "SV_TARGET") || boost::starts_with(parameter->semantic, "COLOR"))
									{
										parameterType.rows = 4;

										initialization += parameter->name + " = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
									}
								}
							}

							visit_type(source, parameterType);
						
							source += ' ' + parameter->name;

							if (parameterType.is_array())
							{
								source += '[' + ((parameterType.array_length >= 1) ? std::to_string(parameterType.array_length) : "") + ']';
							}

							if (!parameter->semantic.empty())
							{
								source += " : " + convert_semantic(parameter->semantic);
							}

							source += ", ";
						}

						source.erase(source.end() - 2, source.end());
					}

					source += ')';

					if (!node->return_semantic.empty())
					{
						source += " : " + convert_semantic(node->return_semantic);
					}

					source += "\n{\n";
					source += initialization;

					if (!node->return_type.is_void())
					{
						visit_type_class(source, returnType);
						
						source += " _return = ";
					}

					if (node->return_type.rows != returnType.rows)
					{
						source += "float4(";
					}

					visit_name(source, node);

					source += '(';

					if (!node->parameter_list.empty())
					{
						for (auto parameter : node->parameter_list)
						{
							source += parameter->name;

							if (boost::starts_with(parameter->semantic, "SV_TARGET") || boost::starts_with(parameter->semantic, "COLOR"))
							{
								source += '.';

								const char swizzle[] = { 'x', 'y', 'z', 'w' };

								for (unsigned int i = 0; i < parameter->type.rows; ++i)
								{
									source += swizzle[i];
								}
							}

							source += ", ";
						}

						source.erase(source.end() - 2, source.end());
					}

					source += ')';

					if (node->return_type.rows != returnType.rows)
					{
						for (unsigned int i = 0; i < 4 - node->return_type.rows; ++i)
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

					if (!node->return_type.is_void())
					{
						source += "return _return;\n";
					}

					source += "}\n";

					LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source.c_str() << "\n";

					UINT flags = 0;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (_skip_shader_optimization)
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
						_is_fatal = true;
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
						error(node->location, "internal shader creation failed with '%u'!", hr);
						return;
					}
				}

			private:
				struct Function
				{
					std::string SourceCode;
					std::unordered_set<const fx::nodes::variable_declaration_node *> SamplerDependencies;
					std::vector<const fx::nodes::function_declaration_node *> FunctionDependencies;
				};

				d3d9_runtime *_runtime;
				const fx::nodetree &_ast;
				bool _is_fatal, _skip_shader_optimization;
				std::string _errors;
				std::string _global_code;
				unsigned int _currentRegisterOffset;
				const fx::nodes::function_declaration_node *_current_function;
				std::unordered_map<std::string, d3d9_sampler> _samplers;
				std::unordered_map<const fx::nodes::function_declaration_node *, Function> _functions;
			};
		}

		// ---------------------------------------------------------------------------------------------------

		d3d9_runtime::d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : runtime(D3D_FEATURE_LEVEL_9_3), _device(device), _swapchain(swapchain), _d3d(nullptr), _stateblock(nullptr), _is_multisampling_enabled(false), _backbuffer_format(D3DFMT_UNKNOWN), _backbuffer(nullptr), _backbuffer_resolved(nullptr), _backbuffer_texture(nullptr), _backbuffer_texture_surface(nullptr), _depthstencil(nullptr), _depthstencil_replacement(nullptr), _depthstencil_texture(nullptr), _default_depthstencil(nullptr), _effect_triangle_buffer(nullptr), _effect_triangle_layout(nullptr), _constant_register_count(0)
		{
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

			_vendor_id = identifier.VendorId;
			_device_id = identifier.DeviceId;
			_behavior_flags = params.BehaviorFlags;
			_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, static_cast<DWORD>(8));
		}
		d3d9_runtime::~d3d9_runtime()
		{
			_device->Release();
			_swapchain->Release();
			_d3d->Release();
		}

		bool d3d9_runtime::on_init(const D3DPRESENT_PARAMETERS &pp)
		{
			_width = pp.BackBufferWidth;
			_height = pp.BackBufferHeight;
			_backbuffer_format = pp.BackBufferFormat;
			_is_multisampling_enabled = pp.MultiSampleType != D3DMULTISAMPLE_NONE;
			_input = input::register_window(pp.hDeviceWindow);

			#pragma region Get backbuffer surface
			HRESULT hr = _swapchain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &_backbuffer);

			assert(SUCCEEDED(hr));

			if (pp.MultiSampleType != D3DMULTISAMPLE_NONE || (pp.BackBufferFormat == D3DFMT_X8R8G8B8 || pp.BackBufferFormat == D3DFMT_X8B8G8R8))
			{
				switch (pp.BackBufferFormat)
				{
					case D3DFMT_X8R8G8B8:
						_backbuffer_format = D3DFMT_A8R8G8B8;
						break;
					case D3DFMT_X8B8G8R8:
						_backbuffer_format = D3DFMT_A8B8G8R8;
						break;
				}

				hr = _device->CreateRenderTarget(_width, _height, _backbuffer_format, D3DMULTISAMPLE_NONE, 0, FALSE, &_backbuffer_resolved, nullptr);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer resolve texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_backbuffer);

					return false;
				}
			}
			else
			{
				_backbuffer_resolved = _backbuffer;
				_backbuffer_resolved->AddRef();
			}
			#pragma endregion

			#pragma region Create backbuffer shader texture
			hr = _device->CreateTexture(_width, _height, 1, D3DUSAGE_RENDERTARGET, _backbuffer_format, D3DPOOL_DEFAULT, &_backbuffer_texture, nullptr);

			if (SUCCEEDED(hr))
			{
				_backbuffer_texture->GetSurfaceLevel(0, &_backbuffer_texture_surface);
			}
			else
			{
				LOG(TRACE) << "Failed to create backbuffer texture! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);

				return false;
			}
			#pragma endregion

			#pragma region Create default depthstencil surface
			hr = _device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_default_depthstencil, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_surface);

				return nullptr;
			}
			#pragma endregion

			#pragma region Create effect stateblock and objects
			hr = _device->CreateStateBlock(D3DSBT_ALL, &_stateblock);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create stateblock! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_surface);
				SAFE_RELEASE(_default_depthstencil);

				return false;
			}

			hr = _device->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effect_triangle_buffer, nullptr);

			if (SUCCEEDED(hr))
			{
				float *data = nullptr;

				hr = _effect_triangle_buffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

				assert(SUCCEEDED(hr));

				for (UINT i = 0; i < 3; ++i)
				{
					data[i] = static_cast<float>(i);
				}

				_effect_triangle_buffer->Unlock();
			}
			else
			{
				LOG(TRACE) << "Failed to create effect vertexbuffer! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_surface);
				SAFE_RELEASE(_default_depthstencil);
				SAFE_RELEASE(_stateblock);

				return false;
			}

			const D3DVERTEXELEMENT9 declaration[] =
			{
				{ 0, 0, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
				D3DDECL_END()
			};

			hr = _device->CreateVertexDeclaration(declaration, &_effect_triangle_layout);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create effect vertex declaration! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_surface);
				SAFE_RELEASE(_default_depthstencil);
				SAFE_RELEASE(_stateblock);
				SAFE_RELEASE(_effect_triangle_buffer);

				return false;
			}
			#pragma endregion

			_gui.reset(new gui(this, nvgCreateD3D9(_device, 0)));

			return runtime::on_init();
		}
		void d3d9_runtime::on_reset()
		{
			if (!_is_initialized)
			{
				return;
			}

			runtime::on_reset();

			// Destroy NanoVG
			NVGcontext *const nvg = _gui->context();

			_gui.reset();

			nvgDeleteD3D9(nvg);

			// Destroy resources
			SAFE_RELEASE(_stateblock);

			SAFE_RELEASE(_backbuffer);
			SAFE_RELEASE(_backbuffer_resolved);
			SAFE_RELEASE(_backbuffer_texture);
			SAFE_RELEASE(_backbuffer_texture_surface);

			SAFE_RELEASE(_depthstencil);
			SAFE_RELEASE(_depthstencil_replacement);
			SAFE_RELEASE(_depthstencil_texture);

			SAFE_RELEASE(_default_depthstencil);

			SAFE_RELEASE(_effect_triangle_buffer);
			SAFE_RELEASE(_effect_triangle_layout);

			// Clearing depth source table
			for (auto &it : _depth_source_table)
			{
				LOG(TRACE) << "Removing depthstencil " << it.first << " from list of possible depth candidates ...";

				it.first->Release();
			}

			_depth_source_table.clear();
		}
		void d3d9_runtime::on_present()
		{
			if (!_is_initialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}

			detect_depth_source();

			// Begin post processing
			HRESULT hr = _device->BeginScene();

			if (FAILED(hr))
			{
				return;
			}

			// Capture device state
			_stateblock->Capture();

			BOOL software_rendering_enabled;
			D3DVIEWPORT9 viewport;
			IDirect3DSurface9 *stateblock_rendertargets[8] = { nullptr }, *stateblock_depthstencil = nullptr;

			_device->GetViewport(&viewport);

			for (DWORD target = 0; target < _num_simultaneous_rendertargets; ++target)
			{
				_device->GetRenderTarget(target, &stateblock_rendertargets[target]);
			}

			_device->GetDepthStencilSurface(&stateblock_depthstencil);

			if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
			{
				software_rendering_enabled = _device->GetSoftwareVertexProcessing();

				_device->SetSoftwareVertexProcessing(FALSE);
			}

			// Resolve backbuffer
			if (_backbuffer_resolved != _backbuffer)
			{
				_device->StretchRect(_backbuffer, nullptr, _backbuffer_resolved, nullptr, D3DTEXF_NONE);
			}

			// Apply post processing
			on_apply_effect();

			// Reset rendertarget
			_device->SetRenderTarget(0, _backbuffer_resolved);
			_device->SetDepthStencilSurface(_default_depthstencil);
			_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

			// Apply presenting
			runtime::on_present();

			// Copy to backbuffer
			if (_backbuffer_resolved != _backbuffer)
			{
				_device->StretchRect(_backbuffer_resolved, nullptr, _backbuffer, nullptr, D3DTEXF_NONE);
			}

			// Apply previous device state
			_stateblock->Apply();

			for (DWORD target = 0; target < _num_simultaneous_rendertargets; ++target)
			{
				_device->SetRenderTarget(target, stateblock_rendertargets[target]);

				SAFE_RELEASE(stateblock_rendertargets[target]);
			}
			
			_device->SetDepthStencilSurface(stateblock_depthstencil);

			SAFE_RELEASE(stateblock_depthstencil);

			_device->SetViewport(&viewport);

			if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
			{
				_device->SetSoftwareVertexProcessing(software_rendering_enabled);
			}

			// End post processing
			_device->EndScene();
		}
		void d3d9_runtime::on_draw_call(D3DPRIMITIVETYPE type, UINT vertices)
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

			runtime::on_draw_call(vertices);

			IDirect3DSurface9 *depthstencil = nullptr;
			_device->GetDepthStencilSurface(&depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == _depthstencil_replacement)
				{
					depthstencil = _depthstencil;
				}

				const auto it = _depth_source_table.find(depthstencil);

				if (it != _depth_source_table.end())
				{
					it->second.drawcall_count = static_cast<FLOAT>(_stats.drawcalls);
					it->second.vertices_count += vertices;
				}
			}
		}
		void d3d9_runtime::on_apply_effect()
		{
			if (!_is_effect_compiled)
			{
				return;
			}

			_device->SetRenderTarget(0, _backbuffer_resolved);
			_device->SetDepthStencilSurface(nullptr);

			// Setup vertex input
			_device->SetStreamSource(0, _effect_triangle_buffer, 0, sizeof(float));
			_device->SetVertexDeclaration(_effect_triangle_layout);

			// Apply post processing
			runtime::on_apply_effect();
		}
		void d3d9_runtime::on_apply_effect_technique(const technique *technique)
		{
			runtime::on_apply_effect_technique(technique);

			bool is_default_depthstencil_cleared = false;

			// Setup shader constants
			_device->SetVertexShaderConstantF(0, reinterpret_cast<const float *>(_uniform_data_storage.data()), _constant_register_count);
			_device->SetPixelShaderConstantF(0, reinterpret_cast<const float *>(_uniform_data_storage.data()), _constant_register_count);

			for (const auto &pass : static_cast<const d3d9_technique *>(technique)->passes)
			{
				// Setup states
				pass.Stateblock->Apply();

				// Save backbuffer of previous pass
				_device->StretchRect(_backbuffer_resolved, nullptr, _backbuffer_texture_surface, nullptr, D3DTEXF_NONE);

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
				for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
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
					_device->SetDepthStencilSurface(_default_depthstencil);
				}
				else
				{
					_device->SetDepthStencilSurface(nullptr);
				}

				if (backbufferSizedRendertarget && !is_default_depthstencil_cleared)
				{
					is_default_depthstencil_cleared = true;

					// Clear depthstencil
					_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
				}
				else
				{
					_device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0.0f, 0);
				}

				// Draw triangle
				_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

				runtime::on_draw_call(3);

				// Update shader resources
				for (IDirect3DSurface9 *const target : pass.RT)
				{
					if (target == nullptr || target == _backbuffer_resolved)
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

		void d3d9_runtime::on_set_depthstencil_surface(IDirect3DSurface9 *&depthstencil)
		{
			if (_depth_source_table.find(depthstencil) == _depth_source_table.end())
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
				const depth_source_info info = { desc.Width, desc.Height };
				_depth_source_table.emplace(depthstencil, info);
			}

			if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
			{
				depthstencil = _depthstencil_replacement;
			}
		}
		void d3d9_runtime::on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil)
		{
			if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil_replacement)
			{
				depthstencil->Release();

				depthstencil = _depthstencil;
				depthstencil->AddRef();
			}
		}

		void d3d9_runtime::screenshot(unsigned char *buffer) const
		{
			if (_backbuffer_format != D3DFMT_X8R8G8B8 && _backbuffer_format != D3DFMT_X8B8G8R8 && _backbuffer_format != D3DFMT_A8R8G8B8 && _backbuffer_format != D3DFMT_A8B8G8R8)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbuffer_format << ".";
				return;
			}

			IDirect3DSurface9 *screenshot_surface = nullptr;

			HRESULT hr = _device->CreateOffscreenPlainSurface(_width, _height, _backbuffer_format, D3DPOOL_SYSTEMMEM, &screenshot_surface, nullptr);

			if (FAILED(hr))
			{
				return;
			}

			// Copy screenshot data to surface
			hr = _device->GetRenderTargetData(_backbuffer_resolved, screenshot_surface);

			if (FAILED(hr))
			{
				screenshot_surface->Release();
				return;
			}

			D3DLOCKED_RECT screenshot_lock;
			hr = screenshot_surface->LockRect(&screenshot_lock, nullptr, D3DLOCK_READONLY);

			if (FAILED(hr))
			{
				screenshot_surface->Release();
				return;
			}

			BYTE *pMem = buffer;
			BYTE *pLocked = static_cast<BYTE *>(screenshot_lock.pBits);

			const UINT pitch = _width * 4;

			// Copy screenshot data to memory
			for (UINT y = 0; y < _height; ++y)
			{
				CopyMemory(pMem, pLocked, std::min(pitch, static_cast<UINT>(screenshot_lock.Pitch)));

				for (UINT x = 0; x < pitch; x += 4)
				{
					pMem[x + 3] = 0xFF;

					if (_backbuffer_format == D3DFMT_A8R8G8B8 || _backbuffer_format == D3DFMT_X8R8G8B8)
					{
						std::swap(pMem[x + 0], pMem[x + 2]);
					}
				}

				pMem += pitch;
				pLocked += screenshot_lock.Pitch;
			}

			screenshot_surface->UnlockRect();

			screenshot_surface->Release();
		}
		bool d3d9_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors)
		{
			bool skip_optimization = false;

			for (const std::string &pragma : pragmas)
			{
				if (!boost::istarts_with(pragma, "reshade "))
				{
					continue;
				}

				const std::string command = pragma.substr(8);

				if (boost::iequals(command, "skipoptimization") || boost::iequals(command, "nooptimization"))
				{
					skip_optimization = true;
				}
			}

			d3d9_fx_compiler visitor(ast, skip_optimization);

			return visitor.compile(this, errors);
		}
		bool d3d9_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
		{
			d3d9_texture *const texture_impl = dynamic_cast<d3d9_texture *>(texture);

			assert(texture_impl != nullptr);
			assert(data != nullptr && size != 0);

			if (texture_impl->basetype != texture::datatype::image)
			{
				return false;
			}

			D3DSURFACE_DESC desc;
			texture_impl->TextureInterface->GetLevelDesc(0, &desc);

			IDirect3DTexture9 *mem_texture = nullptr;

			HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &mem_texture, nullptr);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create memory texture for texture updating! HRESULT is '" << hr << "'.";

				return false;
			}

			D3DLOCKED_RECT mem_lock;
			hr = mem_texture->LockRect(0, &mem_lock, nullptr, 0);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to lock memory texture for texture updating! HRESULT is '" << hr << "'.";

				mem_texture->Release();

				return false;
			}

			size = std::min<size_t>(size, mem_lock.Pitch * texture->height);
			BYTE *pLocked = static_cast<BYTE *>(mem_lock.pBits);

			switch (texture->format)
			{
				case texture::pixelformat::r8:
					for (size_t i = 0; i < size; i += 1, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = 0, pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case texture::pixelformat::rg8:
					for (size_t i = 0; i < size; i += 2, pLocked += 4)
					{
						pLocked[0] = 0, pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = 0;
					}
					break;
				case texture::pixelformat::rgba8:
					for (size_t i = 0; i < size; i += 4, pLocked += 4)
					{
						pLocked[0] = data[i + 2], pLocked[1] = data[i + 1], pLocked[2] = data[i], pLocked[3] = data[i + 3];
					}
					break;
				default:
					CopyMemory(pLocked, data, size);
					break;
			}

			mem_texture->UnlockRect(0);

			hr = _device->UpdateTexture(mem_texture, texture_impl->TextureInterface);

			mem_texture->Release();

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to update texture from memory texture! HRESULT is '" << hr << "'.";

				return false;
			}

			return true;
		}

		void d3d9_runtime::detect_depth_source()
		{
			static int cooldown = 0, traffic = 0;

			if (cooldown-- > 0)
			{
				traffic += s_network_upload > 0;
				return;
			}
			else
			{
				cooldown = 30;

				if (traffic > 10)
				{
					traffic = 0;
					create_depthstencil_replacement(nullptr);
					return;
				}
				else
				{
					traffic = 0;
				}
			}

			if (_is_multisampling_enabled || _depth_source_table.empty())
			{
				return;
			}

			depth_source_info best_info = { 0 };
			IDirect3DSurface9 *best_match = nullptr;

			for (auto it = _depth_source_table.begin(); it != _depth_source_table.end(); ++it)
			{
				if (GetRefCount(it->first) == 1)
				{
					LOG(TRACE) << "Removing depthstencil " << it->first << " from list of possible depth candidates ...";

					it->first->Release();

					it = _depth_source_table.erase(it);
					it = std::prev(it);
					continue;
				}

				if (it->second.drawcall_count == 0)
				{
					continue;
				}
				else if ((it->second.vertices_count * (1.2f - it->second.drawcall_count / _stats.drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _stats.drawcalls)))
				{
					best_match = it->first;
					best_info = it->second;
				}

				it->second.drawcall_count = it->second.vertices_count = 0;
			}

			if (_depthstencil != best_match)
			{
				LOG(TRACE) << "Switched depth source to depthstencil " << best_match << ".";

				create_depthstencil_replacement(best_match);
			}
		}
		bool d3d9_runtime::create_depthstencil_replacement(IDirect3DSurface9 *depthstencil)
		{
			SAFE_RELEASE(_depthstencil);
			SAFE_RELEASE(_depthstencil_replacement);
			SAFE_RELEASE(_depthstencil_texture);

			if (depthstencil != nullptr)
			{
				_depthstencil = depthstencil;
				_depthstencil->AddRef();

				D3DSURFACE_DESC desc;
				_depthstencil->GetDesc(&desc);

				if (desc.Format != D3DFMT_INTZ)
				{
					const HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_INTZ, D3DPOOL_DEFAULT, &_depthstencil_texture, nullptr);

					if (SUCCEEDED(hr))
					{
						_depthstencil_texture->GetSurfaceLevel(0, &_depthstencil_replacement);

						// Update auto depthstencil
						IDirect3DSurface9 *depthstencil = nullptr;
						_device->GetDepthStencilSurface(&depthstencil);

						if (depthstencil != nullptr)
						{
							depthstencil->Release();

							if (depthstencil == _depthstencil)
							{
								_device->SetDepthStencilSurface(_depthstencil_replacement);
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
					_depthstencil_replacement = _depthstencil;
					_depthstencil_replacement->AddRef();
					_depthstencil_replacement->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void **>(&_depthstencil_texture));
				}
			}

			// Update effect textures
			for (const auto &it : _textures)
			{
				d3d9_texture *texture = static_cast<d3d9_texture *>(it.get());

				if (texture->basetype == texture::datatype::depthbuffer)
				{
					texture->change_data_source(texture::datatype::depthbuffer, _depthstencil_texture);
				}
			}

			return true;
		}
	}
}
