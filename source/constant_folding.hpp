#pragma once

#pragma region Forward Declaration
namespace reshade
{
	namespace fx
	{
		class nodetree;

		namespace nodes
		{
			struct expression_node;
			struct literal_expression_node;
		}
	}
}
#pragma endregion

namespace reshade
{
	namespace fx
	{
		void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, int &to);
		void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, unsigned int &to);
		void scalar_literal_cast(const nodes::literal_expression_node *from, size_t i, float &to);
		void vector_literal_cast(const nodes::literal_expression_node *from, size_t k, nodes::literal_expression_node *to, size_t j);

		nodes::expression_node *fold_constant_expression(nodetree &ast, nodes::expression_node *expression);
	}
}
