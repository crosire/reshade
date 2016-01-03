#pragma once

#include "ParserNodes.hpp"

namespace ReShade
{
	namespace FX
	{
		bool parse(const std::string &source, nodetree &ast, std::string &errors);
	}
}
