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

enum op_type
{
	op_cast,
	op_index,
	op_swizzle
};

inline bool operator==(const reshadefx::spirv_type &lhs, const reshadefx::spirv_type &rhs)
{
	//return lhs.base == rhs.base && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition && lhs.is_pointer == rhs.is_pointer;
	//return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	return lhs.base == rhs.base && lhs.rows == rhs.rows && lhs.cols == rhs.cols && lhs.array_length == rhs.array_length && lhs.definition == rhs.definition && lhs.is_ptr == rhs.is_ptr && lhs.is_input == rhs.is_input && lhs.is_output == rhs.is_output;
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

static inline void write(std::vector<uint32_t> &s, uint32_t word)
{
	//s.write(reinterpret_cast<const char *>(&word), 4);;
	s.push_back(word);
}
static void write(std::vector<uint32_t> &s, const spirv_instruction &ins)
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

void spirv_module::write_module(std::vector<uint32_t> &s) const
{
	// Write SPIRV header info
	write(s, spv::MagicNumber);
	write(s, spv::Version);
	write(s, 0u); // Generator magic number, see https://www.khronos.org/registry/spir-v/api/spir-v.xml
	write(s, _next_id); // Maximum ID
	write(s, 0u); // Reserved for instruction schema

	// All capabilities
	write(s, spirv_instruction(spv::OpCapability)
		.add(spv::CapabilityMatrix));
	write(s, spirv_instruction(spv::OpCapability)
		.add(spv::CapabilityShader));

	for (spv::Capability capability : _capabilities)
		write(s, spirv_instruction(spv::OpCapability)
			.add(capability));

	write(s, spirv_instruction(spv::OpExtension)
		.add_string("SPV_GOOGLE_hlsl_functionality1"));

	// Optional extension instructions
	write(s, spirv_instruction(spv::OpExtInstImport, glsl_ext)
		.add_string("GLSL.std.450")); // Import GLSL extension

	// Single required memory model instruction
	write(s, spirv_instruction(spv::OpMemoryModel)
		.add(spv::AddressingModelLogical)
		.add(spv::MemoryModelGLSL450));

	// All entry point declarations
	for (const auto &node : _entries.instructions)
		write(s, node);

	// All debug instructions
	write(s, spirv_instruction(spv::OpSource)
		.add(spv::SourceLanguageHLSL)
		.add(300)); // ReShade FX is similar to HLSL SM 3

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

spirv_instruction &spirv_module::add_instruction(spirv_basic_block &section, const location &loc, spv::Op op, spv::Id type)
{
	spirv_instruction &instruction = add_instruction_without_result(section, loc, op);
	instruction.type = type;
	instruction.result = make_id();
	return instruction;
}
spirv_instruction &spirv_module::add_instruction_without_result(spirv_basic_block &section, const location &loc, spv::Op op)
{
	return section.instructions.emplace_back(op);
}

void spirv_module::define_struct(spv::Id id, const char *name, const location &loc, const std::vector<spv::Id> &members)
{
	add_instruction_without_result(_types_and_constants, loc, spv::OpTypeStruct)
		.add(members.begin(), members.end())
		.result = id;

	if (name != nullptr)
		add_name(id, name);
}
void spirv_module::define_function(spv::Id id, const char *name, const location &loc, const reshadefx::spirv_type &ret_type)
{
	auto &function = _functions2.emplace_back();
	function.return_type = ret_type;

	spirv_instruction &instruction = function.declaration;
	instruction.op = spv::OpFunction;
	instruction.type = convert_type(ret_type);
	instruction.result = id;
	instruction.add(spv::FunctionControlMaskNone);

	_current_function = _functions2.size() - 1;

	if (name != nullptr)
		add_name(id, name);
}
void spirv_module::define_function_param(spv::Id id, const char *name, const location &loc, const reshadefx::spirv_type &type)
{
	_functions2[_current_function].param_types.push_back(type);

	spirv_instruction &instruction = add_instruction_without_result(_functions2[_current_function].variables, loc, spv::OpFunctionParameter);
	instruction.type = convert_type(type);
	instruction.result = id;

	if (name != nullptr)
		add_name(id, name);
}
void spirv_module::define_variable(spv::Id id, const char *name, const location &loc, const reshadefx::spirv_type &type, spv::StorageClass storage, spv::Id initializer)
{
	spirv_instruction &instruction = add_instruction_without_result(storage != spv::StorageClassFunction ? _variables : _functions2[_current_function].variables, loc, spv::OpVariable);
	instruction.type = convert_type(type);
	instruction.result = id;
	instruction.add(storage);
	if (initializer != 0)
		instruction.add(initializer);

	if (name != nullptr)
		add_name(id, name);
}

void spirv_module::add_capability(spv::Capability capability)
{
	_capabilities.insert(capability);
}

void spirv_module::add_name(spv::Id id, const char *name)
{
	assert(name != nullptr);
	add_instruction_without_result(_debug_b, {}, spv::OpName)
		.add(id)
		.add_string(name);
}
void spirv_module::add_builtin(spv::Id id, spv::BuiltIn builtin)
{
	add_instruction_without_result(_annotations, {}, spv::OpDecorate)
		.add(id)
		.add(spv::DecorationBuiltIn)
		.add(builtin);
}
void spirv_module::add_decoration(spv::Id id, spv::Decoration decoration, const char *string)
{
	assert(string != nullptr);
	add_instruction_without_result(_annotations, {}, spv::OpDecorateStringGOOGLE)
		.add(id)
		.add(decoration)
		.add_string(string);
}
void spirv_module::add_decoration(spv::Id id, spv::Decoration decoration, std::initializer_list<uint32_t> values)
{
	add_instruction_without_result(_annotations, {}, spv::OpDecorate)
		.add(id)
		.add(decoration)
		.add(values.begin(), values.end());
}
void spirv_module::add_member_name(spv::Id id, uint32_t member_index, const char *name)
{
	assert(name != nullptr);
	add_instruction_without_result(_debug_b, {}, spv::OpMemberName)
		.add(id)
		.add(member_index)
		.add_string(name);
}
void spirv_module::add_member_builtin(spv::Id id, uint32_t member_index, spv::BuiltIn builtin)
{
	add_instruction_without_result(_annotations, {}, spv::OpMemberDecorate)
		.add(id)
		.add(member_index)
		.add(spv::DecorationBuiltIn)
		.add(builtin);
}
void spirv_module::add_member_decoration(spv::Id id, uint32_t member_index, spv::Decoration decoration, const char *string)
{
	assert(string != nullptr);
	add_instruction_without_result(_annotations, {}, spv::OpMemberDecorateStringGOOGLE)
		.add(id)
		.add(member_index)
		.add(decoration)
		.add_string(string);
}
void spirv_module::add_member_decoration(spv::Id id, uint32_t member_index, spv::Decoration decoration, std::initializer_list<uint32_t> values)
{
	add_instruction_without_result(_annotations, {}, spv::OpMemberDecorate)
		.add(id)
		.add(member_index)
		.add(decoration)
		.add(values.begin(), values.end());
}
void spirv_module::add_entry_point(const char *name, spv::Id function, spv::ExecutionModel model, const std::vector<spv::Id> &io)
{
	assert(name != nullptr);
	add_instruction_without_result(_entries, {}, spv::OpEntryPoint)
		.add(model)
		.add(function)
		.add_string(name)
		.add(io.begin(), io.end());
}

void spirv_module::add_cast_operation(spirv_expression &chain, const reshadefx::spirv_type &in_type)
{
	if (chain.type == in_type)
		return; // There is nothing to do if the expression is already of the target type

	if (chain.is_constant)
	{
		const auto cast_constant = [](spirv_constant &constant, const spirv_type &from, const spirv_type &to) {
			if (to.is_integral())
			{
				if (!from.is_integral())
				{
					for (unsigned int i = 0; i < 16; ++i)
						constant.as_uint[i] = static_cast<int>(constant.as_float[i]);
				}
				else
				{
					// int 2 uint
				}
			}
			else
			{
				if (!from.is_integral())
				{
					// Scalar to vector promotion
					assert(from.is_floating_point() && from.is_scalar() && to.is_vector());
					for (unsigned int i = 1; i < to.components(); ++i)
						constant.as_float[i] = constant.as_float[0];
				}
				else
				{
					if (from.is_scalar())
					{
						const float value = static_cast<float>(static_cast<int>(constant.as_uint[0]));
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_float[i] = value;
					}
					else
					{
						for (unsigned int i = 0; i < 16; ++i)
							constant.as_float[i] = static_cast<float>(static_cast<int>(constant.as_uint[i]));
					}
				}
			}
		};

		for (size_t i = 0; i < chain.constant.array_data.size(); ++i)
		{
			cast_constant(chain.constant.array_data[i], chain.type, in_type);
		}

		cast_constant(chain.constant, chain.type, in_type);
	}
	else
	{
		chain.ops.push_back({ op_cast, chain.type, in_type });
	}

	chain.type = in_type;
	//chain.is_lvalue = false; // Can't do this because of 'if (chain.is_lvalue)' check in 'access_chain_load'
}
void spirv_module::add_member_access(spirv_expression &chain, size_t index, const spirv_type &in_type)
{
	spirv_type target_type = in_type;
	target_type.is_ptr = true;

	spirv_constant index_c = {};
	index_c.as_uint[0] = index;
	chain.ops.push_back({ op_index, chain.type, target_type, convert_constant({ spirv_type::t_uint, 1, 1 }, index_c) });

	chain.is_constant = false;
	chain.type = in_type;
}
void spirv_module::add_static_index_access(spirv_expression &chain, size_t index)
{
	if (!chain.is_constant)
	{
		chain.constant.as_uint[0] = index;
		memset(&chain.constant.as_uint[1], 0, sizeof(uint32_t) * 15); // Clear the reset of the constant
		return add_dynamic_index_access(chain, convert_constant({ spirv_type::t_uint, 1, 1 }, chain.constant));
	}

	if (chain.type.is_array())
	{
		chain.type.array_length = 0;

		chain.constant = chain.constant.array_data[index];
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
void spirv_module::add_dynamic_index_access(spirv_expression &chain, spv::Id index_expression)
{
	spirv_type target_type = chain.type;
	if (target_type.is_array())
		target_type.array_length = 0;
	else if (target_type.is_matrix())
		target_type.rows = target_type.cols,
		target_type.cols = 1;
	else if (target_type.is_vector())
		target_type.rows = 1;

	target_type.is_ptr = true; // OpAccessChain returns a pointer

	chain.ops.push_back({ op_index, chain.type, target_type, index_expression });

	chain.is_constant = false;
	chain.type = target_type;
	chain.type.is_ptr = false;
}
void spirv_module::add_swizzle_access(spirv_expression &chain, signed char in_swizzle[4], size_t length)
{
	// Indexing logic is simpler, so use that when possible
	if (length == 1 && chain.type.is_vector())
		return add_static_index_access(chain, in_swizzle[0]);

	// Check for redundant swizzle and ignore them
	//if (chain.type.is_vector() && length == chain.type.rows)
	//{
	//	bool ignore = true;
	//	for (unsigned int i = 0; i < length; ++i)
	//		if (in_swizzle[i] != i)
	//			ignore = false;
	//	if (ignore)
	//		return;
	//}

	spirv_type target_type = chain.type;
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
		chain.ops.push_back({ op_swizzle, chain.type, target_type, 0, { in_swizzle[0], in_swizzle[1], in_swizzle[2], in_swizzle[3] } });
	}

	chain.type = target_type;
	chain.type.is_ptr = false;
}

void spirv_module::enter_block(spirv_basic_block &section, spv::Id id)
{
	assert(_current_block == 0); // Should never be in another basic block if creating a new one
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only use labels inside functions

	add_instruction_without_result(section, {}, spv::OpLabel)
		.result = id;

	_current_block = id; // All instructions following a label are inside that basic block
}
void spirv_module::leave_block_and_kill(spirv_basic_block &section)
{
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only discard inside functions

	if (_current_block == 0)
		return;

	add_instruction_without_result(section, {}, spv::OpKill);

	_current_block = 0; // A discard leaves the current basic block
}
void spirv_module::leave_block_and_return(spirv_basic_block &section, spv::Id value)
{
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only return from inside functions

	if (_current_block == 0) // Might already have left the last block in which case this has to be ignored
		return;

	if (_functions2[_current_function].return_type.is_void())
	{
		add_instruction_without_result(section, {}, spv::OpReturn);
	}
	else
	{
		if (value == 0)
		{
			value = add_instruction(_types_and_constants, {}, spv::OpUndef, convert_type(_functions2[_current_function].return_type)).result;
		}

		add_instruction_without_result(section, {}, spv::OpReturnValue)
			.add(value);
	}

	_current_block = 0; // A return leaves the current basic block
}
void spirv_module::leave_block_and_branch(spirv_basic_block &section, spv::Id target)
{
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only branch inside functions

	if (_current_block == 0)
		return;

	add_instruction_without_result(section, {}, spv::OpBranch)
		.add(target);

	_current_block = 0; // A branch leaves the current basic block
}
void spirv_module::leave_block_and_branch_conditional(spirv_basic_block &section, spv::Id condition, spv::Id true_target, spv::Id false_target)
{
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only branch inside functions

	if (_current_block == 0)
		return;

	add_instruction_without_result(section, {}, spv::OpBranchConditional)
		.add(condition)
		.add(true_target)
		.add(false_target);

	_current_block = 0; // A branch leaves the current basic block
}
void spirv_module::leave_function()
{
	assert(_current_function != std::numeric_limits<size_t>::max()); // Can only leave if there was a function to begin with

	auto &function = _functions2[_current_function];

	// Append function end instruction
	add_instruction_without_result(function.definition, {}, spv::OpFunctionEnd);

	// Now that all parameters are known, the full function type can be added to the function
	function.declaration.add(convert_type(function)); // Function Type

	_current_function = std::numeric_limits<size_t>::max();
}

spv::Id spirv_module::convert_type(const reshadefx::spirv_type &info)
{
	if (auto it = std::find_if(_type_lookup.begin(), _type_lookup.end(),
		[&info](auto &x) { return x.first == info && (!info.is_ptr || (x.first.qualifiers & (spirv_type::q_static | spirv_type::q_uniform)) == (info.qualifiers & (spirv_type::q_static | spirv_type::q_uniform))); }); it != _type_lookup.end())
		return it->second;

	spv::Id type;

	if (info.is_ptr)
	{
		spirv_type eleminfo = info;
		eleminfo.is_input = false;
		eleminfo.is_output = false;
		eleminfo.is_ptr = false;

		const spv::Id elemtype = convert_type(eleminfo);

		spv::StorageClass storage = spv::StorageClassFunction;
		if (info.is_input)
			storage = spv::StorageClassInput;
		if (info.is_output)
			storage = spv::StorageClassOutput;
		if (info.has(spirv_type::q_static))
			storage = spv::StorageClassPrivate;
		if (info.has(spirv_type::q_uniform))
			storage = (info.is_texture() || info.is_sampler()) ? spv::StorageClassUniformConstant : spv::StorageClassUniform;

		type = add_instruction(_types_and_constants, {}, spv::OpTypePointer)
			.add(storage)
			.add(elemtype)
			.result;
	}
	else if (info.is_array())
	{
		assert(!info.is_ptr);

		spirv_type eleminfo = info;
		eleminfo.array_length = 0;

		const spv::Id elemtype = convert_type(eleminfo);

		if (info.array_length > 0) // Sized array
		{
			spirv_constant length_data = {};
			length_data.as_uint[0] = info.array_length;

			const spv::Id length_constant = convert_constant({ spirv_type::t_uint, 1, 1 }, length_data);

			type = add_instruction(_types_and_constants, {}, spv::OpTypeArray)
				.add(elemtype)
				.add(length_constant)
				.result;
		}
		else // Dynamic array
		{
			type = add_instruction(_types_and_constants, {}, spv::OpTypeRuntimeArray)
				.add(elemtype)
				.result;
		}
	}
	else if (info.is_matrix())
	{
		// Convert MxN matrix to a SPIR-V matrix with M vectors with N elements
		spirv_type eleminfo = info;
		eleminfo.rows = info.cols;
		eleminfo.cols = 1;

		const spv::Id elemtype = convert_type(eleminfo);

		// Matrix types with just one row are interpreted as if they were a vector type
		if (info.rows == 1)
		{
			type = elemtype;
		}
		else
		{
			type = add_instruction(_types_and_constants, {}, spv::OpTypeMatrix)
				.add(elemtype)
				.add(info.rows)
				.result;
		}
	}
	else if (info.is_vector())
	{
		spirv_type eleminfo = info;
		eleminfo.rows = 1;
		eleminfo.cols = 1;

		const spv::Id elemtype = convert_type(eleminfo);

		type = add_instruction(_types_and_constants, {}, spv::OpTypeVector)
			.add(elemtype)
			.add(info.rows)
			.result;
	}
	else
	{
		assert(!info.is_input && !info.is_output);

		switch (info.base)
		{
		case spirv_type::t_void:
			assert(info.rows == 0 && info.cols == 0);
			type = add_instruction(_types_and_constants, {}, spv::OpTypeVoid).result;
			break;
		case spirv_type::t_bool:
			assert(info.rows == 1 && info.cols == 1);
			type = add_instruction(_types_and_constants, {}, spv::OpTypeBool).result;
			break;
		case spirv_type::t_float:
			assert(info.rows == 1 && info.cols == 1);
			type = add_instruction(_types_and_constants, {}, spv::OpTypeFloat)
				.add(32)
				.result;
			break;
		case spirv_type::t_int:
			assert(info.rows == 1 && info.cols == 1);
			type = add_instruction(_types_and_constants, {}, spv::OpTypeInt)
				.add(32)
				.add(1)
				.result;
			break;
		case spirv_type::t_uint:
			assert(info.rows == 1 && info.cols == 1);
			type = add_instruction(_types_and_constants, {}, spv::OpTypeInt)
				.add(32)
				.add(0)
				.result;
			break;
		case spirv_type::t_struct:
			assert(info.rows == 0 && info.cols == 0 && info.definition != 0);
			type = info.definition;
			break;
		case spirv_type::t_texture: {
			assert(info.rows == 0 && info.cols == 0);
			spv::Id sampled_type = convert_type({ spirv_type::t_float, 1, 1 });
			type = add_instruction(_types_and_constants, {}, spv::OpTypeImage)
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
		case spirv_type::t_sampler: {
			assert(info.rows == 0 && info.cols == 0);
			spv::Id image_type = convert_type({ spirv_type::t_texture, 0, 0, spirv_type::q_uniform });
			type = add_instruction(_types_and_constants, {}, spv::OpTypeSampledImage)
				.add(image_type)
				.result;
			break;
		}
		default:
			assert(false);
			return 0;
		}
	}

	_type_lookup.push_back({ info, type });;

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

	spirv_instruction &node = add_instruction(_types_and_constants, {}, spv::OpTypeFunction);
	node.add(return_type);
	for (auto param_type : param_type_ids)
		node.add(param_type);

	_function_type_lookup.push_back({ info, node.result });;

	return node.result;
}

spv::Id spirv_module::convert_constant(const reshadefx::spirv_type &type, const spirv_constant &data)
{
	assert(!type.is_ptr);

	if (auto it = std::find_if(_constant_lookup.begin(), _constant_lookup.end(), [&type, &data](auto &x) {
		if (!(std::get<0>(x) == type && std::memcmp(&std::get<1>(x).as_uint[0], &data.as_uint[0], sizeof(uint32_t) * 16) == 0 && std::get<1>(x).array_data.size() == data.array_data.size()))
			return false;
		for (size_t i = 0; i < data.array_data.size(); ++i)
			if (std::memcmp(&std::get<1>(x).array_data[i].as_uint[0], &data.array_data[i].as_uint[0], sizeof(uint32_t) * 16) != 0)
				return false;
		return true;
	}); it != _constant_lookup.end())
		return std::get<2>(*it);

	spv::Id result = 0;

	if (type.is_array())
	{
		assert(type.array_length > 0);

		std::vector<spv::Id> elements;

		spirv_type elem_type = type;
		elem_type.array_length = 0;

		for (const spirv_constant &elem : data.array_data)
			elements.push_back(convert_constant(elem_type, elem));
		for (size_t i = elements.size(); i < static_cast<size_t>(type.array_length); ++i)
			elements.push_back(convert_constant(elem_type, {}));

		spirv_instruction &node = add_instruction(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

		for (spv::Id elem : elements)
			node.add(elem);

		result = node.result;
	}
	else if (type.is_struct())
	{
		result = add_instruction(_types_and_constants, {}, spv::OpConstantNull, convert_type(type)).result;
	}
	else if (type.is_matrix())
	{
		spv::Id rows[4] = {};

		for (unsigned int i = 0; i < type.rows; ++i)
		{
			spirv_type row_type = type;
			row_type.rows = type.cols;
			row_type.cols = 1;
			spirv_constant row_data = {};
			for (unsigned int k = 0; k < type.cols; ++k)
				row_data.as_uint[k] = data.as_uint[i * type.cols + k];

			rows[i] = convert_constant(row_type, row_data);
		}

		if (type.rows == 1)
		{
			result = rows[0];
		}
		else
		{
			spirv_instruction &node = add_instruction(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

			for (unsigned int i = 0; i < type.rows; ++i)
				node.add(rows[i]);

			result = node.result;
		}
	}
	else if (type.is_vector())
	{
		spv::Id rows[4] = {};

		for (unsigned int i = 0; i < type.rows; ++i)
		{
			spirv_type scalar_type = type;
			scalar_type.rows = 1;
			spirv_constant scalar_data = {};
			scalar_data.as_uint[0] = data.as_uint[i];

			rows[i] = convert_constant(scalar_type, scalar_data);
		}

		spirv_instruction &node = add_instruction(_types_and_constants, {}, spv::OpConstantComposite, convert_type(type));

		for (unsigned int i = 0; i < type.rows; ++i)
			node.add(rows[i]);

		result = node.result;
	}
	else if (type.is_boolean())
	{
		result = add_instruction(_types_and_constants, {}, data.as_uint[0] ? spv::OpConstantTrue : spv::OpConstantFalse, convert_type(type)).result;
	}
	else
	{
		assert(type.is_scalar());
		result = add_instruction(_types_and_constants, {}, spv::OpConstant, convert_type(type)).add(data.as_uint[0]).result;
	}

	_constant_lookup.push_back({ type, data, result });

	return result;
}

spv::Id spirv_module::construct_type(spirv_basic_block &section, const spirv_type &type, std::vector<spirv_expression> &arguments)
{
	std::vector<spv::Id> ids;

	// There must be exactly one constituent for each top-level component of the result
	if (type.is_matrix())
	{
		assert(type.rows == type.cols);

		// First, extract all arguments so that a list of scalars exist
		for (auto &argument : arguments)
		{
			if (!argument.type.is_scalar())
			{
				for (unsigned int index = 0; index < argument.type.components(); ++index)
				{
					spirv_expression scalar = argument;
					add_static_index_access(scalar, index);
					spirv_type scalar_type = scalar.type;
					scalar_type.base = type.base;
					add_cast_operation(scalar, scalar_type);
					ids.push_back(access_chain_load(section, scalar));
					assert(ids.back() != 0);
				}
			}
			else
			{
				spirv_type scalar_type = argument.type;
				scalar_type.base = type.base;
				add_cast_operation(argument, scalar_type);
				ids.push_back(access_chain_load(section, argument));
				assert(ids.back() != 0);
			}
		}

		// Second, turn that list of scalars into a list of column vectors
		for (size_t i = 0, j = 0; i < ids.size(); i += type.rows, ++j)
		{
			spirv_type vector_type = type;
			vector_type.cols = 1;

			spirv_instruction &node = add_instruction(section, {}, spv::OpCompositeConstruct, convert_type(vector_type));
			for (unsigned int k = 0; k < type.rows; ++k)
				node.add(ids[i + k]);

			ids[j] = node.result;
		}

		ids.erase(ids.begin() + type.cols, ids.end());

		// Finally, construct a matrix from those column vectors
		spirv_instruction &node = add_instruction(section, {}, spv::OpCompositeConstruct, convert_type(type));

		for (size_t i = 0; i < ids.size(); i += type.rows)
		{
			node.add(ids[i]);
		}

	}
	// The exception is that for constructing a vector, a contiguous subset of the scalars consumed can be represented by a vector operand instead
	else
	{
		assert(type.is_vector());

		for (spirv_expression &argument : arguments)
		{
			spirv_type target_type = argument.type;
			target_type.base = type.base;
			add_cast_operation(argument, target_type);
			assert(argument.type.is_scalar() || argument.type.is_vector());

			ids.push_back(access_chain_load(section, argument));
			assert(ids.back() != 0);
		}
	}

	return add_instruction(section, {}, spv::OpCompositeConstruct, convert_type(type))
		.add(ids.begin(), ids.end())
		.result;
}

spv::Id spirv_module::access_chain_load(spirv_basic_block &section, const spirv_expression &chain)
{
	if (chain.is_constant) // Constant expressions do not have a complex access chain
		return convert_constant(chain.type, chain.constant);

	spv::Id result = chain.base;

	size_t op_index2 = 0;

	// If a variable is referenced, load the value first
	if (chain.is_lvalue)
	{
		auto base_type = chain.type;
		if (!chain.ops.empty())
			base_type = chain.ops[0].from;

		// Any indexing expressions can be resolved during load with an 'OpAccessChain' already
		if (!chain.ops.empty() && chain.ops[0].type == op_index)
		{
			assert(chain.ops[0].to.is_ptr);
			spirv_instruction &node = add_instruction(section, chain.location, spv::OpAccessChain)
				.add(result); // Base

			// Ignore first index into 1xN matrices, since they were translated to a vector type in SPIR-V
			if (chain.ops[0].from.rows == 1 && chain.ops[0].from.cols > 1)
				op_index2 = 1;

			do {
				assert(chain.ops[op_index2].to.is_ptr);
				base_type = chain.ops[op_index2].to;
				node.add(chain.ops[op_index2++].index); // Indexes
			} while (op_index2 < chain.ops.size() && chain.ops[op_index2].type == op_index);
			node.type = convert_type(chain.ops[op_index2 - 1].to); // Last type is the result
			result = node.result; // Result ID
		}

		base_type.is_ptr = false;

		result = add_instruction(section, chain.location, spv::OpLoad, convert_type(base_type))
			.add(result) // Pointer
			.result; // Result ID
	}

	// Work through all remaining operations in the access chain and apply them to the value
	for (; op_index2 < chain.ops.size(); ++op_index2)
	{
		const auto &op = chain.ops[op_index2];

		switch (op.type)
		{
		case op_cast:
			assert(!op.to.is_ptr);

			if (op.from.base != op.to.base)
			{
				spirv_type from_with_to_base = op.from;
				from_with_to_base.base = op.to.base;

				if (op.from.is_boolean())
				{
					spirv_constant true_value = {};
					spirv_constant false_value = {};
					for (unsigned int i = 0; i < op.to.components(); ++i)
						true_value.as_uint[i] = op.to.is_floating_point() ? 0x3f800000 : 1;
					const spv::Id true_constant = convert_constant(from_with_to_base, true_value);
					const spv::Id false_constant = convert_constant(from_with_to_base, false_value);

					result = add_instruction(section, chain.location, spv::OpSelect, convert_type(from_with_to_base))
						.add(result) // Condition
						.add(true_constant)
						.add(false_constant)
						.result;
				}
				else
				{
					switch (op.to.base)
					{
					case spirv_type::t_bool:
						result = add_instruction(section, chain.location, op.from.is_floating_point() ? spv::OpFOrdNotEqual : spv::OpINotEqual, convert_type(from_with_to_base))
							.add(result)
							.add(convert_constant(op.from, {}))
							.result;
						break;
					case spirv_type::t_int:
						result = add_instruction(section, chain.location, op.from.is_floating_point() ? spv::OpConvertFToS : spv::OpBitcast, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					case spirv_type::t_uint:
						result = add_instruction(section, chain.location, op.from.is_floating_point() ? spv::OpConvertFToU : spv::OpBitcast, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					case spirv_type::t_float:
						assert(op.from.is_integral());
						result = add_instruction(section, chain.location, op.from.is_signed() ? spv::OpConvertSToF : spv::OpConvertUToF, convert_type(from_with_to_base))
							.add(result)
							.result;
						break;
					}
				}
			}

			if (op.to.components() > op.from.components())
			{
				spirv_instruction &composite_node = add_instruction(section, chain.location, chain.is_constant ? spv::OpConstantComposite : spv::OpCompositeConstruct, convert_type(op.to));
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
		case op_index:
			if (op.from.is_vector() && op.to.is_scalar())
			{
				spirv_type target_type = op.to;
				target_type.is_ptr = false;
				spirv_instruction &node = add_instruction(section, chain.location, spv::OpVectorExtractDynamic, convert_type(target_type))
					.add(result) // Vector
					.add(op.index); // Index

				result = node.result; // Result ID
				break;
			}
			assert(false);
			break;
		case op_swizzle:
			if (op.to.is_vector())
			{
				if (op.from.is_matrix())
				{
					spv::Id components[4];

					for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
					{
						const unsigned int row = op.swizzle[i] / 4;
						const unsigned int column = op.swizzle[i] - row * 4;

						spirv_type scalar_type = op.to;
						scalar_type.rows = 1;
						scalar_type.cols = 1;

						spirv_instruction &node = add_instruction(section, chain.location, spv::OpCompositeExtract, convert_type(scalar_type))
							.add(result);

						if (op.from.rows > 1) // Matrix types with a single row are actually vectors, so they don't need the extra index
							node.add(row);

						node.add(column);

						components[i] = node.result;
					}

					spirv_instruction &node = add_instruction(section, chain.location, spv::OpCompositeConstruct, convert_type(op.to));

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

					spirv_instruction &node = add_instruction(section, chain.location, spv::OpVectorShuffle, convert_type(op.to))
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

				spirv_instruction &node = add_instruction(section, chain.location, spv::OpCompositeExtract, convert_type(op.to))
					.add(result); // Composite

				if (op.from.is_matrix() && op.from.rows > 1)
				{
					const unsigned int row = op.swizzle[0] / 4;
					const unsigned int column = op.swizzle[0] - row * 4;
					node.add(row);
					node.add(column);
				}
				else
				{
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
void    spirv_module::access_chain_store(spirv_basic_block &section, const spirv_expression &chain, spv::Id value, const reshadefx::spirv_type &value_type)
{
	assert(chain.is_lvalue && !chain.is_constant);
	assert(!value_type.is_ptr);

	spv::Id target = chain.base;

	size_t op_index2 = 0;

	auto base_type = chain.type;
	if (!chain.ops.empty())
		base_type = chain.ops[0].from;

	// Any indexing expressions can be resolved with an 'OpAccessChain' already
	if (!chain.ops.empty() && chain.ops[0].type == op_index)
	{
		assert(chain.ops[0].to.is_ptr);
		spirv_instruction &node = add_instruction(section, chain.location, spv::OpAccessChain)
			.add(target); // Base

		// Ignore first index into 1xN matrices, since they were translated to a vector type in SPIR-V
		if (chain.ops[0].from.rows == 1 && chain.ops[0].from.cols > 1)
			op_index2 = 1;

		do {
			assert(chain.ops[op_index2].to.is_ptr);
			base_type = chain.ops[op_index2].to;
			node.add(chain.ops[op_index2++].index); // Indexes
		} while (op_index2 < chain.ops.size() && chain.ops[op_index2].type == op_index);
		node.type = convert_type(chain.ops[op_index2 - 1].to); // Last type is the result
		target = node.result; // Result ID
	}

	// TODO: Complex access chains like float4x4[0].m00m10[0] = 0;
	// Work through all remaining operations in the access chain and apply them to the value
	for (; op_index2 < chain.ops.size(); ++op_index2)
	{
		const auto &op = chain.ops[op_index2];

		switch (op.type)
		{
		case op_cast:
			assert(false); // This cannot happen
			break;
		case op_index:
			assert(false);
			break;
		case op_swizzle:
		{
			base_type.is_ptr = false;

			spv::Id result = add_instruction(section, chain.location, spv::OpLoad, convert_type(base_type))
				.add(target) // Pointer
				.result; // Result ID

			if (base_type.is_vector() && value_type.is_vector())
			{
				spirv_instruction &node = add_instruction(section, chain.location, spv::OpVectorShuffle, convert_type(base_type))
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

				spv::Id result2 = add_instruction(section, chain.location, spv::OpCompositeInsert, convert_type(base_type))
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

	add_instruction_without_result(section, chain.location, spv::OpStore)
		.add(target)
		.add(value);
}
