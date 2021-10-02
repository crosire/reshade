/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vector>

struct descriptor_pool_data
{
	std::vector<reshade::api::resource_view> descriptors;
};

struct descriptor_set_tracking
{
	static constexpr uint8_t GUID[16] = { 0x33, 0x31, 0x9e, 0x83, 0x38, 0x7c, 0x44, 0x8e, 0x88, 0x1c, 0x7e, 0x68, 0xfc, 0x2e, 0x52, 0xc4 };

	reshade::api::resource_view lookup_descriptor(reshade::api::descriptor_pool pool, uint32_t offset);

	std::map<reshade::api::descriptor_pool, descriptor_pool_data> pools;
};
