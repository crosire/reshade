#pragma once

#include <memory>
#include "lexer.hpp"
#include "syntax_tree.hpp"

#pragma region Forward Declaration
namespace reshade
{
	namespace fx
	{
		class symbol_table;
	}
}
#pragma endregion

namespace reshade
{
	namespace fx
	{
		class parser
		{
			parser(const parser &) = delete;
			parser &operator=(const parser &) = delete;

		public:
			parser(const std::string &input, syntax_tree &ast, std::string &errors);
			~parser();

			bool run();

		private:
			void error(const location &location, unsigned int code, const std::string &message);
			void warning(const location &location, unsigned int code, const std::string &message);

			void backup();
			void restore();

			bool peek(char tok) const;
			bool peek(lexer::tokenid tokid) const;
			bool peek_multary_op(enum nodes::binary_expression_node::op &op, unsigned int &precedence) const;
			void consume();
			void consume_until(char token);
			void consume_until(lexer::tokenid tokid);
			bool accept(char tok);
			bool accept(lexer::tokenid tokid);
			bool accept_type_class(nodes::type_node &type);
			bool accept_type_qualifiers(nodes::type_node &type);
			bool accept_unary_op(enum nodes::unary_expression_node::op &op);
			bool accept_postfix_op(enum nodes::unary_expression_node::op &op);
			bool accept_assignment_op(enum nodes::assignment_expression_node::op &op);
			bool expect(char tok);
			bool expect(lexer::tokenid tokid);

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
			bool parse_annotations(std::vector<nodes::annotation_node> &annotations);
			bool parse_struct(nodes::struct_declaration_node *&structure);
			bool parse_function_residue(nodes::type_node &type, std::string name, nodes::function_declaration_node *&function);
			bool parse_variable_residue(nodes::type_node &type, std::string name, nodes::variable_declaration_node *&variable, bool global = false);
			bool parse_variable_assignment(nodes::expression_node *&expression);
			bool parse_variable_properties(nodes::variable_declaration_node *variable);
			bool parse_variable_properties_expression(nodes::expression_node *&expression);
			bool parse_technique(nodes::technique_declaration_node *&technique);
			bool parse_technique_pass(nodes::pass_declaration_node *&pass);
			bool parse_technique_pass_expression(nodes::expression_node *&expression);

			syntax_tree &_ast;
			std::string &_errors;
			std::unique_ptr<lexer> _lexer, _lexer_backup;
			lexer::token _token, _token_next, _token_backup;
			std::unique_ptr<symbol_table> _symbol_table;
		};
	}
}
