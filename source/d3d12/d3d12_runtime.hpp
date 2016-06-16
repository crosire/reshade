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
		~d3d12_runtime();

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset() override;
		void on_reset_effect() override;
		void on_present() override;
		void on_apply_effect() override;
		void on_apply_effect_technique(const technique &technique) override;

		ID3D12Device *_device;
		ID3D12CommandQueue *_commandqueue;
		IDXGISwapChain3 *_swapchain;

	private:
		void screenshot(uint8_t *buffer) const override;
		bool update_effect(const reshadefx::syntax_tree &ast, std::string &errors) override;
		bool update_texture(texture &texture, const uint8_t *data) override;

		void render_draw_lists(ImDrawData *data) override;
	};
}
