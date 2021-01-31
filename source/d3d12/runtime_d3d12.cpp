/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "runtime_d3d12.hpp"
#include "runtime_d3d12_objects.hpp"
#include "dxgi/format_utils.hpp"
#include <CoreWindow.h>
#include <d3dcompiler.h>

#define D3D12_RESOURCE_STATE_SHADER_RESOURCE \
	(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)

reshade::d3d12::runtime_d3d12::runtime_d3d12(device_impl *device, command_queue_impl *queue, IDXGISwapChain3 *swapchain) :
	_device_impl(device), _device(device->_device), _swapchain(swapchain), _commandqueue_impl(queue), _commandqueue(queue->_queue), _cmd_impl(static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list()))
{
	_renderer_id = D3D_FEATURE_LEVEL_12_0;

	// There is no swap chain in d3d12on7
	if (com_ptr<IDXGIFactory4> factory;
		_swapchain != nullptr && SUCCEEDED(_swapchain->GetParent(IID_PPV_ARGS(&factory))))
	{
		const LUID luid = _device->GetAdapterLuid();

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

	if (_swapchain != nullptr && !on_init())
		LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
}
reshade::d3d12::runtime_d3d12::~runtime_d3d12()
{
	on_reset();

	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d12::runtime_d3d12::on_init()
{
	assert(_swapchain != nullptr);

	DXGI_SWAP_CHAIN_DESC swap_desc;
	// Get description from IDXGISwapChain interface, since later versions are slightly different
	if (FAILED(_swapchain->GetDesc(&swap_desc)))
		return false;

	// Update window handle in swap chain description for UWP applications
	if (HWND hwnd = nullptr; SUCCEEDED(_swapchain->GetHwnd(&hwnd)))
		swap_desc.OutputWindow = hwnd;
	else if (com_ptr<ICoreWindowInterop> window_interop; // Get window handle of the core window
		SUCCEEDED(_swapchain->GetCoreWindow(IID_PPV_ARGS(&window_interop))) && SUCCEEDED(window_interop->get_WindowHandle(&hwnd)))
		swap_desc.OutputWindow = hwnd;

	return on_init(swap_desc);
}
bool reshade::d3d12::runtime_d3d12::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc)
{
	RECT window_rect = {};
	GetClientRect(swap_desc.OutputWindow, &window_rect);

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_window_width = window_rect.right;
	_window_height = window_rect.bottom;
	_color_bit_depth = dxgi_format_color_depth(swap_desc.BufferDesc.Format);
	_backbuffer_format = swap_desc.BufferDesc.Format;

	// Allocate descriptor heaps
	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		desc.NumDescriptors = swap_desc.BufferCount * 2;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_backbuffer_rtvs))))
			return false;
		_backbuffer_rtvs->SetName(L"ReShade RTV heap");
	}
	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
		desc.NumDescriptors = 1;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_depthstencil_dsvs))))
			return false;
		_depthstencil_dsvs->SetName(L"ReShade DSV heap");
	}

	// Get back buffer textures (skip on d3d12on7 devices, since there is no swap chain there)
	_backbuffers.resize(swap_desc.BufferCount);
	if (_swapchain != nullptr)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();

		for (unsigned int i = 0; i < swap_desc.BufferCount; ++i)
		{
			if (FAILED(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i]))))
				return false;

			for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _device_impl->rtv_handle_size)
			{
				D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
				rtv_desc.Format = srgb_write_enable ?
					make_dxgi_format_srgb(_backbuffer_format) :
					make_dxgi_format_normal(_backbuffer_format);
				rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

				_device->CreateRenderTargetView(_backbuffers[i].get(), &rtv_desc, rtv_handle);
			}
		}
	}

	// Create back buffer shader texture
	{   D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
		desc.Width = _width;
		desc.Height = _height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = make_dxgi_format_typeless(_backbuffer_format);
		desc.SampleDesc = { 1, 0 };
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_DEFAULT };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&_backbuffer_texture))))
			return false;
		_backbuffer_texture->SetName(L"ReShade back buffer");
	}

	// Create effect stencil resource
	{   D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
		desc.Width = _width;
		desc.Height = _height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.SampleDesc = { 1, 0 };
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_DEFAULT };

		D3D12_CLEAR_VALUE clear_value = {};
		clear_value.Format = desc.Format;
		clear_value.DepthStencil = { 1.0f, 0x0 };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&_effect_stencil))))
			return false;
		_effect_stencil->SetName(L"ReShade stencil buffer");

		_device->CreateDepthStencilView(_effect_stencil.get(), nullptr, _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart());
	}

#if RESHADE_GUI
	if (!init_imgui_resources())
		return false;
#endif

	return runtime::on_init(swap_desc.OutputWindow);
}
void reshade::d3d12::runtime_d3d12::on_reset()
{
	runtime::on_reset();

	// Make sure none of the resources below are currently in use (provided the runtime was initialized previously)
	_cmd_impl->flush_and_wait(_commandqueue.get());

	_backbuffers.clear();
	_backbuffer_rtvs.reset();
	_backbuffer_texture.reset();
	_depthstencil_dsvs.reset();

	_effect_stencil.reset();

#if RESHADE_GUI
	for (unsigned int i = 0; i < NUM_IMGUI_BUFFERS; ++i)
	{
		_imgui.indices[i].reset();
		_imgui.vertices[i].reset();
		_imgui.num_indices[i] = 0;
		_imgui.num_vertices[i] = 0;
	}
#endif
}

void reshade::d3d12::runtime_d3d12::on_present()
{
	if (!_is_initialized)
		return;

	// There is no swap chain in d3d12on7
	if (_swapchain != nullptr)
		_swap_index = _swapchain->GetCurrentBackBufferIndex();

	ID3D12GraphicsCommandList *const cmd_list = _cmd_impl->get();

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = _backbuffers[_swap_index].get();
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	cmd_list->ResourceBarrier(1, &transition);

	update_and_render_effects();
	runtime::on_present();

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	cmd_list->ResourceBarrier(1, &transition);

	_cmd_impl->flush(_commandqueue.get());
}
void reshade::d3d12::runtime_d3d12::on_present(ID3D12Resource *source, HWND hwnd)
{
	// Reinitialize runtime when the source texture dimensions changes
	const D3D12_RESOURCE_DESC source_desc = source->GetDesc();
	if (source_desc.Width != _width || source_desc.Height != _height || source_desc.Format != _backbuffer_format)
	{
		on_reset();

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferDesc.Width = static_cast<UINT>(source_desc.Width);
		swap_desc.BufferDesc.Height = source_desc.Height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.BufferCount = 3; // Cycle between three fake back buffers
		swap_desc.OutputWindow = hwnd;

		if (!on_init(swap_desc))
		{
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << this << '!';
			return;
		}
	}

	_swap_index = (_swap_index + 1) % 3;

	// Update source texture render target view
	assert(_backbuffers.size() == 3);
	if (_backbuffers[_swap_index] != source)
	{
		_backbuffers[_swap_index]  = source;

		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();
		rtv_handle.ptr += _device_impl->rtv_handle_size * 2 * _swap_index;

		for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _device_impl->rtv_handle_size)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = srgb_write_enable ?
				make_dxgi_format_srgb(_backbuffer_format) :
				make_dxgi_format_normal(_backbuffer_format);
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			_device->CreateRenderTargetView(source, &rtv_desc, rtv_handle);
		}
	}

	on_present();
}

bool reshade::d3d12::runtime_d3d12::capture_screenshot(uint8_t *buffer) const
{
	if (_color_bit_depth != 8 && _color_bit_depth != 10)
	{
		if (const char *format_string = format_to_string(_backbuffer_format); format_string != nullptr)
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << format_string << '!';
		else
			LOG(ERROR) << "Screenshots are not supported for back buffer format " << _backbuffer_format << '!';
		return false;
	}

	const uint32_t data_pitch = _width * 4;
	const uint32_t download_pitch = (data_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	desc.Width = _height * download_pitch;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_READBACK };

	com_ptr<ID3D12Resource> intermediate;
	if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&intermediate)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width;
		return false;
	}
	intermediate->SetName(L"ReShade screenshot texture");

	ID3D12GraphicsCommandList *const cmd_list = _cmd_impl->get();

	// Was transitioned to D3D12_RESOURCE_STATE_RENDER_TARGET in 'on_present' already
	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = _backbuffers[_swap_index].get();
	transition.Transition.Subresource = 0;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	cmd_list->ResourceBarrier(1, &transition);

	{
		D3D12_TEXTURE_COPY_LOCATION src_location = { _backbuffers[_swap_index].get() };
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_location.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dst_location = { intermediate.get() };
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_location.PlacedFootprint.Footprint.Width = _width;
		dst_location.PlacedFootprint.Footprint.Height = _height;
		dst_location.PlacedFootprint.Footprint.Depth = 1;
		dst_location.PlacedFootprint.Footprint.Format = make_dxgi_format_normal(_backbuffer_format);
		dst_location.PlacedFootprint.Footprint.RowPitch = download_pitch;

		cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	cmd_list->ResourceBarrier(1, &transition);

	// Execute and wait for completion
	_cmd_impl->flush_and_wait(_commandqueue.get());

	// Copy data from system memory texture into output buffer
	uint8_t *mapped_data;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return false;

	for (uint32_t y = 0; y < _height; y++, buffer += data_pitch, mapped_data += download_pitch)
	{
		if (_color_bit_depth == 10)
		{
			for (uint32_t x = 0; x < data_pitch; x += 4)
			{
				const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_data + x);
				// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
				buffer[x + 0] = ( (rgba & 0x000003FF)        /  4) & 0xFF;
				buffer[x + 1] = (((rgba & 0x000FFC00) >> 10) /  4) & 0xFF;
				buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) /  4) & 0xFF;
				buffer[x + 3] = (((rgba & 0xC0000000) >> 30) * 85) & 0xFF;
			}
		}
		else
		{
			std::memcpy(buffer, mapped_data, data_pitch);

			if (_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM ||
				_backbuffer_format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
			{
				// Format is BGRA, but output should be RGBA, so flip channels
				for (uint32_t x = 0; x < data_pitch; x += 4)
					std::swap(buffer[x + 0], buffer[x + 2]);
			}
		}
	}

	intermediate->Unmap(0, nullptr);

	return true;
}

bool reshade::d3d12::runtime_d3d12::init_effect(size_t index)
{
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_47.dll");

	if (_d3d_compiler == nullptr)
	{
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\")!";
		return false;
	}

	effect &effect = _effects[index];

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	const std::string hlsl = effect.preamble + effect.module.hlsl;
	std::unordered_map<std::string, std::vector<char>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
	{
		HRESULT hr = E_FAIL;

		std::string profile;
		switch (entry_point.type)
		{
		case reshadefx::shader_type::vs:
			profile = "vs_5_0";
			break;
		case reshadefx::shader_type::ps:
			profile = "ps_5_0";
			break;
		case reshadefx::shader_type::cs:
			profile = "cs_5_0";
			break;
		}

		UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
		compile_flags |= (_performance_mode ? D3DCOMPILE_OPTIMIZATION_LEVEL3 : D3DCOMPILE_OPTIMIZATION_LEVEL1);
#ifndef NDEBUG
		compile_flags |= D3DCOMPILE_DEBUG;
#endif

		std::string attributes;
		attributes += "entrypoint=" + entry_point.name + ';';
		attributes += "profile=" + profile + ';';
		attributes += "flags=" + std::to_string(compile_flags) + ';';

		const size_t hash = std::hash<std::string_view>()(attributes) ^ std::hash<std::string_view>()(hlsl);
		std::vector<char> &cso = entry_points[entry_point.name];
		if (!load_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]))
		{
			com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
			hr = D3DCompile(
				hlsl.data(), hlsl.size(),
				nullptr, nullptr, nullptr,
				entry_point.name.c_str(),
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
				effect.assembly[entry_point.name].assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

			save_effect_cache(effect.source_file, entry_point.name, hash, cso, effect.assembly[entry_point.name]);
		}
	}

	if (index >= _effect_data.size())
		_effect_data.resize(index + 1);
	effect_data &effect_data = _effect_data[index];

	{   D3D12_DESCRIPTOR_RANGE srv_range = {};
		srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_range.NumDescriptors = effect.module.num_texture_bindings;
		srv_range.BaseShaderRegister = 0;
		D3D12_DESCRIPTOR_RANGE sampler_range = {};
		sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		sampler_range.NumDescriptors = effect.module.num_sampler_bindings;
		sampler_range.BaseShaderRegister = 0;
		D3D12_DESCRIPTOR_RANGE uav_range = {};
		uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uav_range.NumDescriptors = effect.module.num_storage_bindings;
		uav_range.BaseShaderRegister = 0;

		D3D12_ROOT_PARAMETER params[4] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0; // b0 (global constant buffer)
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &srv_range;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable.NumDescriptorRanges = 1;
		params[2].DescriptorTable.pDescriptorRanges = &sampler_range;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[3].DescriptorTable.NumDescriptorRanges = 1;
		params[3].DescriptorTable.pDescriptorRanges = &uav_range;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = effect.module.num_storage_bindings == 0 ? 3 : 4;
		desc.pParameters = params;

		effect_data.signature = _device_impl->create_root_signature(desc);
		if (effect_data.signature == nullptr)
		{
			LOG(ERROR) << "Failed to create root signature for effect file '" << effect.source_file << "'!";
			return false;
		}
	}

	if (!effect.uniform_data_storage.empty())
	{
		D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
		desc.Width = effect.uniform_data_storage.size();
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

		if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&effect_data.cb)); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create constant buffer for effect file '" << effect.source_file << "'! HRESULT is " << hr << '.';
			LOG(DEBUG) << "> Details: Width = " << desc.Width;
			return false;
		}
		effect_data.cb->SetName(L"ReShade constant buffer");

		effect_data.cbv_gpu_address = effect_data.cb->GetGPUVirtualAddress();
	}

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		for (const reshadefx::technique_info &info : effect.module.techniques)
			desc.NumDescriptors += static_cast<UINT>(8 * info.passes.size());

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.rtv_heap))))
			return false;
		effect_data.rtv_heap->SetName(L"ReShade effect RTV heap");

		effect_data.rtv_cpu_base = effect_data.rtv_heap->GetCPUDescriptorHandleForHeapStart();
	}

	UINT num_passes = 0;
	for (const reshadefx::technique_info &info : effect.module.techniques)
		num_passes += static_cast<UINT>(info.passes.size());

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		desc.NumDescriptors = (effect.module.num_texture_bindings + effect.module.num_storage_bindings) * num_passes;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.srv_uav_heap))))
			return false;
		effect_data.srv_uav_heap->SetName(L"ReShade effect SRV heap");

		effect_data.srv_cpu_base = effect_data.srv_uav_heap->GetCPUDescriptorHandleForHeapStart();
		effect_data.srv_gpu_base = effect_data.srv_uav_heap->GetGPUDescriptorHandleForHeapStart();

		effect_data.uav_cpu_base = effect_data.srv_cpu_base;
		effect_data.uav_cpu_base.ptr += effect.module.num_texture_bindings * num_passes * _device_impl->srv_handle_size;
		effect_data.uav_gpu_base = effect_data.srv_gpu_base;
		effect_data.uav_gpu_base.ptr += effect.module.num_texture_bindings * num_passes * _device_impl->srv_handle_size;
	}

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER };
		desc.NumDescriptors = effect.module.num_sampler_bindings;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.sampler_heap))))
			return false;
		effect_data.sampler_heap->SetName(L"ReShade effect sampler heap");

		effect_data.sampler_cpu_base = effect_data.sampler_heap->GetCPUDescriptorHandleForHeapStart();
		effect_data.sampler_gpu_base = effect_data.sampler_heap->GetGPUDescriptorHandleForHeapStart();
	}

	UINT16 sampler_list = 0;
	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding >= D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind sampler '" << info.unique_name << "' since it exceeds the maximum number of allowed sampler slots in " << "D3D12" << " (" << info.binding << ", allowed are up to " << D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT << ").";
			return false;
		}

		// Only initialize sampler if it has not been created before
		if (0 == (sampler_list & (1 << info.binding)))
		{
			sampler_list |= (1 << info.binding); // D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT is 16, so a 16-bit integer is enough to hold all bindings

			D3D12_SAMPLER_DESC desc;
			desc.Filter = static_cast<D3D12_FILTER>(info.filter);
			desc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_u);
			desc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_v);
			desc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_w);
			desc.MipLODBias = info.lod_bias;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			std::memset(desc.BorderColor, 0, sizeof(desc.BorderColor));
			desc.MinLOD = info.min_lod;
			desc.MaxLOD = info.max_lod;

			D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle = effect_data.sampler_cpu_base;
			sampler_handle.ptr += info.binding * _device_impl->sampler_handle_size;

			_device->CreateSampler(&desc, sampler_handle);
		}
	}

	// The render target descriptor table is shared across all techniques in the effect
	D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_base = effect_data.srv_gpu_base;
	D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_base = effect_data.srv_cpu_base;
	D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu_base = effect_data.uav_gpu_base;
	D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu_base = effect_data.uav_cpu_base;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu_base = effect_data.rtv_cpu_base;

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		auto impl = new technique_data();
		technique.impl = impl;

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			pass_data &pass_data = impl->passes[pass_index];
			reshadefx::pass_info &pass_info = technique.passes[pass_index];

			if (!pass_info.cs_entry_point.empty())
			{
				impl->has_compute_passes = true;

				D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = effect_data.signature.get();

				const auto &CS = entry_points.at(pass_info.cs_entry_point);
				pso_desc.CS = { CS.data(), CS.size() };

				pso_desc.NodeMask = 1;

				if (HRESULT hr = _device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pass_data.pipeline)); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create compute pipeline for pass " << pass_index << " in technique '" << technique.name << "'! HRESULT is " << hr << '.';
					return false;
				}
			}
			else
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
				pso_desc.pRootSignature = effect_data.signature.get();

				const auto &VS = entry_points.at(pass_info.vs_entry_point);
				pso_desc.VS = { VS.data(), VS.size() };
				const auto &PS = entry_points.at(pass_info.ps_entry_point);
				pso_desc.PS = { PS.data(), PS.size() };

				// Keep track of the base handle, which is followed by a contiguous range of render target descriptors
				pass_data.render_targets = rtv_cpu_base;
				rtv_cpu_base.ptr += 8 * _device_impl->rtv_handle_size;

				for (UINT k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
				{
					tex_data *const tex_impl = static_cast<tex_data *>(
						look_up_texture_by_name(pass_info.render_target_names[k]).impl);

					pass_data.modified_resources.push_back(tex_impl);

					const D3D12_RESOURCE_DESC desc = tex_impl->resource->GetDesc();

					D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = pass_data.render_targets;
					rtv_handle.ptr += k * _device_impl->rtv_handle_size;

					D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
					rtv_desc.Format = pass_info.srgb_write_enable ?
						make_dxgi_format_srgb(desc.Format) :
						make_dxgi_format_normal(desc.Format);
					rtv_desc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;

					_device->CreateRenderTargetView(tex_impl->resource.get(), &rtv_desc, rtv_handle);

					pso_desc.NumRenderTargets = pass_data.num_render_targets = k + 1;
					pso_desc.RTVFormats[k] = rtv_desc.Format;
				}

				if (pass_info.render_target_names[0].empty())
				{
					pso_desc.NumRenderTargets = 1;
					pso_desc.RTVFormats[0] = pass_info.srgb_write_enable ?
						make_dxgi_format_srgb(_backbuffer_format) :
						make_dxgi_format_normal(_backbuffer_format);

					pass_info.viewport_width = _width;
					pass_info.viewport_height = _height;
				}

				pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
				pso_desc.SampleMask = UINT_MAX;
				pso_desc.SampleDesc = { 1, 0 };
				pso_desc.NodeMask = 1;

				switch (pass_info.topology)
				{
				case reshadefx::primitive_topology::point_list:
					pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
					break;
				case reshadefx::primitive_topology::line_list:
				case reshadefx::primitive_topology::line_strip:
					pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
					break;
				case reshadefx::primitive_topology::triangle_list:
				case reshadefx::primitive_topology::triangle_strip:
					pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
					break;
				}

				{   D3D12_BLEND_DESC &desc = pso_desc.BlendState;
					desc.AlphaToCoverageEnable = FALSE;
					desc.IndependentBlendEnable = FALSE;
					desc.RenderTarget[0].BlendEnable = pass_info.blend_enable;

					const auto convert_blend_op = [](reshadefx::pass_blend_op value) {
						switch (value)
						{
						default:
						case reshadefx::pass_blend_op::add: return D3D12_BLEND_OP_ADD;
						case reshadefx::pass_blend_op::subtract: return D3D12_BLEND_OP_SUBTRACT;
						case reshadefx::pass_blend_op::rev_subtract: return D3D12_BLEND_OP_REV_SUBTRACT;
						case reshadefx::pass_blend_op::min: return D3D12_BLEND_OP_MIN;
						case reshadefx::pass_blend_op::max: return D3D12_BLEND_OP_MAX;
						}
					};
					const auto convert_blend_func = [](reshadefx::pass_blend_func value) {
						switch (value) {
						case reshadefx::pass_blend_func::zero: return D3D12_BLEND_ZERO;
						default:
						case reshadefx::pass_blend_func::one: return D3D12_BLEND_ONE;
						case reshadefx::pass_blend_func::src_color: return D3D12_BLEND_SRC_COLOR;
						case reshadefx::pass_blend_func::src_alpha: return D3D12_BLEND_SRC_ALPHA;
						case reshadefx::pass_blend_func::inv_src_color: return D3D12_BLEND_INV_SRC_COLOR;
						case reshadefx::pass_blend_func::inv_src_alpha: return D3D12_BLEND_INV_SRC_ALPHA;
						case reshadefx::pass_blend_func::dst_color: return D3D12_BLEND_DEST_COLOR;
						case reshadefx::pass_blend_func::dst_alpha: return D3D12_BLEND_DEST_ALPHA;
						case reshadefx::pass_blend_func::inv_dst_color: return D3D12_BLEND_INV_DEST_COLOR;
						case reshadefx::pass_blend_func::inv_dst_alpha: return D3D12_BLEND_INV_DEST_ALPHA;
						}
					};

					desc.RenderTarget[0].SrcBlend = convert_blend_func(pass_info.src_blend);
					desc.RenderTarget[0].DestBlend = convert_blend_func(pass_info.dest_blend);
					desc.RenderTarget[0].BlendOp = convert_blend_op(pass_info.blend_op);
					desc.RenderTarget[0].SrcBlendAlpha = convert_blend_func(pass_info.src_blend_alpha);
					desc.RenderTarget[0].DestBlendAlpha = convert_blend_func(pass_info.dest_blend_alpha);
					desc.RenderTarget[0].BlendOpAlpha = convert_blend_op(pass_info.blend_op_alpha);
					desc.RenderTarget[0].RenderTargetWriteMask = pass_info.color_write_mask;
				}

				{   D3D12_RASTERIZER_DESC &desc = pso_desc.RasterizerState;
					desc.FillMode = D3D12_FILL_MODE_SOLID;
					desc.CullMode = D3D12_CULL_MODE_NONE;
					desc.DepthClipEnable = TRUE;
				}

				{   D3D12_DEPTH_STENCIL_DESC &desc = pso_desc.DepthStencilState;
					desc.DepthEnable = FALSE;
					desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
					desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

					const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
						switch (value) {
						case reshadefx::pass_stencil_op::zero: return D3D12_STENCIL_OP_ZERO;
						default:
						case reshadefx::pass_stencil_op::keep: return D3D12_STENCIL_OP_KEEP;
						case reshadefx::pass_stencil_op::invert: return D3D12_STENCIL_OP_INVERT;
						case reshadefx::pass_stencil_op::replace: return D3D12_STENCIL_OP_REPLACE;
						case reshadefx::pass_stencil_op::incr: return D3D12_STENCIL_OP_INCR;
						case reshadefx::pass_stencil_op::incr_sat: return D3D12_STENCIL_OP_INCR_SAT;
						case reshadefx::pass_stencil_op::decr: return D3D12_STENCIL_OP_DECR;
						case reshadefx::pass_stencil_op::decr_sat: return D3D12_STENCIL_OP_DECR_SAT;
						}
					};
					const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) {
						switch (value)
						{
						case reshadefx::pass_stencil_func::never: return D3D12_COMPARISON_FUNC_NEVER;
						case reshadefx::pass_stencil_func::equal: return D3D12_COMPARISON_FUNC_EQUAL;
						case reshadefx::pass_stencil_func::not_equal: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
						case reshadefx::pass_stencil_func::less: return D3D12_COMPARISON_FUNC_LESS;
						case reshadefx::pass_stencil_func::less_equal: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
						case reshadefx::pass_stencil_func::greater: return D3D12_COMPARISON_FUNC_GREATER;
						case reshadefx::pass_stencil_func::greater_equal: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
						default:
						case reshadefx::pass_stencil_func::always: return D3D12_COMPARISON_FUNC_ALWAYS;
						}
					};

					desc.StencilEnable = pass_info.stencil_enable;
					desc.StencilReadMask = pass_info.stencil_read_mask;
					desc.StencilWriteMask = pass_info.stencil_write_mask;
					desc.FrontFace.StencilFailOp = convert_stencil_op(pass_info.stencil_op_fail);
					desc.FrontFace.StencilDepthFailOp = convert_stencil_op(pass_info.stencil_op_depth_fail);
					desc.FrontFace.StencilPassOp = convert_stencil_op(pass_info.stencil_op_pass);
					desc.FrontFace.StencilFunc = convert_stencil_func(pass_info.stencil_comparison_func);
					desc.BackFace = desc.FrontFace;
				}

				if (HRESULT hr = _device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pass_data.pipeline)); FAILED(hr))
				{
					LOG(ERROR) << "Failed to create graphics pipeline for pass " << pass_index << " in technique '" << technique.name << "'! HRESULT is " << hr << '.';
					return false;
				}
			}

			pass_data.srv_handle = srv_gpu_base;
			for (const reshadefx::sampler_info &info : pass_info.samplers)
			{
				if (info.texture_binding >= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
				{
					LOG(ERROR) << "Cannot bind texture '" << info.texture_name << "' since it exceeds the maximum number of allowed resource slots in " << "D3D12" << " (" << info.texture_binding << ", allowed are up to " << D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT << ").";
					return false;
				}

				const texture &texture = look_up_texture_by_name(info.texture_name);

				D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = srv_cpu_base;
				srv_handle.ptr += info.texture_binding * _device_impl->srv_handle_size;

				com_ptr<ID3D12Resource> resource;
				if (texture.semantic == "COLOR")
				{
					resource = _backbuffer_texture;
				}
				else if (!texture.semantic.empty())
				{
					if (const auto it = _texture_semantic_bindings.find(texture.semantic);
						it != _texture_semantic_bindings.end())
						resource = it->second;

					// Keep track of the texture descriptor to simplify updating it
					effect_data.texture_semantic_to_binding[texture.semantic].push_back(srv_handle);
				}
				else
				{
					resource = static_cast<tex_data *>(texture.impl)->resource;
				}

				if (resource != nullptr)
				{
					D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
					desc.Format = info.srgb ?
						make_dxgi_format_srgb(resource->GetDesc().Format) :
						make_dxgi_format_normal(resource->GetDesc().Format);
					desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					desc.Texture2D.MipLevels = texture.levels;

					_device->CreateShaderResourceView(resource.get(), &desc, srv_handle);
				}
			}
			srv_cpu_base.ptr += effect.module.num_texture_bindings * _device_impl->srv_handle_size;
			srv_gpu_base.ptr += effect.module.num_texture_bindings * _device_impl->srv_handle_size;

			pass_data.uav_handle = uav_gpu_base;
			for (const reshadefx::storage_info &info : pass_info.storages)
			{
				if (info.binding >= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
				{
					LOG(ERROR) << "Cannot bind storage '" << info.unique_name << "' since it exceeds the maximum number of allowed resource slots in " << "D3D12" << " (" << info.binding << ", allowed are up to " << D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT << ").";
					return false;
				}

				const texture &texture = look_up_texture_by_name(info.texture_name);

				D3D12_CPU_DESCRIPTOR_HANDLE uav_handle = uav_cpu_base;
				uav_handle.ptr += info.binding * _device_impl->srv_handle_size;

				const com_ptr<ID3D12Resource> resource =
					static_cast<tex_data *>(texture.impl)->resource;
				if (resource != nullptr)
				{
					D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
					desc.Format = make_dxgi_format_normal(resource->GetDesc().Format);
					desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					desc.Texture2D.MipSlice = 0;

					_device->CreateUnorderedAccessView(resource.get(), nullptr, &desc, uav_handle);

					pass_data.modified_resources.push_back(static_cast<tex_data *>(texture.impl));
				}
			}
			uav_cpu_base.ptr += effect.module.num_storage_bindings * _device_impl->srv_handle_size;
			uav_gpu_base.ptr += effect.module.num_storage_bindings * _device_impl->srv_handle_size;
		}
	}

	return true;
}
void reshade::d3d12::runtime_d3d12::unload_effect(size_t index)
{
	// Make sure no effect resources are currently in use
	_cmd_impl->flush_and_wait(_commandqueue.get());

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);

	if (index < _effect_data.size())
	{
		effect_data &effect_data = _effect_data[index];
		effect_data.cb.reset();
		effect_data.signature.reset();
		effect_data.rtv_heap.reset();
		effect_data.srv_uav_heap.reset();
		effect_data.sampler_heap.reset();
		effect_data.texture_semantic_to_binding.clear();
	}
}
void reshade::d3d12::runtime_d3d12::unload_effects()
{
	// Make sure no effect resources are currently in use
	_cmd_impl->flush_and_wait(_commandqueue.get());

	for (technique &tech : _techniques)
	{
		delete static_cast<technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effects();

	_effect_data.clear();
}

bool reshade::d3d12::runtime_d3d12::init_texture(texture &texture)
{
	auto impl = new tex_data();
	texture.impl = impl;

	// Do not create resource if it is a special reference, those are set in 'render_technique' and 'update_texture_bindings'
	if (!texture.semantic.empty())
		return true;

	D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
	desc.Width = texture.width;
	desc.Height = texture.height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = static_cast<UINT16>(texture.levels);
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	if (texture.render_target)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (texture.storage_access || texture.levels > 1) // Need UAV for mipmap generation
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		desc.Format = DXGI_FORMAT_R8_UNORM;
		break;
	case reshadefx::texture_format::r16f:
		desc.Format = DXGI_FORMAT_R16_FLOAT;
		break;
	case reshadefx::texture_format::r32f:
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		break;
	case reshadefx::texture_format::rg8:
		desc.Format = DXGI_FORMAT_R8G8_UNORM;
		break;
	case reshadefx::texture_format::rg16:
		desc.Format = DXGI_FORMAT_R16G16_UNORM;
		break;
	case reshadefx::texture_format::rg16f:
		desc.Format = DXGI_FORMAT_R16G16_FLOAT;
		break;
	case reshadefx::texture_format::rg32f:
		desc.Format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case reshadefx::texture_format::rgba8:
		desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	case reshadefx::texture_format::rgba16:
		desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
		break;
	case reshadefx::texture_format::rgba16f:
		desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case reshadefx::texture_format::rgba32f:
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case reshadefx::texture_format::rgb10a2:
		desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
		break;
	}

	// Render targets are always either cleared to zero or not cleared at all (see 'ClearRenderTargets' pass state), so can set the optimized clear value here to zero
	D3D12_CLEAR_VALUE clear_value = {};
	clear_value.Format = make_dxgi_format_normal(desc.Format);

	D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_DEFAULT };

	if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_SHADER_RESOURCE, texture.render_target ? &clear_value : nullptr, IID_PPV_ARGS(&impl->resource)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width << ", Height = " << desc.Height << ", Levels = " << desc.MipLevels << ", Format = " << desc.Format << ", Flags = " << std::hex << desc.Flags << std::dec;
		return false;
	}

	std::wstring debug_name;
	debug_name.reserve(texture.unique_name.size());
	utf8::unchecked::utf8to16(texture.unique_name.begin(), texture.unique_name.end(), std::back_inserter(debug_name));
	impl->resource->SetName(debug_name.c_str());

	{	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		heap_desc.NumDescriptors = texture.levels /* SRV */ + texture.levels - 1 /* UAV */;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&impl->descriptors))))
			return false;
		debug_name += L" SRV heap";
		impl->descriptors->SetName(debug_name.c_str());
	}

	D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_handle = impl->descriptors->GetCPUDescriptorHandleForHeapStart();

	for (uint32_t level = 0; level < texture.levels; ++level, srv_cpu_handle.ptr += _device_impl->srv_handle_size)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = make_dxgi_format_normal(desc.Format);
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = level;

		_device->CreateShaderResourceView(impl->resource.get(), &srv_desc, srv_cpu_handle);
	}

	// Generate UAVs for mipmap generation
	for (uint32_t level = 1; level < texture.levels; ++level, srv_cpu_handle.ptr += _device_impl->srv_handle_size)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.Format = make_dxgi_format_normal(desc.Format);
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = level;

		_device->CreateUnorderedAccessView(impl->resource.get(), nullptr, &uav_desc, srv_cpu_handle);
	}

	return true;
}
void reshade::d3d12::runtime_d3d12::upload_texture(const texture &texture, const uint8_t *pixels)
{
	auto impl = static_cast<tex_data *>(texture.impl);
	assert(impl != nullptr && texture.semantic.empty() && pixels != nullptr);

	const uint32_t data_pitch = texture.width * 4;
	const uint32_t upload_pitch = (data_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	desc.Width = texture.height * upload_pitch;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

	com_ptr<ID3D12Resource> intermediate;
	if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create system memory texture for updating texture '" << texture.unique_name << "'! HRESULT is " << hr << '.';
		LOG(DEBUG) << "> Details: Width = " << desc.Width << ", Height = " << desc.Height;
		return;
	}
	intermediate->SetName(L"ReShade upload texture");

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return;

	bool unsupported_format = false;
	switch (texture.format)
	{
	case reshadefx::texture_format::r8:
		for (uint32_t y = 0; y < texture.height; ++y, mapped_data += upload_pitch, pixels += data_pitch)
			for (uint32_t x = 0; x < texture.width; ++x)
				mapped_data[x] = pixels[x * 4];
		break;
	case reshadefx::texture_format::rg8:
		for (uint32_t y = 0; y < texture.height; ++y, mapped_data += upload_pitch, pixels += data_pitch)
			for (uint32_t x = 0; x < texture.width; ++x)
				mapped_data[x * 2 + 0] = pixels[x * 4 + 0],
				mapped_data[x * 2 + 1] = pixels[x * 4 + 1];
		break;
	case reshadefx::texture_format::rgba8:
		for (uint32_t y = 0; y < texture.height; ++y, mapped_data += upload_pitch, pixels += data_pitch)
			std::memcpy(mapped_data, pixels, data_pitch);
		break;
	default:
		unsupported_format = true;
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << " of texture '" << texture.unique_name << "'!";
		break;
	}

	intermediate->Unmap(0, nullptr);

	if (unsupported_format)
		return;

	ID3D12GraphicsCommandList *const cmd_list = _cmd_impl->get();

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = impl->resource.get();
	transition.Transition.Subresource = 0;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_SHADER_RESOURCE;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	cmd_list->ResourceBarrier(1, &transition);

	{ // Copy data from upload buffer into target texture
		D3D12_TEXTURE_COPY_LOCATION src_location = { intermediate.get() };
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src_location.PlacedFootprint.Footprint.Width = texture.width;
		src_location.PlacedFootprint.Footprint.Height = texture.height;
		src_location.PlacedFootprint.Footprint.Depth = 1;
		src_location.PlacedFootprint.Footprint.Format = impl->resource->GetDesc().Format;
		src_location.PlacedFootprint.Footprint.RowPitch = upload_pitch;

		D3D12_TEXTURE_COPY_LOCATION dst_location = { impl->resource.get() };
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_location.SubresourceIndex = 0;

		cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	cmd_list->ResourceBarrier(1, &transition);

	generate_mipmaps(impl);

	// Execute and wait for completion
	_cmd_impl->flush_and_wait(_commandqueue.get());
}
void reshade::d3d12::runtime_d3d12::destroy_texture(texture &texture)
{
	// Make sure texture is not still in use before destroying it
	_cmd_impl->flush_and_wait(_commandqueue.get());

	delete static_cast<tex_data *>(texture.impl);
	texture.impl = nullptr;
}
void reshade::d3d12::runtime_d3d12::generate_mipmaps(const tex_data *impl)
{
	assert(impl != nullptr);

	const D3D12_RESOURCE_DESC desc = impl->resource->GetDesc();
	if (desc.MipLevels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	ID3D12GraphicsCommandList *const cmd_list = _cmd_impl->get();
	cmd_list->SetComputeRootSignature(_device_impl->_mipmap_signature.get());
	cmd_list->SetPipelineState(_device_impl->_mipmap_pipeline.get());
	ID3D12DescriptorHeap *const descriptor_heap = impl->descriptors.get();
	cmd_list->SetDescriptorHeaps(1, &descriptor_heap);

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = impl->resource.get();
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_SHADER_RESOURCE;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmd_list->ResourceBarrier(1, &transition);

	for (uint32_t level = 1; level < desc.MipLevels; ++level)
	{
		const uint32_t width = std::max(1u, static_cast<uint32_t>(desc.Width) >> level);
		const uint32_t height = std::max(1u, desc.Height >> level);

		static const auto float_as_uint = [](float value) { return *reinterpret_cast<uint32_t *>(&value); };
		cmd_list->SetComputeRoot32BitConstant(0, float_as_uint(1.0f / width), 0);
		cmd_list->SetComputeRoot32BitConstant(0, float_as_uint(1.0f / height), 1);
		// Bind next higher mipmap level as input
		cmd_list->SetComputeRootDescriptorTable(1, { impl->descriptors->GetGPUDescriptorHandleForHeapStart().ptr + _device_impl->srv_handle_size * (level - 1) });
		// There is no UAV for level 0, so substract one
		cmd_list->SetComputeRootDescriptorTable(2, { impl->descriptors->GetGPUDescriptorHandleForHeapStart().ptr + _device_impl->srv_handle_size * (desc.MipLevels + level - 1) });

		cmd_list->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
		barrier.UAV.pResource = impl->resource.get();
		cmd_list->ResourceBarrier(1, &barrier);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	cmd_list->ResourceBarrier(1, &transition);
}

void reshade::d3d12::runtime_d3d12::render_technique(technique &technique)
{
	const auto impl = static_cast<technique_data *>(technique.impl);
	effect_data &effect_data = _effect_data[technique.effect_index];

	ID3D12GraphicsCommandList *const cmd_list = _cmd_impl->get();

	RESHADE_ADDON_EVENT(reshade_before_effects, this, _cmd_impl);

	ID3D12DescriptorHeap *const descriptor_heaps[] = { effect_data.srv_uav_heap.get(), effect_data.sampler_heap.get() };
	cmd_list->SetDescriptorHeaps(ARRAYSIZE(descriptor_heaps), descriptor_heaps);
	cmd_list->SetGraphicsRootSignature(effect_data.signature.get());
	if (impl->has_compute_passes)
		cmd_list->SetComputeRootSignature(effect_data.signature.get());

	// Setup shader constants
	if (effect_data.cb != nullptr)
	{
		if (void *mapped;
			SUCCEEDED(effect_data.cb->Map(0, nullptr, &mapped)))
		{
			std::memcpy(mapped, _effects[technique.effect_index].uniform_data_storage.data(), _effects[technique.effect_index].uniform_data_storage.size());
			effect_data.cb->Unmap(0, nullptr);
		}

		cmd_list->SetGraphicsRootConstantBufferView(0, effect_data.cbv_gpu_address);
		if (impl->has_compute_passes)
			cmd_list->SetComputeRootConstantBufferView(0, effect_data.cbv_gpu_address);
	}

	// Setup samplers
	cmd_list->SetGraphicsRootDescriptorTable(2, effect_data.sampler_gpu_base);
	if (impl->has_compute_passes)
		cmd_list->SetComputeRootDescriptorTable(2, effect_data.sampler_gpu_base);

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated
	D3D12_CPU_DESCRIPTOR_HANDLE effect_stencil = _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart();

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			D3D12_RESOURCE_BARRIER transitions[2];
			transitions[0] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			transitions[0].Transition.pResource = _backbuffer_texture.get();
			transitions[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transitions[0].Transition.StateBefore = D3D12_RESOURCE_STATE_SHADER_RESOURCE;
			transitions[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			transitions[1] = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
			transitions[1].Transition.pResource = _backbuffers[_swap_index].get();
			transitions[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transitions[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			transitions[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
			cmd_list->ResourceBarrier(2, transitions);

			// Save back buffer of previous pass
			cmd_list->CopyResource(_backbuffer_texture.get(), _backbuffers[_swap_index].get());

			std::swap(transitions[0].Transition.StateBefore, transitions[0].Transition.StateAfter);
			std::swap(transitions[1].Transition.StateBefore, transitions[1].Transition.StateAfter);
			cmd_list->ResourceBarrier(2, transitions);
		}

		if (pass_index > 0)
		{
			// Set descriptor heaps again, since they may have been changed by 'generate_mipmaps' of previous pass
			cmd_list->SetDescriptorHeaps(ARRAYSIZE(descriptor_heaps), descriptor_heaps);
		}

		const pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

		cmd_list->SetPipelineState(pass_data.pipeline.get());

		if (!pass_info.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_backbuffer_copy = false;

			cmd_list->SetComputeRootDescriptorTable(1, pass_data.srv_handle);
			cmd_list->SetComputeRootDescriptorTable(3, pass_data.uav_handle);

			cmd_list->Dispatch(pass_info.viewport_width, pass_info.viewport_height, pass_info.viewport_dispatch_z);
		}
		else
		{
			cmd_list->SetGraphicsRootDescriptorTable(1, pass_data.srv_handle);

			// Transition resource state for render targets
			if (!pass_data.modified_resources.empty())
			{
				std::vector<D3D12_RESOURCE_BARRIER> transitions;
				transitions.reserve(pass_data.modified_resources.size());
				for (const auto modified_resource : pass_data.modified_resources)
				{
					D3D12_RESOURCE_BARRIER &transition = transitions.emplace_back();
					transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
					transition.Transition.pResource = modified_resource->resource.get();
					transition.Transition.Subresource = 0;
					transition.Transition.StateBefore = D3D12_RESOURCE_STATE_SHADER_RESOURCE;
					transition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
				}
				cmd_list->ResourceBarrier(static_cast<UINT>(transitions.size()), transitions.data());
			}

			cmd_list->OMSetStencilRef(pass_info.stencil_reference_value);

			// Setup render targets
			const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

			if (pass_info.stencil_enable && !is_effect_stencil_cleared)
			{
				is_effect_stencil_cleared = true;

				cmd_list->ClearDepthStencilView(effect_stencil, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			}

			if (pass_data.num_render_targets == 0)
			{
				needs_implicit_backbuffer_copy = true;

				D3D12_CPU_DESCRIPTOR_HANDLE render_target = { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + (_swap_index * 2 + pass_info.srgb_write_enable) * _device_impl->rtv_handle_size };
				cmd_list->OMSetRenderTargets(1, &render_target, false, pass_info.stencil_enable ? &effect_stencil : nullptr);

				if (pass_info.clear_render_targets)
					cmd_list->ClearRenderTargetView(render_target, clear_color, 0, nullptr);
			}
			else
			{
				needs_implicit_backbuffer_copy = false;

				cmd_list->OMSetRenderTargets(pass_data.num_render_targets, &pass_data.render_targets, true,
					pass_info.stencil_enable && pass_info.viewport_width == _width && pass_info.viewport_height == _height ? &effect_stencil : nullptr);

				if (pass_info.clear_render_targets)
					for (UINT k = 0; k < pass_data.num_render_targets; ++k)
						cmd_list->ClearRenderTargetView({ pass_data.render_targets.ptr + k * _device_impl->rtv_handle_size }, clear_color, 0, nullptr);
			}

			const D3D12_RECT scissor_rect = { 0, 0, static_cast<LONG>(pass_info.viewport_width), static_cast<LONG>(pass_info.viewport_height) };
			const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(pass_info.viewport_width), static_cast<FLOAT>(pass_info.viewport_height), 0.0f, 1.0f };
			cmd_list->RSSetViewports(1, &viewport);
			cmd_list->RSSetScissorRects(1, &scissor_rect);

			// Draw primitives
			D3D_PRIMITIVE_TOPOLOGY topology;
			switch (pass_info.topology)
			{
			case reshadefx::primitive_topology::point_list:
				topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
				break;
			case reshadefx::primitive_topology::line_list:
				topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
				break;
			case reshadefx::primitive_topology::line_strip:
				topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
				break;
			default:
			case reshadefx::primitive_topology::triangle_list:
				topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				break;
			case reshadefx::primitive_topology::triangle_strip:
				topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				break;
			}
			cmd_list->IASetPrimitiveTopology(topology);
			cmd_list->DrawInstanced(pass_info.num_vertices, 1, 0, 0);

			// Transition resource state back to shader access
			if (!pass_data.modified_resources.empty())
			{
				std::vector<D3D12_RESOURCE_BARRIER> transitions;
				transitions.reserve(pass_data.modified_resources.size());
				for (const auto modified_resource : pass_data.modified_resources)
				{
					D3D12_RESOURCE_BARRIER &transition = transitions.emplace_back();
					transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
					transition.Transition.pResource = modified_resource->resource.get();
					transition.Transition.Subresource = 0;
					transition.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
					transition.Transition.StateAfter = D3D12_RESOURCE_STATE_SHADER_RESOURCE;
				}
				cmd_list->ResourceBarrier(static_cast<UINT>(transitions.size()), transitions.data());
			}
		}

		// Generate mipmaps for modified resources
		for (const tex_data *modified_texture : pass_data.modified_resources)
			generate_mipmaps(modified_texture);
	}

	RESHADE_ADDON_EVENT(reshade_after_effects, this, _cmd_impl);
}

void reshade::d3d12::runtime_d3d12::update_texture_bindings(const char *semantic, api::resource_view_handle srv)
{
	assert(srv.handle == 0 || (srv.handle & 1) == 1);
	const auto resource = srv.handle != 0 ? reinterpret_cast<ID3D12Resource *>(srv.handle ^ 1) : nullptr;

	// Descriptors may be currently in use, so make sure all previous frames have finished before updating them
	_cmd_impl->flush_and_wait(_commandqueue.get());

	// This potentially destroys the previous resource, so it must not be in use on the GPU at this point anymore!
	_texture_semantic_bindings[semantic] = resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
	if (resource != nullptr)
	{
		const D3D12_RESOURCE_DESC desc = resource->GetDesc();

		srv_desc.Format = make_dxgi_format_normal(desc.Format);
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = desc.MipLevels;
	}
	else
	{
		// Need to provide a description so descriptor type can be determined
		// See https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
	}

	// Either create a shader resource view or set a null descriptor
	for (effect_data &effect_data : _effect_data)
		for (D3D12_CPU_DESCRIPTOR_HANDLE binding : effect_data.texture_semantic_to_binding[semantic])
			_device->CreateShaderResourceView(resource, &srv_desc, binding);
}
