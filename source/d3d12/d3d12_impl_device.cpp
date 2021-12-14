/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_queue.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include <malloc.h>

extern bool is_windows7();

const GUID reshade::d3d12::extra_data_guid = { 0xB2257A30, 0x4014, 0x46EA, { 0xBD, 0x88, 0xDE, 0xC2, 0x1D, 0xB6, 0xA0, 0x2B } };

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
	load_addons();

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

	unload_addons();
#endif
}

bool reshade::d3d12::device_impl::check_capability(api::device_caps capability) const
{
	switch (capability)
	{
	case api::device_caps::compute_shader:
	case api::device_caps::geometry_shader:
	case api::device_caps::hull_and_domain_shader:
		return true;
	case api::device_caps::logic_op:
		if (D3D12_FEATURE_DATA_D3D12_OPTIONS options;
			SUCCEEDED(_orig->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
			return options.OutputMergerLogicOp;
		return false;
	case api::device_caps::dual_source_blend:
	case api::device_caps::independent_blend:
	case api::device_caps::fill_mode_non_solid:
		return true;
	case api::device_caps::conservative_rasterization:
		if (D3D12_FEATURE_DATA_D3D12_OPTIONS options;
			SUCCEEDED(_orig->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
			return options.ConservativeRasterizationTier != D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED;
		return false;
	case api::device_caps::bind_render_targets_and_depth_stencil:
	case api::device_caps::multi_viewport:
	case api::device_caps::partial_push_constant_updates:
		return true;
	case api::device_caps::partial_push_descriptor_updates:
		return false;
	case api::device_caps::draw_instanced:
		return true;
	case api::device_caps::draw_or_dispatch_indirect:
		return false; // TODO: Not currently implemented (could do so with 'ID3D12GraphicsCommandList::ExecuteIndirect')
	case api::device_caps::copy_buffer_region:
	case api::device_caps::copy_buffer_to_texture:
		return true;
	case api::device_caps::blit:
		return false;
	case api::device_caps::resolve_region:
	case api::device_caps::copy_query_pool_results:
	case api::device_caps::sampler_compare:
	case api::device_caps::sampler_anisotropic:
		return true;
	case api::device_caps::sampler_with_resource_view:
		return false;
	case api::device_caps::shared_resource:
	case api::device_caps::shared_resource_nt_handle:
		return !is_windows7();
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

	if ((usage & api::resource_usage::depth_stencil) != 0 && (feature.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) == 0)
		return false;
	if ((usage & api::resource_usage::render_target) != 0 && (feature.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) == 0)
		return false;
	if ((usage & api::resource_usage::shader_resource) != 0 && (feature.Support1 & (D3D12_FORMAT_SUPPORT1_SHADER_LOAD | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) == 0)
		return false;
	if ((usage & api::resource_usage::unordered_access) != 0 && (feature.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) == 0)
		return false;

	if ((usage & (api::resource_usage::resolve_dest | api::resource_usage::resolve_source)) != 0 && (feature.Support1 & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE) == 0)
		return false;

	return true;
}

bool reshade::d3d12::device_impl::create_sampler(const api::sampler_desc &desc, api::sampler *out_handle)
{
	*out_handle = { 0 };

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
	if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].allocate(descriptor_handle))
		return false;

	D3D12_SAMPLER_DESC internal_desc = {};
	convert_sampler_desc(desc, internal_desc);

	_orig->CreateSampler(&internal_desc, descriptor_handle);

	*out_handle = { descriptor_handle.ptr };
	return true;
}
void reshade::d3d12::device_impl::destroy_sampler(api::sampler handle)
{
	if (handle.handle == 0)
		return;

	_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER].free({ static_cast<SIZE_T>(handle.handle) });
}

bool reshade::d3d12::device_impl::create_resource(const api::resource_desc &desc, const api::subresource_data *initial_data, api::resource_usage initial_state, api::resource *out_handle, HANDLE *shared_handle)
{
	*out_handle = { 0 };

	assert((desc.usage & initial_state) == initial_state || initial_state == api::resource_usage::general || initial_state == api::resource_usage::cpu_access);

	const bool is_shared = (desc.flags & api::resource_flags::shared) != 0;
	if (is_shared)
	{
		// Only NT handles are supported
		if (shared_handle == nullptr || (desc.flags & reshade::api::resource_flags::shared_nt_handle) == 0)
			return false;

		if (*shared_handle != nullptr)
		{
			assert(initial_data == nullptr);

			if (com_ptr<ID3D12Resource> object;
				SUCCEEDED(_orig->OpenSharedHandle(*shared_handle, IID_PPV_ARGS(&object))))
			{
				*out_handle = to_handle(object.get());
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_DESC internal_desc = {};
	D3D12_HEAP_PROPERTIES heap_props = {};
	convert_resource_desc(desc, internal_desc, heap_props, heap_flags);

	if (desc.type == api::resource_type::buffer)
	{
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// Constant buffer views need to be aligned to 256 bytes, so make buffer large enough to ensure that is possible
		if ((desc.usage & (api::resource_usage::constant_buffer)) != 0)
			internal_desc.Width = (internal_desc.Width + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u);
	}

	// Use a default clear value of transparent black (all zeroes)
	bool use_default_clear_value = true;
	D3D12_CLEAR_VALUE default_clear_value = {};
	if ((desc.usage & api::resource_usage::render_target) != 0)
		default_clear_value.Format = convert_format(api::format_to_default_typed(desc.texture.format));
	else if ((desc.usage & api::resource_usage::depth_stencil) != 0)
		default_clear_value.Format = convert_format(api::format_to_depth_stencil_typed(desc.texture.format));
	else
		use_default_clear_value = false;

	if (com_ptr<ID3D12Resource> object;
		SUCCEEDED(desc.heap == api::memory_heap::unknown ?
			_orig->CreateReservedResource(&internal_desc, convert_resource_usage_to_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object)) :
			_orig->CreateCommittedResource(&heap_props, heap_flags, &internal_desc, convert_resource_usage_to_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object))))
	{
		if (is_shared && FAILED(_orig->CreateSharedHandle(object.get(), nullptr, GENERIC_ALL, nullptr, shared_handle)))
			return false;

		register_resource(object.get());

		*out_handle = to_handle(object.release());

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
					immediate_command_list->barrier(1, out_handle, &states_upload[0], &states_upload[1]);

					if (desc.type == api::resource_type::buffer)
					{
						update_buffer_region(initial_data->data, *out_handle, 0, desc.buffer.size);
					}
					else
					{
						for (uint32_t subresource = 0; subresource < static_cast<uint32_t>(desc.texture.depth_or_layers) * desc.texture.levels; ++subresource)
							update_texture_region(initial_data[subresource], *out_handle, subresource, nullptr);
					}

					const api::resource_usage states_finalize[2] = { api::resource_usage::copy_dest, initial_state };
					immediate_command_list->barrier(1, out_handle, &states_finalize[0], &states_finalize[1]);

					queue->flush_immediate_command_list();
					break;
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_resource(api::resource handle)
{
	if (handle.handle == 0)
		return;

	unregister_resource(reinterpret_cast<ID3D12Resource *>(handle.handle));

	reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

reshade::api::resource_desc reshade::d3d12::device_impl::get_resource_desc(api::resource resource) const
{
	assert(resource.handle != 0);

	// This will retrieve the heap properties for placed and comitted resources, not for reserved resources (which will then be translated to 'memory_heap::unknown')
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	D3D12_HEAP_PROPERTIES heap_props = {};
	reinterpret_cast<ID3D12Resource *>(resource.handle)->GetHeapProperties(&heap_props, &heap_flags);

	return convert_resource_desc(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc(), heap_props, heap_flags);
}
void reshade::d3d12::device_impl::set_resource_name(api::resource handle, const char *name)
{
	const size_t debug_name_len = strlen(name);
	std::wstring debug_name_wide;
	debug_name_wide.reserve(debug_name_len + 1);
	utf8::unchecked::utf8to16(name, name + debug_name_len, std::back_inserter(debug_name_wide));

	reinterpret_cast<ID3D12Resource *>(handle.handle)->SetName(debug_name_wide.c_str());
}

bool reshade::d3d12::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_handle)
{
	*out_handle = { 0 };

	if (resource.handle == 0)
		return false;

	// Cannot create a resource view with a typeless format
	assert(desc.format != api::format_to_typeless(desc.format));

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

			register_resource_view(descriptor_handle, reinterpret_cast<ID3D12Resource *>(resource.handle), desc);
			*out_handle = to_handle(descriptor_handle);
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

			register_resource_view(descriptor_handle, reinterpret_cast<ID3D12Resource *>(resource.handle), desc);
			*out_handle = to_handle(descriptor_handle);
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

			register_resource_view(descriptor_handle, reinterpret_cast<ID3D12Resource *>(resource.handle), desc);
			*out_handle = to_handle(descriptor_handle);
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

			register_resource_view(descriptor_handle, reinterpret_cast<ID3D12Resource *>(resource.handle), desc);
			*out_handle = to_handle(descriptor_handle);
			return true;
		}
	}

	return false;
}
void reshade::d3d12::device_impl::destroy_resource_view(api::resource_view handle)
{
	if (handle.handle == 0)
		return;

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = { static_cast<SIZE_T>(handle.handle) };

	const std::unique_lock<std::shared_mutex> lock(_resource_mutex);
	_views.erase(descriptor_handle.ptr);

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		_view_heaps[i].free(descriptor_handle);
}

reshade::api::resource reshade::d3d12::device_impl::get_resource_from_view(api::resource_view view) const
{
	assert(view.handle != 0);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = { static_cast<SIZE_T>(view.handle) };

	const std::shared_lock<std::shared_mutex> lock(_resource_mutex);

	if (const auto it = _views.find(descriptor_handle.ptr); it != _views.end())
		return to_handle(it->second.first);
	else
		return assert(false), api::resource { 0 };
}
reshade::api::resource_view_desc reshade::d3d12::device_impl::get_resource_view_desc(api::resource_view view) const
{
	assert(view.handle != 0);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = { static_cast<SIZE_T>(view.handle) };

	const std::shared_lock<std::shared_mutex> lock(_resource_mutex);

	if (const auto it = _views.find(descriptor_handle.ptr); it != _views.end())
		return it->second.second;
	else
		return assert(false), api::resource_view_desc();
}
void reshade::d3d12::device_impl::set_resource_view_name(api::resource_view, const char *)
{
}

bool reshade::d3d12::device_impl::create_pipeline(const api::pipeline_desc &desc, uint32_t dynamic_state_count, const api::dynamic_state *dynamic_states, api::pipeline *out_handle)
{
	*out_handle = { 0 };

	for (uint32_t i = 0; i < dynamic_state_count; ++i)
		if (dynamic_states[i] != api::dynamic_state::stencil_reference_value &&
			dynamic_states[i] != api::dynamic_state::blend_constant &&
			dynamic_states[i] != api::dynamic_state::primitive_topology)
			return false;

	switch (desc.type)
	{
	case api::pipeline_stage::all_compute:
		assert(dynamic_state_count == 0);
		return create_compute_pipeline(desc, out_handle);
	case api::pipeline_stage::all_graphics:
		return create_graphics_pipeline(desc, out_handle);
	default:
		return false;
	}
}
bool reshade::d3d12::device_impl::create_compute_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC internal_desc = {};
	convert_pipeline_desc(desc, internal_desc);

	if (com_ptr<ID3D12PipelineState> pipeline;
		SUCCEEDED(_orig->CreateComputePipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
	{
		*out_handle = to_handle(pipeline.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_graphics_pipeline(const api::pipeline_desc &desc, api::pipeline *out_handle)
{
	if (desc.graphics.topology == api::primitive_topology::triangle_fan)
	{
		*out_handle = { 0 };
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC internal_desc = {};
	convert_pipeline_desc(desc, internal_desc);

	std::vector<D3D12_INPUT_ELEMENT_DESC> internal_elements;
	internal_elements.reserve(16);
	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		const api::input_element &element = desc.graphics.input_layout[i];

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

	if (com_ptr<ID3D12PipelineState> pipeline;
		SUCCEEDED(_orig->CreateGraphicsPipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
	{
		pipeline_graphics_impl extra_data;
		extra_data.topology = convert_primitive_topology(desc.graphics.topology);

		std::copy_n(desc.graphics.blend_state.blend_constant, 4, extra_data.blend_constant);

		pipeline->SetPrivateData(extra_data_guid, sizeof(extra_data), &extra_data);

		*out_handle = to_handle(pipeline.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_pipeline(api::pipeline handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d12::device_impl::create_pipeline_layout(uint32_t param_count, const api::pipeline_layout_param *params, api::pipeline_layout *out_handle)
{
	*out_handle = { 0 };

	std::vector<D3D12_ROOT_PARAMETER> internal_params(param_count);
	std::vector<std::vector<api::descriptor_range>> ranges(param_count);
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> internal_ranges(param_count);

	for (uint32_t i = 0; i < param_count; ++i)
	{
		api::shader_stage visibility_mask = static_cast<api::shader_stage>(0);

		if (params[i].type != api::pipeline_layout_param_type::push_constants)
		{
			const bool push_descriptors = params[i].type == api::pipeline_layout_param_type::push_descriptors;
			const uint32_t range_count = push_descriptors ? 1 : params[i].descriptor_set.count;
			const api::descriptor_range *const input_ranges = push_descriptors ? &params[i].push_descriptors : params[i].descriptor_set.ranges;

			if (range_count == 0 || input_ranges[0].count == 0)
			{
				// Dummy parameter (to prevent root signature creation from failing)
				internal_params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				internal_params[i].Constants.ShaderRegister = i;
				internal_params[i].Constants.RegisterSpace = 255;
				internal_params[i].Constants.Num32BitValues = 1;
				continue;
			}

			ranges[i].reserve(range_count);
			internal_ranges[i].reserve(range_count);

			D3D12_DESCRIPTOR_HEAP_TYPE prev_heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

			for (uint32_t k = 0; k < range_count; ++k)
			{
				const api::descriptor_range &range = input_ranges[k];

				assert(range.array_size <= 1);

				ranges[i].push_back(range);

				D3D12_DESCRIPTOR_RANGE &internal_range = internal_ranges[i].emplace_back();
				internal_range.RangeType = convert_descriptor_type(range.type);
				internal_range.NumDescriptors = range.count;
				internal_range.BaseShaderRegister = range.dx_register_index;
				internal_range.RegisterSpace = range.dx_register_space;
				internal_range.OffsetInDescriptorsFromTableStart = range.offset;

				visibility_mask |= range.visibility;

				// Cannot mix different descriptor heap types in a single descriptor table
				const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(range.type);
				if (prev_heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES && prev_heap_type != heap_type)
					return false;

				prev_heap_type = heap_type;
			}

			internal_params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			internal_params[i].DescriptorTable.NumDescriptorRanges = static_cast<uint32_t>(internal_ranges[i].size());
			internal_params[i].DescriptorTable.pDescriptorRanges = internal_ranges[i].data();
		}
		else
		{
			internal_params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			internal_params[i].Constants.ShaderRegister = params[i].push_constants.dx_register_index;
			internal_params[i].Constants.RegisterSpace = params[i].push_constants.dx_register_space;
			internal_params[i].Constants.Num32BitValues = params[i].push_constants.count;

			visibility_mask = params[i].push_constants.visibility;
		}

		switch (visibility_mask)
		{
		default:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			break;
		case api::shader_stage::vertex:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			break;
		case api::shader_stage::hull:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
			break;
		case api::shader_stage::domain:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
			break;
		case api::shader_stage::geometry:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
			break;
		case api::shader_stage::pixel:
			internal_params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			break;
		}
	}

	D3D12_ROOT_SIGNATURE_DESC internal_desc = {};
	internal_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	internal_desc.NumParameters = param_count;
	internal_desc.pParameters = internal_params.data();

	com_ptr<ID3DBlob> blob;
	com_ptr<ID3D12RootSignature> signature;
	if (SUCCEEDED(D3D12SerializeRootSignature(&internal_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr)) &&
		SUCCEEDED(_orig->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&signature))))
	{
		const auto impl = new pipeline_layout_impl();
		impl->params.assign(params, params + param_count);
		impl->ranges = std::move(ranges);

		for (uint32_t i = 0; i < param_count; ++i)
			if (params[i].type == api::pipeline_layout_param_type::descriptor_set)
				impl->params[i].descriptor_set.ranges = impl->ranges[i].data();

		signature->SetPrivateData(extra_data_guid, sizeof(impl), &impl);

		*out_handle = to_handle(signature.release());
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	if (handle.handle == 0)
		return;

	const auto signature = reinterpret_cast<ID3D12RootSignature *>(handle.handle);

	pipeline_layout_impl *impl = nullptr;
	UINT impl_size = sizeof(impl);
	signature->GetPrivateData(extra_data_guid, &impl_size, &impl);
	delete impl;

	signature->Release();
}

reshade::api::pipeline_layout_param reshade::d3d12::device_impl::get_pipeline_layout_param(api::pipeline_layout layout, uint32_t layout_param) const
{
	assert(layout.handle != 0);

	const auto signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);

	pipeline_layout_impl *impl = nullptr;
	UINT impl_size = sizeof(impl);
	if (SUCCEEDED(signature->GetPrivateData(extra_data_guid, &impl_size, &impl)))
		return impl->params[layout_param];
	else
		return api::pipeline_layout_param {};
}

bool reshade::d3d12::device_impl::create_descriptor_sets(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_set *out_sets)
{
	const api::pipeline_layout_param layout_param_desc = get_pipeline_layout_param(layout, layout_param);

	uint32_t total_count = 0;
	for (uint32_t k = 0; k < layout_param_desc.descriptor_set.count; ++k)
		total_count = std::max(total_count, layout_param_desc.descriptor_set.ranges[k].offset + layout_param_desc.descriptor_set.ranges[k].count);

	for (uint32_t i = 0; i < count; ++i)
	{
		if (total_count == 0)
		{
			out_sets[i] = { 0 };
			continue;
		}

		const auto heap_type = convert_descriptor_type_to_heap_type(layout_param_desc.descriptor_set.ranges[0].type);

		D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;

		if (heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			_gpu_view_heap.allocate_static(total_count, base_handle, base_handle_gpu);
		else
			_gpu_sampler_heap.allocate_static(total_count, base_handle, base_handle_gpu);

		{	const std::unique_lock<std::shared_mutex> lock(_heap_mutex);
			_sets.emplace(base_handle_gpu.ptr, total_count);
		}

		out_sets[i] = { base_handle_gpu.ptr };
	}

	return true;
}
void reshade::d3d12::device_impl::destroy_descriptor_sets(uint32_t count, const api::descriptor_set *sets)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (sets[i].handle == 0)
			continue;

		uint32_t total_count = 0;

		{	const std::unique_lock<std::shared_mutex> lock(_heap_mutex);
			total_count = _sets.at(sets[i].handle);
			_sets.erase(sets[i].handle);
		}

		_gpu_view_heap.free({ static_cast<SIZE_T>(sets[i].handle) }, total_count);
		_gpu_sampler_heap.free({ static_cast<SIZE_T>(sets[i].handle) }, total_count);
	}
}

void reshade::d3d12::device_impl::get_descriptor_pool_offset(api::descriptor_set set, api::descriptor_pool *pool, uint32_t *offset) const
{
	resolve_descriptor_handle(set, nullptr, pool, offset);
}

bool reshade::d3d12::device_impl::map_buffer_region(api::resource resource, uint64_t offset, uint64_t, api::map_access access, void **out_data)
{
	if (out_data == nullptr)
		return false;

	assert(resource.handle != 0);

	const D3D12_RANGE no_read = { 0, 0 };

	if (SUCCEEDED(reinterpret_cast<ID3D12Resource *>(resource.handle)->Map(
		0, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read : nullptr, out_data)))
	{
		*out_data = static_cast<uint8_t *>(*out_data) + offset;
		return true;
	}
	else
	{
		return false;
	}
}
void reshade::d3d12::device_impl::unmap_buffer_region(api::resource resource)
{
	assert(resource.handle != 0);

	reinterpret_cast<ID3D12Resource *>(resource.handle)->Unmap(0, nullptr);
}
bool reshade::d3d12::device_impl::map_texture_region(api::resource resource, uint32_t subresource, const api::subresource_box *box, api::map_access access, api::subresource_data *out_data)
{
	if (out_data == nullptr)
		return false;

	out_data->data = nullptr;
	out_data->row_pitch = 0;
	out_data->slice_pitch = 0;

	// Mapping a subset of a texture is not supported
	if (box != nullptr)
		return false;

	assert(resource.handle != 0);

	const D3D12_RANGE no_read = { 0, 0 };

	const D3D12_RESOURCE_DESC desc = reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
	_orig->GetCopyableFootprints(&desc, subresource, 1, 0, &layout, &out_data->slice_pitch, nullptr, nullptr);
	out_data->row_pitch = layout.Footprint.RowPitch;
	out_data->slice_pitch *= layout.Footprint.RowPitch;

	return SUCCEEDED(reinterpret_cast<ID3D12Resource *>(resource.handle)->Map(
		subresource, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read : nullptr, &out_data->data));
}
void reshade::d3d12::device_impl::unmap_texture_region(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);

	reinterpret_cast<ID3D12Resource *>(resource.handle)->Unmap(subresource, nullptr);
}

void reshade::d3d12::device_impl::update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size)
{
	assert(resource.handle != 0);

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

	assert(!_queues.empty());

	// Copy data from upload buffer into target texture using the first available immediate command list
	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
		{
			immediate_command_list->copy_buffer_region(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, resource, offset, size);

			// Wait for command to finish executing before destroying the upload buffer
			immediate_command_list->flush_and_wait(queue->_orig);
			break;
		}
	}
}
void reshade::d3d12::device_impl::update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)
{
	assert(resource.handle != 0);

	const D3D12_RESOURCE_DESC desc = reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc();

	UINT width = static_cast<UINT>(desc.Width);
	UINT num_rows = desc.Height;
	UINT num_slices = desc.DepthOrArraySize;
	if (box != nullptr)
	{
		width = box->right - box->left;
		num_rows = box->bottom - box->top;
		num_slices = box->back - box->front;
	}

	auto row_pitch = api::format_row_pitch(convert_format(desc.Format), width);
	row_pitch = (row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
	const auto slice_pitch = api::format_slice_pitch(convert_format(desc.Format), row_pitch, num_rows);

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

	assert(!_queues.empty());

	// Copy data from upload buffer into target texture using the first available immediate command list
	for (command_queue_impl *const queue : _queues)
	{
		const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list());
		if (immediate_command_list != nullptr)
		{
			immediate_command_list->copy_buffer_to_texture(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, 0, 0, resource, subresource, box);

			// Wait for command to finish executing before destroying the upload buffer
			immediate_command_list->flush_and_wait(queue->_orig);
			break;
		}
	}
}

void reshade::d3d12::device_impl::update_descriptor_sets(uint32_t count, const api::descriptor_set_update *updates)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto &update = updates[i];

		assert(update.offset >= update.binding);

		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_start;
		if (!resolve_descriptor_handle(update.set, &dst_range_start))
			continue;

		const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(update.type);

		dst_range_start = offset_descriptor_handle(dst_range_start, update.offset, heap_type);

		if (update.type == api::descriptor_type::constant_buffer)
		{
			for (uint32_t k = 0; k < update.count; ++k, dst_range_start.ptr += _descriptor_handle_size[heap_type])
			{
				const auto buffer_range = static_cast<const api::buffer_range *>(update.descriptors)[k];
				const auto buffer_resource = reinterpret_cast<ID3D12Resource *>(buffer_range.buffer.handle);

				D3D12_CONSTANT_BUFFER_VIEW_DESC view_desc;
				view_desc.BufferLocation = buffer_resource->GetGPUVirtualAddress() + buffer_range.offset;
				view_desc.SizeInBytes = (buffer_range.size == UINT64_MAX) ? static_cast<UINT>(buffer_resource->GetDesc().Width) : static_cast<UINT>(buffer_range.size);

				_orig->CreateConstantBufferView(&view_desc, dst_range_start);
			}
		}
		else if (update.type == api::descriptor_type::sampler || update.type == api::descriptor_type::shader_resource_view || update.type == api::descriptor_type::unordered_access_view)
		{
#ifndef WIN64
			const UINT src_range_size = 1;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_handles(update.count);
			for (UINT k = 0; k < update.count; ++k)
				src_handles[k] = { static_cast<SIZE_T>(static_cast<const uint64_t *>(update.descriptors)[k]) };

			_orig->CopyDescriptors(1, &dst_range_start, &update.count, update.count, src_handles.data(), &src_range_size, heap_type);
#else
			static_assert(
				sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) == sizeof(api::sampler) &&
				sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) == sizeof(api::resource_view));

			std::vector<UINT> src_range_sizes(update.count, 1);
			_orig->CopyDescriptors(1, &dst_range_start, &update.count, update.count, static_cast<const D3D12_CPU_DESCRIPTOR_HANDLE *>(update.descriptors), src_range_sizes.data(), heap_type);
#endif
		}
		else
		{
			assert(false);
		}
	}
}

bool reshade::d3d12::device_impl::create_query_pool(api::query_type type, uint32_t size, api::query_pool *out_handle)
{
	com_ptr<ID3D12Resource> readback_resource;
	{
		D3D12_RESOURCE_DESC readback_desc = {};
		readback_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readback_desc.Width = size * sizeof(uint64_t);
		readback_desc.Height = 1;
		readback_desc.DepthOrArraySize = 1;
		readback_desc.MipLevels = 1;
		readback_desc.SampleDesc = { 1, 0 };
		readback_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		const D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_READBACK };

		if (FAILED(_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &readback_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback_resource))))
		{
			*out_handle = { 0 };
			return false;
		}
	}

	D3D12_QUERY_HEAP_DESC internal_desc = {};
	internal_desc.Type = convert_query_type_to_heap_type(type);
	internal_desc.Count = size;

	if (com_ptr<ID3D12QueryHeap> object;
		SUCCEEDED(_orig->CreateQueryHeap(&internal_desc, IID_PPV_ARGS(&object))))
	{
		object->SetPrivateDataInterface(extra_data_guid, readback_resource.get());

		*out_handle = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_query_pool(api::query_pool handle)
{
	if (handle.handle != 0)
		reinterpret_cast<IUnknown *>(handle.handle)->Release();
}

bool reshade::d3d12::device_impl::get_query_pool_results(api::query_pool pool, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(pool.handle != 0);
	assert(stride >= sizeof(uint64_t));

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(pool.handle);

	com_ptr<ID3D12Resource> readback_resource;
	UINT private_size = sizeof(ID3D12Resource *);
	if (SUCCEEDED(heap_object->GetPrivateData(extra_data_guid, &private_size, &readback_resource)))
	{
		const D3D12_RANGE read_range = { static_cast<SIZE_T>(first) * sizeof(uint64_t), (static_cast<SIZE_T>(first) + static_cast<SIZE_T>(count)) * sizeof(uint64_t) };
		const D3D12_RANGE write_range = { 0, 0 };

		void *mapped_data = nullptr;
		if (SUCCEEDED(readback_resource->Map(0, &read_range, &mapped_data)))
		{
			for (uint32_t i = 0; i < count; ++i)
			{
				*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(results) + i * stride) = static_cast<uint64_t *>(mapped_data)[first + i];
			}

			readback_resource->Unmap(0, &write_range);

			return true;
		}
	}

	return false;
}

void reshade::d3d12::device_impl::register_resource(ID3D12Resource *resource)
{
	assert(resource != nullptr);

	const std::unique_lock<std::shared_mutex> lock(_resource_mutex);

#if RESHADE_ADDON
	if (const D3D12_RESOURCE_DESC desc = resource->GetDesc();
		desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS address = resource->GetGPUVirtualAddress();
		if (address != 0)
			_buffer_gpu_addresses.emplace_back(resource, D3D12_GPU_VIRTUAL_ADDRESS_RANGE { address, desc.Width });
	}
#endif
}
void reshade::d3d12::device_impl::unregister_resource(ID3D12Resource *resource)
{
	assert(resource != nullptr);

	const std::unique_lock<std::shared_mutex> lock(_resource_mutex);

#if RESHADE_ADDON
	if (const D3D12_RESOURCE_DESC desc = resource->GetDesc();
		desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		const auto it = std::find_if(_buffer_gpu_addresses.begin(), _buffer_gpu_addresses.end(), [resource](const auto &buffer_info) { return buffer_info.first == resource; });
		if (it != _buffer_gpu_addresses.end())
			_buffer_gpu_addresses.erase(it);
	}
#endif

#if 0
	// Remove all views that referenced this resource
	for (auto it = _views.begin(); it != _views.end();)
	{
		if (it->second.first == resource)
			it = _views.erase(it);
		else
			++it;
	}
#endif
}

#if RESHADE_ADDON
void reshade::d3d12::device_impl::register_descriptor_heap(ID3D12DescriptorHeap *heap)
{
	assert(heap != nullptr);

	const std::unique_lock<std::shared_mutex> lock(_heap_mutex);
	_descriptor_heaps.push_back(heap);
}
void reshade::d3d12::device_impl::unregister_descriptor_heap(ID3D12DescriptorHeap *heap)
{
	assert(heap != nullptr);

	const std::unique_lock<std::shared_mutex> lock(_heap_mutex);
	_descriptor_heaps.erase(std::find(_descriptor_heaps.begin(), _descriptor_heaps.end(), heap));
}
#endif

bool reshade::d3d12::device_impl::resolve_gpu_address(D3D12_GPU_VIRTUAL_ADDRESS address, api::resource *out_resource, uint64_t *out_offset) const
{
	assert(out_offset != nullptr && out_resource != nullptr);

	*out_offset = 0;
	*out_resource = { 0 };

	if (!address)
		return true;

#if RESHADE_ADDON
	const std::shared_lock<std::shared_mutex> lock(_resource_mutex);

	for (const auto &buffer_info : _buffer_gpu_addresses)
	{
		if (address < buffer_info.second.StartAddress)
			continue;

		const UINT64 address_offset = address - buffer_info.second.StartAddress;
		if (address_offset >= buffer_info.second.SizeInBytes)
			continue;

		*out_offset = address_offset;
		*out_resource = to_handle(buffer_info.first);
		return true;
	}
#endif

	assert(false);
	return false;
}

bool reshade::d3d12::device_impl::resolve_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, D3D12_DESCRIPTOR_HEAP_TYPE type, api::descriptor_set *out_set) const
{
	assert(out_set != nullptr);

#if RESHADE_ADDON
	// It is unlikely an application creates or destroys descriptor heaps while this can be called, so avoid locking
	for (ID3D12DescriptorHeap *const heap : _descriptor_heaps)
	{
		const auto desc = heap->GetDesc();
		if (desc.Type != type)
			continue;

		const auto heap_start_cpu = heap->GetCPUDescriptorHandleForHeapStart();
		if (heap_start_cpu.ptr <= _descriptor_handle_size[desc.Type])
		{
			// 'GetCPUDescriptorHandleForHeapStart' returns a global descriptor heap index and CPU descriptor handles use that as alignment
			if ((handle.ptr % _descriptor_handle_size[desc.Type]) != heap_start_cpu.ptr)
				continue;
		}
		else
		{
			if ((handle.ptr < heap_start_cpu.ptr) || (handle.ptr >= (heap_start_cpu.ptr + static_cast<SIZE_T>(desc.NumDescriptors) * _descriptor_handle_size[desc.Type])))
				continue;
		}

		SIZE_T address_offset = handle.ptr - heap_start_cpu.ptr;
		assert(address_offset < (static_cast<SIZE_T>(desc.NumDescriptors) * _descriptor_handle_size[desc.Type]) && (address_offset % _descriptor_handle_size[desc.Type]) == 0);

		*out_set = { heap->GetGPUDescriptorHandleForHeapStart().ptr + address_offset };
		return (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0;
	}
#endif

	assert(false);

	*out_set = { 0 };
	return false;
}
bool reshade::d3d12::device_impl::resolve_descriptor_handle(api::descriptor_set set, D3D12_CPU_DESCRIPTOR_HANDLE *handle, api::descriptor_pool *out_pool, uint32_t *out_offset) const
{
	const D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu = { set.handle };

	if (handle != nullptr)
	{
		assert(out_pool == nullptr && out_offset == nullptr);

		// This handle conversion does not require locking, since it is soly based on the heap base addresses which do not change
		if (_gpu_view_heap.convert_handle(handle_gpu, *handle))
			return true;
		if (_gpu_sampler_heap.convert_handle(handle_gpu, *handle))
			return true;
	}

#if RESHADE_ADDON
	// It is unlikely an application creates or destroys descriptor heaps while this can be called, so avoid locking
	for (ID3D12DescriptorHeap *const heap : _descriptor_heaps)
	{
		if (heap == nullptr)
			continue;

		const D3D12_GPU_DESCRIPTOR_HANDLE base_handle = heap->GetGPUDescriptorHandleForHeapStart();
		if (handle_gpu.ptr < base_handle.ptr)
			continue;

		const auto desc = heap->GetDesc();
		const auto address_offset = handle_gpu.ptr - base_handle.ptr;
		if (address_offset >= (static_cast<UINT64>(desc.NumDescriptors) * _descriptor_handle_size[desc.Type]))
			continue;

		assert((address_offset % _descriptor_handle_size[desc.Type]) == 0);

		if (handle != nullptr)
			handle->ptr = heap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(address_offset);
		if (out_pool != nullptr)
			*out_pool = { reinterpret_cast<uintptr_t>(heap) };
		if (out_offset != nullptr)
			*out_offset = static_cast<UINT>(address_offset / _descriptor_handle_size[desc.Type]);
		return true;
	}
#endif

	assert(false);

	if (handle != nullptr)
		handle->ptr = 0;
	if (out_pool != nullptr)
		*out_pool = { 0 };
	if (out_offset != nullptr)
		*out_offset = 0;
	return false;
}
