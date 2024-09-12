/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "d3d12_device_downlevel.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_pipeline_library.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get 'hr_to_string' helper function
#include "com_utils.hpp"
#include "hook_manager.hpp"
#include "addon_manager.hpp"
#include <cwchar> // std::wcslen
#include <algorithm> // std::find_if
#include <utf8/unchecked.h>

using reshade::d3d12::to_handle;

extern std::shared_mutex g_adapter_mutex;

D3D12Device::D3D12Device(ID3D12Device *original) :
	device_impl(original)
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D12Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D12Device), sizeof(device_proxy), &device_proxy);

#if RESHADE_ADDON
	reshade::load_addons();

	reshade::invoke_addon_event<reshade::addon_event::init_device>(this);
#endif
}
D3D12Device::~D3D12Device()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::destroy_device>(this);

	reshade::unload_addons();
#endif

	// Remove pointer to this proxy object from the private data of the device (in case the device unexpectedly survives)
	_orig->SetPrivateData(__uuidof(D3D12Device), 0, nullptr);
}

bool D3D12Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) || // This is the IID_IUnknown identity object
		riid == __uuidof(ID3D12Object))
		return true;

	static constexpr IID iid_lookup[] = {
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
		__uuidof(ID3D12Device10),
		__uuidof(ID3D12Device11),
		__uuidof(ID3D12Device12),
		__uuidof(ID3D12Device13),
		__uuidof(ID3D12Device14),
	};

	for (unsigned short version = 0; version < ARRAYSIZE(iid_lookup); ++version)
	{
		if (riid != iid_lookup[version])
			continue;

		if (version > _interface_version)
		{
			IUnknown *new_interface = nullptr;
			if (FAILED(_orig->QueryInterface(riid, reinterpret_cast<void **>(&new_interface))))
				return false;
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Upgrading ID3D12Device%hu object %p to ID3D12Device%hu.", _interface_version, this, version);
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
		else
			return E_NOINTERFACE;
	}

	// Unimplemented interfaces:
	//   ID3D12DebugDevice  {3FEBD6DD-4973-4787-8194-E45F9E28923E}
	//   ID3D12DebugDevice1 {A9B71770-D099-4A65-A698-3DEE10020F88}
	//   ID3D12DebugDevice2 {60ECCBC1-378D-4DF1-894C-F8AC5CE4D7DD}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12Device::AddRef()
{
	const std::unique_lock<std::shared_mutex> lock(g_adapter_mutex);

	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12Device::Release()
{
	const std::unique_lock<std::shared_mutex> lock(g_adapter_mutex);

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
	reshade::log::message(reshade::log::level::debug, "Destroying ID3D12Device%hu object %p (%p).", interface_version, this, orig);
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		reshade::log::message(reshade::log::level::warning, "Reference count for ID3D12Device%hu object %p (%p) is inconsistent (%lu).", interface_version, this, orig, ref_orig);
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

UINT    STDMETHODCALLTYPE D3D12Device::GetNodeCount()
{
	return _orig->GetNodeCount();
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid, void **ppCommandQueue)
{
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting ID3D12Device::CreateCommandQueue(this = %p, pDesc = %p, riid = %s, ppCommandQueue = %p) ...",
		this, pDesc, reshade::log::iid_to_string(riid).c_str(), ppCommandQueue);

	if (pDesc == nullptr)
		return E_INVALIDARG;

	reshade::log::message(reshade::log::level::info, "> Dumping command queue description:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Type                                    | %-39d |", static_cast<int>(pDesc->Type));
	reshade::log::message(reshade::log::level::info, "  | Priority                                | %-39d |", pDesc->Priority);
	reshade::log::message(reshade::log::level::info, "  | Flags                                   | %-39x |", static_cast<unsigned int>(pDesc->Flags));
	reshade::log::message(reshade::log::level::info, "  | NodeMask                                | %-39x |", pDesc->NodeMask);
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	const HRESULT hr = _orig->CreateCommandQueue(pDesc, riid, ppCommandQueue);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandQueue != nullptr);

		const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

		// Upgrade to the actual interface version requested here
		if (command_queue_proxy->check_and_upgrade_interface(riid))
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Returning ID3D12CommandQueue object %p (%p).", command_queue_proxy, command_queue_proxy->_orig);
#endif
			*ppCommandQueue = command_queue_proxy;
		}
		else // Do not hook object if we do not support the requested interface
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateCommandQueue.", reshade::log::iid_to_string(riid).c_str());

			delete command_queue_proxy; // Delete instead of release to keep reference count untouched
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateCommandQueue failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator)
{
	return _orig->CreateCommandAllocator(type, riid, ppCommandAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppPipelineState == nullptr || // This can happen when application only wants to validate input parameters
		riid != __uuidof(ID3D12PipelineState) ||
		!invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, true))
#endif
		hr = _orig->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppPipelineState != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateGraphicsPipelineState failed with error code %ld.");
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppPipelineState == nullptr || // This can happen when application only wants to validate input parameters
		riid != __uuidof(ID3D12PipelineState) ||
		!invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, true))
#endif
		hr = _orig->CreateComputePipelineState(pDesc, riid, ppPipelineState);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppPipelineState != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateComputePipelineState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

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
#endif

#if RESHADE_ADDON >= 2
			if (pInitialState != nullptr)
				reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(command_list_proxy, reshade::api::pipeline_stage::all, to_handle(pInitialState));
#endif
		}
		else // Do not hook object if we do not support the requested interface
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateCommandList.", reshade::log::iid_to_string(riid).c_str());

			delete command_list_proxy; // Delete instead of release to keep reference count untouched
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateCommandList failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return _orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
#if RESHADE_VERBOSE_LOG
	reshade::log::message(
		reshade::log::level::info,
		"Redirecting ID3D12Device::CreateDescriptorHeap(this = %p, pDescriptorHeapDesc = %p, riid = %s, ppvHeap = %p) ...",
		this, pDescriptorHeapDesc, reshade::log::iid_to_string(riid).c_str(), ppvHeap);
#endif

	if (pDescriptorHeapDesc == nullptr)
		return E_INVALIDARG;

	const HRESULT hr = _orig->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON >= 2
		if (ppvHeap != nullptr && pDescriptorHeapDesc->Type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			const auto descriptor_heap_proxy = new D3D12DescriptorHeap(this, static_cast<ID3D12DescriptorHeap *>(*ppvHeap));

			// Upgrade to the actual interface version requested here
			if (descriptor_heap_proxy->check_and_upgrade_interface(riid))
			{
				register_descriptor_heap(descriptor_heap_proxy);

				register_destruction_callback_d3dx(descriptor_heap_proxy, [this, descriptor_heap_proxy]() {
					unregister_descriptor_heap(descriptor_heap_proxy);
				});

#if RESHADE_VERBOSE_LOG
				reshade::log::message(reshade::log::level::debug, "Returning ID3D12DescriptorHeap object %p (%p).", descriptor_heap_proxy, descriptor_heap_proxy->_orig);
#endif
				*ppvHeap = descriptor_heap_proxy;
			}
			else // Do not hook object if we do not support the requested interface
			{
				reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateDescriptorHeap.", reshade::log::iid_to_string(riid).c_str());

				delete descriptor_heap_proxy; // Delete instead of release to keep reference count untouched
			}
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateDescriptorHeap failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
UINT    STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
	return _orig->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **ppvRootSignature)
{
	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppvRootSignature == nullptr ||
		riid != __uuidof(ID3D12RootSignature) ||
		!invoke_create_and_init_pipeline_layout_event(nodeMask, pBlobWithRootSignature, blobLengthInBytes, *reinterpret_cast<ID3D12RootSignature **>(ppvRootSignature), hr))
#endif
		hr = _orig->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppvRootSignature != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateRootSignature failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
void    STDMETHODCALLTYPE D3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON >= 2
	const reshade::api::descriptor_table table = convert_to_descriptor_table(DestDescriptor);
	DestDescriptor = convert_to_original_cpu_descriptor_handle(table);
#endif
	_orig->CreateConstantBufferView(pDesc, DestDescriptor);

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
		return;

	reshade::api::buffer_range buffer_range;
	if (pDesc == nullptr || !resolve_gpu_address(pDesc->BufferLocation, &buffer_range.buffer, &buffer_range.offset))
		return;
	buffer_range.size = pDesc->SizeInBytes;

	reshade::api::descriptor_table_update update;
	update.table = table;
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::constant_buffer;
	update.count = 1;
	update.descriptors = &buffer_range;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(this, 1, &update);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	D3D12_SHADER_RESOURCE_VIEW_DESC internal_desc = (pDesc != nullptr) ? *pDesc : D3D12_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d12::convert_resource_view_desc(internal_desc);
	auto usage = reshade::api::resource_usage::shader_resource;
	const bool acceleration_structure = internal_desc.ViewDimension == D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;

	if (acceleration_structure)
	{
		if (pResource != nullptr)
		{
			desc.buffer.offset -= pResource->GetGPUVirtualAddress();
			desc.buffer.size = pResource->GetDesc().Width - desc.buffer.offset;
		}

		usage = reshade::api::resource_usage::acceleration_structure;
	}

	// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
	if (pResource != nullptr &&
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), usage, desc))
	{
		if (acceleration_structure)
		{
			desc.buffer.offset += pResource->GetGPUVirtualAddress();
		}

		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

#if RESHADE_ADDON >= 2
	const reshade::api::descriptor_table table = convert_to_descriptor_table(DestDescriptor);
	DestDescriptor = convert_to_original_cpu_descriptor_handle(table);
#endif
	_orig->CreateShaderResourceView(pResource, pDesc, DestDescriptor);

#if RESHADE_ADDON
	const reshade::api::resource_view descriptor_value = acceleration_structure ? reshade::api::resource_view { internal_desc.RaytracingAccelerationStructure.Location } : to_handle(DestDescriptor);

	register_resource_view(DestDescriptor, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), usage, desc, descriptor_value);
#endif

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
		return;

	reshade::api::descriptor_table_update update;
	update.table = table;
	update.binding = 0;
	update.array_offset = 0;
	update.type = acceleration_structure ? reshade::api::descriptor_type::acceleration_structure :
		internal_desc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER ? reshade::api::descriptor_type::buffer_shader_resource_view : reshade::api::descriptor_type::texture_shader_resource_view;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(this, 1, &update);
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
#endif

#if RESHADE_ADDON >= 2
	const reshade::api::descriptor_table table = convert_to_descriptor_table(DestDescriptor);
	DestDescriptor = convert_to_original_cpu_descriptor_handle(table);
#endif
	_orig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);

#if RESHADE_ADDON
	const reshade::api::resource_view descriptor_value = to_handle(DestDescriptor);

	register_resource_view(DestDescriptor, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), reshade::api::resource_usage::unordered_access, desc, descriptor_value);
#endif

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
		return;

	reshade::api::descriptor_table_update update;
	update.table = table;
	update.binding = 0;
	update.array_offset = 0;
	update.type =
		internal_desc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER ? reshade::api::descriptor_type::buffer_unordered_access_view : reshade::api::descriptor_type::texture_unordered_access_view;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(this, 1, &update);
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
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, to_handle(pResource), internal_desc.Flags != 0 ? reshade::api::resource_usage::depth_stencil_read : reshade::api::resource_usage::depth_stencil, desc))
	{
		reshade::d3d12::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	_orig->CreateDepthStencilView(pResource, pDesc, DestDescriptor);

#if RESHADE_ADDON
	register_resource_view(DestDescriptor, pResource, desc);
	reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(this, to_handle(pResource), internal_desc.Flags != 0 ? reshade::api::resource_usage::depth_stencil_read : reshade::api::resource_usage::depth_stencil, desc, to_handle(DestDescriptor));
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
#endif

#if RESHADE_ADDON >= 2
	const reshade::api::descriptor_table table = convert_to_descriptor_table(DestDescriptor);
	DestDescriptor = convert_to_original_cpu_descriptor_handle(table);
#endif
	_orig->CreateSampler(pDesc, DestDescriptor);

#if RESHADE_ADDON
	const reshade::api::sampler descriptor_value = { DestDescriptor.ptr };

	reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, descriptor_value);
#endif

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
		return;

	reshade::api::descriptor_table_update update;
	update.table = table;
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::sampler;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(this, 1, &update);
#endif
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptors(UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts, const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts, const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
#if RESHADE_ADDON >= 2
	if (DescriptorHeapsType <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		if (reshade::has_addon_event<reshade::addon_event::copy_descriptor_tables>())
		{
			uint32_t num_copies = 0;
			for (UINT i = 0; i < NumDestDescriptorRanges; ++i)
				num_copies += (pDestDescriptorRangeSizes != nullptr ? pDestDescriptorRangeSizes[i] : 1);
			temp_mem<reshade::api::descriptor_table_copy, 32> copies(num_copies);

			num_copies = 0;

			for (UINT dst_range = 0, src_range = 0, dst_offset = 0, src_offset = 0; dst_range < NumDestDescriptorRanges; ++dst_range, dst_offset = 0)
			{
				const UINT dst_count = (pDestDescriptorRangeSizes != nullptr ? pDestDescriptorRangeSizes[dst_range] : 1);

				while (dst_offset < dst_count)
				{
					const UINT src_count = (pSrcDescriptorRangeSizes != nullptr ? pSrcDescriptorRangeSizes[src_range] : 1);

					copies[num_copies].dest_table = convert_to_descriptor_table(pDestDescriptorRangeStarts[dst_range]);
					copies[num_copies].dest_binding = 0;
					copies[num_copies].dest_array_offset = 0;
					copies[num_copies].source_table = convert_to_descriptor_table(pSrcDescriptorRangeStarts[src_range]);
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

			if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_tables>(this, num_copies, copies.p))
				return;
		}

		temp_mem<D3D12_CPU_DESCRIPTOR_HANDLE, 32> descriptor_range_starts(NumDestDescriptorRanges + NumSrcDescriptorRanges);
		for (UINT i = 0; i < NumSrcDescriptorRanges; ++i)
			descriptor_range_starts[i] = convert_to_original_cpu_descriptor_handle(convert_to_descriptor_table(pSrcDescriptorRangeStarts[i]));
		for (UINT i = 0; i < NumDestDescriptorRanges; ++i)
			descriptor_range_starts[NumSrcDescriptorRanges + i] = convert_to_original_cpu_descriptor_handle(convert_to_descriptor_table(pDestDescriptorRangeStarts[i]));

		_orig->CopyDescriptors(NumDestDescriptorRanges, descriptor_range_starts.p + NumSrcDescriptorRanges, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, descriptor_range_starts.p, pSrcDescriptorRangeSizes, DescriptorHeapsType);
		return;
	}
	else
#endif
#if RESHADE_ADDON
	if (DescriptorHeapsType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || DescriptorHeapsType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	{
		for (UINT dst_range = 0, src_range = 0, dst_offset = 0, src_offset = 0; dst_range < NumDestDescriptorRanges; ++dst_range, dst_offset = 0)
		{
			const UINT dst_count = (pDestDescriptorRangeSizes != nullptr ? pDestDescriptorRangeSizes[dst_range] : 1);

			while (dst_offset < dst_count)
			{
				const UINT src_count = (pSrcDescriptorRangeSizes != nullptr ? pSrcDescriptorRangeSizes[src_range] : 1);

				register_resource_view(offset_descriptor_handle(pDestDescriptorRangeStarts[dst_range], dst_offset, DescriptorHeapsType), offset_descriptor_handle(pSrcDescriptorRangeStarts[src_range], src_offset, DescriptorHeapsType));

				src_offset += 1;
				dst_offset += 1;

				if (src_offset >= src_count)
				{
					src_range += 1;
					src_offset = 0;
				}
			}
		}
	}
#endif

	_orig->CopyDescriptors(NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType);
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
#if RESHADE_ADDON >= 2
	if (DescriptorHeapsType <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		reshade::api::descriptor_table_copy copy;
		copy.dest_table = convert_to_descriptor_table(DestDescriptorRangeStart);
		copy.source_table = convert_to_descriptor_table(SrcDescriptorRangeStart);
		copy.count = NumDescriptors;

		if (reshade::invoke_addon_event<reshade::addon_event::copy_descriptor_tables>(this, 1, &copy))
			return;

		_orig->CopyDescriptorsSimple(NumDescriptors, convert_to_original_cpu_descriptor_handle(copy.dest_table), convert_to_original_cpu_descriptor_handle(copy.source_table), DescriptorHeapsType);
		return;
	}
	else
#endif
#if RESHADE_ADDON
	if (DescriptorHeapsType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || DescriptorHeapsType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	{
		for (UINT i = 0; i < NumDescriptors; ++i)
			register_resource_view(offset_descriptor_handle(DestDescriptorRangeStart, i, DescriptorHeapsType), offset_descriptor_handle(SrcDescriptorRangeStart, i, DescriptorHeapsType));
	}
#endif

	_orig->CopyDescriptorsSimple(NumDescriptors, DestDescriptorRangeStart, SrcDescriptorRangeStart, DescriptorHeapsType);
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs)
{
	return _orig->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}
D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE D3D12Device::GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType)
{
	return _orig->GetCustomHeapProperties(nodeMask, heapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, riid, ppvResource);

#if RESHADE_ADDON
	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr; // Disable optimized clear value in case the format was changed by an add-on
	}
#endif

	const HRESULT hr = _orig->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateCommittedResource.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateCommittedResource failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
	return _orig->CreateHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	if (pHeap == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);

#if RESHADE_ADDON
	const auto heap_desc = pHeap->GetDesc();

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_desc.Properties, heap_desc.Flags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = _orig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreatePlacedResource.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreatePlacedResource failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);

#if RESHADE_ADDON
	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc);
	assert(desc.heap == reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = _orig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateReservedResource.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateReservedResource failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
	return _orig->CreateSharedHandle(pObject, pAttributes, Access, Name, pHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
	const HRESULT hr = _orig->OpenSharedHandle(NTHandle, riid, ppvObj);
	if (SUCCEEDED(hr) && ppvObj != nullptr)
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvObj);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
			D3D12_HEAP_PROPERTIES heap_props = {};
			resource->GetHeapProperties(&heap_props, &heap_flags);

			const reshade::api::resource_desc desc = reshade::d3d12::convert_resource_desc(resource->GetDesc(), heap_props, heap_flags);

			assert((desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared);

			invoke_init_resource_event(desc, reshade::api::resource_usage::general, resource);
#endif
		}
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
	return _orig->OpenSharedHandleByName(Name, Access, pNTHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
#if RESHADE_ADDON >= 2
	temp_mem<ID3D12Pageable *> objects(NumObjects);
	for (UINT i = 0; i < NumObjects; ++i)
	{
		if (com_ptr<D3D12DescriptorHeap> descriptor_heap_proxy;
			SUCCEEDED(ppObjects[i]->QueryInterface(&descriptor_heap_proxy)))
			objects[i] = descriptor_heap_proxy->_orig;
		else
			objects[i] = ppObjects[i];
	}
	ppObjects = objects.p;
#endif

	return _orig->MakeResident(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
#if RESHADE_ADDON >= 2
	temp_mem<ID3D12Pageable *> objects(NumObjects);
	for (UINT i = 0; i < NumObjects; ++i)
	{
		if (com_ptr<D3D12DescriptorHeap> descriptor_heap_proxy;
			SUCCEEDED(ppObjects[i]->QueryInterface(&descriptor_heap_proxy)))
			objects[i] = descriptor_heap_proxy->_orig;
		else
			objects[i] = ppObjects[i];
	}
	ppObjects = objects.p;
#endif

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
		return _orig->CreateQueryHeap(pDesc, riid, ppvHeap);

	D3D12_QUERY_HEAP_DESC internal_desc = *pDesc;

	if (reshade::invoke_addon_event<reshade::addon_event::create_query_heap>(this, reshade::d3d12::convert_query_heap_type_to_type(internal_desc.Type), internal_desc.Count))
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

			reshade::invoke_addon_event<reshade::addon_event::init_query_heap>(this, reshade::d3d12::convert_query_heap_type_to_type(internal_desc.Type), internal_desc.Count, to_handle(query_heap));

			if (reshade::has_addon_event<reshade::addon_event::destroy_query_heap>())
			{
				register_destruction_callback_d3dx(query_heap, [this, query_heap]() {
					reshade::invoke_addon_event<reshade::addon_event::destroy_query_heap>(this, to_handle(query_heap));
				});
			}
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device::CreateQueryHeap.", reshade::log::iid_to_string(riid).c_str());
		}
#endif
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device::CreateQueryHeap failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

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

	const HRESULT hr = static_cast<ID3D12Device1 *>(_orig)->CreatePipelineLibrary(pLibraryBlob, BlobLength, riid, ppPipelineLibrary);
	if (SUCCEEDED(hr) && ppPipelineLibrary != nullptr)
	{
#if RESHADE_ADDON >= 2
		if (riid == __uuidof(ID3D12PipelineLibrary) ||
			riid == __uuidof(ID3D12PipelineLibrary1))
		{
			const auto pipeline_library = static_cast<ID3D12PipelineLibrary *>(*ppPipelineLibrary);

			if (reshade::has_addon_event<reshade::addon_event::init_pipeline>() ||
				reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
			{
				reshade::hooks::install("ID3D12PipelineLibrary::LoadGraphicsPipeline", reshade::hooks::vtable_from_instance(pipeline_library), 9, reinterpret_cast<reshade::hook::address>(&ID3D12PipelineLibrary_LoadGraphicsPipeline));
				reshade::hooks::install("ID3D12PipelineLibrary::LoadComputePipeline", reshade::hooks::vtable_from_instance(pipeline_library), 10, reinterpret_cast<reshade::hook::address>(&ID3D12PipelineLibrary_LoadComputePipeline));

				if (com_ptr<ID3D12PipelineLibrary1> pipeline_library1;
					SUCCEEDED(pipeline_library->QueryInterface(IID_PPV_ARGS(&pipeline_library1))))
				{
					reshade::hooks::install("ID3D12PipelineLibrary1::LoadPipeline", reshade::hooks::vtable_from_instance(pipeline_library1.get()), 13, reinterpret_cast<reshade::hook::address>(&ID3D12PipelineLibrary1_LoadPipeline));
				}
			}
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device1::CreatePipelineLibrary.", reshade::log::iid_to_string(riid).c_str());
		}
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetEventOnMultipleFenceCompletion(ID3D12Fence *const *ppFences, const UINT64 *pFenceValues, UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent)
{
	assert(_interface_version >= 1);
	return static_cast<ID3D12Device1 *>(_orig)->SetEventOnMultipleFenceCompletion(ppFences, pFenceValues, NumFences, Flags, hEvent);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetResidencyPriority(UINT NumObjects, ID3D12Pageable *const *ppObjects, const D3D12_RESIDENCY_PRIORITY *pPriorities)
{
#if RESHADE_ADDON >= 2
	temp_mem<ID3D12Pageable *> objects(NumObjects);
	for (UINT i = 0; i < NumObjects; ++i)
	{
		if (com_ptr<D3D12DescriptorHeap> descriptor_heap_proxy;
			SUCCEEDED(ppObjects[i]->QueryInterface(&descriptor_heap_proxy)))
			objects[i] = descriptor_heap_proxy->_orig;
		else
			objects[i] = ppObjects[i];
	}
	ppObjects = objects.p;
#endif

	assert(_interface_version >= 1);
	return static_cast<ID3D12Device1 *>(_orig)->SetResidencyPriority(NumObjects, ppObjects, pPriorities);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	assert(_interface_version >= 2);

	if (pDesc == nullptr)
		return E_INVALIDARG;

	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppPipelineState == nullptr || // This can happen when application only wants to validate input parameters
		riid != __uuidof(ID3D12PipelineState) ||
		!invoke_create_and_init_pipeline_event(*pDesc, *reinterpret_cast<ID3D12PipelineState **>(ppPipelineState), hr, true))
#endif
		hr = static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(pDesc, riid, ppPipelineState);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppPipelineState != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device2::CreatePipelineState failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

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
#if RESHADE_ADDON >= 2
	temp_mem<ID3D12Pageable *> objects(NumObjects);
	for (UINT i = 0; i < NumObjects; ++i)
	{
		if (com_ptr<D3D12DescriptorHeap> descriptor_heap_proxy;
			SUCCEEDED(ppObjects[i]->QueryInterface(&descriptor_heap_proxy)))
			objects[i] = descriptor_heap_proxy->_orig;
		else
			objects[i] = ppObjects[i];
	}
	ppObjects = objects.p;
#endif

	assert(_interface_version >= 3);
	return static_cast<ID3D12Device3 *>(_orig)->EnqueueMakeResident(Flags, NumObjects, ppObjects, pFenceToSignal, FenceValueToSignal);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList1(UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, D3D12_COMMAND_LIST_FLAGS Flags, REFIID riid, void **ppCommandList)
{
	assert(_interface_version >= 4);
	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateCommandList1(NodeMask, Type, Flags, riid, ppCommandList);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandList != nullptr);

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
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device4::CreateCommandList1.", reshade::log::iid_to_string(riid).c_str());

			delete command_list_proxy; // Delete instead of release to keep reference count untouched
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device4::CreateCommandList1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;

}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, REFIID riid, void **ppSession)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateProtectedResourceSession(pDesc, riid, ppSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource1(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 4);

	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);

#if RESHADE_ADDON
	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device4::CreateCommittedResource1.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device4::CreateCommittedResource1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

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

	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);

#if RESHADE_ADDON
	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc);
	assert(desc.heap == reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device4::CreateReservedResource1.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device4::CreateReservedResource1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

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

	if (pDesc == nullptr)
		return E_INVALIDARG;

	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppStateObject == nullptr ||
		riid != __uuidof(ID3D12StateObject) ||
		!invoke_create_and_init_pipeline_event(*pDesc, nullptr, *reinterpret_cast<ID3D12StateObject **>(ppStateObject), hr))
#endif
		hr = static_cast<ID3D12Device5 *>(_orig)->CreateStateObject(pDesc, riid, ppStateObject);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppStateObject != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device5::CreateStateObject failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
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

	if (pAddition == nullptr)
		return E_INVALIDARG;

	HRESULT hr = S_OK;
#if RESHADE_ADDON >= 2
	if (ppNewStateObject == nullptr ||
		riid != __uuidof(ID3D12StateObject) ||
		!invoke_create_and_init_pipeline_event(*pAddition, pStateObjectToGrowFrom, *reinterpret_cast<ID3D12StateObject **>(ppNewStateObject), hr))
#endif
		hr = static_cast<ID3D12Device7 *>(_orig)->AddToStateObject(pAddition, pStateObjectToGrowFrom, riid, ppNewStateObject);

#if RESHADE_VERBOSE_LOG
	if (FAILED(hr) && ppNewStateObject != nullptr)
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device7::AddToStateObject failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
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
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource2(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 8);

	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device8 *>(_orig)->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);

#if RESHADE_ADDON
	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialResourceState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device8 *>(_orig)->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device8::CreateCommittedResource2.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device8::CreateCommittedResource2 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource1(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 8);

	if (pHeap == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device8 *>(_orig)->CreatePlacedResource1(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);

#if RESHADE_ADDON
	const auto heap_desc = pHeap->GetDesc();

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_desc.Properties, heap_desc.Flags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_resource_states_to_usage(InitialState);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device8 *>(_orig)->CreatePlacedResource1(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device8::CreatePlacedResource1.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device8::CreatePlacedResource1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
void    STDMETHODCALLTYPE D3D12Device::CreateSamplerFeedbackUnorderedAccessView(ID3D12Resource *pTargetedResource, ID3D12Resource *pFeedbackResource, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON >= 2
	DestDescriptor = convert_to_original_cpu_descriptor_handle(convert_to_descriptor_table(DestDescriptor));
#endif

	assert(_interface_version >= 8);
	static_cast<ID3D12Device8 *>(_orig)->CreateSamplerFeedbackUnorderedAccessView(pTargetedResource, pFeedbackResource, DestDescriptor);
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

	reshade::log::message(
		reshade::log::level::info,
		"Redirecting ID3D12Device9::CreateCommandQueue1(this = %p, pDesc = %p, CreatorID = %s, riid = %s, ppCommandQueue = %p) ...",
		this, pDesc, reshade::log::iid_to_string(CreatorID).c_str(), reshade::log::iid_to_string(riid).c_str(), ppCommandQueue);

	if (pDesc == nullptr)
		return E_INVALIDARG;

	reshade::log::message(reshade::log::level::info, "> Dumping command queue description:");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Parameter                               | Value                                   |");
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");
	reshade::log::message(reshade::log::level::info, "  | Type                                    | %-39d |", static_cast<int>(pDesc->Type));
	reshade::log::message(reshade::log::level::info, "  | Priority                                | %-39d |", pDesc->Priority);
	reshade::log::message(reshade::log::level::info, "  | Flags                                   | %-39x |", static_cast<unsigned int>(pDesc->Flags));
	reshade::log::message(reshade::log::level::info, "  | NodeMask                                | %-39x |", pDesc->NodeMask);
	reshade::log::message(reshade::log::level::info, "  +-----------------------------------------+-----------------------------------------+");

	const HRESULT hr = static_cast<ID3D12Device9 *>(_orig)->CreateCommandQueue1(pDesc, CreatorID, riid, ppCommandQueue);
	if (SUCCEEDED(hr))
	{
		assert(ppCommandQueue != nullptr);

		const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

		// Upgrade to the actual interface version requested here
		if (command_queue_proxy->check_and_upgrade_interface(riid))
		{
#if RESHADE_VERBOSE_LOG
			reshade::log::message(reshade::log::level::debug, "Returning ID3D12CommandQueue object %p (%p).", command_queue_proxy, command_queue_proxy->_orig);
#endif
			*ppCommandQueue = command_queue_proxy;
		}
		else // Do not hook object if we do not support the requested interface
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device9::CreateCommandQueue1.", reshade::log::iid_to_string(riid).c_str());

			delete command_queue_proxy; // Delete instead of release to keep reference count untouched
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device9::CreateCommandQueue1 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource3(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats, const DXGI_FORMAT *pCastableFormats, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 10);

	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device10 *>(_orig)->CreateCommittedResource3(pHeapProperties, HeapFlags, pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);

#if RESHADE_ADDON
	auto heap_props = *pHeapProperties;

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_props, HeapFlags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_barrier_layout_to_usage(InitialLayout);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc, heap_props, HeapFlags);
		pHeapProperties = &heap_props;
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device10 *>(_orig)->CreateCommittedResource3(pHeapProperties, HeapFlags, pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device10::CreateCommittedResource3.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device10::CreateCommittedResource3 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource2(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1 *pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE *pOptimizedClearValue, UINT32 NumCastableFormats, const DXGI_FORMAT *pCastableFormats, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 10);

	if (pHeap == nullptr || pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device10 *>(_orig)->CreatePlacedResource2(pHeap, HeapOffset, pDesc, InitialLayout, pOptimizedClearValue, NumCastableFormats, pCastableFormats, riid, ppvResource);

#if RESHADE_ADDON
	const auto heap_desc = pHeap->GetDesc();

	D3D12_RESOURCE_DESC1 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc, heap_desc.Properties, heap_desc.Flags);
	assert(desc.heap != reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_barrier_layout_to_usage(InitialLayout);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device10 *>(_orig)->CreatePlacedResource2(pHeap, HeapOffset, pDesc, InitialLayout, pOptimizedClearValue, NumCastableFormats, pCastableFormats, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device10::CreatePlacedResource2.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device10::CreatePlacedResource2 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource2(const D3D12_RESOURCE_DESC *pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats, const DXGI_FORMAT *pCastableFormats, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 10);

	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppvResource == nullptr) // This can happen when application only wants to validate input parameters
		return static_cast<ID3D12Device10 *>(_orig)->CreateReservedResource2(pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);

#if RESHADE_ADDON
	D3D12_RESOURCE_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_resource_desc(internal_desc);
	assert(desc.heap == reshade::api::memory_heap::unknown);
	const auto initial_state = reshade::d3d12::convert_barrier_layout_to_usage(InitialLayout);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, nullptr, initial_state))
	{
		reshade::d3d12::convert_resource_desc(desc, internal_desc);
		pDesc = &internal_desc;
		pOptimizedClearValue = nullptr;
	}
#endif

	const HRESULT hr = static_cast<ID3D12Device10 *>(_orig)->CreateReservedResource2(pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);
	if (SUCCEEDED(hr))
	{
		if (riid == __uuidof(ID3D12Resource) ||
			riid == __uuidof(ID3D12Resource1) ||
			riid == __uuidof(ID3D12Resource2))
		{
			const auto resource = static_cast<ID3D12Resource *>(*ppvResource);

			reshade::hooks::install("ID3D12Resource::GetDevice", reshade::hooks::vtable_from_instance(resource), 7, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_GetDevice));

#if RESHADE_ADDON
			invoke_init_resource_event(desc, initial_state, resource);
#endif
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Unknown interface %s in ID3D12Device10::CreateReservedResource2.", reshade::log::iid_to_string(riid).c_str());
		}
	}
#if RESHADE_VERBOSE_LOG
	else
	{
		reshade::log::message(reshade::log::level::warning, "ID3D12Device10::CreateReservedResource2 failed with error code %s.", reshade::log::hr_to_string(hr).c_str());
	}
#endif

	return hr;
}

void    STDMETHODCALLTYPE D3D12Device::CreateSampler2(const D3D12_SAMPLER_DESC2 *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	assert(_interface_version >= 11);

#if RESHADE_ADDON
	if (pDesc == nullptr) // Not allowed in D3D12
		return;

	D3D12_SAMPLER_DESC2 internal_desc = *pDesc;
	auto desc = reshade::d3d12::convert_sampler_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
	{
		reshade::d3d12::convert_sampler_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

#if RESHADE_ADDON >= 2
	const reshade::api::descriptor_table table = convert_to_descriptor_table(DestDescriptor);
	DestDescriptor = convert_to_original_cpu_descriptor_handle(table);
#endif
	static_cast<ID3D12Device11 *>(_orig)->CreateSampler2(pDesc, DestDescriptor);

#if RESHADE_ADDON
	const reshade::api::sampler descriptor_value = { DestDescriptor.ptr };

	reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, descriptor_value);
#endif

#if RESHADE_ADDON >= 2
	if (!reshade::has_addon_event<reshade::addon_event::update_descriptor_tables>())
		return;

	reshade::api::descriptor_table_update update;
	update.table = table;
	update.binding = 0;
	update.array_offset = 0;
	update.type = reshade::api::descriptor_type::sampler;
	update.count = 1;
	update.descriptors = &descriptor_value;

	reshade::invoke_addon_event<reshade::addon_event::update_descriptor_tables>(this, 1, &update);
#endif
}

D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo3(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC1 *pResourceDescs, const UINT32 *pNumCastableFormats, const DXGI_FORMAT *const *ppCastableFormats, D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
	assert(_interface_version >= 12);
	return static_cast<ID3D12Device12 *>(_orig)->GetResourceAllocationInfo3(visibleMask, numResourceDescs, pResourceDescs, pNumCastableFormats, ppCastableFormats, pResourceAllocationInfo1);
}

HRESULT STDMETHODCALLTYPE D3D12Device::OpenExistingHeapFromAddress1(const void *pAddress, SIZE_T size, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 13);
	return static_cast<ID3D12Device13 *>(_orig)->OpenExistingHeapFromAddress1(pAddress, size, riid, ppvHeap);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignatureFromSubobjectInLibrary(UINT nodeMask, const void *pLibraryBlob, SIZE_T blobLengthInBytes, LPCWSTR subobjectName, REFIID riid, void **ppvRootSignature)
{
	assert(_interface_version >= 14);
	return static_cast<ID3D12Device14 *>(_orig)->CreateRootSignatureFromSubobjectInLibrary(nodeMask, pLibraryBlob, blobLengthInBytes, subobjectName, riid, ppvRootSignature);
}

#if RESHADE_ADDON
void D3D12Device::invoke_init_resource_event(const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, ID3D12Resource *resource)
{
#if RESHADE_ADDON >= 2
	if (reshade::has_addon_event<reshade::addon_event::map_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::map_texture_region>())
		reshade::hooks::install("ID3D12Resource::Map", reshade::hooks::vtable_from_instance(resource), 8, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Map));
	if (reshade::has_addon_event<reshade::addon_event::unmap_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::unmap_texture_region>())
		reshade::hooks::install("ID3D12Resource::Unmap", reshade::hooks::vtable_from_instance(resource), 9, reinterpret_cast<reshade::hook::address>(&ID3D12Resource_Unmap));
#endif

	register_resource(resource, initial_state == reshade::api::resource_usage::acceleration_structure);
	reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, initial_state, to_handle(resource));

	register_destruction_callback_d3dx(resource, [this, resource]() {
		reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, to_handle(resource));
		unregister_resource(resource);
	});

	if (initial_state == reshade::api::resource_usage::acceleration_structure)
	{
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this,
			to_handle(resource),
			reshade::api::resource_usage::acceleration_structure,
			reshade::api::resource_view_desc(reshade::api::resource_view_type::acceleration_structure, reshade::api::format::unknown, 0, UINT64_MAX),
			reshade::api::resource_view { resource->GetGPUVirtualAddress() });
	}
}
#endif

#if RESHADE_ADDON >= 2

#include <d3d12shader.h>

static const void *find_dxbc_part(const void *blob, size_t blob_size, uint32_t tag)
{
	constexpr uint32_t DXBC_TAG = uint32_t('D') | (uint32_t('X') << 8) | (uint32_t('B') << 16) | (uint32_t('C') << 24);

	const auto data = static_cast<uint32_t *>(const_cast<void *>(blob));
	if (data == nullptr ||
		blob_size < (sizeof(uint32_t) * 8) ||
		data[0] != DXBC_TAG ||
		data[5] != 0x00000001 /* DXBC version */)
		return nullptr;

	assert(blob_size == data[6]);

	for (uint32_t i = 0, *part; i < data[7]; ++i)
	{
		const uint32_t part_offset = data[8 + i];
		part = data + (part_offset / sizeof(uint32_t));

		if (part[0] == tag)
			return part + 2;
	}

	return nullptr;
}

bool D3D12Device::invoke_create_and_init_pipeline_event(const D3D12_STATE_OBJECT_DESC &internal_desc, ID3D12StateObject *existing_d3d_pipeline, ID3D12StateObject *&d3d_pipeline, HRESULT &hr)
{
	if (!reshade::has_addon_event<reshade::addon_event::init_pipeline>() &&
		!reshade::has_addon_event<reshade::addon_event::create_pipeline>() &&
		!reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		return false;

	reshade::api::pipeline_layout layout = {};

	std::vector<reshade::api::shader_desc> pixel_desc;
	std::vector<reshade::api::shader_desc> vertex_desc;
	std::vector<reshade::api::shader_desc> geometry_desc;
	std::vector<reshade::api::shader_desc> hull_desc;
	std::vector<reshade::api::shader_desc> domain_desc;
	std::vector<reshade::api::shader_desc> compute_desc;
	std::vector<reshade::api::shader_desc> mesh_desc;
	std::vector<reshade::api::shader_desc> amplication_desc;
	std::vector<reshade::api::shader_desc> raygen_desc;
	std::vector<reshade::api::shader_desc> any_hit_desc;
	std::vector<reshade::api::shader_desc> closest_hit_desc;
	std::vector<reshade::api::shader_desc> miss_desc;
	std::vector<reshade::api::shader_desc> intersection_desc;
	std::vector<reshade::api::shader_desc> callable_desc;

	std::vector<reshade::api::shader_group> shader_groups;

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

	reshade::api::pipeline_flags flags = reshade::api::pipeline_flags::none;
	if (internal_desc.Type == D3D12_STATE_OBJECT_TYPE_COLLECTION)
		flags |= reshade::api::pipeline_flags::library;

	std::vector<reshade::api::pipeline> libraries;
	if (existing_d3d_pipeline != nullptr)
		libraries.push_back({ reinterpret_cast<uintptr_t>(existing_d3d_pipeline) });

	std::vector<reshade::api::dynamic_state> dynamic_states;

	std::vector<reshade::api::pipeline_subobject> subobjects;

	for (UINT i = 0; i < internal_desc.NumSubobjects; ++i)
	{
		const D3D12_STATE_SUBOBJECT &subobject = internal_desc.pSubobjects[i];

		switch (subobject.Type)
		{
		case D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG:
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
			layout = to_handle(static_cast<const D3D12_GLOBAL_ROOT_SIGNATURE *>(subobject.pDesc)->pGlobalRootSignature);
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
		{
			const auto &desc = *static_cast<const D3D12_DXIL_LIBRARY_DESC *>(subobject.pDesc);

			// See https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/DxilRuntimeReflection.h#L23
			if (const auto part = static_cast<const uint32_t *>(
					find_dxbc_part(
						desc.DXILLibrary.pShaderBytecode,
						desc.DXILLibrary.BytecodeLength,
						uint32_t('R') | (uint32_t('D') << 8) | (uint32_t('A') << 16) | (uint32_t('T') << 24))))
			{
				const char *string_table = nullptr;

				for (uint32_t part_index = 0; part_index < part[1]; ++part_index)
				{
					const uint32_t part_offset = part[2 + part_index];
					auto rdat_part = part + part_offset / sizeof(uint32_t);

					switch (rdat_part[0])
					{
					case 1: // StringBuffer
						string_table = reinterpret_cast<const char *>(rdat_part + 2);
						break;
					case 4: // FunctionTable
						if (string_table == nullptr)
							break;

						const uint32_t record_count = rdat_part[2];
						const uint32_t record_stride = rdat_part[3];

						for (uint32_t record_index = 0; record_index < record_count; ++record_index)
						{
							// See https://github.com/microsoft/DirectXShaderCompiler/blob/main/include/dxc/DxilContainer/RDAT_LibraryTypes.inl
							struct rdat_function_info
							{
								uint32_t name;
								uint32_t unmangled_name;
								uint32_t resources;
								uint32_t dependencies;
								uint32_t shader_kind;
								uint32_t payload_size;
								uint32_t attribute_size;
								uint32_t feature_flags1;
								uint32_t feature_flags2;
								uint32_t shader_kind_mask;
								uint32_t min_shader_version;
							};

							const auto function_info = reinterpret_cast<const rdat_function_info *>(rdat_part + 4 + record_index * (record_stride / sizeof(uint32_t)));

							// Filter exports if any are defined
							if (desc.NumExports != 0 &&
								std::find_if(desc.pExports, desc.pExports + desc.NumExports,
									[record_name = string_table + function_info->unmangled_name](const D3D12_EXPORT_DESC &e) {
										std::string name;
										utf8::unchecked::utf16to8(e.Name, e.Name + std::wcslen(e.Name), std::back_inserter(name));
										return name == record_name;
									}) == desc.pExports + desc.NumExports)
								continue;

							reshade::api::shader_desc shader_desc;
							shader_desc.code = desc.DXILLibrary.pShaderBytecode;
							shader_desc.code_size = desc.DXILLibrary.BytecodeLength;
							shader_desc.entry_point = string_table + function_info->unmangled_name;

							switch (static_cast<D3D12_SHADER_VERSION_TYPE>(function_info->shader_kind))
							{
							case D3D12_SHVER_PIXEL_SHADER:
								pixel_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_VERTEX_SHADER:
								vertex_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_GEOMETRY_SHADER:
								geometry_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_HULL_SHADER:
								hull_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_DOMAIN_SHADER:
								domain_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_COMPUTE_SHADER:
								geometry_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_RAY_GENERATION_SHADER:
								shader_groups.push_back({ reshade::api::shader_group_type::raygen, static_cast<uint32_t>(raygen_desc.size()) });
								raygen_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_INTERSECTION_SHADER:
								intersection_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_ANY_HIT_SHADER:
								any_hit_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_CLOSEST_HIT_SHADER:
								closest_hit_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_MISS_SHADER:
								shader_groups.push_back({ reshade::api::shader_group_type::miss, static_cast<uint32_t>(miss_desc.size()) });
								miss_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_CALLABLE_SHADER:
								shader_groups.push_back({ reshade::api::shader_group_type::callable, static_cast<uint32_t>(callable_desc.size()) });
								callable_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_MESH_SHADER:
								mesh_desc.push_back(shader_desc);
								break;
							case D3D12_SHVER_AMPLIFICATION_SHADER:
								amplication_desc.push_back(shader_desc);
								break;
							}
						}
						break;
					}
				}
			}
			else
			{
				// No point in invoking events when no reflection data can be extracted
				return false;
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
			libraries.push_back({ reinterpret_cast<uintptr_t>(static_cast<const D3D12_EXISTING_COLLECTION_DESC *>(subobject.pDesc)->pExistingCollection) });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
			// Used to e.g. associate local root signatures with specific exports
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
			// Currently unsupported
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_payload_size, 1, const_cast<uint32_t *>(&static_cast<const D3D12_RAYTRACING_SHADER_CONFIG *>(subobject.pDesc)->MaxPayloadSizeInBytes) });
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_attribute_size, 1, const_cast<uint32_t *>(&static_cast<const D3D12_RAYTRACING_SHADER_CONFIG *>(subobject.pDesc)->MaxAttributeSizeInBytes) });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_recursion_depth, 1, const_cast<uint32_t *>(&static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG *>(subobject.pDesc)->MaxTraceRecursionDepth) });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
		{
			const auto &desc = *static_cast<const D3D12_HIT_GROUP_DESC *>(subobject.pDesc);

			reshade::api::shader_group &shader_group = shader_groups.emplace_back();
			switch (desc.Type)
			{
			case D3D12_HIT_GROUP_TYPE_TRIANGLES:
				shader_group.type = reshade::api::shader_group_type::hit_group_triangles;
				break;
			case D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE:
				shader_group.type = reshade::api::shader_group_type::hit_group_aabbs;
				break;
			}

			if (desc.AnyHitShaderImport != nullptr)
			{
				std::string any_hit_name;
				utf8::unchecked::utf16to8(desc.AnyHitShaderImport, desc.AnyHitShaderImport + std::wcslen(desc.AnyHitShaderImport), std::back_inserter(any_hit_name));
				if (const auto it = std::find_if(any_hit_desc.begin(), any_hit_desc.end(),
						[&any_hit_name](const reshade::api::shader_desc &shader) { return shader.entry_point == any_hit_name; });
					it != any_hit_desc.end())
					shader_group.hit_group.any_hit_shader_index = static_cast<uint32_t>(std::distance(any_hit_desc.begin(), it));
			}

			if (desc.ClosestHitShaderImport != nullptr)
			{
				std::string closest_hit_name;
				utf8::unchecked::utf16to8(desc.ClosestHitShaderImport, desc.ClosestHitShaderImport + std::wcslen(desc.ClosestHitShaderImport), std::back_inserter(closest_hit_name));
				if (const auto it = std::find_if(closest_hit_desc.begin(), closest_hit_desc.end(),
						[&closest_hit_name](const reshade::api::shader_desc &shader) { return shader.entry_point == closest_hit_name; });
					it != closest_hit_desc.end())
					shader_group.hit_group.closest_hit_shader_index = static_cast<uint32_t>(std::distance(closest_hit_desc.begin(), it));
			}

			if (desc.IntersectionShaderImport != nullptr)
			{
				std::string intersection_name;
				utf8::unchecked::utf16to8(desc.IntersectionShaderImport, desc.IntersectionShaderImport + std::wcslen(desc.IntersectionShaderImport), std::back_inserter(intersection_name));
				if (const auto it = std::find_if(intersection_desc.begin(), intersection_desc.end(),
						[&intersection_name](const reshade::api::shader_desc &shader) { return shader.entry_point == intersection_name; });
					it != intersection_desc.end())
					shader_group.hit_group.intersection_shader_index = static_cast<uint32_t>(std::distance(intersection_desc.begin(), it));
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1:
			subobjects.push_back({ reshade::api::pipeline_subobject_type::max_recursion_depth, 1, const_cast<uint32_t *>(&static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG1 *>(subobject.pDesc)->MaxTraceRecursionDepth) });
			flags |= reshade::d3d12::convert_pipeline_flags(static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG1 *>(subobject.pDesc)->Flags);
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH:
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
			stream_output_desc = reshade::d3d12::convert_stream_output_desc(*static_cast<const D3D12_STREAM_OUTPUT_DESC *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::stream_output_state, 1, &stream_output_desc });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_BLEND:
			blend_desc = reshade::d3d12::convert_blend_desc(*static_cast<const D3D12_BLEND_DESC *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::blend_state, 1, &blend_desc });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
			sample_mask = *static_cast<const UINT *>(subobject.pDesc);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_mask, 1, &sample_mask });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_RASTERIZER:
			rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(*static_cast<const D3D12_RASTERIZER_DESC2 *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(*static_cast<const D3D12_DEPTH_STENCIL_DESC *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
			input_layout.reserve(static_cast<const D3D12_INPUT_LAYOUT_DESC *>(subobject.pDesc)->NumElements);
			for (UINT elem_index = 0; elem_index < static_cast<const D3D12_INPUT_LAYOUT_DESC *>(subobject.pDesc)->NumElements; ++elem_index)
				input_layout.push_back(reshade::d3d12::convert_input_element(static_cast<const D3D12_INPUT_LAYOUT_DESC *>(subobject.pDesc)->pInputElementDescs[elem_index]));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::input_layout, static_cast<uint32_t>(input_layout.size()), input_layout.data()});
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
			topology = reshade::d3d12::convert_primitive_topology_type(*static_cast<const D3D12_PRIMITIVE_TOPOLOGY_TYPE *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::primitive_topology, 1, &topology });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
			assert(static_cast<const D3D12_RT_FORMAT_ARRAY *>(subobject.pDesc)->NumRenderTargets <= 8);
			for (UINT target_index = 0; target_index < static_cast<const D3D12_RT_FORMAT_ARRAY *>(subobject.pDesc)->NumRenderTargets; ++target_index)
				render_target_formats[target_index] = reshade::d3d12::convert_format(static_cast<const D3D12_RT_FORMAT_ARRAY *>(subobject.pDesc)->RTFormats[target_index]);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::render_target_formats, static_cast<const D3D12_RT_FORMAT_ARRAY *>(subobject.pDesc)->NumRenderTargets, render_target_formats });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
			depth_stencil_format = reshade::d3d12::convert_format(*static_cast<const DXGI_FORMAT *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_format, 1, &depth_stencil_format });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
			sample_count = static_cast<const DXGI_SAMPLE_DESC *>(subobject.pDesc)->Count;
			subobjects.push_back({ reshade::api::pipeline_subobject_type::sample_count, 1, &sample_count });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_FLAGS:
			if ((*static_cast<const D3D12_PIPELINE_STATE_FLAGS *>(subobject.pDesc) & D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS) != 0)
			{
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias);
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_clamp);
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_slope_scaled);
			}
			// Ignore 'D3D12_PIPELINE_STATE_FLAG_DYNAMIC_INDEX_BUFFER_STRIP_CUT'
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(*static_cast<const D3D12_DEPTH_STENCIL_DESC1 *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
			assert(static_cast<const D3D12_VIEW_INSTANCING_DESC *>(subobject.pDesc)->ViewInstanceCount <= D3D12_MAX_VIEW_INSTANCE_COUNT);
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_GENERIC_PROGRAM:
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(*static_cast<const D3D12_DEPTH_STENCIL_DESC2 *>(subobject.pDesc));
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			break;
		default:
			// Unknown sub-object type
			assert(false);
		}
	}

	if (!libraries.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::libraries, static_cast<uint32_t>(libraries.size()), libraries.data() });

	if (!pixel_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::pixel_shader, static_cast<uint32_t>(pixel_desc.size()), pixel_desc.data() });
	if (!vertex_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::vertex_shader, static_cast<uint32_t>(vertex_desc.size()), vertex_desc.data() });
	if (!geometry_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::geometry_shader, static_cast<uint32_t>(geometry_desc.size()), geometry_desc.data() });
	if (!hull_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::hull_shader, static_cast<uint32_t>(hull_desc.size()), hull_desc.data() });
	if (!domain_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::domain_shader, static_cast<uint32_t>(domain_desc.size()), domain_desc.data() });
	if (!compute_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::compute_shader, static_cast<uint32_t>(compute_desc.size()), compute_desc.data() });
	if (!raygen_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::raygen_shader, static_cast<uint32_t>(raygen_desc.size()), raygen_desc.data() });
	if (!any_hit_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::any_hit_shader, static_cast<uint32_t>(any_hit_desc.size()), any_hit_desc.data() });
	if (!closest_hit_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::closest_hit_shader, static_cast<uint32_t>(closest_hit_desc.size()), closest_hit_desc.data() });
	if (!miss_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::miss_shader, static_cast<uint32_t>(miss_desc.size()), miss_desc.data() });
	if (!callable_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::callable_shader, static_cast<uint32_t>(callable_desc.size()), callable_desc.data() });
	if (!mesh_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::mesh_shader, static_cast<uint32_t>(mesh_desc.size()), mesh_desc.data() });
	if (!amplication_desc.empty())
		subobjects.push_back({ reshade::api::pipeline_subobject_type::amplification_shader, static_cast<uint32_t>(amplication_desc.size()), amplication_desc.data() });

	if (internal_desc.Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE)
	{
		if (shader_groups.empty())
			return false;

		subobjects.push_back({ reshade::api::pipeline_subobject_type::shader_groups, static_cast<uint32_t>(shader_groups.size()), shader_groups.data() });
		dynamic_states.push_back(reshade::api::dynamic_state::ray_tracing_pipeline_stack_size);
	}

	subobjects.push_back({ reshade::api::pipeline_subobject_type::flags, 1, &flags });
	subobjects.push_back({ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() });

	if (existing_d3d_pipeline != nullptr) // Do not invoke 'create_pipeline' event for 'AddToStateObject' calls
	{
		assert(_interface_version >= 7);
		hr = static_cast<ID3D12Device7 *>(_orig)->AddToStateObject(&internal_desc, existing_d3d_pipeline, IID_PPV_ARGS(&d3d_pipeline));
	}
	else if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data()))
	{
		reshade::api::pipeline pipeline;
		hr = device_impl::create_pipeline(layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pipeline) ? S_OK : E_FAIL;
		d3d_pipeline = reinterpret_cast<ID3D12StateObject *>(pipeline.handle);
	}
	else
	{
		assert(_interface_version >= 5);
		hr = static_cast<ID3D12Device5 *>(_orig)->CreateStateObject(&internal_desc, IID_PPV_ARGS(&d3d_pipeline));
	}

	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), to_handle(d3d_pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(d3d_pipeline, [this, d3d_pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(d3d_pipeline));
			});
		}
	}

	return true;
}
bool D3D12Device::invoke_create_and_init_pipeline_event(const D3D12_PIPELINE_STATE_STREAM_DESC &internal_desc, ID3D12PipelineState *&d3d_pipeline, HRESULT &hr, bool with_create_pipeline)
{
	if (!reshade::has_addon_event<reshade::addon_event::init_pipeline>() &&
		!reshade::has_addon_event<reshade::addon_event::destroy_pipeline>() &&
		(with_create_pipeline && !reshade::has_addon_event<reshade::addon_event::create_pipeline>()))
		return false;

	reshade::api::pipeline_layout layout = {};

	reshade::api::shader_desc vs_desc, ps_desc, ds_desc, hs_desc, gs_desc, cs_desc, as_desc, ms_desc;
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

	std::vector<reshade::api::dynamic_state> dynamic_states = {
		reshade::api::dynamic_state::primitive_topology,
		reshade::api::dynamic_state::blend_constant,
		reshade::api::dynamic_state::front_stencil_reference_value,
		reshade::api::dynamic_state::back_stencil_reference_value
	};

	std::vector<reshade::api::pipeline_subobject> subobjects;

	for (uintptr_t p = reinterpret_cast<uintptr_t>(internal_desc.pPipelineStateSubobjectStream); p < (reinterpret_cast<uintptr_t>(internal_desc.pPipelineStateSubobjectStream) + internal_desc.SizeInBytes);)
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
			input_layout.reserve(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.NumElements);
			for (UINT elem_index = 0; elem_index < reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.NumElements; ++elem_index)
				input_layout.push_back(reshade::d3d12::convert_input_element(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT *>(p)->data.pInputElementDescs[elem_index]));
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
			for (UINT target_index = 0; target_index < reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.NumRenderTargets; ++target_index)
				render_target_formats[target_index] = reshade::d3d12::convert_format(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS *>(p)->data.RTFormats[target_index]);
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
			if ((reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_FLAGS *>(p)->data & D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS) != 0)
			{
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias);
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_clamp);
				dynamic_states.push_back(reshade::api::dynamic_state::depth_bias_slope_scaled);
			}
			// Ignore 'D3D12_PIPELINE_STATE_FLAG_DYNAMIC_INDEX_BUFFER_STRIP_CUT'
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
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
			as_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_AS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::amplification_shader, 1, &as_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_AS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
			ms_desc = reshade::d3d12::convert_shader_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_MS *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::mesh_shader, 1, &ms_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_MS);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2:
			depth_stencil_desc = reshade::d3d12::convert_depth_stencil_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2 *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1:
			rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER1 *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER1);
			continue;
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2:
			rasterizer_desc = reshade::d3d12::convert_rasterizer_desc(reinterpret_cast<const D3D12_PIPELINE_STATE_STREAM_RASTERIZER2 *>(p)->data);
			subobjects.push_back({ reshade::api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_desc });
			p += sizeof(D3D12_PIPELINE_STATE_STREAM_RASTERIZER2);
			continue;
		default:
			// Unknown sub-object type, break out of the loop
			assert(false);
		}
		break;
	}

	subobjects.push_back({ reshade::api::pipeline_subobject_type::dynamic_pipeline_states, static_cast<uint32_t>(dynamic_states.size()), dynamic_states.data() });

	if (with_create_pipeline)
	{
		if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data()))
		{
			reshade::api::pipeline pipeline;
			hr = device_impl::create_pipeline(layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pipeline) ? S_OK : E_FAIL;
			d3d_pipeline = reinterpret_cast<ID3D12PipelineState *>(pipeline.handle);
		}
		else
		{
			if (!check_and_upgrade_interface(__uuidof(ID3D12Device2)))
				return false;

			hr = static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(&internal_desc, IID_PPV_ARGS(&d3d_pipeline));
		}
	}
	else
	{
		assert(d3d_pipeline != nullptr);
		hr = S_OK;
	}

	if (SUCCEEDED(hr))
	{
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), to_handle(d3d_pipeline));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline>())
		{
			register_destruction_callback_d3dx(d3d_pipeline, [this, d3d_pipeline]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, to_handle(d3d_pipeline));
			});
		}
	}

	return true;
}
bool D3D12Device::invoke_create_and_init_pipeline_event(const D3D12_COMPUTE_PIPELINE_STATE_DESC &internal_desc, ID3D12PipelineState *&d3d_pipeline, HRESULT &hr, bool with_create_pipeline)
{
	struct
	{
		D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE root_signature;
		D3D12_PIPELINE_STATE_STREAM_CS cs;
		D3D12_PIPELINE_STATE_STREAM_NODE_MASK node_mask;
		D3D12_PIPELINE_STATE_STREAM_CACHED_PSO cached_pso;
		D3D12_PIPELINE_STATE_STREAM_FLAGS flags;
	} stream_data = {
		{ internal_desc.pRootSignature },
		{ internal_desc.CS },
		{ internal_desc.NodeMask != 0 ? internal_desc.NodeMask : 1 },
		{ internal_desc.CachedPSO },
		{ internal_desc.Flags }
	};

	return invoke_create_and_init_pipeline_event(D3D12_PIPELINE_STATE_STREAM_DESC { sizeof(stream_data), &stream_data }, d3d_pipeline, hr, with_create_pipeline);
}
bool D3D12Device::invoke_create_and_init_pipeline_event(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &internal_desc, ID3D12PipelineState *&d3d_pipeline, HRESULT &hr, bool with_create_pipeline)
{
	struct
	{
		D3D12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE root_signature;
		D3D12_PIPELINE_STATE_STREAM_VS vs;
		D3D12_PIPELINE_STATE_STREAM_PS ps;
		D3D12_PIPELINE_STATE_STREAM_DS ds;
		D3D12_PIPELINE_STATE_STREAM_HS hs;
		D3D12_PIPELINE_STATE_STREAM_GS gs;
		D3D12_PIPELINE_STATE_STREAM_STREAM_OUTPUT stream_output;
		D3D12_PIPELINE_STATE_STREAM_BLEND_DESC blend_state;
		D3D12_PIPELINE_STATE_STREAM_SAMPLE_MASK sample_mask;
		D3D12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer_state;
		D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL depth_stencil_state;
		D3D12_PIPELINE_STATE_STREAM_INPUT_LAYOUT input_layout;
		D3D12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE ib_strip_cut_value;
		D3D12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitive_topology_type;
		D3D12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS render_target_formats;
		D3D12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT depth_stencil_format;
		D3D12_PIPELINE_STATE_STREAM_SAMPLE_DESC sample_desc;
		D3D12_PIPELINE_STATE_STREAM_NODE_MASK node_mask;
		D3D12_PIPELINE_STATE_STREAM_CACHED_PSO cached_pso;
		D3D12_PIPELINE_STATE_STREAM_FLAGS flags;
	} stream_data = {
		{ internal_desc.pRootSignature },
		{ internal_desc.VS },
		{ internal_desc.PS },
		{ internal_desc.DS },
		{ internal_desc.HS },
		{ internal_desc.GS },
		{ internal_desc.StreamOutput },
		{ internal_desc.BlendState },
		{ internal_desc.SampleMask },
		{ internal_desc.RasterizerState },
		{ internal_desc.DepthStencilState },
		{ internal_desc.InputLayout },
		{ internal_desc.IBStripCutValue },
		{ internal_desc.PrimitiveTopologyType },
		{ D3D12_RT_FORMAT_ARRAY { { internal_desc.RTVFormats[0], internal_desc.RTVFormats[1], internal_desc.RTVFormats[2], internal_desc.RTVFormats[3], internal_desc.RTVFormats[4], internal_desc.RTVFormats[5], internal_desc.RTVFormats[6], internal_desc.RTVFormats[7] }, internal_desc.NumRenderTargets } },
		{ internal_desc.DSVFormat },
		{ internal_desc.SampleDesc },
		{ internal_desc.NodeMask != 0 ? internal_desc.NodeMask : 1 }, // See https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_node_mask#remarks
		{ internal_desc.CachedPSO },
		{ internal_desc.Flags }
	};

	return invoke_create_and_init_pipeline_event(D3D12_PIPELINE_STATE_STREAM_DESC { sizeof(stream_data), &stream_data }, d3d_pipeline, hr, with_create_pipeline);
}

bool D3D12Device::invoke_create_and_init_pipeline_layout_event(UINT node_mask, const void *blob, size_t blob_size, ID3D12RootSignature *&root_signature, HRESULT &hr)
{
	// Copy root signature blob in case static sampler descriptions might get modified
	std::vector<uint8_t> data_mem;
	if (reshade::has_addon_event<reshade::addon_event::create_sampler>())
	{
		data_mem.assign(static_cast<const uint8_t *>(blob), static_cast<const uint8_t *>(blob) + blob_size);
		blob = data_mem.data();
	}

	bool modified = false;
	// Parse DXBC root signature, convert it and call descriptor table and pipeline layout events
	uint32_t param_count = 0;
	std::vector<reshade::api::pipeline_layout_param> params;
	std::vector<std::vector<reshade::api::descriptor_range>> ranges;
	std::vector<reshade::api::descriptor_range_with_static_samplers> ranges_with_static_samplers;
	std::vector<reshade::api::sampler_desc> static_samplers;

	if (const auto part = static_cast<uint32_t *>(const_cast<void *>(
			find_dxbc_part(
				blob,
				blob_size,
				uint32_t('R') | (uint32_t('T') << 8) | (uint32_t('S') << 16) | (uint32_t('0') << 24)))))
	{
		const bool has_pipeline_layout_event = reshade::has_addon_event<reshade::addon_event::create_pipeline_layout>() || reshade::has_addon_event<reshade::addon_event::init_pipeline_layout>();

		const uint32_t version = part[0];

		if (has_pipeline_layout_event &&
			(version == D3D_ROOT_SIGNATURE_VERSION_1_0 || version == D3D_ROOT_SIGNATURE_VERSION_1_1 || version == D3D_ROOT_SIGNATURE_VERSION_1_2))
		{
			param_count = part[1];
			const uint32_t param_offset = part[2];
			auto param_list = part + (param_offset / sizeof(uint32_t));

			params.resize(param_count);
			ranges.resize(param_count);

			for (uint32_t param_index = 0; param_index < param_count; ++param_index, param_list += 3)
			{
				const auto param_type = static_cast<D3D12_ROOT_PARAMETER_TYPE>(param_list[0]);
				const auto shader_visibility = static_cast<D3D12_SHADER_VISIBILITY>(param_list[1]);
				auto param_data = part + (param_list[2] / sizeof(uint32_t));

				switch (param_type)
				{
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				{
					const uint32_t range_count = param_data[0];
					uint32_t descriptor_offset = 0;

					ranges[param_index].resize(range_count);

					// Convert descriptor ranges
					switch (version)
					{
					case D3D_ROOT_SIGNATURE_VERSION_1_0:
					{
						auto range_data = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE *>(part + (param_data[1] / sizeof(uint32_t)));

						for (uint32_t range_index = 0; range_index < range_count; ++range_index, ++range_data)
						{
							reshade::api::descriptor_range &range = ranges[param_index][range_index];
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

							if (range_data->NumDescriptors == UINT_MAX)
								// Only the last entry in a table can have an unbounded size
								assert(range_index == (range_count - 1) || range_data[1].OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
							else
								descriptor_offset = range.binding + range.count;
						}
						break;
					}
					case D3D_ROOT_SIGNATURE_VERSION_1_1:
					case D3D_ROOT_SIGNATURE_VERSION_1_2:
					{
						auto range_data = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE1 *>(part + (param_data[1] / sizeof(uint32_t)));

						for (uint32_t range_index = 0; range_index < range_count; ++range_index, ++range_data)
						{
							reshade::api::descriptor_range &range = ranges[param_index][range_index];
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

							if (range_data->NumDescriptors == UINT_MAX)
								// Only the last entry in a table can have an unbounded size
								assert(range_index == (range_count - 1) || range_data[1].OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
							else
								descriptor_offset = range.binding + range.count;
						}
						break;
					}
					}

					params[param_index].type = reshade::api::pipeline_layout_param_type::descriptor_table;
					params[param_index].descriptor_table.count = range_count;
					params[param_index].descriptor_table.ranges = ranges[param_index].data();
					break;
				}
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				{
					auto constant_data = reinterpret_cast<const D3D12_ROOT_CONSTANTS *>(param_data);

					params[param_index].type = reshade::api::pipeline_layout_param_type::push_constants;

					// Convert root constant description
					reshade::api::constant_range &root_constant = params[param_index].push_constants;
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

					params[param_index].type = reshade::api::pipeline_layout_param_type::push_descriptors;

					reshade::api::descriptor_range &range = params[param_index].push_descriptors;
					range.binding = 0;
					range.dx_register_index = descriptor_data->ShaderRegister;
					range.dx_register_space = descriptor_data->RegisterSpace;
					range.count = 1;
					range.array_size = 1;
					range.visibility = reshade::d3d12::convert_shader_visibility(shader_visibility);

					if (param_type == D3D12_ROOT_PARAMETER_TYPE_CBV)
						range.type = reshade::api::descriptor_type::constant_buffer;
					else if (param_type == D3D12_ROOT_PARAMETER_TYPE_SRV)
						range.type = reshade::api::descriptor_type::buffer_shader_resource_view;
					else
						range.type = reshade::api::descriptor_type::buffer_unordered_access_view;
					break;
				}
				}
			}
		}

		if (has_pipeline_layout_event || reshade::has_addon_event<reshade::addon_event::create_sampler>())
		{
			const uint32_t sampler_count = part[3];
			const uint32_t sampler_offset = part[4];

			if (sampler_count != 0)
			{
				const uint32_t param_index = param_count++;
				params.emplace_back(); // Static samplers do not count towards the root signature size limit

				ranges_with_static_samplers.resize(sampler_count);
				static_samplers.resize(sampler_count);

				auto sampler_data = reinterpret_cast<D3D12_STATIC_SAMPLER_DESC *>(part + (sampler_offset / sizeof(uint32_t)));

				for (uint32_t sampler_index = 0; sampler_index < sampler_count; ++sampler_index)
				{
					reshade::api::sampler_desc &desc = static_samplers[sampler_index] = reshade::d3d12::convert_sampler_desc(sampler_data[sampler_index]);

					if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
					{
						reshade::d3d12::convert_sampler_desc(desc, sampler_data[sampler_index]);
						modified = true; // Force pipeline layout creation with the modified description
					}

					reshade::api::descriptor_range_with_static_samplers &range = ranges_with_static_samplers[sampler_index];
					range.binding = sampler_index;
					range.dx_register_index = sampler_data[sampler_index].ShaderRegister;
					range.dx_register_space = sampler_data[sampler_index].RegisterSpace;
					range.count = 1;
					range.visibility = reshade::d3d12::convert_shader_visibility(sampler_data[sampler_index].ShaderVisibility);
					range.type = reshade::api::descriptor_type::sampler;
					range.static_samplers = &desc;
				}

				params[param_index].type = reshade::api::pipeline_layout_param_type::push_descriptors_with_static_samplers;
				params[param_index].descriptor_table_with_static_samplers.count = sampler_count;
				params[param_index].descriptor_table_with_static_samplers.ranges = ranges_with_static_samplers.data();
			}
		}
	}

	reshade::api::pipeline_layout_param *param_data = params.data();

	if (param_count != 0 && (
		reshade::invoke_addon_event<reshade::addon_event::create_pipeline_layout>(this, param_count, param_data) || modified))
	{
		reshade::api::pipeline_layout layout;
		hr = device_impl::create_pipeline_layout(param_count, param_data, &layout) ? S_OK : E_FAIL;
		root_signature = reinterpret_cast<ID3D12RootSignature *>(layout.handle);
	}
	else
	{
		hr = _orig->CreateRootSignature(node_mask, blob, blob_size, IID_PPV_ARGS(&root_signature));
	}

	if (SUCCEEDED(hr) && param_count != 0)
	{
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline_layout>(this, param_count, param_data, to_handle(root_signature));

		if (reshade::has_addon_event<reshade::addon_event::destroy_pipeline_layout>())
		{
			register_destruction_callback_d3dx(root_signature, [this, root_signature]() {
				reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline_layout>(this, to_handle(root_signature));
			});
		}
	}

	return true;
}

#endif
