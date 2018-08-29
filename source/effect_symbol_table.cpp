/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_symbol_table.hpp"
//#include "effect_syntax_tree_nodes.hpp"

#include <assert.h>
#include <algorithm>
#include <functional>

#include <spirv.hpp>
namespace spv {
#include <GLSL.std.450.h>
}

using namespace reshadefx;

#include "effect_intrinsics.inl"

static int compare_functions(const std::vector<reshadefx::type_info> &arguments, const function_info *function1, const function_info *function2)
{
	if (function2 == nullptr)
	{
		return -1;
	}

	const size_t count = arguments.size();

	bool function1_viable = true;
	bool function2_viable = true;
	const auto function1_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));
	const auto function2_ranks = static_cast<unsigned int *>(alloca(count * sizeof(unsigned int)));

	for (size_t i = 0; i < count; ++i)
	{
		function1_ranks[i] = reshadefx::type_info::rank(arguments[i], function1->parameter_list[i].type);

		if (function1_ranks[i] == 0)
		{
			function1_viable = false;
			break;
		}
	}
	for (size_t i = 0; i < count; ++i)
	{
		function2_ranks[i] = reshadefx::type_info::rank(arguments[i], function2->parameter_list[i].type);

		if (function2_ranks[i] == 0)
		{
			function2_viable = false;
			break;
		}
	}

	if (!(function1_viable && function2_viable))
	{
		return function2_viable - function1_viable;
	}

	std::sort(function1_ranks, function1_ranks + count, std::greater<unsigned int>());
	std::sort(function2_ranks, function2_ranks + count, std::greater<unsigned int>());

	for (size_t i = 0; i < count; ++i)
	{
		if (function1_ranks[i] < function2_ranks[i])
		{
			return -1;
		}
		else if (function2_ranks[i] < function1_ranks[i])
		{
			return +1;
		}
	}

	return 0;
}

unsigned int reshadefx::type_info::rank(const reshadefx::type_info &src, const reshadefx::type_info &dst)
{
	if (src.is_array() != dst.is_array() || (src.array_length != dst.array_length && src.array_length > 0 && dst.array_length > 0))
	{
		return 0;
	}
	if (src.is_struct() || dst.is_struct())
	{
		return src.definition == dst.definition;
	}
	if (src.base == dst.base && src.rows == dst.rows && src.cols == dst.cols)
	{
		return 1;
	}
	if (!src.is_numeric() || !dst.is_numeric())
	{
		return 0;
	}

	const int ranks[4][4] =
	{
		{ 0, 5, 5, 5 },
		{ 4, 0, 3, 5 },
		{ 4, 2, 0, 5 },
		{ 4, 4, 4, 0 }
	};

	int u = src.base == spv::OpTypeBool ? 0 : src.base == spv::OpTypeFloat ? 3 : src.is_signed ? 1 : 2;
	int v = dst.base == spv::OpTypeBool ? 0 : dst.base == spv::OpTypeFloat ? 3 : dst.is_signed ? 1 : 2;

	const int rank = ranks[u][v] << 2;

	if (src.is_scalar() && dst.is_vector())
	{
		return rank | 2;
	}
	if (src.is_vector() && dst.is_scalar() || (src.is_vector() == dst.is_vector() && src.rows > dst.rows && src.cols >= dst.cols))
	{
		return rank | 32;
	}
	if (src.is_vector() != dst.is_vector() || src.is_matrix() != dst.is_matrix() || src.rows * src.cols != dst.rows * dst.cols)
	{
		return 0;
	}

	return rank;
}

reshadefx::symbol_table::symbol_table()
{
	_current_scope.name = "::";
	_current_scope.level = 0;
	_current_scope.namespace_level = 0;
}

void reshadefx::symbol_table::enter_scope(void *parent)
{
	if (parent || _parent_stack.empty())
	{
		_parent_stack.push(parent);
	}
	else
	{
		_parent_stack.push(_parent_stack.top());
	}

	_current_scope.level++;
}
void reshadefx::symbol_table::enter_namespace(const std::string &name)
{
	_current_scope.name += name + "::";
	_current_scope.level++;
	_current_scope.namespace_level++;
}
void reshadefx::symbol_table::leave_scope()
{
	assert(_current_scope.level > 0);

	for (auto &symbol : _symbol_stack)
	{
		auto &scope_list = symbol.second;

		for (auto scope_it = scope_list.begin(); scope_it != scope_list.end();)
		{
			if (scope_it->scope.level > scope_it->scope.namespace_level &&
				scope_it->scope.level >= _current_scope.level)
			{
				scope_it = scope_list.erase(scope_it);
			}
			else
			{
				++scope_it;
			}
		}
	}

	_parent_stack.pop();

	_current_scope.level--;
}
void reshadefx::symbol_table::leave_namespace()
{
	assert(_current_scope.level > 0);
	assert(_current_scope.namespace_level > 0);

	_current_scope.name.erase(_current_scope.name.substr(0, _current_scope.name.size() - 2).rfind("::") + 2);
	_current_scope.level--;
	_current_scope.namespace_level--;
}

bool reshadefx::symbol_table::insert(const std::string &name, const symbol &symbol, bool global)
{
	assert(symbol.id != 0);

	// Make sure the symbol does not exist yet
	if (symbol.op != spv::OpFunction && find(name, _current_scope, true).id != 0)
	{
		return false;
	}

	// Insertion routine which keeps the symbol stack sorted by namespace level
	const auto insert_sorted = [](auto &vec, const auto &item) {
		return vec.insert(
			std::upper_bound(vec.begin(), vec.end(), item,
				[](auto lhs, auto rhs) {
					return lhs.scope.namespace_level < rhs.scope.namespace_level;
				}), item);
	};

	// Global symbols are accessible from every scope
	if (global)
	{
		scope scope = { "", 0, 0 };

		// Walk scope chain from global scope back to current one
		for (size_t pos = 0; pos != std::string::npos; pos = _current_scope.name.find("::", pos))
		{
			// Extract scope name
			scope.name = _current_scope.name.substr(0, pos += 2);
			const auto previous_scope_name = _current_scope.name.substr(pos);

			// Insert symbol into this scope
			insert_sorted(_symbol_stack[previous_scope_name + name], symbol_data { symbol, scope });

			// Continue walking up the scope chain
			scope.level = ++scope.namespace_level;
		}
	}
	else
	{
		// This is a local symbol so it's sufficient to update the symbol stack with just the current scope
		insert_sorted(_symbol_stack[name], symbol_data { symbol, _current_scope });
	}

	return true;
}

reshadefx::symbol reshadefx::symbol_table::find(const std::string &name) const
{
	// Default to start search with current scope and walk back the scope chain
	return find(name, _current_scope, false);
}
reshadefx::symbol reshadefx::symbol_table::find(const std::string &name, const scope &scope, bool exclusive) const
{
	const auto stack_it = _symbol_stack.find(name);

	// Check if symbol does exist
	if (stack_it == _symbol_stack.end() || stack_it->second.empty())
	{
		return {};
	}

	// Walk up the scope chain starting at the requested scope level and find a matching symbol
	symbol result = {};

	for (auto it = stack_it->second.rbegin(), end = stack_it->second.rend(); it != end; ++it)
	{
		if (it->scope.level > scope.level ||
			it->scope.namespace_level > scope.namespace_level || (it->scope.namespace_level == scope.namespace_level && it->scope.name != scope.name))
			continue;
		if (exclusive && it->scope.level < scope.level)
			continue;

		if (it->op == spv::OpVariable || it->op == spv::OpTypeStruct)
			return *it;
		else if (result.id == 0)
			result = *it;
	}

	return result;
}

bool reshadefx::symbol_table::resolve_call(const std::string &name, const std::vector<type_info> &arguments, const scope &scope, bool &is_ambiguous, symbol &out_data) const
{
	out_data.op = spv::OpFunctionCall;
	is_ambiguous = false;

	spv::Op intrinsic_op = spv::OpNop;
	const function_info *overload_props = nullptr;
	unsigned int overload_count = 0, overload_namespace = scope.namespace_level;

	const auto stack_it = _symbol_stack.find(name);

	if (stack_it != _symbol_stack.end() && !stack_it->second.empty())
	{
		for (auto it = stack_it->second.rbegin(), end = stack_it->second.rend(); it != end; ++it)
		{
			if (it->scope.level > scope.level ||
				it->scope.namespace_level > scope.namespace_level ||
				it->op != spv::OpFunction)
			{
				continue;
			}

			const auto function = static_cast<const function_info *>(it->info);

			if (function->parameter_list.empty())
			{
				if (arguments.empty())
				{
					out_data.id = it->id;
					out_data.info = it->info;
					out_data.type = function->return_type;
					overload_props = function;
					overload_count = 1;
					break;
				}
				else
				{
					continue;
				}
			}
			else if (arguments.size() != function->parameter_list.size())
			{
				continue;
			}

			const int comparison = compare_functions(arguments, function, overload_props);

			if (comparison < 0)
			{
				out_data.id = it->id;
				out_data.info = it->info;
				out_data.type = function->return_type;
				overload_props = function;
				overload_count = 1;
				overload_namespace = it->scope.namespace_level;
			}
			else if (comparison == 0 && overload_namespace == it->scope.namespace_level)
			{
				++overload_count;
			}
		}
	}

	if (overload_count == 0)
	{
		for (auto &intrinsic : s_intrinsics)
		{
			if (intrinsic.function.name != name)
			{
				continue;
			}
			if (intrinsic.function.parameter_list.size() != arguments.size())
			{
				break;
			}

			const int comparison = compare_functions(arguments, &intrinsic.function, overload_props);

			if (comparison < 0)
			{
				out_data.op = intrinsic.op;
				out_data.id = intrinsic.glsl;
				out_data.info = &intrinsic.function;
				out_data.type = intrinsic.function.return_type;
				overload_props = &intrinsic.function;
				overload_count = 1;

				intrinsic_op = intrinsic.op;
			}
			else if (comparison == 0 && overload_namespace == 0)
			{
				++overload_count;
			}
		}
	}

	if (overload_count == 1)
	{
		return true;
	}
	else
	{
		is_ambiguous = overload_count > 1;

		return false;
	}
}
