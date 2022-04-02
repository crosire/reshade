/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "d3d12_device_downlevel.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"

using reshade::d3d12::to_handle;

D3D12Device::D3D12Device(ID3D12Device *original) :
	device_impl(original)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D12Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D12Device), sizeof(device_proxy), &device_proxy);
}

bool D3D12Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(ID3D12Object))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D12Device),
		__uuidof(ID3D12Device1),
		__uuidof(ID3D12Device2),
		__uuidof(ID3D12Device3),
		__uuidof(ID3D12Device4),
		__uuidof(ID3D12Device5),
		__uuidof(ID3D12Device6),
		__uuidof(ID3D12Device7),
		__uuidof(ID3D12Device8),
		__uuidof(ID3D12Device9),
	};

	for (unsigned int version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			LOG(DEBUG) << "Upgrading ID3D12Device" << _interface_version << " object " << this << " to ID3D12Device" << version << '.';
#endif
			_orig->Release();
			_orig = static_cast<ID3D12Device *>(new_interface);
			_interface_version = version;
		}

		return true;
	}

	return false;
}

HRESULT STDMETHODCALLTYPE D3D12Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Special case for d3d12on7
	if (riid == __uuidof(ID3D12DeviceDownlevel))
	{
		if (ID3D12DeviceDownlevel *downlevel = nullptr; // Not a 'com_ptr' since D3D12DeviceDownlevel will take ownership
			_downlevel == nullptr && SUCCEEDED(_orig->QueryInterface(&downlevel)))
			_downlevel = new D3D12DeviceDownlevel(this, downlevel);
		if (_downlevel != nullptr)
			return _downlevel->QueryInterface(riid, ppvObj);
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12Device::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12Device::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
	{
		_orig->Release();
		return ref;
	}

	if (_downlevel != nullptr)
	{
		// Release the reference that was added when the downlevel interface was first queried in 'QueryInterface' above
		_downlevel->_orig->Release();
		delete _downlevel;
	}

	const auto orig = _orig;
	const auto interface_version = _interface_version;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "ID3D12Device" << interface_version << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D12Device" << interface_version << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetName(LPCWSTR Name)
{
	return _orig->SetName(Name);
}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
#include "hook_manager.hpp"

static HRESULT STDMETHODCALLTYPE ID3D12Resource_Map(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData)
{
	const HRESULT hr = reshade::hooks::call(ID3D12Resource_Map, vtable_from_instance(pResource) + 8)(pResource, Subresource, pReadRange, ppData);
	if (SUCCEEDED(hr))
	{
		com_ptr<ID3D12Device> device;
		pResource->GetDevice(IID_PPV_ARGS(&device));

		if (const auto device_proxy = get_private_pointer<D3D12Device>(device.get()))
		{
			const D3D12_RESOURCE_DESC desc = pResource->GetDesc();

			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{
				assert(Subresource == 0);

				reshade::invoke_addon_event<reshade::addon_event::map_buffer_region>(
					device_proxy,
					to_handle(pResource),
					0,
					std::numeric_limits<uint64_t>::max(),
					reshade::api::map_access::read_write,
					ppData);
			}
			else if (ppData != nullptr)
			{
				reshade::api::subresource_data data;
				data.data = *ppData;

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
				device->GetCopyableFootprints(&desc, Subresource, 1, 0, &layout, &data.slice_pitch, nullptr, nullptr);
				data.row_pitch = layout.Footprint.RowPitch;
				data.slice_pitch *= layout.Footprint.RowPitch;

				reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
					device_proxy,
					to_handle(pResource),
					Subresource,
					nullptr,
					reshade::api::map_access::read_write,
					&data);

				*ppData = data.data;
			}
			else
			{
				reshade::invoke_addon_event<reshade::addon_event::map_texture_region>(
					device_proxy,
					to_handle(pResource),
					Subresource,
					nullptr,
					reshade::api::map_access::read_write,
					nullptr);
			}
		}
	}

	return hr;
}
static HRESULT STDMETHODCALLTYPE ID3D12Resource_Unmap(ID3D12Resource *pResource, UINT Subresource, const D3D12_RANGE *pWrittenRange)
{
	com_ptr<ID3D12Device> device;
	pResource->GetDevice(IID_PPV_ARGS(&device));

	if (const auto device_proxy = get_private_pointer<D3D12Device>(device.get()))
	{
		const D3D12_RESOURCE_DESC desc = pResource->GetDesc();

		if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			assert(Subresource == 0);

			reshade::invoke_addon_event<reshade::addon_event::unmap_buffer_region>(device_proxy, to_handle(pResource));
		}
		else
		{
			reshade::invoke_addon_event<reshade::addon_event::unmap_texture_region>(device_proxy, to_handle(pResource),	Subresource);
		}
	}

	return reshade::hooks::call(ID3D12Resource_Unmap, vtable_from_instance(pResource) + 9)(pResource, Subresource, pWrittenRange);
}
#endif

UINT    STDMETHODCALLTYPE D3D12Device::GetNodeCount()
{
	return _orig->GetNodeCount();
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid, void **ppCommandQueue)
{
	LOG(INFO) << "Redirecting " << "ID3D12Device::CreateCommandQueue" << '(' << "this = " << this << ", pDesc = " << pDesc << ", riid = " << riid << ", ppCommandQueue = " << ppCommandQueue << ')' << " ...";

	if (pDesc == nullptr)
		return E_INVALIDARG;

	LOG(INFO) << "> Dumping command queue description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Type                                    | " << std::setw(39) << pDesc->Type << " |";
	LOG(INFO) << "  | Priority                                | " << std::setw(39) << pDesc->Priority << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << pDesc->Flags << std::dec << " |";
	LOG(INFO) << "  | NodeMask                                | " << std::setw(39) << std::hex << pDesc->NodeMask << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	const HRESULT hr = _orig->CreateCommandQueue(pDesc, riid, ppCommandQueue);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandQueue != nullptr);

		const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

		// Upgrade to the actual interface version requested here
		if (command_queue_proxy->check_and_upgrade_interface(riid))
		{
#if RESHADE_VERBOSE_LOG
			LOG(INFO) << "> Returning ID3D12CommandQueue" << " object " << command_queue_proxy << " (" << command_queue_proxy->_orig << ").";
#endif
			*ppCommandQueue = command_queue_proxy;
		}
		else // Do not hook object if we do not support the requested interface
		{
			LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12Device::CreateCommandQueue" << '.';

			delete command_queue_proxy; // Delete instead of release to keep reference count untouched
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateCommandQueue" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator)
{
	return _orig->CreateCommandAllocator(type, riid, ppCommandAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppPipelineState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGraphicsPipelineState(pDesc, riid, nullptr);

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc = *pDesc;

	reshade::api::shader_desc vs_desc = reshade::d3d12::convert_shader_desc(internal_desc.VS);
	reshade::api::shader_desc ps_desc = reshade::d3d12::convert_shader_desc(internal_desc.PS);
	reshade::api::shader_desc ds_desc = reshade::d3d12::convert_shader_desc(internal_desc.DS);
	reshade::api::shader_desc hs_desc = reshade::d3d12::convert_shader_desc(internal_desc.HS);
	reshade::api::shader_desc gs_desc = reshade::d3d12::convert_shader_desc(internal_desc.GS);
	auto stream_output_desc = reshade::d3d12::convert_stream_output_desc(internal_desc.StreamOutput);
	auto blend_desc = reshade::d3d12::convert_blend_desc(internal_desc.BlendState);
	auto rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(internal_desc.RasterizerState);
	auto depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(internal_desc.DepthStencilState);
	auto input_layout = reshade::d3d12::convert_input_layout_desc(internal_desc.InputLayout.NumElements, internal_desc.InputLayout.pInputElementDescs);
	reshade::api::primitive_topology topology = reshade::d3d12::convert_primitive_topology_type(internal_desc.PrimitiveTopologyType);

	reshade::api::format depth_stencil_format = reshade::d3d12::convert_format(internal_desc.DSVFormat);
	assert(internal_desc.NumRenderTargets <= 8);
	reshade::api::format render_target_formats[8] = {};
	for (UINT i = 0; i < internal_desc.NumRenderTargets; ++i)
		render_target_formats[i] = reshade::d3d12::convert_format(internal_desc.RTVFormats[i]);

	uint32_t sample_mask = internal_desc.SampleMask;
	uint32_t sample_count = internal_desc.SampleDesc.Count;

	reshade::api::dynamic_state dynamic_states[3] = { reshade::api::dynamic_state::primitive_topology, reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::stencil_reference_value };

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc },
		{ reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc },
		{ reshade::api::pipeline_subobject_type::domain_shader, 1, &ds_desc },
		{ reshade::api::pipeline_subobject_type::hull_shader, 1, &hs_desc },
		{ reshade::api::pipeline_subobject_type::geometry_shader, 1, &gs_desc },
		{ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc },
		{ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc },
		{ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask },
		{ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc },
		{ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc },
		{ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() },
		{ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology },
		{ reshade::api::pipeline_subobject_type::render_target_formats, internal_desc.NumRenderTargets, render_target_formats },
		{ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format },
		{ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count },
		{ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(std::size(dynamic_states)), dynamic_states },
	};

	HRESULT hr = S_OK;
	if (riid == __uuidof(ID3D12PipelineState) &&
		reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::api::pipeline pipeline;
		hr = create_pipeline(to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, &pipeline) ? S_OK : E_FAIL;
		*ppPipelineState = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
	}
	else
	{
		hr = _orig->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);
	}
#else
	const HRESULT hr = _orig->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);
#endif

	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		if (riid == __uuidof(ID3D12PipelineState))
		{
			const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

			register_destruction_callback(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateGraphicsPipelineState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppPipelineState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateComputePipelineState(pDesc, riid, nullptr);

	const D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc = *pDesc;

	reshade::api::shader_desc cs_desc = reshade::d3d12::convert_shader_desc(internal_desc.CS);

	const reshade::api::pipeline_subobject subobjects[] = {
		{ reshade::api::pipeline_subobject_type::compute_shader, 1, &cs_desc }
	};

	HRESULT hr = S_OK;
	if (riid == __uuidof(ID3D12PipelineState) &&
		reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects))
	{
		reshade::api::pipeline handle;
		hr = create_pipeline(to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, &handle) ? S_OK : E_FAIL;
		*ppPipelineState = reinterpret_cast<ID3D12PipelineState *>(handle.handle);
	}
	else
	{
		hr = _orig->CreateComputePipelineState(pDesc, riid, ppPipelineState);
	}
#else
	const HRESULT hr = _orig->CreateComputePipelineState(pDesc, riid, ppPipelineState);
#endif

	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		if (riid == __uuidof(ID3D12PipelineState))
		{
			const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, to_handle(internal_desc.pRootSignature), static_cast<uint32_t>(std::size(subobjects)), subobjects, to_handle(pipeline));

			register_destruction_callback(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateComputePipelineState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **ppCommandList)
{
	const HRESULT hr = _orig->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandList != nullptr);

		const auto command_list_proxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList *>(*ppCommandList));

		// Upgrade to the actual interface version requested here (and only hook graphics command lists)
		if (command_list_proxy->check_and_upgrade_interface(riid))
		{
			*ppCommandList = command_list_proxy;

#if RESHADE_ADDON
			// Same implementation as in 'D3D12GraphicsCommandList::Reset'
			reshade::invoke_addon_event<reshade::addon_event::reset_command_list>(command_list_proxy);

			if (pInitialState != nullptr)
				reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(command_list_proxy, reshade::api::pipeline_stage::all, to_handle(pInitialState));
#endif
		}
		else // Do not hook object if we do not support the requested interface
		{
			LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12Device::CreateCommandList" << '.';

			delete command_list_proxy; // Delete instead of release to keep reference count untouched
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateCommandList" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return _orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Redirecting " << "ID3D12Device::CreateDescriptorHeap" << '(' << "this = " << this << ", pDescriptorHeapDesc = " << pDescriptorHeapDesc << ", riid = " << riid << ", ppvHeap = " << ppvHeap << ')' << " ...";
#endif

	if (pDescriptorHeapDesc == nullptr)
		return E_INVALIDARG;

	const HRESULT hr = _orig->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		if (ppvHeap != nullptr && pDescriptorHeapDesc->Type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			const auto descriptor_heap_proxy = new D3D12DescriptorHeap(this, static_cast<ID3D12DescriptorHeap *>(*ppvHeap));

			// Upgrade to the actual interface version requested here
			if (descriptor_heap_proxy->check_and_upgrade_interface(riid))
			{
				register_descriptor_heap(descriptor_heap_proxy);

				register_destruction_callback(descriptor_heap_proxy, [this, descriptor_heap_proxy]() {
					unregister_descriptor_heap(descriptor_heap_proxy);
				});

#if RESHADE_VERBOSE_LOG
				LOG(INFO) << "> Returning ID3D12DescriptorHeap" << " object " << descriptor_heap_proxy << " (" << descriptor_heap_proxy->_orig << ").";
#endif
				*ppvHeap = descriptor_heap_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12Device::CreateDescriptorHeap" << '.';

				delete descriptor_heap_proxy; // Delete instead of release to keep reference count untouched
			}
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateDescriptorHeap" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
UINT    STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
	return _orig->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **ppvRootSignature)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (ppvRootSignature == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, nullptr);
#endif

	const HRESULT hr = _orig->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
		if (riid == __uuidof(ID3D12RootSignature))
		{
			const auto data = static_cast<const uint32_t *>(pBlobWithRootSignature);

			// Parse DXBC root signature, convert it and call descriptor set and pipeline layout events
			constexpr uint32_t DXBC = uint32_t('D') | (uint32_t('X') << 8) | (uint32_t('B') << 16) | (uint32_t('C') << 24);
			constexpr uint32_t RTS0 = uint32_t('R') | (uint32_t('T') << 8) | (uint32_t('S') << 16) | (uint32_t('0') << 24);

			if (blobLengthInBytes >= (sizeof(uint32_t) * 8) &&
				data[0] == DXBC &&
				data[5] == 0x00000001 /* DXBC version */)
			{
				assert((blobLengthInBytes % sizeof(uint32_t)) == 0 && blobLengthInBytes == data[6]);

				std::vector<reshade::api::pipeline_layout_param> params;
				std::vector<std::vector<reshade::api::descriptor_range>> ranges;

				for (uint32_t i = 0; i < data[7]; ++i)
				{
					const uint32_t chunk_offset = data[8 + i], *chunk = data + (chunk_offset / sizeof(uint32_t));

					if (chunk[0] != RTS0)
						continue;
					else
						chunk += 2;

					const uint32_t version = chunk[0];
					if (version != D3D_ROOT_SIGNATURE_VERSION_1_0 && version != D3D_ROOT_SIGNATURE_VERSION_1_1)
						continue;

					const uint32_t param_count = chunk[1];
					const uint32_t param_offset = chunk[2];
					auto param_list = chunk + (param_offset / sizeof(uint32_t));

					params.resize(param_count);
					ranges.resize(param_count);

					for (uint32_t k = 0; k < param_count; ++k, param_list += 3)
					{
						const auto param_type = static_cast<D3D12_ROOT_PARAMETER_TYPE>(param_list[0]);
						const auto shader_visibility = static_cast<D3D12_SHADER_VISIBILITY>(param_list[1]);
						auto param_data = chunk + (param_list[2] / sizeof(uint32_t));

						switch (param_type)
						{
							case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
							{
								const uint32_t range_count = param_data[0];
								uint32_t descriptor_offset = 0;

								ranges[k].resize(range_count);

								// Convert descriptor ranges
								if (version == D3D_ROOT_SIGNATURE_VERSION_1_0)
								{
									auto range_data = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE *>(chunk + (param_data[1] / sizeof(uint32_t)));

									for (uint32_t j = 0; j < range_count; ++j, ++range_data)
									{
										reshade::api::descriptor_range &range = ranges[k][j];
										range.dx_register_index = range_data->BaseShaderRegister;
										range.dx_register_space = range_data->RegisterSpace;
										range.count = range_data->NumDescriptors;
										range.array_size = 1;
										range.type = reshade::d3d12::convert_descriptor_type(range_data->RangeType);
										range.visibility = reshade::d3d12::convert_shader_visibility(shader_visibility);

										if (range_data->OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
											range.binding = descriptor_offset;
										else
											range.binding = range_data->OffsetInDescriptorsFromTableStart;

										descriptor_offset = range.binding + range.count;
									}
								}
								if (version == D3D_ROOT_SIGNATURE_VERSION_1_1)
								{
									auto range_data = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE1 *>(chunk + (param_data[1] / sizeof(uint32_t)));

									for (uint32_t j = 0; j < range_count; ++j, ++range_data)
									{
										reshade::api::descriptor_range &range = ranges[k][j];
										range.dx_register_index = range_data->BaseShaderRegister;
										range.dx_register_space = range_data->RegisterSpace;
										range.count = range_data->NumDescriptors;
										range.array_size = 1;
										range.type = reshade::d3d12::convert_descriptor_type(range_data->RangeType);
										range.visibility = reshade::d3d12::convert_shader_visibility(shader_visibility);

										if (range_data->OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
											range.binding = descriptor_offset;
										else
											range.binding = range_data->OffsetInDescriptorsFromTableStart;

										descriptor_offset = range.binding + range.count;
									}
								}

								params[k].type = reshade::api::pipeline_layout_param_type::descriptor_set;
								params[k].descriptor_set.count = range_count;
								params[k].descriptor_set.ranges = ranges[k].data();
								break;
							}
							case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
							{
								auto constant_data = reinterpret_cast<const D3D12_ROOT_CONSTANTS *>(param_data);

								params[k].type = reshade::api::pipeline_layout_param_type::push_constants;

								// Convert root constant description
								reshade::api::constant_range &root_constant = params[k].push_constants;
								root_constant.binding = 0;
								root_constant.dx_register_index = constant_data->ShaderRegister;
								root_constant.dx_register_space = constant_data->RegisterSpace;
								root_constant.count = constant_data->Num32BitValues;
								root_constant.visibility = reshade::d3d12::convert_shader_visibility(shader_visibility);
								break;
							}
							case D3D12_ROOT_PARAMETER_TYPE_CBV:
							case D3D12_ROOT_PARAMETER_TYPE_SRV:
							case D3D12_ROOT_PARAMETER_TYPE_UAV:
							{
								auto descriptor_data = reinterpret_cast<const D3D12_ROOT_DESCRIPTOR *>(param_data);

								params[k].type = reshade::api::pipeline_layout_param_type::push_descriptors;

								reshade::api::descriptor_range &range = params[k].push_descriptors;
								range.binding = 0;
								range.dx_register_index = descriptor_data->ShaderRegister;
								range.dx_register_space = descriptor_data->RegisterSpace;
								range.count = 1;
								range.array_size = 1;
								range.visibility = reshade::d3d12::convert_shader_visibility(shader_visibility);

								if (param_type == D3D12_ROOT_PARAMETER_TYPE_CBV)
									range.type = reshade::api::descriptor_type::constant_buffer;
								else if (param_type == D3D12_ROOT_PARAMETER_TYPE_SRV)
									range.type = reshade::api::descriptor_type::shader_resource_view;
								else
									range.type = reshade::api::descriptor_type::unordered_access_view;
								break;
							}
						}
					}
					break;
				}

				const auto root_signature = static_cast<ID3D12RootSignature *>(*ppvRootSignature);

				reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(this, static_cast<uint32_t>(params.size()), params.data(), reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(root_signature) });

				register_destruction_callback(root_signature, [this, root_signature]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(this, reshade::api::pipeline_layout { reinterpret_cast<uintptr_t>(root_signature) });
				});
			}
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateRootSignature" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
void    STDMETHODCALLTYPE D3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	_orig->CreateConstantBufferView(pDesc, convert_to_original_cpu_descriptor_handle(DestDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
		return;

	reshade::api::buffer_range buffer_range;
	if (pDesc == nullptr || !resolve_gpu_address(pDesc->BufferLocation, &buffer_range.buffer, &buffer_range.offset))
		return;
	buffer_range.size = pDesc->SizeInBytes;

	reshade::api::descriptor_set_update update;
	update.set = convert_to_descriptor_set(DestDescriptor);
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::constant_buffer;
	update.count = 1;
	update.descriptors = &buffer_range;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(this, 1, &update);
#else
	_orig->CreateConstantBufferView(pDesc, DestDescriptor);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	D3D12_SHADER_RESOURCE_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D12_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d12::convert_resource_view_desc(internal_desc);

	// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
	if (pResource != nullptr &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = convert_to_original_cpu_descriptor_handle(DestDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#else
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = DestDescriptor;
#endif
	_orig->CreateShaderResourceView(pResource, pDesc, original_descriptor_handle);

	const reshade::api::resource_view descriptor_value = to_handle(original_descriptor_handle);

	register_resource_view(original_descriptor_handle, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::shader_resource, desc, descriptor_value);
#else
	_orig->CreateShaderResourceView(pResource, pDesc, DestDescriptor);
#endif

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
		return;

	reshade::api::descriptor_set_update update;
	update.set = convert_to_descriptor_set(DestDescriptor);
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::shader_resource_view;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(this, 1, &update);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	D3D12_UNORDERED_ACCESS_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D12_UNORDERED_ACCESS_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d12::convert_resource_view_desc(internal_desc);

	// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview)
	if (pResource != nullptr &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::unordered_access, desc))
	{
		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = convert_to_original_cpu_descriptor_handle(DestDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#else
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = DestDescriptor;
#endif
	_orig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, original_descriptor_handle);

	const reshade::api::resource_view descriptor_value = to_handle(original_descriptor_handle);

	register_resource_view(original_descriptor_handle, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::unordered_access, desc, descriptor_value);
#else
	_orig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);
#endif

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
		return;

	reshade::api::descriptor_set_update update;
	update.set = convert_to_descriptor_set(DestDescriptor);
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::unordered_access_view;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(this, 1, &update);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	D3D12_RENDER_TARGET_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D12_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_RTV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d12::convert_resource_view_desc(internal_desc);

	// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createrendertargetview)
	if (pResource != nullptr &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::render_target, desc))
	{
		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	_orig->CreateRenderTargetView(pResource, pDesc, DestDescriptor);

#if RESHADE_ADDON
	register_resource_view(DestDescriptor, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::render_target, desc, to_handle(DestDescriptor));
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	D3D12_DEPTH_STENCIL_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D12_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_DSV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d12::convert_resource_view_desc(internal_desc);

	// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdepthstencilview)
	if (pResource != nullptr &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::depth_stencil, desc))
	{
		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	_orig->CreateDepthStencilView(pResource, pDesc, DestDescriptor);

#if RESHADE_ADDON
	register_resource_view(DestDescriptor, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::depth_stencil, desc, to_handle(DestDescriptor));
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	if (pDesc == nullptr) // Not allowed in D3D12
		return;

	D3D12_SAMPLER_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_sampler_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
	{
		reshade::d3d12::convert_sampler_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = convert_to_original_cpu_descriptor_handle(DestDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
#else
	const D3D12_CPU_DESCRIPTOR_HANDLE original_descriptor_handle = DestDescriptor;
#endif
	_orig->CreateSampler(pDesc, original_descriptor_handle);

	const reshade::api::sampler descriptor_value = { original_descriptor_handle.ptr };

	reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, descriptor_value);
#else
	_orig->CreateSampler(pDesc, DestDescriptor);
#endif

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_sets>())
		return;

	reshade::api::descriptor_set_update update;
	update.set = convert_to_descriptor_set(DestDescriptor);
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::sampler;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_sets>(this, 1, &update);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptors(UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts, const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts, const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	// All elements in the source ranges must be in a non shader-visible descriptor heap, so can treat this like an update
	if (DescriptorHeapsType <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER &&
		reshade::has_addon_event<reshade::addon_event::copy_descriptor_sets>())
	{
		uint32_t num_copies = 0;
		for (UINT i = 0; i < NumDestDescriptorRanges; ++i)
			num_copies += (pDestDescriptorRangeSizes != nullptr ? pDestDescriptorRangeSizes[i] : 1);
		temp_mem<reshade::api::descriptor_set_copy, 32> copies(num_copies);

		num_copies = 0;

		for (UINT dst_range = 0, src_range = 0, dst_offset = 0, src_offset = 0; dst_range < NumDestDescriptorRanges; ++dst_range, dst_offset = 0)
		{
			const UINT dst_count = (pDestDescriptorRangeSizes != nullptr ? pDestDescriptorRangeSizes[dst_range] : 1);

			while (dst_offset < dst_count)
			{
				const UINT src_count = (pSrcDescriptorRangeSizes != nullptr ? pSrcDescriptorRangeSizes[src_range] : 1);

				copies[num_copies].dest_set = convert_to_descriptor_set(pDestDescriptorRangeStarts[dst_range]);
				copies[num_copies].dest_binding = 0;
				copies[num_copies].dest_array_offset = 0;
				copies[num_copies].source_set = convert_to_descriptor_set(pSrcDescriptorRangeStarts[src_range]);
				copies[num_copies].source_binding = 0;
				copies[num_copies].source_array_offset = 0;

				if (src_count <= (dst_count - dst_offset))
				{
					copies[num_copies].count = src_count;

					src_range += 1;
					src_offset = 0;
					dst_offset += src_count;
				}
				else
				{
					copies[num_copies].count = 1;

					src_offset += 1;
					dst_offset += 1;

					if (src_offset >= src_count)
					{
						src_range += 1;
						src_offset = 0;
					}
				}

				num_copies++;
			}
		}

		if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_sets>(this, num_copies, copies.p))
			return;
	}

	temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE, 32> descriptor_range_starts(NumDestDescriptorRanges + NumSrcDescriptorRanges);
	for (UINT i = 0; i < NumSrcDescriptorRanges; ++i)
		descriptor_range_starts[i] = convert_to_original_cpu_descriptor_handle(pSrcDescriptorRangeStarts[i], DescriptorHeapsType);
	for (UINT i = 0; i < NumDestDescriptorRanges; ++i)
		descriptor_range_starts[NumSrcDescriptorRanges + i] = convert_to_original_cpu_descriptor_handle(pDestDescriptorRangeStarts[i], DescriptorHeapsType);

	_orig->CopyDescriptors(NumDestDescriptorRanges, descriptor_range_starts.p + NumSrcDescriptorRanges, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, descriptor_range_starts.p, pSrcDescriptorRangeSizes, DescriptorHeapsType);
#else
	_orig->CopyDescriptors(NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	if (DescriptorHeapsType <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER &&
		reshade::has_addon_event<reshade::addon_event::copy_descriptor_sets>())
	{
		reshade::api::descriptor_set_copy copy;
		copy.dest_set = convert_to_descriptor_set(DestDescriptorRangeStart);
		copy.source_set = convert_to_descriptor_set(SrcDescriptorRangeStart);
		copy.count = NumDescriptors;

		if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_sets>(this, 1, &copy))
			return;
	}

	_orig->CopyDescriptorsSimple(NumDescriptors, convert_to_original_cpu_descriptor_handle(DestDescriptorRangeStart, DescriptorHeapsType), convert_to_original_cpu_descriptor_handle(SrcDescriptorRangeStart, DescriptorHeapsType), DescriptorHeapsType);
#else
	_orig->CopyDescriptorsSimple(NumDescriptors, DestDescriptorRangeStart, SrcDescriptorRangeStart, DescriptorHeapsType);
#endif
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs)
{
	return _orig->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}
D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE D3D12Device::GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType)
{
	return _orig->GetCustomHeapProperties(nodeMask, heapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
#if RESHADE_ADDON
	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, riidResource, nullptr);

	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialResourceState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riidResource == __uuidof(ID3D12Resource) ||
			riidResource == __uuidof(ID3D12Resource1) ||
			riidResource == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
			if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("ID3D12Resource::Map", vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
			if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("ID3D12Resource::Unmap", vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateCommittedResource" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
	return _orig->CreateHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
#if RESHADE_ADDON
	if (pHeap == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, nullptr);

	const auto heap_desc = pHeap->GetDesc();

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_desc.Properties, heap_desc.Flags);
	assert(desc.heap != reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
			if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("ID3D12Resource::Map", vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
			if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("ID3D12Resource::Unmap", vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreatePlacedResource" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, nullptr);

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateReservedResource" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
	return _orig->CreateSharedHandle(pObject, pAttributes, Access, Name, pHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
	const HRESULT hr = _orig->OpenSharedHandle(NTHandle, riid, ppvObj);
	if (SUCCEEDED(hr))
	{
		assert(ppvObj != nullptr);

#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvObj);

			D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
			D3D12_HEAP_PROPERTIES heap_props = {};
			resource->GetHeapProperties(&heap_props, &heap_flags);

			const reshade::api::resource_desc desc = reshade::d3d12::convert_resource_desc(resource->GetDesc(), heap_props, heap_flags);

			assert((desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared);

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::OpenSharedHandle" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
	return _orig->OpenSharedHandleByName(Name, Access, pNTHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
	return _orig->MakeResident(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
	return _orig->Evict(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void **ppFence)
{
	return _orig->CreateFence(InitialValue, Flags, riid, ppFence);
}
HRESULT STDMETHODCALLTYPE D3D12Device::GetDeviceRemovedReason()
{
	return _orig->GetDeviceRemovedReason();
}
void    STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints(const D3D12_RESOURCE_DESC *pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts, UINT *pNumRows, UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes)
{
	_orig->GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvHeap == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateQueryHeap(pDesc, riid, nullptr);

	D3D12_QUERY_HEAP_DESC internal_desc = *pDesc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_query_pool>(this, reshade::d3d12::convert_query_heap_type_to_type(internal_desc.Type), internal_desc.Count))
	{
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateQueryHeap(pDesc, riid, ppvHeap);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12QueryHeap))
		{
			const auto query_heap = static_cast<ID3D12QueryHeap *>(*ppvHeap);

			reshade::invoke_addon_event<reshade::addon_event::init_query_pool>(this, reshade::d3d12::convert_query_heap_type_to_type(internal_desc.Type), internal_desc.Count, to_handle(query_heap));

			register_destruction_callback(query_heap, [this, query_heap]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_query_pool>(this, to_handle(query_heap));
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device::CreateQueryHeap" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetStablePowerState(BOOL Enable)
{
	return _orig->SetStablePowerState(Enable);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc, ID3D12RootSignature *pRootSignature, REFIID riid, void **ppvCommandSignature)
{
	return _orig->CreateCommandSignature(pDesc, pRootSignature, riid, ppvCommandSignature);
}
void    STDMETHODCALLTYPE D3D12Device::GetResourceTiling(ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource, D3D12_PACKED_MIP_INFO *pPackedMipDesc, D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips, UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
	_orig->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips, pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}
LUID    STDMETHODCALLTYPE D3D12Device::GetAdapterLuid()
{
	return _orig->GetAdapterLuid();
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePipelineLibrary(const void *pLibraryBlob, SIZE_T BlobLength, REFIID riid, void **ppPipelineLibrary)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D12Device1 *>(_orig)->CreatePipelineLibrary(pLibraryBlob, BlobLength, riid, ppPipelineLibrary);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetEventOnMultipleFenceCompletion(ID3D12Fence *const *ppFences, const UINT64 *pFenceValues, UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D12Device1 *>(_orig)->SetEventOnMultipleFenceCompletion(ppFences, pFenceValues, NumFences, Flags, hEvent);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetResidencyPriority(UINT NumObjects, ID3D12Pageable *const *ppObjects, const D3D12_RESIDENCY_PRIORITY *pPriorities)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D12Device1 *>(_orig)->SetResidencyPriority(NumObjects, ppObjects, pPriorities);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	assert(_interface_version >= 2);

#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppPipelineState == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(pDesc, riid, nullptr);

	reshade::api::pipeline_layout layout = {};

	reshade::api::shader_desc vs_desc, ps_desc, ds_desc, hs_desc, gs_desc, cs_desc;
	reshade::api::stream_output_desc stream_output_desc;
	reshade::api::blend_desc blend_desc;
	reshade::api::rasterizer_desc rasterizer_desc;
	reshade::api::depth_stencil_desc depth_stencil_desc;
	std::vector<reshade::api::input_element> input_layout;
	reshade::api::primitive_topology topology;
	reshade::api::format depth_stencil_format;
	reshade::api::format render_target_formats[8];
	uint32_t sample_mask;
	uint32_t sample_count;

	std::vector<reshade::api::pipeline_subobject> subobjects;

	for (uintptr_t p = reinterpret_cast<uintptr_t>(pDesc->pPipelineStateSubobjectStream); p < (reinterpret_cast<uintptr_t>(pDesc->pPipelineStateSubobjectStream) + pDesc->SizeInBytes);)
	{
		switch (*reinterpret_cast<const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE *>(p))
		{
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
			layout = to_handle(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE *>(p)->data);
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
			vs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_VS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_VS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
			ps_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_PS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_PS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
			ds_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::domain_shader, 1, &ds_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_DS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
			hs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_HS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::hull_shader, 1, &hs_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_HS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
			gs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_GS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::geometry_shader, 1, &gs_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_GS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
			cs_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_CS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::compute_shader, 1, &cs_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_CS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
			stream_output_desc = reshade::d3d12::convert_stream_output_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
			blend_desc = reshade::d3d12::convert_blend_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_BLEND_DESC *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_BLEND_DESC);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
			sample_mask = reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK *>(p)->data;
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
			rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
			input_layout = reshade::d3d12::convert_input_layout_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.NumElements, reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.pInputElementDescs);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data() });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
			topology = reshade::d3d12::convert_primitive_topology_type(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
			assert(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets <= 8);
			for (UINT i = 0; i < reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets; ++i)
				render_target_formats[i] = reshade::d3d12::convert_format(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.RTFormats[i]);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::render_target_formats, reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets, render_target_formats });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
			depth_stencil_format = reshade::d3d12::convert_format(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
			sample_count = reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC *>(p)->data.Count;
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_NODE_MASK);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_CACHED_PSO);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_FLAGS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
			assert(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_VIEW_INSTANCING *>(p)->data.ViewInstanceCount <= D3D12_MAX_VIEW_INSTANCE_COUNT);
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_VIEW_INSTANCING);
			continue;
		default:
			// Unknown sub-object type, break out of the loop
			assert(false);
		}
		break;
	}

	HRESULT hr = S_OK;
	if (riid == __uuidof(ID3D12PipelineState) &&
		reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data()))
	{
		reshade::api::pipeline pipeline;
		hr = create_pipeline(layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pipeline) ? S_OK : E_FAIL;
		*ppPipelineState = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
	}
	else
	{
		hr = static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(pDesc, riid, ppPipelineState);
	}
#else
	const HRESULT hr = static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(pDesc, riid, ppPipelineState);
#endif

	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12PipelineState))
		{
			const auto pipeline = static_cast<ID3D12PipelineState *>(*ppPipelineState);

			reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), to_handle(pipeline));

			register_destruction_callback(pipeline, [this, pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(pipeline));
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device2::CreatePipelineState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::OpenExistingHeapFromAddress(const void *pAddress, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D12Device3 *>(_orig)->OpenExistingHeapFromAddress(pAddress, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenExistingHeapFromFileMapping(HANDLE hFileMapping, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D12Device3 *>(_orig)->OpenExistingHeapFromFileMapping(hFileMapping, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnqueueMakeResident(D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects, ID3D12Pageable *const *ppObjects, ID3D12Fence *pFenceToSignal, UINT64 FenceValueToSignal)
{
	assert(_interface_version >= 3);
	return static_cast<ID3D12Device3 *>(_orig)->EnqueueMakeResident(Flags, NumObjects, ppObjects, pFenceToSignal, FenceValueToSignal);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList1(UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, D3D12_COMMAND_LIST_FLAGS Flags, REFIID riid, void **ppCommandList)
{
	assert(_interface_version >= 4);
	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateCommandList1(NodeMask, Type, Flags, riid, ppCommandList);
	if (SUCCEEDED(hr))
	{
		const auto command_list_proxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList *>(*ppCommandList));

		// Upgrade to the actual interface version requested here (and only hook graphics command lists)
		if (command_list_proxy->check_and_upgrade_interface(riid))
		{
			*ppCommandList = command_list_proxy;

#if RESHADE_ADDON
			reshade::invoke_addon_event<reshade::addon_event::reset_command_list>(command_list_proxy);
#endif
		}
		else // Do not hook object if we do not support the requested interface or this is a compute command list
		{
			LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12Device4::CreateCommandList1" << '.';

			delete command_list_proxy; // Delete instead of release to keep reference count untouched
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device4::CreateCommandList1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;

}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, REFIID riid, void **ppSession)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateProtectedResourceSession(pDesc, riid, ppSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource1(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
	assert(_interface_version >= 4);

#if RESHADE_ADDON
	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, nullptr);

	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialResourceState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riidResource == __uuidof(ID3D12Resource) ||
			riidResource == __uuidof(ID3D12Resource1) ||
			riidResource == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
			if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("ID3D12Resource::Map", vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
			if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("ID3D12Resource::Unmap", vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device4::CreateCommittedResource1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap1(const D3D12_HEAP_DESC *pDesc, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateHeap1(pDesc, pProtectedSession, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource1(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 4);

#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, nullptr);

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc);
	assert(desc.heap == reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device4::CreateReservedResource1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo1(UINT VisibleMask, UINT NumResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->GetResourceAllocationInfo1(VisibleMask, NumResourceDescs, pResourceDescs, pResourceAllocationInfo1);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateLifetimeTracker(ID3D12LifetimeOwner *pOwner, REFIID riid, void **ppvTracker)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->CreateLifetimeTracker(pOwner, riid, ppvTracker);
}
void    STDMETHODCALLTYPE D3D12Device::RemoveDevice()
{
	assert(_interface_version >= 5);
	static_cast<ID3D12Device5 *>(_orig)->RemoveDevice();
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnumerateMetaCommands(UINT *pNumMetaCommands, D3D12_META_COMMAND_DESC *pDescs)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->EnumerateMetaCommands(pNumMetaCommands, pDescs);
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnumerateMetaCommandParameters(REFGUID CommandId, D3D12_META_COMMAND_PARAMETER_STAGE Stage, UINT *pTotalStructureSizeInBytes, UINT *pParameterCount, D3D12_META_COMMAND_PARAMETER_DESC *pParameterDescs)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->EnumerateMetaCommandParameters(CommandId, Stage, pTotalStructureSizeInBytes, pParameterCount, pParameterDescs);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateMetaCommand(REFGUID CommandId, UINT NodeMask, const void *pCreationParametersData, SIZE_T CreationParametersDataSizeInBytes, REFIID riid, void **ppMetaCommand)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->CreateMetaCommand(CommandId, NodeMask, pCreationParametersData, CreationParametersDataSizeInBytes, riid, ppMetaCommand);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateStateObject(const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid, void **ppStateObject)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->CreateStateObject(pDesc, riid, ppStateObject);
}
void    STDMETHODCALLTYPE D3D12Device::GetRaytracingAccelerationStructurePrebuildInfo(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS *pDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO *pInfo)
{
	// assert(_interface_version >= 5); // Cyberpunk 2077 incorrectly calls this on a 'ID3D12Device3' object
	static_cast<ID3D12Device5 *>(_orig)->GetRaytracingAccelerationStructurePrebuildInfo(pDesc, pInfo);
}
D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS STDMETHODCALLTYPE D3D12Device::CheckDriverMatchingIdentifier(D3D12_SERIALIZED_DATA_TYPE SerializedDataType, const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER *pIdentifierToCheck)
{
	assert(_interface_version >= 5);
	return static_cast<ID3D12Device5 *>(_orig)->CheckDriverMatchingIdentifier(SerializedDataType, pIdentifierToCheck);
}

HRESULT STDMETHODCALLTYPE D3D12Device::SetBackgroundProcessingMode(D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction, HANDLE hEventToSignalUponCompletion, BOOL *pbFurtherMeasurementsDesired)
{
	assert(_interface_version >= 6);
	return static_cast<ID3D12Device6*>(_orig)->SetBackgroundProcessingMode(Mode, MeasurementsAction, hEventToSignalUponCompletion, pbFurtherMeasurementsDesired);
}

HRESULT STDMETHODCALLTYPE D3D12Device::AddToStateObject(const D3D12_STATE_OBJECT_DESC *pAddition, ID3D12StateObject *pStateObjectToGrowFrom, REFIID riid, void **ppNewStateObject)
{
	assert(_interface_version >= 7);
	return static_cast<ID3D12Device7 *>(_orig)->AddToStateObject(pAddition, pStateObjectToGrowFrom, riid, ppNewStateObject);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession1(const D3D12_PROTECTED_RESOURCE_SESSION_DESC1 *pDesc, REFIID riid, void **ppSession)
{
	assert(_interface_version >= 7);
	return static_cast<ID3D12Device7 *>(_orig)->CreateProtectedResourceSession1(pDesc, riid, ppSession);
}

D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo2(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC1 *pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
	assert(_interface_version >= 8);
	return static_cast<ID3D12Device8 *>(_orig)->GetResourceAllocationInfo2(visibleMask, numResourceDescs, pResourceDescs, pResourceAllocationInfo1);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource2(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
	assert(_interface_version >= 8);

#if RESHADE_ADDON
	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device8 *>(_orig)->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, nullptr);

	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialResourceState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device8 *>(_orig)->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riidResource == __uuidof(ID3D12Resource) ||
			riidResource == __uuidof(ID3D12Resource1) ||
			riidResource == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
			if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("ID3D12Resource::Map", vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
			if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("ID3D12Resource::Unmap", vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device8::CreateCommittedResource2" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource1(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 8);

#if RESHADE_ADDON
	if (pHeap == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device8 *>(_orig)->CreatePlacedResource1(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, nullptr);

	const auto heap_desc = pHeap->GetDesc();

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_desc.Properties, heap_desc.Flags);
	assert(desc.heap != reshade::api::memory_heap::unknown);

	const auto initial_state = (InitialState == D3D12_RESOURCE_STATE_COMMON) ? reshade::api::resource_usage::general : static_cast<reshade::api::resource_usage>(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device8 *>(_orig)->CreatePlacedResource1(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

#if RESHADE_ADDON && !RESHADE_ADDON_LITE
			if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::map_texture_region>())
				reshade::hooks::install("ID3D12Resource::Map", vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
			if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
				reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
				reshade::hooks::install("ID3D12Resource::Unmap", vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

			register_resource(resource);
			reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

			register_destruction_callback(resource, [this, resource]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
				unregister_resource(resource);
			});
		}
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device8::CreatePlacedResource1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
void    STDMETHODCALLTYPE D3D12Device::CreateSamplerFeedbackUnorderedAccessView(ID3D12Resource *pTargetedResource, ID3D12Resource *pFeedbackResource, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	assert(_interface_version >= 8);
#if RESHADE_ADDON && !RESHADE_ADDON_LITE
	static_cast<ID3D12Device8 *>(_orig)->CreateSamplerFeedbackUnorderedAccessView(pTargetedResource, pFeedbackResource, convert_to_original_cpu_descriptor_handle(DestDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
#else
	static_cast<ID3D12Device8 *>(_orig)->CreateSamplerFeedbackUnorderedAccessView(pTargetedResource, pFeedbackResource, DestDescriptor);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints1(const D3D12_RESOURCE_DESC1 *pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts, UINT *pNumRows, UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes)
{
	assert(_interface_version >= 8);
	static_cast<ID3D12Device8 *>(_orig)->GetCopyableFootprints1(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateShaderCacheSession(const D3D12_SHADER_CACHE_SESSION_DESC *pDesc, REFIID riid, void **ppvSession)
{
	assert(_interface_version >= 9);
	return static_cast<ID3D12Device9 *>(_orig)->CreateShaderCacheSession(pDesc, riid, ppvSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::ShaderCacheControl(D3D12_SHADER_CACHE_KIND_FLAGS Kinds, D3D12_SHADER_CACHE_CONTROL_FLAGS Control)
{
	assert(_interface_version >= 9);
	return static_cast<ID3D12Device9 *>(_orig)->ShaderCacheControl(Kinds, Control);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue1(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID CreatorID, REFIID riid, void **ppCommandQueue)
{
	assert(_interface_version >= 9);

	LOG(INFO) << "Redirecting " << "ID3D12Device9::CreateCommandQueue1" << '(' << "this = " << this << ", pDesc = " << pDesc << ", CreatorID = " << CreatorID << ", riid = " << riid << ", ppCommandQueue = " << ppCommandQueue << ')' << " ...";

	if (pDesc == nullptr)
		return E_INVALIDARG;

	LOG(INFO) << "> Dumping command queue description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Type                                    | " << std::setw(39) << pDesc->Type << " |";
	LOG(INFO) << "  | Priority                                | " << std::setw(39) << pDesc->Priority << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << pDesc->Flags << std::dec << " |";
	LOG(INFO) << "  | NodeMask                                | " << std::setw(39) << std::hex << pDesc->NodeMask << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

	const HRESULT hr = static_cast<ID3D12Device9 *>(_orig)->CreateCommandQueue1(pDesc, CreatorID, riid, ppCommandQueue);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandQueue != nullptr);

		const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

		// Upgrade to the actual interface version requested here
		if (command_queue_proxy->check_and_upgrade_interface(riid))
		{
#if RESHADE_VERBOSE_LOG
			LOG(INFO) << "> Returning ID3D12CommandQueue" << " object " << command_queue_proxy << " (" << command_queue_proxy->_orig << ").";
#endif
			*ppCommandQueue = command_queue_proxy;
		}
		else // Do not hook object if we do not support the requested interface
		{
			LOG(WARN) << "Unknown interface " << riid << " in " << "ID3D12Device9::CreateCommandQueue1" << '.';

			delete command_queue_proxy; // Delete instead of release to keep reference count untouched
		}
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D12Device9::CreateCommandQueue1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
