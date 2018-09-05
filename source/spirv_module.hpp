/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "spirv_helpers.hpp"
#include "spirv_instruction.hpp"
#include <unordered_set>

namespace reshadefx
{
	struct spirv_struct_member_info
	{
		spirv_type type;
		std::string name;
		spv::BuiltIn builtin = spv::BuiltInMax;
		unsigned int semantic_index = 0;
		std::vector<spv::Decoration> decorations;
	};

	struct spirv_struct_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		std::vector<spirv_struct_member_info> member_list;
	};

	struct spirv_function_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		spirv_type return_type;
		spv::BuiltIn return_builtin = spv::BuiltInMax;
		unsigned int return_semantic_index = 0;
		std::vector<spirv_struct_member_info> parameter_list;
		spv::Id entry_point = 0;
	};

	struct spirv_variable_info
	{
		spv::Id definition;
		std::string name;
		std::string unique_name;
		std::string semantic;
		spv::BuiltIn builtin = spv::BuiltInMax;
		unsigned int semantic_index = 0;
		spv::Id texture;
		unsigned int width = 1, height = 1, levels = 1;
		bool srgb_texture;
		unsigned int format = 0;
		unsigned int filter = 0;
		unsigned int address_u = 0;
		unsigned int address_v = 0;
		unsigned int address_w = 0;
		float min_lod, max_lod = FLT_MAX, lod_bias;
	};

	struct spirv_pass_info
	{
		spv::Id render_targets[8] = {};
		std::string vs_entry_point, ps_entry_point;
		bool clear_render_targets = true, srgb_write_enable, blend_enable, stencil_enable;
		unsigned char color_write_mask = 0xF, stencil_read_mask = 0xFF, stencil_write_mask = 0xFF;
		unsigned int blend_op = 1, blend_op_alpha = 1, src_blend = 1, dest_blend = 0, src_blend_alpha = 1, dest_blend_alpha = 0;
		unsigned int stencil_comparison_func = 1, stencil_reference_value, stencil_op_pass = 1, stencil_op_fail = 1, stencil_op_depth_fail = 1;
	};

	class spirv_module
	{
	public:
		const uint32_t glsl_ext = 1;

		struct function_info2
		{
			spirv_instruction declaration;
			spirv_basic_block variables;
			spirv_basic_block definition;
			spirv_type return_type;
			std::vector<spirv_type> param_types;
		};

		void write_module(std::ostream &stream);

		spirv_instruction &add_intruction(spirv_basic_block &section, const location &loc, spv::Op op, spv::Id type = 0);
		spirv_instruction &add_instruction_without_result(spirv_basic_block &section, const location &loc, spv::Op op);

		spv::Id convert_type(const spirv_type &info);
		spv::Id convert_type(const function_info2 &info);

		spv::Id convert_constant(const spirv_type &type, const spirv_constant &data);

		void add_capability(spv::Capability capability);

	protected:
		spv::Id make_id() { return _next_id++; }

		void define_struct(spv::Id id, const char *name, const location &loc, const std::vector<spv::Id> &members);
		void define_function(spv::Id id, const char *name, const location &loc, const spirv_type &ret_type);
		void define_function_param(spv::Id id, const char *name, const location &loc, const spirv_type &type);
		void define_variable(spv::Id id, const char *name, const location &loc, const spirv_type &type, spv::StorageClass storage, spv::Id initializer = 0);

		void add_name(spv::Id object, const char *name);
		void add_builtin(spv::Id object, spv::BuiltIn builtin);
		void add_decoration(spv::Id object, spv::Decoration decoration, const char *string);
		void add_decoration(spv::Id object, spv::Decoration decoration, std::initializer_list<uint32_t> values = {});
		void add_member_name(spv::Id object, uint32_t member_index, const char *name);
		void add_member_builtin(spv::Id object, uint32_t member_index, spv::BuiltIn builtin);
		void add_member_decoration(spv::Id object, uint32_t member_index, spv::Decoration decoration, const char *string);
		void add_member_decoration(spv::Id object, uint32_t member_index, spv::Decoration decoration, std::initializer_list<uint32_t> values = {});
		void add_entry_point(const char *name, spv::Id function, spv::ExecutionModel model, const std::vector<spv::Id> &io);

		void add_cast_operation(spirv_expression &chain, const spirv_type &in_type);
		void add_member_access(spirv_expression &chain, size_t index, const spirv_type &in_type);
		void add_static_index_access(spirv_expression &chain, size_t index);
		void add_dynamic_index_access(spirv_expression &chain, spv::Id index_expression);
		void add_swizzle_access(spirv_expression &chain, signed char in_swizzle[4], size_t length);

		void enter_block(spirv_basic_block &section, spv::Id id);
		void leave_block_and_kill(spirv_basic_block &section);
		void leave_block_and_return(spirv_basic_block &section, spv::Id value);
		void leave_block_and_branch(spirv_basic_block &section, spv::Id target);
		void leave_block_and_branch_conditional(spirv_basic_block &section, spv::Id condition, spv::Id true_target, spv::Id false_target);
		void leave_function();

		spv::Id construct_type(spirv_basic_block &section, const spirv_type &type, std::vector<spirv_expression> &arguments);

		spv::Id access_chain_load(spirv_basic_block &section, const spirv_expression &chain);
		void    access_chain_store(spirv_basic_block &section, const spirv_expression &chain, spv::Id value, const spirv_type &value_type);

		std::vector<function_info2> _functions2;

		spv::Id _current_block = 0;
		size_t  _current_function = std::numeric_limits<size_t>::max();

		std::string _encoded_data;

	private:
		spv::Id _next_id = 10;

		spirv_basic_block _entries;
		spirv_basic_block _debug_a;
		spirv_basic_block _debug_b;
		spirv_basic_block _annotations;
		spirv_basic_block _types_and_constants;
		spirv_basic_block _variables;

		std::unordered_set<spv::Capability> _capabilities;
		std::vector<std::pair<spirv_type, spv::Id>> _type_lookup;
		std::vector<std::pair<function_info2, spv::Id>> _function_type_lookup;
		std::vector<std::tuple<spirv_type, spirv_constant, spv::Id>> _constant_lookup;
	};
}
