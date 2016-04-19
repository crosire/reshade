#pragma once

#include "syntax_tree.hpp"
#include "d3d11_runtime.hpp"

namespace reshade
{
	class d3d11_fx_compiler
	{
	public:
		d3d11_fx_compiler(d3d11_runtime *runtime, const fx::syntax_tree &ast, std::string &errors, bool skipoptimization = false);

		bool run();

	private:
		void error(const fx::location &location, const std::string &message);
		void warning(const fx::location &location, const std::string &message);

		void visit(std::stringstream &output, const fx::nodes::statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::type_node &type, bool with_qualifiers = true);
		void visit(std::stringstream &output, const fx::nodes::lvalue_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::literal_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::expression_sequence_node *node);
		void visit(std::stringstream &output, const fx::nodes::unary_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::binary_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::intrinsic_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::conditional_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::swizzle_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::field_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::assignment_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::call_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::constructor_expression_node *node);
		void visit(std::stringstream &output, const fx::nodes::initializer_list_node *node);
		void visit(std::stringstream &output, const fx::nodes::compound_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::declarator_list_node *node, bool single_statement);
		void visit(std::stringstream &output, const fx::nodes::expression_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::if_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::switch_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::case_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::for_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::while_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::return_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::jump_statement_node *node);
		void visit(std::stringstream &output, const fx::nodes::struct_declaration_node *node);
		void visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type = true);
		void visit(std::stringstream &output, const fx::nodes::function_declaration_node *node);

		void visit_texture(const fx::nodes::variable_declaration_node *node);
		void visit_sampler(const fx::nodes::variable_declaration_node *node);
		void visit_uniform(const fx::nodes::variable_declaration_node *node);
		void visit_technique(const fx::nodes::technique_declaration_node *node);
		void visit_pass(const fx::nodes::pass_declaration_node *node, d3d11_pass &pass);
		void visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, d3d11_pass &pass);

		d3d11_runtime *_runtime;
		bool _success;
		const fx::syntax_tree &_ast;
		std::string &_errors;
		std::stringstream _global_code, _global_uniforms;
		bool _skip_shader_optimization;
		std::unordered_map<size_t, size_t> _sampler_descs;
		UINT _current_global_size;
		bool _is_in_parameter_block, _is_in_function_block;
	};
}
