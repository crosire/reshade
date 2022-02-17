/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "reshade.hpp"
#include "descriptor_set_tracking.hpp"

using namespace reshade::api;

resource_view descriptor_set_tracking::get_shader_resource_view(descriptor_pool pool, uint32_t offset) const
{
	const std::shared_lock<std::shared_mutex> lock(mutex);

	const descriptor_pool_data &pool_data = pools.at(pool);

	if (offset < pool_data.descriptors.size() && pool_data.descriptors[offset].first == descriptor_type::shader_resource_view)
		return { pool_data.descriptors[offset].second };
	else
		return { 0 };
}

pipeline_layout_param descriptor_set_tracking::get_pipeline_layout_param(pipeline_layout layout, uint32_t param) const
{
	const std::shared_lock<std::shared_mutex> lock(mutex);

	const pipeline_layout_data &layout_data = layouts.at(layout);

	if (param < layout_data.params.size())
		return layout_data.params[param];
	else
		return pipeline_layout_param();
}

void descriptor_set_tracking::register_pipeline_layout(pipeline_layout layout, uint32_t count, const pipeline_layout_param *params)
{
	const std::unique_lock<std::shared_mutex> lock(mutex);

	pipeline_layout_data &layout_data = layouts[layout];
	layout_data.params.assign(params, params + count);
	layout_data.ranges.resize(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (params[i].type == pipeline_layout_param_type::descriptor_set)
		{
			layout_data.ranges[i].assign(params[i].descriptor_set.ranges, params[i].descriptor_set.ranges + params[i].descriptor_set.count);
			layout_data.params[i].descriptor_set.ranges = layout_data.ranges[i].data();
		}
	}
}
void descriptor_set_tracking::unregister_pipeline_layout(pipeline_layout layout)
{
	const std::unique_lock<std::shared_mutex> lock(mutex);

	layouts.erase(layout);
}

static void on_init_device(device *device)
{
	device->create_private_data<descriptor_set_tracking>();
}
static void on_destroy_device(device *device)
{
	device->destroy_private_data<descriptor_set_tracking>();
}

static void on_init_pipeline_layout(device *device, uint32_t count, const pipeline_layout_param *params, pipeline_layout layout)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();
	ctx.register_pipeline_layout(layout, count, params);
}
static void on_destroy_pipeline_layout(device *device, pipeline_layout layout)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();
	ctx.unregister_pipeline_layout(layout);
}

static bool on_copy_descriptor_sets(device *device, uint32_t count, const descriptor_set_copy *copies)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();

	const std::unique_lock<std::shared_mutex> lock(ctx.mutex);

	for (uint32_t i = 0; i < count; ++i)
	{
		const descriptor_set_copy &copy = copies[i];

		uint32_t src_offset = 0;
		descriptor_pool src_pool = { 0 };
		device->get_descriptor_pool_offset(copy.source_set, copy.source_binding, copy.source_array_offset, &src_pool, &src_offset);

		uint32_t dst_offset = 0;
		descriptor_pool dst_pool = { 0 };
		device->get_descriptor_pool_offset(copy.dest_set, copy.dest_binding, copy.dest_array_offset, &dst_pool, &dst_offset);

		descriptor_set_tracking::descriptor_pool_data &src_pool_data = ctx.pools[src_pool];
		descriptor_set_tracking::descriptor_pool_data &dst_pool_data = ctx.pools[dst_pool];

		if (dst_offset + copy.count > dst_pool_data.descriptors.size())
			dst_pool_data.descriptors.resize(dst_offset + copy.count);

		for (uint32_t k = 0; k < copy.count; ++k)
		{
			dst_pool_data.descriptors[dst_offset + k] = src_pool_data.descriptors[src_offset + k];
		}
	}

	return false;
}

static bool on_update_descriptor_sets(device *device, uint32_t count, const descriptor_set_update *updates)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();

	const std::unique_lock<std::shared_mutex> lock(ctx.mutex);

	for (uint32_t i = 0; i < count; ++i)
	{
		const descriptor_set_update &update = updates[i];

		uint32_t offset = 0;
		descriptor_pool pool = { 0 };
		device->get_descriptor_pool_offset(update.set, update.binding, update.array_offset, &pool, &offset);

		descriptor_set_tracking::descriptor_pool_data &pool_data = ctx.pools[pool];

		if (offset + update.count > pool_data.descriptors.size())
			pool_data.descriptors.resize(offset + update.count);

		for (uint32_t k = 0; k < update.count; ++k)
		{
			pool_data.descriptors[offset + k].first = update.type;

			if (update.type == descriptor_type::sampler)
				pool_data.descriptors[offset + k].second = static_cast<const sampler *>(update.descriptors)[k].handle;
			else if (update.type == descriptor_type::shader_resource_view || update.type == descriptor_type::unordered_access_view)
				pool_data.descriptors[offset + k].second = static_cast<const resource_view *>(update.descriptors)[k].handle;
		}
	}

	return false;
}

void register_descriptor_set_tracking()
{
	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::init_pipeline_layout>(on_init_pipeline_layout);
	reshade::register_event<reshade::addon_event::destroy_pipeline_layout>(on_destroy_pipeline_layout);
	reshade::register_event<reshade::addon_event::copy_descriptor_sets>(on_copy_descriptor_sets);
	reshade::register_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
void unregister_descriptor_set_tracking()
{
	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::init_pipeline_layout>(on_init_pipeline_layout);
	reshade::unregister_event<reshade::addon_event::destroy_pipeline_layout>(on_destroy_pipeline_layout);
	reshade::unregister_event<reshade::addon_event::copy_descriptor_sets>(on_copy_descriptor_sets);
	reshade::unregister_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
