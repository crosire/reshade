/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "state_block_d3d10.hpp"

namespace reshade::d3d10
{
	class runtime_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		runtime_impl(device_impl *device, IDXGISwapChain *swapchain);
		~runtime_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _device_impl; }

		bool on_init();
		void on_reset();
		void on_present();

		bool capture_screenshot(uint8_t *buffer) const final;

		bool compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso) final;

		api::resource_view get_backbuffer(bool srgb) final { return { reinterpret_cast<uintptr_t>(_backbuffer_rtv[srgb ? 1 : 0].get()) }; }
		api::resource get_backbuffer_resource() final { return { (uintptr_t)_backbuffer.get() }; }
		api::format get_backbuffer_format() final { return (api::format)_backbuffer_format; }

	private:
		const com_ptr<ID3D10Device1> _device;
		device_impl *const _device_impl;

		state_block _app_state;

		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler_state;

		DXGI_FORMAT _backbuffer_format = DXGI_FORMAT_UNKNOWN;
		com_ptr<ID3D10Texture2D> _backbuffer;
		com_ptr<ID3D10Texture2D> _backbuffer_resolved;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv;

		HMODULE _d3d_compiler = nullptr;
		com_ptr<ID3D10RasterizerState> _effect_rasterizer;
	};
}
