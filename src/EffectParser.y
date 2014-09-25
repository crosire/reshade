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
	#include <assert.h>
	#include <string>
	#include <algorithm>
	#include <stack>
	#include <unordered_map>
	#include <unordered_set>
	
	namespace ReShade
	{
		struct													EffectTree
		{
			public:
				typedef std::size_t 							Index;
				struct 											Location
				{
					const char *								Source;
					unsigned int								Line, Column;
				};
				struct 											Node
				{			
					template <typename T> inline bool			Is(void) const
					{
						return this->Type == T::NodeType;
					}
					template <typename T> inline T &			As(void)
					{
						//assert(Is<T>());

						return *reinterpret_cast<T *>(reinterpret_cast<unsigned char *>(this) + sizeof(Node));
					}
					template <typename T> inline const T &		As(void) const
					{
						//assert(Is<T>());

						return *reinterpret_cast<const T *>(reinterpret_cast<const unsigned char *>(this) + sizeof(Node));
					}
				
					int											Type;
					Index										Index;
					Location									Location;
				};

				static const Index								Root = 1;
				
			public:
				EffectTree(void);
				EffectTree(const unsigned char *data, std::size_t size);
			
				inline Node &									operator [](Index index)
				{
					return Get(index);
				}
				inline const Node &								operator [](Index index) const
				{
					return Get(index);
				}
				
				template <typename T> inline Node &				Add(void)
				{
					const Location location = { nullptr, 0, 0 };
					return Add<T>(location);
				}
				template <typename T> Node &					Add(const Location &location) // Any pointers to nodes are no longer valid after this call
				{
					const Index index = this->mNodes.size();
					
					this->mNodes.resize(index + sizeof(Node) + sizeof(T));
					
					EffectTree::Node &node = operator [](index);
					node.Type = T::NodeType;
					node.Index = index;
					node.Location = location;
					
					return node;
				}
				inline Node &									Get(Index index = Root)
				{
					return *reinterpret_cast<Node *>(&this->mNodes[index]);
				}
				inline const Node &								Get(Index index = Root) const
				{
					return *reinterpret_cast<const Node *>(&this->mNodes[index]);
				}
				
			private:
				std::vector<unsigned char>						mNodes;
		};

		class													EffectParser
		{
			public:
				typedef unsigned int							Scope;

			public:
				EffectParser(void);
				~EffectParser(void);

				inline bool 									Parse(const std::string &source, EffectTree &ast, bool append = true)
				{
					std::string errors;
					return Parse(source, ast, errors, append);
				}
				bool											Parse(const std::string &source, EffectTree &ast, std::string &errors, bool append = true);

			public:
				Scope											GetScope(void) const;
				Scope											GetScope(EffectTree::Index &node);
				EffectTree::Index								GetSymbol(const std::string &name, int type) const;
				EffectTree::Index 								GetSymbol(const std::string &name, int type, Scope scope, bool exclusive) const;
				EffectTree::Index								GetSymbolOverload(EffectTree::Index call, bool &ambiguous) const;
				
				void											Error(const EffectTree::Location &location, unsigned int code, const char *message, ...);
				void											Warning(const EffectTree::Location &location, unsigned int code, const char *message, ...);
			
				void 											PushScope(EffectTree::Index node = 0);
				void 											PopScope(void);

				bool											CreateSymbol(EffectTree::Index node);
				std::string										CreateSymbolLookupName(EffectTree::Index node);
				const char *									CreateString(const char *s);
				const char *									CreateString(const char *s, std::size_t length);

			public:
				void *											mLexer;
				int												mNextLexerState, mLexerStateAnnotation;
				void *											mParser;
				EffectTree *									mParserTree;
				bool											mAppendToRoot;
					
			private:
				std::string										mErrors;
				bool											mFailed;
				Scope											mScope;
				std::stack<EffectTree::Index>					mScopeNodes;
				std::unordered_set<std::string>					mStringPool;
				std::unordered_map<std::string, std::vector<std::pair<Scope, std::string>>> mLookup;
				std::unordered_map<std::string, EffectTree::Index> 	mSymbols;

			private:
				EffectParser(const EffectParser &);
				
				void 											operator =(const EffectParser &);
		};

		namespace Nodes
		{
			enum class											Operator
			{
				None											= 0,
				Plus,
				Minus,
				Add,
				Substract,
				Multiply,
				Divide,
				Modulo,
				Increase,
				Decrease,
				PostIncrease,
				PostDecrease,
				Index,
				Less,
				Greater,
				LessOrEqual,
				GreaterOrEqual,
				Equal,
				Unequal,
				LeftShift,
				RightShift,
				BitwiseAnd,
				BitwiseOr,
				BitwiseXor,
				BitwiseNot,
				LogicalAnd,
				LogicalOr,
				LogicalXor,
				LogicalNot,
				Cast
			};
			enum class											Attribute
			{
				None											= 0,
				Flatten,
				Branch,
				ForceCase,
				Loop,
				FastOpt,
				Unroll
			};
			struct 												Type
			{
				enum											Class
				{
					Void,
					Bool,
					Int,
					Uint,
					Half,
					Float,
					Double,
					Texture,
					Sampler,
					Struct,
					String,
					Function
				};
				enum class										Qualifier
				{
					None										= 0,
					Const										= 1 << 0,
					Volatile									= 1 << 1,
					Static										= 1 << 2,
					Extern										= 1 << 3,
					Uniform										= 1 << 4,
					Inline										= 1 << 5,
					In											= 1 << 6,
					Out											= 1 << 7,
					InOut										= In | Out,
					Precise										= 1 << 8,
					NoInterpolation								= 1 << 10,
					NoPerspective								= 1 << 11,
					Linear										= 1 << 12,
					Centroid									= 1 << 13,
					Sample										= 1 << 14,
					RowMajor									= 1 << 15,
					ColumnMajor									= 1 << 16,
					Unorm										= 1 << 17,
					Snorm										= 1 << 18
				};
				enum class										Compatibility
				{
					None 										= 0,
					Match,
					Promotion,
					Implicit,
					ImplicitWithPromotion,
					UpwardVectorPromotion
				};
				
				static Compatibility							Compatible(const Type &left, const Type &right);
				
				static const Type								Undefined;
				
				inline bool										IsArray(void) const
				{
					return this->ElementsDimension > 0;
				}
				inline	bool									IsMatrix(void) const
				{
					return this->Rows >= 1 && this->Cols > 1;
				}
				inline bool										IsVector(void) const
				{
					return this->Rows > 1 && !IsMatrix();
				}
				inline bool										IsScalar(void) const
				{
					return !IsArray() && !IsMatrix() && !IsVector() && !IsStruct();
				}
				inline bool										IsNumeric(void) const
				{
					return this->Class >= Bool && this->Class <= Double;
				}
				inline bool										IsIntegral(void) const
				{
					return this->Class >= Bool && this->Class <= Uint;
				}
				inline bool										IsFloatingPoint(void) const
				{
					return this->Class == Half || this->Class == Float || this->Class == Double;
				}
				inline bool										IsSampler(void) const
				{
					return this->Class == Sampler;
				}
				inline bool										IsTexture(void) const
				{
					return this->Class == Texture;
				}
				inline bool										IsStruct(void) const
				{
					return this->Class == Struct;
				}
				
				inline bool										HasQualifier(Type::Qualifier qualifier) const
				{
					return (this->Qualifiers & static_cast<unsigned int>(qualifier)) == static_cast<unsigned int>(qualifier);
				}

				Class											Class;
				unsigned int									Qualifiers;
				unsigned int									Rows : 4, Cols : 4;
				unsigned int									Elements[16], ElementsDimension;
				EffectTree::Index								Definition;
			};

			struct 												Aggregate
			{
				enum { NodeType = 1 };
			
				EffectTree::Index								Find(const EffectTree &ast, std::size_t index) const
				{
					if (index >= this->Length)
					{
						return 0;
					}
					else if (index < 16)
					{
						return this->Nodes[index];
					}
					else
					{
						return ast[this->Continuation].As<Aggregate>().Find(ast, index - 16);
					}
				}
				void	 										Link(EffectTree &ast, EffectTree::Index node)
				{
					if (node == 0)
					{
						return;
					}

					if (this->Length >= 16)
					{
						if (this->Continuation == 0)
						{
							EffectTree::Index current = (*reinterpret_cast<const EffectTree::Node *>(reinterpret_cast<const unsigned char *>(this) - sizeof(EffectTree::Node))).Index;
							EffectTree::Index continuation = ast.Add<Aggregate>().Index; // Do NOT use the "this" pointer after this call anymore, memory might have changed
							
							ast[current].As<Aggregate>().Continuation = continuation;
							ast[continuation].As<Aggregate>().Previous = current;
							ast[continuation].As<Aggregate>().Link(ast, node);
						}
						else
						{
							ast[this->Continuation].As<Aggregate>().Link(ast, node);
						}
					}
					else
					{
						this->Nodes[this->Length] = node;

						Increment(ast);
					}
				}
				void											Increment(EffectTree &ast)
				{
					if (this->Previous != 0)
					{
						ast[this->Previous].As<Aggregate>().Increment(ast);
					}
					
					this->Length++;
				}
				
				std::size_t										Length;
				EffectTree::Index								Previous, Continuation, Nodes[16];
			};
			struct												Expression
			{
				enum { NodeType = 2 };
				
				Type											Type;
			};
			struct												ExpressionSequence : public Aggregate
			{
				enum { NodeType = 29 };
			};
			struct 												Literal : public Expression
			{		
				enum { NodeType = 3 };

				enum											Enum
				{
					ZERO,
					ONE,
					SRCCOLOR,
					SRCALPHA,
					INVSRCCOLOR,
					INVSRCALPHA,
					DESTCOLOR,
					DESTALPHA,
					INVDESTCOLOR,
					INVDESTALPHA,
					POINT,
					LINEAR,
					ANISOTROPIC,
					CLAMP,
					REPEAT,
					MIRROR,
					BORDER,
					R8,
					R32F,
					RG8,
					RGBA8,
					RGBA16,
					RGBA16F,
					RGBA32F,
					DXT1,
					DXT3,
					DXT5,
					LATC1,
					LATC2,
					NONE,
					BACK,
					FRONT,
					WIREFRAME,
					SOLID,
					CCW,
					CW,
					ADD,
					SUBSTRACT,
					REVSUBSTRACT,
					MIN,
					MAX,
					KEEP,
					REPLACE,
					INVERT,
					INCR,
					INCRSAT,
					DECR,
					DECRSAT,
					NEVER,
					ALWAYS,
					LESS,
					GREATER,
					LESSEQUAL,
					GREATEREQUAL,
					EQUAL,
					NOTEQUAL
				};
				
				union
				{
					int											AsInt;
					unsigned int								AsUint;
					float										AsFloat;
					double 										AsDouble;
					const char *								AsString;
				} Value;
			};
			struct 												Reference : public Expression
			{
				enum { NodeType = 4 };
				
				EffectTree::Index								Symbol;
			};
			struct 												UnaryExpression : public Expression
			{
				enum { NodeType = 5 };
				
				EffectTree::Index								Operand;
				Operator										Operator;
			};
			struct 												BinaryExpression : public Expression
			{
				enum { NodeType = 6 };
				
				EffectTree::Index								Left, Right;
				Operator										Operator;
			};
			struct												Assignment : public BinaryExpression
			{
				enum { NodeType = 28 };
			};
			struct 												Conditional : public Expression
			{
				enum { NodeType = 7 };
				
				EffectTree::Index								Condition, ExpressionTrue, ExpressionFalse;
			};
			struct 												Call : public Expression
			{
				enum { NodeType = 8 };
				
				const char *									CalleeName;
				EffectTree::Index								Left, Callee, Parameters;
			};
			struct 												Constructor : public Expression
			{
				enum { NodeType = 9 };
				
				EffectTree::Index								Parameters;
			};
			struct 												Field : public Expression
			{
				enum { NodeType = 10 };
				
				EffectTree::Index								Left, Callee;
			};
			struct 												Swizzle : public Expression
			{
				enum { NodeType = 11 };
				
				EffectTree::Index								Left;
				int												Offsets[4];
			};
			struct 												StatementBlock : public Aggregate
			{
				enum { NodeType = 27 };
			};
			struct 												ExpressionStatement
			{
				enum { NodeType = 12 };
				
				EffectTree::Index								Expression;
			};
			struct 												DeclarationStatement
			{
				enum { NodeType = 13 };
				
				EffectTree::Index								Declaration;
			};
			struct 												Selection
			{
				enum { NodeType = 14 };
				
				enum											Mode
				{
					If,
					Switch,
					Case
				};
			
				Mode											Mode;
				Attribute										Attributes;
				EffectTree::Index								Condition;
				union
				{
					EffectTree::Index							Statement;
					EffectTree::Index							StatementTrue;
				};
				EffectTree::Index								StatementFalse;
			};
			struct 												Iteration
			{
				enum { NodeType = 15 };
				
				enum											Mode
				{
					For,
					While,
					DoWhile
				};
				
				Mode 											Mode;
				Attribute 										Attributes;
				EffectTree::Index								Initialization, Condition, Expression, Statement;
			};
			struct 												Jump
			{
				enum { NodeType = 16 };
				
				enum											Mode
				{
					Break,
					Continue,
					Return,
					Discard
				};
				
				Mode 											Mode;
				EffectTree::Index								Expression;
			};
			struct 												Annotation
			{
				enum { NodeType = 17 };
				
				const char *									Name;
				Type											Type;
				union
				{
					int											AsInt;
					unsigned int								AsUint;
					float										AsFloat;
					double										AsDouble;
					const char *								AsString;
				} Value;
			};
			struct 												Function
			{
				enum { NodeType = 18 };
				
				inline bool										HasArguments(void) const
				{
					return this->Arguments != 0;
				}
				inline bool										HasDefinition(void) const
				{
					return this->Definition != 0;
				}

				const char *									Name;
				EffectTree::Index								Arguments, Definition;
				Type											ReturnType;
				const char *									ReturnSemantic;
			};
			struct 												Variable
			{
				enum { NodeType = 19 };
				
				inline bool										HasSemantic(void) const
				{
					return this->Semantic != nullptr;
				}
				inline bool 									HasInitializer(void) const
				{
					return this->Initializer != 0;
				}
				inline bool										HasAnnotation(void) const
				{
					return this->Annotations != 0;
				}
								
				const char *									Name;
				Type											Type;
				EffectTree::Index								Initializer, Annotations, Parent;
				const char *									Semantic;
				bool											Argument;
			};
			struct 												Struct
			{
				enum { NodeType = 20 };
				
				const char *									Name;
				EffectTree::Index								Fields;
			};
			struct 												Technique
			{
				enum { NodeType = 23 };
				
				const char *									Name;
				EffectTree::Index								Passes, Annotations;
			};
			struct 												Pass
			{
				enum { NodeType = 24 };
				
				const char *									Name;
				EffectTree::Index								States, Annotations;
			};
			struct 												State
			{
				enum { NodeType = 25 };
				
				enum											Type
				{
					MinFilter,
					MagFilter,
					MipFilter,
					AddressU,
					AddressV,
					AddressW,
					MipLODBias,
					MaxAnisotropy,
					MinLOD,
					MaxLOD,
					SRGB,
					Texture,
					Width,
					Height,
					MipLevels,
					Format,
					VertexShader,
					PixelShader,
					RenderTarget0,
					RenderTarget1,
					RenderTarget2,
					RenderTarget3,
					RenderTarget4,
					RenderTarget5,
					RenderTarget6,
					RenderTarget7,
					RenderTargetWriteMask,
					BlendEnable,
					SrcBlend,
					DestBlend,
					BlendOp,
					BlendOpAlpha,
					DepthEnable,
					DepthWriteMask,
					DepthFunc,
					StencilEnable,
					StencilReadMask,
					StencilWriteMask,
					StencilRef,
					StencilFunc,
					StencilPass,
					StencilFail,
					StencilZFail,
					SRGBWriteEnable
				};
				
				int												Type;
				union
				{
					int											AsInt;
					float										AsFloat;
					EffectTree::Index							AsNode;
				} Value;
			};
			struct												Typedef
			{
				enum { NodeType = 26 };
				
				const char *									Name;
				Type											Type;
			};
		}
	}

	#define YYLTYPE ReShade::EffectTree::Location
	#define YYLTYPE_IS_DECLARED 1
	#define YYLLOC_DEFAULT(Current, Rhs, N) { if (N) { (Current).Source = YYRHSLOC(Rhs, 1).Source, (Current).Line = YYRHSLOC(Rhs, 1).Line, (Current).Column = YYRHSLOC(Rhs, 1).Column; } else { (Current).Source = nullptr, (Current).Line = YYRHSLOC(Rhs, 0).Line, (Current).Column = YYRHSLOC(Rhs, 0).Column; } }
	#define YY_USER_ACTION { yylloc->Line = yylineno; yylloc->Column = yycolumn; yycolumn += yyleng; }
}
%code
{
	#include "EffectLexer.h"
	
	#define AST (*parser.mParserTree)
	#define yyerror(yylloc, yyscanner, context, message, ...) parser.Error(*(yylloc), 3000, message, __VA_ARGS__)
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
			double 												Double;
			struct { const char *p; std::size_t len; } 			String;
		};

		ReShade::Nodes::Type									Type;
		ReShade::EffectTree::Index								Node;
	} l;
	struct
	{
		ReShade::EffectTree::Index								n;
	} y;
}

/* ----------------------------------------------------------------------------------------------
|  Tokens																						|
---------------------------------------------------------------------------------------------- */

%token			TOK_EOF 0										"end of file"

%token<l>		TOK_TECHNIQUE									"technique"
%token<l>		TOK_PASS										"pass"
%token<l>		TOK_STRUCT										"struct"
%token<l>		TOK_TYPEDEF										"typedef"

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

%token<l>		TOK_UNIFORM										"uniform"
%token<l>		TOK_STATIC										"static"
%token<l>		TOK_EXTERN										"extern"
%token<l>		TOK_INLINE										"inline"
%token<l>		TOK_CONST										"const"
%token<l>		TOK_VOLATILE									"volatile"
%token<l>		TOK_PRECISE										"precise"
%token<l>		TOK_ROWMAJOR									"row_major"
%token<l>		TOK_COLUMNMAJOR									"column_major"
%token<l>		TOK_UNORM										"unorm"
%token<l>		TOK_SNORM										"snorm"
%token<l>		TOK_IN											"in"
%token<l>		TOK_OUT											"out"
%token<l>		TOK_INOUT										"inout"
%token<l>		TOK_NOINTERPOLATION								"nointerpolation"
%token<l>		TOK_NOPERSPECTIVE								"noperspective"
%token<l>		TOK_LINEAR										"linear"
%token<l>		TOK_CENTROID									"centroid"
%token<l>		TOK_SAMPLE										"sample"

%token<l>		TOK_LITERAL_BOOL								"boolean literal"
%token<l>		TOK_LITERAL_INT									"integral literal"
%token<l>		TOK_LITERAL_FLOAT								"floating point literal"
%token<l>		TOK_LITERAL_DOUBLE								"double literal"
%token<l>		TOK_LITERAL_ENUM								"enumeration"
%token<l>		TOK_LITERAL_STRING								"string literal"

%token<l>		TOK_IDENTIFIER									"identifier"
%token<l>		TOK_IDENTIFIER_TYPE								"type name"
%token<l>		TOK_IDENTIFIER_SYMBOL							"symbol name"
%token<l>		TOK_IDENTIFIER_FIELD							"member field"
%token<l>		TOK_IDENTIFIER_STATE							"effect state"
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
%token<l>		TOK_TYPE_HALF									"half"
%token<l>		TOK_TYPE_HALFV									"halfN"
%token<l>		TOK_TYPE_HALFM									"halfNxN"
%token<l>		TOK_TYPE_FLOAT									"float"
%token<l>		TOK_TYPE_FLOATV									"floatN"
%token<l>		TOK_TYPE_FLOATM									"floatNxN"
%token<l>		TOK_TYPE_DOUBLE									"double"
%token<l>		TOK_TYPE_DOUBLEV								"doubleN"
%token<l>		TOK_TYPE_DOUBLEM								"doubleNxN"
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
%token<l>		TOK_OPERATOR_GREATEREQUAL						">="
%token<l>		TOK_OPERATOR_LESSEQUAL							"<="
%token<l>		TOK_OPERATOR_GREATER 62							">"
%token<l>		TOK_OPERATOR_LESS 60							"<"
%token<l>		TOK_OPERATOR_LOGNOT 33							"!"
%token<l>		TOK_OPERATOR_BITNOT 126							"~"
%token<l>		TOK_OPERATOR_LOGAND								"&&"
%token<l>		TOK_OPERATOR_LOGXOR								"^^"
%token<l>		TOK_OPERATOR_LOGOR								"||"
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
%token<l>		TOK_OPERATOR_ASSIGNMENT 61						"="
%token<l>		TOK_OPERATOR_INCREMENT							"++"
%token<l>		TOK_OPERATOR_DECREMENT							"--"
%token<l>		TOK_OPERATOR_PLUS 43							"+"
%token<l>		TOK_OPERATOR_MINUS 45							"-"
%token<l>		TOK_OPERATOR_MULTIPLY 42						"*"
%token<l>		TOK_OPERATOR_DIVIDE 47							"/"
%token<l>		TOK_OPERATOR_MODULO 37							"%"
%token<l>		TOK_OPERATOR_BITAND 38							"&"
%token<l>		TOK_OPERATOR_BITXOR 94							"^"
%token<l>		TOK_OPERATOR_BITOR 124							"|"
%token<l>		TOK_OPERATOR_LEFTSHIFT							"<<"
%token<l>		TOK_OPERATOR_RIGHTSHIFT							">>"

%right			TOK_THEN TOK_ELSE

/* ----------------------------------------------------------------------------------------------
|  Rules																						|
---------------------------------------------------------------------------------------------- */

%type<l>		RULE_IDENTIFIER_SYMBOL
%type<y.n>		RULE_IDENTIFIER_FUNCTION
%type<l>		RULE_IDENTIFIER_NAME
%type<l>		RULE_IDENTIFIER_SEMANTIC

%type<l>		RULE_TYPE
%type<l>		RULE_TYPE_SCALAR
%type<l>		RULE_TYPE_VECTOR
%type<l>		RULE_TYPE_MATRIX

%type<l>		RULE_TYPE_STORAGE
%type<l>		RULE_TYPE_STORAGE_FUNCTION
%type<l>		RULE_TYPE_STORAGE_ARGUMENT
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

%type<l>		RULE_ATTRIBUTE

%type<y.n>		RULE_ANNOTATION
%type<y.n>		RULE_ANNOTATION_LIST
%type<y.n>		RULE_ANNOTATIONS

%type<y.n>		RULE_EXPRESSION
%type<y.n>		RULE_EXPRESSION_LITERAL
%type<y.n>		RULE_EXPRESSION_PRIMARY
%type<y.n>		RULE_EXPRESSION_FUNCTION_PARAMETERS
%type<y.n>		RULE_EXPRESSION_FUNCTION
%type<y.n>		RULE_EXPRESSION_POSTFIX
%type<y.n>		RULE_EXPRESSION_UNARY
%type<y.n>		RULE_EXPRESSION_MULTIPLICATIVE
%type<y.n>		RULE_EXPRESSION_ADDITIVE
%type<y.n>		RULE_EXPRESSION_SHIFT
%type<y.n>		RULE_EXPRESSION_RELATIONAL
%type<y.n>		RULE_EXPRESSION_EQUALITY
%type<y.n>		RULE_EXPRESSION_BITAND
%type<y.n>		RULE_EXPRESSION_BITXOR
%type<y.n>		RULE_EXPRESSION_BITOR
%type<y.n>		RULE_EXPRESSION_LOGAND
%type<y.n>		RULE_EXPRESSION_LOGXOR
%type<y.n>		RULE_EXPRESSION_LOGOR
%type<y.n>		RULE_EXPRESSION_CONDITIONAL
%type<y.n>		RULE_EXPRESSION_ASSIGNMENT
%type<y.n>		RULE_EXPRESSION_INITIALIZER
%type<y.n>		RULE_EXPRESSION_INITIALIZER_LIST
%type<y.n>		RULE_EXPRESSION_INITIALIZEROBJECT
%type<y.n>		RULE_EXPRESSION_INITIALIZEROBJECTSTATE
%type<y.n>		RULE_EXPRESSION_INITIALIZEROBJECTSTATE_LIST

%type<y.n>		RULE_STATEMENT
%type<y.n>		RULE_STATEMENT_SCOPELESS
%type<y.n>		RULE_STATEMENT_LIST
%type<y.n>		RULE_STATEMENT_COMPOUND
%type<y.n>		RULE_STATEMENT_COMPOUND_SCOPELESS
%type<y.n>		RULE_STATEMENT_SIMPLE
%type<y.n>		RULE_STATEMENT_DECLARATION
%type<y.n>		RULE_STATEMENT_EXPRESSION
%type<y.n>		RULE_STATEMENT_SELECTION
%type<y.n>		RULE_STATEMENT_CASE
%type<y.n>		RULE_STATEMENT_CASE_LIST
%type<y.n>		RULE_STATEMENT_CASELABEL
%type<y.n>		RULE_STATEMENT_CASELABEL_LIST
%type<y.n>		RULE_STATEMENT_ITERATION
%type<y.n>		RULE_STATEMENT_FOR_INITIALIZER
%type<y.n>		RULE_STATEMENT_FOR_CONDITION
%type<y.n>		RULE_STATEMENT_FOR_ITERATOR
%type<y.n>		RULE_STATEMENT_JUMP
%type<y.n>		RULE_STATEMENT_TYPEDEF

%type<y.n>		RULE_DECLARATION
%type<y.n>		RULE_DECLARATION_STRUCT
%type<y.n>		RULE_DECLARATION_STRUCTFIELD
%type<y.n>		RULE_DECLARATION_STRUCTFIELD_LIST
%type<y.n>		RULE_DECLARATION_STRUCTFIELDDECLARATOR
%type<y.n>		RULE_DECLARATION_STRUCTFIELDDECLARATOR_LIST
%type<y.n>		RULE_DECLARATION_VARIABLE
%type<y.n>		RULE_DECLARATION_VARIABLEDECLARATOR
%type<y.n>		RULE_DECLARATION_VARIABLEDECLARATOR_LIST
%type<y.n>		RULE_DECLARATION_FUNCTIONPROTOTYPE
%type<y.n>		RULE_DECLARATION_FUNCTIONDEFINITION
%type<y.n>		RULE_DECLARATION_FUNCTIONDECLARATOR
%type<y.n>		RULE_DECLARATION_FUNCTIONARGUMENT
%type<y.n>		RULE_DECLARATION_FUNCTIONARGUMENT_LIST
%type<y.n>		RULE_DECLARATION_TECHNIQUE
%type<y.n>		RULE_DECLARATION_PASS
%type<y.n>		RULE_DECLARATION_PASS_LIST
%type<y.n>		RULE_DECLARATION_PASSSTATE
%type<y.n>		RULE_DECLARATION_PASSSTATE_LIST

%type<y>		RULE_MAIN
%type<y.n>		RULE_MAIN_DECLARATION

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
		if (parser.mAppendToRoot && $1 != 0)
		{
			AST[ReShade::EffectTree::Root].As<ReShade::Nodes::Aggregate>().Link(AST, $1);
		}
	}
	| RULE_MAIN RULE_MAIN_DECLARATION
	{
		if (parser.mAppendToRoot && $2 != 0)
		{
			AST[ReShade::EffectTree::Root].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
		}
	}
	;
RULE_MAIN_DECLARATION
	: RULE_DECLARATION
	| RULE_DECLARATION_FUNCTIONDEFINITION
	| RULE_DECLARATION_TECHNIQUE
	| RULE_DECLARATION_STRUCT ";"
	{
		$$ = $1;
	}
	| RULE_STATEMENT_TYPEDEF
	{
		$$ = 0;
	}
	| error ";"
	{
		$$ = 0;
	}
	| ";"
	{
		$$ = 0;
	}
	;
	
 /* Identifiers ------------------------------------------------------------------------------ */

RULE_IDENTIFIER_SYMBOL
	: TOK_IDENTIFIER
	{
		if ($1.String.len > 2 && ($1.String.p[0] == '_' && $1.String.p[1] == '_' && $1.String.p[2] != '_'))
		{
			parser.Warning(@1, 3000, "names starting with '__' are reserved");
		}
		
		@$ = @1;
		$$ = $1;
	}
	| TOK_IDENTIFIER_SYMBOL
	;
RULE_IDENTIFIER_NAME
	: RULE_IDENTIFIER_SYMBOL
	;
RULE_IDENTIFIER_FUNCTION
	: RULE_TYPE
	{
		if ($1.Type.IsArray())
		{
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Constructor>(@1);
		ReShade::Nodes::Constructor &data = node.As<ReShade::Nodes::Constructor>();
		data.Type = $1.Type;
		data.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_SYMBOL
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Call>(@1);
		ReShade::Nodes::Call &data = node.As<ReShade::Nodes::Call>();
		data.CalleeName = parser.CreateString($1.String.p, $1.String.len);
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_IDENTIFIER_FIELD
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Call>(@1);
		ReShade::Nodes::Call &data = node.As<ReShade::Nodes::Call>();
		data.CalleeName = parser.CreateString($1.String.p, $1.String.len);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_IDENTIFIER_SEMANTIC
	: ":" TOK_IDENTIFIER_SEMANTIC
	{
		@$ = @1;
		$$.String = $2.String;
	}
	| ":" TOK_IDENTIFIER
	{
		@$ = @1;
		$$.String = $2.String;
	}
	;

 /* Types ------------------------------------------------------------------------------------ */

RULE_TYPE
	: RULE_TYPE_SCALAR
	| RULE_TYPE_VECTOR
	| RULE_TYPE_MATRIX
	| "texture"
	| "sampler"
	| "void"
	| TOK_IDENTIFIER_TYPE
	| RULE_DECLARATION_STRUCT
	{
		@$ = @1;
		$$.Type = ReShade::Nodes::Type::Undefined;
		$$.Type.Class = ReShade::Nodes::Type::Struct;
		$$.Type.Definition = $1;
	}
	;
RULE_TYPE_SCALAR
	: "bool"
	| "int"
	| "uint"
	| "half"
	| "float"
	| "double"
	;
RULE_TYPE_VECTOR
	: "boolN"
	| "intN"
	| "uintN"
	| "halfN"
	| "floatN"
	| "doubleN"
	| "vector"
	{
		@$ = @1;
		$$.Type = ReShade::Nodes::Type::Undefined;
		$$.Type.Class = ReShade::Nodes::Type::Float;
		$$.Type.Rows = 4;
		$$.Type.Cols = 1;
	}
	| "vector" "<" RULE_TYPE_SCALAR "," TOK_LITERAL_INT ">"
	{
		if ($5.Int < 1 || $5.Int > 4)
		{
			parser.Error(@5, 3052, "vector dimension must be between 1 and 4");
			YYERROR;
		}

		@$ = @1;
		$$ = $3;
		$$.Type.Rows = $5.Uint;
	}
	;
RULE_TYPE_MATRIX
	: "boolNxN"
	| "intNxN"
	| "uintNxN"
	| "halfNxN"
	| "floatNxN"
	| "doubleNxN"
	| "matrix"
	{
		@$ = @1;
		$$.Type = ReShade::Nodes::Type::Undefined;
		$$.Type.Class = ReShade::Nodes::Type::Float;
		$$.Type.Rows = 4;
		$$.Type.Cols = 4;
	}
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
		$$ = $3;
		$$.Type.Rows = $5.Uint;
		$$.Type.Cols = $7.Uint;
	}
	;
 
RULE_TYPE_STORAGE
	: "static"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Static);
	}
	| "extern"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Extern);
	}
	| "uniform"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Uniform);
	}
	| "volatile"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Volatile);
	}
	| "precise"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Precise);
	}
	;
RULE_TYPE_STORAGE_FUNCTION
	: "inline"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Inline);
	}
	;
RULE_TYPE_STORAGE_ARGUMENT
	: "in"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::In);
	}
	| "out"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Out);
	}
	| "inout"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::InOut);
	}
	;
RULE_TYPE_MODIFIER
	: "const"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
	}
	| "row_major"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::RowMajor);
	}
	| "column_major"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::ColumnMajor);
	}
	| "unorm"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Unorm);
	}
	| "snorm"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Snorm);
	}
	;
RULE_TYPE_INTERPOLATION
	: "nointerpolation"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::NoInterpolation);
	}
	| "noperspective"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::NoPerspective);
	}
	| "linear"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Linear);
	}
	| "centroid"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Centroid);
	}
	| "sample"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Type::Qualifier::Sample);
	}
	;
RULE_TYPE_QUALIFIER
	: RULE_TYPE_STORAGE
	{
		@$ = @1;
		$$.Type.Qualifiers = $1.Int;
	}
	| RULE_TYPE_STORAGE_FUNCTION
	{
		@$ = @1;
		$$.Type.Qualifiers = $1.Int;
	}
	| RULE_TYPE_STORAGE_ARGUMENT
	{
		@$ = @1;
		$$.Type.Qualifiers = $1.Int;
	}
	| RULE_TYPE_MODIFIER
	{
		@$ = @1;
		$$.Type.Qualifiers = $1.Int;
	}
	| RULE_TYPE_INTERPOLATION
	{
		@$ = @1;
		$$.Type.Qualifiers = $1.Int;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE
	{
		if (($1.Type.Qualifiers & $2.Int) != 0)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}
		
		@$ = @1;
		$$.Type.Qualifiers = $1.Type.Qualifiers | $2.Int;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE_FUNCTION
	{
		if (($1.Type.Qualifiers & $2.Int) != 0)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}
		
		@$ = @1;
		$$.Type.Qualifiers = $1.Type.Qualifiers | $2.Int;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_STORAGE_ARGUMENT
	{
		if (($1.Type.Qualifiers & $2.Int) != 0)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}
		
		@$ = @1;
		$$.Type.Qualifiers = $1.Type.Qualifiers | $2.Int;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_MODIFIER
	{
		if (($1.Type.Qualifiers & $2.Int) != 0)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}
		if (($2.Int == static_cast<int>(ReShade::Nodes::Type::Qualifier::RowMajor) && $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::ColumnMajor)) || ($2.Int == static_cast<int>(ReShade::Nodes::Type::Qualifier::ColumnMajor) && $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::RowMajor)))
		{
			parser.Error(@2, 3048, "matrix types cannot be both column_major and row_major");
			YYERROR;
		}
		
		@$ = @1;
		$$.Type.Qualifiers = $1.Type.Qualifiers | $2.Int;
	}
	| RULE_TYPE_QUALIFIER RULE_TYPE_INTERPOLATION
	{
		if (($1.Type.Qualifiers & $2.Int) != 0)
		{
			parser.Error(@2, 3048, "duplicate usages specified");
			YYERROR;
		}
		
		@$ = @1;
		$$.Type.Qualifiers = $1.Type.Qualifiers | $2.Int;
	}
	;
RULE_TYPE_ARRAY
	: "[" "]"
	{
		@$ = @1;
		$$.Type.ElementsDimension = 1;
		$$.Type.Elements[0] = 0;
	}
	| "[" RULE_EXPRESSION_CONDITIONAL "]"
	{
		if ($2 == 0)
		{
			YYERROR;
		}
		
		if (!AST[$2].Is<ReShade::Nodes::Literal>() || !AST[$2].As<ReShade::Nodes::Literal>().Type.IsScalar() || !AST[$2].As<ReShade::Nodes::Literal>().Type.IsIntegral())
		{
			parser.Error(@2, 3058, "array dimensions must be literal scalar expressions");
			YYERROR;
		}

		@$ = @1;
		$$.Type.ElementsDimension = 1;
		$$.Type.Elements[0] = AST[$2].As<ReShade::Nodes::Literal>().Value.AsUint;
	}
	| RULE_TYPE_ARRAY "[" "]"
	{
		parser.Error(@3, 3073, "secondary array dimensions must be explicit");
		YYERROR;
	}
	| RULE_TYPE_ARRAY "[" RULE_EXPRESSION_CONDITIONAL "]"
	{
		if ($3 == 0)
		{
			YYERROR;
		}
				
		if (!AST[$3].Is<ReShade::Nodes::Literal>() || !AST[$3].As<ReShade::Nodes::Literal>().Type.IsScalar() || !AST[$3].As<ReShade::Nodes::Literal>().Type.IsIntegral())
		{
			parser.Error(@3, 3058, "array dimensions must be literal scalar expressions");
			YYERROR;
		}

		@$ = @1;
		$$.Type.Elements[$$.Type.ElementsDimension++] = AST[$3].As<ReShade::Nodes::Literal>().Value.AsUint;
	}
	;
	
RULE_TYPE_FULLYSPECIFIED
	: RULE_TYPE
	| RULE_TYPE_QUALIFIER RULE_TYPE
	{
		if ($2.Type.IsIntegral() && ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Centroid) || $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::NoPerspective) || $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Sample)))
		{
			parser.Error(@1, 4576, "signature specifies invalid interpolation mode for integer component type");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Unorm) && $2.Type.Class != ReShade::Nodes::Type::Float)
		{
			parser.Error(@1, 3085, "unorm can not be used with this type");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Snorm) && $2.Type.Class != ReShade::Nodes::Type::Float)
		{
			parser.Error(@1, 3085, "snorm can not be used with this type");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Centroid) && !$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::NoPerspective))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Linear);
		}

		@$ = @1;
		$$ = $2;
		$$.Type.Qualifiers |= $1.Type.Qualifiers;
	}
	;

 /* Operators -------------------------------------------------------------------------------- */

RULE_OPERATOR_POSTFIX
	: "++"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::PostIncrease);
	}
	| "--"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::PostDecrease);
	}
	;
RULE_OPERATOR_UNARY
	: "+"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Plus);
	}
	| "-"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Minus);
	}
	| "~"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::BitwiseNot);
	}
	| "!"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::LogicalNot);
	}
	;
RULE_OPERATOR_MULTIPLICATIVE
	: "*"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Multiply);
	}
	| "/"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Divide);
	}
	| "%"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Modulo);
	}
	;
RULE_OPERATOR_ADDITIVE
	: "+"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Add);
	}
	| "-"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Substract);
	}
	;
RULE_OPERATOR_SHIFT
	: "<<"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::LeftShift);
	}
	| ">>"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::RightShift);
	}
	;
RULE_OPERATOR_RELATIONAL
	: "<"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Less);
	}
	| ">"
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Greater);
	}
	| "<="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::LessOrEqual);
	}
	| ">="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::GreaterOrEqual);
	}
	;
RULE_OPERATOR_EQUALITY
	: "=="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Equal);
	}
	| "!="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Unequal);
	}
	;
RULE_OPERATOR_ASSIGNMENT
	: "*="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Multiply);
	}
	| "/="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Divide);
	}
	| "%="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Modulo);
	}
	| "+="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Add);
	}
	| "-="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::Substract);
	}
	| "<<="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::LeftShift);
	}
	| ">>="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::RightShift);
	}
	| "&="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::BitwiseAnd);
	}
	| "^="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::BitwiseOr);
	}
	| "|="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::BitwiseXor);
	}
	| "="
	{
		@$ = @1;
		$$.Int = static_cast<int>(ReShade::Nodes::Operator::None);
	}
	;

 /* Attributes ------------------------------------------------------------------------------- */

RULE_ATTRIBUTE
	: "[" TOK_IDENTIFIER "]"
	{
		@$ = @1;

		if (::strncmp($2.String.p, "flatten", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::Flatten);
		}
		else if (::strncmp($2.String.p, "branch", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::Branch);
		}
		else if (::strncmp($2.String.p, "forcecase", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::ForceCase);
		}
		else if (::strncmp($2.String.p, "loop", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::Loop);
		}
		else if (::strncmp($2.String.p, "fastopt", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::FastOpt);
		}
		else if (::strncmp($2.String.p, "unroll", $2.String.len) == 0)
		{
			$$.Int = static_cast<int>(ReShade::Nodes::Attribute::Unroll);
		}
		else
		{
			parser.Warning(@1, 3554, "unknown attribute '%.*s'", $2.String.len, $2.String.p);

			$$.Int = 0;
		}
	}
	|
	{
		$$.Int = 0;
	}
	;

 /* Annotations ------------------------------------------------------------------------------ */

RULE_ANNOTATION
	: RULE_TYPE_SCALAR RULE_IDENTIFIER_NAME "=" RULE_EXPRESSION_LITERAL ";"
	{
		if ($1.Type.Rows != AST[$4].As<ReShade::Nodes::Literal>().Type.Rows || $1.Type.Cols != AST[$4].As<ReShade::Nodes::Literal>().Type.Cols)
		{
			parser.Error(@3, 3020, "type mismatch");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Annotation>(@2);
		ReShade::Nodes::Annotation &data = node.As<ReShade::Nodes::Annotation>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Type = $1.Type;

		switch (data.Type.Class)
		{
			case ReShade::Nodes::Type::Bool:
			case ReShade::Nodes::Type::Int:
			{
				switch (AST[$4].As<ReShade::Nodes::Literal>().Type.Class)
				{
					case ReShade::Nodes::Type::Bool:
					case ReShade::Nodes::Type::Int:
					case ReShade::Nodes::Type::Uint:
						data.Value.AsInt = AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt;
						break;
					case ReShade::Nodes::Type::Float:
						data.Value.AsInt = static_cast<int>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsFloat);
						break;
					case ReShade::Nodes::Type::Double:
						data.Value.AsInt = static_cast<int>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsDouble);
						break;
				}
				break;
			}
			case ReShade::Nodes::Type::Uint:
			{
				switch (AST[$4].As<ReShade::Nodes::Literal>().Type.Class)
				{
					case ReShade::Nodes::Type::Bool:
					case ReShade::Nodes::Type::Int:
					case ReShade::Nodes::Type::Uint:
						data.Value.AsUint = AST[$4].As<ReShade::Nodes::Literal>().Value.AsUint;
						break;
					case ReShade::Nodes::Type::Float:
						data.Value.AsUint = static_cast<unsigned int>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsFloat);
						break;
					case ReShade::Nodes::Type::Double:
						data.Value.AsUint = static_cast<unsigned int>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsDouble);
						break;
				}
				break;
			}
			case ReShade::Nodes::Type::Float:
			{
				switch (AST[$4].As<ReShade::Nodes::Literal>().Type.Class)
				{
					case ReShade::Nodes::Type::Bool:
					case ReShade::Nodes::Type::Int:
						data.Value.AsFloat = static_cast<float>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt);
						break;
					case ReShade::Nodes::Type::Uint:
						data.Value.AsFloat = static_cast<float>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsUint);
						break;
					case ReShade::Nodes::Type::Float:
						data.Value.AsFloat = AST[$4].As<ReShade::Nodes::Literal>().Value.AsFloat;
						break;
					case ReShade::Nodes::Type::Double:
						data.Value.AsFloat = static_cast<float>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsDouble);
						break;
				}
				break;
			}
			case ReShade::Nodes::Type::Double:
			{
				switch (AST[$4].As<ReShade::Nodes::Literal>().Type.Class)
				{
					case ReShade::Nodes::Type::Bool:
					case ReShade::Nodes::Type::Int:
						data.Value.AsDouble = static_cast<double>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt);
						break;
					case ReShade::Nodes::Type::Uint:
						data.Value.AsDouble = static_cast<double>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsUint);
						break;
					case ReShade::Nodes::Type::Float:
						data.Value.AsDouble = static_cast<double>(AST[$4].As<ReShade::Nodes::Literal>().Value.AsFloat);
						break;
					case ReShade::Nodes::Type::Double:
						data.Value.AsDouble = AST[$4].As<ReShade::Nodes::Literal>().Value.AsDouble;
						break;
				}
				break;
			}
		}
		
		@$ = @1;
		$$ = node.Index;
	}
	| "string" RULE_IDENTIFIER_NAME "=" TOK_LITERAL_STRING ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Annotation>(@2);
		ReShade::Nodes::Annotation &data = node.As<ReShade::Nodes::Annotation>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Type = $1.Type;
		data.Value.AsString = parser.CreateString($4.String.p, $4.String.len);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_ANNOTATION_LIST
	: RULE_ANNOTATION
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_ANNOTATION_LIST RULE_ANNOTATION
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;

RULE_ANNOTATIONS
	: "<" ">"
	{
		$$ = 0;
	}
	| "<" RULE_ANNOTATION_LIST ">"
	{
		@$ = @1;
		$$ = $2;
	}
	|
	{
		$$ = 0;
	}
	;

 /* Expressions ------------------------------------------------------------------------------ */

RULE_EXPRESSION
	: RULE_EXPRESSION_ASSIGNMENT
	| RULE_EXPRESSION "," RULE_EXPRESSION_ASSIGNMENT
	{
		if ($1 == 0)
		{
			@$ = @3;
			$$ = $3;
		}
		else if (AST[$1].Is<ReShade::Nodes::ExpressionSequence>())
		{
			@$ = @1;
			$$ = $1;
			
			AST[$$].As<ReShade::Nodes::ExpressionSequence>().Link(AST, $3);
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::ExpressionSequence>();
			ReShade::Nodes::ExpressionSequence &data = node.As<ReShade::Nodes::ExpressionSequence>();
			data.Link(AST, $1);
			data.Link(AST, $3);
			
			@$ = @1;
			$$ = node.Index;
		}
	}
	;
RULE_EXPRESSION_LITERAL
	: TOK_LITERAL_BOOL
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Literal>(@1);
		ReShade::Nodes::Literal &data = node.As<ReShade::Nodes::Literal>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Value.AsInt = $1.Int;
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_LITERAL_INT
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Literal>(@1);
		ReShade::Nodes::Literal &data = node.As<ReShade::Nodes::Literal>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Int;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Value.AsInt = $1.Int;
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_LITERAL_FLOAT
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Literal>(@1);
		ReShade::Nodes::Literal &data = node.As<ReShade::Nodes::Literal>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Float;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Value.AsFloat = $1.Float;
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_LITERAL_DOUBLE
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Literal>(@1);
		ReShade::Nodes::Literal &data = node.As<ReShade::Nodes::Literal>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Double;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Value.AsDouble = $1.Double;
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_LITERAL_ENUM
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Literal>(@1);
		ReShade::Nodes::Literal &data = node.As<ReShade::Nodes::Literal>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Int;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Value.AsInt = $1.Int;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_PRIMARY
	: RULE_EXPRESSION_LITERAL
	| RULE_IDENTIFIER_SYMBOL
	{
		if ($1.Node == 0)
		{
			parser.Error(@1, 3004, "undeclared identifier '%.*s'", $1.String.len, $1.String.p);
			YYERROR;
		}
		else if (AST[$1.Node].Is<ReShade::Nodes::Function>())
		{
			parser.Error(@1, 3005, "identifier '%.*s' represents a function, not a variable", $1.String.len, $1.String.p);
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Reference>(@1);
		ReShade::Nodes::Reference &data = node.As<ReShade::Nodes::Reference>();
		data.Type = AST[$1.Node].As<ReShade::Nodes::Variable>().Type;
		data.Symbol = $1.Node;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "(" RULE_EXPRESSION ")"
	{
		@$ = @2;
		$$ = $2;
	}
	;
RULE_EXPRESSION_FUNCTION_PARAMETERS
	: RULE_EXPRESSION_ASSIGNMENT
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_EXPRESSION_FUNCTION_PARAMETERS "," RULE_EXPRESSION_ASSIGNMENT
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $3);
	}
	;
RULE_EXPRESSION_FUNCTION
	: RULE_IDENTIFIER_FUNCTION "(" ")"
	{
		@$ = @1;
		$$ = $1;
	}
	| RULE_IDENTIFIER_FUNCTION "(" RULE_EXPRESSION_FUNCTION_PARAMETERS ")"
	{
		@$ = @1;
		$$ = $1;
		
		if (AST[$$].Is<ReShade::Nodes::Call>())
		{
			AST[$$].As<ReShade::Nodes::Call>().Parameters = $3;
		}
		else if (AST[$$].Is<ReShade::Nodes::Constructor>())
		{
			AST[$$].As<ReShade::Nodes::Constructor>().Parameters = $3;
		}
	}
	;
RULE_EXPRESSION_POSTFIX
	: RULE_EXPRESSION_PRIMARY
	| RULE_EXPRESSION_FUNCTION
	{
		@$ = @1;
		$$ = $1;
		
		if (AST[$$].Is<ReShade::Nodes::Call>())
		{
			ReShade::EffectTree::Node &node = AST[$$];
			ReShade::Nodes::Call &call = node.As<ReShade::Nodes::Call>();
			
			ReShade::EffectTree::Index symbol = parser.GetSymbol(call.CalleeName, ReShade::Nodes::Function::NodeType);
			
			if (symbol == 0)
			{
				parser.Error(@1, 3004, "undeclared identifier '%s'", call.CalleeName);
				YYERROR;
			}
			
			bool ambiguous;
			symbol = parser.GetSymbolOverload($$, ambiguous);

			if (symbol == 0)
			{
				if (ambiguous)
				{
					parser.Error(@1, 3067, "ambiguous function call to '%s'", call.CalleeName);
				}
				else
				{
					parser.Error(@1, 3013, "no matching function overload for '%s'", call.CalleeName);
				}
				YYERROR;
			}

			call.Type = AST[symbol].As<ReShade::Nodes::Function>().ReturnType;
			call.Callee = symbol;
		}
		else if (AST[$$].Is<ReShade::Nodes::Constructor>())
		{
			// TODO
		}
	}
	| RULE_EXPRESSION_POSTFIX RULE_OPERATOR_POSTFIX
	{
		const ReShade::Nodes::Expression expression = AST[$1].As<ReShade::Nodes::Expression>();
		
		if (!expression.Type.IsScalar() && !expression.Type.IsVector() && !expression.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Const) || expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3025, "l-value specifies const object");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::UnaryExpression>(@2);
		ReShade::Nodes::UnaryExpression &data = node.As<ReShade::Nodes::UnaryExpression>();
		data.Type = expression.Type;
		data.Operand = $1;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_EXPRESSION_POSTFIX "[" RULE_EXPRESSION "]"
	{
		const ReShade::Nodes::Expression expression = AST[$1].As<ReShade::Nodes::Expression>();
		
		if (!expression.Type.IsArray() && !expression.Type.IsVector() && !expression.Type.IsMatrix())
		{
			parser.Error(@1, 3121, "array, matrix, vector, or indexable object type expected in index expression");
			YYERROR;
		}
		if (!AST[$3].As<ReShade::Nodes::Expression>().Type.IsScalar())
		{
			parser.Error(@3, 3120, "invalid type for index - index must be a scalar");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = expression.Type;
		data.Operator = ReShade::Nodes::Operator::Index;
		data.Left = $1;
		data.Right = $3;

		if (expression.Type.IsArray())
		{
			data.Type.Elements[--data.Type.ElementsDimension] = 0;
		}
		else if (expression.Type.IsMatrix())
		{
			data.Type.Rows = data.Type.Cols;
			data.Type.Cols = 1;
		}
		else if (expression.Type.IsVector())
		{
			data.Type.Rows = 1;
		}
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_EXPRESSION_POSTFIX "." RULE_EXPRESSION_FUNCTION
	{
		const ReShade::Nodes::Expression expression = AST[$1].As<ReShade::Nodes::Expression>();

		if (!expression.Type.IsStruct() || expression.Type.IsArray())
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
		const ReShade::Nodes::Expression expression = AST[$1].As<ReShade::Nodes::Expression>();

		if (expression.Type.IsArray())
		{
			parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
			YYERROR;
		}
		else if (expression.Type.IsVector())
		{
			int offsets[4] = { -1, -1, -1, -1 };

			if ($3.String.len > 4)
			{
				parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
				YYERROR;
			}

			bool constant = false;
			enum { xyzw, rgba, stpq } set[4];

			for (std::size_t i = 0; i < $3.String.len; ++i)
			{
				switch ($3.String.p[i])
				{
					case 'x': offsets[i] = 0; set[i] = xyzw; break;
					case 'y': offsets[i] = 1; set[i] = xyzw; break;
					case 'z': offsets[i] = 2; set[i] = xyzw; break;
					case 'w': offsets[i] = 3; set[i] = xyzw; break;
					case 'r': offsets[i] = 0; set[i] = rgba; break;
					case 'g': offsets[i] = 1; set[i] = rgba; break;
					case 'b': offsets[i] = 2; set[i] = rgba; break;
					case 'a': offsets[i] = 3; set[i] = rgba; break;
					case 's': offsets[i] = 0; set[i] = stpq; break;
					case 't': offsets[i] = 1; set[i] = stpq; break;
					case 'p': offsets[i] = 2; set[i] = stpq; break;
					case 'q': offsets[i] = 3; set[i] = stpq; break;
					default:
						parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
						YYERROR;
				}

				if ((i > 0 && (set[i] != set[i - 1])) || static_cast<unsigned int>(offsets[i]) >= expression.Type.Rows)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
					YYERROR;
				}
				
				for (std::size_t k = 0; k < i; ++k)
				{
					if (offsets[k] == offsets[i])
					{
						constant = true;
						break;
					}
				}
			}

			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Swizzle>(@2);
			ReShade::Nodes::Swizzle &data = node.As<ReShade::Nodes::Swizzle>();
			data.Type = expression.Type;
			data.Type.Rows = static_cast<unsigned int>($3.String.len);
			data.Left = $1;
			::memcpy(data.Offsets, offsets, 4 * sizeof(int));
			
			if (constant || expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
			{
				data.Type.Qualifiers &= ~static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Uniform);
				data.Type.Qualifiers |=  static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
			}
			
			@$ = @1;
			$$ = node.Index;
		}
		else if (expression.Type.IsMatrix())
		{
			int offsets[4] = { -1, -1, -1, -1 };

			if ($3.String.len < 3)
			{
				parser.Error(@3, 3018, "invalid subscript '%s'", $3.String.p);
				YYERROR;
			}

			bool constant = false;
			const unsigned int type = static_cast<unsigned int>($3.String.p[1] == 'm'), ntype = static_cast<unsigned int>(!type);

			for (std::size_t i = 0, j = 0; i < $3.String.len; i += 3 + type, ++j)
			{
				if ($3.String.p[i] != '_' || $3.String.p[i + type + 1] < '0' + static_cast<char>(ntype) || $3.String.p[i + type + 1] > '3' + static_cast<char>(ntype) || $3.String.p[i + type + 2] < '0' + static_cast<char>(ntype) || $3.String.p[i + type + 2] > '3' + static_cast<char>(ntype) || (type && $3.String.p[i + 1] != 'm'))
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
					YYERROR;
				}

				const unsigned int row = $3.String.p[i + type + 1] - '0' - ntype;
				const unsigned int col = $3.String.p[i + type + 2] - '0' - ntype;

				if (row >= expression.Type.Rows || col >= expression.Type.Cols || j > 3)
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
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

			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Swizzle>(@2);
			ReShade::Nodes::Swizzle &data = node.As<ReShade::Nodes::Swizzle>();
			data.Type = expression.Type;
			data.Type.Rows = static_cast<unsigned int>($3.String.len / (3 + type));
			data.Type.Cols = 1;
			data.Left = $1;
			::memcpy(data.Offsets, offsets, 4 * sizeof(int));
			
			if (constant || expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
			{
				data.Type.Qualifiers &= ~static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Uniform);
				data.Type.Qualifiers |=  static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
			}
			
			@$ = @1;
			$$ = node.Index;
		}
		else if (expression.Type.IsStruct())
		{
			ReShade::EffectTree::Index fields = AST[expression.Type.Definition].As<ReShade::Nodes::Struct>().Fields, field = 0;

			for (std::size_t i = 0, count = AST[fields].As<ReShade::Nodes::Aggregate>().Length; i < count; ++i)
			{
				const ReShade::EffectTree::Node &current = AST[AST[fields].As<ReShade::Nodes::Aggregate>().Find(AST, i)];
				
				if (!current.Is<ReShade::Nodes::Variable>())
				{
					continue;
				}
				
				if (::strncmp(current.As<ReShade::Nodes::Variable>().Name, $3.String.p, $3.String.len) == 0)
				{
					field = current.Index;
					break;
				}
			}

			if (field == 0)
			{
				parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
				YYERROR;
			}
			
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Field>(@2);
			ReShade::Nodes::Field &data = node.As<ReShade::Nodes::Field>();
			data.Type = AST[field].As<ReShade::Nodes::Variable>().Type;
			data.Left = $1;
			data.Callee = field;
			
			if (expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
			{
				data.Type.Qualifiers &= ~static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Uniform);
				data.Type.Qualifiers |=  static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
			}
			
			@$ = @1;
			$$ = node.Index;
		}
		else if (expression.Type.IsScalar())
		{
			int offsets[4] = { -1, -1, -1, -1 };

			for (std::size_t i = 0; i < $3.String.len; ++i)
			{
				if ($3.String.p[i] != 'x' && $3.String.p[i] != 'r' && $3.String.p[i] != 's')
				{
					parser.Error(@3, 3018, "invalid subscript '%.*s'", $3.String.len, $3.String.p);
					YYERROR;
				}

				offsets[i] = 0;
			}

			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Swizzle>(@2);
			ReShade::Nodes::Swizzle &data = node.As<ReShade::Nodes::Swizzle>();
			data.Type = expression.Type;
			data.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
			data.Type.Rows = static_cast<unsigned int>($3.String.len);
			data.Left = $1;
			::memcpy(data.Offsets, offsets, 4 * sizeof(int));
			
			@$ = @1;
			$$ = node.Index;
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
		const ReShade::Nodes::Expression expression = AST[$2].As<ReShade::Nodes::Expression>();

		if (!expression.Type.IsScalar() && !expression.Type.IsVector() && !expression.Type.IsMatrix())
		{
			parser.Error(@2, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Const) || expression.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@2, 3025, "l-value specifies const object");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::UnaryExpression>(@1);
		ReShade::Nodes::UnaryExpression &data = node.As<ReShade::Nodes::UnaryExpression>();
		data.Type = expression.Type;
		data.Operand = $2;
		
		switch ($1.Int)
		{
			case ReShade::Nodes::Operator::PostIncrease:
				data.Operator = ReShade::Nodes::Operator::Increase;
				break;
			case ReShade::Nodes::Operator::PostDecrease:
				data.Operator = ReShade::Nodes::Operator::Decrease;
				break;
		}
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_OPERATOR_UNARY RULE_EXPRESSION_UNARY
	{
		const ReShade::Nodes::Expression expression = AST[$2].As<ReShade::Nodes::Expression>();

		if (!expression.Type.IsScalar() && !expression.Type.IsVector() && !expression.Type.IsMatrix())
		{
			parser.Error(@2, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (static_cast<ReShade::Nodes::Operator>($1.Int) == ReShade::Nodes::Operator::BitwiseNot && !expression.Type.IsIntegral())
		{
			parser.Error(@2, 3082, "int or unsigned int type required");
			YYERROR;
		}

		if (static_cast<ReShade::Nodes::Operator>($1.Int) == ReShade::Nodes::Operator::Plus)
		{
			@$ = @1;
			$$ = $2;
		}
		else if (AST[$2].Is<ReShade::Nodes::Literal>())
		{
			@$ = @1;
			$$ = $2;

			ReShade::Nodes::Literal &data = AST[$$].As<ReShade::Nodes::Literal>();

			switch ($1.Int)
			{
				case ReShade::Nodes::Operator::Minus:
				{
					switch (data.Type.Class)
					{
						case ReShade::Nodes::Type::Int:
						case ReShade::Nodes::Type::Uint:
							data.Value.AsInt = -data.Value.AsInt;
							break;
						case ReShade::Nodes::Type::Float:
							data.Value.AsFloat = -data.Value.AsFloat;
							break;
						case ReShade::Nodes::Type::Double:
							data.Value.AsDouble = -data.Value.AsDouble;
							break;
					}
					break;
				}
				case ReShade::Nodes::Operator::BitwiseNot:
				{
					data.Value.AsUint = ~data.Value.AsUint;
					break;
				}
				case ReShade::Nodes::Operator::LogicalNot:
				{
					data.Value.AsUint = !data.Value.AsUint;
					break;
				}
			}
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::UnaryExpression>(@1);
			ReShade::Nodes::UnaryExpression &data = node.As<ReShade::Nodes::UnaryExpression>();
			data.Type = expression.Type;
			data.Operand = $2;
			data.Operator = static_cast<ReShade::Nodes::Operator>($1.Int);
			
			@$ = @1;
			$$ = node.Index;
		}
	}
	| "(" RULE_TYPE ")" RULE_EXPRESSION_UNARY
	{
		const ReShade::Nodes::Expression expression = AST[$4].As<ReShade::Nodes::Expression>();

		if (expression.Type.Class == $2.Type.Class && (expression.Type.Rows == $2.Type.Rows && expression.Type.Cols == $2.Type.Cols) && !($2.Type.IsArray() || expression.Type.IsArray()))
		{
			@$ = @4;
			$$ = $4;
		}
		else if (AST[$4].Is<ReShade::Nodes::Literal>())
		{
			@$ = @1;
			$$ = $4;
			
			ReShade::Nodes::Literal &data = AST[$$].As<ReShade::Nodes::Literal>();

			switch ($2.Type.Class)
			{
				case ReShade::Nodes::Type::Bool:
				{
					data.Value.AsUint = data.Value.AsUint != 0;
					break;
				}
				case ReShade::Nodes::Type::Int:
				{
					switch (data.Type.Class)
					{
						case ReShade::Nodes::Type::Float:
							data.Value.AsInt = static_cast<int>(data.Value.AsFloat);
							break;
						case ReShade::Nodes::Type::Double:
							data.Value.AsInt = static_cast<int>(data.Value.AsDouble);
							break;
					}
					break;
				}
				case ReShade::Nodes::Type::Uint:
				{
					switch (data.Type.Class)
					{
						case ReShade::Nodes::Type::Float:
							data.Value.AsUint = static_cast<unsigned int>(data.Value.AsFloat);
							break;
						case ReShade::Nodes::Type::Double:
							data.Value.AsUint = static_cast<unsigned int>(data.Value.AsDouble);
							break;
					}
					break;
				}
				case ReShade::Nodes::Type::Float:
				{
					switch (data.Type.Class)
					{
						case ReShade::Nodes::Type::Bool:
						case ReShade::Nodes::Type::Int:
							data.Value.AsFloat = static_cast<float>(data.Value.AsInt);
							break;
						case ReShade::Nodes::Type::Uint:
							data.Value.AsFloat = static_cast<float>(data.Value.AsUint);
							break;
						case ReShade::Nodes::Type::Double:
							data.Value.AsFloat = static_cast<float>(data.Value.AsDouble);
							break;
					}
					break;
				}
				case ReShade::Nodes::Type::Double:
				{
					switch (data.Type.Class)
					{
						case ReShade::Nodes::Type::Bool:
						case ReShade::Nodes::Type::Int:
							data.Value.AsDouble = static_cast<double>(data.Value.AsInt);
							break;
						case ReShade::Nodes::Type::Uint:
							data.Value.AsDouble = static_cast<double>(data.Value.AsUint);
							break;
						case ReShade::Nodes::Type::Float:
							data.Value.AsDouble = static_cast<double>(data.Value.AsFloat);
							break;
					}
					break;
				}
			}
			
			data.Type = $2.Type;
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::UnaryExpression>(@1);
			ReShade::Nodes::UnaryExpression &data = node.As<ReShade::Nodes::UnaryExpression>();
			data.Type = $2.Type;
			data.Operand = $4;
			data.Operator = ReShade::Nodes::Operator::Cast;
			
			@$ = @1;
			$$ = node.Index;
		}
	}
	;
RULE_EXPRESSION_MULTIPLICATIVE
	: RULE_EXPRESSION_UNARY
	| RULE_EXPRESSION_MULTIPLICATIVE RULE_OPERATOR_MULTIPLICATIVE RULE_EXPRESSION_UNARY
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if ((left.Type.IsMatrix() != right.Type.IsMatrix()) && (left.Type.Rows * left.Type.Cols != right.Type.Rows * right.Type.Cols && !(left.Type.IsScalar() || right.Type.IsScalar())))
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}
		
		if (static_cast<ReShade::Nodes::Operator>($2.Int) == ReShade::Nodes::Operator::Divide && (AST[$3].Is<ReShade::Nodes::Literal>() && AST[$3].As<ReShade::Nodes::Literal>().Type.IsNumeric() && AST[$3].As<ReShade::Nodes::Literal>().Value.AsUint == 0))
		{
			parser.Warning(@3, 4008, "division by zero");
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = right.Type.IsFloatingPoint() ? right.Type.Class : left.Type.Class;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_ADDITIVE
	: RULE_EXPRESSION_MULTIPLICATIVE
	| RULE_EXPRESSION_ADDITIVE RULE_OPERATOR_ADDITIVE RULE_EXPRESSION_MULTIPLICATIVE
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if ((left.Type.IsMatrix() != right.Type.IsMatrix()) && (left.Type.Rows * left.Type.Cols != right.Type.Rows * right.Type.Cols && !(left.Type.IsScalar() || right.Type.IsScalar())))
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = right.Type.IsFloatingPoint() ? right.Type.Class : left.Type.Class;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_SHIFT
	: RULE_EXPRESSION_ADDITIVE
	| RULE_EXPRESSION_SHIFT RULE_OPERATOR_SHIFT RULE_EXPRESSION_ADDITIVE
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();
		
		if (!left.Type.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!right.Type.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = left.Type;
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_RELATIONAL
	: RULE_EXPRESSION_SHIFT
	| RULE_EXPRESSION_RELATIONAL RULE_OPERATOR_RELATIONAL RULE_EXPRESSION_SHIFT
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();
		
		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_EQUALITY
	: RULE_EXPRESSION_RELATIONAL
	| RULE_EXPRESSION_EQUALITY RULE_OPERATOR_EQUALITY RULE_EXPRESSION_RELATIONAL
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if ((left.Type.Definition != right.Type.Definition) || left.Type.IsArray() || right.Type.IsArray())
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_BITAND
	: RULE_EXPRESSION_EQUALITY
	| RULE_EXPRESSION_BITAND "&" RULE_EXPRESSION_EQUALITY
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!right.Type.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = left.Type;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::BitwiseAnd;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_BITXOR
	: RULE_EXPRESSION_BITAND
	| RULE_EXPRESSION_BITXOR "^" RULE_EXPRESSION_BITAND
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!right.Type.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = left.Type;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::BitwiseXor;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_BITOR
	: RULE_EXPRESSION_BITXOR
	| RULE_EXPRESSION_BITOR "|" RULE_EXPRESSION_BITXOR
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsIntegral())
		{
			parser.Error(@1, 3082, "int or unsigned int type required");
			YYERROR;
		}
		if (!right.Type.IsIntegral())
		{
			parser.Error(@3, 3082, "int or unsigned int type required");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = left.Type;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::BitwiseOr;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGAND
	: RULE_EXPRESSION_BITOR
	| RULE_EXPRESSION_LOGAND "&&" RULE_EXPRESSION_BITOR
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::LogicalAnd;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGXOR
	: RULE_EXPRESSION_LOGAND
	| RULE_EXPRESSION_LOGXOR "^^" RULE_EXPRESSION_LOGAND
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::LogicalXor;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_LOGOR
	: RULE_EXPRESSION_LOGXOR
	| RULE_EXPRESSION_LOGOR "||" RULE_EXPRESSION_LOGXOR
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();

		if (!left.Type.IsScalar() && !left.Type.IsVector() && !left.Type.IsMatrix())
		{
			parser.Error(@1, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}
		if (!right.Type.IsScalar() && !right.Type.IsVector() && !right.Type.IsMatrix())
		{
			parser.Error(@3, 3022, "scalar, vector, or matrix expected");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::BinaryExpression>(@2);
		ReShade::Nodes::BinaryExpression &data = node.As<ReShade::Nodes::BinaryExpression>();
		data.Type = ReShade::Nodes::Type::Undefined;
		data.Type.Class = ReShade::Nodes::Type::Bool;
		data.Type.Rows = std::max(left.Type.Rows, right.Type.Rows);
		data.Type.Cols = std::max(left.Type.Cols, right.Type.Cols);
		data.Left = $1;
		data.Right = $3;
		data.Operator = ReShade::Nodes::Operator::LogicalOr;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_CONDITIONAL
	: RULE_EXPRESSION_LOGOR
	| RULE_EXPRESSION_LOGOR "?" RULE_EXPRESSION ":" RULE_EXPRESSION_ASSIGNMENT
	{
		const ReShade::Nodes::Expression condition = AST[$1].As<ReShade::Nodes::Expression>();

		if (!condition.Type.IsScalar() && !condition.Type.IsVector())
		{
			parser.Error(@1, 3022, "boolean or vector expression expected");
			YYERROR;
		}
		
		if (AST[$1].Is<ReShade::Nodes::Literal>() && AST[$1].As<ReShade::Nodes::Literal>().Type.IsScalar())
		{
			@$ = (AST[$1].As<ReShade::Nodes::Literal>().Value.AsInt) ? @3 : @5;
			$$ = (AST[$1].As<ReShade::Nodes::Literal>().Value.AsInt) ? $3 : $5;
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Conditional>(@2);
			ReShade::Nodes::Conditional &data = node.As<ReShade::Nodes::Conditional>();
			data.Type = AST[$5].As<ReShade::Nodes::Expression>().Type;
			data.Condition = $1;
			data.ExpressionTrue = $3;
			data.ExpressionFalse = $5;
			
			@$ = @1;
			$$ = node.Index;
		}
	}
	;
RULE_EXPRESSION_ASSIGNMENT
	: RULE_EXPRESSION_CONDITIONAL
	| RULE_EXPRESSION_UNARY RULE_OPERATOR_ASSIGNMENT RULE_EXPRESSION_ASSIGNMENT
	{
		const ReShade::Nodes::Expression left = AST[$1].As<ReShade::Nodes::Expression>(), right = AST[$3].As<ReShade::Nodes::Expression>();
		
		if (left.Type.IsArray())
		{
			parser.Error(@1, 3017, "cannot assign to an array");
			YYERROR;
		}
		if (right.Type.IsArray())
		{
			parser.Error(@3, 3017, "cannot assign an array");
			YYERROR;
		}
		if (left.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Const) || left.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3025, "l-value specifies const object");
			YYERROR;
		}
		
		if (right.Type.Rows > left.Type.Rows || right.Type.Cols > left.Type.Cols)
		{
			parser.Warning(@3, 3206, "implicit truncation of vector type");
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Assignment>(@2);
		ReShade::Nodes::Assignment &data = node.As<ReShade::Nodes::Assignment>();
		data.Type = left.Type;
		data.Left = $1;
		data.Right = $3;
		data.Operator = static_cast<ReShade::Nodes::Operator>($2.Int);
		
		@$ = @1;
		$$ = node.Index;
	}
	;

RULE_EXPRESSION_INITIALIZER
	: RULE_EXPRESSION_ASSIGNMENT
	{
		@$ = @1;
		$$ = $1;
	}
	| "{" "}"
	{
		@$ = @1;
		$$ = 0;
	}
	| "{" RULE_EXPRESSION_INITIALIZER_LIST "}"
	{
		@$ = @1;
		$$ = $2;
	}
	| "{" RULE_EXPRESSION_INITIALIZER_LIST "," "}"
	{
		@$ = @1;
		$$ = $2;
	}
	;
RULE_EXPRESSION_INITIALIZER_LIST
	: RULE_EXPRESSION_INITIALIZER
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_EXPRESSION_INITIALIZER_LIST "," RULE_EXPRESSION_INITIALIZER
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $3);
	}
	;

RULE_EXPRESSION_INITIALIZEROBJECT
	: "{" "}"
	{
		@$ = @1;
		$$ = 0;
	}
	| "{" RULE_EXPRESSION_INITIALIZEROBJECTSTATE_LIST "}"
	{
		@$ = @1;
		$$ = $2;
	}
	;
RULE_EXPRESSION_INITIALIZEROBJECTSTATE
	: TOK_IDENTIFIER_STATE "=" RULE_EXPRESSION_LITERAL ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::State>(@1);
		ReShade::Nodes::State &data = node.As<ReShade::Nodes::State>();
		data.Type = $1.Int;

		switch (AST[$3].As<ReShade::Nodes::Literal>().Type.Class)
		{
			case ReShade::Nodes::Type::Bool:
			case ReShade::Nodes::Type::Int:
			case ReShade::Nodes::Type::Uint:
				data.Value.AsInt = AST[$3].As<ReShade::Nodes::Literal>().Value.AsInt;
				break;
			case ReShade::Nodes::Type::Float:
				data.Value.AsFloat = AST[$3].As<ReShade::Nodes::Literal>().Value.AsFloat;
				break;
			case ReShade::Nodes::Type::Double:
				data.Value.AsFloat = static_cast<float>(AST[$3].As<ReShade::Nodes::Literal>().Value.AsDouble);
				break;
		}
		
		@$ = @1;
		$$ = node.Index;
	}
	| TOK_IDENTIFIER_STATE "=" RULE_IDENTIFIER_NAME ";"
	{	
		const char *name = parser.CreateString($3.String.p, $3.String.len);
		
		ReShade::EffectTree::Index symbol = parser.GetSymbol(name, ReShade::Nodes::Variable::NodeType);

		if (symbol == 0)
		{
			parser.Error(@3, 3004, "undeclared identifier '%s'", name);
			YYERROR;
		}
		if ($1.Int != ReShade::Nodes::State::Texture || !AST[symbol].As<ReShade::Nodes::Variable>().Type.IsTexture() || AST[symbol].As<ReShade::Nodes::Variable>().Type.IsArray())
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::State>(@1);
		ReShade::Nodes::State &data = node.As<ReShade::Nodes::State>();
		data.Type = $1.Int;
		data.Value.AsNode = symbol;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_EXPRESSION_INITIALIZEROBJECTSTATE_LIST
	: RULE_EXPRESSION_INITIALIZEROBJECTSTATE
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_EXPRESSION_INITIALIZEROBJECTSTATE_LIST RULE_EXPRESSION_INITIALIZEROBJECTSTATE
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;

 /* Statements ------------------------------------------------------------------------------- */

RULE_STATEMENT_LIST
	: RULE_STATEMENT
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::StatementBlock>();
		ReShade::Nodes::StatementBlock &data = node.As<ReShade::Nodes::StatementBlock>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_STATEMENT_LIST RULE_STATEMENT
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::StatementBlock>().Link(AST, $2);
	}
	;
RULE_STATEMENT_COMPOUND
	: "{" "}"
	{
		@$ = @1;
		$$ = AST.Add<ReShade::Nodes::StatementBlock>().Index;
	}
	| "{" { parser.PushScope(); } RULE_STATEMENT_LIST { parser.PopScope(); } "}"
	{
		@$ = @1;
		$$ = $3;

		AST[$$].Location = @$;
	}
	;
RULE_STATEMENT_COMPOUND_SCOPELESS
	: "{" "}"
	{
		@$ = @1;
		$$ = AST.Add<ReShade::Nodes::StatementBlock>().Index;
	}
	| "{" RULE_STATEMENT_LIST "}"
	{
		@$ = @1;
		$$ = $2;

		AST[$$].Location = @$;
	}
	;

RULE_STATEMENT
	: RULE_STATEMENT_COMPOUND
	| RULE_STATEMENT_SIMPLE
	;
RULE_STATEMENT_SCOPELESS
	: RULE_STATEMENT_COMPOUND_SCOPELESS
	| RULE_STATEMENT_SIMPLE
	;
RULE_STATEMENT_SIMPLE
	: RULE_STATEMENT_DECLARATION
	| RULE_STATEMENT_EXPRESSION
	| RULE_STATEMENT_SELECTION
	| RULE_STATEMENT_ITERATION
	| RULE_STATEMENT_JUMP
	| RULE_STATEMENT_TYPEDEF
	{
		@$ = @1;
		$$ = 0;
	}
	| error ";"
	{
		$$ = 0;
	}
	;

RULE_STATEMENT_DECLARATION
	: RULE_DECLARATION
	{
		const ReShade::EffectTree::Location location = @1;

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::DeclarationStatement>(location);
		ReShade::Nodes::DeclarationStatement &data = node.As<ReShade::Nodes::DeclarationStatement>();
		data.Declaration = $1;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_STATEMENT_EXPRESSION
	: ";"
	{
		$$ = 0;
	}
	| RULE_EXPRESSION ";"
	{
		const ReShade::EffectTree::Location location = @1;
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::ExpressionStatement>(location);
		ReShade::Nodes::ExpressionStatement &data = node.As<ReShade::Nodes::ExpressionStatement>();
		data.Expression = $1;
		
		@$ = @1;
		$$ = node.Index;
	}
	;

RULE_STATEMENT_SELECTION
	: RULE_ATTRIBUTE "if" "(" RULE_EXPRESSION ")" RULE_STATEMENT "else" RULE_STATEMENT
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Branch && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Flatten))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: branch, flatten");

			$1.Int = 0;
		}
		if (!AST[$4].As<ReShade::Nodes::Expression>().Type.IsScalar())
		{
			parser.Error(@4, 3019, "if statement conditional expressions must evaluate to a scalar");
			YYERROR;
		}

		if (AST[$4].Is<ReShade::Nodes::Literal>())
		{
			@$ = (AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt) ? @6 : @8;
			$$ = (AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt) ? $6 : $8;
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Selection>(@2);
			ReShade::Nodes::Selection &data = node.As<ReShade::Nodes::Selection>();
			data.Mode = ReShade::Nodes::Selection::If;
			data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
			data.Condition = $4;
			data.StatementTrue = $6;
			data.StatementFalse = $8;
			
			@$ = @2;
			$$ = node.Index;
		}
	}
	| RULE_ATTRIBUTE "if" "(" RULE_EXPRESSION ")" RULE_STATEMENT %prec TOK_THEN
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Branch && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Flatten))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: branch, flatten");

			$1.Int = 0;
		}
		if (!AST[$4].As<ReShade::Nodes::Expression>().Type.IsScalar())
		{
			parser.Error(@4, 3019, "if statement conditional expressions must evaluate to a scalar");
			YYERROR;
		}

		if (AST[$4].Is<ReShade::Nodes::Literal>())
		{
			@$ = @6;
			$$ = (AST[$4].As<ReShade::Nodes::Literal>().Value.AsInt) ? $6 : 0;
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Selection>(@2);
			ReShade::Nodes::Selection &data = node.As<ReShade::Nodes::Selection>();
			data.Mode = ReShade::Nodes::Selection::If;
			data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
			data.Condition = $4;
			data.StatementTrue = $6;
			
			@$ = @2;
			$$ = node.Index;
		}
	} 
	| RULE_ATTRIBUTE "switch" "(" RULE_EXPRESSION ")" "{" "}"
	{
		parser.Warning(@2, 5000, "switch statement contains no 'case' or 'default' labels");
		
		@$ = @2;
		$$ = 0;
	}
	| RULE_ATTRIBUTE "switch" "(" RULE_EXPRESSION ")" "{" RULE_STATEMENT_CASE_LIST "}"
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Branch && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Flatten && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::ForceCase))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: branch, flatten, forcecase");

			$1.Int = 0;
		}
		if (!AST[$4].As<ReShade::Nodes::Expression>().Type.IsScalar())
		{
			parser.Error(@4, 3019, "switch statement expression must evaluate to a scalar");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Selection>(@2);
		ReShade::Nodes::Selection &data = node.As<ReShade::Nodes::Selection>();
		data.Mode = ReShade::Nodes::Selection::Switch;
		data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
		data.Condition = $4;
		data.Statement = $7;
		
		@$ = @2;
		$$ = node.Index;
	}
	;
RULE_STATEMENT_CASE
	: RULE_STATEMENT_CASELABEL_LIST RULE_STATEMENT
	{
		ReShade::EffectTree::Node &nodeStatementBlock = AST.Add<ReShade::Nodes::StatementBlock>();
		ReShade::Nodes::StatementBlock &dataStatementBlock = nodeStatementBlock.As<ReShade::Nodes::StatementBlock>();
		dataStatementBlock.Link(AST, $2);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Selection>();
		ReShade::Nodes::Selection &data = node.As<ReShade::Nodes::Selection>();
		data.Mode = ReShade::Nodes::Selection::Case;
		data.Condition = $1;
		data.Statement = nodeStatementBlock.Index;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_STATEMENT_CASE RULE_STATEMENT
	{
		@$ = @1;
		$$ = $1;

		AST[AST[$$].As<ReShade::Nodes::Selection>().Statement].As<ReShade::Nodes::StatementBlock>().Link(AST, $2);
	}
	;
RULE_STATEMENT_CASE_LIST
	: RULE_STATEMENT_CASE
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_STATEMENT_CASE_LIST RULE_STATEMENT_CASE
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;
RULE_STATEMENT_CASELABEL
	: "case" RULE_EXPRESSION ":"
	{
		if (!AST[$2].Is<ReShade::Nodes::Literal>() || !AST[$2].As<ReShade::Nodes::Expression>().Type.IsNumeric())
		{
			parser.Error(@2, 3020, "non-numeric case expression");
			YYERROR;
		}

		@$ = @1;
		$$ = $2;

		AST[$$].Location = @$;
	}
	| "default" ":"
	{
		@$ = @1;
		$$ = AST.Add<ReShade::Nodes::Expression>(@1).Index;
	}
	;
RULE_STATEMENT_CASELABEL_LIST
	: RULE_STATEMENT_CASELABEL
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_STATEMENT_CASELABEL_LIST RULE_STATEMENT_CASELABEL
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;

RULE_STATEMENT_ITERATION
	: RULE_ATTRIBUTE "while" "(" { parser.PushScope(); } RULE_EXPRESSION ")" RULE_STATEMENT_SCOPELESS
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Loop && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::FastOpt && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Unroll))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: loop, fastopt, unroll");

			$1.Int = 0;
		}

		parser.PopScope();
		
		if (AST[$5].Is<ReShade::Nodes::Literal>() && !AST[$5].As<ReShade::Nodes::Literal>().Value.AsInt)
		{
			$$ = 0;
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Iteration>(@2);
			ReShade::Nodes::Iteration &data = node.As<ReShade::Nodes::Iteration>();
			data.Mode = ReShade::Nodes::Iteration::While;
			data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
			data.Condition = $5;
			data.Statement = $7;
			
			@$ = @2;
			$$ = node.Index;
		}
	}
	| RULE_ATTRIBUTE "do" RULE_STATEMENT "while" "(" RULE_EXPRESSION ")" ";"
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Loop && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::FastOpt && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Unroll))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: loop, fastopt, unroll");

			$1.Int = 0;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Iteration>(@2);
		ReShade::Nodes::Iteration &data = node.As<ReShade::Nodes::Iteration>();
		data.Mode = ReShade::Nodes::Iteration::DoWhile;
		data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
		data.Condition = $6;
		data.Statement = $3;
		
		@$ = @2;
		$$ = node.Index;
	}
	| RULE_ATTRIBUTE "for" "(" { parser.PushScope(); } RULE_STATEMENT_FOR_INITIALIZER RULE_STATEMENT_FOR_CONDITION ";" RULE_STATEMENT_FOR_ITERATOR ")" RULE_STATEMENT_SCOPELESS
	{
		if ($1.Int != 0 && (static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Loop && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::FastOpt && static_cast<ReShade::Nodes::Attribute>($1.Int) != ReShade::Nodes::Attribute::Unroll))
		{
			parser.Warning(@1, 3554, "attribute invalid for this statement, valid attributes are: loop, fastopt, unroll");

			$1.Int = 0;
		}
		parser.PopScope();
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Iteration>(@2);
		ReShade::Nodes::Iteration &data = node.As<ReShade::Nodes::Iteration>();
		data.Mode = ReShade::Nodes::Iteration::For;
		data.Attributes = static_cast<ReShade::Nodes::Attribute>($1.Int);
		data.Initialization = $5;
		data.Condition = $6;
		data.Expression = $8;
		data.Statement = $10;
		
		@$ = @2;
		$$ = node.Index;
	}
	;
RULE_STATEMENT_FOR_INITIALIZER
	: RULE_STATEMENT_SIMPLE
	;
RULE_STATEMENT_FOR_CONDITION
	: RULE_EXPRESSION_CONDITIONAL
	|
	{
		$$ = 0;
	}
	;
RULE_STATEMENT_FOR_ITERATOR
	: RULE_EXPRESSION
	|
	{
		$$ = 0;
	}
	;

RULE_STATEMENT_JUMP
	: "continue" ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Jump>(@1);
		ReShade::Nodes::Jump &data = node.As<ReShade::Nodes::Jump>();
		data.Mode = ReShade::Nodes::Jump::Continue;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "break" ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Jump>(@1);
		ReShade::Nodes::Jump &data = node.As<ReShade::Nodes::Jump>();
		data.Mode = ReShade::Nodes::Jump::Break;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "return" ";"
	{
		ReShade::EffectTree::Index function;
		parser.GetScope(function);

		const ReShade::Nodes::Type &type = AST[function].As<ReShade::Nodes::Function>().ReturnType;

		if (type.Class != ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3080, "function must return a value");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Jump>(@1);
		ReShade::Nodes::Jump &data = node.As<ReShade::Nodes::Jump>();
		data.Mode = ReShade::Nodes::Jump::Return;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "return" RULE_EXPRESSION ";"
	{
		ReShade::EffectTree::Index function;
		parser.GetScope(function);

		const ReShade::Nodes::Expression expression = AST[$2].As<ReShade::Nodes::Expression>();
		const ReShade::Nodes::Type &type = AST[function].As<ReShade::Nodes::Function>().ReturnType;

		if (type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3079, "void functions cannot return a value");
			YYERROR;
		}
		if ((type.IsMatrix() != expression.Type.IsMatrix()) && (type.Rows * type.Cols != expression.Type.Rows * expression.Type.Cols))
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Jump>(@1);
		ReShade::Nodes::Jump &data = node.As<ReShade::Nodes::Jump>();
		data.Mode = ReShade::Nodes::Jump::Return;
		data.Expression = $2;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "discard" ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Jump>(@1);
		ReShade::Nodes::Jump &data = node.As<ReShade::Nodes::Jump>();
		data.Mode = ReShade::Nodes::Jump::Discard;
		
		@$ = @1;
		$$ = node.Index;
	}
	;

RULE_STATEMENT_TYPEDEF
	: "typedef" RULE_TYPE TOK_IDENTIFIER ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($3.String.p, $3.String.len);
		data.Type = $2.Type;
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "typedef" "const" RULE_TYPE TOK_IDENTIFIER ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($4.String.p, $4.String.len);
		data.Type = $3.Type;
		data.Type.Qualifiers = static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "typedef" RULE_TYPE "const" TOK_IDENTIFIER ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($4.String.p, $4.String.len);
		data.Type = $2.Type;
		data.Type.Qualifiers = static_cast<int>(ReShade::Nodes::Type::Qualifier::Const);
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "typedef" RULE_TYPE TOK_IDENTIFIER RULE_TYPE_ARRAY ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($3.String.p, $3.String.len);
		data.Type = $2.Type;
		data.Type.ElementsDimension = $4.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $4.Type.Elements, sizeof(data.Type.Elements));
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "typedef" "const" RULE_TYPE TOK_IDENTIFIER RULE_TYPE_ARRAY ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($4.String.p, $4.String.len);
		data.Type = $3.Type;
		data.Type.Qualifiers = static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Type.ElementsDimension = $5.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $5.Type.Elements, sizeof(data.Type.Elements));
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "typedef" RULE_TYPE "const" TOK_IDENTIFIER RULE_TYPE_ARRAY ";"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Typedef>(@1);
		ReShade::Nodes::Typedef &data = node.As<ReShade::Nodes::Typedef>();
		data.Name = parser.CreateString($4.String.p, $4.String.len);
		data.Type = $2.Type;
		data.Type.Qualifiers = static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		data.Type.ElementsDimension = $5.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $5.Type.Elements, sizeof(data.Type.Elements));
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	;

 /* Declarations ----------------------------------------------------------------------------- */

RULE_DECLARATION
	: RULE_DECLARATION_VARIABLE ";"
	{
		@$ = @1;
		$$ = $1;
	}
	| RULE_DECLARATION_FUNCTIONPROTOTYPE ";"
	{
		@$ = @1;
		$$ = $1;
	}
	;
	
 /* Structures ------------------------------------------------------------------------------ */

RULE_DECLARATION_STRUCT
	: "struct" "{" "}"
	{
		parser.Warning(@1, 5000, "struct has no members");

		@$ = @1;
		$$ = AST.Add<ReShade::Nodes::Struct>(@1).Index;
	}
	| "struct" "{"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Struct>(@1);

		parser.PushScope(node.Index);
	}
	  RULE_DECLARATION_STRUCTFIELD_LIST "}"
	{
		ReShade::EffectTree::Index index;
		parser.GetScope(index);

		parser.PopScope();

		ReShade::Nodes::Struct &data = AST[index].As<ReShade::Nodes::Struct>();
		data.Fields = $4;
		
		@$ = @1;
		$$ = index;
	}
	| "struct" RULE_IDENTIFIER_NAME "{" "}"
	{
		const char *name = parser.CreateString($2.String.p, $2.String.len);

		if ($2.Node != 0 && parser.GetSymbol(name, ReShade::Nodes::Struct::NodeType) != 0)
		{
			parser.Error(@2, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		
		parser.Warning(@2, 5000, "struct '%s' has no members", name);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Struct>(@2);
		ReShade::Nodes::Struct &data = node.As<ReShade::Nodes::Struct>();
		data.Name = name;

		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	| "struct" RULE_IDENTIFIER_NAME "{"
	{
		const char *name = parser.CreateString($2.String.p, $2.String.len);
		
		if ($2.Node != 0 && parser.GetSymbol(name, ReShade::Nodes::Struct::NodeType) != 0)
		{
			parser.Error(@2, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Struct>(@2);
		ReShade::Nodes::Struct &data = node.As<ReShade::Nodes::Struct>();
		data.Name = name;
		
		parser.PushScope(node.Index);
	}
	  RULE_DECLARATION_STRUCTFIELD_LIST "}"
	{
		ReShade::EffectTree::Index index;
		parser.GetScope(index);

		parser.PopScope();
		
		ReShade::Nodes::Struct &data = AST[index].As<ReShade::Nodes::Struct>();
		data.Fields = $5;

		parser.CreateSymbol(index);
		
		@$ = @1;
		$$ = index;
	}
	;
RULE_DECLARATION_STRUCTFIELD
	: RULE_TYPE_FULLYSPECIFIED RULE_DECLARATION_STRUCTFIELDDECLARATOR_LIST ";"
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "struct members cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "struct members cannot be declared 'inline'");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::In) || $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			parser.Error(@1, 3055, "struct members cannot be declared 'in' or 'out'");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST[$2];
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		
		for (std::size_t i = 0; i < data.Length; ++i)
		{
			ReShade::Nodes::Variable &variable = AST[data.Find(AST, i)].As<ReShade::Nodes::Variable>();
			
			variable.Type.Class = $1.Type.Class;
			variable.Type.Qualifiers = $1.Type.Qualifiers;
			variable.Type.Rows = $1.Type.Rows;
			variable.Type.Cols = $1.Type.Cols;
			variable.Type.Definition = $1.Type.Definition;
		}

		@$ = @1;
		$$ = $2;
	}
	;
RULE_DECLARATION_STRUCTFIELDDECLARATOR
	: RULE_IDENTIFIER_NAME
	{
		ReShade::EffectTree::Index structure;
		parser.GetScope(structure);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Name = parser.CreateString($1.String.p, $1.String.len);
		data.Parent = structure;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_IDENTIFIER_SEMANTIC
	{
		ReShade::EffectTree::Index structure;
		parser.GetScope(structure);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Name = parser.CreateString($1.String.p, $1.String.len);
		data.Semantic = parser.CreateString($2.String.p, $2.String.len);
		data.Parent = structure;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY
	{
		ReShade::EffectTree::Index structure;
		parser.GetScope(structure);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type.ElementsDimension = $2.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $2.Type.Elements, sizeof(data.Type.Elements));
		data.Name = parser.CreateString($1.String.p, $1.String.len);
		data.Parent = structure;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_IDENTIFIER_SEMANTIC
	{
		ReShade::EffectTree::Index structure;
		parser.GetScope(structure);

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type.ElementsDimension = $2.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $2.Type.Elements, sizeof(data.Type.ElementsDimension));
		data.Name = parser.CreateString($1.String.p, $1.String.len);
		data.Semantic = parser.CreateString($3.String.p, $3.String.len);
		data.Parent = structure;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
	
RULE_DECLARATION_STRUCTFIELD_LIST
	: RULE_DECLARATION_STRUCTFIELD
	{
		@$ = @1;
		$$ = $1;
	}
	| RULE_DECLARATION_STRUCTFIELD_LIST RULE_DECLARATION_STRUCTFIELD
	{
		@$ = @1;
		$$ = $1;

		ReShade::Nodes::Aggregate &data1 = AST[$$].As<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data2 = AST[$2].As<ReShade::Nodes::Aggregate>();
		
		for (std::size_t i = 0; i < data2.Length; ++i)
		{
			data1.Link(AST, data2.Find(AST, i));
		}
	}
	;
RULE_DECLARATION_STRUCTFIELDDECLARATOR_LIST
	: RULE_DECLARATION_STRUCTFIELDDECLARATOR
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_DECLARATION_STRUCTFIELDDECLARATOR_LIST "," RULE_DECLARATION_STRUCTFIELDDECLARATOR
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $3);
	}
	;

 /* Variables -------------------------------------------------------------------------------- */

RULE_DECLARATION_VARIABLE
	: RULE_TYPE_FULLYSPECIFIED RULE_DECLARATION_VARIABLEDECLARATOR_LIST
	{
		ReShade::EffectTree::Index parent;
		parser.GetScope(parent);
	
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "variables cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "variables cannot be declared 'inline'");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::In) || $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			parser.Error(@1, 3055, "variables cannot be declared 'in' or 'out'");
			YYERROR;
		}
		
		if (parent != 0)
		{
			if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
			{
				parser.Error(@1, 3047, "local variables cannot be declared 'uniform'");
				YYERROR;
			}
			if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Extern))
			{
				parser.Error(@1, 3006, "local variables cannot be declared 'extern'");
				YYERROR;
			}
		}
		else
		{
			if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Static))
			{
				if (!$1.Type.IsTexture() && !$1.Type.IsSampler())
				{
					parser.Warning(@1, 3002, "global variables are considered 'uniform' by default");
				}

				$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Extern) | static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Uniform);
			}
		}
		
		ReShade::EffectTree::Node &node = AST[$2];
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		
		for (std::size_t i = 0; i < data.Length; ++i)
		{
			ReShade::EffectTree::Node &variableNode = AST[data.Find(AST, i)];
			ReShade::Nodes::Variable &variable = variableNode.As<ReShade::Nodes::Variable>();
			
			if (variable.Initializer != 0)
			{
				const ReShade::EffectTree::Node &initializer = AST[variable.Initializer];
				
				if (initializer.Is<ReShade::Nodes::Aggregate>())
				{
					const ReShade::Nodes::Aggregate &initializerList = initializer.As<ReShade::Nodes::Aggregate>();
					
					if (AST[initializerList.Find(AST, 0)].Is<ReShade::Nodes::State>() != ($1.Type.IsTexture() || $1.Type.IsSampler()))
					{
						parser.Error(initializer.Location, 3000, "invalid initializer for this type");
						YYERROR;
					}
					if (($1.Type.IsTexture() || $1.Type.IsSampler()) && parent != 0)
					{
						parser.Error(initializer.Location, 3000, "Textures and samplers can only be created in the global scope. If a local variable is desired, use the assignment initializer instead.");
						YYERROR;
					}
				}
				else
				{
					const ReShade::Nodes::Expression &initializerExpression = initializer.As<ReShade::Nodes::Expression>();
					
					if ((initializerExpression.Type.IsSampler() != $1.Type.IsSampler()) || (initializerExpression.Type.IsTexture() != $1.Type.IsTexture()))
					{
						parser.Error(initializer.Location, 3020, "type mismatch");
						YYERROR;
					}
					if ((initializerExpression.Type.Rows < $1.Type.Rows || initializerExpression.Type.Cols < $1.Type.Cols) && !initializerExpression.Type.IsScalar())
					{
						parser.Error(initializer.Location, 3017, "cannot implicitly convert these vector types");
						YYERROR;
					}
					if (initializerExpression.Type.Rows > $1.Type.Rows || initializerExpression.Type.Cols > $1.Type.Cols)
					{
						parser.Warning(initializer.Location, 3206, "implicit truncation of vector type");
					}
				}

				if (parent == 0 && $1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Const) && !$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Static))
				{
					parser.Warning(variableNode.Location, 3207, "Initializer used on a global 'const' variable. This requires setting an external constant. If a literal is desired, use 'static const' instead.");
				}
			}
			else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Const))
			{
				parser.Error(variableNode.Location, 3012, "missing initial value for '%s'", variable.Name);
				YYERROR;
			}

			variable.Type.Class = $1.Type.Class;
			variable.Type.Qualifiers = $1.Type.Qualifiers;
			variable.Type.Rows = $1.Type.Rows;
			variable.Type.Cols = $1.Type.Cols;
			variable.Type.Definition = $1.Type.Definition;
						
			parser.CreateSymbol(variableNode.Index);
		}
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_DECLARATION_VARIABLEDECLARATOR
	: RULE_IDENTIFIER_NAME RULE_ANNOTATIONS
	{
		const char *name = parser.CreateString($1.String.p, $1.String.len);
	
		if ($1.Node != 0 && parser.GetSymbol(name, 0, parser.GetScope(), true) != 0)
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Name = name;
		data.Annotations = $2;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "=" RULE_EXPRESSION_INITIALIZER
	{
		const char *name = parser.CreateString($1.String.p, $1.String.len);

		if ($1.Node != 0 && parser.GetSymbol(name, 0, parser.GetScope(), true) != 0)
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Name = name;
		data.Annotations = $2;
		data.Initializer = $4;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_ANNOTATIONS RULE_EXPRESSION_INITIALIZEROBJECT
	{
		const char *name = parser.CreateString($1.String.p, $1.String.len);

		if ($1.Node != 0 && parser.GetSymbol(name, 0, parser.GetScope(), true) != 0)
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}

		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Name = name;
		data.Annotations = $2;
		data.Initializer = $3;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_ANNOTATIONS
	{
		const char *name = parser.CreateString($1.String.p, $1.String.len);

		if ($1.Node != 0 && parser.GetSymbol(name, 0, parser.GetScope(), true) != 0)
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type.ElementsDimension = $2.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $2.Type.Elements, sizeof(data.Type.Elements));
		data.Name = name;
		data.Annotations = $3;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_ANNOTATIONS "=" RULE_EXPRESSION_INITIALIZER
	{
		const char *name = parser.CreateString($1.String.p, $1.String.len);

		if ($1.Node != 0 && parser.GetSymbol(name, 0, parser.GetScope(), true) != 0)
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		if ($5 != 0 && !AST[$5].Is<ReShade::Nodes::Aggregate>())
		{
			parser.Error(@1, 3017, "type mismatch");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type.ElementsDimension = $2.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $2.Type.Elements, sizeof(data.Type.Elements));
		data.Name = name;
		data.Annotations = $3;
		data.Initializer = $5;
		
		@$ = @1;
		$$ = node.Index;
	}
	;

RULE_DECLARATION_VARIABLEDECLARATOR_LIST
	: RULE_DECLARATION_VARIABLEDECLARATOR
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_DECLARATION_VARIABLEDECLARATOR_LIST "," RULE_DECLARATION_VARIABLEDECLARATOR
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $3);
	}
	;
	
 /* Functions -------------------------------------------------------------------------------- */

RULE_DECLARATION_FUNCTIONPROTOTYPE
	: RULE_DECLARATION_FUNCTIONDECLARATOR
	{
		@$ = @1;
		$$ = $1;
		
		parser.CreateSymbol($$);
	}
	| RULE_DECLARATION_FUNCTIONDECLARATOR RULE_IDENTIFIER_SEMANTIC
	{
		ReShade::EffectTree::Node &node = AST[$1];
		ReShade::Nodes::Function &data = node.As<ReShade::Nodes::Function>();

		if (data.ReturnType.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@2, 3076, "void function cannot have a semantic");
			YYERROR;
		}
		
		data.ReturnSemantic = parser.CreateString($2.String.p, $2.String.len);
		
		parser.CreateSymbol(node.Index);
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_DECLARATION_FUNCTIONDEFINITION
	: RULE_DECLARATION_FUNCTIONPROTOTYPE
	{
		const ReShade::EffectTree::Node &symbol = AST[parser.GetSymbol(parser.CreateSymbolLookupName($1), ReShade::Nodes::Function::NodeType)];

		if (symbol.As<ReShade::Nodes::Function>().HasDefinition())
		{
			parser.Error(@1, 3003, "redefinition of '%s'", symbol.As<ReShade::Nodes::Function>().Name);
			YYERROR;
		}
		
		parser.PushScope($1);
		
		if (symbol.As<ReShade::Nodes::Function>().HasArguments())
		{
			const ReShade::Nodes::Aggregate &arguments = AST[AST[$1].As<ReShade::Nodes::Function>().Arguments].As<ReShade::Nodes::Aggregate>();
			
			for (std::size_t i = 0; i < arguments.Length; ++i)
			{
				parser.CreateSymbol(arguments.Find(AST, i));
			}
		}
	}
	  RULE_STATEMENT_COMPOUND
	{
		parser.PopScope();

		ReShade::EffectTree::Node &node = AST[$1];
		ReShade::Nodes::Function &data = node.As<ReShade::Nodes::Function>();
		data.Definition = $3;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
 
RULE_DECLARATION_FUNCTIONDECLARATOR
	: RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_SYMBOL "(" ")"
	{
		const char *name = parser.CreateString($2.String.p, $2.String.len);
		ReShade::EffectTree::Index symbol = parser.GetSymbol(name, 0, parser.GetScope(), true);

		if ($2.Node != 0 && symbol != 0 && !AST[symbol].Is<ReShade::Nodes::Function>())
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3047, "functions cannot be declared 'uniform'");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Function>(@2);
		ReShade::Nodes::Function &data = node.As<ReShade::Nodes::Function>();
		data.Name = name;
		data.ReturnType = $1.Type;
		data.ReturnType.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_SYMBOL "("
	{
		const char *name = parser.CreateString($2.String.p, $2.String.len);
		ReShade::EffectTree::Index symbol = parser.GetSymbol(name, 0, parser.GetScope(), true);

		if ($2.Node != 0 && symbol != 0 && !AST[symbol].Is<ReShade::Nodes::Function>())
		{
			parser.Error(@1, 3003, "redefinition of '%s'", name);
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3047, "functions cannot be declared 'uniform'");
			YYERROR;
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Function>(@2);
		ReShade::Nodes::Function &data = node.As<ReShade::Nodes::Function>();
		data.Name = name;
		data.ReturnType = $1.Type;
		data.ReturnType.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::Const);
		
		parser.PushScope(node.Index);
	}
	  RULE_DECLARATION_FUNCTIONARGUMENT_LIST ")"
	{
		ReShade::EffectTree::Index index;
		parser.GetScope(index);

		parser.PopScope();
		
		ReShade::Nodes::Function &data = AST[index].As<ReShade::Nodes::Function>();
		data.Arguments = $5;
		
		@$ = @1;
		$$ = index;
	}
	;
RULE_DECLARATION_FUNCTIONARGUMENT
	: RULE_TYPE_FULLYSPECIFIED
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_NAME
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_NAME RULE_IDENTIFIER_SEMANTIC
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Semantic = parser.CreateString($3.String.p, $3.String.len);
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_NAME "=" RULE_EXPRESSION_INITIALIZER
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Initializer = $4;
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Type.ElementsDimension = $3.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $3.Type.Elements, sizeof(data.Type.Elements));
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_TYPE_FULLYSPECIFIED RULE_IDENTIFIER_NAME RULE_TYPE_ARRAY RULE_IDENTIFIER_SEMANTIC
	{
		if ($1.Type.Class == ReShade::Nodes::Type::Void)
		{
			parser.Error(@1, 3038, "function parameters cannot be void");
			YYERROR;
		}
		if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Inline))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'inline'");
			YYERROR;
		}
		else if ($1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Uniform))
		{
			parser.Error(@1, 3055, "function parameters cannot be declared 'uniform', consider placing in global scope instead");
			YYERROR;
		}

		if (!$1.Type.HasQualifier(ReShade::Nodes::Type::Qualifier::Out))
		{
			$1.Type.Qualifiers |= static_cast<unsigned int>(ReShade::Nodes::Type::Qualifier::In);
		}
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Variable>(@1);
		ReShade::Nodes::Variable &data = node.As<ReShade::Nodes::Variable>();
		data.Type = $1.Type;
		data.Type.ElementsDimension = $3.Type.ElementsDimension;
		::memcpy(data.Type.Elements, $3.Type.Elements, sizeof(data.Type.Elements));
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Semantic = parser.CreateString($4.String.p, $4.String.len);
		data.Argument = true;
		
		@$ = @1;
		$$ = node.Index;
	}
	;

RULE_DECLARATION_FUNCTIONARGUMENT_LIST
	: RULE_DECLARATION_FUNCTIONARGUMENT
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_DECLARATION_FUNCTIONARGUMENT_LIST "," RULE_DECLARATION_FUNCTIONARGUMENT
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $3);
	}
	;

 /* Techniques ------------------------------------------------------------------------------- */

RULE_DECLARATION_TECHNIQUE
	: "technique" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" "}"
	{
		parser.Warning(@2, 5000, "technique '%.*s' has no passes", $2.String.len, $2.String.p);
		
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Technique>(@1);
		ReShade::Nodes::Technique &data = node.As<ReShade::Nodes::Technique>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Annotations = $3;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "technique" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" RULE_DECLARATION_PASS_LIST "}"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Technique>(@1);
		ReShade::Nodes::Technique &data = node.As<ReShade::Nodes::Technique>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Annotations = $3;
		data.Passes = $5;
		
		@$ = @1;
		$$ = node.Index;
	}
	;

 /* Passes ----------------------------------------------------------------------------------- */

RULE_DECLARATION_PASS
	: "pass" RULE_ANNOTATIONS "{" "}"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Pass>(@1);
		ReShade::Nodes::Pass &data = node.As<ReShade::Nodes::Pass>();
		data.Annotations = $2;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "pass" RULE_ANNOTATIONS "{" RULE_DECLARATION_PASSSTATE_LIST "}"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Pass>(@1);
		ReShade::Nodes::Pass &data = node.As<ReShade::Nodes::Pass>();
		data.Annotations = $2;
		data.States = $4;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" "}"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Pass>(@1);
		ReShade::Nodes::Pass &data = node.As<ReShade::Nodes::Pass>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Annotations = $3;
		
		@$ = @1;
		$$ = node.Index;
	}
	| "pass" RULE_IDENTIFIER_NAME RULE_ANNOTATIONS "{" RULE_DECLARATION_PASSSTATE_LIST "}"
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Pass>(@1);
		ReShade::Nodes::Pass &data = node.As<ReShade::Nodes::Pass>();
		data.Name = parser.CreateString($2.String.p, $2.String.len);
		data.Annotations = $3;
		data.States = $5;
		
		@$ = @1;
		$$ = node.Index;
	}
	;
RULE_DECLARATION_PASSSTATE
	: TOK_IDENTIFIER_STATE "=" RULE_EXPRESSION_LITERAL ";"
	{
		const bool stateShaderAssignment = $1.Int == ReShade::Nodes::State::VertexShader || $1.Int == ReShade::Nodes::State::PixelShader;
		const bool stateRenderTargetAssignment = $1.Int >= ReShade::Nodes::State::RenderTarget0 && $1.Int <= ReShade::Nodes::State::RenderTarget7;

		if (stateShaderAssignment || stateRenderTargetAssignment)
		{
			if (AST[$3].As<ReShade::Nodes::Literal>().Type.IsIntegral() && AST[$3].As<ReShade::Nodes::Literal>().Value.AsInt == 0)
			{
				@$ = @1;
				$$ = 0;
			}
			else
			{
				parser.Error(@2, 3020, "type mismatch");
				YYERROR;
			}
		}
		else
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::State>(@1);
			ReShade::Nodes::State &data = node.As<ReShade::Nodes::State>();
			data.Type = $1.Int;

			switch (AST[$3].As<ReShade::Nodes::Literal>().Type.Class)
			{
				case ReShade::Nodes::Type::Bool:
				case ReShade::Nodes::Type::Int:
				case ReShade::Nodes::Type::Uint:
					data.Value.AsInt = AST[$3].As<ReShade::Nodes::Literal>().Value.AsInt;
					break;
				case ReShade::Nodes::Type::Float:
					data.Value.AsFloat = AST[$3].As<ReShade::Nodes::Literal>().Value.AsFloat;
					break;
				case ReShade::Nodes::Type::Double:
					data.Value.AsFloat = static_cast<float>(AST[$3].As<ReShade::Nodes::Literal>().Value.AsDouble);
					break;
			}
		
			@$ = @1;
			$$ = node.Index;
		}
	}
	| TOK_IDENTIFIER_STATE "=" RULE_IDENTIFIER_NAME ";"
	{
		const char *name = parser.CreateString($3.String.p, $3.String.len);
		const bool stateShaderAssignment = $1.Int == ReShade::Nodes::State::VertexShader || $1.Int == ReShade::Nodes::State::PixelShader;
		const bool stateRenderTargetAssignment = $1.Int >= ReShade::Nodes::State::RenderTarget0 && $1.Int <= ReShade::Nodes::State::RenderTarget7;
		
		ReShade::EffectTree::Index symbol = parser.GetSymbol(name, 0);

		if (symbol == 0)
		{
			if (stateShaderAssignment)
			{
				parser.Error(@3, 3501, "entrypoint '%s' not found", name);
			}
			else
			{
				parser.Error(@3, 3004, "undeclared identifier '%s'", name);
			}
			YYERROR;
		}
		else if (stateShaderAssignment && AST[symbol].Is<ReShade::Nodes::Function>())
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::State>(@1);
			ReShade::Nodes::State &data = node.As<ReShade::Nodes::State>();
			data.Type = $1.Int;
			data.Value.AsNode = symbol;
			const ReShade::Nodes::Function &function = AST[symbol].As<ReShade::Nodes::Function>();

			if (function.ReturnType.IsStruct())
			{
				const auto &structure = AST[function.ReturnType.Definition].As<ReShade::Nodes::Struct>();

				if (structure.Fields != 0)
				{
					const auto &fields = AST[structure.Fields].As<ReShade::Nodes::Aggregate>();

					for (std::size_t i = 0; i < fields.Length; ++i)
					{
						if (!AST[fields.Find(AST, i)].As<ReShade::Nodes::Variable>().HasSemantic())
						{
							parser.Error(@3, 3503, "function return value missing semantics");
							YYERROR;
						}
					}
				}
			}
			else if (function.ReturnType.Class != ReShade::Nodes::Type::Void && function.ReturnSemantic == nullptr)
			{
				parser.Error(@3, 3503, "function return value missing semantics");
				YYERROR;
			}

			if (function.HasArguments())
			{
				const auto &arguments = AST[function.Arguments].As<ReShade::Nodes::Aggregate>();

				for (std::size_t i = 0; i < arguments.Length; ++i)
				{
					const auto &argument = AST[arguments.Find(AST, i)].As<ReShade::Nodes::Variable>();

					if (argument.Type.IsStruct())
					{
						const auto &structure = AST[argument.Type.Definition].As<ReShade::Nodes::Struct>();

						if (structure.Fields != 0)
						{
							const auto &fields = AST[structure.Fields].As<ReShade::Nodes::Aggregate>();

							for (std::size_t i = 0; i < fields.Length; ++i)
							{
								if (!AST[fields.Find(AST, i)].As<ReShade::Nodes::Variable>().HasSemantic())
								{
									parser.Error(@3, 3502, "function parameter missing semantics");
									YYERROR;
								}
							}
						}
					}
					else if (!argument.HasSemantic())
					{
						parser.Error(@3, 3502, "function parameter missing semantics");
						YYERROR;
					}
				}
			}
			
			@$ = @1;
			$$ = node.Index;
		}
		else if (stateRenderTargetAssignment && AST[symbol].Is<ReShade::Nodes::Variable>() && AST[symbol].As<ReShade::Nodes::Variable>().Type.IsTexture() && !AST[symbol].As<ReShade::Nodes::Variable>().Type.IsArray())
		{
			ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::State>(@1);
			ReShade::Nodes::State &data = node.As<ReShade::Nodes::State>();
			data.Type = $1.Int;
			data.Value.AsNode = symbol;
		
			@$ = @1;
			$$ = node.Index;
		}
		else
		{
			parser.Error(@2, 3020, "type mismatch");
			YYERROR;
		}
	}
	;
	
RULE_DECLARATION_PASS_LIST
	: RULE_DECLARATION_PASS
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_DECLARATION_PASS_LIST RULE_DECLARATION_PASS
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;
RULE_DECLARATION_PASSSTATE_LIST
	: RULE_DECLARATION_PASSSTATE
	{
		ReShade::EffectTree::Node &node = AST.Add<ReShade::Nodes::Aggregate>();
		ReShade::Nodes::Aggregate &data = node.As<ReShade::Nodes::Aggregate>();
		data.Link(AST, $1);
		
		@$ = @1;
		$$ = node.Index;
	}
	| RULE_DECLARATION_PASSSTATE_LIST RULE_DECLARATION_PASSSTATE
	{
		@$ = @1;
		$$ = $1;

		AST[$$].As<ReShade::Nodes::Aggregate>().Link(AST, $2);
	}
	;

%%

#ifdef _MSC_VER
	#include <stdarg.h>

	static inline int											cvsnprintf(char *str, std::size_t size, const char *format, va_list ap)
	{
		int count = -1;

		if (size != 0)
			count = _vsnprintf_s(str, size, _TRUNCATE, format, ap);
		if (count == -1)
			count = _vscprintf(format, ap);

		return count;
	}
	static inline int											csnprintf(char *str, std::size_t size, const char *format, ...)
	{
		va_list ap;
		va_start(ap, format);
		int count = cvsnprintf(str, size, format, ap);
		va_end(ap);

		return count;
	}

	#define vsnprintf											cvsnprintf
	#define snprintf											csnprintf
#endif

namespace ReShade
{
	EffectTree::EffectTree(void)
	{
		// Padding so root node is at index 1
		this->mNodes.resize(Root);

		Add<ReShade::Nodes::Aggregate>();
	}
	EffectTree::EffectTree(const unsigned char *data, std::size_t size) : mNodes(data, data + size)
	{
		assert(size != 0);
	}

	// -----------------------------------------------------------------------------------------------------

	EffectParser::EffectParser(void) : mLexer(nullptr), mNextLexerState(0), mLexerStateAnnotation(0), mParser(nullptr), mErrors(), mScope(0)
	{
		if (yylex_init(&this->mLexer) != 0)
		{
			return;
		}
	}
	EffectParser::~EffectParser(void)
	{
		if (this->mLexer != nullptr)
		{
			yylex_destroy(this->mLexer);
		}
	}
	
	EffectParser::Scope											EffectParser::GetScope(void) const
	{
		return this->mScope;
	}
	EffectParser::Scope											EffectParser::GetScope(EffectTree::Index &node)
	{
		node = this->mScopeNodes.empty() ? 0 : this->mScopeNodes.top();

		return GetScope();
	}
	EffectTree::Index 											EffectParser::GetSymbol(const std::string &name, int type) const
	{
		if (name[0] == '?')
		{
			auto it = this->mSymbols.find(name);
				
			if (it != this->mSymbols.end())
			{
				return it->second;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return GetSymbol(name, type, GetScope(), false);
		}
	}
	EffectTree::Index 											EffectParser::GetSymbol(const std::string &name, int type, Scope scope, bool exclusive) const
	{
		assert(name[0] != '?');
			
		auto it = this->mLookup.find(name);
			
		if (it == this->mLookup.end() || it->second.empty())
		{
			return 0;
		}

		EffectTree::Index result = 0;
			
		for (auto i = it->second.rbegin(), end = it->second.rend(); i != end; ++i)
		{
			if (i->first > scope)
			{
				continue;
			}
			if (exclusive && i->first < scope)
			{
				continue;
			}
				
			const EffectTree::Node &symbol = (*this->mParserTree)[this->mSymbols.at(i->second)];
				
			if (type == 0)
			{
				if (symbol.Is<Nodes::Variable>() || symbol.Is<Nodes::Struct>() || symbol.Is<Nodes::Typedef>())
				{
					return symbol.Index;
				}
				else
				{
					result = symbol.Index;
					continue;
				}
			}
			else if (symbol.Type == type)
			{
				return symbol.Index;
			}
		}
			
		return result;
	}
	EffectTree::Index 											EffectParser::GetSymbolOverload(EffectTree::Index call, bool &ambiguous) const
	{
		assert(call != 0);
		assert((*this->mParserTree)[call].Is<Nodes::Call>());

		const Nodes::Call &callNode = (*this->mParserTree)[call].As<Nodes::Call>();
		const char *name = callNode.CalleeName;
		std::vector<EffectTree::Index> overloads;
		std::size_t parameters = callNode.Parameters != 0 ? (*this->mParserTree)[callNode.Parameters].As<Nodes::Aggregate>().Length : 0;
				
		auto it = this->mLookup.find(name);
		
		if (it == this->mLookup.end() || it->second.empty())
		{
			return 0;
		}
							
		for (auto i = it->second.rbegin(), end = it->second.rend(); i != end; ++i)
		{
			if (i->first > this->mScope)
			{
				continue;
			}
				
			const EffectTree::Node &symbol = (*this->mParserTree)[this->mSymbols.at(i->second)];
				
			if (!symbol.Is<Nodes::Function>())
			{
				continue;
			}
				
			std::size_t arguments = symbol.As<Nodes::Function>().Arguments != 0 ? (*this->mParserTree)[symbol.As<Nodes::Function>().Arguments].As<Nodes::Aggregate>().Length : 0;
				
			if (arguments == parameters)
			{
				overloads.push_back(symbol.Index);
			}
		}
				
		for (std::size_t i = 0; i < parameters; ++i)
		{	
			const Nodes::Type::Compatibility compatibilities[] =
			{
				Nodes::Type::Compatibility::Match,
				Nodes::Type::Compatibility::Promotion,
				Nodes::Type::Compatibility::Implicit,
				Nodes::Type::Compatibility::ImplicitWithPromotion,
				Nodes::Type::Compatibility::UpwardVectorPromotion
			};
	
			const Nodes::Type &type0 = (*this->mParserTree)[(*this->mParserTree)[callNode.Parameters].As<Nodes::Aggregate>().Find(*this->mParserTree, i)].As<Nodes::Expression>().Type;
	
			for (std::size_t test = 0; test < 5; ++test)
			{
				bool compatMatch = false;
				const Nodes::Type::Compatibility compat = compatibilities[test];

				for (auto it = overloads.begin(), end = overloads.end(); it != end; ++it)
				{
					const Nodes::Type &type1 = (*this->mParserTree)[(*this->mParserTree)[(*this->mParserTree)[*it].As<Nodes::Function>().Arguments].As<Nodes::Aggregate>().Find(*this->mParserTree, i)].As<Nodes::Variable>().Type;
			
					// Check to see if the compatibility matches the compatibility type for this test
					if (Nodes::Type::Compatible(type0, type1) == compat)
					{
						compatMatch = true;
						break;
					}
				}
		
				if (compatMatch)
				{
					// Remove all that don't match this compatibility test
					for (auto it = overloads.begin(); it != overloads.end();)
					{
						const Nodes::Type &type1 = (*this->mParserTree)[(*this->mParserTree)[(*this->mParserTree)[*it].As<Nodes::Function>().Arguments].As<Nodes::Aggregate>().Find(*this->mParserTree, i)].As<Nodes::Variable>().Type;
				
						if (Nodes::Type::Compatible(type0, type1) != compat)
						{
							it = overloads.erase(it);
						}
						else
						{
							++it;
						}
					}
				}
			}
		}
			
		if (overloads.size() == 1)
		{
			ambiguous = false;

			return overloads.front();
		}
		else
		{
			ambiguous = !overloads.empty();

			return 0;
		}
	}

	bool 														EffectParser::Parse(const std::string &source, EffectTree &ast, std::string &errors, bool append)
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
		this->mFailed = false;
		this->mParserTree = &ast;
		this->mAppendToRoot = append;

		bool res = yyparse(this->mLexer, *this) == 0 && !this->mFailed;

		errors += this->mErrors;

		return res;
	}
	
	void														EffectParser::Error(const EffectTree::Location &location, unsigned int code, const char *message, ...)
	{
		assert(message != nullptr && std::strlen(message) != 0);

		const std::size_t capacity = 512;
		char buffer[capacity + 2];
		std::size_t size = 0;
	
		size += snprintf(buffer + size, capacity - size, "%s", (location.Source != nullptr) ? location.Source : "");
		size = std::min(size, capacity);
		size += snprintf(buffer + size, capacity - size, "(%u, %u): error", location.Line, location.Column);
		size = std::min(size, capacity);
		size += snprintf(buffer + size, capacity - size, (code != 0) ? " X%u: " : ": ", code);
		size = std::min(size, capacity);

		va_list args;
		va_start(args, message);
		size += vsnprintf(buffer + size, capacity - size, message, args);
		size = std::min(size, capacity);
		va_end(args);

		buffer[size++] = '\n';
		
		this->mErrors.append(buffer, size);
		this->mFailed = true;
	}
	void														EffectParser::Warning(const EffectTree::Location &location, unsigned int code, const char *message, ...)
	{
		assert(message != nullptr && std::strlen(message) != 0);

		const std::size_t capacity = 512;
		char buffer[capacity + 2];
		std::size_t size = 0;
	
		size += snprintf(buffer + size, capacity - size, "%s", (location.Source != nullptr) ? location.Source : "");
		size = std::min(size, capacity);
		size += snprintf(buffer + size, capacity - size, "(%u, %u): warning", location.Line, location.Column);
		size = std::min(size, capacity);
		size += snprintf(buffer + size, capacity - size, (code != 0) ? " X%u: " : ": ", code);
		size = std::min(size, capacity);

		va_list args;
		va_start(args, message);
		size += vsnprintf(buffer + size, capacity - size, message, args);
		size = std::min(size, capacity);
		va_end(args);

		buffer[size++] = '\n';

		this->mErrors.append(buffer, size);
	}

	void														EffectParser::PushScope(EffectTree::Index node)
	{
		if (node != 0 || this->mScopeNodes.empty())
		{
			this->mScopeNodes.push(node);
		}
		else
		{
			this->mScopeNodes.push(this->mScopeNodes.top());
		}

		this->mScope++;
	}
	void 														EffectParser::PopScope(void)
	{
		for (auto it = this->mLookup.begin(), end = this->mLookup.end(); it != end; ++it)
		{
			if (it->second.empty())
			{
				continue;
			}
				
			for (auto i = it->second.begin(); i != it->second.end();)
			{
				if (i->first >= this->mScope)
				{
					this->mSymbols.erase(this->mSymbols.find(i->second));
					i = it->second.erase(i);
				}
				else
				{
					++i;
				}
			}
		}
			
		this->mScope--;
		this->mScopeNodes.pop();
	}

	bool														EffectParser::CreateSymbol(EffectTree::Index index)
	{
		assert(index != 0);

		const char *name;
		EffectTree::Node &node = (*this->mParserTree)[index];
			
		switch (node.Type)
		{
			case Nodes::Function::NodeType:
				name = node.As<Nodes::Function>().Name;
				break;
			case Nodes::Variable::NodeType:
				name = node.As<Nodes::Variable>().Name;
				break;
			case Nodes::Struct::NodeType:
				name = node.As<Nodes::Struct>().Name;
				break;
			case Nodes::Typedef::NodeType:
				name = node.As<Nodes::Typedef>().Name;
				break;
			default:
				return false;
		}
			
		if (name == nullptr)
		{
			return false;
		}

		std::string mangled = CreateSymbolLookupName(index);
		
		EffectTree::Index check = GetSymbol(mangled.c_str(), node.Type);
			
		if (check != 0)
		{
			return check == index;
		}
			
		this->mLookup[name].push_back(std::make_pair(this->mScope, mangled));
		this->mSymbols[mangled] = index;
			
		return true;
	}
	std::string													EffectParser::CreateSymbolLookupName(EffectTree::Index index)
	{
		assert(index != 0);

		const EffectTree::Node &node = (*this->mParserTree)[index];

		const std::size_t capacity = 512;
		char buffer[capacity];
		std::size_t size = 0;
		::memset(buffer, 0, capacity);
			
		const char *name;
		bool qualified = false;
			
		switch (node.Type)
		{
			case Nodes::Function::NodeType: name = node.As<Nodes::Function>().Name; break;
			case Nodes::Variable::NodeType: name = node.As<Nodes::Variable>().Name; break;
			case Nodes::Struct::NodeType: name = node.As<Nodes::Struct>().Name; break;
			case Nodes::Typedef::NodeType: name = node.As<Nodes::Typedef>().Name; break;
			default: return "";
		}
			
		// 1. Basic Name
		size += snprintf(buffer + size, capacity - size, "?%s@", name);
			
		// 3. Terminator
		buffer[size++] = '@';
			
		// 4. Type Code
		Nodes::Type type;
			
		switch (node.Type)
		{
			case Nodes::Function::NodeType: buffer[size++] = qualified ? 'Q' : 'Y'; type = node.As<Nodes::Function>().ReturnType; buffer[size++] = 'K'; break;
			case Nodes::Variable::NodeType: buffer[size++] = qualified ? '2' : '3'; type = node.As<Nodes::Variable>().Type; break;
			case Nodes::Struct::NodeType: buffer[size++] = 'U'; buffer[size++] = '\0'; return buffer;
			case Nodes::Typedef::NodeType: buffer[size++] = 'T'; buffer[size++] = '\0'; return buffer;
		}
						
		switch (type.Class)
		{
			case Nodes::Type::Class::Void:
				buffer[size++] = 'X';
				break;
			case Nodes::Type::Class::Bool:
				buffer[size++] = '_';
				buffer[size++] = 'N';
				break;
			case Nodes::Type::Class::Int:
				buffer[size++] = 'H';
				break;
			case Nodes::Type::Class::Uint:
				buffer[size++] = 'I';
				break;
			case Nodes::Type::Class::Half:
				buffer[size++] = '_';
				buffer[size++] = 'M';
				break;
			case Nodes::Type::Class::Float:
				buffer[size++] = 'M';
				break;
			case Nodes::Type::Class::Double:
				buffer[size++] = 'N';
				break;
			case Nodes::Type::Class::Struct:
				buffer[size++] = 'U';
				size += snprintf(buffer + size, capacity - size, "?%s@", (*this->mParserTree)[type.Definition].As<Nodes::Struct>().Name);
				buffer[size++] = '@';
				break;
		}
			
		buffer[size++] = static_cast<char>('0' + type.Rows);
		buffer[size++] = static_cast<char>('0' + type.Cols);
			
		// 5. Arguments
		if (node.Is<Nodes::Function>())
		{
			if (node.As<Nodes::Function>().HasArguments())
			{
				const Nodes::Aggregate &arguments = (*this->mParserTree)[node.As<Nodes::Function>().Arguments].As<Nodes::Aggregate>();

				for (std::size_t i = 0; i < arguments.Length; ++i)
				{
					const Nodes::Variable &argument = (*this->mParserTree)[arguments.Find(*this->mParserTree, i)].As<Nodes::Variable>();
						
					if (argument.Type.IsArray())
					{
						buffer[size++] = '_';
						buffer[size++] = 'O';
					}
						
					switch (argument.Type.Class)
					{
						case Nodes::Type::Class::Void:
							buffer[size++] = 'X';
							break;
						case Nodes::Type::Class::Bool:
							buffer[size++] = '_';
							buffer[size++] = 'N';
							break;
						case Nodes::Type::Class::Int:
							buffer[size++] = 'H';
							break;
						case Nodes::Type::Class::Uint:
							buffer[size++] = 'I';
							break;
						case Nodes::Type::Class::Half:
							buffer[size++] = '_';
							buffer[size++] = 'M';
							break;
						case Nodes::Type::Class::Float:
							buffer[size++] = 'M';
							break;
						case Nodes::Type::Class::Double:
							buffer[size++] = 'N';
							break;
						case Nodes::Type::Class::Struct:
							buffer[size++] = 'U';
							size += snprintf(buffer + size, capacity - size, "?%s@", (*this->mParserTree)[argument.Type.Definition].As<Nodes::Struct>().Name);
							buffer[size++] = '@';
							break;
					}
						
					buffer[size++] = static_cast<char>('0' + argument.Type.Rows);
					buffer[size++] = static_cast<char>('0' + argument.Type.Cols);
				}
					
				buffer[size++] = '@';
			}
			else
			{
				buffer[size++] = 'X';
			}
		}
			
		// 6. Storage Class
		switch (node.Type)
		{
			case Nodes::Function::NodeType: buffer[size++] = 'Z'; break;
			case Nodes::Variable::NodeType: buffer[size++] = type.HasQualifier(Nodes::Type::Qualifier::Const) && type.HasQualifier(Nodes::Type::Qualifier::Volatile) ? 'D' : type.HasQualifier(Nodes::Type::Qualifier::Volatile) ? 'C' : type.HasQualifier(Nodes::Type::Qualifier::Const) ? 'B' : 'A'; break;
		}

		buffer[size] = '\0';
			
		return buffer;
	}
	const char *												EffectParser::CreateString(const char *s)
	{
		return s != nullptr ? CreateString(s, std::strlen(s)) : nullptr;
	}
	const char *												EffectParser::CreateString(const char *s, std::size_t length)
	{
		return this->mStringPool.emplace(s, length).first->data();
	}

	namespace Nodes
	{
		Type::Compatibility										Type::Compatible(const Type &left, const Type &right)
		{
			if (left.IsArray() != right.IsArray())
			{
				return Compatibility::None;
			}
			else if ((left.IsArray() && right.IsArray()) && left.ElementsDimension != right.ElementsDimension)
			{
				return Compatibility::None;
			}

			if (left.IsNumeric() && right.IsNumeric() && !left.IsArray())
			{
				if (left.IsMatrix() != right.IsMatrix())
				{
					return Compatibility::None;
				}

				if (left.Rows < right.Rows)
				{
					if (left.IsVector() && right.IsVector())
					{
						return Compatibility::UpwardVectorPromotion;
					}
					else
					{
						return Compatibility::None;
					}
				}

				if (left.Class == right.Class)
				{
					if (left.Rows == right.Rows && left.Cols == right.Cols)
					{
						return Compatibility::Match;
					}
					else if (left.Rows >= right.Rows && left.Cols >= right.Cols)
					{
						return Compatibility::Promotion;
					}
					else
					{
						return Compatibility::None;
					}
				}
				else
				{
					if (left.Rows == right.Rows && left.Cols == right.Cols)
					{
						return Compatibility::Implicit;
					}
					else if (left.Rows >= right.Rows && left.Cols >= right.Cols)
					{
						return Compatibility::ImplicitWithPromotion;
					}
					else
					{
						return Compatibility::None;
					}
				}
			}
			else if (left.Class == right.Class && left.Class != Type::Struct)
			{
				return Compatibility::Match;
			}
			else
			{
				return Compatibility::None;
			}
		}
		
		const Type												Type::Undefined = { Type::Class::Void, 0, 1, 1, 0 };
	}
}