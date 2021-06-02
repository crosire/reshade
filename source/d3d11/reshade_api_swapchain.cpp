/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_device_context.hpp"
#include "reshade_api_swapchain.hpp"
#include "reshade_api_type_convert.hpp"

reshade::d3d11::swapchain_impl::swapchain_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain, device, immediate_context),
	_app_state(device->_orig)
{
	_renderer_id = device->_orig->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(device->_orig->QueryInterface(&dxgi_device)))
	{
		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter)))
		{
			if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
			{
				_vendor_id = desc.VendorId;
				_device_id = desc.DeviceId;

				LOG(INFO) << "Running on " << desc.Description;
			}
		}
	}

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(this);
#endif

	if (_orig != nullptr && !on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << this << '!';
}
reshade::d3d11::swapchain_impl::~swapchain_impl()
{
	on_reset();

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this);
#endif
}

void reshade::d3d11::swapchain_impl::get_back_buffer(uint32_t index, api::resource *out)
{
	assert(index == 0);

	*out = { reinterpret_cast<uintptr_t>(_backbuffer_resolved.get()) };
}

bool reshade::d3d11::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	// Get back buffer texture
	if (FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;
	assert(_backbuffer != nullptr);

	if (swap_desc.SampleDesc.Count > 1)
	{
		D3D11_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = swap_desc.BufferDesc.Width;
		tex_desc.Height = swap_desc.BufferDesc.Height;
		tex_desc.MipLevels = 1;
		tex_desc.ArraySize = 1;
		tex_desc.Format = swap_desc.BufferDesc.Format;
		tex_desc.SampleDesc = { 1, 0 };
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

		if (FAILED(static_cast<device_impl *>(_device)->_orig->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_resolved)))
			return false;
		if (FAILED(static_cast<device_impl *>(_device)->_orig->CreateRenderTargetView(_backbuffer.get(), nullptr, &_backbuffer_rtv)))
			return false;
		if (FAILED(static_cast<device_impl *>(_device)->_orig->CreateShaderResourceView(_backbuffer_resolved.get(), nullptr, &_backbuffer_resolved_srv)))
			return false;
	}
	else
	{
		assert(swap_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT);

		_backbuffer_resolved = _backbuffer;

		// Clear reference to make Unreal Engine 4 happy (https://github.com/EpicGames/UnrealEngine/blob/4.7/Engine/Source/Runtime/Windows/D3D11RHI/Private/D3D11Viewport.cpp#L195)
		_backbuffer->Release();
		assert(_backbuffer.ref_count() == 1);
	}

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_backbuffer_format = convert_format(swap_desc.BufferDesc.Format);

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d11::swapchain_impl::on_reset()
{
	if (_backbuffer != nullptr)
	{
		unsigned int add_references = 0;
		// Resident Evil 3 releases all references to the back buffer before calling 'IDXGISwapChain::ResizeBuffers', even ones it does not own
		// Releasing the references ReShade owns would then make the count negative, which consequently breaks DXGI validation, so reset those references here
		if (_backbuffer.ref_count() == 0)
			add_references = _backbuffer == _backbuffer_resolved ? 2 : 1;
		// Add the reference back that was released because of Unreal Engine 4
		else if (_backbuffer == _backbuffer_resolved)
			add_references = 1;

		for (unsigned int i = 0; i < add_references; ++i)
			_backbuffer->AddRef();
	}

	runtime::on_reset();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_rtv.reset();
	_backbuffer_resolved_srv.reset();
}

void reshade::d3d11::swapchain_impl::on_present()
{
	// Close any render pass that is still open at the end of a frame
	if (static_cast<device_context_impl *>(_graphics_queue)->_has_open_render_pass)
	{
		reshade::invoke_addon_event<reshade::addon_event::finish_render_pass>(static_cast<device_context_impl *>(_graphics_queue));
		static_cast<device_context_impl *>(_graphics_queue)->_has_open_render_pass = false;
	}

	if (!is_initialized())
		return;

	ID3D11DeviceContext *const immediate_context = static_cast<device_context_impl *>(_graphics_queue)->_orig;
	_app_state.capture(immediate_context);

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		immediate_context->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, convert_format(_backbuffer_format));

	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
	{
		immediate_context->IASetInputLayout(nullptr);
		const uintptr_t null = 0;
		immediate_context->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D11Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		immediate_context->VSSetShader(static_cast<device_impl *>(_device)->_copy_vert_shader.get(), nullptr, 0);
		immediate_context->HSSetShader(nullptr, nullptr, 0);
		immediate_context->DSSetShader(nullptr, nullptr, 0);
		immediate_context->GSSetShader(nullptr, nullptr, 0);
		immediate_context->PSSetShader(static_cast<device_impl *>(_device)->_copy_pixel_shader.get(), nullptr, 0);
		ID3D11SamplerState *const samplers[] = { static_cast<device_impl *>(_device)->_copy_sampler_state.get() };
		immediate_context->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
		ID3D11ShaderResourceView *const srvs[] = { _backbuffer_resolved_srv.get() };
		immediate_context->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		immediate_context->RSSetState(nullptr);
		const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(_width), static_cast<FLOAT>(_height), 0.0f, 1.0f };
		immediate_context->RSSetViewports(1, &viewport);
		immediate_context->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		immediate_context->OMSetDepthStencilState(nullptr, D3D11_DEFAULT_STENCIL_REFERENCE);
		ID3D11RenderTargetView *const render_targets[] = { _backbuffer_rtv.get() };
		immediate_context->OMSetRenderTargets(ARRAYSIZE(render_targets), render_targets, nullptr);

		immediate_context->Draw(3, 0);
	}

	// Apply previous state from application
	_app_state.apply_and_release();
}
bool reshade::d3d11::swapchain_impl::on_layer_submit(UINT eye, ID3D11Texture2D *source, const float bounds[4], ID3D11Texture2D **target)
{
	assert(eye < 2 && source != nullptr);

	D3D11_TEXTURE2D_DESC source_desc;
	source->GetDesc(&source_desc);

	if (source_desc.SampleDesc.Count > 1)
		return false; // When the resource is multisampled, 'CopySubresourceRegion' can only copy whole subresources

	D3D11_BOX source_region = { 0, 0, 0, source_desc.Width, source_desc.Height, 1 };
	if (bounds != nullptr)
	{
		source_region.left = static_cast<UINT>(source_desc.Width * std::min(bounds[0], bounds[2]));
		source_region.top  = static_cast<UINT>(source_desc.Height * std::min(bounds[1], bounds[3]));
		source_region.right = static_cast<UINT>(source_desc.Width * std::max(bounds[0], bounds[2]));
		source_region.bottom = static_cast<UINT>(source_desc.Height * std::max(bounds[1], bounds[3]));
	}

	const UINT region_width = source_region.right - source_region.left;
	const UINT target_width = region_width * 2;
	const UINT region_height = source_region.bottom - source_region.top;

	if (region_width == 0 || region_height == 0)
		return false;

	if (target_width != _width || region_height != _height || convert_format(source_desc.Format) != _backbuffer_format)
	{
		on_reset();

		source_desc.Width = target_width;
		source_desc.Height = region_height;
		source_desc.MipLevels = 1;
		source_desc.ArraySize = 1;
		source_desc.Format = convert_format(api::format_to_typeless(convert_format(source_desc.Format)));
		source_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		if (HRESULT hr = static_cast<device_impl *>(_device)->_orig->CreateTexture2D(&source_desc, nullptr, &_backbuffer); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create region texture!" << " HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << source_desc.Width << ", Height = " << source_desc.Height << ", Format = " << source_desc.Format;
			return false;
		}

		_is_vr = true;
		_width = target_width;
		_height = region_height;
		_backbuffer_format = convert_format(source_desc.Format);

		if (!runtime::on_init(nullptr))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << this << '!';
			return false;
		}
	}

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	static_cast<device_context_impl *>(_graphics_queue)->_orig->CopySubresourceRegion(_backbuffer.get(), 0, eye * region_width, 0, 0, source, source_desc.ArraySize == 2 ? eye : 0, &source_region);

	*target = _backbuffer.get();

	return true;
}
