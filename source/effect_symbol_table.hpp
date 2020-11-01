/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_module.hpp"
#include <unordered_map> // Used for symbol lookup table

namespace reshadefx
{
	/// <summary>
	/// A scope encapsulating symbols
	/// </summary>
	struct scope
	{
		std::string name;
		unsigned int level, namespace_level;
	};

	/// <summary>
	/// Enumeration of all possible symbol types
	/// </summary>
	enum class symbol_type
	{
		invalid,
		variable,
		constant,
		function,
		intrinsic,
		structure,
	};

	/// <summary>
	/// A single symbol in the symbol table
	/// </summary>
	struct symbol
	{
		symbol_type op = symbol_type::invalid;
		uint32_t id = 0;
		reshadefx::type type = {};
		reshadefx::constant constant = {};
		const reshadefx::function_info *function = nullptr;
	};
	struct scoped_symbol : symbol
	{
		struct scope scope; // Store scope with symbol data
	};

	/// <summary>
	/// A symbol table managing a list of scopes and symbols
	/// </summary>
	class symbol_table
	{
	public:
		symbol_table();

		/// <summary>
		/// Enter a new scope as child of the current one.
		/// </summary>
		void enter_scope();
		/// <summary>
		/// Enter a new namespace as child of the current one.
		/// </summary>
		void enter_namespace(const std::string &name);
		/// <summary>
		/// Leave the current scope and enter the parent one.
		/// </summary>
		void leave_scope();
		/// <summary>
		/// Leave the current namespace and enter the parent one.
		/// </summary>
		void leave_namespace();

		/// <summary>
		/// Get the current scope the symbol table operates in.
		/// </summary>
		/// <returns></returns>
		const scope &current_scope() const { return _current_scope; }

		/// <summary>
		/// Insert an new symbol in the symbol table. Returns <c>false</c> if a symbol by that name and type already exists.
		/// </summary>
		bool insert_symbol(const std::string &name, const symbol &symbol, bool global = false);

		/// <summary>
		/// Look for an existing symbol with the specified <paramref name="name"/>.
		/// </summary>
		scoped_symbol find_symbol(const std::string &name) const;
		scoped_symbol find_symbol(const std::string &name, const scope &scope, bool exclusive) const;

		/// <summary>
		/// Search for the best function or intrinsic overload matching the argument list.
		/// </summary>
		bool resolve_function_call(const std::string &name, const std::vector<expression> &args, const scope &scope, symbol &data, bool &ambiguous) const;

	private:
		scope _current_scope;
		std::unordered_map<std::string, // Lookup table from name to matching symbols
			std::vector<scoped_symbol>> _symbol_stack;
	};
}
