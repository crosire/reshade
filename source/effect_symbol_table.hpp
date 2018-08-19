/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <stack>
#include <unordered_map>
#include <string>
#include "effect_parser.hpp"

namespace reshadefx
{
	/// <summary>
	/// A scope encapsulating a list of symbols.
	/// </summary>
	struct scope
	{
		std::string name;
		unsigned int level, namespace_level;
	};

	/// <summary>
	/// A single symbol in the symbol table.
	/// </summary>
	struct symbol
	{
		spv::Op op;
		spv::Id id;
		type_info type;
		const void *info;
	};

	/// <summary>
	/// A symbol table managing a list of scopes and symbols.
	/// </summary>
	class symbol_table
	{
		struct symbol_data : symbol
		{
			scope scope;
		};

	public:
		symbol_table();

		void enter_scope(void *parent = nullptr);
		void enter_namespace(const std::string &name);
		void leave_scope();
		void leave_namespace();

		void *current_parent() const { return _parent_stack.empty() ? 0 : _parent_stack.top(); }
		const scope &current_scope() const { return _current_scope; }

		bool insert(const std::string &name, const symbol &symbol, bool global = false);

		symbol find(const std::string &name) const;
		symbol find(const std::string &name, const scope &scope, bool exclusive) const;

		bool resolve_call(const std::string &name, const std::vector<type_info> &args, const scope &scope, bool &ambiguous, symbol &out_data) const;

	private:
		scope _current_scope;
		std::stack<void *> _parent_stack;
		std::unordered_map<std::string, std::vector<symbol_data>> _symbol_stack;
	};
}
