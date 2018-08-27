/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_symbol_table.hpp"
#include <algorithm>
#include <assert.h>
#include <fstream>

namespace reshadefx
{
	inline void write(std::ofstream &s, uint32_t word)
	{
		s.write((char *)&word, 4);;
	}
	inline void write(std::ofstream &s, const spv_node &ins)
	{
		// First word of an instruction:
		// The 16 low-order bits are the opcode enumerant
		// The 16 high-order bits are the word count of the instruction
		const uint32_t num_words = 1 + (ins.result_type != 0) + (ins.result != 0) + ins.operands.size();
		write(s, (num_words << 16) | ins.op);

		// Optional instruction type ID
		if (ins.result_type != 0) write(s, ins.result_type);

		// Optional instruction result ID
		if (ins.result != 0) write(s, ins.result);

		// Write out the operands
		for (size_t i = 0; i < ins.operands.size(); ++i)
			write(s, ins.operands[i]);
	}

	const std::string get_token_name(tokenid id)
	{
		switch (id)
		{
			default:
			case tokenid::unknown:
				return "unknown";
			case tokenid::end_of_file:
				return "end of file";
			case tokenid::exclaim:
				return "!";
			case tokenid::hash:
				return "#";
			case tokenid::dollar:
				return "$";
			case tokenid::percent:
				return "%";
			case tokenid::ampersand:
				return "&";
			case tokenid::parenthesis_open:
				return "(";
			case tokenid::parenthesis_close:
				return ")";
			case tokenid::star:
				return "*";
			case tokenid::plus:
				return "+";
			case tokenid::comma:
				return ",";
			case tokenid::minus:
				return "-";
			case tokenid::dot:
				return ".";
			case tokenid::slash:
				return "/";
			case tokenid::colon:
				return ":";
			case tokenid::semicolon:
				return ";";
			case tokenid::less:
				return "<";
			case tokenid::equal:
				return "=";
			case tokenid::greater:
				return ">";
			case tokenid::question:
				return "?";
			case tokenid::at:
				return "@";
			case tokenid::bracket_open:
				return "[";
			case tokenid::backslash:
				return "\\";
			case tokenid::bracket_close:
				return "]";
			case tokenid::caret:
				return "^";
			case tokenid::brace_open:
				return "{";
			case tokenid::pipe:
				return "|";
			case tokenid::brace_close:
				return "}";
			case tokenid::tilde:
				return "~";
			case tokenid::exclaim_equal:
				return "!=";
			case tokenid::percent_equal:
				return "%=";
			case tokenid::ampersand_ampersand:
				return "&&";
			case tokenid::ampersand_equal:
				return "&=";
			case tokenid::star_equal:
				return "*=";
			case tokenid::plus_plus:
				return "++";
			case tokenid::plus_equal:
				return "+=";
			case tokenid::minus_minus:
				return "--";
			case tokenid::minus_equal:
				return "-=";
			case tokenid::arrow:
				return "->";
			case tokenid::ellipsis:
				return "...";
			case tokenid::slash_equal:
				return "|=";
			case tokenid::colon_colon:
				return "::";
			case tokenid::less_less_equal:
				return "<<=";
			case tokenid::less_less:
				return "<<";
			case tokenid::less_equal:
				return "<=";
			case tokenid::equal_equal:
				return "==";
			case tokenid::greater_greater_equal:
				return ">>=";
			case tokenid::greater_greater:
				return ">>";
			case tokenid::greater_equal:
				return ">=";
			case tokenid::caret_equal:
				return "^=";
			case tokenid::pipe_equal:
				return "|=";
			case tokenid::pipe_pipe:
				return "||";
			case tokenid::identifier:
				return "identifier";
			case tokenid::reserved:
				return "reserved word";
			case tokenid::true_literal:
				return "true";
			case tokenid::false_literal:
				return "false";
			case tokenid::int_literal:
			case tokenid::uint_literal:
				return "integral literal";
			case tokenid::float_literal:
			case tokenid::double_literal:
				return "floating point literal";
			case tokenid::string_literal:
				return "string literal";
			case tokenid::namespace_:
				return "namespace";
			case tokenid::struct_:
				return "struct";
			case tokenid::technique:
				return "technique";
			case tokenid::pass:
				return "pass";
			case tokenid::for_:
				return "for";
			case tokenid::while_:
				return "while";
			case tokenid::do_:
				return "do";
			case tokenid::if_:
				return "if";
			case tokenid::else_:
				return "else";
			case tokenid::switch_:
				return "switch";
			case tokenid::case_:
				return "case";
			case tokenid::default_:
				return "default";
			case tokenid::break_:
				return "break";
			case tokenid::continue_:
				return "continue";
			case tokenid::return_:
				return "return";
			case tokenid::discard_:
				return "discard";
			case tokenid::extern_:
				return "extern";
			case tokenid::static_:
				return "static";
			case tokenid::uniform_:
				return "uniform";
			case tokenid::volatile_:
				return "volatile";
			case tokenid::precise:
				return "precise";
			case tokenid::in:
				return "in";
			case tokenid::out:
				return "out";
			case tokenid::inout:
				return "inout";
			case tokenid::const_:
				return "const";
			case tokenid::linear:
				return "linear";
			case tokenid::noperspective:
				return "noperspective";
			case tokenid::centroid:
				return "centroid";
			case tokenid::nointerpolation:
				return "nointerpolation";
			case tokenid::void_:
				return "void";
			case tokenid::bool_:
			case tokenid::bool2:
			case tokenid::bool3:
			case tokenid::bool4:
			case tokenid::bool2x2:
			case tokenid::bool3x3:
			case tokenid::bool4x4:
				return "bool type";
			case tokenid::int_:
			case tokenid::int2:
			case tokenid::int3:
			case tokenid::int4:
			case tokenid::int2x2:
			case tokenid::int3x3:
			case tokenid::int4x4:
				return "int type";
			case tokenid::uint_:
			case tokenid::uint2:
			case tokenid::uint3:
			case tokenid::uint4:
			case tokenid::uint2x2:
			case tokenid::uint3x3:
			case tokenid::uint4x4:
				return "uint type";
			case tokenid::float_:
			case tokenid::float2:
			case tokenid::float3:
			case tokenid::float4:
			case tokenid::float2x2:
			case tokenid::float3x3:
			case tokenid::float4x4:
				return "float type";
			case tokenid::vector:
				return "vector";
			case tokenid::matrix:
				return "matrix";
			case tokenid::string_:
				return "string";
			case tokenid::texture:
				return "texture";
			case tokenid::sampler:
				return "sampler";
		}
	}

	parser::parser() :
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

		while (!peek(tokenid::end_of_file))
		{
			if (!parse_top_level())
			{
				return false;
			}
		}

		std::ofstream s("test.spv", std::ios::binary | std::ios::out);
		// Write SPIRV header info
		write(s, spv::MagicNumber);
		write(s, spv::Version);
		write(s, 0u); // Generator magic number, see https://www.khronos.org/registry/spir-v/api/spir-v.xml
		write(s, 1000u); // Maximum ID
		write(s, 0u); // Reserved for instruction schema

		// All capabilities
		
		write(s, spv_node(spv::OpCapability)
			.add(spv::CapabilityShader));

		// Optional extension instructions
		const uint32_t glsl_ext = 1;

		write(s, spv_node(spv::OpExtInstImport, glsl_ext)
			.add_string("GLSL.std.450")); // Import GLSL extension

		// Single required memory model instruction
		write(s, spv_node(spv::OpMemoryModel)
			.add(spv::AddressingModelLogical)
			.add(spv::MemoryModelGLSL450));

		// All entry point declarations
		for (const auto &node : _entries.instructions)
			write(s, node);

		// All execution mode declarations
		//_out_stream << spv_node(spv::OpExecutionMode, 0 /* function id */,

		// All debug instructions
		const char *filename = "test.fx";
		const uint32_t filename_id = 2;

		write(s, spv_node(spv::OpString, filename_id)
			.add_string(filename));
		write(s, spv_node(spv::OpSource)
			.add(spv::SourceLanguageUnknown)
			.add(0) // Version
			.add(filename_id)); // Filename string ID

		for (const auto &node : _debug.instructions)
			write(s, node);

		// All annotation instructions
		for (const auto &node : _annotations.instructions)
			write(s, node);

		// All type declarations
		for (const auto &node : _variables.instructions)
			write(s, node);

		// All function definitions
		for (const auto &function : _functions)
		{
			for (const auto &node : function->variables.instructions)
				write(s, node);
			for (const auto &node : function->code.instructions)
				write(s, node);
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

	bool parser::peek(tokenid tokid) const
	{
		return _token_next.id == tokid;
	}
	void parser::consume()
	{
		_token = _token_next;
		_token_next = _lexer->lex();
	}
	void parser::consume_until(tokenid tokid)
	{
		while (!accept(tokid) && !peek(tokenid::end_of_file))
		{
			consume();
		}
	}
	bool parser::accept(tokenid tokid)
	{
		if (peek(tokid))
		{
			consume();

			return true;
		}

		return false;
	}
	bool parser::expect(tokenid tokid)
	{
		if (!accept(tokid))
		{
			error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected '" + get_token_name(tokid) + "'");

			return false;
		}

		return true;
	}

	// Types
	bool parser::accept_type_class(type_info &type)
	{
		type.size = type.rows = type.cols = 0;

		if (peek(tokenid::identifier))
		{
			type.base = spv::OpTypeStruct;

			const symbol symbol = _symbol_table->find(_token_next.literal_as_string);

			if (symbol.id && symbol.op == spv::OpTypeStruct)
			{
				type.definition = symbol.id;

				consume();

				return true;
			}

			return false;
		}
		else if (accept(tokenid::vector))
		{
			type.base = spv::OpTypeFloat, type.size = 32;
			type.rows = 4, type.cols = 1;
			type.is_signed = true;;

			if (accept('<'))
			{
				if (!accept_type_class(type))
					return error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected vector element type"), false;

				if (!type.is_scalar())
					return error(_token.location, 3122, "vector element type must be a scalar type"), false;

				if (!(expect(',') && expect(tokenid::int_literal)))
					return false;

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					return error(_token.location, 3052, "vector dimension must be between 1 and 4"), false;

				type.rows = _token.literal_as_int;

				if (!expect('>'))
					return false;
			}

			return true;
		}
		else if (accept(tokenid::matrix))
		{
			type.base = spv::OpTypeFloat, type.size = 32;
			type.rows = 4, type.cols = 4;
			type.is_signed = true;;

			if (accept('<'))
			{
				if (!accept_type_class(type))
					return error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected matrix element type"), false;

				if (!type.is_scalar())
					return error(_token.location, 3123, "matrix element type must be a scalar type"), false;

				if (!(expect(',') && expect(tokenid::int_literal)))
					return false;

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

				type.rows = _token.literal_as_int;

				if (!(expect(',') && expect(tokenid::int_literal)))
					return false;

				if (_token.literal_as_int < 1 || _token.literal_as_int > 4)
					return error(_token.location, 3053, "matrix dimensions must be between 1 and 4"), false;

				type.cols = _token.literal_as_int;

				if (!expect('>'))
					return false;
			}

			return true;
		}

		switch (_token_next.id)
		{
		case tokenid::void_:
			type.base = spv::OpTypeVoid;
			break;
		case tokenid::bool_:
		case tokenid::bool2:
		case tokenid::bool3:
		case tokenid::bool4:
			type.base = spv::OpTypeBool, type.size = 32;
			type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::bool_);
			type.cols = 1;
			break;
		case tokenid::bool2x2:
		case tokenid::bool3x3:
		case tokenid::bool4x4:
			type.base = spv::OpTypeBool, type.size = 32;
			type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::bool2x2);
			type.cols = type.rows;
			break;
		case tokenid::int_:
		case tokenid::int2:
		case tokenid::int3:
		case tokenid::int4:
			type.base = spv::OpTypeInt, type.size = 32;
			type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::int_);
			type.cols = 1;
			type.is_signed = true;
			break;
		case tokenid::int2x2:
		case tokenid::int3x3:
		case tokenid::int4x4:
			type.base = spv::OpTypeInt, type.size = 32;
			type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::int2x2);
			type.cols = type.rows;
			type.is_signed = true;
			break;
		case tokenid::uint_:
		case tokenid::uint2:
		case tokenid::uint3:
		case tokenid::uint4:
			type.base = spv::OpTypeInt, type.size = 32;
			type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::uint_);
			type.cols = 1;
			type.is_signed = false;
			break;
		case tokenid::uint2x2:
		case tokenid::uint3x3:
		case tokenid::uint4x4:
			type.base = spv::OpTypeInt, type.size = 32;
			type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::uint2x2);
			type.cols = type.rows;
			type.is_signed = false;
			break;
		case tokenid::float_:
		case tokenid::float2:
		case tokenid::float3:
		case tokenid::float4:
			type.base = spv::OpTypeFloat, type.size = 32;
			type.rows = 1 + uint32_t(_token_next.id) - uint32_t(tokenid::float_);
			type.cols = 1;
			type.is_signed = true;
			break;
		case tokenid::float2x2:
		case tokenid::float3x3:
		case tokenid::float4x4:
			type.base = spv::OpTypeFloat, type.size = 32;
			type.rows = 2 + uint32_t(_token_next.id) - uint32_t(tokenid::float2x2);
			type.cols = type.rows;
			type.is_signed = true;
			break;
		case tokenid::string_:
			type.base = spv::OpString;
			break;
		case tokenid::texture:
			type.base = spv::OpTypeImage;
			break;
		case tokenid::sampler:
			type.base = spv::OpTypeSampledImage;
			break;
		default:
			return false;
		}

		consume();

		return true;
	}
	bool parser::accept_type_qualifiers(type_info &type)
	{
		unsigned int qualifiers = 0;

		// Storage
		if (accept(tokenid::extern_))
			qualifiers |= qualifier_extern;
		if (accept(tokenid::static_))
			qualifiers |= qualifier_static;
		if (accept(tokenid::uniform_))
			qualifiers |= qualifier_uniform;
		if (accept(tokenid::volatile_))
			qualifiers |= qualifier_volatile;
		if (accept(tokenid::precise))
			qualifiers |= qualifier_precise;

		if (accept(tokenid::in))
			qualifiers |= qualifier_in;
		if (accept(tokenid::out))
			qualifiers |= qualifier_out;
		if (accept(tokenid::inout))
			qualifiers |= qualifier_inout;

		// Modifiers
		if (accept(tokenid::const_))
			qualifiers |= qualifier_const;

		// Interpolation
		if (accept(tokenid::linear))
			qualifiers |= qualifier_linear;
		if (accept(tokenid::noperspective))
			qualifiers |= qualifier_noperspective;
		if (accept(tokenid::centroid))
			qualifiers |= qualifier_centroid;
		if (accept(tokenid::nointerpolation))
			qualifiers |= qualifier_nointerpolation;

		if (qualifiers == 0)
			return false;
		if ((type.qualifiers & qualifiers) == qualifiers)
			warning(_token.location, 3048, "duplicate usages specified");

		type.qualifiers |= qualifiers;

		// Continue parsing potential additional qualifiers until no more are found
		accept_type_qualifiers(type);

		return true;
	}

	bool parser::parse_type(type_info &type)
	{
		type.qualifiers = 0;

		accept_type_qualifiers(type);

		const auto location = _token_next.location;

		if (!accept_type_class(type))
			return false;

		if (type.is_integral() && (type.has(qualifier_centroid) || type.has(qualifier_noperspective)))
			return error(location, 4576, "signature specifies invalid interpolation mode for integer component type"), false;

		if (type.has(qualifier_centroid) && !type.has(qualifier_noperspective))
			type.qualifiers |= qualifier_linear;

		return true;
	}

	// Expressions
	bool parser::accept_unary_op(spv::Op &op)
	{
		switch (_token_next.id)
		{
		case tokenid::exclaim: op = spv::OpLogicalNot; break;
		case tokenid::plus: op = spv::OpNop; break;
		case tokenid::minus: op = spv::OpFNegate; break;
		case tokenid::tilde: op = spv::OpNot; break;
		case tokenid::plus_plus: op = spv::OpFAdd; break;
		case tokenid::minus_minus: op = spv::OpFSub; break;
		default:
			return false;
		}

		consume();

		return true;
	}
	bool parser::accept_postfix_op(const type_info &type, spv::Op &op)
	{
		switch (_token_next.id)
		{
		case tokenid::plus_plus:
			op = type.is_integral() ? spv::OpIAdd : spv::OpFAdd; break;
		case tokenid::minus_minus:
			op = type.is_integral() ? spv::OpISub : spv::OpFSub; break;
		default:
			return false;
		}

		consume();

		return true;
	}
	bool parser::peek_multary_op(spv::Op &op, unsigned int &precedence) const
	{
		// Precedence values taken from https://cppreference.com/w/cpp/language/operator_precedence
		switch (_token_next.id)
		{
		case tokenid::percent: op = spv::OpFMod; precedence = 11; break;
		case tokenid::ampersand: op = spv::OpBitwiseAnd; precedence = 6; break;
		case tokenid::star: op = spv::OpFMul; precedence = 11; break;
		case tokenid::plus: op = spv::OpFAdd; precedence = 10; break;
		case tokenid::minus: op = spv::OpFSub; precedence = 10; break;
		case tokenid::slash: op = spv::OpFDiv; precedence = 11; break;
		case tokenid::less: op = spv::OpFOrdLessThan; precedence = 8; break;
		case tokenid::greater: op = spv::OpFOrdGreaterThan; precedence = 8; break;
		case tokenid::question: op = spv::OpSelect; precedence = 1; break;
		case tokenid::caret: op = spv::OpBitwiseXor; precedence = 5; break;
		case tokenid::pipe: op = spv::OpBitwiseOr; precedence = 4; break;
		case tokenid::exclaim_equal: op = spv::OpLogicalNotEqual; precedence = 7; break;
		case tokenid::ampersand_ampersand: op = spv::OpLogicalAnd; precedence = 3; break;
		case tokenid::less_less: op = spv::OpShiftLeftLogical; precedence = 9; break;
		case tokenid::less_equal: op = spv::OpFOrdLessThanEqual; precedence = 8; break;
		case tokenid::equal_equal: op = spv::OpLogicalEqual; precedence = 7; break;
		case tokenid::greater_greater: op = spv::OpShiftRightLogical; precedence = 9; break;
		case tokenid::greater_equal: op = spv::OpFOrdGreaterThanEqual; precedence = 8; break;
		case tokenid::pipe_pipe: op = spv::OpLogicalOr; precedence = 2; break;
		default:
			return false;
		}

		// Do not consume token yet since the expression may be skipped due to precedence
		return true;
	}
	bool parser::accept_assignment_op(const type_info &type, spv::Op &op)
	{
		switch (_token_next.id)
		{
		case tokenid::equal:
			op = spv::OpNop; break; // Assignment without an additional operation
		case tokenid::percent_equal:
			op = type.is_integral() ? type.is_signed ? spv::OpSMod : spv::OpUMod : spv::OpFMod; break;
		case tokenid::ampersand_equal:
			op = spv::OpBitwiseAnd; break;
		case tokenid::star_equal:
			op = type.is_integral() ? spv::OpIMul : spv::OpFMul; break;
		case tokenid::plus_equal:
			op = type.is_integral() ? spv::OpIAdd : spv::OpFAdd; break;
		case tokenid::minus_equal:
			op = type.is_integral() ? spv::OpISub : spv::OpFSub; break;
		case tokenid::slash_equal:
			op = type.is_integral() ? type.is_signed ? spv::OpSDiv : spv::OpUDiv : spv::OpFDiv; break;
		case tokenid::less_less_equal:
			op = spv::OpShiftLeftLogical; break;
		case tokenid::greater_greater_equal:
			op = spv::OpShiftRightLogical; break;
		case tokenid::caret_equal:
			op = spv::OpBitwiseXor; break;
		case tokenid::pipe_equal:
			op = spv::OpBitwiseOr; break;
		default:
			return false;
		}

		consume();

		return true;
	}

	bool parser::parse_expression(spv_section &section, access_chain &exp)
	{
		// Parse first expression
		if (!parse_expression_assignment(section, exp))
			return false;

		// Continue parsing if an expression sequence is next
		while (accept(','))
			// Overwrite since the last expression in the sequence is the result
			if (!parse_expression_assignment(section, exp))
				return false;

		return true;
	}
	bool parser::parse_expression_unary(spv_section &section, access_chain &exp)
	{
		auto location = _token_next.location;

		#pragma region Prefix
		// Check if a prefix operator exists
		spv::Op op;
		if (accept_unary_op(op))
		{
			// Parse the actual expression
			if (!parse_expression_unary(section, exp))
				return false;

			// Unary operators are only valid on basic types
			if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
				return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;

			// Ignore "+" operator since it does not actually do anything
			if (op != spv::OpNop)
			{
				// The "~" bitwise operator is only valid on integral types
				if (op == spv::OpNot && !exp.type.is_integral())
					return error(exp.location, 3082, "int or unsigned int type required"), false;

				if (exp.type.is_integral())
				{
					switch (op)
					{
					case spv::OpFNegate:
						op = exp.type.is_signed ? spv::OpSNegate : spv::OpBitReverse;
						break;
					case spv::OpFAdd:
						op = spv::OpIAdd;
						break;
					case spv::OpFSub:
						op = spv::OpISub;
						break;
					}
				}

				// Load the right hand side value if it was not yet resolved at this point
				const spv::Id value = access_chain_load(section, exp);

				// Special handling for the "++" and "--" operators
				if (op == spv::OpFAdd || op == spv::OpFSub ||
					op == spv::OpIAdd || op == spv::OpISub)
				{
					if (exp.type.has(qualifier_const) || exp.type.has(qualifier_uniform) || !exp.is_lvalue)
						return error(location, 3025, "l-value specifies const object"), false;

					// Create a constant one in the type of the expression
					constant one = { exp.type.is_floating_point() ? 0x3f800000u : 1u };
					const spv::Id constant = convert_constant(exp.type, one);

					spv::Id result = add_node(section, location, op, convert_type(exp.type))
						.add(value) // Operand 1
						.add(constant) // Operand 2
						.result; // Result ID

					// The "++" and "--" operands modify the source variable, so store result back into it
					access_chain_store(section, exp, result, exp.type);
				}
				else // All other prefix operators return a new r-value
				{
					spv::Id result = add_node(section, location, op, convert_type(exp.type))
						.add(value) // Operand
						.result; // Result ID

					exp.reset_to_rvalue(result, exp.type, location);
				}
			}
		}
		else if (accept('('))
		{
			backup();

			// Check if this is a C-style cast expression
			if (type_info cast_type; accept_type_class(cast_type))
			{
				if (peek('('))
				{
					// This is not a C-style cast but a constructor call, so need to roll-back and parse that instead
					restore();
				}
				else if (expect(')'))
				{
					// Parse expression behind cast operator
					if (!parse_expression_unary(section, exp))
						return false;

					// Check if the types already match, in which case there is nothing to do
					if (exp.type.base == cast_type.base && (exp.type.rows == cast_type.rows && exp.type.cols == cast_type.cols) && !(exp.type.is_array() || cast_type.is_array()))
						return true;

					// Can only cast between numeric types
					if (exp.type.is_numeric() && cast_type.is_numeric())
					{
						if (exp.type.components() < cast_type.components() && !exp.type.is_scalar())
							return error(location, 3017, "cannot convert these vector types"), false;

						exp.push_cast(cast_type, false);
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
					// Type name was not followed by a closing parenthesis
					return false;
				}
			}

			// Parse expression between the parentheses
			if (!parse_expression(section, exp) || !expect(')'))
				return false;
		}
		else if (accept(tokenid::true_literal))
		{
			const type_info literal_type = { spv::OpTypeBool, 32, 1, 1, false, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, true);
		}
		else if (accept(tokenid::false_literal))
		{
			const type_info literal_type = { spv::OpTypeBool, 32, 1, 1, false, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, false);
		}
		else if (accept(tokenid::int_literal))
		{
			const type_info literal_type = { spv::OpTypeInt, 32, 1, 1, true, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, _token.literal_as_int);
		}
		else if (accept(tokenid::uint_literal))
		{
			const type_info literal_type = { spv::OpTypeInt, 32, 1, 1, false, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, _token.literal_as_uint);
		}
		else if (accept(tokenid::float_literal))
		{
			const type_info literal_type = { spv::OpTypeFloat, 32, 1, 1, true, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, *reinterpret_cast<const uint32_t *>(&_token.literal_as_float)); // Interpret float bit pattern as int
		}
		else if (accept(tokenid::double_literal))
		{
			// Convert double literal to float literal for now
			const float value = static_cast<float>(_token.literal_as_double);

			const type_info literal_type = { spv::OpTypeFloat, 32, 1, 1, true, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, *reinterpret_cast<const uint32_t *>(&value)); // Interpret float bit pattern as int
		}
		else if (accept(tokenid::string_literal))
		{
			std::string value = _token.literal_as_string;

			// Multiple string literals in sequence are concatenated into a single string literal
			while (accept(tokenid::string_literal))
				value += _token.literal_as_string;

			const type_info literal_type = { spv::OpString, 0, 0, 0, false, false, qualifier_const };
			exp.reset_to_rvalue_constant(literal_type, location, std::move(value));
		}
		else if (type_info type; accept_type_class(type)) // Check if this is a constructor call expression
		{
			if (!expect('('))
				return false;

			if (!type.is_numeric())
				return error(location, 3037, "constructors only defined for numeric base types"), false;

			// Empty constructors do not exist
			if (accept(')'))
				return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

			// Parse entire argument expression list
			bool constant = true;
			unsigned int num_components = 0;
			std::vector<access_chain> arguments;

			while (!peek(')'))
			{
				// There should be a comma between arguments
				if (!arguments.empty() && !expect(','))
					return false;

				// Parse the argument expression
				if (!parse_expression_assignment(section, arguments.emplace_back()))
					return false;

				access_chain &argument = arguments.back();

				// Constructors are only defined for numeric base types
				if (!argument.type.is_numeric())
					return error(argument.location, 3017, "cannot convert non-numeric types"), false;

				constant &= argument.is_constant; // Result is only constant if all arguments are constant
				num_components += argument.type.components();
			}

			// The list should be terminated with a parenthesis
			if (!expect(')'))
				return false;

			// The total number of argument elements needs to match the number of elements in the result type
			if (num_components != type.components())
				return error(location, 3014, "incorrect number of arguments to numeric-type constructor"), false;

			assert(num_components > 0 && num_components <= 16);

			if (constant) // Constants can be converted at compile time
			{
				struct constant data = {};

				unsigned int i = 0;
				for (auto &argument : arguments)
				{
					type_info target_type = argument.type;
					target_type.base = type.base;
					argument.push_cast(target_type, true);
					for (unsigned int k = 0; k < argument.type.components(); ++k)
						data.as_uint[i++] = argument.constant.as_uint[k];
				}

				exp.reset_to_rvalue_constant(type, location, data);
			}
			else if (arguments.size() > 1)
			{
				// There must be exactly one constituent for each top-level component of the result
				if (type.is_matrix())
				{
					std::vector<spv::Id> ids;
					ids.reserve(num_components);

					// First, extract all arguments so that a list of scalars exist
					for (auto &argument : arguments)
					{
						for (unsigned int index = 0; index < argument.type.components(); ++index)
						{
							access_chain scalar = argument;
							scalar.push_static_index(index, *this);
							ids.push_back(access_chain_load(section, scalar));
						}
					}

					// Second, turn that list of scalars into a list of column vectors
					// TODO
					assert(false);

					// Finally, construct a matrix from those column vectors
					spv_node &node = add_node(section, location, spv::OpCompositeConstruct, convert_type(type));

					for (size_t i = 0; i < ids.size(); ++i)
						node.add(ids[i]);

					exp.reset_to_rvalue(node.result, type, location);
				}
				// The exception is that for constructing a vector, a contiguous subset of the scalars consumed can be represented by a vector operand instead
				else
				{
					assert(type.is_vector());

					std::vector<spv::Id> ids;
					for (auto &argument : arguments)
					{
						type_info target_type = argument.type;
						target_type.base = type.base;
						argument.push_cast(target_type, true);
						assert(argument.type.is_scalar() || argument.type.is_vector());
						ids.push_back(access_chain_load(section, argument));
					}

					spv_node &node = add_node(section, location, spv::OpCompositeConstruct, convert_type(type));

					for (size_t i = 0; i < ids.size(); ++i)
						node.add(ids[i]);

					exp.reset_to_rvalue(node.result, type, location);
				}
			}
			else // A constructor call with a single argument is identical to a cast
			{
				assert(!arguments.empty());

				// Reset expression to only argument
				exp = std::move(arguments[0]);

				// Add cast to expression access chain
				exp.push_cast(type, false);
			}
		}
		else // At this point only identifiers are left to check and resolve
		{
			scope scope;
			bool exclusive;
			std::string identifier;

			// Starting an identifier with '::' restricts the symbol search to the global namespace level
			if (accept(tokenid::colon_colon))
			{
				scope.name = "::";
				scope.namespace_level = scope.level = 0;
				exclusive = true; // Do not search in other namespace levels
			}
			else
			{
				scope = _symbol_table->current_scope();
				exclusive = false;
			}

			if (exclusive ? expect(tokenid::identifier) : accept(tokenid::identifier))
				identifier = _token.literal_as_string;
			else
				return false;

			// Can concatenate multiple '::' to force symbol search for a specific namespace level
			while (accept(tokenid::colon_colon))
			{
				if (!expect(tokenid::identifier))
					return false;
				identifier += "::" + _token.literal_as_string;
			}

			// Lookup name in the symbol table
			symbol symbol = _symbol_table->find(identifier, scope, exclusive);

			// Check if this is a function call or variable reference
			if (accept('('))
			{
				// Can only call symbols that are functions, but do not abort yet if no symbol was found since the identifier may reference an intrinsic
				if (symbol.id && symbol.op != spv::OpFunction)
					return error(location, 3005, "identifier '" + identifier + "' represents a variable, not a function"), false;

				// Parse entire argument expression list
				std::vector<access_chain> arguments;
				std::vector<spv::Id> argument_ids;
				std::vector<type_info> argument_types;

				while (!peek(')'))
				{
					// There should be a comma between arguments
					if (!arguments.empty() && !expect(','))
						return false;

					// Parse the argument expression
					if (!parse_expression_assignment(section, arguments.emplace_back()))
						return false;

					argument_types.push_back(arguments.back().type);
				}

				// The list should be terminated with a parenthesis
				if (!expect(')'))
					return false;

				// Try to resolve the call by searching through both function symbols and intrinsics
				bool undeclared = !symbol.id, ambiguous = false;

				if (!_symbol_table->resolve_call(identifier, argument_types, scope, ambiguous, symbol))
				{
					if (undeclared && symbol.op == spv::OpFunctionCall)
						error(location, 3004, "undeclared identifier '" + identifier + "'");
					else if (ambiguous)
						error(location, 3067, "ambiguous function call to '" + identifier + "'");
					else
						error(location, 3013, "no matching function overload for '" + identifier + "'");
					return false;
				}

				// Load values and perform implicit type conversions for all arguments
				argument_ids.resize(arguments.size());
				for (size_t i = 0; i < arguments.size(); ++i)
				{
					arguments[i].push_cast(static_cast<const function_info *>(symbol.info)->parameter_list[i], true);

					argument_ids[i] = access_chain_load(section, arguments[i]);
				}

				// Check if the call resolving found an intrinsic or function
				if (symbol.op != spv::OpFunctionCall)
				{
					// This is an intrinsic, so add the appropriate operators
					spv_node &node = add_node(section, location, symbol.op, convert_type(symbol.type));

					if (symbol.op == spv::OpExtInst)
					{
						node.add(1) // GLSL extended instruction set
							.add(symbol.id);
					}

					// Some operators need special handling because the arguments from the intrinsic definitions do not match those of the SPIR-V operators
					if (symbol.op == spv::OpImageSampleImplicitLod)
					{
						assert(arguments.size() == 2);

						node.add(argument_ids[0]) // Sampled Image
							.add(argument_ids[1]) // Coordinate
							.add(spv::ImageOperandsMaskNone); // Image Operands
					}
					else if (symbol.op == spv::OpImageSampleExplicitLod)
					{
						assert(arguments.size() == 2);

						node.add(argument_ids[0]) // Sampled Image
							.add(argument_ids[1]) // Coordinate
							.add(spv::ImageOperandsMaskNone); // Image Operands
					}
					else
					{
						for (size_t i = 0; i < arguments.size(); ++i)
							node.add(argument_ids[i]);
					}

					exp.reset_to_rvalue(node.result, symbol.type, location);
				}
				else
				{
					// It is not allowed to do recursive calls
					if (_symbol_table->current_parent() == symbol.info)
						return error(location, 3500, "recursive function calls are not allowed"), false;

					// This is a function symbol, so add a call to it
					spv_node &node = add_node(section, location, spv::OpFunctionCall, convert_type(symbol.type));

					node.add(symbol.id); // Function

					for (size_t i = 0; i < arguments.size(); ++i)
						node.add(argument_ids[i]); // Arguments

					exp.reset_to_rvalue(node.result, symbol.type, location);
				}
			}
			else
			{
				// Show error if no symbol matching the identifier was found
				if (!symbol.id)
					return error(location, 3004, "undeclared identifier '" + identifier + "'"), false;
				// Can only reference variables by name, functions need to be called
				else if (symbol.op != spv::OpVariable)
					return error(location, 3005, "identifier '" + identifier + "' represents a function, not a variable"), false;

				// Simply return the pointer to the variable, dereferencing is done on site where necessary
				exp.reset_to_lvalue(symbol.id, symbol.type, location);
			}
		}
		#pragma endregion

		#pragma region Postfix
		while (!peek(tokenid::end_of_file))
		{
			location = _token_next.location;

			// Check if a postfix operator exists
			if (accept_postfix_op(exp.type, op))
			{
				// Unary operators are only valid on basic types
				if (!exp.type.is_scalar() && !exp.type.is_vector() && !exp.type.is_matrix())
					return error(exp.location, 3022, "scalar, vector, or matrix expected"), false;
				else if (exp.type.has(qualifier_const) || exp.type.has(qualifier_uniform) || !exp.is_lvalue)
					return error(exp.location, 3025, "l-value specifies const object"), false;

				// Load current value from expression
				const spv::Id value = access_chain_load(section, exp);

				// Create a constant one in the type of the expression
				constant one = { exp.type.is_floating_point() ? 0x3f800000u : 1u };
				const spv::Id constant = convert_constant(exp.type, one);

				spv::Id result = add_node(section, location, op, convert_type(exp.type))
					.add(value) // Operand 1
					.add(constant) // Operand 2
					.result; // Result ID

				// The "++" and "--" operands modify the source variable, so store result back into it
				access_chain_store(section, exp, result, exp.type);

				// All postfix operators return a r-value
				exp.reset_to_rvalue(result, exp.type, location);
			}
			else if (accept('.'))
			{
				if (!expect(tokenid::identifier))
					return false;

				location = _token.location;
				const auto subscript = _token.literal_as_string;

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
								return error(location, 3018, "invalid subscript '" + subscript + "'"), false;
						}

						if (i > 0 && (set[i] != set[i - 1]))
							return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;
						if (static_cast<unsigned int>(offsets[i]) >= exp.type.rows)
							return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

						for (size_t k = 0; k < i; ++k)
						{
							if (offsets[k] == offsets[i])
							{
								constant = true;
								break;
							}
						}
					}

					// Add swizzle to current access chain
					exp.push_swizzle(offsets, length);

					if (constant || exp.type.has(qualifier_uniform))
						exp.type.qualifiers = (exp.type.qualifiers | qualifier_const) & ~qualifier_uniform;
				}
				else if (exp.type.is_matrix())
				{
					const size_t length = subscript.size();
					if (length < 3)
						return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

					bool constant = false;
					signed char offsets[4] = { -1, -1, -1, -1 };
					const unsigned int set = subscript[1] == 'm';
					const int coefficient = !set;

					for (size_t i = 0, j = 0; i < length; i += 3 + set, ++j)
					{
						if (subscript[i] != '_' || subscript[i + set + 1] < '0' + coefficient || subscript[i + set + 1] > '3' + coefficient || subscript[i + set + 2] < '0' + coefficient || subscript[i + set + 2] > '3' + coefficient)
							return error(location, 3018, "invalid subscript '" + subscript + "'"), false;
						if (set && subscript[i + 1] != 'm')
							return error(location, 3018, "invalid subscript '" + subscript + "', mixed swizzle sets"), false;

						const unsigned int row = subscript[i + set + 1] - '0' - coefficient;
						const unsigned int col = subscript[i + set + 2] - '0' - coefficient;

						if ((row >= exp.type.rows || col >= exp.type.cols) || j > 3)
							return error(location, 3018, "invalid subscript '" + subscript + "', swizzle out of range"), false;

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

					// Add swizzle to current access chain
					exp.push_swizzle(offsets, length / (3 + set));

					if (constant || exp.type.has(qualifier_uniform))
						exp.type.qualifiers = (exp.type.qualifiers | qualifier_const) & ~qualifier_uniform;
				}
				else if (exp.type.is_struct())
				{
					size_t field_index = 0;

					for (const auto &currfield : _structs[exp.type.definition].field_list)
					{
						if (currfield.first == subscript)
							break;

						++field_index;
					}

					if (field_index >= _structs[exp.type.definition].field_list.size())
						return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

					// Add field index to current access chain
					exp.push_static_index(field_index, *this);

					exp.type = _structs[exp.type.definition].field_list[field_index].second;

					if (exp.type.has(qualifier_uniform))
						exp.type.qualifiers = (exp.type.qualifiers | qualifier_const) & ~qualifier_uniform;
				}
				else if (exp.type.is_scalar())
				{
					const size_t length = subscript.size();
					if (length > 4)
						return error(location, 3018, "invalid subscript '" + subscript + "', swizzle too long"), false;

					for (size_t i = 0; i < length; ++i)
						if ((subscript[i] != 'x' && subscript[i] != 'r' && subscript[i] != 's') || i > 3)
							return error(location, 3018, "invalid subscript '" + subscript + "'"), false;

					type_info target_type = exp.type;
					target_type.rows = static_cast<unsigned int>(length);

					exp.push_cast(target_type, false);
				}
				else
				{
					error(location, 3018, "invalid subscript '" + subscript + "'");
					return false;
				}
			}
			else if (accept('['))
			{
				if (!exp.type.is_array() && !exp.type.is_vector() && !exp.type.is_matrix())
					return error(_token.location, 3121, "array, matrix, vector, or indexable object type expected in index expression"), false;

				// Parse index expression
				access_chain index;
				if (!parse_expression(section, index) || !expect(']'))
					return false;
				else if (!index.type.is_scalar() || !index.type.is_integral())
					return error(index.location, 3120, "invalid type for index - index must be a scalar"), false;

				// Add index expression to current access chain
				if (index.is_constant)
					exp.push_static_index(index.constant.as_uint[0], *this);
				else
					exp.push_dynamic_index(access_chain_load(section, index));
			}
			else
			{
				break;
			}
		}
		#pragma endregion

		return true;
	}
	bool parser::parse_expression_multary(spv_section &section, access_chain &lhs, unsigned int left_precedence)
	{
		// Parse left hand side of the expression
		if (!parse_expression_unary(section, lhs))
			return false;

		// Check if an operator exists so that this is a binary or ternary expression
		spv::Op op;
		unsigned int right_precedence;

		while (peek_multary_op(op, right_precedence))
		{
			// Only process this operator if it has a lower precedence than the current operation, otherwise leave it for later and abort
			if (right_precedence <= left_precedence)
				break;

			// Finally consume the operator token
			consume();

			// Check if this is a binary or ternary operation
			if (op != spv::OpSelect)
			{
				// Parse the right hand side of the binary operation
				access_chain rhs;
				if (!parse_expression_multary(section, rhs, right_precedence))
					return false;

				bool boolean_result = false;

				// Deduce the result base type based on implicit conversion rules
				type_info type = { std::max(lhs.type.base, rhs.type.base), std::max(lhs.type.size, rhs.type.size), 1, 1, lhs.type.is_signed }; // This works because 'OpTypeFloat' is higher than 'OpTypeInt'

				// Do some error checking depending on the operator
				if (op == spv::OpFOrdEqual || op == spv::OpFOrdNotEqual)
				{
					// Select operator matching the argument types
					if (type.is_integral())
					{
						switch (op)
						{
						case spv::OpFOrdEqual:
							op = spv::OpIEqual;
							break;
						case spv::OpFOrdNotEqual:
							op = spv::OpINotEqual;
							break;
						}
					}

					// Equality checks return a boolean value
					boolean_result = true;

					// Cannot check equality between incompatible types
					if (lhs.type.is_array() || rhs.type.is_array() || lhs.type.definition != rhs.type.definition)
						return error(rhs.location, 3020, "type mismatch"), false;
				}
				else if (op == spv::OpBitwiseAnd || op == spv::OpBitwiseOr || op == spv::OpBitwiseXor)
				{
					// Cannot perform bitwise operations on non-integral types
					if (!lhs.type.is_integral())
						return error(lhs.location, 3082, "int or unsigned int type required"), false;
					if (!rhs.type.is_integral())
						return error(rhs.location, 3082, "int or unsigned int type required"), false;
				}
				else
				{
					// TODO: Short circuit for && and || operators

					// Logical operations return a boolean value
					if (op == spv::OpLogicalAnd || op == spv::OpLogicalOr ||
						op == spv::OpFOrdLessThan || op == spv::OpFOrdGreaterThan ||
						op == spv::OpFOrdLessThanEqual || op == spv::OpFOrdGreaterThanEqual)
						boolean_result = true;

					// Select operator matching the argument types
					if (type.is_integral())
					{
						switch (op)
						{
						case spv::OpFMod:
							op = type.is_signed ? spv::OpSMod : spv::OpUMod;
							break;
						case spv::OpFMul:
							op = spv::OpIMul;
							break;
						case spv::OpFAdd:
							op = spv::OpIAdd;
							break;
						case spv::OpFSub:
							op = spv::OpISub;
							break;
						case spv::OpFDiv:
							op = type.is_signed ? spv::OpSDiv : spv::OpUDiv;
							break;
						case spv::OpFOrdLessThan:
							op = type.is_signed ? spv::OpSLessThan : spv::OpULessThan;
							break;
						case spv::OpFOrdGreaterThan:
							op = type.is_signed ? spv::OpSGreaterThan : spv::OpUGreaterThan;
							break;
						case spv::OpFOrdLessThanEqual:
							op = type.is_signed ? spv::OpSLessThanEqual : spv::OpULessThanEqual;
							break;
						case spv::OpFOrdGreaterThanEqual:
							op = type.is_signed ? spv::OpSGreaterThanEqual : spv::OpUGreaterThanEqual;
							break;
						}
					}

					// Cannot perform arithmetic operations on non-basic types
					if (!lhs.type.is_scalar() && !lhs.type.is_vector() && !lhs.type.is_matrix())
						return error(lhs.location, 3022, "scalar, vector, or matrix expected"), false;
					if (!rhs.type.is_scalar() && !rhs.type.is_vector() && !rhs.type.is_matrix())
						return error(rhs.location, 3022, "scalar, vector, or matrix expected"), false;
				}

				// If one side of the expression is scalar, it needs to be promoted to the same dimension as the other side
				if ((lhs.type.rows == 1 && lhs.type.cols == 1) || (rhs.type.rows == 1 && rhs.type.cols == 1))
				{
					type.rows = std::max(lhs.type.rows, rhs.type.rows);
					type.cols = std::max(lhs.type.cols, rhs.type.cols);
				}
				else // Otherwise dimensions match or one side is truncated to match the other one
				{
					type.rows = std::min(lhs.type.rows, rhs.type.rows);
					type.cols = std::min(lhs.type.cols, rhs.type.cols);
				}

				// Load values and perform implicit type conversions
				lhs.push_cast(type, true);
				const spv::Id lhs_value = access_chain_load(section, lhs);
				rhs.push_cast(type, true);
				const spv::Id rhs_value = access_chain_load(section, rhs);

				// Certain operations return a boolean type instead of the type of the input expressions
				if (boolean_result)
					type.base = spv::OpTypeBool;

				spv::Id result = add_node(section, lhs.location, op, convert_type(type))
					.add(lhs_value) /* Operand 1 */
					.add(rhs_value) /* Operand 2 */
					.result; // Result ID

				lhs.reset_to_rvalue(result, type, lhs.location);
			}
			else
			{
				// A conditional expression needs a scalar or vector type condition
				if (!lhs.type.is_scalar() && !lhs.type.is_vector())
					return error(lhs.location, 3022, "boolean or vector expression expected"), false;

				// Parse the first part of the right hand side of the ternary operation
				access_chain true_exp;
				if (!parse_expression(section, true_exp))
					return false;

				if (!expect(':'))
					return false;

				// Parse the second part of the right hand side of the ternary operation
				access_chain false_exp;
				if (!parse_expression_assignment(section, false_exp))
					return false;

				// Check that the condition dimension matches that of at least one side
				if (lhs.type.is_vector() && lhs.type.rows != true_exp.type.rows && lhs.type.cols != true_exp.type.cols)
					return error(lhs.location, 3020, "dimension of conditional does not match value"), false;

				// Check that the two value expressions can be converted between each other
				if (true_exp.type.is_array() || false_exp.type.is_array() || true_exp.type.definition != false_exp.type.definition)
					return error(false_exp.location, 3020, "type mismatch between conditional values"), false;

				// Deduce the result base type based on implicit conversion rules
				type_info type = { std::max(true_exp.type.base, false_exp.type.base), std::max(true_exp.type.size, false_exp.type.size), 1, 1, true_exp.type.is_signed && false_exp.type.is_signed }; // This works because 'OpTypeFloat' is higher than 'OpTypeInt'

				// If one side of the expression is scalar need to promote it to the same dimension as the other side
				if ((true_exp.type.rows == 1 && true_exp.type.cols == 1) || (false_exp.type.rows == 1 && false_exp.type.cols == 1))
				{
					type.rows = std::max(true_exp.type.rows, false_exp.type.rows);
					type.cols = std::max(true_exp.type.cols, false_exp.type.cols);
				}
				else // Otherwise dimensions match or one side is truncated to match the other one
				{
					type.rows = std::min(true_exp.type.rows, false_exp.type.rows);
					type.cols = std::min(true_exp.type.cols, false_exp.type.cols);
				}

				// Load values and perform implicit type conversions
				lhs.push_cast({ spv::OpTypeBool, 32, lhs.type.rows, 1 }, true);
				const spv::Id condition_value = access_chain_load(section, lhs);
				true_exp.push_cast(type, true);
				const spv::Id true_value = access_chain_load(section, true_exp);
				false_exp.push_cast(type, true);
				const spv::Id false_value = access_chain_load(section, false_exp);

				spv::Id result = add_node(section, lhs.location, spv::OpSelect, convert_type(type))
					.add(condition_value) // Condition
					.add(true_value) // Object 1
					.add(false_value) // Object 2
					.result; // Result ID

				lhs.reset_to_rvalue(result, type, lhs.location);
			}
		}

		return true;
	}
	bool parser::parse_expression_assignment(spv_section &section, access_chain &lhs)
	{
		// Parse left hand side of the expression
		if (!parse_expression_multary(section, lhs))
			return false;

		// Check if an operator exists so that this is an assignment
		spv::Op op;
		if (accept_assignment_op(lhs.type, op))
		{
			// Parse right hand side of the assignment expression
			access_chain rhs;
			if (!parse_expression_multary(section, rhs))
				return false;

			// Cannot assign to constants and uniform variables
			if (lhs.type.has(qualifier_const) || lhs.type.has(qualifier_uniform) || !lhs.is_lvalue)
				return error(lhs.location, 3025, "l-value specifies const object"), false;

			// Cannot assign between arrays and incompatible types
			if (lhs.type.is_array() || rhs.type.is_array() || !type_info::rank(lhs.type, rhs.type))
				return error(rhs.location, 3020, "cannot convert these types"), false;

			// Load value of right hand side and perform implicit type conversion
			rhs.push_cast(lhs.type, true);
			spv::Id rhs_value = access_chain_load(section, rhs);

			// Check if this is an assignment with an additional arithmetic instruction
			if (op != spv::OpNop)
			{
				// Load value from left hand side as well to use in the operation
				spv::Id lhs_value = access_chain_load(section, lhs);

				// Handle arithmetic assignment operation
				const spv::Id result = add_node(section, lhs.location, op, convert_type(lhs.type))
					.add(lhs_value) // Operand 1
					.add(rhs_value) // Operand 2
					.result; // Result ID

				// The result of the operation should now be stored in the variable
				rhs_value = result;
			}

			// Write result back to variable
			access_chain_store(section, lhs, rhs_value, lhs.type);

			// Return the result value since you can write assignments within expressions
			lhs.reset_to_rvalue(rhs_value, lhs.type, lhs.location);
		}

		return true;
	}

	// Statements
	bool parser::parse_statement(spv_section &section, bool scoped)
	{
		unsigned int loop_control = spv::LoopControlMaskNone;
		unsigned int selection_control = spv::SelectionControlMaskNone;

		// Read any loop and branch control attributes first
		while (accept('['))
		{
			if (expect(tokenid::identifier))
			{
				const auto attribute = _token.literal_as_string;

				if (expect(']'))
				{
					if (attribute == "unroll")
						loop_control |= spv::LoopControlUnrollMask;
					else if (attribute == "loop")
						loop_control |= spv::LoopControlDontUnrollMask;
					else if (attribute == "branch")
						selection_control |= spv::SelectionControlDontFlattenMask;
					else if (attribute == "flatten")
						selection_control |= spv::SelectionControlFlattenMask;
					else
						warning(_token.location, 0, "unknown attribute");
				}
			}
			else
			{
				accept(']');
			}
		}

		// Parse statement block
		if (peek('{'))
			return parse_statement_block(section, scoped);

		// Ignore empty statements
		if (accept(';'))
			return true;

		#pragma region If
		if (accept(tokenid::if_))
		{
			const spv::Id true_label = make_id();
			const spv::Id false_label = make_id();
			const spv::Id merge_label = make_id();

			access_chain condition;

			if (!(expect('(') && parse_expression(section, condition) && expect(')')))
				return false;

			if (!condition.type.is_scalar())
				return error(condition.location, 3019, "if statement conditional expressions must evaluate to a scalar"), false;

			const spv::Id condition_value = access_chain_load(section, condition);

			add_node_without_result(section, _token.location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(selection_control); // Selection Control

			leave_block_and_branch_conditional(section, condition_value, true_label, false_label);

			enter_block(section, true_label);

			if (!parse_statement(section))
				return false;

			leave_block_and_branch(section, merge_label);

			enter_block(section, false_label);

			if (accept(tokenid::else_) && !parse_statement(section))
				return false;

			leave_block_and_branch(section, merge_label);

			enter_block(section, merge_label);

			return true;
		}
		#pragma endregion

		#pragma region Switch
		if (accept(tokenid::switch_))
		{
			const auto location = _token.location;

			const spv::Id merge_label = make_id();
			spv::Id default_label = merge_label;
			std::vector<spv::Id> case_literal_and_labels;

			access_chain selector;

			if (!expect('(') || !parse_expression(section, selector) || !expect(')'))
				return false;

			if (!selector.type.is_scalar())
				return error(selector.location, 3019, "switch statement expression must evaluate to a scalar"), false;

			// Load selector value and convert to 'OpTypeInt' as required by 'OpSwitch'
			selector.push_cast({ spv::OpTypeInt, 32, 1, 1 }, true);
			const spv::Id selector_value = access_chain_load(section, selector);

			add_node_without_result(section, location, spv::OpSelectionMerge)
				.add(merge_label) // Merge Block
				.add(selection_control); // Selection Control

			size_t switch_index = add_node_without_result(section, _token.location, spv::OpSwitch)
				.add(selector_value).index;

			if (!expect('{'))
				return false;

			_loop_break_target_stack.push_back(merge_label);

			spv::Id current_block = 0;
			unsigned int num_case_labels = 0;

			while (!peek('}') && !peek(tokenid::end_of_file))
			{
				if (peek(tokenid::case_) || peek(tokenid::default_))
				{
					current_block = make_id();

					// Handle fall-through case
					if (num_case_labels != 0)
						leave_block_and_branch(section, current_block);

					enter_block(section, current_block);
				}

				while (accept(tokenid::case_) || accept(tokenid::default_))
				{
					if (_token.id == tokenid::case_)
					{
						access_chain case_label;
						if (!parse_expression(_temporary, case_label))
						{
							_loop_break_target_stack.pop_back();
							return false;
						}

						if (!case_label.type.is_numeric())
						{
							error(case_label.location, 3020, "non-numeric case expression");

							_loop_break_target_stack.pop_back();
							return false;
						}

						unsigned int literal_value = 0; // TODO

						case_literal_and_labels.push_back(literal_value);
						case_literal_and_labels.push_back(current_block);
					}
					else
					{
						default_label = current_block;
					}

					if (!expect(':'))
					{
						_loop_break_target_stack.pop_back();
						return false;
					}

					num_case_labels++;
				}

				if (!parse_statement(section))
				{
					_loop_break_target_stack.pop_back();
					return false;
				}
			}

			if (num_case_labels == 0)
				warning(location, 5002, "switch statement contains no 'case' or 'default' labels");

			// Add all case labels to the switch instruction
			section.instructions[switch_index].add(default_label);
			for (spv::Id operand : case_literal_and_labels)
				section.instructions[switch_index].add(operand);

			enter_block(section, merge_label);

			_loop_break_target_stack.pop_back();

			return expect('}');
		}
		#pragma endregion

		#pragma region For
		if (accept(tokenid::for_))
		{
			const auto location = _token.location;

			if (!expect('('))
				return false;

			_symbol_table->enter_scope();

			// Parse initializer first
			if (type_info type; parse_type(type))
			{
				unsigned int count = 0;

				do // There may be multiple declarations behind a type, so loop through them
				{
					if (count++ > 0 && !expect(','))
						return false;

					if (!expect(tokenid::identifier))
						return false;

					spv::Id variable_id = 0;
					if (!parse_variable_declaration(section, type, _token.literal_as_string, variable_id))
						return false;
				} while (!peek(';'));
			}
			// Initializer can also contain an expression if not a variable declaration list
			else if (access_chain expression; parse_expression(section, expression))
			{
			}

			if (!expect(';'))
			{
				_symbol_table->leave_scope();
				return false;
			}

			const spv::Id header_label = make_id(); // Pointer to the loop merge instruction
			const spv::Id loop_label = make_id(); // Pointer to the main loop body block
			const spv::Id merge_label = make_id(); // Pointer to the end of the loop
			const spv::Id continue_label = make_id(); // Pointer to the continue block
			const spv::Id condition_label = make_id(); // Pointer to the condition check

			leave_block_and_branch(section, header_label);

			enter_block(section, header_label);

			add_node_without_result(section, location, spv::OpLoopMerge)
				.add(merge_label) // Merge Block
				.add(continue_label) // Continue Target
				.add(loop_control); // Loop Control

			leave_block_and_branch(section, condition_label);

			// Parse condition block
			enter_block(section, condition_label);

			access_chain condition;

			parse_expression(section, condition);

			if (!expect(';'))
			{
				_symbol_table->leave_scope();
				return false;
			}

			// Cannot have a non-scalar loop condition
			if (!condition.type.is_scalar())
			{
				error(condition.location, 3019, "scalar value expected");

				_symbol_table->leave_scope();
				return false;
			}

			// Evaluate condition and branch to the right target
			const spv::Id condition_value = access_chain_load(section, condition);

			leave_block_and_branch_conditional(section, condition_value, loop_label, merge_label);

			// Parse loop continue block into separate section so it can be appended to the end later
			spv_section continue_section;
			{
				enter_block(continue_section, continue_label);

				access_chain continue_exp;
				parse_expression(continue_section, continue_exp);

				if (!expect(')'))
				{
					_symbol_table->leave_scope();
					return false;
				}

				// Branch back to the loop header at the end of the continue block
				leave_block_and_branch(continue_section, header_label);
			}

			// Parse loop body block
			enter_block(section, loop_label);

			_loop_break_target_stack.push_back(merge_label);
			_loop_continue_target_stack.push_back(continue_label);

			if (!parse_statement(section, false))
			{
				_symbol_table->leave_scope();
				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();
				return false;
			}

			_loop_break_target_stack.pop_back();
			_loop_continue_target_stack.pop_back();

			leave_block_and_branch(section, continue_label);

			// Append continue section after the main block
			section.instructions.insert(section.instructions.end(),
				continue_section.instructions.begin(), continue_section.instructions.end());

			// Add merge block label to the end of the loop
			enter_block(section, merge_label);

			_symbol_table->leave_scope();

			return true;
		}
		#pragma endregion

		#pragma region While
		if (accept(tokenid::while_))
		{
			_symbol_table->enter_scope();

			const spv::Id header_label = make_id();
			const spv::Id loop_label = make_id();
			const spv::Id merge_label = make_id();
			const spv::Id continue_label = make_id();
			const spv::Id condition_label = make_id();

			// End current block by branching to the next label
			leave_block_and_branch(section, header_label);

			// Begin loop block
			enter_block(section, header_label);

			add_node_without_result(section, _token.location, spv::OpLoopMerge)
				.add(merge_label) // Merge Block
				.add(continue_label) // Continue Target
				.add(loop_control); // Loop Control

			leave_block_and_branch(section, condition_label);

			// Parse condition block
			enter_block(section, condition_label);

			access_chain condition;

			if (!(expect('(') && parse_expression(section, condition) && expect(')')))
			{
				_symbol_table->leave_scope();
				return false;
			}

			if (!condition.type.is_scalar())
			{
				error(condition.location, 3019, "scalar value expected");

				_symbol_table->leave_scope();
				return false;
			}

			// Evaluate condition and branch to the right target
			const spv::Id condition_value = access_chain_load(section, condition);

			leave_block_and_branch_conditional(section, condition_value, loop_label, merge_label);

			// Parse loop body block
			enter_block(section, loop_label);

			_loop_break_target_stack.push_back(merge_label);
			_loop_continue_target_stack.push_back(continue_label);

			if (!parse_statement(section, false))
			{
				_symbol_table->leave_scope();
				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();
				return false;
			}

			_loop_break_target_stack.pop_back();
			_loop_continue_target_stack.pop_back();

			leave_block_and_branch(section, continue_label);

			// Branch back to the loop header in empty continue block
			enter_block(section, continue_label);

			leave_block_and_branch(section, header_label);

			// Add merge block label to the end of the loop
			enter_block(section, merge_label);

			_symbol_table->leave_scope();

			return true;
		}
		#pragma endregion

		#pragma region DoWhile
		if (accept(tokenid::do_))
		{
			const spv::Id header_label = make_id();
			const spv::Id loop_label = make_id();
			const spv::Id merge_label = make_id();
			const spv::Id continue_label = make_id();

			// End current block by branching to the next label
			leave_block_and_branch(section, header_label);

			// Begin loop block
			enter_block(section, header_label);

			add_node_without_result(section, _token.location, spv::OpLoopMerge)
				.add(merge_label) // Merge Block
				.add(continue_label) // Continue Target
				.add(loop_control); // Loop Control

			leave_block_and_branch(section, loop_label);

			// Parse loop body block
			enter_block(section, loop_label);

			_loop_break_target_stack.push_back(merge_label);
			_loop_continue_target_stack.push_back(continue_label);

			if (!parse_statement(section))
			{
				_loop_break_target_stack.pop_back();
				_loop_continue_target_stack.pop_back();
				return false;
			}

			_loop_break_target_stack.pop_back();
			_loop_continue_target_stack.pop_back();

			// Continue block does the condition evaluation
			enter_block(section, continue_label);

			access_chain condition;

			if (!expect(tokenid::while_) || !expect('(') || !parse_expression(section, condition) || !expect(')') || !expect(';'))
				return false;

			if (!condition.type.is_scalar())
				return error(condition.location, 3019, "scalar value expected"), false;

			const spv::Id condition_value = access_chain_load(section, condition);

			leave_block_and_branch_conditional(section, condition_value, header_label, merge_label);

			// Add merge block label to the end of the loop
			enter_block(section, merge_label);


			return true;
		}
		#pragma endregion

		#pragma region Break
		if (accept(tokenid::break_))
		{
			if (_loop_break_target_stack.empty())
				return error(_token.location, 3518, "break must be inside loop"), false;

			// Branch to the break target of the inner most loop on the stack
			leave_block_and_branch(section, _loop_break_target_stack.back());

			return expect(';');
		}
		#pragma endregion

		#pragma region Continue
		if (accept(tokenid::continue_))
		{
			if (_loop_continue_target_stack.empty())
				return error(_token.location, 3519, "continue must be inside loop"), false;

			// Branch to the continue target of the inner most loop on the stack
			leave_block_and_branch(section, _loop_continue_target_stack.back());

			return expect(';');
		}
		#pragma endregion

		#pragma region Return
		if (accept(tokenid::return_))
		{
			const auto parent = static_cast<const function_info *>(_symbol_table->current_parent());
			const auto location = _token.location;

			assert(parent != nullptr);

			if (!peek(';'))
			{
				access_chain return_exp;
				if (!parse_expression(section, return_exp))
					return false;

				if (parent->return_type.is_void())
				{
					error(location, 3079, "void functions cannot return a value");

					// Consume the semicolon that follows the return expression so that parsing may continue
					accept(';');

					return false;
				}

				if (!type_info::rank(return_exp.type, parent->return_type))
					return error(location, 3017, "expression does not match function return type"), false;

				// Load return value and perform implicit cast to function return type
				return_exp.push_cast(parent->return_type, true);
				const spv::Id return_value = access_chain_load(section, return_exp);

				add_node_without_result(section, location, spv::OpReturnValue)
					.add(return_value); // Value
			}
			else if (!parent->return_type.is_void())
			{
				// No return value was found, but the function expects one
				error(location, 3080, "function must return a value");

				// Consume the semicolon that follows the return expression so that parsing may continue
				accept(';');

				return false;
			}
			else
			{
				add_node_without_result(section, location, spv::OpReturn);
			}

			return expect(';');
		}
		#pragma endregion

		#pragma region Discard
		if (accept(tokenid::discard_))
		{
			add_node_without_result(section, _token.location, spv::OpKill);

			return expect(';');
		}
		#pragma endregion

		#pragma region Declaration
		const auto location = _token_next.location;

		// Handle variable declarations
		if (type_info type; parse_type(type))
		{
			unsigned int count = 0;

			do // There may be multiple declarations behind a type, so loop through them
			{
				if (count++ > 0 && !expect(','))
					return false;

				if (!expect(tokenid::identifier))
					return false;

				spv::Id variable_id = 0;
				if (!parse_variable_declaration(section, type, _token.literal_as_string, variable_id))
					return false;
			} while (!peek(';'));

			return expect(';');
		}
		#pragma endregion

		// Handle expression statements
		if (access_chain expression; parse_expression(section, expression))
			return expect(';');

		// No token should come through here, since all statements and expressions should have been handled above, so this is an error in the syntax
		error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "'");

		// Gracefully consume any remaining characters until the statement would usually end, so that parsing may continue despite the error
		consume_until(';');

		return false;
	}
	bool parser::parse_statement_block(spv_section &section, bool scoped)
	{
		if (!expect('{'))
			return false;

		if (scoped)
			_symbol_table->enter_scope();

		while (!peek('}') && !peek(tokenid::end_of_file))
		{
			if (!parse_statement(section))
			{
				if (scoped)
				{
					_symbol_table->leave_scope();
				}

				unsigned level = 0;

				while (!peek(tokenid::end_of_file))
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
		}

		if (scoped)
		{
			_symbol_table->leave_scope();
		}

		return expect('}');
	}

	// Declarations
	bool parser::parse_top_level()
	{
		type_info type = { spv::OpTypeVoid };

		if (peek(tokenid::namespace_))
		{
			return parse_namespace();
		}
		else if (peek(tokenid::struct_))
		{
			spv::Id type_id = 0;

			if (!parse_struct(type_id))
				return false;

			if (!expect(';'))
				return false;
		}
		else if (peek(tokenid::technique))
		{
			technique_properties technique;

			if (!parse_technique(technique))
				return false;

			techniques.push_back(std::move(technique));
		}
		else if (parse_type(type))
		{
			if (!expect(tokenid::identifier))
				return false;

			if (peek('('))
			{
				spv::Id function_id = 0;

				if (!parse_function_declaration(type, _token.literal_as_string, function_id))
					return false;
			}
			else
			{
				unsigned int count = 0;

				do
				{
					if (count++ > 0 && !(expect(',') && expect(tokenid::identifier)))
						return false;

					spv::Id variable_id = 0;

					if (!parse_variable_declaration(_variables, type, _token.literal_as_string, variable_id, true))
					{
						consume_until(';');
						return false;
					}
				}
				while (!peek(';'));

				if (!expect(';'))
					return false;
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
		if (!accept(tokenid::namespace_) || !expect(tokenid::identifier))
			return false;

		const auto name = _token.literal_as_string;

		if (!expect('{'))
			return false;

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

#if 0
		if (accept('['))
		{
			spv::Id expression_id = 0;

			if (accept(']'))
			{
				size = -1;
			}
			else if (parse_expression(expression_id) && expect(']'))
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
#endif

		return true;
	}
	bool parser::parse_annotations(std::unordered_map<std::string, reshade::variant> &annotations)
	{
		if (!accept('<'))
			return true;

		bool success = true;

		while (!peek('>'))
		{
			type_info type;

			if (accept_type_class(type))
				warning(_token.location, 4717, "type prefixes for annotations are deprecated");

			if (!expect(tokenid::identifier))
				return false;

			const auto name = _token.literal_as_string;

			access_chain expression;
			if (!expect('=') || !parse_expression_unary(_temporary, expression) || !expect(';'))
				return false;

			if (!expression.is_constant)
			{
				error(expression.location, 3011, "value must be a literal expression");
				success = false;
				continue;
			}

			switch (expression.type.base)
			{
			case spv::OpTypeInt:
			case spv::OpTypeBool:
				annotations[name] = expression.constant.as_uint[0];
				break;
			case spv::OpTypeFloat:
				annotations[name] = expression.constant.as_float[0];
				break;
			case spv::OpString:
				annotations[name] = expression.constant.as_string;
				break;
			}
		}

		return expect('>') && success;
	}

	bool parser::parse_struct(spv::Id &type_id)
	{
		if (!accept(tokenid::struct_))
			return false;

		const auto location = _token.location;

		std::string name;

		if (accept(tokenid::identifier))
			name = _token.literal_as_string;
		else
			name = "__anonymous_struct_" + std::to_string(location.line) + '_' + std::to_string(location.column);

		//structure->unique_name = 'S' + _symbol_table->current_scope().name + structure->name;
		//std::replace(structure->unique_name.begin(), structure->unique_name.end(), ':', '_');

		if (!expect('{'))
			return false;

		struct_info info;
		std::vector<spv::Id> field_list;
		std::vector<std::string> field_names;

		while (!peek('}'))
		{
			type_info type;

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
			if (type.has(qualifier_in) || type.has(qualifier_out))
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

				if (!expect(tokenid::identifier))
				{
					consume_until('}');
					return false;
				}

				const std::string field_name = _token.literal_as_string;

				if (!parse_array(type.array_length))
					return false;

				if (accept(':'))
				{
					if (!expect(tokenid::identifier))
					{
						consume_until('}');
						return false;
					}

					//field->semantic = _token.literal_as_string;
					//std::transform(field->semantic.begin(), field->semantic.end(), field->semantic.begin(), ::toupper);
				}

				// Add field type to list
				field_list.push_back(convert_type(type));
				field_names.push_back(field_name);

				// Save field name and type for book keeping
				info.field_list.push_back({ _token.literal_as_string, type });

				// Reset array length
				type.array_length = 0;
			}
			while (!peek(';'));

			if (!expect(';'))
			{
				consume_until('}');
				return false;
			}
		}

		if (field_list.empty())
		{
			warning(location, 5001, "struct has no members");

			// A structure with no body specified is an opaque type
			type_id = add_node(_variables, _token.location, spv::OpTypeOpaque)
				.add_string(name.c_str()) // The name of the opaque type
				.result;
		}
		else
		{
			spv_node &node = add_node(_variables, _token.location, spv::OpTypeStruct);

			for (spv::Id type : field_list)
				node.add(type); // Member type

			type_id = node.result;
		}

		add_name(type_id, name.c_str());

		for (uint32_t i = 0; i < field_names.size(); ++i)
		{
			add_member_name(type_id, i, field_names[i].c_str());
		}

		if (!_symbol_table->insert(name, { spv::OpTypeStruct, type_id }, true))
		{
			error(_token.location, 3003, "redefinition of '" + name + "'");
			return false;
		}

		return expect('}');
	}

	bool parser::parse_function_declaration(type_info &type, std::string name, spv::Id &node_id)
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

		type.qualifiers = qualifier_const;

		function_info &function = *_functions.emplace_back(new function_info());

		node_id = add_node(function.variables, location, spv::OpFunction, convert_type(type)) // TODO
			.add(spv::FunctionControlInlineMask)
			.result; // Function Control

		function.name = name;
		function.unique_name = 'F' + _symbol_table->current_scope().name + name;
		std::replace(function.unique_name.begin(), function.unique_name.end(), ':', '_');

		add_name(node_id, name.c_str());

		function.definition = node_id;

		function.return_type = type;

		_symbol_table->insert(name, { spv::OpTypeFunction, node_id, {}, &function }, true);

		_symbol_table->enter_scope(&function);

		unsigned int num_params = 0;

		while (!peek(')'))
		{
			if (num_params != 0 && !expect(','))
			{
				_symbol_table->leave_scope();

				return false;
			}

			type_info param_type;

			if (!parse_type(param_type))
			{
				_symbol_table->leave_scope();

				error(_token_next.location, 3000, "syntax error: unexpected '" + get_token_name(_token_next.id) + "', expected parameter type");

				return false;
			}

			if (!expect(tokenid::identifier))
			{
				_symbol_table->leave_scope();

				return false;
			}

			std::string param_name = _token.literal_as_string;
			auto param_location = _token.location;
			//parameter->unique_name = parameter->name = _token.literal_as_string;

			function.parameter_list.push_back(param_type);

			if (param_type.is_void())
			{
				error(param_location, 3038, "function parameters cannot be void");

				_symbol_table->leave_scope();

				return false;
			}
			if (param_type.has(qualifier_extern))
			{
				error(param_location, 3006, "function parameters cannot be declared 'extern'");

				_symbol_table->leave_scope();

				return false;
			}
			if (param_type.has(qualifier_static))
			{
				error(param_location, 3007, "function parameters cannot be declared 'static'");

				_symbol_table->leave_scope();

				return false;
			}
			if (param_type.has(qualifier_uniform))
			{
				error(param_location, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");

				_symbol_table->leave_scope();

				return false;
			}

			if (param_type.has(qualifier_out))
			{
				if (param_type.has(qualifier_const))
				{
					error(param_location, 3046, "output parameters cannot be declared 'const'");

					_symbol_table->leave_scope();

					return false;
				}
			}
			else
			{
				param_type.qualifiers |= qualifier_in;
			}

			if (!parse_array(param_type.array_length))
				return false;

			param_type.is_pointer = true;

			spv::Id param = add_node(function.variables, param_location, spv::OpFunctionParameter, convert_type(param_type))
				.result;

			if (!_symbol_table->insert(param_name, { spv::OpVariable, param, param_type }))
			{
				error(param_location, 3003, "redefinition of '" + param_name + "'");

				_symbol_table->leave_scope();

				return false;
			}

			if (accept(':'))
			{
				if (!expect(tokenid::identifier))
				{
					_symbol_table->leave_scope();

					return false;
				}

				// TODO
				//parameter->semantic = _token.literal_as_string;
				//std::transform(parameter->semantic.begin(), parameter->semantic.end(), parameter->semantic.begin(), ::toupper);
			}

			num_params++;
		}

		if (!expect(')'))
		{
			_symbol_table->leave_scope();

			return false;
		}

		if (accept(':'))
		{
			if (!expect(tokenid::identifier))
			{
				_symbol_table->leave_scope();

				return false;
			}

			// TODO
			//function->return_semantic = _token.literal_as_string;
			//std::transform(function->return_semantic.begin(), function->return_semantic.end(), function->return_semantic.begin(), ::toupper);

			if (type.is_void())
			{
				error(_token.location, 3076, "void function cannot have a semantic");
				return false;
			}
		}

		lookup_id(node_id).add(convert_type(function)); // Function Type

		// A function has to start with a label
		enter_block(function.variables, make_id());

		if (!parse_statement_block(function.code, false))
		{
			_symbol_table->leave_scope();

			return false;
		}

		_symbol_table->leave_scope();

		// Add implicit return statement to the end of void functions
		if (function.return_type.is_void())
		{
			add_node_without_result(function.code, location, spv::OpReturn);
		}

		add_node_without_result(function.code, location, spv::OpFunctionEnd);

		return true;
	}

	bool parser::parse_variable_declaration(spv_section &section, type_info &type, std::string name, spv::Id &node_id, bool global)
	{
		auto location = _token.location;

		if (type.is_void())
			return error(location, 3038, "variables cannot be void"), false;
		if (type.has(qualifier_in) || type.has(qualifier_out))
			return error(location, 3055, "variables cannot be declared 'in' or 'out'"), false;

		const auto parent = _symbol_table->current_parent();

		if (!parent)
		{
			if (!type.has(qualifier_static))
			{
				if (!type.has(qualifier_uniform) && !(type.is_texture() || type.is_sampler()))
					warning(location, 5000, "global variables are considered 'uniform' by default");

				if (type.has(qualifier_const))
					return error(location, 3035, "variables which are 'uniform' cannot be declared 'const'"), false;

				type.qualifiers |= qualifier_extern | qualifier_uniform;
			}
		}
		else
		{
			if (type.has(qualifier_extern))
				return error(location, 3006, "local variables cannot be declared 'extern'"), false;
			if (type.has(qualifier_uniform))
				return error(location, 3047, "local variables cannot be declared 'uniform'"), false;

			if (type.is_texture() || type.is_sampler())
				return error(location, 3038, "local variables cannot be textures or samplers"), false;
		}

		if (!parse_array(type.array_length))
			return false;

		spv::StorageClass storage = spv::StorageClassPrivate;

		if (global)
		{
			//variable->unique_name = (type.has_qualifier(type_node::qualifier_uniform) ? 'U' : 'V') + _symbol_table->current_scope().name + variable->name;
			//std::replace(variable->unique_name.begin(), variable->unique_name.end(), ':', '_');
		}
		else
		{
			//variable->unique_name = variable->name;
			storage = spv::StorageClassFunction;
		}

		variable_info props; // TODO
		access_chain initializer = {};

		if (accept(':'))
		{
			if (!expect(tokenid::identifier))
				return false;

			// TODO
			//variable->semantic = _token.literal_as_string;
			//std::transform(variable->semantic.begin(), variable->semantic.end(), variable->semantic.begin(), ::toupper);
		}
		else
		{
			if (global && !parse_annotations(props.annotation_list))
				return false;

			if (accept('='))
			{
				if (!parse_variable_assignment(_temporary, initializer)) // TODO OBVIOUSLY NOT "_TEMPORARY"!!!!!!
				{
					return false;
				}

#if 0 // TODO
				if (!parent && lookup_id(initializer.base).op != spv::OpConstant)
					return error(initializer.location, 3011, "initial value must be a literal expression"), false;

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
#endif

				if (!type_info::rank(initializer.type, type))
					return error(initializer.location, 3017, "initial value does not match variable type"), false;
				if ((initializer.type.rows < type.rows || initializer.type.cols < type.cols) && !initializer.type.is_scalar())
					return error(initializer.location, 3017, "cannot implicitly convert these vector types"), false;

				if (initializer.type.rows > type.rows || initializer.type.cols > type.cols)
					warning(initializer.location, 3206, "implicit truncation of vector type");
			}
			else if (type.is_numeric())
			{
				if (type.has(qualifier_const))
					return error(location, 3012, "missing initial value for '" + name + "'"), false;
				else if (!type.has(qualifier_uniform) && !type.is_array())
					initializer.reset_to_rvalue(convert_constant(type, {}), type, location); // TODO
			}
			else if (peek('{'))
			{
				if (!parse_variable_properties(props))
					return false;
			}
		}

		if (type.is_sampler())
		{
			if (!props.texture)
				return error(location, 3012, "missing 'Texture' property for '" + name + "'"), false;

			auto image_ptr = lookup_id(props.texture).result_type;
			type.definition = lookup_id(image_ptr).operands[1];
		}

		type.is_pointer = true;

		if (type.has(qualifier_const))
		{
			// TODO: These are constants not variables
		}

		spv_node &node = add_node(section, location, spv::OpVariable, convert_type(type)) // TODO
			.add(storage);

		node_id = node.result;

		if (initializer.is_constant)
		{
			node.add(access_chain_load(_variables, initializer));
		}
		else
		{
			spv::Id initializer_value = access_chain_load(section, initializer);

			assert(!global);
			access_chain variable;
			variable.reset_to_lvalue(node_id, type, location);
			type.is_pointer = false;
			access_chain_store(section, variable, initializer_value, type);
			type.is_pointer = true;
		}

		add_name(node_id, name.c_str());

		if (!_symbol_table->insert(name, { spv::OpVariable, node_id, type }, global))
		{
			error(location, 3003, "redefinition of '" + name + "'");
			return false;
		}

		return true;
	}

	bool parser::parse_variable_assignment(spv_section &section, access_chain &exp)
	{
#if 0 // TODO
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
#endif

		return parse_expression_assignment(section, exp);
	}

	bool parser::parse_variable_properties(variable_info &props)
	{
		if (!expect('{'))
			return false;

		while (!peek('}'))
		{
			if (!expect(tokenid::identifier))
				return false;

			const auto name = _token.literal_as_string;
			const auto location = _token.location;

			access_chain value_exp;

			if (!expect('=') || !parse_variable_properties_expression(value_exp) || !expect(';'))
				return false;

			if (name == "Texture")
			{
				if (!value_exp.type.is_texture())
					return error(location, 3020, "type mismatch, expected texture name"), false;

				props.texture = value_exp.base;
			}
			else
			{
				if (!value_exp.is_constant)
					return error(location, 3011, "value must be a literal expression"), false;

				const auto value_literal = value_exp.constant.as_uint[0]; // TODO

				if (name == "Width")
				{
					//scalar_literal_cast(value_literal, 0, props.width);
				}
				else if (name == "Height")
				{
					//scalar_literal_cast(value_literal, 0, props.height);
				}
				else if (name == "Depth")
				{
					//scalar_literal_cast(value_literal, 0, props.depth);
				}
				else if (name == "MipLevels")
				{
					//scalar_literal_cast(value_literal, 0, props.levels);

					if (props.levels == 0)
					{
						warning(location, 0, "a texture cannot have 0 mipmap levels, changed it to 1");

						props.levels = 1;
					}
				}
				else if (name == "Format")
				{
					unsigned int format = value_literal;
					//scalar_literal_cast(value_literal, 0, format);
					props.format = static_cast<reshade::texture_format>(format);
				}
				else if (name == "SRGBTexture" || name == "SRGBReadEnable")
				{
					props.srgb_texture = value_literal != 0;
				}
				else if (name == "AddressU")
				{
					unsigned address_mode = value_literal;
					//scalar_literal_cast(value_literal, 0, address_mode);
					props.address_u = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "AddressV")
				{
					unsigned address_mode = value_literal;
					//scalar_literal_cast(value_literal, 0, address_mode);
					props.address_v = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "AddressW")
				{
					unsigned address_mode = value_literal;
					//scalar_literal_cast(value_literal, 0, address_mode);
					props.address_w = static_cast<reshade::texture_address_mode>(address_mode);
				}
				else if (name == "MinFilter")
				{
					unsigned int a = static_cast<unsigned int>(props.filter), b = value_literal;
					//scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x0F) | ((b << 4) & 0x30);
					props.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MagFilter")
				{
					unsigned int a = static_cast<unsigned int>(props.filter), b = value_literal;
					//scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x33) | ((b << 2) & 0x0C);
					props.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MipFilter")
				{
					unsigned int a = static_cast<unsigned int>(props.filter), b = value_literal;
					//scalar_literal_cast(value_literal, 0, b);

					b = (a & 0x3C) | (b & 0x03);
					props.filter = static_cast<reshade::texture_filter>(b);
				}
				else if (name == "MinLOD" || name == "MaxMipLevel")
				{
					//scalar_literal_cast(value_literal, 0, props.min_lod);
				}
				else if (name == "MaxLOD")
				{
					//scalar_literal_cast(value_literal, 0, props.max_lod);
				}
				else if (name == "MipLODBias" || name == "MipMapLodBias")
				{
					//scalar_literal_cast(value_literal, 0, props.lod_bias);
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

	bool parser::parse_technique(technique_properties &technique)
	{
		if (!accept(tokenid::technique))
		{
			return false;
		}

		technique.location = _token.location;

		if (!expect(tokenid::identifier))
		{
			return false;
		}

		technique.name = _token.literal_as_string;
		technique.unique_name = 'T' + _symbol_table->current_scope().name + technique.name;
		std::replace(technique.unique_name.begin(), technique.unique_name.end(), ':', '_');

		if (!parse_annotations(technique.annotation_list))
		{
			return false;
		}

		if (!expect('{'))
		{
			return false;
		}

		while (!peek('}'))
		{
			pass_properties pass;

			if (!parse_technique_pass(pass))
			{
				return false;
			}

			technique.pass_list.push_back(std::move(pass));
		}

		return expect('}');
	}
	bool parser::parse_technique_pass(pass_properties &pass)
	{
		if (!expect(tokenid::pass))
			return false;

		pass.location = _token.location;

		if (accept(tokenid::identifier))
			pass.name = _token.literal_as_string;

		if (!expect('{'))
			return false;

		while (!peek('}'))
		{
			if (!expect(tokenid::identifier))
			{
				return false;
			}

			const auto passstate = _token.literal_as_string;
			const auto location = _token.location;

			access_chain value_exp;

			if (!(expect('=') && parse_technique_pass_expression(value_exp) && expect(';')))
			{
				return false;
			}

			if (passstate == "VertexShader" || passstate == "PixelShader")
			{
				if (lookup_id(value_exp.base).op != spv::OpFunction)
				{
					error(location, 3020, "type mismatch, expected function name");

					return false;
				}

				auto &node = add_node_without_result(_entries, {}, spv::OpEntryPoint);

				if (passstate[0] == 'V')
				{
					node.add(spv::ExecutionModelVertex);
					pass.vertex_shader = value_exp.base;
				}
				else
				{
					node.add(spv::ExecutionModelFragment);
					pass.pixel_shader = value_exp.base;
				}

				std::string name = "main";

				for (auto &f : _functions)
				{
					if (f->definition == value_exp.base)
					{
						name = f->name;
						break;
					}
				}

				node.add(value_exp.base);
				node.add_string(name.c_str());
			}
			else if (passstate.compare(0, 12, "RenderTarget") == 0 && (passstate == "RenderTarget" || (passstate[12] >= '0' && passstate[12] < '8')))
			{
				size_t index = 0;

				if (passstate.size() == 13)
				{
					index = passstate[12] - '0';
				}

				if (!value_exp.type.is_texture())
					return error(location, 3020, "type mismatch, expected texture name"), false;

				pass.render_targets[index] = value_exp.base;
			}
			else
			{
				if (!value_exp.is_constant)
					return error(location, 3011, "pass state value must be a literal expression"), false;

				const auto value_literal = value_exp.constant.as_uint[0]; // TODO

				if (passstate == "SRGBWriteEnable")
				{
					pass.srgb_write_enable = value_literal != 0;
				}
				else if (passstate == "BlendEnable")
				{
					pass.blend_enable = value_literal != 0;
				}
				else if (passstate == "StencilEnable")
				{
					pass.stencil_enable = value_literal != 0;
				}
				else if (passstate == "ClearRenderTargets")
				{
					pass.clear_render_targets = value_literal != 0;
				}
				else if (passstate == "RenderTargetWriteMask" || passstate == "ColorWriteMask")
				{
					unsigned int mask = value_literal; // TODO
					//scalar_literal_cast(value_literal, 0, mask);

					pass.color_write_mask = mask & 0xFF;
				}
				else if (passstate == "StencilReadMask" || passstate == "StencilMask")
				{
					unsigned int mask = value_literal; // TODO
					//scalar_literal_cast(value_literal, 0, mask);

					pass.stencil_read_mask = mask & 0xFF;
				}
				else if (passstate == "StencilWriteMask")
				{
					unsigned int mask = value_literal; // TODO
					//scalar_literal_cast(value_literal, 0, mask);

					pass.stencil_write_mask = mask & 0xFF;
				}
				else if (passstate == "BlendOp")
				{
					//scalar_literal_cast(value_literal, 0, pass.blend_op);
				}
				else if (passstate == "BlendOpAlpha")
				{
					//scalar_literal_cast(value_literal, 0, pass.blend_op_alpha);
				}
				else if (passstate == "SrcBlend")
				{
					//scalar_literal_cast(value_literal, 0, pass.src_blend);
				}
				else if (passstate == "SrcBlendAlpha")
				{
					//scalar_literal_cast(value_literal, 0, pass.src_blend_alpha);
				}
				else if (passstate == "DestBlend")
				{
					//scalar_literal_cast(value_literal, 0, pass.dest_blend);
				}
				else if (passstate == "DestBlend")
				{
					//scalar_literal_cast(value_literal, 0, pass.dest_blend_alpha);
				}
				else if (passstate == "StencilFunc")
				{
					//scalar_literal_cast(value_literal, 0, pass.stencil_comparison_func);
				}
				else if (passstate == "StencilRef")
				{
					//scalar_literal_cast(value_literal, 0, pass.stencil_reference_value);
				}
				else if (passstate == "StencilPass" || passstate == "StencilPassOp")
				{
					//scalar_literal_cast(value_literal, 0, pass.stencil_op_pass);
				}
				else if (passstate == "StencilFail" || passstate == "StencilFailOp")
				{
					//scalar_literal_cast(value_literal, 0, pass.stencil_op_fail);
				}
				else if (passstate == "StencilZFail" || passstate == "StencilDepthFail" || passstate == "StencilDepthFailOp")
				{
					//scalar_literal_cast(value_literal, 0, pass.stencil_op_depth_fail);
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

	bool parser::parse_variable_properties_expression(access_chain &exp)
	{
		backup();

		if (accept(tokenid::identifier))
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
					const type_info literal_type = { spv::OpTypeInt, 32, 1, 1, false };

					exp.reset_to_rvalue_constant(literal_type, location, value.second);

					return true;
				}
			}

			restore();
		}

		return parse_expression_multary(_temporary, exp);
	}
	bool parser::parse_technique_pass_expression(access_chain &exp)
	{
		scope scope;
		bool exclusive;

		if (accept(tokenid::colon_colon))
		{
			scope.namespace_level = scope.level = 0;
			exclusive = true;
		}
		else
		{
			scope = _symbol_table->current_scope();
			exclusive = false;
		}

		if (exclusive ? expect(tokenid::identifier) : accept(tokenid::identifier))
		{
			const std::pair<const char *, unsigned int> s_enums[] = {
				{ "NONE", pass_properties::NONE },
				{ "ZERO", pass_properties::ZERO },
				{ "ONE", pass_properties::ONE },
				{ "SRCCOLOR", pass_properties::SRCCOLOR },
				{ "SRCALPHA", pass_properties::SRCALPHA },
				{ "INVSRCCOLOR", pass_properties::INVSRCCOLOR },
				{ "INVSRCALPHA", pass_properties::INVSRCALPHA },
				{ "DESTCOLOR", pass_properties::DESTCOLOR },
				{ "DESTALPHA", pass_properties::DESTALPHA },
				{ "INVDESTCOLOR", pass_properties::INVDESTCOLOR },
				{ "INVDESTALPHA", pass_properties::INVDESTALPHA },
				{ "ADD", pass_properties::ADD },
				{ "SUBTRACT", pass_properties::SUBTRACT },
				{ "REVSUBTRACT", pass_properties::REVSUBTRACT },
				{ "MIN", pass_properties::MIN },
				{ "MAX", pass_properties::MAX },
				{ "KEEP", pass_properties::KEEP },
				{ "REPLACE", pass_properties::REPLACE },
				{ "INVERT", pass_properties::INVERT },
				{ "INCR", pass_properties::INCR },
				{ "INCRSAT", pass_properties::INCRSAT },
				{ "DECR", pass_properties::DECR },
				{ "DECRSAT", pass_properties::DECRSAT },
				{ "NEVER", pass_properties::NEVER },
				{ "ALWAYS", pass_properties::ALWAYS },
				{ "LESS", pass_properties::LESS },
				{ "GREATER", pass_properties::GREATER },
				{ "LEQUAL", pass_properties::LESSEQUAL },
				{ "LESSEQUAL", pass_properties::LESSEQUAL },
				{ "GEQUAL", pass_properties::GREATEREQUAL },
				{ "GREATEREQUAL", pass_properties::GREATEREQUAL },
				{ "EQUAL", pass_properties::EQUAL },
				{ "NEQUAL", pass_properties::NOTEQUAL },
				{ "NOTEQUAL", pass_properties::NOTEQUAL },
			};

			auto identifier = _token.literal_as_string;
			const auto location = _token.location;
			std::transform(_token.literal_as_string.begin(), _token.literal_as_string.end(), _token.literal_as_string.begin(), ::toupper);

			for (const auto &value : s_enums)
			{
				if (value.first == _token.literal_as_string)
				{
					const type_info literal_type = { spv::OpTypeInt, 32, 1, 1, false };

					exp.reset_to_rvalue_constant(literal_type, location, value.second);

					return true;
				}
			}

			while (accept(tokenid::colon_colon) && expect(tokenid::identifier))
				identifier += "::" + _token.literal_as_string;

			const auto symbol = _symbol_table->find(identifier, scope, exclusive);

			if (!symbol.id)
				return error(location, 3004, "undeclared identifier '" + identifier + "'"), false;

			exp.reset_to_rvalue(symbol.id, symbol.type, location);

			return true;
		}

		return parse_expression_multary(_temporary, exp);
	}
}
