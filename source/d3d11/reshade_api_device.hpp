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
		friend class swapchain_impl;

	public:
		explicit device_impl(ID3D11Device *device);
		~device_impl();

		api::device_api get_api() const final { return api::device_api::d3d11; }

		bool check_capability(api::device_caps capability) const final;
		bool check_format_support(api::format format, api::resource_usage usage) const final;

		bool is_resource_handle_valid(api::resource handle) const final;
		bool is_resource_view_handle_valid(api::resource_view handle) const final;

		bool create_sampler(const api::sampler_desc &desc, api::sampler *out) final;
		bool create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out) final;
		bool create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out) final;

		bool create_pipeline(const api::pipeline_desc &desc, api::pipeline *out) final;
		bool create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_vertex_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_hull_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_domain_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_geometry_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_pixel_shader(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_input_layout(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_blend_state(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_rasterizer_state(const api::pipeline_desc &desc, api::pipeline *out);
		bool create_pipeline_graphics_depth_stencil_state(const api::pipeline_desc &desc, api::pipeline *out);

		bool create_pipeline_layout(uint32_t num_set_layouts, const api::descriptor_set_layout *set_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out) final;
		bool create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_set_layout *out) final;
		bool create_query_pool(api::query_type type, uint32_t count, api::query_pool *out) final;
		bool create_framebuffer(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv, api::framebuffer *out) final;
		bool create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out) final;

		void destroy_sampler(api::sampler handle) final;
		void destroy_resource(api::resource handle) final;
		void destroy_resource_view(api::resource_view handle) final;

		void destroy_pipeline(api::pipeline_stage type, api::pipeline handle) final;
		void destroy_pipeline_layout(api::pipeline_layout handle) final;
		void destroy_descriptor_set_layout(api::descriptor_set_layout handle) final;
		void destroy_query_pool(api::query_pool handle) final;
		void destroy_framebuffer(api::framebuffer handle) final;
		void destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets) final;

		void get_resource_from_view(api::resource_view view, api::resource *out) const final;
		api::resource_desc get_resource_desc(api::resource resource) const final;

		bool get_framebuffer_attachment(api::framebuffer fbo, api::attachment_type type, uint32_t index, api::resource_view *out) const final;

		bool map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data, uint32_t *row_pitch, uint32_t *slice_pitch) final;
		void unmap_resource(api::resource resource, uint32_t subresource) final;

		void upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size) final;
		void upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6]) final;

		void update_descriptor_sets(uint32_t num_updates, const api::descriptor_update *updates) final;

		bool get_query_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride) final;

		void wait_idle() const final { /* no-op */ }

		void set_debug_name(api::resource resource, const char *name) final;

	private:
		ID3D11DeviceContext *_immediate_context_orig = nullptr;
		com_ptr<ID3D11VertexShader> _copy_vert_shader;
		com_ptr<ID3D11PixelShader>  _copy_pixel_shader;
		com_ptr<ID3D11SamplerState> _copy_sampler_state;

	protected:
		com_object_list<ID3D11View> _views;
		com_object_list<ID3D11Resource> _resources;
	};
}
