/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <string>

namespace reshadefx
{
	struct location
	{
		location() : line(1), column(1) { }
		explicit location(unsigned int line, unsigned int column = 1) : line(line), column(column) { }
		explicit location(const std::string &source, unsigned int line, unsigned int column = 1) : source(source), line(line), column(column) { }

		std::string source;
		unsigned int line, column;
	};
}
