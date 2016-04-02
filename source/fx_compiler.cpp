#include "fx_compiler.hpp"

#include <assert.h>

namespace reshade
{
	namespace fx
	{
		using namespace nodes;

		compiler::compiler(const nodetree &ast, std::string &errors) : _success(true), _ast(ast), _errors(errors)
		{
		}

		void compiler::error(const fx::location &location, const std::string &message)
		{
			_success = false;

			_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): error: " + message + '\n';
		}
		void compiler::warning(const fx::location &location, const std::string &message)
		{
			_errors += location.source + "(" + std::to_string(location.line) + ", " + std::to_string(location.column) + "): warning: " + message + '\n';
		}

		void compiler::visit(std::stringstream &output, const statement_node *node)
		{
			if (node == nullptr)
			{
				return;
			}

			switch (node->id)
			{
				case nodeid::compound_statement:
					visit(output, static_cast<const compound_statement_node *>(node));
					break;
				case nodeid::declarator_list:
					visit(output, static_cast<const declarator_list_node *>(node));
					break;
				case nodeid::expression_statement:
					visit(output, static_cast<const expression_statement_node *>(node));
					break;
				case nodeid::if_statement:
					visit(output, static_cast<const if_statement_node *>(node));
					break;
				case nodeid::switch_statement:
					visit(output, static_cast<const switch_statement_node *>(node));
					break;
				case nodeid::for_statement:
					visit(output, static_cast<const for_statement_node *>(node));
					break;
				case nodeid::while_statement:
					visit(output, static_cast<const while_statement_node *>(node));
					break;
				case nodeid::return_statement:
					visit(output, static_cast<const return_statement_node *>(node));
					break;
				case nodeid::jump_statement:
					visit(output, static_cast<const jump_statement_node *>(node));
					break;
				default:
					assert(false);
			}
		}
		void compiler::visit(std::stringstream &output, const expression_node *node)
		{
			switch (node->id)
			{
				case nodeid::lvalue_expression:
					visit(output, static_cast<const lvalue_expression_node *>(node));
					break;
				case nodeid::literal_expression:
					visit(output, static_cast<const literal_expression_node *>(node));
					break;
				case nodeid::expression_sequence:
					visit(output, static_cast<const expression_sequence_node *>(node));
					break;
				case nodeid::unary_expression:
					visit(output, static_cast<const unary_expression_node *>(node));
					break;
				case nodeid::binary_expression:
					visit(output, static_cast<const binary_expression_node *>(node));
					break;
				case nodeid::intrinsic_expression:
					visit(output, static_cast<const intrinsic_expression_node *>(node));
					break;
				case nodeid::conditional_expression:
					visit(output, static_cast<const conditional_expression_node *>(node));
					break;
				case nodeid::swizzle_expression:
					visit(output, static_cast<const swizzle_expression_node *>(node));
					break;
				case nodeid::field_expression:
					visit(output, static_cast<const field_expression_node *>(node));
					break;
				case nodeid::initializer_list:
					visit(output, static_cast<const initializer_list_node *>(node));
					break;
				case nodeid::assignment_expression:
					visit(output, static_cast<const assignment_expression_node *>(node));
					break;
				case nodeid::call_expression:
					visit(output, static_cast<const call_expression_node *>(node));
					break;
				case nodeid::constructor_expression:
					visit(output, static_cast<const constructor_expression_node *>(node));
					break;
				default:
					assert(false);
			}
		}
	}
}
