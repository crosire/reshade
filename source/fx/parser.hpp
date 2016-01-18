#pragma once

#include "ast.hpp"

namespace reshade
{
	namespace fx
	{
		bool parse(const std::string &source, nodetree &ast, std::string &errors);
	}
}
