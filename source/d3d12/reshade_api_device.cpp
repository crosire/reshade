/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "reshade_api_device.hpp"
#include "reshade_api_command_queue.hpp"
#include "reshade_api_type_utils.hpp"
#include "reshade_api_format_utils.hpp"

const GUID reshade::d3d12::pipeline_extra_data_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };

reshade::d3d12::device_impl::device_impl(ID3D12Device *device) :
	api_object_impl(device),
	_view_heaps {
		descriptor_heap_cpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
		descriptor_heap_cpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
		descriptor_heap_cpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
		descriptor_heap_cpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV) },
	_gpu_view_heap(device),
	_gpu_sampler_heap(device)
{
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		_descriptor_handle_size[type] = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));
	}

	// Create mipmap generation states
	{
		D3D12_DESCRIPTOR_RANGE srv_range = {};
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

		if (com_ptr<ID3DBlob> signature_blob;
			FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, nullptr)) ||
			FAILED(_orig->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&_mipmap_signature))))
		{
			LOG(ERROR) << "Failed to create mipmap generation signature!";
			return;
		}

		D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
		pso_desc.pRootSignature = _mipmap_signature.get();

		const resources::data_resource cs = resources::load_data_resource(IDR_MIPMAP_CS);
		pso_desc.CS = { cs.data, cs.data_size };

		if (FAILED(_orig->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&_mipmap_pipeline))))
		{
			LOG(ERROR) << "Failed to create mipmap generation pipeline!";
			return;
		}
	}

#if RESHADE_ADDON
	addon::load_addons();

	invoke_addon_event<addon_event::init_device>(this);
#endif
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

	// Do not call add-on events if initialization failed
	if (_mipmap_pipeline == nullptr)
		return;

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_device>(this);

	addon::unload_addons();
#endif
}

bool reshade::d3d12::device_impl::check_capability(api::device_caps capability) const
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS options;

	switch (capability)
	{
	case api::device_caps::compute_shader:
	case api::device_caps::geometry_shader:
	case api::device_caps::hull_and_domain_shader:
	case api::device_caps::dual_src_blend:
	case api::device_caps::independent_blend:
		return true;
	case api::device_caps::logic_op:
		_orig->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
		return options.OutputMergerLogicOp;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		return false; // TODO: Not currently implemented
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::multi_viewport:
	case api::device_caps::partial_push_constant_updates:
		return true;
	case api::device_caps::partial_push_descriptor_updates:
		return false;
	case api::device_caps::sampler_compare_op:
	case api::device_caps::sampler_anisotropic_filtering:
		return true;
	case api::device_caps::sampler_with_resource_view:
		return false;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
		return true;
	case api::device_caps::blit:
		return false;
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_results:
		return true;
	default:
		return false;
	}
}
bool reshade::d3d12::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT feature = { convert_format(format) };
	if (feature.Format == DXGI_FORMAT_UNKNOWN ||
		FAILED(_orig->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &feature, sizeof(feature))))
		return false;

	if ((usage & api::resource_usage::depth_stencil) != api::resource_usage::undefined &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != api::resource_usage::undefined &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != api::resource_usage::undefined &&
		(feature.Support1 & (D3D12_FORMAT_SUPPORT1_SHADER_LOAD | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != api::resource_usage::undefined &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) == 0)
		return false;
	if ((usage & (api::resource_usage::resolve_dest | api::resource_usage::resolve_source)) != api::resource_usage::undefined &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d12::device_impl::is_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(handle.handle));
}
bool reshade::d3d12::device_impl::is_resource_view_handle_valid(api::resource_view handle) const
{
	const std::lock_guard<std::mutex> lock(_mutex);
	return _views.find(handle.handle) != _views.end();
}

bool reshade::d3d12::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].allocate(descriptor_handle))
	{
		*out = { 0 };
		return false;
	}

	D3D12_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	_orig->CreateSampler(&internal_desc, descriptor_handle);

	*out = { descriptor_handle.ptr };
	return true;
}
bool reshade::d3d12::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out)
{
	assert((desc.usage & initial_state) == initial_state || initial_state == api::resource_usage::cpu_access);

	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_DESC internal_desc = {};
	D3D12_HEAP_PROPERTIES heap_props = {};
	convert_resource_desc(desc, internal_desc, heap_props, heap_flags);
	if (desc.type == api::resource_type::buffer)
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// Constant buffer views need to be aligned to 256 bytes, so make buffer large enough to ensure that is possible
	if ((desc.usage & api::resource_usage::constant_buffer) != api::resource_usage::undefined)
		internal_desc.Width = (internal_desc.Width + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u);

	// Use a default clear value of transparent black (all zeroes)
	D3D12_CLEAR_VALUE default_clear_value = {};
	default_clear_value.Format = convert_format(api::format_to_typeless(desc.texture.format) == desc.texture.format ? api::format_to_default_typed(desc.texture.format) : desc.texture.format);
	const bool use_default_clear_value = (desc.usage & (api::resource_usage::depth_stencil | api::resource_usage::render_target)) != api::resource_usage::undefined;

	if (com_ptr<ID3D12Resource> object;
		SUCCEEDED(_orig->CreateCommittedResource(&heap_props, heap_flags, &internal_desc, convert_resource_usage_to_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object))))
	{
		_resources.register_object(object.get());
		*out = { reinterpret_cast<uintptr_t>(object.release()) };

		if (initial_data != nullptr)
		{
			assert(initial_state != api::resource_usage::undefined);

			// Transition resource into the initial state using the first available immediate command list
			for (command_queue_impl *const queue : _queues)
			{
				const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
				if (immediate_command_list != nullptr)
				{
					const api::resource_usage states_upload[2] = { initial_state, api::resource_usage::copy_dest };
					immediate_command_list->barrier(1, out, &states_upload[0], &states_upload[1]);

					if (desc.type == api::resource_type::buffer)
					{
						upload_buffer_region(initial_data->data, *out, 0, desc.buffer.size);
					}
					else
					{
						for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
							upload_texture_region(initial_data[subresource], *out, subresource, nullptr);
					}

					const api::resource_usage states_finalize[2] = { api::resource_usage::copy_dest, initial_state };
					immediate_command_list->barrier(1, out, &states_finalize[0], &states_finalize[1]);

					queue->flush_immediate_command_list();
					break;
				}
			}
		}

		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out)
{
	assert(resource.handle != 0);

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].allocate(descriptor_handle))
				break; // No more space available in the descriptor heap

			D3D12_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateDepthStencilView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out = { descriptor_handle.ptr };
			return true;
		}
		case api::resource_usage::render_target:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV].allocate(descriptor_handle))
				break;

			D3D12_RENDER_TARGET_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateRenderTargetView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out = { descriptor_handle.ptr };
			return true;
		}
		case api::resource_usage::shader_resource:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].allocate(descriptor_handle))
				break;

			D3D12_SHADER_RESOURCE_VIEW_DESC internal_desc = {};
			internal_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateShaderResourceView(reinterpret_cast<ID3D12Resource *>(resource.handle), &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out = { descriptor_handle.ptr };
			return true;
		}
		case api::resource_usage::unordered_access:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV].allocate(descriptor_handle))
				break;

			D3D12_UNORDERED_ACCESS_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, &internal_desc, descriptor_handle);

			register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), descriptor_handle);
			*out = { descriptor_handle.ptr };
			return true;
		}
	}

	*out = { 0 };
	return false;
}

bool reshade::d3d12::device_impl::create_pipeline(const api::pipeline_desc &desc, api::pipeline *out)
{
	switch (desc.type)
	{
	default:
		*out = { 0 };
		return false;
	case api::pipeline_type::compute:
		return create_pipeline_compute(desc, out);
	case api::pipeline_type::graphics:
		return create_pipeline_graphics_all(desc, out);
	}
}
bool reshade::d3d12::device_impl::create_pipeline_compute(const api::pipeline_desc &desc, api::pipeline *out)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC internal_desc = {};
	internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(desc.layout.handle);

	if (desc.compute.shader.handle != 0)
	{
		const auto cs_module = reinterpret_cast<const shader_module_impl *>(desc.compute.shader.handle);
		internal_desc.CS = { cs_module->bytecode.data(), cs_module->bytecode.size() };
	}

	if (com_ptr<ID3D12PipelineState> pipeline;
		SUCCEEDED(_orig->CreateComputePipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
	{
		*out = { reinterpret_cast<uintptr_t>(pipeline.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_pipeline_graphics_all(const api::pipeline_desc &desc, api::pipeline *out)
{
	for (UINT i = 0; i < desc.graphics.num_dynamic_states; ++i)
	{
		if (desc.graphics.dynamic_states[i] != api::pipeline_state::stencil_reference_value &&
			desc.graphics.dynamic_states[i] != api::pipeline_state::blend_constant &&
			desc.graphics.dynamic_states[i] != api::pipeline_state::primitive_topology)
		{
			*out = { 0 };
			return false;
		}
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC internal_desc = {};
	internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(desc.layout.handle);

	if (desc.graphics.vertex_shader.handle != 0)
	{
		const auto vs_module = reinterpret_cast<const shader_module_impl *>(desc.graphics.vertex_shader.handle);
		internal_desc.VS = { vs_module->bytecode.data(), vs_module->bytecode.size() };
	}
	if (desc.graphics.hull_shader.handle != 0)
	{
		const auto hs_module = reinterpret_cast<const shader_module_impl *>(desc.graphics.hull_shader.handle);
		internal_desc.HS = { hs_module->bytecode.data(), hs_module->bytecode.size() };
	}
	if (desc.graphics.domain_shader.handle != 0)
	{
		const auto ds_module = reinterpret_cast<const shader_module_impl *>(desc.graphics.domain_shader.handle);
		internal_desc.DS = { ds_module->bytecode.data(), ds_module->bytecode.size() };
	}
	if (desc.graphics.geometry_shader.handle != 0)
	{
		const auto gs_module = reinterpret_cast<const shader_module_impl *>(desc.graphics.geometry_shader.handle);
		internal_desc.GS = { gs_module->bytecode.data(), gs_module->bytecode.size() };
	}
	if (desc.graphics.pixel_shader.handle != 0)
	{
		const auto ps_module = reinterpret_cast<const shader_module_impl *>(desc.graphics.pixel_shader.handle);
		internal_desc.PS = { ps_module->bytecode.data(), ps_module->bytecode.size() };
	}

	internal_desc.BlendState.AlphaToCoverageEnable = desc.graphics.multisample_state.alpha_to_coverage;
	internal_desc.BlendState.IndependentBlendEnable = TRUE;

	for (UINT i = 0; i < 8; ++i)
	{
		internal_desc.BlendState.RenderTarget[i].BlendEnable = desc.graphics.blend_state.blend_enable[i];
		internal_desc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
		internal_desc.BlendState.RenderTarget[i].SrcBlend = convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].DestBlend = convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].BlendOp = convert_blend_op(desc.graphics.blend_state.color_blend_op[i]);
		internal_desc.BlendState.RenderTarget[i].SrcBlendAlpha = convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].DestBlendAlpha = convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i]);
		internal_desc.BlendState.RenderTarget[i].BlendOpAlpha = convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i]);
		internal_desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
		internal_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}

	internal_desc.SampleMask = desc.graphics.multisample_state.sample_mask;

	internal_desc.RasterizerState.FillMode = convert_fill_mode(desc.graphics.rasterizer_state.fill_mode);
	internal_desc.RasterizerState.CullMode = convert_cull_mode(desc.graphics.rasterizer_state.cull_mode);
	internal_desc.RasterizerState.FrontCounterClockwise = desc.graphics.rasterizer_state.front_counter_clockwise;
	internal_desc.RasterizerState.DepthBias = static_cast<INT>(desc.graphics.rasterizer_state.depth_bias);
	internal_desc.RasterizerState.DepthBiasClamp = desc.graphics.rasterizer_state.depth_bias_clamp;
	internal_desc.RasterizerState.SlopeScaledDepthBias = desc.graphics.rasterizer_state.slope_scaled_depth_bias;
	internal_desc.RasterizerState.DepthClipEnable = desc.graphics.rasterizer_state.depth_clip;
	internal_desc.RasterizerState.MultisampleEnable = desc.graphics.multisample_state.multisample;
	internal_desc.RasterizerState.AntialiasedLineEnable = desc.graphics.rasterizer_state.antialiased_line;
	internal_desc.RasterizerState.ForcedSampleCount = 0;
	internal_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	internal_desc.DepthStencilState.DepthEnable = desc.graphics.depth_stencil_state.depth_test;
	internal_desc.DepthStencilState.DepthWriteMask = desc.graphics.depth_stencil_state.depth_write_mask ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	internal_desc.DepthStencilState.DepthFunc = convert_compare_op(desc.graphics.depth_stencil_state.depth_func);
	internal_desc.DepthStencilState.StencilEnable = desc.graphics.depth_stencil_state.stencil_test;
	internal_desc.DepthStencilState.StencilReadMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	internal_desc.DepthStencilState.StencilWriteMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	internal_desc.DepthStencilState.BackFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op);
	internal_desc.DepthStencilState.BackFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op);
	internal_desc.DepthStencilState.BackFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op);
	internal_desc.DepthStencilState.BackFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func);
	internal_desc.DepthStencilState.FrontFace.StencilFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op);
	internal_desc.DepthStencilState.FrontFace.StencilDepthFailOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op);
	internal_desc.DepthStencilState.FrontFace.StencilPassOp = convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op);
	internal_desc.DepthStencilState.FrontFace.StencilFunc = convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func);

	std::vector<D3D12_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);
	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const auto &element = desc.graphics.input_layout[i];
		D3D12_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();

		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = convert_format(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}

	internal_desc.InputLayout.NumElements = static_cast<UINT>(internal_elements.size());
	internal_desc.InputLayout.pInputElementDescs = internal_elements.data();

	internal_desc.PrimitiveTopologyType = convert_primitive_topology_type(desc.graphics.rasterizer_state.topology);

	internal_desc.NumRenderTargets = desc.graphics.blend_state.num_render_targets;
	for (UINT i = 0; i < 8; ++i)
		internal_desc.RTVFormats[i] = convert_format(desc.graphics.blend_state.render_target_format[i]);
	internal_desc.DSVFormat = convert_format(desc.graphics.depth_stencil_state.depth_stencil_format);

	internal_desc.SampleDesc.Count = desc.graphics.multisample_state.sample_count;

	if (com_ptr<ID3D12PipelineState> pipeline;
		SUCCEEDED(_orig->CreateGraphicsPipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
	{
		pipeline_graphics_impl extra_data;
		extra_data.topology = convert_primitive_topology(desc.graphics.rasterizer_state.topology);

		pipeline->SetPrivateData(pipeline_extra_data_guid, sizeof(extra_data), &extra_data);

		*out = { reinterpret_cast<uintptr_t>(pipeline.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}

bool reshade::d3d12::device_impl::create_shader_module(api::shader_stage, api::shader_format format, const void *code, size_t code_size, const char *entry_point, api::shader_module *out)
{
	if (format == api::shader_format::dxbc || format == api::shader_format::dxil)
	{
		assert(entry_point == nullptr);

		const auto result = new shader_module_impl();
		result->bytecode.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);

		*out = { reinterpret_cast<uintptr_t>(result) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_pipeline_layout(uint32_t num_set_layouts, const api::descriptor_set_layout *set_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	std::vector<D3D12_ROOT_PARAMETER> params(num_set_layouts + num_constant_ranges);
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> ranges(num_set_layouts);

	for (UINT i = 0; i < num_set_layouts; ++i)
	{
		if (set_layouts[i].handle == 0)
		{
			// Dummy parameter (to prevent root signature creation from failing)
			params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			params[i].Constants.RegisterSpace = 255;
			params[i].Constants.Num32BitValues = 1;
			continue;
		}

		params[i] = reinterpret_cast<descriptor_set_layout_impl *>(set_layouts[i].handle)->param;
	}

	for (UINT i = 0; i < num_constant_ranges; ++i)
	{
		D3D12_ROOT_PARAMETER &param = params[num_set_layouts + i];
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param.Constants.ShaderRegister = constant_ranges[i].dx_shader_register;
		param.Constants.RegisterSpace = 0;
		param.Constants.Num32BitValues = constant_ranges[i].count;

		switch (constant_ranges[i].visibility)
		{
		default:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			break;
		case api::shader_stage::vertex:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			break;
		case api::shader_stage::hull:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
			break;
		case api::shader_stage::domain:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
			break;
		case api::shader_stage::geometry:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
			break;
		case api::shader_stage::pixel:
			param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			break;
		}
	}

	D3D12_ROOT_SIGNATURE_DESC internal_desc = {};
	internal_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	internal_desc.NumParameters = num_set_layouts + num_constant_ranges;
	internal_desc.pParameters = params.data();

	com_ptr<ID3DBlob> blob;
	com_ptr<ID3D12RootSignature> signature;
	if (SUCCEEDED(D3D12SerializeRootSignature(&internal_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)) &&
		SUCCEEDED(_orig->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&signature))))
	{
		*out = { reinterpret_cast<uintptr_t>(signature.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool, api::descriptor_set_layout *out)
{
	uint32_t visibility_mask = 0;

	const auto result = new descriptor_set_layout_impl();
	result->heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	result->total_size = 0;
	result->ranges.resize(num_ranges);

	for (UINT i = 0; i < num_ranges; ++i)
	{
		D3D12_DESCRIPTOR_RANGE &range = result->ranges[i];
		range.RangeType = convert_descriptor_type(ranges[i].type);
		range.NumDescriptors = ranges[i].count;
		range.BaseShaderRegister = ranges[i].dx_shader_register;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = ranges[i].binding;

		visibility_mask |= static_cast<uint32_t>(ranges[i].visibility);

		// Cannot mix different descriptor heap types in a single descriptor table
		const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(ranges[i].type);
		if (result->heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES && result->heap_type != heap_type)
		{
			delete result;

			*out = { 0 };
			return false;
		}

		result->heap_type = heap_type;
		result->total_size = std::max(result->total_size, range.OffsetInDescriptorsFromTableStart + range.NumDescriptors);
	}

	result->param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	result->param.DescriptorTable.NumDescriptorRanges = num_ranges;
	result->param.DescriptorTable.pDescriptorRanges = result->ranges.data();

	switch (static_cast<api::shader_stage>(visibility_mask))
	{
	default:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		break;
	case api::shader_stage::vertex:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		break;
	case api::shader_stage::hull:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
		break;
	case api::shader_stage::domain:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
		break;
	case api::shader_stage::geometry:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		break;
	case api::shader_stage::pixel:
		result->param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		break;
	}

	*out = { reinterpret_cast<uintptr_t>(result) };
	return true;
}
bool reshade::d3d12::device_impl::create_query_pool(api::query_type type, uint32_t count, api::query_pool *out)
{
	com_ptr<ID3D12Resource> readback_resource;
	{
		D3D12_RESOURCE_DESC readback_desc = {};
		readback_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readback_desc.Width = count * sizeof(uint64_t);
		readback_desc.Height = 1;
		readback_desc.DepthOrArraySize = 1;
		readback_desc.MipLevels = 1;
		readback_desc.SampleDesc = { 1, 0 };
		readback_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		const D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_READBACK };

		if (FAILED(_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &readback_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_resource))))
		{
			*out = { 0 };
			return false;
		}
	}

	D3D12_QUERY_HEAP_DESC internal_desc = {};
	internal_desc.Type = convert_query_type_to_heap_type(type);
	internal_desc.Count = count;

	if (com_ptr<ID3D12QueryHeap> object;
		SUCCEEDED(_orig->CreateQueryHeap(&internal_desc, IID_PPV_ARGS(&object))))
	{
		object->SetPrivateDataInterface(pipeline_extra_data_guid, readback_resource.get());

		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out)
{
	const auto layout_impl = reinterpret_cast<const descriptor_set_layout_impl *>(layout.handle);

	for (UINT i = 0; i < count; ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;

		if (layout_impl->heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			_gpu_view_heap.allocate_static(layout_impl->total_size, base_handle, base_handle_gpu);
		else
			_gpu_sampler_heap.allocate_static(layout_impl->total_size, base_handle, base_handle_gpu);

		_descriptor_table_map[base_handle_gpu.ptr] = base_handle;
		out[i] = { base_handle_gpu.ptr };
	}

	return true;
}

void reshade::d3d12::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle == 0)
		return;

	const std::lock_guard<std::mutex> lock(_mutex);
	_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].deallocate({ static_cast<SIZE_T>(handle.handle) });
}
void reshade::d3d12::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle == 0)
		return;

	const std::lock_guard<std::mutex> lock(_mutex);
	_views.erase(handle.handle);

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		_view_heaps[i].deallocate({ static_cast<SIZE_T>(handle.handle) });
}

void reshade::d3d12::device_impl::destroy_pipeline(api::pipeline_type, api::pipeline handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_shader_module(api::shader_module handle)
{
	delete reinterpret_cast<shader_module_impl *>(handle.handle);
}
void reshade::d3d12::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_set_layout_impl *>(handle.handle);
}
void reshade::d3d12::device_impl::destroy_query_pool(api::query_pool handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets)
{
	const auto layout_impl = reinterpret_cast<descriptor_set_layout_impl *>(layout.handle);

	for (UINT i = 0; i < count; ++i)
	{
		if (sets[i].handle == 0)
			continue;

		_gpu_view_heap.deallocate({ static_cast<SIZE_T>(sets[i].handle) }, layout_impl->total_size);
		_gpu_sampler_heap.deallocate({ static_cast<SIZE_T>(sets[i].handle) }, layout_impl->total_size);
	}
}

void reshade::d3d12::device_impl::update_descriptor_sets(uint32_t num_updates, const api::descriptor_update *updates)
{
	for (UINT i = 0; i < num_updates; ++i)
	{
		const auto heap_type = convert_descriptor_type_to_heap_type(updates[i].type);

		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_start = _descriptor_table_map.at(updates[i].set.handle);
		dst_range_start.ptr += updates[i].binding * _descriptor_handle_size[heap_type];

		if (updates[i].type == api::descriptor_type::constant_buffer)
		{
			const auto buffer = reinterpret_cast<ID3D12Resource *>(updates[i].descriptor.resource.handle);

			D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
			view_desc.BufferLocation = buffer->GetGPUVirtualAddress();
			view_desc.SizeInBytes = static_cast<UINT>(buffer->GetDesc().Width);

			_orig->CreateConstantBufferView(&view_desc, dst_range_start);
		}
		else
		{
			D3D12_CPU_DESCRIPTOR_HANDLE src_range_start = {};

			switch (updates[i].type)
			{
			case api::descriptor_type::shader_resource_view:
			case api::descriptor_type::unordered_access_view:
				src_range_start.ptr = static_cast<SIZE_T>(updates[i].descriptor.view.handle);
				break;
			case api::descriptor_type::sampler:
				src_range_start.ptr = static_cast<SIZE_T>(updates[i].descriptor.sampler.handle);
				break;
			}

			_orig->CopyDescriptorsSimple(1, dst_range_start, src_range_start, heap_type);
		}
	}
}

bool reshade::d3d12::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **data)
{
	const D3D12_RANGE no_read_range = { 0, 0 };

	assert(resource.handle != 0);
	return SUCCEEDED(reinterpret_cast<ID3D12Resource *>(resource.handle)->Map(
		subresource, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read_range : nullptr, data));
}
void reshade::d3d12::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D12Resource *>(resource.handle)->Unmap(subresource, nullptr);
}

void reshade::d3d12::device_impl::upload_buffer_region(const void *data, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	assert(dst.handle != 0);
	const auto dst_resource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	const D3D12_RESOURCE_DESC dst_desc = dst_resource->GetDesc();

	// Allocate host memory for upload
	D3D12_RESOURCE_DESC intermediate_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	intermediate_desc.Width = size;
	intermediate_desc.Height = 1;
	intermediate_desc.DepthOrArraySize = 1;
	intermediate_desc.MipLevels = 1;
	intermediate_desc.SampleDesc = { 1, 0 };
	intermediate_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES upload_heap_props = { D3D12_HEAP_TYPE_UPLOAD };

	com_ptr<ID3D12Resource> intermediate;
	if (FAILED(_orig->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &intermediate_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate))))
	{
		LOG(ERROR) << "Failed to create upload buffer!";
		LOG(DEBUG) << "> Details: Width = " << intermediate_desc.Width;
		return;
	}
	intermediate->SetName(L"ReShade upload buffer");

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return;

	std::memcpy(mapped_data, data, static_cast<size_t>(size));

	intermediate->Unmap(0, nullptr);

	// Copy data from upload buffer into target texture using the first available immediate command list
	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
		{
			immediate_command_list->copy_buffer_region(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, dst, dst_offset, size);

			// Wait for command to finish executing before destroying the upload buffer
			immediate_command_list->flush_and_wait(queue->_orig);
			break;
		}
	}
}
void reshade::d3d12::device_impl::upload_texture_region(const api::subresource_data &data, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	assert(dst.handle != 0);
	const auto dst_resource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	const D3D12_RESOURCE_DESC dst_desc = dst_resource->GetDesc();

	UINT width = static_cast<UINT>(dst_desc.Width);
	UINT num_rows = dst_desc.Height;
	UINT num_slices = dst_desc.DepthOrArraySize;
	if (dst_box != nullptr)
	{
		width = dst_box[3] - dst_box[0];
		num_rows = dst_box[4] - dst_box[1];
		num_slices = dst_box[5] - dst_box[2];
	}

	auto row_pitch = width * api::format_bpp(convert_format(dst_desc.Format));
	row_pitch = (row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
	const auto slice_pitch = num_rows * row_pitch;

	// Allocate host memory for upload
	D3D12_RESOURCE_DESC intermediate_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	intermediate_desc.Width = num_slices * slice_pitch;
	intermediate_desc.Height = 1;
	intermediate_desc.DepthOrArraySize = 1;
	intermediate_desc.MipLevels = 1;
	intermediate_desc.SampleDesc = { 1, 0 };
	intermediate_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES upload_heap_props = { D3D12_HEAP_TYPE_UPLOAD };

	com_ptr<ID3D12Resource> intermediate;
	if (FAILED(_orig->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &intermediate_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate))))
	{
		LOG(ERROR) << "Failed to create upload buffer!";
		LOG(DEBUG) << "> Details: Width = " << intermediate_desc.Width;
		return;
	}
	intermediate->SetName(L"ReShade upload buffer");

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	if (FAILED(intermediate->Map(0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return;

	for (UINT z = 0; z < num_slices; ++z)
	{
		const auto dst_slice = mapped_data + z * slice_pitch;
		const auto src_slice = static_cast<const uint8_t *>(data.data) + z * data.slice_pitch;

		for (UINT y = 0; y < num_rows; ++y)
		{
			const size_t row_size = data.row_pitch < row_pitch ?
				data.row_pitch : static_cast<size_t>(row_pitch);
			std::memcpy(
				dst_slice + y * row_pitch,
				src_slice + y * data.row_pitch, row_size);
		}
	}

	intermediate->Unmap(0, nullptr);

	// Copy data from upload buffer into target texture using the first available immediate command list
	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
		{
			immediate_command_list->copy_buffer_to_texture(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, 0, 0, dst, dst_subresource, dst_box);

			// Wait for command to finish executing before destroying the upload buffer
			immediate_command_list->flush_and_wait(queue->_orig);
			break;
		}
	}
}

void reshade::d3d12::device_impl::get_resource_from_view(api::resource_view view, api::resource *out_resource) const
{
	assert(view.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	if (const auto it = _views.find(view.handle); it != _views.end())
		*out_resource = { reinterpret_cast<uintptr_t>(it->second) };
	else
		*out_resource = { 0 };
}

reshade::api::resource_desc reshade::d3d12::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);

	D3D12_HEAP_FLAGS heap_flags;
	D3D12_HEAP_PROPERTIES heap_props = {};
	reinterpret_cast<ID3D12Resource *>(resource.handle)->GetHeapProperties(&heap_props, &heap_flags);

	return convert_resource_desc(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc(), heap_props);
}

bool reshade::d3d12::device_impl::get_query_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(stride >= sizeof(uint64_t));

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(pool.handle);

	com_ptr<ID3D12Resource> readback_resource;
	UINT private_size = sizeof(ID3D12Resource *);
	if (SUCCEEDED(heap_object->GetPrivateData(pipeline_extra_data_guid, &private_size, &readback_resource)))
	{
		const D3D12_RANGE read_range = { first * sizeof(uint64_t), (first + count) * sizeof(uint64_t) };
		const D3D12_RANGE write_range = { 0, 0 };

		void *mapped_data = nullptr;
		if (SUCCEEDED(readback_resource->Map(0, &read_range, &mapped_data)))
		{
			for (UINT i = 0; i < count; ++i)
			{
				*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(results) + i * stride) = static_cast<uint64_t *>(mapped_data)[i + first];
			}

			readback_resource->Unmap(0, &write_range);

			return true;
		}
	}

	return false;
}

void reshade::d3d12::device_impl::wait_idle() const
{
	for (command_queue_impl *const queue : _queues)
		queue->wait_idle();
}

void reshade::d3d12::device_impl::set_debug_name(api::resource resource, const char *name)
{
	const size_t debug_name_len = strlen(name);
	std::wstring debug_name_wide;
	debug_name_wide.reserve(debug_name_len + 1);
	utf8::unchecked::utf8to16(name, name + debug_name_len, std::back_inserter(debug_name_wide));

	reinterpret_cast<ID3D12Resource *>(resource.handle)->SetName(debug_name_wide.c_str());
}
