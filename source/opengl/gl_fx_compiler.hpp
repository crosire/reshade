#pragma once

#include "fx_compiler.hpp"
#include "gl_runtime.hpp"

namespace reshade
{
	class gl_fx_compiler : fx::compiler
	{
	public:
		gl_fx_compiler(gl_runtime *runtime, const fx::nodetree &ast, std::string &errors);

		bool run() override;

	private:
		using compiler::visit;
		void visit(std::stringstream &output, const fx::nodes::type_node &type, bool with_qualifiers = true) override;
		void visit(std::stringstream &output, const fx::nodes::lvalue_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::literal_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::expression_sequence_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::unary_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::binary_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::intrinsic_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::conditional_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::swizzle_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::field_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::assignment_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::call_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::constructor_expression_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::initializer_list_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::initializer_list_node *node, const fx::nodes::type_node &type);
		void visit(std::stringstream &output, const fx::nodes::compound_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::declarator_list_node *node, bool single_statement = false) override;
		void visit(std::stringstream &output, const fx::nodes::expression_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::if_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::switch_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::case_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::for_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::while_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::return_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::jump_statement_node *node) override;
		void visit(std::stringstream &output, const fx::nodes::struct_declaration_node *node);
		void visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type = true);
		void visit(std::stringstream &output, const fx::nodes::function_declaration_node *node);

		void visit_texture(const fx::nodes::variable_declaration_node *node);
		void visit_sampler(const fx::nodes::variable_declaration_node *node);
		void visit_uniform(const fx::nodes::variable_declaration_node *node);
		void visit_technique(const fx::nodes::technique_declaration_node *node);
		void visit_pass(const fx::nodes::pass_declaration_node *node, gl_pass &pass);
		void visit_pass_shader(const fx::nodes::function_declaration_node *node, GLuint shadertype, GLuint &shader);
		void visit_shader_param(std::stringstream &output, fx::nodes::type_node type, unsigned int qualifier, const std::string &name, const std::string &semantic, GLuint shadertype);

		struct function
		{
			std::string code;
			std::vector<const fx::nodes::function_declaration_node *> dependencies;
		};

		gl_runtime *_runtime;
		std::stringstream _global_code, _global_uniforms;
		size_t _current_uniform_size;
		const fx::nodes::function_declaration_node *_current_function;
		std::unordered_map<const fx::nodes::function_declaration_node *, function> _functions;
	};
}
