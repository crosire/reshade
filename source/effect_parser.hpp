/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <memory>
#include "effect_lexer.hpp"
#include "variant.hpp"
#include "source_location.hpp"
#include "runtime_objects.hpp"
#include <spirv.hpp>
#include <algorithm>
#include <assert.h>

namespace reshadefx
{
	struct spv_node
	{
		spv::Op op = spv::OpNop;
		spv::Id result = 0;
		spv::Id result_type = 0;
		std::vector<spv::Id> operands;
		size_t index = 0;
		location location = {};

		explicit spv_node(spv::Op op = spv::OpNop) : op(op) { }
		explicit spv_node(spv::Op op, spv::Id result) : op(op), result_type(result) { }
		explicit spv_node(spv::Op op, spv::Id type, spv::Id result) : op(op), result_type(type), result(result) { }

		spv_node &add(spv::Id operand)
		{
			operands.push_back(operand);
			return *this;
		}
		spv_node &add_string(const char *str)
		{
			uint32_t word = 0;

			while (*str || word & 0xFF000000)
			{
				word = 0;

				for (uint32_t i = 0; i < 4 && *str; ++i)
				{
					reinterpret_cast<uint8_t *>(&word)[i] = *str++;
				}

				add(word);
			}

			return *this;
		}
	};
	struct spv_section
	{
		std::vector<spv_node> instructions;
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

	struct type_info
	{
		static unsigned int rank(const type_info &src, const type_info &dst);

		bool has(qualifier qualifier) const { return (qualifiers & qualifier) == qualifier; }
		bool is_array() const { return array_length != 0; }
		bool is_scalar() const { return !is_array() && !is_matrix() && !is_vector() && is_numeric(); }
		bool is_vector() const { return rows > 1 && cols == 1; }
		bool is_matrix() const { return rows >= 1 && cols > 1; }
		bool is_numeric() const { return is_boolean() || is_integral() || is_floating_point(); }
		bool is_void() const { return base == spv::OpTypeVoid; }
		bool is_boolean() const { return base == spv::OpTypeBool; }
		bool is_integral() const { return base == spv::OpTypeInt; }
		bool is_floating_point() const { return base == spv::OpTypeFloat; }
		bool is_struct() const { return base == spv::OpTypeStruct; }
		bool is_texture() const { return base == spv::OpTypeImage; }
		bool is_sampler() const { return base == spv::OpTypeSampledImage; }

		unsigned int components() const { return rows * cols; }

		spv::Op base;
		unsigned int size : 8;
		unsigned int rows : 4;
		unsigned int cols : 4;
		bool is_signed = false;
		bool is_pointer = false;
		unsigned int qualifiers = 0;
		int array_length = 0;
		spv::Id definition = 0;
		spv::Id array_length_expression = 0;
	};

	inline bool operator==(const type_info &lhs, const type_info &rhs)
	{
		return lhs.base == rhs.base && lhs.size == rhs.size && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.is_signed == rhs.is_signed && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition && lhs.is_pointer == rhs.is_pointer;
	}

	struct struct_info
	{
		std::vector<std::pair<std::string, type_info>> field_list;
	};
	struct function_info
	{
		type_info return_type;
		std::string name;
		std::string unique_name;
		std::vector<type_info> parameter_list;
		std::string return_semantic;
		spv::Id definition;
		spv_section variables;
		spv_section code;
	};
	struct variable_info
	{
		std::unordered_map<std::string, reshade::variant> annotation_list;
		spv::Id texture;
		unsigned int width = 1, height = 1, depth = 1, levels = 1;
		bool srgb_texture;
		reshade::texture_format format = reshade::texture_format::rgba8;
		reshade::texture_filter filter = reshade::texture_filter::min_mag_mip_linear;
		reshade::texture_address_mode address_u = reshade::texture_address_mode::clamp;
		reshade::texture_address_mode address_v = reshade::texture_address_mode::clamp;
		reshade::texture_address_mode address_w = reshade::texture_address_mode::clamp;
		float min_lod, max_lod = FLT_MAX, lod_bias;
	};

	struct pass_properties
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
			INCRSAT,
			DECRSAT,
			INVERT,
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

		location location;
		std::string name;
		std::unordered_map<std::string, reshade::variant> annotation_list;
		spv::Id render_targets[8] = {};
		spv::Id vertex_shader = 0, pixel_shader = 0;
		bool clear_render_targets = true, srgb_write_enable, blend_enable, stencil_enable;
		unsigned char color_write_mask = 0xF, stencil_read_mask = 0xFF, stencil_write_mask = 0xFF;
		unsigned int blend_op = ADD, blend_op_alpha = ADD, src_blend = ONE, dest_blend = ZERO, src_blend_alpha = ONE, dest_blend_alpha = ZERO;
		unsigned int stencil_comparison_func = ALWAYS, stencil_reference_value, stencil_op_pass = KEEP, stencil_op_fail = KEEP, stencil_op_depth_fail = KEEP;
	};
	struct technique_properties
	{
		location location;
		std::string name, unique_name;
		std::unordered_map<std::string, reshade::variant> annotation_list;
		std::vector<pass_properties> pass_list;
	};


	/// <summary>
	/// A parser for the ReShade FX language.
	/// </summary>
	class parser
	{
	public:
		/// <summary>
		/// Construct a new parser instance.
		/// </summary>
		parser();
		parser(const parser &) = delete;
		~parser();

		parser &operator=(const parser &) = delete;

		/// <summary>
		/// Gets the list of error messages.
		/// </summary>
		const std::string &errors() const { return _errors; }

		/// <summary>
		/// Parse the provided input string.
		/// </summary>
		/// <param name="source">The string to analyze.</param>
		/// <returns>A boolean value indicating whether parsing was successful or not.</returns>
		bool run(const std::string &source);

	private:
		void error(const location &location, unsigned int code, const std::string &message);
		void warning(const location &location, unsigned int code, const std::string &message);

		void backup();
		void restore();

		bool peek(char tok) const { return peek(static_cast<tokenid>(tok)); }
		bool peek(tokenid tokid) const;
		bool peek_multary_op(spv::Op &op, unsigned int &precedence) const;
		void consume();
		void consume_until(char tok) { return consume_until(static_cast<tokenid>(tok)); }
		void consume_until(tokenid tokid);
		bool accept(char tok) { return accept(static_cast<tokenid>(tok)); }
		bool accept(tokenid tokid);
		bool expect(char tok) { return expect(static_cast<tokenid>(tok)); }
		bool expect(tokenid tokid);

		bool accept_type_class(type_info &type);
		bool accept_type_qualifiers(type_info &type);

		bool accept_unary_op(spv::Op &op);
		bool accept_postfix_op(const type_info &type, spv::Op &op);
		bool accept_assignment_op(const type_info &type, spv::Op &op);

		bool parse_top_level();
		bool parse_namespace();
		bool parse_type(type_info &type);

		struct constant
		{
			union
			{
				float as_float[16];
				uint32_t as_uint[16];
			};
			std::string as_string;
		};

		struct access_chain
		{
			struct operation
			{
				type_info from, to;
				bool implicit;
				spv::Id index;
			};
			spv::Id base = 0;
			std::vector<operation> ops;
			signed char swizzle[4];
			type_info type;
			type_info base_type;
			location location;
			constant constant;
			bool is_lvalue = false;
			bool is_constant = false;

			void reset_to_lvalue(spv::Id in_base, const type_info &in_type, const struct location &loc)
			{
				assert(in_type.is_pointer);

				this->base = in_base;
				this->type = base_type = in_type;
				this->type.is_pointer = false;
				this->location = loc;
				ops.clear();
				swizzle[0] = -1;
				swizzle[1] = -1;
				swizzle[2] = -1;
				swizzle[3] = -1;
				is_lvalue = true;
			}
			//void reset_to_lvalue_constant() {}
			void reset_to_rvalue(spv::Id in_base, const type_info &in_type, const struct location &loc)
			{
				this->base = in_base;
				this->type = base_type = in_type;
				this->type.qualifiers |= qualifier_const;
				this->location = loc;
				ops.clear();
				swizzle[0] = -1;
				swizzle[1] = -1;
				swizzle[2] = -1;
				swizzle[3] = -1;
				is_lvalue = false;
			}
			void reset_to_rvalue_constant(const type_info &in_type, const struct location &loc, uint32_t data)
			{
				this->type = base_type = in_type;
				this->type.qualifiers |= qualifier_const;
				this->location = loc;
				ops.clear();
				swizzle[0] = -1;
				swizzle[1] = -1;
				swizzle[2] = -1;
				swizzle[3] = -1;
				is_lvalue = false;
				is_constant = true;
				constant = {};
				constant.as_uint[0] = data;
			}
			void reset_to_rvalue_constant(const type_info &in_type, const struct location &loc, std::string &&data)
			{
				this->type = base_type = in_type;
				this->type.qualifiers |= qualifier_const;
				this->location = loc;
				ops.clear();
				swizzle[0] = -1;
				swizzle[1] = -1;
				swizzle[2] = -1;
				swizzle[3] = -1;
				is_lvalue = false;
				is_constant = true;
				constant = {};
				constant.as_string = data;
			}
			void reset_to_rvalue_constant(const type_info &in_type, const struct location &loc, const struct constant &data)
			{
				this->type = base_type = in_type;
				this->type.qualifiers |= qualifier_const;
				this->location = loc;
				ops.clear();
				swizzle[0] = -1;
				swizzle[1] = -1;
				swizzle[2] = -1;
				swizzle[3] = -1;
				is_lvalue = false;
				is_constant = true;
				constant = data;
			}

			void push_cast(const type_info &in_type, bool implicit)
			{
				if (type == in_type)
					return;
				if (is_constant)
				{
					if (in_type.is_integral())
					{
						assert(!type.is_integral());
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_uint[i] = static_cast<int>(constant.as_float[i]);
					}
					else
					{
						assert(type.is_integral());
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_float[i] = static_cast<float>(static_cast<int>(constant.as_uint[i]));
					}
				}
				else
				{
					ops.push_back({ type, in_type, implicit });
				}

				type = in_type;
				is_lvalue = false;
			}
			void push_static_index(size_t index, parser &p)
			{
				if (!is_constant)
				{
					constant.as_uint[0] = index;
					return push_dynamic_index(p.convert_constant({ spv::OpTypeInt, 32, 1, 1, false }, constant));
				}

				assert(type.is_vector() || type.is_matrix());
				constant.as_uint[0] = constant.as_uint[index];

				if (type.is_array()) // Only single-level arrays are supported right now, so indexing into an array always returns an object that is not an array
					type.array_length = 0;
				else if (type.is_matrix()) // Indexing into a matrix returns a row of it as a vector
					type.rows = type.cols,
					type.cols = 1;
				else if (type.is_vector()) // Indexing into a vector returns the element as a scalar
					type.rows = 1;
			}
			void push_dynamic_index(spv::Id index_expression)
			{
				type_info target_type = type;
				if (type.is_array())
					target_type.array_length = 0;
				else if (type.is_matrix())
					target_type.rows = type.cols,
					target_type.cols = 1;
				else if (type.is_vector())
					target_type.rows = 1;

				ops.push_back({ type, target_type, false, index_expression });

				is_constant = false;
				type = target_type;
				type.is_pointer = false;
			}
			void push_swizzle(signed char swizzle2[4], size_t length)
			{
				if (is_constant)
				{
					assert(false);
				}

				type.rows = static_cast<unsigned int>(length);
				type.cols = 1;
				type.is_pointer = false;
				for (size_t i = 0; i < length && swizzle[i] >= 0; ++i)
					swizzle2[i] = swizzle[swizzle2[i]];
				for (size_t i = 0; i < length; ++i)
					swizzle[i] = swizzle2[i];
				for (size_t i = length; i < 4; ++i)
					swizzle[i] = -1;
			}
		};

		bool parse_expression(spv_section &section, access_chain &expression);
		bool parse_expression_unary(spv_section &section, access_chain &expression);
		bool parse_expression_multary(spv_section &section, access_chain &expression, unsigned int precedence = 0);
		bool parse_expression_assignment(spv_section &section, access_chain &expression);

		bool parse_statement(spv_section &section, bool scoped = true);
		bool parse_statement_block(spv_section &section, bool scoped = true);

		bool parse_array(int &size);
		bool parse_annotations(std::unordered_map<std::string, reshade::variant> &annotations);

		bool parse_struct(spv::Id &structure);
		bool parse_function_declaration(type_info &type, std::string name, spv::Id &function);
		bool parse_variable_declaration(spv_section &section, type_info &type, std::string name, spv::Id &variable, bool global = false);
		bool parse_variable_assignment(spv_section &section, access_chain &expression);
		bool parse_variable_properties(variable_info &props);
		bool parse_variable_properties_expression(access_chain &expression);
		bool parse_technique(technique_properties &technique);
		bool parse_technique_pass(pass_properties &pass);
		bool parse_technique_pass_expression(access_chain &expression);

		spv_section _entries;
		spv_section _strings;
		spv_section _debug;
		spv_section _annotations;
		spv_section _variables;
		spv_section _temporary;

		std::unordered_map<spv::Id, struct_info> _structs;
		std::vector<std::unique_ptr<function_info>> _functions;
		std::vector<technique_properties> techniques;

		std::vector<std::pair<spv_section *, size_t>> _id_lookup;
		std::vector<std::pair<type_info, spv::Id>> _type_lookup;
		std::vector<std::pair<constant, spv::Id>> _constant_lookup;
		std::unordered_map<spv::Op, std::vector<type_info>> _type_lookup2;

		std::vector<spv::Id> _loop_break_target_stack;
		std::vector<spv::Id> _loop_continue_target_stack;


		spv::Id access_chain_load(spv_section &section, const access_chain &chain)
		{
			if (chain.is_constant)
			{
				return convert_constant(chain.type, chain.constant);
			}

			spv::Id result = chain.base;

			if (chain.is_lvalue)
			{
				type_info base_type = chain.base_type;
				base_type.is_pointer = false;
				result = add_node(section, chain.location, spv::OpLoad, convert_type(base_type))
					.add(chain.base) // Pointer
					.result; // Result ID
			}

			for (auto &op : chain.ops)
			{
				if (op.index != 0)
				{
					spv_node &node = add_node(section, chain.location, spv::OpAccessChain, convert_type(op.to))
						.add(result)
						.add(op.index);

					result = node.result;
				}
				else
				{
					result = add_cast_node(section, chain.location, op.from, op.to, result, op.implicit, chain.is_constant);
				}
			}

			if (chain.swizzle[0] >= 0)
			{
				if (chain.type.is_vector())
				{
					spv_node &node = add_node(section, chain.location, spv::OpVectorShuffle, convert_type(chain.type))
						.add(result) // Vector 1
						.add(result); // Vector 2

					for (unsigned int i = 0; i < 4 && chain.swizzle[i] >= 0; ++i)
						node.add(chain.swizzle[i]);

					result = node.result;
				}
				else if (chain.type.is_scalar())
				{
					assert(chain.swizzle[1] < 0);

					spv::Id result2 = add_node(section, chain.location, spv::OpCompositeExtract, convert_type(chain.type))
						.add(result) // Composite
						.add(chain.swizzle[0]) // Index
						.result; // Result ID

					result = result2;
				}
				else
				{
					assert(false);
				}
			}

			return result;
		}
		void access_chain_store(spv_section &section, const access_chain &chain, spv::Id value, const type_info &value_type)
		{
			assert(chain.is_lvalue && !chain.is_constant);
			assert(!value_type.is_pointer);

			for (auto &op : chain.ops)
			{
				assert(false);
			}

			if (chain.swizzle[0] >= 0)
			{
				type_info base_type = chain.base_type;
				base_type.is_pointer = false;
				spv::Id result = add_node(section, chain.location, spv::OpLoad, convert_type(base_type))
					.add(chain.base) // Pointer
					.result; // Result ID

				if (base_type.is_vector() && value_type.is_vector())
				{
					spv_node &node = add_node(section, chain.location, spv::OpVectorShuffle, convert_type(base_type))
						.add(result) // Vector 1
						.add(value); // Vector 2

					unsigned int shuffle[4] = { 0, 1, 2, 3 };
					for (unsigned int i = 0; i < base_type.rows; ++i)
						if (chain.swizzle[i] >= 0)
							shuffle[chain.swizzle[i]] = base_type.rows + i;
					for (unsigned int i = 0; i < base_type.rows; ++i)
						node.add(shuffle[i]);

					value = node.result;
				}
				else if (value_type.is_scalar())
				{
					assert(chain.swizzle[1] < 0); // TODO

					spv::Id result2 = add_node(section, chain.location, spv::OpCompositeInsert, convert_type(base_type))
						.add(value) // Object
						.add(result) // Composite
						.add(chain.swizzle[0]) // Index
						.result; // Result ID

					value = result2;
				}
				else
				{
					assert(false);
				}
			}

			add_node_without_result(section, chain.location, spv::OpStore)
				.add(chain.base)
				.add(value);
		}


		spv::Id _next_id = 102;

		spv::Id make_id()
		{
			_id_lookup.emplace_back();
			return _next_id++;
		}

		spv_node &add_node(spv_section &section, location loc, spv::Op op, spv::Id type = 0)
		{
			spv_node &instruction = add_node_without_result(section, loc, op);
			instruction.result = _next_id++;
			instruction.result_type = type;

			_id_lookup.push_back({ &section, instruction.index });

			return instruction;
		}
		spv_node &add_node(spv_section &section, location loc, spv::Op op, spv::Id type, std::initializer_list<spv::Id> operands)
		{
			spv_node &instruction = add_node_without_result(section, loc, op);
			instruction.result = _next_id++;
			instruction.result_type = type;
			for (size_t i = 0; i < operands.size(); ++i)
				instruction.operands.push_back(operands.begin()[i]);

			_id_lookup.push_back({ &section, instruction.index });

			return instruction;
		}

		spv_node &add_node_without_result(spv_section &section, location loc, spv::Op op)
		{
			switch (op)
			{
			case spv::OpBranch:
			case spv::OpBranchConditional:
			case spv::OpSwitch:
			case spv::OpKill:
			case spv::OpReturn:
			case spv::OpReturnValue:
			case spv::OpUnreachable:
			case spv::OpSelectionMerge:
			case spv::OpLoopMerge:
			case spv::OpLoad:
			case spv::OpStore:
			case spv::OpConvertFToS:
			case spv::OpConvertFToU:
			case spv::OpConvertSToF:
			case spv::OpConvertUToF:
			case spv::OpCompositeConstruct:
			case spv::OpFMul:
			case spv::OpFDiv:
			case spv::OpFMod:
			case spv::OpFAdd:
			case spv::OpFSub:
				//assert(_current_block);
				static spv_node trash;
				trash = spv_node {};
				trash.index = -1;
				if (!_current_block)
				{
					return trash;
				}
				break;
			}

			spv_node &instruction = section.instructions.emplace_back();
			instruction.op = op;
			instruction.index = section.instructions.size() - 1;
			instruction.location = loc;

			return instruction;
		}

		void add_name(spv::Id id, const char *name)
		{
			add_node_without_result(_debug, {}, spv::OpName)
				.add(id)
				.add_string(name);
		}
		void add_member_name(spv::Id id, uint32_t member, const char *name)
		{
			add_node_without_result(_debug, {}, spv::OpMemberName)
				.add(id)
				.add(member)
				.add_string(name);
		}

		void enter_block(spv_section &section, spv::Id id)
		{
			spv_node &instruction = section.instructions.emplace_back();
			instruction.op = spv::OpLabel;
			instruction.index = section.instructions.size() - 1;
			instruction.result = id;
			_current_block = id;
		}
		void leave_block_and_branch(spv_section &section, spv::Id target)
		{
			if (_current_block == 0)
				return;
			// Check if the last instruction is a branch already
			//if (!section.instructions.empty() && section.instructions.back().op == spv::OpBranch)
			//	return;

			add_node_without_result(section, {}, spv::OpBranch)
				.add(target);
			_current_block = 0;
		}
		void leave_block_and_branch_conditional(spv_section &section, spv::Id condition, spv::Id true_target, spv::Id false_target)
		{
			if (_current_block == 0)
				return;

			add_node_without_result(section, {}, spv::OpBranchConditional)
				.add(condition)
				.add(true_target)
				.add(false_target);

			_current_block = 0;
		}

		spv::Id _current_block = 0;

		spv::Id add_cast_node(spv_section &section, location loc, const type_info &from, const type_info &to, spv::Id input, bool implicit, bool constant)
		{
			assert(!to.is_pointer);

			if (to.components() > from.components())
			{
				auto composite_type = to;
				composite_type.base = from.base;
				spv_node &composite_node = add_node(section, loc, constant ? spv::OpConstantComposite : spv::OpCompositeConstruct, convert_type(composite_type));
				for (unsigned int i = 0; i < composite_type.components(); ++i)
					composite_node.add(input);
				input = composite_node.result;
			}
			if (from.components() > to.components())
			{
				if (implicit)
					warning(loc, 3206, "implicit truncation of vector type");

				//signed char swizzle[4] = { -1, -1, -1, -1 };
				//for (unsigned int i = 0; i < rhs.type.rows; ++i)
				//	swizzle[i] = i;
				//from.push_swizzle(swizzle);
				assert(false); // TODO
			}

			if (from.base == to.base)
				return input;

			spv::Id result = input;

			if (from.is_boolean())
			{
				struct constant true_c = {};
				true_c.as_uint[0] = to.is_floating_point() ? 0x3f800000 : 1;
				const spv::Id true_constant = convert_constant(to, true_c);
				const spv::Id false_constant = convert_constant(to, {});

				result = add_node(section, loc, spv::OpSelect, convert_type(to))
					.add(input) // Condition
					.add(true_constant)
					.add(false_constant)
					.result;
			}
			else
			{
				switch (to.base)
				{
				case spv::OpTypeBool:
					result = add_node(section, loc, from.is_floating_point() ? spv::OpFOrdNotEqual : spv::OpINotEqual, convert_type(to))
						.add(input)
						.add(convert_constant(from, {}))
						.result;
					break;
				case spv::OpTypeInt:
					assert(from.is_floating_point());
					result = add_node(section, loc, to.is_signed ? spv::OpConvertFToS : spv::OpConvertFToU, convert_type(to))
						.add(input)
						.result;
					break;
				case spv::OpTypeFloat:
					assert(from.is_integral());
					result = add_node(section, loc, from.is_signed ? spv::OpConvertSToF : spv::OpConvertUToF, convert_type(to))
						.add(input)
						.result;
					break;
				}
			}

			return result;
		}

		spv::Id convert_type(const type_info &info)
		{
			if (auto it = std::find_if(_type_lookup.begin(), _type_lookup.end(), [&info](auto &x) { return x.first == info; }); it != _type_lookup.end())
				return it->second;

			spv::Id type;

			type_info eleminfo = info;

			if (info.is_array())
			{
				eleminfo.array_length = 0;
				eleminfo.array_length_expression = 0;

				spv::Id elemtype = convert_type(eleminfo);

				// TODO: Array stride
				if (info.array_length > 0) // Sized array
				{
					//assert(info.array_lenghth_expression);

					type = add_node(_variables, {}, spv::OpTypeArray)
						.add(elemtype)
						.add(info.array_length_expression)
						.result;
				}
				else // Dynamic array
				{
					type = add_node(_variables, {}, spv::OpTypeRuntimeArray)
						.add(elemtype)
						.result;
				}
			}
			else if (info.is_pointer)
			{
				eleminfo.is_pointer = false;

				spv::Id elemtype = convert_type(eleminfo);

				type = add_node(_variables, {}, spv::OpTypePointer)
					.add(spv::StorageClassFunction)
					.add(elemtype)
					.result;
			}
			else
			{
				if (info.is_vector())
				{
					eleminfo.rows = 1;
					eleminfo.cols = 1;

					const spv::Id elemtype = convert_type(eleminfo);

					type = add_node(_variables, {}, spv::OpTypeVector)
						.add(elemtype)
						.add(info.rows)
						.result;
				}
				else if (info.is_matrix())
				{
					eleminfo.cols = 1;

					const spv::Id elemtype = convert_type(eleminfo);

					type = add_node(_variables, {}, spv::OpTypeMatrix)
						.add(elemtype)
						.add(info.cols)
						.result;
				}
				else
				{
					switch (info.base)
					{
					case spv::OpTypeVoid:
						type = add_node(_variables, {}, spv::OpTypeVoid).result;
						break;
					case spv::OpTypeBool:
						type = add_node(_variables, {}, spv::OpTypeBool).result;
						break;
					case spv::OpTypeFloat:
						type = add_node(_variables, {}, spv::OpTypeFloat)
							.add(info.size)
							.result;
						break;
					case spv::OpTypeInt:
						type = add_node(_variables, {}, spv::OpTypeInt)
							.add(info.size)
							.add(info.is_signed ? 1 : 0)
							.result;
						break;
					case spv::OpTypeStruct:
						assert(info.definition);
						type = info.definition;
						break;
					case spv::OpTypeImage: {
						spv::Id sampled_type = convert_type(type_info { spv::OpTypeFloat, 32, 4, 1, true });
						//assert(info.definition);
						type = add_node(_variables, {}, spv::OpTypeImage)
							.add(sampled_type) // Sampled Type
							.add(spv::Dim2D)
							.add(0) // Not a depth image
							.add(0) // Not an array
							.add(0) // Not multi-sampled
							.add(1) // Will be used with a sampler
							.add(spv::ImageFormatRgba8)
							.result;
						break;
					}
					case spv::OpTypeSampledImage:
						assert(info.definition);
						type = add_node(_variables, {}, spv::OpTypeSampledImage)
							.add(info.definition)
							.result;
						break;
					default:
						return 0;
					}
				}
			}

			_type_lookup.push_back({ info, type });;

			return type;
		}
		spv::Id convert_type(const function_info &info)
		{
			spv::Id return_type = convert_type(info.return_type);
			std::vector<spv::Id> param_types;
			for (const auto &param : info.parameter_list)
				param_types.push_back(convert_type(param));

			spv_node &node = add_node(_variables, {}, spv::OpTypeFunction);
			node.add(return_type);
			for (auto param_type : param_types)
				node.add(param_type);
			return node.result;
		}

		spv::Id convert_constant(const type_info &type, const constant &data)
		{
			if (auto it = std::find_if(_constant_lookup.begin(), _constant_lookup.end(), [&type, &data](auto &x) { return std::memcmp(&x.first.as_uint[0], &data.as_uint[0], sizeof(uint32_t) * 16) == 0; }); it != _constant_lookup.end())
				return it->second; // TODO check type match

			assert(!type.is_pointer);

			spv::Id result = 0;

			if (type.is_matrix())
			{
				spv::Id columns[4] = {};

				for (unsigned int i = 0; i < type.cols; ++i)
				{
					constant column = {};
					for (unsigned int k = 0; k < type.rows; ++k)
						column.as_uint[k] = data.as_uint[i * type.rows + k];
					type_info column_type = type;
					column_type.cols = 1;
					columns[i] = convert_constant(column_type, column);
				}

				spv_node &node = add_node(_variables, {}, spv::OpConstantComposite, convert_type(type));

				for (unsigned int i = 0; i < type.cols; ++i)
					node.add(columns[i]);

				result = node.result;
			}
			else if (type.is_vector())
			{
				spv::Id rows[4] = {};

				for (unsigned int i = 0; i < type.rows; ++i)
				{
					constant scalar = {};
					scalar.as_uint[0] = data.as_uint[i];
					type_info value_type = type;
					value_type.rows = 1;
					rows[i] = convert_constant(value_type, scalar);
				}

				spv_node &node = add_node(_variables, {}, spv::OpConstantComposite, convert_type(type));

				for (unsigned int i = 0; i < type.rows; ++i)
					node.add(rows[i]);

				result = node.result;
			}
			else if (type.is_boolean())
			{
				result = add_node(_variables, {}, data.as_uint[0] ? spv::OpConstantTrue : spv::OpConstantFalse, convert_type(type)).result;
			}
			//else if (data.as_uint[0] == 0)
			//{
			//	result =  add_node(_variables, {}, spv::OpConstantNull, convert_type(type)).result;
			//}
			else
			{
				assert(type.is_scalar());
				result = add_node(_variables, {}, spv::OpConstant, convert_type(type)).add(data.as_uint[0]).result;
			}

			_constant_lookup.push_back({ data, result });

			return result;
		}

		spv_node &lookup_id(spv::Id id)
		{
			assert(id != 0);
			size_t index = _id_lookup[id - 102].second;
			static spv_node trash;
			trash = spv_node {};
			if (index == -1)
				return trash;
			return _id_lookup[id - 102].first->instructions[index];
		}

		std::string _errors;
		std::unique_ptr<lexer> _lexer, _lexer_backup;
		token _token, _token_next, _token_backup;
		std::unique_ptr<class symbol_table> _symbol_table;
	};
}
