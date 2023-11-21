/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_queue.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_resource_call_vtable.inl"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include <algorithm>
#include <dxgi1_4.h>

extern bool is_windows7();

#ifdef _WIN64
constexpr size_t heap_index_start = 28;
#else
// Make a bit more space for the heap index in descriptor handles, at the cost of less space for the descriptor index, due to overall limit of only 32-bit being available
constexpr size_t heap_index_start = 24;
#endif

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
		_descriptor_handle_size[type] = device->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));

#if RESHADE_ADDON >= 2
	// Make some space in the descriptor heap array, so that it is unlikely to need reallocation
	_descriptor_heaps.reserve(4096);

	const auto gpu_view_heap = new D3D12DescriptorHeap(device, _gpu_view_heap.get());
	register_descriptor_heap(gpu_view_heap);
	const auto gpu_sampler_heap = new D3D12DescriptorHeap(device, _gpu_sampler_heap.get());
	register_descriptor_heap(gpu_sampler_heap);

	assert(_descriptor_heaps.size() == 2);
#endif

	// Create mipmap generation states
	{
		const resources::data_resource cs = resources::load_data_resource(IDR_MIPMAP_CS);

		// Creating a root signature with static samplers here cause banding artifacts in Call of Duty: Modern Warfare for some strange reason, so need to use sampler in descriptor table instead
		if (com_ptr<ID3DBlob> signature_blob;
			FAILED(_orig->CreateRootSignature(0, cs.data, cs.data_size, IID_PPV_ARGS(&_mipmap_signature))))
		{
			LOG(ERROR) << "Failed to create mipmap generation signature!";
		}
		else
		{
			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = _mipmap_signature.get();
			pso_desc.CS = { cs.data, cs.data_size };

			if (FAILED(_orig->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&_mipmap_pipeline))))
			{
				LOG(ERROR) << "Failed to create mipmap generation pipeline!";
			}
		}
	}
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

#if RESHADE_ADDON >= 2
	const auto gpu_view_heap = _descriptor_heaps[0];
	unregister_descriptor_heap(gpu_view_heap);
	delete gpu_view_heap;
	const auto gpu_sampler_heap = _descriptor_heaps[1];
	unregister_descriptor_heap(gpu_sampler_heap);
	delete gpu_sampler_heap;

	assert(_descriptor_heaps.empty());
#endif
}

reshade::api::device_properties reshade::d3d12::device_impl::get_properties() const
{
	api::device_properties props;
	props.api_version = D3D_FEATURE_LEVEL_12_0;

	const LUID luid = _orig->GetAdapterLuid();
	com_ptr<IDXGIAdapter> dxgi_adapter;

	const auto CreateDXGIFactory1 = reinterpret_cast<HRESULT(WINAPI *)(REFIID riid, void **ppFactory)>(
		GetProcAddress(GetModuleHandleW(L"dxgi.dll"), "CreateDXGIFactory1"));
	assert(CreateDXGIFactory1 != nullptr);
	const auto CreateDXGIFactory2 = reinterpret_cast<HRESULT(WINAPI *)(UINT Flags, REFIID riid, void **ppFactory)>(
		GetProcAddress(GetModuleHandleW(L"dxgi.dll"), "CreateDXGIFactory2"));
	assert(CreateDXGIFactory2 != nullptr || is_windows7());

	if (com_ptr<IDXGIFactory4> factory4;
		CreateDXGIFactory2 != nullptr && SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory4))))
	{
		factory4->EnumAdapterByLuid(luid, IID_PPV_ARGS(&dxgi_adapter));
	}
	else if (com_ptr<IDXGIFactory1> factory1;
		SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory1))))
	{
		for (UINT i = 0; factory1->EnumAdapters(i, &dxgi_adapter) != DXGI_ERROR_NOT_FOUND; ++i, dxgi_adapter.reset())
		{
			DXGI_ADAPTER_DESC adapter_desc;
			if (SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc)) && std::memcmp(&adapter_desc.AdapterLuid, &luid, sizeof(luid)) == 0)
				break;
		}
	}

	if (dxgi_adapter != nullptr)
	{
		LARGE_INTEGER umd_version;
		if (SUCCEEDED(dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd_version)))
		{
			props.driver_version = LOWORD(umd_version.LowPart) + (HIWORD(umd_version.LowPart) % 10) * 10000;
		}

		DXGI_ADAPTER_DESC adapter_desc;
		if (SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc)))
		{
			props.vendor_id = adapter_desc.VendorId;
			props.device_id = adapter_desc.DeviceId;

			static_assert(std::size(props.description) >= std::size(adapter_desc.Description));
			utf8::unchecked::utf16to8(adapter_desc.Description, adapter_desc.Description + std::size(adapter_desc.Description), props.description);
		}
	}

	return props;
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
	case api::device_caps::copy_query_heap_results:
	case api::device_caps::sampler_compare:
	case api::device_caps::sampler_anisotropic:
		return true;
	case api::device_caps::sampler_with_resource_view:
		return false;
	case api::device_caps::shared_resource:
	case api::device_caps::shared_resource_nt_handle:
		return !is_windows7();
	case api::device_caps::resolve_depth_stencil:
		return true;
	case api::device_caps::shared_fence:
	case api::device_caps::shared_fence_nt_handle:
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

	com_ptr<ID3D12Resource> object;

	const bool is_shared = (desc.flags & api::resource_flags::shared) != 0;
	if (is_shared)
	{
		// Only NT handles are supported
		if (shared_handle == nullptr || (desc.flags & api::resource_flags::shared_nt_handle) == 0)
			return false;

		if (*shared_handle != nullptr)
		{
			assert(initial_data == nullptr);

			if (SUCCEEDED(_orig->OpenSharedHandle(*shared_handle, IID_PPV_ARGS(&object))))
			{
				*out_handle = to_handle(object.release());
				return true;
			}

			return false;
		}
	}

	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	D3D12_RESOURCE_DESC internal_desc = {};
	D3D12_HEAP_PROPERTIES heap_props = {};
	convert_resource_desc(desc, internal_desc, heap_props, heap_flags);

	D3D12_SUBRESOURCE_FOOTPRINT footprint = {};

	if (desc.type == api::resource_type::buffer)
	{
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		// Constant buffer views need to be aligned to 256 bytes, so make buffer large enough to ensure that is possible
		if ((desc.usage & (api::resource_usage::constant_buffer)) != 0)
			internal_desc.Width = (internal_desc.Width + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1u);
	}
	else if ((desc.heap == api::memory_heap::gpu_to_cpu || desc.heap == api::memory_heap::cpu_only) && desc.texture.levels == 1)
	{
		// Textures in the upload or readback heap are created as buffers, so that they can be mapped
		internal_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

		auto row_pitch = api::format_row_pitch(desc.texture.format, desc.texture.width);
		row_pitch = (row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
		const auto slice_pitch = api::format_slice_pitch(desc.texture.format, row_pitch, desc.texture.height);

		footprint.Format = internal_desc.Format;
		footprint.Width = static_cast<UINT>(internal_desc.Width);
		footprint.Height = internal_desc.Height;
		footprint.Depth = internal_desc.DepthOrArraySize;
		footprint.RowPitch = row_pitch;

		internal_desc.Width = static_cast<UINT64>(slice_pitch) * desc.texture.depth_or_layers;
		internal_desc.Height = 1;
		internal_desc.DepthOrArraySize = 1;
		internal_desc.Format = DXGI_FORMAT_UNKNOWN;
		internal_desc.SampleDesc = { 1, 0 };
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
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

	if (SUCCEEDED(desc.heap == api::memory_heap::unknown ?
			_orig->CreateReservedResource(&internal_desc, convert_usage_to_resource_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object)) :
			_orig->CreateCommittedResource(&heap_props, heap_flags, &internal_desc, convert_usage_to_resource_states(initial_state), use_default_clear_value ? &default_clear_value : nullptr, IID_PPV_ARGS(&object))))
	{
		if (is_shared && FAILED(_orig->CreateSharedHandle(object.get(), nullptr, GENERIC_ALL, nullptr, shared_handle)))
			return false;

		if (footprint.Format != DXGI_FORMAT_UNKNOWN)
			object->SetPrivateData(extra_data_guid, sizeof(footprint), &footprint);

		register_resource(object.get());

		*out_handle = to_handle(object.release());

		if (initial_data != nullptr)
		{
			assert(initial_state != api::resource_usage::undefined);

			// Transition resource into the initial state using the first available immediate command list
			if (const auto immediate_command_list = get_first_immediate_command_list())
			{
				const api::resource_usage states_upload[2] = { initial_state, api::resource_usage::copy_dest };
				immediate_command_list->barrier(1, out_handle, &states_upload[0], &states_upload[1]);

				if (desc.type == api::resource_type::buffer)
				{
					update_buffer_region(initial_data->data, *out_handle, 0, desc.buffer.size);
				}
				else
				{
					for (uint32_t subresource = 0; subresource < (desc.type == api::resource_type::texture_3d ? 1u : static_cast<uint32_t>(desc.texture.depth_or_layers)) * desc.texture.levels; ++subresource)
						update_texture_region(initial_data[subresource], *out_handle, subresource, nullptr);
				}

				const api::resource_usage states_finalize[2] = { api::resource_usage::copy_dest, initial_state };
				immediate_command_list->barrier(1, out_handle, &states_finalize[0], &states_finalize[1]);

				immediate_command_list->flush();
			}
		}

		return true;
	}

	return false;
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

	// This will retrieve the heap properties for placed and committed resources, not for reserved resources (which will then be translated to 'memory_heap::unknown')
	D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
	D3D12_HEAP_PROPERTIES heap_props = {};
	reinterpret_cast<ID3D12Resource *>(resource.handle)->GetHeapProperties(&heap_props, &heap_flags);

	return convert_resource_desc(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc(), heap_props, heap_flags);
}

bool reshade::d3d12::device_impl::create_resource_view(api::resource resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view *out_handle)
{
	*out_handle = { 0 };

	if (resource.handle == 0)
		return false;

	// Cannot create a resource view with a typeless format
	assert(desc.format != api::format_to_typeless(desc.format) || api::format_to_typeless(desc.format) == api::format_to_default_typed(desc.format));

	switch (usage_type)
	{
		case api::resource_usage::depth_stencil:
		{
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
			if (!_view_heaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV].allocate(descriptor_handle))
				break; // No more space available in the descriptor heap

			D3D12_DEPTH_STENCIL_VIEW_DESC internal_desc = {};
			convert_resource_view_desc(desc, internal_desc);

			_orig->CreateDepthStencilView(reinterpret_cast<ID3D12Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, descriptor_handle);

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

			_orig->CreateRenderTargetView(reinterpret_cast<ID3D12Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, descriptor_handle);

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

			_orig->CreateShaderResourceView(reinterpret_cast<ID3D12Resource *>(resource.handle), desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, descriptor_handle);

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

			_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), nullptr, desc.type != api::resource_view_type::unknown ? &internal_desc : nullptr, descriptor_handle);

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

bool reshade::d3d12::device_impl::map_buffer_region(api::resource resource, uint64_t offset, uint64_t, api::map_access access, void **out_data)
{
	if (out_data == nullptr)
		return false;

	assert(resource.handle != 0);

	const D3D12_RANGE no_read = { 0, 0 };

	if (SUCCEEDED(ID3D12Resource_Map(reinterpret_cast<ID3D12Resource *>(resource.handle), 0, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read : nullptr, out_data)))
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

	ID3D12Resource_Unmap(reinterpret_cast<ID3D12Resource *>(resource.handle), 0, nullptr);
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
	if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		if (subresource != 0)
			return false;

		UINT extra_data_size = sizeof(layout.Footprint);
		if (FAILED(reinterpret_cast<ID3D12Resource *>(resource.handle)->GetPrivateData(extra_data_guid, &extra_data_size, &layout.Footprint)))
			return false;

		out_data->slice_pitch = layout.Footprint.Height;
	}
	else
	{
		_orig->GetCopyableFootprints(&desc, subresource, 1, 0, &layout, &out_data->slice_pitch, nullptr, nullptr);
	}

	out_data->row_pitch = layout.Footprint.RowPitch;
	out_data->slice_pitch *= layout.Footprint.RowPitch;

	return SUCCEEDED(ID3D12Resource_Map(reinterpret_cast<ID3D12Resource *>(resource.handle),
		subresource, access == api::map_access::write_only || access == api::map_access::write_discard ? &no_read : nullptr, &out_data->data));
}
void reshade::d3d12::device_impl::unmap_texture_region(api::resource resource, uint32_t subresource)
{
	assert(resource.handle != 0);

	ID3D12Resource_Unmap(reinterpret_cast<ID3D12Resource *>(resource.handle), subresource, nullptr);
}

void reshade::d3d12::device_impl::update_buffer_region(const void *data, api::resource resource, uint64_t offset, uint64_t size)
{
	assert(resource.handle != 0);
	assert(data != nullptr);

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
		LOG(ERROR) << "Failed to create upload buffer (width = " << intermediate_desc.Width << ")!";
		return;
	}
	intermediate->SetName(L"ReShade upload buffer");

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	if (FAILED(ID3D12Resource_Map(intermediate.get(), 0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return;

	std::memcpy(mapped_data, data, static_cast<size_t>(size));

	ID3D12Resource_Unmap(intermediate.get(), 0, nullptr);

	// Copy data from upload buffer into target texture using the first available immediate command list
	if (const auto immediate_command_list = get_first_immediate_command_list())
	{
		immediate_command_list->copy_buffer_region(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, resource, offset, size);

		// Wait for command to finish executing before destroying the upload buffer
		immediate_command_list->flush_and_wait();
	}
}
void reshade::d3d12::device_impl::update_texture_region(const api::subresource_data &data, api::resource resource, uint32_t subresource, const api::subresource_box *box)
{
	assert(resource.handle != 0);
	assert(data.data != nullptr);

	const D3D12_RESOURCE_DESC desc = reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc();
	if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		if (subresource != 0 || box != nullptr)
			return;

		update_buffer_region(data.data, resource, 0, data.slice_pitch);
		return;
	}

	UINT width = static_cast<UINT>(desc.Width);
	UINT num_rows = desc.Height;
	UINT num_slices = desc.DepthOrArraySize;
	if (box != nullptr)
	{
		width = box->width();
		num_rows = box->height();
		num_slices = box->depth();
	}

	auto row_pitch = api::format_row_pitch(convert_format(desc.Format), width);
	row_pitch = (row_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
	const auto slice_pitch = api::format_slice_pitch(convert_format(desc.Format), row_pitch, num_rows);

	// Allocate host memory for upload
	D3D12_RESOURCE_DESC intermediate_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };
	intermediate_desc.Width = static_cast<UINT64>(num_slices) * static_cast<UINT64>(slice_pitch);
	intermediate_desc.Height = 1;
	intermediate_desc.DepthOrArraySize = 1;
	intermediate_desc.MipLevels = 1;
	intermediate_desc.SampleDesc = { 1, 0 };
	intermediate_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const D3D12_HEAP_PROPERTIES upload_heap_props = { D3D12_HEAP_TYPE_UPLOAD };

	com_ptr<ID3D12Resource> intermediate;
	if (FAILED(_orig->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &intermediate_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate))))
	{
		LOG(ERROR) << "Failed to create upload buffer (width = " << intermediate_desc.Width << ")!";
		return;
	}
	intermediate->SetName(L"ReShade upload buffer");

	// Fill upload buffer with pixel data
	uint8_t *mapped_data;
	if (FAILED(ID3D12Resource_Map(intermediate.get(), 0, nullptr, reinterpret_cast<void **>(&mapped_data))))
		return;

	const size_t row_size = data.row_pitch < row_pitch ? data.row_pitch : static_cast<size_t>(row_pitch);

	for (size_t z = 0; z < num_slices; ++z)
	{
		const auto dst_slice = mapped_data + z * slice_pitch;
		const auto src_slice = static_cast<const uint8_t *>(data.data) + z * data.slice_pitch;

		for (size_t y = 0; y < num_rows; ++y)
		{
			std::memcpy(
				dst_slice + y * row_pitch,
				src_slice + y * data.row_pitch, row_size);
		}
	}

	ID3D12Resource_Unmap(intermediate.get(), 0, nullptr);

	// Copy data from upload buffer into target texture using the first available immediate command list
	if (const auto immediate_command_list = get_first_immediate_command_list())
	{
		immediate_command_list->copy_buffer_to_texture(api::resource { reinterpret_cast<uintptr_t>(intermediate.get()) }, 0, 0, 0, resource, subresource, box);

		// Wait for command to finish executing before destroying the upload buffer
		immediate_command_list->flush_and_wait();
	}
}

bool reshade::d3d12::device_impl::create_pipeline(api::pipeline_layout layout, uint32_t subobject_count, const api::pipeline_subobject *subobjects, api::pipeline *out_handle)
{
	api::shader_desc vs_desc = {};
	api::shader_desc hs_desc = {};
	api::shader_desc ds_desc = {};
	api::shader_desc gs_desc = {};
	api::shader_desc ps_desc = {};
	api::shader_desc cs_desc = {};
	api::pipeline_subobject input_layout_desc = {};
	api::stream_output_desc stream_output_desc = {};
	api::blend_desc blend_desc = {};
	api::rasterizer_desc rasterizer_desc = {};
	api::depth_stencil_desc depth_stencil_desc = {};
	api::primitive_topology topology = api::primitive_topology::triangle_list;
	api::format depth_stencil_format = api::format::unknown;
	api::pipeline_subobject render_target_formats = {};
	uint32_t sample_mask = UINT32_MAX;
	uint32_t sample_count = 1;

	for (uint32_t i = 0; i < subobject_count; ++i)
	{
		if (subobjects[i].count == 0)
			continue;

		switch (subobjects[i].type)
		{
		case api::pipeline_subobject_type::vertex_shader:
			assert(subobjects[i].count == 1);
			vs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::hull_shader:
			assert(subobjects[i].count == 1);
			hs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::domain_shader:
			assert(subobjects[i].count == 1);
			ds_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::geometry_shader:
			assert(subobjects[i].count == 1);
			gs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::pixel_shader:
			assert(subobjects[i].count == 1);
			ps_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::compute_shader:
			assert(subobjects[i].count == 1);
			cs_desc = *static_cast<const api::shader_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::input_layout:
			input_layout_desc = subobjects[i];
			break;
		case api::pipeline_subobject_type::stream_output_state:
			assert(subobjects[i].count == 1);
			stream_output_desc = *static_cast<const api::stream_output_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::blend_state:
			assert(subobjects[i].count == 1);
			blend_desc = *static_cast<const api::blend_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::rasterizer_state:
			assert(subobjects[i].count == 1);
			rasterizer_desc = *static_cast<const api::rasterizer_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::depth_stencil_state:
			assert(subobjects[i].count == 1);
			depth_stencil_desc = *static_cast<const api::depth_stencil_desc *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::primitive_topology:
			assert(subobjects[i].count == 1);
			topology = *static_cast<const api::primitive_topology *>(subobjects[i].data);
			if (topology == api::primitive_topology::triangle_fan)
				goto exit_failure;
			break;
		case api::pipeline_subobject_type::depth_stencil_format:
			assert(subobjects[i].count == 1);
			depth_stencil_format = *static_cast<const api::format *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::render_target_formats:
			assert(subobjects[i].count <= 8);
			render_target_formats = subobjects[i];
			break;
		case api::pipeline_subobject_type::sample_mask:
			assert(subobjects[i].count == 1);
			sample_mask = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::sample_count:
			assert(subobjects[i].count == 1);
			sample_count = *static_cast<const uint32_t *>(subobjects[i].data);
			break;
		case api::pipeline_subobject_type::viewport_count:
			assert(subobjects[i].count == 1);
			break;
		case api::pipeline_subobject_type::dynamic_pipeline_states:
			for (uint32_t k = 0; k < subobjects[i].count; ++k)
				if (static_cast<const api::dynamic_state *>(subobjects[i].data)[k] != api::dynamic_state::primitive_topology &&
					static_cast<const api::dynamic_state *>(subobjects[i].data)[k] != api::dynamic_state::blend_constant &&
					static_cast<const api::dynamic_state *>(subobjects[i].data)[k] != api::dynamic_state::front_stencil_reference_value &&
					static_cast<const api::dynamic_state *>(subobjects[i].data)[k] != api::dynamic_state::back_stencil_reference_value)
					goto exit_failure;
			break;
		case api::pipeline_subobject_type::max_vertex_count:
			assert(subobjects[i].count == 1);
			break; // Ignored
		default:
			assert(false);
			goto exit_failure;
		}
	}

	if (cs_desc.code_size != 0)
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC internal_desc = {};
		internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
		convert_shader_desc(cs_desc, internal_desc.CS);

		if (com_ptr<ID3D12PipelineState> pipeline;
			SUCCEEDED(_orig->CreateComputePipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
		{
			*out_handle = to_handle(pipeline.release());
			return true;
		}
	}
	else
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC internal_desc = {};
		internal_desc.pRootSignature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
		reshade::d3d12::convert_shader_desc(vs_desc, internal_desc.VS);
		reshade::d3d12::convert_shader_desc(ps_desc, internal_desc.PS);
		reshade::d3d12::convert_shader_desc(ds_desc, internal_desc.DS);
		reshade::d3d12::convert_shader_desc(hs_desc, internal_desc.HS);
		reshade::d3d12::convert_shader_desc(gs_desc, internal_desc.GS);
		reshade::d3d12::convert_stream_output_desc(stream_output_desc, internal_desc.StreamOutput);
		reshade::d3d12::convert_blend_desc(blend_desc, internal_desc.BlendState);
		internal_desc.SampleMask = sample_mask;
		reshade::d3d12::convert_rasterizer_desc(rasterizer_desc, internal_desc.RasterizerState);
		reshade::d3d12::convert_depth_stencil_desc(depth_stencil_desc, internal_desc.DepthStencilState);

		std::vector<D3D12_INPUT_ELEMENT_DESC> internal_elements;
		reshade::d3d12::convert_input_layout_desc(input_layout_desc.count, static_cast<const api::input_element *>(input_layout_desc.data), internal_elements);
		internal_desc.InputLayout.NumElements = static_cast<UINT>(internal_elements.size());
		internal_desc.InputLayout.pInputElementDescs = internal_elements.data();

		internal_desc.PrimitiveTopologyType = reshade::d3d12::convert_primitive_topology_type(topology);

		internal_desc.NumRenderTargets = render_target_formats.count;
		for (UINT i = 0; i < internal_desc.NumRenderTargets; ++i)
			internal_desc.RTVFormats[i] = reshade::d3d12::convert_format(static_cast<const api::format *>(render_target_formats.data)[i]);
		internal_desc.DSVFormat = reshade::d3d12::convert_format(depth_stencil_format);

		internal_desc.SampleDesc.Count = sample_count;

		if (com_ptr<ID3D12PipelineState> pipeline;
			SUCCEEDED(_orig->CreateGraphicsPipelineState(&internal_desc, IID_PPV_ARGS(&pipeline))))
		{
			pipeline_extra_data extra_data;
			extra_data.topology = convert_primitive_topology(topology);

			std::copy_n(blend_desc.blend_constant, 4, extra_data.blend_constant);

			pipeline->SetPrivateData(extra_data_guid, sizeof(extra_data), &extra_data);

			*out_handle = to_handle(pipeline.release());
			return true;
		}
	}

exit_failure:
	*out_handle = { 0 };
	return false;
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
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> internal_ranges(param_count);
	const auto set_ranges = new std::pair<D3D12_DESCRIPTOR_HEAP_TYPE, UINT>[param_count];

	for (uint32_t i = 0; i < param_count; ++i)
	{
		api::shader_stage visibility_mask = static_cast<api::shader_stage>(0);

		set_ranges[i] = { D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES, 0 };

		if (params[i].type != api::pipeline_layout_param_type::push_constants)
		{
			bool push_descriptors = (params[i].type == api::pipeline_layout_param_type::push_descriptors);
			const uint32_t range_count = push_descriptors ? 1 : params[i].descriptor_table.count;
			const api::descriptor_range *const input_ranges = push_descriptors ? &params[i].push_descriptors : params[i].descriptor_table.ranges;

			if (range_count == 0 || input_ranges[0].count == 0)
			{
				// Dummy parameter (to prevent root signature creation from failing)
				internal_params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				internal_params[i].Constants.ShaderRegister = i;
				internal_params[i].Constants.RegisterSpace = 255;
				internal_params[i].Constants.Num32BitValues = 1;
				continue;
			}

			internal_ranges[i].reserve(range_count);

			D3D12_DESCRIPTOR_HEAP_TYPE prev_heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

			for (uint32_t k = 0; k < range_count; ++k)
			{
				const api::descriptor_range &range = input_ranges[k];

				assert(range.array_size <= 1);

				D3D12_DESCRIPTOR_RANGE &internal_range = internal_ranges[i].emplace_back();
				internal_range.RangeType = convert_descriptor_type(range.type);
				internal_range.NumDescriptors = range.count;
				internal_range.BaseShaderRegister = range.dx_register_index;
				internal_range.RegisterSpace = range.dx_register_space;
				internal_range.OffsetInDescriptorsFromTableStart = range.binding;

				visibility_mask |= range.visibility;

				// Cannot mix different descriptor heap types in a single descriptor table
				const D3D12_DESCRIPTOR_HEAP_TYPE heap_type = convert_descriptor_type_to_heap_type(range.type);
				if (prev_heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES && prev_heap_type != heap_type)
					return false;

				prev_heap_type = heap_type;

				if (range.count != UINT32_MAX) // Don't count unbounded ranges
					set_ranges[i].second = std::max(set_ranges[i].second, range.binding + range.count);
			}

			set_ranges[i].first = prev_heap_type;

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

	const auto D3D12SerializeRootSignature = reinterpret_cast<HRESULT(WINAPI *)(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)>(
		GetProcAddress(GetModuleHandleW(L"d3d12.dll"), "D3D12SerializeRootSignature"));
	assert(D3D12SerializeRootSignature != nullptr);

	D3D12_ROOT_SIGNATURE_DESC internal_desc = {};
	internal_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	internal_desc.NumParameters = param_count;
	internal_desc.pParameters = internal_params.data();

	com_ptr<ID3DBlob> signature_blob, error_blob;
	com_ptr<ID3D12RootSignature> signature;
	if (SUCCEEDED(D3D12SerializeRootSignature(&internal_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob)) &&
		SUCCEEDED(_orig->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&signature))))
	{
		pipeline_layout_extra_data extra_data;
		extra_data.ranges = set_ranges;
		UINT extra_data_size = sizeof(extra_data);

		// D3D12 runtime returns the same root signature object for identical input blobs, just with the reference count increased
		// Do not overwrite the existing attached extra data in this case
		if (signature.ref_count() == 1 || FAILED(signature->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
		{
			signature->SetPrivateData(extra_data_guid, sizeof(extra_data), &extra_data);
		}
		else
		{
#ifndef NDEBUG
			for (uint32_t i = 0; i < param_count; ++i)
				assert(extra_data.ranges[i] == set_ranges[i]);
#endif
			delete[] set_ranges;
		}

		*out_handle = to_handle(signature.release());
		return true;
	}
	else
	{
		if (error_blob != nullptr)
			LOG(ERROR) << "Failed to create root signature: " << static_cast<const char *>(error_blob->GetBufferPointer());

		delete[] set_ranges;

		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_pipeline_layout(api::pipeline_layout handle)
{
	if (handle.handle == 0)
		return;

	const com_ptr<ID3D12RootSignature> signature(reinterpret_cast<ID3D12RootSignature *>(handle.handle), true);

	pipeline_layout_extra_data extra_data;
	UINT extra_data_size = sizeof(extra_data);
	// Only destroy attached extra data when this is the last reference to the root signature object
	if (signature.ref_count() == 1 && SUCCEEDED(signature->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
	{
		delete[] extra_data.ranges;
	}
}

bool reshade::d3d12::device_impl::allocate_descriptor_tables(uint32_t count, api::pipeline_layout layout, uint32_t layout_param, api::descriptor_table *out_tables)
{
	uint32_t total_count = 0;
	D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	const com_ptr<ID3D12RootSignature> signature(reinterpret_cast<ID3D12RootSignature *>(layout.handle), false);

	if (signature != nullptr)
	{
		pipeline_layout_extra_data extra_data;
		UINT extra_data_size = sizeof(extra_data);
		if (SUCCEEDED(signature->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
		{
			heap_type = extra_data.ranges[layout_param].first;
			total_count = extra_data.ranges[layout_param].second;
		}
	}

	if (total_count != 0 && total_count <= 0xFF)
	{
		assert(heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);

		for (uint32_t i = 0; i < count; ++i)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE base_handle;
			D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu;

			if (heap_type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
				!_gpu_view_heap.allocate_static(total_count, base_handle, base_handle_gpu) :
				!_gpu_sampler_heap.allocate_static(total_count, base_handle, base_handle_gpu))
			{
				free_descriptor_tables(count - i - 1, out_tables);
				goto exit_failure;
			}

			out_tables[i] = convert_to_descriptor_table(base_handle_gpu);
		}

		return true;
	}

exit_failure:
	for (uint32_t i = 0; i < count; ++i)
		out_tables[i] = { 0 };
	return false;
}
void reshade::d3d12::device_impl::free_descriptor_tables(uint32_t count, const api::descriptor_table *tables)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		if (tables[i].handle == 0)
			continue;

		_gpu_view_heap.free(convert_to_original_gpu_descriptor_handle(tables[i]));
		_gpu_sampler_heap.free(convert_to_original_gpu_descriptor_handle(tables[i]));
	}
}

void reshade::d3d12::device_impl::get_descriptor_heap_offset(api::descriptor_table table, uint32_t binding, uint32_t array_offset, api::descriptor_heap *heap, uint32_t *offset) const
{
	assert(table.handle != 0 && array_offset == 0 && heap != nullptr);

#if RESHADE_ADDON >= 2
	// Check if this is a D3D12_CPU_DESCRIPTOR_HANDLE or D3D12_GPU_DESCRIPTOR_HANDLE
	if ((table.handle & 0xF000000000000000ull) == 0xF000000000000000ull)
	{
		const size_t heap_index = (table.handle >> heap_index_start) & 0xFFFFFFF;
		assert(heap_index < _descriptor_heaps.size() && _descriptor_heaps[heap_index] != nullptr);

		*heap = to_handle(_descriptor_heaps[heap_index]->_orig);

		if (offset != nullptr)
		{
			const D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(table.handle & 0x3);
			*offset = ((table.handle & (((1ull << heap_index_start) - 1) ^ 0x7)) / _descriptor_handle_size[type]) + binding;
			assert(*offset < _descriptor_heaps[heap_index]->_orig->GetDesc().NumDescriptors);
		}
		return;
	}
#endif

	const D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu = { table.handle };

#if RESHADE_ADDON >= 2
	for (D3D12DescriptorHeap *const heap_impl : _descriptor_heaps)
	{
		if (heap_impl == nullptr || handle_gpu.ptr < heap_impl->_orig_base_gpu_handle.ptr)
			continue;

		D3D12_DESCRIPTOR_HEAP_DESC desc = heap_impl->_orig->GetDesc();
		if (handle_gpu.ptr >= offset_descriptor_handle(heap_impl->_orig_base_gpu_handle, desc.NumDescriptors, desc.Type).ptr)
			continue;

		*heap = to_handle(heap_impl->_orig);

		if (offset != nullptr)
			*offset = static_cast<uint32_t>((handle_gpu.ptr - heap_impl->_orig_base_gpu_handle.ptr) / _descriptor_handle_size[desc.Type]) + binding;
		return;
	}
#else
	if (_gpu_view_heap.contains(handle_gpu))
	{
		*heap = to_handle(_gpu_view_heap.get());

		if (offset != nullptr)
		{
			const D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			*offset = static_cast<uint32_t>((handle_gpu.ptr - _gpu_view_heap.get()->GetGPUDescriptorHandleForHeapStart().ptr) / _descriptor_handle_size[type]) + binding;
		}
		return;
	}
	else if (_gpu_sampler_heap.contains(handle_gpu))
	{
		*heap = to_handle(_gpu_sampler_heap.get());

		if (offset != nullptr)
		{
			const D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			*offset = static_cast<uint32_t>((handle_gpu.ptr - _gpu_sampler_heap.get()->GetGPUDescriptorHandleForHeapStart().ptr) / _descriptor_handle_size[type]) + binding;
		}
		return;
	}
#endif

	assert(false);

	*heap = { 0 }; // Not implemented
	if (offset != nullptr)
		*offset = binding;
}

void reshade::d3d12::device_impl::copy_descriptor_tables(uint32_t count, const api::descriptor_table_copy *copies)
{
	D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_table_copy &copy = copies[i];

		assert(copy.dest_array_offset == 0 && copy.source_array_offset == 0);

		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_start = convert_to_original_cpu_descriptor_handle(copy.dest_table, &heap_type);
		dst_range_start = offset_descriptor_handle(dst_range_start, copy.dest_binding, heap_type);
		D3D12_CPU_DESCRIPTOR_HANDLE src_range_start = convert_to_original_cpu_descriptor_handle(copy.source_table, &heap_type);
		src_range_start = offset_descriptor_handle(src_range_start, copy.source_binding, heap_type);

		_orig->CopyDescriptorsSimple(copy.count, dst_range_start, src_range_start, heap_type);
	}
}
void reshade::d3d12::device_impl::update_descriptor_tables(uint32_t count, const api::descriptor_table_update *updates)
{
	D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	for (uint32_t i = 0; i < count; ++i)
	{
		const api::descriptor_table_update &update = updates[i];

		assert(update.array_offset == 0);

		D3D12_CPU_DESCRIPTOR_HANDLE dst_range_start = convert_to_original_cpu_descriptor_handle(update.table, &heap_type);
		dst_range_start = offset_descriptor_handle(dst_range_start, update.binding, heap_type);

		assert(convert_descriptor_type_to_heap_type(update.type) == heap_type);

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
#ifndef _WIN64
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

bool reshade::d3d12::device_impl::create_query_heap(api::query_type type, uint32_t size, api::query_heap *out_handle)
{
	com_ptr<ID3D12Resource> readback_resource;
	std::vector<com_ptr<ID3D12Fence>> fences(size);

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

	for (uint32_t i = 0; i < size; i++)
	{
		if (FAILED(_orig->CreateFence(0ull, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i]))))
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
		query_heap_extra_data extra_data;
		extra_data.size = size;
		extra_data.readback_resource = readback_resource.release();
		extra_data.fences = new std::pair<ID3D12Fence *, UINT64>[size];
		for (uint32_t i = 0; i < extra_data.size; ++i)
			// Start with fence value above the initial value, so that 'get_query_heap_results' will return false for the first frame as well
			extra_data.fences[i] = std::make_pair(fences[i].release(), 1ull);

		object->SetPrivateData(extra_data_guid, sizeof(extra_data), &extra_data);

		*out_handle = { reinterpret_cast<uintptr_t>(object.release()) };
		return true;
	}
	else
	{
		*out_handle = { 0 };
		return false;
	}
}
void reshade::d3d12::device_impl::destroy_query_heap(api::query_heap handle)
{
	if (handle.handle == 0)
		return;

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(handle.handle);

	query_heap_extra_data extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(heap_object->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
	{
		extra_data.readback_resource->Release();

		for (uint32_t i = 0; i < extra_data.size; ++i)
			extra_data.fences[i].first->Release();
		delete[] extra_data.fences;
	}

	heap_object->Release();
}

bool reshade::d3d12::device_impl::get_query_heap_results(api::query_heap heap, uint32_t first, uint32_t count, void *results, uint32_t stride)
{
	assert(heap.handle != 0);
	assert(stride >= sizeof(uint64_t));

	const auto heap_object = reinterpret_cast<ID3D12QueryHeap *>(heap.handle);

	query_heap_extra_data extra_data;
	UINT extra_data_size = sizeof(extra_data);
	if (SUCCEEDED(heap_object->GetPrivateData(extra_data_guid, &extra_data_size, &extra_data)))
	{
		for (size_t i = 0; i < count; ++i)
		{
			const std::pair<ID3D12Fence *, UINT64> &fence = extra_data.fences[i + first];

			// Verify that the query has finished executing on the GPU
			if (fence.first->GetCompletedValue() < fence.second)
				return false;
		}

		const D3D12_RANGE read_range = { static_cast<SIZE_T>(first) * sizeof(uint64_t), (static_cast<SIZE_T>(first) + static_cast<SIZE_T>(count)) * sizeof(uint64_t) };
		const D3D12_RANGE write_range = { 0, 0 };

		void *mapped_data = nullptr;
		if (SUCCEEDED(ID3D12Resource_Map(extra_data.readback_resource, 0, &read_range, &mapped_data)))
		{
			for (size_t i = 0; i < count; ++i)
				*reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(results) + i * stride) = static_cast<uint64_t *>(mapped_data)[first + i];

			ID3D12Resource_Unmap(extra_data.readback_resource, 0, &write_range);

			return true;
		}
	}

	return false;
}

void reshade::d3d12::device_impl::set_resource_name(api::resource handle, const char *name)
{
	const size_t debug_name_len = std::strlen(name);
	std::wstring debug_name_wide;
	debug_name_wide.reserve(debug_name_len + 1);
	utf8::unchecked::utf8to16(name, name + debug_name_len, std::back_inserter(debug_name_wide));

	reinterpret_cast<ID3D12Resource *>(handle.handle)->SetName(debug_name_wide.c_str());
}

bool reshade::d3d12::device_impl::create_fence(uint64_t initial_value, api::fence_flags flags, api::fence *out_handle, HANDLE *shared_handle)
{
	*out_handle = { 0 };

	com_ptr<ID3D12Fence> object;

	const bool is_shared = (flags & api::fence_flags::shared) != 0;
	if (is_shared)
	{
		// Only NT handles are supported
		if (shared_handle == nullptr || (flags & api::fence_flags::shared_nt_handle) == 0)
			return false;

		if (*shared_handle != nullptr)
		{
			if (SUCCEEDED(_orig->OpenSharedHandle(*shared_handle, IID_PPV_ARGS(&object))))
			{
				*out_handle = to_handle(object.release());
				return true;
			}

			return false;
		}
	}

	if (SUCCEEDED(_orig->CreateFence(initial_value, convert_fence_flags(flags), IID_PPV_ARGS(&object))))
	{
		if (is_shared && FAILED(_orig->CreateSharedHandle(object.get(), nullptr, GENERIC_ALL, nullptr, shared_handle)))
			return false;

		*out_handle = to_handle(object.release());
		return true;
	}

	return false;
}
void reshade::d3d12::device_impl::destroy_fence(api::fence handle)
{
	if (handle.handle == 0)
		return;

	reinterpret_cast<ID3D12Fence *>(handle.handle)->Release();
}

uint64_t reshade::d3d12::device_impl::get_completed_fence_value(api::fence fence) const
{
	return reinterpret_cast<ID3D12Fence *>(fence.handle)->GetCompletedValue();
}

bool reshade::d3d12::device_impl::wait(api::fence fence, uint64_t value, uint64_t timeout)
{
	if (value <= reinterpret_cast<ID3D12Fence *>(fence.handle)->GetCompletedValue())
		return true;

	DWORD res = WAIT_FAILED;

	const HANDLE temp_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (SUCCEEDED(reinterpret_cast<ID3D12Fence *>(fence.handle)->SetEventOnCompletion(value, temp_event)))
		res = WaitForSingleObject(temp_event, (timeout == UINT64_MAX) ? INFINITE : (timeout / 1000000) & 0xFFFFFFFF);

	CloseHandle(temp_event);
	return res == WAIT_OBJECT_0;
}
bool reshade::d3d12::device_impl::signal(api::fence fence, uint64_t value)
{
	return SUCCEEDED(reinterpret_cast<ID3D12Fence *>(fence.handle)->Signal(value));
}

void reshade::d3d12::device_impl::register_resource(ID3D12Resource *resource)
{
	assert(resource != nullptr);

#if RESHADE_ADDON >= 2
	if (const D3D12_RESOURCE_DESC desc = resource->GetDesc();
		desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		if (const D3D12_GPU_VIRTUAL_ADDRESS address = resource->GetGPUVirtualAddress())
		{
			const std::unique_lock<std::shared_mutex> lock(_resource_mutex);

			_buffer_gpu_addresses.emplace_back(resource, D3D12_GPU_VIRTUAL_ADDRESS_RANGE { address, desc.Width });
		}
	}
#endif
}
void reshade::d3d12::device_impl::unregister_resource(ID3D12Resource *resource)
{
	assert(resource != nullptr);

#if RESHADE_ADDON >= 2
	if (const D3D12_RESOURCE_DESC desc = resource->GetDesc();
		desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		const std::unique_lock<std::shared_mutex> lock(_resource_mutex);

		if (const auto it = std::find_if(_buffer_gpu_addresses.begin(), _buffer_gpu_addresses.end(),
				[resource](const std::pair<ID3D12Resource *, D3D12_GPU_VIRTUAL_ADDRESS_RANGE> &buffer_info) {
					return buffer_info.first == resource;
				});
			it != _buffer_gpu_addresses.end())
		{
			_buffer_gpu_addresses.erase(it);
		}
	}
#endif

#if 0
	const std::unique_lock<std::shared_mutex> lock(_resource_mutex);

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

reshade::d3d12::command_list_immediate_impl *reshade::d3d12::device_impl::get_first_immediate_command_list()
{
	assert(!_queues.empty());

	for (command_queue_impl *const queue : _queues)
		if (const auto immediate_command_list = static_cast<command_list_immediate_impl *>(queue->get_immediate_command_list()))
			return immediate_command_list;
	return nullptr;
}

#if RESHADE_ADDON >= 2
bool reshade::d3d12::device_impl::resolve_gpu_address(D3D12_GPU_VIRTUAL_ADDRESS address, api::resource *out_resource, uint64_t *out_offset) const
{
	assert(out_offset != nullptr && out_resource != nullptr);

	*out_offset = 0;
	*out_resource = { 0 };

	if (!address)
		return true;

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

	assert(false);
	return false;
}

void reshade::d3d12::device_impl::register_descriptor_heap(D3D12DescriptorHeap *heap)
{
	const auto it = _descriptor_heaps.push_back(heap);

	heap->initialize_descriptor_base_handle(std::distance(_descriptor_heaps.begin(), it));
}
void reshade::d3d12::device_impl::unregister_descriptor_heap(D3D12DescriptorHeap *heap)
{
	size_t num_heaps = _descriptor_heaps.size();

	for (size_t heap_index = 0; heap_index < num_heaps; ++heap_index)
	{
		if (heap == _descriptor_heaps[heap_index])
		{
			_descriptor_heaps[heap_index] = nullptr;
			break;
		}
	}

	while (num_heaps != 0)
	{
		if (_descriptor_heaps[num_heaps - 1] == nullptr)
			num_heaps--;
		else
			break;
	}

	_descriptor_heaps.resize(num_heaps);
}

void D3D12DescriptorHeap::initialize_descriptor_base_handle(size_t heap_index)
{
	// Generate a descriptor handle of the following format:
	//   Bit  0 -  2: Heap type
	//   Bit  3 -  3: Heap flags
	//   Bit  4 - 27: Descriptor index
	//   Bit 28 - 55: Heap index
	//   Bit 56 - 64: Extra data

	_orig_base_cpu_handle = _orig->GetCPUDescriptorHandleForHeapStart();
	_internal_base_cpu_handle = { 0 };

	assert(heap_index < (1ull << std::min(sizeof(SIZE_T) * 8 - heap_index_start, heap_index_start)));
	_internal_base_cpu_handle.ptr |= static_cast<SIZE_T>(heap_index) << heap_index_start;

	const D3D12_DESCRIPTOR_HEAP_DESC heap_desc = _orig->GetDesc();
	assert(heap_desc.Type <= 0x3);
	_internal_base_cpu_handle.ptr |= heap_desc.Type;
	assert(heap_desc.Flags <= 0x1);
	_internal_base_cpu_handle.ptr |= heap_desc.Flags << 2;

#ifdef _WIN64
	static_assert((D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2 * 32) < (1 << heap_index_start));
#else
	if (heap_index >= (1ull << std::min(sizeof(SIZE_T) * 8 - heap_index_start, heap_index_start)))
	{
		LOG(ERROR) << "Descriptor heap index is too big to fit into handle!";
	}
#endif
	if (_device->GetDescriptorHandleIncrementSize(heap_desc.Type) < (1 << 3))
	{
		assert(false);
		LOG(ERROR) << "Descriptor heap contains descriptors that are too small!";
	}
	if ((heap_desc.NumDescriptors * _device->GetDescriptorHandleIncrementSize(heap_desc.Type)) >= (1 << heap_index_start))
	{
		assert(false);
		LOG(ERROR) << "Descriptor heap contains too many descriptors to fit into handle!";
	}

	if (heap_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		_orig_base_gpu_handle = _orig->GetGPUDescriptorHandleForHeapStart();
	}
}
#endif

D3D12_CPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_cpu_descriptor_handle(api::descriptor_table table, D3D12_DESCRIPTOR_HEAP_TYPE *type) const
{
#if RESHADE_ADDON >= 2
	// Check if this is a D3D12_CPU_DESCRIPTOR_HANDLE or D3D12_GPU_DESCRIPTOR_HANDLE
	if ((table.handle & 0xF000000000000000ull) == 0xF000000000000000ull)
	{
		const size_t heap_index = (table.handle >> heap_index_start) & 0xFFFFFFF;
		if (type != nullptr)
			*type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(table.handle & 0x3);
		assert(heap_index < _descriptor_heaps.size() && _descriptor_heaps[heap_index] != nullptr);

		return { _descriptor_heaps[heap_index]->_orig_base_cpu_handle.ptr + (table.handle & (((1ull << heap_index_start) - 1) ^ 0x7)) };
	}
#endif

	D3D12_CPU_DESCRIPTOR_HANDLE handle = { 0 };
	const D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu = { table.handle };

#if RESHADE_ADDON >= 2
	for (D3D12DescriptorHeap *const heap_impl : _descriptor_heaps)
	{
		if (heap_impl == nullptr || handle_gpu.ptr < heap_impl->_orig_base_gpu_handle.ptr)
			continue;

		D3D12_DESCRIPTOR_HEAP_DESC desc = heap_impl->_orig->GetDesc();
		if (handle_gpu.ptr >= offset_descriptor_handle(heap_impl->_orig_base_gpu_handle, desc.NumDescriptors, desc.Type).ptr)
			continue;

		handle.ptr = heap_impl->_orig_base_cpu_handle.ptr + static_cast<SIZE_T>(handle_gpu.ptr - heap_impl->_orig_base_gpu_handle.ptr);

		if (type != nullptr)
			*type = desc.Type;
		break;
	}
#else
	if (_gpu_view_heap.contains(handle_gpu))
	{
		if (type != nullptr)
			*type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		_gpu_view_heap.convert_handle(handle_gpu, handle);
	}
	else if (_gpu_sampler_heap.contains(handle_gpu))
	{
		if (type != nullptr)
			*type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		_gpu_sampler_heap.convert_handle(handle_gpu, handle);
	}
#endif

	assert(handle.ptr != 0);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE reshade::d3d12::device_impl::convert_to_original_gpu_descriptor_handle(api::descriptor_table table) const
{
#if RESHADE_ADDON >= 2
	// Check if this is a D3D12_CPU_DESCRIPTOR_HANDLE or D3D12_GPU_DESCRIPTOR_HANDLE
	if ((table.handle & 0xF000000000000000ull) == 0xF000000000000000ull)
	{
		const size_t heap_index = (table.handle >> heap_index_start) & 0xFFFFFFF;
		assert(heap_index < _descriptor_heaps.size() && _descriptor_heaps[heap_index] != nullptr);

		return { _descriptor_heaps[heap_index]->_orig_base_gpu_handle.ptr + (table.handle & (((1ull << heap_index_start) - 1) ^ 0x7)) };
	}
#endif

	return { table.handle };
}
