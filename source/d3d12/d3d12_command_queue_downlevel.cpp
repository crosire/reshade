/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_D3D12ON7

#include "log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"
#include <assert.h>

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	_orig(original),
	_device(queue->_device) {
	assert(original != nullptr);
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
	++_ref;

	return _orig->AddRef();
}
ULONG   STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Release()
{
	if (--_ref == 0)
	{
		_runtime->on_reset();
		_device->clear_drawcall_stats(true); // Release any live references to depth buffers etc.
		_device->_runtimes.erase(std::remove(_device->_runtimes.begin(), _device->_runtimes.end(), _runtime), _device->_runtimes.end());

		_runtime.reset();
	}

	// Decrease internal reference count and verify it against our own count
	const ULONG ref = _orig->Release();
	if (ref != 0 && _ref != 0)
		return ref;
	else if (ref != 0)
		LOG(WARN) << "Reference count for ID3D12CommandQueueDownlevel object " << this << " is inconsistent: " << ref << ", but expected 0.";

	assert(_ref <= 0);
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroyed ID3D12CommandQueueDownlevel object " << this << ".";
#endif
	delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueueDownlevel::Present(ID3D12GraphicsCommandList *pOpenCommandList, ID3D12Resource *pSourceTex2D, HWND hWindow, D3D12_DOWNLEVEL_PRESENT_FLAGS Flags)
{
	// Create runtime on first call to present (since we need to know which window it presents to)
	if (_runtime == nullptr)
	{
		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferCount = 1; // There is only one fake back buffer texture
		desc.BufferDesc.Format = pSourceTex2D->GetDesc().Format;
		desc.OutputWindow = hWindow;

		com_ptr<ID3D12CommandQueue> queue;
		_orig->QueryInterface(&queue);

		const auto runtime = std::make_shared<reshade::d3d12::runtime_d3d12>(_device->_orig, queue.get(), nullptr);
		if (!runtime->on_init(desc, pSourceTex2D))
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << runtime.get() << '.';

		_device->_runtimes.push_back(runtime);

		_runtime = std::move(runtime);
	}

	_runtime->on_present(_device->_draw_call_tracker);

	_device->clear_drawcall_stats();

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}

#endif
