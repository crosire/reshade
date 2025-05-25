/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"
#include "addon_manager.hpp"
#include "runtime_manager.hpp"
#include <algorithm> // std::find

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	swapchain_d3d12on7_impl(queue->_device, original),
	_parent_queue(queue)
{
	assert(_orig != nullptr && _parent_queue != nullptr);
	assert(!_back_buffers.empty());

	reshade::create_effect_runtime(this, queue);
}
D3D12CommandQueueDownlevel::~D3D12CommandQueueDownlevel()
{
	if (_back_buffers[0] != nullptr)
	{
		reshade::reset_effect_runtime(this);

#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this, false);
#endif
	}

	reshade::destroy_effect_runtime(this);
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	// IUnknown is handled by D3D12CommandQueue
	if (riid == __uuidof(this) ||
		riid == __uuidof(ID3D12CommandQueueDownlevel))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _parent_queue->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::AddRef()
{
	return _parent_queue->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Release()
{
	return _parent_queue->Release();
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
	assert(pSourceTex2D != nullptr);

	// Synchronize access to this command queue while events are invoked and the immediate command list may be accessed
	std::unique_lock<std::recursive_mutex> lock(_parent_queue->_mutex);

	_hwnd = hWindow;
	_swap_index = (_swap_index + 1) % static_cast<UINT>(_back_buffers.size());

	// Update source texture render target view
	if (_back_buffers[_swap_index] != pSourceTex2D)
	{
		reshade::reset_effect_runtime(this);

#if RESHADE_ADDON
		const bool resize = (_back_buffers[0] != nullptr);
		if (resize)
			reshade::invoke_addon_event<reshade::addon_event::destroy_swapchain>(this, resize);
#endif

		// Reduce number of back buffers if less are used than predicted
		if (const auto it = std::find(_back_buffers.begin(), _back_buffers.end(), pSourceTex2D); it != _back_buffers.end())
			_back_buffers.erase(it);
		else
			_back_buffers[_swap_index] = pSourceTex2D;

		// Do not initialize before all back buffers have been set
		// The first to be set is at index 1 due to the addition above, so it is sufficient to check the last to be set, which will be at index 0
		if (_back_buffers[0] != nullptr)
		{
#if RESHADE_ADDON
			reshade::invoke_addon_event<reshade::addon_event::init_swapchain>(this, resize);
#endif

			reshade::init_effect_runtime(this);
		}
	}

	// Do not call 'present' event before 'init_swapchain' event
	if (_back_buffers[0] != nullptr)
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::present>(_parent_queue, this, nullptr, nullptr, 0, nullptr);
#endif

		reshade::present_effect_runtime(this);

		_parent_queue->flush_immediate_command_list();
	}

	lock.unlock();

	// Get original command list pointer from proxy object
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pOpenCommandList->QueryInterface(&command_list_proxy)))
		pOpenCommandList = command_list_proxy->_orig;

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}
