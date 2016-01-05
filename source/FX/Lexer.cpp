#include "Lexer.hpp"

namespace ReShade
{
	namespace FX
	{
		namespace
		{
			bool isdigit(char c)
			{
				return static_cast<unsigned>(c - '0') < 10;
			}
			bool isodigit(char c)
			{
				return static_cast<unsigned>(c - '0') < 8;
			}
			bool isxdigit(char c)
			{
				return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
			}
			bool isalpha(char c)
			{
				return static_cast<unsigned>((c | 32) - 'a') < 26;
			}
			bool isalnum(char c)
			{
				return isalpha(c) || isdigit(c) || c == '_';
			}
		}

		lexer::lexer(const lexer &lexer) : _input(lexer._input), _location(lexer._location), _ignore_whitespace(lexer._ignore_whitespace), _ignore_pp_directives(lexer._ignore_pp_directives), _ignore_keywords(lexer._ignore_keywords), _escape_string_literals(lexer._escape_string_literals)
		{
			_cur = _input.data() + (lexer._cur - lexer._input.data());
			_end = _input.data() + _input.size();
		}
		lexer::lexer(const std::string &source, bool ignore_whitespace, bool ignore_pp_directives, bool ignore_keywords, bool escape_string_literals) : _input(source), _ignore_whitespace(ignore_whitespace), _ignore_pp_directives(ignore_pp_directives), _ignore_keywords(ignore_keywords), _escape_string_literals(escape_string_literals)
		{
			_cur = _input.data();
			_end = _cur + _input.size();
		}

		lexer::token lexer::lex()
		{
			bool is_at_line_begin = _location.column <= 1;
		next_token:
			token tok;
			tok.id = tokenid::unknown;
			tok.location = _location;
			tok.offset = _cur - _input.data();
			tok.length = 1;
			tok.literal_as_double = 0;

			switch (_cur[0])
			{
				case '\0':
					tok.id = tokenid::end_of_file;
					return tok;
				case '\t':
				case '\v':
				case '\f':
				case '\r':
				case ' ':
					skip_space();
					if (_ignore_whitespace || is_at_line_begin || _cur[0] == '\n')
						goto next_token;
					tok.id = tokenid::space;
					return tok;
				case '\n':
					_cur++;
					_location.line++;
					_location.column = 1;
					is_at_line_begin = true;
					if (_ignore_whitespace)
						goto next_token;
					tok.id = tokenid::end_of_line;
					return tok;
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
					if (isdigit(_cur[1]))
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
								_location.line++;
								_location.column = 1;
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
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					parse_numeric_literal(tok);
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
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				case 'G':
				case 'H':
				case 'I':
				case 'J':
				case 'K':
				case 'L':
				case 'M':
				case 'N':
				case 'O':
				case 'P':
				case 'Q':
				case 'R':
				case 'S':
				case 'T':
				case 'U':
				case 'V':
				case 'W':
				case 'X':
				case 'Y':
				case 'Z':
					parse_identifier(tok);
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
				case '_':
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				case 'g':
				case 'h':
				case 'i':
				case 'j':
				case 'k':
				case 'l':
				case 'm':
				case 'n':
				case 'o':
				case 'p':
				case 'q':
				case 'r':
				case 's':
				case 't':
				case 'u':
				case 'v':
				case 'w':
				case 'x':
				case 'y':
				case 'z':
					parse_identifier(tok);
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
			_location.column += length;
		}
		void lexer::skip_space()
		{
			while ((*_cur == ' ' || ((*_cur >= '\t' && *_cur <= '\r') && *_cur != '\n')) && _cur < _end)
			{
				skip(1);
			}
		}
		void lexer::skip_to_next_line()
		{
			while ((*_cur != '\n') && _cur < _end)
			{
				skip(1);
			}
		}

		void lexer::parse_identifier(token &tok) const
		{
			const char *const begin = _cur, *end = begin;

			while (isalnum(*++end))
			{
				continue;
			}

			tok.id = tokenid::identifier;
			tok.length = end - begin;
			tok.literal_as_string = std::string(begin, end);

			if (_ignore_keywords)
			{
				return;
			}

			#pragma region Keywords
			struct keyword
			{
				const char *data;
				unsigned char length;
				tokenid id;
			};

			static const keyword keywords[] =
			{
				{ "asm", 3, tokenid::reserved },
				{ "asm_fragment", 12, tokenid::reserved },
				{ "auto", 4, tokenid::reserved },
				{ "bool", 4, tokenid::bool_ },
				{ "bool2", 5, tokenid::bool2 },
				{ "bool2x2", 7, tokenid::bool2x2 },
				{ "bool3", 5, tokenid::bool3 },
				{ "bool3x3", 7, tokenid::bool3x3 },
				{ "bool4", 5, tokenid::bool4 },
				{ "bool4x4", 7, tokenid::bool4x4 },
				{ "break", 5, tokenid::break_ },
				{ "case", 4, tokenid::case_ },
				{ "cast", 4, tokenid::reserved },
				{ "catch", 5, tokenid::reserved },
				{ "centroid", 8, tokenid::reserved },
				{ "char", 4, tokenid::reserved },
				{ "class", 5, tokenid::reserved },
				{ "column_major", 12, tokenid::reserved },
				{ "compile", 7, tokenid::reserved },
				{ "const", 5, tokenid::const_ },
				{ "const_cast", 10, tokenid::reserved },
				{ "continue", 8, tokenid::continue_ },
				{ "default", 7, tokenid::default_ },
				{ "delete", 6, tokenid::reserved },
				{ "discard", 7, tokenid::discard_ },
				{ "do", 2, tokenid::do_ },
				{ "double", 6, tokenid::reserved },
				{ "dword", 5, tokenid::uint_ },
				{ "dword2", 6, tokenid::uint2 },
				{ "dword2x2", 8, tokenid::uint2x2 },
				{ "dword3", 6, tokenid::uint3, },
				{ "dword3x3", 8, tokenid::uint3x3 },
				{ "dword4", 6, tokenid::uint4 },
				{ "dword4x4", 8, tokenid::uint4x4 },
				{ "dynamic_cast", 12, tokenid::reserved },
				{ "else", 4, tokenid::else_ },
				{ "enum", 4, tokenid::reserved },
				{ "explicit", 8, tokenid::reserved },
				{ "extern", 6, tokenid::extern_ },
				{ "external", 8, tokenid::reserved },
				{ "false", 5, tokenid::false_literal },
				{ "FALSE", 5, tokenid::false_literal },
				{ "float", 5, tokenid::float_ },
				{ "float2", 6, tokenid::float2 },
				{ "float2x2", 8, tokenid::float2x2 },
				{ "float3", 6, tokenid::float3 },
				{ "float3x3", 8, tokenid::float3x3 },
				{ "float4", 6, tokenid::float4 },
				{ "float4x4", 8, tokenid::float4x4 },
				{ "for", 3, tokenid::for_ },
				{ "foreach", 7, tokenid::reserved },
				{ "friend", 6, tokenid::reserved },
				{ "globallycoherent", 16, tokenid::reserved },
				{ "goto", 4, tokenid::reserved },
				{ "groupshared", 11, tokenid::reserved },
				{ "half", 4, tokenid::reserved },
				{ "half2", 5, tokenid::reserved },
				{ "half2x2", 7, tokenid::reserved },
				{ "half3", 5, tokenid::reserved },
				{ "half3x3", 7, tokenid::reserved },
				{ "half4", 5, tokenid::reserved },
				{ "half4x4", 7, tokenid::reserved },
				{ "if", 2, tokenid::if_ },
				{ "in", 2, tokenid::in },
				{ "inline", 6, tokenid::reserved },
				{ "inout", 5, tokenid::inout },
				{ "int", 3, tokenid::int_ },
				{ "int2", 4, tokenid::int2 },
				{ "int2x2", 6, tokenid::int2x2 },
				{ "int3", 4, tokenid::int3 },
				{ "int3x3", 6, tokenid::int3x3 },
				{ "int4", 4, tokenid::int4 },
				{ "int4x4", 6, tokenid::int4x4 },
				{ "interface", 9, tokenid::reserved },
				{ "linear", 6, tokenid::linear },
				{ "long", 4, tokenid::reserved },
				{ "matrix", 6, tokenid::matrix },
				{ "mutable", 7, tokenid::reserved },
				{ "namespace", 9, tokenid::namespace_ },
				{ "new", 3, tokenid::reserved },
				{ "noinline", 8, tokenid::reserved },
				{ "nointerpolation", 15, tokenid::nointerpolation },
				{ "noperspective", 13, tokenid::noperspective },
				{ "operator", 8, tokenid::reserved },
				{ "out", 3, tokenid::out },
				{ "packed", 6, tokenid::reserved },
				{ "packoffset", 10, tokenid::reserved },
				{ "pass", 4, tokenid::pass },
				{ "precise", 7, tokenid::precise },
				{ "private", 7, tokenid::reserved },
				{ "protected", 9, tokenid::reserved },
				{ "public", 6, tokenid::reserved },
				{ "register", 8, tokenid::reserved },
				{ "reinterpret_cast", 16, tokenid::reserved },
				{ "return", 6, tokenid::return_ },
				{ "row_major", 9, tokenid::reserved },
				{ "sample", 6, tokenid::reserved },
				{ "sampler", 7, tokenid::sampler2d },
				{ "sampler1D", 9, tokenid::sampler1d },
				{ "sampler1DArray", 14, tokenid::reserved },
				{ "sampler1DArrayShadow", 20, tokenid::reserved },
				{ "sampler1DShadow", 15, tokenid::reserved },
				{ "sampler2D", 9, tokenid::sampler2d },
				{ "sampler2DArray", 14, tokenid::reserved },
				{ "sampler2DArrayShadow", 20, tokenid::reserved },
				{ "sampler2DMS", 11, tokenid::reserved },
				{ "sampler2DMSArray", 16, tokenid::reserved },
				{ "sampler2DShadow", 15, tokenid::reserved },
				{ "sampler3D", 9, tokenid::sampler3d },
				{ "sampler_state", 13, tokenid::reserved },
				{ "samplerCUBE", 11, tokenid::reserved },
				{ "samplerRECT", 11, tokenid::reserved },
				{ "SamplerState", 12, tokenid::reserved },
				{ "shared", 6, tokenid::reserved },
				{ "short", 5, tokenid::reserved },
				{ "signed", 6, tokenid::reserved },
				{ "sizeof", 6, tokenid::reserved },
				{ "snorm", 5, tokenid::reserved },
				{ "static", 6, tokenid::static_ },
				{ "static_cast", 11, tokenid::reserved },
				{ "string", 6, tokenid::string_ },
				{ "struct", 6, tokenid::struct_ },
				{ "switch", 6, tokenid::switch_ },
				{ "technique", 9, tokenid::technique },
				{ "template", 8, tokenid::reserved },
				{ "texture", 7, tokenid::texture2d },
				{ "Texture1D", 9, tokenid::reserved },
				{ "texture1D", 9, tokenid::texture1d },
				{ "Texture1DArray", 14, tokenid::reserved },
				{ "Texture2D", 9, tokenid::reserved },
				{ "texture2D", 9, tokenid::texture2d },
				{ "Texture2DArray", 14, tokenid::reserved },
				{ "Texture2DMS", 11, tokenid::reserved },
				{ "Texture2DMSArray", 16, tokenid::reserved },
				{ "Texture3D", 9, tokenid::reserved },
				{ "texture3D", 9, tokenid::texture3d },
				{ "textureCUBE", 11, tokenid::reserved },
				{ "TextureCube", 11, tokenid::reserved },
				{ "TextureCubeArray", 16, tokenid::reserved },
				{ "textureRECT", 11, tokenid::reserved },
				{ "this", 4, tokenid::reserved },
				{ "true", 4, tokenid::true_literal },
				{ "TRUE", 4, tokenid::true_literal },
				{ "try", 3, tokenid::reserved },
				{ "typedef", 7, tokenid::reserved },
				{ "uint", 4, tokenid::uint_ },
				{ "uint2", 5, tokenid::uint2 },
				{ "uint2x2", 7, tokenid::uint2x2 },
				{ "uint3", 5, tokenid::uint3 },
				{ "uint3x3", 7, tokenid::uint3x3 },
				{ "uint4", 5, tokenid::uint4 },
				{ "uint4x4", 7, tokenid::uint4x4 },
				{ "uniform", 7, tokenid::uniform_ },
				{ "union", 5, tokenid::reserved },
				{ "unorm", 5, tokenid::reserved },
				{ "unsigned", 8, tokenid::reserved },
				{ "vector", 6, tokenid::vector },
				{ "virtual", 7, tokenid::reserved },
				{ "void", 4, tokenid::void_ },
				{ "volatile", 8, tokenid::volatile_ },
				{ "while", 5, tokenid::while_ }
			};
			#pragma endregion

			for (size_t i = 0; i < sizeof(keywords) / sizeof(keyword); i++)
			{
				if (keywords[i].length == tok.length && strncmp(begin, keywords[i].data, tok.length) == 0)
				{
					tok.id = keywords[i].id;
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

				_location.line = tok.literal_as_int;

				if (_location.line != 0)
				{
					_location.line--;
				}

				skip_space();

				if (_cur[0] == '"')
				{
					token temptok;
					parse_string_literal(temptok, false);

					_location.source = temptok.literal_as_string;
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
			char c;
			const char *const begin = _cur, *end = begin;

			while ((c = *++end) != '"')
			{
				if (c == '\n' || c == '\r' || end >= _end)
				{
					end--;
					break;
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
							for (unsigned int i = 0; i < 3 && isodigit(*end) && end < _end; i++)
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
							if (isxdigit(*++end))
							{
								while (isxdigit(*end) && end < _end)
								{
									c = *end++;
									n = (n << 4) | (isdigit(c) ? c - '0' : (c - 55 - 32 * (c & 0x20)));
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
			int radix = 0;
			const char *begin = _cur, *end = begin;

			tok.id = tokenid::int_literal;

			if (begin[0] == '0')
			{
				if (begin[1] == 'x' || begin[1] == 'X')
				{
					radix = 16;
					end = begin + 1;

					while (isxdigit(*++end))
					{
						continue;
					}
				}
				else
				{
					radix = 8;

					while (isodigit(*++end))
					{
						continue;
					}

					if (isdigit(*end))
					{
						while (isdigit(*++end))
						{
							continue;
						}

						if (*end == '.' || *end == 'e' || *end == 'E' || *end == 'f' || *end == 'F')
						{
							radix = 10;
						}
					}
				}
			}
			else if (begin[0] != '.')
			{
				radix = 10;

				while (isdigit(*++end))
				{
					continue;
				}
			}

			if (radix != 16)
			{
				if (*end == '.')
				{
					radix = 10;
					tok.id = tokenid::float_literal;

					while (isdigit(*++end))
					{
						continue;
					}
				}

				if (end[0] == 'e' || end[0] == 'E')
				{
					if ((end[1] == '+' || end[1] == '-') && isdigit(end[2]))
					{
						end++;
					}

					if (isdigit(end[1]))
					{
						radix = 10;
						tok.id = tokenid::float_literal;

						while (isdigit(*++end))
						{
							continue;
						}
					}
				}
			}

			if (*end == 'f' || *end == 'F')
			{
				radix = 10;
				tok.id = tokenid::float_literal;

				end++;
			}
			else if (*end == 'l' || *end == 'L')
			{
				radix = 10;
				tok.id = tokenid::double_literal;

				end++;
			}
			else if (tok.id == tokenid::int_literal && (*end == 'u' || *end == 'U'))
			{
				tok.id = tokenid::uint_literal;

				end++;
			}

			tok.length = end - begin;

			switch (tok.id)
			{
				case tokenid::int_literal:
					tok.literal_as_int = strtol(begin, nullptr, radix) & UINT_MAX;
					break;
				case tokenid::uint_literal:
					tok.literal_as_uint = strtoul(begin, nullptr, radix) & UINT_MAX;
					break;
				case tokenid::float_literal:
					tok.literal_as_float = static_cast<float>(strtod(begin, nullptr));
					break;
				case tokenid::double_literal:
					tok.literal_as_double = strtod(begin, nullptr);
					break;
			}
		}
	}
}
