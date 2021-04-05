/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "render_d3d12.hpp"
#include "render_d3d12_utils.hpp"

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
	reshade::addon::load_addons();

	reshade::invoke_addon_event_without_trampoline<reshade::addon_event::init_device>(this);
#endif
}
reshade::d3d12::device_impl::~device_impl()
{
	assert(_queues.empty()); // All queues should have been unregistered and destroyed by the application at this point

#if RESHADE_ADDON
	reshade::invoke_addon_event_without_trampoline<reshade::addon_event::destroy_device>(this);

	reshade::addon::unload_addons();
#endif
}

bool reshade::d3d12::device_impl::check_format_support(uint32_t format, api::resource_usage usage) const
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

bool reshade::d3d12::device_impl::check_resource_handle_valid(api::resource_handle resource) const
{
	return resource.handle != 0 && _resources.has_object(reinterpret_cast<ID3D12Resource *>(resource.handle));
}
bool reshade::d3d12::device_impl::check_resource_view_handle_valid(api::resource_view_handle view) const
{
	const std::lock_guard<std::mutex> lock(_mutex);
	return _views.find(view.handle) != _views.end();
}

bool reshade::d3d12::device_impl::create_resource(const api::resource_desc &desc, api::resource_usage initial_state, api::resource_handle *out_resource)
{
	assert((desc.usage & initial_state) == initial_state);

	D3D12_RESOURCE_DESC internal_desc = {};
	D3D12_HEAP_PROPERTIES heap_props = {};
	convert_resource_desc(desc, internal_desc, heap_props);
	if (desc.type == api::resource_type::buffer)
		internal_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	if (ID3D12Resource *resource = nullptr;
		SUCCEEDED(_orig->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &internal_desc, convert_resource_usage_to_states(initial_state), nullptr, IID_PPV_ARGS(&resource))))
	{
		_resources.register_object(resource);
		*out_resource = { reinterpret_cast<uintptr_t>(resource) };
		return true;
	}
	else
	{
		*out_resource = { 0 };
		return false;
	}
}
bool reshade::d3d12::device_impl::create_resource_view(api::resource_handle resource, api::resource_usage usage_type, const api::resource_view_desc &desc, api::resource_view_handle *out_view)
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
			*out_view = { descriptor_handle.ptr };
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
			*out_view = { descriptor_handle.ptr };
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
			*out_view = { descriptor_handle.ptr };
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
			*out_view = { descriptor_handle.ptr };
			return true;
		}
	}

	*out_view = { 0 };
	return false;
}

void reshade::d3d12::device_impl::destroy_resource(api::resource_handle resource)
{
	assert(resource.handle != 0);
	reinterpret_cast<ID3D12Resource *>(resource.handle)->Release();
}
void reshade::d3d12::device_impl::destroy_resource_view(api::resource_view_handle view)
{
	assert(view.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	_views.erase(view.handle);

	// Mark free slot in the descriptor heap
	for (UINT type = 0; type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++type)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE heap_start = _resource_view_pool[type]->GetCPUDescriptorHandleForHeapStart();
		if (view.handle >= heap_start.ptr && view.handle < (heap_start.ptr + _resource_view_pool_state[type].size() * _descriptor_handle_size[type]))
		{
			const size_t index = static_cast<size_t>(view.handle - heap_start.ptr) / _descriptor_handle_size[type];
			_resource_view_pool_state[type][index] = false;
			break;
		}
	}
}

void reshade::d3d12::device_impl::get_resource_from_view(api::resource_view_handle view, api::resource_handle *out_resource) const
{
	assert(view.handle != 0);

	const std::lock_guard<std::mutex> lock(_mutex);
	if (const auto it = _views.find(view.handle); it != _views.end())
		*out_resource = { reinterpret_cast<uintptr_t>(it->second) };
	else
		*out_resource = { 0 };
}

reshade::api::resource_desc reshade::d3d12::device_impl::get_resource_desc(api::resource_handle resource) const
{
	assert(resource.handle != 0);
	const D3D12_RESOURCE_DESC internal_desc = reinterpret_cast<ID3D12Resource *>(resource.handle)->GetDesc();

	D3D12_HEAP_FLAGS heap_flags;
	D3D12_HEAP_PROPERTIES heap_props = {};
	reinterpret_cast<ID3D12Resource *>(resource.handle)->GetHeapProperties(&heap_props, &heap_flags);

	return convert_resource_desc(internal_desc, heap_props);
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
#if RESHADE_ADDON
	if (_has_commands) // Do not call add-on event for immediate command list
	{
		reshade::invoke_addon_event_without_trampoline<reshade::addon_event::init_command_list>(this);
	}
#endif
}
reshade::d3d12::command_list_impl::~command_list_impl()
{
#if RESHADE_ADDON
	if (_has_commands)
	{
		reshade::invoke_addon_event_without_trampoline<reshade::addon_event::destroy_command_list>(this);
	}
#endif
}

void reshade::d3d12::command_list_impl::copy_resource(api::resource_handle source, api::resource_handle destination)
{
	_has_commands = true;

	assert(source.handle != 0 && destination.handle != 0);
	_orig->CopyResource(reinterpret_cast<ID3D12Resource *>(destination.handle), reinterpret_cast<ID3D12Resource *>(source.handle));
}

void reshade::d3d12::command_list_impl::transition_state(api::resource_handle resource, api::resource_usage old_layout, api::resource_usage new_layout)
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

void reshade::d3d12::command_list_impl::clear_depth_stencil_view(api::resource_view_handle dsv, uint32_t clear_flags, float depth, uint8_t stencil)
{
	_has_commands = true;

	assert(dsv.handle != 0);
	_orig->ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(dsv.handle) }, static_cast<D3D12_CLEAR_FLAGS>(clear_flags), depth, stencil, 0, nullptr);
}
void reshade::d3d12::command_list_impl::clear_render_target_view(api::resource_view_handle rtv, const float color[4])
{
	_has_commands = true;

	assert(rtv.handle != 0);
	_orig->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE { static_cast<SIZE_T>(rtv.handle) }, color, 0, nullptr);
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

#if RESHADE_ADDON
	reshade::invoke_addon_event_without_trampoline<reshade::addon_event::init_command_queue>(this);
#endif
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
#if RESHADE_ADDON
	reshade::invoke_addon_event_without_trampoline<reshade::addon_event::destroy_command_queue>(this);
#endif

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
