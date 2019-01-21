/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_lexer.hpp"
#include "effect_codegen.hpp"
#include "effect_symbol_table.hpp"
#include <memory>

namespace reshadefx
{
	/// <summary>
	/// A parser for the ReShade FX shader language.
	/// </summary>
	class parser : symbol_table
	{
	public:
		/// <summary>
		/// Parse the provided input string.
		/// </summary>
		/// <param name="source">The string to analyze.</param>
		/// <param name="backend">The code generation implementation to use.</param>
		/// <param name="shader_model">The shader model version to target for code generation. Can be 30, 40, 41, 50, ...</param>
		/// <param name="debug_info">Whether to append debug information like line directives to the generated code.</param>
		/// <param name="uniforms_to_spec_constants">Whether to convert uniform variables to specialization constants.</param>
		/// <param name="result">A reference to a module that will be filled with the generated code.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool parse(std::string source, codegen::backend backend, unsigned int shader_model, bool debug_info, bool uniforms_to_spec_constants, module &result);

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

		bool peek(char tok) const { return peek(static_cast<tokenid>(tok)); }
		bool peek(tokenid tokid) const;
		void consume();
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		void consume_until(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }
		bool expect(tokenid tokid);

		bool accept_type_class(type &type);
		bool accept_type_qualifiers(type &type);

		bool parse_type(type &type);

		bool parse_array_size(type &type);

		bool accept_unary_op();
		bool accept_postfix_op();
		bool peek_multary_op(unsigned int &precedence) const;
		bool accept_assignment_op();

		bool parse_expression(expression &expression);
		bool parse_expression_unary(expression &expression);
		bool parse_expression_multary(expression &expression, unsigned int precedence = 0);
		bool parse_expression_assignment(expression &expression);

		bool parse_annotations(std::unordered_map<std::string, std::pair<type, constant>> &annotations);

		bool parse_statement(bool scoped);
		bool parse_statement_block(bool scoped);

		bool parse_top();
		bool parse_struct();
		bool parse_function(type type, std::string name);
		bool parse_variable(type type, std::string name, bool global = false);
		bool parse_technique();
		bool parse_technique_pass(pass_info &info);

		std::string _errors;
		token _token, _token_next, _token_backup;
		std::unique_ptr<lexer> _lexer, _lexer_backup;
		std::unique_ptr<codegen> _codegen;

		std::vector<codegen::id> _loop_break_target_stack;
		std::vector<codegen::id> _loop_continue_target_stack;
		type _current_return_type;
	};
}
