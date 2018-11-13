/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

// Use the C++ variant of the SPIR-V headers
#include <spirv.hpp>
namespace spv {
#include <GLSL.std.450.h>
}

using namespace reshadefx;

/// <summary>
/// A single instruction in a SPIR-V module
/// </summary>
struct spirv_instruction
{
	spv::Op op;
	spv::Id type;
	spv::Id result;
	std::vector<spv::Id> operands;

	explicit spirv_instruction(spv::Op op = spv::OpNop) : op(op), type(0), result(0) { }
	spirv_instruction(spv::Op op, spv::Id result) : op(op), type(result), result(0) { }
	spirv_instruction(spv::Op op, spv::Id type, spv::Id result) : op(op), type(type), result(result) { }

	/// <summary>
	/// Add a single operand to the instruction.
	/// </summary>
	spirv_instruction &add(spv::Id operand)
	{
		operands.push_back(operand);
		return *this;
	}

	/// <summary>
	/// Add a range of operands to the instruction.
	/// </summary>
	template <typename It>
	spirv_instruction &add(It begin, It end)
	{
		operands.insert(operands.end(), begin, end);
		return *this;
	}

	/// <summary>
	/// Add a null-terminated literal UTF-8 string to the instruction.
	/// </summary>
	spirv_instruction &add_string(const char *string)
	{
		uint32_t word;
		do {
			word = 0;
			for (uint32_t i = 0; i < 4 && *string; ++i)
				reinterpret_cast<uint8_t *>(&word)[i] = *string++;
			add(word);
		} while (*string || word & 0xFF000000);
		return *this;
	}

	/// <summary>
	/// Write this instruction to a SPIR-V module.
	/// </summary>
	/// <param name="output">The output stream to append this instruction to.</param>
	void write(std::vector<uint32_t> &output) const
	{
		// See: https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html
		// 0             | Opcode: The 16 high-order bits are the WordCount of the instruction. The 16 low-order bits are the opcode enumerant.
		// 1             | Optional instruction type <id>
		// .             | Optional instruction Result <id>
		// .             | Operand 1 (if needed)
		// .             | Operand 2 (if needed)
		// ...           | ...
		// WordCount - 1 | Operand N (N is determined by WordCount minus the 1 to 3 words used for the opcode, instruction type <id>, and instruction Result <id>).

		const uint32_t num_words = 1 + (type != 0) + (result != 0) + static_cast<uint32_t>(operands.size());
		output.push_back((num_words << spv::WordCountShift) | op);

		// Optional instruction type ID
		if (type != 0) output.push_back(type);

		// Optional instruction result ID
		if (result != 0) output.push_back(result);

		// Write out the operands
		output.insert(output.end(), operands.begin(), operands.end());
	}
};

/// <summary>
/// A list of instructions forming a basic block in the SPIR-V module
/// </summary>
struct spirv_basic_block
{
	std::vector<spirv_instruction> instructions;

	/// <summary>
	/// Append another basic block the end of this one.
	/// </summary>
	void append(const spirv_basic_block &block)
	{
		instructions.insert(instructions.end(), block.instructions.begin(), block.instructions.end());
	}
};

class codegen_spirv final : public codegen
{
public:
	codegen_spirv(bool debug_info, bool uniforms_to_spec_constants)
		: _debug_info(debug_info), _uniforms_to_spec_constants(uniforms_to_spec_constants)
	{
		_glsl_ext = make_id();
	}

private:
	struct type_lookup
	{
		type type;
		spv::StorageClass storage;
		bool is_ptr;
		spv::Id id;
	};
	struct function_blocks
	{
		spirv_basic_block declaration;
		spirv_basic_block variables;
		spirv_basic_block definition;
		type return_type;
		std::vector<type> param_types;

		friend bool operator==(const function_blocks &lhs, const function_blocks &rhs)
		{
			if (lhs.param_types.size() != rhs.param_types.size())
				return false;
			for (size_t i = 0; i < lhs.param_types.size(); ++i)
				if (!(lhs.param_types[i] == rhs.param_types[i]))
					return false;
			return lhs.return_type == rhs.return_type;
		}
	};

	spirv_basic_block _entries;
	spirv_basic_block _debug_a;
	spirv_basic_block _debug_b;
	spirv_basic_block _annotations;
	spirv_basic_block _types_and_constants;
	spirv_basic_block _variables;

	std::unordered_set<spv::Capability> _capabilities;
	std::vector<type_lookup> _type_lookup;
	std::vector<std::pair<function_blocks, spv::Id>> _function_type_lookup;
	std::vector<std::tuple<type, constant, spv::Id>> _constant_lookup;
	std::unordered_map<std::string, uint32_t> _semantic_to_location;
	std::unordered_map<std::string, spv::Id> _string_lookup;
	std::unordered_map<spv::Id, spv::StorageClass> _storage_lookup;
	uint32_t _current_sampler_binding = 0;
	uint32_t _current_semantic_location = 10;
	std::unordered_set<spv::Id> _spec_constants;

	std::vector<function_blocks> _functions2;
	std::unordered_map<id, spirv_basic_block> _block_data;
	spirv_basic_block *_current_block_data = nullptr;

	bool _debug_info = false;
	bool _uniforms_to_spec_constants = false;
	id _next_id = 1;
	id _glsl_ext = 0;
	id _last_block = 0;
	id _current_block = 0;
	struct_info _global_ubo_type;
	id _global_ubo_variable = 0;
	uint32_t _global_ubo_offset = 0;
	function_blocks *_current_function = nullptr;

	inline id make_id() { return _next_id++; }

	inline void add_location(const location &loc, spirv_basic_block &block)
	{
		if (loc.source.empty() || !_debug_info)
			return;

		spv::Id file = _string_lookup[loc.source];
		if (file == 0) {
			file = add_instruction(spv::OpString, 0, _debug_a)
				.add_string(loc.source.c_str())
				.result;
			_string_lookup[loc.source] = file;
		}

		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpLine
		add_instruction_without_result(spv::OpLine, block)
			.add(file)
			.add(loc.line)
			.add(loc.column);
	}
	inline spirv_instruction &add_instruction(spv::Op op, spv::Id type = 0)
	{
		assert(is_in_function() && is_in_block());
		return add_instruction(op, type, *_current_block_data);
	}
	inline spirv_instruction &add_instruction(spv::Op op, spv::Id type, spirv_basic_block &block)
	{
		spirv_instruction &instruction = add_instruction_without_result(op, block);
		instruction.type = type;
		instruction.result = make_id();
		return instruction;
	}
	inline spirv_instruction &add_instruction_without_result(spv::Op op)
	{
		assert(is_in_function() && is_in_block());
		return add_instruction_without_result(op, *_current_block_data);
	}
	inline spirv_instruction &add_instruction_without_result(spv::Op op, spirv_basic_block &block)
	{
		return block.instructions.emplace_back(op);
	}

	void write_result(module &module) override
	{
		// First create the UBO struct type now that all member types are known
		if (_global_ubo_type.definition != 0)
		{
			define_struct({}, _global_ubo_type);

			define_variable(_global_ubo_variable, {}, { type::t_struct, 0, 0, type::q_uniform, 0, _global_ubo_type.definition }, "$Globals", spv::StorageClassUniform);
		}

		module = std::move(_module);

		// Write SPIRV header info
		module.spirv.push_back(spv::MagicNumber);
		module.spirv.push_back(spv::Version);
		module.spirv.push_back(0u); // Generator magic number, see https://www.khronos.org/registry/spir-v/api/spir-v.xml
		module.spirv.push_back(_next_id); // Maximum ID
		module.spirv.push_back(0u); // Reserved for instruction schema

		// All capabilities
		spirv_instruction(spv::OpCapability)
			.add(spv::CapabilityMatrix)
			.write(module.spirv);
		spirv_instruction(spv::OpCapability)
			.add(spv::CapabilityShader)
			.write(module.spirv);

		for (spv::Capability capability : _capabilities)
			spirv_instruction(spv::OpCapability)
				.add(capability)
				.write(module.spirv);

		spirv_instruction(spv::OpExtension)
			.add_string("SPV_GOOGLE_hlsl_functionality1")
			.write(module.spirv);

		// Optional extension instructions
		spirv_instruction(spv::OpExtInstImport, _glsl_ext)
			.add_string("GLSL.std.450") // Import GLSL extension
			.write(module.spirv);

		// Single required memory model instruction
		spirv_instruction(spv::OpMemoryModel)
			.add(spv::AddressingModelLogical)
			.add(spv::MemoryModelGLSL450)
			.write(module.spirv);

		// All entry point declarations
		for (const auto &node : _entries.instructions)
			node.write(module.spirv);

		if (_debug_info)
		{
			// All debug instructions
			for (const auto &node : _debug_a.instructions)
				node.write(module.spirv);
			for (const auto &node : _debug_b.instructions)
				node.write(module.spirv);
		}

		// All annotation instructions
		for (const auto &node : _annotations.instructions)
			node.write(module.spirv);

		// All type declarations
		for (const auto &node : _types_and_constants.instructions)
			node.write(module.spirv);
		for (const auto &node : _variables.instructions)
			node.write(module.spirv);

		// All function definitions
		for (const auto &function : _functions2)
		{
			if (function.definition.instructions.empty())
				continue;

			for (const auto &node : function.declaration.instructions)
				node.write(module.spirv);

			// Grab first label and move it in front of variable declarations
			function.definition.instructions.front().write(module.spirv);
			assert(function.definition.instructions.front().op == spv::OpLabel);

			for (const auto &node : function.variables.instructions)
				node.write(module.spirv);
			for (auto it = function.definition.instructions.begin() + 1; it != function.definition.instructions.end(); ++it)
				it->write(module.spirv);
		}
	}

	spv::Id convert_type(const type &info, bool is_ptr = false, spv::StorageClass storage = spv::StorageClassFunction)
	{
		if (auto it = std::find_if(_type_lookup.begin(), _type_lookup.end(),
			[&](auto &x) { return x.type == info && x.is_ptr == is_ptr && x.storage == storage; }); it != _type_lookup.end())
			return it->id;

		spv::Id type;

		if (is_ptr)
		{
			const spv::Id elemtype = convert_type(info);

			type = add_instruction(spv::OpTypePointer, 0, _types_and_constants)
				.add(storage)
				.add(elemtype)
				.result;
		}
		else if (info.is_array())
		{
			auto eleminfo = info;
			eleminfo.array_length = 0;

			// Make sure we don't get any dynamic arrays here
			assert(info.array_length > 0);

			const spv::Id length = emit_constant(info.array_length);
			const spv::Id elemtype = convert_type(eleminfo);

			type = add_instruction(spv::OpTypeArray, 0, _types_and_constants)
				.add(elemtype)
				.add(length)
				.result;
		}
		else if (info.is_matrix())
		{
			// Convert MxN matrix to a SPIR-V matrix with M vectors with N elements
			auto eleminfo = info;
			eleminfo.rows = info.cols;
			eleminfo.cols = 1;

			const spv::Id elemtype = convert_type(eleminfo);

			// Matrix types with just one row are interpreted as if they were a vector type
			if (info.rows == 1)
				type = elemtype;
			else
				type = add_instruction(spv::OpTypeMatrix, 0, _types_and_constants)
					.add(elemtype)
					.add(info.rows)
					.result;
		}
		else if (info.is_vector())
		{
			auto eleminfo = info;
			eleminfo.rows = 1;
			eleminfo.cols = 1;

			const spv::Id elemtype = convert_type(eleminfo);

			type = add_instruction(spv::OpTypeVector, 0, _types_and_constants)
				.add(elemtype)
				.add(info.rows)
				.result;
		}
		else
		{
			switch (info.base)
			{
			case type::t_void:
				assert(info.rows == 0 && info.cols == 0);
				type = add_instruction(spv::OpTypeVoid, 0, _types_and_constants).result;
				break;
			case type::t_bool:
				assert(info.rows == 1 && info.cols == 1);
				type = add_instruction(spv::OpTypeBool, 0, _types_and_constants).result;
				break;
			case type::t_int:
				assert(info.rows == 1 && info.cols == 1);
				type = add_instruction(spv::OpTypeInt, 0, _types_and_constants)
					.add(32)
					.add(1)
					.result;
				break;
			case type::t_uint:
				assert(info.rows == 1 && info.cols == 1);
				type = add_instruction(spv::OpTypeInt, 0, _types_and_constants)
					.add(32)
					.add(0)
					.result;
				break;
			case type::t_float:
				assert(info.rows == 1 && info.cols == 1);
				type = add_instruction( spv::OpTypeFloat, 0, _types_and_constants)
					.add(32)
					.result;
				break;
			case type::t_struct:
				assert(info.rows == 0 && info.cols == 0 && info.definition != 0);
				type = info.definition;
				break;
			case type::t_texture: {
				assert(info.rows == 0 && info.cols == 0);
				spv::Id sampled_type = convert_type({ type::t_float, 1, 1 });
				type = add_instruction(spv::OpTypeImage, 0, _types_and_constants)
					.add(sampled_type) // Sampled Type
					.add(spv::Dim2D)
					.add(0) // Not a depth image
					.add(0) // Not an array
					.add(0) // Not multi-sampled
					.add(1) // Will be used with a sampler
					.add(spv::ImageFormatUnknown)
					.result;
				break; }
			case type::t_sampler: {
				assert(info.rows == 0 && info.cols == 0);
				spv::Id image_type = convert_type({ type::t_texture, 0, 0, type::q_uniform });
				type = add_instruction(spv::OpTypeSampledImage, 0, _types_and_constants)
					.add(image_type)
					.result;
				break; }
			default:
				assert(false);
				return 0;
			}
		}

		_type_lookup.push_back({ info, storage, is_ptr, type });;

		return type;
	}
	spv::Id convert_type(const function_blocks &info)
	{
		if (auto it = std::find_if(_function_type_lookup.begin(), _function_type_lookup.end(), [&info](auto &x) { return x.first == info; }); it != _function_type_lookup.end())
			return it->second;

		spv::Id return_type = convert_type(info.return_type);
		assert(return_type != 0);
		std::vector<spv::Id> param_type_ids;
		for (auto param : info.param_types)
			param_type_ids.push_back(convert_type(param, true));

		spirv_instruction &node = add_instruction(spv::OpTypeFunction, 0, _types_and_constants);
		node.add(return_type);
		for (auto param_type : param_type_ids)
			node.add(param_type);

		_function_type_lookup.push_back({ info, node.result });;

		return node.result;
	}

	inline void add_name(id id, const char *name)
	{
		if (!_debug_info)
			return;

		assert(name != nullptr);
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpName
		add_instruction_without_result(spv::OpName, _debug_b)
			.add(id)
			.add_string(name);
	}
	inline void add_builtin(id id, spv::BuiltIn builtin)
	{
		add_instruction_without_result(spv::OpDecorate, _annotations)
			.add(id)
			.add(spv::DecorationBuiltIn)
			.add(builtin);
	}
	inline void add_decoration(id id, spv::Decoration decoration, const char *string)
	{
		assert(string != nullptr);
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpDecorateStringGOOGLE
		add_instruction_without_result(spv::OpDecorateStringGOOGLE, _annotations)
			.add(id)
			.add(decoration)
			.add_string(string);
	}
	inline void add_decoration(id id, spv::Decoration decoration, std::initializer_list<uint32_t> values = {})
	{
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpDecorate
		add_instruction_without_result(spv::OpDecorate, _annotations)
			.add(id)
			.add(decoration)
			.add(values.begin(), values.end());
	}
	inline void add_member_name(id id, uint32_t member_index, const char *name)
	{
		if (!_debug_info)
			return;

		assert(name != nullptr);
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpMemberName
		add_instruction_without_result(spv::OpMemberName, _debug_b)
			.add(id)
			.add(member_index)
			.add_string(name);
	}
	inline void add_member_builtin(id id, uint32_t member_index, spv::BuiltIn builtin)
	{
		add_instruction_without_result(spv::OpMemberDecorate, _annotations)
			.add(id)
			.add(member_index)
			.add(spv::DecorationBuiltIn)
			.add(builtin);
	}
	inline void add_member_decoration(id id, uint32_t member_index, spv::Decoration decoration, const char *string)
	{
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpMemberDecorateStringGOOGLE
		assert(string != nullptr);
		add_instruction_without_result(spv::OpMemberDecorateStringGOOGLE, _annotations)
			.add(id)
			.add(member_index)
			.add(decoration)
			.add_string(string);
	}
	inline void add_member_decoration(id id, uint32_t member_index, spv::Decoration decoration, std::initializer_list<uint32_t> values = {})
	{
		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpMemberDecorate
		add_instruction_without_result(spv::OpMemberDecorate, _annotations)
			.add(id)
			.add(member_index)
			.add(decoration)
			.add(values.begin(), values.end());
	}
	inline void add_capability(spv::Capability capability)
	{
		_capabilities.insert(capability);
	}

	id   define_struct(const location &loc, struct_info &info) override
	{
		// First define all member types to make sure they are declared before the struct type references them
		std::vector<spv::Id> member_types;
		member_types.reserve(info.member_list.size());
		for (const auto &member : info.member_list)
			member_types.push_back(convert_type(member.type));

		// Afterwards define the actual struct type
		add_location(loc, _types_and_constants);

		spirv_instruction &instruction = add_instruction(spv::OpTypeStruct, 0, _types_and_constants);
		for (spv::Id type_id : member_types)
			instruction.add(type_id);

		// Special handling for when this is called from 'create_global_ubo'
		if (info.definition == _global_ubo_type.definition)
			instruction.result = info.definition;
		else
			assert(info.definition == 0), info.definition = instruction.result;

		if (!info.unique_name.empty())
			add_name(info.definition, info.unique_name.c_str());

		for (uint32_t index = 0; index < info.member_list.size(); ++index)
			add_member_name(info.definition, index, info.member_list[index].name.c_str());

		_structs.push_back(info);

		return info.definition;
	}
	id   define_texture(const location &, texture_info &info) override
	{
		info.id = make_id();

		_module.textures.push_back(info);

		return info.id;
	}
	id   define_sampler(const location &loc, sampler_info &info) override
	{
		info.id = make_id();
		info.set = 1;
		info.binding = _current_sampler_binding++;

		define_variable(info.id, loc, { type::t_sampler, 0, 0, type::q_extern | type::q_uniform }, info.unique_name.c_str(), spv::StorageClassUniformConstant);

		add_decoration(info.id, spv::DecorationBinding, { info.binding });
		add_decoration(info.id, spv::DecorationDescriptorSet, { info.set });

		_module.samplers.push_back(info);

		return info.id;
	}
	id   define_uniform(const location &, uniform_info &info) override
	{
		if (_uniforms_to_spec_constants && info.type.is_scalar() && info.has_initializer_value)
		{
			const id res = emit_constant(info.type, info.initializer_value, true);

			add_name(res, info.name.c_str());

			_spec_constants.insert(res);
			_module.spec_constants.push_back(info);

			return res;
		}
		else
		{
			if (_global_ubo_type.definition == 0)
			{
				_global_ubo_type.definition = make_id();

				add_decoration(_global_ubo_type.definition, spv::DecorationBlock);
				add_decoration(_global_ubo_type.definition, spv::DecorationBinding, { 0 });
				add_decoration(_global_ubo_type.definition, spv::DecorationDescriptorSet, { 0 });
			}
			if (_global_ubo_variable == 0)
			{
				_global_ubo_variable = make_id();
			}

			// GLSL specification on std140 layout:
			// 1. If the member is a scalar consuming N basic machine units, the base alignment is N.
			// 2. If the member is a two- or four-component vector with components consuming N basic machine units, the base alignment is 2N or 4N, respectively.
			// 3. If the member is a three-component vector with components consuming N basic machine units, the base alignment is 4N.
			const unsigned int size = 4 * (info.type.rows == 3 ? 4 : info.type.rows) * info.type.cols * std::max(1, info.type.array_length);
			const unsigned int alignment = size;

			info.size = size;
			info.offset = (_global_ubo_offset % alignment != 0) ? _global_ubo_offset + alignment - _global_ubo_offset % alignment : _global_ubo_offset;
			_global_ubo_offset = info.offset + size;

			_module.uniforms.push_back(info);

			auto &member_list = _global_ubo_type.member_list;
			member_list.push_back({ info.type, info.name });

			// Convert boolean uniform variables to integer type so that they have a defined size
			if (info.type.is_boolean())
				member_list.back().type.base = type::t_uint;

			const uint32_t member_index = static_cast<uint32_t>(member_list.size() - 1);

			add_member_decoration(_global_ubo_type.definition, member_index, spv::DecorationOffset, { info.offset });

			return 0xF0000000 | member_index;
		}
	}
	id   define_variable(const location &loc, const type &type, std::string name, bool global, id initializer_value) override
	{
		const id res = make_id();

		define_variable(res, loc, type, name.c_str(), global ? spv::StorageClassPrivate : spv::StorageClassFunction, initializer_value);

		return res;
	}
	void define_variable(id id, const location &loc, const type &type, const char *name, spv::StorageClass storage, spv::Id initializer_value = 0)
	{
		spirv_basic_block &block = storage != spv::StorageClassFunction ? _variables : _current_function->variables;

		add_location(loc, block);

		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpVariable
		spirv_instruction &instruction = add_instruction_without_result(spv::OpVariable, block);
		instruction.type = convert_type(type, true, storage);
		instruction.result = id;
		instruction.add(storage);
		if (initializer_value != 0)
			instruction.add(initializer_value);

		if (name != nullptr && *name != '\0')
			add_name(id, name);

		_storage_lookup[id] = storage;
	}
	id   define_function(const location &loc, function_info &info) override
	{
		assert(!is_in_function());

		auto &function = _functions2.emplace_back();
		function.return_type = info.return_type;

		_current_function = &function;

		for (auto &param : info.parameter_list)
			function.param_types.push_back(param.type);

		add_location(loc, function.declaration);

		info.definition = add_instruction(spv::OpFunction, convert_type(info.return_type), function.declaration)
			.add(spv::FunctionControlMaskNone)
			.add(convert_type(function))
			.result;

		if (!info.name.empty())
			add_name(info.definition, info.name.c_str());

		for (auto &param : info.parameter_list)
		{
			add_location(param.location, function.declaration);

			param.definition = add_instruction(spv::OpFunctionParameter, convert_type(param.type, true), function.declaration)
				.result;

			add_name(param.definition, param.name.c_str());
		}

		_functions.push_back(std::make_unique<function_info>(info));

		return info.definition;
	}

	id   create_block() override
	{
		return make_id();
	}

	void create_entry_point(const function_info &func, bool is_ps) override
	{
		if (const auto it = std::find_if(_module.entry_points.begin(), _module.entry_points.end(),
			[&func](const auto &ep) { return ep.first == func.unique_name; }); it != _module.entry_points.end())
			return;

		_module.entry_points.push_back({ func.unique_name, is_ps });

		std::vector<expression> call_params;
		std::vector<unsigned int> inputs_and_outputs;

		// Generate the glue entry point function
		function_info entry_point;
		entry_point.return_type = { type::t_void };

		define_function({}, entry_point);
		enter_block(create_block());

		const auto semantic_to_builtin = [is_ps](const std::string &semantic, spv::BuiltIn &builtin) {
			builtin = spv::BuiltInMax;
			if (semantic == "SV_POSITION")
				builtin = is_ps ? spv::BuiltInFragCoord : spv::BuiltInPosition;
			if (semantic == "SV_POINTSIZE")
				builtin = spv::BuiltInPointSize;
			if (semantic == "SV_DEPTH")
				builtin = spv::BuiltInFragDepth;
			if (semantic == "VERTEXID" || semantic == "SV_VERTEXID")
				builtin = spv::BuiltInVertexId;
			return builtin != spv::BuiltInMax;
		};

		const auto create_input_param = [this, &call_params](const struct_member_info &param) {
			const auto function_variable = make_id();
			define_variable(function_variable, {}, param.type, nullptr, spv::StorageClassFunction);
			call_params.emplace_back().reset_to_lvalue({}, function_variable, param.type);
			return function_variable;
		};
		const auto create_input_variable = [this, &inputs_and_outputs, &semantic_to_builtin](const struct_member_info &param) {
			const auto input_variable = make_id();
			define_variable(input_variable, {}, param.type, nullptr, spv::StorageClassInput);

			if (spv::BuiltIn builtin; semantic_to_builtin(param.semantic, builtin))
				add_builtin(input_variable, builtin);
			else
			{
				uint32_t location = 0;
				if (param.semantic.size() >= 5 && param.semantic.compare(0, 5, "COLOR") == 0)
					location = std::strtol(param.semantic.substr(5).c_str(), nullptr, 10);
				else if (param.semantic.size() >= 9 && param.semantic.compare(0, 9, "SV_TARGET") == 0)
					location = std::strtol(param.semantic.substr(9).c_str(), nullptr, 10);
				else if (param.semantic.size() >= 8 && param.semantic.compare(0, 8, "TEXCOORD") == 0)
					location = std::strtol(param.semantic.substr(8).c_str(), nullptr, 10);
				else if (const auto it = _semantic_to_location.find(param.semantic); it != _semantic_to_location.end())
					location = it->second;
				else
					_semantic_to_location[param.semantic] = location = _current_semantic_location++;

				add_decoration(input_variable, spv::DecorationLocation, { location });
			}

			if (param.type.has(type::q_noperspective))
				add_decoration(input_variable, spv::DecorationNoPerspective);
			if (param.type.has(type::q_centroid))
				add_decoration(input_variable, spv::DecorationCentroid);
			if (param.type.has(type::q_nointerpolation))
				add_decoration(input_variable, spv::DecorationFlat);

			inputs_and_outputs.push_back(input_variable);
			return input_variable;
		};
		const auto create_output_param = [this, &call_params](const struct_member_info &param) {
			const auto function_variable = make_id();
			define_variable(function_variable, {}, param.type, nullptr, spv::StorageClassFunction);
			call_params.emplace_back().reset_to_lvalue({}, function_variable, param.type);
			return function_variable;
		};
		const auto create_output_variable = [this, &inputs_and_outputs, &semantic_to_builtin](const struct_member_info &param) {
			const auto output_variable = make_id();
			define_variable(output_variable, {}, param.type, nullptr, spv::StorageClassOutput);

			if (spv::BuiltIn builtin; semantic_to_builtin(param.semantic, builtin))
				add_builtin(output_variable, builtin);
			else
			{
				uint32_t location = 0;
				if (param.semantic.size() >= 5 && param.semantic.compare(0, 5, "COLOR") == 0)
					location = std::strtol(param.semantic.substr(5).c_str(), nullptr, 10);
				else if (param.semantic.size() >= 9 && param.semantic.compare(0, 9, "SV_TARGET") == 0)
					location = std::strtol(param.semantic.substr(9).c_str(), nullptr, 10);
				else if (param.semantic.size() >= 8 && param.semantic.compare(0, 8, "TEXCOORD") == 0)
					location = std::strtol(param.semantic.substr(8).c_str(), nullptr, 10);
				else if (const auto it = _semantic_to_location.find(param.semantic); it != _semantic_to_location.end())
					location = it->second;
				else
					_semantic_to_location[param.semantic] = location = _current_semantic_location++;

				add_decoration(output_variable, spv::DecorationLocation, { location });
			}

			if (param.type.has(type::q_noperspective))
				add_decoration(output_variable, spv::DecorationNoPerspective);
			if (param.type.has(type::q_centroid))
				add_decoration(output_variable, spv::DecorationCentroid);
			if (param.type.has(type::q_nointerpolation))
				add_decoration(output_variable, spv::DecorationFlat);

			inputs_and_outputs.push_back(output_variable);
			return output_variable;
		};

		// Handle input parameters
		for (const auto &param : func.parameter_list)
		{
			if (param.type.has(type::q_out))
			{
				create_output_param(param);

				// Flatten structure parameters
				if (param.type.is_struct())
					for (const auto &member : find_struct(param.type.definition).member_list)
						create_output_variable(member);
				else
					create_output_variable(param);
			}
			else
			{
				const auto param_variable = create_input_param(param);

				// Flatten structure parameters
				if (param.type.is_struct())
				{
					std::vector<unsigned int> elements;

					for (const auto &member : find_struct(param.type.definition).member_list)
					{
						const auto input_variable = create_input_variable(member);

						const auto value = add_instruction(spv::OpLoad, convert_type(member.type))
							.add(input_variable)
							.result;
						elements.push_back(value);
					}

					spirv_instruction &construct = add_instruction(spv::OpCompositeConstruct, convert_type(param.type));
					for (auto elem : elements)
						construct.add(elem);
					const auto composite_value = construct.result;

					add_instruction_without_result(spv::OpStore)
						.add(param_variable)
						.add(composite_value);
				}
				else
				{
					const auto input_variable = create_input_variable(param);

					const auto value = add_instruction(spv::OpLoad, convert_type(param.type))
						.add(input_variable)
						.result;
					add_instruction_without_result(spv::OpStore)
						.add(param_variable)
						.add(value);
				}
			}
		}

		const auto call_result = emit_call({}, func.definition, func.return_type, call_params);

		size_t param_index = 0;
		size_t inputs_and_outputs_index = 0;
		for (const struct_member_info &param : func.parameter_list)
		{
			if (param.type.has(type::q_out))
			{
				const auto value = add_instruction(spv::OpLoad, convert_type(param.type))
					.add(call_params[param_index++].base)
					.result;

				if (param.type.is_struct())
				{
					unsigned int member_index = 0;
					for (const auto &member : find_struct(param.type.definition).member_list)
					{
						const auto member_value = add_instruction(spv::OpCompositeExtract, convert_type(member.type))
							.add(value)
							.add(member_index++)
							.result;
						add_instruction_without_result(spv::OpStore)
							.add(inputs_and_outputs[inputs_and_outputs_index++])
							.add(member_value);
					}
				}
				else
				{
					add_instruction_without_result(spv::OpStore)
						.add(inputs_and_outputs[inputs_and_outputs_index++])
						.add(value);
				}
			}
			else
			{
				param_index++;
				inputs_and_outputs_index += param.type.is_struct() ? find_struct(param.type.definition).member_list.size() : 1;
			}
		}

		if (func.return_type.is_struct())
		{
			unsigned int member_index = 0;
			for (const auto &member : find_struct(func.return_type.definition).member_list)
			{
				const auto result = create_output_variable(member);

				const auto member_result = add_instruction(spv::OpCompositeExtract, convert_type(member.type))
					.add(call_result)
					.add(member_index)
					.result;

				add_instruction_without_result(spv::OpStore)
					.add(result)
					.add(member_result);

				member_index++;
			}
		}
		else if (!func.return_type.is_void())
		{
			const auto result = make_id();
			define_variable(result, {}, func.return_type, nullptr, spv::StorageClassOutput);

			if (spv::BuiltIn builtin; semantic_to_builtin(func.return_semantic, builtin))
				add_builtin(result, builtin);
			else
			{
				uint32_t semantic_location = 0;
				if (func.return_semantic.size() >= 5 && func.return_semantic.compare(0, 5, "COLOR") == 0)
					semantic_location = std::strtol(func.return_semantic.substr(5).c_str(), nullptr, 10);
				else if (func.return_semantic.size() >= 9 && func.return_semantic.compare(0, 9, "SV_TARGET") == 0)
					semantic_location = std::strtol(func.return_semantic.substr(9).c_str(), nullptr, 10);
				else if (func.return_semantic.size() >= 8 && func.return_semantic.compare(0, 8, "TEXCOORD") == 0)
					semantic_location = std::strtol(func.return_semantic.substr(8).c_str(), nullptr, 10);
				else if (const auto it = _semantic_to_location.find(func.return_semantic); it != _semantic_to_location.end())
					semantic_location = it->second;
				else
					_semantic_to_location[func.return_semantic] = semantic_location = _current_semantic_location++;

				add_decoration(result, spv::DecorationLocation, { semantic_location });
			}

			inputs_and_outputs.push_back(result);

			add_instruction_without_result(spv::OpStore)
				.add(result)
				.add(call_result);
		}

		leave_block_and_return(0);
		leave_function();

		assert(!func.unique_name.empty());
		add_instruction_without_result(spv::OpEntryPoint, _entries)
			.add(is_ps ? spv::ExecutionModelFragment : spv::ExecutionModelVertex)
			.add(entry_point.definition)
			.add_string(func.unique_name.c_str())
			.add(inputs_and_outputs.begin(), inputs_and_outputs.end());
	}

	id   emit_load(const expression &exp) override
	{
		if (exp.is_constant) // Constant expressions do not have a complex access chain
			return emit_constant(exp.type, exp.constant);

		spv::Id result = exp.base;

		size_t op_index2 = 0;
		auto base_type = exp.type;
		bool is_uniform_bool = false;

		if (exp.is_lvalue || !exp.chain.empty())
			add_location(exp.location, *_current_block_data);

		// If a variable is referenced, load the value first
		if (exp.is_lvalue && _spec_constants.find(exp.base) == _spec_constants.end())
		{
			if (!exp.chain.empty())
				base_type = exp.chain[0].from;

			spv::StorageClass storage = spv::StorageClassFunction;
			if (const auto it = _storage_lookup.find(exp.base); it != _storage_lookup.end())
				storage = it->second;

			// Check if this is a uniform variable (see 'define_uniform' function above) and dereference it
			if (result & 0xF0000000)
			{
				const uint32_t member_index = result ^ 0xF0000000;

				is_uniform_bool = base_type.is_boolean();

				if (is_uniform_bool)
					base_type.base = type::t_uint;

				result = add_instruction(spv::OpAccessChain, convert_type(base_type, true, spv::StorageClassUniform))
					.add(_global_ubo_variable)
					.add(emit_constant(member_index))
					.result;

				storage = spv::StorageClassUniform;
			}

			// Any indexing expressions can be resolved during load with an 'OpAccessChain' already
			if (!exp.chain.empty() && (exp.chain[0].op == expression::operation::op_index || exp.chain[0].op == expression::operation::op_member))
			{
				spirv_instruction &node = add_instruction(spv::OpAccessChain)
					.add(result); // Base

				// Ignore first index into 1xN matrices, since they were translated to a vector type in SPIR-V
				if (exp.chain[0].from.rows == 1 && exp.chain[0].from.cols > 1)
					op_index2 = 1;

				do {
					base_type = exp.chain[op_index2].to;
					if (exp.chain[op_index2].op == expression::operation::op_index)
						node.add(exp.chain[op_index2++].index); // Indexes
					else
						node.add(emit_constant(exp.chain[op_index2++].index)); // Indexes
				} while (op_index2 < exp.chain.size() && (exp.chain[op_index2].op == expression::operation::op_index || exp.chain[op_index2].op == expression::operation::op_member));
				node.type = convert_type(exp.chain[op_index2 - 1].to, true, storage); // Last type is the result
				result = node.result; // Result ID
			}

			result = add_instruction(spv::OpLoad, convert_type(base_type))
				.add(result) // Pointer
				.result; // Result ID
		}

		if (is_uniform_bool)
		{
			base_type.base = type::t_bool;

			result = add_instruction(spv::OpINotEqual, convert_type(base_type))
				.add(result)
				.add(emit_constant(0))
				.result;
		}

		// Work through all remaining operations in the access chain and apply them to the value
		for (; op_index2 < exp.chain.size(); ++op_index2)
		{
			const auto &op = exp.chain[op_index2];

			switch (op.op)
			{
			case expression::operation::op_cast:
				if (op.from.base != op.to.base)
				{
					type from_with_to_base = op.from;
					from_with_to_base.base = op.to.base;

					if (op.from.is_boolean())
					{
						constant true_value = {};
						constant false_value = {};
						for (unsigned int i = 0; i < op.to.components(); ++i)
							true_value.as_uint[i] = op.to.is_floating_point() ? 0x3f800000 : 1;
						const spv::Id true_constant = emit_constant(from_with_to_base, true_value);
						const spv::Id false_constant = emit_constant(from_with_to_base, false_value);

						result = add_instruction(spv::OpSelect, convert_type(from_with_to_base))
							.add(result) // Condition
							.add(true_constant)
							.add(false_constant)
							.result;
					}
					else
					{
						switch (op.to.base)
						{
						case type::t_bool:
							result = add_instruction(op.from.is_floating_point() ? spv::OpFOrdNotEqual : spv::OpINotEqual, convert_type(from_with_to_base))
								.add(result)
								.add(emit_constant(op.from, {}))
								.result;
							break;
						case type::t_int:
							result = add_instruction(op.from.is_floating_point() ? spv::OpConvertFToS : spv::OpBitcast, convert_type(from_with_to_base))
								.add(result)
								.result;
							break;
						case type::t_uint:
							result = add_instruction(op.from.is_floating_point() ? spv::OpConvertFToU : spv::OpBitcast, convert_type(from_with_to_base))
								.add(result)
								.result;
							break;
						case type::t_float:
							assert(op.from.is_integral());
							result = add_instruction(op.from.is_signed() ? spv::OpConvertSToF : spv::OpConvertUToF, convert_type(from_with_to_base))
								.add(result)
								.result;
							break;
						}
					}
				}

				if (op.to.components() > op.from.components())
				{
					spirv_instruction &composite_node = add_instruction(exp.is_constant ? spv::OpConstantComposite : spv::OpCompositeConstruct, convert_type(op.to));
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
			case expression::operation::op_index:
				if (op.from.is_array())
				{
					assert(false);
					/*assert(result != 0);
					//result = add_instruction(section, chain.location, spv::OpCompositeExtract, convert_type(op.to))
					//	.add(result)
					//	.add(op.index)
					//	.result;
					result = add_instruction(section, chain.location, spv::OpAccessChain, convert_type(op.to))
						.add(result)
						.add(op.index)
						.result;
					result = add_instruction(section, chain.location, spv::OpLoad, convert_type(op.to))
						.add(result)
						.result;*/
					break;
				}
				else if (op.from.is_vector() && op.to.is_scalar())
				{
					assert(result != 0);
					result = add_instruction(spv::OpVectorExtractDynamic, convert_type(op.to))
						.add(result) // Vector
						.add(op.index) // Index
						.result; // Result ID
					break;
				}
				assert(false);
				break;
			case expression::operation::op_swizzle:
				if (op.to.is_vector())
				{
					if (op.from.is_matrix())
					{
						spv::Id components[4];

						for (unsigned int i = 0; i < 4 && op.swizzle[i] >= 0; ++i)
						{
							const unsigned int row = op.swizzle[i] / 4;
							const unsigned int column = op.swizzle[i] - row * 4;

							type scalar_type = op.to;
							scalar_type.rows = 1;
							scalar_type.cols = 1;

							assert(result != 0);
							spirv_instruction &node = add_instruction(spv::OpCompositeExtract, convert_type(scalar_type))
								.add(result);

							if (op.from.rows > 1) // Matrix types with a single row are actually vectors, so they don't need the extra index
								node.add(row);

							node.add(column);

							components[i] = node.result;
						}

						spirv_instruction &node = add_instruction(spv::OpCompositeConstruct, convert_type(op.to));

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

						spirv_instruction &node = add_instruction(spv::OpVectorShuffle, convert_type(op.to))
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

					assert(result != 0);
					spirv_instruction &node = add_instruction(spv::OpCompositeExtract, convert_type(op.to))
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
	void emit_store(const expression &exp, id value) override
	{
		assert(value != 0);
		assert(exp.is_lvalue && !exp.is_constant);

		add_location(exp.location, *_current_block_data);

		spv::Id target = exp.base;

		size_t op_index2 = 0;

		auto base_type = exp.type;
		if (!exp.chain.empty())
			base_type = exp.chain[0].from;

		// Any indexing expressions can be resolved with an 'OpAccessChain' already
		if (!exp.chain.empty() && (exp.chain[0].op == expression::operation::op_index || exp.chain[0].op == expression::operation::op_member))
		{
			spv::StorageClass storage = spv::StorageClassFunction;
			if (const auto it = _storage_lookup.find(exp.base); it != _storage_lookup.end())
				storage = it->second;

			spirv_instruction &node = add_instruction(spv::OpAccessChain)
				.add(target); // Base

			// Ignore first index into 1xN matrices, since they were translated to a vector type in SPIR-V
			if (exp.chain[0].from.rows == 1 && exp.chain[0].from.cols > 1)
				op_index2 = 1;

			do {
				base_type = exp.chain[op_index2].to;
				if (exp.chain[op_index2].op == expression::operation::op_index)
					node.add(exp.chain[op_index2++].index); // Indexes
				else
					node.add(emit_constant(exp.chain[op_index2++].index)); // Indexes
			} while (op_index2 < exp.chain.size() && (exp.chain[op_index2].op == expression::operation::op_index || exp.chain[op_index2].op == expression::operation::op_member));
			node.type = convert_type(exp.chain[op_index2 - 1].to, true, storage); // Last type is the result
			target = node.result; // Result ID
		}

		// TODO: Complex access chains like float4x4[0].m00m10[0] = 0;
		// Work through all remaining operations in the access chain and apply them to the value
		for (; op_index2 < exp.chain.size(); ++op_index2)
		{
			const auto &op = exp.chain[op_index2];

			switch (op.op)
			{
			case expression::operation::op_cast:
				assert(false); // This cannot happen
				break;
			case expression::operation::op_index:
				assert(false);
				break;
			case expression::operation::op_swizzle:
			{
				spv::Id result = add_instruction(spv::OpLoad, convert_type(base_type))
					.add(target) // Pointer
					.result; // Result ID

				if (base_type.is_vector())
				{
					spirv_instruction &node = add_instruction(spv::OpVectorShuffle, convert_type(base_type))
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

					spirv_instruction &node = add_instruction(spv::OpCompositeInsert, convert_type(base_type))
						.add(value) // Object
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

					value = node.result; // Result ID
				}
				else
				{
					assert(false);
				}
				break;
			}
			}
		}

		add_instruction_without_result(spv::OpStore)
			.add(target)
			.add(value);
	}

	id   emit_constant(uint32_t value)
	{
		constant data;
		data.as_uint[0] = value;
		return emit_constant({ type::t_uint, 1, 1 }, data, false);
	}
	id   emit_constant(const type &type, const constant &data) override
	{
		return emit_constant(type, data, false);
	}
	id   emit_constant(const type &type, const constant &data, bool spec_constant)
	{
		if (!spec_constant)
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
			elements.reserve(type.array_length);

			auto elem_type = type;
			elem_type.array_length = 0;

			for (const constant &elem : data.array_data)
				elements.push_back(emit_constant(elem_type, elem));
			for (size_t i = elements.size(); i < static_cast<size_t>(type.array_length); ++i)
				elements.push_back(emit_constant(elem_type, {}));

			spirv_instruction &node = add_instruction(
				spec_constant ? spv::OpSpecConstantComposite : spv::OpConstantComposite,
				convert_type(type), _types_and_constants);

			for (spv::Id elem : elements)
				node.add(elem);

			result = node.result;
		}
		else if (type.is_struct())
		{
			assert(!spec_constant);

			result = add_instruction(spv::OpConstantNull, convert_type(type), _types_and_constants).result;
		}
		else if (type.is_matrix())
		{
			spv::Id rows[4] = {};

			for (unsigned int i = 0; i < type.rows; ++i)
			{
				auto row_type = type;
				row_type.rows = type.cols;
				row_type.cols = 1;
				constant row_data = {};
				for (unsigned int k = 0; k < type.cols; ++k)
					row_data.as_uint[k] = data.as_uint[i * type.cols + k];

				rows[i] = emit_constant(row_type, row_data);
			}

			if (type.rows == 1)
			{
				result = rows[0];
			}
			else
			{
				spirv_instruction &node = add_instruction(
					spec_constant ? spv::OpSpecConstantComposite : spv::OpConstantComposite,
					convert_type(type), _types_and_constants);

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
				auto scalar_type = type;
				scalar_type.rows = 1;
				constant scalar_data = {};
				scalar_data.as_uint[0] = data.as_uint[i];

				rows[i] = emit_constant(scalar_type, scalar_data);
			}

			spirv_instruction &node = add_instruction(
				spec_constant ? spv::OpSpecConstantComposite : spv::OpConstantComposite,
				convert_type(type), _types_and_constants);

			for (unsigned int i = 0; i < type.rows; ++i)
				node.add(rows[i]);

			result = node.result;
		}
		else if (type.is_boolean())
		{
			result = add_instruction(data.as_uint[0] ?
				(spec_constant ? spv::OpSpecConstantTrue : spv::OpConstantTrue) :
				(spec_constant ? spv::OpSpecConstantFalse : spv::OpConstantFalse),
				convert_type(type), _types_and_constants).result;
		}
		else
		{
			assert(type.is_scalar());
			result = add_instruction(
				spec_constant ? spv::OpSpecConstant : spv::OpConstant,
				convert_type(type), _types_and_constants).add(data.as_uint[0]).result;
		}

		if (!spec_constant)
			_constant_lookup.push_back({ type, data, result });

		return result;
	}

	id   emit_unary_op(const location &loc, tokenid op, const type &type, id val) override
	{
		spv::Op spv_op = spv::OpNop;

		switch (op)
		{
		case tokenid::minus:
			spv_op = type.is_floating_point() ? spv::OpFNegate : spv::OpSNegate;
			break;
		case tokenid::tilde:
			spv_op = spv::OpNot;
			break;
		case tokenid::exclaim:
			spv_op = spv::OpLogicalNot;
			break;
		default:
			return assert(false), 0;
		}

		add_location(loc, *_current_block_data);

		const spv::Id result = add_instruction(spv_op, convert_type(type))
			.add(val) // Operand
			.result; // Result ID

		return result;
	}
	id   emit_binary_op(const location &loc, tokenid op, const type &res_type, const type &type, id lhs, id rhs) override
	{
		spv::Op spv_op = spv::OpNop;

		switch (op)
		{
		case tokenid::plus:
		case tokenid::plus_plus:
		case tokenid::plus_equal:
			spv_op = type.is_floating_point() ? spv::OpFAdd : spv::OpIAdd;
			break;
		case tokenid::minus:
		case tokenid::minus_minus:
		case tokenid::minus_equal:
			spv_op = type.is_floating_point() ? spv::OpFSub : spv::OpISub;
			break;
		case tokenid::star:
		case tokenid::star_equal:
			spv_op = type.is_floating_point() ? spv::OpFMul : spv::OpIMul;
			break;
		case tokenid::slash:
		case tokenid::slash_equal:
			spv_op = type.is_floating_point() ? spv::OpFDiv : type.is_signed() ? spv::OpSDiv : spv::OpUDiv;
			break;
		case tokenid::percent:
		case tokenid::percent_equal:
			spv_op = type.is_floating_point() ? spv::OpFRem : type.is_signed() ? spv::OpSRem : spv::OpUMod;
			break;
		case tokenid::caret:
		case tokenid::caret_equal:
			spv_op = spv::OpBitwiseXor;
			break;
		case tokenid::pipe:
		case tokenid::pipe_equal:
			spv_op = spv::OpBitwiseOr;
			break;
		case tokenid::ampersand:
		case tokenid::ampersand_equal:
			spv_op = spv::OpBitwiseAnd;
			break;
		case tokenid::less_less:
		case tokenid::less_less_equal:
			spv_op = spv::OpShiftLeftLogical;
			break;
		case tokenid::greater_greater:
		case tokenid::greater_greater_equal:
			spv_op = type.is_signed() ? spv::OpShiftRightArithmetic : spv::OpShiftRightLogical;
			break;
		case tokenid::pipe_pipe:
			spv_op = spv::OpLogicalOr;
			break;
		case tokenid::ampersand_ampersand:
			spv_op = spv::OpLogicalAnd;
			break;
		case tokenid::less:
			spv_op = type.is_floating_point() ? spv::OpFOrdLessThan : type.is_signed() ? spv::OpSLessThan : spv::OpULessThan;
			break;
		case tokenid::less_equal:
			spv_op = type.is_floating_point() ? spv::OpFOrdLessThanEqual : type.is_signed() ? spv::OpSLessThanEqual : spv::OpULessThanEqual;
			break;
		case tokenid::greater:
			spv_op = type.is_floating_point() ? spv::OpFOrdGreaterThan : type.is_signed() ? spv::OpSGreaterThan : spv::OpUGreaterThan;
			break;
		case tokenid::greater_equal:
			spv_op = type.is_floating_point() ? spv::OpFOrdGreaterThanEqual : type.is_signed() ? spv::OpSGreaterThanEqual : spv::OpUGreaterThanEqual;
			break;
		case tokenid::equal_equal:
			spv_op = type.is_floating_point() ? spv::OpFOrdEqual : type.is_integral() ? spv::OpIEqual : spv::OpLogicalEqual;
			break;
		case tokenid::exclaim_equal:
			spv_op = type.is_integral() ? spv::OpINotEqual : type.is_floating_point() ? spv::OpFOrdNotEqual : spv::OpLogicalNotEqual;
			break;
		default:
			return assert(false), 0;
		}

		add_location(loc, *_current_block_data);

		const spv::Id result = add_instruction(spv_op, convert_type(res_type))
			.add(lhs) // Operand 1
			.add(rhs) // Operand 2
			.result; // Result ID

		if (res_type.has(type::q_precise))
			add_decoration(result, spv::DecorationNoContraction);

		return result;
	}
	id   emit_ternary_op(const location &loc, tokenid op, const type &type, id condition, id true_value, id false_value) override
	{
		assert(op == tokenid::question);

		add_location(loc, *_current_block_data);

		const spv::Id result = add_instruction(spv::OpSelect, convert_type(type))
			.add(condition) // Condition
			.add(true_value) // Object 1
			.add(false_value) // Object 2
			.result; // Result ID

		return result;
	}
	id   emit_call(const location &loc, id function, const type &res_type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);

		add_location(loc, *_current_block_data);

		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpFunctionCall
		spirv_instruction &call = add_instruction(spv::OpFunctionCall, convert_type(res_type))
			.add(function); // Function
		for (size_t i = 0; i < args.size(); ++i)
			call.add(args[i].base); // Arguments

		return call.result;
	}
	id   emit_call_intrinsic(const location &loc, id intrinsic, const type &res_type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert(arg.chain.empty() && arg.base != 0);

		add_location(loc, *_current_block_data);

		enum
		{
#define IMPLEMENT_INTRINSIC_SPIRV(name, i, code) name##i,
#include "effect_symbol_table_intrinsics.inl"
		};

		switch (intrinsic)
		{
#define IMPLEMENT_INTRINSIC_SPIRV(name, i, code) case name##i: code
#include "effect_symbol_table_intrinsics.inl"
		default:
			return 0;
		}
	}
	id   emit_construct(const location &loc, const type &type, const std::vector<expression> &args) override
	{
		for (const auto &arg : args)
			assert((arg.type.is_scalar() || type.is_array()) && arg.chain.empty() && arg.base != 0);

		add_location(loc, *_current_block_data);

		std::vector<spv::Id> ids;
		ids.reserve(args.size());

		// There must be exactly one constituent for each top-level component of the result
		if (type.is_matrix())
		{
			assert(type.rows == type.cols);

			// Second, turn that list of scalars into a list of column vectors
			for (size_t i = 0, j = 0; i < args.size(); i += type.rows, ++j)
			{
				auto vector_type = type;
				vector_type.cols = 1;

				spirv_instruction &node = add_instruction(spv::OpCompositeConstruct, convert_type(vector_type));
				for (unsigned int k = 0; k < type.rows; ++k)
					node.add(args[i + k].base);

				ids.push_back(node.result);
			}

			ids.erase(ids.begin() + type.cols, ids.end());
		}
		// The exception is that for constructing a vector, a contiguous subset of the scalars consumed can be represented by a vector operand instead
		else
		{
			assert(type.is_vector() || type.is_array());

			for (const auto &arg : args)
				ids.push_back(arg.base);
		}

		return add_instruction(spv::OpCompositeConstruct, convert_type(type))
			.add(ids.begin(), ids.end())
			.result;
	}

	void emit_if(const location &loc, id, id condition_block, id true_statement_block, id false_statement_block, unsigned int selection_control) override
	{
		spirv_instruction merge_label = _current_block_data->instructions.back();
		assert(merge_label.op == spv::OpLabel);
		_current_block_data->instructions.pop_back();

		// Add previous block containing the condition value first
		_current_block_data->append(_block_data[condition_block]);

		spirv_instruction branch_inst = _current_block_data->instructions.back();
		assert(branch_inst.op == spv::OpBranchConditional);
		_current_block_data->instructions.pop_back();

		// Add structured control flow instruction
		add_location(loc, *_current_block_data);
		add_instruction_without_result(spv::OpSelectionMerge)
			.add(merge_label.result)
			.add(selection_control); // 'SelectionControl' happens to match the flags produced by the parser

		// Append all blocks belonging to the branch
		_current_block_data->instructions.push_back(branch_inst);
		_current_block_data->append(_block_data[true_statement_block]);
		_current_block_data->append(_block_data[false_statement_block]);

		_current_block_data->instructions.push_back(merge_label);
	}
	id   emit_phi(const location &loc, id, id condition_block, id true_value, id true_statement_block, id false_value, id false_statement_block, const type &type) override
	{
		spirv_instruction merge_label = _current_block_data->instructions.back();
		assert(merge_label.op == spv::OpLabel);
		_current_block_data->instructions.pop_back();

		// Add previous block containing the condition value first
		_current_block_data->append(_block_data[condition_block]);

		if (true_statement_block != condition_block)
			_current_block_data->append(_block_data[true_statement_block]);
		if (false_statement_block != condition_block)
			_current_block_data->append(_block_data[false_statement_block]);

		_current_block_data->instructions.push_back(merge_label);

		add_location(loc, *_current_block_data);

		// https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#OpPhi
		const spv::Id result = add_instruction(spv::OpPhi, convert_type(type))
			.add(true_value) // Variable 0
			.add(true_statement_block) // Parent 0
			.add(false_value) // Variable 1
			.add(false_statement_block) // Parent 1
			.result;

		return result;
	}
	void emit_loop(const location &loc, id, id prev_block, id header_block, id condition_block, id loop_block, id continue_block, unsigned int loop_control) override
	{
		spirv_instruction merge_label = _current_block_data->instructions.back();
		assert(merge_label.op == spv::OpLabel);
		_current_block_data->instructions.pop_back();

		// Add previous block first
		_current_block_data->append(_block_data[prev_block]);

		// Fill header block
		assert(_block_data[header_block].instructions.size() == 2);
		_current_block_data->instructions.push_back(_block_data[header_block].instructions[0]);
		assert(_current_block_data->instructions.back().op == spv::OpLabel);

		// Add structured control flow instruction
		add_location(loc, *_current_block_data);
		add_instruction_without_result(spv::OpLoopMerge)
			.add(merge_label.result)
			.add(continue_block)
			.add(loop_control); // 'LoopControl' happens to match the flags produced by the parser

		_current_block_data->instructions.push_back(_block_data[header_block].instructions[1]);
		assert(_current_block_data->instructions.back().op == spv::OpBranch);

		// Add condition block if it exists
		if (condition_block != 0)
			_current_block_data->append(_block_data[condition_block]);

		// Append loop body block before continue block
		_current_block_data->append(_block_data[loop_block]);
		_current_block_data->append(_block_data[continue_block]);

		_current_block_data->instructions.push_back(merge_label);
	}
	void emit_switch(const location &loc, id, id selector_block, id default_label, const std::vector<id> &case_literal_and_labels, unsigned int selection_control) override
	{
		spirv_instruction merge_label = _current_block_data->instructions.back();
		assert(merge_label.op == spv::OpLabel);
		_current_block_data->instructions.pop_back();

		// Add previous block containing the selector value first
		_current_block_data->append(_block_data[selector_block]);

		spirv_instruction switch_inst = _current_block_data->instructions.back();
		assert(switch_inst.op == spv::OpSwitch);
		_current_block_data->instructions.pop_back();

		// Add structured control flow instruction
		add_location(loc, *_current_block_data);
		add_instruction_without_result(spv::OpSelectionMerge)
			.add(merge_label.result)
			.add(selection_control); // 'SelectionControl' happens to match the flags produced by the parser

		// Update switch instruction to contain all case labels
		switch_inst.operands[1] = default_label;
		switch_inst.add(case_literal_and_labels.begin(), case_literal_and_labels.end());

		// Append all blocks belonging to the switch
		_current_block_data->instructions.push_back(switch_inst);
		for (size_t i = 0; i < case_literal_and_labels.size(); i += 2)
			_current_block_data->append(_block_data[case_literal_and_labels[i + 1]]);
		if (default_label != merge_label.result)
			_current_block_data->append(_block_data[default_label]);

		_current_block_data->instructions.push_back(merge_label);
	}

	bool is_in_block() const override { return _current_block != 0; }
	bool is_in_function() const override { return _current_function != nullptr; }

	id   set_block(id id) override
	{
		_last_block = _current_block;
		_current_block = id;
		_current_block_data = &_block_data[id];

		return _last_block;
	}
	void enter_block(id id) override
	{
		assert(id != 0);
		// Can only use labels inside functions and should never be in another basic block if creating a new one
		assert(is_in_function() && !is_in_block());

		set_block(id);

		add_instruction_without_result(spv::OpLabel)
			.result = id;
	}
	id   leave_block_and_kill() override
	{
		assert(is_in_function()); // Can only discard inside functions

		if (!is_in_block())
			return 0;

		add_instruction_without_result(spv::OpKill);

		return set_block(0);
	}
	id   leave_block_and_return(id value) override
	{
		assert(is_in_function()); // Can only return from inside functions

		if (!is_in_block()) // Might already have left the last block in which case this has to be ignored
			return 0;

		if (_current_function->return_type.is_void())
		{
			add_instruction_without_result(spv::OpReturn);
		}
		else
		{
			if (value == 0) // The implicit return statement needs this
				value = add_instruction(spv::OpUndef, convert_type(_current_function->return_type), _types_and_constants).result;

			add_instruction_without_result(spv::OpReturnValue)
				.add(value);
		}

		return set_block(0);
	}
	id   leave_block_and_switch(id value, id default_target) override
	{
		assert(value != 0 && default_target != 0);
		assert(is_in_function()); // Can only switch inside functions

		if (!is_in_block())
			return _last_block;

		add_instruction_without_result(spv::OpSwitch)
			.add(value)
			.add(default_target);

		return set_block(0);
	}
	id   leave_block_and_branch(id target, unsigned int) override
	{
		assert(target != 0);
		assert(is_in_function()); // Can only branch inside functions

		if (!is_in_block())
			return _last_block;

		add_instruction_without_result(spv::OpBranch)
			.add(target);

		return set_block(0);
	}
	id   leave_block_and_branch_conditional(id condition, id true_target, id false_target) override
	{
		assert(condition != 0 && true_target != 0 && false_target != 0);
		assert(is_in_function()); // Can only branch inside functions

		if (!is_in_block())
			return _last_block;

		add_instruction_without_result(spv::OpBranchConditional)
			.add(condition)
			.add(true_target)
			.add(false_target);

		return set_block(0);
	}
	void leave_function() override
	{
		assert(is_in_function()); // Can only leave if there was a function to begin with

		_current_function->definition = _block_data[_last_block];

		// Append function end instruction
		add_instruction_without_result(spv::OpFunctionEnd, _current_function->definition);

		_current_function = nullptr;
	}
};

codegen *create_codegen_spirv(bool debug_info, bool uniforms_to_spec_constants)
{
	return new codegen_spirv(debug_info, uniforms_to_spec_constants);
}
