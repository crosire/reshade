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
			bool isspace(char c)
			{
				return c == ' ' || ((c >= '\t' && c <= '\r') && c != '\n');
			}
		}

		Lexer::Lexer(const Lexer &lexer)
		{
			operator=(lexer);
		}
		Lexer::Lexer(const std::string &source) : _source(source), _cur(_source.data()), _end(_cur + _source.size())
		{
		}

		Lexer &Lexer::operator=(const Lexer &lexer)
		{
			_source = lexer._source;
			_cur = _source.data() + (lexer._cur - lexer._source.data());
			_end = _source.data() + _source.size();

			return *this;
		}

		Lexer::Token Lexer::Lex()
		{
		NextToken:
			Token token;
			token.Id = TokenId::Unknown;
			token.Location = _location;
			token.Length = 0;
			token.LiteralAsDouble = 0;

			switch (_cur[0])
			{
				case '\0':
					token.Id = TokenId::EndOfStream;
					return token;
				case '\t':
				case '\v':
				case '\f':
				case '\r':
				case ' ':
					SkipSpace();
					goto NextToken;
				case '\n':
					_cur++;
					_location.Line++;
					_location.Column = 1;
					goto NextToken;
				case '!':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::ExclaimEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Exclaim;
						token.Length = 1;
					}
					break;
				case '"':
					ParseStringLiteral(token, true);
					break;
				case '#':
					if (_location.Column <= 1)
					{
						_cur++;
						ParsePreProcessorDirective();
						goto NextToken;
					}

					token.Id = TokenId::Hash;
					token.Length = 1;
					break;
				case '$':
					token.Id = TokenId::Dollar;
					token.Length = 1;
					break;
				case '%':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::PercentEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Percent;
						token.Length = 1;
					}
					break;
				case '&':
					if (_cur[1] == '&')
					{
						token.Id = TokenId::AmpersandAmpersand;
						token.Length = 2;
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::AmpersandEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Ampersand;
						token.Length = 1;
					}
					break;
				case '(':
					token.Id = TokenId::ParenthesisOpen;
					token.Length = 1;
					break;
				case ')':
					token.Id = TokenId::ParenthesisClose;
					token.Length = 1;
					break;
				case '*':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::StarEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Star;
						token.Length = 1;
					}
					break;
				case '+':
					if (_cur[1] == '+')
					{
						token.Id = TokenId::PlusPlus;
						token.Length = 2;
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::PlusEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Plus;
						token.Length = 1;
					}
					break;
				case ',':
					token.Id = TokenId::Comma;
					token.Length = 1;
					break;
				case '-':
					if (_cur[1] == '-')
					{
						token.Id = TokenId::MinusMinus;
						token.Length = 2;
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::MinusEqual;
						token.Length = 2;
					}
					else if (_cur[1] == '>')
					{
						token.Id = TokenId::Arrow;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Minus;
						token.Length = 1;
					}
					break;
				case '.':
					if (isdigit(_cur[1]))
					{
						ParseNumericLiteral(token);
					}
					else if (_cur[1] == '.' && _cur[2] == '.')
					{
						token.Id = TokenId::Ellipsis;
						token.Length = 3;
					}
					else
					{
						token.Id = TokenId::Dot;
						token.Length = 1;
					}
					break;
				case '/':
					if (_cur[1] == '/')
					{
						SkipToNewLine();
						goto NextToken;
					}
					else if (_cur[1] == '*')
					{
						while (_cur < _end)
						{
							if (*++_cur == '\n')
							{
								_location.Line++;
								_location.Column = 1;
							}
							else if (_cur[0] == '*' && _cur[1] == '/')
							{
								_cur += 2;
								_location.Column += 2;
								break;
							}
							else
							{
								_location.Column++;
							}
						}

						goto NextToken;
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::SlashEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Slash;
						token.Length = 1;
					}
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
					ParseNumericLiteral(token);
					break;
				case ':':
					if (_cur[1] == ':')
					{
						token.Id = TokenId::ColonColon;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Colon;
						token.Length = 1;
					}
					break;
				case ';':
					token.Id = TokenId::Semicolon;
					token.Length = 1;
					break;
				case '<':
					if (_cur[1] == '<')
					{
						if (_cur[2] == '=')
						{
							token.Id = TokenId::LessLessEqual;
							token.Length = 3;
						}
						else
						{
							token.Id = TokenId::LessLess;
							token.Length = 2;
						}
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::LessEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Less;
						token.Length = 1;
					}
					break;
				case '=':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::EqualEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Equal;
						token.Length = 1;
					}
					break;
				case '>':
					if (_cur[1] == '>')
					{
						if (_cur[2] == '=')
						{
							token.Id = TokenId::GreaterGreaterEqual;
							token.Length = 3;
						}
						else
						{
							token.Id = TokenId::GreaterGreater;
							token.Length = 2;
						}
					}
					else if (_cur[1] == '=')
					{
						token.Id = TokenId::GreaterEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Greater;
						token.Length = 1;
					}
					break;
				case '?':
					token.Id = TokenId::Question;
					token.Length = 1;
					break;
				case '@':
					token.Id = TokenId::At;
					token.Length = 1;
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
					ParseIdentifier(token);
					break;
				case '[':
					token.Id = TokenId::BracketOpen;
					token.Length = 1;
					break;
				case '\\':
					token.Id = TokenId::BackSlash;
					token.Length = 1;
					break;
				case ']':
					token.Id = TokenId::BracketClose;
					token.Length = 1;
					break;
				case '^':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::CaretEqual;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Caret;
						token.Length = 1;
					}
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
					ParseIdentifier(token);
					break;
				case '{':
					token.Id = TokenId::BraceOpen;
					token.Length = 1;
					break;
				case '|':
					if (_cur[1] == '=')
					{
						token.Id = TokenId::PipeEqual;
						token.Length = 2;
					}
					else if (_cur[1] == '|')
					{
						token.Id = TokenId::PipePipe;
						token.Length = 2;
					}
					else
					{
						token.Id = TokenId::Pipe;
						token.Length = 1;
					}
					break;
				case '}':
					token.Id = TokenId::BraceClose;
					token.Length = 1;
					break;
				case '~':
					token.Id = TokenId::Tilde;
					token.Length = 1;
					break;
				default:
					token.Id = TokenId::Unknown;
					break;
			}

			_cur += token.Length;
			_location.Column += token.Length;

			return token;
		}

		void Lexer::SkipSpace()
		{
			while (isspace(*_cur) && _cur < _end)
			{
				_cur++;
				_location.Column++;
			}
		}
		void Lexer::SkipToNewLine()
		{
			while (*_cur != '\n' && _cur < _end)
			{
				_cur++;
			}
		}

		void Lexer::ParseIdentifier(Token &token) const
		{
			const char *const begin = _cur, *end = begin;

			while (isalnum(*++end))
			{
				continue;
			}

			token.Id = TokenId::Identifier;
			token.Length = static_cast<unsigned int>(end - begin);
			token.LiteralAsString = std::string(begin, end);

			#pragma region Keywords
			struct Keyword
			{
				const char *data;
				unsigned char length;
				TokenId id;
			};

			static const Keyword sKeywords[] =
			{
				{ "asm", 3, TokenId::ReservedWord },
				{ "asm_fragment", 12, TokenId::ReservedWord },
				{ "auto", 4, TokenId::ReservedWord },
				{ "bool", 4, TokenId::Bool },
				{ "bool2", 5, TokenId::Bool2 },
				{ "bool2x2", 7, TokenId::Bool2x2 },
				{ "bool3", 5, TokenId::Bool3 },
				{ "bool3x3", 7, TokenId::Bool3x3 },
				{ "bool4", 5, TokenId::Bool4 },
				{ "bool4x4", 7, TokenId::Bool4x4 },
				{ "break", 5, TokenId::Break },
				{ "case", 4, TokenId::Case },
				{ "cast", 4, TokenId::ReservedWord },
				{ "catch", 5, TokenId::ReservedWord },
				{ "centroid", 8, TokenId::ReservedWord },
				{ "char", 4, TokenId::ReservedWord },
				{ "class", 5, TokenId::ReservedWord },
				{ "column_major", 12, TokenId::ReservedWord },
				{ "compile", 7, TokenId::ReservedWord },
				{ "const", 5, TokenId::Const },
				{ "const_cast", 10, TokenId::ReservedWord },
				{ "continue", 8, TokenId::Continue },
				{ "default", 7, TokenId::Default },
				{ "delete", 6, TokenId::ReservedWord },
				{ "discard", 7, TokenId::Discard },
				{ "do", 2, TokenId::Do },
				{ "double", 6, TokenId::ReservedWord },
				{ "dword", 5, TokenId::Uint },
				{ "dword2", 6, TokenId::Uint2 },
				{ "dword2x2", 8, TokenId::Uint2x2 },
				{ "dword3", 6, TokenId::Uint3, },
				{ "dword3x3", 8, TokenId::Uint3x3 },
				{ "dword4", 6, TokenId::Uint4 },
				{ "dword4x4", 8, TokenId::Uint4x4 },
				{ "dynamic_cast", 12, TokenId::ReservedWord },
				{ "else", 4, TokenId::Else },
				{ "enum", 4, TokenId::ReservedWord },
				{ "explicit", 8, TokenId::ReservedWord },
				{ "extern", 6, TokenId::Extern },
				{ "external", 8, TokenId::ReservedWord },
				{ "false", 5, TokenId::False },
				{ "FALSE", 5, TokenId::False },
				{ "float", 5, TokenId::Float },
				{ "float2", 6, TokenId::Float2 },
				{ "float2x2", 8, TokenId::Float2x2 },
				{ "float3", 6, TokenId::Float3 },
				{ "float3x3", 8, TokenId::Float3x3 },
				{ "float4", 6, TokenId::Float4 },
				{ "float4x4", 8, TokenId::Float4x4 },
				{ "for", 3, TokenId::For },
				{ "foreach", 7, TokenId::ReservedWord },
				{ "friend", 6, TokenId::ReservedWord },
				{ "globallycoherent", 16, TokenId::ReservedWord },
				{ "goto", 4, TokenId::ReservedWord },
				{ "groupshared", 11, TokenId::ReservedWord },
				{ "half", 4, TokenId::ReservedWord },
				{ "half2", 5, TokenId::ReservedWord },
				{ "half2x2", 7, TokenId::ReservedWord },
				{ "half3", 5, TokenId::ReservedWord },
				{ "half3x3", 7, TokenId::ReservedWord },
				{ "half4", 5, TokenId::ReservedWord },
				{ "half4x4", 7, TokenId::ReservedWord },
				{ "if", 2, TokenId::If },
				{ "in", 2, TokenId::In },
				{ "inline", 6, TokenId::ReservedWord },
				{ "inout", 5, TokenId::InOut },
				{ "int", 3, TokenId::Int },
				{ "int2", 4, TokenId::Int2 },
				{ "int2x2", 6, TokenId::Int2x2 },
				{ "int3", 4, TokenId::Int3 },
				{ "int3x3", 6, TokenId::Int3x3 },
				{ "int4", 4, TokenId::Int4 },
				{ "int4x4", 6, TokenId::Int4x4 },
				{ "interface", 9, TokenId::ReservedWord },
				{ "linear", 6, TokenId::Linear },
				{ "long", 4, TokenId::ReservedWord },
				{ "matrix", 6, TokenId::Matrix },
				{ "mutable", 7, TokenId::ReservedWord },
				{ "namespace", 9, TokenId::Namespace },
				{ "new", 3, TokenId::ReservedWord },
				{ "noinline", 8, TokenId::ReservedWord },
				{ "nointerpolation", 15, TokenId::NoInterpolation },
				{ "noperspective", 13, TokenId::NoPerspective },
				{ "operator", 8, TokenId::ReservedWord },
				{ "out", 3, TokenId::Out },
				{ "packed", 6, TokenId::ReservedWord },
				{ "packoffset", 10, TokenId::ReservedWord },
				{ "pass", 4, TokenId::Pass },
				{ "precise", 7, TokenId::Precise },
				{ "private", 7, TokenId::ReservedWord },
				{ "protected", 9, TokenId::ReservedWord },
				{ "public", 6, TokenId::ReservedWord },
				{ "register", 8, TokenId::ReservedWord },
				{ "reinterpret_cast", 16, TokenId::ReservedWord },
				{ "return", 6, TokenId::Return },
				{ "row_major", 9, TokenId::ReservedWord },
				{ "sample", 6, TokenId::ReservedWord },
				{ "sampler", 7, TokenId::Sampler2D },
				{ "sampler1D", 9, TokenId::Sampler1D },
				{ "sampler1DArray", 14, TokenId::ReservedWord },
				{ "sampler1DArrayShadow", 20, TokenId::ReservedWord },
				{ "sampler1DShadow", 15, TokenId::ReservedWord },
				{ "sampler2D", 9, TokenId::Sampler2D },
				{ "sampler2DArray", 14, TokenId::ReservedWord },
				{ "sampler2DArrayShadow", 20, TokenId::ReservedWord },
				{ "sampler2DMS", 11, TokenId::ReservedWord },
				{ "sampler2DMSArray", 16, TokenId::ReservedWord },
				{ "sampler2DShadow", 15, TokenId::ReservedWord },
				{ "sampler3D", 9, TokenId::Sampler3D },
				{ "sampler_state", 13, TokenId::ReservedWord },
				{ "samplerCUBE", 11, TokenId::ReservedWord },
				{ "samplerRECT", 11, TokenId::ReservedWord },
				{ "SamplerState", 12, TokenId::ReservedWord },
				{ "shared", 6, TokenId::ReservedWord },
				{ "short", 5, TokenId::ReservedWord },
				{ "signed", 6, TokenId::ReservedWord },
				{ "sizeof", 6, TokenId::ReservedWord },
				{ "snorm", 5, TokenId::ReservedWord },
				{ "static", 6, TokenId::Static },
				{ "static_cast", 11, TokenId::ReservedWord },
				{ "string", 6, TokenId::String },
				{ "struct", 6, TokenId::Struct },
				{ "switch", 6, TokenId::Switch },
				{ "technique", 9, TokenId::Technique },
				{ "template", 8, TokenId::ReservedWord },
				{ "texture", 7, TokenId::Texture2D },
				{ "Texture1D", 9, TokenId::ReservedWord },
				{ "texture1D", 9, TokenId::Texture1D },
				{ "Texture1DArray", 14, TokenId::ReservedWord },
				{ "Texture2D", 9, TokenId::ReservedWord },
				{ "texture2D", 9, TokenId::Texture2D },
				{ "Texture2DArray", 14, TokenId::ReservedWord },
				{ "Texture2DMS", 11, TokenId::ReservedWord },
				{ "Texture2DMSArray", 16, TokenId::ReservedWord },
				{ "Texture3D", 9, TokenId::ReservedWord },
				{ "texture3D", 9, TokenId::Texture3D },
				{ "textureCUBE", 11, TokenId::ReservedWord },
				{ "TextureCube", 11, TokenId::ReservedWord },
				{ "TextureCubeArray", 16, TokenId::ReservedWord },
				{ "textureRECT", 11, TokenId::ReservedWord },
				{ "this", 4, TokenId::ReservedWord },
				{ "true", 4, TokenId::True },
				{ "TRUE", 4, TokenId::True },
				{ "try", 3, TokenId::ReservedWord },
				{ "typedef", 7, TokenId::ReservedWord },
				{ "uint", 4, TokenId::Uint },
				{ "uint2", 5, TokenId::Uint2 },
				{ "uint2x2", 7, TokenId::Uint2x2 },
				{ "uint3", 5, TokenId::Uint3 },
				{ "uint3x3", 7, TokenId::Uint3x3 },
				{ "uint4", 5, TokenId::Uint4 },
				{ "uint4x4", 7, TokenId::Uint4x4 },
				{ "uniform", 7, TokenId::Uniform },
				{ "union", 5, TokenId::ReservedWord },
				{ "unorm", 5, TokenId::ReservedWord },
				{ "unsigned", 8, TokenId::ReservedWord },
				{ "vector", 6, TokenId::Vector },
				{ "virtual", 7, TokenId::ReservedWord },
				{ "void", 4, TokenId::Void },
				{ "volatile", 8, TokenId::Volatile },
				{ "while", 5, TokenId::While }
			};
			#pragma endregion

			for (unsigned int i = 0; i < sizeof(sKeywords) / sizeof(Keyword); i++)
			{
				if (sKeywords[i].length == token.Length && strncmp(begin, sKeywords[i].data, token.Length) == 0)
				{
					token.Id = sKeywords[i].id;
					break;
				}
			}
		}
		void Lexer::ParseStringLiteral(Token &token, bool escape) const
		{
			char c;
			const char *const begin = _cur, *end = begin;

			while ((c = *++end) != '"')
			{
				if (c == '\n' || c == '\r' || end >= _end)
				{
					token.Id = TokenId::StringLiteral;
					token.Length = static_cast<unsigned int>(end - begin);
					return;
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
							for (unsigned int i = 0; i < 3 && isodigit(*end) && end < _end; ++i)
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

				token.LiteralAsString += c;
			}

			token.Id = TokenId::StringLiteral;
			token.Length = static_cast<unsigned int>(end - begin + 1);
		}
		void Lexer::ParseNumericLiteral(Token &token) const
		{
			int radix = 0;
			const char *begin = _cur, *end = begin;

			token.Id = TokenId::IntLiteral;

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
					token.Id = TokenId::FloatLiteral;

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
						token.Id = TokenId::FloatLiteral;

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
				token.Id = TokenId::FloatLiteral;

				end++;
			}
			else if (*end == 'l' || *end == 'L')
			{
				radix = 10;
				token.Id = TokenId::DoubleLiteral;

				end++;
			}
			else if (token.Id == TokenId::IntLiteral && (*end == 'u' || *end == 'U'))
			{
				token.Id = TokenId::UintLiteral;

				end++;
			}

			token.Length = static_cast<unsigned int>(end - begin);

			switch (token.Id)
			{
				case TokenId::IntLiteral:
					token.LiteralAsInt = strtol(begin, nullptr, radix) & UINT_MAX;
					break;
				case TokenId::UintLiteral:
					token.LiteralAsUint = strtoul(begin, nullptr, radix) & UINT_MAX;
					break;
				case TokenId::FloatLiteral:
					token.LiteralAsFloat = static_cast<float>(strtod(begin, nullptr));
					break;
				case TokenId::DoubleLiteral:
					token.LiteralAsDouble = strtod(begin, nullptr);
					break;
			}
		}
		void Lexer::ParsePreProcessorDirective()
		{
			SkipSpace();

			std::string command;

			while (isalnum(*_cur))
			{
				command.push_back(*_cur++);
			}

			SkipSpace();

			if (command == "line")
			{
				_location.Line = static_cast<int>(strtol(_cur, const_cast<char **>(&_cur), 10));

				if (_location.Line != 0)
				{
					_location.Line--;
				}

				SkipSpace();

				if (_cur[0] == '"')
				{
					Token token;
					ParseStringLiteral(token, false);

					_location.Source = token.LiteralAsString;
				}
			}
			else
			{
				while ((_cur[0] == '\n' || _cur[-1] == '\\') && _cur < _end)
				{
					continue;
				}
			}

			SkipToNewLine();
		}
	}
}
