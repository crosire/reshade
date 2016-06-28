#pragma once

#include "runtime.hpp"
#include "com_ptr.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

namespace reshade
{
	class d3d12_runtime : public runtime
	{
	public:
		d3d12_runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain);

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_present_effect() override;

		void capture_frame(uint8_t *buffer) const override;
		bool update_effect(const reshadefx::syntax_tree &ast, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;
		bool update_texture_reference(texture &texture, unsigned short id) override;

		void render_technique(const technique &technique) override;
		void render_draw_lists(ImDrawData *data) override;

		com_ptr<ID3D12Device> _device;
		com_ptr<ID3D12CommandQueue> _commandqueue;
		com_ptr<IDXGISwapChain3> _swapchain;
	};
}
