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
		std::string website_url;
		std::string issues_url;
		union version
		{
			struct number
			{
				uint16_t major;
				uint16_t minor;
				uint16_t build;
				uint16_t revision;
			} number;
			uint64_t value;
		} version = {};
		bool external = true;

		std::vector<std::pair<uint32_t, void *>> event_callbacks;
#if RESHADE_GUI
		void(*settings_overlay_callback)(api::effect_runtime *) = nullptr;
		std::vector<overlay_callback> overlay_callbacks;
#endif
	};
}

#endif
