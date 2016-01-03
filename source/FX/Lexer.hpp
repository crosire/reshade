#pragma once

#include <string>

namespace ReShade
{
	namespace FX
	{
		struct location
		{
			explicit location(unsigned int line = 1, unsigned int column = 1) : line(line), column(column)
			{
			}
			explicit location(const std::string &source, unsigned int line = 1, unsigned int column = 1) : source(source), line(line), column(column)
			{
			}

			std::string source;
			unsigned int line, column;
		};

		class lexer
		{
			lexer &operator=(const lexer &);

		public:
			enum class tokenid
			{
				unknown = -1,
				end_of_stream = 0,

				// operators
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
				in,
				out,
				inout,
				const_,
				linear,
				noperspective,
				centroid,
				nointerpolation,

				// types
				void_,
				bool_,
				bool2,
				bool2x2,
				bool3,
				bool3x3,
				bool4,
				bool4x4,
				int_,
				int2,
				int2x2,
				int3,
				int3x3,
				int4,
				int4x4,
				uint_,
				uint2,
				uint2x2,
				uint3,
				uint3x3,
				uint4,
				uint4x4,
				float_,
				float2,
				float2x2,
				float3,
				float3x3,
				float4,
				float4x4,
				vector,
				matrix,
				string_,
				texture1d,
				texture2d,
				texture3d,
				sampler1d,
				sampler2d,
				sampler3d,

				// preprocessor directives
				pp_include,
				pp_line,
				pp_define,
				pp_undef,
				pp_if,
				pp_ifdef,
				pp_ifndef,
				pp_else,
				pp_elif,
				pp_endif,
				pp_error,
				pp_warning,
				pp_pragma,
				pp_unknown,
			};
			struct token
			{
				tokenid id;
				location location;
				size_t length;
				union
				{
					int literal_as_int;
					unsigned int literal_as_uint;
					float literal_as_float;
					double literal_as_double;
				};
				std::string literal_as_string;
			};

			/// <summary>
			/// Construct a copy of an existing instance.
			/// </summary>
			/// <param name="lexer">The instance to copy.</param>
			lexer(const lexer &lexer);
			/// <summary>
			/// Construct a new lexical analyzer for an input <paramref name="source"/> string.
			/// </summary>
			/// <param name="source">The string to analyze.</param>
			explicit lexer(const std::string &source);

			/// <summary>
			/// Perform lexical analysis on the input string and return the next token in sequence.
			/// </summary>
			/// <returns>The next token in sequence.</returns>
			token lex(bool skip_pp_directives = true);

			/// <summary>
			/// Advances to the next new line, ignoring all tokens.
			/// </summary>
			void skip_to_next_line();

		private:
			void skip(size_t length);
			void skip_space();

			void parse_identifier(token &tok) const;
			void parse_pp_directive(token &tok);
			void parse_string_literal(token &tok, bool escape) const;
			void parse_numeric_literal(token &tok) const;

			location _location;
			std::string _source;
			const std::string::value_type *_cur, *_end;
		};
	}
}
