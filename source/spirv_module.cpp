/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_lexer.hpp"
#include "spirv_module.hpp"
#include <assert.h>
#include <iostream>
#include <algorithm>

using namespace reshadefx;

inline bool operator==(const reshadefx::spv_type &lhs, const reshadefx::spv_type &rhs)
{
	//return lhs.base == rhs.base && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition && lhs.is_pointer == rhs.is_pointer;
	//return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	return lhs.base == rhs.base && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition && lhs.is_pointer == rhs.is_pointer && (!lhs.is_pointer || (lhs.qualifiers & (spv_type::qualifier_inout | spv_type::qualifier_static | spv_type::qualifier_uniform)) == (rhs.qualifiers & (spv_type::qualifier_inout | spv_type::qualifier_static | spv_type::qualifier_uniform)));
}
inline bool operator==(const reshadefx::spirv_module::function_info2 &lhs, const reshadefx::spirv_module::function_info2 &rhs)
{
	if (lhs.param_types.size() != rhs.param_types.size())
		return false;
	for (size_t i = 0; i < lhs.param_types.size(); ++i)
		if (!(lhs.param_types[i] == rhs.param_types[i]))
			return false;
	return lhs.return_type == rhs.return_type;
}

static inline void write(std::ostream &s, uint32_t word)
{
	s.write(reinterpret_cast<const char *>(&word), 4);;
}
static void write(std::ostream &s, const spv_instruction &ins)
{
	// First word of an instruction:
	// The 16 low-order bits are the opcode
	// The 16 high-order bits are the word count of the instruction
	const uint32_t num_words = 1 + (ins.type != 0) + (ins.result != 0) + ins.operands.size();
	write(s, (num_words << 16) | ins.op);

	// Optional instruction type ID
	if (ins.type != 0) write(s, ins.type);

	// Optional instruction result ID
	if (ins.result != 0) write(s, ins.result);

	// Write out the operands
	for (size_t i = 0; i < ins.operands.size(); ++i)
		write(s, ins.operands[i]);
}

void spirv_module::write_module(std::ostream &s)
{
	// Write SPIRV header info
	write(s, spv::MagicNumber);
	write(s, spv::Version);
	write(s, 20u); // Generator magic number, see https://www.khronos.org/registry/spir-v/api/spir-v.xml
	write(s, _next_id); // Maximum ID
	write(s, 0u); // Reserved for instruction schema

	// All capabilities
	write(s, spv_instruction(spv::OpCapability)
		.add(spv::CapabilityMatrix));
	write(s, spv_instruction(spv::OpCapability)
		.add(spv::CapabilityShader));

	// Optional extension instructions
	write(s, spv_instruction(spv::OpExtInstImport, glsl_ext)
		.add_string("GLSL.std.450")); // Import GLSL extension

	// Single required memory model instruction
	write(s, spv_instruction(spv::OpMemoryModel)
		.add(spv::AddressingModelLogical)
		.add(spv::MemoryModelGLSL450));

	// All entry point declarations
	for (const auto &node : _entries.instructions)
		write(s, node);

	// All debug instructions
	for (const auto &node : _debug_a.instructions)
		write(s, node);
	for (const auto &node : _debug_b.instructions)
		write(s, node);

	// All annotation instructions
	for (const auto &node : _annotations.instructions)
		write(s, node);

	// All type declarations
	for (const auto &node : _types_and_constants.instructions)
		write(s, node);
	for (const auto &node : _variables.instructions)
		write(s, node);

	// All function definitions
	for (const auto &function : _functions2)
	{
		write(s, function.declaration);

		for (const auto &node : function.variables.instructions)
			write(s, node);
		for (const auto &node : function.definition.instructions)
			write(s, node);
	}
}

spv_instruction &spirv_module::add_node(spv_basic_block &section, const location &loc, spv::Op op, spv::Id type)
{
	spv_instruction &instruction = add_node_without_result(section, loc, op);
	instruction.type = type;
	instruction.result = make_id();
	return instruction;
}
spv_instruction &spirv_module::add_node_without_result(spv_basic_block &section, const location &loc, spv::Op op)
{
	spv_instruction &instruction = section.instructions.emplace_back(op);
	instruction.location = loc;
	return instruction;
}

void spirv_module::define_struct(spv::Id id, const char *name, const location &loc, const std::vector<spv::Id> &members)
{
	spv_instruction &node = add_node_without_result(_types_and_constants, loc, spv::OpTypeStruct);
	node.result = id;
	for (spv::Id type : members)
		node.add(type);

	if (name)
		add_node_without_result(_debug_b, {}, spv::OpName)
		.add(node.result)
		.add_string(name);
}
spv::Id spirv_module::define_struct(const char *name, const location &loc, const std::vector<spv::Id> &members)
{
	spv_instruction &node = add_node(_types_and_constants, loc, spv::OpTypeStruct);
	for (spv::Id type : members)
		node.add(type);

	if (name)
		add_node_without_result(_debug_b, {}, spv::OpName)
			.add(node.result)
			.add_string(name);

	return node.result;
}
spv::Id spirv_module::define_function(const char *name, const location &loc, const reshadefx::spv_type &return_type)
{
	auto &function = _functions2.emplace_back();
	function.return_type = return_type;
	spv_instruction &instruction = function.declaration;
	instruction.op = spv::OpFunction;
	instruction.type = convert_type(return_type);
	instruction.result = make_id();
	instruction.location = loc;
	instruction.add(spv::FunctionControlMaskNone);

	_current_function = _functions2.size() - 1;

	if (name)
		add_node_without_result(_debug_b, {}, spv::OpName)
			.add(instruction.result)
			.add_string(name);

	return instruction.result;
}
void spirv_module::define_variable(spv::Id id, const char *name, const location &loc, const reshadefx::spv_type &type, spv::StorageClass storage, spv::Id initializer)
{
	spv_instruction &node = add_node_without_result(storage != spv::StorageClassFunction ? _variables : _functions2[_current_function].variables, loc, spv::OpVariable)
		.add(storage);
	node.type = convert_type(type);
	node.result = id;
	if (initializer)
		node.add(initializer);
	if (name)
		add_node_without_result(_debug_b, {}, spv::OpName)
		.add(node.result)
		.add_string(name);
}
spv::Id spirv_module::define_variable(const char *name, const location &loc, const reshadefx::spv_type &type, spv::StorageClass storage, spv::Id initializer)
{
	spv_instruction &node = add_node(storage != spv::StorageClassFunction ? _variables : _functions2[_current_function].variables, loc, spv::OpVariable, convert_type(type))
		.add(storage);
	if (initializer)
		node.add(initializer);
	if (name)
		add_node_without_result(_debug_b, {}, spv::OpName)
			.add(node.result)
			.add_string(name);
	return node.result;
}
spv::Id spirv_module::define_parameter(const char *name, const location &loc, const reshadefx::spv_type &type)
{
	_functions2[_current_function].param_types.push_back(type);
	spv_instruction &node = add_node(_functions2[_current_function].variables, loc, spv::OpFunctionParameter, convert_type(type));

	add_node_without_result(_debug_b, {}, spv::OpName)
		.add(node.result)
		.add_string(name);

	return node.result;
}

void spirv_module::add_builtin(spv::Id id, spv::BuiltIn builtin)
{
	add_node_without_result(_annotations, {}, spv::OpDecorate)
		.add(id)
		.add(spv::DecorationBuiltIn)
		.add(builtin);
}
void spirv_module::add_decoration(spv::Id id, spv::Decoration decoration, std::initializer_list<uint32_t> values)
{
	spv_instruction &node = add_node_without_result(_annotations, {}, spv::OpDecorate)
		.add(id)
		.add(decoration);

	for (uint32_t value : values)
		node.add(value);
}
void spirv_module::add_member_name(spv::Id id, uint32_t member_index, const char *name)
{
	add_node_without_result(_debug_b, {}, spv::OpMemberName)
		.add(id)
		.add(member_index)
		.add_string(name);
}
void spirv_module::add_member_builtin(spv::Id id, uint32_t member_index, spv::BuiltIn builtin)
{
	add_node_without_result(_annotations, {}, spv::OpMemberDecorate)
		.add(id)
		.add(member_index)
		.add(spv::DecorationBuiltIn)
		.add(builtin);
}
void spirv_module::add_member_decoration(spv::Id id, uint32_t member_index, spv::Decoration decoration, std::initializer_list<uint32_t> values)
{
	spv_instruction &node = add_node_without_result(_annotations, {}, spv::OpMemberDecorate)
		.add(id)
		.add(member_index)
		.add(decoration);

	for (uint32_t value : values)
		node.add(value);
}
void spirv_module::add_entry_point(const char *name, spv::Id function, spv::ExecutionModel model, const std::vector<spv::Id> &io)
{
	spv_instruction &node = add_node_without_result(_entries, {}, spv::OpEntryPoint);
	node.add(model);
	node.add(function);
	node.add_string(name);
	for (auto interface : io)
		node.add(interface);
}

void spirv_module::add_cast_operation(spv_expression &chain, const reshadefx::spv_type &in_type)
{
	if (chain.type == in_type)
		return; // There is nothing to do if the expression is already of the target type

	if (chain.is_constant)
	{
		if (in_type.is_integral())
		{
			if (!chain.type.is_integral())
			{
				for (unsigned int i = 0; i < 16; ++i)
					chain.constant.as_uint[i] = static_cast<int>(chain.constant.as_float[i]);
			}
			else
			{
				// int 2 uint
			}
		}
		else
		{
			if (!chain.type.is_integral())
			{
				// Scalar to vector promotion
				assert(chain.type.is_floating_point() && chain.type.is_scalar() && in_type.is_vector());
				for (unsigned int i = 1; i < in_type.components(); ++i)
					chain.constant.as_float[i] = chain.constant.as_float[0];
			}
			else
			{
				if (chain.type.is_scalar())
				{
					const float value = static_cast<float>(static_cast<int>(chain.constant.as_uint[0]));
					for (unsigned int i = 0; i < 16; ++i)
						chain.constant.as_float[i] = value;
				}
				else
				{
					for (unsigned int i = 0; i < 16; ++i)
						chain.constant.as_float[i] = static_cast<float>(static_cast<int>(chain.constant.as_uint[i]));
				}
			}
		}
	}
	else
	{
		chain.ops.push_back({ spv_expression::cast, chain.type, in_type });
	}

	chain.type = in_type;
	//chain.is_lvalue = false; // Can't do this because of 'if (chain.is_lvalue)' check in 'access_chain_load'
}
void spirv_module::add_member_access(spv_expression &chain, size_t index, const spv_type &in_type)
{
	spv_type target_type = in_type;
	target_type.is_pointer = true;

	spv_constant index_c = {};
	index_c.as_uint[0] = index;
	chain.ops.push_back({ spv_expression::index, chain.type, target_type, convert_constant({ spv_type::datatype_uint, 1, 1 }, index_c) });

	chain.is_constant = false;
	chain.type = in_type;
}
void spirv_module::add_static_index_access(spv_expression &chain, size_t index)
{
	if (!chain.is_constant)
	{
		chain.constant.as_uint[0] = index;
		memset(&chain.constant.as_uint[1], 0, sizeof(uint32_t) * 15); // Clear the reset of the constant
		return add_dynamic_index_access(chain, convert_constant({ spv_type::datatype_uint, 1, 1 }, chain.constant));
	}

	if (chain.type.is_array())
	{
		chain.type.array_length = 0;

		chain.constant = chain.constant.as_array[index];
	}
	else if (chain.type.is_matrix()) // Indexing into a matrix returns a row of it as a vector
	{
		chain.type.rows = chain.type.cols,
		chain.type.cols = 1;

		for (unsigned int i = 0; i < 4; ++i)
			chain.constant.as_uint[i] = chain.constant.as_uint[index * 4 + i];
		memset(&chain.constant.as_uint[4], 0, sizeof(uint32_t) * 12); // Clear the reset of the constant
	}
	else if (chain.type.is_vector()) // Indexing into a vector returns the element as a scalar
	{
		chain.type.rows = 1;

		chain.constant.as_uint[0] = chain.constant.as_uint[index];
		memset(&chain.constant.as_uint[1], 0, sizeof(uint32_t) * 15); // Clear the reset of the constant
	}
	else
	{
		assert(false);
	}
}
void spirv_module::add_dynamic_index_access(spv_expression &chain, spv::Id index_expression)
{
	spv_type target_type = chain.type;
	if (target_type.is_array())
		target_type.array_length = 0;
	else if (target_type.is_matrix())
		target_type.rows = target_type.cols,
		target_type.cols = 1;
	else if (target_type.is_vector())
		target_type.rows = 1;

	target_type.is_pointer = true; // OpAccessChain returns a pointer

	chain.ops.push_back({ spv_expression::index, chain.type, target_type, index_expression });

	chain.is_constant = false;
	chain.type = target_type;
	chain.type.is_pointer = false;
}
void spirv_module::add_swizzle_access(spv_expression &chain, signed char in_swizzle[4], size_t length)
{
	// Indexing logic is simpler, so use that when possible
	if (length == 1 && chain.type.is_vector())
		return add_static_index_access(chain, in_swizzle[0]);

	spv_type target_type = chain.type;
	target_type.rows = static_cast<unsigned int>(length);
	target_type.cols = 1;

	if (chain.is_constant)
	{
		uint32_t data[16];
		memcpy(data, &chain.constant.as_uint[0], sizeof(data));
		for (size_t i = 0; i < length; ++i)
			chain.constant.as_uint[i] = data[in_swizzle[i]];
		memset(&chain.constant.as_uint[length], 0, sizeof(uint32_t) * (16 - length)); // Clear the reset of the constant
	}
	else
	{
		chain.ops.push_back({ spv_expression::swizzle, chain.type, target_type, 0, { in_swizzle[0], in_swizzle[1], in_swizzle[2], in_swizzle[3] } });
	}

	chain.type = target_type;
	chain.type.is_pointer = false;
}

void spirv_module::enter_block(spv_basic_block &section, spv::Id id)
{
	assert(_current_block == 0);
	assert(_current_function != std::numeric_limits<size_t>::max());

	_current_block = id;

	spv_instruction &instruction = add_node_without_result(section, {}, spv::OpLabel);
	instruction.result = id;
}
void spirv_module::leave_block_and_kill(spv_basic_block &section)
{
	assert(_current_function != std::numeric_limits<size_t>::max());
	if (_current_block == 0) // Might already have left
		return;
	add_node_without_result(section, {}, spv::OpKill);
	_current_block = 0;
}
void spirv_module::leave_block_and_return(spv_basic_block &section, spv::Id value)
{
	assert(_current_function != std::numeric_limits<size_t>::max());
	if (_current_block == 0) // Might already have left
		return;
	if (_functions2[_current_function].return_type.is_void())
	{
		add_node_without_result(section, {}, spv::OpReturn);
	}
	else
	{
		if (value == 0)
		{
			value = add_node(_types_and_constants, {}, spv::OpUndef, convert_type(_functions2[_current_function].return_type)).result;
		}

		add_node_without_result(section, {}, spv::OpReturnValue)
			.add(value);
	}
	_current_block = 0;
}
void spirv_module::leave_block_and_branch(spv_basic_block &section, spv::Id target)
{
	assert(_current_function != std::numeric_limits<size_t>::max());
	if (_current_block == 0)
		return;
	add_node_without_result(section, {}, spv::OpBranch)
		.add(target);
	_current_block = 0;
}
void spirv_module::leave_block_and_branch_conditional(spv_basic_block &section, spv::Id condition, spv::Id true_target, spv::Id false_target)
{
	assert(_current_function != std::numeric_limits<size_t>::max());
	if (_current_block == 0)
		return;
	add_node_without_result(section, {}, spv::OpBranchConditional)
		.add(condition)
		.add(true_target)
		.add(false_target);
	_current_block = 0;
}
void spirv_module::leave_function()
{
	assert(_current_function != std::numeric_limits<size_t>::max());
	auto &function = _functions2[_current_function];

	add_node_without_result(function.definition, {}, spv::OpFunctionEnd);

	function.declaration.add(convert_type(function)); // Function Type

	_current_function = std::numeric_limits<size_t>::max();
}

spv::Id spirv_module::convert_type(const reshadefx::spv_type &info)
{
	if (auto it = std::find_if(_type_lookup.begin(), _type_lookup.end(), [&info](auto &x) { return x.first == info; }); it != _type_lookup.end())
		return it->second;

	spv::Id type;

	spv_type eleminfo = info;

	if (info.is_pointer)
	{
		eleminfo.is_pointer = false;

		spv::Id elemtype = convert_type(eleminfo);

		spv::StorageClass storage = spv::StorageClassFunction;
		if (info.has(spv_type::qualifier_in) && info.has_semantic)
			storage = spv::StorageClassInput;
		if (info.has(spv_type::qualifier_out) && info.has_semantic)
			storage = spv::StorageClassOutput;
		if (info.has(spv_type::qualifier_static))
			storage = spv::StorageClassPrivate;
		if (info.has(spv_type::qualifier_uniform))
			storage = spv::StorageClassUniform;

		type = add_node(_types_and_constants, {}, spv::OpTypePointer)
			.add(storage)
			.add(elemtype)
			.result;
	}
	else if (info.is_array())
	{
		assert(!info.is_pointer);
		eleminfo.array_length = 0;

		spv::Id elemtype = convert_type(eleminfo);

		// TODO: Array stride
		if (info.array_length > 0) // Sized array
		{
			spv_constant array_length = {};
			array_length.as_uint[0] = info.array_length;
			spv::Id array_length_id = convert_constant({ spv_type::datatype_uint, 1, 1 }, array_length);
			type = add_node(_types_and_constants, {}, spv::OpTypeArray)
				.add(elemtype)
				.add(array_length_id)
				.result;
		}
		else // Dynamic array
		{
			type = add_node(_types_and_constants, {}, spv::OpTypeRuntimeArray)
				.add(elemtype)
				.result;
		}
	}
	else if (info.is_matrix())
	{
		// Convert MxN matrix to a SPIR-V matrix with M vectors with N elements
		eleminfo.rows = info.cols;
		eleminfo.cols = 1;

		const spv::Id elemtype = convert_type(eleminfo);

		type = add_node(_types_and_constants, {}, spv::OpTypeMatrix)
			.add(elemtype)
			.add(info.rows)
			.result;
	}
	else if (info.is_vector())
	{
		eleminfo.rows = 1;
		eleminfo.cols = 1;

		const spv::Id elemtype = convert_type(eleminfo);

		type = add_node(_types_and_constants, {}, spv::OpTypeVector)
			.add(elemtype)
			.add(info.rows)
			.result;
	}
	else
	{
		switch (info.base)
		{
		case spv_type::datatype_void:
			assert(info.rows == 0 && info.cols == 0);
			type = add_node(_types_and_constants, {}, spv::OpTypeVoid).result;
			break;
		case spv_type::datatype_bool:
			assert(info.rows == 1 && info.cols == 1);
			type = add_node(_types_and_constants, {}, spv::OpTypeBool).result;
			break;
		case spv_type::datatype_float:
			assert(info.rows == 1 && info.cols == 1);
			type = add_node(_types_and_constants, {}, spv::OpTypeFloat)
				.add(32)
				.result;
			break;
		case spv_type::datatype_int:
			assert(info.rows == 1 && info.cols == 1);
			type = add_node(_types_and_constants, {}, spv::OpTypeInt)
				.add(32)
				.add(1)
				.result;
			break;
		case spv_type::datatype_uint:
			assert(info.rows == 1 && info.cols == 1);
			type = add_node(_types_and_constants, {}, spv::OpTypeInt)
				.add(32)
				.add(0)
				.result;
			break;
		case spv_type::datatype_struct:
			assert(info.definition != 0 && info.rows == 0 && info.cols == 0);
			type = info.definition;
			break;
		case spv_type::datatype_texture: {
			assert(info.rows == 0 && info.cols == 0);
			spv::Id sampled_type = convert_type({ spv_type::datatype_float, 1, 1 });
			type = add_node(_types_and_constants, {}, spv::OpTypeImage)
				.add(sampled_type) // Sampled Type
				.add(spv::Dim2D)
				.add(0) // Not a depth image
				.add(0) // Not an array
				.add(0) // Not multi-sampled
				.add(1) // Will be used with a sampler
				.add(spv::ImageFormatUnknown)
				.result;
			break;
		}
		case spv_type::datatype_sampler: {
			assert(info.rows == 0 && info.cols == 0);
			spv::Id image_type = convert_type({ spv_type::datatype_texture });
			type = add_node(_types_and_constants, {}, spv::OpTypeSampledImage)
				.add(image_type)
				.result;
			break;
		}
		default:
			assert(false);
			return 0;
		}
	}

	spv_type type2 = info;
	type2.has_semantic = false;
	_type_lookup.push_back({ type2, type });;

	return type;
}
spv::Id spirv_module::convert_type(const function_info2 &info)
{
	if (auto it = std::find_if(_function_type_lookup.begin(), _function_type_lookup.end(), [&info](auto &x) { return x.first == info; }); it != _function_type_lookup.end())
		return it->second;

	spv::Id return_type = convert_type(info.return_type);
	assert(return_type != 0);
	std::vector<spv::Id> param_type_ids;
	for (auto param : info.param_types)
		param_type_ids.push_back(convert_type(param));

	spv_instruction &node = add_node(_types_and_constants, {}, spv::OpTypeFunction);
	node.add(return_type);
	for (auto param_type : param_type_ids)
		node.add(param_type);

	_function_type_lookup.push_back({ info, node.result });;

	return node.result;
}

spv::Id spirv_module::convert_constant(const reshadefx::spv_type &type, const spv_constant &data)
{
	assert(!type.is_pointer);

	if (auto it = std::find_if(_constant_lookup.begin(), _constant_lookup.end(), [&type, &data](auto &x) {
		return std::get<0>(x) == type && std::memcmp(&std::get<1>(x).as_uint[0], &data.as_uint[0], sizeof(uint32_t) * 16) == 0;
	}); it != _constant_lookup.end())
		return std::get<2>(*it);

	spv::Id result = 0;

	if (type.is_array())
	{
		std::vector<spv::Id> elements;

		spv_type elem_type = type;
		elem_type.array_length = 0;

		for (const spv_constant &elem : data.as_array)
		{
			elements.push_back(convert_constant(elem_type, elem));
		}

		spv_instruction &node = add_node(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

		for (spv::Id elem : elements)
			node.add(elem);

		result = node.result;
	}
	else if (type.is_struct())
	{
		result = add_node(_types_and_constants, {}, spv::OpConstantNull, convert_type(type)).result;
	}
	else if (type.is_matrix())
	{
		spv::Id rows[4] = {};

		for (unsigned int i = 0; i < type.rows; ++i)
		{
			spv_type row_type = type;
			row_type.rows = type.cols;
			row_type.cols = 1;
			spv_constant row_data = {};
			for (unsigned int k = 0; k < type.cols; ++k)
				row_data.as_uint[k] = data.as_uint[i * type.cols + k];

			rows[i] = convert_constant(row_type, row_data);
		}

		spv_instruction &node = add_node(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

		for (unsigned int i = 0; i < type.rows; ++i)
			node.add(rows[i]);

		result = node.result;
	}
	else if (type.is_vector())
	{
		spv::Id rows[4] = {};

		for (unsigned int i = 0; i < type.rows; ++i)
		{
			spv_type scalar_type = type;
			scalar_type.rows = 1;
			spv_constant scalar_data = {};
			scalar_data.as_uint[0] = data.as_uint[i];

			rows[i] = convert_constant(scalar_type, scalar_data);
		}

		spv_instruction &node = add_node(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

		for (unsigned int i = 0; i < type.rows; ++i)
			node.add(rows[i]);

		result = node.result;
	}
	else if (type.is_boolean())
	{
		result = add_node(_types_and_constants, {}, data.as_uint[0] ? spv::OpConstantTrue : spv::OpConstantFalse, convert_type(type)).result;
	}
	else
	{
		assert(type.is_scalar());
		result = add_node(_types_and_constants, {}, spv::OpConstant, convert_type(type)).add(data.as_uint[0]).result;
	}

	_constant_lookup.push_back({ type, data, result });

	return result;
}

spv::Id spirv_module::access_chain_load(spv_basic_block &section, const spv_expression &chain)
{
	if (chain.is_constant) // Constant expressions do not have a complex access chain
		return convert_constant(chain.type, chain.constant);

	spv::Id result = chain.base;

	size_t op_index = 0;

	// If a variable is referenced, load the value first
	if (chain.is_lvalue)
	{
		auto base_type = chain.type;
		if (!chain.ops.empty())
			base_type = chain.ops[0].from;

		// Any indexing expressions can be resolved during load with an 'OpAccessChain' already
		if (!chain.ops.empty() && chain.ops[0].type == spv_expression::index)
		{
			assert(chain.ops[0].to.is_pointer);
			spv_instruction &node = add_node(section, chain.location, spv::OpAccessChain)
				.add(result); // Base
			do {
				assert(chain.ops[op_index].to.is_pointer);
				base_type = chain.ops[op_index].to;
				node.add(chain.ops[op_index++].index); // Indexes
			} while (op_index < chain.ops.size() && chain.ops[op_index].type == spv_expression::index);
			node.type = convert_type(chain.ops[op_index - 1].to); // Last type is the result
			result = node.result; // Result ID
		}

		base_type.is_pointer = false;

		result = add_node(section, chain.location, spv::OpLoad, convert_type(base_type))
			.add(result) // Pointer
			.result; // Result ID
	}

	// Work through all remaining operations in the access chain and apply them to the value
	for (; op_index < chain.ops.size(); ++op_index)
	{
		const auto &op = chain.ops[op_index];

		switch (op.type)
		{
		case spv_expression::cast:
			assert(!op.to.is_pointer);

			if (op.from.base != op.to.base)
			{
				spv_type from_with_to_base = op.from;
				from_with_to_base.base = op.to.base;

				if (op.from.is_boolean())
				{
					spv_constant true_c = {};
					for (unsigned int i = 0; i < op.to.components(); ++i)
						true_c.as_uint[i] = op.to.is_floating_point() ? 0x3f800000 : 1;
					const spv::Id true_constant = convert_constant(from_with_to_base, true_c);
					const spv::Id false_constant = convert_constant(from_with_to_base, {});

					result = add_node(section, chain.location, spv::OpSelect, convert_type(from_with_to_base))
						.add(result) // Condition
						.add(true_constant)
						.add(false_constant)
						.result;
				}
				else
				{
					switch (op.to.base)
					{
					case spv_type::datatype_bool:
						result = add_node(section, chain.location, op.from.is_floating_point() ? spv::OpFOrdNotEqual : spv::OpINotEqual, convert_type(from_with_to_base))
							.add(result)
							.add(convert_constant(op.from, {}))
							.result;
						break;
					case spv_type::datatype_int:
						assert(op.from.is_floating_point());
						result = add_node(section, chain.location, spv::OpConvertFToS, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					case spv_type::datatype_uint:
						assert(op.from.is_floating_point());
						result = add_node(section, chain.location, spv::OpConvertFToU, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					case spv_type::datatype_float:
						assert(op.from.is_integral());
						result = add_node(section, chain.location, op.from.base == spv_type::datatype_int ? spv::OpConvertSToF : spv::OpConvertUToF, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					}
				}
			}

			if (op.to.components() > op.from.components())
			{
				spv_instruction &composite_node = add_node(section, chain.location, chain.is_constant ? spv::OpConstantComposite : spv::OpCompositeConstruct, convert_type(op.to));
				for (unsigned int i = 0; i < op.to.components(); ++i)
					composite_node.add(result);
				result = composite_node.result;
			}
			if (op.from.components() > op.to.components())
			{
				//signed char swizzle[4] = { -1, -1, -1, -1 };
				//for (unsigned int i = 0; i < rhs.type.rows; ++i)
				//	swizzle[i] = i;
				//from.push_swizzle(swizzle);
				assert(false); // TODO
			}
			break;
		case spv_expression::index:
			if (op.from.is_vector() && op.to.is_scalar())
			{
				spv_type target_type = op.to;
				target_type.is_pointer = false;
				spv_instruction &node = add_node(section, chain.location, spv::OpVectorExtractDynamic, convert_type(target_type))
					.add(result) // Vector
					.add(op.index); // Index

				result = node.result; // Result ID
				break;
			}
			assert(false);
			break;
		case spv_expression::swizzle:
			if (op.to.is_vector())
			{
				if (op.from.is_matrix())
				{
					spv::Id components[4];

					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					{
						const unsigned int row = op.swizzle[i] / 4;
						const unsigned int column = op.swizzle[i] - row * 4;

						spv_type scalar_type = op.to;
						scalar_type.rows = 1;
						scalar_type.cols = 1;

						components[i] = add_node(section, chain.location, spv::OpCompositeExtract, convert_type(scalar_type))
							.add(result)
							.add(row)
							.add(column)
							.result;
					}

					spv_instruction &node = add_node(section, chain.location, spv::OpCompositeConstruct, convert_type(op.to));

					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					{
						node.add(components[i]);
					}

					result = node.result;
					break;
				}
				else
				{
					assert(op.from.is_vector());

					spv_instruction &node = add_node(section, chain.location, spv::OpVectorShuffle, convert_type(chain.type))
						.add(result) // Vector 1
						.add(result); // Vector 2

					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						node.add(op.swizzle[i]);

					result = node.result;
					break;
				}
			}
			else if (op.to.is_scalar())
			{
				assert(op.swizzle[1] < 0);

				spv_instruction &node = add_node(section, chain.location, spv::OpCompositeExtract, convert_type(op.to))
					.add(result); // Composite

				if (op.from.is_matrix())
				{
					const unsigned int row = op.swizzle[0] / 4;
					const unsigned int column = op.swizzle[0] - row * 4;
					node.add(row);
					node.add(column);
				}
				else
				{
					assert(op.from.is_vector());
					node.add(op.swizzle[0]);
				}

				result = node.result; // Result ID
				break;
			}
			assert(false);
			break;
		}
	}

	return result;
}
void    spirv_module::access_chain_store(spv_basic_block &section, const spv_expression &chain, spv::Id value, const reshadefx::spv_type &value_type)
{
	assert(chain.is_lvalue && !chain.is_constant);
	assert(!value_type.is_pointer);

	spv::Id target = chain.base;

	size_t op_index = 0;

	auto base_type = chain.type;
	if (!chain.ops.empty())
		base_type = chain.ops[0].from;

	// Any indexing expressions can be resolved with an 'OpAccessChain' already
	if (!chain.ops.empty() && chain.ops[0].type == spv_expression::index)
	{
		assert(chain.ops[0].to.is_pointer);
		spv_instruction &node = add_node(section, chain.location, spv::OpAccessChain)
			.add(target); // Base
		do {
			assert(chain.ops[op_index].to.is_pointer);
			base_type = chain.ops[op_index].to;
			node.add(chain.ops[op_index++].index); // Indexes
		} while (op_index < chain.ops.size() && chain.ops[op_index].type == spv_expression::index);
		node.type = convert_type(chain.ops[op_index - 1].to); // Last type is the result
		target = node.result; // Result ID
	}

	// TODO: Complex access chains like float4x4[0].m00m10[0] = 0;
	// Work through all remaining operations in the access chain and apply them to the value
	for (; op_index < chain.ops.size(); ++op_index)
	{
		const auto &op = chain.ops[op_index];

		switch (op.type)
		{
		case spv_expression::cast:
			assert(false); // This cannot happen
			break;
		case spv_expression::index:
			assert(false);
			break;
		case spv_expression::swizzle:
		{
			base_type.is_pointer = false;

			spv::Id result = add_node(section, chain.location, spv::OpLoad, convert_type(base_type))
				.add(target) // Pointer
				.result; // Result ID

			if (base_type.is_vector() && value_type.is_vector())
			{
				spv_instruction &node = add_node(section, chain.location, spv::OpVectorShuffle, convert_type(base_type))
					.add(result) // Vector 1
					.add(value); // Vector 2

				unsigned int shuffle[4] = { 0, 1, 2, 3 };
				for (unsigned int i = 0; i < base_type.rows; ++i)
					if (op.swizzle[i] >= 0)
						shuffle[op.swizzle[i]] = base_type.rows + i;
				for (unsigned int i = 0; i < base_type.rows; ++i)
					node.add(shuffle[i]);

				value = node.result;
			}
			else if (op.to.is_scalar())
			{
				assert(op.swizzle[1] < 0); // TODO

				spv::Id result2 = add_node(section, chain.location, spv::OpCompositeInsert, convert_type(base_type))
					.add(value) // Object
					.add(result) // Composite
					.add(op.swizzle[0]) // Index
					.result; // Result ID

				value = result2;
			}
			else
			{
				assert(false);
			}
			break;
		}
		}
	}

	add_node_without_result(section, chain.location, spv::OpStore)
		.add(target)
		.add(value);
}
