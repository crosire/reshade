/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade.hpp"
#include "descriptor_set_tracking.hpp"

reshade::api::resource_view descriptor_set_tracking::lookup_descriptor(reshade::api::descriptor_pool pool, uint32_t offset)
{
	const std::shared_lock<std::shared_mutex> lock(mutex);

	auto &pool_data = pools.at(pool);

	if (offset >= pool_data.descriptors.size())
		return {};

	return { pool_data.descriptors[offset].second };
}

static void on_init_device(reshade::api::device *device)
{
	device->create_private_data<descriptor_set_tracking>();
}
static void on_destroy_device(reshade::api::device *device)
{
	device->destroy_private_data<descriptor_set_tracking>();
}

static bool on_copy_descriptor_sets(reshade::api::device *device, uint32_t count, const reshade::api::descriptor_set_copy *copies)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();

	const std::unique_lock<std::shared_mutex> lock(ctx.mutex);

	for (uint32_t i = 0; i < count; ++i)
	{
		const reshade::api::descriptor_set_copy &copy = copies[i];

		uint32_t src_offset = 0;
		reshade::api::descriptor_pool src_pool = { 0 };
		device->get_descriptor_pool_offset(copy.source_set, copy.source_binding, copy.source_array_offset, &src_pool, &src_offset);

		uint32_t dst_offset = 0;
		reshade::api::descriptor_pool dst_pool = { 0 };
		device->get_descriptor_pool_offset(copy.dest_set, copy.dest_binding, copy.dest_array_offset, &dst_pool, &dst_offset);

		descriptor_pool_data &src_pool_data = ctx.pools[src_pool];
		descriptor_pool_data &dst_pool_data = ctx.pools[dst_pool];

		if (dst_offset + copy.count > dst_pool_data.descriptors.size())
			dst_pool_data.descriptors.resize(dst_offset + copy.count);

		for (uint32_t k = 0; k < copy.count; ++k)
		{
			dst_pool_data.descriptors[dst_offset + k] = src_pool_data.descriptors[src_offset + k];
		}
	}

	return false;
}

static bool on_update_descriptor_sets(reshade::api::device *device, uint32_t count, const reshade::api::descriptor_set_update *updates)
{
	descriptor_set_tracking &ctx = device->get_private_data<descriptor_set_tracking>();

	const std::unique_lock<std::shared_mutex> lock(ctx.mutex);

	for (uint32_t i = 0; i < count; ++i)
	{
		const reshade::api::descriptor_set_update &update = updates[i];

		uint32_t offset = 0;
		reshade::api::descriptor_pool pool = { 0 };
		device->get_descriptor_pool_offset(update.set, update.binding, update.array_offset, &pool, &offset);

		descriptor_pool_data &pool_data = ctx.pools[pool];

		if (offset + update.count > pool_data.descriptors.size())
			pool_data.descriptors.resize(offset + update.count);

		for (uint32_t k = 0; k < update.count; ++k)
		{
			pool_data.descriptors[offset + k].first = update.type;

			if (update.type == reshade::api::descriptor_type::sampler)		
				pool_data.descriptors[offset + k].second = static_cast<const reshade::api::sampler *>(update.descriptors)[k].handle;
			else if (update.type == reshade::api::descriptor_type::shader_resource_view)		
				pool_data.descriptors[offset + k].second = static_cast<const reshade::api::resource_view *>(update.descriptors)[k].handle;
		}
	}

	return false;
}

void register_addon_descriptor_set_tracking()
{
	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::copy_descriptor_sets>(on_copy_descriptor_sets);
	reshade::register_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
void unregister_addon_descriptor_set_tracking()
{
	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::copy_descriptor_sets>(on_copy_descriptor_sets);
	reshade::unregister_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
