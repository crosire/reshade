/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_list_immediate.hpp"

reshade::d3d12::command_list_immediate_impl::command_list_immediate_impl(device_impl *device) :
	command_list_impl(device, nullptr)
{
	// Create multiple command allocators to buffer for multiple frames
	for (uint32_t i = 0; i < NUM_COMMAND_FRAMES; ++i)
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
