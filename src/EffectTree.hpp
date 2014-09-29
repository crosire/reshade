#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <typeinfo>

namespace ReShade
{
	struct														EffectTree
	{
		public:
			typedef std::size_t 								Index;
			struct 												Location
			{
				const char *									Source;
				unsigned int									Line, Column;
			};
			struct 												Node
			{
				public:
					template <typename T> inline bool			Is(void) const
					{
						return typeid(T).hash_code() == this->mT;
					}
					template <typename T> inline T &			As(void)
					{
						static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

						return static_cast<T &>(*this);
					}
					template <typename T> inline const T &		As(void) const
					{
						static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

						return static_cast<const T &>(*this);
					}

					template <typename V> void					Accept(V &visitor);
					template <typename V> void					Accept(V &visitor) const;

					Index										Index;
					Location									Location;

				protected:
					std::size_t									mT;
			};
			template <class T, class BASE = Node> class			NodeImplementation : public BASE
			{
				static_assert(std::is_base_of<Node, BASE>::value, "invalid effect tree node base type");

				public:
					inline NodeImplementation(void)
					{
						this->mT = typeid(T).hash_code();
					}

					template <typename V> inline void			Accept(V &visitor)
					{
						visitor.Visit(static_cast<T &>(*this));
					}
					template <typename V> inline void			Accept(V &visitor) const
					{
						visitor.Visit(static_cast<const T &>(*this));
					}
			};

			static const Index									Null = 0, Root = 1;
				
		public:
			EffectTree(void) : mNodes(Root) // Padding so root node is at index 1
			{
			}
			
			inline Node &										operator [](Index index)
			{
				return Get(index);
			}
			inline const Node &									operator [](Index index) const
			{
				return Get(index);
			}
			
			template <typename T> inline T &					Add(void)
			{
				const Location location = { nullptr, 0, 0 };
				return Add<T>(location);
			}
			template <typename T> T &							Add(const Location &location) // Any pointers to nodes are no longer valid after this call
			{
				static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

				const Index index = this->mNodes.size();

				this->mNodes.resize(index + sizeof(T), 0);

				Node &node = *new (&Get(index)) T;
				node.Index = index;
				node.Location = location;
					
				return static_cast<T &>(node);
			}
			inline const char *									AddString(const char *string, std::size_t length)
			{
				return (*this->mStrings.emplace(string, length).first).c_str();
			}
			inline Node &										Get(Index index = Root)
			{
				return *reinterpret_cast<Node *>(&this->mNodes[index]);
			}
			inline const Node &									Get(Index index = Root) const
			{
				return *reinterpret_cast<const Node *>(&this->mNodes[index]);
			}

		private:
			std::vector<unsigned char>							mNodes;
			std::unordered_set<std::string>						mStrings;
	};

	namespace													EffectNodes
	{
		struct													Type
		{
			enum												Base
			{
				Void,
				Bool,
				Int,
				Uint,
				Float,
				Texture,
				Sampler,
				Struct,
				String
			};
			enum												Qualifier
			{
				// Storage
				Extern											= 1 << 0,
				Static											= 1 << 1,
				Uniform											= 1 << 2,
				Volatile										= 1 << 3,
				Precise											= 1 << 4,
				In												= 1 << 5,
				Out												= 1 << 6,
				InOut											= In | Out,

				// Modifier
				Const											= 1 << 7,

				// Interpolation
				NoInterpolation									= 1 << 10,
				NoPerspective									= 1 << 11,
				Linear											= 1 << 12,
				Centroid										= 1 << 13,
				Sample											= 1 << 14
			};

			static unsigned int									Compatible(const Type &left, const Type &right)
			{
				if (left.IsArray() != right.IsArray() || left.ArrayLength != right.ArrayLength)
				{
					return 0;
				}
				if (left.IsStruct() || right.IsStruct())
				{
					return left.Definition == right.Definition;
				}
				if (left.Class == right.Class && left.Rows == right.Rows && left.Cols == right.Cols)
				{
					return 1;
				}
				if (!left.IsNumeric() || !right.IsNumeric())
				{
					return 0;
				}

				const int ranks[4][4] =
				{
					{ 0, 5, 5, 5 },
					{ 4, 0, 3, 5 },
					{ 4, 2, 0, 5 },
					{ 4, 4, 4, 0 }
				};

				const int rank = ranks[left.Class - 1][right.Class - 1] << 2;

				if (left.IsScalar() && right.IsVector())
				{
					return rank | 2;
				}
				if (left.IsVector() && right.IsScalar() || (left.IsVector() == right.IsVector() && left.Rows > right.Rows && left.Cols >= right.Cols))
				{
					return rank | 32;
				}
				if (left.IsVector() != right.IsVector() || left.IsMatrix() != right.IsMatrix() || left.Rows * left.Cols != right.Rows * right.Cols)
				{
					return 0;
				}

				return rank;
			}

			inline bool											IsArray(void) const
			{
				return this->ArrayLength != 0;
			}
			inline bool											IsMatrix(void) const
			{
				return this->Rows >= 1 && this->Cols > 1;
			}
			inline bool											IsVector(void) const
			{
				return this->Rows > 1 && !IsMatrix();
			}
			inline bool											IsScalar(void) const
			{
				return !IsArray() && !IsMatrix() && !IsVector() && IsNumeric();
			}
			inline bool											IsNumeric(void) const
			{
				return IsBoolean() || IsIntegral() || IsFloatingPoint();
			}
			inline bool											IsVoid(void) const
			{
				return this->Class == Void;
			}
			inline bool											IsBoolean(void) const
			{
				return this->Class == Bool;
			}
			inline bool											IsIntegral(void) const
			{
				return this->Class == Int || this->Class == Uint;
			}
			inline bool											IsFloatingPoint(void) const
			{
				return this->Class == Float;
			}
			inline bool											IsTexture(void) const
			{
				return this->Class == Texture;
			}
			inline bool											IsSampler(void) const
			{
				return this->Class == Sampler;
			}
			inline bool											IsStruct(void) const
			{
				return this->Class == Struct;
			}

			inline bool											HasQualifier(Qualifier qualifier) const
			{
				return HasQualifiers(qualifier);
			}
			inline bool											HasQualifiers(unsigned int qualifiers) const
			{
				return (this->Qualifiers & qualifiers) == qualifiers;
			}

			Base												Class;
			unsigned int										Qualifiers;
			unsigned int										Rows : 4, Cols : 4;
			int													ArrayLength : 24;
			EffectTree::Index									Definition;
		};
		struct 													List : public EffectTree::NodeImplementation<List>
		{
			EffectTree::Index									Find(const EffectTree &ast, std::size_t index) const
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
					return ast[this->Next].As<List>().Find(ast, index - 16);
				}
			}
			void	 											Link(EffectTree &ast, EffectTree::Index node)
			{
				if (node == 0)
				{
					return;
				}

				this->AST = &ast;

				if (ast[node].Is<List>())
				{
					EffectTree::Index current = this->Index;
					const unsigned int length = ast[node].As<List>().Length;

					for (unsigned int i = 0; i < length; ++i)
					{
						ast[current].As<List>().Link(ast, ast[node].As<List>()[i]);
					}
				}
				else if (this->Length >= 16)
				{
					if (this->Next == 0)
					{
						EffectTree::Index current = this->Index;
						EffectTree::Index continuation = ast.Add<List>().Index; // Do NOT use the "this" pointer after this call anymore, memory might have changed
							
						ast[current].As<List>().Next = continuation;
						ast[continuation].As<List>().Previous = current;
						ast[continuation].As<List>().Link(ast, node);
					}
					else
					{
						ast[this->Next].As<List>().Link(ast, node);
					}
				}
				else
				{
					this->Nodes[this->Length] = node;

					Increment(ast);
				}
			}
			void												Increment(EffectTree &ast)
			{
				if (this->Previous != 0)
				{
					ast[this->Previous].As<List>().Increment(ast);
				}
					
				this->Length++;
			}
			
			inline EffectTree::Index							operator[](unsigned int index) const
			{
				return Find(*this->AST, index);
			}

			EffectTree *										AST;
			unsigned int										Length;
			EffectTree::Index									Previous, Next, Nodes[16];
		};
		struct													RValue : public EffectTree::Node
		{
			Type												Type;
		};
		struct													LValue : public EffectTree::NodeImplementation<LValue, RValue>
		{
			EffectTree::Index									Reference;
		};
		struct													Literal : public EffectTree::NodeImplementation<Literal, RValue>
		{
			enum												Enum
			{
				NONE											= 0,
				ZERO											= 0,
				ONE												= 1,
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
				POINT,
				LINEAR,
				ANISOTROPIC,
				CLAMP,
				REPEAT,
				MIRROR,
				BORDER,
				SRCCOLOR,
				SRCALPHA,
				INVSRCCOLOR,
				INVSRCALPHA,
				DESTCOLOR,
				DESTALPHA,
				INVDESTCOLOR,
				INVDESTALPHA,
				ADD,
				SUBTRACT,
				REVSUBTRACT,
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

			template <typename T> T								Cast(std::size_t index) const;
			template <>	int										Cast(std::size_t index) const
			{
				switch (this->Type.Class)
				{
					default:
					case Type::Int:
					case Type::Bool:
						return this->Value.Int[index];
					case Type::Uint:
						return static_cast<int>(this->Value.Uint[index]);
					case Type::Float:
						return static_cast<int>(this->Value.Float[index]);
				}
			}
			template <>	unsigned int							Cast(std::size_t index) const
			{
				switch (this->Type.Class)
				{
					case Type::Int:
					case Type::Bool:
						return static_cast<unsigned int>(this->Value.Int[index]);
					default:
					case Type::Uint:
						return this->Value.Uint[index];
					case Type::Float:
						return static_cast<unsigned int>(this->Value.Float[index]);
				}
			}
			template <>	float									Cast(std::size_t index) const
			{
				switch (this->Type.Class)
				{
					case Type::Int:
					case Type::Bool:
						return static_cast<float>(this->Value.Int[index]);
					case Type::Uint:
						return static_cast<float>(this->Value.Uint[index]);
					default:
					case Type::Float:
						return this->Value.Float[index];
				}
			}

			union												Value
			{
				int												Int[16], Bool[16];
				unsigned int									Uint[16];
				float											Float[16];
				const char *									String;
			} Value;
		};
		struct													Expression : public EffectTree::NodeImplementation<Expression, RValue>
		{
			enum												Operator
			{
				None,

				// Unary
				Negate,
				BitNot,
				LogicNot,
				Increase,
				Decrease,
				PostIncrease,
				PostDecrease,
				Abs,
				Sign,
				Rcp,
				All,
				Any,
				Sin,
				Sinh,
				Cos,
				Cosh,
				Tan,
				Tanh,
				Asin,
				Acos,
				Atan,
				Exp,
				Exp2,
				Log,
				Log2,
				Log10,
				Sqrt,
				Rsqrt,
				Ceil,
				Floor,
				Frac,
				Trunc,
				Round,
				Saturate,
				Radians,
				Degrees,
				Noise,
				Length,
				Normalize,
				Transpose,
				Determinant,
				Cast,
				BitCastInt2Float,
				BitCastUint2Float,
				BitCastFloat2Int,
				BitCastFloat2Uint,

				// Binary
				Add,
				Subtract,
				Multiply,
				Divide,
				Modulo,
				Less,
				Greater,
				LessOrEqual,
				GreaterOrEqual,
				Equal,
				NotEqual,
				LeftShift,
				RightShift,
				BitAnd,
				BitXor,
				BitOr,
				LogicAnd,
				LogicXor,
				LogicOr,
				Mul,
				Atan2,
				Dot,
				Cross,
				Distance,
				Pow,
				Modf,
				Frexp,
				Ldexp,
				Min,
				Max,
				Step,
				Reflect,
				Extract,
				Swizzle,
				Field,
				Tex,
				TexLevel,
				TexBias,
				TexFetch,
				TexSize,

				// Ternary
				Mad,
				SinCos,
				Lerp,
				Clamp,
				SmoothStep,
				Refract,
				FaceForward,
				Conditional,
				TexOffset,
				TexLevelOffset,

				OperatorCount
			};

			unsigned int										Operator;
			EffectTree::Index									Operands[3];
		};
		struct													Sequence : public EffectTree::NodeImplementation<Sequence, List>
		{
			Type												Type;
		};
		struct													Assignment : public EffectTree::NodeImplementation<Assignment, RValue>
		{
			unsigned int										Operator;
			EffectTree::Index									Left, Right;
		};
		struct													Call : public EffectTree::NodeImplementation<Call, RValue>
		{
			inline bool											HasArguments(void) const
			{
				return this->Arguments != 0;
			}

			const char *										CalleeName;
			EffectTree::Index									Callee, Arguments;
		};
		struct													Constructor : public EffectTree::NodeImplementation<Constructor, RValue>
		{
			EffectTree::Index									Arguments;
		};
		struct													Swizzle : public EffectTree::NodeImplementation<Swizzle, Expression>
		{
			signed char											Mask[4];
		};
		struct													If : public EffectTree::NodeImplementation<If>
		{
			EffectTree::Index									Condition;
			EffectTree::Index									StatementOnTrue, StatementOnFalse;
		};
		struct													Switch : public EffectTree::NodeImplementation<Switch>
		{
			EffectTree::Index									Test;
			EffectTree::Index									Cases;
		};
		struct													Case : public EffectTree::NodeImplementation<Case>
		{
			EffectTree::Index									Labels;
			EffectTree::Index									Statements;
		};
		struct													For : public EffectTree::NodeImplementation<For>
		{
			EffectTree::Index									Initialization, Condition, Iteration;
			EffectTree::Index									Statements;
		};
		struct													While : public EffectTree::NodeImplementation<While>
		{
			bool												DoWhile;
			EffectTree::Index									Condition;
			EffectTree::Index									Statements;
		};
		struct													Jump : public EffectTree::NodeImplementation<Jump>
		{
			enum												Mode
			{
				Return,
				Break,
				Continue,
				Discard
			};

			Mode												Mode;
			EffectTree::Index									Value;
		};
		struct													ExpressionStatement : public EffectTree::NodeImplementation<ExpressionStatement>
		{
			EffectTree::Index									Expression;
		};
		struct													StatementBlock : public EffectTree::NodeImplementation<StatementBlock, List>
		{
		};
		struct													Annotation : public EffectTree::NodeImplementation<Annotation>
		{
			const char *										Name;
			EffectTree::Index									Value;
		};
		struct													Struct : public EffectTree::NodeImplementation<Struct>
		{
			inline bool											HasFields(void) const
			{
				return this->Fields != 0;
			}

			const char *										Name;
			EffectTree::Index									Fields;
		};
		struct													Variable : public EffectTree::NodeImplementation<Variable>
		{
			enum												Property
			{
				Width,
				Height,
				MipLevels,
				Format,
				Texture,
				AddressU,
				AddressV,
				AddressW,
				MinFilter,
				MagFilter,
				MipFilter,
				MipLODBias,
				MaxAnisotropy,
				MinLOD,
				MaxLOD,
				SRGBTexture,

				PropertyCount
			};

			inline bool											HasAnnotations(void) const
			{
				return this->Annotations != 0;
			}
			inline bool											HasInitializer(void) const
			{
				return this->Initializer != 0;
			}

			Type												Type;
			const char *										Name;
			const char *										Semantic;
			EffectTree::Index									Annotations;
			EffectTree::Index									Initializer;
			EffectTree::Index									Properties[PropertyCount];
		};
		struct													Function : public EffectTree::NodeImplementation<Function>
		{
			inline bool											HasParameters(void) const
			{
				return this->Parameters != 0;
			}
			inline bool											HasDefinition(void) const
			{
				return this->Definition != 0;
			}

			Type												ReturnType;
			const char *										Name;
			EffectTree::Index									Parameters;
			const char *										ReturnSemantic;
			EffectTree::Index									Definition;
		};
		struct													Technique : public EffectTree::NodeImplementation<Technique>
		{
			inline bool											HasAnnotations(void) const
			{
				return this->Annotations != 0;
			}

			const char *										Name;
			EffectTree::Index									Annotations;
			EffectTree::Index									Passes;
		};
		struct													Pass : public EffectTree::NodeImplementation<Pass>
		{
			enum												State
			{
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
				ColorWriteMask,
				SRGBWriteEnable,
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
				StencilDepthFail,

				StateCount
			};

			inline bool											HasAnnotations(void) const
			{
				return this->Annotations != 0;
			}

			const char *										Name;
			EffectTree::Index									Annotations;
			EffectTree::Index									States[StateCount];
		};
	}

	template <typename V> void									EffectTree::Node::Accept(V &visitor)
	{
		using namespace EffectNodes;

		if (Is<Variable>())
		{
			visitor.Visit(As<Variable>());
		}
		else if (Is<Function>())
		{
			visitor.Visit(As<Function>());
		}
		else if (Is<Literal>())
		{
			visitor.Visit(As<Literal>());
		}
		else if (Is<LValue>())
		{
			visitor.Visit(As<LValue>());
		}
		else if (Is<Expression>())
		{
			visitor.Visit(As<Expression>());
		}
		else if (Is<Swizzle>())
		{
			visitor.Visit(As<Swizzle>());
		}
		else if (Is<Assignment>())
		{
			visitor.Visit(As<Assignment>());
		}
		else if (Is<Sequence>())
		{
			visitor.Visit(As<Sequence>());
		}
		else if (Is<Constructor>())
		{
			visitor.Visit(As<Constructor>());
		}
		else if (Is<Call>())
		{
			visitor.Visit(As<Call>());
		}
		else if (Is<ExpressionStatement>())
		{
			visitor.Visit(As<Constructor>());
		}
		else if (Is<If>())
		{
			visitor.Visit(As<If>());
		}
		else if (Is<Switch>())
		{
			visitor.Visit(As<Switch>());
		}
		else if (Is<Case>())
		{
			visitor.Visit(As<Case>());
		}
		else if (Is<For>())
		{
			visitor.Visit(As<For>());
		}
		else if (Is<While>())
		{
			visitor.Visit(As<While>());
		}
		else if (Is<Jump>())
		{
			visitor.Visit(As<Jump>());
		}
		else if (Is<StatementBlock>())
		{
			visitor.Visit(As<StatementBlock>());
		}
		else if (Is<Annotation>())
		{
			visitor.Visit(As<Annotation>());
		}
		else if (Is<Struct>())
		{
			visitor.Visit(As<Struct>());
		}
		else if (Is<Technique>())
		{
			visitor.Visit(As<Technique>());
		}
		else if (Is<Pass>())
		{
			visitor.Visit(As<Pass>());
		}
		else
		{
			assert(false);
		}
	}
	template <typename V> void									EffectTree::Node::Accept(V &visitor) const
	{
		using namespace EffectNodes;

		if (Is<Variable>())
		{
			visitor.Visit(As<Variable>());
		}
		else if (Is<Function>())
		{
			visitor.Visit(As<Function>());
		}
		else if (Is<Literal>())
		{
			visitor.Visit(As<Literal>());
		}
		else if (Is<LValue>())
		{
			visitor.Visit(As<LValue>());
		}
		else if (Is<Expression>())
		{
			visitor.Visit(As<Expression>());
		}
		else if (Is<Swizzle>())
		{
			visitor.Visit(As<Swizzle>());
		}
		else if (Is<Assignment>())
		{
			visitor.Visit(As<Assignment>());
		}
		else if (Is<Sequence>())
		{
			visitor.Visit(As<Sequence>());
		}
		else if (Is<Constructor>())
		{
			visitor.Visit(As<Constructor>());
		}
		else if (Is<Call>())
		{
			visitor.Visit(As<Call>());
		}
		else if (Is<ExpressionStatement>())
		{
			visitor.Visit(As<ExpressionStatement>());
		}
		else if (Is<If>())
		{
			visitor.Visit(As<If>());
		}
		else if (Is<Switch>())
		{
			visitor.Visit(As<Switch>());
		}
		else if (Is<Case>())
		{
			visitor.Visit(As<Case>());
		}
		else if (Is<For>())
		{
			visitor.Visit(As<For>());
		}
		else if (Is<While>())
		{
			visitor.Visit(As<While>());
		}
		else if (Is<Jump>())
		{
			visitor.Visit(As<Jump>());
		}
		else if (Is<StatementBlock>())
		{
			visitor.Visit(As<StatementBlock>());
		}
		else if (Is<Annotation>())
		{
			visitor.Visit(As<Annotation>());
		}
		else if (Is<Struct>())
		{
			visitor.Visit(As<Struct>());
		}
		else if (Is<Technique>())
		{
			visitor.Visit(As<Technique>());
		}
		else if (Is<Pass>())
		{
			visitor.Visit(As<Pass>());
		}
		else
		{
			assert(false);
		}
	}
}