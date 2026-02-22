/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include "d3d9_device.hpp"
#include "d3d9on12_device.hpp"
#include "d3d12/d3d12_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"

Direct3DDevice9On12::Direct3DDevice9On12(Direct3DDevice9 *device_9, D3D12Device *device_12, IDirect3DDevice9On12 *original) :
	_orig(original),
	_parent_device_9(device_9),
	_parent_device_12(device_12)
{
	assert(_orig != nullptr && _parent_device_9 != nullptr && _parent_device_12 != nullptr);

	_parent_device_12->AddRef();
}
Direct3DDevice9On12::~Direct3DDevice9On12()
{
	_parent_device_12->Release();
}

bool Direct3DDevice9On12::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		riid == __uuidof(IDirect3DDevice9On12))
		return true;

	return false;
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9On12::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	// IUnknown is handled by Direct3DDevice9
	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	return _parent_device_9->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9On12::AddRef()
{
	return _parent_device_9->AddRef();
}
ULONG   STDMETHODCALLTYPE Direct3DDevice9On12::Release()
{
	return _parent_device_9->Release();
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9On12::GetD3D12Device(REFIID riid, void **ppvDevice)
{
	return _parent_device_12->QueryInterface(riid, ppvDevice);
}

HRESULT STDMETHODCALLTYPE Direct3DDevice9On12::UnwrapUnderlyingResource(IDirect3DResource9 *pResource, ID3D12CommandQueue *pCommandQueue, REFIID riid, void **ppvResource12)
{
	if (com_ptr<D3D12CommandQueue> command_queue_proxy;
		SUCCEEDED(pCommandQueue->QueryInterface(&command_queue_proxy)))
		pCommandQueue = command_queue_proxy->_orig;

	return _orig->UnwrapUnderlyingResource(pResource, pCommandQueue, riid, ppvResource12);
}
HRESULT STDMETHODCALLTYPE Direct3DDevice9On12::ReturnUnderlyingResource(IDirect3DResource9 *pResource, UINT NumSync, UINT64 *pSignalValues, ID3D12Fence **ppFences)
{
	return _orig->ReturnUnderlyingResource(pResource, NumSync, pSignalValues, ppFences);
}
