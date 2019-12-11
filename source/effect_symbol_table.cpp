/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_symbol_table.hpp"
#include <cassert>
#include <algorithm> // std::upper_bound, std::sort

#pragma region Import intrinsic functions

using namespace reshadefx;

struct intrinsic
{
	intrinsic(const char *name, unsigned int id, const type &ret_type, std::initializer_list<type> arg_types) : id(id)
	{
		function.name = name;
		function.return_type = ret_type;
		function.parameter_list.reserve(arg_types.size());
		for (const type &arg_type : arg_types)
			function.parameter_list.push_back({ arg_type });
	}

	unsigned int id;
	function_info function;
};

// Import intrinsic callback functions
#define IMPLEMENT_INTRINSIC_SPIRV(name, i, code) name##i,
enum {
#include "effect_symbol_table_intrinsics.inl"
};

#define void { reshadefx::type::t_void }
#define bool { reshadefx::type::t_bool, 1, 1 }
#define bool2 { reshadefx::type::t_bool, 2, 1 }
#define bool3 { reshadefx::type::t_bool, 3, 1 }
#define bool4 { reshadefx::type::t_bool, 4, 1 }
#define int { reshadefx::type::t_int, 1, 1 }
#define int2 { reshadefx::type::t_int, 2, 1 }
#define int3 { reshadefx::type::t_int, 3, 1 }
#define int4 { reshadefx::type::t_int, 4, 1 }
#define uint { reshadefx::type::t_uint, 1, 1 }
#define uint2 { reshadefx::type::t_uint, 2, 1 }
#define uint3 { reshadefx::type::t_uint, 3, 1 }
#define uint4 { reshadefx::type::t_uint, 4, 1 }
#define float { reshadefx::type::t_float, 1, 1 }
#define float2 { reshadefx::type::t_float, 2, 1 }
#define float3 { reshadefx::type::t_float, 3, 1 }
#define float4 { reshadefx::type::t_float, 4, 1 }
#define float2x2 { reshadefx::type::t_float, 2, 2 }
#define float3x3 { reshadefx::type::t_float, 3, 3 }
#define float4x4 { reshadefx::type::t_float, 4, 4 }
#define out_float { reshadefx::type::t_float, 1, 1, reshadefx::type::q_out }
#define out_float2 { reshadefx::type::t_float, 2, 1, reshadefx::type::q_out }
#define out_float3 { reshadefx::type::t_float, 3, 1, reshadefx::type::q_out }
#define out_float4 { reshadefx::type::t_float, 4, 1, reshadefx::type::q_out }
#define sampler { reshadefx::type::t_sampler }

// Import intrinsic function definitions
#define DEFINE_INTRINSIC(name, i, ret_type, ...) intrinsic(#name, name##i, ret_type, { __VA_ARGS__ }),
static const intrinsic s_intrinsics[] = {
#include "effect_symbol_table_intrinsics.inl"
};

#undef void
#undef bool
#undef bool2
#undef bool3
#undef bool4
#undef int
#undef int2
#undef int3
#undef int4
#undef uint
#undef uint2
#undef uint3
#undef uint4
#undef float1
#undef float2
#undef float3
#undef float4
#undef float2x2
#undef float3x3
#undef float4x4
#undef out_float
#undef out_float2
#undef out_float3
#undef out_float4
#undef sampler

#pragma endregion

unsigned int reshadefx::type::rank(const type &src, const type &dst)
{
	if (src.is_array() != dst.is_array() || (src.array_length != dst.array_length && src.array_length > 0 && dst.array_length > 0))
		return 0; // Arrays of different sizes are not compatible
	if (src.is_struct() || dst.is_struct())
		return src.definition == dst.definition ? 32 : 0; // Structs are only compatible if they are the same type
	if (!src.is_numeric() || !dst.is_numeric())
		return src.base == dst.base ? 32 : 0; // Numeric values are not compatible with other types

	// This table is based on the following rules:
	//  - Floating point has a higher rank than integer types
	//  - Integer to floating point promotion has a higher rank than floating point to integer conversion
	//  - Signed to unsigned integer conversion has a higher rank than unsigned to signed integer conversion
	static const int ranks[4][4] = {
		{ 5, 4, 4, 4 },
		{ 3, 5, 2, 4 },
		{ 3, 1, 5, 4 },
		{ 3, 3, 3, 6 }
	};

	assert(src.base > 0 && src.base <= 4);
	assert(dst.base > 0 && dst.base <= 4);

	const int rank = ranks[src.base - 1][dst.base - 1] << 2;

	if (src.is_scalar() && dst.is_vector())
		return rank >> 1; // Scalar to vector promotion has a lower rank
	if (src.is_vector() && dst.is_scalar() || (src.is_vector() == dst.is_vector() && src.rows > dst.rows && src.cols >= dst.cols))
		return rank >> 2; // Vector to scalar conversion has an even lower rank
	if (src.is_vector() != dst.is_vector() || src.is_matrix() != dst.is_matrix() || src.components() != dst.components())
		return 0; // If components weren't converted at this point, the types are not compatible

	return rank * src.components(); // More components causes a higher rank
}

reshadefx::symbol_table::symbol_table()
{
	_current_scope.name = "::";
	_current_scope.level = 0;
	_current_scope.namespace_level = 0;
}

void reshadefx::symbol_table::enter_scope()
{
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
		std::vector<scoped_symbol> &scope_list = symbol.second;

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

bool reshadefx::symbol_table::insert_symbol(const std::string &name, const symbol &symbol, bool global)
{
	assert(symbol.id != 0 || symbol.op == symbol_type::constant);

	// Make sure the symbol does not exist yet
	if (symbol.op != symbol_type::function && find_symbol(name, _current_scope, true).id != 0)
		return false;

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
			insert_sorted(_symbol_stack[previous_scope_name + name], scoped_symbol { symbol, scope });

			// Continue walking up the scope chain
			scope.level = ++scope.namespace_level;
		}
	}
	else
	{
		// This is a local symbol so it's sufficient to update the symbol stack with just the current scope
		insert_sorted(_symbol_stack[name], scoped_symbol { symbol, _current_scope });
	}

	return true;
}

reshadefx::symbol reshadefx::symbol_table::find_symbol(const std::string &name) const
{
	// Default to start search with current scope and walk back the scope chain
	return find_symbol(name, _current_scope, false);
}
reshadefx::symbol reshadefx::symbol_table::find_symbol(const std::string &name, const scope &scope, bool exclusive) const
{
	const auto stack_it = _symbol_stack.find(name);

	// Check if symbol does exist
	if (stack_it == _symbol_stack.end() || stack_it->second.empty())
		return {};

	// Walk up the scope chain starting at the requested scope level and find a matching symbol
	symbol result = {};

	for (auto it = stack_it->second.rbegin(), end = stack_it->second.rend(); it != end; ++it)
	{
		if (it->scope.level > scope.level ||
			it->scope.namespace_level > scope.namespace_level || (it->scope.namespace_level == scope.namespace_level && it->scope.name != scope.name))
			continue;
		if (exclusive && it->scope.level < scope.level)
			continue;

		if (it->op == symbol_type::constant || it->op == symbol_type::variable || it->op == symbol_type::structure)
			return *it; // Variables and structures have the highest priority and are always picked immediately
		else if (result.id == 0)
			result = *it; // Function names have a lower priority, so continue searching in case a variable with the same name exists
	}

	return result;
}

static int compare_functions(const std::vector<reshadefx::expression> &arguments, const reshadefx::function_info *function1, const reshadefx::function_info *function2)
{
	const size_t num_arguments = arguments.size();

	// Check if the first function matches the argument types
	bool function1_viable = true;
	const auto function1_ranks = static_cast<unsigned int *>(alloca(num_arguments * sizeof(unsigned int)));
	for (size_t i = 0; i < num_arguments; ++i)
		if ((function1_ranks[i] = reshadefx::type::rank(arguments[i].type, function1->parameter_list[i].type)) == 0)
		{
			function1_viable = false;
			break;
		}

	// Catch case where the second function does not exist
	if (function2 == nullptr)
		return function1_viable ? -1 : 1; // If the first function is not viable, this compare fails

	// Check if the second function matches the argument types
	bool function2_viable = true;
	const auto function2_ranks = static_cast<unsigned int *>(alloca(num_arguments * sizeof(unsigned int)));
	for (size_t i = 0; i < num_arguments; ++i)
		if ((function2_ranks[i] = reshadefx::type::rank(arguments[i].type, function2->parameter_list[i].type)) == 0)
		{
			function2_viable = false;
			break;
		}

	// If one of the functions is not viable, then the other one automatically wins
	if (!function1_viable || !function2_viable)
		return function2_viable - function1_viable;

	// Both functions are possible, so find the one with the higher ranking
	std::sort(function1_ranks, function1_ranks + num_arguments, std::greater<unsigned int>());
	std::sort(function2_ranks, function2_ranks + num_arguments, std::greater<unsigned int>());

	for (size_t i = 0; i < num_arguments; ++i)
		if (function1_ranks[i] > function2_ranks[i])
			return -1; // Left function wins
		else if (function2_ranks[i] > function1_ranks[i])
			return +1; // Right function wins

	return 0; // Both functions are equally viable
}

bool reshadefx::symbol_table::resolve_function_call(const std::string &name, const std::vector<expression> &arguments, const scope &scope, symbol &out_data, bool &is_ambiguous) const
{
	out_data.op = symbol_type::function;

	const function_info *result = nullptr;
	unsigned int num_overloads = 0;
	unsigned int overload_namespace = scope.namespace_level;

	// Look up function name in the symbol stack and loop through the associated symbols
	const auto stack_it = _symbol_stack.find(name);

	if (stack_it != _symbol_stack.end() && !stack_it->second.empty())
	{
		for (auto it = stack_it->second.rbegin(), end = stack_it->second.rend(); it != end; ++it)
		{
			if (it->scope.level > scope.level ||
				it->scope.namespace_level > scope.namespace_level ||
				it->op != symbol_type::function)
				continue;

			const function_info *const function = it->function;

			if (function == nullptr)
				continue;

			if (function->parameter_list.empty())
			{
				if (arguments.empty())
				{
					out_data.id = it->id;
					out_data.type = function->return_type;
					out_data.function = result = function;
					num_overloads = 1;
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

			// A new possibly-matching function was found, compare it against the current result
			const int comparison = compare_functions(arguments, function, result);

			if (comparison < 0) // The new function is a better match
			{
				out_data.id = it->id;
				out_data.type = function->return_type;
				out_data.function = result = function;
				num_overloads = 1;
				overload_namespace = it->scope.namespace_level;
			}
			else if (comparison == 0 && overload_namespace == it->scope.namespace_level) // Both functions are equally viable, so the call is ambiguous
			{
				++num_overloads;
			}
		}
	}

	// Try matching against intrinsic functions if no matching user-defined function was found up to this point
	if (num_overloads == 0)
	{
		for (const intrinsic &intrinsic : s_intrinsics)
		{
			if (intrinsic.function.name != name || intrinsic.function.parameter_list.size() != arguments.size())
				continue;

			// A new possibly-matching intrinsic function was found, compare it against the current result
			const int comparison = compare_functions(arguments, &intrinsic.function, result);

			if (comparison < 0) // The new function is a better match
			{
				out_data.op = symbol_type::intrinsic;
				out_data.id = intrinsic.id;
				out_data.type = intrinsic.function.return_type;
				out_data.function = &intrinsic.function;
				result = out_data.function;
				num_overloads = 1;
			}
			else if (comparison == 0 && overload_namespace == 0) // Both functions are equally viable, so the call is ambiguous (intrinsics are always in the global namespace)
			{
				++num_overloads;
			}
		}
	}

	is_ambiguous = num_overloads > 1;

	return num_overloads == 1;
}
