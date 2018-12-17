/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "runtime_d3d12.hpp"
#include <assert.h>

reshade::d3d12::runtime_d3d12::runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) :
	_device(device), _commandqueue(queue), _swapchain(swapchain)
{
	assert(queue != nullptr);
	assert(device != nullptr);
	assert(swapchain != nullptr);

	_renderer_id = D3D_FEATURE_LEVEL_12_0;
}

bool reshade::d3d12::runtime_d3d12::on_init(const DXGI_SWAP_CHAIN_DESC &desc)
{
	_width = desc.BufferDesc.Width;
	_height = desc.BufferDesc.Height;

	return runtime::on_init(desc.OutputWindow);
}
void reshade::d3d12::runtime_d3d12::on_reset()
{
	runtime::on_reset();
}

void reshade::d3d12::runtime_d3d12::on_present()
{
	if (!_is_initialized)
		return;

	update_and_render_effects();
	runtime::on_present();
}

void reshade::d3d12::runtime_d3d12::capture_screenshot(uint8_t *buffer) const
{
}

bool reshade::d3d12::runtime_d3d12::init_texture(texture &info)
{
	return false;
}
void reshade::d3d12::runtime_d3d12::upload_texture(texture &texture, const uint8_t *data)
{
}

bool reshade::d3d12::runtime_d3d12::compile_effect(effect_data &effect)
{
	return false;
}
void reshade::d3d12::runtime_d3d12::unload_effects()
{
	runtime::unload_effects();
}

void reshade::d3d12::runtime_d3d12::render_technique(technique &technique)
{
}

#if RESHADE_GUI
void reshade::d3d12::runtime_d3d12::render_imgui_draw_data(ImDrawData *draw_data)
{
}
#endif
