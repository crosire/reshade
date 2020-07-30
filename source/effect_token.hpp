/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>
#include <vector>

namespace reshadefx
{
	/// <summary>
	/// Structure which keeps track of a code location
	/// </summary>
	struct location
	{
		location() : line(1), column(1) {}
		explicit location(unsigned int line, unsigned int column = 1) : line(line), column(column) {}
		explicit location(std::string source, unsigned int line, unsigned int column = 1) : source(std::move(source)), line(line), column(column) {}

		std::string source;
		unsigned int line, column;
	};

	/// <summary>
	/// A collection of identifiers for various possible tokens.
	/// </summary>
	enum class tokenid
	{
		unknown = -1,
		end_of_file = 0,
		end_of_line = '\n',

		// operators
		space = ' ',
		exclaim = '!',
		hash = '#',
		dollar = '$',
		percent = '%',
		ampersand = '&',
		parenthesis_open = '(',
		parenthesis_close = ')',
		star = '*',
		plus = '+',
		comma = ',',
		minus = '-',
		dot = '.',
		slash = '/',
		colon = ':',
		semicolon = ';',
		less = '<',
		equal = '=',
		greater = '>',
		question = '?',
		at = '@',
		bracket_open = '[',
		backslash = '\\',
		bracket_close = ']',
		caret = '^',
		brace_open = '{',
		pipe = '|',
		brace_close = '}',
		tilde = '~',
		exclaim_equal = 256 /* != */,
		percent_equal /* %= */,
		ampersand_ampersand /* && */,
		ampersand_equal /* &= */,
		star_equal /* *= */,
		plus_plus /* ++*/,
		plus_equal /* += */,
		minus_minus /* -- */,
		minus_equal /* -= */,
		arrow /* -> */,
		ellipsis /* ... */,
		slash_equal /* /= */,
		colon_colon /* :: */,
		less_less_equal /* <<= */,
		less_less /* << */,
		less_equal /* <= */,
		equal_equal /* == */,
		greater_greater_equal /* >>= */,
		greater_greater /* >> */,
		greater_equal /* >= */,
		caret_equal /* ^= */,
		pipe_equal /* |= */,
		pipe_pipe /* || */,

		// identifiers
		reserved,
		identifier,

		// literals
		true_literal,
		false_literal,
		int_literal,
		uint_literal,
		float_literal,
		double_literal,
		string_literal,

		// keywords
		namespace_,
		struct_,
		technique,
		pass,
		for_,
		while_,
		do_,
		if_,
		else_,
		switch_,
		case_,
		default_,
		break_,
		continue_,
		return_,
		discard_,
		extern_,
		static_,
		uniform_,
		volatile_,
		precise,
		groupshared,
		in,
		out,
		inout,
		const_,
		linear,
		noperspective,
		centroid,
		nointerpolation,

		void_,
		bool_,
		bool2,
		bool3,
		bool4,
		bool2x2,
		bool3x3,
		bool4x4,
		int_,
		int2,
		int3,
		int4,
		int2x2,
		int3x3,
		int4x4,
		uint_,
		uint2,
		uint3,
		uint4,
		uint2x2,
		uint3x3,
		uint4x4,
		float_,
		float2,
		float3,
		float4,
		float2x2,
		float3x3,
		float4x4,
		vector,
		matrix,
		string_,
		texture,
		sampler,
		storage,

		// preprocessor directives
		hash_def,
		hash_undef,
		hash_if,
		hash_ifdef,
		hash_ifndef,
		hash_else,
		hash_elif,
		hash_endif,
		hash_error,
		hash_warning,
		hash_pragma,
		hash_include,
		hash_unknown,

		single_line_comment,
		multi_line_comment,
	};

	/// <summary>
	/// A structure describing a single token in the input string.
	/// </summary>
	struct token
	{
		tokenid id;
		reshadefx::location location;
		size_t offset, length;
		union
		{
			int literal_as_int;
			unsigned int literal_as_uint;
			float literal_as_float;
			double literal_as_double;
		};
		std::string literal_as_string;

		inline operator tokenid() const { return id; }

		static std::string id_to_name(tokenid id);
	};
}
