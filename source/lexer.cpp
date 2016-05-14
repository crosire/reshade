#include "lexer.hpp"

namespace reshadefx
{
	namespace
	{
		enum token_type
		{
			DIGIT = '0',
			IDENT = 'A',
			SPACE = ' ',
		};
		struct keyword_type
		{
			const char *data;
			unsigned char length;
			lexer::tokenid id;
		};

		const unsigned type_lookup[256] = {
			 0xFF,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00, SPACE,
			 '\n', SPACE, SPACE, SPACE,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
			 0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,  0x00,
			 0x00,  0x00, SPACE,   '!',   '"',   '#',   '$',   '%',   '&',  '\'',
			  '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/', DIGIT, DIGIT,
			DIGIT, DIGIT, DIGIT, DIGIT, DIGIT, DIGIT, DIGIT, DIGIT,   ':',   ';',
			 '<',    '=',   '>',   '?',   '@', IDENT, IDENT, IDENT, IDENT, IDENT,
			IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT,
			IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT,
			IDENT,   '[',  '\\',   ']',   '^', IDENT,  0x00, IDENT, IDENT, IDENT,
			IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT,
			IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT, IDENT,
			IDENT, IDENT, IDENT,   '{',   '|',   '}',   '~',  0x00,  0x00,  0x00,
		};
		const keyword_type keyword_lookup[] = {
			{ "asm", 3, lexer::tokenid::reserved },
			{ "asm_fragment", 12, lexer::tokenid::reserved },
			{ "auto", 4, lexer::tokenid::reserved },
			{ "bool", 4, lexer::tokenid::bool_ },
			{ "bool2", 5, lexer::tokenid::bool2 },
			{ "bool2x2", 7, lexer::tokenid::bool2x2 },
			{ "bool3", 5, lexer::tokenid::bool3 },
			{ "bool3x3", 7, lexer::tokenid::bool3x3 },
			{ "bool4", 5, lexer::tokenid::bool4 },
			{ "bool4x4", 7, lexer::tokenid::bool4x4 },
			{ "break", 5, lexer::tokenid::break_ },
			{ "case", 4, lexer::tokenid::case_ },
			{ "cast", 4, lexer::tokenid::reserved },
			{ "catch", 5, lexer::tokenid::reserved },
			{ "centroid", 8, lexer::tokenid::reserved },
			{ "char", 4, lexer::tokenid::reserved },
			{ "class", 5, lexer::tokenid::reserved },
			{ "column_major", 12, lexer::tokenid::reserved },
			{ "compile", 7, lexer::tokenid::reserved },
			{ "const", 5, lexer::tokenid::const_ },
			{ "const_cast", 10, lexer::tokenid::reserved },
			{ "continue", 8, lexer::tokenid::continue_ },
			{ "default", 7, lexer::tokenid::default_ },
			{ "delete", 6, lexer::tokenid::reserved },
			{ "discard", 7, lexer::tokenid::discard_ },
			{ "do", 2, lexer::tokenid::do_ },
			{ "double", 6, lexer::tokenid::reserved },
			{ "dword", 5, lexer::tokenid::uint_ },
			{ "dword2", 6, lexer::tokenid::uint2 },
			{ "dword2x2", 8, lexer::tokenid::uint2x2 },
			{ "dword3", 6, lexer::tokenid::uint3, },
			{ "dword3x3", 8, lexer::tokenid::uint3x3 },
			{ "dword4", 6, lexer::tokenid::uint4 },
			{ "dword4x4", 8, lexer::tokenid::uint4x4 },
			{ "dynamic_cast", 12, lexer::tokenid::reserved },
			{ "else", 4, lexer::tokenid::else_ },
			{ "enum", 4, lexer::tokenid::reserved },
			{ "explicit", 8, lexer::tokenid::reserved },
			{ "extern", 6, lexer::tokenid::extern_ },
			{ "external", 8, lexer::tokenid::reserved },
			{ "false", 5, lexer::tokenid::false_literal },
			{ "FALSE", 5, lexer::tokenid::false_literal },
			{ "float", 5, lexer::tokenid::float_ },
			{ "float2", 6, lexer::tokenid::float2 },
			{ "float2x2", 8, lexer::tokenid::float2x2 },
			{ "float3", 6, lexer::tokenid::float3 },
			{ "float3x3", 8, lexer::tokenid::float3x3 },
			{ "float4", 6, lexer::tokenid::float4 },
			{ "float4x4", 8, lexer::tokenid::float4x4 },
			{ "for", 3, lexer::tokenid::for_ },
			{ "foreach", 7, lexer::tokenid::reserved },
			{ "friend", 6, lexer::tokenid::reserved },
			{ "globallycoherent", 16, lexer::tokenid::reserved },
			{ "goto", 4, lexer::tokenid::reserved },
			{ "groupshared", 11, lexer::tokenid::reserved },
			{ "half", 4, lexer::tokenid::reserved },
			{ "half2", 5, lexer::tokenid::reserved },
			{ "half2x2", 7, lexer::tokenid::reserved },
			{ "half3", 5, lexer::tokenid::reserved },
			{ "half3x3", 7, lexer::tokenid::reserved },
			{ "half4", 5, lexer::tokenid::reserved },
			{ "half4x4", 7, lexer::tokenid::reserved },
			{ "if", 2, lexer::tokenid::if_ },
			{ "in", 2, lexer::tokenid::in },
			{ "inline", 6, lexer::tokenid::reserved },
			{ "inout", 5, lexer::tokenid::inout },
			{ "int", 3, lexer::tokenid::int_ },
			{ "int2", 4, lexer::tokenid::int2 },
			{ "int2x2", 6, lexer::tokenid::int2x2 },
			{ "int3", 4, lexer::tokenid::int3 },
			{ "int3x3", 6, lexer::tokenid::int3x3 },
			{ "int4", 4, lexer::tokenid::int4 },
			{ "int4x4", 6, lexer::tokenid::int4x4 },
			{ "interface", 9, lexer::tokenid::reserved },
			{ "linear", 6, lexer::tokenid::linear },
			{ "long", 4, lexer::tokenid::reserved },
			{ "matrix", 6, lexer::tokenid::matrix },
			{ "mutable", 7, lexer::tokenid::reserved },
			{ "namespace", 9, lexer::tokenid::namespace_ },
			{ "new", 3, lexer::tokenid::reserved },
			{ "noinline", 8, lexer::tokenid::reserved },
			{ "nointerpolation", 15, lexer::tokenid::nointerpolation },
			{ "noperspective", 13, lexer::tokenid::noperspective },
			{ "operator", 8, lexer::tokenid::reserved },
			{ "out", 3, lexer::tokenid::out },
			{ "packed", 6, lexer::tokenid::reserved },
			{ "packoffset", 10, lexer::tokenid::reserved },
			{ "pass", 4, lexer::tokenid::pass },
			{ "precise", 7, lexer::tokenid::precise },
			{ "private", 7, lexer::tokenid::reserved },
			{ "protected", 9, lexer::tokenid::reserved },
			{ "public", 6, lexer::tokenid::reserved },
			{ "register", 8, lexer::tokenid::reserved },
			{ "reinterpret_cast", 16, lexer::tokenid::reserved },
			{ "return", 6, lexer::tokenid::return_ },
			{ "row_major", 9, lexer::tokenid::reserved },
			{ "sample", 6, lexer::tokenid::reserved },
			{ "sampler", 7, lexer::tokenid::sampler },
			{ "sampler1D", 9, lexer::tokenid::sampler },
			{ "sampler1DArray", 14, lexer::tokenid::reserved },
			{ "sampler1DArrayShadow", 20, lexer::tokenid::reserved },
			{ "sampler1DShadow", 15, lexer::tokenid::reserved },
			{ "sampler2D", 9, lexer::tokenid::sampler },
			{ "sampler2DArray", 14, lexer::tokenid::reserved },
			{ "sampler2DArrayShadow", 20, lexer::tokenid::reserved },
			{ "sampler2DMS", 11, lexer::tokenid::reserved },
			{ "sampler2DMSArray", 16, lexer::tokenid::reserved },
			{ "sampler2DShadow", 15, lexer::tokenid::reserved },
			{ "sampler3D", 9, lexer::tokenid::sampler },
			{ "sampler_state", 13, lexer::tokenid::reserved },
			{ "samplerCUBE", 11, lexer::tokenid::reserved },
			{ "samplerRECT", 11, lexer::tokenid::reserved },
			{ "SamplerState", 12, lexer::tokenid::reserved },
			{ "shared", 6, lexer::tokenid::reserved },
			{ "short", 5, lexer::tokenid::reserved },
			{ "signed", 6, lexer::tokenid::reserved },
			{ "sizeof", 6, lexer::tokenid::reserved },
			{ "snorm", 5, lexer::tokenid::reserved },
			{ "static", 6, lexer::tokenid::static_ },
			{ "static_cast", 11, lexer::tokenid::reserved },
			{ "string", 6, lexer::tokenid::string_ },
			{ "struct", 6, lexer::tokenid::struct_ },
			{ "switch", 6, lexer::tokenid::switch_ },
			{ "technique", 9, lexer::tokenid::technique },
			{ "template", 8, lexer::tokenid::reserved },
			{ "texture", 7, lexer::tokenid::texture },
			{ "Texture1D", 9, lexer::tokenid::reserved },
			{ "texture1D", 9, lexer::tokenid::texture },
			{ "Texture1DArray", 14, lexer::tokenid::reserved },
			{ "Texture2D", 9, lexer::tokenid::reserved },
			{ "texture2D", 9, lexer::tokenid::texture },
			{ "Texture2DArray", 14, lexer::tokenid::reserved },
			{ "Texture2DMS", 11, lexer::tokenid::reserved },
			{ "Texture2DMSArray", 16, lexer::tokenid::reserved },
			{ "Texture3D", 9, lexer::tokenid::reserved },
			{ "texture3D", 9, lexer::tokenid::texture },
			{ "textureCUBE", 11, lexer::tokenid::reserved },
			{ "TextureCube", 11, lexer::tokenid::reserved },
			{ "TextureCubeArray", 16, lexer::tokenid::reserved },
			{ "textureRECT", 11, lexer::tokenid::reserved },
			{ "this", 4, lexer::tokenid::reserved },
			{ "true", 4, lexer::tokenid::true_literal },
			{ "TRUE", 4, lexer::tokenid::true_literal },
			{ "try", 3, lexer::tokenid::reserved },
			{ "typedef", 7, lexer::tokenid::reserved },
			{ "uint", 4, lexer::tokenid::uint_ },
			{ "uint2", 5, lexer::tokenid::uint2 },
			{ "uint2x2", 7, lexer::tokenid::uint2x2 },
			{ "uint3", 5, lexer::tokenid::uint3 },
			{ "uint3x3", 7, lexer::tokenid::uint3x3 },
			{ "uint4", 5, lexer::tokenid::uint4 },
			{ "uint4x4", 7, lexer::tokenid::uint4x4 },
			{ "uniform", 7, lexer::tokenid::uniform_ },
			{ "union", 5, lexer::tokenid::reserved },
			{ "unorm", 5, lexer::tokenid::reserved },
			{ "unsigned", 8, lexer::tokenid::reserved },
			{ "vector", 6, lexer::tokenid::vector },
			{ "virtual", 7, lexer::tokenid::reserved },
			{ "void", 4, lexer::tokenid::void_ },
			{ "volatile", 8, lexer::tokenid::volatile_ },
			{ "while", 5, lexer::tokenid::while_ }
		};

		bool is_octal_digit(char c)
		{
			return static_cast<unsigned>(c - '0') < 8;
		}
		bool is_hexadecimal_digit(char c)
		{
			return static_cast<unsigned>(c - '0') < 10 || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}
		long long octal_to_decimal(long long n)
		{
			long long m = 0;

			while (n != 0)
			{
				m *= 8;
				m += n & 7;
				n >>= 3;
			}

			while (m != 0)
			{
				n *= 10;
				n += m & 7;
				m >>= 3;
			}

			return n;
		}
	}

	lexer::lexer(const lexer &lexer) :
		_input(lexer._input),
		_cur_location(lexer._cur_location),
		_ignore_whitespace(lexer._ignore_whitespace),
		_ignore_pp_directives(lexer._ignore_pp_directives),
		_ignore_keywords(lexer._ignore_keywords),
		_escape_string_literals(lexer._escape_string_literals)
	{
		_cur = _input.data() + (lexer._cur - lexer._input.data());
		_end = _input.data() + _input.size();
	}
	lexer::lexer(const std::string &source, bool ignore_whitespace, bool ignore_pp_directives, bool ignore_keywords, bool escape_string_literals) :
		_input(source),
		_ignore_whitespace(ignore_whitespace),
		_ignore_pp_directives(ignore_pp_directives),
		_ignore_keywords(ignore_keywords),
		_escape_string_literals(escape_string_literals)
	{
		_cur = _input.data();
		_end = _cur + _input.size();
	}

	lexer::token lexer::lex()
	{
		bool is_at_line_begin = _cur_location.column <= 1;
		token tok;
	next_token:
		tok.location = _cur_location;
		tok.offset = _cur - _input.data();
		tok.length = 1;
		tok.literal_as_double = 0;

		switch (type_lookup[*_cur])
		{
			case 0xFF:
				tok.id = tokenid::end_of_file;
				return tok;
			case SPACE:
				skip_space();
				if (_ignore_whitespace || is_at_line_begin || *_cur == '\n')
					goto next_token;
				tok.id = tokenid::space;
				return tok;
			case '\n':
				_cur++;
				_cur_location.line++;
				_cur_location.column = 1;
				is_at_line_begin = true;
				if (_ignore_whitespace)
					goto next_token;
				tok.id = tokenid::end_of_line;
				return tok;
			case DIGIT:
				parse_numeric_literal(tok);
				break;
			case IDENT:
				parse_identifier(tok);
				break;
			case '!':
				if (_cur[1] == '=')
					tok.id = tokenid::exclaim_equal,
					tok.length = 2;
				else
					tok.id = tokenid::exclaim;
				break;
			case '"':
				parse_string_literal(tok, _escape_string_literals);
				break;
			case '#':
				if (is_at_line_begin)
				{
					if (!parse_pp_directive(tok) || _ignore_pp_directives)
					{
						skip_to_next_line();
						goto next_token;
					}
				}
				else
					tok.id = tokenid::hash;
				break;
			case '$':
				tok.id = tokenid::dollar;
				break;
			case '%':
				if (_cur[1] == '=')
					tok.id = tokenid::percent_equal,
					tok.length = 2;
				else
					tok.id = tokenid::percent;
				break;
			case '&':
				if (_cur[1] == '&')
					tok.id = tokenid::ampersand_ampersand,
					tok.length = 2;
				else if (_cur[1] == '=')
					tok.id = tokenid::ampersand_equal,
					tok.length = 2;
				else
					tok.id = tokenid::ampersand;
				break;
			case '(':
				tok.id = tokenid::parenthesis_open;
				break;
			case ')':
				tok.id = tokenid::parenthesis_close;
				break;
			case '*':
				if (_cur[1] == '=')
					tok.id = tokenid::star_equal,
					tok.length = 2;
				else
					tok.id = tokenid::star;
				break;
			case '+':
				if (_cur[1] == '+')
					tok.id = tokenid::plus_plus,
					tok.length = 2;
				else if (_cur[1] == '=')
					tok.id = tokenid::plus_equal,
					tok.length = 2;
				else
					tok.id = tokenid::plus;
				break;
			case ',':
				tok.id = tokenid::comma;
				break;
			case '-':
				if (_cur[1] == '-')
					tok.id = tokenid::minus_minus,
					tok.length = 2;
				else if (_cur[1] == '=')
					tok.id = tokenid::minus_equal,
					tok.length = 2;
				else if (_cur[1] == '>')
					tok.id = tokenid::arrow,
					tok.length = 2;
				else
					tok.id = tokenid::minus;
				break;
			case '.':
				if (type_lookup[_cur[1]] == DIGIT)
					parse_numeric_literal(tok);
				else if (_cur[1] == '.' && _cur[2] == '.')
					tok.id = tokenid::ellipsis,
					tok.length = 3;
				else
					tok.id = tokenid::dot;
				break;
			case '/':
				if (_cur[1] == '/')
				{
					skip_to_next_line();
					goto next_token;
				}
				else if (_cur[1] == '*')
				{
					while (_cur < _end)
					{
						if (*_cur == '\n')
						{
							_cur_location.line++;
							_cur_location.column = 1;
						}
						else if (_cur[0] == '*' && _cur[1] == '/')
						{
							skip(2);
							break;
						}
						skip(1);
					}
					goto next_token;
				}
				else if (_cur[1] == '=')
					tok.id = tokenid::slash_equal,
					tok.length = 2;
				else
					tok.id = tokenid::slash;
				break;
			case ':':
				if (_cur[1] == ':')
					tok.id = tokenid::colon_colon,
					tok.length = 2;
				else
					tok.id = tokenid::colon;
				break;
			case ';':
				tok.id = tokenid::semicolon;
				break;
			case '<':
				if (_cur[1] == '<')
					if (_cur[2] == '=')
						tok.id = tokenid::less_less_equal,
						tok.length = 3;
					else
						tok.id = tokenid::less_less,
						tok.length = 2;
				else if (_cur[1] == '=')
					tok.id = tokenid::less_equal,
					tok.length = 2;
				else
					tok.id = tokenid::less;
				break;
			case '=':
				if (_cur[1] == '=')
					tok.id = tokenid::equal_equal,
					tok.length = 2;
				else
					tok.id = tokenid::equal;
				break;
			case '>':
				if (_cur[1] == '>')
					if (_cur[2] == '=')
						tok.id = tokenid::greater_greater_equal,
						tok.length = 3;
					else
						tok.id = tokenid::greater_greater,
						tok.length = 2;
				else if (_cur[1] == '=')
					tok.id = tokenid::greater_equal,
					tok.length = 2;
				else
					tok.id = tokenid::greater;
				break;
			case '?':
				tok.id = tokenid::question;
				break;
			case '@':
				tok.id = tokenid::at;
				break;
			case '[':
				tok.id = tokenid::bracket_open;
				break;
			case '\\':
				tok.id = tokenid::backslash;
				break;
			case ']':
				tok.id = tokenid::bracket_close;
				break;
			case '^':
				if (_cur[1] == '=')
					tok.id = tokenid::caret_equal,
					tok.length = 2;
				else
					tok.id = tokenid::caret;
				break;
			case '{':
				tok.id = tokenid::brace_open;
				break;
			case '|':
				if (_cur[1] == '=')
					tok.id = tokenid::pipe_equal,
					tok.length = 2;
				else if (_cur[1] == '|')
					tok.id = tokenid::pipe_pipe,
					tok.length = 2;
				else
					tok.id = tokenid::pipe;
				break;
			case '}':
				tok.id = tokenid::brace_close;
				break;
			case '~':
				tok.id = tokenid::tilde;
				break;
			default:
				tok.id = tokenid::unknown;
				break;
		}

		skip(tok.length);

		return tok;
	}

	void lexer::skip(size_t length)
	{
		_cur += length;
		_cur_location.column += length;
	}
	void lexer::skip_space()
	{
		while (type_lookup[*_cur] == SPACE && _cur < _end)
		{
			skip(1);
		}
	}
	void lexer::skip_to_next_line()
	{
		while (*_cur != '\n' && _cur < _end)
		{
			skip(1);
		}
	}

	void lexer::parse_identifier(token &tok) const
	{
		const char *const begin = _cur, *end = begin;

		do end++; while (type_lookup[*end] == IDENT || type_lookup[*end] == DIGIT);

		tok.id = tokenid::identifier;
		tok.length = end - begin;
		tok.literal_as_string.assign(begin, end);

		if (_ignore_keywords)
		{
			return;
		}

		for (size_t i = 0; i < _countof(keyword_lookup); i++)
		{
			if (keyword_lookup[i].length == tok.length && strncmp(begin, keyword_lookup[i].data, tok.length) == 0)
			{
				tok.id = keyword_lookup[i].id;
				break;
			}
		}
	}
	bool lexer::parse_pp_directive(token &tok)
	{
		skip(1);
		skip_space();
		parse_identifier(tok);

		if (tok.literal_as_string == "line")
		{
			skip(tok.length);
			skip_space();
			parse_numeric_literal(tok);
			skip(tok.length);

			_cur_location.line = tok.literal_as_int;

			if (_cur_location.line != 0)
			{
				_cur_location.line--;
			}

			skip_space();

			if (_cur[0] == '"')
			{
				token temptok;
				parse_string_literal(temptok, false);

				_cur_location.source = temptok.literal_as_string;
			}

			return false;
		}
		else if (tok.literal_as_string == "define")
		{
			tok.id = tokenid::hash_def;
		}
		else if (tok.literal_as_string == "undef")
		{
			tok.id = tokenid::hash_undef;
		}
		else if (tok.literal_as_string == "if")
		{
			tok.id = tokenid::hash_if;
		}
		else if (tok.literal_as_string == "ifdef")
		{
			tok.id = tokenid::hash_ifdef;
		}
		else if (tok.literal_as_string == "ifndef")
		{
			tok.id = tokenid::hash_ifndef;
		}
		else if (tok.literal_as_string == "else")
		{
			tok.id = tokenid::hash_else;
		}
		else if (tok.literal_as_string == "elif")
		{
			tok.id = tokenid::hash_elif;
		}
		else if (tok.literal_as_string == "endif")
		{
			tok.id = tokenid::hash_endif;
		}
		else if (tok.literal_as_string == "error")
		{
			tok.id = tokenid::hash_error;
		}
		else if (tok.literal_as_string == "warning")
		{
			tok.id = tokenid::hash_warning;
		}
		else if (tok.literal_as_string == "pragma")
		{
			tok.id = tokenid::hash_pragma;
		}
		else if (tok.literal_as_string == "include")
		{
			tok.id = tokenid::hash_include;
		}
		else
		{
			tok.id = tokenid::hash_unknown;
		}

		return true;
	}
	void lexer::parse_string_literal(token &tok, bool escape) const
	{
		const char *const begin = _cur, *end = begin + 1;

		for (char c = *end; c != '"'; c = *++end)
		{
			if (c == '\n' || end >= _end)
			{
				end--;
				break;
			}
			if (c == '\\' && end[1] == '\n')
			{
				end++;
				continue;
			}

			if (c == '\\' && escape)
			{
				unsigned int n = 0;

				switch (c = *++end)
				{
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
						for (unsigned int i = 0; i < 3 && is_octal_digit(*end) && end < _end; i++)
						{
							c = *end++;
							n = (n << 3) | (c - '0');
						}
						c = static_cast<char>(n & 0xFF);
						end--;
						break;
					case 'a':
						c = '\a';
						break;
					case 'b':
						c = '\b';
						break;
					case 'f':
						c = '\f';
						break;
					case 'n':
						c = '\n';
						break;
					case 'r':
						c = '\r';
						break;
					case 't':
						c = '\t';
						break;
					case 'v':
						c = '\v';
						break;
					case 'x':
						if (is_hexadecimal_digit(*++end))
						{
							while (is_hexadecimal_digit(*end) && end < _end)
							{
								c = *end++;
								n = (n << 4) | (static_cast<unsigned>(c -= '0') < 10 ? c : (c + '0' - 55 - 32 * (c & 0x20)));
							}

							c = static_cast<char>(n);
						}
						end--;
						break;
				}
			}

			tok.literal_as_string += c;
		}

		tok.id = tokenid::string_literal;
		tok.length = end - begin + 1;
	}
	void lexer::parse_numeric_literal(token &tok) const
	{
		const char *const begin = _cur, *end = _cur;
		int mantissa_size = 0, decimal_location = -1, radix = 10;
		long long fraction = 0, exponent = 0;

		if (begin[0] == '0')
		{
			if (begin[1] == 'x' || begin[1] == 'X')
			{
				end = begin + 2;
				radix = 16;
			}
			else
			{
				radix = 8;
			}
		}

		for (; mantissa_size <= 18; mantissa_size++, end++)
		{
			int c = *end;

			if (c >= '0' && c <= '9')
			{
				c -= '0';

				if (c >= radix)
				{
					break;
				}
			}
			else if (radix == 16)
			{
				if (c >= 'A' && c <= 'F')
				{
					c -= 'A' - 10;
				}
				else if (c >= 'a' && c <= 'f')
				{
					c -= 'a' - 10;
				}
				else
				{
					break;
				}
			}
			else
			{
				if (c != '.' || decimal_location >= 0)
				{
					break;
				}

				if (radix == 8)
				{
					radix = 10;
					fraction = octal_to_decimal(fraction);
				}

				decimal_location = mantissa_size;
				continue;
			}

			fraction *= radix;
			fraction += c;
		}

		while (is_hexadecimal_digit(*end))
		{
			// Ignore additional digits that cannot affect the value
			end++;
		}

		if (decimal_location < 0)
		{
			tok.id = tokenid::int_literal;

			decimal_location = mantissa_size;
		}
		else
		{
			tok.id = tokenid::float_literal;

			mantissa_size -= 1;
		}

		if (*end == 'E' || *end == 'e')
		{
			auto tmp = end + 1;
			const bool negative = *tmp == '-';

			if (negative || *tmp == '+')
			{
				tmp++;
			}

			if (*tmp >= '0' && *tmp <= '9')
			{
				end = tmp;

				tok.id = tokenid::float_literal;

				do
				{
					exponent *= 10;
					exponent += *end++ - '0';
				}
				while (*end >= '0' && *end <= '9');

				if (negative)
				{
					exponent = -exponent;
				}
			}
		}

		if (*end == 'F' || *end == 'f')
		{
			end++;

			tok.id = tokenid::float_literal;
		}
		else if (*end == 'L' || *end == 'l')
		{
			end++;

			tok.id = tokenid::double_literal;
		}
		else if (tok.id == tokenid::int_literal && (*end == 'U' || *end == 'u'))
		{
			end++;

			tok.id = tokenid::uint_literal;
		}

		if (tok.id == tokenid::float_literal || tok.id == tokenid::double_literal)
		{
			exponent += decimal_location - mantissa_size;

			const bool exponent_negative = exponent < 0;

			if (exponent_negative)
			{
				exponent = -exponent;
			}

			if (exponent > 511)
			{
				exponent = 511;
			}

			double e = 1.0;
			const double powers_of_10[] = {
				10.,
				100.,
				1.0e4,
				1.0e8,
				1.0e16,
				1.0e32,
				1.0e64,
				1.0e128,
				1.0e256
			};

			for (auto d = powers_of_10; exponent != 0; exponent >>= 1, d++)
			{
				if (exponent & 1)
				{
					e *= *d;
				}
			}

			if (tok.id == tokenid::float_literal)
			{
				tok.literal_as_float = exponent_negative ? fraction / static_cast<float>(e) : fraction * static_cast<float>(e);
			}
			else
			{
				tok.literal_as_double = exponent_negative ? fraction / e : fraction * e;
			}
		}
		else
		{
			tok.literal_as_uint = static_cast<unsigned int>(fraction & 0xFFFFFFFF);
		}

		tok.length = end - begin;
	}
}
