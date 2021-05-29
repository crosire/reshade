/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "runtime.hpp"
#include "reshade_api_device.hpp"
#include "state_block.hpp"

namespace reshade::d3d10
{
	class swapchain_impl : public api::api_object_impl<IDXGISwapChain *, runtime>
	{
	public:
		swapchain_impl(device_impl *device, IDXGISwapChain *swapchain);
		~swapchain_impl();

		api::device *get_device() final { return _device_impl; }
		api::command_queue *get_command_queue() final { return _device_impl; }

		void get_current_back_buffer(api::resource *out) final
		{
			*out = { (uintptr_t)_backbuffer.get() };
		}
		void get_current_back_buffer_target(bool srgb, api::resource_view *out) final
		{
			*out = { reinterpret_cast<uintptr_t>(_backbuffer_rtv[srgb ? 1 : 0].get()) };
		}

		bool on_init();
		void on_reset();
		void on_present();

	private:
		device_impl *const _device_impl;

		state_block _app_state;

		com_ptr<ID3D10PixelShader> _copy_pixel_shader;
		com_ptr<ID3D10VertexShader> _copy_vertex_shader;
		com_ptr<ID3D10SamplerState> _copy_sampler_state;

		com_ptr<ID3D10Texture2D> _backbuffer;
		com_ptr<ID3D10Texture2D> _backbuffer_resolved;
		com_ptr<ID3D10RenderTargetView> _backbuffer_rtv[3];
		com_ptr<ID3D10Texture2D> _backbuffer_texture;
		com_ptr<ID3D10ShaderResourceView> _backbuffer_texture_srv;

		com_ptr<ID3D10RasterizerState> _effect_rasterizer;
	};
}
