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
	void convert_resource_desc(const api::resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels = 1);
	api::resource_desc convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels = 1);
	api::resource_desc convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc);

	class device_impl : public api::api_object_impl<api::device, api::command_queue, api::command_list>
	{
	public:
		explicit device_impl(IDirect3DDevice9 *device);
		~device_impl();

		uint64_t get_native_object() const override { return reinterpret_cast<uintptr_t>(_orig); }

		reshade::api::render_api get_api() const override { return api::render_api::d3d9; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const override;

		bool check_resource_handle_valid(api::resource_handle resource) const override;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const override;

		api::resource_desc get_resource_desc(api::resource_handle resource) const override;

		void wait_idle() const override { /* no-op */ }

		api::device *get_device() override { return this; }
		api::command_list *get_immediate_command_list() override { return this; }

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) override;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override;

		void copy_resource(api::resource_handle source, api::resource_handle destination) override;

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) override { /* no-op */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle dsv, const float color[4]) override;

		// Pointer to original device object (managed by Direct3DDevice9 class)
		IDirect3DDevice9 *_orig;

		// Cached device capabilities for quick access
		D3DCAPS9 _caps;
		D3DDEVICE_CREATION_PARAMETERS _cp;
		// Device-local resources that may be used by multiple effect runtimes
		state_block _app_state;
		com_ptr<IDirect3DStateBlock9> _copy_state;

	protected:
#if RESHADE_ADDON
		api::resource_view_handle get_resource_view_handle(IDirect3DSurface9 *surface)
		{
			if (surface != nullptr)
				_resources.register_object(surface);
			return { reinterpret_cast<uintptr_t>(surface) };
		}
#endif

		void on_reset();
		void on_after_reset(const D3DPRESENT_PARAMETERS &pp);

		com_object_list<IDirect3DResource9, true> _resources;
	};
}
