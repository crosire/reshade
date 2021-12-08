/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vector>
#include <shared_mutex>

struct descriptor_pool_data
{
	std::vector<reshade::api::resource_view> descriptors;
};

struct __declspec(uuid("33319e83-387c-448e-881c-7e68fc2e52c4")) descriptor_set_tracking
{
	reshade::api::resource_view lookup_descriptor(reshade::api::descriptor_pool pool, uint32_t offset);

	std::map<reshade::api::descriptor_pool, descriptor_pool_data> pools;
	std::shared_mutex mutex;
};
