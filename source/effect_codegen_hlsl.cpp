/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <assert.h>

using namespace reshadefx;

class codegen_hlsl final : public codegen
{
public:
	codegen_hlsl(unsigned int shader_model, bool debug_info) : _shader_model(shader_model), _debug_info(debug_info)
	{
		struct_info cbuffer_type;
		cbuffer_type.name = "$Globals";
		cbuffer_type.unique_name = "_Globals";
		cbuffer_type.definition = _cbuffer_type_id = make_id();
		_structs.push_back(cbuffer_type);

		_names[_cbuffer_type_id] = cbuffer_type.unique_name;
	}

private:
	id _next_id = 1;
	id _last_block = 0;
	id _current_block = 0;
	id _cbuffer_type_id = 0;
	std::unordered_map<id, std::string> _names;
	std::unordered_map<id, std::string> _blocks;
	bool _debug_info = false;
	unsigned int _shader_model = 0;
	unsigned int _current_cbuffer_size = 0;
	unsigned int _current_sampler_binding = 0;
	std::unordered_map<id, std::vector<id>> _switch_fallthrough_blocks;
	std::vector<std::pair<std::string, bool>> _entry_points;

	inline std::string &code() { return _blocks[_current_block]; }

	void write_result(module &s) const override
	{
		if (_shader_model >= 40)
		{
			s.hlsl += "struct __sampler2D { Texture2D t; SamplerState s; };\n";

			if (_blocks.count(_cbuffer_type_id))
				s.hlsl += "cbuffer _Globals {\n" + _blocks.at(_cbuffer_type_id) + "};\n";
		}
		else
		{
			s.hlsl += "struct __sampler2D { sampler2D s; float2 pixelsize; };\nuniform float2 __TEXEL_SIZE__ : register(c255);\n";

			if (_blocks.count(_cbuffer_type_id))
				s.hlsl += _blocks.at(_cbuffer_type_id);
		}

		s.hlsl += _blocks.at(0);

		s.samplers = _samplers;
		s.textures = _textures;
		s.uniforms = _uniforms;
		s.techniques = _techniques;
		s.entry_points = _entry_points;
	}

	std::string write_type(const type &type, bool is_param = false, bool is_decl = true)
	{
		std::string s;

		if (is_decl)
		{
			if (type.has(type::q_precise))
				s += "precise ";
		}

		if (is_param)
		{
			if (type.has(type::q_linear))
				s += "linear ";
			if (type.has(type::q_noperspective))
				s += "noperspective ";
			if (type.has(type::q_centroid))
				s += "centroid ";
			if (type.has(type::q_nointerpolation))
				s += "nointerpolation ";

			if (type.has(type::q_inout))
				s += "inout ";
			else if (type.has(type::q_in))
				s += "in ";
			else if (type.has(type::q_out))
				s += "out ";
		}

		switch (type.base)
		{
		case type::t_void:
			s += "void";
			break;
		case type::t_bool:
			s += "bool";
			break;
		case type::t_int:
			s += "int";
			break;
		case type::t_uint:
			s += "uint";
			break;
		case type::t_float:
			s += "float";
			break;
		case type::t_struct:
			s += id_to_name(type.definition);
			break;
		case type::t_sampler:
			s += "__sampler2D";
			break;
		default:
			assert(false);
		}

		if (type.rows > 1)
			s += std::to_string(type.rows);
		if (type.cols > 1)
			s += 'x' + std::to_string(type.cols);

		return s;
	}
	std::string write_constant(const type &type, const constant &data)
	{
		assert(type.is_numeric() || (type.is_struct() && data.as_uint[0] == 0));

		std::string s;

		if (type.is_array())
		{
			struct type elem_type = type;
			elem_type.array_length = 0;

			s += "{ ";

			for (int i = 0; i < type.array_length; ++i)
			{
				s += write_constant(elem_type, i < static_cast<int>(data.array_data.size()) ? data.array_data[i] : constant());

				if (i < type.array_length - 1)
					s += ", ";
			}

			s += " }";

			return s;
		}
		if (type.is_struct())
		{
			s += '(' + id_to_name(type.definition) + ")0";

			return s;
		}

		if (!type.is_scalar())
			s += write_type(type, false, false);

		s += '(';

		for (unsigned int i = 0, components = type.components(); i < components; ++i)
		{
			switch (type.base)
			{
			case type::t_bool:
				s += data.as_uint[i] ? "true" : "false";
				break;
			case type::t_int:
				s += std::to_string(data.as_int[i]);
				break;
			case type::t_uint:
				s += std::to_string(data.as_uint[i]);
				break;
			case type::t_float:
				s += std::to_string(data.as_float[i]);
				break;
			}

			if (i < components - 1)
				s += ", ";
		}

		s += ')';

		return s;
	}
	std::string write_location(const location &loc)
	{
		if (loc.source.empty() || !_debug_info)
			return std::string();

		return "#line " + std::to_string(loc.line) + " \"" + loc.source + "\"\n";
	}

	std::string convert_semantic(const std::string &semantic) const
	{
		if (_shader_model < 40)
		{
			if (semantic == "SV_VERTEXID" || semantic == "VERTEXID")
				return "TEXCOORD0 /* VERTEXID */";
			else if (semantic == "SV_POSITION")
				return "POSITION";
			else if (semantic.compare(0, 9, "SV_TARGET") == 0)
				return "COLOR" + semantic.substr(9);
			else if (semantic == "SV_DEPTH")
				return "DEPTH";
		}
		else
		{
			if (semantic == "VERTEXID")
				return "SV_VERTEXID";
			else if (semantic == "POSITION" || semantic == "VPOS")
				return "SV_POSITION";
			else if (semantic.compare(0, 5, "COLOR") == 0)
				return "SV_TARGET" + semantic.substr(5);
			else if (semantic == "DEPTH")
				return "SV_DEPTH";
		}

		return semantic;
	}

	inline id make_id() { return _next_id++; }

	inline std::string id_to_name(id id) const
	{
		if (const auto it = _names.find(id); it != _names.end())
			return it->second;
		return '_' + std::to_string(id);
	}

	static void increase_indentation_level(std::string &block)
	{
		if (block.empty())
			return;

		for (size_t pos = 0; (pos = block.find("\n\t", pos)) != std::string::npos; pos += 3)
			block.replace(pos, 2, "\n\t\t");

		block.insert(block.begin(), '\t');
	}

	id   define_struct(const location &loc, struct_info &info) override
	{
		info.definition = make_id();
		_names[info.definition] = info.unique_name;

		_structs.push_back(info);

		code() += write_location(loc) + "struct " + id_to_name(info.definition) + "\n{\n";

		for (const auto &member : info.member_list)
		{
			code() += '\t' + write_type(member.type, true) + ' ' + member.name;
			if (!member.semantic.empty())
				code() += " : " + convert_semantic(member.semantic);
			code() += ";\n";
		}

		code() += "};\n";

		return info.definition;
	}
	id   define_texture(const location &, texture_info &info) override
	{
		info.id = make_id();

		_textures.push_back(info);

		return info.id;
	}
	id   define_sampler(const location &loc, sampler_info &info) override
	{
		info.id = make_id();
		info.binding = _current_sampler_binding++;

		_names[info.id] = info.unique_name;

		_samplers.push_back(info);

		if (_shader_model >= 40)
		{
			code() += "Texture2D " + info.unique_name + "_t : register(t" + std::to_string(info.binding) + ");\n";
			code() += "SamplerState " + info.unique_name + "_s : register(s" + std::to_string(info.binding) + ");\n";
			code() += write_location(loc) + "static const __sampler2D " + info.unique_name + " = { " + info.unique_name + "_t, " + info.unique_name + "_s };\n";
		}
		else
		{
			const auto tex = std::find_if(_textures.begin(), _textures.end(),
				[&info](const auto &it) { return it.unique_name == info.texture_name; });
			assert(tex != _textures.end());

			code() += "sampler2D " + info.unique_name + "_s : register(s" + std::to_string(info.binding) + ");\n";
			code() += write_location(loc) + "static const __sampler2D " + info.unique_name + " = { " + info.unique_name + "_s, float2(";

			if (tex->semantic.empty())
				code() += std::to_string(1.0f / tex->width) + ", " + std::to_string(1.0f / tex->height);
			else
				code() += "0, 0";

			code() += ") }; \n";
		}

		return info.id;
	}
	id   define_uniform(const location &loc, uniform_info &info) override
	{
		const unsigned int size = info.type.rows * info.type.cols * std::max(1, info.type.array_length) * 4;
		const unsigned int alignment = 16 - (_current_cbuffer_size % 16);
		_current_cbuffer_size += (size > alignment && (alignment != 16 || size <= 16)) ? size + alignment : size;
		info.size = size;
		info.offset = _current_cbuffer_size - size;

		struct_member_info member;
		member.type = info.type;
		member.name = info.name;

		const_cast<struct_info &>(find_struct(_cbuffer_type_id))
			.member_list.push_back(std::move(member));

		_blocks[_cbuffer_type_id] += write_location(loc);

		if (_shader_model < 40)
			_blocks[_cbuffer_type_id] += write_type(info.type) + " _Globals_" + info.name + " : register(c" + std::to_string(info.offset / 4) + ')';
		else
			_blocks[_cbuffer_type_id] += '\t' + write_type(info.type) + " _Globals_" + info.name;

		_blocks[_cbuffer_type_id] += ";\n";

		info.member_index = static_cast<uint32_t>(_uniforms.size());

		_uniforms.push_back(info);

		return _cbuffer_type_id;
	}
	id   define_variable(const location &loc, const type &type, const char *name, bool global, id initializer_value) override
	{
		const id res = make_id();

		if (name != nullptr)
			_names[res] = name;

		code() += write_location(loc);

		if (!global)
			code() += '\t';

		code() += write_type(type) + ' ' + id_to_name(res);

		if (type.is_array())
			code() += '[' + std::to_string(type.array_length) + ']';

		if (initializer_value != 0)
			code() += " = " + id_to_name(initializer_value);

		code() += ";\n";

		return res;
	}
	id   define_function(const location &loc, function_info &info) override
	{
		return define_function(loc, info, _shader_model >= 40);
	}
	id   define_function(const location &loc, function_info &info, bool is_entry_point)
	{
		std::string name = info.unique_name;
		if (!is_entry_point)
			name += '_';

		info.definition = make_id();
		_names[info.definition] = std::move(name);

		code() += write_location(loc) + write_type(info.return_type) + ' ' + id_to_name(info.definition) + '(';

		for (size_t i = 0, num_params = info.parameter_list.size(); i < num_params; ++i)
		{
			auto &param = info.parameter_list[i];

			param.definition = make_id();
			_names[param.definition] = param.name;

			code() += '\n' + write_location(param.location) + '\t' + write_type(param.type, true) + ' ' + param.name;

			if (param.type.is_array())
				code() += '[' + std::to_string(param.type.array_length) + ']';

			if (is_entry_point && !param.semantic.empty())
				code() += " : " + convert_semantic(param.semantic);

			if (i < num_params - 1)
				code() += ',';
		}

		code() += ')';

		if (is_entry_point && !info.return_semantic.empty())
			code() += " : " + convert_semantic(info.return_semantic);

		code() += '\n';

		_functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}

	id   create_block() override
	{
		return make_id();
	}

	void create_entry_point(const function_info &func, bool is_ps) override
	{
		if (const auto it = std::find_if(_entry_points.begin(), _entry_points.end(),
			[&func](const auto &ep) { return ep.first == func.unique_name; }); it != _entry_points.end())
			return;

		_entry_points.push_back({ func.unique_name, is_ps });

		// Only have to rewrite the entry point function signature in shader model 3
		if (_shader_model >= 40)
			return;

		auto entry_point = func;

		const auto is_color_semantic = [](const std::string &semantic) { return semantic.compare(0, 9, "SV_TARGET") == 0 || semantic.compare(0, 5, "COLOR") == 0; };
		const auto is_position_semantic = [](const std::string &semantic) { return semantic == "SV_POSITION" || semantic == "POSITION"; };

		const auto ret = make_id();
		_names[ret] = "ret";

		std::string position_variable_name;

		if (func.return_type.is_struct() && !is_ps)
			// If this function returns a struct which contains a position output, keep track of its member name
			for (const auto &member : find_struct(func.return_type.definition).member_list)
				if (is_position_semantic(member.semantic))
					position_variable_name = id_to_name(ret) + '.' + member.name;
		if (is_color_semantic(func.return_semantic))
			// The COLOR output semantic has to be a four-component vector in shader model 3, so enforce that
			entry_point.return_type.rows = 4;
		else if (is_position_semantic(func.return_semantic) && !is_ps)
			position_variable_name = id_to_name(ret);

		for (auto &param : entry_point.parameter_list)
			if (is_color_semantic(param.semantic))
				param.type.rows = 4;
			else if (is_position_semantic(param.semantic))
				if (is_ps) // Change the position input semantic in pixel shaders
					param.semantic = "VPOS";
				else // Keep track of the position output variable
					position_variable_name = param.name;

		define_function({}, entry_point, true);
		enter_block(create_block());

		// Clear all color output parameters so no component is left uninitialized
		for (auto &param : entry_point.parameter_list)
			if (is_color_semantic(param.semantic))
				code() += '\t' + param.name + " = float4(0.0, 0.0, 0.0, 0.0);\n";

		code() += '\t';
		if (is_color_semantic(func.return_semantic))
			code() += "const float4 ret = float4(";
		else if (!func.return_type.is_void())
			code() += write_type(func.return_type) + ' ' + id_to_name(ret) + " = ";

		// Call the function this entry point refers to
		code() += id_to_name(func.definition) + '(';

		for (size_t i = 0, num_params = func.parameter_list.size(); i < num_params; ++i)
		{
			code() += func.parameter_list[i].name;

			if (is_color_semantic(func.parameter_list[i].semantic))
			{
				code() += '.';
				for (unsigned int k = 0; k < func.parameter_list[i].type.rows; k++)
					code() += "xyzw"[k];
			}

			if (i < num_params - 1)
				code() += ", ";
		}

		code() += ')';

		// Cast the output value to a four-component vector
		if (is_color_semantic(func.return_semantic))
		{
			for (unsigned int i = 0; i < 4 - func.return_type.rows; i++)
				code() += ", 0.0";
			code() += ')';
		}

		code() += ";\n";

		// Shift everything by half a viewport pixel to workaround the different half-pixel offset in D3D9 (https://aras-p.info/blog/2016/04/08/solving-dx9-half-pixel-offset/)
		if (!position_variable_name.empty() && !is_ps) // Check if we are in a vertex shader definition
			code() += '\t' + position_variable_name + ".xy += __TEXEL_SIZE__ * " + position_variable_name + ".ww;\n";

		leave_block_and_return(func.return_type.is_void() ? 0 : ret);
		leave_function();
	}

	id   emit_load(const expression &chain) override
	{
		// Can refer to l-values without access chain directly
		if (chain.is_lvalue && chain.ops.empty())
			return chain.base;
		else if (chain.is_constant)
			return emit_constant(chain.type, chain.constant);

		const id res = make_id();

		code() += write_location(chain.location) + '\t' + write_type(chain.type) + ' ' + id_to_name(res);

		if (chain.type.is_array())
			code() += '[' + std::to_string(chain.type.array_length) + ']';

		code() += " = ";

		std::string newcode = id_to_name(chain.base);

		for (const auto &op : chain.ops)
		{
			switch (op.type)
			{
			case expression::operation::op_cast:
				newcode = "((" + write_type(op.to, false, false) + ')' + newcode + ')';
				break;
			case expression::operation::op_index:
				newcode += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_member:
				newcode += op.from.definition == _cbuffer_type_id ? '_' : '.';
				newcode += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_swizzle:
				newcode += '.';
				for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					newcode += "xyzw"[op.swizzle[i]];
				break;
			}
		}

		code() += newcode;

		code() += ";\n";

		return res;
	}
	void emit_store(const expression &chain, id value, const type &) override
	{
		code() += write_location(chain.location) + '\t' + id_to_name(chain.base);

		for (const auto &op : chain.ops)
		{
			switch (op.type)
			{
			case expression::operation::op_index:
				code() += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_member:
				code() += '.';
				code() += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_swizzle:
				code() += '.';
				for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					code() += "xyzw"[op.swizzle[i]];
				break;
			}
		}

		code() += " = " + id_to_name(value) + ";\n";
	}

	id   emit_constant(const type &type, const constant &data) override
	{
		const id res = make_id();

		code() += "\tconst " + write_type(type) + ' ' + id_to_name(res);

		if (type.is_array())
			code() += '[' + std::to_string(type.array_length) + ']';

		code() += " = " + write_constant(type, data) + ";\n";

		return res;
	}

	id   emit_unary_op(const location &loc, tokenid op, const type &res_type, id val) override
	{
		const id res = make_id();

		code() += write_location(loc) + '\t' + write_type(res_type) + ' ' + id_to_name(res) + " = ";

		if (_shader_model < 40 && op == tokenid::tilde)
			code() += "0xFFFFFFFF -"; // Emulate bitwise not operator on shader model 3
		else
			code() += char(op);

		code() += ' ' + id_to_name(val) + ";\n";

		return res;
	}
	id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &, id lhs, id rhs) override
	{
		const id res = make_id();

		code() += write_location(loc) + '\t' + write_type(res_type) + ' ' + id_to_name(res) + " = ";

		if (_shader_model < 40 && (op == tokenid::greater_greater || op == tokenid::greater_greater_equal))
			code() += "floor(";

		code() += '(' + id_to_name(lhs) + ' ';

		switch (op)
		{
		case tokenid::plus:
		case tokenid::plus_plus:
		case tokenid::plus_equal:
			code() += '+';
			break;
		case tokenid::minus:
		case tokenid::minus_minus:
		case tokenid::minus_equal:
			code() += '-';
			break;
		case tokenid::star:
		case tokenid::star_equal:
			code() += '*';
			break;
		case tokenid::slash:
		case tokenid::slash_equal:
			code() += '/';
			break;
		case tokenid::percent:
		case tokenid::percent_equal:
			code() += '%';
			break;
		case tokenid::caret:
		case tokenid::caret_equal:
			code() += '^';
			break;
		case tokenid::pipe:
		case tokenid::pipe_equal:
			code() += '|';
			break;
		case tokenid::ampersand:
		case tokenid::ampersand_equal:
			code() += '&';
			break;
		case tokenid::less_less:
		case tokenid::less_less_equal:
			code() += _shader_model >= 40 ? "<<" : ") * exp2("; // Emulate bitwise shift operators on shader model 3
			break;
		case tokenid::greater_greater:
		case tokenid::greater_greater_equal:
			code() += _shader_model >= 40 ? ">>" : ") / exp2(";
			break;
		case tokenid::pipe_pipe:
			code() += "||";
			break;
		case tokenid::ampersand_ampersand:
			code() += "&&";
			break;
		case tokenid::less:
			code() += '<';
			break;
		case tokenid::less_equal:
			code() += "<=";
			break;
		case tokenid::greater:
			code() += '>';
			break;
		case tokenid::greater_equal:
			code() += ">=";
			break;
		case tokenid::equal_equal:
			code() += "==";
			break;
		case tokenid::exclaim_equal:
			code() += "!=";
			break;
		default:
			assert(false);
		}

		code() += ' ' + id_to_name(rhs) + ')';

		if (_shader_model < 40 && (op == tokenid::greater_greater || op == tokenid::greater_greater_equal))
			code() += ')';

		code() += ";\n";

		return res;
	}
	id   emit_ternary_op(const location &loc, tokenid op, const type &res_type, id condition, id true_value, id false_value) override
	{
		assert(op == tokenid::question);

		const id res = make_id();

		code() += write_location(loc) + '\t' + write_type(res_type) + ' ' + id_to_name(res);

		if (res_type.is_array())
			code() += '[' + std::to_string(res_type.array_length) + ']';

		code() += " = " + id_to_name(condition) + " ? " + id_to_name(true_value) + " : " + id_to_name(false_value) + ";\n";

		return res;
	}
	id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert(arg.ops.empty() && arg.base != 0);

		const id res = make_id();

		code() += write_location(loc) + '\t';

		if (!res_type.is_void())
		{
			code() += write_type(res_type) + ' ' + id_to_name(res);

			if (res_type.is_array())
				code() += '[' + std::to_string(res_type.array_length) + ']';

			code() += " = ";
		}

		code() += id_to_name(function) + '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code() += id_to_name(args[i].base);

			if (i < num_args - 1)
				code() += ", ";
		}

		code() += ");\n";

		return res;
	}
	id   emit_call_intrinsic(const location &loc, id intrinsic, const type &res_type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert(arg.ops.empty() && arg.base != 0);

		const id res = make_id();

		code() += write_location(loc) + '\t';

		enum
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) name##i,
#include "effect_symbol_table_intrinsics.inl"
		};

		if (_shader_model >= 40 && (intrinsic == tex2Dsize0 || intrinsic == tex2Dsize1))
		{
			// Implementation of the 'tex2Dsize' intrinsic passes the result variable into 'GetDimensions' as output argument
			code() += write_type(res_type) + ' ' + id_to_name(res) + "; ";
		}
		else if (!res_type.is_void())
		{
			code() += write_type(res_type) + ' ' + id_to_name(res) + " = ";
		}

		switch (intrinsic)
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) case name##i: code break;
#include "effect_symbol_table_intrinsics.inl"
		default:
			assert(false);
		}

		code() += ";\n";

		return res;
	}
	id   emit_construct(const location &loc, const type &type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert((arg.type.is_scalar() || type.is_array()) && arg.ops.empty() && arg.base != 0);

		const id res = make_id();

		code() += write_location(loc) + '\t' + write_type(type) + ' ' + id_to_name(res);

		if (type.is_array())
			code() += '[' + std::to_string(type.array_length) + ']';

		code() += " = ";

		if (type.is_array())
			code() += "{ ";
		else
			code() += write_type(type, false, false) + '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code() += id_to_name(args[i].base);

			if (i < num_args - 1)
				code() += ", ";
		}

		if (type.is_array())
			code() += " }";
		else
			code() += ')';

		code() += ";\n";

		return res;
	}

	void emit_if(const location &loc, id condition_value, id condition_block, id true_statement_block, id false_statement_block, unsigned int flags) override
	{
		assert(condition_value != 0 && condition_block != 0 && true_statement_block != 0 && false_statement_block != 0);

		increase_indentation_level(_blocks[true_statement_block]);
		increase_indentation_level(_blocks[false_statement_block]);

		code() += _blocks[condition_block];
		code() += write_location(loc);

		if (flags & flatten) code() += "[flatten]";
		if (flags & dont_flatten) code() += "[branch]";

		code() += "\tif (" + id_to_name(condition_value) + ")\n\t{\n" + _blocks[true_statement_block] + "\t}\n";

		if (!_blocks[false_statement_block].empty())
			code() += "\telse\n\t{\n" + _blocks[false_statement_block] + "\t}\n";

		// Remove consumed blocks to save memory resources
		_blocks.erase(condition_block);
		_blocks.erase(true_statement_block);
		_blocks.erase(false_statement_block);
	}
	id   emit_phi(const location &loc, id condition_value, id condition_block, id true_value, id true_statement_block, id false_value, id false_statement_block, const type &type) override
	{
		assert(condition_value != 0 && condition_block != 0 && true_value != 0 && true_statement_block != 0 && false_value != 0 && false_statement_block != 0);

		increase_indentation_level(_blocks[true_statement_block]);
		increase_indentation_level(_blocks[false_statement_block]);

		const id res = make_id();

		code() += _blocks[condition_block] +
			'\t' + write_type(type) + ' ' + id_to_name(res) + ";\n" +
			write_location(loc) +
			"\tif (" + id_to_name(condition_value) + ")\n\t{\n" +
			(true_statement_block != condition_block ? _blocks[true_statement_block] : std::string()) +
			"\t\t" + id_to_name(res) + " = " + id_to_name(true_value) + ";\n" +
			"\t}\n\telse\n\t{\n" +
			(false_statement_block != condition_block ? _blocks[false_statement_block] : std::string()) +
			"\t\t" + id_to_name(res) + " = " + id_to_name(false_value) + ";\n" +
			"\t}\n";

		// Remove consumed blocks to save memory resources
		_blocks.erase(condition_block);
		_blocks.erase(true_statement_block);
		_blocks.erase(false_statement_block);

		return res;
	}
	void emit_loop(const location &loc, id condition_value, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, unsigned int flags) override
	{
		assert(condition_value != 0 && prev_block != 0 && header_block != 0 && loop_block != 0 && continue_block != 0);

		increase_indentation_level(_blocks[loop_block]);
		increase_indentation_level(_blocks[continue_block]);

		code() += _blocks[prev_block];

		if (condition_block == 0)
			code() += "\tbool " + id_to_name(condition_value) + ";\n";
		else
			code() += _blocks[condition_block];

		code() += write_location(loc) + '\t';

		if (flags & unroll) code() += "[unroll] ";
		if (flags & dont_unroll) code() += "[loop] ";

		if (condition_block == 0)
		{
			// Convert variable initializer to assignment statement
			std::string loop_condition = std::move(_blocks[continue_block]);
			auto pos_assign = loop_condition.rfind(id_to_name(condition_value));
			auto pos_prev_assign = loop_condition.rfind('\t', pos_assign);
			loop_condition.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign - 1);

			code() += "do\n\t{\n" + _blocks[loop_block] + loop_condition + "}\n\twhile (" + id_to_name(condition_value) + ");\n";
		}
		else
		{
			increase_indentation_level(_blocks[condition_block]);

			// Convert variable initializer to assignment statement
			std::string loop_condition = std::move(_blocks[condition_block]);
			auto pos_assign = loop_condition.rfind(id_to_name(condition_value));
			auto pos_prev_assign = loop_condition.rfind('\t', pos_assign);
			loop_condition.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign - 1);

			code() += "while (" + id_to_name(condition_value) + ")\n\t{\n" + _blocks[loop_block] + _blocks[continue_block] + loop_condition + "\t}\n";

			_blocks.erase(condition_block);
		}

		// Remove consumed blocks to save memory resources
		_blocks.erase(prev_block);
		_blocks.erase(header_block);
		_blocks.erase(loop_block);
		_blocks.erase(continue_block);
	}
	void emit_switch(const location &loc, id selector_value, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, unsigned int flags) override
	{
		assert(selector_value != 0 && selector_block != 0 && default_label != 0);

		code() += _blocks[selector_block];
		code() += write_location(loc) + '\t';

		if (flags & flatten) code() += "[flatten]";
		if (flags & dont_flatten) code() += "[branch]";

		code() += "switch (" + id_to_name(selector_value) + ")\n\t{\n";

		for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
		{
			assert(case_literal_and_labels[i + 1] != 0);

			increase_indentation_level(_blocks[case_literal_and_labels[i + 1]]);

			code() += "\t\tcase " + std::to_string(case_literal_and_labels[i]) + ": {\n" + _blocks[case_literal_and_labels[i + 1]];

			// Handle fall-through blocks
			for (id fallthrough : _switch_fallthrough_blocks[case_literal_and_labels[i + 1]])
				code() += _blocks[fallthrough];

			code() += "\t\t}\n";
		}

		if (default_label != _current_block)
		{
			increase_indentation_level(_blocks[default_label]);

			code() += "\t\tdefault: {\n" + _blocks[default_label] + "\t\t}\n";

			_blocks.erase(default_label);
		}

		code() += "\t}\n";

		// Remove consumed blocks to save memory resources
		_blocks.erase(selector_block);
		for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
			_blocks.erase(case_literal_and_labels[i + 1]);
	}

	bool is_in_block() const override { return _current_block != 0; }
	bool is_in_function() const override { return is_in_block(); }

	id   set_block(id id) override
	{
		_last_block = _current_block;
		_current_block = id;

		return _last_block;
	}
	void enter_block(id id) override
	{
		_current_block = id;
	}
	id   leave_block_and_kill() override
	{
		if (!is_in_block())
			return 0;

		code() += "\tdiscard;\n";

		return set_block(0);
	}
	id   leave_block_and_return(id value) override
	{
		if (!is_in_block())
			return 0;

		// Skip implicit return statement
		if (!_functions.back()->return_type.is_void() && value == 0)
			return set_block(0);

		code() += "\treturn";

		if (value != 0)
			code() += ' ' + id_to_name(value);

		code() += ";\n";

		return set_block(0);
	}
	id   leave_block_and_switch(id, id) override
	{
		if (!is_in_block())
			return _last_block;

		return set_block(0);
	}
	id   leave_block_and_branch(id target, unsigned int loop_flow) override
	{
		if (!is_in_block())
			return _last_block;

		switch (loop_flow)
		{
		case 1:
			code() += "\tbreak;\n";
			break;
		case 2:
			code() += "\tcontinue;\n";
			break;
		case 3:
			_switch_fallthrough_blocks[_current_block].push_back(target);
			break;
		}

		return set_block(0);
	}
	id   leave_block_and_branch_conditional(id, id, id) override
	{
		if (!is_in_block())
			return _last_block;

		return set_block(0);
	}
	void leave_function() override
	{
		assert(_last_block != 0);

		code() += "{\n" + _blocks[_last_block] + "}\n";
	}
};

codegen *create_codegen_hlsl(unsigned int shader_model, bool debug_info)
{
	return new codegen_hlsl(shader_model, debug_info);
}
