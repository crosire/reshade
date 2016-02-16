#include "log.hpp"
#include "d3d10_runtime.hpp"
#include "fx\ast.hpp"
#include "fx\compiler.hpp"
#include "gui.hpp"
#include "input.hpp"
#include "utils\com.hpp"

#include <assert.h>
#include <d3dcompiler.h>
#include <nanovg_d3d10.h>
#include <boost\algorithm\string.hpp>

namespace reshade
{
	struct d3d10_pass
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
	struct d3d10_technique : public technique
	{
		~d3d10_technique()
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

		std::vector<d3d10_pass> passes;
	};
	struct d3d10_texture : public texture
	{
		d3d10_texture() : ShaderRegister(0), TextureInterface(nullptr), ShaderResourceView(), RenderTargetView()
		{
		}
		~d3d10_texture()
		{
			safe_release(RenderTargetView[0]);
			safe_release(RenderTargetView[1]);
			safe_release(ShaderResourceView[0]);
			safe_release(ShaderResourceView[1]);

			safe_release(TextureInterface);
		}

		size_t ShaderRegister;
		ID3D10Texture2D *TextureInterface;
		ID3D10ShaderResourceView *ShaderResourceView[2];
		ID3D10RenderTargetView *RenderTargetView[2];
	};

	class d3d10_fx_compiler : fx::compiler
	{
	public:
		d3d10_fx_compiler(d3d10_runtime *runtime, const fx::nodetree &ast, std::string &errors, bool skipoptimization = false) : compiler(ast, errors), _runtime(runtime), _skip_shader_optimization(skipoptimization), _is_in_parameter_block(false), _is_in_function_block(false), _current_global_size(0)
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

				visit(_global_code, function);
			}
			for (auto node : _ast.techniques)
			{
				visit_technique(static_cast<fx::nodes::technique_declaration_node *>(node));
			}

			if (_current_global_size != 0)
			{
				CD3D10_BUFFER_DESC globalsDesc(RoundToMultipleOf16(_current_global_size), D3D10_BIND_CONSTANT_BUFFER, D3D10_USAGE_DYNAMIC, D3D10_CPU_ACCESS_WRITE);
				D3D10_SUBRESOURCE_DATA globalsInitial;
				globalsInitial.pSysMem = _runtime->get_uniform_value_storage().data();
				globalsInitial.SysMemPitch = globalsInitial.SysMemSlicePitch = _runtime->_constant_buffer_size = _current_global_size;
				_runtime->_device->CreateBuffer(&globalsDesc, &globalsInitial, &_runtime->_constant_buffer);
			}

			return _success;
		}

		static inline UINT RoundToMultipleOf16(UINT size)
		{
			return (size + 15) & ~15;
		}
		static D3D10_STENCIL_OP LiteralToStencilOp(unsigned int value)
		{
			if (value == fx::nodes::pass_declaration_node::states::ZERO)
			{
				return D3D10_STENCIL_OP_ZERO;
			}
							
			return static_cast<D3D10_STENCIL_OP>(value);
		}
		static D3D10_BLEND LiteralToBlend(unsigned int value)
		{
			switch (value)
			{
				case fx::nodes::pass_declaration_node::states::ZERO:
					return D3D10_BLEND_ZERO;
				case fx::nodes::pass_declaration_node::states::ONE:
					return D3D10_BLEND_ONE;
			}

			return static_cast<D3D10_BLEND>(value);
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

	private:
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

		using compiler::visit;
		void visit(std::string &output, const fx::nodes::type_node &type, bool with_qualifiers = true) override
		{
			if (with_qualifiers)
			{
				if (type.has_qualifier(fx::nodes::type_node::qualifier_extern))
					output += "extern ";
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

			output.pop_back();
			output.pop_back();

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
					
			output.pop_back();
			output.pop_back();

			output += ')';
		}
		void visit(std::string &output, const fx::nodes::unary_expression_node *node) override
		{
			std::string part2;

			switch (node->op)
			{
				case fx::nodes::unary_expression_node::negate:
					output += '-';
					break;
				case fx::nodes::unary_expression_node::bitwise_not:
					output += "~";
					break;
				case fx::nodes::unary_expression_node::logical_not:
					output += '!';
					break;
				case fx::nodes::unary_expression_node::pre_increase:
					output += "++";
					break;
				case fx::nodes::unary_expression_node::pre_decrease:
					output += "--";
					break;
				case fx::nodes::unary_expression_node::post_increase:
					part2 = "++";
					break;
				case fx::nodes::unary_expression_node::post_decrease:
					part2 = "--";
					break;
				case fx::nodes::unary_expression_node::cast:
					visit(output, node->type, false);
					output += '(';
					part2 = ')';
					break;
			}

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

						output += "__tex2Dgatheroffset" + std::to_string(component) + "(";
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
				const char swizzle[16][5] =
				{
					"_m00", "_m01", "_m02", "_m03",
					"_m10", "_m11", "_m12", "_m13",
					"_m20", "_m21", "_m22", "_m23",
					"_m30", "_m31", "_m32", "_m33"
				};

				for (int i = 0; i < 4 && node->mask[i] >= 0; ++i)
				{
					output += swizzle[node->mask[i]];
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
			output += '(';
			visit(output, node->left);
			output += ' ';

			switch (node->op)
			{
				case fx::nodes::assignment_expression_node::none:
					output += '=';
					break;
				case fx::nodes::assignment_expression_node::add:
					output += "+=";
					break;
				case fx::nodes::assignment_expression_node::subtract:
					output += "-=";
					break;
				case fx::nodes::assignment_expression_node::multiply:
					output += "*=";
					break;
				case fx::nodes::assignment_expression_node::divide:
					output += "/=";
					break;
				case fx::nodes::assignment_expression_node::modulo:
					output += "%=";
					break;
				case fx::nodes::assignment_expression_node::left_shift:
					output += "<<=";
					break;
				case fx::nodes::assignment_expression_node::right_shift:
					output += ">>=";
					break;
				case fx::nodes::assignment_expression_node::bitwise_and:
					output += "&=";
					break;
				case fx::nodes::assignment_expression_node::bitwise_or:
					output += "|=";
					break;
				case fx::nodes::assignment_expression_node::bitwise_xor:
					output += "^=";
					break;
			}

			output += ' ';
			visit(output, node->right);
			output += ')';
		}
		void visit(std::string &output, const fx::nodes::call_expression_node *node) override
		{
			output += node->callee->unique_name;
			output += '(';

			for (auto argument : node->arguments)
			{
				visit(output, argument);

				output += ", ";
			}

			if (!node->arguments.empty())
			{
				output.pop_back();
				output.pop_back();
			}

			output += ')';
		}
		void visit(std::string &output, const fx::nodes::constructor_expression_node *node) override
		{
			visit(output, node->type, false);

			output += '(';

			for (auto argument : node->arguments)
			{
				visit(output, argument);

				output += ", ";
			}

			if (!node->arguments.empty())
			{
				output.pop_back();
				output.pop_back();
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
		void visit(std::string &output, const fx::nodes::declarator_list_node *node, bool single_statement) override
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
				output += '[';
				output += attribute;
				output += ']';
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
			for (auto &attribute : node->attributes)
			{
				output += '[';
				output += attribute;
				output += ']';
			}

			output += "switch (";
			visit(output, node->test_expression);
			output += ")\n{\n";

			for (auto cases : node->case_list)
			{
				visit(output, cases);
			}

			output += "}\n";
		}
		void visit(std::string &output, const fx::nodes::case_statement_node *node) override
		{
			for (auto label : node->labels)
			{
				if (label == nullptr)
				{
					output += "default";
				}
				else
				{
					output += "case ";

					visit(output, label);
				}

				output += ":\n";
			}

			visit(output, node->statement_list);
		}
		void visit(std::string &output, const fx::nodes::for_statement_node *node) override
		{
			for (auto &attribute : node->attributes)
			{
				output += '[';
				output += attribute;
				output += ']';
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
				output += '[';
				output += attribute;
				output += ']';
			}

			if (node->is_do_while)
			{
				output += "do\n{\n";

				if (node->statement_list != nullptr)
				{
					visit(output, node->statement_list);
				}

				output += "}\n";
				output += "while (";
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
				}
			}
			else
			{
				output += "float _dummy;\n";
			}

			output += "};\n";
		}
		void visit(std::string &output, const fx::nodes::variable_declaration_node *node, bool with_type = true)
		{
			if (with_type)
			{
				visit(output, node->type);

				output += ' ';
			}

			if (!node->name.empty())
			{
				output += node->unique_name;
			}

			if (node->type.is_array())
			{
				output += '[';
				output += (node->type.array_length >= 1) ? std::to_string(node->type.array_length) : "";
				output += ']';
			}

			if (!node->semantic.empty())
			{
				output += " : " + convert_semantic(node->semantic);
			}

			if (node->initializer_expression != nullptr)
			{
				output += " = ";

				visit(output, node->initializer_expression);
			}

			if (!(_is_in_parameter_block || _is_in_function_block))
			{
				output += ";\n";
			}
		}
		void visit(std::string &output, const fx::nodes::function_declaration_node *node)
		{
			visit(output, node->return_type, false);

			output += ' ';
			output += node->unique_name;
			output += '(';

			_is_in_parameter_block = true;

			for (auto parameter : node->parameter_list)
			{
				visit(output, parameter);

				output += ", ";
			}

			_is_in_parameter_block = false;

			if (!node->parameter_list.empty())
			{
				output.pop_back();
				output.pop_back();
			}

			output += ')';

			if (!node->return_semantic.empty())
			{
				output += " : " + convert_semantic(node->return_semantic);
			}

			output += '\n';

			_is_in_function_block = true;

			visit(output, node->definition);

			_is_in_function_block = false;
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
			D3D10_TEXTURE2D_DESC texdesc;
			ZeroMemory(&texdesc, sizeof(D3D10_TEXTURE2D_DESC));

			d3d10_texture *obj = new d3d10_texture();
			obj->name = node->name;
			texdesc.Width = obj->width = node->properties.Width;
			texdesc.Height = obj->height = node->properties.Height;
			texdesc.MipLevels = obj->levels = node->properties.MipLevels;
			texdesc.ArraySize = 1;
			texdesc.Format = LiteralToFormat(node->properties.Format, obj->format);
			texdesc.SampleDesc.Count = 1;
			texdesc.SampleDesc.Quality = 0;
			texdesc.Usage = D3D10_USAGE_DEFAULT;
			texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
			texdesc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

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

				_runtime->update_texture_datatype(obj, texture::datatype::backbuffer, _runtime->_backbuffer_texture_srv[0], _runtime->_backbuffer_texture_srv[1]);
			}
			else if (node->semantic == "DEPTH" || node->semantic == "SV_DEPTH")
			{
				if (texdesc.Width != 1 || texdesc.Height != 1 || texdesc.MipLevels != 1 || texdesc.Format != DXGI_FORMAT_R8G8B8A8_TYPELESS)
				{
					warning(node->location, "texture properties on depthbuffer textures are ignored");
				}

				_runtime->update_texture_datatype(obj, texture::datatype::depthbuffer, _runtime->_depthstencil_texture_srv, nullptr);
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
					error(node->location, "'ID3D10Device::CreateTexture2D' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
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
					error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}

				srvdesc.Format = MakeSRGBFormat(texdesc.Format);

				if (srvdesc.Format != texdesc.Format)
				{
					hr = _runtime->_device->CreateShaderResourceView(obj->TextureInterface, &srvdesc, &obj->ShaderResourceView[1]);

					if (FAILED(hr))
					{
						error(node->location, "'ID3D10Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
						return;
					}
				}
			}

			_global_code += "Texture2D ";
			_global_code += node->unique_name;
			_global_code += " : register(t" + std::to_string(_runtime->_effect_shader_resources.size()) + "), __";
			_global_code += node->unique_name;
			_global_code += "SRGB : register(t" + std::to_string(_runtime->_effect_shader_resources.size() + 1) + ");\n";

			_runtime->_effect_shader_resources.push_back(obj->ShaderResourceView[0]);
			_runtime->_effect_shader_resources.push_back(obj->ShaderResourceView[1]);

			_runtime->add_texture(obj);
		}
		void visit_sampler(const fx::nodes::variable_declaration_node *node)
		{
			if (node->properties.Texture == nullptr)
			{
				error(node->location, "sampler '" + node->name + "' is missing required 'Texture' property");
				return;
			}

			D3D10_SAMPLER_DESC desc;
			desc.AddressU = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressU);
			desc.AddressV = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressV);
			desc.AddressW = static_cast<D3D10_TEXTURE_ADDRESS_MODE>(node->properties.AddressW);
			desc.MipLODBias = node->properties.MipLODBias;
			desc.MinLOD = node->properties.MinLOD;
			desc.MaxLOD = node->properties.MaxLOD;
			desc.MaxAnisotropy = node->properties.MaxAnisotropy;
			desc.ComparisonFunc = D3D10_COMPARISON_NEVER;

			const UINT minfilter = node->properties.MinFilter;
			const UINT magfilter = node->properties.MagFilter;
			const UINT mipfilter = node->properties.MipFilter;

			if (minfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || magfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC || mipfilter == fx::nodes::variable_declaration_node::properties::ANISOTROPIC)
			{
				desc.Filter = D3D10_FILTER_ANISOTROPIC;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
			{
				desc.Filter = D3D10_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
			{
				desc.Filter = D3D10_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::POINT && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
			{
				desc.Filter = D3D10_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
			{
				desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::POINT && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
			{
				desc.Filter = D3D10_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::POINT)
			{
				desc.Filter = D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			else if (minfilter == fx::nodes::variable_declaration_node::properties::LINEAR && magfilter == fx::nodes::variable_declaration_node::properties::LINEAR && mipfilter == fx::nodes::variable_declaration_node::properties::LINEAR)
			{
				desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
			}
			else
			{
				desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
			}

			d3d10_texture *texture = static_cast<d3d10_texture *>(_runtime->find_texture(node->properties.Texture->name));

			if (texture == nullptr)
			{
				error(node->location, "texture '" + node->properties.Texture->name + "' for sampler '" + node->name + "' is missing due to previous error");
				return;
			}

			const size_t descHash = D3D10_SAMPLER_DESC_HASH(desc);
			auto it = _sampler_descs.find(descHash);

			if (it == _sampler_descs.end())
			{
				ID3D10SamplerState *sampler = nullptr;

				HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

				if (FAILED(hr))
				{
					error(node->location, "'ID3D10Device::CreateSamplerState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}

				_runtime->_effect_sampler_states.push_back(sampler);
				it = _sampler_descs.emplace(descHash, _runtime->_effect_sampler_states.size() - 1).first;

				_global_code += "SamplerState __SamplerState" + std::to_string(it->second) + " : register(s" + std::to_string(it->second) + ");\n";
			}

			_global_code += "static const __sampler2D ";
			_global_code += node->unique_name;
			_global_code += " = { ";

			if (node->properties.SRGBTexture && texture->ShaderResourceView[1] != nullptr)
			{
				_global_code += "__";
				_global_code += node->properties.Texture->unique_name;
				_global_code += "SRGB";
			}
			else
			{
				_global_code += node->properties.Texture->unique_name;
			}

			_global_code += ", __SamplerState" + std::to_string(it->second) + " };\n";
		}
		void visit_uniform(const fx::nodes::variable_declaration_node *node)
		{
			visit(_global_uniforms, node->type);

			_global_uniforms += ' ';
			_global_uniforms += node->unique_name;

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

			auto &uniform_storage = _runtime->get_uniform_value_storage();

			if (_current_global_size >= uniform_storage.size())
			{
				uniform_storage.resize(uniform_storage.size() + 128);
			}

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
			d3d10_technique *obj = new d3d10_technique();
			obj->name = node->name;
			obj->pass_count = static_cast<unsigned int>(node->pass_list.size());

			visit_annotation(node->annotation_list, *obj);

			for (auto pass : node->pass_list)
			{
				visit_pass(pass, obj->passes);
			}

			_runtime->add_technique(obj);
		}
		void visit_pass(const fx::nodes::pass_declaration_node *node, std::vector<d3d10_pass> &passes)
		{
			d3d10_pass pass;
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

				d3d10_texture *texture = static_cast<d3d10_texture *>(_runtime->find_texture(node->states.RenderTargets[i]->name));

				if (texture == nullptr)
				{
					error(node->location, "texture not found");
					return;
				}

				D3D10_TEXTURE2D_DESC desc;
				texture->TextureInterface->GetDesc(&desc);

				if (pass.Viewport.Width != 0 && pass.Viewport.Height != 0 && (desc.Width != static_cast<unsigned int>(pass.Viewport.Width) || desc.Height != static_cast<unsigned int>(pass.Viewport.Height)))
				{
					error(node->location, "cannot use multiple rendertargets with different sized textures");
					return;
				}
				else
				{
					pass.Viewport.Width = desc.Width;
					pass.Viewport.Height = desc.Height;
				}

				D3D10_RENDER_TARGET_VIEW_DESC rtvdesc;
				ZeroMemory(&rtvdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
				rtvdesc.Format = node->states.SRGBWriteEnable ? MakeSRGBFormat(desc.Format) : MakeNonSRGBFormat(desc.Format);
				rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS : D3D10_RTV_DIMENSION_TEXTURE2D;

				if (texture->RenderTargetView[targetIndex] == nullptr)
				{
					HRESULT hr = _runtime->_device->CreateRenderTargetView(texture->TextureInterface, &rtvdesc, &texture->RenderTargetView[targetIndex]);

					if (FAILED(hr))
					{
						warning(node->location, "'CreateRenderTargetView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					}
				}

				pass.RT[i] = texture->RenderTargetView[targetIndex];
				pass.RTSRV[i] = texture->ShaderResourceView[targetIndex];
			}

			if (pass.Viewport.Width == 0 && pass.Viewport.Height == 0)
			{
				pass.Viewport.Width = _runtime->frame_width();
				pass.Viewport.Height = _runtime->frame_height();
			}

			D3D10_DEPTH_STENCIL_DESC ddesc;
			ddesc.DepthEnable = node->states.DepthEnable;
			ddesc.DepthWriteMask = node->states.DepthWriteMask ? D3D10_DEPTH_WRITE_MASK_ALL : D3D10_DEPTH_WRITE_MASK_ZERO;
			ddesc.DepthFunc = static_cast<D3D10_COMPARISON_FUNC>(node->states.DepthFunc);
			ddesc.StencilEnable = node->states.StencilEnable;
			ddesc.StencilReadMask = node->states.StencilReadMask;
			ddesc.StencilWriteMask = node->states.StencilWriteMask;
			ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D10_COMPARISON_FUNC>(node->states.StencilFunc);
			ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = LiteralToStencilOp(node->states.StencilOpPass);
			ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = LiteralToStencilOp(node->states.StencilOpFail);
			ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = LiteralToStencilOp(node->states.StencilOpDepthFail);
			pass.StencilRef = node->states.StencilRef;

			HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.DSS);

			if (FAILED(hr))
			{
				warning(node->location, "'ID3D10Device::CreateDepthStencilState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			}

			D3D10_BLEND_DESC bdesc;
			bdesc.AlphaToCoverageEnable = FALSE;
			bdesc.RenderTargetWriteMask[0] = node->states.RenderTargetWriteMask;
			bdesc.BlendEnable[0] = node->states.BlendEnable;
			bdesc.BlendOp = static_cast<D3D10_BLEND_OP>(node->states.BlendOp);
			bdesc.BlendOpAlpha = static_cast<D3D10_BLEND_OP>(node->states.BlendOpAlpha);
			bdesc.SrcBlend = LiteralToBlend(node->states.SrcBlend);
			bdesc.DestBlend = LiteralToBlend(node->states.DestBlend);

			for (UINT i = 1; i < 8; ++i)
			{
				bdesc.RenderTargetWriteMask[i] = bdesc.RenderTargetWriteMask[0];
				bdesc.BlendEnable[i] = bdesc.BlendEnable[0];
			}

			hr = _runtime->_device->CreateBlendState(&bdesc, &pass.BS);

			if (FAILED(hr))
			{
				warning(node->location, "'ID3D10Device::CreateBlendState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
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
		void visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, d3d10_pass &pass)
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

			if (!_global_uniforms.empty())
			{
				source += "cbuffer __GLOBAL__ : register(b0)\n{\n" + _global_uniforms + "};\n";
			}

			source += _global_code;

			LOG(TRACE) << "> Compiling shader '" << node->name << "':\n\n" << source.c_str() << "\n";

			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
			ID3DBlob *compiled = nullptr, *errors = nullptr;

			if (_skip_shader_optimization)
			{
				flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
			}

			HRESULT hr = D3DCompile(source.c_str(), source.length(), nullptr, nullptr, nullptr, node->unique_name.c_str(), profile.c_str(), flags, 0, &compiled, &errors);

			if (errors != nullptr)
			{
				_errors += std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());

				errors->Release();
			}

			if (FAILED(hr))
			{
				error(node->location, "internal shader compilation failed");
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
				error(node->location, "'CreateShader' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}
		}

	private:
		d3d10_runtime *_runtime;
		std::string _global_code, _global_uniforms;
		bool _skip_shader_optimization;
		std::unordered_map<size_t, size_t> _sampler_descs;
		UINT _current_global_size;
		bool _is_in_parameter_block, _is_in_function_block;
	};

	static UINT get_renderer_id(ID3D10Device *device)
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

	d3d10_runtime::d3d10_runtime(ID3D10Device *device, IDXGISwapChain *swapchain) : runtime(get_renderer_id(device)), _device(device), _swapchain(swapchain), _backbuffer_format(DXGI_FORMAT_UNKNOWN), _is_multisampling_enabled(false), _stateblock(device), _backbuffer(nullptr), _backbuffer_resolved(nullptr), _backbuffer_texture(nullptr), _backbuffer_texture_srv(), _backbuffer_rtv(), _depthstencil(nullptr), _depthstencil_replacement(nullptr), _depthstencil_texture(nullptr), _depthstencil_texture_srv(nullptr), _copy_vertex_shader(nullptr), _copy_pixel_shader(nullptr), _copy_sampler(nullptr), _effect_rasterizer_state(nullptr), _constant_buffer(nullptr), _constant_buffer_size(0)
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
			
		_vendor_id = desc.VendorId;
		_device_id = desc.DeviceId;
	}
	d3d10_runtime::~d3d10_runtime()
	{
		_device->Release();
		_swapchain->Release();
	}

	bool d3d10_runtime::on_init(const DXGI_SWAP_CHAIN_DESC &desc)
	{
		_width = desc.BufferDesc.Width;
		_height = desc.BufferDesc.Height;
		_backbuffer_format = desc.BufferDesc.Format;
		_is_multisampling_enabled = desc.SampleDesc.Count > 1;
		input::register_window(desc.OutputWindow, _input);

		#pragma region Get backbuffer rendertarget
		HRESULT hr = _swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void **>(&_backbuffer));

		assert(SUCCEEDED(hr));

		D3D10_TEXTURE2D_DESC texdesc;
		texdesc.Width = _width;
		texdesc.Height = _height;
		texdesc.ArraySize = texdesc.MipLevels = 1;
		texdesc.Format = d3d10_fx_compiler::MakeTypelessFormat(_backbuffer_format);
		texdesc.SampleDesc.Count = 1;
		texdesc.SampleDesc.Quality = 0;
		texdesc.Usage = D3D10_USAGE_DEFAULT;
		texdesc.BindFlags = D3D10_BIND_RENDER_TARGET;
		texdesc.MiscFlags = texdesc.CPUAccessFlags = 0;

		OSVERSIONINFOEX verinfoWin7 = { sizeof(OSVERSIONINFOEX), 6, 1 };
		const bool isWindows7 = VerifyVersionInfo(&verinfoWin7, VER_MAJORVERSION | VER_MINORVERSION, VerSetConditionMask(VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)) != FALSE;

		if (_is_multisampling_enabled || d3d10_fx_compiler::MakeNonSRGBFormat(_backbuffer_format) != _backbuffer_format || !isWindows7)
		{
			hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbuffer_resolved);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create backbuffer replacement (Width = " << texdesc.Width << ", Height = " << texdesc.Height << ", Format = " << texdesc.Format << ", SampleCount = " << texdesc.SampleDesc.Count << ", SampleQuality = " << texdesc.SampleDesc.Quality << ")! HRESULT is '" << hr << "'.";

				safe_release(_backbuffer);

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
		texdesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

		hr = _device->CreateTexture2D(&texdesc, nullptr, &_backbuffer_texture);

		if (SUCCEEDED(hr))
		{
			D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc;
			ZeroMemory(&srvdesc, sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC));
			srvdesc.Format = d3d10_fx_compiler::MakeNonSRGBFormat(texdesc.Format);
			srvdesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = texdesc.MipLevels;

			if (SUCCEEDED(hr))
			{
				hr = _device->CreateShaderResourceView(_backbuffer_texture, &srvdesc, &_backbuffer_texture_srv[0]);
			}
			else
			{
				LOG(TRACE) << "Failed to create backbuffer texture resource view (Format = " << srvdesc.Format << ")! HRESULT is '" << hr << "'.";
			}

			srvdesc.Format = d3d10_fx_compiler::MakeSRGBFormat(texdesc.Format);

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
			safe_release(_backbuffer);
			safe_release(_backbuffer_resolved);
			safe_release(_backbuffer_texture);
			safe_release(_backbuffer_texture_srv[0]);
			safe_release(_backbuffer_texture_srv[1]);

			return false;
		}
		#pragma endregion

		D3D10_RENDER_TARGET_VIEW_DESC rtdesc;
		ZeroMemory(&rtdesc, sizeof(D3D10_RENDER_TARGET_VIEW_DESC));
		rtdesc.Format = d3d10_fx_compiler::MakeNonSRGBFormat(texdesc.Format);
		rtdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;

		hr = _device->CreateRenderTargetView(_backbuffer_resolved, &rtdesc, &_backbuffer_rtv[0]);

		if (FAILED(hr))
		{
			safe_release(_backbuffer);
			safe_release(_backbuffer_resolved);
			safe_release(_backbuffer_texture);
			safe_release(_backbuffer_texture_srv[0]);
			safe_release(_backbuffer_texture_srv[1]);

			LOG(TRACE) << "Failed to create backbuffer rendertarget (Format = " << rtdesc.Format << ")! HRESULT is '" << hr << "'.";

			return false;
		}

		rtdesc.Format = d3d10_fx_compiler::MakeSRGBFormat(texdesc.Format);

		hr = _device->CreateRenderTargetView(_backbuffer_resolved, &rtdesc, &_backbuffer_rtv[1]);

		if (FAILED(hr))
		{
			safe_release(_backbuffer);
			safe_release(_backbuffer_resolved);
			safe_release(_backbuffer_texture);
			safe_release(_backbuffer_texture_srv[0]);
			safe_release(_backbuffer_texture_srv[1]);
			safe_release(_backbuffer_rtv[0]);

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
			hr = _device->CreateDepthStencilView(dstexture, nullptr, &_default_depthstencil);

			safe_release(dstexture);
		}
		if (FAILED(hr))
		{
			LOG(TRACE) << "Failed to create default depthstencil! HRESULT is '" << hr << "'.";

			return false;
		}
		#pragma endregion

		const BYTE vs[] = { 68, 88, 66, 67, 224, 206, 72, 137, 142, 185, 68, 219, 247, 216, 225, 132, 111, 78, 106, 20, 1, 0, 0, 0, 156, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 140, 0, 0, 0, 192, 0, 0, 0, 24, 1, 0, 0, 32, 2, 0, 0, 82, 68, 69, 70, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 0, 0, 0, 0, 4, 254, 255, 0, 1, 0, 0, 28, 0, 0, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 171, 171, 73, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 83, 86, 95, 86, 101, 114, 116, 101, 120, 73, 68, 0, 79, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 12, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 83, 72, 68, 82, 0, 1, 0, 0, 64, 0, 1, 0, 64, 0, 0, 0, 96, 0, 0, 4, 18, 16, 16, 0, 0, 0, 0, 0, 6, 0, 0, 0, 103, 0, 0, 4, 242, 32, 16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 101, 0, 0, 3, 50, 32, 16, 0, 1, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 32, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 6, 16, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 10, 50, 0, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 50, 0, 0, 15, 50, 32, 16, 0, 0, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 64, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 128, 191, 0, 0, 128, 63, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 5, 50, 32, 16, 0, 1, 0, 0, 0, 70, 0, 16, 0, 0, 0, 0, 0, 54, 0, 0, 8, 194, 32, 16, 0, 0, 0, 0, 0, 2, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 6, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		hr = _device->CreateVertexShader(vs, ARRAYSIZE(vs), &_copy_vertex_shader);

		assert(SUCCEEDED(hr));

		const BYTE ps[] = { 68, 88, 66, 67, 93, 102, 148, 45, 34, 106, 51, 79, 54, 23, 136, 21, 27, 217, 232, 71, 1, 0, 0, 0, 116, 2, 0, 0, 5, 0, 0, 0, 52, 0, 0, 0, 208, 0, 0, 0, 40, 1, 0, 0, 92, 1, 0, 0, 248, 1, 0, 0, 82, 68, 69, 70, 148, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 28, 0, 0, 0, 0, 4, 255, 255, 0, 1, 0, 0, 98, 0, 0, 0, 92, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 95, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 1, 0, 0, 0, 13, 0, 0, 0, 115, 48, 0, 116, 48, 0, 77, 105, 99, 114, 111, 115, 111, 102, 116, 32, 40, 82, 41, 32, 72, 76, 83, 76, 32, 83, 104, 97, 100, 101, 114, 32, 67, 111, 109, 112, 105, 108, 101, 114, 32, 54, 46, 51, 46, 57, 54, 48, 48, 46, 49, 54, 51, 56, 52, 0, 73, 83, 71, 78, 80, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0, 0, 56, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 3, 3, 0, 0, 83, 86, 95, 80, 111, 115, 105, 116, 105, 111, 110, 0, 84, 69, 88, 67, 79, 79, 82, 68, 0, 171, 171, 171, 79, 83, 71, 78, 44, 0, 0, 0, 1, 0, 0, 0, 8, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 83, 86, 95, 84, 97, 114, 103, 101, 116, 0, 171, 171, 83, 72, 68, 82, 148, 0, 0, 0, 64, 0, 0, 0, 37, 0, 0, 0, 90, 0, 0, 3, 0, 96, 16, 0, 0, 0, 0, 0, 88, 24, 0, 4, 0, 112, 16, 0, 0, 0, 0, 0, 85, 85, 0, 0, 98, 16, 0, 3, 50, 16, 16, 0, 1, 0, 0, 0, 101, 0, 0, 3, 242, 32, 16, 0, 0, 0, 0, 0, 104, 0, 0, 2, 1, 0, 0, 0, 69, 0, 0, 9, 242, 0, 16, 0, 0, 0, 0, 0, 70, 16, 16, 0, 1, 0, 0, 0, 70, 126, 16, 0, 0, 0, 0, 0, 0, 96, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 114, 32, 16, 0, 0, 0, 0, 0, 70, 2, 16, 0, 0, 0, 0, 0, 54, 0, 0, 5, 130, 32, 16, 0, 0, 0, 0, 0, 1, 64, 0, 0, 0, 0, 128, 63, 62, 0, 0, 1, 83, 84, 65, 84, 116, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		hr = _device->CreatePixelShader(ps, ARRAYSIZE(ps), &_copy_pixel_shader);

		assert(SUCCEEDED(hr));

		const D3D10_SAMPLER_DESC copysampdesc = { D3D10_FILTER_MIN_MAG_MIP_POINT, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP, D3D10_TEXTURE_ADDRESS_CLAMP };
		hr = _device->CreateSamplerState(&copysampdesc, &_copy_sampler);

		assert(SUCCEEDED(hr));

		#pragma region Create effect objects
		D3D10_RASTERIZER_DESC rsdesc;
		ZeroMemory(&rsdesc, sizeof(D3D10_RASTERIZER_DESC));
		rsdesc.FillMode = D3D10_FILL_SOLID;
		rsdesc.CullMode = D3D10_CULL_NONE;
		rsdesc.DepthClipEnable = TRUE;

		hr = _device->CreateRasterizerState(&rsdesc, &_effect_rasterizer_state);

		assert(SUCCEEDED(hr));
		#pragma endregion

		_gui.reset(new gui(this, nvgCreateD3D10(_device, 0)));

		return runtime::on_init();
	}
	void d3d10_runtime::on_reset()
	{
		if (!_is_initialized)
		{
			return;
		}

		runtime::on_reset();

		// Destroy NanoVG
		NVGcontext *const nvg = _gui->context();

		_gui.reset();

		nvgDeleteD3D10(nvg);

		// Destroy resources
		safe_release(_backbuffer);
		safe_release(_backbuffer_resolved);
		safe_release(_backbuffer_texture);
		safe_release(_backbuffer_texture_srv[0]);
		safe_release(_backbuffer_texture_srv[1]);
		safe_release(_backbuffer_rtv[0]);
		safe_release(_backbuffer_rtv[1]);
		safe_release(_backbuffer_rtv[2]);

		safe_release(_depthstencil);
		safe_release(_depthstencil_replacement);
		safe_release(_depthstencil_texture);
		safe_release(_depthstencil_texture_srv);

		safe_release(_default_depthstencil);
		safe_release(_copy_vertex_shader);
		safe_release(_copy_pixel_shader);
		safe_release(_copy_sampler);

		safe_release(_effect_rasterizer_state);
	}
	void d3d10_runtime::on_reset_effect()
	{
		runtime::on_reset_effect();

		for (auto it : _effect_sampler_states)
		{
			it->Release();
		}

		_effect_sampler_states.clear();
		_effect_shader_resources.clear();

		safe_release(_constant_buffer);

		_constant_buffer_size = 0;
	}
	void d3d10_runtime::on_present()
	{
		if (!_is_initialized)
		{
			LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
			return;
		}
		else if (_drawcalls == 0)
		{
			return;
		}

		detect_depth_source();

		// Capture device state
		_stateblock.capture();

		ID3D10RenderTargetView *stateblock_rendertargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
		ID3D10DepthStencilView *stateblock_depthstencil = nullptr;

		_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblock_rendertargets, &stateblock_depthstencil);

		// Resolve backbuffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->ResolveSubresource(_backbuffer_resolved, 0, _backbuffer, 0, _backbuffer_format);
		}

		// Apply post processing
		on_apply_effect();

		// Reset rendertarget
		_device->OMSetRenderTargets(1, &_backbuffer_rtv[0], _default_depthstencil);

		const D3D10_VIEWPORT viewport = { 0, 0, _width, _height, 0.0f, 1.0f };
		_device->RSSetViewports(1, &viewport);

		// Apply presenting
		runtime::on_present();

		// Copy to backbuffer
		if (_backbuffer_resolved != _backbuffer)
		{
			_device->OMSetRenderTargets(1, &_backbuffer_rtv[2], nullptr);
			_device->CopyResource(_backbuffer_texture, _backbuffer_resolved);

			_device->VSSetShader(_copy_vertex_shader);
			_device->PSSetShader(_copy_pixel_shader);
			_device->PSSetSamplers(0, 1, &_copy_sampler);
			_device->PSSetShaderResources(0, 1, &_backbuffer_texture_srv[d3d10_fx_compiler::MakeSRGBFormat(_backbuffer_format) == _backbuffer_format]);
			_device->Draw(3, 0);
		}

		// Apply previous device state
		_stateblock.apply_and_release();

		_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, stateblock_rendertargets, stateblock_depthstencil);

		for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			safe_release(stateblock_rendertargets[i]);
		}

		safe_release(stateblock_depthstencil);
	}
	void d3d10_runtime::on_draw_call(UINT vertices)
	{
		runtime::on_draw_call(vertices);

		ID3D10DepthStencilView *depthstencil = nullptr;
		_device->OMGetRenderTargets(0, nullptr, &depthstencil);

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
				it->second.drawcall_count = static_cast<FLOAT>(_drawcalls);
				it->second.vertices_count += vertices;
			}
		}
	}
	void d3d10_runtime::on_apply_effect()
	{
		if (!_is_effect_compiled)
		{
			return;
		}

		// Setup real backbuffer
		_device->OMSetRenderTargets(1, &_backbuffer_rtv[0], nullptr);

		// Setup vertex input
		const uintptr_t null = 0;
		_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_device->IASetInputLayout(nullptr);
		_device->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D10Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));

		_device->RSSetState(_effect_rasterizer_state);

		// Disable unused pipeline stages
		_device->GSSetShader(nullptr);

		// Setup samplers
		_device->VSSetSamplers(0, static_cast<UINT>(_effect_sampler_states.size()), _effect_sampler_states.data());
		_device->PSSetSamplers(0, static_cast<UINT>(_effect_sampler_states.size()), _effect_sampler_states.data());

		// Setup shader constants
		_device->VSSetConstantBuffers(0, 1, &_constant_buffer);
		_device->PSSetConstantBuffers(0, 1, &_constant_buffer);

		// Apply post processing
		runtime::on_apply_effect();
	}
	void d3d10_runtime::on_apply_effect_technique(const technique *technique)
	{
		runtime::on_apply_effect_technique(technique);

		bool defaultDepthStencilCleared = false;

		// Update shader constants
		if (_constant_buffer != nullptr)
		{
			void *data = nullptr;

			const HRESULT hr = _constant_buffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &data);

			if (SUCCEEDED(hr))
			{
				CopyMemory(data, get_uniform_value_storage().data(), _constant_buffer_size);

				_constant_buffer->Unmap();
			}
			else
			{
				LOG(TRACE) << "Failed to map constant buffer! HRESULT is '" << hr << "'!";
			}
		}

		for (const auto &pass : static_cast<const d3d10_technique *>(technique)->passes)
		{
			// Setup states
			_device->VSSetShader(pass.VS);
			_device->PSSetShader(pass.PS);

			const FLOAT blendfactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			_device->OMSetBlendState(pass.BS, blendfactor, D3D10_DEFAULT_SAMPLE_MASK);
			_device->OMSetDepthStencilState(pass.DSS, pass.StencilRef);

			// Save backbuffer of previous pass
			_device->CopyResource(_backbuffer_texture, _backbuffer_resolved);

			// Setup shader resources
			_device->VSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());
			_device->PSSetShaderResources(0, static_cast<UINT>(pass.SRV.size()), pass.SRV.data());

			// Setup rendertargets
			if (pass.Viewport.Width == _width && pass.Viewport.Height == _height)
			{
				_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.RT, _default_depthstencil);

				if (!defaultDepthStencilCleared)
				{
					defaultDepthStencilCleared = true;

					// Clear depthstencil
					_device->ClearDepthStencilView(_default_depthstencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0f, 0);
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

			runtime::on_draw_call(3);

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

	void d3d10_runtime::on_set_depthstencil_view(ID3D10DepthStencilView *&depthstencil)
	{
		if (_depth_source_table.find(depthstencil) == _depth_source_table.end())
		{
			ID3D10Resource *resource;
			ID3D10Texture2D *texture;
			D3D10_TEXTURE2D_DESC texture_desc;

			depthstencil->GetResource(&resource);

			if (FAILED(resource->QueryInterface(&texture)))
			{
				resource->Release();
				return;
			}

			texture->GetDesc(&texture_desc);
			texture->Release();
			resource->Release();

			// Early depth-stencil rejection
			if (texture_desc.Width != _width || texture_desc.Height != _height || texture_desc.SampleDesc.Count > 1)
			{
				return;
			}

			LOG(TRACE) << "Adding depth-stencil " << depthstencil << " (Width: " << texture_desc.Width << ", Height: " << texture_desc.Height << ", Format: " << texture_desc.Format << ") to list of possible depth candidates ...";

			depthstencil->AddRef();

			// Begin tracking new depth-stencil
			const depth_source_info info = { texture_desc.Width, texture_desc.Height };
			_depth_source_table.emplace(depthstencil, info);
		}

		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
		{
			depthstencil = _depthstencil_replacement;
		}
	}
	void d3d10_runtime::on_get_depthstencil_view(ID3D10DepthStencilView *&depthstencil)
	{
		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil_replacement)
		{
			depthstencil->Release();

			depthstencil = _depthstencil;
			depthstencil->AddRef();
		}
	}
	void d3d10_runtime::on_clear_depthstencil_view(ID3D10DepthStencilView *&depthstencil)
	{
		if (_depthstencil_replacement != nullptr && depthstencil == _depthstencil)
		{
			depthstencil = _depthstencil_replacement;
		}
	}
	void d3d10_runtime::on_copy_resource(ID3D10Resource *&dest, ID3D10Resource *&source)
	{
		if (_depthstencil_replacement != nullptr)
		{
			ID3D10Resource *resource = nullptr;
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

	void d3d10_runtime::screenshot(unsigned char *buffer) const
	{
		if (_backbuffer_format != DXGI_FORMAT_R8G8B8A8_UNORM && _backbuffer_format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB && _backbuffer_format != DXGI_FORMAT_B8G8R8A8_UNORM && _backbuffer_format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
		{
			LOG(WARNING) << "Screenshots are not supported for backbuffer format " << _backbuffer_format << ".";
			return;
		}

		D3D10_TEXTURE2D_DESC texdesc;
		ZeroMemory(&texdesc, sizeof(D3D10_TEXTURE2D_DESC));
		texdesc.Width = _width;
		texdesc.Height = _height;
		texdesc.Format = _backbuffer_format;
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

		_device->CopyResource(textureStaging, _backbuffer_resolved);
				
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

				if (_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM || _backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
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
	bool d3d10_runtime::update_effect(const fx::nodetree &ast, const std::vector<std::string> &pragmas, std::string &errors)
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

		return d3d10_fx_compiler(this, ast, errors, skip_optimization).run();
	}
	bool d3d10_runtime::update_texture(texture *texture, const unsigned char *data, size_t size)
	{
		d3d10_texture *texture_impl = dynamic_cast<d3d10_texture *>(texture);

		assert(texture_impl != nullptr);
		assert(data != nullptr && size != 0);

		if (texture->basetype != texture::datatype::image)
		{
			return false;
		}

		assert(texture->height != 0);

		_device->UpdateSubresource(texture_impl->TextureInterface, 0, nullptr, data, static_cast<UINT>(size / texture->height), static_cast<UINT>(size));

		if (texture->levels > 1)
		{
			_device->GenerateMips(texture_impl->ShaderResourceView[0]);
		}

		return true;
	}
	void d3d10_runtime::update_texture_datatype(texture *texture, texture::datatype source, ID3D10ShaderResourceView *srv, ID3D10ShaderResourceView *srvSRGB)
	{
		d3d10_texture *texture_impl = dynamic_cast<d3d10_texture *>(texture);

		texture->basetype = source;

		if (srvSRGB == nullptr)
		{
			srvSRGB = srv;
		}

		if (srv == texture_impl->ShaderResourceView[0] && srvSRGB == texture_impl->ShaderResourceView[1])
		{
			return;
		}

		safe_release(texture_impl->RenderTargetView[0]);
		safe_release(texture_impl->RenderTargetView[1]);
		safe_release(texture_impl->ShaderResourceView[0]);
		safe_release(texture_impl->ShaderResourceView[1]);

		safe_release(texture_impl->TextureInterface);

		if (srv != nullptr)
		{
			texture_impl->ShaderResourceView[0] = srv;
			texture_impl->ShaderResourceView[0]->AddRef();
			texture_impl->ShaderResourceView[0]->GetResource(reinterpret_cast<ID3D10Resource **>(&texture_impl->TextureInterface));
			texture_impl->ShaderResourceView[1] = srvSRGB;
			texture_impl->ShaderResourceView[1]->AddRef();

			D3D10_TEXTURE2D_DESC texdesc;
			texture_impl->TextureInterface->GetDesc(&texdesc);

			texture->width = texdesc.Width;
			texture->height = texdesc.Height;
			texture->format = texture::pixelformat::unknown;
			texture->levels = texdesc.MipLevels;
		}
		else
		{
			texture->width = texture->height = texture->levels = 0;
			texture->format = texture::pixelformat::unknown;
		}

		// Update techniques shader resourceviews
		for (auto &technique : _techniques)
		{
			for (auto &pass : static_cast<d3d10_technique *>(technique.get())->passes)
			{
				pass.SRV[texture_impl->ShaderRegister] = texture_impl->ShaderResourceView[0];
				pass.SRV[texture_impl->ShaderRegister + 1] = texture_impl->ShaderResourceView[1];
			}
		}
	}

	void d3d10_runtime::detect_depth_source()
	{
		for (auto it = _depth_source_table.begin(); it != _depth_source_table.end();)
		{
			const auto depthstencil = it->first;
			depthstencil->AddRef();
			const auto ref = depthstencil->Release();

			if (ref == 1)
			{
				LOG(TRACE) << "Removing depthstencil " << depthstencil << " from list of possible depth candidates ...";

				if (depthstencil == _depthstencil)
				{
					create_depthstencil_replacement(nullptr);
				}

				depthstencil->Release();

				it = _depth_source_table.erase(it);
			}
			else
			{
				++it;
			}
		}

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
		ID3D10DepthStencilView *best_match = nullptr;

		for (auto &it : _depth_source_table)
		{
			if (it.second.drawcall_count == 0)
			{
				continue;
			}
			else if ((it.second.vertices_count * (1.2f - it.second.drawcall_count / _drawcalls)) >= (best_info.vertices_count * (1.2f - best_info.drawcall_count / _drawcalls)))
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
	bool d3d10_runtime::create_depthstencil_replacement(ID3D10DepthStencilView *depthstencil)
	{
		safe_release(_depthstencil);
		safe_release(_depthstencil_replacement);
		safe_release(_depthstencil_texture);
		safe_release(_depthstencil_texture_srv);

		if (depthstencil != nullptr)
		{
			_depthstencil = depthstencil;
			_depthstencil->AddRef();
			_depthstencil->GetResource(reinterpret_cast<ID3D10Resource **>(&_depthstencil_texture));

			D3D10_TEXTURE2D_DESC texdesc;
			_depthstencil_texture->GetDesc(&texdesc);

			HRESULT hr = S_OK;

			if ((texdesc.BindFlags & D3D10_BIND_SHADER_RESOURCE) == 0)
			{
				safe_release(_depthstencil_texture);

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

				hr = _device->CreateTexture2D(&texdesc, nullptr, &_depthstencil_texture);

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

					hr = _device->CreateDepthStencilView(_depthstencil_texture, &dsvdesc, &_depthstencil_replacement);
				}
			}
			else
			{
				_depthstencil_replacement = _depthstencil;
				_depthstencil_replacement->AddRef();
			}

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create depthstencil replacement texture! HRESULT is '" << hr << "'.";

				safe_release(_depthstencil);
				safe_release(_depthstencil_texture);

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

			hr = _device->CreateShaderResourceView(_depthstencil_texture, &srvdesc, &_depthstencil_texture_srv);

			if (FAILED(hr))
			{
				LOG(TRACE) << "Failed to create depthstencil replacement resource view! HRESULT is '" << hr << "'.";

				safe_release(_depthstencil);
				safe_release(_depthstencil_replacement);
				safe_release(_depthstencil_texture);

				return false;
			}

			if (_depthstencil != _depthstencil_replacement)
			{
				// Update auto depthstencil
				depthstencil = nullptr;
				ID3D10RenderTargetView *targets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };

				_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depthstencil);

				if (depthstencil != nullptr)
				{
					if (depthstencil == _depthstencil)
					{
						_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, _depthstencil_replacement);
					}

					depthstencil->Release();
				}

				for (UINT i = 0; i < D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				{
					safe_release(targets[i]);
				}
			}
		}

		// Update effect textures
		for (const auto &it : _textures)
		{
			d3d10_texture *texture = static_cast<d3d10_texture *>(it.get());

			if (texture->basetype == texture::datatype::depthbuffer)
			{
				update_texture_datatype(texture, texture::datatype::depthbuffer, _depthstencil_texture_srv, nullptr);
			}
		}

		return true;
	}
}
