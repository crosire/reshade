/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::vulkan
{
	void work_around_amd_driver_bug(std::vector<uint32_t> &spirv, const std::string &entry_point)
	{
		/* The AMD driver has a really hard time with SPIR-V modules that have multiple entry points.
		 * Trying to create a graphics pipeline using a shader module created from such a SPIR-V module
		 * tends to just fail with a generic VK_ERROR_OUT_OF_HOST_MEMORY.
		 * This is a pretty unpleasant driver bug, but until fixed, this function rewrites a given
		 * SPIR-V module and removes all but a single entry point (and associated functions/variables).
		 */

		const auto extract_string = [](const std::vector<uint32_t> &spirv, size_t &offset)
		{
			std::string ret;
			while (offset < spirv.size())
				for (uint32_t i = 0, word = spirv[offset++]; i < 4; ++i, word >>= 8)
				{
					const char c = (word & 0xFF);
					if (c == '\0')
						return ret;
					ret += c;
				}
			return std::string();
		};

		size_t current_function_offset = 0;
		uint32_t current_function = 0;
		std::vector<uint32_t> functions_to_remove;
		std::vector<uint32_t> variables_to_remove;

		for (size_t i = 5 /* Skip SPIR-V header information */; i < spirv.size();)
		{
			const uint32_t op = spirv[i] & 0xFFFF;
			const uint32_t len = (spirv[i] >> 16) & 0xFFFF;

			switch (op)
			{
			case 15: // OpEntryPoint
				// Look for any non-matching entry points
				if (size_t offset = i + 3; extract_string(spirv, offset) != entry_point)
				{
					functions_to_remove.push_back(spirv[i + 2]);

					// Get interface variables
					for (++offset; offset < i + len; ++offset)
						variables_to_remove.push_back(spirv[offset]);

					// Remove this entry point from the module
					spirv.erase(spirv.begin() + i, spirv.begin() + i + len);
					continue;
				}
				break;
			case 16: // OpExecutionMode
				if (std::find(functions_to_remove.begin(), functions_to_remove.end(), spirv[i + 1]) != functions_to_remove.end())
				{
					spirv.erase(spirv.begin() + i, spirv.begin() + i + len);
					continue;
				}
				break;
			case 59: // OpVariable
				// Remove all declarations of the interface variables for non-matching entry points
				if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[i + 2]) != variables_to_remove.end())
				{
					spirv.erase(spirv.begin() + i, spirv.begin() + i + len);
					continue;
				}
				break;
			case 71: // OpDecorate
				// Remove all decorations targeting any of the interface variables for non-matching entry points
				if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[i + 1]) != variables_to_remove.end())
				{
					spirv.erase(spirv.begin() + i, spirv.begin() + i + len);
					continue;
				}
				break;
			case 54: // OpFunction
				current_function = spirv[i + 2];
				current_function_offset = i;
				break;
			case 56: // OpFunctionEnd
				// Remove all function definitions for non-matching entry points
				if (std::find(functions_to_remove.begin(), functions_to_remove.end(), current_function) != functions_to_remove.end())
				{
					spirv.erase(spirv.begin() + current_function_offset, spirv.begin() + i + len);
					i = current_function_offset;
					continue;
				}
				break;
			}

			i += len;
		}
	}
}
