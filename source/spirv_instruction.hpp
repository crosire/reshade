/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "source_location.hpp"
#include <vector>
#include <spirv.hpp>

namespace reshadefx
{
	struct spv_instruction
	{
		spv::Op op;
		spv::Id type = 0;
		spv::Id result = 0;
		std::vector<spv::Id> operands;
		location location = {};

		explicit spv_instruction(spv::Op op = spv::OpNop) : op(op) { }
		spv_instruction(spv::Op op, spv::Id result) : op(op), type(result) { }
		spv_instruction(spv::Op op, spv::Id type, spv::Id result) : op(op), type(type), result(result) { }

		spv_instruction &add(spv::Id operand)
		{
			operands.push_back(operand);
			return *this;
		}
		template <typename It>
		spv_instruction &add(It begin, It end)
		{
			operands.insert(operands.end(), begin, end);
			return *this;
		}

		spv_instruction &add_string(const char *string)
		{
			uint32_t word = 0;

			do {
				word = 0;
				for (uint32_t i = 0; i < 4 && *string; ++i)
					reinterpret_cast<uint8_t *>(&word)[i] = *string++;
				add(word);
			} while (*string || word & 0xFF000000);

			return *this;
		}
	};

	struct spv_basic_block
	{
		std::vector<spv_instruction> instructions;
	};
}
