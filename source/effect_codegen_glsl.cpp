/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <assert.h>
#include <unordered_set>

using namespace reshadefx;

class codegen_glsl final : public codegen
{
public:
	codegen_glsl(bool debug_info, bool uniforms_to_spec_constants)
		: _debug_info(debug_info), _uniforms_to_spec_constants(uniforms_to_spec_constants)
	{
		// Create default block and reserve a memory block to avoid frequent reallocations
		std::string &block = _blocks.emplace(0, std::string()).first->second;
		block.reserve(8192);
	}

private:
	id _next_id = 1;
	id _last_block = 0;
	id _current_block = 0;
	std::string _ubo_block;
	std::unordered_map<id, std::string> _names;
	std::unordered_map<id, std::string> _blocks;
	bool _debug_info = false;
	bool _uniforms_to_spec_constants = false;
	unsigned int _current_ubo_offset = 0;
	unsigned int _current_sampler_binding = 0;
	std::unordered_map<id, id> _remapped_sampler_variables;

	void write_result(module &module) override
	{
		module = std::move(_module);

		module.hlsl +=
			"float hlsl_fmod(float x, float y) { return x - y * trunc(x / y); }\n"
			" vec2 hlsl_fmod( vec2 x,  vec2 y) { return x - y * trunc(x / y); }\n"
			" vec3 hlsl_fmod( vec3 x,  vec3 y) { return x - y * trunc(x / y); }\n"
			" vec4 hlsl_fmod( vec4 x,  vec4 y) { return x - y * trunc(x / y); }\n"
			" mat2 hlsl_fmod( mat2 x,  mat2 y) { return x - matrixCompMult(y, mat2(trunc(x[0] / y[0]), trunc(x[1] / y[1]))); }\n"
			" mat3 hlsl_fmod( mat3 x,  mat3 y) { return x - matrixCompMult(y, mat3(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]))); }\n"
			" mat4 hlsl_fmod( mat4 x,  mat4 y) { return x - matrixCompMult(y, mat4(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]), trunc(x[3] / y[3]))); }\n";

		if (!_ubo_block.empty())
			module.hlsl += "layout(std140, binding = 0) uniform _Globals {\n" + _ubo_block + "};\n";
		module.hlsl += _blocks.at(0);
	}

	template <bool is_param = false, bool is_decl = true, bool is_interface = false>
	void write_type(std::string &s, const type &type) const
	{
		if constexpr (is_decl)
		{
			if (type.has(type::q_precise))
				s += "precise ";
		}

		if constexpr (is_interface)
		{
			if (type.has(type::q_linear))
				s += "smooth ";
			if (type.has(type::q_noperspective))
				s += "noperspective ";
			if (type.has(type::q_centroid))
				s += "centroid ";
			if (type.has(type::q_nointerpolation))
				s += "flat ";
		}

		if constexpr (is_interface || is_param)
		{
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
			if (type.cols > 1)
				s += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
			else if (type.rows > 1)
				s += "bvec" + std::to_string(type.rows);
			else
				s += "bool";
			break;
		case type::t_int:
			if (type.cols > 1)
				s += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
			else if (type.rows > 1)
				s += "ivec" + std::to_string(type.rows);
			else
				s += "int";
			break;
		case type::t_uint:
			if (type.cols > 1)
				s += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
			else if (type.rows > 1)
				s += "uvec" + std::to_string(type.rows);
			else
				s += "uint";
			break;
		case type::t_float:
			if (type.cols > 1)
				s += "mat" + std::to_string(type.rows) + 'x' + std::to_string(type.cols);
			else if (type.rows > 1)
				s += "vec" + std::to_string(type.rows);
			else
				s += "float";
			break;
		case type::t_struct:
			s += id_to_name(type.definition);
			break;
		case type::t_sampler:
			s += "sampler2D";
			break;
		default:
			assert(false);
		}
	}
	void write_constant(std::string &s, const type &type, const constant &data) const
	{
		if (type.is_array())
		{
			auto elem_type = type;
			elem_type.array_length = 0;

			write_type<false, false>(s, elem_type);
			s += "[](";

			for (int i = 0; i < type.array_length; ++i)
			{
				write_constant(s, elem_type, i < static_cast<int>(data.array_data.size()) ? data.array_data[i] : constant());

				if (i < type.array_length - 1)
					s += ", ";
			}

			s += ')';
			return;
		}

		assert(type.is_numeric());

		if (!type.is_scalar())
		{
			if (type.is_matrix())
				s += "transpose(";

			write_type<false, false>(s, type);
		}

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
				s += std::to_string(data.as_uint[i]) + 'u';
				break;
			case type::t_float:
				std::string temp(_scprintf("%.8f", data.as_float[i]), '\0');
				sprintf_s(temp.data(), temp.size() + 1, "%.8f", data.as_float[i]);
				s += temp;
				break;
			}

			if (i < components - 1)
				s += ", ";
		}

		if (!type.is_scalar() && type.is_matrix())
		{
			s += ')';
		}

		s += ')';
	}
	void write_location(std::string &s, const location &loc) const
	{
		if (loc.source.empty() || !_debug_info)
			return;

		s += "#line " + std::to_string(loc.line) + '\n';
	}

	id make_id() { return _next_id++; }

	std::string id_to_name(id id) const
	{
		if (const auto it = _remapped_sampler_variables.find(id); it != _remapped_sampler_variables.end())
			id = it->second;
		assert(id != 0);
		if (const auto it = _names.find(id); it != _names.end())
			return it->second;
		return '_' + std::to_string(id);
	}

	static void escape_name(std::string &name)
	{
		static const std::unordered_set<std::string> s_reserverd_names = {
			"common", "partition", "input", "ouput", "active", "filter", "superp", "invariant",
			"lowp", "mediump", "highp", "precision", "patch", "subroutine",
			"abs", "sign", "all", "any", "sin", "sinh", "cos", "cosh", "tan", "tanh", "asin", "acos", "atan",
			"exp", "exp2", "log", "log2", "sqrt", "inversesqrt", "ceil", "floor", "fract", "trunc", "round",
			"radians", "degrees", "length", "normalize", "transpose", "determinant", "intBitsToFloat", "uintBitsToFloat",
			"floatBitsToInt", "floatBitsToUint", "matrixCompMult", "not", "lessThan", "greaterThan", "lessThanEqual",
			"greaterThanEqual", "equal", "notEqual", "dot", "cross", "distance", "pow", "modf", "frexp", "ldexp",
			"min", "max", "step", "reflect", "texture", "textureOffset", "fma", "mix", "clamp", "smoothstep", "refract",
			"faceforward", "textureLod", "textureLodOffset", "texelFetch", "main"
		};

		if (name.compare(0, 3, "gl_") == 0 || s_reserverd_names.count(name))
			name += '_';
		for (size_t pos = 0; (pos = name.find("__", pos)) != std::string::npos; pos += 3)
			name.replace(pos, 2, "_US");
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
		escape_name(_names[info.definition] = info.unique_name);

		_structs.push_back(info);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += "struct " + id_to_name(info.definition) + "\n{\n";

		for (const auto &member : info.member_list)
		{
			code += '\t';
			write_type(code, member.type); // GLSL does not allow interpolation attributes on struct members
			code += ' ' + member.name;
			if (member.type.is_array())
				code += '[' + std::to_string(member.type.array_length) + ']';
			code += ";\n";
		}

		if (info.member_list.empty())
			code += "float _dummy;\n";

		code += "};\n";

		return info.definition;
	}
	id   define_texture(const location &, texture_info &info) override
	{
		info.id = make_id();

		_module.textures.push_back(info);

		return info.id;
	}
	id   define_sampler(const location &loc, sampler_info &info) override
	{
		info.id = make_id();
		info.binding = _current_sampler_binding++;

		escape_name(_names[info.id] = info.unique_name);

		_module.samplers.push_back(info);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += "layout(binding = " + std::to_string(info.binding) + ") uniform sampler2D " + id_to_name(info.id) + ";\n";

		return info.id;
	}
	id   define_uniform(const location &loc, uniform_info &info) override
	{
		const id res = make_id();

		_names[res] = "_Globals_" + info.name;

		if (_uniforms_to_spec_constants && info.type.is_scalar() && info.has_initializer_value)
		{
			std::string &code = _blocks.at(_current_block);

			write_location(code, loc);

			code += "const ";
			write_type(code, info.type);
			code += ' ' + id_to_name(res) + " = ";
			write_type<false, false>(code, info.type);
			code += "(SPEC_CONSTANT_" + info.name + ");\n";

			_module.spec_constants.push_back(info);
		}
		else
		{
			// GLSL specification on std140 layout:
			// 1. If the member is a scalar consuming N basic machine units, the base alignment is N.
			// 2. If the member is a two- or four-component vector with components consuming N basic machine units, the base alignment is 2N or 4N, respectively.
			// 3. If the member is a three-component vector with components consuming N basic machine units, the base alignment is 4N.
			const unsigned int size = 4 * info.type.rows * info.type.cols * std::max(1, info.type.array_length);
			const unsigned int alignment = 4 * (info.type.rows == 3 ? 4 : info.type.rows) * info.type.cols * std::max(1, info.type.array_length);

			info.size = size;
			info.offset = (_current_ubo_offset % alignment != 0) ? _current_ubo_offset + alignment - _current_ubo_offset % alignment : _current_ubo_offset;
			_current_ubo_offset = info.offset + info.size;

			write_location(_ubo_block, loc);

			_ubo_block += '\t';
			write_type(_ubo_block, info.type);
			_ubo_block += ' ' + id_to_name(res) + ";\n";

			_module.uniforms.push_back(info);
		}

		return res;
	}
	id   define_variable(const location &loc, const type &type, std::string name, bool global, id initializer_value) override
	{
		const id res = make_id();

		// GLSL does not allow local sampler variables, so try to remap those
		if (!global && type.is_sampler())
			return (_remapped_sampler_variables[res] = 0), res;

		if (!name.empty())
			escape_name(_names[res] = name);

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
		return define_function(loc, info, false);
	}
	id   define_function(const location &loc, function_info &info, bool is_entry_point)
	{
		info.definition = make_id();

		if (is_entry_point)
			_names[info.definition] = "main";
		else
			escape_name(_names[info.definition] = info.unique_name);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		write_type(code, info.return_type);
		code += ' ' + id_to_name(info.definition) + '(';

		assert(info.parameter_list.empty() || !is_entry_point);

		for (size_t i = 0, num_params = info.parameter_list.size(); i < num_params; ++i)
		{
			auto &param = info.parameter_list[i];

			param.definition = make_id();
			_names[param.definition] = param.name;

			code += '\n';
			write_location(code, param.location);
			code += '\t';
			write_type<true>(code, param.type); // GLSL does not allow interpolation attributes on function parameters
			code += ' ' + param.name;

			if (param.type.is_array())
				code += '[' + std::to_string(param.type.array_length) + ']';

			if (i < num_params - 1)
				code += ',';
		}

		code += ")\n";

		_functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}

	id   create_block() override
	{
		const id res = make_id();

		std::string &block = _blocks.emplace(res, std::string()).first->second;
		// Reserve a decently big enough memory block to avoid frequent reallocations
		block.reserve(4096);

		return res;
	}

	void create_entry_point(const function_info &func, bool is_ps) override
	{
		if (const auto it = std::find_if(_module.entry_points.begin(), _module.entry_points.end(),
			[&func](const auto &ep) { return ep.first == func.unique_name; }); it != _module.entry_points.end())
			return;

		_module.entry_points.push_back({ func.unique_name, is_ps });

		_blocks.at(0) += "#ifdef ENTRY_POINT_" + func.unique_name + '\n';

		function_info entry_point;
		entry_point.return_type = { type::t_void };

		const auto is_color_semantic = [](const std::string &semantic) { return semantic.compare(0, 9, "SV_TARGET") == 0 || semantic.compare(0, 5, "COLOR") == 0; };

		const auto escape_name_with_builtins = [this, is_ps](std::string name, const std::string &semantic) -> std::string
		{
			if (semantic == "SV_VERTEXID" || semantic == "VERTEXID")
				return "gl_VertexID";
			else if (semantic == "SV_POSITION" || semantic == "POSITION" || semantic == "VPOS")
				return is_ps ? "gl_FragCoord" : "gl_Position";
			else if (semantic == "SV_DEPTH" || semantic == "DEPTH")
				return "gl_FragDepth";

			escape_name(name);
			return name;
		};
		const auto visit_shader_param = [this, is_ps, &escape_name_with_builtins](type type, unsigned int quals, const std::string &name, const std::string &semantic) {
			type.qualifiers = quals;

			// OpenGL does not allow varying of type boolean
			if (type.base == type::t_bool)
				type.base  = type::t_float;

			std::string &code = _blocks.at(_current_block);

			unsigned long location = 0;

			for (int i = 0, array_length = std::max(1, type.array_length); i < array_length; ++i)
			{
				if (!escape_name_with_builtins(std::string(), semantic).empty())
					continue;
				else if (semantic.compare(0, 5, "COLOR") == 0)
					location = strtoul(semantic.c_str() + 5, nullptr, 10);
				else if (semantic.compare(0, 8, "TEXCOORD") == 0)
					location = strtoul(semantic.c_str() + 8, nullptr, 10) + 1;
				else if (semantic.compare(0, 9, "SV_TARGET") == 0)
					location = strtoul(semantic.c_str() + 9, nullptr, 10);

				code += "layout(location = " + std::to_string(location + i) + ") ";
				write_type<false, false, true>(code, type);
				code += ' ' + name;
				if (type.is_array())
					code += std::to_string(i);
				code += ";\n";
			}
		};

		// Translate function parameters to input/output variables
		if (func.return_type.is_struct())
			for (const auto &member : find_struct(func.return_type.definition).member_list)
				visit_shader_param(member.type, type::q_out, "_return_" + member.name, member.semantic);
		else if (!func.return_type.is_void())
			visit_shader_param(func.return_type, type::q_out, "_return", func.return_semantic);

		for (const auto &param : func.parameter_list)
			if (param.type.is_struct())
				for (const auto &member : find_struct(param.type.definition).member_list)
					visit_shader_param(member.type, param.type.qualifiers | member.type.qualifiers, "_param_" + param.name + '_' + member.name, member.semantic);
			else
				visit_shader_param(param.type, param.type.qualifiers, "_param_" + param.name, param.semantic);

		define_function({}, entry_point, true);
		enter_block(create_block());

		std::string &code = _blocks.at(_current_block);

		// Handle input parameters
		for (const auto &param : func.parameter_list)
		{
			for (int i = 0, array_length = std::max(1, param.type.array_length); i < array_length; i++)
			{
				// Build struct from separate member input variables
				if (param.type.is_struct())
				{
					code += '\t';
					write_type<false, true>(code, param.type);
					code += " _param_" + param.name;
					if (param.type.is_array())
						code += std::to_string(i);
					code += " = ";
					write_type<false, false>(code, param.type);
					code += '(';

					for (const auto &member : find_struct(param.type.definition).member_list)
						code += escape_name_with_builtins("_param_" + param.name + '_' + member.name + (param.type.is_array() ? std::to_string(i) : std::string()), member.semantic) + ", ";

					code.pop_back();
					code.pop_back();

					code += ");\n";
				}
			}

			if (param.type.is_array())
			{
				code += '\t';
				write_type<false, true>(code, param.type);
				code += " _param_" + param.name + "[] = ";
				write_type<false, false>(code, param.type);
				code += "[](";

				for (int i = 0; i < param.type.array_length; ++i)
				{
					code += "_param_" + param.name + std::to_string(i);
					if (i < param.type.array_length - 1)
						code += ", ";
				}

				code += ");\n";
			}
		}

		code += '\t';
		// Structs cannot be output variables, so have to write to a temporary first and then output each member separately
		if (func.return_type.is_struct())
			write_type(code, func.return_type), code += " _return = ";
		// All other output types can write to the output variable directly
		else if (!func.return_type.is_void())
			code += "_return = ";

		// Call the function this entry point refers to
		code += id_to_name(func.definition) + '(';

		for (size_t i = 0, num_params = func.parameter_list.size(); i < num_params; ++i)
		{
			code += escape_name_with_builtins("_param_" + func.parameter_list[i].name, func.parameter_list[i].semantic);

			if (i < num_params - 1)
				code += ", ";
		}

		code += ");\n";

		// Handle output parameters
		for (const auto &param : func.parameter_list)
		{
			if (!param.type.has(type::q_out))
				continue;

			for (int i = 0; i < param.type.array_length; i++)
				code += "\t_param_" + param.name + std::to_string(i) + " = _param_" + param.name + '[' + std::to_string(i) + "];\n";

			for (int i = 0; i < std::max(1, param.type.array_length); i++)
				if (param.type.is_struct())
					for (const auto &member : find_struct(param.type.definition).member_list)
						if (param.type.is_array())
							code += "\t_param_" + param.name + '_' + member.name + std::to_string(i) + " = _param_" + param.name + '.' + member.name + '[' + std::to_string(i) + "];\n";
						else
							code += "\t_param_" + param.name + '_' + member.name + " = _param_" + param.name + '.' + member.name + ";\n";
		}

		// Handle return struct output variables
		if (func.return_type.is_struct())
			for (const auto &member : find_struct(func.return_type.definition).member_list)
				code += '\t' + escape_name_with_builtins("_return_" + member.name, member.semantic) + " = _return." + member.name + ";\n";

		leave_block_and_return(0);
		leave_function();

		_blocks.at(0) += "#endif\n";
	}

	id   emit_load(const expression &exp) override
	{
		if (exp.is_constant)
			return emit_constant(exp.type, exp.constant);
		else if (exp.chain.empty()) // Can refer to values without access chain directly
			return exp.base;

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, exp.location);

		code += '\t';
		write_type(code, exp.type);
		code += ' ' + id_to_name(res);

		if (exp.type.is_array())
			code += '[' + std::to_string(exp.type.array_length) + ']';

		code += " = ";

		std::string newcode = id_to_name(exp.base);

		for (const auto &op : exp.chain)
		{
			switch (op.op)
			{
			case expression::operation::op_cast:
				{ std::string type; write_type<false, false>(type, op.to);
				newcode = type + '(' + newcode + ')'; }
				break;
			case expression::operation::op_index:
				newcode += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_member:
				newcode += '.';
				newcode += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_swizzle:
				if (op.from.is_matrix())
				{
					if (op.swizzle[1] < 0)
					{
						const unsigned int row = op.swizzle[0] % 4;
						const unsigned int col = (op.swizzle[0] - row) / 4;

						newcode += '[' + std::to_string(row) + "][" + std::to_string(col) + ']';
					}
					else
					{
						assert(false);
					}
				}
				else
				{
					newcode += '.';
					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						newcode += "xyzw"[op.swizzle[i]];
				}
				break;
			}
		}

		code += newcode;
		code += ";\n";

		return res;
	}
	void emit_store(const expression &exp, id value) override
	{
		if (const auto it = _remapped_sampler_variables.find(exp.base); it != _remapped_sampler_variables.end())
		{
			assert(it->second == 0);
			it->second = value;
			return;
		}

		std::string &code = _blocks.at(_current_block);

		write_location(code, exp.location);

		code += '\t' + id_to_name(exp.base);

		for (const auto &op : exp.chain)
		{
			switch (op.op)
			{
			case expression::operation::op_index:
				code += '[' + id_to_name(op.index) + ']';
				break;
			case expression::operation::op_member:
				code += '.';
				code += find_struct(op.from.definition).member_list[op.index].name;
				break;
			case expression::operation::op_swizzle:
				if (op.from.is_matrix())
				{
					if (op.swizzle[1] < 0)
					{
						const unsigned int row = op.swizzle[0] % 4;
						const unsigned int col = (op.swizzle[0] - row) / 4;

						code += '[' + std::to_string(row) + "][" + std::to_string(col) + ']';
					}
					else
					{
						assert(false);
					}
				}
				else
				{
					code += '.';
					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						code += "xyzw"[op.swizzle[i]];
				}
				break;
			}
		}

		code += " = " + id_to_name(value) + ";\n";
	}

	id   emit_constant(const type &type, const constant &data) override
	{
		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		// Struct initialization is not supported right now
		if (type.is_struct())
		{
			code += '\t';
			write_type(code, type);
			code += ' ' + id_to_name(res) + ";\n";
			return res;
		}

		code += "\tconst ";
		write_type(code, type);
		code += ' ' + id_to_name(res);

		if (type.is_array())
			code += '[' + std::to_string(type.array_length) + ']';

		code += " = ";
		write_constant(code, type, data);
		code += ";\n";

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

		switch (op)
		{
		case tokenid::minus:
			code += '-';
			break;
		case tokenid::tilde:
			code += '~';
			break;
		case tokenid::exclaim:
			if (res_type.is_vector())
				code += "not";
			else
				code += "!bool";
			break;
		default:
			assert(false);
		}

		code += '(' + id_to_name(val) + ");\n";

		return res;
	}
	id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &type, id lhs, id rhs) override
	{
		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, res_type);
		code += ' ' + id_to_name(res) + " = ";

		std::string intrinsic, operator_code;

		switch (op)
		{
		case tokenid::plus:
		case tokenid::plus_plus:
		case tokenid::plus_equal:
			operator_code = '+';
			break;
		case tokenid::minus:
		case tokenid::minus_minus:
		case tokenid::minus_equal:
			operator_code = '-';
			break;
		case tokenid::star:
		case tokenid::star_equal:
			if (type.is_matrix())
				intrinsic = "matrixCompMult";
			else
				operator_code = '*';
			break;
		case tokenid::slash:
		case tokenid::slash_equal:
			operator_code = '/';
			break;
		case tokenid::percent:
		case tokenid::percent_equal:
			if (type.is_floating_point())
				intrinsic = "hlsl_fmod";
			else
				operator_code = '%';
			break;
		case tokenid::caret:
		case tokenid::caret_equal:
			operator_code = '^';
			break;
		case tokenid::pipe:
		case tokenid::pipe_equal:
			operator_code = '|';
			break;
		case tokenid::ampersand:
		case tokenid::ampersand_equal:
			operator_code = '&';
			break;
		case tokenid::less_less:
		case tokenid::less_less_equal:
			operator_code = "<<";
			break;
		case tokenid::greater_greater:
		case tokenid::greater_greater_equal:
			operator_code = ">>";
			break;
		case tokenid::pipe_pipe:
			operator_code = "||";
			break;
		case tokenid::ampersand_ampersand:
			operator_code = "&&";
			break;
		case tokenid::less:
			if (type.is_vector())
				intrinsic = "lessThan";
			else
				operator_code = '<';
			break;
		case tokenid::less_equal:
			if (type.is_vector())
				intrinsic = "lessThanEqual";
			else
				operator_code = "<=";
			break;
		case tokenid::greater:
			if (type.is_vector())
				intrinsic = "greaterThan";
			else
				operator_code = '>';
			break;
		case tokenid::greater_equal:
			if (type.is_vector())
				intrinsic = "greaterThanEqual";
			else
				operator_code = ">=";
			break;
		case tokenid::equal_equal:
			if (type.is_vector())
				intrinsic = "equal";
			else
				operator_code = "==";
			break;
		case tokenid::exclaim_equal:
			if (type.is_vector())
				intrinsic = "notEqual";
			else
				operator_code = "!=";
			break;
		default:
			assert(false);
		}

		if (!intrinsic.empty())
			code += intrinsic + '(' + id_to_name(lhs) + ", " + id_to_name(rhs) + ')';
		else
			code += id_to_name(lhs) + ' ' + operator_code + ' ' + id_to_name(rhs);

		code += ";\n";

		return res;
	}
	id   emit_ternary_op(const location &loc, tokenid op, const type &res_type, id condition, id true_value, id false_value) override
	{
		assert(op == tokenid::question);

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, res_type);
		code += ' ' + id_to_name(res);

		if (res_type.is_array())
			code += '[' + std::to_string(res_type.array_length) + ']';

		code += " = ";

		// GLSL requires the selection first expression to be a scalar boolean
		if (!res_type.is_scalar())
			code += "all";

		code += '(' + id_to_name(condition) + ") ? " + id_to_name(true_value) + " : " + id_to_name(false_value) + ";\n";

		return res;
	}
	id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);

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
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';

		if (!res_type.is_void())
		{
			write_type(code, res_type);
			code += ' ' + id_to_name(res) + " = ";
		}

		enum
		{
#define IMPLEMENT_INTRINSIC_GLSL(name, i, code) name##i,
#include "effect_symbol_table_intrinsics.inl"
		};

		switch (intrinsic)
		{
#define IMPLEMENT_INTRINSIC_GLSL(name, i, code) case name##i: code break;
#include "effect_symbol_table_intrinsics.inl"
		default:
			assert(false);
		}

		code += ";\n";

		return res;
	}
	id   emit_construct(const location &loc, const type &type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert((arg.type.is_scalar() || type.is_array()) && arg.chain.empty() && arg.base != 0);

		const id res = make_id();

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += '\t';
		write_type(code, type);
		code += ' ' + id_to_name(res);

		if (type.is_array())
			code += '[' + std::to_string(type.array_length) + ']';

		code += " = ";

		if (!type.is_array() && type.is_matrix())
			code += "transpose(";

		write_type<false, false>(code, type);

		if (type.is_array())
			code += "[]";

		code += '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code += id_to_name(args[i].base);

			if (i < num_args - 1)
				code += ", ";
		}

		if (!type.is_array() && type.is_matrix())
			code += ')';

		code += ");\n";

		return res;
	}

	void emit_if(const location &loc, id condition_value, id condition_block, id true_statement_block, id false_statement_block, unsigned int) override
	{
		assert(condition_value != 0 && condition_block != 0 && true_statement_block != 0 && false_statement_block != 0);

		std::string &code = _blocks.at(_current_block);

		std::string &true_statement_data = _blocks.at(true_statement_block);
		std::string &false_statement_data = _blocks.at(false_statement_block);

		increase_indentation_level(true_statement_data);
		increase_indentation_level(false_statement_data);

		code += _blocks.at(condition_block);

		write_location(code, loc);

		code += "\tif (" + id_to_name(condition_value) + ")\n\t{\n";
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
	void emit_loop(const location &loc, id condition_value, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, unsigned int) override
	{
		assert(condition_value != 0 && prev_block != 0 && header_block != 0 && loop_block != 0 && continue_block != 0);

		std::string &code = _blocks.at(_current_block);

		std::string &loop_data = _blocks.at(loop_block);
		std::string &continue_data = _blocks.at(continue_block);

		increase_indentation_level(loop_data);
		increase_indentation_level(loop_data);
		increase_indentation_level(continue_data);

		code += _blocks.at(prev_block);

		if (condition_block == 0)
			code += "\tbool " + id_to_name(condition_value) + ";\n";
		else
			code += _blocks.at(condition_block);

		write_location(code, loc);

		code += '\t';

		if (condition_block == 0)
		{
			// Convert variable initializer to assignment statement
			auto pos_assign = continue_data.rfind(id_to_name(condition_value));
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
			code += "\t}\n\twhile (" + id_to_name(condition_value) + ");\n";
		}
		else
		{
			std::string &condition_data = _blocks.at(condition_block);

			increase_indentation_level(condition_data);

			// Convert variable initializer to assignment statement
			auto pos_assign = condition_data.rfind(id_to_name(condition_value));
			auto pos_prev_assign = condition_data.rfind('\t', pos_assign);
			condition_data.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign - 1);

			const std::string continue_id = "__CONTINUE__" + std::to_string(continue_block);
			for (size_t offset = 0; (offset = loop_data.find(continue_id, offset)) != std::string::npos; offset += continue_data.size())
				loop_data.replace(offset, continue_id.size(), continue_data + condition_data);

			code += "while (" + id_to_name(condition_value) + ")\n\t{\n\t\t{\n";
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
	void emit_switch(const location &loc, id selector_value, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, unsigned int) override
	{
		assert(selector_value != 0 && selector_block != 0 && default_label != 0);

		std::string &code = _blocks.at(_current_block);

		code += _blocks.at(selector_block);

		write_location(code, loc);

		code += "\tswitch (" + id_to_name(selector_value) + ")\n\t{\n";

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

		// Remove consumed blocks to save memory
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

		std::string &code = _blocks.at(_current_block);

		code += "\tdiscard;\n";

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

codegen *create_codegen_glsl(bool debug_info, bool uniforms_to_spec_constants)
{
	return new codegen_glsl(debug_info, uniforms_to_spec_constants);
}
