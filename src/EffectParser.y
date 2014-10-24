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
	#include "EffectTree.hpp"
	
	#include <stack>
	#include <unordered_map>

	#define YYLTYPE ReShade::EffectTree::Location

	namespace ReShade
	{
		class													EffectParser
		{
			public:
				typedef unsigned int							Scope;

			public:
				EffectParser(EffectTree &ast);
				~EffectParser(void);

				inline bool										Parse(const std::string &source)
				{
					std::string errors;
					return Parse(source, errors);
				}
				bool											Parse(const std::string &source, std::string &errors);

				void											Error(const YYLTYPE &location, unsigned int code, const char *message, ...);
				void											Warning(const YYLTYPE &location, unsigned int code, const char *message, ...);

			public:
				Scope											GetCurrentScope(void) const;
				EffectTree::Index								GetCurrentParent(void) const;
				inline EffectTree::Index						FindSymbol(const std::string &name) const
				{
					return FindSymbol(name, GetCurrentScope(), false);
				}
				EffectTree::Index 								FindSymbol(const std::string &name, Scope scope, bool exclusive = false) const;
				bool											ResolveCall(EffectNodes::Call &call, bool &intrinsic, bool &ambiguous) const;
			
				void 											PushScope(EffectTree::Index parent = EffectTree::Null);
				bool											PushSymbol(EffectTree::Index symbol);
				void 											PopScope(void);

			public:
				EffectTree &									mAST;
				void *											mLexer;
				void *											mParser;
				int												mNextLexerState;

			private:
				std::string										mErrors;
				unsigned int									mErrorsCount;
				Scope											mCurrentScope;
				std::stack<EffectTree::Index>					mParentStack;
				std::unordered_map<
					std::string,
					std::vector<
						std::pair<Scope, EffectTree::Index>
					>
				>												mSymbolStack;

			private:
				EffectParser(const EffectParser &);
		
				void 											operator =(const EffectParser &);
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
			bool												Bool;
			int													Int;
			unsigned int										Uint;
			float												Float;
			struct { const char *p; std::size_t len; } 			String;
		};

		ReShade::EffectTree::Index								Node;
		ReShade::EffectNodes::Type								Type;
	} l;
	struct
	{
		ReShade::EffectTree::Index								Index, States[ReShade::EffectNodes::Pass::StateCount], Properties[ReShade::EffectNodes::Variable::PropertyCount];
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

%type<l>		RULE_TYPE
%type<l>		RULE_TYPE_SCALAR
%type<l>		RULE_TYPE_VECTOR
%type<l>		RULE_TYPE_MATRIX

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

%type<y.Index>	RULE_ATTRIBUTE
%type<y.Index>	RULE_ATTRIBUTE_LIST
%type<y.Index>	RULE_ATTRIBUTES

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
		parser.mAST.Get().As<EffectNodes::List>().Link(parser.mAST, $1);
	}
	| RULE_MAIN RULE_MAIN_DECLARATION
	{
		parser.mAST.Get().As<EffectNodes::List>().Link(parser.mAST, $2);
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

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_IDENTIFIER_SYMBOL
	{
		EffectNodes::Call &node = parser.mAST.Add<EffectNodes::Call>(@1);
		node.CalleeName = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Callee = $1.Node;

		@$ = node.Location, $$ = node.Index;
	}
	| TOK_IDENTIFIER_FIELD
	{
		EffectNodes::Call &node = parser.mAST.Add<EffectNodes::Call>(@1);
		node.CalleeName = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_IDENTIFIER_SEMANTIC
	: TOK_IDENTIFIER_SEMANTIC { @$ = @1, $$.String = $1.String; }
	| TOK_IDENTIFIER { @$ = @1, $$.String = $1.String; }
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
	| "vector" "<" RULE_TYPE_SCALAR "," TOK_LITERAL_INT ">"
	{
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
	| "matrix" "<" RULE_TYPE_SCALAR "," TOK_LITERAL_INT "," TOK_LITERAL_INT ">"
	{
		if ($5.Int < 1 || $5.Int > 4)
		{
			parser.Error(@5, 3052, "matrix dimensions must be between 1 and 4");
			YYERROR;
		}
		if ($7.Int < 1 || $7.Int > 4)
		{
			parser.Error(@7, 3052, "matrix dimensions must be between 1 and 4");
			YYERROR;
		}

		@$ = @1;
		$$.Type = $3.Type;
		$$.Type.Rows = $5.Uint;
		$$.Type.Cols = $7.Uint;
	}
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
		if ($1.Type.HasQualifiers($2.Uint))
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE_PARAMETER
	{
		if ($1.Type.HasQualifiers($2.Uint))
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_MODIFIER
	{
		if ($1.Type.HasQualifiers($2.Uint))
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}

		@$ = @1, $$.Type.Qualifiers = $1.Type.Qualifiers | $2.Uint;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_INTERPOLATION
	{
		if ($1.Type.HasQualifiers($2.Uint))
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
		if (parser.mAST[$1].Is<EffectNodes::Sequence>())
		{
			EffectNodes::Sequence &node = parser.mAST[$1].As<EffectNodes::Sequence>();
			node.Type = parser.mAST[$3].As<EffectNodes::RValue>().Type;

			@$ = node.Location, $$ = node.Index;

			node.Link(parser.mAST, $3);
		}
		else
		{
			EffectNodes::Sequence &node = parser.mAST.Add<EffectNodes::Sequence>();
			node.Type = parser.mAST[$3].As<EffectNodes::RValue>().Type;

			@$ = node.Location, $$ = node.Index;

			node.Link(parser.mAST, $1);
			parser.mAST[$$].As<EffectNodes::Sequence>().Link(parser.mAST, $3);
		}
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

		@$ = node.Location, $$ = node.Index;
	}
	| TOK_LITERAL_INT
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Int;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Int[0] = $1.Int;

		@$ = node.Location, $$ = node.Index;
	}
	| TOK_LITERAL_ENUM
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Int;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Int[0] = $1.Int;

		@$ = node.Location, $$ = node.Index;
	}
	| TOK_LITERAL_FLOAT
	{
		EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
		node.Type.Class = EffectNodes::Type::Float;
		node.Type.Qualifiers = EffectNodes::Type::Const;
		node.Type.Rows = node.Type.Cols = 1;
		node.Value.Float[0] = $1.Float;

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		if (parser.mAST[$$].Is<EffectNodes::Call>())
		{
			parser.mAST[$$].As<EffectNodes::Call>().Arguments = $3;
		}
		else if (parser.mAST[$$].Is<EffectNodes::Constructor>())
		{
			parser.mAST[$$].As<EffectNodes::Constructor>().Arguments = $3;
		}
	}
	;
RULE_EXPRESSION_FUNCTION_ARGUMENTS
	: RULE_EXPRESSION_ASSIGNMENT
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_EXPRESSION_FUNCTION_ARGUMENTS "," RULE_EXPRESSION_ASSIGNMENT
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $3);
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
				node.Operator = callee;
				node.Operands[0] = parser.mAST[arguments].As<EffectNodes::List>()[0];
				node.Operands[1] = parser.mAST[arguments].As<EffectNodes::List>()[1];
				node.Operands[2] = parser.mAST[arguments].As<EffectNodes::List>()[2];

				@$ = node.Location, $$ = node.Index;
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
			const EffectNodes::List &arguments = parser.mAST[constructor.Arguments].As<EffectNodes::List>();

			for (unsigned int i = 0; i < arguments.Length; ++i)
			{
				const EffectNodes::RValue &argument = parser.mAST[arguments[i]].As<EffectNodes::RValue>();

				if (!argument.Type.IsNumeric())
				{
					parser.Error(argument.Location, 3017, "cannot convert non-numeric types");
					YYERROR;
				}

				if (!argument.Is<EffectNodes::Literal>())
				{
					literal = false;
				}

				elements += argument.Type.Rows * argument.Type.Cols;
			}

			if (elements != constructor.Type.Rows * constructor.Type.Cols)
			{
				parser.Error(@1, 3014, "incorrect number of arguments to numeric-type constructor");
				YYERROR;
			}

			if (literal)
			{
				const EffectNodes::Type type = constructor.Type;
				const EffectTree::Index arguments = constructor.Arguments;

				EffectNodes::Literal &node = parser.mAST.Add<EffectNodes::Literal>(@1);
				node.Type = type;

				for (unsigned int i = 0, j = 0; i < parser.mAST[arguments].As<EffectNodes::List>().Length; ++i)
				{
					const EffectNodes::Literal &argument = parser.mAST[parser.mAST[arguments].As<EffectNodes::List>()[i]].As<EffectNodes::Literal>();

					for (unsigned int k = 0; k < argument.Type.Rows * argument.Type.Cols; ++k, ++j)
					{
						switch (type.Class)
						{
							case EffectNodes::Type::Bool:
								node.Value.Bool[j] = argument.Cast<int>(k);
								break;
							case EffectNodes::Type::Int:
								node.Value.Int[j] = argument.Cast<int>(k);
								break;
							case EffectNodes::Type::Uint:
								node.Value.Uint[j] = argument.Cast<unsigned int>(k);
								break;
							case EffectNodes::Type::Float:
								node.Value.Float[j] = argument.Cast<float>(k);
								break;
						}
					}
				}

				@$ = node.Location, $$ = node.Index;
			}
			else if (arguments.Length == 1)
			{
				const EffectNodes::Type type = constructor.Type;
				const EffectTree::Index arguments = constructor.Arguments;

				EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
				node.Type = type;
				node.Operator = EffectNodes::Expression::Cast;
				node.Operands[0] = parser.mAST[arguments].As<EffectNodes::List>()[0];

				@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

			@$ = node.Location, $$ = node.Index;
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

			@$ = node.Location, $$ = node.Index;
		}
		else if (type.IsStruct())
		{
			EffectTree::Index symbol = EffectTree::Null;

			if (parser.mAST[type.Definition].As<EffectNodes::Struct>().HasFields())
			{
				EffectNodes::List fields = parser.mAST[parser.mAST[type.Definition].As<EffectNodes::Struct>().Fields].As<EffectNodes::List>();

				for (unsigned int i = 0; i < fields.Length; ++i)
				{
					const EffectNodes::Variable &field = parser.mAST[fields[i]].As<EffectNodes::Variable>();
		
					if (::strncmp(field.Name, $3.String.p, $3.String.len) == 0)
					{
						symbol = field.Index;
						break;
					}
				}
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

			@$ = node.Location, $$ = node.Index;
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

			@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

			@$ = node.Location, $$ = node.Index;
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
				node.Type = $2.Type;

				union EffectNodes::Literal::Value value;
				::memset(&value, 0, sizeof(value));

				for (unsigned int k = 0, size = std::min(expressionType.Rows * expressionType.Cols, $2.Type.Rows * $2.Type.Cols); k < size; ++k)
				{
					switch ($2.Type.Class)
					{
						case EffectNodes::Type::Bool:
							value.Bool[k] = node.Cast<int>(k);
							break;
						case EffectNodes::Type::Int:
							value.Int[k] = node.Cast<int>(k);
							break;
						case EffectNodes::Type::Uint:
							value.Uint[k] = node.Cast<unsigned int>(k);
							break;
						case EffectNodes::Type::Float:
							value.Float[k] = node.Cast<float>(k);
							break;
					}
				}

				node.Value = value;

				@$ = @1, $$ = node.Index;
			}
			else
			{
				EffectNodes::Expression &node = parser.mAST.Add<EffectNodes::Expression>(@1);
				node.Type = $2.Type;
				node.Operator = EffectNodes::Expression::Cast;
				node.Operands[0] = $4;

				@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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
	
		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_STATEMENT_LIST RULE_STATEMENT
	{
		EffectNodes::StatementBlock &node = parser.mAST[$1].As<EffectNodes::StatementBlock>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $2);
	}
	;
RULE_STATEMENT_BLOCK
	: "{" "}"
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);

		@$ = node.Location, $$ = node.Index;
	}
	| "{" { parser.PushScope(); } RULE_STATEMENT_LIST "}"
	{
		parser.PopScope();

		@$ = @1, $$ = $3; parser.mAST[$$].Location = @$;
	}
	;
RULE_STATEMENT_BLOCK_SCOPELESS
	: "{" "}"
	{
		EffectNodes::StatementBlock &node = parser.mAST.Add<EffectNodes::StatementBlock>(@1);

		@$ = node.Location, $$ = node.Index;
	}
	| "{" RULE_STATEMENT_LIST "}"
	{
		@$ = @1, $$ = $2; parser.mAST[$$].Location = @$;
	}
	;

RULE_STATEMENT_SINGLE
	: RULE_ATTRIBUTES RULE_STATEMENT_EXPRESSION
	{
		@$ = @2, $$ = $2;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_DECLARATION
	{
		@$ = @2, $$ = $2;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_IF
	{
		EffectNodes::If &node = parser.mAST[$2].As<EffectNodes::If>();
		node.Attributes = $1;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_SWITCH
	{
		EffectNodes::Switch &node = parser.mAST[$2].As<EffectNodes::Switch>();
		node.Attributes = $1;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_FOR
	{
		EffectNodes::For &node = parser.mAST[$2].As<EffectNodes::For>();
		node.Attributes = $1;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_WHILE
	{
		EffectNodes::While &node = parser.mAST[$2].As<EffectNodes::While>();
		node.Attributes = $1;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_ATTRIBUTES RULE_STATEMENT_JUMP
	{
		EffectNodes::Jump &node = parser.mAST[$2].As<EffectNodes::Jump>();
		node.Attributes = $1;

		@$ = node.Location, $$ = node.Index;
	}
	| error RULE_STATEMENT_SINGLE { $$ = $2; }
	;
RULE_STATEMENT_EXPRESSION
	: ";"
	{
		EffectNodes::ExpressionStatement &node = parser.mAST.Add<EffectNodes::ExpressionStatement>(@1);

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_EXPRESSION ";"
	{
		EffectNodes::ExpressionStatement &node = parser.mAST.Add<EffectNodes::ExpressionStatement>(@1);
		node.Expression = $1;

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_STATEMENT_DECLARATION
	: RULE_VARIABLE ";"
	{
		@$ = @1, $$ = $1;
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
	
		@$ = node.Location, $$ = node.Index;
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
	
		@$ = node.Location, $$ = node.Index;
	}
	;

RULE_STATEMENT_SWITCH
	: "switch" "(" RULE_EXPRESSION ")" "{" "}"
	{
		parser.Warning(@1, 5002, "switch statement contains no 'case' or 'default' labels");

		@$ = @1, $$ = 0;
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

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_STATEMENT_CASE
	: RULE_STATEMENT_CASE_LABEL_LIST RULE_STATEMENT
	{
		EffectNodes::StatementBlock &statements = parser.mAST.Add<EffectNodes::StatementBlock>(@2);
		$$ = statements.Index;
		statements.Link(parser.mAST, $2);

		EffectNodes::Case &node = parser.mAST.Add<EffectNodes::Case>(@1);
		node.Labels = $1;
		node.Statements = $$;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_STATEMENT_CASE RULE_STATEMENT
	{
		EffectNodes::Case &node = parser.mAST[$1].As<EffectNodes::Case>();

		@$ = node.Location, $$ = node.Index;

		EffectNodes::StatementBlock &statements = parser.mAST[node.Statements].As<EffectNodes::StatementBlock>();
		statements.Link(parser.mAST, $2);
	}
	;
RULE_STATEMENT_CASE_LIST
	: RULE_STATEMENT_CASE
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_STATEMENT_CASE_LIST RULE_STATEMENT_CASE
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $2);
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

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_STATEMENT_CASE_LABEL_LIST
	: RULE_STATEMENT_CASE_LABEL
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_STATEMENT_CASE_LABEL_LIST RULE_STATEMENT_CASE_LABEL
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $2);
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

		@$ = node.Location, $$ = node.Index;
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
	
		@$ = node.Location, $$ = node.Index;
	}
	| "do" RULE_STATEMENT "while" "(" RULE_EXPRESSION ")" ";"
	{
		EffectNodes::While &node = parser.mAST.Add<EffectNodes::While>(@1);
		node.DoWhile = true;
		node.Condition = $5;
		node.Statements = $2;

		@$ = node.Location, $$ = node.Index;
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

		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Return;

		@$ = node.Location, $$ = node.Index;
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

			EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
			node.Mode = EffectNodes::Jump::Return;
			node.Value = $2;

			@$ = node.Location, $$ = node.Index;
		}
	}
	| "break" ";"
	{
		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Break;

		@$ = node.Location, $$ = node.Index;
	}
	| "continue" ";"
	{
		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Continue;

		@$ = node.Location, $$ = node.Index;
	}
	| "discard" ";"
	{
		EffectNodes::Jump &node = parser.mAST.Add<EffectNodes::Jump>(@1);
		node.Mode = EffectNodes::Jump::Discard;

		@$ = node.Location, $$ = node.Index;
	}
	;

 /* Attributes ------------------------------------------------------------------------------- */

 RULE_ATTRIBUTE
	: "[" RULE_IDENTIFIER_NAME "]"
	{
		EffectNodes::Literal &value = parser.mAST.Add<EffectNodes::Literal>(@1);
		value.Type.Class = EffectNodes::Type::String;
		value.Value.String = parser.mAST.AddString($2.String.p, $2.String.len);
		
		@$ = value.Location, $$ = value.Index;
	}
	| "[" error "]"
	{
		$$ = EffectTree::Null;
	}
	;
RULE_ATTRIBUTE_LIST
	: RULE_ATTRIBUTE
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);
		node.Link(parser.mAST, $1);

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_ATTRIBUTE_LIST RULE_ATTRIBUTE
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();
		node.Link(parser.mAST, $2);

		@$ = node.Location, $$ = node.Index;
	}
	;

RULE_ATTRIBUTES
	:
	{
		$$ = EffectTree::Null;
	}
	| RULE_ATTRIBUTE_LIST
	;

 /* Annotations ------------------------------------------------------------------------------ */

RULE_ANNOTATION
	: RULE_TYPE_SCALAR RULE_IDENTIFIER_NAME "=" RULE_EXPRESSION_LITERAL ";"
	{
		EffectNodes::Annotation &node = parser.mAST.Add<EffectNodes::Annotation>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Value = $4;

		@$ = node.Location, $$ = node.Index;
	}
	| "string" RULE_IDENTIFIER_NAME "=" TOK_LITERAL_STRING ";"
	{
		EffectNodes::Literal &value = parser.mAST.Add<EffectNodes::Literal>(@4);
		value.Type = $1.Type;
		value.Value.String = parser.mAST.AddString($4.String.p, $4.String.len);

		const EffectTree::Index valueIndex = value.Index;

		EffectNodes::Annotation &node = parser.mAST.Add<EffectNodes::Annotation>(@2);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Value = valueIndex;

		@$ = node.Location, $$ = node.Index;
	}
	| error ";"
	{
		$$ = EffectTree::Null;
	}
	;
RULE_ANNOTATION_LIST
	: RULE_ANNOTATION
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_ANNOTATION_LIST RULE_ANNOTATION
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $2);
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		if (!parser.PushSymbol(node.Index))
		{
			parser.Error(@2, 3003, "redefinition of '%s'", node.Name);
			YYERROR;
		}

		@$ = node.Location, $$ = node.Index;
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

		EffectNodes::List &fields = parser.mAST[$2].As<EffectNodes::List>();

		for (unsigned int i = 0; i < fields.Length; ++i)
		{
			EffectNodes::Variable &field = parser.mAST[fields[i]].As<EffectNodes::Variable>();
	
			field.Type.Class = $1.Type.Class;
			field.Type.Qualifiers = $1.Type.Qualifiers;
			field.Type.Rows = $1.Type.Rows;
			field.Type.Cols = $1.Type.Cols;
			field.Type.Definition = $1.Type.Definition;
		}

		@$ = @1, $$ = fields.Index;
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
		@$ = @1, $$ = $1;

		parser.mAST[$1].As<EffectNodes::List>().Link(parser.mAST, $2);
	}
	;

RULE_STRUCT_FIELD_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY ":" RULE_IDENTIFIER_SEMANTIC
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = parser.mAST.AddString($4.String.p, $4.String.len);

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_STRUCT_FIELD_DECLARATOR_LIST
	: RULE_STRUCT_FIELD_DECLARATOR
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_STRUCT_FIELD_DECLARATOR_LIST "," RULE_STRUCT_FIELD_DECLARATOR
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $3);
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

		EffectNodes::List &declarators = parser.mAST[$2].As<EffectNodes::List>();

		for (unsigned int i = 0; i < declarators.Length; ++i)
		{
			EffectNodes::Variable &variable = parser.mAST[declarators[i]].As<EffectNodes::Variable>();
			variable.Type.Class = $1.Type.Class;
			variable.Type.Qualifiers = $1.Type.Qualifiers;
			variable.Type.Rows = $1.Type.Rows;
			variable.Type.Cols = $1.Type.Cols;
			variable.Type.Definition = $1.Type.Definition;

			if (!parser.PushSymbol(variable.Index))
			{
				parser.Error(@1, 3003, "redefinition of '%s'", variable.Name);
				YYERROR;
			}
	
			if (variable.HasInitializer())
			{
				const EffectNodes::RValue &initializer = parser.mAST[variable.Initializer].As<EffectNodes::RValue>();
			
				if ((initializer.Type.Rows < variable.Type.Rows || initializer.Type.Cols < variable.Type.Cols) && !initializer.Type.IsScalar())
				{
					parser.Error(initializer.Location, 3017, "cannot implicitly convert these vector types");
					YYERROR;
				}
				if (!EffectNodes::Type::Compatible(initializer.Type, variable.Type))
				{
					parser.Error(initializer.Location, 3020, "type mismatch");
					YYERROR;
				}

				if (initializer.Type.Rows > variable.Type.Rows || initializer.Type.Cols > variable.Type.Cols)
				{
					parser.Warning(initializer.Location, 3206, "implicit truncation of vector type");
				}

				if (parent == EffectTree::Null && !initializer.Is<EffectNodes::Literal>())
				{
					parser.Error(initializer.Location, 3011, "initial value must be a literal expression");
					YYERROR;
				}
			}
			else if ($1.Type.IsNumeric() && $1.Type.HasQualifier(EffectNodes::Type::Const))
			{
				parser.Error(variable.Location, 3012, "missing initial value for '%s'", variable.Name);
				YYERROR;
			}
		}

		@$ = @1, $$ = declarators.Index;
	}
	;

RULE_VARIABLE_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_ANNOTATIONS
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Annotations = $3;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_ANNOTATIONS "=" RULE_EXPRESSION_ASSIGNMENT
	{
		if ($2.Type.IsArray())
		{
			parser.Error(@4, 3009, "cannot initialize arrays");
			YYERROR;
		}

		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Annotations = $3;
		node.Initializer = $5;

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_ANNOTATIONS "{" RULE_VARIABLE_PROPERTY_LIST "}"
	{
		if ($2.Type.IsArray())
		{
			parser.Error(@4, 3009, "cannot initialize arrays");
			YYERROR;
		}

		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Annotations = $3;

		for (unsigned int i = 0; i < EffectNodes::Variable::PropertyCount; ++i)
		{
			node.Properties[i] = $5.Properties[i];
		}

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_VARIABLE_DECLARATOR_LIST
	: RULE_VARIABLE_DECLARATOR
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_VARIABLE_DECLARATOR_LIST "," RULE_VARIABLE_DECLARATOR
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $3);
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
	: RULE_TYPE_FULLYSPECIFIED RULE_FUNCTION_DECLARATOR
	{
		if ($1.Type.Qualifiers != 0)
		{
			parser.Error(@1, 3047, "function return type cannot have any qualifiers");
			YYERROR;
		}

		EffectNodes::Function &node = parser.mAST[$2].As<EffectNodes::Function>();
		node.ReturnType = $1.Type;
		node.ReturnType.Qualifiers = EffectNodes::Type::Const;

		parser.PushSymbol(node.Index);

		@$ = @1, $$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_FUNCTION_DECLARATOR ":" RULE_IDENTIFIER_SEMANTIC
	{
		if ($1.Type.IsVoid())
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
		node.ReturnSemantic = parser.mAST.AddString($4.String.p, $4.String.len);

		parser.PushSymbol(node.Index);

		@$ = @1, $$ = node.Index;
	}
	;
RULE_FUNCTION_DEFINITION
	: RULE_FUNCTION_PROTOTYPE
	{
		const EffectNodes::Function &node = parser.mAST[$1].As<EffectNodes::Function>();

		parser.PushScope(node.Index);

		if (node.HasParameters())
		{
			const EffectNodes::List &parameters = parser.mAST[node.Parameters].As<EffectNodes::List>();
	
			for (unsigned int i = 0; i < parameters.Length; ++i)
			{
				const EffectNodes::Variable &parameter = parser.mAST[parameters[i]].As<EffectNodes::Variable>();

				if (!parser.PushSymbol(parameter.Index))
				{
					parser.Error(@1, 3003, "redefinition of '%s'", parameter.Name);
					YYERROR;
				}
			}
		}
	}
	  RULE_STATEMENT_BLOCK
	{
		const EffectTree::Index index = parser.GetCurrentParent();
		parser.PopScope();

		EffectNodes::Function &node = parser.mAST[index].As<EffectNodes::Function>();
		node.Definition = $3;

		@$ = node.Location, $$ = node.Index;
	}
	;

RULE_FUNCTION_DECLARATOR
	: RULE_IDENTIFIER_SYMBOL "(" ")"
	{
		EffectNodes::Function &node = parser.mAST.Add<EffectNodes::Function>(@1);
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		if (!$1.Type.HasQualifier(EffectNodes::Type::Out))
		{
			$1.Type.Qualifiers |= EffectNodes::Type::In;
		}

		EffectNodes::Variable &node = parser.mAST[$2].As<EffectNodes::Variable>();
		node.Type.Class = $1.Type.Class;
		node.Type.Qualifiers = $1.Type.Qualifiers;
		node.Type.Rows = $1.Type.Rows;
		node.Type.Cols = $1.Type.Cols;
		node.Type.Definition = $1.Type.Definition;

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_FUNCTION_PARAMETER_LIST
	: RULE_FUNCTION_PARAMETER
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_FUNCTION_PARAMETER_LIST "," RULE_FUNCTION_PARAMETER
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $3);
	}
	;

RULE_FUNCTION_PARAMETER_DECLARATOR
	: RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);

		@$ = node.Location, $$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY ":" RULE_IDENTIFIER_SEMANTIC
	{
		EffectNodes::Variable &node = parser.mAST.Add<EffectNodes::Variable>(@1);
		node.Type.ArrayLength = $2.Type.ArrayLength;
		node.Name = parser.mAST.AddString($1.String.p, $1.String.len);
		node.Semantic = parser.mAST.AddString($4.String.p, $4.String.len);

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
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

		@$ = node.Location, $$ = node.Index;
	}
	| "pass" RULE_ANNOTATIONS "{" RULE_PASSSTATE_LIST "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@1);
		node.Annotations = $2;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = $4.States[i];
		}

		@$ = node.Location, $$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@1);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Annotations = $3;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = EffectTree::Null;
		}

		@$ = node.Location, $$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" RULE_PASSSTATE_LIST "}"
	{
		EffectNodes::Pass &node = parser.mAST.Add<EffectNodes::Pass>(@1);
		node.Name = parser.mAST.AddString($2.String.p, $2.String.len);
		node.Annotations = $3;

		for (unsigned int i = 0; i < EffectNodes::Pass::StateCount; ++i)
		{
			node.States[i] = $5.States[i];
		}

		@$ = node.Location, $$ = node.Index;
	}
	;
RULE_PASS_LIST
	: RULE_PASS
	{
		EffectNodes::List &node = parser.mAST.Add<EffectNodes::List>(@1);

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $1);
	}
	| RULE_PASS_LIST RULE_PASS
	{
		EffectNodes::List &node = parser.mAST[$1].As<EffectNodes::List>();

		@$ = node.Location, $$ = node.Index;

		node.Link(parser.mAST, $2);
	}
	;

RULE_PASSSTATE
	: TOK_IDENTIFIER_PASSSTATE "=" RULE_EXPRESSION_LITERAL ";"
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
	| TOK_IDENTIFIER_PASSSTATE "=" RULE_IDENTIFIER_NAME ";"
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

namespace ReShade
{
	EffectParser::EffectParser(EffectTree &ast) : mAST(ast), mLexer(nullptr), mParser(nullptr), mNextLexerState(0), mCurrentScope(0)
	{
		if (yylex_init(&this->mLexer) != 0)
		{
			return;
		}

		// Add root node
		this->mAST.Add<EffectNodes::List>();
	}
	EffectParser::~EffectParser(void)
	{
		if (this->mLexer != nullptr)
		{
			yylex_destroy(this->mLexer);
		}
	}

	bool 														EffectParser::Parse(const std::string &source, std::string &errors)
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

	void														EffectParser::Error(const YYLTYPE &location, unsigned int code, const char *message, ...)
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
	void														EffectParser::Warning(const YYLTYPE &location, unsigned int code, const char *message, ...)
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

	EffectParser::Scope											EffectParser::GetCurrentScope(void) const
	{
		return this->mCurrentScope;
	}
	EffectTree::Index											EffectParser::GetCurrentParent(void) const
	{
		return !this->mParentStack.empty() ? this->mParentStack.top() : EffectTree::Null;
	}
	EffectTree::Index 											EffectParser::FindSymbol(const std::string &name, Scope scope, bool exclusive) const
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

	void														EffectParser::PushScope(EffectTree::Index parent)
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
	bool														EffectParser::PushSymbol(EffectTree::Index symbol)
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
	void 														EffectParser::PopScope(void)
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