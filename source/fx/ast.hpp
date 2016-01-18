#pragma once

#include "lexer.hpp"

#include <vector>
#include <list>
#include <algorithm>

namespace reshade
{
	namespace fx
	{
		enum class nodeid
		{
			unknown,

			lvalue_expression,
			literal_expression,
			unary_expression,
			binary_expression,
			intrinsic_expression,
			conditional_expression,
			assignment_expression,
			expression_sequence,
			call_expression,
			constructor_expression,
			swizzle_expression,
			field_expression,
			initializer_list,

			compound_statement,
			expression_statement,
			if_statement,
			switch_statement,
			case_statement,
			for_statement,
			while_statement,
			return_statement,
			jump_statement,

			annotation,
			declarator_list,
			variable_declaration,
			struct_declaration,
			function_declaration,
			pass_declaration,
			technique_declaration,
		};
		class node abstract
		{
			void operator=(const node &);

		public:
			const nodeid id;
			location location;
			
		protected:
			explicit node(nodeid id) : id(id), location() { }
		};
		class nodetree
		{
		public:
			template <typename T>
			T *make_node(const location &location)
			{
				const auto node = _pool.add<T>();
				node->location = location;

				return node;
			}

			std::vector<node *> structs, uniforms, functions, techniques;

		private:
			class memory_pool
			{
			public:
				~memory_pool()
				{
					clear();
				}

				template <typename T>
				T *add()
				{
					auto size = sizeof(nodeinfo) - sizeof(node) + sizeof(T);
					auto page = std::find_if(_pages.begin(), _pages.end(),
						[size](const struct page &page)
						{
							return page.cursor + size < page.memory.size();
						});

					if (page == _pages.end())
					{
						_pages.emplace_back(std::max(size_t(4096), size));

						page = std::prev(_pages.end());
					}

					const auto node = new (&page->memory.at(page->cursor)) nodeinfo;
					const auto node_data = new (&node->data) T();
					node->size = size;
					node->dtor = [](void *object) { reinterpret_cast<T *>(object)->~T(); };

					page->cursor += node->size;

					return node_data;
				}
				void clear()
				{
					for (auto &page : _pages)
					{
						auto node = reinterpret_cast<nodeinfo *>(&page.memory.front());

						do
						{
							node->dtor(node->data);
							node = reinterpret_cast<nodeinfo *>(reinterpret_cast<unsigned char *>(node) + node->size);
						}
						while (node->size > 0 && (page.cursor -= node->size) > 0);
					}
				}

			private:
				struct page
				{
					explicit page(size_t size) : cursor(0), memory(size, '\0') { }

					size_t cursor;
					std::vector<unsigned char> memory;
				};
				struct nodeinfo
				{
					size_t size;
					void(*dtor)(void *);
					unsigned char data[sizeof(node)];
				};

				std::list<page> _pages;
			} _pool;
		};

		namespace nodes
		{
			struct type_node
			{
				enum datatype
				{
					datatype_void,
					datatype_bool,
					datatype_int,
					datatype_uint,
					datatype_float,
					datatype_string,
					datatype_sampler,
					datatype_texture,
					datatype_struct,
				};
				enum qualifier : unsigned int
				{
					// Storage
					qualifier_extern = 1 << 0,
					qualifier_static = 1 << 1,
					qualifier_uniform = 1 << 2,
					qualifier_volatile = 1 << 3,
					qualifier_precise = 1 << 4,
					qualifier_in = 1 << 5,
					qualifier_out = 1 << 6,
					qualifier_inout = qualifier_in | qualifier_out,

					// Modifier
					qualifier_const = 1 << 8,

					// Interpolation
					qualifier_linear = 1 << 10,
					qualifier_noperspective = 1 << 11,
					qualifier_centroid = 1 << 12,
					qualifier_nointerpolation = 1 << 13,
				};

				inline bool is_array() const { return array_length != 0; }
				inline bool is_matrix() const { return rows >= 1 && cols > 1; }
				inline bool is_vector() const { return rows > 1 && !is_matrix(); }
				inline bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
				inline bool is_numeric() const { return is_boolean() || is_integral() || is_floating_point(); }
				inline bool is_void() const { return basetype == datatype_void; }
				inline bool is_boolean() const { return basetype == datatype_bool; }
				inline bool is_integral() const { return basetype == datatype_int || basetype == datatype_uint; }
				inline bool is_floating_point() const { return basetype == datatype_float; }
				inline bool is_texture() const { return basetype == datatype_texture; }
				inline bool is_sampler() const { return basetype == datatype_sampler; }
				inline bool is_struct() const { return basetype == datatype_struct; }
				inline bool has_qualifier(qualifier qualifier) const { return (qualifiers & qualifier) == qualifier; }

				datatype basetype;
				unsigned int qualifiers;
				unsigned int rows : 4, cols : 4;
				int array_length;
				struct struct_declaration_node *definition;
			};
	
			struct expression_node abstract : public node
			{
				type_node type;

			protected:
				expression_node(nodeid id) : node(id) { }
			};
			struct statement_node abstract : public node
			{
				std::vector<std::string> attributes;

			protected:
				statement_node(nodeid id) : node(id) { }
			};
			struct declaration_node abstract : public node
			{
				std::string name, Namespace;

			protected:
				declaration_node(nodeid id) : node(id) { }
			};

			// Expressions
			struct lvalue_expression_node : public expression_node
			{
				const struct variable_declaration_node *reference;

				lvalue_expression_node() : expression_node(nodeid::lvalue_expression) { }
			};
			struct literal_expression_node : public expression_node
			{
				union
				{
					int value_int[16];
					unsigned int value_uint[16];
					float value_float[16];
				};

				std::string value_string;

				literal_expression_node() : expression_node(nodeid::literal_expression) { }
			};
			struct unary_expression_node : public expression_node
			{
				enum op
				{
					none,
					negate,
					bitwise_not,
					logical_not,
					pre_increase,
					pre_decrease,
					post_increase,
					post_decrease,
					cast,
				};

				op op;
				expression_node *operand;

				unary_expression_node() : expression_node(nodeid::unary_expression) { }
			};
			struct binary_expression_node : public expression_node
			{
				enum op
				{
					none,
					add,
					subtract,
					multiply,
					divide,
					modulo,
					less,
					greater,
					less_equal,
					greater_equal,
					equal,
					not_equal,
					left_shift,
					right_shift,
					bitwise_or,
					bitwise_xor,
					bitwise_and,
					logical_or,
					logical_and,
					element_extract
				};

				op op;
				expression_node *operands[2];

				binary_expression_node() : expression_node(nodeid::binary_expression) { }
			};
			struct intrinsic_expression_node : public expression_node
			{
				enum op
				{
					none,
					abs,
					acos,
					all,
					any,
					bitcast_int2float,
					bitcast_uint2float,
					asin,
					bitcast_float2int,
					bitcast_float2uint,
					atan,
					atan2,
					ceil,
					clamp,
					cos,
					cosh,
					cross,
					ddx,
					ddy,
					degrees,
					determinant,
					distance,
					dot,
					exp,
					exp2,
					faceforward,
					floor,
					frac,
					frexp,
					fwidth,
					ldexp,
					length,
					lerp,
					log,
					log10,
					log2,
					mad,
					max,
					min,
					modf,
					mul,
					normalize,
					pow,
					radians,
					rcp,
					reflect,
					refract,
					round,
					rsqrt,
					saturate,
					sign,
					sin,
					sincos,
					sinh,
					smoothstep,
					sqrt,
					step,
					tan,
					tanh,
					tex2d,
					tex2dfetch,
					tex2dgather,
					tex2dgatheroffset,
					tex2dgrad,
					tex2dlevel,
					tex2dleveloffset,
					tex2doffset,
					tex2dproj,
					tex2dsize,
					transpose,
					trunc,
				};

				op op;
				expression_node *arguments[4];

				intrinsic_expression_node() : expression_node(nodeid::intrinsic_expression) { }
			};
			struct conditional_expression_node : public expression_node
			{
				expression_node *condition;
				expression_node *expression_when_true, *expression_when_false;

				conditional_expression_node() : expression_node(nodeid::conditional_expression) { }
			};
			struct assignment_expression_node : public expression_node
			{
				enum op
				{
					none,
					add,
					subtract,
					multiply,
					divide,
					modulo,
					bitwise_and,
					bitwise_or,
					bitwise_xor,
					left_shift,
					right_shift
				};

				op op;
				expression_node *left, *right;

				assignment_expression_node() : expression_node(nodeid::assignment_expression) { }
			};
			struct expression_sequence_node : public expression_node
			{
				std::vector<expression_node *> expression_list;

				expression_sequence_node() : expression_node(nodeid::expression_sequence) { }
			};
			struct call_expression_node : public expression_node
			{
				std::string callee_name;
				const struct function_declaration_node *callee;
				std::vector<expression_node *> arguments;

				call_expression_node() : expression_node(nodeid::call_expression) { }
			};
			struct constructor_expression_node : public expression_node
			{
				std::vector<expression_node *> arguments;

				constructor_expression_node() : expression_node(nodeid::constructor_expression) { }
			};
			struct swizzle_expression_node : public expression_node
			{
				expression_node *operand;
				signed char mask[4];

				swizzle_expression_node() : expression_node(nodeid::swizzle_expression) { }
			};
			struct field_expression_node : public expression_node
			{
				expression_node *operand;
				variable_declaration_node *field_reference;

				field_expression_node() : expression_node(nodeid::field_expression) { }
			};
			struct initializer_list_node : public expression_node
			{
				std::vector<expression_node *> values;

				initializer_list_node() : expression_node(nodeid::initializer_list) { }
			};

			// Statements
			struct compound_statement_node : public statement_node
			{
				std::vector<statement_node *> statement_list;

				compound_statement_node() : statement_node(nodeid::compound_statement) { }
			};
			struct expression_statement_node : public statement_node
			{
				expression_node *expression;

				expression_statement_node() : statement_node(nodeid::expression_statement) { }
			};
			struct if_statement_node : public statement_node
			{
				expression_node *condition;
				statement_node *statement_when_true, *statement_when_false;

				if_statement_node() : statement_node(nodeid::if_statement) { }
			};
			struct case_statement_node : public statement_node
			{
				std::vector<struct literal_expression_node *> labels;
				statement_node *statement_list;

				case_statement_node() : statement_node(nodeid::case_statement) { }
			};
			struct switch_statement_node : public statement_node
			{
				expression_node *test_expression;
				std::vector<case_statement_node *> case_list;

				switch_statement_node() : statement_node(nodeid::switch_statement) { }
			};
			struct for_statement_node : public statement_node
			{
				statement_node *init_statement;
				expression_node *condition, *increment_expression;
				statement_node *statement_list;

				for_statement_node() : statement_node(nodeid::for_statement) { }
			};
			struct while_statement_node : public statement_node
			{
				bool is_do_while;
				expression_node *condition;
				statement_node *statement_list;

				while_statement_node() : statement_node(nodeid::while_statement) { }
			};
			struct return_statement_node : public statement_node
			{
				bool is_discard;
				expression_node *return_value;

				return_statement_node() : statement_node(nodeid::return_statement) { }
			};
			struct jump_statement_node : public statement_node
			{
				bool is_break, is_continue;

				jump_statement_node() : statement_node(nodeid::jump_statement) { }
			};

			// Declarations
			struct annotation_node : public node
			{
				std::string name;
				literal_expression_node *value;

				annotation_node() : node(nodeid::annotation) { }
			};
			struct declarator_list_node : public statement_node
			{
				std::vector<struct variable_declaration_node *> declarator_list;

				declarator_list_node() : statement_node(nodeid::declarator_list) { }
			};
			struct variable_declaration_node : public declaration_node
			{
				struct properties
				{
					enum : unsigned int
					{
						NONE = 0,

						R8 = 50,
						R16F = 111,
						R32F = 114,
						RG8 = 51,
						RG16 = 34,
						RG16F = 112,
						RG32F = 115,
						RGBA8 = 32,
						RGBA16 = 36,
						RGBA16F = 113,
						RGBA32F = 116,
						DXT1 = 827611204,
						DXT3 = 861165636,
						DXT5 = 894720068,
						LATC1 = 826889281,
						LATC2 = 843666497,

						POINT = 1,
						LINEAR,
						ANISOTROPIC,

						WRAP = 1,
						REPEAT = 1,
						MIRROR,
						CLAMP,
						BORDER,
					};

					properties() : Texture(nullptr), Width(1), Height(1), Depth(1), MipLevels(1), Format(RGBA8), SRGBTexture(false), AddressU(CLAMP), AddressV(CLAMP), AddressW(CLAMP), MinFilter(LINEAR), MagFilter(LINEAR), MipFilter(LINEAR), MaxAnisotropy(1), MinLOD(0), MaxLOD(FLT_MAX), MipLODBias(0.0f)
					{
					}

					const variable_declaration_node *Texture;
					unsigned int Width, Height, Depth, MipLevels;
					unsigned int Format;
					bool SRGBTexture;
					unsigned int AddressU, AddressV, AddressW, MinFilter, MagFilter, MipFilter;
					unsigned int MaxAnisotropy;
					float MinLOD, MaxLOD, MipLODBias;
				};

				type_node type;
				std::vector<annotation_node> annotations;
				std::string semantic;
				properties properties;
				expression_node *initializer_expression;

				variable_declaration_node() : declaration_node(nodeid::variable_declaration) { }
			};
			struct struct_declaration_node : public declaration_node
			{
				std::vector<variable_declaration_node *> field_list;

				struct_declaration_node() : declaration_node(nodeid::struct_declaration) { }
			};
			struct function_declaration_node : public declaration_node
			{
				type_node return_type;
				std::vector<variable_declaration_node *> parameter_list;
				std::string return_semantic;
				compound_statement_node *definition;

				function_declaration_node() : declaration_node(nodeid::function_declaration) { }
			};
			struct pass_declaration_node : public declaration_node
			{
				struct states
				{
					enum : unsigned int
					{
						NONE = 0,

						ZERO = 0,
						ONE = 1,
						SRCCOLOR,
						INVSRCCOLOR,
						SRCALPHA,
						INVSRCALPHA,
						DESTALPHA,
						INVDESTALPHA,
						DESTCOLOR,
						INVDESTCOLOR,

						ADD = 1,
						SUBTRACT,
						REVSUBTRACT,
						MIN,
						MAX,

						KEEP = 1,
						REPLACE = 3,
						INVERT,
						INCRSAT,
						DECRSAT,
						INCR,
						DECR,

						NEVER = 1,
						LESS,
						EQUAL,
						LESSEQUAL,
						GREATER,
						NOTEQUAL,
						GREATEREQUAL,
						ALWAYS
					};

					states() : RenderTargets(), VertexShader(nullptr), PixelShader(nullptr), SRGBWriteEnable(false), BlendEnable(false), DepthEnable(false), StencilEnable(false), RenderTargetWriteMask(0xF), DepthWriteMask(1), StencilReadMask(0xFF), StencilWriteMask(0xFF), BlendOp(ADD), BlendOpAlpha(ADD), SrcBlend(ONE), DestBlend(ZERO), DepthFunc(LESS), StencilFunc(ALWAYS), StencilRef(0), StencilOpPass(KEEP), StencilOpFail(KEEP), StencilOpDepthFail(KEEP)
					{
					}

					const variable_declaration_node *RenderTargets[8];
					const function_declaration_node *VertexShader, *PixelShader;
					bool SRGBWriteEnable, BlendEnable, DepthEnable, StencilEnable;
					unsigned char RenderTargetWriteMask, DepthWriteMask, StencilReadMask, StencilWriteMask;
					unsigned int BlendOp, BlendOpAlpha, SrcBlend, DestBlend, DepthFunc, StencilFunc, StencilRef, StencilOpPass, StencilOpFail, StencilOpDepthFail;
				};

				std::vector<annotation_node> annotation_list;
				states states;

				pass_declaration_node() : declaration_node(nodeid::pass_declaration) { }
			};
			struct technique_declaration_node : public declaration_node
			{
				std::vector<annotation_node> annotation_list;
				std::vector<pass_declaration_node *> pass_list;

				technique_declaration_node() : declaration_node(nodeid::technique_declaration) { }
			};
		}
	}
}
