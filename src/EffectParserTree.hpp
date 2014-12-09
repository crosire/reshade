#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <typeinfo>

namespace ReShade
{
	struct EffectTree
	{
	public:
		typedef std::size_t Index;
		struct Location
		{
			const char *Source;
			unsigned int Line, Column;
		};
		struct Node
		{
		public:
			template <typename T>
			inline bool Is() const
			{
				return typeid(T).hash_code() == this->mT;
			}
			template <typename T>
			inline T &As()
			{
				static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

				return static_cast<T &>(*this);
			}
			template <typename T>
			inline const T &As() const
			{
				static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

				return static_cast<const T &>(*this);
			}

			Index Index;
			Location Location;

		protected:
			std::size_t mT;
		};
		template <class T, class BASE = Node>
		class NodeImplementation : public BASE
		{
			static_assert(std::is_base_of<Node, BASE>::value, "invalid effect tree node base type");

		public:
			inline NodeImplementation()
			{
				this->mT = typeid(T).hash_code();
			}
		};

		static const Index Null = 0, Root = 1;
				
	public:
		EffectTree() : mNodes(Root) // Padding so root node is at index 1
		{
		}
			
		inline Node &operator [](Index index)
		{
			return Get(index);
		}
		inline const Node &operator [](Index index) const
		{
			return Get(index);
		}
			
		template <typename T>
		inline T &Add()
		{
			const Location location = { nullptr, 0, 0 };
			return Add<T>(location);
		}
		template <typename T>
		T &Add(const Location &location) // Any pointers to nodes are no longer valid after this call
		{
			static_assert(std::is_base_of<Node, T>::value, "invalid effect tree node type");

			const Index index = this->mNodes.size();

			this->mNodes.resize(index + sizeof(T), 0);

			Node &node = *new (&Get(index)) T;
			node.Index = index;
			node.Location = location;
					
			return static_cast<T &>(node);
		}
		inline const char *AddString(const char *string, std::size_t length)
		{
			return (*this->mStrings.emplace(string, length).first).c_str();
		}
		inline Node &Get(Index index = Root)
		{
			return *reinterpret_cast<Node *>(&this->mNodes[index]);
		}
		inline const Node &Get(Index index = Root) const
		{
			return *reinterpret_cast<const Node *>(&this->mNodes[index]);
		}

	private:
		std::vector<unsigned char> mNodes;
		std::unordered_set<std::string> mStrings;
	};

	namespace EffectNodes
	{
		struct Root : public EffectTree::NodeImplementation<Root>
		{
			EffectTree::Index NextDeclaration;
		};
		struct Type
		{
			enum Base
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
			enum Qualifier
			{
				// Storage
				Extern = 1 << 0,
				Static = 1 << 1,
				Uniform = 1 << 2,
				Volatile = 1 << 3,
				Precise = 1 << 4,
				In = 1 << 5,
				Out = 1 << 6,
				InOut = In | Out,

				// Modifier
				Const = 1 << 7,

				// Interpolation
				NoInterpolation = 1 << 10,
				NoPerspective = 1 << 11,
				Linear = 1 << 12,
				Centroid = 1 << 13,
				Sample = 1 << 14
			};

			static unsigned int Compatible(const Type &src, const Type &dst)
			{
				if (src.IsArray() != dst.IsArray() || src.ArrayLength != dst.ArrayLength)
				{
					return 0;
				}
				if (src.IsStruct() || dst.IsStruct())
				{
					return src.Definition == dst.Definition;
				}
				if (src.Class == dst.Class && src.Rows == dst.Rows && src.Cols == dst.Cols)
				{
					return 1;
				}
				if (!src.IsNumeric() || !dst.IsNumeric())
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

				const int rank = ranks[src.Class - 1][dst.Class - 1] << 2;

				if (src.IsScalar() && dst.IsVector())
				{
					return rank | 2;
				}
				if (src.IsVector() && dst.IsScalar() || (src.IsVector() == dst.IsVector() && src.Rows > dst.Rows && src.Cols >= dst.Cols))
				{
					return rank | 32;
				}
				if (src.IsVector() != dst.IsVector() || src.IsMatrix() != dst.IsMatrix() || src.Rows * src.Cols != dst.Rows * dst.Cols)
				{
					return 0;
				}

				return rank;
			}

			inline bool IsArray() const
			{
				return this->ArrayLength != 0;
			}
			inline bool IsMatrix() const
			{
				return this->Rows >= 1 && this->Cols > 1;
			}
			inline bool IsVector() const
			{
				return this->Rows > 1 && !IsMatrix();
			}
			inline bool IsScalar() const
			{
				return !IsArray() && !IsMatrix() && !IsVector() && IsNumeric();
			}
			inline bool IsNumeric() const
			{
				return IsBoolean() || IsIntegral() || IsFloatingPoint();
			}
			inline bool IsVoid() const
			{
				return this->Class == Void;
			}
			inline bool IsBoolean() const
			{
				return this->Class == Bool;
			}
			inline bool IsIntegral() const
			{
				return this->Class == Int || this->Class == Uint;
			}
			inline bool IsFloatingPoint() const
			{
				return this->Class == Float;
			}
			inline bool IsTexture() const
			{
				return this->Class == Texture;
			}
			inline bool IsSampler() const
			{
				return this->Class == Sampler;
			}
			inline bool IsStruct() const
			{
				return this->Class == Struct;
			}

			inline bool HasQualifier(Qualifier qualifier) const
			{
				return (this->Qualifiers & static_cast<unsigned int>(qualifier)) == static_cast<unsigned int>(qualifier);
			}

			Base Class;
			unsigned int Qualifiers;
			unsigned int Rows : 4, Cols : 4;
			int ArrayLength : 24;
			EffectTree::Index Definition;
		};
		struct RValue : public EffectTree::Node
		{
			Type Type;
			EffectTree::Index NextExpression;
		};
		struct LValue : public EffectTree::NodeImplementation<LValue, RValue>
		{
			EffectTree::Index Reference;
		};
		struct Literal : public EffectTree::NodeImplementation<Literal, RValue>
		{
			enum Enum
			{
				NONE = 0,
				ZERO = 0,
				ONE = 1,
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

			static void Cast(const Literal &from, std::size_t k, Literal &to, std::size_t j)
			{
				switch (to.Type.Class)
				{
					case EffectNodes::Type::Bool:
					case EffectNodes::Type::Int:
						switch (from.Type.Class)
						{
							case Type::Bool:
							case Type::Int:
							case Type::Uint:
								to.Value.Int[k] = from.Value.Int[j];
								break;
							case Type::Float:
								to.Value.Int[k] = static_cast<int>(from.Value.Float[j]);
								break;
						}
						break;
					case EffectNodes::Type::Uint:
						switch (from.Type.Class)
						{
							case Type::Bool:
							case Type::Int:
							case Type::Uint:
								to.Value.Uint[k] = from.Value.Uint[j];
								break;
							case Type::Float:
								to.Value.Uint[k] = static_cast<unsigned int>(from.Value.Float[j]);
								break;
						}
						break;
					case EffectNodes::Type::Float:
						switch (from.Type.Class)
						{
							case Type::Int:
							case Type::Bool:
								to.Value.Float[k] = static_cast<float>(from.Value.Int[j]);
								break;
							case Type::Uint:
								to.Value.Float[k] = static_cast<float>(from.Value.Uint[j]);
								break;
							case Type::Float:
								to.Value.Float[k] = from.Value.Float[j];
								break;
						}
						break;
				}
			}

			union Value
			{
				static_assert(sizeof(int) == sizeof(float), "union array requires element size to match to allow for manipulation");

				int Int[16], Bool[16];
				unsigned int Uint[16];
				float Float[16];
				const char *String;
			} Value;
		};
		struct Expression : public EffectTree::NodeImplementation<Expression, RValue>
		{
			enum Operator
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
				PartialDerivativeX,
				PartialDerivativeY,
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
				TexGather,
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
				TexGatherOffset,

				OperatorCount
			};

			unsigned int Operator;
			EffectTree::Index Operands[3];
		};
		struct Sequence : public EffectTree::NodeImplementation<Sequence, RValue>
		{
			EffectTree::Index Expressions;
		};
		struct Assignment : public EffectTree::NodeImplementation<Assignment, RValue>
		{
			unsigned int Operator;
			EffectTree::Index Left, Right;
		};
		struct Call : public EffectTree::NodeImplementation<Call, RValue>
		{
			const char *CalleeName;
			EffectTree::Index Callee, Arguments;
			unsigned int ArgumentCount;
		};
		struct Constructor : public EffectTree::NodeImplementation<Constructor, RValue>
		{
			EffectTree::Index Arguments;
			unsigned int ArgumentCount;
		};
		struct Swizzle : public EffectTree::NodeImplementation<Swizzle, Expression>
		{
			signed char Mask[4];
		};
		struct InitializerList : public EffectTree::NodeImplementation<InitializerList, RValue>
		{
			EffectTree::Index Expressions;
		};
		struct Statement : public EffectTree::NodeImplementation<Statement>
		{
			const char *Attributes;
			EffectTree::Index NextStatement;
		};
		struct If : public EffectTree::NodeImplementation<If, Statement>
		{
			EffectTree::Index Condition;
			EffectTree::Index StatementOnTrue, StatementOnFalse;
		};
		struct Switch : public EffectTree::NodeImplementation<Switch, Statement>
		{
			EffectTree::Index Test;
			EffectTree::Index Cases;
		};
		struct Case : public EffectTree::NodeImplementation<Case>
		{
			EffectTree::Index Labels;
			EffectTree::Index Statements;
			EffectTree::Index NextCase;
		};
		struct For : public EffectTree::NodeImplementation<For, Statement>
		{
			EffectTree::Index Initialization, Condition, Iteration;
			EffectTree::Index Statements;
		};
		struct While : public EffectTree::NodeImplementation<While, Statement>
		{
			bool DoWhile;
			EffectTree::Index Condition;
			EffectTree::Index Statements;
		};
		struct Return : public EffectTree::NodeImplementation<Return, Statement>
		{
			bool Discard;
			EffectTree::Index Value;
		};
		struct Jump : public EffectTree::NodeImplementation<Jump, Statement>
		{
			enum Mode
			{
				Break,
				Continue
			};

			Mode Mode;
		};
		struct ExpressionStatement : public EffectTree::NodeImplementation<ExpressionStatement, Statement>
		{
			EffectTree::Index Expression;
		};
		struct DeclarationStatement : public EffectTree::NodeImplementation<DeclarationStatement, Statement>
		{
			EffectTree::Index Declaration;
		};
		struct StatementBlock : public EffectTree::NodeImplementation<StatementBlock, Statement>
		{
			EffectTree::Index Statements;
		};
		struct Annotation : public EffectTree::NodeImplementation<Annotation>
		{
			const char *Name;
			EffectTree::Index Value;
			EffectTree::Index NextAnnotation;
		};
		struct Struct : public EffectTree::NodeImplementation<Struct, Root>
		{
			const char *Name;
			EffectTree::Index Fields;
		};
		struct Variable : public EffectTree::NodeImplementation<Variable, Root>
		{
			enum Property
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

			Type Type;
			const char *Name;
			const char *Semantic;
			EffectTree::Index Annotations;
			EffectTree::Index Initializer;
			EffectTree::Index Properties[PropertyCount];
			EffectTree::Index NextDeclarator;
		};
		struct Function : public EffectTree::NodeImplementation<Function, Root>
		{
			Type ReturnType;
			const char *Name;
			EffectTree::Index Parameters;
			unsigned int ParameterCount;
			const char *ReturnSemantic;
			EffectTree::Index Definition;
		};
		struct Technique : public EffectTree::NodeImplementation<Technique, Root>
		{
			const char *Name;
			EffectTree::Index Annotations;
			EffectTree::Index Passes;
		};
		struct Pass : public EffectTree::NodeImplementation<Pass>
		{
			enum State
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

			const char *Name;
			EffectTree::Index Annotations;
			EffectTree::Index States[StateCount];
			EffectTree::Index NextPass;
		};
	}
}