#pragma once

#include "ParserNodes.hpp"

namespace ReShade
{
	namespace FX
	{
		class Lexer
		{
		public:
			enum class TokenId
			{
				Unknown = -1,
				EndOfStream = 0,

				// Operators
				Exclaim = '!',
				Hash = '#',
				Dollar = '$',
				Percent = '%',
				Ampersand = '&',
				ParenthesisOpen = '(',
				ParenthesisClose = ')',
				Star = '*',
				Plus = '+',
				Comma = ',',
				Minus = '-',
				Dot = '.',
				Slash = '/',
				Colon = ':',
				Semicolon = ';',
				Less = '<',
				Equal = '=',
				Greater = '>',
				Question = '?',
				At = '@',
				BracketOpen = '[',
				BackSlash = '\\',
				BracketClose = ']',
				Caret = '^',
				BraceOpen = '{',
				Pipe = '|',
				BraceClose = '}',
				Tilde = '~',
				ExclaimEqual = 256 /* != */,
				PercentEqual /* %= */,
				AmpersandAmpersand /* && */,
				AmpersandEqual /* &= */,
				StarEqual /* *= */,
				PlusPlus /* ++*/,
				PlusEqual /* += */,
				MinusMinus /* -- */,
				MinusEqual /* -= */,
				Arrow /* -> */,
				Ellipsis /* ... */,
				SlashEqual /* /= */,
				ColonColon /* :: */,
				LessLessEqual /* <<= */,
				LessLess /* << */,
				LessEqual /* <= */,
				EqualEqual /* == */,
				GreaterGreaterEqual /* >>= */,
				GreaterGreater /* >> */,
				GreaterEqual /* >= */,
				CaretEqual /* ^= */,
				PipeEqual /* |= */,
				PipePipe /* || */,

				// Identifier
				Identifier,
				ReservedWord,

				// Literal
				True,
				False,
				IntLiteral,
				UintLiteral,
				FloatLiteral,
				DoubleLiteral,
				StringLiteral,

				// Keywords
				Namespace,
				Struct,
				Technique,
				Pass,
				For,
				While,
				Do,
				If,
				Else,
				Switch,
				Case,
				Default,
				Break,
				Continue,
				Return,
				Discard,
				Extern,
				Static,
				Uniform,
				Volatile,
				Precise,
				In,
				Out,
				InOut,
				Const,
				Linear,
				NoPerspective,
				Centroid,
				NoInterpolation,

				// Types
				Void,
				Bool,
				Bool2,
				Bool2x2,
				Bool3,
				Bool3x3,
				Bool4,
				Bool4x4,
				Int,
				Int2,
				Int2x2,
				Int3,
				Int3x3,
				Int4,
				Int4x4,
				Uint,
				Uint2,
				Uint2x2,
				Uint3,
				Uint3x3,
				Uint4,
				Uint4x4,
				Float,
				Float2,
				Float2x2,
				Float3,
				Float3x3,
				Float4,
				Float4x4,
				Vector,
				Matrix,
				String,
				Texture1D,
				Texture2D,
				Texture3D,
				Sampler1D,
				Sampler2D,
				Sampler3D,
			};
			struct Token
			{
				TokenId Id;
				Location Location;
				unsigned int Length;
				union
				{
					int LiteralAsInt;
					unsigned int LiteralAsUint;
					float LiteralAsFloat;
					double LiteralAsDouble;
				};
				std::string LiteralAsString;
			};

			/// <summary>
			/// Copies a lexical analyzer.
			/// </summary>
			/// <param name="lexer">The instance to copy.</param>
			Lexer(const Lexer &lexer);
			/// <summary>
			/// Construct new lexical analyzer for the given input <paramref name="source"/> string.
			/// </summary>
			/// <param name="source">The string to analyze.</param>
			explicit Lexer(const std::string &source);
			/// <summary>
			/// Copies a lexical analyzer.
			/// </summary>
			/// <param name="lexer">The instance to copy.</param>
			Lexer &operator=(const Lexer &lexer);

			/// <summary>
			/// Perform lexical analysis on the input string and return the next token in sequence.
			/// </summary>
			/// <returns>The next token in sequence.</returns>
			Token Lex();

		private:
			void SkipSpace();
			void SkipToNewLine();
			void ParseIdentifier(Token &token) const;
			void ParseStringLiteral(Token &token, bool escape) const;
			void ParseNumericLiteral(Token &token) const;
			void ParsePreProcessorDirective();

			std::string _source;
			const std::string::value_type *_cur, *_end;
			bool _atLineBegin;
			Location _location;
		};
	}
}
