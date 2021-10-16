/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "reshade.hpp"
#include "descriptor_set_tracking.hpp"

reshade::api::resource_view descriptor_set_tracking::lookup_descriptor(reshade::api::descriptor_pool pool, uint32_t offset)
{
	const std::shared_lock<std::shared_mutex> lock(mutex);

	if (offset >= pools.at(pool).descriptors.size())
		return {};

	return pools.at(pool).descriptors[offset];
}

static void on_init_device(reshade::api::device *device)
{
	device->create_user_data<descriptor_set_tracking>(descriptor_set_tracking::GUID);
}
static void on_destroy_device(reshade::api::device *device)
{
	device->destroy_user_data<descriptor_set_tracking>(descriptor_set_tracking::GUID);
}

static bool on_update_descriptor_sets(reshade::api::device *device, uint32_t count, const reshade::api::descriptor_set_update *updates)
{
	descriptor_set_tracking &ctx = device->get_user_data<descriptor_set_tracking>(descriptor_set_tracking::GUID);

	const std::unique_lock<std::shared_mutex> lock(ctx.mutex);

	for (uint32_t i = 0; i < count; ++i)
	{
		const reshade::api::descriptor_set_update &update = updates[i];

		uint32_t offset = 0;
		reshade::api::descriptor_pool pool = { 0 };
		device->get_descriptor_pool_offset(update.set, &pool, &offset);
		offset += update.offset;

		descriptor_pool_data &pool_data = ctx.pools[pool];

		if (offset + update.count > pool_data.descriptors.size())
			pool_data.descriptors.resize(offset + update.count);

		for (uint32_t k = 0; k < update.count; ++k)
		{
			if (update.type != reshade::api::descriptor_type::shader_resource_view &&
				update.type != reshade::api::descriptor_type::unordered_access_view)
				continue;

			pool_data.descriptors[offset + k] = static_cast<const reshade::api::resource_view *>(update.descriptors)[k];
		}
	}

	return false;
}

void register_addon_descriptor_set_tracking()
{
	reshade::register_event<reshade::addon_event::init_device>(on_init_device);
	reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::register_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
void unregister_addon_descriptor_set_tracking()
{
	reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
	reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
	reshade::unregister_event<reshade::addon_event::update_descriptor_sets>(on_update_descriptor_sets);
}
