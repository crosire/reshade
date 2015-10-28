#include "Lexer.hpp"

namespace
{
	inline bool isdigit(char c)
	{
		return static_cast<unsigned>(c - '0') < 10;
	}
	inline bool isodigit(char c)
	{
		return static_cast<unsigned>(c - '0') < 8;
	}
	inline bool isxdigit(char c)
	{
		return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	}
	inline bool isalpha(char c)
	{
		return static_cast<unsigned>((c | 32) - 'a') < 26;
	}
	inline bool isalnum(char c)
	{
		return isalpha(c) || isdigit(c) || c == '_';
	}
	inline bool isspace(char c)
	{
		return c == ' ' || ((c >= '\t' && c <= '\r') && c != '\n');
	}
}

namespace ReShade
{
	namespace FX
	{
		std::string Lexer::Token::GetName(Id id)
		{
			switch (id)
			{
				default:
				case Id::Unknown:
					return "unknown";
				case Id::EndOfStream:
					return "end of file";
				case Id::Exclaim:
					return "!";
				case Id::Hash:
					return "#";
				case Id::Dollar:
					return "$";
				case Id::Percent:
					return "%";
				case Id::Ampersand:
					return "&";
				case Id::ParenthesisOpen:
					return "(";
				case Id::ParenthesisClose:
					return ")";
				case Id::Star:
					return "*";
				case Id::Plus:
					return "+";
				case Id::Comma:
					return ",";
				case Id::Minus:
					return "-";
				case Id::Dot:
					return ".";
				case Id::Slash:
					return "/";
				case Id::Colon:
					return ":";
				case Id::Semicolon:
					return ";";
				case Id::Less:
					return "<";
				case Id::Equal:
					return "=";
				case Id::Greater:
					return ">";
				case Id::Question:
					return "?";
				case Id::At:
					return "@";
				case Id::BracketOpen:
					return "[";
				case Id::BackSlash:
					return "\\";
				case Id::BracketClose:
					return "]";
				case Id::Caret:
					return "^";
				case Id::BraceOpen:
					return "{";
				case Id::Pipe:
					return "|";
				case Id::BraceClose:
					return "}";
				case Id::Tilde:
					return "~";
				case Id::ExclaimEqual:
					return "!=";
				case Id::PercentEqual:
					return "%=";
				case Id::AmpersandAmpersand:
					return "&&";
				case Id::AmpersandEqual:
					return "&=";
				case Id::StarEqual:
					return "*=";
				case Id::PlusPlus:
					return "++";
				case Id::PlusEqual:
					return "+=";
				case Id::MinusMinus:
					return "--";
				case Id::MinusEqual:
					return "-=";
				case Id::Arrow:
					return "->";
				case Id::Ellipsis:
					return "...";
				case Id::SlashEqual:
					return "|=";
				case Id::ColonColon:
					return "::";
				case Id::LessLessEqual:
					return "<<=";
				case Id::LessLess:
					return "<<";
				case Id::LessEqual:
					return "<=";
				case Id::EqualEqual:
					return "==";
				case Id::GreaterGreaterEqual:
					return ">>=";
				case Id::GreaterGreater:
					return ">>";
				case Id::GreaterEqual:
					return ">=";
				case Id::CaretEqual:
					return "^=";
				case Id::PipeEqual:
					return "|=";
				case Id::PipePipe:
					return "||";
				case Id::Identifier:
					return "identifier";
				case Id::ReservedWord:
					return "reserved word";
				case Id::True:
					return "true";
				case Id::False:
					return "false";
				case Id::IntLiteral:
				case Id::UintLiteral:
					return "integral literal";
				case Id::FloatLiteral:
				case Id::DoubleLiteral:
					return "floating point literal";
				case Id::StringLiteral:
					return "string literal";
				case Id::Namespace:
					return "namespace";
				case Id::Struct:
					return "struct";
				case Id::Technique:
					return "technique";
				case Id::Pass:
					return "pass";
				case Id::For:
					return "for";
				case Id::While:
					return "while";
				case Id::Do:
					return "do";
				case Id::If:
					return "if";
				case Id::Else:
					return "else";
				case Id::Switch:
					return "switch";
				case Id::Case:
					return "case";
				case Id::Default:
					return "default";
				case Id::Break:
					return "break";
				case Id::Continue:
					return "continue";
				case Id::Return:
					return "return";
				case Id::Discard:
					return "discard";
				case Id::Extern:
					return "extern";
				case Id::Static:
					return "static";
				case Id::Uniform:
					return "uniform";
				case Id::Volatile:
					return "volatile";
				case Id::Precise:
					return "precise";
				case Id::In:
					return "in";
				case Id::Out:
					return "out";
				case Id::InOut:
					return "inout";
				case Id::Const:
					return "const";
				case Id::Linear:
					return "linear";
				case Id::NoPerspective:
					return "noperspective";
				case Id::Centroid:
					return "centroid";
				case Id::NoInterpolation:
					return "nointerpolation";
				case Id::Void:
					return "void";
				case Id::Bool:
				case Id::Bool2:
				case Id::Bool3:
				case Id::Bool4:
				case Id::Bool2x2:
				case Id::Bool3x3:
				case Id::Bool4x4:
					return "bool";
				case Id::Int:
				case Id::Int2:
				case Id::Int3:
				case Id::Int4:
				case Id::Int2x2:
				case Id::Int3x3:
				case Id::Int4x4:
					return "int";
				case Id::Uint:
				case Id::Uint2:
				case Id::Uint3:
				case Id::Uint4:
				case Id::Uint2x2:
				case Id::Uint3x3:
				case Id::Uint4x4:
					return "uint";
				case Id::Float:
				case Id::Float2:
				case Id::Float3:
				case Id::Float4:
				case Id::Float2x2:
				case Id::Float3x3:
				case Id::Float4x4:
					return "float";
				case Id::Vector:
					return "vector";
				case Id::Matrix:
					return "matrix";
				case Id::String:
					return "string";
				case Id::Texture1D:
					return "texture1D";
				case Id::Texture2D:
					return "texture2D";
				case Id::Texture3D:
					return "texture3D";
				case Id::Sampler1D:
					return "sampler1D";
				case Id::Sampler2D:
					return "sampler2D";
				case Id::Sampler3D:
					return "sampler3D";
			}
		}

		Lexer::Lexer(const std::string &source) : mSource(source), mPos(this->mSource.data()), mEnd(this->mSource.data() + this->mSource.size()), mCurrentAtLineBegin(true)
		{
		}

		Lexer &Lexer::operator=(const Lexer &lexer)
		{
			this->mSource = lexer.mSource;
			this->mPos = this->mSource.data() + (lexer.mPos - lexer.mSource.data());
			this->mEnd = this->mSource.data() + this->mSource.size();
			this->mCurrentAtLineBegin = lexer.mCurrentAtLineBegin;

			return *this;
		}

		Lexer::Token Lexer::Lex()
		{
		NextToken:
			Token token;
			token.mLocation = this->mCurrentLocation;
			token.mRawData = this->mPos;

			switch (this->mPos[0])
			{
				case '\0':
				{
					token.mId = Token::Id::EndOfStream;

					return token;
				}
				case '\t':
				case '\v':
				case '\f':
				case '\r':
				case ' ':
				{
					while (isspace(*this->mPos) && this->mPos < this->mEnd)
					{
						this->mPos++;
						this->mCurrentLocation.Column++;
					}

					goto NextToken;
				}
				case '\n':
				{
					this->mPos++;
					this->mCurrentAtLineBegin = true;
					this->mCurrentLocation.Line++;
					this->mCurrentLocation.Column = 1;

					goto NextToken;
				}
				case '!':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::ExclaimEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Exclaim;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '"':
				{
					ParseStringLiteral(token);
					break;
				}
				case '#':
				{
					if (this->mCurrentAtLineBegin)
					{
						this->mPos++;

						ParsePreProcessorDirective();

						goto NextToken;
					}
					else
					{
						token.mId = Token::Id::Hash;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '$':
				{
					token.mId = Token::Id::Dollar;
					token.mRawDataLength = 1;
					break;
				}
				case '%':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::PercentEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Percent;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '&':
				{
					if (this->mPos[1] == '&')
					{
						token.mId = Token::Id::AmpersandAmpersand;
						token.mRawDataLength = 2;
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::AmpersandEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Ampersand;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '(':
				{
					token.mId = Token::Id::ParenthesisOpen;
					token.mRawDataLength = 1;
					break;
				}
				case ')':
				{
					token.mId = Token::Id::ParenthesisClose;
					token.mRawDataLength = 1;
					break;
				}
				case '*':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::StarEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Star;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '+':
				{
					if (this->mPos[1] == '+')
					{
						token.mId = Token::Id::PlusPlus;
						token.mRawDataLength = 2;
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::PlusEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Plus;
						token.mRawDataLength = 1;
					}
					break;
				}
				case ',':
				{
					token.mId = Token::Id::Comma;
					token.mRawDataLength = 1;
					break;
				}
				case '-':
				{
					if (this->mPos[1] == '-')
					{
						token.mId = Token::Id::MinusMinus;
						token.mRawDataLength = 2;
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::MinusEqual;
						token.mRawDataLength = 2;
					}
					else if (this->mPos[1] == '>')
					{
						token.mId = Token::Id::Arrow;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Minus;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '.':
				{
					if (isdigit(this->mPos[1]))
					{
						ParseNumericLiteral(token);
					}
					else if (this->mPos[1] == '.' && this->mPos[2] == '.')
					{
						token.mId = Token::Id::Ellipsis;
						token.mRawDataLength = 3;
					}
					else
					{
						token.mId = Token::Id::Dot;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '/':
				{
					if (this->mPos[1] == '/')
					{
						while (*this->mPos != '\n' && this->mPos < this->mEnd)
						{
							this->mPos++;
						}

						goto NextToken;
					}
					else if (this->mPos[1] == '*')
					{
						while (this->mPos < this->mEnd)
						{
							if (*++this->mPos == '\n')
							{
								this->mCurrentLocation.Line++;
								this->mCurrentLocation.Column = 1;
							}
							else if (this->mPos[0] == '*' && this->mPos[1] == '/')
							{
								this->mPos += 2;
								this->mCurrentLocation.Column += 2;
								break;
							}
							else
							{
								this->mCurrentLocation.Column++;
							}
						}

						goto NextToken;
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::SlashEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Slash;
						token.mRawDataLength = 1;
					}
					break;
				}
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
				{
					ParseNumericLiteral(token);
					break;
				}
				case ':':
				{
					if (this->mPos[1] == ':')
					{
						token.mId = Token::Id::ColonColon;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Colon;
						token.mRawDataLength = 1;
					}
					break;
				}
				case ';':
				{
					token.mId = Token::Id::Semicolon;
					token.mRawDataLength = 1;
					break;
				}
				case '<':
				{
					if (this->mPos[1] == '<')
					{
						if (this->mPos[2] == '=')
						{
							token.mId = Token::Id::LessLessEqual;
							token.mRawDataLength = 3;
						}
						else
						{
							token.mId = Token::Id::LessLess;
							token.mRawDataLength = 2;
						}
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::LessEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Less;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '=':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::EqualEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Equal;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '>':
				{
					if (this->mPos[1] == '>')
					{
						if (this->mPos[2] == '=')
						{
							token.mId = Token::Id::GreaterGreaterEqual;
							token.mRawDataLength = 3;
						}
						else
						{
							token.mId = Token::Id::GreaterGreater;
							token.mRawDataLength = 2;
						}
					}
					else if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::GreaterEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Greater;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '?':
				{
					token.mId = Token::Id::Question;
					token.mRawDataLength = 1;
					break;
				}
				case '@':
				{
					token.mId = Token::Id::At;
					token.mRawDataLength = 1;
					break;
				}
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
				{
					ParseIdentifier(token);
					break;
				}
				case '[':
				{
					token.mId = Token::Id::BracketOpen;
					token.mRawDataLength = 1;
					break;
				}
				case '\\':
				{
					token.mId = Token::Id::BackSlash;
					token.mRawDataLength = 1;
					break;
				}
				case ']':
				{
					token.mId = Token::Id::BracketClose;
					token.mRawDataLength = 1;
					break;
				}
				case '^':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::CaretEqual;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Caret;
						token.mRawDataLength = 1;
					}
					break;
				}
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
				{
					ParseIdentifier(token);
					break;
				}
				case '{':
				{
					token.mId = Token::Id::BraceOpen;
					token.mRawDataLength = 1;
					break;
				}
				case '|':
				{
					if (this->mPos[1] == '=')
					{
						token.mId = Token::Id::PipeEqual;
						token.mRawDataLength = 2;
					}
					else if (this->mPos[1] == '|')
					{
						token.mId = Token::Id::PipePipe;
						token.mRawDataLength = 2;
					}
					else
					{
						token.mId = Token::Id::Pipe;
						token.mRawDataLength = 1;
					}
					break;
				}
				case '}':
				{
					token.mId = Token::Id::BraceClose;
					token.mRawDataLength = 1;
					break;
				}
				case '~':
				{
					token.mId = Token::Id::Tilde;
					token.mRawDataLength = 1;
					break;
				}
				default:
				{
					token.mId = Token::Id::Unknown;
					token.mRawDataLength = 1;
					break;
				}
			}

			this->mPos += token.mRawDataLength;
			this->mCurrentAtLineBegin = false;
			this->mCurrentLocation.Column += static_cast<unsigned int>(token.mRawDataLength);

			return token;
		}
		void Lexer::ParseIdentifier(Token &token) const
		{
			const char *const begin = this->mPos, *end = begin;

			while (isalnum(*++end))
			{
				continue;
			}

			token.mId = Token::Id::Identifier;
			token.mRawData = begin;
			token.mRawDataLength = end - begin;

			#pragma region Keywords
			struct Keyword
			{
				const char *data;
				unsigned char length;
				Lexer::Token::Id id;
			};

			static const Keyword sKeywords[] =
			{
				{ "asm", 3, Lexer::Token::Id::ReservedWord },
				{ "asm_fragment", 12, Lexer::Token::Id::ReservedWord },
				{ "auto", 4, Lexer::Token::Id::ReservedWord },
				{ "bool", 4, Lexer::Token::Id::Bool },
				{ "bool2", 5, Lexer::Token::Id::Bool2 },
				{ "bool2x2", 7, Lexer::Token::Id::Bool2x2 },
				{ "bool3", 5, Lexer::Token::Id::Bool3 },
				{ "bool3x3", 7, Lexer::Token::Id::Bool3x3 },
				{ "bool4", 5, Lexer::Token::Id::Bool4 },
				{ "bool4x4", 7, Lexer::Token::Id::Bool4x4 },
				{ "break", 5, Lexer::Token::Id::Break },
				{ "case", 4, Lexer::Token::Id::Case },
				{ "cast", 4, Lexer::Token::Id::ReservedWord },
				{ "catch", 5, Lexer::Token::Id::ReservedWord },
				{ "centroid", 8, Lexer::Token::Id::ReservedWord },
				{ "char", 4, Lexer::Token::Id::ReservedWord },
				{ "class", 5, Lexer::Token::Id::ReservedWord },
				{ "column_major", 12, Lexer::Token::Id::ReservedWord },
				{ "compile", 7, Lexer::Token::Id::ReservedWord },
				{ "const", 5, Lexer::Token::Id::Const },
				{ "const_cast", 10, Lexer::Token::Id::ReservedWord },
				{ "continue", 8, Lexer::Token::Id::Continue },
				{ "default", 7, Lexer::Token::Id::Default },
				{ "delete", 6, Lexer::Token::Id::ReservedWord },
				{ "discard", 7, Lexer::Token::Id::Discard },
				{ "do", 2, Lexer::Token::Id::Do },
				{ "double", 6, Lexer::Token::Id::ReservedWord },
				{ "dword", 5, Lexer::Token::Id::Uint },
				{ "dword2", 6, Lexer::Token::Id::Uint2 },
				{ "dword2x2", 8, Lexer::Token::Id::Uint2x2 },
				{ "dword3", 6, Lexer::Token::Id::Uint3, },
				{ "dword3x3", 8, Lexer::Token::Id::Uint3x3 },
				{ "dword4", 6, Lexer::Token::Id::Uint4 },
				{ "dword4x4", 8, Lexer::Token::Id::Uint4x4 },
				{ "dynamic_cast", 12, Lexer::Token::Id::ReservedWord },
				{ "else", 4, Lexer::Token::Id::Else },
				{ "enum", 4, Lexer::Token::Id::ReservedWord },
				{ "explicit", 8, Lexer::Token::Id::ReservedWord },
				{ "extern", 6, Lexer::Token::Id::Extern },
				{ "external", 8, Lexer::Token::Id::ReservedWord },
				{ "false", 5, Lexer::Token::Id::False },
				{ "FALSE", 5, Lexer::Token::Id::False },
				{ "float", 5, Lexer::Token::Id::Float },
				{ "float2", 6, Lexer::Token::Id::Float2 },
				{ "float2x2", 8, Lexer::Token::Id::Float2x2 },
				{ "float3", 6, Lexer::Token::Id::Float3 },
				{ "float3x3", 8, Lexer::Token::Id::Float3x3 },
				{ "float4", 6, Lexer::Token::Id::Float4 },
				{ "float4x4", 8, Lexer::Token::Id::Float4x4 },
				{ "for", 3, Lexer::Token::Id::For },
				{ "foreach", 7, Lexer::Token::Id::ReservedWord },
				{ "friend", 6, Lexer::Token::Id::ReservedWord },
				{ "globallycoherent", 16, Lexer::Token::Id::ReservedWord },
				{ "goto", 4, Lexer::Token::Id::ReservedWord },
				{ "groupshared", 11, Lexer::Token::Id::ReservedWord },
				{ "half", 4, Lexer::Token::Id::ReservedWord },
				{ "half2", 5, Lexer::Token::Id::ReservedWord },
				{ "half2x2", 7, Lexer::Token::Id::ReservedWord },
				{ "half3", 5, Lexer::Token::Id::ReservedWord },
				{ "half3x3", 7, Lexer::Token::Id::ReservedWord },
				{ "half4", 5, Lexer::Token::Id::ReservedWord },
				{ "half4x4", 7, Lexer::Token::Id::ReservedWord },
				{ "if", 2, Lexer::Token::Id::If },
				{ "in", 2, Lexer::Token::Id::In },
				{ "inline", 6, Lexer::Token::Id::ReservedWord },
				{ "inout", 5, Lexer::Token::Id::InOut },
				{ "int", 3, Lexer::Token::Id::Int },
				{ "int2", 4, Lexer::Token::Id::Int2 },
				{ "int2x2", 6, Lexer::Token::Id::Int2x2 },
				{ "int3", 4, Lexer::Token::Id::Int3 },
				{ "int3x3", 6, Lexer::Token::Id::Int3x3 },
				{ "int4", 4, Lexer::Token::Id::Int4 },
				{ "int4x4", 6, Lexer::Token::Id::Int4x4 },
				{ "interface", 9, Lexer::Token::Id::ReservedWord },
				{ "linear", 6, Lexer::Token::Id::Linear },
				{ "long", 4, Lexer::Token::Id::ReservedWord },
				{ "matrix", 6, Lexer::Token::Id::Matrix },
				{ "mutable", 7, Lexer::Token::Id::ReservedWord },
				{ "namespace", 9, Lexer::Token::Id::Namespace },
				{ "new", 3, Lexer::Token::Id::ReservedWord },
				{ "noinline", 8, Lexer::Token::Id::ReservedWord },
				{ "nointerpolation", 15, Lexer::Token::Id::NoInterpolation },
				{ "noperspective", 13, Lexer::Token::Id::NoPerspective },
				{ "operator", 8, Lexer::Token::Id::ReservedWord },
				{ "out", 3, Lexer::Token::Id::Out },
				{ "packed", 6, Lexer::Token::Id::ReservedWord },
				{ "packoffset", 10, Lexer::Token::Id::ReservedWord },
				{ "pass", 4, Lexer::Token::Id::Pass },
				{ "precise", 7, Lexer::Token::Id::Precise },
				{ "private", 7, Lexer::Token::Id::ReservedWord },
				{ "protected", 9, Lexer::Token::Id::ReservedWord },
				{ "public", 6, Lexer::Token::Id::ReservedWord },
				{ "register", 8, Lexer::Token::Id::ReservedWord },
				{ "reinterpret_cast", 16, Lexer::Token::Id::ReservedWord },
				{ "return", 6, Lexer::Token::Id::Return },
				{ "row_major", 9, Lexer::Token::Id::ReservedWord },
				{ "sample", 6, Lexer::Token::Id::ReservedWord },
				{ "sampler", 7, Lexer::Token::Id::Sampler2D },
				{ "sampler1D", 9, Lexer::Token::Id::Sampler1D },
				{ "sampler1DArray", 14, Lexer::Token::Id::ReservedWord },
				{ "sampler1DArrayShadow", 20, Lexer::Token::Id::ReservedWord },
				{ "sampler1DShadow", 15, Lexer::Token::Id::ReservedWord },
				{ "sampler2D", 9, Lexer::Token::Id::Sampler2D },
				{ "sampler2DArray", 14, Lexer::Token::Id::ReservedWord },
				{ "sampler2DArrayShadow", 20, Lexer::Token::Id::ReservedWord },
				{ "sampler2DMS", 11, Lexer::Token::Id::ReservedWord },
				{ "sampler2DMSArray", 16, Lexer::Token::Id::ReservedWord },
				{ "sampler2DShadow", 15, Lexer::Token::Id::ReservedWord },
				{ "sampler3D", 9, Lexer::Token::Id::Sampler3D },
				{ "sampler_state", 13, Lexer::Token::Id::ReservedWord },
				{ "samplerCUBE", 11, Lexer::Token::Id::ReservedWord },
				{ "samplerRECT", 11, Lexer::Token::Id::ReservedWord },
				{ "SamplerState", 12, Lexer::Token::Id::ReservedWord },
				{ "shared", 6, Lexer::Token::Id::ReservedWord },
				{ "short", 5, Lexer::Token::Id::ReservedWord },
				{ "signed", 6, Lexer::Token::Id::ReservedWord },
				{ "sizeof", 6, Lexer::Token::Id::ReservedWord },
				{ "snorm", 5, Lexer::Token::Id::ReservedWord },
				{ "static", 6, Lexer::Token::Id::Static },
				{ "static_cast", 11, Lexer::Token::Id::ReservedWord },
				{ "string", 6, Lexer::Token::Id::String },
				{ "struct", 6, Lexer::Token::Id::Struct },
				{ "switch", 6, Lexer::Token::Id::Switch },
				{ "technique", 9, Lexer::Token::Id::Technique },
				{ "template", 8, Lexer::Token::Id::ReservedWord },
				{ "texture", 7, Lexer::Token::Id::Texture2D },
				{ "Texture1D", 9, Lexer::Token::Id::ReservedWord },
				{ "texture1D", 9, Lexer::Token::Id::Texture1D },
				{ "Texture1DArray", 14, Lexer::Token::Id::ReservedWord },
				{ "Texture2D", 9, Lexer::Token::Id::ReservedWord },
				{ "texture2D", 9, Lexer::Token::Id::Texture2D },
				{ "Texture2DArray", 14, Lexer::Token::Id::ReservedWord },
				{ "Texture2DMS", 11, Lexer::Token::Id::ReservedWord },
				{ "Texture2DMSArray", 16, Lexer::Token::Id::ReservedWord },
				{ "Texture3D", 9, Lexer::Token::Id::ReservedWord },
				{ "texture3D", 9, Lexer::Token::Id::Texture3D },
				{ "textureCUBE", 11, Lexer::Token::Id::ReservedWord },
				{ "TextureCube", 11, Lexer::Token::Id::ReservedWord },
				{ "TextureCubeArray", 16, Lexer::Token::Id::ReservedWord },
				{ "textureRECT", 11, Lexer::Token::Id::ReservedWord },
				{ "this", 4, Lexer::Token::Id::ReservedWord },
				{ "true", 4, Lexer::Token::Id::True },
				{ "TRUE", 4, Lexer::Token::Id::True },
				{ "try", 3, Lexer::Token::Id::ReservedWord },
				{ "typedef", 7, Lexer::Token::Id::ReservedWord },
				{ "uint", 4, Lexer::Token::Id::Uint },
				{ "uint2", 5, Lexer::Token::Id::Uint2 },
				{ "uint2x2", 7, Lexer::Token::Id::Uint2x2 },
				{ "uint3", 5, Lexer::Token::Id::Uint3 },
				{ "uint3x3", 7, Lexer::Token::Id::Uint3x3 },
				{ "uint4", 5, Lexer::Token::Id::Uint4 },
				{ "uint4x4", 7, Lexer::Token::Id::Uint4x4 },
				{ "uniform", 7, Lexer::Token::Id::Uniform },
				{ "union", 5, Lexer::Token::Id::ReservedWord },
				{ "unorm", 5, Lexer::Token::Id::ReservedWord },
				{ "unsigned", 8, Lexer::Token::Id::ReservedWord },
				{ "vector", 6, Lexer::Token::Id::Vector },
				{ "virtual", 7, Lexer::Token::Id::ReservedWord },
				{ "void", 4, Lexer::Token::Id::Void },
				{ "volatile", 8, Lexer::Token::Id::Volatile },
				{ "while", 5, Lexer::Token::Id::While }
			};
			#pragma endregion

			for (unsigned int i = 0; i < sizeof(sKeywords) / sizeof(Keyword); i++)
			{
				if (sKeywords[i].length == token.mRawDataLength && strncmp(token.mRawData, sKeywords[i].data, token.mRawDataLength) == 0)
				{
					token.mId = sKeywords[i].id;
					break;
				}
			}
		}
		void Lexer::ParseStringLiteral(Token &token) const
		{
			char c = 0;
			const char *const begin = this->mPos, *end = begin;

			while ((c = *++end) != '"')
			{
				if (c == '\n' || c == '\r' || end >= this->mEnd)
				{
					token.mId = Token::Id::StringLiteral;
					token.mRawData = begin;
					token.mRawDataLength = end - begin;
					return;
				}

				if (c == '\\')
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
							for (unsigned int i = 0; i < 3 && isodigit(*end) && end < this->mEnd; ++i)
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
								while (isxdigit(*end) && end < this->mEnd)
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

				token.mLiteralString += c;
			}

			token.mId = Token::Id::StringLiteral;
			token.mRawData = begin;
			token.mRawDataLength = end - begin + 1;
		}
		void Lexer::ParseNumericLiteral(Token &token) const
		{
			int radix = 0;
			const char *begin = this->mPos, *end = begin;

			token.mId = Token::Id::IntLiteral;

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
					token.mId = Token::Id::FloatLiteral;

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
						token.mId = Token::Id::FloatLiteral;

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
				token.mId = Token::Id::FloatLiteral;

				end++;
			}
			else if (*end == 'l' || *end == 'L')
			{
				radix = 10;
				token.mId = Token::Id::DoubleLiteral;

				end++;
			}
			else if (token.mId == Token::Id::IntLiteral && (*end == 'u' || *end == 'U'))
			{
				token.mId = Token::Id::UintLiteral;

				end++;
			}

			token.mRawData = begin;
			token.mRawDataLength = end - begin;

			switch (token.mId)
			{
				case Token::Id::IntLiteral:
					token.mLiteralNumeric.Int = strtol(token.mRawData, nullptr, radix) & UINT_MAX;
					break;
				case Token::Id::UintLiteral:
					token.mLiteralNumeric.Uint = strtoul(token.mRawData, nullptr, radix) & UINT_MAX;
					break;
				case Token::Id::FloatLiteral:
					token.mLiteralNumeric.Float = static_cast<float>(strtod(token.mRawData, nullptr));
					break;
				case Token::Id::DoubleLiteral:
					token.mLiteralNumeric.Double = strtod(token.mRawData, nullptr);
					break;
			}
		}
		void Lexer::ParsePreProcessorDirective()
		{
			while (isspace(*this->mPos) && this->mPos < this->mEnd)
			{
				this->mPos++;
			}

			auto command = this->mPos;
			size_t commandLength = 0;

			while (isalnum(*this->mPos++))
			{
				++commandLength;
			}

			while (isspace(*this->mPos) && this->mPos < this->mEnd)
			{
				this->mPos++;
			}

			if (commandLength == 4 && strncmp(command, "line", 4) == 0)
			{
				char *sourceBegin, *sourceEnd = nullptr;

				this->mCurrentLocation.Line = static_cast<int>(strtol(this->mPos, &sourceEnd, 10));

				while (*sourceEnd != '\n' && *sourceEnd != '\0' && *++sourceEnd != '"')
				{
					continue;
				}

				sourceBegin = sourceEnd;

				while (*sourceEnd != '\n' && *sourceEnd != '\0' && *++sourceEnd != '"')
				{
					continue;
				}

				const size_t sourceLength = sourceEnd - sourceBegin;

				if (sourceLength != 0)
				{
					this->mCurrentLocation.Source = std::string(sourceBegin + 1, sourceLength - 1);
				}

				if (this->mCurrentLocation.Line != 0)
				{
					this->mCurrentLocation.Line--;
				}
			}
			else
			{
				while ((this->mPos[0] == '\n' || this->mPos[-1] == '\\') && this->mPos < this->mEnd)
				{
					continue;
				}
			}

			while (*this->mPos != '\n' && this->mPos < this->mEnd)
			{
				this->mPos++;
			}
		}
	}
}
