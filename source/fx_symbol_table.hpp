#pragma once

#include <stack>
#include <unordered_map>

#pragma region Forward Declaration
namespace reshade
{
	namespace fx
	{
		namespace nodes
		{
			struct declaration_node;
			struct call_expression_node;
		}
	}
}
#pragma endregion

namespace reshade
{
	namespace fx
	{
		struct scope
		{
			std::string name;
			unsigned int level, namespace_level;
		};

		typedef nodes::declaration_node *symbol;

		class symbol_table
		{
		public:
			symbol_table();

			void enter_scope(symbol parent = nullptr);
			void enter_namespace(const std::string &name);
			void leave_scope();
			void leave_namespace();

			symbol current_parent() const { return _parent_stack.empty() ? nullptr : _parent_stack.top(); }
			inline const scope &current_scope() const { return _current_scope; }

			bool insert(symbol symbol, bool global = false);
			symbol find(const std::string &name) const;
			symbol find(const std::string &name, const scope &scope, bool exclusive) const;
			bool resolve_call(nodes::call_expression_node *call, const scope &scope, bool &intrinsic, bool &ambiguous) const;

		private:
			scope _current_scope;
			std::stack<symbol> _parent_stack;
			std::unordered_map<std::string, std::vector<std::pair<scope, symbol>>> _symbol_stack;
		};
	}
}
