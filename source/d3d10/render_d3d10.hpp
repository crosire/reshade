/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include <d3d10_1.h>

namespace reshade::d3d10
{
	class device_impl : public api::api_object_impl<ID3D10Device1 *, api::device, api::command_queue, api::command_list>
	{
	public:
		explicit device_impl(ID3D10Device1 *device);
		~device_impl();

		api::render_api get_api() const final { return api::render_api::d3d10; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const final;

		bool check_resource_handle_valid(api::resource_handle resource) const final;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler_handle *out_sampler) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource_handle *out_resource) final;
		bool create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) final;

		void destroy_sampler(api::sampler_handle sampler) final;
		void destroy_resource(api::resource_handle resource) final;
		void destroy_resource_view(api::resource_view_handle view) final;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const final;

		api::resource_desc get_resource_desc(api::resource_handle resource) const final;

		void wait_idle() const final { /* no-op */ }

		api::device *get_device() final { return this; }

		api::command_list *get_immediate_command_list() final { return this; }

		void flush_immediate_command_list() const final;

		void blit(api::resource_handle src, uint32_t src_subresource, const int32_t src_box[6], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_box[6], api::texture_filter filter) final;
		void resolve(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format) final;
		void copy_resource(api::resource_handle src, api::resource_handle dst) final;
		void copy_buffer_region(api::resource_handle src, uint64_t src_offset, api::resource_handle dst, uint64_t dst_offset, uint64_t size) final;
		void copy_buffer_to_texture(api::resource_handle src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;
		void copy_texture_region(api::resource_handle src, uint32_t src_subresource, const int32_t src_offset[3], api::resource_handle dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3]) final;
		void copy_texture_to_buffer(api::resource_handle src, uint32_t src_subresource, const int32_t src_box[6], api::resource_handle dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height) final;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) final;
		void clear_render_target_views(uint32_t count, const api::resource_view_handle *rtvs, const float color[4]) final;

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) final { /* no-op */ }

	protected:
		com_object_list<ID3D10View> _views;
		com_object_list<ID3D10Resource> _resources;
	};
}
