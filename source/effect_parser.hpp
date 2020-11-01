/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_symbol_table.hpp"
#include <memory> // std::unique_ptr

namespace reshadefx
{
	/// <summary>
	/// A parser for the ReShade FX shader language.
	/// </summary>
	class parser : symbol_table
	{
	public:
		// Define constructor explicitly because lexer class is not included here
		parser();
		~parser();

		/// <summary>
		/// Parse the provided input string.
		/// </summary>
		/// <param name="source">The string to analyze.</param>
		/// <param name="backend">The code generation implementation to use.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool parse(std::string source, class codegen *backend);

		/// <summary>
		/// Get the list of error messages.
		/// </summary>
		std::string &errors() { return _errors; }
		const std::string &errors() const { return _errors; }

	private:
		void error(const location &location, unsigned int code, const std::string &message);
		void warning(const location &location, unsigned int code, const std::string &message);

		void backup();
		void restore();

		bool peek(char tok) const { return _token_next.id == static_cast<tokenid>(tok); }
		bool peek(tokenid tokid) const { return _token_next.id == tokid; }
		void consume();
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		void consume_until(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }
		bool expect(tokenid tokid);

		bool accept_symbol(std::string &identifier, scoped_symbol &symbol);
		bool accept_type_class(type &type);
		bool accept_type_qualifiers(type &type);
		bool accept_unary_op();
		bool accept_postfix_op();
		bool peek_multary_op(unsigned int &precedence) const;
		bool accept_assignment_op();

		void parse_top(bool &parse_success);
		bool parse_struct();
		bool parse_function(type type, std::string name);
		bool parse_variable(type type, std::string name, bool global = false);
		bool parse_technique();
		bool parse_technique_pass(pass_info &info);
		bool parse_type(type &type);
		bool parse_array_size(type &type);
		bool parse_expression(expression &expression);
		bool parse_expression_unary(expression &expression);
		bool parse_expression_multary(expression &expression, unsigned int precedence = 0);
		bool parse_expression_assignment(expression &expression);
		bool parse_annotations(std::vector<annotation> &annotations);
		bool parse_statement(bool scoped);
		bool parse_statement_block(bool scoped);

		codegen *_codegen = nullptr;
		std::string _errors;
		token _token, _token_next, _token_backup;
		std::unique_ptr<class lexer> _lexer;
		size_t _lexer_backup_offset = 0;
		std::vector<uint32_t> _loop_break_target_stack;
		std::vector<uint32_t> _loop_continue_target_stack;
		reshadefx::function_info *_current_function = nullptr;
	};
}
