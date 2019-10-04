/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "com_ptr.hpp"
#include "draw_call_tracker.hpp"
#include <d3d12.h>
#include <dxgi1_5.h>

#define RESHADE_DX12_CAPTURE_DEPTH_BUFFERS 1

namespace reshadefx { struct sampler_info; }

namespace reshade::d3d12
{
	class runtime_d3d12 : public runtime
	{
	public:
		runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain);
		~runtime_d3d12();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc
#if RESHADE_D3D12ON7
			, ID3D12Resource *backbuffer = nullptr
#endif
			);
		void on_reset();
		void on_present(draw_call_tracker& tracker);
		void on_create_depthstencil_view(ID3D12Resource *pResource);

		bool capture_screenshot(uint8_t *buffer) const override;

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		com_ptr<ID3D12Resource> select_depth_texture_save(D3D12_RESOURCE_DESC &texture_desc, const D3D12_HEAP_PROPERTIES *props);
#endif

		bool depth_buffer_before_clear = false;
		bool depth_buffer_more_copies = false;
		bool extended_depth_buffer_detection = false;
		unsigned int cleared_depth_buffer_index = 0;
		int depth_buffer_texture_format = 0; // No depth buffer texture format filter by default
		std::atomic<unsigned int> clear_DSV_iter = 1;

	private:
		bool init_backbuffer_textures(UINT num_buffers);
		bool init_default_depth_stencil();
		bool init_mipmap_pipeline();

		bool init_texture(texture &info) override;
		void upload_texture(texture &texture, const uint8_t *pixels) override;

		bool compile_effect(effect_data &effect) override;
		void unload_effect(size_t id) override;
		void unload_effects() override;

		bool init_technique(technique &technique, const std::unordered_map<std::string, com_ptr<ID3DBlob>> &entry_points);

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		bool init_imgui_resources();
		void render_imgui_draw_data(ImDrawData *data) override;
		void draw_debug_menu();
#endif

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
		void detect_depth_source(draw_call_tracker& tracker);
		bool create_depthstencil_replacement(ID3D12Resource *depthstencil, ID3D12Resource *texture);

		struct depth_texture_save_info
		{
			com_ptr<ID3D12Resource> src_texture;
			D3D12_RESOURCE_DESC src_texture_desc;
			com_ptr<ID3D12Resource> dest_texture;
			bool cleared = false;
		};
#endif

		void generate_mipmaps(const com_ptr<ID3D12GraphicsCommandList> &list, texture &texture);

		com_ptr<ID3D12RootSignature> create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const;
		com_ptr<ID3D12GraphicsCommandList> create_command_list(const com_ptr<ID3D12PipelineState> &state = nullptr) const;
		void execute_command_list(const com_ptr<ID3D12GraphicsCommandList> &list) const;
		void wait_for_command_queue() const;

		com_ptr<ID3D12Device> _device;
		com_ptr<ID3D12CommandQueue> _commandqueue;
		com_ptr<IDXGISwapChain3> _swapchain;

		UINT _swap_index = 0;
		HANDLE _fence_event = nullptr;
		mutable std::vector<UINT64> _fence_value;
		std::vector<com_ptr<ID3D12Fence>> _fence;

		com_ptr<ID3D12GraphicsCommandList> _cmd_list;
		std::vector<com_ptr<ID3D12CommandAllocator>> _cmd_alloc;

		bool _is_multisampling_enabled = false;
		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D12DescriptorHeap> _backbuffer_rtvs;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_dsvs;
		std::vector<com_ptr<ID3D12Resource>> _backbuffers;
		com_ptr<ID3D12Resource> _depthstencil, _depthstencil_replacement;
		com_ptr<ID3D12Resource> _default_depthstencil;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_texture_srv;
		com_ptr<ID3D12DescriptorHeap> _depthstencil_texture_dsv;
		com_ptr<ID3D12Resource> _depthstencil_texture;
		ID3D12Resource *_best_depth_stencil_overwrite = nullptr;

		std::map<UINT, depth_texture_save_info> _displayed_depth_textures;
		std::unordered_map<UINT64, com_ptr<ID3D12Resource>> _depth_texture_saves;

		com_ptr<ID3D12Resource> _backbuffer_texture;

		com_ptr<ID3D12PipelineState> _mipmap_pipeline;
		com_ptr<ID3D12RootSignature> _mipmap_signature;

		UINT _srv_handle_size = 0;
		UINT _rtv_handle_size = 0;
		UINT _dsv_handle_size = 0;
		UINT _sampler_handle_size = 0;

		std::vector<struct d3d12_effect_data> _effect_data;

		HMODULE _d3d_compiler = nullptr;

		draw_call_tracker *_current_tracker = nullptr;

#if RESHADE_GUI
		unsigned int _imgui_index_buffer_size[3] = {};
		com_ptr<ID3D12Resource> _imgui_index_buffer[3];
		unsigned int _imgui_vertex_buffer_size[3] = {};
		com_ptr<ID3D12Resource> _imgui_vertex_buffer[3];
		com_ptr<ID3D12PipelineState> _imgui_pipeline;
		com_ptr<ID3D12RootSignature> _imgui_signature;
#endif
	};
}
