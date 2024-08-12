/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "d3d12_impl_device.hpp"
#include "d3d12_impl_command_queue.hpp"
#include "dll_log.hpp"

extern void encode_pix3blob(UINT64(&pix3blob)[64], const char *label, const float color[4]);

reshade::d3d12::command_queue_impl::command_queue_impl(device_impl *device, ID3D12CommandQueue *queue) :
	api_object_impl(queue),
	_device_impl(device)
{
	// Register queue to device (technically need to lock here, since queues may be created on multiple threads simultaneously via 'ID3D12Device::CreateCommandQueue', but it is unlikely an application actually does that)
	_device_impl->_queues.push_back(this);

	// Only create an immediate command list for graphics queues (since the implemented commands do not work on other queue types)
	if (queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		_immediate_cmd_list = new command_list_immediate_impl(device, queue);
		// Ensure the immediate command list was initialized successfully, otherwise disable it
		if (_immediate_cmd_list->_orig == nullptr)
		{
			log::message(log::level::error, "Failed to create immediate command list for queue %p!", _orig);

			delete _immediate_cmd_list;
			_immediate_cmd_list = nullptr;
		}
	}

	// Create auto-reset event and fence for wait for idle synchronization
	_wait_idle_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_wait_idle_fence_event == nullptr ||
		FAILED(_device_impl->_orig->CreateFence(_wait_idle_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_wait_idle_fence))))
	{
		log::message(log::level::error, "Failed to create wait for idle resources for queue %p!", _orig);
	}
}
reshade::d3d12::command_queue_impl::~command_queue_impl()
{
	if (_wait_idle_fence_event != nullptr)
		CloseHandle(_wait_idle_fence_event);

	delete _immediate_cmd_list;

	// Unregister queue from device
	_device_impl->_queues.erase(std::find(_device_impl->_queues.begin(), _device_impl->_queues.end(), this));
}

reshade::api::device *reshade::d3d12::command_queue_impl::get_device()
{
	return _device_impl;
}

reshade::api::command_queue_type reshade::d3d12::command_queue_impl::get_type() const
{
	switch (_orig->GetDesc().Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		return api::command_queue_type::graphics | api::command_queue_type::compute | api::command_queue_type::copy;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		return api::command_queue_type::compute;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		return api::command_queue_type::copy;
	default:
		return static_cast<api::command_queue_type>(0);
	}
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

void reshade::d3d12::command_queue_impl::flush_immediate_command_list() const
{
	if (_immediate_cmd_list != nullptr)
		_immediate_cmd_list->flush();
}

void reshade::d3d12::command_queue_impl::begin_debug_event(const char *label, const float color[4])
{
	assert(label != nullptr);

#if 0
	_orig->BeginEvent(1, label, static_cast<UINT>(std::strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->BeginEvent(2, pix3blob, sizeof(pix3blob));
#endif
}
void reshade::d3d12::command_queue_impl::end_debug_event()
{
	_orig->EndEvent();
}
void reshade::d3d12::command_queue_impl::insert_debug_marker(const char *label, const float color[4])
{
	assert(label != nullptr);

#if 0
	_orig->SetMarker(1, label, static_cast<UINT>(std::strlen(label)));
#else
	UINT64 pix3blob[64];
	encode_pix3blob(pix3blob, label, color);
	_orig->SetMarker(2, pix3blob, sizeof(pix3blob));
#endif
}

bool reshade::d3d12::command_queue_impl::wait(api::fence fence, uint64_t value)
{
	return SUCCEEDED(_orig->Wait(reinterpret_cast<ID3D12Fence *>(fence.handle), value));
}
bool reshade::d3d12::command_queue_impl::signal(api::fence fence, uint64_t value)
{
	flush_immediate_command_list();

	return SUCCEEDED(_orig->Signal(reinterpret_cast<ID3D12Fence *>(fence.handle), value));
}

uint64_t reshade::d3d12::command_queue_impl::get_timestamp_frequency() const
{
	UINT64 frequency;
	if (SUCCEEDED(_orig->GetTimestampFrequency(&frequency)))
		return frequency;

	return 0;
}
