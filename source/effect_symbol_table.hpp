/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "spirv_helpers.hpp"

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
		spv_type type;
		const spv_function_info *function = nullptr;
		spv_constant constant = {};
		size_t member_index = std::numeric_limits<size_t>::max();
	};

	/// <summary>
	/// A symbol table managing a list of scopes and symbols.
	/// </summary>
	class symbol_table
	{
	public:
		symbol_table();

		void enter_scope();
		void enter_namespace(const std::string &name);
		void leave_scope();
		void leave_namespace();

		const scope &current_scope() const { return _current_scope; }

		bool insert_symbol(const std::string &name, const symbol &symbol, bool global = false);

		symbol find_symbol(const std::string &name) const;
		symbol find_symbol(const std::string &name, const scope &scope, bool exclusive) const;

		bool resolve_function_call(const std::string &name, const std::vector<spv_expression> &args, const scope &scope, bool &ambiguous, symbol &out_data) const;

	private:
		struct scoped_symbol : symbol {
			scope scope; // Store scope with symbol data
		};

		scope _current_scope;
		std::unordered_map<std::string, // Lookup table from name to matching symbols
			std::vector<scoped_symbol>> _symbol_stack;
	};
}
