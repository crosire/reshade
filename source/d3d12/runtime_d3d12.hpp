/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "com_ptr.hpp"
#include <d3d12.h>
#include <dxgi1_5.h>

namespace reshade::d3d12
{
	class runtime_d3d12 : public runtime
	{
	public:
		runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain);

		bool on_init(const DXGI_SWAP_CHAIN_DESC &desc);
		void on_reset();
		void on_present();

		void capture_screenshot(uint8_t *buffer) const override;

	private:
		bool init_texture(texture &info) override;
		void upload_texture(texture &texture, const uint8_t *data) override;

		bool compile_effect(effect_data &effect) override;
		void unload_effects() override;

		void render_technique(technique &technique) override;

#if RESHADE_GUI
		void render_imgui_draw_data(ImDrawData *data) override;
#endif

		com_ptr<ID3D12Device> _device;
		com_ptr<ID3D12CommandQueue> _commandqueue;
		com_ptr<IDXGISwapChain3> _swapchain;
	};
}
