/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "hook_manager.hpp"
#include "runtime_d3d12.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "dxgi/format_utils.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <d3dcompiler.h>

#define D3D12_RESOURCE_STATE_SHADER_RESOURCE \
	(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)

namespace reshade::d3d12
{
	struct d3d12_tex_data
	{
		com_ptr<ID3D12Resource> resource;
		com_ptr<ID3D12DescriptorHeap> descriptors;
	};

	struct d3d12_pass_data
	{
		com_ptr<ID3D12PipelineState> pipeline;
		UINT num_render_targets;
		D3D12_CPU_DESCRIPTOR_HANDLE render_targets;
	};

	struct d3d12_effect_data
	{
		com_ptr<ID3D12Resource> cb;
		com_ptr<ID3D12RootSignature> signature;
		com_ptr<ID3D12DescriptorHeap> srv_heap;
		com_ptr<ID3D12DescriptorHeap> rtv_heap;
		com_ptr<ID3D12DescriptorHeap> sampler_heap;

		D3D12_GPU_VIRTUAL_ADDRESS cbv_gpu_address;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_base;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu_base;
		D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE depth_texture_binding = {};
	};

	struct d3d12_technique_data
	{
		std::vector<d3d12_pass_data> passes;
	};

	static void transition_state(
		const com_ptr<ID3D12GraphicsCommandList> &list,
		const com_ptr<ID3D12Resource> &res,
		D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to,
		UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
		transition.Transition.pResource = res.get();
		transition.Transition.Subresource = subresource;
		transition.Transition.StateBefore = from;
		transition.Transition.StateAfter = to;
		list->ResourceBarrier(1, &transition);
	}
}

reshade::d3d12::runtime_d3d12::runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) :
	_device(device), _commandqueue(queue), _swapchain(swapchain)
{
	assert(queue != nullptr);
	assert(device != nullptr);

	_renderer_id = D3D_FEATURE_LEVEL_12_0;

	if (com_ptr<IDXGIFactory4> factory;
		swapchain != nullptr && SUCCEEDED(swapchain->GetParent(IID_PPV_ARGS(&factory))))
	{
		const LUID luid = device->GetAdapterLuid();

		if (com_ptr<IDXGIAdapter> dxgi_adapter;
			SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&dxgi_adapter))))
		{
			DXGI_ADAPTER_DESC desc;
			if (SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
				_vendor_id = desc.VendorId, _device_id = desc.DeviceId;
		}
	}

#if RESHADE_GUI && RESHADE_DEPTH
	subscribe_to_ui("DX12", [this]() {
		assert(_buffer_detection != nullptr);
		draw_depth_debug_menu(*_buffer_detection);
	});
#endif
#if RESHADE_DEPTH
	subscribe_to_load_config([this](const ini_file &config) {
		config.get("DX12_BUFFER_DETECTION", "DepthBufferRetrievalMode", _preserve_depth_buffers);
		config.get("DX12_BUFFER_DETECTION", "DepthBufferClearingNumber", _depth_clear_index_override);
		config.get("DX12_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);

		if (_depth_clear_index_override == 0)
			// Zero is not a valid clear index, since it disables depth buffer preservation
			_depth_clear_index_override = std::numeric_limits<UINT>::max();
	});
	subscribe_to_save_config([this](ini_file &config) {
		config.set("DX12_BUFFER_DETECTION", "DepthBufferRetrievalMode", _preserve_depth_buffers);
		config.set("DX12_BUFFER_DETECTION", "DepthBufferClearingNumber", _depth_clear_index_override);
		config.set("DX12_BUFFER_DETECTION", "UseAspectRatioHeuristics", _filter_aspect_ratio);
	});
#endif
}
reshade::d3d12::runtime_d3d12::~runtime_d3d12()
{
	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d12::runtime_d3d12::on_init(const DXGI_SWAP_CHAIN_DESC &swap_desc
#if RESHADE_D3D12ON7
	, ID3D12Resource *backbuffer
#endif
	)
{
	RECT window_rect = {};
	GetClientRect(swap_desc.OutputWindow, &window_rect);

	_width = swap_desc.BufferDesc.Width;
	_height = swap_desc.BufferDesc.Height;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;
	_color_bit_depth = dxgi_format_color_depth(swap_desc.BufferDesc.Format);
	_backbuffer_format = swap_desc.BufferDesc.Format;

	_srv_handle_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_rtv_handle_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dsv_handle_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_sampler_handle_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

#if RESHADE_D3D12ON7
	if (backbuffer != nullptr)
	{
		_backbuffers.resize(1);
		_backbuffers[0] = backbuffer;
		assert(swap_desc.BufferCount == 1);
	}
#endif

	// Create multiple command allocators to buffer for multiple frames
	_cmd_alloc.resize(swap_desc.BufferCount);
	for (UINT i = 0; i < swap_desc.BufferCount; ++i)
		if (FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc[i]))))
			return false;
	if (FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[0].get(), nullptr, IID_PPV_ARGS(&_cmd_list))))
		return false;
	_cmd_list->Close(); // Immediately close since it will be reset on first use

	// Create auto-reset event and fences for synchronization
	_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fence_event == nullptr)
		return false;
	_fence.resize(swap_desc.BufferCount);
	_fence_value.resize(swap_desc.BufferCount);
	for (UINT i = 0; i < swap_desc.BufferCount; ++i)
		if (FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]))))
			return false;

	// Allocate descriptor heaps
	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		desc.NumDescriptors = swap_desc.BufferCount * 2;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_backbuffer_rtvs))))
			return false;
	}
	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
		desc.NumDescriptors = 1;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_depthstencil_dsvs))))
			return false;
	}

	// Get back buffer textures
	_backbuffers.resize(swap_desc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart();

	for (unsigned int i = 0; i < swap_desc.BufferCount; ++i)
	{
		if (_swapchain != nullptr && FAILED(_swapchain->GetBuffer(i, IID_PPV_ARGS(&_backbuffers[i]))))
			return false;

		assert(_backbuffers[i] != nullptr);
#ifndef NDEBUG
		_backbuffers[i]->SetName(L"Back buffer");
#endif

		for (int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _rtv_handle_size)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			rtv_desc.Format = srgb_write_enable ?
				make_dxgi_format_srgb(_backbuffer_format) :
				make_dxgi_format_normal(_backbuffer_format);
			rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			_device->CreateRenderTargetView(_backbuffers[i].get(), &rtv_desc, rtv_handle);
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
#ifndef NDEBUG
		_backbuffer_texture->SetName(L"ReShade back buffer");
#endif
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
#ifndef NDEBUG
		_effect_stencil->SetName(L"ReShade stencil buffer");
#endif
		_device->CreateDepthStencilView(_effect_stencil.get(), nullptr, _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart());
	}

	// Create mipmap generation states
	{   D3D12_DESCRIPTOR_RANGE srv_range = {};
		srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_range.NumDescriptors = 1;
		srv_range.BaseShaderRegister = 0; // t0
		D3D12_DESCRIPTOR_RANGE uav_range = {};
		uav_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uav_range.NumDescriptors = 1;
		uav_range.BaseShaderRegister = 0; // u0

		D3D12_ROOT_PARAMETER params[3] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0; // b0
		params[0].Constants.Num32BitValues = 2;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &srv_range;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable.NumDescriptorRanges = 1;
		params[2].DescriptorTable.pDescriptorRanges = &uav_range;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC samplers[1] = {};
		samplers[0].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplers[0].ShaderRegister = 0; // s0
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = ARRAYSIZE(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = ARRAYSIZE(samplers);
		desc.pStaticSamplers = samplers;

		_mipmap_signature = create_root_signature(desc);
	}

	{   D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
		pso_desc.pRootSignature = _mipmap_signature.get();

		const resources::data_resource cs = resources::load_data_resource(IDR_MIPMAP_CS);
		pso_desc.CS = { cs.data, cs.data_size };

		if (FAILED(_device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&_mipmap_pipeline))))
			return false;
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
	if (!_fence.empty() && !_fence_value.empty())
		wait_for_command_queue();

	_cmd_list.reset();
	_cmd_alloc.clear();

	CloseHandle(_fence_event);
	_fence.clear();
	_fence_value.clear();

	_backbuffers.clear();
	_backbuffer_rtvs.reset();
	_backbuffer_texture.reset();
	_depthstencil_dsvs.reset();

	_mipmap_pipeline.reset();
	_mipmap_signature.reset();

	_effect_stencil.reset();

#if RESHADE_GUI
	_imgui.pipeline.reset();
	_imgui.signature.reset();

	for (unsigned int i = 0; i < NUM_IMGUI_BUFFERS; ++i)
	{
		_imgui.indices[i].reset();
		_imgui.vertices[i].reset();
		_imgui.num_indices[i] = 0;
		_imgui.num_vertices[i] = 0;
	}
#endif

#if RESHADE_DEPTH
	_depth_texture.reset();

	_has_depth_texture = false;
	_depth_texture_override = nullptr;
#endif
}

void reshade::d3d12::runtime_d3d12::on_present()
{
	if (!_is_initialized)
		return;

	assert(_buffer_detection != nullptr);
	_vertices = _buffer_detection->total_vertices();
	_drawcalls = _buffer_detection->total_drawcalls();

	// There is no swap chain for d3d12on7
	if (_swapchain != nullptr)
		_swap_index = _swapchain->GetCurrentBackBufferIndex();

	// Make sure all commands for this command allocator have finished executing before reseting it
	if (_fence[_swap_index]->GetCompletedValue() < _fence_value[_swap_index])
	{
		if (SUCCEEDED(_fence[_swap_index]->SetEventOnCompletion(_fence_value[_swap_index], _fence_event)))
			WaitForSingleObject(_fence_event, INFINITE); // Event is automatically reset after this wait is released
	}

	// Reset command allocator before using it this frame again
	_cmd_alloc[_swap_index]->Reset();

	if (!begin_command_list())
		return;

#if RESHADE_DEPTH
	assert(_depth_clear_index_override != 0);
	update_depth_texture_bindings(_buffer_detection->update_depth_texture(
		_commandqueue.get(), _cmd_list.get(), _filter_aspect_ratio ? _width : 0, _height, _depth_texture_override, _preserve_depth_buffers ? _depth_clear_index_override : 0));
#endif

	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	update_and_render_effects();
	runtime::on_present();

	// Potentially have to restart command list here because a screenshot was taken
	if (!begin_command_list())
		return;

	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	execute_command_list();

	if (const UINT64 sync_value = _fence_value[_swap_index] + 1;
		SUCCEEDED(_commandqueue->Signal(_fence[_swap_index].get(), sync_value)))
		_fence_value[_swap_index] = sync_value;
}

bool reshade::d3d12::runtime_d3d12::capture_screenshot(uint8_t *buffer) const
{
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
	if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&intermediate))))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture!";
		return false;
	}

#ifndef NDEBUG
	intermediate->SetName(L"ReShade screenshot texture");
#endif

	if (!begin_command_list())
		return false;

	// Was transitioned to D3D12_RESOURCE_STATE_RENDER_TARGET in 'on_present' already
	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
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

		_cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}
	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, 0);

	// Execute and wait for completion
	if (!wait_for_command_queue())
		return false;

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
				buffer[x + 0] = ((rgba & 0x3FF) / 4) & 0xFF;
				buffer[x + 1] = (((rgba & 0xFFC00) >> 10) / 4) & 0xFF;
				buffer[x + 2] = (((rgba & 0x3FF00000) >> 20) / 4) & 0xFF;
				buffer[x + 3] = 0xFF;
			}
		}
		else
		{
			std::memcpy(buffer, mapped_data, data_pitch);

			for (uint32_t x = 0; x < data_pitch; x += 4)
				buffer[x + 3] = 0xFF; // Clear alpha channel
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
		LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\").";
		return false;
	}

	effect &effect = _effects[index];

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));
	const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(_d3d_compiler, "D3DDisassemble"));

	const std::string hlsl = effect.preamble + effect.module.hlsl;
	std::unordered_map<std::string, com_ptr<ID3DBlob>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
	{
		com_ptr<ID3DBlob> d3d_errors;

		const HRESULT hr = D3DCompile(
			hlsl.c_str(), hlsl.size(),
			nullptr, nullptr, nullptr,
			entry_point.name.c_str(),
			entry_point.is_pixel_shader ? "ps_5_0" : "vs_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
			&entry_points[entry_point.name], &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;

		if (com_ptr<ID3DBlob> d3d_disassembled; SUCCEEDED(D3DDisassemble(entry_points[entry_point.name]->GetBufferPointer(), entry_points[entry_point.name]->GetBufferSize(), 0, nullptr, &d3d_disassembled)))
			effect.assembly[entry_point.name] = std::string(static_cast<const char *>(d3d_disassembled->GetBufferPointer()));
	}

	if (index >= _effect_data.size())
		_effect_data.resize(index + 1);
	d3d12_effect_data &effect_data = _effect_data[index];

	{   D3D12_DESCRIPTOR_RANGE srv_range = {};
		srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_range.NumDescriptors = effect.module.num_texture_bindings;
		srv_range.BaseShaderRegister = 0;
		D3D12_DESCRIPTOR_RANGE sampler_range = {};
		sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		sampler_range.NumDescriptors = effect.module.num_sampler_bindings;
		sampler_range.BaseShaderRegister = 0;

		D3D12_ROOT_PARAMETER params[3] = {};
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

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = ARRAYSIZE(params);
		desc.pParameters = params;

		effect_data.signature = create_root_signature(desc);
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

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&effect_data.cb))))
			return false;
#ifndef NDEBUG
		effect_data.cb->SetName(L"ReShade constant buffer");
#endif
		effect_data.cbv_gpu_address = effect_data.cb->GetGPUVirtualAddress();
	}

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		desc.NumDescriptors = effect.module.num_texture_bindings;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.srv_heap))))
			return false;

		effect_data.srv_cpu_base = effect_data.srv_heap->GetCPUDescriptorHandleForHeapStart();
		effect_data.srv_gpu_base = effect_data.srv_heap->GetGPUDescriptorHandleForHeapStart();
	}

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		for (const reshadefx::technique_info &info : effect.module.techniques)
			desc.NumDescriptors += static_cast<UINT>(8 * info.passes.size());

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.rtv_heap))))
			return false;

		effect_data.rtv_cpu_base = effect_data.rtv_heap->GetCPUDescriptorHandleForHeapStart();
	}

	{   D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER };
		desc.NumDescriptors = effect.module.num_sampler_bindings;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&effect_data.sampler_heap))))
			return false;

		effect_data.sampler_cpu_base = effect_data.sampler_heap->GetCPUDescriptorHandleForHeapStart();
		effect_data.sampler_gpu_base = effect_data.sampler_heap->GetGPUDescriptorHandleForHeapStart();
	}

	UINT16 sampler_list = 0;

	for (const reshadefx::sampler_info &info : effect.module.samplers)
	{
		if (info.binding >= D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind sampler '" << info.unique_name << "' since it exceeds the maximum number of allowed sampler slots in D3D12 (" << info.binding << ", allowed are up to " << D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT << ").";
			return false;
		}
		if (info.texture_binding >= D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
		{
			LOG(ERROR) << "Cannot bind texture '" << info.texture_name << "' since it exceeds the maximum number of allowed resource slots in D3D12 (" << info.texture_binding << ", allowed are up to " << D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT << ").";
			return false;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = effect_data.srv_cpu_base;
		srv_handle.ptr += info.texture_binding * _srv_handle_size;

		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		assert(existing_texture != _textures.end());

		com_ptr<ID3D12Resource> resource;
		switch (existing_texture->impl_reference)
		{
		case texture_reference::back_buffer:
			resource = _backbuffer_texture;
			break;
		case texture_reference::depth_buffer:
#if RESHADE_DEPTH
			resource = _depth_texture; // Note: This can be a "nullptr"
#endif
			// Keep track of the depth buffer texture descriptor to simplify updating it
			effect_data.depth_texture_binding = srv_handle;
			break;
		default:
			resource = static_cast<d3d12_tex_data *>(existing_texture->impl)->resource;
			break;
		}

		if (resource != nullptr)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format = info.srgb ?
				make_dxgi_format_srgb(resource->GetDesc().Format) :
				make_dxgi_format_normal(resource->GetDesc().Format);
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Texture2D.MipLevels = existing_texture->levels;

			_device->CreateShaderResourceView(resource.get(), &desc, srv_handle);
		}

		// Only initialize sampler if it has not been created before
		if (0 == (sampler_list & (1 << info.binding)))
		{
			sampler_list |= (1 << info.binding); // D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT is 16, so a 16-bit integer is enough to hold all bindings

			D3D12_SAMPLER_DESC desc = {};
			desc.Filter = static_cast<D3D12_FILTER>(info.filter);
			desc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_u);
			desc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_v);
			desc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(info.address_w);
			desc.MipLODBias = info.lod_bias;
			desc.MaxAnisotropy = 1;
			desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			desc.MinLOD = info.min_lod;
			desc.MaxLOD = info.max_lod;

			D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle = effect_data.sampler_cpu_base;
			sampler_handle.ptr += info.binding * _sampler_handle_size;

			_device->CreateSampler(&desc, sampler_handle);
		}
	}

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		auto impl = new d3d12_technique_data();
		technique.impl = impl;

		impl->passes.resize(technique.passes.size());
		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			d3d12_pass_data &pass_data = impl->passes[pass_index];
			reshadefx::pass_info &pass_info = technique.passes[pass_index];

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = effect_data.signature.get();

			const auto &VS = entry_points.at(pass_info.vs_entry_point);
			pso_desc.VS = { VS->GetBufferPointer(), VS->GetBufferSize() };
			const auto &PS = entry_points.at(pass_info.ps_entry_point);
			pso_desc.PS = { PS->GetBufferPointer(), PS->GetBufferSize() };

			pso_desc.NumRenderTargets = 1;
			pso_desc.RTVFormats[0] = pass_info.srgb_write_enable ?
				make_dxgi_format_srgb(_backbuffer_format) :
				make_dxgi_format_normal(_backbuffer_format);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = effect_data.rtv_cpu_base;
			rtv_handle.ptr += pass_index * 8 * _rtv_handle_size;

			// Keep track of base handle, which is followed by a contiguous range of render target descriptors
			pass_data.render_targets = rtv_handle;

			for (UINT k = 0; k < 8 && !pass_info.render_target_names[k].empty(); ++k)
			{
				const auto texture_impl = static_cast<d3d12_tex_data *>(std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				})->impl);
				assert(texture_impl != nullptr);

				D3D12_RENDER_TARGET_VIEW_DESC desc = {};
				desc.Format = pass_info.srgb_write_enable ?
					make_dxgi_format_srgb(texture_impl->resource->GetDesc().Format) :
					make_dxgi_format_normal(texture_impl->resource->GetDesc().Format);
				desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

				_device->CreateRenderTargetView(texture_impl->resource.get(), &desc, rtv_handle);

				pso_desc.RTVFormats[k] = desc.Format;
				pso_desc.NumRenderTargets = pass_data.num_render_targets = k + 1;

				// Increment handle to next descriptor position
				rtv_handle.ptr += _rtv_handle_size;
			}

			if (pass_info.render_target_names[0].empty())
			{
				pass_info.viewport_width = _width;
				pass_info.viewport_height = _height;
			}

			pso_desc.NodeMask = 1;
			pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			pso_desc.SampleMask = UINT_MAX;
			pso_desc.SampleDesc = { 1, 0 };
			pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			{   D3D12_BLEND_DESC &desc = pso_desc.BlendState;
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
					default:
					case reshadefx::pass_blend_func::one: return D3D12_BLEND_ONE;
					case reshadefx::pass_blend_func::zero: return D3D12_BLEND_ZERO;
					case reshadefx::pass_blend_func::src_alpha: return D3D12_BLEND_SRC_ALPHA;
					case reshadefx::pass_blend_func::src_color: return D3D12_BLEND_SRC_COLOR;
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
				desc.DepthClipEnable = true;
			}

			{   D3D12_DEPTH_STENCIL_DESC &desc = pso_desc.DepthStencilState;
				desc.DepthEnable = FALSE;
				desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
					switch (value) {
					default:
					case reshadefx::pass_stencil_op::keep: return D3D12_STENCIL_OP_KEEP;
					case reshadefx::pass_stencil_op::zero: return D3D12_STENCIL_OP_ZERO;
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
					default:
					case reshadefx::pass_stencil_func::always: return D3D12_COMPARISON_FUNC_ALWAYS;
					case reshadefx::pass_stencil_func::never: return D3D12_COMPARISON_FUNC_NEVER;
					case reshadefx::pass_stencil_func::equal: return D3D12_COMPARISON_FUNC_EQUAL;
					case reshadefx::pass_stencil_func::not_equal: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
					case reshadefx::pass_stencil_func::less: return D3D12_COMPARISON_FUNC_LESS;
					case reshadefx::pass_stencil_func::less_equal: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
					case reshadefx::pass_stencil_func::greater: return D3D12_COMPARISON_FUNC_GREATER;
					case reshadefx::pass_stencil_func::greater_equal: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
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
				LOG(ERROR) << "Failed to create pipeline for pass " << pass_index << " in technique '" << technique.name << "'! "
					"HRESULT is " << hr << '.';
				return false;
			}
		}
	}

	return true;
}
void reshade::d3d12::runtime_d3d12::unload_effect(size_t index)
{
	// Make sure no effect resources are currently in use
	wait_for_command_queue();

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != index)
			continue;

		delete static_cast<d3d12_technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effect(index);

	if (index < _effect_data.size())
	{
		d3d12_effect_data &effect_data = _effect_data[index];
		effect_data.cb.reset();
		effect_data.signature.reset();
		effect_data.srv_heap.reset();
		effect_data.rtv_heap.reset();
		effect_data.sampler_heap.reset();
		effect_data.depth_texture_binding = { 0 };
	}
}
void reshade::d3d12::runtime_d3d12::unload_effects()
{
	// Make sure no effect resources are currently in use
	wait_for_command_queue();

	for (technique &tech : _techniques)
	{
		delete static_cast<d3d12_technique_data *>(tech.impl);
		tech.impl = nullptr;
	}

	runtime::unload_effects();

	_effect_data.clear();
}

bool reshade::d3d12::runtime_d3d12::init_texture(texture &texture)
{
	auto impl = new d3d12_tex_data();
	texture.impl = impl;

	// Do not create resource if it is a reference, it is set in 'render_technique'
	if (texture.impl_reference != texture_reference::none)
		return true;

	D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D };
	desc.Width = texture.width;
	desc.Height = texture.height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = static_cast<UINT16>(texture.levels);
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc = { 1, 0 };
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // Textures may be bound as render target

	if (texture.levels > 1) // Need UAV for mipmap generation
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

	if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_SHADER_RESOURCE, &clear_value, IID_PPV_ARGS(&impl->resource)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "' ("
			"Width = " << desc.Width << ", "
			"Height = " << desc.Height << ", "
			"Levels = " << desc.MipLevels << ", "
			"Format = " << desc.Format << ")! "
			"HRESULT is " << hr << '.';
		return false;
	}

#ifndef NDEBUG
	std::wstring debug_name;
	debug_name.reserve(texture.unique_name.size());
	utf8::unchecked::utf8to16(texture.unique_name.begin(), texture.unique_name.end(), std::back_inserter(debug_name));
	impl->resource->SetName(debug_name.c_str());
#endif

	{	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		heap_desc.NumDescriptors = texture.levels /* SRV */ + texture.levels - 1 /* UAV */;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if (FAILED(_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&impl->descriptors))))
			return false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_handle = impl->descriptors->GetCPUDescriptorHandleForHeapStart();

	for (uint32_t level = 0; level < texture.levels; ++level, srv_cpu_handle.ptr += _srv_handle_size)
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
	for (uint32_t level = 1; level < texture.levels; ++level, srv_cpu_handle.ptr += _srv_handle_size)
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
	auto impl = static_cast<d3d12_tex_data *>(texture.impl);
	assert(impl != nullptr && pixels != nullptr && texture.impl_reference == texture_reference::none);

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
	if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate))))
	{
		LOG(ERROR) << "Failed to create system memory texture for texture updating!";
		return;
	}

#ifndef NDEBUG
	intermediate->SetName(L"ReShade upload texture");
#endif

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
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<unsigned int>(texture.format) << '!';
		break;
	}

	intermediate->Unmap(0, nullptr);

	if (unsupported_format || !begin_command_list())
		return;

	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST, 0);
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

		_cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}
	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_SHADER_RESOURCE, 0);

	generate_mipmaps(texture);

	// Execute and wait for completion
	wait_for_command_queue();
}
void reshade::d3d12::runtime_d3d12::destroy_texture(texture &texture)
{
	delete static_cast<d3d12_tex_data *>(texture.impl);
	texture.impl = nullptr;
}
void reshade::d3d12::runtime_d3d12::generate_mipmaps(const texture &texture)
{
	if (texture.levels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	auto impl = static_cast<d3d12_tex_data *>(texture.impl);
	assert(impl != nullptr);

	_cmd_list->SetComputeRootSignature(_mipmap_signature.get());
	_cmd_list->SetPipelineState(_mipmap_pipeline.get());
	ID3D12DescriptorHeap *const descriptor_heap = impl->descriptors.get();
	_cmd_list->SetDescriptorHeaps(1, &descriptor_heap);

	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	for (uint32_t level = 1; level < texture.levels; ++level)
	{
		const uint32_t width = std::max(1u, texture.width >> level);
		const uint32_t height = std::max(1u, texture.height >> level);

		static const auto float_as_uint = [](float value) { return *reinterpret_cast<uint32_t *>(&value); };
		_cmd_list->SetComputeRoot32BitConstant(0, float_as_uint(1.0f / width), 0);
		_cmd_list->SetComputeRoot32BitConstant(0, float_as_uint(1.0f / height), 1);
		// Bind next higher mipmap level as input
		_cmd_list->SetComputeRootDescriptorTable(1, { impl->descriptors->GetGPUDescriptorHandleForHeapStart().ptr + _srv_handle_size * (level - 1) });
		// There is no UAV for level 0, so substract one
		_cmd_list->SetComputeRootDescriptorTable(2, { impl->descriptors->GetGPUDescriptorHandleForHeapStart().ptr + _srv_handle_size * (texture.levels + level - 1) });

		_cmd_list->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
		barrier.UAV.pResource = impl->resource.get();
		_cmd_list->ResourceBarrier(1, &barrier);
	}
	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_SHADER_RESOURCE);
}

void reshade::d3d12::runtime_d3d12::render_technique(technique &technique)
{
	const auto impl = static_cast<d3d12_technique_data *>(technique.impl);
	d3d12_effect_data &effect_data = _effect_data[technique.effect_index];

	if (!begin_command_list())
		return;

	ID3D12DescriptorHeap *const descriptor_heaps[] = { effect_data.srv_heap.get(), effect_data.sampler_heap.get() };
	_cmd_list->SetDescriptorHeaps(ARRAYSIZE(descriptor_heaps), descriptor_heaps);
	_cmd_list->SetGraphicsRootSignature(effect_data.signature.get());

	// Setup vertex input
	_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Setup shader constants
	if (effect_data.cb != nullptr)
	{
		if (void *mapped; SUCCEEDED(effect_data.cb->Map(0, nullptr, &mapped)))
		{
			std::memcpy(mapped, _effects[technique.effect_index].uniform_data_storage.data(), _effects[technique.effect_index].uniform_data_storage.size());
			effect_data.cb->Unmap(0, nullptr);
		}

		_cmd_list->SetGraphicsRootConstantBufferView(0, effect_data.cbv_gpu_address);
	}

	// Setup shader resources
	_cmd_list->SetGraphicsRootDescriptorTable(1, effect_data.srv_gpu_base);

	// Setup samplers
	_cmd_list->SetGraphicsRootDescriptorTable(2, effect_data.sampler_gpu_base);

	// TODO: Technically need to transition the depth texture here as well

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_backbuffer_copy = true; // First pass always needs the back buffer updated
	D3D12_CPU_DESCRIPTOR_HANDLE effect_stencil = _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart();

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		if (needs_implicit_backbuffer_copy)
		{
			// Save back buffer of previous pass
			transition_state(_cmd_list, _backbuffer_texture, D3D12_RESOURCE_STATE_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
			_cmd_list->CopyResource(_backbuffer_texture.get(), _backbuffers[_swap_index].get());
			transition_state(_cmd_list, _backbuffer_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_SHADER_RESOURCE);
			transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		const d3d12_pass_data &pass_data = impl->passes[pass_index];
		const reshadefx::pass_info &pass_info = technique.passes[pass_index];

		// Transition resource state for render targets
		for (UINT k = 0; k < pass_data.num_render_targets; ++k)
		{
			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			transition_state(_cmd_list, static_cast<d3d12_tex_data *>(render_target_texture->impl)->resource, D3D12_RESOURCE_STATE_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		// Setup states
		_cmd_list->SetPipelineState(pass_data.pipeline.get());
		_cmd_list->OMSetStencilRef(pass_info.stencil_reference_value);

		// Setup render targets
		const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		if (pass_info.stencil_enable && !is_effect_stencil_cleared)
		{
			is_effect_stencil_cleared = true;

			_cmd_list->ClearDepthStencilView(effect_stencil, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		}

		if (pass_data.num_render_targets == 0)
		{
			needs_implicit_backbuffer_copy = true;

			D3D12_CPU_DESCRIPTOR_HANDLE render_target = { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + (_swap_index * 2 + pass_info.srgb_write_enable) * _rtv_handle_size };
			_cmd_list->OMSetRenderTargets(1, &render_target, false, pass_info.stencil_enable ? &effect_stencil : nullptr);

			if (pass_info.clear_render_targets)
				_cmd_list->ClearRenderTargetView(render_target, clear_color, 0, nullptr);
		}
		else
		{
			needs_implicit_backbuffer_copy = false;

			_cmd_list->OMSetRenderTargets(pass_data.num_render_targets, &pass_data.render_targets, true,
				pass_info.stencil_enable && pass_info.viewport_width == _width && pass_info.viewport_height == _height ? &effect_stencil : nullptr);

			if (pass_info.clear_render_targets)
				for (UINT k = 0; k < pass_data.num_render_targets; ++k)
					_cmd_list->ClearRenderTargetView({ pass_data.render_targets.ptr + k * _rtv_handle_size }, clear_color, 0, nullptr);
		}

		const D3D12_RECT scissor_rect = { 0, 0, static_cast<LONG>(pass_info.viewport_width), static_cast<LONG>(pass_info.viewport_height) };
		const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<FLOAT>(pass_info.viewport_width), static_cast<FLOAT>(pass_info.viewport_height), 0.0f, 1.0f };
		_cmd_list->RSSetViewports(1, &viewport);
		_cmd_list->RSSetScissorRects(1, &scissor_rect);

		// Draw triangle
		_cmd_list->DrawInstanced(pass_info.num_vertices, 1, 0, 0);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		// Generate mipmaps and transition resource state back to shader access
		for (UINT k = 0; k < pass_data.num_render_targets; ++k)
		{
			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			transition_state(_cmd_list, static_cast<d3d12_tex_data *>(render_target_texture->impl)->resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_SHADER_RESOURCE);
			generate_mipmaps(*render_target_texture);
		}
	}
}

bool reshade::d3d12::runtime_d3d12::begin_command_list(const com_ptr<ID3D12PipelineState> &state) const
{
	if (_cmd_list_is_recording)
	{
		if (state != nullptr) // Update pipeline state if requested
			_cmd_list->SetPipelineState(state.get());
		return true;
	}

	// Reset command list using current command allocator and put it into the recording state
	_cmd_list_is_recording = SUCCEEDED(_cmd_list->Reset(_cmd_alloc[_swap_index].get(), state.get()));
	return _cmd_list_is_recording;
}
void reshade::d3d12::runtime_d3d12::execute_command_list() const
{
	assert(_cmd_list_is_recording);
	_cmd_list_is_recording = false;

	if (FAILED(_cmd_list->Close()))
		return;

	ID3D12CommandList *const cmd_lists[] = { _cmd_list.get() };
	_commandqueue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);
}
bool reshade::d3d12::runtime_d3d12::wait_for_command_queue() const
{
	// Flush command list, to avoid it still referencing resources that may be destroyed after this call
	if (_cmd_list_is_recording)
		execute_command_list();

	// Increment fence value to ensure it has not been signaled before
	const UINT64 sync_value = _fence_value[_swap_index] + 1;
	if (FAILED(_commandqueue->Signal(_fence[_swap_index].get(), sync_value)))
		return false; // Cannot wait on fence if signaling was not successful
	if (SUCCEEDED(_fence[_swap_index]->SetEventOnCompletion(sync_value, _fence_event)))
		WaitForSingleObject(_fence_event, INFINITE);
	// Update CPU side fence value now that it is guaranteed to have come through
	_fence_value[_swap_index] = sync_value;
	return true;
}

com_ptr<ID3D12RootSignature> reshade::d3d12::runtime_d3d12::create_root_signature(const D3D12_ROOT_SIGNATURE_DESC &desc) const
{
	com_ptr<ID3D12RootSignature> signature;
	if (com_ptr<ID3DBlob> blob; SUCCEEDED(hooks::call(D3D12SerializeRootSignature)(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)))
		_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&signature));
	return signature;
}

#if RESHADE_GUI
bool reshade::d3d12::runtime_d3d12::init_imgui_resources()
{
	{   D3D12_DESCRIPTOR_RANGE srv_range = {};
		srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_range.NumDescriptors = 1;
		srv_range.BaseShaderRegister = 0; // t0

		D3D12_ROOT_PARAMETER params[2] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0; // b0
		params[0].Constants.Num32BitValues = 16;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &srv_range;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC samplers[1] = {};
		samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplers[0].ShaderRegister = 0; // s0
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = ARRAYSIZE(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = ARRAYSIZE(samplers);
		desc.pStaticSamplers = samplers;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		_imgui.signature = create_root_signature(desc);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = _imgui.signature.get();
	pso_desc.SampleMask = UINT_MAX;
	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = _backbuffer_format;
	pso_desc.SampleDesc = { 1, 0 };
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso_desc.NodeMask = 1;

	{   const resources::data_resource vs = resources::load_data_resource(IDR_IMGUI_VS);
		pso_desc.VS = { vs.data, vs.data_size };

		static const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(ImDrawVert, uv ), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		pso_desc.InputLayout = { input_layout, ARRAYSIZE(input_layout) };
	}
	{   const resources::data_resource ps = resources::load_data_resource(IDR_IMGUI_PS);
		pso_desc.PS = { ps.data, ps.data_size };
	}

	{   D3D12_BLEND_DESC &desc = pso_desc.BlendState;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	{   D3D12_RASTERIZER_DESC &desc = pso_desc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.DepthClipEnable = true;
	}

	{   D3D12_DEPTH_STENCIL_DESC &desc = pso_desc.DepthStencilState;
		desc.DepthEnable = false;
		desc.StencilEnable = false;
	}

	return SUCCEEDED(_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&_imgui.pipeline)));
}

void reshade::d3d12::runtime_d3d12::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Need to multi-buffer vertex data so not to modify data below when the previous frame is still in flight
	const unsigned int buffer_index = _framecount % NUM_IMGUI_BUFFERS;

	// Create and grow vertex/index buffers if needed
	if (_imgui.num_indices[buffer_index] < draw_data->TotalIdxCount)
	{
		wait_for_command_queue(); // Be safe and ensure nothing still uses this buffer

		_imgui.indices[buffer_index].reset();

		const int new_size = draw_data->TotalIdxCount + 10000;
		D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
		desc.Width = new_size * sizeof(ImDrawIdx);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&_imgui.indices[buffer_index]))))
			return;
#ifndef NDEBUG
		_imgui.indices[buffer_index]->SetName(L"ImGui index buffer");
#endif
		_imgui.num_indices[buffer_index] = new_size;
	}
	if (_imgui.num_vertices[buffer_index] < draw_data->TotalVtxCount)
	{
		wait_for_command_queue();

		_imgui.vertices[buffer_index].reset();

		const int new_size = draw_data->TotalVtxCount + 5000;
		D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
		desc.Width = new_size * sizeof(ImDrawVert);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&_imgui.vertices[buffer_index]))))
			return;
#ifndef NDEBUG
		_imgui.vertices[buffer_index]->SetName(L"ImGui vertex buffer");
#endif
		_imgui.num_vertices[buffer_index] = new_size;
	}

	if (ImDrawIdx *idx_dst;
		SUCCEEDED(_imgui.indices[buffer_index]->Map(0, nullptr, reinterpret_cast<void **>(&idx_dst))))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			idx_dst += draw_list->IdxBuffer.Size;
		}

		_imgui.indices[buffer_index]->Unmap(0, nullptr);
	}
	if (ImDrawVert *vtx_dst;
		SUCCEEDED(_imgui.vertices[buffer_index]->Map(0, nullptr, reinterpret_cast<void **>(&vtx_dst))))
	{
		for (int n = 0; n < draw_data->CmdListsCount; ++n)
		{
			const ImDrawList *const draw_list = draw_data->CmdLists[n];
			std::memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			vtx_dst += draw_list->VtxBuffer.Size;
		}

		_imgui.vertices[buffer_index]->Unmap(0, nullptr);
	}

	if (!begin_command_list(_imgui.pipeline))
		return;

	// Setup orthographic projection matrix
	const float ortho_projection[16] = {
		2.0f / draw_data->DisplaySize.x, 0.0f,  0.0f, 0.0f,
		0.0f, -2.0f / draw_data->DisplaySize.y, 0.0f, 0.0f,
		0.0f,                            0.0f,  0.5f, 0.0f,
		-(2 * draw_data->DisplayPos.x + draw_data->DisplaySize.x) / draw_data->DisplaySize.x,
		+(2 * draw_data->DisplayPos.y + draw_data->DisplaySize.y) / draw_data->DisplaySize.y, 0.5f, 1.0f,
	};

	// Setup render state and render draw lists
	const D3D12_INDEX_BUFFER_VIEW index_buffer_view = {
		_imgui.indices[buffer_index]->GetGPUVirtualAddress(), _imgui.num_indices[buffer_index] * sizeof(ImDrawIdx), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
	_cmd_list->IASetIndexBuffer(&index_buffer_view);
	const D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {
		_imgui.vertices[buffer_index]->GetGPUVirtualAddress(), _imgui.num_vertices[buffer_index] * sizeof(ImDrawVert),  sizeof(ImDrawVert) };
	_cmd_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmd_list->SetGraphicsRootSignature(_imgui.signature.get());
	_cmd_list->SetGraphicsRoot32BitConstants(0, sizeof(ortho_projection) / 4, ortho_projection, 0);
	const D3D12_VIEWPORT viewport = { 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y, 0.0f, 1.0f };
	_cmd_list->RSSetViewports(1, &viewport);
	const FLOAT blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
	_cmd_list->OMSetBlendFactor(blend_factor);
	D3D12_CPU_DESCRIPTOR_HANDLE render_target = { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + _swap_index * 2 * _rtv_handle_size };
	_cmd_list->OMSetRenderTargets(1, &render_target, false, nullptr);

	UINT vtx_offset = 0, idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; ++n)
	{
		const ImDrawList *const draw_list = draw_data->CmdLists[n];

		for (const ImDrawCmd &cmd : draw_list->CmdBuffer)
		{
			assert(cmd.TextureId != 0);
			assert(cmd.UserCallback == nullptr);

			const D3D12_RECT scissor_rect = {
				static_cast<LONG>(cmd.ClipRect.x - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.y - draw_data->DisplayPos.y),
				static_cast<LONG>(cmd.ClipRect.z - draw_data->DisplayPos.x),
				static_cast<LONG>(cmd.ClipRect.w - draw_data->DisplayPos.y)
			};
			_cmd_list->RSSetScissorRects(1, &scissor_rect);

			// First descriptor in resource-specific descriptor heap is SRV to top-most mipmap level
			// Can assume that the resource state is D3D12_RESOURCE_STATE_SHADER_RESOURCE at this point
			ID3D12DescriptorHeap *const descriptor_heap = { static_cast<d3d12_tex_data *>(cmd.TextureId)->descriptors.get() };
			_cmd_list->SetDescriptorHeaps(1, &descriptor_heap);
			_cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_heap->GetGPUDescriptorHandleForHeapStart());

			_cmd_list->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}
}
#endif

#if RESHADE_DEPTH
void reshade::d3d12::runtime_d3d12::draw_depth_debug_menu(buffer_detection_context &tracker)
{
	if (!ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (_has_high_network_activity)
	{
		ImGui::TextColored(ImColor(204, 204, 0), "High network activity discovered.\nAccess to depth buffers is disabled to prevent exploitation.");
		return;
	}

	bool modified = false;
	modified |= ImGui::Checkbox("Use aspect ratio heuristics", &_filter_aspect_ratio);
	modified |= ImGui::Checkbox("Copy depth buffers before clear operation", &_preserve_depth_buffers);

	if (modified) // Detection settings have changed, reset heuristic
		// Do not release resources here, as they may still be in use on the device
		tracker.reset(false);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	for (const auto &[dsv_texture, snapshot] : tracker.depth_buffer_counters())
	{
		// TODO: Display current resource when not preserving depth buffers
		char label[512] = "";
		sprintf_s(label, "%s0x%p", (dsv_texture == tracker.current_depth_texture() ? "> " : "  "), dsv_texture.get());

		const D3D12_RESOURCE_DESC desc = dsv_texture->GetDesc();

		const bool msaa = desc.SampleDesc.Count > 1;
		if (msaa) // Disable widget for MSAA textures
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
		}

		if (bool value = (_depth_texture_override == dsv_texture);
			ImGui::Checkbox(label, &value))
			_depth_texture_override = value ? dsv_texture.get() : nullptr;

		ImGui::SameLine();
		ImGui::Text("| %4ux%-4u | %5u draw calls ==> %8u vertices |%s",
			desc.Width, desc.Height, snapshot.total_stats.drawcalls, snapshot.total_stats.vertices, (msaa ? " MSAA" : ""));

		if (_preserve_depth_buffers && dsv_texture == tracker.current_depth_texture())
		{
			for (UINT clear_index = 1; clear_index <= snapshot.clears.size(); ++clear_index)
			{
				sprintf_s(label, "%s  CLEAR %2u", (clear_index == tracker.current_clear_index() ? "> " : "  "), clear_index);

				if (bool value = (_depth_clear_index_override == clear_index);
					ImGui::Checkbox(label, &value))
				{
					_depth_clear_index_override = value ? clear_index : std::numeric_limits<UINT>::max();
					modified = true;
				}

				ImGui::SameLine();
				ImGui::Text("%*s|           | %5u draw calls ==> %8u vertices |",
					sizeof(dsv_texture.get()) == 8 ? 8 : 0, "", // Add space to fill pointer length
					snapshot.clears[clear_index - 1].drawcalls, snapshot.clears[clear_index - 1].vertices);
			}
		}

		if (msaa)
		{
			ImGui::PopStyleColor();
			ImGui::PopItemFlag();
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (modified)
		runtime::save_config();
}

void reshade::d3d12::runtime_d3d12::update_depth_texture_bindings(com_ptr<ID3D12Resource> depth_texture)
{
	if (_has_high_network_activity)
		depth_texture.reset();

	if (depth_texture == _depth_texture)
		return;

	_depth_texture = std::move(depth_texture);

	D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = {};

	if (_depth_texture != nullptr)
	{
		const D3D12_RESOURCE_DESC desc = _depth_texture->GetDesc();

		view_desc.Format = make_dxgi_format_normal(desc.Format);
		view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view_desc.Texture2D.MipLevels = desc.MipLevels;

		_has_depth_texture = true;
	}
	else
	{
		// Need to provide a description so descriptor type can be determined
		// See https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview
		view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		view_desc.Texture2D.MipLevels = 1;
		view_desc.Texture2D.MostDetailedMip = 0;
		view_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		_has_depth_texture = false;
	}

	// Descriptors may be currently in use, so make sure all previous frames have finished before updating them
	wait_for_command_queue();

	for (d3d12_effect_data &effect_data : _effect_data)
	{
		if (effect_data.depth_texture_binding.ptr == 0)
			continue; // Skip effects that do not have a depth buffer binding

		// Either create a shader resource view or a null descriptor
		_device->CreateShaderResourceView(_depth_texture.get(), &view_desc, effect_data.depth_texture_binding);
	}
}
#endif
