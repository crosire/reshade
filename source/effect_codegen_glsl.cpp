/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <cmath> // signbit, isinf, isnan
#include <cstdio> // snprintf
#include <cassert>
#include <algorithm> // std::max
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
	enum class naming
	{
		// After escaping, name should already be unique, so no additional steps are taken
		unique,
		// After escaping, will be numbered when clashing with another name
		general,
		// This is a special name that is not modified and should be unique
		reserved,
		// Replace name with a code snippet
		expression,
	};

	std::string _ubo_block;
	std::unordered_map<id, std::string> _names;
	std::unordered_map<id, std::string> _blocks;
	bool _debug_info = false;
	bool _uniforms_to_spec_constants = false;
	std::unordered_map<id, id> _remapped_sampler_variables;
	std::unordered_map<std::string, uint32_t> _semantic_to_location;

	// Only write compatibility intrinsics to result if they are actually in use
	bool _uses_fmod = false;
	bool _uses_componentwise_or = false;
	bool _uses_componentwise_and = false;
	bool _uses_componentwise_cond = false;

	void write_result(module &module) override
	{
		module = std::move(_module);

		if (_uses_fmod)
			module.hlsl += "float fmodHLSL(float x, float y) { return x - y * trunc(x / y); }\n"
				"vec2 fmodHLSL(vec2 x, vec2 y) { return x - y * trunc(x / y); }\n"
				"vec3 fmodHLSL(vec3 x, vec3 y) { return x - y * trunc(x / y); }\n"
				"vec4 fmodHLSL(vec4 x, vec4 y) { return x - y * trunc(x / y); }\n"
				"mat2 fmodHLSL(mat2 x, mat2 y) { return x - matrixCompMult(y, mat2(trunc(x[0] / y[0]), trunc(x[1] / y[1]))); }\n"
				"mat3 fmodHLSL(mat3 x, mat3 y) { return x - matrixCompMult(y, mat3(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]))); }\n"
				"mat4 fmodHLSL(mat4 x, mat4 y) { return x - matrixCompMult(y, mat4(trunc(x[0] / y[0]), trunc(x[1] / y[1]), trunc(x[2] / y[2]), trunc(x[3] / y[3]))); }\n";
		if (_uses_componentwise_or)
			module.hlsl +=
				"bvec2 compOr(bvec2 a, bvec2 b) { return bvec2(a.x || b.x, a.y || b.y); }\n"
				"bvec3 compOr(bvec3 a, bvec3 b) { return bvec3(a.x || b.x, a.y || b.y, a.z || b.z); }\n"
				"bvec4 compOr(bvec4 a, bvec4 b) { return bvec4(a.x || b.x, a.y || b.y, a.z || b.z, a.w || b.w); }\n";
		if (_uses_componentwise_and)
			module.hlsl +=
				"bvec2 compAnd(bvec2 a, bvec2 b) { return bvec2(a.x && b.x, a.y && b.y); }\n"
				"bvec3 compAnd(bvec3 a, bvec3 b) { return bvec3(a.x && b.x, a.y && b.y, a.z && b.z); }\n"
				"bvec4 compAnd(bvec4 a, bvec4 b) { return bvec4(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w); }\n";
		if (_uses_componentwise_cond)
			module.hlsl +=
				"vec2 compCond(bvec2 cond, vec2 a, vec2 b) { return vec2(cond.x ? a.x : b.x, cond.y ? a.y : b.y); }\n"
				"vec3 compCond(bvec3 cond, vec3 a, vec3 b) { return vec3(cond.x ? a.x : b.x, cond.y ? a.y : b.y, cond.z ? a.z : b.z); }\n"
				"vec4 compCond(bvec4 cond, vec4 a, vec4 b) { return vec4(cond.x ? a.x : b.x, cond.y ? a.y : b.y, cond.z ? a.z : b.z, cond.w ? a.w : b.w); }\n";

		if (!_ubo_block.empty())
			// Read matrices in column major layout, even though they are actually row major, to avoid transposing them on every access (since GLSL uses column matrices)
			// TODO: This technically only works with square matrices
			module.hlsl += "layout(std140, column_major, binding = 0) uniform _Globals {\n" + _ubo_block + "};\n";
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
			s += '[' + std::to_string(type.array_length) + "](";

			for (int i = 0; i < type.array_length; ++i)
			{
				write_constant(s, elem_type, i < static_cast<int>(data.array_data.size()) ? data.array_data[i] : constant());

				if (i < type.array_length - 1)
					s += ", ";
			}

			s += ')';
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
				s += std::to_string(data.as_uint[i]) + 'u';
				break;
			case type::t_float:
				if (std::isnan(data.as_float[i])) {
					s += "0.0/0.0/*nan*/";
					break;
				}
				if (std::isinf(data.as_float[i])) {
					s += std::signbit(data.as_float[i]) ? "1.0/0.0/*inf*/" : "-1.0/0.0/*-inf*/";
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
	void write_location(std::string &s, const location &loc) const
	{
		if (loc.source.empty() || !_debug_info)
			return;

		s += "#line " + std::to_string(loc.line) + '\n';
	}

	std::string id_to_name(id id) const
	{
		if (const auto it = _remapped_sampler_variables.find(id); it != _remapped_sampler_variables.end())
			id = it->second;
		assert(id != 0);
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
		if constexpr (naming_type != naming::reserved)
			name = escape_name(std::move(name));
		if constexpr (naming_type == naming::general)
			if (std::find_if(_names.begin(), _names.end(), [&name](const auto &it) { return it.second == name; }) != _names.end())
				name += '_' + std::to_string(id); // Append a numbered suffix if the name already exists
		_names[id] = std::move(name);
	}

	static std::string escape_name(std::string name)
	{
		static const std::unordered_set<std::string> s_reserverd_names = {
			"common", "partition", "input", "output", "active", "filter", "superp", "invariant",
			"lowp", "mediump", "highp", "precision", "patch", "subroutine",
			"abs", "sign", "all", "any", "sin", "sinh", "cos", "cosh", "tan", "tanh", "asin", "acos", "atan",
			"exp", "exp2", "log", "log2", "sqrt", "inversesqrt", "ceil", "floor", "fract", "trunc", "round",
			"radians", "degrees", "length", "normalize", "transpose", "determinant", "intBitsToFloat", "uintBitsToFloat",
			"floatBitsToInt", "floatBitsToUint", "matrixCompMult", "not", "lessThan", "greaterThan", "lessThanEqual",
			"greaterThanEqual", "equal", "notEqual", "dot", "cross", "distance", "pow", "modf", "frexp", "ldexp",
			"min", "max", "step", "reflect", "texture", "textureOffset", "fma", "mix", "clamp", "smoothstep", "refract",
			"faceforward", "textureLod", "textureLodOffset", "texelFetch", "main"
		};

		// Append something to reserved names so that they do not fail to compile
		if (name.compare(0, 3, "gl_") == 0 || s_reserverd_names.count(name))
			name += "_RESERVED"; // Do not append an underscore at the end, since another one may get added in 'define_name'

		// Remove double underscore symbols from name which can occur due to namespaces but are not allowed in GLSL
		for (size_t pos = 0; (pos = name.find("__", pos)) != std::string::npos; pos += 3)
			name.replace(pos, 2, "_UNDERSCORE");

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
			write_type(code, member.type); // GLSL does not allow interpolation attributes on struct members
			code += ' ';
			code += escape_name(member.name);
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
		info.binding = _module.num_sampler_bindings++;

		define_name<naming::unique>(info.id, info.unique_name);

		_module.samplers.push_back(info);

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		code += "layout(binding = " + std::to_string(info.binding) + ") uniform sampler2D " + id_to_name(info.id) + ";\n";

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

			code += "const ";
			write_type(code, info.type);
			code += ' ' + id_to_name(res) + " = ";
			if (!info.type.is_scalar())
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
			// 4. If the member is an array of scalars or vectors, the base alignment and array stride are set to match the base alignment of a single array element,
			//    according to rules (1), (2), and (3), and rounded up to the base alignment of a four-component vector.
			// 7. If the member is a row-major matrix with C columns and R rows, the matrix is stored identically to an array of R row vectors with C components each, according to rule (4).
			// 8. If the member is an array of S row-major matrices with C columns and R rows, the matrix is stored identically to a row of S*R row vectors with C components each, according to rule (4).
			const uint32_t alignment = info.type.is_array() || info.type.is_matrix() ? 16 /* (4) */ : (info.type.rows == 3 ? 4 /* (3) */ : info.type.rows /* (2)*/) * 4 /* (1)*/;
			if (info.type.is_matrix())
				info.size = std::max(info.type.array_length, 1) * info.type.rows * alignment /* (7), (8) */;
			else if (info.type.is_array())
				info.size = info.type.array_length * alignment;
			else
				info.size = info.type.rows * 4;

			// Adjust offset according to alignment rules from above
			info.offset = _module.total_uniform_size;
			info.offset = align_up(info.offset, alignment);
			_module.total_uniform_size = info.offset + info.size;

			write_location(_ubo_block, loc);

			_ubo_block += '\t';
			// Note: All matrices are floating-point, even if the uniform type says different!!
			write_type(_ubo_block, info.type);
			_ubo_block += ' ' + id_to_name(res);

			if (info.type.is_array())
				_ubo_block += '[' + std::to_string(info.type.array_length) + ']';

			_ubo_block += ";\n";

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
		return define_function(loc, info, false);
	}
	id   define_function(const location &loc, function_info &info, bool is_entry_point)
	{
		info.definition = make_id();

		// Name is used in other places like the "ENTRY_POINT" defines, so escape it here
		info.unique_name = escape_name(info.unique_name);

		if (!is_entry_point)
			define_name<naming::unique>(info.definition, info.unique_name);
		else
			define_name<naming::reserved>(info.definition, "main");

		std::string &code = _blocks.at(_current_block);

		write_location(code, loc);

		write_type(code, info.return_type);
		code += ' ' + id_to_name(info.definition) + '(';

		assert(info.parameter_list.empty() || !is_entry_point);

		for (size_t i = 0, num_params = info.parameter_list.size(); i < num_params; ++i)
		{
			auto &param = info.parameter_list[i];

			param.definition = make_id();
			define_name<naming::unique>(param.definition, param.name);

			code += '\n';
			write_location(code, param.location);
			code += '\t';
			write_type<true>(code, param.type); // GLSL does not allow interpolation attributes on function parameters
			code += ' ' + id_to_name(param.definition);

			if (param.type.is_array())
				code += '[' + std::to_string(param.type.array_length) + ']';

			if (i < num_params - 1)
				code += ',';
		}

		code += ")\n";

		_functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}

	void define_entry_point(const function_info &func, bool is_ps) override
	{
		if (const auto it = std::find_if(_module.entry_points.begin(), _module.entry_points.end(),
			[&func](const auto &ep) { return ep.name == func.unique_name; }); it != _module.entry_points.end())
			return;

		_module.entry_points.push_back({ func.unique_name, is_ps });

		_blocks.at(0) += "#ifdef ENTRY_POINT_" + func.unique_name + '\n';
		if (is_ps)
			_blocks.at(0) += "layout(origin_upper_left) in vec4 gl_FragCoord;\n";

		function_info entry_point;
		entry_point.return_type = { type::t_void };

		const auto semantic_to_builtin = [this, is_ps](std::string name, const std::string &semantic) -> std::string
		{
			if (semantic == "SV_POSITION" || semantic == "POSITION" || semantic == "VPOS")
				return is_ps ? "gl_FragCoord" : "gl_Position";
			if (semantic == "SV_DEPTH" || semantic == "DEPTH")
				return "gl_FragDepth";
			if (semantic == "SV_VERTEXID")
				return "gl_VertexID";
			return escape_name(name);
		};

		const auto create_varying_variable = [this, is_ps, &semantic_to_builtin](type type, unsigned int quals, const std::string &name, std::string semantic) {
			type.qualifiers = quals;

			// OpenGL does not allow varying of type boolean
			if (type.base == type::t_bool)
				type.base  = type::t_float;

			std::string &code = _blocks.at(_current_block);

			for (int i = 0, array_length = std::max(1, type.array_length); i < array_length; ++i)
			{
				if (!semantic_to_builtin({}, semantic).empty())
					continue; // Skip built in variables

				if (const char c = semantic.back(); c < '0' || c > '9')
					semantic += '0'; // Always numerate semantics, so that e.g. TEXCOORD and TEXCOORD0 point to the same location

				uint32_t location = 0;
				if (semantic.compare(0, 9, "SV_TARGET") == 0)
					location = std::strtoul(semantic.c_str() + 9, nullptr, 10);
				else if (semantic.compare(0, 5, "COLOR") == 0)
					location = std::strtoul(semantic.c_str() + 5, nullptr, 10);
				else if (const auto it = _semantic_to_location.find(semantic); it != _semantic_to_location.end())
					location = it->second;
				else
					_semantic_to_location[semantic] = location = static_cast<uint32_t>(_semantic_to_location.size());

				code += "layout(location = " + std::to_string(location + i) + ") ";
				write_type<false, false, true>(code, type);
				code += ' ';
				if (type.is_array())
					code += escape_name(name + '_' + std::to_string(i));
				else
					code += escape_name(name);
				code += ";\n";
			}
		};

		// Translate function parameters to input/output variables
		if (func.return_type.is_struct())
			for (const struct_member_info &member : find_struct(func.return_type.definition).member_list)
				create_varying_variable(member.type, type::q_out, "_return_" + member.name, member.semantic);
		else if (!func.return_type.is_void())
			create_varying_variable(func.return_type, type::q_out, "_return", func.return_semantic);

		size_t num_params = func.parameter_list.size();
		for (size_t i = 0; i < num_params; ++i)
		{
			const type &param_type = func.parameter_list[i].type;
			const std::string param_name = "_param" + std::to_string(i);

			// Flatten structure parameters
			if (param_type.is_struct())
				for (const struct_member_info &member : find_struct(param_type.definition).member_list)
					create_varying_variable(member.type, param_type.qualifiers | member.type.qualifiers, param_name + '_' + member.name, member.semantic);
			else
				create_varying_variable(param_type, param_type.qualifiers, param_name, func.parameter_list[i].semantic);
		}

		define_function({}, entry_point, true);
		enter_block(create_block());

		std::string &code = _blocks.at(_current_block);

		// Handle input parameters
		for (size_t i = 0; i < num_params; ++i)
		{
			const type &param_type = func.parameter_list[i].type;
			const std::string param_name = "_param" + std::to_string(i);

			for (int a = 0, array_length = std::max(1, param_type.array_length); a < array_length; a++)
			{
				// Build struct from separate member input variables
				if (param_type.is_struct())
				{
					code += '\t';
					write_type<false, true>(code, param_type);
					code += ' ';
					if (param_type.is_array())
						code += escape_name(param_name + '_' + std::to_string(a));
					else
						code += escape_name(param_name);
					code += " = ";
					write_type<false, false>(code, param_type);
					code += '(';

					const struct_info &definition = find_struct(param_type.definition);

					for (const struct_member_info &member : definition.member_list)
					{
						if (param_type.is_array())
							code += semantic_to_builtin(param_name + '_' + member.name + '_' + std::to_string(a), member.semantic);
						else
							code += semantic_to_builtin(param_name + '_' + member.name, member.semantic);

						code += ", ";
					}

					// There can be no empty structs, so can assume that the last two characters are always ", "
					code.pop_back();
					code.pop_back();

					code += ");\n";
				}
			}

			if (param_type.is_array())
			{
				code += '\t';
				write_type<false, true>(code, param_type);
				code += ' ';
				code += escape_name(param_name);
				code += "[] = ";
				write_type<false, false>(code, param_type);
				code += "[](";

				for (int a = 0; a < param_type.array_length; ++a)
				{
					code += escape_name(param_name + '_' + std::to_string(a));

					if (a < param_type.array_length - 1)
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
			code += semantic_to_builtin("_return", func.return_semantic) + " = ";

		// Call the function this entry point refers to
		code += id_to_name(func.definition) + '(';

		for (size_t i = 0; i < num_params; ++i)
		{
			code += semantic_to_builtin("_param" + std::to_string(i), func.parameter_list[i].semantic);

			if (i < num_params - 1)
				code += ", ";
		}

		code += ");\n";

		// Handle output parameters
		for (size_t i = 0; i < num_params; ++i)
		{
			const type &param_type = func.parameter_list[i].type;
			if (!param_type.has(type::q_out))
				continue;

			const std::string param_name = "_param" + std::to_string(i);

			for (int a = 0; a < param_type.array_length; a++)
			{
				code += '\t';
				code += escape_name(param_name + '_' + std::to_string(a));
				code += " = ";
				code += escape_name(param_name);
				code += '[' + std::to_string(a) + "];\n";
			}

			for (int a = 0; a < std::max(1, param_type.array_length); a++)
			{
				if (!param_type.is_struct())
					continue;

				const struct_info &definition = find_struct(param_type.definition);

				for (const struct_member_info &member : definition.member_list)
				{
					code += '\t';
					if (param_type.is_array())
						code += escape_name(param_name + '_' + member.name + '_' + std::to_string(a));
					else
						code += escape_name(param_name + '_' + member.name);
					code += " = ";
					code += escape_name(param_name);
					code += '.';
					code += escape_name(member.name);
					if (param_type.is_array())
						code += '[' + std::to_string(a) + ']';
					code += ";\n";
				}
			}
		}

		// Handle return struct output variables
		if (func.return_type.is_struct())
		{
			const struct_info &definition = find_struct(func.return_type.definition);

			for (const struct_member_info &member : definition.member_list)
			{
				code += '\t';
				code += semantic_to_builtin("_return_" + member.name, member.semantic);
				code += " = _return." + escape_name(member.name) + ";\n";
			}
		}

		leave_block_and_return(0);
		leave_function();

		_blocks.at(0) += "#endif\n";
	}

	id   emit_load(const expression &exp, bool force_new_id) override
	{
		if (exp.is_constant)
			return emit_constant(exp.type, exp.constant);
		else if (exp.chain.empty() && !force_new_id) // Can refer to values without access chain directly
			return exp.base;

		const id res = make_id();

		std::string type, expr_code = id_to_name(exp.base);

		for (const auto &op : exp.chain)
		{
			switch (op.op)
			{
			case expression::operation::op_cast:
				type.clear();
				write_type<false, false>(type, op.to);
				expr_code = type + '(' + expr_code + ')';
				break;
			case expression::operation::op_member:
				expr_code += '.';
				expr_code += escape_name(find_struct(op.from.definition).member_list[op.index].name);
				break;
			case expression::operation::op_dynamic_index:
				// For matrices this will extract a column, but that is fine, since they are initialized column-wise too
				// Also cast to an integer, since it could be a boolean too, but GLSL does not allow those in index expressions
				expr_code += "[int(" + id_to_name(op.index) + ")]";
				break;
			case expression::operation::op_constant_index:
				if (op.from.is_vector() && !op.from.is_array())
					expr_code += '.',
					expr_code += "xyzw"[op.index];
				else
					expr_code += '[' + std::to_string(op.index) + ']';
				break;
			case expression::operation::op_swizzle:
				if (op.from.is_matrix())
				{
					if (op.swizzle[1] < 0)
					{
						const int row = (op.swizzle[0] % 4);
						const int col = (op.swizzle[0] - row) / 4;

						expr_code += '[' + std::to_string(row) + "][" + std::to_string(col) + ']';
					}
					else
					{
						// TODO: Implement matrix to vector swizzles
						assert(false);
						expr_code += "_NOT_IMPLEMENTED_"; // Make sure compilation fails
					}
				}
				else
				{
					expr_code += '.';
					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						expr_code += "xyzw"[op.swizzle[i]];
				}
				break;
			}
		}

		// GLSL matrices are always floating point, so need to cast result to the target type
		if (!exp.chain.empty() && exp.chain[0].from.is_matrix() && !exp.chain[0].from.is_floating_point())
		{
			type.clear();
			write_type<false, false>(type, exp.type);
			expr_code = type + '(' + expr_code + ')';
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
		if (const auto it = _remapped_sampler_variables.find(exp.base);
			it != _remapped_sampler_variables.end())
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
			case expression::operation::op_member:
				code += '.';
				code += escape_name(find_struct(op.from.definition).member_list[op.index].name);
				break;
			case expression::operation::op_dynamic_index:
				code += "[int(" + id_to_name(op.index) + ")]";
				break;
			case expression::operation::op_constant_index:
				code += '[' + std::to_string(op.index) + ']';
				break;
			case expression::operation::op_swizzle:
				if (op.from.is_matrix())
				{
					if (op.swizzle[1] < 0)
					{
						const int row = (op.swizzle[0] % 4);
						const int col = (op.swizzle[0] - row) / 4;

						code += '[' + std::to_string(row) + "][" + std::to_string(col) + ']';
					}
					else
					{
						// TODO: Implement matrix to vector swizzles
						assert(false);
						code += "_NOT_IMPLEMENTED_"; // Make sure compilation fails
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

		code += " = ";

		// GLSL matrices are always floating point, so need to cast type
		if (!exp.chain.empty() && exp.chain[0].from.is_matrix() && !exp.chain[0].from.is_floating_point())
			// Only supporting scalar assignments to matrices currently, so can assume to always cast to float
			code += "float(" + id_to_name(value) + ");\n";
		else
			code += id_to_name(value) + ";\n";
	}

	id   emit_constant(const type &type, const constant &data) override
	{
		const id res = make_id();

		if (type.is_array() || type.is_struct())
		{
			std::string &code = _blocks.at(_current_block);

			code += '\t';

			// GLSL requires constants to be initialized
			if (!type.is_struct())
				code += "const ";

			write_type(code, type);
			code += ' ' + id_to_name(res);

			// Array constants need to be stored in a constant variable as they cannot be used in-place
			if (type.is_array())
				code += '[' + std::to_string(type.array_length) + ']';

			// Struct initialization is not supported right now
			if (!type.is_struct()) {
				code += " = ";
				write_constant(code, type, data);
			}

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
				intrinsic = "fmodHLSL",
				_uses_fmod = true;
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
			if (type.is_vector())
				intrinsic = "compOr",
				_uses_componentwise_or = true;
			else
				operator_code = "||";
			break;
		case tokenid::ampersand_ampersand:
			if (type.is_vector())
				intrinsic = "compAnd",
				_uses_componentwise_and = true;
			else
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

		code += " = ";

		if (res_type.is_vector())
			code += "compCond(" + id_to_name(condition) + ", " + id_to_name(true_value) + ", " + id_to_name(false_value) + ");\n",
			_uses_componentwise_cond = true;
		else // GLSL requires the conditional expression to be a scalar boolean
			code += id_to_name(condition) + " ? " + id_to_name(true_value) + " : " + id_to_name(false_value) + ";\n";

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

		write_type<false, false>(code, type);

		if (type.is_array())
			code += '[' + std::to_string(type.array_length) + ']';

		code += '(';

		for (size_t i = 0, num_args = args.size(); i < num_args; ++i)
		{
			code += id_to_name(args[i].base);

			if (i < num_args - 1)
				code += ", ";
		}

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
			// Add a return statement to exit functions in case discard is the last control flow statement
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

codegen *reshadefx::create_codegen_glsl(bool debug_info, bool uniforms_to_spec_constants)
{
	return new codegen_glsl(debug_info, uniforms_to_spec_constants);
}
