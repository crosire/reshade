#pragma once

#include <vector>

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
				template <typename T> inline bool				Is(void) const
				{
					return this->Type == T::NodeType;
				}
				template <typename T> inline T &				As(void)
				{
					//assert(Is<T>());

					return *reinterpret_cast<T *>(reinterpret_cast<unsigned char *>(this) + sizeof(Node));
				}
				template <typename T> inline const T &			As(void) const
				{
					//assert(Is<T>());

					return *reinterpret_cast<const T *>(reinterpret_cast<const unsigned char *>(this) + sizeof(Node));
				}
			
				int												Type;
				Index											Index;
				Location										Location;
			};

			static const Index									Root = 1;
			
		public:
			EffectTree(void);
			EffectTree(const unsigned char *data, std::size_t size);
		
			inline Node &										operator [](Index index)
			{
				return Get(index);
			}
			inline const Node &									operator [](Index index) const
			{
				return Get(index);
			}
			
			template <typename T> inline Node &					Add(void)
			{
				const Location location = { nullptr, 0, 0 };
				return Add<T>(location);
			}
			template <typename T> Node &						Add(const Location &location) // Any pointers to nodes are no longer valid after this call
			{
				const Index index = this->mNodes.size();
				
				this->mNodes.resize(index + sizeof(Node) + sizeof(T));
				
				EffectTree::Node &node = operator [](index);
				node.Type = T::NodeType;
				node.Index = index;
				node.Location = location;
				
				return node;
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
	};

	namespace Nodes
	{
		enum class												Operator
		{
			None												= 0,
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
		enum class												Attribute
		{
			None												= 0,
			Flatten,
			Branch,
			ForceCase,
			Loop,
			FastOpt,
			Unroll
		};
		struct 													Type
		{
			enum												Class
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
			enum class											Qualifier
			{
				None											= 0,
				Const											= 1 << 0,
				Volatile										= 1 << 1,
				Static											= 1 << 2,
				Extern											= 1 << 3,
				Uniform											= 1 << 4,
				Inline											= 1 << 5,
				In												= 1 << 6,
				Out												= 1 << 7,
				InOut											= In | Out,
				Precise											= 1 << 8,
				NoInterpolation									= 1 << 10,
				NoPerspective									= 1 << 11,
				Linear											= 1 << 12,
				Centroid										= 1 << 13,
				Sample											= 1 << 14,
				RowMajor										= 1 << 15,
				ColumnMajor										= 1 << 16,
				Unorm											= 1 << 17,
				Snorm											= 1 << 18
			};
			enum class											Compatibility
			{
				None 											= 0,
				Match,
				Promotion,
				Implicit,
				ImplicitWithPromotion,
				UpwardVectorPromotion
			};
			
			static Compatibility								Compatible(const Type &left, const Type &right);
			
			static const Type									Undefined;
			
			inline bool											IsArray(void) const
			{
				return this->ElementsDimension > 0;
			}
			inline	bool										IsMatrix(void) const
			{
				return this->Rows >= 1 && this->Cols > 1;
			}
			inline bool											IsVector(void) const
			{
				return this->Rows > 1 && !IsMatrix();
			}
			inline bool											IsScalar(void) const
			{
				return !IsArray() && !IsMatrix() && !IsVector() && !IsStruct();
			}
			inline bool											IsNumeric(void) const
			{
				return this->Class >= Bool && this->Class <= Double;
			}
			inline bool											IsIntegral(void) const
			{
				return this->Class >= Bool && this->Class <= Uint;
			}
			inline bool											IsFloatingPoint(void) const
			{
				return this->Class == Half || this->Class == Float || this->Class == Double;
			}
			inline bool											IsSampler(void) const
			{
				return this->Class == Sampler;
			}
			inline bool											IsTexture(void) const
			{
				return this->Class == Texture;
			}
			inline bool											IsStruct(void) const
			{
				return this->Class == Struct;
			}
			
			inline bool											HasQualifier(Type::Qualifier qualifier) const
			{
				return (this->Qualifiers & static_cast<unsigned int>(qualifier)) == static_cast<unsigned int>(qualifier);
			}

			Class												Class;
			unsigned int										Qualifiers;
			unsigned int										Rows : 4, Cols : 4;
			unsigned int										Elements[16], ElementsDimension;
			EffectTree::Index									Definition;
		};

		struct 													Aggregate
		{
			enum { NodeType = 1 };
		
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
					return ast[this->Continuation].As<Aggregate>().Find(ast, index - 16);
				}
			}
			void	 											Link(EffectTree &ast, EffectTree::Index node)
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
			void												Increment(EffectTree &ast)
			{
				if (this->Previous != 0)
				{
					ast[this->Previous].As<Aggregate>().Increment(ast);
				}
				
				this->Length++;
			}
			
			std::size_t											Length;
			EffectTree::Index									Previous, Continuation, Nodes[16];
		};
		struct													Expression
		{
			enum { NodeType = 2 };
			
			Type												Type;
		};
		struct													ExpressionSequence : public Aggregate
		{
			enum { NodeType = 29 };
		};
		struct 													Literal : public Expression
		{		
			enum { NodeType = 3 };

			enum												Enum
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
				int												AsInt;
				unsigned int									AsUint;
				float											AsFloat;
				double 											AsDouble;
				const char *									AsString;
			} Value;
		};
		struct 													Reference : public Expression
		{
			enum { NodeType = 4 };
			
			EffectTree::Index									Symbol;
		};
		struct 													UnaryExpression : public Expression
		{
			enum { NodeType = 5 };
			
			EffectTree::Index									Operand;
			Operator											Operator;
		};
		struct 													BinaryExpression : public Expression
		{
			enum { NodeType = 6 };
			
			EffectTree::Index									Left, Right;
			Operator											Operator;
		};
		struct													Assignment : public BinaryExpression
		{
			enum { NodeType = 28 };
		};
		struct 													Conditional : public Expression
		{
			enum { NodeType = 7 };
			
			EffectTree::Index									Condition, ExpressionTrue, ExpressionFalse;
		};
		struct 													Call : public Expression
		{
			enum { NodeType = 8 };
			
			const char *										CalleeName;
			EffectTree::Index									Left, Callee, Parameters;
		};
		struct 													Constructor : public Expression
		{
			enum { NodeType = 9 };
			
			EffectTree::Index									Parameters;
		};
		struct 													Field : public Expression
		{
			enum { NodeType = 10 };
			
			EffectTree::Index									Left, Callee;
		};
		struct 													Swizzle : public Expression
		{
			enum { NodeType = 11 };
			
			EffectTree::Index									Left;
			int													Offsets[4];
		};
		struct 													StatementBlock : public Aggregate
		{
			enum { NodeType = 27 };
		};
		struct 													ExpressionStatement
		{
			enum { NodeType = 12 };
			
			EffectTree::Index									Expression;
		};
		struct 													DeclarationStatement
		{
			enum { NodeType = 13 };
			
			EffectTree::Index									Declaration;
		};
		struct 													Selection
		{
			enum { NodeType = 14 };
			
			enum												Mode
			{
				If,
				Switch,
				Case
			};
		
			Mode												Mode;
			Attribute											Attributes;
			EffectTree::Index									Condition;
			union
			{
				EffectTree::Index								Statement;
				EffectTree::Index								StatementTrue;
			};
			EffectTree::Index									StatementFalse;
		};
		struct 													Iteration
		{
			enum { NodeType = 15 };
			
			enum												Mode
			{
				For,
				While,
				DoWhile
			};
			
			Mode 												Mode;
			Attribute 											Attributes;
			EffectTree::Index									Initialization, Condition, Expression, Statement;
		};
		struct 													Jump
		{
			enum { NodeType = 16 };
			
			enum												Mode
			{
				Break,
				Continue,
				Return,
				Discard
			};
			
			Mode 												Mode;
			EffectTree::Index									Expression;
		};
		struct 													Annotation
		{
			enum { NodeType = 17 };
			
			const char *										Name;
			Type												Type;
			union
			{
				int												AsInt;
				unsigned int									AsUint;
				float											AsFloat;
				double											AsDouble;
				const char *									AsString;
			} Value;
		};
		struct 													Function
		{
			enum { NodeType = 18 };
			
			inline bool											HasArguments(void) const
			{
				return this->Arguments != 0;
			}
			inline bool											HasDefinition(void) const
			{
				return this->Definition != 0;
			}

			const char *										Name;
			EffectTree::Index									Arguments, Definition;
			Type												ReturnType;
			const char *										ReturnSemantic;
		};
		struct 													Variable
		{
			enum { NodeType = 19 };
			
			inline bool											HasSemantic(void) const
			{
				return this->Semantic != nullptr;
			}
			inline bool 										HasInitializer(void) const
			{
				return this->Initializer != 0;
			}
			inline bool											HasAnnotation(void) const
			{
				return this->Annotations != 0;
			}
							
			const char *										Name;
			Type												Type;
			EffectTree::Index									Initializer, Annotations, Parent;
			const char *										Semantic;
			bool												Argument;
		};
		struct 													Struct
		{
			enum { NodeType = 20 };
			
			const char *										Name;
			EffectTree::Index									Fields;
		};
		struct 													Technique
		{
			enum { NodeType = 23 };
			
			const char *										Name;
			EffectTree::Index									Passes, Annotations;
		};
		struct 													Pass
		{
			enum { NodeType = 24 };
			
			const char *										Name;
			EffectTree::Index									States, Annotations;
		};
		struct 													State
		{
			enum { NodeType = 25 };
			
			enum												Type
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
			
			int													Type;
			union
			{
				int												AsInt;
				float											AsFloat;
				EffectTree::Index								AsNode;
			} Value;
		};
		struct													Typedef
		{
			enum { NodeType = 26 };
			
			const char *										Name;
			Type												Type;
		};
	}
}