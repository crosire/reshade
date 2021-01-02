/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "state_tracking.hpp"
#include <dxgi1_5.h>

namespace reshade::d3d12
{
	class runtime_d3d12 : public runtime
	{
		static const uint32_t NUM_IMGUI_BUFFERS = 4;

	public:
		runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain, state_tracking_context *state_tracking);
		~runtime_d3d12();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();
		void on_present(ID3D12Resource *backbuffer, HWND hwnd);

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(const texture &texture, const uint8_t *pixels) override;
		void destroy_texture(texture &texture) override;
		void generate_mipmaps(const struct tex_data *impl);

		void render_technique(technique &technique) override;

		bool begin_command_list(const com_ptr<ID3D12PipelineState> &state = nullptr) const;
		void execute_command_list() const;
		bool wait_for_command_queue() const;

		com_ptr<ID3D12RootSignature> create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const;

		state_tracking_context &_state_tracking;
		const com_ptr<ID3D12Device> _device;
		const com_ptr<IDXGISwapChain3> _swapchain;
		const com_ptr<ID3D12CommandQueue> _commandqueue;
		UINT _srv_handle_size = 0;
		UINT _rtv_handle_size = 0;
		UINT _dsv_handle_size = 0;
		UINT _sampler_handle_size = 0;

		UINT _swap_index = 0;
		HANDLE _fence_event = nullptr;
		mutable std::vector<UINT64> _fence_value;
		std::vector<com_ptr<ID3D12Fence>> _fence;
		mutable bool _cmd_list_is_recording = false;
		com_ptr<ID3D12GraphicsCommandList> _cmd_list;
		std::vector<com_ptr<ID3D12CommandAllocator>> _cmd_alloc;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12Resource> _backbuffer_texture;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_dsvs;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D12Resource> _effect_stencil;
		std::vector<struct effect_data> _effect_data;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

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

#if RESHADE_DEPTH
		void draw_depth_debug_menu();
		void update_depth_texture_bindings(com_ptr<ID3D12Resource> texture);

		com_ptr<ID3D12Resource> _depth_texture;
		ID3D12Resource *_depth_texture_override = nullptr;
#endif
	};
}
