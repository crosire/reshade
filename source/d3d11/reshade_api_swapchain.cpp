/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "reshade_api_swapchain.hpp"
#include "reshade_api_type_convert.hpp"

extern bool is_windows7();

extern const char *dxgi_format_to_string(DXGI_FORMAT format);

reshade::d3d11::swapchain_impl::swapchain_impl(device_impl *device, device_context_impl *immediate_context, IDXGISwapChain *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device),
	_immediate_context_impl(immediate_context),
	_app_state(device->_orig)
{
	_renderer_id = _device_impl->_orig->GetFeatureLevel();

	if (com_ptr<IDXGIDevice> dxgi_device;
		SUCCEEDED(_device_impl->_orig->QueryInterface(&dxgi_device)))
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

bool reshade::d3d11::swapchain_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	return on_init(swap_desc);
}
bool reshade::d3d11::swapchain_impl::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc)
{
	_width = _window_width = swap_desc.BufferDesc.Width;
	_height = _window_height = swap_desc.BufferDesc.Height;
	_backbuffer_format = convert_format(swap_desc.BufferDesc.Format);

	if (swap_desc.OutputWindow != nullptr)
	{
		RECT window_rect = {};
		GetClientRect(swap_desc.OutputWindow, &window_rect);
		_window_width = window_rect.right;
		_window_height = window_rect.bottom;
	}

	// Get back buffer texture (skip when there is no swap chain, in which case it should already have been set in 'on_present')
	if (_orig != nullptr && FAILED(_orig->GetBuffer(0, IID_PPV_ARGS(&_backbuffer))))
		return false;
	assert(_backbuffer != nullptr);

	D3D11_TEXTURE2D_DESC tex_desc = {};
	tex_desc.Width = _width;
	tex_desc.Height = _height;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = convert_format(api::format_to_typeless(_backbuffer_format));
	tex_desc.SampleDesc = { 1, 0 };
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

	// Creating a render target view for the back buffer fails on Windows 8+, so use a intermediate texture there
	if (_orig != nullptr && (
		swap_desc.SampleDesc.Count > 1 ||
		api::format_to_default_typed(_backbuffer_format) != _backbuffer_format ||
		!is_windows7()))
	{
		if (FAILED(_device_impl->_orig->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_resolved)))
			return false;
		if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer.get(), nullptr, &_backbuffer_rtv[2])))
			return false;
	}
	else
	{
		_backbuffer_resolved = _backbuffer;
	}

	// Create back buffer shader texture
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(_device_impl->_orig->CreateTexture2D(&tex_desc, nullptr, &_backbuffer_texture)))
		return false;
	_device_impl->set_debug_name({ reinterpret_cast<uintptr_t>(_backbuffer_texture.get()) }, "ReShade back buffer");

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	srv_desc.Format = convert_format(api::format_to_default_typed(_backbuffer_format));
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
	if (FAILED(_device_impl->_orig->CreateShaderResourceView(_backbuffer_texture.get(), &srv_desc, &_backbuffer_texture_srv)))
		return false;

	D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = convert_format(api::format_to_default_typed(_backbuffer_format));
	rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[0])))
		return false;
	rtv_desc.Format = convert_format(api::format_to_default_typed_srgb(_backbuffer_format));
	if (FAILED(_device_impl->_orig->CreateRenderTargetView(_backbuffer_resolved.get(), &rtv_desc, &_backbuffer_rtv[1])))
		return false;

	// Clear reference to make Unreal Engine 4 happy (which checks the reference count)
	_backbuffer->Release();

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
		else if (_is_initialized)
			add_references = 1;

		for (unsigned int i = 0; i < add_references; ++i)
			_backbuffer->AddRef();
	}

	runtime::on_reset();

	_backbuffer.reset();
	_backbuffer_resolved.reset();
	_backbuffer_rtv[0].reset();
	_backbuffer_rtv[1].reset();
	_backbuffer_rtv[2].reset();
	_backbuffer_texture.reset();
	_backbuffer_texture_srv.reset();
}

void reshade::d3d11::swapchain_impl::on_present()
{
	if (!_is_initialized)
		return;

	ID3D11DeviceContext *const immediate_context = _immediate_context_impl->_orig;
	_app_state.capture(immediate_context);

	// Resolve MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
		immediate_context->ResolveSubresource(_backbuffer_resolved.get(), 0, _backbuffer.get(), 0, convert_format(_backbuffer_format));

	update_and_render_effects();
	runtime::on_present();

	// Stretch main render target back into MSAA back buffer if MSAA is active
	if (_backbuffer_resolved != _backbuffer)
	{
		immediate_context->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		immediate_context->IASetInputLayout(nullptr);
		const uintptr_t null = 0;
		immediate_context->IASetVertexBuffers(0, 1, reinterpret_cast<ID3D11Buffer *const *>(&null), reinterpret_cast<const UINT *>(&null), reinterpret_cast<const UINT *>(&null));
		immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		immediate_context->VSSetShader(_device_impl->_copy_vert_shader.get(), nullptr, 0);
		immediate_context->HSSetShader(nullptr, nullptr, 0);
		immediate_context->DSSetShader(nullptr, nullptr, 0);
		immediate_context->GSSetShader(nullptr, nullptr, 0);
		immediate_context->PSSetShader(_device_impl->_copy_pixel_shader.get(), nullptr, 0);
		ID3D11SamplerState *const samplers[] = { _device_impl->_copy_sampler_state.get() };
		immediate_context->PSSetSamplers(0, ARRAYSIZE(samplers), samplers);
		ID3D11ShaderResourceView *const srvs[] = { _backbuffer_texture_srv.get() };
		immediate_context->PSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		immediate_context->RSSetState(nullptr);
		const D3D11_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(_width), static_cast<FLOAT>(_height), 0.0f, 1.0f };
		immediate_context->RSSetViewports(1, &viewport);
		immediate_context->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
		immediate_context->OMSetDepthStencilState(nullptr, D3D11_DEFAULT_STENCIL_REFERENCE);
		ID3D11RenderTargetView *const render_targets[] = { _backbuffer_rtv[2].get() };
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

		_is_vr = true;

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Width = target_width;
		swap_desc.BufferDesc.Height = region_height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.SampleDesc = source_desc.SampleDesc;
		swap_desc.BufferCount = 1;

		source_desc.Width = target_width;
		source_desc.Height = region_height;
		source_desc.MipLevels = 1;
		source_desc.ArraySize = 1;
		source_desc.Format = convert_format(api::format_to_typeless(convert_format(source_desc.Format)));
		source_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		if (HRESULT hr = _device_impl->_orig->CreateTexture2D(&source_desc, nullptr, &_backbuffer); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create region texture!" << " HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << source_desc.Width << ", Height = " << source_desc.Height << ", Format = " << source_desc.Format;
			return false;
		}

		if (!on_init(swap_desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 11 runtime environment on runtime " << this << '!';
			return false;
		}
	}

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	_immediate_context_impl->_orig->CopySubresourceRegion(_backbuffer.get(), 0, eye * region_width, 0, 0, source, source_desc.ArraySize == 2 ? eye : 0, &source_region);

	*target = _backbuffer.get();

	return true;
}
