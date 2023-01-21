/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <map>
#include <vector>
#include <shared_mutex>

 /// <summary>
 /// An instance of this is automatically created for all devices and can be queried with <c>device->get_private_data&lt;descriptor_tracking&gt;()</c> (assuming descriptor tracking was registered via <c>register_descriptor_tracking</c>).
 /// </summary>
struct __declspec(uuid("33319e83-387c-448e-881c-7e68fc2e52c4")) descriptor_tracking
{
	/// <summary>
	/// Gets the shader resource view in a descriptor set at the specified offset.
	/// </summary>
	reshade::api::resource_view get_shader_resource_view(reshade::api::descriptor_pool pool, uint32_t offset) const;

	/// <summary>
	/// Gets the description that was used to create the specified pipeline layout parameter.
	/// </summary>
	reshade::api::pipeline_layout_param get_pipeline_layout_param(reshade::api::pipeline_layout layout, uint32_t param) const;

public /* internal */:
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

void register_descriptor_tracking();
void unregister_descriptor_tracking();
