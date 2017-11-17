/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <memory>
#include "effect_lexer.hpp"
#include "effect_syntax_tree.hpp"

namespace reshadefx
{
	/// <summary>
	/// A parser for the ReShade FX language.
	/// </summary>
	class parser
	{
	public:
		/// <summary>
		/// Construct a new parser instance.
		/// </summary>
		explicit parser(syntax_tree &ast);
		parser(const parser &) = delete;
		~parser();

		parser &operator=(const parser &) = delete;

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

		bool peek(tokenid tokid) const;
		bool peek(char tok) const { return peek(static_cast<tokenid>(tok)); }
		bool peek_multary_op(enum nodes::binary_expression_node::op &op, unsigned int &precedence) const;
		void consume();
		void consume_until(tokenid tokid);
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept_type_class(nodes::type_node &type);
		bool accept_type_qualifiers(nodes::type_node &type);
		bool accept_unary_op(enum nodes::unary_expression_node::op &op);
		bool accept_postfix_op(enum nodes::unary_expression_node::op &op);
		bool accept_assignment_op(enum nodes::assignment_expression_node::op &op);
		bool expect(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }

		bool parse_top_level();
		bool parse_namespace();
		bool parse_type(nodes::type_node &type);
		bool parse_expression(nodes::expression_node *&node);
		bool parse_expression_unary(nodes::expression_node *&node);
		bool parse_expression_multary(nodes::expression_node *&left, unsigned int left_precedence = 0);
		bool parse_expression_assignment(nodes::expression_node *&left);
		bool parse_statement(nodes::statement_node *&statement, bool scoped = true);
		bool parse_statement_block(nodes::statement_node *&statement, bool scoped = true);
		bool parse_statement_declarator_list(nodes::statement_node *&statement);
		bool parse_array(int &size);
		bool parse_annotations(std::unordered_map<std::string, reshade::variant> &annotations);
		bool parse_struct(nodes::struct_declaration_node *&structure);
		bool parse_function_declaration(nodes::type_node &type, std::string name, nodes::function_declaration_node *&function);
		bool parse_variable_declaration(nodes::type_node &type, std::string name, nodes::variable_declaration_node *&variable, bool global = false);
		bool parse_variable_assignment(nodes::expression_node *&expression);
		bool parse_variable_properties(nodes::variable_declaration_node *variable);
		bool parse_variable_properties_expression(nodes::expression_node *&expression);
		bool parse_technique(nodes::technique_declaration_node *&technique);
		bool parse_technique_pass(nodes::pass_declaration_node *&pass);
		bool parse_technique_pass_expression(nodes::expression_node *&expression);

		syntax_tree &_ast;
		std::string _errors;
		std::unique_ptr<lexer> _lexer, _lexer_backup;
		token _token, _token_next, _token_backup;
		std::unique_ptr<class symbol_table> _symbol_table;
	};
}
