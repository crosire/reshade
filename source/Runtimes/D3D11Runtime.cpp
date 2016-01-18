#include "Log.hpp"
#include "D3D11Runtime.hpp"
#include "FX\ParserNodes.hpp"
#include "GUI.hpp"
#include "Input.hpp"

#include <assert.h>
#include <d3dcompiler.h>
#include <nanovg_d3d11.h>
#include <boost\algorithm\string.hpp>

// ---------------------------------------------------------------------------------------------------

inline bool operator ==(const D3D11_SAMPLER_DESC &left, const D3D11_SAMPLER_DESC &right)
{
	return std::memcmp(&left, &right, sizeof(D3D11_SAMPLER_DESC)) == 0;
}

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

			struct d3d11_pass
			{
				ID3D11VertexShader *VS;
				ID3D11PixelShader *PS;
				ID3D11BlendState *BS;
				ID3D11DepthStencilState *DSS;
				UINT StencilRef;
				ID3D11RenderTargetView *RT[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
				ID3D11ShaderResourceView *RTSRV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
				D3D11_VIEWPORT Viewport;
				std::vector<ID3D11ShaderResourceView *> SRV;
			};
			struct d3d11_technique : public technique
			{
				~d3d11_technique()
				{
					for (auto &pass : passes)
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

				std::vector<d3d11_pass> passes;
			};
			struct d3d11_texture : public texture
			{
				d3d11_texture() : ShaderRegister(0), TextureInterface(nullptr), ShaderResourceView(), RenderTargetView()
				{
				}
				~d3d11_texture()
				{
					SAFE_RELEASE(RenderTargetView[0]);
					SAFE_RELEASE(RenderTargetView[1]);
					SAFE_RELEASE(ShaderResourceView[0]);
					SAFE_RELEASE(ShaderResourceView[1]);

					SAFE_RELEASE(TextureInterface);
				}

				void change_data_source(datatype source, d3d11_runtime *runtime, ID3D11ShaderResourceView *srv, ID3D11ShaderResourceView *srvSRGB)
				{
					basetype = source;

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
						ShaderResourceView[0]->GetResource(reinterpret_cast<ID3D11Resource **>(&TextureInterface));
						ShaderResourceView[1] = srvSRGB;
						ShaderResourceView[1]->AddRef();

						D3D11_TEXTURE2D_DESC texdesc;
						TextureInterface->GetDesc(&texdesc);

						width = texdesc.Width;
						height = texdesc.Height;
						format = texture::pixelformat::unknown;
						levels = texdesc.MipLevels;
					}
					else
					{
						width = height = levels = 0;
						format = texture::pixelformat::unknown;
					}

					// Update techniques shader resourceviews
					for (auto &technique : runtime->GetTechniques())
					{
						for (auto &pass : static_cast<d3d11_technique *>(technique.get())->passes)
						{
							pass.SRV[ShaderRegister] = ShaderResourceView[0];
							pass.SRV[ShaderRegister + 1] = ShaderResourceView[1];
						}
					}
				}

				size_t ShaderRegister;
				ID3D11Texture2D *TextureInterface;
				ID3D11ShaderResourceView *ShaderResourceView[2];
				ID3D11RenderTargetView *RenderTargetView[2];
			};

			class d3d11_fx_compiler
			{
				d3d11_fx_compiler(const d3d11_fx_compiler &);
				d3d11_fx_compiler &operator=(const d3d11_fx_compiler &);

			public:
				d3d11_fx_compiler(const fx::nodetree &ast, bool skipoptimization = false) : _ast(ast), _runtime(nullptr), _is_fatal(false), _skip_shader_optimization(skipoptimization), _is_in_parameter_block(false), _is_in_function_block(false), _is_in_for_initialization(0), _current_global_size(0)
				{
				}

				bool compile(d3d11_runtime *runtime, std::string &errors)
				{
					_runtime = runtime;
					_errors.clear();
					_is_fatal = false;
					_current_source.clear();

					for (auto type : _ast.structs)
					{
						visit(static_cast<fx::nodes::struct_declaration_node *>(type));
					}
					for (auto uniform : _ast.uniforms)
					{
						visit(static_cast<fx::nodes::variable_declaration_node *>(uniform));
					}
					for (auto function : _ast.functions)
					{
						visit(static_cast<fx::nodes::function_declaration_node *>(function));
					}
					for (auto technique : _ast.techniques)
					{
						visit(static_cast<fx::nodes::technique_declaration_node *>(technique));
					}

					if (_current_global_size != 0)
					{
						CD3D11_BUFFER_DESC globalsDesc(RoundToMultipleOf16(_current_global_size), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
						D3D11_SUBRESOURCE_DATA globalsInitial;
						globalsInitial.pSysMem = _runtime->get_uniform_data_storage();
						globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = _current_global_size;
						_runtime->_device->CreateBuffer(&globalsDesc, &globalsInitial, &_runtime->_constant_buffer);
					}

					errors += _errors;

					return !_is_fatal;
				}

				static inline UINT RoundToMultipleOf16(UINT size)
				{
					return (size + 15) & ~15;
				}
				static D3D11_STENCIL_OP LiteralToStencilOp(unsigned int value)
				{
					if (value == fx::nodes::pass_declaration_node::states::ZERO)
					{
						return D3D11_STENCIL_OP_ZERO;
					}
							
					return static_cast<D3D11_STENCIL_OP>(value);
				}
				static D3D11_BLEND LiteralToBlend(unsigned int value)
				{
					switch (value)
					{
						case fx::nodes::pass_declaration_node::states::ZERO:
							return D3D11_BLEND_ZERO;
						case fx::nodes::pass_declaration_node::states::ONE:
							return D3D11_BLEND_ONE;
					}

					return static_cast<D3D11_BLEND>(value);
				}
				static DXGI_FORMAT LiteralToFormat(unsigned int value, texture::pixelformat &name)
				{
					switch (value)
					{
						case fx::nodes::variable_declaration_node::properties::R8:
							name = texture::pixelformat::r8;
							return DXGI_FORMAT_R8_UNORM;
						case fx::nodes::variable_declaration_node::properties::R16F:
							name = texture::pixelformat::r16f;
							return DXGI_FORMAT_R16_FLOAT;
						case fx::nodes::variable_declaration_node::properties::R32F:
							name = texture::pixelformat::r32f;
							return DXGI_FORMAT_R32_FLOAT;
						case fx::nodes::variable_declaration_node::properties::RG8:
							name = texture::pixelformat::rg8;
							return DXGI_FORMAT_R8G8_UNORM;
						case fx::nodes::variable_declaration_node::properties::RG16:
							name = texture::pixelformat::rg16;
							return DXGI_FORMAT_R16G16_UNORM;
						case fx::nodes::variable_declaration_node::properties::RG16F:
							name = texture::pixelformat::rg16f;
							return DXGI_FORMAT_R16G16_FLOAT;
						case fx::nodes::variable_declaration_node::properties::RG32F:
							name = texture::pixelformat::rg32f;
							return DXGI_FORMAT_R32G32_FLOAT;
						case fx::nodes::variable_declaration_node::properties::RGBA8:
							name = texture::pixelformat::rgba8;
							return DXGI_FORMAT_R8G8B8A8_TYPELESS;
						case fx::nodes::variable_declaration_node::properties::RGBA16:
							name = texture::pixelformat::rgba16;
							return DXGI_FORMAT_R16G16B16A16_UNORM;
						case fx::nodes::variable_declaration_node::properties::RGBA16F:
							name = texture::pixelformat::rgba16f;
							return DXGI_FORMAT_R16G16B16A16_FLOAT;
						case fx::nodes::variable_declaration_node::properties::RGBA32F:
							name = texture::pixelformat::rgba32f;
							return DXGI_FORMAT_R32G32B32A32_FLOAT;
						case fx::nodes::variable_declaration_node::properties::DXT1:
							name = texture::pixelformat::dxt1;
							return DXGI_FORMAT_BC1_TYPELESS;
						case fx::nodes::variable_declaration_node::properties::DXT3:
							name = texture::pixelformat::dxt3;
							return DXGI_FORMAT_BC2_TYPELESS;
						case fx::nodes::variable_declaration_node::properties::DXT5:
							name = texture::pixelformat::dxt5;
							return DXGI_FORMAT_BC3_TYPELESS;
						case fx::nodes::variable_declaration_node::properties::LATC1:
							name = texture::pixelformat::latc1;
							return DXGI_FORMAT_BC4_UNORM;
						case fx::nodes::variable_declaration_node::properties::LATC2:
							name = texture::pixelformat::latc2;
							return DXGI_FORMAT_BC5_UNORM;
						default:
							name = texture::pixelformat::unknown;
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
				static size_t D3D11_SAMPLER_DESC_HASH(const D3D11_SAMPLER_DESC &s) 
				{
					const unsigned char *p = reinterpret_cast<const unsigned char *>(&s);
					size_t h = 2166136261;

					for (size_t i = 0; i < sizeof(D3D11_SAMPLER_DESC); ++i)
					{
						h = (h * 16777619) ^ p[i];
					}

					return h;
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

				static std::string convert_semantic(const std::string &semantic)
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
				std::string print_type(const fx::nodes::type_node &type)
				{
					std::string res;

					switch (type.basetype)
					{
						case fx::nodes::type_node::datatype_void:
							res += "void";
							break;
						case fx::nodes::type_node::datatype_bool:
							res += "bool";
							break;
						case fx::nodes::type_node::datatype_int:
							res += "int";
							break;
						case fx::nodes::type_node::datatype_uint:
							res += "uint";
							break;
						case fx::nodes::type_node::datatype_float:
							res += "float";
							break;
						case fx::nodes::type_node::datatype_sampler:
							res += "__sampler2D";
							break;
						case fx::nodes::type_node::datatype_struct:
							res += print_name(type.definition);
							break;
					}

					if (type.is_matrix())
					{
						res += std::to_string(type.rows) + "x" + std::to_string(type.cols);
					}
					else if (type.is_vector())
					{
						res += std::to_string(type.rows);
					}

					return res;
				}
				std::string print_type_with_qualifiers(const fx::nodes::type_node &type)
				{
					std::string qualifiers;

					if (type.has_qualifier(fx::nodes::type_node::qualifier_extern))
						qualifiers += "extern ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_static))
						qualifiers += "static ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_const))
						qualifiers += "const ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_volatile))
						qualifiers += "volatile ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_precise))
						qualifiers += "precise ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_linear))
						qualifiers += "linear ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_noperspective))
						qualifiers += "noperspective ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_centroid))
						qualifiers += "centroid ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_nointerpolation))
						qualifiers += "nointerpolation ";
					if (type.has_qualifier(fx::nodes::type_node::qualifier_inout))
						qualifiers += "inout ";
					else if (type.has_qualifier(fx::nodes::type_node::qualifier_in))
						qualifiers += "in ";
					else if (type.has_qualifier(fx::nodes::type_node::qualifier_out))
						qualifiers += "out ";
					else if (type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
						qualifiers += "uniform ";

					return qualifiers + print_type(type);
				}
				inline std::string print_name(const fx::nodes::declaration_node *declaration)
				{
					return boost::replace_all_copy(declaration->Namespace, "::", "_NS") + declaration->name;
				}

				void visit(const fx::nodes::statement_node *node)
				{
					if (node == nullptr)
					{
						return;
					}

					switch (node->id)
					{
						case fx::nodeid::compound_statement:
							visit(static_cast<const fx::nodes::compound_statement_node *>(node));
							break;
						case fx::nodeid::declarator_list:
							visit(static_cast<const fx::nodes::declarator_list_node *>(node));
							break;
						case fx::nodeid::expression_statement:
							visit(static_cast<const fx::nodes::expression_statement_node *>(node));
							break;
						case fx::nodeid::if_statement:
							visit(static_cast<const fx::nodes::if_statement_node *>(node));
							break;
						case fx::nodeid::switch_statement:
							visit(static_cast<const fx::nodes::switch_statement_node *>(node));
							break;
						case fx::nodeid::for_statement:
							visit(static_cast<const fx::nodes::for_statement_node *>(node));
							break;
						case fx::nodeid::while_statement:
							visit(static_cast<const fx::nodes::while_statement_node *>(node));
							break;
						case fx::nodeid::return_statement:
							visit(static_cast<const fx::nodes::return_statement_node *>(node));
							break;
						case fx::nodeid::jump_statement:
							visit(static_cast<const fx::nodes::jump_statement_node *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}
				void visit(const fx::nodes::expression_node *node)
				{
					switch (node->id)
					{
						case fx::nodeid::lvalue_expression:
							visit(static_cast<const fx::nodes::lvalue_expression_node *>(node));
							break;
						case fx::nodeid::literal_expression:
							visit(static_cast<const fx::nodes::literal_expression_node *>(node));
							break;
						case fx::nodeid::expression_sequence:
							visit(static_cast<const fx::nodes::expression_sequence_node *>(node));
							break;
						case fx::nodeid::unary_expression:
							visit(static_cast<const fx::nodes::unary_expression_node *>(node));
							break;
						case fx::nodeid::binary_expression:
							visit(static_cast<const fx::nodes::binary_expression_node *>(node));
							break;
						case fx::nodeid::intrinsic_expression:
							visit(static_cast<const fx::nodes::intrinsic_expression_node *>(node));
							break;
						case fx::nodeid::conditional_expression:
							visit(static_cast<const fx::nodes::conditional_expression_node *>(node));
							break;
						case fx::nodeid::swizzle_expression:
							visit(static_cast<const fx::nodes::swizzle_expression_node *>(node));
							break;
						case fx::nodeid::field_expression:
							visit(static_cast<const fx::nodes::field_expression_node *>(node));
							break;
						case fx::nodeid::assignment_expression:
							visit(static_cast<const fx::nodes::assignment_expression_node *>(node));
							break;
						case fx::nodeid::call_expression:
							visit(static_cast<const fx::nodes::call_expression_node *>(node));
							break;
						case fx::nodeid::constructor_expression:
							visit(static_cast<const fx::nodes::constructor_expression_node *>(node));
							break;
						case fx::nodeid::initializer_list:
							visit(static_cast<const fx::nodes::initializer_list_node *>(node));
							break;
						default:
							assert(false);
							break;
					}
				}

				void visit(const fx::nodes::compound_statement_node *node)
				{
					_current_source += "{\n";

					for (auto statement : node->statement_list)
					{
						visit(statement);
					}

					_current_source += "}\n";
				}
				void visit(const fx::nodes::declarator_list_node *node)
				{
					for (auto declarator : node->declarator_list)
					{
						visit(declarator);

						if (_is_in_for_initialization)
						{
							_current_source += ", ";
							_is_in_for_initialization++;
						}
						else
						{
							_current_source += ";\n";
						}
					}
				}
				void visit(const fx::nodes::expression_statement_node *node)
				{
					visit(node->expression);

					_current_source += ";\n";
				}
				void visit(const fx::nodes::if_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						_current_source += '[';
						_current_source += attribute;
						_current_source += ']';
					}

					_current_source += "if (";
					visit(node->condition);
					_current_source += ")\n";

					if (node->statement_when_true != nullptr)
					{
						visit(node->statement_when_true);
					}
					else
					{
						_current_source += "\t;";
					}
					
					if (node->statement_when_false != nullptr)
					{
						_current_source += "else\n";
						
						visit(node->statement_when_false);
					}
				}
				void visit(const fx::nodes::switch_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						_current_source += '[';
						_current_source += attribute;
						_current_source += ']';
					}

					_current_source += "switch (";
					visit(node->test_expression);
					_current_source += ")\n{\n";

					for (auto cases : node->case_list)
					{
						visit(cases);
					}

					_current_source += "}\n";
				}
				void visit(const fx::nodes::case_statement_node *node)
				{
					for (auto label : node->labels)
					{
						if (label == nullptr)
						{
							_current_source += "default";
						}
						else
						{
							_current_source += "case ";

							visit(label);
						}

						_current_source += ":\n";
					}

					visit(node->statement_list);
				}
				void visit(const fx::nodes::for_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						_current_source += '[';
						_current_source += attribute;
						_current_source += ']';
					}

					_current_source += "for (";

					if (node->init_statement != nullptr)
					{
						_is_in_for_initialization = 1;

						visit(node->init_statement);

						_is_in_for_initialization = 0;

						_current_source.pop_back();
						_current_source.pop_back();
					}

					_current_source += "; ";
										
					if (node->condition != nullptr)
					{
						visit(node->condition);
					}

					_current_source += "; ";

					if (node->increment_expression != nullptr)
					{
						visit(node->increment_expression);
					}

					_current_source += ")\n";

					if (node->statement_list != nullptr)
					{
						visit(node->statement_list);
					}
					else
					{
						_current_source += "\t;";
					}
				}
				void visit(const fx::nodes::while_statement_node *node)
				{
					for (auto &attribute : node->attributes)
					{
						_current_source += '[';
						_current_source += attribute;
						_current_source += ']';
					}

					if (node->is_do_while)
					{
						_current_source += "do\n{\n";

						if (node->statement_list != nullptr)
						{
							visit(node->statement_list);
						}

						_current_source += "}\n";
						_current_source += "while (";
						visit(node->condition);
						_current_source += ");\n";
					}
					else
					{
						_current_source += "while (";
						visit(node->condition);
						_current_source += ")\n";

						if (node->statement_list != nullptr)
						{
							visit(node->statement_list);
						}
						else
						{
							_current_source += "\t;";
						}
					}
				}
				void visit(const fx::nodes::return_statement_node *node)
				{
					if (node->is_discard)
					{
						_current_source += "discard";
					}
					else
					{
						_current_source += "return";

						if (node->return_value != nullptr)
						{
							_current_source += ' ';

							visit(node->return_value);
						}
					}

					_current_source += ";\n";
				}
				void visit(const fx::nodes::jump_statement_node *node)
				{
					if (node->is_break)
					{
						_current_source += "break;\n";

					}
					else if (node->is_continue)
					{
						_current_source += "continue;\n";
					}
				}

				void visit(const fx::nodes::lvalue_expression_node *node)
				{
					_current_source += print_name(node->reference);
				}
				void visit(const fx::nodes::literal_expression_node *node)
				{
					if (!node->type.is_scalar())
					{
						_current_source += print_type(node->type);
						_current_source += '(';
					}

					for (unsigned int i = 0; i < node->type.rows * node->type.cols; ++i)
					{
						switch (node->type.basetype)
						{
							case fx::nodes::type_node::datatype_bool:
								_current_source += node->value_int[i] ? "true" : "false";
								break;
							case fx::nodes::type_node::datatype_int:
								_current_source += std::to_string(node->value_int[i]);
								break;
							case fx::nodes::type_node::datatype_uint:
								_current_source += std::to_string(node->value_uint[i]);
								break;
							case fx::nodes::type_node::datatype_float:
								_current_source += std::to_string(node->value_float[i]) + "f";
								break;
						}

						_current_source += ", ";
					}

					_current_source.pop_back();
					_current_source.pop_back();

					if (!node->type.is_scalar())
					{
						_current_source += ')';
					}
				}
				void visit(const fx::nodes::expression_sequence_node *node)
				{
					_current_source += '(';

					for (auto expression : node->expression_list)
					{
						visit(expression);

						_current_source += ", ";
					}
					
					_current_source.pop_back();
					_current_source.pop_back();

					_current_source += ')';
				}
				void visit(const fx::nodes::unary_expression_node *node)
				{
					std::string part1, part2;

					switch (node->op)
					{
						case fx::nodes::unary_expression_node::negate:
							part1 = '-';
							break;
						case fx::nodes::unary_expression_node::bitwise_not:
							part1 = "~";
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
							part1 = print_type(node->type) + '(';
							part2 = ')';
							break;
					}

					_current_source += part1;
					visit(node->operand);
					_current_source += part2;
				}
				void visit(const fx::nodes::binary_expression_node *node)
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
							part1 = "(";
							part2 = " << ";
							part3 = ")";
							break;
						case fx::nodes::binary_expression_node::right_shift:
							part1 = "(";
							part2 = " >> ";
							part3 = ")";
							break;
						case fx::nodes::binary_expression_node::bitwise_and:
							part1 = "(";
							part2 = " & ";
							part3 = ")";
							break;
						case fx::nodes::binary_expression_node::bitwise_or:
							part1 = "(";
							part2 = " | ";
							part3 = ")";
							break;
						case fx::nodes::binary_expression_node::bitwise_xor:
							part1 = "(";
							part2 = " ^ ";
							part3 = ")";
							break;
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

					_current_source += part1;
					visit(node->operands[0]);
					_current_source += part2;
					visit(node->operands[1]);
					_current_source += part3;
				}
				void visit(const fx::nodes::intrinsic_expression_node *node)
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
							part1 = "asfloat(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_uint2float:
							part1 = "asfloat(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::asin:
							part1 = "asin(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_float2int:
							part1 = "asint(";
							part2 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::bitcast_float2uint:
							part1 = "asuint(";
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
							part1 = "__tex2D(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dfetch:
							part1 = "__tex2Dfetch(";
							part2 = ", ";
							part3 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dgather:
							if (node->arguments[2]->id == fx::nodeid::literal_expression && node->arguments[2]->type.is_integral())
							{
								const int component = static_cast<const fx::nodes::literal_expression_node *>(node->arguments[2])->value_int[0];

								_current_source += "__tex2Dgather" + std::to_string(component) + "(";
								visit(node->arguments[0]);
								_current_source += ", ";
								visit(node->arguments[1]);
								_current_source += ")";
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

								_current_source += "__tex2Dgatheroffset" + std::to_string(component) + "(";
								visit(node->arguments[0]);
								_current_source += ", ";
								visit(node->arguments[1]);
								_current_source += ", ";
								visit(node->arguments[2]);
								_current_source += ")";
							}
							else
							{
								error(node->location, "texture gather component argument has to be constant");
							}
							return;
						case fx::nodes::intrinsic_expression_node::tex2dgrad:
							part1 = "__tex2Dgrad(";
							part2 = ", ";
							part3 = ", ";
							part4 = ", ";
							part5 = ")";
							break;
						case fx::nodes::intrinsic_expression_node::tex2dlevel:
							part1 = "__tex2Dlod(";
							part2 = ", ";
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
							part1 = "__tex2Dproj(";
							part2 = ", ";
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

					_current_source += part1;

					if (node->arguments[0] != nullptr)
					{
						visit(node->arguments[0]);
					}

					_current_source += part2;

					if (node->arguments[1] != nullptr)
					{
						visit(node->arguments[1]);
					}

					_current_source += part3;

					if (node->arguments[2] != nullptr)
					{
						visit(node->arguments[2]);
					}

					_current_source += part4;

					if (node->arguments[3] != nullptr)
					{
						visit(node->arguments[3]);
					}

					_current_source += part5;
				}
				void visit(const fx::nodes::conditional_expression_node *node)
				{
					_current_source += '(';
					visit(node->condition);
					_current_source += " ? ";
					visit(node->expression_when_true);
					_current_source += " : ";
					visit(node->expression_when_false);
					_current_source += ')';
				}
				void visit(const fx::nodes::swizzle_expression_node *node)
				{
					visit(node->operand);

					_current_source += '.';

					if (node->operand->type.is_matrix())
					{
						const char swizzle[16][5] =
						{
							"_m00", "_m01", "_m02", "_m03",
							"_m10", "_m11", "_m12", "_m13",
							"_m20", "_m21", "_m22", "_m23",
							"_m30", "_m31", "_m32", "_m33"
						};

						for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
						{
							_current_source += swizzle[node->mask[i]];
						}
					}
					else
					{
						const char swizzle[4] =
						{
							'x', 'y', 'z', 'w'
						};

						for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
						{
							_current_source += swizzle[node->mask[i]];
						}
					}
				}
				void visit(const fx::nodes::field_expression_node *node)
				{
					_current_source += '(';

					visit(node->operand);

					_current_source += '.';
					_current_source += print_name(node->field_reference);
					_current_source += ')';
				}
				void visit(const fx::nodes::assignment_expression_node *node)
				{
					_current_source += '(';
					visit(node->left);
					_current_source += ' ';

					switch (node->op)
					{
						case fx::nodes::assignment_expression_node::none:
							_current_source += '=';
							break;
						case fx::nodes::assignment_expression_node::add:
							_current_source += "+=";
							break;
						case fx::nodes::assignment_expression_node::subtract:
							_current_source += "-=";
							break;
						case fx::nodes::assignment_expression_node::multiply:
							_current_source += "*=";
							break;
						case fx::nodes::assignment_expression_node::divide:
							_current_source += "/=";
							break;
						case fx::nodes::assignment_expression_node::modulo:
							_current_source += "%=";
							break;
						case fx::nodes::assignment_expression_node::left_shift:
							_current_source += "<<=";
							break;
						case fx::nodes::assignment_expression_node::right_shift:
							_current_source += ">>=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_and:
							_current_source += "&=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_or:
							_current_source += "|=";
							break;
						case fx::nodes::assignment_expression_node::bitwise_xor:
							_current_source += "^=";
							break;
					}

					_current_source += ' ';
					visit(node->right);
					_current_source += ')';
				}
				void visit(const fx::nodes::call_expression_node *node)
				{
					_current_source += print_name(node->callee);
					_current_source += '(';

					for (auto argument : node->arguments)
					{
						visit(argument);

						_current_source += ", ";
					}

					if (!node->arguments.empty())
					{
						_current_source.pop_back();
						_current_source.pop_back();
					}

					_current_source += ')';
				}
				void visit(const fx::nodes::constructor_expression_node *node)
				{
					_current_source += print_type(node->type);
					_current_source += '(';

					for (auto argument : node->arguments)
					{
						visit(argument);

						_current_source += ", ";
					}

					if (!node->arguments.empty())
					{
						_current_source.pop_back();
						_current_source.pop_back();
					}

					_current_source += ')';
				}
				void visit(const fx::nodes::initializer_list_node *node)
				{
					_current_source += "{ ";

					for (auto expression : node->values)
					{
						visit(expression);

						_current_source += ", ";
					}

					_current_source += " }";
				}

				template <typename T>
				void visit_annotation(const std::vector<fx::nodes::annotation_node> &annotations, T &object)
				{
					for (auto &annotation : annotations)
					{
						switch (annotation.value->type.basetype)
						{
							case fx::nodes::type_node::datatype_bool:
							case fx::nodes::type_node::datatype_int:
								object.annotations[annotation.name] = annotation.value->value_int;
								break;
							case fx::nodes::type_node::datatype_uint:
								object.annotations[annotation.name] = annotation.value->value_uint;
								break;
							case fx::nodes::type_node::datatype_float:
								object.annotations[annotation.name] = annotation.value->value_float;
								break;
							case fx::nodes::type_node::datatype_string:
								object.annotations[annotation.name] = annotation.value->value_string;
								break;
						}
					}
				}
				void visit(const fx::nodes::struct_declaration_node *node)
				{
					_current_source += "struct ";
					_current_source += print_name(node);
					_current_source += "\n{\n";

					if (!node->field_list.empty())
					{
						for (auto field : node->field_list)
						{
							visit(field);
						}
					}
					else
					{
						_current_source += "float _dummy;\n";
					}

					_current_source += "};\n";
				}
				void visit(const fx::nodes::variable_declaration_node *node)
				{
					if (!(_is_in_parameter_block || _is_in_function_block))
					{
						if (node->type.is_texture())
						{
							visit_texture(node);
							return;
						}
						else if (node->type.is_sampler())
						{
							visit_sampler(node);
							return;
						}
						else if (node->type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
						{
							visit_uniform(node);
							return;
						}
					}

					if (_is_in_for_initialization <= 1)
					{
						_current_source += print_type_with_qualifiers(node->type);
					}

					if (!node->name.empty())
					{
						_current_source += ' ' + print_name(node);
					}

					if (node->type.is_array())
					{
						_current_source += '[';
						_current_source += (node->type.array_length >= 1) ? std::to_string(node->type.array_length) : "";
						_current_source += ']';
					}

					if (!node->semantic.empty())
					{
						_current_source += " : " + convert_semantic(node->semantic);
					}

					if (node->initializer_expression != nullptr)
					{
						_current_source += " = ";

						visit(node->initializer_expression);
					}

					if (!(_is_in_parameter_block || _is_in_function_block))
					{
						_current_source += ";\n";
					}
				}
				void visit_texture(const fx::nodes::variable_declaration_node *node)
				{
					D3D11_TEXTURE2D_DESC texdesc;
					ZeroMemory(&texdesc, sizeof(D3D11_TEXTURE2D_DESC));

					d3d11_texture *obj = new d3d11_texture();
					obj->name = node->name;
					texdesc.Width = obj->width = node->properties.Width;
					texdesc.Height = obj->height = node->properties.Height;
					texdesc.MipLevels = obj->levels = node->properties.MipLevels;
					texdesc.ArraySize = 1;
					texdesc.Format = LiteralToFormat(node->properties.Format, obj->format);
					texdesc.SampleDesc.Count = 1;
					texdesc.SampleDesc.Quality = 0;
					texdesc.Usage = D3D11_USAGE_DEFAULT;
					texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
					texdesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

					obj->ShaderRegister = _runtime->_effect_shader_resources.size();
					obj->TextureInterface = nullptr;
					obj->ShaderResourceView[0] = nullptr;
					obj->ShaderResourceView[1] = nullptr;

					visit_annotation(node->annotations, *obj);

					if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							warning(node->location, "texture properties on backbuffer textures are ignored");
						}

						obj->change_data_source(texture::datatype::backbuffer, _runtime, _runtime->_backbuffer_texture_srv[0], _runtime->_backbuffer_texture_srv[1]);
					}
					else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
					{
						if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
						{
							warning(node->location, "texture properties on depthbuffer textures are ignored");
						}

						obj->change_data_source(texture::datatype::depthbuffer, _runtime, _runtime->_depthstencil_texture_srv, nullptr);
					}
					else
					{
						if (texdesc.MipLevels == 0)
						{
							warning(node->location, "a texture cannot have 0 miplevels, changed it to 1");

							texdesc.MipLevels = 1;
						}

						HRESULT hr = _runtime->_device->CreateTexture2D(&texdesc, nullptr, &obj->TextureInterface);

						if (FAILED(hr))
						{
							error(node->location, "'ID3D11Device::CreateTexture2D' failed with %u!", hr);
							return;
						}

						D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
						ZeroMemory(&srvdesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
						srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
						srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
						srvdesc.Format = MakeNonSRGBFormat(texdesc.Format);

						hr = _runtime->_device->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[0]);

						if (FAILED(hr))
						{
							error(node->location, "'ID3D11Device::CreateShaderResourceView' failed with %u!", hr);
							return;
						}

						srvdesc.Format = MakeSRGBFormat(texdesc.Format);

						if (srvdesc.Format != texdesc.Format)
						{
							hr = _runtime->_device->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[1]);

							if (FAILED(hr))
							{
								error(node->location, "'ID3D11Device::CreateShaderResourceView' failed with %u!", hr);
								return;
							}
						}
					}

					_current_source += "Texture2D ";
					_current_source += print_name(node);
					_current_source += " : register(t" + std::to_string(_runtime->_effect_shader_resources.size()) + "), __";
					_current_source += print_name(node);
					_current_source += "SRGB : register(t" + std::to_string(_runtime->_effect_shader_resources.size() + 1) + ");\n";

					_runtime->_effect_shader_resources.push_back(obj->ShaderResourceView[0]);
					_runtime->_effect_shader_resources.push_back(obj->ShaderResourceView[1]);

					_runtime->add_texture(obj);
				}
				void visit_sampler(const fx::nodes::variable_declaration_node *node)
				{
					if (node->properties.Texture == nullptr)
					{
						error(node->location, "sampler '%s' is missing required 'Texture' property", node->name.c_str());
						return;
					}

					D3D11_SAMPLER_DESC desc;
					desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(node->properties.AddressU);
					desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(node->properties.AddressV);
					desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(node->properties.AddressW);
					desc.MipLODBias = node->properties.MipLODBias;
					desc.MinLOD = node->properties.MinLOD;
					desc.MaxLOD = node->properties.MaxLOD;
					desc.MaxAnisotropy = node->properties.MaxAnisotropy;
					desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

					const UINT minfilter = node->properties.MinFilter;
					const UINT magfilter = node->properties.MagFilter;
					const UINT mipfilter = node->properties.MipFilter;

					if (minfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || magfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || mipfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC)
					{
						desc.Filter = D3D11_FILTER_ANISOTROPIC;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
					}
					else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
					}
					else
					{
						desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
					}

					d3d11_texture *texture = static_cast<d3d11_texture *>(_runtime->get_texture(node->properties.Texture->name));

					if (texture == nullptr)
					{
						error(node->location, "texture '%s' for sampler '%s' is missing due to previous error", node->properties.Texture->name.c_str(), node->name.c_str());
						return;
					}

					const size_t descHash = D3D11_SAMPLER_DESC_HASH(desc);
					auto it = _sampler_descs.find(descHash);

					if (it == _sampler_descs.end())
					{
						ID3D11SamplerState *sampler = nullptr;

						HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

						if (FAILED(hr))
						{
							error(node->location, "'ID3D11Device::CreateSamplerState' failed with %u!", hr);
							return;
						}

						_runtime->_effect_sampler_states.push_back(sampler);
						it = _sampler_descs.emplace(descHash, _runtime->_effect_sampler_states.size() - 1).first;

						_current_source += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
					}

					_current_source += "static const __sampler2D ";
					_current_source += print_name(node);
					_current_source += " = { ";

					if (node->properties.SRGBTexture && texture->ShaderResourceView[1] != nullptr)
					{
						_current_source += "__";
						_current_source += print_name(node->properties.Texture);
						_current_source += "SRGB";
					}
					else
					{
						_current_source += print_name(node->properties.Texture);
					}

					_current_source += ", __SamplerState" + std::to_string(it->second) + " };\n";
				}
				void visit_uniform(const fx::nodes::variable_declaration_node *node)
				{
					_global_uniforms += print_type_with_qualifiers(node->type);
					_global_uniforms += ' ';
					_global_uniforms += print_name(node);

					if (node->type.is_array())
					{
						_global_uniforms += '[';
						_global_uniforms += (node->type.array_length >= 1) ? std::to_string(node->type.array_length) : "";
						_global_uniforms += ']';
					}

					_global_uniforms += ";\n";

					uniform *obj = new uniform();
					obj->name = node->name;
					obj->rows = node->type.rows;
					obj->columns = node->type.cols;
					obj->elements = node->type.array_length;
					obj->storage_size = node->type.rows * node->type.cols;

					switch (node->type.basetype)
					{
						case fx::nodes::type_node::datatype_bool:
							obj->basetype = uniform::datatype::bool_;
							obj->storage_size *= sizeof(int);
							break;
						case fx::nodes::type_node::datatype_int:
							obj->basetype = uniform::datatype::int_;
							obj->storage_size *= sizeof(int);
							break;
						case fx::nodes::type_node::datatype_uint:
							obj->basetype = uniform::datatype::uint_;
							obj->storage_size *= sizeof(unsigned int);
							break;
						case fx::nodes::type_node::datatype_float:
							obj->basetype = uniform::datatype::float_;
							obj->storage_size *= sizeof(float);
							break;
					}

					const UINT alignment = 16 - (_current_global_size % 16);
					_current_global_size += static_cast<UINT>((obj->storage_size > alignment && (alignment != 16 || obj->storage_size <= 16)) ? obj->storage_size + alignment : obj->storage_size);
					obj->storage_offset = _current_global_size - obj->storage_size;

					visit_annotation(node->annotations, *obj);

					if (_current_global_size >= _runtime->get_uniform_data_storage_size())
					{
						_runtime->enlarge_uniform_data_storage();
					}

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
				void visit(const fx::nodes::function_declaration_node *node)
				{
					_current_source += print_type(node->return_type);
					_current_source += ' ';
					_current_source += print_name(node);
					_current_source += '(';

					_is_in_parameter_block = true;

					for (auto parameter : node->parameter_list)
					{
						visit(parameter);

						_current_source += ", ";
					}

					_is_in_parameter_block = false;

					if (!node->parameter_list.empty())
					{
						_current_source.pop_back();
						_current_source.pop_back();
					}

					_current_source += ')';
								
					if (!node->return_semantic.empty())
					{
						_current_source += " : " + convert_semantic(node->return_semantic);
					}

					_current_source += '\n';

					_is_in_function_block = true;

					visit(node->definition);

					_is_in_function_block = false;
				}
				void visit(const fx::nodes::technique_declaration_node *node)
				{
					d3d11_technique *obj = new d3d11_technique();
					obj->name = node->name;
					obj->pass_count = static_cast<unsigned int>(node->pass_list.size());

					visit_annotation(node->annotation_list, *obj);

					for (auto pass : node->pass_list)
					{
						visit_pass(pass, obj->passes);
					}

					_runtime->add_technique(obj);
				}
				void visit_pass(const fx::nodes::pass_declaration_node *node, std::vector<d3d11_pass> &passes)
				{
					d3d11_pass pass;
					pass.VS = nullptr;
					pass.PS = nullptr;
					pass.BS = nullptr;
					pass.DSS = nullptr;
					pass.StencilRef = 0;
					pass.Viewport.TopLeftX = pass.Viewport.TopLeftY = pass.Viewport.Width = pass.Viewport.Height = 0.0f;
					pass.Viewport.MinDepth = 0.0f;
					pass.Viewport.MaxDepth = 1.0f;
					ZeroMemory(pass.RT, sizeof(pass.RT));
					ZeroMemory(pass.RTSRV, sizeof(pass.RTSRV));
					pass.SRV = _runtime->_effect_shader_resources;

					if (node->states.VertexShader != nullptr)
					{
						visit_pass_shader(node->states.VertexShader, "vs", pass);
					}
					if (node->states.PixelShader != nullptr)
					{
						visit_pass_shader(node->states.PixelShader, "ps", pass);
					}

					const int targetIndex = node->states.SRGBWriteEnable ? 1 : 0;
					pass.RT[0] = _runtime->_backbuffer_rtv[targetIndex];
					pass.RTSRV[0] = _runtime->_backbuffer_texture_srv[targetIndex];

					for (unsigned int i = 0; i < 8; ++i)
					{
						if (node->states.RenderTargets[i] == nullptr)
						{
							continue;
						}

						d3d11_texture *texture = static_cast<d3d11_texture *>(_runtime->get_texture(node->states.RenderTargets[i]->name));

						if (texture == nullptr)
						{
							_is_fatal = true;
							return;
						}

						D3D11_TEXTURE2D_DESC desc;
						texture->TextureInterface->GetDesc(&desc);

						if (pass.Viewport.Width != 0 && pass.Viewport.Height != 0 && (desc.Width != static_cast<unsigned int>(pass.Viewport.Width) || desc.Height != static_cast<unsigned int>(pass.Viewport.Height)))
						{
							error(node->location, "cannot use multiple rendertargets with different sized textures");
							return;
						}
						else
						{
							pass.Viewport.Width = static_cast<FLOAT>(desc.Width);
							pass.Viewport.Height = static_cast<FLOAT>(desc.Height);
						}

						D3D11_RENDER_TARGET_VIEW_DESC rtvdesc;
						ZeroMemory(&rtvdesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
						rtvdesc.Format = node->states.SRGBWriteEnable ? MakeSRGBFormat(desc.Format) : MakeNonSRGBFormat(desc.Format);
						rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;

						if (texture->RenderTargetView[targetIndex] == nullptr)
						{
							HRESULT hr = _runtime->_device->CreateRenderTargetView(texture->TextureInterface, &rtvdesc, &texture->RenderTargetView[targetIndex]);

							if (FAILED(hr))
							{
								warning(node->location, "'ID3D11Device::CreateRenderTargetView' failed with %u!", hr);
							}
						}

						pass.RT[i] = texture->RenderTargetView[targetIndex];
						pass.RTSRV[i] = texture->ShaderResourceView[targetIndex];
					}

					if (pass.Viewport.Width == 0 && pass.Viewport.Height == 0)
					{
						pass.Viewport.Width = static_cast<FLOAT>(_runtime->buffer_width());
						pass.Viewport.Height = static_cast<FLOAT>(_runtime->buffer_height());
					}

					D3D11_DEPTH_STENCIL_DESC ddesc;
					ddesc.DepthEnable = node->states.DepthEnable;
					ddesc.DepthWriteMask = node->states.DepthWriteMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
					ddesc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(node->states.DepthFunc);
					ddesc.StencilEnable = node->states.StencilEnable;
					ddesc.StencilReadMask = node->states.StencilReadMask;
					ddesc.StencilWriteMask = node->states.StencilWriteMask;
					ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D11_COMPARISON_FUNC>(node->states.StencilFunc);
					ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = LiteralToStencilOp(node->states.StencilOpPass);
					ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = LiteralToStencilOp(node->states.StencilOpFail);
					ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = LiteralToStencilOp(node->states.StencilOpDepthFail);
					pass.StencilRef = node->states.StencilRef;

					HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.DSS);

					if (FAILED(hr))
					{
						warning(node->location, "'ID3D11Device::CreateDepthStencilState' failed with %u!", hr);
					}

					D3D11_BLEND_DESC bdesc;
					bdesc.AlphaToCoverageEnable = FALSE;
					bdesc.IndependentBlendEnable = FALSE;
					bdesc.RenderTarget[0].RenderTargetWriteMask = node->states.RenderTargetWriteMask;
					bdesc.RenderTarget[0].BlendEnable = node->states.BlendEnable;
					bdesc.RenderTarget[0].BlendOp = static_cast<D3D11_BLEND_OP>(node->states.BlendOp);
					bdesc.RenderTarget[0].BlendOpAlpha = static_cast<D3D11_BLEND_OP>(node->states.BlendOpAlpha);
					bdesc.RenderTarget[0].SrcBlend = LiteralToBlend(node->states.SrcBlend);
					bdesc.RenderTarget[0].DestBlend = LiteralToBlend(node->states.DestBlend);

					hr = _runtime->_device->CreateBlendState(&bdesc, &pass.BS);

					if (FAILED(hr))
					{
						warning(node->location, "'ID3D11Device::CreateBlendState' failed with %u!", hr);
					}

					for (ID3D11ShaderResourceView *&srv : pass.SRV)
					{
						if (srv == nullptr)
						{
							continue;
						}

						ID3D11Resource *res1, *res2;
						srv->GetResource(&res1);
						res1->Release();

						for (ID3D11RenderTargetView *rtv : pass.RT)
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
				void visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, d3d11_pass &pass)
				{
					std::string profile = shadertype;
					const D3D_FEATURE_LEVEL featurelevel = _runtime->_device->GetFeatureLevel();

					switch (featurelevel)
					{
						default:
						case D3D_FEATURE_LEVEL_11_0:
							profile += "_5_0";
							break;
						case D3D_FEATURE_LEVEL_10_1:
							profile += "_4_1";
							break;
						case D3D_FEATURE_LEVEL_10_0:
							profile += "_4_0";
							break;
						case D3D_FEATURE_LEVEL_9_1:
						case D3D_FEATURE_LEVEL_9_2:
							profile += "_4_0_level_9_1";
							break;
						case D3D_FEATURE_LEVEL_9_3:
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

					if (featurelevel >= D3D_FEATURE_LEVEL_10_1)
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

					if (featurelevel >= D3D_FEATURE_LEVEL_11_0)
					{
						source +=
							"inline float4 __tex2Dgather1(__sampler2D s, float2 c) { return s.t.GatherGreen(s.s, c); }\n"
							"inline float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return s.t.GatherGreen(s.s, c, offset); }\n"
							"inline float4 __tex2Dgather2(__sampler2D s, float2 c) { return s.t.GatherBlue(s.s, c); }\n"
							"inline float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return s.t.GatherBlue(s.s, c, offset); }\n"
							"inline float4 __tex2Dgather3(__sampler2D s, float2 c) { return s.t.GatherAlpha(s.s, c); }\n"
							"inline float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return s.t.GatherAlpha(s.s, c, offset); }\n";
					}
					else
					{
						source +=
							"inline float4 __tex2Dgather1(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0).g); }\n"
							"inline float4 __tex2Dgather1offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).g, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).g, s.t.SampleLevel(s.s, c, 0, offset).g); }\n"
							"inline float4 __tex2Dgather2(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0).b); }\n"
							"inline float4 __tex2Dgather2offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).b, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).b, s.t.SampleLevel(s.s, c, 0, offset).b); }\n"
							"inline float4 __tex2Dgather3(__sampler2D s, float2 c) { return float4( s.t.SampleLevel(s.s, c, 0, int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0).a); }\n"
							"inline float4 __tex2Dgather3offset(__sampler2D s, float2 c, int2 offset) { return float4( s.t.SampleLevel(s.s, c, 0, offset + int2(0, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 1)).a, s.t.SampleLevel(s.s, c, 0, offset + int2(1, 0)).a, s.t.SampleLevel(s.s, c, 0, offset).a); }\n";
					}

					if (!_global_uniforms.empty())
					{
						source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + _global_uniforms + "};\n";
					}

					source += _current_source;

					LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source.c_str() << "\n";

					UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
					ID3DBlob *compiled = nullptr, *errors = nullptr;

					if (_skip_shader_optimization)
					{
						flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
					}

					HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, print_name(node).c_str(), profile.c_str(), flags, 0, &compiled, &errors);

					if (errors != nullptr)
					{
						_errors += std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

						errors->Release();
					}

					if (FAILED(hr))
					{
						_is_fatal = true;
						return;
					}

					if (shadertype == "vs")
					{
						hr = _runtime->_device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &pass.VS);
					}
					else if (shadertype == "ps")
					{
						hr = _runtime->_device->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &pass.PS);
					}

					compiled->Release();

					if (FAILED(hr))
					{
						error(node->location, "'CreateShader' failed with %u!", hr);
						return;
					}
				}

			private:
				const fx::nodetree &_ast;
				d3d11_runtime *_runtime;
				std::string _current_source;
				std::string _errors;
				bool _is_fatal, _skip_shader_optimization;
				std::unordered_map<size_t, size_t> _sampler_descs;
				std::string _global_uniforms;
				UINT _current_global_size, _is_in_for_initialization;
				bool _is_in_parameter_block, _is_in_function_block;
			};
		}

		class d3d11_stateblock
		{
		public:
			d3d11_stateblock(ID3D11Device *device)
			{
				ZeroMemory(this, sizeof(d3d11_stateblock));

				device->GetImmediateContext(&_deviceContext);
				_featureLevel = device->GetFeatureLevel();
			}
			~d3d11_stateblock()
			{
				release_all_device_objects();

				_deviceContext->Release();
			}

			void capture()
			{
				_deviceContext->IAGetPrimitiveTopology(&_IAPrimitiveTopology);
				_deviceContext->IAGetInputLayout(&_IAInputLayout);

				if (_featureLevel > D3D_FEATURE_LEVEL_10_0)
				{
					_deviceContext->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				}
				else
				{
					_deviceContext->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				}

				_deviceContext->IAGetIndexBuffer(&_IAIndexBuffer, &_IAIndexFormat, &_IAIndexOffset);

				_deviceContext->RSGetState(&_RSState);
				_deviceContext->RSGetViewports(&_RSNumViewports, nullptr);
				_deviceContext->RSGetViewports(&_RSNumViewports, _RSViewports);

				_VSNumClassInstances = 256;
				_deviceContext->VSGetShader(&_VS, _VSClassInstances, &_VSNumClassInstances);
				_deviceContext->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _VSConstantBuffers);
				_deviceContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, _VSSamplerStates);
				_deviceContext->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _VSShaderResources);

				if (_featureLevel >= D3D_FEATURE_LEVEL_10_0)
				{
					if (_featureLevel >= D3D_FEATURE_LEVEL_11_0)
					{
						_HSNumClassInstances = 256;
						_deviceContext->HSGetShader(&_HS, _HSClassInstances, &_HSNumClassInstances);

						_DSNumClassInstances = 256;
						_deviceContext->DSGetShader(&_DS, _DSClassInstances, &_DSNumClassInstances);
					}

					_GSNumClassInstances = 256;
					_deviceContext->GSGetShader(&_GS, _GSClassInstances, &_GSNumClassInstances);
				}

				_PSNumClassInstances = 256;
				_deviceContext->PSGetShader(&_PS, _PSClassInstances, &_PSNumClassInstances);
				_deviceContext->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _PSConstantBuffers);
				_deviceContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, _PSSamplerStates);
				_deviceContext->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _PSShaderResources);

				_deviceContext->OMGetBlendState(&_OMBlendState, _OMBlendFactor, &_OMSampleMask);
				_deviceContext->OMGetDepthStencilState(&_OMDepthStencilState, &_OMStencilRef);
				_deviceContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, _OMRenderTargets, &_OMDepthStencil);
			}
			void apply_and_release()
			{
				_deviceContext->IASetPrimitiveTopology(_IAPrimitiveTopology);
				_deviceContext->IASetInputLayout(_IAInputLayout);

				if (_featureLevel > D3D_FEATURE_LEVEL_10_0)
				{
					_deviceContext->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				}
				else
				{
					_deviceContext->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, _IAVertexBuffers, _IAVertexStrides, _IAVertexOffsets);
				}

				_deviceContext->IASetIndexBuffer(_IAIndexBuffer, _IAIndexFormat, _IAIndexOffset);

				_deviceContext->RSSetState(_RSState);
				_deviceContext->RSSetViewports(_RSNumViewports, _RSViewports);

				_deviceContext->VSSetShader(_VS, _VSClassInstances, _VSNumClassInstances);
				_deviceContext->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _VSConstantBuffers);
				_deviceContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, _VSSamplerStates);
				_deviceContext->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _VSShaderResources);

				if (_featureLevel >= D3D_FEATURE_LEVEL_10_0)
				{
					if (_featureLevel >= D3D_FEATURE_LEVEL_11_0)
					{
						_deviceContext->HSSetShader(_HS, _HSClassInstances, _HSNumClassInstances);

						_deviceContext->DSSetShader(_DS, _DSClassInstances, _DSNumClassInstances);
					}

					_deviceContext->GSSetShader(_GS, _GSClassInstances, _GSNumClassInstances);
				}

				_deviceContext->PSSetShader(_PS, _PSClassInstances, _PSNumClassInstances);
				_deviceContext->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, _PSConstantBuffers);
				_deviceContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, _PSSamplerStates);
				_deviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, _PSShaderResources);

				_deviceContext->OMSetBlendState(_OMBlendState, _OMBlendFactor, _OMSampleMask);
				_deviceContext->OMSetDepthStencilState(_OMDepthStencilState, _OMStencilRef);
				_deviceContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, _OMRenderTargets, _OMDepthStencil);

				release_all_device_objects();
			}

		private:
			void release_all_device_objects()
			{
				SAFE_RELEASE(_IAInputLayout);

				for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_IAVertexBuffers[i]);
				}

				SAFE_RELEASE(_IAIndexBuffer);

				SAFE_RELEASE(_VS);

				for (UINT i = 0; i < _VSNumClassInstances; ++i)
				{
					SAFE_RELEASE(_VSClassInstances[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_VSShaderResources[i]);
				}

				SAFE_RELEASE(_HS);

				for (UINT i = 0; i < _HSNumClassInstances; ++i)
				{
					SAFE_RELEASE(_HSClassInstances[i]);
				}

				SAFE_RELEASE(_DS);
			
				for (UINT i = 0; i < _DSNumClassInstances; ++i)
				{
					SAFE_RELEASE(_DSClassInstances[i]);
				}

				SAFE_RELEASE(_GS);

				for (UINT i = 0; i < _GSNumClassInstances; ++i)
				{
					SAFE_RELEASE(_GSClassInstances[i]);
				}

				SAFE_RELEASE(_RSState);

				SAFE_RELEASE(_PS);
			
				for (UINT i = 0; i < _PSNumClassInstances; ++i)
				{
					SAFE_RELEASE(_PSClassInstances[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSConstantBuffers[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSSamplerStates[i]);
				}
				for (UINT i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				{
					SAFE_RELEASE(_PSShaderResources[i]);
				}

				SAFE_RELEASE(_OMBlendState);
				SAFE_RELEASE(_OMDepthStencilState);

				for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					SAFE_RELEASE(_OMRenderTargets[i]);
				}

				SAFE_RELEASE(_OMDepthStencil);
			}

			ID3D11DeviceContext *_deviceContext;
			D3D_FEATURE_LEVEL _featureLevel;

			ID3D11InputLayout *_IAInputLayout;
			D3D11_PRIMITIVE_TOPOLOGY _IAPrimitiveTopology;
			ID3D11Buffer *_IAVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT _IAVertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			UINT _IAVertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11Buffer *_IAIndexBuffer;
			DXGI_FORMAT _IAIndexFormat;
			UINT _IAIndexOffset;
			ID3D11VertexShader *_VS;
			UINT _VSNumClassInstances;
			ID3D11ClassInstance *_VSClassInstances[256];
			ID3D11Buffer *_VSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11SamplerState *_VSSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D11ShaderResourceView *_VSShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11HullShader *_HS;
			UINT _HSNumClassInstances;
			ID3D11ClassInstance *_HSClassInstances[256];
			ID3D11DomainShader *_DS;
			UINT _DSNumClassInstances;
			ID3D11ClassInstance *_DSClassInstances[256];
			ID3D11GeometryShader *_GS;
			UINT _GSNumClassInstances;
			ID3D11ClassInstance *_GSClassInstances[256];
			ID3D11RasterizerState *_RSState;
			UINT _RSNumViewports;
			D3D11_VIEWPORT _RSViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			ID3D11PixelShader *_PS;
			UINT _PSNumClassInstances;
			ID3D11ClassInstance *_PSClassInstances[256];
			ID3D11Buffer *_PSConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
			ID3D11SamplerState *_PSSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
			ID3D11ShaderResourceView *_PSShaderResources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
			ID3D11BlendState *_OMBlendState;
			FLOAT _OMBlendFactor[4];
			UINT _OMSampleMask;
			ID3D11DepthStencilState *_OMDepthStencilState;
			UINT _OMStencilRef;
			ID3D11RenderTargetView *_OMRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
			ID3D11DepthStencilView *_OMDepthStencil;
		};

		// ---------------------------------------------------------------------------------------------------

		d3d11_runtime::d3d11_runtime(ID3D11Device *device, IDXGISwapChain *swapchain) : runtime(device->GetFeatureLevel()), _device(device), _swapchain(swapchain), _immediate_context(nullptr), _backbuffer_format(DXGI_FORMAT_UNKNOWN), _is_multisampling_enabled(false), _stateblock(new d3d11_stateblock(device)), _backbuffer(nullptr), _backbuffer_resolved(nullptr), _backbuffer_texture(nullptr), _backbuffer_texture_srv(), _backbuffer_rtv(), _depthstencil(nullptr), _depthstencil_replacement(nullptr), _depthstencil_texture(nullptr), _depthstencil_texture_srv(nullptr), _copy_vertex_shader(nullptr), _copy_pixel_shader(nullptr), _copy_sampler(nullptr), _effect_rasterizer_state(nullptr), _constant_buffer(nullptr)
		{
			_device->AddRef();
			_device->GetImmediateContext(&_immediate_context);
			_swapchain->AddRef();

			assert(_immediate_context != nullptr);

			IDXGIDevice *dxgidevice = nullptr;
			IDXGIAdapter *adapter = nullptr;

			HRESULT hr = _device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgidevice));

			assert(SUCCEEDED(hr));

			hr = dxgidevice->GetAdapter(&adapter);

			SAFE_RELEASE(dxgidevice);

			assert(SUCCEEDED(hr));

			DXGI_ADAPTER_DESC desc;
			hr = adapter->GetDesc(&desc);

			SAFE_RELEASE(adapter);

			assert(SUCCEEDED(hr));

			_vendor_id = desc.VendorId;
			_device_id = desc.DeviceId;
		}
		d3d11_runtime::~d3d11_runtime()
		{
			_immediate_context->Release();
			_device->Release();
			_swapchain->Release();
		}

		bool d3d11_runtime::on_init(const DXGI_SWAP_CHAIN_DESC &desc)
		{
			_width = desc.BufferDesc.Width;
			_height = desc.BufferDesc.Height;
			_backbuffer_format = desc.BufferDesc.Format;
			_is_multisampling_enabled = desc.SampleDesc.Count > 1;
			_input = input::register_window(desc.OutputWindow);

			#pragma region Get backbuffer rendertarget
			HRESULT hr = _swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&_backbuffer));

			assert(SUCCEEDED(hr));

			D3D11_TEXTURE2D_DESC texdesc;
			texdesc.Width = _width;
			texdesc.Height = _height;
			texdesc.ArraySize = texdesc.MipLevels = 1;
			texdesc.Format = d3d11_fx_compiler::MakeTypelessFormat(_backbuffer_format);
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D11_USAGE_DEFAULT;
			texdesc.BindFlags = D3D11_BIND_RENDER_TARGET;
			texdesc.MiscFlags = texdesc.CPUAccessFlags = 0;

			OSVERSIONINFOEX verinfoWin7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
			const bool isWindows7 = VerifyVersionInfo(&verinfoWin7, VER_MAJORVERSION | VER_MINORVERSION, VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;

			if (_is_multisampling_enabled || d3d11_fx_compiler::MakeNonSRGBFormat(_backbuffer_format) != _backbuffer_format || !isWindows7)
			{
				hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbuffer_resolved);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create backbuffer resolve texture (Width = " << texdesc.Width << ", Height = " << texdesc.Height << ", Format = " << texdesc.Format << ", SampleCount = " << texdesc.SampleDesc.Count << ", SampleQuality = " << texdesc.SampleDesc.Quality << ")! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_backbuffer);

					return false;
				}

				hr = _device->CreateRenderTargetView(_backbuffer, nullptr, &_backbuffer_rtv[2]);

				assert(SUCCEEDED(hr));
			}
			else
			{
				_backbuffer_resolved = _backbuffer;
				_backbuffer_resolved->AddRef();
			}
			#pragma endregion

			#pragma region Create backbuffer shader texture
			texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbuffer_texture);

			if (SUCCEEDED(hr))
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
				ZeroMemory(&srvdesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
				srvdesc.Format = d3d11_fx_compiler::MakeNonSRGBFormat(texdesc.Format);
				srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = texdesc.MipLevels;

				if (SUCCEEDED(hr))
				{
					hr = _device->CreateShaderResourceView(_backbuffer_texture, &srvdesc, &_backbuffer_texture_srv[0]);
				}
				else
				{
					LOG(TRACE) << "Failed to create backbuffer texture resource view (Format = " << srvdesc.Format << ")! HRESULT is '" << hr << "'.";
				}

				srvdesc.Format = d3d11_fx_compiler::MakeSRGBFormat(texdesc.Format);

				if (SUCCEEDED(hr))
				{
					hr = _device->CreateShaderResourceView(_backbuffer_texture, &srvdesc, &_backbuffer_texture_srv[1]);
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
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_srv[0]);
				SAFE_RELEASE(_backbuffer_texture_srv[1]);

				return false;
			}
			#pragma endregion

			D3D11_RENDER_TARGET_VIEW_DESC rtdesc;
			ZeroMemory(&rtdesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			rtdesc.Format = d3d11_fx_compiler::MakeNonSRGBFormat(texdesc.Format);
			rtdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			hr = _device->CreateRenderTargetView(_backbuffer_resolved, &rtdesc, &_backbuffer_rtv[0]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_srv[0]);
				SAFE_RELEASE(_backbuffer_texture_srv[1]);

				LOG(TRACE) << "Failed to create backbuffer rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

				return false;
			}

			rtdesc.Format = d3d11_fx_compiler::MakeSRGBFormat(texdesc.Format);

			hr = _device->CreateRenderTargetView(_backbuffer_resolved, &rtdesc, &_backbuffer_rtv[1]);

			if (FAILED(hr))
			{
				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_srv[0]);
				SAFE_RELEASE(_backbuffer_texture_srv[1]);
				SAFE_RELEASE(_backbuffer_rtv[0]);

				LOG(TRACE) << "Failed to create backbuffer SRGB rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

				return false;
			}

			#pragma region Create default depthstencil surface
			D3D11_TEXTURE2D_DESC dstdesc;
			ZeroMemory(&dstdesc, sizeof(D3D11_TEXTURE2D_DESC));
			dstdesc.Width = desc.BufferDesc.Width;
			dstdesc.Height = desc.BufferDesc.Height;
			dstdesc.MipLevels = 1;
			dstdesc.ArraySize = 1;
			dstdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dstdesc.SampleDesc.Count = 1;
			dstdesc.SampleDesc.Quality = 0;
			dstdesc.Usage = D3D11_USAGE_DEFAULT;
			dstdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

			ID3D11Texture2D *dstexture = nullptr;
			hr = _device->CreateTexture2D(&dstdesc, nullptr, &dstexture);

			if (SUCCEEDED(hr))
			{
				hr = _device->CreateDepthStencilView(dstexture, nullptr, &_default_depthstencil);

				SAFE_RELEASE(dstexture);
			}
			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

				SAFE_RELEASE(_backbuffer);
				SAFE_RELEASE(_backbuffer_resolved);
				SAFE_RELEASE(_backbuffer_texture);
				SAFE_RELEASE(_backbuffer_texture_srv[0]);
				SAFE_RELEASE(_backbuffer_texture_srv[1]);
				SAFE_RELEASE(_backbuffer_rtv[0]);
				SAFE_RELEASE(_backbuffer_rtv[1]);

				return false;
			}
			#pragma endregion

			const BYTE vs[] = { 68, 88, 66, 67, 224, 206, 72, 137, 142, 185, 68, 219, 247, 216, 225, 132, 111, 78, 106, 20, 1, 0, 0, 0, 156, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 140, 0, 0, 0, 192, 0, 0, 0, 24, 1, 0, 0, 32, 2, 0, 0, 82, 68, 69, 70, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 0, 0, 0, 0, 4, 254, 255, 0, 1, 0, 0, 28, 0, 0, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 171, 171, 73, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 83, 86, 95, 86, 101, 114, 116, 101, 120, 73, 68, 0, 79, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 12, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 83, 72, 68, 82, 0, 1, 0, 0, 64, 0, 1, 0, 64, 0, 0, 0, 96, 0, 0, 4, 18, 16, 16, 0, 0, 0, 0, 0, 6, 0, 0, 0, 103, 0, 0, 4, 242, 32, 16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 101, 0, 0, 3, 50, 32, 16, 0, 1, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 32, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 6, 16, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 15, 50, 32, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 128, 191, 0, 0, 128, 63, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 5, 50, 32, 16, 0, 1, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 54, 0, 0, 8, 194, 32, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = _device->CreateVertexShader(vs, ARRAYSIZE(vs), nullptr, &_copy_vertex_shader);

			assert(SUCCEEDED(hr));

			const BYTE ps[] = { 68, 88, 66, 67, 93, 102, 148, 45, 34, 106, 51, 79, 54, 23, 136, 21, 27, 217, 232, 71, 1, 0, 0, 0, 116, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 208, 0, 0, 0, 40, 1, 0, 0, 92, 1, 0, 0, 248, 1, 0, 0, 82, 68, 69, 70, 148, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 28, 0, 0, 0, 0, 4, 255, 255, 0, 1, 0, 0, 98, 0, 0, 0, 92, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 95, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 1, 0, 0, 0, 13, 0, 0, 0, 115, 48, 0, 116, 48, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 73, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 3, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 79, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 83, 86, 95, 84, 97, 114, 103, 101, 116, 0, 171, 171, 83, 72, 68, 82, 148, 0, 0, 0, 64, 0, 0, 0, 37, 0, 0, 0, 90, 0, 0, 3, 0, 96, 16, 0, 0, 0, 0, 0, 88, 24, 0, 4, 0, 112, 16, 0, 0, 0, 0, 0, 85, 85, 0, 0, 98, 16, 0, 3, 50, 16, 16, 0, 1, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 69, 0, 0, 9, 242, 0, 16, 0, 0, 0, 0, 0, 70, 16, 16, 0, 1, 0, 0, 0, 70, 126, 16, 0, 0, 0, 0, 0, 0, 96, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 114, 32, 16, 0, 0, 0, 0, 0, 70, 2, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 130, 32, 16, 0, 0, 0, 0, 0, 1, 64, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			hr = _device->CreatePixelShader(ps, ARRAYSIZE(ps), nullptr, &_copy_pixel_shader);

			assert(SUCCEEDED(hr));

			const D3D11_SAMPLER_DESC copysampdesc = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP };
			hr = _device->CreateSamplerState(&copysampdesc, &_copy_sampler);

			assert(SUCCEEDED(hr));

			#pragma region Create effect objects
			D3D11_RASTERIZER_DESC rsdesc;
			ZeroMemory(&rsdesc, sizeof(D3D11_RASTERIZER_DESC));
			rsdesc.FillMode = D3D11_FILL_SOLID;
			rsdesc.CullMode = D3D11_CULL_NONE;
			rsdesc.DepthClipEnable = TRUE;

			hr = _device->CreateRasterizerState(&rsdesc, &_effect_rasterizer_state);

			assert(SUCCEEDED(hr));
			#pragma endregion

			_gui.reset(new gui(this, nvgCreateD3D11(_device, 0)));

			// Clear reference count to make UnrealEngine happy
			_backbuffer->Release();
			_backbuffer->Release();

			return runtime::on_init();
		}
		void d3d11_runtime::on_reset()
		{
			if (!_is_initialized)
			{
				return;
			}

			runtime::on_reset();

			// Destroy NanoVG
			NVGcontext *const nvg = _gui->context();

			_gui.reset();

			nvgDeleteD3D11(nvg);

			// Reset reference count to make UnrealEngine happy
			_backbuffer->AddRef();
			_backbuffer = nullptr;

			// Destroy resources
			SAFE_RELEASE(_backbuffer_resolved);
			SAFE_RELEASE(_backbuffer_texture);
			SAFE_RELEASE(_backbuffer_texture_srv[0]);
			SAFE_RELEASE(_backbuffer_texture_srv[1]);
			SAFE_RELEASE(_backbuffer_rtv[0]);
			SAFE_RELEASE(_backbuffer_rtv[1]);
			SAFE_RELEASE(_backbuffer_rtv[2]);

			SAFE_RELEASE(_depthstencil);
			SAFE_RELEASE(_depthstencil_replacement);
			SAFE_RELEASE(_depthstencil_texture);
			SAFE_RELEASE(_depthstencil_texture_srv);

			SAFE_RELEASE(_default_depthstencil);
			SAFE_RELEASE(_copy_vertex_shader);
			SAFE_RELEASE(_copy_pixel_shader);
			SAFE_RELEASE(_copy_sampler);

			SAFE_RELEASE(_effect_rasterizer_state);
		}
		void d3d11_runtime::on_reset_effect()
		{
			runtime::on_reset_effect();

			for (auto it : _effect_sampler_states)
			{
				it->Release();
			}

			_effect_sampler_states.clear();
			_effect_shader_resources.clear();

			SAFE_RELEASE(_constant_buffer);
		}
		void d3d11_runtime::on_present()
		{
			if (!_is_initialized)
			{
				LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
				return;
			}
			else if (_stats.drawcalls == 0)
			{
				return;
			}

			detect_depth_source();

			// Capture device state
			_stateblock->capture();

			// Resolve backbuffer
			if (_backbuffer_resolved != _backbuffer)
			{
				_immediate_context->ResolveSubresource(_backbuffer_resolved, 0, _backbuffer, 0, _backbuffer_format);
			}

			// Apply post processing
			on_apply_effect();

			// Reset rendertargets
			_immediate_context->OMSetRenderTargets(1, &_backbuffer_rtv[0], _default_depthstencil);

			const D3D11_VIEWPORT viewport = { 0, 0, static_cast<FLOAT>(_width), static_cast<FLOAT>(_height), 0.0f, 1.0f };
			_immediate_context->RSSetViewports(1, &viewport);

			// Apply presenting
			runtime::on_present();

			// Copy to backbuffer
			if (_backbuffer_resolved != _backbuffer)
			{
				_immediate_context->OMSetRenderTargets(1, &_backbuffer_rtv[2], nullptr);
				_immediate_context->CopyResource(_backbuffer_texture, _backbuffer_resolved);

				_immediate_context->VSSetShader(_copy_vertex_shader, nullptr, 0);
				_immediate_context->PSSetShader(_copy_pixel_shader, nullptr, 0);
				_immediate_context->PSSetSamplers(0, 1, &_copy_sampler);
				_immediate_context->PSSetShaderResources(0, 1, &_backbuffer_texture_srv[d3d11_fx_compiler::MakeSRGBFormat(_backbuffer_format) == _backbuffer_format]);
				_immediate_context->Draw(3, 0);
			}

			// Apply previous device state
			_stateblock->apply_and_release();
		}
		void d3d11_runtime::on_draw_call(ID3D11DeviceContext *context, unsigned int vertices)
		{
			const utils::critical_section::lock lock(_cs);

			runtime::on_draw_call(vertices);

			ID3D11DepthStencilView *depthstencil = nullptr;
			context->OMGetRenderTargets(0, nullptr, &depthstencil);

			if (depthstencil != nullptr)
			{
				depthstencil->Release();

				if (depthstencil == _default_depthstencil)
				{
					return;
				}
				else if (depthstencil == _depthstencil_replacement)
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
		void d3d11_runtime::on_apply_effect()
		{
			if (!_is_effect_compiled)
			{
				return;
			}

			// Setup real backbuffer
			_immediate_context->OMSetRenderTargets(1, &_backbuffer_rtv[0], nullptr);

			// Setup vertex input
			const uintptr_t null = 0;
			_immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_immediate_context->IASetInputLayout(nullptr);
			_immediate_context->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D11Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

			_immediate_context->RSSetState(_effect_rasterizer_state);

			// Disable unsued pipeline stages
			_immediate_context->HSSetShader(nullptr, nullptr, 0);
			_immediate_context->DSSetShader(nullptr, nullptr, 0);
			_immediate_context->GSSetShader(nullptr, nullptr, 0);

			// Setup samplers
			_immediate_context->VSSetSamplers(0, static_cast<UINT>(_effect_sampler_states.size()), _effect_sampler_states.data());
			_immediate_context->PSSetSamplers(0, static_cast<UINT>(_effect_sampler_states.size()), _effect_sampler_states.data());

			// Setup shader constants
			_immediate_context->VSSetConstantBuffers(0, 1, &_constant_buffer);
			_immediate_context->PSSetConstantBuffers(0, 1, &_constant_buffer);

			// Apply post processing
			runtime::on_apply_effect();
		}
		void d3d11_runtime::on_apply_effect_technique(const technique *technique)
		{
			runtime::on_apply_effect_technique(technique);

			bool is_default_depthstencil_cleared = false;

			// Update shader constants
			if (_constant_buffer != nullptr)
			{
				D3D11_MAPPED_SUBRESOURCE mapped;

				const HRESULT hr = _immediate_context->Map(_constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

				if (SUCCEEDED(hr))
				{
					CopyMemory(mapped.pData, _uniform_data_storage.data(), mapped.RowPitch);

					_immediate_context->Unmap(_constant_buffer, 0);
				}
				else
				{
					LOG(TRACE) << "Failed to map constant buffer! HRESULT is '" << hr << "'!";
				}
			}

			for (const auto &pass : static_cast<const d3d11_technique *>(technique)->passes)
			{
				// Setup states
				_immediate_context->VSSetShader(pass.VS, nullptr, 0);
				_immediate_context->PSSetShader(pass.PS, nullptr, 0);

				const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				_immediate_context->OMSetBlendState(pass.BS, blendfactor, D3D11_DEFAULT_SAMPLE_MASK);
				_immediate_context->OMSetDepthStencilState(pass.DSS, pass.StencilRef);

				// Save backbuffer of previous pass
				_immediate_context->CopyResource(_backbuffer_texture, _backbuffer_resolved);

				// Setup shader resources
				_immediate_context->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());
				_immediate_context->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());

				// Setup rendertargets
				if (static_cast<UINT>(pass.Viewport.Width) == _width && static_cast<UINT>(pass.Viewport.Height) == _height)
				{
					_immediate_context->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, _default_depthstencil);

					if (!is_default_depthstencil_cleared)
					{
						is_default_depthstencil_cleared = true;

						// Clear depthstencil
						_immediate_context->ClearDepthStencilView(_default_depthstencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
					}
				}
				else
				{
					_immediate_context->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, nullptr);
				}

				_immediate_context->RSSetViewports(1, &pass.Viewport);

				for (ID3D11RenderTargetView *const target : pass.RT)
				{
					if (target == nullptr)
					{
						continue;
					}

					const FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
					_immediate_context->ClearRenderTargetView(target, color);
				}

				// Draw triangle
				_immediate_context->Draw(3, 0);

				runtime::on_draw_call(3);

				// Reset rendertargets
				_immediate_context->OMSetRenderTargets(0, nullptr, nullptr);

				// Reset shader resources
				ID3D11ShaderResourceView *null[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
				_immediate_context->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);
				_immediate_context->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), null);

				// Update shader resources
				for (ID3D11ShaderResourceView *const resource : pass.RTSRV)
				{
					if (resource == nullptr)
					{
						continue;
					}

					D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
					resource->GetDesc(&srvdesc);

					if (srvdesc.Texture2D.MipLevels > 1)
					{
						_immediate_context->GenerateMips(resource);
					}
				}
			}
		}

		void d3d11_runtime::on_create_depthstencil_view(ID3D11Resource *resource, ID3D11DepthStencilView *depthstencil)
		{
			assert(resource != nullptr);
			assert(depthstencil != nullptr);

			const utils::critical_section::lock lock(_cs);

			// Do not track default depthstencil
			if (!_is_initialized)
			{
				return;
			}

			ID3D11Texture2D *texture = nullptr;
			const HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture));

			if (FAILED(hr))
			{
				return;
			}

			D3D11_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);

			SAFE_RELEASE(texture);

			// Early depthstencil rejection
			if (desc.Width != _width || desc.Height != _height || desc.SampleDesc.Count > 1)
			{
				return;
			}

			LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

			// Begin tracking new depthstencil
			const depth_source_info info = { desc.Width, desc.Height };
			_depth_source_table.emplace(depthstencil, info);
		}
		void d3d11_runtime::on_delete_depthstencil_view(ID3D11DepthStencilView *depthstencil)
		{
			assert(depthstencil != nullptr);

			const utils::critical_section::lock lock(_cs);

			const auto it = _depth_source_table.find(depthstencil);

			if (it != _depth_source_table.end())
			{
				LOG(TRACE) << "Removing depthstencil " << depthstencil << " from list of possible depth candidates ...";

				_depth_source_table.erase(it);

				if (depthstencil == _depthstencil)
				{
					create_depthstencil_replacement(nullptr);
				}
			}
		}
		void d3d11_runtime::on_set_depthstencil_view(ID3D11DepthStencilView *&depthstencil)
		{
			const utils::critical_section::lock lock(_cs);

			if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
			{
				depthstencil = _depthstencil_replacement;
			}
		}
		void d3d11_runtime::on_get_depthstencil_view(ID3D11DepthStencilView *&depthstencil)
		{
			const utils::critical_section::lock lock(_cs);

			if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil_replacement)
			{
				depthstencil->Release();

				depthstencil = _depthstencil;
				depthstencil->AddRef();
			}
		}
		void d3d11_runtime::on_clear_depthstencil_view(ID3D11DepthStencilView *&depthstencil)
		{
			const utils::critical_section::lock lock(_cs);

			if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
			{
				depthstencil = _depthstencil_replacement;
			}
		}
		void d3d11_runtime::on_copy_resource(ID3D11Resource *&dest, ID3D11Resource *&source)
		{
			const utils::critical_section::lock lock(_cs);

			if (_depthstencil_replacement != nullptr)
			{
				ID3D11Resource *resource = nullptr;
				_depthstencil->GetResource(&resource);

				if (dest == resource)
				{
					dest = _depthstencil_texture;
				}
				if (source == resource)
				{
					source = _depthstencil_texture;
				}

				resource->Release();
			}
		}

		void d3d11_runtime::screenshot(unsigned char *buffer) const
		{
			if (_backbuffer_format != DXGI_FORMAT_R8G8B8A8_UNORM && _backbuffer_format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && _backbuffer_format != DXGI_FORMAT_B8G8R8A8_UNORM && _backbuffer_format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			{
				LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbuffer_format << ".";
				return;
			}

			D3D11_TEXTURE2D_DESC texdesc;
			ZeroMemory(&texdesc, sizeof(D3D11_TEXTURE2D_DESC));
			texdesc.Width = _width;
			texdesc.Height = _height;
			texdesc.ArraySize = 1;
			texdesc.MipLevels = 1;
			texdesc.Format = _backbuffer_format;
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D11_USAGE_STAGING;
			texdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

			ID3D11Texture2D *texture_staging = nullptr;
			HRESULT hr = _device->CreateTexture2D(&texdesc, nullptr, &texture_staging);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create staging resource for screenshot capture! HRESULT is '" << hr << "'.";
				return;
			}

			_immediate_context->CopyResource(texture_staging, _backbuffer_resolved);

			D3D11_MAPPED_SUBRESOURCE mapped;
			hr = _immediate_context->Map(texture_staging, 0, D3D11_MAP_READ, 0, &mapped);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to map staging resource with screenshot capture! HRESULT is '" << hr << "'.";

				texture_staging->Release();
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

					if (texdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || texdesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
					{
						std::swap(pMem[x + 0], pMem[x + 2]);
					}
				}

				pMem += pitch;
				pMapped += mapped.RowPitch;
			}

			_immediate_context->Unmap(texture_staging, 0);

			texture_staging->Release();
		}
		bool d3d11_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors)
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

			d3d11_fx_compiler visitor(ast, skip_optimization);

			return visitor.compile(this, errors);
		}
		bool d3d11_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
		{
			d3d11_texture *const texture_impl = dynamic_cast<d3d11_texture *>(texture);

			assert(texture_impl != nullptr);
			assert(data != nullptr && size != 0);

			if (texture_impl->basetype != texture::datatype::image)
			{
				return false;
			}

			assert(texture->height != 0);

			_immediate_context->UpdateSubresource(texture_impl->TextureInterface, 0, nullptr, data, static_cast<UINT>(size / texture->height), static_cast<UINT>(size));

			if (texture->levels > 1)
			{
				_immediate_context->GenerateMips(texture_impl->ShaderResourceView[0]);
			}

			return true;
		}

		void d3d11_runtime::detect_depth_source()
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

			const utils::critical_section::lock lock(_cs);

			if (_is_multisampling_enabled || _depth_source_table.empty())
			{
				return;
			}

			depth_source_info best_info = { 0 };
			ID3D11DepthStencilView *best_match = nullptr;

			for (auto &it : _depth_source_table)
			{
				if (it.second.drawcall_count == 0)
				{
					continue;
				}
				else if ((it.second.vertices_count * (1.2f - it.second.drawcall_count / _stats.drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _stats.drawcalls)))
				{
					best_match = it.first;
					best_info = it.second;
				}

				it.second.drawcall_count = it.second.vertices_count = 0;
			}

			if (best_match != nullptr && _depthstencil != best_match)
			{
				LOG(TRACE) << "Switched depth source to depthstencil " << best_match << ".";

				create_depthstencil_replacement(best_match);
			}
		}
		bool d3d11_runtime::create_depthstencil_replacement(ID3D11DepthStencilView *depthstencil)
		{
			SAFE_RELEASE(_depthstencil);
			SAFE_RELEASE(_depthstencil_replacement);
			SAFE_RELEASE(_depthstencil_texture);
			SAFE_RELEASE(_depthstencil_texture_srv);

			if (depthstencil != nullptr)
			{
				_depthstencil = depthstencil;
				_depthstencil->AddRef();
				_depthstencil->GetResource(reinterpret_cast<ID3D11Resource **>(&_depthstencil_texture));
		
				D3D11_TEXTURE2D_DESC texdesc;
				_depthstencil_texture->GetDesc(&texdesc);

				HRESULT hr = S_OK;

				if ((texdesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0)
				{
					SAFE_RELEASE(_depthstencil_texture);

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

					texdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

					hr = _device->CreateTexture2D(&texdesc, nullptr, &_depthstencil_texture);

					if (SUCCEEDED(hr))
					{
						D3D11_DEPTH_STENCIL_VIEW_DESC dsvdesc;
						ZeroMemory(&dsvdesc, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
						dsvdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

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

						hr = _device->CreateDepthStencilView(_depthstencil_texture, &dsvdesc, &_depthstencil_replacement);
					}
				}

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement texture! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_depthstencil);
					SAFE_RELEASE(_depthstencil_texture);

					return false;
				}

				D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc;
				ZeroMemory(&srvdesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
				srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
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

				hr = _device->CreateShaderResourceView(_depthstencil_texture, &srvdesc, &_depthstencil_texture_srv);

				if (FAILED(hr))
				{
					LOG(TRACE) << "Failed to create depthstencil replacement resource view! HRESULT is '" << hr << "'.";

					SAFE_RELEASE(_depthstencil);
					SAFE_RELEASE(_depthstencil_replacement);
					SAFE_RELEASE(_depthstencil_texture);

					return false;
				}

				if (_depthstencil != _depthstencil_replacement)
				{
					// Update auto depthstencil
					depthstencil = nullptr;
					ID3D11RenderTargetView *targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };

					_immediate_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depthstencil);

					if (depthstencil != nullptr)
					{
						if (depthstencil == _depthstencil)
						{
							_immediate_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, _depthstencil_replacement);
						}

						depthstencil->Release();
					}

					for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
					{
						SAFE_RELEASE(targets[i]);
					}
				}
			}

			// Update effect textures
			for (const auto &it : _textures)
			{
				d3d11_texture *texture = static_cast<d3d11_texture *>(it.get());

				if (texture->basetype == texture::datatype::depthbuffer)
				{
					texture->change_data_source(texture::datatype::depthbuffer, this, _depthstencil_texture_srv, nullptr);
				}
			}

			return true;
		}
	}
}
