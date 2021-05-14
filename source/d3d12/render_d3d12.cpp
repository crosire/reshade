/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "render_d3d12.hpp"
#include "render_d3d12_utils.hpp"
#include "dxgi/format_utils.hpp"

namespace
{
	struct shader_module_impl
	{
		std::vector<uint8_t> bytecode;
	};

	struct pipeline_graphics_impl
	{
		D3D12_PRIMITIVE_TOPOLOGY topology;
	};

	struct descriptor_table_layout_impl
	{
		D3D12_DESCRIPTOR_HEAP_TYPE heap_type;
		UINT total_size;
		D3D12_ROOT_PARAMETER param;
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	};

	const GUID pipeline_extra_data_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };
}

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
#endif

	invoke_addon_event<addon_event::init_device>(this);
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

	// Do not call add-on events if initialization failed
	if (_mipmap_pipeline == nullptr)
		return;

	invoke_addon_event<addon_event::destroy_device>(this);

#if RESHADE_ADDON
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
	case api::device_caps::tessellation_shaders:
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
	case api::device_caps::sampler_anisotropy:
	case api::device_caps::partial_push_constant_updates:
		return true;
	case api::device_caps::partial_push_descriptor_updates:
		return false;
	case api::device_caps::descriptor_sets:
		return true;
	case api::device_caps::sampler_with_resource_view:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return false;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
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

bool reshade::d3d12::device_impl::check_resource_handle_valid(api::resource handle) const
{
	return handle.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(handle.handle));
}
bool reshade::d3d12::device_impl::check_resource_view_handle_valid(api::resource_view handle) const
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
	if (initial_data != nullptr)
		return false;

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
	default_clear_value.Format = make_dxgi_format_normal(internal_desc.Format);
	const bool use_default_clear_value = (desc.usage & (api::resource_usage::depth_stencil | api::resource_usage::render_target)) != api::resource_usage::undefined;

	if (com_ptr<ID3D12Resource> object;
		SUCCEEDED(_orig->CreateCommittedResource(&heap_props, heap_flags, &internal_desc, convert_resource_usage_to_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object))))
	{
		_resources.register_object(object.get());
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
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

bool reshade::d3d12::device_impl::create_shader_module(api::shader_stage, api::shader_format format, const char *entry_point, const void *code, size_t code_size, api::shader_module *out)
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
bool reshade::d3d12::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_set_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
{
	std::vector<D3D12_ROOT_PARAMETER> params(num_table_layouts + num_constant_ranges);
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> ranges(num_table_layouts);

	for (UINT i = 0; i < num_table_layouts; ++i)
	{
		params[i] = reinterpret_cast<descriptor_table_layout_impl *>(table_layouts[i].handle)->param;
	}

	for (UINT i = 0; i < num_constant_ranges; ++i)
	{
		D3D12_ROOT_PARAMETER &param = params[num_table_layouts + i];
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
	internal_desc.NumParameters = num_table_layouts + num_constant_ranges;
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
bool reshade::d3d12::device_impl::create_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, api::descriptor_set *out)
{
	const auto layout_impl = reinterpret_cast<const descriptor_table_layout_impl *>(layout.handle);

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
bool reshade::d3d12::device_impl::create_descriptor_set_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool, api::descriptor_set_layout *out)
{
	uint32_t visibility_mask = 0;

	const auto result = new descriptor_table_layout_impl();
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
void reshade::d3d12::device_impl::destroy_descriptor_sets(api::descriptor_set_layout layout, uint32_t count, const api::descriptor_set *sets)
{
	const auto layout_impl = reinterpret_cast<descriptor_table_layout_impl *>(layout.handle);

	for (UINT i = 0; i < count; ++i)
	{
		if (sets[i].handle == 0)
			continue;

		_gpu_view_heap.deallocate({ static_cast<SIZE_T>(sets[i].handle) }, layout_impl->total_size);
		_gpu_sampler_heap.deallocate({ static_cast<SIZE_T>(sets[i].handle) }, layout_impl->total_size);
	}
}
void reshade::d3d12::device_impl::destroy_descriptor_set_layout(api::descriptor_set_layout handle)
{
	delete reinterpret_cast<descriptor_table_layout_impl *>(handle.handle);
}

void reshade::d3d12::device_impl::destroy_query_pool(api::query_pool handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
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

bool reshade::d3d12::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access access, void **mapped_ptr)
{
	const D3D12_RANGE no_read_range = { 0, 0 };

	assert(resource.handle != 0);
	return SUCCEEDED(reinterpret_cast<ID3D12Resource *>(resource.handle)->Map(
		subresource, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read_range : nullptr, mapped_ptr));
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

	UINT row_length = 0;
	UINT slice_count = 1;
	UINT slice_height = 0;
	UINT64 upload_size = 0;
	UINT64 upload_row_pitch = 0;
	UINT64 upload_slice_pitch = 0;

	if (dst_box != nullptr)
	{
		row_length = dst_box[3] - dst_box[0];
		slice_count = dst_box[5] - dst_box[2];
		slice_height = dst_box[4] - dst_box[1];
		upload_row_pitch = row_length * dxgi_format_bpp(dst_desc.Format);
		upload_row_pitch = (upload_row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
		upload_slice_pitch = slice_height * upload_row_pitch;
		upload_size = slice_count * upload_slice_pitch;
	}
	else
	{
		_orig->GetCopyableFootprints(&dst_desc, dst_subresource, 1, 0, nullptr, &slice_height, &upload_row_pitch, &upload_size);
		row_length = static_cast<UINT>(dst_desc.Width);
		upload_row_pitch = (upload_row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
		upload_slice_pitch = upload_size;
	}

	// Allocate host memory for upload
	D3D12_RESOURCE_DESC intermediate_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	intermediate_desc.Width = upload_size;
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

	for (UINT z = 0; z < slice_count; ++z)
	{
		const auto dst_slice = mapped_data + z * upload_slice_pitch;
		const auto src_slice = static_cast<const uint8_t *>(data.data) + z * data.slice_pitch;

		for (UINT y = 0; y < slice_height; ++y)
		{
			const size_t row_size = data.row_pitch < upload_row_pitch ?
				data.row_pitch : static_cast<size_t>(upload_row_pitch);
			std::memcpy(
				dst_slice + y * upload_row_pitch,
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
			immediate_command_list->copy_buffer_to_texture(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, row_length, slice_height, dst, dst_subresource, dst_box);

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

reshade::d3d12::command_list_impl::command_list_impl(device_impl *device, ID3D12GraphicsCommandList *cmd_list) :
	api_object_impl(cmd_list), _device_impl(device), _has_commands(cmd_list != nullptr)
{
	if (_has_commands) // Do not call add-on event for immediate command list
		invoke_addon_event<addon_event::init_command_list>(this);
}
reshade::d3d12::command_list_impl::~command_list_impl()
{
	if (_has_commands)
		invoke_addon_event<addon_event::destroy_command_list>(this);
}

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	const auto pipeline_object = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);

	_orig->SetPipelineState(pipeline_object);

	pipeline_graphics_impl extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(pipeline_object->GetPrivateData(pipeline_extra_data_guid, &extra_data_size, &extra_data)))
	{
		_orig->IASetPrimitiveTopology(extra_data.topology);
	}
}
void reshade::d3d12::command_list_impl::bind_pipeline_states(uint32_t count, const api::pipeline_state *states, const uint32_t *values)
{
	for (UINT i = 0; i < count; ++i)
	{
		switch (states[i])
		{
		case api::pipeline_state::stencil_reference_value:
			_orig->OMSetStencilRef(values[i]);
			break;
		case api::pipeline_state::blend_constant:
		{
			float blend_factor[4];
			blend_factor[0] = ((values[i]      ) & 0xFF) / 255.0f;
			blend_factor[1] = ((values[i] >>  4) & 0xFF) / 255.0f;
			blend_factor[2] = ((values[i] >>  8) & 0xFF) / 255.0f;
			blend_factor[3] = ((values[i] >> 12) & 0xFF) / 255.0f;

			_orig->OMSetBlendFactor(blend_factor);
			break;
		}
		case api::pipeline_state::primitive_topology:
			_orig->IASetPrimitiveTopology(convert_primitive_topology(static_cast<api::primitive_topology>(values[i])));
			break;
		default:
			assert(false);
			break;
		}
	}
}
void reshade::d3d12::command_list_impl::bind_viewports(uint32_t first, uint32_t count, const float *viewports)
{
	assert(first == 0);

	_orig->RSSetViewports(count, reinterpret_cast<const D3D12_VIEWPORT *>(viewports));
}
void reshade::d3d12::command_list_impl::bind_scissor_rects(uint32_t first, uint32_t count, const int32_t *rects)
{
	assert(first == 0);

	_orig->RSSetScissorRects(count, reinterpret_cast<const D3D12_RECT *>(rects));
}

void reshade::d3d12::command_list_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const void *values)
{
	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (stage == api::shader_stage::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}

		_orig->SetComputeRoot32BitConstants(layout_index, count, values, first);
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}

		_orig->SetGraphicsRoot32BitConstants(layout_index, count, values, first);
	}
}
void reshade::d3d12::command_list_impl::push_descriptors(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, api::descriptor_type type, uint32_t first, uint32_t count, const void *descriptors)
{
	assert(first == 0);

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (type != api::descriptor_type::sampler ?
		!_device_impl->_gpu_view_heap.allocate_transient(first + count, base_handle, base_handle_gpu) :
		!_device_impl->_gpu_sampler_heap.allocate_transient(first + count, base_handle, base_handle_gpu))
		return;

	base_handle.ptr += first * _device_impl->_descriptor_handle_size[convert_descriptor_type_to_heap_type(type)];

	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (stage == api::shader_stage::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

#ifndef WIN64
	const UINT src_range_size = 1;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(count);
	switch (type)
	{
	case api::descriptor_type::sampler:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::sampler *>(descriptors)[i].handle) };
		}
		break;
	case api::descriptor_type::shader_resource_view:
	case api::descriptor_type::unordered_access_view:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::resource_view *>(descriptors)[i].handle) };
		}
		break;
	case api::descriptor_type::constant_buffer:
		for (UINT i = 0; i < count; ++i)
		{
			src_handles[i] = { static_cast<SIZE_T>(static_cast<const api::resource *>(descriptors)[i].handle) };
		}
		break;
	}

	_device_impl->_orig->CopyDescriptors(1, &base_handle, &count, count, src_handles.data(), &src_range_size, convert_descriptor_type_to_heap_type(type));
#else
	std::vector<UINT> src_range_sizes(count, 1);
	_device_impl->_orig->CopyDescriptors(1, &base_handle, &count, count, static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(descriptors), src_range_sizes.data(), convert_descriptor_type_to_heap_type(type));
#endif

	if (stage == api::shader_stage::compute)
		_orig->SetComputeRootDescriptorTable(layout_index, base_handle_gpu);
	else
		_orig->SetGraphicsRootDescriptorTable(layout_index, base_handle_gpu);
}
void reshade::d3d12::command_list_impl::bind_descriptor_sets(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_set *sets)
{
	if (_current_descriptor_heaps[0] != _device_impl->_gpu_sampler_heap.get() ||
		_current_descriptor_heaps[1] != _device_impl->_gpu_view_heap.get())
	{
		ID3D12DescriptorHeap *const heaps[2] = { _device_impl->_gpu_sampler_heap.get(), _device_impl->_gpu_view_heap.get() };
		_orig->SetDescriptorHeaps(2, heaps);

		_current_descriptor_heaps[0] = heaps[0];
		_current_descriptor_heaps[1] = heaps[1];
	}

	const auto root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	if (type == api::pipeline_type::compute)
	{
		if (_current_root_signature[1] != root_signature)
		{
			_current_root_signature[1]  = root_signature;
			_orig->SetComputeRootSignature(root_signature);
		}
	}
	else
	{
		if (_current_root_signature[0] != root_signature)
		{
			_current_root_signature[0]  = root_signature;
			_orig->SetGraphicsRootSignature(root_signature);
		}
	}

	for (uint32_t i = 0; i < count; ++i)
	{
		if (type == api::pipeline_type::compute)
			_orig->SetComputeRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
		else
			_orig->SetGraphicsRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { sets[i].handle });
	}
}

void reshade::d3d12::command_list_impl::bind_index_buffer(api::resource buffer, uint64_t offset, uint32_t index_size)
{
	if (buffer.handle != 0)
	{
		assert(index_size == 2 || index_size == 4);

		const auto buffer_ptr = reinterpret_cast<ID3D12Resource *>(buffer.handle);

		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = buffer_ptr->GetGPUVirtualAddress() + offset;
		view.Format = index_size == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<UINT>(buffer_ptr->GetDesc().Width - offset);

		_orig->IASetIndexBuffer(&view);
	}
	else
	{
		_orig->IASetIndexBuffer(nullptr);
	}
}
void reshade::d3d12::command_list_impl::bind_vertex_buffers(uint32_t first, uint32_t count, const api::resource *buffers, const uint64_t *offsets, const uint32_t *strides)
{
	const auto views = static_cast<D3D12_VERTEX_BUFFER_VIEW *>(alloca(sizeof(D3D12_VERTEX_BUFFER_VIEW) * count));
	for (UINT i = 0; i < count; ++i)
	{
		const auto buffer_ptr = reinterpret_cast<ID3D12Resource *>(buffers[i].handle);

		views[i].BufferLocation = buffer_ptr->GetGPUVirtualAddress() + offsets[i];
		views[i].SizeInBytes = static_cast<UINT>(buffer_ptr->GetDesc().Width - offsets[i]);
		views[i].StrideInBytes = strides[i];
	}

	_orig->IASetVertexBuffers(first, count, views);
}

void reshade::d3d12::command_list_impl::draw(uint32_t vertices, uint32_t instances, uint32_t first_vertex, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawInstanced(vertices, instances, first_vertex, first_instance);
}
void reshade::d3d12::command_list_impl::draw_indexed(uint32_t indices, uint32_t instances, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	_has_commands = true;

	_orig->DrawIndexedInstanced(indices, instances, first_index, vertex_offset, first_instance);
}
void reshade::d3d12::command_list_impl::dispatch(uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z)
{
	_has_commands = true;

	_orig->Dispatch(num_groups_x, num_groups_y, num_groups_z);
}
void reshade::d3d12::command_list_impl::draw_or_dispatch_indirect(uint32_t, api::resource, uint64_t, uint32_t, uint32_t)
{
	assert(false);
}

void reshade::d3d12::command_list_impl::begin_render_pass(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		depth_stencil_desc.cpuDescriptor = { static_cast<SIZE_T>(dsv.handle) };
		depth_stencil_desc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
		depth_stencil_desc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

		D3D12_RENDER_PASS_RENDER_TARGET_DESC render_target_desc[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		for (UINT i = 0; i < count; ++i)
		{
			render_target_desc[i].cpuDescriptor = { static_cast<SIZE_T>(rtvs[i].handle) };
			render_target_desc[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
			render_target_desc[i].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
		}

		cmd_list4->BeginRenderPass(count, render_target_desc, dsv.handle != 0 ? &depth_stencil_desc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
	}
	else
	{
#ifndef WIN64
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_handles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
		for (UINT i = 0; i < count; ++i)
			rtv_handles[i] = { static_cast<SIZE_T>(rtvs[i].handle) };
#else
		const auto rtv_handles = reinterpret_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(rtvs);
#endif
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = { static_cast<SIZE_T>(dsv.handle) };

		_orig->OMSetRenderTargets(count, rtv_handles, FALSE, dsv.handle != 0 ? &dsv_handle : nullptr);
	}
}
void reshade::d3d12::command_list_impl::end_render_pass()
{
	com_ptr<ID3D12GraphicsCommandList4> cmd_list4;
	if (SUCCEEDED(_orig->QueryInterface(&cmd_list4)))
	{
		cmd_list4->EndRenderPass();
	}
}

void reshade::d3d12::command_list_impl::blit(api::resource, uint32_t, const int32_t[6], api::resource, uint32_t, const int32_t[6], api::texture_filter)
{
	assert(false);
}
void reshade::d3d12::command_list_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], api::format format)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset == nullptr && dst_offset == nullptr && size == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, convert_format(format));
}
void reshade::d3d12::command_list_impl::copy_resource(api::resource src, api::resource dst)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyResource(reinterpret_cast<ID3D12Resource *>(dst.handle), reinterpret_cast<ID3D12Resource *>(src.handle));
}
void reshade::d3d12::command_list_impl::copy_buffer_region(api::resource src, uint64_t src_offset, api::resource dst, uint64_t dst_offset, uint64_t size)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	_orig->CopyBufferRegion(reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset, reinterpret_cast<ID3D12Resource *>(src.handle), src_offset, size);
}
void reshade::d3d12::command_list_impl::copy_buffer_to_texture(api::resource src, uint64_t src_offset, uint32_t row_length, uint32_t slice_height, api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6])
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	const D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(dst.handle)->GetDesc();

	D3D12_BOX src_box = {};
	if (dst_box != nullptr)
	{
		src_box.right = src_box.left + (dst_box[3] - dst_box[0]);
		src_box.bottom = src_box.top + (dst_box[4] - dst_box[1]);
		src_box.back = src_box.front + (dst_box[5] - dst_box[2]);
	}
	else
	{
		src_box.right = src_box.left + std::max(1u, static_cast<UINT>(res_desc.Width) >> (dst_subresource % res_desc.MipLevels));
		src_box.bottom = src_box.top + std::max(1u, res_desc.Height >> (dst_subresource % res_desc.MipLevels));
		src_box.back = src_box.front + (res_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, static_cast<UINT>(res_desc.DepthOrArraySize) >> (dst_subresource % res_desc.MipLevels)) : 1u);
	}

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src_copy_location.PlacedFootprint.Offset = src_offset;
	src_copy_location.PlacedFootprint.Footprint.Format = res_desc.Format;
	src_copy_location.PlacedFootprint.Footprint.Width = row_length != 0 ? row_length : src_box.right - src_box.left;
	src_copy_location.PlacedFootprint.Footprint.Height = slice_height != 0 ? slice_height : src_box.bottom - src_box.top;
	src_copy_location.PlacedFootprint.Footprint.Depth = src_box.back - src_box.front;
	src_copy_location.PlacedFootprint.Footprint.RowPitch = src_copy_location.PlacedFootprint.Footprint.Width * dxgi_format_bpp(res_desc.Format);
	src_copy_location.PlacedFootprint.Footprint.RowPitch = (src_copy_location.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_box != nullptr ? dst_box[0] : 0, dst_box != nullptr ? dst_box[1] : 0, dst_box != nullptr ? dst_box[2] : 0,
		&src_copy_location, &src_box);
}
void reshade::d3d12::command_list_impl::copy_texture_region(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3])
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	const D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc();

	D3D12_BOX src_box = {};
	if (src_offset != nullptr)
		std::copy_n(src_offset, 3, &src_box.left);

	if (size != nullptr)
	{
		src_box.right = src_box.left + size[0];
		src_box.bottom = src_box.top + size[1];
		src_box.back = src_box.front + size[2];
	}
	else
	{
		src_box.right = src_box.left + std::max(1u, static_cast<UINT>(res_desc.Width) >> (src_subresource % res_desc.MipLevels));
		src_box.bottom = src_box.top + std::max(1u, res_desc.Height >> (src_subresource % res_desc.MipLevels));
		src_box.back = src_box.front + (res_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? std::max(1u, static_cast<UINT>(res_desc.DepthOrArraySize) >> (src_subresource % res_desc.MipLevels)) : 1u);
	}

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_copy_location.SubresourceIndex = dst_subresource;

	_orig->CopyTextureRegion(
		&dst_copy_location, dst_offset != nullptr ? dst_offset[0] : 0, dst_offset != nullptr ? dst_offset[1] : 0, dst_offset != nullptr ? dst_offset[2] : 0,
		&src_copy_location, &src_box);
}
void reshade::d3d12::command_list_impl::copy_texture_to_buffer(api::resource src, uint32_t src_subresource, const int32_t src_box[6], api::resource dst, uint64_t dst_offset, uint32_t row_length, uint32_t slice_height)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);

	const D3D12_RESOURCE_DESC res_desc = reinterpret_cast<ID3D12Resource *>(src.handle)->GetDesc();

	D3D12_TEXTURE_COPY_LOCATION src_copy_location;
	src_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(src.handle);
	src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_copy_location.SubresourceIndex = src_subresource;

	D3D12_TEXTURE_COPY_LOCATION dst_copy_location;
	dst_copy_location.pResource = reinterpret_cast<ID3D12Resource *>(dst.handle);
	dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst_copy_location.PlacedFootprint.Offset = dst_offset;
	dst_copy_location.PlacedFootprint.Footprint.Format = res_desc.Format;
	dst_copy_location.PlacedFootprint.Footprint.Width = row_length != 0 ? row_length : std::max(1u, static_cast<UINT>(res_desc.Width) >> (src_subresource % res_desc.MipLevels));
	dst_copy_location.PlacedFootprint.Footprint.Height = slice_height != 0 ? slice_height : std::max(1u, res_desc.Height >> (src_subresource % res_desc.MipLevels));
	dst_copy_location.PlacedFootprint.Footprint.Depth = 1;
	dst_copy_location.PlacedFootprint.Footprint.RowPitch = dst_copy_location.PlacedFootprint.Footprint.Width * dxgi_format_bpp(res_desc.Format);
	dst_copy_location.PlacedFootprint.Footprint.RowPitch = (dst_copy_location.PlacedFootprint.Footprint.RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

	_orig->CopyTextureRegion(
		&dst_copy_location, 0, 0, 0,
		&src_copy_location, reinterpret_cast<const D3D12_BOX *>(src_box));
}

void reshade::d3d12::command_list_impl::generate_mipmaps(api::resource_view srv)
{
	assert(srv.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(srv, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	const D3D12_RESOURCE_DESC desc = resource->GetDesc();

	D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(desc.MipLevels * 2, base_handle, base_handle_gpu))
		return;

	for (uint32_t level = 0; level < desc.MipLevels; ++level, base_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = make_dxgi_format_normal(desc.Format);
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Texture2D.MipLevels = 1;
		srv_desc.Texture2D.MostDetailedMip = level;
		srv_desc.Texture2D.PlaneSlice = 0;
		srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

		_device_impl->_orig->CreateShaderResourceView(resource, &srv_desc, base_handle);
	}
	for (uint32_t level = 1; level < desc.MipLevels; ++level, base_handle.ptr += _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV])
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		uav_desc.Format = make_dxgi_format_normal(desc.Format);
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uav_desc.Texture2D.MipSlice = level;
		uav_desc.Texture2D.PlaneSlice = 0;

		_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, &uav_desc, base_handle);
	}

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_orig->SetComputeRootSignature(_device_impl->_mipmap_signature.get());
	_orig->SetPipelineState(_device_impl->_mipmap_pipeline.get());

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = resource;
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	transition.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	_orig->ResourceBarrier(1, &transition);

	for (uint32_t level = 1; level < desc.MipLevels; ++level)
	{
		const uint32_t width = std::max(1u, static_cast<uint32_t>(desc.Width) >> level);
		const uint32_t height = std::max(1u, desc.Height >> level);

		const float dimensions[2] = { 1.0f / width, 1.0f / height };
		_orig->SetComputeRoot32BitConstants(0, 2, dimensions, 0);

		// Bind next higher mipmap level as input
		_orig->SetComputeRootDescriptorTable(1, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * (level - 1) });
		// There is no UAV for level 0, so substract one
		_orig->SetComputeRootDescriptorTable(2, { base_handle_gpu.ptr + _device_impl->_descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * (desc.MipLevels + level - 1) });

		_orig->Dispatch(std::max(1u, (width + 7) / 8), std::max(1u, (height + 7) / 8), 1);

		// Wait for all accesses to be finished, since the result will be the input for the next mipmap
		D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
		barrier.UAV.pResource = transition.Transition.pResource;
		_orig->ResourceBarrier(1, &barrier);
	}

	std::swap(transition.Transition.StateBefore, transition.Transition.StateAfter);
	_orig->ResourceBarrier(1, &transition);

	// Reset root signature and descriptor heaps
	_orig->SetComputeRootSignature(_current_root_signature[1]);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(api::resource_view dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	assert(dsv.handle != 0);

	_orig->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, 0, nullptr);
}
void reshade::d3d12::command_list_impl::clear_render_target_views(uint32_t count, const api::resource_view *rtvs, const float color[4])
{
	_has_commands = true;

	for (UINT i = 0; i < count; ++i)
	{
		assert(rtvs[i].handle != 0);

		_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtvs[i].handle) }, color, 0, nullptr);
	}
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_uint(api::resource_view uav, const uint32_t values[4])
{
	_has_commands = true;

	assert(uav.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(uav, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewUint(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { uav.handle }, resource, values, 0, nullptr);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4])
{
	_has_commands = true;

	assert(uav.handle != 0);
	api::resource resource_handle;
	_device_impl->get_resource_from_view(uav, &resource_handle);
	assert(resource_handle.handle != 0);

	const auto resource = reinterpret_cast<ID3D12Resource *>(resource_handle.handle);

	D3D12_CPU_DESCRIPTOR_HANDLE table_base;
	D3D12_GPU_DESCRIPTOR_HANDLE table_base_gpu;
	if (!_device_impl->_gpu_view_heap.allocate_transient(1, table_base, table_base_gpu))
		return;

	const auto view_heap = _device_impl->_gpu_view_heap.get();
	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap)
		_orig->SetDescriptorHeaps(1, &view_heap);

	_device_impl->_orig->CreateUnorderedAccessView(resource, nullptr, nullptr, table_base);
	_orig->ClearUnorderedAccessViewFloat(table_base_gpu, D3D12_CPU_DESCRIPTOR_HANDLE { uav.handle }, resource, values, 0, nullptr);

	if (_current_descriptor_heaps[0] != view_heap && _current_descriptor_heaps[1] != view_heap && _current_descriptor_heaps[0] != nullptr)
		_orig->SetDescriptorHeaps(_current_descriptor_heaps[1] != nullptr ? 2 : 1, _current_descriptor_heaps);
}

void reshade::d3d12::command_list_impl::begin_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	_orig->BeginQuery(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index);
}
void reshade::d3d12::command_list_impl::end_query(api::query_pool pool, api::query_type type, uint32_t index)
{
	_has_commands = true;

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(pool.handle);
	const auto d3d_query_type = convert_query_type(type);
	_orig->EndQuery(heap_object, d3d_query_type, index);

	com_ptr<ID3D12Resource> readback_resource;
	UINT private_size = sizeof(ID3D12Resource *);
	if (SUCCEEDED(heap_object->GetPrivateData(pipeline_extra_data_guid, &private_size, &readback_resource)))
	{
		_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(pool.handle), convert_query_type(type), index, 1, readback_resource.get(), index * sizeof(uint64_t));
	}
}
void reshade::d3d12::command_list_impl::copy_query_results(api::query_pool heap, api::query_type type, uint32_t first, uint32_t count, api::resource dst, uint64_t dst_offset, uint32_t stride)
{
	assert(stride == sizeof(uint64_t));

	_has_commands = true;

	_orig->ResolveQueryData(reinterpret_cast<ID3D12QueryHeap *>(heap.handle), convert_query_type(type), first, count, reinterpret_cast<ID3D12Resource *>(dst.handle), dst_offset);
}

void reshade::d3d12::command_list_impl::insert_barrier(uint32_t count, const api::resource *resources, const api::resource_usage *old_states, const api::resource_usage *new_states)
{
	if (count == 0)
		return;

	_has_commands = true;

	const auto barriers = static_cast<D3D12_RESOURCE_BARRIER *>(alloca(sizeof(D3D12_RESOURCE_BARRIER) * count));
	for (UINT i = 0; i < count; ++i)
	{
		if (old_states[i] == api::resource_usage::unordered_access && new_states[i] == api::resource_usage::unordered_access)
		{
			barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[i].UAV.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
		}
		else
		{
			barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[i].Transition.pResource = reinterpret_cast<ID3D12Resource *>(resources[i].handle);
			barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[i].Transition.StateBefore = convert_resource_usage_to_states(old_states[i]);
			barriers[i].Transition.StateAfter = convert_resource_usage_to_states(new_states[i]);
		}
	}

	_orig->ResourceBarrier(count, barriers);
}

static inline void encode_pix3blob(UINT64(&pix3blob)[64], const char *label, const float color[4])
{
	pix3blob[0] = (0x2ull /* PIXEvent_BeginEvent_NoArgs */ << 10);
	pix3blob[1] = 0xFF000000;
	if (color != nullptr)
		pix3blob[1] |= ((static_cast<DWORD>(color[0] * 255) & 0xFF) << 16) | ((static_cast<DWORD>(color[1] * 255) & 0xFF) << 8) | (static_cast<DWORD>(color[2] * 255) & 0xFF);
	pix3blob[2] = (8ull /* copyChunkSize */ << 55) | (1ull /* isANSI */ << 54);
	std::strncpy(reinterpret_cast<char *>(pix3blob + 3), label, sizeof(pix3blob) - (4 * sizeof(UINT64)));
	pix3blob[63] = 0;
}

void reshade::d3d12::command_list_impl::begin_debug_marker(const char *label, const float color[4])
{
#if 0
	// Metadata is WINPIX_EVENT_ANSI_VERSION
	_orig->BeginEvent(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	// Metadata is WINPIX_EVENT_PIX3BLOB_VERSION
	_orig->BeginEvent(2, pix3blob, sizeof(pix3blob));
#endif
}
void reshade::d3d12::command_list_impl::end_debug_marker()
{
	_orig->EndEvent();
}
void reshade::d3d12::command_list_impl::insert_debug_marker(const char *label, const float color[4])
{
#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}

reshade::d3d12::command_list_immediate_impl::command_list_immediate_impl(device_impl *device) :
	command_list_impl(device, nullptr)
{
	// Create multiple command allocators to buffer for multiple frames
	for (UINT i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		if (FAILED(_device_impl->_orig->CreateFence(_fence_value[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]))))
			return;
		if (FAILED(_device_impl->_orig->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc[i]))))
			return;
	}

	// Create auto-reset event for synchronization
	_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fence_event == nullptr)
		return;

	// Create and open the command list for recording
	if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
		_orig->SetName(L"ReShade immediate command list");
}
reshade::d3d12::command_list_immediate_impl::~command_list_immediate_impl()
{
	if (_orig != nullptr)
		_orig->Release();
	if (_fence_event != nullptr)
		CloseHandle(_fence_event);

	// Signal to 'command_list_impl' destructor that this is an immediate command list
	_has_commands = false;
}

bool reshade::d3d12::command_list_immediate_impl::flush(ID3D12CommandQueue *queue)
{
	if (!_has_commands)
		return true;
	_has_commands = false;

	_current_root_signature[0] = nullptr;
	_current_root_signature[1] = nullptr;
	_current_descriptor_heaps[0] = nullptr;
	_current_descriptor_heaps[1] = nullptr;

	assert(_orig != nullptr);

	if (const HRESULT hr = _orig->Close(); FAILED(hr))
	{
		LOG(ERROR) << "Failed to close immediate command list!" << " HRESULT is " << hr << '.';

		// A command list that failed to close can never be reset, so destroy it and create a new one
		_device_impl->wait_idle();
		_orig->Release(); _orig = nullptr;
		if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
			_orig->SetName(L"ReShade immediate command list");
		return false;
	}

	ID3D12CommandList *const cmd_lists[] = { _orig };
	queue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);

	if (const UINT64 sync_value = _fence_value[_cmd_index] + NUM_COMMAND_FRAMES;
		SUCCEEDED(queue->Signal(_fence[_cmd_index].get(), sync_value)))
		_fence_value[_cmd_index] = sync_value;

	// Continue with next command list now that the current one was submitted
	_cmd_index = (_cmd_index + 1) % NUM_COMMAND_FRAMES;

	// Make sure all commands for the next command allocator have finished executing before reseting it
	if (_fence[_cmd_index]->GetCompletedValue() < _fence_value[_cmd_index])
	{
		if (SUCCEEDED(_fence[_cmd_index]->SetEventOnCompletion(_fence_value[_cmd_index], _fence_event)))
			WaitForSingleObject(_fence_event, INFINITE); // Event is automatically reset after this wait is released
	}

	// Reset command allocator before using it this frame again
	_cmd_alloc[_cmd_index]->Reset();

	// Reset command list using current command allocator and put it into the recording state
	return SUCCEEDED(_orig->Reset(_cmd_alloc[_cmd_index].get(), nullptr));
}
bool reshade::d3d12::command_list_immediate_impl::flush_and_wait(ID3D12CommandQueue *queue)
{
	if (!_has_commands)
		return true;

	// Index is updated during flush below, so keep track of the current one to wait on
	const UINT cmd_index_to_wait_on = _cmd_index;

	if (!flush(queue))
		return false;

	if (FAILED(_fence[cmd_index_to_wait_on]->SetEventOnCompletion(_fence_value[cmd_index_to_wait_on], _fence_event)))
		return false;
	return WaitForSingleObject(_fence_event, INFINITE) == WAIT_OBJECT_0;
}

reshade::d3d12::command_queue_impl::command_queue_impl(device_impl *device, ID3D12CommandQueue *queue) :
	api_object_impl(queue), _device_impl(device)
{
	// Register queue to device
	{	const std::lock_guard<std::mutex> lock(_device_impl->_mutex);
		_device_impl->_queues.push_back(this);
	}

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device);
		// Ensure the immediate command list was initialized successfully, otherwise disable it
		if (_immediate_cmd_list->_orig == nullptr)
		{
			LOG(ERROR) << "Failed to create immediate command list for queue " << _orig << '!';

			delete _immediate_cmd_list;
			_immediate_cmd_list = nullptr;
		}
	}

	// Create auto-reset event and fence for wait for idle synchronization
	_wait_idle_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_wait_idle_fence_event == nullptr ||
		FAILED(_device_impl->_orig->CreateFence(_wait_idle_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_wait_idle_fence))))
	{
		LOG(ERROR) << "Failed to create wait for idle resources for queue " << _orig << '!';
	}

	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);

	if (_wait_idle_fence_event != nullptr)
		CloseHandle(_wait_idle_fence_event);

	delete _immediate_cmd_list;

	// Unregister queue from device
	{	const std::lock_guard<std::mutex> lock(_device_impl->_mutex);
		_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), this));
	}
}

void reshade::d3d12::command_queue_impl::flush_immediate_command_list() const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig);
}

void reshade::d3d12::command_queue_impl::wait_idle() const
{
	// Flush command list, to avoid it still referencing resources that may be destroyed after this call
	flush_immediate_command_list();

	assert(_wait_idle_fence != nullptr && _wait_idle_fence_event != nullptr);

	// Increment fence value to ensure it has not been signaled before
	if (const UINT64 sync_value = _wait_idle_fence_value + 1;
		SUCCEEDED(_orig->Signal(_wait_idle_fence.get(), sync_value)))
		_wait_idle_fence_value = sync_value;
	else
		return; // Cannot wait on fence if signaling was not successful

	if (SUCCEEDED(_wait_idle_fence->SetEventOnCompletion(_wait_idle_fence_value, _wait_idle_fence_event)))
		WaitForSingleObject(_wait_idle_fence_event, INFINITE);
}

void reshade::d3d12::command_queue_impl::begin_debug_marker(const char *label, const float color[4])
{
#if 0
	_orig->BeginEvent(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->BeginEvent(2, pix3blob, sizeof(pix3blob));
#endif
}
void reshade::d3d12::command_queue_impl::end_debug_marker()
{
	_orig->EndEvent();
}
void reshade::d3d12::command_queue_impl::insert_debug_marker(const char *label, const float color[4])
{
#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}
