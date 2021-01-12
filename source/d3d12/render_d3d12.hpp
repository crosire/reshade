/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "com_ptr.hpp"
#include "com_tracking.hpp"
#include "addon_manager.hpp"
#include <d3d12.h>
#include <dxgi1_5.h>
#include <unordered_map>

namespace reshade::d3d12
{
	void convert_resource_desc(const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc);

	void convert_depth_stencil_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_depth_stencil_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);

	void convert_render_target_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_render_target_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);

	void convert_shader_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_shader_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);

	class device_impl : public api::device
	{
		friend class runtime_d3d12;

	public:
		explicit device_impl(ID3D12Device *device);
		~device_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return SUCCEEDED(_device->GetPrivateData(*reinterpret_cast<const GUID *>(guid), &size, data)); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override { _device->SetPrivateData(*reinterpret_cast<const GUID *>(guid), size, data); }

		api::render_api get_api() override { return api::render_api::d3d12; }

		bool check_format_support(uint32_t format, api::resource_usage usage) override;

		bool is_resource_valid(api::resource_handle resource) override;
		bool is_resource_view_valid(api::resource_view_handle view) override;

		bool create_resource(const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) override;

		api::resource_desc get_resource_desc(api::resource_handle resource) override;

		void wait_idle() override;

		void register_queue(ID3D12CommandQueue *queue);
		void register_resource(ID3D12Resource *resource) { _resources.register_object(resource); }
		void register_resource_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE handle);

		com_ptr<ID3D12RootSignature> create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const;

		UINT srv_handle_size = 0;
		UINT rtv_handle_size = 0;
		UINT dsv_handle_size = 0;
		UINT sampler_handle_size = 0;

	private:
		const com_ptr<ID3D12Device> _device;
		std::vector<com_ptr<ID3D12CommandQueue>> _queues;
		com_object_list<ID3D12Resource> _resources;
		std::unordered_map<uint64_t, ID3D12Resource *> _views;
	};

	class command_list_impl : public api::command_list
	{
	public:
		command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list);
		~command_list_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return SUCCEEDED(_cmd_list->GetPrivateData(*reinterpret_cast<const GUID *>(guid), &size, data)); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override { _cmd_list->SetPrivateData(*reinterpret_cast<const GUID *>(guid), size, data); }

		api::device *get_device() override { return _device_impl; }

		void transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state) override;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		void copy_resource(api::resource_handle source, api::resource_handle dest) override;

	private:
		device_impl *const _device_impl;
		const com_ptr<ID3D12GraphicsCommandList> _cmd_list;
	};

	class command_queue_impl : public api::command_queue
	{
		friend class runtime_d3d12;

	public:
		command_queue_impl(device_impl *device, ID3D12CommandQueue *queue);
		~command_queue_impl();

		bool get_data(const uint8_t guid[16], uint32_t size, void *data) override { return SUCCEEDED(_queue->GetPrivateData(*reinterpret_cast<const GUID *>(guid), &size, data)); }
		void set_data(const uint8_t guid[16], uint32_t size, const void *data) override { _queue->SetPrivateData(*reinterpret_cast<const GUID *>(guid), size, data); }

		api::device *get_device() override { return _device_impl; }

		api::command_list *get_immediate_command_list() override { return nullptr; }

	private:
		device_impl *const _device_impl;
		const com_ptr<ID3D12CommandQueue> _queue;
	};
}
