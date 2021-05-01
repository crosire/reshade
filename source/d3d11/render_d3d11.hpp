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

		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource handle) const final;
		bool check_resource_view_handle_valid(api::resource_view handle) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void get_resource_from_view(api::resource_view view, api::resource *out_resource) const final;

		api::resource_desc get_resource_desc(api::resource resource) const final;

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

		void blit(api::resource, uint32_t, const int32_t[6], api::resource, uint32_t, const int32_t[6], api::texture_filter) final { assert(false); }
		void resolve(api::resource, uint32_t, const int32_t[3], api::resource, uint32_t, const int32_t[3], const uint32_t[3], uint32_t) final { assert(false); }
		void copy_resource(api::resource, api::resource) final { assert(false); }
		void copy_buffer_region(api::resource, uint64_t, api::resource, uint64_t, uint64_t) final { assert(false); }
		void copy_buffer_to_texture(api::resource, uint64_t, uint32_t, uint32_t, api::resource, uint32_t, const int32_t[6]) final { assert(false); }
		void copy_texture_region(api::resource, uint32_t, const int32_t[3], api::resource, uint32_t, const int32_t[3], const uint32_t[3]) final { assert(false); }
		void copy_texture_to_buffer(api::resource, uint32_t, const int32_t[6], api::resource, uint64_t, uint32_t, uint32_t) final { assert(false); }

		void clear_depth_stencil_view(api::resource_view, uint32_t, float, uint8_t) final { assert(false); }
		void clear_render_target_views(uint32_t, const api::resource_view *, const float[4]) final { assert(false); }

		void transition_state(api::resource, api::resource_usage, api::resource_usage) final { assert(false); }

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

		void blit(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format) final;
		void copy_resource(api::resource src, api::resource dst) final;
		void copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) final;
		void copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;

		void clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4]) final;

		void transition_state(api::resource, api::resource_usage, api::resource_usage) final { /* no-op */ }

	private:
		device_impl *const _device_impl;
	};
}
