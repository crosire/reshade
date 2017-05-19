/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "parser.hpp"
#include "symbol_table.hpp"
#include "constant_folding.hpp"
#include <algorithm>

namespace reshadefx
{
	using namespace nodes;

	const std::string get_token_name(lexer::tokenid id)
	{
		switch (id)
		{
			default:
			case lexer::tokenid::unknown:
				return "unknown";
			case lexer::tokenid::end_of_file:
				return "end of file";
			case lexer::tokenid::exclaim:
				return "!";
			case lexer::tokenid::hash:
				return "#";
			case lexer::tokenid::dollar:
				return "$";
			case lexer::tokenid::percent:
				return "%";
			case lexer::tokenid::ampersand:
				return "&";
			case lexer::tokenid::parenthesis_open:
				return "(";
			case lexer::tokenid::parenthesis_close:
				return ")";
			case lexer::tokenid::star:
				return "*";
			case lexer::tokenid::plus:
				return "+";
			case lexer::tokenid::comma:
				return ",";
			case lexer::tokenid::minus:
				return "-";
			case lexer::tokenid::dot:
				return ".";
			case lexer::tokenid::slash:
				return "/";
			case lexer::tokenid::colon:
				return ":";
			case lexer::tokenid::semicolon:
				return ";";
			case lexer::tokenid::less:
				return "<";
			case lexer::tokenid::equal:
				return "=";
			case lexer::tokenid::greater:
				return ">";
			case lexer::tokenid::question:
				return "?";
			case lexer::tokenid::at:
				return "@";
			case lexer::tokenid::bracket_open:
				return "[";
			case lexer::tokenid::backslash:
				return "\\";
			case lexer::tokenid::bracket_close:
				return "]";
			case lexer::tokenid::caret:
				return "^";
			case lexer::tokenid::brace_open:
				return "{";
			case lexer::tokenid::pipe:
				return "|";
			case lexer::tokenid::brace_close:
				return "}";
			case lexer::tokenid::tilde:
				return "~";
			case lexer::tokenid::exclaim_equal:
				return "!=";
			case lexer::tokenid::percent_equal:
				return "%=";
			case lexer::tokenid::ampersand_ampersand:
				return "&&";
			case lexer::tokenid::ampersand_equal:
				return "&=";
			case lexer::tokenid::star_equal:
				return "*=";
			case lexer::tokenid::plus_plus:
				return "++";
			case lexer::tokenid::plus_equal:
				return "+=";
			case lexer::tokenid::minus_minus:
				return "--";
			case lexer::tokenid::minus_equal:
				return "-=";
			case lexer::tokenid::arrow:
				return "->";
			case lexer::tokenid::ellipsis:
				return "...";
			case lexer::tokenid::slash_equal:
				return "|=";
			case lexer::tokenid::colon_colon:
				return "::";
			case lexer::tokenid::less_less_equal:
				return "<<=";
			case lexer::tokenid::less_less:
				return "<<";
			case lexer::tokenid::less_equal:
				return "<=";
			case lexer::tokenid::equal_equal:
				return "==";
			case lexer::tokenid::greater_greater_equal:
				return ">>=";
			case lexer::tokenid::greater_greater:
				return ">>";
			case lexer::tokenid::greater_equal:
				return ">=";
			case lexer::tokenid::caret_equal:
				return "^=";
			case lexer::tokenid::pipe_equal:
				return "|=";
			case lexer::tokenid::pipe_pipe:
				return "||";
			case lexer::tokenid::identifier:
				return "identifier";
			case lexer::tokenid::reserved:
				return "reserved word";
			case lexer::tokenid::true_literal:
				return "true";
			case lexer::tokenid::false_literal:
				return "false";
			case lexer::tokenid::int_literal:
			case lexer::tokenid::uint_literal:
				return "integral literal";
			case lexer::tokenid::float_literal:
			case lexer::tokenid::double_literal:
				return "floating point literal";
			case lexer::tokenid::string_literal:
				return "string literal";
			case lexer::tokenid::namespace_:
				return "namespace";
			case lexer::tokenid::struct_:
				return "struct";
			case lexer::tokenid::technique:
				return "technique";
			case lexer::tokenid::pass:
				return "pass";
			case lexer::tokenid::for_:
				return "for";
			case lexer::tokenid::while_:
				return "while";
			case lexer::tokenid::do_:
				return "do";
			case lexer::tokenid::if_:
				return "if";
			case lexer::tokenid::else_:
				return "else";
			case lexer::tokenid::switch_:
				return "switch";
			case lexer::tokenid::case_:
				return "case";
			case lexer::tokenid::default_:
				return "default";
			case lexer::tokenid::break_:
				return "break";
			case lexer::tokenid::continue_:
				return "continue";
			case lexer::tokenid::return_:
				return "return";
			case lexer::tokenid::discard_:
				return "discard";
			case lexer::tokenid::extern_:
				return "extern";
			case lexer::tokenid::static_:
				return "static";
			case lexer::tokenid::uniform_:
				return "uniform";
			case lexer::tokenid::volatile_:
				return "volatile";
			case lexer::tokenid::precise:
				return "precise";
			case lexer::tokenid::in:
				return "in";
			case lexer::tokenid::out:
				return "out";
			case lexer::tokenid::inout:
				return "inout";
			case lexer::tokenid::const_:
				return "const";
			case lexer::tokenid::linear:
				return "linear";
			case lexer::tokenid::noperspective:
				return "noperspective";
			case lexer::tokenid::centroid:
				return "centroid";
			case lexer::tokenid::nointerpolation:
				return "nointerpolation";
			case lexer::tokenid::void_:
				return "void";
			case lexer::tokenid::bool_:
			case lexer::tokenid::bool2:
			case lexer::tokenid::bool3:
			case lexer::tokenid::bool4:
			case lexer::tokenid::bool2x2:
			case lexer::tokenid::bool3x3:
			case lexer::tokenid::bool4x4:
				return "bool type";
			case lexer::tokenid::int_:
			case lexer::tokenid::int2:
			case lexer::tokenid::int3:
			case lexer::tokenid::int4:
			case lexer::tokenid::int2x2:
			case lexer::tokenid::int3x3:
			case lexer::tokenid::int4x4:
				return "int type";
			case lexer::tokenid::uint_:
			case lexer::tokenid::uint2:
			case lexer::tokenid::uint3:
			case lexer::tokenid::uint4:
			case lexer::tokenid::uint2x2:
			case lexer::tokenid::uint3x3:
			case lexer::tokenid::uint4x4:
				return "uint type";
			case lexer::tokenid::float_:
			case lexer::tokenid::float2:
			case lexer::tokenid::float3:
			case lexer::tokenid::float4:
			case lexer::tokenid::float2x2:
			case lexer::tokenid::float3x3:
			case lexer::tokenid::float4x4:
				return "float type";
			case lexer::tokenid::vector:
				return "vector";
			case lexer::tokenid::matrix:
				return "matrix";
			case lexer::tokenid::string_:
				return "string";
			case lexer::tokenid::texture:
				return "texture";
			case lexer::tokenid::sampler:
				return "sampler";
		}
	}

	parser::parser(syntax_tree &ast, std::string &errors) :
		_ast(ast),
		_errors(errors),
		_symbol_table(new symbol_table())
	{
	}
	parser::~parser()
	{
	}

	bool parser::run(const std::string &input)
	{
		_lexer.reset(new lexer(input));
		_lexer_backup.reset();

		consume();

		while (!peek(lexer::tokenid::end_of_file))
		{
			if (!parse_top_level())
			{
				return false;
			}
		}

		return true;
	}

	// Error handling
	void parser::error(const location &location, unsigned int code, const std::string &message)
	{
		_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

		if (code == 0)
		{
			_errors += "error: ";
		}
		else
		{
			_errors += "error X" + std::to_string(code) + ": ";
		}

		_errors += message + '\n';
	}
	void parser::warning(const location &location, unsigned int code, const std::string &message)
	{
		_errors += location.source + '(' + std::to_string(location.line) + ", " + std::to_string(location.column) + ')' + ": ";

		if (code == 0)
		{
			_errors += "warning: ";
		}
		else
		{
			_errors += "warning X" + std::to_string(code) + ": ";
		}

		_errors += message + '\n';
	}

	// Input management
	void parser::backup()
	{
		_lexer.swap(_lexer_backup);
		_lexer.reset(new lexer(*_lexer_backup));
		_token_backup = _token_next;
	}
	void parser::restore()
	{
		_lexer.swap(_lexer_backup);
		_token_next = _token_backup;
	}

	bool parser::peek(char tok) const
	{
		return peek(static_cast<lexer::tokenid>(tok));
	}
	bool parser::peek(lexer::tokenid tokid) const
	{
		return _token_next.id == tokid;
	}
	void parser::consume()
	{
		_token = _token_next;
		_token_next = _lexer->lex();
	}
	void parser::consume_until(char token)
	{
		consume_until(static_cast<lexer::tokenid>(token));
	}
	void parser::consume_until(lexer::tokenid tokid)
	{
		while (!accept(tokid) && !peek(lexer::tokenid::end_of_file))
		{
			consume();
		}
	}
	bool parser::accept(char tok)
	{
		return accept(static_cast<lexer::tokenid>(tok));
	}
	bool parser::accept(lexer::tokenid tokid)
	{
		if (peek(tokid))
		{
			consume();

			return true;
		}

		return false;
	}
	bool parser::expect(char tok)
	{
		return expect(static_cast<lexer::tokenid>(tok));
	}
	bool parser::expect(lexer::tokenid tokid)
	{
		if (!accept(tokid))
		{
			error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected '" + get_token_name(tokid) + "'");

			return false;
		}

		return true;
	}

	// Types
	bool parser::accept_type_class(type_node &type)
	{
		type.definition = nullptr;
		type.array_length = 0;

		if (peek(lexer::tokenid::identifier))
		{
			type.rows = type.cols = 0;
			type.basetype = type_node::datatype_struct;

			const auto symbol = _symbol_table->find(_token_next.literal_as_string);

			if (symbol != nullptr && symbol->id == nodeid::struct_declaration)
			{
				type.definition = static_cast<struct_declaration_node *>(symbol);

				consume();
			}
			else
			{
				return false;
			}
		}
		else if (accept(lexer::tokenid::vector))
		{
			type.rows = 4, type.cols = 1;
			type.basetype = type_node::datatype_float;

			if (accept('<'))
			{
				if (!accept_type_class(type))
				{
					error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected vector element type");

					return false;
				}

				if (!type.is_scalar())
				{
					error(_token.location, 3122, "vector element type must be a scalar type");

					return false;
				}

				if (!(expect(',') && expect(lexer::tokenid::int_literal)))
				{
					return false;
				}

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				{
					error(_token.location, 3052, "vector dimension must be between 1 and 4");

					return false;
				}

				type.rows = _token.literal_as_int;

				if (!expect('>'))
				{
					return false;
				}
			}
		}
		else if (accept(lexer::tokenid::matrix))
		{
			type.rows = 4, type.cols = 4;
			type.basetype = type_node::datatype_float;

			if (accept('<'))
			{
				if (!accept_type_class(type))
				{
					error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected matrix element type");

					return false;
				}

				if (!type.is_scalar())
				{
					error(_token.location, 3123, "matrix element type must be a scalar type");

					return false;
				}

				if (!(expect(',') && expect(lexer::tokenid::int_literal)))
				{
					return false;
				}

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				{
					error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

					return false;
				}

				type.rows = _token.literal_as_int;

				if (!(expect(',') && expect(lexer::tokenid::int_literal)))
				{
					return false;
				}

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
				{
					error(_token.location, 3053, "matrix dimensions must be between 1 and 4");

					return false;
				}

				type.cols = _token.literal_as_int;

				if (!expect('>'))
				{
					return false;
				}
			}
		}
		else
		{
			type.rows = type.cols = 0;

			switch (_token_next.id)
			{
				case lexer::tokenid::void_:
					type.basetype = type_node::datatype_void;
					break;
				case lexer::tokenid::bool_:
					type.rows = 1, type.cols = 1;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool2:
					type.rows = 2, type.cols = 1;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool2x2:
					type.rows = 2, type.cols = 2;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool3:
					type.rows = 3, type.cols = 1;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool3x3:
					type.rows = 3, type.cols = 3;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool4:
					type.rows = 4, type.cols = 1;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::bool4x4:
					type.rows = 4, type.cols = 4;
					type.basetype = type_node::datatype_bool;
					break;
				case lexer::tokenid::int_:
					type.rows = 1, type.cols = 1;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int2:
					type.rows = 2, type.cols = 1;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int2x2:
					type.rows = 2, type.cols = 2;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int3:
					type.rows = 3, type.cols = 1;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int3x3:
					type.rows = 3, type.cols = 3;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int4:
					type.rows = 4, type.cols = 1;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::int4x4:
					type.rows = 4, type.cols = 4;
					type.basetype = type_node::datatype_int;
					break;
				case lexer::tokenid::uint_:
					type.rows = 1, type.cols = 1;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint2:
					type.rows = 2, type.cols = 1;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint2x2:
					type.rows = 2, type.cols = 2;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint3:
					type.rows = 3, type.cols = 1;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint3x3:
					type.rows = 3, type.cols = 3;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint4:
					type.rows = 4, type.cols = 1;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::uint4x4:
					type.rows = 4, type.cols = 4;
					type.basetype = type_node::datatype_uint;
					break;
				case lexer::tokenid::float_:
					type.rows = 1, type.cols = 1;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float2:
					type.rows = 2, type.cols = 1;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float2x2:
					type.rows = 2, type.cols = 2;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float3:
					type.rows = 3, type.cols = 1;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float3x3:
					type.rows = 3, type.cols = 3;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float4:
					type.rows = 4, type.cols = 1;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::float4x4:
					type.rows = 4, type.cols = 4;
					type.basetype = type_node::datatype_float;
					break;
				case lexer::tokenid::string_:
					type.basetype = type_node::datatype_string;
					break;
				case lexer::tokenid::texture:
					type.basetype = type_node::datatype_texture;
					break;
				case lexer::tokenid::sampler:
					type.basetype = type_node::datatype_sampler;
					break;
				default:
					return false;
			}

			consume();
		}

		return true;
	}
	bool parser::accept_type_qualifiers(type_node &type)
	{
		unsigned int qualifiers = 0;

		// Storage
		if (accept(lexer::tokenid::extern_))
		{
			qualifiers |= type_node::qualifier_extern;
		}
		if (accept(lexer::tokenid::static_))
		{
			qualifiers |= type_node::qualifier_static;
		}
		if (accept(lexer::tokenid::uniform_))
		{
			qualifiers |= type_node::qualifier_uniform;
		}
		if (accept(lexer::tokenid::volatile_))
		{
			qualifiers |= type_node::qualifier_volatile;
		}
		if (accept(lexer::tokenid::precise))
		{
			qualifiers |= type_node::qualifier_precise;
		}

		if (accept(lexer::tokenid::in))
		{
			qualifiers |= type_node::qualifier_in;
		}
		if (accept(lexer::tokenid::out))
		{
			qualifiers |= type_node::qualifier_out;
		}
		if (accept(lexer::tokenid::inout))
		{
			qualifiers |= type_node::qualifier_inout;
		}

		// Modifiers
		if (accept(lexer::tokenid::const_))
		{
			qualifiers |= type_node::qualifier_const;
		}

		// Interpolation
		if (accept(lexer::tokenid::linear))
		{
			qualifiers |= type_node::qualifier_linear;
		}
		if (accept(lexer::tokenid::noperspective))
		{
			qualifiers |= type_node::qualifier_noperspective;
		}
		if (accept(lexer::tokenid::centroid))
		{
			qualifiers |= type_node::qualifier_centroid;
		}
		if (accept(lexer::tokenid::nointerpolation))
		{
			qualifiers |= type_node::qualifier_nointerpolation;
		}

		if (qualifiers == 0)
		{
			return false;
		}
		if ((type.qualifiers & qualifiers) == qualifiers)
		{
			warning(_token.location, 3048, "duplicate usages specified");
		}

		type.qualifiers |= qualifiers;

		accept_type_qualifiers(type);

		return true;
	}
	bool parser::parse_type(type_node &type)
	{
		type.qualifiers = 0;

		accept_type_qualifiers(type);

		const auto location = _token_next.location;

		if (!accept_type_class(type))
		{
			return false;
		}

		if (type.is_integral() && (type.has_qualifier(type_node::qualifier_centroid) || type.has_qualifier(type_node::qualifier_noperspective)))
		{
			error(location, 4576, "signature specifies invalid interpolation mode for integer component type");

			return false;
		}

		if (type.has_qualifier(type_node::qualifier_centroid) && !type.has_qualifier(type_node::qualifier_noperspective))
		{
			type.qualifiers |= type_node::qualifier_linear;
		}

		return true;
	}

	// Expressions
	bool parser::accept_unary_op(enum unary_expression_node::op &op)
	{
		switch (_token_next.id)
		{
			case lexer::tokenid::exclaim:
				op = unary_expression_node::logical_not;
				break;
			case lexer::tokenid::plus:
				op = unary_expression_node::none;
				break;
			case lexer::tokenid::minus:
				op = unary_expression_node::negate;
				break;
			case lexer::tokenid::tilde:
				op = unary_expression_node::bitwise_not;
				break;
			case lexer::tokenid::plus_plus:
				op = unary_expression_node::pre_increase;
				break;
			case lexer::tokenid::minus_minus:
				op = unary_expression_node::pre_decrease;
				break;
			default:
				return false;
		}

		consume();

		return true;
	}
	bool parser::accept_postfix_op(enum unary_expression_node::op &op)
	{
		switch (_token_next.id)
		{
			case lexer::tokenid::plus_plus:
				op = unary_expression_node::post_increase;
				break;
			case lexer::tokenid::minus_minus:
				op = unary_expression_node::post_decrease;
				break;
			default:
				return false;
		}

		consume();

		return true;
	}
	bool parser::peek_multary_op(enum binary_expression_node::op &op, unsigned int &precedence) const
	{
		switch (_token_next.id)
		{
			case lexer::tokenid::percent:
				op = binary_expression_node::modulo;
				precedence = 11;
				break;
			case lexer::tokenid::ampersand:
				op = binary_expression_node::bitwise_and;
				precedence = 6;
				break;
			case lexer::tokenid::star:
				op = binary_expression_node::multiply;
				precedence = 11;
				break;
			case lexer::tokenid::plus:
				op = binary_expression_node::add;
				precedence = 10;
				break;
			case lexer::tokenid::minus:
				op = binary_expression_node::subtract;
				precedence = 10;
				break;
			case lexer::tokenid::slash:
				op = binary_expression_node::divide;
				precedence = 11;
				break;
			case lexer::tokenid::less:
				op = binary_expression_node::less;
				precedence = 8;
				break;
			case lexer::tokenid::greater:
				op = binary_expression_node::greater;
				precedence = 8;
				break;
			case lexer::tokenid::question:
				op = binary_expression_node::none;
				precedence = 1;
				break;
			case lexer::tokenid::caret:
				op = binary_expression_node::bitwise_xor;
				precedence = 5;
				break;
			case lexer::tokenid::pipe:
				op = binary_expression_node::bitwise_or;
				precedence = 4;
				break;
			case lexer::tokenid::exclaim_equal:
				op = binary_expression_node::not_equal;
				precedence = 7;
				break;
			case lexer::tokenid::ampersand_ampersand:
				op = binary_expression_node::logical_and;
				precedence = 3;
				break;
			case lexer::tokenid::less_less:
				op = binary_expression_node::left_shift;
				precedence = 9;
				break;
			case lexer::tokenid::less_equal:
				op = binary_expression_node::less_equal;
				precedence = 8;
				break;
			case lexer::tokenid::equal_equal:
				op = binary_expression_node::equal;
				precedence = 7;
				break;
			case lexer::tokenid::greater_greater:
				op = binary_expression_node::right_shift;
				precedence = 9;
				break;
			case lexer::tokenid::greater_equal:
				op = binary_expression_node::greater_equal;
				precedence = 8;
				break;
			case lexer::tokenid::pipe_pipe:
				op = binary_expression_node::logical_or;
				precedence = 2;
				break;
			default:
				return false;
		}

		return true;
	}
	bool parser::accept_assignment_op(enum assignment_expression_node::op &op)
	{
		switch (_token_next.id)
		{
			case lexer::tokenid::equal:
				op = assignment_expression_node::none;
				break;
			case lexer::tokenid::percent_equal:
				op = assignment_expression_node::modulo;
				break;
			case lexer::tokenid::ampersand_equal:
				op = assignment_expression_node::bitwise_and;
				break;
			case lexer::tokenid::star_equal:
				op = assignment_expression_node::multiply;
				break;
			case lexer::tokenid::plus_equal:
				op = assignment_expression_node::add;
				break;
			case lexer::tokenid::minus_equal:
				op = assignment_expression_node::subtract;
				break;
			case lexer::tokenid::slash_equal:
				op = assignment_expression_node::divide;
				break;
			case lexer::tokenid::less_less_equal:
				op = assignment_expression_node::left_shift;
				break;
			case lexer::tokenid::greater_greater_equal:
				op = assignment_expression_node::right_shift;
				break;
			case lexer::tokenid::caret_equal:
				op = assignment_expression_node::bitwise_xor;
				break;
			case lexer::tokenid::pipe_equal:
				op = assignment_expression_node::bitwise_or;
				break;
			default:
				return false;
		}

		consume();

		return true;
	}
	bool parser::parse_expression(expression_node *&node)
	{
		if (!parse_expression_assignment(node))
		{
			return false;
		}

		if (peek(','))
		{
			const auto sequence = _ast.make_node<expression_sequence_node>(node->location);
			sequence->expression_list.push_back(node);

			while (accept(','))
			{
				expression_node *expression = nullptr;

				if (!parse_expression_assignment(expression))
				{
					return false;
				}

				sequence->expression_list.push_back(std::move(expression));
			}

			sequence->type = sequence->expression_list.back()->type;

			node = sequence;
		}

		return true;
	}
	bool parser::parse_expression_unary(expression_node *&node)
	{
		type_node type;
		enum unary_expression_node::op op;
		auto location = _token_next.location;

		#pragma region Prefix
		if (accept_unary_op(op))
		{
			if (!parse_expression_unary(node))
			{
				return false;
			}

			if (!node->type.is_scalar() && !node->type.is_vector() && !node->type.is_matrix())
			{
				error(node->location, 3022, "scalar, vector, or matrix expected");

				return false;
			}

			if (op != unary_expression_node::none)
			{
				if (op == unary_expression_node::bitwise_not && !node->type.is_integral())
				{
					error(node->location, 3082, "int or unsigned int type required");

					return false;
				}
				else if ((op == unary_expression_node::pre_increase || op == unary_expression_node::pre_decrease) && (node->type.has_qualifier(type_node::qualifier_const) || node->type.has_qualifier(type_node::qualifier_uniform)))
				{
					error(node->location, 3025, "l-value specifies const object");

					return false;
				}

				const auto newexpression = _ast.make_node<unary_expression_node>(location);
				newexpression->type = node->type;
				newexpression->op = op;
				newexpression->operand = node;

				node = fold_constant_expression(_ast, newexpression);
			}

			type = node->type;
		}
		else if (accept('('))
		{
			backup();

			if (accept_type_class(type))
			{
				if (peek('('))
				{
					restore();
				}
				else if (expect(')'))
				{
					if (!parse_expression_unary(node))
					{
						return false;
					}

					if (node->type.basetype == type.basetype && (node->type.rows == type.rows && node->type.cols == type.cols) && !(node->type.is_array() || type.is_array()))
					{
						return true;
					}
					else if (node->type.is_numeric() && type.is_numeric())
					{
						if ((node->type.rows < type.rows || node->type.cols < type.cols) && !node->type.is_scalar())
						{
							error(location, 3017, "cannot convert these vector types");

							return false;
						}

						if (node->type.rows > type.rows || node->type.cols > type.cols)
						{
							warning(location, 3206, "implicit truncation of vector type");
						}

						const auto castexpression = _ast.make_node<unary_expression_node>(location);
						type.qualifiers = type_node::qualifier_const;
						castexpression->type = type;
						castexpression->op = unary_expression_node::cast;
						castexpression->operand = node;

						node = fold_constant_expression(_ast, castexpression);

						return true;
					}
					else
					{
						error(location, 3017, "cannot convert non-numeric types");

						return false;
					}
				}
				else
				{
					return false;
				}
			}

			if (!parse_expression(node))
			{
				return false;
			}

			if (!expect(')'))
			{
				return false;
			}

			type = node->type;
		}
		else if (accept(lexer::tokenid::true_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_bool;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_int[0] = 1;

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::false_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_bool;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_int[0] = 0;

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::int_literal))
		{
			literal_expression_node *const literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_int;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_int[0] = _token.literal_as_int;

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::uint_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_uint;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_uint[0] = _token.literal_as_uint;

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::float_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_float;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_float[0] = _token.literal_as_float;

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::double_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_float;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 1, literal->type.array_length = 0;
			literal->value_float[0] = static_cast<float>(_token.literal_as_double);

			node = literal;
			type = literal->type;
		}
		else if (accept(lexer::tokenid::string_literal))
		{
			const auto literal = _ast.make_node<literal_expression_node>(location);
			literal->type.basetype = type_node::datatype_string;
			literal->type.qualifiers = type_node::qualifier_const;
			literal->type.rows = literal->type.cols = 0, literal->type.array_length = 0;
			literal->value_string = _token.literal_as_string;

			while (accept(lexer::tokenid::string_literal))
			{
				literal->value_string += _token.literal_as_string;
			}

			node = literal;
			type = literal->type;
		}
		else if (accept_type_class(type))
		{
			if (!expect('('))
			{
				return false;
			}

			if (!type.is_numeric())
			{
				error(location, 3037, "constructors only defined for numeric base types");

				return false;
			}

			if (accept(')'))
			{
				error(location, 3014, "incorrect number of arguments to numeric-type constructor");

				return false;
			}

			const auto constructor = _ast.make_node<constructor_expression_node>(location);
			constructor->type = type;
			constructor->type.qualifiers = type_node::qualifier_const;

			unsigned int elements = 0;

			while (!peek(')'))
			{
				if (!constructor->arguments.empty() && !expect(','))
				{
					return false;
				}

				expression_node *argument = nullptr;

				if (!parse_expression_assignment(argument))
				{
					return false;
				}

				if (!argument->type.is_numeric())
				{
					error(argument->location, 3017, "cannot convert non-numeric types");

					return false;
				}

				elements += argument->type.rows * argument->type.cols;

				constructor->arguments.push_back(std::move(argument));
			}

			if (!expect(')'))
			{
				return false;
			}

			if (elements != type.rows * type.cols)
			{
				error(location, 3014, "incorrect number of arguments to numeric-type constructor");

				return false;
			}

			if (constructor->arguments.size() > 1)
			{
				node = constructor;
				type = constructor->type;
			}
			else
			{
				const auto castexpression = _ast.make_node<unary_expression_node>(constructor->location);
				castexpression->type = type;
				castexpression->op = unary_expression_node::cast;
				castexpression->operand = constructor->arguments[0];

				node = castexpression;
			}

			node = fold_constant_expression(_ast, node);
		}
		else
		{
			scope scope;
			bool exclusive;
			std::string identifier;

			if (accept(lexer::tokenid::colon_colon))
			{
				scope.namespace_level = scope.level = 0;
				exclusive = true;
			}
			else
			{
				scope = _symbol_table->current_scope();
				exclusive = false;
			}

			if (exclusive ? expect(lexer::tokenid::identifier) : accept(lexer::tokenid::identifier))
			{
				identifier = _token.literal_as_string;
			}
			else
			{
				return false;
			}

			while (accept(lexer::tokenid::colon_colon))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				identifier += "::" + _token.literal_as_string;
			}

			const auto symbol = _symbol_table->find(identifier, scope, exclusive);

			if (accept('('))
			{
				if (symbol != nullptr && symbol->id == nodeid::variable_declaration)
				{
					error(location, 3005, "identifier '" + identifier + "' represents a variable, not a function");

					return false;
				}

				const auto callexpression = _ast.make_node<call_expression_node>(location);
				callexpression->callee_name = identifier;

				while (!peek(')'))
				{
					if (!callexpression->arguments.empty() && !expect(','))
					{
						return false;
					}

					expression_node *argument = nullptr;

					if (!parse_expression_assignment(argument))
					{
						return false;
					}

					callexpression->arguments.push_back(std::move(argument));
				}

				if (!expect(')'))
				{
					return false;
				}

				bool undeclared = symbol == nullptr, intrinsic = false, ambiguous = false;

				if (!_symbol_table->resolve_call(callexpression, scope, intrinsic, ambiguous))
				{
					if (undeclared && !intrinsic)
					{
						error(location, 3004, "undeclared identifier '" + identifier + "'");
					}
					else if (ambiguous)
					{
						error(location, 3067, "ambiguous function call to '" + identifier + "'");
					}
					else
					{
						error(location, 3013, "no matching function overload for '" + identifier + "'");
					}

					return false;
				}

				if (intrinsic)
				{
					const auto newexpression = _ast.make_node<intrinsic_expression_node>(callexpression->location);
					newexpression->type = callexpression->type;
					newexpression->op = static_cast<enum intrinsic_expression_node::op>(callexpression->callee_name[0]);

					for (size_t i = 0, count = std::min(callexpression->arguments.size(), _countof(newexpression->arguments)); i < count; ++i)
					{
						newexpression->arguments[i] = callexpression->arguments[i];
					}

					node = fold_constant_expression(_ast, newexpression);
				}
				else
				{
					const auto parent = _symbol_table->current_parent();

					if (parent == callexpression->callee)
					{
						error(location, 3500, "recursive function calls are not allowed");

						return false;
					}

					node = callexpression;
				}

				type = node->type;

				for (size_t i = 0; i < callexpression->arguments.size(); i++)
				{
					const auto argument = callexpression->arguments[i];
					const auto parameter = callexpression->callee->parameter_list[i];

					if (argument->type.rows > parameter->type.rows || argument->type.cols > parameter->type.cols)
					{
						warning(argument->location, 3206, "implicit truncation of vector type");
					}
				}
			}
			else
			{
				if (symbol == nullptr)
				{
					error(location, 3004, "undeclared identifier '" + identifier + "'");

					return false;
				}

				if (symbol->id != nodeid::variable_declaration)
				{
					error(location, 3005, "identifier '" + identifier + "' represents a function, not a variable");

					return false;
				}

				const auto newexpression = _ast.make_node<lvalue_expression_node>(location);
				newexpression->reference = static_cast<const variable_declaration_node *>(symbol);
				newexpression->type = newexpression->reference->type;

				node = fold_constant_expression(_ast, newexpression);
				type = node->type;
			}
		}
		#pragma endregion

		#pragma region Postfix
		while (!peek(lexer::tokenid::end_of_file))
		{
			location = _token_next.location;

			if (accept_postfix_op(op))
			{
				if (!type.is_scalar() && !type.is_vector() && !type.is_matrix())
				{
					error(node->location, 3022, "scalar, vector, or matrix expected");

					return false;
				}

				if (type.has_qualifier(type_node::qualifier_const) || type.has_qualifier(type_node::qualifier_uniform))
				{
					error(node->location, 3025, "l-value specifies const object");

					return false;
				}

				const auto newexpression = _ast.make_node<unary_expression_node>(location);
				newexpression->type = type;
				newexpression->type.qualifiers |= type_node::qualifier_const;
				newexpression->op = op;
				newexpression->operand = node;

				node = newexpression;
				type = node->type;
			}
			else if (accept('.'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					return false;
				}

				location = _token.location;
				const auto subscript = _token.literal_as_string;

				if (accept('('))
				{
					if (!type.is_struct() || type.is_array())
					{
						error(location, 3087, "object does not have methods");
					}
					else
					{
						error(location, 3088, "structures do not have methods");
					}

					return false;
				}
				else if (type.is_array())
				{
					error(location, 3018, "invalid subscript on array");

					return false;
				}
				else if (type.is_vector())
				{
					const size_t length = subscript.size();

					if (length > 4)
					{
						error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long");

						return false;
					}

					bool constant = false;
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
								error(location, 3018, "invalid subscript '" + subscript + "'");
								return false;
						}

						if (i > 0 && (set[i] != set[i - 1]))
						{
							error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets");

							return false;
						}
						if (static_cast<unsigned int>(offsets[i]) >= type.rows)
						{
							error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range");

							return false;
						}

						for (size_t k = 0; k < i; ++k)
						{
							if (offsets[k] == offsets[i])
							{
								constant = true;
								break;
							}
						}
					}

					const auto newexpression = _ast.make_node<swizzle_expression_node>(location);
					newexpression->type = type;
					newexpression->type.rows = static_cast<unsigned int>(length);
					newexpression->operand = node;
					newexpression->mask[0] = offsets[0];
					newexpression->mask[1] = offsets[1];
					newexpression->mask[2] = offsets[2];
					newexpression->mask[3] = offsets[3];

					if (constant || type.has_qualifier(type_node::qualifier_uniform))
					{
						newexpression->type.qualifiers |= type_node::qualifier_const;
						newexpression->type.qualifiers &= ~type_node::qualifier_uniform;
					}

					node = fold_constant_expression(_ast, newexpression);
					type = node->type;
				}
				else if (type.is_matrix())
				{
					const size_t length = subscript.size();

					if (length < 3)
					{
						error(location, 3018, "invalid subscript '" + subscript + "'");

						return false;
					}

					bool constant = false;
					signed char offsets[4] = { -1, -1, -1, -1 };
					const unsigned int set = subscript[1] == 'm';
					const char coefficient = static_cast<char>(!set);

					for (size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
					{
						if (subscript[i] != '_' || subscript[i + set + 1] < '0' + coefficient || subscript[i + set + 1] > '3' + coefficient || subscript[i + set + 2] < '0' + coefficient || subscript[i + set + 2] > '3' + coefficient)
						{
							error(location, 3018, "invalid subscript '" + subscript + "'");

							return false;
						}
						if (set && subscript[i + 1] != 'm')
						{
							error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets");

							return false;
						}

						const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
						const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

						if ((row >= type.rows || col >= type.cols) || j > 3)
						{
							error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range");

							return false;
						}

						offsets[j] = static_cast<unsigned char>(row * 4 + col);

						for (size_t k = 0; k < j; ++k)
						{
							if (offsets[k] == offsets[j])
							{
								constant = true;
								break;
							}
						}
					}

					const auto newexpression = _ast.make_node<swizzle_expression_node>(_token.location);
					newexpression->type = type;
					newexpression->type.rows = static_cast<unsigned int>(length / (3 + set));
					newexpression->type.cols = 1;
					newexpression->operand = node;
					newexpression->mask[0] = offsets[0];
					newexpression->mask[1] = offsets[1];
					newexpression->mask[2] = offsets[2];
					newexpression->mask[3] = offsets[3];

					if (constant || type.has_qualifier(type_node::qualifier_uniform))
					{
						newexpression->type.qualifiers |= type_node::qualifier_const;
						newexpression->type.qualifiers &= ~type_node::qualifier_uniform;
					}

					node = fold_constant_expression(_ast, newexpression);
					type = node->type;
				}
				else if (type.is_struct())
				{
					variable_declaration_node *field = nullptr;

					for (auto currfield : type.definition->field_list)
					{
						if (currfield->name == subscript)
						{
							field = currfield;
							break;
						}
					}

					if (field == nullptr)
					{
						error(location, 3018, "invalid subscript '" + subscript + "'");

						return false;
					}

					const auto newexpression = _ast.make_node<field_expression_node>(location);
					newexpression->type = field->type;
					newexpression->operand = node;
					newexpression->field_reference = field;

					if (type.has_qualifier(type_node::qualifier_uniform))
					{
						newexpression->type.qualifiers |= type_node::qualifier_const;
						newexpression->type.qualifiers &= ~type_node::qualifier_uniform;
					}

					node = newexpression;
					type = node->type;
				}
				else if (type.is_scalar())
				{
					signed char offsets[4] = { -1, -1, -1, -1 };
					const size_t length = subscript.size();

					for (size_t i = 0; i < length; ++i)
					{
						if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
						{
							error(location, 3018, "invalid subscript '" + subscript + "'");

							return false;
						}

						offsets[i] = 0;
					}

					const auto newexpression = _ast.make_node<swizzle_expression_node>(location);
					newexpression->type = type;
					newexpression->type.qualifiers |= type_node::qualifier_const;
					newexpression->type.rows = static_cast<unsigned int>(length);
					newexpression->operand = node;
					newexpression->mask[0] = offsets[0];
					newexpression->mask[1] = offsets[1];
					newexpression->mask[2] = offsets[2];
					newexpression->mask[3] = offsets[3];

					node = newexpression;
					type = node->type;
				}
				else
				{
					error(location, 3018, "invalid subscript '" + subscript + "'");

					return false;
				}
			}
			else if (accept('['))
			{
				if (!type.is_array() && !type.is_vector() && !type.is_matrix())
				{
					error(node->location, 3121, "array, matrix, vector, or indexable object type expected in index expression");

					return false;
				}

				const auto newexpression = _ast.make_node<binary_expression_node>(location);
				newexpression->type = type;
				newexpression->op = binary_expression_node::element_extract;
				newexpression->operands[0] = node;

				if (!parse_expression(newexpression->operands[1]))
				{
					return false;
				}

				if (!newexpression->operands[1]->type.is_scalar())
				{
					error(newexpression->operands[1]->location, 3120, "invalid type for index - index must be a scalar");

					return false;
				}

				if (type.is_array())
				{
					newexpression->type.array_length = 0;
				}
				else if (type.is_matrix())
				{
					newexpression->type.rows = newexpression->type.cols;
					newexpression->type.cols = 1;
				}
				else if (type.is_vector())
				{
					newexpression->type.rows = 1;
				}

				node = fold_constant_expression(_ast, newexpression);
				type = node->type;

				if (!expect(']'))
				{
					return false;
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
	bool parser::parse_expression_multary(expression_node *&left, unsigned int left_precedence)
	{
		if (!parse_expression_unary(left))
		{
			return false;
		}

		enum binary_expression_node::op op;
		unsigned int right_precedence;

		while (peek_multary_op(op, right_precedence))
		{
			if (right_precedence <= left_precedence)
			{
				break;
			}

			consume();

			bool boolean = false;
			expression_node *right1 = nullptr, *right2 = nullptr;

			if (op != binary_expression_node::none)
			{
				if (!parse_expression_multary(right1, right_precedence))
				{
					return false;
				}

				if (op == binary_expression_node::equal || op == binary_expression_node::not_equal)
				{
					boolean = true;

					if (left->type.is_array() || right1->type.is_array() || left->type.definition != right1->type.definition)
					{
						error(right1->location, 3020, "type mismatch");

						return false;
					}
				}
				else if (op == binary_expression_node::bitwise_and || op == binary_expression_node::bitwise_or || op == binary_expression_node::bitwise_xor)
				{
					if (!left->type.is_integral())
					{
						error(left->location, 3082, "int or unsigned int type required");

						return false;
					}
					if (!right1->type.is_integral())
					{
						error(right1->location, 3082, "int or unsigned int type required");

						return false;
					}
				}
				else
				{
					boolean = op == binary_expression_node::logical_and || op == binary_expression_node::logical_or || op == binary_expression_node::less || op == binary_expression_node::greater || op == binary_expression_node::less_equal || op == binary_expression_node::greater_equal;

					if (!left->type.is_scalar() && !left->type.is_vector() && !left->type.is_matrix())
					{
						error(left->location, 3022, "scalar, vector, or matrix expected");

						return false;
					}
					if (!right1->type.is_scalar() && !right1->type.is_vector() && !right1->type.is_matrix())
					{
						error(right1->location, 3022, "scalar, vector, or matrix expected");

						return false;
					}
				}

				const auto newexpression = _ast.make_node<binary_expression_node>(left->location);
				newexpression->op = op;
				newexpression->operands[0] = left;
				newexpression->operands[1] = right1;

				right2 = right1, right1 = left;
				left = newexpression;
			}
			else
			{
				if (!left->type.is_scalar() && !left->type.is_vector())
				{
					error(left->location, 3022, "boolean or vector expression expected");

					return false;
				}

				if (!(parse_expression(right1) && expect(':') && parse_expression_assignment(right2)))
				{
					return false;
				}

				if (right1->type.is_array() || right2->type.is_array() || right1->type.definition != right2->type.definition)
				{
					error(left->location, 3020, "type mismatch between conditional values");

					return false;
				}

				const auto newexpression = _ast.make_node<conditional_expression_node>(left->location);
				newexpression->condition = left;
				newexpression->expression_when_true = right1;
				newexpression->expression_when_false = right2;

				left = newexpression;
			}

			if (boolean)
			{
				left->type.basetype = type_node::datatype_bool;
			}
			else
			{
				left->type.basetype = std::max(right1->type.basetype, right2->type.basetype);
			}

			if ((right1->type.rows == 1 && right2->type.cols == 1) || (right2->type.rows == 1 && right2->type.cols == 1))
			{
				left->type.rows = std::max(right1->type.rows, right2->type.rows);
				left->type.cols = std::max(right1->type.cols, right2->type.cols);
			}
			else
			{
				left->type.rows = std::min(right1->type.rows, right2->type.rows);
				left->type.cols = std::min(right1->type.cols, right2->type.cols);

				if (right1->type.rows > right2->type.rows || right1->type.cols > right2->type.cols)
				{
					warning(right1->location, 3206, "implicit truncation of vector type");
				}
				if (right2->type.rows > right1->type.rows || right2->type.cols > right1->type.cols)
				{
					warning(right2->location, 3206, "implicit truncation of vector type");
				}
			}

			left = fold_constant_expression(_ast, left);
		}

		return true;
	}
	bool parser::parse_expression_assignment(expression_node *&left)
	{
		if (!parse_expression_multary(left))
		{
			return false;
		}

		enum assignment_expression_node::op op;

		if (accept_assignment_op(op))
		{
			expression_node *right = nullptr;

			if (!parse_expression_multary(right))
			{
				return false;
			}

			if (left->type.has_qualifier(type_node::qualifier_const) || left->type.has_qualifier(type_node::qualifier_uniform))
			{
				error(left->location, 3025, "l-value specifies const object");

				return false;
			}

			if (left->type.is_array() || right->type.is_array() || !type_node::rank(left->type, right->type))
			{
				error(right->location, 3020, "cannot convert these types");

				return false;
			}

			if (right->type.rows > left->type.rows || right->type.cols > left->type.cols)
			{
				warning(right->location, 3206, "implicit truncation of vector type");
			}

			const auto assignment = _ast.make_node<assignment_expression_node>(left->location);
			assignment->type = left->type;
			assignment->op = op;
			assignment->left = left;
			assignment->right = right;

			left = assignment;
		}

		return true;
	}

	// Statements
	bool parser::parse_statement(statement_node *&statement, bool scoped)
	{
		std::vector<std::string> attributes;

		// Attributes
		while (accept('['))
		{
			if (expect(lexer::tokenid::identifier))
			{
				const auto attribute = _token.literal_as_string;

				if (expect(']'))
				{
					attributes.push_back(attribute);
				}
			}
			else
			{
				accept(']');
			}
		}

		if (peek('{'))
		{
			if (!parse_statement_block(statement, scoped))
			{
				return false;
			}

			statement->attributes = attributes;

			return true;
		}

		if (accept(';'))
		{
			statement = nullptr;

			return true;
		}

		#pragma region If
		if (accept(lexer::tokenid::if_))
		{
			const auto newstatement = _ast.make_node<if_statement_node>(_token.location);
			newstatement->attributes = attributes;

			if (!(expect('(') && parse_expression(newstatement->condition) && expect(')')))
			{
				return false;
			}

			if (!newstatement->condition->type.is_scalar())
			{
				error(newstatement->condition->location, 3019, "if statement conditional expressions must evaluate to a scalar");

				return false;
			}

			if (!parse_statement(newstatement->statement_when_true))
			{
				return false;
			}

			statement = newstatement;

			if (accept(lexer::tokenid::else_))
			{
				return parse_statement(newstatement->statement_when_false);
			}

			return true;
		}
		#pragma endregion

		#pragma region Switch
		if (accept(lexer::tokenid::switch_))
		{
			const auto newstatement = _ast.make_node<switch_statement_node>(_token.location);
			newstatement->attributes = attributes;

			if (!(expect('(') && parse_expression(newstatement->test_expression) && expect(')')))
			{
				return false;
			}

			if (!newstatement->test_expression->type.is_scalar())
			{
				error(newstatement->test_expression->location, 3019, "switch statement expression must evaluate to a scalar");

				return false;
			}

			if (!expect('{'))
			{
				return false;
			}

			while (!peek('}') && !peek(lexer::tokenid::end_of_file))
			{
				const auto casenode = _ast.make_node<case_statement_node>(_token_next.location);

				while (accept(lexer::tokenid::case_) || accept(lexer::tokenid::default_))
				{
					expression_node *label = nullptr;

					if (_token.id == lexer::tokenid::case_)
					{
						if (!parse_expression(label))
						{
							return false;
						}

						if (label->id != nodeid::literal_expression || !label->type.is_numeric())
						{
							error(label->location, 3020, "non-numeric case expression");

							return false;
						}
					}

					if (!expect(':'))
					{
						return false;
					}

					casenode->labels.push_back(static_cast<literal_expression_node *>(label));
				}

				if (casenode->labels.empty())
				{
					error(casenode->location, 3000, "a case body can only contain a single statement");

					return false;
				}

				if (!parse_statement(casenode->statement_list))
				{
					return false;
				}

				newstatement->case_list.push_back(casenode);
			}

			if (newstatement->case_list.empty())
			{
				warning(newstatement->location, 5002, "switch statement contains no 'case' or 'default' labels");

				statement = nullptr;
			}
			else
			{
				statement = newstatement;
			}

			return expect('}');
		}
		#pragma endregion

		#pragma region For
		if (accept(lexer::tokenid::for_))
		{
			const auto newstatement = _ast.make_node<for_statement_node>(_token.location);
			newstatement->attributes = attributes;

			if (!expect('('))
			{
				return false;
			}

			_symbol_table->enter_scope();

			if (!parse_statement_declarator_list(newstatement->init_statement))
			{
				expression_node *expression = nullptr;

				if (parse_expression(expression))
				{
					const auto initialization = _ast.make_node<expression_statement_node>(expression->location);
					initialization->expression = expression;

					newstatement->init_statement = initialization;
				}
			}

			if (!expect(';'))
			{
				_symbol_table->leave_scope();

				return false;
			}

			parse_expression(newstatement->condition);

			if (!expect(';'))
			{
				_symbol_table->leave_scope();

				return false;
			}

			parse_expression(newstatement->increment_expression);

			if (!expect(')'))
			{
				_symbol_table->leave_scope();

				return false;
			}

			if (!newstatement->condition->type.is_scalar())
			{
				error(newstatement->condition->location, 3019, "scalar value expected");

				return false;
			}

			if (!parse_statement(newstatement->statement_list, false))
			{
				_symbol_table->leave_scope();

				return false;
			}

			_symbol_table->leave_scope();

			statement = newstatement;

			return true;
		}
		#pragma endregion

		#pragma region While
		if (accept(lexer::tokenid::while_))
		{
			const auto newstatement = _ast.make_node<while_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_do_while = false;

			_symbol_table->enter_scope();

			if (!(expect('(') && parse_expression(newstatement->condition) && expect(')')))
			{
				_symbol_table->leave_scope();

				return false;
			}

			if (!newstatement->condition->type.is_scalar())
			{
				error(newstatement->condition->location, 3019, "scalar value expected");

				_symbol_table->leave_scope();

				return false;
			}

			if (!parse_statement(newstatement->statement_list, false))
			{
				_symbol_table->leave_scope();

				return false;
			}

			_symbol_table->leave_scope();

			statement = newstatement;

			return true;
		}
		#pragma endregion

		#pragma region DoWhile
		if (accept(lexer::tokenid::do_))
		{
			const auto newstatement = _ast.make_node<while_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_do_while = true;

			if (!(parse_statement(newstatement->statement_list) && expect(lexer::tokenid::while_) && expect('(') && parse_expression(newstatement->condition) && expect(')') && expect(';')))
			{
				return false;
			}

			if (!newstatement->condition->type.is_scalar())
			{
				error(newstatement->condition->location, 3019, "scalar value expected");

				return false;
			}

			statement = newstatement;

			return true;
		}
		#pragma endregion

		#pragma region Break
		if (accept(lexer::tokenid::break_))
		{
			const auto newstatement = _ast.make_node<jump_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_break = true;

			statement = newstatement;

			return expect(';');
		}
		#pragma endregion

		#pragma region Continue
		if (accept(lexer::tokenid::continue_))
		{
			const auto newstatement = _ast.make_node<jump_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_continue = true;

			statement = newstatement;

			return expect(';');
		}
		#pragma endregion

		#pragma region Return
		if (accept(lexer::tokenid::return_))
		{
			const auto newstatement = _ast.make_node<return_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_discard = false;

			const auto parent = static_cast<const function_declaration_node *>(_symbol_table->current_parent());

			if (!peek(';'))
			{
				if (!parse_expression(newstatement->return_value))
				{
					return false;
				}

				if (parent->return_type.is_void())
				{
					error(newstatement->location, 3079, "void functions cannot return a value");

					accept(';');

					return false;
				}

				if (!type_node::rank(newstatement->return_value->type, parent->return_type))
				{
					error(newstatement->location, 3017, "expression does not match function return type");

					return false;
				}

				if (newstatement->return_value->type.rows > parent->return_type.rows || newstatement->return_value->type.cols > parent->return_type.cols)
				{
					warning(newstatement->location, 3206, "implicit truncation of vector type");
				}
			}
			else if (!parent->return_type.is_void())
			{
				error(newstatement->location, 3080, "function must return a value");

				accept(';');

				return false;
			}

			statement = newstatement;

			return expect(';');
		}
		#pragma endregion

		#pragma region Discard
		if (accept(lexer::tokenid::discard_))
		{
			const auto newstatement = _ast.make_node<return_statement_node>(_token.location);
			newstatement->attributes = attributes;
			newstatement->is_discard = true;

			statement = newstatement;

			return expect(';');
		}
		#pragma endregion

		#pragma region Declaration
		if (parse_statement_declarator_list(statement))
		{
			statement->attributes = attributes;

			return expect(';');
		}
		#pragma endregion

		#pragma region Expression
		expression_node *expression = nullptr;

		if (parse_expression(expression))
		{
			const auto newstatement = _ast.make_node<expression_statement_node>(expression->location);
			newstatement->attributes = attributes;
			newstatement->expression = expression;

			statement = newstatement;

			return expect(';');
		}
		#pragma endregion

		error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "'");

		consume_until(';');

		return false;
	}
	bool parser::parse_statement_block(statement_node *&statement, bool scoped)
	{
		if (!expect('{'))
		{
			return false;
		}

		const auto compound = _ast.make_node<compound_statement_node>(_token.location);

		if (scoped)
		{
			_symbol_table->enter_scope();
		}

		while (!peek('}') && !peek(lexer::tokenid::end_of_file))
		{
			statement_node *compound_statement = nullptr;

			if (!parse_statement(compound_statement))
			{
				if (scoped)
				{
					_symbol_table->leave_scope();
				}

				unsigned level = 0;

				while (!peek(lexer::tokenid::end_of_file))
				{
					if (accept('{'))
					{
						++level;
					}
					else if (accept('}'))
					{
						if (level-- == 0)
						{
							break;
						}
					}
					else
					{
						consume();
					}
				}

				return false;
			}

			compound->statement_list.push_back(compound_statement);
		}

		if (scoped)
		{
			_symbol_table->leave_scope();
		}

		statement = compound;

		return expect('}');
	}
	bool parser::parse_statement_declarator_list(statement_node *&statement)
	{
		type_node type;

		const auto location = _token_next.location;

		if (!parse_type(type))
		{
			return false;
		}

		const auto declarators = _ast.make_node<declarator_list_node>(location);
		unsigned int count = 0;

		do
		{
			if (count++ > 0 && !expect(','))
			{
				return false;
			}

			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			variable_declaration_node *declarator = nullptr;

			if (!parse_variable_declaration(type, _token.literal_as_string, declarator))
			{
				return false;
			}

			declarators->declarator_list.push_back(std::move(declarator));
		}
		while (!peek(';'));

		statement = declarators;

		return true;
	}

	// Declarations
	bool parser::parse_top_level()
	{
		type_node type = { type_node::datatype_void };

		if (peek(lexer::tokenid::namespace_))
		{
			return parse_namespace();
		}
		else if (peek(lexer::tokenid::struct_))
		{
			struct_declaration_node *structure = nullptr;

			if (!parse_struct(structure))
			{
				return false;
			}

			if (!expect(';'))
			{
				return false;
			}
		}
		else if (peek(lexer::tokenid::technique))
		{
			technique_declaration_node *technique = nullptr;

			if (!parse_technique(technique))
			{
				return false;
			}

			_ast.techniques.push_back(std::move(technique));
		}
		else if (parse_type(type))
		{
			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			if (peek('('))
			{
				function_declaration_node *function = nullptr;

				if (!parse_function_declaration(type, _token.literal_as_string, function))
				{
					return false;
				}

				_ast.functions.push_back(std::move(function));
			}
			else
			{
				unsigned int count = 0;

				do
				{
					if (count++ > 0 && !(expect(',') && expect(lexer::tokenid::identifier)))
					{
						return false;
					}

					variable_declaration_node *variable = nullptr;

					if (!parse_variable_declaration(type, _token.literal_as_string, variable, true))
					{
						consume_until(';');

						return false;
					}

					_ast.variables.push_back(std::move(variable));
				}
				while (!peek(';'));

				if (!expect(';'))
				{
					return false;
				}
			}
		}
		else if (!accept(';'))
		{
			consume();

			error(_token.location, 3000, "syntax error: unexpected '" + get_token_name(_token.id) + "'");

			return false;
		}

		return true;
	}
	bool parser::parse_namespace()
	{
		if (!accept(lexer::tokenid::namespace_))
		{
			return false;
		}

		if (!expect(lexer::tokenid::identifier))
		{
			return false;
		}

		const auto name = _token.literal_as_string;

		if (!expect('{'))
		{
			return false;
		}

		_symbol_table->enter_namespace(name);

		bool success = true;

		while (!peek('}'))
		{
			if (!parse_top_level())
			{
				success = false;
				break;
			}
		}

		_symbol_table->leave_namespace();

		return success && expect('}');
	}
	bool parser::parse_array(int &size)
	{
		size = 0;

		if (accept('['))
		{
			expression_node *expression;

			if (accept(']'))
			{
				size = -1;
			}
			else if (parse_expression(expression) && expect(']'))
			{
				if (expression->id != nodeid::literal_expression || !(expression->type.is_scalar() && expression->type.is_integral()))
				{
					error(expression->location, 3058, "array dimensions must be literal scalar expressions");

					return false;
				}

				size = static_cast<literal_expression_node *>(expression)->value_int[0];

				if (size < 1 || size > 65536)
				{
					error(expression->location, 3059, "array dimension must be between 1 and 65536");

					return false;
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	}
	bool parser::parse_annotations(std::unordered_map<std::string, reshade::variant> &annotations)
	{
		if (!accept('<'))
		{
			return true;
		}

		while (!peek('>'))
		{
			type_node type;

			if (accept_type_class(type))
			{
				warning(_token.location, 4717, "type prefixes for annotations are deprecated");
			}

			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			const auto name = _token.literal_as_string;
			literal_expression_node *expression = nullptr;

			if (!(expect('=') && parse_expression_unary(reinterpret_cast<expression_node *&>(expression)) && expect(';')))
			{
				return false;
			}

			if (expression->id != nodeid::literal_expression)
			{
				error(expression->location, 3011, "value must be a literal expression");

				continue;
			}

			switch (expression->type.basetype)
			{
				case type_node::datatype_int:
					annotations[name] = expression->value_int;
					break;
				case type_node::datatype_bool:
				case type_node::datatype_uint:
					annotations[name] = expression->value_uint;
					break;
				case type_node::datatype_float:
					annotations[name] = expression->value_float;
					break;
				case type_node::datatype_string:
					annotations[name] = expression->value_string;
					break;
			}
		}

		return expect('>');
	}
	bool parser::parse_struct(struct_declaration_node *&structure)
	{
		if (!accept(lexer::tokenid::struct_))
		{
			return false;
		}

		structure = _ast.make_node<struct_declaration_node>(_token.location);

		if (accept(lexer::tokenid::identifier))
		{
			structure->name = _token.literal_as_string;

			if (!_symbol_table->insert(structure, true))
			{
				error(_token.location, 3003, "redefinition of '" + structure->name + "'");

				return false;
			}
		}
		else
		{
			structure->name = "__anonymous_struct_" + std::to_string(structure->location.line) + '_' + std::to_string(structure->location.column);
		}

		structure->unique_name = 'S' + _symbol_table->current_scope().name + structure->name;
		std::replace(structure->unique_name.begin(), structure->unique_name.end(), ':', '_');

		if (!expect('{'))
		{
			return false;
		}

		while (!peek('}'))
		{
			type_node type;

			if (!parse_type(type))
			{
				error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected struct member type");

				consume_until('}');

				return false;
			}

			if (type.is_void())
			{
				error(_token_next.location, 3038, "struct members cannot be void");

				consume_until('}');

				return false;
			}
			if (type.has_qualifier(type_node::qualifier_in) || type.has_qualifier(type_node::qualifier_out))
			{
				error(_token_next.location, 3055, "struct members cannot be declared 'in' or 'out'");

				consume_until('}');

				return false;
			}

			unsigned int count = 0;

			do
			{
				if (count++ > 0 && !expect(','))
				{
					consume_until('}');

					return false;
				}

				if (!expect(lexer::tokenid::identifier))
				{
					consume_until('}');

					return false;
				}

				const auto field = _ast.make_node<variable_declaration_node>(_token.location);
				field->unique_name = field->name = _token.literal_as_string;
				field->type = type;

				if (!parse_array(field->type.array_length))
				{
					return false;
				}

				if (accept(':'))
				{
					if (!expect(lexer::tokenid::identifier))
					{
						consume_until('}');

						return false;
					}

					field->semantic = _token.literal_as_string;
					std::transform(field->semantic.begin(), field->semantic.end(), field->semantic.begin(), ::toupper);
				}

				structure->field_list.push_back(std::move(field));
			}
			while (!peek(';'));

			if (!expect(';'))
			{
				consume_until('}');

				return false;
			}
		}

		if (structure->field_list.empty())
		{
			warning(structure->location, 5001, "struct has no members");
		}

		_ast.structs.push_back(structure);

		return expect('}');
	}
	bool parser::parse_function_declaration(type_node &type, std::string name, function_declaration_node *&function)
	{
		const auto location = _token.location;

		if (!expect('('))
		{
			return false;
		}

		if (type.qualifiers != 0)
		{
			error(location, 3047, "function return type cannot have any qualifiers");

			return false;
		}

		function = _ast.make_node<function_declaration_node>(location);
		function->return_type = type;
		function->return_type.qualifiers = type_node::qualifier_const;
		function->name = name;

		function->unique_name = 'F' + _symbol_table->current_scope().name + function->name;
		std::replace(function->unique_name.begin(), function->unique_name.end(), ':', '_');

		_symbol_table->insert(function, true);

		_symbol_table->enter_scope(function);

		while (!peek(')'))
		{
			if (!function->parameter_list.empty() && !expect(','))
			{
				_symbol_table->leave_scope();

				return false;
			}

			const auto parameter = _ast.make_node<variable_declaration_node>(struct location());

			if (!parse_type(parameter->type))
			{
				_symbol_table->leave_scope();

				error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected parameter type");

				return false;
			}

			if (!expect(lexer::tokenid::identifier))
			{
				_symbol_table->leave_scope();

				return false;
			}

			parameter->unique_name = parameter->name = _token.literal_as_string;
			parameter->location = _token.location;

			if (parameter->type.is_void())
			{
				error(parameter->location, 3038, "function parameters cannot be void");

				_symbol_table->leave_scope();

				return false;
			}
			if (parameter->type.has_qualifier(type_node::qualifier_extern))
			{
				error(parameter->location, 3006, "function parameters cannot be declared 'extern'");

				_symbol_table->leave_scope();

				return false;
			}
			if (parameter->type.has_qualifier(type_node::qualifier_static))
			{
				error(parameter->location, 3007, "function parameters cannot be declared 'static'");

				_symbol_table->leave_scope();

				return false;
			}
			if (parameter->type.has_qualifier(type_node::qualifier_uniform))
			{
				error(parameter->location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");

				_symbol_table->leave_scope();

				return false;
			}

			if (parameter->type.has_qualifier(type_node::qualifier_out))
			{
				if (parameter->type.has_qualifier(type_node::qualifier_const))
				{
					error(parameter->location, 3046, "output parameters cannot be declared 'const'");

					_symbol_table->leave_scope();

					return false;
				}
			}
			else
			{
				parameter->type.qualifiers |= type_node::qualifier_in;
			}

			if (!parse_array(parameter->type.array_length))
			{
				return false;
			}

			if (!_symbol_table->insert(parameter))
			{
				error(parameter->location, 3003, "redefinition of '" + parameter->name + "'");

				_symbol_table->leave_scope();

				return false;
			}

			if (accept(':'))
			{
				if (!expect(lexer::tokenid::identifier))
				{
					_symbol_table->leave_scope();

					return false;
				}

				parameter->semantic = _token.literal_as_string;
				std::transform(parameter->semantic.begin(), parameter->semantic.end(), parameter->semantic.begin(), ::toupper);
			}

			function->parameter_list.push_back(parameter);
		}

		if (!expect(')'))
		{
			_symbol_table->leave_scope();

			return false;
		}

		if (accept(':'))
		{
			if (!expect(lexer::tokenid::identifier))
			{
				_symbol_table->leave_scope();

				return false;
			}

			function->return_semantic = _token.literal_as_string;
			std::transform(function->return_semantic.begin(), function->return_semantic.end(), function->return_semantic.begin(), ::toupper);

			if (type.is_void())
			{
				error(_token.location, 3076, "void function cannot have a semantic");

				return false;
			}
		}

		if (!parse_statement_block(reinterpret_cast<statement_node *&>(function->definition), false))
		{
			_symbol_table->leave_scope();

			return false;
		}

		_symbol_table->leave_scope();

		return true;
	}
	bool parser::parse_variable_declaration(type_node &type, std::string name, variable_declaration_node *&variable, bool global)
	{
		auto location = _token.location;

		if (type.is_void())
		{
			error(location, 3038, "variables cannot be void");

			return false;
		}
		if (type.has_qualifier(type_node::qualifier_in) || type.has_qualifier(type_node::qualifier_out))
		{
			error(location, 3055, "variables cannot be declared 'in' or 'out'");

			return false;
		}

		const auto parent = _symbol_table->current_parent();

		if (parent == nullptr)
		{
			if (!type.has_qualifier(type_node::qualifier_static))
			{
				if (!type.has_qualifier(type_node::qualifier_uniform) && !(type.is_texture() || type.is_sampler()))
				{
					warning(location, 5000, "global variables are considered 'uniform' by default");
				}

				if (type.has_qualifier(type_node::qualifier_const))
				{
					error(location, 3035, "variables which are 'uniform' cannot be declared 'const'");

					return false;
				}

				type.qualifiers |= type_node::qualifier_extern | type_node::qualifier_uniform;
			}
		}
		else
		{
			if (type.has_qualifier(type_node::qualifier_extern))
			{
				error(location, 3006, "local variables cannot be declared 'extern'");

				return false;
			}
			if (type.has_qualifier(type_node::qualifier_uniform))
			{
				error(location, 3047, "local variables cannot be declared 'uniform'");

				return false;
			}

			if (type.is_texture() || type.is_sampler())
			{
				error(location, 3038, "local variables cannot be textures or samplers");

				return false;
			}
		}

		if (!parse_array(type.array_length))
		{
			return false;
		}

		variable = _ast.make_node<variable_declaration_node>(location);
		variable->type = type;
		variable->name = name;

		if (global)
		{
			variable->unique_name = (type.has_qualifier(type_node::qualifier_uniform) ? 'U' : 'V') + _symbol_table->current_scope().name + variable->name;
			std::replace(variable->unique_name.begin(), variable->unique_name.end(), ':', '_');
		}
		else
		{
			variable->unique_name = variable->name;
		}

		if (!_symbol_table->insert(variable, global))
		{
			error(location, 3003, "redefinition of '" + name + "'");

			return false;
		}

		if (accept(':'))
		{
			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			variable->semantic = _token.literal_as_string;
			std::transform(variable->semantic.begin(), variable->semantic.end(), variable->semantic.begin(), ::toupper);

			return true;
		}

		if (global && !parse_annotations(variable->annotation_list))
		{
			return false;
		}

		if (accept('='))
		{
			location = _token.location;

			if (!parse_variable_assignment(variable->initializer_expression))
			{
				return false;
			}

			if (parent == nullptr && variable->initializer_expression->id != nodeid::literal_expression)
			{
				error(location, 3011, "initial value must be a literal expression");

				return false;
			}

			if (variable->initializer_expression->id == nodeid::initializer_list && type.is_numeric())
			{
				const auto nullval = _ast.make_node<literal_expression_node>(location);
				nullval->type.basetype = type.basetype;
				nullval->type.qualifiers = type_node::qualifier_const;
				nullval->type.rows = type.rows, nullval->type.cols = type.cols, nullval->type.array_length = 0;

				const auto initializerlist = static_cast<initializer_list_node *>(variable->initializer_expression);

				while (initializerlist->type.array_length < type.array_length)
				{
					initializerlist->type.array_length++;
					initializerlist->values.push_back(nullval);
				}
			}

			if (!type_node::rank(variable->initializer_expression->type, type))
			{
				error(location, 3017, "initial value does not match variable type");

				return false;
			}
			if ((variable->initializer_expression->type.rows < type.rows || variable->initializer_expression->type.cols < type.cols) && !variable->initializer_expression->type.is_scalar())
			{
				error(location, 3017, "cannot implicitly convert these vector types");

				return false;
			}

			if (variable->initializer_expression->type.rows > type.rows || variable->initializer_expression->type.cols > type.cols)
			{
				warning(location, 3206, "implicit truncation of vector type");
			}
		}
		else if (type.is_numeric())
		{
			if (type.has_qualifier(type_node::qualifier_const))
			{
				error(location, 3012, "missing initial value for '" + name + "'");

				return false;
			}
			else if (!type.has_qualifier(type_node::qualifier_uniform) && !type.is_array())
			{
				const auto zero_initializer = _ast.make_node<literal_expression_node>(location);
				zero_initializer->type = type;
				zero_initializer->type.qualifiers = type_node::qualifier_const;

				variable->initializer_expression = zero_initializer;
			}
		}
		else if (peek('{'))
		{
			if (!parse_variable_properties(variable))
			{
				return false;
			}
		}

		if (type.is_sampler() && variable->properties.texture == nullptr)
		{
			error(location, 3012, "missing 'Texture' property for '" + name + "'");

			return false;
		}

		return true;
	}
	bool parser::parse_variable_assignment(expression_node *&expression)
	{
		if (accept('{'))
		{
			const auto initializerlist = _ast.make_node<initializer_list_node>(_token.location);

			while (!peek('}'))
			{
				if (!initializerlist->values.empty() && !expect(','))
				{
					return false;
				}

				if (peek('}'))
				{
					break;
				}

				if (!parse_variable_assignment(expression))
				{
					consume_until('}');

					return false;
				}

				if (expression->id == nodeid::initializer_list && static_cast<initializer_list_node *>(expression)->values.empty())
				{
					continue;
				}

				initializerlist->values.push_back(expression);
			}

			if (!initializerlist->values.empty())
			{
				initializerlist->type = initializerlist->values[0]->type;
				initializerlist->type.array_length = static_cast<int>(initializerlist->values.size());
			}

			expression = initializerlist;

			return expect('}');
		}

		return parse_expression_assignment(expression);
	}
	bool parser::parse_variable_properties(variable_declaration_node *variable)
	{
		if (!expect('{'))
		{
			return false;
		}

		while (!peek('}'))
		{
			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			const auto name = _token.literal_as_string;
			const auto location = _token.location;

			expression_node *value = nullptr;

			if (!(expect('=') && parse_variable_properties_expression(value) && expect(';')))
			{
				return false;
			}

			if (name == "Texture")
			{
				if (value->id != nodeid::lvalue_expression || static_cast<lvalue_expression_node *>(value)->reference->id != nodeid::variable_declaration || !static_cast<lvalue_expression_node *>(value)->reference->type.is_texture() || static_cast<lvalue_expression_node *>(value)->reference->type.is_array())
				{
					error(location, 3020, "type mismatch, expected texture name");

					return false;
				}

				variable->properties.texture = static_cast<lvalue_expression_node *>(value)->reference;
			}
			else
			{
				if (value->id != nodeid::literal_expression)
				{
					error(location, 3011, "value must be a literal expression");

					return false;
				}

				const auto value_literal = static_cast<literal_expression_node *>(value);

				if (name == "Width")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.width);
				}
				else if (name == "Height")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.height);
				}
				else if (name == "Depth")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.depth);
				}
				else if (name == "MipLevels")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.levels);

					if (variable->properties.levels == 0)
					{
						warning(location, 0, "a texture cannot have 0 mipmap levels, changed it to 1");

						variable->properties.levels = 1;
					}
				}
				else if (name == "Format")
				{
					unsigned int format;
					scalar_literal_cast(value_literal, 0, format);
					variable->properties.format = static_cast<reshade::texture_format>(format);
				}
				else if (name == "SRGBTexture" || name == "SRGBReadEnable")
				{
					variable->properties.srgb_texture = value_literal->value_int[0] != 0;
				}
				else if (name == "AddressU")
				{
					unsigned address_mode;
					scalar_literal_cast(value_literal, 0, address_mode);
					variable->properties.address_u = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "AddressV")
				{
					unsigned address_mode;
					scalar_literal_cast(value_literal, 0, address_mode);
					variable->properties.address_v = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "AddressW")
				{
					unsigned address_mode;
					scalar_literal_cast(value_literal, 0, address_mode);
					variable->properties.address_w = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "MinFilter")
				{
					unsigned int a = static_cast<unsigned int>(variable->properties.filter), b;
					scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x0F) | ((b << 4) & 0x30);
					variable->properties.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MagFilter")
				{
					unsigned int a = static_cast<unsigned int>(variable->properties.filter), b;
					scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x33) | ((b << 2) & 0x0C);
					variable->properties.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MipFilter")
				{
					unsigned int a = static_cast<unsigned int>(variable->properties.filter), b;
					scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x3C) | (b & 0x03);
					variable->properties.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MinLOD" || name == "MaxMipLevel")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.min_lod);
				}
				else if (name == "MaxLOD")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.max_lod);
				}
				else if (name == "MipLODBias" || name == "MipMapLodBias")
				{
					scalar_literal_cast(value_literal, 0, variable->properties.lod_bias);
				}
				else
				{
					error(location, 3004, "unrecognized property '" + name + "'");

					return false;
				}
			}
		}

		if (!expect('}'))
		{
			return false;
		}

		return true;
	}
	bool parser::parse_variable_properties_expression(expression_node *&expression)
	{
		backup();

		if (accept(lexer::tokenid::identifier))
		{
			const std::pair<const char *, unsigned int> s_values[] = {
				{ "NONE", 0 },
				{ "POINT", 0 },
				{ "LINEAR", 1 },
				{ "ANISOTROPIC", 3 },
				{ "CLAMP", static_cast<unsigned int>(reshade::texture_address_mode::clamp) },
				{ "WRAP", static_cast<unsigned int>(reshade::texture_address_mode::wrap) },
				{ "REPEAT", static_cast<unsigned int>(reshade::texture_address_mode::wrap) },
				{ "MIRROR", static_cast<unsigned int>(reshade::texture_address_mode::mirror) },
				{ "BORDER", static_cast<unsigned int>(reshade::texture_address_mode::border) },
				{ "R8", static_cast<unsigned int>(reshade::texture_format::r8) },
				{ "R16F", static_cast<unsigned int>(reshade::texture_format::r16f) },
				{ "R32F", static_cast<unsigned int>(reshade::texture_format::r32f) },
				{ "RG8", static_cast<unsigned int>(reshade::texture_format::rg8) },
				{ "R8G8", static_cast<unsigned int>(reshade::texture_format::rg8) },
				{ "RG16", static_cast<unsigned int>(reshade::texture_format::rg16) },
				{ "R16G16", static_cast<unsigned int>(reshade::texture_format::rg16) },
				{ "RG16F", static_cast<unsigned int>(reshade::texture_format::rg16f) },
				{ "R16G16F", static_cast<unsigned int>(reshade::texture_format::rg16f) },
				{ "RG32F", static_cast<unsigned int>(reshade::texture_format::rg32f) },
				{ "R32G32F", static_cast<unsigned int>(reshade::texture_format::rg32f) },
				{ "RGBA8", static_cast<unsigned int>(reshade::texture_format::rgba8) },
				{ "R8G8B8A8", static_cast<unsigned int>(reshade::texture_format::rgba8) },
				{ "RGBA16", static_cast<unsigned int>(reshade::texture_format::rgba16) },
				{ "R16G16B16A16", static_cast<unsigned int>(reshade::texture_format::rgba16) },
				{ "RGBA16F", static_cast<unsigned int>(reshade::texture_format::rgba16f) },
				{ "R16G16B16A16F", static_cast<unsigned int>(reshade::texture_format::rgba16f) },
				{ "RGBA32F", static_cast<unsigned int>(reshade::texture_format::rgba32f) },
				{ "R32G32B32A32F", static_cast<unsigned int>(reshade::texture_format::rgba32f) },
				{ "DXT1", static_cast<unsigned int>(reshade::texture_format::dxt1) },
				{ "DXT3", static_cast<unsigned int>(reshade::texture_format::dxt3) },
				{ "DXT4", static_cast<unsigned int>(reshade::texture_format::dxt5) },
				{ "LATC1", static_cast<unsigned int>(reshade::texture_format::latc1) },
				{ "LATC2", static_cast<unsigned int>(reshade::texture_format::latc2) },
			};

			const auto location = _token.location;
			std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), ::toupper);

			for (const auto &value : s_values)
			{
				if (value.first == _token.literal_as_string)
				{
					const auto newexpression = _ast.make_node<literal_expression_node>(location);
					newexpression->type.basetype = type_node::datatype_uint;
					newexpression->type.rows = newexpression->type.cols = 1, newexpression->type.array_length = 0;
					newexpression->value_uint[0] = value.second;

					expression = newexpression;

					return true;
				}
			}

			restore();
		}

		return parse_expression_multary(expression);
	}
	bool parser::parse_technique(technique_declaration_node *&technique)
	{
		if (!accept(lexer::tokenid::technique))
		{
			return false;
		}

		const auto location = _token.location;

		if (!expect(lexer::tokenid::identifier))
		{
			return false;
		}

		technique = _ast.make_node<technique_declaration_node>(location);
		technique->name = _token.literal_as_string;

		technique->unique_name = 'T' + _symbol_table->current_scope().name + technique->name;
		std::replace(technique->unique_name.begin(), technique->unique_name.end(), ':', '_');

		if (!parse_annotations(technique->annotation_list))
		{
			return false;
		}

		if (!expect('{'))
		{
			return false;
		}

		while (!peek('}'))
		{
			pass_declaration_node *pass = nullptr;

			if (!parse_technique_pass(pass))
			{
				return false;
			}

			technique->pass_list.push_back(std::move(pass));
		}

		return expect('}');
	}
	bool parser::parse_technique_pass(pass_declaration_node *&pass)
	{
		if (!expect(lexer::tokenid::pass))
		{
			return false;
		}

		pass = _ast.make_node<pass_declaration_node>(_token.location);

		if (accept(lexer::tokenid::identifier))
		{
			pass->unique_name = pass->name = _token.literal_as_string;
		}

		if (!expect('{'))
		{
			return false;
		}

		while (!peek('}'))
		{
			if (!expect(lexer::tokenid::identifier))
			{
				return false;
			}

			const auto passstate = _token.literal_as_string;
			const auto location = _token.location;

			expression_node *value = nullptr;

			if (!(expect('=') && parse_technique_pass_expression(value) && expect(';')))
			{
				return false;
			}

			if (passstate == "VertexShader" || passstate == "PixelShader")
			{
				if (value->id != nodeid::lvalue_expression || static_cast<lvalue_expression_node *>(value)->reference->id != nodeid::function_declaration)
				{
					error(location, 3020, "type mismatch, expected function name");

					return false;
				}

				(passstate[0] == 'V' ? pass->vertex_shader : pass->pixel_shader) = reinterpret_cast<const function_declaration_node *>(static_cast<lvalue_expression_node *>(value)->reference);
			}
			else if (passstate.compare(0, 12, "RenderTarget") == 0 && (passstate == "RenderTarget" || (passstate[12] >= '0' && passstate[12] < '8')))
			{
				size_t index = 0;

				if (passstate.size() == 13)
				{
					index = passstate[12] - '0';
				}

				if (value->id != nodeid::lvalue_expression || static_cast<lvalue_expression_node *>(value)->reference->id != nodeid::variable_declaration || static_cast<lvalue_expression_node *>(value)->reference->type.basetype != type_node::datatype_texture || static_cast<lvalue_expression_node *>(value)->reference->type.is_array())
				{
					error(location, 3020, "type mismatch, expected texture name");

					return false;
				}

				pass->render_targets[index] = static_cast<lvalue_expression_node *>(value)->reference;
			}
			else
			{
				if (value->id != nodeid::literal_expression)
				{
					error(location, 3011, "pass state value must be a literal expression");

					return false;
				}

				const auto value_literal = static_cast<literal_expression_node *>(value);

				if (passstate == "SRGBWriteEnable")
				{
					pass->srgb_write_enable = value_literal->value_int[0] != 0;
				}
				else if (passstate == "BlendEnable")
				{
					pass->blend_enable = value_literal->value_int[0] != 0;
				}
				else if (passstate == "StencilEnable")
				{
					pass->stencil_enable = value_literal->value_int[0] != 0;
				}
				else if (passstate == "ClearRenderTargets")
				{
					pass->clear_render_targets = value_literal->value_int[0] != 0;
				}
				else if (passstate == "RenderTargetWriteMask" || passstate == "ColorWriteMask")
				{
					unsigned int mask = 0;
					scalar_literal_cast(value_literal, 0, mask);

					pass->color_write_mask = mask & 0xFF;
				}
				else if (passstate == "StencilReadMask" || passstate == "StencilMask")
				{
					unsigned int mask = 0;
					scalar_literal_cast(value_literal, 0, mask);

					pass->stencil_read_mask = mask & 0xFF;
				}
				else if (passstate == "StencilWriteMask")
				{
					unsigned int mask = 0;
					scalar_literal_cast(value_literal, 0, mask);

					pass->stencil_write_mask = mask & 0xFF;
				}
				else if (passstate == "BlendOp")
				{
					scalar_literal_cast(value_literal, 0, pass->blend_op);
				}
				else if (passstate == "BlendOpAlpha")
				{
					scalar_literal_cast(value_literal, 0, pass->blend_op_alpha);
				}
				else if (passstate == "SrcBlend")
				{
					scalar_literal_cast(value_literal, 0, pass->src_blend);
				}
				else if (passstate == "DestBlend")
				{
					scalar_literal_cast(value_literal, 0, pass->dest_blend);
				}
				else if (passstate == "StencilFunc")
				{
					scalar_literal_cast(value_literal, 0, pass->stencil_comparison_func);
				}
				else if (passstate == "StencilRef")
				{
					scalar_literal_cast(value_literal, 0, pass->stencil_reference_value);
				}
				else if (passstate == "StencilPass" || passstate == "StencilPassOp")
				{
					scalar_literal_cast(value_literal, 0, pass->stencil_op_pass);
				}
				else if (passstate == "StencilFail" || passstate == "StencilFailOp")
				{
					scalar_literal_cast(value_literal, 0, pass->stencil_op_fail);
				}
				else if (passstate == "StencilZFail" || passstate == "StencilDepthFail" || passstate == "StencilDepthFailOp")
				{
					scalar_literal_cast(value_literal, 0, pass->stencil_op_depth_fail);
				}
				else
				{
					error(location, 3004, "unrecognized pass state '" + passstate + "'");

					return false;
				}
			}
		}

		return expect('}');
	}
	bool parser::parse_technique_pass_expression(expression_node *&expression)
	{
		scope scope;
		bool exclusive;

		if (accept(lexer::tokenid::colon_colon))
		{
			scope.namespace_level = scope.level = 0;
			exclusive = true;
		}
		else
		{
			scope = _symbol_table->current_scope();
			exclusive = false;
		}

		if (exclusive ? expect(lexer::tokenid::identifier) : accept(lexer::tokenid::identifier))
		{
			const std::pair<const char *, unsigned int> s_enums[] = {
				{ "NONE", pass_declaration_node::NONE },
				{ "ZERO", pass_declaration_node::ZERO },
				{ "ONE", pass_declaration_node::ONE },
				{ "SRCCOLOR", pass_declaration_node::SRCCOLOR },
				{ "SRCALPHA", pass_declaration_node::SRCALPHA },
				{ "INVSRCCOLOR", pass_declaration_node::INVSRCCOLOR },
				{ "INVSRCALPHA", pass_declaration_node::INVSRCALPHA },
				{ "DESTCOLOR", pass_declaration_node::DESTCOLOR },
				{ "DESTALPHA", pass_declaration_node::DESTALPHA },
				{ "INVDESTCOLOR", pass_declaration_node::INVDESTCOLOR },
				{ "INVDESTALPHA", pass_declaration_node::INVDESTALPHA },
				{ "ADD", pass_declaration_node::ADD },
				{ "SUBTRACT", pass_declaration_node::SUBTRACT },
				{ "REVSUBTRACT", pass_declaration_node::REVSUBTRACT },
				{ "MIN", pass_declaration_node::MIN },
				{ "MAX", pass_declaration_node::MAX },
				{ "KEEP", pass_declaration_node::KEEP },
				{ "REPLACE", pass_declaration_node::REPLACE },
				{ "INVERT", pass_declaration_node::INVERT },
				{ "INCR", pass_declaration_node::INCR },
				{ "INCRSAT", pass_declaration_node::INCRSAT },
				{ "DECR", pass_declaration_node::DECR },
				{ "DECRSAT", pass_declaration_node::DECRSAT },
				{ "NEVER", pass_declaration_node::NEVER },
				{ "ALWAYS", pass_declaration_node::ALWAYS },
				{ "LESS", pass_declaration_node::LESS },
				{ "GREATER", pass_declaration_node::GREATER },
				{ "LEQUAL", pass_declaration_node::LESSEQUAL },
				{ "LESSEQUAL", pass_declaration_node::LESSEQUAL },
				{ "GEQUAL", pass_declaration_node::GREATEREQUAL },
				{ "GREATEREQUAL", pass_declaration_node::GREATEREQUAL },
				{ "EQUAL", pass_declaration_node::EQUAL },
				{ "NEQUAL", pass_declaration_node::NOTEQUAL },
				{ "NOTEQUAL", pass_declaration_node::NOTEQUAL },
			};

			auto identifier = _token.literal_as_string;
			const auto location = _token.location;
			std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), ::toupper);

			for (const auto &value : s_enums)
			{
				if (value.first == _token.literal_as_string)
				{
					const auto newexpression = _ast.make_node<literal_expression_node>(location);
					newexpression->type.basetype = type_node::datatype_uint;
					newexpression->type.rows = newexpression->type.cols = 1, newexpression->type.array_length = 0;
					newexpression->value_uint[0] = value.second;

					expression = newexpression;

					return true;
				}
			}

			while (accept(lexer::tokenid::colon_colon) && expect(lexer::tokenid::identifier))
			{
				identifier += "::" + _token.literal_as_string;
			}

			const auto symbol = _symbol_table->find(identifier, scope, exclusive);

			if (symbol == nullptr)
			{
				error(location, 3004, "undeclared identifier '" + identifier + "'");

				return false;
			}

			const auto newexpression = _ast.make_node<lvalue_expression_node>(location);
			newexpression->reference = static_cast<const variable_declaration_node *>(symbol);
			newexpression->type = symbol->id == nodeid::function_declaration ? static_cast<const function_declaration_node *>(symbol)->return_type : newexpression->reference->type;

			expression = newexpression;

			return true;
		}

		return parse_expression_multary(expression);
	}
}
