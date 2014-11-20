/**
 * ReShade
 * Copyright (C) 2014, Crosire
 *
 * This file may only be used, modified, and distributed under the terms of the license specified
 * in License.txt. By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it completly.
 */

/*
 * Based on ANSI C Yacc grammar by Jeff Lee 1985, Tom Stockfisch 1987, Jutta Degener 1995
 */

/* ----------------------------------------------------------------------------------------------
|  Options																						|
---------------------------------------------------------------------------------------------- */

%pure-parser
%error-verbose
%locations

%code requires
{
	#include "EffectParserTree.hpp"
	
	#include <stack>
	#include <unordered_map>

	#define YYLTYPE ReShade::EffectTree::Location

	namespace ReShade
	{
		class EffectParser
		{
		public:
			typedef unsigned int Scope;

		public:
			EffectParser(EffectTree &ast);
			~EffectParser();

			inline bool Parse(const std::string &source)
			{
				std::string errors;
				return Parse(source, errors);
			}
			bool Parse(const std::string &source, std::string &errors);

			void Error(const YYLTYPE &location, unsigned int code, const char *message, ...);
			void Warning(const YYLTYPE &location, unsigned int code, const char *message, ...);

		public:
			inline const std::vector<std::string> &GetPragmas() const
			{
				return this->mPragmas;
			}
			Scope GetCurrentScope() const;
			EffectTree::Index GetCurrentParent() const;
			inline EffectTree::Index FindSymbol(const std::string &name) const
			{
				return FindSymbol(name, GetCurrentScope(), false);
			}
			EffectTree::Index FindSymbol(const std::string &name, Scope scope, bool exclusive = false) const;
			bool ResolveCall(EffectNodes::Call &call, bool &intrinsic, bool &ambiguous) const;
			
			void PushScope(EffectTree::Index parent = EffectTree::Null);
			bool PushSymbol(EffectTree::Index symbol);
			void PopScope();

		public:
			EffectTree &mAST;
			void *mLexer;
			void *mParser;
			int mNextLexerState;
			std::vector<std::string> mPragmas;

		private:
			std::string mErrors;
			unsigned int mErrorsCount;
			Scope mCurrentScope;
			std::stack<EffectTree::Index> mParentStack;
			std::unordered_map<std::string, std::vector<std::pair<Scope, EffectTree::Index>>> mSymbolStack;

		private:
			EffectParser(const EffectParser &);
		
			void operator =(const EffectParser &);
		};
	}
}
%code
{
	#include "EffectLexer.h"

	using namespace ReShade;

	// Bison Macros
	#define yyerror(yylloc, yyscanner, context, message, ...) parser.Error(*(yylloc), 3000, message, __VA_ARGS__)
	#define YYLLOC_DEFAULT(Current, Rhs, N) { if (N) { (Current).Source = YYRHSLOC(Rhs, 1).Source, (Current).Line = YYRHSLOC(Rhs, 1).Line, (Current).Column = YYRHSLOC(Rhs, 1).Column; } else { (Current).Source = nullptr, (Current).Line = YYRHSLOC(Rhs, 0).Line, (Current).Column = YYRHSLOC(Rhs, 0).Column; } }
}

%parse-param { void *yyscanner }
%parse-param { ReShade::EffectParser &parser }
%lex-param { void *yyscanner }

%initial-action
{
	@$.Source = nullptr;
	@$.Line = 1;
	@$.Column = 1;

	yyset_lineno(1, yyscanner);
	yyset_column(1, yyscanner);
	yyset_extra(&parser, yyscanner);

#if YYDEBUG
	yydebug = 1;
#endif
}

/* ----------------------------------------------------------------------------------------------
|  Value type																					|
---------------------------------------------------------------------------------------------- */

%union
{
	struct
	{
		union
		{
			bool Bool;
			int Int;
			unsigned int Uint;
			float Float;
			struct { const char *p; std::size_t len; } String;
		};

		ReShade::EffectTree::Index Node;
		ReShade::EffectNodes::Type Type;
	} l;
	struct
	{
		ReShade::EffectTree::Index Index, States[ReShade::EffectNodes::Pass::StateCount], Properties[ReShade::EffectNodes::Variable::PropertyCount];
	} y;
}

/* ----------------------------------------------------------------------------------------------
|  Tokens																						|
---------------------------------------------------------------------------------------------- */

%token			TOK_EOF 0										"end of file"

%token<l>		TOK_TECHNIQUE									"technique"
%token<l>		TOK_PASS										"pass"
%token<l>		TOK_STRUCT										"struct"

%token<l>		TOK_FOR											"for"
%token<l>		TOK_WHILE										"while"
%token<l>		TOK_DO											"do"
%token<l>		TOK_IF											"if"
%token<l>		TOK_ELSE										"else"
%token<l>		TOK_SWITCH										"switch"
%token<l>		TOK_CASE										"case"
%token<l>		TOK_DEFAULT										"default"
%token<l>		TOK_BREAK										"break"
%token<l>		TOK_CONTINUE									"continue"
%token<l>		TOK_RETURN										"return"
%token<l>		TOK_DISCARD										"discard"

%token<l>		TOK_EXTERN										"extern"
%token<l>		TOK_STATIC										"static"
%token<l>		TOK_UNIFORM										"uniform"
%token<l>		TOK_VOLATILE									"volatile"
%token<l>		TOK_PRECISE										"precise"
%token<l>		TOK_IN											"in"
%token<l>		TOK_OUT											"out"
%token<l>		TOK_INOUT										"inout"
%token<l>		TOK_CONST										"const"
%token<l>		TOK_NOINTERPOLATION								"nointerpolation"
%token<l>		TOK_NOPERSPECTIVE								"noperspective"
%token<l>		TOK_LINEAR										"linear"
%token<l>		TOK_CENTROID									"centroid"
%token<l>		TOK_SAMPLE										"sample"

%token<l>		TOK_LITERAL_BOOL								"boolean literal"
%token<l>		TOK_LITERAL_INT									"integral literal"
%token<l>		TOK_LITERAL_FLOAT								"floating point literal"
%token<l>		TOK_LITERAL_ENUM								"enumeration"
%token<l>		TOK_LITERAL_STRING								"string literal"

%token<l>		TOK_IDENTIFIER									"identifier"
%token<l>		TOK_IDENTIFIER_TYPE								"type name"
%token<l>		TOK_IDENTIFIER_SYMBOL							"symbol name"
%token<l>		TOK_IDENTIFIER_FIELD							"member field"
%token<l>		TOK_IDENTIFIER_PROPERTY							"variable property"
%token<l>		TOK_IDENTIFIER_PASSSTATE						"pass state"
%token<l>		TOK_IDENTIFIER_SEMANTIC							"semantic"

%token<l>		TOK_TYPE_VOID									"void"
%token<l>		TOK_TYPE_BOOL									"bool"
%token<l>		TOK_TYPE_BOOLV									"boolN"
%token<l>		TOK_TYPE_BOOLM									"boolNxN"
%token<l>		TOK_TYPE_INT									"int"
%token<l>		TOK_TYPE_INTV									"intN"
%token<l>		TOK_TYPE_INTM									"intNxN"
%token<l>		TOK_TYPE_UINT									"uint"
%token<l>		TOK_TYPE_UINTV									"uintN"
%token<l>		TOK_TYPE_UINTM									"uintNxN"
%token<l>		TOK_TYPE_FLOAT									"float"
%token<l>		TOK_TYPE_FLOATV									"floatN"
%token<l>		TOK_TYPE_FLOATM									"floatNxN"
%token<l>		TOK_TYPE_TEXTURE								"texture"
%token<l>		TOK_TYPE_SAMPLER								"sampler"
%token<l>		TOK_TYPE_STRING									"string"
%token<l>		TOK_TYPE_VECTOR									"vector"
%token<l>		TOK_TYPE_MATRIX									"matrix"

%token<l>		TOK_ELLIPSIS									"..."
%token<l>		TOK_DOT 46										"."
%token<l>		TOK_COLON 58									":"
%token<l>		TOK_COMMA 44									","
%token<l>		TOK_SEMICOLON 59								";"
%token<l>		TOK_QUESTION 63									"?"

%token<l>		TOK_BRACKET_OPEN 40								"("
%token<l>		TOK_BRACKET_CLOSE 41							")"
%token<l>		TOK_CURLYBRACKET_OPEN 123						"{"
%token<l>		TOK_CURLYBRACKET_CLOSE 125						"}"
%token<l>		TOK_SQUAREBRACKET_OPEN 91						"["
%token<l>		TOK_SQUAREBRACKET_CLOSE 93						"]"

%token<l>		TOK_OPERATOR_EQUAL								"=="
%token<l>		TOK_OPERATOR_NOTEQUAL							"!="
%token<l>		TOK_OPERATOR_LESSEQUAL							"<="
%token<l>		TOK_OPERATOR_GREATEREQUAL						">="
%token<l>		TOK_OPERATOR_LESS 60							"<"
%token<l>		TOK_OPERATOR_GREATER 62							">"
%token<l>		TOK_OPERATOR_ASSIGNMENT 61						"="
%token<l>		TOK_OPERATOR_PLUS 43							"+"
%token<l>		TOK_OPERATOR_MINUS 45							"-"
%token<l>		TOK_OPERATOR_MULTIPLY 42						"*"
%token<l>		TOK_OPERATOR_DIVIDE 47							"/"
%token<l>		TOK_OPERATOR_MODULO 37							"%"
%token<l>		TOK_OPERATOR_BITNOT 126							"~"
%token<l>		TOK_OPERATOR_LOGICNOT 33						"!"
%token<l>		TOK_OPERATOR_BITAND 38							"&"
%token<l>		TOK_OPERATOR_BITXOR 94							"^"
%token<l>		TOK_OPERATOR_BITOR 124							"|"
%token<l>		TOK_OPERATOR_INCREMENT							"++"
%token<l>		TOK_OPERATOR_DECREMENT							"--"
%token<l>		TOK_OPERATOR_LEFTSHIFT							"<<"
%token<l>		TOK_OPERATOR_RIGHTSHIFT							">>"
%token<l>		TOK_OPERATOR_LOGICAND							"&&"
%token<l>		TOK_OPERATOR_LOGICXOR							"^^"
%token<l>		TOK_OPERATOR_LOGICOR							"||"
%token<l>		TOK_OPERATOR_INCREMENTASSIGN					"+="
%token<l>		TOK_OPERATOR_DECREMENTASSIGN					"-="
%token<l>		TOK_OPERATOR_MULTIPLYASSIGN						"*="
%token<l>		TOK_OPERATOR_DIVIDEASSIGN						"/="
%token<l>		TOK_OPERATOR_MODULOASSIGN						"%="
%token<l>		TOK_OPERATOR_BITANDASSIGN						"&="
%token<l>		TOK_OPERATOR_BITXORASSIGN						"^="
%token<l>		TOK_OPERATOR_BITORASSIGN						"|="
%token<l>		TOK_OPERATOR_LEFTSHIFTASSIGN					"<<="
%token<l>		TOK_OPERATOR_RIGHTSHIFTASSIGN					">>="

%right			TOK_THEN TOK_ELSE

/* ----------------------------------------------------------------------------------------------
|  Rules																						|
---------------------------------------------------------------------------------------------- */

%type<l>		RULE_IDENTIFIER_SYMBOL
%type<y.Index>	RULE_IDENTIFIER_FUNCTION
%type<l>		RULE_IDENTIFIER_NAME
%type<l>		RULE_IDENTIFIER_SEMANTIC
%type<l>		RULE_IDENTIFIER_PASSSTATE

%type<l>		RULE_TYPE
%type<l>		RULE_TYPE_SCALAR
%type<l>		RULE_TYPE_VECTOR
%type<l>		RULE_TYPE_MATRIX
%type<l>		RULE_TYPE_SINGLE

%type<l>		RULE_TYPE_STORAGE
%type<l>		RULE_TYPE_STORAGE_PARAMETER
%type<l>		RULE_TYPE_MODIFIER
%type<l>		RULE_TYPE_INTERPOLATION
%type<l>		RULE_TYPE_QUALIFIER
%type<l>		RULE_TYPE_ARRAY
%type<l>		RULE_TYPE_FULLYSPECIFIED

%type<l>		RULE_OPERATOR_POSTFIX
%type<l>		RULE_OPERATOR_UNARY
%type<l>		RULE_OPERATOR_MULTIPLICATIVE
%type<l>		RULE_OPERATOR_ADDITIVE
%type<l>		RULE_OPERATOR_SHIFT
%type<l>		RULE_OPERATOR_RELATIONAL
%type<l>		RULE_OPERATOR_EQUALITY
%type<l>		RULE_OPERATOR_ASSIGNMENT

%type<y.Index>	RULE_EXPRESSION
%type<y.Index>	RULE_EXPRESSION_LITERAL
%type<y.Index>	RULE_EXPRESSION_PRIMARY
%type<y.Index>	RULE_EXPRESSION_FUNCTION
%type<y.Index>	RULE_EXPRESSION_FUNCTION_ARGUMENTS
%type<y.Index>	RULE_EXPRESSION_POSTFIX
%type<y.Index>	RULE_EXPRESSION_UNARY
%type<y.Index>	RULE_EXPRESSION_MULTIPLICATIVE
%type<y.Index>	RULE_EXPRESSION_ADDITIVE
%type<y.Index>	RULE_EXPRESSION_SHIFT
%type<y.Index>	RULE_EXPRESSION_RELATIONAL
%type<y.Index>	RULE_EXPRESSION_EQUALITY
%type<y.Index>	RULE_EXPRESSION_BITAND
%type<y.Index>	RULE_EXPRESSION_BITXOR
%type<y.Index>	RULE_EXPRESSION_BITOR
%type<y.Index>	RULE_EXPRESSION_LOGICAND
%type<y.Index>	RULE_EXPRESSION_LOGICXOR
%type<y.Index>	RULE_EXPRESSION_LOGICOR
%type<y.Index>	RULE_EXPRESSION_CONDITIONAL
%type<y.Index>	RULE_EXPRESSION_ASSIGNMENT
%type<y.Index>  RULE_EXPRESSION_INITIALIZER
%type<y.Index>  RULE_EXPRESSION_INITIALIZER_LIST

%type<y.Index>	RULE_STATEMENT
%type<y.Index>	RULE_STATEMENT_SCOPELESS
%type<y.Index>	RULE_STATEMENT_LIST
%type<y.Index>	RULE_STATEMENT_BLOCK
%type<y.Index>	RULE_STATEMENT_BLOCK_SCOPELESS
%type<y.Index>	RULE_STATEMENT_SINGLE
%type<y.Index>	RULE_STATEMENT_EXPRESSION
%type<y.Index>	RULE_STATEMENT_DECLARATION
%type<y.Index>	RULE_STATEMENT_IF
%type<y.Index>	RULE_STATEMENT_SWITCH
%type<y.Index>	RULE_STATEMENT_CASE
%type<y.Index>	RULE_STATEMENT_CASE_LIST
%type<y.Index>	RULE_STATEMENT_CASE_LABEL
%type<y.Index>	RULE_STATEMENT_CASE_LABEL_LIST
%type<y.Index>	RULE_STATEMENT_FOR
%type<y.Index>	RULE_STATEMENT_FOR_INITIALIZER
%type<y.Index>	RULE_STATEMENT_FOR_CONDITION
%type<y.Index>	RULE_STATEMENT_FOR_ITERATOR
%type<y.Index>	RULE_STATEMENT_WHILE
%type<y.Index>	RULE_STATEMENT_JUMP

%type<l>		RULE_ATTRIBUTE
%type<l>		RULE_ATTRIBUTE_LIST
%type<l>		RULE_ATTRIBUTES

%type<l>		RULE_SEMANTICS

%type<y.Index>	RULE_ANNOTATION
%type<y.Index>	RULE_ANNOTATION_LIST
%type<y.Index>	RULE_ANNOTATIONS

%type<y.Index>	RULE_STRUCT
%type<y.Index>	RULE_STRUCT_FIELD
%type<y.Index>	RULE_STRUCT_FIELD_LIST
%type<y.Index>	RULE_STRUCT_FIELD_DECLARATOR
%type<y.Index>	RULE_STRUCT_FIELD_DECLARATOR_LIST

%type<y.Index>	RULE_VARIABLE
%type<y.Index>	RULE_VARIABLE_DECLARATOR
%type<y.Index>	RULE_VARIABLE_DECLARATOR_LIST
%type<y>		RULE_VARIABLE_PROPERTY
%type<y>		RULE_VARIABLE_PROPERTY_LIST

%type<y.Index>	RULE_FUNCTION_PROTOTYPE
%type<y.Index>	RULE_FUNCTION_DEFINITION
%type<y.Index>	RULE_FUNCTION_DECLARATOR
%type<y.Index>	RULE_FUNCTION_PARAMETER
%type<y.Index>	RULE_FUNCTION_PARAMETER_LIST
%type<y.Index>	RULE_FUNCTION_PARAMETER_DECLARATOR

%type<y.Index>	RULE_TECHNIQUE

%type<y.Index>	RULE_PASS
%type<y.Index>	RULE_PASS_LIST

%type<y>		RULE_PASSSTATE
%type<y>		RULE_PASSSTATE_LIST

%type<y.Index>	RULE_MAIN_DECLARATION

%start			RULE_START

%%

 /* Translation Unit ------------------------------------------------------------------------- */

RULE_START
	:
	| RULE_MAIN
	| error
	{
		YYABORT;
	}
RULE_MAIN
	: RULE_MAIN_DECLARATION
	{
		EffectNodes::Root &root = parser.mAST.Get().As<EffectNodes::Root>();
		root.NextDeclaration = $1;
	}
	| RULE_MAIN RULE_MAIN_DECLARATION
	{
		EffectNodes::Root *root = &parser.mAST.Get().As<EffectNodes::Root>();

		while (root->NextDeclaration != EffectTree::Null)
		{
			root = &parser.mAST[root->NextDeclaration].As<EffectNodes::Root>();
		}

		root->NextDeclaration = $2;
	}
	;
RULE_MAIN_DECLARATION
	: RULE_STRUCT ";"
	| RULE_VARIABLE ";"
	| RULE_FUNCTION_PROTOTYPE ";"
	| RULE_FUNCTION_DEFINITION
	| RULE_TECHNIQUE
	| ";" { $$ = EffectTree::Null; }
	;

  /* Identifiers ----------------------------------------------------------------------------- */

RULE_IDENTIFIER_SYMBOL
	: TOK_IDENTIFIER_SYMBOL
	| TOK_IDENTIFIER
	{
		if ($1.String.len > 2 && ($1.String.p[0] == '_' && $1.String.p[1] == '_' && $1.String.p[2] != '_'))
		{
			parser.Warning(@1, 3000, "names starting with '__' are reserved");
		}

		@$ = @1, $$ = $1;
	}
	;
RULE_IDENTIFIER_NAME
	: RULE_IDENTIFIER_SYMBOL
	;
RULE_IDENTIFIER_FUNCTION
	: RULE_TYPE
	{
		if (!$1.Type.IsNumeric())
		{
			parser.Error(@1, 3037, "constructors only defined for numeric base types");
			YYERROR;
		}

		EffectNodes::Constructor &node = parser.mAST.Add<EffectNodes::Constructor>(@1);
		node.Type = $1.Type;
		node.Type.Qualifiers = EffectNodes::Type::Const;

		@$ = @1, $$ = node.Index;
	}
	| RULE_IDENTIFIER_SYMBOL
	{
		EffectNodes::Call &node = parser.mAST.Add<EffectNodes::Call>(@1);
		node.CalleeName = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Callee = $1.Node;

		@$ = @1, $$ = node.Index;
	}
	| TOK_IDENTIFIER_FIELD
	{
		EffectNodes::Call &node = parser.mAST.Add<EffectNodes::Call>(@1);
		node.CalleeName = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @1, $$ = node.Index;
	}
	;
RULE_IDENTIFIER_SEMANTIC
	: TOK_IDENTIFIER_SEMANTIC { @$ = @1, $$.String = $1.String; }
	| TOK_IDENTIFIER { @$ = @1, $$.String = $1.String; }
	;
RULE_IDENTIFIER_PASSSTATE
	: TOK_IDENTIFIER_PASSSTATE
	| TOK_IDENTIFIER
	{
		parser.Error(@1, 3000, "unrecognized pass state '%.*s'", $1.String.len, $1.String.p);
		YYERROR;
	}
	;

 /* Types ------------------------------------------------------------------------------------ */

RULE_TYPE
	: "void"
	| RULE_TYPE_SCALAR
	| RULE_TYPE_VECTOR
	| RULE_TYPE_MATRIX
	{
		@$ = @1;
		$$.Type = $1.Type;
	}
	| "texture"
	| "sampler"
	| TOK_IDENTIFIER_TYPE
	| RULE_STRUCT
	{
		@$ = @1;
		$$.Type.Class = EffectNodes::Type::Struct;
		$$.Type.Qualifiers = 0;
		$$.Type.Rows = 0;
		$$.Type.Cols = 0;
		$$.Type.ArrayLength = 0;
		$$.Type.Definition = $1;
	}
	;
RULE_TYPE_SCALAR
	: "bool"
	| "int"
	| "uint"
	| "float"
	;
RULE_TYPE_VECTOR
	: "boolN"
	| "intN"
	| "uintN"
	| "floatN"
	| "vector"
	| "vector" "<" RULE_TYPE "," TOK_LITERAL_INT ">"
	{
		if (!$3.Type.IsScalar())
		{
			parser.Error(@3, 3122, "vector element type must be a scalar type");
			YYERROR;
		}
		if ($5.Int < 1 || $5.Int > 4)
		{
			parser.Error(@5, 3052, "vector dimension must be between 1 and 4");
			YYERROR;
		}

		@$ = @1;
		$$.Type = $3.Type;
		$$.Type.Rows = $5.Uint;
	}
	;
RULE_TYPE_MATRIX
	: "boolNxN"
	| "intNxN"
	| "uintNxN"
	| "floatNxN"
	| "matrix"
	| "matrix" "<" RULE_TYPE "," TOK_LITERAL_INT "," TOK_LITERAL_INT ">"
	{
		if (!$3.Type.IsScalar())
		{
			parser.Error(@3, 3123, "matrix element type must be a scalar type");
			YYERROR;
		}
		if ($5.Int < 1 || $5.Int > 4)
		{
			parser.Error(@5, 3053, "matrix dimensions must be between 1 and 4");
			YYERROR;
		}
		if ($7.Int < 1 || $7.Int > 4)
		{
			parser.Error(@7, 3053, "matrix dimensions must be between 1 and 4");
			YYERROR;
		}

		@$ = @1;
		$$.Type = $3.Type;
		$$.Type.Rows = $5.Uint;
		$$.Type.Cols = $7.Uint;
	}
	;
RULE_TYPE_SINGLE
	: RULE_TYPE_SCALAR
	| "string"
	;
 
RULE_TYPE_STORAGE
	: "extern"
	| "static"
	| "uniform"
	| "volatile"
	| "precise"
	;
RULE_TYPE_STORAGE_PARAMETER
	: "in"
	| "out"
	| "inout"
	;
RULE_TYPE_MODIFIER
	: "const"
	;
RULE_TYPE_INTERPOLATION
	: "nointerpolation"
	| "noperspective"
	| "linear"
	| "centroid"
	| "sample"
	;
RULE_TYPE_QUALIFIER
	: RULE_TYPE_STORAGE
	{
		@$ = @1, $$.Type.Qualifiers = $1.Uint;
	}
	| RULE_TYPE_STORAGE_PARAMETER
	{
		@$ = @1, $$.Type.Qualifiers = $1.Uint;
	}
	| RULE_TYPE_MODIFIER
	{
		@$ = @1, $$.Type.Qualifiers = $1.Uint;
	}
	| RULE_TYPE_INTERPOLATION
	{
		@$ = @1, $$.Type.Qualifiers = $1.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE
	{
		if (($1.Type.Qualifiers & $2.Uint) == $2.Uint)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE_PARAMETER
	{
		if (($1.Type.Qualifiers & $2.Uint) == $2.Uint)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_MODIFIER
	{
		if (($1.Type.Qualifiers & $2.Uint) == $2.Uint)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_INTERPOLATION
	{
		if (($1.Type.Qualifiers & $2.Uint) == $2.Uint)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	;

RULE_TYPE_ARRAY
	:
	{
		$$.Type.ArrayLength = 0;
	}
	| "[" "]"
	{
		@$ = @1, $$.Type.ArrayLength = -1;
	}
	| "[" RULE_EXPRESSION_CONDITIONAL "]"
	{
		if (!parser.mAST[$2].Is<EffectNodes::Literal>() || !parser.mAST[$2].As<EffectNodes::RValue>().Type.IsScalar() || !parser.mAST[$2].As<EffectNodes::RValue>().Type.IsIntegral())
		{
			parser.Error(@2, 3058, "array dimensions must be literal scalar expressions");
			YYERROR;
		}

		@$ = @1, $$.Type.ArrayLength = parser.mAST[$2].As<EffectNodes::Literal>().Value.Int[0];
		
		if ($$.Type.ArrayLength < 1 || $$.Type.ArrayLength > 65536)
		{
			parser.Error(@2, 3059, "array dimension must be between 1 and 65536");
			YYERROR;
		}
	}
	;
	
RULE_TYPE_FULLYSPECIFIED
	: RULE_TYPE
	| RULE_TYPE_QUALIFIER RULE_TYPE
	{
		if ($2.Type.IsIntegral() && ($1.Type.HasQualifier(EffectNodes::Type::Centroid) || $1.Type.HasQualifier(EffectNodes::Type::NoPerspective) || $1.Type.HasQualifier(EffectNodes::Type::Sample)))
		{
			parser.Error(@1, 4576, "signature specifies invalid interpolation mode for integer component type");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::Centroid) && !$1.Type.HasQualifier(EffectNodes::Type::NoPerspective))
		{
			$1.Type.Qualifiers |= EffectNodes::Type::Linear;
		}

		@$ = @1;
		$$.Type = $2.Type;
		$$.Type.Qualifiers |= $1.Type.Qualifiers;
	}
	;

 /* Operators -------------------------------------------------------------------------------- */

RULE_OPERATOR_POSTFIX
	: "++"
	| "--"
	;
RULE_OPERATOR_UNARY
	: "+" { @$ = @1, $$.Uint = 0; }
	| "-" { @$ = @1, $$.Uint = EffectNodes::Expression::Negate; }
	| "~"
	| "!"
	;
RULE_OPERATOR_MULTIPLICATIVE
	: "*"
	| "/"
	| "%"
	;
RULE_OPERATOR_ADDITIVE
	: "+"
	| "-"
	;
RULE_OPERATOR_SHIFT
	: "<<"
	| ">>"
	;
RULE_OPERATOR_RELATIONAL
	: "<"
	| ">"
	| "<="
	| ">="
	;
RULE_OPERATOR_EQUALITY
	: "=="
	| "!="
	;
RULE_OPERATOR_ASSIGNMENT
	: "="
	| "+="
	| "-="
	| "*="
	| "/="
	| "%="
	| "&="
	| "^="
	| "|="
	| "<<="
	| ">>="
	;

 /* Expressions ------------------------------------------------------------------------------ */

RULE_EXPRESSION
	: RULE_EXPRESSION_ASSIGNMENT
	| RULE_EXPRESSION "," RULE_EXPRESSION_ASSIGNMENT
	{
		EffectNodes::Sequence &node = parser.mAST[$1].Is<EffectNodes::Sequence>() ? parser.mAST[$1].As<EffectNodes::Sequence>() : parser.mAST.Add<EffectNodes::Sequence>(@1);

		if (node.Expressions == EffectTree::Null)
		{
			node.Expressions = $1;
		}

		EffectNodes::RValue *expression = &parser.mAST[node.Expressions].As<EffectNodes::RValue>();

		while (expression->NextExpression != EffectTree::Null)
		{
			expression = &parser.mAST[expression->NextExpression].As<EffectNodes::RValue>();
		}

		expression->NextExpression = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_LITERAL
	: TOK_LITERAL_BOOL
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Bool;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Bool[0] = $1.Int;

		@$ = @1, $$ = node.Index;
	}
	| TOK_LITERAL_INT
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Int;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Int[0] = $1.Int;

		@$ = @1, $$ = node.Index;
	}
	| TOK_LITERAL_ENUM
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Int;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Int[0] = $1.Int;

		@$ = @1, $$ = node.Index;
	}
	| TOK_LITERAL_FLOAT
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Float;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Float[0] = $1.Float;

		@$ = @1, $$ = node.Index;
	}
	| TOK_LITERAL_STRING
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::String;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Value.String = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_PRIMARY
	: RULE_EXPRESSION_LITERAL
	| RULE_IDENTIFIER_SYMBOL
	{
		if ($1.Node == EffectTree::Null)
		{
			parser.Error(@1, 3004, "undeclared identifier '%.*s'", $1.String.len, $1.String.p);
			YYERROR;
		}
		else if (parser.mAST[$1.Node].Is<EffectNodes::Function>())
		{
			parser.Error(@1, 3005, "identifier '%.*s' represents a function, not a variable", $1.String.len, $1.String.p);
			YYERROR;
		}

		EffectNodes::LValue &node = parser.mAST.Add<EffectNodes::LValue>(@1);
		node.Type = parser.mAST[$1.Node].As<EffectNodes::Variable>().Type;
		node.Reference = $1.Node;

		@$ = @1, $$ = node.Index;
	}
	| "(" RULE_EXPRESSION ")"
	{
		@$ = @2, $$ = $2;
	}
	;
RULE_EXPRESSION_FUNCTION
	: RULE_IDENTIFIER_FUNCTION "(" ")"
	{
		if (parser.mAST[$$].Is<EffectNodes::Constructor>())
		{
			parser.Error(@1, 3014, "incorrect number of arguments to numeric-type constructor");
			YYERROR;
		}

		@$ = @1, $$ = $1;
	}
	| RULE_IDENTIFIER_FUNCTION "(" RULE_EXPRESSION_FUNCTION_ARGUMENTS ")"
	{
		@$ = @1, $$ = $1;

		unsigned int argumentCount = 1;
		const EffectNodes::RValue *argument = &parser.mAST[$3].As<EffectNodes::RValue>();

		while (argument->NextExpression != EffectTree::Null)
		{
			argument = &parser.mAST[argument->NextExpression].As<EffectNodes::RValue>();
			argumentCount++;
		}

		if (parser.mAST[$$].Is<EffectNodes::Call>())
		{
			parser.mAST[$$].As<EffectNodes::Call>().Arguments = $3;
			parser.mAST[$$].As<EffectNodes::Call>().ArgumentCount = argumentCount;
		}
		else if (parser.mAST[$$].Is<EffectNodes::Constructor>())
		{
			parser.mAST[$$].As<EffectNodes::Constructor>().Arguments = $3;
			parser.mAST[$$].As<EffectNodes::Constructor>().ArgumentCount = argumentCount;
		}
	}
	;
RULE_EXPRESSION_FUNCTION_ARGUMENTS
	: RULE_EXPRESSION_ASSIGNMENT
	| RULE_EXPRESSION_FUNCTION_ARGUMENTS "," RULE_EXPRESSION_ASSIGNMENT
	{
		EffectNodes::RValue *node = &parser.mAST[$1].As<EffectNodes::RValue>();

		while (node->NextExpression != EffectTree::Null)
		{
			node = &parser.mAST[node->NextExpression].As<EffectNodes::RValue>();
		}

		node->NextExpression = $3;

		@$ = @1, $$ = $1;
	}
	;
RULE_EXPRESSION_POSTFIX
	: RULE_EXPRESSION_PRIMARY
	| RULE_EXPRESSION_FUNCTION
	{
		if (parser.mAST[$1].Is<EffectNodes::Call>())
		{
			EffectNodes::Call &call = parser.mAST[$1].As<EffectNodes::Call>();
			
			bool undeclared = call.Callee == EffectTree::Null, intrinsic = false, ambiguous = false;

			if (!parser.ResolveCall(call, intrinsic, ambiguous))
			{
				if (undeclared && !intrinsic)
				{
					parser.Error(@1, 3004, "undeclared identifier '%s'", call.CalleeName);
				}
				else if (ambiguous)
				{
					parser.Error(@1, 3067, "ambiguous function call to '%s'", call.CalleeName);
				}
				else
				{
					parser.Error(@1, 3013, "no matching function overload for '%s'", call.CalleeName);
				}
				YYERROR;
			}
			
			if (intrinsic)
			{
				const EffectNodes::Type type = call.Type;
				const EffectTree::Index callee = call.Callee, arguments = call.Arguments;

				EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
				node.Type = type;
				node.Operator = static_cast<unsigned int>(callee);
				node.Operands[0] = arguments;
				
				if (node.Operands[0] != EffectTree::Null)
				{
					node.Operands[1] = parser.mAST[node.Operands[0]].As<EffectNodes::RValue>().NextExpression;
				}
				if (node.Operands[1] != EffectTree::Null)
				{
					node.Operands[2] = parser.mAST[node.Operands[1]].As<EffectNodes::RValue>().NextExpression;
				}

				@$ = @1, $$ = node.Index;
			}
			else
			{
				@$ = @1, $$ = $1;
			}
		}
		else if (parser.mAST[$1].Is<EffectNodes::Constructor>())
		{
			EffectNodes::Constructor &constructor = parser.mAST[$1].As<EffectNodes::Constructor>();

			bool literal = true;
			unsigned int elements = 0;
			const EffectNodes::Type type = constructor.Type;
			const EffectTree::Index arguments = constructor.Arguments;
			const EffectNodes::RValue *argument = &parser.mAST[arguments].As<EffectNodes::RValue>();

			do
			{
				if (!argument->Type.IsNumeric())
				{
					parser.Error(argument->Location, 3017, "cannot convert non-numeric types");
					YYERROR;
				}

				if (!argument->Is<EffectNodes::Literal>())
				{
					literal = false;
				}

				elements += argument->Type.Rows * argument->Type.Cols;

				if (argument->NextExpression != EffectTree::Null)
				{
					argument = &parser.mAST[argument->NextExpression].As<EffectNodes::RValue>();
				}
				else
				{
					argument = nullptr;
				}
			}
			while (argument != nullptr);

			if (elements != constructor.Type.Rows * constructor.Type.Cols)
			{
				parser.Error(@1, 3014, "incorrect number of arguments to numeric-type constructor");
				YYERROR;
			}

			if (literal)
			{
				EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
				node.Type = type;

				unsigned int k = 0;
				const EffectNodes::Literal *argument = &parser.mAST[arguments].As<EffectNodes::Literal>();

				do
				{
					for (unsigned int j = 0; j < argument->Type.Rows * argument->Type.Cols; ++k, ++j)
					{
						EffectNodes::Literal::Cast(*argument, k, node, j);
					}

					if (argument->NextExpression != EffectTree::Null)
					{
						argument = &parser.mAST[argument->NextExpression].As<EffectNodes::Literal>();
					}
					else
					{
						argument = nullptr;
					}
				}
				while (argument != nullptr);

				@$ = @1, $$ = node.Index;
			}
			else if (constructor.ArgumentCount == 1)
			{
				EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
				node.Type = type;
				node.Operator = EffectNodes::Expression::Cast;
				node.Operands[0] = arguments;

				@$ = @1, $$ = node.Index;
			}
			else
			{
				@$ = @1, $$ = $1;
			}
		}
	}
	| RULE_EXPRESSION_POSTFIX RULE_OPERATOR_POSTFIX
	{
		const EffectNodes::Type type = parser.mAST[$1].As<EffectNodes::RValue>().Type;

		if (!type.IsScalar() && !type.IsVector() && !type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (type.HasQualifier(EffectNodes::Type::Const) || type.HasQualifier(EffectNodes::Type::Uniform))
		{
			parser.Error(@1, 3025, "l-value specifies const object");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = type;
		node.Operator = $2.Uint + (EffectNodes::Expression::PostIncrease - EffectNodes::Expression::Increase);
		node.Operands[0] = $1;

		@$ = @1, $$ = node.Index;
	}
	| RULE_EXPRESSION_POSTFIX "[" RULE_EXPRESSION "]"
	{
		const EffectNodes::Type type = parser.mAST[$1].As<EffectNodes::RValue>().Type, indexType = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!type.IsArray() && !type.IsVector() && !type.IsMatrix())
		{
			parser.Error(@1, 3121, "array, matrix, vector, or indexable object type expected in index expression");
			YYERROR;
		}
		if (!indexType.IsScalar())
		{
			parser.Error(@3, 3120, "invalid type for index - index must be a scalar");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = type;
		node.Operator = EffectNodes::Expression::Extract;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		if (type.IsArray())
		{
			node.Type.ArrayLength = 0;
		}
		else if (type.IsMatrix())
		{
			node.Type.Rows = node.Type.Cols;
			node.Type.Cols = 1;
		}
		else if (type.IsVector())
		{
			node.Type.Rows = 1;
		}

		@$ = @1, $$ = node.Index;
	}
	| RULE_EXPRESSION_POSTFIX "." RULE_EXPRESSION_FUNCTION
	{
		const EffectNodes::Type type = parser.mAST[$1].As<EffectNodes::RValue>().Type;

		if (!type.IsStruct() || type.IsArray())
		{
			parser.Error(@2, 3087, "object does not have methods");
			YYERROR;
		}
		else
		{
			parser.Error(@2, 3088, "structures do not have methods");
			YYERROR;
		}
	}
	| RULE_EXPRESSION_POSTFIX "." TOK_IDENTIFIER_FIELD
	{
		const EffectNodes::Type type = parser.mAST[$1].As<EffectNodes::RValue>().Type;

		if (type.IsArray())
		{
			parser.Error(@3, 3018, "invalid subscript on array");
			YYERROR;
		}
		else if (type.IsVector())
		{
			if ($3.String.len > 4)
			{
				parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
				YYERROR;
			}

			bool constant = false;
			int offsets[4] = { -1, -1, -1, -1 };
			enum { xyzw, rgba, stpq } set[4];

			for (std::size_t i = 0; i < $3.String.len; ++i)
			{
				switch ($3.String.p[i])
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
						parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
						YYERROR;
				}

				const int offset = offsets[i];

				if (i > 0 && (set[i] != set[i - 1]))
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s', mixed swizzle sets", $3.String.len, $3.String.p);
					YYERROR;
				}
				if (static_cast<unsigned int>(offset) >= type.Rows)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s', swizzle out of range", $3.String.len, $3.String.p);
					YYERROR;
				}
		
				for (std::size_t k = 0; k < i; ++k)
				{
					if (offsets[k] == offset)
					{
						constant = true;
						break;
					}
				}
			}

			EffectNodes::Swizzle &node = parser.mAST.Add<EffectNodes::Swizzle>(@2);
			node.Type = type;
			node.Type.Rows = static_cast<unsigned int>($3.String.len);
			node.Operator = EffectNodes::Expression::Swizzle;
			node.Operands[0] = $1;
			node.Mask[0] = offsets[0] & 0xFF, node.Mask[1] = offsets[1] & 0xFF, node.Mask[2] = offsets[2] & 0xFF, node.Mask[3] = offsets[3] & 0xFF;

			if (constant || type.HasQualifier(EffectNodes::Type::Uniform))
			{
				node.Type.Qualifiers &= ~EffectNodes::Type::Uniform;
				node.Type.Qualifiers |= EffectNodes::Type::Const;
			}

			@$ = @1, $$ = node.Index;
		}
		else if (type.IsMatrix())
		{
			if ($3.String.len < 3)
			{
				parser.Error(@3, 3018, "invalid subscript '%s'", $3.String.p);
				YYERROR;
			}

			bool constant = false;
			int offsets[4] = { -1, -1, -1, -1 };
			const unsigned int set = $3.String.p[1] == 'm';
			const char coefficient = static_cast<char>(!set);

			for (std::size_t i = 0, j = 0; i < $3.String.len; i += 3 + set, ++j)
			{
				if ($3.String.p[i] != '_' || $3.String.p[i + set + 1] < '0' + coefficient || $3.String.p[i + set + 1] > '3' + coefficient || $3.String.p[i + set + 2] < '0' + coefficient || $3.String.p[i + set + 2] > '3' + coefficient)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
					YYERROR;
				}
				if (set && $3.String.p[i + 1] != 'm')
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s', mixed swizzle sets", $3.String.len, $3.String.p);
					YYERROR;
				}

				const unsigned int row = $3.String.p[i + set + 1] - '0' - coefficient;
				const unsigned int col = $3.String.p[i + set + 2] - '0' - coefficient;

				if ((row >= type.Rows || col >= type.Cols) || j > 3)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s', swizzle out of range", $3.String.len, $3.String.p);
					YYERROR;
				}

				const int offset = offsets[j] = row * 4 + col;
		
				for (std::size_t k = 0; k < j; ++k)
				{
					if (offsets[k] == offset)
					{
						constant = true;
						break;
					}
				}
			}

			EffectNodes::Swizzle &node = parser.mAST.Add<EffectNodes::Swizzle>(@2);
			node.Type = type;
			node.Type.Rows = static_cast<unsigned int>($3.String.len / (3 + set));
			node.Type.Cols = 1;
			node.Operator = EffectNodes::Expression::Swizzle;
			node.Operands[0] = $1;
			node.Mask[0] = offsets[0] & 0xFF, node.Mask[1] = offsets[1] & 0xFF, node.Mask[2] = offsets[2] & 0xFF, node.Mask[3] = offsets[3] & 0xFF;

			if (constant || type.HasQualifier(EffectNodes::Type::Uniform))
			{
				node.Type.Qualifiers &= ~EffectNodes::Type::Uniform;
				node.Type.Qualifiers |= EffectNodes::Type::Const;
			}

			@$ = @1, $$ = node.Index;
		}
		else if (type.IsStruct())
		{
			EffectTree::Index symbol = EffectTree::Null;
			const EffectTree::Index fields = parser.mAST[type.Definition].As<EffectNodes::Struct>().Fields;

			if (fields != EffectTree::Null)
			{
				const EffectNodes::Variable *field = &parser.mAST[fields].As<EffectNodes::Variable>();

				do
				{
					if (strncmp(field->Name, $3.String.p, $3.String.len) == 0)
					{
						symbol = field->Index;
						break;
					}

					if (field->NextDeclarator != EffectTree::Null)
					{
						field = &parser.mAST[field->NextDeclarator].As<EffectNodes::Variable>();
					}
					else
					{
						field = nullptr;
					}
				}
				while (field != nullptr);
			}

			if (symbol == EffectTree::Null)
			{
				parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
				YYERROR;
			}

			EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
			node.Type = parser.mAST[symbol].As<EffectNodes::Variable>().Type;
			node.Operator = EffectNodes::Expression::Field;
			node.Operands[0] = $1;
			node.Operands[1] = symbol;

			if (type.HasQualifier(EffectNodes::Type::Uniform))
			{
				node.Type.Qualifiers ^= EffectNodes::Type::Uniform;
				node.Type.Qualifiers |= EffectNodes::Type::Const;
			}

			@$ = @1, $$ = node.Index;
		}
		else if (type.IsScalar())
		{
			int offsets[4] = { -1, -1, -1, -1 };

			for (std::size_t i = 0; i < $3.String.len; ++i)
			{
				if (($3.String.p[i] != 'x' && $3.String.p[i] != 'r' && $3.String.p[i] != 's') || i > 3)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
					YYERROR;
				}

				offsets[i] = 0;
			}

			EffectNodes::Swizzle &node = parser.mAST.Add<EffectNodes::Swizzle>(@2);
			node.Type = type;
			node.Type.Qualifiers |= EffectNodes::Type::Const;
			node.Type.Rows = static_cast<unsigned int>($3.String.len);
			node.Operator = EffectNodes::Expression::Swizzle;
			node.Operands[0] = $1;
			node.Mask[0] = offsets[0] & 0xFF, node.Mask[1] = offsets[1] & 0xFF, node.Mask[2] = offsets[2] & 0xFF, node.Mask[3] = offsets[3] & 0xFF;

			@$ = @1, $$ = node.Index;
		}
		else
		{
			parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
			YYERROR;
		}
	}
	;
RULE_EXPRESSION_UNARY
	: RULE_EXPRESSION_POSTFIX
	| RULE_OPERATOR_POSTFIX RULE_EXPRESSION_UNARY
	{
		const EffectNodes::Type type = parser.mAST[$2].As<EffectNodes::RValue>().Type;

		if (!type.IsScalar() && !type.IsVector() && !type.IsMatrix())
		{
			parser.Error(@2, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (type.HasQualifier(EffectNodes::Type::Const) || type.HasQualifier(EffectNodes::Type::Uniform))
		{
			parser.Error(@2, 3025, "l-value specifies const object");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
		node.Type = type;
		node.Operator = $1.Uint;
		node.Operands[0] = $2;

		@$ = @1, $$ = node.Index;
	}
	| RULE_OPERATOR_UNARY RULE_EXPRESSION_UNARY
	{
		const EffectNodes::Type type = parser.mAST[$2].As<EffectNodes::RValue>().Type;

		if (!type.IsScalar() && !type.IsVector() && !type.IsMatrix())
		{
			parser.Error(@2, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if ($1.Uint == EffectNodes::Expression::BitNot && !type.IsIntegral())
		{
			parser.Error(@2, 3082, "int or unsigned int type required");
			YYERROR;
		}

		if ($1.Uint == 0)
		{
			@$ = @1, $$ = $2;
		}
		else if ($1.Uint == EffectNodes::Expression::Negate && parser.mAST[$2].Is<EffectNodes::Literal>())
		{
			EffectNodes::Literal &node = parser.mAST[$2].As<EffectNodes::Literal>();

			for (unsigned int i = 0; i < node.Type.Rows * node.Type.Cols; ++i)
			{
				switch (node.Type.Class)
				{
					case EffectNodes::Type::Bool:
					case EffectNodes::Type::Int:
					case EffectNodes::Type::Uint:
						node.Value.Int[i] = -node.Value.Int[i];
						break;
					case EffectNodes::Type::Float:
						node.Value.Float[i] = -node.Value.Float[i];
						break;
				}
			}

			@$ = @1, $$ = node.Index;
		}
		else
		{
			EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
			node.Type = type;
			node.Operator = $1.Uint;
			node.Operands[0] = $2;

			@$ = @1, $$ = node.Index;
		}
	}
	| "(" RULE_TYPE ")" RULE_EXPRESSION_UNARY
	{
		const EffectNodes::Type expressionType = parser.mAST[$4].As<EffectNodes::RValue>().Type;

		if (expressionType.Class == $2.Type.Class && (expressionType.Rows == $2.Type.Rows && expressionType.Cols == $2.Type.Cols) && !(expressionType.IsArray() || $2.Type.IsArray()))
		{
			@$ = @4, $$ = $4;
		}
		else if (expressionType.IsNumeric() && $2.Type.IsNumeric())
		{
			if ((expressionType.Rows < $2.Type.Rows || expressionType.Cols < $2.Type.Cols) && !expressionType.IsScalar())
			{
				parser.Error(@1, 3017, "cannot convert these vector types");
				YYERROR;
			}

			if (expressionType.Rows > $2.Type.Rows || expressionType.Cols > $2.Type.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}

			if (parser.mAST[$4].Is<EffectNodes::Literal>())
			{
				EffectNodes::Literal &node = parser.mAST[$4].As<EffectNodes::Literal>();
				EffectNodes::Literal value = node;
				node.Type = $2.Type;
				::memset(&node.Value, 0, sizeof(node.Value));

				for (unsigned int i = 0, size = std::min(expressionType.Rows * expressionType.Cols, $2.Type.Rows * $2.Type.Cols); i < size; ++i)
				{
					EffectNodes::Literal::Cast(value, i, node, i);
				}

				@$ = @1, $$ = node.Index;
			}
			else
			{
				EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
				node.Type = $2.Type;
				node.Operator = EffectNodes::Expression::Cast;
				node.Operands[0] = $4;

				@$ = @1, $$ = node.Index;
			}
		}
		else
		{
			parser.Error(@1, 3017, "cannot convert non-numeric types");
			YYERROR;
		}
	}
	;
RULE_EXPRESSION_MULTIPLICATIVE
	: RULE_EXPRESSION_UNARY
	| RULE_EXPRESSION_MULTIPLICATIVE RULE_OPERATOR_MULTIPLICATIVE RULE_EXPRESSION_UNARY
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = std::max(typeLeft.Class, typeRight.Class);

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = $2.Uint;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_ADDITIVE
	: RULE_EXPRESSION_MULTIPLICATIVE
	| RULE_EXPRESSION_ADDITIVE RULE_OPERATOR_ADDITIVE RULE_EXPRESSION_MULTIPLICATIVE
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = std::max(typeLeft.Class, typeRight.Class);

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = $2.Uint;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_SHIFT
	: RULE_EXPRESSION_ADDITIVE
	| RULE_EXPRESSION_SHIFT RULE_OPERATOR_SHIFT RULE_EXPRESSION_ADDITIVE
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!typeRight.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = typeLeft;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = $2.Uint;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_RELATIONAL
	: RULE_EXPRESSION_SHIFT
	| RULE_EXPRESSION_RELATIONAL RULE_OPERATOR_RELATIONAL RULE_EXPRESSION_SHIFT
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = EffectNodes::Type::Bool;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = $2.Uint;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_EQUALITY
	: RULE_EXPRESSION_RELATIONAL
	| RULE_EXPRESSION_EQUALITY RULE_OPERATOR_EQUALITY RULE_EXPRESSION_RELATIONAL
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (typeLeft.IsArray() || typeRight.IsArray() || typeLeft.Definition != typeRight.Definition)
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = EffectNodes::Type::Bool;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = $2.Uint;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_BITAND
	: RULE_EXPRESSION_EQUALITY
	| RULE_EXPRESSION_BITAND "&" RULE_EXPRESSION_EQUALITY
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!typeRight.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = typeLeft;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::BitAnd;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_BITXOR
	: RULE_EXPRESSION_BITAND
	| RULE_EXPRESSION_BITXOR "^" RULE_EXPRESSION_BITAND
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!typeRight.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = typeLeft;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::BitXor;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_BITOR
	: RULE_EXPRESSION_BITXOR
	| RULE_EXPRESSION_BITOR "|" RULE_EXPRESSION_BITXOR
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!typeRight.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type = typeLeft;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::BitOr;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGICAND
	: RULE_EXPRESSION_BITOR
	| RULE_EXPRESSION_LOGICAND "&&" RULE_EXPRESSION_BITOR
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = EffectNodes::Type::Bool;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::LogicAnd;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGICXOR
	: RULE_EXPRESSION_LOGICAND
	| RULE_EXPRESSION_LOGICXOR "^^" RULE_EXPRESSION_LOGICAND
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = EffectNodes::Type::Bool;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::LogicXor;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGICOR
	: RULE_EXPRESSION_LOGICXOR
	| RULE_EXPRESSION_LOGICOR "||" RULE_EXPRESSION_LOGICXOR
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!typeLeft.IsScalar() && !typeLeft.IsVector() && !typeLeft.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!typeRight.IsScalar() && !typeRight.IsVector() && !typeRight.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = EffectNodes::Type::Bool;

		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@1, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::LogicOr;
		node.Operands[0] = $1;
		node.Operands[1] = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_CONDITIONAL
	: RULE_EXPRESSION_LOGICOR
	| RULE_EXPRESSION_LOGICOR "?" RULE_EXPRESSION ":" RULE_EXPRESSION_ASSIGNMENT
	{
		const EffectNodes::Type conditionType = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeLeft = parser.mAST[$3].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$5].As<EffectNodes::RValue>().Type;

		if (!conditionType.IsScalar() && !conditionType.IsVector())
		{
			parser.Error(@1, 3022, "boolean or vector expression expected");
			YYERROR;
		}
		if (typeLeft.IsArray() || typeRight.IsArray() || typeLeft.Definition != typeRight.Definition)
		{
			parser.Error(@1, 3020, "type mismatch between conditional values");
			YYERROR;
		}

		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@2);
		node.Type.Class = std::max(typeLeft.Class, typeRight.Class);
		
		if ((typeLeft.Rows == 1 && typeRight.Cols == 1) || (typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			node.Type.Rows = std::max(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::max(typeLeft.Cols, typeRight.Cols);
		}
		else
		{
			node.Type.Rows = std::min(typeLeft.Rows, typeRight.Rows);
			node.Type.Cols = std::min(typeLeft.Cols, typeRight.Cols);

			if (typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols)
			{
				parser.Warning(@3, 3206, "implicit truncation of vector type");
			}
			if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
			{
				parser.Warning(@5, 3206, "implicit truncation of vector type");
			}
		}

		node.Operator = EffectNodes::Expression::Conditional;
		node.Operands[0] = $1;
		node.Operands[1] = $3;
		node.Operands[2] = $5;
	
		@$ = @1, $$ = node.Index;
	}
	;
RULE_EXPRESSION_ASSIGNMENT
	: RULE_EXPRESSION_CONDITIONAL
	| RULE_EXPRESSION_UNARY RULE_OPERATOR_ASSIGNMENT RULE_EXPRESSION_ASSIGNMENT
	{
		const EffectNodes::Type typeLeft = parser.mAST[$1].As<EffectNodes::RValue>().Type, typeRight = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (typeLeft.IsArray() || typeRight.IsArray() || typeLeft.Definition != typeRight.Definition)
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}
		if ((typeLeft.Rows > typeRight.Rows || typeLeft.Cols > typeRight.Cols) && !(typeRight.Rows == 1 && typeRight.Cols == 1))
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}
		if (typeLeft.HasQualifier(EffectNodes::Type::Const) || typeLeft.HasQualifier(EffectNodes::Type::Uniform))
		{
			parser.Error(@1, 3025, "l-value specifies const object");
			YYERROR;
		}

		if (typeRight.Rows > typeLeft.Rows || typeRight.Cols > typeLeft.Cols)
		{
			parser.Warning(@3, 3206, "implicit truncation of vector type");
		}

		EffectNodes::Assignment &node = parser.mAST.Add<EffectNodes::Assignment>(@2);
		node.Type = typeLeft;
		node.Operator = $2.Uint;
		node.Left = $1;
		node.Right = $3;

		@$ = @1, $$ = node.Index;
	}
	;

RULE_EXPRESSION_INITIALIZER
	: RULE_EXPRESSION_ASSIGNMENT
	| "{" RULE_EXPRESSION_INITIALIZER_LIST "}"
	{
		@$ = @1, $$ = $2;
	}
	| "{" RULE_EXPRESSION_INITIALIZER_LIST "," "}"
	{
		@$ = @1, $$ = $2;
	}
	;
RULE_EXPRESSION_INITIALIZER_LIST
	: RULE_EXPRESSION_INITIALIZER
	| RULE_EXPRESSION_INITIALIZER_LIST "," RULE_EXPRESSION_INITIALIZER
	{
		EffectNodes::RValue *node = &parser.mAST[$1].As<EffectNodes::RValue>();

		while (node->NextExpression != EffectTree::Null)
		{
			node = &parser.mAST[node->NextExpression].As<EffectNodes::RValue>();
		}

		node->NextExpression = $3;

		@$ = @1, $$ = $1;
	}
	;

 /* Statements ------------------------------------------------------------------------------- */

RULE_STATEMENT
	: RULE_STATEMENT_BLOCK
	| RULE_STATEMENT_SINGLE
	;
RULE_STATEMENT_SCOPELESS
	: RULE_STATEMENT_BLOCK_SCOPELESS
	| RULE_STATEMENT_SINGLE
	;
RULE_STATEMENT_LIST
	: RULE_STATEMENT
	| RULE_STATEMENT_LIST RULE_STATEMENT
	{
		EffectNodes::Statement *node = &parser.mAST[$1].As<EffectNodes::Statement>();

		while (node->NextStatement != EffectTree::Null)
		{
			node = &parser.mAST[node->NextStatement].As<EffectNodes::Statement>();
		}

		node->NextStatement = $2;

		@$ = @1, $$ = $1;
	}
	;
RULE_STATEMENT_BLOCK
	: "{" "}"
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);

		@$ = @1, $$ = node.Index;
	}
	| "{" { parser.PushScope(); } RULE_STATEMENT_LIST "}"
	{
		parser.PopScope();

		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);
		node.Statements = $3;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STATEMENT_BLOCK_SCOPELESS
	: "{" "}"
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);

		@$ = @1, $$ = node.Index;
	}
	| "{" RULE_STATEMENT_LIST "}"
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);
		node.Statements = $2;

		@$ = @1, $$ = node.Index;
	}
	;

RULE_STATEMENT_SINGLE
	: RULE_ATTRIBUTES RULE_STATEMENT_EXPRESSION
	{
		EffectNodes::ExpressionStatement &node = parser.mAST[$2].As<EffectNodes::ExpressionStatement>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = $2;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_DECLARATION
	{
		EffectNodes::DeclarationStatement &node = parser.mAST[$2].As<EffectNodes::DeclarationStatement>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = $2;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_IF
	{
		EffectNodes::If &node = parser.mAST[$2].As<EffectNodes::If>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_SWITCH
	{
		EffectNodes::Switch &node = parser.mAST[$2].As<EffectNodes::Switch>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_FOR
	{
		EffectNodes::For &node = parser.mAST[$2].As<EffectNodes::For>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_WHILE
	{
		EffectNodes::While &node = parser.mAST[$2].As<EffectNodes::While>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_JUMP
	{
		EffectNodes::Jump &node = parser.mAST[$2].As<EffectNodes::Jump>();
		node.Attributes = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @2, $$ = node.Index;
	}
	| error RULE_STATEMENT_SINGLE { $$ = $2; }
	;
RULE_STATEMENT_EXPRESSION
	: ";"
	{
		EffectNodes::ExpressionStatement &node = parser.mAST.Add<EffectNodes::ExpressionStatement>(@1);

		@$ = @1, $$ = node.Index;
	}
	| RULE_EXPRESSION ";"
	{
		EffectNodes::ExpressionStatement &node = parser.mAST.Add<EffectNodes::ExpressionStatement>(@1);
		node.Expression = $1;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STATEMENT_DECLARATION
	: RULE_VARIABLE ";"
	{
		EffectNodes::DeclarationStatement &node = parser.mAST.Add<EffectNodes::DeclarationStatement>(@1);
		node.Declaration = $1;

		@$ = @1, $$ = node.Index;
	}
	;

RULE_STATEMENT_IF
	: "if" "(" RULE_EXPRESSION ")" RULE_STATEMENT "else" RULE_STATEMENT
	{
		const EffectNodes::Type conditionType = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!conditionType.IsScalar())
		{
			parser.Error(@3, 3019, "if statement conditional expressions must evaluate to a scalar");
			YYERROR;
		}

		EffectNodes::If &node = parser.mAST.Add<EffectNodes::If>(@1);
		node.Condition = $3;
		node.StatementOnTrue = $5;
		node.StatementOnFalse = $7;
	
		@$ = @1, $$ = node.Index;
	}
	| "if" "(" RULE_EXPRESSION ")" RULE_STATEMENT %prec TOK_THEN
	{
		const EffectNodes::Type conditionType = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!conditionType.IsScalar())
		{
			parser.Error(@3, 3019, "if statement conditional expressions must evaluate to a scalar");
			YYERROR;
		}

		EffectNodes::If &node = parser.mAST.Add<EffectNodes::If>(@1);
		node.Condition = $3;
		node.StatementOnTrue = $5;
	
		@$ = @1, $$ = node.Index;
	}
	;

RULE_STATEMENT_SWITCH
	: "switch" "(" RULE_EXPRESSION ")" "{" "}"
	{
		parser.Warning(@1, 5002, "switch statement contains no 'case' or 'default' labels");

		@$ = @1, $$ = EffectTree::Null;
	}
	| "switch" "(" RULE_EXPRESSION ")" "{" RULE_STATEMENT_CASE_LIST "}"
	{
		const EffectNodes::Type expressionType = parser.mAST[$3].As<EffectNodes::RValue>().Type;

		if (!expressionType.IsScalar())
		{
			parser.Error(@3, 3019, "switch statement expression must evaluate to a scalar");
			YYERROR;
		}

		EffectNodes::Switch &node = parser.mAST.Add<EffectNodes::Switch>(@1);
		node.Test = $3;
		node.Cases = $6;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STATEMENT_CASE
	: RULE_STATEMENT_CASE_LABEL_LIST RULE_STATEMENT
	{
		EffectNodes::Case &node = parser.mAST.Add<EffectNodes::Case>(@1);
		node.Labels = $1;
		node.Statements = $2;

		@$ = @1, $$ = node.Index;
	}
	| RULE_STATEMENT_CASE RULE_STATEMENT
	{
		EffectNodes::Case &node = parser.mAST[$1].As<EffectNodes::Case>();
		EffectNodes::Statement *statement = &parser.mAST[node.Statements].As<EffectNodes::Statement>();

		while (statement->NextStatement != EffectTree::Null)
		{
			statement = &parser.mAST[statement->NextStatement].As<EffectNodes::Statement>();
		}

		statement->NextStatement = $2;

		@$ = @1, $$ = $1;
	}
	;
RULE_STATEMENT_CASE_LIST
	: RULE_STATEMENT_CASE
	| RULE_STATEMENT_CASE_LIST RULE_STATEMENT_CASE
	{
		EffectNodes::Case *node = &parser.mAST[$1].As<EffectNodes::Case>();

		while (node->NextCase != EffectTree::Null)
		{
			node = &parser.mAST[node->NextCase].As<EffectNodes::Case>();
		}

		node->NextCase = $2;

		@$ = @1, $$ = $1;
	}
	;
RULE_STATEMENT_CASE_LABEL
	: "case" RULE_EXPRESSION ":"
	{
		if (!parser.mAST[$2].Is<EffectNodes::Literal>() || !(parser.mAST[$2].As<EffectNodes::Literal>().Type.IsNumeric() || parser.mAST[$2].As<EffectNodes::Literal>().Type.IsBoolean()))
		{
			parser.Error(@2, 3020, "non-numeric case expression");
			YYERROR;
		}

		@$ = @1, $$ = $2; parser.mAST[$$].Location = @$;
	}
	| "case" error ":"
	{
		$$ = EffectTree::Null;
	}
	| "default" ":"
	{
		EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STATEMENT_CASE_LABEL_LIST
	: RULE_STATEMENT_CASE_LABEL
	| RULE_STATEMENT_CASE_LABEL_LIST RULE_STATEMENT_CASE_LABEL
	{
		EffectNodes::RValue *node = &parser.mAST[$1].As<EffectNodes::RValue>();

		while (node->NextExpression != EffectTree::Null)
		{
			node = &parser.mAST[node->NextExpression].As<EffectNodes::RValue>();
		}

		node->NextExpression = $2;

		@$ = @1, $$ = $1;
	}
	;

RULE_STATEMENT_FOR
	: "for" "(" { parser.PushScope(); } RULE_STATEMENT_FOR_INITIALIZER RULE_STATEMENT_FOR_CONDITION ";" RULE_STATEMENT_FOR_ITERATOR ")" RULE_STATEMENT_SCOPELESS
	{
		parser.PopScope();

		EffectNodes::For &node = parser.mAST.Add<EffectNodes::For>(@1);
		node.Initialization = $4;
		node.Condition = $5;
		node.Iteration = $7;
		node.Statements = $9;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STATEMENT_FOR_INITIALIZER
	: RULE_STATEMENT_SINGLE
	;
RULE_STATEMENT_FOR_CONDITION
	: { $$ = EffectTree::Null; }
	| RULE_EXPRESSION_CONDITIONAL
	;
RULE_STATEMENT_FOR_ITERATOR
	: { $$ = EffectTree::Null; }
	| RULE_EXPRESSION
	;

RULE_STATEMENT_WHILE
	: "while" "(" { parser.PushScope(); } RULE_EXPRESSION ")" RULE_STATEMENT_SCOPELESS
	{
		parser.PopScope();

		EffectNodes::While &node = parser.mAST.Add<EffectNodes::While>(@1);
		node.Condition = $4;
		node.Statements = $6;
	
		@$ = @1, $$ = node.Index;
	}
	| "do" RULE_STATEMENT "while" "(" RULE_EXPRESSION ")" ";"
	{
		EffectNodes::While &node = parser.mAST.Add<EffectNodes::While>(@1);
		node.DoWhile = true;
		node.Condition = $5;
		node.Statements = $2;

		@$ = @1, $$ = node.Index;
	}
	;

RULE_STATEMENT_JUMP
	: "return" ";"
	{
		const EffectTree::Index function = parser.GetCurrentParent();

		if (!parser.mAST[function].As<EffectNodes::Function>().ReturnType.IsVoid())
		{
			parser.Error(@1, 3080, "function must return a value");
			YYERROR;
		}

		EffectNodes::Return &node = parser.mAST.Add<EffectNodes::Return>(@1);

		@$ = @1, $$ = node.Index;
	}
	| "return" RULE_EXPRESSION ";"
	{
		const EffectTree::Index function = parser.GetCurrentParent();
		const EffectNodes::Type valueType = parser.mAST[$2].As<EffectNodes::RValue>().Type, returnType = parser.mAST[function].As<EffectNodes::Function>().ReturnType;

		if (returnType.IsVoid())
		{
			if (!valueType.IsVoid())
			{
				parser.Error(@1, 3079, "void functions cannot return a value");
				YYERROR;
			}

			@$ = @2, $$ = $2;
		}
		else
		{
			if (!EffectNodes::Type::Compatible(valueType, returnType))
			{
				parser.Error(@2, 3020, "type mismatch");
				YYERROR;
			}

			if (valueType.Rows > returnType.Rows || valueType.Cols > returnType.Cols)
			{
				parser.Warning(@2, 3206, "implicit truncation of vector type");
			}

			EffectNodes::Return &node = parser.mAST.Add<EffectNodes::Return>(@1);
			node.Value = $2;

			@$ = @1, $$ = node.Index;
		}
	}
	| "break" ";"
	{
		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Break;

		@$ = @1, $$ = node.Index;
	}
	| "continue" ";"
	{
		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Continue;

		@$ = @1, $$ = node.Index;
	}
	| "discard" ";"
	{
		EffectNodes::Return &node = parser.mAST.Add<EffectNodes::Return>(@1);
		node.Discard = true;

		@$ = @1, $$ = node.Index;
	}
	;

 /* Attributes ------------------------------------------------------------------------------- */

 RULE_ATTRIBUTE
	: "[" RULE_IDENTIFIER_NAME "]"
	{
		@$ = @1, $$ = $2;
	}
	| "[" error "]"
	{
		$$.String.p = nullptr;
		$$.String.len = 0;
	}
	;
RULE_ATTRIBUTE_LIST
	: RULE_ATTRIBUTE
	| RULE_ATTRIBUTE_LIST RULE_ATTRIBUTE
	{
		parser.Error(@2, 3084, "cannot attach multiple attributes to a statement");
		YYERROR;
	}
	;

RULE_ATTRIBUTES
	:
	{
		$$.String.p = nullptr;
		$$.String.len = 0;
	}
	| RULE_ATTRIBUTE_LIST
	;

 /* Semantics -------------------------------------------------------------------------------- */

RULE_SEMANTICS
	:
	{
		$$.String.p = nullptr;
		$$.String.len = 0;
	}
	| ":" RULE_IDENTIFIER_SEMANTIC
	{
		@$ = @1, $$ = $2;
	}
	| ":" error
	{
		$$.String.p = nullptr;
		$$.String.len = 0;
	}
	;

 /* Annotations ------------------------------------------------------------------------------ */

RULE_ANNOTATION
	: RULE_IDENTIFIER_NAME "=" RULE_EXPRESSION_ASSIGNMENT ";"
	{
		if (!parser.mAST[$3].Is<EffectNodes::Literal>())
		{
			parser.Error(@3, 3011, "value must be a literal expression");
			YYERROR;
		}

		EffectNodes::Annotation &node = parser.mAST.Add<EffectNodes::Annotation>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Value = $3;

		@$ = @1, $$ = node.Index;
	}
	| RULE_TYPE_SINGLE RULE_IDENTIFIER_NAME "=" RULE_EXPRESSION_ASSIGNMENT ";"
	{
		if (!parser.mAST[$4].Is<EffectNodes::Literal>())
		{
			parser.Error(@4, 3011, "value must be a literal expression");
			YYERROR;
		}

		EffectNodes::Annotation &node = parser.mAST.Add<EffectNodes::Annotation>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Value = $4;

		@$ = @1, $$ = node.Index;
	}
	| error ";"
	{
		$$ = EffectTree::Null;
	}
	;
RULE_ANNOTATION_LIST
	: RULE_ANNOTATION
	| RULE_ANNOTATION_LIST RULE_ANNOTATION
	{
		EffectNodes::Annotation *node = &parser.mAST[$1].As<EffectNodes::Annotation>();

		while (node->NextAnnotation != EffectTree::Null)
		{
			node = &parser.mAST[node->NextAnnotation].As<EffectNodes::Annotation>();
		}

		node->NextAnnotation = $2;

		@$ = @1, $$ = $1;
	}
	;

RULE_ANNOTATIONS
	:
	{
		$$ = EffectTree::Null;
	}
	| "<" ">"
	{
		$$ = EffectTree::Null;
	}
	| "<" RULE_ANNOTATION_LIST ">"
	{
		@$ = @1, $$ = $2; parser.mAST[$$].Location = @$;
	}
	| "<" error ">"
	{
		$$ = EffectTree::Null;
	}
	;

 /* Struct Declaration .---------------------------------------------------------------------- */

RULE_STRUCT
	: "struct" "{" "}"
	{
		EffectNodes::Struct &node = parser.mAST.Add<EffectNodes::Struct>(@1);

		parser.Warning(@1, 5001, "struct has no members");

		@$ = @1, $$ = node.Index;
	}
	| "struct" "{"
	{
		EffectNodes::Struct &node = parser.mAST.Add<EffectNodes::Struct>(@1);

		parser.PushScope(node.Index);
	}
	  RULE_STRUCT_FIELD_LIST "}"
	{
		const EffectTree::Index index = parser.GetCurrentParent();
		parser.PopScope();

		EffectNodes::Struct &node = parser.mAST[index].As<EffectNodes::Struct>();
		node.Fields = $4;

		@$ = @1, $$ = index;
	}
	| "struct" RULE_IDENTIFIER_NAME "{" "}"
	{
		EffectNodes::Struct &node = parser.mAST.Add<EffectNodes::Struct>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);

		parser.Warning(@2, 5001, "struct '%s' has no members", node.Name);

		if (!parser.PushSymbol(node.Index))
		{
			parser.Error(@2, 3003, "redefinition of '%s'", node.Name);
			YYERROR;
		}

		@$ = @1, $$ = node.Index;
	}
	| "struct" RULE_IDENTIFIER_NAME "{"
	{
		EffectNodes::Struct &node = parser.mAST.Add<EffectNodes::Struct>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);

		parser.PushScope(node.Index);
	}
	  RULE_STRUCT_FIELD_LIST "}"
	{
		const EffectTree::Index index = parser.GetCurrentParent();
		parser.PopScope();

		EffectNodes::Struct &node = parser.mAST[index].As<EffectNodes::Struct>();
		node.Fields = $5;

		if (!parser.PushSymbol(index))
		{
			parser.Error(@2, 3003, "redefinition of '%s'", node.Name);
			YYERROR;
		}

		@$ = @1, $$ = index;
	}
	;

RULE_STRUCT_FIELD
	: RULE_TYPE_FULLYSPECIFIED RULE_STRUCT_FIELD_DECLARATOR_LIST ";"
	{
		if ($1.Type.IsVoid())
		{
			parser.Error(@1, 3038, "struct members cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::In) || $1.Type.HasQualifier(EffectNodes::Type::Out))
		{
			parser.Error(@1, 3055, "struct members cannot be declared 'in' or 'out'");
			YYERROR;
		}

		EffectNodes::Variable *field = &parser.mAST[$2].As<EffectNodes::Variable>();

		do
		{
			field->Type.Class = $1.Type.Class;
			field->Type.Qualifiers = $1.Type.Qualifiers;
			field->Type.Rows = $1.Type.Rows;
			field->Type.Cols = $1.Type.Cols;
			field->Type.Definition = $1.Type.Definition;

			if (field->NextDeclarator != EffectTree::Null)
			{
				field = &parser.mAST[field->NextDeclarator].As<EffectNodes::Variable>();
			}
			else
			{
				field = nullptr;
			}
		}
		while (field != nullptr);

		@$ = @1, $$ = $2;
	}
	| error ";"
	{
		$$ = EffectTree::Null;
	}
	;
RULE_STRUCT_FIELD_LIST
	: RULE_STRUCT_FIELD
	| RULE_STRUCT_FIELD_LIST RULE_STRUCT_FIELD
	{
		EffectNodes::Variable *node = &parser.mAST[$1].As<EffectNodes::Variable>();

		while (node->NextDeclarator != EffectTree::Null)
		{
			node = &parser.mAST[node->NextDeclarator].As<EffectNodes::Variable>();
		}

		node->NextDeclarator = $2;

		@$ = @1, $$ = $1;
	}
	;

RULE_STRUCT_FIELD_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_STRUCT_FIELD_DECLARATOR_LIST
	: RULE_STRUCT_FIELD_DECLARATOR
	| RULE_STRUCT_FIELD_DECLARATOR_LIST "," RULE_STRUCT_FIELD_DECLARATOR
	{
		EffectNodes::Variable *node = &parser.mAST[$1].As<EffectNodes::Variable>();

		while (node->NextDeclarator != EffectTree::Null)
		{
			node = &parser.mAST[node->NextDeclarator].As<EffectNodes::Variable>();
		}

		node->NextDeclarator = $3;

		@$ = @1, $$ = $1;
	}
	;

 /* Variable Declaration --------------------------------------------------------------------- */

RULE_VARIABLE
	: RULE_TYPE_FULLYSPECIFIED RULE_VARIABLE_DECLARATOR_LIST
	{	
		if ($1.Type.IsVoid())
		{
			parser.Error(@1, 3038, "variables cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::In) || $1.Type.HasQualifier(EffectNodes::Type::Out))
		{
			parser.Error(@1, 3055, "variables cannot be declared 'in' or 'out'");
			YYERROR;
		}

		const EffectTree::Index parent = parser.GetCurrentParent();

		if (parent != EffectTree::Null)
		{
			if ($1.Type.HasQualifier(EffectNodes::Type::Extern))
			{
				parser.Error(@1, 3006, "local variables cannot be declared 'extern'");
				YYERROR;
			}
			if ($1.Type.HasQualifier(EffectNodes::Type::Uniform))
			{
				parser.Error(@1, 3047, "local variables cannot be declared 'uniform'");
				YYERROR;
			}
		}
		else
		{
			if (!$1.Type.HasQualifier(EffectNodes::Type::Static))
			{
				if (!$1.Type.HasQualifier(EffectNodes::Type::Uniform) && !$1.Type.IsTexture() && !$1.Type.IsSampler())
				{
					parser.Warning(@1, 5000, "global variables are considered 'uniform' by default");
				}

				$1.Type.Qualifiers |= EffectNodes::Type::Extern | EffectNodes::Type::Uniform;
			}
		}

		EffectNodes::Variable *variable = &parser.mAST[$2].As<EffectNodes::Variable>();

		do
		{
			variable->Type.Class = $1.Type.Class;
			variable->Type.Qualifiers = $1.Type.Qualifiers;
			variable->Type.Rows = $1.Type.Rows;
			variable->Type.Cols = $1.Type.Cols;
			variable->Type.Definition = $1.Type.Definition;

			if (!parser.PushSymbol(variable->Index))
			{
				parser.Error(variable->Location, 3003, "redefinition of '%s'", variable->Name);
				YYERROR;
			}
	
			if (variable->Initializer != EffectTree::Null)
			{
				const EffectNodes::RValue *initializer = &parser.mAST[variable->Initializer].As<EffectNodes::RValue>();
			
				if ((initializer->Type.Rows < variable->Type.Rows || initializer->Type.Cols < variable->Type.Cols) && !initializer->Type.IsScalar())
				{
					parser.Error(initializer->Location, 3017, "cannot implicitly convert these vector types");
					YYERROR;
				}
				if (!EffectNodes::Type::Compatible(initializer->Type, variable->Type))
				{
					parser.Error(initializer->Location, 3017, "initializer does not match type");
					YYERROR;
				}

				if (initializer->Type.Rows > variable->Type.Rows || initializer->Type.Cols > variable->Type.Cols)
				{
					parser.Warning(initializer->Location, 3206, "implicit truncation of vector type");
				}

				if (parent == EffectTree::Null && !initializer->Is<EffectNodes::Literal>())
				{
					parser.Error(initializer->Location, 3011, "initial value must be a literal expression");
					YYERROR;
				}
			}
			else if ($1.Type.IsNumeric() && $1.Type.HasQualifier(EffectNodes::Type::Const))
			{
				parser.Error(variable->Location, 3012, "missing initial value for '%s'", variable->Name);
				YYERROR;
			}

			if (variable->NextDeclarator != EffectTree::Null)
			{
				variable = &parser.mAST[variable->NextDeclarator].As<EffectNodes::Variable>();
			}
			else
			{
				variable = nullptr;
			}
		}
		while (variable != nullptr);

		@$ = @1, $$ = $2;
	}
	;

RULE_VARIABLE_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS RULE_ANNOTATIONS
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;
		node.Annotations = $4;

		@$ = @1, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS RULE_ANNOTATIONS "=" RULE_EXPRESSION_INITIALIZER
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;
		node.Annotations = $4;

		@$ = @1, $$ = node.Index;

		if (parser.mAST[$6].As<EffectNodes::RValue>().NextExpression != EffectTree::Null || $2.Type.IsArray())
		{
			EffectNodes::InitializerList &initializer = parser.mAST.Add<EffectNodes::InitializerList>(@6);
			const EffectNodes::RValue *expression = &parser.mAST[$6].As<EffectNodes::RValue>();
			initializer.Type = expression->Type;
			initializer.Type.ArrayLength = 1;
			initializer.Expressions = $6;

			while (expression->NextExpression != EffectTree::Null)
			{
				initializer.Type.ArrayLength++;

				expression = &parser.mAST[expression->NextExpression].As<EffectNodes::RValue>();
			}

			parser.mAST[$$].As<EffectNodes::Variable>().Initializer = initializer.Index;
		}
		else
		{
			parser.mAST[$$].As<EffectNodes::Variable>().Initializer = $6;
		}
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS RULE_ANNOTATIONS "=" error
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;
		node.Annotations = $4;

		@$ = @1, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS RULE_ANNOTATIONS "{" RULE_VARIABLE_PROPERTY_LIST "}"
	{
		if ($2.Type.IsArray())
		{
			parser.Error(@4, 3009, "cannot initialize arrays with property list");
			YYERROR;
		}

		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;
		node.Annotations = $4;

		for (unsigned int i = 0; i < EffectNodes::Variable::PropertyCount; ++i)
		{
			node.Properties[i] = $6.Properties[i];
		}

		@$ = @1, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS RULE_ANNOTATIONS "{" error "}"
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;
		node.Annotations = $4;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_VARIABLE_DECLARATOR_LIST
	: RULE_VARIABLE_DECLARATOR
	| RULE_VARIABLE_DECLARATOR_LIST "," RULE_VARIABLE_DECLARATOR
	{
		EffectNodes::Variable *node = &parser.mAST[$1].As<EffectNodes::Variable>();

		while (node->NextDeclarator != EffectTree::Null)
		{
			node = &parser.mAST[node->NextDeclarator].As<EffectNodes::Variable>();
		}

		node->NextDeclarator = $3;

		@$ = @1, $$ = $1;
	}
	;

RULE_VARIABLE_PROPERTY
	: TOK_IDENTIFIER_PROPERTY "=" RULE_EXPRESSION_LITERAL ";"
	{
		@$ = @1, $$.Properties[$$.Index = $1.Uint] = $3;
	}
	| TOK_IDENTIFIER_PROPERTY "=" RULE_IDENTIFIER_NAME ";"
	{	
		if ($3.Node == EffectTree::Null)
		{
			parser.Error(@3, 3004, "undeclared identifier '%.*s'", $3.String.len, $3.String.p);
			YYERROR;
		}
		if ($1.Uint != EffectNodes::Variable::Texture || !parser.mAST[$3.Node].Is<EffectNodes::Variable>() || !parser.mAST[$3.Node].As<EffectNodes::Variable>().Type.IsTexture() || parser.mAST[$3.Node].As<EffectNodes::Variable>().Type.IsArray())
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		@$ = @1, $$.Properties[$$.Index = $1.Uint] = $3.Node;
	}
	| error ";"
	{
		$$.Index = 0;
		$$.Properties[0] = 0;
	}
	;
RULE_VARIABLE_PROPERTY_LIST
	: RULE_VARIABLE_PROPERTY
	{
		for (unsigned int i = 0; i < EffectNodes::Variable::PropertyCount; ++i)
		{
			$$.Properties[i] = 0;
		}

		@$ = @1, $$.Properties[$1.Index] = $1.Properties[$1.Index];
	}
	| RULE_VARIABLE_PROPERTY_LIST RULE_VARIABLE_PROPERTY
	{
		for (unsigned int i = 0; i < EffectNodes::Variable::PropertyCount; ++i)
		{
			$$.Properties[i] = $1.Properties[i];
		}

		@$ = @1, $$.Properties[$2.Index] = $2.Properties[$2.Index];
	}
	;

 /* Function Declaration --------------------------------------------------------------------- */

RULE_FUNCTION_PROTOTYPE
	: RULE_TYPE_FULLYSPECIFIED RULE_FUNCTION_DECLARATOR RULE_SEMANTICS
	{
		if ($3.String.p != nullptr && $1.Type.IsVoid())
		{
			parser.Error(@2, 3076, "void function cannot have a semantic");
			YYERROR;
		}
		if ($1.Type.Qualifiers != 0)
		{
			parser.Error(@1, 3047, "function return type cannot have any qualifiers");
			YYERROR;
		}

		EffectNodes::Function &node = parser.mAST[$2].As<EffectNodes::Function>();
		node.ReturnType = $1.Type;
		node.ReturnType.Qualifiers = EffectNodes::Type::Const;
		node.ReturnSemantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;

		parser.PushSymbol(node.Index);

		@$ = @1, $$ = node.Index;
	}
	;
RULE_FUNCTION_DEFINITION
	: RULE_FUNCTION_PROTOTYPE
	{
		const EffectNodes::Function &node = parser.mAST[$1].As<EffectNodes::Function>();

		parser.PushScope(node.Index);

		if (node.Parameters != EffectTree::Null)
		{
			const EffectNodes::Variable *parameter = &parser.mAST[node.Parameters].As<EffectNodes::Variable>();
	
			do
			{
				if (!parser.PushSymbol(parameter->Index))
				{
					parser.Error(parameter->Location, 3003, "redefinition of '%s'", parameter->Name);
					YYERROR;
				}

				if (parameter->NextDeclaration != EffectTree::Null)
				{
					parameter = &parser.mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
				}
				else
				{
					parameter = nullptr;
				}
			}
			while (parameter != nullptr);
		}
	}
	  RULE_STATEMENT_BLOCK
	{
		const EffectTree::Index index = parser.GetCurrentParent();
		parser.PopScope();

		EffectNodes::Function &node = parser.mAST[index].As<EffectNodes::Function>();
		node.Definition = $3;

		@$ = @1, $$ = index;
	}
	;

RULE_FUNCTION_DECLARATOR
	: RULE_IDENTIFIER_SYMBOL "(" ")"
	{
		EffectNodes::Function &node = parser.mAST.Add<EffectNodes::Function>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = @1, $$ = node.Index;
	}
	| RULE_IDENTIFIER_SYMBOL "("
	{
		EffectNodes::Function &node = parser.mAST.Add<EffectNodes::Function>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);

		parser.PushScope(node.Index);
	}
	  RULE_FUNCTION_PARAMETER_LIST ")"
	{
		const EffectTree::Index index = parser.GetCurrentParent();
		parser.PopScope();

		EffectNodes::Function &node = parser.mAST[index].As<EffectNodes::Function>();
		node.Parameters = $4;
		node.ParameterCount = 1;

		const EffectNodes::Variable *parameter = &parser.mAST[$4].As<EffectNodes::Variable>();

		while (parameter->NextDeclaration != EffectTree::Null)
		{
			node.ParameterCount++;

			parameter = &parser.mAST[parameter->NextDeclaration].As<EffectNodes::Variable>();
		}

		@$ = @1, $$ = index;
	}
	;

RULE_FUNCTION_PARAMETER
	: RULE_TYPE_FULLYSPECIFIED RULE_FUNCTION_PARAMETER_DECLARATOR
	{
		if ($1.Type.IsVoid())
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::Extern))
		{
			parser.Error(@1, 3006, "function parameters cannot be declared 'extern'");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::Static))
		{
			parser.Error(@1, 3007, "function parameters cannot be declared 'static'");
			YYERROR;
		}
		if ($1.Type.HasQualifier(EffectNodes::Type::Uniform))
		{
			parser.Error(@1, 3047, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if ($1.Type.HasQualifier(EffectNodes::Type::Out))
		{
			if ($1.Type.HasQualifier(EffectNodes::Type::Const))
			{
				parser.Error(@1, 3046, "output parameters cannot be declared 'const'");
				YYERROR;
			}
		}
		else
		{
			$1.Type.Qualifiers |= EffectNodes::Type::In;
		}

		EffectNodes::Variable &node = parser.mAST[$2].As<EffectNodes::Variable>();
		node.Type.Class = $1.Type.Class;
		node.Type.Qualifiers = $1.Type.Qualifiers;
		node.Type.Rows = $1.Type.Rows;
		node.Type.Cols = $1.Type.Cols;
		node.Type.Definition = $1.Type.Definition;

		@$ = @1, $$ = node.Index;
	}
	;
RULE_FUNCTION_PARAMETER_LIST
	: RULE_FUNCTION_PARAMETER
	| RULE_FUNCTION_PARAMETER_LIST "," RULE_FUNCTION_PARAMETER
	{
		EffectNodes::Variable *node = &parser.mAST[$1].As<EffectNodes::Variable>();

		while (node->NextDeclaration != EffectTree::Null)
		{
			node = &parser.mAST[node->NextDeclaration].As<EffectNodes::Variable>();
		}

		node->NextDeclaration = $3;

		@$ = @1, $$ = $1;
	}
	;

RULE_FUNCTION_PARAMETER_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_SEMANTICS
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = ($3.String.p != nullptr) ? parser.mAST.AddString($3.String.p, $3.String.len) : nullptr;

		@$ = @1, $$ = node.Index;
	}
	;

 /* Technique Declaration -------------------------------------------------------------------- */

RULE_TECHNIQUE
	: "technique" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" "}"
	{
		parser.Error(@2, 5003, "technique '%.*s' has no passes", $2.String.len, $2.String.p);
		YYERROR;
	}
	| "technique" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" RULE_PASS_LIST "}"
	{
		EffectNodes::Technique &node = parser.mAST.Add<EffectNodes::Technique>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Annotations = $3;
		node.Passes = $5;

		@$ = @1, $$ = node.Index;
	}
	;

 /* Pass Declaration ------------------------------------------------------------------------- */

RULE_PASS
	: "pass" RULE_ANNOTATIONS "{" "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@1);
		node.Annotations = $2;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = EffectTree::Null;
		}

		@$ = @1, $$ = node.Index;
	}
	| "pass" RULE_ANNOTATIONS "{" RULE_PASSSTATE_LIST "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@1);
		node.Annotations = $2;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = $4.States[i];
		}

		@$ = @1, $$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Annotations = $3;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = EffectTree::Null;
		}

		@$ = @1, $$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" RULE_PASSSTATE_LIST "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Annotations = $3;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = $5.States[i];
		}

		@$ = @1, $$ = node.Index;
	}
	;
RULE_PASS_LIST
	: RULE_PASS
	| RULE_PASS_LIST RULE_PASS
	{
		EffectNodes::Pass *node = &parser.mAST[$1].As<EffectNodes::Pass>();

		while (node->NextPass != EffectTree::Null)
		{
			node = &parser.mAST[node->NextPass].As<EffectNodes::Pass>();
		}

		node->NextPass = $2;

		@$ = @1, $$ = $1;
	}
	;

RULE_PASSSTATE
	: RULE_IDENTIFIER_PASSSTATE "=" RULE_EXPRESSION_LITERAL ";"
	{
		if (($1.Uint == EffectNodes::Pass::VertexShader || $1.Uint == EffectNodes::Pass::PixelShader) || ($1.Uint >= EffectNodes::Pass::RenderTarget0 && $1.Uint <= EffectNodes::Pass::RenderTarget7))
		{
			if (parser.mAST[$3].As<EffectNodes::Literal>().Value.Uint[0] != 0)
			{
				parser.Error(@2, 3020, "type mismatch");
				YYERROR;
			}

			$3 = EffectTree::Null;
		}

		@$ = @1, $$.States[$$.Index = $1.Uint] = $3;
	}
	| RULE_IDENTIFIER_PASSSTATE "=" RULE_IDENTIFIER_NAME ";"
	{
		const bool stateShaderAssignment = $1.Uint == EffectNodes::Pass::VertexShader || $1.Uint == EffectNodes::Pass::PixelShader;
		const bool stateRenderTargetAssignment = $1.Uint >= EffectNodes::Pass::RenderTarget0 && $1.Uint <= EffectNodes::Pass::RenderTarget7;

		if ($3.Node == EffectTree::Null)
		{
			if (stateShaderAssignment)
			{
				parser.Error(@3, 3501, "entrypoint '%.*s' not found", $3.String.len, $3.String.p);
			}
			else
			{
				parser.Error(@3, 3004, "undeclared identifier '%.*s'", $3.String.len, $3.String.p);
			}
			YYERROR;
		}
		if ((stateShaderAssignment && !parser.mAST[$3.Node].Is<EffectNodes::Function>()) || (stateRenderTargetAssignment && (!parser.mAST[$3.Node].Is<EffectNodes::Variable>() || !parser.mAST[$3.Node].As<EffectNodes::Variable>().Type.IsTexture() || parser.mAST[$3.Node].As<EffectNodes::Variable>().Type.IsArray())))
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		@$ = @1, $$.States[$$.Index = $1.Uint] = $3.Node;
	}
	| error ";"
	{
		$$.Index = 0;
		$$.States[0] = EffectTree::Null;
	}
	;
RULE_PASSSTATE_LIST
	: RULE_PASSSTATE
	{
		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			$$.States[i] = EffectTree::Null;
		}

		@$ = @1, $$.States[$1.Index] = $1.States[$1.Index];
	}
	| RULE_PASSSTATE_LIST RULE_PASSSTATE
	{
		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			$$.States[i] = $1.States[i];
		}

		@$ = @1, $$.States[$2.Index] = $2.States[$2.Index];
	}
	;

 /* ------------------------------------------------------------------------------------------ */

%%

#include <cstdarg>
#include <functional>
#include <boost\assign\list_of.hpp>

namespace ReShade
{
	EffectParser::EffectParser(EffectTree &ast) : mAST(ast), mLexer(nullptr), mParser(nullptr), mNextLexerState(0), mCurrentScope(0)
	{
		if (yylex_init(&this->mLexer) != 0)
		{
			return;
		}

		// Add root node
		this->mAST.Add<EffectNodes::Root>();
	}
	EffectParser::~EffectParser()
	{
		if (this->mLexer != nullptr)
		{
			yylex_destroy(this->mLexer);
		}
	}

	bool EffectParser::Parse(const std::string &source, std::string &errors)
	{
		if (this->mLexer == nullptr)
		{
			return false;
		}
	
		this->mParser = yy_scan_bytes(source.c_str(), source.length(), this->mLexer);

		if (this->mParser == nullptr)
		{
			return false;
		}

		this->mErrors.clear();
		this->mErrorsCount = 0;

		bool res = yyparse(this->mLexer, *this) == 0;
		
		if (this->mErrorsCount > 0)
		{
			res = false;

			this->mErrors += '\n';
			this->mErrors += "compilation failed with " + std::to_string(this->mErrorsCount) + " errors.";
		}

		errors += this->mErrors;

		this->mParser = nullptr;

		return res;
	}

	void EffectParser::Error(const YYLTYPE &location, unsigned int code, const char *message, ...)
	{
		if (location.Source != nullptr)
		{
			this->mErrors += location.Source;
		}

		this->mErrors += '(' + std::to_string(location.Line) + ", " + std::to_string(location.Column) + ')' + ": ";

		if (code != 0)
		{
			this->mErrors += "error X" + std::to_string(code);
		}
		else
		{
			this->mErrors += "error";
		}

		this->mErrors += ": ";
	
		va_list args;
		va_start(args, message);

		char formatted[512];
		vsprintf_s(formatted, message, args);

		va_end(args);

		this->mErrors += formatted;
		this->mErrors += '\n';

		++this->mErrorsCount;
	}
	void EffectParser::Warning(const YYLTYPE &location, unsigned int code, const char *message, ...)
	{
		if (location.Source != nullptr)
		{
			this->mErrors += location.Source;
		}

		this->mErrors += '(' + std::to_string(location.Line) + ", " + std::to_string(location.Column) + ')' + ": ";

		if (code != 0)
		{
			this->mErrors += "warning X" + std::to_string(code);
		}
		else
		{
			this->mErrors += "warning";
		}

		this->mErrors += ": ";
	
		va_list args;
		va_start(args, message);

		char formatted[512];
		vsprintf_s(formatted, message, args);

		va_end(args);

		this->mErrors += formatted;
		this->mErrors += '\n';
	}

	EffectParser::Scope EffectParser::GetCurrentScope() const
	{
		return this->mCurrentScope;
	}
	EffectTree::Index EffectParser::GetCurrentParent() const
	{
		return !this->mParentStack.empty() ? this->mParentStack.top() : EffectTree::Null;
	}
	EffectTree::Index EffectParser::FindSymbol(const std::string &name, Scope scope, bool exclusive) const
	{
		const auto it = this->mSymbolStack.find(name);
	
		if (it == this->mSymbolStack.end() || it->second.empty())
		{
			return EffectTree::Null;
		}

		EffectTree::Index result = EffectTree::Null;
		const auto &scopes = it->second;
	
		for (auto it = scopes.rbegin(), end = scopes.rend(); it != end; ++it)
		{
			if (it->first > scope)
			{
				continue;
			}
			if (exclusive && it->first < scope)
			{
				continue;
			}
		
			const EffectTree::Node &symbol = this->mAST[it->second];
		
			if (symbol.Is<EffectNodes::Variable>() || symbol.Is<EffectNodes::Struct>())
			{
				return symbol.Index;
			}
			else if (result == EffectTree::Null)
			{
				result = symbol.Index;
			}
		}
	
		return result;
	}
	
	static bool GetCallRanks(const EffectTree &ast, const EffectNodes::Call &call, const EffectTree &ast1, const EffectNodes::Function *function, unsigned int ranks[], unsigned int argumentCount)
	{
		const EffectNodes::RValue *argument = &ast[call.Arguments].As<EffectNodes::RValue>();
		const EffectNodes::Variable *parameter = &ast1[function->Parameters].As<EffectNodes::Variable>();

		for (unsigned int i = 0; i < argumentCount; ++i)
		{
			ranks[i] = EffectNodes::Type::Compatible(argument->Type, parameter->Type);
			
			if (!ranks[i])
			{
				return false;
			}

			argument = &ast[argument->NextExpression].As<EffectNodes::RValue>();
			parameter = &ast1[parameter->NextDeclaration].As<EffectNodes::Variable>();
		}

		return argument != nullptr && parameter != nullptr;
	}
	static int CompareFunctions(const EffectTree &ast, const EffectNodes::Call &call, const EffectTree &ast1, const EffectNodes::Function *function1, const EffectTree &ast2, const EffectNodes::Function *function2, unsigned int argumentCount)
	{
		if (function2 == nullptr)
		{
			return -1;
		}

		// Adapted from: https://github.com/unknownworlds/hlslparser
		unsigned int *function1Ranks = static_cast<unsigned int *>(alloca(argumentCount * sizeof(unsigned int)));
		unsigned int *function2Ranks = static_cast<unsigned int *>(alloca(argumentCount * sizeof(unsigned int)));
		const bool function1Viable = GetCallRanks(ast, call, ast1, function1, function1Ranks, argumentCount);
		const bool function2Viable = GetCallRanks(ast, call, ast2, function2, function2Ranks, argumentCount);

		if (!(function1Viable && function2Viable))
		{
			if (function1Viable)
			{
				return -1;
			}
			else if (function2Viable)
			{
				return +1;
			}
			else
			{
				return 0;
			}
		}
		
		std::sort(function1Ranks, function1Ranks + argumentCount, std::greater<unsigned int>());
		std::sort(function2Ranks, function2Ranks + argumentCount, std::greater<unsigned int>());
		
		for (unsigned int i = 0; i < argumentCount; ++i)
		{
			if (function1Ranks[i] < function2Ranks[i])
			{
				return -1;
			}
			else if (function2Ranks[i] < function1Ranks[i])
			{
				return +1;
			}
		}

		return 0;
	}
	bool EffectParser::ResolveCall(EffectNodes::Call &call, bool &intrinsic, bool &ambiguous) const
	{
		intrinsic = false;
		ambiguous = false;

		#pragma region Intrinsics
		static EffectTree sIntrinsics;
		static bool sIntrinsicsInitialized = false;
		static const char *sIntrinsicOverloads =
			"float abs(float x); float2 abs(float2 x); float3 abs(float3 x); float4 abs(float4 x);"
			"int sign(float x); int2 sign(float2 x); int3 sign(float3 x); int4 sign(float4 x);"
			"float rcp(float x); float2 rcp(float2 x); float3 rcp(float3 x); float4 rcp(float4 x);"
			"bool all(bool x); bool all(bool2 x); bool all(bool3 x); bool all(bool4 x);"
			"bool any(bool x); bool any(bool2 x); bool any(bool3 x); bool any(bool4 x);"
			"float sin(float x); float2 sin(float2 x); float3 sin(float3 x); float4 sin(float4 x);"
			"float sinh(float x); float2 sinh(float2 x); float3 sinh(float3 x); float4 sinh(float4 x);"
			"float cos(float x); float2 cos(float2 x); float3 cos(float3 x); float4 cos(float4 x);"
			"float cosh(float x); float2 cosh(float2 x); float3 cosh(float3 x); float4 cosh(float4 x);"
			"float tan(float x); float2 tan(float2 x); float3 tan(float3 x); float4 tan(float4 x);"
			"float tanh(float x); float2 tanh(float2 x); float3 tanh(float3 x); float4 tanh(float4 x);"
			"float asin(float x); float2 asin(float2 x); float3 asin(float3 x); float4 asin(float4 x);"
			"float acos(float x); float2 acos(float2 x); float3 acos(float3 x); float4 acos(float4 x);"
			"float atan(float x); float2 atan(float2 x); float3 atan(float3 x); float4 atan(float4 x);"
			"float atan2(float x, float y); float2 atan2(float2 x, float2 y); float3 atan2(float3 x, float3 y); float4 atan2(float4 x, float4 y);"
			"void sincos(float x, out float s, out float c); void sincos(float2 x, out float2 s, out float2 c); void sincos(float3 x, out float3 s, out float3 c); void sincos(float4 x, out float4 s, out float4 c);"
			"float exp(float x); float2 exp(float2 x); float3 exp(float3 x); float4 exp(float4 x);"
			"float exp2(float x); float2 exp2(float2 x); float3 exp2(float3 x); float4 exp2(float4 x);"
			"float log(float x); float2 log(float2 x); float3 log(float3 x); float4 log(float4 x);"
			"float log2(float x); float2 log2(float2 x); float3 log2(float3 x); float4 log2(float4 x);"
			"float log10(float x); float2 log10(float2 x); float3 log10(float3 x); float4 log10(float4 x);"
			"float sqrt(float x); float2 sqrt(float2 x); float3 sqrt(float3 x); float4 sqrt(float4 x);"
			"float rsqrt(float x); float2 rsqrt(float2 x); float3 rsqrt(float3 x); float4 rsqrt(float4 x);"
			"float ceil(float x); float2 ceil(float2 x); float3 ceil(float3 x); float4 ceil(float4 x);"
			"float floor(float x); float2 floor(float2 x); float3 floor(float3 x); float4 floor(float4 x);"
			"float frac(float x); float2 frac(float2 x); float3 frac(float3 x); float4 frac(float4 x);"
			"float trunc(float x); float2 trunc(float2 x); float3 trunc(float3 x); float4 trunc(float4 x);"
			"float round(float x); float2 round(float2 x); float3 round(float3 x); float4 round(float4 x);"
			"float ddx(float x); float2 ddx(float2 x); float3 ddx(float3 x); float4 ddx(float4 x);"
			"float ddy(float x); float2 ddy(float2 x); float3 ddy(float3 x); float4 ddy(float4 x);"
			"float radians(float x); float2 radians(float2 x); float3 radians(float3 x); float4 radians(float4 x);"
			"float degrees(float x); float2 degrees(float2 x); float3 degrees(float3 x); float4 degrees(float4 x);"
			"float noise(float x); float noise(float2 x); float noise(float3 x); float noise(float4 x);"
			"float length(float x); float length(float2 x); float length(float3 x); float length(float4 x);"
			"float normalize(float x); float2 normalize(float2 x); float3 normalize(float3 x); float4 normalize(float4 x);"
			"float2x2 transpose(float2x2 m); float3x3 transpose(float3x3 m); float4x4 transpose(float4x4 m);"
			"float determinant(float2x2 m); float determinant(float3x3 m); float determinant(float4x4 m);"
			"int asint(float x); int2 asint(float2 x); int3 asint(float3 x); int4 asint(float4 x);"
			"uint asuint(float x); uint2 asuint(float2 x); uint3 asuint(float3 x); uint4 asuint(float4 x);"
			"float asfloat(int x); float2 asfloat(int2 x); float3 asfloat(int3 x); float4 asfloat(int4 x);"
			"float asfloat(uint x); float2 asfloat(uint2 x); float3 asfloat(uint3 x); float4 asfloat(uint4 x);"
			"float mul(float a, float b);"
			"float2 mul(float a, float2 b); float3 mul(float a, float3 b); float4 mul(float a, float4 b);"
			"float2 mul(float2 a, float b); float3 mul(float3 a, float b); float4 mul(float4 a, float b);"
			"float2x2 mul(float a, float2x2 b); float3x3 mul(float a, float3x3 b); float4x4 mul(float a, float4x4 b);"
			"float2x2 mul(float2x2 a, float b); float3x3 mul(float3x3 a, float b); float4x4 mul(float4x4 a, float b);"
			"float2 mul(float2 a, float2x2 b); float3 mul(float3 a, float3x3 b); float4 mul(float4 a, float4x4 b);"
			"float2 mul(float2x2 a, float2 b); float3 mul(float3x3 a, float3 b); float4 mul(float4x4 a, float4 b);"
			"float mad(float m, float a, float b); float2 mad(float2 m, float2 a, float2 b); float3 mad(float3 m, float3 a, float3 b); float4 mad(float4 m, float4 a, float4 b);"
			"float dot(float x, float y); float dot(float2 x, float2 y); float dot(float3 x, float3 y); float dot(float4 x, float4 y);"
			"float3 cross(float3 x, float3 y);"
			"float distance(float x, float y); float distance(float2 x, float2 y); float distance(float3 x, float3 y); float distance(float4 x, float4 y);"
			"float pow(float x, float y); float2 pow(float2 x, float2 y); float3 pow(float3 x, float3 y); float4 pow(float4 x, float4 y);"
			"float modf(float x, out float ip); float2 modf(float2 x, out float2 ip); float3 modf(float3 x, out float3 ip); float4 modf(float4 x, out float4 ip);"
			"float frexp(float x, out float exp); float2 frexp(float2 x, out float2 exp); float3 frexp(float3 x, out float3 exp); float4 frexp(float4 x, out float4 exp);"
			"float ldexp(float x, float exp); float2 ldexp(float2 x, float2 exp); float3 ldexp(float3 x, float3 exp); float4 ldexp(float4 x, float4 exp);"
			"float min(float x, float y); float2 min(float2 x, float2 y); float3 min(float3 x, float3 y); float4 min(float4 x, float4 y);"
			"float max(float x, float y); float2 max(float2 x, float2 y); float3 max(float3 x, float3 y); float4 max(float4 x, float4 y);"
			"float clamp(float x, float min, float max); float2 clamp(float2 x, float2 min, float2 max); float3 clamp(float3 x, float3 min, float3 max); float4 clamp(float4 x, float4 min, float4 max);"
			"float saturate(float x); float2 saturate(float2 x); float3 saturate(float3 x); float4 saturate(float4 x);"
			"float step(float y, float x); float2 step(float2 y, float2 x); float3 step(float3 y, float3 x); float4 step(float4 y, float4 x);"
			"float smoothstep(float min, float max, float x); float2 smoothstep(float2 min, float2 max, float2 x); float3 smoothstep(float3 min, float3 max, float3 x); float4 smoothstep(float4 min, float4 max, float4 x);"
			"float lerp(float x, float y, float s); float2 lerp(float2 x, float2 y, float2 s); float3 lerp(float3 x, float3 y, float3 s); float4 lerp(float4 x, float4 y, float4 s);"
			"float reflect(float i, float n); float2 reflect(float2 i, float2 n); float3 reflect(float3 i, float3 n); float4 reflect(float4 i, float4 n);"
			"float refract(float i, float n, float r); float2 refract(float2 i, float2 n, float r); float3 refract(float3 i, float3 n, float r); float4 refract(float4 i, float4 n, float r);"
			"float faceforward(float n, float i, float ng); float2 faceforward(float2 n, float2 i, float2 ng); float3 faceforward(float3 n, float3 i, float3 ng); float4 faceforward(float4 n, float4 i, float4 ng);"
			"float4 tex2D(sampler2D s, float2 t);"
			"float4 tex2Doffset(sampler2D s, float2 t, int2 o);"
			"float4 tex2Dlod(sampler2D s, float4 t);"
			"float4 tex2Dlodoffset(sampler2D s, float4 t, int2 o);"
			"float4 tex2Dgather(sampler2D s, float2 t);"
			"float4 tex2Dgatheroffset(sampler2D s, float2 t, int2 o);"
			"float4 tex2Dfetch(sampler2D s, int2 t);"
			"float4 tex2Dbias(sampler2D s, float4 t);"
			"int2 tex2Dsize(sampler2D s, int lod);";
		static const std::unordered_map<std::string, unsigned int> sIntrinsicOperators =
			boost::assign::map_list_of
			("abs", EffectNodes::Expression::Abs)
			("sign", EffectNodes::Expression::Sign)
			("rcp", EffectNodes::Expression::Rcp)
			("all", EffectNodes::Expression::All)
			("any", EffectNodes::Expression::Any)
			("sin", EffectNodes::Expression::Sin)
			("sinh", EffectNodes::Expression::Sinh)
			("cos", EffectNodes::Expression::Cos)
			("cosh", EffectNodes::Expression::Cosh)
			("tan", EffectNodes::Expression::Tan)
			("tanh", EffectNodes::Expression::Tanh)
			("asin", EffectNodes::Expression::Asin)
			("acos", EffectNodes::Expression::Acos)
			("atan", EffectNodes::Expression::Atan)
			("atan2", EffectNodes::Expression::Atan2)
			("sincos", EffectNodes::Expression::SinCos)
			("exp", EffectNodes::Expression::Exp)
			("exp2", EffectNodes::Expression::Exp2)
			("log", EffectNodes::Expression::Log)
			("log2", EffectNodes::Expression::Log2)
			("log10", EffectNodes::Expression::Log10)
			("sqrt", EffectNodes::Expression::Sqrt)
			("rsqrt", EffectNodes::Expression::Rsqrt)
			("ceil", EffectNodes::Expression::Ceil)
			("floor", EffectNodes::Expression::Floor)
			("frac", EffectNodes::Expression::Frac)
			("trunc", EffectNodes::Expression::Trunc)
			("round", EffectNodes::Expression::Round)
			("radians", EffectNodes::Expression::Radians)
			("degrees", EffectNodes::Expression::Degrees)
			("ddx", EffectNodes::Expression::PartialDerivativeX)
			("ddy", EffectNodes::Expression::PartialDerivativeY)
			("noise", EffectNodes::Expression::Noise)
			("length", EffectNodes::Expression::Length)
			("normalize", EffectNodes::Expression::Normalize)
			("transpose", EffectNodes::Expression::Transpose)
			("determinant", EffectNodes::Expression::Determinant)
			("asint", EffectNodes::Expression::BitCastFloat2Int)
			("asuint", EffectNodes::Expression::BitCastFloat2Uint)
			("asfloat", EffectNodes::Expression::BitCastInt2Float)
			("asfloat", EffectNodes::Expression::BitCastUint2Float)
			("mul", EffectNodes::Expression::Mul)
			("mad", EffectNodes::Expression::Mad)
			("dot", EffectNodes::Expression::Dot)
			("cross", EffectNodes::Expression::Cross)
			("distance", EffectNodes::Expression::Distance)
			("pow", EffectNodes::Expression::Pow)
			("modf", EffectNodes::Expression::Modf)
			("frexp", EffectNodes::Expression::Frexp)
			("ldexp", EffectNodes::Expression::Ldexp)
			("min", EffectNodes::Expression::Min)
			("max", EffectNodes::Expression::Max)
			("clamp", EffectNodes::Expression::Clamp)
			("saturate", EffectNodes::Expression::Saturate)
			("step", EffectNodes::Expression::Step)
			("smoothstep", EffectNodes::Expression::SmoothStep)
			("lerp", EffectNodes::Expression::Lerp)
			("reflect", EffectNodes::Expression::Reflect)
			("refract", EffectNodes::Expression::Refract)
			("faceforward", EffectNodes::Expression::FaceForward)
			("tex2D", EffectNodes::Expression::Tex)
			("tex2Doffset", EffectNodes::Expression::TexOffset)
			("tex2Dlod", EffectNodes::Expression::TexLevel)
			("tex2Dlodoffset",	EffectNodes::Expression::TexLevelOffset)
			("tex2Dgather", EffectNodes::Expression::TexGather)
			("tex2Dgatheroffset", EffectNodes::Expression::TexGatherOffset)
			("tex2Dbias", EffectNodes::Expression::TexBias)
			("tex2Dfetch", EffectNodes::Expression::TexFetch)
			("tex2Dsize", EffectNodes::Expression::TexSize);

		if (!sIntrinsicsInitialized)
		{
			sIntrinsicsInitialized = EffectParser(sIntrinsics).Parse(sIntrinsicOverloads);
		}
		#pragma endregion

		EffectNodes::Function const *overload = nullptr;
		unsigned int overloadCount = 0;
		unsigned int intrinsicOperator = EffectNodes::Expression::None;

		const auto it = this->mSymbolStack.find(call.CalleeName);

		if (it != this->mSymbolStack.end() && !it->second.empty())
		{
			const auto &scopes = it->second;

			for (auto it = scopes.rbegin(), end = scopes.rend(); it != end; ++it)
			{
				if (it->first > this->mCurrentScope)
				{
					continue;
				}
		
				const EffectTree::Node &symbol = this->mAST[it->second];
		
				if (!symbol.Is<EffectNodes::Function>())
				{
					continue;
				}

				const EffectNodes::Function &function = symbol.As<EffectNodes::Function>();

				if (!function.Parameters != EffectTree::Null)
				{
					if (call.ArgumentCount == 0)
					{
						overload = &function;
						overloadCount = 1;
						break;
					}
					else
					{
						continue;
					}
				}
				else if (call.ArgumentCount != function.ParameterCount)
				{
					continue;
				}

				const int result = CompareFunctions(this->mAST, call, this->mAST, &function, this->mAST, overload, call.ArgumentCount);

				if (result < 0)
				{
					overload = &function;
					overloadCount = 1;
				}
				else if (result == 0)
				{
					++overloadCount;
				}
			}
		}

		const EffectNodes::Function *intrinsics = &sIntrinsics[sIntrinsics.Get().As<EffectNodes::Root>().NextDeclaration].As<EffectNodes::Function>();

		do
		{
			if (::strcmp(intrinsics->Name, call.CalleeName) == 0)
			{
				if (call.ArgumentCount != intrinsics->ParameterCount)
				{
					if (overloadCount == 0)
					{
						intrinsic = true;
					}
					break;
				}

				const int result = CompareFunctions(this->mAST, call, sIntrinsics, intrinsics, intrinsic ? sIntrinsics : this->mAST, overload, call.ArgumentCount);

				if (result < 0)
				{
					overload = intrinsics;
					overloadCount = 1;

					intrinsic = true;
					intrinsicOperator = sIntrinsicOperators.at(call.CalleeName);
				}
				else if (result == 0)
				{
					++overloadCount;
				}
			}

			if (intrinsics->NextDeclaration != EffectTree::Null)
			{
				intrinsics = &sIntrinsics[intrinsics->NextDeclaration].As<EffectNodes::Function>();
			}
			else
			{
				intrinsics = nullptr;
			}
		}
		while (intrinsics != nullptr);

		if (overloadCount == 1)
		{
			call.Type = overload->ReturnType;
			call.Callee = intrinsic ? intrinsicOperator : overload->Index;

			return true;
		}
		else
		{
			ambiguous = overloadCount > 1;

			return false;
		}
	}

	void EffectParser::PushScope(EffectTree::Index parent)
	{
		if (parent != EffectTree::Null || this->mParentStack.empty())
		{
			this->mParentStack.push(parent);
		}
		else
		{
			this->mParentStack.push(this->mParentStack.top());
		}

		++this->mCurrentScope;
	}
	bool EffectParser::PushSymbol(EffectTree::Index symbol)
	{
		std::string name;
		const EffectTree::Node &node = this->mAST[symbol];
	
		if (node.Is<EffectNodes::Function>())
		{
			name = node.As<EffectNodes::Function>().Name;
		}
		else if (node.Is<EffectNodes::Variable>())
		{
			name = node.As<EffectNodes::Variable>().Name;
		}
		else if (node.Is<EffectNodes::Struct>() && node.As<EffectNodes::Struct>().Name != nullptr)
		{
			name = node.As<EffectNodes::Struct>().Name;
		}
		else
		{
			return false;
		}

		if (FindSymbol(name, this->mCurrentScope, true) != EffectTree::Null && !node.Is<EffectNodes::Function>())
		{
			return false;
		}
	
		this->mSymbolStack[name].push_back(std::make_pair(this->mCurrentScope, symbol));

		return true;
	}
	void EffectParser::PopScope()
	{
		for (auto it = this->mSymbolStack.begin(), end = this->mSymbolStack.end(); it != end; ++it)
		{
			auto &scopes = it->second;

			if (scopes.empty())
			{
				continue;
			}
		
			for (auto it = scopes.begin(); it != scopes.end();)
			{
				if (it->first >= this->mCurrentScope)
				{
					it = scopes.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		this->mParentStack.pop();

		--this->mCurrentScope;
	}
}