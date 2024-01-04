/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api.hpp"
#include <string>
#include <vector>

#if RESHADE_ADDON

namespace reshade
{
	struct addon_info
	{
		struct overlay_callback
		{
			std::string title;
			void(*callback)(api::effect_runtime *) = nullptr;
		};

		void *handle = nullptr;
		std::string name;
		std::string description;
		std::string file;
		std::string author;
		uint32_t version = 1;

		std::vector<std::pair<uint32_t, void *>> event_callbacks;
#if RESHADE_GUI
		void(*settings_overlay_callback)(api::effect_runtime *) = nullptr;
		std::vector<overlay_callback> overlay_callbacks;
#endif
	};
}

#endif
