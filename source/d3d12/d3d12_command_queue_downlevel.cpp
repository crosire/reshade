/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#if RESHADE_D3D12ON7

#include "dll_log.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_queue_downlevel.hpp"
#include "runtime_d3d12.hpp"

D3D12CommandQueueDownlevel::D3D12CommandQueueDownlevel(D3D12CommandQueue *queue, ID3D12CommandQueueDownlevel *original) :
	_orig(original),
	_device(queue->_device) {
	assert(_orig != nullptr);
	_runtime = std::make_unique<reshade::d3d12::runtime_d3d12>(_device->_orig, queue->_orig, nullptr);
	_runtime->_buffer_detection = &_device->_buffer_detection;
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

	_runtime->on_reset();

	// Release any live references to depth buffers etc.
	_device->_buffer_detection.reset(true);

	const ULONG ref_orig = _orig->Release();
	if (ref_orig != 0)
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

	// Reinitialize runtime when the source texture changes
	if (pSourceTex2D != _last_source_tex)
	{
		_last_source_tex = pSourceTex2D;

		_runtime->on_reset();

		DXGI_SWAP_CHAIN_DESC swap_desc = {};
		swap_desc.BufferCount = 1; // There is only one fake back buffer texture
		const D3D12_RESOURCE_DESC source_desc = pSourceTex2D->GetDesc();
		swap_desc.BufferDesc.Width = static_cast<UINT>(source_desc.Width);
		swap_desc.BufferDesc.Height = source_desc.Height;
		swap_desc.BufferDesc.Format = source_desc.Format;
		swap_desc.OutputWindow = hWindow;

		if (!_runtime->on_init(swap_desc, pSourceTex2D))
			LOG(ERROR) << "Failed to initialize Direct3D 12 runtime environment on runtime " << _runtime.get() << '.';
	}

	_runtime->on_present();

	// Clear current frame stats
	_device->_buffer_detection.reset(false);

	// Get original command list pointer from proxy object
	if (com_ptr<D3D12GraphicsCommandList> command_list_proxy;
		SUCCEEDED(pOpenCommandList->QueryInterface(&command_list_proxy)))
		pOpenCommandList = command_list_proxy->_orig;

	return _orig->Present(pOpenCommandList, pSourceTex2D, hWindow, Flags);
}

#endif
