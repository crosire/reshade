#pragma once

#include "EffectParserTree.hpp"

namespace ReShade
{
	EffectTree::Index OptimizeExpression(EffectTree &ast, EffectTree::Index expression);
}