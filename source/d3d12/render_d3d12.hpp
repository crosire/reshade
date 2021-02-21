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
	void convert_resource_desc(api::resource_type type, const api::resource_desc &desc, D3D12_RESOURCE_DESC &internal_desc);
	std::pair<api::resource_type, api::resource_desc> convert_resource_desc(const D3D12_RESOURCE_DESC &internal_desc);

	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	void convert_resource_view_desc(const api::resource_view_desc &desc, D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_DEPTH_STENCIL_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_RENDER_TARGET_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_SHADER_RESOURCE_VIEW_DESC &internal_desc);
	api::resource_view_desc convert_resource_view_desc(const D3D12_UNORDERED_ACCESS_VIEW_DESC &internal_desc);

	class device_impl : public api::api_object_impl<api::device>
	{
		friend class command_queue_impl;

	public:
		explicit device_impl(ID3D12Device *device);
		~device_impl();

		uint64_t get_native_object() const override { return reinterpret_cast<uintptr_t>(_orig); }

		reshade::api::render_api get_api() const override { return api::render_api::d3d12; }

		bool check_format_support(uint32_t format, api::resource_usage usage) const override;

		bool check_resource_handle_valid(api::resource_handle resource) const override;
		bool check_resource_view_handle_valid(api::resource_view_handle view) const override;

		bool create_resource(api::resource_type type, const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource) override;
		bool create_resource_view(api::resource_handle resource, api::resource_view_type type, const api::resource_view_desc &desc, api::resource_view_handle *out_view) override;

		void destroy_resource(api::resource_handle resource) override;
		void destroy_resource_view(api::resource_view_handle view) override;

		void get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const override;

		api::resource_desc get_resource_desc(api::resource_handle resource) const override;

		void wait_idle() const override;

		com_ptr<ID3D12RootSignature> create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const;

		// Pointer to original device object (managed by D3D12Device class)
		ID3D12Device *_orig;

		// Cached device capabilities for quick access
		UINT _srv_handle_size = 0;
		UINT _rtv_handle_size = 0;
		UINT _dsv_handle_size = 0;
		UINT _sampler_handle_size = 0;
		// Device-local resources that may be used by multiple effect runtimes
		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

	protected:
#if RESHADE_ADDON
		inline void register_resource_view(ID3D12Resource *resource, D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			assert(resource != nullptr);
			const std::lock_guard<std::mutex> lock(_mutex);
			_views.emplace(handle.ptr, resource);
		}
#endif

		com_object_list<ID3D12Resource> _resources;
		std::unordered_map<uint64_t, ID3D12Resource *> _views;
		mutable std::mutex _mutex;
		std::vector<ID3D12CommandQueue *> _queues;
	};

	class command_list_impl : public api::api_object_impl<api::command_list>
	{
	public:
		command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list);
		~command_list_impl();

		uint64_t get_native_object() const override { return reinterpret_cast<uintptr_t>(_orig); }

		api::device *get_device() override { return _device_impl; }

		void draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance) override;
		void draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) override;

		void copy_resource(api::resource_handle source, api::resource_handle destination) override;

		void transition_state(api::resource_handle resource, api::resource_usage old_state, api::resource_usage new_state) override;

		void clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil) override;
		void clear_render_target_view(api::resource_view_handle rtv, const float color[4]) override;

		// Pointer to original command list object (managed by D3D12CommandList class)
		ID3D12GraphicsCommandList *_orig;

	protected:
		device_impl *const _device_impl;
		bool _has_commands = false;
	};

	class command_list_immediate_impl : public command_list_impl
	{
		static const UINT NUM_COMMAND_FRAMES = 4;

	public:
		command_list_immediate_impl(device_impl *device);
		~command_list_immediate_impl();

		bool flush(ID3D12CommandQueue *queue);
		bool flush_and_wait(ID3D12CommandQueue *queue);

		ID3D12GraphicsCommandList *const begin_commands() { _has_commands = true; return _orig; }

	private:
		UINT _cmd_index = 0;
		HANDLE _fence_event = nullptr;
		UINT64 _fence_value[NUM_COMMAND_FRAMES] = {};
		com_ptr<ID3D12Fence> _fence[NUM_COMMAND_FRAMES];
		com_ptr<ID3D12CommandAllocator> _cmd_alloc[NUM_COMMAND_FRAMES];
	};

	class command_queue_impl : public api::api_object_impl<api::command_queue>
	{
	public:
		command_queue_impl(device_impl *device, ID3D12CommandQueue *queue);
		~command_queue_impl();

		uint64_t get_native_object() const override { return reinterpret_cast<uintptr_t>(_orig); }

		api::device *get_device() override { return _device_impl; }

		api::command_list *get_immediate_command_list() override { return _immediate_cmd_list; }

		void flush_immediate_command_list() const override;

		// Pointer to original command queue object (managed by D3D12CommandQueue class)
		ID3D12CommandQueue *_orig;

	private:
		device_impl *const _device_impl;
		command_list_immediate_impl *_immediate_cmd_list = nullptr;
	};
}
