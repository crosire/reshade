/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "hook_manager.hpp"
#include "runtime_d3d12.hpp"
#include "runtime_config.hpp"
#include "runtime_objects.hpp"
#include "../dxgi/format_utils.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <d3dcompiler.h>

namespace reshade::d3d12
{
	struct d3d12_tex_data : base_object
	{
		D3D12_RESOURCE_STATES state;
		com_ptr<ID3D12Resource> resource;
		com_ptr<ID3D12DescriptorHeap> descriptors;
	};
	struct d3d12_pass_data : base_object
	{
		com_ptr<ID3D12PipelineState> pipeline;
		D3D12_VIEWPORT viewport;
		UINT num_render_targets;
		D3D12_CPU_DESCRIPTOR_HANDLE render_targets;
	};
	struct d3d12_technique_data : base_object
	{
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

	static uint32_t float_as_uint(float value)
	{
		return *reinterpret_cast<uint32_t *>(&value);
	}

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

		if (com_ptr<IDXGIAdapter> adapter;
			factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);
			_vendor_id = desc.VendorId;
			_device_id = desc.DeviceId;
		}
	}

#if RESHADE_GUI && RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	subscribe_to_ui("DX12", [this]() { draw_depth_debug_menu(); });
#endif
#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
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

	// Create fences for synchronization
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
#ifdef _DEBUG
		_backbuffers[i]->SetName(L"Backbuffer");
#endif

		for (unsigned int srgb_write_enable = 0; srgb_write_enable < 2; ++srgb_write_enable, rtv_handle.ptr += _rtv_handle_size)
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

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&_backbuffer_texture))))
			return false;
#ifdef _DEBUG
		_backbuffer_texture->SetName(L"ReShade Backbuffer Texture");
#endif
	}

	// Create effect depth stencil resource
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

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear_value, IID_PPV_ARGS(&_effect_depthstencil))))
			return false;
#ifdef _DEBUG
		_effect_depthstencil->SetName(L"ReShade Default Depth-Stencil");
#endif
		_device->CreateDepthStencilView(_effect_depthstencil.get(), nullptr, _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart());
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

	_cmd_list.reset();
	_cmd_alloc.clear();

	CloseHandle(_fence_event);
	_fence.clear();
	_fence_value.clear();

	_backbuffers.clear();
	_backbuffer_rtvs.reset();
	_backbuffer_texture.reset();
	_depth_texture.reset();
	_depthstencil_dsvs.reset();

	_mipmap_pipeline.reset();
	_mipmap_signature.reset();

	_effect_depthstencil.reset();

#if RESHADE_GUI
	for (unsigned int i = 0; i < NUM_IMGUI_BUFFERS; ++i)
	{
		_imgui_index_buffer[i].reset();
		_imgui_index_buffer_size[i] = 0;
		_imgui_vertex_buffer[i].reset();
		_imgui_vertex_buffer_size[i] = 0;
	}

	_imgui_pipeline.reset();
	_imgui_signature.reset();
#endif

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	_has_depth_texture = false;
	_depth_texture_override = nullptr;
#endif
}

void reshade::d3d12::runtime_d3d12::on_present(buffer_detection_context &tracker)
{
	if (!_is_initialized)
		return;

	_vertices = tracker.total_vertices();
	_drawcalls = tracker.total_drawcalls();

	// There is no swap chain for d3d12on7
	if (_swapchain != nullptr)
		_swap_index = _swapchain->GetCurrentBackBufferIndex();

	// Make sure all commands for this command allocator have finished executing before reseting it
	if (_fence[_swap_index]->GetCompletedValue() < _fence_value[_swap_index])
	{
		_fence[_swap_index]->SetEventOnCompletion(_fence_value[_swap_index], _fence_event);
		WaitForSingleObject(_fence_event, INFINITE);
	}

	// Reset command allocator before using it this frame again
	_cmd_alloc[_swap_index]->Reset();

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	_current_tracker = &tracker;
	assert(_depth_clear_index_override != 0);
	update_depthstencil_texture(_has_high_network_activity ? nullptr :
		tracker.find_best_depth_texture(_commandqueue.get(), _filter_aspect_ratio ? _width : 0, _height, _depth_texture_override, _preserve_depth_buffers ? _depth_clear_index_override : 0));
#endif

	update_and_render_effects();

	runtime::on_present();

	_commandqueue->Signal(_fence[_swap_index].get(), ++_fence_value[_swap_index]);
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

#ifdef _DEBUG
	intermediate->SetName(L"ReShade screenshot texture");
#endif

	if (!begin_command_list())
		return false;

	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
	{
		D3D12_TEXTURE_COPY_LOCATION src_location = { _backbuffers[_swap_index].get() };
		src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src_location.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dst_location = { intermediate.get() };
		dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst_location.PlacedFootprint.Footprint.Width = _width;
		dst_location.PlacedFootprint.Footprint.Height = _height;
		dst_location.PlacedFootprint.Footprint.Depth = 1;
		dst_location.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dst_location.PlacedFootprint.Footprint.RowPitch = download_pitch;

		_cmd_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);
	}
	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT, 0);

	// Execute and wait for completion
	execute_command_list();
	wait_for_command_queue();

	// Copy data from system memory texture into output buffer
	uint8_t *mapped_data;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return false;

	for (uint32_t y = 0; y < _height; y++, buffer += data_pitch, mapped_data += download_pitch)
	{
		if (_backbuffer_format == DXGI_FORMAT_R10G10B10A2_UNORM ||
			_backbuffer_format == DXGI_FORMAT_R10G10B10A2_UINT)
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
	for (const auto &entry_point : effect.module.entry_points)
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
#ifdef _DEBUG
		effect_data.cb->SetName(L"ReShade Global CB");
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
		for (auto &info : effect.module.techniques)
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

		const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
			[&texture_name = info.texture_name](const auto &item) {
			return item.unique_name == texture_name && item.impl != nullptr;
		});
		if (existing_texture == _textures.end())
			return false;

		com_ptr<ID3D12Resource> resource;
		switch (existing_texture->impl_reference)
		{
		case texture_reference::back_buffer:
			resource = _backbuffer_texture;
			break;
		case texture_reference::depth_buffer:
			resource = _depth_texture;
			break;
		default:
			resource = existing_texture->impl->as<d3d12_tex_data>()->resource;
			break;
		}

		if (resource == nullptr)
			continue;

		{   D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format = info.srgb ?
				make_dxgi_format_srgb(resource->GetDesc().Format) :
				make_dxgi_format_normal(resource->GetDesc().Format);
			desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			desc.Texture2D.MipLevels = existing_texture->levels;

			D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = effect_data.srv_cpu_base;
			srv_handle.ptr += info.texture_binding * _srv_handle_size;

			_device->CreateShaderResourceView(resource.get(), &desc, srv_handle);

			// Keep track of the depth buffer texture descriptor to simplify updating it
			if (existing_texture->impl_reference == texture_reference::depth_buffer)
				effect_data.depth_texture_binding = srv_handle;
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

	bool success = true;

	for (technique &technique : _techniques)
	{
		if (technique.impl != nullptr || technique.effect_index != index)
			continue;

		technique.impl = std::make_unique<d3d12_technique_data>();

		for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
		{
			technique.passes_data.push_back(std::make_unique<d3d12_pass_data>());

			auto &pass_data = *technique.passes_data.back()->as<d3d12_pass_data>();
			const auto &pass_info = technique.passes[pass_index];

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = effect_data.signature.get();

			const auto &VS = entry_points.at(pass_info.vs_entry_point);
			pso_desc.VS = { VS->GetBufferPointer(), VS->GetBufferSize() };
			const auto &PS = entry_points.at(pass_info.ps_entry_point);
			pso_desc.PS = { PS->GetBufferPointer(), PS->GetBufferSize() };

			pass_data.viewport.Width = pass_info.viewport_width ? FLOAT(pass_info.viewport_width) : FLOAT(frame_width());
			pass_data.viewport.Height = pass_info.viewport_height ? FLOAT(pass_info.viewport_height) : FLOAT(frame_height());
			pass_data.viewport.MaxDepth = 1.0f;

			D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = effect_data.rtv_cpu_base;
			rtv_handle.ptr += pass_index * 8 * _rtv_handle_size;
			pass_data.render_targets = rtv_handle;

			pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

			for (unsigned int k = 0; k < 8; k++)
			{
				if (pass_info.render_target_names[k].empty())
					continue; // Skip unbound render targets

				const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
					[&render_target = pass_info.render_target_names[k]](const auto &item) {
					return item.unique_name == render_target;
				});
				if (render_target_texture == _textures.end())
					return assert(false), false;

				const auto texture_impl = render_target_texture->impl->as<d3d12_tex_data>();
				assert(texture_impl != nullptr);

				rtv_handle.ptr += k * _rtv_handle_size;

				D3D12_RENDER_TARGET_VIEW_DESC desc = {};
				desc.Format = pass_info.srgb_write_enable ?
					make_dxgi_format_srgb(texture_impl->resource->GetDesc().Format) :
					make_dxgi_format_normal(texture_impl->resource->GetDesc().Format);
				desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

				_device->CreateRenderTargetView(texture_impl->resource.get(), &desc, rtv_handle);

				pso_desc.RTVFormats[k] = desc.Format;
				pso_desc.NumRenderTargets = k + 1;
			}

			pass_data.num_render_targets = pso_desc.NumRenderTargets;

			if (pso_desc.NumRenderTargets == 0)
			{
				pso_desc.NumRenderTargets = 1;
				pso_desc.RTVFormats[0] = pass_info.srgb_write_enable ?
					make_dxgi_format_srgb(_backbuffer_format) :
					make_dxgi_format_normal(_backbuffer_format);
			}

			pso_desc.SampleMask = UINT_MAX;
			pso_desc.SampleDesc = { 1, 0 };
			pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pso_desc.NodeMask = 1;

			{   D3D12_BLEND_DESC &desc = pso_desc.BlendState;
				desc.RenderTarget[0].BlendEnable = pass_info.blend_enable;

				const auto literal_to_blend_func = [](unsigned int value) {
					switch (value) {
					default:
					case 1: return D3D12_BLEND_ONE;
					case 0: return D3D12_BLEND_ZERO;
					case 2: return D3D12_BLEND_SRC_COLOR;
					case 4: return D3D12_BLEND_INV_SRC_COLOR;
					case 3: return D3D12_BLEND_SRC_ALPHA;
					case 5: return D3D12_BLEND_INV_SRC_ALPHA;
					case 6: return D3D12_BLEND_DEST_ALPHA;
					case 7: return D3D12_BLEND_INV_DEST_ALPHA;
					case 8: return D3D12_BLEND_DEST_COLOR;
					case 9: return D3D12_BLEND_INV_DEST_COLOR;
					}
				};

				desc.RenderTarget[0].SrcBlend = literal_to_blend_func(pass_info.src_blend);
				desc.RenderTarget[0].DestBlend = literal_to_blend_func(pass_info.dest_blend);
				desc.RenderTarget[0].BlendOp = static_cast<D3D12_BLEND_OP>(pass_info.blend_op);
				desc.RenderTarget[0].SrcBlendAlpha = literal_to_blend_func(pass_info.src_blend_alpha);
				desc.RenderTarget[0].DestBlendAlpha = literal_to_blend_func(pass_info.dest_blend_alpha);
				desc.RenderTarget[0].BlendOpAlpha = static_cast<D3D12_BLEND_OP>(pass_info.blend_op_alpha);
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

				const auto literal_to_stencil_op = [](unsigned int value) {
					switch (value) {
					default:
					case 1: return D3D12_STENCIL_OP_KEEP;
					case 0: return D3D12_STENCIL_OP_ZERO;
					case 3: return D3D12_STENCIL_OP_REPLACE;
					case 4: return D3D12_STENCIL_OP_INCR_SAT;
					case 5: return D3D12_STENCIL_OP_DECR_SAT;
					case 6: return D3D12_STENCIL_OP_INVERT;
					case 7: return D3D12_STENCIL_OP_INCR;
					case 8: return D3D12_STENCIL_OP_DECR;
					}
				};

				desc.StencilEnable = pass_info.stencil_enable;
				desc.StencilReadMask = pass_info.stencil_read_mask;
				desc.StencilWriteMask = pass_info.stencil_write_mask;
				desc.FrontFace.StencilFailOp = literal_to_stencil_op(pass_info.stencil_op_fail);
				desc.FrontFace.StencilDepthFailOp = literal_to_stencil_op(pass_info.stencil_op_depth_fail);
				desc.FrontFace.StencilPassOp = literal_to_stencil_op(pass_info.stencil_op_pass);
				desc.FrontFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(pass_info.stencil_comparison_func);
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

	return success;
}
void reshade::d3d12::runtime_d3d12::unload_effect(size_t index)
{
	// Wait for all GPU operations to finish so resources are no longer referenced
	wait_for_command_queue();

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
	if (!_is_initialized)
		return;

	// Wait for all GPU operations to finish so resources are no longer referenced
	wait_for_command_queue();

	runtime::unload_effects();

	_effect_data.clear();
}

bool reshade::d3d12::runtime_d3d12::init_texture(texture &texture)
{
	texture.impl = std::make_unique<d3d12_tex_data>();
	const auto impl = texture.impl->as<d3d12_tex_data>();

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

	D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_DEFAULT };

	// Render targets are always either cleared to zero or not cleared at all (see 'ClearRenderTargets' pass state), so can set the optimized clear value here to zero
	D3D12_CLEAR_VALUE clear_value = {};
	clear_value.Format = make_dxgi_format_normal(desc.Format);

	// Initialize resource to the pixel shader state immediately, so no additional transition is required
	impl->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	if (HRESULT hr = _device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, impl->state, &clear_value, IID_PPV_ARGS(&impl->resource)); FAILED(hr))
	{
		LOG(ERROR) << "Failed to create texture '" << texture.unique_name << "' ("
			"Width = " << desc.Width << ", "
			"Height = " << desc.Height << ", "
			"Format = " << desc.Format << ")! "
			"HRESULT is " << hr << '.';
		return false;
	}

#ifdef _DEBUG
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
void reshade::d3d12::runtime_d3d12::upload_texture(texture &texture, const uint8_t *pixels)
{
	const auto impl = texture.impl->as<d3d12_tex_data>();
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

#ifdef _DEBUG
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

	transition_state(_cmd_list, impl->resource, impl->state, D3D12_RESOURCE_STATE_COPY_DEST, 0);
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
	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_COPY_DEST, impl->state, 0);

	generate_mipmaps(texture);

	// Execute and wait for completion
	execute_command_list();
	wait_for_command_queue();
}
void reshade::d3d12::runtime_d3d12::generate_mipmaps(texture &texture)
{
	if (texture.levels <= 1)
		return; // No need to generate mipmaps when texture does not have any

	const auto impl = texture.impl->as<d3d12_tex_data>();
	assert(impl != nullptr);

	_cmd_list->SetComputeRootSignature(_mipmap_signature.get());
	_cmd_list->SetPipelineState(_mipmap_pipeline.get());
	ID3D12DescriptorHeap *const descriptor_heap = impl->descriptors.get();
	_cmd_list->SetDescriptorHeaps(1, &descriptor_heap);

	transition_state(_cmd_list, impl->resource, impl->state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	for (uint32_t level = 1; level < texture.levels; ++level)
	{
		const uint32_t width = std::max(1u, texture.width >> level);
		const uint32_t height = std::max(1u, texture.height >> level);

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
	transition_state(_cmd_list, impl->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, impl->state);
}

void reshade::d3d12::runtime_d3d12::render_technique(technique &technique)
{
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
		void *mapped;
		if (HRESULT hr = effect_data.cb->Map(0, nullptr, &mapped); SUCCEEDED(hr))
		{
			std::memcpy(mapped, _effects[technique.effect_index].uniform_data_storage.data(), _effects[technique.effect_index].uniform_data_storage.size());
			effect_data.cb->Unmap(0, nullptr);
		}
		else
		{
			LOG(ERROR) << "Failed to map constant buffer! HRESULT is " << hr << '.';
		}

		_cmd_list->SetGraphicsRootConstantBufferView(0, effect_data.cbv_gpu_address);
	}

	// Setup shader resources
	_cmd_list->SetGraphicsRootDescriptorTable(1, effect_data.srv_gpu_base);

	// Setup samplers
	_cmd_list->SetGraphicsRootDescriptorTable(2, effect_data.sampler_gpu_base);

	// Clear default depth stencil
	const D3D12_CPU_DESCRIPTOR_HANDLE default_depth_stencil = _depthstencil_dsvs->GetCPUDescriptorHandleForHeapStart();
	_cmd_list->ClearDepthStencilView(default_depth_stencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	for (size_t i = 0; i < technique.passes.size(); ++i)
	{
		const auto &pass_info = technique.passes[i];
		const auto &pass_data = *technique.passes_data[i]->as<d3d12_pass_data>();

		// Transition render targets
		for (unsigned int k = 0; k < pass_data.num_render_targets; ++k)
		{
			const auto texture_impl = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			})->impl->as<d3d12_tex_data>();

			if (texture_impl->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
				transition_state(_cmd_list, texture_impl->resource, texture_impl->state, D3D12_RESOURCE_STATE_RENDER_TARGET);
			texture_impl->state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		// Save back buffer of previous pass
		transition_state(_cmd_list, _backbuffer_texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
		_cmd_list->CopyResource(_backbuffer_texture.get(), _backbuffers[_swap_index].get());
		transition_state(_cmd_list, _backbuffer_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Setup states
		_cmd_list->SetPipelineState(pass_data.pipeline.get());
		_cmd_list->OMSetStencilRef(pass_info.stencil_reference_value);

		// Setup render targets
		const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		if (pass_data.num_render_targets == 0)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE render_target = { _backbuffer_rtvs->GetCPUDescriptorHandleForHeapStart().ptr + (_swap_index * 2 + pass_info.srgb_write_enable) * _rtv_handle_size };
			_cmd_list->OMSetRenderTargets(1, &render_target, false, pass_info.stencil_enable ? &default_depth_stencil : nullptr);

			if (pass_info.clear_render_targets)
				_cmd_list->ClearRenderTargetView(render_target, clear_color, 0, nullptr);
		}
		else if (_width == UINT(pass_data.viewport.Width) && _height == UINT(pass_data.viewport.Height))
		{
			_cmd_list->OMSetRenderTargets(pass_data.num_render_targets, &pass_data.render_targets, true, pass_info.stencil_enable ? &default_depth_stencil : nullptr);

			if (pass_info.clear_render_targets)
				for (UINT k = 0; k < pass_data.num_render_targets; ++k)
					_cmd_list->ClearRenderTargetView({ pass_data.render_targets.ptr + k * _rtv_handle_size }, clear_color, 0, nullptr);
		}
		else
		{
			assert(!pass_info.stencil_enable);

			_cmd_list->OMSetRenderTargets(pass_data.num_render_targets, &pass_data.render_targets, true, nullptr);

			if (pass_info.clear_render_targets)
				for (UINT k = 0; k < pass_data.num_render_targets; ++k)
					_cmd_list->ClearRenderTargetView({ pass_data.render_targets.ptr + k * _rtv_handle_size }, clear_color, 0, nullptr);
		}

		_cmd_list->RSSetViewports(1, &pass_data.viewport);

		D3D12_RECT scissor_rect = { 0, 0, LONG(pass_data.viewport.Width), LONG(pass_data.viewport.Height) };
		_cmd_list->RSSetScissorRects(1, &scissor_rect);

		// Draw triangle
		_cmd_list->DrawInstanced(pass_info.num_vertices, 1, 0, 0);

		_vertices += pass_info.num_vertices;
		_drawcalls += 1;

		// Generate mipmaps
		for (unsigned int k = 0; k < pass_data.num_render_targets; ++k)
		{
			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			generate_mipmaps(*render_target_texture);
		}
	}

	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	execute_command_list();
}

bool reshade::d3d12::runtime_d3d12::begin_command_list(const com_ptr<ID3D12PipelineState> &state) const
{
	// Reset command list using current command allocator and put it into the recording state
	return SUCCEEDED(_cmd_list->Reset(_cmd_alloc[_swap_index].get(), state.get()));
}
void reshade::d3d12::runtime_d3d12::execute_command_list() const
{
	if (FAILED(_cmd_list->Close()))
		return;

	ID3D12CommandList *const cmd_lists[] = { _cmd_list.get() };
	_commandqueue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);
}
void reshade::d3d12::runtime_d3d12::wait_for_command_queue() const
{
	const UINT64 sync_value = ++_fence_value[_swap_index];
	_commandqueue->Signal(_fence[_swap_index].get(), sync_value);
	_fence[_swap_index]->SetEventOnCompletion(sync_value, _fence_event);
	WaitForSingleObject(_fence_event, INFINITE);
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

		_imgui_signature = create_root_signature(desc);
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = _imgui_signature.get();
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

	return SUCCEEDED(_device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&_imgui_pipeline)));
}

void reshade::d3d12::runtime_d3d12::render_imgui_draw_data(ImDrawData *draw_data)
{
	// Need to multi-buffer vertex data so not to modify data below when the previous frame is still in flight
	const unsigned int buffer_index = _framecount % NUM_IMGUI_BUFFERS;

	// Create and grow vertex/index buffers if needed
	if (_imgui_index_buffer_size[buffer_index] < draw_data->TotalIdxCount)
	{
		_imgui_index_buffer[buffer_index].reset();

		const int new_size = draw_data->TotalIdxCount + 10000;
		D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
		desc.Width = new_size * sizeof(ImDrawIdx);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&_imgui_index_buffer[buffer_index]))))
			return;
#ifdef _DEBUG
		_imgui_index_buffer[buffer_index]->SetName(L"ImGui Index Buffer");
#endif
		_imgui_index_buffer_size[buffer_index] = new_size;
	}
	if (_imgui_vertex_buffer_size[buffer_index] < draw_data->TotalVtxCount)
	{
		_imgui_vertex_buffer[buffer_index].reset();

		const int new_size = draw_data->TotalVtxCount + 5000;
		D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
		desc.Width = new_size * sizeof(ImDrawVert);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc = { 1, 0 };
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		D3D12_HEAP_PROPERTIES props = { D3D12_HEAP_TYPE_UPLOAD };

		if (FAILED(_device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&_imgui_vertex_buffer[buffer_index]))))
			return;
#ifdef _DEBUG
		_imgui_index_buffer[buffer_index]->SetName(L"ImGui Vertex Buffer");
#endif
		_imgui_vertex_buffer_size[buffer_index] = new_size;
	}

	ImDrawIdx *idx_dst; ImDrawVert *vtx_dst;
	if (FAILED(_imgui_index_buffer[buffer_index]->Map(0, nullptr, reinterpret_cast<void **>(&idx_dst))) ||
		FAILED(_imgui_vertex_buffer[buffer_index]->Map(0, nullptr, reinterpret_cast<void **>(&vtx_dst))))
		return;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList *draw_list = draw_data->CmdLists[n];
		CopyMemory(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		CopyMemory(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		idx_dst += draw_list->IdxBuffer.Size;
		vtx_dst += draw_list->VtxBuffer.Size;
	}

	_imgui_index_buffer[buffer_index]->Unmap(0, nullptr);
	_imgui_vertex_buffer[buffer_index]->Unmap(0, nullptr);

	if (!begin_command_list(_imgui_pipeline))
		return;

	// Transition render target
	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

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
		_imgui_index_buffer[buffer_index]->GetGPUVirtualAddress(), _imgui_index_buffer_size[buffer_index] * sizeof(ImDrawIdx), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
	_cmd_list->IASetIndexBuffer(&index_buffer_view);
	const D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {
		_imgui_vertex_buffer[buffer_index]->GetGPUVirtualAddress(), _imgui_vertex_buffer_size[buffer_index] * sizeof(ImDrawVert),  sizeof(ImDrawVert) };
	_cmd_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmd_list->SetGraphicsRootSignature(_imgui_signature.get());
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

			const auto texture_impl = static_cast<d3d12_tex_data *>(cmd.TextureId);

			if (texture_impl->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				transition_state(_cmd_list, texture_impl->resource, texture_impl->state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			texture_impl->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			// First descriptor in resource-specific descriptor heap is SRV to top-most mipmap level
			ID3D12DescriptorHeap *const descriptor_heap = { texture_impl->descriptors.get() };
			_cmd_list->SetDescriptorHeaps(1, &descriptor_heap);
			_cmd_list->SetGraphicsRootDescriptorTable(1, descriptor_heap->GetGPUDescriptorHandleForHeapStart());

			_cmd_list->DrawIndexedInstanced(cmd.ElemCount, 1, cmd.IdxOffset + idx_offset, cmd.VtxOffset + vtx_offset, 0);

		}

		idx_offset += draw_list->IdxBuffer.Size;
		vtx_offset += draw_list->VtxBuffer.Size;
	}

	// Transition render target back to previous state
	transition_state(_cmd_list, _backbuffers[_swap_index], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	execute_command_list();
}
#endif

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
void reshade::d3d12::runtime_d3d12::draw_depth_debug_menu()
{
	if (ImGui::CollapsingHeader("Depth Buffers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		assert(_current_tracker != nullptr);

		bool modified = false;
		modified |= ImGui::Checkbox("Use aspect ratio heuristics", &_filter_aspect_ratio);
		modified |= ImGui::Checkbox("Copy depth buffers before clear operation", &_preserve_depth_buffers);

		if (modified) // Detection settings have changed, reset heuristic
			_current_tracker->reset(true, true);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		for (const auto &[dsv_texture, snapshot] : _current_tracker->depth_buffer_counters())
		{
			char label[512] = "";
			sprintf_s(label, "%s0x%p", (dsv_texture == _depth_texture || dsv_texture == _current_tracker->current_depth_texture() ? "> " : "  "), dsv_texture.get());

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

			if (_preserve_depth_buffers && dsv_texture == _current_tracker->current_depth_texture())
			{
				for (UINT clear_index = 1; clear_index <= snapshot.clears.size(); ++clear_index)
				{
					sprintf_s(label, "%s  CLEAR %2u", (clear_index == _current_tracker->current_clear_index() ? "> " : "  "), clear_index);

					if (bool value = (_depth_clear_index_override == clear_index);
						ImGui::Checkbox(label, &value))
					{
						_depth_clear_index_override = value ? clear_index : std::numeric_limits<UINT>::max();
						modified = true;
					}

					ImGui::SameLine();
					ImGui::Text("%*s|           | %5u draw calls ==> %8u vertices |",
						sizeof(dsv_texture) - 4, "", // Add space to fill pointer length
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
}

void reshade::d3d12::runtime_d3d12::update_depthstencil_texture(com_ptr<ID3D12Resource> texture)
{
	if (texture == _depth_texture)
		return;

	_depth_texture = std::move(texture);

	D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = {};

	if (_depth_texture != nullptr)
	{
		for (auto &tex : _textures)
		{
			if (tex.impl != nullptr && tex.impl_reference == texture_reference::depth_buffer)
			{
				tex.width = frame_width();
				tex.height = frame_height();
			}
		}

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
