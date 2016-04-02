#pragma once

#include "fx_compiler.hpp"
#include "d3d9_runtime.hpp"

#include <unordered_set>

namespace reshade
{
	class d3d9_fx_compiler : fx::compiler
	{
	public:
		d3d9_fx_compiler(d3d9_runtime *runtime, const fx::nodetree &ast, std::string &errors, bool skipoptimization = false);

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
		void visit(std::stringstream &output, const fx::nodes::variable_declaration_node *node, bool with_type = true, bool with_semantic = true);
		void visit(std::stringstream &output, const fx::nodes::function_declaration_node *node);

		void visit_texture(const fx::nodes::variable_declaration_node *node);
		void visit_sampler(const fx::nodes::variable_declaration_node *node);
		void visit_uniform(const fx::nodes::variable_declaration_node *node);
		void visit_technique(const fx::nodes::technique_declaration_node *node);
		void visit_pass(const fx::nodes::pass_declaration_node *node, d3d9_pass &pass);
		void visit_pass_shader(const fx::nodes::function_declaration_node *node, const std::string &shadertype, const std::string &samplers, d3d9_pass &pass);

		struct function
		{
			std::string code;
			std::vector<const fx::nodes::function_declaration_node *> dependencies;
			std::unordered_set<const fx::nodes::variable_declaration_node *> sampler_dependencies;
		};

		d3d9_runtime *_runtime;
		std::stringstream _global_code, _global_uniforms;
		bool _skip_shader_optimization;
		unsigned int _current_register_offset;
		const fx::nodes::function_declaration_node *_current_function;
		std::unordered_map<std::string, d3d9_sampler> _samplers;
		std::unordered_map<const fx::nodes::function_declaration_node *, function> _functions;
	};
}
