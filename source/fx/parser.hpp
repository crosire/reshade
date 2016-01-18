#pragma once

#include <string>

namespace reshade
{
	namespace fx
	{
		class nodetree;

		bool parse(const std::string &source, nodetree &ast, std::string &errors);
	}
}
