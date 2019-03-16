/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "runtime_d3d12.hpp"
#include "runtime_objects.hpp"
#include "dxgi/format_utils.hpp"
#include <imgui.h>
#include <d3dcompiler.h>

namespace reshade::d3d12
{
	struct d3d12_tex_data : base_object
	{
		com_ptr<ID3D12Resource> resource;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv[2];
	};
	struct d3d12_pass_data : base_object
	{
		com_ptr<ID3D12PipelineState> pipeline;
		D3D12_VIEWPORT viewport;
		bool clear_render_targets;
		UINT stencil_reference;
		D3D12_CPU_DESCRIPTOR_HANDLE render_targets[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	};
	struct d3d12_technique_data : base_object
	{

	};
}

reshade::d3d12::runtime_d3d12::runtime_d3d12(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) :
	_device(device), _commandqueue(queue), _swapchain(swapchain)
{
	assert(queue != nullptr);
	assert(device != nullptr);
	assert(swapchain != nullptr);

	com_ptr<IDXGIDevice> dxgi_device;
	_device->QueryInterface(&dxgi_device);
	com_ptr<IDXGIAdapter> dxgi_adapter;
	dxgi_device->GetAdapter(&dxgi_adapter);

	_renderer_id = D3D_FEATURE_LEVEL_12_0;
	if (DXGI_ADAPTER_DESC desc; SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
		_vendor_id = desc.VendorId, _device_id = desc.DeviceId;
}
reshade::d3d12::runtime_d3d12::~runtime_d3d12()
{
	if (_d3d_compiler != nullptr)
		FreeLibrary(_d3d_compiler);
}

bool reshade::d3d12::runtime_d3d12::on_init(const DXGI_SWAP_CHAIN_DESC &desc)
{
	RECT window_rect = {};
	GetClientRect(desc.OutputWindow, &window_rect);

	_width = desc.BufferDesc.Width;
	_height = desc.BufferDesc.Height;
	_window_width = window_rect.right - window_rect.left;
	_window_height = window_rect.bottom - window_rect.top;

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
	assert(texture.impl_reference == texture_reference::none);

	const auto texture_impl = texture.impl->as<d3d12_tex_data>();

	D3D12_RESOURCE_DESC host_desc = texture_impl->resource->GetDesc();
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

	com_ptr<ID3D12Resource> intermediate;
	_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &host_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate));

	uint8_t *data2 = nullptr;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&data2))))
		return;

	switch (texture.format)
	{
	case reshadefx::texture_format::r8: {
		for (size_t i = 0, k = 0; i < texture.width * texture.height * 4; i += 4, k++)
			data2[k] = data[i];
		break; }
	case reshadefx::texture_format::rg8: {
		for (size_t i = 0, k = 0; i < texture.width * texture.height * 4; i += 4, k += 2)
			data2[k] = data[i],
			data2[k + 1] = data[i + 1];
		break; }
	default:
		memcpy(data2, data, texture.width * texture.height * 4);
		break;
	}

	intermediate->Unmap(0, nullptr);

	com_ptr<ID3D12GraphicsCommandList> cmd_list;
	_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, nullptr, nullptr, IID_PPV_ARGS(&cmd_list));

	cmd_list->CopyResource(texture_impl->resource.get(), intermediate.get());

	ID3D12CommandList *cmd_lists[] = { cmd_list.get() };
	_commandqueue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);
}

bool reshade::d3d12::runtime_d3d12::compile_effect(effect_data &effect)
{
	if (_d3d_compiler == nullptr)
		_d3d_compiler = LoadLibraryW(L"d3dcompiler_47.dll");

	if (_d3d_compiler == nullptr)
	{
		LOG(ERROR) << "Unable to load D3DCompiler library.";
		return false;
	}

	const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3d_compiler, "D3DCompile"));

	const std::string hlsl = effect.preamble + effect.module.hlsl;

	std::unordered_map<std::string, com_ptr<ID3DBlob>> entry_points;

	// Compile the generated HLSL source code to DX byte code
	for (const auto &entry_point : effect.module.entry_points)
	{
		std::string profile = entry_point.second ? "ps_6_0" : "vs_6_0";

		com_ptr<ID3DBlob> d3d_errors;

		HRESULT hr = D3DCompile(hlsl.c_str(), hlsl.size(), nullptr, nullptr, nullptr, entry_point.first.c_str(), profile.c_str(), D3DCOMPILE_ENABLE_STRICTNESS, 0, &entry_points[entry_point.first], &d3d_errors);

		if (d3d_errors != nullptr) // Append warnings to the output error string as well
			effect.errors.append(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		// No need to setup resources if any of the shaders failed to compile
		if (FAILED(hr))
			return false;
	}

	/*if (effect.storage_size != 0)
	{
		com_ptr<ID3D11Buffer> cbuffer;

		const D3D11_BUFFER_DESC desc = { static_cast<UINT>(effect.storage_size), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE };
		const D3D11_SUBRESOURCE_DATA init_data = { _uniform_data_storage.data() + effect.storage_offset, static_cast<UINT>(effect.storage_size) };

		if (const HRESULT hr = _device->CreateBuffer(&desc, &init_data, &cbuffer); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create constant buffer for effect file " << effect.source_file << ". "
				"HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}

		_constant_buffers.push_back(std::move(cbuffer));
	}*/

	bool success = true;

	d3d12_technique_data technique_init;
	//technique_init.uniform_storage_index = _constant_buffers.size() - 1;
	//technique_init.uniform_storage_offset = effect.storage_offset;

	//for (const reshadefx::sampler_info &info : effect.module.samplers)
	//	success &= add_sampler(info, technique_init);

	for (technique &technique : _techniques)
		if (technique.impl == nullptr && technique.effect_index == effect.index)
			success &= init_technique(technique, technique_init, entry_points);

	return success;
}
void reshade::d3d12::runtime_d3d12::unload_effects()
{
	runtime::unload_effects();
}

bool reshade::d3d12::runtime_d3d12::init_technique(technique &technique, const d3d12_technique_data &impl_init, const std::unordered_map<std::string, com_ptr<ID3DBlob>> &entry_points)
{
	technique.impl = std::make_unique<d3d12_technique_data>(impl_init);

	const auto technique_data = technique.impl->as<d3d12_technique_data>();

	for (size_t pass_index = 0; pass_index < technique.passes.size(); ++pass_index)
	{
		technique.passes_data.push_back(std::make_unique<d3d12_pass_data>());

		auto &pass = *technique.passes_data.back()->as<d3d12_pass_data>();
		const auto &pass_info = technique.passes[pass_index];

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc = {};

		const auto &VS = entry_points.at(pass_info.vs_entry_point);
		pipeline_desc.VS.pShaderBytecode = VS->GetBufferPointer();
		pipeline_desc.VS.BytecodeLength = VS->GetBufferSize();
		const auto &PS = entry_points.at(pass_info.ps_entry_point);
		pipeline_desc.PS.pShaderBytecode = PS->GetBufferPointer();
		pipeline_desc.PS.BytecodeLength = PS->GetBufferSize();

		pass.viewport.MaxDepth = 1.0f;
		pass.viewport.Width = static_cast<FLOAT>(pass_info.viewport_width);
		pass.viewport.Height = static_cast<FLOAT>(pass_info.viewport_height);

		pass.clear_render_targets = pass_info.clear_render_targets;

		const int target_index = pass_info.srgb_write_enable ? 1 : 0;
		//pass.render_targets[0] = _backbuffer_rtv[target_index];

		pipeline_desc.NumRenderTargets = 1;
		pipeline_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		for (unsigned int k = 0; k < 8; k++)
		{
			if (pass_info.render_target_names[k].empty())
				continue; // Skip unbound render targets

			pipeline_desc.NumRenderTargets = k + 1;
			pipeline_desc.RTVFormats[k] = DXGI_FORMAT_UNKNOWN; // TODO

			const auto render_target_texture = std::find_if(_textures.begin(), _textures.end(),
				[&render_target = pass_info.render_target_names[k]](const auto &item) {
				return item.unique_name == render_target;
			});

			if (render_target_texture == _textures.end())
				return assert(false), false;

			const auto texture_impl = render_target_texture->impl->as<d3d12_tex_data>();

			assert(texture_impl != nullptr);

			const D3D12_RESOURCE_DESC desc = texture_impl->resource->GetDesc();

			D3D12_RENDER_TARGET_VIEW_DESC rtvdesc = {};
			rtvdesc.Format = pass_info.srgb_write_enable ? make_dxgi_format_srgb(desc.Format) : make_dxgi_format_normal(desc.Format);
			rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;

			if (!texture_impl->rtv[target_index].ptr)
			{
				// TODO Init descriptor

				_device->CreateRenderTargetView(texture_impl->resource.get(), &rtvdesc, texture_impl->rtv[target_index]);
			}

			pass.render_targets[k] = texture_impl->rtv[target_index];
		}

		if (pass.viewport.Width == 0)
		{
			pass.viewport.Width = static_cast<FLOAT>(frame_width());
			pass.viewport.Height = static_cast<FLOAT>(frame_height());
		}

		pass.stencil_reference = pass_info.stencil_reference_value;

		pipeline_desc.SampleMask = 0;
		pipeline_desc.SampleDesc = { 1, 0 };

		pipeline_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		pipeline_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipeline_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipeline_desc.RasterizerState.DepthClipEnable = true;

		const auto literal_to_stencil_op = [](unsigned int value) {
			switch (value)
			{
			default:
			case 1:
				return D3D12_STENCIL_OP_KEEP;
			case 0:
				return D3D12_STENCIL_OP_ZERO;
			case 3:
				return D3D12_STENCIL_OP_REPLACE;
			case 4:
				return D3D12_STENCIL_OP_INCR_SAT;
			case 5:
				return D3D12_STENCIL_OP_DECR_SAT;
			case 6:
				return D3D12_STENCIL_OP_INVERT;
			case 7:
				return D3D12_STENCIL_OP_INCR;
			case 8:
				return D3D12_STENCIL_OP_DECR;
			}
		};

		pipeline_desc.DepthStencilState.DepthEnable = FALSE;
		pipeline_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		pipeline_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		pipeline_desc.DepthStencilState.StencilEnable = pass_info.stencil_enable;
		pipeline_desc.DepthStencilState.StencilReadMask = pass_info.stencil_read_mask;
		pipeline_desc.DepthStencilState.StencilWriteMask = pass_info.stencil_write_mask;
		pipeline_desc.DepthStencilState.FrontFace.StencilFunc =
			pipeline_desc.DepthStencilState.BackFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(pass_info.stencil_comparison_func);
		pipeline_desc.DepthStencilState.FrontFace.StencilPassOp =
			pipeline_desc.DepthStencilState.BackFace.StencilPassOp = literal_to_stencil_op(pass_info.stencil_op_pass);
		pipeline_desc.DepthStencilState.FrontFace.StencilFailOp =
			pipeline_desc.DepthStencilState.BackFace.StencilFailOp = literal_to_stencil_op(pass_info.stencil_op_fail);
		pipeline_desc.DepthStencilState.FrontFace.StencilDepthFailOp =
			pipeline_desc.DepthStencilState.BackFace.StencilDepthFailOp = literal_to_stencil_op(pass_info.stencil_op_depth_fail);

		const auto literal_to_blend_func = [](unsigned int value) {
			switch (value)
			{
			case 0:
				return D3D12_BLEND_ZERO;
			default:
			case 1:
				return D3D12_BLEND_ONE;
			case 2:
				return D3D12_BLEND_SRC_COLOR;
			case 4:
				return D3D12_BLEND_INV_SRC_COLOR;
			case 3:
				return D3D12_BLEND_SRC_ALPHA;
			case 5:
				return D3D12_BLEND_INV_SRC_ALPHA;
			case 6:
				return D3D12_BLEND_DEST_ALPHA;
			case 7:
				return D3D12_BLEND_INV_DEST_ALPHA;
			case 8:
				return D3D12_BLEND_DEST_COLOR;
			case 9:
				return D3D12_BLEND_INV_DEST_COLOR;
			}
		};

		pipeline_desc.BlendState.AlphaToCoverageEnable = FALSE;
		pipeline_desc.BlendState.IndependentBlendEnable = FALSE;
		pipeline_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = pass_info.color_write_mask;
		pipeline_desc.BlendState.RenderTarget[0].BlendEnable = pass_info.blend_enable;
		pipeline_desc.BlendState.RenderTarget[0].BlendOp = static_cast<D3D12_BLEND_OP>(pass_info.blend_op);
		pipeline_desc.BlendState.RenderTarget[0].BlendOpAlpha = static_cast<D3D12_BLEND_OP>(pass_info.blend_op_alpha);
		pipeline_desc.BlendState.RenderTarget[0].SrcBlend = literal_to_blend_func(pass_info.src_blend);
		pipeline_desc.BlendState.RenderTarget[0].DestBlend = literal_to_blend_func(pass_info.dest_blend);
		pipeline_desc.BlendState.RenderTarget[0].SrcBlendAlpha = literal_to_blend_func(pass_info.src_blend_alpha);
		pipeline_desc.BlendState.RenderTarget[0].DestBlendAlpha = literal_to_blend_func(pass_info.dest_blend_alpha);

		if (HRESULT hr = _device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(&pass.pipeline)); FAILED(hr))
		{
			LOG(ERROR) << "Failed to create pipeline for pass " << pass_index << " in technique '" << technique.name << "'! "
				"HRESULT is '" << std::hex << hr << std::dec << "'.";
			return false;
		}
	}

	return true;
}

void reshade::d3d12::runtime_d3d12::render_technique(technique &technique)
{
	d3d12_technique_data &technique_data = *technique.impl->as<d3d12_technique_data>();

	bool is_default_depthstencil_cleared = false;

	com_ptr<ID3D12GraphicsCommandList> cmd_list;

	for (const auto &pass_object : technique.passes_data)
	{
		const d3d12_pass_data &pass = *pass_object->as<d3d12_pass_data>();

		cmd_list->SetPipelineState(pass.pipeline.get());
		cmd_list->OMSetStencilRef(pass.stencil_reference);

		// Save back buffer of previous pass
		//cmd_list->CopyResource(_backbuffer_texture.get(), _backbuffer_resolved.get());

		// Setup shader resources
		cmd_list->SetGraphicsRootShaderResourceView(0, 0); // TODO

		// Setup render targets
		if (static_cast<UINT>(pass.viewport.Width) == _width && static_cast<UINT>(pass.viewport.Height) == _height)
		{
			cmd_list->OMSetRenderTargets(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.render_targets, false, &_default_depthstencil);

			if (!is_default_depthstencil_cleared)
			{
				is_default_depthstencil_cleared = true;

				cmd_list->ClearDepthStencilView(_default_depthstencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
			}
		}
		else
		{
			cmd_list->OMSetRenderTargets(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, pass.render_targets, false, nullptr);
		}

		cmd_list->RSSetViewports(1, &pass.viewport);

		if (pass.clear_render_targets)
		{
			for (const auto &target : pass.render_targets)
			{
				if (target.ptr == 0)
					continue;

				const float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				cmd_list->ClearRenderTargetView(target, color, 0, nullptr);
			}
		}

		// Draw triangle
		cmd_list->DrawInstanced(3, 1, 0, 0);

		_vertices += 3;
		_drawcalls += 1;
	}

	ID3D12CommandList *const cmd_lists[] = { cmd_list.get() };
	_commandqueue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);
}

#if RESHADE_GUI
void reshade::d3d12::runtime_d3d12::render_imgui_draw_data(ImDrawData *draw_data)
{
}
#endif
