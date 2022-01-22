/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <map>
#include <vector>
#include <shared_mutex>

struct __declspec(uuid("33319e83-387c-448e-881c-7e68fc2e52c4")) descriptor_set_tracking
{
	reshade::api::resource_view get_shader_resource_view(reshade::api::descriptor_pool pool, uint32_t offset) const;

	reshade::api::pipeline_layout_param get_pipeline_layout_param(reshade::api::pipeline_layout layout, uint32_t param) const;

	void register_pipeline_layout(reshade::api::pipeline_layout layout, uint32_t count, const reshade::api::pipeline_layout_param *params);
	void unregister_pipeline_layout(reshade::api::pipeline_layout layout);

	struct pipeline_layout_data
	{
		std::vector<reshade::api::pipeline_layout_param> params;
		std::vector<std::vector<reshade::api::descriptor_range>> ranges;
	};
	struct descriptor_pool_data
	{
		std::vector<std::pair<reshade::api::descriptor_type, uint64_t>> descriptors;
	};

	mutable std::shared_mutex mutex;
	std::map<reshade::api::descriptor_pool, descriptor_pool_data> pools;

private:
	std::map<reshade::api::pipeline_layout, pipeline_layout_data> layouts;
};

extern void register_descriptor_set_tracking();
extern void unregister_descriptor_set_tracking();
