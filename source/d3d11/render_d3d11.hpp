/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include <d3d11_4.h>

namespace reshade::d3d11
{
	class device_impl : public api::api_object_impl<ID3D11Device *, api::device>
	{
	public:
		explicit device_impl(ID3D11Device *device);
		~device_impl();

		api::render_api get_api() const final { return api::render_api::d3d11; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource_handle resource) const final;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const final;

		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource_handle *out_resource) final;
		bool create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) final;

		void destroy_resource(api::resource_handle resource) final;
		void destroy_resource_view(api::resource_view_handle view) final;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const final;

		api::resource_desc get_resource_desc(api::resource_handle resource) const final;

		void wait_idle() const final { /* no-op */ }

	protected:
		com_object_list<ID3D11View> _views;
		com_object_list<ID3D11Resource> _resources;
	};

	class command_list_impl : public api::api_object_impl<ID3D11CommandList *, api::command_list>
	{
	public:
		command_list_impl(device_impl *device, ID3D11CommandList *cmd_list);
		~command_list_impl();

		api::device *get_device() final { return _device_impl; }

		void copy_resource(api::resource_handle, api::resource_handle) final {}

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) final {}

		void clear_depth_stencil_view(api::resource_view_handle, uint32_t, float, uint8_t) final {}
		void clear_render_target_view(api::resource_view_handle, const float[4]) final {}

	private:
		device_impl *const _device_impl;
	};

	class device_context_impl : public api::api_object_impl<ID3D11DeviceContext *, api::command_list, api::command_queue>
	{
	public:
		device_context_impl(device_impl *device, ID3D11DeviceContext *context);
		~device_context_impl();

		api::device *get_device() final { return _device_impl; }

		api::command_list *get_immediate_command_list() final { assert(_orig->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE); return this; }

		void flush_immediate_command_list() const final;

		void copy_resource(api::resource_handle source, api::resource_handle destination) final;

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) final { /* no-op */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) final;

	private:
		device_impl *const _device_impl;
	};
}
