/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_lexer.hpp"
#include <algorithm>

namespace reshadefx
{
	/// <summary>
	/// A code generation back-end interface for the parser to call into.
	/// </summary>
	class codegen abstract
	{
	public:
		/// <summary>
		/// An opaque ID referring to a SSA value or basic block.
		/// </summary>
		using id = uint32_t;

		/// <summary>
		/// Enumeration with all existing code generation implementations.
		/// </summary>
		enum class backend
		{
			glsl,
			hlsl,
			spirv,
		};

		/// <summary>
		/// Define a new struct type.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The struct type description.</param>
		/// <returns>New SSA ID of the type.</returns>
		virtual id  define_struct(const location &loc, struct_info &info) = 0;
		/// <summary>
		/// Define a new texture binding.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The texture description.</param>
		/// <returns>New SSA ID of the binding.</returns>
		virtual id  define_texture(const location &loc, texture_info &info) = 0;
		/// <summary>
		/// Define a new sampler binding.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The sampler description.</param>
		/// <returns>New SSA ID of the binding.</returns>
		virtual id  define_sampler(const location &loc, sampler_info &info) = 0;
		/// <summary>
		/// Define a new uniform variable.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The uniform variable description.</param>
		/// <returns>New SSA ID of the variable.</returns>
		virtual id  define_uniform(const location &loc, uniform_info &info) = 0;
		/// <summary>
		/// Define a new variable.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The variable description.</param>
		/// <returns>New SSA ID of the variable.</returns>
		virtual id  define_variable(const location &loc, const type &type, std::string name = std::string(), bool global = false, id initializer_value = 0) = 0;
		/// <summary>
		/// Define a new function and its function parameters and make it current. Any code added after this call is added to this function.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The function description.</param>
		/// <returns>New SSA ID of the function.</returns>
		virtual id  define_function(const location &loc, function_info &info) = 0;
		/// <summary>
		/// Define a new effect technique.
		/// </summary>
		/// <param name="loc">Source location matching this definition (for debugging).</param>
		/// <param name="info">The technique description.</param>
		inline void define_technique(technique_info &info)
		{
			_module.techniques.push_back(info);
		}

		/// <summary>
		/// Create a new basic block.
		/// </summary>
		/// <returns>New SSA ID of the basic block.</returns>
		virtual id   create_block() = 0;

		/// <summary>
		/// Make a function a shader entry point.
		/// </summary>
		/// <param name="function">The function to use as entry point.</param>
		/// <param name="is_ps"><c>true</c> if this is a pixel shader, <c>false</c> if it is a vertex shader.</param>
		/// <returns>New SSA ID of the shader entry point.</returns>
		virtual void create_entry_point(const function_info &function, bool is_ps) = 0;

		/// <summary>
		/// Resolve the access chain and add a load operation to the output.
		/// </summary>
		/// <param name="chain">The access chain pointing to the variable to load from.</param>
		/// <returns>New SSA ID with the loaded value.</returns>
		virtual id   emit_load(const expression &chain) = 0;
		/// <summary>
		/// Resolve the access chain and add a store operation to the output.
		/// </summary>
		/// <param name="chain">The access chain pointing to the variable to store to.</param>
		/// <param name="value">The SSA ID of the value to store.</param>
		virtual void emit_store(const expression &chain, id value) = 0;

		/// <summary>
		/// Create a SSA constant value.
		/// </summary>
		/// <param name="type">The data type of the constant.</param>
		/// <param name="data">The actual constant data to convert into a SSA ID.</param>
		/// <returns>New SSA ID with the constant value.</returns>
		virtual id   emit_constant(const type &type, const constant &data) = 0;

		/// <summary>
		/// Add an unary operation to the output (built-in operation with one argument).
		/// </summary>
		/// <param name="loc">Source location matching this operation (for debugging).</param>
		/// <param name="op">The unary operator to use.</param>
		/// <param name="type">The data type of the input value.</param>
		/// <param name="val">The SSA ID of value to perform the operation on.</param>
		/// <returns>New SSA ID with the result of the operation.</returns>
		virtual id   emit_unary_op(const location &loc, tokenid op, const type &type, id val) = 0;
		/// <summary>
		/// Add a binary operation to the output (built-in operation with two arguments).
		/// </summary>
		/// <param name="loc">Source location matching this operation (for debugging).</param>
		/// <param name="op">The binary operator to use.</param>
		/// <param name="res_type">The data type of the result.</param>
		/// <param name="type">The data type of the input values.</param>
		/// <param name="lhs">The SSA ID of the value on the left-hand side of the binary operation.</param>
		/// <param name="rhs">The SSA ID of the value on the right-hand side of the binary operation.</param>
		/// <returns>New SSA ID with the result of the operation.</returns>
		virtual id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &type, id lhs, id rhs) = 0;
		        id   emit_binary_op(const location &loc, tokenid op, const type &type, id lhs, id rhs) { return emit_binary_op(loc, op, type, type, lhs, rhs); }
		/// <summary>
		/// Add a ternary operation to the output (built-in operation with three arguments).
		/// </summary>
		/// <param name="loc">Source location matching this operation (for debugging).</param>
		/// <param name="op">The ternary operator to use.</param>
		/// <param name="type">The data type of the input values.</param>
		/// <param name="condition">The SSA ID of the condition value of the ternary operation.</param>
		/// <param name="true_value">The SSA ID of the first value of the ternary operation.</param>
		/// <param name="false_value">The SSA ID of the second value of the ternary operation.</param>
		/// <returns>New SSA ID with the result of the operation.</returns>
		virtual id   emit_ternary_op(const location &loc, tokenid op, const type &type, id condition, id true_value, id false_value) = 0;
		/// <summary>
		/// Add a function call to the output.
		/// </summary>
		/// <param name="loc">Source location matching this operation (for debugging).</param>
		/// <param name="function">The SSA ID of the function to call.</param>
		/// <param name="res_type">The data type of the call result.</param>
		/// <param name="args">A list of SSA IDs representing the call arguments.</param>
		/// <returns>New SSA ID with the result of the function call.</returns>
		virtual id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) = 0;
		/// <summary>
		/// Add an intrinsic function call to the output.
		/// </summary>
		/// <param name="loc">Source location matching this operation (for debugging).</param>
		/// <param name="function">The intrinsic to call.</param>
		/// <param name="res_type">The data type of the call result.</param>
		/// <param name="args">A list of SSA IDs representing the call arguments.</param>
		/// <returns>New SSA ID with the result of the function call.</returns>
		virtual id   emit_call_intrinsic(const location &loc, id function, const type &res_type, const std::vector<expression> &args) = 0;
		/// <summary>
		/// Add a type constructor call to the output.
		/// </summary>
		/// <param name="type">The data type to construct.</param>
		/// <param name="args">A list of SSA IDs representing the scalar constructor arguments.</param>
		/// <returns>New SSA ID with the constructed value.</returns>
		virtual id   emit_construct(const location &loc, const type &type, const std::vector<expression> &args) = 0;

		/// <summary>
		/// Add a structured branch control flow to the output.
		/// </summary>
		/// <param name="loc">Source location matching this branch (for debugging).</param>
		/// <param name="flags">0 - default, 1 - flatten, 2 - do not flatten</param>
		virtual void emit_if(const location &loc, id condition_value, id condition_block, id true_statement_block, id false_statement_block, unsigned int flags) = 0;
		/// <summary>
		/// Add a branch control flow with a SSA phi operation to the output.
		/// </summary>
		/// <param name="loc">Source location matching this branch (for debugging).</param>
		/// <returns>New SSA ID with the result of the phi operation.</returns>
		virtual id   emit_phi(const location &loc, id condition_value, id condition_block, id true_value, id true_statement_block, id false_value, id false_statement_block, const type &type) = 0;
		/// <summary>
		/// Add a structured loop control flow to the output.
		/// </summary>
		/// <param name="loc">Source location matching this loop (for debugging).</param>
		/// <param name="flags">0 - default, 1 - unroll, 2 - do not unroll</param>
		virtual void emit_loop(const location &loc, id condition_value, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, unsigned int flags) = 0;
		/// <summary>
		/// Add a structured switch control flow to the output.
		/// </summary>
		/// <param name="loc">Source location matching this switch (for debugging).</param>
		/// <param name="flags">0 - default, 1 - flatten, 2 - do not flatten</param>
		virtual void emit_switch(const location &loc, id selector_value, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, unsigned int flags) = 0;

		/// <summary>
		/// Returns true if code is currently added to a basic block.
		/// </summary>
		virtual bool is_in_block() const = 0;
		/// <summary>
		/// Returns true if code is currently added to a function.
		/// </summary>
		virtual bool is_in_function() const = 0;

		/// <summary>
		/// Overwrite the current block ID.
		/// </summary>
		/// <param name="id">The ID of the block to make current.</param>
		/// <returns>The ID of the previous basic block.</returns>
		virtual id   set_block(id id) = 0;
		/// <summary>
		/// Create a new basic block and make it current.
		/// </summary>
		/// <param name="id">The ID of the basic block to create and make current.</param>
		virtual void enter_block(id id) = 0;
		/// <summary>
		/// Return from the current basic block and kill the shader invocation.
		/// </summary>
		/// <returns>The ID of the current basic block.</returns>
		virtual id   leave_block_and_kill() = 0;
		/// <summary>
		/// Return from the current basic block and hand control flow over to the function call side.
		/// </summary>
		/// <param name="value">Optional SSA ID of a return value.</param>
		/// <returns>The ID of the current basic block.</returns>
		virtual id   leave_block_and_return(id value = 0) = 0;
		/// <summary>
		/// Diverge the current control flow and enter a switch.
		/// </summary>
		/// <param name="value">SSA ID of the selector value to decide the switch path.</param>
		/// <returns>The ID of the current basic block.</returns>
		virtual id   leave_block_and_switch(id value, id default_target) = 0;
		/// <summary>
		/// Diverge the current control flow and jump to the specified target block.
		/// </summary>
		/// <param name="target">The ID of the basic block to jump to.</param>
		/// <param name="is_continue">True if this corresponds to a loop continue statement.</param>
		/// <returns>The ID of the current basic block.</returns>
		virtual id   leave_block_and_branch(id target, unsigned int loop_flow = 0) = 0;
		/// <summary>
		/// Diverge the current control flow and jump to one of the specified target blocks, depending on the condition.
		/// </summary>
		/// <param name="condition">The SSA ID of a value used to choose which path to take.</param>
		/// <param name="true_target">The ID of the basic block to jump to when the condition is true.</param>
		/// <param name="false_target">The ID of the basic block to jump to when the condition is false.</param>
		/// <returns>The ID of the current basic block.</returns>
		virtual id   leave_block_and_branch_conditional(id condition, id true_target, id false_target) = 0;
		/// <summary>
		/// Leave the current function. Any code added after this call is added in the global scope.
		/// </summary>
		virtual void leave_function() = 0;

		/// <summary>
		/// Write result of the code generation to the specified <paramref name="module"/>.
		/// </summary>
		/// <param name="module">The target module to fill.</param>
		virtual void write_result(module &module) = 0;

		/// <summary>
		/// Look up an existing struct definition.
		/// </summary>
		/// <param name="id">The SSA ID of the struct type to find.</param>
		/// <returns>A reference to the struct description.</returns>
		inline struct_info &find_struct(id id)
		{
			return *std::find_if(_structs.begin(), _structs.end(),
				[id](const auto &it) { return it.definition == id; });
		}
		/// <summary>
		/// Look up an existing texture definition.
		/// </summary>
		/// <param name="id">The SSA ID of the texture variable to find.</param>
		/// <returns>A reference to the texture description.</returns>
		inline texture_info &find_texture(id id)
		{
			return *std::find_if(_module.textures.begin(), _module.textures.end(),
				[id](const auto &it) { return it.id == id; });
		}
		/// <summary>
		/// Look up an existing function definition.
		/// </summary>
		/// <param name="id">The SSA ID of the function variable to find.</param>
		/// <returns>A reference to the function description.</returns>
		inline function_info &find_function(id id)
		{
			return *std::find_if(_functions.begin(), _functions.end(),
				[id](const auto &it) { return it->definition == id; })->get();
		}

	protected:
		module _module;
		std::vector<struct_info> _structs;
		std::vector<std::unique_ptr<function_info>> _functions;
	};
}
