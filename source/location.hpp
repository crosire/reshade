#pragma once

#include <string>

namespace reshade
{
	namespace fx
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
}
