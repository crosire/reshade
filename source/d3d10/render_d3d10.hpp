/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include "state_block_d3d10.hpp"

namespace reshade::d3d10
{
	void convert_resource_desc(const api::resource_desc &desc, D3D10_BUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE1D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE2D_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3D10_TEXTURE3D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_BUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE1D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE2D_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D10_TEXTURE3D_DESC &internal_desc);

	void convert_depth_stencil_view_desc(const api::resource_view_desc &desc, D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_depth_stencil_view_desc(const D3D10_DEPTH_STENCIL_VIEW_DESC &internal_desc);

	void convert_render_target_view_desc(const api::resource_view_desc &desc, D3D10_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_render_target_view_desc(const D3D10_RENDER_TARGET_VIEW_DESC &internal_desc);

	void convert_shader_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_shader_resource_view_desc(const api::resource_view_desc &desc, D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);
	api::resource_view_desc convert_shader_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_shader_resource_view_desc(const D3D10_SHADER_RESOURCE_VIEW_DESC1 &internal_desc);

	class device_impl : public api::device, public api::command_queue, public api::command_list
	{
		friend class runtime_d3d10;

	public:
		explicit device_impl(ID3D10Device1 *device);
		~device_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return SUCCEEDED(_device->GetPrivateData(*reinterpret_cast<const GUID *>(guid), &size, data)); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override { _device->SetPrivateData(*reinterpret_cast<const GUID *>(guid), size, data); }

		void *get_native_object() override { return _device.get(); }

		api::render_api get_api() override { return api::render_api::d3d10; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool check_resource_handle_valid(api::resource_handle resource) override;
		bool check_resource_view_handle_valid(api::resource_view_handle view) override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override { /* no-op */ }

		api::device *get_device() override { return this; }
		api::command_list *get_immediate_command_list() override { return this; }

		void transition_state(api::resource_handle, api::resource_usage, api::resource_usage) override { /* no-op */ }

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		void copy_resource(api::resource_handle source, api::resource_handle dest) override;

		inline void register_resource(ID3D10Resource *resource) { _resources.register_object(resource); }
		inline void register_resource_view(ID3D10View *resource_view) { _views.register_object(resource_view); }

	private:
		com_ptr<ID3D10Device1> _device;
		com_object_list<ID3D10View> _views;
		com_object_list<ID3D10Resource> _resources;

		state_block _app_state;
		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState>  _copy_sampler_state;
	};
}
