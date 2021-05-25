/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "runtime_d3d12.hpp"
#include "runtime_objects.hpp"
#include "reshade_api_type_utils.hpp"
#include <CoreWindow.h>
#include <d3dcompiler.h>

extern const char *dxgi_format_to_string(DXGI_FORMAT format);

reshade::d3d12::runtime_impl::runtime_impl(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain) :
	api_object_impl(swapchain),
	_device_impl(device),
	_cmd_queue_impl(queue)
{
	_renderer_id = D3D_FEATURE_LEVEL_12_0;

	// There is no swap chain in d3d12on7
	if (com_ptr<IDXGIFactory4> factory;
		_orig != nullptr && SUCCEEDED(_orig->GetParent(IID_PPV_ARGS(&factory))))
	{
		const LUID luid = _device_impl->_orig->GetAdapterLuid();

		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&dxgi_adapter))))
		{
			if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
			{
				_vendor_id = desc.VendorId;
				_device_id = desc.DeviceId;

				LOG(INFO) << "Running on " << desc.Description;
			}
		}
	}

	if (_orig != nullptr && !on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
}
reshade::d3d12::runtime_impl::~runtime_impl()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d12::runtime_impl::on_init()
{
	assert(_orig != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	// Get description from IDXGISwapChain interface, since later versions are slightly different
	if (FAILED(_orig->GetDesc(&swap_desc)))
		return false;

	// Update window handle in swap chain description for UWP applications
	if (HWND hwnd = nullptr; SUCCEEDED(_orig->GetHwnd(&hwnd)))
		swap_desc.OutputWindow = hwnd;
	else if (com_ptr<ICoreWindowInterop> window_interop; // Get window handle of the core window
		SUCCEEDED(_orig->GetCoreWindow(IID_PPV_ARGS(&window_interop))) && SUCCEEDED(window_interop->get_WindowHandle(&hwnd)))
		swap_desc.OutputWindow = hwnd;

	return on_init(swap_desc);
}
bool reshade::d3d12::runtime_impl::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc)
{
	if (swap_desc.SampleDesc.Count > 1)
		return false; // Multisampled swap chains are not currently supported

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

	// Allocate descriptor heaps
	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		desc.NumDescriptors = swap_desc.BufferCount * 2;

		if (FAILED(_device_impl->_orig->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_backbuffer_rtvs))))
			return false;
		_backbuffer_rtvs->SetName(L"ReShade RTV heap");
	}

	// Get back buffer textures (skip on d3d12on7 devices, since there is no swap chain there)
	_backbuffers.resize(swap_desc.BufferCount);
	if (_orig != nullptr)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();

		for (unsigned int i = 0; i < swap_desc.BufferCount; ++i)
		{
			if (FAILED(_orig->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i]))))
				return false;

			for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV])
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = srgb_write_enable ?
					convert_format(api::format_to_default_typed_srgb(_backbuffer_format)) :
					convert_format(api::format_to_default_typed(_backbuffer_format));
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

				_device_impl->_orig->CreateRenderTargetView(_backbuffers[i].get(), &rtv_desc, rtv_handle);
			}
		}
	}

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d12::runtime_impl::on_reset()
{
	runtime::on_reset();

	// Make sure none of the resources below are currently in use (provided the runtime was initialized previously)
	_cmd_queue_impl->wait_idle();

	_backbuffers.clear();
	_backbuffer_rtvs.reset();
}

void reshade::d3d12::runtime_impl::on_present()
{
	if (!_is_initialized)
		return;

	// There is no swap chain in d3d12on7
	if (_orig != nullptr)
		_swap_index = _orig->GetCurrentBackBufferIndex();

	update_and_render_effects();
	runtime::on_present();
}
bool reshade::d3d12::runtime_impl::on_present(ID3D12Resource *source, HWND hwnd)
{
	assert(source != nullptr);

	// Reinitialize runtime when the source texture dimensions changes
	const D3D12_RESOURCE_DESC source_desc = source->GetDesc();
	if (source_desc.Width != _width || source_desc.Height != _height || convert_format(source_desc.Format) != _backbuffer_format)
	{
		on_reset();

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Width = static_cast<UINT>(source_desc.Width);
		swap_desc.BufferDesc.Height = source_desc.Height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.SampleDesc = source_desc.SampleDesc;
		swap_desc.BufferCount = 3; // Cycle between three fake back buffers
		swap_desc.OutputWindow = hwnd;

		if (!on_init(swap_desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
			return false;
		}
	}

	_swap_index = (_swap_index + 1) % 3;

	// Update source texture render target view
	assert(_backbuffers.size() == 3);
	if (_backbuffers[_swap_index] != source)
	{
		_backbuffers[_swap_index]  = source;

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();
		rtv_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] * 2 * _swap_index;

		for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV])
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = srgb_write_enable ?
				convert_format(api::format_to_default_typed_srgb(_backbuffer_format)) :
				convert_format(api::format_to_default_typed(_backbuffer_format));
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			_device_impl->_orig->CreateRenderTargetView(source, &rtv_desc, rtv_handle);
		}
	}

	on_present();
	return true;
}
bool reshade::d3d12::runtime_impl::on_layer_submit(UINT eye, ID3D12Resource *source, const float bounds[4], ID3D12Resource **target)
{
	assert(eye < 2 && source != nullptr);

	D3D12_RESOURCE_DESC source_desc = source->GetDesc();

	if (source_desc.SampleDesc.Count > 1)
		return false; // When the resource is multisampled, 'CopyTextureRegion' can only copy whole subresources

	D3D12_BOX source_region = { 0, 0, 0, static_cast<UINT>(source_desc.Width), source_desc.Height, 1 };
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

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Width = target_width;
		swap_desc.BufferDesc.Height = region_height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.SampleDesc = source_desc.SampleDesc;
		swap_desc.BufferCount = 1;

		if (!on_init(swap_desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
			return false;
		}

		assert(_backbuffers.size() == 1);

		source_desc.Width = target_width;
		source_desc.Height = region_height;
		source_desc.DepthOrArraySize = 1;
		source_desc.MipLevels = 1;
		source_desc.Format = convert_format(api::format_to_typeless(convert_format(source_desc.Format)));

		const D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT };

		if (HRESULT hr = _device_impl->_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &source_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&_backbuffers[0])); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create region texture!" << " HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << source_desc.Width << ", Height = " << source_desc.Height << ", Format = " << source_desc.Format << ", Flags = " << std::hex << source_desc.Flags << std::dec;
			return false;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();

		for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_RTV])
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = srgb_write_enable ?
				convert_format(api::format_to_default_typed_srgb(_backbuffer_format)) :
				convert_format(api::format_to_default_typed(_backbuffer_format));
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			_device_impl->_orig->CreateRenderTargetView(_backbuffers[0].get(), &rtv_desc, rtv_handle);
		}
	}

	ID3D12GraphicsCommandList *const cmd_list = static_cast<command_list_immediate_impl *>(_cmd_queue_impl->get_immediate_command_list())->begin_commands();

	D3D12_RESOURCE_BARRIER transitions[2];
	transitions[0] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transitions[0].Transition.pResource = source;
	transitions[0].Transition.Subresource = 0;
	transitions[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	transitions[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	transitions[1] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transitions[1].Transition.pResource = _backbuffers[0].get();
	transitions[1].Transition.Subresource = 0;
	transitions[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	transitions[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	cmd_list->ResourceBarrier(2, transitions);

	// Copy region of the source texture (in case of an array texture, copy from the layer corresponding to the current eye)
	D3D12_TEXTURE_COPY_LOCATION src_location;
	src_location.pResource = source;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = source_desc.DepthOrArraySize == 2 ? eye : 0;
	D3D12_TEXTURE_COPY_LOCATION dst_location;
	dst_location.pResource = _backbuffers[0].get();
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_location.SubresourceIndex = 0;
	cmd_list->CopyTextureRegion(&dst_location, eye * region_width, 0, 0, &src_location, &source_region);

	std::swap(transitions[0].Transition.StateBefore, transitions[0].Transition.StateAfter);
	std::swap(transitions[1].Transition.StateBefore, transitions[1].Transition.StateAfter);
	cmd_list->ResourceBarrier(2, transitions);

	*target = _backbuffers[0].get();

	return true;
}

bool reshade::d3d12::runtime_impl::compile_effect(effect &effect, api::shader_stage type, const std::string &entry_point, std::vector<char> &cso)
{
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_47.dll");

	if (_d3d_compiler == nullptr)
	{
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\")!";
		return false;
	}

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	HRESULT hr = E_FAIL;
	const std::string hlsl = effect.preamble + effect.module.hlsl;

	std::string profile;
	switch (type)
	{
	case api::shader_stage::vertex:
		profile = "vs_5_0";
		break;
	case api::shader_stage::pixel:
		profile = "ps_5_0";
		break;
	case api::shader_stage::compute:
		profile = "cs_5_0";
		break;
	}

	UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
	compile_flags |= (_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1);
#ifndef NDEBUG
	compile_flags |= D3DCOMPILE_DEBUG;
#endif

	std::string attributes;
	attributes += "entrypoint=" + entry_point + ';';
	attributes += "profile=" + profile + ';';
	attributes += "flags=" + std::to_string(compile_flags) + ';';

	const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
	if (!load_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]))
	{
		com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
		hr = D3DCompile(
			hlsl.data(), hlsl.size(),
			nullptr, nullptr, nullptr,
			entry_point.c_str(),
			profile.c_str(),
			compile_flags, 0,
			&d3d_compiled, &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		cso.resize(d3d_compiled->GetBufferSize());
		std::memcpy(cso.data(), d3d_compiled->GetBufferPointer(), cso.size());

		if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &d3d_disassembled)))
			effect.assembly[entry_point].assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

		save_effect_cache(effect.source_file, entry_point, hash, cso, effect.assembly[entry_point]);
	}

	return true;
}
