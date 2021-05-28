/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	runtime_impl(queue->_device, queue, nullptr),
	_orig(original),
	_parent_queue(queue)
{
	assert(_orig != nullptr && _parent_queue != nullptr);
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
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::present>(_parent_queue, this);
#endif

	assert(pSourceTex2D != nullptr);
	runtime_impl::on_present(pSourceTex2D, hWindow);

	_parent_queue->flush_immediate_command_list();

	// Get original command list pointer from proxy object
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pOpenCommandList->QueryInterface(&command_list_proxy)))
		pOpenCommandList = command_list_proxy->_orig;

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}
