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
	id _last_block = 0;
	std::unordered_map<id, std::string> _names;
	std::unordered_map<id, std::string> _blocks;
	unsigned int _scope_level = 0;

	inline std::string &code() { return _blocks[_current_block]; }

	void write_result(module &s) const override
	{
		s.hlsl = "struct __sampler { Texture2D t; SamplerState s; };\nint2 __tex2Dsize(__sampler s, int lod) { uint w, h, l; s.t.GetDimensions(lod, w, h, l); return int2(w, h); }\n" + _blocks.at(0);

		s.samplers = samplers;
		s.textures = textures;
		s.uniforms = uniforms;
		s.techniques = techniques;
	}

	std::string write_type(const type &type)
	{
		std::string s;
		switch (type.base)
		{
		case type::t_void:
			s += "void"; break;
		case type::t_bool:
			s += "bool"; break;
		case type::t_int:
			s += "int"; break;
		case type::t_uint:
			s += "uint"; break;
		case type::t_float:
			s += "float"; break;
		case type::t_sampler:
			s += "__sampler"; break;
		}

		if (type.rows > 1)
			s += std::to_string(type.rows);
		if (type.cols > 1)
			s += 'x' + std::to_string(type.cols);
		return s;
	}
	std::string write_constant(const type &type, const constant &data)
	{
		std::string s;

		if (type.is_array())
		{
			s += "{ ";

			struct type elem_type = type;
			elem_type.array_length = 0;

			for (size_t i = 0; i < data.array_data.size(); ++i)
				s += write_constant(elem_type, data.array_data[i]) + ", ";

			s += " }";
			return s;
		}

		if (!type.is_scalar())
			s += write_type(type);

		s += '(';

		for (unsigned int c = 0; c < type.cols; ++c)
		{
			for (unsigned int r = 0; r < type.rows; ++r, s += ", ")
			{
				switch (type.base)
				{
				case type::t_int:
					s += std::to_string(data.as_int[c * type.rows + r]);
					break;
				case type::t_uint:
					s += std::to_string(data.as_uint[c * type.rows + r]);
					break;
				case type::t_float:
					s += std::to_string(data.as_float[c * type.rows + r]);
					break;
				}
			}
		}

		if (s.back() == ' ')
		{
			s.pop_back();
			s.pop_back();
		}

		s += ')';
		return s;
	}
	std::string write_scope()
	{
		return std::string(_scope_level, '\t');
	}
	std::string write_location(const location &loc)
	{
		if (loc.source.empty())
			return std::string();

		return "#line " + std::to_string(loc.line) + " \"" + loc.source + "\"\n";
	}

	inline std::string id_to_name(id id) const
	{
		if (const auto it = _names.find(id); it != _names.end())
			return it->second;
		return '_' + std::to_string(id);
	}

	id define_struct(const location &loc, struct_info &info) override
	{
		structs.push_back(info);

		code() += write_location(loc);
		code() += "struct " + info.unique_name + "\n{\n";

		for (const auto &member : info.member_list)
		{
			code() += write_type(member.type) + ' ' + member.name;
			if (!member.semantic.empty())
				code() += ':' + member.semantic;
			code() += ';';
		}

		code() += "\n};\n";

		return info.definition;
	}
	id define_texture(const location &, texture_info &info) override
	{
		textures.push_back(info);

		return info.id;
	}
	id define_sampler(const location &loc, sampler_info &info) override
	{
		samplers.push_back(info);

		code() += "Texture2D " + info.unique_name + "_t : register(t" + std::to_string(info.binding) + ");\n";
		code() += "SamplerState " + info.unique_name + "_s : register(s" + std::to_string(info.binding) + ");\n";
		code() += write_location(loc);
		code() += "__sampler " + info.unique_name + " = { " + info.unique_name + "_t, " + info.unique_name + "_s };\n";

		_names[info.id] = info.unique_name;

		return info.id;
	}
	id define_uniform(const location &loc, uniform_info &info) override
	{
		info.member_index = uniforms.size();

		uniforms.push_back(info);

		code() += write_location(loc);
		code() += write_type(info.type) + ' ' + info.name + ";\n";

		_names[0xFFFFFFFF] = "_Globals";

		return 0xFFFFFFFF;
	}
	id define_variable(const location &loc, const type &type, const char *name, bool, id initializer_value) override
	{
		id id = make_id();

		if (name != nullptr)
			_names[id] = name;

		code() += write_location(loc);
		code() += write_scope() + write_type(type) + ' ' + id_to_name(id);

		if (initializer_value != 0)
			code() += " = " + id_to_name(initializer_value);

		code() += ";\n";

		return id;
	}
	id define_function(const location &, function_info &info) override
	{
		// Remove last comma from the parameter list
		if (code().back() == ',')
			code().pop_back();

		code() += ')';

		if (!info.return_semantic.empty())
			code() += " : " + info.return_semantic;

		code() += '\n';

		//_names[info.definition] = info.unique_name;

		functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}
	id define_parameter(const location &loc, struct_member_info &info) override
	{
		id id = make_id();

		_names[id] = info.name;

		code() += '\n' + write_location(loc);
		code() += '\t' + write_type(info.type) + ' ' + id_to_name(id);

		if (!info.semantic.empty())
			code() += " : " + info.semantic;

		code() +',';

		return id;
	}
	id define_technique(const location &, technique_info &info) override
	{
		techniques.push_back(info);

		return 0;
	}

	id create_entry_point(const function_info &func, bool is_ps) override
	{
		return func.definition;
	}

	id   emit_load(const expression &chain) override
	{
		id res = make_id();

		code() += write_location(chain.location);
		code() += write_scope() + "const " + write_type(chain.type) + ' ' + id_to_name(res) + " = ";

		if (chain.is_constant)
		{
			code() += write_constant(chain.type, chain.constant);
		}
		else
		{
			std::string newcode = id_to_name(chain.base);

			for (const auto &op : chain.ops)
			{
				switch (op.type)
				{
				case expression::operation::op_cast: {
					newcode = "((" + write_type(op.to) + ')' + newcode + ')';
					break;
				}
				case expression::operation::op_index:
					newcode += '[' + id_to_name(op.index) + ']';
					break;
				case expression::operation::op_swizzle:
					newcode += '.';
					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						newcode += "xyzw"[op.swizzle[i]];
					break;
				}
			}

			code() += newcode;
		}

		code() += ";\n";

		return res;
	}
	void emit_store(const expression &chain, id value, const type &) override
	{
		code() += write_location(chain.location);
		code() += write_scope() + id_to_name(chain.base);

		for (const auto &op : chain.ops)
		{
			switch (op.type)
			{
			case expression::operation::op_index:
				code() += '[' + id_to_name(op.index) + ']';
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
		id id = make_id();

		code() += write_scope() + "const " + write_type(type) + ' ' + id_to_name(id);

		if (type.is_array())
			code() += '[' + std::to_string(type.array_length) + ']';

		code() += " = " + write_constant(type, data) + ";\n";

		return id;
	}

	id   emit_unary_op(const location &loc, tokenid op, const type &type, id val) override
	{
		id res = make_id();
		std::string hlsl_op;

		switch (op)
		{
		case tokenid::exclaim: hlsl_op = "!"; break;
		case tokenid::minus: hlsl_op = "-"; break;
		case tokenid::tilde: hlsl_op = "~"; break;
		case tokenid::plus_plus: hlsl_op = "+ 1"; break;
		case tokenid::minus_minus: hlsl_op = "- 1"; break;
		default:
			return assert(false), 0;
		}

		code() += write_location(loc);
		code() += write_scope() + "const " + write_type(type) + ' ' + id_to_name(res) + " = " + id_to_name(val) + ' ' + hlsl_op + ";\n";

		return res;
	}
	id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &, id lhs, id rhs) override
	{
		id res = make_id();
		std::string hlsl_op;

		switch (op)
		{
		case tokenid::percent:
		case tokenid::percent_equal: hlsl_op = "%"; break;
		case tokenid::ampersand:
		case tokenid::ampersand_equal: hlsl_op = "&"; break;
		case tokenid::star:
		case tokenid::star_equal: hlsl_op = "*"; break;
		case tokenid::plus:
		case tokenid::plus_plus:
		case tokenid::plus_equal: hlsl_op = "+"; break;
		case tokenid::minus:
		case tokenid::minus_minus:
		case tokenid::minus_equal: hlsl_op = "-"; break;
		case tokenid::slash:
		case tokenid::slash_equal: hlsl_op = "/"; break;
		case tokenid::less: hlsl_op = "<"; break;
		case tokenid::greater: hlsl_op = ">"; break;
		case tokenid::caret:
		case tokenid::caret_equal: hlsl_op = "^"; break;
		case tokenid::pipe:
		case tokenid::pipe_equal: hlsl_op = "|"; break;
		case tokenid::exclaim_equal: hlsl_op = "!"; break;
		case tokenid::ampersand_ampersand: hlsl_op = "&&";  break;
		case tokenid::less_less:
		case tokenid::less_less_equal: hlsl_op = "<<"; break;
		case tokenid::less_equal: hlsl_op = "<="; break;
		case tokenid::equal_equal: hlsl_op = "=="; break;
		case tokenid::greater_greater:
		case tokenid::greater_greater_equal: hlsl_op = ">>"; break;
		case tokenid::greater_equal: hlsl_op = ">="; break;
		case tokenid::pipe_pipe: hlsl_op = "||"; break;
		default:
			return assert(false), 0;
		}

		code() += write_location(loc);
		code() += write_scope() + "const " + write_type(res_type) + ' ' + id_to_name(res) + " = " + id_to_name(lhs) + ' ' + hlsl_op + ' ' + id_to_name(rhs) + ";\n";

		return res;
	}
	id   emit_ternary_op(const location &loc, tokenid op, const type &type, id condition, id true_value, id false_value) override
	{
		assert(op == tokenid::question);

		id res = make_id();

		code() += write_location(loc);
		code() += write_scope() + "const " + write_type(type) + ' ' + id_to_name(res) + " = " + id_to_name(condition) + " ? " + id_to_name(true_value) + " : " + id_to_name(false_value) + ";\n";

		return res;
	}
	id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) override
	{
		id res = make_id();

		code() += write_location(loc);
		code() += write_scope() + "const " + write_type(res_type) + ' ' + id_to_name(res) + " = " + id_to_name(function) + "(";

		for (const auto &arg : args)
		{
			code() += id_to_name(arg.base) + ", ";
		}

		if (code().back() == ' ')
		{
			code().pop_back();
			code().pop_back();
		}

		code() += ");\n";

		return res;
	}
	id   emit_call_intrinsic(const location &loc, id intrinsic, const type &res_type, const std::vector<expression> &args) override
	{
		id res = make_id();

		code() += write_location(loc);
		code() += write_scope();

		if (!res_type.is_void())
		{
			code() += "const " + write_type(res_type) + ' ' + id_to_name(res) + " = ";
		}

		enum
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) name##i,
#include "effect_symbol_table_intrinsics.inl"
		};

		switch (intrinsic)
		{
#define IMPLEMENT_INTRINSIC_HLSL(name, i, code) case name##i: code break;
#include "effect_symbol_table_intrinsics.inl"
		}

		code() += ";\n";

		return res;
	}
	id   emit_construct(const type &type, std::vector<expression> &args) override
	{
		id id = make_id();

		code() += write_scope() + "const " + write_type(type) + ' ' + id_to_name(id) + " = " + write_type(type) + '(';

		for (const auto &arg : args)
		{
			code() += arg.is_constant ? write_constant(arg.type, arg.constant) : id_to_name(arg.base);
			code() += ", ";
		}

		if (code().back() == ' ')
		{
			code().pop_back();
			code().pop_back();
		}

		code() += ");\n";

		return id;
	}

	void emit_if(const location &loc, id condition_value, id condition_block, id true_statement_block, id false_statement_block, id merge_block, unsigned int flags) override
	{
		assert(condition_value != 0);
		assert(condition_block != 0);
		assert(true_statement_block != 0);
		assert(false_statement_block != 0);
		assert(merge_block != 0);

		_blocks[merge_block] += _blocks[condition_block];
		_blocks[merge_block] += write_location(loc);

		if (flags & flatten) _blocks[merge_block] += "[flatten]";
		if (flags & dont_flatten) _blocks[merge_block] += "[branch]";

		_blocks[merge_block] +=
			write_scope() + "if (" + id_to_name(condition_value) + ")\n" + write_scope() + "{\n" +
			write_scope() + _blocks[true_statement_block] +
			write_scope() + "}\n" + write_scope() + "else\n" + write_scope () + "{\n" +
			write_scope() + _blocks[false_statement_block] +
			write_scope() + "}\n";
	}
	id   emit_phi(const type &type, id condition_value, id condition_block, id true_value, id true_statement_block, id false_value, id false_statement_block) override
	{
		id res = make_id();

		code() += _blocks[condition_block] +
			write_scope() + write_type(type) + ' ' + id_to_name(res) + ";\n" +
			write_scope() + "if (" + id_to_name(condition_value) + ")\n" + write_scope () + "{\n" +
			(true_statement_block != condition_block ? _blocks[true_statement_block] : std::string()) +
			write_scope() + id_to_name(res) + " = " + id_to_name(true_value) + ";\n" +
			write_scope() + "}\n" + write_scope() + "else\n" + write_scope () + "{\n" +
			(false_statement_block != condition_block ? _blocks[false_statement_block] : std::string()) +
			write_scope() + id_to_name(res) + " = " + id_to_name(false_value) + ";\n" +
			write_scope() + "}\n";

		return res;
	}
	void emit_loop(const location &loc, id condition_value, id prev_block, id, id condition_block, id loop_block, id continue_block, id merge_block, unsigned int flags) override
	{
		assert(condition_value != 0);
		assert(prev_block != 0);
		assert(loop_block != 0);
		assert(continue_block != 0);
		assert(merge_block != 0);

		_blocks[merge_block] += _blocks[prev_block];

		if (condition_block == 0)
		{
			_blocks[merge_block] += write_scope() + "bool " + id_to_name(condition_value) + ";\n";
		}
		else
		{
			_blocks[merge_block] += _blocks[condition_block];
		}

		_blocks[merge_block] += write_location(loc) + write_scope();

		if (flags & unroll) _blocks[merge_block] += "[unroll] ";
		if (flags & dont_unroll) _blocks[merge_block] += "[loop] ";

		if (condition_block == 0)
		{
			std::string loop_condition = _blocks[continue_block];
			auto pos_assign = loop_condition.rfind(id_to_name(condition_value));
			auto pos_prev_assign = loop_condition.rfind('\n', pos_assign);
			loop_condition.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign);

			_blocks[merge_block] += "do\n" + write_scope() + "{\n" + _blocks[loop_block] + loop_condition + "}\n" + write_scope() + "while (" + id_to_name(condition_value) + ");\n";
		}
		else
		{
			std::string loop_condition = _blocks[condition_block];
			auto pos_assign = loop_condition.rfind(id_to_name(condition_value));
			auto pos_prev_assign = loop_condition.rfind('\n', pos_assign);
			loop_condition.erase(pos_prev_assign + 1, pos_assign - pos_prev_assign);

			_blocks[merge_block] += "while (" + id_to_name(condition_value) + ")\n" + write_scope() + "{\n" + _blocks[loop_block] + _blocks[continue_block] + loop_condition + write_scope() + "}\n";
		}
	}
	void emit_switch(const location &loc, id selector_value, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, id merge_block, unsigned int flags) override
	{
		assert(selector_value != 0);
		assert(selector_block != 0);
		assert(default_label != 0);
		assert(merge_block != 0);

		_blocks[merge_block] += _blocks[selector_block];
		_blocks[merge_block] += write_location(loc) + write_scope();

		if (flags & flatten) _blocks[merge_block] += "[flatten]";
		if (flags & dont_flatten) _blocks[merge_block] += "[branch]";

		_blocks[merge_block] += "switch (" + id_to_name(selector_value) + ")\n{\n";

		_scope_level++;

		for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
		{
			_blocks[merge_block] += write_scope() + "case " + std::to_string(case_literal_and_labels[i]) + ": " + _blocks[case_literal_and_labels[i + 1]] + '\n';
		}

		if (default_label != merge_block)
		{
			_blocks[merge_block] += write_scope() + "default: " + _blocks[default_label] + '\n';
		}

		_scope_level--;

		_blocks[merge_block] += write_scope() + "}\n";
	}

	void set_block(id id) override
	{
		_current_block = id;
	}
	void enter_block(id id) override
	{
		_current_block = id;
	}
	id   leave_block_and_kill() override
	{
		if (!is_in_block())
			return 0;

		code() += write_scope() + "discard;\n";

		_last_block = _current_block;
		_current_block = 0;

		return _last_block;
	}
	id   leave_block_and_return(id value) override
	{
		if (!is_in_block())
			return 0;

		code() += write_scope() + "return" + (value ? ' ' + id_to_name(value) : std::string()) + ";\n";

		_last_block = _current_block;
		_current_block = 0;

		return _last_block;
	}
	id   leave_block_and_switch(id) override
	{
		if (!is_in_block())
			return _last_block;

		_last_block = _current_block;
		_current_block = 0;

		return _last_block;
	}
	id   leave_block_and_branch(id, bool is_continue) override
	{
		if (!is_in_block())
			return _last_block;

		if (is_continue)
			code() += write_scope() + "continue;\n";

		_last_block = _current_block;
		_current_block = 0;

		return _last_block;
	}
	id   leave_block_and_branch_conditional(id, id, id) override
	{
		if (!is_in_block())
			return _last_block;

		_last_block = _current_block;
		_current_block = 0;

		return _last_block;
	}

	void enter_function(id id, const type &ret_type) override
	{
		code() += write_type(ret_type) + ' ' + id_to_name(id) + '(';

		_scope_level++;
		_current_function = functions.size();
	}
	void leave_function() override
	{
		code() += "{\n" + _blocks[_last_block] + "}\n";

		assert(_scope_level > 0);
		_scope_level--;
		_current_function = 0xFFFFFFFF;
	}
};

codegen *create_codegen_hlsl()
{
	return new codegen_hlsl();
}
