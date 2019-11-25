/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "buffer_detection.hpp"
#include <dxgi1_5.h>

namespace reshadefx { struct sampler_info; }

namespace reshade::d3d12
{
	class runtime_d3d12 : public runtime
	{
		static const uint32_t NUM_IMGUI_BUFFERS = 5;

	public:
		runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain);
		~runtime_d3d12();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc
#if RESHADE_D3D12ON7
			, ID3D12Resource *backbuffer = nullptr
#endif
			);
		void on_reset();
		void on_present(buffer_detection_context &tracker);

		bool capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_effect(size_t index) override;
		void unload_effect(size_t index) override;
		void unload_effects() override;

		bool init_texture(texture &texture) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;
		void generate_mipmaps(texture &texture);

		void render_technique(technique &technique) override;

		bool begin_command_list(const com_ptr<ID3D12PipelineState> &state = nullptr) const;
		void execute_command_list() const;
		void wait_for_command_queue() const;

		com_ptr<ID3D12RootSignature> create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const;

		const com_ptr<ID3D12Device> _device;
		const com_ptr<ID3D12CommandQueue> _commandqueue;
		const com_ptr<IDXGISwapChain3> _swapchain;
		UINT _srv_handle_size = 0;
		UINT _rtv_handle_size = 0;
		UINT _dsv_handle_size = 0;
		UINT _sampler_handle_size = 0;

		UINT _swap_index = 0;
		HANDLE _fence_event = nullptr;
		mutable std::vector<UINT64> _fence_value;
		std::vector<com_ptr<ID3D12Fence>> _fence;
		com_ptr<ID3D12GraphicsCommandList> _cmd_list;
		std::vector<com_ptr<ID3D12CommandAllocator>> _cmd_alloc;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12Resource> _backbuffer_texture;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;
		com_ptr<ID3D12Resource> _depth_texture;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_dsvs;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D12Resource> _effect_depthstencil;
		std::vector<struct d3d12_effect_data> _effect_data;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;

		int _imgui_index_buffer_size[NUM_IMGUI_BUFFERS] = {};
		com_ptr<ID3D12Resource> _imgui_index_buffer[NUM_IMGUI_BUFFERS];
		int _imgui_vertex_buffer_size[NUM_IMGUI_BUFFERS] = {};
		com_ptr<ID3D12Resource> _imgui_vertex_buffer[NUM_IMGUI_BUFFERS];
		com_ptr<ID3D12PipelineState> _imgui_pipeline;
		com_ptr<ID3D12RootSignature> _imgui_signature;
#endif

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void draw_depth_debug_menu();
		void update_depthstencil_texture(com_ptr<ID3D12Resource> texture);

		bool _filter_aspect_ratio = true;
		bool _preserve_depth_buffers = false;
		UINT _depth_clear_index_override = std::numeric_limits<UINT>::max();
		ID3D12Resource *_depth_texture_override = nullptr;
		buffer_detection_context *_current_tracker = nullptr;
#endif
	};
}
