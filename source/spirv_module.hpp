/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "spirv_helpers.hpp"
#include "spirv_instruction.hpp"

namespace reshadefx
{
	class spirv_module
	{
	public:
		void write_module(std::ostream &stream);

	protected:
		spv::Id make_id() { return _next_id++; }

		spv_instruction &add_node(spv_basic_block &section, const location &loc, spv::Op op, spv::Id type = 0);
		spv_instruction &add_node_without_result(spv_basic_block &section, const location &loc, spv::Op op);

		spv::Id define_struct(const location &loc, const std::vector<spv::Id> &members);
		spv::Id define_variable(const location &loc, const spv_type &type, spv::StorageClass storage, spv::Id initializer = 0);
		spv::Id define_parameter(const location &loc, const spv_type &type);
		spv::Id define_function(const location &loc, const spv_type &return_type);

		void add_name(spv::Id object, const char *name);
		void add_builtin(spv::Id object, spv::BuiltIn builtin);
		void add_decoration(spv::Id object, spv::Decoration decoration);
		void add_member_name(spv::Id object, uint32_t member_index, const char *name);
		void add_member_builtin(spv::Id object, uint32_t member_index, spv::BuiltIn builtin);
		void add_member_decoration(spv::Id object, uint32_t member_index, spv::Decoration decoration);
		void add_entry_point(const char *name, spv::Id function, spv::ExecutionModel model, const std::vector<spv::Id> &io);

		void add_cast_operation(spv_expression &chain, const spv_type &in_type);
		void add_static_index_access(spv_expression &chain, size_t index);
		void add_dynamic_index_access(spv_expression &chain, spv::Id index_expression);
		void add_swizzle_access(spv_expression &chain, signed char in_swizzle[4], size_t length);

		void enter_block(spv_basic_block &section, spv::Id id);
		void leave_block_and_kill(spv_basic_block &section);
		void leave_block_and_return(spv_basic_block &section, spv::Id value);
		void leave_block_and_branch(spv_basic_block &section, spv::Id target);
		void leave_block_and_branch_conditional(spv_basic_block &section, spv::Id condition, spv::Id true_target, spv::Id false_target);
		void leave_function();

		spv::Id convert_type(const spv_type &info);
		spv::Id convert_type(const spv_type &return_info, const std::vector<spv_type> &param_types);

		spv::Id convert_constant(const spv_type &type, const spv_constant &data);

		spv::Id access_chain_load(spv_basic_block &section, const spv_expression &chain);
		void    access_chain_store(spv_basic_block &section, const spv_expression &chain, spv::Id value, const spv_type &value_type);

		const uint32_t glsl_ext = 1;

		struct function_info2
		{
			spv_instruction declaration;
			spv_basic_block variables;
			spv_basic_block definition;
			spv_type return_type;
			std::vector<spv_type> param_types;
		};

		std::vector<function_info2> _functions2;

		spv::Id _current_block = 0;
		size_t  _current_function = std::numeric_limits<size_t>::max();

	private:
		spv::Id _next_id = 10;

		spv_basic_block _entries;
		spv_basic_block _debug_a;
		spv_basic_block _debug_b;
		spv_basic_block _annotations;
		spv_basic_block _types;
		spv_basic_block _constants;
		spv_basic_block _variables;

		std::vector<std::pair<spv_type, spv::Id>> _type_lookup;
		std::vector<std::tuple<spv_type, spv_constant, spv::Id>> _constant_lookup;
	};
}
