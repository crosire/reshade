/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_device_downlevel.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "render_d3d12_utils.hpp"

D3D12Device::D3D12Device(ID3D12Device *original) :
	device_impl(original),
	_interface_version(0)
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
			LOG(DEBUG) << "Upgraded ID3D12Device" << _interface_version << " object " << this << " to ID3D12Device" << version << '.';
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
		return _orig->Release(), ref;

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
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D12Device::CreateCommandQueue" << " failed with error code " << hr << '.';
		return hr;
	}

	assert(ppCommandQueue != nullptr);

	const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

	// Upgrade to the actual interface version requested here
	if (command_queue_proxy->check_and_upgrade_interface(riid))
	{
#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "> Returning ID3D12CommandQueue" << command_queue_proxy->_interface_version << " object " << command_queue_proxy << '.';
#endif
		*ppCommandQueue = command_queue_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		delete command_queue_proxy; // Delete instead of release to keep reference count untouched
	}

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC new_desc = *pDesc;

	std::vector<uint8_t> temp_vs_code;
	if (new_desc.VS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_vs_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.VS.pShaderBytecode)
				{
					temp_vs_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.VS.BytecodeLength = code_size;
					new_desc.VS.pShaderBytecode = temp_vs_code.data();
				}
				return true;
			}, this, new_desc.VS.pShaderBytecode, new_desc.VS.BytecodeLength);
	std::vector<uint8_t> temp_ps_code;
	if (new_desc.PS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_ps_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.PS.pShaderBytecode)
				{
					temp_ps_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.PS.BytecodeLength = code_size;
					new_desc.PS.pShaderBytecode = temp_ps_code.data();
				}
				return true;
			}, this, new_desc.PS.pShaderBytecode, new_desc.PS.BytecodeLength);
	std::vector<uint8_t> temp_ds_code;
	if (new_desc.DS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_ds_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.PS.pShaderBytecode)
				{
					temp_ds_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.DS.BytecodeLength = code_size;
					new_desc.DS.pShaderBytecode = temp_ds_code.data();
				}
				return true;
			}, this, new_desc.DS.pShaderBytecode, new_desc.DS.BytecodeLength);
	std::vector<uint8_t> temp_hs_code;
	if (new_desc.HS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_hs_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.HS.pShaderBytecode)
				{
					temp_hs_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.HS.BytecodeLength = code_size;
					new_desc.HS.pShaderBytecode = temp_hs_code.data();
				}
				return true;
			}, this, new_desc.HS.pShaderBytecode, new_desc.HS.BytecodeLength);
	std::vector<uint8_t> temp_gs_code;
	if (new_desc.GS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_gs_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.GS.pShaderBytecode)
				{
					temp_gs_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.GS.BytecodeLength = code_size;
					new_desc.GS.pShaderBytecode = temp_gs_code.data();
				}
				return true;
			}, this, new_desc.GS.pShaderBytecode, new_desc.GS.BytecodeLength);

	return _orig->CreateGraphicsPipelineState(&new_desc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D12_COMPUTE_PIPELINE_STATE_DESC new_desc = *pDesc;

	std::vector<uint8_t> temp_cs_code;
	if (new_desc.CS.BytecodeLength != 0)
		reshade::invoke_addon_event<reshade::addon_event::create_shader_module>(
			[&new_desc, &temp_cs_code](reshade::api::device *, const void *code, size_t code_size) {
				if (code != new_desc.CS.pShaderBytecode)
				{
					temp_cs_code.assign(static_cast<const uint8_t *>(code), static_cast<const uint8_t *>(code) + code_size);
					new_desc.CS.BytecodeLength = code_size;
					new_desc.CS.pShaderBytecode = temp_cs_code.data();
				}
				return true;
			}, this, new_desc.CS.pShaderBytecode, new_desc.CS.BytecodeLength);

	return _orig->CreateComputePipelineState(&new_desc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **ppCommandList)
{
	const HRESULT hr = _orig->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D12Device::CreateCommandList" << " failed with error code " << hr << '.';
		return hr;
	}

	assert(ppCommandList != nullptr);

	const auto command_list_proxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList *>(*ppCommandList));

	// Upgrade to the actual interface version requested here (and only hook graphics command lists)
	if (command_list_proxy->check_and_upgrade_interface(riid))
	{
		*ppCommandList = command_list_proxy;
	}
	else // Do not hook object if we do not support the requested interface
	{
		delete command_list_proxy; // Delete instead of release to keep reference count untouched
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return _orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
	return _orig->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
}
UINT    STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
	return _orig->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void **ppvRootSignature)
{
	return _orig->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature);
}
void    STDMETHODCALLTYPE D3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	_orig->CreateConstantBufferView(pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateShaderResourceView(ID3D12Resource *pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	if (pResource != nullptr)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC new_desc =
			pDesc != nullptr ? *pDesc : D3D12_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_SRV_DIMENSION_UNKNOWN };

		reshade::api::resource_view_handle out = { 0 };
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[this, &new_desc, DestDescriptor](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
				if (usage_type != reshade::api::resource_usage::shader_resource || out == nullptr)
					return false;
				reshade::d3d12::convert_resource_view_desc(desc, new_desc);

				_orig->CreateShaderResourceView(reinterpret_cast<ID3D12Resource *>(resource.handle), new_desc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN ? &new_desc : nullptr, DestDescriptor);

				register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), DestDescriptor);
				*out = { DestDescriptor.ptr };
				return true;
			}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, reshade::d3d12::convert_resource_view_desc(new_desc), &out);
	}
	else
#endif
		// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview)
		_orig->CreateShaderResourceView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	if (pResource != nullptr)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC new_desc =
			pDesc != nullptr ? *pDesc : D3D12_UNORDERED_ACCESS_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_UNKNOWN };

		reshade::api::resource_view_handle out = { 0 };
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[this, &new_desc, pCounterResource, DestDescriptor](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
				if (usage_type != reshade::api::resource_usage::unordered_access || out == nullptr)
					return false;
				reshade::d3d12::convert_resource_view_desc(desc, new_desc);

				_orig->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource *>(resource.handle), pCounterResource, new_desc.ViewDimension != D3D12_UAV_DIMENSION_UNKNOWN ? &new_desc : nullptr, DestDescriptor);

				register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), DestDescriptor);
				*out = { DestDescriptor.ptr };
				return true;
			}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::unordered_access, reshade::d3d12::convert_resource_view_desc(new_desc), &out);
	}
	else
#endif
		// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview)
		_orig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	if (pResource != nullptr)
	{
		D3D12_RENDER_TARGET_VIEW_DESC new_desc =
			pDesc != nullptr ? *pDesc : D3D12_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_RTV_DIMENSION_UNKNOWN };

		reshade::api::resource_view_handle out = { 0 };
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[this, &new_desc, DestDescriptor](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
				if (usage_type != reshade::api::resource_usage::render_target || out == nullptr)
					return false;
				reshade::d3d12::convert_resource_view_desc(desc, new_desc);

				_orig->CreateRenderTargetView(reinterpret_cast<ID3D12Resource *>(resource.handle), new_desc.ViewDimension != D3D12_RTV_DIMENSION_UNKNOWN ? &new_desc : nullptr, DestDescriptor);

				register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), DestDescriptor);
				*out = { DestDescriptor.ptr };
				return true;
			}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, reshade::d3d12::convert_resource_view_desc(new_desc), &out);
	}
	else
#endif
		// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createrendertargetview)
		_orig->CreateRenderTargetView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
#if RESHADE_ADDON
	if (pResource != nullptr)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC new_desc =
			pDesc != nullptr ? *pDesc : D3D12_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D12_DSV_DIMENSION_UNKNOWN };

		reshade::api::resource_view_handle out = { 0 };
		reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(
			[this, &new_desc, DestDescriptor](reshade::api::device *, reshade::api::resource_handle resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc &desc, reshade::api::resource_view_handle *out) {
				if (usage_type != reshade::api::resource_usage::depth_stencil || out == nullptr)
					return false;
				reshade::d3d12::convert_resource_view_desc(desc, new_desc);

				_orig->CreateDepthStencilView(reinterpret_cast<ID3D12Resource *>(resource.handle), new_desc.ViewDimension != D3D12_DSV_DIMENSION_UNKNOWN ? &new_desc : nullptr, DestDescriptor);

				register_resource_view(reinterpret_cast<ID3D12Resource *>(resource.handle), DestDescriptor);
				*out = { DestDescriptor.ptr };
				return true;
			}, this, reshade::api::resource_handle { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, reshade::d3d12::convert_resource_view_desc(new_desc), &out);
	}
	else
#endif
		// Calling with no resource is valid and used to initialize a null descriptor (see https://docs.microsoft.com/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdepthstencilview)
		_orig->CreateDepthStencilView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	_orig->CreateSampler(pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptors(UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts, const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts, const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
	_orig->CopyDescriptors(NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType);
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
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
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riidResource, void **ppvResource)
{
	if (pHeapProperties == nullptr || pResourceDesc == nullptr)
		return E_INVALIDARG;

	D3D12_RESOURCE_DESC new_desc = *pResourceDesc;
	D3D12_HEAP_PROPERTIES heap_props = *pHeapProperties;

	HRESULT hr = E_FAIL;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, &heap_props, HeapFlags, pOptimizedClearValue, riidResource](reshade::api::device *, const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, const reshade::api::mapped_subresource *initial_data, reshade::api::resource_handle *out) {
			if (initial_data != nullptr)
				return false;
			reshade::d3d12::convert_resource_desc(desc, new_desc, heap_props);

			ID3D12Resource *resource = nullptr;
			hr = _orig->CreateCommittedResource(&heap_props, HeapFlags, &new_desc, reshade::d3d12::convert_resource_usage_to_states(initial_state), pOptimizedClearValue, riidResource, (out != nullptr) ? reinterpret_cast<void **>(&resource) : nullptr);
			if (SUCCEEDED(hr))
			{
				if (out != nullptr)
				{
					_resources.register_object(resource);
#if RESHADE_ADDON
					if (new_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
						register_buffer_gpu_address(resource, new_desc.Width);
#endif
					*out = { reinterpret_cast<uintptr_t>(resource) };
				}
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D12Device::CreateCommittedResource" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Dimension                               | " << std::setw(39) << new_desc.Dimension << " |";
				LOG(DEBUG) << "  | Alignment                               | " << std::setw(39) << new_desc.Alignment << " |";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | DepthOrArraySize                        | " << std::setw(39) << new_desc.DepthOrArraySize << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Layout                                  | " << std::setw(39) << new_desc.Layout << " |";
				LOG(DEBUG) << "  | Flags                                   | " << std::setw(39) << std::hex << new_desc.Flags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d12::convert_resource_desc(new_desc, heap_props), static_cast<reshade::api::resource_usage>(InitialResourceState), nullptr, (ppvResource != nullptr) ? &out : nullptr);
	if (ppvResource != nullptr)
		*ppvResource = reinterpret_cast<void *>(out.handle);

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

	D3D12_RESOURCE_DESC new_desc = *pDesc;
	D3D12_HEAP_PROPERTIES heap_props = pHeap->GetDesc().Properties;

	HRESULT hr = E_FAIL;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, &heap_props, pHeap, HeapOffset, pOptimizedClearValue, riid](reshade::api::device *, const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, const reshade::api::mapped_subresource *initial_data, reshade::api::resource_handle *out) {
			if (desc.mem_usage != reshade::d3d12::convert_memory_usage(heap_props.Type) || initial_data != nullptr)
				return false;
			reshade::d3d12::convert_resource_desc(desc, new_desc, heap_props);

			ID3D12Resource *resource = nullptr;
			hr = _orig->CreatePlacedResource(pHeap, HeapOffset, &new_desc, reshade::d3d12::convert_resource_usage_to_states(initial_state), pOptimizedClearValue, riid, (out != nullptr) ? reinterpret_cast<void **>(resource) : nullptr);
			if (SUCCEEDED(hr))
			{
				if (out != nullptr)
				{
					_resources.register_object(resource);
#if RESHADE_ADDON
					if (new_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
						register_buffer_gpu_address(resource, new_desc.Width);
#endif
					*out = { reinterpret_cast<uintptr_t>(resource) };
				}
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D12Device::CreatePlacedResource" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Dimension                               | " << std::setw(39) << new_desc.Dimension << " |";
				LOG(DEBUG) << "  | Alignment                               | " << std::setw(39) << new_desc.Alignment << " |";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | DepthOrArraySize                        | " << std::setw(39) << new_desc.DepthOrArraySize << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Layout                                  | " << std::setw(39) << new_desc.Layout << " |";
				LOG(DEBUG) << "  | Flags                                   | " << std::setw(39) << std::hex << new_desc.Flags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d12::convert_resource_desc(new_desc, heap_props), static_cast<reshade::api::resource_usage>(InitialState), nullptr, (ppvResource != nullptr) ? &out : nullptr);
	if (ppvResource != nullptr)
		*ppvResource = reinterpret_cast<void *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D12_RESOURCE_DESC new_desc = *pDesc;
	D3D12_HEAP_PROPERTIES heap_props = {};

	HRESULT hr = E_FAIL;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, &heap_props, pOptimizedClearValue, riid](reshade::api::device *, const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, const reshade::api::mapped_subresource *initial_data, reshade::api::resource_handle *out) {
			if (desc.mem_usage != reshade::api::memory_usage::unknown || initial_data != nullptr)
				return false;
			reshade::d3d12::convert_resource_desc(desc, new_desc, heap_props);

			ID3D12Resource *resource = nullptr;
			hr = _orig->CreateReservedResource(&new_desc, reshade::d3d12::convert_resource_usage_to_states(initial_state), pOptimizedClearValue, riid, (out != nullptr) ? reinterpret_cast<void **>(resource) : nullptr);
			if (SUCCEEDED(hr))
			{
				if (out != nullptr)
				{
					_resources.register_object(resource);
					*out = { reinterpret_cast<uintptr_t>(resource) };
				}
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D12Device::CreateReservedResource" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Dimension                               | " << std::setw(39) << new_desc.Dimension << " |";
				LOG(DEBUG) << "  | Alignment                               | " << std::setw(39) << new_desc.Alignment << " |";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | DepthOrArraySize                        | " << std::setw(39) << new_desc.DepthOrArraySize << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Layout                                  | " << std::setw(39) << new_desc.Layout << " |";
				LOG(DEBUG) << "  | Flags                                   | " << std::setw(39) << std::hex << new_desc.Flags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d12::convert_resource_desc(new_desc, heap_props), static_cast<reshade::api::resource_usage>(InitialState), nullptr, (ppvResource != nullptr) ? &out : nullptr);
	if (ppvResource != nullptr)
		*ppvResource = reinterpret_cast<void *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject, const SECURITY_ATTRIBUTES *pAttributes, DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
	return _orig->CreateSharedHandle(pObject, pAttributes, Access, Name, pHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
	return _orig->OpenSharedHandle(NTHandle, riid, ppvObj);
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
	return _orig->CreateQueryHeap(pDesc, riid, ppvHeap);
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
	return static_cast<ID3D12Device2 *>(_orig)->CreatePipelineState(pDesc, riid, ppPipelineState);
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
	if (FAILED(hr))
	{
		LOG(WARN) << "ID3D12Device4::CreateCommandList1" << " failed with error code " << hr << '.';
		return hr;
	}

	const auto command_list_proxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList *>(*ppCommandList));

	// Upgrade to the actual interface version requested here (and only hook graphics command lists)
	if (command_list_proxy->check_and_upgrade_interface(riid))
	{
		*ppCommandList = command_list_proxy;
	}
	else // Do not hook object if we do not support the requested interface or this is a compute command list
	{
		delete command_list_proxy; // Delete instead of release to keep reference count untouched
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
	if (pHeapProperties == nullptr || pDesc == nullptr)
		return E_INVALIDARG;

	D3D12_RESOURCE_DESC new_desc = *pDesc;
	D3D12_HEAP_PROPERTIES heap_props = *pHeapProperties;

	HRESULT hr = E_FAIL;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, &heap_props, HeapFlags, pOptimizedClearValue, pProtectedSession, riidResource](reshade::api::device *, const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, const reshade::api::mapped_subresource *initial_data, reshade::api::resource_handle *out) {
			if (initial_data != nullptr)
				return false;
			reshade::d3d12::convert_resource_desc(desc, new_desc, heap_props);

			ID3D12Resource *resource = nullptr;
			assert(_interface_version >= 4);
			hr = static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(&heap_props, HeapFlags, &new_desc, reshade::d3d12::convert_resource_usage_to_states(initial_state), pOptimizedClearValue, pProtectedSession, riidResource, (out != nullptr) ? reinterpret_cast<void **>(&resource) : nullptr);
			if (SUCCEEDED(hr))
			{
				if (out != nullptr)
				{
					_resources.register_object(resource);
#if RESHADE_ADDON
					if (new_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
						register_buffer_gpu_address(resource, new_desc.Width);
#endif
					*out = { reinterpret_cast<uintptr_t>(resource) };
				}
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D12Device::CreateCommittedResource1" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Dimension                               | " << std::setw(39) << new_desc.Dimension << " |";
				LOG(DEBUG) << "  | Alignment                               | " << std::setw(39) << new_desc.Alignment << " |";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | DepthOrArraySize                        | " << std::setw(39) << new_desc.DepthOrArraySize << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Layout                                  | " << std::setw(39) << new_desc.Layout << " |";
				LOG(DEBUG) << "  | Flags                                   | " << std::setw(39) << std::hex << new_desc.Flags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d12::convert_resource_desc(new_desc, heap_props), static_cast<reshade::api::resource_usage>(InitialResourceState), nullptr, (ppvResource != nullptr) ? &out : nullptr);
	if (ppvResource != nullptr)
		*ppvResource = reinterpret_cast<void *>(out.handle);

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap1(const D3D12_HEAP_DESC *pDesc, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateHeap1(pDesc, pProtectedSession, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource1(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
	if (pDesc == nullptr)
		return E_INVALIDARG;

	D3D12_RESOURCE_DESC new_desc = *pDesc;
	D3D12_HEAP_PROPERTIES heap_props = {};

	HRESULT hr = E_FAIL;
	reshade::api::resource_handle out = { 0 };
	reshade::invoke_addon_event<reshade::addon_event::create_resource>(
		[this, &hr, &new_desc, &heap_props, pOptimizedClearValue, pProtectedSession, riid](reshade::api::device *, const reshade::api::resource_desc &desc, reshade::api::resource_usage initial_state, const reshade::api::mapped_subresource *initial_data, reshade::api::resource_handle *out) {
			if (desc.mem_usage != reshade::api::memory_usage::unknown || initial_data != nullptr)
				return false;
			reshade::d3d12::convert_resource_desc(desc, new_desc, heap_props);

			ID3D12Resource *resource = nullptr;
			assert(_interface_version >= 4);
			hr = static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(&new_desc, reshade::d3d12::convert_resource_usage_to_states(initial_state), pOptimizedClearValue, pProtectedSession, riid, (out != nullptr) ? reinterpret_cast<void **>(&resource) : nullptr);
			if (SUCCEEDED(hr))
			{
				if (out != nullptr)
				{
					_resources.register_object(resource);
					*out = { reinterpret_cast<uintptr_t>(resource) };
				}
				return true;
			}
			else
			{
				LOG(WARN) << "ID3D12Device::CreateReservedResource1" << " failed with error code " << hr << '.';
#if RESHADE_VERBOSE_LOG
				LOG(DEBUG) << "> Dumping description:";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
				LOG(DEBUG) << "  | Dimension                               | " << std::setw(39) << new_desc.Dimension << " |";
				LOG(DEBUG) << "  | Alignment                               | " << std::setw(39) << new_desc.Alignment << " |";
				LOG(DEBUG) << "  | Width                                   | " << std::setw(39) << new_desc.Width << " |";
				LOG(DEBUG) << "  | Height                                  | " << std::setw(39) << new_desc.Height << " |";
				LOG(DEBUG) << "  | DepthOrArraySize                        | " << std::setw(39) << new_desc.DepthOrArraySize << " |";
				LOG(DEBUG) << "  | MipLevels                               | " << std::setw(39) << new_desc.MipLevels << " |";
				LOG(DEBUG) << "  | Format                                  | " << std::setw(39) << new_desc.Format << " |";
				LOG(DEBUG) << "  | SampleCount                             | " << std::setw(39) << new_desc.SampleDesc.Count << " |";
				LOG(DEBUG) << "  | SampleQuality                           | " << std::setw(39) << new_desc.SampleDesc.Quality << " |";
				LOG(DEBUG) << "  | Layout                                  | " << std::setw(39) << new_desc.Layout << " |";
				LOG(DEBUG) << "  | Flags                                   | " << std::setw(39) << std::hex << new_desc.Flags << std::dec << " |";
				LOG(DEBUG) << "  +-----------------------------------------+-----------------------------------------+";
#endif
				return false;
			}
		}, this, reshade::d3d12::convert_resource_desc(new_desc, heap_props), static_cast<reshade::api::resource_usage>(InitialState), nullptr, (ppvResource != nullptr) ? &out : nullptr);
	if (ppvResource != nullptr)
		*ppvResource = reinterpret_cast<void *>(out.handle);

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
	assert(_interface_version >= 5);
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
