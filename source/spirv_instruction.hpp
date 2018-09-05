/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>
// Use the C++ variant of the SPIR-V headers
#include <spirv.hpp>

namespace reshadefx
{
	/// <summary>
	/// A single instruction in a SPIR-V module
	/// </summary>
	struct spirv_instruction
	{
		// See: https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html
		// 0             | Opcode: The 16 high-order bits are the WordCount of the instruction. The 16 low-order bits are the opcode enumerant.
		// 1             | Optional instruction type <id>
		// .             | Optional instruction Result <id>
		// .             | Operand 1 (if needed)
		// .             | Operand 2 (if needed)
		// ...           | ...
		// WordCount - 1 | Operand N (N is determined by WordCount minus the 1 to 3 words used for the opcode, instruction type <id>, and instruction Result <id>).

		spv::Op op;
		spv::Id type;
		spv::Id result;
		std::vector<spv::Id> operands;

		explicit spirv_instruction(spv::Op op = spv::OpNop) : op(op), type(0), result(0) { }
		spirv_instruction(spv::Op op, spv::Id result) :  op(op), type(result), result(0) { }
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
		/// Add a null-terminated literal string to the instruction.
		/// </summary>
		/// <param name="string"></param>
		/// <returns></returns>
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
}
