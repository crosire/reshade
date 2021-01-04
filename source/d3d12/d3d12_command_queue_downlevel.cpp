/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"
#include "runtime_d3d12.hpp"

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	_orig(original),
	_queue(queue),
	_runtime(new reshade::d3d12::runtime_d3d12(queue->_device->_orig, queue->_orig, nullptr, &queue->_device->_state))
{
	assert(_orig != nullptr && _queue != nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (riid == __uuidof(this) ||
		riid == __uuidof(IUnknown) ||
		riid == __uuidof(ID3D12CommandQueueDownlevel))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::AddRef()
{
	_orig->AddRef();
	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Release()
{
	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	delete _runtime;

	// Only release internal reference after the runtime has been destroyed, so any references it held are cleaned up at this point
	const ULONG ref_orig = _orig->Release();
	if (ref_orig > 1) // Verify internal reference count against one instead of zero because parent queue still holds a reference
		LOG(WARN) << "Reference count for ID3D12CommandQueueDownlevel object " << this << " is inconsistent.";

#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D12CommandQueueDownlevel object " << this << ".";
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
	assert(pSourceTex2D != nullptr);
	_runtime->on_present(pSourceTex2D, hWindow);

	// Clear current frame stats
	_queue->_device->_state.reset(false);

	// Get original command list pointer from proxy object
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pOpenCommandList->QueryInterface(&command_list_proxy)))
		pOpenCommandList = command_list_proxy->_orig;

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}
