/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <sstream>
#include "syntax_tree.hpp"

namespace reshade::opengl
{
	#pragma region Forward Declarations
	struct opengl_pass_data;
	class opengl_runtime;
	#pragma endregion

	class opengl_effect_compiler
	{
	public:
		opengl_effect_compiler(opengl_runtime *runtime, const reshadefx::syntax_tree &ast, std::string &errors);

		bool run();

	private:
		void error(const reshadefx::location &location, const std::string &message);
		void warning(const reshadefx::location &location, const std::string &message);

		void visit(std::stringstream &output, const reshadefx::nodes::statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::type_node &type, bool with_qualifiers = true);
		void visit(std::stringstream &output, const reshadefx::nodes::lvalue_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::literal_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::expression_sequence_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::unary_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::binary_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::intrinsic_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::conditional_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::swizzle_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::field_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::assignment_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::call_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::constructor_expression_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::initializer_list_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::initializer_list_node *node, const reshadefx::nodes::type_node &type);
		void visit(std::stringstream &output, const reshadefx::nodes::compound_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::declarator_list_node *node, bool single_statement = false);
		void visit(std::stringstream &output, const reshadefx::nodes::expression_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::if_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::switch_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::case_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::for_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::while_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::return_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::jump_statement_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::struct_declaration_node *node);
		void visit(std::stringstream &output, const reshadefx::nodes::variable_declaration_node *node, bool with_type = true);
		void visit(std::stringstream &output, const reshadefx::nodes::function_declaration_node *node);

		void visit_texture(const reshadefx::nodes::variable_declaration_node *node);
		void visit_sampler(const reshadefx::nodes::variable_declaration_node *node);
		void visit_uniform(const reshadefx::nodes::variable_declaration_node *node);
		void visit_technique(const reshadefx::nodes::technique_declaration_node *node);
		void visit_pass(const reshadefx::nodes::pass_declaration_node *node, opengl_pass_data &pass);
		void visit_pass_shader(const reshadefx::nodes::function_declaration_node *node, unsigned int shadertype, unsigned int &shader);
		void visit_shader_param(std::stringstream &output, reshadefx::nodes::type_node type, unsigned int qualifier, const std::string &name, const std::string &semantic, unsigned int shadertype);

		struct function
		{
			std::string code;
			std::vector<const reshadefx::nodes::function_declaration_node *> dependencies;
		};

		opengl_runtime *_runtime;
		bool _success;
		const reshadefx::syntax_tree &_ast;
		std::string &_errors;
		std::stringstream _global_code, _global_uniforms;
		const reshadefx::nodes::function_declaration_node *_current_function;
		std::unordered_map<const reshadefx::nodes::function_declaration_node *, function> _functions;
		GLintptr _uniform_storage_offset = 0, _uniform_buffer_size = 0;
	};
}
