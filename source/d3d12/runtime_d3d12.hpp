/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "render_d3d12.hpp"

namespace reshade::d3d12
{
	class runtime_impl : public api::api_object_impl<IDXGISwapChain3 *, runtime>
	{
	public:
		runtime_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _cmd_queue_impl; }

		bool on_init();
		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		void on_present(ID3D12Resource *backbuffer, HWND hwnd);

		bool capture_screenshot(uint8_t *buffer) const final;

		void update_texture_bindings(const char *semantic, api::resource_view_handle srv) final;

	private:
		bool init_effect(size_t index) final;
		void unload_effect(size_t index) final;
		void unload_effects() final;

		bool init_texture(texture &texture) final;
		void upload_texture(const texture &texture, const uint8_t *pixels) final;
		void destroy_texture(texture &texture) final;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) final;

		const com_ptr<ID3D12Device> _device;
		const com_ptr<ID3D12CommandQueue> _cmd_queue;
		device_impl *const _device_impl;
		command_queue_impl *const _cmd_queue_impl;
		command_list_immediate_impl *const _cmd_impl;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

		UINT _swap_index = 0;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12Resource> _backbuffer_texture;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_dsvs;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D12Resource> _effect_stencil;
		std::vector<struct effect_data> _effect_data;

		std::unordered_map<std::string, D3D12_CPU_DESCRIPTOR_HANDLE> _texture_semantic_bindings;

#if RESHADE_GUI
		static const uint32_t NUM_IMGUI_BUFFERS = 4;

		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) final;

		struct imgui_resources
		{
			com_ptr<ID3D12PipelineState> pipeline;
			com_ptr<ID3D12RootSignature> signature;

			com_ptr<ID3D12Resource> indices[NUM_IMGUI_BUFFERS];
			com_ptr<ID3D12Resource> vertices[NUM_IMGUI_BUFFERS];
			int num_indices[NUM_IMGUI_BUFFERS] = {};
			int num_vertices[NUM_IMGUI_BUFFERS] = {};
		} _imgui;
#endif
	};
}
