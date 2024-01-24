/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <vector>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>

/// <summary>
/// An instance of this is automatically created for all devices and can be queried with <c>device->get_private_data&lt;descriptor_tracking&gt;()</c> (assuming descriptor tracking was registered via <see cref="descriptor_tracking::register_events"/>).
/// </summary>
class __declspec(uuid("f02b21ba-f5dd-44af-9e90-c54535a98ce3")) descriptor_tracking
{
public:
	/// <summary>
	/// Registers all the necessary add-on events for descriptor tracking to work.
	/// </summary>
	static void register_events();
	/// <summary>
	/// Unregisters all the necessary add-on events for descriptor tracking to work.
	/// </summary>
	static void unregister_events();

	/// <summary>
	/// Gets the buffer range in a descriptor table at the specified offset.
	/// </summary>
	reshade::api::buffer_range get_buffer_range(reshade::api::descriptor_heap heap, uint32_t offset) const;

	/// <summary>
	/// Gets the sampler in a descriptor table at the specified offset.
	/// </summary>
	reshade::api::sampler get_sampler(reshade::api::descriptor_heap heap, uint32_t offset) const;
	/// <summary>
	/// Gets the resource view in a descriptor table at the specified offset.
	/// </summary>
	reshade::api::resource_view get_resource_view(reshade::api::descriptor_heap heap, uint32_t offset) const;

	/// <summary>
	/// Gets the description that was used to create the specified pipeline layout parameter.
	/// </summary>
	reshade::api::pipeline_layout_param get_pipeline_layout_param(reshade::api::pipeline_layout layout, uint32_t param) const;

private:
	void register_pipeline_layout(reshade::api::pipeline_layout layout, uint32_t count, const reshade::api::pipeline_layout_param *params);
	void unregister_pipeline_layout(reshade::api::pipeline_layout layout);

	static void on_init_pipeline_layout(reshade::api::device *device, uint32_t count, const reshade::api::pipeline_layout_param *params, reshade::api::pipeline_layout layout);
	static void on_destroy_pipeline_layout(reshade::api::device *device, reshade::api::pipeline_layout layout);

	static bool on_copy_descriptor_tables(reshade::api::device *device, uint32_t count, const reshade::api::descriptor_table_copy *copies);
	static bool on_update_descriptor_tables(reshade::api::device *device, uint32_t count, const reshade::api::descriptor_table_update *updates);

	struct descriptor_data
	{
		descriptor_data() : b() {}

		union
		{
			reshade::api::buffer_range b;
			reshade::api::sampler_with_resource_view t;
		};
	};
	struct descriptor_heap_data
	{
		concurrency::concurrent_vector<std::pair<reshade::api::descriptor_type, descriptor_data>> descriptors;
	};
	struct descriptor_heap_hash : std::hash<uint64_t>
	{
		size_t operator()(reshade::api::descriptor_heap handle) const
		{
			return std::hash<uint64_t>::operator()(handle.handle);
		}
	};

	struct pipeline_layout_data
	{
		std::vector<reshade::api::pipeline_layout_param> params;
		std::vector<std::vector<reshade::api::descriptor_range>> ranges;
	};
	struct pipeline_layout_hash : std::hash<uint64_t>
	{
		size_t operator()(reshade::api::pipeline_layout handle) const
		{
			return std::hash<uint64_t>::operator()(handle.handle);
		}
	};

	concurrency::concurrent_unordered_map<reshade::api::descriptor_heap, descriptor_heap_data, descriptor_heap_hash> heaps;
	concurrency::concurrent_unordered_map<reshade::api::pipeline_layout, pipeline_layout_data, pipeline_layout_hash> layouts;
};
