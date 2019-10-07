/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "effect_parser.hpp"
#include <vector>
#include <functional>

namespace reshadefx
{
	/// <summary>
	/// A parser for the ReShade FX shader language.
	/// </summary>
	class effect_parser_injector
	{
	public:
		void subscribe_uniform_definition(std::function<void(reshadefx::uniform_info &)> function) { _uniform_info_callables.push_back(function); }
		void emit_uniform_definition(reshadefx::uniform_info &uniform_info) { for (auto &callback : _uniform_info_callables) { callback(uniform_info); } }

	private:
		std::vector<std::function<void(reshadefx::uniform_info &)>> _uniform_info_callables;
	};
}
