/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <cmath> // signbit, isinf, isnan
#include <cstdio> // snprintf
#include <cassert>
#include <cstring> // stricmp
#include <algorithm> // std::max

using namespace reshadefx;

class codegen_hlsl final : public codegen
{
public:
	codegen_hlsl(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants)
		: _shader_model(shader_model), _debug_info(debug_info), _uniforms_to_spec_constants(uniforms_to_spec_constants)
	{
		// Create default block and reserve a memory block to avoid frequent reallocations
		std::string &block = _blocks.emplace(0, std::string()).first->second;
		block.reserve(8192);
	}

private:
	enum class naming
	{
		// Name should already be unique, so no additional steps are taken
		unique,
		// Will be numbered when clashing with another name
		general,
		// Replace name with a code snippet
		expression,
	};

	std::string _cbuffer_block;
	std::string _current_location;
	std::unordered_map<id, std::string> _names;
	std::unordered_map<id, std::string> _blocks;
	bool _debug_info = false;
	bool _uniforms_to_spec_constants = false;
	unsigned int _shader_model = 0;

	void write_result(module &module) override
	{
		module = std::move(_module);

		if (_shader_model >= 40)
		{
			module.hlsl += "struct __sampler2D { Texture2D t; SamplerState s; };\n";

			if (!_cbuffer_block.empty())
				module.hlsl += "cbuffer _Globals {\n" + _cbuffer_block + "};\n";
		}
		else
		{
			module.hlsl += "struct __sampler2D { sampler2D s; float2 pixelsize; };\nuniform float2 __TEXEL_SIZE__ : register(c255);\n";

			if (!_cbuffer_block.empty())
				module.hlsl += _cbuffer_block;

			// Offsets were multiplied in 'define_uniform', so adjust total size here accordingly
			module.total_uniform_size *= 4;
		}

		module.hlsl += _blocks.at(0);
	}

	template <bool is_param = false, bool is_decl = true>
	void write_type(std::string &s, const type &type) const
	{
		if constexpr (is_decl)
		{
			if (type.has(type::q_precise))
				s += "precise ";
		}

		if constexpr (is_param)
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
			// In shader model 3, uints can only be used with known-positive values, so use ints instead
			s += _shader_model >= 40 ? "uint" : "int";
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
	}
	void write_constant(std::string &s, const type &type, const constant &data) const
	{
		if (type.is_array())
		{
			auto elem_type = type;
			elem_type.array_length = 0;

			s += "{ ";

			for (int i = 0; i < type.array_length; ++i)
			{
				write_constant(s, elem_type, i < static_cast<int>(data.array_data.size()) ? data.array_data[i] : constant());

				if (i < type.array_length - 1)
					s += ", ";
			}

			s += " }";
			return;
		}

		if (type.is_struct())
		{
			assert(data.as_uint[0] == 0);

			s += '(' + id_to_name(type.definition) + ")0";
			return;
		}

		// There can only be numeric constants
		assert(type.is_numeric());

		if (!type.is_scalar())
			write_type<false, false>(s, type), s += '(';

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
				if (std::isnan(data.as_float[i])) {
					s += "-1.#IND";
					break;
				}
				if (std::isinf(data.as_float[i])) {
					s += std::signbit(data.as_float[i]) ? "1.#INF" : "-1.#INF";
					break;
				}
				char temp[64] = "";
				std::snprintf(temp, sizeof(temp), "%.8f", data.as_float[i]);
				s += temp;
				break;
			}

			if (i < components - 1)
				s += ", ";
		}

		if (!type.is_scalar())
			s += ')';
	}
	template <bool force_source = false>
	void write_location(std::string &s, const location &loc)
	{
		if (loc.source.empty() || !_debug_info)
			return;

		s += "#line " + std::to_string(loc.line);

		// Avoid writing the file name every time to reduce output text size
		if constexpr (force_source)
		{
			s += " \"" + loc.source + '\"';
		}
		else if (loc.source != _current_location)
		{
			s += " \"" + loc.source + '\"';

			_current_location = loc.source;
		}

		s += '\n';
	}

	std::string id_to_name(id id) const
	{
		if (const auto it = _names.find(id); it != _names.end())
			return it->second;
		return '_' + std::to_string(id);
	}

	template <naming naming_type = naming::general>
	void define_name(const id id, std::string name)
	{
		assert(!name.empty());
		if constexpr (naming_type != naming::expression)
			if (name[0] == '_')
				return; // Filter out names that may clash with automatic ones
		name = escape_name(std::move(name));
		if constexpr (naming_type == naming::general)
			if (std::find_if(_names.begin(), _names.end(), [&name](const auto &it) { return it.second == name; }) != _names.end())
				name += '_' + std::to_string(id); // Append a numbered suffix if the name already exists
		_names[id] = std::move(name);
	}

	std::string convert_semantic(const std::string &semantic) const
	{
		if (_shader_model < 40)
		{
			if (semantic == "SV_POSITION")
				return "POSITION"; // For pixel shaders this has to be "VPOS", so need to redefine that in post
			if (semantic.compare(0, 9, "SV_TARGET") == 0)
				return "COLOR" + semantic.substr(9);
			if (semantic == "SV_DEPTH")
				return "DEPTH";
			if (semantic == "SV_VERTEXID")
				return "TEXCOORD0 /* VERTEXID */";
		}
		else
		{
			if (semantic == "POSITION" || semantic == "VPOS")
				return "SV_POSITION";
			if (semantic.compare(0, 5, "COLOR") == 0)
				return "SV_TARGET" + semantic.substr(5);
			if (semantic == "DEPTH")
				return "SV_DEPTH";
		}

		return semantic;
	}

	static std::string escape_name(std::string name)
	{
		static const auto stringicmp = [](const std::string &a, const std::string &b) {
#ifdef _WIN32
			return _stricmp(a.c_str(), b.c_str()) == 0;
#else
			return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) { return tolower(a) == tolower(b); });
#endif
		};

		// HLSL compiler complains about "technique" and "pass" names in strict mode (no matter the casing)
		if (stringicmp(name, "pass") ||
			stringicmp(name, "technique"))
			name += "_RESERVED";

		return name;
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
		define_name<naming::unique>(info.definition, info.unique_name);

		_structs.push_back(info);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += "struct " + id_to_name(info.definition) + "\n{\n";

		for (const auto &member : info.member_list)
		{
			code += '\t';
			write_type<true>(code, member.type); // HLSL allows interpolation attributes on struct members, so handle this like a parameter
			code += ' ' + member.name;
			if (member.type.is_array())
				code += '[' + std::to_string(member.type.array_length) + ']';
			if (!member.semantic.empty())
				code += " : " + convert_semantic(member.semantic);
			code += ";\n";
		}

		code += "};\n";

		return info.definition;
	}
	id   define_texture(const location &loc, texture_info &info) override
	{
		info.id = make_id();
		info.binding = _module.num_texture_bindings;

		_module.textures.push_back(info);

		std::string &code = _blocks.at(_current_block);

		if (_shader_model >= 40)
		{
			write_location(code, loc);

			code += "Texture2D "       + info.unique_name + " : register(t" + std::to_string(info.binding + 0) + ");\n";
			code += "Texture2D __srgb" + info.unique_name + " : register(t" + std::to_string(info.binding + 1) + ");\n";

			_module.num_texture_bindings += 2;
		}

		return info.id;
	}
	id   define_sampler(const location &loc, sampler_info &info) override
	{
		info.id = make_id();

		define_name<naming::unique>(info.id, info.unique_name);

		const auto texture = std::find_if(_module.textures.begin(), _module.textures.end(),
			[&info](const auto &it) { return it.unique_name == info.texture_name; });
		assert(texture != _module.textures.end());

		std::string &code = _blocks.at(_current_block);

		if (_shader_model >= 40)
		{
			// Try and reuse a sampler binding with the same sampler description
			const auto existing_sampler = std::find_if(_module.samplers.begin(), _module.samplers.end(),
				[&info](const auto &it) { return it.filter == info.filter && it.address_u == info.address_u && it.address_v == info.address_v && it.address_w == info.address_w && it.min_lod == info.min_lod && it.max_lod == info.max_lod && it.lod_bias == info.lod_bias; });

			if (existing_sampler != _module.samplers.end())
			{
				info.binding = existing_sampler->binding;
			}
			else
			{
				info.binding = _module.num_sampler_bindings++;

				code += "SamplerState __s" + std::to_string(info.binding) + " : register(s" + std::to_string(info.binding) + ");\n";
			}

			assert(info.srgb == 0 || info.srgb == 1);
			info.texture_binding = texture->binding + info.srgb; // Offset binding by one to choose the SRGB variant

			write_location(code, loc);

			code += "static const __sampler2D " + id_to_name(info.id) + " = { " + (info.srgb ? "__srgb" : "") + info.texture_name + ", __s" + std::to_string(info.binding) + " };\n";
		}
		else
		{
			info.binding = _module.num_sampler_bindings++;

			code += "sampler2D __" + info.unique_name + "_s : register(s" + std::to_string(info.binding) + ");\n";

			write_location(code, loc);

			code += "static const __sampler2D " + id_to_name(info.id) + " = { __" + info.unique_name + "_s, float2(";

			if (texture->semantic.empty())
				code += "1.0 / " + std::to_string(texture->width) + ", 1.0 / " + std::to_string(texture->height);
			else
				code += texture->semantic + "_PIXEL_SIZE"; // Expect application to set inverse texture size via a define if it is not known here

			code += ") }; \n";
		}

		_module.samplers.push_back(info);

		return info.id;
	}
	id   define_uniform(const location &loc, uniform_info &info) override
	{
		const id res = make_id();

		define_name<naming::unique>(res, info.name);

		if (_uniforms_to_spec_constants && info.has_initializer_value)
		{
			info.size = info.type.components() * 4;
			if (info.type.is_array())
				info.size += info.type.array_length;

			std::string &code = _blocks.at(_current_block);

			write_location(code, loc);

			code += "static const ";
			write_type(code, info.type);
			code += ' ' + id_to_name(res) + " = ";
			if (!info.type.is_scalar())
				write_type<false, false>(code, info.type);
			code += "(SPEC_CONSTANT_" + info.name + ");\n";

			_module.spec_constants.push_back(info);
		}
		else
		{
			if (info.type.is_matrix())
				info.size = align_up(info.type.cols * 4, 16, info.type.rows);
			else // Vectors are column major (1xN), matrices are row major (NxM)
				info.size = info.type.rows * 4;
			// Arrays are not packed in HLSL by default, each element is stored in a four-component vector (16 bytes)
			if (info.type.is_array())
				info.size = align_up(info.size, 16, info.type.array_length);

			// Data is packed into 4-byte boundaries (see https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules)
			// This is already guaranteed, since all types are at least 4-byte in size
			info.offset = _module.total_uniform_size;
			// Additionally, HLSL packs data so that it does not cross a 16-byte boundary
			const uint32_t remaining = 16 - (info.offset & 15);
			if (remaining != 16 && info.size > remaining)
				info.offset += remaining;
			_module.total_uniform_size = info.offset + info.size;

			write_location<true>(_cbuffer_block, loc);

			if (_shader_model >= 40)
				_cbuffer_block += '\t';
			if (info.type.is_matrix()) // Force row major matrices
				_cbuffer_block += "row_major ";

			type type = info.type;
			if (_shader_model < 40)
			{
				// The HLSL compiler tries to evaluate boolean values with temporary registers, which breaks branches, so force it to use constant float registers
				if (type.is_boolean())
					type.base = type::t_float;

				// Simply put each uniform into a separate constant register in shader model 3 for now
				info.offset *= 4;
			}

			write_type(_cbuffer_block, type);
			_cbuffer_block += ' ' + id_to_name(res);

			if (info.type.is_array())
				_cbuffer_block += '[' + std::to_string(info.type.array_length) + ']';

			if (_shader_model < 40)
			{
				// Every constant register is 16 bytes wide, so divide memory offset by 16 to get the constant register index
				// Note: All uniforms are floating-point in shader model 3, even if the uniform type says different!!
				_cbuffer_block += " : register(c" + std::to_string(info.offset / 16) + ')';
			}

			_cbuffer_block += ";\n";

			_module.uniforms.push_back(info);
		}

		return res;
	}
	id   define_variable(const location &loc, const type &type, std::string name, bool global, id initializer_value) override
	{
		const id res = make_id();

		if (!name.empty())
			define_name<naming::general>(res, name);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		if (!global)
			code += '\t';

		write_type(code, type);
		code += ' ' + id_to_name(res);

		if (type.is_array())
			code += '[' + std::to_string(type.array_length) + ']';

		if (initializer_value != 0)
			code += " = " + id_to_name(initializer_value);

		code += ";\n";

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
		define_name<naming::unique>(info.definition, std::move(name));

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		write_type(code, info.return_type);
		code += ' ' + id_to_name(info.definition) + '(';

		for (size_t i = 0, num_params = info.parameter_list.size(); i < num_params; ++i)
		{
			auto &param = info.parameter_list[i];

			param.definition = make_id();
			define_name<naming::unique>(param.definition, param.name);

			code += '\n';
			write_location(code, param.location);
			code += '\t';
			write_type<true>(code, param.type);
			code += ' ' + id_to_name(param.definition);

			if (param.type.is_array())
				code += '[' + std::to_string(param.type.array_length) + ']';

			if (is_entry_point && !param.semantic.empty())
				code += " : " + convert_semantic(param.semantic);

			if (i < num_params - 1)
				code += ',';
		}

		code += ')';

		if (is_entry_point && !info.return_semantic.empty())
			code += " : " + convert_semantic(info.return_semantic);

		code += '\n';

		_functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}

	void define_entry_point(const function_info &func, bool is_ps) override
	{
		if (const auto it = std::find_if(_module.entry_points.begin(), _module.entry_points.end(),
			[&func](const auto &ep) { return ep.name == func.unique_name; }); it != _module.entry_points.end())
			return;

		_module.entry_points.push_back({ func.unique_name, is_ps });

		// Only have to rewrite the entry point function signature in shader model 3
		if (_shader_model >= 40)
			return;

		auto entry_point = func;

		const auto is_color_semantic = [](const std::string &semantic) { return semantic.compare(0, 9, "SV_TARGET") == 0 || semantic.compare(0, 5, "COLOR") == 0; };
		const auto is_position_semantic = [](const std::string &semantic) { return semantic == "SV_POSITION" || semantic == "POSITION"; };

		const auto ret = make_id();
		define_name<naming::general>(ret, "ret");

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

		std::string &code = _blocks.at(_current_block);

		// Clear all color output parameters so no component is left uninitialized
		for (auto &param : entry_point.parameter_list)
			if (is_color_semantic(param.semantic))
				code += '\t' + param.name + " = float4(0.0, 0.0, 0.0, 0.0);\n";

		code += '\t';
		if (is_color_semantic(func.return_semantic))
			code += "const float4 " + id_to_name(ret) + " = float4(";
		else if (!func.return_type.is_void())
			write_type(code, func.return_type), code += ' ' + id_to_name(ret) + " = ";

		// Call the function this entry point refers to
		code += id_to_name(func.definition) + '(';

		for (size_t i = 0, num_params = func.parameter_list.size(); i < num_params; ++i)
		{
			code += func.parameter_list[i].name;

			if (is_color_semantic(func.parameter_list[i].semantic))
			{
				code += '.';
				for (unsigned int k = 0; k < func.parameter_list[i].type.rows; k++)
					code += "xyzw"[k];
			}

			if (i < num_params - 1)
				code += ", ";
		}

		code += ')';

		// Cast the output value to a four-component vector
		if (is_color_semantic(func.return_semantic))
		{
			for (unsigned int i = 0; i < 4 - func.return_type.rows; i++)
				code += ", 0.0";
			code += ')';
		}

		code += ";\n";

		// Shift everything by half a viewport pixel to workaround the different half-pixel offset in D3D9 (https://aras-p.info/blog/2016/04/08/solving-dx9-half-pixel-offset/)
		if (!position_variable_name.empty() && !is_ps) // Check if we are in a vertex shader definition
			code += '\t' + position_variable_name + ".xy += __TEXEL_SIZE__ * " + position_variable_name + ".ww;\n";

		leave_block_and_return(func.return_type.is_void() ? 0 : ret);
		leave_function();
	}

	id   emit_load(const expression &exp, bool force_new_id) override
	{
		if (exp.is_constant)
			return emit_constant(exp.type, exp.constant);
		else if (exp.chain.empty() && !force_new_id) // Can refer to values without access chain directly
			return exp.base;

		const id res = make_id();

		static const char s_matrix_swizzles[16][5] = {
			"_m00", "_m01", "_m02", "_m03",
			"_m10", "_m11", "_m12", "_m13",
			"_m20", "_m21", "_m22", "_m23",
			"_m30", "_m31", "_m32", "_m33"
		};

		std::string type, expr_code = id_to_name(exp.base);

		for (const auto &op : exp.chain)
		{
			switch (op.op)
			{
			case expression::operation::op_cast:
				type.clear();
				write_type<false, false>(type, op.to);
				// Cast is in parentheses so that a subsequent operation operates on the casted value
				expr_code = "((" + type + ')' + expr_code + ')';
				break;
			case expression::operation::op_member:
				expr_code += '.';
				expr_code += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_dynamic_index:
				expr_code += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_constant_index:
				if (op.from.is_vector() && !op.from.is_array())
					expr_code += '.',
					expr_code += "xyzw"[op.index];
				else
					expr_code += '[' + std::to_string(op.index) + ']';
				break;
			case expression::operation::op_swizzle:
				expr_code += '.';
				for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					if (op.from.is_matrix())
						expr_code += s_matrix_swizzles[op.swizzle[i]];
					else
						expr_code += "xyzw"[op.swizzle[i]];
				break;
			}
		}

		if (force_new_id)
		{
			// Need to store value in a new variable to comply with request for a new ID
			std::string &code = _blocks.at(_current_block);

			code += '\t';
			write_type(code, exp.type);
			code += ' ' + id_to_name(res) + " = " + expr_code + ";\n";
		}
		else
		{
			// Avoid excessive variable definitions by instancing simple load operations in code every time
			define_name<naming::expression>(res, std::move(expr_code));
		}

		return res;
	}
	void emit_store(const expression &exp, id value) override
	{
		std::string &code = _blocks.at(_current_block);

		write_location(code, exp.location);

		code += '\t' + id_to_name(exp.base);

		static const char s_matrix_swizzles[16][5] = {
			"_m00", "_m01", "_m02", "_m03",
			"_m10", "_m11", "_m12", "_m13",
			"_m20", "_m21", "_m22", "_m23",
			"_m30", "_m31", "_m32", "_m33"
		};

		for (const auto &op : exp.chain)
		{
			switch (op.op)
			{
			case expression::operation::op_member:
				code += '.';
				code += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_dynamic_index:
				code += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_constant_index:
				code += '[' + std::to_string(op.index) + ']';
				break;
			case expression::operation::op_swizzle:
				code += '.';
				for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					if (op.from.is_matrix())
						code += s_matrix_swizzles[op.swizzle[i]];
					else
						code += "xyzw"[op.swizzle[i]];
				break;
			}
		}

		code += " = " + id_to_name(value) + ";\n";
	}

	id   emit_constant(const type &type, const constant &data) override
	{
		const id res = make_id();

		if (type.is_array())
		{
			std::string &code = _blocks.at(_current_block);

			// Array constants need to be stored in a constant variable as they cannot be used in-place
			code += "\tconst ";
			write_type(code, type);
			code += ' ' + id_to_name(res);
			code += '[' + std::to_string(type.array_length) + ']';
			code += " = ";
			write_constant(code, type, data);
			code += ";\n";
			return res;
		}

		std::string code;
		write_constant(code, type, data);
		define_name<naming::expression>(res, std::move(code));

		return res;
	}

	id   emit_unary_op(const location &loc, tokenid op, const type &res_type, id val) override
	{
		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, res_type);
		code += ' ' + id_to_name(res) + " = ";

		if (_shader_model < 40 && op == tokenid::tilde)
			code += "0xFFFFFFFF - "; // Emulate bitwise not operator on shader model 3
		else
			code += char(op);

		code += id_to_name(val) + ";\n";

		return res;
	}
	id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &, id lhs, id rhs) override
	{
		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, res_type);
		code += ' ' + id_to_name(res) + " = ";

		if (_shader_model < 40)
		{
			// See bitwise shift operator emulation below
			if (op == tokenid::less_less || op == tokenid::less_less_equal)
				code += '(';
			else if (op == tokenid::greater_greater || op == tokenid::greater_greater_equal)
				code += "floor(";
		}

		code += id_to_name(lhs) + ' ';

		switch (op)
		{
		case tokenid::plus:
		case tokenid::plus_plus:
		case tokenid::plus_equal:
			code += '+';
			break;
		case tokenid::minus:
		case tokenid::minus_minus:
		case tokenid::minus_equal:
			code += '-';
			break;
		case tokenid::star:
		case tokenid::star_equal:
			code += '*';
			break;
		case tokenid::slash:
		case tokenid::slash_equal:
			code += '/';
			break;
		case tokenid::percent:
		case tokenid::percent_equal:
			code += '%';
			break;
		case tokenid::caret:
		case tokenid::caret_equal:
			code += '^';
			break;
		case tokenid::pipe:
		case tokenid::pipe_equal:
			code += '|';
			break;
		case tokenid::ampersand:
		case tokenid::ampersand_equal:
			code += '&';
			break;
		case tokenid::less_less:
		case tokenid::less_less_equal:
			code += _shader_model >= 40 ? "<<" : ") * exp2("; // Emulate bitwise shift operators on shader model 3
			break;
		case tokenid::greater_greater:
		case tokenid::greater_greater_equal:
			code += _shader_model >= 40 ? ">>" : ") / exp2(";
			break;
		case tokenid::pipe_pipe:
			code += "||";
			break;
		case tokenid::ampersand_ampersand:
			code += "&&";
			break;
		case tokenid::less:
			code += '<';
			break;
		case tokenid::less_equal:
			code += "<=";
			break;
		case tokenid::greater:
			code += '>';
			break;
		case tokenid::greater_equal:
			code += ">=";
			break;
		case tokenid::equal_equal:
			code += "==";
			break;
		case tokenid::exclaim_equal:
			code += "!=";
			break;
		default:
			assert(false);
		}

		code += ' ' + id_to_name(rhs);

		if (_shader_model < 40)
		{
			// See bitwise shift operator emulation above
			if (op == tokenid::less_less || op == tokenid::less_less_equal ||
				op == tokenid::greater_greater || op == tokenid::greater_greater_equal)
				code += ')';
		}

		code += ";\n";

		return res;
	}
	id   emit_ternary_op(const location &loc, tokenid op, const type &res_type, id condition, id true_value, id false_value) override
	{
		if (op != tokenid::question)
			return assert(false), 0; // Should never happen, since this is the only ternary operator currently supported

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, res_type);
		code += ' ' + id_to_name(res);

		if (res_type.is_array())
			code += '[' + std::to_string(res_type.array_length) + ']';

		code += " = " + id_to_name(condition) + " ? " + id_to_name(true_value) + " : " + id_to_name(false_value) + ";\n";

		return res;
	}
	id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) override
	{
#ifndef NDEBUG
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);
#endif

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';

		if (!res_type.is_void())
		{
			write_type(code, res_type);
			code += ' ' + id_to_name(res);

			if (res_type.is_array())
				code += '[' + std::to_string(res_type.array_length) + ']';

			code += " = ";
		}

		code += id_to_name(function) + '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code += id_to_name(args[i].base);

			if (i < num_args - 1)
				code += ", ";
		}

		code += ");\n";

		return res;
	}
	id   emit_call_intrinsic(const location &loc, id intrinsic, const type &res_type, const std::vector<expression> &args) override
	{
#ifndef NDEBUG
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);
#endif

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';

		enum
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) name##i,
#include "effect_symbol_table_intrinsics.inl"
		};

		if (_shader_model >= 40 && (intrinsic == tex2Dsize0 || intrinsic == tex2Dsize1))
		{
			// Implementation of the 'tex2Dsize' intrinsic passes the result variable into 'GetDimensions' as output argument
			write_type(code, res_type);
			code += ' ' + id_to_name(res) + "; ";
		}
		else if (!res_type.is_void())
		{
			write_type(code, res_type);
			code += ' ' + id_to_name(res) + " = ";
		}

		switch (intrinsic)
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) case name##i: code break;
#include "effect_symbol_table_intrinsics.inl"
		default:
			assert(false);
		}

		code += ";\n";

		return res;
	}
	id   emit_construct(const location &loc, const type &type, const std::vector<expression> &args) override
	{
#ifndef NDEBUG
		for (const auto &arg : args)
			assert((arg.type.is_scalar() || type.is_array()) && arg.chain.empty() && arg.base != 0);
#endif

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, type);
		code += ' ' + id_to_name(res);

		if (type.is_array())
			code += '[' + std::to_string(type.array_length) + ']';

		code += " = ";

		if (type.is_array())
			code += "{ ";
		else
			write_type<false, false>(code, type), code += '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code += id_to_name(args[i].base);

			if (i < num_args - 1)
				code += ", ";
		}

		if (type.is_array())
			code += " }";
		else
			code += ')';

		code += ";\n";

		return res;
	}

	void emit_if(const location &loc, id condition_value, id condition_block, id true_statement_block, id false_statement_block, unsigned int flags) override
	{
		assert(condition_value != 0 && condition_block != 0 && true_statement_block != 0 && false_statement_block != 0);

		std::string &code = _blocks.at(_current_block);

		std::string &true_statement_data = _blocks.at(true_statement_block);
		std::string &false_statement_data = _blocks.at(false_statement_block);

		increase_indentation_level(true_statement_data);
		increase_indentation_level(false_statement_data);

		code += _blocks.at(condition_block);

		write_location(code, loc);

		code += '\t';

		if (flags & 0x1) code += "[flatten] ";
		if (flags & 0x2) code +=  "[branch] ";

		code += "if (" + id_to_name(condition_value) + ")\n\t{\n";
		code += true_statement_data;
		code += "\t}\n";

		if (!false_statement_data.empty())
		{
			code += "\telse\n\t{\n";
			code += false_statement_data;
			code += "\t}\n";
		}

		// Remove consumed blocks to save memory
		_blocks.erase(condition_block);
		_blocks.erase(true_statement_block);
		_blocks.erase(false_statement_block);
	}
	id   emit_phi(const location &loc, id condition_value, id condition_block, id true_value, id true_statement_block, id false_value, id false_statement_block, const type &type) override
	{
		assert(condition_value != 0 && condition_block != 0 && true_value != 0 && true_statement_block != 0 && false_value != 0 && false_statement_block != 0);

		std::string &code = _blocks.at(_current_block);

		std::string &true_statement_data = _blocks.at(true_statement_block);
		std::string &false_statement_data = _blocks.at(false_statement_block);

		increase_indentation_level(true_statement_data);
		increase_indentation_level(false_statement_data);

		const id res = make_id();

		code += _blocks.at(condition_block);

		code += '\t';
		write_type(code, type);
		code += ' ' + id_to_name(res) + ";\n";

		write_location(code, loc);

		code += "\tif (" + id_to_name(condition_value) + ")\n\t{\n";
		code += (true_statement_block != condition_block ? true_statement_data : std::string());
		code += "\t\t" + id_to_name(res) + " = " + id_to_name(true_value) + ";\n";
		code += "\t}\n\telse\n\t{\n";
		code += (false_statement_block != condition_block ? false_statement_data : std::string());
		code += "\t\t" + id_to_name(res) + " = " + id_to_name(false_value) + ";\n";
		code += "\t}\n";

		// Remove consumed blocks to save memory
		_blocks.erase(condition_block);
		_blocks.erase(true_statement_block);
		_blocks.erase(false_statement_block);

		return res;
	}
	void emit_loop(const location &loc, id condition_value, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, unsigned int flags) override
	{
		assert(prev_block != 0 && header_block != 0 && loop_block != 0 && continue_block != 0);

		std::string &code = _blocks.at(_current_block);

		std::string &loop_data = _blocks.at(loop_block);
		std::string &continue_data = _blocks.at(continue_block);

		// Condition value can be missing in infinite loop constructs like "for (;;)"
		const std::string condition_name = condition_value != 0 ? id_to_name(condition_value) : "true";

		increase_indentation_level(loop_data);
		increase_indentation_level(loop_data);
		increase_indentation_level(continue_data);

		code += _blocks.at(prev_block);

		if (condition_block == 0)
			code += "\tbool " + condition_name + ";\n";
		else
			code += _blocks.at(condition_block);

		write_location(code, loc);

		code += '\t';

		if (flags & 0x1) code += "[unroll] ";
		if (flags & 0x2) code +=   "[loop] ";

		if (condition_block == 0)
		{
			// Convert variable initializer to assignment statement
			auto pos_assign = continue_data.rfind(condition_name);
			auto pos_prev_assign = continue_data.rfind('\t', pos_assign);
			continue_data.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign - 1);

			// We need to add the continue block to all "continue" statements as well
			const std::string continue_id = "__CONTINUE__" + std::to_string(continue_block);
			for (size_t offset = 0; (offset = loop_data.find(continue_id, offset)) != std::string::npos; offset += continue_data.size())
				loop_data.replace(offset, continue_id.size(), continue_data);

			code += "do\n\t{\n\t\t{\n";
			code += loop_data; // Encapsulate loop body into another scope, so not to confuse any local variables with the current iteration variable accessed in the continue block below
			code += "\t\t}\n";
			code += continue_data;
			code += "\t}\n\twhile (" + condition_name + ");\n";
		}
		else
		{
			std::string &condition_data = _blocks.at(condition_block);

			increase_indentation_level(condition_data);

			// Convert variable initializer to assignment statement
			auto pos_assign = condition_data.rfind(condition_name);
			auto pos_prev_assign = condition_data.rfind('\t', pos_assign);
			condition_data.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign - 1);

			const std::string continue_id = "__CONTINUE__" + std::to_string(continue_block);
			for (size_t offset = 0; (offset = loop_data.find(continue_id, offset)) != std::string::npos; offset += continue_data.size())
				loop_data.replace(offset, continue_id.size(), continue_data + condition_data);

			code += "while (" + condition_name + ")\n\t{\n\t\t{\n";
			code += loop_data;
			code += "\t\t}\n";
			code += continue_data;
			code += condition_data;
			code += "\t}\n";

			_blocks.erase(condition_block);
		}

		// Remove consumed blocks to save memory
		_blocks.erase(prev_block);
		_blocks.erase(header_block);
		_blocks.erase(loop_block);
		_blocks.erase(continue_block);
	}
	void emit_switch(const location &loc, id selector_value, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, unsigned int flags) override
	{
		assert(selector_value != 0 && selector_block != 0 && default_label != 0);

		std::string &code = _blocks.at(_current_block);

		code += _blocks.at(selector_block);

		if (_shader_model >= 40)
		{
			write_location(code, loc);

			code += '\t';

			if (flags & 0x1) code += "[flatten] ";
			if (flags & 0x2) code += "[branch] ";

			code += "switch (" + id_to_name(selector_value) + ")\n\t{\n";

			for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
			{
				assert(case_literal_and_labels[i + 1] != 0);

				std::string &case_data = _blocks.at(case_literal_and_labels[i + 1]);

				increase_indentation_level(case_data);

				code += "\tcase " + std::to_string(case_literal_and_labels[i]) + ": {\n";
				code += case_data;
				code += "\t}\n";
			}

			if (default_label != _current_block)
			{
				std::string &default_data = _blocks.at(default_label);

				increase_indentation_level(default_data);

				code += "\tdefault: {\n";
				code += default_data;
				code += "\t}\n";

				_blocks.erase(default_label);
			}

			code += "\t}\n";
		}
		else // Switch statements do not work correctly in SM3 if a constant is used as selector value (this is a D3DCompiler bug), so replace them with if statements
		{
			write_location(code, loc);

			code += "\t[unroll] do { "; // This dummy loop makes "break" statements work

			if (flags & 0x1) code += "[flatten] ";
			if (flags & 0x2) code += "[branch] ";

			for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
			{
				assert(case_literal_and_labels[i + 1] != 0);

				std::string &case_data = _blocks.at(case_literal_and_labels[i + 1]);

				increase_indentation_level(case_data);

				code += "if (" + id_to_name(selector_value) + " == " + std::to_string(case_literal_and_labels[i]) + ")\n\t{\n";
				code += case_data;
				code += "\t}\n\telse\n\t";

			}

			code += "{\n";

			if (default_label != _current_block)
			{
				std::string &default_data = _blocks.at(default_label);

				increase_indentation_level(default_data);

				code += default_data;

				_blocks.erase(default_label);
			}

			code += "\t} } while (false);\n";
		}

		// Remove consumed blocks to save memory
		_blocks.erase(selector_block);
		for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
			_blocks.erase(case_literal_and_labels[i + 1]);
	}

	id   create_block() override
	{
		const id res = make_id();

		std::string &block = _blocks.emplace(res, std::string()).first->second;
		// Reserve a decently big enough memory block to avoid frequent reallocations
		block.reserve(4096);

		return res;
	}
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

		std::string &code = _blocks.at(_current_block);

		code += "\tdiscard;\n";

		const auto &return_type = _functions.back()->return_type;
		if (!return_type.is_void())
		{
			// HLSL compiler doesn't handle discard like a shader kill
			// Add a return statement to exit functions in case discard is the last control flow statement
			// See https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/discard--sm4---asm-
			code += "\treturn ";
			write_constant(code, return_type, constant());
			code += ";\n";
		}

		return set_block(0);
	}
	id   leave_block_and_return(id value) override
	{
		if (!is_in_block())
			return 0;

		// Skip implicit return statement
		if (!_functions.back()->return_type.is_void() && value == 0)
			return set_block(0);

		std::string &code = _blocks.at(_current_block);

		code += "\treturn";

		if (value != 0)
			code += ' ' + id_to_name(value);

		code += ";\n";

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

		std::string &code = _blocks.at(_current_block);

		switch (loop_flow)
		{
		case 1:
			code += "\tbreak;\n";
			break;
		case 2: // Keep track of continue target block, so we can insert its code here later
			code += "__CONTINUE__" + std::to_string(target) + "\tcontinue;\n";
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

		_blocks.at(0) += "{\n" + _blocks.at(_last_block) + "}\n";
	}
};

codegen *reshadefx::create_codegen_hlsl(unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants)
{
	return new codegen_hlsl(shader_model, debug_info, uniforms_to_spec_constants);
}
