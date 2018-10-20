/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_types.hpp"
#include "effect_lexer.hpp"
#include <algorithm>

namespace reshadefx
{
	struct module
	{
		std::string hlsl;
		std::vector<uint32_t> spirv;
		std::vector<struct texture_info> textures;
		std::vector<struct sampler_info> samplers;
		std::vector<struct uniform_info> uniforms;
		std::vector<struct technique_info> techniques;
	};

	enum class codegen_backend
	{
		hlsl,
		spirv,
	};

	enum control_mask
	{
		unroll = 0x00000001,
		dont_unroll = 0x00000002,
		flatten = 0x00000004,
		dont_flatten = 0x00000008,
	};

	class codegen abstract
	{
	public:
		using id = unsigned int;

		unsigned int make_id() { return _next_id++; }

		id current_block() const { return _current_block; }
		const struct function_info &current_function() const { return *functions[_current_function]; }
		bool is_in_block() const { return _current_block != 0; }
		bool is_in_function() const { return _current_function != 0xFFFFFFFF; }

		virtual void write_result(module &stream) const = 0;

		virtual   id define_struct(const location &loc, struct struct_info &info) = 0;
		virtual   id define_texture(const location &loc, struct texture_info &info) = 0;
		virtual   id define_sampler(const location &loc, struct sampler_info &info) = 0;
		virtual   id define_uniform(const location &loc, struct uniform_info &info) = 0;
		virtual   id define_variable(const location &loc, const type &type, const char *name, bool global, id initializer_value) = 0;
		virtual   id define_function(const location &loc, struct function_info &info) = 0;
		virtual   id define_parameter(const location &loc, struct struct_member_info &info) = 0;
		virtual   id define_technique(const location &loc, struct technique_info &info) = 0;

		virtual   id create_entry_point(const function_info &function, bool is_ps) = 0;

		virtual   id emit_constant(const type &type, const constant &data) = 0;

		virtual   id emit_unary_op(const location &loc, tokenid op, const type &type, id val) = 0;
		virtual   id emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &type, id lhs, id rhs) = 0;
		          id emit_binary_op(const location &loc, tokenid op, const type &type, id lhs, id rhs) { return emit_binary_op(loc, op, type, type, lhs, rhs); }
		virtual   id emit_ternary_op(const location &loc, tokenid op, const type &type, id condition, id true_value, id false_value) = 0;
		virtual   id emit_phi(const type &type, id lhs_value, id lhs_block, id rhs_value, id rhs_block) = 0;
		virtual   id emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) = 0;
		virtual   id emit_call_intrinsic(const location &loc, id function, const type &res_type, const std::vector<expression> &args) = 0;
		virtual   id emit_construct(const type &type, std::vector<expression> &args) = 0;

		virtual void emit_if(const location &loc, id condition, id prev_block, id true_statement_block, id false_statement_block, id merge_block, unsigned int flags) = 0;
		virtual void emit_loop(const location &loc, id condition, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, id merge_block, unsigned int flags) = 0;
		virtual void emit_switch(const location &loc, id selector_value, id prev_block, id default_label, const std::vector<id> &case_literal_and_labels, id merge_block, unsigned int flags) = 0;

		virtual   id emit_load(const expression &chain) = 0;
		virtual void emit_store(const expression &chain, id value, const type &value_type) = 0;

		virtual void set_block(id id) = 0;
		virtual void enter_block(id id) = 0;
		virtual void leave_block_and_kill() = 0;
		virtual void leave_block_and_return(id value) = 0;
		virtual void leave_block_and_switch(id value) = 0;
		virtual void leave_block_and_branch(id target) = 0;
		virtual void leave_block_and_branch_conditional(id condition, id true_target, id false_target) = 0;

		virtual void enter_function(id id, const type &ret_type) = 0;
		virtual void leave_function() = 0;

		const struct struct_info &find_struct(id id)
		{
			return *std::find_if(structs.begin(), structs.end(), [id](const auto &it) { return it.definition == id; });
		}
		const struct texture_info &find_texture(id id)
		{
			return *std::find_if(textures.begin(), textures.end(),[id](const auto &it) { return it.id == id; });
		}
		struct function_info &find_function(id id)
		{
			return *std::find_if(functions.begin(), functions.end(), [id](const auto &it) { return it->definition == id; })->get();
		}

	protected:
		std::vector<struct struct_info> structs;
		std::vector<struct texture_info> textures;
		std::vector<struct sampler_info> samplers;
		std::vector<struct uniform_info> uniforms;
		std::vector<std::unique_ptr<struct function_info>> functions;
		std::vector<struct technique_info> techniques;

		unsigned int _next_id = 1;
		unsigned int _current_block = 0;
		unsigned int _current_function = 0;
	};
}
