/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include "state_block_d3d9.hpp"

namespace reshade::d3d9
{
	void convert_resource_desc(const api::resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels = nullptr);
	void convert_resource_desc(const api::resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels = nullptr);
	api::resource_desc convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels = 1);
	api::resource_desc convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels = 1);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3DSURFACE_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3DSURFACE_DESC &internal_desc);

	class device_impl : public api::device, public api::command_queue, public api::command_list, api::api_data
	{
		friend class runtime_d3d9;

	public:
		explicit device_impl(IDirect3DDevice9 *device);
		~device_impl();

		void on_reset();
		void on_after_reset();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return api_data::get_data(guid, size, data); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override  { api_data::set_data(guid, size, data); }

		api::render_api get_api() override { return api::render_api::d3d9; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool is_resource_valid(api::resource_handle resource) override;
		bool is_resource_view_valid(api::resource_view_handle view) override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override { /* no-op */ }

		api::device *get_device() override { return this; }
		api::command_list *get_immediate_command_list() override { return this; }

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) override { /* no-op */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle dsv, const float color[4]) override;

		void copy_resource(api::resource_handle source, api::resource_handle dest) override;

		void register_resource(IDirect3DResource9 *resource) { _resources.register_object(resource); }
		api::resource_view_handle get_resource_view_handle(IDirect3DSurface9 *surface)
		{
			if (surface != nullptr)
				register_resource(surface);
			return { reinterpret_cast<uintptr_t>(surface) };
		}

		unsigned int _num_samplers;
		unsigned int _num_simultaneous_rendertargets;
		unsigned int _behavior_flags;

	private:
		const com_ptr<IDirect3DDevice9> _device;
		com_object_list<IDirect3DResource9, true> _resources;

		state_block _backup_state;
		com_ptr<IDirect3DStateBlock9> _copy_state;
	};
}
