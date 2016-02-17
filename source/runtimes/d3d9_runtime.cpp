#include "log.hpp"
#include "d3d9_runtime.hpp"
#include "fx\ast.hpp"
#include "fx\compiler.hpp"
#include "gui.hpp"
#include "input.hpp"

#include <assert.h>
#include <d3dx9math.h>
#include <d3dcompiler.h>
#include <nanovg_d3d9.h>
#include <unordered_set>

const D3DFORMAT D3DFMT_INTZ = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));

namespace reshade
{
	namespace
	{
		inline bool is_pow2(int x)
		{
			return ((x > 0) && ((x & (x - 1)) == 0));
		}
	}

	struct d3d9_texture : public texture
	{
		com_ptr<IDirect3DTexture9> texture;
		com_ptr<IDirect3DSurface9> surface;
	};
	struct d3d9_sampler
	{
		DWORD States[12];
		d3d9_texture *Texture;
	};
	struct d3d9_pass : public technique::pass
	{
		d3d9_pass() : samplers(), sampler_count(0), render_targets() { }

		com_ptr<IDirect3DVertexShader9> vertex_shader;
		com_ptr<IDirect3DPixelShader9> pixel_shader;
		d3d9_sampler samplers[16];
		DWORD sampler_count;
		com_ptr<IDirect3DStateBlock9> stateblock;
		IDirect3DSurface9 *render_targets[8];
	};

	class d3d9_fx_compiler : fx::compiler
	{
	public:
		d3d9_fx_compiler(d3d9_runtime *runtime, const fx::nodetree &ast, std::string &errors, bool skipoptimization = false) : compiler(ast, errors), _runtime(runtime), _skip_shader_optimization(skipoptimization), _current_function(nullptr), _current_register_offset(0)
		{
		}

		bool run() override
		{
			for (auto node : _ast.structs)
			{
				visit(_global_code, static_cast<fx::nodes::struct_declaration_node *>(node));
			}

			for (auto node : _ast.uniforms)
			{
				const auto uniform = static_cast<fx::nodes::variable_declaration_node *>(node);

				if (uniform->type.is_texture())
				{
					visit_texture(uniform);
				}
				else if (uniform->type.is_sampler())
				{
					visit_sampler(uniform);
				}
				else if (uniform->type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
				{
					visit_uniform(uniform);
				}
				else
				{
					visit(_global_code, uniform);

					_global_code += ";\n";
				}
			}

			for (auto node : _ast.functions)
			{
				const auto function = static_cast<fx::nodes::function_declaration_node *>(node);

				_current_function = function;

				visit(_functions[function].code, function);
			}

			for (auto node : _ast.techniques)
			{
				visit_technique(static_cast<fx::nodes::technique_declaration_node *>(node));
			}

			return _success;
		}

	private:
		static inline D3DBLEND literal_to_blend_func(unsigned int value)
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
		static inline D3DSTENCILOP literal_to_stencil_op(unsigned int value)
		{
			if (value == fx::nodes::pass_declaration_node::states::ZERO)
			{
				return D3DSTENCILOP_ZERO;
			}

			return static_cast<D3DSTENCILOP>(value);
		}
		static D3DFORMAT literal_to_format(unsigned int value, texture::pixelformat &name)
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
			if (semantic.compare(0, 3, "SV_") == 0)
			{
				if (semantic == "SV_VERTEXID")
				{
					return "TEXCOORD0";
				}
				else if (semantic == "SV_POSITION")
				{
					return "POSITION";
				}
				else if (semantic.compare(0, 9, "SV_TARGET") == 0)
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

		using compiler::visit;
		void visit(std::string &output, const fx::nodes::type_node &type, bool with_qualifiers = true) override
		{
			if (with_qualifiers)
			{
				if (type.has_qualifier(fx::nodes::type_node::qualifier_static))
					output += "static ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_const))
					output += "const ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_volatile))
					output += "volatile ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_precise))
					output += "precise ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_linear))
					output += "linear ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_noperspective))
					output += "noperspective ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_centroid))
					output += "centroid ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_nointerpolation))
					output += "nointerpolation ";
				if (type.has_qualifier(fx::nodes::type_node::qualifier_inout))
					output += "inout ";
				else if (type.has_qualifier(fx::nodes::type_node::qualifier_in))
					output += "in ";
				else if (type.has_qualifier(fx::nodes::type_node::qualifier_out))
					output += "out ";
				else if (type.has_qualifier(fx::nodes::type_node::qualifier_uniform))
					output += "uniform ";
			}

			switch (type.basetype)
			{
				case fx::nodes::type_node::datatype_void:
					output += "void";
					break;
				case fx::nodes::type_node::datatype_bool:
					output += "bool";
					break;
				case fx::nodes::type_node::datatype_int:
					output += "int";
					break;
				case fx::nodes::type_node::datatype_uint:
					output += "uint";
					break;
				case fx::nodes::type_node::datatype_float:
					output += "float";
					break;
				case fx::nodes::type_node::datatype_sampler:
					output += "__sampler2D";
					break;
				case fx::nodes::type_node::datatype_struct:
					output += type.definition->unique_name;
					break;
			}

			if (type.is_matrix())
			{
				output += std::to_string(type.rows) + "x" + std::to_string(type.cols);
			}
			else if (type.is_vector())
			{
				output += std::to_string(type.rows);
			}
		}
		void visit(std::string &output, const fx::nodes::lvalue_expression_node *node) override
		{
			output += node->reference->unique_name;

			if (node->reference->type.is_sampler() && (_samplers.find(node->reference->name) != _samplers.end()))
			{
				_functions.at(_current_function).sampler_dependencies.insert(node->reference);
			}
		}
		void visit(std::string &output, const fx::nodes::literal_expression_node *node) override
		{
			if (!node->type.is_scalar())
			{
				visit(output, node->type, false);
						
				output += '(';
			}

			for (unsigned int i = 0; i < node->type.rows * node->type.cols; ++i)
			{
				switch (node->type.basetype)
				{
					case fx::nodes::type_node::datatype_bool:
						output += node->value_int[i] ? "true" : "false";
						break;
					case fx::nodes::type_node::datatype_int:
						output += std::to_string(node->value_int[i]);
						break;
					case fx::nodes::type_node::datatype_uint:
						output += std::to_string(node->value_uint[i]);
						break;
					case fx::nodes::type_node::datatype_float:
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
		void visit(std::string &output, const fx::nodes::expression_sequence_node *node) override
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
		void visit(std::string &output, const fx::nodes::unary_expression_node *node) override
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
					visit(part1, node->type, false);
					part1 += '(';
					part2 = ')';
					break;
			}

			output += part1;
			visit(output, node->operand);
			output += part2;
		}
		void visit(std::string &output, const fx::nodes::binary_expression_node *node) override
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

						if (is_pow2(value + 1))
						{
							output += "((" + std::to_string(value + 1) + ") * frac((";
							visit(output, node->operands[0]);
							output += ") / (" + std::to_string(value + 1) + ")))";
							return;
						}
						else if (is_pow2(value))
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
		void visit(std::string &output, const fx::nodes::intrinsic_expression_node *node) override
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
		void visit(std::string &output, const fx::nodes::conditional_expression_node *node) override
		{
			output += '(';
			visit(output, node->condition);
			output += " ? ";
			visit(output, node->expression_when_true);
			output += " : ";
			visit(output, node->expression_when_false);
			output += ')';
		}
		void visit(std::string &output, const fx::nodes::swizzle_expression_node *node) override
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
		void visit(std::string &output, const fx::nodes::field_expression_node *node) override
		{
			output += '(';
			visit(output, node->operand);
			output += '.';
			output += node->field_reference->unique_name;
			output += ')';
		}
		void visit(std::string &output, const fx::nodes::assignment_expression_node *node) override
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
		void visit(std::string &output, const fx::nodes::call_expression_node *node) override
		{
			output += node->callee->unique_name;
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
					
			info.sampler_dependencies.insert(infoCallee.sampler_dependencies.begin(), infoCallee.sampler_dependencies.end());

			for (auto dependency : infoCallee.dependencies)
			{
				if (std::find(info.dependencies.begin(), info.dependencies.end(), dependency) == info.dependencies.end())
				{
					info.dependencies.push_back(dependency);
				}
			}

			if (std::find(info.dependencies.begin(), info.dependencies.end(), node->callee) == info.dependencies.end())
			{
				info.dependencies.push_back(node->callee);
			}
		}
		void visit(std::string &output, const fx::nodes::constructor_expression_node *node) override
		{
			visit(output, node->type, false);

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
		void visit(std::string &output, const fx::nodes::initializer_list_node *node) override
		{
			output += "{ ";

			for (auto expression : node->values)
			{
				visit(output, expression);

				output += ", ";
			}

			output += " }";
		}
		void visit(std::string &output, const fx::nodes::compound_statement_node *node) override
		{
			output += "{\n";

			for (auto statement : node->statement_list)
			{
				visit(output, statement);
			}

			output += "}\n";
		}
		void visit(std::string &output, const fx::nodes::declarator_list_node *node, bool single_statement = false) override
		{
			bool with_type = true;

			for (auto declarator : node->declarator_list)
			{
				visit(output, declarator, with_type);

				if (single_statement)
				{
					output += ", ";

					with_type = false;
				}
				else
				{
					output += ";\n";
				}
			}

			if (single_statement)
			{
				output.erase(output.end() - 2, output.end());

				output += ";\n";
			}
		}
		void visit(std::string &output, const fx::nodes::expression_statement_node *node) override
		{
			visit(output, node->expression);

			output += ";\n";
		}
		void visit(std::string &output, const fx::nodes::if_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::switch_statement_node *node) override
		{
			warning(node->location, "switch statements do not currently support fallthrough in Direct3D9!");

			output += "[unroll] do { ";

			visit(output, node->test_expression->type, false);

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
		void visit(std::string &output, const fx::nodes::case_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::for_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::while_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::return_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::jump_statement_node *node) override
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
		void visit(std::string &output, const fx::nodes::struct_declaration_node *node)
		{
			output += "struct ";
			output += node->unique_name;
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
		void visit(std::string &output, const fx::nodes::variable_declaration_node *node, bool with_type = true, bool with_semantic = true)
		{
			if (with_type)
			{
				visit(output, node->type);

				output += ' ';
			}

			output += node->unique_name;

			if (node->type.is_array())
			{
				output += '[' + ((node->type.array_length > 0) ? std::to_string(node->type.array_length) : "") + ']';
			}

			if (with_semantic && !node->semantic.empty())
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
			visit(output, node->return_type, false);
					
			output += ' ';
			output += node->unique_name;
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
		void visit_texture(const fx::nodes::variable_declaration_node *node)
		{
			const auto obj = new d3d9_texture();
			obj->name = node->name;
			UINT width = obj->width = node->properties.Width;
			UINT height = obj->height = node->properties.Height;
			UINT levels = obj->levels = node->properties.MipLevels;
			const D3DFORMAT format = literal_to_format(node->properties.Format, obj->format);

			visit_annotation(node->annotations, *obj);

			if (node->semantic == "COLOR" || node->semantic == "SV_TARGET")
			{
				if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
				{
					warning(node->location, "texture properties on backbuffer textures are ignored");
				}

				_runtime->update_texture_datatype(obj, texture::datatype::backbuffer, _runtime->_backbuffer_texture);
			}
			else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
			{
				if (width != 1 || height != 1 || levels != 1 || format != D3DFMT_A8R8G8B8)
				{
					warning(node->location, "texture properties on depthbuffer textures are ignored");
				}

				_runtime->update_texture_datatype(obj, texture::datatype::depthbuffer, _runtime->_depthstencil_texture);
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

				hr = _runtime->_device->CreateTexture(width, height, levels, usage, format, D3DPOOL_DEFAULT, &obj->texture, nullptr);

				if (FAILED(hr))
				{
					error(node->location, "internal texture creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}

				hr = obj->texture->GetSurfaceLevel(0, &obj->surface);

				assert(SUCCEEDED(hr));
			}

			_runtime->add_texture(obj);
		}
		void visit_sampler(const fx::nodes::variable_declaration_node *node)
		{
			if (node->properties.Texture == nullptr)
			{
				error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
				return;
			}

			const auto texture = static_cast<d3d9_texture *>(_runtime->find_texture(node->properties.Texture->name));

			if (texture == nullptr)
			{
				error(node->location, "texture not found");
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
			visit(_global_code, node->type);
					
			_global_code += ' ';
			_global_code += node->unique_name;

			if (node->type.is_array())
			{
				_global_code += '[' + ((node->type.array_length > 0) ? std::to_string(node->type.array_length) : "") + ']';
			}

			_global_code += " : register(c" + std::to_string(_current_register_offset / 4) + ");\n";

			const auto obj = new uniform();
			obj->name = node->name;
			obj->rows = node->type.rows;
			obj->columns = node->type.cols;
			obj->elements = node->type.array_length;
			obj->storage_size = obj->rows * obj->columns * std::max(1u, obj->elements);

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

			const UINT regsize = static_cast<UINT>(static_cast<float>(obj->storage_size) / sizeof(float));
			const UINT regalignment = 4 - (regsize % 4);

			obj->storage_offset = _current_register_offset * sizeof(float);
			_current_register_offset += regsize + regalignment;

			visit_annotation(node->annotations, *obj);

			auto &uniform_storage = _runtime->get_uniform_value_storage();

			if (_current_register_offset * sizeof(float) >= uniform_storage.size())
			{
				uniform_storage.resize(uniform_storage.size() + 64 * sizeof(float));
			}

			_runtime->_constant_register_count = _current_register_offset / 4;

			if (node->initializer_expression != nullptr && node->initializer_expression->id == fx::nodeid::literal_expression)
			{
				CopyMemory(uniform_storage.data() + obj->storage_offset, &static_cast<const fx::nodes::literal_expression_node *>(node->initializer_expression)->value_float, obj->storage_size);
			}
			else
			{
				ZeroMemory(uniform_storage.data() + obj->storage_offset, obj->storage_size);
			}

			_runtime->add_uniform(obj);
		}
		void visit_technique(const fx::nodes::technique_declaration_node *node)
		{
			const auto obj = new technique();
			obj->name = node->name;

			visit_annotation(node->annotation_list, *obj);

			for (auto pass : node->pass_list)
			{
				obj->passes.emplace_back(new d3d9_pass());
				visit_pass(pass, *static_cast<d3d9_pass *>(obj->passes.back().get()));
			}

			_runtime->add_technique(obj);
		}
		void visit_pass(const fx::nodes::pass_declaration_node *node, d3d9_pass & pass)
		{
			pass.render_targets[0] = _runtime->_backbuffer_resolved.get();

			std::string samplers;
			const char shader_types[2][3] = { "vs", "ps" };
			const fx::nodes::function_declaration_node *shader_functions[2] = { node->states.VertexShader, node->states.PixelShader };

			for (unsigned int i = 0; i < 2; i++)
			{
				if (shader_functions[i] == nullptr)
				{
					continue;
				}

				for (auto sampler : _functions.at(shader_functions[i]).sampler_dependencies)
				{
					pass.samplers[pass.sampler_count] = _samplers.at(sampler->name);
					const auto *const texture = sampler->properties.Texture;

					samplers += "sampler2D __Sampler";
					samplers += sampler->unique_name;
					samplers += " : register(s" + std::to_string(pass.sampler_count++) + ");\n";
					samplers += "static const __sampler2D ";
					samplers += sampler->unique_name;
					samplers += " = { __Sampler";
					samplers += sampler->unique_name;

					if (texture->semantic == "COLOR" || texture->semantic == "SV_TARGET" || texture->semantic == "DEPTH" || texture->semantic == "SV_DEPTH")
					{
						samplers += ", float2(" + std::to_string(1.0f / _runtime->frame_width()) + ", " + std::to_string(1.0f / _runtime->frame_height()) + ")";
					}
					else
					{
						samplers += ", float2(" + std::to_string(1.0f / texture->properties.Width) + ", " + std::to_string(1.0f / texture->properties.Height) + ")";
					}

					samplers += " };\n";

					if (pass.sampler_count == 16)
					{
						error(node->location, "maximum sampler count of 16 reached in pass '" + node->name + "'");
						return;
					}
				}
			}

			for (unsigned int i = 0; i < 2; ++i)
			{
				if (shader_functions[i] != nullptr)
				{
					visit_pass_shader(shader_functions[i], shader_types[i], samplers, pass);
				}
			}

			const auto &device = _runtime->_device;
			const HRESULT hr = device->BeginStateBlock();

			if (FAILED(hr))
			{
				error(node->location, "internal pass stateblock creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			device->SetVertexShader(pass.vertex_shader.get());
			device->SetPixelShader(pass.pixel_shader.get());

			device->SetRenderState(D3DRS_ZENABLE, node->states.DepthEnable);
			device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
			device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
			device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
			device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device->SetRenderState(D3DRS_LASTPIXEL, TRUE);
			device->SetRenderState(D3DRS_SRCBLEND, literal_to_blend_func(node->states.SrcBlend));
			device->SetRenderState(D3DRS_DESTBLEND, literal_to_blend_func(node->states.DestBlend));
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
			device->SetRenderState(D3DRS_STENCILPASS, literal_to_stencil_op(node->states.StencilOpPass));
			device->SetRenderState(D3DRS_STENCILFAIL, literal_to_stencil_op(node->states.StencilOpFail));
			device->SetRenderState(D3DRS_STENCILZFAIL, literal_to_stencil_op(node->states.StencilOpDepthFail));
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

			device->EndStateBlock(&pass.stateblock);

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
					warning(node->location, "device only supports " + std::to_string(caps.NumSimultaneousRTs) + " simultaneous render targets, but pass '" + node->name + "' uses more, which are ignored");
					break;
				}

				const auto texture = static_cast<d3d9_texture *>(_runtime->find_texture(node->states.RenderTargets[i]->name));

				if (texture == nullptr)
				{
					error(node->location, "texture not found");
					return;
				}

				pass.render_targets[i] = texture->surface.get();
			}
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

			for (auto dependency : _functions.at(node).dependencies)
			{
				source += _functions.at(dependency).code;
			}

			source += _functions.at(node).code;

			std::string position_variable, initialization;
			fx::nodes::type_node return_type = node->return_type;

			if (node->return_type.is_struct())
			{
				for (auto field : node->return_type.definition->field_list)
				{
					if (field->semantic == "SV_POSITION" || field->semantic == "POSITION")
					{
						position_variable = "_return." + field->name;
						break;
					}
					else if ((field->semantic.compare(0, 9, "SV_TARGET") == 0 || field->semantic.compare(0, 5, "COLOR") == 0) && field->type.rows != 4)
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
					position_variable = "_return";
				}
				else if (node->return_semantic.compare(0, 9, "SV_TARGET") == 0 || node->return_semantic.compare(0, 5, "COLOR") == 0)
				{
					return_type.rows = 4;
				}
			}

			visit(source, return_type, false);
					
			source += " __main(";
				
			if (!node->parameter_list.empty())
			{
				for (auto parameter : node->parameter_list)
				{
					fx::nodes::type_node parameter_type = parameter->type;

					if (parameter->type.has_qualifier(fx::nodes::type_node::qualifier_out))
					{
						if (parameter_type.is_struct())
						{
							for (auto field : parameter_type.definition->field_list)
							{
								if (field->semantic == "SV_POSITION" || field->semantic == "POSITION")
								{
									position_variable = parameter->name + '.' + field->name;
									break;
								}
								else if ((field->semantic.compare(0, 9, "SV_TARGET") == 0 || field->semantic.compare(0, 5, "COLOR") == 0) && field->type.rows != 4)
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
								position_variable = parameter->name;
							}
							else if (parameter->semantic.compare(0, 9, "SV_TARGET") == 0 || parameter->semantic.compare(0, 5, "COLOR") == 0)
							{
								parameter_type.rows = 4;

								initialization += parameter->name + " = float4(0.0f, 0.0f, 0.0f, 0.0f);\n";
							}
						}
					}

					visit(source, parameter_type);
						
					source += ' ' + parameter->name;

					if (parameter_type.is_array())
					{
						source += '[' + ((parameter_type.array_length >= 1) ? std::to_string(parameter_type.array_length) : "") + ']';
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
				visit(source, return_type, false);
						
				source += " _return = ";
			}

			if (node->return_type.rows != return_type.rows)
			{
				source += "float4(";
			}

			source += node->unique_name;
			source += '(';

			if (!node->parameter_list.empty())
			{
				for (auto parameter : node->parameter_list)
				{
					source += parameter->name;

					if (parameter->semantic.compare(0, 9, "SV_TARGET") == 0 || parameter->semantic.compare(0, 5, "COLOR") == 0)
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

			if (node->return_type.rows != return_type.rows)
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
				source += position_variable + ".xy += __TEXEL_SIZE__ * " + position_variable + ".ww;\n";
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
				error(node->location, "internal shader compilation failed");
				return;
			}

			if (shadertype == "vs")
			{
				hr = _runtime->_device->CreateVertexShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.vertex_shader);
			}
			else if (shadertype == "ps")
			{
				hr = _runtime->_device->CreatePixelShader(static_cast<const DWORD *>(compiled->GetBufferPointer()), &pass.pixel_shader);
			}

			compiled->Release();

			if (FAILED(hr))
			{
				error(node->location, "internal shader creation failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}
		}

	private:
		struct function
		{
			std::string code;
			std::vector<const fx::nodes::function_declaration_node *> dependencies;
			std::unordered_set<const fx::nodes::variable_declaration_node *> sampler_dependencies;
		};

		d3d9_runtime *_runtime;
		std::string _global_code, _global_uniforms;
		bool _skip_shader_optimization;
		unsigned int _current_register_offset;
		const fx::nodes::function_declaration_node *_current_function;
		std::unordered_map<std::string, d3d9_sampler> _samplers;
		std::unordered_map<const fx::nodes::function_declaration_node *, function> _functions;
	};

	d3d9_runtime::d3d9_runtime(IDirect3DDevice9 *device, IDirect3DSwapChain9 *swapchain) : runtime(D3D_FEATURE_LEVEL_9_3), _device(device), _swapchain(swapchain), _is_multisampling_enabled(false), _backbuffer_format(D3DFMT_UNKNOWN), _constant_register_count(0)
	{
		assert(device != nullptr);
		assert(swapchain != nullptr);

		_device->GetDirect3D(&_d3d);

		D3DCAPS9 caps;
		D3DADAPTER_IDENTIFIER9 adapter_desc;
		D3DDEVICE_CREATION_PARAMETERS creation_params;

		_device->GetDeviceCaps(&caps);
		_device->GetCreationParameters(&creation_params);
		_d3d->GetAdapterIdentifier(creation_params.AdapterOrdinal, 0, &adapter_desc);

		_vendor_id = adapter_desc.VendorId;
		_device_id = adapter_desc.DeviceId;
		_behavior_flags = creation_params.BehaviorFlags;
		_num_simultaneous_rendertargets = std::min(caps.NumSimultaneousRTs, 8ul);
	}

	bool d3d9_runtime::on_init(const D3DPRESENT_PARAMETERS &pp)
	{
		_width = pp.BackBufferWidth;
		_height = pp.BackBufferHeight;
		_backbuffer_format = pp.BackBufferFormat;
		_is_multisampling_enabled = pp.MultiSampleType != D3DMULTISAMPLE_NONE;
		input::register_window(pp.hDeviceWindow, _input);

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

				return false;
			}
		}
		else
		{
			_backbuffer_resolved = _backbuffer;
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

			return false;
		}
		#pragma endregion

		#pragma region Create default depthstencil surface
		hr = _device->CreateDepthStencilSurface(_width, _height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &_default_depthstencil, nullptr);

		if (FAILED(hr))
		{
			LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

			return nullptr;
		}
		#pragma endregion

		#pragma region Create effect stateblock and objects
		hr = _device->CreateStateBlock(D3DSBT_ALL, &_stateblock);

		if (FAILED(hr))
		{
			LOG(TRACE) << "Failed to create stateblock! HRESULT is '" << hr << "'.";

			return false;
		}

		hr = _device->CreateVertexBuffer(3 * sizeof(float), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &_effect_triangle_buffer, nullptr);

		if (SUCCEEDED(hr))
		{
			float *data = nullptr;

			hr = _effect_triangle_buffer->Lock(0, 3 * sizeof(float), reinterpret_cast<void **>(&data), 0);

			assert(SUCCEEDED(hr));

			for (UINT i = 0; i < 3; i++)
			{
				data[i] = static_cast<float>(i);
			}

			_effect_triangle_buffer->Unlock();
		}
		else
		{
			LOG(TRACE) << "Failed to create effect vertexbuffer! HRESULT is '" << hr << "'.";

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

			return false;
		}
		#pragma endregion

		_gui.reset(new gui(this, nvgCreateD3D9(_device.get(), 0)));

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
		_stateblock.reset();

		_backbuffer.reset();
		_backbuffer_resolved.reset();
		_backbuffer_texture.reset();
		_backbuffer_texture_surface.reset();

		_depthstencil.reset();
		_depthstencil_replacement.reset();
		_depthstencil_texture.reset();

		_default_depthstencil.reset();

		_effect_triangle_buffer.reset();
		_effect_triangle_layout.reset();

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
		com_ptr<IDirect3DSurface9> stateblock_rendertargets[8], stateblock_depthstencil;

		_device->GetViewport(&viewport);

		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
		{
			_device->GetRenderTarget(target, &stateblock_rendertargets[target]);
		}

		_device->GetDepthStencilSurface(&stateblock_depthstencil);

		if ((_behavior_flags & D3DCREATE_MIXED_VERTEXPROCESSING) != 0)
		{
			software_rendering_enabled = _device->GetSoftwareVertexProcessing();

			_device->SetSoftwareVertexProcessing(FALSE);
		}

		// Resolve back buffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->StretchRect(_backbuffer.get(), nullptr, _backbuffer_resolved.get(), nullptr, D3DTEXF_NONE);
		}

		// Apply post processing
		on_apply_effect();

		// Reset render target
		_device->SetRenderTarget(0, _backbuffer_resolved.get());
		_device->SetDepthStencilSurface(_default_depthstencil.get());
		_device->Clear(0, nullptr, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

		// Apply presenting
		runtime::on_present();

		// Copy to back buffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer.get(), nullptr, D3DTEXF_NONE);
		}

		// Apply previous device state
		_stateblock->Apply();

		for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
		{
			_device->SetRenderTarget(target, stateblock_rendertargets[target].get());
		}

		_device->SetDepthStencilSurface(stateblock_depthstencil.get());

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

		com_ptr<IDirect3DSurface9> depthstencil;
		_device->GetDepthStencilSurface(&depthstencil);

		if (depthstencil != nullptr)
		{
			if (depthstencil == _depthstencil_replacement)
			{
				depthstencil = _depthstencil;
			}

			const auto it = _depth_source_table.find(depthstencil.get());

			if (it != _depth_source_table.end())
			{
				it->second.drawcall_count = static_cast<float>(_drawcalls);
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

		_device->SetRenderTarget(0, _backbuffer_resolved.get());
		_device->SetDepthStencilSurface(nullptr);

		// Setup vertex input
		_device->SetStreamSource(0, _effect_triangle_buffer.get(), 0, sizeof(float));
		_device->SetVertexDeclaration(_effect_triangle_layout.get());

		// Apply post processing
		runtime::on_apply_effect();
	}
	void d3d9_runtime::on_apply_effect_technique(const technique &technique)
	{
		runtime::on_apply_effect_technique(technique);

		bool is_default_depthstencil_cleared = false;

		// Setup shader constants
		const auto uniform_storage_data = reinterpret_cast<const float *>(get_uniform_value_storage().data());
		_device->SetVertexShaderConstantF(0, uniform_storage_data, _constant_register_count);
		_device->SetPixelShaderConstantF(0, uniform_storage_data, _constant_register_count);

		for (const auto &pass_ptr : technique.passes)
		{
			const auto &pass = *static_cast<const d3d9_pass *>(pass_ptr.get());

			// Setup states
			pass.stateblock->Apply();

			// Save back buffer of previous pass
			_device->StretchRect(_backbuffer_resolved.get(), nullptr, _backbuffer_texture_surface.get(), nullptr, D3DTEXF_NONE);

			// Setup shader resources
			for (DWORD sampler = 0; sampler < pass.sampler_count; sampler++)
			{
				_device->SetTexture(sampler, pass.samplers[sampler].Texture->texture.get());

				for (DWORD state = D3DSAMP_ADDRESSU; state <= D3DSAMP_SRGBTEXTURE; state++)
				{
					_device->SetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(state), pass.samplers[sampler].States[state]);
				}
			}

			// Setup render targets
			for (DWORD target = 0; target < _num_simultaneous_rendertargets; target++)
			{
				_device->SetRenderTarget(target, pass.render_targets[target]);
			}

			D3DVIEWPORT9 viewport;
			_device->GetViewport(&viewport);

			const float texelsize[4] = { -1.0f / viewport.Width, 1.0f / viewport.Height };
			_device->SetVertexShaderConstantF(255, texelsize, 1);

			const bool is_viewport_sized = viewport.Width == _width && viewport.Height == _height;

			_device->SetDepthStencilSurface(is_viewport_sized ? _default_depthstencil.get() : nullptr);

			if (is_viewport_sized && !is_default_depthstencil_cleared)
			{
				is_default_depthstencil_cleared = true;

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
			for (const auto target : pass.render_targets)
			{
				if (target == nullptr || target == _backbuffer_resolved)
				{
					continue;
				}

				com_ptr<IDirect3DBaseTexture9> texture;

				if (SUCCEEDED(target->GetContainer(IID_PPV_ARGS(&texture))) && texture->GetLevelCount() > 1)
				{
					texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
					texture->GenerateMipSubLevels();
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

			// Early rejection
			if ((desc.Width < _width * 0.95 || desc.Width > _width * 1.05) || (desc.Height < _height * 0.95 || desc.Height > _height * 1.05) || desc.MultiSampleType != D3DMULTISAMPLE_NONE)
			{
				return;
			}
	
			LOG(TRACE) << "Adding depthstencil " << depthstencil << " (Width: " << desc.Width << ", Height: " << desc.Height << ", Format: " << desc.Format << ") to list of possible depth candidates ...";

			depthstencil->AddRef();

			// Begin tracking
			const depth_source_info info = { desc.Width, desc.Height };
			_depth_source_table.emplace(depthstencil, info);
		}

		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
		{
			depthstencil = _depthstencil_replacement.get();
		}
	}
	void d3d9_runtime::on_get_depthstencil_surface(IDirect3DSurface9 *&depthstencil)
	{
		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil_replacement)
		{
			depthstencil->Release();

			depthstencil = _depthstencil.get();

			depthstencil->AddRef();
		}
	}

	void d3d9_runtime::screenshot(unsigned char *buffer) const
	{
		if (_backbuffer_format != D3DFMT_X8R8G8B8 &&
			_backbuffer_format != D3DFMT_X8B8G8R8 &&
			_backbuffer_format != D3DFMT_A8R8G8B8 &&
			_backbuffer_format != D3DFMT_A8B8G8R8)
		{
			LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbuffer_format << ".";
			return;
		}

		HRESULT hr;
		com_ptr<IDirect3DSurface9> screenshot_surface;

		hr = _device->CreateOffscreenPlainSurface(_width, _height, _backbuffer_format, D3DPOOL_SYSTEMMEM, &screenshot_surface, nullptr);

		if (FAILED(hr))
		{
			return;
		}

		hr = _device->GetRenderTargetData(_backbuffer_resolved.get(), screenshot_surface.get());

		if (FAILED(hr))
		{
			return;
		}

		D3DLOCKED_RECT mapped_rect;
		hr = screenshot_surface->LockRect(&mapped_rect, nullptr, D3DLOCK_READONLY);

		if (FAILED(hr))
		{
			return;
		}

		auto mapped_data = static_cast<BYTE *>(mapped_rect.pBits);
		const UINT pitch = _width * 4;

		for (UINT y = 0; y < _height; y++)
		{
			CopyMemory(buffer, mapped_data, std::min(pitch, static_cast<UINT>(mapped_rect.Pitch)));

			for (UINT x = 0; x < pitch; x += 4)
			{
				buffer[x + 3] = 0xFF;

				if (_backbuffer_format == D3DFMT_A8R8G8B8 || _backbuffer_format == D3DFMT_X8R8G8B8)
				{
					std::swap(buffer[x + 0], buffer[x + 2]);
				}
			}

			buffer += pitch;
			mapped_data += mapped_rect.Pitch;
		}

		screenshot_surface->UnlockRect();
	}
	bool d3d9_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors)
	{
		bool skip_optimization = false;

		for (const auto &pragma : pragmas)
		{
			fx::lexer lexer(pragma);

			const auto prefix_token = lexer.lex();

			if (prefix_token.literal_as_string != "reshade")
			{
				continue;
			}

			const auto command_token = lexer.lex();

			if (command_token.literal_as_string == "skipoptimization" || command_token.literal_as_string == "nooptimization")
			{
				skip_optimization = true;
			}
		}

		return d3d9_fx_compiler(this, ast, errors, skip_optimization).run();
	}
	bool d3d9_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
	{
		const auto texture_impl = dynamic_cast<d3d9_texture *>(texture);

		assert(texture_impl != nullptr);
		assert(data != nullptr && size != 0);

		if (texture_impl->basetype != texture::datatype::image)
		{
			return false;
		}

		D3DSURFACE_DESC desc;
		texture_impl->texture->GetLevelDesc(0, &desc);

		com_ptr<IDirect3DTexture9> mem_texture;

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

		hr = _device->UpdateTexture(mem_texture.get(), texture_impl->texture.get());

		if (FAILED(hr))
		{
			LOG(TRACE) << "Failed to update texture from memory texture! HRESULT is '" << hr << "'.";

			return false;
		}

		return true;
	}
	void d3d9_runtime::update_texture_datatype(texture *texture, texture::datatype source, const com_ptr<IDirect3DTexture9> &newtexture)
	{
		const auto texture_impl = static_cast<d3d9_texture *>(texture);

		texture_impl->basetype = source;

		if (texture_impl->texture == newtexture)
		{
			return;
		}

		texture_impl->texture.reset();
		texture_impl->surface.reset();

		if (newtexture != nullptr)
		{
			texture_impl->texture = newtexture;
			newtexture->GetSurfaceLevel(0, &texture_impl->surface);

			D3DSURFACE_DESC desc;
			texture_impl->surface->GetDesc(&desc);

			texture_impl->width = desc.Width;
			texture_impl->height = desc.Height;
			texture_impl->format = texture::pixelformat::unknown;
			texture_impl->levels = newtexture->GetLevelCount();
		}
		else
		{
			texture_impl->width = texture_impl->height = texture_impl->levels = 0;
			texture_impl->format = texture::pixelformat::unknown;
		}
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
			if (com_ptr<IDirect3DSurface9>(it->first).ref_count() == 1)
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
			else if ((it->second.vertices_count * (1.2f - it->second.drawcall_count / _drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _drawcalls)))
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
		_depthstencil.reset();
		_depthstencil_replacement.reset();
		_depthstencil_texture.reset();

		if (depthstencil != nullptr)
		{
			_depthstencil = depthstencil;

			D3DSURFACE_DESC desc;
			_depthstencil->GetDesc(&desc);

			if (desc.Format != D3DFMT_INTZ)
			{
				const HRESULT hr = _device->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_INTZ, D3DPOOL_DEFAULT, &_depthstencil_texture, nullptr);

				if (SUCCEEDED(hr))
				{
					_depthstencil_texture->GetSurfaceLevel(0, &_depthstencil_replacement);

					// Update auto depth-stencil
					com_ptr<IDirect3DSurface9> current_depthstencil;
					_device->GetDepthStencilSurface(&current_depthstencil);

					if (current_depthstencil != nullptr)
					{
						if (current_depthstencil == _depthstencil)
						{
							_device->SetDepthStencilSurface(_depthstencil_replacement.get());
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

				_depthstencil_replacement->GetContainer(IID_PPV_ARGS(&_depthstencil_texture));
			}
		}

		// Update effect textures
		for (const auto &texture : _textures)
		{
			if (texture->basetype == texture::datatype::depthbuffer)
			{
				update_texture_datatype(texture.get(), texture::datatype::depthbuffer, _depthstencil_texture);
			}
		}

		return true;
	}
}
