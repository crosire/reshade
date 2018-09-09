/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_lexer.hpp"
#include "effect_symbol_table.hpp"
#include "spirv_module.hpp"
#include <memory>

namespace reshadefx
{
	/// <summary>
	/// A parser for the ReShade FX shader language.
	/// </summary>
	class parser : symbol_table, public spirv_module
	{
	public:
		/// <summary>
		/// Parse the provided input string.
		/// </summary>
		/// <param name="source">The string to analyze.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool parse(const std::string &source);

		/// <summary>
		/// Gets the list of error messages.
		/// </summary>
		const std::string &errors() const { return _errors; }

	private:
		void error(const location &location, unsigned int code, const std::string &message);
		void warning(const location &location, unsigned int code, const std::string &message);

		void backup();
		void restore();

		bool peek(char tok) const { return peek(static_cast<tokenid>(tok)); }
		bool peek(tokenid tokid) const;
		void consume();
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		void consume_until(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }
		bool expect(tokenid tokid);

		bool accept_type_class(spirv_type &type);
		bool accept_type_qualifiers(spirv_type &type);

		bool parse_type(spirv_type &type);
		bool parse_array_size(spirv_type &type);

		bool accept_unary_op(spv::Op &op);
		bool accept_postfix_op(const spirv_type &type, spv::Op &op);
		bool peek_multary_op(spv::Op &op, unsigned int &precedence) const;
		bool accept_assignment_op(const spirv_type &type, spv::Op &op);

		bool parse_expression(spirv_basic_block &block, spirv_expression &expression);
		bool parse_expression_unary(spirv_basic_block &block, spirv_expression &expression);
		bool parse_expression_multary(spirv_basic_block &block, spirv_expression &expression, unsigned int precedence = 0);
		bool parse_expression_assignment(spirv_basic_block &block, spirv_expression &expression);

		bool parse_annotations(std::unordered_map<std::string, std::string> &annotations);

		bool parse_statement(spirv_basic_block &block, bool scoped);
		bool parse_statement_block(spirv_basic_block &block, bool scoped);

		bool parse_top();
		bool parse_struct();
		bool parse_function(spirv_type type, std::string name);
		bool parse_variable(spirv_type type, std::string name, spirv_basic_block &block, bool global = false);
		bool parse_technique();
		bool parse_technique_pass(spirv_pass_info &info);

		std::string _errors;
		std::unique_ptr<lexer> _lexer, _lexer_backup;
		token _token, _token_next, _token_backup;

		std::vector<spv::Id> _loop_break_target_stack;
		std::vector<spv::Id> _loop_continue_target_stack;

		std::unordered_map<spv::Id, spirv_struct_info> _structs;
		std::vector<std::unique_ptr<spirv_function_info>> _functions;
		std::unordered_map<std::string, uint32_t> _semantic_to_location;
		uint32_t _current_sampler_binding = 0;
		uint32_t _current_semantic_location = 10;
		spv::Id _global_ubo_type = 0;
		spv::Id _global_ubo_variable = 0;
		uint32_t _global_ubo_offset = 0;
	};
}
