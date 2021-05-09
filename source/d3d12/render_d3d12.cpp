/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
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
		UINT total_size;
		D3D12_ROOT_PARAMETER param;
		std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	};

	const GUID pipeline_extra_data_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };
}

reshade::d3d12::device_impl::device_impl(ID3D12Device *device) :
	api_object_impl(device)
{
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		_descriptor_handle_size[type] = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));

		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type);
		heap_desc.NumDescriptors = 32;
		_orig->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_resource_view_pool[type]));
		_resource_view_pool_state[type].resize(heap_desc.NumDescriptors);
	}

#if RESHADE_ADDON
	addon::load_addons();
#endif

	invoke_addon_event<addon_event::init_device>(this);
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

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
	case api::device_caps::draw_or_dispatch_indirect:
	case api::device_caps::fill_mode_non_solid:
	case api::device_caps::multi_viewport:
	case api::device_caps::sampler_anisotropy:
		return true;
	case api::device_caps::push_descriptors:
		return false;
	case api::device_caps::descriptor_tables:
		return true;
	case api::device_caps::sampler_with_resource_view:
	case api::device_caps::blit:
	case api::device_caps::resolve_region:
		return false;
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
		return true;
	default:
		return false;
	}
}
bool reshade::d3d12::device_impl::check_format_support(api::format format, api::resource_usage usage) const
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT feature = { static_cast<DXGI_FORMAT>(format) };
	if (FAILED(_orig->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &feature, sizeof(feature))))
		return false;

	if ((usage & api::resource_usage::render_target) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::depth_stencil) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) == 0)
		return false;
	if ((usage & (api::resource_usage::resolve_dest | api::resource_usage::resolve_source)) != 0 &&
		(feature.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d12::device_impl::check_resource_handle_valid(api::resource resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(resource.handle));
}
bool reshade::d3d12::device_impl::check_resource_view_handle_valid(api::resource_view view) const
{
	const std::lock_guard<std::mutex> lock(_mutex);
	return _views.find(view.handle) != _views.end();
}

bool reshade::d3d12::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out)
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	if (descriptor_handle.ptr == 0)
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

	assert((desc.usage & initial_state) == initial_state || initial_state == api::resource_usage::host);

	D3D12_RESOURCE_DESC internal_desc = {};
	D3D12_HEAP_PROPERTIES heap_props = {};
	convert_resource_desc(desc, internal_desc, heap_props);
	if (desc.type == api::resource_type::buffer)
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	if (com_ptr<ID3D12Resource> object;
		SUCCEEDED(_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &internal_desc, convert_resource_usage_to_states(initial_state), nullptr, IID_PPV_ARGS(&object))))
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
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			if (descriptor_handle.ptr == 0)
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
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			if (descriptor_handle.ptr == 0)
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
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (descriptor_handle.ptr == 0)
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
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			if (descriptor_handle.ptr == 0)
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
		convert_blend_factor(desc.graphics.blend_state.src_color_blend_factor[i], internal_desc.BlendState.RenderTarget[i].SrcBlend);
		convert_blend_factor(desc.graphics.blend_state.dst_color_blend_factor[i], internal_desc.BlendState.RenderTarget[i].DestBlend);
		convert_blend_op(desc.graphics.blend_state.color_blend_op[i], internal_desc.BlendState.RenderTarget[i].BlendOp);
		convert_blend_factor(desc.graphics.blend_state.src_alpha_blend_factor[i], internal_desc.BlendState.RenderTarget[i].SrcBlendAlpha);
		convert_blend_factor(desc.graphics.blend_state.dst_alpha_blend_factor[i], internal_desc.BlendState.RenderTarget[i].DestBlendAlpha);
		convert_blend_op(desc.graphics.blend_state.alpha_blend_op[i], internal_desc.BlendState.RenderTarget[i].BlendOpAlpha);
		internal_desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
		internal_desc.BlendState.RenderTarget[i].RenderTargetWriteMask = desc.graphics.blend_state.render_target_write_mask[i];
	}

	internal_desc.SampleMask = desc.graphics.multisample_state.sample_mask;

	convert_fill_mode(desc.graphics.rasterizer_state.fill_mode, internal_desc.RasterizerState.FillMode);
	convert_cull_mode(desc.graphics.rasterizer_state.cull_mode, internal_desc.RasterizerState.CullMode);
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
	convert_compare_op(desc.graphics.depth_stencil_state.depth_func, internal_desc.DepthStencilState.DepthFunc);
	internal_desc.DepthStencilState.StencilEnable = desc.graphics.depth_stencil_state.stencil_test;
	internal_desc.DepthStencilState.StencilReadMask = desc.graphics.depth_stencil_state.stencil_read_mask;
	internal_desc.DepthStencilState.StencilWriteMask = desc.graphics.depth_stencil_state.stencil_write_mask;
	convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_fail_op, internal_desc.DepthStencilState.BackFace.StencilFailOp);
	convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_depth_fail_op, internal_desc.DepthStencilState.BackFace.StencilDepthFailOp);
	convert_stencil_op(desc.graphics.depth_stencil_state.back_stencil_pass_op, internal_desc.DepthStencilState.BackFace.StencilPassOp);
	convert_compare_op(desc.graphics.depth_stencil_state.back_stencil_func, internal_desc.DepthStencilState.BackFace.StencilFunc);
	convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_fail_op, internal_desc.DepthStencilState.FrontFace.StencilFailOp);
	convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_depth_fail_op, internal_desc.DepthStencilState.FrontFace.StencilDepthFailOp);
	convert_stencil_op(desc.graphics.depth_stencil_state.front_stencil_pass_op, internal_desc.DepthStencilState.FrontFace.StencilPassOp);
	convert_compare_op(desc.graphics.depth_stencil_state.front_stencil_func, internal_desc.DepthStencilState.FrontFace.StencilFunc);

	std::vector<D3D12_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);
	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const auto &element = desc.graphics.input_layout[i];
		D3D12_INPUT_ELEMENT_DESC &internal_element = internal_elements.emplace_back();

		internal_element.SemanticName = element.semantic;
		internal_element.SemanticIndex = element.semantic_index;
		internal_element.Format = static_cast<DXGI_FORMAT>(element.format);
		internal_element.InputSlot = element.buffer_binding;
		internal_element.AlignedByteOffset = element.offset;
		internal_element.InputSlotClass = element.instance_step_rate > 0 ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		internal_element.InstanceDataStepRate = element.instance_step_rate;
	}

	internal_desc.InputLayout.NumElements = static_cast<UINT>(internal_elements.size());
	internal_desc.InputLayout.pInputElementDescs = internal_elements.data();

	switch (desc.graphics.rasterizer_state.topology)
	{
	default:
	case api::primitive_topology::undefined:
		internal_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		break;
	case api::primitive_topology::point_list:
		internal_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		break;
	case api::primitive_topology::line_list:
	case api::primitive_topology::line_strip:
	case api::primitive_topology::line_list_adj:
	case api::primitive_topology::line_strip_adj:
		internal_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case api::primitive_topology::triangle_list:
	case api::primitive_topology::triangle_strip:
	case api::primitive_topology::triangle_fan:
	case api::primitive_topology::triangle_list_adj:
	case api::primitive_topology::triangle_strip_adj:
		internal_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case api::primitive_topology::patch_list_01_cp:
	case api::primitive_topology::patch_list_02_cp:
	case api::primitive_topology::patch_list_03_cp:
	case api::primitive_topology::patch_list_04_cp:
	case api::primitive_topology::patch_list_05_cp:
	case api::primitive_topology::patch_list_06_cp:
	case api::primitive_topology::patch_list_07_cp:
	case api::primitive_topology::patch_list_08_cp:
	case api::primitive_topology::patch_list_09_cp:
	case api::primitive_topology::patch_list_10_cp:
	case api::primitive_topology::patch_list_11_cp:
	case api::primitive_topology::patch_list_12_cp:
	case api::primitive_topology::patch_list_13_cp:
	case api::primitive_topology::patch_list_14_cp:
	case api::primitive_topology::patch_list_15_cp:
	case api::primitive_topology::patch_list_16_cp:
	case api::primitive_topology::patch_list_17_cp:
	case api::primitive_topology::patch_list_18_cp:
	case api::primitive_topology::patch_list_19_cp:
	case api::primitive_topology::patch_list_20_cp:
	case api::primitive_topology::patch_list_21_cp:
	case api::primitive_topology::patch_list_22_cp:
	case api::primitive_topology::patch_list_23_cp:
	case api::primitive_topology::patch_list_24_cp:
	case api::primitive_topology::patch_list_25_cp:
	case api::primitive_topology::patch_list_26_cp:
	case api::primitive_topology::patch_list_27_cp:
	case api::primitive_topology::patch_list_28_cp:
	case api::primitive_topology::patch_list_29_cp:
	case api::primitive_topology::patch_list_30_cp:
	case api::primitive_topology::patch_list_31_cp:
	case api::primitive_topology::patch_list_32_cp:
		internal_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		break;
	}

	internal_desc.NumRenderTargets = desc.graphics.blend_state.num_render_targets;
	for (UINT i = 0; i < 8; ++i)
		internal_desc.RTVFormats[i] = static_cast<DXGI_FORMAT>(desc.graphics.blend_state.render_target_format[i]);
	internal_desc.DSVFormat = static_cast<DXGI_FORMAT>(desc.graphics.depth_stencil_state.depth_stencil_format);

	internal_desc.SampleDesc.Count = desc.graphics.multisample_state.sample_count;

	if (com_ptr<ID3D12PipelineState> pipeline;
		SUCCEEDED(_orig->CreateGraphicsPipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
	{
		pipeline_graphics_impl extra_data;
		extra_data.topology = static_cast<D3D12_PRIMITIVE_TOPOLOGY>(desc.graphics.rasterizer_state.topology);

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
bool reshade::d3d12::device_impl::create_pipeline_layout(uint32_t num_table_layouts, const api::descriptor_table_layout *table_layouts, uint32_t num_constant_ranges, const api::constant_range *constant_ranges, api::pipeline_layout *out)
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
bool reshade::d3d12::device_impl::create_descriptor_heap(uint32_t, uint32_t num_sizes, const api::descriptor_heap_size *sizes, api::descriptor_heap *out)
{
	D3D12_DESCRIPTOR_HEAP_TYPE prev_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	D3D12_DESCRIPTOR_HEAP_DESC internal_desc = {};
	internal_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	for (UINT i = 0; i < num_sizes; ++i)
	{
		switch (sizes[i].type)
		{
		case api::descriptor_type::sampler:
			internal_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			break;
		case api::descriptor_type::constant_buffer:
		case api::descriptor_type::shader_resource_view:
		case api::descriptor_type::unordered_access_view:
			internal_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			break;
		}

		// Cannot mix sampler and resource views in a descriptor heap
		if (prev_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES && prev_type != internal_desc.Type)
		{
			*out = { 0 };
			return false;
		}

		prev_type = internal_desc.Type;
		internal_desc.NumDescriptors += sizes[i].count;
	}

	if (com_ptr<ID3D12DescriptorHeap> object;
		SUCCEEDED(_orig->CreateDescriptorHeap(&internal_desc, IID_PPV_ARGS(&object))))
	{
		*out = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_descriptor_tables(api::descriptor_heap heap, api::descriptor_table_layout layout, uint32_t count, api::descriptor_table *out)
{
	const auto heap_object = reinterpret_cast<ID3D12DescriptorHeap *>(heap.handle);

	const UINT size = reinterpret_cast<const descriptor_table_layout_impl *>(layout.handle)->total_size;
	const UINT offset = _descriptor_heap_offset[heap_object];
	const UINT increment = _descriptor_handle_size[heap_object->GetDesc().Type];
	_descriptor_heap_offset[heap_object] += size * count;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = heap_object->GetCPUDescriptorHandleForHeapStart();
	cpu_handle.ptr += offset * increment;
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = heap_object->GetGPUDescriptorHandleForHeapStart();
	gpu_handle.ptr += offset * increment;

	for (UINT i = 0; i < count; ++i)
	{
		_descriptor_table_map[gpu_handle.ptr] = cpu_handle;
		out[i] = { gpu_handle.ptr };

		cpu_handle.ptr += size * increment;
		gpu_handle.ptr += size * increment;
	}

	return true;
}
bool reshade::d3d12::device_impl::create_descriptor_table_layout(uint32_t num_ranges, const api::descriptor_range *ranges, bool push_descriptors, api::descriptor_table_layout *out)
{
	if (push_descriptors)
	{
		*out = { 0 };
		return false;
	}

	uint32_t visibility_mask = 0;

	const auto result = new descriptor_table_layout_impl();
	result->total_size = 0;
	result->ranges.resize(num_ranges);

	for (UINT i = 0; i < num_ranges; ++i)
	{
		D3D12_DESCRIPTOR_RANGE &range = result->ranges[i];
		switch (ranges[i].type)
		{
		case api::descriptor_type::constant_buffer:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			break;
		case api::descriptor_type::shader_resource_view:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			break;
		case api::descriptor_type::unordered_access_view:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			break;
		case api::descriptor_type::sampler:
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			break;
		}

		range.NumDescriptors = ranges[i].count;
		range.BaseShaderRegister = ranges[i].dx_shader_register;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = ranges[i].binding;

		visibility_mask |= static_cast<uint32_t>(ranges[i].visibility);

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

void reshade::d3d12::device_impl::destroy_sampler(api::sampler handle)
{
	assert(handle.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);

	const D3D12_CPU_DESCRIPTOR_HANDLE heap_start = _resource_view_pool[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->GetCPUDescriptorHandleForHeapStart();
	if (handle.handle >= heap_start.ptr && handle.handle < (heap_start.ptr + _resource_view_pool_state[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].size() * _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]))
	{
		const size_t index = static_cast<size_t>(handle.handle - heap_start.ptr) / _descriptor_handle_size[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
		_resource_view_pool_state[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER][index] = false;
	}
}
void reshade::d3d12::device_impl::destroy_resource(api::resource handle)
{
	assert(handle.handle != 0);
	reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_resource_view(api::resource_view handle)
{
	assert(handle.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	_views.erase(handle.handle);

	// Mark free slot in the descriptor heap
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE heap_start = _resource_view_pool[type]->GetCPUDescriptorHandleForHeapStart();
		if (handle.handle >= heap_start.ptr && handle.handle < (heap_start.ptr + _resource_view_pool_state[type].size() * _descriptor_handle_size[type]))
		{
			const size_t index = static_cast<size_t>(handle.handle - heap_start.ptr) / _descriptor_handle_size[type];
			_resource_view_pool_state[type][index] = false;
			break;
		}
	}
}

void reshade::d3d12::device_impl::destroy_pipeline(api::pipeline_type, api::pipeline handle)
{
	assert(handle.handle != 0);
	reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_shader_module(api::shader_module handle)
{
	delete reinterpret_cast<shader_module_impl *>(handle.handle);
}
void reshade::d3d12::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	assert(handle.handle != 0);
	reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_descriptor_heap(api::descriptor_heap handle)
{
	assert(handle.handle != 0);
	reinterpret_cast<IUnknown *>(handle.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_descriptor_table_layout(api::descriptor_table_layout handle)
{
	delete reinterpret_cast<descriptor_table_layout_impl *>(handle.handle);
}

void reshade::d3d12::device_impl::update_descriptor_tables(uint32_t num_updates, const api::descriptor_update *updates)
{
	for (UINT i = 0; i < num_updates; ++i)
	{
		const auto heap_type = convert_descriptor_type_to_heap_type(updates[i].type);

		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_start = _descriptor_table_map.at(updates[i].table.handle);
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
				src_range_start.ptr = updates[i].descriptor.view.handle;
				break;
			case api::descriptor_type::sampler:
				src_range_start.ptr = updates[i].descriptor.sampler.handle;
				break;
			}

			_orig->CopyDescriptorsSimple(1, dst_range_start, src_range_start, heap_type);
		}
	}
}

bool reshade::d3d12::device_impl::map_resource(api::resource resource, uint32_t subresource, api::map_access, void **mapped_ptr)
{
	assert(resource.handle != 0);
	return SUCCEEDED(reinterpret_cast<ID3D12Resource *>(resource.handle)->Map(subresource, nullptr, mapped_ptr));
}
void reshade::d3d12::device_impl::unmap_resource(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D12Resource *>(resource.handle)->Unmap(subresource, nullptr);
}

void reshade::d3d12::device_impl::upload_buffer_region(api::resource dst, uint64_t dst_offset, const void *data, uint64_t size)
{
	assert(false); // TODO
}
void reshade::d3d12::device_impl::upload_texture_region(api::resource dst, uint32_t dst_subresource, const int32_t dst_box[6], const void *data, uint32_t row_pitch, uint32_t depth_pitch)
{
	assert(false); // TODO
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

void reshade::d3d12::device_impl::wait_idle() const
{
	com_ptr<ID3D12Fence> fence;
	if (FAILED(_orig->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		return;

	const HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr)
		return;

	const std::lock_guard<std::mutex> lock(_mutex);

	UINT64 signal_value = 1;
	for (ID3D12CommandQueue *const queue : _queues)
	{
		queue->Signal(fence.get(), signal_value);
		fence->SetEventOnCompletion(signal_value, fence_event);
		WaitForSingleObject(fence_event, INFINITE);
		signal_value++;
	}

	CloseHandle(fence_event);
}

void reshade::d3d12::device_impl::set_debug_name(api::resource resource, const char *name)
{
	const size_t debug_name_len = strlen(name);
	std::wstring debug_name_wide;
	debug_name_wide.reserve(debug_name_len + 1);
	utf8::unchecked::utf8to16(name, name + debug_name_len, std::back_inserter(debug_name_wide));

	reinterpret_cast<ID3D12Resource *>(resource.handle)->SetName(debug_name_wide.c_str());
}

D3D12_CPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::allocate_descriptor_handle(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	std::vector<bool> &state = _resource_view_pool_state[type];
	// Find free entry in the descriptor heap
	const std::lock_guard<std::mutex> lock(_mutex);
	if (const auto it = std::find(state.begin(), state.end(), false);
		it != state.end() && _resource_view_pool[type] != nullptr)
	{
		const size_t index = it - state.begin();
		state[index] = true; // Mark this entry as being in use

		D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = _resource_view_pool[type]->GetCPUDescriptorHandleForHeapStart();
		descriptor_handle.ptr += index * _descriptor_handle_size[type];
		return descriptor_handle;
	}
	else
	{
		return D3D12_CPU_DESCRIPTOR_HANDLE { 0 };
	}
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

void reshade::d3d12::command_list_impl::bind_pipeline(api::pipeline_type, api::pipeline pipeline)
{
	assert(pipeline.handle != 0);
	_orig->SetPipelineState(reinterpret_cast<ID3D12PipelineState *>(pipeline.handle));

	pipeline_graphics_impl extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(reinterpret_cast<ID3D12PipelineState *>(pipeline.handle)->GetPrivateData(pipeline_extra_data_guid, &extra_data_size, &extra_data)))
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
		default:
			assert(false);
			break;
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
			_orig->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(values[i]));
			break;
		}
	}
}

void reshade::d3d12::command_list_impl::push_constants(api::shader_stage stage, api::pipeline_layout layout, uint32_t layout_index, uint32_t first, uint32_t count, const uint32_t *values)
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
void reshade::d3d12::command_list_impl::push_descriptors(api::shader_stage, api::pipeline_layout, uint32_t, api::descriptor_type, uint32_t, uint32_t, const void *)
{
	assert(false);
}
void reshade::d3d12::command_list_impl::bind_descriptor_heaps(uint32_t count, const api::descriptor_heap *heaps)
{
#ifndef WIN64
	const auto heap_ptrs = static_cast<ID3D12DescriptorHeap **>(alloca(sizeof(ID3D12DescriptorHeap *) * count));
	for (uint32_t i = 0; i < count; ++i)
		heap_ptrs[i] = reinterpret_cast<ID3D12DescriptorHeap *>(heaps[i].handle);
#else
	const auto heap_ptrs = reinterpret_cast<ID3D12DescriptorHeap *const *>(heaps);
#endif

	_orig->SetDescriptorHeaps(count, heap_ptrs);
}
void reshade::d3d12::command_list_impl::bind_descriptor_tables(api::pipeline_type type, api::pipeline_layout layout, uint32_t first, uint32_t count, const api::descriptor_table *tables)
{
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
			_orig->SetComputeRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { tables[i].handle });
		else
			_orig->SetGraphicsRootDescriptorTable(i + first, D3D12_GPU_DESCRIPTOR_HANDLE { tables[i].handle });
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
void reshade::d3d12::command_list_impl::bind_render_targets_and_depth_stencil(uint32_t count, const api::resource_view *rtvs, api::resource_view dsv)
{
	if (count > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT)
	{
		assert(false);
		count = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}

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
void reshade::d3d12::command_list_impl::draw_or_dispatch_indirect(uint32_t type, api::resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	_has_commands = true;

	assert(false); // TODO
	// _orig->ExecuteIndirect(nullptr, draw_count, reinterpret_cast<ID3D12Resource *>(buffer.handle), offset, nullptr, 0);
}

void reshade::d3d12::command_list_impl::blit(api::resource, uint32_t, const int32_t[6], api::resource, uint32_t, const int32_t[6], api::texture_filter)
{
	assert(false);
}
void reshade::d3d12::command_list_impl::resolve(api::resource src, uint32_t src_subresource, const int32_t src_offset[3], api::resource dst, uint32_t dst_subresource, const int32_t dst_offset[3], const uint32_t size[3], uint32_t format)
{
	_has_commands = true;

	assert(src.handle != 0 && dst.handle != 0);
	assert(src_offset == nullptr && dst_offset == nullptr && size == nullptr);

	_orig->ResolveSubresource(
		reinterpret_cast<ID3D12Resource *>(dst.handle), dst_subresource,
		reinterpret_cast<ID3D12Resource *>(src.handle), src_subresource, static_cast<DXGI_FORMAT>(format));
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
	src_copy_location.PlacedFootprint.Footprint.Depth = 1;
	src_copy_location.PlacedFootprint.Footprint.RowPitch = src_copy_location.PlacedFootprint.Footprint.Width * dxgi_format_bpp(res_desc.Format);

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

	_orig->CopyTextureRegion(
		&dst_copy_location, 0, 0, 0,
		&src_copy_location, reinterpret_cast<const D3D12_BOX *>(src_box));
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

	assert(false); // TODO
	// _orig->ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE { 0 }, D3D12_CPU_DESCRIPTOR_HANDLE { uav.handle }, nullptr, values, 0, nullptr); // TODO
}
void reshade::d3d12::command_list_impl::clear_unordered_access_view_float(api::resource_view uav, const float values[4])
{
	_has_commands = true;

	assert(uav.handle != 0);

	assert(false); // TODO
	// _orig->ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE { 0 }, D3D12_CPU_DESCRIPTOR_HANDLE { uav.handle }, nullptr, values, 0, nullptr); // TODO
}

void reshade::d3d12::command_list_impl::transition_state(api::resource resource, api::resource_usage old_layout, api::resource_usage new_layout)
{
	_has_commands = true;

	assert(resource.handle != 0);

	D3D12_RESOURCE_BARRIER transition = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
	transition.Transition.pResource = reinterpret_cast<ID3D12Resource *>(resource.handle);
	transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transition.Transition.StateBefore = convert_resource_usage_to_states(old_layout);
	transition.Transition.StateAfter = convert_resource_usage_to_states(new_layout);

	_orig->ResourceBarrier(1, &transition);
}

reshade::d3d12::command_list_immediate_impl::command_list_immediate_impl(device_impl *device) :
	command_list_impl(device, nullptr)
{
	// Create multiple command allocators to buffer for multiple frames
	for (UINT i = 0; i < NUM_COMMAND_FRAMES; ++i)
	{
		if (FAILED(_device_impl->_orig->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence[i]))))
			return;
		if (FAILED(_device_impl->_orig->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmd_alloc[i]))))
			return;
	}

	// Create and open the command list for recording
	if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
		_orig->SetName(L"ReShade command list");

	// Create auto-reset event and fences for synchronization
	_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
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

	_current_root_signature[0] = nullptr;
	_current_root_signature[1] = nullptr;

	if (const HRESULT hr = _orig->Close(); FAILED(hr))
	{
		LOG(ERROR) << "Failed to close immediate command list! HRESULT is " << hr << '.';

		// A command list that failed to close can never be reset, so destroy it and create a new one
		_device_impl->wait_idle();
		_orig->Release(); _orig = nullptr;
		if (SUCCEEDED(_device_impl->_orig->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmd_alloc[_cmd_index].get(), nullptr, IID_PPV_ARGS(&_orig))))
			_orig->SetName(L"ReShade command list");
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
		_device_impl->_queues.push_back(_orig);
	}

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
		_immediate_cmd_list = new command_list_immediate_impl(device);

	invoke_addon_event<addon_event::init_command_queue>(this);
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
	invoke_addon_event<addon_event::destroy_command_queue>(this);

	delete _immediate_cmd_list;

	// Unregister queue from device
	{	const std::lock_guard<std::mutex> lock(_device_impl->_mutex);
		_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), _orig));
	}
}

void reshade::d3d12::command_queue_impl::flush_immediate_command_list() const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush(_orig);
}
