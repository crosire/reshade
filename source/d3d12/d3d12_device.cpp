/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_queue.hpp"
#include "../dxgi/dxgi_device.hpp"
#include "hook_manager.hpp"
#include <assert.h>

#undef ID3D12GraphicsCommandList_Close
#undef ID3D12GraphicsCommandList_DrawInstanced
#undef ID3D12GraphicsCommandList_DrawIndexedInstanced
#undef ID3D12GraphicsCommandList_OMSetRenderTargets
#undef ID3D12GraphicsCommandList_ClearDepthStencilView

extern reshade::log::message &operator<<(reshade::log::message &m, REFIID riid);
std::unordered_map<ID3D12CommandList *, D3D12Device *> d3d12_current_device;
std::mutex d3d12_current_device_mutex;

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

void D3D12Device::add_commandlist_trackers(ID3D12CommandList *command_list, reshade::d3d12::draw_call_tracker &tracker_source)
{
	assert(command_list != nullptr);

	const std::lock_guard<std::mutex> lock(_trackers_per_commandlist_mutex);

	// Merges the counters in counters_source in the counters_per_commandlist for the command list specified
	const auto it = _trackers_per_commandlist.find(command_list);
	if (it == _trackers_per_commandlist.end())
		_trackers_per_commandlist.emplace(command_list, tracker_source);
	else
		it->second.merge(tracker_source);

	tracker_source.reset();
}
void D3D12Device::merge_commandlist_trackers(ID3D12CommandList* command_list, reshade::d3d12::draw_call_tracker &tracker_destination)
{
	assert(command_list != nullptr);

	const std::lock_guard<std::mutex> lock(_trackers_per_commandlist_mutex);

	// Merges the counters logged for the specified command list in the counters destination tracker specified
	const auto it = _trackers_per_commandlist.find(command_list);
	if (it != _trackers_per_commandlist.end())
	{
		tracker_destination.merge(it->second);
		it->second.reset();
	}
}

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
bool D3D12Device::save_depth_texture(ID3D12GraphicsCommandList * cmdList, reshade::d3d12::draw_call_tracker &draw_call_tracker, D3D12_CPU_DESCRIPTOR_HANDLE pDepthStencilView, bool cleared)
{
	if (_runtimes.empty())
		return false;

	const auto runtime = _runtimes.front();

	if (!runtime->depth_buffer_before_clear)
		return false;
	if (!cleared && !runtime->extended_depth_buffer_detection)
		return false;

	assert(&pDepthStencilView != nullptr);

	// Retrieve texture from depth stencil
	com_ptr<ID3D12Resource> depthstencil = _draw_call_tracker.retrieve_depthstencil_from_handle(pDepthStencilView);

	if (depthstencil == nullptr)
		return false;

	D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();
	D3D12_HEAP_PROPERTIES heapProps;
	D3D12_HEAP_FLAGS heapFlags;
	HRESULT hr = depthstencil->GetHeapProperties(&heapProps, &heapFlags);
	if (FAILED(hr))
		return hr;

	// Check if aspect ratio is similar to the back buffer one
	const float width_factor = float(runtime->frame_width()) / float(desc.Width);
	const float height_factor = float(runtime->frame_height()) / float(desc.Height);
	const float aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
	const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

	if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
		return false; // No match, not a good fit

	// In case the depth texture is retrieved, we make a copy of it and store it in an ordered map to use it later in the final rendering stage.
	if ((runtime->cleared_depth_buffer_index == 0 && cleared) || (runtime->clear_DSV_iter <= runtime->cleared_depth_buffer_index))
	{
		// Select an appropriate destination texture
		com_ptr<ID3D12Resource> depth_texture_save = runtime->select_depth_texture_save(desc, &heapProps);
		if (depth_texture_save == nullptr)
			return false;

		// Copy the depth texture. This is necessary because the content of the depth texture is cleared.
		// This way, we can retrieve this content in the final rendering stage
		transition_state(cmdList, depth_texture_save, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->CopyResource(depth_texture_save.get(), depthstencil.get());
		transition_state(cmdList, depth_texture_save, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

		// Store the saved texture in the ordered map.
		draw_call_tracker.track_depth_texture(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil.get(), depthstencil.get(), depth_texture_save, cleared);
	}
	else
	{
		// Store a null depth texture in the ordered map in order to display it even if the user chose a previous cleared texture.
		// This way the texture will still be visible in the depth buffer selection window and the user can choose it.
		draw_call_tracker.track_depth_texture(runtime->depth_buffer_texture_format, runtime->clear_DSV_iter, depthstencil.get(), depthstencil.get(), nullptr, cleared);
	}

	runtime->clear_DSV_iter++;
	return true;
}
void D3D12Device::track_active_rendertargets(ID3D12GraphicsCommandList * cmdList, reshade::d3d12::draw_call_tracker &draw_call_tracker, D3D12_CPU_DESCRIPTOR_HANDLE pDepthStencilView)
{
	if (pDepthStencilView.ptr == 0 || _runtimes.empty())
		return;

	const auto runtime = _runtimes.front();

	// Retrieve texture from depth stencil
	com_ptr<ID3D12Resource> depthstencil = _draw_call_tracker.retrieve_depthstencil_from_handle(pDepthStencilView);

	if (depthstencil == nullptr)
		return;

	D3D12_RESOURCE_DESC desc = depthstencil->GetDesc();

	// Check if aspect ratio is similar to the back buffer one
	const float width_factor = float(runtime->frame_width()) / float(desc.Width);
	const float height_factor = float(runtime->frame_height()) / float(desc.Height);
	const float aspect_ratio = float(runtime->frame_width()) / float(runtime->frame_height());
	const float texture_aspect_ratio = float(desc.Width) / float(desc.Height);

	if (fabs(texture_aspect_ratio - aspect_ratio) > 0.1f || width_factor > 1.85f || height_factor > 1.85f || width_factor < 0.5f || height_factor < 0.5f)
		return; // No match, not a good fit

	if (desc.SampleDesc.Count > 1)
		return;

	current_depthstencil = depthstencil;

	draw_call_tracker.track_rendertargets(runtime->depth_buffer_texture_format, current_depthstencil.get());

	save_depth_texture(cmdList, draw_call_tracker, pDepthStencilView, false);

	draw_call_tracker._depthstencil = current_depthstencil;
}

void D3D12Device::track_cleared_depthstencil(ID3D12GraphicsCommandList * cmdList, reshade::d3d12::draw_call_tracker &draw_call_tracker, D3D12_CLEAR_FLAGS ClearFlags, D3D12_CPU_DESCRIPTOR_HANDLE pDepthStencilView)
{
	if (_runtimes.empty() || &pDepthStencilView == nullptr)
		return;

	const auto runtime = _runtimes.front();

	if (ClearFlags & D3D12_CLEAR_FLAG_DEPTH || (runtime->depth_buffer_more_copies && ClearFlags & D3D12_CLEAR_FLAG_STENCIL))
		save_depth_texture(cmdList, draw_call_tracker, pDepthStencilView, true);
}
#endif

HRESULT STDMETHODCALLTYPE ID3D12GraphicsCommandList_Close(
	ID3D12GraphicsCommandList *pcmdList)
{
	const HRESULT hr = reshade::hooks::call(ID3D12GraphicsCommandList_Close, vtable_from_instance(pcmdList) + 9)(pcmdList);
	reshade::d3d12::draw_call_tracker cmd_list_tracker;

	if (SUCCEEDED(hr) && pcmdList != nullptr)
	{
		std::lock_guard lock(d3d12_current_device_mutex);
		if (const auto it = d3d12_current_device.find(pcmdList); it != d3d12_current_device.end())
			it->second->add_commandlist_trackers(pcmdList, cmd_list_tracker);
	}

	return hr;
}

void STDMETHODCALLTYPE ID3D12GraphicsCommandList_DrawInstanced(
	ID3D12GraphicsCommandList *pcmdList,
	UINT VertexCountPerInstance,
	UINT InstanceCount,
	UINT StartVertexLocation,
	UINT StartInstanceLocation)
{
	reshade::hooks::call(ID3D12GraphicsCommandList_DrawInstanced, vtable_from_instance(pcmdList) + 12)(pcmdList, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);

	if (const auto it = d3d12_current_device.find(pcmdList); it != d3d12_current_device.end())
	{
		std::lock_guard lock(it->second->_trackers_per_commandlist_mutex);
		if (&it->second->_trackers_per_commandlist[pcmdList] != nullptr)
			it->second->_trackers_per_commandlist[pcmdList].on_draw(it->second->current_depthstencil, VertexCountPerInstance * InstanceCount);
	}
}

void STDMETHODCALLTYPE ID3D12GraphicsCommandList_DrawIndexedInstanced(
	ID3D12GraphicsCommandList *pcmdList,
	UINT IndexCountPerInstance,
	UINT InstanceCount,
	UINT StartIndexLocation,
	INT BaseVertexLocation,
	UINT StartInstanceLocation)
{
	reshade::hooks::call(ID3D12GraphicsCommandList_DrawIndexedInstanced, vtable_from_instance(pcmdList) + 13)(pcmdList, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);

	if (const auto it = d3d12_current_device.find(pcmdList); it != d3d12_current_device.end())
	{
		std::lock_guard lock(it->second->_trackers_per_commandlist_mutex);
		if (&it->second->_trackers_per_commandlist[pcmdList] != nullptr)
			it->second->_trackers_per_commandlist[pcmdList].on_draw(it->second->current_depthstencil, IndexCountPerInstance * InstanceCount);
	}
}

void STDMETHODCALLTYPE ID3D12GraphicsCommandList_OMSetRenderTargets(
	ID3D12GraphicsCommandList *pcmdList,
	UINT NumRenderTargetDescriptors,
	const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
	BOOL RTsSingleHandleToDescriptorRange,
	const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor)
{
	reshade::hooks::call(ID3D12GraphicsCommandList_OMSetRenderTargets, vtable_from_instance(pcmdList) + 46)(pcmdList, NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);

	if (pDepthStencilDescriptor != nullptr)
	{
		if (const auto it = d3d12_current_device.find(pcmdList); it != d3d12_current_device.end())
		{
			std::lock_guard<std::mutex> lock(it->second->_trackers_per_commandlist_mutex);
			if (&it->second->_trackers_per_commandlist[pcmdList] != nullptr)
				it->second->track_active_rendertargets(pcmdList, it->second->_trackers_per_commandlist[pcmdList], *pDepthStencilDescriptor);
		}
	}
}

void STDMETHODCALLTYPE ID3D12GraphicsCommandList_ClearDepthStencilView(
	ID3D12GraphicsCommandList *pcmdList,
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
	D3D12_CLEAR_FLAGS ClearFlags,
	FLOAT Depth,
	UINT8 Stencil,
	UINT NumRects,
	const D3D12_RECT *pRects)
{

#if RESHADE_DX12_CAPTURE_DEPTH_BUFFERS
	if (const auto it = d3d12_current_device.find(pcmdList); it != d3d12_current_device.end())
	{
		std::lock_guard<std::mutex> lock(it->second->_trackers_per_commandlist_mutex);
		if (&it->second->_trackers_per_commandlist[pcmdList] != nullptr)
			it->second->track_cleared_depthstencil(pcmdList, it->second->_trackers_per_commandlist[pcmdList], ClearFlags, DepthStencilView);
	}
#endif

	reshade::hooks::call(ID3D12GraphicsCommandList_ClearDepthStencilView, vtable_from_instance(pcmdList) + 47)(pcmdList, DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}

D3D12Device::D3D12Device(ID3D12Device *original) :
	_orig(original),
	_interface_version(0) {
	assert(original != nullptr);
}

void D3D12Device::clear_drawcall_stats(bool all)
{
	_draw_call_tracker.reset(all);
	std::lock_guard lock(_trackers_per_commandlist_mutex);

	_trackers_per_commandlist.clear();
}

bool D3D12Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12Object))
		return true;

	static const IID iid_lookup[] = {
		__uuidof(ID3D12Device),
		__uuidof(ID3D12Device1),
		__uuidof(ID3D12Device2),
		__uuidof(ID3D12Device3),
		__uuidof(ID3D12Device4),
		__uuidof(ID3D12Device5),
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

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12Device::AddRef()
{
	++_ref;

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12Device::Release()
{
	--_ref;

	// Decrease internal reference count and verify it against our own count
	const ULONG ref = _orig->Release();
	if (ref != 0 && _ref != 0)
		return ref;
	else if (ref != 0)
		LOG(WARN) << "Reference count for ID3D12Device" << _interface_version << " object " << this << " is inconsistent: " << ref << ", but expected 0.";

	assert(_ref <= 0);
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D12Device" << _interface_version << " object " << this << ".";
#endif
	delete this;

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
	LOG(INFO) << "Redirecting ID3D12Device::CreateCommandQueue" << '(' << this << ", " << pDesc << ", " << riid << ", " << ppCommandQueue << ')' << " ...";

	if (ppCommandQueue == nullptr)
		return E_INVALIDARG;

	const HRESULT hr = _orig->CreateCommandQueue(pDesc, riid, ppCommandQueue);
	if (FAILED(hr))
	{
		LOG(WARN) << "> 'ID3D12Device::CreateCommandQueue' failed with error code " << std::hex << hr << std::dec << "!";
		return hr;
	}

	// The returned object should alway implement the 'ID3D12CommandQueue' base interface
	const auto command_queue_proxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue *>(*ppCommandQueue));

	// Upgrade to the actual interface version requested here
	if (command_queue_proxy->check_and_upgrade_interface(riid))
		*ppCommandQueue = command_queue_proxy;
	else // Do not hook object if we do not support the requested interface
		delete command_queue_proxy;

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Returning ID3D12CommandQueue object " << *ppCommandQueue << '.';
#endif
	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator)
{
	return _orig->CreateCommandAllocator(type, riid, ppCommandAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	return _orig->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState)
{
	return _orig->CreateComputePipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator *pCommandAllocator, ID3D12PipelineState *pInitialState, REFIID riid, void **ppCommandList)
{
	const HRESULT hr = _orig->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
	if (FAILED(hr))
	{
		LOG(WARN) << "> 'ID3D12Device::CreateCommandList' failed with error code " << std::hex << hr << std::dec << "!";
		return hr;
	}

	if (riid == __uuidof(ID3D12GraphicsCommandList) || riid == __uuidof(ID3D12GraphicsCommandList1))
	{
		ID3D12GraphicsCommandList *const cmdList = static_cast<ID3D12GraphicsCommandList *>(*ppCommandList);
		const std::lock_guard<std::mutex> lock(d3d12_current_device_mutex);
		const auto it = d3d12_current_device.find(cmdList);
		if (it == d3d12_current_device.end())
			d3d12_current_device.emplace(cmdList, this);

		// hook ID3D12GrapgicsCommandList methods
		reshade::hooks::install("ID3D12GraphicsCommandList::Close", vtable_from_instance(cmdList), 9, reinterpret_cast<reshade::hook::address>(&ID3D12GraphicsCommandList_Close));
		reshade::hooks::install("ID3D12GraphicsCommandList::DrawInstanced", vtable_from_instance(cmdList), 12, reinterpret_cast<reshade::hook::address>(&ID3D12GraphicsCommandList_DrawInstanced));
		reshade::hooks::install("ID3D12GraphicsCommandList::DrawIndexedInstanced", vtable_from_instance(cmdList), 13, reinterpret_cast<reshade::hook::address>(&ID3D12GraphicsCommandList_DrawIndexedInstanced));
		reshade::hooks::install("ID3D12GraphicsCommandList::OMSetRenderTargets", vtable_from_instance(cmdList), 46, reinterpret_cast<reshade::hook::address>(&ID3D12GraphicsCommandList_OMSetRenderTargets));
		reshade::hooks::install("ID3D12GraphicsCommandList::ClearDepthStencilView", vtable_from_instance(cmdList), 47, reinterpret_cast<reshade::hook::address>(&ID3D12GraphicsCommandList_ClearDepthStencilView));
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return _orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc, REFIID riid, void **ppvHeap)
{
	return  _orig->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
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
	_orig->CreateShaderResourceView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource, ID3D12Resource *pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	_orig->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(ID3D12Resource *pResource, const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	_orig->CreateRenderTargetView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(ID3D12Resource *pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
	_orig->CreateDepthStencilView(pResource, pDesc, DestDescriptor);

	if (pResource != nullptr && !_runtimes.empty())
	{
		const auto runtime = _runtimes.front();
		_draw_call_tracker.track_depthstencil_resource_by_handle(DestDescriptor, pResource);
		runtime->on_create_depthstencil_view(pResource);
	}
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
	// Remove D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE flag so that we can access depth stencil resources in post-processing shaders.
	// The flag is only valid in combination with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL anyway (see https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags), so can always remove it.
	D3D12_RESOURCE_DESC new_desc = *pResourceDesc;
	new_desc.Flags &= ~D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	return _orig->CreateCommittedResource(pHeapProperties, HeapFlags, &new_desc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
	return _orig->CreateHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	return _orig->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, REFIID riid, void **ppvResource)
{
	return _orig->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
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
	return static_cast<ID3D12Device4 *>(_orig)->CreateCommandList1(NodeMask, Type, Flags, riid, ppCommandList);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC *pDesc, REFIID riid, void **ppSession)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateProtectedResourceSession(pDesc, riid, ppSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource1(const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riidResource, void **ppvResource)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, ppvResource);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap1(const D3D12_HEAP_DESC *pDesc, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvHeap)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateHeap1(pDesc, pProtectedSession, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource1(const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *pOptimizedClearValue, ID3D12ProtectedResourceSession *pProtectedSession, REFIID riid, void **ppvResource)
{
	assert(_interface_version >= 4);
	return static_cast<ID3D12Device4 *>(_orig)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
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
