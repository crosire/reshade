#pragma once

#include "ast.hpp"

namespace reshade
{
	namespace fx
	{
		class compiler
		{
			compiler(const compiler &);
			compiler &operator=(const compiler &);

		public:
			compiler(const nodetree &ast, std::string &errors);

			virtual bool run() = 0;

		protected:
			void error(const location &location, const std::string &message);
			void warning(const location &location, const std::string &message);

			void visit(std::stringstream &output, const nodes::statement_node *node);
			void visit(std::stringstream &output, const nodes::expression_node *node);
			virtual void visit(std::stringstream &output, const nodes::type_node &type, bool with_qualifiers) = 0;
			virtual void visit(std::stringstream &output, const nodes::lvalue_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::literal_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::expression_sequence_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::unary_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::binary_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::intrinsic_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::conditional_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::swizzle_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::field_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::initializer_list_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::assignment_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::call_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::constructor_expression_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::compound_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::declarator_list_node *node, bool output_single_statement = false) = 0;
			virtual void visit(std::stringstream &output, const nodes::expression_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::if_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::switch_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::case_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::for_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::while_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::return_statement_node *node) = 0;
			virtual void visit(std::stringstream &output, const nodes::jump_statement_node *node) = 0;

			bool _success;
			const nodetree &_ast;
			std::string &_errors;
		};
	}
}
