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
	/// A parser for the ReShade FX language.
	/// </summary>
	class parser : symbol_table, spirv_module
	{
	public:
		/// <summary>
		/// Gets the list of error messages.
		/// </summary>
		const std::string &errors() const { return _errors; }

		/// <summary>
		/// Parse the provided input string.
		/// </summary>
		/// <param name="source">The string to analyze.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool run(const std::string &source);

	private:
		void error(const location &location, unsigned int code, const std::string &message);
		void warning(const location &location, unsigned int code, const std::string &message);

		void backup();
		void restore();

		bool peek(char tok) const { return peek(static_cast<tokenid>(tok)); }
		bool peek(tokenid tokid) const;
		bool peek_multary_op(spv::Op &op, unsigned int &precedence) const;
		void consume();
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		void consume_until(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }
		bool expect(tokenid tokid);

		bool accept_type_class(spv_type &type);
		bool accept_type_qualifiers(spv_type &type);

		bool accept_unary_op(spv::Op &op);
		bool accept_postfix_op(const spv_type &type, spv::Op &op);
		bool accept_assignment_op(const spv_type &type, spv::Op &op);

		bool parse_type(spv_type &type);
		bool parse_array_size(spv_type &type);

		bool parse_expression(spv_basic_block &section, struct spv_expression &expression);
		bool parse_expression_unary(spv_basic_block &section, struct spv_expression &expression);
		bool parse_expression_multary(spv_basic_block &section, struct spv_expression &expression, unsigned int precedence = 0);
		bool parse_expression_assignment(spv_basic_block &section, struct spv_expression &expression);
		bool parse_expression_initializer(spv_basic_block &section, struct spv_expression &expression);

		bool parse_top_level();

		bool parse_statement(spv_basic_block &section, bool scoped);
		bool parse_statement_block(spv_basic_block &section, bool scoped);

		bool parse_struct();
		bool parse_function(spv_type type, std::string name);
		bool parse_variable(spv_type type, std::string name, spv_basic_block &section, bool global = false);

		bool parse_variable_properties(spv_variable_info &props);
		bool parse_variable_properties_expression(struct spv_expression &expression);

		bool parse_technique(spv_technique_info &info);
		bool parse_technique_pass(spv_pass_info &info);

		bool parse_annotations(std::unordered_map<std::string, spv_constant> &annotations);

		std::string _errors;
		std::unique_ptr<lexer> _lexer, _lexer_backup;
		token _token, _token_next, _token_backup;

		spv_struct_info _uniforms;
		std::vector<std::unique_ptr<spv_function_info>> _functions;
		std::vector<spv_technique_info> techniques;
		std::unordered_map<spv::Id, spv_struct_info> _structs;

		std::vector<spv::Id> _loop_break_target_stack;
		std::vector<spv::Id> _loop_continue_target_stack;
	};
}
