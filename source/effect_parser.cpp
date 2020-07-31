/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_lexer.hpp"
#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <cassert>
#include <algorithm>
#include <functional>

struct on_scope_exit
{
	template <typename F>
	on_scope_exit(F lambda) : leave(lambda) { }
	~on_scope_exit() { leave(); }
	std::function<void()> leave;
};

reshadefx::parser::parser()
{
}
reshadefx::parser::~parser()
{
}

bool reshadefx::parser::parse(std::string input, codegen *backend)
{
	_lexer.reset(new lexer(std::move(input)));

	// Set backend for subsequent code-generation
	_codegen = backend;

	consume();

	bool parse_success = true;

	while (!peek(tokenid::end_of_file))
	{
		if (!parse_top())
			parse_success = false;
	}

	return parse_success;
}

// -- Error Handling -- //

void reshadefx::parser::error(const location &location, unsigned int code, const std::string &message)
{
	if (_errors.size() > 1000)
		return; // Stop printing any more errors after a certain amount

	_errors += location.source;
	_errors += '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": error";
	_errors += (code == 0) ? ": " : " X" + std::to_string(code) + ": ";
	_errors += message;
	_errors += '\n';
}
void reshadefx::parser::warning(const location &location, unsigned int code, const std::string &message)
{
	_errors += location.source;
	_errors += '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": warning";
	_errors += (code == 0) ? ": " : " X" + std::to_string(code) + ": ";
	_errors += message;
	_errors += '\n';
}

// -- Token Management -- //

void reshadefx::parser::backup()
{
	_token_backup = _token_next;
	_lexer_backup_offset = _lexer->input_offset();
}
void reshadefx::parser::restore()
{
	_lexer->reset_to_offset(_lexer_backup_offset);
	_token_next = _token_backup; // Copy instead of move here, since restore may be called twice (from 'accept_type_class' and then again from 'parse_expression_unary')
}

void reshadefx::parser::consume()
{
	_token = std::move(_token_next);
	_token_next = _lexer->lex();
}
void reshadefx::parser::consume_until(tokenid tokid)
{
	while (!accept(tokid) && !peek(tokenid::end_of_file))
	{
		consume();
	}
}

bool reshadefx::parser::accept(tokenid tokid)
{
	if (peek(tokid))
	{
		consume();
		return true;
	}

	return false;
}
bool reshadefx::parser::expect(tokenid tokid)
{
	if (!accept(tokid))
	{
		error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected '" + token::id_to_name(tokid) + '\'');
		return false;
	}

	return true;
}

// -- Type Parsing -- //

bool reshadefx::parser::accept_symbol(std::string &identifier, scope &scope, symbol &symbol)
{
	// Starting an identifier with '::' restricts the symbol search to the global namespace level
	const bool exclusive = accept(tokenid::colon_colon);

	if (exclusive ? !expect(tokenid::identifier) : !accept(tokenid::identifier))
	{
		// No token should come through here, since all possible prefix expressions should have been handled above, so this is an error in the syntax
		if (!exclusive)
			error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + '\'');
		return false;
	}

	identifier = std::move(_token.literal_as_string);

	// Can concatenate multiple '::' to force symbol search for a specific namespace level
	while (accept(tokenid::colon_colon))
	{
		if (!expect(tokenid::identifier))
			return false;
		identifier += "::" + std::move(_token.literal_as_string);
	}

	// Figure out which scope to start searching in
	scope = { "::", 0, 0 };
	if (!exclusive) scope = current_scope();

	// Lookup name in the symbol table
	symbol = find_symbol(identifier, scope, exclusive);

	return true;
}

bool reshadefx::parser::accept_type_class(type &type)
{
	type.rows = type.cols = 0;

	if (peek(tokenid::identifier) || peek(tokenid::colon_colon))
	{
		type.base = type::t_struct;

		backup(); // Need to restore if this identifier does not turn out to be a structure

		scope scope; symbol symbol;
		if (std::string identifier; accept_symbol(identifier, scope, symbol))
		{
			if (symbol.id && symbol.op == symbol_type::structure)
			{
				type.definition = symbol.id;
				return true;
			}
		}

		restore();

		return false;
	}
	else if (accept(tokenid::vector))
	{
		type.base = type::t_float; // Default to float4 unless a type is specified (see below)
		type.rows = 4, type.cols = 1;

		if (accept('<'))
		{
			if (!accept_type_class(type)) // This overwrites the base type again
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected vector element type"), false;
			else if (!type.is_scalar())
				return error(_token.location, 3122, "vector element type must be a scalar type"), false;

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3052, "vector dimension must be between 1 and 4"), false;

			type.rows = static_cast<unsigned int>(_token.literal_as_int);

			if (!expect('>'))
				return false;
		}

		return true;
	}
	else if (accept(tokenid::matrix))
	{
		type.base = type::t_float; // Default to float4x4 unless a type is specified (see below)
		type.rows = 4, type.cols = 4;

		if (accept('<'))
		{
			if (!accept_type_class(type)) // This overwrites the base type again
				return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected matrix element type"), false;
			else if (!type.is_scalar())
				return error(_token.location, 3123, "matrix element type must be a scalar type"), false;

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

			type.rows = static_cast<unsigned int>(_token.literal_as_int);

			if (!expect(',') || !expect(tokenid::int_literal))
				return false;
			else if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

			type.cols = static_cast<unsigned int>(_token.literal_as_int);

			if (!expect('>'))
				return false;
		}

		return true;
	}

	switch (_token_next.id)
	{
	case tokenid::void_:
		type.base = type::t_void;
		break;
	case tokenid::bool_:
	case tokenid::bool2:
	case tokenid::bool3:
	case tokenid::bool4:
		type.base = type::t_bool;
		type.rows = 1 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::bool_);
		type.cols = 1;
		break;
	case tokenid::bool2x2:
	case tokenid::bool3x3:
	case tokenid::bool4x4:
		type.base = type::t_bool;
		type.rows = 2 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::bool2x2);
		type.cols = type.rows;
		break;
	case tokenid::int_:
	case tokenid::int2:
	case tokenid::int3:
	case tokenid::int4:
		type.base = type::t_int;
		type.rows = 1 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::int_);
		type.cols = 1;
		break;
	case tokenid::int2x2:
	case tokenid::int3x3:
	case tokenid::int4x4:
		type.base = type::t_int;
		type.rows = 2 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::int2x2);
		type.cols = type.rows;
		break;
	case tokenid::uint_:
	case tokenid::uint2:
	case tokenid::uint3:
	case tokenid::uint4:
		type.base = type::t_uint;
		type.rows = 1 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::uint_);
		type.cols = 1;
		break;
	case tokenid::uint2x2:
	case tokenid::uint3x3:
	case tokenid::uint4x4:
		type.base = type::t_uint;
		type.rows = 2 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::uint2x2);
		type.cols = type.rows;
		break;
	case tokenid::float_:
	case tokenid::float2:
	case tokenid::float3:
	case tokenid::float4:
		type.base = type::t_float;
		type.rows = 1 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::float_);
		type.cols = 1;
		break;
	case tokenid::float2x2:
	case tokenid::float3x3:
	case tokenid::float4x4:
		type.base = type::t_float;
		type.rows = 2 + static_cast<unsigned int>(_token_next.id) - static_cast<unsigned int>(tokenid::float2x2);
		type.cols = type.rows;
		break;
	case tokenid::string_:
		type.base = type::t_string;
		break;
	case tokenid::texture:
		type.base = type::t_texture;
		break;
	case tokenid::sampler:
		type.base = type::t_sampler;
		break;
	case tokenid::storage:
		type.base = type::t_storage;
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_type_qualifiers(type &type)
{
	unsigned int qualifiers = 0;

	// Storage
	if (accept(tokenid::extern_))
		qualifiers |= type::q_extern;
	if (accept(tokenid::static_))
		qualifiers |= type::q_static;
	if (accept(tokenid::uniform_))
		qualifiers |= type::q_uniform;
	if (accept(tokenid::volatile_))
		qualifiers |= type::q_volatile;
	if (accept(tokenid::precise))
		qualifiers |= type::q_precise;
	if (accept(tokenid::groupshared))
		qualifiers |= type::q_groupshared;

	if (accept(tokenid::in))
		qualifiers |= type::q_in;
	if (accept(tokenid::out))
		qualifiers |= type::q_out;
	if (accept(tokenid::inout))
		qualifiers |= type::q_inout;

	// Modifiers
	if (accept(tokenid::const_))
		qualifiers |= type::q_const;

	// Interpolation
	if (accept(tokenid::linear))
		qualifiers |= type::q_linear;
	if (accept(tokenid::noperspective))
		qualifiers |= type::q_noperspective;
	if (accept(tokenid::centroid))
		qualifiers |= type::q_centroid;
	if (accept(tokenid::nointerpolation))
		qualifiers |= type::q_nointerpolation;

	if (qualifiers == 0)
		return false;
	if ((type.qualifiers & qualifiers) == qualifiers)
		warning(_token.location, 3048, "duplicate usages specified");

	type.qualifiers |= qualifiers;

	// Continue parsing potential additional qualifiers until no more are found
	accept_type_qualifiers(type);

	return true;
}

bool reshadefx::parser::parse_type(type &type)
{
	type.qualifiers = 0;

	accept_type_qualifiers(type);

	if (!accept_type_class(type))
		return false;

	if (type.is_integral() && (type.has(type::q_centroid) || type.has(type::q_noperspective)))
		return error(_token.location, 4576, "signature specifies invalid interpolation mode for integer component type"), false;
	else if (type.has(type::q_centroid) && !type.has(type::q_noperspective))
		type.qualifiers |= type::q_linear;

	return true;
}

bool reshadefx::parser::parse_array_size(type &type)
{
	// Reset array length to zero before checking if one exists
	type.array_length = 0;

	if (accept('['))
	{
		if (accept(']'))
		{
			// No length expression, so this is an unsized array
			type.array_length = -1;
		}
		else if (expression expression; parse_expression(expression) && expect(']'))
		{
			if (!expression.is_constant || !(expression.type.is_scalar() && expression.type.is_integral()))
				return error(expression.location, 3058, "array dimensions must be literal scalar expressions"), false;

			type.array_length = expression.constant.as_uint[0];

			if (type.array_length < 1 || type.array_length > 65536)
				return error(expression.location, 3059, "array dimension must be between 1 and 65536"), false;
		}
		else
		{
			return false;
		}
	}

	// Multi-dimensional arrays are not supported
	if (peek('['))
		return error(_token_next.location, 3119, "arrays cannot be multi-dimensional"), false;

	return true;
}

// -- Expression Parsing -- //

bool reshadefx::parser::accept_unary_op()
{
	switch (_token_next.id)
	{
	case tokenid::exclaim: // !x (logical not)
	case tokenid::plus: // +x
	case tokenid::minus: // -x (negate)
	case tokenid::tilde: // ~x (bitwise not)
	case tokenid::plus_plus: // ++x
	case tokenid::minus_minus: // --x
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::accept_postfix_op()
{
	switch (_token_next.id)
	{
	case tokenid::plus_plus: // ++x
	case tokenid::minus_minus: // --x
		break;
	default:
		return false;
	}

	consume();

	return true;
}
bool reshadefx::parser::peek_multary_op(unsigned int &precedence) const
{
	// Precedence values taken from https://cppreference.com/w/cpp/language/operator_precedence
	switch (_token_next.id)
	{
	case tokenid::question: precedence = 1; break; // x ? a : b
	case tokenid::pipe_pipe: precedence = 2; break; // a || b (logical or)
	case tokenid::ampersand_ampersand: precedence = 3; break; // a && b (logical and)
	case tokenid::pipe: precedence = 4; break; // a | b (bitwise or)
	case tokenid::caret: precedence = 5; break; // a ^ b (bitwise xor)
	case tokenid::ampersand: precedence = 6; break; // a & b (bitwise and)
	case tokenid::equal_equal: precedence = 7; break; // a == b (equal)
	case tokenid::exclaim_equal: precedence = 7; break; // a != b (not equal)
	case tokenid::less: precedence = 8; break; // a < b
	case tokenid::greater: precedence = 8; break; // a > b
	case tokenid::less_equal: precedence = 8; break; // a <= b
	case tokenid::greater_equal: precedence = 8; break; // a >= b
	case tokenid::less_less: precedence = 9; break; // a << b (left shift)
	case tokenid::greater_greater: precedence = 9; break; // a >> b (right shift)
	case tokenid::plus: precedence = 10; break; // a + b (add)
	case tokenid::minus: precedence = 10; break; // a - b (subtract)
	case tokenid::star: precedence = 11; break; // a * b (multiply)
	case tokenid::slash: precedence = 11; break; // a / b (divide)
	case tokenid::percent: precedence = 11; break; // a % b (modulo)
	default:
		return false;
	}

	// Do not consume token yet since the expression may be skipped due to precedence
	return true;
}
bool reshadefx::parser::accept_assignment_op()
{
	switch (_token_next.id)
	{
	case tokenid::equal: // a = b
	case tokenid::percent_equal: // a %= b
	case tokenid::ampersand_equal: // a &= b
	case tokenid::star_equal: // a *= b
	case tokenid::plus_equal: // a += b
	case tokenid::minus_equal: // a -= b
	case tokenid::slash_equal: // a /= b
	case tokenid::less_less_equal: // a <<= b
	case tokenid::greater_greater_equal: // a >>= b
	case tokenid::caret_equal: // a ^= b
	case tokenid::pipe_equal: // a |= b
		break;
	default:
		return false;
	}

	consume();

	return true;
}

bool reshadefx::parser::parse_expression(expression &exp)
{
	// Parse first expression
	if (!parse_expression_assignment(exp))
		return false;

	// Continue parsing if an expression sequence is next (in the form "a, b, c, ...")
	while (accept(','))
		// Overwrite 'exp' since conveniently the last expression in the sequence is the result
		if (!parse_expression_assignment(exp))
			return false;

	return true;
}

bool reshadefx::parser::parse_expression_unary(expression &exp)
{
	auto location = _token_next.location;

	#pragma region Prefix Expression
	// Check if a prefix operator exists
	if (accept_unary_op())
	{
		// Remember the operator token before parsing the expression that follows it
		const tokenid op = _token.id;

		// Parse the actual expression
		if (!parse_expression_unary(exp))
			return false;

		// Unary operators are only valid on basic types
		if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
			return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;

		// Special handling for the "++" and "--" operators
		if (op == tokenid::plus_plus || op == tokenid::minus_minus)
		{
			if (exp.type.has(type::q_const) || exp.type.has(type::q_uniform) || !exp.is_lvalue)
				return error(location, 3025, "l-value specifies const object"), false;

			// Create a constant one in the type of the expression
			constant one = {};
			for (unsigned int i = 0; i < exp.type.components(); ++i)
				if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] = 1u;

			const auto value = _codegen->emit_load(exp);
			const auto result = _codegen->emit_binary_op(location, op, exp.type, value,
				_codegen->emit_constant(exp.type, one));

			// The "++" and "--" operands modify the source variable, so store result back into it
			_codegen->emit_store(exp, result);
		}
		else if (op != tokenid::plus) // Ignore "+" operator since it does not actually do anything
		{
			// The "~" bitwise operator is only valid on integral types
			if (op == tokenid::tilde && !exp.type.is_integral())
				return error(exp.location, 3082, "int or unsigned int type required"), false;
			// The logical not operator expects a boolean type as input, so perform cast if necessary
			if (op == tokenid::exclaim && !exp.type.is_boolean())
				exp.add_cast_operation({ type::t_bool, exp.type.rows, exp.type.cols }); // Note: The result will be boolean as well

			// Constant expressions can be evaluated at compile time
			if (!exp.evaluate_constant_expression(op))
			{
				const auto value = _codegen->emit_load(exp);
				const auto result = _codegen->emit_unary_op(location, op, exp.type, value);

				exp.reset_to_rvalue(location, result, exp.type);
			}
		}
	}
	else if (accept('('))
	{
		// Note: This backup may get overridden in 'accept_type_class', but should point to the same token still
		backup();

		// Check if this is a C-style cast expression
		if (type cast_type; accept_type_class(cast_type))
		{
			if (peek('('))
			{
				// This is not a C-style cast but a constructor call, so need to roll-back and parse that instead
				restore();
			}
			else if (expect(')'))
			{
				// Parse the expression behind cast operator
				if (!parse_expression_unary(exp))
					return false;

				// Check if the types already match, in which case there is nothing to do
				if (exp.type == cast_type)
					return true;

				// Check if a cast between these types is valid
				if (!type::rank(exp.type, cast_type))
					return error(location, 3017, "cannot convert these types (from " + exp.type.description() + " to " + cast_type.description() + ')'), false;

				exp.add_cast_operation(cast_type);
				return true;
			}
			else
			{
				// Type name was not followed by a closing parenthesis
				return false;
			}
		}

		// Parse expression between the parentheses
		if (!parse_expression(exp) || !expect(')'))
			return false;
	}
	else if (accept('{'))
	{
		bool is_constant = true;
		std::vector<expression> elements;
		type composite_type = { type::t_void, 1, 1 };

		while (!peek('}'))
		{
			// There should be a comma between arguments
			if (!elements.empty() && !expect(','))
				return consume_until('}'), false;

			// Initializer lists might contain a comma at the end, so break out of the loop if nothing follows afterwards
			if (peek('}'))
				break;

			// Parse the argument expression
			if (!parse_expression_assignment(elements.emplace_back()))
				return consume_until('}'), false;

			expression &element = elements.back();

			if (element.type.is_array())
				return error(element.location, 3119, "arrays cannot be multi-dimensional"), consume_until('}'), false;
			if (composite_type.base != type::t_void && element.type.definition != composite_type.definition)
				return error(element.location, 3017, "cannot convert these types (from " + element.type.description() + " to " + composite_type.description() + ')'), false;

			is_constant &= element.is_constant; // Result is only constant if all arguments are constant
			composite_type = type::merge(composite_type, element.type);
		}

		// Constant arrays can be constructed at compile time
		if (is_constant)
		{
			constant res = {};
			for (expression &element : elements)
			{
				element.add_cast_operation(composite_type);
				res.array_data.push_back(element.constant);
			}

			composite_type.array_length = static_cast<int>(elements.size());

			exp.reset_to_rvalue_constant(location, std::move(res), composite_type);
		}
		else
		{
			composite_type.array_length = static_cast<int>(elements.size());

			// Resolve all access chains
			for (expression &element : elements)
			{
				element.reset_to_rvalue(element.location, _codegen->emit_load(element), element.type);
			}

			const auto result = _codegen->emit_construct(location, composite_type, elements);

			exp.reset_to_rvalue(location, result, composite_type);
		}

		return expect('}');
	}
	else if (accept(tokenid::true_literal))
	{
		exp.reset_to_rvalue_constant(location, true);
	}
	else if (accept(tokenid::false_literal))
	{
		exp.reset_to_rvalue_constant(location, false);
	}
	else if (accept(tokenid::int_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_int);
	}
	else if (accept(tokenid::uint_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_uint);
	}
	else if (accept(tokenid::float_literal))
	{
		exp.reset_to_rvalue_constant(location, _token.literal_as_float);
	}
	else if (accept(tokenid::double_literal))
	{
		// Convert double literal to float literal for now
		warning(location, 5000, "double literal truncated to float literal");

		exp.reset_to_rvalue_constant(location, static_cast<float>(_token.literal_as_double));
	}
	else if (accept(tokenid::string_literal))
	{
		std::string value = std::move(_token.literal_as_string);

		// Multiple string literals in sequence are concatenated into a single string literal
		while (accept(tokenid::string_literal))
			value += _token.literal_as_string;

		exp.reset_to_rvalue_constant(location, std::move(value));
	}
	else if (type type; accept_type_class(type)) // Check if this is a constructor call expression
	{
		if (!expect('('))
			return false;
		if (!type.is_numeric())
			return error(location, 3037, "constructors only defined for numeric base types"), false;

		// Empty constructors do not exist
		if (accept(')'))
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		// Parse entire argument expression list
		bool is_constant = true;
		unsigned int num_components = 0;
		std::vector<expression> arguments;

		while (!peek(')'))
		{
			// There should be a comma between arguments
			if (!arguments.empty() && !expect(','))
				return false;

			// Parse the argument expression
			if (!parse_expression_assignment(arguments.emplace_back()))
				return false;

			expression &argument = arguments.back();

			// Constructors are only defined for numeric base types
			if (!argument.type.is_numeric())
				return error(argument.location, 3017, "cannot convert non-numeric types"), false;

			is_constant &= argument.is_constant; // Result is only constant if all arguments are constant
			num_components += argument.type.components();
		}

		// The list should be terminated with a parenthesis
		if (!expect(')'))
			return false;

		// The total number of argument elements needs to match the number of elements in the result type
		if (num_components != type.components())
			return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

		assert(num_components > 0 && num_components <= 16 && !type.is_array());

		if (is_constant) // Constants can be converted at compile time
		{
			constant res = {};
			unsigned int i = 0;
			for (expression &argument : arguments)
			{
				argument.add_cast_operation({ type.base, argument.type.rows, argument.type.cols });
				for (unsigned int k = 0; k < argument.type.components(); ++k)
					res.as_uint[i++] = argument.constant.as_uint[k];
			}

			exp.reset_to_rvalue_constant(location, std::move(res), type);
		}
		else if (arguments.size() > 1)
		{
			// Flatten all arguments to a list of scalars
			for (auto it = arguments.begin(); it != arguments.end();)
			{
				// Argument is a scalar already, so only need to cast it
				if (it->type.is_scalar())
				{
					expression &argument = *it++;

					auto scalar_type = argument.type;
					scalar_type.base = type.base;
					argument.add_cast_operation(scalar_type);

					argument.reset_to_rvalue(argument.location, _codegen->emit_load(argument), scalar_type);
				}
				else
				{
					const expression argument = *it;
					it = arguments.erase(it);

					// Convert to a scalar value and re-enter the loop in the next iteration (in case a cast is necessary too)
					for (unsigned int i = argument.type.components(); i > 0; --i)
					{
						expression scalar = argument;
						scalar.add_constant_index_access(i - 1);

						it = arguments.insert(it, scalar);
					}
				}
			}

			const auto result = _codegen->emit_construct(location, type, arguments);

			exp.reset_to_rvalue(location, result, type);
		}
		else // A constructor call with a single argument is identical to a cast
		{
			assert(!arguments.empty());

			// Reset expression to only argument and add cast to expression access chain
			exp = std::move(arguments[0]); exp.add_cast_operation(type);
		}
	}
	// At this point only identifiers are left to check and resolve
	else
	{
		scope scope;
		symbol symbol;
		std::string identifier;
		if (!accept_symbol(identifier, scope, symbol))
			return false;

		// Check if this is a function call or variable reference
		if (accept('('))
		{
			// Can only call symbols that are functions, but do not abort yet if no symbol was found since the identifier may reference an intrinsic
			if (symbol.id && symbol.op != symbol_type::function)
				return error(location, 3005, "identifier '" + identifier + "' represents a variable, not a function"), false;

			// Parse entire argument expression list
			std::vector<expression> arguments;

			while (!peek(')'))
			{
				// There should be a comma between arguments
				if (!arguments.empty() && !expect(','))
					return false;

				// Parse the argument expression
				if (!parse_expression_assignment(arguments.emplace_back()))
					return false;
			}

			// The list should be terminated with a parenthesis
			if (!expect(')'))
				return false;

			// Function calls can only be made from within functions
			if (!_codegen->is_in_function())
				return error(location, 3005, "invalid function call outside of a function"), false;

			// Try to resolve the call by searching through both function symbols and intrinsics
			bool undeclared = !symbol.id, ambiguous = false;

			if (!resolve_function_call(identifier, arguments, scope, symbol, ambiguous))
			{
				if (undeclared)
					error(location, 3004, "undeclared identifier or no matching intrinsic overload for '" + identifier + '\'');
				else if (ambiguous)
					error(location, 3067, "ambiguous function call to '" + identifier + '\'');
				else
					error(location, 3013, "no matching function overload for '" + identifier + '\'');
				return false;
			}

			assert(symbol.function != nullptr);

			std::vector<expression> parameters(arguments.size());

			// We need to allocate some temporary variables to pass in and load results from pointer parameters
			for (size_t i = 0; i < arguments.size(); ++i)
			{
				const auto &param_type = symbol.function->parameter_list[i].type;

				if (param_type.has(type::q_out) && (arguments[i].type.has(type::q_const) || arguments[i].type.has(type::q_uniform) || !arguments[i].is_lvalue))
					return error(arguments[i].location, 3025, "l-value specifies const object for an 'out' parameter"), false;

				if (arguments[i].type.components() > param_type.components())
					warning(arguments[i].location, 3206, "implicit truncation of vector type");

				arguments[i].add_cast_operation(param_type);

				if (symbol.op == symbol_type::function || param_type.has(type::q_out))
				{
					if (param_type.is_sampler() || param_type.is_storage())
					{
						// Do not shadow sampler parameters to function calls (but do load them for intrinsics)
						parameters[i] = arguments[i];
					}
					else
					{
						// All user-defined functions actually accept pointers as arguments, same applies to intrinsics with 'out' parameters
						const auto temp_variable = _codegen->define_variable(arguments[i].location, param_type);
						parameters[i].reset_to_lvalue(arguments[i].location, temp_variable, param_type);
					}
				}
				else
				{
					parameters[i].reset_to_rvalue(arguments[i].location, _codegen->emit_load(arguments[i]), param_type);

					// Keep track of whether the parameter is a constant for code generation (this makes the expression invalid for all other uses)
					parameters[i].is_constant = arguments[i].is_constant;
				}
			}

			// Copy in parameters from the argument access chains to parameter variables
			for (size_t i = 0; i < arguments.size(); ++i)
				// Only do this for pointer parameters as discovered above
				if (parameters[i].is_lvalue && parameters[i].type.has(type::q_in) && !parameters[i].type.is_sampler() && !parameters[i].type.is_storage())
					_codegen->emit_store(parameters[i], _codegen->emit_load(arguments[i]));

			// Check if the call resolving found an intrinsic or function and invoke the corresponding code
			const auto result = symbol.op == symbol_type::function ?
				_codegen->emit_call(location, symbol.id, symbol.type, parameters) :
				_codegen->emit_call_intrinsic(location, symbol.id, symbol.type, parameters);

			exp.reset_to_rvalue(location, result, symbol.type);

			// Copy out parameters from parameter variables back to the argument access chains
			for (size_t i = 0; i < arguments.size(); ++i)
				// Only do this for pointer parameters as discovered above
				if (parameters[i].is_lvalue && parameters[i].type.has(type::q_out) && !parameters[i].type.is_sampler() && !parameters[i].type.is_storage())
					_codegen->emit_store(arguments[i], _codegen->emit_load(parameters[i]));
		}
		else if (symbol.op == symbol_type::invalid)
		{
			// Show error if no symbol matching the identifier was found
			return error(location, 3004, "undeclared identifier '" + identifier + '\''), false;
		}
		else if (symbol.op == symbol_type::variable)
		{
			assert(symbol.id != 0);
			// Simply return the pointer to the variable, dereferencing is done on site where necessary
			exp.reset_to_lvalue(location, symbol.id, symbol.type);
		}
		else if (symbol.op == symbol_type::constant)
		{
			// Constants are loaded into the access chain
			exp.reset_to_rvalue_constant(location, symbol.constant, symbol.type);
		}
		else
		{
			// Can only reference variables and constants by name, functions need to be called
			return error(location, 3005, "identifier '" + identifier + "' represents a function, not a variable"), false;
		}
	}
	#pragma endregion

	#pragma region Postfix Expression
	while (!peek(tokenid::end_of_file))
	{
		location = _token_next.location;

		// Check if a postfix operator exists
		if (accept_postfix_op())
		{
			// Unary operators are only valid on basic types
			if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;
			if (exp.type.has(type::q_const) || exp.type.has(type::q_uniform) || !exp.is_lvalue)
				return error(exp.location, 3025, "l-value specifies const object"), false;

			// Create a constant one in the type of the expression
			constant one = {};
			for (unsigned int i = 0; i < exp.type.components(); ++i)
				if (exp.type.is_floating_point()) one.as_float[i] = 1.0f; else one.as_uint[i] = 1u;

			const auto value = _codegen->emit_load(exp, true);
			const auto result = _codegen->emit_binary_op(location, _token.id, exp.type, value, _codegen->emit_constant(exp.type, one));

			// The "++" and "--" operands modify the source variable, so store result back into it
			_codegen->emit_store(exp, result);

			// All postfix operators return a r-value rather than a l-value to the variable
			exp.reset_to_rvalue(location, value, exp.type);
		}
		else if (accept('.'))
		{
			if (!expect(tokenid::identifier))
				return false;

			location = std::move(_token.location);
			const auto subscript = std::move(_token.literal_as_string);

			if (accept('(')) // Methods (function calls on types) are not supported right now
			{
				if (!exp.type.is_struct() || exp.type.is_array())
					error(location, 3087, "object does not have methods");
				else
					error(location, 3088, "structures do not have methods");
				return false;
			}
			else if (exp.type.is_array()) // Arrays do not have subscripts
			{
				error(location, 3018, "invalid subscript on array");
				return false;
			}
			else if (exp.type.is_vector())
			{
				const size_t length = subscript.size();
				if (length > 4)
					return error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long"), false;

				bool is_const = false;
				signed char offsets[4] = { -1, -1, -1, -1 };
				enum { xyzw, rgba, stpq } set[4];

				for (size_t i = 0; i < length; ++i)
				{
					switch (subscript[i])
					{
					case 'x': offsets[i] = 0, set[i] = xyzw; break;
					case 'y': offsets[i] = 1, set[i] = xyzw; break;
					case 'z': offsets[i] = 2, set[i] = xyzw; break;
					case 'w': offsets[i] = 3, set[i] = xyzw; break;
					case 'r': offsets[i] = 0, set[i] = rgba; break;
					case 'g': offsets[i] = 1, set[i] = rgba; break;
					case 'b': offsets[i] = 2, set[i] = rgba; break;
					case 'a': offsets[i] = 3, set[i] = rgba; break;
					case 's': offsets[i] = 0, set[i] = stpq; break;
					case 't': offsets[i] = 1, set[i] = stpq; break;
					case 'p': offsets[i] = 2, set[i] = stpq; break;
					case 'q': offsets[i] = 3, set[i] = stpq; break;
					default:
						return error(location, 3018, "invalid subscript '" + subscript + '\''), false;
					}

					if (i > 0 && (set[i] != set[i - 1]))
						return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;
					if (static_cast<unsigned int>(offsets[i]) >= exp.type.rows)
						return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

					// The result is not modifiable if a swizzle appears multiple times
					for (size_t k = 0; k < i; ++k)
						if (offsets[k] == offsets[i]) {
							is_const = true;
							break;
						}
				}

				// Add swizzle to current access chain
				exp.add_swizzle_access(offsets, static_cast<unsigned int>(length));

				if (is_const || exp.type.has(type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_matrix())
			{
				const size_t length = subscript.size();
				if (length < 3)
					return error(location, 3018, "invalid subscript '" + subscript + '\''), false;

				bool is_const = false;
				signed char offsets[4] = { -1, -1, -1, -1 };
				const unsigned int set = subscript[1] == 'm';
				const int coefficient = !set;

				for (size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
				{
					if (subscript[i] != '_' ||
						subscript[i + set + 1] < '0' + coefficient ||
						subscript[i + set + 1] > '3' + coefficient ||
						subscript[i + set + 2] < '0' + coefficient ||
						subscript[i + set + 2] > '3' + coefficient)
						return error(location, 3018, "invalid subscript '" + subscript + '\''), false;
					if (set && subscript[i + 1] != 'm')
						return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;

					const unsigned int row = static_cast<unsigned int>((subscript[i + set + 1] - '0') - coefficient);
					const unsigned int col = static_cast<unsigned int>((subscript[i + set + 2] - '0') - coefficient);

					if ((row >= exp.type.rows || col >= exp.type.cols) || j > 3)
						return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

					offsets[j] = static_cast<signed char>(row * 4 + col);

					// The result is not modifiable if a swizzle appears multiple times
					for (size_t k = 0; k < j; ++k)
						if (offsets[k] == offsets[j]) {
							is_const = true;
							break;
						}
				}

				// Add swizzle to current access chain
				exp.add_swizzle_access(offsets, static_cast<unsigned int>(length / (3 + set)));

				if (is_const || exp.type.has(type::q_uniform))
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_struct())
			{
				const auto &member_list = _codegen->find_struct(exp.type.definition).member_list;

				// Find member with matching name is structure definition
				uint32_t member_index = 0;
				for (const struct_member_info &member : member_list) {
					if (member.name == subscript)
						break;
					++member_index;
				}

				if (member_index >= member_list.size())
					return error(location, 3018, "invalid subscript '" + subscript + '\''), false;

				// Add field index to current access chain
				exp.add_member_access(member_index, member_list[member_index].type);

				if (exp.type.has(type::q_uniform)) // Member access to uniform structure is not modifiable
					exp.type.qualifiers = (exp.type.qualifiers | type::q_const) & ~type::q_uniform;
			}
			else if (exp.type.is_scalar())
			{
				const size_t length = subscript.size();
				if (length > 4)
					return error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long"), false;

				for (size_t i = 0; i < length; ++i)
					if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
						return error(location, 3018, "invalid subscript '" + subscript + '\''), false;

				// Promote scalar to vector type using cast
				auto target_type = exp.type;
				target_type.rows = static_cast<unsigned int>(length);

				exp.add_cast_operation(target_type);
			}
			else
			{
				error(location, 3018, "invalid subscript '" + subscript + '\'');
				return false;
			}
		}
		else if (accept('['))
		{
			if (!exp.type.is_array() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(_token.location, 3121, "array, matrix, vector, or indexable object type expected in index expression"), false;

			// Parse index expression
			expression index;
			if (!parse_expression(index) || !expect(']'))
				return false;
			else if (!index.type.is_scalar() || !index.type.is_integral())
				return error(index.location, 3120, "invalid type for index - index must be an integer scalar"), false;

			// Add index expression to current access chain
			if (index.is_constant)
			{
				// Check array bounds if known
				if (exp.type.array_length > 0 && index.constant.as_uint[0] >= static_cast<unsigned int>(exp.type.array_length))
					return error(index.location, 3504, "array index out of bounds"), false;

				exp.add_constant_index_access(index.constant.as_uint[0]);
			}
			else
			{
				if (exp.is_constant)
				{
					// To handle a dynamic index into a constant means we need to create a local variable first or else any of the indexing instructions do not work
					const auto temp_variable = _codegen->define_variable(location, exp.type, std::string(), false, _codegen->emit_constant(exp.type, exp.constant));
					exp.reset_to_lvalue(exp.location, temp_variable, exp.type);
				}

				exp.add_dynamic_index_access(_codegen->emit_load(index));
			}
		}
		else
		{
			break;
		}
	}
	#pragma endregion

	return true;
}

bool reshadefx::parser::parse_expression_multary(expression &lhs, unsigned int left_precedence)
{
	// Parse left hand side of the expression
	if (!parse_expression_unary(lhs))
		return false;

	// Check if an operator exists so that this is a binary or ternary expression
	unsigned int right_precedence;

	while (peek_multary_op(right_precedence))
	{
		// Only process this operator if it has a lower precedence than the current operation, otherwise leave it for later and abort
		if (right_precedence <= left_precedence)
			break;

		// Finally consume the operator token
		consume();

		const tokenid op = _token.id;

		// Check if this is a binary or ternary operation
		if (op != tokenid::question)
		{
			#pragma region Binary Expression
#if RESHADEFX_SHORT_CIRCUIT
			codegen::id lhs_block = 0;
			codegen::id rhs_block = 0;
			codegen::id merge_block = 0;

			// Switch block to a new one before parsing right-hand side value in case it needs to be skipped during short-circuiting
			if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
			{
				lhs_block = _codegen->set_block(0);
				rhs_block = _codegen->create_block();
				merge_block = _codegen->create_block();

				_codegen->enter_block(rhs_block);
			}
#endif
			// Parse the right hand side of the binary operation
			expression rhs;
			if (!parse_expression_multary(rhs, right_precedence))
				return false;

			// Deduce the result base type based on implicit conversion rules
			type type = type::merge(lhs.type, rhs.type);
			bool is_bool_result = false;

			// Do some error checking depending on the operator
			if (op == tokenid::equal_equal || op == tokenid::exclaim_equal)
			{
				// Equality checks return a boolean value
				is_bool_result = true;

				// Cannot check equality between incompatible types
				if (lhs.type.is_array() || rhs.type.is_array() || lhs.type.definition != rhs.type.definition)
					return error(rhs.location, 3020, "type mismatch"), false;
			}
			else if (op == tokenid::ampersand || op == tokenid::pipe || op == tokenid::caret)
			{
				// Cannot perform bitwise operations on non-integral types
				if (!lhs.type.is_integral())
					return error(lhs.location, 3082, "int or unsigned int type required"), false;
				if (!rhs.type.is_integral())
					return error(rhs.location, 3082, "int or unsigned int type required"), false;
			}
			else
			{
				if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
					type.base = type::t_bool;

				// Logical operations return a boolean value
				if (op == tokenid::less || op == tokenid::less_equal || op == tokenid::greater || op == tokenid::greater_equal)
					is_bool_result = true;

				// Cannot perform arithmetic operations on non-basic types
				if (!lhs.type.is_scalar() && !lhs.type.is_vector() && !lhs.type.is_matrix())
					return error(lhs.location, 3022, "scalar, vector, or matrix expected"), false;
				if (!rhs.type.is_scalar() && !rhs.type.is_vector() && !rhs.type.is_matrix())
					return error(rhs.location, 3022, "scalar, vector, or matrix expected"), false;
			}

			// Perform implicit type conversion
			if (lhs.type.components() > type.components())
				warning(lhs.location, 3206, "implicit truncation of vector type");
			if (rhs.type.components() > type.components())
				warning(rhs.location, 3206, "implicit truncation of vector type");

			lhs.add_cast_operation(type);
			rhs.add_cast_operation(type);

#if RESHADEFX_SHORT_CIRCUIT
			// Reset block to left-hand side since the load of the left-hand side value has to happen in there
			if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
				_codegen->set_block(lhs_block);
#endif

			// Constant expressions can be evaluated at compile time
			if (rhs.is_constant && lhs.evaluate_constant_expression(op, rhs.constant))
				continue;

			const auto lhs_value = _codegen->emit_load(lhs);

#if RESHADEFX_SHORT_CIRCUIT
			// Short circuit for logical && and || operators
			if (op == tokenid::ampersand_ampersand || op == tokenid::pipe_pipe)
			{
				// Emit "if ( lhs) result = rhs" for && expression
				codegen::id condition_value = lhs_value;
				// Emit "if (!lhs) result = rhs" for || expression
				if (op == tokenid::pipe_pipe)
					condition_value = _codegen->emit_unary_op(lhs.location, tokenid::exclaim, type, lhs_value);

				_codegen->leave_block_and_branch_conditional(condition_value, rhs_block, merge_block);

				_codegen->set_block(rhs_block);
				// Only load value of right hand side expression after entering the second block
				const auto rhs_value = _codegen->emit_load(rhs);
				_codegen->leave_block_and_branch(merge_block);

				_codegen->enter_block(merge_block);

				const auto result_value = _codegen->emit_phi(lhs.location, condition_value, lhs_block, rhs_value, rhs_block, lhs_value, lhs_block, type);

				lhs.reset_to_rvalue(lhs.location, result_value, type);
				continue;
			}
#endif
			const auto rhs_value = _codegen->emit_load(rhs);

			// Certain operations return a boolean type instead of the type of the input expressions
			if (is_bool_result)
				type = { type::t_bool, type.rows, type.cols };

			const auto result_value = _codegen->emit_binary_op(lhs.location, op, type, lhs.type, lhs_value, rhs_value);

			lhs.reset_to_rvalue(lhs.location, result_value, type);
			#pragma endregion
		}
		else
		{
			#pragma region Ternary Expression
			// A conditional expression needs a scalar or vector type condition
			if (!lhs.type.is_scalar() && !lhs.type.is_vector())
				return error(lhs.location, 3022, "boolean or vector expression expected"), false;

#if RESHADEFX_SHORT_CIRCUIT
			// Switch block to a new one before parsing first part in case it needs to be skipped during short-circuiting
			const codegen::id merge_block = _codegen->create_block();
			const codegen::id condition_block = _codegen->set_block(0);
			codegen::id true_block = _codegen->create_block();
			codegen::id false_block = _codegen->create_block();

			_codegen->enter_block(true_block);
#endif
			// Parse the first part of the right hand side of the ternary operation
			expression true_exp;
			if (!parse_expression(true_exp))
				return false;

			if (!expect(':'))
				return false;

#if RESHADEFX_SHORT_CIRCUIT
			// Switch block to a new one before parsing second part in case it needs to be skipped during short-circuiting
			_codegen->set_block(0);
			_codegen->enter_block(false_block);
#endif
			// Parse the second part of the right hand side of the ternary operation
			expression false_exp;
			if (!parse_expression_assignment(false_exp))
				return false;

			// Check that the condition dimension matches that of at least one side
			if (lhs.type.rows != true_exp.type.rows && lhs.type.cols != true_exp.type.cols)
				return error(lhs.location, 3020, "dimension of conditional does not match value"), false;

			// Check that the two value expressions can be converted between each other
			if (true_exp.type.array_length != false_exp.type.array_length || true_exp.type.definition != false_exp.type.definition)
				return error(false_exp.location, 3020, "type mismatch between conditional values"), false;

			// Deduce the result base type based on implicit conversion rules
			const type type = type::merge(true_exp.type, false_exp.type);

			if (true_exp.type.components() > type.components())
				warning(true_exp.location, 3206, "implicit truncation of vector type");
			if (false_exp.type.components() > type.components())
				warning(false_exp.location, 3206, "implicit truncation of vector type");

#if RESHADEFX_SHORT_CIRCUIT
			// Reset block to left-hand side since the load of the condition value has to happen in there
			_codegen->set_block(condition_block);
#else
			// The conditional operator instruction expects the condition to be a boolean type
			lhs.add_cast_operation({ type::t_bool, type.rows, 1 });
#endif
			true_exp.add_cast_operation(type);
			false_exp.add_cast_operation(type);

			// Load condition value from expression
			const auto condition_value = _codegen->emit_load(lhs);

#if RESHADEFX_SHORT_CIRCUIT
			_codegen->leave_block_and_branch_conditional(condition_value, true_block, false_block);

			_codegen->set_block(true_block);
			// Only load true expression value after entering the first block
			const auto true_value = _codegen->emit_load(true_exp);
			true_block = _codegen->leave_block_and_branch(merge_block);

			_codegen->set_block(false_block);
			// Only load false expression value after entering the second block
			const auto false_value = _codegen->emit_load(false_exp);
			false_block = _codegen->leave_block_and_branch(merge_block);

			_codegen->enter_block(merge_block);

			const auto result_value = _codegen->emit_phi(lhs.location, condition_value, condition_block, true_value, true_block, false_value, false_block, type);
#else
			const auto true_value = _codegen->emit_load(true_exp);
			const auto false_value = _codegen->emit_load(false_exp);

			const auto result_value = _codegen->emit_ternary_op(lhs.location, op, type, condition_value, true_value, false_value);
#endif
			lhs.reset_to_rvalue(lhs.location, result_value, type);
			#pragma endregion
		}
	}

	return true;
}

bool reshadefx::parser::parse_expression_assignment(expression &lhs)
{
	// Parse left hand side of the expression
	if (!parse_expression_multary(lhs))
		return false;

	// Check if an operator exists so that this is an assignment
	if (accept_assignment_op())
	{
		// Remember the operator token before parsing the expression that follows it
		const tokenid op = _token.id;

		// Parse right hand side of the assignment expression
		// This may be another assignment expression to support chains like "a = b = c = 0;"
		expression rhs;
		if (!parse_expression_assignment(rhs))
			return false;

		// Check if the assignment is valid
		if (lhs.type.has(type::q_const) || lhs.type.has(type::q_uniform) || !lhs.is_lvalue)
			return error(lhs.location, 3025, "l-value specifies const object"), false;
		if (!type::rank(lhs.type, rhs.type))
			return error(rhs.location, 3020, "cannot convert these types (from " + rhs.type.description() + " to " + lhs.type.description() + ')'), false;

		// Cannot perform bitwise operations on non-integral types
		if (!lhs.type.is_integral() && (op == tokenid::ampersand_equal || op == tokenid::pipe_equal || op == tokenid::caret_equal))
			return error(lhs.location, 3082, "int or unsigned int type required"), false;

		// Perform implicit type conversion of right hand side value
		if (rhs.type.components() > lhs.type.components())
			warning(rhs.location, 3206, "implicit truncation of vector type");

		rhs.add_cast_operation(lhs.type);

		auto result = _codegen->emit_load(rhs);

		// Check if this is an assignment with an additional arithmetic instruction
		if (op != tokenid::equal)
		{
			// Load value for modification
			const auto value = _codegen->emit_load(lhs);

			// Handle arithmetic assignment operation
			result = _codegen->emit_binary_op(lhs.location, op, lhs.type, value, result);
		}

		// Write result back to variable
		_codegen->emit_store(lhs, result);

		// Return the result value since you can write assignments within expressions
		lhs.reset_to_rvalue(lhs.location, result, lhs.type);
	}

	return true;
}

bool reshadefx::parser::parse_annotations(std::vector<annotation> &annotations)
{
	// Check if annotations exist and return early if none do
	if (!accept('<'))
		return true;

	bool parse_success = true;

	while (!peek('>'))
	{
		if (type type; accept_type_class(type))
			warning(_token.location, 4717, "type prefixes for annotations are deprecated and ignored");

		if (!expect(tokenid::identifier))
			return consume_until('>'), false;

		auto name = std::move(_token.literal_as_string);

		if (expression expression; !expect('=') || !parse_expression_multary(expression) || !expect(';'))
			return consume_until('>'), false;
		else if (expression.is_constant)
			annotations.push_back({ expression.type, std::move(name), std::move(expression.constant) });
		else // Continue parsing annotations despite this not being a constant, since the syntax is still correct
			parse_success = false,
			error(expression.location, 3011, "value must be a literal expression");
	}

	return expect('>') && parse_success;
}

// -- Statement & Declaration Parsing -- //

bool reshadefx::parser::parse_statement(bool scoped)
{
	if (!_codegen->is_in_block())
		return error(_token_next.location, 0, "unreachable code"), false;

	unsigned int loop_control = 0;
	unsigned int selection_control = 0;

	// Read any loop and branch control attributes first
	while (accept('['))
	{
		enum control_mask
		{
			unroll = 0x1,
			dont_unroll = 0x2,
			flatten = 0x4,
			dont_flatten = 0x8,
		};

		const auto attribute = std::move(_token_next.literal_as_string);

		if (!expect(tokenid::identifier) || !expect(']'))
			return false;

		if (attribute == "unroll")
			loop_control |= unroll;
		else if (attribute == "loop" || attribute == "fastopt")
			loop_control |= dont_unroll;
		else if (attribute == "flatten")
			selection_control |= flatten;
		else if (attribute == "branch")
			selection_control |= dont_flatten;
		else
			warning(_token.location, 0, "unknown attribute");

		if ((loop_control & (unroll | dont_unroll)) == (unroll | dont_unroll))
			return error(_token.location, 3524, "can't use loop and unroll attributes together"), false;
		if ((selection_control & (flatten | dont_flatten)) == (flatten | dont_flatten))
			return error(_token.location, 3524, "can't use branch and flatten attributes together"), false;
	}

	// Shift by two so that the possible values are 0x01 for 'flatten' and 0x02 for 'dont_flatten', equivalent to 'unroll' and 'dont_unroll'
	selection_control >>= 2;

	if (peek('{')) // Parse statement block
		return parse_statement_block(scoped);
	else if (accept(';')) // Ignore empty statements
		return true;

	// Most statements with the exception of declarations are only valid inside functions
	if (_codegen->is_in_function())
	{
		const auto location = _token_next.location;

		#pragma region If
		if (accept(tokenid::if_))
		{
			codegen::id true_block = _codegen->create_block(); // Block which contains the statements executed when the condition is true
			codegen::id false_block = _codegen->create_block(); // Block which contains the statements executed when the condition is false
			const codegen::id merge_block = _codegen->create_block(); // Block that is executed after the branch re-merged with the current control flow

			expression condition;
			if (!expect('(') || !parse_expression(condition) || !expect(')'))
				return false;
			else if (!condition.type.is_scalar())
				return error(condition.location, 3019, "if statement conditional expressions must evaluate to a scalar"), false;

			// Load condition and convert to boolean value as required by 'OpBranchConditional'
			condition.add_cast_operation({ type::t_bool, 1, 1 });

			const codegen::id condition_value = _codegen->emit_load(condition);
			const codegen::id condition_block = _codegen->leave_block_and_branch_conditional(condition_value, true_block, false_block);

			{ // Then block of the if statement
				_codegen->enter_block(true_block);

				if (!parse_statement(true))
					return false;

				true_block = _codegen->leave_block_and_branch(merge_block);
			}
			{ // Else block of the if statement
				_codegen->enter_block(false_block);

				if (accept(tokenid::else_) && !parse_statement(true))
					return false;

				false_block = _codegen->leave_block_and_branch(merge_block);
			}

			_codegen->enter_block(merge_block);

			// Emit structured control flow for an if statement and connect all basic blocks
			_codegen->emit_if(location, condition_value, condition_block, true_block, false_block, selection_control);

			return true;
		}
		#pragma endregion

		#pragma region Switch
		if (accept(tokenid::switch_))
		{
			const codegen::id merge_block = _codegen->create_block(); // Block that is executed after the switch re-merged with the current control flow

			expression selector_exp;
			if (!expect('(') || !parse_expression(selector_exp) || !expect(')'))
				return false;
			else if (!selector_exp.type.is_scalar())
				return error(selector_exp.location, 3019, "switch statement expression must evaluate to a scalar"), false;

			// Load selector and convert to integral value as required by switch instruction
			selector_exp.add_cast_operation({ type::t_int, 1, 1 });

			const auto selector_value = _codegen->emit_load(selector_exp);
			const auto selector_block = _codegen->leave_block_and_switch(selector_value, merge_block);

			if (!expect('{'))
				return false;

			_loop_break_target_stack.push_back(merge_block);
			on_scope_exit _([this]() { _loop_break_target_stack.pop_back(); });

			bool parse_success = true;
			codegen::id default_label = merge_block; // The default case jumps to the end of the switch statement if not overwritten
			std::vector<codegen::id> case_literal_and_labels;
			size_t last_case_label_index = 0;

			// Enter first switch statement body block
			_codegen->enter_block(_codegen->create_block());

			while (!peek(tokenid::end_of_file))
			{
				while (accept(tokenid::case_) || accept(tokenid::default_))
				{
					if (_token.id == tokenid::case_)
					{
						expression case_label;
						if (!parse_expression(case_label))
							return consume_until('}'), false;
						else if (!case_label.type.is_scalar() || !case_label.type.is_integral() || !case_label.is_constant)
							return error(case_label.location, 3020, "invalid type for case expression - value must be an integer scalar"), consume_until('}'), false;

						// Check for duplicate case values
						for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
						{
							if (case_literal_and_labels[i] == case_label.constant.as_uint[0])
							{
								parse_success = false;
								error(case_label.location, 3532, "duplicate case " + std::to_string(case_label.constant.as_uint[0]));
								break;
							}
						}

						case_literal_and_labels.push_back(case_label.constant.as_uint[0]);
						case_literal_and_labels.emplace_back(); // This is set to the actual block below
					}
					else
					{
						// Check if the default label was already changed by a previous 'default' statement
						if (default_label != merge_block)
						{
							parse_success = false;
							error(_token.location, 3532, "duplicate default in switch statement");
						}

						default_label = 0; // This is set to the actual block below
					}

					if (!expect(':'))
						return consume_until('}'), false;
				}

				// It is valid for no statement to follow if this is the last label in the switch body
				const bool end_of_switch = peek('}');

				if (!end_of_switch && !parse_statement(true))
					return consume_until('}'), false;

				// Handle fall-through case and end of switch statement
				if (peek(tokenid::case_) || peek(tokenid::default_) || end_of_switch)
				{
					if (_codegen->is_in_block()) // Disallow fall-through for now
					{
						parse_success = false;
						error(_token_next.location, 3533, "non-empty case statements must have break or return");
					}

					const codegen::id next_block = end_of_switch ? merge_block : _codegen->create_block();
					const codegen::id current_block = _codegen->leave_block_and_branch(next_block);

					assert(current_block != 0);

					if (default_label == 0)
						default_label = current_block;
					else
						for (size_t i = last_case_label_index; i < case_literal_and_labels.size(); i += 2)
							case_literal_and_labels[i + 1] = current_block;

					_codegen->enter_block(next_block);

					if (end_of_switch) // We reached the end, nothing more to do
						break;

					last_case_label_index = case_literal_and_labels.size();
				}
			}

			if (case_literal_and_labels.empty() && default_label == merge_block)
				warning(location, 5002, "switch statement contains no 'case' or 'default' labels");

			// Emit structured control flow for a switch statement and connect all basic blocks
			_codegen->emit_switch(location, selector_value, selector_block, default_label, case_literal_and_labels, selection_control);

			return expect('}') && parse_success;
		}
		#pragma endregion

		#pragma region For
		if (accept(tokenid::for_))
		{
			if (!expect('('))
				return false;

			enter_scope();
			on_scope_exit _([this]() { leave_scope(); });

			// Parse initializer first
			if (type type; parse_type(type))
			{
				unsigned int count = 0;
				do { // There may be multiple declarations behind a type, so loop through them
					if (count++ > 0 && !expect(','))
						return false;
					if (!expect(tokenid::identifier) || !parse_variable(type, std::move(_token.literal_as_string)))
						return false;
				} while (!peek(';'));
			}
			else // Initializer can also contain an expression if not a variable declaration list
			{
				expression expression;
				parse_expression(expression); // It is valid for there to be no initializer expression, so ignore result
			}

			if (!expect(';'))
				return false;

			const codegen::id merge_block = _codegen->create_block(); // Block that is executed after the loop
			const codegen::id header_label = _codegen->create_block(); // Pointer to the loop merge instruction
			const codegen::id continue_label = _codegen->create_block(); // Pointer to the continue block
			codegen::id loop_block = _codegen->create_block(); // Pointer to the main loop body block
			codegen::id condition_block = _codegen->create_block(); // Pointer to the condition check
			codegen::id condition_value = 0;

			// End current block by branching to the next label
			const codegen::id prev_block = _codegen->leave_block_and_branch(header_label);

			{ // Begin loop block (this header is used for explicit structured control flow)
				_codegen->enter_block(header_label);

				_codegen->leave_block_and_branch(condition_block);
			}

			{ // Parse condition block
				_codegen->enter_block(condition_block);

				if (expression condition; parse_expression(condition))
				{
					if (!condition.type.is_scalar())
						return error(condition.location, 3019, "scalar value expected"), false;

					// Evaluate condition and branch to the right target
					condition.add_cast_operation({ type::t_bool, 1, 1 });

					condition_value = _codegen->emit_load(condition);

					condition_block = _codegen->leave_block_and_branch_conditional(condition_value, loop_block, merge_block);
				}
				else // It is valid for there to be no condition expression
				{
					condition_block = _codegen->leave_block_and_branch(loop_block);
				}

				if (!expect(';'))
					return false;
			}

			{ // Parse loop continue block into separate block so it can be appended to the end down the line
				_codegen->enter_block(continue_label);

				expression continue_exp;
				parse_expression(continue_exp); // It is valid for there to be no continue expression, so ignore result

				if (!expect(')'))
					return false;

				// Branch back to the loop header at the end of the continue block
				_codegen->leave_block_and_branch(header_label);
			}

			{ // Parse loop body block
				_codegen->enter_block(loop_block);

				_loop_break_target_stack.push_back(merge_block);
				_loop_continue_target_stack.push_back(continue_label);

				const bool parse_success = parse_statement(false);

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				if (!parse_success)
					return false;

				loop_block = _codegen->leave_block_and_branch(continue_label);
			}

			// Add merge block label to the end of the loop
			_codegen->enter_block(merge_block);

			// Emit structured control flow for a loop statement and connect all basic blocks
			_codegen->emit_loop(location, condition_value, prev_block, header_label, condition_block, loop_block, continue_label, loop_control);

			return true;
		}
		#pragma endregion

		#pragma region While
		if (accept(tokenid::while_))
		{
			enter_scope();
			on_scope_exit _([this]() { leave_scope(); });

			const codegen::id merge_block = _codegen->create_block();
			const codegen::id header_label = _codegen->create_block();
			const codegen::id continue_label = _codegen->create_block();
			codegen::id loop_block = _codegen->create_block();
			codegen::id condition_block = _codegen->create_block();
			codegen::id condition_value = 0;

			// End current block by branching to the next label
			const codegen::id prev_block = _codegen->leave_block_and_branch(header_label);

			{ // Begin loop block
				_codegen->enter_block(header_label);

				_codegen->leave_block_and_branch(condition_block);
			}

			{ // Parse condition block
				_codegen->enter_block(condition_block);

				expression condition;
				if (!expect('(') || !parse_expression(condition) || !expect(')'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				condition.add_cast_operation({ type::t_bool, 1, 1 });

				condition_value = _codegen->emit_load(condition);

				condition_block = _codegen->leave_block_and_branch_conditional(condition_value, loop_block, merge_block);
			}

			{ // Parse loop body block
				_codegen->enter_block(loop_block);

				_loop_break_target_stack.push_back(merge_block);
				_loop_continue_target_stack.push_back(continue_label);

				const bool parse_success = parse_statement(false);

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				if (!parse_success)
					return false;

				loop_block = _codegen->leave_block_and_branch(continue_label);
			}

			{ // Branch back to the loop header in empty continue block
				_codegen->enter_block(continue_label);

				_codegen->leave_block_and_branch(header_label);
			}

			// Add merge block label to the end of the loop
			_codegen->enter_block(merge_block);

			// Emit structured control flow for a loop statement and connect all basic blocks
			_codegen->emit_loop(location, condition_value, prev_block, header_label, condition_block, loop_block, continue_label, loop_control);

			return true;
		}
		#pragma endregion

		#pragma region DoWhile
		if (accept(tokenid::do_))
		{
			const codegen::id merge_block = _codegen->create_block();
			const codegen::id header_label = _codegen->create_block();
			const codegen::id continue_label = _codegen->create_block();
			codegen::id loop_block = _codegen->create_block();
			codegen::id condition_value = 0;

			// End current block by branching to the next label
			const codegen::id prev_block = _codegen->leave_block_and_branch(header_label);

			{ // Begin loop block
				_codegen->enter_block(header_label);

				_codegen->leave_block_and_branch(loop_block);
			}

			{ // Parse loop body block
				_codegen->enter_block(loop_block);

				_loop_break_target_stack.push_back(merge_block);
				_loop_continue_target_stack.push_back(continue_label);

				const bool parse_success = parse_statement(true);

				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();

				if (!parse_success)
					return false;

				loop_block = _codegen->leave_block_and_branch(continue_label);
			}

			{ // Continue block does the condition evaluation
				_codegen->enter_block(continue_label);

				expression condition;
				if (!expect(tokenid::while_) || !expect('(') || !parse_expression(condition) || !expect(')') || !expect(';'))
					return false;
				else if (!condition.type.is_scalar())
					return error(condition.location, 3019, "scalar value expected"), false;

				// Evaluate condition and branch to the right target
				condition.add_cast_operation({ type::t_bool, 1, 1 });

				condition_value = _codegen->emit_load(condition);

				_codegen->leave_block_and_branch_conditional(condition_value, header_label, merge_block);
			}

			// Add merge block label to the end of the loop
			_codegen->enter_block(merge_block);

			// Emit structured control flow for a loop statement and connect all basic blocks
			_codegen->emit_loop(location, condition_value, prev_block, header_label, 0, loop_block, continue_label, loop_control);

			return true;
		}
		#pragma endregion

		#pragma region Break
		if (accept(tokenid::break_))
		{
			if (_loop_break_target_stack.empty())
				return error(location, 3518, "break must be inside loop"), false;

			// Branch to the break target of the inner most loop on the stack
			_codegen->leave_block_and_branch(_loop_break_target_stack.back(), 1);

			return expect(';');
		}
		#pragma endregion

		#pragma region Continue
		if (accept(tokenid::continue_))
		{
			if (_loop_continue_target_stack.empty())
				return error(location, 3519, "continue must be inside loop"), false;

			// Branch to the continue target of the inner most loop on the stack
			_codegen->leave_block_and_branch(_loop_continue_target_stack.back(), 2);

			return expect(';');
		}
		#pragma endregion

		#pragma region Return
		if (accept(tokenid::return_))
		{
			const type &ret_type = _current_return_type;

			if (!peek(';'))
			{
				expression expression;
				if (!parse_expression(expression))
					return consume_until(';'), false;

				// Cannot return to void
				if (ret_type.is_void())
					// Consume the semicolon that follows the return expression so that parsing may continue
					return error(location, 3079, "void functions cannot return a value"), accept(';'), false;

				// Cannot return arrays from a function
				if (expression.type.is_array() || !type::rank(expression.type, ret_type))
					return error(location, 3017, "expression does not match function return type"), accept(';'), false;

				// Load return value and perform implicit cast to function return type
				if (expression.type.components() > ret_type.components())
					warning(expression.location, 3206, "implicit truncation of vector type");

				expression.add_cast_operation(ret_type);

				const auto return_value = _codegen->emit_load(expression);

				_codegen->leave_block_and_return(return_value);
			}
			else if (!ret_type.is_void())
			{
				// No return value was found, but the function expects one
				error(location, 3080, "function must return a value");

				// Consume the semicolon that follows the return expression so that parsing may continue
				accept(';');

				return false;
			}
			else
			{
				_codegen->leave_block_and_return();
			}

			return expect(';');
		}
		#pragma endregion

		#pragma region Discard
		if (accept(tokenid::discard_))
		{
			// Leave the current function block
			_codegen->leave_block_and_kill();

			return expect(';');
		}
		#pragma endregion
	}

	#pragma region Declaration
	// Handle variable declarations
	if (type type; parse_type(type))
	{
		unsigned int count = 0;
		do { // There may be multiple declarations behind a type, so loop through them
			if (count++ > 0 && !expect(','))
				// Try to consume the rest of the declaration so that parsing may continue despite the error
				return consume_until(';'), false;
			if (!expect(tokenid::identifier) || !parse_variable(type, std::move(_token.literal_as_string)))
				return consume_until(';'), false;
		} while (!peek(';'));

		return expect(';');
	}
	#pragma endregion

	// Handle expression statements
	if (expression expression; parse_expression(expression))
		return expect(';'); // A statement has to be terminated with a semicolon

	// Gracefully consume any remaining characters until the statement would usually end, so that parsing may continue despite the error
	consume_until(';');

	return false;
}

bool reshadefx::parser::parse_statement_block(bool scoped)
{
	if (!expect('{'))
		return false;

	if (scoped)
		enter_scope();

	// Parse statements until the end of the block is reached
	while (!peek('}') && !peek(tokenid::end_of_file))
	{
		if (!parse_statement(true))
		{
			if (scoped)
				leave_scope();

			// Ignore the rest of this block
			unsigned int level = 0;

			while (!peek(tokenid::end_of_file))
			{
				if (accept('{'))
				{
					++level;
				}
				else if (accept('}'))
				{
					if (level-- == 0)
						break;
				} // These braces are necessary to match the 'else' to the correct 'if' statement
				else
				{
					consume();
				}
			}

			return false;
		}
	}

	if (scoped)
		leave_scope();

	return expect('}');
}

bool reshadefx::parser::parse_top()
{
	if (accept(tokenid::namespace_))
	{
		// Anonymous namespaces are not supported right now, so an identifier is a must
		if (!expect(tokenid::identifier))
			return false;

		const auto name = std::move(_token.literal_as_string);

		if (!expect('{'))
			return false;

		enter_namespace(name);

		bool parse_success = true;

		// Recursively parse top level statements until the namespace is closed again
		while (!peek('}') && parse_success) // Empty namespaces are valid
		{
			if (!parse_top())
				parse_success = false;
		}

		leave_namespace();

		return expect('}') && parse_success;
	}
	else if (accept(tokenid::struct_)) // Structure keyword found, parse the structure definition
	{
		if (!parse_struct() || !expect(';')) // Structure definitions are terminated with a semicolon
			return false;
	}
	else if (accept(tokenid::technique)) // Technique keyword found, parse the technique definition
	{
		if (!parse_technique())
			return false;
	}
	else
	{
		if (type type; parse_type(type)) // Type found, this can be either a variable or a function declaration
		{
			if (!expect(tokenid::identifier))
				return false;

			if (peek('('))
			{
				const auto name = std::move(_token.literal_as_string);
				// This is definitely a function declaration, so parse it
				if (!parse_function(type, name)) {
					// Insert dummy function into symbol table, so later references can be resolved despite the error
					insert_symbol(name, { symbol_type::function, ~0u, { type::t_function } }, true);
					return false;
				}
			}
			else
			{
				// There may be multiple variable names after the type, handle them all
				unsigned int count = 0;
				do {
					if (count++ > 0 && !(expect(',') && expect(tokenid::identifier)))
						return false;
					const auto name = std::move(_token.literal_as_string);
					if (!parse_variable(type, name, true)) {
						// Insert dummy variable into symbol table, so later references can be resolved despite the error
						insert_symbol(name, { symbol_type::variable, ~0u, type }, true);
						return consume_until(';'), false; // Skip the rest of the statement in case of an error
					}
				} while (!peek(';'));

				if (!expect(';')) // Variable declarations are terminated with a semicolon
					return false;
			}
		}
		else if (!accept(';')) // Ignore single semicolons in the source
		{
			consume(); // Unexpected token in source stream, consume and report an error about it
			error(_token.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token.id) + '\'');
			return false;
		}
	}

	return true;
}

bool reshadefx::parser::parse_struct()
{
	const auto location = std::move(_token.location);

	struct_info info;
	// The structure name is optional
	if (accept(tokenid::identifier))
		info.name = std::move(_token.literal_as_string);
	else
		info.name = "_anonymous_struct_" + std::to_string(location.line) + '_' + std::to_string(location.column);

	info.unique_name = 'S' + current_scope().name + info.name;
	std::replace(info.unique_name.begin(), info.unique_name.end(), ':', '_');

	if (!expect('{'))
		return false;

	bool parse_success = true;

	while (!peek('}')) // Empty structures are possible
	{
		struct_member_info member;

		if (!parse_type(member.type))
			return error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected struct member type"), consume_until('}'), accept(';'), false;

		unsigned int count = 0;
		do {
			if (count++ > 0 && !expect(','))
				return consume_until('}'), accept(';'), false;

			if (!expect(tokenid::identifier))
				return consume_until('}'), accept(';'), false;

			member.name = std::move(_token.literal_as_string);
			member.location = std::move(_token.location);

			if (member.type.is_void())
				parse_success = false,
				error(member.location, 3038, '\'' + member.name + "': struct members cannot be void");
			if (member.type.is_struct()) // Nesting structures would make input/output argument flattening more complicated, so prevent it for now
				parse_success = false,
				error(member.location, 3090, '\'' + member.name + "': nested struct members are not supported");

			if (member.type.has(type::q_in) || member.type.has(type::q_out))
				parse_success = false,
				error(member.location, 3055, '\'' + member.name + "': struct members cannot be declared 'in' or 'out'");
			if (member.type.has(type::q_const))
				parse_success = false,
				error(member.location, 3035, '\'' + member.name + "': struct members cannot be declared 'const'");
			if (member.type.has(type::q_extern))
				parse_success = false,
				error(member.location, 3006, '\'' + member.name + "': struct members cannot be declared 'extern'");
			if (member.type.has(type::q_uniform))
				parse_success = false,
				error(member.location, 3047, '\'' + member.name + "': struct members cannot be declared 'uniform'");
			if (member.type.has(type::q_groupshared))
				parse_success = false,
				error(member.location, 3010, '\'' + member.name + "': struct members cannot be declared 'groupshared'");

			// Modify member specific type, so that following members in the declaration list are not affected by this
			if (!parse_array_size(member.type))
				return consume_until('}'), accept(';'), false;
			else if (member.type.array_length < 0)
				parse_success = false,
				error(member.location, 3072, '\'' + member.name + "': array dimensions of struct members must be explicit");

			// Structure members may have semantics to use them as input/output types
			if (accept(':'))
			{
				if (!expect(tokenid::identifier))
					return consume_until('}'), accept(';'), false;

				member.semantic = std::move(_token.literal_as_string);
				// Make semantic upper case to simplify comparison later on
				std::transform(member.semantic.begin(), member.semantic.end(), member.semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
			}

			// Save member name and type for book keeping
			info.member_list.push_back(member);
		} while (!peek(';'));

		if (!expect(';'))
			return consume_until('}'), accept(';'), false;
	}

	// Empty structures are valid, but not usually intended, so emit a warning
	if (info.member_list.empty())
		warning(location, 5001, "struct has no members");

	// Define the structure now that information about all the member types was gathered
	const auto id = _codegen->define_struct(location, info);

	// Insert the symbol into the symbol table
	const symbol symbol = { symbol_type::structure, id };

	if (!insert_symbol(info.name, symbol, true))
		return error(location, 3003, "redefinition of '" + info.name + '\''), false;

	return expect('}') && parse_success;
}

bool reshadefx::parser::parse_function(type type, std::string name)
{
	const auto location = std::move(_token.location);

	if (!expect('(')) // Functions always have a parameter list
		return false;
	if (type.qualifiers != 0)
		return error(location, 3047, '\'' + name + "': function return type cannot have any qualifiers"), false;

	function_info info;
	info.name = name;
	info.unique_name = 'F' + current_scope().name + name;
	std::replace(info.unique_name.begin(), info.unique_name.end(), ':', '_');

	info.return_type = type;
	_current_return_type = info.return_type;

	bool parse_success = true;
	bool expect_parenthesis = true;

	// Enter function scope
	enter_scope(); on_scope_exit _([this]() { leave_scope(); _codegen->leave_function(); });

	while (!peek(')'))
	{
		if (!info.parameter_list.empty() && !expect(','))
		{
			parse_success = false;
			expect_parenthesis = false;
			consume_until(')');
			break;
		}

		struct_member_info param;

		if (!parse_type(param.type))
		{
			error(_token_next.location, 3000, "syntax error: unexpected '" + token::id_to_name(_token_next.id) + "', expected parameter type");
			parse_success = false;
			expect_parenthesis = false;
			consume_until(')');
			break;
		}

		if (!expect(tokenid::identifier))
		{
			parse_success = false;
			expect_parenthesis = false;
			consume_until(')');
			break;
		}

		param.name = std::move(_token.literal_as_string);
		param.location = std::move(_token.location);

		if (param.type.is_void())
			parse_success = false,
			error(param.location, 3038, '\'' + param.name + "': function parameters cannot be void");
		if (param.type.has(type::q_extern))
			parse_success = false,
			error(param.location, 3006, '\'' + param.name + "': function parameters cannot be declared 'extern'");
		if (param.type.has(type::q_static))
			parse_success = false,
			error(param.location, 3007, '\'' + param.name + "': function parameters cannot be declared 'static'");
		if (param.type.has(type::q_uniform))
			parse_success = false,
			error(param.location, 3047, '\'' + param.name + "': function parameters cannot be declared 'uniform', consider placing in global scope instead");

		if (param.type.has(type::q_out) && param.type.has(type::q_const))
			parse_success = false,
			error(param.location, 3046, '\'' + param.name + "': output parameters cannot be declared 'const'");
		else if (!param.type.has(type::q_out))
			param.type.qualifiers |= type::q_in; // Function parameters are implicitly 'in' if not explicitly defined as 'out'

		if (!parse_array_size(param.type))
		{
			parse_success = false;
			expect_parenthesis = false;
			consume_until(')');
			break;
		}
		else if (param.type.array_length < 0)
		{
			parse_success = false;
			error(param.location, 3072, '\'' + param.name + "': array dimensions of function parameters must be explicit");
		}

		// Handle parameter type semantic
		if (accept(':'))
		{
			if (!expect(tokenid::identifier))
			{
				parse_success = false;
				expect_parenthesis = false;
				consume_until(')');
				break;
			}

			param.semantic = std::move(_token.literal_as_string);
			// Make semantic upper case to simplify comparison later on
			std::transform(param.semantic.begin(), param.semantic.end(), param.semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });

			if (param.type.is_integral() && !param.type.has(type::q_nointerpolation) &&
				param.semantic.compare(0, 3, "SV_") != 0)
			{
				param.type.qualifiers |= type::q_nointerpolation; // Integer parameters do not interpolate, so make this explicit (to avoid issues with GLSL)
				warning(param.location, 4568, '\'' + param.name + "': integer parameters have the 'nointerpolation' qualifier by default");
			}
		}

		info.parameter_list.push_back(std::move(param));
	}

	if (expect_parenthesis && !expect(')'))
		return false;

	// Handle return type semantic
	if (accept(':'))
	{
		if (!expect(tokenid::identifier))
			return false;
		if (type.is_void())
			return error(_token.location, 3076, '\'' + name + "': void function cannot have a semantic"), false;

		info.return_semantic = std::move(_token.literal_as_string);
		// Make semantic upper case to simplify comparison later on
		std::transform(info.return_semantic.begin(), info.return_semantic.end(), info.return_semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
	}

	// Check if this is a function declaration without a body
	if (accept(';'))
		return error(location, 3510, '\'' + name + "': function is missing an implementation"), false;

	// Define the function now that information about the declaration was gathered
	const auto id = _codegen->define_function(location, info);

	// Insert the function and parameter symbols into the symbol table
	symbol symbol = { symbol_type::function, id, { type::t_function } };
	symbol.function = &_codegen->find_function(id);

	if (!insert_symbol(name, symbol, true))
		return error(location, 3003, "redefinition of '" + name + '\''), false;

	for (const struct_member_info &param : info.parameter_list)
		if (!insert_symbol(param.name, { symbol_type::variable, param.definition, param.type }))
			return error(param.location, 3003, "redefinition of '" + param.name + '\''), false;

	// A function has to start with a new block
	_codegen->enter_block(_codegen->create_block());

	if (!parse_statement_block(false))
		parse_success = false;

	// Add implicit return statement to the end of functions
	if (_codegen->is_in_block())
		_codegen->leave_block_and_return();

	return parse_success;
}

bool reshadefx::parser::parse_variable(type type, std::string name, bool global)
{
	const auto location = std::move(_token.location);

	if (type.is_void())
		return error(location, 3038, '\'' + name + "': variables cannot be void"), false;
	if (type.has(type::q_in) || type.has(type::q_out))
		return error(location, 3055, '\'' + name + "': variables cannot be declared 'in' or 'out'"), false;

	// Local and global variables have different requirements
	if (global)
	{
		// Check that type qualifier combinations are valid
		if (type.has(type::q_static))
		{
			// Global variables that are 'static' cannot be of another storage class
			if (type.has(type::q_uniform))
				return error(location, 3007, '\'' + name + "': uniform global variables cannot be declared 'static'"), false;
			// The 'volatile qualifier is only valid memory object declarations that are storage images or uniform blocks
			if (type.has(type::q_volatile))
				return error(location, 3008, '\'' + name + "': global variables cannot be declared 'volatile'"), false;
		}
		else if (!type.has(type::q_groupshared))
		{
			// Make all global variables 'uniform' by default, since they should be externally visible without the 'static' keyword
			if (!type.has(type::q_uniform) && !(type.is_texture() || type.is_sampler() || type.is_storage()))
				warning(location, 5000, '\'' + name + "': global variables are considered 'uniform' by default");

			// Global variables that are not 'static' are always 'extern' and 'uniform'
			type.qualifiers |= type::q_extern | type::q_uniform;

			// It is invalid to make 'uniform' variables constant, since they can be modified externally
			if (type.has(type::q_const))
				return error(location, 3035, '\'' + name + "': variables which are 'uniform' cannot be declared 'const'"), false;
		}
	}
	else
	{
		if (type.has(type::q_extern))
			return error(location, 3006, '\'' + name + "': local variables cannot be declared 'extern'"), false;
		if (type.has(type::q_uniform))
			return error(location, 3047, '\'' + name + "': local variables cannot be declared 'uniform'"), false;
		if (type.has(type::q_groupshared))
			return error(location, 3010, '\'' + name + "': local variables cannot be declared 'groupshared'"), false;

		if (type.is_texture() || type.is_sampler() || type.is_storage())
			return error(location, 3038, '\'' + name + "': local variables cannot be textures or samplers"), false;
	}

	// The variable name may be followed by an optional array size expression
	if (!parse_array_size(type))
		return false;

	bool parse_success = true;
	expression initializer;
	texture_info texture_info;
	sampler_info sampler_info;
	storage_info storage_info;

	if (accept(':'))
	{
		if (!expect(tokenid::identifier))
			return false;
		else if (!global) // Only global variables can have a semantic
			return error(_token.location, 3043, '\'' + name + "': local variables cannot have semantics"), false;

		std::string &semantic = texture_info.semantic;
		semantic = std::move(_token.literal_as_string);

		// Make semantic upper case to simplify comparison later on
		std::transform(semantic.begin(), semantic.end(), semantic.begin(), [](char c) { return static_cast<char>(toupper(c)); });
	}
	else
	{
		// Global variables can have optional annotations
		if (global && !parse_annotations(sampler_info.annotations))
			parse_success = false;

		// Variables without a semantic may have an optional initializer
		if (accept('='))
		{
			if (!parse_expression_assignment(initializer))
				return false;

			if (type.has(type::q_groupshared))
				return error(initializer.location, 3009, '\'' + name + "': variables declared 'groupshared' cannot have an initializer"), false;
			if (global && !initializer.is_constant) // TODO: This could be resolved by initializing these at the beginning of the entry point
				return error(initializer.location, 3011, '\'' + name + "': initial value must be a literal expression"), false;

			// Check type compatibility
			if ((type.array_length >= 0 && initializer.type.array_length != type.array_length) || !type::rank(initializer.type, type))
				return error(initializer.location, 3017, '\'' + name + "': initial value (" + initializer.type.description() + ") does not match variable type (" + type.description() + ')'), false;
			if ((initializer.type.rows < type.rows || initializer.type.cols < type.cols) && !initializer.type.is_scalar())
				return error(initializer.location, 3017, '\'' + name + "': cannot implicitly convert these vector types (from " + initializer.type.description() + " to " + type.description() + ')'), false;

			// Deduce array size from the initializer expression
			if (initializer.type.is_array())
				type.array_length = initializer.type.array_length;

			// Perform implicit cast from initializer expression to variable type
			if (initializer.type.components() > type.components())
				warning(initializer.location, 3206, "implicit truncation of vector type");

			initializer.add_cast_operation(type);
		}
		else if (type.is_numeric() || type.is_struct()) // Numeric variables without an initializer need special handling
		{
			if (type.has(type::q_const)) // Constants have to have an initial value
				return error(location, 3012, '\'' + name + "': missing initial value"), false;
			else if (!type.has(type::q_uniform)) // Zero initialize all global variables
				initializer.reset_to_rvalue_constant(location, {}, type);
		}
		else if (global && accept('{')) // Textures and samplers can have a property block attached to their declaration
		{
			// Non-numeric variables cannot be constants
			if (type.has(type::q_const))
				return error(location, 3035, '\'' + name + "': this variable type cannot be declared 'const'"), false;

			while (!peek('}'))
			{
				if (!expect(tokenid::identifier))
					return consume_until('}'), false;

				const auto property_name = std::move(_token.literal_as_string);
				const auto property_location = std::move(_token.location);

				if (!expect('='))
					return consume_until('}'), false;

				backup();

				expression expression;

				if (accept(tokenid::identifier)) // Handle special enumeration names for property values
				{
					// Transform identifier to uppercase to do case-insensitive comparison
					std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), [](char c) { return static_cast<char>(toupper(c)); });

					static const std::unordered_map<std::string, uint32_t> s_values = {
						{ "NONE", 0 }, { "POINT", 0 },
						{ "LINEAR", 1 },
						{ "WRAP", uint32_t(texture_address_mode::wrap) }, { "REPEAT", uint32_t(texture_address_mode::wrap) },
						{ "MIRROR", uint32_t(texture_address_mode::mirror) },
						{ "CLAMP", uint32_t(texture_address_mode::clamp) },
						{ "BORDER", uint32_t(texture_address_mode::border) },
						{ "R8", uint32_t(texture_format::r8) },
						{ "R16F", uint32_t(texture_format::r16f) },
						{ "R32F", uint32_t(texture_format::r32f) },
						{ "RG8", uint32_t(texture_format::rg8) }, { "R8G8", uint32_t(texture_format::rg8) },
						{ "RG16", uint32_t(texture_format::rg16) }, { "R16G16", uint32_t(texture_format::rg16) },
						{ "RG16F", uint32_t(texture_format::rg16f) }, { "R16G16F", uint32_t(texture_format::rg16f) },
						{ "RG32F", uint32_t(texture_format::rg32f) }, { "R32G32F", uint32_t(texture_format::rg32f) },
						{ "RGBA8", uint32_t(texture_format::rgba8) }, { "R8G8B8A8", uint32_t(texture_format::rgba8) },
						{ "RGBA16", uint32_t(texture_format::rgba16) }, { "R16G16B16A16", uint32_t(texture_format::rgba16) },
						{ "RGBA16F", uint32_t(texture_format::rgba16f) }, { "R16G16B16A16F", uint32_t(texture_format::rgba16f) },
						{ "RGBA32F", uint32_t(texture_format::rgba32f) }, { "R32G32B32A32F", uint32_t(texture_format::rgba32f) },
						{ "RGB10A2", uint32_t(texture_format::rgb10a2) }, { "R10G10B10A2", uint32_t(texture_format::rgb10a2) },
					};

					// Look up identifier in list of possible enumeration names
					if (const auto it = s_values.find(_token.literal_as_string);
						it != s_values.end())
						expression.reset_to_rvalue_constant(_token.location, it->second);
					else // No match found, so rewind to parser state before the identifier was consumed and try parsing it as a normal expression
						restore();
				}

				// Parse right hand side as normal expression if no special enumeration name was matched already
				if (!expression.is_constant && !parse_expression_multary(expression))
					return consume_until('}'), false;

				if (property_name == "Texture")
				{
					// Ignore invalid symbols that were added during error recovery
					if (expression.base == 0xFFFFFFFF)
						return consume_until('}'), false;

					if (!expression.type.is_texture())
						return error(expression.location, 3020, "type mismatch, expected texture name"), consume_until('}'), false;

					reshadefx::texture_info &target_info = _codegen->find_texture(expression.base);
					if (type.is_storage())
						// Texture is used as storage
						target_info.storage_access = true;

					texture_info = target_info;
					sampler_info.texture_name = target_info.unique_name;
					storage_info.texture_name = target_info.unique_name;
				}
				else
				{
					if (!expression.is_constant || !expression.type.is_scalar())
						return error(expression.location, 3538, "value must be a literal scalar expression"), consume_until('}'), false;

					// All states below expect the value to be of an unsigned integer type
					expression.add_cast_operation({ type::t_uint, 1, 1 });
					const unsigned int value = expression.constant.as_uint[0];

					if (property_name == "Width")
						texture_info.width = value > 0 ? value : 1;
					else if (property_name == "Height")
						texture_info.height = value > 0 ? value : 1;
					else if (property_name == "MipLevels")
						texture_info.levels = value > 0 ? value : 1;
					else if (property_name == "Format")
						texture_info.format = static_cast<texture_format>(value);
					else if (property_name == "SRGBTexture" || property_name == "SRGBReadEnable")
						sampler_info.srgb = value != 0;
					else if (property_name == "AddressU")
						sampler_info.address_u = static_cast<texture_address_mode>(value);
					else if (property_name == "AddressV")
						sampler_info.address_v = static_cast<texture_address_mode>(value);
					else if (property_name == "AddressW")
						sampler_info.address_w = static_cast<texture_address_mode>(value);
					else if (property_name == "MinFilter")
						sampler_info.filter = static_cast<texture_filter>((uint32_t(sampler_info.filter) & 0x0F) | ((value << 4) & 0x30)); // Combine sampler filter components into a single filter enumeration value
					else if (property_name == "MagFilter")
						sampler_info.filter = static_cast<texture_filter>((uint32_t(sampler_info.filter) & 0x33) | ((value << 2) & 0x0C));
					else if (property_name == "MipFilter")
						sampler_info.filter = static_cast<texture_filter>((uint32_t(sampler_info.filter) & 0x3C) |  (value       & 0x03));
					else if (property_name == "MinLOD" || property_name == "MaxMipLevel")
						sampler_info.min_lod = static_cast<float>(value);
					else if (property_name == "MaxLOD")
						sampler_info.max_lod = static_cast<float>(value);
					else if (property_name == "MipLODBias" || property_name == "MipMapLodBias")
						sampler_info.lod_bias = static_cast<float>(value);
					else
						return error(property_location, 3004, "unrecognized property '" + property_name + '\''), consume_until('}'), false;
				}

				if (!expect(';'))
					return consume_until('}'), false;
			}

			if (!expect('}'))
				return false;
		}
	}

	// At this point the array size should be known (either from the declaration or the initializer)
	if (type.array_length < 0)
		return error(location, 3074, '\'' + name + "': implicit array missing initial value"), false;

	symbol symbol;

	// Variables with a constant initializer and constant type are named constants
	if (type.is_numeric() && type.has(type::q_const) && initializer.is_constant)
	{
		// Named constants are special symbols
		symbol = { symbol_type::constant, 0, type, initializer.constant };
	}
	else if (type.is_texture())
	{
		assert(global);

		// Add namespace scope to avoid name clashes
		texture_info.unique_name = 'V' + current_scope().name + name;
		std::replace(texture_info.unique_name.begin(), texture_info.unique_name.end(), ':', '_');

		texture_info.annotations = std::move(sampler_info.annotations);

		symbol = { symbol_type::variable, 0, type };
		symbol.id = _codegen->define_texture(location, texture_info);
	}
	// Samplers are actually combined image samplers
	else if (type.is_sampler())
	{
		assert(global);

		if (sampler_info.texture_name.empty())
			return error(location, 3012, '\'' + name + "': missing 'Texture' property"), false;
		if (sampler_info.srgb && texture_info.format != texture_format::rgba8)
			return error(location, 4582, '\'' + name + "': texture does not support sRGB sampling (only textures with RGBA8 format do)"), false;

		// Add namespace scope to avoid name clashes
		sampler_info.unique_name = 'V' + current_scope().name + name;
		std::replace(sampler_info.unique_name.begin(), sampler_info.unique_name.end(), ':', '_');

		symbol = { symbol_type::variable, 0, type };
		symbol.id = _codegen->define_sampler(location, sampler_info);
	}
	else if (type.is_storage())
	{
		assert(global);

		if (storage_info.texture_name.empty())
			return error(location, 3012, '\'' + name + "': missing 'Texture' property"), false;

		// Add namespace scope to avoid name clashes
		storage_info.unique_name = 'V' + current_scope().name + name;
		std::replace(storage_info.unique_name.begin(), storage_info.unique_name.end(), ':', '_');

		symbol = { symbol_type::variable, 0, type };
		symbol.id = _codegen->define_storage(location, storage_info);
	}
	// Uniform variables are put into a global uniform buffer structure
	else if (type.has(type::q_uniform))
	{
		assert(global);

		uniform_info uniform_info;
		uniform_info.name = name;
		uniform_info.type = type;

		uniform_info.annotations = std::move(sampler_info.annotations);

		uniform_info.initializer_value = std::move(initializer.constant);
		uniform_info.has_initializer_value = initializer.is_constant;

		symbol = { symbol_type::variable, 0, type };
		symbol.id = _codegen->define_uniform(location, uniform_info);
	}
	// All other variables are separate entities
	else
	{
		// Update global variable names to contain the namespace scope to avoid name clashes
		std::string unique_name = global ? 'V' + current_scope().name + name : name;
		std::replace(unique_name.begin(), unique_name.end(), ':', '_');

		symbol = { symbol_type::variable, 0, type };
		symbol.id = _codegen->define_variable(location, type, std::move(unique_name), global,
			// Shared variables cannot have an initializer
			type.has(type::q_groupshared) ? 0 : _codegen->emit_load(initializer));
	}

	// Insert the symbol into the symbol table
	if (!insert_symbol(name, symbol, global))
		return error(location, 3003, "redefinition of '" + name + '\''), false;

	return parse_success;
}

bool reshadefx::parser::parse_technique()
{
	if (!expect(tokenid::identifier))
		return false;

	technique_info info;
	info.name = std::move(_token.literal_as_string);

	bool parse_success = parse_annotations(info.annotations);

	if (!expect('{'))
		return false;

	while (!peek('}'))
	{
		if (pass_info pass; parse_technique_pass(pass))
			info.passes.push_back(std::move(pass));
		else {
			parse_success = false;
			if (!peek(tokenid::pass) && !peek('}')) // If there is another pass definition following, try to parse that despite the error
				return consume_until('}'), false;
		}
	}

	_codegen->define_technique(info);

	return expect('}') && parse_success;
}

bool reshadefx::parser::parse_technique_pass(pass_info &info)
{
	if (!expect(tokenid::pass))
		return false;

	const auto pass_location = std::move(_token.location);

	// Passes can have an optional name, so consume and ignore that if it exists
	accept(tokenid::identifier);

	bool parse_success = true;
	bool targets_support_srgb = true;
	function_info vs_info, ps_info, cs_info;

	if (!expect('{'))
		return false;

	while (!peek('}'))
	{
		// Parse pass states
		if (!expect(tokenid::identifier))
			return consume_until('}'), false;

		auto location = std::move(_token.location);
		const auto state = std::move(_token.literal_as_string);

		if (!expect('='))
			return consume_until('}'), false;

		const bool is_shader_state = state == "VertexShader" || state == "PixelShader" || state == "ComputeShader";
		const bool is_texture_state = state.compare(0, 12, "RenderTarget") == 0 && (state.size() == 12 || (state[12] >= '0' && state[12] < '8'));

		// Shader and render target assignment looks up values in the symbol table, so handle those separately from the other states
		if (is_shader_state || is_texture_state)
		{
			scope scope;
			symbol symbol;
			std::string identifier;
			if (!accept_symbol(identifier, scope, symbol))
				return consume_until('}'), false;

			location = std::move(_token.location);

			int num_threads[2] = { 1, 1 };
			if (accept('<'))
			{
				expression x, y;
				if (!parse_expression_multary(x, 8) || !expect(',') || !parse_expression_multary(y, 8))
					return consume_until('}'), false;

				if (!x.is_constant)
					return error(x.location, 3011, "value must be a literal expression"), consume_until('}'), false;
				if (!y.is_constant)
					return error(y.location, 3011, "value must be a literal expression"), consume_until('}'), false;
				x.add_cast_operation({ type::t_int, 1, 1 });
				y.add_cast_operation({ type::t_int, 1, 1 });
				num_threads[0] = x.constant.as_int[0];
				num_threads[1] = y.constant.as_int[0];

				if (!expect('>'))
					return consume_until('}'), false;
			}

			// Ignore invalid symbols that were added during error recovery
			if (symbol.id != 0xFFFFFFFF)
			{
				if (is_shader_state)
				{
					if (!symbol.id)
						parse_success = false,
						error(location, 3501, "undeclared identifier '" + identifier + "', expected function name");
					else if (!symbol.type.is_function())
						parse_success = false,
						error(location, 3020, "type mismatch, expected function name");
					else {
						// Look up the matching function info for this function definition
						function_info &function_info = _codegen->find_function(symbol.id);

						// We potentially need to generate a special entry point function which translates between function parameters and input/output variables
						switch (state[0])
						{
						case 'V':
							vs_info = function_info;
							_codegen->define_entry_point(vs_info, shader_type::vs);
							info.vs_entry_point = vs_info.unique_name;
							break;
						case 'P':
							ps_info = function_info;
							_codegen->define_entry_point(ps_info, shader_type::ps);
							info.ps_entry_point = ps_info.unique_name;
							break;
						case 'C':
							cs_info = function_info;
							_codegen->define_entry_point(cs_info, shader_type::cs, num_threads);
							info.cs_entry_point = cs_info.unique_name;
							break;
						}
					}
				}
				else
				{
					assert(is_texture_state);

					if (!symbol.id)
						parse_success = false,
						error(location, 3004, "undeclared identifier '" + identifier + "', expected texture name");
					else if (!symbol.type.is_texture())
						parse_success = false,
						error(location, 3020, "type mismatch, expected texture name");
					else {
						reshadefx::texture_info &target_info = _codegen->find_texture(symbol.id);
						// Texture is used as a render target
						target_info.render_target = true;

						// Verify that all render targets in this pass have the same dimensions
						if (info.viewport_width != 0 && info.viewport_height != 0 && (target_info.width != info.viewport_width || target_info.height != info.viewport_height))
							parse_success = false,
							error(location, 4545, "cannot use multiple render targets with different texture dimensions (is " + std::to_string(target_info.width) + 'x' + std::to_string(target_info.height) + ", but expected " + std::to_string(info.viewport_width) + 'x' + std::to_string(info.viewport_height) + ')');

						info.viewport_width = target_info.width;
						info.viewport_height = target_info.height;

						const auto target_index = state.size() > 12 ? (state[12] - '0') : 0;
						info.render_target_names[target_index] = target_info.unique_name;

						// Only RGBA8 format supports sRGB writes across all APIs
						if (target_info.format != texture_format::rgba8)
							targets_support_srgb = false;
					}
				}
			}
			else
			{
				parse_success = false;
			}
		}
		else // Handle the rest of the pass states
		{
			backup();

			expression expression;

			if (accept(tokenid::identifier)) // Handle special enumeration names for pass states
			{
				// Transform identifier to uppercase to do case-insensitive comparison
				std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), [](char c) { return static_cast<char>(toupper(c)); });

				static const std::unordered_map<std::string, uint32_t> s_enum_values = {
					{ "NONE", 0 }, { "ZERO", 0 }, { "ONE", 1 },
					{ "ADD", uint32_t(pass_blend_op::add) },
					{ "SUBTRACT", uint32_t(pass_blend_op::subtract) },
					{ "REVSUBTRACT", uint32_t(pass_blend_op::rev_subtract) },
					{ "MIN", uint32_t(pass_blend_op::min) },
					{ "MAX", uint32_t(pass_blend_op::max) },
					{ "SRCCOLOR", uint32_t(pass_blend_func::src_color) },
					{ "SRCALPHA", uint32_t(pass_blend_func::src_alpha) },
					{ "INVSRCCOLOR", uint32_t(pass_blend_func::inv_src_color) },
					{ "INVSRCALPHA", uint32_t(pass_blend_func::inv_src_alpha) },
					{ "DESTCOLOR", uint32_t(pass_blend_func::dst_color) },
					{ "DESTALPHA", uint32_t(pass_blend_func::dst_alpha) },
					{ "INVDESTCOLOR", uint32_t(pass_blend_func::inv_dst_color) },
					{ "INVDESTALPHA", uint32_t(pass_blend_func::inv_dst_alpha) },
					{ "KEEP", uint32_t(pass_stencil_op::keep) },
					{ "REPLACE", uint32_t(pass_stencil_op::replace) },
					{ "INVERT", uint32_t(pass_stencil_op::invert) },
					{ "INCR", uint32_t(pass_stencil_op::incr) },
					{ "INCRSAT", uint32_t(pass_stencil_op::incr_sat) },
					{ "DECR", uint32_t(pass_stencil_op::decr) },
					{ "DECRSAT", uint32_t(pass_stencil_op::decr_sat) },
					{ "NEVER", uint32_t(pass_stencil_func::never) },
					{ "EQUAL", uint32_t(pass_stencil_func::equal) },
					{ "NEQUAL", uint32_t(pass_stencil_func::not_equal) }, { "NOTEQUAL", uint32_t(pass_stencil_func::not_equal)  },
					{ "LESS", uint32_t(pass_stencil_func::less) },
					{ "GREATER", uint32_t(pass_stencil_func::greater) },
					{ "LEQUAL", uint32_t(pass_stencil_func::less_equal) }, { "LESSEQUAL", uint32_t(pass_stencil_func::less_equal) },
					{ "GEQUAL", uint32_t(pass_stencil_func::greater_equal) }, { "GREATEREQUAL", uint32_t(pass_stencil_func::greater_equal) },
					{ "ALWAYS", uint32_t(pass_stencil_func::always) },
					{ "POINTS", uint32_t(primitive_topology::point_list) },
					{ "POINTLIST", uint32_t(primitive_topology::point_list) },
					{ "LINES", uint32_t(primitive_topology::line_list) },
					{ "LINELIST", uint32_t(primitive_topology::line_list) },
					{ "LINESTRIP", uint32_t(primitive_topology::line_strip) },
					{ "TRIANGLES", uint32_t(primitive_topology::triangle_list) },
					{ "TRIANGLELIST", uint32_t(primitive_topology::triangle_list) },
					{ "TRIANGLESTRIP", uint32_t(primitive_topology::triangle_strip) },
				};

				// Look up identifier in list of possible enumeration names
				if (const auto it = s_enum_values.find(_token.literal_as_string);
					it != s_enum_values.end())
					expression.reset_to_rvalue_constant(_token.location, it->second);
				else // No match found, so rewind to parser state before the identifier was consumed and try parsing it as a normal expression
					restore();
			}

			// Parse right hand side as normal expression if no special enumeration name was matched already
			if (!expression.is_constant && !parse_expression_multary(expression))
				return consume_until('}'), false;
			else if (!expression.is_constant || !expression.type.is_scalar())
				parse_success = false,
				error(expression.location, 3011, "pass state value must be a literal scalar expression");

			// All states below expect the value to be of an unsigned integer type
			expression.add_cast_operation({ type::t_uint, 1, 1 });
			const unsigned int value = expression.constant.as_uint[0];

			if (state == "SRGBWriteEnable")
				info.srgb_write_enable = value != 0;
			else if (state == "BlendEnable")
				info.blend_enable = value != 0;
			else if (state == "StencilEnable")
				info.stencil_enable = value != 0;
			else if (state == "ClearRenderTargets")
				info.clear_render_targets = value != 0;
			else if (state == "RenderTargetWriteMask" || state == "ColorWriteMask")
				info.color_write_mask = value & 0xFF;
			else if (state == "StencilReadMask" || state == "StencilMask")
				info.stencil_read_mask = value & 0xFF;
			else if (state == "StencilWriteMask")
				info.stencil_write_mask = value & 0xFF;
			else if (state == "BlendOp")
				info.blend_op = static_cast<pass_blend_op>(value);
			else if (state == "BlendOpAlpha")
				info.blend_op_alpha = static_cast<pass_blend_op>(value);
			else if (state == "SrcBlend")
				info.src_blend = static_cast<pass_blend_func>(value);
			else if (state == "SrcBlendAlpha")
				info.src_blend_alpha = static_cast<pass_blend_func>(value);
			else if (state == "DestBlend")
				info.dest_blend = static_cast<pass_blend_func>(value);
			else if (state == "DestBlendAlpha")
				info.dest_blend_alpha = static_cast<pass_blend_func>(value);
			else if (state == "StencilFunc")
				info.stencil_comparison_func = static_cast<pass_stencil_func>(value);
			else if (state == "StencilRef")
				info.stencil_reference_value = value;
			else if (state == "StencilPass" || state == "StencilPassOp")
				info.stencil_op_pass = static_cast<pass_stencil_op>(value);
			else if (state == "StencilFail" || state == "StencilFailOp")
				info.stencil_op_fail = static_cast<pass_stencil_op>(value);
			else if (state == "StencilZFail" || state == "StencilDepthFail" || state == "StencilDepthFailOp")
				info.stencil_op_depth_fail = static_cast<pass_stencil_op>(value);
			else if (state == "VertexCount")
				info.num_vertices = value;
			else if (state == "PrimitiveType" || state == "PrimitiveTopology")
				info.topology = static_cast<primitive_topology>(value);
			else if (state == "DispatchSizeX")
				info.viewport_width = value;
			else if (state == "DispatchSizeY")
				info.viewport_height = value;
			else
				parse_success = false,
				error(location, 3004, "unrecognized pass state '" + state + '\'');
		}

		if (!expect(';'))
			return consume_until('}'), false;
	}

	if (parse_success)
	{
		if (!info.cs_entry_point.empty())
		{
			if (info.viewport_width == 0 || info.viewport_height == 0)
			{
				parse_success = false;
				error(pass_location, 3012, "pass is missing 'DispatchSizeX' or 'DispatchSizeY' property");
			}
		}
		else if (info.vs_entry_point.empty() || info.ps_entry_point.empty())
		{
			parse_success = false;

			if (info.vs_entry_point.empty())
				error(pass_location, 3012, "pass is missing 'VertexShader' property");
			if (info.ps_entry_point.empty())
				error(pass_location, 3012,  "pass is missing 'PixelShader' property");
		}
		else
		{
			// Verify that shader signatures between VS and PS match (both semantics and interpolation qualifiers)
			std::unordered_map<std::string, type> vs_semantic_mapping;
			if (vs_info.return_semantic.empty())
			{
				if (!vs_info.return_type.is_void() && !vs_info.return_type.is_struct())
				{
					parse_success = false;
					error(pass_location, 3503, '\'' + vs_info.name + "': function return value is missing semantics");
				}
			}
			else
			{
				vs_semantic_mapping[vs_info.return_semantic] = vs_info.return_type;
			}

			for (const struct_member_info &param : vs_info.parameter_list)
			{
				if (param.semantic.empty())
				{
					if (!param.type.is_struct())
					{
						parse_success = false;
						if (param.type.has(type::q_in))
							error(pass_location, 3502, '\'' + vs_info.name + "': input parameter '" + param.name + "' is missing semantics");
						else
							error(pass_location, 3503, '\'' + vs_info.name + "': output parameter '" + param.name + "' is missing semantics");
					}
				}
				else if (param.type.has(type::q_out))
				{
					vs_semantic_mapping[param.semantic] = param.type;
				}
			}

			if (ps_info.return_semantic.empty())
			{
				if (!ps_info.return_type.is_void() && !ps_info.return_type.is_struct())
				{
					parse_success = false;
					error(pass_location, 3503, '\'' + ps_info.name + "': function return value is missing semantics");
				}
			}

			for (const struct_member_info &param : ps_info.parameter_list)
			{
				if (param.semantic.empty())
				{
					if (!param.type.is_struct())
					{
						parse_success = false;
						if (param.type.has(type::q_in))
							error(pass_location, 3502, '\'' + ps_info.name + "': input parameter '" + param.name + "' is missing semantics");
						else
							error(pass_location, 3503, '\'' + ps_info.name + "': output parameter '" + param.name + "' is missing semantics");
					}
				}
				else if (param.type.has(type::q_in))
				{
					if (const auto it = vs_semantic_mapping.find(param.semantic);
						it == vs_semantic_mapping.end() || it->second != param.type)
						warning(pass_location, 4576, '\'' + ps_info.name + "': input parameter '" + param.name + "' semantic does not match vertex shader one");
					else if (((it->second.qualifiers ^ param.type.qualifiers) & (type::q_linear | type::q_noperspective | type::q_centroid | type::q_nointerpolation)) != 0)
						parse_success = false,
						error(  pass_location, 4568, '\'' + ps_info.name + "': input parameter '" + param.name + "' interpolation qualifiers do not match vertex shader ones");
				}
			}
		}

		// Verify render target format supports sRGB writes if enabled
		if (info.srgb_write_enable && !targets_support_srgb)
			parse_success = false,
			error(pass_location, 4582, "one or more render target(s) do not support sRGB writes (only textures with RGBA8 format do)");
	}

	return expect('}') && parse_success;
}
